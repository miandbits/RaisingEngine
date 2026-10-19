# Development and reproducibility

The [root README](../../README.md) is the supported build sequence. Run commands at the repository root. Qt, CLI and MCP share Node/C++ services; the archived Electron editor is not required.

## Requirements and outputs

- Node 22.14.0 and npm; install with `npm ci`.
- CMake and MinGW on PATH for the Windows core/player. Set `QT_ROOT`, `QT_COMPILER`, `NINJA` for the Qt 6.10.1 desktop kit.
- Set `EMSDK` to the SDK directory containing `.emscripten` and `upstream/emscripten`. The external compiler is a prerequisite, not a copied engine build cache.
- `npm run prepare:native` obtains pinned upstream sources from `config/native-dependencies.json`. It preserves and rejects modified/mismatched existing checkouts.
- Standard outputs: `dist/qt-studio`, `dist/native-player-app`, `dist/web`. Overrides: `QT_STUDIO_OUT`, `NATIVE_PLAYER_BUILD`, `NATIVE_PLAYER_OUT`, `WEB_OUT`.
- Limit build load with `BUILD_JOBS=2`. Use separate outputs and offscreen tests while the desktop is in use.

## Fixtures and tests

`npm ci` runs `prepare:fixtures`. Identical media are materialized at historical fixture paths from `tests/fixtures/media-manifest.json`. Hashes are verified; local edits are never overwritten. If lifecycle scripts are disabled, run `npm run prepare:fixtures` explicitly. Public templates retain their own assets so copied projects are independent.

Run `check:repository` and `check:public-content` before committing structural changes. Linux headless CI does not imply Qt/Windows/browser verification.

## Standalone preview

After building the core, Qt editor, Windows player and Web player:

```powershell
npm run package:qt-studio
npm run test:qt-preview
$env:RAISING_TEST_WEB_EXPORT = (Get-Content .cache/qt-release/latest.json | ConvertFrom-Json).web
node --test tests/journey/authored-adventure-web.test.mjs
```

Packaging requires a new output directory; set `QT_PACKAGE_OUT` on repeated runs. The preview test uses actual Qt widgets offscreen to create/edit a Unicode-path project, export both players and verify hidden Windows save replay. The Chromium test covers exploration, battle and downloaded save replay. Temporary test helper DLLs come from the installed Qt SDK; product files come from the new package.

`test:qt-package` additionally exercises an NSIS installer. Set `NSIS_COMPILER` to `makensis.exe`. Portable tests do not establish installation or clean-VM certification. See [packaging history](qt-preview-release.md).

## Legacy and publication

See [legacy policy](../../legacy/README.md). Historical Electron development dependencies remain in the root lockfile for opt-in regression work; set `ELECTRON_SKIP_BINARY_DOWNLOAD=1` for Qt/Web builds.

The [cleanup report](repository-cleanup.md) records clean-checkout acceptance and an isolated history experiment. Original shared branches/worktrees are not rewritten. [Public assets](public-assets.md), [release gates](open-source-readiness.md) and [platform support](platform-support.md) remain authoritative.