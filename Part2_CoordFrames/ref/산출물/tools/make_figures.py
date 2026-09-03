# -*- coding: utf-8 -*-
"""발표자료 그림 생성기. 7장(회전)과 10장(안테나 극좌표) 그림을 PNG 로 뽑는다.

    python make_figures.py              # 두 장 다
    python make_figures.py polar        # 10장 (fig02_polar.png)
    python make_figures.py rotate       # 7장 (fig05_same_target.png)

결과는 ../../참고 자료/figures/ 에 저장되고 같은 파일이 pptx 에 들어간다.
글꼴은 맑은 고딕이 있으면 그걸 쓰고, 없으면(리눅스) 나눔바른고딕을 쓴다. build_combo.py 와 같은 규칙이다.
각도가 눈에 보이는 대로여야 하므로 그림 영역은 전부 aspect="equal" 로 두고 데이터 좌표에서 직접 그린다.
"""
import math
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import font_manager as fm
from matplotlib import patheffects as pe
from matplotlib.patches import Arc, Circle, FancyArrowPatch, Polygon

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.normpath(os.path.join(HERE, "..", "..", "참고 자료", "figures"))

FONT_CANDIDATES = [
    (r"C:\Windows\Fonts\malgun.ttf", r"C:\Windows\Fonts\malgunbd.ttf"),
    ("/usr/share/fonts/truetype/nanum/NanumBarunGothic.ttf",
     "/usr/share/fonts/truetype/nanum/NanumBarunGothicBold.ttf"),
    ("/usr/share/fonts/truetype/nanum/NanumGothic.ttf",
     "/usr/share/fonts/truetype/nanum/NanumGothicBold.ttf"),
]
for _reg, _bold in FONT_CANDIDATES:
    if os.path.exists(_reg) and os.path.exists(_bold):
        REG, BOLD = fm.FontProperties(fname=_reg), fm.FontProperties(fname=_bold)
        break
else:
    sys.exit("한글 글꼴을 못 찾음 (맑은 고딕 / 나눔바른고딕 / 나눔고딕)")

# 색은 다른 그림들이 쓰는 값 그대로
NAVY = "#18202F"        # 제목 · 본문 글자
GREY = "#5A6478"        # 부제 · 설명 글자
FAINT = "#8A93A8"       # 보조선 · 흐린 글자
BLUE = "#2B57A6"        # 축 · 선체 외곽선
HULL = "#E2EAF7"        # 선체 채움
DECK = "#C7D6EE"        # 갑판 구조물 채움
ORANGE = "#C2521C"      # 표적 · 측정값
WATER = "#9FBADF"       # 물결
DPI = 170


# ---------------------------------------------------------------- 캔버스 도구
def canvas(px_w, px_h):
    fig = plt.figure(figsize=(px_w / DPI, px_h / DPI), dpi=DPI, facecolor="white")
    lay = fig.add_axes([0, 0, 1, 1], zorder=10)     # 글자 전용 층 (그림 좌표 0~1)
    lay.set_xlim(0, 1)
    lay.set_ylim(0, 1)
    lay.set_axis_off()
    lay.patch.set_alpha(0)
    return fig, lay


def stage(fig, rect, xlim, ylim):
    """rect = [left, bottom, w, h] (그림 비율). 데이터 좌표는 xlim/ylim, 가로세로 1:1"""
    ax = fig.add_axes(rect)
    ax.set_xlim(*xlim)
    ax.set_ylim(*ylim)
    ax.set_aspect("equal", adjustable="box")
    ax.set_axis_off()
    ax.patch.set_alpha(0)
    return ax


def text(ax, x, y, s, size=11, color=NAVY, bold=False, ha="left", va="center", **kw):
    return ax.text(x, y, s, fontproperties=(BOLD if bold else REG), fontsize=size,
                   color=color, ha=ha, va=va, **kw)


def arrow(ax, p0, p1, color=BLUE, lw=1.9, head=10.0, z=5, ls="-", halo=False):
    """halo=True 면 흰 테두리를 둘러 배 위를 지나가도 화살표가 끊겨 보이지 않게 한다"""
    a = FancyArrowPatch(p0, p1, arrowstyle="-|>", mutation_scale=head, lw=lw,
                        color=color, linestyle=ls, shrinkA=0, shrinkB=0,
                        zorder=z, joinstyle="miter")
    if halo:
        a.set_path_effects([pe.withStroke(linewidth=lw + 2.2, foreground="white")])
    ax.add_patch(a)


def line(ax, p0, p1, color=FAINT, lw=1.0, ls="-", z=2, dashes=None):
    ln, = ax.plot([p0[0], p1[0]], [p0[1], p1[1]], color=color, lw=lw, ls=ls, zorder=z,
                  solid_capstyle="butt")
    if dashes:
        ln.set_dashes(dashes)
    return ln


def poly(ax, pts, fc=HULL, ec=BLUE, lw=1.6, z=3, closed=True):
    ax.add_patch(Polygon(pts, closed=closed, facecolor=fc, edgecolor=ec, lw=lw, zorder=z,
                         joinstyle="round"))


def arcdeg(ax, c, r, a0, a1, color=ORANGE, lw=1.7, z=6):
    ax.add_patch(Arc(c, 2 * r, 2 * r, theta1=a0, theta2=a1, color=color, lw=lw, zorder=z))


def at(c, r, deg):
    return (c[0] + r * math.cos(math.radians(deg)), c[1] + r * math.sin(math.radians(deg)))


# ------------------------------------------------------------------ 배 모양
# 배 길이를 1.0 으로 본 비율. 선미가 0.0, 선수가 1.0. 구축함 정도 비율(길이:폭 = 8:1)
HALF_BEAM = 0.062       # 평면도 선체 반폭 (배 길이 기준). HULL_TOP 의 최대 |v| 와 같다
HULL_TOP = [(0.00, -0.049), (0.00, 0.049), (0.13, 0.059), (0.56, 0.062),
            (0.73, 0.058), (0.87, 0.045), (0.955, 0.023), (1.00, 0.000),
            (0.955, -0.023), (0.87, -0.045), (0.73, -0.058), (0.56, -0.062),
            (0.13, -0.059)]
HULL_SIDE = [(0.00, -0.018), (0.00, 0.042), (0.34, 0.042), (0.66, 0.048),
             (0.86, 0.060), (1.00, 0.086), (0.988, 0.020), (0.940, -0.010),
             (0.80, -0.026), (0.45, -0.034), (0.14, -0.033)]


def ship_top(ax, x_stern, y_axis, length, z=3, wake=True, ang=0.0):
    """위에서 내려다본 배. ang = 선수가 향하는 각(0 이면 오른쪽, 90 이면 위쪽).
    (x_stern, y_axis) 는 ang = 0 일 때의 선미 중앙이고, 회전은 그 점을 축으로 한다"""
    ca, sa = math.cos(math.radians(ang)), math.sin(math.radians(ang))

    def P(u, v):
        dx, dy = u * length, v * length
        return (x_stern + dx * ca - dy * sa, y_axis + dx * sa + dy * ca)

    poly(ax, [P(u, v) for u, v in HULL_TOP], fc=HULL, ec=BLUE, lw=1.7, z=z)
    poly(ax, [P(0.22, -0.033), P(0.22, 0.033), P(0.36, 0.033), P(0.36, -0.033)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 격납고
    poly(ax, [P(0.40, -0.026), P(0.40, 0.026), P(0.50, 0.026), P(0.50, -0.026)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 연돌
    poly(ax, [P(0.54, -0.038), P(0.54, 0.038), P(0.72, 0.038), P(0.72, -0.038)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 함교
    poly(ax, [P(0.80, -0.026), P(0.80, 0.026), P(0.88, 0.026), P(0.88, -0.026)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 함포
    line(ax, P(0.18, -0.052), P(0.18, 0.052), color=BLUE, lw=0.9, z=z + 1)  # 비행갑판 경계
    ax.add_patch(Circle(P(0.09, 0.0), 0.034 * length, facecolor="none", edgecolor=BLUE,
                        lw=0.9, zorder=z + 1))                    # 헬기 갑판
    if wake:
        for k in (1, 2):
            d = 0.045 * k
            line(ax, P(-0.01, 0.045), P(-0.13 - d, 0.070 + d), color=WATER, lw=1.1, z=z - 1)
            line(ax, P(-0.01, -0.045), P(-0.13 - d, -0.070 - d), color=WATER, lw=1.1, z=z - 1)
    return P


def ship_side(ax, x0, y_water, length, z=3, flip=False):
    """옆에서 본 배. flip=False 면 선수가 오른쪽, True 면 왼쪽. 흘수선은 y = y_water"""
    def P(u, v):
        uu = (1.0 - u) if flip else u
        return (x0 + uu * length, y_water + v * length)

    poly(ax, [P(u, v) for u, v in HULL_SIDE], fc=HULL, ec=BLUE, lw=1.7, z=z)
    poly(ax, [P(0.16, 0.041), P(0.16, 0.104), P(0.76, 0.104), P(0.76, 0.052)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 갑판실
    poly(ax, [P(0.545, 0.104), P(0.545, 0.180), P(0.700, 0.180), P(0.755, 0.104)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 함교
    poly(ax, [P(0.400, 0.104), P(0.378, 0.198), P(0.442, 0.198), P(0.478, 0.104)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 연돌
    poly(ax, [P(0.78, 0.050), P(0.78, 0.088), P(0.875, 0.088), P(0.875, 0.050)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 함포
    poly(ax, [P(0.548, 0.180), P(0.556, 0.322), P(0.584, 0.322), P(0.592, 0.180)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 마스트
    ax.plot(*zip(P(0.524, 0.272), P(0.616, 0.272)), color=BLUE, lw=1.1, zorder=z + 2)
    return P


# 선미 쪽에서 본 배. 배 폭을 1.0 으로 본 비율. 흘수선이 0.0, 위가 +, 화면 오른쪽이 우현
HULL_STERN = [(-0.20, -0.34), (-0.36, -0.26), (-0.45, -0.10), (-0.48, 0.10),
              (-0.50, 0.42), (0.50, 0.42), (0.48, 0.10), (0.45, -0.10),
              (0.36, -0.26), (0.20, -0.34)]


def ship_stern(ax, x_center, y_water, beam, z=3):
    """배 뒤에서 선수 쪽을 본 모습. 배가 관측자를 등지고 있어 화면 오른쪽이 우현이다.
    보어사이트가 우현이면 El 이 놓인 수직면이 곧 이 화면이라 각을 그대로 그릴 수 있다"""
    def P(u, v):
        return (x_center + u * beam, y_water + v * beam)

    poly(ax, [P(u, v) for u, v in HULL_STERN], fc=HULL, ec=BLUE, lw=1.7, z=z)
    poly(ax, [P(-0.17, 0.10), P(-0.17, 0.38), P(0.17, 0.38), P(0.17, 0.10)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 격납고 문
    poly(ax, [P(-0.40, 0.42), P(-0.40, 0.72), P(0.40, 0.72), P(0.40, 0.42)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 갑판실
    poly(ax, [P(-0.27, 0.72), P(-0.27, 1.02), P(0.27, 1.02), P(0.27, 0.72)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 상부 구조물
    poly(ax, [P(-0.12, 1.02), P(-0.12, 1.20), P(0.12, 1.20), P(0.12, 1.02)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 마스트 받침
    poly(ax, [P(-0.038, 1.20), P(-0.028, 1.58), P(0.028, 1.58), P(0.038, 1.20)],
         fc=DECK, ec=BLUE, lw=1.0, z=z + 1)                       # 마스트
    line(ax, P(-0.15, 1.40), P(0.15, 1.40), color=BLUE, lw=1.1, z=z + 2)
    return P


def sea_line(ax, x0, x1, y, z=1, tick=None):
    """수면. 잔물결을 아래쪽에 몇 개 그어서 '옆에서 본 그림' 임을 알려 준다"""
    line(ax, (x0, y), (x1, y), color=WATER, lw=1.5, z=z)
    tick = tick or (x1 - x0) * 0.012
    step = (x1 - x0) / 22.0
    for i in range(22):
        if i % 3 == 1:
            continue
        xa = x0 + i * step
        line(ax, (xa, y - tick), (xa + step * 0.5, y - tick), color=WATER, lw=1.0, z=z)


def antenna_face(ax, c, deg, half=3.4, thick=1.5, z=8, color=BLUE):
    """안테나 면. deg 는 보어사이트 방향, 면은 거기 수직"""
    n = math.radians(deg)
    ux, uy = math.cos(n + math.pi / 2), math.sin(n + math.pi / 2)
    vx, vy = math.cos(n), math.sin(n)
    pts = [(c[0] + ux * half - vx * thick / 2, c[1] + uy * half - vy * thick / 2),
           (c[0] + ux * half + vx * thick / 2, c[1] + uy * half + vy * thick / 2),
           (c[0] - ux * half + vx * thick / 2, c[1] - uy * half + vy * thick / 2),
           (c[0] - ux * half - vx * thick / 2, c[1] - uy * half - vy * thick / 2)]
    poly(ax, pts, fc=color, ec=color, lw=1.0, z=z)


def save(fig, name):
    if not os.path.isdir(OUT_DIR):
        sys.exit("figures 폴더를 못 찾음: %s" % OUT_DIR)
    path = os.path.join(OUT_DIR, name)
    fig.savefig(path, dpi=DPI, facecolor="white")
    plt.close(fig)
    print("생성: %s" % path)


# ============================================================== 10장 : 극좌표
def draw_polar():
    """안테나 극좌표 R / Az / El.

    (a) 는 갑판을 내려다본 그림이다. 안테나는 과제 조건대로 우현(선수 기준 시계방향 90°)을
    보고 있으므로 선수는 위, 보어사이트는 오른쪽, 안테나 기준 오른쪽(y)은 선미 쪽이 된다.

    (b) 는 배 뒤에서 선수 쪽을 본 그림이다. 보어사이트가 우현이라 El 이 놓인 수직면은 배의
    횡단면이고, 그 면을 정면으로 보려면 시선이 선수-선미 축과 나란해야 한다. 선미 쪽에서 보면
    화면 오른쪽이 그대로 우현 = 보어사이트가 되어 (a) 와 좌우가 맞고 El 도 제 각으로 그려진다.
    선체 측면도로 그리면 화면 오른쪽이 선미가 되어 보어사이트가 선미를 향하는 것처럼 읽힌다.
    """
    W, H = 1615, 760
    fig, lay = canvas(W, H)

    box = (0.020, 0.028, 0.462, 0.812)
    ar = (box[3] * H) / (box[2] * W)
    TOP = 100.0 * ar
    axA = stage(fig, list(box), (0, 100), (0, TOP))
    axB = stage(fig, [0.518, box[1], box[2], box[3]], (0, 100), (0, TOP))
    line(lay, (0.501, 0.050), (0.501, 0.900), color="#E4E7ED", lw=1.0, z=1)

    L, OX = 38.0, 28.0                  # 배 길이, 안테나(원점) x — 두 패널 공통
    XAX, ZAX, YAX, RAY = 56.0, 21.0, 40.0, 46.0
    AZ, EL = 30.0, 25.0
    HALF = HALF_BEAM * L

    # ------------------------------------------------------ (a) 위에서 본 그림
    yc = TOP * 0.605                                   # 안테나 y
    O = (OX, yc)
    cx = OX - 1.6 * HALF                               # 선체 중심선. 축이 뱃전 선에 먹히지 않게 띄운다
    ship_top(axA, cx, yc - 0.32 * L, L, ang=90.0)
    antenna_face(axA, O, 0.0, half=2.4, thick=1.3)

    arrow(axA, O, (OX + XAX, yc), color=BLUE, lw=1.9, z=6)
    arrow(axA, O, (OX, yc - YAX), color=BLUE, lw=1.9, z=6)
    text(axA, OX + XAX, yc + 3.0, "x  보어사이트", size=11.5, color=BLUE, bold=True,
         ha="right", va="bottom")
    text(axA, OX + 3.0, yc - YAX + 1.5, "y  오른쪽 (선미 쪽)", size=11.5, color=BLUE,
         bold=True, ha="left", va="center")

    tgt = at(O, RAY, -AZ)
    arrow(axA, O, tgt, color=ORANGE, lw=2.2, head=11, z=7)
    axA.add_patch(Circle(tgt, 1.5, facecolor=ORANGE, edgecolor="none", zorder=8))
    text(axA, tgt[0] + 3.0, tgt[1] - 1.6, "표적", size=11.5, color=NAVY, bold=True, va="top")

    arcdeg(axA, O, 13.5, -AZ, 0.0)
    text(axA, *at(O, 19.0, -AZ / 2), s="Az", size=13, color=ORANGE, bold=True,
         ha="center", va="center")
    text(axA, *at(O, RAY * 0.58, -AZ + 7.5), s="R", size=13, color=ORANGE, bold=True,
         ha="center", va="center")

    text(axA, cx, yc + 0.70 * L, "선수", size=10, color=GREY, ha="center", va="bottom")
    text(axA, cx - HALF - 2.0, yc - 0.26 * L, "선미", size=10, color=GREY, ha="right",
         va="center")
    text(axA, OX - HALF * 2 - 3.0, yc, "안테나", size=10.5, color=BLUE, bold=True,
         ha="right", va="center")

    vy = yc - 0.32 * L - 12.0
    arrow(axA, (cx, vy), (cx, vy + 9.0), color=FAINT, lw=1.3, head=9, z=2)
    text(axA, cx - 2.6, vy + 4.5, "(b)", size=10.5, color=GREY, bold=True,
         ha="right", va="center")
    text(axA, 2.0, 4.0, "(b) 는 이 배를 화살표 방향으로 본 모습이다.", size=10.5,
         color=GREY, va="center")

    # ------------------------------------------------------ (b) 뒤에서 본 그림
    B = 24.0                                           # 배 폭 (뒤에서 본 그림은 폭 기준)
    ZB = 1.40 * B                                      # z 축은 선저 아래까지 뽑아 라벨을 띄운다
    yw = TOP * 0.300                                   # 흘수선
    sea_line(axB, 1.5, 98.5, yw, tick=1.7)
    ship_stern(axB, OX - 0.27 * B, yw, B)              # 안테나는 상부 구조물 우현 면
    ya = yw + 0.95 * B
    Ob = (OX, ya)
    antenna_face(axB, Ob, 0.0, half=2.4, thick=1.3)

    arrow(axB, Ob, (OX + XAX, ya), color=BLUE, lw=1.9, z=9)
    arrow(axB, Ob, (OX, ya - ZB), color=BLUE, lw=1.9, z=9, halo=True)
    text(axB, OX + XAX, ya + 3.0, "수평 (보어사이트)", size=11.5, color=BLUE, bold=True,
         ha="right", va="bottom")
    text(axB, OX + 3.0, ya - ZB + 1.5, "z  아래", size=11.5, color=BLUE, bold=True,
         ha="left", va="center")

    tb = at(Ob, RAY, EL)
    line(axB, (tb[0], ya), tb, color="#BFC5D0", lw=1.0, dashes=(4, 3), z=2)
    arrow(axB, Ob, tb, color=ORANGE, lw=2.2, head=11, z=7)
    axB.add_patch(Circle(tb, 1.5, facecolor=ORANGE, edgecolor="none", zorder=8))
    text(axB, tb[0] + 3.0, tb[1] + 1.6, "표적", size=11.5, color=NAVY, bold=True, va="bottom")

    arcdeg(axB, Ob, 13.5, 0.0, EL)
    text(axB, *at(Ob, 19.0, EL / 2), s="El", size=13, color=ORANGE, bold=True,
         ha="center", va="center")
    text(axB, *at(Ob, RAY * 0.58, EL + 7.5), s="R", size=13, color=ORANGE, bold=True,
         ha="center", va="center")

    text(axB, OX + 3.4, ya - 5.2, "안테나", size=10.5, color=BLUE, bold=True,
         ha="left", va="center")
    text(axB, 97.0, yw - 3.6, "해수면", size=10.5, color=GREY, ha="right", va="center")
    text(axB, 2.0, 4.0, "z 는 아래가 +.  표적이 수평면보다 위에 있으면 z 는 음수가 된다.",
         size=10.5, color=GREY, va="center")

    # ------------------------------------------------------ 머리글
    text(lay, 0.038, 0.955, "(a) 위에서 본 그림 — 방위각 Az", size=14.5, bold=True)
    text(lay, 0.038, 0.913, "갑판을 내려다본 모습.  보어사이트에서 오른쪽으로 잰 각",
         size=11, color=GREY)
    text(lay, 0.536, 0.955, "(b) 뒤에서 본 그림 — 고각 El", size=14.5, bold=True)
    text(lay, 0.536, 0.913, "같은 배를 선미 쪽에서 본 모습.  수평면에서 위로 잰 각",
         size=11, color=GREY)

    save(fig, "fig02_polar.png")


# ================================================================ 7장 : 회전
def draw_rotate():
    """같은 배 · 같은 표적을 안테나 축과 함선 축에서 각각 읽으면 숫자가 어떻게 달라지는가"""
    W, H = 1496, 760
    fig, lay = canvas(W, H)

    box = (0.024, 0.180, 0.452, 0.688)
    ar = (box[3] * H) / (box[2] * W)
    TOP = 100.0 * ar
    axA = stage(fig, list(box), (0, 100), (0, TOP))
    axB = stage(fig, [0.524, box[1], box[2], box[3]], (0, 100), (0, TOP))

    L = 26.0                            # 배 길이
    OX, OY = 26.0, TOP * 0.50           # 안테나 = 두 그림 공통 원점
    AXL, AXS = 60.0, 33.0               # 긴 축 / 짧은 축
    SC = 50.0 / 17320.0                 # m 를 그림 단위로
    TX, TY = OX + 17320.0 * SC, OY - 10000.0 * SC

    def scene(ax):
        """두 그림에 똑같이 들어가는 것 : 배, 안테나, 표적까지의 화살표"""
        ship_top(ax, OX - 1.6 * HALF_BEAM * L, OY - 0.44 * L, L, ang=90.0, wake=False)
        antenna_face(ax, (OX, OY), 0.0, half=2.2, thick=1.3, color=ORANGE)
        arrow(ax, (OX, OY), (TX, TY), color=ORANGE, lw=2.3, head=11, z=7)
        ax.add_patch(Circle((TX, TY), 1.5, facecolor=ORANGE, edgecolor="none", zorder=8))
        text(ax, TX + 3.0, TY - 1.6, "표적", size=12, color=NAVY, bold=True, va="top")
        text(ax, OX - 4.4, OY, "안테나", size=10, color=ORANGE, bold=True, ha="right",
             va="center")
        text(ax, OX - 1.6 * HALF_BEAM * L - 3.2, OY + 0.46 * L, "선수", size=10, color=GREY,
             ha="right", va="center")

    def guides(ax, tone, ylab, note=None, back=False):
        """표적에서 두 축으로 수선을 내리고 읽은 값을 붙인다.
        back=True 면 성분이 축 화살표와 반대쪽이라는 뜻이라 그 구간을 점선으로 덧그린다"""
        line(ax, (TX, TY), (TX, OY), color="#D0D5DE", lw=1.0, dashes=(4, 3), z=2)
        line(ax, (TX, TY), (OX, TY), color="#D0D5DE", lw=1.0, dashes=(4, 3), z=2)
        if back:
            arrow(ax, (OX, OY), (OX, TY), color=tone, lw=1.6, head=9, z=6, ls=(0, (4, 3)))
        text(ax, (OX + TX) / 2 + 6.0, OY + 2.4, "+17 320", size=12, color=tone, bold=True,
             ha="center", va="bottom")
        text(ax, OX - 4.6, (OY + TY) / 2, ylab, size=12, color=tone, bold=True,
             ha="right", va="center")
        if note:
            text(ax, OX - 4.6, (OY + TY) / 2 - 4.8, note, size=9.5, color=GREY,
                 ha="right", va="center")

    # ------------------------------------------------- (a) 안테나 축에서 읽으면
    scene(axA)
    arrow(axA, (OX, OY), (OX + AXL, OY), color=ORANGE, lw=1.9, z=6)
    arrow(axA, (OX, OY), (OX, OY - AXS), color=ORANGE, lw=1.9, z=6)
    text(axA, OX + AXL + 2.0, OY, "x_a", size=12.5, color=ORANGE, bold=True, va="center")
    text(axA, OX + 2.6, OY - AXS + 1.2, "y_a", size=12.5, color=ORANGE, bold=True,
         ha="left", va="center")
    guides(axA, ORANGE, "+10 000")

    # ------------------------------------------------- (b) 함선 축에서 읽으면
    scene(axB)
    arrow(axB, (OX, OY), (OX, OY + AXS), color=BLUE, lw=1.9, z=6)
    arrow(axB, (OX, OY), (OX + AXL, OY), color=BLUE, lw=1.9, z=6)
    text(axB, OX + 2.6, OY + AXS - 1.2, "x_b", size=12.5, color=BLUE, bold=True,
         ha="left", va="center")
    text(axB, OX + AXL + 2.0, OY, "y_b", size=12.5, color=BLUE, bold=True, va="center")
    guides(axB, BLUE, "-10 000", note="(선수 기준 뒤쪽)", back=True)

    # ------------------------------------------------------ 머리글 · 결론 · 캡션
    text(lay, 0.040, 0.950, "(a) 안테나 축에서 읽으면", size=14.5, bold=True)
    text(lay, 0.040, 0.901, "x_a = 보어사이트 (우현 쪽),   y_a = 그 오른쪽 (선미 쪽)",
         size=11, color=GREY)
    text(lay, 0.540, 0.950, "(b) 함선 축에서 읽으면", size=14.5, bold=True)
    text(lay, 0.540, 0.901, "x_b = 선수,   y_b = 우현    (x_a 와 y_b 는 같은 방향)",
         size=11, color=GREY)
    text(lay, 0.040, 0.134, "읽은 값 :  x_a = +17 320 m,   y_a = +10 000 m",
         size=12.5, color=ORANGE, bold=True)
    text(lay, 0.540, 0.134, "읽은 값 :  x_b = -10 000 m,   y_b = +17 320 m",
         size=12.5, color=BLUE, bold=True)
    text(lay, 0.500, 0.052, "배도 표적도 그대로다.  축만 90° 돌아가 있다.", size=12.5,
         color=NAVY, ha="center")

    save(fig, "fig05_same_target.png")


if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "all"
    if what not in ("all", "polar", "rotate"):
        sys.exit("사용법: python make_figures.py [all|polar|rotate]")
    if what in ("all", "polar"):
        draw_polar()
    if what in ("all", "rotate"):
        draw_rotate()
