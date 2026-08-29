//
// @file	IpcSocket.c
// @brief	TCP/IP 송/수신 (SocketUtility.h/.c v5.0 의 Windows 포팅).
//			흐름과 반환 규약은 원본 .c 를 그대로 따름. RDMA 쪽은 제외.
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

// 헤더에 따라 안 나오는 SDK 가 있어서 직접 정의
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET               _WSAIOW(IOC_VENDOR, 12)
#endif

#define IPC_TCP_ACCEPT_SLICE_MS         200
#define IPC_TCP_CONNECT_TIMEOUT_MS      1000
#define IPC_TCP_CONNECT_RETRY_GAP_MS    500
#define IPC_TCP_DEFAULT_TIMEOUT_S       1

static INIT_ONCE            s_stWsaOnce = INIT_ONCE_STATIC_INIT;
static LONG                 s_lWsaReady = 0;

// 원본 기본값 그대로 (65507 = UDP 최대, 조각 간격 100us)
static volatile LONG        s_lUDP_PartitionSize_Byte  = 65507;
static volatile LONG        s_lUDP_PartitionSendGap_us = 100;

static volatile LONG        s_lCancelRequested = 0;

static ST_Socket            s_stDemoSocket;
static HANDLE               s_hDemoThread   = NULL;
static HANDLE               s_hDemoStop     = NULL;
static volatile LONG        s_lDemoRunning  = 0;
static INT32                s_iDemoIsSender = 0;
static INT32                s_iDemoUseNBO   = 0;
static CHAR                 s_caDemoIp[64]  = "127.0.0.1";
static UINT16               s_usDemoPort    = 12345U;

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

// Windows 소켓 타임아웃은 timeval 이 아니라 ms 단위 DWORD 라 여기서 변환함.
// 원본처럼 Tx 는 SO_SNDTIMEO, Rx 는 SO_RCVTIMEO 한쪽만 검
static VOID f_SetSockTimeout(const SOCKET hSock, const INT32 iOptName, const INT64 lTimeOut_s, const INT64 lTimeOut_us)
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

    (VOID)setsockopt(hSock, SOL_SOCKET, iOptName, (const CHAR *)&dwTimeOut_ms, (INT32)sizeof(dwTimeOut_ms));
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

// 상대 포트가 아직 없으면 ICMP 때문에 다음 recvfrom 이 10054 로 깨지는 Windows 동작 끔 (Linux 엔 없음)
static VOID f_DisableUdpConnReset(const SOCKET hSock)
{
    BOOL  bNewBehavior = FALSE;
    DWORD dwBytes      = 0;

    (VOID)WSAIoctl(hSock, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior), NULL, 0, &dwBytes, NULL, NULL);
}

// listen 소켓에서 접속 하나 받음. select 조각 대기라 취소에 반응함. 실패/취소면 INVALID_SOCKET
static SOCKET f_AcceptWithCancel(const SOCKET hListen, struct sockaddr_in *stpPeerAddr)
{
    INT32 iPeerAddrSize = (INT32)sizeof(*stpPeerAddr);

    for (;;)
    {
        fd_set         st_ReadSet;
        struct timeval st_TimeOut;
        INT32          iSelect;

        if (f_IsCancelRequested() != 0)
        {
            f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] accept canceled");
            return INVALID_SOCKET;
        }

        FD_ZERO(&st_ReadSet);
        FD_SET(hListen, &st_ReadSet);
        st_TimeOut.tv_sec  = 0;
        st_TimeOut.tv_usec = (LONG)(IPC_TCP_ACCEPT_SLICE_MS * 1000);

        iSelect = select(0, &st_ReadSet, NULL, NULL, &st_TimeOut);
        if (iSelect == SOCKET_ERROR)
        {
            f_LogSocketError("TCP Rx select()");
            return INVALID_SOCKET;
        }
        if (iSelect == 0)
        {
            continue;
        }

        return accept(hListen, (struct sockaddr *)stpPeerAddr, &iPeerAddrSize);
    }
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

// 원본은 pthread mutex 로 막는데 여기선 Interlocked 로 대신함
INT32 __cdecl f_SocketSetUDP_PartitionSize(const INT32 iUDP_PartitionSize_Byte)
{
    if ((iUDP_PartitionSize_Byte < 1) || (iUDP_PartitionSize_Byte > 65507))
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[UDP] partition size must be 1..65507");
        return SOCKET_FAIL;
    }

    InterlockedExchange(&s_lUDP_PartitionSize_Byte, (LONG)iUDP_PartitionSize_Byte);

    return SOCKET_PASS;
}

VOID __cdecl f_SocketGetUDP_PartitionSize(INT32 *ipUDP_PartitionSize_Byte)
{
    if (ipUDP_PartitionSize_Byte != NULL)
    {
        *ipUDP_PartitionSize_Byte = (INT32)InterlockedCompareExchange(&s_lUDP_PartitionSize_Byte, 0, 0);
    }
}

INT32 __cdecl f_SocketSetUDP_PartitionSendGap(const UINT32 uiUDP_PartitionSendGap_us)
{
    if (uiUDP_PartitionSendGap_us < 1U)
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[UDP] partition send gap must be >= 1");
        return SOCKET_FAIL;
    }

    InterlockedExchange(&s_lUDP_PartitionSendGap_us, (LONG)uiUDP_PartitionSendGap_us);

    return SOCKET_PASS;
}

VOID __cdecl f_SocketGetUDP_PartitionSendGap(UINT32 *uipUDP_PartitionSendGap_us)
{
    if (uipUDP_PartitionSendGap_us != NULL)
    {
        *uipUDP_PartitionSendGap_us = (UINT32)InterlockedCompareExchange(&s_lUDP_PartitionSendGap_us, 0, 0);
    }
}

// ---- UDP ----

// 수신하면서 송신측 주소를 stSockAddr 에 받아둠. Rx Sync 라인 체크에서 상대 주소를 얻는 용도
static INT64 f_SocketRecvUDP_IPv4_Address(ST_Socket *stpSocket, VOID *vpDataAddr, const INT64 lMaxSize)
{
    INT32 iAddrSize = (INT32)sizeof(stpSocket->stSockAddr);
    INT32 iRecv;

    iRecv = recvfrom((SOCKET)stpSocket->hSockId, (CHAR *)vpDataAddr, (INT32)lMaxSize, 0,
                     (struct sockaddr *)&stpSocket->stSockAddr, &iAddrSize);
    if (iRecv == SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSAETIMEDOUT)
        {
            f_LogSocketError("UDP recvfrom(address)");
        }
        return -1;
    }

    stpSocket->uiSockAddrSize = (UINT32)iAddrSize;

    return (INT64)iRecv;
}

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

    f_DisableUdpConnReset(hSock);

    st_Socket.hSockId     = (SOCKET_HANDLE)hSock;
    st_Socket.iSockStatus = enum_Socket_Status_NotDefined;

    return st_Socket;
}

// UDP 수신 소켓. 자기 주소에 bind 하고 수신 타임아웃만 검
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

    f_DisableUdpConnReset(hSock);
    f_SetSockTimeout(hSock, SO_RCVTIMEO, lTimeOut_s, lTimeOut_us);

    if (bind(hSock, (const struct sockaddr *)&st_Socket.stSockAddr, (INT32)sizeof(st_Socket.stSockAddr)) == SOCKET_ERROR)
    {
        f_LogSocketError("UDP Rx bind()");
        (VOID)closesocket(hSock);
        return st_Socket;
    }

    st_Socket.hSockId              = (SOCKET_HANDLE)hSock;
    st_Socket.iSockStatus          = enum_Socket_Status_NotDefined;
    st_Socket.stTimeOutVal.tv_sec  = (LONG)lTimeOut_s;
    st_Socket.stTimeOutVal.tv_usec = (LONG)lTimeOut_us;

    return st_Socket;
}

// UDP 송신. sendto 결과 그대로 리턴 (-1 에러)
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

#if(DISP_SOCKET_ERROR_WARNING == 1)
    if (lDataSize > 65507)
    {
        f_IpcLog(enum_IpcLogCh_Tcp, "[UDP] %lld byte is over UDP max", (long long)lDataSize);
    }
#endif

    iSent = sendto((SOCKET)stpSocket->hSockId, (const CHAR *)vpDataAddr, (INT32)lDataSize, 0,
                   (const struct sockaddr *)&stpSocket->stSockAddr, (INT32)sizeof(stpSocket->stSockAddr));
    if (iSent == SOCKET_ERROR)
    {
        f_LogSocketError("UDP sendto()");
        return -1;
    }

    return (INT64)iSent;
}

// UDP 분할 송신. 설정한 크기로 잘라 보내고 조각마다 gap 만큼 쉼.
// 에러가 나도 원본처럼 여기까지 보낸 크기를 리턴함
INT64 __cdecl f_SocketSendUDP_IPv4_Partition(const ST_Socket *stpSocket, const VOID *vpDataAddr,
                                             const INT64 lFixedDataSize)
{
    const CHAR *cpCursor;
    INT64       lRemain;
    INT64       lTotalSent = 0;
    INT32       iPartSize;
    UINT32      uiGap_us;

    if ((stpSocket == NULL) || (vpDataAddr == NULL) || (lFixedDataSize <= 0))
    {
        return -1;
    }

    iPartSize = (INT32)InterlockedCompareExchange(&s_lUDP_PartitionSize_Byte, 0, 0);
    uiGap_us  = (UINT32)InterlockedCompareExchange(&s_lUDP_PartitionSendGap_us, 0, 0);

    cpCursor = (const CHAR *)vpDataAddr;
    lRemain  = lFixedDataSize;

    while (lRemain > 0)
    {
        INT64 lChunk = (lRemain > (INT64)iPartSize) ? (INT64)iPartSize : lRemain;
        INT32 iSent  = sendto((SOCKET)stpSocket->hSockId, cpCursor, (INT32)lChunk, 0,
                              (const struct sockaddr *)&stpSocket->stSockAddr, (INT32)sizeof(stpSocket->stSockAddr));
        if (iSent == SOCKET_ERROR)
        {
            f_LogSocketError("UDP sendto(partition)");
            break;
        }

        cpCursor   += iSent;
        lTotalSent += iSent;
        lRemain     = lFixedDataSize - lTotalSent;

        f_SleepMicroseconds(uiGap_us);
    }

    return lTotalSent;
}

// UDP 수신. recvfrom 결과 그대로 리턴 (-1 에러, 타임아웃 포함)
INT64 __cdecl f_SocketRecvUDP_IPv4_Normal(const ST_Socket *stpSocket, const VOID *vpDataAddr,
                                          const INT64 lMaxSize)
{
    INT32 iRecv;

    if ((stpSocket == NULL) || (vpDataAddr == NULL) || (lMaxSize <= 0))
    {
        return -1;
    }

    // 주소가 안 바뀌게 원본은 버림용 구조체를 쓰는데 여기선 NULL 로 안 받음
    iRecv = recvfrom((SOCKET)stpSocket->hSockId, (CHAR *)vpDataAddr, (INT32)lMaxSize, 0, NULL, NULL);
    if (iRecv == SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSAETIMEDOUT)
        {
            f_LogSocketError("UDP recvfrom()");
        }
        return -1;
    }

    return (INT64)iRecv;
}

// UDP 분할 수신. 에러/타임아웃이 나도 여기까지 받은 크기를 리턴함
INT64 __cdecl f_SocketRecvUDP_IPv4_Partition(const ST_Socket *stpSocket, const VOID *vpDataAddr,
                                             const INT64 lFixedDataSize)
{
    CHAR *cpCursor;
    INT64 lRemain;
    INT64 lTotalRecv = 0;
    INT32 iPartSize;

    if ((stpSocket == NULL) || (vpDataAddr == NULL) || (lFixedDataSize <= 0))
    {
        return -1;
    }

    iPartSize = (INT32)InterlockedCompareExchange(&s_lUDP_PartitionSize_Byte, 0, 0);

    cpCursor = (CHAR *)vpDataAddr;
    lRemain  = lFixedDataSize;

    while (lRemain > 0)
    {
        INT64 lChunk = (lRemain > (INT64)iPartSize) ? (INT64)iPartSize : lRemain;
        INT32 iRecv  = recvfrom((SOCKET)stpSocket->hSockId, cpCursor, (INT32)lChunk, 0, NULL, NULL);
        if (iRecv == SOCKET_ERROR)
        {
            if (WSAGetLastError() != WSAETIMEDOUT)
            {
                f_LogSocketError("UDP recvfrom(partition)");
            }
            break;
        }

        cpCursor   += iRecv;
        lTotalRecv += iRecv;
        lRemain     = lFixedDataSize - lTotalRecv;
    }

    return lTotalRecv;
}

// ---- TCP ----

//
// @brief	TCP 클라이언트(Tx) 초기화. 연결될 때까지 재시도함.
//			Windows 는 실패한 소켓에 connect 를 다시 못 걸어서 소켓을 새로 만들고,
//			블로킹 connect 는 실패 확정까지 20초 이상 걸려서 논블로킹 + select 로 함
// @param	cpIpAddr	서버 IPv4 주소
// @param	usPortNum	서버 포트 번호
// @param	lTimeOut_s	송신 타임아웃 (초)
// @param	lTimeOut_us	송신 타임아웃 (마이크로초)
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

    f_SetSockTimeout(hSock, SO_SNDTIMEO, lTimeOut_s, lTimeOut_us);

    st_Socket.hSockId              = (SOCKET_HANDLE)hSock;
    st_Socket.stSockAddr           = st_Addr;
    st_Socket.iSockStatus          = enum_Socket_Status_Connected;
    st_Socket.stTimeOutVal.tv_sec  = (LONG)lTimeOut_s;
    st_Socket.stTimeOutVal.tv_usec = (LONG)lTimeOut_us;

    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] connected %s:%u", (const CHAR *)cpIpAddr, usPortNum);

    return st_Socket;
}

//
// @brief	TCP 서버(Rx) 초기화. bind/listen 후 접속 하나 받고 listen 소켓은 닫음 (원본과 동일, 1:1).
//			구조체에는 자기 bind 주소를 남겨둠 (끊겼을 때 다시 listen 하는 데 씀)
// @param	cpIpAddr	bind 할 IPv4 주소 (NULL/빈 문자열이면 INADDR_ANY)
// @param	usPortNum	bind 할 포트 번호
// @param	lTimeOut_s	수신 타임아웃 (초)
// @param	lTimeOut_us	수신 타임아웃 (마이크로초)
// @return	소켓 구조체 (iSockStatus 로 성공 여부 확인)
//
ST_Socket __cdecl f_SocketInitTCP_IPv4Rx_Normal(const INT8 *cpIpAddr, const UINT16 usPortNum,
                                                const INT64 lTimeOut_s, const INT64 lTimeOut_us)
{
    ST_Socket          st_Socket = f_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Rx);
    struct sockaddr_in st_Addr;
    struct sockaddr_in st_PeerAddr;
    SOCKET             hListen;
    SOCKET             hAccept;
    INT32              iReuse = 1;
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

    // 재기동하면 TIME_WAIT 때문에 bind 가 막혀서 넣음 (원본과 동일)
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

    hAccept = f_AcceptWithCancel(hListen, &st_PeerAddr);
    if (hAccept == INVALID_SOCKET)
    {
        (VOID)closesocket(hListen);
        return st_Socket;
    }

    f_SetSockTimeout(hAccept, SO_RCVTIMEO, lTimeOut_s, lTimeOut_us);

    // 접속 하나 받으면 listen 소켓은 바로 닫음
    (VOID)closesocket(hListen);

    (VOID)memset(caPeerIp, 0, sizeof(caPeerIp));
    (VOID)inet_ntop(AF_INET, &st_PeerAddr.sin_addr, caPeerIp, sizeof(caPeerIp));

    st_Socket.hSockId              = (SOCKET_HANDLE)hAccept;
    st_Socket.stSockAddr           = st_Addr;
    st_Socket.iSockStatus          = enum_Socket_Status_Connected;
    st_Socket.stTimeOutVal.tv_sec  = (LONG)lTimeOut_s;
    st_Socket.stTimeOutVal.tv_usec = (LONG)lTimeOut_us;

    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] accept %s:%u", caPeerIp, ntohs(st_PeerAddr.sin_port));

    return st_Socket;
}

// Sync 는 원본과 같은 절차임. TCP 를 붙이기 전에 같은 포트의 UDP 로 신호를 주고받음.
//   Tx : TX_LINE_CHECK 반복 송신 -> RX_LINE_CHECK 수신 -> LISTEN 수신 -> connect -> CONNECT 송신
//        -> TCP 로 SEND_TO_TX 수신 -> SEND_TO_RX 송신
//   Rx : TX_LINE_CHECK 수신(여기서 상대 주소 획득, iMaxSyncTime_s 한도) -> RX_LINE_CHECK 송신
//        -> listen -> LISTEN 송신 -> CONNECT 수신 -> accept -> TCP 로 SEND_TO_TX 송신 -> SEND_TO_RX 수신

//
// @brief	TCP 클라이언트 초기화 (동기). UDP 로 서로 확인한 뒤 접속함
// @param	cpIpAddr	서버 IPv4 주소
// @param	usPortNum	서버 포트
// @param	lTimeOut_s	송신 타임아웃 (초)
// @param	lTimeOut_us	송신 타임아웃 (마이크로초)
// @return	소켓 구조체
//
ST_Socket __cdecl f_SocketInitTCP_IPv4Tx_Sync(const INT8 *cpIpAddr, const UINT16 usPortNum,
                                              const INT64 lTimeOut_s, const INT64 lTimeOut_us)
{
    ST_Socket st_SocketTcp = f_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Tx);
    ST_Socket st_SocketUdp;
    INT32     iSync = 0;
    DWORD     dwStep_ms = 500U;

    st_SocketUdp = f_SocketInitUDP_IPv4Tx(cpIpAddr, usPortNum);
    if (st_SocketUdp.hSockId == SOCKET_INVALID_HANDLE)
    {
        return st_SocketTcp;
    }
    (VOID)setsockopt((SOCKET)st_SocketUdp.hSockId, SOL_SOCKET, SO_RCVTIMEO,
                     (const CHAR *)&dwStep_ms, (INT32)sizeof(dwStep_ms));

    // 상대가 응답할 때까지 TX_LINE_CHECK 를 보냄
    while (iSync != SOCKET_SYNC_PASS_RX_LINE_CHECK)
    {
        INT32 iPing = SOCKET_SYNC_PASS_TX_LINE_CHECK;

        if (f_IsCancelRequested() != 0)
        {
            (VOID)f_SocketClose(st_SocketUdp);
            return st_SocketTcp;
        }

        (VOID)f_SocketSendUDP_IPv4_Normal(&st_SocketUdp, &iPing, (INT64)sizeof(iPing));
        (VOID)f_SocketRecvUDP_IPv4_Normal(&st_SocketUdp, &iSync, (INT64)sizeof(iSync));
    }

    // 상대가 listen 들어갔다는 신호 대기
    while (iSync != SOCKET_SYNC_PASS_LISTEN)
    {
        if (f_IsCancelRequested() != 0)
        {
            (VOID)f_SocketClose(st_SocketUdp);
            return st_SocketTcp;
        }

        (VOID)f_SocketRecvUDP_IPv4_Normal(&st_SocketUdp, &iSync, (INT64)sizeof(iSync));
    }

    // 접속하고 CONNECT 신호를 UDP 로 알림
    st_SocketTcp = f_SocketInitTCP_IPv4Tx_Normal(cpIpAddr, usPortNum, lTimeOut_s, lTimeOut_us);
    if (st_SocketTcp.iSockStatus != enum_Socket_Status_Connected)
    {
        (VOID)f_SocketClose(st_SocketUdp);
        return st_SocketTcp;
    }

    iSync = SOCKET_SYNC_PASS_CONNECT;
    (VOID)f_SocketSendUDP_IPv4_Normal(&st_SocketUdp, &iSync, (INT64)sizeof(iSync));

    // TCP 로 SEND_TO_TX 받고 SEND_TO_RX 로 답함
    iSync = 0;
    while (iSync != SOCKET_SYNC_PASS_SEND_TO_TX)
    {
        if (f_SocketRecvTCP_IPv4(&st_SocketTcp, &iSync, (INT64)sizeof(iSync)) != (INT64)sizeof(iSync))
        {
            f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] sync failed (tx)");
            (VOID)f_SocketClose(st_SocketUdp);
            (VOID)f_SocketClose(st_SocketTcp);
            return f_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Tx);
        }
    }

    iSync = SOCKET_SYNC_PASS_SEND_TO_RX;
    (VOID)f_SocketSendTCP_IPv4(&st_SocketTcp, &iSync, (INT64)sizeof(iSync));

    (VOID)f_SocketClose(st_SocketUdp);

    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] sync ok (tx)");

    return st_SocketTcp;
}

//
// @brief	TCP 서버 초기화 (동기). UDP 라인 체크는 iMaxSyncTime_s 까지만 기다림 (0 이면 무한)
// @param	cpIpAddr	bind 할 IPv4 주소
// @param	usPortNum	bind 할 포트
// @param	lTimeOut_s	수신 타임아웃 (초)
// @param	lTimeOut_us	수신 타임아웃 (마이크로초)
// @param	iMaxSyncTime_s	라인 체크 대기 한도 (초)
// @return	소켓 구조체
//
ST_Socket __cdecl f_SocketInitTCP_IPv4Rx_Sync(const INT8 *cpIpAddr, const UINT16 usPortNum,
                                              const INT64 lTimeOut_s, const INT64 lTimeOut_us,
                                              const INT32 iMaxSyncTime_s)
{
    ST_Socket          st_SocketTcp = f_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Rx);
    ST_Socket          st_SocketUdp;
    struct sockaddr_in st_Addr;
    struct sockaddr_in st_PeerAddr;
    SOCKET             hListen;
    SOCKET             hAccept;
    INT32              iReuse = 1;
    INT32              iSync = 0;
    INT32              nRunTime_s = 0;
    INT32              iMaxRunTime_s = (iMaxSyncTime_s == 0) ? INT_MAX : iMaxSyncTime_s;

    // UDP 라인 체크. 1 초 타임아웃으로 돌면서 경과 시간을 셈
    st_SocketUdp = f_SocketInitUDP_IPv4Rx(cpIpAddr, usPortNum, 1, 0);
    if (st_SocketUdp.hSockId == SOCKET_INVALID_HANDLE)
    {
        return st_SocketTcp;
    }

    while (iSync != SOCKET_SYNC_PASS_TX_LINE_CHECK)
    {
        if (f_IsCancelRequested() != 0)
        {
            (VOID)f_SocketClose(st_SocketUdp);
            return st_SocketTcp;
        }

        // 여기서 송신측 주소를 같이 받아둠 (응답 보낼 곳)
        if (f_SocketRecvUDP_IPv4_Address(&st_SocketUdp, &iSync, (INT64)sizeof(iSync)) <= 0)
        {
            nRunTime_s++;
            if (nRunTime_s >= iMaxRunTime_s)
            {
                f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] sync timeout (%ds)", iMaxSyncTime_s);
                (VOID)f_SocketClose(st_SocketUdp);
                return st_SocketTcp;
            }
        }
    }

    iSync = SOCKET_SYNC_PASS_RX_LINE_CHECK;
    (VOID)f_SocketSendUDP_IPv4_Normal(&st_SocketUdp, &iSync, (INT64)sizeof(iSync));

    // listen 준비하고 LISTEN 신호를 UDP 로 알림
    if (f_FillSockAddr(&st_Addr, cpIpAddr, usPortNum) != SOCKET_PASS)
    {
        (VOID)f_SocketClose(st_SocketUdp);
        return st_SocketTcp;
    }

    hListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (hListen == INVALID_SOCKET)
    {
        f_LogSocketError("TCP Rx socket()");
        (VOID)f_SocketClose(st_SocketUdp);
        return st_SocketTcp;
    }

    (VOID)setsockopt(hListen, SOL_SOCKET, SO_REUSEADDR, (const CHAR *)&iReuse, (INT32)sizeof(iReuse));

    if ((bind(hListen, (const struct sockaddr *)&st_Addr, (INT32)sizeof(st_Addr)) == SOCKET_ERROR) ||
        (listen(hListen, SOCKET_TCP_QUEUE_SIZE) == SOCKET_ERROR))
    {
        f_LogSocketError("TCP Rx bind/listen()");
        (VOID)closesocket(hListen);
        (VOID)f_SocketClose(st_SocketUdp);
        return st_SocketTcp;
    }

    iSync = SOCKET_SYNC_PASS_LISTEN;
    (VOID)f_SocketSendUDP_IPv4_Normal(&st_SocketUdp, &iSync, (INT64)sizeof(iSync));

    // CONNECT 신호 받고 accept
    iSync = 0;
    while (iSync != SOCKET_SYNC_PASS_CONNECT)
    {
        if (f_IsCancelRequested() != 0)
        {
            (VOID)closesocket(hListen);
            (VOID)f_SocketClose(st_SocketUdp);
            return st_SocketTcp;
        }

        (VOID)f_SocketRecvUDP_IPv4_Normal(&st_SocketUdp, &iSync, (INT64)sizeof(iSync));
    }

    hAccept = f_AcceptWithCancel(hListen, &st_PeerAddr);
    if (hAccept == INVALID_SOCKET)
    {
        (VOID)closesocket(hListen);
        (VOID)f_SocketClose(st_SocketUdp);
        return st_SocketTcp;
    }

    f_SetSockTimeout(hAccept, SO_RCVTIMEO, lTimeOut_s, lTimeOut_us);
    (VOID)closesocket(hListen);

    st_SocketTcp.hSockId              = (SOCKET_HANDLE)hAccept;
    st_SocketTcp.stSockAddr           = st_Addr;
    st_SocketTcp.iSockStatus          = enum_Socket_Status_Connected;
    st_SocketTcp.stTimeOutVal.tv_sec  = (LONG)lTimeOut_s;
    st_SocketTcp.stTimeOutVal.tv_usec = (LONG)lTimeOut_us;

    // TCP 로 SEND_TO_TX 보내고 SEND_TO_RX 대기
    iSync = SOCKET_SYNC_PASS_SEND_TO_TX;
    (VOID)f_SocketSendTCP_IPv4(&st_SocketTcp, &iSync, (INT64)sizeof(iSync));

    iSync = 0;
    while (iSync != SOCKET_SYNC_PASS_SEND_TO_RX)
    {
        if (f_SocketRecvTCP_IPv4(&st_SocketTcp, &iSync, (INT64)sizeof(iSync)) != (INT64)sizeof(iSync))
        {
            f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] sync failed (rx)");
            (VOID)f_SocketClose(st_SocketUdp);
            (VOID)f_SocketClose(st_SocketTcp);
            return f_MakeInvalidSocket(enum_Socket_Type_TCP_IPv4Rx);
        }
    }

    (VOID)f_SocketClose(st_SocketUdp);

    f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] sync ok (rx)");

    return st_SocketTcp;
}

//
// @brief	정해진 크기를 다 보낼 때까지 send 반복함. 에러가 나도 여기까지 보낸 크기를 리턴함.
//			끊겨 있으면 접속될 때까지 닫고 다시 붙음 (원본 LINK_RECOVERY 동작, 취소 가능)
// @param	stpSocket		송신 소켓
// @param	vpDataAddr		송신 데이터 주소
// @param	lFixedDataSize	전체 송신 크기 (byte)
// @return	보낸 바이트 수 / -1 인자 오류
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

#if(CTRL_SOCKET_LINK_RECOVERY == 1)
    while (stpSocket->iSockStatus != enum_Socket_Status_Connected)
    {
        CHAR caIp[INET_ADDRSTRLEN];

        if (f_IsCancelRequested() != 0)
        {
            return lTotalSent;
        }

        Sleep(100U);

        if (stpSocket->hSockId != SOCKET_INVALID_HANDLE)
        {
            (VOID)closesocket((SOCKET)stpSocket->hSockId);
        }

        (VOID)memset(caIp, 0, sizeof(caIp));
        (VOID)inet_ntop(AF_INET, &stpSocket->stSockAddr.sin_addr, caIp, sizeof(caIp));

        *stpSocket = f_SocketInitTCP_IPv4Tx_Normal((const INT8 *)caIp, ntohs(stpSocket->stSockAddr.sin_port),
                                                   (INT64)stpSocket->stTimeOutVal.tv_sec,
                                                   (INT64)stpSocket->stTimeOutVal.tv_usec);
    }
#endif

    cpCursor = (const CHAR *)vpDataAddr;
    lRemain  = lFixedDataSize;

    while ((lRemain > 0) && (stpSocket->iSockStatus == enum_Socket_Status_Connected))
    {
        INT32 iChunk = (lRemain > (INT64)INT_MAX) ? INT_MAX : (INT32)lRemain;
        INT32 iSent  = send((SOCKET)stpSocket->hSockId, cpCursor, iChunk, 0);

        if (iSent != SOCKET_ERROR)
        {
            cpCursor   += iSent;
            lTotalSent += iSent;
            lRemain     = lFixedDataSize - lTotalSent;
        }
        else
        {
            // 타임아웃도 -1 이라 원본처럼 끊긴 걸로 침
            if (WSAGetLastError() != WSAETIMEDOUT)
            {
                f_LogSocketError("TCP send()");
            }
            stpSocket->iSockStatus = enum_Socket_Status_Disconnected;
        }
    }

    return lTotalSent;
}

//
// @brief	정해진 크기를 다 받을 때까지 recv 반복함. 에러가 나도 여기까지 받은 크기를 리턴함.
//			끊겨 있으면 닫고 다시 listen/accept 함 (원본 LINK_RECOVERY 동작, 취소 가능)
// @param	stpSocket		수신 소켓
// @param	vpDataAddr		수신 버퍼 주소
// @param	lFixedDataSize	전체 수신 크기 (byte)
// @return	받은 바이트 수 / -1 인자 오류
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

#if(CTRL_SOCKET_LINK_RECOVERY == 1)
    if (stpSocket->iSockStatus != enum_Socket_Status_Connected)
    {
        CHAR caIp[INET_ADDRSTRLEN];

        if (f_IsCancelRequested() != 0)
        {
            return 0;
        }

        if (stpSocket->hSockId != SOCKET_INVALID_HANDLE)
        {
            (VOID)closesocket((SOCKET)stpSocket->hSockId);
        }

        // Rx 구조체에는 bind 주소가 들어 있어서 그걸로 다시 listen 함
        (VOID)memset(caIp, 0, sizeof(caIp));
        (VOID)inet_ntop(AF_INET, &stpSocket->stSockAddr.sin_addr, caIp, sizeof(caIp));

        *stpSocket = f_SocketInitTCP_IPv4Rx_Normal((const INT8 *)caIp, ntohs(stpSocket->stSockAddr.sin_port),
                                                   (INT64)stpSocket->stTimeOutVal.tv_sec,
                                                   (INT64)stpSocket->stTimeOutVal.tv_usec);
    }
#endif

    cpCursor = (CHAR *)vpDataAddr;
    lRemain  = lFixedDataSize;

    while ((lRemain > 0) && (stpSocket->iSockStatus == enum_Socket_Status_Connected))
    {
        INT32 iChunk = (lRemain > (INT64)INT_MAX) ? INT_MAX : (INT32)lRemain;
        INT32 iRecv  = recv((SOCKET)stpSocket->hSockId, cpCursor, iChunk, 0);

        if (iRecv > 0)
        {
            cpCursor   += iRecv;
            lTotalRecv += iRecv;
            lRemain     = lFixedDataSize - lTotalRecv;
        }
        else
        {
            // 0 이면 상대 종료, -1 이면 에러. 타임아웃도 -1 이라 원본처럼 끊긴 걸로 침
            if (iRecv == 0)
            {
                f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] peer closed");
            }
            else if (WSAGetLastError() != WSAETIMEDOUT)
            {
                f_LogSocketError("TCP recv()");
            }
            stpSocket->iSockStatus = enum_Socket_Status_Disconnected;
        }
    }

    return lTotalRecv;
}

// 소켓 닫기. 원본처럼 close 성공 여부를 리턴함
INT32 __cdecl f_SocketClose(const ST_Socket st_Socket)
{
    if (st_Socket.hSockId == SOCKET_INVALID_HANDLE)
    {
        return SOCKET_FAIL;
    }

    return (closesocket((SOCKET)st_Socket.hSockId) == 0) ? SOCKET_PASS : SOCKET_FAIL;
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
        if (lRecv == (INT64)sizeof(iWire))
        {
            INT32 iData = (s_iDemoUseNBO != 0) ? (INT32)ntohl((u_long)iWire) : iWire;

            nRecvCount++;
            f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] rx %d", iData);
        }
        else if (s_stDemoSocket.iSockStatus != enum_Socket_Status_Connected)
        {
            // 상대 종료 또는 타임아웃. 데모는 재접속 안 하고 끝냄
            f_IpcLog(enum_IpcLogCh_Tcp, "[TCP] rx stopped");
            break;
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
// @param	usPortNum				포트 번호 (0 이면 12345)
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

    s_usDemoPort    = (usPortNum != 0U) ? usPortNum : 1234U;
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
