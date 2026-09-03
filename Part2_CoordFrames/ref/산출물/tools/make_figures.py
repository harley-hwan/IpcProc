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
from matplotlib.patches import Arc, Circle, FancyArrowPatch, Polygon, Wedge

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
GREEN = "#2F855A"       # 진북 · NED 계열
WATER = "#9FBADF"       # 물결

# 어두운 배경 슬라이드(2장)용. 배경 #12223E 위에 얹는다
DK_TEXT = "#FFFFFF"     # 굵은 글자
DK_MUTE = "#8FA6CC"     # 설명 글자 (카드 라벨과 같은 색)
DK_GRID = "#5A79AC"     # 격자 · 거리 링
DK_BLUE = "#9CC0F0"     # 안테나 · 배
DK_ORANGE = "#F0894C"   # 표적 · 측정값
DPI = 170


# ---------------------------------------------------------------- 캔버스 도구
def canvas(px_w, px_h, bg="white"):
    """bg=None 이면 배경을 비운다 (어두운 슬라이드에 얹을 때)"""
    fig = plt.figure(figsize=(px_w / DPI, px_h / DPI), dpi=DPI,
                     facecolor=("none" if bg is None else bg))
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


def ship_side(ax, x0, y_water, length, z=3, flip=False, ang=0.0):
    """옆에서 본 배. flip=False 면 선수가 오른쪽, True 면 왼쪽. 흘수선은 y = y_water.
    ang 은 배 한가운데를 축으로 한 기울기(도). 선수가 들리는 쪽이 +"""
    ca, sa = math.cos(math.radians(ang)), math.sin(math.radians(ang))
    px, py = x0 + 0.5 * length, y_water

    def P(u, v):
        uu = (1.0 - u) if flip else u
        dx, dy = x0 + uu * length - px, v * length
        return (px + dx * ca - dy * sa, py + dx * sa + dy * ca)

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


def ship_stern(ax, x_center, y_water, beam, z=3, ang=0.0):
    """배 뒤에서 선수 쪽을 본 모습. 배가 관측자를 등지고 있어 화면 오른쪽이 우현이다.
    보어사이트가 우현이면 El 이 놓인 수직면이 곧 이 화면이라 각을 그대로 그릴 수 있다.
    ang 은 흘수선 한가운데를 축으로 한 횡경사(도). 우현(화면 오른쪽)이 내려가는 쪽이 +"""
    ca, sa = math.cos(math.radians(-ang)), math.sin(math.radians(-ang))

    def P(u, v):
        dx, dy = u * beam, v * beam
        return (x_center + dx * ca - dy * sa, y_water + dx * sa + dy * ca)

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


# ---------------------------------------------------------------- 사선(축측) 투영
# 수평면을 위에서 비스듬히 내려다본 평행투영. 오른손 좌표계가 눈으로도 오른손으로 보인다.
# 첫째 축은 뒤로 물러나고(북 · 선수), 둘째 축은 오른쪽 앞으로 나오고(동 · 우현),
# 셋째 축은 화면 수직이다(아래 또는 위).
ISO_FWD = (0.500, 0.420)
ISO_RGT = (0.940, -0.342)


def iso(o, f, r, d=0.0):
    """평면 위 (앞으로 f, 오른쪽으로 r) 과 아래로 d 를 화면 좌표로 옮긴다"""
    return (o[0] + f * ISO_FWD[0] + r * ISO_RGT[0],
            o[1] + f * ISO_FWD[1] + r * ISO_RGT[1] - d)


def iso_axis(ax, o, key, length, color, label, num=None, tone=None, size=11.5, z=6):
    """원점에서 뻗는 축 하나. key 는 fwd / rgt / down / up"""
    d = {"fwd": ISO_FWD, "rgt": ISO_RGT, "down": (0.0, -1.0), "up": (0.0, 1.0)}[key]
    tip = (o[0] + d[0] * length, o[1] + d[1] * length)
    arrow(ax, o, tip, color=color, lw=2.0, head=11, z=z)
    q = (o[0] + d[0] * length * 1.20, o[1] + d[1] * length * 1.20)
    text(ax, q[0], q[1], label, size=size, color=color, bold=True, ha="center", va="center")
    if num:                                             # 몇 번째로 세는 축인가
        h = math.hypot(*d)
        c = (o[0] + d[0] * length * 0.60 - d[1] / h * 4.4,
             o[1] + d[1] * length * 0.60 + d[0] / h * 4.4)
        ax.add_patch(Circle(c, 2.6, facecolor="white", edgecolor=tone, lw=1.3, zorder=z + 1))
        text(ax, c[0], c[1] - 0.1, num, size=10.5, color=tone, bold=True, ha="center",
             va="center", zorder=z + 2)
    return tip


def ship_iso(ax, o, length, z=3):
    """사선으로 본 배. 갑판은 앞-오른쪽 평면에 놓이고 선체는 아래로 두껍게 그린다.

    P(f, s, d) 에서 f 는 선미 0 ~ 선수 1, s 는 우현 쪽 거리, d 는 아래로 내려간 거리다.
    (모두 배 길이를 1 로 본 비율)
    """
    hull_d = 0.052 * length

    def P(f, s, d=0.0):
        return iso(o, (f - 0.42) * length, s * length, d * length)

    deck = [P(f, -v) for f, v in HULL_TOP]              # 위에서 본 그림은 +v 가 좌현이다
    keel = [P(f, -v, 0.052) for f, v in HULL_TOP]
    poly(ax, keel, fc="#CFDBEE", ec=BLUE, lw=1.0, z=z)
    for i in range(len(deck)):                          # 뱃전. 갑판에 가려 앞쪽만 남는다
        j = (i + 1) % len(deck)
        poly(ax, [deck[i], deck[j], keel[j], keel[i]], fc="#CFDBEE", ec=BLUE, lw=0.9,
             z=z + 1)
    poly(ax, deck, fc=HULL, ec=BLUE, lw=1.6, z=z + 2)

    def box(f0, f1, w, h):
        """갑판 위 구조물. 윗면과 보이는 두 옆면만 그리면 입체로 보인다"""
        A, D_ = P(f0, -w), P(f0, w)
        C = P(f1, w)
        At, Dt, Ct = P(f0, -w, -h), P(f0, w, -h), P(f1, w, -h)
        Bt = P(f1, -w, -h)
        poly(ax, [D_, C, Ct, Dt], fc="#B9CBE6", ec=BLUE, lw=0.8, z=z + 3)   # 우현 면
        poly(ax, [A, D_, Dt, At], fc="#C7D6EE", ec=BLUE, lw=0.8, z=z + 3)   # 선미 면
        poly(ax, [At, Bt, Ct, Dt], fc=DECK, ec=BLUE, lw=0.9, z=z + 4)       # 윗면

    box(0.22, 0.36, 0.033, 0.030)                       # 격납고
    box(0.40, 0.50, 0.026, 0.038)                       # 연돌
    box(0.54, 0.72, 0.038, 0.050)                       # 함교
    box(0.80, 0.88, 0.026, 0.022)                       # 함포
    line(ax, P(0.18, -0.052), P(0.18, 0.052), color=BLUE, lw=0.9, z=z + 3)
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


def axis_mark(ax, c, r, into=True, color=BLUE, lw=1.3, z=8):
    """화면을 뚫는 축 표시. into=True 면 들어가는 방향 ⊗, False 면 나오는 방향 ⊙"""
    ax.add_patch(Circle(c, r, facecolor="white", edgecolor=color, lw=lw, zorder=z))
    if into:
        d = r * 0.70
        line(ax, (c[0] - d, c[1] - d), (c[0] + d, c[1] + d), color=color, lw=lw, z=z + 1)
        line(ax, (c[0] - d, c[1] + d), (c[0] + d, c[1] - d), color=color, lw=lw, z=z + 1)
    else:
        ax.add_patch(Circle(c, r * 0.30, facecolor=color, edgecolor="none", zorder=z + 1))


def curve_arrow(ax, p0, p1, rad=0.35, color=ORANGE, lw=1.8, head=11, z=6):
    """회전 방향을 나타내는 굽은 화살표"""
    ax.add_patch(FancyArrowPatch(p0, p1, arrowstyle="-|>", mutation_scale=head, lw=lw,
                                 color=color, shrinkA=0, shrinkB=0, zorder=z,
                                 connectionstyle="arc3,rad=%.3f" % rad))


def num(v, sign=True):
    """천 단위마다 쉼표를 넣어 자릿수를 헷갈리지 않게 쓴다. 20000 -> 20,000"""
    body = "{:,.0f}".format(abs(v)) if float(v).is_integer() else "{:,}".format(abs(v))
    return ("-" if v < 0 else ("+" if sign else "")) + body


def blip(ax, c, r=1.5, color=None, z=5):
    """표적 점. 가운데 점에 흐린 후광을 둘러 어두운 배경에서도 눈에 띄게 한다"""
    color = color or DK_ORANGE
    ax.add_patch(Circle(c, r * 2.0, facecolor=color, edgecolor="none", alpha=0.30, zorder=z))
    ax.add_patch(Circle(c, r, facecolor=color, edgecolor="none", zorder=z + 1))


def save(fig, name, bg="white"):
    """bg=None 이면 배경 없이(투명하게) 저장한다"""
    if not os.path.isdir(OUT_DIR):
        sys.exit("figures 폴더를 못 찾음: %s" % OUT_DIR)
    path = os.path.join(OUT_DIR, name)
    if bg is None:
        fig.savefig(path, dpi=DPI, transparent=True)
    else:
        fig.savefig(path, dpi=DPI, facecolor=bg)
    plt.close(fig)
    print("생성: %s" % path)


# ============================================== 2장 : 레이다 화면 vs 전시기 화면
SHIP_LLA = (127.307, 36.408)        # 과제 조건의 함선 위치
TGT_LLA = (127.3643, 36.2337)       # 같은 표적을 위경도로 옮긴 값 (31장 최종 출력)


def draw_intro():
    """같은 표적을 레이다는 부채꼴 화면 위 한 점으로, 전시기는 지도 위 한 점으로 찍는다.

    2장은 배경이 어두운 슬라이드라 배경을 비우고 밝은 색으로 그린다.
    왼쪽은 안테나 정면을 기준으로 잰 거리와 각도, 오른쪽은 지구를 기준으로 잰 위경도다.
    지도는 가로세로가 실제 거리 비율과 맞도록 경도 범위를 상자 모양에서 역산해 정한다.
    """
    W, H = 1955, 320
    fig, lay = canvas(W, H, bg=None)

    box = (0.012, 0.040, 0.415, 0.930)
    ar = (box[3] * H) / (box[2] * W)
    TOP = 100.0 * ar
    axA = stage(fig, list(box), (0, 100), (0, TOP))
    axB = stage(fig, [0.573, box[1], box[2], box[3]], (0, 100), (0, TOP))

    # ------------------------------------------------ 왼쪽 : 레이다가 보는 것
    O = (30.0, TOP * 0.82)
    R_OUT, R_IN, AZ = 44.0, 22.0, 30.0
    axA.add_patch(Wedge(O, R_OUT, -34, 6, facecolor=DK_BLUE, edgecolor="none",
                        alpha=0.14, zorder=1))
    for r, lab in ((R_IN, "10 km"), (R_OUT, "20 km")):
        axA.add_patch(Arc(O, 2 * r, 2 * r, theta1=-34, theta2=6, color=DK_GRID,
                          lw=1.0, zorder=2))
        text(axA, O[0] + r, O[1] + 1.6, lab, size=9, color=DK_GRID, ha="center",
             va="bottom")
    line(axA, O, at(O, R_OUT + 6.0, 0.0), color=DK_GRID, lw=1.0, dashes=(5, 4), z=2)
    text(axA, O[0] + R_OUT + 7.5, O[1], "안테나 정면", size=10, color=DK_MUTE, va="center")

    tgt = at(O, R_OUT, -AZ)
    arrow(axA, O, tgt, color=DK_ORANGE, lw=2.2, head=11, z=5)
    blip(axA, tgt)
    text(axA, tgt[0] + 3.6, tgt[1], "표적", size=11, color=DK_TEXT, bold=True, va="center")
    arcdeg(axA, O, 13.0, -AZ, 0.0, color=DK_ORANGE, lw=1.5)
    text(axA, *at(O, 18.5, -AZ / 2), s="30°", size=11, color=DK_ORANGE, bold=True,
         ha="center", va="center")
    text(axA, *at(O, R_OUT * 0.56, -AZ + 9.0), s="20 km", size=11, color=DK_ORANGE,
         bold=True, ha="center", va="center")

    antenna_face(axA, O, 0.0, half=3.0, thick=1.6, color=DK_BLUE)
    text(axA, O[0] - 4.5, O[1], "안테나", size=10, color=DK_BLUE, bold=True,
         ha="right", va="center")

    # ------------------------------------------------ 오른쪽 : 전시기가 보여주는 것
    X0, X1 = 18.0, 82.0
    Y0, Y1 = 0.11 * TOP, 0.89 * TOP
    LAT0, LAT1, LON_MID = 36.12, 36.48, 127.35
    lat_km = (LAT1 - LAT0) * 111.13
    lon_deg = (lat_km * (X1 - X0) / (Y1 - Y0)) / (111.32 * math.cos(math.radians(36.3)))
    LON0, LON1 = LON_MID - lon_deg / 2, LON_MID + lon_deg / 2

    def M(lon, lat):
        return (X0 + (lon - LON0) / (LON1 - LON0) * (X1 - X0),
                Y0 + (lat - LAT0) / (LAT1 - LAT0) * (Y1 - Y0))

    poly(axB, [(X0, Y0), (X1, Y0), (X1, Y1), (X0, Y1)], fc="#1B2E50", ec=DK_GRID,
         lw=1.2, z=1)
    ship, tb = M(*SHIP_LLA), M(*TGT_LLA)
    step = 0.2                                                 # 경도 눈금 간격
    for k in range(int(math.ceil(LON0 / step)), int(math.floor(LON1 / step)) + 1):
        lon = round(k * step, 2)
        line(axB, M(lon, LAT0), M(lon, LAT1), color=DK_GRID, lw=0.8, z=2)
        if abs(M(lon, LAT0)[0] - tb[0]) > 13.0:                # 표적 눈금과 겹치면 생략
            text(axB, M(lon, LAT0)[0], Y0 - 2.4, "%.1f°E" % lon, size=8.5,
                 color=DK_GRID, ha="center", va="top")
    for lat in (36.2, 36.3, 36.4):
        line(axB, M(LON0, lat), M(LON1, lat), color=DK_GRID, lw=0.8, z=2)
        if abs(M(LON0, lat)[1] - tb[1]) > 4.0:
            text(axB, X0 - 2.0, M(LON0, lat)[1], "%.1f°N" % lat, size=8.5,
                 color=DK_GRID, ha="right", va="center")

    line(axB, ship, tb, color=DK_MUTE, lw=1.0, dashes=(3, 3), z=3)
    line(axB, tb, (X0, tb[1]), color=DK_ORANGE, lw=1.0, dashes=(4, 3), z=3)
    line(axB, tb, (tb[0], Y0), color=DK_ORANGE, lw=1.0, dashes=(4, 3), z=3)

    ca, sa = math.cos(math.radians(-45.0)), math.sin(math.radians(-45.0))   # 함수 45°
    hull = [(0.0, 3.0), (1.6, -0.4), (1.2, -2.5), (-1.2, -2.5), (-1.6, -0.4)]
    poly(axB, [(ship[0] + u * ca - v * sa, ship[1] + u * sa + v * ca)
               for u, v in hull], fc=DK_BLUE, ec=DK_BLUE, lw=1.0, z=5)
    text(axB, ship[0] - 3.6, ship[1] + 1.4, "우리 배", size=10, color=DK_BLUE, bold=True,
         ha="right", va="center")

    blip(axB, tb)
    text(axB, tb[0] + 3.6, tb[1] + 1.2, "표적", size=11, color=DK_TEXT, bold=True,
         ha="left", va="bottom")
    text(axB, X0 - 2.0, tb[1], "36.2337°N", size=10, color=DK_ORANGE, bold=True,
         ha="right", va="center")
    text(axB, tb[0], Y0 - 2.4, "127.3643°E", size=10, color=DK_ORANGE, bold=True,
         ha="center", va="top")

    # ------------------------------------------------ 사이 화살표
    arrow(lay, (0.462, 0.50), (0.538, 0.50), color=DK_MUTE, lw=2.4, head=16, z=5)

    save(fig, "fig15_scope_vs_chart.png", bg=None)


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
    text(lay, 0.038, 0.955, "(a) 위에서 본 그림 — 방위각 Az (Azimuth)", size=14.5, bold=True)
    text(lay, 0.038, 0.913, "갑판을 내려다본 모습.  보어사이트에서 오른쪽으로 잰 각",
         size=11, color=GREY)
    text(lay, 0.536, 0.955, "(b) 뒤에서 본 그림 — 고각 El (Elevation)", size=14.5, bold=True)
    text(lay, 0.536, 0.913, "같은 배를 선미 쪽에서 본 모습.  수평면에서 위로 잰 각",
         size=11, color=GREY)

    save(fig, "fig02_polar.png")


# ================================================ 4장 : 한 표적을 여러 기준에서 재기
def draw_one_target():
    """같은 표적 하나를 안테나 · 함선 · 진북 세 기준에서 재면 각도만 30 · 120 · 165 로
    달라진다는 것을 왼쪽에, 그 표적이 지구에서는 어디인지를 오른쪽에 그린다.
    """
    W, H = 1580, 910
    fig, lay = canvas(W, H)
    axL = stage(fig, [0.020, 0.050, 0.500, 0.900], (0, 100),
                (0, 100 * (0.900 * H) / (0.500 * W)))
    axR = stage(fig, [0.580, 0.190, 0.360, 0.620], (0, 100),
                (0, 100 * (0.620 * H) / (0.360 * W)))

    # ------------------------------------------------- 왼쪽 : 세 기준에서 잰 각
    CG, L = (30.0, 60.0), 24.0
    bow, bore = 45.0, -45.0                            # 화면 각도. 선수 45°, 보어사이트 우현
    los = bore - 30.0

    for ang_, lab in ((90.0, "N"), (0.0, "E")):
        arrow(axL, CG, at(CG, 18.0, ang_), color=GREEN, lw=1.8, head=10, z=5)
        text(axL, *at(CG, 20.5, ang_), s=lab, size=12, color=GREEN, bold=True,
             ha="center", va="center")
    line(axL, at(CG, 18.0, 90.0), at(CG, 25.0, 90.0), color=FAINT, lw=1.1,
         dashes=(4, 3), z=2)
    text(axL, CG[0], CG[1] + 26.0, "진북", size=11.5, color=NAVY, bold=True,
         ha="center", va="bottom")

    rad = math.radians(bow)
    P = ship_top(axL, CG[0] - 0.42 * L * math.cos(rad), CG[1] - 0.42 * L * math.sin(rad),
                 L, ang=bow, wake=False)
    # 축 이름은 호(弧) 바깥쪽에 두어 호와 글자가 겹치지 않게 한다
    arrow(axL, CG, at(CG, 35.0, bow), color=BLUE, lw=1.8, head=10, z=6)
    text(axL, *at(CG, 36.5, bow), s="x_b 선수", size=11.5, color=BLUE, bold=True,
         ha="left", va="bottom")
    tip = at(CG, 35.0, bore)                           # 보어사이트와 우현은 같은 방향이다
    arrow(axL, CG, tip, color=BLUE, lw=1.8, head=10, z=6)
    text(axL, tip[0] + 1.6, tip[1] + 1.4, "y_b 우현", size=11.5, color=BLUE, bold=True,
         ha="left", va="bottom")
    text(axL, tip[0] + 1.6, tip[1] - 1.4, "x_a 보어사이트", size=11.5, color=ORANGE,
         bold=True, ha="left", va="top")

    ant = P(0.34, -0.062)
    antenna_face(axL, ant, bore, half=2.4, thick=1.4, color=ORANGE)
    arrow(axL, ant, at(ant, 16.0, bore - 90.0), color=ORANGE, lw=1.8, head=10, z=6)
    text(axL, *at(ant, 17.5, bore - 90.0), s="y_a", size=11.5, color=ORANGE, bold=True,
         ha="right", va="center")

    tgt = at(ant, 42.0, los)
    arrow(axL, ant, tgt, color=NAVY, lw=2.4, head=12, z=6)
    axL.add_patch(Circle(tgt, 1.8, facecolor=NAVY, edgecolor="none", zorder=8))
    text(axL, tgt[0] + 3.2, tgt[1] + 1.0, "표적은 하나", size=12.5, color=NAVY, bold=True,
         ha="left", va="bottom")
    text(axL, tgt[0] + 3.2, tgt[1] - 1.0, "안테나에서 20 km", size=10, color=FAINT,
         ha="left", va="top")

    arcs = ((32.0, los, 90.0, GREEN, "진북에서 165°", 62.0),
            (25.0, los, bow, BLUE, "선수에서 120°", 54.0),
            (18.0, los, bore, ORANGE, "보어사이트에서 30°", 46.0))
    for r, a0, a1, col, lab, ly in arcs:                # 라벨은 오른쪽에 세로로 모아 둔다
        arcdeg(axL, CG, r, a0, a1, color=col, lw=1.6)
        text(axL, 68.0, ly, lab, size=11, color=col, bold=True, ha="left", va="center")

    # ------------------------------------------------------- 오른쪽 : 지구에서 보면
    TR = axR.get_ylim()[1]
    C, R = (46.0, TR * 0.52), 30.0
    axR.add_patch(Circle(C, R, facecolor=HULL, edgecolor="#3D4A5F", lw=1.6, zorder=2))
    axR.add_patch(Arc(C, R * 0.9, 2 * R, theta1=0, theta2=360, color="#8A93A8",
                      lw=1.0, linestyle=(0, (4, 3)), zorder=3))
    line(axR, (C[0] - R, C[1]), (C[0] + R, C[1]), color="#8A93A8", lw=1.0,
         dashes=(5, 4), z=3)
    text(axR, C[0] - R + 2.0, C[1] + 2.0, "적도", size=10, color=FAINT, va="bottom")
    text(axR, C[0], C[1] - R - 2.5, "그리니치 자오선", size=10, color=FAINT,
         ha="center", va="top")

    arrow(axR, C, (C[0], C[1] + R + 10.0), color=NAVY, lw=1.7, head=10, z=6)
    arrow(axR, C, (C[0] + R + 12.0, C[1]), color=NAVY, lw=1.7, head=10, z=6)
    text(axR, C[0], C[1] + R + 11.5, "Z  북극", size=11.5, color=NAVY, bold=True,
         ha="center", va="bottom")
    text(axR, C[0] + R + 13.5, C[1], "X", size=11.5, color=NAVY, bold=True, va="center")
    axR.add_patch(Circle(C, 1.0, facecolor=NAVY, edgecolor="none", zorder=7))
    text(axR, C[0] + 2.0, C[1] - 4.0, "지구 중심 = ECEF 원점", size=10, color=GREY,
         va="center")

    ptt = at(C, R, 36.0)
    line(axR, C, ptt, color=ORANGE, lw=1.2, dashes=(4, 3), z=5)
    arcdeg(axR, C, 13.0, 0.0, 36.0, color=ORANGE, lw=1.4)
    text(axR, *at(C, 17.0, 18.0), s="위도", size=10.5, color=ORANGE, bold=True,
         ha="left", va="center")
    axR.add_patch(Circle(ptt, 2.0, facecolor=ORANGE, edgecolor="none", zorder=8))
    text(axR, ptt[0] + 3.0, ptt[1] + 3.0, "배와 표적", size=11, color=NAVY, bold=True,
         ha="left", va="bottom")
    text(axR, ptt[0] + 3.0, ptt[1] + 1.0, "북위 36°, 동경 127°", size=10, color=GREY,
         ha="left", va="top")

    text(lay, 0.580, 0.905, "지구에서 보면", size=13, color=NAVY, bold=True)
    text(lay, 0.580, 0.175, "NED  :  배 위치에서 잰 북 · 동 · 아래", size=11, color=GREY)
    text(lay, 0.580, 0.118, "ECEF :  지구 중심에서 잰 (X, Y, Z) m", size=11, color=GREY)
    text(lay, 0.580, 0.061, "LLA  :  위도 · 경도 · 고도 (지도 좌표)", size=11, color=GREY)
    text(lay, 0.500, 0.014,
         "NED = North-East-Down    ·    ECEF = Earth-Centered Earth-Fixed"
         "    ·    LLA = Latitude-Longitude-Altitude",
         size=10, color=FAINT, ha="center")

    save(fig, "fig14_one_target.png")


# ========================================================== 8장 : 회전행렬 유도
def draw_derive():
    """삼각함수 덧셈정리에서 2차원 회전식이 나오는 과정.

    슬라이드에서 7.50 in 로 줄여 놓기 때문에 그림 안 글자는 13 pt 근처로 잡아야
    화면에서 12 pt 정도로 읽힌다. 슬라이드 제목과 겹치는 그림 안 머리글은 두지 않는다.
    """
    W, H = 1334, 689
    fig, lay = canvas(W, H)
    ax = stage(fig, [0.020, 0.055, 0.345, 0.890], (0, 100),
               (0, 100 * (0.890 * H) / (0.345 * W)))

    O, R, A, T = (14.0, 22.0), 72.0, 25.0, 42.0        # 원점, 반지름, 처음 각, 더 돌린 각
    p = at(O, R, A)
    q = at(O, R, A + T)

    arrow(ax, O, (96.0, O[1]), color=GREY, lw=1.3, head=9, z=3)
    arrow(ax, O, (O[0], 118.0), color=GREY, lw=1.3, head=9, z=3)
    text(ax, 97.5, O[1], "x", size=12, color=GREY, va="center")
    text(ax, O[0], 119.5, "y", size=12, color=GREY, ha="center", va="bottom")

    for pt_, col in ((p, "#C9CDD8"), (q, "#E2C3AE")):
        line(ax, pt_, (pt_[0], O[1]), color=col, lw=1.0, dashes=(4, 3), z=2)
        line(ax, pt_, (O[0], pt_[1]), color=col, lw=1.0, dashes=(4, 3), z=2)

    arrow(ax, O, p, color=GREY, lw=2.0, head=11, z=5)
    arrow(ax, O, q, color=ORANGE, lw=2.3, head=12, z=6)
    ax.add_patch(Circle(p, 1.6, facecolor=GREY, edgecolor="none", zorder=7))
    ax.add_patch(Circle(q, 1.6, facecolor=ORANGE, edgecolor="none", zorder=7))
    text(ax, p[0] + 2.4, p[1] - 2.0, "P (x, y)", size=12, color=NAVY, bold=True, va="top")
    text(ax, q[0] + 2.4, q[1] + 2.0, "P' (x', y')", size=12, color=ORANGE, bold=True,
         va="bottom")

    arcdeg(ax, O, 22.0, 0.0, A, color=GREY, lw=1.4)
    text(ax, *at(O, 27.0, A / 2), s="α", size=13, color=GREY, ha="center", va="center")
    arcdeg(ax, O, 46.0, A, A + T, color=ORANGE, lw=1.6)
    text(ax, *at(O, 51.5, A + T / 2), s="θ", size=13.5, color=ORANGE, bold=True,
         ha="center", va="center")

    # ---------------------------------------------------------------- 유도 세 단계
    x0, y = 0.415, 0.905
    steps = [
        (GREY, 11, "원래 점을 극좌표로 쓰면", False),
        (NAVY, 12.5, "x = r·cos α ,   y = r·sin α", False),
        (None, 0, "", False),
        (GREY, 11, "θ 만큼 더 돌리면 각이 α+θ 가 되므로", False),
        (NAVY, 12.5, "x' = r·cos(α+θ) = r·cosα·cosθ - r·sinα·sinθ", False),
        (NAVY, 12.5, "y' = r·sin(α+θ) = r·sinα·cosθ + r·cosα·sinθ", False),
        (None, 0, "", False),
        (GREY, 11, "r·cosα = x,  r·sinα = y  를 되돌려 넣으면", False),
        (ORANGE, 13, "x' = x·cos θ - y·sin θ", True),
        (ORANGE, 13, "y' = x·sin θ + y·cos θ", True),
        (None, 0, "", False),
        (GREY, 11, "이 두 줄을 표로 적은 것이 Rz(θ) 다.", False),
    ]
    for col, size, txt, bold in steps:
        if col is None:
            y -= 0.040
            continue
        text(lay, x0, y, txt, size=size, color=col, bold=bold, va="top")
        y -= 0.072

    save(fig, "fig04_rot_derive.png")


# ============================================================ 13장 : 동체 좌표계
def draw_body():
    """동체(Body) 좌표계 FRD 와 자세각 세 가지.

    자세각 세 칸은 각각 다른 시점이라 배도 그 시점대로 그린다.
    roll 은 뒤에서 본 선미도, pitch 는 옆에서 본 측면도, yaw 는 위에서 본 평면도다.
    z 축은 화면 속으로 들어가므로 ⊙(나오는 방향) 이 아니라 ⊗ 로 표시한다.
    """
    W, H = 1378, 589
    fig, lay = canvas(W, H)

    axL = stage(fig, [0.020, 0.120, 0.400, 0.640], (0, 100), (0, 100 * (0.640 * H) / (0.400 * W)))
    axR = stage(fig, [0.455, 0.300, 0.530, 0.470], (0, 100), (0, 100 * (0.470 * H) / (0.530 * W)))

    # ------------------------------------------------------- 왼쪽 : FRD 정의
    # 사선으로 그려야 z 가 아래로 내려가는 것이 기호 없이 그대로 보인다
    TL = axL.get_ylim()[1]
    cg = (30.0, TL * 0.60)                              # 무게중심 = 원점
    ship_iso(axL, cg, 52.0)
    axL.add_patch(Circle(cg, 1.2, facecolor=NAVY, edgecolor="none", zorder=9))
    for key, ln, lab, ha, va, k in (("fwd", 32.0, "x  선수(앞)", "left", "bottom", 1.05),
                                    ("rgt", 30.0, "y  우현(오른쪽)", "left", "center", 1.06),
                                    ("down", 26.0, "z  아래", "center", "top", 1.10)):
        d = {"fwd": ISO_FWD, "rgt": ISO_RGT, "down": (0.0, -1.0)}[key]
        arrow(axL, cg, (cg[0] + d[0] * ln, cg[1] + d[1] * ln), color=BLUE, lw=2.0,
              head=11, z=9)
        text(axL, cg[0] + d[0] * ln * k + (1.5 if ha == "left" else 0.0),
             cg[1] + d[1] * ln * k - (1.5 if va == "top" else 0.0),
             lab, size=11, color=BLUE, bold=True, ha=ha, va=va)

    # ------------------------------------------------- 오른쪽 : 자세각 세 가지
    TR = axR.get_ylim()[1]
    base = TR * 0.46                                    # 세 칸 공통 기준선
    for cx in (17.0, 50.0, 83.0):
        line(axR, (cx - 15.0, base), (cx + 15.0, base), color="#C7CDD8", lw=1.0,
             dashes=(4, 3), z=1)

    # 굽은 화살표는 배 밑을 지나가고, 화살촉이 향하는 쪽이 곧 + 방향이다
    ship_stern(axR, 17.0, base, 15.0, ang=24.0)         # roll : 우현이 내려간 모습
    curve_arrow(axR, (17.0 - 13.0, base - 7.0), (17.0 + 13.0, base - 10.0), rad=-0.34)

    ship_side(axR, 50.0 - 17.0, base, 34.0, ang=15.0)   # pitch : 선수가 들린 모습
    curve_arrow(axR, (50.0 - 14.0, base - 10.0), (50.0 + 14.0, base - 4.0), rad=0.34)

    yc2 = base - 6.0
    ship_top(axR, 83.0, yc2, 30.0, ang=62.0, wake=False)           # yaw : 위에서 본 모습
    line(axR, (83.0, yc2), (83.0, yc2 + 21.0), color="#C7CDD8", lw=1.0, dashes=(4, 3), z=1)
    text(axR, 83.0, yc2 + 22.0, "진북", size=9, color=FAINT, ha="center", va="bottom")
    curve_arrow(axR, at((83.0, yc2), 14.5, 90.0), at((83.0, yc2), 14.5, 66.0), rad=-0.32)

    # ------------------------------------------------------------------ 글자
    # 오른쪽 제목이 0.455 에서 시작하므로 왼쪽 제목은 거기까지만 쓴다
    text(lay, 0.020, 0.945, "동체(Body) 좌표계 — FRD", size=13, bold=True)
    text(lay, 0.020, 0.905, "FRD = Forward-Right-Down.\n원점은 무게중심. 세 축은 배에 붙어 함께 움직인다.",
         size=9.5, color=FAINT, va="top", linespacing=1.5)
    text(lay, 0.455, 0.945, "자세각 세 가지", size=13, bold=True)
    text(lay, 0.455, 0.888, "C_ned←body = Rz(yaw) · Ry(pitch) · Rx(roll)   (3-2-1 순서)",
         size=11.5, color=ORANGE, bold=True)

    cols = (("roll  (횡동요)", "뒤에서 본 모습\nx축 둘레\n우현이 내려가면 +", 0.545),
            ("pitch (종동요)", "옆에서 본 모습\ny축 둘레\n선수가 들리면 +", 0.720),
            ("yaw   (선수방위)", "위에서 본 모습\nz축 둘레\n진북에서 시계방향 +", 0.895))
    for label, sub, fx in cols:
        text(lay, fx, 0.240, label, size=12.5, color=NAVY, bold=True, ha="center")
        text(lay, fx, 0.195, sub, size=9, color=FAINT, ha="center", va="top",
             linespacing=1.45)

    save(fig, "fig06_body.png")


# ====================================================== 25장 : 세 각이 더해지는 그림
def draw_layout():
    """진북에서 선수 45°, 선수에서 안테나 90°, 보어사이트에서 측정 30°.
    셋을 더하면 진북 기준 165° 가 된다는 것을 한 그림에서 보인다.
    """
    W, H = 918, 850
    fig, lay = canvas(W, H)
    ax = stage(fig, [0.030, 0.030, 0.940, 0.940], (0, 100),
               (0, 100 * (0.940 * H) / (0.940 * W)))

    CG, L = (40.0, 59.0), 34.0
    HEAD, MOUNT, AZ = 45.0, 90.0, 30.0                 # 선수 방위 · 설치 방위 · 측정 방위
    bow = 90.0 - HEAD                                  # 화면 각도로 바꾼 값
    bore = bow - MOUNT
    los = bore - AZ

    line(ax, CG, (CG[0], CG[1] + 24.0), color=FAINT, lw=1.2, dashes=(5, 4), z=2)
    arrow(ax, (CG[0], CG[1] + 20.0), (CG[0], CG[1] + 25.0), color=GREY, lw=1.4, head=9, z=3)
    text(ax, CG[0], CG[1] + 26.5, "진북", size=12, color=NAVY, bold=True, ha="center",
         va="bottom")

    rad = math.radians(bow)
    P = ship_top(ax, CG[0] - 0.42 * L * math.cos(rad), CG[1] - 0.42 * L * math.sin(rad),
                 L, ang=bow, wake=False)
    ax.add_patch(Circle(CG, 1.1, facecolor=BLUE, edgecolor="none", zorder=8))
    line(ax, CG, at(CG, 0.62 * L, bow), color=FAINT, lw=1.1, dashes=(5, 4), z=2)

    ant = P(0.34, -0.062)                              # 우현 뱃전
    antenna_face(ax, ant, bore, half=3.0, thick=1.6, color=ORANGE)

    # ① 과 ② 는 같은 반지름으로 이어 그려 진북 -> 선수 -> 보어사이트 가 한 흐름으로 보이게 한다
    arcdeg(ax, CG, 25.0, bow, 90.0, color=GREY, lw=1.5)
    text(ax, *at(CG, 27.0, 70.0), s="① 선수 45°", size=12, color=NAVY, bold=True,
         ha="left", va="bottom")
    arcdeg(ax, CG, 25.0, bore, bow, color=GREY, lw=1.5)
    text(ax, *at(CG, 27.0, 14.0), s="② 안테나 설치 90°", size=12, color=NAVY, bold=True,
         ha="left", va="center")

    # 보어사이트 화살표 위쪽에 글자를 놓아 시선(파란 화살표)과 겹치지 않게 한다
    arrow(ax, ant, at(ant, 24.0, bore), color=ORANGE, lw=1.8, head=11, z=6, ls=(0, (5, 3)))
    text(ax, *at(ant, 25.5, bore + 4.0), s="안테나가 보는 방향", size=11.5, color=ORANGE,
         bold=True, ha="left", va="bottom")
    arcdeg(ax, ant, 17.0, los, bore, color=ORANGE, lw=1.6)
    text(ax, *at(ant, 19.0, (los + bore) / 2), s="③ 측정 Az 30°", size=12,
         color=ORANGE, bold=True, ha="left", va="top")

    tgt = at(ant, 48.0, los)
    arrow(ax, ant, tgt, color=BLUE, lw=2.2, head=12, z=6)
    ax.add_patch(Circle(tgt, 1.6, facecolor=BLUE, edgecolor="none", zorder=8))
    text(ax, tgt[0] + 3.0, tgt[1] - 0.5, "표적  20 km", size=12, color=NAVY, bold=True,
         va="center")

    save(fig, "fig10_layout.png")


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
        arrow(ax, (OX, OY), (TX, TY), color=NAVY, lw=2.4, head=11, z=7)
        ax.add_patch(Circle((TX, TY), 1.6, facecolor=NAVY, edgecolor="none", zorder=8))
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
        text(ax, (OX + TX) / 2 + 6.0, OY + 2.4, "+17,320", size=12, color=tone, bold=True,
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
    guides(axA, ORANGE, "+10,000")

    # ------------------------------------------------- (b) 함선 축에서 읽으면
    scene(axB)
    arrow(axB, (OX, OY), (OX, OY + AXS), color=BLUE, lw=1.9, z=6)
    arrow(axB, (OX, OY), (OX + AXL, OY), color=BLUE, lw=1.9, z=6)
    text(axB, OX + 2.6, OY + AXS - 1.2, "x_b", size=12.5, color=BLUE, bold=True,
         ha="left", va="center")
    text(axB, OX + AXL + 2.0, OY, "y_b", size=12.5, color=BLUE, bold=True, va="center")
    guides(axB, BLUE, "-10,000", note="(선수 기준 뒤쪽)", back=True)

    # ------------------------------------------------------ 머리글 · 결론 · 캡션
    text(lay, 0.040, 0.950, "(a) 안테나 축에서 읽으면", size=14.5, bold=True)
    text(lay, 0.040, 0.901, "x_a = 보어사이트 (우현 쪽),   y_a = 그 오른쪽 (선미 쪽)",
         size=11, color=GREY)
    text(lay, 0.540, 0.950, "(b) 함선 축에서 읽으면", size=14.5, bold=True)
    text(lay, 0.540, 0.901, "x_b = 선수,   y_b = 우현    (x_a 와 y_b 는 같은 방향)",
         size=11, color=GREY)
    text(lay, 0.040, 0.134, "읽은 값 :  x_a = +17,320 m,   y_a = +10,000 m",
         size=12.5, color=ORANGE, bold=True)
    text(lay, 0.540, 0.134, "읽은 값 :  x_b = -10,000 m,   y_b = +17,320 m",
         size=12.5, color=BLUE, bold=True)
    text(lay, 0.500, 0.052, "배도 표적도 그대로다.  축만 90° 돌아가 있다.", size=12.5,
         color=NAVY, ha="center")

    save(fig, "fig05_same_target.png")


# ==================================================== 17장 : NED 와 ENU 는 같은 점
NED_TGT = (-19340.0, 5155.0, -10.0)     # 과제 값 (N, E, D). 26장 계산 결과를 반올림한 값


def draw_ned_enu():
    """NED 와 ENU 는 같은 점을 부르는 순서와 셋째 축의 방향만 다르다.

    북 · 동 두 축은 두 칸에 똑같이 그린다. 가리키는 방향이 실제로 같기 때문이다.
    셋째 축만 아래(D) 와 위(U) 로 뒤집히는데, 그것이 눈에 보이도록 사선으로 그린다.
    표적은 배보다 10 m 위에 있어 NED 에서는 D 가 음수, ENU 에서는 U 가 양수가 된다.
    """
    W, H = 1325, 553
    fig, lay = canvas(W, H)
    boxes = ([0.035, 0.205, 0.400, 0.595], [0.545, 0.205, 0.400, 0.595])
    TOP = 100 * (0.595 * H) / (0.400 * W)
    axA, axB = (stage(fig, list(b_), (0, 100), (0, TOP)) for b_ in boxes)

    O, L = (44.0, 0.53 * TOP), 19.0
    SC = 17.0 / 20000.0                                 # 20 km 를 평면 위 17 칸으로 본다
    RISE = 4.2                                          # 표적 높이. 보이라고 크게 부풀린 값

    def panel(order, ax, tone):
        # 국지 수평면(북 · 동이 놓인 평면)을 평행사변형으로 깔아 준다
        a_, b_ = 1.24 * L, 0.90 * L
        quad = [iso(O, sf * b_, sr * a_) for sf, sr in ((1, 1), (1, -1), (-1, -1), (-1, 1))]
        poly(ax, quad, fc="#EFF3FA", ec="#C7CFDD", lw=1.1, z=2)
        text(ax, quad[3][0] + 1.2, quad[3][1] - 1.4, "국지 수평면", size=9, color=FAINT,
             ha="left", va="top")

        for i, (key, lab) in enumerate(order):
            col = GREEN if key in ("fwd", "rgt") else tone
            iso_axis(ax, O, key, L, col, lab, num="%d" % (i + 1), tone=tone)
        ax.add_patch(Circle(O, 1.1, facecolor=NAVY, edgecolor="none", zorder=9))

        # 표적. 평면 위 자리까지 안내선을 긋고, 높이만큼 띄워 점을 찍는다
        foot = iso(O, NED_TGT[0] * SC, NED_TGT[1] * SC)
        line(ax, O, foot, color="#B9C2D0", lw=1.0, dashes=(4, 3), z=4)
        head = (foot[0], foot[1] + RISE)
        line(ax, foot, head, color=ORANGE, lw=1.3, dashes=(3, 2), z=5)
        ax.add_patch(Circle(foot, 0.9, facecolor="#9AA6B8", edgecolor="none", zorder=5))
        ax.add_patch(Circle(head, 1.9, facecolor=ORANGE, edgecolor="none", zorder=8))
        text(ax, head[0] - 2.8, head[1], "표적", size=11, color=ORANGE, bold=True,
             ha="right", va="center")

    panel((("fwd", "N  북"), ("rgt", "E  동"), ("down", "D  아래")), axA, BLUE)
    panel((("rgt", "E  동"), ("fwd", "N  북"), ("up", "U  위")), axB, ORANGE)

    n, e, d = NED_TGT
    text(lay, boxes[0][0], 0.938, "NED  (항공우주 · 항법 · 무기체계)", size=12.5, bold=True)
    text(lay, boxes[0][0], 0.888, "North-East-Down.  북 → 동 → 아래 순서, 셋째 축이 아래로 +",
         size=10, color=FAINT)
    text(lay, boxes[1][0], 0.938, "ENU  (측지 · 측량 · GIS · 로보틱스)", size=12.5, bold=True)
    text(lay, boxes[1][0], 0.888, "East-North-Up.  동 → 북 → 위 순서, 셋째 축이 위로 +", size=10,
         color=FAINT)

    text(lay, boxes[0][0], 0.128, "(N %s,  E %s,  D %s) m" % (num(n), num(e), num(d)),
         size=12.5, color=BLUE, bold=True)
    text(lay, boxes[1][0], 0.128, "(E %s,  N %s,  U %s) m" % (num(e), num(n), num(-d)),
         size=12.5, color=ORANGE, bold=True)
    text(lay, 0.520, 0.045, "북 · 동은 같은 방향이다. 셋째 축만 뒤집히고, 둘 다 오른손 좌표계다.",
         size=11.5, color=NAVY, ha="center")
    text(lay, 0.035, 0.045, "표적 높이는 부풀린 그림", size=9, color=FAINT)

    save(fig, "fig07_ned_enu.png")


# ================================================== 18장 : ECEF 와 ECI 의 차이
def draw_ecef_eci():
    """북극에서 내려다본 그림 두 칸으로 '축이 도느냐 마느냐' 를 보인다.

    ECEF 는 X 축과 건물이 같이 돌아 사이 각(경도)이 세 시각 모두 같고,
    ECI 는 X 축이 별에 묶여 있어 건물만 하루에 한 바퀴 돈다.
    한 시각만 그리면 두 칸이 똑같아 보이므로 세 시각을 겹쳐 그린다.
    """
    W, H = 1378, 619
    fig, lay = canvas(W, H)
    ax = stage(fig, [0.015, 0.250, 0.970, 0.575], (0, 100),
               (0, 100 * (0.575 * H) / (0.970 * W)))

    R, LON = 9.8, 40.0                                # 지구 반지름(칸), 건물의 경도
    HOURS = (0.0, 8.0, 16.0)                           # 겹쳐 그릴 시각
    SPIN = 360.0 / 24.0                                # 한 시간에 도는 각
    CY = 0.50 * ax.get_ylim()[1]

    def globe(c):
        ax.add_patch(Circle(c, R, facecolor=HULL, edgecolor="#3D4A5F", lw=1.5, zorder=2))
        axis_mark(ax, c, 1.7, into=False, color="#3D4A5F")
        text(ax, c[0], c[1] - 3.0, "Z 북극 ⊙", size=9, color=FAINT, ha="center", va="top")

    def spoke(c, deg, solid):
        # 원점에서 조금 띄워 시작해야 세 시각의 축이 한 줄로 붙어 보이지 않는다
        ax.add_patch(FancyArrowPatch(at(c, 2.8, deg), at(c, R + 3.0, deg), arrowstyle="-|>",
                                     mutation_scale=9, lw=2.0 if solid else 1.3,
                                     color=BLUE, shrinkA=0, shrinkB=0,
                                     alpha=1.0 if solid else 0.38,
                                     zorder=6 if solid else 4))
        if solid:
            q = at(c, R + 4.6, deg)
            text(ax, q[0], q[1], "X", size=11, color=BLUE, bold=True, ha="left",
                 va="center")

    def build(c, deg, solid, tag=None):
        p = at(c, R, deg)
        ax.add_patch(Circle(p, 1.8 if solid else 1.5, facecolor=ORANGE, edgecolor="none",
                            alpha=1.0 if solid else 0.32, zorder=7))
        if tag:                                        # 시각표는 원 안쪽에 둔다
            q = at(c, R - 3.4, deg)
            text(ax, q[0], q[1], tag, size=9, color=ORANGE if solid else FAINT,
                 bold=solid, ha="center", va="center")

    # ------------------------------------------- 왼쪽 : ECEF, 축과 건물이 같이 돈다
    CA = (24.0, CY)
    globe(CA)
    for h in HOURS:
        solid = h == 0.0
        spoke(CA, h * SPIN, solid)
        build(CA, h * SPIN + LON, solid)
        arcdeg(ax, CA, 5.6, h * SPIN, h * SPIN + LON, color=GREY if solid else "#B9C0CC",
               lw=1.5 if solid else 1.2)
    text(ax, *at(CA, 7.8, LON / 2), s="경도", size=9.5, color=GREY, bold=True,
         ha="center", va="center")

    # ------------------------------------------- 오른쪽 : ECI, 축은 멈춰 있다
    CB = (76.0, CY)
    globe(CB)
    ax.add_patch(Circle(CB, R + 2.4, facecolor="none", edgecolor=ORANGE, lw=1.0,
                        linestyle=(0, (4, 3)), zorder=3))
    spoke(CB, 0.0, True)
    for h in HOURS:
        build(CB, h * SPIN + LON, h == 0.0, tag="%d시" % int(h))
    arcdeg(ax, CB, 5.6, 0.0, LON, color=ORANGE, lw=1.5)
    text(ax, *at(CB, 8.4, LON / 2), s="θ", size=11, color=ORANGE, bold=True,
         ha="left", va="center")
    # 건물이 없는 쪽(왼쪽 아래)에 도는 방향 표시를 둔다
    curve_arrow(ax, at(CB, R + 2.4, 196.0), at(CB, R + 2.4, 238.0), rad=-0.26,
                color=ORANGE, lw=1.6, head=10)
    text(ax, *at(CB, R + 5.6, 217.0), s="하루 한 바퀴", size=9.5, color=ORANGE, bold=True,
         ha="right", va="center")

    # ------------------------------------------- 가운데 : 두 좌표계를 잇는 회전
    for dy, lab, back in ((4.2, "Rz(θ)", False), (-4.2, "Rz(θ)^T", True)):
        p0, p1 = (42.0, CY + dy), (58.0, CY + dy)
        arrow(ax, p1 if back else p0, p0 if back else p1, color=GREY, lw=1.6, head=11, z=5)
        text(ax, 50.0, CY + dy + (1.6 if dy > 0 else -1.6), lab, size=11, color=GREY,
             bold=True, ha="center", va="bottom" if dy > 0 else "top")

    # 머리글 세 줄 : 이름 / 약어를 푼 영어 / 그림 읽는 법
    for fx, ttl, eng, how in (
            (0.030, "ECEF — 지구와 함께 돈다", "Earth-Centered, Earth-Fixed",
             "북극에서 내려다본 그림. 0시 · 8시 · 16시를 겹쳐 그렸다."),
            (0.545, "ECI — 별에 고정, 돌지 않는다", "Earth-Centered Inertial",
             "같은 세 시각. 원점은 둘 다 지구 중심으로 같다.")):
        text(lay, fx, 0.958, ttl, size=13, bold=True)
        text(lay, fx, 0.914, eng, size=10, color=GREY)
        text(lay, fx, 0.872, how, size=9.5, color=FAINT)

    text(lay, 0.030, 0.168, "X 축 = 그리니치 자오선. 지구와 함께 돈다.", size=11,
         color=BLUE, bold=True)
    text(lay, 0.030, 0.108, "축과 건물이 같이 도니 사이 각(경도)이 세 시각 모두 같다",
         size=11, color=GREY)
    text(lay, 0.545, 0.168, "X 축 = 별(춘분점)에 고정. 돌지 않는다.", size=11,
         color=BLUE, bold=True)
    text(lay, 0.545, 0.108, "축이 멈춰 있으니 건물만 하루에 한 바퀴 돈다",
         size=11, color=GREY)
    text(lay, 0.500, 0.038,
         "θ = GMST (Greenwich Mean Sidereal Time).  시각이 1 ms 어긋나면 적도에서 0.47 m 어긋난다.",
         size=11.5, color=ORANGE, bold=True, ha="center")

    save(fig, "fig09_ecef_eci.png")


# =============================================== 32장 : 배가 기울면 빔도 같이 기운다
def draw_roll_beam():
    """우현 안테나는 배에 볼트로 붙어 있어 배가 기울면 보어사이트도 같이 기운다.

    보어사이트가 우현 정횡이라 배 뒤에서 선수 쪽을 본 단면이 곧 빔이 놓인 평면이다.
    그래서 화면 오른쪽이 우현이고, 고각을 화면에서 그대로 잴 수 있다.
    """
    W, H = 1300, 484
    fig, lay = canvas(W, H)
    ax = stage(fig, [0.015, 0.045, 0.970, 0.900], (0, 100),
               (0, 100 * (0.900 * H) / (0.970 * W)))

    TOP = ax.get_ylim()[1]
    ROLL, SEA, BEAM = 20.0, 0.495 * TOP, 10.0
    ax.add_patch(Polygon([(0.0, 0.0), (100.0, 0.0), (100.0, SEA), (0.0, SEA)],
                         closed=True, facecolor="#EAF0F9", edgecolor="none", zorder=0))
    sea_line(ax, 0.0, 100.0, SEA, z=1)
    text(ax, 1.5, SEA - 2.0, "해수면", size=10.5, color=FAINT, va="top")

    P = ship_stern(ax, 15.0, SEA, BEAM, ang=ROLL)
    ant = P(0.50, 0.42)                                 # 우현 뱃전
    antenna_face(ax, ant, -ROLL, half=2.2, thick=1.3, color=ORANGE)
    text(ax, ant[0] + 3.6, ant[1] + 3.2, "안테나 (우현)", size=11.5, color=ORANGE,
         bold=True, ha="left", va="bottom")
    line(ax, (ant[0] + 3.4, ant[1] + 3.0), (ant[0] + 1.2, ant[1] + 1.2), color=FAINT,
         lw=0.9, z=7)

    line(ax, ant, (ant[0] + 42.0, ant[1]), color="#B9C2D0", lw=1.2, dashes=(6, 4), z=2)
    text(ax, ant[0] + 20.0, ant[1] + 1.4, "수평 기준선 = 고각 0°", size=11, color=FAINT,
         va="bottom")

    tip = at(ant, 42.0, -ROLL)
    arrow(ax, ant, tip, color=ORANGE, lw=2.4, head=13, z=6)
    arcdeg(ax, ant, 26.0, -ROLL, 0.0, color=NAVY, lw=1.4)
    text(ax, *at(ant, 29.0, -ROLL / 2), s="20°", size=13, color=NAVY, bold=True,
         ha="left", va="center")

    # 오른쪽 빈 자리에 '보고한 값' 과 '실제 값' 을 나란히 적는다
    text(ax, 66.0, 14.0, "레이다가 보고한 고각", size=11, color=GREY)
    text(ax, 97.0, 14.0, "0°", size=12.5, color=GREY, bold=True, ha="right")
    text(ax, 66.0, 9.5, "실제 빔의 고각", size=11, color=ORANGE)
    text(ax, 97.0, 9.5, "-20°", size=13.5, color=ORANGE, bold=True, ha="right")
    line(ax, (66.0, 6.9), (97.0, 6.9), color="#C7CDD8", lw=1.0, z=2)
    text(ax, 66.0, 4.7, "빔은 하늘이 아니라 바다 쪽으로 나간다", size=11, color=NAVY,
         bold=True, va="center")

    save(fig, "fig12_roll.png")


# ================================================== 12장 : UV (방향코사인) 이란
def draw_uv():
    """u, v, w 가 어디서 오는지를 두 단계로 나눠 보인다.

    표적이 어느 쪽인지는 '길이 1 짜리 화살표' 하나로 적을 수 있다.
    그 화살표를 안테나의 세 축에 나눠 담은 양이 u, v, w 다.
    나누는 순서가 곧 공식의 모양이다. 먼저 El 로 위아래를 떼고(v),
    남은 수평 성분 cos(El) 을 Az 로 다시 앞뒤·좌우에 나눠 담는다(w, u).
    """
    W, H = 1156, 663
    fig, lay = canvas(W, H)
    ar = (0.300 * H) / (0.380 * W)
    axS = stage(fig, [0.045, 0.590, 0.380, 0.300], (0, 100), (0, 100 * ar))
    axT = stage(fig, [0.045, 0.250, 0.380, 0.300], (0, 100), (0, 100 * ar))
    axD = stage(fig, [0.520, 0.230, 0.430, 0.680], (0, 100),
                (0, 100 * (0.680 * H) / (0.430 * W)))

    EL, AZ, ARM = 26.0, 30.0, 58.0

    # ---------------------------------------- 1단계 : 옆에서 보면 El 이 위아래를 뗀다
    O = (13.0, 11.0)
    line(axS, (O[0] - 5.0, O[1]), (O[0] + 80.0, O[1]), color="#C7CDD8", lw=1.1, z=1)
    text(axS, O[0] + 81.0, O[1], "수평면", size=9.5, color=FAINT, va="center")
    tip = at(O, ARM, EL)
    foot = (tip[0], O[1])
    arrow(axS, O, tip, color=ORANGE, lw=2.2, head=11, z=6)
    text(axS, *at(O, ARM * 0.55, EL + 7.0), s="길이 1", size=11, color=ORANGE, bold=True,
         ha="center", va="bottom")
    arrow(axS, O, foot, color=BLUE, lw=1.6, head=9, z=5)
    text(axS, (O[0] + foot[0]) / 2, O[1] - 2.0, "cos(El)", size=11, color=BLUE, bold=True,
         ha="center", va="top")
    arrow(axS, foot, tip, color=GREEN, lw=1.6, head=9, z=5)
    text(axS, tip[0] + 2.0, (O[1] + tip[1]) / 2, "v = sin(El)", size=11, color=GREEN,
         bold=True, va="center")
    arcdeg(axS, O, 13.0, 0.0, EL, color=NAVY, lw=1.3)
    text(axS, *at(O, 16.5, EL / 2), s="El", size=11, color=NAVY, bold=True, ha="left",
         va="center")

    # ------------------------ 2단계 : 위에서 보면 Az 가 남은 cos(El) 을 다시 나눈다
    P = (13.0, 33.0)
    arrow(axT, P, (P[0] + 78.0, P[1]), color="#C7CDD8", lw=1.4, head=9, z=1)
    text(axT, P[0] + 79.0, P[1], "보어사이트", size=9.5, color=FAINT, va="center")
    tp = at(P, ARM, -AZ)
    ft = (tp[0], P[1])
    arrow(axT, P, tp, color=ORANGE, lw=2.2, head=11, z=6)
    text(axT, *at(P, ARM * 0.55, -AZ - 8.0), s="cos(El)", size=11, color=ORANGE, bold=True,
         ha="center", va="top")
    arrow(axT, P, ft, color=BLUE, lw=1.6, head=9, z=5)
    text(axT, (P[0] + ft[0]) / 2, P[1] + 2.0, "w = cos(El)·cos(Az)", size=11, color=BLUE,
         bold=True, ha="center", va="bottom")
    arrow(axT, ft, tp, color=GREEN, lw=1.6, head=9, z=5)
    text(axT, tp[0] + 2.0, (P[1] + tp[1]) / 2, "u = cos(El)·sin(Az)", size=11, color=GREEN,
         bold=True, va="center")
    arcdeg(axT, P, 13.0, -AZ, 0.0, color=NAVY, lw=1.3)
    text(axT, *at(P, 16.5, -AZ / 2), s="Az", size=11, color=NAVY, bold=True, ha="left",
         va="center")

    # ------------------------------------- 안테나 면을 정면에서 본 그림 (u-v 평면)
    C, RD = (48.0, 0.47 * axD.get_ylim()[1]), 34.0
    axD.add_patch(Circle(C, RD, facecolor="#F2F5FA", edgecolor="#3D4A5F", lw=1.6, zorder=2))
    for frac, lab in ((0.500, "30°"), (0.866, "60°")):     # 보어사이트에서 벌어진 각
        axD.add_patch(Circle(C, RD * frac, facecolor="none", edgecolor="#C7CDD8", lw=1.0,
                             linestyle=(0, (4, 3)), zorder=3))
        text(axD, *at(C, RD * frac, -118.0), s=lab, size=9, color=FAINT, ha="center",
             va="center")
    for d_, lab, ha, va in ((0.0, "u", "left", "center"), (90.0, "v", "center", "bottom")):
        arrow(axD, C, at(C, RD + 5.0, d_), color=GREY, lw=1.4, head=9, z=5)
        q = at(C, RD + 7.5, d_)
        text(axD, q[0], q[1], lab, size=12, color=GREY, bold=True, ha=ha, va=va)
    axis_mark(axD, C, 2.2, into=False, color=NAVY)
    text(axD, C[0] - 3.2, C[1] + 3.2, "w ⊙", size=10, color=NAVY, bold=True, ha="right",
         va="bottom")

    tgt = (C[0] + RD * 0.5, C[1])
    axD.add_patch(Circle(tgt, 2.4, facecolor=ORANGE, edgecolor="none", zorder=8))
    text(axD, tgt[0], tgt[1] - 6.2, "표적  u = 0.500,  v = 0", size=10.5, color=ORANGE,
         bold=True, ha="center", va="top")
    gl = at(C, RD * 0.55, 148.0)                       # 표적 · 눈금과 겹치지 않는 자리
    axD.add_patch(Circle(gl, 4.2, facecolor="none", edgecolor="#5A6478", lw=1.1,
                         linestyle=(0, (3, 2)), zorder=6))
    text(axD, gl[0], gl[1] + 5.4, "격자로브", size=9.5, color=GREY, ha="center", va="bottom")
    text(axD, C[0], C[1] - RD - 5.0, "u² + v² = 1  —  이 원 밖으로는 빔을 못 만든다",
         size=10, color=GREY, ha="center", va="top")

    # ------------------------------------------------------------------ 글자
    text(lay, 0.045, 0.950, "길이 1 화살표를 세 축에 나눠 담기", size=12.5, bold=True)
    text(lay, 0.045, 0.908, "① El 로 위아래를 떼고  →  ② 남은 cos(El) 을 Az 로 나눈다",
         size=10, color=GREY)
    text(lay, 0.545, 0.950, "안테나 면에서 본 u–v 평면", size=12.5, bold=True)
    text(lay, 0.545, 0.908, "점 (u, v) 는 표적 방향의 그림자", size=10, color=GREY)

    text(lay, 0.045, 0.158, "u = cos(El) · sin(Az)      v = sin(El)      w = cos(El) · cos(Az)",
         size=12, color=NAVY, bold=True)
    text(lay, 0.045, 0.104,
         "세 성분을 제곱해 더하면 1 이 된다 — 길이 1 인 화살표를 나눠 담은 것이므로.",
         size=10.5, color=GREY)
    text(lay, 0.045, 0.052,
         "각 성분은 그 축과 이루는 각의 코사인이라 '방향코사인' 이라 부른다. "
         "u, v 는 약자가 아니라 성분의 이름이다.",
         size=10.5, color=GREY)

    save(fig, "fig03_uv.png")


if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "all"
    names = ("all", "intro", "target", "polar", "derive", "body", "layout", "rotate",
             "nedenu", "ecef", "roll", "uv")
    if what not in names:
        sys.exit("사용법: python make_figures.py [%s]" % "|".join(names))
    if what in ("all", "intro"):
        draw_intro()
    if what in ("all", "target"):
        draw_one_target()
    if what in ("all", "polar"):
        draw_polar()
    if what in ("all", "derive"):
        draw_derive()
    if what in ("all", "body"):
        draw_body()
    if what in ("all", "layout"):
        draw_layout()
    if what in ("all", "rotate"):
        draw_rotate()
    if what in ("all", "nedenu"):
        draw_ned_enu()
    if what in ("all", "ecef"):
        draw_ecef_eci()
    if what in ("all", "roll"):
        draw_roll_beam()
    if what in ("all", "uv"):
        draw_uv()
