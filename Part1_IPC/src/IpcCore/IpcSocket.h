//
// @file	IpcSocket.h
// @brief	TCP/IP 송/수신. Linux SocketUtility.h/.c v5.0 (ref\참고) 을 Winsock2 로 포팅한 것.
//			포팅하면서 달라진 점
//			  - 소켓 핸들이 int 가 아니라 SOCKET(UINT_PTR) 이라 iSockId -> hSockId 로 바꿈
//			  - SO_RCVTIMEO/SO_SNDTIMEO 가 timeval 이 아니라 DWORD ms 임 (내부에서 변환)
//			  - close -> closesocket, errno -> WSAGetLastError, WSAStartup/WSACleanup 필요
//			  - Windows 는 실패한 소켓에 connect 재시도가 안 돼서 소켓을 새로 만들어 논블로킹 + select 로 붙음
//			  - accept/재접속 대기를 밖에서 끊는 f_SocketCancelBlockingCalls 추가
//			  - MSG_NOSIGNAL 은 Windows 에 SIGPIPE 가 없어서 뺌, RDMA 쪽은 제외
//			Tx 가 클라이언트(connect, 주소는 목적지), Rx 가 서버(bind/listen/accept, 주소는 자기 bind 주소).
//			TCP Sync 메시지 값(SOCKET_SYNC_PASS_*)은 CSCI 간 통신 규약이라 IpcExternalICD.h 에 있음.
// @author	hwan
// @date	2026.08.19.
//
#ifndef IPCSOCKET_H_
#define IPCSOCKET_H_

#include "IpcExternalICD.h"

// winsock2.h 가 windows.h 보다 먼저 와야 함
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
#define CTRL_SOCKET_LINK_RECOVERY   1           // 끊김 감지 시 닫고 다시 붙음

// Linux 는 int, Windows 는 SOCKET(UINT_PTR)
typedef uintptr_t                   SOCKET_HANDLE;
#define SOCKET_INVALID_HANDLE       ((SOCKET_HANDLE)~(uintptr_t)0)      // == INVALID_SOCKET

#define SOCKET_ALWAYS                       1

#define SOCKET_PASS                         1
#define SOCKET_FAIL                         0

#define SOCKET_MBYTE2BYTE                   (INT64)(1024*1024)

// TCP
#define SOCKET_TCP_QUEUE_SIZE               5

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

// 원본과 같이 패킹 고정
#pragma pack(push, 1)

//
// @struct	ST_VerInfo_SOCKET
// @brief	소켓 유틸 버전 정보
//
typedef struct
{
    INT32                       iMajorVersion;
    INT32                       iMinorVersion;
    INT32                       iBuildDate;             // YYYYMMDD
} ST_VerInfo_SOCKET;

//
// @struct	ST_Socket
// @brief	소켓 하나의 상태. Tx 는 목적지 주소, Rx 는 자기 bind 주소를 들고 있음 (재접속에 씀)
//
typedef struct
{
    SOCKET_HANDLE               hSockId;
    INT32                       iSockStatus;
    INT32                       iSockType;
    UINT32                      uiSockAddrSize;
    struct sockaddr_in          stSockAddr;
    struct timeval              stTimeOutVal;
} ST_Socket;

#pragma pack(pop)

IPCCORE_API VOID  __cdecl f_SocketGetVerInfo(ST_VerInfo_SOCKET *stpVerInfo);

IPCCORE_API INT32 __cdecl f_SocketStartup(VOID);
IPCCORE_API VOID  __cdecl f_SocketCleanup(VOID);
IPCCORE_API VOID  __cdecl f_SocketCancelBlockingCalls(VOID);
IPCCORE_API VOID  __cdecl f_SocketResetCancel(VOID);

IPCCORE_API INT32 __cdecl f_SocketSetUDP_PartitionSize(const INT32 iUDP_PartitionSize_Byte);
IPCCORE_API VOID  __cdecl f_SocketGetUDP_PartitionSize(INT32 *ipUDP_PartitionSize_Byte);
IPCCORE_API INT32 __cdecl f_SocketSetUDP_PartitionSendGap(const UINT32 uiUDP_PartitionSendGap_us);
IPCCORE_API VOID  __cdecl f_SocketGetUDP_PartitionSendGap(UINT32 *uipUDP_PartitionSendGap_us);

IPCCORE_API ST_Socket __cdecl f_SocketInitUDP_IPv4Tx(const INT8 *cpIpAddr, const UINT16 usPortNum);
IPCCORE_API ST_Socket __cdecl f_SocketInitUDP_IPv4Rx(const INT8 *cpIpAddr, const UINT16 usPortNum, const INT64 lTimeOut_s, const INT64 lTimeOut_us);
IPCCORE_API INT64     __cdecl f_SocketSendUDP_IPv4_Normal(const ST_Socket *stpSocket, const VOID *vpDataAddr, const INT64 lDataSize);
IPCCORE_API INT64     __cdecl f_SocketSendUDP_IPv4_Partition(const ST_Socket *stpSocket, const VOID *vpDataAddr, const INT64 lFixedDataSize);
IPCCORE_API INT64     __cdecl f_SocketRecvUDP_IPv4_Normal(const ST_Socket *stpSocket, const VOID *vpDataAddr, const INT64 lMaxSize);
IPCCORE_API INT64     __cdecl f_SocketRecvUDP_IPv4_Partition(const ST_Socket *stpSocket, const VOID *vpDataAddr, const INT64 lFixedDataSize);

IPCCORE_API ST_Socket __cdecl f_SocketInitTCP_IPv4Tx_Normal(const INT8 *cpIpAddr, const UINT16 usPortNum, const INT64 lTimeOut_s, const INT64 lTimeOut_us);
IPCCORE_API ST_Socket __cdecl f_SocketInitTCP_IPv4Tx_Sync(const INT8 *cpIpAddr, const UINT16 usPortNum, const INT64 lTimeOut_s, const INT64 lTimeOut_us);
IPCCORE_API ST_Socket __cdecl f_SocketInitTCP_IPv4Rx_Normal(const INT8 *cpIpAddr, const UINT16 usPortNum, const INT64 lTimeOut_s, const INT64 lTimeOut_us);
IPCCORE_API ST_Socket __cdecl f_SocketInitTCP_IPv4Rx_Sync(const INT8 *cpIpAddr, const UINT16 usPortNum, const INT64 lTimeOut_s, const INT64 lTimeOut_us, const INT32 iMaxSyncTime_s);
IPCCORE_API INT64     __cdecl f_SocketSendTCP_IPv4(ST_Socket *stpSocket, const VOID *vpDataAddr, const INT64 lFixedDataSize);
IPCCORE_API INT64     __cdecl f_SocketRecvTCP_IPv4(ST_Socket *stpSocket, const VOID *vpDataAddr, const INT64 lFixedDataSize);

IPCCORE_API INT32 __cdecl f_SocketClose(const ST_Socket st_Socket);

// iIsSender : 1 = 송신(클라이언트), 0 = 수신(서버)
IPCCORE_API INT32 __cdecl f_IpcTcpDemoStart(const INT32 iIsSender, const INT8 *cpIpAddr,
                                            const UINT16 usPortNum, const INT32 iUseNetworkByteOrder);
IPCCORE_API INT32 __cdecl f_IpcTcpDemoStop(VOID);
IPCCORE_API INT32 __cdecl f_IpcTcpDemoIsRunning(VOID);

#ifdef __cplusplus
}
#endif

#endif // IPCSOCKET_H_
