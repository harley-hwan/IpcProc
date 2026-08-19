/* IpcCore.c : 로그 / 초기화 */
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

/* DllMain 에서는 동기화 객체를 만들지 않는 편이 안전하므로 지연 초기화 */
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

IPC_VOID __cdecl f_IpcSetLogHandler(IPC_LOG_FN fnLog, IPC_VOID *vpUserCtx)
{
    s_EnsureLogLock();

    EnterCriticalSection(&g_stLogLock);
    g_fnLogHandler = fnLog;
    g_vpLogUserCtx = vpUserCtx;
    LeaveCriticalSection(&g_stLogLock);
}

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

IPC_INT32 __cdecl f_IpcCoreInit(IPC_VOID)
{
    s_EnsureLogLock();

    if (f_SocketStartup() != SOCKET_PASS)
    {
        return IPC_FAIL;
    }

    return f_IpcThreadQueueInit();
}

IPC_VOID __cdecl f_IpcCoreDeinit(IPC_VOID)
{
    (void)f_IpcThreadDemoStop();
    (void)f_IpcMsgQDemoStop();
    (void)f_IpcTcpDemoStop();

    f_IpcThreadQueueDeinit();
    f_SocketCleanup();

    f_IpcSetLogHandler(NULL, NULL);
}
