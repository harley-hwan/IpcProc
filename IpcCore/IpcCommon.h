//
// @file	IpcCommon.h
// @brief	IpcCore DLL 공통 정의.
//			내보내기 매크로, 기본 자료형, 반환 코드, 데모 시나리오 상수,
//			로그 채널 및 초기화/로그 공개 API 를 선언한다.
// @author	harley-hwan
// @date	2026.08.19.
//
#ifndef IPCCOMMON_H_
#define IPCCOMMON_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// @def		IPCCORE_API
// @brief	DLL 내보내기/가져오기 지정자. IpcCore 빌드 시에만 IPCCORE_EXPORTS 가 정의된다.
#ifdef IPCCORE_EXPORTS
#define IPCCORE_API __declspec(dllexport)
#else
#define IPCCORE_API __declspec(dllimport)
#endif

// 코딩 규칙의 "기본 데이터형 대신 typedef 사용" 항목을 IPC_ 접두사로 적용한 것.
// windows.h(winnt.h) 가 CHAR/INT8/UINT32/VOID 등 같은 이름을 이미 정의(일부는 매크로)하므로
// 이름 충돌을 피하기 위해 모듈 접두사를 붙인다. (SocketUtility.h 의 SOCKET_ 계열과 같은 방식)
typedef void                    IPC_VOID;
typedef int8_t                  IPC_CHAR8;
typedef uint8_t                 IPC_UCHAR8;
typedef uint16_t                IPC_UINT16;
typedef int32_t                 IPC_INT32;
typedef uint32_t                IPC_UINT32;
typedef int64_t                 IPC_INT64;
typedef uint64_t                IPC_UINT64;

// @def		IPC_PASS
// @brief	함수 성공 반환값
#define IPC_PASS                1

// @def		IPC_FAIL
// @brief	함수 실패 반환값
#define IPC_FAIL                0

// 송신측 : 1 로 초기화된 int 를 0.1 초 간격으로 100 까지 1씩 증가시키며 송신

// @def		IPC_DEMO_FIRST_VALUE
// @brief	데모 송신 시작 값
#define IPC_DEMO_FIRST_VALUE    1

// @def		IPC_DEMO_LAST_VALUE
// @brief	데모 송신 종료 값
#define IPC_DEMO_LAST_VALUE     100

// @def		IPC_DEMO_INTERVAL_MS
// @brief	데모 송신 주기 (ms)
#define IPC_DEMO_INTERVAL_MS    100

// 로그 채널
enum {
    enum_IpcLogCh_Core   = 0,                       // 코어 공통
    enum_IpcLogCh_Thread = 1,                       // 쓰레드 간 송수신
    enum_IpcLogCh_MsgQ   = 2,                       // 프로세스 간 송수신(메시지 큐)
    enum_IpcLogCh_Tcp    = 3                        // TCP/IP 송수신
};

// 로그 콜백. 워커 쓰레드에서 호출된다. cpMessage 는 UTF-8 이고 콜백이 끝나면 무효.
typedef IPC_VOID (__cdecl *IPC_LOG_FN)(IPC_VOID *vpUserCtx, IPC_INT32 iChannel, const char *cpMessage);

// @brief	코어 초기화 (Winsock 기동 + 쓰레드 큐 준비)
IPCCORE_API IPC_INT32 __cdecl f_IpcCoreInit(IPC_VOID);
// @brief	코어 해제 (모든 데모 정지 + 자원 반납)
IPCCORE_API IPC_VOID  __cdecl f_IpcCoreDeinit(IPC_VOID);
// @brief	로그 콜백 등록/해제
IPCCORE_API IPC_VOID  __cdecl f_IpcSetLogHandler(IPC_LOG_FN fnLog, IPC_VOID *vpUserCtx);
// @brief	printf 형식 로그 출력
IPCCORE_API IPC_VOID  __cdecl f_IpcLog(const IPC_INT32 iChannel, const char *cpFormat, ...);

#ifdef __cplusplus
}
#endif

#endif /* IPCCOMMON_H_ */
