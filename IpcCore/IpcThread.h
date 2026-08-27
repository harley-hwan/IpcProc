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

//
// @struct	st_IpcThreadMsg
// @brief	쓰레드 간 큐에 싣는 메시지
//
typedef struct st_IpcThreadMsg
{
    INT32                   iSeq;
    INT32                   iData;
} st_IpcThreadMsg;

IPCCORE_API INT32 __cdecl f_IpcThreadQueueInit(VOID);
IPCCORE_API VOID  __cdecl f_IpcThreadQueueDeinit(VOID);
IPCCORE_API INT32 __cdecl f_IpcThreadQueueSend(const st_IpcThreadMsg *stpMsg, UINT32 uiTimeOut_ms);
IPCCORE_API INT32 __cdecl f_IpcThreadQueueRecv(st_IpcThreadMsg *stpMsg, UINT32 uiTimeOut_ms);
IPCCORE_API INT32 __cdecl f_IpcThreadQueueGetCount(VOID);

IPCCORE_API INT32 __cdecl f_IpcThreadDemoStart(VOID);
IPCCORE_API INT32 __cdecl f_IpcThreadDemoStop(VOID);
IPCCORE_API INT32 __cdecl f_IpcThreadDemoIsRunning(VOID);

#ifdef __cplusplus
}
#endif

#endif // IPCTHREAD_H_
