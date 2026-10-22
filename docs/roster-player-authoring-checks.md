# Player gaps found during a multi-character authoring trial

Two problems were reproduced while composing a short, separate game with the existing roster and story systems:

- Starting a game at a noninteractive scene node left an empty story panel until the first click. Starting now uses the same narrative settling path as subsequent actions. It stops at a dialogue or choice; it does not select an answer.
- The schedule element's existing **Show activity description** setting was ignored for rosters. Windows and web now show the authored description for each selected draft activity. It remains opt-in, escapes text, and bounds long descriptions. Native columns align at the top even when descriptions differ in length.

Descriptions are designer-authored text, not a live calculated cost forecast. They must be updated when activity rules change.

## Checks

- `tests/native-player/startup-narrative.test.mjs`: actual player startup, stop at a choice, next dialogue, save/load and fresh-process load.
- `tests/native-player/roster-description.test.mjs`: actual native controls, independent selection, apply, opt-out and save/load.
- `tests/visual/roster-description.test.mjs`: actual browser rendering, selection, literal markup, long text, apply and opt-out.
- Web build succeeds.
- Existing native timeline test: the playback/resume case passes; the invalid-cursor/branch case fails at a missing `timeline-skip` control on both the previous runtime and this build. That pre-existing test/fixture failure is not represented as a pass and needs a separate investigation.

The separate private game also compares core state and displayed personal attributes. A correctly computed simulation is insufficient if a UI card is connected to the wrong actor. No private assets or game data are included here.
