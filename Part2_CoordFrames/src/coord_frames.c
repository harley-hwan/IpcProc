/*==============================================================================
 *  coord_frames.c  —  레이다 좌표계 변환 구현
 *============================================================================*/
#include "coord_frames.h"
#include <math.h>
#include <string.h>

/*----------------------------------------------------------------------------
 *  내부 헬퍼
 *--------------------------------------------------------------------------*/

/*  [ asin / acos 를 쓰지 않는 이유 ]
 *
 *  각도를 되찾을 때 흔히 asin(z/R) 를 쓰지만 두 가지 문제가 있다.
 *    (1) 반올림으로 인자가 1.0000000000000002 가 되면 NaN 이 나오고,
 *        NaN 은 아무 경고 없이 뒤 계산 전체로 번진다. (clamp 필요)
 *    (2) 인자가 ±1 에 가까우면 asin 의 기울기가 무한대로 발산해
 *        정밀도가 급격히 떨어진다.
 *  이 구현은 대신 atan2(분자, 분모) 형태를 쓴다. 정의역 제한이 없고
 *  전 구간에서 조건수가 좋아 clamp 자체가 필요 없다.
 */

/*============================================================================
 *  1. 벡터·행렬 기본 연산
 *==========================================================================*/
cf_vec3_t cf_vec_add(cf_vec3_t a, cf_vec3_t b)
{
    cf_vec3_t r = { a.x + b.x, a.y + b.y, a.z + b.z };
    return r;
}

cf_vec3_t cf_vec_sub(cf_vec3_t a, cf_vec3_t b)
{
    cf_vec3_t r = { a.x - b.x, a.y - b.y, a.z - b.z };
    return r;
}

double cf_vec_norm(cf_vec3_t a)
{
    return sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
}

/*  기본 회전행렬 — 각 축 둘레의 우수(right-handed) 회전
 *
 *          [ 1   0    0 ]        [  c  0  s ]        [ c -s  0 ]
 *  Rx(a) = [ 0   c   -s ]  Ry(b)=[  0  1  0 ]  Rz(g)=[ s  c  0 ]
 *          [ 0   s    c ]        [ -s  0  c ]        [ 0  0  1 ]
 */
cf_mat3_t cf_rot_x(double a)
{
    const double c = cos(a), s = sin(a);
    cf_mat3_t R = { { {1.0, 0.0, 0.0},
                      {0.0,   c,  -s},
                      {0.0,   s,   c} } };
    return R;
}

cf_mat3_t cf_rot_y(double a)
{
    const double c = cos(a), s = sin(a);
    cf_mat3_t R = { { {  c, 0.0,   s},
                      {0.0, 1.0, 0.0},
                      { -s, 0.0,   c} } };
    return R;
}

cf_mat3_t cf_rot_z(double a)
{
    const double c = cos(a), s = sin(a);
    cf_mat3_t R = { { {  c,  -s, 0.0},
                      {  s,   c, 0.0},
                      {0.0, 0.0, 1.0} } };
    return R;
}

cf_mat3_t cf_mat_mul(cf_mat3_t A, cf_mat3_t B)
{
    cf_mat3_t R;
    int i, j, k;
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            double s = 0.0;
            for (k = 0; k < 3; ++k) s += A.m[i][k] * B.m[k][j];
            R.m[i][j] = s;
        }
    }
    return R;
}

cf_vec3_t cf_mat_apply(cf_mat3_t A, cf_vec3_t v)
{
    cf_vec3_t r;
    r.x = A.m[0][0]*v.x + A.m[0][1]*v.y + A.m[0][2]*v.z;
    r.y = A.m[1][0]*v.x + A.m[1][1]*v.y + A.m[1][2]*v.z;
    r.z = A.m[2][0]*v.x + A.m[2][1]*v.y + A.m[2][2]*v.z;
    return r;
}

cf_mat3_t cf_mat_trans(cf_mat3_t A)
{
    cf_mat3_t R;
    int i, j;
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j) R.m[i][j] = A.m[j][i];
    return R;
}

double cf_mat_det(cf_mat3_t A)
{
    return A.m[0][0] * (A.m[1][1]*A.m[2][2] - A.m[1][2]*A.m[2][1])
         - A.m[0][1] * (A.m[1][0]*A.m[2][2] - A.m[1][2]*A.m[2][0])
         + A.m[0][2] * (A.m[1][0]*A.m[2][1] - A.m[1][1]*A.m[2][0]);
}

double cf_mat_orthonormality_error(cf_mat3_t A)
{
    cf_mat3_t P = cf_mat_mul(cf_mat_trans(A), A);
    double worst = 0.0;
    int i, j;
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            const double expect = (i == j) ? 1.0 : 0.0;
            const double e = fabs(P.m[i][j] - expect);
            if (e > worst) worst = e;
        }
    }
    return worst;
}

/*============================================================================
 *  1단계  안테나 극좌표  <->  안테나 직교좌표  <->  UV(방향코사인)
 *--------------------------------------------------------------------------
 *  안테나 프레임:  x = 보어사이트,  y = 오른쪽,  z = 아래
 *
 *      x = R * cos(El) * cos(Az)
 *      y = R * cos(El) * sin(Az)
 *      z = -R * sin(El)            <- z 가 '아래'이므로 위로 향하면 음수
 *==========================================================================*/
cf_vec3_t cf_antcart_from_polar(cf_polar_t p)
{
    const double ce = cos(p.el_rad), se = sin(p.el_rad);
    cf_vec3_t r;
    r.x =  p.range_m * ce * cos(p.az_rad);
    r.y =  p.range_m * ce * sin(p.az_rad);
    r.z = -p.range_m * se;
    return r;
}

cf_polar_t cf_polar_from_antcart(cf_vec3_t p)
{
    cf_polar_t out;
    out.range_m = cf_vec_norm(p);
    if (out.range_m < 1e-12) {          /* 원점: 방향이 정의되지 않는다 */
        out.az_rad = 0.0;
        out.el_rad = 0.0;
        return out;
    }
    out.az_rad = atan2(p.y, p.x);                    /* 사분면 보존 */
    /*  El 은 asin(-z/R) 로도 구할 수 있지만, |El| 이 90도에 가까우면
     *  asin 의 기울기가 무한대로 발산해 정밀도가 급격히 떨어진다.
     *  atan2 형태는 전 구간에서 조건수가 좋다.                          */
    out.el_rad = atan2(-p.z, sqrt(p.x * p.x + p.y * p.y));
    return out;
}

cf_dircos_t cf_dircos_from_polar(cf_polar_t p)
{
    const double ce = cos(p.el_rad);
    cf_dircos_t d;
    d.u = ce * sin(p.az_rad);   /* 면 가로(오른쪽) 성분 */
    d.v = sin(p.el_rad);        /* 면 세로(위)     성분 */
    d.w = ce * cos(p.az_rad);   /* 보어사이트 성분      */
    return d;
}

cf_polar_t cf_polar_from_dircos(cf_dircos_t d, double range_m)
{
    cf_polar_t p;
    p.range_m = range_m;
    p.az_rad  = atan2(d.u, d.w);
    p.el_rad  = atan2(d.v, sqrt(d.u * d.u + d.w * d.w));   /* asin 보다 안정 */
    return p;
}

/*============================================================================
 *  2단계  안테나  <->  동체(Body)
 *--------------------------------------------------------------------------
 *  고정형 안테나이므로 회전행렬은 설치 도면이 정하는 '상수'다.
 *      C_body_from_ant = Rz(설치방위) * Ry(백틸트)
 *  그리고 원점이 다르므로 레버암(무게중심 -> 안테나)을 더한다.
 *==========================================================================*/
cf_mat3_t cf_dcm_body_from_antenna(const cf_mount_t *mount)
{
    if (mount == NULL) {                       /* 방어: 단위행렬 반환 */
        cf_mat3_t I = { { {1,0,0},{0,1,0},{0,0,1} } };
        return I;
    }
    return cf_mat_mul(cf_rot_z(mount->yaw_rad), cf_rot_y(mount->pitch_rad));
}

cf_vec3_t cf_body_from_antenna(const cf_mount_t *mount, cf_vec3_t p_ant)
{
    cf_vec3_t rotated = cf_mat_apply(cf_dcm_body_from_antenna(mount), p_ant);
    if (mount == NULL) return rotated;
    return cf_vec_add(rotated, mount->lever_arm_b);       /* + 레버암 */
}

cf_vec3_t cf_antenna_from_body(const cf_mount_t *mount, cf_vec3_t p_body)
{
    cf_vec3_t shifted = p_body;
    if (mount != NULL) shifted = cf_vec_sub(p_body, mount->lever_arm_b); /* - 레버암 */
    return cf_mat_apply(cf_mat_trans(cf_dcm_body_from_antenna(mount)), shifted);
}

/*============================================================================
 *  3단계  동체  <->  로컬(NED)
 *--------------------------------------------------------------------------
 *  C_ned_from_body = Rz(yaw) * Ry(pitch) * Rx(roll)     (3-2-1 오일러)
 *==========================================================================*/
cf_mat3_t cf_dcm_ned_from_body(const cf_attitude_t *att)
{
    if (att == NULL) {
        cf_mat3_t I = { { {1,0,0},{0,1,0},{0,0,1} } };
        return I;
    }
    return cf_mat_mul(cf_mat_mul(cf_rot_z(att->yaw_rad),
                                 cf_rot_y(att->pitch_rad)),
                      cf_rot_x(att->roll_rad));
}

cf_vec3_t cf_ned_from_body(const cf_attitude_t *att, cf_vec3_t p_body)
{
    return cf_mat_apply(cf_dcm_ned_from_body(att), p_body);
}

cf_vec3_t cf_body_from_ned(const cf_attitude_t *att, cf_vec3_t p_ned)
{
    return cf_mat_apply(cf_mat_trans(cf_dcm_ned_from_body(att)), p_ned);
}

/*============================================================================
 *  4단계  로컬 <-> ECEF,  LLA <-> ECEF
 *==========================================================================*/
cf_mat3_t cf_dcm_ned_from_ecef(double lat_rad, double lon_rad)
{
    const double sp = sin(lat_rad), cp = cos(lat_rad);
    const double sl = sin(lon_rad), cl = cos(lon_rad);
    cf_mat3_t R = { { { -sp*cl, -sp*sl,  cp },
                      {   -sl ,    cl ,  0.0},
                      { -cp*cl, -cp*sl, -sp } } };
    return R;
}

cf_vec3_t cf_ecef_from_geodetic(cf_geodetic_t g)
{
    const double sp = sin(g.lat_rad), cp = cos(g.lat_rad);
    const double sl = sin(g.lon_rad), cl = cos(g.lon_rad);
    /* N: 묘유선 곡률반경 — 그 위도에서 지구가 얼마나 '뚱뚱한가' */
    const double N  = CF_WGS84_A / sqrt(1.0 - CF_WGS84_E2 * sp * sp);
    cf_vec3_t p;
    p.x = (N + g.alt_m) * cp * cl;
    p.y = (N + g.alt_m) * cp * sl;
    p.z = (N * (1.0 - CF_WGS84_E2) + g.alt_m) * sp;   /* (1-e^2) 누락 주의 */
    return p;
}

/*  ECEF -> LLA : Bowring 폐형식(1회 반복)
 *  지상~수십 km 고도에서 오차 1e-5 m 미만. 반복문이 필요 없다.        */
cf_status_t cf_geodetic_from_ecef(cf_vec3_t p, cf_geodetic_t *out)
{
    double rho, theta, st, ct, sp, N, h;
    int    iter;

    if (out == NULL) return CF_ERR_NULL;

    rho = sqrt(p.x * p.x + p.y * p.y);          /* 자전축까지의 거리 */

    if (rho < 1e-9) {                            /* 극점 특이점 처리 */
        out->lat_rad = (p.z < 0.0) ? -CF_PI/2.0 : CF_PI/2.0;
        out->lon_rad = 0.0;                      /* 극에서 경도는 정의되지 않음 */
        out->alt_m   = fabs(p.z) - CF_WGS84_B;
        return CF_OK;
    }

    out->lon_rad = atan2(p.y, p.x);

    /*  (1) Bowring 폐형식으로 초기 위도를 구한다. 지상~40 km 에서는
     *      이 한 줄만으로도 오차가 1e-5 m 미만이다.                       */
    theta = atan2(p.z * CF_WGS84_A, rho * CF_WGS84_B);   /* 보조 규약위도 */
    st = sin(theta); ct = cos(theta);
    out->lat_rad = atan2(p.z   + CF_WGS84_EP2 * CF_WGS84_B * st * st * st,
                         rho   - CF_WGS84_E2  * CF_WGS84_A * ct * ct * ct);

    /*  (2) Hirvonen 고정점 반복으로 다듬는다. 고고도(수백 km) 표적에서도
     *      기계정밀도까지 수렴한다. 2회면 충분하다.                       */
    for (iter = 0; iter < CF_ECEF2LLA_REFINE; ++iter) {
        sp = sin(out->lat_rad);
        N  = CF_WGS84_A / sqrt(1.0 - CF_WGS84_E2 * sp * sp);
        h  = rho * cos(out->lat_rad) + (p.z + CF_WGS84_E2 * N * sp) * sp - N;
        out->lat_rad = atan2(p.z, rho * (1.0 - CF_WGS84_E2 * N / (N + h)));
    }

    sp = sin(out->lat_rad);
    N  = CF_WGS84_A / sqrt(1.0 - CF_WGS84_E2 * sp * sp);

    /*  h = rho/cos(lat) - N  대신 아래 항등식을 쓴다.
     *  1/cos(lat) 이 없으므로 고위도·극점에서도 안전하다.
     *      rho*cos(lat) + (z + e^2*N*sin(lat))*sin(lat) = N + h            */
    out->alt_m = rho * cos(out->lat_rad)
               + (p.z + CF_WGS84_E2 * N * sp) * sp
               - N;
    return CF_OK;
}

cf_vec3_t cf_ecef_from_ned(cf_geodetic_t origin, cf_vec3_t p_ned)
{
    cf_mat3_t C_ecef_from_ned =
        cf_mat_trans(cf_dcm_ned_from_ecef(origin.lat_rad, origin.lon_rad));
    return cf_vec_add(cf_ecef_from_geodetic(origin),
                      cf_mat_apply(C_ecef_from_ned, p_ned));
}

cf_vec3_t cf_ned_from_ecef(cf_geodetic_t origin, cf_vec3_t p_ecef)
{
    cf_vec3_t d = cf_vec_sub(p_ecef, cf_ecef_from_geodetic(origin));
    return cf_mat_apply(cf_dcm_ned_from_ecef(origin.lat_rad, origin.lon_rad), d);
}

/*============================================================================
 *  참고: ECEF <-> ECI  (지구 자전만 고려한 간이 변환)
 *--------------------------------------------------------------------------
 *  이 설계(함선 표적 위치 산출)에는 쓰이지 않지만, 1.5 절 내용을
 *  코드로 확인할 수 있도록 포함한다. 세차·장동·극운동은 생략했다.
 *==========================================================================*/
double cf_gmst_rad(double jd_ut1)
{
    const double d = jd_ut1 - 2451545.0;         /* J2000.0 로부터의 일수 */
    const double T = d / 36525.0;                /* 율리우스 세기         */
    double deg = 280.46061837 + 360.98564736629 * d
               + 0.000387933 * T * T - (T * T * T) / 38710000.0;
    deg = fmod(deg, 360.0);
    if (deg < 0.0) deg += 360.0;
    return CF_DEG2RAD(deg);
}

cf_vec3_t cf_eci_from_ecef(cf_vec3_t p_ecef, double gmst_rad)
{
    return cf_mat_apply(cf_rot_z(gmst_rad), p_ecef);
}

cf_vec3_t cf_ecef_from_eci(cf_vec3_t p_eci, double gmst_rad)
{
    return cf_mat_apply(cf_mat_trans(cf_rot_z(gmst_rad)), p_eci);
}

/*============================================================================
 *  전체 체인
 *==========================================================================*/
cf_status_t cf_forward_chain(cf_polar_t           meas,
                             const cf_mount_t    *mount,
                             const cf_attitude_t *att,
                             cf_geodetic_t        platform,
                             cf_geodetic_t       *target_out,
                             cf_chain_t          *chain_out)
{
    cf_chain_t c;
    cf_status_t st;

    if (mount == NULL || att == NULL || target_out == NULL) return CF_ERR_NULL;
    if (meas.range_m <= 0.0)                                return CF_ERR_DOMAIN;

    memset(&c, 0, sizeof(c));

    /* 1단계 — 안테나 극좌표 -> 직교(+UV) */
    c.dircos           = cf_dircos_from_polar(meas);
    c.p_antenna        = cf_antcart_from_polar(meas);

    /* 2단계 — 안테나 -> 동체 (회전 + 레버암) */
    c.C_body_from_ant  = cf_dcm_body_from_antenna(mount);
    c.p_body           = cf_body_from_antenna(mount, c.p_antenna);

    /* 3단계 — 동체 -> 로컬 NED (자세 회전) */
    c.C_ned_from_body  = cf_dcm_ned_from_body(att);
    c.p_ned            = cf_ned_from_body(att, c.p_body);

    /* 4단계 — 로컬 -> ECEF */
    c.p_ecef_platform  = cf_ecef_from_geodetic(platform);
    c.p_ecef_target    = cf_ecef_from_ned(platform, c.p_ned);

    /* 5단계 — ECEF -> LLA */
    st = cf_geodetic_from_ecef(c.p_ecef_target, &c.target);
    if (st != CF_OK) return st;

    c.ground_range_m   = sqrt(c.p_ned.x * c.p_ned.x + c.p_ned.y * c.p_ned.y);
    c.true_bearing_rad = atan2(c.p_ned.y, c.p_ned.x);
    if (c.true_bearing_rad < 0.0) c.true_bearing_rad += 2.0 * CF_PI;

    *target_out = c.target;
    if (chain_out != NULL) *chain_out = c;
    return CF_OK;
}

cf_status_t cf_inverse_chain(cf_geodetic_t        target,
                             const cf_mount_t    *mount,
                             const cf_attitude_t *att,
                             cf_geodetic_t        platform,
                             cf_polar_t          *meas_out)
{
    cf_vec3_t p_ecef, p_ned, p_body, p_ant;

    if (mount == NULL || att == NULL || meas_out == NULL) return CF_ERR_NULL;

    p_ecef = cf_ecef_from_geodetic(target);         /* LLA  -> ECEF   */
    p_ned  = cf_ned_from_ecef(platform, p_ecef);    /* ECEF -> 로컬   */
    p_body = cf_body_from_ned(att, p_ned);          /* 로컬 -> 동체   (전치) */
    p_ant  = cf_antenna_from_body(mount, p_body);   /* 동체 -> 안테나 (전치 + 레버암 빼기) */

    *meas_out = cf_polar_from_antcart(p_ant);       /* 직교 -> 극좌표 */
    return CF_OK;
}
