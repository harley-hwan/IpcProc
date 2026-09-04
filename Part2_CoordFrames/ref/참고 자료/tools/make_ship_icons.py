# -*- coding: utf-8 -*-
"""발표자료 그림에 들어 있는 함선을 낱개 아이콘(투명 PNG · SVG)으로 뽑는다.

배 모양과 색은 make_figures.py 의 배 그리기 함수를 그대로 불러 쓰므로 발표자료 그림과 똑같다.

    python make_ship_icons.py                 # 전부
    python make_ship_icons.py ship_top ship_iso   # 일부만 (이름은 아래 ICONS 의 키)

결과는 ../함선 아이콘/ 에 저장된다.
  · <이름>.png   배경이 투명한 비트맵 (배 길이 100 단위 = 4 in → 1600 px)
  · <이름>.svg   벡터. PowerPoint 에 넣은 뒤 '그래픽 → 도형으로 변환' 하면 색·선을 바꿀 수 있다
  · preview.png  전체 미리보기 한 장
"""
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.image import imread
from matplotlib.patches import Polygon, Rectangle

import make_figures as mf

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.path.normpath(os.path.join(HERE, "..", "함선 아이콘"))
SCALE = 0.04        # 데이터 1 단위 = 0.04 in. 배 길이 100 단위 → 4 in (발표자료 그림과 비슷한 크기라 선 굵기 비율이 같다)
DPI = 400           # PNG 해상도. 배 길이 4 in → 1600 px
PAD = 0.05          # 아이콘 둘레 여백 (in)
DARK_BG = "#12223E"  # 어두운 슬라이드 배경색 (미리보기에서 어두운 배경용 아이콘 뒤에 깐다)

matplotlib.rcParams["svg.hashsalt"] = "ship-icons"   # SVG 안의 id 가 만들 때마다 같도록


def canvas(xlim, ylim):
    """데이터 좌표가 그대로 물리 크기가 되는 도화지. 축 장식은 없다"""
    fig = plt.figure(figsize=((xlim[1] - xlim[0]) * SCALE, (ylim[1] - ylim[0]) * SCALE))
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(*xlim)
    ax.set_ylim(*ylim)
    ax.set_aspect("equal")
    ax.axis("off")
    return fig, ax


def marker(ax, color):
    """전시기 지도 위 '우리 배' 표식. 선수가 위쪽. 지도 단위가 작아서 8 배로 키운다"""
    ax.add_patch(Polygon([(u * 8.0, v * 8.0) for u, v in mf.HULL_MARKER], closed=True,
                         facecolor=color, edgecolor=color, lw=1.0, joinstyle="round"))


# 이름 → (설명, 그리기, xlim, ylim, 미리보기 배경). 범위는 넉넉히 잡고 저장할 때 꼭 맞게 자른다
ICONS = {
    "ship_top": ("위에서 본 모습 · 선수 오른쪽",
                 lambda ax: mf.ship_top(ax, 0.0, 0.0, 100.0, wake=False),
                 (-4, 104), (-9, 9), None),
    "ship_top_wake": ("위에서 본 모습 · 항적 포함",
                      lambda ax: mf.ship_top(ax, 0.0, 0.0, 100.0, wake=True),
                      (-26, 104), (-18, 18), None),
    "ship_side": ("측면 · 선수 오른쪽 (흘수선 y = 0)",
                  lambda ax: mf.ship_side(ax, 0.0, 0.0, 100.0),
                  (-4, 104), (-6, 36), None),
    "ship_stern": ("후방 · 선미에서 선수 쪽으로 (오른쪽이 우현)",
                   lambda ax: mf.ship_stern(ax, 0.0, 0.0, 40.0),
                   (-24, 24), (-17, 67), None),
    "ship_iso": ("사선 · 축측 투영",
                 lambda ax: mf.ship_iso(ax, (0.0, 0.0), 100.0),
                 (-32, 40), (-30, 36), None),
    "ship_marker": ("전시기 지도 표식 · 밝은 배경용",
                    lambda ax: marker(ax, mf.BLUE),
                    (-16, 16), (-24, 28), None),
    "ship_marker_dark": ("전시기 지도 표식 · 어두운 배경용 (발표자료 색)",
                         lambda ax: marker(ax, mf.DK_BLUE),
                         (-16, 16), (-24, 28), DARK_BG),
}


def make(name):
    label, draw, xlim, ylim, _ = ICONS[name]
    for ext in ("png", "svg"):
        fig, ax = canvas(xlim, ylim)
        draw(ax)
        path = os.path.join(OUT_DIR, "%s.%s" % (name, ext))
        opts = dict(transparent=True, bbox_inches="tight", pad_inches=PAD)
        if ext == "png":
            fig.savefig(path, dpi=DPI, **opts)
        else:
            fig.savefig(path, metadata={"Date": None}, **opts)   # 날짜를 빼서 내용이 같으면 파일도 같게
        plt.close(fig)
        print("생성: %s" % path)


def preview():
    """만들어진 PNG 를 한 장에 모아 보여 준다"""
    names = [n for n in ICONS if os.path.exists(os.path.join(OUT_DIR, n + ".png"))]
    cols, rows = 4, (len(names) + 3) // 4
    fig, axes = plt.subplots(rows, cols, figsize=(4.2 * cols, 3.4 * rows))
    fig.patch.set_facecolor("white")
    for ax in axes.flat:
        ax.axis("off")
    for ax, name in zip(axes.flat, names):
        label, _, _, _, bg = ICONS[name]
        if bg:
            ax.add_patch(Rectangle((0, 0), 1, 1, transform=ax.transAxes, facecolor=bg, zorder=0))
        ax.imshow(imread(os.path.join(OUT_DIR, name + ".png")), zorder=1)
        ax.set_title(label, fontproperties=mf.BOLD, fontsize=11, color=mf.NAVY, pad=8)
        ax.text(0.5, -0.06, name + ".png / .svg", transform=ax.transAxes, ha="center",
                va="top", fontproperties=mf.REG, fontsize=9.5, color=mf.GREY)
    fig.suptitle("Chapter 2 발표자료의 함선 아이콘", fontproperties=mf.BOLD, fontsize=14,
                 color=mf.NAVY, y=0.99)
    fig.tight_layout(rect=(0, 0, 1, 0.96))
    path = os.path.join(OUT_DIR, "preview.png")
    fig.savefig(path, dpi=150, facecolor="white")
    plt.close(fig)
    print("생성: %s" % path)


if __name__ == "__main__":
    wanted = sys.argv[1:] or list(ICONS)
    bad = [w for w in wanted if w not in ICONS]
    if bad:
        sys.exit("모르는 이름: %s (가능: %s)" % (", ".join(bad), ", ".join(ICONS)))
    os.makedirs(OUT_DIR, exist_ok=True)
    for name in wanted:
        make(name)
    preview()
