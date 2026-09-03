/**
 * LIG Defense & Aerospace — PowerPoint template generator
 *
 * Rebuilds the printed template (17 photographed pages) as a native .pptx:
 *   - 6 slide masters (layouts) with real title/body placeholders
 *   - 17 sample slides, one per photographed layout
 *
 * Usage:  node build_lig_template.js [output.pptx]
 */
"use strict";

const path = require("path");
const React = require("react");
const ReactDOMServer = require("react-dom/server");
const sharp = require("sharp");
const pptxgen = require("pptxgenjs");
const lu = require("react-icons/lu");
const { svgFor } = require("./lig_icons");

// ─────────────────────────────────────────────────────────────────────────────
// Design tokens (measured from the photographed template)
// ─────────────────────────────────────────────────────────────────────────────
const C = Object.freeze({
  DARK: "2C2C2C", // header bar, cover, section blocks
  BG: "EDEDED", // slide background (content pages)
  PANEL: "FFFFFF", // white content panel / cards
  CARD: "E3E3E3", // grey card body on white panel
  TEXT: "1A1A1A",
  MUTED: "6B6B6B",
  LINE: "9A9A9A",
  WHITE: "FFFFFF",
});

const F = Object.freeze({
  KO: "맑은 고딕", // Korean UI text (ships with Windows Office)
  EN: "Arial", // Latin text / wordmark stand-in
});

const SLIDE = Object.freeze({ W: 13.333, H: 7.5 });

// Shared geometry for content pages
const G = Object.freeze({
  M: 0.5, // outer margin
  HEADER_Y: 0.45,
  HEADER_H: 0.72,
  PANEL_Y: 1.35,
  PANEL_H: 5.35,
  PANEL_BOTTOM: 1.35 + 5.35, // 6.70
  INNER_W: SLIDE.W - 2 * 0.5, // 12.333
  FOOTER_LINE_Y: 6.95,
  FOOTER_TEXT_Y: 7.0,
});

const COMPANY = "LIG Defense&Aerospace";
const WORDMARK = "LIG";

// ─────────────────────────────────────────────────────────────────────────────
// Small helpers (fresh option objects every call — pptxgenjs mutates them)
// ─────────────────────────────────────────────────────────────────────────────
const txt = (slide, text, o) =>
  slide.addText(text, {
    isTextBox: true,
    margin: 0,
    fontFace: F.KO,
    color: C.TEXT,
    valign: "top",
    ...o,
  });

const rect = (slide, x, y, w, h, fill, extra = {}) =>
  slide.addShape("rect", { x, y, w, h, fill: { color: fill }, line: { color: fill, width: 0 }, ...extra });

const hline = (slide, x1, x2, y, color = C.LINE, width = 0.75, dash = "solid") =>
  slide.addShape("line", { x: x1, y, w: x2 - x1, h: 0, line: { color, width, dashType: dash } });

const vline = (slide, x, y1, y2, color = C.LINE, width = 0.75, dash = "solid") =>
  slide.addShape("line", { x, y: y1, w: 0, h: y2 - y1, line: { color, width, dashType: dash } });

/** Wordmark stand-in — replace with the official LIG logo asset (see notes). */
const wordmark = (slide, x, y, size, color, align = "left", w = 1.6) =>
  txt(slide, WORDMARK, {
    x, y, w, h: size / 60,
    fontFace: F.EN, fontSize: size, bold: true, color, align, charSpacing: 1, valign: "middle",
  });

/** White centred label inside a dark block (e.g. "제안사항 입력"). */
const darkLabelBox = (slide, x, y, w, h, text, fontSize = 16) => {
  rect(slide, x, y, w, h, C.DARK);
  txt(slide, text, { x, y, w, h, fontSize, bold: true, color: C.WHITE, align: "center", valign: "middle" });
};

/** Thin vertical accent bar + bold heading (used on several pages). */
const accentHeading = (slide, x, y, text, fontSize = 15) => {
  rect(slide, x, y + 0.02, 0.05, fontSize / 40, C.DARK);
  txt(slide, text, { x: x + 0.18, y, w: 5, h: fontSize / 40 + 0.05, fontSize, bold: true, valign: "middle" });
};

// ─────────────────────────────────────────────────────────────────────────────
// Icon rasteriser (react-icons → SVG → PNG → base64 data URI)
// ─────────────────────────────────────────────────────────────────────────────
async function iconDataUri(IconComponent, color, px = 320) {
  const svg = ReactDOMServer.renderToStaticMarkup(
    React.createElement(IconComponent, { size: px, color: `#${color}`, strokeWidth: 1.5 })
  );
  const png = await sharp(Buffer.from(svg)).png().toBuffer();
  return `image/png;base64,${png.toString("base64")}`;
}

/** Hand-drawn LIG product pictogram (see lig_icons.js) → PNG data URI. */
async function pictogramDataUri(name, color) {
  const png = await sharp(Buffer.from(svgFor(name, color))).png().toBuffer();
  return `image/png;base64,${png.toString("base64")}`;
}

// ─────────────────────────────────────────────────────────────────────────────
// Slide masters (= layouts the user can pick from "New Slide")
// ─────────────────────────────────────────────────────────────────────────────
function defineMasters(pres) {
  // 1) COVER ------------------------------------------------------------------
  pres.defineSlideMaster({
    title: "LIG_COVER",
    background: { color: C.DARK },
    objects: [
      { text: { text: WORDMARK, options: { x: 0.6, y: 0.45, w: 2.2, h: 1.0, fontFace: F.EN, fontSize: 54, bold: true, color: C.WHITE, margin: 0, valign: "middle" } } },
      { text: { text: "Confidential", options: { x: 10.3, y: 0.55, w: 2.5, h: 0.35, fontFace: F.EN, fontSize: 11, bold: true, color: C.WHITE, align: "right", margin: 0 } } },
      { placeholder: { options: { name: "title", type: "title", x: 4.3, y: 2.85, w: 8.5, h: 1.0, fontFace: F.KO, fontSize: 40, bold: true, color: C.WHITE, align: "right", valign: "middle", margin: 0 }, text: "제목을 입력하십시오" } },
      { text: { text: COMPANY, options: { x: 6.8, y: 4.05, w: 6.0, h: 0.4, fontFace: F.EN, fontSize: 16, bold: true, color: C.WHITE, align: "right", margin: 0 } } },
      { placeholder: { options: { name: "body", type: "body", x: 6.8, y: 4.5, w: 6.0, h: 0.35, fontFace: F.KO, fontSize: 12, color: C.WHITE, align: "right", margin: 0 }, text: "부서명/담당자명 기입" } },
    ],
  });

  // 2) INDEX ------------------------------------------------------------------
  pres.defineSlideMaster({
    title: "LIG_INDEX",
    background: { color: C.BG },
    objects: [
      { rect: { x: 0, y: 0, w: 4.9, h: SLIDE.H, fill: { color: C.DARK }, line: { color: C.DARK, width: 0 } } },
      { line: { x: 2.65, y: 0.86, w: 9.35, h: 0, line: { color: C.LINE, width: 0.75 } } },
      { text: { text: "INDEX", options: { x: 1.25, y: 0.55, w: 2.2, h: 0.6, fontFace: F.EN, fontSize: 30, bold: true, color: C.WHITE, margin: 0, valign: "middle" } } },
      { text: { text: WORDMARK, options: { x: 11.4, y: 0.4, w: 1.4, h: 0.6, fontFace: F.EN, fontSize: 30, bold: true, color: C.DARK, align: "right", margin: 0, valign: "middle" } } },
      { placeholder: { options: { name: "title", type: "title", x: 0.4, y: 3.2, w: 4.1, h: 1.1, fontFace: F.KO, fontSize: 20, bold: true, color: C.WHITE, align: "center", valign: "middle", margin: 0 }, text: "작성하실 제목을\n입력하시기 바랍니다." } },
      { text: { text: COMPANY, options: { x: 0.4, y: G.FOOTER_TEXT_Y, w: 3.5, h: 0.3, fontFace: F.EN, fontSize: 10, bold: true, color: C.WHITE, margin: 0 } } },
    ],
    slideNumber: { x: 6.2, y: G.FOOTER_TEXT_Y, w: 1.0, h: 0.3, fontFace: F.EN, fontSize: 10, color: C.TEXT, align: "center" },
  });

  // 3-4) SECTION dividers (dark / light) ---------------------------------------
  for (const [name, bg, fg] of [["LIG_SECTION_DARK", C.DARK, C.WHITE], ["LIG_SECTION_LIGHT", C.BG, C.DARK]]) {
    pres.defineSlideMaster({
      title: name,
      background: { color: bg },
      objects: [
        { text: { text: WORDMARK, options: { x: 0.55, y: 0.5, w: 1.6, h: 0.7, fontFace: F.EN, fontSize: 34, bold: true, color: fg, margin: 0, valign: "middle" } } },
        { line: { x: 2.0, y: 0.86, w: 10.8, h: 0, line: { color: C.LINE, width: 0.75 } } },
        // company label sits on the line (background-filled box breaks the rule under the text)
        { text: { text: COMPANY, options: { x: 10.05, y: 0.7, w: 2.3, h: 0.32, fontFace: F.EN, fontSize: 11, bold: true, color: fg, fill: { color: bg }, align: "center", valign: "middle", margin: 0 } } },
        { placeholder: { options: { name: "title", type: "title", x: 0.95, y: 1.35, w: 11.5, h: 0.75, fontFace: F.EN, fontSize: 28, bold: true, color: fg, align: "left", valign: "middle", margin: 0 }, text: "Please write the main title here" } },
        { placeholder: { options: { name: "subtitle", type: "body", x: 0.95, y: 2.1, w: 11.5, h: 0.4, fontFace: F.EN, fontSize: 16, color: fg, align: "left", valign: "middle", margin: 0 }, text: "Enter the subtitle here" } },
        { placeholder: { options: { name: "body", type: "body", x: 0.95, y: 2.5, w: 11.5, h: 0.5, fontFace: F.KO, fontSize: 18, color: fg, align: "left", valign: "middle", margin: 0 }, text: "텍스트를 입력하십시오" } },
        { line: { x: G.M, y: G.FOOTER_LINE_Y, w: G.INNER_W, h: 0, line: { color: C.LINE, width: 0.75 } } },
      ],
      slideNumber: { x: 6.2, y: G.FOOTER_TEXT_Y, w: 1.0, h: 0.3, fontFace: F.EN, fontSize: 10, color: fg, align: "center" },
    });
  }

  // 5) CONTENT (header bar + footer) --------------------------------------------
  pres.defineSlideMaster({
    title: "LIG_CONTENT",
    background: { color: C.BG },
    objects: [
      { rect: { x: G.M, y: G.HEADER_Y, w: G.INNER_W, h: G.HEADER_H, fill: { color: C.DARK }, line: { color: C.DARK, width: 0 } } },
      { placeholder: { options: { name: "title", type: "title", x: G.M + 0.2, y: G.HEADER_Y, w: 9.0, h: G.HEADER_H, fontFace: F.KO, fontSize: 20, bold: true, color: C.WHITE, align: "left", valign: "middle", margin: 0 }, text: "01. 메시지 입력" } },
      { text: { text: WORDMARK, options: { x: 11.2, y: G.HEADER_Y, w: 1.45, h: G.HEADER_H, fontFace: F.EN, fontSize: 24, bold: true, color: C.WHITE, align: "right", valign: "middle", margin: 0 } } },
      { line: { x: G.M, y: G.FOOTER_LINE_Y, w: G.INNER_W, h: 0, line: { color: C.LINE, width: 0.75 } } },
      { text: { text: COMPANY, options: { x: G.M, y: G.FOOTER_TEXT_Y, w: 3.5, h: 0.3, fontFace: F.EN, fontSize: 10, bold: true, color: C.TEXT, margin: 0 } } },
    ],
    slideNumber: { x: 6.2, y: G.FOOTER_TEXT_Y, w: 1.0, h: 0.3, fontFace: F.EN, fontSize: 10, color: C.TEXT, align: "center" },
  });

  // 6) ICON LIBRARY -----------------------------------------------------------
  pres.defineSlideMaster({
    title: "LIG_ICON_LIB",
    background: { color: C.WHITE },
    objects: [
      { rect: { x: 8.0, y: 0, w: 5.333, h: 1.85, fill: { color: C.DARK }, line: { color: C.DARK, width: 0 } } },
      { text: { text: WORDMARK, options: { x: 8.0, y: 0, w: 5.333, h: 1.85, fontFace: F.EN, fontSize: 48, bold: true, color: C.WHITE, align: "center", valign: "middle", margin: 0 } } },
      { text: { text: WORDMARK, options: { x: 0.6, y: 0.45, w: 1.6, h: 0.7, fontFace: F.EN, fontSize: 30, bold: true, color: C.DARK, margin: 0, valign: "middle" } } },
    ],
  });
}

// ─────────────────────────────────────────────────────────────────────────────
// Content-page building blocks
// ─────────────────────────────────────────────────────────────────────────────
const contentSlide = (pres, title) => {
  const s = pres.addSlide({ masterName: "LIG_CONTENT" });
  s.addText(title, { placeholder: "title" });
  return s;
};

const whitePanel = (s, x = G.M, y = G.PANEL_Y, w = G.INNER_W, h = G.PANEL_H) => rect(s, x, y, w, h, C.PANEL);

/** 3-row × 5-col icon grid used by all four library pages. */
async function iconLibrarySlide(pres, cells, notes) {
  const s = pres.addSlide({ masterName: "LIG_ICON_LIB" });
  const cols = 5, rows = 3;
  const x0 = 0, y0 = 1.85, cw = SLIDE.W / cols, ch = (SLIDE.H - y0) / rows;

  // dotted grid
  for (let c = 1; c < cols; c++) vline(s, x0 + c * cw, y0, SLIDE.H, C.LINE, 0.5, "sysDot");
  for (let r = 0; r <= rows; r++) hline(s, x0, SLIDE.W, y0 + r * ch, C.LINE, 0.5, r === 0 ? "solid" : "sysDot");

  for (let i = 0; i < cells.length; i++) {
    const { label, icon, svg, tagline } = cells[i];
    const cx = x0 + (i % cols) * cw, cy = y0 + Math.floor(i / cols) * ch;
    if (tagline) {
      txt(s, tagline, { x: cx + 0.2, y: cy + ch - 0.5, w: cw - 0.4, h: 0.3, fontFace: F.EN, fontSize: 9, bold: true, color: C.DARK, align: "right", valign: "bottom" });
      continue;
    }
    txt(s, label, { x: cx + 0.2, y: cy + 0.15, w: cw - 0.4, h: 0.45, fontSize: 9, color: C.TEXT });
    if (icon) {
      const size = 0.95;
      s.addImage({ data: await iconDataUri(icon, C.DARK), x: cx + (cw - size) / 2, y: cy + (ch - size) / 2 + 0.15, w: size, h: size });
    } else if (svg) {
      const size = 1.15;
      s.addImage({ data: await pictogramDataUri(svg, C.DARK), x: cx + (cw - size) / 2, y: cy + (ch - size) / 2 + 0.15, w: size, h: size });
    } else {
      // empty slot: paste the product pictogram from the LIG asset library here
      const size = 1.0;
      s.addShape("rect", { x: cx + (cw - size) / 2, y: cy + (ch - size) / 2 + 0.15, w: size, h: size, fill: { color: C.WHITE, transparency: 100 }, line: { color: C.LINE, width: 0.5, dashType: "dash" } });
    }
  }
  if (notes) s.addNotes(notes);
  return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// Deck assembly
// ─────────────────────────────────────────────────────────────────────────────
async function build(outFile) {
  const pres = new pptxgen();
  pres.layout = "LAYOUT_WIDE"; // 13.333 × 7.5 in
  pres.title = "LIG Defense & Aerospace – Presentation Template";
  pres.company = "LIG Nex1";
  pres.lang = "ko-KR";
  defineMasters(pres);

  // 01 Cover ------------------------------------------------------------------
  {
    const s = pres.addSlide({ masterName: "LIG_COVER" });
    s.addText("제목을 입력하십시오", { placeholder: "title" });
    s.addText("부서명/담당자명 기입", { placeholder: "body" });
    s.addNotes("표지: 'LIG' 워드마크 텍스트는 공식 로고 이미지로 교체하십시오.");
  }

  // 02 Index --------------------------------------------------------------------
  {
    const s = pres.addSlide({ masterName: "LIG_INDEX" });
    s.addText("작성하실 제목을\n입력하시기 바랍니다.", { placeholder: "title" });
    const rowsY = [1.95, 3.45, 4.95];
    rowsY.forEach((y, i) => {
      txt(s, `0${i + 1}`, { x: 5.55, y, w: 1.1, h: 0.9, fontFace: F.EN, fontSize: 40, bold: true, color: C.DARK, valign: "middle" });
      txt(s, `Components ${i + 1}`, { x: 6.75, y, w: 2.4, h: 0.9, fontFace: F.EN, fontSize: 22, color: C.TEXT, valign: "middle" });
      txt(
        s,
        [1, 2, 3].map((n, k) => ({ text: "Enter the content you", options: { bullet: { type: "number" }, breakLine: k < 2 } })),
        { x: 9.15, y: y - 0.05, w: 3.6, h: 1.0, fontFace: F.EN, fontSize: 12, color: C.TEXT, paraSpaceAfter: 6, valign: "middle" }
      );
    });
  }

  // 03-04 Section dividers ------------------------------------------------------
  for (const master of ["LIG_SECTION_DARK", "LIG_SECTION_LIGHT"]) {
    const s = pres.addSlide({ masterName: master });
    s.addText("Please write the main title here", { placeholder: "title" });
    s.addText("Enter the subtitle here", { placeholder: "subtitle" });
    s.addText("텍스트를 입력하십시오", { placeholder: "body" });
  }

  // 05 Content · single panel ---------------------------------------------------
  {
    const s = contentSlide(pres, "01. 메시지를 입력");
    whitePanel(s);
    accentHeading(s, 0.65, 1.55, "텍스트 입력", 16);
  }

  // 06 Content · two columns (wide left) ---------------------------------------
  {
    const s = contentSlide(pres, "01. 메시지 입력");
    whitePanel(s);
    txt(s, "텍스트 입력", { x: 0.75, y: 1.55, w: 6.8, h: 0.4, fontSize: 16 });
    vline(s, 7.9, 1.55, G.PANEL_BOTTOM - 0.15, C.LINE, 0.5);
    txt(s, "텍스트 입력", { x: 8.1, y: 1.55, w: 4.5, h: 0.4, fontSize: 16, bold: true });
    txt(s, "텍스트 입력", { x: 8.1, y: 2.0, w: 4.5, h: 0.4, fontSize: 16 });
  }

  // 07 Content · two columns (bold heading left) -------------------------------
  {
    const s = contentSlide(pres, "01. 메시지 입력");
    whitePanel(s);
    txt(s, "텍스트 입력", { x: 0.75, y: 1.55, w: 7.5, h: 0.4, fontSize: 16, bold: true });
    txt(s, "텍스트 입력", { x: 0.75, y: 2.0, w: 7.5, h: 0.4, fontSize: 16 });
    vline(s, 8.6, 1.55, G.PANEL_BOTTOM - 0.15, C.LINE, 0.5);
    txt(s, "텍스트 입력", { x: 8.8, y: 1.55, w: 3.8, h: 0.4, fontSize: 16 });
  }

  // 08 Content · key message + 4 cards -----------------------------------------
  {
    const s = contentSlide(pres, "01. 메시지 입력");
    whitePanel(s);
    accentHeading(s, 0.65, 1.95, "중요 메시지", 14);
    hline(s, 3.6, 12.5, 2.15, C.DARK, 0.75);
    const n = 4, gap = 0.28, x0 = 0.7, total = G.INNER_W - 0.4, cw = (total - gap * (n - 1)) / n;
    for (let i = 0; i < n; i++) {
      const x = x0 + i * (cw + gap);
      darkLabelBox(s, x, 2.75, cw, 0.5, "텍스트 입력", 13);
      rect(s, x, 3.3, cw, 1.75, C.CARD);
      txt(s, "텍스트 입력", { x: x + 0.15, y: 3.42, w: cw - 0.3, h: 0.35, fontSize: 13 });
    }
  }

  // 09 Content · 3 proposal rows ------------------------------------------------
  {
    const s = contentSlide(pres, "01. 메시지 입력");
    const rowH = 1.4, gap = 0.22, y0 = 1.6, labelW = 2.3;
    for (let i = 0; i < 3; i++) {
      const y = y0 + i * (rowH + gap);
      darkLabelBox(s, 0.65, y, labelW, rowH, "제안사항\n입력", 18);
      rect(s, 0.65 + labelW + 0.15, y, G.INNER_W - labelW - 0.45, rowH, C.PANEL);
      txt(s, [{ text: "텍스트 입력", options: { bullet: true } }], { x: 0.65 + labelW + 0.3, y, w: 9.4, h: rowH, fontSize: 13, valign: "middle" });
    }
  }

  // 10 Content · proposal row + 3 detail boxes ---------------------------------
  {
    const s = contentSlide(pres, "01. 메시지 입력");
    const labelW = 2.3, rowH = 1.4;
    darkLabelBox(s, 0.65, 1.6, labelW, rowH, "제안사항\n입력", 18);
    rect(s, 0.65 + labelW + 0.15, 1.6, G.INNER_W - labelW - 0.45, rowH, C.PANEL);
    txt(s, [{ text: "텍스트 입력", options: { bullet: true } }], { x: 0.65 + labelW + 0.3, y: 1.6, w: 9.4, h: rowH, fontSize: 13, valign: "middle" });

    txt(s, "내용 입력", { x: 0.65, y: 3.3, w: 2.0, h: 0.35, fontSize: 14, bold: true, valign: "middle" });
    hline(s, 2.6, 12.7, 3.48, C.DARK, 0.75);

    const n = 3, gap = 0.22, x0 = 0.65, cw = (G.INNER_W - 0.3 - gap * (n - 1)) / n;
    for (let i = 0; i < n; i++) {
      const x = x0 + i * (cw + gap);
      rect(s, x, 3.85, cw, 2.75, C.PANEL);
      txt(s, "텍스트 입력", { x: x + 0.15, y: 4.0, w: cw - 0.3, h: 0.35, fontSize: 13 });
    }
  }

  // 11 Content · big heading + description + box -------------------------------
  {
    const s = contentSlide(pres, "01. 메시지 입력");
    txt(s, "제목", { x: 0.7, y: 1.5, w: 1.5, h: 0.6, fontSize: 28, bold: true, valign: "middle" });
    hline(s, 2.2, 12.7, 1.8, C.DARK, 0.75);
    txt(s, "텍스트", { x: 0.7, y: 2.15, w: 11.5, h: 0.3, fontSize: 11, color: C.MUTED });
    rect(s, 0.7, 2.65, G.INNER_W - 0.4, 3.95, C.PANEL);
    txt(s, "텍스트", { x: 0.9, y: 2.85, w: 11.0, h: 0.3, fontSize: 11 });
  }

  // 12 Content · two panels with dot bullets -----------------------------------
  {
    const s = contentSlide(pres, "01. 메시지 입력");
    const gap = 0.2, pw = (G.INNER_W - 0.3 - gap) / 2;
    for (let i = 0; i < 2; i++) {
      const x = 0.65 + i * (pw + gap), y = 1.55, h = G.PANEL_BOTTOM - 0.05 - y;
      rect(s, x, y, pw, h, C.PANEL);
      s.addShape("ellipse", { x: x + 0.2, y: y + 0.24, w: 0.22, h: 0.22, fill: { color: C.DARK }, line: { color: C.DARK, width: 0 } });
      txt(s, "텍스트 입력", { x: x + 0.55, y: y + 0.18, w: pw - 0.8, h: 0.35, fontSize: 14, bold: true, valign: "middle" });
      txt(s, "텍스트 입력", { x: x + 0.2, y: y + 0.65, w: pw - 0.4, h: 0.35, fontSize: 12 });
    }
  }

  // 13 Content · key message + 3 timeline columns ------------------------------
  {
    const s = contentSlide(pres, "01. 메시지를 입력");
    whitePanel(s);
    const lineY = 2.35;
    txt(s, "내용을 입력", { x: 0.85, y: 1.75, w: 3.5, h: 0.35, fontSize: 13, bold: true });
    hline(s, 0.6, 12.7, lineY, C.DARK, 1.0);
    txt(s, "중요한 메시지를\n입력해 주시기 바랍니다.", { x: 0.85, y: 2.6, w: 4.0, h: 1.1, fontSize: 22, bold: true, lineSpacingMultiple: 1.15 });
    wordmark(s, 0.85, 3.8, 40, C.DARK, "left", 2.0);
    txt(s, "내용을 입력", { x: 0.85, y: 4.9, w: 3.0, h: 0.3, fontSize: 10, color: C.MUTED });

    const colX = [5.0, 7.15, 9.5], entries = [1, 2, 3];
    colX.forEach((x, i) => {
      txt(s, "내용 입력", { x: x - 0.15, y: 1.75, w: 2.0, h: 0.35, fontSize: 13, bold: true });
      s.addShape("ellipse", { x: x - 0.06, y: lineY - 0.06, w: 0.12, h: 0.12, fill: { color: C.DARK }, line: { color: C.DARK, width: 0 } });
      vline(s, x, lineY, G.PANEL_BOTTOM - 0.15, C.LINE, 0.5, "sysDot");
      for (let k = 0; k < entries[i]; k++) {
        const y = 2.6 + k * 0.6;
        txt(s, "1900.00", { x: x + 0.08, y, w: 1.8, h: 0.3, fontFace: F.EN, fontSize: 13, bold: true });
        txt(s, "내용 입력", { x: x + 0.3, y: y + 0.27, w: 1.6, h: 0.25, fontSize: 10 });
      }
    });
  }

  // 14-17 Icon libraries ---------------------------------------------------------
  await iconLibrarySlide(pres, [
    { label: "List", icon: lu.LuList }, { label: "Copy", icon: lu.LuCopy }, { label: "TV", icon: lu.LuTv }, { label: "Image", icon: lu.LuImage }, { label: "Human", icon: lu.LuUsers },
    { label: "Laptop", icon: lu.LuLaptop }, { label: "Folder", icon: lu.LuFolder }, { label: "Creative", icon: lu.LuLightbulb }, { label: "Video", icon: lu.LuVideo }, { label: "Set-up", icon: lu.LuSettings },
    { label: "Mobile", icon: lu.LuSmartphone }, { label: "Pie chart", icon: lu.LuChartPie }, { label: "Mail", icon: lu.LuMail }, { label: "Wifi(router)", icon: lu.LuRouter }, { label: "Chart", icon: lu.LuChartBar },
  ], "아이콘 라이브러리 A (일반). 아이콘은 Lucide(ISC 라이선스) 라인 아이콘으로 대체되어 있으며, 사내 공식 아이콘으로 교체 가능합니다.");

  await iconLibrarySlide(pres, [
    { label: "Lock", icon: lu.LuLock }, { label: "Employee ID", icon: lu.LuIdCard }, { label: "Schedule", icon: lu.LuCalendar }, { label: "Home", icon: lu.LuHouse }, { label: "Government\nOffice", icon: lu.LuLandmark },
    { label: "Alert", icon: lu.LuTriangleAlert }, { label: "Money", icon: lu.LuCoins }, { label: "Cloud", icon: lu.LuCloud }, { label: "Trash", icon: lu.LuTrash2 }, { label: "Mute", icon: lu.LuVolumeX },
    { label: "Sound", icon: lu.LuVolume2 }, { label: "Location", icon: lu.LuMapPin }, { label: "Certificate", icon: lu.LuFileBadge }, { label: "Target", icon: lu.LuTarget }, { label: "Projector", icon: lu.LuProjector },
  ], "아이콘 라이브러리 B (일반). Lucide 라인 아이콘 사용.");

  await iconLibrarySlide(pres, [
    { label: "천궁-II", svg: "천궁-II" }, { label: "LSAM AAM", svg: "LSAM AAM" }, { label: "LSAM ABM", svg: "LSAM ABM" }, { label: "LAMD", svg: "LAMD" }, { label: "해궁", svg: "해궁" },
    { label: "홍상어", svg: "홍상어" }, { label: "천궁 발사차량", svg: "천궁 발사차량" }, { label: "천궁 MFR차량", svg: "천궁 MFR차량" }, { label: "천궁\n지휘통제차량", svg: "천궁 지휘통제차량" }, { label: "CIWS-II", svg: "CIWS-II" },
    { label: "KF21", svg: "KF21" }, { label: "KDDX", svg: "KDDX" }, { label: "정찰용무인\n수상정", svg: "정찰용무인수상정" }, { label: "자폭용무인\n수상정", svg: "자폭용무인수상정" }, { label: "AUV", svg: "AUV" },
  ], "제품 픽토그램 라이브러리 A. 아이콘은 lig_icons.js의 원본 SVG로 생성되었으며 사내 공식 픽토그램으로 교체 가능합니다.");

  await iconLibrarySlide(pres, [
    { label: "국지방공레이더\n차량", svg: "국지방공레이더 차량" }, { label: "고스트로보틱스\n비전60", svg: "고스트로보틱스 비전60" }, { label: "KPS위성", svg: "KPS위성" }, { label: "SAR위성", svg: "SAR위성" }, { label: "40kg 카고드론", svg: "40kg 카고드론" },
    { label: "다목적무인헬기", svg: "다목적무인헬기" }, { label: "레이저소화기\n착용군인", svg: "레이저소화기 착용군인" }, { label: "헬멧착용 군인", svg: "헬멧착용 군인" }, { label: "중형무인기", svg: "중형무인기" }, { label: "비궁", svg: "비궁" },
    { label: "해룡", svg: "해룡" }, { label: "장보고 Batch-III", svg: "장보고 Batch-III" }, { label: "초소형 위성", svg: "초소형 위성" }, { label: "장거리공대지\n유도탄", svg: "장거리공대지 유도탄" }, { tagline: "Leading in the Greater World" },
  ], "제품 픽토그램 라이브러리 B. 아이콘은 lig_icons.js의 원본 SVG로 생성되었으며 사내 공식 픽토그램으로 교체 가능합니다.");

  await pres.writeFile({ fileName: outFile });
  return outFile;
}

// ─────────────────────────────────────────────────────────────────────────────
const out = path.resolve(process.argv[2] || "LIG_Template.pptx");
build(out)
  .then((f) => console.log(`written: ${f}`))
  .catch((err) => { console.error(err); process.exit(1); });
