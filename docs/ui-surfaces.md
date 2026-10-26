# Decorative UI surfaces

Use **Appearance → Background surface…** to add a texture to an element. The same editor is available in shared/default styles, reusable component children and instance overrides, conditional presentation variants, and the reading-layout background. Choose a registered PNG image; no code or JSON editing is needed. Surface images must use a .png path and stay within 20 MiB, 8192 pixels per side, and 32 megapixels in total.

- **Inherit** removes this setting, so the normal style or component inheritance applies.
- **None** explicitly clears an inherited texture.
- **Tile** repeats the entire PNG from the top-left corner.
- **Nine-slice** preserves the corners and stretches the edges and center. Enter the top, right, bottom, and left edge thicknesses in source pixels. The source preview draws the four slice guides beside a resized example.

Opposing edge thicknesses must total less than the image width or height, leaving at least one center pixel. The editor rejects an unreadable or non-PNG asset and invalid slice dimensions. Cancel leaves the original value untouched.

One source pixel is one logical layout unit. If a target is smaller than its opposing caps, those caps shrink proportionally on that axis. The texture paints inside the existing border; it does not change the element's dimensions, padding, clipping, or click area. Transparent pixels show the existing fill. PNG alpha defines textured corner shapes; the existing radius still controls the solid fill and border. Authored hover/pressed fills replace a button's base texture in those states.

The canvas previews tiles and nine-slice geometry for authored elements and resolved component styles. Use **Inspect renderer / Layout checks** to review the actual selected game state, especially reading-layout backgrounds and generated widgets. A texture can reduce text contrast; image-backed contrast still needs visual review, and automatic layout checks do not certify accessibility.

Shared-style changes update every connected element. Element edits, shared styles, component edits, variants, and reading-layout changes use the normal undo/save workflow. Instance overrides retain their own texture until reset or returned to inheritance.

This version supports whole PNG images only. It does not add sprite-atlas regions, density variants, or a theme package system.

## Shared control colors

For a schedule, save-record, or load-record widget, choose **Appearance → Control color style** and select an existing shared style. **Reading layouts → Reading tool color style** sets the colors for generated reading tools and supplies the default for widgets that have no explicit control style while reading layout is active. An explicit widget reference wins. Leaving both unset preserves the existing player colors.

Only the named style's background, text, border, hover, pressed, and disabled-text colors apply. Unspecified colors retain the player defaults; project default-style colors are not copied. Fonts, size, padding, radius, and textured surfaces in that style do not change generated controls. Use the actual renderer inspector to review normal, focused, disabled, and confirmation states.

These references support the ordinary undo, save, and reopen workflow. A missing style remains visible in the picker so it can be repaired, and validation reports it. The shared-style editor prevents deleting a style still referenced by generated controls or the reading layout. Color styles do not change actions, routing, focus order, or hit areas.
