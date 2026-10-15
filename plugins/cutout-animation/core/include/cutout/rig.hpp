// Cutout animation data contract (schemaVersion 1).
// Immutable rig data shared by every instance. Qt/SDL independent.
#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "curve.hpp"
#include "json.hpp"

namespace cutout {
using Json = nlohmann::json;

constexpr int SCHEMA_VERSION = 1;
constexpr const char* FORMAT_ID = "raising-cutout-v1";

/** 2D affine matrix: [a c e; b d f; 0 0 1]. Maps local -> parent. */
struct Affine {
  double a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;
  Affine operator*(const Affine& o) const {
    return {a * o.a + c * o.b, b * o.a + d * o.b, a * o.c + c * o.d, b * o.c + d * o.d,
            a * o.e + c * o.f + e, b * o.e + d * o.f + f};
  }
  void apply(double x, double y, double& ox, double& oy) const { ox = a * x + c * y + e; oy = b * x + d * y + f; }
  std::optional<Affine> inverse() const;
  /** translate * rotate(deg, clockwise on y-down screens) * scale */
  static Affine compose(double x, double y, double rotationDeg, double scaleX, double scaleY);
};

struct Transform { double x = 0, y = 0, rotation = 0, scaleX = 1, scaleY = 1; };

/** `length` (plugin 1.5.0, px >= 0, default 0 = point bone): the bone reaches from its origin to
 *  (length, 0) in its own space (+x, Spine convention). Not a transform: it never moves anything
 *  except the two-bone IK effector (IkConstraint::tipLength). `color` is an editor-only display
 *  hint ("#rrggbb" / "#rrggbbaa", "" = default); runtimes ignore it. */
struct Bone { std::string id; std::string parent; Transform setup; int parentIndex = -1; double length = 0; std::string color; };
struct Slot { std::string id; std::string bone; std::optional<std::string> attachment; double alpha = 1; int boneIndex = -1; };
/** One bone's share of a weighted mesh vertex; `bind` indexes Attachment::binds. */
struct Influence { int bind = -1; double weight = 0; };
constexpr size_t MAX_MESH_VERTICES = 2048, MAX_MESH_TRIANGLES = 4096, MAX_INFLUENCES = 4;
/**
 * Atlas region (plugin 1.9.0, written by the export packer, never by authoring): the attachment's
 * picture is the sub-image (x, y, width, height) of the image asset `page` (page pixels). `trim`
 * (optional, regions only) is the rectangle of the attachment image (its own units) that sub-image
 * shows; without it the sub-image is the whole picture. Draw items then name the page and carry
 * the region; transparent pixels trimmed away are simply not drawn, so nothing renders differently.
 */
struct AtlasRegion {
  std::string page;
  double x = 0, y = 0, width = 0, height = 0;
  bool trimmed = false;
  double trimX = 0, trimY = 0, trimWidth = 0, trimHeight = 0;
};
struct Attachment {
  std::string slot, name, imageAssetId;
  Transform offset;       // placement of the image pivot in bone space
  double pivotX = 0.5, pivotY = 0.5;  // normalized point of the image placed at offset
  double width = 0, height = 0;       // source image pixels (checked against PNG by the service)
  /**
   * Mesh (plugin 1.4.0, `type: "mesh"`). `vertices` are image-pixel positions of the setup shape
   * (x, y pairs, the same space as a region's pixels), `uvs` texture coordinates 0..1 and
   * `triangles` index triples. Unweighted, the mesh is placed exactly like a region (slot bone ·
   * offset · pivot). Weighted, every vertex follows up to 4 bones: the bind pose is the rig's
   * setup pose (forward kinematics of the bones' setup transforms), and `binds` holds per
   * influencing bone the map image px -> that bone's setup space (inverse bind · slot setup
   * world · placement), derived at parse time, so p = Σ weight · world(bone) · bind · (v + deform).
   */
  bool mesh = false;
  std::vector<double> vertices, uvs;
  std::vector<int> triangles;
  std::vector<std::vector<Influence>> weights;  // empty = unweighted; else one list per vertex (sum 1)
  std::vector<std::pair<int, Affine>> binds;
  /** Editor-only outline data, never used for drawing: the first `hull` vertices are the outline
   *  polygon in order (0 = none), `edges` index pairs of the outline and of internal edges the
   *  author pinned (plugin 1.6.0 mesh edits keep pinned edges through re-triangulation). */
  size_t hull = 0;
  std::vector<int> edges;
  std::optional<AtlasRegion> atlas;  // export-time atlas packing (plugin 1.9.0)
  size_t vertexCount() const { return vertices.size() / 2; }
};

/** Expression is the rig-level track `{"target":"rig","property":"expression"}` (step only).
 *  Mix / BendPositive key IK constraints: `{"target":"ik:<id>"}` (bend is step only, value bool).
 *  Deform keys mesh vertex offsets: `{"target":"slot:<id>","property":"deform","attachment":"<name>"}`. */
enum class Property { X, Y, Rotation, ScaleX, ScaleY, Alpha, Attachment, Expression, Mix, BendPositive, Deform };
enum class Interp { Step, Linear, Bezier };
/** `curve` is used only when interp is Bezier (segment from this key to the next).
 *  Deform keys carry `offsets` (dx, dy per vertex, image pixels) instead of `value`. */
struct Key { int64_t timeMs = 0; double value = 0; std::optional<std::string> name; Interp interp = Interp::Linear; Curve curve = LINEAR_CURVE; std::vector<double> offsets; };
/** bone=false + rig=true: the rig expression track (target "", targetIndex -1, key.name = expression id).
 *  bone=false + ik=true: an IK constraint track (target = constraint id, targetIndex = Rig::ik index;
 *  bendPositive keys carry value 1/0). Deform tracks are slot tracks naming `attachment`. */
struct Track { bool bone = true; bool rig = false; bool ik = false; std::string target; int targetIndex = -1; Property property = Property::X; std::vector<Key> keys; std::string attachment; };
/**
 * Rig-level event definition (plugin 1.8.0, Spine-like): optional default payload of every key
 * of that name. `audio` names an audio asset the host MAY play when the event fires (with
 * `volume` 0..1 and `balance` -1..1); players stay free to ignore it. Presentation only: clip
 * events never reach controller bindings or game rules.
 */
struct EventDef {
  std::string name;
  std::optional<int64_t> intValue;
  std::optional<double> floatValue;
  std::optional<std::string> stringValue, audio;
  double volume = 1, balance = 0;
};
/** A clip event key. `payload` is resolved at parse time (definition defaults, then the key's own
 *  int/float/string overrides; audio/volume/balance from the definition): only the fields that are
 *  set appear, so rigs without payloads keep the plugin 1.7 notification shape. */
struct ClipEvent { int64_t timeMs = 0; std::string name; Json payload = Json::object(); };
struct Clip { std::string id, name; int64_t durationMs = 1000; bool loop = true; std::vector<Track> tracks; std::vector<ClipEvent> events; };
struct MixPair { std::string from, to; int64_t durationMs = 0; };
/** Outfit/variant set: provides or overrides attachments by (slot, name); clips keep keying names. */
struct Skin { std::string id, name; std::vector<Attachment> attachments; };
/** Absolute override of listed bone channels (unlisted channels keep the animated value). */
struct BoneOverride { int bone = -1; std::optional<double> x, y, rotation, scaleX, scaleY; };
/** Named facial/pose set applied as an override layer (see Instance::pose for the order). */
struct Expression {
  std::string id, name;
  std::vector<std::pair<int, std::optional<std::string>>> slots;  // slot index -> attachment name (nullopt = hidden)
  std::vector<BoneOverride> bones;
};

/**
 * IK constraint (plugin 1.3.0). `bones` = 1 (aim: the bone-local direction `aimAxis` (plugin 1.4.1,
 * degrees, default 0 = +x, the direction a bone length points) points at the target) or 2 (parent,
 * direct child: two-bone limb). Two-bone effector precedence: the explicit `tip` bone's origin
 * (plugin 1.4.0, a direct child of the lower bone) > the lower bone's own tip (length, 0) when its
 * length > 0 (plugin 1.5.0) > its first child bone in array order > its origin. Solved after
 * expressions on world transforms, in `Rig::ikOrder`.
 */
struct IkConstraint {
  std::string id, name;
  std::vector<int> bones;     // chain, parent first
  int target = -1;
  int tip = -1;               // two-bone effector bone (-1 = none: tipLength, else the child origin aims at the target)
  double tipLength = 0;       // two-bone, tip < 0: effector (tipLength, 0) in the lower bone (its length, plugin 1.5.0)
  bool explicitTip = false;   // `tip` named in the data (else the implicit first-child rule)
  double mix = 1;
  double aimAxis = 0;         // aim: bone-local direction (degrees, before the bone's scale) aimed at the target
  bool bendPositive = true;   // two-bone: the child turns in the positive (clockwise on screen) direction
  bool stretch = false;       // accepted, not applied in 1.3.0 (warning ANIM_IK_STRETCH)
  int order = 0;
  std::vector<int> affected;  // bones[0] and its descendants, parents first: world recomputed after solving
};

struct Rig {
  std::string id;
  double originX = 0, originY = 0;  // instance anchor in rig space
  double width = 0, height = 0;     // authoring canvas size, used for placement boxes
  std::vector<Bone> bones;          // any order in JSON; evalOrder is parents-first
  std::vector<int> evalOrder;
  std::vector<Slot> slots;          // draw order: back -> front
  std::vector<Attachment> attachments;
  std::vector<Clip> clips;
  int64_t defaultMixMs = 0;
  std::vector<MixPair> mixes;
  std::vector<Skin> skins;
  std::vector<Expression> expressions;
  std::vector<IkConstraint> ik;
  std::vector<int> ikOrder;         // evaluation order: (order, array index) ascending
  std::vector<EventDef> events;     // event definitions (plugin 1.8.0); empty = any key name

  int boneIndex(const std::string& id) const;
  int slotIndex(const std::string& id) const;
  int clipIndex(const std::string& id) const;
  int skinIndex(const std::string& id) const;        // -1 = unknown / "" (default attachments only)
  int expressionIndex(const std::string& id) const;  // -1 = unknown / none
  int ikIndex(const std::string& id) const;
  int eventIndex(const std::string& name) const;     // -1 = not defined
  /** Lookup order: skin `skin` (when >= 0) first, then the rig's default attachments. */
  const Attachment* attachment(const std::string& slot, const std::string& name, int skin = -1) const;
  int64_t mixDuration(const std::string& from, const std::string& to) const;
};

struct Diagnostic {
  std::string code, severity, pointer, message;
  Json toJson() const { return {{"code", code}, {"severity", severity}, {"pointer", pointer}, {"message", message}}; }
};

/** Semantic validation of a rig `data` object. Never throws on bad input. */
std::vector<Diagnostic> validateRig(const Json& data);
/** Parse validated data. Throws std::runtime_error when validateRig reports errors. */
Rig parseRig(const std::string& id, const Json& data);
/** parseRig for data the caller has just validated (validateRig reported no error): skips the
 *  second validation pass, which costs as much as the parse on large rigs (plugin 1.9.0). */
Rig parseValidatedRig(const std::string& id, const Json& data);

const char* propertyName(Property p);
std::optional<Property> parseProperty(const std::string& name);
}  // namespace cutout
