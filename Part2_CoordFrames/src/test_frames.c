/*==============================================================================
 *  test_frames.c  —  coord_frames 단위 시험
 *------------------------------------------------------------------------------
 *  좌표변환 코드의 버그는 대부분 "컴파일도 되고 값도 그럴듯해 보이는" 종류다.
 *  아래 시험들은 각각 특정 유형의 버그를 겨냥한다.
 *
 *    T1 행렬 성질      det=+1, 직교성       → 회전행렬 구성 오류
 *    T2 극↔직교 왕복                        → sin/cos 자리바꿈, z 부호
 *    T3 UV↔극 왕복                          → 방향코사인 정의 오류
 *    T4 LLA↔ECEF 왕복                       → (1−e²) 누락, Bowring 오타
 *    T5 전체 체인 왕복                       → 단계 연결·레버암 부호
 *    T6 기지값(예제 기대값)                    → 전체 구현 정합성
 *    T7 특이점                               → NaN/Inf, 0 나눗셈
 *    T8 비대칭 자세                          → 전치 방향 뒤바뀜
 *    T9 부호 민감도                          → 부호를 바꿔도 결과가 같은 실수
 *============================================================================*/
#include "coord_frames.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

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

static int g_pass = 0, g_fail = 0;

static void check(const char *name, int ok, double measured, double tol)
{
    if (ok) { ++g_pass; printf("  [ OK ] %-46s %9.2e (허용 %.0e)\n", name, measured, tol); }
    else    { ++g_fail; printf("  [FAIL] %-46s %9.2e (허용 %.0e)\n", name, measured, tol); }
}

static void check_le(const char *name, double measured, double tol)
{
    check(name, (measured <= tol) && !isnan(measured), measured, tol);
}

/* 재현 가능한 난수 (표준 rand 로 충분) */
static double urand(double lo, double hi)
{
    return lo + (hi - lo) * ((double)rand() / (double)RAND_MAX);
}

/*--------------------------------------------------------------------------*/
static void t1_matrix_properties(void)
{
    double worst_det = 0.0, worst_orth = 0.0;
    int i;
    printf("\nT1  회전행렬 성질 (det = +1, 직교)\n");
    for (i = 0; i < 2000; ++i) {
        cf_attitude_t a;
        cf_mount_t    m;
        cf_mat3_t     C;
        double        lat, lon;

        a.roll_rad  = urand(-CF_PI, CF_PI);
        a.pitch_rad = urand(-CF_PI/2.0, CF_PI/2.0);
        a.yaw_rad   = urand(-CF_PI, CF_PI);
        C = cf_dcm_ned_from_body(&a);
        worst_det  = fmax(worst_det,  fabs(cf_mat_det(C) - 1.0));
        worst_orth = fmax(worst_orth, cf_mat_orthonormality_error(C));

        m.yaw_rad = urand(-CF_PI, CF_PI);
        m.pitch_rad = urand(-CF_PI/2.0, CF_PI/2.0);
        m.lever_arm_b.x = m.lever_arm_b.y = m.lever_arm_b.z = 0.0;
        C = cf_dcm_body_from_antenna(&m);
        worst_det  = fmax(worst_det,  fabs(cf_mat_det(C) - 1.0));
        worst_orth = fmax(worst_orth, cf_mat_orthonormality_error(C));

        lat = urand(-CF_PI/2.0, CF_PI/2.0);
        lon = urand(-CF_PI, CF_PI);
        C = cf_dcm_ned_from_ecef(lat, lon);
        worst_det  = fmax(worst_det,  fabs(cf_mat_det(C) - 1.0));
        worst_orth = fmax(worst_orth, cf_mat_orthonormality_error(C));
    }
    check_le("|det − 1| 최대", worst_det, 1e-12);
    check_le("|AᵀA − I| 최대", worst_orth, 1e-12);
}

/*--------------------------------------------------------------------------*/
static void t2_polar_cart_roundtrip(void)
{
    double worst_r = 0.0, worst_a = 0.0, worst_e = 0.0;
    int i;
    printf("\nT2  안테나 극좌표 ↔ 직교좌표 왕복\n");
    for (i = 0; i < 100000; ++i) {
        cf_polar_t p, q;
        p.range_m = urand(1.0, 500000.0);
        p.az_rad  = urand(-CF_PI + 1e-6, CF_PI - 1e-6);
        p.el_rad  = urand(-CF_PI/2.0 + 1e-6, CF_PI/2.0 - 1e-6);
        q = cf_polar_from_antcart(cf_antcart_from_polar(p));
        worst_r = fmax(worst_r, fabs(q.range_m - p.range_m));
        worst_a = fmax(worst_a, fabs(q.az_rad  - p.az_rad));
        worst_e = fmax(worst_e, fabs(q.el_rad  - p.el_rad));
    }
    check_le("거리 오차 [m]",   worst_r, 1e-6);
    check_le("방위각 오차 [rad]", worst_a, 1e-12);
    check_le("고각 오차 [rad]",   worst_e, 1e-13);
}

/*--------------------------------------------------------------------------*/
static void t3_uv_roundtrip(void)
{
    double worst_a = 0.0, worst_e = 0.0, worst_n = 0.0;
    int i;
    printf("\nT3  UV(방향코사인) ↔ 극좌표 왕복\n");
    for (i = 0; i < 100000; ++i) {
        cf_polar_t  p, q;
        cf_dircos_t d;
        double      n;
        /* 보어사이트 반구 안에서만 정의 (|Az| < 90°) */
        p.range_m = 1.0;
        p.az_rad  = urand(-CF_PI/2.0 + 1e-6, CF_PI/2.0 - 1e-6);
        p.el_rad  = urand(-CF_PI/2.0 + 1e-6, CF_PI/2.0 - 1e-6);
        d = cf_dircos_from_polar(p);
        n = sqrt(d.u*d.u + d.v*d.v + d.w*d.w);
        worst_n = fmax(worst_n, fabs(n - 1.0));
        q = cf_polar_from_dircos(d, p.range_m);
        worst_a = fmax(worst_a, fabs(q.az_rad - p.az_rad));
        worst_e = fmax(worst_e, fabs(q.el_rad - p.el_rad));
    }
    check_le("‖(u,v,w)‖ − 1",     worst_n, 1e-14);
    check_le("방위각 오차 [rad]", worst_a, 1e-13);
    check_le("고각 오차 [rad]",   worst_e, 1e-13);
}

/*--------------------------------------------------------------------------*/
static void t4_lla_ecef_roundtrip(void)
{
    double worst_h = 0.0, worst_pos = 0.0;
    int i;
    printf("\nT4  LLA ↔ ECEF 왕복 (Bowring)\n");
    for (i = 0; i < 200000; ++i) {
        cf_geodetic_t g, back;
        cf_vec3_t     p, p2;
        g.lat_rad = urand(CF_DEG2RAD(-89.5), CF_DEG2RAD(89.5));
        g.lon_rad = urand(-CF_PI, CF_PI);
        g.alt_m   = urand(-500.0, 40000.0);
        p = cf_ecef_from_geodetic(g);
        if (cf_geodetic_from_ecef(p, &back) != CF_OK) { ++g_fail; return; }
        worst_h = fmax(worst_h, fabs(back.alt_m - g.alt_m));
        p2 = cf_ecef_from_geodetic(back);           /* 위치 오차로 환산 */
        worst_pos = fmax(worst_pos, cf_vec_norm(cf_vec_sub(p2, p)));
    }
    check_le("고도 오차 [m]",     worst_h,   1e-6);
    check_le("위치 왕복 오차 [m]", worst_pos, 1e-6);
}

/*--------------------------------------------------------------------------*/
static void t5_full_chain_roundtrip(void)
{
    double worst_r = 0.0, worst_a = 0.0, worst_e = 0.0;
    int i;
    printf("\nT5  전체 체인 왕복 (측정값 → LLA → 측정값)\n");
    for (i = 0; i < 20000; ++i) {
        cf_polar_t    meas, back;
        cf_mount_t    mount;
        cf_attitude_t att;
        cf_geodetic_t plat, tgt;

        meas.range_m = urand(100.0, 300000.0);
        meas.az_rad  = urand(CF_DEG2RAD(-80.0), CF_DEG2RAD(80.0));
        meas.el_rad  = urand(CF_DEG2RAD(-60.0), CF_DEG2RAD(60.0));

        mount.yaw_rad       = urand(-CF_PI, CF_PI);
        mount.pitch_rad     = urand(CF_DEG2RAD(-30.0), CF_DEG2RAD(30.0));
        mount.lever_arm_b.x = urand(-60.0, 60.0);
        mount.lever_arm_b.y = urand(-20.0, 20.0);
        mount.lever_arm_b.z = urand(-40.0, 10.0);

        att.roll_rad  = urand(CF_DEG2RAD(-40.0), CF_DEG2RAD(40.0));
        att.pitch_rad = urand(CF_DEG2RAD(-20.0), CF_DEG2RAD(20.0));
        att.yaw_rad   = urand(-CF_PI, CF_PI);

        plat.lat_rad = urand(CF_DEG2RAD(-70.0), CF_DEG2RAD(70.0));
        plat.lon_rad = urand(-CF_PI, CF_PI);
        plat.alt_m   = urand(0.0, 50.0);

        if (cf_forward_chain(meas, &mount, &att, plat, &tgt, NULL) != CF_OK) { ++g_fail; return; }
        if (cf_inverse_chain(tgt, &mount, &att, plat, &back)       != CF_OK) { ++g_fail; return; }

        worst_r = fmax(worst_r, fabs(back.range_m - meas.range_m));
        worst_a = fmax(worst_a, fabs(back.az_rad  - meas.az_rad));
        worst_e = fmax(worst_e, fabs(back.el_rad  - meas.el_rad));
    }
    /*  [ 허용치를 이렇게 잡은 근거 ]
     *  ECEF 좌표는 6.4e6 m 규모다. double 의 상대정밀도가 약 2.2e-16 이므로
     *  이 크기에서 표현 가능한 최소 간격이 약 1.4e-9 m 이고, 체인이 여러 번
     *  더하고 빼는 동안 그 몇 배(대략 1e-8 m)가 쌓인다.
     *  각도 오차는 그 위치 오차를 거리로 나눈 값이므로, 본 시험의 최단
     *  거리 100 m 를 기준으로  1e-8 m / 100 m = 1e-10 rad 이 한계다.
     *  즉 아래 값은 알고리즘 오차가 아니라 배정밀도 자체의 바닥이다.       */
    check_le("거리 오차 [m]",     worst_r, 1e-6);
    check_le("방위각 오차 [rad]", worst_a, 1e-10);
    check_le("고각 오차 [rad]",   worst_e, 1e-10);
}

/*--------------------------------------------------------------------------*/
static void t6_known_answer(void)
{
    /* 예제 입력값에 대한 기대값 (독립 구현(NumPy)으로 교차 검증) */
    const cf_polar_t    meas  = { 20000.0, CF_DEG2RAD(30.0), CF_DEG2RAD(0.0) };
    const cf_geodetic_t plat  = { CF_DEG2RAD(36.408), CF_DEG2RAD(127.307), 0.0 };
    const cf_attitude_t att   = { 0.0, 0.0, CF_DEG2RAD(45.0) };
    const cf_mount_t    mount = { CF_DEG2RAD(90.0), 0.0, { -30.0, 0.0, -10.0 } };

    const double EXP_LAT = 36.233700252;   /* deg */
    const double EXP_LON = 127.364344961;  /* deg */
    const double EXP_ALT = 41.4952;        /* m   */

    cf_geodetic_t tgt;
    cf_chain_t    ch;
    cf_polar_t    back;

    printf("\nT6  기지값 — 예제 입력값\n");
    if (cf_forward_chain(meas, &mount, &att, plat, &tgt, &ch) != CF_OK) { ++g_fail; return; }
    if (cf_inverse_chain(tgt, &mount, &att, plat, &back)      != CF_OK) { ++g_fail; return; }

    check_le("표적 위도 오차 [deg]", fabs(CF_RAD2DEG(tgt.lat_rad) - EXP_LAT), 1e-8);
    check_le("표적 경도 오차 [deg]", fabs(CF_RAD2DEG(tgt.lon_rad) - EXP_LON), 1e-8);
    check_le("표적 고도 오차 [m]",   fabs(tgt.alt_m - EXP_ALT),               1e-3);
    check_le("역변환 거리 오차 [m]", fabs(back.range_m - 20000.0),            1e-6);
    check_le("역변환 방위 오차 [deg]", fabs(CF_RAD2DEG(back.az_rad) - 30.0),  1e-9);
    check_le("역변환 고각 오차 [deg]", fabs(CF_RAD2DEG(back.el_rad) -  0.0),  1e-9);

    /* 중간값도 확인 — 단계별 서술과 문서가 일치하는지 */
    check_le("p_ant.x 기대 +17320.5081", fabs(ch.p_antenna.x - 17320.5080757), 1e-6);
    check_le("p_body.x 기대 −10030.0000", fabs(ch.p_body.x  - (-10030.0)),     1e-6);
    check_le("p_ned.N  기대 −19339.7297", fabs(ch.p_ned.x   - (-19339.72971)), 1e-4);
    check_le("진북방위 기대 165.074374°",
             fabs(CF_RAD2DEG(ch.true_bearing_rad) - 165.074374), 1e-5);
}

/*--------------------------------------------------------------------------*/
static void t7_singularities(void)
{
    cf_geodetic_t g, back;
    cf_vec3_t     p;
    cf_polar_t    pol, q;
    double worst = 0.0;
    int i;
    const double lats[4] = { 90.0, -90.0, 89.99999, 0.0 };

    printf("\nT7  특이점 처리 (NaN / Inf 없음)\n");

    for (i = 0; i < 4; ++i) {                    /* 극점 부근 */
        g.lat_rad = CF_DEG2RAD(lats[i]);
        g.lon_rad = CF_DEG2RAD(123.0);
        g.alt_m   = 100.0;
        p = cf_ecef_from_geodetic(g);
        if (cf_geodetic_from_ecef(p, &back) != CF_OK) { ++g_fail; return; }
        if (isnan(back.lat_rad) || isnan(back.alt_m)) { ++g_fail; return; }
        worst = fmax(worst, fabs(back.alt_m - g.alt_m));
    }
    check_le("극점 부근 고도 오차 [m]", worst, 1e-4);

    /* 보어사이트 정면 / 천정 / 천저 */
    worst = 0.0;
    {
        const double els[3] = { 0.0, 90.0, -90.0 };
        for (i = 0; i < 3; ++i) {
            pol.range_m = 1000.0;
            pol.az_rad  = 0.0;
            pol.el_rad  = CF_DEG2RAD(els[i]);
            q = cf_polar_from_antcart(cf_antcart_from_polar(pol));
            if (isnan(q.az_rad) || isnan(q.el_rad)) { ++g_fail; return; }
            worst = fmax(worst, fabs(q.el_rad - pol.el_rad));
        }
    }
    check_le("El = 0/±90° 왕복 오차 [rad]", worst, 1e-12);

    /* 거리 0 — 방향이 정의되지 않아도 NaN 이 나오면 안 된다 */
    {
        cf_vec3_t zero = { 0.0, 0.0, 0.0 };
        q = cf_polar_from_antcart(zero);
        check("거리 0 에서 NaN 없음",
              (!isnan(q.range_m) && !isnan(q.az_rad) && !isnan(q.el_rad)), 0.0, 0.0);
    }

    /* 경도 ±180 경계 */
    {
        cf_geodetic_t a = { CF_DEG2RAD(10.0), CF_DEG2RAD(179.999999), 0.0 };
        cf_geodetic_t b2;
        if (cf_geodetic_from_ecef(cf_ecef_from_geodetic(a), &b2) != CF_OK) { ++g_fail; return; }
        check_le("경도 ±180° 경계 오차 [deg]",
                 fabs(CF_RAD2DEG(b2.lon_rad) - 179.999999), 1e-9);
    }
}

/*--------------------------------------------------------------------------*/
static void t8_asymmetric_attitude(void)
{
    /*  전치 방향이 뒤바뀐 버그는 대칭적인 자세에서는 드러나지 않는다.
     *  roll·pitch·yaw 를 전부 다른 값으로 두고, C 와 Cᵀ 가 실제로
     *  다른 결과를 주는지, 그리고 C·Cᵀ 가 항등인지 확인한다.            */
    const cf_attitude_t att = { CF_DEG2RAD(17.0), CF_DEG2RAD(-9.0), CF_DEG2RAD(123.0) };
    const cf_vec3_t     v   = { 1234.5, -678.9, 246.8 };
    cf_vec3_t fwd, rt;
    double diff, ident;

    printf("\nT8  비대칭 자세 — 전치 방향 검출\n");
    fwd = cf_ned_from_body(&att, v);
    rt  = cf_body_from_ned(&att, fwd);

    ident = cf_vec_norm(cf_vec_sub(rt, v));
    check_le("C 적용 후 Cᵀ 적용 = 원본 [m]", ident, 1e-9);

    /* 정방향과 역방향 결과가 '달라야' 정상이다 (같으면 전치가 무의미) */
    diff = cf_vec_norm(cf_vec_sub(fwd, cf_body_from_ned(&att, v)));
    check("정변환 ≠ 역변환 (구분됨)", diff > 1.0, diff, 1.0);
}

/*--------------------------------------------------------------------------*/
static void t9_sign_sensitivity(void)
{
    /*  부호를 뒤집었을 때 결과가 '반드시 달라져야' 한다.
     *  같다면 그 항이 실제로는 계산에 쓰이지 않고 있다는 뜻이다.        */
    const cf_polar_t    meas  = { 20000.0, CF_DEG2RAD(30.0), CF_DEG2RAD(12.0) };
    const cf_geodetic_t plat  = { CF_DEG2RAD(36.408), CF_DEG2RAD(127.307), 0.0 };
    cf_attitude_t att   = { CF_DEG2RAD(10.0), CF_DEG2RAD(5.0), CF_DEG2RAD(45.0) };
    cf_mount_t    mount = { CF_DEG2RAD(90.0), 0.0, { -30.0, 0.0, -10.0 } };
    cf_geodetic_t base, flipped;

    printf("\nT9  부호 민감도\n");
    if (cf_forward_chain(meas, &mount, &att, plat, &base, NULL) != CF_OK) { ++g_fail; return; }

    att.roll_rad = -att.roll_rad;                 /* roll 부호 반전 */
    if (cf_forward_chain(meas, &mount, &att, plat, &flipped, NULL) != CF_OK) { ++g_fail; return; }
    check("roll 부호 반전 → 결과 변함",
          fabs(flipped.alt_m - base.alt_m) > 1.0,
          fabs(flipped.alt_m - base.alt_m), 1.0);
    att.roll_rad = -att.roll_rad;

    mount.lever_arm_b.z = -mount.lever_arm_b.z;   /* 레버암 z 부호 반전 */
    if (cf_forward_chain(meas, &mount, &att, plat, &flipped, NULL) != CF_OK) { ++g_fail; return; }
    check("레버암 z 부호 반전 → 결과 변함",
          fabs(flipped.alt_m - base.alt_m) > 1.0,
          fabs(flipped.alt_m - base.alt_m), 1.0);
}

/*--------------------------------------------------------------------------*/
int main(void)
{
    console_use_utf8();
    srand(20260901u);
    printf("════════════════════════════════════════════════════════════════════\n");
    printf(" coord_frames 단위 시험\n");
    printf("════════════════════════════════════════════════════════════════════\n");

    t1_matrix_properties();
    t2_polar_cart_roundtrip();
    t3_uv_roundtrip();
    t4_lla_ecef_roundtrip();
    t5_full_chain_roundtrip();
    t6_known_answer();
    t7_singularities();
    t8_asymmetric_attitude();
    t9_sign_sensitivity();

    printf("\n════════════════════════════════════════════════════════════════════\n");
    printf(" 결과 : 통과 %d / 실패 %d\n", g_pass, g_fail);
    printf("════════════════════════════════════════════════════════════════════\n");
    return (g_fail == 0) ? 0 : 1;
}
