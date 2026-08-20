//
// @file	IpcThread.h
// @brief	쓰레드 간 메시지 송/수신 (전역 링버퍼 + 뮤텍스 + 세마포어)
// @author	hwan
// @date	2026.08.19.
//
#ifndef IPCTHREAD_H_
#define IPCTHREAD_H_

#include "IpcCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IPC_THREAD_RING_SLOTS   8

typedef struct st_IpcThreadMsg
{
    IPC_INT32                   iSeq;
    IPC_INT32                   iData;
} st_IpcThreadMsg;

IPCCORE_API IPC_INT32 __cdecl f_IpcThreadQueueInit(IPC_VOID);
IPCCORE_API IPC_VOID  __cdecl f_IpcThreadQueueDeinit(IPC_VOID);
IPCCORE_API IPC_INT32 __cdecl f_IpcThreadQueueSend(const st_IpcThreadMsg *stpMsg, IPC_UINT32 uiTimeOut_ms);
IPCCORE_API IPC_INT32 __cdecl f_IpcThreadQueueRecv(st_IpcThreadMsg *stpMsg, IPC_UINT32 uiTimeOut_ms);
IPCCORE_API IPC_INT32 __cdecl f_IpcThreadQueueGetCount(IPC_VOID);

IPCCORE_API IPC_INT32 __cdecl f_IpcThreadDemoStart(IPC_VOID);
IPCCORE_API IPC_INT32 __cdecl f_IpcThreadDemoStop(IPC_VOID);
IPCCORE_API IPC_INT32 __cdecl f_IpcThreadDemoIsRunning(IPC_VOID);

#ifdef __cplusplus
}
#endif

#endif /* IPCTHREAD_H_ */
