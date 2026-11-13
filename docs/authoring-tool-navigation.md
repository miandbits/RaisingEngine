# Finding commands and authoring tools

Open **Design → Find commands and tools…** or press **Cmd+K** on macOS or **Ctrl+K** on Windows. Search using a tool name or words describing the task. Multiple words narrow the results; for example, the Korean query `인물 일정` finds the roster and activity assignment tool. The search includes translated command names, menu paths, available tool descriptions and native keyboard shortcuts. It covers File commands such as Save and Export, Play, validation, panel navigation, screen and canvas tools, and authoring workspaces. Shared menu and toolbar actions appear once.

Use **↑/↓** to select a result, then press **Enter** or **Open**. **Esc** cancels and returns focus to the field or panel you were using. Unavailable commands remain disabled, including commands inside a disabled menu or toolbar. Search uses the original command, so Save, export and unsaved-change guards still apply. Active IME composition must finish before the finder can open.

Toolbar choices such as preview language, screen size and the element palette are searchable. The current choice is marked; choosing another item follows the same selection handler as its toolbar control.

The Design menu groups tools into characters/growth, story/rules, screens/presentation, exploration/combat/trading, validation/regression and starters/samples. Story authoring, the integrated workspace and selected-element connections remain near the top. Existing toolbar buttons and action identifiers are preserved.

This changes editor navigation only. Project data and save formats are unchanged.

## Small windows and toolbar access

The **Authoring tools** menu in the menu bar and the first **Authoring tools**
button in the main toolbar expose every main-toolbar command, even when it does
not fit on the screen. The menu also includes the preview language selector and
**＋ Element** palette. It uses the original commands, so disabled tools remain
disabled and shortcuts and checked states are preserved. **Find commands and tools…**
is available at the top of this menu for commands across the editor.

Qt's automatic toolbar overflow remains available; the explicit menu provides a
labelled alternative to its small chevron. Changing the window size does not edit
project data or save files.

Verified with the real Qt widgets at 1000×680, 1280×720 and 1440×920 in Korean,
English and Japanese. Tests cover repeated opening, the far-right renderer
command, language selection, disabled commands and the existing tool finder.
In a Windows editor narrowed to 1000 pixels, the menu also opened the hidden
validation command and displayed its result. This is editor navigation coverage;
it does not replace gameplay or export tests.

## Verification

- Qt widget tests: keyword matching, original action dispatch, duplicate suppression, live availability, disabled menu/widget controls, cancellation/focus restoration, menu paths, native shortcuts and toolbar choices.
- Isolated-project test: a focused inspector edit → command search → Save → reopen preserves the edited Unicode label.
- Actual Studio command inventory at 1000×680 includes Save, Export, Play, Validate, panel restore and canvas/palette actions.
- Korean, English and Japanese translation coverage.
- Windows editor: Ctrl+K → task query → Enter → roster tool → cancel, using a separately authored three-character project.

Search matches command names and descriptions; it does not interpret natural-language editing instructions or promise a complete synonym dictionary. Automated coverage does not establish independent designer discoverability.

## Inspecting dynamic layouts before playing

The canvas shows a multi-character schedule's initial roster and a choice widget's
first available authored choice node. Choice cards include the authored consequence
hint when that widget enables choice effects. These are explicitly marked layout
examples: they do not evaluate availability, execute effects, or predict a play session.
Empty narrative content shows a hint instead of fabricated choices. Content too tall
for the widget is clipped, with an overflow hint where space permits.

If dialogue, choices and advance buttons occupy the same area, use the canvas
**Condition preview…** (`조건 보기…`) and choose **Choices** (`선택지`). Apply
the temporary preview to respect their phase-based visibility rules. **All elements**
(`전체 요소`) returns to the full editable layout. This does not change the project
or a running game's state. Reapply the preview after editing the project.

The sample is taken from the first nonempty choice node in the loaded event/ending
documents, not a selected saved game's current event. Play and validation are still
needed to check conditions, character targeting, effects, and save/load behavior.
