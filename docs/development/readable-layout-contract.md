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
      "actions": ["advance", "save", "load"],
      "actionLayout": "grouped",
      "primaryAction": "advance"
    }
  }]
}
```

Every global field is optional with the defaults above. `background` is an opaque six-digit hex color; authors select a surface that contrasts with their existing text colors. The global object enables the feature. A screen opts in with `readableLayout`; its `items` is required and may be empty when actions supply its interface. Items and actions reference expanded existing element IDs on the same screen. An ID appears only once across both lists. Actions must resolve to buttons. A screen without a readable layout retains its authored canvas and produces a designer warning when the global feature is enabled.

Version 1 supports text, image, dialogue, choices, button, stats, meter, schedule, result, input, saveSlots, loadSlots and playerSettings. Referencing any other kind is a validation error. Reusable components can supply supported expanded leaf IDs. Inherited visibility, conditions, enabled state, labels, bindings and actions remain in force. Empty or hidden items consume no space. Unlisted decoration is intentionally omitted. Unlisted interactive content produces a designer warning.

`actionLayout: "grouped"` opts into one primary button and a labeled utility menu. `primaryAction` is optional and must name a button already in `actions`; it is invalid without grouping. A disabled or hidden primary never becomes executable through the menu. Without a primary, all available actions use the menu. Reading preferences remain reachable there. Omitting grouping preserves the legacy footer.

A schedule reading item can specify `{"elementId":"planner","schedulePresentation":"compact"}`. This replaces its expanded catalogue with a summary of the project's actual `periodsPerMonth` assignments. A slot opens a picker with existing authored categories, description and cost/effect rules. Selection is pending until **Apply** updates the existing schedule draft; **Cancel** discards it. Neither action commits time. Requirements are still evaluated by normal execution; the summary is not a forecast and must not disable future assignments based only on today's resources. Roster schedules are rejected for compact presentation until participant/target editing is supported. Canvas `scheduleLayout` remains unchanged.

## Presentation and input

Auto activates when the safe viewport's shorter edge is at most `maxShortEdge`. A device preference can force reading layout on supported screens. Preferences are `readingLayout: "auto" | "on"`, and `textScale: 1 | 1.25 | 1.5 | 2`, persisted outside campaign saves; missing/invalid values default to auto and 1.

The reading surface is unscaled and scrolls vertically, with natural text/button/choice height and no horizontal content scrolling. Item font size defaults to the global font size, never below it, then multiplies by the text preference. Image height defaults to 160 points and is bounded by the authored optional image height. Preserve the existing authored colors, font asset and surface styles. Layout ignores authored x/y/width/height, clipping and group flow for these listed leaves.

The bottom action region wraps buttons, remains outside the scrollable reading content, and cannot cover it. Buttons grow for enlarged or translated labels. Every interactive control has a real minimum width and height of `minTargetSize` (at least 44 points), including internal widget controls. Avoid overlapping invisible hit slop. Schedule, save/load and settings widgets need explicit narrow layouts, not merely a wider wrapper. A fixed action region must remain usable at 200% text and small landscape heights; allow it to scroll if necessary rather than hiding content.

Apply safe-area insets once. Native reads `SDL_GetWindowSize` and `SDL_GetWindowSafeArea`; rendering density remains separate. Web uses `env(safe-area-inset-*)` and `viewport-fit=cover`. Native swipe scrolling cancels taps after a drag; scroll restoration must not leak between story lines/screens. Rotation and preference changes preserve exact simulation state, command history and pending choices.

Grouped footers keep the menu closed until requested and avoid an independently scrolling persistent utility list. Menus and pickers pause automatic story/timeline progression, isolate input and restore focus on dismissal. Explicit authored navigation retains its usual navigation commands; grouping and return-position bookkeeping add none.

Reading positions are bounded, session-only presentation data keyed by screen/view and narrative/phase identity. Same-size returns restore the nonzero scroll position and available focus target. Reflow restores a logical text anchor: Web uses a rendered character offset; native uses a paragraph anchor and fractional position within it. A long native paragraph therefore retains an approximate within-paragraph position after reflow. New narrative content resets the position. This is neither chronological reading history nor persisted campaign state, and requires no save format change.

## Designer workflow and validation

Expose global controls plus a screen reading-order list with add/remove/reorder, item font/image size, compact schedule selection and bottom-action roles. Offer a starting order from existing supported elements; no JSON editing is required. Show phone preview dimensions and whether the reading layout is active. A missing ID, duplicate, unsupported kind, non-button bottom action, stale primary role or compact roster is an error before export. Export requires `readable-layout-v1`; grouping additionally requires `readable-actions-v1`, and compact planning requires `compact-planning-v1`. Older players must reject unsupported capabilities.

## Acceptance evidence

Test 320/390-point portrait, 844×390 landscape, safe insets, all text sizes through 200%, long English and CJK text, keyboard focus, touch scroll/tap cancellation, rotation, settings restart persistence, schedule, records and recovery. Measure actual control rectangles after scaling and native pixel-to-point conversion. Include unchanged legacy layouts, native/Web parity and real iOS simulator gameplay. Physical-device and screen-reader acceptance remain separate until observed.

Focused regression entry points are `tests/visual/readable-layout.test.mjs`, `tests/journey/readable-ux-web.test.mjs`, `tests/journey/readable-layout-web.test.mjs`, `tests/native-player/reading-workflow.test.mjs`, and the Qt `qt-readable-layout-test` executable. Cover local picker cancellation, exact canonical route/save recovery, dynamic period counts, unchanged-content return anchors and new-node reset. Creator CI runs these alongside the existing legacy layout checks.

The 44-point product target follows [Apple's design guidance](https://developer.apple.com/design/tips/). Text growth through 200% and a 320 CSS-pixel reading width are informed by [W3C reflow guidance](https://www.w3.org/WAI/WCAG21/Understanding/reflow). Passing these particular checks is not a claim of complete accessibility conformance.
