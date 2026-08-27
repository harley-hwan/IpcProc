//
// @file	IpcThread.c
// @brief	쓰레드 간 메시지 송/수신.
//			전역 링버퍼를 두고 뮤텍스로 상호배제, 세마포어 2개로 빈 슬롯/찬 슬롯 개수를 센다.
//			뮤텍스만 쓰면 "빌 때까지 대기" 가 바쁜 대기가 되므로 세마포어를 같이 쓴다.
//			CRITICAL_SECTION = pthread_mutex_t, CreateSemaphore = sem_init 에 해당.
// @author	hwan
// @date	2026.08.19.
//
#include <winsock2.h>
#include <windows.h>
#include <process.h>
#include <string.h>

#include "IpcCore.h"

// 송신/수신 쓰레드가 함께 쓰는 전역 자원
static st_IpcThreadMsg      s_staRing[IPC_THREAD_RING_SLOTS];
static INT32            s_iRingHead = 0;
static INT32            s_iRingTail = 0;
static INT32            s_iRingCount = 0;

static CRITICAL_SECTION     s_stRingLock;
static HANDLE               s_hSemEmpty = NULL;
static HANDLE               s_hSemFull  = NULL;
static LONG                 s_lQueueReady = 0;

static HANDLE               s_hTxThread   = NULL;
static HANDLE               s_hRxThread   = NULL;
static HANDLE               s_hStopEvent  = NULL;
static volatile LONG        s_lDemoRunning = 0;

#define IPC_THREAD_WAIT_SLICE_MS    100

//
// @brief	쓰레드 간 큐 생성. 링버퍼/뮤텍스/세마포어를 준비한다 (이미 있으면 그대로 성공).
// @return	IPC_PASS / IPC_FAIL
// @author	hwan
//
INT32 __cdecl f_IpcThreadQueueInit(VOID)
{
    if (InterlockedCompareExchange(&s_lQueueReady, 1, 0) != 0)
    {
        return IPC_PASS;
    }

    InitializeCriticalSection(&s_stRingLock);

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
    s_iRingCount = 0;

    return IPC_PASS;
}

//
// @brief	쓰레드 간 큐 해제. 뮤텍스와 세마포어를 반납한다
// @param	사용 안함
// @return	없음
// @author	hwan
//
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
// @brief	큐에 메시지 한 건 송신. 큐가 가득 차면 빈 슬롯이 날 때까지 대기한다.
// @param	stpMsg			송신할 메시지
// @param	uiTimeOut_ms	빈 슬롯 대기 한도 (ms)
// @return	IPC_PASS / IPC_FAIL(타임아웃 포함)
// @author	hwan
//
INT32 __cdecl f_IpcThreadQueueSend(const st_IpcThreadMsg *stpMsg, UINT32 uiTimeOut_ms)
{
    if ((stpMsg == NULL) || (InterlockedCompareExchange(&s_lQueueReady, 1, 1) != 1))
    {
        return IPC_FAIL;
    }

    // 빈 슬롯 대기 -> 큐가 가득 차면 여기서 블록된다
    if (WaitForSingleObject(s_hSemEmpty, (DWORD)uiTimeOut_ms) != WAIT_OBJECT_0)
    {
        return IPC_FAIL;
    }

    EnterCriticalSection(&s_stRingLock);
    s_staRing[s_iRingTail] = *stpMsg;
    s_iRingTail = (s_iRingTail + 1) % IPC_THREAD_RING_SLOTS;
    s_iRingCount++;
    LeaveCriticalSection(&s_stRingLock);

    (VOID)ReleaseSemaphore(s_hSemFull, 1, NULL);

    return IPC_PASS;
}

//
// @brief	큐에서 메시지 한 건 수신. 큐가 비면 찬 슬롯이 생길 때까지 대기한다.
// @param	stpMsg			수신 메시지를 담을 버퍼
// @param	uiTimeOut_ms	찬 슬롯 대기 한도 (ms)
// @return	IPC_PASS / IPC_FAIL(타임아웃 포함)
// @author	hwan
//
INT32 __cdecl f_IpcThreadQueueRecv(st_IpcThreadMsg *stpMsg, UINT32 uiTimeOut_ms)
{
    if ((stpMsg == NULL) || (InterlockedCompareExchange(&s_lQueueReady, 1, 1) != 1))
    {
        return IPC_FAIL;
    }

    // 찬 슬롯 대기 -> 큐가 비면 여기서 블록된다
    if (WaitForSingleObject(s_hSemFull, (DWORD)uiTimeOut_ms) != WAIT_OBJECT_0)
    {
        return IPC_FAIL;
    }

    EnterCriticalSection(&s_stRingLock);
    *stpMsg = s_staRing[s_iRingHead];
    s_iRingHead = (s_iRingHead + 1) % IPC_THREAD_RING_SLOTS;
    s_iRingCount--;
    LeaveCriticalSection(&s_stRingLock);

    (VOID)ReleaseSemaphore(s_hSemEmpty, 1, NULL);

    return IPC_PASS;
}

INT32 __cdecl f_IpcThreadQueueGetCount(VOID)
{
    INT32 iCount;

    if (InterlockedCompareExchange(&s_lQueueReady, 1, 1) != 1)
    {
        return 0;
    }

    EnterCriticalSection(&s_stRingLock);
    iCount = s_iRingCount;
    LeaveCriticalSection(&s_stRingLock);

    return iCount;
}

static INT32 f_IsStopRequested(UINT32 uiWait_ms)
{
    if (s_hStopEvent == NULL)
    {
        return 1;
    }

    return (WaitForSingleObject(s_hStopEvent, (DWORD)uiWait_ms) == WAIT_OBJECT_0) ? 1 : 0;
}

//
// @brief	데모 송신 쓰레드. 1 부터 100 까지 0.1 초 간격으로 큐에 송신한다.
// @param	vpArg	사용 안함
// @return	0 고정
// @author	hwan
//
static UINT32 __stdcall f_TxThreadProc(VOID *vpArg)
{
    st_IpcThreadMsg stMsg;
    INT32       iData;
    INT32       iSeq = 0;

    (VOID)vpArg;

    for (iData = IPC_DEMO_FIRST_VALUE; iData <= IPC_DEMO_LAST_VALUE; iData++)
    {
        iSeq++;

        stMsg.iSeq  = iSeq;
        stMsg.iData = iData;

        if (f_IpcThreadQueueSend(&stMsg, 1000U) != IPC_PASS)
        {
            f_IpcLog(enum_IpcLogCh_Thread, "[TH] tx full, data=%d", iData);
        }
        else
        {
            f_IpcLog(enum_IpcLogCh_Thread, "[TH] tx %d", iData);
        }

        if (f_IsStopRequested(IPC_DEMO_INTERVAL_MS) != 0)
        {
            break;
        }
    }

    if (iData > IPC_DEMO_LAST_VALUE)
    {
        f_IpcLog(enum_IpcLogCh_Thread, "[TH] tx done (%d)", iSeq);
    }

    return 0U;
}

//
// @brief	데모 수신 쓰레드. 정지 요청까지 큐에서 메시지를 꺼내 로그로 출력한다.
// @param	vpArg	사용 안함
// @return	0 고정
// @author	hwan
//
static UINT32 __stdcall f_RxThreadProc(VOID *vpArg)
{
    st_IpcThreadMsg stMsg;
    INT32       iRecvCount = 0;

    (VOID)vpArg;

    while (f_IsStopRequested(0U) == 0)
    {
        if (f_IpcThreadQueueRecv(&stMsg, IPC_THREAD_WAIT_SLICE_MS) == IPC_PASS)
        {
            iRecvCount++;
            f_IpcLog(enum_IpcLogCh_Thread, "[TH] rx %d", stMsg.iData);
        }
    }

    f_IpcLog(enum_IpcLogCh_Thread, "[TH] rx done (%d)", iRecvCount);

    return 0U;
}

//
// @brief	쓰레드 데모 시작. 큐를 준비하고 송신/수신 쓰레드 한 쌍을 기동한다.
// @return	IPC_PASS / IPC_FAIL(이미 동작 중 포함)
// @author	hwan
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

    s_hRxThread = (HANDLE)_beginthreadex(NULL, 0, f_RxThreadProc, NULL, 0, NULL);
    s_hTxThread = (HANDLE)_beginthreadex(NULL, 0, f_TxThreadProc, NULL, 0, NULL);

    if ((s_hRxThread == NULL) || (s_hTxThread == NULL))
    {
        f_IpcLog(enum_IpcLogCh_Thread, "[TH] thread create failed");
        (VOID)f_IpcThreadDemoStop();
        return IPC_FAIL;
    }

    f_IpcLog(enum_IpcLogCh_Thread, "[TH] start");

    return IPC_PASS;
}

//
// @brief	쓰레드 데모 정지. 정지 이벤트를 올리고 두 쓰레드의 종료를 기다린다.
// @return	IPC_PASS 고정
// @author	hwan
//
INT32 __cdecl f_IpcThreadDemoStop(VOID)
{
    HANDLE haThread[2];
    DWORD  dwCount = 0U;

    if (s_hStopEvent != NULL)
    {
        (VOID)SetEvent(s_hStopEvent);
    }

    if (s_hTxThread != NULL) { haThread[dwCount++] = s_hTxThread; }
    if (s_hRxThread != NULL) { haThread[dwCount++] = s_hRxThread; }

    if (dwCount > 0U)
    {
        (VOID)WaitForMultipleObjects(dwCount, haThread, TRUE, 3000U);
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
