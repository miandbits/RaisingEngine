# Raising Studio

A 2D raising-simulation engine with a **Qt editor**, a deterministic **C++ runtime**, and **Windows / macOS / Web players**, plus an **iOS Xcode export**. Designers and LLM tools edit the same validated project data.

**Authoring alpha.** Original code is MIT licensed. See the [support matrix](docs/development/platform-support.md) for verified features and platform limitations.

The first-game workflow now includes automatic draft recovery, ending explanations, early target checks, versioned game/project ZIPs, and a one-click local Web preview. Contributors can run `npm run test:creator-smoke` and `npm run benchmark:creator`; see [the contributor guide](CONTRIBUTING.md#creator-smoke-test-p1) and [the designer walkthrough](docs/getting-started/first-game.md).

## Install on Windows

**[0.1.0-preview.3 — unsigned preview](https://github.com/miandbits/RaisingEngine/releases/tag/v0.1.0-preview.3)** · Windows x64 · 한국어 / English / 日本語

| Download | Start using the editor |
|---|---|
| **[Windows installer (.exe)](https://github.com/miandbits/RaisingEngine/releases/download/v0.1.0-preview.3/RaisingEngine-0.1.0-preview.3-windows-x64.exe)** | Run the installer, then open Raising Studio from the Start menu. |
| **[Portable ZIP](https://github.com/miandbits/RaisingEngine/releases/download/v0.1.0-preview.3/RaisingEngine-0.1.0-preview.3-windows-x64-portable.zip)** | Extract the entire ZIP, then run `RaisingStudio/RaisingStudioQt.exe`. |

No separate Node.js, Qt SDK or compiler is needed to use these packages. Open the editor, choose **New project**, select a template, and save your game outside the installation folder.

This preview is **unsigned**: Windows may display an unknown-publisher warning or block execution under device policy. Verify downloads against [SHA-256 checksums](https://github.com/miandbits/RaisingEngine/releases/download/v0.1.0-preview.3/SHA256SUMS.txt). Keep project/save backups when upgrading; this is not a stable release.

Installation, first-game and export guides: **[한국어](docs/getting-started/preview/ko.md)** · **[English](docs/getting-started/preview/en.md)** · **[日本語](docs/getting-started/preview/ja.md)**.

**WinGet:** package `miandbits.RaisingEngine.Preview` has been [submitted for registration (#456322)](https://github.com/microsoft/winget-pkgs/pull/456322). Approval and source indexing are pending; use the direct downloads above meanwhile. See [package status and install commands](packaging/winget/README.md).

## Make a game

Open Raising Studio and choose **My first raising game · recommended**. Its guided checklist takes you through a complete three-month game, two endings, playtesting and export using the existing visual tools. Keep user projects outside the engine checkout.

- [First game](docs/getting-started/first-game.md)
- [Preview install and first-game guides — 한국어 / English / 日本語](docs/getting-started/preview/README.md)
- [Editor and game languages](docs/manual/editor-languages.md)
- [Authoring manual](docs/manual/README.md)
- [Public examples](examples/README.md)
- [Game export](docs/manual/qt-game-export.md)
- [Task navigation and game-state previews](docs/creator-workflow.md)
- [App branding, explicit notices and verified backup restoration](docs/creator-release.md)
- [Period-by-period balance and before/after comparison](docs/manual/balance-period-comparisons.md)
- [Verified presentation updates that preserve saves](docs/manual/image-only-updates.md)

## Build from a clean checkout

Install Git, Node.js 22.14.0, CMake, a MinGW C++17 toolchain on PATH, and Qt 6.10.1 with its compatible MinGW 13.1 kit and Ninja. Web additionally requires the official Emscripten SDK. No existing `dist`, private projects or local Git excludes should be needed.

```powershell
$env:ELECTRON_SKIP_BINARY_DOWNLOAD = '1' # archived Electron tools are optional
$env:BUILD_JOBS = '2'
npm ci
npm run build:headless
npm run build:native-player

$env:QT_ROOT = 'C:/Qt/6.10.1/mingw_64'
$env:QT_COMPILER = 'C:/Qt/Tools/mingw1310_64/bin'
$env:NINJA = 'C:/Qt/Tools/Ninja/ninja.exe'
npm run build:studio
npm run studio

$env:EMSDK = 'C:/path/to/emsdk'
npm run build:web
```

`npm ci` reconstructs identical test media from a checked-in source/hash manifest. The player builder obtains dependencies at exact commits from `config/native-dependencies.json`, rejecting modified or mismatched existing checkouts. Read [development setup](docs/development/README.md) for isolated outputs, tests and packaging.

## macOS and iOS from source

On macOS, install Xcode (or Command Line Tools for desktop-only work), Node 22.12+, CMake, Ninja and Qt 6. Use official Node 22.14.0 for reproducible sample-media checks and release packaging. With Homebrew:

```sh
brew install cmake ninja qt node@22
export PATH="/opt/homebrew/opt/node@22/bin:/opt/homebrew/bin:$PATH" # Intel Macs: /usr/local
ELECTRON_SKIP_BINARY_DOWNLOAD=1 npm ci
npm run build:headless
npm run build:native-player
npm run build:qt-studio
npm run studio
```

`QT_ROOT` can point to another Qt desktop installation. The editor selects the native macOS player automatically. Live canvas playback uses a separate player window with pause/reset/stop controls. **File → Export game** offers a macOS app or an iOS Xcode project. Apple exports currently support plugin-free games.

For iOS, install full Xcode and its iOS SDK. The editor prepares an Xcode project automatically. The equivalent build commands are:

```sh
npm run build:ios -- --bundle /path/to/exported-game --out /path/to/new-ios-project
npm run build:ios -- --project /path/to/ios-project --sdk iphoneos
# Add --team YOURTEAMID to enable signing with your configured development team.
```

The default builds an arm64 simulator app on Apple Silicon (x86_64 on Intel); `iphoneos` builds an unsigned device app unless a team is supplied. SDL 3.4.18 is pinned separately for iOS scene lifecycle support; existing desktop SDL sources are preserved. Simulator builds and device builds do not imply App Store readiness or physical-device verification.

A standalone local editor bundle can be created with `npm run package:macos`. Set `MACOS_NODE_ROOT` to an extracted official Node macOS archive containing `bin/node` and `LICENSE`, matching the build architecture. Homebrew Node is unsuitable for bundling because it links external libraries. The packager includes Qt, Node, services, templates, the native player and iOS sources. `MACOS_STUDIO_OUT` must name a new `.app`. This is a development package; release signing, notarization, Qt corresponding-source review and a clean-machine test remain release work. Web export is included if `dist/web` has been built.

Validation: `npm run test:first-game`, `npm run test:first-game-native`, and `npm run test:first-game-ui` (build the `qt-first-game-test` target first). `node scripts/test-ios-game.mjs /path/to/ios-project` tests both starter routes in an isolated simulator, then deletes that simulator.

## Repository map

| Directory | Responsibility |
|---|---|
| `native/studio` | Current Qt editor and widget tests |
| `native/src`, `native/include` | C++ rules and protocol |
| `native/player` | Native graphical player |
| `apps/web`, `apps/player/renderer` | Current Web entry and shared React presentation |
| `apps/qt-studio`, `packages` | Shared editing, validation, export and runtime services |
| `apps/tools` | CLI and MCP |
| `plugins` | Animation extension and integration contract |
| `examples` | Self-contained templates and representative samples |
| `tests/fixtures` | Regression projects and media manifest |
| `legacy` | Archived Electron applications; explicit `legacy:*` commands |
| `docs` | Manual, reference, development and historical archive |

`projects/`, `internal/`, builds, caches and recordings are local-only. Shared React presentation remains production Web code. Repository cleanup does not change game/save formats or plugin contracts.

## Verify and contribute

```powershell
npm run check:repository
npm run check:public-content
npm run test:public-content
npm run test:regression
npm run test:rule-contracts
npm run test:durability
```

Read [CONTRIBUTING](CONTRIBUTING.md), [architecture](docs/reference/architecture.md), [rule extensions](docs/manual/rule-extensions.md) and the [documentation index](docs/README.md).

## License and publication

[MIT](LICENSE) covers original code. [Third-party notices](THIRD_PARTY_NOTICES.md) and [asset provenance](docs/development/public-assets.md) remain separate. Complete dependency/source, media and secret/history review before public binary distribution or publishing rewritten history. Mobile devices and consoles are not certified by Windows/Web tests.
