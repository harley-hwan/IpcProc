//
// @file	IpcCommon.h
// @brief	IpcCore DLL 공통 정의 (내보내기 매크로 / 기본 자료형 / 로그 / 초기화 API)
// @author	hwan
// @date	2026.08.19.
//
#ifndef IPCCOMMON_H_
#define IPCCOMMON_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef IPCCORE_EXPORTS
#define IPCCORE_API __declspec(dllexport)
#else
#define IPCCORE_API __declspec(dllimport)
#endif

typedef void                    IPC_VOID;
typedef int8_t                  IPC_CHAR8;
typedef uint8_t                 IPC_UCHAR8;
typedef uint16_t                IPC_UINT16;
typedef int32_t                 IPC_INT32;
typedef uint32_t                IPC_UINT32;
typedef int64_t                 IPC_INT64;
typedef uint64_t                IPC_UINT64;

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
typedef IPC_VOID (__cdecl *IPC_LOG_FN)(IPC_VOID *vpUserCtx, IPC_INT32 iChannel, const char *cpMessage);

IPCCORE_API IPC_INT32 __cdecl f_IpcCoreInit(IPC_VOID);
IPCCORE_API IPC_VOID  __cdecl f_IpcCoreDeinit(IPC_VOID);
IPCCORE_API IPC_VOID  __cdecl f_IpcSetLogHandler(IPC_LOG_FN fnLog, IPC_VOID *vpUserCtx);
IPCCORE_API IPC_VOID  __cdecl f_IpcLog(IPC_INT32 iChannel, const char *cpFormat, ...);

#ifdef __cplusplus
}
#endif

#endif // IPCCOMMON_H_
