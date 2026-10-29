# Raising Studio Qt

Qt 6 Widgets is the current desktop editor. It uses the shared authoring services and the C++ player for creating, validating, playing and exporting games. Start with [My first raising game](../../docs/getting-started/first-game.md) for a no-code walkthrough, or the [authoring manual](../../docs/manual/README.md) for individual tools.

## Build and run

Follow the [root build instructions](../../README.md#build-from-a-clean-checkout) for Windows or [macOS setup](../../README.md#macos-and-ios-from-source). Run these commands from the repository root after installing the documented toolchain:

```sh
npm ci
npm run build:headless
npm run build:native-player
npm run build:qt-studio
npm run studio
```

Windows defaults to Qt 6.10.1 with its MinGW 13.1 kit. macOS uses a native Qt desktop installation and Apple toolchain. Override `QT_ROOT`, `QT_COMPILER`, `NINJA`, or `QT_STUDIO_OUT` when needed. Node 22.14.0 is the reproducible build version; `RAISING_NODE` can select its executable. Web exports require a separately built Web runtime.

The Windows and macOS package commands bundle the required runtime services. Development dependencies are needed to build those packages, not to use a completed standalone package. See [export and packaging](../../docs/manual/qt-game-export.md) and the [support matrix](../../docs/development/platform-support.md) for platform-specific evidence and release limitations. An iOS game export does not provide an iOS editor.

## Architecture

- `bridge.*` connects to the Node service using asynchronous JSON lines. The Qt client sends one authoring request at a time and checks response IDs; transport errors remain visible.
- `apps/qt-studio/service.mjs` coordinates the shared `StudioEditor`, templates, validation, snapshot building, export and analysis services. Game-document edits retain the shared transaction and conflict checks. Saved playtests and priorities are separate authoring metadata.
- `studio.*` manages the dock workspace, selection, property inspector, navigation, undo/redo, diagnostics and execution state.
- `screens.cpp` manages screen creation, duplication, references and open-document tabs.
- `canvas.*` renders the authoring view and handles selection, layout, snapping and movement. Dynamic widgets have authoring previews; the actual player remains the rendering authority.
- `native/player/main.cpp --editor-events` provides opt-in observations from the real renderer. Normal gameplay uses canonical commands; isolated presentation previews are identified separately.

## Authoring and play

The recommended starter opens with a guided checklist covering a complete game and its endings. Existing projects can use [linked authoring](../../docs/manual/linked-authoring.md), the [story workshop](../../docs/manual/story-flow-workshop.md), [rules](../../docs/manual/generic-rules.md), [multi-selection](../../docs/manual/multi-selection.md), [components](../../docs/manual/component-context.md), and [phone reading layouts](../../docs/manual/phone-reading-layout.md).

The Project panel lists screens; open screens appear as tabs. Closing a tab does not delete a screen or discard edits. Duplication preserves external links and rewires self-links to the copy. Deletion checks references, explains required-screen restrictions and is undoable. See [free screens](../../docs/manual/free-screens.md).

Property changes and pointer operations use undo transactions. Layout and window geometry are stored outside game data. Unsaved changes require save or explicit discard when closing or switching projects. External conflicts preserve both the editor draft and the disk version. Draft recovery provides a separate recovery workflow; see [editor basics](../../docs/manual/editor-basics.md).

Normal Play validates and exports a separate snapshot before launching the native player with its own save location. Editing the project does not rewrite a running campaign. Renderer previews, current-screen checks and live canvas playback are separate tools with explicit preview controls; do not treat a forced presentation state as proof that a gameplay route is reachable.

Use **Auto play · Route map** for deterministic analysis, saved runs and replay. [Named route tests](../../docs/manual/goal-route-regression.md) check recorded paths. [Player priorities](../../docs/manual/player-priorities.md) add saved immediate-result preferences and sampled resource reports. Automated completion and coverage do not establish player enjoyment or commercial readiness.

## Verification and limits

`npm run test:creator-smoke` runs the service workflow after a headless build. Desktop and Web coverage require their built runtimes and test executables; use the documented [Creator workflow gate](../../CONTRIBUTING.md#creator-smoke-test-p1). The gate includes actual Qt authoring, validation, play/export, save recovery and readable-layout checks. Package tests additionally run the bundled services and player.

Individual widget tests live in this directory; service tests are under `tests/qt-studio`. They cover different surfaces and are not interchangeable. Setup/cleanup rows in QtTest are not additional feature cases.

The editor remains an authoring alpha. Canvas composition and opt-in reading layouts have different behavior; adding a phone viewport does not automatically make an arbitrary canvas responsive. Apple exports currently require plugin-free games. Windows and macOS have separate CI/package evidence; Linux headless checks do not certify a Linux desktop editor. Signed distribution, physical-device acceptance, assistive-technology testing and independent novice playtests remain release gates. Use the [current support matrix](../../docs/development/platform-support.md) rather than historical implementation notes when making platform claims.
