#include "cutout/host.hpp"
#include "cutout/signals.hpp"
#include <stdexcept>

namespace cutout {
namespace {
constexpr size_t MAX_INSTANCES = 256;
Json diagnosticsJson(const std::vector<Diagnostic>& items, const std::string& prefix) {
  Json out = Json::array();
  for (auto d : items) { d.pointer = prefix + d.pointer; out.push_back(d.toJson()); }
  return out;
}
const std::string& str(const Json& r, const char* key) {
  if (!r.contains(key) || !r.at(key).is_string()) throw std::runtime_error(std::string("missing string field: ") + key);
  return r.at(key).get_ref<const std::string&>();
}
Then thenOf(const Json& r) {
  const std::string t = r.value("then", std::string("default"));
  return t == "hold" ? Then::Hold : t == "stop" ? Then::Stop : Then::Default;
}
}  // namespace

Json drawItemJson(const DrawItem& i) {
  return {{"slot", i.slot}, {"imageAssetId", i.imageAssetId}, {"matrix", {i.matrix.a, i.matrix.b, i.matrix.c, i.matrix.d, i.matrix.e, i.matrix.f}},
          {"width", i.width}, {"height", i.height}, {"alpha", i.alpha}};
}
Json poseJson(const Rig& rig, const Pose& pose) {
  Json bones = Json::object(), slots = Json::object();
  for (size_t i = 0; i < rig.bones.size(); i++) {
    const auto& t = pose.bones[i]; const auto& w = pose.world[i];
    bones[rig.bones[i].id] = {{"x", t.x}, {"y", t.y}, {"rotation", t.rotation}, {"scaleX", t.scaleX}, {"scaleY", t.scaleY},
                              {"world", {w.a, w.b, w.c, w.d, w.e, w.f}}};
  }
  for (size_t i = 0; i < rig.slots.size(); i++)
    slots[rig.slots[i].id] = {{"attachment", pose.attachments[i] ? Json(*pose.attachments[i]) : Json(nullptr)}, {"alpha", pose.alpha[i]}};
  return {{"bones", bones}, {"slots", slots}};
}

Instance* Host::instance(const std::string& id) {
  auto it = instances_.find(id);
  return it == instances_.end() ? nullptr : it->second.get();
}

Json Host::request(const std::string& text) {
  try {
    return handle(Json::parse(text));
  } catch (const std::exception& error) {
    return {{"ok", false}, {"error", error.what()}};
  }
}

Json Host::handle(const Json& r) {
  const std::string& op = str(r, "op");
  auto need = [&](const char* key) -> Instance& {
    auto* i = instance(str(r, key));
    if (!i) throw std::runtime_error("unknown instance: " + str(r, key));
    return *i;
  };
  if (op == "version") return {{"ok", true}, {"api", 1}, {"format", FORMAT_ID}, {"schemaVersion", SCHEMA_VERSION}};
  if (op == "validate") {
    Json diagnostics = Json::array();
    std::optional<Rig> rig;
    if (r.contains("rig")) {
      const auto issues = validateRig(r.at("rig"));
      diagnostics = diagnosticsJson(issues, r.value("rigPointer", std::string()));
      bool bad = false; for (const auto& d : issues) bad |= d.severity == "error";
      if (!bad) rig = parseRig("validate", r.at("rig"));
    }
    if (r.contains("controller"))
      for (auto& d : diagnosticsJson(validateController(r.at("controller"), rig ? &*rig : nullptr), r.value("controllerPointer", std::string()))) diagnostics.push_back(d);
    return {{"ok", true}, {"diagnostics", diagnostics}};
  }
  if (op == "loadRig") {
    rigs_[str(r, "id")] = std::make_shared<const Rig>(parseRig(str(r, "id"), r.at("data")));
    return {{"ok", true}};
  }
  if (op == "unloadRig") {
    for (const auto& [id, inst] : instances_) if (inst->rig().id == str(r, "id")) throw std::runtime_error("rig still in use by " + id);
    rigs_.erase(str(r, "id"));
    return {{"ok", true}};
  }
  if (op == "create") {
    auto rig = rigs_.find(str(r, "rig"));
    if (rig == rigs_.end()) throw std::runtime_error("unknown rig: " + str(r, "rig"));
    if (instances_.size() >= MAX_INSTANCES && !instances_.count(str(r, "instance"))) throw std::runtime_error("too many instances");
    std::optional<Controller> controller;
    if (r.contains("controller") && r.at("controller").is_object()) {
      const auto issues = validateController(r.at("controller"), rig->second.get());
      for (const auto& d : issues) if (d.severity == "error") throw std::runtime_error(d.code + " " + d.pointer + ": " + d.message);
      controller = parseController(r.value("controllerId", std::string("controller")), r.at("controller"));
    }
    instances_[str(r, "instance")] = std::make_unique<Instance>(rig->second, controller);
    return {{"ok", true}};
  }
  if (op == "destroy") { instances_.erase(str(r, "instance")); return {{"ok", true}}; }
  if (op == "reset") { instances_.clear(); rigs_.clear(); return {{"ok", true}}; }
  if (op == "signal") {
    std::map<std::string, std::string> params;
    if (r.contains("params") && r.at("params").is_object())
      for (auto it = r.at("params").begin(); it != r.at("params").end(); ++it) if (it.value().is_string()) params[it.key()] = it.value().get<std::string>();
    Json matched = Json::array();
    if (r.contains("instance")) for (const auto& id : need("instance").signal(str(r, "name"), params)) matched.push_back(id);
    else for (auto& [id, inst] : instances_) for (const auto& b : inst->signal(str(r, "name"), params)) matched.push_back(id + ":" + b);
    return {{"ok", true}, {"matched", matched}};
  }
  if (op == "observe") {
    // Shared game->presentation adapter: hosts pass committed transitions, never frames.
    const auto signals = deriveSignals(r.value("before", Json::object()), r.value("after", Json::object()), r.value("command", Json::object()),
                                       r.contains("error") && r.at("error").is_string() ? r.at("error").get<std::string>() : "");
    Json list = Json::array(), matched = Json::array();
    for (const auto& s : signals) {
      list.push_back(signalJson(s));
      if (r.value("dispatch", true)) for (auto& [id, inst] : instances_) for (const auto& b : inst->signal(s.name, s.params)) matched.push_back(id + ":" + b);
    }
    return {{"ok", true}, {"signals", list}, {"matched", matched}};
  }
  if (op == "play") {
    auto& inst = need("instance");
    std::optional<bool> loop; if (r.contains("loop") && r.at("loop").is_boolean()) loop = r.at("loop").get<bool>();
    const auto serial = inst.play(r.value("track", 0), str(r, "clip"), loop, r.value("speed", 1.0), thenOf(r), r.value("mixMs", int64_t(-1)));
    if (!serial) throw std::runtime_error("unknown clip or track: " + str(r, "clip"));
    return {{"ok", true}, {"serial", serial}};
  }
  if (op == "stop") { need("instance").stop(r.value("track", 0), r.value("mixMs", int64_t(-1))); return {{"ok", true}}; }
  if (op == "seek") { need("instance").seek(r.value("track", 0), r.value("timeMs", 0.0)); return {{"ok", true}}; }
  if (op == "setPaused") { need("instance").setPaused(r.value("paused", true)); return {{"ok", true}}; }
  if (op == "setTimeScale") { need("instance").setTimeScale(r.value("timeScale", 1.0)); return {{"ok", true}}; }
  if (op == "update") {
    const double dt = r.value("dtMs", 0.0);
    Json notes = Json::object();
    for (auto& [id, inst] : instances_) {
      if (r.contains("instance") && r.at("instance") != id) continue;
      inst->update(dt);
      Json list = Json::array();
      for (const auto& n : inst->takeNotifications()) list.push_back(n.toJson());
      if (!list.empty()) notes[id] = list;
    }
    return {{"ok", true}, {"notifications", notes}};
  }
  if (op == "frame") {
    Json items = Json::array();
    for (const auto& item : need("instance").drawList()) items.push_back(drawItemJson(item));
    return {{"ok", true}, {"items", items}};
  }
  if (op == "save") return {{"ok", true}, {"state", need("instance").saveState()}};
  if (op == "restore") { need("instance").restoreState(r.at("state")); return {{"ok", true}}; }
  if (op == "describe") return {{"ok", true}, {"state", need("instance").describe()}};
  if (op == "notifications") {
    Json list = Json::array();
    for (const auto& n : need("instance").takeNotifications()) list.push_back(n.toJson());
    return {{"ok", true}, {"notifications", list}};
  }
  if (op == "sample") {
    const Rig rig = parseRig("sample", r.at("rig"));
    const int clip = r.contains("clip") && r.at("clip").is_string() ? rig.clipIndex(r.at("clip").get<std::string>()) : -1;
    return {{"ok", true}, {"pose", poseJson(rig, samplePose(rig, clip, r.value("timeMs", 0.0)))}};
  }
  throw std::runtime_error("unknown op: " + op);
}
}  // namespace cutout
