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

`--test files-roundtrip` saves a backup through the system Files picker to the local On My iPhone or On My iPad location. It terminates the player, starts a new process at the title, then reselects the exported file, cancels the first review, and confirms the second. The test checks exact restored state and commands. The runner records both process IDs and checks that the private staging directory is empty while each process is still running.

The runner never changes the original export or source project. In the copied app it replaces the title with a small text-input probe and adds a readable record modal, while retaining the native executable byte for byte. A new uniquely named Simulator and app identifier isolate its records and settings. The test invokes ordinary game controls and real system UI; a localhost endpoint only returns the player's `--editor-events` diagnostics. It does not inject picker results or dispatch game commands.

Evidence is written under `.cache/ios-system-ui/run-*`: the result receipt, source and fixture fingerprints, native executable hash, harness hash, authored probe, logs, and XCTest result bundles with screenshots and accessibility transcripts. `.cache/ios-system-ui/latest.json` points to the latest result. The runner shuts down and deletes only the Simulator it created. Use `--keep-failed-device` to retain that isolated device for diagnosis after a failure. Each external build or test process has a ten-minute limit; XCTest may spend part of that time waiting for UIKit animation notifications. A timeout remains a failed, incomplete run.

`--test input-idle` separately holds a focused native input for eight wall-clock minutes, checks that its draft and editing session remain unchanged, then exercises rotation, Hide/refocus and exact-value game creation. The runner samples the app's physical footprint (including compressed pages) with macOS `footprint` every 15 seconds, retaining it alongside RSS in `input-memory-samples.json`. RSS alone cannot satisfy this gate: page reclamation can hide allocation growth. It compares the median of the first and last five idle samples, excluding the first 15 seconds, and fails if growth exceeds 8 MiB or the median interval growth rate exceeds 1 MiB/minute. The interval rate is retained alongside the net change for diagnosis. This is a focused regression for per-frame temporary-object retention, not a universal memory budget or proof of no leaks. The ordinary five-case suite does not include this longer check.

`--test composition-probe` separately tests rotation between Korean jamo. On the tested iPadOS 27 simulator, typing `ㅎ`, rotating, then typing `ㅏㄴ글` has produced `ㅎㅏㄴ글`. An isolated standard UIKit field reproduced the same failure, though other reference runs composed correctly. The strict probe is separate from the ordinary suite; its intermittent failure remains open and is not repaired by rewriting user text. Recheck it on other OS versions and physical devices.

The same simulator/XCTest setup has also intermittently split Korean jamo during ordinary typing, before rotation. A standard UIKit reference reproduced this without the engine. Keep the ordinary Korean test strict: a passing rerun does not erase a failed run or establish reliable Korean composition on this OS/device combination. This remains an open platform acceptance item.

Run the independent reference with `python3 scripts/probe-ios-keyboard.py --family ipad --mode typing`, or use `--mode mid-syllable-rotation`. Use `--runtime RUNTIME_ID` to select another installed OS from `xcrun simctl list runtimes`. It builds a plain UIKit app, uses a fresh simulator, records actual keyboard input and screenshots, and deletes only that simulator. Results are under `.cache/ios-keyboard-reference/run-*`. `completed` means an observation was collected; `textMatched` records whether the expected text was produced. A mismatch exits unsuccessfully, and `productAcceptance` is always false. This reference does not certify engine behavior or physical devices.

A pass covers the selected Simulator cases and local provider. The broader portable-save acceptance matrix includes other providers, malformed backups, physical devices, and distribution. A failed test keeps the available screenshot, system hierarchy and tool logs to distinguish a game defect from an OS presentation or targeting problem.

## VoiceOver focus and speech

Use Xcode 27 and an installed iOS 27 or later iPhone Simulator runtime with a built public starter export.

```sh
node scripts/test-ios-voiceover.mjs \
  /absolute/path/to/ios-export/build-iphonesimulator/native-player/Release-iphonesimulator/RaisingPlayer.app
```

The public `XCUIVoiceOverService` moves VoiceOver focus and captures spoken output on the starter title and reading menu at 200% game text. The test opens the menu with an ordinary tap while VoiceOver is disabled, then re-enables VoiceOver to check its spoken controls. Gesture activation and physical-device acceptance remain separate. [Apple’s VoiceOver test service](https://developer.apple.com/documentation/xcuiautomation/xcuivoiceoverservice)

The `iOS Simulator` GitHub workflow builds a fresh public starter export on the standard `xcode-27` runner and runs the focused Files round trip and VoiceOver speech cases. It retains the checkout identity, toolchain, XCTest bundles, and public fixture receipts. Local VoiceOver evidence is written under `.cache/ios-voiceover/run-*`.
