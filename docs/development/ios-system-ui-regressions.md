# Actual iOS system UI regressions

The optional macOS runner exercises two UIKit/SDL boundaries through XCTest input. It opens the real Files picker, uses its search keyboard, backgrounds the app, rotates it, and cancels. It also types into a real authored game input across portrait and landscape, dismisses the keyboard, and starts the game with the exact entered value.

Use Xcode with an installed iOS Simulator runtime. Build the project service and a simulator iOS export first. Supply the same source project that produced that export:

```sh
node scripts/test-ios-system-ui.mjs /absolute/path/to/ios-export \
  --project /absolute/path/to/project --family iphone
```

Use `--family ipad` for the tablet case. The export must contain `build-iphonesimulator/native-player/Release-iphonesimulator/RaisingPlayer.app`. The input project must have an authored `title` screen; the public “My first raising game” starter is the intended fixture source. The runner verifies the project fingerprint against the supplied app before making a cache-only copy.

The runner never changes the original export or source project. In the copied app it replaces the title with a small text-input probe and adds a readable record modal, while retaining the native executable byte for byte. A new uniquely named Simulator and app identifier isolate its records and settings. The test invokes ordinary game controls and real system UI; a localhost endpoint only returns the player's `--editor-events` diagnostics. It does not inject picker results or dispatch game commands.

Evidence is written under `.cache/ios-system-ui/run-*`: the result receipt, source and fixture fingerprints, native executable hash, harness hash, authored probe, logs, and XCTest result bundles with screenshots and accessibility transcripts. `.cache/ios-system-ui/latest.json` points to the latest result. The runner shuts down and deletes only the Simulator it created. Use `--keep-failed-device` to retain that isolated device for diagnosis after a failure. Each external build or test process has a ten-minute limit; XCTest may spend part of that time waiting for UIKit animation notifications. A timeout remains a failed, incomplete run.

A pass covers these two focused Simulator regressions. It does not establish full game usability, save import/export correctness, cloud-provider behavior, physical-device behavior, signing, or App Store readiness. The broader portable-save acceptance matrix remains separate. A failure must preserve the actual screenshot and system hierarchy so an observed OS presentation delay or test-targeting problem is not mislabeled as a game defect.
