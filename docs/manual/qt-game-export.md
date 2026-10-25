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

## Check existing saves before an update

Expand **Check existing saves** in the export dialog and choose **Previous project backup**. Use the saved, editable project folder retained with the old release, such as an extracted `Project-backup.zip`; an exported `game.json` alone is not a project backup. Save the current project first. The check never saves drafts or modifies project documents, undo history, or original player saves. Ordinary first-release exports remain available without this optional check.

The result leads with what happens to existing saves:

- **Same content identity:** the fingerprint is unchanged. An optional example save also checks its save/core version, command replay and final-state integrity. Without a sample, this is an identity comparison only.
- **Changed content identity:** old saves cannot open directly in this ordinary export. Layout and text edits can change identity even when gameplay rules are unchanged. Choose **Review save migration…** to test conversion.
- **Different game IDs:** migration is unavailable. Preserve the original game and its saves.

Game IDs and full fingerprints are available under **Check details**. An invalid example save does not become migration evidence. One passing example cannot guarantee every player's save; include representative late-game and in-progress checkpoints.

**Review save migration…** opens the existing migration dialog with both saved project folders and the optional sample prefilled. It validates the original against the old project, replays every command on the new project, and reports changed canonical values. If presentation reset is required, opt in explicitly: it removes the unconfirmed planner draft, record metadata and thumbnail reference, playback position and opaque plugin presentation state. It does not copy unverified gameplay state. Review the differences, then write to a **new file**. Existing destinations and originals are never overwritten. A truncated difference report cannot be approved through the dialog.

Changing the selected files clears the result. Checked exports revalidate the previous project, current saved project, sample bytes and editor revision before publication; an external edit requires another check. A failed export clears the displayed check. Migration publication separately recomputes its approved result and rejects stale approval.

Load the converted save in the newly exported target player, save again, and test a fresh launch before distributing the update. The editor comparison is separate from actual target-platform acceptance. The narrow [image-only update API](image-only-updates.md) is not exposed by this check; an ordinary export still uses the current fingerprint.

For iOS, **iOS build number** defaults to `1`. Increase it for each new build you distribute. It accepts one to three dot-separated nonnegative integers, with one to nine digits per part, for example `2` or `1.2.3`. The release version and iOS build number are sent to iOS export even when archive creation is unchecked. The build-number field is hidden for other targets.

Focused coverage: `tests/qt-studio/update-compatibility.test.mjs` tests saved identity, samples, stale/disk changes, publication guards and strict migration. `qt-update-compatibility-test` exercises the actual localized dialogs, prefilled migration, explicit reset, cancellation, unchanged source bytes and iOS metadata parameters; its iOS request-boundary fixture avoids creating Xcode projects and does not replace actual iOS export acceptance.
