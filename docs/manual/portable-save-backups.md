# Portable save backups

A portable backup is one JSON file containing a game's saved progress. It is a
copy you can keep outside the app. It is separate from the manual records and
automatic checkpoints shown in the game's record list.

## iPhone and iPad

In a game with a save or load record screen, open that screen and choose
**Export current progress** under **File backups**. Choose a destination in the
system Files picker, then finish saving there. The suggested filename is
`raising-save.json`; you can use the system picker to choose another name.
Export uses the current game, even when a different record is selected in the
list. It does not overwrite the selected manual record or move the live save.
Export is unavailable before a game has started.

To continue a backup, choose **Import backup**, then select one JSON file in
Files. The game validates the file and shows **Review backup**, including the
incoming date and place when available. Confirm only when you want to replace
the current unsaved progress. Your manual records remain unchanged. Save a new
manual record afterward if you want that imported position in the record list.

Cancel either the Files picker or the review to keep the current game. In a
compact iPhone folder view, Files may put Cancel on the Browse screen; return
to Browse or swipe the sheet down to dismiss it. Import errors leave the
current progress in place. The game pauses its automatic
progression while the picker or review is open. Returning from another app
should leave the operation pending or cancelled; it does not silently accept an
import.

These controls are supplied by the native iOS player to existing save/load
record widgets. A custom game without a reachable record screen may need its
designer to add one. For phone-sized record controls, enable a reading layout
on that record screen, put its save/load widget in the reading order, and include
its Close button in the actions. Legacy canvas record screens retain their
authored scaling; adding backup tools does not make every canvas target 44 points.
The import review itself uses a readable physical-size layout.

This does not expose the app's private save directory in Files. Files
destinations depend on the device and its installed providers;
choosing an iCloud destination does not make the engine a cloud-sync service.

## Moving between players

The Web player's **Download backup** and **Import backup** use the same save
format. Native desktop saves also use this format. The destination must have
compatible game content and engine rules. A file from another game or a changed
content fingerprint is rejected rather than silently converted. A publisher may
provide a separately tested migration when an update intentionally changes the
content.

Portable files do not carry captured slot images. Missing screenshots do not
invalidate progress. Recent reading is session-only: a successful replacement
clears that transcript. It is not exported as a persistent reading history.
Player preferences are not a promise of cross-device settings transfer.

Keep a second copy of an important backup. Removing an app or clearing browser
storage can remove the saves held by that app or browser. The engine reports
completion after Files accepts the export; later provider synchronization and
storage retention are outside the game's control.

## Designer and test scope

The native file-picker support is platform-specific. `save-slots-v1` alone does
not establish that a runtime has Files access. Check the platform-specific
acceptance evidence shipped with the iOS runtime.
These automatic record tools introduce no new authored contract or compatibility
requirement. Existing authored actions, save JSON versions and game rules are
unchanged.

Validation accepts bounded JSON backups up to 16 MiB, checks duplicate keys and
nesting, and applies the ordinary strict replay, fingerprint and presentation
validation before replacing the live game. It does not execute code from a
backup. Provider filenames are display information, not game instructions.

Tests using a substitute file-picker bridge cover host transactions and failure
paths. They do not prove the system picker works. iOS acceptance must separately
exercise real Files selection, export, cancellation and dismissal on iPhone and
iPad, including enlarged text, rotation and backgrounding. Simulator results do
not establish physical-device, third-party provider, signing or App Store
readiness. Those remain separate release checks.
