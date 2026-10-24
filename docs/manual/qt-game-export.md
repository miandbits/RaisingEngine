# Qt game export

Use **파일 → 게임 내보내기…**, select macOS, iOS, Windows or Web (available targets depend on the host), choose an existing parent
directory and enter a new folder name. The dialog validates the current editor
draft and exports it through the shared `exportGame` service. Unsaved changes
are included; project files are not saved implicitly. Existing output folders
are rejected rather than overwritten.

On success the dialog shows the target, absolute output directory and content
fingerprint, with a button to open the directory. Windows produces a player
executable with its runtime and assets. Web produces files to serve through a
web server; export does not upload or publish them.

The current desktop runtime respects `RAISING_NATIVE_PLAYER_RUNTIME`; the default Web
runtime is `dist/web`. Schema errors, stale project revisions, external file
changes and unsupported renderer capabilities prevent a successful export.
Errors remain visible in the dialog with retry guidance. After correcting a
runtime or destination problem, the same dialog and output name can be retried.
If the project revision changed, reopen the dialog to use the new revision.

The export includes an immutable snapshot, its manifest and relative asset
paths. It does not change gameplay saves, project revision, dirty state or
undo/redo history.

The focused Qt `exportGameDialogRoundtrip` test passed on 2026-10-13, exporting
both actual Windows and Web runtimes from an unsaved disposable project. It
verifies the changed activity in each bundle, existing-folder rejection, an
unchanged source file and preserved revision/documents/dirty/undo state. Final
Qt output is `dist/qt-studio-export-retry/RaisingStudioQt.exe`;
`Start-PM2-Editor.cmd` selects that build and the scene-preview native runtime.
Independent service tests in `tests/runtime/qt-export.test.mjs` also passed
4 cases: both targets and relative assets, whole-project hashes/history,
invalid target/path/content, existing destinations and stale revisions. A
revision or disk change during export is checked before publishing the staged
bundle. Failure removes only that operation's temporary directory and leaves no
completed destination. Existing destinations are preserved. Packaging failures
also clean up staging, allowing retry with the same output path.

Follow-up regression and actual editor evidence:
Claude fixes verification (historical internal evidence; not distributed).

## Apple targets (source builds)

macOS exports `RaisingPlayer.app/Contents/Resources/game.json` and contained assets. Its player executable is `Contents/MacOS/RaisingPlayer`. The service selects this layout for Play and renderer previews as well as export. Export signs the completed app ad hoc on macOS; it does not notarize it.

iOS export stages a plugin-free game and pinned C++ sources, generates CMake presets for `iphonesimulator` and `iphoneos`, then invokes CMake to create an Xcode project at the final output path. Xcode generation failure is reported separately from successful source export, leaving the regeneration command and sources available. A moved export must regenerate its Xcode project. No signing identity is selected automatically. See the [first-game guide](../getting-started/first-game.md).

Runtime overrides: `RAISING_MACOS_PLAYER_RUNTIME`, `RAISING_WINDOWS_PLAYER_RUNTIME`, `RAISING_WEB_PLAYER_RUNTIME`, and `RAISING_IOS_PLAYER_RUNTIME`. The iOS runtime supplies fonts/licenses/capability metadata; executable source preparation is mandatory. Native plugin exports on macOS/iOS are rejected until compatible plugin runtimes exist.

## Versioned release workflow (2026-10-23)

The checklist remembers a preferred target and reports missing runtime, unsupported feature/asset, or plugin-platform issues before export. This check uses the same core feature detection as the exporter. Validation, plugin-specific export checks and playtesting remain necessary.

The export form defaults to version `0.1.0` and creates `Game.zip`, `Project-backup.zip` and `RELEASE.json`. Game archives preserve executable permissions and exact exported bytes; project backups preserve the current editable draft, referenced assets, named routes, approved baselines and pinned plugin-store files. Missing or modified pinned dependencies prevent claiming a complete backup. See [the release and recovery instructions](../getting-started/first-game.md).

**Play exported game** launches the native player, opens the generated iOS Xcode project (or its source folder if generation needs attention), or starts a Web preview bound only to `127.0.0.1`. The local preview lasts until the editor closes and does not serve the creator backup or release manifest. Publishing remains an explicit action outside the editor. Invalid version, existing destination and failed preparation preserve the form inputs and remove only that attempt's staging folder.
