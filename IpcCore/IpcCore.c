//
// @file	IpcCore.c
// @brief	로그 파이프라인과 초기화/해제.
//			워커 쓰레드들이 f_IpcLog 로 남긴 로그를 등록된 콜백으로 전달한다.
// @author	hwan
// @date	2026.08.19.
//
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "IpcCore.h"

static CRITICAL_SECTION     g_stLogLock;
static LONG                 g_lLogLockReady = 0;
static IPC_LOG_FN           g_fnLogHandler  = NULL;
static IPC_VOID *           g_vpLogUserCtx  = NULL;

// DllMain 에서는 동기화 객체를 만들지 않는 편이 안전하므로 지연 초기화
static IPC_VOID s_EnsureLogLock(IPC_VOID)
{
    if (InterlockedCompareExchange(&g_lLogLockReady, 1, 0) == 0)
    {
        InitializeCriticalSection(&g_stLogLock);
        InterlockedExchange(&g_lLogLockReady, 2);
    }
    else
    {
        while (InterlockedCompareExchange(&g_lLogLockReady, 2, 2) != 2)
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
// @author	hwan
//
IPC_VOID __cdecl f_IpcSetLogHandler(IPC_LOG_FN fnLog, IPC_VOID *vpUserCtx)
{
    s_EnsureLogLock();

    EnterCriticalSection(&g_stLogLock);
    g_fnLogHandler = fnLog;
    g_vpLogUserCtx = vpUserCtx;
    LeaveCriticalSection(&g_stLogLock);
}

//
// @brief	printf 형식으로 로그 한 줄을 만들어 등록된 콜백으로 전달한다.
// @param	iChannel	로그 채널 (enum_IpcLogCh_*)
// @param	cpFormat	printf 형식 문자열 (UTF-8)
// @param	...			형식 인자
// @return	없음
// @author	hwan
//
IPC_VOID __cdecl f_IpcLog(IPC_INT32 iChannel, const char *cpFormat, ...)
{
    char        caBuff[512];
    va_list     stArgs;
    IPC_LOG_FN  fnLog;
    IPC_VOID *  vpCtx;

    if (cpFormat == NULL)
    {
        return;
    }

    s_EnsureLogLock();

    EnterCriticalSection(&g_stLogLock);
    fnLog = g_fnLogHandler;
    vpCtx = g_vpLogUserCtx;
    LeaveCriticalSection(&g_stLogLock);

    if (fnLog == NULL)
    {
        return;
    }

    va_start(stArgs, cpFormat);
    (void)vsnprintf(caBuff, sizeof(caBuff), cpFormat, stArgs);
    va_end(stArgs);
    caBuff[sizeof(caBuff) - 1] = '\0';

    fnLog(vpCtx, iChannel, caBuff);
}

//
// @brief	코어 초기화. Winsock 을 기동하고 쓰레드 간 큐를 준비한다.
// @return	IPC_PASS / IPC_FAIL
// @author	hwan
//
IPC_INT32 __cdecl f_IpcCoreInit(IPC_VOID)
{
    s_EnsureLogLock();

    if (f_SocketStartup() != SOCKET_PASS)
    {
        return IPC_FAIL;
    }

    return f_IpcThreadQueueInit();
}

//
// @brief	코어 해제. 진행 중인 데모를 모두 정지시키고 자원을 반납한다.
// @return	없음
// @author	hwan
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
