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
 * Apply one operation {op, ...}. Ops: addBone setBone reparentBone setPivot deleteBone
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
 * mix?, bendPositive?, order?} setIk {ik, name?, mix?, bendPositive?, order?, target?, bones?}
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
 * `controllers` = [{id, data}] lets clip/skin/expression rename/delete report (and rewrite) controller refs.
 * Throws EditError when refused. An edit that turns a valid rig invalid is refused too.
 */
EditResult applyEdit(const Json& rig, const Json& operation, const Json& controllers = Json::array());

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
