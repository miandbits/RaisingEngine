# One UIKit contact reference

This isolated diagnostic uses a standard system UIButton, normal superclass touch handling, and its ordinary touchUpInside target. It contains no engine, SDL, game content, gesture recognizer, custom hit test, or synthesized application command.

Run once on the standard macos-26 hosted runner:

```sh
DEVELOPER_DIR=/Applications/Xcode_26.6.app/Contents/Developer \
python3 .github/diagnostics/ios-uikit-first-contact/run.py \
  --out .cache/uikit-contact-reference \
  --runtime com.apple.CoreSimulator.SimRuntime.iOS-26-5 \
  --device-type com.apple.CoreSimulator.SimDeviceType.iPhone-17e
```

The runner compiles an independent app and XCTest harness, creates one owned Simulator, waits for bootstatus, installs and launches once. XCTest checks the bound PID and zero counters, then uses one public coordinate press at (191, 572.5000305) for 0.20 seconds. It observes a terminal phase for at most ten seconds. There are no gesture retries, warmup contacts, or fixed waits.

Input callbacks increment in-memory counts and always forward to UIButton. XCTest reads counters through accessibility outside the callbacks. The required result is began=1, ended=1, cancelled=0, activations=1. A cancellation remains a failed XCTest and process exit. `completed` means a final observation exists; it does not mean the contact passed. `productAcceptance` is always false.

Capture all `.cache/uikit-contact-reference/run-*/result.json`, `.log`, `.stdout`, `.stderr`, `.xcresult`, and `fixture-source` files with upload-artifact `if: always()`. Preserve source files, project, and checksums with the run. `simulator-contact.log` is collected only after the test and before owned-device cleanup. If collection fails or times out it is independently reported; missing logs are not proof of missing input.

A down/cancel with zero activations and matching BackBoard missing-path/detach cancellation would reproduce the transport failure without the engine. A valid down/up activation is only one positive control and cannot disprove an intermittent failure. The reference differs from the engine app in its UI and preparation timing; no physical-device or product acceptance is claimed.
