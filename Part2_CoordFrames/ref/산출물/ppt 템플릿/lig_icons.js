/**
 * LIG product pictograms — original line icons drawn to match the template's
 * outline style (uniform stroke, round caps/joins, no fills).
 *
 * Every icon lives in a 100×100 viewBox and is exported as a factory
 * `(color, strokeWidth) => svgString` so the deck builder can pick the palette.
 */
"use strict";

// ─── primitives ──────────────────────────────────────────────────────────────
const wrap = (inner, color, sw, extra = "") =>
  `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="400" height="400" ` +
  `fill="none" stroke="#${color}" stroke-width="${sw}" stroke-linecap="round" stroke-linejoin="round"${extra}>${inner}</svg>`;

const g = (inner, transform) => (transform ? `<g transform="${transform}">${inner}</g>` : inner);
const path = (d) => `<path d="${d}"/>`;
const circle = (cx, cy, r) => `<circle cx="${cx}" cy="${cy}" r="${r}"/>`;
const rect = (x, y, w, h, rx = 0) => `<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="${rx}"/>`;
const line = (x1, y1, x2, y2) => `<line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}"/>`;

/** Gentle water line under boats. */
const waves = (y = 88) => path(`M6,${y} q7,-5 14,0 t14,0 t14,0 t14,0 t14,0 t14,0`);

/** Concentric "signal" arcs opening toward +x, centred at (cx,cy). */
const signal = (cx, cy, rs = [8, 15]) =>
  rs.map((r) => path(`M${cx + r * 0.7},${cy - r * 0.7} A${r},${r} 0 0 1 ${cx + r * 0.7},${cy + r * 0.7}`)).join("");

/**
 * Parametric missile, nose up. `fins`: [{ y (root bottom), len (span), h (root height), sweep }]
 * Rotate 90 → nose right.
 */
function missile({ w = 12, top = 8, noseLen = 16, bottom = 82, fins = [], bands = [], nozzle = true, rotate = 0, cx = 50 }) {
  const x1 = cx - w / 2, x2 = cx + w / 2, bt = top + noseLen;
  const parts = [`M${x1},${bt} L${cx},${top} L${x2},${bt} L${x2},${bottom} L${x1},${bottom} Z`];
  fins.forEach(({ y, len, h, sweep = 0 }) => {
    parts.push(`M${x1},${y - h} L${x1 - len},${y + sweep} L${x1 - len},${y} L${x1},${y}`);
    parts.push(`M${x2},${y - h} L${x2 + len},${y + sweep} L${x2 + len},${y} L${x2},${y}`);
  });
  bands.forEach((y) => parts.push(`M${x1},${y} L${x2},${y}`));
  if (nozzle) parts.push(`M${x1 + 2},${bottom} L${x1 + 1},${bottom + 6} L${x2 - 1},${bottom + 6} L${x2 - 2},${bottom}`);
  return g(path(parts.join(" ")), rotate ? `rotate(${rotate} 50 50)` : "");
}

/** Military truck chassis (cab left) — payload is drawn by the caller on top of the bed (y ≤ 60). */
function truck() {
  return [
    path("M6,60 L6,50 L12,50 L17,40 L32,40 L32,60"), // cab
    path("M20,44 L29,44 L29,52 L20,52"), // window
    path("M6,60 L94,60 L94,70 L6,70 Z"), // bed
    circle(20, 74, 6), circle(50, 74, 6), circle(76, 74, 6),
    line(26, 74, 44, 74), line(56, 74, 70, 74),
  ].join("");
}

// ─── page 16 : 방공·유도무기 / 함정 ────────────────────────────────────────────
const P16 = {
  "천궁-II": () => missile({ w: 11, top: 6, noseLen: 18, bottom: 82, fins: [{ y: 82, len: 10, h: 14, sweep: -3 }], bands: [30, 34] }),
  "LSAM AAM": () => missile({ w: 12, top: 6, noseLen: 16, bottom: 84, fins: [{ y: 52, len: 8, h: 10, sweep: -3 }, { y: 84, len: 11, h: 12 }] }),
  "LSAM ABM": () => missile({ w: 10, top: 4, noseLen: 24, bottom: 86, fins: [{ y: 86, len: 8, h: 10 }], bands: [44, 60] }),
  LAMD: () => missile({ w: 14, top: 8, noseLen: 14, bottom: 80, fins: [{ y: 58, len: 9, h: 12, sweep: -4 }, { y: 80, len: 7, h: 8 }] }),
  해궁: () => missile({ w: 13, top: 12, noseLen: 14, bottom: 78, fins: [{ y: 40, len: 6, h: 6 }, { y: 78, len: 13, h: 16, sweep: -4 }] }),
  홍상어: () => missile({ w: 14, top: 12, noseLen: 8, bottom: 82, fins: [{ y: 82, len: 9, h: 12 }], bands: [40, 62], rotate: 90 }),
  "천궁 발사차량": () =>
    truck() +
    g(
      [0, 1, 2, 3].map((i) => rect(44, 26 + i * 7, 44, 5)).join("") + path("M44,26 L44,54"),
      "rotate(-38 50 58)"
    ) + path("M40,60 L40,52"),
  "천궁 MFR차량": () =>
    truck() +
    g(rect(48, 24, 34, 24) + path("M48,32 L82,32 M48,40 L82,40 M56,24 L56,48 M65,24 L65,48 M74,24 L74,48"), "rotate(-18 65 36)") +
    path("M64,60 L64,50") + signal(88, 34, [7, 13]),
  "천궁 지휘통제차량": () =>
    truck() + rect(38, 34, 50, 26) + path("M38,44 L88,44 M52,34 L52,60") + line(80, 34, 80, 22) + circle(80, 19, 3),
  "CIWS-II": () =>
    [
      path("M22,88 L78,88 L74,78 L26,78 Z"), // base
      path("M30,78 L30,60 L38,52 L62,52 L70,60 L70,78"), // turret
      path("M30,68 L70,68"),
      circle(50, 60, 4),
      path("M60,56 L84,44"), path("M64,62 L88,50"), path("M84,44 L88,50"), // twin barrel + muzzle
      path("M88,38 L94,32 M92,48 L98,48 M90,56 L96,60"), // fire
    ].join(""),
  KF21: () =>
    g(
      [
        path("M50,6 L44,24 L44,68 L50,90 L56,68 L56,24 Z"), // fuselage
        path("M44,38 L12,66 L20,72 L44,62"), path("M56,38 L88,66 L80,72 L56,62"), // delta wings
        path("M44,70 L30,86 L36,88 L46,78"), path("M56,70 L70,86 L64,88 L54,78"), // tail
        path("M47,26 L53,26 L52,40 L48,40 Z"), // canopy
      ].join(""),
      "rotate(-28 50 50)"
    ),
  KDDX: () =>
    [
      path("M4,66 L12,84 L90,84 L98,66 Z"), // hull
      path("M30,66 L30,54 L72,54 L72,66"), // deck house
      path("M40,54 L40,42 L58,42 L58,54"), // bridge
      line(49, 42, 49, 26), path("M42,32 L56,32"), // mast
      rect(14, 60, 10, 6), path("M24,63 L34,60"), // gun
      circle(66, 48, 3),
    ].join(""),
  정찰용무인수상정: () =>
    [
      path("M18,66 L26,78 L78,78 L88,66 Z"),
      path("M42,66 L42,56 L62,56 L62,66"), line(52, 56, 52, 46),
      path("M44,46 A8,8 0 0 1 60,46"), path("M38,40 A14,14 0 0 1 66,40"),
      waves(88),
    ].join(""),
  자폭용무인수상정: () =>
    [
      path("M14,66 L22,78 L80,78 L92,66 Z"),
      path("M46,66 L46,60 L60,60 L60,66"),
      line(66, 60, 66, 40), circle(66, 36, 3), path("M60,40 L72,40"),
      path("M26,66 L36,60"), // bow chine
      waves(88),
    ].join(""),
  AUV: () =>
    [
      path("M18,40 L70,40 A12,10 0 0 1 70,60 L18,60 A6,10 0 0 1 18,40 Z"),
      path("M18,42 L8,34 M18,58 L8,66 M14,50 L6,50"), // tail fins + prop shaft
      path("M40,40 L46,32 L52,40"), // dorsal fin
      path("M26,50 L34,50"),
    ].join(""),
};

// ─── page 17 : 레이더·무인체계·항공우주 ───────────────────────────────────────
const P17 = {
  "국지방공레이더 차량": () =>
    truck() + line(64, 60, 64, 36) + g(rect(48, 20, 38, 16) + path("M48,28 L86,28 M57,20 L57,36 M67,20 L67,36 M77,20 L77,36"), "rotate(-12 67 28)"),
  "고스트로보틱스 비전60": () =>
    [
      rect(24, 38, 52, 16, 5), // body
      path("M76,40 L90,38 L90,48 L76,50"), circle(85, 43, 2), // head
      line(40, 38, 40, 28),
      path("M70,54 L66,68 L74,82"), path("M60,54 L56,68 L64,82"), // front legs
      path("M32,54 L36,68 L28,82"), path("M42,54 L46,68 L38,82"), // hind legs
    ].join(""),
  KPS위성: () =>
    g(
      [
        rect(41, 41, 18, 18),
        rect(6, 44, 28, 12), path("M13,44 L13,56 M20,44 L20,56 M27,44 L27,56 M6,50 L34,50"),
        rect(66, 44, 28, 12), path("M73,44 L73,56 M80,44 L80,56 M87,44 L87,56 M66,50 L94,50"),
        line(34, 50, 41, 50), line(59, 50, 66, 50),
        line(50, 41, 50, 32), path("M42,32 A8,8 0 0 1 58,32"),
      ].join(""),
      "rotate(-30 50 50)"
    ),
  SAR위성: () =>
    [
      g(rect(36, 22, 22, 22) + rect(4, 26, 26, 12) + path("M11,26 L11,38 M18,26 L18,38 M4,32 L30,32") + line(30, 32, 36, 32), "rotate(-20 47 33)"),
      line(50, 44, 50, 58), rect(20, 58, 60, 10), path("M32,58 L32,68 M44,58 L44,68 M56,58 L56,68 M68,58 L68,68"),
      path("M42,78 A10,10 0 0 0 58,78"), path("M35,86 A17,17 0 0 0 65,86"),
    ].join(""),
  "40kg 카고드론": () =>
    [
      line(18, 40, 82, 40), line(8, 32, 30, 32), line(19, 32, 19, 40), line(70, 32, 92, 32), line(81, 32, 81, 40),
      rect(40, 36, 20, 10, 2), path("M42,46 L36,56 M58,46 L64,56"),
      line(50, 46, 50, 60), rect(36, 60, 28, 24), path("M36,72 L64,72 M50,60 L50,84"),
    ].join(""),
  다목적무인헬기: () =>
    [
      path("M22,58 Q22,44 36,44 L58,44 Q72,44 72,58 Q72,70 58,70 L36,70 Q22,70 22,58 Z"),
      path("M58,48 Q68,48 68,58 L58,58 Z"),
      path("M72,54 L94,50 M72,62 L90,60"), line(92, 42, 92, 60),
      line(48, 44, 48, 36), line(10, 36, 86, 36),
      path("M32,70 L32,78 M60,70 L60,78 M24,78 L68,78"),
    ].join(""),
  "레이저소화기 착용군인": () =>
    [
      path("M36,26 L62,26 M40,26 A10,10 0 0 1 60,26"), // helmet
      path("M42,26 L42,34 Q42,40 50,40 Q58,40 58,34 L58,26"), // face
      path("M38,42 L60,42 L62,70 L36,70 Z"), path("M40,70 L38,92 M56,70 L58,92"),
      path("M22,56 L76,50"), path("M62,50 L70,60"), path("M42,48 L30,58"), path("M58,48 L62,56"),
    ].join(""),
  "헬멧착용 군인": () =>
    [
      path("M40,24 A12,12 0 0 1 64,24 L64,28 L40,28 Z"), path("M43,28 L43,36 Q43,42 52,42 Q61,42 61,36 L61,28"),
      path("M38,46 L66,46 L68,72 L36,72 Z"), path("M44,46 L44,72 M60,46 L60,72"),
      path("M38,48 L28,66 M66,48 L76,66"), path("M42,72 L40,92 M62,72 L64,92"),
      path("M14,72 L14,40 M8,46 L14,40 L20,46"), // forward arrow
    ].join(""),
  중형무인기: () =>
    g(
      [
        path("M50,12 L46,22 L46,72 L50,88 L54,72 L54,22 Z"),
        path("M46,40 L6,42 L6,48 L46,48"), path("M54,40 L94,42 L94,48 L54,48"),
        path("M46,74 L32,84 L34,88 L46,80"), path("M54,74 L68,84 L66,88 L54,80"),
        circle(50, 18, 2),
      ].join(""),
      "rotate(-25 50 50)"
    ),
  비궁: () => missile({ w: 10, top: 6, noseLen: 20, bottom: 84, fins: [{ y: 84, len: 8, h: 10 }], bands: [36], rotate: 90 }),
  해룡: () => missile({ w: 12, top: 8, noseLen: 14, bottom: 84, fins: [{ y: 54, len: 12, h: 18, sweep: -4 }, { y: 84, len: 7, h: 8 }], rotate: 90 }),
  "장보고 Batch-III": () =>
    [
      path("M8,58 Q8,44 22,44 L78,44 Q94,44 94,58 Q94,72 78,72 L22,72 Q8,72 8,58 Z"),
      path("M40,44 L44,30 L60,30 L64,44"), line(52, 30, 52, 20),
      path("M14,48 L8,38 M8,58 L2,50 M8,58 L2,66"), path("M60,58 L82,58"),
    ].join(""),
  "초소형 위성": () =>
    [
      rect(36, 42, 24, 24), path("M36,42 L46,32 L70,32 L60,42"), path("M60,42 L70,32 L70,56 L60,66"),
      path("M36,54 L60,54 M48,42 L48,66"),
      rect(8, 46, 22, 12), path("M15,46 L15,58 M22,46 L22,58"), rect(74, 36, 22, 12), path("M81,36 L81,48 M88,36 L88,48"),
      path("M4,22 L22,22 M2,30 L14,30 M6,38 L18,38"), // motion streaks
    ].join(""),
  "장거리공대지 유도탄": () =>
    g(
      [
        path("M50,6 L46,16 L46,80 L50,92 L54,80 L54,16 Z"),
        path("M46,44 L18,52 L18,56 L46,56"), path("M54,44 L82,52 L82,56 L54,56"),
        path("M46,76 L36,86 L46,82"), path("M54,76 L64,86 L54,82"),
        path("M46,64 L42,66 L42,72 L46,72"), // intake
      ].join(""),
      "rotate(-38 50 50)"
    ),
};

/** Build one SVG document for a named icon. */
function svgFor(name, color = "2C2C2C", strokeWidth = 3.6) {
  const fn = P16[name] || P17[name];
  if (!fn) throw new Error(`Unknown pictogram: ${name}`);
  return wrap(fn(), color, strokeWidth);
}

module.exports = { svgFor, names16: Object.keys(P16), names17: Object.keys(P17) };
