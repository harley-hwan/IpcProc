//
// @file	main.c
// @brief	과제 3.1. 신호처리 입력값을 넣고 데이터처리 최종 출력값을 확인.
//			입력  : R 20 km, Az 30 deg, El 0 deg
//			플랫폼: 36.408 N, 127.307 E, 고도 0 m, 자세각 (r, y, p) = (0, 45, 0) deg
//			안테나: 선수 기준 시계방향 90 deg, 무게중심에서 30 m 뒤, 10 m 위
//			출력  : 표적 위/경/고도, 표적 안테나 기준 R / Az / El
//			"test" 인자를 주면 예제 대신 시험 12항목을 돌림 (test_frames.c)
// @author	hwan
// @date	2026.09.02.
//
#ifdef _WIN32
#include <windows.h>    // 한글 콘솔 출력용. coord_frames.h 보다 먼저 include 해야 함
#endif
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "coord_frames.h"
#include "test_frames.h"

static VOID f_PrintVec(const CHAR *cpLabel, const ST_Vec3 stV)
{
    printf("  %s\n", cpLabel);
    printf("      %16.4f %16.4f %16.4f  [m]\n", stV.dX, stV.dY, stV.dZ);
}

INT32 main(INT32 argc, CHAR **argv)
{
    // 과제 조건. 안테나 설치 위치는 동체 기준이라 30 m 뒤 = x -30, 10 m 위 = z -10
    ST_Polar    stMeas  = { 20000.0, DEG2RAD(30.0), DEG2RAD(0.0) };
    ST_Lla      stShip  = { DEG2RAD(36.408), DEG2RAD(127.307), 0.0 };
    ST_Attitude stAtt   = { DEG2RAD(0.0), DEG2RAD(0.0), DEG2RAD(45.0) };     // roll, pitch, yaw
    ST_Mount    stMount = { DEG2RAD(90.0), DEG2RAD(0.0), { -30.0, 0.0, -10.0 } };

    ST_Vec3     stAnt;
    ST_Vec3     stBody;
    ST_Vec3     stNed;
    ST_Vec3     stEcef;
    ST_Lla      stTarget;
    ST_Polar    stBack;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    if ((argc > 1) && (strcmp(argv[1], "test") == 0))
    {
        return f_RunTests();
    }

    printf("[입력]\n");
    printf("  측정값       R = %.1f m, Az = %.3f deg, El = %.3f deg\n",
           stMeas.dRange, RAD2DEG(stMeas.dAz), RAD2DEG(stMeas.dEl));
    printf("  플랫폼 위치  lat = %.6f deg, lon = %.6f deg, alt = %.1f m\n",
           RAD2DEG(stShip.dLat), RAD2DEG(stShip.dLon), stShip.dAlt);
    printf("  플랫폼 자세  (r, y, p) = (%.3f, %.3f, %.3f) deg\n",
           RAD2DEG(stAtt.dRoll), RAD2DEG(stAtt.dYaw), RAD2DEG(stAtt.dPitch));
    printf("  안테나 설치  az = %.3f deg, tilt = %.3f deg, offset = (%.1f, %.1f, %.1f) m\n",
           RAD2DEG(stMount.dAz), RAD2DEG(stMount.dTilt),
           stMount.stOffset.dX, stMount.stOffset.dY, stMount.stOffset.dZ);

    // 정변환. 안테나에서 출발해서 한 단계씩 밖으로
    printf("\n[정변환]\n");

    stAnt = f_PolarToXyz(stMeas);
    f_PrintVec("1) 극좌표 -> 안테나 직교 (x, y, z)", stAnt);

    stBody = f_AntToBody(stMount, stAnt);
    f_PrintVec("2) 안테나 -> 동체 (x, y, z)", stBody);

    stNed = f_BodyToNed(stAtt, stBody);
    f_PrintVec("3) 동체 -> NED (N, E, D)", stNed);

    stEcef = f_NedToEcef(stShip, stNed);
    f_PrintVec("4) NED -> ECEF (X, Y, Z)", stEcef);
    f_PrintVec("   함선 ECEF (X, Y, Z)", f_LlaToEcef(stShip));

    stTarget = f_EcefToLla(stEcef);
    printf("  5) ECEF -> LLA\n");
    printf("      lat = %.9f deg, lon = %.9f deg, alt = %.4f m\n",
           RAD2DEG(stTarget.dLat), RAD2DEG(stTarget.dLon), stTarget.dAlt);

    // 역변환. 표적 LLA 에서 다시 안테나까지. 입력값이 그대로 나와야 정상
    printf("\n[역변환]\n");

    stEcef = f_LlaToEcef(stTarget);
    f_PrintVec("1) LLA -> ECEF (X, Y, Z)", stEcef);

    stNed = f_EcefToNed(stShip, stEcef);
    f_PrintVec("2) ECEF -> NED (N, E, D)", stNed);

    stBody = f_NedToBody(stAtt, stNed);
    f_PrintVec("3) NED -> 동체 (x, y, z)", stBody);

    stAnt = f_BodyToAnt(stMount, stBody);
    f_PrintVec("4) 동체 -> 안테나 직교 (x, y, z)", stAnt);

    stBack = f_XyzToPolar(stAnt);
    printf("  5) 안테나 직교 -> 극좌표\n");
    printf("      R = %.6f m, Az = %.9f deg, El = %.9f deg\n",
           stBack.dRange, RAD2DEG(stBack.dAz), RAD2DEG(stBack.dEl));
    printf("      입력과 차이  dR = %.1e m, dAz = %.1e deg, dEl = %.1e deg\n",
           fabs(stBack.dRange - stMeas.dRange),
           fabs(RAD2DEG(stBack.dAz - stMeas.dAz)),
           fabs(RAD2DEG(stBack.dEl - stMeas.dEl)));

    printf("\n[최종 출력]\n");
    printf("  1) 표적 위/경/고도     lat = %.6f deg, lon = %.6f deg, alt = %.1f m\n",
           RAD2DEG(stTarget.dLat), RAD2DEG(stTarget.dLon), stTarget.dAlt);
    printf("  2) 표적 안테나 극좌표  R = %.1f m, Az = %.3f deg, El = %.3f deg\n",
           stBack.dRange, RAD2DEG(stBack.dAz), RAD2DEG(stBack.dEl));
    printf("\n");
    return 0;
}
