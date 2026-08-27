//
// @file	IpcMsgQ.c
// @brief	프로세스 간 메시지 송/수신 (메시지 큐).
//			커널 오브젝트 이름
//			  Local\\IpcProc.<이름>.map     공유메모리(링버퍼)
//			  Local\\IpcProc.<이름>.mtx     뮤텍스
//			  Local\\IpcProc.<이름>.sem.e   빈 슬롯 세마포어
//			  Local\\IpcProc.<이름>.sem.f   찬 슬롯 세마포어
// @author	hwan
// @date	2026.08.19.
//
#include <winsock2.h>
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <string.h>

#include "IpcCore.h"

// 메시지/링버퍼 구조체와 오브젝트 이름은 IpcInternalICD.h 에 있음
#define IPC_MSGQ_WAIT_SLICE_MS  200

static ST_IpcMsgQ           s_stDemoQueue;
static HANDLE               s_hDemoThread   = NULL;
static HANDLE               s_hDemoStop     = NULL;
static volatile LONG        s_lDemoRunning  = 0;
static INT32                s_iDemoIsSender = 0;

static VOID f_BuildObjName(wchar_t *wcpOut, const size_t szOutCch, const CHAR *cpName, const wchar_t *wcpSuffix)
{
    wchar_t wcaName[IPC_MSGQ_NAME_MAX];

    (VOID)memset(wcaName, 0, sizeof(wcaName));

    if (MultiByteToWideChar(CP_UTF8, 0, cpName, -1, wcaName, (INT32)(sizeof(wcaName) / sizeof(wcaName[0])) - 1) == 0)
    {
        (VOID)wcscpy_s(wcaName, sizeof(wcaName) / sizeof(wcaName[0]), L"Default");
    }

    // %s 도 되긴 하는데 %ls 가 표준이라 이걸 씀
    (VOID)_snwprintf_s(wcpOut, szOutCch, _TRUNCATE, L"%ls%ls%ls", IPC_MSGQ_OBJ_PREFIX, wcaName, wcpSuffix);
}

static VOID f_CloseHandleSafe(VOID **vppHandle)
{
    if ((vppHandle != NULL) && (*vppHandle != NULL))
    {
        (VOID)CloseHandle((HANDLE)(*vppHandle));
        *vppHandle = NULL;
    }
}

// 링버퍼 헤더 초기화는 뮤텍스 잡고 처음 한 번만 함
static INT32 f_PrepareRing(const ST_IpcMsgQ *stpQ)
{
    ST_IpcMsgQRing *stpRing = (ST_IpcMsgQRing *)stpQ->vpRing;
    DWORD           dwWait;
    INT32           iResult = IPC_PASS;

    dwWait = WaitForSingleObject((HANDLE)stpQ->vpMutex, 5000U);
    if ((dwWait != WAIT_OBJECT_0) && (dwWait != WAIT_ABANDONED))
    {
        return IPC_FAIL;
    }

    if (stpRing->uiMagic != IPC_MSGQ_MAGIC)
    {
        // 파일 매핑은 0 으로 채워져 나와서 head/tail/count 는 안 건드려도 됨
        stpRing->uiMagic     = IPC_MSGQ_MAGIC;
        stpRing->iCapacity   = IPC_MSGQ_CAPACITY;
        stpRing->iPayloadMax = IPC_MSGQ_PAYLOAD_MAX;
        stpRing->iHead       = 0;
        stpRing->iTail       = 0;
        stpRing->nCount      = 0;
    }
    else if ((stpRing->iCapacity != IPC_MSGQ_CAPACITY) || (stpRing->iPayloadMax != IPC_MSGQ_PAYLOAD_MAX))
    {
        f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] queue layout mismatch");
        iResult = IPC_FAIL;
    }

    (VOID)ReleaseMutex((HANDLE)stpQ->vpMutex);

    return iResult;
}

//
// @brief	큐 열기 공통. 공유메모리랑 동기화 객체 준비
// @param	cpName	큐 이름
// @param	iCreate	1 이면 없을 때 생성 / 0 이면 있을 때만 참여
// @return	큐 핸들 (iStatus 로 성공 여부 확인)
//
static ST_IpcMsgQ f_MsgQOpenInternal(const CHAR *cpName, const INT32 iCreate)
{
    ST_IpcMsgQ  st_Q;
    wchar_t     wcaObj[256];
    HANDLE      hMap;
    BOOL        bAlreadyExist = FALSE;

    (VOID)memset(&st_Q, 0, sizeof(st_Q));
    st_Q.iStatus = enum_IpcMsgQ_Status_Error;

    if ((cpName == NULL) || (cpName[0] == '\0'))
    {
        cpName = IPC_MSGQ_DEFAULT_NAME;
    }
    (VOID)strncpy_s(st_Q.caName, sizeof(st_Q.caName), cpName, _TRUNCATE);

    // 공유메모리
    f_BuildObjName(wcaObj, sizeof(wcaObj) / sizeof(wcaObj[0]), cpName, L".map");

    if (iCreate != 0)
    {
        hMap = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                  0, (DWORD)sizeof(ST_IpcMsgQRing), wcaObj);
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
        return st_Q;
    }
    st_Q.vpMapHandle = (VOID *)hMap;
    st_Q.iIsCreator  = ((iCreate != 0) && (bAlreadyExist == FALSE)) ? 1 : 0;

    st_Q.vpRing = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ST_IpcMsgQRing));
    if (st_Q.vpRing == NULL)
    {
        f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] map failed (%lu)", GetLastError());
        (VOID)f_IpcMsgQClose(&st_Q);
        return st_Q;
    }

    // 뮤텍스
    f_BuildObjName(wcaObj, sizeof(wcaObj) / sizeof(wcaObj[0]), cpName, L".mtx");
    st_Q.vpMutex = (VOID *)CreateMutexW(NULL, FALSE, wcaObj);

    // 세마포어 2개. 초기 카운트는 처음 만들 때만 먹힘
    f_BuildObjName(wcaObj, sizeof(wcaObj) / sizeof(wcaObj[0]), cpName, L".sem.e");
    st_Q.vpSemEmpty = (VOID *)CreateSemaphoreW(NULL, (LONG)IPC_MSGQ_CAPACITY, (LONG)IPC_MSGQ_CAPACITY, wcaObj);

    f_BuildObjName(wcaObj, sizeof(wcaObj) / sizeof(wcaObj[0]), cpName, L".sem.f");
    st_Q.vpSemFull = (VOID *)CreateSemaphoreW(NULL, 0, (LONG)IPC_MSGQ_CAPACITY, wcaObj);

    if ((st_Q.vpMutex == NULL) || (st_Q.vpSemEmpty == NULL) || (st_Q.vpSemFull == NULL))
    {
        f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] sync object failed (%lu)", GetLastError());
        (VOID)f_IpcMsgQClose(&st_Q);
        return st_Q;
    }

    if (f_PrepareRing(&st_Q) != IPC_PASS)
    {
        (VOID)f_IpcMsgQClose(&st_Q);
        return st_Q;
    }

    st_Q.iStatus = enum_IpcMsgQ_Status_Opened;

    f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] queue '%s' open", st_Q.caName);

    return st_Q;
}

// 없으면 만들고 있으면 붙음
ST_IpcMsgQ __cdecl f_IpcMsgQCreate(const CHAR *cpName)
{
    return f_MsgQOpenInternal(cpName, 1);
}

// 이미 있는 큐에만 붙음
ST_IpcMsgQ __cdecl f_IpcMsgQOpen(const CHAR *cpName)
{
    return f_MsgQOpenInternal(cpName, 0);
}

//
// @brief	큐에 메시지 한 건 송신 (msgsnd 대응). 세마포어(빈슬롯) 대기 -> 뮤텍스 -> write
// @param	stpQ			대상 큐 핸들
// @param	stpMsg			송신할 메시지
// @param	uiTimeOut_ms	빈 슬롯 대기 한도 (ms)
// @return	IPC_PASS / IPC_FAIL (타임아웃 포함)
//
INT32 __cdecl f_IpcMsgQSend(ST_IpcMsgQ *stpQ, const ST_IpcMsg *stpMsg, const UINT32 uiTimeOut_ms)
{
    ST_IpcMsgQRing *stpRing;
    DWORD           dwWait;

    if ((stpQ == NULL) || (stpMsg == NULL) || (stpQ->iStatus != enum_IpcMsgQ_Status_Opened))
    {
        return IPC_FAIL;
    }
    if ((stpMsg->iDataSize < 0) || (stpMsg->iDataSize > IPC_MSGQ_PAYLOAD_MAX))
    {
        return IPC_FAIL;
    }

    stpRing = (ST_IpcMsgQRing *)stpQ->vpRing;

    // 빈 슬롯 대기
    if (WaitForSingleObject((HANDLE)stpQ->vpSemEmpty, (DWORD)uiTimeOut_ms) != WAIT_OBJECT_0)
    {
        return IPC_FAIL;
    }

    // 프로세스 간 상호배제용 뮤텍스
    dwWait = WaitForSingleObject((HANDLE)stpQ->vpMutex, 5000U);
    if ((dwWait != WAIT_OBJECT_0) && (dwWait != WAIT_ABANDONED))
    {
        (VOID)ReleaseSemaphore((HANDLE)stpQ->vpSemEmpty, 1, NULL);
        return IPC_FAIL;
    }

    stpRing->staSlot[stpRing->iTail] = *stpMsg;
    stpRing->iTail = (stpRing->iTail + 1) % stpRing->iCapacity;
    stpRing->nCount++;

    (VOID)ReleaseMutex((HANDLE)stpQ->vpMutex);

    (VOID)ReleaseSemaphore((HANDLE)stpQ->vpSemFull, 1, NULL);

    return IPC_PASS;
}

//
// @brief	큐에서 메시지 한 건 수신 (msgrcv 대응). 세마포어(찬슬롯) 대기 -> 뮤텍스 -> read
// @param	stpQ			대상 큐 핸들
// @param	stpMsg			수신 메시지를 담을 버퍼
// @param	uiTimeOut_ms	찬 슬롯 대기 한도 (ms)
// @return	IPC_PASS / IPC_FAIL (타임아웃 포함)
//
INT32 __cdecl f_IpcMsgQRecv(ST_IpcMsgQ *stpQ, ST_IpcMsg *stpMsg, const UINT32 uiTimeOut_ms)
{
    ST_IpcMsgQRing *stpRing;
    DWORD           dwWait;

    if ((stpQ == NULL) || (stpMsg == NULL) || (stpQ->iStatus != enum_IpcMsgQ_Status_Opened))
    {
        return IPC_FAIL;
    }

    stpRing = (ST_IpcMsgQRing *)stpQ->vpRing;

    // 찬 슬롯 대기
    if (WaitForSingleObject((HANDLE)stpQ->vpSemFull, (DWORD)uiTimeOut_ms) != WAIT_OBJECT_0)
    {
        return IPC_FAIL;
    }

    dwWait = WaitForSingleObject((HANDLE)stpQ->vpMutex, 5000U);
    if ((dwWait != WAIT_OBJECT_0) && (dwWait != WAIT_ABANDONED))
    {
        (VOID)ReleaseSemaphore((HANDLE)stpQ->vpSemFull, 1, NULL);
        return IPC_FAIL;
    }

    *stpMsg = stpRing->staSlot[stpRing->iHead];
    stpRing->iHead = (stpRing->iHead + 1) % stpRing->iCapacity;
    stpRing->nCount--;

    (VOID)ReleaseMutex((HANDLE)stpQ->vpMutex);

    (VOID)ReleaseSemaphore((HANDLE)stpQ->vpSemEmpty, 1, NULL);

    return IPC_PASS;
}

INT32 __cdecl f_IpcMsgQGetCount(const ST_IpcMsgQ *stpQ)
{
    const ST_IpcMsgQRing *stpRing;

    if ((stpQ == NULL) || (stpQ->iStatus != enum_IpcMsgQ_Status_Opened))
    {
        return 0;
    }

    stpRing = (const ST_IpcMsgQRing *)stpQ->vpRing;

    return stpRing->nCount;
}

// 큐 닫기. 매핑 풀고 핸들 전부 반납
INT32 __cdecl f_IpcMsgQClose(ST_IpcMsgQ *stpQ)
{
    if (stpQ == NULL)
    {
        return IPC_FAIL;
    }

    if (stpQ->vpRing != NULL)
    {
        (VOID)UnmapViewOfFile(stpQ->vpRing);
        stpQ->vpRing = NULL;
    }

    f_CloseHandleSafe(&stpQ->vpSemFull);
    f_CloseHandleSafe(&stpQ->vpSemEmpty);
    f_CloseHandleSafe(&stpQ->vpMutex);
    f_CloseHandleSafe(&stpQ->vpMapHandle);

    stpQ->iStatus = enum_IpcMsgQ_Status_NotOpened;

    return IPC_PASS;
}

static INT32 f_IsStopRequested(const UINT32 uiWait_ms)
{
    if (s_hDemoStop == NULL)
    {
        return 1;
    }

    return (WaitForSingleObject(s_hDemoStop, (DWORD)uiWait_ms) == WAIT_OBJECT_0) ? 1 : 0;
}

// 데모 송신 쓰레드. 1~100 을 0.1 초마다 보냄
static UINT32 __stdcall f_SenderProc(VOID *vpArg)
{
    ST_IpcMsg   st_Msg;
    INT32       iData;
    INT32       nSeq = 0;

    (VOID)vpArg;

    for (iData = IPC_DEMO_FIRST_VALUE; iData <= IPC_DEMO_LAST_VALUE; iData++)
    {
        nSeq++;

        (VOID)memset(&st_Msg, 0, sizeof(st_Msg));
        st_Msg.iMsgType  = 1;
        st_Msg.iDataSize = (INT32)sizeof(iData);
        (VOID)memcpy(st_Msg.ucaData, &iData, sizeof(iData));

        if (f_IpcMsgQSend(&s_stDemoQueue, &st_Msg, 1000U) != IPC_PASS)
        {
            f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] tx full, data=%d", iData);
        }
        else
        {
            f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] tx %d", iData);
        }

        if (f_IsStopRequested(IPC_DEMO_INTERVAL_MS) != 0)
        {
            break;
        }
    }

    if (iData > IPC_DEMO_LAST_VALUE)
    {
        f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] tx done (%d)", nSeq);
    }

    return 0U;
}

// 데모 수신 쓰레드. 멈추라고 할 때까지 꺼내서 로그로 찍음
static UINT32 __stdcall f_ReceiverProc(VOID *vpArg)
{
    ST_IpcMsg   st_Msg;
    INT32       iData;
    INT32       nRecvCount = 0;

    (VOID)vpArg;

    while (f_IsStopRequested(0U) == 0)
    {
        if (f_IpcMsgQRecv(&s_stDemoQueue, &st_Msg, IPC_MSGQ_WAIT_SLICE_MS) != IPC_PASS)
        {
            continue;
        }

        nRecvCount++;

        if (st_Msg.iDataSize == (INT32)sizeof(iData))
        {
            (VOID)memcpy(&iData, st_Msg.ucaData, sizeof(iData));
            f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] rx %d", iData);
        }
        else
        {
            f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] rx type=%d size=%d", st_Msg.iMsgType, st_Msg.iDataSize);
        }
    }

    f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] rx done (%d)", nRecvCount);

    return 0U;
}

//
// @brief	데모 시작. 큐 만들고 역할에 맞는 쓰레드 띄움
// @param	cpName		큐 이름 (NULL/빈 문자열이면 기본 이름)
// @param	iIsSender	1:송신, 0:수신
// @return	IPC_PASS / IPC_FAIL (이미 동작 중 포함)
//
INT32 __cdecl f_IpcMsgQDemoStart(const CHAR *cpName, const INT32 iIsSender)
{
    if (InterlockedCompareExchange(&s_lDemoRunning, 1, 0) != 0)
    {
        return IPC_FAIL;
    }

    s_stDemoQueue = f_IpcMsgQCreate(cpName);
    if (s_stDemoQueue.iStatus != enum_IpcMsgQ_Status_Opened)
    {
        InterlockedExchange(&s_lDemoRunning, 0);
        return IPC_FAIL;
    }

    s_hDemoStop = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (s_hDemoStop == NULL)
    {
        (VOID)f_IpcMsgQClose(&s_stDemoQueue);
        InterlockedExchange(&s_lDemoRunning, 0);
        return IPC_FAIL;
    }

    s_iDemoIsSender = (iIsSender != 0) ? 1 : 0;
    s_hDemoThread   = (HANDLE)_beginthreadex(NULL, 0,
                                             (s_iDemoIsSender != 0) ? f_SenderProc : f_ReceiverProc,
                                             NULL, 0, NULL);
    if (s_hDemoThread == NULL)
    {
        f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] thread create failed");
        (VOID)f_IpcMsgQDemoStop();
        return IPC_FAIL;
    }

    f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] start (%s)", (s_iDemoIsSender != 0) ? "tx" : "rx");

    return IPC_PASS;
}

// 데모 정지. 쓰레드 끝나면 큐 닫음
INT32 __cdecl f_IpcMsgQDemoStop(VOID)
{
    if (s_hDemoStop != NULL)
    {
        (VOID)SetEvent(s_hDemoStop);
    }

    if (s_hDemoThread != NULL)
    {
        (VOID)WaitForSingleObject(s_hDemoThread, 3000U);
        (VOID)CloseHandle(s_hDemoThread);
        s_hDemoThread = NULL;
    }

    if (s_hDemoStop != NULL)
    {
        (VOID)CloseHandle(s_hDemoStop);
        s_hDemoStop = NULL;
    }

    (VOID)f_IpcMsgQClose(&s_stDemoQueue);

    if (InterlockedCompareExchange(&s_lDemoRunning, 0, 1) == 1)
    {
        f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] stop");
    }

    return IPC_PASS;
}

INT32 __cdecl f_IpcMsgQDemoIsRunning(VOID)
{
    return (INT32)InterlockedCompareExchange(&s_lDemoRunning, 0, 0);
}
