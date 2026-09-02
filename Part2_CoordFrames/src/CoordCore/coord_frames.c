//
// @file	coord_frames.c
// @brief	좌표변환 구현. 행렬 연산은 참고 소스코드로 받은 matrixCalcLib 을 그대로 씀.
//			위치는 3x1, 회전은 3x3 matrix 로 두고 Matrix_Product / Matrix_Transpose 로 돌림.
//			각도 되찾을 때는 asin 말고 atan2. 반올림으로 인자가 1 을 넘으면 asin 은 NaN 이 나옴.
// @author	hwan
// @date	2026.09.02.
//
#include <math.h>
#include "matrixCalcLib.h"
#include "coord_frames.h"

// ECEF -> LLA 반복 종료 조건
#define LLA_ITER_MAX        10
#define LLA_TOL_LAT         1e-12   // [rad]
#define LLA_TOL_ALT         1e-6    // [m]

// ST_Vec3 <-> 3x1 행렬. 라이브러리 함수가 전부 matrix 만 받아서 들어갈 때 / 나올 때 바꿔줌
static matrix f_ToMat(const ST_Vec3 stV)
{
    matrix stM;

    Matrix_initialize(&stM, 3, 1);
    stM.e[0][0] = stV.dX;
    stM.e[1][0] = stV.dY;
    stM.e[2][0] = stV.dZ;
    return stM;
}

static ST_Vec3 f_ToVec(const matrix *stpM)
{
    ST_Vec3 stV;

    stV.dX = stpM->e[0][0];
    stV.dY = stpM->e[1][0];
    stV.dZ = stpM->e[2][0];
    return stV;
}

// 각 축 회전행렬. Matrix_Identity 는 9x9 로 잡혀서 못 쓰고, 3x3 으로 초기화한 뒤 0 아닌 칸만 채움
static matrix f_RotX(const FLOAT64 dAng)
{
    matrix stR;

    Matrix_initialize(&stR, 3, 3);
    stR.e[0][0] = 1.0;
    stR.e[1][1] =  cos(dAng);
    stR.e[1][2] = -sin(dAng);
    stR.e[2][1] =  sin(dAng);
    stR.e[2][2] =  cos(dAng);
    return stR;
}

static matrix f_RotY(const FLOAT64 dAng)
{
    matrix stR;

    Matrix_initialize(&stR, 3, 3);
    stR.e[0][0] =  cos(dAng);
    stR.e[0][2] =  sin(dAng);
    stR.e[1][1] = 1.0;
    stR.e[2][0] = -sin(dAng);
    stR.e[2][2] =  cos(dAng);
    return stR;
}

static matrix f_RotZ(const FLOAT64 dAng)
{
    matrix stR;

    Matrix_initialize(&stR, 3, 3);
    stR.e[0][0] =  cos(dAng);
    stR.e[0][1] = -sin(dAng);
    stR.e[1][0] =  sin(dAng);
    stR.e[1][1] =  cos(dAng);
    stR.e[2][2] = 1.0;
    return stR;
}

// 안테나 -> 동체 회전. 설치 방위만큼 z 축으로, 설치 고각만큼 y 축으로
static matrix f_RotAntToBody(const ST_Mount stMount)
{
    matrix stRz = f_RotZ(stMount.dAz);
    matrix stRy = f_RotY(stMount.dTilt);

    return Matrix_Product2(&stRz, &stRy);
}

// 동체 -> NED 회전. yaw, pitch, roll 순서로 곱함. 순서를 바꾸면 값이 달라지니 주의
static matrix f_RotBodyToNed(const ST_Attitude stAtt)
{
    matrix stRz = f_RotZ(stAtt.dYaw);
    matrix stRy = f_RotY(stAtt.dPitch);
    matrix stRx = f_RotX(stAtt.dRoll);

    return Matrix_Product3(&stRz, &stRy, &stRx);
}

// NED -> ECEF 회전. 참고 자료에 있는 Rz(경도) * Ry(-90 - 위도) 그대로
static matrix f_RotNedToEcef(const ST_Lla stOrigin)
{
    matrix stRz = f_RotZ(stOrigin.dLon);
    matrix stRy = f_RotY(-PI / 2.0 - stOrigin.dLat);

    return Matrix_Product2(&stRz, &stRy);
}

ST_Vec3 f_PolarToXyz(const ST_Polar stPolar)
{
    ST_Vec3 stXyz;

    stXyz.dX =  stPolar.dRange * cos(stPolar.dEl) * cos(stPolar.dAz);
    stXyz.dY =  stPolar.dRange * cos(stPolar.dEl) * sin(stPolar.dAz);
    stXyz.dZ = -stPolar.dRange * sin(stPolar.dEl);     // z 가 아래라서 위에 있으면 음수
    return stXyz;
}

ST_Polar f_XyzToPolar(const ST_Vec3 stXyz)
{
    ST_Polar stPolar;
    FLOAT64  dXy = sqrt(stXyz.dX * stXyz.dX + stXyz.dY * stXyz.dY);    // 수평 거리

    stPolar.dRange = sqrt(dXy * dXy + stXyz.dZ * stXyz.dZ);
    stPolar.dAz    = atan2(stXyz.dY, stXyz.dX);
    stPolar.dEl    = atan2(-stXyz.dZ, dXy);
    return stPolar;
}

// 회전하고 나서 설치 위치만큼 옮김
ST_Vec3 f_AntToBody(const ST_Mount stMount, const ST_Vec3 stAnt)
{
    matrix stR   = f_RotAntToBody(stMount);
    matrix stP   = f_ToMat(stAnt);
    matrix stOff = f_ToMat(stMount.stOffset);
    matrix stRot = Matrix_Product2(&stR, &stP);
    matrix stOut = Matrix_Add(&stRot, &stOff);

    return f_ToVec(&stOut);
}

// 정변환 반대로. 먼저 빼고 전치한 행렬로 회전
ST_Vec3 f_BodyToAnt(const ST_Mount stMount, const ST_Vec3 stBody)
{
    matrix stR    = f_RotAntToBody(stMount);
    matrix stRt   = Matrix_Transpose(&stR);
    matrix stP    = f_ToMat(stBody);
    matrix stOff  = f_ToMat(stMount.stOffset);
    matrix stDiff = Matrix_Subtract(&stP, &stOff);
    matrix stOut  = Matrix_Product2(&stRt, &stDiff);

    return f_ToVec(&stOut);
}

// 동체와 NED 는 원점이 같아서 회전만
ST_Vec3 f_BodyToNed(const ST_Attitude stAtt, const ST_Vec3 stBody)
{
    matrix stR   = f_RotBodyToNed(stAtt);
    matrix stP   = f_ToMat(stBody);
    matrix stOut = Matrix_Product2(&stR, &stP);

    return f_ToVec(&stOut);
}

ST_Vec3 f_NedToBody(const ST_Attitude stAtt, const ST_Vec3 stNed)
{
    matrix stR   = f_RotBodyToNed(stAtt);
    matrix stRt  = Matrix_Transpose(&stR);
    matrix stP   = f_ToMat(stNed);
    matrix stOut = Matrix_Product2(&stRt, &stP);

    return f_ToVec(&stOut);
}

// NED 벡터를 지구 방향으로 돌리고 함선 ECEF 위치를 더함
ST_Vec3 f_NedToEcef(const ST_Lla stOrigin, const ST_Vec3 stNed)
{
    matrix stR    = f_RotNedToEcef(stOrigin);
    matrix stP    = f_ToMat(stNed);
    matrix stShip = f_ToMat(f_LlaToEcef(stOrigin));
    matrix stRot  = Matrix_Product2(&stR, &stP);
    matrix stOut  = Matrix_Add(&stRot, &stShip);

    return f_ToVec(&stOut);
}

ST_Vec3 f_EcefToNed(const ST_Lla stOrigin, const ST_Vec3 stEcef)
{
    matrix stR    = f_RotNedToEcef(stOrigin);
    matrix stRt   = Matrix_Transpose(&stR);
    matrix stP    = f_ToMat(stEcef);
    matrix stShip = f_ToMat(f_LlaToEcef(stOrigin));
    matrix stDiff = Matrix_Subtract(&stP, &stShip);
    matrix stOut  = Matrix_Product2(&stRt, &stDiff);

    return f_ToVec(&stOut);
}

ST_Vec3 f_LlaToEcef(const ST_Lla stLla)
{
    ST_Vec3 stEcef;
    FLOAT64 dN;     // 그 위도에서 본 지구 곡률반경 (동서 방향). 적도에서 가장 작고 극에서 가장 큼

    dN = WGS84_A / sqrt(1.0 - WGS84_E2 * sin(stLla.dLat) * sin(stLla.dLat));

    stEcef.dX = (dN + stLla.dAlt) * cos(stLla.dLat) * cos(stLla.dLon);
    stEcef.dY = (dN + stLla.dAlt) * cos(stLla.dLat) * sin(stLla.dLon);
    stEcef.dZ = (dN * (1.0 - WGS84_E2) + stLla.dAlt) * sin(stLla.dLat);   // z 만 (1 - e^2) 가 붙음. 지구가 극 쪽으로 눌린 만큼 줄이는 것
    return stEcef;
}

//
// @brief	ECEF -> LLA. 위도를 알아야 N 이 나오고 N 을 알아야 위도가 나와서 한 번에 안 풀림.
//			자료 순서도대로 위도 / 고도를 번갈아 계산하면서 변화가 없어질 때까지 반복
// @param	stEcef	ECEF 위치 [m]
// @return	위도 / 경도 / 고도
//
ST_Lla f_EcefToLla(const ST_Vec3 stEcef)
{
    ST_Lla  stLla;
    FLOAT64 dP = sqrt(stEcef.dX * stEcef.dX + stEcef.dY * stEcef.dY);   // 지구 자전축에서 떨어진 거리
    FLOAT64 dLat0;
    FLOAT64 dAlt0;
    FLOAT64 dLat;
    FLOAT64 dAlt;
    FLOAT64 dN;
    INT32   i;

    stLla.dLon = atan2(stEcef.dY, stEcef.dX);

    // 초기값은 지구를 그냥 공으로 봤을 때 위도, 고도는 0
    dLat0 = atan2(stEcef.dZ, dP);
    dAlt0 = 0.0;
    dLat  = dLat0;
    dAlt  = dAlt0;

    for (i = 0; i < LLA_ITER_MAX; i++)
    {
        dN   = WGS84_A / sqrt(1.0 - WGS84_E2 * sin(dLat0) * sin(dLat0));
        dAlt = dP / cos(dLat0) - dN;
        dLat = atan2(stEcef.dZ * (dN + dAlt), dP * (dN * (1.0 - WGS84_E2) + dAlt));

        if ((fabs(dLat - dLat0) < LLA_TOL_LAT) && (fabs(dAlt - dAlt0) < LLA_TOL_ALT))
        {
            break;
        }
        dLat0 = dLat;
        dAlt0 = dAlt;
    }

    stLla.dLat = dLat;
    stLla.dAlt = dAlt;
    return stLla;
}
