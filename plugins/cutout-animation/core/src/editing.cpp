#include "cutout/editing.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <set>
#include "cutout/curve.hpp"
#include "cutout/runtime.hpp"

namespace cutout {
namespace {
constexpr double PI = 3.14159265358979323846;
using Str = std::string;

// Round away float noise (1e-15) so documents stay readable and diff-friendly.
double clean(double v) {
  const double r = std::round(v * 1e6) / 1e6;
  return r == 0 ? 0.0 : r;
}
Transform readT(const Json& o) {
  Transform t;
  auto get = [&](const char* k, double f) { return o.is_object() && o.contains(k) && o.at(k).is_number() ? o.at(k).get<double>() : f; };
  t.x = get("x", 0); t.y = get("y", 0); t.rotation = get("rotation", 0); t.scaleX = get("scaleX", 1); t.scaleY = get("scaleY", 1);
  return t;
}
void writeT(Json& o, const Transform& t) {
  o["x"] = clean(t.x); o["y"] = clean(t.y);
  auto opt = [&](const char* k, double v, double def) { v = clean(v); if (v == def) o.erase(k); else o[k] = v; };
  opt("rotation", t.rotation, 0); opt("scaleX", t.scaleX, 1); opt("scaleY", t.scaleY, 1);
}
Affine local(const Transform& t) { return Affine::compose(t.x, t.y, t.rotation, t.scaleX, t.scaleY); }

Json& arr(Json& d, const char* k) {
  if (!d.contains(k) || !d.at(k).is_array()) d[k] = Json::array();
  return d[k];
}
/** Read-only list lookup that never adds the key (optional skins/expressions stay absent). */
const Json& listOr(const Json& d, const char* k) {
  static const Json empty = Json::array();
  return d.is_object() && d.contains(k) && d.at(k).is_array() ? d.at(k) : empty;
}
const Str& reqStr(const Json& op, const char* k) {
  if (!op.contains(k) || !op.at(k).is_string() || op.at(k).get_ref<const Str&>().empty()) throw EditError(Str(k) + " 값이 필요합니다.");
  return op.at(k).get_ref<const Str&>();
}
Str optStr(const Json& op, const char* k, const Str& def = "") { return op.contains(k) && op.at(k).is_string() ? op.at(k).get<Str>() : def; }
bool has(const Json& op, const char* k) { return op.is_object() && op.contains(k) && !op.at(k).is_null(); }
double num(const Json& op, const char* k, double def) {
  if (!has(op, k)) return def;
  const auto& v = op.at(k);
  if (!v.is_number() || !std::isfinite(v.get<double>()) || std::fabs(v.get<double>()) > 1e6) throw EditError(Str(k) + " 값은 유한한 숫자여야 합니다.");
  return v.get<double>();
}
int64_t ms(const Json& op, const char* k) {
  if (!has(op, k) || !op.at(k).is_number()) throw EditError(Str(k) + " 시간(ms)이 필요합니다.");
  const double v = op.at(k).get<double>();
  if (!std::isfinite(v) || v != std::floor(v)) throw EditError(Str(k) + " 시간은 정수 ms여야 합니다.");
  return int64_t(v);
}
int findIndex(const Json& a, const char* key, const Str& id) {
  if (!a.is_array()) return -1;
  for (size_t i = 0; i < a.size(); i++) if (a[i].is_object() && a[i].value(key, Str()) == id) return int(i);
  return -1;
}
Json& bone(Json& d, const Str& id) {
  const int i = findIndex(arr(d, "bones"), "id", id);
  if (i < 0) throw EditError("없는 본입니다: " + id);
  return d["bones"][size_t(i)];
}
Json& slot(Json& d, const Str& id) {
  const int i = findIndex(arr(d, "slots"), "id", id);
  if (i < 0) throw EditError("없는 슬롯(조각)입니다: " + id);
  return d["slots"][size_t(i)];
}
Json& clip(Json& d, const Str& id) {
  const int i = findIndex(arr(d, "clips"), "id", id);
  if (i < 0) throw EditError("없는 클립입니다: " + id);
  return d["clips"][size_t(i)];
}
Str parentOf(const Json& b) { return b.contains("parent") && b.at("parent").is_string() ? b.at("parent").get<Str>() : ""; }
bool hasErrors(const std::vector<Diagnostic>& ds) { for (const auto& x : ds) if (x.severity == "error") return true; return false; }
void warn(EditResult& r, const Str& code, const Str& pointer, const Str& message) { r.diagnostics.push_back({code, "warning", pointer, message}); }

/** Nearest equivalent decomposition to `hint` (keeps mirror sign and rotation winding). */
Transform decomposeNear(const Affine& m, const Transform& hint, double* skew) {
  Transform t = decompose(m, skew);
  if (hint.scaleX < 0 && t.scaleX > 0) { t.scaleX = -t.scaleX; t.scaleY = -t.scaleY; t.rotation -= 180; }
  // Rotation keys interpolate numerically, so stay within ±180 of the previous value.
  while (t.rotation - hint.rotation > 180) t.rotation -= 360;
  while (t.rotation - hint.rotation < -180) t.rotation += 360;
  return t;
}
Affine inverseOr(const Affine& m, const char* what) {
  auto inv = m.inverse();
  if (!inv) throw EditError(Str(what) + "의 크기가 0이라 계산할 수 없습니다.");
  return *inv;
}

Json* findTrack(Json& c, const Str& target, const Str& property) {
  for (auto& t : arr(c, "tracks")) if (t.value("target", Str()) == target && t.value("property", Str()) == property) return &t;
  return nullptr;
}
Curve curveOf(const Json& key) {
  Curve c = LINEAR_CURVE;
  if (key.contains("curve") && key.at("curve").is_array() && key.at("curve").size() == 4)
    for (size_t i = 0; i < 4; i++) if (key.at("curve")[i].is_number()) c[i] = key.at("curve")[i].get<double>();
  return c;
}
Json curveJson(const Curve& c) { return Json::array({clean(c[0]), clean(c[1]), clean(c[2]), clean(c[3])}); }
// Json-level sampler (mirrors runtime sampleTrack) so edits work on not-yet-parsed data.
double sampleJson(const Json* track, double t, double fallback) {
  if (!track || !track->contains("keys") || !track->at("keys").is_array() || track->at("keys").empty()) return fallback;
  const auto& keys = track->at("keys");
  auto timeOf = [&](size_t i) { return keys[i].value("timeMs", 0.0); };
  auto valueOf = [&](size_t i) { return keys[i].contains("value") && keys[i].at("value").is_number() ? keys[i].at("value").get<double>() : fallback; };
  if (t <= timeOf(0)) return valueOf(0);
  if (t >= timeOf(keys.size() - 1)) return valueOf(keys.size() - 1);
  for (size_t i = 0; i + 1 < keys.size(); i++) {
    if (t < timeOf(i + 1)) {
      const Str interp = keys[i].contains("interp") && keys[i].at("interp").is_string() ? keys[i].at("interp").get<Str>() : "linear";
      if (interp == "step") return valueOf(i);
      const double a = timeOf(i), b = timeOf(i + 1), u = (t - a) / (b - a);
      return valueOf(i) + (valueOf(i + 1) - valueOf(i)) * (interp == "bezier" ? evalCurve(curveOf(keys[i]), u) : u);
    }
  }
  return valueOf(keys.size() - 1);
}
void sortKeys(Json& track) {
  auto& keys = arr(track, "keys");
  std::stable_sort(keys.begin(), keys.end(), [](const Json& a, const Json& b) { return a.value("timeMs", int64_t(0)) < b.value("timeMs", int64_t(0)); });
}
bool targetHasKeys(Json& d, const Str& target) {
  for (auto& c : arr(d, "clips")) for (auto& t : arr(c, "tracks")) if (t.value("target", Str()) == target && !arr(t, "keys").empty()) return true;
  return false;
}
void removeTracks(Json& d, const std::set<Str>& targets) {
  for (auto& c : arr(d, "clips")) {
    auto& tracks = arr(c, "tracks");
    tracks.erase(std::remove_if(tracks.begin(), tracks.end(), [&](const Json& t) { return targets.count(t.value("target", Str())) > 0; }), tracks.end());
  }
}
/** Names a slot can show: the default list plus every skin (an outfit-only part is keyable too). */
std::set<Str> attachmentNames(Json& d, const Str& slotId) {
  std::set<Str> out;
  for (auto& a : arr(d, "attachments")) if (a.value("slot", Str()) == slotId) out.insert(a.value("name", Str()));
  for (const auto& s : listOr(d, "skins"))
    for (const auto& a : listOr(s, "attachments")) if (optStr(a, "slot") == slotId) out.insert(optStr(a, "name"));
  return out;
}
/** Deleted slots/bones leave no dangling skin attachments or expression entries behind. */
void purgeLookRefs(Json& d, const std::set<Str>& slots, const std::set<Str>& bones) {
  if (d.contains("skins") && d["skins"].is_array())
    for (auto& s : d["skins"]) {
      if (!s.is_object() || !s.contains("attachments") || !s["attachments"].is_array()) continue;
      auto& list = s["attachments"];
      list.erase(std::remove_if(list.begin(), list.end(), [&](const Json& a) { return slots.count(optStr(a, "slot")) > 0; }), list.end());
    }
  if (d.contains("expressions") && d["expressions"].is_array())
    for (auto& e : d["expressions"]) {
      if (!e.is_object()) continue;
      for (const auto& [key, ids] : {std::pair<const char*, const std::set<Str>*>{"slots", &slots}, {"bones", &bones}}) {
        if (!e.contains(key) || !e[key].is_object()) continue;
        for (const auto& id : *ids) e[key].erase(id);
        if (e[key].empty()) e.erase(key);
      }
    }
}
void ensureUniqueBone(Json& d, const Str& id) {
  if (!validId(id)) throw EditError("ID는 소문자 영문·숫자·하이픈 형식이어야 합니다: " + id);
  if (findIndex(arr(d, "bones"), "id", id) >= 0) throw EditError("이미 있는 본 ID입니다: " + id);
}
void ensureUniqueClip(Json& d, const Str& id) {
  if (!validId(id)) throw EditError("클립 ID는 소문자 영문·숫자·하이픈 형식이어야 합니다: " + id);
  if (findIndex(arr(d, "clips"), "id", id) >= 0) throw EditError("이미 있는 클립 ID입니다: " + id);
}
struct Target { bool bone; Str id; Property property; bool rig = false; };
Target target(Json& d, const Json& op) {
  const Str target = reqStr(op, "target"), prop = reqStr(op, "property");
  const auto p = parseProperty(prop);
  if (!p) throw EditError("알 수 없는 속성입니다: " + prop);
  // The rig-level track keys the active expression (step only).
  if (target == "rig") {
    if (*p != Property::Expression) throw EditError("rig 트랙의 속성은 expression뿐입니다.");
    return {false, "", *p, true};
  }
  if (*p == Property::Expression) throw EditError("표정 키의 대상은 rig입니다(target: \"rig\").");
  const bool isBone = target.rfind("bone:", 0) == 0, isSlot = target.rfind("slot:", 0) == 0;
  if (!isBone && !isSlot) throw EditError("트랙 대상은 bone:<id> 또는 slot:<id> 형식입니다.");
  const Str id = target.substr(5);
  if (isBone) bone(d, id); else slot(d, id);
  const bool slotProp = *p == Property::Alpha || *p == Property::Attachment;
  if (isBone == slotProp) throw EditError("대상 종류에 맞지 않는 속성입니다: " + prop);
  return {isBone, id, *p};
}
Str keyRef(const Str& clipId, const Json& op, int64_t t) { return "key:" + clipId + "/" + op.value("target", Str()) + "/" + op.value("property", Str()) + "@" + std::to_string(t); }
Json& keyAt(Json& track, int64_t t) {
  for (auto& k : arr(track, "keys")) if (k.value("timeMs", int64_t(-1)) == t) return k;
  throw EditError("그 시간에 키가 없습니다: " + std::to_string(t) + "ms");
}
Curve readCurve(const Json& op) {
  if (!has(op, "curve") || !op.at("curve").is_array() || op.at("curve").size() != 4) throw EditError("curve는 숫자 4개 [x1, y1, x2, y2]여야 합니다.");
  Curve c;
  for (size_t i = 0; i < 4; i++) {
    const Json& v = op.at("curve")[i];
    if (!v.is_number() || !std::isfinite(v.get<double>())) throw EditError("curve 값은 유한한 숫자여야 합니다.");
    c[i] = v.get<double>();
  }
  // Clamping x (not refusing) lets a handle dragged past the next key still mean "as late as possible".
  return clampCurve(c);
}
/** "auto" handles for the segment starting at `t`, from the track's current keys. */
Curve autoCurveAt(const Json& track, int64_t t) {
  std::vector<double> times, values;
  size_t at = 0;
  for (const auto& k : track.at("keys")) {
    if (!k.contains("value") || !k.at("value").is_number()) continue;
    if (k.value("timeMs", int64_t(-1)) == t) at = times.size();
    times.push_back(k.value("timeMs", 0.0));
    values.push_back(k.at("value").get<double>());
  }
  return autoCurve(times, values, at);
}
/**
 * Interpolation fields of a key from op {interp?, preset?, curve?}; interp may itself be a
 * preset name. linear (default) and step drop `curve`, so data never carries stale handles.
 * Returns false when the op says nothing about interpolation (existing interp is kept).
 */
bool writeInterp(Json& key, const Json& op, const Json& track) {
  Str interp = optStr(op, "interp");
  if (has(op, "preset")) interp = reqStr(op, "preset");
  if (interp.empty() && has(op, "curve")) interp = "bezier";
  if (interp.empty()) return false;
  if (interp == "linear" || interp == "step") {
    if (interp == "linear") key.erase("interp"); else key["interp"] = "step";
    key.erase("curve");
    return true;
  }
  Curve c;
  if (interp == "bezier") c = readCurve(op);
  else if (interp == "auto") c = autoCurveAt(track, key.value("timeMs", int64_t(-1)));
  else if (auto preset = presetCurve(interp)) c = *preset;
  else throw EditError("보간은 step, linear, bezier 또는 프리셋(easeIn, easeOut, easeInOut, auto, flat, back)입니다: " + interp);
  key["interp"] = "bezier";
  key["curve"] = curveJson(c);
  return true;
}

// ---- Bones ----
void addBone(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str id = reqStr(op, "id");
  ensureUniqueBone(d, id);
  const Str parent = optStr(op, "parent");
  Transform t;
  if (!parent.empty()) bone(d, parent);
  if (has(op, "worldX") || has(op, "worldY")) {
    // Place the new origin at a rig-space point (canvas click) regardless of parent transform.
    const Affine inv = parent.empty() ? Affine{} : inverseOr(setupWorld(d)[parent], "부모 본");
    inv.apply(num(op, "worldX", 0), num(op, "worldY", 0), t.x, t.y);
  } else { t.x = num(op, "x", 0); t.y = num(op, "y", 0); }
  t.rotation = num(op, "rotation", 0); t.scaleX = num(op, "scaleX", 1); t.scaleY = num(op, "scaleY", 1);
  Json b{{"id", id}, {"parent", parent.empty() ? Json(nullptr) : Json(parent)}};
  writeT(b, t);
  arr(d, "bones").push_back(b);
  r.affected.push_back("bone:" + id);
}
void setBone(EditResult& r, const Json& op) {
  const Str id = reqStr(op, "bone");
  Json& b = bone(r.data, id);
  Transform t = readT(b);
  t.x = num(op, "x", t.x); t.y = num(op, "y", t.y); t.rotation = num(op, "rotation", t.rotation);
  t.scaleX = num(op, "scaleX", t.scaleX); t.scaleY = num(op, "scaleY", t.scaleY);
  writeT(b, t);
  r.affected.push_back("bone:" + id);
}
void reparentBone(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str id = reqStr(op, "bone"), parent = optStr(op, "parent");
  Json& b = bone(d, id);
  if (parent == parentOf(b)) return;
  if (parent == id) throw EditError("본을 자기 자신 아래로 옮길 수 없습니다.");
  if (!parent.empty()) {
    bone(d, parent);
    std::map<Str, Str> parents;
    for (auto& x : arr(d, "bones")) parents[x.value("id", Str())] = parentOf(x);
    size_t guard = 0;
    for (Str cur = parent; !cur.empty() && guard++ <= parents.size(); cur = parents.count(cur) ? parents[cur] : "")
      if (cur == id) throw EditError("자손 본 아래로 옮기면 부모 관계가 순환합니다.");
  }
  auto world = setupWorld(d);
  const Affine inv = parent.empty() ? Affine{} : inverseOr(world[parent], "새 부모 본");
  double skew = 0;
  // Keep the world pose: new local = newParentWorld^-1 * oldWorld.
  writeT(b, decomposeNear(inv * world[id], readT(b), &skew));
  b["parent"] = parent.empty() ? Json(nullptr) : Json(parent);
  if (skew > 1e-6) warn(r, "ANIM_REPARENT_SKEW", "", "부모의 비균일 크기 때문에 기울임이 생겨 모양이 조금 달라질 수 있습니다.");
  if (targetHasKeys(d, "bone:" + id)) warn(r, "ANIM_REPARENT_KEYS", "", "이 본의 기존 애니메이션 키는 이전 부모 기준 값입니다. 필요하면 키를 다시 맞추세요.");
  r.affected.push_back("bone:" + id);
}
void setPivot(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str id = reqStr(op, "bone");
  Json& b = bone(d, id);
  double px = 0, py = 0;  // new origin expressed in the bone's current local space
  if (has(op, "worldX") || has(op, "worldY")) inverseOr(setupWorld(d)[id], "본").apply(num(op, "worldX", 0), num(op, "worldY", 0), px, py);
  else { px = num(op, "localX", 0); py = num(op, "localY", 0); }
  if (std::fabs(px) < 1e-9 && std::fabs(py) < 1e-9) return;
  const Transform setup = readT(b);
  Transform t = setup;
  local(setup).apply(px, py, t.x, t.y);  // L' = L * T(p): origin moves, rotation/scale stay
  writeT(b, t);
  // Everything expressed in this bone's space shifts by -p so it stays where it was.
  std::vector<Str> children;
  for (auto& c : arr(d, "bones")) if (parentOf(c) == id) {
    Transform ct = readT(c); ct.x -= px; ct.y -= py; writeT(c, ct);
    children.push_back(c.value("id", Str()));
  }
  std::set<Str> slotsOn;
  for (auto& s : arr(d, "slots")) if (s.value("bone", Str()) == id) slotsOn.insert(s.value("id", Str()));
  for (auto& a : arr(d, "attachments")) if (slotsOn.count(a.value("slot", Str()))) {
    a["x"] = clean(num(a, "x", 0) - px); a["y"] = clean(num(a, "y", 0) - py);
  }
  // Keys are absolute local values: child offsets shift exactly; the bone's own position
  // shifts by R(t)S(t)p using the clip's rotation/scale at each key time.
  const Str self = "bone:" + id;
  for (auto& c : arr(d, "clips")) {
    Json* rot = findTrack(c, self, "rotation"); Json* sx = findTrack(c, self, "scaleX"); Json* sy = findTrack(c, self, "scaleY");
    for (const char* axis : {"x", "y"}) {
      Json* tr = findTrack(c, self, axis);
      if (!tr) continue;
      for (auto& k : arr(*tr, "keys")) {
        if (!k.contains("value") || !k.at("value").is_number()) continue;
        const double tm = k.value("timeMs", 0.0);
        const Affine m = Affine::compose(0, 0, sampleJson(rot, tm, setup.rotation), sampleJson(sx, tm, setup.scaleX), sampleJson(sy, tm, setup.scaleY));
        double ox, oy; m.apply(px, py, ox, oy);
        k["value"] = clean(k.at("value").get<double>() + (axis[0] == 'x' ? ox : oy));
      }
    }
    for (const auto& child : children) for (const char* axis : {"x", "y"}) {
      Json* tr = findTrack(c, "bone:" + child, axis);
      if (!tr) continue;
      for (auto& k : arr(*tr, "keys")) if (k.contains("value") && k.at("value").is_number()) k["value"] = clean(k.at("value").get<double>() - (axis[0] == 'x' ? px : py));
    }
  }
  r.affected.push_back("bone:" + id);
  for (const auto& c : children) r.affected.push_back("bone:" + c);
  for (const auto& s : slotsOn) r.affected.push_back("slot:" + s);
}
void deleteBone(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str id = reqStr(op, "bone");
  Json& b = bone(d, id);
  if (arr(d, "bones").size() <= 1) throw EditError("마지막 본은 삭제할 수 없습니다.");
  std::set<Str> slotsOn;
  for (auto& s : arr(d, "slots")) if (s.value("bone", Str()) == id) slotsOn.insert(s.value("id", Str()));
  if (!slotsOn.empty() && !op.value("cascade", false))
    throw EditError("이 본에 조각 " + std::to_string(slotsOn.size()) + "개가 붙어 있습니다. 함께 삭제하려면 조각도 삭제를 선택하세요.");
  const Str parent = parentOf(b);
  auto world = setupWorld(d);
  const Affine inv = parent.empty() ? Affine{} : inverseOr(world[parent], "부모 본");
  for (auto& c : arr(d, "bones")) if (parentOf(c) == id) {
    // Children move up one level and keep their world pose.
    writeT(c, decomposeNear(inv * world[c.value("id", Str())], readT(c), nullptr));
    c["parent"] = parent.empty() ? Json(nullptr) : Json(parent);
    r.affected.push_back("bone:" + c.value("id", Str()));
  }
  std::set<Str> targets{"bone:" + id};
  for (const auto& s : slotsOn) targets.insert("slot:" + s);
  auto& slots = arr(d, "slots");
  slots.erase(std::remove_if(slots.begin(), slots.end(), [&](const Json& s) { return slotsOn.count(s.value("id", Str())) > 0; }), slots.end());
  auto& atts = arr(d, "attachments");
  atts.erase(std::remove_if(atts.begin(), atts.end(), [&](const Json& a) { return slotsOn.count(a.value("slot", Str())) > 0; }), atts.end());
  removeTracks(d, targets);
  purgeLookRefs(d, slotsOn, {id});
  auto& bones = arr(d, "bones");
  bones.erase(bones.begin() + findIndex(bones, "id", id));
  r.affected.push_back("bone:" + id);
  for (const auto& s : slotsOn) r.affected.push_back("slot:" + s);
}

// ---- Slots / parts ----
void addPart(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str slotId = reqStr(op, "slot"), asset = reqStr(op, "imageAssetId");
  if (!validId(slotId)) throw EditError("조각 ID는 소문자 영문·숫자·하이픈 형식이어야 합니다: " + slotId);
  const double w = num(op, "width", 0), h = num(op, "height", 0);
  if (w <= 0 || h <= 0) throw EditError("이미지 크기를 알 수 없습니다(0보다 커야 합니다).");
  const int si = findIndex(arr(d, "slots"), "id", slotId);
  Str boneId = optStr(op, "bone");
  if (si >= 0) {
    const Str owner = d["slots"][size_t(si)].value("bone", Str());
    if (!boneId.empty() && boneId != owner) throw EditError("이미 다른 본에 붙은 조각 ID입니다: " + slotId);
    boneId = owner;
  } else {
    if (boneId.empty()) throw EditError("조각을 붙일 본을 선택하세요.");
    bone(d, boneId);
  }
  const auto names = attachmentNames(d, slotId);
  Str base = optStr(op, "name", si < 0 ? slotId : slugId(asset, slotId));
  if (!validName(base)) throw EditError("첨부 이름 형식이 올바르지 않습니다: " + base);
  Str name = base;
  for (int n = 2; names.count(name); n++) name = base + "-" + std::to_string(n);
  if (has(op, "name") && name != base) throw EditError("같은 조각에 이미 있는 첨부 이름입니다: " + base);
  Json att{{"slot", slotId}, {"name", name}, {"imageAssetId", asset}, {"width", clean(w)}, {"height", clean(h)}};
  double x = num(op, "x", 0), y = num(op, "y", 0);
  if (has(op, "worldX") || has(op, "worldY")) inverseOr(setupWorld(d)[boneId], "본").apply(num(op, "worldX", 0), num(op, "worldY", 0), x, y);
  att["x"] = clean(x); att["y"] = clean(y);
  att["pivotX"] = clean(num(op, "pivotX", 0.5)); att["pivotY"] = clean(num(op, "pivotY", 0.5));
  arr(d, "attachments").push_back(att);
  if (si < 0) {
    auto& slots = arr(d, "slots");
    const int64_t index = std::clamp<int64_t>(op.value("index", int64_t(slots.size())), 0, int64_t(slots.size()));
    slots.insert(slots.begin() + index, Json{{"id", slotId}, {"bone", boneId}, {"attachment", name}});
  }
  r.affected.push_back("slot:" + slotId);
  r.extra["attachment"] = name;
}
void setSlot(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str id = reqStr(op, "slot");
  Json& s = slot(d, id);
  if (op.contains("attachment")) {
    if (op.at("attachment").is_null()) s["attachment"] = nullptr;
    else if (!attachmentNames(d, id).count(optStr(op, "attachment"))) throw EditError("이 조각에 없는 첨부입니다: " + optStr(op, "attachment"));
    else s["attachment"] = op.at("attachment");
  }
  if (has(op, "alpha")) {
    const double a = num(op, "alpha", 1);
    if (a < 0 || a > 1) throw EditError("투명도는 0..1이어야 합니다.");
    if (a == 1) s.erase("alpha"); else s["alpha"] = clean(a);
  }
  const Str newBone = optStr(op, "bone");
  if (!newBone.empty() && newBone != s.value("bone", Str())) {
    bone(d, newBone);
    auto world = setupWorld(d);
    const Affine from = world[s.value("bone", Str())], inv = inverseOr(world[newBone], "새 본");
    // Attachments keep their on-screen placement under the new bone.
    for (auto& a : arr(d, "attachments")) if (a.value("slot", Str()) == id) {
      const Transform old = readT(a);
      const Transform t = decomposeNear(inv * from * local(old), old, nullptr);
      a["x"] = clean(t.x); a["y"] = clean(t.y);
      auto opt = [&](const char* k, double v, double def) { v = clean(v); if (v == def) a.erase(k); else a[k] = v; };
      opt("rotation", t.rotation, 0); opt("scaleX", t.scaleX, 1); opt("scaleY", t.scaleY, 1);
    }
    s["bone"] = newBone;
  }
  r.affected.push_back("slot:" + id);
}
void setAttachment(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str id = reqStr(op, "slot"), name = reqStr(op, "name");
  slot(d, id);
  for (auto& a : arr(d, "attachments")) {
    if (a.value("slot", Str()) != id || a.value("name", Str()) != name) continue;
    for (const char* k : {"x", "y", "rotation", "scaleX", "scaleY", "pivotX", "pivotY", "width", "height"})
      if (has(op, k)) a[k] = clean(num(op, k, 0));
    if (has(op, "imageAssetId")) a["imageAssetId"] = reqStr(op, "imageAssetId");
    r.affected.push_back("slot:" + id);
    return;
  }
  throw EditError("없는 첨부입니다: " + id + "/" + name);
}
void deleteSlot(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str id = reqStr(op, "slot");
  slot(d, id);
  auto& slots = arr(d, "slots");
  slots.erase(slots.begin() + findIndex(slots, "id", id));
  auto& atts = arr(d, "attachments");
  atts.erase(std::remove_if(atts.begin(), atts.end(), [&](const Json& a) { return a.value("slot", Str()) == id; }), atts.end());
  removeTracks(d, {"slot:" + id});
  purgeLookRefs(d, {id}, {});
  r.affected.push_back("slot:" + id);
}
void moveSlot(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str id = reqStr(op, "slot");
  slot(d, id);
  auto& slots = arr(d, "slots");
  const int64_t from = findIndex(slots, "id", id), n = int64_t(slots.size());
  int64_t to = op.contains("index") ? op.value("index", from) : from + op.value("delta", int64_t(0));
  to = std::clamp<int64_t>(to, 0, n - 1);
  if (to == from) return;
  Json moved = slots[size_t(from)];
  slots.erase(slots.begin() + from);
  slots.insert(slots.begin() + to, moved);
  r.affected.push_back("slot:" + id);
}

// ---- Keys ----
void setKey(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str clipId = reqStr(op, "clip");
  const Target tg = target(d, op);
  Json& c = clip(d, clipId);
  const int64_t t = ms(op, "timeMs"), duration = c.value("durationMs", int64_t(0));
  if (t < 0 || t > duration) throw EditError("키 시간이 클립 범위(0.." + std::to_string(duration) + "ms) 밖입니다.");
  if (!op.contains("value")) throw EditError("키 값이 필요합니다.");
  Json value = op.at("value");
  if (tg.property == Property::Expression) {
    if (!value.is_null() && (!value.is_string() || findIndex(listOr(d, "expressions"), "id", value.get<Str>()) < 0))
      throw EditError("없는 표정입니다: " + (value.is_string() ? value.get<Str>() : value.dump()));
  } else if (tg.property == Property::Attachment) {
    if (!value.is_null() && (!value.is_string() || !attachmentNames(d, tg.id).count(value.get<Str>())))
      throw EditError("이 조각에 없는 첨부입니다: " + (value.is_string() ? value.get<Str>() : value.dump()));
  } else {
    const double v = num(op, "value", 0);
    if (!value.is_number()) throw EditError("키 값은 숫자여야 합니다.");
    if (tg.property == Property::Alpha && (v < 0 || v > 1)) throw EditError("투명도는 0..1이어야 합니다.");
    value = clean(v);
  }
  const Str targetText = op.at("target").get<Str>(), prop = op.at("property").get<Str>();
  Json* tr = findTrack(c, targetText, prop);
  if (!tr) { arr(c, "tracks").push_back({{"target", targetText}, {"property", prop}, {"keys", Json::array()}}); tr = &arr(c, "tracks").back(); }
  Json* key = nullptr;
  for (auto& k : arr(*tr, "keys")) if (k.value("timeMs", int64_t(-1)) == t) key = &k;
  if (!key) { arr(*tr, "keys").push_back({{"timeMs", t}}); key = &arr(*tr, "keys").back(); }
  (*key)["value"] = value;
  const bool stepOnly = tg.property == Property::Attachment || tg.property == Property::Expression;
  if (stepOnly) { key->erase("interp"); key->erase("curve"); }
  sortKeys(*tr);
  // After sorting: "auto" handles read the neighbours including this key's new value.
  if (!stepOnly) writeInterp(keyAt(*tr, t), op, *tr);
  r.affected.push_back(keyRef(clipId, op, t));
}
Json& existingTrack(Json& d, const Json& op, Json*& clipOut) {
  const Str clipId = reqStr(op, "clip");
  target(d, op);
  clipOut = &clip(d, clipId);
  Json* tr = findTrack(*clipOut, op.at("target").get<Str>(), op.at("property").get<Str>());
  if (!tr) throw EditError("이 대상·속성에는 키가 없습니다.");
  return *tr;
}
void deleteKey(EditResult& r, const Json& op) {
  Json* c = nullptr;
  Json& tr = existingTrack(r.data, op, c);
  const int64_t t = ms(op, "timeMs");
  keyAt(tr, t);
  auto& keys = arr(tr, "keys");
  keys.erase(std::remove_if(keys.begin(), keys.end(), [&](const Json& k) { return k.value("timeMs", int64_t(-1)) == t; }), keys.end());
  if (keys.empty()) {
    auto& tracks = arr(*c, "tracks");
    for (size_t i = 0; i < tracks.size(); i++) if (&tracks[i] == &tr) { tracks.erase(tracks.begin() + int64_t(i)); break; }
  }
  r.affected.push_back(keyRef(op.at("clip").get<Str>(), op, t));
}
void moveKey(EditResult& r, const Json& op) {
  Json* c = nullptr;
  Json& tr = existingTrack(r.data, op, c);
  const int64_t from = ms(op, "fromMs"), to = ms(op, "toMs"), duration = c->value("durationMs", int64_t(0));
  Json& key = keyAt(tr, from);
  if (from == to) return;
  if (to < 0 || to > duration) throw EditError("키 시간이 클립 범위(0.." + std::to_string(duration) + "ms) 밖입니다.");
  for (auto& k : arr(tr, "keys")) if (k.value("timeMs", int64_t(-1)) == to) throw EditError("같은 시간에 이미 키가 있습니다: " + std::to_string(to) + "ms");
  key["timeMs"] = to;
  sortKeys(tr);
  r.affected.push_back(keyRef(op.at("clip").get<Str>(), op, to));
}
void setInterp(EditResult& r, const Json& op) {
  Json* c = nullptr;
  Json& tr = existingTrack(r.data, op, c);
  if (op.value("property", Str()) == "attachment" || op.value("property", Str()) == "expression") throw EditError("첨부·표정 키는 항상 계단(step) 전환입니다.");
  const int64_t t = ms(op, "timeMs");
  if (!has(op, "preset")) reqStr(op, "interp");
  writeInterp(keyAt(tr, t), op, tr);
  r.affected.push_back(keyRef(op.at("clip").get<Str>(), op, t));
}
void setCurve(EditResult& r, const Json& op) {
  Json* c = nullptr;
  Json& tr = existingTrack(r.data, op, c);
  if (op.value("property", Str()) == "attachment" || op.value("property", Str()) == "expression") throw EditError("첨부·표정 키는 항상 계단(step) 전환입니다.");
  const int64_t t = ms(op, "timeMs");
  Json& key = keyAt(tr, t);
  key["interp"] = "bezier";
  key["curve"] = curveJson(readCurve(op));
  r.affected.push_back(keyRef(op.at("clip").get<Str>(), op, t));
}

// ---- Multi-key ops: one dopesheet selection gesture = one op = one undo step ----
// A selection is `keys: [{target, property, timeMs}]` inside one clip. Ops validate every
// key first and change nothing when any of them is refused (collision, range, unknown key).
constexpr size_t MAX_KEYS = 5000;
struct Picked { Json* track; std::set<int64_t> times; Str target, property; };
Json refJson(const Str& target, const Str& property, int64_t t) { return {{"target", target}, {"property", property}, {"timeMs", t}}; }
std::vector<Picked> pickKeys(Json& d, Json& c, const Json& op) {
  if (!has(op, "keys") || !op.at("keys").is_array() || op.at("keys").empty() || op.at("keys").size() > MAX_KEYS)
    throw EditError("keys는 1.." + std::to_string(MAX_KEYS) + "개의 [{target, property, timeMs}] 배열입니다.");
  std::vector<Picked> out;
  for (const auto& k : op.at("keys")) {
    if (!k.is_object()) throw EditError("keys 항목은 {target, property, timeMs} 객체입니다.");
    target(d, k);
    const Str tg = k.at("target").get<Str>(), prop = k.at("property").get<Str>();
    const int64_t t = ms(k, "timeMs");
    Json* tr = findTrack(c, tg, prop);
    if (!tr) throw EditError("이 대상·속성에는 키가 없습니다: " + tg + " " + prop);
    keyAt(*tr, t);
    auto it = std::find_if(out.begin(), out.end(), [&](const Picked& p) { return p.track == tr; });
    if (it == out.end()) { out.push_back({tr, {}, tg, prop}); it = out.end() - 1; }
    it->times.insert(t);  // duplicates in the list are harmless
  }
  return out;
}
/** Re-time the picked keys with `map`; refuses out-of-range, merged and colliding keys before changing anything. */
void retimeKeys(EditResult& r, Json& c, const Str& clipId, std::vector<Picked>& picked, const std::function<int64_t(int64_t)>& map) {
  const int64_t duration = c.value("durationMs", int64_t(0));
  for (const auto& p : picked) {
    std::set<int64_t> mapped;
    for (int64_t t : p.times) {
      const int64_t to = map(t);
      if (to < 0 || to > duration) throw EditError("옮긴 키가 클립 범위(0.." + std::to_string(duration) + "ms) 밖으로 나갑니다: " + std::to_string(to) + "ms");
      if (!mapped.insert(to).second) throw EditError("간격이 너무 좁아져 키가 같은 시간에 겹칩니다: " + p.target + " " + p.property + " " + std::to_string(to) + "ms");
    }
    for (auto& k : arr(*p.track, "keys")) {
      const int64_t t = k.value("timeMs", int64_t(-1));
      if (!p.times.count(t) && mapped.count(t)) throw EditError("같은 시간에 이미 키가 있습니다: " + p.target + " " + p.property + " " + std::to_string(t) + "ms");
    }
  }
  Json moved = Json::array();
  for (auto& p : picked) {
    // Each key is visited once and mapped from its original time, so a key landing on
    // another selected key's old time is never mapped twice.
    for (auto& k : arr(*p.track, "keys")) {
      const int64_t t = k.value("timeMs", int64_t(-1));
      if (!p.times.count(t)) continue;
      const int64_t to = map(t);
      k["timeMs"] = to;
      moved.push_back(refJson(p.target, p.property, to));
      r.affected.push_back("key:" + clipId + "/" + p.target + "/" + p.property + "@" + std::to_string(to));
    }
    // Normalized curves ride along with their key: segment shapes stay, only their timing changes.
    sortKeys(*p.track);
  }
  r.extra["keys"] = moved;
}
void moveKeys(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str clipId = reqStr(op, "clip");
  Json& c = clip(d, clipId);
  auto picked = pickKeys(d, c, op);
  int64_t delta = ms(op, "deltaMs");
  int64_t lo = INT64_MAX, hi = INT64_MIN;
  for (const auto& p : picked) { lo = std::min(lo, *p.times.begin()); hi = std::max(hi, *p.times.rbegin()); }
  // clamp: a drag past the clip edge stops the whole group there and keeps its spacing.
  if (op.value("clamp", false)) delta = std::clamp<int64_t>(delta, -lo, std::max<int64_t>(-lo, c.value("durationMs", int64_t(0)) - hi));
  r.extra["deltaMs"] = delta;
  if (delta == 0) { r.extra["keys"] = Json::array(); return; }
  retimeKeys(r, c, clipId, picked, [delta](int64_t t) { return t + delta; });
}
void scaleKeys(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str clipId = reqStr(op, "clip");
  Json& c = clip(d, clipId);
  auto picked = pickKeys(d, c, op);
  const double factor = num(op, "factor", 1);
  if (!(factor > 0) || factor > 1000) throw EditError("factor(시간 배율)는 0보다 크고 1000 이하여야 합니다.");
  int64_t pivot = INT64_MAX;
  for (const auto& p : picked) pivot = std::min(pivot, *p.times.begin());
  if (has(op, "pivotMs")) pivot = ms(op, "pivotMs");
  if (factor == 1) { r.extra["keys"] = Json::array(); return; }
  retimeKeys(r, c, clipId, picked, [pivot, factor](int64_t t) { return pivot + int64_t(std::llround(double(t - pivot) * factor)); });
}
void deleteKeys(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str clipId = reqStr(op, "clip");
  Json& c = clip(d, clipId);
  auto picked = pickKeys(d, c, op);
  for (auto& p : picked) {
    auto& keys = arr(*p.track, "keys");
    keys.erase(std::remove_if(keys.begin(), keys.end(), [&](const Json& k) { return p.times.count(k.value("timeMs", int64_t(-1))) > 0; }), keys.end());
    for (int64_t t : p.times) r.affected.push_back("key:" + clipId + "/" + p.target + "/" + p.property + "@" + std::to_string(t));
  }
  auto& tracks = arr(c, "tracks");
  tracks.erase(std::remove_if(tracks.begin(), tracks.end(), [](const Json& t) { return !t.contains("keys") || !t.at("keys").is_array() || t.at("keys").empty(); }), tracks.end());
}
/**
 * Clipboard of keys (no data change): extra.clipboard = {keys: [{target, property, offsetMs,
 * value, interp?, curve?}], fromMs} with offsets from the earliest copied key, so it can be
 * pasted at any time of any clip. Numeric keys always carry their interp; attachment keys never do.
 */
void copyKeys(EditResult& r, const Json& op) {
  Json& d = r.data;
  Json& c = clip(d, reqStr(op, "clip"));
  const auto picked = pickKeys(d, c, op);
  int64_t lo = INT64_MAX;
  for (const auto& p : picked) lo = std::min(lo, *p.times.begin());
  Json keys = Json::array();
  for (const auto& p : picked)
    for (const auto& k : p.track->at("keys")) {
      const int64_t t = k.value("timeMs", int64_t(-1));
      if (!p.times.count(t)) continue;
      Json e{{"target", p.target}, {"property", p.property}, {"offsetMs", t - lo}, {"value", k.contains("value") ? k.at("value") : Json(nullptr)}};
      if (p.property != "attachment" && p.property != "expression") {
        e["interp"] = k.contains("interp") && k.at("interp").is_string() ? k.at("interp") : Json("linear");
        if (e["interp"] == "bezier") e["curve"] = curveJson(curveOf(k));
      }
      keys.push_back(e);
    }
  r.extra["clipboard"] = {{"keys", keys}, {"fromMs", lo}};
}
/**
 * Paste a clipboard at `timeMs` of `clip`. Same-time keys are overwritten (value and, when
 * the entry has one, interpolation). `target` re-targets a clipboard that holds one target
 * only (copy a row, paste onto another bone). Out-of-range offsets refuse the whole paste.
 */
void pasteKeys(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str clipId = reqStr(op, "clip");
  Json& c = clip(d, clipId);
  const int64_t at = ms(op, "timeMs"), duration = c.value("durationMs", int64_t(0));
  if (!has(op, "keys") || !op.at("keys").is_array() || op.at("keys").empty() || op.at("keys").size() > MAX_KEYS)
    throw EditError("붙여넣을 keys는 1.." + std::to_string(MAX_KEYS) + "개의 배열입니다.");
  if (at < 0) throw EditError("붙여넣을 시간은 0 이상이어야 합니다.");
  std::set<Str> targets;
  size_t outside = 0;
  for (const auto& e : op.at("keys")) {
    if (!e.is_object()) throw EditError("붙여넣을 키는 {target, property, offsetMs, value} 객체입니다.");
    targets.insert(reqStr(e, "target"));
    const int64_t offset = ms(e, "offsetMs");
    if (offset < 0) throw EditError("offsetMs는 0 이상이어야 합니다.");
    outside += at + offset > duration;
  }
  const Str retarget = optStr(op, "target");
  if (!retarget.empty() && targets.size() != 1) throw EditError("여러 대상의 키는 한 대상으로 붙여넣을 수 없습니다. 같은 대상으로 붙이세요.");
  if (outside) throw EditError("붙여넣을 키 " + std::to_string(outside) + "개가 클립 범위(0.." + std::to_string(duration) + "ms) 밖입니다. 재생 헤드를 앞으로 옮기세요.");
  Json pasted = Json::array();
  for (const auto& e : op.at("keys")) {
    const Str tg = retarget.empty() ? e.at("target").get<Str>() : retarget, prop = optStr(e, "property");
    const int64_t t = at + ms(e, "offsetMs");
    Json set{{"clip", clipId}, {"target", tg}, {"property", prop}, {"timeMs", t}};
    if (e.contains("value")) set["value"] = e.at("value");
    for (const char* k : {"interp", "curve"}) if (has(e, k)) set[k] = e.at(k);
    setKey(r, set);
    pasted.push_back(refJson(tg, prop, t));
  }
  r.extra["keys"] = pasted;
}
const char* const POSE_PROPS[] = {"x", "y", "rotation", "scaleX", "scaleY"};
double setupValue(const Json& b, const Str& prop) {
  const Transform t = readT(b);
  return prop == "x" ? t.x : prop == "y" ? t.y : prop == "rotation" ? t.rotation : prop == "scaleX" ? t.scaleX : t.scaleY;
}
/** Bone transforms sampled at clip time (setup pose without `clip`): extra.pose = [{bone, x, y, rotation, scaleX, scaleY}]. */
void copyPose(EditResult& r, const Json& op) {
  Json& d = r.data;
  Json* c = has(op, "clip") ? &clip(d, reqStr(op, "clip")) : nullptr;
  const int64_t t = c ? ms(op, "timeMs") : 0;
  if (c && (t < 0 || t > c->value("durationMs", int64_t(0)))) throw EditError("포즈를 복사할 시간이 클립 범위 밖입니다.");
  std::vector<Str> ids;
  if (has(op, "bones")) {
    if (!op.at("bones").is_array()) throw EditError("bones는 본 ID 배열입니다.");
    for (const auto& b : op.at("bones")) ids.push_back(b.is_string() ? b.get<Str>() : Str());
  } else for (auto& b : arr(d, "bones")) ids.push_back(b.value("id", Str()));
  Json pose = Json::array();
  for (const auto& id : ids) {
    const Json& b = bone(d, id);
    Json e{{"bone", id}};
    for (const char* prop : POSE_PROPS) e[prop] = clean(sampleJson(c ? findTrack(*c, "bone:" + id, prop) : nullptr, double(t), setupValue(b, prop)));
    pose.push_back(e);
  }
  r.extra["pose"] = pose;
}
/**
 * Key a copied pose at `timeMs`: every channel of every listed bone, except channels that
 * the clip does not animate and whose value is the setup value (they already show it).
 * Existing keys at that time keep their interpolation.
 */
void pastePose(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str clipId = reqStr(op, "clip");
  Json& c = clip(d, clipId);
  const int64_t t = ms(op, "timeMs");
  if (!has(op, "pose") || !op.at("pose").is_array() || op.at("pose").empty()) throw EditError("pose는 [{bone, x, y, rotation, scaleX, scaleY}] 배열입니다.");
  Json pasted = Json::array();
  for (const auto& e : op.at("pose")) {
    const Str id = reqStr(e, "bone");
    const double setup[] = {setupValue(bone(d, id), "x"), setupValue(bone(d, id), "y"), setupValue(bone(d, id), "rotation"),
                            setupValue(bone(d, id), "scaleX"), setupValue(bone(d, id), "scaleY")};
    for (size_t i = 0; i < 5; i++) {
      const char* prop = POSE_PROPS[i];
      if (!has(e, prop)) continue;
      const double v = clean(num(e, prop, 0));
      if (!findTrack(c, "bone:" + id, prop) && std::fabs(v - clean(setup[i])) < 1e-9) continue;
      setKey(r, {{"clip", clipId}, {"target", "bone:" + id}, {"property", prop}, {"timeMs", t}, {"value", v}});
      pasted.push_back(refJson("bone:" + id, prop, t));
    }
  }
  r.extra["keys"] = pasted;
}

// ---- Clips ----
void addClip(EditResult& r, const Json& op) {
  Json& d = r.data;
  Json c = has(op, "clip") ? op.at("clip") : Json::object();
  if (!c.is_object()) throw EditError("clip은 객체여야 합니다.");
  const Str id = has(op, "id") ? reqStr(op, "id") : optStr(c, "id");
  ensureUniqueClip(d, id);
  c["id"] = id;
  if (has(op, "name")) c["name"] = optStr(op, "name");
  if (!c.contains("durationMs")) c["durationMs"] = has(op, "durationMs") ? ms(op, "durationMs") : int64_t(1000);
  if (!c.contains("loop")) c["loop"] = op.value("loop", true);
  if (!c.contains("tracks")) c["tracks"] = Json::array();
  arr(d, "clips").push_back(c);
  r.affected.push_back("clip:" + id);
}
void duplicateClip(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str from = reqStr(op, "clip"), id = reqStr(op, "id");
  Json c = clip(d, from);
  ensureUniqueClip(d, id);
  c["id"] = id;
  c["name"] = has(op, "name") ? optStr(op, "name") : c.value("name", from) + " 복사";
  arr(d, "clips").push_back(c);
  r.affected.push_back("clip:" + id);
}
void renameClip(EditResult& r, const Json& op, const Json& controllers) {
  Json& d = r.data;
  const Str from = reqStr(op, "clip");
  Json& c = clip(d, from);
  const Str to = has(op, "id") ? reqStr(op, "id") : from;
  if (has(op, "name")) c["name"] = optStr(op, "name");
  r.affected.push_back("clip:" + to);
  if (to == from) return;
  ensureUniqueClip(d, to);
  c["id"] = to;
  if (d.contains("mix") && d["mix"].is_object())
    for (auto& p : arr(d["mix"], "pairs")) for (const char* k : {"from", "to"}) if (p.value(k, Str()) == from) p[k] = to;
  // Controllers live in other documents: return rewritten copies so the caller commits them together.
  Json changed = Json::array(), refs = Json::array();
  for (const auto& entry : controllers) {
    if (!entry.is_object() || !entry.contains("data")) continue;
    Json data = entry.at("data");
    bool touched = false;
    if (data.value("defaultClip", Str()) == from) { data["defaultClip"] = to; touched = true; }
    for (auto& l : arr(data, "layers")) if (l.value("clip", Str()) == from) { l["clip"] = to; touched = true; }
    for (auto& b : arr(data, "bindings")) if (b.value("clip", Str()) == from) { b["clip"] = to; touched = true; }
    if (touched) { changed.push_back({{"id", entry.value("id", Str())}, {"data", data}}); refs.push_back(entry.value("id", Str())); }
  }
  r.extra["controllers"] = changed;
  r.extra["controllerRefs"] = refs;
}
void setClip(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str id = reqStr(op, "clip");
  Json& c = clip(d, id);
  if (has(op, "name")) c["name"] = optStr(op, "name");
  if (has(op, "loop")) c["loop"] = op.value("loop", true);
  if (has(op, "durationMs")) {
    const int64_t duration = ms(op, "durationMs");
    if (duration < 1 || duration > 600000) throw EditError("클립 길이는 1..600000ms입니다.");
    size_t outside = 0;
    for (auto& t : arr(c, "tracks")) for (auto& k : arr(t, "keys")) outside += k.value("timeMs", int64_t(0)) > duration;
    for (auto& e : arr(c, "events")) outside += e.value("timeMs", int64_t(0)) > duration;
    if (outside && !op.value("trim", false)) throw EditError("새 길이 밖에 키·이벤트 " + std::to_string(outside) + "개가 있습니다. 잘라내려면 trim을 지정하세요.");
    for (auto& t : arr(c, "tracks")) {
      auto& keys = arr(t, "keys");
      keys.erase(std::remove_if(keys.begin(), keys.end(), [&](const Json& k) { return k.value("timeMs", int64_t(0)) > duration; }), keys.end());
    }
    auto& tracks = arr(c, "tracks");
    tracks.erase(std::remove_if(tracks.begin(), tracks.end(), [](const Json& t) { return !t.contains("keys") || t.at("keys").empty(); }), tracks.end());
    auto& events = arr(c, "events");
    events.erase(std::remove_if(events.begin(), events.end(), [&](const Json& e) { return e.value("timeMs", int64_t(0)) > duration; }), events.end());
    if (events.empty()) c.erase("events");
    c["durationMs"] = duration;
  }
  r.affected.push_back("clip:" + id);
}
void deleteClip(EditResult& r, const Json& op, const Json& controllers) {
  Json& d = r.data;
  const Str id = reqStr(op, "clip");
  clip(d, id);
  const auto refs = controllerClipRefs(controllers, id);
  if (!refs.empty() && !op.value("force", false)) {
    Str list; for (const auto& x : refs) list += (list.empty() ? "" : ", ") + x;
    throw EditError("게임 연결에서 사용 중인 클립입니다: " + list);
  }
  auto& clips = arr(d, "clips");
  clips.erase(clips.begin() + findIndex(clips, "id", id));
  if (d.contains("mix") && d["mix"].is_object()) {
    auto& pairs = arr(d["mix"], "pairs");
    pairs.erase(std::remove_if(pairs.begin(), pairs.end(), [&](const Json& p) { return p.value("from", Str()) == id || p.value("to", Str()) == id; }), pairs.end());
  }
  r.extra["controllerRefs"] = refs;
  r.affected.push_back("clip:" + id);
}
void setMix(EditResult& r, const Json& op) {
  Json& d = r.data;
  if (!d.contains("mix") || !d["mix"].is_object()) d["mix"] = Json::object();
  if (has(op, "defaultMs")) d["mix"]["defaultMs"] = ms(op, "defaultMs");
  if (op.contains("pairs")) {
    if (!op.at("pairs").is_array()) throw EditError("pairs는 배열이어야 합니다.");
    std::set<std::pair<Str, Str>> seen;
    Json pairs = Json::array();
    for (const auto& p : op.at("pairs")) {
      const Str from = reqStr(p, "from"), to = reqStr(p, "to");
      clip(d, from); clip(d, to);
      if (!seen.insert({from, to}).second) throw EditError("같은 전환 쌍이 중복됩니다: " + from + " → " + to);
      pairs.push_back({{"from", from}, {"to", to}, {"durationMs", ms(p, "durationMs")}});
    }
    d["mix"]["pairs"] = pairs;
  }
  r.affected.push_back("mix");
}
void setCanvas(EditResult& r, const Json& op) {
  Json& cv = r.data["canvas"];
  if (!cv.is_object()) cv = Json::object();
  for (const char* k : {"width", "height", "originX", "originY"}) if (has(op, k)) cv[k] = clean(num(op, k, 0));
  r.affected.push_back("canvas");
}
void applySetupDeltaToKeys(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str id = reqStr(op, "bone");
  bone(d, id);
  const Json delta = op.value("delta", Json::object());
  std::set<Str> only;
  if (op.contains("clips") && op.at("clips").is_array()) for (const auto& c : op.at("clips")) if (c.is_string()) only.insert(c.get<Str>());
  int64_t count = 0;
  for (auto& c : arr(d, "clips")) {
    if (!only.empty() && !only.count(c.value("id", Str()))) continue;
    for (const char* prop : {"x", "y", "rotation", "scaleX", "scaleY"}) {
      const double dv = num(delta, prop, 0);
      Json* tr = findTrack(c, "bone:" + id, prop);
      if (!tr || dv == 0) continue;
      for (auto& k : arr(*tr, "keys")) if (k.contains("value") && k.at("value").is_number()) { k["value"] = clean(k.at("value").get<double>() + dv); count++; }
      r.affected.push_back("clip:" + c.value("id", Str()));
    }
  }
  r.extra["keys"] = count;
}

// ---- Skins and expressions (plugin 1.2.0) ----
Json& skinAt(Json& d, const Str& id) {
  const int i = findIndex(listOr(d, "skins"), "id", id);
  if (i < 0) throw EditError("없는 스킨입니다: " + id);
  return d["skins"][size_t(i)];
}
Json& expressionAt(Json& d, const Str& id) {
  const int i = findIndex(listOr(d, "expressions"), "id", id);
  if (i < 0) throw EditError("없는 표정입니다: " + id);
  return d["expressions"][size_t(i)];
}
void ensureUniqueIn(const Json& d, const char* list, const Str& id, const Str& what) {
  if (!validId(id)) throw EditError(what + " ID는 소문자 영문·숫자·하이픈 형식이어야 합니다: " + id);
  if (findIndex(listOr(d, list), "id", id) >= 0) throw EditError("이미 있는 " + what + " ID입니다: " + id);
}
Str displayName(const Json& op, const Str& fallback) {
  const Str n = has(op, "name") ? optStr(op, "name") : fallback;
  if (n.size() > 80) throw EditError("이름이 너무 깁니다(80자 이하).");
  return n.empty() ? fallback : n;
}
void eraseFrom(Json& d, const char* list, const Str& id) {
  auto& a = arr(d, list);
  a.erase(a.begin() + findIndex(a, "id", id));
  if (a.empty()) d.erase(list);  // the last skin/expression gone: the document returns to the 1.1 shape
}
/** Controllers live in other documents: `fn` rewrites a copy and returns true when it changed it. */
void rewriteControllers(EditResult& r, const Json& controllers, const std::function<bool(Json&)>& fn) {
  Json changed = Json::array(), refs = Json::array();
  if (controllers.is_array())
    for (const auto& entry : controllers) {
      if (!entry.is_object() || !entry.contains("data") || !entry.at("data").is_object()) continue;
      Json data = entry.at("data");
      if (fn(data)) { changed.push_back({{"id", entry.value("id", Str())}, {"data", data}}); refs.push_back(entry.value("id", Str())); }
    }
  r.extra["controllers"] = changed;
  r.extra["controllerRefs"] = refs;
}
/** Controller ids whose `defaultField` or any binding `field` names `id`. */
std::vector<Str> controllerLookRefs(const Json& controllers, const char* defaultField, const char* field, const Str& id) {
  std::vector<Str> out;
  if (!controllers.is_array()) return out;
  for (const auto& entry : controllers) {
    if (!entry.is_object() || !entry.contains("data") || !entry.at("data").is_object()) continue;
    const Json& c = entry.at("data");
    bool used = optStr(c, defaultField) == id;
    for (const auto& b : listOr(c, "bindings")) used |= optStr(b, field) == id;
    if (used) out.push_back(entry.value("id", Str()));
  }
  return out;
}
/** Rename (to non-empty) or drop (to empty) controller refs; bindings left with nothing to do are removed. */
bool retargetLook(Json& c, const char* defaultField, const char* field, const Str& from, const Str& to) {
  bool touched = false;
  if (optStr(c, defaultField) == from) { if (to.empty()) c.erase(defaultField); else c[defaultField] = to; touched = true; }
  auto& bindings = arr(c, "bindings");
  for (auto& b : bindings)
    if (b.is_object() && optStr(b, field) == from) { if (to.empty()) b.erase(field); else b[field] = to; touched = true; }
  bindings.erase(std::remove_if(bindings.begin(), bindings.end(), [](const Json& b) {
    return b.is_object() && !b.contains("clip") && !b.contains("skin") && !b.contains("expression");
  }), bindings.end());
  return touched;
}
Str refList(const std::vector<Str>& refs) { Str list; for (const auto& x : refs) list += (list.empty() ? "" : ", ") + x; return list; }

void addSkin(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str id = reqStr(op, "id");
  ensureUniqueIn(d, "skins", id, "스킨");
  Json skin{{"id", id}, {"name", displayName(op, id)}, {"attachments", Json::array()}};
  if (has(op, "from")) skin["attachments"] = listOr(skinAt(d, reqStr(op, "from")), "attachments");
  arr(d, "skins").push_back(skin);
  r.affected.push_back("skin:" + id);
}
void duplicateSkin(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str from = reqStr(op, "skin"), id = reqStr(op, "id");
  Json skin = skinAt(d, from);
  ensureUniqueIn(d, "skins", id, "스킨");
  skin["id"] = id;
  skin["name"] = displayName(op, optStr(skin, "name", from) + " 복사");
  arr(d, "skins").push_back(skin);
  r.affected.push_back("skin:" + id);
}
void renameSkin(EditResult& r, const Json& op, const Json& controllers) {
  Json& d = r.data;
  const Str from = reqStr(op, "skin");
  Json& s = skinAt(d, from);
  const Str to = has(op, "id") ? reqStr(op, "id") : from;
  if (has(op, "name")) s["name"] = displayName(op, to);
  r.affected.push_back("skin:" + to);
  if (to == from) return;
  ensureUniqueIn(d, "skins", to, "스킨");
  s["id"] = to;
  rewriteControllers(r, controllers, [&](Json& c) { return retargetLook(c, "defaultSkin", "skin", from, to); });
}
void deleteSkin(EditResult& r, const Json& op, const Json& controllers) {
  Json& d = r.data;
  const Str id = reqStr(op, "skin");
  skinAt(d, id);
  const auto refs = controllerLookRefs(controllers, "defaultSkin", "skin", id);
  if (!refs.empty() && !(has(op, "force") && op.at("force").is_boolean() && op.at("force").get<bool>()))
    throw EditError("게임 연결에서 사용 중인 스킨입니다: " + refList(refs) + " (force로 연결에서도 지웁니다)");
  eraseFrom(d, "skins", id);
  if (!refs.empty()) rewriteControllers(r, controllers, [&](Json& c) { return retargetLook(c, "defaultSkin", "skin", id, ""); });
  else r.extra["controllerRefs"] = Json::array();
  r.affected.push_back("skin:" + id);
}
/**
 * Add or update one skin attachment (slot, name). `name` defaults to the slot's setup
 * attachment, so "import an outfit image for this part" needs only skin + slot + image. A new
 * entry starts from the placement of the default attachment of that name (or the part's shown
 * one): the outfit lands where the base image was, even at a different pixel size.
 */
void setSkinAttachment(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str skinId = reqStr(op, "skin"), slotId = reqStr(op, "slot");
  Json& s = skinAt(d, skinId);
  const Json& sl = slot(d, slotId);
  Str name = optStr(op, "name");
  if (name.empty()) name = optStr(sl, "attachment");
  if (name.empty()) throw EditError("첨부 이름이 필요합니다(이 조각에는 기본 첨부가 없습니다).");
  if (!validName(name)) throw EditError("첨부 이름 형식이 올바르지 않습니다: " + name);
  auto& list = arr(s, "attachments");
  Json* att = nullptr;
  for (auto& a : list) if (optStr(a, "slot") == slotId && optStr(a, "name") == name) att = &a;
  if (!att) {
    const Str asset = reqStr(op, "imageAssetId");
    const double w = num(op, "width", 0), h = num(op, "height", 0);
    if (w <= 0 || h <= 0) throw EditError("이미지 크기를 알 수 없습니다(0보다 커야 합니다).");
    Json a{{"slot", slotId}, {"name", name}, {"imageAssetId", asset}, {"width", clean(w)}, {"height", clean(h)}, {"x", 0}, {"y", 0}};
    const Json* base = nullptr;
    for (const Str& want : {name, optStr(sl, "attachment")})
      for (const auto& x : listOr(d, "attachments")) if (!base && optStr(x, "slot") == slotId && optStr(x, "name") == want) base = &x;
    for (const auto& x : listOr(d, "attachments")) if (!base && optStr(x, "slot") == slotId) base = &x;
    for (const char* k : {"x", "y", "rotation", "scaleX", "scaleY", "pivotX", "pivotY"}) if (base && base->contains(k)) a[k] = base->at(k);
    list.push_back(a);
    att = &list.back();
    r.extra["created"] = true;
  } else if (has(op, "imageAssetId")) (*att)["imageAssetId"] = reqStr(op, "imageAssetId");
  for (const char* k : {"x", "y", "rotation", "scaleX", "scaleY", "pivotX", "pivotY", "width", "height"}) if (has(op, k)) (*att)[k] = clean(num(op, k, 0));
  r.affected.push_back("skin:" + skinId);
  r.affected.push_back("slot:" + slotId);
  r.extra["attachment"] = name;
}
void removeSkinAttachment(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str skinId = reqStr(op, "skin"), slotId = reqStr(op, "slot"), name = reqStr(op, "name");
  auto& list = arr(skinAt(d, skinId), "attachments");
  const auto before = list.size();
  list.erase(std::remove_if(list.begin(), list.end(), [&](const Json& a) { return optStr(a, "slot") == slotId && optStr(a, "name") == name; }), list.end());
  if (list.size() == before) throw EditError("이 스킨에 없는 첨부입니다: " + slotId + "/" + name);
  r.affected.push_back("skin:" + skinId);
  r.affected.push_back("slot:" + slotId);
}

void addExpression(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str id = reqStr(op, "id");
  ensureUniqueIn(d, "expressions", id, "표정");
  Json e{{"id", id}, {"name", displayName(op, id)}};
  for (const char* k : {"slots", "bones"}) {
    if (!has(op, k)) continue;
    if (!op.at(k).is_object()) throw EditError(Str(k) + "는 객체여야 합니다.");
    if (!op.at(k).empty()) e[k] = op.at(k);
  }
  arr(d, "expressions").push_back(e);
  r.affected.push_back("expression:" + id);
}
/** Clip keys of the rig expression track naming `from`: renamed to `to`, or removed when `to` is empty. */
size_t retargetExpressionKeys(Json& d, const Str& from, const Str& to) {
  size_t count = 0;
  for (auto& c : arr(d, "clips")) {
    for (auto& t : arr(c, "tracks")) {
      if (t.value("target", Str()) != "rig") continue;
      auto& keys = arr(t, "keys");
      for (auto& k : keys) if (optStr(k, "value") == from) { count++; if (!to.empty()) k["value"] = to; }
      if (to.empty()) keys.erase(std::remove_if(keys.begin(), keys.end(), [&](const Json& k) { return optStr(k, "value") == from; }), keys.end());
    }
    auto& tracks = arr(c, "tracks");
    tracks.erase(std::remove_if(tracks.begin(), tracks.end(), [](const Json& t) { return !t.contains("keys") || !t.at("keys").is_array() || t.at("keys").empty(); }), tracks.end());
  }
  return count;
}
void renameExpression(EditResult& r, const Json& op, const Json& controllers) {
  Json& d = r.data;
  const Str from = reqStr(op, "expression");
  Json& e = expressionAt(d, from);
  const Str to = has(op, "id") ? reqStr(op, "id") : from;
  if (has(op, "name")) e["name"] = displayName(op, to);
  r.affected.push_back("expression:" + to);
  if (to == from) return;
  ensureUniqueIn(d, "expressions", to, "표정");
  e["id"] = to;
  r.extra["keys"] = retargetExpressionKeys(d, from, to);
  rewriteControllers(r, controllers, [&](Json& c) { return retargetLook(c, "defaultExpression", "expression", from, to); });
}
void deleteExpression(EditResult& r, const Json& op, const Json& controllers) {
  Json& d = r.data;
  const Str id = reqStr(op, "expression");
  expressionAt(d, id);
  const auto refs = controllerLookRefs(controllers, "defaultExpression", "expression", id);
  size_t keyed = 0;
  for (const auto& c : listOr(d, "clips"))
    for (const auto& t : listOr(c, "tracks"))
      if (t.value("target", Str()) == "rig") for (const auto& k : listOr(t, "keys")) keyed += optStr(k, "value") == id;
  if ((!refs.empty() || keyed) && !(has(op, "force") && op.at("force").is_boolean() && op.at("force").get<bool>()))
    throw EditError("사용 중인 표정입니다" + Str(refs.empty() ? "" : ": 게임 연결 " + refList(refs)) + (keyed ? " · 표정 키 " + std::to_string(keyed) + "개" : "") + " (force로 함께 지웁니다)");
  eraseFrom(d, "expressions", id);
  r.extra["keys"] = retargetExpressionKeys(d, id, "");
  if (!refs.empty()) rewriteControllers(r, controllers, [&](Json& c) { return retargetLook(c, "defaultExpression", "expression", id, ""); });
  else r.extra["controllerRefs"] = Json::array();
  r.affected.push_back("expression:" + id);
}
void setExpressionSlot(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str id = reqStr(op, "expression"), slotId = reqStr(op, "slot");
  Json& e = expressionAt(d, id);
  slot(d, slotId);
  if (has(op, "remove") && op.at("remove").is_boolean() && op.at("remove").get<bool>()) {
    if (e.contains("slots") && e["slots"].is_object()) { e["slots"].erase(slotId); if (e["slots"].empty()) e.erase("slots"); }
  } else {
    if (!op.contains("attachment")) throw EditError("attachment(첨부 이름 또는 null)가 필요합니다.");
    const Json& v = op.at("attachment");
    if (!v.is_null() && (!v.is_string() || !attachmentNames(d, slotId).count(v.get<Str>())))
      throw EditError("이 조각에 없는 첨부입니다: " + (v.is_string() ? v.get<Str>() : v.dump()));
    if (!e.contains("slots") || !e["slots"].is_object()) e["slots"] = Json::object();
    e["slots"][slotId] = v;
  }
  r.affected.push_back("expression:" + id);
}
const char* const BONE_CHANNELS[] = {"x", "y", "rotation", "scaleX", "scaleY"};
void setExpressionBone(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str id = reqStr(op, "expression"), boneId = reqStr(op, "bone");
  Json& e = expressionAt(d, id);
  bone(d, boneId);
  if (!e.contains("bones") || !e["bones"].is_object()) e["bones"] = Json::object();
  Json& bones = e["bones"];
  if (has(op, "remove") && op.at("remove").is_boolean() && op.at("remove").get<bool>()) bones.erase(boneId);
  else {
    Json entry = bones.contains(boneId) && bones[boneId].is_object() ? bones[boneId] : Json::object();
    for (const char* k : BONE_CHANNELS) {
      if (!op.contains(k)) continue;
      if (op.at(k).is_null()) entry.erase(k);  // null drops the channel (it animates again)
      else entry[k] = clean(num(op, k, 0));
    }
    if (entry.empty()) bones.erase(boneId); else bones[boneId] = entry;
  }
  if (bones.empty()) e.erase("bones");
  r.affected.push_back("expression:" + id);
}
/**
 * "현재 상태로 표정 만들기": sample a pose (`from` rig data or this rig, at clip/timeMs or the
 * setup pose) and store what differs from the baseline as expression `id`. Baseline: the setup
 * pose, or with `from` the same clip time of this rig, so only the unsaved editor changes are
 * captured. Listed `slots`/`bones` are captured whole instead (all five channels). Expressions
 * never apply while sampling. `replace: true` overwrites an existing expression (keeps its name).
 */
void captureExpression(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str id = reqStr(op, "id");
  const bool replace = has(op, "replace") && op.at("replace").is_boolean() && op.at("replace").get<bool>();
  const int existing = findIndex(listOr(d, "expressions"), "id", id);
  if (existing < 0 || !replace) ensureUniqueIn(d, "expressions", id, "표정");
  const bool fromOther = has(op, "from");
  Rig target, src;
  try {
    target = parseRig("capture", d);
    src = fromOther ? parseRig("capture-from", op.at("from")) : target;
  } catch (const std::exception& e) { throw EditError(Str("표정을 담을 리그를 읽을 수 없습니다: ") + e.what()); }
  const Str clipId = optStr(op, "clip");
  const double t = has(op, "timeMs") ? double(ms(op, "timeMs")) : 0;
  auto clipIn = [&](const Rig& rig) {
    const int c = clipId.empty() ? -1 : rig.clipIndex(clipId);
    if (!clipId.empty() && c < 0) throw EditError("없는 클립입니다: " + clipId);
    return c;
  };
  Look raw;
  raw.expressions = false;
  const Pose now = samplePose(src, clipIn(src), t, raw);
  const Pose base = fromOther ? samplePose(target, clipIn(target), t, raw) : samplePose(target, -1, 0, raw);
  auto listed = [&](const char* key, bool isBone) {
    std::set<Str> out;
    if (!has(op, key)) return out;
    if (!op.at(key).is_array()) throw EditError(Str(key) + "는 ID 배열입니다.");
    for (const auto& v : op.at(key)) {
      const Str x = v.is_string() ? v.get<Str>() : Str();
      if (isBone) bone(d, x); else slot(d, x);
      out.insert(x);
    }
    return out;
  };
  const std::set<Str> wantSlots = listed("slots", false), wantBones = listed("bones", true);
  Json slots = Json::object(), bones = Json::object();
  for (size_t i = 0; i < target.slots.size(); i++) {
    const Str& sid = target.slots[i].id;
    const int si = src.slotIndex(sid);
    if (si < 0 || (has(op, "slots") && !wantSlots.count(sid))) continue;
    const auto& value = now.attachments[size_t(si)];
    if (has(op, "slots") || value != base.attachments[i]) slots[sid] = value ? Json(*value) : Json(nullptr);
  }
  for (size_t i = 0; i < target.bones.size(); i++) {
    const Str& bid = target.bones[i].id;
    const int bi = src.boneIndex(bid);
    if (bi < 0 || (has(op, "bones") && !wantBones.count(bid))) continue;
    const Transform& a = now.bones[size_t(bi)];
    const Transform& b = base.bones[i];
    const double av[] = {a.x, a.y, a.rotation, a.scaleX, a.scaleY}, bv[] = {b.x, b.y, b.rotation, b.scaleX, b.scaleY};
    Json entry = Json::object();
    for (size_t k = 0; k < 5; k++) if (has(op, "bones") || std::fabs(av[k] - bv[k]) > 1e-6) entry[BONE_CHANNELS[k]] = clean(av[k]);
    if (!entry.empty()) bones[bid] = entry;
  }
  if (slots.empty() && bones.empty())
    throw EditError("기본과 달라진 그림·자세가 없어 표정으로 담을 것이 없습니다. 애니메이션 모드에서 눈·입 그림이나 본을 바꾼 뒤 다시 누르세요.");
  Json e{{"id", id}, {"name", displayName(op, existing >= 0 ? optStr(d["expressions"][size_t(existing)], "name", id) : id)}};
  if (!slots.empty()) e["slots"] = slots;
  if (!bones.empty()) e["bones"] = bones;
  if (existing >= 0) d["expressions"][size_t(existing)] = e;
  else arr(d, "expressions").push_back(e);
  r.affected.push_back("expression:" + id);
  r.extra["expression"] = e;
}

void dispatch(EditResult& r, const Json& op, const Json& controllers, int depth) {
  if (!op.is_object()) throw EditError("편집 명령은 객체여야 합니다.");
  const Str name = reqStr(op, "op");
  if (name == "batch") {
    if (depth > 0) throw EditError("batch는 중첩할 수 없습니다.");
    if (!op.contains("operations") || !op.at("operations").is_array() || op.at("operations").size() > 200) throw EditError("operations는 1..200개 배열입니다.");
    for (const auto& sub : op.at("operations")) {
      EditResult step; step.data = std::move(r.data);
      dispatch(step, sub, controllers, depth + 1);
      r.data = std::move(step.data);
      r.affected.insert(r.affected.end(), step.affected.begin(), step.affected.end());
      r.diagnostics.insert(r.diagnostics.end(), step.diagnostics.begin(), step.diagnostics.end());
      for (auto it = step.extra.begin(); it != step.extra.end(); ++it) r.extra[it.key()] = it.value();
    }
    return;
  }
  static const std::map<Str, void (*)(EditResult&, const Json&)> simple = {
    {"addBone", addBone}, {"setBone", setBone}, {"reparentBone", reparentBone}, {"setPivot", setPivot}, {"deleteBone", deleteBone},
    {"addPart", addPart}, {"setSlot", setSlot}, {"setAttachment", setAttachment}, {"deleteSlot", deleteSlot}, {"moveSlot", moveSlot},
    {"setKey", setKey}, {"deleteKey", deleteKey}, {"moveKey", moveKey}, {"setInterp", setInterp}, {"setCurve", setCurve},
    {"moveKeys", moveKeys}, {"scaleKeys", scaleKeys}, {"deleteKeys", deleteKeys}, {"copyKeys", copyKeys}, {"pasteKeys", pasteKeys},
    {"copyPose", copyPose}, {"pastePose", pastePose},
    {"addClip", addClip}, {"duplicateClip", duplicateClip}, {"setClip", setClip}, {"setMix", setMix}, {"setCanvas", setCanvas},
    {"applySetupDeltaToKeys", applySetupDeltaToKeys},
    {"addSkin", addSkin}, {"duplicateSkin", duplicateSkin}, {"setSkinAttachment", setSkinAttachment}, {"removeSkinAttachment", removeSkinAttachment},
    {"addExpression", addExpression}, {"setExpressionSlot", setExpressionSlot}, {"setExpressionBone", setExpressionBone}, {"captureExpression", captureExpression},
  };
  if (auto it = simple.find(name); it != simple.end()) return it->second(r, op);
  if (name == "renameClip") return renameClip(r, op, controllers);
  if (name == "deleteClip") return deleteClip(r, op, controllers);
  if (name == "renameSkin") return renameSkin(r, op, controllers);
  if (name == "deleteSkin") return deleteSkin(r, op, controllers);
  if (name == "renameExpression") return renameExpression(r, op, controllers);
  if (name == "deleteExpression") return deleteExpression(r, op, controllers);
  if (name == "setName") {
    const Str n = optStr(op, "name");
    if (n.size() > 240) throw EditError("이름이 너무 깁니다.");
    if (n.empty()) r.data.erase("name"); else r.data["name"] = n;
    r.affected.push_back("name");
    return;
  }
  throw EditError("알 수 없는 편집 명령입니다: " + name);
}
}  // namespace

bool validId(const std::string& id) {
  if (id.empty() || id.size() > 64 || id[0] < 'a' || id[0] > 'z' || id.back() == '-') return false;
  for (size_t i = 0; i < id.size(); i++) {
    const char c = id[i];
    if (c == '-') { if (id[i - 1] == '-') return false; continue; }
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) return false;
  }
  return true;
}
bool validName(const std::string& n) {
  if (n.empty() || n.size() > 64 || !std::isalnum(static_cast<unsigned char>(n[0]))) return false;
  for (char c : n) if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') return false;
  return true;
}
std::string slugId(const std::string& text, const std::string& fallback) {
  std::string out;
  for (unsigned char c : text) {
    if (c == '.') break;  // drop file extensions ("eye.png")
    if (std::isalnum(c) && c < 128) out += char(std::tolower(c));
    else if (!out.empty() && out.back() != '-') out += '-';
  }
  while (!out.empty() && (out.back() == '-')) out.pop_back();
  while (!out.empty() && !(out[0] >= 'a' && out[0] <= 'z')) out.erase(out.begin());
  if (out.size() > 48) out.resize(48);
  while (!out.empty() && out.back() == '-') out.pop_back();
  return validId(out) ? out : fallback;
}

Transform decompose(const Affine& m, double* skew) {
  Transform t;
  t.x = m.e; t.y = m.f;
  t.scaleX = std::hypot(m.a, m.b);
  t.rotation = t.scaleX > 1e-12 ? std::atan2(m.b, m.a) * 180.0 / PI : 0;
  const double det = m.a * m.d - m.b * m.c;
  t.scaleY = t.scaleX > 1e-12 ? det / t.scaleX : std::hypot(m.c, m.d);
  if (skew) {
    const double r = t.rotation * PI / 180.0;
    *skew = std::fabs(m.c + std::sin(r) * t.scaleY) + std::fabs(m.d - std::cos(r) * t.scaleY);
  }
  return t;
}

std::map<std::string, Affine> setupWorld(const Json& rig) {
  std::map<Str, Affine> out;
  std::map<Str, const Json*> byId;
  if (rig.is_object() && rig.contains("bones") && rig.at("bones").is_array())
    for (const auto& b : rig.at("bones")) if (b.is_object()) byId[b.value("id", Str())] = &b;
  std::function<Affine(const Str&, size_t)> world = [&](const Str& id, size_t depth) -> Affine {
    if (auto it = out.find(id); it != out.end()) return it->second;
    const Json& b = *byId.at(id);
    const Str p = parentOf(b);
    // depth guard turns a cyclic (invalid) hierarchy into roots instead of recursing forever
    const Affine w = !p.empty() && byId.count(p) && depth < byId.size() ? world(p, depth + 1) * local(readT(b)) : local(readT(b));
    out[id] = w;
    return w;
  };
  for (const auto& [id, _] : byId) world(id, 0);
  return out;
}

std::vector<std::string> controllerClipRefs(const Json& controllers, const std::string& clipId) {
  std::vector<Str> out;
  if (!controllers.is_array()) return out;
  for (const auto& entry : controllers) {
    if (!entry.is_object() || !entry.contains("data") || !entry.at("data").is_object()) continue;
    const Json& data = entry.at("data");
    bool used = data.value("defaultClip", Str()) == clipId;
    for (const char* k : {"layers", "bindings"})
      if (data.contains(k) && data.at(k).is_array()) for (const auto& x : data.at(k)) used |= x.is_object() && x.value("clip", Str()) == clipId;
    if (used) out.push_back(entry.value("id", Str()));
  }
  return out;
}

EditResult applyEdit(const Json& rig, const Json& operation, const Json& controllers) {
  if (!rig.is_object()) throw EditError("리그 데이터는 객체여야 합니다.");
  EditResult r;
  r.data = rig;
  dispatch(r, operation, controllers, 0);
  const auto after = validateRig(r.data);
  if (hasErrors(after) && !hasErrors(validateRig(rig))) {
    for (const auto& x : after)
      if (x.severity == "error") throw EditError("편집 결과가 올바르지 않아 적용하지 않았습니다: " + x.message + " (" + x.pointer + ")");
  }
  r.diagnostics.insert(r.diagnostics.end(), after.begin(), after.end());
  std::sort(r.affected.begin(), r.affected.end());
  r.affected.erase(std::unique(r.affected.begin(), r.affected.end()), r.affected.end());
  return r;
}
}  // namespace cutout
