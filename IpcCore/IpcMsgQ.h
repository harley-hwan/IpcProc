//
// @file	IpcMsgQ.h
// @brief	프로세스 간 메시지 송/수신 (메시지 큐).
//			Windows 에는 System V 메시지 큐가 없어서 아래 조합으로 같은 동작을 만듦.
//			  msgget -> CreateFileMapping (네임드 공유메모리) + 링버퍼
//			  msgsnd -> 세마포어(빈슬롯) 대기 -> 뮤텍스 -> write -> 세마포어(찬슬롯) 증가
//			  msgrcv -> 세마포어(찬슬롯) 대기 -> 뮤텍스 -> read  -> 세마포어(빈슬롯) 증가
//			  mtype  -> ST_IpcMsg.iMsgType
//			메시지/링버퍼 정의(ST_IpcMsg, ST_IpcMsgQRing)는 IpcInternalICD.h 에 있음.
//			오브젝트 이름에 Local\ 을 쓰므로 관리자 권한 필요 없음.
// @author	hwan
// @date	2026.08.19.
//
#ifndef IPCMSGQ_H_
#define IPCMSGQ_H_

#include "IpcInternalICD.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    enum_IpcMsgQ_Status_Error     = -1,
    enum_IpcMsgQ_Status_NotOpened =  0,
    enum_IpcMsgQ_Status_Opened    =  1
};

//
// @struct	ST_IpcMsgQ
// @brief	프로세스 로컬 큐 핸들. 공개 헤더가 windows.h 에 안 묶이게 HANDLE 은 void* 로 담음
//
typedef struct
{
    VOID *                  vpMapHandle;
    VOID *                  vpMutex;
    VOID *                  vpSemEmpty;
    VOID *                  vpSemFull;
    VOID *                  vpRing;
    INT32                   iIsCreator;
    INT32                   iStatus;
    CHAR                    caName[IPC_MSGQ_NAME_MAX];
} ST_IpcMsgQ;

// Create 는 없으면 만들고 있으면 참여함 (기동 순서 무관), Open 은 있을 때만 참여함
IPCCORE_API ST_IpcMsgQ __cdecl f_IpcMsgQCreate(const CHAR *cpName);
IPCCORE_API ST_IpcMsgQ __cdecl f_IpcMsgQOpen(const CHAR *cpName);
IPCCORE_API INT32      __cdecl f_IpcMsgQSend(ST_IpcMsgQ *stpQ, const ST_IpcMsg *stpMsg, const UINT32 uiTimeOut_ms);
IPCCORE_API INT32      __cdecl f_IpcMsgQRecv(ST_IpcMsgQ *stpQ, ST_IpcMsg *stpMsg, const UINT32 uiTimeOut_ms);
IPCCORE_API INT32      __cdecl f_IpcMsgQGetCount(const ST_IpcMsgQ *stpQ);
IPCCORE_API INT32      __cdecl f_IpcMsgQClose(ST_IpcMsgQ *stpQ);

// iIsSender : 1 = 송신, 0 = 수신
IPCCORE_API INT32 __cdecl f_IpcMsgQDemoStart(const CHAR *cpName, const INT32 iIsSender);
IPCCORE_API INT32 __cdecl f_IpcMsgQDemoStop(VOID);
IPCCORE_API INT32 __cdecl f_IpcMsgQDemoIsRunning(VOID);

#ifdef __cplusplus
}
#endif

#endif // IPCMSGQ_H_
