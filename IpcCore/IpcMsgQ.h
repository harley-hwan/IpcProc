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

/* 공유메모리에 그대로 올라가므로 패킹 고정 */
typedef struct st_IpcMsg
{
    IPC_INT32                   iMsgType;
    IPC_INT32                   iDataSize;
    IPC_UCHAR8                  ucaData[IPC_MSGQ_PAYLOAD_MAX];
} st_IpcMsg;

#pragma pack(pop)

/* 프로세스 로컬 핸들. 공개 헤더가 windows.h 에 의존하지 않게 HANDLE 을 void* 로 담는다. */
typedef struct st_IpcMsgQ
{
    IPC_VOID *                  vpMapHandle;
    IPC_VOID *                  vpMutex;
    IPC_VOID *                  vpSemEmpty;
    IPC_VOID *                  vpSemFull;
    IPC_VOID *                  vpRing;
    IPC_INT32                   iIsCreator;
    IPC_INT32                   iStatus;
    char                        caName[IPC_MSGQ_NAME_MAX];
} st_IpcMsgQ;

/* Create 는 없으면 만들고 있으면 참여한다(기동 순서 무관), Open 은 있을 때만 참여 */
IPCCORE_API st_IpcMsgQ __cdecl f_IpcMsgQCreate(const char *cpName);
IPCCORE_API st_IpcMsgQ __cdecl f_IpcMsgQOpen(const char *cpName);
IPCCORE_API IPC_INT32  __cdecl f_IpcMsgQSend(st_IpcMsgQ *stpQ, const st_IpcMsg *stpMsg, IPC_UINT32 uiTimeOut_ms);
IPCCORE_API IPC_INT32  __cdecl f_IpcMsgQRecv(st_IpcMsgQ *stpQ, st_IpcMsg *stpMsg, IPC_UINT32 uiTimeOut_ms);
IPCCORE_API IPC_INT32  __cdecl f_IpcMsgQGetCount(const st_IpcMsgQ *stpQ);
IPCCORE_API IPC_INT32  __cdecl f_IpcMsgQClose(st_IpcMsgQ *stpQ);

/* iIsSender : 1 = 송신, 0 = 수신 */
IPCCORE_API IPC_INT32 __cdecl f_IpcMsgQDemoStart(const char *cpName, IPC_INT32 iIsSender);
IPCCORE_API IPC_INT32 __cdecl f_IpcMsgQDemoStop(IPC_VOID);
IPCCORE_API IPC_INT32 __cdecl f_IpcMsgQDemoIsRunning(IPC_VOID);

#ifdef __cplusplus
}
#endif

#endif /* IPCMSGQ_H_ */
