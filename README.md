# Raising Engine / Raising Studio

A 2D raising-simulation toolkit with a Qt editor, a deterministic C++ runtime, and shared data and validation services for human designers and LLM tools.

**Status: authoring alpha, preparing for an open-source release.** Small complete games and a three-month production pilot have been verified. This is not yet a broadly validated commercial engine. The engine license and redistribution review of reference-game assets are pending; see [release preparation](docs/open-source-readiness.md).

## What works today

- Schedules, character attributes, relationships, inventory, conditions, effects, events and endings.
- Character-scoped actions, timed progression sessions, branching exploration and party turn-based combat within the documented limits.
- Qt docks, screen canvas, properties, reusable/nested UI, anchors, layout, scrolling and data-bound lists.
- Image-frame direction, character expression changes and save/resume.
- Deterministic route replay, target search, baseline comparison, partial approval and restoration.
- CLI/GitHub Actions regression reports, including direct run/report browsing and graph review in the editor.
- Shared project editing, validation and version-conflict checks for GUI and MCP workflows.

See the [documentation map](docs/README.md) and [current scope](docs/universal-raising-engine-plan.md). Feature-specific limitations are documented alongside each system.

## Supported development path

The current editor and latest player features are verified on **Windows**. Headless regression CI runs on Linux. Earlier web and Android results cover a subset of features; they do not establish support for all newer UI, combat and direction systems. iOS and console release support is not verified.

Prerequisites for the Windows path:

- Node.js 22.12 or later, npm, Git and CMake.
- A supported C++ toolchain for the runtime/player. See [native runtime](native/README.md) and [player build](native/player/README.md).
- Qt 6 desktop development kit, its compatible compiler and Ninja for the editor. The tested kit is Qt 6.10.1 / MinGW 13.1.
- Optional GitHub CLI (`gh auth login`) for browsing Actions reports in the editor.

The Qt preview still uses the repository's Node service and dependencies. It is not a self-contained public installer. Some native dependencies are downloaded during their first build.

## Build and run

Launching without `--project` opens the project manager: create a project from a public template, open an existing folder, or search recent projects. Projects can live outside the engine checkout. See [project manager and release boundary](docs/project-manager-and-public-release.md).

Run from the repository root. Configure paths for your own Qt installation:

```powershell
npm ci
npm run build:headless
npm run build:native-player

$env:QT_ROOT = 'C:/Qt/6.10.1/mingw_64'
$env:QT_COMPILER = 'C:/Qt/Tools/mingw1310_64/bin'
$env:NINJA = 'C:/Qt/Tools/Ninja/ninja.exe'
$env:QT_STUDIO_OUT = 'dist/qt-studio'
npm run build:qt-studio
npm.cmd run studio:qt -- --project ./examples/academy-demo
```

Both the Qt builder and launcher honor `QT_STUDIO_OUT`. Use a separate output directory when another editor is running. Native player builds also support `NATIVE_PLAYER_BUILD` and `NATIVE_PLAYER_OUT`; set `RAISING_NATIVE_PLAYER_RUNTIME` when the editor should use a non-default player output.

The `examples/starlight-three-months` pilot exercises more systems. Public sample candidates are separated from the ignored `projects/` user workspace and contain no known extracted reference-game assets. Engine licensing and generated-asset/dependency notice review remain pending. See [sample separation](docs/public-sample-separation.md). Development launchers may point to historical local build outputs; the npm commands above are the primary entry point.

For a short visual presentation sample, see [Last Grandson — winter short](examples/last-grandson-polished/README.md). It uses original generated artwork and the cutout-animation 1.13.0 plugin for portrait motion and expressions. The plugin must be built/installed separately; see its [review and verification record](docs/short-game-polish.md).

## Verify

```powershell
npm run build:headless
npm run test:regression
npm run check:public-content
npm run test:public-content
npm.cmd run check:regression -- examples/starlight-three-months --runtime-policy verify-replay --report-dir .cache/goal-regression
npm run audit:repository
```

The route suite checks approval/failure semantics and save replay. It is not the entire engine test suite. Qt authoring/player tests may require generated fixtures and explicit environment variables; use the relevant feature document rather than assuming every desktop test runs from a fresh clone without preparation.

In the editor, open **디자인 → GitHub Actions · CI 경로 검토…** to inspect workflow runs and reports without manually downloading archives. See [GitHub report review](docs/github-actions-editor-review.md).

## Repository map

| Path | Responsibility |
|---|---|
| `native/studio/` | Current Qt editor |
| `apps/qt-studio/` | Editor service and route analysis/review adapters |
| `native/src/`, `packages/` | Runtime, shared contracts, authoring, validation and MCP services |
| `native/player/`, `apps/web/` | Native and web player surfaces |
| Other `apps/` | Tools and retained Electron compatibility paths |
| `plugins/` | Animation integration and extension examples |
| `examples/` | Public sample candidates and pilot |
| `tests/fixtures/projects/` | Isolated regression inputs |
| `projects/` | Ignored user games and internal/reference experiments, preserved locally |
| `tests/`, `scripts/` | Verification, build and development tooling |
| `docs/` | Current guides and historical milestone records |

[Contributing](CONTRIBUTING.md) · [Documentation](docs/README.md) · [Open-source cleanup plan](docs/open-source-readiness.md)
