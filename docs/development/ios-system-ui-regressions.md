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

`--test files-roundtrip` saves a backup through the system Files picker to the local On My iPhone or On My iPad location. It terminates the player, starts a new process at the title, then reselects the exported file, cancels the first review, and confirms the second. The test checks exact restored state and commands. The runner records both process IDs and checks that the private staging directory is empty while each process is still running. The same test invocation runs the separate `SystemUIDiagnosticsTests` reader regressions and records them independently from the two real Files cases.

Build a current Simulator export for `files-roundtrip`. The player reports its normal save path on per-launch stdout when the runner requests a sandbox receipt. The runner binds the receipt to the launch nonce, process ID and fixture fingerprint, checks the path against the owned Simulator's device inventory, and verifies that both launches use the same save directory.

The runner never changes the original export or source project. In the copied app it replaces the title with a small text-input probe and adds a readable record modal, while retaining the native executable byte for byte. A new uniquely named Simulator and app identifier isolate its records and settings. XCTest invokes ordinary game controls and real system UI. It reads complete `--editor-events` records from the live player's per-launch stdout. The import check compares restored state with the successful export's checkpoint from the same run.

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

The public `XCUIVoiceOverService` moves VoiceOver focus and captures spoken output on the starter title, reading menu and first schedule at 200% game text. The test uses ordinary taps with VoiceOver disabled to open the menu and start the game. It then re-enables VoiceOver to check spoken controls and the separately exposed Skill name and initial value. Gesture activation and physical-device acceptance remain separate. [Apple’s VoiceOver test service](https://developer.apple.com/documentation/xcuiautomation/xcuivoiceoverservice)

The `iOS export and Simulator Files` GitHub workflow uses Xcode 27.2 on the standard `xcode-27` runner to build a fresh public starter export and compile the VoiceOver XCTest harness without starting its Simulator. A separate standard `macos-26` job verifies the same checkout and app archive hashes, then runs the actual Files picker cases using Xcode 26.6 and iOS 26.5 (23F77) on iPhone 17e. The workflow retains checkout identity, toolchains, XCTest bundles and public fixture receipts.

Use `run_voiceover` on a manual workflow dispatch to explicitly attempt the real hosted VoiceOver test. The default hosted check covers compilation of that harness. Actual VoiceOver focus and spoken output are verified locally on iOS 27, with receipts under `.cache/ios-voiceover/run-*`. The standard Xcode 27 hosted image timed out before XCTest in separate app and app-independent checks.

The test scripts choose the newest available runtime with a supported device in the requested family when neither is pinned. Set `RAISING_IOS_SIMULATOR_RUNTIME` and `RAISING_IOS_SIMULATOR_DEVICE_TYPE` to override the runtime or device type by its full Simulator identifier. An unavailable explicit choice fails before device creation. Setup and cleanup failures are recorded separately.

### Hosted first-contact failures

The retained iOS 26.5 (23F77) and iOS 26.4.1 (23E254a) failure logs show BackBoard skipping later events for the Recap touch path after the initial contact. Detaching that digitizer canceled the still-active contact in the player's scene. The Start control can gain focus while the game remains on the title screen. The iOS 26.5 cancellation occurred with Xcode 26.6 and 26.5. [Original failure](https://github.com/miandbits/RaisingEngine/actions/runs/43129725228/job/130898255537) · [Client comparison](https://github.com/miandbits/RaisingEngine/actions/runs/43145078594/job/130946627783).

The iOS 26.4.1 comparison passed once with the same executable, fixture and harness. The default PR job timed out installing the app. Its single setup retry then reproduced the touch-path loss on that same runtime. The hosted first-contact failure remains unresolved. The hosted check keeps the single Start press, fresh-process import, cancellation, restored state and staging assertions. For a recurrence, export the failed `.xcresult` diagnostics and correlate the bound player PID and scene with the Recap sender and BackBoard touch path. [Passing runtime comparison](https://github.com/miandbits/RaisingEngine/actions/runs/43148536836/job/130958409506) · [Default runtime failure](https://github.com/miandbits/RaisingEngine/actions/runs/43152899366/job/130976034411).
