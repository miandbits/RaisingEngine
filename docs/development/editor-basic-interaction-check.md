# Basic editor interaction stabilization

The 2026-10-22 time-boxed pass adds panel navigation without changing project or save formats.

- `F6` moves to the next visible panel; `Shift+F6` moves back.
- **View → Focus Panel** restores a closed panel and moves keyboard focus into it.
- Panel order remains stable when tabbed docks are raised. Read-only panels are focus stops too.
- **Authoring Tools** retains access to toolbar commands in narrow windows.
- The unapplied-workspace warning explicitly maps Escape to Cancel, like the project-unsaved warning.

Existing keyboard commands remain: Ctrl+C/V for elements or focused text, Ctrl+Z for undo,
Ctrl+Y or Ctrl+Shift+Z for redo, and Ctrl+S for project save. Text widgets retain their own editing history.

## Verification

`qt-editor-interaction-test` contains a project-independent panel-navigation test and an opt-in
project journey. Set `RAISING_INTERACTION_PROJECT` to a **disposable copy** and
`RAISING_ENGINE_ROOT` to the repository root. The journey changes and saves that copy.
Use `RAISING_QT_TEST=1` to keep user window preferences unchanged.

The journey checks 1000×680 layout, keyboard element copy/paste and undo/redo, text copy/paste
and local undo/redo, panel round trips, closed-panel restoration, Escape from the unsaved warning,
Ctrl+S, closing and reopening with identical project data, and validation.

A separate Windows GUI pass used a private workshop project copy: changed an element name,
attempted to close while the field was focused, cancelled the unsaved warning, saved via Ctrl+S,
then closed and reopened the editor and confirmed the changed name persisted. The narrow GUI
window was 1000 pixels wide. Private project contents and screenshots are not part of this repository.

This is a bounded stabilization pass, not an exhaustive keyboard/accessibility audit. Floating
multi-monitor docks, all modal tools, and every platform remain separate checks.

## Korean input and tool navigation pass

The second 20-minute pass on 2026-10-22 adds:

- Ctrl+K tool search: Up/Down selects enabled results while keeping the search field focused;
  Enter opens the selection. Navigation does not steal arrows during active text composition.
- F6 panel changes, Ctrl+K tool search and project saves ask the input method to commit pending composition first.
  A still-active composition blocks the transition instead of silently losing the preedit text.
- In editor-owned text fields, the first Escape or Ctrl+Z during composition cancels only
  preedit. It does not dismiss the tool or undo already committed project content.
- Child tool dialogs use QObject ownership for input handling; QWidget ancestry alone excludes
  these window boundaries. Confirmed Korean text keeps Qt's local copy/paste and undo/redo.
- Saving flushes the focused main-editor field before the asynchronous project save. This does
  not apply every tool's unapplied draft; those tools retain their explicit Apply/Save workflow.

The Qt test target passed 6 cases including initialization/cleanup (4 substantive cases): panel
navigation, composition/cancel in a child dialog, keyboard tool search, and the disposable-project
save/reopen journey. Composition tests send QInputMethodEvent preedit/commit events and verify
that project save and F6 wait for unresolved preedit. They also verify Escape, local undo/redo,
Korean clipboard text, and that search arrows do not interfere with composition.

The Windows GUI pass opened a relationship tool with Ctrl+K, Korean search, Down and Enter,
cancelled with Escape, then renamed a UI element in a private workshop copy using Korean text.
Ctrl+Z/Ctrl+Y, Ctrl+C/Ctrl+V and Ctrl+S were exercised in the focused field. Closing and reopening
the editor confirmed the saved Korean name persisted. The final review build uses a separate
output folder and test profile; existing user windows and project files were preserved.

Evidence boundary: GUI Korean text was inserted as Unicode. Physical Microsoft Korean IME key
composition was not verified: the desktop automation API did not support its Hangul toggle key.
The event-level tests are not a substitute for testing that complete Windows IME pipeline,
candidate windows, Japanese conversion, or every custom text widget.

Qt contracts: [input-method events](https://doc.qt.io/qt-6/qinputmethodevent.html),
[commit/reset](https://doc.qt.io/qt-6/qinputmethod.html).
