//
// @file	IpcSocket.c
// @brief	TCP/IP 송/수신 (SocketUtility.h 의 Windows 포팅).
//			원본 .c 가 없어서 헤더 선언과 상수만 보고 구현함.
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

static INIT_ONCE            s_stWsaOnce = INIT_ONCE_STATIC_INIT;
static LONG                 s_lWsaReady = 0;

static INT32                s_iUDP_PartitionSize_Byte   = 8192;
static UINT32               s_uiUDP_PartitionSendGap_us = 0;

static volatile LONG        s_lCancelRequested = 0;

static ST_Socket            s_stDemoSocket;
static HANDLE               s_hDemoThread   = NULL;
static HANDLE               s_hDemoStop     = NULL;
static volatile LONG        s_lDemoRunning  = 0;
static INT32                s_iDemoIsSender = 0;
static INT32                s_iDemoUseNBO   = 0;
static CHAR                 s_caDemoIp[64]  = "127.0.0.1";
static UINT16               s_usDemoPort    = 51000U;

static BOOL CALLBACK f_WsaInitOnce(PINIT_ONCE ipInitOnce, PVOID vpParam, PVOID *vppCtx)
{
    WSADATA st_WsaData;

    (VOID)ipInitOnce;
    (VOID)vpParam;
    (VOID)vppCtx;

    if (WSAStartup(MAKEWORD(2, 2), &st_WsaData) != 0)
    {
        return FALSE;
    }

    InterlockedExchange(&s_lWsaReady, 1);

    return TRUE;
}

static INT32 f_EnsureWinsock(VOID)
{
    if (InitOnceExecuteOnce(&s_stWsaOnce, f_WsaInitOnce, NULL, NULL) == FALSE)
    {
        return SOCKET_FAIL;
    }

    return SOCKET_PASS;
}

static VOID f_LogSocketError(const CHAR *cpWhere)
{
#if(DISP_SOCKET_ERROR_WARNING == 1)
    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] %s failed (%d)", cpWhere, WSAGetLastError());
#else
    (VOID)cpWhere;
#endif
}

static ST_Socket f_MakeInvalidSocket(const INT32 iSockType)
{
    ST_Socket st_Socket;

    (VOID)memset(&st_Socket, 0, sizeof(st_Socket));
    st_Socket.hSockId        = SOCKET_INVALID_HANDLE;
    st_Socket.hListenId      = SOCKET_INVALID_HANDLE;
    st_Socket.iSockStatus    = enum_Socket_Status_Disconnected;
    st_Socket.iSockType      = iSockType;
    st_Socket.uiSockAddrSize = (UINT32)sizeof(struct sockaddr_in);

    return st_Socket;
}

static INT32 f_FillSockAddr(struct sockaddr_in *stpAddr, const INT8 *cpIpAddr, const UINT16 usPortNum)
{
    (VOID)memset(stpAddr, 0, sizeof(*stpAddr));
    stpAddr->sin_family = AF_INET;
    stpAddr->sin_port   = htons(usPortNum);

    if ((cpIpAddr == NULL) || (cpIpAddr[0] == '\0'))
    {
        stpAddr->sin_addr.s_addr = htonl(INADDR_ANY);
        return SOCKET_PASS;
    }

    if (inet_pton(AF_INET, (const CHAR *)cpIpAddr, &stpAddr->sin_addr) != 1)
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] bad ip: %s", (const CHAR *)cpIpAddr);
        return SOCKET_FAIL;
    }

    return SOCKET_PASS;
}

// Windows 소켓 타임아웃은 timeval 이 아니라 ms 단위 DWORD 임
static VOID f_ApplyTimeout(const SOCKET hSock, const INT64 lTimeOut_s, const INT64 lTimeOut_us)
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

    (VOID)setsockopt(hSock, SOL_SOCKET, SO_RCVTIMEO, (const CHAR *)&dwTimeOut_ms, (INT32)sizeof(dwTimeOut_ms));
    (VOID)setsockopt(hSock, SOL_SOCKET, SO_SNDTIMEO, (const CHAR *)&dwTimeOut_ms, (INT32)sizeof(dwTimeOut_ms));
}

// usleep 이 없어서 긴 건 Sleep, 짧은 건 QPC 로 스핀
static VOID f_SleepMicroseconds(const UINT32 uiMicroSec)
{
    LARGE_INTEGER st_Freq;
    LARGE_INTEGER st_Start;
    LARGE_INTEGER st_Now;
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

    (VOID)QueryPerformanceFrequency(&st_Freq);
    (VOID)QueryPerformanceCounter(&st_Start);
    llTarget = st_Start.QuadPart + ((st_Freq.QuadPart * (LONGLONG)uiMicroSec) / 1000000LL);

    do
    {
        YieldProcessor();
        (VOID)QueryPerformanceCounter(&st_Now);
    } while (st_Now.QuadPart < llTarget);
}

static INT32 f_IsCancelRequested(VOID)
{
    return (InterlockedCompareExchange(&s_lCancelRequested, 0, 0) != 0) ? 1 : 0;
}

VOID __cdecl f_SocketGetVerInfo(ST_VerInfo_SOCKET *stpVerInfo)
{
    if (stpVerInfo == NULL)
    {
        return;
    }

    stpVerInfo->iMajorVersion = SOCKETUTILITY_VER_MAJOR;
    stpVerInfo->iMinorVersion = SOCKETUTILITY_VER_MINOR;
    stpVerInfo->iBuildDate    = SOCKETUTILITY_BUILD_DATE;
}

INT32 __cdecl f_SocketStartup(VOID)
{
    return f_EnsureWinsock();
}

VOID __cdecl f_SocketCleanup(VOID)
{
    if (InterlockedCompareExchange(&s_lWsaReady, 0, 1) == 1)
    {
        (VOID)WSACleanup();
    }
}

VOID __cdecl f_SocketCancelBlockingCalls(VOID)
{
    InterlockedExchange(&s_lCancelRequested, 1);
}

VOID __cdecl f_SocketResetCancel(VOID)
{
    InterlockedExchange(&s_lCancelRequested, 0);
}

INT32 __cdecl f_SocketSetUDP_PartitionSize(const INT32 iUDP_PartitionSize_Byte)
{
    if ((iUDP_PartitionSize_Byte <= 0) || (iUDP_PartitionSize_Byte > 65507))
    {
        return SOCKET_FAIL;
    }

    s_iUDP_PartitionSize_Byte = iUDP_PartitionSize_Byte;

    return SOCKET_PASS;
}

VOID __cdecl f_SocketGetUDP_PartitionSize(INT32 *ipUDP_PartitionSize_Byte)
{
    if (ipUDP_PartitionSize_Byte != NULL)
    {
        *ipUDP_PartitionSize_Byte = s_iUDP_PartitionSize_Byte;
    }
}

INT32 __cdecl f_SocketSetUDP_PartitionSendGap(const UINT32 uiUDP_PartitionSendGap_us)
{
    s_uiUDP_PartitionSendGap_us = uiUDP_PartitionSendGap_us;

    return SOCKET_PASS;
}

VOID __cdecl f_SocketGetUDP_PartitionSendGap(UINT32 *uipUDP_PartitionSendGap_us)
{
    if (uipUDP_PartitionSendGap_us != NULL)
    {
        *uipUDP_PartitionSendGap_us = s_uiUDP_PartitionSendGap_us;
    }
}

// ---- UDP ----

// UDP 송신 소켓. 주소는 보낼 곳
ST_Socket __cdecl f_SocketInitUDP_IPv4Tx(const INT8 *cpIpAddr, const UINT16 usPortNum)
{
    ST_Socket st_Socket = f_MakeInvalidSocket(enum_Socket_Type_UDP_IPv4Tx);
    SOCKET    hSock;

    if (f_EnsureWinsock() != SOCKET_PASS)
    {
        return st_Socket;
    }

    if (f_FillSockAddr(&st_Socket.stSockAddr, cpIpAddr, usPortNum) != SOCKET_PASS)
    {
        return st_Socket;
    }

    hSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (hSock == INVALID_SOCKET)
    {
        f_LogSocketError("UDP Tx socket()");
        return st_Socket;
    }

    st_Socket.hSockId     = (SOCKET_HANDLE)hSock;
    st_Socket.iSockStatus = enum_Socket_Status_NotDefined;

    f_IpcLog(enum_IpcLogCh_Tcp, "[UDP] tx %s:%u", (const CHAR *)cpIpAddr, usPortNum);

    return st_Socket;
}

// UDP 수신 소켓. 자기 주소에 bind 함
ST_Socket __cdecl f_SocketInitUDP_IPv4Rx(const INT8 *cpIpAddr, const UINT16 usPortNum,
                                         const INT64 lTimeOut_s, const INT64 lTimeOut_us)
{
    ST_Socket st_Socket = f_MakeInvalidSocket(enum_Socket_Type_UDP_IPv4Rx);
    SOCKET    hSock;

    if (f_EnsureWinsock() != SOCKET_PASS)
    {
        return st_Socket;
    }

    if (f_FillSockAddr(&st_Socket.stSockAddr, cpIpAddr, usPortNum) != SOCKET_PASS)
    {
        return st_Socket;
    }

    hSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (hSock == INVALID_SOCKET)
    {
        f_LogSocketError("UDP Rx socket()");
        return st_Socket;
    }

    if (bind(hSock, (const struct sockaddr *)&st_Socket.stSockAddr, (INT32)sizeof(st_Socket.stSockAddr)) == SOCKET_ERROR)
    {
        f_LogSocketError("UDP Rx bind()");
        (VOID)closesocket(hSock);
        return st_Socket;
    }

    f_ApplyTimeout(hSock, lTimeOut_s, lTimeOut_us);

    st_Socket.hSockId              = (SOCKET_HANDLE)hSock;
    st_Socket.iSockStatus          = enum_Socket_Status_NotDefined;
    st_Socket.stTimeOutVal.tv_sec  = (LONG)lTimeOut_s;
    st_Socket.stTimeOutVal.tv_usec = (LONG)lTimeOut_us;

    f_IpcLog(enum_IpcLogCh_Tcp, "[UDP] bind %s:%u", (const CHAR *)cpIpAddr, usPortNum);

    return st_Socket;
}

// UDP 송신. >0 보낸 바이트, -1 에러
INT64 __cdecl f_SocketSendUDP_IPv4_Normal(const ST_Socket *stpSocket, const VOID *vpDataAddr,
                                          const INT64 lDataSize)
{
    INT32 iSent;

    if ((stpSocket == NULL) || (vpDataAddr == NULL) || (lDataSize <= 0))
    {
        return -1;
    }
    if (stpSocket->hSockId == SOCKET_INVALID_HANDLE)
    {
        return -1;
    }

    iSent = sendto((SOCKET)stpSocket->hSockId, (const CHAR *)vpDataAddr, (INT32)lDataSize, 0,
                   (const struct sockaddr *)&stpSocket->stSockAddr, (INT32)sizeof(stpSocket->stSockAddr));
    if (iSent == SOCKET_ERROR)
    {
        f_LogSocketError("UDP sendto()");
        return -1;
    }

    return (INT64)iSent;
}

// UDP 분할 송신. 설정한 크기로 잘라 보냄
INT64 __cdecl f_SocketSendUDP_IPv4_Partition(const ST_Socket *stpSocket, const VOID *vpDataAddr,
                                             const INT64 lFixedDataSize)
{
    const CHAR *cpCursor;
    INT64       lRemain;
    INT64       lTotalSent = 0;

    if ((stpSocket == NULL) || (vpDataAddr == NULL) || (lFixedDataSize <= 0))
    {
        return -1;
    }

    cpCursor = (const CHAR *)vpDataAddr;
    lRemain  = lFixedDataSize;

    while (lRemain > 0)
    {
        INT64 lChunk = (lRemain > (INT64)s_iUDP_PartitionSize_Byte)
                            ? (INT64)s_iUDP_PartitionSize_Byte : lRemain;
        INT32 iSent = sendto((SOCKET)stpSocket->hSockId, cpCursor, (INT32)lChunk, 0,
                             (const struct sockaddr *)&stpSocket->stSockAddr, (INT32)sizeof(stpSocket->stSockAddr));
        if (iSent == SOCKET_ERROR)
        {
            f_LogSocketError("UDP sendto(partition)");
            return -1;
        }

        cpCursor   += iSent;
        lRemain    -= iSent;
        lTotalSent += iSent;

        if (lRemain > 0)
        {
            f_SleepMicroseconds(s_uiUDP_PartitionSendGap_us);
        }
    }

    return lTotalSent;
}

// UDP 수신. >0 받은 바이트, 0 타임아웃, -1 에러
INT64 __cdecl f_SocketRecvUDP_IPv4_Normal(const ST_Socket *stpSocket, const VOID *vpDataAddr,
                                          const INT64 lMaxSize)
{
    INT32 iRecv;

    if ((stpSocket == NULL) || (vpDataAddr == NULL) || (lMaxSize <= 0))
    {
        return -1;
    }

    iRecv = recvfrom((SOCKET)stpSocket->hSockId, (CHAR *)vpDataAddr, (INT32)lMaxSize, 0, NULL, NULL);
    if (iRecv == SOCKET_ERROR)
    {
        if (WSAGetLastError() == WSAETIMEDOUT)
        {
            return 0;
        }
        f_LogSocketError("UDP recvfrom()");
        return -1;
    }

    return (INT64)iRecv;
}

// UDP 분할 수신. 다 받을 때까지 반복함
INT64 __cdecl f_SocketRecvUDP_IPv4_Partition(const ST_Socket *stpSocket, const VOID *vpDataAddr,
                                             const INT64 lFixedDataSize)
{
    CHAR *cpCursor;
    INT64 lRemain;
    INT64 lTotalRecv = 0;

    if ((stpSocket == NULL) || (vpDataAddr == NULL) || (lFixedDataSize <= 0))
    {
        return -1;
    }

    cpCursor = (CHAR *)vpDataAddr;
    lRemain  = lFixedDataSize;

    while (lRemain > 0)
    {
        INT64 lChunk = (lRemain > (INT64)s_iUDP_PartitionSize_Byte)
                            ? (INT64)s_iUDP_PartitionSize_Byte : lRemain;
        INT32 iRecv = recvfrom((SOCKET)stpSocket->hSockId, cpCursor, (INT32)lChunk, 0, NULL, NULL);
        if (iRecv == SOCKET_ERROR)
        {
            if ((WSAGetLastError() == WSAETIMEDOUT) && (lTotalRecv == 0))
            {
                return 0;
            }
            f_LogSocketError("UDP recvfrom(partition)");
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
// @brief	TCP 클라이언트(Tx) 초기화. 연결될 때까지 재시도함.
//			블로킹 connect 는 실패 확정까지 20초 이상 걸려서 논블로킹 + select 로 처리함
// @param	cpIpAddr	서버 IPv4 주소
// @param	usPortNum	서버 포트 번호
// @param	lTimeOut_s	송/수신 타임아웃 (초)
// @param	lTimeOut_us	송/수신 타임아웃 (마이크로초)
// @return	소켓 구조체 (iSockStatus 로 성공 여부 확인)
//
ST_Socket __cdecl f_SocketInitTCP_IPv4Tx_Normal(const INT8 *cpIpAddr, const UINT16 usPortNum,
                                                const INT64 lTimeOut_s, const INT64 lTimeOut_us)
{
    ST_Socket          st_Socket = f_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Tx);
    struct sockaddr_in st_Addr;
    SOCKET             hSock;
    INT32              iConnected = 0;

    if (f_EnsureWinsock() != SOCKET_PASS)
    {
        return st_Socket;
    }

    if (f_FillSockAddr(&st_Addr, cpIpAddr, usPortNum) != SOCKET_PASS)
    {
        return st_Socket;
    }

    do
    {
        u_long         ulNonBlocking = 1UL;
        fd_set         st_WriteSet;
        fd_set         st_ErrorSet;
        struct timeval st_TimeOut;
        INT32          iSelect;

        hSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (hSock == INVALID_SOCKET)
        {
            f_LogSocketError("TCP Tx socket()");
            return st_Socket;
        }

        (VOID)ioctlsocket(hSock, FIONBIO, &ulNonBlocking);

        if (connect(hSock, (const struct sockaddr *)&st_Addr, (INT32)sizeof(st_Addr)) == SOCKET_ERROR)
        {
            if (WSAGetLastError() == WSAEWOULDBLOCK)
            {
                FD_ZERO(&st_WriteSet);
                FD_ZERO(&st_ErrorSet);
                FD_SET(hSock, &st_WriteSet);
                FD_SET(hSock, &st_ErrorSet);

                st_TimeOut.tv_sec  = (LONG)(IPC_TCP_CONNECT_TIMEOUT_MS / 1000);
                st_TimeOut.tv_usec = (LONG)((IPC_TCP_CONNECT_TIMEOUT_MS % 1000) * 1000);

                iSelect = select(0, NULL, &st_WriteSet, &st_ErrorSet, &st_TimeOut);
                if ((iSelect > 0) && (FD_ISSET(hSock, &st_WriteSet) != 0))
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
            (VOID)closesocket(hSock);
            hSock = INVALID_SOCKET;

#if(CTRL_SOCKET_REPEAT_CONNECT == 1)
            if (f_IsCancelRequested() != 0)
            {
                f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] connect canceled");
                return st_Socket;
            }
            f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] retry connect %s:%u",
                     (const CHAR *)cpIpAddr, usPortNum);
            Sleep((DWORD)IPC_TCP_CONNECT_RETRY_GAP_MS);
#else
            f_LogSocketError("TCP Tx connect()");
            return st_Socket;
#endif
        }
    } while (iConnected == 0);

    {
        u_long ulBlocking = 0UL;
        (VOID)ioctlsocket(hSock, FIONBIO, &ulBlocking);
    }

    f_ApplyTimeout(hSock, lTimeOut_s, lTimeOut_us);

    st_Socket.hSockId              = (SOCKET_HANDLE)hSock;
    st_Socket.stSockAddr           = st_Addr;
    st_Socket.iSockStatus          = enum_Socket_Status_Connected;
    st_Socket.stTimeOutVal.tv_sec  = (LONG)lTimeOut_s;
    st_Socket.stTimeOutVal.tv_usec = (LONG)lTimeOut_us;

    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] connected %s:%u", (const CHAR *)cpIpAddr, usPortNum);

    return st_Socket;
}

//
// @brief	TCP 서버(Rx) 초기화. bind/listen 후 접속을 기다림.
//			accept 를 select 로 잘게 나눠 대기해야 정지 요청에 반응할 수 있음
// @param	cpIpAddr	bind 할 IPv4 주소 (NULL/빈 문자열이면 INADDR_ANY)
// @param	usPortNum	bind 할 포트 번호
// @param	lTimeOut_s	송/수신 타임아웃 (초)
// @param	lTimeOut_us	송/수신 타임아웃 (마이크로초)
// @return	소켓 구조체 (iSockStatus 로 성공 여부 확인)
//
ST_Socket __cdecl f_SocketInitTCP_IPv4Rx_Normal(const INT8 *cpIpAddr, const UINT16 usPortNum,
                                                const INT64 lTimeOut_s, const INT64 lTimeOut_us)
{
    ST_Socket          st_Socket = f_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Rx);
    struct sockaddr_in st_Addr;
    struct sockaddr_in st_PeerAddr;
    INT32              iPeerAddrSize = (INT32)sizeof(st_PeerAddr);
    SOCKET             hListen;
    SOCKET             hAccept = INVALID_SOCKET;
    INT32              iReuse  = 1;
    CHAR               caPeerIp[INET_ADDRSTRLEN];

    if (f_EnsureWinsock() != SOCKET_PASS)
    {
        return st_Socket;
    }

    if (f_FillSockAddr(&st_Addr, cpIpAddr, usPortNum) != SOCKET_PASS)
    {
        return st_Socket;
    }

    hListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (hListen == INVALID_SOCKET)
    {
        f_LogSocketError("TCP Rx socket()");
        return st_Socket;
    }

    // 재기동하면 TIME_WAIT 때문에 bind 가 막혀서 넣음.
    // Windows 의 SO_REUSEADDR 은 Linux 랑 의미가 좀 달라서 실제 운영 코드면 SO_EXCLUSIVEADDRUSE 쓸 것
    (VOID)setsockopt(hListen, SOL_SOCKET, SO_REUSEADDR, (const CHAR *)&iReuse, (INT32)sizeof(iReuse));

    if (bind(hListen, (const struct sockaddr *)&st_Addr, (INT32)sizeof(st_Addr)) == SOCKET_ERROR)
    {
        f_LogSocketError("TCP Rx bind()");
        (VOID)closesocket(hListen);
        return st_Socket;
    }

    if (listen(hListen, SOCKET_TCP_QUEUE_SIZE) == SOCKET_ERROR)
    {
        f_LogSocketError("TCP Rx listen()");
        (VOID)closesocket(hListen);
        return st_Socket;
    }

    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] listen %s:%u", (const CHAR *)cpIpAddr, usPortNum);

    while (hAccept == INVALID_SOCKET)
    {
        fd_set         st_ReadSet;
        struct timeval st_TimeOut;
        INT32          iSelect;

        if (f_IsCancelRequested() != 0)
        {
            f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] accept canceled");
            (VOID)closesocket(hListen);
            return st_Socket;
        }

        FD_ZERO(&st_ReadSet);
        FD_SET(hListen, &st_ReadSet);
        st_TimeOut.tv_sec  = 0;
        st_TimeOut.tv_usec = (LONG)(IPC_TCP_ACCEPT_SLICE_MS * 1000);

        iSelect = select(0, &st_ReadSet, NULL, NULL, &st_TimeOut);
        if (iSelect == SOCKET_ERROR)
        {
            f_LogSocketError("TCP Rx select()");
            (VOID)closesocket(hListen);
            return st_Socket;
        }
        if (iSelect == 0)
        {
            continue;
        }

        hAccept = accept(hListen, (struct sockaddr *)&st_PeerAddr, &iPeerAddrSize);
        if (hAccept == INVALID_SOCKET)
        {
            f_LogSocketError("TCP Rx accept()");
            (VOID)closesocket(hListen);
            return st_Socket;
        }
    }

    f_ApplyTimeout(hAccept, lTimeOut_s, lTimeOut_us);

    (VOID)memset(caPeerIp, 0, sizeof(caPeerIp));
    (VOID)inet_ntop(AF_INET, &st_PeerAddr.sin_addr, caPeerIp, sizeof(caPeerIp));

    st_Socket.hSockId              = (SOCKET_HANDLE)hAccept;
    st_Socket.hListenId            = (SOCKET_HANDLE)hListen;
    st_Socket.stSockAddr           = st_PeerAddr;
    st_Socket.iSockStatus          = enum_Socket_Status_Connected;
    st_Socket.stTimeOutVal.tv_sec  = (LONG)lTimeOut_s;
    st_Socket.stTimeOutVal.tv_usec = (LONG)lTimeOut_us;

    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] accept %s:%u", caPeerIp, ntohs(st_PeerAddr.sin_port));

    return st_Socket;
}

// Sync 쪽 핸드셰이크는 SOCKET_SYNC_PASS_* 값만 보고 맞춘 것.
// Rx 가 LISTEN 을 보내면 Tx 가 CONNECT 로 답함. 상대 절차가 다르면 이 두 함수만 고치면 됨.

// TCP 클라이언트 초기화 (동기). 접속 후 LISTEN 을 받고 CONNECT 로 답함
ST_Socket __cdecl f_SocketInitTCP_IPv4Tx_Sync(const INT8 *cpIpAddr, const UINT16 usPortNum,
                                              const INT64 lTimeOut_s, const INT64 lTimeOut_us)
{
    ST_Socket st_Socket;
    UINT16    usSync = 0U;

    st_Socket = f_SocketInitTCP_IPv4Tx_Normal(cpIpAddr, usPortNum, lTimeOut_s, lTimeOut_us);
    if (st_Socket.iSockStatus != enum_Socket_Status_Connected)
    {
        return st_Socket;
    }

    if (f_SocketRecvTCP_IPv4(&st_Socket, &usSync, (INT64)sizeof(usSync)) != (INT64)sizeof(usSync))
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] sync failed (listen)");
        (VOID)f_SocketClose(st_Socket);
        return f_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Tx);
    }

    if (usSync != (UINT16)SOCKET_SYNC_PASS_LISTEN)
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] sync bad value 0x%04X", usSync);
        (VOID)f_SocketClose(st_Socket);
        return f_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Tx);
    }

    usSync = (UINT16)SOCKET_SYNC_PASS_CONNECT;
    (VOID)f_SocketSendTCP_IPv4(&st_Socket, &usSync, (INT64)sizeof(usSync));

    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] sync ok");

    return st_Socket;
}

// TCP 서버 초기화 (동기). LISTEN 을 보내고 CONNECT 를 기다림. 동기화 구간에만 iMaxSyncTime_s 적용함
ST_Socket __cdecl f_SocketInitTCP_IPv4Rx_Sync(const INT8 *cpIpAddr, const UINT16 usPortNum,
                                              const INT64 lTimeOut_s, const INT64 lTimeOut_us,
                                              const INT32 iMaxSyncTime_s)
{
    ST_Socket st_Socket;
    UINT16    usSync;

    st_Socket = f_SocketInitTCP_IPv4Rx_Normal(cpIpAddr, usPortNum, lTimeOut_s, lTimeOut_us);
    if (st_Socket.iSockStatus != enum_Socket_Status_Connected)
    {
        return st_Socket;
    }

    f_ApplyTimeout((SOCKET)st_Socket.hSockId, (INT64)iMaxSyncTime_s, 0);

    usSync = (UINT16)SOCKET_SYNC_PASS_LISTEN;
    if (f_SocketSendTCP_IPv4(&st_Socket, &usSync, (INT64)sizeof(usSync)) != (INT64)sizeof(usSync))
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] sync failed (listen)");
        (VOID)f_SocketClose(st_Socket);
        return f_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Rx);
    }

    usSync = 0U;
    if (f_SocketRecvTCP_IPv4(&st_Socket, &usSync, (INT64)sizeof(usSync)) != (INT64)sizeof(usSync))
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] sync timeout (%ds)", iMaxSyncTime_s);
        (VOID)f_SocketClose(st_Socket);
        return f_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Rx);
    }

    if (usSync != (UINT16)SOCKET_SYNC_PASS_CONNECT)
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] sync bad value 0x%04X", usSync);
        (VOID)f_SocketClose(st_Socket);
        return f_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Rx);
    }

    f_ApplyTimeout((SOCKET)st_Socket.hSockId, lTimeOut_s, lTimeOut_us);

    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] sync ok");

    return st_Socket;
}

//
// @brief	정해진 크기를 다 보낼 때까지 send 반복함
// @param	stpSocket		송신 소켓 (끊김 감지 시 상태 갱신)
// @param	vpDataAddr		송신 데이터 주소
// @param	lFixedDataSize	전체 송신 크기 (byte)
// @return	>0 처리 바이트 / 0 타임아웃(한 바이트도 못 보냄) / -1 에러 또는 상대 종료
//
INT64 __cdecl f_SocketSendTCP_IPv4(ST_Socket *stpSocket, const VOID *vpDataAddr,
                                   const INT64 lFixedDataSize)
{
    const CHAR *cpCursor;
    INT64       lRemain;
    INT64       lTotalSent = 0;

    if ((stpSocket == NULL) || (vpDataAddr == NULL) || (lFixedDataSize <= 0))
    {
        return -1;
    }
    if ((stpSocket->hSockId == SOCKET_INVALID_HANDLE) || (stpSocket->iSockStatus != enum_Socket_Status_Connected))
    {
        return -1;
    }

    cpCursor = (const CHAR *)vpDataAddr;
    lRemain  = lFixedDataSize;

    while (lRemain > 0)
    {
        INT32 iChunk = (lRemain > (INT64)INT_MAX) ? INT_MAX : (INT32)lRemain;
        INT32 iSent  = send((SOCKET)stpSocket->hSockId, cpCursor, iChunk, 0);

        if (iSent == SOCKET_ERROR)
        {
            if (WSAGetLastError() == WSAETIMEDOUT)
            {
                if (lTotalSent == 0)
                {
                    return 0;
                }
                continue;                           // 프레임 중간이면 계속 보냄
            }

            f_LogSocketError("TCP send()");
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
// @brief	정해진 크기를 다 받을 때까지 recv 반복함
// @param	stpSocket		수신 소켓 (끊김 감지 시 상태 갱신)
// @param	vpDataAddr		수신 버퍼 주소
// @param	lFixedDataSize	전체 수신 크기 (byte)
// @return	>0 처리 바이트 / 0 타임아웃(한 바이트도 못 받음) / -1 에러 또는 상대 종료
//
INT64 __cdecl f_SocketRecvTCP_IPv4(ST_Socket *stpSocket, const VOID *vpDataAddr,
                                   const INT64 lFixedDataSize)
{
    CHAR *cpCursor;
    INT64 lRemain;
    INT64 lTotalRecv = 0;

    if ((stpSocket == NULL) || (vpDataAddr == NULL) || (lFixedDataSize <= 0))
    {
        return -1;
    }
    if ((stpSocket->hSockId == SOCKET_INVALID_HANDLE) || (stpSocket->iSockStatus != enum_Socket_Status_Connected))
    {
        return -1;
    }

    cpCursor = (CHAR *)vpDataAddr;
    lRemain  = lFixedDataSize;

    while (lRemain > 0)
    {
        INT32 iChunk = (lRemain > (INT64)INT_MAX) ? INT_MAX : (INT32)lRemain;
        INT32 iRecv  = recv((SOCKET)stpSocket->hSockId, cpCursor, iChunk, 0);

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
                continue;                           // 프레임 중간이면 계속 기다림
            }

            f_LogSocketError("TCP recv()");
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

// 소켓 닫기. listen 소켓까지 같이 닫음
INT32 __cdecl f_SocketClose(const ST_Socket st_Socket)
{
    if (st_Socket.hSockId != SOCKET_INVALID_HANDLE)
    {
        (VOID)shutdown((SOCKET)st_Socket.hSockId, SD_BOTH);
        (VOID)closesocket((SOCKET)st_Socket.hSockId);
    }

    if (st_Socket.hListenId != SOCKET_INVALID_HANDLE)
    {
        (VOID)closesocket((SOCKET)st_Socket.hListenId);
    }

    return SOCKET_PASS;
}

// 데모는 int 하나를 헤더 없이 그대로 주고받음.
// x64 는 Windows/Linux 둘 다 little-endian 이라 그냥 보내도 값이 맞음.
static INT32 f_IsDemoStopRequested(const UINT32 uiWait_ms)
{
    if (s_hDemoStop == NULL)
    {
        return 1;
    }

    return (WaitForSingleObject(s_hDemoStop, (DWORD)uiWait_ms) == WAIT_OBJECT_0) ? 1 : 0;
}

// 데모 송신(클라이언트) 쓰레드. 서버에 붙어서 1~100 을 0.1 초마다 보냄
static UINT32 __stdcall f_TcpSenderProc(VOID *vpArg)
{
    INT32 iData;
    INT32 nSeq = 0;

    (VOID)vpArg;

    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] connecting %s:%u", s_caDemoIp, s_usDemoPort);

    s_stDemoSocket = f_SocketInitTCP_IPv4Tx_Normal((const INT8 *)s_caDemoIp, s_usDemoPort,
                                                   (INT64)IPC_TCP_DEFAULT_TIMEOUT_S, 0);
    if (s_stDemoSocket.iSockStatus != enum_Socket_Status_Connected)
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] connect failed");
        InterlockedExchange(&s_lDemoRunning, 0);
        return 0U;
    }

    for (iData = IPC_DEMO_FIRST_VALUE; iData <= IPC_DEMO_LAST_VALUE; iData++)
    {
        INT32 iWire = (s_iDemoUseNBO != 0) ? (INT32)htonl((u_long)iData) : iData;
        INT64 lSent;

        nSeq++;

        lSent = f_SocketSendTCP_IPv4(&s_stDemoSocket, &iWire, (INT64)sizeof(iWire));
        if (lSent == (INT64)sizeof(iWire))
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

        if (f_IsDemoStopRequested(IPC_DEMO_INTERVAL_MS) != 0)
        {
            break;
        }
    }

    if (iData > IPC_DEMO_LAST_VALUE)
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] tx done (%d)", nSeq);
    }

    (VOID)f_SocketClose(s_stDemoSocket);
    s_stDemoSocket = f_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Tx);

    InterlockedExchange(&s_lDemoRunning, 0);

    return 0U;
}

// 데모 수신(서버) 쓰레드. 접속 받고 멈추라고 할 때까지 받아서 로그로 찍음
static UINT32 __stdcall f_TcpReceiverProc(VOID *vpArg)
{
    INT32 nRecvCount = 0;

    (VOID)vpArg;

    s_stDemoSocket = f_SocketInitTCP_IPv4Rx_Normal((const INT8 *)s_caDemoIp, s_usDemoPort,
                                                   (INT64)IPC_TCP_DEFAULT_TIMEOUT_S, 0);
    if (s_stDemoSocket.iSockStatus != enum_Socket_Status_Connected)
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] listen aborted");
        InterlockedExchange(&s_lDemoRunning, 0);
        return 0U;
    }

    while (f_IsDemoStopRequested(0U) == 0)
    {
        INT32 iWire = 0;
        INT64 lRecv;

        lRecv = f_SocketRecvTCP_IPv4(&s_stDemoSocket, &iWire, (INT64)sizeof(iWire));
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
            INT32 iData = (s_iDemoUseNBO != 0) ? (INT32)ntohl((u_long)iWire) : iWire;

            nRecvCount++;
            f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] rx %d", iData);
        }
    }

    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] rx done (%d)", nRecvCount);

    (VOID)f_SocketClose(s_stDemoSocket);
    s_stDemoSocket = f_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Rx);

    InterlockedExchange(&s_lDemoRunning, 0);

    return 0U;
}

//
// @brief	TCP 데모 시작. 역할에 맞는 쓰레드 띄움
// @param	iIsSender				1:송신(클라이언트), 0:수신(서버)
// @param	cpIpAddr				상대(또는 bind) IPv4 주소 (비어 있으면 127.0.0.1)
// @param	usPortNum				포트 번호 (0 이면 51000)
// @param	iUseNetworkByteOrder	1:htonl/ntohl 적용
// @return	SOCKET_PASS / SOCKET_FAIL (이미 동작 중 포함)
//
INT32 __cdecl f_IpcTcpDemoStart(const INT32 iIsSender, const INT8 *cpIpAddr,
                                const UINT16 usPortNum, const INT32 iUseNetworkByteOrder)
{
    if (InterlockedCompareExchange(&s_lDemoRunning, 1, 0) != 0)
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] already running");
        return SOCKET_FAIL;
    }

    // 워커가 알아서 끝난 뒤 재시작하는 경우가 있어서 이전 핸들부터 정리함
    if (s_hDemoThread != NULL)
    {
        (VOID)WaitForSingleObject(s_hDemoThread, 1000U);
        (VOID)CloseHandle(s_hDemoThread);
        s_hDemoThread = NULL;
    }
    if (s_hDemoStop != NULL)
    {
        (VOID)CloseHandle(s_hDemoStop);
        s_hDemoStop = NULL;
    }

    if (f_EnsureWinsock() != SOCKET_PASS)
    {
        InterlockedExchange(&s_lDemoRunning, 0);
        return SOCKET_FAIL;
    }

    (VOID)memset(s_caDemoIp, 0, sizeof(s_caDemoIp));
    if ((cpIpAddr != NULL) && (cpIpAddr[0] != '\0'))
    {
        (VOID)strncpy_s(s_caDemoIp, sizeof(s_caDemoIp), (const CHAR *)cpIpAddr, _TRUNCATE);
    }
    else
    {
        (VOID)strncpy_s(s_caDemoIp, sizeof(s_caDemoIp), "127.0.0.1", _TRUNCATE);
    }

    s_usDemoPort    = (usPortNum != 0U) ? usPortNum : 51000U;
    s_iDemoIsSender = (iIsSender != 0) ? 1 : 0;
    s_iDemoUseNBO   = (iUseNetworkByteOrder != 0) ? 1 : 0;
    s_stDemoSocket  = f_MakeInvalidSocket(enum_Socket_Type_NotDefined);

    f_SocketResetCancel();

    s_hDemoStop = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (s_hDemoStop == NULL)
    {
        InterlockedExchange(&s_lDemoRunning, 0);
        return SOCKET_FAIL;
    }

    s_hDemoThread = (HANDLE)_beginthreadex(NULL, 0,
                                           (s_iDemoIsSender != 0) ? f_TcpSenderProc : f_TcpReceiverProc,
                                           NULL, 0, NULL);
    if (s_hDemoThread == NULL)
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] thread create failed");
        (VOID)f_IpcTcpDemoStop();
        return SOCKET_FAIL;
    }

    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] start (%s)", (s_iDemoIsSender != 0) ? "tx" : "rx");

    return SOCKET_PASS;
}

// 데모 정지. 이벤트/취소 올리고 쓰레드 끝날 때까지 기다림
INT32 __cdecl f_IpcTcpDemoStop(VOID)
{
    if (s_hDemoStop != NULL)
    {
        (VOID)SetEvent(s_hDemoStop);
    }

    f_SocketCancelBlockingCalls();

    if (s_hDemoThread != NULL)
    {
        (VOID)WaitForSingleObject(s_hDemoThread, 5000U);
        (VOID)CloseHandle(s_hDemoThread);
        s_hDemoThread = NULL;
    }

    if (s_hDemoStop != NULL)
    {
        (VOID)CloseHandle(s_hDemoStop);
        s_hDemoStop = NULL;
    }

    if (InterlockedExchange(&s_lDemoRunning, 0) == 1)
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] stop");
    }

    f_SocketResetCancel();

    return SOCKET_PASS;
}

INT32 __cdecl f_IpcTcpDemoIsRunning(VOID)
{
    return (INT32)InterlockedCompareExchange(&s_lDemoRunning, 0, 0);
}
