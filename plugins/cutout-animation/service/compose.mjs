// Layer compositing of the layer import (plugin 1.15.0): what turns a Photoshop clipping group or
// a group marked to merge into ONE picture. Pictures are straight-alpha RGBA rectangles
// {left, top, width, height, rgba} in the import's picture pixels; a Surface is a float canvas
// (straight colour 0..1, alpha 0..1) that pictures are blended onto, rounded to 8 bits once.
//
// Math (docs/contract.md "Layer import", W3C Compositing Level 1 separable blend modes):
//   * blendOnto (a layer onto the pictures below it inside a merged group, isolated):
//       as = source alpha × opacity, mix = (1 − ab)·Cs + ab·B(Cb, Cs)
//       ao = as + ab·(1 − as),  Co = (as·mix + ab·(1 − as)·Cb) / ao
//   * paintLocked (a layer clipped to its base): Photoshop limits a clipped layer to the base's
//     alpha, i.e. it paints with the base's transparency locked:
//       t = source alpha × opacity, Co = (1 − t)·Cb + t·B(Cb, Cs), ao = ab (unchanged)
//   B: normal Cs; multiply Cb·Cs; screen Cb + Cs − Cb·Cs; overlay = hard light with the layers
//   swapped; linear dodge (add) min(1, Cb + Cs); lighten max; darken min; soft light = the W3C
//   formula (Photoshop's own curve differs by a few levels in the mid tones). Opacity always mixes
//   linearly (Photoshop's "special" fill behaviour of add-like modes is not reproduced). Other
//   modes are drawn as normal (the importer warns).
//   * paintClippedGroup (plugin 1.19.1, a layer clipped to a group that stays several pictures):
//     the same locked paint, into every picture that shows at the pixel (see the function).

/** Blend keys composited exactly when baking (`pass` = a group's pass-through, drawn isolated). */
export const BAKED_BLENDS = new Set(['norm', 'pass', 'mul ', 'scrn', 'over', 'lddg', 'lite', 'dark', 'sLit']);
const softLight = (b, s) => s <= 0.5 ? b - (1 - 2 * s) * b * (1 - b) : b + (2 * s - 1) * ((b <= 0.25 ? ((16 * b - 12) * b + 4) * b : Math.sqrt(b)) - b);
const MODES = {
  'mul ': (b, s) => b * s,
  scrn: (b, s) => b + s - b * s,
  over: (b, s) => b <= 0.5 ? 2 * b * s : 1 - 2 * (1 - b) * (1 - s),
  lddg: (b, s) => Math.min(1, b + s),
  lite: (b, s) => Math.max(b, s),
  dark: (b, s) => Math.min(b, s),
  sLit: softLight,
};
/** The blend function of a key, or null for normal (and every mode drawn as normal). */
const modeOf = blend => MODES[blend] ?? null;

export class Surface {
  /** A transparent canvas over the rectangle; `alphaOnly` keeps only coverage (previews). */
  constructor({ left, top, width, height }, alphaOnly = false) {
    Object.assign(this, { left, top, width, height, alphaOnly });
    this.data = new Float32Array(width * height * 4);
  }
  /** A surface holding a picture (the base of a clipping group). */
  static of(picture, alphaOnly = false) {
    const s = new Surface(picture, alphaOnly), { rgba } = picture, d = s.data;
    for (let i = 0; i < rgba.length; i++) d[i] = rgba[i] / 255;
    return s;
  }
  /** Calls fn(dstIndex, srcIndex) for every pixel where `picture` overlaps this surface. */
  each(picture, fn) {
    const x0 = Math.max(this.left, picture.left), y0 = Math.max(this.top, picture.top);
    const x1 = Math.min(this.left + this.width, picture.left + picture.width), y1 = Math.min(this.top + this.height, picture.top + picture.height);
    for (let y = y0; y < y1; y++) {
      let d = ((y - this.top) * this.width + (x0 - this.left)) * 4, s = ((y - picture.top) * picture.width + (x0 - picture.left)) * 4;
      for (let x = x0; x < x1; x++, d += 4, s += 4) fn(d, s);
    }
  }
  /** Source-over with a blend mode and opacity (an isolated group's children, back to front). */
  blendOnto(picture, blend, opacity = 1) {
    const d = this.data, src = picture.rgba, B = modeOf(blend), k = opacity / 255;
    this.each(picture, (i, j) => {
      const as = src[j + 3] * k;
      if (as <= 0) return;
      const ab = d[i + 3], ao = as + ab * (1 - as);
      d[i + 3] = ao;
      if (this.alphaOnly) return;
      for (let c = 0; c < 3; c++) {
        const cs = src[j + c] / 255, cb = d[i + c], mix = B ? (1 - ab) * cs + ab * B(cb, cs) : cs;
        d[i + c] = (as * mix + ab * (1 - as) * cb) / ao;
      }
    });
  }
  /** A clipped layer: painted inside this surface's alpha (transparency locked). */
  paintLocked(picture, blend, opacity = 1) {
    if (this.alphaOnly) return;  // the alpha stays the base's: nothing to compute for a preview
    const d = this.data, src = picture.rgba, B = modeOf(blend), k = opacity / 255;
    this.each(picture, (i, j) => {
      const t = src[j + 3] * k;
      if (t <= 0 || d[i + 3] <= 0) return;
      for (let c = 0; c < 3; c++) {
        const cs = src[j + c] / 255, cb = d[i + c];
        d[i + c] = (1 - t) * cb + t * (B ? B(cb, cs) : cs);
      }
    });
  }
  /** 8-bit straight RGBA of the whole surface. */
  picture() {
    const rgba = Buffer.alloc(this.width * this.height * 4), d = this.data;
    for (let i = 0; i < d.length; i += 4) {
      const a = Math.round(d[i + 3] * 255);
      if (!a) continue;
      rgba[i + 3] = a;
      for (let c = 0; c < 3; c++) rgba[i + c] = Math.max(0, Math.min(255, Math.round(d[i + c] * 255)));
    }
    return { left: this.left, top: this.top, width: this.width, height: this.height, rgba };
  }
}

/**
 * Layers clipped to a group that is not merged (plugin 1.19.1). Photoshop clips them to the
 * group's composite alpha; the rig draws the group's pictures one by one, so the effect is painted
 * into the pictures that make that composite at each pixel: the topmost picture covering it and,
 * where it is partly transparent, the pictures showing through it, down to the first opaque one.
 * Each gets the full strength (t = αs·opacity, transparency locked as paintLocked): the drawn mix
 * Σ wᵢ·Cᵢ' then equals Photoshop's (1 − t)·Cg + t·B(Cg, Cs) exactly for modes linear in the
 * backdrop (normal, multiply, screen, linear dodge below the clamp) and within a few levels for
 * overlay / soft light / lighten / darken, which see each picture's colour instead of the mix.
 * Without semi-transparent overlap (one picture per pixel) it is Photoshop's result for every
 * mode. Pixels under an opaque picture get nothing: the artist never saw the effect there.
 *   stack: the shown pictures, back → front: [{picture: {x, y, width, height, rgba}, opacity}]
 *     (opacity relative to the group: how much a picture hides the ones below it);
 *   variants: hidden pictures, painted within their own alpha as if shown in the stack, either in
 *     place of the shown picture of their slot (`replace`: stack index) or at their own draw
 *     position (`replace` −1, `at`: the number of stack pictures below them);
 *   clips: [{picture: {left, top, width, height, rgba}, blend, opacity}], bottom → top (in order).
 * Paints the pictures' rgba in place (colour only; alpha stays) and returns how many changed.
 */
export function paintClippedGroup(stack, variants, clips) {
  const n = stack.length, open = new Float64Array(n + 1), floats = new Map();
  const floatOf = m => {
    let f = floats.get(m);
    if (!f) { const { rgba } = m.picture; f = new Float32Array(rgba.length); for (let i = 0; i < rgba.length; i++) f[i] = rgba[i] / 255; floats.set(m, f); }
    return f;
  };
  const inside = (q, x, y) => x >= q.x && y >= q.y && x < q.x + q.width && y < q.y + q.height;
  for (const clip of clips) {
    const p = clip.picture, src = p.rgba, B = modeOf(clip.blend), k = clip.opacity / 255;
    const meets = m => { const q = m.picture; return q.x < p.left + p.width && p.left < q.x + q.width && q.y < p.top + p.height && p.top < q.y + q.height; };
    const hit = stack.map(meets), hitVariants = variants.filter(meets);
    // Only the part of the clip over the pictures it can reach.
    let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
    for (const m of [...stack.filter((_, s) => hit[s]), ...hitVariants]) {
      const q = m.picture;
      x0 = Math.min(x0, q.x); y0 = Math.min(y0, q.y); x1 = Math.max(x1, q.x + q.width); y1 = Math.max(y1, q.y + q.height);
    }
    x0 = Math.max(x0, p.left); y0 = Math.max(y0, p.top); x1 = Math.min(x1, p.left + p.width); y1 = Math.min(y1, p.top + p.height);
    const paint = (m, i, t, j) => {
      const f = floatOf(m);
      for (let c = 0; c < 3; c++) { const cs = src[j + c] / 255, cb = f[i + c]; f[i + c] = (1 - t) * cb + t * (B ? B(cb, cs) : cs); }
    };
    for (let y = y0; y < y1; y++) for (let x = x0; x < x1; x++) {
      const j = ((y - p.top) * p.width + (x - p.left)) * 4, t = src[j + 3] * k;
      if (t <= 0) continue;
      // open[s] = how much of the pixel the stack pictures s.. (front part) leave uncovered.
      open[n] = 1;
      for (let s = n - 1; s >= 0; s--) {
        open[s] = open[s + 1];
        const m = stack[s], q = m.picture;
        if (!hit[s] || !inside(q, x, y)) continue;
        const i = ((y - q.y) * q.width + (x - q.x)) * 4, a = q.rgba[i + 3] / 255 * m.opacity;
        if (q.rgba[i + 3] && open[s + 1] > 0) paint(m, i, t, j);
        if (a > 0) open[s] = open[s + 1] * (1 - a);
      }
      for (const v of hitVariants) {
        const q = v.picture;
        if (!inside(q, x, y)) continue;
        const i = ((y - q.y) * q.width + (x - q.x)) * 4;
        if (q.rgba[i + 3] && (v.replace >= 0 ? open[v.replace + 1] : open[v.at]) > 0) paint(v, i, t, j);
      }
    }
  }
  for (const [m, f] of floats) {
    const { rgba } = m.picture;
    for (let i = 0; i < rgba.length; i += 4) if (rgba[i + 3]) for (let c = 0; c < 3; c++) rgba[i + c] = Math.max(0, Math.min(255, Math.round(f[i + c] * 255)));
  }
  return floats.size;
}
