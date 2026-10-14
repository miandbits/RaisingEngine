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
 * setCurve addClip duplicateClip renameClip setClip deleteClip setMix setCanvas setName
 * applySetupDeltaToKeys batch.
 * Interpolation (setKey/setInterp): interp step|linear|bezier (+curve [x1,y1,x2,y2]) or a
 * preset name (easeIn easeOut easeInOut auto flat back) as `interp` or `preset`; setCurve
 * sets one key's handles (x clamped to 0..1). Many keys = one `batch`.
 * `controllers` = [{id, data}] lets clip rename/delete report (and rewrite) controller refs.
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
