# Cutout animation plugin — data contract (format `raising-cutout-v1`, schemaVersion 1)

A self-owned 2D cutout (bone + image part) animation format. It is **not** Spine
compatible and does not read Spine files. A future provider for another format would be
a separate plugin implementing the same runtime extension ABI (`raising-extension-1`).

Documents (engine envelope, see `packages/plugin-host/CONTRACT.md`):

* `plugins/cutout-animation/rig/<id>.json`: skeleton, parts, clips (`schema/rig.schema.json`)
* `plugins/cutout-animation/controller/<id>.json`: game signal → clip bindings (`schema/controller.schema.json`)

## Coordinates and transforms

* Rig space is the authoring canvas in pixels, y down, origin at canvas top-left.
  `canvas.originX/Y` is the instance anchor (usually between the feet).
* Bone local matrix = `T(x,y) · R(rotation°, clockwise on screen) · S(scaleX,scaleY)`;
  world = parent world · local. Root bones are in rig space.
* The bone origin **is** the rotation center (pivot). Moving a pivot in the editor moves
  the bone and compensates child bones/attachments so nothing jumps.
* An attachment places image point `(pivotX·width, pivotY·height)` at `(x,y)` in bone
  space with its own rotation/scale: `M = boneWorld · T(x,y)·R·S · T(-pivotX·w, -pivotY·h)`.
* `width/height` are the source PNG size; the service warns when the file differs
  (`ANIM_IMAGE_SIZE_MISMATCH`).
* `slots` array order is draw order (back → front). Hierarchy and draw order are independent.
* Editor-only visibility (eye icon) is never stored. Hiding a part in the game is
  `attachment: null` or `alpha: 0`, both keyable.

## Clips and keys

* Keys store **absolute** values (not offsets from the setup pose). Editing the setup pose
  never changes existing clips silently; the editor offers "apply setup change to keys".
* `interp` on a key governs the segment to the next key: `linear` (default), `step` or
  `bezier` (plugin 1.1.0). Attachment keys are always step. Before the first key the first
  value holds; after the last key the last value holds.
* `bezier` keys carry `curve: [x1, y1, x2, y2]`, the inner handles of a cubic Bezier from
  (0,0) to (1,1) in segment-normalized space: x = fraction of the segment's time, y =
  fraction of the value change. x must be in 0..1 so time never runs backwards (editing ops
  clamp it; evaluation clamps too), y in -4..5 so overshoot ("back") is allowed. Bad or
  missing handles are `ANIM_CURVE`; `bezier` on an attachment key is `ANIM_INTERP`; a
  `curve` on a non-bezier key is ignored (warning `ANIM_CURVE_UNUSED`).
* Evaluation: u = (t - t0) / (t1 - t0); solve x(s) = u for the Bezier parameter s (Newton,
  bisection fallback, |x(s) - u| < 1e-7, fixed iteration rules); value = v0 + (v1 - v0) · y(s).
  It depends only on clip time, so it is frame-rate independent, and the native and Wasm
  runtimes run the same code (`core/src/curve.cpp`). Handles are normalized, so moving a key
  or changing its value keeps the segment's shape.
* Presets are editing helpers; stored data is always the four numbers: `easeIn`
  [0.42,0,1,1], `easeOut` [0,0,0.58,1], `easeInOut` [0.42,0,0.58,1], `flat` [⅓,0,⅔,1],
  `back` [0.34,1.56,0.64,1] and `auto` (Catmull-Rom slopes from the neighbouring keys, flat at
  local extrema and track ends, Fritsch-Carlson limited so the segment never overshoots its
  keys; computed once when applied, not kept in sync with later key edits).
* Editing ops (`edit`): `setInterp {clip, target, property, timeMs, interp}` where interp is
  `step` | `linear` | `bezier` (+ `curve`) | a preset name (or `preset: <name>`);
  `setCurve {…, curve}` sets one key's handles; `setKey` accepts the same interp fields.
  `linear`/`step` remove `curve`. Many keys at once = one `batch` (one undo step).
* Key selections (plugin 1.1.0 editing ops; the data format is unchanged). A selection is
  `keys: [{target, property, timeMs}]` inside one `clip` (at most 5000). Every op checks all
  keys first and changes nothing when any is refused, so a dopesheet gesture is one edit:
  * `moveKeys {clip, keys, deltaMs, clamp?}`: every key moves by `deltaMs` (spacing kept).
    Refused when a key would leave `0..durationMs` or land on an unselected key of its track
    (selected keys may land on each other's old times). `clamp: true` first limits the delta
    so the group stops at the clip edge.
  * `scaleKeys {clip, keys, factor, pivotMs?}`: `t' = pivot + round((t - pivot) · factor)`,
    pivot defaults to the earliest key, `0 < factor ≤ 1000`. Refused when keys of one track
    would merge, collide with unselected keys or leave the clip.
  * Re-timing never touches `curve`: handles are normalized to their segment, so x stays in
    0..1 and each segment keeps its shape at the new timing.
  * `deleteKeys {clip, keys}` (empty tracks are removed).
  * `copyKeys {clip, keys}` changes nothing and returns `clipboard: {fromMs, keys: [{target,
    property, offsetMs, value, interp?, curve?}]}` (offsets from the earliest copied key;
    numeric entries always carry `interp`, attachment entries never).
  * `pasteKeys {clip, timeMs, keys, target?}` writes such entries at `timeMs + offsetMs`
    (any clip). Same-time keys are overwritten (value, and interp/curve when the entry has
    them). `target` re-targets a clipboard holding a single target (copy a row, paste it onto
    another bone or part); values are validated on the new target. Entries past the clip end
    refuse the whole paste.
  * `copyPose {clip?, timeMs, bones?}` returns `pose: [{bone, x, y, rotation, scaleX, scaleY}]`
    sampled exactly like the runtime (setup pose without `clip`; all bones without `bones`).
    `pastePose {clip, timeMs, pose}` keys it; a channel the clip does not animate and whose
    value equals the setup value is skipped (it already shows that value), and existing keys
    at that time keep their interpolation.
  * Re-timing and paste ops report the resulting keys as `keys` in the reply (`extra.keys`).
* Editor-only state is never stored in documents: time zoom/scroll, the key selection, the
  clipboards and the preview loop range (in/out markers; editor playback loops inside it,
  remembered per clip for the session). Runtimes always play whole clips.
* Compatibility: schemaVersion stays 1 and every 1.0.0 document loads and plays unchanged.
  Documents that use `bezier` need plugin 1.1.0 or newer (1.0.0 rejects the interp value).
* Rotation interpolates numerically (no shortest-path wrapping): 0→350 turns 350°.
* Times are integer milliseconds, strictly increasing per track, within `0..durationMs`.
* Events: `{timeMs, name}`. An event fires once per occurrence in the half-open interval
  `[previous, current)` of clip time; for one-shot clips an event exactly at the end fires
  with completion.

## Playback (per instance)

* Tracks 0..15 applied in ascending order; a higher track overrides only the properties
  its clip keys. Track 0 is the base action; controller `layers` (e.g. blink) run on 1..15.
* `play` replaces the track entry. If the mix duration (`mix.pairs` → `mix.defaultMs`, or
  explicit) is > 0, the old entry keeps animating and is cross-faded linearly; attachments
  switch at the halfway point. Events and completions of the outgoing entry are suppressed.
* Notifications: `start`, `interrupt` (replaced), `end` (no longer applied), `complete`
  (every loop boundary with loop index; once for one-shots), `event`. Each is emitted once
  per entry `serial`; hosts never need deduplication.
* One-shot `then`: `default` (track 0 → controller default clip; other tracks stop),
  `hold` (stay on the last frame), `stop` (mix out to lower tracks/setup).
* `update(dt)` clamps dt to 0..10 s; a long pause completes loops instead of replaying them.

## Controller bindings and game signals

Signals are derived **only from committed game transitions** (one call per command), never
from frames. Hosts call the core's `observe` op; all hosts use the same C++ derivation.

| Signal | Params | When |
|---|---|---|
| `actionStarted` | `actionId` | executeAction committed |
| `actionResult` | `actionId`, `outcome=success\|failure` | after executeAction (failure = rejected command) |
| `activityStarted` | `activityId` | a new `state.lastActivityResult` appeared |
| `activityResult` | `activityId`, `outcome=success\|failure\|done` | same transition (`done` = no success roll) |
| `dialogueLine` | `speakerId`, `nodeId` | narrative cursor moved to a dialogue node |
| `dialogueEnded` | `speakerId` | narrative left dialogue |
| `portraitChanged` | `characterId`, `portraitId`, `position` | a scene node changed a portrait (expression) |
| `phaseChanged` | `phase` | state phase changed |

If the controller sets `characterId`, every signal also gets `self=true|false`
(`speakerId` or `characterId` equals it). Binding fields: `signal`, `match` (all listed
params must be equal), `clip`, `track`, `loop`, `speed`, `then`, `stopOn` (signals that end
the clip early, e.g. `["dialogueEnded"]`). The first matching binding per track wins.

Game effects (costs, stats, RNG) are fully committed before any signal exists. Animation
completion only changes presentation (e.g. return to idle) and can never submit a command,
so it cannot duplicate a game effect.

## Save / load

`playerPresentation.extensions["cutout-animation"]` stores per mounted instance:
controller ref, active clip per track, binding, `then`, `stopOn`, speed.

* One-shot clips resume at their saved time.
* Looping (decorative) clips — idle, breathing, blink — restart at 0. Their phase is not
  saved; this is intentional and invisible to game state.
* Instances whose controller changed since the save are skipped and start at defaults.
* Missing plugin on load: the game loads normally; animation state is ignored.

## Templates

`breathing` (rest → peak → rest on a bone's `scaleY`/`y` with `easeInOut` segments, a
sine-like breath in three keys; default period 3200 ms) and `blink`
(attachment swap open → half → closed → half → open with a 3–5 s idle gap) generate
ordinary clips; they are editable data, not procedural runtime effects.
