# Actual iOS system UI regressions

The optional macOS runner exercises UIKit boundaries through real XCTest input. Its ordinary suite covers:

- Files search, background return, rotation and cancellation without changing game state.
- Authored text input across rotation, ordinary dismissal and iPad's actual Hide keyboard control, followed by exact-value game creation.
- Selection replacement, Unicode/emoji deletion, separate fields, iPad Tab and isolated undo/redo, and background return.
- Korean text composition before and after rotation, preserving completed text.
- Japanese marked text and the active editing session across rotation.

The native iOS adapter uses a visible UIKit text field for the focused authored input. UIKit owns text editing, selection, composition and keyboard reopening; ordinary RmlUi change events still update the game's draft. SDL keyboard events do not apply a second edit. A document rebuild reconnects the same field without replacing its text or selection. Each new field session has a separate undo history.

Use Xcode with an installed iOS Simulator runtime. Build the project service and a simulator iOS export first. Supply the same source project that produced that export:

```sh
node scripts/test-ios-system-ui.mjs /absolute/path/to/ios-export \
  --project /absolute/path/to/project --family iphone
```

Use `--family ipad` for the tablet case and `--text-scale 2` for 200% text. `--test files|input|selection|composition|japanese` selects one case; the default `all` runs the five ordinary cases. Composition tests inspect or add keyboards through Settings on the newly created simulator only. The export must contain `build-iphonesimulator/native-player/Release-iphonesimulator/RaisingPlayer.app`. The input project must have an authored `title` screen; the public “My first raising game” starter is the intended fixture source. The runner verifies the project fingerprint against the supplied app before making a cache-only copy.

The runner never changes the original export or source project. In the copied app it replaces the title with a small text-input probe and adds a readable record modal, while retaining the native executable byte for byte. A new uniquely named Simulator and app identifier isolate its records and settings. The test invokes ordinary game controls and real system UI; a localhost endpoint only returns the player's `--editor-events` diagnostics. It does not inject picker results or dispatch game commands.

Evidence is written under `.cache/ios-system-ui/run-*`: the result receipt, source and fixture fingerprints, native executable hash, harness hash, authored probe, logs, and XCTest result bundles with screenshots and accessibility transcripts. `.cache/ios-system-ui/latest.json` points to the latest result. The runner shuts down and deletes only the Simulator it created. Use `--keep-failed-device` to retain that isolated device for diagnosis after a failure. Each external build or test process has a ten-minute limit; XCTest may spend part of that time waiting for UIKit animation notifications. A timeout remains a failed, incomplete run.

`--test composition-probe` separately tests rotation between Korean jamo. On the tested iPadOS 27 simulator, typing `ㅎ`, rotating, then typing `ㅏㄴ글` has produced `ㅎㅏㄴ글`. An isolated standard UIKit field reproduced the same failure, though other reference runs composed correctly. The strict probe is separate from the ordinary suite; its intermittent failure remains open and is not repaired by rewriting user text. Recheck it on other OS versions and physical devices.

The same simulator/XCTest setup has also intermittently split Korean jamo during ordinary typing, before rotation. A standard UIKit reference reproduced this without the engine. Keep the ordinary Korean test strict: a passing rerun does not erase a failed run or establish reliable Korean composition on this OS/device combination. This remains an open platform acceptance item.

Run the independent reference with `python3 scripts/probe-ios-keyboard.py --family ipad --mode typing`, or use `--mode mid-syllable-rotation`. Use `--runtime RUNTIME_ID` to select another installed OS from `xcrun simctl list runtimes`. It builds a plain UIKit app, uses a fresh simulator, records actual keyboard input and screenshots, and deletes only that simulator. Results are under `.cache/ios-keyboard-reference/run-*`. `completed` means an observation was collected; `textMatched` records whether the expected text was produced. A mismatch exits unsuccessfully, and `productAcceptance` is always false. This reference does not certify engine behavior or physical devices.

A pass covers only the selected focused Simulator regressions. It does not establish full game usability, save import/export correctness, cloud-provider behavior, physical-device behavior, signing, or App Store readiness. The broader portable-save acceptance matrix remains separate. A failure must preserve the actual screenshot and system hierarchy so an observed OS presentation delay or test-targeting problem is not mislabeled as a game defect.
