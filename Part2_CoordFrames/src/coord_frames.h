/*==============================================================================
 *  coord_frames.h  —  레이다 좌표계 변환 라이브러리
 *------------------------------------------------------------------------------
 *  Chapter 2. 좌표계, 좌표변환  /  3. C 코딩을 통한 검증
 *
 *  [ 좌표계 축 약속 ]  이 라이브러리는 모든 프레임에서 z 를 "아래"로 둔다.
 *
 *    안테나(Antenna)  x = 보어사이트(안테나가 바라보는 방향)
 *                     y = 안테나의 오른쪽
 *                     z = 아래
 *    동체(Body, FRD)  x = 선수(앞)   y = 우현(오른쪽)   z = 아래
 *    로컬(NED)        x = 진북(N)    y = 동(E)          z = 아래(D)
 *    ECEF             X = 적도∩그리니치자오선  Y = 오른손계  Z = 북극
 *
 *  [ 회전행렬 표기 ]  이름에 방향을 명시한다.
 *      C_ned_from_body  :  v_ned = C_ned_from_body * v_body
 *      역변환은 전치(transpose).  C_body_from_ned = C_ned_from_body^T
 *
 *  [ 각도 단위 ]  구조체·함수 인자는 전부 라디안. 도(degree)는 입출력 경계에서만.
 *
 *  [ 오일러각 순서 ]  3-2-1 (yaw -> pitch -> roll)
 *      C_ned_from_body = Rz(yaw) * Ry(pitch) * Rx(roll)
 *============================================================================*/
#ifndef COORD_FRAMES_H
#define COORD_FRAMES_H

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------
 *  WGS-84 지구 타원체 상수  (GPS·해도가 쓰는 국제 표준)
 *--------------------------------------------------------------------------*/
#define CF_WGS84_A      6378137.0                        /* 장반경 [m]        */
#define CF_WGS84_F_INV  298.257223563                    /* 편평률의 역수     */
#define CF_WGS84_F      (1.0 / CF_WGS84_F_INV)           /* 편평률            */
#define CF_WGS84_B      (CF_WGS84_A * (1.0 - CF_WGS84_F))/* 단반경 [m]        */
#define CF_WGS84_E2     (CF_WGS84_F * (2.0 - CF_WGS84_F))/* 제1이심률 제곱    */
#define CF_WGS84_EP2    (CF_WGS84_E2 / (1.0 - CF_WGS84_E2)) /* 제2이심률 제곱 */
#define CF_WGS84_OMEGA  7.292115e-5                      /* 자전각속도[rad/s] */

/** ECEF->LLA 개선 반복 횟수. 0이면 Bowring 1회만(지상~40 km 충분),
 *  2면 고고도(수백 km) 표적까지 기계정밀도. */
#define CF_ECEF2LLA_REFINE  2

#define CF_PI           3.14159265358979323846
#define CF_DEG2RAD(d)   ((d) * (CF_PI / 180.0))
#define CF_RAD2DEG(r)   ((r) * (180.0 / CF_PI))

/*----------------------------------------------------------------------------
 *  기본 타입
 *--------------------------------------------------------------------------*/
typedef struct { double x, y, z; } cf_vec3_t;   /* 3차원 벡터            */
typedef struct { double m[3][3];  } cf_mat3_t;  /* 3x3 행렬 (행 우선)    */

/** 안테나 극좌표 — 신호처리가 넘겨주는 원측정값 */
typedef struct {
    double range_m;   /**< 시선거리(Slant Range) [m]                        */
    double az_rad;    /**< 방위각: 보어사이트에서 오른쪽(+)으로 잰 각 [rad] */
    double el_rad;    /**< 고각  : 안테나 수평면에서 위(+)로 잰 각   [rad]  */
} cf_polar_t;

/** 방향코사인 (UV 좌표) — 단위 지향벡터를 안테나 면에 정사영한 값 */
typedef struct {
    double u;  /**< 안테나 면 가로(오른쪽) 성분,  u = cos(El)*sin(Az) */
    double v;  /**< 안테나 면 세로(위)     성분,  v = sin(El)         */
    double w;  /**< 보어사이트 성분,             w = cos(El)*cos(Az)  */
} cf_dircos_t;

/** 측지 좌표 (LLA) */
typedef struct {
    double lat_rad;  /**< 측지 위도 [rad]                        */
    double lon_rad;  /**< 경도      [rad]                        */
    double alt_m;    /**< 타원체고(HAE) [m] — 해발(MSL) 아님     */
} cf_geodetic_t;

/** 플랫폼 자세 (NED 기준 3-2-1 오일러각) */
typedef struct {
    double roll_rad;   /**< 좌우 기울기, 우현이 내려가면 +  */
    double pitch_rad;  /**< 앞뒤 기울기, 선수가 들리면 +    */
    double yaw_rad;    /**< 선수 방위(heading), 진북에서 시계방향 + */
} cf_attitude_t;

/** 안테나 설치 정보 — 고정형이므로 상수 */
typedef struct {
    double     yaw_rad;      /**< 설치 방위: 선수 기준 시계방향 [rad]     */
    double     pitch_rad;    /**< 백틸트: 보어사이트를 들어올린 각 [rad]  */
    cf_vec3_t  lever_arm_b;  /**< 무게중심 -> 안테나 위상중심, 동체 FRD [m]
                                  예) 30 m 뒤·10 m 위 => {-30, 0, -10}    */
} cf_mount_t;

/** 상태 코드 */
typedef enum {
    CF_OK          =  0,
    CF_ERR_NULL    = -1,   /**< 널 포인터                       */
    CF_ERR_DOMAIN  = -2,   /**< 정의역 위반 (예: 거리 0 또는 음수) */
    CF_ERR_SINGULAR= -3    /**< 특이점에서 값을 정의할 수 없음   */
} cf_status_t;

/** 전 단계 중간값 — 보고서/디버깅용으로 한 번에 받아본다 */
typedef struct {
    cf_dircos_t   dircos;            /**< 1단계: 방향코사인(UV)        */
    cf_vec3_t     p_antenna;         /**< 1단계: 안테나 직교 [m]       */
    cf_mat3_t     C_body_from_ant;   /**< 2단계: 마운팅 회전행렬       */
    cf_vec3_t     p_body;            /**< 2단계: 동체 [m]              */
    cf_mat3_t     C_ned_from_body;   /**< 3단계: 자세 회전행렬         */
    cf_vec3_t     p_ned;             /**< 3단계: 로컬 NED [m]          */
    cf_vec3_t     p_ecef_platform;   /**< 4단계: 플랫폼 ECEF [m]       */
    cf_vec3_t     p_ecef_target;     /**< 4단계: 표적 ECEF [m]         */
    cf_geodetic_t target;            /**< 5단계: 표적 LLA              */
    double        ground_range_m;    /**< 부수: 로컬 수평거리 [m]      */
    double        true_bearing_rad;  /**< 부수: 진북 기준 방위 [rad]   */
} cf_chain_t;

/*============================================================================
 *  1. 벡터·행렬 기본 연산
 *==========================================================================*/
cf_vec3_t cf_vec_add  (cf_vec3_t a, cf_vec3_t b);
cf_vec3_t cf_vec_sub  (cf_vec3_t a, cf_vec3_t b);
double    cf_vec_norm (cf_vec3_t a);

cf_mat3_t cf_rot_x    (double angle_rad);   /**< x축 둘레 우수 회전 */
cf_mat3_t cf_rot_y    (double angle_rad);   /**< y축 둘레 우수 회전 */
cf_mat3_t cf_rot_z    (double angle_rad);   /**< z축 둘레 우수 회전 */
cf_mat3_t cf_mat_mul  (cf_mat3_t A, cf_mat3_t B);
cf_vec3_t cf_mat_apply(cf_mat3_t A, cf_vec3_t v);
cf_mat3_t cf_mat_trans(cf_mat3_t A);
double    cf_mat_det  (cf_mat3_t A);
/** |A^T*A - I| 의 최대 절대값. 0에 가까울수록 정상적인 회전행렬. */
double    cf_mat_orthonormality_error(cf_mat3_t A);

/*============================================================================
 *  2. 단계별 변환
 *==========================================================================*/
/* --- 1단계: 안테나 극좌표 <-> 직교 <-> UV --------------------------------*/
cf_vec3_t   cf_antcart_from_polar (cf_polar_t p);
cf_polar_t  cf_polar_from_antcart (cf_vec3_t p);
cf_dircos_t cf_dircos_from_polar  (cf_polar_t p);
cf_polar_t  cf_polar_from_dircos  (cf_dircos_t d, double range_m);

/* --- 2단계: 안테나 <-> 동체 ---------------------------------------------*/
cf_mat3_t cf_dcm_body_from_antenna(const cf_mount_t *mount);
cf_vec3_t cf_body_from_antenna    (const cf_mount_t *mount, cf_vec3_t p_ant);
cf_vec3_t cf_antenna_from_body    (const cf_mount_t *mount, cf_vec3_t p_body);

/* --- 3단계: 동체 <-> 로컬(NED) ------------------------------------------*/
cf_mat3_t cf_dcm_ned_from_body(const cf_attitude_t *att);
cf_vec3_t cf_ned_from_body    (const cf_attitude_t *att, cf_vec3_t p_body);
cf_vec3_t cf_body_from_ned    (const cf_attitude_t *att, cf_vec3_t p_ned);

/* --- 4단계: 로컬 <-> ECEF, LLA <-> ECEF ---------------------------------*/
cf_mat3_t   cf_dcm_ned_from_ecef  (double lat_rad, double lon_rad);
cf_vec3_t   cf_ecef_from_geodetic (cf_geodetic_t g);
cf_status_t cf_geodetic_from_ecef (cf_vec3_t p_ecef, cf_geodetic_t *out);
cf_vec3_t   cf_ecef_from_ned      (cf_geodetic_t origin, cf_vec3_t p_ned);
cf_vec3_t   cf_ned_from_ecef      (cf_geodetic_t origin, cf_vec3_t p_ecef);

/* --- 참고: ECEF <-> ECI (지구 자전만 고려한 간이 변환) ------------------*/
double    cf_gmst_rad     (double julian_date_ut1);
cf_vec3_t cf_eci_from_ecef(cf_vec3_t p_ecef, double gmst_rad);
cf_vec3_t cf_ecef_from_eci(cf_vec3_t p_eci,  double gmst_rad);

/*============================================================================
 *  3. 전체 체인 (정변환 / 역변환)
 *==========================================================================*/
/** 신호처리 측정값 -> 표적 LLA. 중간값이 필요하면 chain 에 받는다(널 허용). */
cf_status_t cf_forward_chain(cf_polar_t          meas,
                             const cf_mount_t   *mount,
                             const cf_attitude_t*att,
                             cf_geodetic_t       platform,
                             cf_geodetic_t      *target_out,
                             cf_chain_t         *chain_out);

/** 표적 LLA -> 안테나 극좌표 (통제제어 출력 2번 항목). */
cf_status_t cf_inverse_chain(cf_geodetic_t       target,
                             const cf_mount_t   *mount,
                             const cf_attitude_t*att,
                             cf_geodetic_t       platform,
                             cf_polar_t         *meas_out);

#ifdef __cplusplus
}
#endif
#endif /* COORD_FRAMES_H */
