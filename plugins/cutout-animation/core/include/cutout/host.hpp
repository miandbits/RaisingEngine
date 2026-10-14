// Multi-instance host: owns shared rigs and independent instances.
// Used directly by C++ hosts (Qt, tests) and through the C ABI by DLL/Wasm hosts.
#pragma once
#include "runtime.hpp"

namespace cutout {
class Host {
 public:
  /** Execute one JSON command. Always returns {ok:true,...} or {ok:false,error}. */
  Json request(const std::string& text);
  Json handle(const Json& request);
  Instance* instance(const std::string& id);

 private:
  std::map<std::string, std::shared_ptr<const Rig>> rigs_;
  std::map<std::string, std::unique_ptr<Instance>> instances_;
};
Json drawItemJson(const DrawItem& item);
Json poseJson(const Rig& rig, const Pose& pose);
}  // namespace cutout
