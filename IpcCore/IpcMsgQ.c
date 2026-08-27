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

#define IPC_MSGQ_MAGIC          0x51435049U         // 'IPCQ'
#define IPC_MSGQ_OBJ_PREFIX     L"Local\\IpcProc."
#define IPC_MSGQ_WAIT_SLICE_MS  200

//
// @struct	st_IpcMsgQRing
// @brief	공유메모리에 올라가는 링버퍼
//
#pragma pack(push, 1)
typedef struct st_IpcMsgQRing
{
    UINT32                  uiMagic;
    INT32                   iCapacity;
    INT32                   iPayloadMax;
    INT32                   iHead;                          // 소비 인덱스
    INT32                   iTail;                          // 생산 인덱스
    INT32                   iCount;                         // 적재 건수
    st_IpcMsg                   staSlot[IPC_MSGQ_CAPACITY];
} st_IpcMsgQRing;
#pragma pack(pop)

static st_IpcMsgQ           s_stDemoQueue;
static HANDLE               s_hDemoThread  = NULL;
static HANDLE               s_hDemoStop    = NULL;
static volatile LONG        s_lDemoRunning = 0;
static INT32            s_iDemoIsSender = 0;

static VOID f_BuildObjName(wchar_t *wcpOut, size_t szOutCch, const CHAR *cpName, const wchar_t *wcpSuffix)
{
    wchar_t wcaName[IPC_MSGQ_NAME_MAX];

    (VOID)memset(wcaName, 0, sizeof(wcaName));

    if (MultiByteToWideChar(CP_UTF8, 0, cpName, -1, wcaName, (INT32)(sizeof(wcaName) / sizeof(wcaName[0])) - 1) == 0)
    {
        (VOID)wcscpy_s(wcaName, sizeof(wcaName) / sizeof(wcaName[0]), L"Default");
    }

    // %ls 를 쓴다 : MSVC 는 %s 도 와이드로 보지만 %ls 가 표준이고 이식성이 있다
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

// 링버퍼 헤더는 뮤텍스 안에서 처음 한 번만 초기화한다
static INT32 f_PrepareRing(st_IpcMsgQ *stpQ)
{
    st_IpcMsgQRing *stpRing = (st_IpcMsgQRing *)stpQ->vpRing;
    DWORD           dwWait;
    INT32       iResult = IPC_PASS;

    dwWait = WaitForSingleObject((HANDLE)stpQ->vpMutex, 5000U);
    if ((dwWait != WAIT_OBJECT_0) && (dwWait != WAIT_ABANDONED))
    {
        return IPC_FAIL;
    }

    if (stpRing->uiMagic != IPC_MSGQ_MAGIC)
    {
        // 파일 매핑은 0 으로 채워져 있어 head/tail/count 는 이미 0
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

    (VOID)ReleaseMutex((HANDLE)stpQ->vpMutex);

    return iResult;
}

//
// @brief	큐 열기 공통 처리. 공유메모리와 동기화 객체를 준비한다
// @param	cpName	큐 이름
// @param	iCreate	1 없으면 생성 / 0 있을 때만 참여
// @return	큐 핸들 (iStatus 로 확인)
// @author	hwan
//
static st_IpcMsgQ f_MsgQOpenInternal(const CHAR *cpName, INT32 iCreate)
{
    st_IpcMsgQ  stQ;
    wchar_t     wcaObj[256];
    HANDLE      hMap;
    BOOL        bAlreadyExist = FALSE;

    (VOID)memset(&stQ, 0, sizeof(stQ));
    stQ.iStatus = enum_IpcMsgQ_Status_Error;

    if ((cpName == NULL) || (cpName[0] == '\0'))
    {
        cpName = IPC_MSGQ_DEFAULT_NAME;
    }
    (VOID)strncpy_s(stQ.caName, sizeof(stQ.caName), cpName, _TRUNCATE);

    // 공유메모리
    f_BuildObjName(wcaObj, sizeof(wcaObj) / sizeof(wcaObj[0]), cpName, L".map");

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
    stQ.vpMapHandle = (VOID *)hMap;
    stQ.iIsCreator  = ((iCreate != 0) && (bAlreadyExist == FALSE)) ? 1 : 0;

    stQ.vpRing = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(st_IpcMsgQRing));
    if (stQ.vpRing == NULL)
    {
        f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] map failed (%lu)", GetLastError());
        (VOID)f_IpcMsgQClose(&stQ);
        return stQ;
    }

    // 뮤텍스
    f_BuildObjName(wcaObj, sizeof(wcaObj) / sizeof(wcaObj[0]), cpName, L".mtx");
    stQ.vpMutex = (VOID *)CreateMutexW(NULL, FALSE, wcaObj);

    // 세마포어 2개. 초기 카운트는 최초 생성 시에만 반영된다
    f_BuildObjName(wcaObj, sizeof(wcaObj) / sizeof(wcaObj[0]), cpName, L".sem.e");
    stQ.vpSemEmpty = (VOID *)CreateSemaphoreW(NULL, (LONG)IPC_MSGQ_CAPACITY, (LONG)IPC_MSGQ_CAPACITY, wcaObj);

    f_BuildObjName(wcaObj, sizeof(wcaObj) / sizeof(wcaObj[0]), cpName, L".sem.f");
    stQ.vpSemFull = (VOID *)CreateSemaphoreW(NULL, 0, (LONG)IPC_MSGQ_CAPACITY, wcaObj);

    if ((stQ.vpMutex == NULL) || (stQ.vpSemEmpty == NULL) || (stQ.vpSemFull == NULL))
    {
        f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] sync object failed (%lu)", GetLastError());
        (VOID)f_IpcMsgQClose(&stQ);
        return stQ;
    }

    if (f_PrepareRing(&stQ) != IPC_PASS)
    {
        (VOID)f_IpcMsgQClose(&stQ);
        return stQ;
    }

    stQ.iStatus = enum_IpcMsgQ_Status_Opened;

    f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] queue '%s' open", stQ.caName);

    return stQ;
}

//
// @brief	큐 생성. 없으면 만들고 있으면 참여한다 (기동 순서 무관).
// @param	cpName	큐 이름 (NULL/빈 문자열이면 기본 이름)
// @return	큐 핸들 (iStatus 로 성공 여부 확인)
// @author	hwan
//
st_IpcMsgQ __cdecl f_IpcMsgQCreate(const CHAR *cpName)
{
    return f_MsgQOpenInternal(cpName, 1);
}

//
// @brief	이미 있는 큐에 참여한다
// @param	cpName	큐 이름
// @return	큐 핸들 (iStatus 로 확인)
// @author	hwan
//
st_IpcMsgQ __cdecl f_IpcMsgQOpen(const CHAR *cpName)
{
    return f_MsgQOpenInternal(cpName, 0);
}

//
// @brief	큐에 메시지 한 건 송신 (msgsnd 대응). 세마포어(빈슬롯) 대기 -> 뮤텍스 -> write.
// @param	stpQ			대상 큐 핸들
// @param	stpMsg			송신할 메시지
// @param	uiTimeOut_ms	빈 슬롯 대기 한도 (ms)
// @return	IPC_PASS / IPC_FAIL(타임아웃 포함)
// @author	hwan
//
INT32 __cdecl f_IpcMsgQSend(st_IpcMsgQ *stpQ, const st_IpcMsg *stpMsg, UINT32 uiTimeOut_ms)
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

    // 빈 슬롯 대기
    if (WaitForSingleObject((HANDLE)stpQ->vpSemEmpty, (DWORD)uiTimeOut_ms) != WAIT_OBJECT_0)
    {
        return IPC_FAIL;
    }

    // 프로세스 경계를 넘는 상호배제
    dwWait = WaitForSingleObject((HANDLE)stpQ->vpMutex, 5000U);
    if ((dwWait != WAIT_OBJECT_0) && (dwWait != WAIT_ABANDONED))
    {
        (VOID)ReleaseSemaphore((HANDLE)stpQ->vpSemEmpty, 1, NULL);
        return IPC_FAIL;
    }

    stpRing->staSlot[stpRing->iTail] = *stpMsg;
    stpRing->iTail = (stpRing->iTail + 1) % stpRing->iCapacity;
    stpRing->iCount++;

    (VOID)ReleaseMutex((HANDLE)stpQ->vpMutex);


    (VOID)ReleaseSemaphore((HANDLE)stpQ->vpSemFull, 1, NULL);

    return IPC_PASS;
}

//
// @brief	큐에서 메시지 한 건 수신 (msgrcv 대응). 세마포어(찬슬롯) 대기 -> 뮤텍스 -> read.
// @param	stpQ			대상 큐 핸들
// @param	stpMsg			수신 메시지를 담을 버퍼
// @param	uiTimeOut_ms	찬 슬롯 대기 한도 (ms)
// @return	IPC_PASS / IPC_FAIL(타임아웃 포함)
// @author	hwan
//
INT32 __cdecl f_IpcMsgQRecv(st_IpcMsgQ *stpQ, st_IpcMsg *stpMsg, UINT32 uiTimeOut_ms)
{
    st_IpcMsgQRing *stpRing;
    DWORD           dwWait;

    if ((stpQ == NULL) || (stpMsg == NULL) || (stpQ->iStatus != enum_IpcMsgQ_Status_Opened))
    {
        return IPC_FAIL;
    }

    stpRing = (st_IpcMsgQRing *)stpQ->vpRing;

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
    stpRing->iCount--;

    (VOID)ReleaseMutex((HANDLE)stpQ->vpMutex);


    (VOID)ReleaseSemaphore((HANDLE)stpQ->vpSemEmpty, 1, NULL);

    return IPC_PASS;
}

INT32 __cdecl f_IpcMsgQGetCount(const st_IpcMsgQ *stpQ)
{
    const st_IpcMsgQRing *stpRing;

    if ((stpQ == NULL) || (stpQ->iStatus != enum_IpcMsgQ_Status_Opened))
    {
        return 0;
    }

    stpRing = (const st_IpcMsgQRing *)stpQ->vpRing;

    return stpRing->iCount;
}

//
// @brief	큐 닫기. 매핑을 해제하고 모든 핸들을 반납한다.
// @param	stpQ	대상 큐 핸들
// @return	IPC_PASS / IPC_FAIL(NULL 인자)
// @author	hwan
//
INT32 __cdecl f_IpcMsgQClose(st_IpcMsgQ *stpQ)
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

static INT32 f_IsStopRequested(UINT32 uiWait_ms)
{
    if (s_hDemoStop == NULL)
    {
        return 1;
    }

    return (WaitForSingleObject(s_hDemoStop, (DWORD)uiWait_ms) == WAIT_OBJECT_0) ? 1 : 0;
}

//
// @brief	데모 송신 쓰레드. 1 부터 100 까지 0.1 초 간격으로 큐에 송신한다.
// @param	vpArg	사용 안함
// @return	0 고정
// @author	hwan
//
static UINT32 __stdcall f_SenderProc(VOID *vpArg)
{
    st_IpcMsg   stMsg;
    INT32   iData;
    INT32   iSeq = 0;

    (VOID)vpArg;

    for (iData = IPC_DEMO_FIRST_VALUE; iData <= IPC_DEMO_LAST_VALUE; iData++)
    {
        iSeq++;

        (VOID)memset(&stMsg, 0, sizeof(stMsg));
        stMsg.iMsgType  = 1;
        stMsg.iDataSize = (INT32)sizeof(iData);
        (VOID)memcpy(stMsg.ucaData, &iData, sizeof(iData));

        if (f_IpcMsgQSend(&s_stDemoQueue, &stMsg, 1000U) != IPC_PASS)
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
        f_IpcLog(enum_IpcLogCh_MsgQ, "[MQ] tx done (%d)", iSeq);
    }

    return 0U;
}

//
// @brief	데모 수신 쓰레드. 정지 요청까지 큐에서 메시지를 꺼내 로그로 출력한다.
// @param	vpArg	사용 안함
// @return	0 고정
// @author	hwan
//
static UINT32 __stdcall f_ReceiverProc(VOID *vpArg)
{
    st_IpcMsg   stMsg;
    INT32   iData;
    INT32   iRecvCount = 0;

    (VOID)vpArg;

    while (f_IsStopRequested(0U) == 0)
    {
        if (f_IpcMsgQRecv(&s_stDemoQueue, &stMsg, IPC_MSGQ_WAIT_SLICE_MS) != IPC_PASS)
        {
            continue;
        }

        iRecvCount++;

        if (stMsg.iDataSize == (INT32)sizeof(iData))
        {
            (VOID)memcpy(&iData, stMsg.ucaData, sizeof(iData));
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

//
// @brief	메시지 큐 데모 시작. 큐를 만들고(또는 참여하고) 역할에 맞는 워커 쓰레드를 기동한다.
// @param	cpName		큐 이름 (NULL/빈 문자열이면 기본 이름)
// @param	iIsSender	1:송신, 0:수신
// @return	IPC_PASS / IPC_FAIL(이미 동작 중 포함)
// @author	hwan
//
INT32 __cdecl f_IpcMsgQDemoStart(const CHAR *cpName, INT32 iIsSender)
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

//
// @brief	메시지 큐 데모 정지. 워커 쓰레드 종료 후 큐를 닫는다.
// @return	IPC_PASS 고정
// @author	hwan
//
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
