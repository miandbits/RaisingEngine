#include "cutout/host.hpp"
#include "cutout/signals.hpp"
#include "cutout/templates.hpp"
#include <algorithm>
#include <cmath>
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
  Json j{{"slot", i.slot}, {"imageAssetId", i.imageAssetId}, {"matrix", {i.matrix.a, i.matrix.b, i.matrix.c, i.matrix.d, i.matrix.e, i.matrix.f}},
         {"width", i.width}, {"height", i.height}, {"alpha", i.alpha}};
  // ABI extension (CONTRACT.md §7, plugin-mesh-v1): mesh items add triangles; matrix stays the fallback rectangle.
  if (i.mesh()) j["mesh"] = {{"positions", i.positions}, {"uvs", i.uvs}, {"indices", i.indices}};
  // Atlas (plugin-atlas-v1): the item's picture is this sub-image of imageAssetId.
  if (i.hasRegion) j["region"] = {i.region[0], i.region[1], i.region[2], i.region[3]};
  return j;
}

void appendFixed(std::string& out, double v, int decimals) {
  static const int64_t scale[] = {1, 10, 100, 1000, 10000, 100000, 1000000};
  if (!std::isfinite(v) || decimals < 0 || decimals > 6) { out += '0'; return; }
  // Integer arithmetic after one rounding: no locale, no printf, the same text on every toolchain.
  const double limit = 1e12;
  int64_t q = std::llround(std::clamp(v, -limit, limit) * double(scale[decimals]));
  if (q == 0) { out += '0'; return; }
  if (q < 0) { out += '-'; q = -q; }
  const int64_t whole = q / scale[decimals];
  int64_t frac = q % scale[decimals];
  out += std::to_string(whole);
  if (!frac) return;
  int digits = decimals;
  while (frac % 10 == 0) { frac /= 10; digits--; }
  const std::string f = std::to_string(frac);
  out += '.';
  out.append(size_t(digits) - f.size(), '0');
  out += f;
}

namespace {
/** One compact (format 2) item: [image, a, b, c, d, e, f, width, height, alpha(, region|null(, meshId, positions))]. */
std::string encodeItem(const DrawItem& i, int mesh) {
  std::string s = "[";
  s += Json(i.imageAssetId).dump();
  const double m[6] = {i.matrix.a, i.matrix.b, i.matrix.c, i.matrix.d, i.matrix.e, i.matrix.f};
  // a..d scale a whole picture (1e-6 keeps a 4096 px edge within 0.005 px); offsets/sizes/vertices in 1/1000 px.
  for (int k = 0; k < 6; k++) { s += ','; appendFixed(s, m[k], k < 4 ? 6 : 3); }
  s += ','; appendFixed(s, i.width, 3);
  s += ','; appendFixed(s, i.height, 3);
  s += ','; appendFixed(s, i.alpha, 4);
  if (i.hasRegion || i.mesh()) {
    if (i.hasRegion) {
      s += ",[";
      for (int k = 0; k < 4; k++) { if (k) s += ','; appendFixed(s, i.region[k], 3); }
      s += ']';
    } else s += ",null";
  }
  if (i.mesh()) {
    s += ',' + std::to_string(mesh) + ",[";
    for (size_t k = 0; k < i.positions.size(); k++) { if (k) s += ','; appendFixed(s, i.positions[k], 3); }
    s += ']';
  }
  s += ']';
  return s;
}
}  // namespace

void Host::resetMeshes() {
  meshList_.clear();
  meshIds_.clear();
  dynamicIds_.clear();
  meshEpoch_++;
  // Stable ids for the init lifetime: rigs by id, then each rig's default list and skins in order.
  for (const auto& [id, rig] : rigs_) {
    for (const auto& a : rig->attachments) if (a.mesh) meshId(rig, &a);
    for (const auto& skin : rig->skins) for (const auto& a : skin.attachments) if (a.mesh) meshId(rig, &a);
  }
}
int Host::meshId(const std::shared_ptr<const Rig>& rig, const Attachment* a) {
  auto it = meshIds_.find(a);
  if (it != meshIds_.end()) return it->second;
  meshList_.push_back({rig, a, {}, {}});
  return meshIds_[a] = int(meshList_.size() - 1);
}
int Host::dynamicMeshId(const DrawItem& item) {
  // Exact bytes as the key: equal topology (a clip that did not move relative to its parts) keeps its id.
  std::string key(reinterpret_cast<const char*>(item.uvs.data()), item.uvs.size() * sizeof(double));
  key.append(reinterpret_cast<const char*>(item.indices.data()), item.indices.size() * sizeof(int));
  auto it = dynamicIds_.find(key);
  if (it != dynamicIds_.end()) return it->second;
  meshList_.push_back({nullptr, nullptr, item.uvs, item.indices});
  return dynamicIds_[key] = int(meshList_.size() - 1);
}
Json Host::meshTable(const Json& ids) {
  Json out = Json::array();
  auto entry = [&](size_t id) {
    const MeshEntry& e = meshList_[id];
    return e.attachment ? Json{{"id", id}, {"uvs", e.attachment->uvs}, {"indices", e.attachment->triangles}} : Json{{"id", id}, {"uvs", e.uvs}, {"indices", e.indices}};
  };
  if (ids.is_array()) {
    for (const auto& v : ids) if (v.is_number_integer() && v.get<int64_t>() >= 0 && size_t(v.get<int64_t>()) < meshList_.size()) out.push_back(entry(size_t(v.get<int64_t>())));
  } else for (size_t i = 0; i < meshList_.size(); i++) out.push_back(entry(i));
  return out;
}
std::string Host::frameCompact(const std::string& id, uint64_t since) {
  auto* inst = instance(id);
  if (!inst) throw std::runtime_error("unknown instance: " + id);
  // Moving clips add a topology per frame: restart the table (new epoch: hosts drop their cache and
  // fetch what they miss) before it grows without bound. Only between frames, so a reply never
  // mixes ids of two epochs; cached items (delta "0") would name old ids, so they go too.
  if (dynamicIds_.size() > MAX_DYNAMIC_MESHES) { resetMeshes(); frames_.clear(); }
  const Affine place = placement(id, inst->rig());
  FrameCache& cache = frames_[id];
  // Delta only against the frame the host says it holds; anything else gets every item.
  const bool delta = since != 0 && since == cache.serial;
  const uint64_t serial = ++frameSerial_;
  std::string out = "{\"ok\":true,\"format\":2,\"meshEpoch\":" + std::to_string(meshEpoch_) + ",\"frame\":" + std::to_string(serial) + ",\"items\":[";
  std::vector<std::string> encoded;
  size_t k = 0;
  for (auto item : inst->drawList()) {
    item.matrix = place * item.matrix;
    for (size_t v = 0; v + 1 < item.positions.size(); v += 2) {
      const double x = item.positions[v], y = item.positions[v + 1];
      place.apply(x, y, item.positions[v], item.positions[v + 1]);
    }
    std::string e = encodeItem(item, !item.mesh() ? -1 : item.clipped ? dynamicMeshId(item) : meshId(inst->sharedRig(), item.source));
    if (k) out += ',';
    out += delta && k < cache.items.size() && cache.items[k] == e ? std::string("0") : e;
    encoded.push_back(std::move(e));
    k++;
  }
  out += "]}";
  cache.serial = serial;
  cache.items = std::move(encoded);
  return out;
}
Json poseJson(const Rig& rig, const Pose& pose) {
  Json bones = Json::object(), slots = Json::object();
  for (size_t i = 0; i < rig.bones.size(); i++) {
    const auto& t = pose.bones[i]; const auto& w = pose.world[i];
    bones[rig.bones[i].id] = {{"x", t.x}, {"y", t.y}, {"rotation", t.rotation}, {"scaleX", t.scaleX}, {"scaleY", t.scaleY},
                              {"world", {w.a, w.b, w.c, w.d, w.e, w.f}}};
    // Shear (plugin 1.16.0): only when sampled non-zero, so replies of older rigs keep their shape.
    if (t.shearX != 0) bones[rig.bones[i].id]["shearX"] = t.shearX;
    if (t.shearY != 0) bones[rig.bones[i].id]["shearY"] = t.shearY;
  }
  for (size_t i = 0; i < rig.slots.size(); i++) {
    slots[rig.slots[i].id] = {{"attachment", pose.attachments[i] ? Json(*pose.attachments[i]) : Json(nullptr)}, {"alpha", pose.alpha[i]}};
    // Sequences (plugin 1.12.0): the frame index the shown attachment draws; absent otherwise.
    const Attachment* a = pose.attachments[i] ? rig.attachment(rig.slots[i].id, *pose.attachments[i], pose.skin) : nullptr;
    if (a && a->sequence()) slots[rig.slots[i].id]["sequence"] = attachmentFrame(pose, int(i), *a);
  }
  Json out = {{"bones", bones}, {"slots", slots},
              {"skin", pose.skin >= 0 && pose.skin < int(rig.skins.size()) ? Json(rig.skins[size_t(pose.skin)].id) : Json(nullptr)},
              {"expression", pose.expression >= 0 && pose.expression < int(rig.expressions.size()) ? Json(rig.expressions[size_t(pose.expression)].id) : Json(nullptr)}};
  // IK (plugin 1.3.0): bone x..scaleY stay the animated values, `world` is solved; the solved local
  // rotation is `appliedRotation`. Rigs without IK keep the 1.2 reply shape.
  if (!rig.ik.empty()) {
    Json ik = Json::object();
    for (size_t i = 0; i < rig.ik.size(); i++)
      ik[rig.ik[i].id] = {{"mix", i < pose.ikMix.size() ? pose.ikMix[i] : rig.ik[i].mix}, {"bendPositive", i < pose.ikBend.size() ? pose.ikBend[i] != 0 : rig.ik[i].bendPositive}};
    out["ik"] = ik;
    for (size_t i = 0; i < rig.bones.size(); i++) out["bones"][rig.bones[i].id]["appliedRotation"] = pose.local(i).rotation;
    // Chains and stretch (plugin 1.13.0): only where a constraint stretched or ran FABRIK, so
    // replies of 1.12 rigs keep their shape.
    for (size_t i = 0; i < rig.ik.size() && i < pose.ikSolve.size(); i++) {
      const IkSolve& s = pose.ikSolve[i];
      if (rig.ik[i].bones.size() > 2) ik[rig.ik[i].id]["iterations"] = s.iterations, ik[rig.ik[i].id]["error"] = s.error;
      if (s.stretch != 1) ik[rig.ik[i].id]["stretch"] = s.stretch;
      for (int b : rig.ik[i].bones) if (pose.local(size_t(b)).scaleX != pose.bones[size_t(b)].scaleX) out["bones"][rig.bones[size_t(b)].id]["appliedScaleX"] = pose.local(size_t(b)).scaleX;
    }
    out["ik"] = ik;
  }
  // Transform constraints (plugin 1.16.0): the sampled channel mixes, and the local values the
  // constraints left on every bone they move (`applied`, rotation..shearY).
  if (!rig.transforms.empty()) {
    static const char* const keys[TRANSFORM_MIXES] = {"mixRotate", "mixX", "mixY", "mixScaleX", "mixScaleY", "mixShearY"};
    Json transform = Json::object();
    for (size_t i = 0; i < rig.transforms.size(); i++) {
      Json mixes = Json::object();
      for (size_t m = 0; m < TRANSFORM_MIXES; m++) mixes[keys[m]] = i < pose.transformMix.size() ? pose.transformMix[i][m] : rig.transforms[i].mix[m];
      transform[rig.transforms[i].id] = mixes;
      for (int b : rig.transforms[i].bones) {
        const Transform& a = pose.local(size_t(b));
        out["bones"][rig.bones[size_t(b)].id]["applied"] = {{"x", a.x}, {"y", a.y}, {"rotation", a.rotation}, {"scaleX", a.scaleX}, {"scaleY", a.scaleY}, {"shearX", a.shearX}, {"shearY", a.shearY}};
      }
    }
    out["transform"] = transform;
  }
  // Physics (plugin 1.13.0): the sampled mixes. A sampled pose has no simulation (stateless).
  if (!rig.physics.empty()) {
    Json physics = Json::object();
    for (size_t i = 0; i < rig.physics.size(); i++) physics[rig.physics[i].id] = {{"mix", i < pose.physicsMix.size() ? pose.physicsMix[i] : rig.physics[i].mix}};
    out["physics"] = physics;
  }
  // Deform (plugin 1.4.0): sampled mesh offsets as "<slot>/<attachment>" -> [dx, dy, ...]; absent without deform keys.
  if (!pose.deform.empty()) {
    Json deform = Json::object();
    for (const auto& [key, offsets] : pose.deform)
      if (key.first >= 0 && size_t(key.first) < rig.slots.size()) deform[rig.slots[size_t(key.first)].id + "/" + key.second] = offsets;
    out["deform"] = deform;
  }
  return out;
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
std::string Host::requestText(const std::string& text) {
  try {
    const Json r = Json::parse(text);
    // The per-frame hot path: no reply tree, numbers written once (format 2, plugin 1.9.0).
    if (r.is_object() && r.value("op", std::string()) == "frame" && r.value("format", 1) == 2)
      return frameCompact(str(r, "instance"), r.contains("since") && r.at("since").is_number_unsigned() ? r.at("since").get<uint64_t>() : 0);
    return handle(r).dump();
  } catch (const std::exception& error) {
    return Json({{"ok", false}, {"error", error.what()}}).dump();
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
      if (!bad) rig = parseValidatedRig("validate", r.at("rig"));  // just validated: no second pass
    }
    if (r.contains("controller"))
      for (auto& d : diagnosticsJson(validateController(r.at("controller"), rig ? &*rig : nullptr), r.value("controllerPointer", std::string()))) diagnostics.push_back(d);
    Json reply{{"ok", true}, {"diagnostics", diagnostics}};
    // Every controller of the rig in one request (plugin 1.9.0): the rig is validated and parsed
    // once instead of once per controller. One diagnostics list per entry, in order.
    if (r.contains("controllers") && r.at("controllers").is_array()) {
      Json each = Json::array();
      for (const auto& c : r.at("controllers"))
        each.push_back(diagnosticsJson(validateController(c.is_object() ? c.value("data", Json()) : Json(), rig ? &*rig : nullptr), c.is_object() ? c.value("pointer", std::string()) : std::string()));
      reply["controllerDiagnostics"] = each;
    }
    return reply;
  }
  if (op == "loadRig") {
    rigs_[str(r, "id")] = std::make_shared<const Rig>(parseRig(str(r, "id"), r.at("data")));
    resetMeshes();
    return {{"ok", true}};
  }
  if (op == "unloadRig") {
    for (const auto& [id, inst] : instances_) if (inst->rig().id == str(r, "id")) throw std::runtime_error("rig still in use by " + id);
    rigs_.erase(str(r, "id"));
    resetMeshes();
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
  if (op == "destroy" || op == "unmount") { instances_.erase(str(r, "instance")); mounts_.erase(str(r, "instance")); frames_.erase(str(r, "instance")); return {{"ok", true}}; }
  if (op == "reset") { instances_.clear(); rigs_.clear(); controllers_.clear(); mounts_.clear(); frames_.clear(); resetMeshes(); return {{"ok", true}}; }
  // ---- Generic runtime-extension protocol (host knows only these ops) ----
  if (op == "init") {
    instances_.clear(); rigs_.clear(); controllers_.clear(); mounts_.clear(); frames_.clear();
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
    resetMeshes();
    Json reply{{"ok", true}, {"api", 1}, {"diagnostics", diagnostics}};
    // Optional requests this plugin answers (plugin 1.17.0, CONTRACT.md §7): hosts call `hitTest`
    // on clicks and `points` for anchors only when listed, so older plugins are never asked.
    reply["ops"] = {"points", "hitTest"};
    // Frame format negotiation (CONTRACT.md §7, plugin 1.9.0): a host listing 2 in frameFormats gets
    // the mesh topology table now and may request compact frames; others keep the documented items.
    if (r.contains("frameFormats") && r.at("frameFormats").is_array())
      for (const auto& f : r.at("frameFormats"))
        if (f == 2) { reply["frameFormat"] = 2; reply["meshEpoch"] = meshEpoch_; reply["meshes"] = meshTable(nullptr); }
    return reply;
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
    if (fresh || mounts_[id].width != m.width || mounts_[id].height != m.height || mounts_[id].scale != m.scale || mounts_[id].flipX != m.flipX) frames_.erase(id);
    if (!m.skin.empty() && (fresh || mounts_[id].skin != m.skin)) instances_[id]->setSkin(m.skin);
    mounts_[id] = m;
    // `interactive` (plugin 1.17.0): the rig has bounding boxes, so clicks on the element may matter
    // (hosts may skip hit tests of other elements). Absent otherwise: older replies keep their shape.
    Json reply{{"ok", true}};
    const Rig& mounted = *rig->second;
    auto anyBox = [](const std::vector<Attachment>& list) { return std::any_of(list.begin(), list.end(), [](const Attachment& a) { return a.boundingBox; }); };
    bool boxes = anyBox(mounted.attachments);
    for (const auto& skin : mounted.skins) boxes = boxes || anyBox(skin.attachments);
    if (boxes) reply["interactive"] = true;
    return reply;
  }
  if (op == "saveAll") {
    Json state = Json::object();
    for (const auto& [id, inst] : instances_) state[id] = {{"ref", mounts_.count(id) ? mounts_.at(id).ref : ""}, {"state", inst->saveState(r.value("exact", false))}};
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
  // Physics (plugin 1.13.0, optional for hosts): preview wind/gravity, and a reset a host may send
  // when it moves an instance far (players never need either; the core detects bone jumps itself).
  if (op == "setPhysicsEnvironment") {
    const double wind = r.value("wind", 0.0), gravity = r.value("gravity", 0.0);
    if (!std::isfinite(wind) || !std::isfinite(gravity)) throw std::runtime_error("wind/gravity must be finite");
    for (auto& [id, inst] : instances_) if (!r.contains("instance") || r.at("instance") == id) inst->setPhysicsEnvironment(wind, gravity);
    return {{"ok", true}};
  }
  if (op == "resetPhysics") { need("instance").resetPhysics(); return {{"ok", true}}; }
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
  if (op == "frame" && r.value("format", 1) == 2)
    return Json::parse(frameCompact(str(r, "instance"), r.contains("since") && r.at("since").is_number_unsigned() ? r.at("since").get<uint64_t>() : 0));
  // Mesh topology of compact frames: {ids?} -> [{id, uvs, indices}] (all when ids is absent).
  if (op == "meshes") return {{"ok", true}, {"meshEpoch", meshEpoch_}, {"meshes", meshTable(r.contains("ids") ? r.at("ids") : Json())}};
  if (op == "frame") {
    auto& inst = need("instance");
    // Mounted instances get element-local pixels; raw instances stay in rig-origin space.
    const Affine place = placement(str(r, "instance"), inst.rig());
    Json items = Json::array();
    for (auto item : inst.drawList()) {
      item.matrix = place * item.matrix;
      for (size_t v = 0; v + 1 < item.positions.size(); v += 2) {
        const double x = item.positions[v], y = item.positions[v + 1];
        place.apply(x, y, item.positions[v], item.positions[v + 1]);
      }
      items.push_back(drawItemJson(item));
    }
    return {{"ok", true}, {"items", items}};
  }
  // ---- Bounding boxes and points (plugin 1.17.0): the space of `frame` (element px when mounted,
  // rig-origin px otherwise), the current pose. Nothing here changes playback except a dispatching
  // hitTest, which is a presentation signal like any other (never game state).
  if (op == "points" || op == "boxes" || op == "hitTest") {
    auto& inst = need("instance");
    const Rig& rig = inst.rig();
    const Affine place = placement(str(r, "instance"), rig) * Affine::compose(-rig.originX, -rig.originY, 0, 1, 1);
    const Pose pose = inst.pose();
    if (op == "points") {
      Json list = Json::array();
      for (const auto& p : pointWorlds(rig, pose)) {
        double x, y;
        place.apply(p.x, p.y, x, y);
        const double rad = p.rotation * 3.14159265358979323846 / 180, dx = place.a * std::cos(rad) + place.c * std::sin(rad), dy = place.b * std::cos(rad) + place.d * std::sin(rad);
        list.push_back({{"slot", rig.slots[size_t(p.slot)].id}, {"name", p.attachment->name}, {"x", x}, {"y", y}, {"rotation", std::atan2(dy, dx) * 180 / 3.14159265358979323846}});
      }
      return {{"ok", true}, {"points", list}};
    }
    std::vector<BoxWorld> boxes = boxPolygons(rig, pose);
    for (auto& b : boxes)
      for (size_t v = 0; v + 1 < b.polygon.size(); v += 2) { const double x = b.polygon[v], y = b.polygon[v + 1]; place.apply(x, y, b.polygon[v], b.polygon[v + 1]); }
    auto ref = [&](const BoxWorld& b) { return Json{{"slot", rig.slots[size_t(b.slot)].id}, {"name", b.attachment->name}}; };
    if (op == "boxes") {
      Json list = Json::array();
      for (const auto& b : boxes) { Json e = ref(b); e["polygon"] = b.polygon; list.push_back(e); }
      return {{"ok", true}, {"boxes", list}};
    }
    if (!r.contains("x") || !r.contains("y") || !r.at("x").is_number() || !r.at("y").is_number()) throw std::runtime_error("hitTest needs numbers x, y");
    const double x = r.at("x").get<double>(), y = r.at("y").get<double>();
    // Every box under the point, topmost (last drawn) first; the first is the hit.
    Json hits = Json::array();
    for (size_t k = boxes.size(); k-- > 0;) if (polygonContains(boxes[k].polygon, x, y)) hits.push_back(ref(boxes[k]));
    Json reply{{"ok", true}, {"hit", hits.empty() ? Json(nullptr) : hits[0]}, {"hits", hits}};
    // dispatch: the click becomes the presentation signal `partClicked {box, slot}` of that instance.
    if (r.value("dispatch", false)) {
      Json matched = Json::array();
      if (!hits.empty())
        for (const auto& id : inst.signal("partClicked", {{"box", hits[0].at("name").get<std::string>()}, {"slot", hits[0].at("slot").get<std::string>()}})) matched.push_back(id);
      reply["matched"] = matched;
    }
    return reply;
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
