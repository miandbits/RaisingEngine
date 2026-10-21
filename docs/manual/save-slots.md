# Save slots and automatic checkpoints (Windows source builds)

Add **Save slots · write record** or **Save slots · load record** from the Qt
element palette to an authored modal screen. Connect ordinary `openModal` buttons
to those screens. Place an authored `closeModal` button before the widget in the
element order for safe keyboard focus. F5/F9 open the corresponding authored
modal when slot mode is enabled; they do not overwrite or load immediately.

The widget uses its normal position, size, font, `color`, `fill`, `hoverFill`
(selection/accent), and `borderColor`. A 1160×500 widget works on a 1280×720
canvas. Use fit scaling for smaller windows. It supports reusable components.
The slot cards show a captured gameplay screen, game date, screen/location name,
and each record's local wall-clock timestamp. Captures preserve 16:9
proportions; they do not contain the save/menu overlay. Names and dates are frozen
at save time. Legacy records without metadata show a date and no screenshot.

Optional project configuration:

```json
"saveSystem": { "manualSlots": 6, "autoSlots": 3 }
```

This is under `visual`. Manual counts support 1–30, automatic counts 0–9.
Defaults are six and three; setting automatic count to zero disables checkpoint
rotation. Pages contain six records. Automatic records are sorted newest first
and cannot be manually overwritten. Empty, unreadable, or incompatible records
cannot be loaded. Browsing never rewrites damaged records.

Saving to an occupied manual slot requires confirmation. Loading any record
requires confirmation and warns that unsaved progress will be lost. Cancel,
Escape, and closing the modal leave gameplay unchanged. If another process
changes the selected file during confirmation, the operation is rejected and the
user must select it again. Successful loading still verifies the command replay,
content fingerprint, timeline and plugin presentation states before changing the
live game. It never silently loads a backup or a different slot.

Automatic checkpoints are written after a newly rendered gameplay commit: new
game, choices, activity results and other rule-changing actions. Screen/location
navigation and consecutive dialogue lines do not consume the ring. Finishing a
story creates another checkpoint. Modal browsing, recollection playback and
loading a record do not themselves create checkpoints. Failed automatic writes
report an error and do not replace manual records. The legacy mobile lifecycle
recovery file is separate from this browser; this change does not claim mobile
or web support.

Manual slot 1 uses the existing `--save` file. Other files are under
`<save-stem>.slots/`, so independent playtest profiles cannot collide. Each save
atomically replaces its JSON envelope and retains a `.bak`. Screenshot metadata
is in `playerPresentation.saveRecord`; the image is an immutable PNG written
before committing that envelope. Images referenced by current records and
backups are retained; obsolete captures are pruned. Nothing is written to the
installed game's content/assets directory. Existing single-file projects and
save envelopes without this optional metadata retain their behavior.

Requires `save-slots-v1`. Export rejects incompatible Windows and web runtimes.
This feature is not in the previously published preview binary.

Verification:

```powershell
$env:NATIVE_PLAYER_TEST_RUNTIME='dist/native-player-slots'
node --test tests/native-player/save-slots.test.mjs tests/native-player/save-panel.test.mjs
```

The native test drives real rendered buttons, checks exact state restoration,
cancel behavior, ring retention, metadata and PNG files, damaged-record
isolation, legacy records and old-renderer rejection. Qt `saveSlotsAuthoring`
checks palette creation through the normal authoring service.
