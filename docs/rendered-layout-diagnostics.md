# Check the layout with the game renderer

Open a screen, then press **Cmd+K** on macOS or **Ctrl+K** on Windows and search for **layout** or **diagnostics**. Open **Actual renderer inspection**. The same action is available in the editor toolbar.

Choose a **Layout check preset** (desktop, phone, or phone at 200% text), or set the viewport, language, reading layout and text scale individually. Presets only change inspection settings. Press **Render current screen** to render the current unsaved draft in a temporary, isolated game snapshot. The viewport values are window points, not an assertion that a named physical device was tested.

The **Layout checks** tab lists measured findings. Click a row or activate it with Enter to select its authored element. A runtime widget links to the element that owns it. Screen-wide controls without an authored owner remain screen-level findings. Selection never applies an automatic fix. Results from an older project revision cannot select elements; render again after editing. **Raw measurements** retains the actual bounds and computed styles for investigation.

The inspector reports:

- Text whose real shaped layout exceeds its hidden area, or whose element box is clipped by a fixed ancestor.
- Controls and scroll regions clipped by a fixed ancestor, and authored elements or controls outside the viewport without a scrolling ancestor.
- Controls smaller than the engine's 44 × 44 point touch guidance, using the actual renderer-to-window conversion. The size check uses the full target, so scrolling a button partly out of view does not turn it into a smaller target.
- Low contrast where actual text and an opaque, plain background can be measured safely. The threshold is 4.5:1 for ordinary text, or 3:1 for text at least 24 window points (18.67 points when bold). Disabled text is measured as coverage but does not produce a contrast finding.

Intentional scrolling is counted as coverage. Scrolled-off descendants are not automatically reported as clipped or outside the viewport. A scroll region that is itself cut off by a fixed parent is reported separately.

The engine's generated planner and save/load controls reserve at least 44 × 44 window points when the authored canvas is fitted into a smaller window. Their layout is recalculated after resizing, including space used by the reading-options toolbar. Save confirmation buttons grow with wrapped labels, and the surrounding flow scrolls when needed so its actions remain reachable.

Authored buttons keep the rectangles the designer chose. They are not automatically enlarged into neighboring elements; undersized or clipped authored controls still appear in Layout checks. Inspect each selected screen and state after changing its viewport or language, and verify scrolling, focus and touch interaction separately.

The coverage summary records the actual screen, locale, viewport, requested text scale, and whether readable layout was active. **200% text only reflows screens configured for readable layout.** An inactive readable layout is shown explicitly. Use **Configure authoring state** to inspect an eligible saved state or virtual inspection state, then rerender. The default is the game's initial state, not every phase of the game.

Cancel is available while the snapshot is being prepared and while the native player is rendering. Rendering times out after 60 seconds. Inspection does not save the project, execute game actions, or write to the user's game save.

## What the report does not prove

A clean report is not accessibility or release acceptance. Check other screens, phases, interaction states, languages and text sizes separately. Test real touch use, focus order, screen readers, occlusion and usability with people. Physical-device acceptance remains separate from simulator or desktop rendering.

Text measurements use RmlUI's shaped text layout extents and element boxes, not per-glyph pixel analysis. Contrast uses computed solid colors. Images, decorators, layered painted surfaces, transparent text/backgrounds and unknown backgrounds are reported as **unmeasured**, rather than assumed to pass. The collector examines up to 4,096 DOM nodes and retains up to 512 findings; exceeding either bound marks coverage incomplete.

## Automation report

Every native player UI checkpoint includes `layoutDiagnostics` with:

- `version`, `measurement: "rmlui-rendered-layout"`, and `scope: "current-screen-and-state"`.
- `configuration`: actual screen/locale, logical viewport, window size, requested text scale, readable-layout activation, and state source.
- `issues`: stable codes, authored `elementId`, rendered `nodeId`, measured bounds and evidence. Codes are `CLIPPED_TEXT`, `CLIPPED_CONTROL`, `CLIPPED_SCROLL_REGION`, `OUTSIDE_VIEWPORT`, `SMALL_TOUCH_TARGET`, and `LOW_CONTRAST`.
- `contrastSamples`: measured ratios or explicit unmeasured reasons.
- `coverage` and `limits`, including truncation and contrast coverage.

The report is reusable for exported games and simulator automation. It is distinct from schema validation and does not turn a single selected screen into a whole-game pass/fail gate.
