/* IpcThread.c : 쓰레드 간 메시지 송/수신
 *
 * 전역 링버퍼를 두고 뮤텍스로 상호배제, 세마포어 2개로 빈 슬롯/찬 슬롯 개수를 센다.
 * 뮤텍스만 쓰면 "빌 때까지 대기" 가 바쁜 대기가 되므로 세마포어를 같이 쓴다.
 * CRITICAL_SECTION = pthread_mutex_t, CreateSemaphore = sem_init 에 해당.
 */
#include <winsock2.h>
#include <windows.h>
#include <process.h>
#include <string.h>

#include "IpcCore.h"

/* 송신/수신 쓰레드가 함께 쓰는 전역 자원 */
static st_IpcThreadMsg      g_staRing[IPC_THREAD_RING_SLOTS];
static IPC_INT32            g_iRingHead = 0;
static IPC_INT32            g_iRingTail = 0;
static IPC_INT32            g_iRingCount = 0;

static CRITICAL_SECTION     g_stRingLock;
static HANDLE               g_hSemEmpty = NULL;
static HANDLE               g_hSemFull  = NULL;
static LONG                 g_lQueueReady = 0;

static HANDLE               g_hTxThread   = NULL;
static HANDLE               g_hRxThread   = NULL;
static HANDLE               g_hStopEvent  = NULL;
static volatile LONG        g_lDemoRunning = 0;

#define IPC_THREAD_WAIT_SLICE_MS    100

IPC_INT32 __cdecl f_IpcThreadQueueInit(IPC_VOID)
{
    if (InterlockedCompareExchange(&g_lQueueReady, 1, 0) != 0)
    {
        return IPC_PASS;
    }

    InitializeCriticalSection(&g_stRingLock);

    g_hSemEmpty = CreateSemaphoreW(NULL, (LONG)IPC_THREAD_RING_SLOTS, (LONG)IPC_THREAD_RING_SLOTS, NULL);
    g_hSemFull  = CreateSemaphoreW(NULL, 0, (LONG)IPC_THREAD_RING_SLOTS, NULL);

    if ((g_hSemEmpty == NULL) || (g_hSemFull == NULL))
    {
        f_IpcLog(enum_IpcLogCh_Thread, "[TH] semaphore create failed (%lu)", GetLastError());

        if (g_hSemEmpty != NULL) { (void)CloseHandle(g_hSemEmpty); g_hSemEmpty = NULL; }
        if (g_hSemFull  != NULL) { (void)CloseHandle(g_hSemFull);  g_hSemFull  = NULL; }

        DeleteCriticalSection(&g_stRingLock);
        InterlockedExchange(&g_lQueueReady, 0);
        return IPC_FAIL;
    }

    (void)memset(g_staRing, 0, sizeof(g_staRing));
    g_iRingHead  = 0;
    g_iRingTail  = 0;
    g_iRingCount = 0;

    return IPC_PASS;
}

IPC_VOID __cdecl f_IpcThreadQueueDeinit(IPC_VOID)
{
    if (InterlockedCompareExchange(&g_lQueueReady, 0, 1) != 1)
    {
        return;
    }

    if (g_hSemEmpty != NULL) { (void)CloseHandle(g_hSemEmpty); g_hSemEmpty = NULL; }
    if (g_hSemFull  != NULL) { (void)CloseHandle(g_hSemFull);  g_hSemFull  = NULL; }

    DeleteCriticalSection(&g_stRingLock);
}

IPC_INT32 __cdecl f_IpcThreadQueueSend(const st_IpcThreadMsg *stpMsg, IPC_UINT32 uiTimeOut_ms)
{
    if ((stpMsg == NULL) || (InterlockedCompareExchange(&g_lQueueReady, 1, 1) != 1))
    {
        return IPC_FAIL;
    }

    /* 빈 슬롯 대기 -> 큐가 가득 차면 여기서 블록된다 */
    if (WaitForSingleObject(g_hSemEmpty, (DWORD)uiTimeOut_ms) != WAIT_OBJECT_0)
    {
        return IPC_FAIL;
    }

    EnterCriticalSection(&g_stRingLock);
    g_staRing[g_iRingTail] = *stpMsg;
    g_iRingTail = (g_iRingTail + 1) % IPC_THREAD_RING_SLOTS;
    g_iRingCount++;
    LeaveCriticalSection(&g_stRingLock);

    (void)ReleaseSemaphore(g_hSemFull, 1, NULL);

    return IPC_PASS;
}

IPC_INT32 __cdecl f_IpcThreadQueueRecv(st_IpcThreadMsg *stpMsg, IPC_UINT32 uiTimeOut_ms)
{
    if ((stpMsg == NULL) || (InterlockedCompareExchange(&g_lQueueReady, 1, 1) != 1))
    {
        return IPC_FAIL;
    }

    /* 찬 슬롯 대기 -> 큐가 비면 여기서 블록된다 */
    if (WaitForSingleObject(g_hSemFull, (DWORD)uiTimeOut_ms) != WAIT_OBJECT_0)
    {
        return IPC_FAIL;
    }

    EnterCriticalSection(&g_stRingLock);
    *stpMsg = g_staRing[g_iRingHead];
    g_iRingHead = (g_iRingHead + 1) % IPC_THREAD_RING_SLOTS;
    g_iRingCount--;
    LeaveCriticalSection(&g_stRingLock);

    (void)ReleaseSemaphore(g_hSemEmpty, 1, NULL);

    return IPC_PASS;
}

IPC_INT32 __cdecl f_IpcThreadQueueGetCount(IPC_VOID)
{
    IPC_INT32 iCount;

    if (InterlockedCompareExchange(&g_lQueueReady, 1, 1) != 1)
    {
        return 0;
    }

    EnterCriticalSection(&g_stRingLock);
    iCount = g_iRingCount;
    LeaveCriticalSection(&g_stRingLock);

    return iCount;
}

static IPC_INT32 s_IsStopRequested(IPC_UINT32 uiWait_ms)
{
    if (g_hStopEvent == NULL)
    {
        return 1;
    }

    return (WaitForSingleObject(g_hStopEvent, (DWORD)uiWait_ms) == WAIT_OBJECT_0) ? 1 : 0;
}

static unsigned __stdcall s_TxThreadProc(void *vpArg)
{
    st_IpcThreadMsg stMsg;
    IPC_INT32       iData;
    IPC_INT32       iSeq = 0;

    (void)vpArg;

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

        if (s_IsStopRequested(IPC_DEMO_INTERVAL_MS) != 0)
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

static unsigned __stdcall s_RxThreadProc(void *vpArg)
{
    st_IpcThreadMsg stMsg;
    IPC_INT32       iRecvCount = 0;

    (void)vpArg;

    while (s_IsStopRequested(0U) == 0)
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

IPC_INT32 __cdecl f_IpcThreadDemoStart(IPC_VOID)
{
    if (InterlockedCompareExchange(&g_lDemoRunning, 1, 0) != 0)
    {
        return IPC_FAIL;
    }

    if (f_IpcThreadQueueInit() != IPC_PASS)
    {
        InterlockedExchange(&g_lDemoRunning, 0);
        return IPC_FAIL;
    }

    g_hStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (g_hStopEvent == NULL)
    {
        InterlockedExchange(&g_lDemoRunning, 0);
        return IPC_FAIL;
    }

    g_hRxThread = (HANDLE)_beginthreadex(NULL, 0, s_RxThreadProc, NULL, 0, NULL);
    g_hTxThread = (HANDLE)_beginthreadex(NULL, 0, s_TxThreadProc, NULL, 0, NULL);

    if ((g_hRxThread == NULL) || (g_hTxThread == NULL))
    {
        f_IpcLog(enum_IpcLogCh_Thread, "[TH] thread create failed");
        (void)f_IpcThreadDemoStop();
        return IPC_FAIL;
    }

    f_IpcLog(enum_IpcLogCh_Thread, "[TH] start");

    return IPC_PASS;
}

IPC_INT32 __cdecl f_IpcThreadDemoStop(IPC_VOID)
{
    HANDLE haThread[2];
    DWORD  dwCount = 0U;

    if (g_hStopEvent != NULL)
    {
        (void)SetEvent(g_hStopEvent);
    }

    if (g_hTxThread != NULL) { haThread[dwCount++] = g_hTxThread; }
    if (g_hRxThread != NULL) { haThread[dwCount++] = g_hRxThread; }

    if (dwCount > 0U)
    {
        (void)WaitForMultipleObjects(dwCount, haThread, TRUE, 3000U);
    }

    if (g_hTxThread  != NULL) { (void)CloseHandle(g_hTxThread);  g_hTxThread  = NULL; }
    if (g_hRxThread  != NULL) { (void)CloseHandle(g_hRxThread);  g_hRxThread  = NULL; }
    if (g_hStopEvent != NULL) { (void)CloseHandle(g_hStopEvent); g_hStopEvent = NULL; }

    if (InterlockedCompareExchange(&g_lDemoRunning, 0, 1) == 1)
    {
        f_IpcLog(enum_IpcLogCh_Thread, "[TH] stop");
    }

    return IPC_PASS;
}

IPC_INT32 __cdecl f_IpcThreadDemoIsRunning(IPC_VOID)
{
    return (IPC_INT32)InterlockedCompareExchange(&g_lDemoRunning, 0, 0);
}
