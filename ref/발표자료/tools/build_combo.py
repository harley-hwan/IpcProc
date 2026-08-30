# -*- coding: utf-8 -*-
"""슬라이드 PDF + 대본 md -> 대본합본 PDF (기존 합본 레이아웃 그대로)"""
import io, re, sys
import pymupdf as fitz

MG  = r"C:\Windows\Fonts\malgun.ttf"
MGB = r"C:\Windows\Fonts\malgunbd.ttf"
F   = {"mg": fitz.Font(fontfile=MG), "mgb": fitz.Font(fontfile=MGB)}
DARK, BODY, GOLD, GREY, PANEL, RULE = (
    (0.118,0.137,0.180), (0.239,0.267,0.318), (0.541,0.369,0.063),
    (0.478,0.510,0.561), (0.949,0.957,0.969), (0.835,0.859,0.894))

def parse(md):
    """## N. 제목  -> {번호: (제목, 시간, [본문], [(질문,답)])}"""
    out = {}
    for blk in re.split(r"(?m)^## ", md)[1:]:
        m = re.match(r"(\d+)\.\s+(.+)", blk)
        if not m or not (1 <= int(m.group(1)) <= 22): continue
        num, title = int(m.group(1)), m.group(2).strip()
        time, body, qa, fence = "", [], [], False
        for ln in blk.split("\n")[1:]:
            t = ln.strip()
            if t.startswith("```"): fence = not fence; continue
            if fence or not t or t.startswith(("|", "#", "---", "![")): continue
            b = re.match(r"\[(.+?)\]$", t)
            if b and not time: time = b.group(1); continue
            if t.startswith(">"):
                q = re.match(r">\s*\*\*(.+?)\*\*\s*[—-]\s*(.*)", t)
                if q: qa.append((q.group(1).strip(), q.group(2).strip()))
                continue
            body.append(re.sub(r"\*\*(.+?)\*\*", r"\1", t))
        out[num] = (title, time, body, qa)
    return out

def wrap(txt, fnt, size, width):
    lines, cur = [], ""
    for ch in txt:
        if fnt.text_length(cur + ch, size) > width and cur:
            if ch != " ": lines.append(cur); cur = ch
            else: lines.append(cur); cur = ""
        else: cur += ch
    if cur.strip(): lines.append(cur)
    return lines

def build(slides_pdf, md_path, out):
    src = fitz.open(slides_pdf)
    secs = parse(io.open(md_path, encoding="utf-8").read())
    doc = fitz.open()
    for i in range(src.page_count):
        num = i + 1
        title, time, body, qa = secs.get(num, ("", "", [], []))
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

        # 본문 + 예상 질문이 패널에 들어가도록 크기를 자동으로 줄인다
        for size in [10.0, 9.5, 9.0, 8.5, 8.0, 7.5, 7.0, 6.5, 6.0]:
            lh, qsz = size * 1.45, max(6.0, size - 2.0)
            bl = [wrap(p, F["mg"], size, 479) for p in body]
            h = sum(len(x) * lh + lh * 0.55 for x in bl)
            ql = [wrap("%s — %s" % (q, a), F["mg"], qsz, 479) for q, a in qa]
            qh = (18 + sum(len(x) * qsz * 1.5 for x in ql)) if qa else 0
            if 420 + h + qh < 790: break
        y = 420
        for para in bl:
            for ln in para:
                pg.insert_text(fitz.Point(58, y), ln, fontname="mg", fontsize=size, color=BODY)
                y += lh
            y += lh * 0.55
        if qa:
            y = max(y + 6, 790 - qh)
            s2 = pg.new_shape(); s2.draw_rect(fitz.Rect(58, y - 8, 537, y - 7))
            s2.finish(fill=RULE, color=None); s2.commit()
            pg.insert_text(fitz.Point(58, y + 4), "예상 질문", fontname="mgb", fontsize=qsz + 0.5, color=GOLD)
            y += 19
            for lines in ql:
                for ln in lines:
                    pg.insert_text(fitz.Point(58, y), ln, fontname="mg", fontsize=qsz, color=DARK)
                    y += qsz * 1.5
        foot = "IPC (Inter Process Communication)  ·  Janghwan Kim (Harley)"
        pg.insert_text(fitz.Point(40, 812), foot, fontname="mg", fontsize=8.5, color=GREY)
        pn = "%d / %d" % (num, src.page_count)
        pg.insert_text(fitz.Point(556 - F["mg"].text_length(pn, 8.5), 812), pn,
                       fontname="mg", fontsize=8.5, color=GREY)
    doc.set_metadata({"title": "IPC 발표자료 · 대본 합본", "producer": "IpcProc build_combo.py"})
    doc.subset_fonts(verbose=False)
    doc.save(out, deflate=True, garbage=4); doc.close(); src.close()
    print("합본 생성: %s" % out)

if __name__ == "__main__":
    build(sys.argv[1], sys.argv[2], sys.argv[3])
