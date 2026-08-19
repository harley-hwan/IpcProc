/* IpcMsgQ.c : 프로세스 간 메시지 송/수신 (메시지 큐)
 *
 * 커널 오브젝트 이름
 *   Local\\IpcProc.<이름>.map     공유메모리(링버퍼)
 *   Local\\IpcProc.<이름>.mtx     뮤텍스
 *   Local\\IpcProc.<이름>.sem.e   빈 슬롯 세마포어
 *   Local\\IpcProc.<이름>.sem.f   찬 슬롯 세마포어
 */
#include <winsock2.h>
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <string.h>

#include "IpcCore.h"

#define IPC_MSGQ_MAGIC          0x51435049U         /* 'IPCQ' */
#define IPC_MSGQ_OBJ_PREFIX     L"Local\\IpcProc."
#define IPC_MSGQ_WAIT_SLICE_MS  200

/* 공유메모리에 올라가는 링버퍼 */
#pragma pack(push, 1)
typedef struct st_IpcMsgQRing
{
    IPC_UINT32                  uiMagic;
    IPC_INT32                   iCapacity;
    IPC_INT32                   iPayloadMax;
    IPC_INT32                   iHead;                          /* 소비 인덱스 */
    IPC_INT32                   iTail;                          /* 생산 인덱스 */
    IPC_INT32                   iCount;                         /* 적재 건수   */
    st_IpcMsg                   staSlot[IPC_MSGQ_CAPACITY];
} st_IpcMsgQRing;
#pragma pack(pop)

static st_IpcMsgQ           g_stDemoQueue;
static HANDLE               g_hDemoThread  = NULL;
static HANDLE               g_hDemoStop    = NULL;
static volatile LONG        g_lDemoRunning = 0;
static IPC_INT32            g_iDemoIsSender = 0;

static IPC_VOID s_BuildObjName(wchar_t *wcpOut, size_t szOutCch, const char *cpName, const wchar_t *wcpSuffix)
{
    wchar_t wcaName[IPC_MSGQ_NAME_MAX];

    (void)memset(wcaName, 0, sizeof(wcaName));

    if (MultiByteToWideChar(CP_UTF8, 0, cpName, -1, wcaName, (int)(sizeof(wcaName) / sizeof(wcaName[0])) - 1) == 0)
    {
        (void)wcscpy_s(wcaName, sizeof(wcaName) / sizeof(wcaName[0]), L"Default");
    }

    /* %ls 를 쓴다 : MSVC 는 %s 도 와이드로 보지만 %ls 가 표준이고 이식성이 있다 */
    (void)_snwprintf_s(wcpOut, szOutCch, _TRUNCATE, L"%ls%ls%ls", IPC_MSGQ_OBJ_PREFIX, wcaName, wcpSuffix);
}

static IPC_VOID s_CloseHandleSafe(IPC_VOID **vppHandle)
{
    if ((vppHandle != NULL) && (*vppHandle != NULL))
    {
        (void)CloseHandle((HANDLE)(*vppHandle));
        *vppHandle = NULL;
    }
}

/* 링버퍼 헤더는 뮤텍스 안에서 처음 한 번만 초기화한다 */
static IPC_INT32 s_PrepareRing(st_IpcMsgQ *stpQ)
{
    st_IpcMsgQRing *stpRing = (st_IpcMsgQRing *)stpQ->vpRing;
    DWORD           dwWait;
    IPC_INT32       iResult = IPC_PASS;

    dwWait = WaitForSingleObject((HANDLE)stpQ->vpMutex, 5000U);
    if ((dwWait != WAIT_OBJECT_0) && (dwWait != WAIT_ABANDONED))
    {
        return IPC_FAIL;
    }

    if (stpRing->uiMagic != IPC_MSGQ_MAGIC)
    {
        /* 파일 매핑은 0 으로 채워져 있어 head/tail/count 는 이미 0 */
        stpRing->uiMagic     = IPC_MSGQ_MAGIC;
        stpRing->iCapacity   = IPC_MSGQ_CAPACITY;
        stpRing->iPayloadMax = IPC_MSGQ_PAYLOAD_MAX;
        stpRing->iHead       = 0;
        stpRing->iTail       = 0;
        stpRing->iCount      = 0;
    }
    else if ((stpRing->iCapacity != IPC_MSGQ_CAPACITY) || (stpRing->iPayloadMax != IPC_MSGQ_PAYLOAD_MAX))
    {
        f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] queue layout mismatch");
        iResult = IPC_FAIL;
    }

    (void)ReleaseMutex((HANDLE)stpQ->vpMutex);

    return iResult;
}

static st_IpcMsgQ s_MsgQOpenInternal(const char *cpName, IPC_INT32 iCreate)
{
    st_IpcMsgQ  stQ;
    wchar_t     wcaObj[256];
    HANDLE      hMap;
    BOOL        bAlreadyExist = FALSE;

    (void)memset(&stQ, 0, sizeof(stQ));
    stQ.iStatus = enum_IpcMsgQ_Status_Error;

    if ((cpName == NULL) || (cpName[0] == '\0'))
    {
        cpName = IPC_MSGQ_DEFAULT_NAME;
    }
    (void)strncpy_s(stQ.caName, sizeof(stQ.caName), cpName, _TRUNCATE);

    /* 공유메모리 */
    s_BuildObjName(wcaObj, sizeof(wcaObj) / sizeof(wcaObj[0]), cpName, L".map");

    if (iCreate != 0)
    {
        hMap = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                  0, (DWORD)sizeof(st_IpcMsgQRing), wcaObj);
        bAlreadyExist = (GetLastError() == ERROR_ALREADY_EXISTS) ? TRUE : FALSE;
    }
    else
    {
        hMap = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, wcaObj);
        bAlreadyExist = TRUE;
    }

    if (hMap == NULL)
    {
        f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] shared memory failed (%lu)", GetLastError());
        return stQ;
    }
    stQ.vpMapHandle = (IPC_VOID *)hMap;
    stQ.iIsCreator  = ((iCreate != 0) && (bAlreadyExist == FALSE)) ? 1 : 0;

    stQ.vpRing = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(st_IpcMsgQRing));
    if (stQ.vpRing == NULL)
    {
        f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] map failed (%lu)", GetLastError());
        (void)f_IpcMsgQClose(&stQ);
        return stQ;
    }

    /* 뮤텍스 */
    s_BuildObjName(wcaObj, sizeof(wcaObj) / sizeof(wcaObj[0]), cpName, L".mtx");
    stQ.vpMutex = (IPC_VOID *)CreateMutexW(NULL, FALSE, wcaObj);

    /* 세마포어 2개. 초기 카운트는 최초 생성 시에만 반영된다 */
    s_BuildObjName(wcaObj, sizeof(wcaObj) / sizeof(wcaObj[0]), cpName, L".sem.e");
    stQ.vpSemEmpty = (IPC_VOID *)CreateSemaphoreW(NULL, (LONG)IPC_MSGQ_CAPACITY, (LONG)IPC_MSGQ_CAPACITY, wcaObj);

    s_BuildObjName(wcaObj, sizeof(wcaObj) / sizeof(wcaObj[0]), cpName, L".sem.f");
    stQ.vpSemFull = (IPC_VOID *)CreateSemaphoreW(NULL, 0, (LONG)IPC_MSGQ_CAPACITY, wcaObj);

    if ((stQ.vpMutex == NULL) || (stQ.vpSemEmpty == NULL) || (stQ.vpSemFull == NULL))
    {
        f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] sync object failed (%lu)", GetLastError());
        (void)f_IpcMsgQClose(&stQ);
        return stQ;
    }

    if (s_PrepareRing(&stQ) != IPC_PASS)
    {
        (void)f_IpcMsgQClose(&stQ);
        return stQ;
    }

    stQ.iStatus = enum_IpcMsgQ_Status_Opened;

    f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] queue '%s' open", stQ.caName);

    return stQ;
}

st_IpcMsgQ __cdecl f_IpcMsgQCreate(const char *cpName)
{
    return s_MsgQOpenInternal(cpName, 1);
}

st_IpcMsgQ __cdecl f_IpcMsgQOpen(const char *cpName)
{
    return s_MsgQOpenInternal(cpName, 0);
}

IPC_INT32 __cdecl f_IpcMsgQSend(st_IpcMsgQ *stpQ, const st_IpcMsg *stpMsg, IPC_UINT32 uiTimeOut_ms)
{
    st_IpcMsgQRing *stpRing;
    DWORD           dwWait;

    if ((stpQ == NULL) || (stpMsg == NULL) || (stpQ->iStatus != enum_IpcMsgQ_Status_Opened))
    {
        return IPC_FAIL;
    }
    if ((stpMsg->iDataSize < 0) || (stpMsg->iDataSize > IPC_MSGQ_PAYLOAD_MAX))
    {
        return IPC_FAIL;
    }

    stpRing = (st_IpcMsgQRing *)stpQ->vpRing;

    /* 빈 슬롯 대기 */
    if (WaitForSingleObject((HANDLE)stpQ->vpSemEmpty, (DWORD)uiTimeOut_ms) != WAIT_OBJECT_0)
    {
        return IPC_FAIL;
    }

    /* 프로세스 경계를 넘는 상호배제 */
    dwWait = WaitForSingleObject((HANDLE)stpQ->vpMutex, 5000U);
    if ((dwWait != WAIT_OBJECT_0) && (dwWait != WAIT_ABANDONED))
    {
        (void)ReleaseSemaphore((HANDLE)stpQ->vpSemEmpty, 1, NULL);
        return IPC_FAIL;
    }

    stpRing->staSlot[stpRing->iTail] = *stpMsg;
    stpRing->iTail = (stpRing->iTail + 1) % stpRing->iCapacity;
    stpRing->iCount++;

    (void)ReleaseMutex((HANDLE)stpQ->vpMutex);


    (void)ReleaseSemaphore((HANDLE)stpQ->vpSemFull, 1, NULL);

    return IPC_PASS;
}

IPC_INT32 __cdecl f_IpcMsgQRecv(st_IpcMsgQ *stpQ, st_IpcMsg *stpMsg, IPC_UINT32 uiTimeOut_ms)
{
    st_IpcMsgQRing *stpRing;
    DWORD           dwWait;

    if ((stpQ == NULL) || (stpMsg == NULL) || (stpQ->iStatus != enum_IpcMsgQ_Status_Opened))
    {
        return IPC_FAIL;
    }

    stpRing = (st_IpcMsgQRing *)stpQ->vpRing;

    /* 찬 슬롯 대기 */
    if (WaitForSingleObject((HANDLE)stpQ->vpSemFull, (DWORD)uiTimeOut_ms) != WAIT_OBJECT_0)
    {
        return IPC_FAIL;
    }


    dwWait = WaitForSingleObject((HANDLE)stpQ->vpMutex, 5000U);
    if ((dwWait != WAIT_OBJECT_0) && (dwWait != WAIT_ABANDONED))
    {
        (void)ReleaseSemaphore((HANDLE)stpQ->vpSemFull, 1, NULL);
        return IPC_FAIL;
    }

    *stpMsg = stpRing->staSlot[stpRing->iHead];
    stpRing->iHead = (stpRing->iHead + 1) % stpRing->iCapacity;
    stpRing->iCount--;

    (void)ReleaseMutex((HANDLE)stpQ->vpMutex);


    (void)ReleaseSemaphore((HANDLE)stpQ->vpSemEmpty, 1, NULL);

    return IPC_PASS;
}

IPC_INT32 __cdecl f_IpcMsgQGetCount(const st_IpcMsgQ *stpQ)
{
    const st_IpcMsgQRing *stpRing;

    if ((stpQ == NULL) || (stpQ->iStatus != enum_IpcMsgQ_Status_Opened))
    {
        return 0;
    }

    stpRing = (const st_IpcMsgQRing *)stpQ->vpRing;

    return stpRing->iCount;
}

IPC_INT32 __cdecl f_IpcMsgQClose(st_IpcMsgQ *stpQ)
{
    if (stpQ == NULL)
    {
        return IPC_FAIL;
    }

    if (stpQ->vpRing != NULL)
    {
        (void)UnmapViewOfFile(stpQ->vpRing);
        stpQ->vpRing = NULL;
    }

    s_CloseHandleSafe(&stpQ->vpSemFull);
    s_CloseHandleSafe(&stpQ->vpSemEmpty);
    s_CloseHandleSafe(&stpQ->vpMutex);
    s_CloseHandleSafe(&stpQ->vpMapHandle);

    stpQ->iStatus = enum_IpcMsgQ_Status_NotOpened;

    return IPC_PASS;
}

static IPC_INT32 s_IsStopRequested(IPC_UINT32 uiWait_ms)
{
    if (g_hDemoStop == NULL)
    {
        return 1;
    }

    return (WaitForSingleObject(g_hDemoStop, (DWORD)uiWait_ms) == WAIT_OBJECT_0) ? 1 : 0;
}

static unsigned __stdcall s_SenderProc(void *vpArg)
{
    st_IpcMsg   stMsg;
    IPC_INT32   iData;
    IPC_INT32   iSeq = 0;

    (void)vpArg;

    for (iData = IPC_DEMO_FIRST_VALUE; iData <= IPC_DEMO_LAST_VALUE; iData++)
    {
        iSeq++;

        (void)memset(&stMsg, 0, sizeof(stMsg));
        stMsg.iMsgType  = 1;
        stMsg.iDataSize = (IPC_INT32)sizeof(iData);
        (void)memcpy(stMsg.ucaData, &iData, sizeof(iData));

        if (f_IpcMsgQSend(&g_stDemoQueue, &stMsg, 1000U) != IPC_PASS)
        {
            f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] tx full, data=%d", iData);
        }
        else
        {
            f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] tx %d", iData);
        }

        if (s_IsStopRequested(IPC_DEMO_INTERVAL_MS) != 0)
        {
            break;
        }
    }

    if (iData > IPC_DEMO_LAST_VALUE)
    {
        f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] tx done (%d)", iSeq);
    }

    return 0U;
}

static unsigned __stdcall s_ReceiverProc(void *vpArg)
{
    st_IpcMsg   stMsg;
    IPC_INT32   iData;
    IPC_INT32   iRecvCount = 0;

    (void)vpArg;

    while (s_IsStopRequested(0U) == 0)
    {
        if (f_IpcMsgQRecv(&g_stDemoQueue, &stMsg, IPC_MSGQ_WAIT_SLICE_MS) != IPC_PASS)
        {
            continue;
        }

        iRecvCount++;

        if (stMsg.iDataSize == (IPC_INT32)sizeof(iData))
        {
            (void)memcpy(&iData, stMsg.ucaData, sizeof(iData));
            f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] rx %d", iData);
        }
        else
        {
            f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] rx type=%d size=%d", stMsg.iMsgType, stMsg.iDataSize);
        }
    }

    f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] rx done (%d)", iRecvCount);

    return 0U;
}

IPC_INT32 __cdecl f_IpcMsgQDemoStart(const char *cpName, IPC_INT32 iIsSender)
{
    if (InterlockedCompareExchange(&g_lDemoRunning, 1, 0) != 0)
    {
        return IPC_FAIL;
    }

    g_stDemoQueue = f_IpcMsgQCreate(cpName);
    if (g_stDemoQueue.iStatus != enum_IpcMsgQ_Status_Opened)
    {
        InterlockedExchange(&g_lDemoRunning, 0);
        return IPC_FAIL;
    }

    g_hDemoStop = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (g_hDemoStop == NULL)
    {
        (void)f_IpcMsgQClose(&g_stDemoQueue);
        InterlockedExchange(&g_lDemoRunning, 0);
        return IPC_FAIL;
    }

    g_iDemoIsSender = (iIsSender != 0) ? 1 : 0;
    g_hDemoThread   = (HANDLE)_beginthreadex(NULL, 0,
                                             (g_iDemoIsSender != 0) ? s_SenderProc : s_ReceiverProc,
                                             NULL, 0, NULL);
    if (g_hDemoThread == NULL)
    {
        f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] thread create failed");
        (void)f_IpcMsgQDemoStop();
        return IPC_FAIL;
    }

    f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] start (%s)", (g_iDemoIsSender != 0) ? "tx" : "rx");

    return IPC_PASS;
}

IPC_INT32 __cdecl f_IpcMsgQDemoStop(IPC_VOID)
{
    if (g_hDemoStop != NULL)
    {
        (void)SetEvent(g_hDemoStop);
    }

    if (g_hDemoThread != NULL)
    {
        (void)WaitForSingleObject(g_hDemoThread, 3000U);
        (void)CloseHandle(g_hDemoThread);
        g_hDemoThread = NULL;
    }

    if (g_hDemoStop != NULL)
    {
        (void)CloseHandle(g_hDemoStop);
        g_hDemoStop = NULL;
    }

    (void)f_IpcMsgQClose(&g_stDemoQueue);

    if (InterlockedCompareExchange(&g_lDemoRunning, 0, 1) == 1)
    {
        f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] stop");
    }

    return IPC_PASS;
}

IPC_INT32 __cdecl f_IpcMsgQDemoIsRunning(IPC_VOID)
{
    return (IPC_INT32)InterlockedCompareExchange(&g_lDemoRunning, 0, 0);
}
