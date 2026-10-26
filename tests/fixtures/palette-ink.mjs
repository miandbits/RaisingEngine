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
