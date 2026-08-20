//
// @file	IpcCore.c
// @brief	IpcCore DLL 의 로그 파이프라인과 초기화/해제 구현.
//			워커 쓰레드들이 f_IpcLog 로 남긴 로그를 등록된 콜백으로 전달한다.
// @author	harley-hwan
// @date	2026.08.19.
//
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "IpcCore.h"

static CRITICAL_SECTION     s_stLogLock;                        // 로그 핸들러 교체/조회 상호배제
static LONG                 s_lLogLockReady = 0;                // 0:미초기화, 1:초기화 중, 2:완료
static IPC_LOG_FN           p_fnLogHandler  = NULL;             // 등록된 로그 콜백
static IPC_VOID *           p_vpLogUserCtx  = NULL;             // 콜백에 되돌려 줄 사용자 컨텍스트

//
// @brief	로그 락 지연 초기화.
//			DllMain 에서는 동기화 객체를 만들지 않는 편이 안전하므로 최초 사용 시점에 만든다.
// @return	없음
// @author	harley-hwan
//
static IPC_VOID f_EnsureLogLock(IPC_VOID)
{
    if (InterlockedCompareExchange(&s_lLogLockReady, 1, 0) == 0)
    {
        InitializeCriticalSection(&s_stLogLock);
        InterlockedExchange(&s_lLogLockReady, 2);
    }
    else
    {
        while (InterlockedCompareExchange(&s_lLogLockReady, 2, 2) != 2)
        {
            Sleep(0);
        }
    }
}

//
// @brief	로그 콜백을 등록하거나 해제한다.
// @param	fnLog		로그 콜백 (NULL 이면 해제)
// @param	vpUserCtx	콜백 호출 시 그대로 전달되는 사용자 컨텍스트
// @return	없음
// @author	harley-hwan
//
IPC_VOID __cdecl f_IpcSetLogHandler(IPC_LOG_FN fnLog, IPC_VOID *vpUserCtx)
{
    f_EnsureLogLock();

    EnterCriticalSection(&s_stLogLock);
    p_fnLogHandler = fnLog;
    p_vpLogUserCtx = vpUserCtx;
    LeaveCriticalSection(&s_stLogLock);
}

//
// @brief	printf 형식으로 로그 한 줄을 만들어 등록된 콜백으로 전달한다.
//			콜백이 등록되어 있지 않으면 아무 일도 하지 않는다.
// @param	iChannel	로그 채널 (enum_IpcLogCh_*)
// @param	cpFormat	printf 형식 문자열 (UTF-8)
// @param	...			형식 인자
// @return	없음
// @author	harley-hwan
//
IPC_VOID __cdecl f_IpcLog(const IPC_INT32 iChannel, const char *cpFormat, ...)
{
    char        caBuff[512];
    va_list     vaArgs;
    IPC_LOG_FN  fnLog;
    IPC_VOID *  vpCtx;

    if (cpFormat == NULL)
    {
        return;
    }

    f_EnsureLogLock();

    EnterCriticalSection(&s_stLogLock);
    fnLog = p_fnLogHandler;
    vpCtx = p_vpLogUserCtx;
    LeaveCriticalSection(&s_stLogLock);

    if (fnLog == NULL)
    {
        return;
    }

    va_start(vaArgs, cpFormat);
    (void)vsnprintf(caBuff, sizeof(caBuff), cpFormat, vaArgs);
    va_end(vaArgs);
    caBuff[sizeof(caBuff) - 1] = '\0';

    fnLog(vpCtx, iChannel, caBuff);
}

//
// @brief	코어 초기화. Winsock 을 기동하고 쓰레드 간 큐를 준비한다.
// @return	IPC_PASS 성공 / IPC_FAIL 실패
// @author	harley-hwan
//
IPC_INT32 __cdecl f_IpcCoreInit(IPC_VOID)
{
    f_EnsureLogLock();

    if (f_SocketStartup() != SOCKET_PASS)
    {
        return IPC_FAIL;
    }

    return f_IpcThreadQueueInit();
}

//
// @brief	코어 해제. 진행 중인 데모를 모두 정지시키고 자원을 반납한다.
// @return	없음
// @author	harley-hwan
//
IPC_VOID __cdecl f_IpcCoreDeinit(IPC_VOID)
{
    (void)f_IpcThreadDemoStop();
    (void)f_IpcMsgQDemoStop();
    (void)f_IpcTcpDemoStop();

    f_IpcThreadQueueDeinit();
    f_SocketCleanup();

    f_IpcSetLogHandler(NULL, NULL);
}
