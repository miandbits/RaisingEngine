# Authored save panels (Windows source builds)

For multiple manual slots and automatic checkpoints, see [Save slots](save-slots.md).

Create a modal screen and open it with an `openModal` button. Its `save`, `load`,
and `closeModal` buttons operate on the player's configured save file. Opening a
panel does not advance the simulation or change its location.

The Qt text-binding selector exposes **Saved record summary** (`save-summary`)
and **Player feedback** (`player-notice`). These work on screens and reusable
components. The following optional `valueFormat` values select save metadata:

| Format | Display |
| --- | --- |
| omitted | Saved game date, period, and destination; narrative/finished indication |
| `attributes` | Saved shared attributes, restricted and ordered by `attributeIds` |
| `status` | Empty, unreadable, compatible, or other-content-version status |
| `write-warning` | Overwrite explanation, cleared after a successful modal save |

Place live `attribute-value` elements next to the saved values when a comparison
is useful. Label each column explicitly: saved values never follow the live
state. Attribute summaries use the project's labels; authors should explain units
or limits alongside them. A record's screen/location label is resolved from the
current content. Exact text may differ if the project has renamed that content.

`player-notice` displays the last operation's feedback, including errors. A visible
authored feedback element replaces the default status bar. When a modal is open,
background feedback is hidden and only feedback on that modal takes precedence.
For keyboard safety, place `closeModal` before a destructive load button in the
element order, even when their visual positions differ.

Load buttons are disabled for missing or unreadable save metadata. A successfully
saved modal disables its save button until the panel is reopened. Invalid files
are never overwritten just by opening a preview. Loading still performs the
runtime's full save verification; readable metadata is not a guarantee that an
older save is compatible. Projects without slot widgets continue to preview the configured single file.

These bindings require `save-panel-v1`. Export rejects runtimes without that
capability, including the current web runtime. They are not included in previously
published preview binaries. Build the Windows player and Qt editor from source.

Validation:

```powershell
node --test tests/native-player/save-panel.test.mjs
```

Set `NATIVE_PLAYER_TEST_RUNTIME` to use an isolated Windows runtime output. The
test covers missing/damaged records, stable saved values while live state changes,
exact restoration, authored modal styles, repeated-save prevention, and export
rejection for a renderer lacking the capability.
