//
// @file	coord_frames.c
// @brief	좌표계 변환 구현.
//			각도를 되찾을 때 asin/acos 대신 atan2 를 씀. 반올림으로 인자가 1 을 살짝 넘으면
//			asin 은 NaN 이 나오고, ±1 부근에서 정밀도도 떨어져서 clamp 가 필요해짐.
//			atan2 는 정의역 제한이 없어서 그냥 쓸 수 있음.
// @author	hwan
// @date	2026.09.02.
//
#include "coord_frames.h"
#include <math.h>
#include <string.h>

ST_CfVec3 f_CfVecAdd(const ST_CfVec3 stA, const ST_CfVec3 stB)
{
    ST_CfVec3 stR;

    stR.dX = stA.dX + stB.dX;
    stR.dY = stA.dY + stB.dY;
    stR.dZ = stA.dZ + stB.dZ;
    return stR;
}

ST_CfVec3 f_CfVecSub(const ST_CfVec3 stA, const ST_CfVec3 stB)
{
    ST_CfVec3 stR;

    stR.dX = stA.dX - stB.dX;
    stR.dY = stA.dY - stB.dY;
    stR.dZ = stA.dZ - stB.dZ;
    return stR;
}

FLOAT64 f_CfVecNorm(const ST_CfVec3 stA)
{
    return sqrt(stA.dX * stA.dX + stA.dY * stA.dY + stA.dZ * stA.dZ);
}

// 각 축 둘레의 우수(right-handed) 회전
ST_CfMat3 f_CfRotX(const FLOAT64 dAngle_rad)
{
    const FLOAT64 dC = cos(dAngle_rad);
    const FLOAT64 dS = sin(dAngle_rad);
    ST_CfMat3     stR = { { {1.0, 0.0, 0.0},
                            {0.0,  dC, -dS},
                            {0.0,  dS,  dC} } };
    return stR;
}

ST_CfMat3 f_CfRotY(const FLOAT64 dAngle_rad)
{
    const FLOAT64 dC = cos(dAngle_rad);
    const FLOAT64 dS = sin(dAngle_rad);
    ST_CfMat3     stR = { { { dC, 0.0,  dS},
                            {0.0, 1.0, 0.0},
                            {-dS, 0.0,  dC} } };
    return stR;
}

ST_CfMat3 f_CfRotZ(const FLOAT64 dAngle_rad)
{
    const FLOAT64 dC = cos(dAngle_rad);
    const FLOAT64 dS = sin(dAngle_rad);
    ST_CfMat3     stR = { { { dC, -dS, 0.0},
                            { dS,  dC, 0.0},
                            {0.0, 0.0, 1.0} } };
    return stR;
}

ST_CfMat3 f_CfMatMul(const ST_CfMat3 stA, const ST_CfMat3 stB)
{
    ST_CfMat3 stR;
    INT32     i;
    INT32     j;
    INT32     k;

    for (i = 0; i < 3; ++i)
    {
        for (j = 0; j < 3; ++j)
        {
            FLOAT64 dSum = 0.0;
            for (k = 0; k < 3; ++k)
            {
                dSum += stA.daM[i][k] * stB.daM[k][j];
            }
            stR.daM[i][j] = dSum;
        }
    }
    return stR;
}

ST_CfVec3 f_CfMatApply(const ST_CfMat3 stA, const ST_CfVec3 stV)
{
    ST_CfVec3 stR;

    stR.dX = stA.daM[0][0] * stV.dX + stA.daM[0][1] * stV.dY + stA.daM[0][2] * stV.dZ;
    stR.dY = stA.daM[1][0] * stV.dX + stA.daM[1][1] * stV.dY + stA.daM[1][2] * stV.dZ;
    stR.dZ = stA.daM[2][0] * stV.dX + stA.daM[2][1] * stV.dY + stA.daM[2][2] * stV.dZ;
    return stR;
}

ST_CfMat3 f_CfMatTrans(const ST_CfMat3 stA)
{
    ST_CfMat3 stR;
    INT32     i;
    INT32     j;

    for (i = 0; i < 3; ++i)
    {
        for (j = 0; j < 3; ++j)
        {
            stR.daM[i][j] = stA.daM[j][i];
        }
    }
    return stR;
}

FLOAT64 f_CfMatDet(const ST_CfMat3 stA)
{
    return stA.daM[0][0] * (stA.daM[1][1] * stA.daM[2][2] - stA.daM[1][2] * stA.daM[2][1])
         - stA.daM[0][1] * (stA.daM[1][0] * stA.daM[2][2] - stA.daM[1][2] * stA.daM[2][0])
         + stA.daM[0][2] * (stA.daM[1][0] * stA.daM[2][1] - stA.daM[1][1] * stA.daM[2][0]);
}

// |A^T*A - I| 의 최대 절대값. 0 에 가까울수록 정상적인 회전행렬
FLOAT64 f_CfMatOrthoError(const ST_CfMat3 stA)
{
    ST_CfMat3 stP = f_CfMatMul(f_CfMatTrans(stA), stA);
    FLOAT64   dWorst = 0.0;
    INT32     i;
    INT32     j;

    for (i = 0; i < 3; ++i)
    {
        for (j = 0; j < 3; ++j)
        {
            const FLOAT64 dExpect = (i == j) ? 1.0 : 0.0;
            const FLOAT64 dErr    = fabs(stP.daM[i][j] - dExpect);
            if (dErr > dWorst)
            {
                dWorst = dErr;
            }
        }
    }
    return dWorst;
}

//
// @brief	안테나 극좌표 -> 안테나 직교좌표.
//			x = 보어사이트, y = 오른쪽, z = 아래라서 위로 향하면 z 가 음수
// @param	stPolar		거리 / 방위각 / 고각
// @return	안테나 직교 위치 [m]
//
ST_CfVec3 f_CfAntCartFromPolar(const ST_CfPolar stPolar)
{
    const FLOAT64 dCe = cos(stPolar.dEl_rad);
    ST_CfVec3     stR;

    stR.dX =  stPolar.dRange_m * dCe * cos(stPolar.dAz_rad);
    stR.dY =  stPolar.dRange_m * dCe * sin(stPolar.dAz_rad);
    stR.dZ = -stPolar.dRange_m * sin(stPolar.dEl_rad);
    return stR;
}

ST_CfPolar f_CfPolarFromAntCart(const ST_CfVec3 stPos)
{
    ST_CfPolar stOut;

    stOut.dRange_m = f_CfVecNorm(stPos);
    if (stOut.dRange_m < 1e-12)     // 원점에서는 방향이 정의되지 않음
    {
        stOut.dAz_rad = 0.0;
        stOut.dEl_rad = 0.0;
        return stOut;
    }

    stOut.dAz_rad = atan2(stPos.dY, stPos.dX);
    stOut.dEl_rad = atan2(-stPos.dZ, sqrt(stPos.dX * stPos.dX + stPos.dY * stPos.dY));
    return stOut;
}

ST_CfDirCos f_CfDirCosFromPolar(const ST_CfPolar stPolar)
{
    const FLOAT64 dCe = cos(stPolar.dEl_rad);
    ST_CfDirCos   stD;

    stD.dU = dCe * sin(stPolar.dAz_rad);
    stD.dV = sin(stPolar.dEl_rad);
    stD.dW = dCe * cos(stPolar.dAz_rad);
    return stD;
}

ST_CfPolar f_CfPolarFromDirCos(const ST_CfDirCos stDirCos, const FLOAT64 dRange_m)
{
    ST_CfPolar stP;

    stP.dRange_m = dRange_m;
    stP.dAz_rad  = atan2(stDirCos.dU, stDirCos.dW);
    stP.dEl_rad  = atan2(stDirCos.dV, sqrt(stDirCos.dU * stDirCos.dU + stDirCos.dW * stDirCos.dW));
    return stP;
}

// 고정형 안테나라 회전행렬은 설치 도면이 정하는 상수. Rz(설치방위) * Ry(백틸트)
ST_CfMat3 f_CfDcmBodyFromAnt(const ST_CfMount *stpMount)
{
    if (stpMount == NULL)
    {
        ST_CfMat3 stI = { { {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0} } };
        return stI;
    }
    return f_CfMatMul(f_CfRotZ(stpMount->dYaw_rad), f_CfRotY(stpMount->dPitch_rad));
}

// 회전만으로는 부족하고 원점이 다르니 레버암을 더해줘야 함
ST_CfVec3 f_CfBodyFromAnt(const ST_CfMount *stpMount, const ST_CfVec3 stAntPos)
{
    ST_CfVec3 stRotated = f_CfMatApply(f_CfDcmBodyFromAnt(stpMount), stAntPos);

    if (stpMount == NULL)
    {
        return stRotated;
    }
    return f_CfVecAdd(stRotated, stpMount->stLeverArm_b);
}

ST_CfVec3 f_CfAntFromBody(const ST_CfMount *stpMount, const ST_CfVec3 stBodyPos)
{
    ST_CfVec3 stShifted = stBodyPos;

    if (stpMount != NULL)
    {
        stShifted = f_CfVecSub(stBodyPos, stpMount->stLeverArm_b);
    }
    return f_CfMatApply(f_CfMatTrans(f_CfDcmBodyFromAnt(stpMount)), stShifted);
}

// 3-2-1 오일러. Rz(yaw) * Ry(pitch) * Rx(roll)
ST_CfMat3 f_CfDcmNedFromBody(const ST_CfAttitude *stpAtt)
{
    if (stpAtt == NULL)
    {
        ST_CfMat3 stI = { { {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0} } };
        return stI;
    }
    return f_CfMatMul(f_CfMatMul(f_CfRotZ(stpAtt->dYaw_rad),
                                 f_CfRotY(stpAtt->dPitch_rad)),
                      f_CfRotX(stpAtt->dRoll_rad));
}

ST_CfVec3 f_CfNedFromBody(const ST_CfAttitude *stpAtt, const ST_CfVec3 stBodyPos)
{
    return f_CfMatApply(f_CfDcmNedFromBody(stpAtt), stBodyPos);
}

ST_CfVec3 f_CfBodyFromNed(const ST_CfAttitude *stpAtt, const ST_CfVec3 stNedPos)
{
    return f_CfMatApply(f_CfMatTrans(f_CfDcmNedFromBody(stpAtt)), stNedPos);
}

ST_CfMat3 f_CfDcmNedFromEcef(const FLOAT64 dLat_rad, const FLOAT64 dLon_rad)
{
    const FLOAT64 dSp = sin(dLat_rad);
    const FLOAT64 dCp = cos(dLat_rad);
    const FLOAT64 dSl = sin(dLon_rad);
    const FLOAT64 dCl = cos(dLon_rad);
    ST_CfMat3     stR = { { {-dSp * dCl, -dSp * dSl,  dCp},
                            {      -dSl,        dCl,  0.0},
                            {-dCp * dCl, -dCp * dSl, -dSp} } };
    return stR;
}

ST_CfVec3 f_CfEcefFromGeodetic(const ST_CfGeodetic stGeo)
{
    const FLOAT64 dSp = sin(stGeo.dLat_rad);
    const FLOAT64 dCp = cos(stGeo.dLat_rad);
    const FLOAT64 dSl = sin(stGeo.dLon_rad);
    const FLOAT64 dCl = cos(stGeo.dLon_rad);
    const FLOAT64 dN  = CF_WGS84_A / sqrt(1.0 - CF_WGS84_E2 * dSp * dSp);   // 묘유선 곡률반경
    ST_CfVec3     stP;

    stP.dX = (dN + stGeo.dAlt_m) * dCp * dCl;
    stP.dY = (dN + stGeo.dAlt_m) * dCp * dSl;
    stP.dZ = (dN * (1.0 - CF_WGS84_E2) + stGeo.dAlt_m) * dSp;   // (1-e^2) 빠뜨리기 쉬움
    return stP;
}

//
// @brief	ECEF -> LLA. Bowring 폐형식으로 초기 위도를 잡고 Hirvonen 반복으로 다듬음
// @param	stEcefPos	ECEF 위치 [m]
// @param	stpOut		측지 좌표 결과
// @return	enum_CfStatus_Ok / enum_CfStatus_Null
//
INT32 f_CfGeodeticFromEcef(const ST_CfVec3 stEcefPos, ST_CfGeodetic *stpOut)
{
    FLOAT64 dRho;
    FLOAT64 dTheta;
    FLOAT64 dSt;
    FLOAT64 dCt;
    FLOAT64 dSp;
    FLOAT64 dN;
    FLOAT64 dH;
    INT32   iIter;

    if (stpOut == NULL)
    {
        return enum_CfStatus_Null;
    }

    dRho = sqrt(stEcefPos.dX * stEcefPos.dX + stEcefPos.dY * stEcefPos.dY);

    if (dRho < 1e-9)    // 극점. 여기선 경도가 정의되지 않음
    {
        stpOut->dLat_rad = (stEcefPos.dZ < 0.0) ? -CF_PI / 2.0 : CF_PI / 2.0;
        stpOut->dLon_rad = 0.0;
        stpOut->dAlt_m   = fabs(stEcefPos.dZ) - CF_WGS84_B;
        return enum_CfStatus_Ok;
    }

    stpOut->dLon_rad = atan2(stEcefPos.dY, stEcefPos.dX);

    dTheta = atan2(stEcefPos.dZ * CF_WGS84_A, dRho * CF_WGS84_B);   // 보조 규약위도
    dSt = sin(dTheta);
    dCt = cos(dTheta);
    stpOut->dLat_rad = atan2(stEcefPos.dZ + CF_WGS84_EP2 * CF_WGS84_B * dSt * dSt * dSt,
                             dRho         - CF_WGS84_E2  * CF_WGS84_A * dCt * dCt * dCt);

    for (iIter = 0; iIter < CF_ECEF2LLA_REFINE; ++iIter)
    {
        dSp = sin(stpOut->dLat_rad);
        dN  = CF_WGS84_A / sqrt(1.0 - CF_WGS84_E2 * dSp * dSp);
        dH  = dRho * cos(stpOut->dLat_rad) + (stEcefPos.dZ + CF_WGS84_E2 * dN * dSp) * dSp - dN;
        stpOut->dLat_rad = atan2(stEcefPos.dZ, dRho * (1.0 - CF_WGS84_E2 * dN / (dN + dH)));
    }

    dSp = sin(stpOut->dLat_rad);
    dN  = CF_WGS84_A / sqrt(1.0 - CF_WGS84_E2 * dSp * dSp);

    // h = rho/cos(lat) - N 대신 아래 항등식. 1/cos(lat) 이 없어서 극점에서도 안전함
    stpOut->dAlt_m = dRho * cos(stpOut->dLat_rad)
                   + (stEcefPos.dZ + CF_WGS84_E2 * dN * dSp) * dSp
                   - dN;
    return enum_CfStatus_Ok;
}

ST_CfVec3 f_CfEcefFromNed(const ST_CfGeodetic stOrigin, const ST_CfVec3 stNedPos)
{
    ST_CfMat3 stCEcefFromNed =
        f_CfMatTrans(f_CfDcmNedFromEcef(stOrigin.dLat_rad, stOrigin.dLon_rad));

    return f_CfVecAdd(f_CfEcefFromGeodetic(stOrigin),
                      f_CfMatApply(stCEcefFromNed, stNedPos));
}

ST_CfVec3 f_CfNedFromEcef(const ST_CfGeodetic stOrigin, const ST_CfVec3 stEcefPos)
{
    ST_CfVec3 stDiff = f_CfVecSub(stEcefPos, f_CfEcefFromGeodetic(stOrigin));

    return f_CfMatApply(f_CfDcmNedFromEcef(stOrigin.dLat_rad, stOrigin.dLon_rad), stDiff);
}

// 세차/장동/극운동은 뺀 간이 계산. 설계에는 안 쓰이고 1.5 절 확인용
FLOAT64 f_CfGmstRad(const FLOAT64 dJulianDateUt1)
{
    const FLOAT64 dD = dJulianDateUt1 - 2451545.0;  // J2000.0 부터의 일수
    const FLOAT64 dT = dD / 36525.0;                // 율리우스 세기
    FLOAT64       dDeg;

    dDeg = 280.46061837 + 360.98564736629 * dD
         + 0.000387933 * dT * dT - (dT * dT * dT) / 38710000.0;
    dDeg = fmod(dDeg, 360.0);
    if (dDeg < 0.0)
    {
        dDeg += 360.0;
    }
    return CF_DEG2RAD(dDeg);
}

ST_CfVec3 f_CfEciFromEcef(const ST_CfVec3 stEcefPos, const FLOAT64 dGmst_rad)
{
    return f_CfMatApply(f_CfRotZ(dGmst_rad), stEcefPos);
}

ST_CfVec3 f_CfEcefFromEci(const ST_CfVec3 stEciPos, const FLOAT64 dGmst_rad)
{
    return f_CfMatApply(f_CfMatTrans(f_CfRotZ(dGmst_rad)), stEciPos);
}

//
// @brief	신호처리 측정값 -> 표적 LLA. 안테나부터 LLA 까지 한 번에 넘김
// @param	stMeas			안테나 극좌표 측정값
// @param	stpMount		안테나 설치 정보
// @param	stpAtt			플랫폼 자세
// @param	stPlatform		플랫폼 위치 (LLA)
// @param	stpTargetOut	표적 LLA 결과
// @param	stpChainOut		단계별 중간값. 필요 없으면 NULL
// @return	enum_CfStatus_Ok / enum_CfStatus_Null / enum_CfStatus_Domain
//
INT32 f_CfForwardChain(const ST_CfPolar stMeas, const ST_CfMount *stpMount,
                       const ST_CfAttitude *stpAtt, const ST_CfGeodetic stPlatform,
                       ST_CfGeodetic *stpTargetOut, ST_CfChain *stpChainOut)
{
    ST_CfChain stChain;
    INT32      iStatus;

    if ((stpMount == NULL) || (stpAtt == NULL) || (stpTargetOut == NULL))
    {
        return enum_CfStatus_Null;
    }
    if (stMeas.dRange_m <= 0.0)
    {
        return enum_CfStatus_Domain;
    }

    (VOID)memset(&stChain, 0, sizeof(stChain));

    // 1단계 극좌표 -> 직교(+UV)
    stChain.stDirCos        = f_CfDirCosFromPolar(stMeas);
    stChain.stAntPos        = f_CfAntCartFromPolar(stMeas);

    // 2단계 안테나 -> 동체 (회전 + 레버암)
    stChain.stCBodyFromAnt  = f_CfDcmBodyFromAnt(stpMount);
    stChain.stBodyPos       = f_CfBodyFromAnt(stpMount, stChain.stAntPos);

    // 3단계 동체 -> 로컬 NED (자세 회전)
    stChain.stCNedFromBody  = f_CfDcmNedFromBody(stpAtt);
    stChain.stNedPos        = f_CfNedFromBody(stpAtt, stChain.stBodyPos);

    // 4단계 로컬 -> ECEF
    stChain.stEcefPlatform  = f_CfEcefFromGeodetic(stPlatform);
    stChain.stEcefTarget    = f_CfEcefFromNed(stPlatform, stChain.stNedPos);

    // 5단계 ECEF -> LLA
    iStatus = f_CfGeodeticFromEcef(stChain.stEcefTarget, &stChain.stTarget);
    if (iStatus != enum_CfStatus_Ok)
    {
        return iStatus;
    }

    stChain.dGroundRange_m   = sqrt(stChain.stNedPos.dX * stChain.stNedPos.dX
                                  + stChain.stNedPos.dY * stChain.stNedPos.dY);
    stChain.dTrueBearing_rad = atan2(stChain.stNedPos.dY, stChain.stNedPos.dX);
    if (stChain.dTrueBearing_rad < 0.0)
    {
        stChain.dTrueBearing_rad += 2.0 * CF_PI;
    }

    *stpTargetOut = stChain.stTarget;
    if (stpChainOut != NULL)
    {
        *stpChainOut = stChain;
    }
    return enum_CfStatus_Ok;
}

//
// @brief	표적 LLA -> 안테나 극좌표. 정변환을 거꾸로 타면서 회전은 전치를 씀
// @param	stTarget	표적 LLA
// @param	stpMount	안테나 설치 정보
// @param	stpAtt		플랫폼 자세
// @param	stPlatform	플랫폼 위치 (LLA)
// @param	stpMeasOut	안테나 극좌표 결과
// @return	enum_CfStatus_Ok / enum_CfStatus_Null
//
INT32 f_CfInverseChain(const ST_CfGeodetic stTarget, const ST_CfMount *stpMount,
                       const ST_CfAttitude *stpAtt, const ST_CfGeodetic stPlatform,
                       ST_CfPolar *stpMeasOut)
{
    ST_CfVec3 stEcefPos;
    ST_CfVec3 stNedPos;
    ST_CfVec3 stBodyPos;
    ST_CfVec3 stAntPos;

    if ((stpMount == NULL) || (stpAtt == NULL) || (stpMeasOut == NULL))
    {
        return enum_CfStatus_Null;
    }

    stEcefPos = f_CfEcefFromGeodetic(stTarget);
    stNedPos  = f_CfNedFromEcef(stPlatform, stEcefPos);
    stBodyPos = f_CfBodyFromNed(stpAtt, stNedPos);
    stAntPos  = f_CfAntFromBody(stpMount, stBodyPos);

    *stpMeasOut = f_CfPolarFromAntCart(stAntPos);
    return enum_CfStatus_Ok;
}
