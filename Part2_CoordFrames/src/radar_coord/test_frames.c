//
// @file	test_frames.c
// @brief	coord_frames 확인용. 단계마다 갔다가 돌아오면 원래 값이 나오는지 보고,
//			과제 예제값은 손으로 계산한 값과 맞는지 봄. 부호나 전치 방향이 하나만 틀려도 왕복에서 걸림.
//			radar_coord 를 "test" 인자로 실행하면 돌아감
// @author	hwan
// @date	2026.09.02.
//
#include <stdio.h>
#include <math.h>
#include "test_frames.h"

static INT32 s_nTotal = 0;
static INT32 s_nPass  = 0;

static VOID f_Check(const CHAR *cpName, const FLOAT64 dErr, const FLOAT64 dTol)
{
    s_nTotal++;
    if (dErr <= dTol)
    {
        s_nPass++;
        printf("  OK    %s  (%.1e)\n", cpName, dErr);
    }
    else
    {
        printf("  FAIL  %s  (%.1e > %.0e)\n", cpName, dErr, dTol);
    }
}

static FLOAT64 f_Dist(const ST_Vec3 stA, const ST_Vec3 stB)
{
    FLOAT64 dDx = stA.dX - stB.dX;
    FLOAT64 dDy = stA.dY - stB.dY;
    FLOAT64 dDz = stA.dZ - stB.dZ;

    return sqrt(dDx * dDx + dDy * dDy + dDz * dDz);
}

// 극좌표 -> 직교 -> 극좌표 -> 직교. 방위각 / 고각 몇 개 골라서 돌려봄
static VOID f_TestPolar(VOID)
{
    FLOAT64 dWorst = 0.0;
    INT32   iAz;
    INT32   iEl;

    for (iAz = -60; iAz <= 60; iAz += 30)
    {
        for (iEl = -30; iEl <= 60; iEl += 30)
        {
            ST_Polar stP = { 1000.0, DEG2RAD(iAz), DEG2RAD(iEl) };
            ST_Vec3  stV = f_PolarToXyz(stP);
            FLOAT64  dErr = f_Dist(f_PolarToXyz(f_XyzToPolar(stV)), stV);

            if (dErr > dWorst)
            {
                dWorst = dErr;
            }
        }
    }
    f_Check("극좌표 <-> 직교 왕복 [m]", dWorst, 1e-9);
}

// 안테나 -> 동체 -> 안테나. 설치 고각도 넣어서 Ry 까지 같이 봄
static VOID f_TestAntBody(VOID)
{
    ST_Mount stMount = { DEG2RAD(90.0), DEG2RAD(5.0), { -30.0, 0.0, -10.0 } };
    ST_Vec3  stV     = { 17320.5, 10000.0, -1234.5 };
    ST_Vec3  stBack  = f_BodyToAnt(stMount, f_AntToBody(stMount, stV));

    f_Check("안테나 <-> 동체 왕복 [m]", f_Dist(stBack, stV), 1e-9);
}

// 동체 -> NED -> 동체. roll / pitch / yaw 전부 다른 값으로. 대칭이면 전치 방향이 틀려도 통과해버림
static VOID f_TestBodyNed(VOID)
{
    ST_Attitude stAtt  = { DEG2RAD(17.0), DEG2RAD(-9.0), DEG2RAD(123.0) };
    ST_Vec3     stV    = { 1234.5, -678.9, 246.8 };
    ST_Vec3     stBack = f_NedToBody(stAtt, f_BodyToNed(stAtt, stV));

    f_Check("동체 <-> NED 왕복 [m]", f_Dist(stBack, stV), 1e-9);
}

// LLA -> ECEF -> LLA -> ECEF. 위도 / 경도 / 고도 격자로 돌려봄
static VOID f_TestLlaEcef(VOID)
{
    FLOAT64 dWorst = 0.0;
    INT32   iLat;
    INT32   iLon;
    INT32   iAlt;

    for (iLat = -80; iLat <= 80; iLat += 20)
    {
        for (iLon = -180; iLon <= 180; iLon += 60)
        {
            for (iAlt = 0; iAlt <= 10000; iAlt += 5000)
            {
                ST_Lla  stG   = { DEG2RAD(iLat), DEG2RAD(iLon), (FLOAT64)iAlt };
                ST_Vec3 stE   = f_LlaToEcef(stG);
                FLOAT64 dErr  = f_Dist(f_LlaToEcef(f_EcefToLla(stE)), stE);

                if (dErr > dWorst)
                {
                    dWorst = dErr;
                }
            }
        }
    }
    f_Check("LLA <-> ECEF 왕복 [m]", dWorst, 1e-6);
}

// ECEF -> NED -> ECEF. 원점은 과제 함선 위치
static VOID f_TestNedEcef(VOID)
{
    ST_Lla  stShip  = { DEG2RAD(36.408), DEG2RAD(127.307), 0.0 };
    ST_Vec3 stV     = { -3125892.0, 4093775.0, 3749164.0 };
    ST_Vec3 stBack  = f_NedToEcef(stShip, f_EcefToNed(stShip, stV));

    f_Check("NED <-> ECEF 왕복 [m]", f_Dist(stBack, stV), 1e-6);
}

// 과제 예제값. 동체 좌표는 손으로 계산 가능 (Rz(90) 돌리고 오프셋 더하면 끝)
static VOID f_TestExample(VOID)
{
    ST_Polar    stMeas  = { 20000.0, DEG2RAD(30.0), DEG2RAD(0.0) };
    ST_Lla      stShip  = { DEG2RAD(36.408), DEG2RAD(127.307), 0.0 };
    ST_Attitude stAtt   = { DEG2RAD(0.0), DEG2RAD(0.0), DEG2RAD(45.0) };
    ST_Mount    stMount = { DEG2RAD(90.0), DEG2RAD(0.0), { -30.0, 0.0, -10.0 } };
    ST_Vec3     stBodyExp = { -10030.0, 17320.5080757, -10.0 };

    ST_Vec3     stBody;
    ST_Vec3     stEcef;
    ST_Lla      stTgt;
    ST_Polar    stBack;

    stBody = f_AntToBody(stMount, f_PolarToXyz(stMeas));
    f_Check("예제값 동체 좌표 [m]", f_Dist(stBody, stBodyExp), 1e-6);

    stEcef = f_NedToEcef(stShip, f_BodyToNed(stAtt, stBody));
    stTgt  = f_EcefToLla(stEcef);
    f_Check("예제값 표적 위도 [deg]", fabs(RAD2DEG(stTgt.dLat) - 36.233700252), 1e-8);
    f_Check("예제값 표적 경도 [deg]", fabs(RAD2DEG(stTgt.dLon) - 127.364344961), 1e-8);
    f_Check("예제값 표적 고도 [m]",   fabs(stTgt.dAlt - 41.4952), 1e-3);

    // 되돌리면 입력값이 나와야 함
    stBack = f_XyzToPolar(f_BodyToAnt(stMount,
                f_NedToBody(stAtt, f_EcefToNed(stShip, f_LlaToEcef(stTgt)))));
    f_Check("예제값 왕복 거리 [m]",     fabs(stBack.dRange - stMeas.dRange), 1e-6);
    f_Check("예제값 왕복 방위각 [deg]", fabs(RAD2DEG(stBack.dAz - stMeas.dAz)), 1e-9);
    f_Check("예제값 왕복 고각 [deg]",   fabs(RAD2DEG(stBack.dEl - stMeas.dEl)), 1e-9);
}

INT32 f_RunTests(VOID)
{
    printf("coord_frames 테스트\n\n");

    f_TestPolar();
    f_TestAntBody();
    f_TestBodyNed();
    f_TestLlaEcef();
    f_TestNedEcef();
    f_TestExample();

    printf("\n%d 개 중 %d 개 통과\n", s_nTotal, s_nPass);
    return (s_nPass == s_nTotal) ? 0 : 1;
}
