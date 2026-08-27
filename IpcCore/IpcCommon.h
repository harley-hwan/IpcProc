//
// @file	IpcCommon.h
// @brief	IpcCore DLL 공통 정의 (내보내기 매크로 / 기본 자료형 / 로그 / 초기화 API)
// @author	hwan
// @date	2026.08.19.
//
#ifndef IPCCOMMON_H_
#define IPCCOMMON_H_

#include <stdint.h>
#include <winsock2.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef IPCCORE_EXPORTS
#define IPCCORE_API __declspec(dllexport)
#else
#define IPCCORE_API __declspec(dllimport)
#endif

#define IPC_PASS                1
#define IPC_FAIL                0

// 송신측 : 1 로 초기화된 int 를 0.1 초 간격으로 100 까지 1씩 증가시키며 송신
#define IPC_DEMO_FIRST_VALUE    1
#define IPC_DEMO_LAST_VALUE     100
#define IPC_DEMO_INTERVAL_MS    100

// 로그 채널
enum {
    enum_IpcLogCh_Core   = 0,
    enum_IpcLogCh_Thread = 1,
    enum_IpcLogCh_MsgQ   = 2,
    enum_IpcLogCh_Tcp    = 3
};

// 워커 쓰레드에서 호출된다. cpMessage 는 UTF-8 이고 콜백이 끝나면 무효.
typedef VOID (__cdecl *IPC_LOG_FN)(VOID *vpUserCtx, INT32 iChannel, const CHAR *cpMessage);

IPCCORE_API INT32 __cdecl f_IpcCoreInit(VOID);
IPCCORE_API VOID  __cdecl f_IpcCoreDeinit(VOID);
IPCCORE_API VOID  __cdecl f_IpcSetLogHandler(IPC_LOG_FN fnLog, VOID *vpUserCtx);
IPCCORE_API VOID  __cdecl f_IpcLog(INT32 iChannel, const CHAR *cpFormat, ...);

#ifdef __cplusplus
}
#endif

#endif // IPCCOMMON_H_
