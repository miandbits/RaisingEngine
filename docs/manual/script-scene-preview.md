# Script scene preview

In the Qt script workshop, select a scene node and choose **선택 장면 미리보기**.
The dialog displays **장면 미리보기 · 게임 진행 검증 아님** and renders a real
native-player PNG. It accepts the current workshop scene draft without saving
the script or project. Choose a UI screen (the story screen is selected when
present) and use the renderer refresh button as needed.

This is an initial-state composition preview. Omitted scene fields retain the
initial presentation, not values from earlier script nodes. Previous nodes,
effects, schedules, rules and dialogue progression are not executed. Conditional
UI uses the initial state; this preview does not prove reachability or gameplay
outcomes. The preview cannot play, save or change the project. Audio is not a
previewed outcome. Close it, adjust the scene and open it again to inspect a new
draft.

The shared preview service validates a copied loaded project, preserving source
location maps. It exports the unchanged content snapshot into a temporary cache
and writes the scene draft to a separate JSON file. The native player accepts
that file only in preview mode and applies it to rendering presentation, leaving
its game state untouched. Older runtimes without `scene-preview-v1` are rejected.
The unsaved scene draft is separately checked for portrait scale and offsets.
When any such field is present (including an explicit default), the renderer
must advertise `scene-portrait-transform-v1`. Both the selected runtime and its
temporary copy are checked before rendering. A rejection leaves the scene draft,
project files, revision and history unchanged; a compatible runtime can render
the same draft without saving it first.

Verification on 2026-10-13:

- Qt draft isolation and actual Qt→service→hidden-native PNG tests: 2 passed.
- Service isolation, invalid-scene and compatibility checks: 3 passed, including
  project-tree SHA256, revision, dirty state and undo/redo preservation.
- Native preview tests independently confirm unchanged initial game state,
  transform geometry, explicit background clearing and blocked play/save input.

Qt/native integration evidence: `.cache/qt-render-preview/run-4e0JO6/preview.png`
and `preview-report.json`. These use a disposable academy fixture, not production
gameplay. Final launcher `Start-PM2-Editor.cmd` connects
`dist/qt-studio-export-retry` to `dist/native-player-scene-preview` and opens the
production project without changing it.

Follow-up regression and actual editor evidence:
[Claude fixes verification](../evidence/claude-fixes-2026-10-13/README.md).
