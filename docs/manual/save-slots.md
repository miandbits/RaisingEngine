# Save slots and automatic checkpoints

Add **Save slots · write record** or **Save slots · load record** from the Qt
element palette to an authored modal screen. Connect ordinary `openModal` buttons
to those screens. Place an authored `closeModal` button before the widget in the
element order for safe keyboard focus. F5/F9 open the corresponding authored
modal when slot mode is enabled; they do not overwrite or load immediately.

The widget uses its normal position, size, font, `color`, `fill`, `hoverFill`
(selection/accent), and `borderColor`. A 1160×500 widget works on a 1280×720
canvas. Use fit scaling for smaller windows. It supports reusable components.
The slot cards show the game date, screen/location name, and save timestamp.
Native players also capture the gameplay screen. The Web player truthfully shows
**No captured image** and stores an empty thumbnail reference; it does not capture
the DOM or include a screenshot library. Captures preserve 16:9
proportions; they do not contain the save/menu overlay. Names are frozen at save time. Custom calendar dates use the current translated
calendar labels when browsing records. Legacy records without metadata show a date and no screenshot.

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
report an error and do not replace manual records. The legacy mobile lifecycle recovery file is separate from the slot browser.

On native players, manual slot 1 uses the existing `--save` file. Other files are under
`<save-stem>.slots/`, so independent playtest profiles cannot collide. Each save
atomically replaces its JSON envelope and retains a `.bak`. Screenshot metadata
is in `playerPresentation.saveRecord`; the image is an immutable PNG written
before committing that envelope. Images referenced by current records and
backups are retained; obsolete captures are pruned. Nothing is written to the
installed game's content/assets directory. Existing single-file projects and
save envelopes without this optional metadata retain their behavior.

The Web player stores each record in an atomic IndexedDB transaction, isolated by
browser origin, deployment directory and `gameId`. Updating content keeps old
records visible as incompatible; it never silently replays against different
content. Another tab changing a record invalidates its pending confirmation.
Failed writes preserve the previous record. Storage failure is reported; the
player keeps running and **Download backup** remains available. Browser storage
is tied to that browser profile and may be cleared by its owner or storage policy.
Download a backup before moving hosting locations or clearing browser data.

**Download backup** and **Import backup** use the existing portable JSON save
format and replay validation. Importing during a game requires confirmation.
Importing or loading does not create an automatic checkpoint; explicitly save a
manual record to retain the imported position in this browser. Screenshots are
not included in portable JSON files. Records without screenshots remain valid
on native players. No cloud sync or cross-device storage is implied.

Requires `save-slots-v1`. Export rejects incompatible native and Web runtimes.
This capability includes authored save/load modals, their open/close controls,
paging, keyboard focus, decorative images/text/statistics, and backup controls.
Gameplay widgets inside a record modal, ordinary modals, scrolling, sorting, selection and
other advanced composition still require their separate runtime support.
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

Web verification:

```sh
node --test tests/native/web-save-slots.test.mjs tests/runtime/native-slot-metadata.test.mjs
RAISING_BROWSER_CHANNEL=chrome node --test tests/journey/save-slots-web.test.mjs
```

The browser test drives authored controls through a normal export and verifies
IndexedDB persistence across reload, exact restored state, ring retention,
confirmation cancellation and conflicts, damaged records, quota failures,
backup download/import and unavailable storage. On CI, omit
`RAISING_BROWSER_CHANNEL` to use Playwright's installed Chromium.
