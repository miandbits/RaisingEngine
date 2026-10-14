#include "cutout/host.hpp"
#include "cutout/signals.hpp"
#include "cutout/templates.hpp"
#include <algorithm>
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
std::optional<std::string> expressionOf(const Json& r) {
  if (!r.contains("expression") || r.at("expression").is_null()) return std::nullopt;
  if (!r.at("expression").is_string()) throw std::runtime_error("expression must be an id or null");
  return r.at("expression").get<std::string>();
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
  return {{"bones", bones}, {"slots", slots},
          {"skin", pose.skin >= 0 && pose.skin < int(rig.skins.size()) ? Json(rig.skins[size_t(pose.skin)].id) : Json(nullptr)},
          {"expression", pose.expression >= 0 && pose.expression < int(rig.expressions.size()) ? Json(rig.expressions[size_t(pose.expression)].id) : Json(nullptr)}};
}

Affine Host::placement(const std::string& id, const Rig& rig) const {
  auto it = mounts_.find(id);
  if (it == mounts_.end()) return {};
  const Mount& m = it->second;
  // Fit the authoring canvas into the element box ("contain"), centered; rig origin follows the canvas.
  const double rw = rig.width > 0 ? rig.width : m.width, rh = rig.height > 0 ? rig.height : m.height;
  const double s = (rw > 0 && rh > 0 ? std::min(m.width / rw, m.height / rh) : 1) * m.scale;
  const double tx = (m.width - rw * s) / 2, ty = (m.height - rh * s) / 2;
  Affine a = Affine::compose(tx, ty, 0, s, s) * Affine::compose(rig.originX, rig.originY, 0, 1, 1);
  if (m.flipX) a = Affine::compose(m.width, 0, 0, -1, 1) * a;
  return a;
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
  if (op == "destroy" || op == "unmount") { instances_.erase(str(r, "instance")); mounts_.erase(str(r, "instance")); return {{"ok", true}}; }
  if (op == "reset") { instances_.clear(); rigs_.clear(); controllers_.clear(); mounts_.clear(); return {{"ok", true}}; }
  // ---- Generic runtime-extension protocol (host knows only these ops) ----
  if (op == "init") {
    instances_.clear(); rigs_.clear(); controllers_.clear(); mounts_.clear();
    Json diagnostics = Json::array();
    const Json docs = r.value("documents", Json::array());
    for (const auto& d : docs) {
      const std::string type = d.value("type", std::string()), id = d.value("id", std::string());
      if (type == "rig") {
        try { rigs_[id] = std::make_shared<const Rig>(parseRig(id, d.at("data"))); }
        catch (const std::exception& e) { diagnostics.push_back({{"document", id}, {"message", e.what()}}); }
      }
    }
    for (const auto& d : docs) if (d.value("type", std::string()) == "controller") controllers_[d.value("id", std::string())] = d.at("data");
    return {{"ok", true}, {"api", 1}, {"diagnostics", diagnostics}};
  }
  if (op == "mount") {
    const std::string id = str(r, "instance");
    const Json element = r.value("element", Json::object());
    Mount m; m.ref = element.value("ref", std::string());
    m.width = element.value("width", 0.0); m.height = element.value("height", 0.0);
    const Json props = element.value("props", Json::object());
    m.scale = props.value("scale", 1.0); m.flipX = props.value("flipX", false);
    // props.skin picks the outfit of this placed instance (host passes props through untouched).
    m.skin = props.contains("skin") && props.at("skin").is_string() ? props.at("skin").get<std::string>() : "";
    if (element.value("type", std::string()) != "character") throw std::runtime_error("unknown element type");
    auto c = controllers_.find(m.ref);
    if (c == controllers_.end()) throw std::runtime_error("unknown controller: " + m.ref);
    auto rig = rigs_.find(c->second.value("rig", std::string()));
    if (rig == rigs_.end()) throw std::runtime_error("controller rig not loaded: " + c->second.value("rig", std::string()));
    if (instances_.size() >= MAX_INSTANCES && !instances_.count(id)) throw std::runtime_error("too many instances");
    const auto issues = validateController(c->second, rig->second.get());
    for (const auto& d : issues) if (d.severity == "error") throw std::runtime_error(d.code + " " + d.pointer + ": " + d.message);
    // Re-mounting the same element (screen rebuild) keeps its playback state (and a skin a
    // binding chose); only a changed props.skin is re-applied. Unknown skins are ignored here:
    // the service reports them, and the controller default stays.
    const bool fresh = !instances_.count(id) || mounts_[id].ref != m.ref;
    if (fresh) instances_[id] = std::make_unique<Instance>(rig->second, parseController(m.ref, c->second));
    if (!m.skin.empty() && (fresh || mounts_[id].skin != m.skin)) instances_[id]->setSkin(m.skin);
    mounts_[id] = m;
    return {{"ok", true}};
  }
  if (op == "saveAll") {
    Json state = Json::object();
    for (const auto& [id, inst] : instances_) state[id] = {{"ref", mounts_.count(id) ? mounts_.at(id).ref : ""}, {"state", inst->saveState()}};
    return {{"ok", true}, {"state", {{"version", 1}, {"instances", state}}}};
  }
  if (op == "restoreAll") {
    const Json state = r.value("state", Json::object());
    if (state.value("version", 0) != 1) throw std::runtime_error("unknown extension state version");
    Json skipped = Json::array();
    const Json saved = state.value("instances", Json::object());
    for (auto it = saved.begin(); it != saved.end(); ++it) {
      auto* inst = instance(it.key());
      // Only restore into an instance mounted with the same controller; anything else is stale.
      if (!inst || !mounts_.count(it.key()) || mounts_.at(it.key()).ref != it.value().value("ref", std::string())) { skipped.push_back(it.key()); continue; }
      inst->restoreState(it.value().at("state"));
    }
    return {{"ok", true}, {"skipped", skipped}};
  }
  if (op == "describeAll") {
    Json out = Json::object();
    for (const auto& [id, inst] : instances_) out[id] = inst->describe();
    return {{"ok", true}, {"instances", out}};
  }
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
  // Preview / host-driven look changes ("" or null skin = default attachments only).
  if (op == "setSkin") {
    const std::string skin = r.contains("skin") && r.at("skin").is_string() ? r.at("skin").get<std::string>() : "";
    if (!need("instance").setSkin(skin)) throw std::runtime_error("unknown skin: " + skin);
    return {{"ok", true}};
  }
  if (op == "setExpression") {
    const auto expression = expressionOf(r);
    if (!need("instance").setExpression(expression)) throw std::runtime_error("unknown expression: " + expression.value_or(""));
    return {{"ok", true}};
  }
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
    auto& inst = need("instance");
    // Mounted instances get element-local pixels; raw instances stay in rig-origin space.
    const Affine place = placement(str(r, "instance"), inst.rig());
    Json items = Json::array();
    for (auto item : inst.drawList()) { item.matrix = place * item.matrix; items.push_back(drawItemJson(item)); }
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
    Look look;
    if (r.contains("skin") && r.at("skin").is_string() && !r.at("skin").get<std::string>().empty() && (look.skin = rig.skinIndex(r.at("skin").get<std::string>())) < 0)
      throw std::runtime_error("unknown skin: " + r.at("skin").get<std::string>());
    if (const auto e = expressionOf(r); e && (look.expression = rig.expressionIndex(*e)) < 0) throw std::runtime_error("unknown expression: " + *e);
    return {{"ok", true}, {"pose", poseJson(rig, samplePose(rig, clip, r.value("timeMs", 0.0), look))}};
  }
  // ---- Authoring ops (pure data in/out; shared by the Qt editor and the service/LLM path) ----
  if (op == "template") {
    const Json clip = templateClip(str(r, "kind"), r.at("rig"), r.value("params", Json::object()), str(r, "clipId"));
    return {{"ok", true}, {"clip", clip}};
  }
  if (op == "edit") {
    const EditResult result = applyEdit(r.at("rig"), r.at("operation"), r.value("controllers", Json::array()));
    Json reply{{"ok", true}, {"rig", result.data}, {"diagnostics", diagnosticsJson(result.diagnostics, "")}, {"affected", result.affected}};
    for (auto it = result.extra.begin(); it != result.extra.end(); ++it) reply[it.key()] = it.value();
    return reply;
  }
  throw std::runtime_error("unknown op: " + op);
}
}  // namespace cutout
