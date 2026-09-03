# -*- coding: utf-8 -*-
"""슬라이드 PDF + 대본 md -> 대본합본 PDF. Part1 의 build_combo.py 와 같은 레이아웃.

    python build_combo.py Chapter2_coordinate_systems.pdf Chapter2_발표대본.md Chapter2_발표자료_대본합본.pdf

슬라이드 PDF 는 PowerPoint 나 LibreOffice 로 내보낸 것을 그대로 씀.
글꼴은 맑은 고딕이 있으면 그걸 쓰고, 없으면(리눅스) 나눔고딕을 씀.
"""
import io, os, re, sys
import pymupdf as fitz

CANDIDATES = [
    (r"C:\Windows\Fonts\malgun.ttf", r"C:\Windows\Fonts\malgunbd.ttf"),
    ("/usr/share/fonts/truetype/nanum/NanumGothic.ttf", "/usr/share/fonts/truetype/nanum/NanumGothicBold.ttf"),
]
MG = MGB = None
for a, b in CANDIDATES:
    if os.path.exists(a) and os.path.exists(b):
        MG, MGB = a, b
        break
if MG is None:
    sys.exit("한글 글꼴을 못 찾음 (맑은 고딕 / 나눔고딕)")
F = {"mg": fitz.Font(fontfile=MG), "mgb": fitz.Font(fontfile=MGB)}
DARK, BODY, GOLD, GREY, PANEL, RULE = (
    (0.118,0.137,0.180), (0.239,0.267,0.318), (0.541,0.369,0.063),
    (0.478,0.510,0.561), (0.949,0.957,0.969), (0.835,0.859,0.894))
FOOT = "Chapter 2 좌표계와 좌표변환  ·  Janghwan Kim (Harley)"

def parse(md):
    """## N. 제목  -> {번호: (제목, 시간, [본문])}. 끝의 부록은 뺌"""
    md = md.split("\n# 부록")[0]
    out = {}
    for blk in re.split(r"(?m)^## ", md)[1:]:
        m = re.match(r"(\d+)\.\s+(.+)", blk)
        if not m: continue
        num, title = int(m.group(1)), m.group(2).strip()
        time, body = "", []
        for ln in blk.split("\n")[1:]:
            t = ln.strip()
            if not t or t.startswith(("|", "#", "---", "![")): continue
            b = re.match(r"\[(.+?)\]$", t)
            if b and not time: time = b.group(1); continue
            t = re.sub(r"\*\*(.+?)\*\*", r"\1", t)
            body.append(t.replace("\u2212", "-"))     # 수학 빼기 기호는 나눔고딕에 없음
        out[num] = (title, time, body)
    return out

# 한 글자씩 끊되, 20,000 같은 숫자 덩어리는 통째로 옮긴다 (20, / 000 으로 갈라지면 안 된다)
TOKEN = re.compile(r"\d[\d,.]*\d|.", re.S)

def wrap(txt, fnt, size, width):
    lines, cur = [], ""
    for tok in TOKEN.findall(txt):
        if fnt.text_length(cur + tok, size) > width and cur:
            lines.append(cur); cur = "" if tok == " " else tok
        else: cur += tok
    if cur.strip(): lines.append(cur)
    return lines

def build(slides_pdf, md_path, out):
    src = fitz.open(slides_pdf)
    secs = parse(io.open(md_path, encoding="utf-8").read())
    doc = fitz.open()
    for i in range(src.page_count):
        num = i + 1
        title, time, body = secs.get(num, ("", "", []))
        pg = doc.new_page(width=595, height=842)
        for t, f in (("mg", MG), ("mgb", MGB)): pg.insert_font(fontname=t, fontfile=f)
        s = pg.new_shape()
        s.draw_rect(fitz.Rect(40, 34, 84, 59));   s.finish(fill=DARK, color=None)
        s.draw_rect(fitz.Rect(40, 381, 556, 797)); s.finish(fill=PANEL, color=None)
        s.commit()
        pg.insert_text(fitz.Point(62 - F["mgb"].text_length(str(num), 12)/2, 51),
                       str(num), fontname="mgb", fontsize=12, color=(1,1,1))
        pg.insert_text(fitz.Point(96, 51), title, fontname="mgb", fontsize=12, color=DARK)
        pg.show_pdf_page(fitz.Rect(40, 70, 556, 361), src, i)      # 슬라이드를 벡터로 삽입
        pg.insert_text(fitz.Point(58, 400), "발표 대본", fontname="mgb", fontsize=10.5, color=DARK)
        if time:
            w = F["mg"].text_length(time, 9.5)
            pg.insert_text(fitz.Point(537 - w, 400), time, fontname="mg", fontsize=9.5, color=GOLD)

        # 본문이 패널에 들어가도록 글자 크기를 줄여 가며 맞춤
        for size in [10.0, 9.5, 9.0, 8.5, 8.0, 7.5, 7.0, 6.5, 6.0]:
            lh = size * 1.45
            bl = [wrap(p, F["mg"], size, 479) for p in body]
            h = sum(len(x) * lh + lh * 0.55 for x in bl)
            if 420 + h < 790: break
        y = 420
        for para in bl:
            for ln in para:
                pg.insert_text(fitz.Point(58, y), ln, fontname="mg", fontsize=size, color=BODY)
                y += lh
            y += lh * 0.55
        pg.insert_text(fitz.Point(40, 812), FOOT, fontname="mg", fontsize=8.5, color=GREY)
        pn = "%d / %d" % (num, src.page_count)
        pg.insert_text(fitz.Point(556 - F["mg"].text_length(pn, 8.5), 812), pn,
                       fontname="mg", fontsize=8.5, color=GREY)
    doc.set_metadata({"title": "Chapter 2 좌표계와 좌표변환 · 대본 합본", "producer": "build_combo.py"})
    doc.subset_fonts(verbose=False)
    doc.save(out, deflate=True, garbage=4); doc.close(); src.close()
    print("합본 생성: %s" % out)

if __name__ == "__main__":
    build(sys.argv[1], sys.argv[2], sys.argv[3])
