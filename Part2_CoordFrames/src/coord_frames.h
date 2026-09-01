//
// @file	coord_frames.h
// @brief	레이다 좌표계 변환 라이브러리. 이거 하나만 include 하면 됨.
//			안테나 -> 동체 -> 로컬(NED) -> ECEF -> LLA 순서로 넘기고, 역변환도 같이 있음.
//			모든 프레임에서 z 는 아래. 구조체/함수 인자의 각도는 전부 라디안.
//			회전행렬 이름에 방향을 그대로 씀. CNedFromBody 는 v_ned = C * v_body 라는 뜻이고
//			역변환은 전치(transpose) 하면 됨.
// @author	hwan
// @date	2026.09.02.
//
#ifndef COORD_FRAMES_H_
#define COORD_FRAMES_H_

#ifdef __cplusplus
extern "C" {
#endif

// 규칙에 있는 자료형 중 여기서 쓰는 것만 정의.
// main/test 처럼 windows.h 를 같이 쓰는 경우엔 거기 있는 정의를 그대로 씀
#ifndef _WINDOWS_
typedef signed int          INT32;
typedef char                CHAR;
#endif
#ifndef VOID
typedef void                VOID;
#endif
typedef double              FLOAT64;

// WGS-84 지구 타원체 상수
#define CF_WGS84_A          6378137.0                           // 장반경 [m]
#define CF_WGS84_F_INV      298.257223563                       // 편평률의 역수
#define CF_WGS84_F          (1.0 / CF_WGS84_F_INV)              // 편평률
#define CF_WGS84_B          (CF_WGS84_A * (1.0 - CF_WGS84_F))   // 단반경 [m]
#define CF_WGS84_E2         (CF_WGS84_F * (2.0 - CF_WGS84_F))   // 제1이심률 제곱
#define CF_WGS84_EP2        (CF_WGS84_E2 / (1.0 - CF_WGS84_E2)) // 제2이심률 제곱
#define CF_WGS84_OMEGA      7.292115e-5                         // 자전각속도 [rad/s]

// ECEF -> LLA 반복 횟수. 0 이면 Bowring 1회만 (지상~40 km 는 충분), 2 면 고고도까지 커버
#define CF_ECEF2LLA_REFINE  2

#define CF_PI               3.14159265358979323846
#define CF_DEG2RAD(d)       ((d) * (CF_PI / 180.0))
#define CF_RAD2DEG(r)       ((r) * (180.0 / CF_PI))

// 상태 코드
enum {
    enum_CfStatus_Ok        =  0,
    enum_CfStatus_Null      = -1,   // 널 포인터
    enum_CfStatus_Domain    = -2,   // 정의역 위반 (거리가 0 이하 등)
    enum_CfStatus_Singular  = -3    // 특이점이라 값을 정할 수 없음
};

//
// @struct	ST_CfVec3
// @brief	3차원 벡터
//
typedef struct
{
    FLOAT64                 dX;
    FLOAT64                 dY;
    FLOAT64                 dZ;
} ST_CfVec3;

//
// @struct	ST_CfMat3
// @brief	3x3 행렬 (행 우선)
//
typedef struct
{
    FLOAT64                 daM[3][3];
} ST_CfMat3;

//
// @struct	ST_CfPolar
// @brief	안테나 극좌표. 신호처리가 넘겨주는 원측정값
//
typedef struct
{
    FLOAT64                 dRange_m;   // 시선거리(Slant Range)
    FLOAT64                 dAz_rad;    // 방위각. 보어사이트에서 오른쪽이 (+)
    FLOAT64                 dEl_rad;    // 고각. 안테나 수평면에서 위가 (+)
} ST_CfPolar;

//
// @struct	ST_CfDirCos
// @brief	방향코사인(UV). 단위 지향벡터를 안테나 면에 정사영한 값
//
typedef struct
{
    FLOAT64                 dU;         // 면 가로(오른쪽), cos(El)*sin(Az)
    FLOAT64                 dV;         // 면 세로(위),     sin(El)
    FLOAT64                 dW;         // 보어사이트,      cos(El)*cos(Az)
} ST_CfDirCos;

//
// @struct	ST_CfGeodetic
// @brief	측지 좌표(LLA). 고도는 타원체고(HAE) 이고 해발(MSL) 이 아님
//
typedef struct
{
    FLOAT64                 dLat_rad;
    FLOAT64                 dLon_rad;
    FLOAT64                 dAlt_m;
} ST_CfGeodetic;

//
// @struct	ST_CfAttitude
// @brief	플랫폼 자세. NED 기준 3-2-1 (yaw -> pitch -> roll) 오일러각
//
typedef struct
{
    FLOAT64                 dRoll_rad;  // 우현이 내려가면 (+)
    FLOAT64                 dPitch_rad; // 선수가 들리면 (+)
    FLOAT64                 dYaw_rad;   // 선수방위. 진북에서 시계방향이 (+)
} ST_CfAttitude;

//
// @struct	ST_CfMount
// @brief	안테나 설치 정보. 고정형이라 상수로 두고 씀
//
typedef struct
{
    FLOAT64                 dYaw_rad;       // 설치 방위. 선수 기준 시계방향
    FLOAT64                 dPitch_rad;     // 백틸트. 보어사이트를 들어올린 각
    ST_CfVec3               stLeverArm_b;   // 무게중심 -> 안테나 위상중심, 동체 FRD [m]
} ST_CfMount;

//
// @struct	ST_CfChain
// @brief	정변환 중간값 모음. 보고서/디버깅용으로 한 번에 받아봄
//
typedef struct
{
    ST_CfDirCos             stDirCos;           // 1단계 방향코사인(UV)
    ST_CfVec3               stAntPos;           // 1단계 안테나 직교 [m]
    ST_CfMat3               stCBodyFromAnt;     // 2단계 마운팅 회전행렬
    ST_CfVec3               stBodyPos;          // 2단계 동체 [m]
    ST_CfMat3               stCNedFromBody;     // 3단계 자세 회전행렬
    ST_CfVec3               stNedPos;           // 3단계 로컬 NED [m]
    ST_CfVec3               stEcefPlatform;     // 4단계 플랫폼 ECEF [m]
    ST_CfVec3               stEcefTarget;       // 4단계 표적 ECEF [m]
    ST_CfGeodetic           stTarget;           // 5단계 표적 LLA
    FLOAT64                 dGroundRange_m;     // 로컬 수평거리
    FLOAT64                 dTrueBearing_rad;   // 진북 기준 방위
} ST_CfChain;

// ---- 벡터 / 행렬 기본 연산 ----

ST_CfVec3   f_CfVecAdd(const ST_CfVec3 stA, const ST_CfVec3 stB);
ST_CfVec3   f_CfVecSub(const ST_CfVec3 stA, const ST_CfVec3 stB);
FLOAT64     f_CfVecNorm(const ST_CfVec3 stA);

ST_CfMat3   f_CfRotX(const FLOAT64 dAngle_rad);
ST_CfMat3   f_CfRotY(const FLOAT64 dAngle_rad);
ST_CfMat3   f_CfRotZ(const FLOAT64 dAngle_rad);
ST_CfMat3   f_CfMatMul(const ST_CfMat3 stA, const ST_CfMat3 stB);
ST_CfVec3   f_CfMatApply(const ST_CfMat3 stA, const ST_CfVec3 stV);
ST_CfMat3   f_CfMatTrans(const ST_CfMat3 stA);
FLOAT64     f_CfMatDet(const ST_CfMat3 stA);
FLOAT64     f_CfMatOrthoError(const ST_CfMat3 stA);

// ---- 1단계: 안테나 극좌표 <-> 직교 <-> UV ----

ST_CfVec3   f_CfAntCartFromPolar(const ST_CfPolar stPolar);
ST_CfPolar  f_CfPolarFromAntCart(const ST_CfVec3 stPos);
ST_CfDirCos f_CfDirCosFromPolar(const ST_CfPolar stPolar);
ST_CfPolar  f_CfPolarFromDirCos(const ST_CfDirCos stDirCos, const FLOAT64 dRange_m);

// ---- 2단계: 안테나 <-> 동체 ----

ST_CfMat3   f_CfDcmBodyFromAnt(const ST_CfMount *stpMount);
ST_CfVec3   f_CfBodyFromAnt(const ST_CfMount *stpMount, const ST_CfVec3 stAntPos);
ST_CfVec3   f_CfAntFromBody(const ST_CfMount *stpMount, const ST_CfVec3 stBodyPos);

// ---- 3단계: 동체 <-> 로컬(NED) ----

ST_CfMat3   f_CfDcmNedFromBody(const ST_CfAttitude *stpAtt);
ST_CfVec3   f_CfNedFromBody(const ST_CfAttitude *stpAtt, const ST_CfVec3 stBodyPos);
ST_CfVec3   f_CfBodyFromNed(const ST_CfAttitude *stpAtt, const ST_CfVec3 stNedPos);

// ---- 4단계: 로컬 <-> ECEF, LLA <-> ECEF ----

ST_CfMat3   f_CfDcmNedFromEcef(const FLOAT64 dLat_rad, const FLOAT64 dLon_rad);
ST_CfVec3   f_CfEcefFromGeodetic(const ST_CfGeodetic stGeo);
INT32       f_CfGeodeticFromEcef(const ST_CfVec3 stEcefPos, ST_CfGeodetic *stpOut);
ST_CfVec3   f_CfEcefFromNed(const ST_CfGeodetic stOrigin, const ST_CfVec3 stNedPos);
ST_CfVec3   f_CfNedFromEcef(const ST_CfGeodetic stOrigin, const ST_CfVec3 stEcefPos);

// ---- 참고: ECEF <-> ECI. 지구 자전만 본 간이 변환 ----

FLOAT64     f_CfGmstRad(const FLOAT64 dJulianDateUt1);
ST_CfVec3   f_CfEciFromEcef(const ST_CfVec3 stEcefPos, const FLOAT64 dGmst_rad);
ST_CfVec3   f_CfEcefFromEci(const ST_CfVec3 stEciPos, const FLOAT64 dGmst_rad);

// ---- 전체 체인 ----

INT32       f_CfForwardChain(const ST_CfPolar stMeas, const ST_CfMount *stpMount,
                             const ST_CfAttitude *stpAtt, const ST_CfGeodetic stPlatform,
                             ST_CfGeodetic *stpTargetOut, ST_CfChain *stpChainOut);
INT32       f_CfInverseChain(const ST_CfGeodetic stTarget, const ST_CfMount *stpMount,
                             const ST_CfAttitude *stpAtt, const ST_CfGeodetic stPlatform,
                             ST_CfPolar *stpMeasOut);

#ifdef __cplusplus
}
#endif

#endif // COORD_FRAMES_H_
