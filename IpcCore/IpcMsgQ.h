//
// @file	IpcMsgQ.h
// @brief	프로세스 간 메시지 송/수신 (메시지 큐)
//			Windows 에는 System V 메시지 큐가 없어서 아래 조합으로 같은 동작을 만든다.
//			  msgget  -> CreateFileMapping (네임드 공유메모리) + 링버퍼
//			  msgsnd  -> 세마포어(빈슬롯) 대기 -> 뮤텍스 -> write -> 세마포어(찬슬롯) 증가
//			  msgrcv  -> 세마포어(찬슬롯) 대기 -> 뮤텍스 -> read  -> 세마포어(빈슬롯) 증가
//			  mtype   -> st_IpcMsg.iMsgType
//			오브젝트 이름에 Local\ 을 쓰므로 관리자 권한은 필요 없다.
// @author	hwan
// @date	2026.08.19.
//
#ifndef IPCMSGQ_H_
#define IPCMSGQ_H_

#include "IpcCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IPC_MSGQ_NAME_MAX       64
#define IPC_MSGQ_PAYLOAD_MAX    256
#define IPC_MSGQ_CAPACITY       64
#define IPC_MSGQ_DEFAULT_NAME   "IpcDemoQ"

enum {
    enum_IpcMsgQ_Status_Error     = -1,
    enum_IpcMsgQ_Status_NotOpened =  0,
    enum_IpcMsgQ_Status_Opened    =  1
};

#pragma pack(push, 1)

//
// @struct	st_IpcMsg
// @brief	큐에 싣는 메시지. 공유메모리에 올라가므로 패킹 고정
//
typedef struct st_IpcMsg
{
    INT32                   iMsgType;
    INT32                   iDataSize;
    UCHAR                  ucaData[IPC_MSGQ_PAYLOAD_MAX];
} st_IpcMsg;

#pragma pack(pop)

//
// @struct	st_IpcMsgQ
// @brief	프로세스 로컬 큐 핸들. 공개 헤더가 windows.h 에 안 묶이게 HANDLE 을 void* 로 담는다
//
typedef struct st_IpcMsgQ
{
    VOID *                  vpMapHandle;
    VOID *                  vpMutex;
    VOID *                  vpSemEmpty;
    VOID *                  vpSemFull;
    VOID *                  vpRing;
    INT32                   iIsCreator;
    INT32                   iStatus;
    CHAR                        caName[IPC_MSGQ_NAME_MAX];
} st_IpcMsgQ;

// Create 는 없으면 만들고 있으면 참여한다(기동 순서 무관), Open 은 있을 때만 참여
IPCCORE_API st_IpcMsgQ __cdecl f_IpcMsgQCreate(const CHAR *cpName);
IPCCORE_API st_IpcMsgQ __cdecl f_IpcMsgQOpen(const CHAR *cpName);
IPCCORE_API INT32  __cdecl f_IpcMsgQSend(st_IpcMsgQ *stpQ, const st_IpcMsg *stpMsg, UINT32 uiTimeOut_ms);
IPCCORE_API INT32  __cdecl f_IpcMsgQRecv(st_IpcMsgQ *stpQ, st_IpcMsg *stpMsg, UINT32 uiTimeOut_ms);
IPCCORE_API INT32  __cdecl f_IpcMsgQGetCount(const st_IpcMsgQ *stpQ);
IPCCORE_API INT32  __cdecl f_IpcMsgQClose(st_IpcMsgQ *stpQ);

// iIsSender : 1 = 송신, 0 = 수신
IPCCORE_API INT32 __cdecl f_IpcMsgQDemoStart(const CHAR *cpName, INT32 iIsSender);
IPCCORE_API INT32 __cdecl f_IpcMsgQDemoStop(VOID);
IPCCORE_API INT32 __cdecl f_IpcMsgQDemoIsRunning(VOID);

#ifdef __cplusplus
}
#endif

#endif // IPCMSGQ_H_
