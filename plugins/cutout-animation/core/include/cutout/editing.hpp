// Pure authoring operations on rig `data` JSON (schemaVersion 1).
// Shared by the Qt authoring module and the service/LLM path (host op `edit`), so a
// gesture in the editor and a tool call produce byte-identical documents.
#pragma once
#include <stdexcept>
#include "rig.hpp"

namespace cutout {
/** A refused edit (unknown id, cycle, key collision...). what() is user-facing Korean. */
struct EditError : std::runtime_error { using std::runtime_error::runtime_error; };

struct EditResult {
  Json data;                            // new rig data (input is never modified)
  std::vector<std::string> affected;    // "bone:<id>", "slot:<id>", "clip:<id>", "key:<clip>/<target>/<property>@<ms>"
  std::vector<Diagnostic> diagnostics;  // validateRig(result) plus edit warnings
  Json extra = Json::object();          // op specific (controllerRefs, controllers, keys...)
};

/**
 * Apply one operation {op, ...}. Ops: addBone setBone transformBones reparentBone setPivot deleteBone
 * addPart setSlot setAttachment deleteSlot moveSlot setKey deleteKey moveKey setInterp
 * setCurve moveKeys scaleKeys deleteKeys copyKeys pasteKeys copyPose pastePose addClip
 * duplicateClip renameClip setClip deleteClip setMix setCanvas setName applySetupDeltaToKeys batch.
 * Interpolation (setKey/setInterp): interp step|linear|bezier (+curve [x1,y1,x2,y2]) or a
 * preset name (easeIn easeOut easeInOut auto flat back) as `interp` or `preset`; setCurve
 * sets one key's handles (x clamped to 0..1). Many keys = one `batch`.
 * Key selections (one clip, keys = [{target, property, timeMs}], all-or-nothing):
 * moveKeys {deltaMs, clamp?} and scaleKeys {factor, pivotMs?=first key} re-time keys with
 * their curves (refusing collisions/out-of-range), deleteKeys; copyKeys returns
 * extra.clipboard {keys: [{target, property, offsetMs, value, interp?, curve?}], fromMs}
 * without changing data; pasteKeys {clip, timeMs, keys, target?} writes it back (same-time
 * keys overwritten). copyPose {clip?, timeMs, bones?} returns extra.pose; pastePose
 * {clip, timeMs, pose} keys it. Re-timing/paste ops report the resulting keys in extra.keys.
 * Skins/expressions (plugin 1.2.0): addSkin {id, name?, from?} duplicateSkin {skin, id}
 * renameSkin {skin, id?, name?} deleteSkin {skin, force?} setSkinAttachment {skin, slot, name?
 * (= slot's setup attachment), imageAssetId, width, height, x.. pivotY} removeSkinAttachment
 * {skin, slot, name}; addExpression {id, name?, slots?, bones?} renameExpression {expression,
 * id?, name?} (also rewrites clip expression keys) deleteExpression {expression, force?}
 * setExpressionSlot {expression, slot, attachment|null | remove} setExpressionBone {expression,
 * bone, x?..scaleY? (null drops a channel) | remove} captureExpression {id, name?, clip?,
 * timeMs?, slots?, bones?, from?, replace?}. Expression keys: setKey {target:"rig",
 * property:"expression", value: id|null} (step only).
 * IK (plugin 1.3.0): addIk {id?, name?, bones, target? | createTarget (+ targetId?, targetParent?),
 * mix?, bendPositive?, order?, aimAxis?} setIk {ik, name?, mix?, bendPositive?, order?, target?, bones?,
 * aimAxis? (1.4.1: aim chains, degrees, null = +x), fitAimAxis? (+ clip?, timeMs?)}
 * renameIk {ik, id?, name?} (renames ik:<id> tracks) deleteIk {ik, deleteTarget?} bakeIk {clip,
 * timeMs, ik?, disable?}; keys: setKey {target:"ik:<id>", property: mix (0..1) | bendPositive (bool,
 * step)}. reparentBone refuses to break a constraint; deleteBone of a chain/target bone needs
 * cascade and removes the constraint. addIk/setIk take `tip` (plugin 1.4.0, explicit two-bone
 * effector, a child of the lower bone; setIk tip: null = implicit first child); addIk sets it
 * whenever the chain end has a child.
 * Meshes (plugin 1.4.0; attachment = {slot, name? (setup attachment), skin?}): convertToMesh
 * {rows?, cols?, cells?} convertToRegion setMeshVertices {vertices?, positions? | dx, dy,
 * keepUvs?} autoWeights {bones?, vertices?, influences?, falloff?} bindMesh {bones, ...as
 * autoWeights} setWeights {vertices, bone, weight, strength? | weights} unbindMesh; deform keys:
 * setKey/deleteKey {target: "slot:<id>", property: "deform", attachment, offsets?} (refs of the
 * multi-key ops carry `attachment` too). deleteBone moves mesh weights off the deleted bone.
 * Bone length (plugin 1.5.0): addBone/setBone take `length` (px >= 0, null/0 = point bone) and
 * `color` ("#rrggbb[aa]", editor display only; null/"" = default); setting a length on the lower
 * bone of a two-bone chain pins its first child as `tip` so the limb keeps its effector.
 * fitBoneLengths {bones?} ("자동 길이": length = distance to the first child, extra.lengths)
 * orientBones {bones?, fitLength?} ("본 방향 맞추기": turns point-bone frames toward their child /
 * images, compensating children, keys, attachments and aim axes so nothing draws differently;
 * extra.oriented / extra.skipped). addIk prefers a bone's length direction (+x) for aim chains
 * and the lower bone's length tip as two-bone effector.
 * Freeform meshes (plugin 1.6.0; points in image px): createMeshFromOutline {hull, points?, spacing?,
 * edges?} (constrained Delaunay of the outline polygon; self-intersections refused) autoOutline
 * {mask: {width, height, alpha (base64 bytes)}, alphaThreshold?, detail?, spacing?} (traceAlphaOutline,
 * then as createMeshFromOutline; the service fills `mask` from `imagePath`) addMeshVertex {position}
 * removeMeshVertex {vertices} addMeshEdge / removeMeshEdge {edge: [a, b]} (pinned internal edges, kept
 * in `edges`) retriangulate. Meshes with a `hull` re-triangulate from it, others are edited locally;
 * uvs, weights and deform keys follow (new vertices blend barycentrically, ANIM_MESH_DEFORM_REMAPPED).
 * Selections (plugin 1.7.0): transformBones {bones, delta: {x?, y?, rotation?, scaleX?, scaleY?}, space?
 * (world | parent), pivot? (origin | centroid | {x, y}), clip? + timeMs? (= keys), compensateBones?,
 * compensateImages?} moves the topmost selected bones and turns/scales every selected bone about its
 * own origin, or the topmost ones as a group about the pivot (one canvas gesture = one op).
 * setBone also takes `axes` (local | parent | world, see toAxes) and the compensation flags:
 * unselected children / the bone's images keep their setup world pose (setup only: Spine semantics).
 * Events (plugin 1.8.0): definitions addEventDef {name, int?, float?, string?, audio?, volume?, balance?}
 * (the first one also defines every name keys already use: extra.defined) setEventDef {event, ...same,
 * null removes} renameEventDef {event, name} (every key follows) deleteEventDef {event, force?} (keys
 * using it refuse without force); keys setEventKey {clip, timeMs, name, newName?, int?, float?, string?}
 * (creates or edits; null drops an override) moveEventKey {clip, name, fromMs, toMs} deleteEventKey
 * {clip, name, timeMs}. The multi-key ops (moveKeys scaleKeys deleteKeys copyKeys pasteKeys) take event
 * keys as refs {target: "event", property: <name>, timeMs}; event keys collide only with a key of the
 * same name at the same time.
 * Clipping (plugin 1.12.0): addClipping {slot (new), bone?, before? | index?, name? ("clip"), vertices
 * (bone px) | worldVertices (rig px, setup pose), end? (= before)} inserts a slot showing a clipping
 * attachment; setClipping {slot, name?, skin?, vertices? | worldVertices?, end?}; deleteSlot of a clip
 * range's end moves it to the slot below (an emptied range removes the clipping).
 * Sequences (plugin 1.12.0): setSequence {slot, name?, skin?, frames (image ids; [] / null = remove),
 * setupIndex?, width?, height?}; keys setKey {target: "slot:<id>", property: "sequence", mode? (hold),
 * index? (0), delay? (100 ms)} (step only; copy/paste carry mode/index/delay).
 * IK chains (plugin 1.13.0): addIk bones may be a run of 3..32 bones (each the direct child of the
 * previous; tip / length tip of the last bone as effector, solved by FABRIK); addIk/setIk take
 * stretch / compress (aim only) / uniform (booleans, false drops the key).
 * Physics (plugin 1.13.0): addPhysics {bone | bones, preset? (hair | skirt | accessory), id?, name?,
 * x?, y?, rotate?, scaleX?, inertia?, strength?, damping?, mass?, wind?, gravity?, mix?, limit?,
 * step?, teleport?, resetOnStart?} (one constraint per bone, extra.physics = ids) setPhysics
 * {physics, preset?, bone?, name?, ...fields (null = default)} renamePhysics {physics, id?, name?}
 * deletePhysics {physics}; keys setKey {target: "physics:<id>", property: "mix"}. deleteBone drops
 * the bone's physics (warning ANIM_PHYSICS_REMOVED).
 * Shear / inherit (plugin 1.16.0): addBone/setBone take shearX, shearY (degrees, 0 drops the key)
 * and inherit (normal | onlyTranslation | noRotationOrReflection | noScale | noScaleOrReflection;
 * null / normal drops it); shear keys setKey {target: "bone:<id>", property: shearX | shearY}. Ops
 * that turn a world pose into local values (reparentBone, deleteBone children, compensation,
 * setPivot, setBone `axes`) invert the bone's mode exactly (skew of shaped bones goes into shearY).
 * Transform constraints (plugin 1.16.0): addTransform {bones, target, id?, name?, order?, mixRotate?,
 * mixX?, mixY?, mixScaleX?, mixScaleY?, mixShearY? (0..1), offsetRotation?, offsetX?, offsetY?,
 * offsetScaleX?, offsetScaleY?, offsetShearY?, local?, relative?, keepPose?} (extra.transform; keepPose
 * = offsets so nothing moves now) setTransform {transform, ...same (null = default)} renameTransform
 * {transform, id?, name?} (renames transform:<id> tracks) deleteTransform {transform}; keys setKey
 * {target: "transform:<id>", property: mixRotate..mixShearY}. deleteBone of a target needs cascade
 * (the constraint goes, ANIM_TRANSFORM_REMOVED); a constrained bone just leaves the list.
 * Bounding boxes / points (plugin 1.17.0): addBoundingBox {slot (new), bone?, from? (a part: right above
 * it, its picture's outline from `mask` + alphaThreshold?/detail?, else its rectangle), fromName?,
 * before? | index?, name? ("box"), vertices | worldVertices, color?} setBoundingBox {slot, name?, skin?,
 * vertices? | worldVertices?, color? (null removes)} addPoint {slot (new), bone, before? | index?, name?
 * ("point"), x?, y?, rotation? | worldX?, worldY?, worldRotation?} setPoint {slot, name?, skin?, ...same}.
 * `controllers` = [{id, data}] lets clip/skin/expression rename/delete report (and rewrite) controller refs.
 * Throws EditError when refused. An edit that turns a valid rig invalid is refused too.
 */
EditResult applyEdit(const Json& rig, const Json& operation, const Json& controllers = Json::array());

/**
 * Axis display spaces of a bone's x/y/rotation (plugin 1.7.0, editor fields and setBone `axes`);
 * values are always stored local. Local = as stored (x/y in the parent's space, rotation relative
 * to the parent). World = the origin in rig px and the on-screen angle of the bone's +x axis.
 * Parent = the origin's offset from the parent origin along the parent's unscaled axes (px) and the
 * angle from the parent's +x axis (equal to Local under an unscaled parent). Scale stays local.
 */
enum class Axes { Local, Parent, World };
std::optional<Axes> parseAxes(const std::string& name);  // "local" | "parent" | "world"
/** `local` shown in `axes` given the parent's world matrix (identity for a root). `inherit`
 *  (plugin 1.16.0) is the bone's mode: the world angle follows inheritFrame (roots: Normal). */
Transform toAxes(const Transform& local, const Affine& parentWorld, Axes axes, Inherit inherit = Inherit::Normal);
/** Inverse of toAxes; the rotation stays within ±180° of `hint` (the current local value).
 *  nullopt when the parent matrix is singular. */
std::optional<Transform> fromAxes(const Transform& shown, const Affine& parentWorld, Axes axes, const Transform& hint, Inherit inherit = Inherit::Normal);

/** Physics presets (plugin 1.13.0): [{id, label (Korean), fields}] for addPhysics/setPhysics `preset`. */
const Json& physicsPresets();
/** Decompose into x,y,rotation,scaleX,scaleY. Skew (non-uniform parent scale) is dropped; *skew gets its size. */
Transform decompose(const Affine& m, double* skew = nullptr);
/** Setup-pose world matrices keyed by bone id. Tolerates unknown parents (treated as roots) and cycles. */
std::map<std::string, Affine> setupWorld(const Json& rig);
/** Controller ids (from [{id,data}]) whose defaultClip, layers or bindings use `clip`. */
std::vector<std::string> controllerClipRefs(const Json& controllers, const std::string& clip);
bool validId(const std::string& id);
bool validName(const std::string& name);
/** Lowercase id derived from free text ("Eye L.png" -> "eye-l"), never empty. */
std::string slugId(const std::string& text, const std::string& fallback = "part");
}  // namespace cutout
