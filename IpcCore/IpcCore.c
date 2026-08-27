//
// @file	IpcCore.c
// @brief	로그 파이프라인과 코어 초기화/해제.
//			워커 쓰레드들이 f_IpcLog 로 남긴 로그를 등록된 콜백으로 전달함.
// @author	hwan
// @date	2026.08.19.
//
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "IpcCore.h"

static CRITICAL_SECTION     s_stLogLock;
static LONG                 s_lLogLockReady = 0;
static IPC_LOG_FN           p_fnLogHandler  = NULL;
static VOID *               p_vpLogUserCtx  = NULL;

// DllMain 에서 동기화 객체를 만들면 위험하므로 첫 사용 시점에 지연 초기화함
static VOID f_EnsureLogLock(VOID)
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

// 로그 콜백 등록/해제. fnLog 가 NULL 이면 해제임
VOID __cdecl f_IpcSetLogHandler(IPC_LOG_FN fnLog, VOID *vpUserCtx)
{
    f_EnsureLogLock();

    EnterCriticalSection(&s_stLogLock);
    p_fnLogHandler = fnLog;
    p_vpLogUserCtx = vpUserCtx;
    LeaveCriticalSection(&s_stLogLock);
}

//
// @brief	printf 형식으로 로그 한 줄을 만들어 등록된 콜백으로 전달함
// @param	iChannel	로그 채널 (enum_IpcLogCh_*)
// @param	cpFormat	printf 형식 문자열 (UTF-8)
// @return	없음
//
VOID __cdecl f_IpcLog(const INT32 iChannel, const CHAR *cpFormat, ...)
{
    CHAR        caBuff[512];
    va_list     stArgs;
    IPC_LOG_FN  fnLog;
    VOID *      vpCtx;

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

    va_start(stArgs, cpFormat);
    (VOID)vsnprintf(caBuff, sizeof(caBuff), cpFormat, stArgs);
    va_end(stArgs);
    caBuff[sizeof(caBuff) - 1] = '\0';

    fnLog(vpCtx, iChannel, caBuff);
}

//
// @brief	코어 초기화. Winsock 기동 + 쓰레드 간 큐 준비
// @return	IPC_PASS / IPC_FAIL
//
INT32 __cdecl f_IpcCoreInit(VOID)
{
    f_EnsureLogLock();

    if (f_SocketStartup() != SOCKET_PASS)
    {
        return IPC_FAIL;
    }

    return f_IpcThreadQueueInit();
}

//
// @brief	코어 해제. 진행 중인 데모를 모두 정지시키고 자원 반납
// @return	없음
//
VOID __cdecl f_IpcCoreDeinit(VOID)
{
    (VOID)f_IpcThreadDemoStop();
    (VOID)f_IpcMsgQDemoStop();
    (VOID)f_IpcTcpDemoStop();

    f_IpcThreadQueueDeinit();
    f_SocketCleanup();

    f_IpcSetLogHandler(NULL, NULL);
}
