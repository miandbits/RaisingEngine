# Cutout animation plugin — data contract (format `raising-cutout-v1`, schemaVersion 1)

A self-owned 2D cutout (bone + image part) animation format. It is **not** Spine
compatible and does not read Spine files. A future provider for another format would be
a separate plugin implementing the same runtime extension ABI (`raising-extension-1`).

Documents (engine envelope, see `packages/plugin-host/CONTRACT.md`):

* `plugins/cutout-animation/rig/<id>.json`: skeleton, parts, clips (`schema/rig.schema.json`)
* `plugins/cutout-animation/controller/<id>.json`: game signal → clip bindings (`schema/controller.schema.json`)
* `plugins/cutout-animation/settings/project.json`: export settings (plugin 1.9.0, `schema/settings.schema.json`)

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

## Bone length (plugin 1.5.0)

* A bone may carry `length` (px, number 0..100000, default/absent **0 = point bone**): the bone
  reaches from its origin to `(length, 0)` in its own space, i.e. along its local **+x** after its
  rotation and scale (Spine convention). Length is **not a transform**: it never moves the bone,
  its children or its parts, so documents without lengths and documents with them play
  identically except for the two-bone IK effector below. Validation `ANIM_BONE_LENGTH` (not a
  finite number in range); the schema rejects the same values structurally.
* `color` (optional, `"#rrggbb"` or `"#rrggbbaa"`, `ANIM_BONE_COLOR` otherwise) is an
  **editor-only display hint** stored on the bone (like Spine's bone colour, so it travels with the
  bone through reparenting, copy and MCP edits); runtimes ignore it. The rig `editor` block stays
  free for view metadata.
* IK with lengths: an aim chain still aims its `aimAxis` (default +x = the length direction), so a
  bone with a length aims its tip at the target without any axis. Two-bone effector
  **precedence**: explicit `tip` bone origin > the lower bone's own tip `(length, 0)` when its
  length > 0 > its first child bone (array order) > its origin. All older documents have no
  lengths and keep the 1.4 effector bit for bit.
* Editor: bones with a length are drawn as a tapered diamond from origin to tip in their colour
  (selected / hover states), point bones as the origin circle with an x-axis tick. Canvas picking:
  mesh vertices > IK target crosshair > selected bone (origin, a point bone's axis) > bone origins
  > **bone bodies** (inside the diamond or within 4 px of the origin→tip line) > parts; Alt+click
  walks the candidates. In the Move (V) and Rotate (R) tools a press on a body turns the bone
  around its origin and a press on its origin moves it. The **create tool** (C, setup mode only)
  is one `addBone` per gesture: parent = the selected bone (a part's bone), else the first
  top-level bone; `x`/`y` = the press point and `rotation`/`length` = the drag vector, both
  expressed in the parent's shown world transform (the tip lands under the cursor); Shift snaps
  the on-screen angle to 15°; a click without a drag makes a point bone.
* Editing ops (`edit`): `addBone`/`setBone` take `length` (null/0 drops the key) and `color`
  (null/"" drops it). Setting a length on the lower bone of a two-bone chain that has no `tip`
  pins its first child as `tip`, so no gesture moves a limb by surprise.
  * `fitBoneLengths {bones?}` ("자동 길이"): length = the distance from the origin to the first
    child bone, in the bone's own space (all bones when `bones` is absent; childless bones keep
    theirs, a listed one warns `ANIM_BONE_NO_CHILD`; nothing to change = refused). `extra.lengths`.
  * `orientBones {bones?, fitLength? = true}` ("본 방향 맞추기", migration of point-bone rigs):
    turns each point bone's frame by δ so +x points along its natural direction (first child bone,
    else the area-weighted centre of its slots' setup images, as `addIk` reads it) and sets the
    length to that reach. Everything expressed in the bone's frame is rewritten so **every pose
    draws exactly as before** (setup, every clip, layered tracks, cross-fades, skins,
    expressions, IK, weighted meshes, deform keys): the bone's rotation (setup, keys, expression
    overrides) + δ; each child's position R(−δ)·p and rotation − δ (setup, keys, expressions);
    attachment offsets of its slots in the default list and every skin R(−δ) and rotation − δ;
    aim axes − δ (an `aimAxis: 90` arm ends at +x and loses the field). Child x/y keys rotate as
    pairs; a channel keyed alone gets its partner keyed at the same times from setup, which is
    only exact while no clip or expression keys that partner — otherwise the parent is skipped.
    Also skipped (`extra.skipped {id: reason}`, warning `ANIM_ORIENT_SKIPPED` for listed bones):
    bones that already have a length, nothing to point at, a scale that is not uniform at every
    time (scaleX ≠ scaleY anywhere, e.g. a breathing body, or mirrored), and child keys that cannot
    pair. A lower bone of a two-bone chain without `tip` and without a child keeps length 0
    (its effector would move). Turned values are rounded to 1e-9 (not 1e-6) so draw lists stay
    within 1e-6 px. `extra.oriented {id: δ°}`.
  * `addIk` prefers lengths: an aim chain on a bone with a length stores no `aimAxis` and its
    created target sits at the tip; a two-bone chain whose lower bone has a length stores no
    implicit `tip` (the length tip is the effector) and its created target sits at that tip.
* Editor-only visibility (eye icon) is never stored. Hiding a part in the game is
  `attachment: null` or `alpha: 0`, both keyable.

## Multi-bone selection, compensation and axis spaces (plugin 1.7.0)

* No data change: selection, pivot mode, axis mode, relative input and the compensation toggles
  are editor state (never stored). 1.7.0 adds edit ops and op flags only; every document plays
  as before.
* `transformBones {bones, delta: {x?, y?, rotation?, scaleX?, scaleY?}, space? = "world",
  pivot? = "origin", clip?, timeMs?, compensateBones?, compensateImages?}` — one canvas gesture on
  a bone selection (one bone or many; the LLM `edit` op gets the same op):
  * `x`/`y`: a translation in rig px (`space: "world"`) or added to the local x/y (`"parent"`).
    Only the **topmost** selected bones move (a selected descendant already follows its parent).
  * `rotation` (degrees, clockwise on screen) and `scaleX`/`scaleY` (factors) about the pivot.
    `pivot: "origin"`: every selected bone turns/scales about its own origin (local rotation
    += δ, flipped under a mirrored parent; local scale ×= f), so a selected chain curls (Spine).
    `pivot: "centroid"` (mean of the selected bones' origins) or `{x, y}` (rig px): the topmost
    selected bones turn as one piece — their origins rotate/scale about the pivot in rig axes,
    rotation += δ, scale ×= f; selected descendants follow unchanged.
  * Values come from the pose the editor shows (forward kinematics + IK, no expressions). With
    `clip` + `timeMs` the result is keyed (x+y, rotation, scaleX+scaleY: the channel groups that
    change; existing keys keep their interpolation), otherwise the setup pose changes.
    `extra.bones` lists the bones that changed. Refused: no/unknown bones, empty delta, zero scale
    factor, unknown `pivot`/`space`, key time out of range.
* **Compensation** (Spine "본 보정 / 이미지 보정", **setup pose only**): `compensateBones` /
  `compensateImages` on `setBone` and `transformBones`. After the bones change, every direct
  child bone that is not itself selected gets `local = newParentWorld⁻¹ · oldWorld` (so it and
  its whole subtree stay put), and every attachment of the changed bones' slots — default list
  and every skin — is re-placed the same way (offset x/y/rotation/scale). Values are rounded to
  1e-9 so world poses stay within 1e-6. Keys and expression overrides are **not** rewritten
  (they are absolute values in the old frame: warning `ANIM_COMPENSATE_KEYS`); non-uniform scale
  can only be approximated (decomposition drops skew: `ANIM_COMPENSATE_SKEW`). With `clip` the
  flags are ignored with warning `ANIM_COMPENSATE_SETUP_ONLY` (Spine compensates setup only; an
  animator moving a parent wants the children to follow).
* **Axis spaces** (`cutout::toAxes` / `fromAxes`; `setBone {axes: "local" | "parent" |
  "world"}`): values are always stored local; `axes` only says in which space `x`/`y`/`rotation`
  are given (channels left out keep their shown value; scale is always local).
  * local: as stored (x/y in the parent's space, rotation relative to the parent).
  * world: x/y = the origin in rig px, rotation = the on-screen angle of the bone's +x axis
    (`parentWorld · R(rotation) · (1, 0)`), kept within ±180° of the parent angle + local value.
  * parent: x/y = the origin's offset from the parent origin along the parent's **unscaled**
    axes (px), rotation = the angle from the parent's +x axis — equal to local under an unscaled
    parent. `setBone` converts with the setup world as it is at that op, so a `batch` processing
    parents first sees each moved parent. Singular parent = refused.
* Editor: Ctrl/Shift+click (canvas, hierarchy, dopesheet row) toggles items; with the Move tool a
  drag on empty space is a box selection of bone origins (Ctrl/Shift adds; a click on empty space
  clears; right/middle drag still pans); Ctrl+A (canvas or hierarchy) = all bones, Ctrl+Shift+A
  (hierarchy only: on the canvas it stays the host's workspace toggle) = add every descendant of
  the selected bones; both are also in the `선택 ▾` menu. Pressing a member of a multi-selection drags all
  of it (`transformBones`), a plain click on it keeps only it. The numeric fields edit every
  selected bone (one batch of `setBone` parents first, or `setKey` in Animate), show `—` where
  values differ, and either set (absolute) or add (`상대값 입력`) the typed value. `K` keys every
  selected bone (5 channels) and part (attachment); pose copy takes exactly the selected bones.

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

* `ik: [{id, name?, bones, target, tip?, mix?, aimAxis?, bendPositive?, stretch?, order?}]` (optional;
  absent = no constraints). `bones` is the chain, parent first:
  * **1 bone = aim**: the bone is rotated so its bone-local direction **`aimAxis`** (plugin
    1.4.1; degrees, −360..360, default **0 = +x**, 90 = +y, i.e. down a y-down screen) points at
    the target. The axis is measured after the bone's own scale (`atan2(scaleY·sin a,
    scaleX·cos a)`): a negative `scaleX` flips +x like the drawing does, a non-uniform scale
    tilts an oblique axis exactly as the image tilts. Cutout limbs usually keep rotation 0 while
    their image hangs down: such an arm aims with `aimAxis: 90`. Absent and 0 are identical
    (bit for bit the 1.3.0 solve), so older documents play unchanged.
  * **2 bones = two-bone limb** `[upper, lower]`, `lower` a **direct child** of `upper`. The
    **effector** is the origin of the `tip` bone (plugin 1.4.0, optional: must be a **direct
    child of `lower`**, `ANIM_IK_TIP` otherwise, also on an aim chain), else `lower`'s own tip
    `(length, 0)` when it has a length (plugin 1.5.0, see Bone length), else the origin of
    `lower`'s **first child bone** in `bones` array order (hand, foot). The
    explicit tip keeps a forearm with several children (hand, sleeve, weapon) pointing at the
    right one. Without a child the limb degenerates to aiming `lower`'s origin at the target.
    `upper` and `lower` are rotated (law of cosines). Two-bone chains solve from these bone
    **positions** only; `aimAxis` is an error there (`ANIM_IK_AIM_AXIS`) — to make an image lie
    along the limb, place the child bone (wrist) where the image ends.
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
  chain), `ANIM_IK_TIP` (tip unknown, not a direct child of the lower bone, or on an aim chain), `ANIM_IK_AIM_AXIS`
  (not a number in −360..360, or on a two-bone chain), `ANIM_IK_MIX` (setup or key out of 0..1), `ANIM_IK_CYCLE` (constraints that move each
  other's target or chain base: no order can satisfy them; a constraint moves every strict
  descendant of its chain bones), warning `ANIM_IK_ORDER` (a dependent constraint is solved
  first; the order is honoured anyway), `ANIM_STRUCTURE` for wrong field types.
* Editing ops (`edit`; each is one undo step in the editor):
  * `addIk {id?, name?, bones, target? | createTarget: true (+ targetId?, targetParent?), mix?,
    bendPositive?, order?, aimAxis?}`: an aim chain without `aimAxis` gets the bone's natural
    direction (1.4.1): toward its first child bone (array order) when that is off the origin,
    else toward the area-weighted centre of its slots' setup attachments (region corners / mesh
    vertices through their placement), reaching the farthest point along it; nothing to read
    (no child, images centred on the origin) = +x as before. Only a non-zero axis is stored.
    `createTarget` adds a bone where the chain end is shown now (two bones: the effector; aim:
    on the aim axis at that reach, else 60 px),
    under the chain's top-level ancestor; without `bendPositive` the current bend side is kept,
    so creating a constraint never moves the limb. `tip?` (1.4.0) names the effector; a two-bone
    chain whose end has a child always gets an explicit `tip` (the bone the target was placed
    at), so adding another child bone later never changes the limb. `extra.ik`, `extra.target`.
  * `setIk {ik, name?, mix?, bendPositive?, order? (null = default), target?, tip? (null = the
    implicit first child), bones?, aimAxis? (null = +x), fitAimAxis? (+ clip?, timeMs?)}`;
    `fitAimAxis: true` (aim chains) sets the axis to the direction of the target seen from the
    bone in the pose without IK (setup, or `clip` at `timeMs`), so the constraint stops turning
    the bone there (the editor's 현재 방향으로 맞추기),
    `renameIk {ik, id?, name?}` (renames `ik:<id>` tracks; `extra.tracks`),
    `deleteIk {ik, deleteTarget?}` (removes its tracks; `deleteTarget` also deletes the target
    bone when no other constraint or part uses it), `bakeIk {clip, timeMs, ik?, disable?}` (keys
    the solved rotation of every chain bone at `timeMs`; `disable` also keys `mix` 0 there).
  * Hierarchy edits keep references consistent: `reparentBone` is refused when it would break a
    constraint (chain no longer adjacent, target moved into a chain); `deleteBone` of a chain or
    target bone needs `cascade` and then removes those constraints and their tracks (warning
    `ANIM_IK_REMOVED`); `setPivot` keeps ids (the chain simply solves around the new origin);
    deleting the `tip` bone drops the field (warning `ANIM_IK_TIP_REMOVED`, the implicit rule
    applies again) and reparenting it away from the lower bone is refused.
  * Deleting the last constraint removes the `ik` list (documents keep the 1.2 shape).
* Editor-only: the constraint shown in the IK page, the target crosshair and the highlighted
  chain on the canvas never reach documents.

## Mesh attachments and weights (plugin 1.4.0)

* An attachment (default list or a skin) may be `type: "mesh"` (absent / `"region"` = the image
  rectangle as before). Mesh fields:
  * `vertices: [x, y, …]` — **image-pixel positions of the setup shape** (the same space as a
    region's pixels: (0,0) top-left, (width,height) bottom-right; vertices may lie outside it).
  * `uvs: [u, v, …]` — texture coordinates 0..1, one pair per vertex.
  * `triangles: [i, j, k, …]` — vertex index triples (drawn in array order).
  * `hull?` (the first `hull` vertices are the outline) and `edges?` (index pairs: outline and,
    from plugin 1.6.0, pinned internal edges — see Freeform meshes) are editor metadata;
    runtimes ignore them.
  * `weights?: [[{bone, weight}, …] per vertex]` — 1..4 bone shares per vertex. Absent =
    **unweighted**: the mesh is placed exactly like a region (`slotBone world · offset ·
    pivot`) and follows the slot's bone; with `vertices = uv × size` it draws pixel-identically
    to the region.
* **Weighted meshes: bind pose = the setup pose.** Vertices stay stored in image pixels (so
  uvs, hull and editing ops are the same for both kinds). At load every influencing bone gets a
  bind matrix `B = setupWorld(bone)⁻¹ · setupWorld(slotBone) · placement` (setup world = forward
  kinematics of the bones' setup transforms: no IK, no expression); a vertex is drawn at
  `Σ weight · world(bone) · B · (vertex + deform)` with the pose's world matrices **after IK**.
  Consequences: the setup pose always shows the authored shape; changing a bone's setup
  transform re-binds instead of deforming the mesh (animation acts relative to the new setup);
  weighting a mesh never moves it. Shares are normalized at load (a sum off by more than 1e-3 is
  warning `ANIM_MESH_WEIGHT_SUM`). Spine stores bone-local offsets per influence instead; the
  derived binds give the same skinning for a fixed setup pose.
* **Deform keys**: track `{"target": "slot:<id>", "property": "deform", "attachment":
  "<mesh name>", "keys": [{timeMs, offsets: [dx, dy, …], interp?, curve?}]}` — offsets in image
  pixels added to the setup vertices **before** skinning (so a weighted mesh's deformation moves
  with its bones). Every interp works (`step`, `linear`, `bezier` with `curve`; one eased
  fraction for all vertices of a segment); before the first key the first offsets hold, after
  the last the last. One deform track per (slot, attachment): a slot that swaps meshes (mouth
  shapes) keys each by name, and the offsets apply whenever that name is shown (any skin) and
  have exactly 2 × its vertex count; a skin whose same-named mesh has another count ignores
  them (warning `ANIM_MESH_DEFORM_SKIN`). Higher tracks override lower ones; cross-fades lerp
  offsets per vertex (a side without the track contributes 0). Deform keys carry no `value`.
* Order of operations: tracks → expression → world → IK → **skinning/deform** → draw list.
* Validation (`ANIM_MESH_*`): `TYPE`, `VERTICES` (x/y pairs, 3..2048 vertices), `UVS` (one pair
  per vertex, 0..1), `TRIANGLES` (index triples, 1..4096 triangles, indices in range), `HULL`,
  `EDGES`, `WEIGHTS` (one list per vertex, 0..1, no bone twice, sum > 0), `INFLUENCES` (max 4),
  `BONE_MISSING`, `DEFORM` (attachment missing / not a mesh / offset count); warnings
  `WEIGHT_SUM`, `DEFORM_SKIN`, `UNUSED` (mesh fields on a region, `attachment` on another
  track); `ANIM_NONFINITE` for numbers. A broken mesh draws nothing, so its deform keys are
  reported too.
* Draw items (ABI extension, `packages/plugin-host/CONTRACT.md` §7, capability
  `plugin-mesh-v1`): a mesh item adds `mesh: {positions, uvs, indices}` (positions in the same
  element-local pixels as `matrix`'s output, after placement and flipX). `matrix`/`width`/`height`
  stay the undeformed image rectangle (a fallback only; mesh-aware hosts never draw it for a mesh
  item). The host `sample` reply adds `deform: {"<slot>/<attachment>": offsets}` when keyed.
  The service's `requiredHostFeatures` reports `plugin-mesh-v1` for documents with any mesh
  (default list or skin), and export refuses a player that lacks it.
* Editing ops (`edit`; attachment = `{slot, name?, skin?}`, name defaults to the slot's setup
  attachment, `skin` edits that skin's own entry):
  * `convertToMesh {…, rows? = 4, cols? = 4, cells?}`: region (or mesh) → grid of rows × cols
    cells over the image; vertices on the region's pixels with matching uvs (the part looks the
    same), the full grid lists its outline first (`hull`), `edges` = outline. `cells` ("1"/"0"
    per cell, row-major) keeps only marked cells — the editor marks cells holding opaque pixels
    (alpha-trimmed grid). Clears weights; deform keys that no longer fit are removed (warning
    `ANIM_MESH_DEFORM_REMOVED`, `extra.keys`). `extra.vertices`, `extra.triangles`.
  * `convertToRegion {…}` (mesh fields and its deform keys removed).
  * `setMeshVertices {…, vertices?, positions? | dx?, dy?, keepUvs?}`: setup vertices in image
    pixels (`positions` per listed vertex, all vertices without `vertices`; or a common dx/dy).
    uvs follow by default (`uv = position / size`, clamped: the image stays under the outline);
    `keepUvs` stretches the image instead.
  * `autoWeights {…, bones?, vertices?, influences? = 4, falloff? = 2}` ("자동 웨이트"): each
    vertex gets the nearest `influences` bones by distance (setup pose) to each bone's segment
    (origin → its child's origin, preferring a child among the candidates; a childless bone is a
    point); share ∝ 1 / (distance + 1 px)^falloff, normalized and rounded so every vertex sums to
    exactly 1. Default bones: those already weighting the mesh, else the slot bone, its parent
    and children. `bindMesh {…, bones, …}` is the same with required bones. `extra.bones`.
  * `setWeights {…, vertices, bone, weight, strength? = 1}` (weight brush): the bone's share
    moves towards `weight` by `strength`, the other shares are rescaled to the rest (a vertex
    with no other bone hands the rest to the slot bone); or `{…, vertices, weights: [[{bone,
    weight}]…]}` sets shares outright. An unweighted mesh starts from 100 % slot bone.
  * `unbindMesh {…}` removes `weights` (the setup shape stays).
  * Deform keys: `setKey {clip, target: "slot:<id>", property: "deform", attachment, timeMs,
    offsets?, interp?…}` (no offsets = the rest shape, zeros) and `deleteKey {…, attachment}`;
    `moveKeys`/`scaleKeys`/`deleteKeys`/`copyKeys`/`pasteKeys` refs carry `attachment` too
    (clipboard entries hold `offsets`).
  * `deleteBone` moves mesh weights off the deleted bone (renormalized; a vertex left without
    bones follows its slot bone; warning `ANIM_MESH_WEIGHTS_MOVED`); `removeSkinAttachment` of an
    outfit-only mesh drops its deform keys.
* Editor-only: the mesh edit mode (정점 편집 / 웨이트 칠하기), the vertex selection, the brush
  (bone, weight, strength, radius) and the heat map never reach documents.

## Freeform meshes (plugin 1.6.0)

* No new required data: a freeform mesh is a `type: "mesh"` attachment like a grid. What 1.6.0
  gives meaning to is the editor metadata: `hull` = the first `hull` vertices are the **outline
  polygon in order** (either direction, simple), and `edges` = index pairs of the outline edges
  (derived from the triangles: edges used by one triangle) **plus internal edges the author
  pinned** (Spine's convention). Runtimes still ignore both; triangles alone are drawn.
* Triangulation = **constrained Delaunay**: the outline is ear-clipped, flipped to Delaunay, the
  interior vertices are inserted (a point on an existing edge splits it), pinned edges are forced
  in by flipping the edges they cross (Sloan), and every flip keeps the outline and pinned edges.
  The core does it with plain IEEE arithmetic (sqrt, no hypot/trig, no unordered iteration), so
  native and Wasm produce identical documents (`tests/plugins/cutout-outline-parity.test.mjs`).
  Every result tiles the outline exactly: positive triangles inside it, each edge used by at most
  two, area sum = outline area. Limits as before (2048 vertices, 4096 triangles).
* Meshes **with** an outline (`hull >= 3`: outline ops, and every full `convertToMesh` grid) are
  re-triangulated from the outline on each structural edit. Meshes **without** one (alpha-trimmed
  grids, documents of other tools) are edited locally on their existing triangles (split, fan
  re-triangulation, flips); their outline vertices cannot be removed.
* Vertex data follows every change: kept vertices keep their uv, weights and deform offsets
  (renumbered); a new vertex gets uv, weights (merged per bone, normalized, top 4) and deform
  offsets blended **barycentrically** from the old setup triangle it lies in (the nearest one when
  outside), so the picture and the animation do not jump. Deform keys of the edited
  (slot, name) whose offset count is the old vertex count are rewritten (warning
  `ANIM_MESH_DEFORM_REMAPPED`, `extra.remappedKeys`); keys of another count belong to a
  same-named mesh in another skin and are left alone; keys that no longer fit any mesh are
  removed (`ANIM_MESH_DEFORM_REMOVED`, as in 1.4.0).
* Editing ops (attachment = `{slot, name?, skin?}`; points in image pixels, rounded to 1e-6 like
  stored vertices; every op reports `extra.vertices`, `extra.triangles`, `extra.hull`,
  `extra.pinnedEdges`):
  * `createMeshFromOutline {…, hull: [x, y, …] (>= 3 points), points?: [x, y, …] (interior),
    spacing? (px, 2..10000: also fill the inside with a grid, kept spacing/2 from the outline),
    edges?: [i, j, …] (pinned, indices into hull + points)}`: region or mesh → the constrained
    Delaunay mesh. `uv = position / image size` (clamped). Refused with the reason: fewer than 3
    points, a doubled point, a spike, **a self-intersecting outline** ("윤곽이 스스로
    교차합니다(가장자리 i-j와 k-l)"), zero area, an interior point outside / on the outline, a pinned
    edge crossing another or leaving the mesh. Re-outlining a mesh resamples its weights and
    deform keys at the new vertices.
  * `autoOutline {…, mask: {width, height, alpha: base64, one byte per pixel, row-major},
    alphaThreshold? = 0.05 (0..1), detail? = 0.5 (0..1), spacing?, points?, edges?}`: the outline
    of the **largest opaque region** of the mask (pixels with alpha > threshold × 255), grown by
    the simplification tolerance (so the simplified polygon stays around the picture), traced with
    marching squares on pixel-centre samples (points on [0, w] × [0, h], saddles kept apart) and
    simplified by Douglas-Peucker with tolerance `0.5 + (1 − detail) × 4 % of the larger side` px
    (grown until at most 256 points; shrunk while the result would cross itself); mask pixels
    are scaled to the attachment's image size; then meshed as createMeshFromOutline.
    `extra.outline` = the outline points. **The core decodes no PNG**: Qt passes the decoded
    image's alpha; the node service fills `mask` from `imagePath` (an absolute PNG path, read
    only: 8/16-bit gray, gray+alpha, RGB, RGBA, palettes with tRNS; interlaced PNGs are refused)
    for `edit` operations (alone or in a batch) before calling the core. The trace function is
    exported (`cutout::traceAlphaOutline`) so the editor previews exactly what the op makes.
  * `addMeshVertex {…, position: [x, y]}`: with an outline, inside → an interior vertex
    (appended), on an outline edge (within 1e-6 × size) → an outline vertex inserted right after
    the edge's first vertex (later vertices shift by one; weights / deform keys follow), then
    re-triangulation; without one, the triangle (or edge) under the point is split. Outside / on
    an existing vertex is refused. `extra.vertex` = the new index.
  * `removeMeshVertex {…, vertices}`: later vertices shift down; pinned edges touching them go.
    Outline vertices only while >= 3 stay, the outline stays simple and every interior vertex
    stays inside it; meshes without an outline only lose interior vertices.
  * `addMeshEdge {…, edge: [a, b]}` / `removeMeshEdge {…, edge: [a, b]}`: pin / unpin an internal
    edge (a crease the mesh folds along). Refused: the same vertex twice, an outline edge, an
    already pinned (or, to remove, an unpinned) edge, an edge through another vertex, crossing an
    outline or pinned edge, or running outside the mesh (across a concave notch).
  * `retriangulate {…}`: the constrained Delaunay triangulation of the current vertices (from the
    outline, so a mesh whose triangles folded after vertex drags is repaired; without an outline
    by flipping). Vertices, uvs, weights and keys are untouched. Refused when the outline crosses
    itself or an interior vertex left it.
* Validation adds warnings only (the data is editor metadata): `ANIM_MESH_HULL` (a hull of 1..2
  vertices), `ANIM_MESH_HULL_SELF` (the outline polygon is not simple, e.g. after dragging an
  outline vertex across the mesh; re-triangulation is refused until it is fixed),
  `ANIM_MESH_EDGES` (an edge joining a vertex to itself). Documents of 1.4.0/1.5.0 load, validate
  and draw unchanged.
* Editor rendering (Qt, not a data rule): a mesh whose vertices are one affine image of their uvs
  (undeformed, or moved as a whole by its bones) is filled once through its outline; a deformed
  mesh is drawn as textured-brush triangles, aliased inside (exact tiling) and antialiased only on
  the outline. 2048 triangles, 1024² offscreen: ~1.3 ms rigid, ~7.5 ms deformed (was ~41 / 45 ms
  with a clip path per triangle).

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
  * Plugin 1.10.0: until the user drags a splitter the layout is automatic (pane sizes are
    proportional to the window, never below the width the pane's widest tool row needs, side
    panes capped at a multiple of it); only a dragged layout is saved (`layout/version` 3,
    `layout/custom`). Older (v1/v2) states are ignored; a v3 state is restored and widened
    where a row would hide tools behind the overflow `»`. `레이아웃 초기화` returns to automatic.
  * 작업 공간 글자 크기 (`view/uiScale`: 0.9 / 1 / 1.25 / 1.5, Ctrl+= / Ctrl+- / Ctrl+0) scales the
    workspace text over the host font, plus icons, row paddings, the dopesheet row height,
    ruler and label column, graph key/handle radii, canvas gizmos and their pick radii. Absent,
    the default comes from the screen: 1.25 (short side >= 1400) or 1.5 (>= 2000) at 100 %
    Windows scaling when the width keeps 1280 logical px per step, else 1.
  * The first-run hint strip (`view/hintDismissed`) and the command palette (Ctrl+K / F1: every
    workspace button, menu command and page by name, group or shortcut; running one is the
    same as clicking it, so an editing command is exactly one edit) are presentation only.
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
  need plugin 1.3.0; without them they are unchanged. Documents with an IK `tip`, mesh
  attachments or `deform` tracks need plugin 1.4.0 (and, exported, a player with
  `plugin-mesh-v1` for meshes); without them they are unchanged.
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
* Events: `{timeMs, name, int?, float?, string?}`. An event fires once per occurrence in the
  half-open interval `[previous, current)` of clip time; for one-shot clips an event exactly at
  the end fires with completion (a looping clip never fires an event at exactly `durationMs`).
  Seeking (editor scrubbing) never fires events. See "Clip events" below for definitions and
  payloads.

## Clip events (plugin 1.8.0)

Spine-like event definitions at the rig level, all optional (rigs without them load and play as
in 1.7):

```json
"events": [{"name": "footstep", "int": 1, "float": 0.5, "string": "left", "audio": "sfx-step", "volume": 0.8, "balance": -0.2}],
"clips": [{"id": "walk", "...": "...", "events": [{"timeMs": 0, "name": "footstep"}, {"timeMs": 300, "name": "footstep", "string": "right"}]}]
```

* Definition: `name` (`$defs/name`, unique), default payload `int` (32-bit integer), `float`
  (finite, |v| <= 1e6), `string` (<= 1000 bytes), and `audio` (an asset id of kind audio) with
  `volume` 0..1 (default 1) and `balance` -1 (left)..1 (right) (default 0). Defaults (0, 0, "",
  1, 0) are not stored by the editing ops. Volume/balance without audio warn
  (`ANIM_EVENT_AUDIO_UNUSED`).
* Keys: `name` plus optional `int` / `float` / `string` overriding the definition (audio, volume
  and balance always come from the definition). Once `events` holds at least one definition every
  key must name one (`ANIM_EVENT_UNKNOWN`); without definitions any name is valid (1.7 rigs).
* Validation codes: `ANIM_EVENT_NAME` (definition name format), `ANIM_DUP_ID` (definition),
  `ANIM_EVENT_PAYLOAD` (int/float/string types, volume/balance ranges), `ANIM_EVENT_AUDIO`
  (core: not a non-empty id; service: the asset is missing or not of kind audio),
  `ANIM_EVENT_TIME` (key time not an integer in `0..durationMs`), `ANIM_EVENT_UNKNOWN`,
  `ANIM_EVENT_DUPLICATE` (warning: the same name twice at one time; both fire, editing ops
  address the first).
* Notification (`kind: "event"`, op `update`/`notifications`): `{kind, track, clip, serial, name}`
  plus the resolved payload — only the fields that are set: `int`, `float`, `string` (key
  override, else definition) and, when the definition has a sound, `audio`, `volume`, `balance`.
  A rig without payloads produces exactly the 1.7 shape. Deterministic: the same notifications
  (payload included) at any frame pacing.
* **Presentation only.** Controller bindings react to game signals only; they never match clip
  events, and events never reach game rules, costs or RNG. Playing `audio` is the host's job
  (packages/plugin-host/CONTRACT.md §7): the native player plays it through its sound-effect path
  (miniaudio volume/pan) and the web player through `HTMLAudioElement` (balance via a Web Audio
  stereo panner), both only while sound is on; both report the newest 32 events with payload
  (`extensions.events`) for automation. Other hosts may ignore sounds and read the payload only.
* Editing ops (one gesture = one op): `addEventDef {name, int?, float?, string?, audio?, volume?,
  balance?}` (the first definition also defines every name keys already use: `extra.defined`),
  `setEventDef {event, ...}` (null removes a field), `renameEventDef {event, name}` (keys in every
  clip follow: `extra.renamedKeys`), `deleteEventDef {event, force?}` (refused while keys use it;
  force deletes them: `extra.removedKeys`), `setEventKey {clip, timeMs, name, newName?, int?,
  float?, string?}` (creates or edits; null drops an override; `extra.keys`), `moveEventKey {clip,
  name, fromMs, toMs}`, `deleteEventKey {clip, name, timeMs}`. The multi-key ops `moveKeys`,
  `scaleKeys`, `deleteKeys`, `copyKeys`, `pasteKeys` take event keys as refs `{target: "event",
  property: <name>, timeMs}` next to track keys; an event key collides only with a key of the same
  name at the same time (different events may share a time). `setInterp`/`setCurve` refuse them.

## Playback (per instance)

* Tracks 0..15 applied in ascending order; a higher track overrides only the properties
  its clip keys. Track 0 is the base action; controller `layers` (e.g. blink) run on 1..15.
  The active expression (plugin 1.2.0) applies between track 0 and track 1; the instance skin
  resolves every attachment name at draw time; IK constraints (plugin 1.3.0) are solved on the
  world transforms after all tracks, and meshes (plugin 1.4.0) are skinned on the solved world.
* `play` replaces the track entry. If the mix duration (`mix.pairs` → `mix.defaultMs`, or
  explicit) is > 0, the old entry keeps animating and is cross-faded linearly; attachments
  switch at the halfway point. Events and completions of the outgoing entry are suppressed.
* Notifications: `start`, `interrupt` (replaced), `end` (no longer applied), `complete`
  (every loop boundary with loop index; once for one-shots), `event` (with its payload, plugin
  1.8.0). Each is emitted once per entry `serial`; hosts never need deduplication.
* One-shot `then`: `default` (track 0 → controller default clip; other tracks stop),
  `hold` (stay on the last frame), `stop` (mix out to lower tracks/setup).
* `update(dt)` clamps dt to 0..10 s; a long pause completes loops instead of replaying them.

## Controller bindings and game signals

Signals are derived **only from committed game transitions** (one call per command), never
from frames. Clip events are not signals: a binding can never depend on an animation event
(game logic stays independent of presentation timing). Hosts call the core's `observe` op; all hosts use the same C++ derivation.

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
`skin` and `expression`. IK and meshes add no state: constraint values, weights and deform
offsets come from the rig and the clips.

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

## Large rigs (plugin 1.9.0)

Measured on the deterministic stress rig (`core/tests/stress-rig.hpp`: 200 bones, 150 slots, 40
grid meshes with 1680 vertices of which 1260 weighted, 30 clips / 9148 keys incl. a 3000-key clip,
4 skins, 6 expressions, 6 IK constraints, event keys); `tests/plugins/cutout-perf.test.mjs`
reports every layer and fails on ~3x regressions (core budgets in `core/tests/bench.hpp`).

* **Compact frames** (engine CONTRACT.md §7): `init {frameFormats:[2]}` answers `frameFormat: 2`
  and the mesh topology table; `frame {format:2, since}` answers fixed-point number arrays with
  `0` for items unchanged since `since`. Both players use them; the documented items stay the
  default for every other host. 146 KB → 39 KB per character and frame (a paused one: ~0.4 KB).
* **Batched validation**: the `validate` op takes `controllers: [{data, pointer}]` and answers
  `controllerDiagnostics` (one list each): the rig is validated and parsed once per pass (the
  service also remembers the last results by rig text). `parseValidatedRig` skips the second
  validation of data that was just validated.
* Hosts pass large requests through Wasm heap memory (the runtime exports `stringToUTF8`,
  `lengthBytesUTF8`, `UTF8ToString`): rigs above ~1 MB used to overflow the Wasm stack.
* Plugin documents may be up to 8 MiB on disk (engine CONTRACT.md §3).

## Settings document (plugin 1.9.0)

`plugins/cutout-animation/settings/project.json` (type `settings`, id `project`, schema
`schema/settings.schema.json`; any other id is warning `ANIM_SETTINGS_ID` and ignored). Absent =
the defaults. It is project data (edited with ordinary edits, undoable, readable by MCP) and is
never exported.

```jsonc
{ "schemaVersion": 1, "export": { "atlas": { "enabled": true, "maxPageSize": 2048, "padding": 2, "trim": true } } }
```

## Atlas packing on export (plugin 1.9.0)

The service hook `prepareExport` (engine CONTRACT.md §10) packs every PNG that an exported rig
draws (default attachments and skins) into pages of at most `maxPageSize` px when `enabled` and
the target player advertises `plugin-atlas-v1`; other players (and exports whose caller passes no
player features) keep separate files. Images that no rig draws are never read. Kept separate
(`report.skipped`): non-PNG, 16-bit, interlaced or colour-managed (`gAMA`/`iCCP`/`cHRM`) PNGs and
pictures larger than a page. Packing is deterministic (skyline, tallest first).

* **Exact pixels**: a page holds byte copies of the pictures; around each one `padding` pixels
  repeat its edge pixels, so linear filtering at a region's edge samples what the separate image
  gave (its clamped edge). **Trimming** (`trim`, regions drawn at their own pixel size only: never
  meshes, never pictures drawn scaled) drops fully transparent borders but keeps a 1 px
  transparent ring, so edges still blend towards transparency exactly as before.
* **Rig data**: each packed attachment gains `atlas: {page, x, y, width, height, trim?}` (schema
  `$defs/attachment`): the picture is that sub-image of the page asset; `trim` is the rectangle
  of the original picture it shows. Validation (`ANIM_ATLAS`): page id, non-negative position,
  positive size, trim inside the picture, no trim on meshes.
* **Draw items**: `imageAssetId` = the page, `region: [x, y, w, h]`; a trimmed region's matrix is
  `matrix · translate(trim.x, trim.y)` with the trimmed width/height (the kept rectangle lands
  where it was); meshes keep matrix, uvs and triangles (uvs address the sub-image).
  `requiredHostFeatures` adds `plugin-atlas-v1` for such rigs.
* **Export**: pages are `assets/cutout-atlas-<n>.png`; packed images' own files are left out
  unless the content names them elsewhere (fewer files: the test fixture's 8 rig images become one
  page). Screenshots with and without atlas differ by a mean of 0.0013/255 (native) and 0.004/255
  (web) (`tests/plugins/cutout-atlas.test.mjs`).

## Layer import (plugin 1.11.0)

Layered art becomes a rig whose setup pose looks exactly like the document. One implementation
(`service/psd.mjs` reader + `service/layer-import.mjs` mapping) serves every path: the service
import hook `prepareImport` (engine CONTRACT.md §11; Qt `pluginImport`, MCP `plugin_import`) and
the read-only op `importLayers` (`plugin_call`). No document format change: everything it writes
is ordinary schemaVersion 1 data plus editor metadata.

* **Sources** (`payload.file`, absolute): a `.psd` (version 1) or `.psb` (version 2); a folder of
  PNGs, a PNG inside it (= the folder) or its `layout.json`
  `{width?, height?, layers: [{file, name?, x?, y?, visible?, opacity?, group?: "body/head"}]}`
  (first = back; without a layout every PNG at 0,0, sub-folders = groups, natural name order, a
  leading number `01-` only sorts). PSD: 8-bit RGB only — 16/32-bit, CMYK, indexed, grayscale,
  Lab, … are refused with a message saying how to re-save; raw and PackBits channels (ZIP channels
  refused); Unicode names (`luni`); section dividers (`lsct`/`lsdk`) = groups; a document without
  layer records imports its merged image as one layer. Canvas ≤ 8192 px, ≤ 64 Mpx of layers.
* **Payload**: `file`, `id` (new character, default from the file name; an existing rig or
  controller id is refused) **or** `rig` (re-import onto that rig), `name?`, `origin?`
  (`bottom-center` default, `center`, `top-center`, `top-left`, `{x, y}` in document px),
  `trim?` (default true), `scale?` (0.05..4, default 1), `exclude?` (report layer keys).
* **Mapping** (new character): canvas = document size × scale, root bone `root` at the origin;
  a top-level `[pivot]` layer's centre overrides the origin.
  * group → bone (id from the name, nested groups = child bones, local = world − parent); origin =
    the centre of a `[pivot]` / `[bone]` / `#pivot` marker layer inside it (not imported as a
    picture), else the centre of its content. A group without pictures makes no bone.
  * layer → region attachment, picture trimmed to its pixels (alpha > 0) plus a 1 px transparent
    ring (linear filtering at the edge blends to transparency like the document), placed with
    pivot 0.5 at its centre in the bone's space; `scale` ≠ 1 sets the attachment scale (the
    pictures keep their resolution). `trim: false` keeps the whole layer rectangle.
  * names: `slot/attachment` or `slot@attachment` → one slot (`eyes/open`, `eyes/closed`); the
    layers of a `[slot]` group are attachments of the slot named after the group; every plain
    layer is its own slot (attachment named like the slot). Ids are lowercase kebab-case from
    the name (accents dropped); names without Latin letters fall back to `part-N` / `bone-N`; a
    slot name used under two bones becomes `<bone>-<slot>`, later duplicates `-2`.
  * draw order = layer order (a slot sits where its shown layer is); the topmost visible layer of a
    slot is its setup attachment, hidden layers (or layers of hidden groups) are attachments that
    are not shown (expression / key swaps); a slot with only hidden layers has `attachment: null`.
    Layer opacity × group opacities → slot `alpha` (overlapping children of a translucent group can
    differ slightly from Photoshop's isolated group).
  * `[ignore]` / `[skip]` drop a layer or a whole group; `exclude` keys likewise.
  * Warnings (report): blend modes other than normal/pass-through (`ANIM_IMPORT_BLEND`, drawn
    normal), clipping masks (`ANIM_IMPORT_CLIPPING`), layer masks (`ANIM_IMPORT_MASK`, not applied),
    translucent groups, two visible layers in one slot, empty layers/groups, more than one marker.
  * New rig: `idle` clip, mix 150 ms; new controller `{rig: id, defaultClip: "idle"}`.
* **Provenance**: `editor.layerImport = {source, format, width, height, scale, layers: {"slot/name":
  {x, y, width, height, pixels}}}` (picture rectangle in document px and a pixel hash). Runtimes
  ignore `editor`.
* **Re-import** (`rig`): bones, slots, keys, skins, expressions, IK and every placement stay.
  Matching is by id: groups map to bones of the same id (kept, never moved; new groups become new
  bones placed through the current setup pose), `slot/name` to existing attachments.
  * Unchanged picture (same rectangle and pixel hash): untouched (`unchanged`).
  * Changed region attachment: new image and size; the placement (x, y, rotation, scale) is kept
    and the pivot moves so the same picture content stays under (x, y):
    `pivot' · size' = pivot · size − d`, `d` = how far the picture's corner moved since the last
    import (no provenance: placement kept, warning `ANIM_IMPORT_PLACEMENT`). An untouched rig
    re-imported from a changed document therefore draws the new document exactly.
  * Mesh attachments only get a new image when its size and position are unchanged
    (`ANIM_IMPORT_MESH` otherwise).
  * New layers: new attachments / slots (inserted after the nearest earlier imported slot).
    Attachments imported before but absent now are reported (`missing`) and kept. The canvas
    stays (`ANIM_IMPORT_CANVAS` when the document size differs); the first import's scale is used.
* **Result**: `{documents: [{type: "rig", id, data}, {type: "controller", …}?], images: [{id, bytes}],
  report}`; image ids are `image-<sha256 of the PNG, 24 hex>` (the same as `importImage`), so an
  unchanged picture is the same asset. `report`: `mode` (`create`/`update`), `rigId`, `source`,
  `options`, `canvas`, `layers` (tree, back → front: `key`, `name`, `kind`, `visible`, `opacity`,
  `blend`, `bounds`, `include`, `role` = image/bone/pivot/slot-group/ignored/excluded/empty/…,
  `bone`, `slot`, `attachment`, `setup`, `status` = add/update/same/skip/kept, `warnings`),
  `warnings`, `changes` (bones added/kept, slots added/kept, attachments added/updated/unchanged/
  skipped/missing) and `summary`. The rig passes core validation before it is returned. A preview
  (`dryRun`) has no images and empty `imageAssetId`s.
* `plugin_call` op `importLayers`: the same with `target: {id, rig}` for a re-import preview of rig
  data and `includeImages: true` for base64 PNGs; it never writes anything.
* Editor: `PSD 가져오기…` (character row `cutoutImportLayers`, plugin menu
  `cutoutImportLayersAction`, command palette) → file → preview → dialog
  `cutoutLayerImportDialog` (`cutoutLayerImportSource`, `…Target` new / update, `…Id`, `…Origin`,
  `…Scale`, `…Trim`, layer tree `…Tree` with include boxes and bone/slot/attachment/status columns,
  `…Warnings`, `…Summary`, `…Refresh` = preview again) → `가져오기` = one edit (one Ctrl+Z). The
  file the selected character was imported from defaults to updating it.

## Clipping (plugin 1.12.0)

Spine clipping attachments: a polygon that hides everything of a slot range outside it.

* **Data**: an attachment (default list or a skin) `type: "clipping"` with `vertices: [x, y, …]`
  (3..256 points, a simple polygon, convex or concave) and `end: <slot id>`. No picture:
  `imageAssetId`, `width`, `height`, `pivotX/Y`, `uvs`, `triangles`, `hull`, `edges`, `atlas`,
  `sequence` are not used (`ANIM_CLIP_UNUSED` warning). The polygon is placed like a mesh: points
  in the slot bone's space through the attachment's `x, y, rotation, scaleX, scaleY` (pivot and
  size are 0), or `weights` (1..4 bones per point, bind pose = setup pose, exactly the mesh rules);
  deform keys (`property: "deform"`, `attachment` = the clip's name, offsets per point) move it.
* **Range**: when a slot shows a clipping attachment, the slots after it in draw order up to and
  including `end` draw only inside the polygon. The clipping slot itself draws nothing and its
  alpha does not matter. The range ends at `end` even when that slot shows nothing. A clipping
  attachment inside an active range is ignored (no nesting, as in Spine).
* **Runtime** (`cutout::drawItems`, every player): each drawn item of the range is cut on the CPU.
  The polygon is ear-clipped and merged into convex parts (Hertel–Mehlhorn, so concave polygons
  work); every item triangle (a region = its two-triangle quad over uvs 0..1 of its picture or
  atlas sub-image) inside one part is kept, others are cut against each part (Sutherland–Hodgman,
  uvs interpolated linearly). Results: an item entirely inside stays exactly as it was (a region
  stays a region, a mesh keeps its topology id); an item entirely outside is dropped; otherwise
  the item becomes a mesh item (`positions`, `uvs`, `indices`; matrix/size/region unchanged as the
  fallback rectangle) with shared vertices deduplicated, split into several items of at most 2048
  vertices / 4096 triangles. Hosts therefore need nothing beyond `plugin-mesh-v1`
  (`requiredHostFeatures` lists it for rigs with a clipping attachment). Positions are snapped to
  1/4096 px before cutting, the cut uses plain IEEE arithmetic (sqrt, no libm) and every
  intersection is computed from its segment's endpoints in one canonical order: native and Wasm
  emit bit-identical triangles, and two triangles sharing an edge get the same cut points.
* **Compact frames**: a clipped item's topology changes with the pose, so its mesh id is interned
  by content (`uvs` + `indices`) instead of by attachment: an unmoved clip keeps its id, a moving
  one adds ids that hosts fetch with `meshes` like any unknown id. After 4096 such topologies the
  plugin restarts the table under a new `meshEpoch` (between frames), which hosts already handle.
* **Validation**: `ANIM_CLIP_END` (missing, unknown, not after the clipping slot),
  `ANIM_CLIP_VERTICES` (pairs, 3..256), `ANIM_CLIP_POLYGON` (`polygonProblem`: crossing or
  touching edges, spikes, no area), weights as meshes (`ANIM_MESH_WEIGHTS`, …). A pose may still
  fold a weighted polygon over itself: the runtime then fans what ear clipping cannot cut
  (bounded, deterministic).
* **Editing ops**: `addClipping {slot (new), bone?, before? | index?, name? ("clip"), vertices |
  worldVertices (rig px at the setup pose), end? (= before)}`, `setClipping {slot, name?, skin?,
  vertices? | worldVertices?, end?}` (a new point count drops weights and the polygon's deform
  keys). `deleteSlot` of a range's end moves `end` to the slot below; a range that becomes empty
  deletes the clipping slot too (`ANIM_CLIP_REMOVED`). `moveSlot` that would put `end` before the
  clip is refused (`ANIM_CLIP_END`).
* **Editor** (page `클리핑`, `cutoutClippingPage`): `클리핑 만들기` (`cutoutClippingCreate`) draws
  the polygon with the outline tool over the selected part (clicks; first point / double-click /
  Enter closes, Esc cancels) → `addClipping` (new slot `<part>-clip` right below the part, end =
  the part); `모양 다시 그리기` (`cutoutClippingRedraw`) → `setClipping`; `끝 조각`
  (`cutoutClippingEnd`) → `setClipping {end}`; `클리핑 삭제` (`cutoutClippingDelete`); the list
  `cutoutClippingList` (clip slot · first → end); `윤곽 보기` (`cutoutClippingOutlines`, dashed
  polygons on the canvas, editor-only). The canvas clips with a painter clip path from the same
  core polygon (`drawRig`); a clipping slot hidden with the eye toggle clips nothing.

## Image sequences (plugin 1.12.0)

Spine sequences: one attachment shows one of several same-size pictures, picked by keys.

* **Data**: a region or mesh attachment may have `sequence: {frames: [imageAssetId…], setupIndex?,
  atlas?}`: 1..256 image assets of the attachment's size (`imageAssetId` must be `frames[0]`,
  `ANIM_SEQUENCE_IMAGE`; the service checks every frame's PNG size against the first,
  `ANIM_SEQUENCE_FRAME_SIZE`, and that it is an image, `ANIM_IMAGE_MISSING`). All frames share the
  placement (and a mesh's uvs/triangles). `setupIndex` (default 0) is shown without keys.
* **Keys**: slot track `{target: "slot:<id>", property: "sequence", keys: [{timeMs, mode, index,
  delay}]}`, step only (`ANIM_INTERP`); `mode` = `hold` (default) | `once` | `loop` | `pingpong`
  | `onceReverse` | `loopReverse` | `pingpongReverse` (`ANIM_SEQUENCE_KEY`), `index` (start frame,
  default 0, below the slot's largest frame count: `ANIM_SEQUENCE_INDEX`), `delay` (ms per frame,
  default 100, > 0: `ANIM_SEQUENCE_DELAY`). The slot must have a sequence attachment
  (`ANIM_SEQUENCE_TARGET`).
* **Frame rule** (`cutout::sequenceFrame`, Spine `SequenceTimeline`): before the clip's first key
  the frame is `setupIndex`; afterwards from the last key at or before the clip time `t`:
  `i = index + floor((t − key.timeMs) / delay + 1e-6)` (hold: `i = index`), then with `n` frames:
  once `min(i, n−1)`, loop `i mod n`, pingpong `i mod (2n−2)` mirrored above `n−1`, onceReverse
  `max(n−1−i, 0)`, loopReverse `n−1 − i mod n`, pingpongReverse `(i + n−1) mod (2n−2)` mirrored;
  clamped to `0..n−1`. The epsilon keeps exact multiples on the new frame despite rounding
  (83.33 ms frames at 250 ms = frame 3). Clip time is the looping/clamped time like every track, so
  a looping clip restarts the sequence. Cross-fades switch the frame halfway, like attachments.
* **Draw items**: `imageAssetId` = the shown frame; packed sequences use `sequence.atlas[frame]`
  (written by the export packer: one region per frame, `null` = that frame kept its own file; the
  attachment's own `atlas` is not used for sequences). `sample` reports `slots[id].sequence` (the
  frame index) for slots showing a sequence.
* **Editing ops**: `setSequence {slot, name?, skin?, frames ([] / null removes), setupIndex?,
  width?, height?}`; keys with `setKey {…, property: "sequence", mode?, index?, delay?}`; the
  multi-key ops (move, scale, delete, copy, paste) carry `mode`/`index`/`delay`. Fewer frames clamp
  key indices (`ANIM_SEQUENCE_INDEX_CLAMPED`); removing the last sequence of a slot removes its
  sequence keys (`ANIM_SEQUENCE_KEYS_REMOVED`).
* **Editor** (page `시퀀스`, `cutoutSequencePage`): `PNG로 시퀀스…` (`cutoutSequenceImport`: PNG
  files in natural name order → one edit on the selected part, or a new part on the selected bone),
  `프레임 추가…` (`cutoutSequenceAddFrames`), `시퀀스 해제` (`cutoutSequenceClear`), frame list
  `cutoutSequenceFrames` with `▲`/`▼`/`빼기` (`cutoutSequenceUp`/`Down`/`RemoveFrame`), `기본
  프레임` (`cutoutSequenceSetup`), preview playback `▶ 미리보기` (`cutoutSequencePlay`, editor-only)
  with `재생 방식` / `시작 프레임` / `프레임 간격` (`cutoutSequenceMode`/`Index`/`Delay`), and in
  Animate mode `시퀀스 키` / `키 삭제` (`cutoutSequenceKey` / `DeleteKey`) at the playhead; the
  dopesheet shows a `<slot> 시퀀스` row (step keys). The canvas draws the pose's frame while
  scrubbing (`attachmentImage`).
