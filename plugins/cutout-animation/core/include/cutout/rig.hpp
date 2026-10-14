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

struct Bone { std::string id; std::string parent; Transform setup; int parentIndex = -1; };
struct Slot { std::string id; std::string bone; std::optional<std::string> attachment; double alpha = 1; int boneIndex = -1; };
struct Attachment {
  std::string slot, name, imageAssetId;
  Transform offset;       // placement of the image pivot in bone space
  double pivotX = 0.5, pivotY = 0.5;  // normalized point of the image placed at offset
  double width = 0, height = 0;       // source image pixels (checked against PNG by the service)
};

enum class Property { X, Y, Rotation, ScaleX, ScaleY, Alpha, Attachment };
enum class Interp { Step, Linear, Bezier };
/** `curve` is used only when interp is Bezier (segment from this key to the next). */
struct Key { int64_t timeMs = 0; double value = 0; std::optional<std::string> name; Interp interp = Interp::Linear; Curve curve = LINEAR_CURVE; };
struct Track { bool bone = true; std::string target; int targetIndex = -1; Property property = Property::X; std::vector<Key> keys; };
struct ClipEvent { int64_t timeMs = 0; std::string name; };
struct Clip { std::string id, name; int64_t durationMs = 1000; bool loop = true; std::vector<Track> tracks; std::vector<ClipEvent> events; };
struct MixPair { std::string from, to; int64_t durationMs = 0; };

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

  int boneIndex(const std::string& id) const;
  int slotIndex(const std::string& id) const;
  int clipIndex(const std::string& id) const;
  const Attachment* attachment(const std::string& slot, const std::string& name) const;
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

const char* propertyName(Property p);
std::optional<Property> parseProperty(const std::string& name);
}  // namespace cutout
