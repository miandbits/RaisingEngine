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

Compact schedule items may also opt into `"planningGuidance":"first-use"`. This requires `planning-guidance-v1` in addition to the compact and readable-layout capabilities. Validation rejects it on other items or standard schedules. Existing projects omit the field and retain their previous behavior.

The optional three-step guide explains the actual period count and names, activity inspection, **Apply** versus **Cancel**, and the available authored progression control. Copy must distinguish confirming a draft from executing an activity; an authored `advance` can do either depending on the current schedule state, while `commitPlan` confirms and executes. Unknown progression controls receive neutral copy. Authored `commitPlan` buttons use the narrow `commit-plan-action-v1` capability; older native players with `schedule-board-v1` also satisfy it. Supporting this action alone does not advertise the whole board-widget capability. Opening, navigating or dismissing guidance never issues simulation commands, edits a draft or records campaign progress.

The first eligible editable compact planner offers the guide once. **Skip**, Escape and **Done** acknowledge it; **How planning works** reopens it. Acknowledgment uses the stable game ID and versioned topic `compact-planning-v1` outside campaign saves. Web also scopes it to the deployment directory. Content fingerprints are deliberately excluded so a presentation update does not repeat onboarding. Missing identity or editor preview must not write a persistent preference. Broken or unavailable storage uses a session fallback. The guide shares modal input isolation, automatic-playback pause, focus restoration and resize behavior with the activity picker; guide, picker and utility menu never overlap.

Save/load reading layouts put date, place and phase before secondary timestamp and record number, show the selected checkpoint beside the confirmation controls, and identify current progress separately. Empty slots stay compact. Existing six/manual and three/automatic configurations, compatibility checks and explicit overwrite/load confirmation are retained. Save records keep their existing schema. Web may localize explicitly zoned timestamps; historical native timestamps without a zone must not be interpreted as UTC. No narrative excerpt or unvisited content is synthesized.

## Reusing a confirmed plan

An ordinary compact schedule can opt into `"planReuse":"last-committed"` on its reading item. It requires the narrow `plan-reuse-v1` capability in addition to compact planning and reading layout. Other item kinds, noncompact schedules and roster projects are rejected. Omission preserves existing behavior and adds no history projection. In the editor, select **Offer reuse of last confirmed plan** for a compact schedule.

The source is the newest accepted `setSchedule` command in the current session. It may have been confirmed in the current phase; it is not necessarily the previous completed phase. Invalid length or unknown activities make that source unavailable. The player never silently substitutes an older command, another save slot, actor assignments or an unconfirmed draft. Player views expose only a small defensive projection, never the entire command history. A successful load selects the loaded branch's history even when two branches contain the same number of commands; a failed load keeps the live session.

**Use last confirmed plan** opens a review of every localized period and activity. **Apply to draft** replaces the remaining assignments atomically. Completed assignments, including the current activity during an intermission, retain their canonical values. Missing locked-prefix data or no remaining editable periods disables reuse. Cancel and Escape preserve the exact draft and selection state. Neither opening nor applying the review spends time or resources, dispatches simulation commands, or changes a save.

The review explains that normal requirements and costs apply when activities execute. `setSchedule` does not reserve future resources or validate future affordability, so reuse must not pretend to forecast a whole phase. Replacing an edited remaining draft is disclosed. Named routines, multi-actor schedules and cost forecasts are separate features.

An open review is tied to its session generation, current revision, source command and assignments, period count/index, owning screen/element, canonical locked prefix, draft and selection state. Changes invalidate it before Apply. Hidden or disabled owners cannot be applied. The review isolates background shortcuts, pauses automatic playback and restores focus/reading position. A resize cannot revive a stale review. Reuse state is presentation-only; no new canonical command, save field or persistent preference is introduced.

Acceptance covers shared/native structural parity; actual exported Web and native UI; branch restore, rejected load and execution-failure cases; intermission prefixes; atomic multi-slot application; 1, 3, 5 and 12 periods; English, Korean and Japanese labels; phone widths and 200% text. `scripts/smoke-creator.mjs` runs the projection, Web, native and existing Qt authoring checks in desktop CI. Human usability and physical-device acceptance remain separate.

## Presentation and input

Auto activates when the safe viewport's shorter edge is at most `maxShortEdge`. A device preference can force reading layout on supported screens. Preferences are `readingLayout: "auto" | "on"`, and `textScale: 1 | 1.25 | 1.5 | 2`, persisted outside campaign saves; missing/invalid values default to auto and 1.

The reading surface is unscaled and scrolls vertically, with natural text/button/choice height and no horizontal content scrolling. Item font size defaults to the global font size, never below it, then multiplies by the text preference. Image height defaults to 160 points and is bounded by the authored optional image height. Preserve the existing authored colors, font asset and surface styles. Layout ignores authored x/y/width/height, clipping and group flow for these listed leaves.

The bottom action region wraps buttons, remains outside the scrollable reading content, and cannot cover it. Buttons grow for enlarged or translated labels. Every interactive control has a real minimum width and height of `minTargetSize` (at least 44 points), including internal widget controls. Avoid overlapping invisible hit slop. Schedule, save/load and settings widgets need explicit narrow layouts, not merely a wider wrapper. A fixed action region must remain usable at 200% text and small landscape heights; allow it to scroll if necessary rather than hiding content.

Apply safe-area insets once. Native reads `SDL_GetWindowSize` and `SDL_GetWindowSafeArea`; rendering density remains separate. Web uses `env(safe-area-inset-*)` and `viewport-fit=cover`. Native swipe scrolling cancels taps after a drag; scroll restoration must not leak between story lines/screens. Rotation and preference changes preserve exact simulation state, command history and pending choices.

Grouped footers keep the menu closed until requested and avoid an independently scrolling persistent utility list. Menus and pickers pause automatic story/timeline progression, isolate input and restore focus on dismissal. Explicit authored navigation retains its usual navigation commands; grouping and return-position bookkeeping add none.

Reading positions are bounded, session-only presentation data keyed by screen/view and narrative/phase identity. Same-size returns restore the nonzero scroll position and available focus target. Reflow restores a logical text anchor: Web uses a rendered character offset; native uses a paragraph anchor and fractional position within it. A long native paragraph therefore retains an approximate within-paragraph position after reflow. New narrative content resets the position. This is neither chronological reading history nor persisted campaign state, and requires no save format change.

## Designer workflow and validation

Expose global controls plus a screen reading-order list with add/remove/reorder, item font/image size, compact schedule selection and bottom-action roles. Offer a starting order from existing supported elements; no JSON editing is required. Show phone preview dimensions and whether the reading layout is active. A missing ID, duplicate, unsupported kind, non-button bottom action, stale primary role or compact roster is an error before export. Export requires `readable-layout-v1`; grouping additionally requires `readable-actions-v1`, compact planning requires `compact-planning-v1`, and optional guidance requires `planning-guidance-v1`. Older players must reject unsupported capabilities.

## Acceptance evidence

Test 320/390-point portrait, 844×390 landscape, safe insets, all text sizes through 200%, long English and CJK text, keyboard focus, touch scroll/tap cancellation, rotation, settings restart persistence, schedule, records and recovery. Measure actual control rectangles after scaling and native pixel-to-point conversion. Include unchanged legacy layouts, native/Web parity and real iOS simulator gameplay. Physical-device and screen-reader acceptance remain separate until observed.

Focused regression entry points are `tests/visual/readable-layout.test.mjs`, `tests/journey/readable-ux-web.test.mjs`, `tests/journey/readable-layout-web.test.mjs`, `tests/native-player/reading-workflow.test.mjs`, the planning-guidance journey tests, and the Qt `qt-readable-layout-test` executable. Cover local picker cancellation, exact canonical route/save recovery, dynamic period counts, unchanged-content return anchors and new-node reset. Creator CI runs these alongside the existing legacy layout checks.

The 44-point product target follows [Apple's design guidance](https://developer.apple.com/design/tips/). Text growth through 200% and a 320 CSS-pixel reading width are informed by [W3C reflow guidance](https://www.w3.org/WAI/WCAG21/Understanding/reflow). Passing these particular checks is not a claim of complete accessibility conformance.
