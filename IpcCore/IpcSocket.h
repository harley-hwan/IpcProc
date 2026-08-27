//
// @file	IpcSocket.h
// @brief	TCP/IP 송/수신. Linux SocketUtility.h v5.0 을 Winsock2 로 포팅한 것.
//			포팅하면서 달라진 점
//			  - 소켓 핸들이 int 가 아니라 SOCKET(UINT_PTR) 이라 iSockId -> hSockId 로 바꿨다.
//			  - SO_RCVTIMEO/SO_SNDTIMEO 가 timeval 이 아니라 DWORD 밀리초다. 내부에서 변환한다.
//			  - close -> closesocket, errno -> WSAGetLastError, WSAStartup/WSACleanup 필요.
//			  - accept/connect 대기를 밖에서 끊을 수 있게 f_SocketCancelBlockingCalls 를 추가했다.
//			역할 : Tx = 클라이언트(connect), Rx = 서버(bind/listen/accept).
//			원본 .c 가 없어 UDP 쪽 규약(Tx 는 목적지 주소, Rx 는 자기 bind 주소)에서 맞춘 것이므로
//			상대가 반대로 동작하면 Tx/Rx Init 함수를 바꿔 부르면 된다.
// @author	hwan
// @date	2026.08.19.
//
#ifndef IPCSOCKET_H_
#define IPCSOCKET_H_

#include "IpcCommon.h"

// winsock2.h 는 windows.h 보다 먼저 포함되어야 한다
#include <winsock2.h>
#include <ws2tcpip.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SOCKETUTILITY_VER_MAJOR     5
#define SOCKETUTILITY_VER_MINOR     0
#define SOCKETUTILITY_BUILD_DATE    20240823

#define DISP_SOCKET_ERROR_WARNING   1           // 0:OFF, 1:ON
#define DISP_SOCKET_RESULT          0           // 0:OFF, 1:ON

#define CTRL_SOCKET_REPEAT_CONNECT  1           // 연결 실패 시 재시도
#define CTRL_SOCKET_LINK_RECOVERY   1           // 끊김 감지 시 상태 갱신

// Linux 는 int, Windows 는 SOCKET(UINT_PTR)
typedef uintptr_t                   SOCKET_HANDLE;
#define SOCKET_INVALID_HANDLE       ((SOCKET_HANDLE)~(uintptr_t)0)      // == INVALID_SOCKET

#define SOCKET_ALWAYS                       1

#define SOCKET_PASS                         1
#define SOCKET_FAIL                         0

#define SOCKET_MBYTE2BYTE                   (INT64)(1024*1024)

// TCP
#define SOCKET_TCP_QUEUE_SIZE               5

// TCP Sync Message
#define SOCKET_SYNC_PASS_TX_LINE_CHECK      0xAAAA
#define SOCKET_SYNC_PASS_RX_LINE_CHECK      0xBBBB
#define SOCKET_SYNC_PASS_LISTEN             0xCCCC
#define SOCKET_SYNC_PASS_CONNECT            0xDDDD
#define SOCKET_SYNC_PASS_SEND_TO_TX         0xEEEE
#define SOCKET_SYNC_PASS_SEND_TO_RX         0xFFFF

enum{
    // Socket Status
    enum_Socket_Status_Disconnected = -1,
    enum_Socket_Status_NotDefined   = 0,        // Always when UDP
    enum_Socket_Status_Connected    = 1
};

enum{
    // Socket Type
    enum_Socket_Type_NotDefined  = 0,

    enum_Socket_Type_UDP_IPv4Tx  = 1,
    enum_Socket_Type_UDP_IPv4Rx  = 2,

    enum_Socket_Type_TCP_IPv4Tx  = 3,
    enum_Socket_Type_TCP_IPv4Rx  = 4
};

//
// @struct	st_VerInfo_SOCKET
// @brief	소켓 유틸 버전 정보
//
typedef struct st_VerInfo_SOCKET
{
    INT32                iMajorVersion;
    INT32                iMinorVersion;
    INT32                iBuildDate;             // YYYYMMDD
} st_VerInfo_SOCKET;

//
// @struct	st_Socket
// @brief	소켓 하나의 상태
//
typedef struct st_Socket
{
    SOCKET_HANDLE               hSockId;
    SOCKET_HANDLE               hListenId;              // TCP Rx 의 listen 소켓
    INT32                iSockStatus;
    INT32                iSockType;
    UINT32               uiSockAddrSize;
    struct sockaddr_in          stSockAddr;
    struct timeval              stTimeOutVal;
} st_Socket;

IPCCORE_API VOID  __cdecl f_SocketGetVerInfo(st_VerInfo_SOCKET *stpVerInfo);

IPCCORE_API INT32 __cdecl f_SocketStartup(VOID);
IPCCORE_API VOID  __cdecl f_SocketCleanup(VOID);
IPCCORE_API VOID  __cdecl f_SocketCancelBlockingCalls(VOID);
IPCCORE_API VOID  __cdecl f_SocketResetCancel(VOID);

IPCCORE_API INT32 __cdecl f_SocketSetUDP_PartitionSize(const INT32 iUDP_PartitionSize_Byte);
IPCCORE_API VOID  __cdecl f_SocketGetUDP_PartitionSize(INT32 *ipUDP_PartitionSize_Byte);
IPCCORE_API INT32 __cdecl f_SocketSetUDP_PartitionSendGap(const UINT32 uiUDP_PartitionSendGap_us);
IPCCORE_API VOID  __cdecl f_SocketGetUDP_PartitionSendGap(UINT32 *uipUDP_PartitionSendGap_us);

IPCCORE_API st_Socket    __cdecl f_SocketInitUDP_IPv4Tx(const INT8 *cpIpAddr, const UINT16 usPortNum);
IPCCORE_API st_Socket    __cdecl f_SocketInitUDP_IPv4Rx(const INT8 *cpIpAddr, const UINT16 usPortNum, const INT64 lTimeOut_s, const INT64 lTimeOut_us);
IPCCORE_API INT64 __cdecl f_SocketSendUDP_IPv4_Normal(const st_Socket *stpSocket, const VOID *vpDataAddr, const INT64 lDataSize);
IPCCORE_API INT64 __cdecl f_SocketSendUDP_IPv4_Partition(const st_Socket *stpSocket, const VOID *vpDataAddr, const INT64 lFixedDataSize);
IPCCORE_API INT64 __cdecl f_SocketRecvUDP_IPv4_Normal(const st_Socket *stpSocket, const VOID *vpDataAddr, const INT64 lMaxSize);
IPCCORE_API INT64 __cdecl f_SocketRecvUDP_IPv4_Partition(const st_Socket *stpSocket, const VOID *vpDataAddr, const INT64 lFixedDataSize);

IPCCORE_API st_Socket    __cdecl f_SocketInitTCP_IPv4Tx_Normal(const INT8 *cpIpAddr, const UINT16 usPortNum, const INT64 lTimeOut_s, const INT64 lTimeOut_us);
IPCCORE_API st_Socket    __cdecl f_SocketInitTCP_IPv4Tx_Sync(const INT8 *cpIpAddr, const UINT16 usPortNum, const INT64 lTimeOut_s, const INT64 lTimeOut_us);
IPCCORE_API st_Socket    __cdecl f_SocketInitTCP_IPv4Rx_Normal(const INT8 *cpIpAddr, const UINT16 usPortNum, const INT64 lTimeOut_s, const INT64 lTimeOut_us);
IPCCORE_API st_Socket    __cdecl f_SocketInitTCP_IPv4Rx_Sync(const INT8 *cpIpAddr, const UINT16 usPortNum, const INT64 lTimeOut_s, const INT64 lTimeOut_us, const INT32 iMaxSyncTime_s);
IPCCORE_API INT64 __cdecl f_SocketSendTCP_IPv4(st_Socket *stpSocket, const VOID *vpDataAddr, const INT64 lFixedDataSize);
IPCCORE_API INT64 __cdecl f_SocketRecvTCP_IPv4(st_Socket *stpSocket, const VOID *vpDataAddr, const INT64 lFixedDataSize);

IPCCORE_API INT32 __cdecl f_SocketClose(st_Socket stSocket);

// iIsSender : 1 = 송신(클라이언트), 0 = 수신(서버)
IPCCORE_API INT32 __cdecl f_IpcTcpDemoStart(INT32 iIsSender, const INT8 *cpIpAddr,
                                                   UINT16 usPortNum, INT32 iUseNetworkByteOrder);
IPCCORE_API INT32 __cdecl f_IpcTcpDemoStop(VOID);
IPCCORE_API INT32 __cdecl f_IpcTcpDemoIsRunning(VOID);

#ifdef __cplusplus
}
#endif

#endif // IPCSOCKET_H_
