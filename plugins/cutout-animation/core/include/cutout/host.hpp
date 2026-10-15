// Multi-instance host: owns shared rigs and independent instances.
// Used directly by C++ hosts (Qt, tests) and through the C ABI by DLL/Wasm hosts.
#pragma once
#include "runtime.hpp"

namespace cutout {
class Host {
 public:
  /** Execute one JSON command. Always returns {ok:true,...} or {ok:false,error}. */
  Json request(const std::string& text);
  /** The C ABI entry (DLL / Wasm): request(text).dump(), except that compact `frame` replies
   *  (format 2, plugin 1.9.0) are written straight into the string without a JSON tree. */
  std::string requestText(const std::string& text);
  Json handle(const Json& request);
  Instance* instance(const std::string& id);

 private:
  struct Mount { double width = 0, height = 0, scale = 1; bool flipX = false; std::string ref, skin; };
  std::map<std::string, std::shared_ptr<const Rig>> rigs_;
  std::map<std::string, Json> controllers_;
  std::map<std::string, std::unique_ptr<Instance>> instances_;
  std::map<std::string, Mount> mounts_;
  // Compact frames (format 2): the last encoded items per instance (delta "0" = unchanged) and
  // the mesh topology table hosts cache once (uvs + triangles never change between frames).
  struct FrameCache { uint64_t serial = 0; std::vector<std::string> items; };
  std::map<std::string, FrameCache> frames_;
  uint64_t frameSerial_ = 0, meshEpoch_ = 1;
  // Entries keep their rig alive, so an id never points at freed (or reused) attachment memory.
  std::vector<std::pair<std::shared_ptr<const Rig>, const Attachment*>> meshList_;
  std::map<const Attachment*, int> meshIds_;
  Affine placement(const std::string& instance, const Rig& rig) const;
  void resetMeshes();
  int meshId(const std::shared_ptr<const Rig>& rig, const Attachment* attachment);
  Json meshTable(const Json& ids);
  std::string frameCompact(const std::string& instance, uint64_t since);
};
Json drawItemJson(const DrawItem& item);
/** Fixed-point decimal text of v (at most `decimals` fraction digits, trailing zeros dropped): the
 *  compact frame number format, identical on every platform for the same double. */
void appendFixed(std::string& out, double v, int decimals);
Json poseJson(const Rig& rig, const Pose& pose);
}  // namespace cutout
