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
multi-monitor docks, all modal tools, IME composition, and every platform remain separate checks.
