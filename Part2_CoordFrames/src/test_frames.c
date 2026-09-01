//
// @file	test_frames.c
// @brief	coord_frames 단위 시험. 좌표변환 버그는 대부분 컴파일도 되고 값도 그럴듯한 종류라
//			각 시험이 노리는 버그를 하나씩 정해 뒀음.
//			T1 행렬 성질 / T2 극↔직교 / T3 UV↔극 / T4 LLA↔ECEF / T5 전체 체인 /
//			T6 기지값 / T7 특이점 / T8 전치 방향 / T9 부호 민감도
// @author	hwan
// @date	2026.09.02.
//
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include "coord_frames.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

static INT32 s_nPass = 0;
static INT32 s_nFail = 0;

// 한국어 Windows 콘솔은 CP949 라서 UTF-8 한글이 그대로 나가면 깨짐. 시작할 때 한 번만 바꿔줌
static VOID f_ConsoleUseUtf8(VOID)
{
#ifdef _WIN32
    (VOID)SetConsoleOutputCP(CP_UTF8);
#endif
}

static VOID f_Check(const CHAR *cpName, const INT32 iOk, const FLOAT64 dMeasured, const FLOAT64 dTol)
{
    if (iOk != 0)
    {
        ++s_nPass;
        printf("  [ OK ] %-46s %9.2e (허용 %.0e)\n", cpName, dMeasured, dTol);
    }
    else
    {
        ++s_nFail;
        printf("  [FAIL] %-46s %9.2e (허용 %.0e)\n", cpName, dMeasured, dTol);
    }
}

static VOID f_CheckLe(const CHAR *cpName, const FLOAT64 dMeasured, const FLOAT64 dTol)
{
    f_Check(cpName, (dMeasured <= dTol) && !isnan(dMeasured), dMeasured, dTol);
}

// 재현 가능해야 해서 srand 고정하고 표준 rand 만 씀
static FLOAT64 f_URand(const FLOAT64 dLo, const FLOAT64 dHi)
{
    return dLo + (dHi - dLo) * ((FLOAT64)rand() / (FLOAT64)RAND_MAX);
}

static VOID f_T1MatrixProperties(VOID)
{
    FLOAT64 dWorstDet  = 0.0;
    FLOAT64 dWorstOrth = 0.0;
    INT32   i;

    printf("\nT1  회전행렬 성질 (det = +1, 직교)\n");
    for (i = 0; i < 2000; ++i)
    {
        ST_CfAttitude stAtt;
        ST_CfMount    stMount;
        ST_CfMat3     stC;
        FLOAT64       dLat;
        FLOAT64       dLon;

        stAtt.dRoll_rad  = f_URand(-CF_PI, CF_PI);
        stAtt.dPitch_rad = f_URand(-CF_PI / 2.0, CF_PI / 2.0);
        stAtt.dYaw_rad   = f_URand(-CF_PI, CF_PI);
        stC = f_CfDcmNedFromBody(&stAtt);
        dWorstDet  = fmax(dWorstDet,  fabs(f_CfMatDet(stC) - 1.0));
        dWorstOrth = fmax(dWorstOrth, f_CfMatOrthoError(stC));

        stMount.dYaw_rad   = f_URand(-CF_PI, CF_PI);
        stMount.dPitch_rad = f_URand(-CF_PI / 2.0, CF_PI / 2.0);
        stMount.stLeverArm_b.dX = 0.0;
        stMount.stLeverArm_b.dY = 0.0;
        stMount.stLeverArm_b.dZ = 0.0;
        stC = f_CfDcmBodyFromAnt(&stMount);
        dWorstDet  = fmax(dWorstDet,  fabs(f_CfMatDet(stC) - 1.0));
        dWorstOrth = fmax(dWorstOrth, f_CfMatOrthoError(stC));

        dLat = f_URand(-CF_PI / 2.0, CF_PI / 2.0);
        dLon = f_URand(-CF_PI, CF_PI);
        stC = f_CfDcmNedFromEcef(dLat, dLon);
        dWorstDet  = fmax(dWorstDet,  fabs(f_CfMatDet(stC) - 1.0));
        dWorstOrth = fmax(dWorstOrth, f_CfMatOrthoError(stC));
    }
    f_CheckLe("|det − 1| 최대", dWorstDet, 1e-12);
    f_CheckLe("|AᵀA − I| 최대", dWorstOrth, 1e-12);
}

static VOID f_T2PolarCartRoundtrip(VOID)
{
    FLOAT64 dWorstR = 0.0;
    FLOAT64 dWorstA = 0.0;
    FLOAT64 dWorstE = 0.0;
    INT32   i;

    printf("\nT2  안테나 극좌표 ↔ 직교좌표 왕복\n");
    for (i = 0; i < 100000; ++i)
    {
        ST_CfPolar stP;
        ST_CfPolar stQ;

        stP.dRange_m = f_URand(1.0, 500000.0);
        stP.dAz_rad  = f_URand(-CF_PI + 1e-6, CF_PI - 1e-6);
        stP.dEl_rad  = f_URand(-CF_PI / 2.0 + 1e-6, CF_PI / 2.0 - 1e-6);
        stQ = f_CfPolarFromAntCart(f_CfAntCartFromPolar(stP));
        dWorstR = fmax(dWorstR, fabs(stQ.dRange_m - stP.dRange_m));
        dWorstA = fmax(dWorstA, fabs(stQ.dAz_rad  - stP.dAz_rad));
        dWorstE = fmax(dWorstE, fabs(stQ.dEl_rad  - stP.dEl_rad));
    }
    f_CheckLe("거리 오차 [m]",   dWorstR, 1e-6);
    f_CheckLe("방위각 오차 [rad]", dWorstA, 1e-12);
    f_CheckLe("고각 오차 [rad]",   dWorstE, 1e-13);
}

static VOID f_T3UvRoundtrip(VOID)
{
    FLOAT64 dWorstA = 0.0;
    FLOAT64 dWorstE = 0.0;
    FLOAT64 dWorstN = 0.0;
    INT32   i;

    printf("\nT3  UV(방향코사인) ↔ 극좌표 왕복\n");
    for (i = 0; i < 100000; ++i)
    {
        ST_CfPolar  stP;
        ST_CfPolar  stQ;
        ST_CfDirCos stD;
        FLOAT64     dNorm;

        // 보어사이트 반구 안에서만 정의됨 (|Az| < 90도)
        stP.dRange_m = 1.0;
        stP.dAz_rad  = f_URand(-CF_PI / 2.0 + 1e-6, CF_PI / 2.0 - 1e-6);
        stP.dEl_rad  = f_URand(-CF_PI / 2.0 + 1e-6, CF_PI / 2.0 - 1e-6);
        stD = f_CfDirCosFromPolar(stP);
        dNorm = sqrt(stD.dU * stD.dU + stD.dV * stD.dV + stD.dW * stD.dW);
        dWorstN = fmax(dWorstN, fabs(dNorm - 1.0));
        stQ = f_CfPolarFromDirCos(stD, stP.dRange_m);
        dWorstA = fmax(dWorstA, fabs(stQ.dAz_rad - stP.dAz_rad));
        dWorstE = fmax(dWorstE, fabs(stQ.dEl_rad - stP.dEl_rad));
    }
    f_CheckLe("‖(u,v,w)‖ − 1",     dWorstN, 1e-14);
    f_CheckLe("방위각 오차 [rad]", dWorstA, 1e-13);
    f_CheckLe("고각 오차 [rad]",   dWorstE, 1e-13);
}

static VOID f_T4LlaEcefRoundtrip(VOID)
{
    FLOAT64 dWorstH   = 0.0;
    FLOAT64 dWorstPos = 0.0;
    INT32   i;

    printf("\nT4  LLA ↔ ECEF 왕복 (Bowring)\n");
    for (i = 0; i < 200000; ++i)
    {
        ST_CfGeodetic stG;
        ST_CfGeodetic stBack;
        ST_CfVec3     stP;
        ST_CfVec3     stP2;

        stG.dLat_rad = f_URand(CF_DEG2RAD(-89.5), CF_DEG2RAD(89.5));
        stG.dLon_rad = f_URand(-CF_PI, CF_PI);
        stG.dAlt_m   = f_URand(-500.0, 40000.0);
        stP = f_CfEcefFromGeodetic(stG);
        if (f_CfGeodeticFromEcef(stP, &stBack) != enum_CfStatus_Ok)
        {
            ++s_nFail;
            return;
        }
        dWorstH = fmax(dWorstH, fabs(stBack.dAlt_m - stG.dAlt_m));
        stP2 = f_CfEcefFromGeodetic(stBack);        // 위치 오차로 환산
        dWorstPos = fmax(dWorstPos, f_CfVecNorm(f_CfVecSub(stP2, stP)));
    }
    f_CheckLe("고도 오차 [m]",     dWorstH,   1e-6);
    f_CheckLe("위치 왕복 오차 [m]", dWorstPos, 1e-6);
}

static VOID f_T5FullChainRoundtrip(VOID)
{
    FLOAT64 dWorstR = 0.0;
    FLOAT64 dWorstA = 0.0;
    FLOAT64 dWorstE = 0.0;
    INT32   i;

    printf("\nT5  전체 체인 왕복 (측정값 → LLA → 측정값)\n");
    for (i = 0; i < 20000; ++i)
    {
        ST_CfPolar    stMeas;
        ST_CfPolar    stBack;
        ST_CfMount    stMount;
        ST_CfAttitude stAtt;
        ST_CfGeodetic stPlat;
        ST_CfGeodetic stTgt;

        stMeas.dRange_m = f_URand(100.0, 300000.0);
        stMeas.dAz_rad  = f_URand(CF_DEG2RAD(-80.0), CF_DEG2RAD(80.0));
        stMeas.dEl_rad  = f_URand(CF_DEG2RAD(-60.0), CF_DEG2RAD(60.0));

        stMount.dYaw_rad        = f_URand(-CF_PI, CF_PI);
        stMount.dPitch_rad      = f_URand(CF_DEG2RAD(-30.0), CF_DEG2RAD(30.0));
        stMount.stLeverArm_b.dX = f_URand(-60.0, 60.0);
        stMount.stLeverArm_b.dY = f_URand(-20.0, 20.0);
        stMount.stLeverArm_b.dZ = f_URand(-40.0, 10.0);

        stAtt.dRoll_rad  = f_URand(CF_DEG2RAD(-40.0), CF_DEG2RAD(40.0));
        stAtt.dPitch_rad = f_URand(CF_DEG2RAD(-20.0), CF_DEG2RAD(20.0));
        stAtt.dYaw_rad   = f_URand(-CF_PI, CF_PI);

        stPlat.dLat_rad = f_URand(CF_DEG2RAD(-70.0), CF_DEG2RAD(70.0));
        stPlat.dLon_rad = f_URand(-CF_PI, CF_PI);
        stPlat.dAlt_m   = f_URand(0.0, 50.0);

        if (f_CfForwardChain(stMeas, &stMount, &stAtt, stPlat, &stTgt, NULL) != enum_CfStatus_Ok)
        {
            ++s_nFail;
            return;
        }
        if (f_CfInverseChain(stTgt, &stMount, &stAtt, stPlat, &stBack) != enum_CfStatus_Ok)
        {
            ++s_nFail;
            return;
        }

        dWorstR = fmax(dWorstR, fabs(stBack.dRange_m - stMeas.dRange_m));
        dWorstA = fmax(dWorstA, fabs(stBack.dAz_rad  - stMeas.dAz_rad));
        dWorstE = fmax(dWorstE, fabs(stBack.dEl_rad  - stMeas.dEl_rad));
    }
    // 아래 허용치는 알고리즘 오차가 아니라 배정밀도 바닥값.
    // ECEF 가 6.4e6 m 규모라 표현 간격이 1.4e-9 m 이고, 체인을 돌면서 1e-8 m 쯤 쌓임.
    // 각도는 그걸 최단거리 100 m 로 나눈 1e-10 rad 이 한계
    f_CheckLe("거리 오차 [m]",     dWorstR, 1e-6);
    f_CheckLe("방위각 오차 [rad]", dWorstA, 1e-10);
    f_CheckLe("고각 오차 [rad]",   dWorstE, 1e-10);
}

static VOID f_T6KnownAnswer(VOID)
{
    // 예제 입력값에 대한 기대값. NumPy 독립 구현으로 교차 검증한 값임
    const ST_CfPolar    stMeas  = { 20000.0, CF_DEG2RAD(30.0), CF_DEG2RAD(0.0) };
    const ST_CfGeodetic stPlat  = { CF_DEG2RAD(36.408), CF_DEG2RAD(127.307), 0.0 };
    const ST_CfAttitude stAtt   = { 0.0, 0.0, CF_DEG2RAD(45.0) };
    const ST_CfMount    stMount = { CF_DEG2RAD(90.0), 0.0, { -30.0, 0.0, -10.0 } };

    const FLOAT64 dExpLat = 36.233700252;   // deg
    const FLOAT64 dExpLon = 127.364344961;  // deg
    const FLOAT64 dExpAlt = 41.4952;        // m

    ST_CfGeodetic stTgt;
    ST_CfChain    stChain;
    ST_CfPolar    stBack;

    printf("\nT6  기지값 — 예제 입력값\n");
    if (f_CfForwardChain(stMeas, &stMount, &stAtt, stPlat, &stTgt, &stChain) != enum_CfStatus_Ok)
    {
        ++s_nFail;
        return;
    }
    if (f_CfInverseChain(stTgt, &stMount, &stAtt, stPlat, &stBack) != enum_CfStatus_Ok)
    {
        ++s_nFail;
        return;
    }

    f_CheckLe("표적 위도 오차 [deg]", fabs(CF_RAD2DEG(stTgt.dLat_rad) - dExpLat), 1e-8);
    f_CheckLe("표적 경도 오차 [deg]", fabs(CF_RAD2DEG(stTgt.dLon_rad) - dExpLon), 1e-8);
    f_CheckLe("표적 고도 오차 [m]",   fabs(stTgt.dAlt_m - dExpAlt),               1e-3);
    f_CheckLe("역변환 거리 오차 [m]", fabs(stBack.dRange_m - 20000.0),            1e-6);
    f_CheckLe("역변환 방위 오차 [deg]", fabs(CF_RAD2DEG(stBack.dAz_rad) - 30.0),  1e-9);
    f_CheckLe("역변환 고각 오차 [deg]", fabs(CF_RAD2DEG(stBack.dEl_rad) -  0.0),  1e-9);

    // 중간값도 확인. 단계별 서술과 문서가 맞는지 보는 용도
    f_CheckLe("p_ant.x 기대 +17320.5081", fabs(stChain.stAntPos.dX - 17320.5080757), 1e-6);
    f_CheckLe("p_body.x 기대 −10030.0000", fabs(stChain.stBodyPos.dX - (-10030.0)),  1e-6);
    f_CheckLe("p_ned.N  기대 −19339.7297", fabs(stChain.stNedPos.dX - (-19339.72971)), 1e-4);
    f_CheckLe("진북방위 기대 165.074374°",
              fabs(CF_RAD2DEG(stChain.dTrueBearing_rad) - 165.074374), 1e-5);
}

static VOID f_T7Singularities(VOID)
{
    const FLOAT64 daLats[4] = { 90.0, -90.0, 89.99999, 0.0 };
    ST_CfGeodetic stG;
    ST_CfGeodetic stBack;
    ST_CfVec3     stP;
    ST_CfPolar    stPol;
    ST_CfPolar    stQ;
    FLOAT64       dWorst = 0.0;
    INT32         i;

    printf("\nT7  특이점 처리 (NaN / Inf 없음)\n");

    for (i = 0; i < 4; ++i)     // 극점 부근
    {
        stG.dLat_rad = CF_DEG2RAD(daLats[i]);
        stG.dLon_rad = CF_DEG2RAD(123.0);
        stG.dAlt_m   = 100.0;
        stP = f_CfEcefFromGeodetic(stG);
        if (f_CfGeodeticFromEcef(stP, &stBack) != enum_CfStatus_Ok)
        {
            ++s_nFail;
            return;
        }
        if (isnan(stBack.dLat_rad) || isnan(stBack.dAlt_m))
        {
            ++s_nFail;
            return;
        }
        dWorst = fmax(dWorst, fabs(stBack.dAlt_m - stG.dAlt_m));
    }
    f_CheckLe("극점 부근 고도 오차 [m]", dWorst, 1e-4);

    // 보어사이트 정면 / 천정 / 천저
    dWorst = 0.0;
    {
        const FLOAT64 daEls[3] = { 0.0, 90.0, -90.0 };
        for (i = 0; i < 3; ++i)
        {
            stPol.dRange_m = 1000.0;
            stPol.dAz_rad  = 0.0;
            stPol.dEl_rad  = CF_DEG2RAD(daEls[i]);
            stQ = f_CfPolarFromAntCart(f_CfAntCartFromPolar(stPol));
            if (isnan(stQ.dAz_rad) || isnan(stQ.dEl_rad))
            {
                ++s_nFail;
                return;
            }
            dWorst = fmax(dWorst, fabs(stQ.dEl_rad - stPol.dEl_rad));
        }
    }
    f_CheckLe("El = 0/±90° 왕복 오차 [rad]", dWorst, 1e-12);

    // 거리 0. 방향이 정의되지 않아도 NaN 은 나오면 안 됨
    {
        ST_CfVec3 stZero = { 0.0, 0.0, 0.0 };
        stQ = f_CfPolarFromAntCart(stZero);
        f_Check("거리 0 에서 NaN 없음",
                (!isnan(stQ.dRange_m) && !isnan(stQ.dAz_rad) && !isnan(stQ.dEl_rad)), 0.0, 0.0);
    }

    // 경도 ±180 경계
    {
        ST_CfGeodetic stA = { CF_DEG2RAD(10.0), CF_DEG2RAD(179.999999), 0.0 };
        ST_CfGeodetic stB;
        if (f_CfGeodeticFromEcef(f_CfEcefFromGeodetic(stA), &stB) != enum_CfStatus_Ok)
        {
            ++s_nFail;
            return;
        }
        f_CheckLe("경도 ±180° 경계 오차 [deg]",
                  fabs(CF_RAD2DEG(stB.dLon_rad) - 179.999999), 1e-9);
    }
}

// 전치 방향이 뒤바뀐 버그는 대칭적인 자세에서는 안 드러나서 roll/pitch/yaw 를 전부 다르게 둠
static VOID f_T8AsymmetricAttitude(VOID)
{
    const ST_CfAttitude stAtt = { CF_DEG2RAD(17.0), CF_DEG2RAD(-9.0), CF_DEG2RAD(123.0) };
    const ST_CfVec3     stV   = { 1234.5, -678.9, 246.8 };
    ST_CfVec3 stFwd;
    ST_CfVec3 stRt;
    FLOAT64   dDiff;
    FLOAT64   dIdent;

    printf("\nT8  비대칭 자세 — 전치 방향 검출\n");
    stFwd = f_CfNedFromBody(&stAtt, stV);
    stRt  = f_CfBodyFromNed(&stAtt, stFwd);

    dIdent = f_CfVecNorm(f_CfVecSub(stRt, stV));
    f_CheckLe("C 적용 후 Cᵀ 적용 = 원본 [m]", dIdent, 1e-9);

    // 정방향과 역방향이 달라야 정상. 같으면 전치가 아무 일도 안 하고 있는 것
    dDiff = f_CfVecNorm(f_CfVecSub(stFwd, f_CfBodyFromNed(&stAtt, stV)));
    f_Check("정변환 ≠ 역변환 (구분됨)", dDiff > 1.0, dDiff, 1.0);
}

// 부호를 뒤집으면 결과도 달라져야 함. 같다면 그 항이 계산에 안 쓰이고 있다는 뜻
static VOID f_T9SignSensitivity(VOID)
{
    const ST_CfPolar    stMeas  = { 20000.0, CF_DEG2RAD(30.0), CF_DEG2RAD(12.0) };
    const ST_CfGeodetic stPlat  = { CF_DEG2RAD(36.408), CF_DEG2RAD(127.307), 0.0 };
    ST_CfAttitude stAtt   = { CF_DEG2RAD(10.0), CF_DEG2RAD(5.0), CF_DEG2RAD(45.0) };
    ST_CfMount    stMount = { CF_DEG2RAD(90.0), 0.0, { -30.0, 0.0, -10.0 } };
    ST_CfGeodetic stBase;
    ST_CfGeodetic stFlipped;

    printf("\nT9  부호 민감도\n");
    if (f_CfForwardChain(stMeas, &stMount, &stAtt, stPlat, &stBase, NULL) != enum_CfStatus_Ok)
    {
        ++s_nFail;
        return;
    }

    stAtt.dRoll_rad = -stAtt.dRoll_rad;
    if (f_CfForwardChain(stMeas, &stMount, &stAtt, stPlat, &stFlipped, NULL) != enum_CfStatus_Ok)
    {
        ++s_nFail;
        return;
    }
    f_Check("roll 부호 반전 → 결과 변함",
            fabs(stFlipped.dAlt_m - stBase.dAlt_m) > 1.0,
            fabs(stFlipped.dAlt_m - stBase.dAlt_m), 1.0);
    stAtt.dRoll_rad = -stAtt.dRoll_rad;

    stMount.stLeverArm_b.dZ = -stMount.stLeverArm_b.dZ;
    if (f_CfForwardChain(stMeas, &stMount, &stAtt, stPlat, &stFlipped, NULL) != enum_CfStatus_Ok)
    {
        ++s_nFail;
        return;
    }
    f_Check("레버암 z 부호 반전 → 결과 변함",
            fabs(stFlipped.dAlt_m - stBase.dAlt_m) > 1.0,
            fabs(stFlipped.dAlt_m - stBase.dAlt_m), 1.0);
}

INT32 main(VOID)
{
    f_ConsoleUseUtf8();
    srand(20260901u);
    printf("════════════════════════════════════════════════════════════════════\n");
    printf(" coord_frames 단위 시험\n");
    printf("════════════════════════════════════════════════════════════════════\n");

    f_T1MatrixProperties();
    f_T2PolarCartRoundtrip();
    f_T3UvRoundtrip();
    f_T4LlaEcefRoundtrip();
    f_T5FullChainRoundtrip();
    f_T6KnownAnswer();
    f_T7Singularities();
    f_T8AsymmetricAttitude();
    f_T9SignSensitivity();

    printf("\n════════════════════════════════════════════════════════════════════\n");
    printf(" 결과 : 통과 %d / 실패 %d\n", s_nPass, s_nFail);
    printf("════════════════════════════════════════════════════════════════════\n");
    return (s_nFail == 0) ? 0 : 1;
}
