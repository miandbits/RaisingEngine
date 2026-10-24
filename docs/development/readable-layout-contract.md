# Readable layouts (implementation contract)

An opt-in `readable-layout-v1` presentation uses viewport CSS pixels on Web and SDL window points on native. Legacy `fit` and `responsive` canvases retain their behavior. Layout changes must never issue simulation commands or modify saves.

## Authored data

```json
{
  "readableLayout": {
    "maxShortEdge": 600,
    "fontSize": 18,
    "minTargetSize": 44,
    "padding": 16,
    "gap": 12,
    "maxWidth": 760,
    "background": "#172c29"
  },
  "screens": [{
    "id": "story",
    "elements": [],
    "readableLayout": {
      "items": [
        {"elementId": "calendar"},
        {"elementId": "portrait", "imageHeight": 120},
        {"elementId": "speaker", "fontSize": 24},
        {"elementId": "dialogue"},
        {"elementId": "choices"}
      ],
      "actions": ["advance", "save", "load"]
    }
  }]
}
```

Every global field is optional with the defaults above. `background` is an opaque six-digit hex color; authors select a surface that contrasts with their existing text colors. The global object enables the feature. A screen opts in with `readableLayout`; its `items` is required and may be empty when actions supply its interface. Items and actions reference expanded existing element IDs on the same screen. An ID appears only once across both lists. Actions must resolve to buttons. A screen without a readable layout retains its authored canvas and produces a designer warning when the global feature is enabled.

Version 1 supports text, image, dialogue, choices, button, stats, meter, schedule, result, input, saveSlots, loadSlots and playerSettings. Referencing any other kind is a validation error. Reusable components can supply supported expanded leaf IDs. Inherited visibility, conditions, enabled state, labels, bindings and actions remain in force. Empty or hidden items consume no space. Unlisted decoration is intentionally omitted. Unlisted interactive content produces a designer warning.

## Presentation and input

Auto activates when the safe viewport's shorter edge is at most `maxShortEdge`. A device preference can force reading layout on supported screens. Preferences are `readingLayout: "auto" | "on"`, and `textScale: 1 | 1.25 | 1.5 | 2`, persisted outside campaign saves; missing/invalid values default to auto and 1.

The reading surface is unscaled and scrolls vertically, with natural text/button/choice height and no horizontal content scrolling. Item font size defaults to the global font size, never below it, then multiplies by the text preference. Image height defaults to 160 points and is bounded by the authored optional image height. Preserve the existing authored colors, font asset and surface styles. Layout ignores authored x/y/width/height, clipping and group flow for these listed leaves.

The bottom action region wraps buttons, remains outside the scrollable reading content, and cannot cover it. Buttons grow for enlarged or translated labels. Every interactive control has a real minimum width and height of `minTargetSize` (at least 44 points), including internal widget controls. Avoid overlapping invisible hit slop. Schedule, save/load and settings widgets need explicit narrow layouts, not merely a wider wrapper. A fixed action region must remain usable at 200% text and small landscape heights; allow it to scroll if necessary rather than hiding content.

Apply safe-area insets once. Native reads `SDL_GetWindowSize` and `SDL_GetWindowSafeArea`; rendering density remains separate. Web uses `env(safe-area-inset-*)` and `viewport-fit=cover`. Native swipe scrolling cancels taps after a drag; scroll restoration must not leak between story lines/screens. Rotation and preference changes preserve exact simulation state, command history and pending choices.

## Designer workflow and validation

Expose global controls plus a screen reading-order list with add/remove/reorder, item font/image size and bottom-action placement. Offer a starting order from existing supported elements; no JSON editing is required. Show phone preview dimensions and whether the reading layout is active. A missing ID, duplicate, unsupported kind, or non-button bottom action is an error before export. Export requires `readable-layout-v1`; older players must reject it.

## Acceptance evidence

Test 320/390-point portrait, 844×390 landscape, safe insets, all text sizes through 200%, long English and CJK text, keyboard focus, touch scroll/tap cancellation, rotation, settings restart persistence, schedule, records and recovery. Measure actual control rectangles after scaling and native pixel-to-point conversion. Include unchanged legacy layouts, native/Web parity and real iOS simulator gameplay. Physical-device and screen-reader acceptance remain separate until observed.

The 44-point product target follows [Apple's design guidance](https://developer.apple.com/design/tips/). Text growth through 200% and a 320 CSS-pixel reading width are informed by [W3C reflow guidance](https://www.w3.org/WAI/WCAG21/Understanding/reflow). Passing these particular checks is not a claim of complete accessibility conformance.
