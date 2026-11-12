# Fixed vendor-isolation diagnostic

This isolated source is not a release fix or replacement for product acceptance.
Two preset triplets run once: U→S→E and E→S→U. Every arm receives a fresh owned
Simulator. There are six common first-Start contacts: original XCTest app-origin
coordinate input at (191,572.5000305), one explicit 0.20s press. E additionally
runs the unchanged complete Files cases twice, including original fresh Load
`target.tap()`. The historical 0.05s scheduled Load offset is context only, not an explicit
duration argument or a qualification threshold for that API. All attempts,
including setup and observation failures, are retained. No retry/expansion.

U is an ordinary UIKit UIButton with superclass handling. S uses the same built
SDL3.4.18 static library and same video/gamepad, orientation, indirect-input,
window, renderer-selection and vsync configuration as E, without RmlUi/game data.
S's rectangle activation state machine is a diagnostic sample, not the engine's
button implementation. U's real-button AX readiness queries differ from S's
bound ready receipt and E's authored-layout diagnostics; this is dependency
isolation, not a single-variable timing comparison. No idb, host input, GUI
preparation, runtime change, warmup or semantic activation is used.

## Passive observation limits

S/E share `simulator-input-proof.hpp` byte-for-byte. It records consumed SDL
queue order, original event timestamps, app SDL_GetTicksNS capture timestamps,
window-event type/data1/data2, and direct touch identity. The per-touch
SDL_GetTouchDeviceType lookup is an identical extra read in both arms; conversion
values are copied from each app's existing conversion. Fixed buffers are 64 input
samples and 512 relevant events, with explicit overflow. No allocation/output,
extra hit test, forced layout, focus request or event synthesis occurs there.

Initial geometry is sampled in the first existing frame after logical dimensions
are set, before E's context Update; it is not a stability/readiness assertion.
Final geometry is sampled after the batch containing the first observed terminal
(touch UP/CANCELED or mouse UP), which may be invalid/foreign. The proof validator
still requires exactly one real DIRECT matching pair. Missing terminal after a
first observed DOWN yields an explicitly incomplete record after 10s in the normal
loop, without adding a timer or delay. If no relevant DOWN/terminal is observed,
there may be no emitted record: that is incomplete observation, not proof that no
upstream events occurred. Transient UIKit callbacks without consumed SDL events,
pre-renderer generic→Metal view replacement, and a stalled event loop are not
excluded. Window-event payload meaning is event-specific; safe-area/pixel/scale
changes cannot be reconstructed from event numbers alone.

U has separate 256-entry primitive storage in real UIKit touch/layout/lifecycle
callbacks. It forwards all superclass behavior and serializes only during the
reference's existing accessibility reads. Touch time is UITouch.timestamp;
capture/layout time is ProcessInfo.systemUptime. It does not observe SDL, and its
callback coverage differs from S/E. No new animation/frame producer is installed.

E checks its native proof in tearDown, after all original test assertions and
waits. It cannot extend/rescue Start, Files, cancel, restoration or cleanup
acceptance. Failed original tests remain failed even when an input proof exists.
Observed geometry changes are retained, not forbidden as an added readiness gate.

## Interpretation

A matching target-specific BackBoard missing-path/active-path cancellation in U
shows SDL/full engine is unnecessary for that occurrence. The same signature in
S shows full engine/RmlUi unnecessary; a positive U does not establish SDL cause.
Only E failing does not prove an engine cause given intermittency. Valid direct
SDL delivery with failed downstream activation points to a different layer and
must be investigated as such. All-positive, missing evidence or setup failure
is inconclusive. Duplicate contact-state objects alone are not the failure
signature, and no result here establishes an Apple sender repair API.

Preserve source/library/app hashes, real synthesized XCTest events, full xcresult,
PID/nonce/window identity, lifecycle/geometry receipts and scoped OS evidence.
Capture or parse failures remain explicit. Stop and clean up only owned devices;
never touch existing devices or the public Windows release.

E diagnostic qualification never throws from tearDown: original functional Files exit is preserved and fresh import proceeds when export originally passes. Missing or rejected proof is separately incomplete. Common Start requires measured 0.20 ±0.02 seconds. Fresh Load is unchanged `target.tap()`; actual serialized offsets and guest duration are reported without a nominal 0.05 gate.

E records its authored first-contact expectation after the original case. Start must match the U/S common point within 0.0001 point for diagnostic qualification; a different layout does not alter its physical press or original functional verdict.
