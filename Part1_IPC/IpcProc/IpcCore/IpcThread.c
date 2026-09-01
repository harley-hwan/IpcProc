// @file	IpcThread.c
// @brief	쓰레드 간 메시지 송/수신.
//			전역 링버퍼 + 뮤텍스, 빈/찬 슬롯 개수는 세마포어 2개로 셈.
//			뮤텍스만 쓰면 빌 때까지 기다리는 게 바쁜 대기가 돼서 세마포어를 같이 씀.
//			CRITICAL_SECTION = pthread_mutex_t, CreateSemaphore = sem_init 이라고 보면 됨.
// @author	hwan
// @date	2026.08.19.
//
#include <winsock2.h>
#include <windows.h>
#include <process.h>
#include <string.h>
#include "IpcCore.h"

// 송신/수신 쓰레드가 같이 쓰는 것들
static ST_IpcThreadMsg      s_staRing[IPC_THREAD_RING_SLOTS];
static INT32                s_iRingHead  = 0;
static INT32                s_iRingTail  = 0;
static INT32                s_nRingCount = 0;
static CRITICAL_SECTION     s_stRingLock;
static HANDLE               s_hSemEmpty   = NULL;
static HANDLE               s_hSemFull    = NULL;
static LONG                 s_lQueueReady = 0;

static HANDLE               s_hTxThread    = NULL;
static HANDLE               s_hRxThread    = NULL;
static HANDLE               s_hStopEvent   = NULL;
static volatile LONG        s_lDemoRunning = 0;

#define IPC_THREAD_WAIT_SLICE_MS    100

// 큐 생성. 이미 만들어져 있으면 그냥 성공 처리
INT32 __cdecl f_IpcThreadQueueInit(VOID)
{
    if (InterlockedCompareExchange(&s_lQueueReady, 1, 0) != 0)
    {
        return IPC_PASS;
    }

    InitializeCriticalSection(&s_stRingLock);

    // 빈 슬롯 = 링버퍼 슬롯 수, 찬 슬롯 = 0 에서 시작
    s_hSemEmpty = CreateSemaphoreW(NULL, (LONG)IPC_THREAD_RING_SLOTS, (LONG)IPC_THREAD_RING_SLOTS, NULL);
    s_hSemFull  = CreateSemaphoreW(NULL, 0, (LONG)IPC_THREAD_RING_SLOTS, NULL);
    if ((s_hSemEmpty == NULL) || (s_hSemFull == NULL))
    {
        f_IpcLog(enum_IpcLogCh_Thread, "[TH] semaphore create failed (%lu)", GetLastError());
        if (s_hSemEmpty != NULL) { (VOID)CloseHandle(s_hSemEmpty); s_hSemEmpty = NULL; }
        if (s_hSemFull  != NULL) { (VOID)CloseHandle(s_hSemFull);  s_hSemFull  = NULL; }
        DeleteCriticalSection(&s_stRingLock);
        InterlockedExchange(&s_lQueueReady, 0);
        return IPC_FAIL;
    }

    (VOID)memset(s_staRing, 0, sizeof(s_staRing));
    s_iRingHead  = 0;
    s_iRingTail  = 0;
    s_nRingCount = 0;
    return IPC_PASS;
}

// 큐 해제
VOID __cdecl f_IpcThreadQueueDeinit(VOID)
{
    if (InterlockedCompareExchange(&s_lQueueReady, 0, 1) != 1)
    {
        return;
    }

    if (s_hSemEmpty != NULL) { (VOID)CloseHandle(s_hSemEmpty); s_hSemEmpty = NULL; }
    if (s_hSemFull  != NULL) { (VOID)CloseHandle(s_hSemFull);  s_hSemFull  = NULL; }
    DeleteCriticalSection(&s_stRingLock);
}

//
// @brief	큐에 한 건 보냄. 가득 차면 빈 자리 날 때까지 기다림
// @param	stpMsg			송신할 메시지
// @param	uiTimeOut_ms	빈 슬롯 대기 한도 (ms)
// @return	IPC_PASS / IPC_FAIL (타임아웃 포함)
//
INT32 __cdecl f_IpcThreadQueueSend(const ST_IpcThreadMsg *stpMsg, const UINT32 uiTimeOut_ms)
{
    if ((stpMsg == NULL) || (InterlockedCompareExchange(&s_lQueueReady, 1, 1) != 1))
    {
        return IPC_FAIL;
    }

    // 1. 빈 슬롯 대기   가득 차 있으면 여기서 막힘
    if (WaitForSingleObject(s_hSemEmpty, (DWORD)uiTimeOut_ms) != WAIT_OBJECT_0)
    {
        return IPC_FAIL;
    }
    
	// 2. 뮤택스로 링버퍼 상호배제.
    EnterCriticalSection(&s_stRingLock);
    s_staRing[s_iRingTail] = *stpMsg;
    s_iRingTail = (s_iRingTail + 1) % IPC_THREAD_RING_SLOTS;
    s_nRingCount++;
    LeaveCriticalSection(&s_stRingLock);

	// 3. 찬 슬롯 + 1 - 수신 쓰레드를 깨운다
    (VOID)ReleaseSemaphore(s_hSemFull, 1, NULL);
    return IPC_PASS;
}

//
// @brief	큐에서 한 건 꺼냄. 비어 있으면 들어올 때까지 기다림
// @param	stpMsg			수신 메시지를 담을 버퍼
// @param	uiTimeOut_ms	찬 슬롯 대기 한도 (ms)
// @return	IPC_PASS / IPC_FAIL (타임아웃 포함)
//
INT32 __cdecl f_IpcThreadQueueRecv(ST_IpcThreadMsg *stpMsg, const UINT32 uiTimeOut_ms)
{
    if ((stpMsg == NULL) || (InterlockedCompareExchange(&s_lQueueReady, 1, 1) != 1))
    {
        return IPC_FAIL;
    }

    // 1. 찬 슬롯 대기 - 비어 있으면 여기서 막힘
    if (WaitForSingleObject(s_hSemFull, (DWORD)uiTimeOut_ms) != WAIT_OBJECT_0)
    {
        return IPC_FAIL;
    }

    // 2. 같은 뮤택스로 링버퍼 상호배제
    EnterCriticalSection(&s_stRingLock);
    *stpMsg = s_staRing[s_iRingHead];
    s_iRingHead = (s_iRingHead + 1) % IPC_THREAD_RING_SLOTS;
    s_nRingCount--;
    LeaveCriticalSection(&s_stRingLock);

    // 3. 빈 슬롯 + 1 - 송신 쓰레드를 깨운다
    (VOID)ReleaseSemaphore(s_hSemEmpty, 1, NULL);
    return IPC_PASS;
}

INT32 __cdecl f_IpcThreadQueueGetCount(VOID)
{
    INT32 nCount;

    if (InterlockedCompareExchange(&s_lQueueReady, 1, 1) != 1)
    {
        return 0;
    }

    EnterCriticalSection(&s_stRingLock);
    nCount = s_nRingCount;
    LeaveCriticalSection(&s_stRingLock);
    return nCount;
}

static INT32 f_IsStopRequested(const UINT32 uiWait_ms)
{
    if (s_hStopEvent == NULL)
    {
        return 1;
    }
    return (WaitForSingleObject(s_hStopEvent, (DWORD)uiWait_ms) == WAIT_OBJECT_0) ? 1 : 0;
}

// 데모 송신 쓰레드. 1~100 을 0.1 초마다 보냄
static UINT32 __stdcall f_TxThreadProc(VOID *vpArg)
{
    ST_IpcThreadMsg st_Msg;
    INT32           iData;
    INT32           nSeq = 0;

    (VOID)vpArg;

    for (iData = IPC_DEMO_FIRST_VALUE; iData <= IPC_DEMO_LAST_VALUE; iData++)
    {
        nSeq++;
        st_Msg.nSeq  = nSeq;
        st_Msg.iData = iData;

        if (f_IpcThreadQueueSend(&st_Msg, 1000U) != IPC_PASS)
        {
            f_IpcLog(enum_IpcLogCh_Thread, "[TH] tx full, data=%d", iData);
        }
        else
        {
            f_IpcLog(enum_IpcLogCh_Thread, "[TH] tx %d", iData);
        }

        // 다음 건까지 IPC_DEMO_INTERVAL_MS 를 쉰다.
        // 이 간격이 수신 쪽이 한 건 꺼내는 시간보다 훨씬 길어서 큐가 매번 비워짐.
        // 그래서 보통은 한 줄씩 번갈아 나오지만, 로그는 두 쓰레드가 각자 찍으므로
        // 줄 순서까지 보장되지는 않음.
        if (f_IsStopRequested(IPC_DEMO_INTERVAL_MS) != 0)
        {
            break;
        }
    }

    if (iData > IPC_DEMO_LAST_VALUE)
    {
        f_IpcLog(enum_IpcLogCh_Thread, "[TH] tx done (%d)", nSeq);
    }
    return 0U;
}

// 데모 수신 쓰레드. 멈추라고 할 때까지 꺼내서 로그로 찍음
//  받은 nSeq 를 기대값과 대조해서 순서가 어긋났는지 같이 셈.
static UINT32 __stdcall f_RxThreadProc(VOID *vpArg)
{
    ST_IpcThreadMsg st_Msg;
    INT32           nRecvCount   = 0;
    INT32           nExpectSeq   = 1;
    INT32           nSeqErrCount = 0;

    (VOID)vpArg;

    while (f_IsStopRequested(0U) == 0)
    {
        if (f_IpcThreadQueueRecv(&st_Msg, IPC_THREAD_WAIT_SLICE_MS) == IPC_PASS)
        {
            nRecvCount++;
            f_IpcLog(enum_IpcLogCh_Thread, "[TH] rx %d", st_Msg.iData);
            if (st_Msg.nSeq != nExpectSeq)
            {
                nSeqErrCount++;
                f_IpcLog(enum_IpcLogCh_Thread, "[TH] rx seq mismatch (expect %d, got %d)",
                         nExpectSeq, st_Msg.nSeq);
            }
            nExpectSeq = st_Msg.nSeq + 1;
        }
    }

    // 멈추라는 신호가 와도 큐에 남아 있는 건 마저 꺼내고 끝낸다
    while (f_IpcThreadQueueRecv(&st_Msg, 0U) == IPC_PASS)
    {
        nRecvCount++;
        f_IpcLog(enum_IpcLogCh_Thread, "[TH] rx %d", st_Msg.iData);
        if (st_Msg.nSeq != nExpectSeq)
        {
            nSeqErrCount++;
            f_IpcLog(enum_IpcLogCh_Thread, "[TH] rx seq mismatch (expect %d, got %d)",
                     nExpectSeq, st_Msg.nSeq);
        }
        nExpectSeq = st_Msg.nSeq + 1;
    }

    f_IpcLog(enum_IpcLogCh_Thread, "[TH] rx done (%d, seq err %d)", nRecvCount, nSeqErrCount);
    return 0U;
}

//
// @brief	데모 시작. 큐 준비하고 송신/수신 쓰레드 한 쌍 띄움
// @return	IPC_PASS / IPC_FAIL (이미 동작 중 포함)
//
INT32 __cdecl f_IpcThreadDemoStart(VOID)
{
    if (InterlockedCompareExchange(&s_lDemoRunning, 1, 0) != 0)
    {
        return IPC_FAIL;
    }

    if (f_IpcThreadQueueInit() != IPC_PASS)
    {
        InterlockedExchange(&s_lDemoRunning, 0);
        return IPC_FAIL;
    }

    s_hStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (s_hStopEvent == NULL)
    {
        InterlockedExchange(&s_lDemoRunning, 0);
        return IPC_FAIL;
    }

    // 시작 로그를 먼저 찍고 쓰레드를 띄운다. 반대로 하면 tx 첫 줄이 start 보다 앞설 수 있다.
    f_IpcLog(enum_IpcLogCh_Thread, "[TH] start");

    s_hRxThread = (HANDLE)_beginthreadex(NULL, 0, f_RxThreadProc, NULL, 0, NULL);
    s_hTxThread = (HANDLE)_beginthreadex(NULL, 0, f_TxThreadProc, NULL, 0, NULL);
    if ((s_hRxThread == NULL) || (s_hTxThread == NULL))
    {
        f_IpcLog(enum_IpcLogCh_Thread, "[TH] thread create failed");
        (VOID)f_IpcThreadDemoStop();
        return IPC_FAIL;
    }

    return IPC_PASS;
}

// 데모 정지. 이벤트 올리고 두 쓰레드 끝날 때까지 기다림
INT32 __cdecl f_IpcThreadDemoStop(VOID)
{
    HANDLE haThread[2];
    DWORD  nCount = 0U;

    if (s_hStopEvent != NULL)
    {
        (VOID)SetEvent(s_hStopEvent);
    }

    if (s_hTxThread != NULL) { haThread[nCount++] = s_hTxThread; }
    if (s_hRxThread != NULL) { haThread[nCount++] = s_hRxThread; }
    if (nCount > 0U)
    {
        (VOID)WaitForMultipleObjects(nCount, haThread, TRUE, 3000U);
    }

    if (s_hTxThread  != NULL) { (VOID)CloseHandle(s_hTxThread);  s_hTxThread  = NULL; }
    if (s_hRxThread  != NULL) { (VOID)CloseHandle(s_hRxThread);  s_hRxThread  = NULL; }
    if (s_hStopEvent != NULL) { (VOID)CloseHandle(s_hStopEvent); s_hStopEvent = NULL; }

    if (InterlockedCompareExchange(&s_lDemoRunning, 0, 1) == 1)
    {
        f_IpcLog(enum_IpcLogCh_Thread, "[TH] stop");
    }
    return IPC_PASS;
}

INT32 __cdecl f_IpcThreadDemoIsRunning(VOID)
{
    return (INT32)InterlockedCompareExchange(&s_lDemoRunning, 0, 0);
}
