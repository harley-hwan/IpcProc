//
// @file	coord_frames.h
// @brief	레이다 좌표변환. 안테나 극좌표 -> 안테나 직교 -> 동체 -> NED -> ECEF -> LLA 순서로 가고
//			역변환은 그 반대. 거리는 m, 각도는 라디안. 모든 좌표계에서 z 는 아래 방향.
// @author	hwan
// @date	2026.09.02.
//
#ifndef COORD_FRAMES_H_
#define COORD_FRAMES_H_

#ifdef __cplusplus
extern "C" {
#endif

// 규칙 자료형 중 여기서 쓰는 것만. windows.h 를 먼저 include 했으면 거기 있는 걸 그대로 씀
#ifndef _WINDOWS_
typedef signed int          INT32;
typedef char                CHAR;
typedef void                VOID;
#endif
typedef double              FLOAT64;

#define PI                  3.14159265358979323846
#define DEG2RAD(d)          ((d) * PI / 180.0)
#define RAD2DEG(r)          ((r) * 180.0 / PI)

// 지구 타원체 상수. 지구는 완전한 구가 아니라 적도 쪽이 약간 볼록해서 이 값들로 모양을 잡음 (GPS 가 쓰는 WGS-84 값)
#define WGS84_A             6378137.0                       // 장반경 [m]. 지구 중심에서 적도까지 거리
#define WGS84_F             (1.0 / 298.257223563)           // 편평률. 지구가 극 쪽으로 눌린 정도 (a - b) / a. 약 0.3 %
#define WGS84_E2            (WGS84_F * (2.0 - WGS84_F))     // 이심률 제곱. 위도에 따라 지구 곡률이 얼마나 변하는지 정하는 값

//
// @struct	ST_Vec3
// @brief	3차원 벡터. 어느 좌표계든 공통으로 씀
//
typedef struct
{
    FLOAT64                 dX;
    FLOAT64                 dY;
    FLOAT64                 dZ;
} ST_Vec3;

//
// @struct	ST_Polar
// @brief	안테나 극좌표. 신호처리에서 넘어오는 측정값
//
typedef struct
{
    FLOAT64                 dRange;     // 시선거리 [m]
    FLOAT64                 dAz;        // 방위각 [rad]. 안테나 정면에서 오른쪽이 +
    FLOAT64                 dEl;        // 고각 [rad]. 위가 +
} ST_Polar;

//
// @struct	ST_Lla
// @brief	위도 / 경도 / 고도. 고도는 지구 타원체면에서 잰 높이 (해발고도가 아님)
//
typedef struct
{
    FLOAT64                 dLat;       // [rad]
    FLOAT64                 dLon;       // [rad]
    FLOAT64                 dAlt;       // [m]
} ST_Lla;

//
// @struct	ST_Attitude
// @brief	함선 자세. INS 에서 받음
//
typedef struct
{
    FLOAT64                 dRoll;      // [rad]. 우현이 내려가면 +
    FLOAT64                 dPitch;     // [rad]. 선수가 들리면 +
    FLOAT64                 dYaw;       // [rad]. 북쪽에서 시계방향이 +
} ST_Attitude;

//
// @struct	ST_Mount
// @brief	안테나 설치 정보. 고정형이라 한 번 정해지면 안 바뀜
//
typedef struct
{
    FLOAT64                 dAz;        // 안테나가 보는 방향 [rad]. 뱃머리에서 시계방향으로 잰 각
    FLOAT64                 dTilt;      // 안테나 면이 위로 들린 각 [rad]
    ST_Vec3                 stOffset;   // 무게중심 -> 안테나 위치 [m]. 동체 좌표 (x 앞, y 오른쪽, z 아래)
} ST_Mount;

// 1) 안테나 극좌표 <-> 안테나 직교좌표 (x 안테나 정면, y 오른쪽, z 아래)
ST_Vec3     f_PolarToXyz(const ST_Polar stPolar);
ST_Polar    f_XyzToPolar(const ST_Vec3 stXyz);

// 2) 안테나 <-> 동체 (x 뱃머리, y 오른쪽, z 아래)
ST_Vec3     f_AntToBody(const ST_Mount stMount, const ST_Vec3 stAnt);
ST_Vec3     f_BodyToAnt(const ST_Mount stMount, const ST_Vec3 stBody);

// 3) 동체 <-> NED (x 북, y 동, z 아래)
ST_Vec3     f_BodyToNed(const ST_Attitude stAtt, const ST_Vec3 stBody);
ST_Vec3     f_NedToBody(const ST_Attitude stAtt, const ST_Vec3 stNed);

// 4) NED <-> ECEF (지구 중심 직교좌표). stOrigin 은 NED 원점, 즉 함선 위치
ST_Vec3     f_NedToEcef(const ST_Lla stOrigin, const ST_Vec3 stNed);
ST_Vec3     f_EcefToNed(const ST_Lla stOrigin, const ST_Vec3 stEcef);

// 5) ECEF <-> LLA (위도 / 경도 / 고도)
ST_Vec3     f_LlaToEcef(const ST_Lla stLla);
ST_Lla      f_EcefToLla(const ST_Vec3 stEcef);

#ifdef __cplusplus
}
#endif

#endif // COORD_FRAMES_H_
