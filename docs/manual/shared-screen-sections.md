# Share a screen section

Use **Design → Share screen section across screens…** to author a common HUD or set of navigation controls once. This uses ordinary UI component definitions, instances and named styles; it adds no project or save format.

1. In the layer tree, select a complete group, consecutive top-level elements, or one existing component instance. Selecting a group includes its descendants.
2. Open the sharing dialog and give a new section a recognizable name. Choose target screens. Screens already containing the same component are skipped; existing instances and their overrides are never replaced.
3. For screens with a reading layout, choose **Append supported items to the reading body** (the default), or explicitly choose canvas only. The review lists any omitted reading items. Appended buttons are ordinary body controls; existing bottom actions and their primary action stay unchanged. Decorative panels/groups are omitted from reading flow.
4. Select **Review placement**, inspect every screen, generated instance ID, location and reading result, then **Apply**. Changing the selection or reading choice requires another review. Cancel writes nothing. Apply is one undoable project edit; use the editor's Save action to write it to disk.
5. Select a resulting instance and open **Edit component source**. Changes to the definition propagate to every instance, while existing instance overrides remain independent.

Extraction keeps the original canvas reference size and child coordinates, preserving responsive anchors and group geometry. The generated instance stretches with the canvas. The review gives its actual rectangle; it does not automatically avoid visual overlap. Preview the destination screens and phone reading order after applying. Existing component instances retain their own rectangle and overrides when shared.

Numeric/text bindings, conditions, actions, styles and authored presentation variants are retained. Source reading membership and order remain exactly as authored; only IDs (including bottom/primary actions), reminder IDs and translation catalog paths are updated when elements move into the definition. Intentionally omitted source items stay omitted. The source screen is converted in the same atomic edit, so there is one shared source rather than a detached duplicate.

Supported sections include text, images, buttons, meters, inputs and groups, plus **schedule boards, stats and activity results**. An activity result can retain its growth-summary layout. Select a specialized child in the component source editor and use **Configure selected widget layout, activities, and stats…** to edit the same settings available on an ordinary screen, including schedule categories, forecasts, displayed attributes and growth presentation. Existing nested components can be shared by selecting their outer instance or included with other supported elements; expanded reading IDs are remapped.

Schedules and results still read the current game's planning/result state. Sharing a widget does not create a second schedule or a separate simulation. These game-wide widgets cannot be repeated inside data-list rows. Partial-parent selections, nonconsecutive top-level selections, unsupported component settings and collection-selection-dependent bindings are rejected with an explanation. Select the complete safe group or keep an unsupported widget on its screen.

Exports containing shared schedule, stats or result children require a player with the `component-widgets-v1` capability. An older bundled runtime is rejected before export; update the runtime instead of removing the capability check. Converting a section changes its element paths. Save compatibility must still be explicitly checked against a previous export; sharing a widget is not automatic approval for preserving saves.

A target without a reading layout retains its existing canvas behavior; this tool does not enable reading mode, invent a new phone layout, or change gameplay rules. Canvas-only selection on a reading screen intentionally leaves new controls out of reading flow and is called out in the review.
