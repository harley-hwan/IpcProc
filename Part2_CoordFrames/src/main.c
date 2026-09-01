/*==============================================================================
 *  main.c  —  Chapter 2 / 3.1  예제 입력값에 대한 좌표변환 실행
 *------------------------------------------------------------------------------
 *  [ 입력 ]
 *    신호처리 측정값 : Slant Range 20 km, Azimuth 30 deg, Elevation 0 deg
 *    플랫폼 위치     : 36.408 N, 127.307 E, 0 m
 *    플랫폼 자세     : roll 0 deg, yaw 45 deg, pitch 0 deg
 *    안테나 설치     : 선수 기준 시계방향 90 deg, 무게중심에서 30 m 뒤 / 10 m 위
 *
 *  [ 출력 ]
 *    (1) 표적 위도 / 경도 / 고도
 *    (2) 표적 안테나 기준 시선거리 / 방위각 / 고각
 *============================================================================*/
#include "coord_frames.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void print_rule(const char *title)
{
    printf("\n────────────────────────────────────────────────────────────────────\n");
    if (title != NULL) printf(" %s\n", title);
    printf("────────────────────────────────────────────────────────────────────\n");
}

static void print_vec(const char *label, cf_vec3_t v, const char *unit)
{
    printf("  %-26s %+18.4f %+18.4f %+18.4f  [%s]\n", label, v.x, v.y, v.z, unit);
}

static void print_mat(const char *label, cf_mat3_t A)
{
    int i;
    printf("  %s\n", label);
    for (i = 0; i < 3; ++i)
        printf("      [ %+13.9f  %+13.9f  %+13.9f ]\n",
               A.m[i][0], A.m[i][1], A.m[i][2]);
    printf("      det = %+.15f,   |A^T·A − I|max = %.3e\n",
           cf_mat_det(A), cf_mat_orthonormality_error(A));
}

/*  옵션이 요구하는 값의 개수. 모르는 옵션이면 -1.
 *  값 개수 검사보다 '아는 옵션인가'를 먼저 판정해야
 *  오타를 "값이 부족하다"로 잘못 안내하지 않는다.                         */
/*  Windows 콘솔은 기본 코드페이지가 UTF-8 이 아니라서(한국어 Windows 는 CP949)
 *  UTF-8 로 저장된 한글 문자열이 그대로 출력되면 깨진다.
 *  실행 직후 한 번만 콘솔 출력 코드페이지를 UTF-8 로 바꿔 준다.
 *  다른 OS 에서는 아무 일도 하지 않는다.                                    */
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

static void console_use_utf8(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
}

static int opt_nargs(const char *o)
{
    static const char *one[] = {
        "-r","--range","-a","--az","-e","--el",
        "--lat","--lon","--alt","--roll","--pitch","--yaw",
        "--mount-az","--mount-el", NULL
    };
    int i;
    for (i = 0; one[i] != NULL; ++i)
        if (strcmp(o, one[i]) == 0) return 1;
    if (strcmp(o, "--lever") == 0) return 3;
    return -1;
}

static void usage(const char *prog)
{
    printf("사용법: %s [옵션]...\n", prog);
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
    printf("    %s                          기본 예제값\n", prog);
    printf("    %s -r 50000 -a 15 -e 3      측정값만 변경\n", prog);
    printf("    %s --roll 20                배가 20도 기울었을 때\n", prog);
    printf("    %s --lever 0 0 0            레버암을 무시했을 때\n", prog);
}

int main(int argc, char **argv)
{
    console_use_utf8();

    /*------------------------------------------------------------------
     *  기본 입력값. -h 로 옵션을 확인할 수 있고, 명령행 인자로 덮어쓸 수 있다.
     *----------------------------------------------------------------*/
    cf_polar_t meas = {
        20000.0,                 /* Slant Range 20 km          */
        CF_DEG2RAD(30.0),        /* Azimuth   30 deg           */
        CF_DEG2RAD(0.0)          /* Elevation  0 deg           */
    };

    cf_geodetic_t platform = {
        CF_DEG2RAD(36.408),      /* 위도                       */
        CF_DEG2RAD(127.307),     /* 경도                       */
        0.0                      /* 고도(타원체고)             */
    };

    cf_attitude_t attitude = {
        CF_DEG2RAD(0.0),         /* roll                       */
        CF_DEG2RAD(0.0),         /* pitch                      */
        CF_DEG2RAD(45.0)         /* yaw (heading)              */
    };

    /* 조건2: 선수 기준 시계방향 90도, 무게중심에서 30 m 뒤 / 10 m 위
     *        동체 FRD 기준이므로  뒤 = x 음수,  위 = z 음수            */
    cf_mount_t mount = {
        CF_DEG2RAD(90.0),        /* 설치 방위                  */
        CF_DEG2RAD(0.0),         /* 백틸트 없음                */
        { -30.0, 0.0, -10.0 }    /* 레버암 [m]                 */
    };

    cf_geodetic_t target;
    cf_chain_t    chain;
    cf_polar_t    back;
    cf_status_t   st;
    int           i;

    /*------------------------------------------------------------------
     *  명령행 인자 처리 — 인자가 없으면 위 기본값을 그대로 쓴다.
     *----------------------------------------------------------------*/
    for (i = 1; i < argc; ++i) {
        const char *o = argv[i];
        int need;

        if (strcmp(o, "-h") == 0 || strcmp(o, "--help") == 0) {
            usage(argv[0]);
            return 0;
        }

        need = opt_nargs(o);
        if (need < 0) {
            fprintf(stderr, "오류: 알 수 없는 옵션 '%s'\n\n", o);
            usage(argv[0]);
            return 2;
        }
        if (i + need >= argc) {
            fprintf(stderr, "오류: %s 옵션에 값이 %d개 필요합니다.\n\n", o, need);
            usage(argv[0]);
            return 2;
        }

        if      (strcmp(o,"-r")==0 || strcmp(o,"--range")==0)  meas.range_m    = atof(argv[++i]);
        else if (strcmp(o,"-a")==0 || strcmp(o,"--az")==0)     meas.az_rad     = CF_DEG2RAD(atof(argv[++i]));
        else if (strcmp(o,"-e")==0 || strcmp(o,"--el")==0)     meas.el_rad     = CF_DEG2RAD(atof(argv[++i]));
        else if (strcmp(o,"--lat")==0)      platform.lat_rad   = CF_DEG2RAD(atof(argv[++i]));
        else if (strcmp(o,"--lon")==0)      platform.lon_rad   = CF_DEG2RAD(atof(argv[++i]));
        else if (strcmp(o,"--alt")==0)      platform.alt_m     = atof(argv[++i]);
        else if (strcmp(o,"--roll")==0)     attitude.roll_rad  = CF_DEG2RAD(atof(argv[++i]));
        else if (strcmp(o,"--pitch")==0)    attitude.pitch_rad = CF_DEG2RAD(atof(argv[++i]));
        else if (strcmp(o,"--yaw")==0)      attitude.yaw_rad   = CF_DEG2RAD(atof(argv[++i]));
        else if (strcmp(o,"--mount-az")==0) mount.yaw_rad      = CF_DEG2RAD(atof(argv[++i]));
        else if (strcmp(o,"--mount-el")==0) mount.pitch_rad    = CF_DEG2RAD(atof(argv[++i]));
        else if (strcmp(o,"--lever")==0) {
            mount.lever_arm_b.x = atof(argv[++i]);
            mount.lever_arm_b.y = atof(argv[++i]);
            mount.lever_arm_b.z = atof(argv[++i]);
        }
    }

    if (meas.range_m <= 0.0) {
        fprintf(stderr, "오류: 거리(-r)는 0보다 커야 합니다.\n");
        return 2;
    }

    /*------------------------------------------------------------------
     *  입력 요약
     *----------------------------------------------------------------*/
    print_rule("입력 조건");
    printf("  신호처리 측정값   R = %.1f m,  Az = %.3f deg,  El = %.3f deg\n",
           meas.range_m, CF_RAD2DEG(meas.az_rad), CF_RAD2DEG(meas.el_rad));
    printf("  플랫폼 위치       lat = %.6f deg,  lon = %.6f deg,  alt = %.3f m\n",
           CF_RAD2DEG(platform.lat_rad), CF_RAD2DEG(platform.lon_rad), platform.alt_m);
    printf("  플랫폼 자세       roll = %.3f,  pitch = %.3f,  yaw = %.3f  [deg]\n",
           CF_RAD2DEG(attitude.roll_rad), CF_RAD2DEG(attitude.pitch_rad),
           CF_RAD2DEG(attitude.yaw_rad));
    printf("  안테나 설치       방위 = %.3f deg,  백틸트 = %.3f deg\n",
           CF_RAD2DEG(mount.yaw_rad), CF_RAD2DEG(mount.pitch_rad));
    printf("  레버암(FRD)       (%+.1f, %+.1f, %+.1f) m   = 30 m 뒤, 10 m 위\n",
           mount.lever_arm_b.x, mount.lever_arm_b.y, mount.lever_arm_b.z);

    /*------------------------------------------------------------------
     *  정변환
     *----------------------------------------------------------------*/
    st = cf_forward_chain(meas, &mount, &attitude, platform, &target, &chain);
    if (st != CF_OK) {
        fprintf(stderr, "정변환 실패: status = %d\n", (int)st);
        return 1;
    }

    print_rule("1단계  안테나 극좌표 → 안테나 직교좌표 / UV");
    printf("  방향코사인(UV)             u = %+.12f  (면 가로·오른쪽)\n", chain.dircos.u);
    printf("                             v = %+.12f  (면 세로·위)\n",     chain.dircos.v);
    printf("                             w = %+.12f  (보어사이트)\n",     chain.dircos.w);
    printf("  ‖(u,v,w)‖ − 1              %+.3e\n",
           sqrt(chain.dircos.u*chain.dircos.u + chain.dircos.v*chain.dircos.v
              + chain.dircos.w*chain.dircos.w) - 1.0);
    print_vec("안테나 직교 p_ant", chain.p_antenna, "m");
    printf("      x = 보어사이트 방향,  y = 오른쪽,  z = 아래\n");

    print_rule("2단계  안테나 좌표계 → 동체 좌표계  (회전 + 평행이동)");
    print_mat("C_body_from_antenna = Rz(90°)·Ry(0°)", chain.C_body_from_ant);
    print_vec("회전만 적용", cf_mat_apply(chain.C_body_from_ant, chain.p_antenna), "m");
    print_vec("+ 레버암 → p_body", chain.p_body, "m");
    printf("      선수 기준 방위 %.6f deg,  수평거리 %.4f m\n",
           fmod(CF_RAD2DEG(atan2(chain.p_body.y, chain.p_body.x)) + 360.0, 360.0),
           sqrt(chain.p_body.x*chain.p_body.x + chain.p_body.y*chain.p_body.y));

    print_rule("3단계  동체 좌표계 → 로컬(NED) 좌표계  (회전)");
    print_mat("C_ned_from_body = Rz(45°)·Ry(0°)·Rx(0°)", chain.C_ned_from_body);
    print_vec("p_ned  (N, E, D)", chain.p_ned, "m");
    printf("      진북 기준 방위 %.6f deg,  수평거리 %.4f m,  평면고도 %.4f m\n",
           CF_RAD2DEG(chain.true_bearing_rad), chain.ground_range_m,
           platform.alt_m - chain.p_ned.z);

    print_rule("4단계  로컬(NED) → ECEF");
    print_mat("C_ned_from_ecef(lat, lon)",
              cf_dcm_ned_from_ecef(platform.lat_rad, platform.lon_rad));
    print_vec("플랫폼 ECEF", chain.p_ecef_platform, "m");
    print_vec("표적   ECEF", chain.p_ecef_target,   "m");

    print_rule("5단계  ECEF → LLA (Bowring)");
    printf("  표적 위도                  %.9f deg\n", CF_RAD2DEG(target.lat_rad));
    printf("  표적 경도                  %.9f deg\n", CF_RAD2DEG(target.lon_rad));
    printf("  표적 고도(타원체고)        %.4f m\n",    target.alt_m);
    printf("  로컬 평면 기준 고도        %.4f m   (차이 %.1f m = 지구 곡률)\n",
           platform.alt_m - chain.p_ned.z,
           target.alt_m - (platform.alt_m - chain.p_ned.z));

    /*------------------------------------------------------------------
     *  역변환 (조건6의 두 번째 출력)
     *----------------------------------------------------------------*/
    st = cf_inverse_chain(target, &mount, &attitude, platform, &back);
    if (st != CF_OK) {
        fprintf(stderr, "역변환 실패: status = %d\n", (int)st);
        return 1;
    }

    print_rule("역변환  LLA → ECEF → 로컬 → 동체 → 안테나 극좌표");
    printf("  시선거리 R                 %.6f m\n",  back.range_m);
    printf("  방위각   Az                %.9f deg\n", CF_RAD2DEG(back.az_rad));
    printf("  고각     El                %.9f deg\n", CF_RAD2DEG(back.el_rad));
    printf("  왕복오차                   ΔR = %.3e m,  ΔAz = %.3e deg,  ΔEl = %.3e deg\n",
           fabs(back.range_m - meas.range_m),
           fabs(CF_RAD2DEG(back.az_rad - meas.az_rad)),
           fabs(CF_RAD2DEG(back.el_rad - meas.el_rad)));

    /*------------------------------------------------------------------
     *  최종 출력 (조건6)
     *----------------------------------------------------------------*/
    print_rule("최종 출력  —  통제제어 전달 값");
    printf("  [1] 위/경/고도 좌표계\n");
    printf("        위도  %.9f deg\n", CF_RAD2DEG(target.lat_rad));
    printf("        경도  %.9f deg\n", CF_RAD2DEG(target.lon_rad));
    printf("        고도  %.4f m  (WGS-84 타원체고)\n", target.alt_m);
    printf("  [2] 안테나 좌표계 (극좌표)\n");
    printf("        시선거리  %.4f m\n",   back.range_m);
    printf("        방위각    %.6f deg\n", CF_RAD2DEG(back.az_rad));
    printf("        고각      %.6f deg\n", CF_RAD2DEG(back.el_rad));
    printf("\n");
    return 0;
}
