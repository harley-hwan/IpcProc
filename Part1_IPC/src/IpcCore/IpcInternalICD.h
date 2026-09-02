//
// @file	IpcInternalICD.h
// @brief	내부 ICD. IpcCore 안에서 쓰레드 간 / 프로세스 간 통신에 쓰는 정의.
//			규칙대로 CSC 당 1개. 외부 배포 대상은 아님.
// @author	hwan
// @date	2026.08.19.
//
#ifndef IPCINTERNALICD_H_
#define IPCINTERNALICD_H_

#include "IpcExternalICD.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---- 쓰레드 간 (IpcThread) ----

#define IPC_THREAD_RING_SLOTS   8

//
// @struct	ST_IpcThreadMsg
// @brief	쓰레드 간 큐에 싣는 메시지
//
typedef struct
{
    INT32                   nSeq;
    INT32                   iData;
} ST_IpcThreadMsg;

// ---- 프로세스 간 (IpcMsgQ) ----

#define IPC_MSGQ_NAME_MAX       64
#define IPC_MSGQ_PAYLOAD_MAX    256
#define IPC_MSGQ_CAPACITY       64
#define IPC_MSGQ_DEFAULT_NAME   "IpcDemoQ"

#define IPC_MSGQ_MAGIC          0x51435049U         // 'IPCQ'
#define IPC_MSGQ_OBJ_PREFIX     L"Local\\IpcProc."  // 커널 오브젝트 이름 앞부분. 뒤에 .map/.mtx/.sem.e/.sem.f 붙여 씀

#pragma pack(push, 1)

//
// @struct	ST_IpcMsg
// @brief	큐에 싣는 메시지. 공유메모리에 올라가서 패킹 고정
//
typedef struct
{
    INT32                   iMsgType;
    INT32                   iDataSize;
    UCHAR                   ucaData[IPC_MSGQ_PAYLOAD_MAX];
} ST_IpcMsg;

//
// @struct	ST_IpcMsgQRing
// @brief	공유메모리에 올라가는 링버퍼. 양쪽 프로세스가 같이 보는 레이아웃
//
typedef struct
{
    UINT32                  uiMagic;
    INT32                   iCapacity;
    INT32                   iPayloadMax;
    INT32                   iHead;                          // 읽는 위치
    INT32                   iTail;                          // 쓰는 위치
    INT32                   nCount;                         // 쌓인 개수
    ST_IpcMsg               staSlot[IPC_MSGQ_CAPACITY];
} ST_IpcMsgQRing;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif // IPCINTERNALICD_H_
