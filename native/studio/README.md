# Raising Studio Qt — first authoring slice

Qt 6 Widgets desktop editor. This is a workspace preview, not a standalone installer or a full replacement for every Electron workspace.

## Build and run (verified on Windows)

The existing engine build (`npm run build`) supplies the shared authoring service. The native player build (`npm run build:native-player`) supplies the SDL3/RmlUi player. Then:

```powershell
npm run build:qt-studio
npm run studio:qt -- --project D:/Projects/TheSword/RaisingEngine/projects/qt-shop-playtest-2026-10-12
npm run test:qt-service
npm run test:qt-studio
```

Current build defaults: Qt 6.10.1 `C:/Qt/6.10.1/mingw_64`, GCC 13.1 `C:/Qt/Tools/mingw1310_64/bin`, Qt Ninja. Override `QT_ROOT`, `QT_COMPILER`, `NINJA`, `QT_STUDIO_OUT`. Node 22 must be on PATH, or set `RAISING_NODE`. The build uses shared Qt libraries and runs `windeployqt`. Qt license/module compliance and backend packaging still need a distribution pass before releasing a standalone installer.

## Boundaries

- `bridge.*`: asynchronous JSON-lines QProcess connection. One authoring request at a time, response IDs checked; transport errors are visible.
- `apps/qt-studio/service.mjs`: adapter around the SAME `StudioEditor`, `createTemplateProject`, validator, snapshot builder and exporter used by the existing engine. It never writes project JSON directly. File-version checks and atomic commit behavior remain in the shared service.
- `studio.*`: dock layout, selection, inspector, document navigation, undo/redo, project/asset browser, diagnostics and execution state.
- `screens.cpp`: screen creation, duplication, label changes, reference-aware deletion and open-document tabs. All data edits use the shared authoring transaction service.
- `canvas.*`: QGraphicsView authoring canvas; logical coordinates, asset rendering, selection, dragging, four corner resize handles, keyboard movement, optional 8-unit snapping, zoom and pan. Dynamic gameplay widgets are layout previews; actual rendering is checked in the player.
- `native/player/main.cpp --editor-events`: opt-in JSON-lines observation stream from the real C++ renderer. Emits the completed rendered view and associated action after rebuild. No state injection or alternate simulation logic.

## Authoring behavior

The Project panel lists all screens. Double-click to open a screen; only opened screens appear as tabs. Use × or Ctrl+W to close a tab without deleting its data or discarding unsaved edits. Closing the last tab shows an empty workspace; save/undo remain available. Opening a project starts with Home (or its first screen), not every screen.

Use `＋ 새 화면…` in the toolbar, the Screen menu or the Project panel. The Project panel also provides duplicate, rename and delete, including a context menu. These operations target the selected project screen. Names accept 1–100 non-whitespace characters; internal IDs are generated uniquely and stay stable when a name changes. Duplicate copies all elements and rewires self-links to the copy while preserving external links. The existing schema allows at most 100 screens.

Delete checks references across loaded documents and shows file/JSON-pointer locations when a button or location still points to the screen. Self-links inside the deleted screen do not block deletion. `title`, `schedule`, `story`, and `ending` remain required runtime screens: rename/content editing is allowed, deletion is blocked with an explanation. Deletion is confirmed and undoable. Closing a tab never deletes a screen; undoing deletion restores the data without unexpectedly opening its tab.

To link a new screen, select a button, choose `화면 이동`, and choose the screen under `대상`. `대상 화면 열기` opens it even if its tab was closed. Navigation preserves the current gameplay location; use a location action when a location change is intended.

Open a project folder or create a new project from the 12-month adventure template using the File menu. Choose the home screen. `＋ 버튼` creates a unique button, initially set to navigate to the current screen (explicitly shown in the inspector). Change its label/text and geometry; choose `상점 열기 · 장소 이동`, then a shop. This uses the existing canonical `visit` action and the shop's `locationId`; it respects location connectivity and runtime availability. It is NOT an overlay-shop implementation.

Enter or focus loss commits a property change. One pointer drag/resize is one undo transaction. Ctrl+Z / Ctrl+Y undo/redo; keyboard Delete and Ctrl+D apply only with canvas focus. Locked elements cannot move, resize, align or delete. Hide means hidden in the game; it is not a separate editor-only visibility toggle.

F5 saves/validates the authored project and exports an immutable snapshot to a unique `.cache/qt-preview/run-*/game` folder before launching the native player. A separate save slot is used for every run. Source/asset changes during export are checked. Semantic errors block play; the shared service can explicitly save structurally valid drafts with semantic errors. The player starts from the normal title screen, not a forced state. Play state never overwrites design data. Edits made while playing apply to the next run. Execution logs and the execution-state dock receive actual C++ state changes.

Dock layout/geometry are stored in user QSettings, not game data. Unsaved changes require explicit discard on close/open; save via Ctrl+S first. External write conflicts preserve the Qt draft and external disk version; no automatic overwrite is offered.

## Current limits

- Fixed logical canvas coordinates; “오른쪽 아래 정렬” computes a one-time alignment. Persistent anchors/responsive layouts are not implemented.
- Single selection and existing flat element order; hierarchy reparenting, multi-select, asset drag creation, reusable components, multi-action sequences and sophisticated condition editors remain future work.
- Screen and property editing are implemented; narrative graphs, full content tables and all former analysis workspaces have not been ported to Qt.
- Current screen test/forced test state and embedded player viewport are not implemented. The separate native player is authoritative.
- Node sidecar and repository files are currently required. No macOS/Linux Qt build or mobile/console editor build is claimed.

## Verification

`tests.cpp` drives actual Qt widgets and pointer events, then launches the real C++ player with its existing pointer-input automation. Checks include creating/editing a button, action binding, undo/redo, keyboard movement, pointer dragging/resizing, save/reopen, entering the authored shop, buying, saving, buying another item, loading and returning home. A second UI case checks dock detach/restore. Setup/cleanup rows in QtTest are not separate feature tests.

`tests/qt-studio/service.test.mjs` checks shared validation, stale revisions, history, safe asset paths and two independent authoring clients competing to save. Existing native player renderer/input and invalid-bundle tests also cover the opt-in telemetry addition's regression surface.

The added `screenLifecycleAndPlay` case covers cancelled creation, create/undo/redo, copy self-links, rename, deletion cancellation and undo/redo, reference and required-screen protection, all-tabs-closed save, reopening via the project list and action target, stable IDs after rename, save/reopen, and a real native home → custom screen → home journey. There are now 3 Qt feature cases plus setup/cleanup.
