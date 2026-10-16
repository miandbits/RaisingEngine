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
