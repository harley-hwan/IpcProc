//
// @file	main.c
// @brief	Chapter 2 / 3.1 예제 입력값으로 좌표변환을 돌려보는 실행 파일.
//			입력은 측정값(R 20 km, Az 30, El 0), 플랫폼 36.408N 127.307E, 자세 yaw 45,
//			안테나는 선수 기준 90 도에 무게중심에서 30 m 뒤 / 10 m 위.
//			출력은 표적 LLA 와, 그걸 되돌린 안테나 극좌표 두 가지.
//			인자를 주면 기본값을 덮어쓸 수 있음. -h 로 확인.
// @author	hwan
// @date	2026.09.02.
//
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

#include "coord_frames.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// 한국어 Windows 콘솔은 CP949 라서 UTF-8 한글이 그대로 나가면 깨짐. 시작할 때 한 번만 바꿔줌
static VOID f_ConsoleUseUtf8(VOID)
{
#ifdef _WIN32
    (VOID)SetConsoleOutputCP(CP_UTF8);
#endif
}

static VOID f_PrintRule(const CHAR *cpTitle)
{
    printf("\n────────────────────────────────────────────────────────────────────\n");
    if (cpTitle != NULL)
    {
        printf(" %s\n", cpTitle);
    }
    printf("────────────────────────────────────────────────────────────────────\n");
}

static VOID f_PrintVec(const CHAR *cpLabel, const ST_CfVec3 stV, const CHAR *cpUnit)
{
    printf("  %-26s %+18.4f %+18.4f %+18.4f  [%s]\n", cpLabel, stV.dX, stV.dY, stV.dZ, cpUnit);
}

static VOID f_PrintMat(const CHAR *cpLabel, const ST_CfMat3 stA)
{
    INT32 i;

    printf("  %s\n", cpLabel);
    for (i = 0; i < 3; ++i)
    {
        printf("      [ %+13.9f  %+13.9f  %+13.9f ]\n",
               stA.daM[i][0], stA.daM[i][1], stA.daM[i][2]);
    }
    printf("      det = %+.15f,   |A^T·A − I|max = %.3e\n",
           f_CfMatDet(stA), f_CfMatOrthoError(stA));
}

// 옵션이 몇 개의 값을 받는지. 모르는 옵션이면 -1.
// 값 개수보다 아는 옵션인지를 먼저 봐야 오타를 "값이 부족하다"로 잘못 안내하지 않음
static INT32 f_OptNArgs(const CHAR *cpOpt)
{
    static const CHAR *cpaOne[] = {
        "-r","--range","-a","--az","-e","--el",
        "--lat","--lon","--alt","--roll","--pitch","--yaw",
        "--mount-az","--mount-el", NULL
    };
    INT32 i;

    for (i = 0; cpaOne[i] != NULL; ++i)
    {
        if (strcmp(cpOpt, cpaOne[i]) == 0)
        {
            return 1;
        }
    }
    if (strcmp(cpOpt, "--lever") == 0)
    {
        return 3;
    }
    return -1;
}

static VOID f_Usage(const CHAR *cpProg)
{
    printf("사용법: %s [옵션]...\n", cpProg);
    printf("  인자를 주지 않으면 문서에 실린 기본 예제값으로 실행합니다.\n\n");
    printf("  측정값 (안테나 기준)\n");
    printf("    -r, --range <m>       시선거리      [기본 20000]\n");
    printf("    -a, --az    <deg>     방위각        [기본 30]\n");
    printf("    -e, --el    <deg>     고각          [기본 0]\n\n");
    printf("  플랫폼 위치·자세\n");
    printf("    --lat <deg>           위도          [기본 36.408]\n");
    printf("    --lon <deg>           경도          [기본 127.307]\n");
    printf("    --alt <m>             타원체고      [기본 0]\n");
    printf("    --roll  <deg>         횡동요        [기본 0]\n");
    printf("    --pitch <deg>         종동요        [기본 0]\n");
    printf("    --yaw   <deg>         선수방위      [기본 45]\n\n");
    printf("  안테나 설치\n");
    printf("    --mount-az <deg>      설치 방위     [기본 90]\n");
    printf("    --mount-el <deg>      백틸트        [기본 0]\n");
    printf("    --lever <x> <y> <z>   레버암 [m], 동체 FRD [기본 -30 0 -10]\n\n");
    printf("  예)\n");
    printf("    %s                          기본 예제값\n", cpProg);
    printf("    %s -r 50000 -a 15 -e 3      측정값만 변경\n", cpProg);
    printf("    %s --roll 20                배가 20도 기울었을 때\n", cpProg);
    printf("    %s --lever 0 0 0            레버암을 무시했을 때\n", cpProg);
}

INT32 main(INT32 argc, CHAR **argv)
{
    // 기본 입력값. 명령행 인자로 덮어쓸 수 있음
    ST_CfPolar    stMeas     = { 20000.0, CF_DEG2RAD(30.0), CF_DEG2RAD(0.0) };
    ST_CfGeodetic stPlatform = { CF_DEG2RAD(36.408), CF_DEG2RAD(127.307), 0.0 };
    ST_CfAttitude stAttitude = { CF_DEG2RAD(0.0), CF_DEG2RAD(0.0), CF_DEG2RAD(45.0) };

    // 조건2: 선수 기준 시계방향 90도, 무게중심에서 30 m 뒤 / 10 m 위.
    // 동체 FRD 기준이라 뒤 = x 음수, 위 = z 음수
    ST_CfMount    stMount    = { CF_DEG2RAD(90.0), CF_DEG2RAD(0.0), { -30.0, 0.0, -10.0 } };

    ST_CfGeodetic stTarget;
    ST_CfChain    stChain;
    ST_CfPolar    stBack;
    INT32         iStatus;
    INT32         i;

    f_ConsoleUseUtf8();

    for (i = 1; i < argc; ++i)
    {
        const CHAR *cpOpt = argv[i];
        INT32       nNeed;

        if ((strcmp(cpOpt, "-h") == 0) || (strcmp(cpOpt, "--help") == 0))
        {
            f_Usage(argv[0]);
            return 0;
        }

        nNeed = f_OptNArgs(cpOpt);
        if (nNeed < 0)
        {
            fprintf(stderr, "오류: 알 수 없는 옵션 '%s'\n\n", cpOpt);
            f_Usage(argv[0]);
            return 2;
        }
        if (i + nNeed >= argc)
        {
            fprintf(stderr, "오류: %s 옵션에 값이 %d개 필요합니다.\n\n", cpOpt, nNeed);
            f_Usage(argv[0]);
            return 2;
        }

        if      (strcmp(cpOpt,"-r")==0 || strcmp(cpOpt,"--range")==0)  stMeas.dRange_m     = atof(argv[++i]);
        else if (strcmp(cpOpt,"-a")==0 || strcmp(cpOpt,"--az")==0)     stMeas.dAz_rad      = CF_DEG2RAD(atof(argv[++i]));
        else if (strcmp(cpOpt,"-e")==0 || strcmp(cpOpt,"--el")==0)     stMeas.dEl_rad      = CF_DEG2RAD(atof(argv[++i]));
        else if (strcmp(cpOpt,"--lat")==0)      stPlatform.dLat_rad    = CF_DEG2RAD(atof(argv[++i]));
        else if (strcmp(cpOpt,"--lon")==0)      stPlatform.dLon_rad    = CF_DEG2RAD(atof(argv[++i]));
        else if (strcmp(cpOpt,"--alt")==0)      stPlatform.dAlt_m      = atof(argv[++i]);
        else if (strcmp(cpOpt,"--roll")==0)     stAttitude.dRoll_rad   = CF_DEG2RAD(atof(argv[++i]));
        else if (strcmp(cpOpt,"--pitch")==0)    stAttitude.dPitch_rad  = CF_DEG2RAD(atof(argv[++i]));
        else if (strcmp(cpOpt,"--yaw")==0)      stAttitude.dYaw_rad    = CF_DEG2RAD(atof(argv[++i]));
        else if (strcmp(cpOpt,"--mount-az")==0) stMount.dYaw_rad       = CF_DEG2RAD(atof(argv[++i]));
        else if (strcmp(cpOpt,"--mount-el")==0) stMount.dPitch_rad     = CF_DEG2RAD(atof(argv[++i]));
        else if (strcmp(cpOpt,"--lever")==0)
        {
            stMount.stLeverArm_b.dX = atof(argv[++i]);
            stMount.stLeverArm_b.dY = atof(argv[++i]);
            stMount.stLeverArm_b.dZ = atof(argv[++i]);
        }
    }

    if (stMeas.dRange_m <= 0.0)
    {
        fprintf(stderr, "오류: 거리(-r)는 0보다 커야 합니다.\n");
        return 2;
    }

    f_PrintRule("입력 조건");
    printf("  신호처리 측정값   R = %.1f m,  Az = %.3f deg,  El = %.3f deg\n",
           stMeas.dRange_m, CF_RAD2DEG(stMeas.dAz_rad), CF_RAD2DEG(stMeas.dEl_rad));
    printf("  플랫폼 위치       lat = %.6f deg,  lon = %.6f deg,  alt = %.3f m\n",
           CF_RAD2DEG(stPlatform.dLat_rad), CF_RAD2DEG(stPlatform.dLon_rad), stPlatform.dAlt_m);
    printf("  플랫폼 자세       roll = %.3f,  pitch = %.3f,  yaw = %.3f  [deg]\n",
           CF_RAD2DEG(stAttitude.dRoll_rad), CF_RAD2DEG(stAttitude.dPitch_rad),
           CF_RAD2DEG(stAttitude.dYaw_rad));
    printf("  안테나 설치       방위 = %.3f deg,  백틸트 = %.3f deg\n",
           CF_RAD2DEG(stMount.dYaw_rad), CF_RAD2DEG(stMount.dPitch_rad));
    printf("  레버암(FRD)       (%+.1f, %+.1f, %+.1f) m   = 30 m 뒤, 10 m 위\n",
           stMount.stLeverArm_b.dX, stMount.stLeverArm_b.dY, stMount.stLeverArm_b.dZ);

    iStatus = f_CfForwardChain(stMeas, &stMount, &stAttitude, stPlatform, &stTarget, &stChain);
    if (iStatus != enum_CfStatus_Ok)
    {
        fprintf(stderr, "정변환 실패: status = %d\n", iStatus);
        return 1;
    }

    f_PrintRule("1단계  안테나 극좌표 → 안테나 직교좌표 / UV");
    printf("  방향코사인(UV)             u = %+.12f  (면 가로·오른쪽)\n", stChain.stDirCos.dU);
    printf("                             v = %+.12f  (면 세로·위)\n",     stChain.stDirCos.dV);
    printf("                             w = %+.12f  (보어사이트)\n",     stChain.stDirCos.dW);
    printf("  ‖(u,v,w)‖ − 1              %+.3e\n",
           sqrt(stChain.stDirCos.dU * stChain.stDirCos.dU
              + stChain.stDirCos.dV * stChain.stDirCos.dV
              + stChain.stDirCos.dW * stChain.stDirCos.dW) - 1.0);
    f_PrintVec("안테나 직교 p_ant", stChain.stAntPos, "m");
    printf("      x = 보어사이트 방향,  y = 오른쪽,  z = 아래\n");

    f_PrintRule("2단계  안테나 좌표계 → 동체 좌표계  (회전 + 평행이동)");
    f_PrintMat("C_body_from_antenna = Rz(90°)·Ry(0°)", stChain.stCBodyFromAnt);
    f_PrintVec("회전만 적용", f_CfMatApply(stChain.stCBodyFromAnt, stChain.stAntPos), "m");
    f_PrintVec("+ 레버암 → p_body", stChain.stBodyPos, "m");
    printf("      선수 기준 방위 %.6f deg,  수평거리 %.4f m\n",
           fmod(CF_RAD2DEG(atan2(stChain.stBodyPos.dY, stChain.stBodyPos.dX)) + 360.0, 360.0),
           sqrt(stChain.stBodyPos.dX * stChain.stBodyPos.dX
              + stChain.stBodyPos.dY * stChain.stBodyPos.dY));

    f_PrintRule("3단계  동체 좌표계 → 로컬(NED) 좌표계  (회전)");
    f_PrintMat("C_ned_from_body = Rz(45°)·Ry(0°)·Rx(0°)", stChain.stCNedFromBody);
    f_PrintVec("p_ned  (N, E, D)", stChain.stNedPos, "m");
    printf("      진북 기준 방위 %.6f deg,  수평거리 %.4f m,  평면고도 %.4f m\n",
           CF_RAD2DEG(stChain.dTrueBearing_rad), stChain.dGroundRange_m,
           stPlatform.dAlt_m - stChain.stNedPos.dZ);

    f_PrintRule("4단계  로컬(NED) → ECEF");
    f_PrintMat("C_ned_from_ecef(lat, lon)",
               f_CfDcmNedFromEcef(stPlatform.dLat_rad, stPlatform.dLon_rad));
    f_PrintVec("플랫폼 ECEF", stChain.stEcefPlatform, "m");
    f_PrintVec("표적   ECEF", stChain.stEcefTarget,   "m");

    f_PrintRule("5단계  ECEF → LLA (Bowring)");
    printf("  표적 위도                  %.9f deg\n", CF_RAD2DEG(stTarget.dLat_rad));
    printf("  표적 경도                  %.9f deg\n", CF_RAD2DEG(stTarget.dLon_rad));
    printf("  표적 고도(타원체고)        %.4f m\n",    stTarget.dAlt_m);
    printf("  로컬 평면 기준 고도        %.4f m   (차이 %.1f m = 지구 곡률)\n",
           stPlatform.dAlt_m - stChain.stNedPos.dZ,
           stTarget.dAlt_m - (stPlatform.dAlt_m - stChain.stNedPos.dZ));

    iStatus = f_CfInverseChain(stTarget, &stMount, &stAttitude, stPlatform, &stBack);
    if (iStatus != enum_CfStatus_Ok)
    {
        fprintf(stderr, "역변환 실패: status = %d\n", iStatus);
        return 1;
    }

    f_PrintRule("역변환  LLA → ECEF → 로컬 → 동체 → 안테나 극좌표");
    printf("  시선거리 R                 %.6f m\n",  stBack.dRange_m);
    printf("  방위각   Az                %.9f deg\n", CF_RAD2DEG(stBack.dAz_rad));
    printf("  고각     El                %.9f deg\n", CF_RAD2DEG(stBack.dEl_rad));
    printf("  왕복오차                   ΔR = %.3e m,  ΔAz = %.3e deg,  ΔEl = %.3e deg\n",
           fabs(stBack.dRange_m - stMeas.dRange_m),
           fabs(CF_RAD2DEG(stBack.dAz_rad - stMeas.dAz_rad)),
           fabs(CF_RAD2DEG(stBack.dEl_rad - stMeas.dEl_rad)));

    f_PrintRule("최종 출력  —  통제제어 전달 값");
    printf("  [1] 위/경/고도 좌표계\n");
    printf("        위도  %.9f deg\n", CF_RAD2DEG(stTarget.dLat_rad));
    printf("        경도  %.9f deg\n", CF_RAD2DEG(stTarget.dLon_rad));
    printf("        고도  %.4f m  (WGS-84 타원체고)\n", stTarget.dAlt_m);
    printf("  [2] 안테나 좌표계 (극좌표)\n");
    printf("        시선거리  %.4f m\n",   stBack.dRange_m);
    printf("        방위각    %.6f deg\n", CF_RAD2DEG(stBack.dAz_rad));
    printf("        고각      %.6f deg\n", CF_RAD2DEG(stBack.dEl_rad));
    printf("\n");
    return 0;
}
