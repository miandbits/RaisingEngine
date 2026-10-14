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
* `interp` on a key governs the segment to the next key: `linear` (default) or `step`.
  Attachment keys are always step. Before the first key the first value holds; after the
  last key the last value holds.
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

`breathing` (sine-like keys on a bone's `scaleY`/`y`, default period 3200 ms) and `blink`
(attachment swap open → half → closed → half → open with a 3–5 s idle gap) generate
ordinary clips; they are editable data, not procedural runtime effects.
