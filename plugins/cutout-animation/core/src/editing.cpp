#include "cutout/editing.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <set>
#include "cutout/curve.hpp"
#include "cutout/mesh.hpp"
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
/** Finer rounding (1e-9) for ops that must leave the drawing unchanged (orientBones rotates
 *  coordinates by arbitrary angles: 1e-6 rounding would move parts by up to ~1e-6 px). */
double fine(double v) {
  const double r = std::round(v * 1e9) / 1e9;
  return r == 0 ? 0.0 : r;
}
void writeT(Json& o, const Transform& t, double (*round)(double) = clean) {
  o["x"] = round(t.x); o["y"] = round(t.y);
  auto opt = [&](const char* k, double v, double def) { v = round(v); if (v == def) o.erase(k); else o[k] = v; };
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

/** Deform tracks (plugin 1.4.0) are told apart by the mesh attachment they key. */
Json* findTrack(Json& c, const Str& target, const Str& property, const Str& attachment = "") {
  for (auto& t : arr(c, "tracks"))
    if (t.value("target", Str()) == target && t.value("property", Str()) == property && (property != "deform" || t.value("attachment", Str()) == attachment)) return &t;
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
/** Keys of these properties always switch (no interp/curve is stored, copied or accepted). */
bool stepOnly(const Str& property) { return property == "attachment" || property == "expression" || property == "bendPositive" || property == "sequence"; }
/** Most frames of the slot's sequence attachments, any list (0 = none): what sequence keys may index (plugin 1.12.0). */
size_t sequenceFrames(const Json& d, const Str& slotId) {
  size_t most = 0;
  auto scan = [&](const Json& list) {
    for (const auto& a : list)
      if (a.is_object() && a.value("slot", Str()) == slotId && a.contains("sequence") && a.at("sequence").is_object())
        most = std::max(most, listOr(a.at("sequence"), "frames").size());
  };
  scan(listOr(d, "attachments"));
  for (const auto& s : listOr(d, "skins")) scan(listOr(s, "attachments"));
  return most;
}
Json& ikAt(Json& d, const Str& id) {
  const int i = findIndex(listOr(d, "ik"), "id", id);
  if (i < 0) throw EditError("없는 IK 제약입니다: " + id);
  return d["ik"][size_t(i)];
}
struct Target { bool bone; Str id; Property property; bool rig = false; bool ik = false; };
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
  // IK constraint tracks (plugin 1.3.0): mix (numeric) and bendPositive (step, bool).
  const bool ikProp = *p == Property::Mix || *p == Property::BendPositive;
  if (target.rfind("ik:", 0) == 0) {
    if (!ikProp) throw EditError("IK 트랙의 속성은 mix, bendPositive입니다.");
    ikAt(d, target.substr(3));
    return {false, target.substr(3), *p, false, true};
  }
  if (ikProp) throw EditError("mix·bendPositive 키의 대상은 ik:<id>입니다.");
  const bool isBone = target.rfind("bone:", 0) == 0, isSlot = target.rfind("slot:", 0) == 0;
  if (!isBone && !isSlot) throw EditError("트랙 대상은 bone:<id>, slot:<id>, ik:<id> 또는 rig 형식입니다.");
  const Str id = target.substr(5);
  if (isBone) bone(d, id); else slot(d, id);
  const bool slotProp = *p == Property::Alpha || *p == Property::Attachment || *p == Property::Deform || *p == Property::Sequence;
  if (isBone == slotProp) throw EditError("대상 종류에 맞지 않는 속성입니다: " + prop);
  if (*p == Property::Deform && optStr(op, "attachment").empty()) throw EditError("변형(deform) 키에는 메시 첨부 이름(attachment)이 필요합니다.");
  return {isBone, id, *p};
}
/** Property of a key ref; deform keys add their attachment ("deform:<name>") so refs stay unique. */
Str refProperty(const Json& op) {
  const Str p = op.value("property", Str());
  return p == "deform" ? p + ":" + optStr(op, "attachment") : p;
}
Str keyRef(const Str& clipId, const Json& op, int64_t t) { return "key:" + clipId + "/" + op.value("target", Str()) + "/" + refProperty(op) + "@" + std::to_string(t); }
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

// ---- IK helpers (ops below) ----
/** First IK error of `d` ("" = fine): hierarchy edits refuse to break a constraint. */
Str ikProblem(const Json& d) {
  if (listOr(d, "ik").empty()) return "";
  for (const auto& x : validateRig(d)) if (x.severity == "error" && x.code.rfind("ANIM_IK_", 0) == 0) return x.message;
  return "";
}
/** Constraint ids whose chain or target is `bone`. */
std::vector<Str> ikUsing(const Json& d, const Str& bone) {
  std::vector<Str> out;
  for (const auto& k : listOr(d, "ik")) {
    bool uses = optStr(k, "target") == bone;
    for (const auto& b : listOr(k, "bones")) uses = uses || (b.is_string() && b.get<Str>() == bone);
    if (uses) out.push_back(optStr(k, "id"));
  }
  return out;
}
Str refListOf(const std::vector<Str>& ids) { Str s; for (const auto& x : ids) s += (s.empty() ? "" : ", ") + x; return s; }
/** Drop constraint `id` and its clip tracks (ik:<id>); the last one gone removes the list. */
size_t removeIk(Json& d, const Str& id) {
  auto& list = arr(d, "ik");
  list.erase(list.begin() + findIndex(list, "id", id));
  if (list.empty()) d.erase("ik");
  size_t keys = 0;
  for (auto& c : arr(d, "clips"))
    for (const auto& t : arr(c, "tracks")) if (t.value("target", Str()) == "ik:" + id) keys += t.contains("keys") ? t.at("keys").size() : 0;
  removeTracks(d, {"ik:" + id});
  return keys;
}

// ---- Mesh helpers (plugin 1.4.0; ops near the end) ----
bool isMesh(const Json& a) { return a.is_object() && optStr(a, "type") == "mesh"; }
bool isClipping(const Json& a) { return a.is_object() && optStr(a, "type") == "clipping"; }
/** Vertex counts of the meshes (and clip polygons, plugin 1.12.0: deform keys move them too) named (slot, name) in the default list and in every skin. */
std::set<size_t> meshCounts(const Json& d, const Str& slotId, const Str& name) {
  std::set<size_t> out;
  auto scan = [&](const Json& list) {
    for (const auto& a : list)
      if ((isMesh(a) || isClipping(a)) && optStr(a, "slot") == slotId && optStr(a, "name") == name && a.contains("vertices") && a.at("vertices").is_array())
        out.insert(a.at("vertices").size() / 2);
  };
  scan(listOr(d, "attachments"));
  for (const auto& s : listOr(d, "skins")) scan(listOr(s, "attachments"));
  return out;
}
/** Deform tracks whose mesh is gone or whose offsets no longer fit its vertex count are removed. */
size_t purgeDeform(EditResult& r) {
  Json& d = r.data;
  size_t removed = 0;
  for (auto& c : arr(d, "clips")) {
    auto& tracks = arr(c, "tracks");
    tracks.erase(std::remove_if(tracks.begin(), tracks.end(), [&](const Json& t) {
      const Str target = t.value("target", Str());
      if (t.value("property", Str()) != "deform" || target.rfind("slot:", 0) != 0) return false;
      const auto counts = meshCounts(d, target.substr(5), optStr(t, "attachment"));
      bool fits = !counts.empty();
      for (const auto& k : listOr(t, "keys"))
        fits = fits && k.contains("offsets") && k.at("offsets").is_array() && k.at("offsets").size() % 2 == 0 && counts.count(k.at("offsets").size() / 2);
      if (!fits) removed += listOr(t, "keys").size();
      return !fits;
    }), tracks.end());
  }
  if (removed) warn(r, "ANIM_MESH_DEFORM_REMOVED", "", "메시 정점이 바뀌어 맞지 않게 된 변형 키 " + std::to_string(removed) + "개를 지웠습니다.");
  return removed;
}
/** Weights of every mesh drop `boneId` (renormalized); a vertex left without bones follows its slot bone. */
void dropBoneWeights(EditResult& r, const Str& boneId) {
  Json& d = r.data;
  bool moved = false;
  auto fix = [&](Json& a) {
    if (!isMesh(a) || !a.contains("weights") || !a["weights"].is_array()) return;
    const Json* s = nullptr;
    for (const auto& x : listOr(d, "slots")) if (optStr(x, "id") == optStr(a, "slot")) s = &x;
    const Str fallback = s ? optStr(*s, "bone") : Str();
    for (auto& list : a["weights"]) {
      if (!list.is_array()) continue;
      const auto before = list.size();
      list.erase(std::remove_if(list.begin(), list.end(), [&](const Json& w) { return optStr(w, "bone") == boneId; }), list.end());
      if (list.size() == before) continue;
      moved = true;
      double sum = 0;
      for (const auto& w : list) sum += w.value("weight", 0.0);
      if (list.empty() || !(sum > 1e-9)) list = Json::array({{{"bone", fallback}, {"weight", 1}}});
      else for (auto& w : list) w["weight"] = clean(w.value("weight", 0.0) / sum);
    }
  };
  for (auto& a : arr(d, "attachments")) fix(a);
  if (d.contains("skins") && d["skins"].is_array())
    for (auto& s : d["skins"]) if (s.is_object() && s.contains("attachments") && s["attachments"].is_array()) for (auto& a : s["attachments"]) fix(a);
  if (moved) warn(r, "ANIM_MESH_WEIGHTS_MOVED", "", "지운 본의 메시 웨이트를 남은 본(없으면 조각의 본)으로 옮겼습니다.");
}

// ---- Bones ----
double boneLength(const Json& b) { return b.is_object() && b.contains("length") && b.at("length").is_number() ? b.at("length").get<double>() : 0; }
/** `length` (px, 0..100000; 0 / null = point bone, the key is dropped) and `color` ("#rrggbb[aa]";
 *  null / "" = default) of addBone/setBone (plugin 1.5.0). */
void writeLook(Json& b, const Json& op) {
  if (op.contains("length")) {
    const double len = op.at("length").is_null() ? 0 : num(op, "length", 0);
    if (!(len >= 0) || len > 100000) throw EditError("본 길이(length)는 0..100000 사이의 숫자(px)입니다.");
    if (clean(len) == 0) b.erase("length"); else b["length"] = clean(len);
  }
  if (op.contains("color")) {
    const Str c = optStr(op, "color");
    if (c.empty()) b.erase("color");
    else b["color"] = c;  // the edit is refused by validation when malformed (ANIM_BONE_COLOR)
  }
}
/**
 * A lower bone of a two-bone chain that gets a length would switch its implicit effector from
 * its first child to its own tip (the 1.5.0 precedence). Gestures that set a length must never
 * move a limb by surprise, so that child becomes the explicit `tip`.
 */
void pinLimbTips(EditResult& r, const Str& id) {
  Json& d = r.data;
  if (!d.contains("ik") || !d.at("ik").is_array()) return;
  Str child;
  for (const auto& c : arr(d, "bones")) if (parentOf(c) == id) { child = c.value("id", Str()); break; }
  if (child.empty()) return;
  for (auto& k : d["ik"])
    if (k.is_object() && !k.contains("tip") && k.contains("bones") && k.at("bones").is_array() && k.at("bones").size() == 2 && k.at("bones")[1] == id) {
      k["tip"] = child;
      r.affected.push_back("ik:" + optStr(k, "id"));
    }
}
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
  writeLook(b, op);
  arr(d, "bones").push_back(b);
  r.affected.push_back("bone:" + id);
}
/** `axes` of setBone (plugin 1.7.0): the space its x/y/rotation are given in. */
Axes axesOf(const Json& op) {
  if (!has(op, "axes")) return Axes::Local;
  const auto a = op.at("axes").is_string() ? parseAxes(op.at("axes").get<Str>()) : std::nullopt;
  if (!a) throw EditError("axes는 local, parent, world 중 하나입니다.");
  return *a;
}
bool flag(const Json& op, const char* k) { return has(op, k) && op.at(k) == true; }
/**
 * Bone / image compensation (plugin 1.7.0, Spine "보정", setup pose only): the bones in `changed`
 * got new setup transforms; their direct children that are not in `keep` (the selection, which
 * moves on purpose) get local = newParentWorld^-1 · oldWorld, so they (and everything below them)
 * stay where they were, and with `images` every attachment of their slots (default and skins) is
 * re-placed the same way. `before` is the data before the change. Values use the fine rounding:
 * the drawn result must stay within 1e-6 px.
 */
void compensate(EditResult& r, const Json& before, const std::set<Str>& changed, const std::set<Str>& keep, bool bones, bool images) {
  if ((!bones && !images) || changed.empty()) return;
  Json& d = r.data;
  const auto oldWorld = setupWorld(before), newWorld = setupWorld(d);
  bool skewed = false, keyed = false;
  auto rebase = [&](const Str& parent, const Affine& oldInParent, const Transform& hint) {
    double skew = 0;
    const Transform t = decomposeNear(inverseOr(newWorld.at(parent), "본") * oldWorld.at(parent) * oldInParent, hint, &skew);
    skewed = skewed || skew > 1e-6;
    return t;
  };
  if (bones)
    for (auto& c : arr(d, "bones")) {
      const Str id = c.value("id", Str()), parent = parentOf(c);
      if (!changed.count(parent) || keep.count(id) || !oldWorld.count(parent) || !newWorld.count(parent)) continue;
      writeT(c, rebase(parent, local(readT(c)), readT(c)), fine);
      keyed = keyed || targetHasKeys(d, "bone:" + id);
      for (const auto& e : listOr(d, "expressions")) keyed = keyed || (e.contains("bones") && e.at("bones").is_object() && e.at("bones").contains(id));
      r.affected.push_back("bone:" + id);
    }
  if (images) {
    std::map<Str, Str> slotBone;
    for (const auto& s : arr(d, "slots")) if (changed.count(s.value("bone", Str()))) slotBone[s.value("id", Str())] = s.value("bone", Str());
    auto place = [&](Json& list) {
      for (auto& a : list) {
        if (!a.is_object() || !slotBone.count(a.value("slot", Str()))) continue;
        const Str b = slotBone[a.value("slot", Str())];
        if (!oldWorld.count(b) || !newWorld.count(b)) continue;
        writeT(a, rebase(b, local(readT(a)), readT(a)), fine);
        r.affected.push_back("slot:" + a.value("slot", Str()));
      }
    };
    place(arr(d, "attachments"));
    if (d.contains("skins") && d["skins"].is_array())
      for (auto& sk : d["skins"]) if (sk.is_object() && sk.contains("attachments") && sk["attachments"].is_array()) place(sk["attachments"]);
  }
  if (skewed) warn(r, "ANIM_COMPENSATE_SKEW", "", "비균일 크기 때문에 보정한 자식·그림에 기울임이 생겨 조금 달라질 수 있습니다.");
  if (keyed) warn(r, "ANIM_COMPENSATE_KEYS", "", "보정한 자식 본의 기존 키·표정 값은 이전 기본 자세 기준입니다(보정은 기본 자세만 바꿉니다).");
}
void setBone(EditResult& r, const Json& op) {
  const Str id = reqStr(op, "bone");
  const bool compBones = flag(op, "compensateBones"), compImages = flag(op, "compensateImages");
  const Json before = compBones || compImages ? r.data : Json();
  Json& b = bone(r.data, id);
  Transform t = readT(b);
  if (const Axes axes = axesOf(op); axes != Axes::Local && (has(op, "x") || has(op, "y") || has(op, "rotation"))) {
    // Values given in parent/world axes are converted through the parent's setup world.
    const Str parent = parentOf(b);
    const Affine pw = parent.empty() ? Affine{} : setupWorld(r.data).at(parent);
    Transform shown = toAxes(t, pw, axes);
    shown.x = num(op, "x", shown.x); shown.y = num(op, "y", shown.y); shown.rotation = num(op, "rotation", shown.rotation);
    const auto back = fromAxes(shown, pw, axes, t);
    if (!back) throw EditError("부모 본의 크기가 0이라 " + optStr(op, "axes") + " 값으로 바꿀 수 없습니다.");
    t.x = back->x; t.y = back->y; t.rotation = back->rotation;
  } else { t.x = num(op, "x", t.x); t.y = num(op, "y", t.y); t.rotation = num(op, "rotation", t.rotation); }
  t.scaleX = num(op, "scaleX", t.scaleX); t.scaleY = num(op, "scaleY", t.scaleY);
  writeT(b, t);
  const bool hadLength = boneLength(b) > 0;
  writeLook(b, op);
  if (!hadLength && boneLength(bone(r.data, id)) > 0) pinLimbTips(r, id);
  compensate(r, before, {id}, {id}, compBones, compImages);
  r.affected.push_back("bone:" + id);
}
/**
 * transformBones {bones, delta: {x?, y?, rotation?, scaleX?, scaleY?}, space?, pivot?, clip?, timeMs?,
 * compensateBones?, compensateImages?} (plugin 1.7.0): one gesture on a bone selection; the canvas
 * drag of a selection (one bone or many) and the LLM share it.
 * - x/y: a translation in rig px (`space` "world", default) or added to the local x/y ("parent").
 *   Only the topmost selected bones move: a selected descendant already follows its parent.
 * - rotation (degrees, clockwise on screen) / scaleX, scaleY (factors) about `pivot`:
 *   "origin" (default): every selected bone turns / scales about its own origin (local rotation
 *   += δ, a mirrored parent flips the sign; local scale ×= f), so a selected chain curls;
 *   "centroid" (the mean of the selected bones' origins) or {x, y} (rig px): the topmost bones turn
 *   as a group (origins rotated / scaled about the pivot in rig axes, rotation += δ, scale ×= f).
 * Values come from the pose shown (FK + IK, no expressions); with `clip` + `timeMs` the result is
 * keyed there (x+y, rotation, scaleX+scaleY: the channel groups that change), otherwise the setup
 * pose is edited. compensateBones / compensateImages keep unselected children / attached images in
 * place (setup pose only; with a clip they are ignored, warning ANIM_COMPENSATE_SETUP_ONLY).
 * extra.bones = the bones that changed.
 */
void setKey(EditResult& r, const Json& op);
void transformBones(EditResult& r, const Json& op) {
  Json& d = r.data;
  if (!has(op, "bones") || !op.at("bones").is_array() || op.at("bones").empty()) throw EditError("bones는 바꿀 본 ID 배열입니다.");
  std::vector<Str> ids;
  for (const auto& v : op.at("bones")) {
    const Str id = v.is_string() ? v.get<Str>() : Str();
    bone(d, id);
    if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
  }
  const std::set<Str> selected(ids.begin(), ids.end());
  const Json delta = has(op, "delta") ? op.at("delta") : Json::object();
  if (!delta.is_object()) throw EditError("delta는 {x, y, rotation, scaleX, scaleY} 객체입니다.");
  const double dx = num(delta, "x", 0), dy = num(delta, "y", 0), dr = num(delta, "rotation", 0), fx = num(delta, "scaleX", 1), fy = num(delta, "scaleY", 1);
  if (fx == 0 || fy == 0) throw EditError("크기 배율은 0이 될 수 없습니다.");
  const Str space = optStr(op, "space", "world");
  if (space != "world" && space != "parent") throw EditError("space는 world 또는 parent입니다.");
  Rig rig;
  try { rig = parseRig("transform", d); } catch (const std::exception& e) { throw EditError(Str("리그를 읽을 수 없습니다: ") + e.what()); }
  for (auto& c : rig.clips) c.loop = false;  // a key at the clip's end samples the end pose, not time 0
  const bool animate = has(op, "clip");
  int64_t t = 0;
  if (animate) {
    t = ms(op, "timeMs");
    const Json& c = clip(d, reqStr(op, "clip"));
    if (t < 0 || t > c.value("durationMs", int64_t(0))) throw EditError("키 시간이 클립 범위 밖입니다.");
  }
  Look look;
  look.expressions = false;  // keyable values, as the properties panel shows them
  const Pose pose = samplePose(rig, animate ? rig.clipIndex(optStr(op, "clip")) : -1, double(t), look);
  auto parentWorld = [&](int i) { const int p = rig.bones[size_t(i)].parentIndex; return p >= 0 ? pose.world[size_t(p)] : Affine{}; };
  // Pivot: each bone's own origin, the selection centroid, or a rig-space point.
  bool group = false;
  double cx = 0, cy = 0;
  if (has(op, "pivot") && op.at("pivot").is_object()) { group = true; cx = num(op.at("pivot"), "x", 0); cy = num(op.at("pivot"), "y", 0); }
  else if (const Str pivot = optStr(op, "pivot", "origin"); pivot == "centroid") {
    group = true;
    for (const Str& id : ids) { const auto& w = pose.world[size_t(rig.boneIndex(id))]; cx += w.e / double(ids.size()); cy += w.f / double(ids.size()); }
  } else if (pivot != "origin") throw EditError("pivot은 origin, centroid 또는 {x, y}입니다.");
  auto topmost = [&](int i) {
    for (int p = rig.bones[size_t(i)].parentIndex, guard = 0; p >= 0 && guard++ < int(rig.bones.size()); p = rig.bones[size_t(p)].parentIndex)
      if (selected.count(rig.bones[size_t(p)].id)) return false;
    return true;
  };
  struct Change { Str id; Transform next; bool moved, turned, scaled; };
  std::vector<Change> changes;
  for (const Str& id : ids) {
    const int i = rig.boneIndex(id);
    const bool top = topmost(i);
    if (group && !top) continue;  // follows its selected ancestor as part of the group
    const Transform cur = pose.bones[size_t(i)];
    Transform next = cur;
    const Affine pw = parentWorld(i);
    const double sign = pw.a * pw.d - pw.b * pw.c < 0 ? -1 : 1;
    next.rotation += sign * dr;
    next.scaleX *= fx;
    next.scaleY *= fy;
    double wx = pose.world[size_t(i)].e, wy = pose.world[size_t(i)].f;
    if (group) {
      // The origin turns and scales about the pivot in rig axes.
      const double ox = (wx - cx) * fx, oy = (wy - cy) * fy, cs = std::cos(dr * PI / 180), sn = std::sin(dr * PI / 180);
      wx = cx + cs * ox - sn * oy;
      wy = cy + sn * ox + cs * oy;
    }
    const bool translate = top && (dx != 0 || dy != 0);
    if (translate && space == "world") { wx += dx; wy += dy; }
    if (group || (translate && space == "world")) {
      const auto inv = pw.inverse();
      if (!inv) throw EditError("본 「" + id + "」의 부모 크기가 0이라 옮길 수 없습니다.");
      inv->apply(wx, wy, next.x, next.y);
    }
    if (translate && space == "parent") { next.x += dx; next.y += dy; }
    const bool moved = translate || std::fabs(next.x - cur.x) > 1e-9 || std::fabs(next.y - cur.y) > 1e-9;
    const bool turned = dr != 0, scaled = fx != 1 || fy != 1;
    if (moved || turned || scaled) changes.push_back({id, next, moved, turned, scaled});
  }
  if (changes.empty()) throw EditError("바뀐 본이 없습니다(delta가 비어 있습니다).");
  const bool compBones = flag(op, "compensateBones"), compImages = flag(op, "compensateImages");
  std::vector<Str> changed;
  if (animate) {
    if (compBones || compImages) warn(r, "ANIM_COMPENSATE_SETUP_ONLY", "", "본·이미지 보정은 조립(기본 자세)에서만 적용됩니다. 키에는 적용하지 않았습니다.");
    const Str clipId = reqStr(op, "clip");
    for (const auto& c : changes) {
      auto key = [&](const char* prop, double v) { setKey(r, {{"clip", clipId}, {"target", "bone:" + c.id}, {"property", prop}, {"timeMs", t}, {"value", v}}); };
      if (c.moved) { key("x", c.next.x); key("y", c.next.y); }
      if (c.turned) key("rotation", c.next.rotation);
      if (c.scaled) { key("scaleX", c.next.scaleX); key("scaleY", c.next.scaleY); }
      changed.push_back(c.id);
    }
  } else {
    const Json before = compBones || compImages ? d : Json();
    for (const auto& c : changes) {
      Json& b = bone(d, c.id);
      Transform s = readT(b);
      if (c.moved) { s.x = c.next.x; s.y = c.next.y; }
      if (c.turned) s.rotation = c.next.rotation;
      if (c.scaled) { s.scaleX = c.next.scaleX; s.scaleY = c.next.scaleY; }
      writeT(b, s);
      r.affected.push_back("bone:" + c.id);
      changed.push_back(c.id);
    }
    compensate(r, before, std::set<Str>(changed.begin(), changed.end()), selected, compBones, compImages);
  }
  r.extra["bones"] = changed;
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
  if (const Str problem = ikProblem(d); !problem.empty()) throw EditError("IK 제약이 깨져 옮기지 않았습니다: " + problem);
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
  // IK constraints whose chain or target is this bone cannot survive it: removed with cascade.
  const std::vector<Str> iks = ikUsing(d, id);
  if (!iks.empty() && !op.value("cascade", false))
    throw EditError("IK 제약 " + refListOf(iks) + "이(가) 이 본을 씁니다. 함께 삭제하려면 cascade를 켜세요.");
  for (const auto& ik : iks) {
    removeIk(d, ik);
    r.affected.push_back("ik:" + ik);
    warn(r, "ANIM_IK_REMOVED", "", "본을 지우면서 IK 제약 「" + ik + "」도 지웠습니다.");
  }
  // An explicit effector that goes away falls back to the implicit rule (the lower bone's first child).
  if (d.contains("ik") && d["ik"].is_array())
    for (auto& k : d["ik"])
      if (k.is_object() && optStr(k, "tip") == id) {
        k.erase("tip");
        r.affected.push_back("ik:" + optStr(k, "id"));
        warn(r, "ANIM_IK_TIP_REMOVED", "", "IK 「" + optStr(k, "id") + "」의 끝점 본을 지워, 아래 본의 첫 자식 본을 끝점으로 씁니다.");
      }
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
  dropBoneWeights(r, id);
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
std::set<Str> retargetClipEnds(EditResult& r, const Str& deleted);
void deleteSlot(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str id = reqStr(op, "slot");
  slot(d, id);
  // Clipping slots whose range ended here and is now empty go with it (nothing left to clip).
  std::set<Str> ids = retargetClipEnds(r, id);
  ids.insert(id);
  auto& slots = arr(d, "slots");
  for (const auto& s : ids) {
    slots.erase(slots.begin() + findIndex(slots, "id", s));
    auto& atts = arr(d, "attachments");
    atts.erase(std::remove_if(atts.begin(), atts.end(), [&](const Json& a) { return a.value("slot", Str()) == s; }), atts.end());
    removeTracks(d, {"slot:" + s});
    purgeLookRefs(d, {s}, {});
    r.affected.push_back("slot:" + s);
  }
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
  const bool deform = tg.property == Property::Deform, sequence = tg.property == Property::Sequence;
  if (!op.contains("value") && !deform && !sequence) throw EditError("키 값이 필요합니다.");
  Json value = op.contains("value") ? op.at("value") : Json();
  if (sequence) {
    // {mode, index, delay} (plugin 1.12.0): frames of the slot's sequence attachment, step only.
    const size_t frames = sequenceFrames(d, tg.id);
    if (!frames) throw EditError("이 조각에는 시퀀스 첨부가 없습니다: " + tg.id + " (먼저 「시퀀스 만들기」로 프레임을 넣으세요)");
    const Str mode = optStr(op, "mode", "hold");
    if (!parseSequenceMode(mode)) throw EditError("시퀀스 재생 방식은 hold, once, loop, pingpong, onceReverse, loopReverse, pingpongReverse 중 하나입니다: " + mode);
    const Json index = op.contains("index") ? op.at("index") : Json(0);
    if (!index.is_number_integer() || index.get<int64_t>() < 0 || index.get<int64_t>() >= int64_t(frames)) throw EditError("시퀀스 프레임 번호는 0.." + std::to_string(frames - 1) + " 정수입니다.");
    const double delay = num(op, "delay", 100);
    if (!(delay > 0) || delay > 60000) throw EditError("프레임 간격(delay)은 0보다 크고 60000ms 이하입니다.");
    value = Json{{"mode", mode}, {"index", index}, {"delay", clean(delay)}};
  } else if (deform) {
    // offsets = dx, dy per vertex in image pixels; absent = the rest shape (all zero).
    const std::set<size_t> counts = meshCounts(d, tg.id, optStr(op, "attachment"));
    if (counts.empty()) throw EditError("메시가 아닌(또는 없는) 첨부입니다: " + tg.id + "/" + optStr(op, "attachment") + " (먼저 메시로 변환하세요)");
    value = Json::array();
    if (has(op, "offsets")) {
      if (!op.at("offsets").is_array()) throw EditError("offsets는 정점마다 dx, dy 숫자 배열입니다.");
      for (const auto& v : op.at("offsets")) {
        if (!v.is_number() || !std::isfinite(v.get<double>()) || std::fabs(v.get<double>()) > 1e6) throw EditError("offsets 값은 유한한 숫자여야 합니다.");
        value.push_back(clean(v.get<double>()));
      }
    } else value = Json(std::vector<double>(*counts.begin() * 2, 0.0));
    if (value.size() % 2 || !counts.count(value.size() / 2))
      throw EditError("offsets 개수(" + std::to_string(value.size()) + ")가 메시 정점 수 × 2(" + std::to_string(*counts.begin() * 2) + ")와 다릅니다.");
  } else if (tg.property == Property::BendPositive) {
    if (value.is_number() && (value == 0 || value == 1)) value = value == 1;  // 0/1 from numeric tools
    if (!value.is_boolean()) throw EditError("IK 굽힘 키 값은 true 또는 false입니다.");
  } else if (tg.property == Property::Expression) {
    if (!value.is_null() && (!value.is_string() || findIndex(listOr(d, "expressions"), "id", value.get<Str>()) < 0))
      throw EditError("없는 표정입니다: " + (value.is_string() ? value.get<Str>() : value.dump()));
  } else if (tg.property == Property::Attachment) {
    if (!value.is_null() && (!value.is_string() || !attachmentNames(d, tg.id).count(value.get<Str>())))
      throw EditError("이 조각에 없는 첨부입니다: " + (value.is_string() ? value.get<Str>() : value.dump()));
  } else {
    const double v = num(op, "value", 0);
    if (!value.is_number()) throw EditError("키 값은 숫자여야 합니다.");
    if (tg.property == Property::Alpha && (v < 0 || v > 1)) throw EditError("투명도는 0..1이어야 합니다.");
    if (tg.property == Property::Mix && (v < 0 || v > 1)) throw EditError("IK 혼합(mix)은 0..1이어야 합니다.");
    value = clean(v);
  }
  const Str targetText = op.at("target").get<Str>(), prop = op.at("property").get<Str>(), attachment = deform ? optStr(op, "attachment") : Str();
  Json* tr = findTrack(c, targetText, prop, attachment);
  if (!tr) {
    Json track{{"target", targetText}, {"property", prop}, {"keys", Json::array()}};
    if (deform) track["attachment"] = attachment;
    arr(c, "tracks").push_back(track);
    tr = &arr(c, "tracks").back();
  }
  Json* key = nullptr;
  for (auto& k : arr(*tr, "keys")) if (k.value("timeMs", int64_t(-1)) == t) key = &k;
  if (!key) { arr(*tr, "keys").push_back({{"timeMs", t}}); key = &arr(*tr, "keys").back(); }
  if (sequence) { key->erase("value"); for (auto it = value.begin(); it != value.end(); ++it) (*key)[it.key()] = it.value(); }
  else (*key)[deform ? "offsets" : "value"] = value;
  const bool step = stepOnly(prop);
  if (step) { key->erase("interp"); key->erase("curve"); }
  sortKeys(*tr);
  // After sorting: "auto" handles read the neighbours including this key's new value.
  if (!step) writeInterp(keyAt(*tr, t), op, *tr);
  r.affected.push_back(keyRef(clipId, op, t));
}
Json& existingTrack(Json& d, const Json& op, Json*& clipOut) {
  const Str clipId = reqStr(op, "clip");
  target(d, op);
  clipOut = &clip(d, clipId);
  Json* tr = findTrack(*clipOut, op.at("target").get<Str>(), op.at("property").get<Str>(), optStr(op, "attachment"));
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
  if (stepOnly(op.value("property", Str()))) throw EditError("첨부·표정·IK 굽힘 키는 항상 계단(step) 전환입니다.");
  const int64_t t = ms(op, "timeMs");
  if (!has(op, "preset")) reqStr(op, "interp");
  writeInterp(keyAt(tr, t), op, tr);
  r.affected.push_back(keyRef(op.at("clip").get<Str>(), op, t));
}
void setCurve(EditResult& r, const Json& op) {
  Json* c = nullptr;
  Json& tr = existingTrack(r.data, op, c);
  if (stepOnly(op.value("property", Str()))) throw EditError("첨부·표정·IK 굽힘 키는 항상 계단(step) 전환입니다.");
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
struct Picked {
  Json* track; std::set<int64_t> times; Str target, property, attachment;
  Str refProperty() const { return property == "deform" ? property + ":" + attachment : property; }
};
Json refJson(const Str& target, const Str& property, int64_t t, const Str& attachment = "") {
  Json j{{"target", target}, {"property", property}, {"timeMs", t}};
  if (property == "deform") j["attachment"] = attachment;
  return j;
}
// ---- Clip events (plugin 1.8.0) ----
// Keys live in clips[].events as {timeMs, name, int?, float?, string?}, identified by (timeMs, name)
// (the first match when a document repeats one); rig-level `events` holds optional definitions.
// Multi-key ops take them as refs {target: "event", property: <name>, timeMs}.
constexpr const char* EVENT_TARGET = "event";
constexpr int64_t EVENT_INT_LIMIT = 2147483647;
const char* const EVENT_PAYLOAD[] = {"int", "float", "string"};
bool isEventRef(const Json& k) { return k.is_object() && k.value("target", Str()) == EVENT_TARGET; }
bool eventDefined(const Json& d, const Str& name) { return findIndex(listOr(d, "events"), "name", name) >= 0; }
Json& eventDefAt(Json& d, const Str& name) {
  const int i = findIndex(arr(d, "events"), "name", name);
  if (i < 0) throw EditError("없는 이벤트 정의입니다: " + name);
  return d["events"][size_t(i)];
}
void checkEventName(const Str& name) {
  if (!validName(name)) throw EditError("이벤트 이름은 영문·숫자로 시작하고 영문·숫자·_·-만 씁니다(64자 이하): " + name);
}
/** Key names must be defined once the rig has definitions (else any valid name is a new event). */
void requireEventName(const Json& d, const Str& name) {
  checkEventName(name);
  if (!listOr(d, "events").empty() && !eventDefined(d, name)) throw EditError("정의되지 않은 이벤트입니다. 먼저 이벤트 목록에 추가하세요: " + name);
}
int findEvent(Json& c, int64_t t, const Str& name) {
  auto& events = arr(c, "events");
  for (size_t i = 0; i < events.size(); i++) if (events[i].value("timeMs", int64_t(-1)) == t && events[i].value("name", Str()) == name) return int(i);
  return -1;
}
int eventAt(Json& c, int64_t t, const Str& name) {
  const int i = findEvent(c, t, name);
  if (i < 0) throw EditError("그 시간에 이벤트 키가 없습니다: " + name + " " + std::to_string(t) + "ms");
  return i;
}
void sortEvents(Json& c) {
  auto& events = arr(c, "events");
  std::stable_sort(events.begin(), events.end(), [](const Json& a, const Json& b) { return a.value("timeMs", int64_t(0)) < b.value("timeMs", int64_t(0)); });
  if (events.empty()) c.erase("events");
}
Str eventRef(const Str& clipId, const Str& name, int64_t t) { return "key:" + clipId + "/" + EVENT_TARGET + "/" + name + "@" + std::to_string(t); }
/** int/float/string (+ audio, volume, balance for definitions) from `op` onto `e`; null removes a field. */
void writeEventPayload(Json& e, const Json& op, bool definition) {
  for (const char* k : EVENT_PAYLOAD) {
    if (!op.contains(k)) continue;
    const Json& v = op.at(k);
    if (v.is_null()) { e.erase(k); continue; }
    if (Str(k) == "int") {
      if (!v.is_number() || v.get<double>() != std::floor(v.get<double>()) || std::fabs(v.get<double>()) > double(EVENT_INT_LIMIT)) throw EditError("이벤트 int 값은 32비트 정수입니다.");
      e[k] = int64_t(v.get<double>());
    } else if (Str(k) == "float") e[k] = clean(num(op, k, 0));
    else {
      if (!v.is_string() || v.get_ref<const Str&>().size() > 1000) throw EditError("이벤트 string 값은 1000바이트 이하의 문자열입니다.");
      e[k] = v;
    }
  }
  if (!definition) return;
  if (op.contains("audio")) {
    const Json& v = op.at("audio");
    if (v.is_null() || (v.is_string() && v.get_ref<const Str&>().empty())) e.erase("audio");
    else if (!v.is_string() || v.get_ref<const Str&>().size() > 64) throw EditError("이벤트 소리(audio)는 오디오 에셋 ID입니다.");
    else e["audio"] = v;
  }
  // Defaults are not stored: volume 1 and balance 0 are what a missing field means.
  if (op.contains("volume")) {
    const double v = num(op, "volume", 1);
    if (v < 0 || v > 1) throw EditError("이벤트 음량(volume)은 0..1입니다.");
    if (op.at("volume").is_null() || clean(v) == 1) e.erase("volume"); else e["volume"] = clean(v);
  }
  if (op.contains("balance")) {
    const double v = num(op, "balance", 0);
    if (v < -1 || v > 1) throw EditError("이벤트 좌우(balance)는 -1..1입니다.");
    if (op.at("balance").is_null() || clean(v) == 0) e.erase("balance"); else e["balance"] = clean(v);
  }
}
/** Event names used by keys of any clip, in first-use order. */
std::vector<Str> usedEventNames(Json& d) {
  std::vector<Str> out;
  for (auto& c : arr(d, "clips"))
    for (const auto& e : listOr(c, "events")) {
      const Str n = e.value("name", Str());
      if (!n.empty() && std::find(out.begin(), out.end(), n) == out.end()) out.push_back(n);
    }
  return out;
}
/** addEventDef {name, int?, float?, string?, audio?, volume?, balance?}. The first definition also
 *  defines every name keys already use (extra.defined), so the rig stays valid. */
void addEventDef(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str name = reqStr(op, "name");
  checkEventName(name);
  if (eventDefined(d, name)) throw EditError("이미 있는 이벤트 이름입니다: " + name);
  Json defined = Json::array();
  if (listOr(d, "events").empty())
    for (const auto& used : usedEventNames(d))
      if (used != name && validName(used)) { arr(d, "events").push_back({{"name", used}}); defined.push_back(used); }
  Json e{{"name", name}};
  writeEventPayload(e, op, true);
  arr(d, "events").push_back(e);
  r.extra["defined"] = defined;
  r.affected.push_back("event:" + name);
}
/** setEventDef {event, int?, float?, string?, audio?, volume?, balance?}: null removes a field. */
void setEventDef(EditResult& r, const Json& op) {
  const Str name = reqStr(op, "event");
  writeEventPayload(eventDefAt(r.data, name), op, true);
  r.affected.push_back("event:" + name);
}
/** renameEventDef {event, name}: every key of the old name in every clip follows (extra.renamedKeys). */
void renameEventDef(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str from = reqStr(op, "event"), to = reqStr(op, "name");
  Json& e = eventDefAt(d, from);
  if (from == to) return;
  checkEventName(to);
  if (eventDefined(d, to)) throw EditError("이미 있는 이벤트 이름입니다: " + to);
  e["name"] = to;
  size_t renamed = 0;
  for (auto& c : arr(d, "clips"))
    for (auto& k : arr(c, "events")) if (k.value("name", Str()) == from) { k["name"] = to; renamed++; }
  for (auto& c : arr(d, "clips")) if (listOr(c, "events").empty()) c.erase("events");
  r.extra["renamedKeys"] = renamed;
  r.affected.push_back("event:" + to);
}
/** deleteEventDef {event, force?}: refused while keys use it unless force (then they go too). */
void deleteEventDef(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str name = reqStr(op, "event");
  eventDefAt(d, name);
  size_t used = 0;
  for (auto& c : arr(d, "clips")) for (const auto& k : listOr(c, "events")) used += k.value("name", Str()) == name;
  if (used && !op.value("force", false)) throw EditError("이 이벤트를 쓰는 키가 " + std::to_string(used) + "개 있습니다. 키까지 지우려면 force를 지정하세요.");
  for (auto& c : arr(d, "clips")) {
    auto& events = arr(c, "events");
    events.erase(std::remove_if(events.begin(), events.end(), [&](const Json& k) { return k.value("name", Str()) == name; }), events.end());
    if (events.empty()) c.erase("events");
  }
  auto& defs = arr(d, "events");
  defs.erase(defs.begin() + findIndex(defs, "name", name));
  if (defs.empty()) d.erase("events");
  r.extra["removedKeys"] = used;
  r.affected.push_back("event:" + name);
}
/**
 * setEventKey {clip, timeMs, name, newName?, int?, float?, string?}: creates the key (timeMs, name)
 * or edits it; newName renames that one key; payload fields override the definition (null = use
 * the definition's value again).
 */
void setEventKey(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str clipId = reqStr(op, "clip"), name = reqStr(op, "name");
  Json& c = clip(d, clipId);
  const int64_t t = ms(op, "timeMs"), duration = c.value("durationMs", int64_t(0));
  if (t < 0 || t > duration) throw EditError("이벤트 시간이 클립 범위(0.." + std::to_string(duration) + "ms) 밖입니다.");
  int i = findEvent(c, t, name);
  if (i < 0) {
    requireEventName(d, name);
    arr(c, "events").push_back({{"timeMs", t}, {"name", name}});
    i = int(c["events"].size()) - 1;
  }
  Json& e = c["events"][size_t(i)];
  Str now = name;
  if (has(op, "newName") && optStr(op, "newName") != name) {
    now = reqStr(op, "newName");
    requireEventName(d, now);
    if (findEvent(c, t, now) >= 0) throw EditError("같은 시간에 이미 그 이벤트 키가 있습니다: " + now);
    e["name"] = now;
  }
  writeEventPayload(e, op, false);
  sortEvents(c);
  r.extra["keys"] = Json::array({refJson(EVENT_TARGET, now, t)});
  r.affected.push_back(eventRef(clipId, now, t));
}
/** moveEventKey {clip, name, fromMs, toMs}: refused on range and on a same-name key at toMs. */
void moveEventKey(EditResult& r, const Json& op) {
  Json& c = clip(r.data, reqStr(op, "clip"));
  const Str name = reqStr(op, "name");
  const int64_t from = ms(op, "fromMs"), to = ms(op, "toMs"), duration = c.value("durationMs", int64_t(0));
  const int i = eventAt(c, from, name);
  if (from == to) return;
  if (to < 0 || to > duration) throw EditError("이벤트 시간이 클립 범위(0.." + std::to_string(duration) + "ms) 밖입니다.");
  if (findEvent(c, to, name) >= 0) throw EditError("같은 시간에 이미 그 이벤트 키가 있습니다: " + name + " " + std::to_string(to) + "ms");
  c["events"][size_t(i)]["timeMs"] = to;
  sortEvents(c);
  r.affected.push_back(eventRef(op.at("clip").get<Str>(), name, to));
}
void deleteEventKey(EditResult& r, const Json& op) {
  Json& c = clip(r.data, reqStr(op, "clip"));
  const Str name = reqStr(op, "name");
  const int64_t t = ms(op, "timeMs");
  auto& events = arr(c, "events");
  events.erase(events.begin() + eventAt(c, t, name));
  if (events.empty()) c.erase("events");
  r.affected.push_back(eventRef(op.at("clip").get<Str>(), name, t));
}

/** Selected event keys (plugin 1.8.0): indices into the clip's `events`, each once. */
using PickedEvents = std::set<size_t>;
std::vector<Picked> pickKeys(Json& d, Json& c, const Json& op, PickedEvents* events = nullptr) {
  if (!has(op, "keys") || !op.at("keys").is_array() || op.at("keys").empty() || op.at("keys").size() > MAX_KEYS)
    throw EditError("keys는 1.." + std::to_string(MAX_KEYS) + "개의 [{target, property, timeMs}] 배열입니다.");
  std::vector<Picked> out;
  for (const auto& k : op.at("keys")) {
    if (!k.is_object()) throw EditError("keys 항목은 {target, property, timeMs} 객체입니다.");
    if (isEventRef(k)) {
      if (!events) throw EditError("이 명령은 이벤트 키를 다루지 않습니다.");
      events->insert(size_t(eventAt(c, ms(k, "timeMs"), reqStr(k, "property"))));
      continue;
    }
    target(d, k);
    const Str tg = k.at("target").get<Str>(), prop = k.at("property").get<Str>(), attachment = prop == "deform" ? optStr(k, "attachment") : Str();
    const int64_t t = ms(k, "timeMs");
    Json* tr = findTrack(c, tg, prop, attachment);
    if (!tr) throw EditError("이 대상·속성에는 키가 없습니다: " + tg + " " + prop);
    keyAt(*tr, t);
    auto it = std::find_if(out.begin(), out.end(), [&](const Picked& p) { return p.track == tr; });
    if (it == out.end()) { out.push_back({tr, {}, tg, prop, attachment}); it = out.end() - 1; }
    it->times.insert(t);  // duplicates in the list are harmless
  }
  return out;
}
/** Earliest / latest time of a selection (tracks and event keys). */
void selectionSpan(Json& c, const std::vector<Picked>& picked, const PickedEvents& events, int64_t& lo, int64_t& hi) {
  lo = INT64_MAX; hi = INT64_MIN;
  for (const auto& p : picked) { lo = std::min(lo, *p.times.begin()); hi = std::max(hi, *p.times.rbegin()); }
  for (size_t i : events) { const int64_t t = c["events"][i].value("timeMs", int64_t(0)); lo = std::min(lo, t); hi = std::max(hi, t); }
}
/** Re-time the picked keys with `map`; refuses out-of-range, merged and colliding keys before changing anything. */
void retimeKeys(EditResult& r, Json& c, const Str& clipId, std::vector<Picked>& picked, const std::function<int64_t(int64_t)>& map, const PickedEvents& events = {}) {
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
  // Event keys collide only with keys of the same name (different events may share a time).
  if (!events.empty()) {
    std::set<std::pair<int64_t, Str>> mapped;
    auto& list = arr(c, "events");
    for (size_t i : events) {
      const int64_t to = map(list[i].value("timeMs", int64_t(0)));
      const Str name = list[i].value("name", Str());
      if (to < 0 || to > duration) throw EditError("옮긴 이벤트가 클립 범위(0.." + std::to_string(duration) + "ms) 밖으로 나갑니다: " + std::to_string(to) + "ms");
      if (!mapped.insert({to, name}).second) throw EditError("간격이 너무 좁아져 이벤트가 같은 시간에 겹칩니다: " + name + " " + std::to_string(to) + "ms");
    }
    for (size_t i = 0; i < list.size(); i++)
      if (!events.count(i) && mapped.count({list[i].value("timeMs", int64_t(-1)), list[i].value("name", Str())}))
        throw EditError("같은 시간에 이미 그 이벤트 키가 있습니다: " + list[i].value("name", Str()) + " " + std::to_string(list[i].value("timeMs", int64_t(-1))) + "ms");
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
      moved.push_back(refJson(p.target, p.property, to, p.attachment));
      r.affected.push_back("key:" + clipId + "/" + p.target + "/" + p.refProperty() + "@" + std::to_string(to));
    }
    // Normalized curves ride along with their key: segment shapes stay, only their timing changes.
    sortKeys(*p.track);
  }
  if (!events.empty()) {
    auto& list = arr(c, "events");
    for (size_t i : events) {
      const int64_t to = map(list[i].value("timeMs", int64_t(0)));
      list[i]["timeMs"] = to;
      moved.push_back(refJson(EVENT_TARGET, list[i].value("name", Str()), to));
      r.affected.push_back(eventRef(clipId, list[i].value("name", Str()), to));
    }
    sortEvents(c);
  }
  r.extra["keys"] = moved;
}
void moveKeys(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str clipId = reqStr(op, "clip");
  Json& c = clip(d, clipId);
  PickedEvents events;
  auto picked = pickKeys(d, c, op, &events);
  int64_t delta = ms(op, "deltaMs");
  int64_t lo, hi;
  selectionSpan(c, picked, events, lo, hi);
  // clamp: a drag past the clip edge stops the whole group there and keeps its spacing.
  if (op.value("clamp", false)) delta = std::clamp<int64_t>(delta, -lo, std::max<int64_t>(-lo, c.value("durationMs", int64_t(0)) - hi));
  r.extra["deltaMs"] = delta;
  if (delta == 0) { r.extra["keys"] = Json::array(); return; }
  retimeKeys(r, c, clipId, picked, [delta](int64_t t) { return t + delta; }, events);
}
void scaleKeys(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str clipId = reqStr(op, "clip");
  Json& c = clip(d, clipId);
  PickedEvents events;
  auto picked = pickKeys(d, c, op, &events);
  const double factor = num(op, "factor", 1);
  if (!(factor > 0) || factor > 1000) throw EditError("factor(시간 배율)는 0보다 크고 1000 이하여야 합니다.");
  int64_t pivot, hi;
  selectionSpan(c, picked, events, pivot, hi);
  if (has(op, "pivotMs")) pivot = ms(op, "pivotMs");
  if (factor == 1) { r.extra["keys"] = Json::array(); return; }
  retimeKeys(r, c, clipId, picked, [pivot, factor](int64_t t) { return pivot + int64_t(std::llround(double(t - pivot) * factor)); }, events);
}
void deleteKeys(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str clipId = reqStr(op, "clip");
  Json& c = clip(d, clipId);
  PickedEvents events;
  auto picked = pickKeys(d, c, op, &events);
  for (auto& p : picked) {
    auto& keys = arr(*p.track, "keys");
    keys.erase(std::remove_if(keys.begin(), keys.end(), [&](const Json& k) { return p.times.count(k.value("timeMs", int64_t(-1))) > 0; }), keys.end());
    for (int64_t t : p.times) r.affected.push_back("key:" + clipId + "/" + p.target + "/" + p.refProperty() + "@" + std::to_string(t));
  }
  auto& tracks = arr(c, "tracks");
  tracks.erase(std::remove_if(tracks.begin(), tracks.end(), [](const Json& t) { return !t.contains("keys") || !t.at("keys").is_array() || t.at("keys").empty(); }), tracks.end());
  if (!events.empty()) {
    auto& list = arr(c, "events");
    for (auto it = events.rbegin(); it != events.rend(); ++it) {
      r.affected.push_back(eventRef(clipId, list[*it].value("name", Str()), list[*it].value("timeMs", int64_t(0))));
      list.erase(list.begin() + int64_t(*it));
    }
    if (list.empty()) c.erase("events");
  }
}
/**
 * Clipboard of keys (no data change): extra.clipboard = {keys: [{target, property, offsetMs,
 * value, interp?, curve?}], fromMs} with offsets from the earliest copied key, so it can be
 * pasted at any time of any clip. Numeric keys always carry their interp; attachment keys never do.
 * Event keys copy as {target: "event", property: <name>, offsetMs, int?, float?, string?}.
 */
void copyKeys(EditResult& r, const Json& op) {
  Json& d = r.data;
  Json& c = clip(d, reqStr(op, "clip"));
  PickedEvents events;
  const auto picked = pickKeys(d, c, op, &events);
  int64_t lo, hi;
  selectionSpan(c, picked, events, lo, hi);
  Json keys = Json::array();
  for (const auto& p : picked)
    for (const auto& k : p.track->at("keys")) {
      const int64_t t = k.value("timeMs", int64_t(-1));
      if (!p.times.count(t)) continue;
      Json e{{"target", p.target}, {"property", p.property}, {"offsetMs", t - lo}, {"value", k.contains("value") ? k.at("value") : Json(nullptr)}};
      if (p.property == "deform") { e.erase("value"); e["attachment"] = p.attachment; e["offsets"] = k.contains("offsets") ? k.at("offsets") : Json::array(); }
      if (p.property == "sequence") { e.erase("value"); for (const char* f : {"mode", "index", "delay"}) if (k.contains(f)) e[f] = k.at(f); }
      if (!stepOnly(p.property)) {
        e["interp"] = k.contains("interp") && k.at("interp").is_string() ? k.at("interp") : Json("linear");
        if (e["interp"] == "bezier") e["curve"] = curveJson(curveOf(k));
      }
      keys.push_back(e);
    }
  for (size_t i : events) {
    const Json& k = c["events"][i];
    Json e{{"target", EVENT_TARGET}, {"property", k.value("name", Str())}, {"offsetMs", k.value("timeMs", int64_t(0)) - lo}};
    for (const char* f : EVENT_PAYLOAD) if (k.contains(f)) e[f] = k.at(f);
    keys.push_back(e);
  }
  r.extra["clipboard"] = {{"keys", keys}, {"fromMs", lo}};
}
/**
 * Paste a clipboard at `timeMs` of `clip`. Same-time keys are overwritten (value and, when
 * the entry has one, interpolation). `target` re-targets a clipboard that holds one target
 * only (copy a row, paste onto another bone); event entries always keep their event.
 * Out-of-range offsets refuse the whole paste.
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
    if (reqStr(e, "target") != EVENT_TARGET) targets.insert(reqStr(e, "target"));
    const int64_t offset = ms(e, "offsetMs");
    if (offset < 0) throw EditError("offsetMs는 0 이상이어야 합니다.");
    outside += at + offset > duration;
  }
  const Str retarget = optStr(op, "target");
  if (!retarget.empty() && targets.size() != 1) throw EditError("여러 대상의 키는 한 대상으로 붙여넣을 수 없습니다. 같은 대상으로 붙이세요.");
  if (outside) throw EditError("붙여넣을 키 " + std::to_string(outside) + "개가 클립 범위(0.." + std::to_string(duration) + "ms) 밖입니다. 재생 헤드를 앞으로 옮기세요.");
  Json pasted = Json::array();
  for (const auto& e : op.at("keys")) {
    const int64_t t = at + ms(e, "offsetMs");
    if (e.at("target") == EVENT_TARGET) {
      // An event key at the same time and name is replaced, payload included.
      const Str name = reqStr(e, "property");
      if (const int i = findEvent(c, t, name); i >= 0) for (const char* f : EVENT_PAYLOAD) c["events"][size_t(i)].erase(f);
      Json set{{"clip", clipId}, {"timeMs", t}, {"name", name}};
      for (const char* f : EVENT_PAYLOAD) if (has(e, f)) set[f] = e.at(f);
      setEventKey(r, set);
      pasted.push_back(refJson(EVENT_TARGET, name, t));
      continue;
    }
    const Str tg = retarget.empty() ? e.at("target").get<Str>() : retarget, prop = optStr(e, "property");
    Json set{{"clip", clipId}, {"target", tg}, {"property", prop}, {"timeMs", t}};
    if (e.contains("value")) set["value"] = e.at("value");
    for (const char* k : {"interp", "curve", "attachment", "offsets", "mode", "index", "delay"}) if (has(e, k)) set[k] = e.at(k);
    setKey(r, set);
    pasted.push_back(refJson(tg, prop, t, optStr(e, "attachment")));
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
  purgeDeform(r);  // an outfit-only mesh takes its deform keys along
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

// ---- IK constraints (plugin 1.3.0) ----
/** Constraint pose (the setup pose, constraints applied) of rig data; refuses data that does not parse. */
Pose ikSetupPose(const Json& d, Rig& rig) {
  try { rig = parseRig("ik", d); } catch (const std::exception& e) { throw EditError(Str("리그를 읽을 수 없습니다: ") + e.what()); }
  return samplePose(rig, -1, 0);
}
double optUnit(const Json& op, const char* k) {
  const double v = num(op, k, 1);
  if (v < 0 || v > 1) throw EditError("IK 혼합(mix)은 0..1이어야 합니다.");
  return v;
}
double aimAxisOf(const Json& op) {
  if (!op.at("aimAxis").is_number() || !std::isfinite(op.at("aimAxis").get<double>()) || std::fabs(op.at("aimAxis").get<double>()) > 360)
    throw EditError("조준 축(aimAxis)은 -360..360 사이의 각도(도)입니다.");
  return op.at("aimAxis").get<double>();
}
void putAimAxis(Json& k, double degrees) {
  degrees = clean(degrees);
  if (degrees == 0) k.erase("aimAxis");  // the 1.3.0 default (+x): documents stay as they were
  else k["aimAxis"] = degrees;
}
/**
 * The direction a bone "points" in its own space, for a new aim constraint: toward its first
 * child bone (array order) when that is off the origin, else toward the far end of the setup
 * attachments of its slots (their area-weighted centre; reach = the farthest corner/vertex along
 * that direction). Cutout limbs typically keep rotation 0 with the image hanging down (+y), which
 * is why +x alone aimed them 90° off. false = nothing to infer (no child, centred or no images).
 */
bool naturalAimAxis(const Rig& rig, int bone, double& degrees, double& reach) {
  for (size_t i = 0; i < rig.bones.size(); i++) {
    if (rig.bones[i].parentIndex != bone) continue;
    const Transform& t = rig.bones[i].setup;
    if (std::hypot(t.x, t.y) > 1) {
      degrees = std::atan2(t.y, t.x) * 180 / PI;
      reach = std::hypot(t.x, t.y);
      return true;
    }
    break;
  }
  std::vector<std::pair<double, double>> points;
  double cx = 0, cy = 0, area = 0;
  for (const Slot& s : rig.slots) {
    if (s.boneIndex != bone || !s.attachment) continue;
    const Attachment* a = rig.attachment(s.id, *s.attachment);
    if (!a || !(a->width > 0) || !(a->height > 0)) continue;
    std::vector<std::pair<double, double>> image;
    if (a->mesh) for (size_t v = 0; v + 1 < a->vertices.size(); v += 2) image.push_back({a->vertices[v], a->vertices[v + 1]});
    else image = {{0, 0}, {a->width, 0}, {a->width, a->height}, {0, a->height}};
    if (image.empty()) continue;
    const Affine m = attachmentPlacement(*a);
    double sx = 0, sy = 0;
    for (const auto& [u, v] : image) {
      double x, y;
      m.apply(u, v, x, y);
      points.push_back({x, y});
      sx += x;
      sy += y;
    }
    const double weight = a->width * a->height;
    cx += sx / double(image.size()) * weight;
    cy += sy / double(image.size()) * weight;
    area += weight;
  }
  if (!(area > 0)) return false;
  cx /= area;
  cy /= area;
  const double dist = std::hypot(cx, cy);
  if (dist < 1) return false;  // centred on the bone origin: no direction to read
  double far = 0;
  for (const auto& [x, y] : points) far = std::max(far, (x * cx + y * cy) / dist);
  degrees = std::atan2(cy, cx) * 180 / PI;
  reach = far > 1 ? far : dist;
  return true;
}
// ---- Bone length and orientation (plugin 1.5.0) ----
/** The listed `bones` (validated), or every bone in array order when absent. */
std::vector<Str> boneList(Json& d, const Json& op) {
  std::vector<Str> ids;
  if (has(op, "bones")) {
    if (!op.at("bones").is_array()) throw EditError("bones는 본 ID 배열입니다.");
    for (const auto& v : op.at("bones")) { ids.push_back(v.is_string() ? v.get<Str>() : Str()); bone(d, ids.back()); }
  } else for (const auto& b : arr(d, "bones")) ids.push_back(b.value("id", Str()));
  return ids;
}
/**
 * fitBoneLengths {bones?} ("자동 길이"): each bone's length becomes the distance (in its own
 * space) to its first child bone (array order). Bones without a child keep theirs. A lower bone of
 * a two-bone chain keeps its effector (the child is pinned as `tip`). extra.lengths {id: length}.
 */
void fitBoneLengths(EditResult& r, const Json& op) {
  Json& d = r.data;
  Json lengths = Json::object();
  for (const Str& id : boneList(d, op)) {
    const Json* child = nullptr;
    for (const auto& c : arr(d, "bones")) if (parentOf(c) == id) { child = &c; break; }
    if (!child) { if (has(op, "bones")) warn(r, "ANIM_BONE_NO_CHILD", "", "본 「" + id + "」에는 자식 본이 없어 길이를 그대로 둡니다."); continue; }
    const Transform t = readT(*child);
    const double len = clean(std::hypot(t.x, t.y));
    Json& b = bone(d, id);
    if (len == boneLength(b)) continue;
    const bool had = boneLength(b) > 0;
    writeLook(b, {{"length", len}});
    if (!had && len > 0) pinLimbTips(r, id);
    lengths[id] = len;
    r.affected.push_back("bone:" + id);
  }
  if (lengths.empty()) throw EditError("길이를 바꿀 본이 없습니다(자식 본이 없거나 길이가 이미 맞습니다).");
  r.extra["lengths"] = lengths;
}
/** Uniform scale (scaleX = scaleY at every time): only then does L · R(δ) stay a T·R·S transform
 *  with rotation + δ, so a bone frame can turn without changing what is drawn. */
bool uniformScale(Json& d, const Str& id) {
  const Transform t = readT(bone(d, id));
  if (std::fabs(t.scaleX - t.scaleY) > 1e-12) return false;
  const Str target = "bone:" + id;
  auto keyed = [](const Json* tr) { return tr && tr->contains("keys") && tr->at("keys").is_array() && !tr->at("keys").empty(); };
  for (auto& c : arr(d, "clips")) {
    const Json* sx = findTrack(c, target, "scaleX");
    const Json* sy = findTrack(c, target, "scaleY");
    if (keyed(sx) && keyed(sy)) { if (sx->at("keys") != sy->at("keys")) return false; continue; }
    if (const Json* one = keyed(sx) ? sx : keyed(sy) ? sy : nullptr)
      for (const auto& k : one->at("keys")) if (!k.contains("value") || !k.at("value").is_number() || k.at("value").get<double>() != t.scaleX) return false;
  }
  for (const auto& e : listOr(d, "expressions"))
    if (e.contains("bones") && e.at("bones").is_object() && e.at("bones").contains(id)) {
      const Json& o = e.at("bones").at(id);
      if (o.contains("scaleX") != o.contains("scaleY") || (o.contains("scaleX") && o.at("scaleX") != o.at("scaleY"))) return false;
    }
  return true;
}
/**
 * The x/y keys of a child bone are a vector in its parent's frame: turning that frame rotates the
 * pair, which needs both channels at the same key times with the same interpolation (a linear map
 * commutes with any shared easing). A source (clip or expression) keying one channel alone is
 * fine only while no source keys the other one: that channel is then always its setup value.
 * Returns "" when the child can be rotated, else the reason.
 */
Str pairProblem(Json& d, const Str& child) {
  const Str target = "bone:" + child;
  bool onlyX = false, onlyY = false, anyX = false, anyY = false;
  auto keyed = [](const Json* tr) { return tr && tr->contains("keys") && tr->at("keys").is_array() && !tr->at("keys").empty(); };
  for (auto& c : arr(d, "clips")) {
    const Json* tx = findTrack(c, target, "x");
    const Json* ty = findTrack(c, target, "y");
    const bool kx = keyed(tx), ky = keyed(ty);
    anyX = anyX || kx; anyY = anyY || ky;
    onlyX = onlyX || (kx && !ky); onlyY = onlyY || (ky && !kx);
    if (kx && ky) {
      const Json& a = tx->at("keys");
      const Json& b = ty->at("keys");
      if (a.size() != b.size()) return "자식 본 「" + child + "」의 x·y 키 시간이 서로 다릅니다";
      for (size_t i = 0; i < a.size(); i++)
        if (a[i].value("timeMs", -1.0) != b[i].value("timeMs", -1.0) || a[i].value("interp", Str("linear")) != b[i].value("interp", Str("linear")) ||
            (a[i].value("interp", Str()) == "bezier" && a[i].value("curve", Json()) != b[i].value("curve", Json())))
          return "자식 본 「" + child + "」의 x·y 키 시간이나 보간이 서로 다릅니다";
    }
  }
  for (const auto& e : listOr(d, "expressions"))
    if (e.contains("bones") && e.at("bones").is_object() && e.at("bones").contains(child)) {
      const Json& o = e.at("bones").at(child);
      anyX = anyX || o.contains("x"); anyY = anyY || o.contains("y");
      onlyX = onlyX || (o.contains("x") && !o.contains("y")); onlyY = onlyY || (o.contains("y") && !o.contains("x"));
    }
  if ((onlyX && anyY) || (onlyY && anyX)) return "자식 본 「" + child + "」의 x와 y를 따로 키한 클립·표정이 있습니다";
  return "";
}
/** (x, y) seen from a frame turned by `deg`: R(-deg) · (x, y). */
void unturn(double deg, double& x, double& y) {
  const double a = deg * PI / 180, c = std::cos(a), s = std::sin(a), x0 = x;
  x = c * x0 + s * y;
  y = -s * x0 + c * y;
}
/** Turn bone `id`'s frame by `delta` degrees (L' = L · R(δ)) and express everything that lives in
 *  it (child bones and their keys/expressions, attachments in every skin, aim axes) in the new
 *  frame, so every pose draws exactly as before (the caller checked uniform scale and pairs). */
void turnFrame(EditResult& r, const Str& id, double delta) {
  Json& d = r.data;
  const Str self = "bone:" + id;
  auto shift = [&](Json& v, double by) { if (v.is_number()) v = fine(v.get<double>() + by); };
  // The bone itself: rotation + δ (setup, keys, expression overrides).
  { Json& b = bone(d, id); Transform t = readT(b); t.rotation += delta; writeT(b, t, fine); }
  for (auto& c : arr(d, "clips"))
    if (Json* tr = findTrack(c, self, "rotation")) for (auto& k : arr(*tr, "keys")) if (k.contains("value")) shift(k["value"], delta);
  if (d.contains("expressions") && d["expressions"].is_array())
    for (auto& e : d["expressions"])
      if (e.contains("bones") && e["bones"].is_object() && e["bones"].contains(id) && e["bones"][id].contains("rotation")) shift(e["bones"][id]["rotation"], delta);
  // Aim axes are bone-local directions: the same direction is δ less in the turned frame.
  if (d.contains("ik") && d["ik"].is_array())
    for (auto& k : d["ik"])
      if (k.is_object() && k.contains("bones") && k["bones"] == Json::array({id})) {
        double axis = std::fmod(k.value("aimAxis", 0.0) - delta, 360.0);
        if (axis > 180) axis -= 360;
        else if (axis <= -180) axis += 360;
        const double rounded = fine(axis);
        if (rounded == 0) k.erase("aimAxis"); else k["aimAxis"] = rounded;
        r.affected.push_back("ik:" + optStr(k, "id"));
      }
  // Children: position R(-δ)·p, rotation - δ; key pairs rotate together (missing partner = setup).
  for (auto& cb : arr(d, "bones")) {
    if (parentOf(cb) != id) continue;
    const Str child = cb.value("id", Str()), target = "bone:" + child;
    const Transform setup = readT(cb);
    Transform t = setup;
    unturn(delta, t.x, t.y);
    t.rotation -= delta;
    writeT(cb, t, fine);
    for (auto& c : arr(d, "clips")) {
      Json* tx = findTrack(c, target, "x");
      Json* ty = findTrack(c, target, "y");
      const bool kx = tx && !arr(*tx, "keys").empty(), ky = ty && !arr(*ty, "keys").empty();
      if (kx != ky) {  // the partner channel is constant (pairProblem): key it at the same times
        Json made = kx ? *tx : *ty;
        made["property"] = kx ? "y" : "x";
        for (auto& k : made["keys"]) k["value"] = kx ? setup.y : setup.x;
        if (Json* empty = kx ? ty : tx) *empty = made; else arr(c, "tracks").push_back(made);
        tx = findTrack(c, target, "x");
        ty = findTrack(c, target, "y");
      }
      if (kx || ky)
        for (size_t i = 0; i < (*tx)["keys"].size(); i++) {
          Json& a = (*tx)["keys"][i];
          Json& b = (*ty)["keys"][i];
          double x = a.value("value", setup.x), y = b.value("value", setup.y);
          unturn(delta, x, y);
          a["value"] = fine(x);
          b["value"] = fine(y);
        }
      if (Json* tr = findTrack(c, target, "rotation")) for (auto& k : arr(*tr, "keys")) if (k.contains("value")) shift(k["value"], -delta);
    }
    if (d.contains("expressions") && d["expressions"].is_array())
      for (auto& e : d["expressions"]) {
        if (!e.contains("bones") || !e["bones"].is_object() || !e["bones"].contains(child)) continue;
        Json& o = e["bones"][child];
        if (o.contains("x") || o.contains("y")) {
          double x = o.value("x", setup.x), y = o.value("y", setup.y);
          unturn(delta, x, y);
          o["x"] = fine(x);
          o["y"] = fine(y);
        }
        if (o.contains("rotation")) shift(o["rotation"], -delta);
      }
    r.affected.push_back(target);
  }
  // Attachments of its slots, default and every skin: placement R(-δ)·offset.
  std::set<Str> slotsOn;
  for (const auto& sl : arr(d, "slots")) if (sl.value("bone", Str()) == id) slotsOn.insert(sl.value("id", Str()));
  auto place = [&](Json& list) {
    for (auto& a : list) {
      if (!a.is_object() || !slotsOn.count(a.value("slot", Str()))) continue;
      double x = a.value("x", 0.0), y = a.value("y", 0.0);
      unturn(delta, x, y);
      a["x"] = fine(x);
      a["y"] = fine(y);
      const double rot = fine(a.value("rotation", 0.0) - delta);
      if (rot == 0) a.erase("rotation"); else a["rotation"] = rot;
    }
  };
  place(arr(d, "attachments"));
  if (d.contains("skins") && d["skins"].is_array()) for (auto& sk : d["skins"]) if (sk.is_object() && sk.contains("attachments")) place(sk["attachments"]);
  for (const auto& sl : slotsOn) r.affected.push_back("slot:" + sl);
  r.affected.push_back(self);
}
/**
 * orientBones {bones?, fitLength? = true} ("본 방향 맞추기", migration of point-bone rigs): turns
 * each bone's frame so +x points along its natural direction (first child bone, else its images:
 * naturalAimAxis) and, with fitLength, sets its length to that reach, while every pose draws
 * exactly as before (turnFrame). A bone is skipped (extra.skipped {id: reason}, warning
 * ANIM_ORIENT_SKIPPED when listed) when it already has a length, has nothing to point at, has a
 * non-uniform or mirrored scale anywhere, or a child whose x/y keys cannot rotate as pairs. Aim
 * axes shrink by the turn (an arm aimed with aimAxis 90 ends at +x: no axis). The lower bone of a
 * two-bone chain only gets a length when it keeps its effector (a child, pinned as `tip`).
 * extra.oriented {id: degrees turned}.
 */
void orientBones(EditResult& r, const Json& op) {
  Json& d = r.data;
  const bool fitLength = !has(op, "fitLength") || op.at("fitLength") == true;
  const std::vector<Str> ids = boneList(d, op);
  Json oriented = Json::object(), skipped = Json::object();
  for (const Str& id : ids) {
    auto skip = [&](const Str& why) {
      skipped[id] = why;
      if (has(op, "bones")) warn(r, "ANIM_ORIENT_SKIPPED", "", "본 「" + id + "」: " + why);
    };
    if (boneLength(bone(d, id)) > 0) { skip("이미 길이가 있어 방향이 정해져 있습니다"); continue; }
    Rig rig;
    try { rig = parseRig("orient", d); } catch (const std::exception& e) { throw EditError(Str("리그를 읽을 수 없습니다: ") + e.what()); }
    double degrees = 0, reach = 0;
    if (!naturalAimAxis(rig, rig.boneIndex(id), degrees, reach)) { skip("가리킬 자식 본이나 그림이 없습니다"); continue; }
    if (!uniformScale(d, id)) { skip("크기가 가로·세로 달라(또는 뒤집혀) 방향만 돌릴 수 없습니다"); continue; }
    Str problem;
    for (const auto& c : arr(d, "bones")) if (problem.empty() && parentOf(c) == id) problem = pairProblem(d, c.value("id", Str()));
    if (!problem.empty()) { skip(problem); continue; }
    // A lower bone without a child would switch its effector from its origin to the new tip.
    bool lowerWithoutChild = false;
    for (const auto& k : listOr(d, "ik"))
      if (!k.contains("tip") && k.contains("bones") && k.at("bones").is_array() && k.at("bones").size() == 2 && k.at("bones")[1] == id) {
        bool child = false;
        for (const auto& c : arr(d, "bones")) child = child || parentOf(c) == id;
        lowerWithoutChild = lowerWithoutChild || !child;
      }
    const double delta = clean(degrees);
    const bool lengthen = fitLength && clean(reach) > 0 && !lowerWithoutChild;
    if (fitLength && clean(reach) > 0 && lowerWithoutChild) warn(r, "ANIM_ORIENT_SKIPPED", "", "본 「" + id + "」: 두 본 IK의 끝점이 바뀌지 않도록 길이는 넣지 않았습니다.");
    if (delta == 0 && !lengthen) continue;  // already pointing there: nothing to change
    if (delta != 0) turnFrame(r, id, delta);
    if (lengthen) {
      writeLook(bone(d, id), {{"length", reach}});
      pinLimbTips(r, id);
    }
    oriented[id] = delta;
    r.affected.push_back("bone:" + id);
  }
  if (oriented.empty()) throw EditError(skipped.empty() ? Str("방향을 맞출 본이 없습니다.") : "방향을 맞출 수 있는 본이 없습니다: " + skipped.begin().value().get<Str>());
  r.extra["oriented"] = oriented;
  r.extra["skipped"] = skipped;
}
/**
 * addIk {id?, name?, bones: [parent?, child], target? | createTarget: true (+ targetId?,
 * targetParent?), mix?, bendPositive?, order?, aimAxis?}. createTarget adds a bone at the chain
 * end as shown now (two-bone: the effector = the child's first child, else the child origin;
 * aim: along the aim axis), under the chain's top-level ancestor, so the new constraint starts by
 * holding the current pose; bendPositive defaults to the chain's current bend side. An aim chain
 * without `aimAxis` gets the bone's natural direction (naturalAimAxis; plugin 1.4.1), stored only
 * when it is not +x.
 */
void addIk(EditResult& r, const Json& op) {
  Json& d = r.data;
  if (!has(op, "bones") || !op.at("bones").is_array() || op.at("bones").empty() || op.at("bones").size() > 2)
    throw EditError("bones는 본 1개(조준) 또는 2개(부모 → 자식) ID 배열입니다.");
  std::vector<Str> chain;
  for (const auto& b : op.at("bones")) { chain.push_back(b.is_string() ? b.get<Str>() : Str()); bone(d, chain.back()); }
  if (chain.size() == 2 && parentOf(bone(d, chain[1])) != chain[0]) throw EditError("두 본 IK 사슬은 직접 부모 → 자식이어야 합니다: " + chain[0] + " → " + chain[1]);
  Str id = has(op, "id") ? reqStr(op, "id") : slugId(chain.back() + "-ik", "ik");
  if (!has(op, "id")) for (int n = 2; findIndex(listOr(d, "ik"), "id", id) >= 0; n++) id = slugId(chain.back() + "-ik-" + std::to_string(n), "ik");
  ensureUniqueIn(d, "ik", id, "IK");
  Rig rig;
  const Pose pose = ikSetupPose(d, rig);
  const int end = rig.boneIndex(chain.back());
  int tip = -1;
  if (has(op, "tip")) {
    if (chain.size() != 2) throw EditError("끝점(tip)은 두 본 IK에만 지정합니다.");
    bone(d, reqStr(op, "tip"));
    tip = rig.boneIndex(reqStr(op, "tip"));
    if (rig.bones[size_t(tip)].parentIndex != end) throw EditError("끝점(tip)은 아래 본 「" + chain.back() + "」의 직접 자식이어야 합니다: " + reqStr(op, "tip"));
  }
  // A lower bone with a length (plugin 1.5.0) reaches with its own tip: no child is picked then.
  const double endLength = rig.bones[size_t(end)].length;
  const bool lengthTip = chain.size() == 2 && tip < 0 && endLength > 0;
  for (size_t i = 0; i < rig.bones.size() && tip < 0 && !lengthTip; i++) if (rig.bones[i].parentIndex == end) tip = int(i);
  Json k{{"id", id}, {"name", displayName(op, id)}, {"bones", chain}};
  // A known end becomes the explicit effector: adding a sibling bone later never changes the limb.
  if (chain.size() == 2 && tip >= 0) k["tip"] = rig.bones[size_t(tip)].id;
  double axis = 0, reach = 0;
  bool hasAxis = false;
  if (has(op, "aimAxis")) {
    if (chain.size() != 1) throw EditError("조준 축(aimAxis)은 조준 IK(본 1개)에만 지정합니다.");
    axis = aimAxisOf(op);
    double natural = 0;
    if (endLength > 0) reach = endLength;
    else naturalAimAxis(rig, end, natural, reach);
    hasAxis = true;
  } else if (chain.size() == 1 && endLength > 0) {
    hasAxis = true;  // a bone with a length points along +x (Spine): that is its natural aim
    reach = endLength;
  } else if (chain.size() == 1) hasAxis = naturalAimAxis(rig, end, axis, reach);
  // The two-bone effector as shown now (rig space); `effector` false = none (the origin aims).
  double ex = 0, ey = 0;
  const bool effector = chain.size() == 2 && (tip >= 0 || lengthTip);
  if (tip >= 0) { ex = pose.world[size_t(tip)].e; ey = pose.world[size_t(tip)].f; }
  else if (lengthTip) pose.world[size_t(end)].apply(endLength, 0, ex, ey);
  if (chain.size() == 1) putAimAxis(k, axis);
  Str target = optStr(op, "target");
  if (target.empty()) {
    if (!(has(op, "createTarget") && op.at("createTarget") == true)) throw EditError("target(대상 본) 또는 createTarget: true가 필요합니다.");
    // Where the chain end is shown now: the solve then starts from the current pose.
    const Affine& w = pose.world[size_t(end)];
    double wx = w.e, wy = w.f;
    if (effector) { wx = ex; wy = ey; }
    else if (chain.size() == 1 && hasAxis) {
      // On the aim axis at the bone's own length (child or image end): solving changes nothing.
      const double cx = std::cos(axis * PI / 180), cy = std::sin(axis * PI / 180);
      const double unit = std::hypot(w.a * cx + w.c * cy, w.b * cx + w.d * cy);
      const double local = reach > 1 ? reach : unit > 1e-9 ? 60 / unit : 0;
      w.apply(cx * local, cy * local, wx, wy);
    } else if (chain.size() == 1) {
      const double len = std::hypot(w.a, w.b);
      const double far = tip >= 0 ? std::hypot(pose.world[size_t(tip)].e - w.e, pose.world[size_t(tip)].f - w.f) : 0;
      const double dist = far > 1 ? far : 60;
      if (len > 1e-9) { wx += w.a / len * dist; wy += w.b / len * dist; }
    }
    Str top = chain[0];
    for (Str p = parentOf(bone(d, top)); !p.empty(); p = parentOf(bone(d, p))) top = p;
    const Str parent = has(op, "targetParent") ? reqStr(op, "targetParent") : top == chain[0] ? Str() : top;
    target = has(op, "targetId") ? reqStr(op, "targetId") : slugId(chain.back() + "-target", "ik-target");
    if (!has(op, "targetId")) for (int n = 2; findIndex(arr(d, "bones"), "id", target) >= 0; n++) target = slugId(chain.back() + "-target-" + std::to_string(n), "ik-target");
    ensureUniqueBone(d, target);
    if (!parent.empty()) bone(d, parent);
    const int pi = parent.empty() ? -1 : rig.boneIndex(parent);
    const auto inv = (pi < 0 ? Affine{} : pose.world[size_t(pi)]).inverse();
    if (!inv) throw EditError("대상 본의 부모 크기가 0이라 계산할 수 없습니다.");
    Transform t;
    inv->apply(wx, wy, t.x, t.y);
    Json b{{"id", target}, {"parent", parent.empty() ? Json(nullptr) : Json(parent)}};
    writeT(b, t);
    arr(d, "bones").push_back(b);
    r.affected.push_back("bone:" + target);
  } else bone(d, target);
  k["target"] = target;
  if (has(op, "mix")) k["mix"] = clean(optUnit(op, "mix"));
  bool bend = true;
  if (has(op, "bendPositive")) {
    if (!op.at("bendPositive").is_boolean()) throw EditError("bendPositive는 true 또는 false입니다.");
    bend = op.at("bendPositive").get<bool>();
  } else if (effector) {
    // Keep the side the limb bends to now (the angle between its two links, on screen).
    const int up = rig.boneIndex(chain[0]);
    const Affine& u = pose.world[size_t(up)];
    const Affine& l = pose.world[size_t(end)];
    const double cross = (l.e - u.e) * (ey - l.f) - (l.f - u.f) * (ex - l.e);
    bend = cross >= 0;
  }
  k["bendPositive"] = bend;
  if (has(op, "order")) k["order"] = int64_t(num(op, "order", 0));
  arr(d, "ik").push_back(k);
  if (const Str problem = ikProblem(d); !problem.empty()) throw EditError(problem);
  r.affected.push_back("ik:" + id);
  r.extra["ik"] = id;
  r.extra["target"] = target;
}
/**
 * setIk {ik, name?, mix?, bendPositive?, order? (null = default), target?, bones?, aimAxis?
 * (null = +x), fitAimAxis? (+ clip?, timeMs?)}: setup values of one constraint. fitAimAxis (aim
 * chains) sets the axis to where the target is seen from the bone in the pose without IK (setup,
 * or `clip` at `timeMs`), so turning the constraint on stops moving the bone.
 */
void setIk(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str id = reqStr(op, "ik");
  Json& k = ikAt(d, id);
  if (has(op, "name")) k["name"] = displayName(op, id);
  if (has(op, "mix")) k["mix"] = clean(optUnit(op, "mix"));
  if (has(op, "bendPositive")) {
    if (!op.at("bendPositive").is_boolean()) throw EditError("bendPositive는 true 또는 false입니다.");
    k["bendPositive"] = op.at("bendPositive");
  }
  if (op.contains("order")) {
    if (op.at("order").is_null()) k.erase("order");
    else k["order"] = int64_t(num(op, "order", 0));
  }
  if (has(op, "target")) { bone(d, reqStr(op, "target")); k["target"] = reqStr(op, "target"); }
  if (op.contains("tip")) {  // null = back to the implicit first-child rule
    if (op.at("tip").is_null()) k.erase("tip");
    else { bone(d, reqStr(op, "tip")); k["tip"] = reqStr(op, "tip"); }
  }
  if (has(op, "bones")) {
    if (!op.at("bones").is_array()) throw EditError("bones는 본 ID 배열입니다.");
    k["bones"] = op.at("bones");
  }
  if (op.contains("aimAxis")) {
    if (op.at("aimAxis").is_null()) k.erase("aimAxis");
    else putAimAxis(k, aimAxisOf(op));
  }
  if (has(op, "fitAimAxis") && op.at("fitAimAxis") == true) {
    if (!k.contains("bones") || !k.at("bones").is_array() || k.at("bones").size() != 1) throw EditError("현재 방향으로 맞추기는 조준 IK(본 1개)에만 씁니다.");
    Rig rig;
    try { rig = parseRig("ik", d); } catch (const std::exception& e) { throw EditError(Str("리그를 읽을 수 없습니다: ") + e.what()); }
    const int b = rig.boneIndex(k.at("bones")[0].get<Str>()), target = rig.boneIndex(optStr(k, "target"));
    const Str clip = optStr(op, "clip");
    const int ci = clip.empty() ? -1 : rig.clipIndex(clip);
    if (!clip.empty() && ci < 0) throw EditError("클립이 없습니다: " + clip);
    if (b < 0 || target < 0) throw EditError("IK의 본 또는 대상 본이 없습니다.");
    Look look;
    look.ik = false;  // the pose the bone has on its own: the axis then needs no rotation to aim
    const Pose pose = samplePose(rig, ci, ci < 0 ? 0 : num(op, "timeMs", 0), look);
    const auto inv = pose.world[size_t(b)].inverse();
    if (!inv) throw EditError("본의 크기가 0이라 방향을 계산할 수 없습니다.");
    double lx, ly;
    inv->apply(pose.world[size_t(target)].e, pose.world[size_t(target)].f, lx, ly);
    if (std::hypot(lx, ly) < 1e-6) throw EditError("대상 본이 본의 원점에 있어 방향을 정할 수 없습니다. 대상을 조금 옮긴 뒤 다시 누르세요.");
    putAimAxis(k, std::atan2(ly, lx) * 180 / PI);
  }
  if (const Str problem = ikProblem(d); !problem.empty()) throw EditError(problem);
  r.affected.push_back("ik:" + id);
}
/** renameIk {ik, id?, name?}: a new id also renames its clip tracks (ik:<old> -> ik:<new>). */
void renameIk(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str from = reqStr(op, "ik");
  Json& k = ikAt(d, from);
  const Str to = has(op, "id") ? reqStr(op, "id") : from;
  if (has(op, "name")) k["name"] = displayName(op, to);
  r.affected.push_back("ik:" + to);
  if (to == from) return;
  ensureUniqueIn(d, "ik", to, "IK");
  k["id"] = to;
  size_t tracks = 0;
  for (auto& c : arr(d, "clips"))
    for (auto& t : arr(c, "tracks")) if (t.value("target", Str()) == "ik:" + from) { t["target"] = "ik:" + to; tracks++; }
  r.extra["tracks"] = tracks;
}
/** deleteIk {ik, deleteTarget?}: the constraint, its keys and (deleteTarget) its target bone when nothing else uses it. */
void deleteIk(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str id = reqStr(op, "ik");
  const Str target = optStr(ikAt(d, id), "target");
  r.extra["keys"] = removeIk(d, id);
  r.affected.push_back("ik:" + id);
  if (has(op, "deleteTarget") && op.at("deleteTarget") == true && ikUsing(d, target).empty()) {
    bool partsOn = false;
    for (const auto& s : listOr(d, "slots")) partsOn = partsOn || optStr(s, "bone") == target;
    if (!partsOn) {
      deleteBone(r, {{"bone", target}});
      r.extra["deletedTarget"] = target;
    }
  }
}
/**
 * bakeIk {clip, timeMs, ik?, disable?} ("IK 결과를 키로 굽기"): keys the solved local rotation of
 * every chain bone of `ik` (all constraints without it) at timeMs, so the pose stays when the
 * constraint is later turned off; disable: true also keys mix 0 there.
 */
void bakeIk(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str clipId = reqStr(op, "clip");
  const Json& c = clip(d, clipId);
  const int64_t t = ms(op, "timeMs");
  if (t < 0 || t > c.value("durationMs", int64_t(0))) throw EditError("키 시간이 클립 범위 밖입니다.");
  Rig rig;
  try { rig = parseRig("bake", d); } catch (const std::exception& e) { throw EditError(Str("리그를 읽을 수 없습니다: ") + e.what()); }
  for (auto& x : rig.clips) x.loop = false;  // the clip end is a real time here, not the next loop's start
  const Pose pose = samplePose(rig, rig.clipIndex(clipId), double(t));
  const Str only = optStr(op, "ik");
  if (!only.empty()) ikAt(d, only);
  if (rig.ik.empty()) throw EditError("이 리그에는 IK 제약이 없습니다.");
  Json keyed = Json::array();
  for (const auto& k : rig.ik) {
    if (!only.empty() && k.id != only) continue;
    for (int b : k.bones) {
      const Str boneId = rig.bones[size_t(b)].id;
      setKey(r, {{"clip", clipId}, {"target", "bone:" + boneId}, {"property", "rotation"}, {"timeMs", t}, {"value", clean(pose.local(size_t(b)).rotation)}});
      keyed.push_back(refJson("bone:" + boneId, "rotation", t));
    }
    if (has(op, "disable") && op.at("disable") == true) {
      setKey(r, {{"clip", clipId}, {"target", "ik:" + k.id}, {"property", "mix"}, {"timeMs", t}, {"value", 0}});
      keyed.push_back(refJson("ik:" + k.id, "mix", t));
    }
  }
  r.extra["keys"] = keyed;
}

// ---- Mesh attachments (plugin 1.4.0) ----
/**
 * The attachment entry an op names: {slot, name? (default: the slot's setup attachment),
 * skin?}. With `skin` the skin's own entry (an outfit mesh is edited there), else the default list.
 */
Json& attachmentEntry(Json& d, const Json& op, Str& slotId, Str& name) {
  slotId = reqStr(op, "slot");
  const Json& s = slot(d, slotId);
  name = has(op, "name") ? reqStr(op, "name") : optStr(s, "attachment");
  if (name.empty()) throw EditError("이 조각에는 기본 첨부가 없습니다. name(첨부 이름)을 지정하세요.");
  const Str skin = has(op, "skin") ? reqStr(op, "skin") : Str();
  Json& list = skin.empty() ? arr(d, "attachments") : arr(skinAt(d, skin), "attachments");
  for (auto& a : list) if (optStr(a, "slot") == slotId && optStr(a, "name") == name) return a;
  throw EditError("없는 첨부입니다: " + slotId + "/" + name + (skin.empty() ? Str() : " (스킨 「" + skin + "」)"));
}
Json& meshEntry(Json& d, const Json& op, Str& slotId, Str& name) {
  Json& a = attachmentEntry(d, op, slotId, name);
  if (!isMesh(a)) throw EditError("메시가 아닌 첨부입니다: " + slotId + "/" + name + " (먼저 「메시로 변환」하세요)");
  return a;
}
std::vector<double> numbersOf(const Json& a, const char* k) {
  std::vector<double> out;
  for (const auto& v : listOr(a, k)) out.push_back(v.is_number() ? v.get<double>() : 0.0);
  return out;
}
Json numbersJson(const std::vector<double>& v) { Json out = Json::array(); for (double x : v) out.push_back(clean(x)); return out; }
/** Vertex indices of op `vertices` (all vertices when absent): unique, in range. */
std::vector<size_t> vertexList(const Json& op, size_t n, bool required) {
  std::vector<size_t> out;
  if (!has(op, "vertices")) {
    if (required) throw EditError("vertices(정점 번호 배열)가 필요합니다.");
    for (size_t i = 0; i < n; i++) out.push_back(i);
    return out;
  }
  if (!op.at("vertices").is_array() || op.at("vertices").empty()) throw EditError("vertices는 정점 번호 배열입니다.");
  std::set<size_t> seen;
  for (const auto& v : op.at("vertices")) {
    if (!v.is_number_integer() || v.get<int64_t>() < 0 || v.get<int64_t>() >= int64_t(n)) throw EditError("정점 번호가 범위(0.." + std::to_string(n - 1) + ") 밖입니다: " + v.dump());
    if (seen.insert(size_t(v.get<int64_t>())).second) out.push_back(size_t(v.get<int64_t>()));
  }
  return out;
}
using Weights = std::vector<std::vector<std::pair<Str, double>>>;
/** Current weights of a mesh entry; an unweighted mesh reads as 100 % on its slot bone. */
Weights weightsOf(const Json& d, const Json& a, size_t n) {
  Weights out(n);
  const Json& list = listOr(a, "weights");
  Str slotBone;
  for (const auto& s : listOr(d, "slots")) if (optStr(s, "id") == optStr(a, "slot")) slotBone = optStr(s, "bone");
  for (size_t v = 0; v < n; v++) {
    if (v < list.size() && list[v].is_array())
      for (const auto& w : list[v]) out[v].push_back({optStr(w, "bone"), w.value("weight", 0.0)});
    if (out[v].empty()) out[v].push_back({slotBone, 1.0});
  }
  return out;
}
/** Normalize one vertex: drop ~0 shares, keep the 4 largest, sum 1, largest first. */
void normalizeWeights(std::vector<std::pair<Str, double>>& w) {
  w.erase(std::remove_if(w.begin(), w.end(), [](const auto& x) { return !(x.second > 1e-4); }), w.end());
  std::stable_sort(w.begin(), w.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
  if (w.size() > MAX_INFLUENCES) w.resize(MAX_INFLUENCES);
  double sum = 0;
  for (const auto& x : w) sum += x.second;
  for (auto& x : w) x.second /= sum;
}
void writeWeights(Json& a, const Weights& weights) {
  Json out = Json::array();
  for (const auto& list : weights) {
    Json v = Json::array();
    double rest = 1;
    for (size_t i = 0; i < list.size(); i++) {
      // Rounded shares, the last one takes the remainder: every vertex sums to exactly 1.
      const double w = i + 1 == list.size() ? clean(rest) : clean(list[i].second);
      rest -= w;
      v.push_back({{"bone", list[i].first}, {"weight", w}});
    }
    out.push_back(v);
  }
  a["weights"] = out;
}
/** Edges used by exactly one triangle (the outline, holes included), as sorted index pairs. */
std::vector<int> boundaryEdges(const std::vector<int>& tris) {
  std::map<std::pair<int, int>, int> uses;
  for (size_t t = 0; t + 2 < tris.size(); t += 3)
    for (int k = 0; k < 3; k++) {
      const int a = tris[t + size_t(k)], b = tris[t + size_t((k + 1) % 3)];
      uses[{std::min(a, b), std::max(a, b)}]++;
    }
  std::vector<int> out;
  for (const auto& [e, n] : uses) if (n == 1) { out.push_back(e.first); out.push_back(e.second); }
  return out;
}

/**
 * convertToMesh {slot, name?, skin?, rows? = 4, cols? = 4, cells?}: region (or mesh) -> grid
 * mesh over the image, rows × cols cells. `cells` ("1"/"0" per cell, row-major) keeps only the
 * marked cells (alpha-trimmed grids: the editor marks cells that hold opaque pixels); unused
 * vertices are dropped. Vertices sit exactly on the region's pixels with matching uvs, so the
 * part looks unchanged; the full grid lists its outline first (`hull`). Weights are cleared.
 */
void convertToMesh(EditResult& r, const Json& op) {
  Json& d = r.data;
  Str slotId, name;
  Json& a = attachmentEntry(d, op, slotId, name);
  if (isClipping(a)) throw EditError("클리핑 첨부는 메시로 바꿀 수 없습니다(그림이 없음). 모양은 「모양 다시 그리기」로 바꿉니다.");
  const int64_t rows = has(op, "rows") ? int64_t(num(op, "rows", 4)) : 4, cols = has(op, "cols") ? int64_t(num(op, "cols", 4)) : 4;
  if (rows < 1 || cols < 1 || rows > 64 || cols > 64) throw EditError("격자 칸 수(rows, cols)는 1..64입니다.");
  if (size_t((rows + 1) * (cols + 1)) > MAX_MESH_VERTICES) throw EditError("정점이 너무 많습니다(최대 " + std::to_string(MAX_MESH_VERTICES) + "개). 칸 수를 줄이세요.");
  const Str cells = optStr(op, "cells");
  if (!cells.empty() && (int64_t(cells.size()) != rows * cols || cells.find_first_not_of("01") != Str::npos))
    throw EditError("cells는 칸마다 1(남김) 또는 0(뺌)인 " + std::to_string(rows * cols) + "글자 문자열입니다.");
  if (!cells.empty() && cells.find('1') == Str::npos) throw EditError("남길 칸이 없습니다(이미지가 모두 투명합니다).");
  const bool full = cells.find('0') == Str::npos;
  const double w = num(a, "width", 0), h = num(a, "height", 0);
  auto kept = [&](int64_t rr, int64_t cc) { return full || cells[size_t(rr * cols + cc)] == '1'; };
  // Grid vertex (row, col) is used when any kept cell touches it.
  std::vector<int> index(size_t((rows + 1) * (cols + 1)), -1);
  auto at = [&](int64_t rr, int64_t cc) -> int& { return index[size_t(rr * (cols + 1) + cc)]; };
  std::vector<std::pair<int64_t, int64_t>> order;
  auto use = [&](int64_t rr, int64_t cc) { if (at(rr, cc) < 0) { at(rr, cc) = int(order.size()); order.push_back({rr, cc}); } };
  if (full) {
    // Outline first, clockwise from the top-left corner (Spine's hull convention), then the inside.
    for (int64_t cc = 0; cc <= cols; cc++) use(0, cc);
    for (int64_t rr = 1; rr <= rows; rr++) use(rr, cols);
    for (int64_t cc = cols - 1; cc >= 0; cc--) use(rows, cc);
    for (int64_t rr = rows - 1; rr >= 1; rr--) use(rr, 0);
  }
  const size_t hull = order.size();
  for (int64_t rr = 0; rr <= rows; rr++)
    for (int64_t cc = 0; cc <= cols; cc++) {
      bool touched = false;
      for (int64_t dr = -1; dr <= 0; dr++)
        for (int64_t dc = -1; dc <= 0; dc++)
          touched = touched || (rr + dr >= 0 && rr + dr < rows && cc + dc >= 0 && cc + dc < cols && kept(rr + dr, cc + dc));
      if (touched) use(rr, cc);
    }
  std::vector<double> vertices, uvs;
  for (const auto& [rr, cc] : order) {
    vertices.push_back(double(cc) / double(cols) * w); vertices.push_back(double(rr) / double(rows) * h);
    uvs.push_back(double(cc) / double(cols)); uvs.push_back(double(rr) / double(rows));
  }
  std::vector<int> tris;
  for (int64_t rr = 0; rr < rows; rr++)
    for (int64_t cc = 0; cc < cols; cc++) {
      if (!kept(rr, cc)) continue;
      const int v00 = at(rr, cc), v01 = at(rr, cc + 1), v10 = at(rr + 1, cc), v11 = at(rr + 1, cc + 1);
      // Alternating diagonals bend symmetrically (no direction is favoured by the triangulation).
      if ((rr + cc) % 2 == 0) tris.insert(tris.end(), {v00, v01, v11, v00, v11, v10});
      else tris.insert(tris.end(), {v00, v01, v10, v01, v11, v10});
    }
  if (tris.size() / 3 > MAX_MESH_TRIANGLES) throw EditError("삼각형이 너무 많습니다(최대 " + std::to_string(MAX_MESH_TRIANGLES) + "개).");
  a["type"] = "mesh";
  a["vertices"] = numbersJson(vertices);
  a["uvs"] = numbersJson(uvs);
  a["triangles"] = tris;
  if (full) a["hull"] = hull; else a.erase("hull");
  a["edges"] = boundaryEdges(tris);
  a.erase("weights");
  r.extra["vertices"] = vertices.size() / 2;
  r.extra["triangles"] = tris.size() / 3;
  r.extra["keys"] = purgeDeform(r);
  r.affected.push_back("slot:" + slotId);
}
/** convertToRegion {slot, name?, skin?}: back to a plain image rectangle (its deform keys go too). */
void convertToRegion(EditResult& r, const Json& op) {
  Json& d = r.data;
  Str slotId, name;
  Json& a = meshEntry(d, op, slotId, name);
  for (const char* k : {"type", "vertices", "uvs", "triangles", "hull", "edges", "weights"}) a.erase(k);
  r.extra["keys"] = purgeDeform(r);
  r.affected.push_back("slot:" + slotId);
}
/**
 * setMeshVertices {slot, name?, skin?, vertices?, positions? | dx?, dy?, keepUvs?}: move setup
 * vertices (image pixels). `positions` = x, y per listed vertex (all vertices without
 * `vertices`); dx/dy move them all by the same amount. By default uvs follow (uv = position /
 * image size, clamped), so the image stays put under the outline; keepUvs stretches it instead.
 */
void setMeshVertices(EditResult& r, const Json& op) {
  Json& d = r.data;
  Str slotId, name;
  Json& a = meshEntry(d, op, slotId, name);
  std::vector<double> vertices = numbersOf(a, "vertices"), uvs = numbersOf(a, "uvs");
  const size_t n = vertices.size() / 2;
  const std::vector<size_t> list = vertexList(op, n, false);
  if (has(op, "positions")) {
    if (!op.at("positions").is_array() || op.at("positions").size() != list.size() * 2) throw EditError("positions는 정점마다 x, y(" + std::to_string(list.size() * 2) + "개)입니다.");
    for (size_t i = 0; i < list.size(); i++)
      for (size_t k = 0; k < 2; k++) {
        const Json& v = op.at("positions")[2 * i + k];
        if (!v.is_number() || !std::isfinite(v.get<double>()) || std::fabs(v.get<double>()) > 1e5) throw EditError("positions 값은 유한한 숫자여야 합니다.");
        vertices[2 * list[i] + k] = v.get<double>();
      }
  } else {
    if (!has(op, "dx") && !has(op, "dy")) throw EditError("positions 또는 dx, dy가 필요합니다.");
    const double dx = num(op, "dx", 0), dy = num(op, "dy", 0);
    for (size_t v : list) { vertices[2 * v] += dx; vertices[2 * v + 1] += dy; }
  }
  if (!(has(op, "keepUvs") && op.at("keepUvs") == true)) {
    const double w = num(a, "width", 1), h = num(a, "height", 1);
    for (size_t v : list) { uvs[2 * v] = std::clamp(vertices[2 * v] / w, 0.0, 1.0); uvs[2 * v + 1] = std::clamp(vertices[2 * v + 1] / h, 0.0, 1.0); }
  }
  a["vertices"] = numbersJson(vertices);
  a["uvs"] = numbersJson(uvs);
  r.extra["vertices"] = list.size();
  r.affected.push_back("slot:" + slotId);
}
double segmentDistance(double px, double py, double ax, double ay, double bx, double by) {
  const double vx = bx - ax, vy = by - ay, len2 = vx * vx + vy * vy;
  const double u = len2 > 1e-12 ? std::clamp(((px - ax) * vx + (py - ay) * vy) / len2, 0.0, 1.0) : 0;
  return std::hypot(px - (ax + u * vx), py - (ay + u * vy));
}
/**
 * 자동 웨이트: every listed vertex (all by default) gets the nearest 1..`influences` (default 4)
 * of `bones` by distance to each bone's segment (origin -> its child bone's origin, preferring a
 * child that is itself a candidate; a bone without children is a point), measured in the setup
 * pose. Share ∝ 1 / (distance + 1 px)^falloff (default 2: smooth, a vertex on a bone is ~100 %
 * that bone), normalized. Without `bones`: the bones already weighting the mesh, else the slot's
 * bone with its parent and children.
 */
void autoWeights(EditResult& r, const Json& op, bool requireBones) {
  Json& d = r.data;
  Str slotId, name;
  Json& a = meshEntry(d, op, slotId, name);
  const std::vector<double> vertices = numbersOf(a, "vertices");
  const size_t n = vertices.size() / 2;
  const Str slotBone = optStr(slot(d, slotId), "bone");
  std::vector<Str> bones;
  auto add = [&](const Str& id) { if (!id.empty() && std::find(bones.begin(), bones.end(), id) == bones.end()) bones.push_back(id); };
  if (has(op, "bones")) {
    if (!op.at("bones").is_array()) throw EditError("bones는 본 ID 배열입니다.");
    for (const auto& b : op.at("bones")) { const Str id = b.is_string() ? b.get<Str>() : Str(); bone(d, id); add(id); }
  } else if (requireBones) throw EditError("메시를 묶을 본(bones)을 하나 이상 고르세요.");
  else if (a.contains("weights")) { for (const auto& list : weightsOf(d, a, n)) for (const auto& [b, w] : list) add(b); }
  else {
    add(slotBone);
    add(parentOf(bone(d, slotBone)));
    for (const auto& b : listOr(d, "bones")) if (parentOf(b) == slotBone) add(optStr(b, "id"));
  }
  if (bones.empty()) throw EditError("웨이트를 줄 본이 없습니다.");
  const int64_t influences = has(op, "influences") ? int64_t(num(op, "influences", 4)) : 4;
  if (influences < 1 || influences > int64_t(MAX_INFLUENCES)) throw EditError("정점당 본 수(influences)는 1..4입니다.");
  const double falloff = num(op, "falloff", 2);
  if (falloff < 0.5 || falloff > 8) throw EditError("falloff(거리 감쇠)는 0.5..8입니다.");
  auto world = setupWorld(d);
  // Bone segments in the setup pose (the bind pose).
  struct Segment { double ax, ay, bx, by; };
  std::vector<Segment> segments;
  for (const auto& id : bones) {
    const Affine& w = world[id];
    Segment s{w.e, w.f, w.e, w.f};
    const Json* child = nullptr;
    for (const auto& b : listOr(d, "bones"))
      if (parentOf(b) == id && (!child || (std::find(bones.begin(), bones.end(), optStr(b, "id")) != bones.end() &&
                                           std::find(bones.begin(), bones.end(), optStr(*child, "id")) == bones.end()))) child = &b;
    if (child) { s.bx = world[optStr(*child, "id")].e; s.by = world[optStr(*child, "id")].f; }
    segments.push_back(s);
  }
  const Affine placed = world[slotBone] * Affine::compose(num(a, "x", 0), num(a, "y", 0), num(a, "rotation", 0), num(a, "scaleX", 1), num(a, "scaleY", 1)) *
                        Affine::compose(-num(a, "pivotX", 0.5) * num(a, "width", 0), -num(a, "pivotY", 0.5) * num(a, "height", 0), 0, 1, 1);
  Weights weights = weightsOf(d, a, n);
  const std::vector<size_t> list = vertexList(op, n, false);
  for (size_t v : list) {
    double px, py;
    placed.apply(vertices[2 * v], vertices[2 * v + 1], px, py);
    std::vector<std::pair<Str, double>> w;
    for (size_t b = 0; b < bones.size(); b++)
      w.push_back({bones[b], 1 / std::pow(segmentDistance(px, py, segments[b].ax, segments[b].ay, segments[b].bx, segments[b].by) + 1, falloff)});
    std::stable_sort(w.begin(), w.end(), [](const auto& x, const auto& y) { return x.second > y.second; });
    if (int64_t(w.size()) > influences) w.resize(size_t(influences));
    double sum = 0;
    for (const auto& x : w) sum += x.second;
    for (auto& x : w) x.second /= sum;
    normalizeWeights(w);
    weights[v] = w;
  }
  writeWeights(a, weights);
  r.extra["vertices"] = list.size();
  r.extra["bones"] = bones;
  r.affected.push_back("slot:" + slotId);
}
/**
 * setWeights {slot, name?, skin?, vertices, bone, weight, strength?} (weight brush): each listed
 * vertex moves its share of `bone` towards `weight` by `strength` (0..1, default 1) and the other
 * bones are rescaled to fill the rest (a vertex with no other bone gives the rest to the slot
 * bone). Or {vertices, weights: [[{bone, weight}]...] per listed vertex} sets them outright.
 * Shares are normalized, at most 4 per vertex.
 */
void setWeights(EditResult& r, const Json& op) {
  Json& d = r.data;
  Str slotId, name;
  Json& a = meshEntry(d, op, slotId, name);
  const size_t n = a.contains("vertices") && a.at("vertices").is_array() ? a.at("vertices").size() / 2 : 0;
  const Str slotBone = optStr(slot(d, slotId), "bone");
  Weights weights = weightsOf(d, a, n);
  const std::vector<size_t> list = vertexList(op, n, true);
  if (has(op, "weights")) {
    if (!op.at("weights").is_array() || op.at("weights").size() != list.size()) throw EditError("weights는 고른 정점마다 [{bone, weight}] 목록 하나씩입니다.");
    for (size_t i = 0; i < list.size(); i++) {
      std::vector<std::pair<Str, double>> w;
      for (const auto& x : op.at("weights")[i]) {
        const Str b = reqStr(x, "bone");
        bone(d, b);
        const double value = num(x, "weight", 0);
        if (value < 0 || value > 1) throw EditError("웨이트는 0..1입니다.");
        for (const auto& y : w) if (y.first == b) throw EditError("한 정점에 같은 본이 두 번 있습니다: " + b);
        w.push_back({b, value});
      }
      double sum = 0;
      for (const auto& x : w) sum += x.second;
      if (!(sum > 1e-9)) throw EditError("정점의 웨이트 합이 0입니다.");
      if (w.size() > MAX_INFLUENCES) throw EditError("정점 하나에 영향을 주는 본은 최대 4개입니다.");
      normalizeWeights(w);
      weights[list[i]] = w;
    }
  } else {
    const Str b = reqStr(op, "bone");
    bone(d, b);
    const double target = num(op, "weight", 1), strength = num(op, "strength", 1);
    if (target < 0 || target > 1 || strength < 0 || strength > 1) throw EditError("weight와 strength는 0..1입니다.");
    for (size_t v : list) {
      auto& w = weights[v];
      double cur = 0, others = 0;
      for (const auto& x : w) (x.first == b ? cur : others) += x.second;
      const double next = cur + strength * (target - cur);
      if (others <= 1e-9 && next < 1) {
        if (b == slotBone) continue;  // the only bone of this vertex: nothing to hand the rest to
        w.push_back({slotBone, 1 - next});
        others = 1 - next;
      } else for (auto& x : w) if (x.first != b) x.second *= (1 - next) / others;
      bool found = false;
      for (auto& x : w) if (x.first == b) { x.second = next; found = true; }
      if (!found) w.push_back({b, next});
      // Merge a slot-bone share added above with an existing one.
      std::vector<std::pair<Str, double>> merged;
      for (const auto& x : w) {
        auto it = std::find_if(merged.begin(), merged.end(), [&](const auto& y) { return y.first == x.first; });
        if (it == merged.end()) merged.push_back(x); else it->second += x.second;
      }
      normalizeWeights(merged);
      w = merged;
    }
  }
  writeWeights(a, weights);
  r.extra["vertices"] = list.size();
  r.affected.push_back("slot:" + slotId);
}
/** unbindMesh {slot, name?, skin?}: weights removed; the mesh follows its slot bone like a region again. */
void unbindMesh(EditResult& r, const Json& op) {
  Str slotId, name;
  Json& a = meshEntry(r.data, op, slotId, name);
  a.erase("weights");
  r.affected.push_back("slot:" + slotId);
}

// ---- Freeform meshes (plugin 1.6.0): outlines, vertex / edge edits, re-triangulation ----
/** Where a vertex of the edited mesh comes from: an old vertex (`from`), or a point inside the
 *  old setup mesh (its triangle and barycentric shares), so uvs, weights and deform offsets of new
 *  vertices blend their neighbours' and the picture does not move. */
struct Origin { int from = -1; int v[3] = {-1, -1, -1}; double w[3] = {0, 0, 0}; };
Origin locateIn(const std::vector<double>& xy, const std::vector<int>& tris, double px, double py) {
  Origin best;
  double bestD = std::numeric_limits<double>::infinity();
  auto X = [&](int i) { return xy[2 * size_t(i)]; };
  auto Y = [&](int i) { return xy[2 * size_t(i) + 1]; };
  for (size_t t = 0; t + 2 < tris.size(); t += 3) {
    const int a = tris[t], b = tris[t + 1], c = tris[t + 2];
    const double det = (X(b) - X(a)) * (Y(c) - Y(a)) - (Y(b) - Y(a)) * (X(c) - X(a));
    if (std::fabs(det) < 1e-12) continue;
    double l[3] = {((X(b) - px) * (Y(c) - py) - (Y(b) - py) * (X(c) - px)) / det, ((X(c) - px) * (Y(a) - py) - (Y(c) - py) * (X(a) - px)) / det, 0};
    l[2] = 1 - l[0] - l[1];
    double dist = 0;
    if (std::min({l[0], l[1], l[2]}) < -1e-9) {
      // Outside: the clamped shares give a point on the triangle; the nearest such triangle wins.
      double sum = 0;
      for (double& x : l) { x = std::max(0.0, x); sum += x; }
      for (double& x : l) x /= sum;
      const double qx = l[0] * X(a) + l[1] * X(b) + l[2] * X(c), qy = l[0] * Y(a) + l[1] * Y(b) + l[2] * Y(c);
      dist = std::sqrt((qx - px) * (qx - px) + (qy - py) * (qy - py));
    }
    if (dist < bestD) {
      bestD = dist;
      best.v[0] = a; best.v[1] = b; best.v[2] = c;
      for (int k = 0; k < 3; k++) best.w[k] = std::clamp(l[k], 0.0, 1.0);
      if (dist == 0) break;
    }
  }
  return best;
}
/** The editable parts of a mesh entry; `pinned` = internal edges listed in `edges` (the outline
 *  edges there are derived again from the triangles on write). */
struct MeshShape {
  std::vector<double> xy, uvs;
  std::vector<int> tris, pinned;
  size_t hull = 0;
  size_t count() const { return xy.size() / 2; }
};
std::vector<int> intsOf(const Json& a, const char* k) {
  std::vector<int> out;
  for (const auto& v : listOr(a, k)) out.push_back(v.is_number_integer() ? v.get<int>() : -1);
  return out;
}
MeshShape readShape(const Json& a) {
  MeshShape m;
  m.xy = numbersOf(a, "vertices");
  m.uvs = numbersOf(a, "uvs");
  m.tris = intsOf(a, "triangles");
  const int n = int(m.count());
  if (a.contains("hull") && a.at("hull").is_number_integer() && a.at("hull").get<int64_t>() >= 3 && a.at("hull").get<int64_t>() <= n) m.hull = size_t(a.at("hull").get<int64_t>());
  std::set<std::pair<int, int>> outline, seen;
  const std::vector<int> b = boundaryEdges(m.tris);
  for (size_t i = 0; i + 1 < b.size(); i += 2) outline.insert({b[i], b[i + 1]});
  const std::vector<int> e = intsOf(a, "edges");
  for (size_t i = 0; i + 1 < e.size(); i += 2) {
    const std::pair<int, int> k{std::min(e[i], e[i + 1]), std::max(e[i], e[i + 1])};
    if (k.first < 0 || k.second >= n || k.first == k.second || outline.count(k) || !seen.insert(k).second) continue;
    m.pinned.push_back(k.first);
    m.pinned.push_back(k.second);
  }
  return m;
}
/** Outline-based constrained Delaunay triangulation of the shape (it must have a hull). */
void rebuild(MeshShape& m) {
  const PlanarMesh pm = PlanarMesh::fromOutline(m.xy, m.hull, m.pinned);
  m.tris = pm.triangles();
  m.pinned = pm.constraints();
}
void writeShape(EditResult& r, Json& a, const MeshShape& m) {
  if (m.count() < 3 || m.count() > MAX_MESH_VERTICES) throw EditError("메시 정점은 3.." + std::to_string(MAX_MESH_VERTICES) + "개입니다.");
  if (m.tris.size() / 3 > MAX_MESH_TRIANGLES) throw EditError("삼각형이 너무 많습니다(최대 " + std::to_string(MAX_MESH_TRIANGLES) + "개). 정점을 줄이세요.");
  a["type"] = "mesh";
  a["vertices"] = numbersJson(m.xy);
  a["uvs"] = numbersJson(m.uvs);
  a["triangles"] = m.tris;
  if (m.hull >= 3) a["hull"] = m.hull; else a.erase("hull");
  // Spine's convention: the outline edges (derived from the triangles) plus the pinned ones.
  std::set<std::pair<int, int>> all;
  const std::vector<int> b = boundaryEdges(m.tris);
  for (size_t i = 0; i + 1 < b.size(); i += 2) all.insert({b[i], b[i + 1]});
  for (size_t i = 0; i + 1 < m.pinned.size(); i += 2) all.insert({std::min(m.pinned[i], m.pinned[i + 1]), std::max(m.pinned[i], m.pinned[i + 1])});
  Json edges = Json::array();
  for (const auto& [x, y] : all) { edges.push_back(x); edges.push_back(y); }
  a["edges"] = edges;
  r.extra["vertices"] = m.count();
  r.extra["triangles"] = m.tris.size() / 3;
  r.extra["hull"] = m.hull;
  r.extra["pinnedEdges"] = m.pinned.size() / 2;
}
/** uv of a vertex: kept, blended from the old mesh where it lies on it, else position / image size. */
void blendUv(const MeshShape& old, const Origin& o, double px, double py, double w, double h, double& u, double& v) {
  if (o.from >= 0) { u = old.uvs[2 * size_t(o.from)]; v = old.uvs[2 * size_t(o.from) + 1]; return; }
  if (o.v[0] >= 0) {
    u = v = 0;
    for (int k = 0; k < 3; k++) { u += o.w[k] * old.uvs[2 * size_t(o.v[k])]; v += o.w[k] * old.uvs[2 * size_t(o.v[k]) + 1]; }
  } else { u = px / w; v = py / h; }
  u = std::clamp(u, 0.0, 1.0);
  v = std::clamp(v, 0.0, 1.0);
}
/**
 * Weights and this attachment's deform keys follow a vertex change: kept vertices keep theirs,
 * new ones blend their old triangle's (barycentric). Keys with another vertex count belong to a
 * same-named mesh of another skin and are left alone; keys of the old count are rewritten.
 */
void remapVertexData(EditResult& r, Json& a, const Str& slotId, const Str& name, size_t oldN, const std::vector<Origin>& origins) {
  bool identity = origins.size() == oldN;
  for (size_t i = 0; i < origins.size() && identity; i++) identity = origins[i].from == int(i);
  if (identity) return;
  if (a.contains("weights")) {
    const Weights old = weightsOf(r.data, a, oldN);
    Weights next(origins.size());
    const Str slotBone = optStr(slot(r.data, slotId), "bone");
    for (size_t i = 0; i < origins.size(); i++) {
      const Origin& o = origins[i];
      if (o.from >= 0) { next[i] = old[size_t(o.from)]; continue; }
      std::vector<std::pair<Str, double>> mix;
      for (int k = 0; k < 3; k++) {
        if (o.v[k] < 0) continue;
        for (const auto& [bone, w] : old[size_t(o.v[k])]) {
          auto it = std::find_if(mix.begin(), mix.end(), [&](const auto& x) { return x.first == bone; });
          if (it == mix.end()) mix.push_back({bone, w * o.w[k]}); else it->second += w * o.w[k];
        }
      }
      if (mix.empty()) mix.push_back({slotBone, 1});
      normalizeWeights(mix);
      next[i] = mix;
    }
    writeWeights(a, next);
  }
  size_t remapped = 0;
  for (auto& c : arr(r.data, "clips"))
    for (auto& t : arr(c, "tracks")) {
      if (t.value("target", Str()) != "slot:" + slotId || t.value("property", Str()) != "deform" || optStr(t, "attachment") != name) continue;
      for (auto& k : arr(t, "keys")) {
        const std::vector<double> off = numbersOf(k, "offsets");
        if (off.size() != 2 * oldN) continue;
        std::vector<double> next(2 * origins.size(), 0.0);
        for (size_t i = 0; i < origins.size(); i++)
          for (size_t c2 = 0; c2 < 2; c2++) {
            const Origin& o = origins[i];
            if (o.from >= 0) next[2 * i + c2] = off[2 * size_t(o.from) + c2];
            else for (int m = 0; m < 3; m++) if (o.v[m] >= 0) next[2 * i + c2] += o.w[m] * off[2 * size_t(o.v[m]) + c2];
          }
        k["offsets"] = numbersJson(next);
        remapped++;
      }
    }
  if (remapped) warn(r, "ANIM_MESH_DEFORM_REMAPPED", "", "메시 정점이 바뀌어 변형 키 " + std::to_string(remapped) + "개의 오프셋을 새 정점에 맞췄습니다(새 정점은 주변 정점의 값을 섞음).");
  r.extra["remappedKeys"] = remapped;
}
/** x, y pairs of op[k]: finite, |v| <= 1e5, rounded like stored vertices (triangulation sees the saved values). */
std::vector<double> pointList(const Json& op, const char* k) {
  if (!has(op, k) || !op.at(k).is_array() || op.at(k).size() % 2) throw EditError(Str(k) + "는 x, y 숫자 쌍의 배열입니다.");
  std::vector<double> out;
  for (const auto& v : op.at(k)) {
    if (!v.is_number() || !std::isfinite(v.get<double>()) || std::fabs(v.get<double>()) > 1e5) throw EditError(Str(k) + " 값은 유한한 숫자여야 합니다.");
    out.push_back(clean(v.get<double>()));
  }
  return out;
}
std::pair<int, int> edgeOf(const Json& op, size_t n) {
  if (!has(op, "edge") || !op.at("edge").is_array() || op.at("edge").size() != 2) throw EditError("edge는 정점 번호 두 개 [a, b]입니다.");
  int e[2];
  for (size_t i = 0; i < 2; i++) {
    const Json& v = op.at("edge")[i];
    if (!v.is_number_integer() || v.get<int64_t>() < 0 || v.get<int64_t>() >= int64_t(n)) throw EditError("정점 번호가 범위(0.." + std::to_string(n - 1) + ") 밖입니다: " + v.dump());
    e[i] = int(v.get<int64_t>());
  }
  if (e[0] == e[1]) throw EditError("가장자리는 서로 다른 두 정점을 잇습니다.");
  return {std::min(e[0], e[1]), std::max(e[0], e[1])};
}
/**
 * createMeshFromOutline / autoOutline: the attachment (region or mesh) becomes the constrained
 * Delaunay mesh of `hull` (image px, either direction, simple) plus `points` (interior) and a grid
 * every `spacing` px inside, with pinned `edges` (index pairs into hull + points). uvs = position
 * / image size. Re-outlining a mesh resamples its weights and deform keys at the new vertices.
 */
void outlineMesh(EditResult& r, const Json& op, const std::vector<double>& hull) {
  Json& d = r.data;
  Str slotId, name;
  Json& a = attachmentEntry(d, op, slotId, name);
  const double w = num(a, "width", 0), h = num(a, "height", 0);
  if (!(w > 0 && h > 0)) throw EditError("그림 크기(width, height)가 없는 첨부입니다.");
  MeshShape m;
  m.xy = hull;
  m.hull = hull.size() / 2;
  if (m.hull < 3) throw EditError("윤곽에는 점이 3개 이상 필요합니다.");
  if (has(op, "points")) { const auto p = pointList(op, "points"); m.xy.insert(m.xy.end(), p.begin(), p.end()); }
  if (has(op, "spacing")) {
    const double spacing = num(op, "spacing", 0);
    if (spacing != 0 && (spacing < 2 || spacing > 1e4)) throw EditError("spacing(안쪽 정점 간격)은 0(없음) 또는 2..10000 px입니다.");
    for (double v : interiorGrid(hull, spacing)) m.xy.push_back(clean(v));
  }
  if (m.count() > MAX_MESH_VERTICES) throw EditError("정점이 너무 많습니다(최대 " + std::to_string(MAX_MESH_VERTICES) + "개). 간격을 넓히세요.");
  if (has(op, "edges")) {
    if (!op.at("edges").is_array() || op.at("edges").size() % 2) throw EditError("edges는 정점 번호 쌍의 배열입니다.");
    for (const auto& v : op.at("edges")) {
      if (!v.is_number_integer() || v.get<int64_t>() < 0 || v.get<int64_t>() >= int64_t(m.count())) throw EditError("edges의 정점 번호가 범위를 벗어났습니다: " + v.dump());
      m.pinned.push_back(int(v.get<int64_t>()));
    }
  }
  rebuild(m);
  const bool wasMesh = isMesh(a);
  const MeshShape old = wasMesh ? readShape(a) : MeshShape();
  std::vector<Origin> origins(m.count());
  for (size_t i = 0; i < m.count(); i++) {
    const double px = m.xy[2 * i], py = m.xy[2 * i + 1];
    for (size_t j = 0; j < old.count() && origins[i].from < 0; j++)
      if (std::fabs(old.xy[2 * j] - px) < 1e-6 && std::fabs(old.xy[2 * j + 1] - py) < 1e-6) origins[i].from = int(j);
    if (origins[i].from < 0 && wasMesh) origins[i] = locateIn(old.xy, old.tris, px, py);
    m.uvs.push_back(std::clamp(px / w, 0.0, 1.0));
    m.uvs.push_back(std::clamp(py / h, 0.0, 1.0));
  }
  if (wasMesh) remapVertexData(r, a, slotId, name, old.count(), origins);
  else a.erase("weights");
  writeShape(r, a, m);
  r.extra["keys"] = purgeDeform(r);
  r.affected.push_back("slot:" + slotId);
}
void createMeshFromOutline(EditResult& r, const Json& op) { outlineMesh(r, op, pointList(op, "hull")); }

std::vector<uint8_t> decodeBase64(const Str& s) {
  static const Str table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::vector<uint8_t> out;
  out.reserve(s.size() * 3 / 4);
  uint32_t acc = 0;
  int bits = 0;
  for (char ch : s) {
    if (ch == '=' || ch == '\n' || ch == '\r' || ch == ' ') continue;
    const size_t v = table.find(ch);
    if (v == Str::npos) throw EditError("mask.alpha는 base64 문자열입니다.");
    acc = ((acc << 6) | uint32_t(v)) & 0xffffff;
    bits += 6;
    if (bits >= 8) { bits -= 8; out.push_back(uint8_t((acc >> bits) & 0xff)); }
  }
  return out;
}
/**
 * autoOutline {slot, name?, skin?, mask: {width, height, alpha: base64 bytes, one per pixel},
 * alphaThreshold? = 0.05, detail? = 0.5, spacing?, points?, edges?}: the outline of the largest
 * opaque region of the mask (traceAlphaOutline), scaled from mask to image pixels, meshed like
 * createMeshFromOutline. The core decodes no PNG: Qt passes the decoded image's alpha, the node
 * service fills `mask` from `imagePath` (PNG) before calling the core.
 */
void autoOutline(EditResult& r, const Json& op) {
  if (!has(op, "mask") || !op.at("mask").is_object()) throw EditError("mask(그림의 알파 값: width, height, alpha)가 필요합니다. 서비스에서는 imagePath(PNG)로 채울 수 있습니다.");
  const Json& mask = op.at("mask");
  const double mw = num(mask, "width", 0), mh = num(mask, "height", 0);
  if (mw < 1 || mh < 1 || mw > 8192 || mh > 8192 || mw != std::floor(mw) || mh != std::floor(mh)) throw EditError("mask 크기는 1..8192 정수입니다.");
  const std::vector<uint8_t> alpha = decodeBase64(optStr(mask, "alpha"));
  if (alpha.size() != size_t(mw) * size_t(mh)) throw EditError("mask.alpha 길이(" + std::to_string(alpha.size()) + ")가 width × height와 다릅니다.");
  const double threshold = num(op, "alphaThreshold", 0.05), detail = num(op, "detail", 0.5);
  if (threshold < 0 || threshold > 1 || detail < 0 || detail > 1) throw EditError("alphaThreshold와 detail은 0..1입니다.");
  Str slotId, name;
  const Json& a = attachmentEntry(r.data, op, slotId, name);
  const double sx = num(a, "width", mw) / mw, sy = num(a, "height", mh) / mh;
  std::vector<double> hull = traceAlphaOutline(alpha, int(mw), int(mh), threshold, detail);
  for (size_t i = 0; i < hull.size(); i += 2) { hull[i] = clean(hull[i] * sx); hull[i + 1] = clean(hull[i + 1] * sy); }
  outlineMesh(r, op, hull);
  r.extra["outline"] = numbersJson(hull);
}
bool hasOutline(const MeshShape& m) { return m.hull >= 3; }
Str outlineHint() { return " (윤곽(hull)이 없는 메시입니다. 「윤곽 그리기」·「자동 윤곽」으로 다시 만들면 자유롭게 고칠 수 있습니다.)"; }
/**
 * addMeshVertex {slot, name?, skin?, position: [x, y]} (image px). With an outline (`hull`): a point
 * inside becomes an interior vertex (appended), a point on an outline edge an outline vertex
 * (inserted after that edge's first vertex; later vertices shift by one), then the whole mesh is
 * re-triangulated (constrained Delaunay, outline and pinned edges kept). Without one (alpha-trimmed
 * grids): the triangle under the point is split locally. Weights, uvs and deform offsets of the new
 * vertex are interpolated from the triangle it lands in; extra.vertex = its index.
 */
void addMeshVertex(EditResult& r, const Json& op) {
  Json& d = r.data;
  Str slotId, name;
  Json& a = meshEntry(d, op, slotId, name);
  const std::vector<double> pos = pointList(op, "position");
  if (pos.size() != 2) throw EditError("position은 [x, y]입니다.");
  const MeshShape old = readShape(a);
  const size_t n = old.count();
  MeshShape m = old;
  std::vector<Origin> origins(n);
  for (size_t i = 0; i < n; i++) origins[i].from = int(i);
  const Origin here = locateIn(old.xy, old.tris, pos[0], pos[1]);
  int index = int(n);
  if (hasOutline(old)) {
    const std::vector<double> outline(old.xy.begin(), old.xy.begin() + std::ptrdiff_t(2 * old.hull));
    double scale = 1;
    for (double v : outline) scale = std::max(scale, std::fabs(v));
    const double onTol = 1e-6 * scale;
    int edgeAt = -1;
    for (size_t i = 0; i < old.hull && edgeAt < 0; i++) {
      const size_t j = (i + 1) % old.hull;
      if (segmentDistance(pos[0], pos[1], old.xy[2 * i], old.xy[2 * i + 1], old.xy[2 * j], old.xy[2 * j + 1]) <= onTol) edgeAt = int(i);
    }
    if (edgeAt < 0 && !insidePolygon(outline, pos[0], pos[1])) throw EditError("윤곽 밖입니다. 정점은 메시 안쪽이나 윤곽 위에 추가하세요.");
    if (edgeAt >= 0) {
      // A new outline vertex keeps the outline-first order: it goes right after the edge's start.
      index = edgeAt + 1;
      m.xy.insert(m.xy.begin() + 2 * index, pos.begin(), pos.end());
      origins.insert(origins.begin() + index, here);
      m.hull++;
      for (int& v : m.pinned) if (v >= index) v++;
    } else {
      m.xy.insert(m.xy.end(), pos.begin(), pos.end());
      origins.push_back(here);
    }
    rebuild(m);
  } else {
    PlanarMesh pm(old.xy, old.tris, old.pinned);
    pm.addPoint(pos[0], pos[1], true);
    m.xy = pm.points();
    m.tris = pm.triangles();
    m.pinned = pm.constraints();
    origins.push_back(here);
  }
  m.uvs.clear();
  for (size_t i = 0; i < m.count(); i++) {
    double u, v;
    blendUv(old, origins[i], m.xy[2 * i], m.xy[2 * i + 1], num(a, "width", 1), num(a, "height", 1), u, v);
    m.uvs.push_back(u);
    m.uvs.push_back(v);
  }
  remapVertexData(r, a, slotId, name, n, origins);
  writeShape(r, a, m);
  r.extra["vertex"] = index;
  r.affected.push_back("slot:" + slotId);
}
/**
 * removeMeshVertex {slot, name?, skin?, vertices}: the listed vertices go, later ones shift down.
 * With an outline, outline vertices may go while >= 3 stay and the outline stays simple with every
 * interior vertex inside; then the mesh is re-triangulated. Without one only interior vertices can
 * go (their neighbourhood is re-triangulated). Pinned edges touching them are dropped; weights,
 * uvs and deform offsets of the other vertices are kept.
 */
void removeMeshVertex(EditResult& r, const Json& op) {
  Json& d = r.data;
  Str slotId, name;
  Json& a = meshEntry(d, op, slotId, name);
  const MeshShape old = readShape(a);
  const size_t n = old.count();
  const std::vector<size_t> list = vertexList(op, n, true);
  if (n - list.size() < 3) throw EditError("메시에는 정점이 3개 이상 남아야 합니다.");
  std::vector<int> renumber(n, -1);
  std::vector<bool> gone(n, false);
  for (size_t v : list) gone[v] = true;
  MeshShape m;
  std::vector<Origin> origins;
  for (size_t i = 0; i < n; i++) {
    if (gone[i]) continue;
    renumber[i] = int(m.count());
    m.xy.push_back(old.xy[2 * i]);
    m.xy.push_back(old.xy[2 * i + 1]);
    m.uvs.push_back(old.uvs[2 * i]);
    m.uvs.push_back(old.uvs[2 * i + 1]);
    Origin o;
    o.from = int(i);
    origins.push_back(o);
    if (i < old.hull) m.hull++;
  }
  for (size_t i = 0; i + 1 < old.pinned.size(); i += 2) {
    const int x = renumber[size_t(old.pinned[i])], y = renumber[size_t(old.pinned[i + 1])];
    if (x >= 0 && y >= 0) { m.pinned.push_back(x); m.pinned.push_back(y); }
  }
  if (hasOutline(old)) {
    if (m.hull < 3) throw EditError("윤곽에는 정점이 3개 이상 남아야 합니다.");
    rebuild(m);
  } else {
    PlanarMesh pm(old.xy, old.tris, old.pinned);
    for (size_t v : list) {
      if (pm.outlineVertex(int(v))) throw EditError("가장자리 정점(" + std::to_string(v) + ")은 지울 수 없습니다." + outlineHint());
      pm.removeVertex(int(v));
    }
    for (int v : pm.triangles()) m.tris.push_back(renumber[size_t(v)]);
    m.pinned.clear();
    for (int v : pm.constraints()) m.pinned.push_back(renumber[size_t(v)]);
    m.hull = 0;
  }
  remapVertexData(r, a, slotId, name, n, origins);
  writeShape(r, a, m);
  r.extra["removed"] = list.size();
  r.affected.push_back("slot:" + slotId);
}
/** addMeshEdge / removeMeshEdge {slot, name?, skin?, edge: [a, b]}: pin (or unpin) an internal
 *  edge so re-triangulation keeps it (a crease the mesh folds along); outline edges stay. */
void meshEdge(EditResult& r, const Json& op, bool add) {
  Json& d = r.data;
  Str slotId, name;
  Json& a = meshEntry(d, op, slotId, name);
  MeshShape m = readShape(a);
  const auto [x, y] = edgeOf(op, m.count());
  const std::vector<int> b = boundaryEdges(m.tris);
  for (size_t i = 0; i + 1 < b.size(); i += 2)
    if (b[i] == x && b[i + 1] == y) throw EditError(add ? "이미 있는 윤곽 가장자리입니다." : "윤곽 가장자리는 지울 수 없습니다(정점을 지우거나 윤곽을 다시 그리세요).");
  size_t at = m.pinned.size();
  for (size_t i = 0; i + 1 < m.pinned.size(); i += 2) if (std::min(m.pinned[i], m.pinned[i + 1]) == x && std::max(m.pinned[i], m.pinned[i + 1]) == y) at = i;
  if (add && at < m.pinned.size()) throw EditError("이미 고정된 가장자리입니다.");
  if (!add && at == m.pinned.size()) throw EditError("고정된 안쪽 가장자리가 아닙니다: " + std::to_string(x) + "-" + std::to_string(y));
  if (hasOutline(m)) {
    if (add) { m.pinned.push_back(x); m.pinned.push_back(y); }
    else m.pinned.erase(m.pinned.begin() + std::ptrdiff_t(at), m.pinned.begin() + std::ptrdiff_t(at + 2));
    rebuild(m);
  } else {
    PlanarMesh pm(m.xy, m.tris, m.pinned);
    if (add) pm.addConstraint(x, y); else pm.removeConstraint(x, y);
    m.tris = pm.triangles();
    m.pinned = pm.constraints();
  }
  writeShape(r, a, m);
  r.affected.push_back("slot:" + slotId);
}
/** retriangulate {slot, name?, skin?}: constrained Delaunay triangulation of the current vertices
 *  (from the outline when the mesh has one, else by flipping the existing triangles). */
void retriangulate(EditResult& r, const Json& op) {
  Json& d = r.data;
  Str slotId, name;
  Json& a = meshEntry(d, op, slotId, name);
  MeshShape m = readShape(a);
  if (hasOutline(m)) rebuild(m);
  else {
    PlanarMesh pm(m.xy, m.tris, m.pinned);
    pm.delaunay();
    m.tris = pm.triangles();
    m.pinned = pm.constraints();
  }
  writeShape(r, a, m);
  r.affected.push_back("slot:" + slotId);
}

// ---- Clipping (plugin 1.12.0) ----
/** Clip polygon of op `vertices` (the slot bone's px) or `worldVertices` (rig px at the setup pose). */
std::vector<double> clipPolygonOf(Json& d, const Json& op, const Str& boneId) {
  const bool world = has(op, "worldVertices");
  std::vector<double> pts = pointList(op, world ? "worldVertices" : "vertices");
  if (world) {
    const Affine inv = inverseOr(setupWorld(d)[boneId], "본");
    for (size_t i = 0; i + 1 < pts.size(); i += 2) { double x, y; inv.apply(pts[i], pts[i + 1], x, y); pts[i] = clean(x); pts[i + 1] = clean(y); }
  }
  if (pts.size() / 2 < 3 || pts.size() / 2 > MAX_CLIP_VERTICES) throw EditError("클리핑 다각형은 점 3.." + std::to_string(MAX_CLIP_VERTICES) + "개입니다.");
  if (const Str problem = polygonProblem(pts); !problem.empty()) throw EditError("클리핑 다각형이 올바르지 않습니다: " + problem);
  return pts;
}
/** Clip ranges ending at slot `deleted` end at the slot before it instead. Returns the clipping
 *  slots whose range that empties (the caller deletes them with `deleted`). */
std::set<Str> retargetClipEnds(EditResult& r, const Str& deleted) {
  Json& d = r.data;
  const auto& slots = arr(d, "slots");
  const int at = findIndex(slots, "id", deleted);
  std::set<Str> emptied;
  auto fix = [&](Json& list) {
    for (auto& a : list) {
      if (!isClipping(a) || optStr(a, "end") != deleted || optStr(a, "slot") == deleted) continue;
      const int from = findIndex(slots, "id", optStr(a, "slot"));
      if (at - 1 > from) { a["end"] = slots[size_t(at - 1)].value("id", Str()); continue; }
      warn(r, "ANIM_CLIP_REMOVED", "", "클리핑 「" + optStr(a, "slot") + "」의 범위가 비어 그 클리핑 조각도 지웠습니다.");
      emptied.insert(optStr(a, "slot"));
    }
  };
  fix(arr(d, "attachments"));
  if (d.contains("skins") && d["skins"].is_array()) for (auto& s : d["skins"]) if (s.is_object() && s.contains("attachments") && s["attachments"].is_array()) fix(s["attachments"]);
  return emptied;
}
/**
 * addClipping {slot (new), bone?, before? | index?, name?, vertices | worldVertices, end?}: a new
 * slot showing a clipping attachment, inserted before slot `before` (default: on top), clipping
 * the slots after it through `end` (default `before`). bone defaults to `before`'s bone.
 */
void addClipping(EditResult& r, const Json& op) {
  Json& d = r.data;
  const Str slotId = reqStr(op, "slot"), before = optStr(op, "before");
  if (!validId(slotId)) throw EditError("조각 ID는 소문자 영문·숫자·하이픈 형식이어야 합니다: " + slotId);
  auto& slots = arr(d, "slots");
  if (findIndex(slots, "id", slotId) >= 0) throw EditError("이미 있는 조각 ID입니다: " + slotId);
  Str boneId = optStr(op, "bone");
  if (!before.empty() && boneId.empty()) boneId = slot(d, before).value("bone", Str());
  if (boneId.empty()) throw EditError("클리핑을 붙일 본을 선택하세요.");
  bone(d, boneId);
  const Str name = optStr(op, "name", "clip");
  if (!validName(name)) throw EditError("첨부 이름 형식이 올바르지 않습니다: " + name);
  const std::vector<double> polygon = clipPolygonOf(d, op, boneId);
  const Str end = optStr(op, "end", before);
  if (end.empty()) throw EditError("클리핑할 마지막 조각(end)을 지정하세요.");
  slot(d, end);
  const int64_t index = !before.empty() ? findIndex(slots, "id", before) : std::clamp<int64_t>(op.value("index", int64_t(slots.size())), 0, int64_t(slots.size()));
  slots.insert(slots.begin() + index, Json{{"id", slotId}, {"bone", boneId}, {"attachment", name}});
  arr(d, "attachments").push_back(Json{{"slot", slotId}, {"name", name}, {"type", "clipping"}, {"vertices", numbersJson(polygon)}, {"end", end}});
  r.affected.push_back("slot:" + slotId);
  r.extra["attachment"] = name;
}
/** setClipping {slot, name?, skin?, vertices? | worldVertices?, end?}: a new polygon (weights and deform keys
 *  dropped when the point count changes) and/or a new last clipped slot. */
void setClipping(EditResult& r, const Json& op) {
  Json& d = r.data;
  Str slotId, name;
  Json& a = attachmentEntry(d, op, slotId, name);
  if (!isClipping(a)) throw EditError("클리핑 첨부가 아닙니다: " + slotId + "/" + name);
  if (has(op, "vertices") || has(op, "worldVertices")) {
    const std::vector<double> polygon = clipPolygonOf(d, op, slot(d, slotId).value("bone", Str()));
    if (listOr(a, "vertices").size() != polygon.size() && a.contains("weights")) {
      a.erase("weights");
      warn(r, "ANIM_CLIP_WEIGHTS_DROPPED", "", "클리핑 점 수가 바뀌어 웨이트를 지웠습니다.");
    }
    a["vertices"] = numbersJson(polygon);
    if (const size_t removed = purgeDeform(r)) warn(r, "ANIM_MESH_DEFORM_REMOVED", "", "점 수가 바뀌어 클리핑 변형 키 트랙 " + std::to_string(removed) + "개를 지웠습니다.");
  }
  if (has(op, "end")) {
    const Str end = reqStr(op, "end");
    slot(d, end);
    a["end"] = end;
  }
  r.affected.push_back("slot:" + slotId);
}

// ---- Image sequences (plugin 1.12.0) ----
/**
 * setSequence {slot, name?, skin?, frames, setupIndex?, width?, height?}: the attachment shows
 * `frames` (image assets of one size; imageAssetId = frames[0], width/height when given). Keys of
 * the slot that index past the new frame count are clamped (ANIM_SEQUENCE_INDEX_CLAMPED).
 * frames null / [] removes the sequence; the slot's sequence keys go when no sequence is left.
 */
void setSequence(EditResult& r, const Json& op) {
  Json& d = r.data;
  Str slotId, name;
  Json& a = attachmentEntry(d, op, slotId, name);
  if (isClipping(a)) throw EditError("클리핑 첨부에는 시퀀스를 쓸 수 없습니다.");
  const bool clear = !has(op, "frames") || (op.at("frames").is_array() && op.at("frames").empty());
  if (clear) {
    if (!a.contains("sequence")) throw EditError("시퀀스가 없는 첨부입니다: " + slotId + "/" + name);
    a.erase("sequence");
  } else {
    const Json& frames = op.at("frames");
    if (!frames.is_array() || frames.size() > MAX_SEQUENCE_FRAMES) throw EditError("frames는 이미지 에셋 ID 1.." + std::to_string(MAX_SEQUENCE_FRAMES) + "개의 배열입니다.");
    for (const auto& f : frames) if (!f.is_string() || f.get<Str>().empty() || f.get<Str>().size() > 64) throw EditError("프레임은 이미지 에셋 ID입니다: " + f.dump());
    const int64_t previous = a.contains("sequence") && a.at("sequence").is_object() ? a.at("sequence").value("setupIndex", int64_t(0)) : 0;
    const int64_t setup = has(op, "setupIndex") ? op.at("setupIndex").get<int64_t>() : std::min<int64_t>(previous, int64_t(frames.size()) - 1);
    if (setup < 0 || setup >= int64_t(frames.size())) throw EditError("기본 프레임(setupIndex)은 0.." + std::to_string(frames.size() - 1) + "입니다.");
    Json q{{"frames", frames}};
    if (setup) q["setupIndex"] = setup;
    a["sequence"] = q;
    a["imageAssetId"] = frames[0];
    for (const char* k : {"width", "height"}) if (has(op, k)) { const double v = num(op, k, 0); if (!(v > 0)) throw EditError("이미지 크기는 0보다 커야 합니다."); a[k] = clean(v); }
  }
  // Keys follow the frames that exist now: past the end -> the last frame; no sequence left -> gone.
  const size_t frames = sequenceFrames(d, slotId);
  size_t clamped = 0, removed = 0;
  for (auto& c : arr(d, "clips")) {
    auto& tracks = arr(c, "tracks");
    for (auto it = tracks.begin(); it != tracks.end();) {
      if (it->value("target", Str()) != "slot:" + slotId || it->value("property", Str()) != "sequence") { ++it; continue; }
      if (!frames) { removed += listOr(*it, "keys").size(); it = tracks.erase(it); continue; }
      for (auto& k : arr(*it, "keys")) if (k.value("index", int64_t(0)) >= int64_t(frames)) { k["index"] = int64_t(frames) - 1; clamped++; }
      ++it;
    }
  }
  if (clamped) warn(r, "ANIM_SEQUENCE_INDEX_CLAMPED", "", "프레임 수가 줄어 시퀀스 키 " + std::to_string(clamped) + "개를 마지막 프레임으로 맞췄습니다.");
  if (removed) warn(r, "ANIM_SEQUENCE_KEYS_REMOVED", "", "시퀀스가 없어져 시퀀스 키 " + std::to_string(removed) + "개를 지웠습니다.");
  r.affected.push_back("slot:" + slotId);
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
    {"addBone", addBone}, {"setBone", setBone}, {"transformBones", transformBones}, {"reparentBone", reparentBone}, {"setPivot", setPivot}, {"deleteBone", deleteBone},
    {"addPart", addPart}, {"setSlot", setSlot}, {"setAttachment", setAttachment}, {"deleteSlot", deleteSlot}, {"moveSlot", moveSlot},
    {"setKey", setKey}, {"deleteKey", deleteKey}, {"moveKey", moveKey}, {"setInterp", setInterp}, {"setCurve", setCurve},
    {"moveKeys", moveKeys}, {"scaleKeys", scaleKeys}, {"deleteKeys", deleteKeys}, {"copyKeys", copyKeys}, {"pasteKeys", pasteKeys},
    {"copyPose", copyPose}, {"pastePose", pastePose},
    {"addEventDef", addEventDef}, {"setEventDef", setEventDef}, {"renameEventDef", renameEventDef}, {"deleteEventDef", deleteEventDef},
    {"setEventKey", setEventKey}, {"moveEventKey", moveEventKey}, {"deleteEventKey", deleteEventKey},
    {"addClip", addClip}, {"duplicateClip", duplicateClip}, {"setClip", setClip}, {"setMix", setMix}, {"setCanvas", setCanvas},
    {"applySetupDeltaToKeys", applySetupDeltaToKeys},
    {"addSkin", addSkin}, {"duplicateSkin", duplicateSkin}, {"setSkinAttachment", setSkinAttachment}, {"removeSkinAttachment", removeSkinAttachment},
    {"addExpression", addExpression}, {"setExpressionSlot", setExpressionSlot}, {"setExpressionBone", setExpressionBone}, {"captureExpression", captureExpression},
    {"addIk", addIk}, {"setIk", setIk}, {"fitBoneLengths", fitBoneLengths}, {"orientBones", orientBones}, {"renameIk", renameIk}, {"deleteIk", deleteIk}, {"bakeIk", bakeIk},
    {"convertToMesh", convertToMesh}, {"convertToRegion", convertToRegion}, {"setMeshVertices", setMeshVertices}, {"setWeights", setWeights},
    {"unbindMesh", unbindMesh}, {"autoWeights", [](EditResult& r, const Json& op) { autoWeights(r, op, false); }},
    {"bindMesh", [](EditResult& r, const Json& op) { autoWeights(r, op, true); }},
    {"createMeshFromOutline", createMeshFromOutline}, {"autoOutline", autoOutline}, {"addMeshVertex", addMeshVertex},
    {"removeMeshVertex", removeMeshVertex}, {"retriangulate", retriangulate},
    {"addClipping", addClipping}, {"setClipping", setClipping}, {"setSequence", setSequence},
    {"addMeshEdge", [](EditResult& r, const Json& op) { meshEdge(r, op, true); }},
    {"removeMeshEdge", [](EditResult& r, const Json& op) { meshEdge(r, op, false); }},
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

std::optional<Axes> parseAxes(const std::string& name) {
  if (name == "local") return Axes::Local;
  if (name == "parent") return Axes::Parent;
  if (name == "world") return Axes::World;
  return std::nullopt;
}
namespace {
/** Bring `deg` within ±180 of `near` (rotation fields interpolate numerically: no 360 jumps). */
double unwrapNear(double deg, double near) {
  while (deg - near > 180) deg -= 360;
  while (deg - near < -180) deg += 360;
  return deg;
}
double parentAngle(const Affine& p) { return std::atan2(p.b, p.a) * 180 / PI; }
double parentSign(const Affine& p) { return p.a * p.d - p.b * p.c < 0 ? -1 : 1; }
}  // namespace
Transform toAxes(const Transform& t, const Affine& parent, Axes axes) {
  if (axes == Axes::Local) return t;
  Transform out = t;
  double wx, wy;
  parent.apply(t.x, t.y, wx, wy);
  // The bone's +x axis (before its own scale) on screen: the world rotation a user sees.
  const double r = t.rotation * PI / 180, dx = parent.a * std::cos(r) + parent.c * std::sin(r), dy = parent.b * std::cos(r) + parent.d * std::sin(r);
  const double pa = parentAngle(parent), world = unwrapNear(std::atan2(dy, dx) * 180 / PI, pa + parentSign(parent) * t.rotation);
  if (axes == Axes::World) { out.x = wx; out.y = wy; out.rotation = world; return out; }
  // Parent: offset from the parent origin along the parent's (unscaled) axes, angle from its +x.
  const double ox = wx - parent.e, oy = wy - parent.f, c = std::cos(pa * PI / 180), s = std::sin(pa * PI / 180);
  out.x = c * ox + s * oy;
  out.y = -s * ox + c * oy;
  out.rotation = world - pa;
  return out;
}
std::optional<Transform> fromAxes(const Transform& shown, const Affine& parent, Axes axes, const Transform& hint) {
  if (axes == Axes::Local) return shown;
  const auto inv = parent.inverse();
  if (!inv) return std::nullopt;
  double wx = shown.x, wy = shown.y, world = shown.rotation;
  if (axes == Axes::Parent) {
    const double pa = parentAngle(parent), c = std::cos(pa * PI / 180), s = std::sin(pa * PI / 180);
    wx = parent.e + c * shown.x - s * shown.y;
    wy = parent.f + s * shown.x + c * shown.y;
    world = shown.rotation + pa;
  }
  Transform out = shown;
  inv->apply(wx, wy, out.x, out.y);
  // The local direction that the parent maps onto the shown world direction.
  const double r = world * PI / 180, dx = inv->a * std::cos(r) + inv->c * std::sin(r), dy = inv->b * std::cos(r) + inv->d * std::sin(r);
  out.rotation = unwrapNear(std::atan2(dy, dx) * 180 / PI, hint.rotation);
  return out;
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
