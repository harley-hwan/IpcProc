//
// @file	IpcThread.h
// @brief	쓰레드 간 메시지 송/수신 (전역 링버퍼 + 뮤텍스 + 세마포어).
//			메시지 구조체는 IpcInternalICD.h 에 있음
// @author	hwan
// @date	2026.08.19.
//
#ifndef IPCTHREAD_H_
#define IPCTHREAD_H_

#include "IpcInternalICD.h"

#ifdef __cplusplus
extern "C" {
#endif

IPCCORE_API INT32 __cdecl f_IpcThreadQueueInit(VOID);
IPCCORE_API VOID  __cdecl f_IpcThreadQueueDeinit(VOID);
IPCCORE_API INT32 __cdecl f_IpcThreadQueueSend(const ST_IpcThreadMsg *stpMsg, const UINT32 uiTimeOut_ms);
IPCCORE_API INT32 __cdecl f_IpcThreadQueueRecv(ST_IpcThreadMsg *stpMsg, const UINT32 uiTimeOut_ms);
IPCCORE_API INT32 __cdecl f_IpcThreadQueueGetCount(VOID);

IPCCORE_API INT32 __cdecl f_IpcThreadDemoStart(VOID);
IPCCORE_API INT32 __cdecl f_IpcThreadDemoStop(VOID);
IPCCORE_API INT32 __cdecl f_IpcThreadDemoIsRunning(VOID);

#ifdef __cplusplus
}
#endif

#endif // IPCTHREAD_H_
