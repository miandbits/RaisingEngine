# Qt game export

Use **파일 → 게임 내보내기…**, select Windows or Web, choose an existing parent
directory and enter a new folder name. The dialog validates the current editor
draft and exports it through the shared `exportGame` service. Unsaved changes
are included; project files are not saved implicitly. Existing output folders
are rejected rather than overwritten.

On success the dialog shows the target, absolute output directory and content
fingerprint, with a button to open the directory. Windows produces a player
executable with its runtime and assets. Web produces files to serve through a
web server; export does not upload or publish them.

The Windows runtime respects `RAISING_NATIVE_PLAYER_RUNTIME`; the default Web
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
