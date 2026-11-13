import assert from 'node:assert/strict';

// Screenshots contain the rendered viewport, not the surrounding window bars.
// Express coverage in logical square pixels so Retina density cannot satisfy
// an ink assertion merely by supplying more raster pixels.
export function paletteInkCoverage(image, viewport, box, color) {
  const sx = image.width / viewport.width, sy = image.height / viewport.height;
  assert.ok(Number.isFinite(sx) && sx > 0 && Number.isFinite(sy) && sy > 0);
  assert.match(color, /^#[0-9a-f]{6}$/i);
  const expected = [1, 3, 5].map(i => parseInt(color.slice(i, i + 2), 16));
  const bounds = {
    left: Math.max(0, Math.ceil(box.x * sx)),
    top: Math.max(0, Math.ceil(box.y * sy)),
    right: Math.min(image.width, Math.ceil((box.x + box.width) * sx)),
    bottom: Math.min(image.height, Math.ceil((box.y + box.height) * sy)),
  };
  assert.ok(bounds.right > bounds.left && bounds.bottom > bounds.top);
  let pixels = 0;
  for (let y = bounds.top; y < bounds.bottom; y++) {
    for (let x = bounds.left; x < bounds.right; x++) {
      const at = (y * image.width + x) * 4;
      if (expected.every((value, i) => Math.abs(image.pixels[at + i] - value) < 4)) pixels++;
    }
  }
  return {pixels, logicalArea: pixels / (sx * sy), scaleX: sx, scaleY: sy, bounds};
}

// Antialiased glyphs are mixtures of the authored ink and solid background.
// Least-squares alpha must explain every RGB channel within 4 levels. Ignore
// coverage below 20% (near-background noise), allow 2% quantization overshoot,
// and require a >1-level residual advantage over the enabled ink mixture.
// That last comparison rejects similar-hued enabled glyphs instead of counting
// them as disabled ink. Alpha-weighted coverage is measured in logical pixels;
// this is a raster-presence check, not a legibility certification.
export function paletteBlendCoverage(image, viewport, box, foreground, background, enabledForeground) {
  const exact = paletteInkCoverage(image, viewport, box, foreground);
  const rgb = color => {
    assert.match(color, /^#[0-9a-f]{6}$/i);
    return [1, 3, 5].map(i => parseInt(color.slice(i, i + 2), 16));
  };
  const bg = rgb(background);
  const vector = color => {
    const delta = rgb(color).map((value, i) => value - bg[i]);
    const norm = delta.reduce((sum, value) => sum + value * value, 0);
    assert.ok(norm > 0, 'Foreground must differ from the solid background');
    return {delta, norm};
  };
  const target = vector(foreground), alternative = vector(enabledForeground);
  const fit = (pixel, vector, clamp) => {
    let alpha = pixel.reduce((sum, value, i) => sum + (value - bg[i]) * vector.delta[i], 0) / vector.norm;
    if (clamp) alpha = Math.max(0, Math.min(1, alpha));
    return {alpha, residual: Math.max(...pixel.map((value, i) => Math.abs(value - (bg[i] + alpha * vector.delta[i]))))};
  };
  let blendPixels = 0, weightedPixels = 0;
  for (let y = exact.bounds.top; y < exact.bounds.bottom; y++) {
    for (let x = exact.bounds.left; x < exact.bounds.right; x++) {
      const at = (y * image.width + x) * 4;
      const pixel = [0, 1, 2].map(i => image.pixels[at + i]);
      const match = fit(pixel, target, false);
      if (match.alpha < .2 || match.alpha > 1.02 || match.residual >= 4) continue;
      if (fit(pixel, alternative, true).residual <= match.residual + 1) continue;
      blendPixels++;
      weightedPixels += Math.min(1, match.alpha);
    }
  }
  return {...exact, blendPixels, weightedPixels, logicalCoverage: weightedPixels / (exact.scaleX * exact.scaleY)};
}
