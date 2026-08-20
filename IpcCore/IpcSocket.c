//
// @file	IpcSocket.c
// @brief	TCP/IP 송/수신 (SocketUtility.h 의 Windows 포팅).
//			원본 .c 가 없어 헤더의 선언과 상수만 보고 구현했다.
// @author	hwan
// @date	2026.08.19.
//
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "IpcCore.h"

#pragma comment(lib, "Ws2_32.lib")

#define IPC_TCP_ACCEPT_SLICE_MS         200
#define IPC_TCP_CONNECT_TIMEOUT_MS      1000
#define IPC_TCP_CONNECT_RETRY_GAP_MS    500
#define IPC_TCP_DEFAULT_TIMEOUT_S       1

static INIT_ONCE            g_stWsaOnce = INIT_ONCE_STATIC_INIT;
static LONG                 g_lWsaReady = 0;

static SOCKET_INT32         g_iUDP_PartitionSize_Byte   = 8192;
static SOCKET_UINT32        g_uiUDP_PartitionSendGap_us = 0;

static volatile LONG        g_lCancelRequested = 0;

static st_Socket            g_stDemoSocket;
static HANDLE               g_hDemoThread   = NULL;
static HANDLE               g_hDemoStop     = NULL;
static volatile LONG        g_lDemoRunning  = 0;
static SOCKET_INT32         g_iDemoIsSender = 0;
static SOCKET_INT32         g_iDemoUseNBO   = 0;
static char                 g_caDemoIp[64]  = "127.0.0.1";
static SOCKET_UINT16        g_usDemoPort    = 51000U;

static BOOL CALLBACK s_WsaInitOnce(PINIT_ONCE ipInitOnce, PVOID vpParam, PVOID *vppCtx)
{
    WSADATA stWsaData;

    (void)ipInitOnce;
    (void)vpParam;
    (void)vppCtx;

    if (WSAStartup(MAKEWORD(2, 2), &stWsaData) != 0)
    {
        return FALSE;
    }

    InterlockedExchange(&g_lWsaReady, 1);

    return TRUE;
}

static SOCKET_INT32 s_EnsureWinsock(SOCKET_VOID)
{
    if (InitOnceExecuteOnce(&g_stWsaOnce, s_WsaInitOnce, NULL, NULL) == FALSE)
    {
        return SOCKET_FAIL;
    }

    return SOCKET_PASS;
}

static SOCKET_VOID s_LogSocketError(const char *cpWhere)
{
#if(DISP_SOCKET_ERROR_WARNING == 1)
    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] %s failed (%d)", cpWhere, WSAGetLastError());
#else
    (void)cpWhere;
#endif
}

static st_Socket s_MakeInvalidSocket(SOCKET_INT32 iSockType)
{
    st_Socket stSocket;

    (void)memset(&stSocket, 0, sizeof(stSocket));
    stSocket.hSockId        = SOCKET_INVALID_HANDLE;
    stSocket.hListenId      = SOCKET_INVALID_HANDLE;
    stSocket.iSockStatus    = enum_Socket_Status_Disconnected;
    stSocket.iSockType      = iSockType;
    stSocket.uiSockAddrSize = (SOCKET_UINT32)sizeof(struct sockaddr_in);

    return stSocket;
}

static SOCKET_INT32 s_FillSockAddr(struct sockaddr_in *stpAddr, const SOCKET_CHAR8 *cpIpAddr, const SOCKET_UINT16 usPortNum)
{
    (void)memset(stpAddr, 0, sizeof(*stpAddr));
    stpAddr->sin_family = AF_INET;
    stpAddr->sin_port   = htons(usPortNum);

    if ((cpIpAddr == NULL) || (cpIpAddr[0] == '\0'))
    {
        stpAddr->sin_addr.s_addr = htonl(INADDR_ANY);
        return SOCKET_PASS;
    }

    if (inet_pton(AF_INET, (const char *)cpIpAddr, &stpAddr->sin_addr) != 1)
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] bad ip: %s", (const char *)cpIpAddr);
        return SOCKET_FAIL;
    }

    return SOCKET_PASS;
}

// Windows 는 밀리초 DWORD 를 받는다
static SOCKET_VOID s_ApplyTimeout(SOCKET hSock, const SOCKET_INT64 lTimeOut_s, const SOCKET_INT64 lTimeOut_us)
{
    DWORD dwTimeOut_ms;

    if ((lTimeOut_s <= 0) && (lTimeOut_us <= 0))
    {
        return;                                     // 0 이하면 무한 대기
    }

    dwTimeOut_ms = (DWORD)((lTimeOut_s * 1000) + (lTimeOut_us / 1000));
    if (dwTimeOut_ms == 0U)
    {
        dwTimeOut_ms = 1U;
    }

    (void)setsockopt(hSock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&dwTimeOut_ms, (int)sizeof(dwTimeOut_ms));
    (void)setsockopt(hSock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&dwTimeOut_ms, (int)sizeof(dwTimeOut_ms));
}

static SOCKET_VOID s_SleepMicroseconds(SOCKET_UINT32 uiMicroSec)
{
    LARGE_INTEGER stFreq;
    LARGE_INTEGER stStart;
    LARGE_INTEGER stNow;
    LONGLONG      llTarget;

    if (uiMicroSec == 0U)
    {
        return;
    }

    if (uiMicroSec >= 2000U)
    {
        Sleep((DWORD)(uiMicroSec / 1000U));
        return;
    }

    // usleep 이 없으므로 짧은 대기는 QPC 스핀
    (void)QueryPerformanceFrequency(&stFreq);
    (void)QueryPerformanceCounter(&stStart);
    llTarget = stStart.QuadPart + ((stFreq.QuadPart * (LONGLONG)uiMicroSec) / 1000000LL);

    do
    {
        YieldProcessor();
        (void)QueryPerformanceCounter(&stNow);
    } while (stNow.QuadPart < llTarget);
}

static SOCKET_INT32 s_IsCancelRequested(SOCKET_VOID)
{
    return (InterlockedCompareExchange(&g_lCancelRequested, 0, 0) != 0) ? 1 : 0;
}

SOCKET_VOID __cdecl f_SocketGetVerInfo(st_VerInfo_SOCKET *stpVerInfo)
{
    if (stpVerInfo == NULL)
    {
        return;
    }

    stpVerInfo->iMajorVersion = SOCKETUTILITY_VER_MAJOR;
    stpVerInfo->iMinorVersion = SOCKETUTILITY_VER_MINOR;
    stpVerInfo->iBuildDate    = SOCKETUTILITY_BUILD_DATE;
}

SOCKET_INT32 __cdecl f_SocketStartup(SOCKET_VOID)
{
    return s_EnsureWinsock();
}

SOCKET_VOID __cdecl f_SocketCleanup(SOCKET_VOID)
{
    if (InterlockedCompareExchange(&g_lWsaReady, 0, 1) == 1)
    {
        (void)WSACleanup();
    }
}

SOCKET_VOID __cdecl f_SocketCancelBlockingCalls(SOCKET_VOID)
{
    InterlockedExchange(&g_lCancelRequested, 1);
}

SOCKET_VOID __cdecl f_SocketResetCancel(SOCKET_VOID)
{
    InterlockedExchange(&g_lCancelRequested, 0);
}

SOCKET_INT32 __cdecl f_SocketSetUDP_PartitionSize(const SOCKET_INT32 iUDP_PartitionSize_Byte)
{
    if ((iUDP_PartitionSize_Byte <= 0) || (iUDP_PartitionSize_Byte > 65507))
    {
        return SOCKET_FAIL;
    }

    g_iUDP_PartitionSize_Byte = iUDP_PartitionSize_Byte;

    return SOCKET_PASS;
}

SOCKET_VOID __cdecl f_SocketGetUDP_PartitionSize(SOCKET_INT32 *ipUDP_PartitionSize_Byte)
{
    if (ipUDP_PartitionSize_Byte != NULL)
    {
        *ipUDP_PartitionSize_Byte = g_iUDP_PartitionSize_Byte;
    }
}

SOCKET_INT32 __cdecl f_SocketSetUDP_PartitionSendGap(const SOCKET_UINT32 uiUDP_PartitionSendGap_us)
{
    g_uiUDP_PartitionSendGap_us = uiUDP_PartitionSendGap_us;

    return SOCKET_PASS;
}

SOCKET_VOID __cdecl f_SocketGetUDP_PartitionSendGap(SOCKET_UINT32 *uipUDP_PartitionSendGap_us)
{
    if (uipUDP_PartitionSendGap_us != NULL)
    {
        *uipUDP_PartitionSendGap_us = g_uiUDP_PartitionSendGap_us;
    }
}

// ---- UDP ----
st_Socket __cdecl f_SocketInitUDP_IPv4Tx(const SOCKET_CHAR8 *cpIpAddr, const SOCKET_UINT16 usPortNum)
{
    st_Socket stSocket = s_MakeInvalidSocket(enum_Socket_Type_UDP_IPv4Tx);
    SOCKET    hSock;

    if (s_EnsureWinsock() != SOCKET_PASS)
    {
        return stSocket;
    }

    // Tx 의 주소는 목적지
    if (s_FillSockAddr(&stSocket.stSockAddr, cpIpAddr, usPortNum) != SOCKET_PASS)
    {
        return stSocket;
    }

    hSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (hSock == INVALID_SOCKET)
    {
        s_LogSocketError("UDP Tx socket()");
        return stSocket;
    }

    stSocket.hSockId     = (SOCKET_HANDLE)hSock;
    stSocket.iSockStatus = enum_Socket_Status_NotDefined;

    f_IpcLog(enum_IpcLogCh_Tcp, "[UDP] tx %s:%u", (const char *)cpIpAddr, usPortNum);

    return stSocket;
}

st_Socket __cdecl f_SocketInitUDP_IPv4Rx(const SOCKET_CHAR8 *cpIpAddr, const SOCKET_UINT16 usPortNum,
                                         const SOCKET_INT64 lTimeOut_s, const SOCKET_INT64 lTimeOut_us)
{
    st_Socket stSocket = s_MakeInvalidSocket(enum_Socket_Type_UDP_IPv4Rx);
    SOCKET    hSock;

    if (s_EnsureWinsock() != SOCKET_PASS)
    {
        return stSocket;
    }

    // Rx 의 주소는 자기 bind 주소
    if (s_FillSockAddr(&stSocket.stSockAddr, cpIpAddr, usPortNum) != SOCKET_PASS)
    {
        return stSocket;
    }

    hSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (hSock == INVALID_SOCKET)
    {
        s_LogSocketError("UDP Rx socket()");
        return stSocket;
    }

    if (bind(hSock, (const struct sockaddr *)&stSocket.stSockAddr, (int)sizeof(stSocket.stSockAddr)) == SOCKET_ERROR)
    {
        s_LogSocketError("UDP Rx bind()");
        (void)closesocket(hSock);
        return stSocket;
    }

    s_ApplyTimeout(hSock, lTimeOut_s, lTimeOut_us);

    stSocket.hSockId              = (SOCKET_HANDLE)hSock;
    stSocket.iSockStatus          = enum_Socket_Status_NotDefined;
    stSocket.stTimeOutVal.tv_sec  = (long)lTimeOut_s;
    stSocket.stTimeOutVal.tv_usec = (long)lTimeOut_us;

    f_IpcLog(enum_IpcLogCh_Tcp, "[UDP] bind %s:%u", (const char *)cpIpAddr, usPortNum);

    return stSocket;
}

SOCKET_INT64 __cdecl f_SocketSendUDP_IPv4_Normal(const st_Socket *stpSocket, const SOCKET_VOID *vpDataAddr,
                                                 const SOCKET_INT64 lDataSize)
{
    int iSent;

    if ((stpSocket == NULL) || (vpDataAddr == NULL) || (lDataSize <= 0))
    {
        return -1;
    }
    if (stpSocket->hSockId == SOCKET_INVALID_HANDLE)
    {
        return -1;
    }

    iSent = sendto((SOCKET)stpSocket->hSockId, (const char *)vpDataAddr, (int)lDataSize, 0,
                   (const struct sockaddr *)&stpSocket->stSockAddr, (int)sizeof(stpSocket->stSockAddr));
    if (iSent == SOCKET_ERROR)
    {
        s_LogSocketError("UDP sendto()");
        return -1;
    }

    return (SOCKET_INT64)iSent;
}

SOCKET_INT64 __cdecl f_SocketSendUDP_IPv4_Partition(const st_Socket *stpSocket, const SOCKET_VOID *vpDataAddr,
                                                    const SOCKET_INT64 lFixedDataSize)
{
    const char * cpCursor;
    SOCKET_INT64 lRemain;
    SOCKET_INT64 lTotalSent = 0;

    if ((stpSocket == NULL) || (vpDataAddr == NULL) || (lFixedDataSize <= 0))
    {
        return -1;
    }

    cpCursor = (const char *)vpDataAddr;
    lRemain  = lFixedDataSize;

    while (lRemain > 0)
    {
        SOCKET_INT64 lChunk = (lRemain > (SOCKET_INT64)g_iUDP_PartitionSize_Byte)
                            ? (SOCKET_INT64)g_iUDP_PartitionSize_Byte : lRemain;
        int iSent = sendto((SOCKET)stpSocket->hSockId, cpCursor, (int)lChunk, 0,
                           (const struct sockaddr *)&stpSocket->stSockAddr, (int)sizeof(stpSocket->stSockAddr));
        if (iSent == SOCKET_ERROR)
        {
            s_LogSocketError("UDP sendto(partition)");
            return -1;
        }

        cpCursor   += iSent;
        lRemain    -= iSent;
        lTotalSent += iSent;

        if (lRemain > 0)
        {
            s_SleepMicroseconds(g_uiUDP_PartitionSendGap_us);
        }
    }

    return lTotalSent;
}

SOCKET_INT64 __cdecl f_SocketRecvUDP_IPv4_Normal(const st_Socket *stpSocket, const SOCKET_VOID *vpDataAddr,
                                                 const SOCKET_INT64 lMaxSize)
{
    int iRecv;

    if ((stpSocket == NULL) || (vpDataAddr == NULL) || (lMaxSize <= 0))
    {
        return -1;
    }

    iRecv = recvfrom((SOCKET)stpSocket->hSockId, (char *)vpDataAddr, (int)lMaxSize, 0, NULL, NULL);
    if (iRecv == SOCKET_ERROR)
    {
        if (WSAGetLastError() == WSAETIMEDOUT)
        {
            return 0;                               // 타임아웃
        }
        s_LogSocketError("UDP recvfrom()");
        return -1;
    }

    return (SOCKET_INT64)iRecv;
}

SOCKET_INT64 __cdecl f_SocketRecvUDP_IPv4_Partition(const st_Socket *stpSocket, const SOCKET_VOID *vpDataAddr,
                                                    const SOCKET_INT64 lFixedDataSize)
{
    char *       cpCursor;
    SOCKET_INT64 lRemain;
    SOCKET_INT64 lTotalRecv = 0;

    if ((stpSocket == NULL) || (vpDataAddr == NULL) || (lFixedDataSize <= 0))
    {
        return -1;
    }

    cpCursor = (char *)vpDataAddr;
    lRemain  = lFixedDataSize;

    while (lRemain > 0)
    {
        SOCKET_INT64 lChunk = (lRemain > (SOCKET_INT64)g_iUDP_PartitionSize_Byte)
                            ? (SOCKET_INT64)g_iUDP_PartitionSize_Byte : lRemain;
        int iRecv = recvfrom((SOCKET)stpSocket->hSockId, cpCursor, (int)lChunk, 0, NULL, NULL);
        if (iRecv == SOCKET_ERROR)
        {
            if ((WSAGetLastError() == WSAETIMEDOUT) && (lTotalRecv == 0))
            {
                return 0;
            }
            s_LogSocketError("UDP recvfrom(partition)");
            return -1;
        }

        cpCursor   += iRecv;
        lRemain    -= iRecv;
        lTotalRecv += iRecv;
    }

    return lTotalRecv;
}

// ---- TCP ----

//
// @brief	TCP 클라이언트(Tx) 초기화. 연결될 때까지 재시도한다.
//			블로킹 connect 는 실패 확정까지 오래 걸리므로 논블로킹 + select 로 처리한다.
// @param	cpIpAddr	서버 IPv4 주소
// @param	usPortNum	서버 포트 번호
// @param	lTimeOut_s	송/수신 타임아웃 (초)
// @param	lTimeOut_us	송/수신 타임아웃 (마이크로초)
// @return	소켓 구조체 (iSockStatus 로 성공 여부 확인)
// @author	hwan
//
st_Socket __cdecl f_SocketInitTCP_IPv4Tx_Normal(const SOCKET_CHAR8 *cpIpAddr, const SOCKET_UINT16 usPortNum,
                                                const SOCKET_INT64 lTimeOut_s, const SOCKET_INT64 lTimeOut_us)
{
    st_Socket          stSocket = s_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Tx);
    struct sockaddr_in stAddr;
    SOCKET             hSock;
    SOCKET_INT32       iConnected = 0;

    if (s_EnsureWinsock() != SOCKET_PASS)
    {
        return stSocket;
    }

    if (s_FillSockAddr(&stAddr, cpIpAddr, usPortNum) != SOCKET_PASS)
    {
        return stSocket;
    }

    do
    {
        u_long         ulNonBlocking = 1UL;
        fd_set         stWriteSet;
        fd_set         stErrorSet;
        struct timeval stTimeOut;
        int            iSelect;

        hSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (hSock == INVALID_SOCKET)
        {
            s_LogSocketError("TCP Tx socket()");
            return stSocket;
        }

        // 블로킹 connect 는 실패 확정까지 20초 이상 걸리므로 논블로킹 + select
        (void)ioctlsocket(hSock, FIONBIO, &ulNonBlocking);

        if (connect(hSock, (const struct sockaddr *)&stAddr, (int)sizeof(stAddr)) == SOCKET_ERROR)
        {
            if (WSAGetLastError() == WSAEWOULDBLOCK)
            {
                FD_ZERO(&stWriteSet);
                FD_ZERO(&stErrorSet);
                FD_SET(hSock, &stWriteSet);
                FD_SET(hSock, &stErrorSet);

                stTimeOut.tv_sec  = (long)(IPC_TCP_CONNECT_TIMEOUT_MS / 1000);
                stTimeOut.tv_usec = (long)((IPC_TCP_CONNECT_TIMEOUT_MS % 1000) * 1000);

                iSelect = select(0, NULL, &stWriteSet, &stErrorSet, &stTimeOut);
                if ((iSelect > 0) && (FD_ISSET(hSock, &stWriteSet) != 0))
                {
                    iConnected = 1;
                }
            }
        }
        else
        {
            iConnected = 1;
        }

        if (iConnected == 0)
        {
            (void)closesocket(hSock);
            hSock = INVALID_SOCKET;

#if(CTRL_SOCKET_REPEAT_CONNECT == 1)
            if (s_IsCancelRequested() != 0)
            {
                f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] connect canceled");
                return stSocket;
            }
            f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] retry connect %s:%u",
                     (const char *)cpIpAddr, usPortNum);
            Sleep((DWORD)IPC_TCP_CONNECT_RETRY_GAP_MS);
#else
            s_LogSocketError("TCP Tx connect()");
            return stSocket;
#endif
        }
    } while (iConnected == 0);

    {
        u_long ulBlocking = 0UL;
        (void)ioctlsocket(hSock, FIONBIO, &ulBlocking);
    }

    s_ApplyTimeout(hSock, lTimeOut_s, lTimeOut_us);

    stSocket.hSockId              = (SOCKET_HANDLE)hSock;
    stSocket.stSockAddr           = stAddr;
    stSocket.iSockStatus          = enum_Socket_Status_Connected;
    stSocket.stTimeOutVal.tv_sec  = (long)lTimeOut_s;
    stSocket.stTimeOutVal.tv_usec = (long)lTimeOut_us;

    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] connected %s:%u", (const char *)cpIpAddr, usPortNum);

    return stSocket;
}

//
// @brief	TCP 서버(Rx) 초기화. bind/listen 후 접속을 기다린다.
//			accept 를 select 로 잘게 나눠 대기해 정지 요청에 반응한다.
// @param	cpIpAddr	bind 할 IPv4 주소 (NULL/빈 문자열이면 INADDR_ANY)
// @param	usPortNum	bind 할 포트 번호
// @param	lTimeOut_s	송/수신 타임아웃 (초)
// @param	lTimeOut_us	송/수신 타임아웃 (마이크로초)
// @return	소켓 구조체 (iSockStatus 로 성공 여부 확인)
// @author	hwan
//
st_Socket __cdecl f_SocketInitTCP_IPv4Rx_Normal(const SOCKET_CHAR8 *cpIpAddr, const SOCKET_UINT16 usPortNum,
                                                const SOCKET_INT64 lTimeOut_s, const SOCKET_INT64 lTimeOut_us)
{
    st_Socket          stSocket = s_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Rx);
    struct sockaddr_in stAddr;
    struct sockaddr_in stPeerAddr;
    int                iPeerAddrSize = (int)sizeof(stPeerAddr);
    SOCKET             hListen;
    SOCKET             hAccept = INVALID_SOCKET;
    int                iReuse  = 1;
    char               caPeerIp[INET_ADDRSTRLEN];

    if (s_EnsureWinsock() != SOCKET_PASS)
    {
        return stSocket;
    }

    if (s_FillSockAddr(&stAddr, cpIpAddr, usPortNum) != SOCKET_PASS)
    {
        return stSocket;
    }

    hListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (hListen == INVALID_SOCKET)
    {
        s_LogSocketError("TCP Rx socket()");
        return stSocket;
    }

    // 재기동 시 TIME_WAIT 로 bind 가 막히는 것을 피한다.
    // Windows 의 SO_REUSEADDR 은 Linux 와 의미가 달라 운영 코드라면 SO_EXCLUSIVEADDRUSE 를 검토할 것
    (void)setsockopt(hListen, SOL_SOCKET, SO_REUSEADDR, (const char *)&iReuse, (int)sizeof(iReuse));

    if (bind(hListen, (const struct sockaddr *)&stAddr, (int)sizeof(stAddr)) == SOCKET_ERROR)
    {
        s_LogSocketError("TCP Rx bind()");
        (void)closesocket(hListen);
        return stSocket;
    }

    if (listen(hListen, SOCKET_TCP_QUEUE_SIZE) == SOCKET_ERROR)
    {
        s_LogSocketError("TCP Rx listen()");
        (void)closesocket(hListen);
        return stSocket;
    }

    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] listen %s:%u", (const char *)cpIpAddr, usPortNum);

    // accept 를 select 로 잘게 나눠 대기해야 정지 요청에 반응할 수 있다
    while (hAccept == INVALID_SOCKET)
    {
        fd_set         stReadSet;
        struct timeval stTimeOut;
        int            iSelect;

        if (s_IsCancelRequested() != 0)
        {
            f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] accept canceled");
            (void)closesocket(hListen);
            return stSocket;
        }

        FD_ZERO(&stReadSet);
        FD_SET(hListen, &stReadSet);
        stTimeOut.tv_sec  = 0;
        stTimeOut.tv_usec = (long)(IPC_TCP_ACCEPT_SLICE_MS * 1000);

        iSelect = select(0, &stReadSet, NULL, NULL, &stTimeOut);
        if (iSelect == SOCKET_ERROR)
        {
            s_LogSocketError("TCP Rx select()");
            (void)closesocket(hListen);
            return stSocket;
        }
        if (iSelect == 0)
        {
            continue;
        }

        hAccept = accept(hListen, (struct sockaddr *)&stPeerAddr, &iPeerAddrSize);
        if (hAccept == INVALID_SOCKET)
        {
            s_LogSocketError("TCP Rx accept()");
            (void)closesocket(hListen);
            return stSocket;
        }
    }

    s_ApplyTimeout(hAccept, lTimeOut_s, lTimeOut_us);

    (void)memset(caPeerIp, 0, sizeof(caPeerIp));
    (void)inet_ntop(AF_INET, &stPeerAddr.sin_addr, caPeerIp, sizeof(caPeerIp));

    stSocket.hSockId              = (SOCKET_HANDLE)hAccept;
    stSocket.hListenId            = (SOCKET_HANDLE)hListen;
    stSocket.stSockAddr           = stPeerAddr;
    stSocket.iSockStatus          = enum_Socket_Status_Connected;
    stSocket.stTimeOutVal.tv_sec  = (long)lTimeOut_s;
    stSocket.stTimeOutVal.tv_usec = (long)lTimeOut_us;

    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] accept %s:%u", caPeerIp, ntohs(stPeerAddr.sin_port));

    return stSocket;
}

// Sync 계열 핸드셰이크는 SOCKET_SYNC_PASS_* 상수만 보고 맞춘 것이다.
// Rx 가 LISTEN 을 보내고 Tx 가 CONNECT 로 답한다. 상대 절차가 다르면 이 두 함수만 고치면 된다.
st_Socket __cdecl f_SocketInitTCP_IPv4Tx_Sync(const SOCKET_CHAR8 *cpIpAddr, const SOCKET_UINT16 usPortNum,
                                              const SOCKET_INT64 lTimeOut_s, const SOCKET_INT64 lTimeOut_us)
{
    st_Socket     stSocket;
    SOCKET_UINT16 usSync = 0U;

    stSocket = f_SocketInitTCP_IPv4Tx_Normal(cpIpAddr, usPortNum, lTimeOut_s, lTimeOut_us);
    if (stSocket.iSockStatus != enum_Socket_Status_Connected)
    {
        return stSocket;
    }

    if (f_SocketRecvTCP_IPv4(&stSocket, &usSync, (SOCKET_INT64)sizeof(usSync)) != (SOCKET_INT64)sizeof(usSync))
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] sync failed (listen)");
        (void)f_SocketClose(stSocket);
        return s_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Tx);
    }

    if (usSync != (SOCKET_UINT16)SOCKET_SYNC_PASS_LISTEN)
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] sync bad value 0x%04X", usSync);
        (void)f_SocketClose(stSocket);
        return s_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Tx);
    }

    usSync = (SOCKET_UINT16)SOCKET_SYNC_PASS_CONNECT;
    (void)f_SocketSendTCP_IPv4(&stSocket, &usSync, (SOCKET_INT64)sizeof(usSync));

    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] sync ok");

    return stSocket;
}

st_Socket __cdecl f_SocketInitTCP_IPv4Rx_Sync(const SOCKET_CHAR8 *cpIpAddr, const SOCKET_UINT16 usPortNum,
                                              const SOCKET_INT64 lTimeOut_s, const SOCKET_INT64 lTimeOut_us,
                                              const SOCKET_INT32 iMaxSyncTime_s)
{
    st_Socket     stSocket;
    SOCKET_UINT16 usSync;

    stSocket = f_SocketInitTCP_IPv4Rx_Normal(cpIpAddr, usPortNum, lTimeOut_s, lTimeOut_us);
    if (stSocket.iSockStatus != enum_Socket_Status_Connected)
    {
        return stSocket;
    }

    // 동기화 구간에만 iMaxSyncTime_s 적용
    s_ApplyTimeout((SOCKET)stSocket.hSockId, (SOCKET_INT64)iMaxSyncTime_s, 0);

    usSync = (SOCKET_UINT16)SOCKET_SYNC_PASS_LISTEN;
    if (f_SocketSendTCP_IPv4(&stSocket, &usSync, (SOCKET_INT64)sizeof(usSync)) != (SOCKET_INT64)sizeof(usSync))
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] sync failed (listen)");
        (void)f_SocketClose(stSocket);
        return s_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Rx);
    }

    usSync = 0U;
    if (f_SocketRecvTCP_IPv4(&stSocket, &usSync, (SOCKET_INT64)sizeof(usSync)) != (SOCKET_INT64)sizeof(usSync))
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] sync timeout (%ds)", iMaxSyncTime_s);
        (void)f_SocketClose(stSocket);
        return s_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Rx);
    }

    if (usSync != (SOCKET_UINT16)SOCKET_SYNC_PASS_CONNECT)
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] sync bad value 0x%04X", usSync);
        (void)f_SocketClose(stSocket);
        return s_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Rx);
    }

    s_ApplyTimeout((SOCKET)stSocket.hSockId, lTimeOut_s, lTimeOut_us);

    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] sync ok");

    return stSocket;
}

//
// @brief	요청한 크기를 다 보낼 때까지 TCP 송신을 반복한다.
// @param	stpSocket		송신 소켓 (끊김 감지 시 상태 갱신)
// @param	vpDataAddr		송신 데이터 주소
// @param	lFixedDataSize	전체 송신 크기 (byte)
// @return	>0 처리 바이트 수 / 0 타임아웃(한 바이트도 못 함) / -1 에러 또는 상대 종료
// @author	hwan
//
SOCKET_INT64 __cdecl f_SocketSendTCP_IPv4(st_Socket *stpSocket, const SOCKET_VOID *vpDataAddr,
                                          const SOCKET_INT64 lFixedDataSize)
{
    const char * cpCursor;
    SOCKET_INT64 lRemain;
    SOCKET_INT64 lTotalSent = 0;

    if ((stpSocket == NULL) || (vpDataAddr == NULL) || (lFixedDataSize <= 0))
    {
        return -1;
    }
    if ((stpSocket->hSockId == SOCKET_INVALID_HANDLE) || (stpSocket->iSockStatus != enum_Socket_Status_Connected))
    {
        return -1;
    }

    cpCursor = (const char *)vpDataAddr;
    lRemain  = lFixedDataSize;

    while (lRemain > 0)
    {
        int iChunk = (lRemain > (SOCKET_INT64)INT_MAX) ? INT_MAX : (int)lRemain;
        int iSent  = send((SOCKET)stpSocket->hSockId, cpCursor, iChunk, 0);

        if (iSent == SOCKET_ERROR)
        {
            if (WSAGetLastError() == WSAETIMEDOUT)
            {
                if (lTotalSent == 0)
                {
                    return 0;
                }
                continue;                           // 프레임 중간이면 계속 보낸다
            }

            s_LogSocketError("TCP send()");
#if(CTRL_SOCKET_LINK_RECOVERY == 1)
            stpSocket->iSockStatus = enum_Socket_Status_Disconnected;
#endif
            return -1;
        }

        cpCursor   += iSent;
        lRemain    -= iSent;
        lTotalSent += iSent;
    }

    return lTotalSent;
}

//
// @brief	요청한 크기를 다 받을 때까지 TCP 수신을 반복한다.
// @param	stpSocket		수신 소켓 (끊김 감지 시 상태 갱신)
// @param	vpDataAddr		수신 버퍼 주소
// @param	lFixedDataSize	전체 수신 크기 (byte)
// @return	>0 처리 바이트 수 / 0 타임아웃(한 바이트도 못 함) / -1 에러 또는 상대 종료
// @author	hwan
//
SOCKET_INT64 __cdecl f_SocketRecvTCP_IPv4(st_Socket *stpSocket, const SOCKET_VOID *vpDataAddr,
                                          const SOCKET_INT64 lFixedDataSize)
{
    char *       cpCursor;
    SOCKET_INT64 lRemain;
    SOCKET_INT64 lTotalRecv = 0;

    if ((stpSocket == NULL) || (vpDataAddr == NULL) || (lFixedDataSize <= 0))
    {
        return -1;
    }
    if ((stpSocket->hSockId == SOCKET_INVALID_HANDLE) || (stpSocket->iSockStatus != enum_Socket_Status_Connected))
    {
        return -1;
    }

    cpCursor = (char *)vpDataAddr;
    lRemain  = lFixedDataSize;

    while (lRemain > 0)
    {
        int iChunk = (lRemain > (SOCKET_INT64)INT_MAX) ? INT_MAX : (int)lRemain;
        int iRecv  = recv((SOCKET)stpSocket->hSockId, cpCursor, iChunk, 0);

        if (iRecv == 0)
        {
            f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] peer closed");
#if(CTRL_SOCKET_LINK_RECOVERY == 1)
            stpSocket->iSockStatus = enum_Socket_Status_Disconnected;
#endif
            return -1;
        }

        if (iRecv == SOCKET_ERROR)
        {
            if (WSAGetLastError() == WSAETIMEDOUT)
            {
                if (lTotalRecv == 0)
                {
                    return 0;
                }
                continue;                           // 프레임 중간이면 계속 기다린다
            }

            s_LogSocketError("TCP recv()");
#if(CTRL_SOCKET_LINK_RECOVERY == 1)
            stpSocket->iSockStatus = enum_Socket_Status_Disconnected;
#endif
            return -1;
        }

        cpCursor   += iRecv;
        lRemain    -= iRecv;
        lTotalRecv += iRecv;
    }

    return lTotalRecv;
}

//
// @brief	소켓 닫기. 데이터 소켓과 listen 소켓을 모두 반납한다.
// @param	stSocket	닫을 소켓
// @return	SOCKET_PASS 고정
// @author	hwan
//
SOCKET_INT32 __cdecl f_SocketClose(st_Socket stSocket)
{
    if (stSocket.hSockId != SOCKET_INVALID_HANDLE)
    {
        (void)shutdown((SOCKET)stSocket.hSockId, SD_BOTH);
        (void)closesocket((SOCKET)stSocket.hSockId);
    }

    if (stSocket.hListenId != SOCKET_INVALID_HANDLE)
    {
        (void)closesocket((SOCKET)stSocket.hListenId);
    }

    return SOCKET_PASS;
}

// 데모 : int(4바이트) 하나를 프레임 헤더 없이 그대로 주고받는다.
// x64 Windows 와 x86-64 Linux 는 둘 다 little-endian 이라 기본값으로 값이 맞는다.
static SOCKET_INT32 s_IsDemoStopRequested(SOCKET_UINT32 uiWait_ms)
{
    if (g_hDemoStop == NULL)
    {
        return 1;
    }

    return (WaitForSingleObject(g_hDemoStop, (DWORD)uiWait_ms) == WAIT_OBJECT_0) ? 1 : 0;
}

//
// @brief	데모 송신(클라이언트) 쓰레드. 서버에 접속해 1 부터 100 까지 0.1 초 간격으로 송신한다.
// @param	vpArg	사용하지 않음
// @return	0 고정
// @author	hwan
//
static unsigned __stdcall s_TcpSenderProc(void *vpArg)
{
    SOCKET_INT32 iData;
    SOCKET_INT32 iSeq = 0;

    (void)vpArg;

    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] connecting %s:%u", g_caDemoIp, g_usDemoPort);

    g_stDemoSocket = f_SocketInitTCP_IPv4Tx_Normal((const SOCKET_CHAR8 *)g_caDemoIp, g_usDemoPort,
                                                   (SOCKET_INT64)IPC_TCP_DEFAULT_TIMEOUT_S, 0);
    if (g_stDemoSocket.iSockStatus != enum_Socket_Status_Connected)
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] connect failed");
        InterlockedExchange(&g_lDemoRunning, 0);
        return 0U;
    }

    for (iData = IPC_DEMO_FIRST_VALUE; iData <= IPC_DEMO_LAST_VALUE; iData++)
    {
        SOCKET_INT32 iWire = (g_iDemoUseNBO != 0) ? (SOCKET_INT32)htonl((u_long)iData) : iData;
        SOCKET_INT64 lSent;

        iSeq++;

        lSent = f_SocketSendTCP_IPv4(&g_stDemoSocket, &iWire, (SOCKET_INT64)sizeof(iWire));
        if (lSent == (SOCKET_INT64)sizeof(iWire))
        {
            f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] tx %d", iData);
        }
        else if (lSent == 0)
        {
            f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] tx timeout, data=%d", iData);
        }
        else
        {
            f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] tx failed, data=%d", iData);
            break;
        }

        if (s_IsDemoStopRequested(IPC_DEMO_INTERVAL_MS) != 0)
        {
            break;
        }
    }

    if (iData > IPC_DEMO_LAST_VALUE)
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] tx done (%d)", iSeq);
    }

    (void)f_SocketClose(g_stDemoSocket);
    g_stDemoSocket = s_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Tx);

    InterlockedExchange(&g_lDemoRunning, 0);

    return 0U;
}

//
// @brief	데모 수신(서버) 쓰레드. 접속을 받아 정지 요청까지 int 값을 수신해 로그로 출력한다.
// @param	vpArg	사용하지 않음
// @return	0 고정
// @author	hwan
//
static unsigned __stdcall s_TcpReceiverProc(void *vpArg)
{
    SOCKET_INT32 iRecvCount = 0;

    (void)vpArg;

    g_stDemoSocket = f_SocketInitTCP_IPv4Rx_Normal((const SOCKET_CHAR8 *)g_caDemoIp, g_usDemoPort,
                                                   (SOCKET_INT64)IPC_TCP_DEFAULT_TIMEOUT_S, 0);
    if (g_stDemoSocket.iSockStatus != enum_Socket_Status_Connected)
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] listen aborted");
        InterlockedExchange(&g_lDemoRunning, 0);
        return 0U;
    }

    while (s_IsDemoStopRequested(0U) == 0)
    {
        SOCKET_INT32 iWire = 0;
        SOCKET_INT64 lRecv;

        lRecv = f_SocketRecvTCP_IPv4(&g_stDemoSocket, &iWire, (SOCKET_INT64)sizeof(iWire));
        if (lRecv == 0)
        {
            continue;
        }
        if (lRecv < 0)
        {
            f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] rx stopped");
            break;
        }

        {
            SOCKET_INT32 iData = (g_iDemoUseNBO != 0) ? (SOCKET_INT32)ntohl((u_long)iWire) : iWire;

            iRecvCount++;
            f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] rx %d", iData);
        }
    }

    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] rx done (%d)", iRecvCount);

    (void)f_SocketClose(g_stDemoSocket);
    g_stDemoSocket = s_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Rx);

    InterlockedExchange(&g_lDemoRunning, 0);

    return 0U;
}

//
// @brief	TCP 데모 시작. 역할에 맞는 워커 쓰레드를 기동한다.
// @param	iIsSender				1:송신(클라이언트), 0:수신(서버)
// @param	cpIpAddr				상대(또는 bind) IPv4 주소 (비어 있으면 127.0.0.1)
// @param	usPortNum				포트 번호 (0 이면 51000)
// @param	iUseNetworkByteOrder	1:htonl/ntohl 적용
// @return	SOCKET_PASS / SOCKET_FAIL(이미 동작 중 포함)
// @author	hwan
//
SOCKET_INT32 __cdecl f_IpcTcpDemoStart(SOCKET_INT32 iIsSender, const SOCKET_CHAR8 *cpIpAddr,
                                       SOCKET_UINT16 usPortNum, SOCKET_INT32 iUseNetworkByteOrder)
{
    if (InterlockedCompareExchange(&g_lDemoRunning, 1, 0) != 0)
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] already running");
        return SOCKET_FAIL;
    }

    // 워커가 스스로 끝난 뒤 다시 시작하는 경우가 있어 이전 핸들을 먼저 반납한다
    if (g_hDemoThread != NULL)
    {
        (void)WaitForSingleObject(g_hDemoThread, 1000U);
        (void)CloseHandle(g_hDemoThread);
        g_hDemoThread = NULL;
    }
    if (g_hDemoStop != NULL)
    {
        (void)CloseHandle(g_hDemoStop);
        g_hDemoStop = NULL;
    }

    if (s_EnsureWinsock() != SOCKET_PASS)
    {
        InterlockedExchange(&g_lDemoRunning, 0);
        return SOCKET_FAIL;
    }

    (void)memset(g_caDemoIp, 0, sizeof(g_caDemoIp));
    if ((cpIpAddr != NULL) && (cpIpAddr[0] != '\0'))
    {
        (void)strncpy_s(g_caDemoIp, sizeof(g_caDemoIp), (const char *)cpIpAddr, _TRUNCATE);
    }
    else
    {
        (void)strncpy_s(g_caDemoIp, sizeof(g_caDemoIp), "127.0.0.1", _TRUNCATE);
    }

    g_usDemoPort    = (usPortNum != 0U) ? usPortNum : 51000U;
    g_iDemoIsSender = (iIsSender != 0) ? 1 : 0;
    g_iDemoUseNBO   = (iUseNetworkByteOrder != 0) ? 1 : 0;
    g_stDemoSocket  = s_MakeInvalidSocket(enum_Socket_Type_NotDefined);

    f_SocketResetCancel();

    g_hDemoStop = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (g_hDemoStop == NULL)
    {
        InterlockedExchange(&g_lDemoRunning, 0);
        return SOCKET_FAIL;
    }

    g_hDemoThread = (HANDLE)_beginthreadex(NULL, 0,
                                           (g_iDemoIsSender != 0) ? s_TcpSenderProc : s_TcpReceiverProc,
                                           NULL, 0, NULL);
    if (g_hDemoThread == NULL)
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] thread create failed");
        (void)f_IpcTcpDemoStop();
        return SOCKET_FAIL;
    }

    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] start (%s)", (g_iDemoIsSender != 0) ? "tx" : "rx");

    return SOCKET_PASS;
}

//
// @brief	TCP 데모 정지. 정지 이벤트와 중단 요청을 올리고 워커 쓰레드 종료를 기다린다.
// @return	SOCKET_PASS 고정
// @author	hwan
//
SOCKET_INT32 __cdecl f_IpcTcpDemoStop(SOCKET_VOID)
{
    if (g_hDemoStop != NULL)
    {
        (void)SetEvent(g_hDemoStop);
    }

    f_SocketCancelBlockingCalls();

    if (g_hDemoThread != NULL)
    {
        (void)WaitForSingleObject(g_hDemoThread, 5000U);
        (void)CloseHandle(g_hDemoThread);
        g_hDemoThread = NULL;
    }

    if (g_hDemoStop != NULL)
    {
        (void)CloseHandle(g_hDemoStop);
        g_hDemoStop = NULL;
    }

    if (InterlockedExchange(&g_lDemoRunning, 0) == 1)
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] stop");
    }

    f_SocketResetCancel();

    return SOCKET_PASS;
}

SOCKET_INT32 __cdecl f_IpcTcpDemoIsRunning(SOCKET_VOID)
{
    return (SOCKET_INT32)InterlockedCompareExchange(&g_lDemoRunning, 0, 0);
}
