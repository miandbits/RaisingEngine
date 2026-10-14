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

## Skins (plugin 1.2.0)

* `skins: [{id, name?, attachments: [<attachment>]}]` — outfit/variant sets. Skin attachments
  have exactly the fields of `attachments` entries (slot, name, imageAssetId, placement, size).
* Lookup of a shown name (slot setup attachment, attachment key, expression slot value):
  **active skin first, then the default `attachments`**. Clips keep keying attachment
  *names*, so changing the outfit needs no new animation (blink keys `closed`; the winter
  skin's `eyes/closed` image is drawn while it is active).
* A name is valid when the default list or *any* skin provides it (an outfit-only part such as
  a hat); `ANIM_ATTACHMENT_UNKNOWN` otherwise. A skin that provides neither the name nor has it
  in the default list draws nothing there while active: warning `ANIM_SKIN_MISSING_ATTACHMENT`
  (pointer `/skins/<i>`). Per skin: unknown slot `ANIM_SLOT_MISSING`, duplicate (slot, name)
  inside one skin `ANIM_DUP_ID` (the same pair in two skins is the point), no image
  `ANIM_IMAGE_MISSING`, size `ANIM_IMAGE_SIZE`; the service checks skin images like default
  ones (`ANIM_IMAGE_MISSING` for unknown assets, `ANIM_IMAGE_SIZE_MISMATCH` against the PNG).
* The active skin of an instance: controller `defaultSkin`, overridden by the placed element's
  `props.skin` (per instance; the host passes props through untouched), then by bindings
  (`skin`) and `setSkin`. `""` = default attachments only. An unknown `props.skin` is ignored
  by the runtime (controller default stays) and reported by the service (`ANIM_SKIN_UNKNOWN`,
  warning). Re-mounting the same element keeps the instance's skin; only a changed
  `props.skin` is applied again.

## Expressions (plugin 1.2.0)

* `expressions: [{id, name?, slots?: {"<slot>": "<attachmentName>" | null}, bones?: {"<bone>":
  {x?, y?, rotation?, scaleX?, scaleY?}}}]` — named facial/pose sets. Slot values replace the
  shown attachment (`null` hides the slot); bone channels are **absolute** values like keys;
  unlisted slots and channels keep animating.
* **Layer order** (the rule every runtime and the editor use): track 0 → *expression* →
  tracks 1..15. The expression is a base-layer override, so it wins over the base action
  (an action keying the head cannot undo a smile's head tilt) while layer tracks still win over
  it: a blink layer on track 1 keying the eyes keeps blinking. Trade-off: a layer that keys a
  slot the expression also sets shows the layer's value; put expression-specific eye shapes
  into the blink clip's own names, or let the expression leave the eyes out.
* Which expression is active: the instance expression (controller `defaultExpression`,
  bindings `expression`, `setExpression`), overridden while a playing clip keys the rig
  expression track. The highest track whose current clip keys it wins; during a cross-fade the
  keyed value switches at the halfway point like attachments.
* Rig expression track: `{"target": "rig", "property": "expression", "keys": [{timeMs, value:
  "<id>" | null}]}` — step only (`bezier` → `ANIM_INTERP`; other interp values are ignored),
  `null` = no expression from that key on (it also overrides the instance expression), before
  the first key the first value holds. `rig` tracks only key `expression` and `expression` is
  only keyed on `rig` (`ANIM_PROPERTY`); unknown ids are `ANIM_EXPRESSION_UNKNOWN`.
* Validation: unknown slot `ANIM_SLOT_MISSING`, unknown bone `ANIM_BONE_MISSING`, unknown
  attachment name `ANIM_ATTACHMENT_UNKNOWN`, channels other than x/y/rotation/scaleX/scaleY
  `ANIM_PROPERTY`, duplicate ids `ANIM_DUP_ID`.
* `samplePose(rig, clip, t, look)` samples one clip as the base track; `look.expression` (or
  the clip's own expression keys) applies after it, `look.skin` is carried in `Pose.skin` for
  drawing. `look.expressions = false` gives the raw keyable values (the editor's properties,
  K and drags use them so a previewed expression never gets baked into keys).

## IK constraints (plugin 1.3.0)

* `ik: [{id, name?, bones, target, mix?, bendPositive?, stretch?, order?}]` (optional; absent =
  no constraints). `bones` is the chain, parent first:
  * **1 bone = aim**: the bone is rotated so its local **+x axis** points at the target (a
    negative `scaleX` flips that axis, like the drawing does).
  * **2 bones = two-bone limb** `[upper, lower]`, `lower` a **direct child** of `upper`. Bones
    have no length in this format, so the **effector** is the origin of `lower`'s **first child
    bone** in `bones` array order (hand, foot). Without a child the limb degenerates to aiming
    `lower`'s origin at the target. `upper` and `lower` are rotated (law of cosines).
  * `target`: the bone whose **world origin** the effector reaches. It must not be a chain bone
    or a descendant of one (solving would move it).
  * `mix` 0..1 (default 1): `rotation = animated + shortestDelta(animated → solved) · mix`, so
    0 = forward kinematics, 1 = fully solved, 0.5 = halfway (the short way round, ±180°).
  * `bendPositive` (default true): the side a two-bone limb bends to **on screen** —
    `cross(lower − upper, effector − lower) > 0`, i.e. the lower link turns clockwise from the
    upper link on a y-down screen. A mirrored parent space keeps the on-screen side.
  * `order` integer −1000..1000 (default 0): constraints are solved in ascending `order`, ties
    in array order. `stretch` is accepted and **not applied** in 1.3.0 (warning
    `ANIM_IK_STRETCH`).
* **Order of operations** (all runtimes, `samplePose`, onion skin, trail and drawing): tracks
  0..15 (with the expression layer between track 0 and 1) → world transforms → **IK
  constraints in order**, each reading the current world of its target and chain parent and
  recomputing the world of its chain and every descendant → draw list. A constraint solved
  later sees the result of earlier ones (a spine solved before an arm moves the shoulder).
* Solving happens in the parent space of the chain's first bone, where its local rotation acts
  rigidly: the result is **exact for any ancestor transform** (rotation, non-uniform scale,
  skew, mirror). A uniformly scaled or mirrored upper bone and any scale of the lower bone are
  exact too; an upper bone with non-uniform scale uses the geometric mean for the lower link
  (approximate, as in Spine). Unreachable targets clamp: too far = the limb points straight at
  the target, too close = folded. Zero distances/links leave the pose unchanged (no NaN).
  Results depend only on clip time and data, so they are frame-rate independent and the
  native and Wasm runtimes run the same code (`core/src/runtime.cpp`, `applyIk`).
* `Pose.bones` stay the **animated (keyable) values**; `Pose.world` includes the constraints and
  `Pose.applied` holds the solved local values. The editor's properties, K and drags use the
  animated values (a constraint never bakes itself into keys); `bakeIk` does that on request.
  The host `sample` reply adds `ik: {<id>: {mix, bendPositive}}` and `appliedRotation` per bone
  for rigs that have constraints (other rigs keep the 1.2 reply shape).
* Keys: track `{"target": "ik:<id>", "property": "mix" | "bendPositive"}`. `mix` keys are
  numeric 0..1 with every interp (curves too); `bendPositive` keys are booleans and always step
  (`bezier` → `ANIM_INTERP`, a non-boolean value → `ANIM_KEY_VALUE`). `ik:` tracks only key
  these two properties and they are only keyed there (`ANIM_PROPERTY`); an unknown id is
  `ANIM_TARGET_MISSING`. Cross-fades lerp `mix` and switch `bendPositive` halfway.
* Validation: `ANIM_IK_DUP_ID`, `ANIM_IK_BONE_MISSING` (chain or target), `ANIM_IK_CHAIN` (not
  1–2 bones, or not a direct parent → child pair), `ANIM_IK_TARGET` (target in or below the
  chain), `ANIM_IK_MIX` (setup or key out of 0..1), `ANIM_IK_CYCLE` (constraints that move each
  other's target or chain base: no order can satisfy them; a constraint moves every strict
  descendant of its chain bones), warning `ANIM_IK_ORDER` (a dependent constraint is solved
  first; the order is honoured anyway), `ANIM_STRUCTURE` for wrong field types.
* Editing ops (`edit`; each is one undo step in the editor):
  * `addIk {id?, name?, bones, target? | createTarget: true (+ targetId?, targetParent?), mix?,
    bendPositive?, order?}`: `createTarget` adds a bone where the chain end is shown now (two
    bones: the effector; aim: on the bone's x axis at the first child's distance or 60 px),
    under the chain's top-level ancestor; without `bendPositive` the current bend side is kept,
    so creating a constraint never moves the limb. `extra.ik`, `extra.target`.
  * `setIk {ik, name?, mix?, bendPositive?, order? (null = default), target?, bones?}`,
    `renameIk {ik, id?, name?}` (renames `ik:<id>` tracks; `extra.tracks`),
    `deleteIk {ik, deleteTarget?}` (removes its tracks; `deleteTarget` also deletes the target
    bone when no other constraint or part uses it), `bakeIk {clip, timeMs, ik?, disable?}` (keys
    the solved rotation of every chain bone at `timeMs`; `disable` also keys `mix` 0 there).
  * Hierarchy edits keep references consistent: `reparentBone` is refused when it would break a
    constraint (chain no longer adjacent, target moved into a chain); `deleteBone` of a chain or
    target bone needs `cascade` and then removes those constraints and their tracks (warning
    `ANIM_IK_REMOVED`); `setPivot` keeps ids (the chain simply solves around the new origin).
  * Deleting the last constraint removes the `ik` list (documents keep the 1.2 shape).
* Editor-only: the constraint shown in the IK page, the target crosshair and the highlighted
  chain on the canvas never reach documents.

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
  clipboards, the previewed skin/expression of the canvas (스킨 · 표정 lists) and the preview loop range (in/out markers; editor playback loops inside it,
  remembered per clip for the session). Runtimes always play whole clips.
* The authoring layout is editor-only too: the 애니메이션 작업 공간 (host workspace) switch,
  splitter sizes and collapsed sections (QSettings `plugins/cutout-animation/layout/*`) and
  the Tab full view never send an edit and never reach documents or the game.
* Onion skin and motion trail are editor-only views; their settings are a per-user
  preference (QSettings `plugins/cutout-animation/onionSkin/*`), never rig/controller data
  (not even an `editor` block). Ghosts are pure `samplePose` results of the clip being edited
  at `playhead ± k·stepMs` (step mode) or at the nearest key times of the selected target
  (keys-only mode; nothing keyed there = all visible tracks): looping clips wrap like the
  runtime (`t mod duration`), one-shots clamp to `[0, duration]` and repeated boundary times
  collapse into one ghost. They never fire clip events, touch preview instances or edit.
  The controller preview (bindings, mixes, layers) is runtime state without a fixed time, so
  no ghosts are drawn while the canvas shows it. "Include current" ghosts the saved
  (committed) pose at the playhead, which differs from the live pose only while a change is
  pending or a drag is in progress. The trail samples the selected bone's world origin over
  `[0, duration]` (end inclusive, editor scrubbing rule); dots mark keys of the bone and its
  ancestors.
* Compatibility: schemaVersion stays 1 and every 1.0.0 document loads and plays unchanged.
  Documents that use `bezier` need plugin 1.1.0 or newer (1.0.0 rejects the interp value).
  Documents with `skins`, `expressions`, rig expression tracks or the controller fields
  `defaultSkin`/`defaultExpression`/binding `skin`/`expression` need plugin 1.2.0; documents
  without them are unchanged (editing ops never add empty `skins`/`expressions` lists, and
  deleting the last skin or expression removes the list). Documents with `ik` or `ik:<id>` tracks
  need plugin 1.3.0; without them they are unchanged.
* Skin/expression editing ops (`edit`, plugin 1.2.0; each is one undo step in the editor):
  * `addSkin {id, name?, from?}` (`from` copies another skin's attachments),
    `duplicateSkin {skin, id, name?}`, `renameSkin {skin, id?, name?}` and
    `deleteSkin {skin, force?}`. Rename returns rewritten controllers (`controllers`,
    `controllerRefs`: `defaultSkin` and binding `skin`) like `renameClip`; delete is refused
    while a controller uses the skin unless `force`, which also drops those refs (a binding
    left with no clip/skin/expression is removed).
  * `setSkinAttachment {skin, slot, name?, imageAssetId, width, height, x?, y?, rotation?,
    scaleX?, scaleY?, pivotX?, pivotY?}` adds or updates one skin attachment ("import an image
    into this skin for a part"). `name` defaults to the slot's setup attachment; a new entry
    copies the placement of the default attachment with that name (or the part's shown one),
    so the outfit image lands where the base image was. `removeSkinAttachment {skin, slot, name}`.
  * `addExpression {id, name?, slots?, bones?}`, `renameExpression {expression, id?, name?}`
    (also rewrites rig expression keys; `keys` = count), `deleteExpression {expression, force?}`
    (refused while keyed or used by a controller unless `force`, which removes those keys and
    refs), `setExpressionSlot {expression, slot, attachment: name|null}` or `{…, remove: true}`,
    `setExpressionBone {expression, bone, x?..scaleY?}` (`null` drops a channel) or `{…, remove: true}`.
  * `captureExpression {id, name?, clip?, timeMs?, slots?, bones?, from?, replace?}` ("현재
    상태로 표정 만들기"): samples a pose (`from` rig data or this rig, at `clip`/`timeMs` or the
    setup pose, expressions off) and stores what differs from the baseline — the setup pose,
    or with `from` the same clip time of this rig, so only the editor's unsaved changes become
    the expression. Listed `slots`/`bones` are captured whole (bones: all five channels).
    Nothing different → refused. `replace: true` overwrites an existing id (keeps its name).
    `extra.expression` is the stored entry.
  * Expression keys use the ordinary key ops with `target: "rig", property: "expression"`
    (`setKey` value id|null, `moveKeys`, `copyKeys`/`pasteKeys` without interp, `deleteKeys`);
    `setInterp`/`setCurve` on them are refused.
  * Deleting a slot or bone also removes its skin attachments and expression entries.
* Rotation interpolates numerically (no shortest-path wrapping): 0→350 turns 350°.
* Times are integer milliseconds, strictly increasing per track, within `0..durationMs`.
* Events: `{timeMs, name}`. An event fires once per occurrence in the half-open interval
  `[previous, current)` of clip time; for one-shot clips an event exactly at the end fires
  with completion.

## Playback (per instance)

* Tracks 0..15 applied in ascending order; a higher track overrides only the properties
  its clip keys. Track 0 is the base action; controller `layers` (e.g. blink) run on 1..15.
  The active expression (plugin 1.2.0) applies between track 0 and track 1; the instance skin
  resolves every attachment name at draw time; IK constraints (plugin 1.3.0) are solved on the
  world transforms after all tracks.
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

Skins and expressions (plugin 1.2.0): controller `defaultSkin` (skin id) and
`defaultExpression` (expression id or null) set the look of new instances. A binding may carry
`skin` and/or `expression` (id, or `null` = no expression); `clip` is then optional. On a
match the skin/expression is set; it persists until something changes it (a clip ending does
not revert it). Skin/expression-only bindings never take a track, and the first matching
binding that names a skin (an expression) sets it. Unknown ids are `ANIM_SKIN_UNKNOWN` /
`ANIM_EXPRESSION_UNKNOWN`; a binding with none of clip/skin/expression is `ANIM_CLIP_UNKNOWN`.
The signal vocabulary is unchanged. `portraitChanged` is the natural expression signal, e.g.:

```json
{"id": "face-smile", "signal": "portraitChanged", "match": {"portraitId": "smile", "self": "true"}, "expression": "smile"},
{"id": "face-reset", "signal": "dialogueEnded", "match": {"self": "true"}, "expression": null},
{"id": "dress-festival", "signal": "phaseChanged", "match": {"phase": "festival"}, "skin": "festival"}
```

Runtime ops (generic extension protocol additions, plugin 1.2.0): `setSkin {instance, skin}`
(`""`/null = default attachments) and `setExpression {instance, expression: id|null}`, both
refused (`ok:false`) for unknown ids; `sample` accepts `skin`/`expression` and its pose reports
`skin`/`expression`; `describe` reports `skin`, `expression` (instance) and
`activeExpression` (after clip keys). The Wasm runtime is built with exception catching, so
refused ops answer `{ok:false, error}` there exactly like the native DLL. No new notifications.

Game effects (costs, stats, RNG) are fully committed before any signal exists. Animation
completion only changes presentation (e.g. return to idle) and can never submit a command,
so it cannot duplicate a game effect.

## Save / load

`playerPresentation.extensions["cutout-animation"]` stores per mounted instance:
controller ref, active clip per track, binding, `then`, `stopOn`, speed, and (plugin 1.2.0)
`skin` and `expression`. IK adds no state: constraint values come from the rig and the clips.

* Policy: skin and expression are both restored. An id removed since the save falls back to
  the controller default; saves without these fields (1.x) keep the current look.

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
