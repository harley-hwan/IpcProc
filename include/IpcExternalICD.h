//
// @file	IpcExternalICD.h
// @brief	외부 ICD. CSC 간(IpcUI <-> IpcCore), CSCI 간(상대 장비) 통신에 쓰는 공통 정의.
//			규칙대로 CSCI 당 1개만 두고 프로젝트_CSCI\include 에 둠.
// @author	hwan
// @date	2026.08.19.
//
#ifndef IPCEXTERNALICD_H_
#define IPCEXTERNALICD_H_

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

// 규칙에 있는 자료형 중 windows.h 에 없는 것만 정의.
// 나머지(UCHAR, INT32, VOID 등)는 windows.h 에 이미 있음. 그대로 사용.
typedef unsigned short int	USHORTINT;
typedef unsigned long int	ULONGINT;
typedef signed short int	SSHORT_INT;
typedef signed long			SLONG;
typedef signed long	int		SLONGINT;
typedef float				FLOAT32;
typedef double				FLOAT64;

#define IPC_PASS                1
#define IPC_FAIL                0

// 1 부터 100 까지 0.1 초 간격으로 1씩 증가시키면서 보냄
#define IPC_DEMO_FIRST_VALUE    1
#define IPC_DEMO_LAST_VALUE     100
#define IPC_DEMO_INTERVAL_MS    100

// TCP 핸드셰이크에 쓰는 값. SocketUtility v5.0 이랑 같음
#define SOCKET_SYNC_PASS_TX_LINE_CHECK      0xAAAA
#define SOCKET_SYNC_PASS_RX_LINE_CHECK      0xBBBB
#define SOCKET_SYNC_PASS_LISTEN             0xCCCC
#define SOCKET_SYNC_PASS_CONNECT            0xDDDD
#define SOCKET_SYNC_PASS_SEND_TO_TX         0xEEEE
#define SOCKET_SYNC_PASS_SEND_TO_RX         0xFFFF

// 로그 채널
enum {
    enum_IpcLogCh_Core   = 0,
    enum_IpcLogCh_Thread = 1,
    enum_IpcLogCh_MsgQ   = 2,
    enum_IpcLogCh_Tcp    = 3
};

// 워커 쓰레드에서 불림. cpMessage 는 UTF-8, 콜백 리턴 후에는 못 씀
typedef VOID (__cdecl *IPC_LOG_FN)(VOID *vpUserCtx, INT32 iChannel, const CHAR *cpMessage);

IPCCORE_API INT32 __cdecl f_IpcCoreInit(VOID);
IPCCORE_API VOID  __cdecl f_IpcCoreDeinit(VOID);
IPCCORE_API VOID  __cdecl f_IpcSetLogHandler(IPC_LOG_FN fnLog, VOID *vpUserCtx);
IPCCORE_API VOID  __cdecl f_IpcLog(const INT32 iChannel, const CHAR *cpFormat, ...);

#ifdef __cplusplus
}
#endif

#endif // IPCEXTERNALICD_H_
