# Raising Studio

A 2D raising-simulation engine with a **Qt editor**, a deterministic **C++ runtime**, and **Windows / Web players**. Designers and LLM tools edit the same validated project data.

**Authoring alpha.** Original code is MIT licensed. See the [support matrix](docs/development/platform-support.md) for verified features and platform limitations.

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

Open `RaisingStudioQt.exe` to create or open a project. Keep user projects outside the engine checkout.

- [First game](docs/getting-started/first-game.md)
- [Preview install and first-game guides — 한국어 / English / 日本語](docs/getting-started/preview/README.md)
- [Editor and game languages](docs/manual/editor-languages.md)
- [Authoring manual](docs/manual/README.md)
- [Public examples](examples/README.md)
- [Windows / Web export](docs/manual/qt-game-export.md)

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
