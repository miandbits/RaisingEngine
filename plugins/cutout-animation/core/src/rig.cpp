#include "cutout/rig.hpp"
#include <cmath>
#include <set>
#include <stdexcept>

namespace cutout {
namespace {
constexpr double PI = 3.14159265358979323846;
constexpr int64_t MAX_DURATION_MS = 10 * 60 * 1000;
constexpr double MAX_ABS = 1e6;

struct Checker {
  std::vector<Diagnostic> out;
  void error(const std::string& pointer, const std::string& code, const std::string& message) { out.push_back({code, "error", pointer, message}); }
  void warn(const std::string& pointer, const std::string& code, const std::string& message) { out.push_back({code, "warning", pointer, message}); }
  bool hasErrors() const { for (const auto& d : out) if (d.severity == "error") return true; return false; }
};
std::string esc(const std::string& key) {
  std::string r;
  for (char c : key) { if (c == '~') r += "~0"; else if (c == '/') r += "~1"; else r += c; }
  return r;
}
std::string at(const std::string& base, const std::string& key) { return base + "/" + esc(key); }
std::string at(const std::string& base, size_t index) { return base + "/" + std::to_string(index); }

bool finiteNumber(const Json& v) { return v.is_number() && std::isfinite(v.get<double>()) && std::fabs(v.get<double>()) <= MAX_ABS; }
// Reads an optional numeric field and reports non-finite or out-of-range values.
double number(Checker& c, const Json& obj, const std::string& pointer, const char* key, double fallback) {
  if (!obj.is_object() || !obj.contains(key)) return fallback;
  const auto& v = obj.at(key);
  if (!finiteNumber(v)) { c.error(at(pointer, key), "ANIM_NONFINITE", std::string(key) + " 값은 유한한 숫자여야 합니다."); return fallback; }
  return v.get<double>();
}
std::string text(const Json& obj, const char* key) {
  if (!obj.is_object() || !obj.contains(key) || !obj.at(key).is_string()) return "";
  return obj.at(key).get<std::string>();
}
void checkTransform(Checker& c, const Json& obj, const std::string& pointer) {
  number(c, obj, pointer, "x", 0); number(c, obj, pointer, "y", 0); number(c, obj, pointer, "rotation", 0);
  for (const char* k : {"scaleX", "scaleY"}) {
    const double s = number(c, obj, pointer, k, 1);
    if (s == 0) c.warn(at(pointer, k), "ANIM_SCALE_ZERO", "크기 0은 조각을 보이지 않게 합니다.");
  }
}
Transform readTransform(const Json& obj) {
  Transform t;
  auto get = [&](const char* k, double f) { return obj.contains(k) && obj.at(k).is_number() ? obj.at(k).get<double>() : f; };
  t.x = get("x", 0); t.y = get("y", 0); t.rotation = get("rotation", 0); t.scaleX = get("scaleX", 1); t.scaleY = get("scaleY", 1);
  return t;
}
// Bezier handles: x must stay in [0,1] so segment time never runs backwards.
void checkCurve(Checker& c, const Json& key, const std::string& kp) {
  const std::string p = at(kp, "curve");
  if (!key.contains("curve") || !key.at("curve").is_array() || key.at("curve").size() != 4) {
    c.error(p, "ANIM_CURVE", "bezier 키에는 curve [x1, y1, x2, y2] 숫자 4개가 필요합니다.");
    return;
  }
  for (size_t i = 0; i < 4; i++) {
    const Json& v = key.at("curve")[i];
    if (!v.is_number() || !std::isfinite(v.get<double>())) { c.error(at(p, i), "ANIM_CURVE", "curve 값은 유한한 숫자여야 합니다."); continue; }
    const double x = v.get<double>();
    if (i % 2 == 0 && (x < 0 || x > 1)) c.error(at(p, i), "ANIM_CURVE", "curve의 시간 손잡이(x)는 0..1이어야 합니다.");
    if (i % 2 == 1 && (x < CURVE_Y_MIN || x > CURVE_Y_MAX)) c.error(at(p, i), "ANIM_CURVE", "curve의 값 손잡이(y)는 -4..5이어야 합니다.");
  }
}
const Json& arrayOr(const Json& obj, const char* key) {
  static const Json empty = Json::array();
  return obj.is_object() && obj.contains(key) && obj.at(key).is_array() ? obj.at(key) : empty;
}
}  // namespace

std::optional<Affine> Affine::inverse() const {
  const double det = a * d - b * c;
  if (std::fabs(det) < 1e-12) return std::nullopt;
  const double ia = d / det, ib = -b / det, ic = -c / det, id = a / det;
  return Affine{ia, ib, ic, id, -(ia * e + ic * f), -(ib * e + id * f)};
}
Affine Affine::compose(double x, double y, double rotationDeg, double sx, double sy) {
  const double r = rotationDeg * PI / 180.0, cs = std::cos(r), sn = std::sin(r);
  return {cs * sx, sn * sx, -sn * sy, cs * sy, x, y};
}

const char* propertyName(Property p) {
  switch (p) {
    case Property::X: return "x"; case Property::Y: return "y"; case Property::Rotation: return "rotation";
    case Property::ScaleX: return "scaleX"; case Property::ScaleY: return "scaleY";
    case Property::Alpha: return "alpha"; case Property::Attachment: return "attachment"; case Property::Expression: return "expression";
  }
  return "";
}
std::optional<Property> parseProperty(const std::string& n) {
  for (auto p : {Property::X, Property::Y, Property::Rotation, Property::ScaleX, Property::ScaleY, Property::Alpha, Property::Attachment, Property::Expression})
    if (n == propertyName(p)) return p;
  return std::nullopt;
}

int Rig::boneIndex(const std::string& id) const { for (size_t i = 0; i < bones.size(); i++) if (bones[i].id == id) return int(i); return -1; }
int Rig::slotIndex(const std::string& id) const { for (size_t i = 0; i < slots.size(); i++) if (slots[i].id == id) return int(i); return -1; }
int Rig::clipIndex(const std::string& id) const { for (size_t i = 0; i < clips.size(); i++) if (clips[i].id == id) return int(i); return -1; }
int Rig::skinIndex(const std::string& id) const { for (size_t i = 0; i < skins.size(); i++) if (skins[i].id == id) return int(i); return -1; }
int Rig::expressionIndex(const std::string& id) const { for (size_t i = 0; i < expressions.size(); i++) if (expressions[i].id == id) return int(i); return -1; }
const Attachment* Rig::attachment(const std::string& slot, const std::string& name, int skin) const {
  if (skin >= 0 && skin < int(skins.size()))
    for (const auto& a : skins[size_t(skin)].attachments) if (a.slot == slot && a.name == name) return &a;
  for (const auto& a : attachments) if (a.slot == slot && a.name == name) return &a;
  return nullptr;
}
int64_t Rig::mixDuration(const std::string& from, const std::string& to) const {
  for (const auto& m : mixes) if (m.from == from && m.to == to) return m.durationMs;
  return defaultMixMs;
}

std::vector<Diagnostic> validateRig(const Json& data) {
  Checker c;
  if (!data.is_object()) { c.error("", "ANIM_STRUCTURE", "리그 데이터는 객체여야 합니다."); return c.out; }
  if (!data.contains("schemaVersion") || data.at("schemaVersion") != SCHEMA_VERSION)
    c.error("/schemaVersion", "ANIM_SCHEMA_VERSION", "지원하는 리그 schemaVersion은 1입니다.");
  if (data.contains("canvas")) {
    const auto& canvas = data.at("canvas");
    for (const char* k : {"width", "height"}) if (number(c, canvas, "/canvas", k, 1) <= 0) c.error(at("/canvas", k), "ANIM_CANVAS_SIZE", "캔버스 크기는 0보다 커야 합니다.");
    number(c, canvas, "/canvas", "originX", 0); number(c, canvas, "/canvas", "originY", 0);
  }

  // Bones: unique ids, known parents, no cycles.
  const auto& bones = arrayOr(data, "bones");
  std::map<std::string, std::string> parentOf;
  std::map<std::string, size_t> boneAt;
  for (size_t i = 0; i < bones.size(); i++) {
    const std::string p = at("/bones", i), id = text(bones[i], "id");
    if (id.empty()) { c.error(at(p, "id"), "ANIM_ID_MISSING", "본 ID가 필요합니다."); continue; }
    if (boneAt.count(id)) c.error(at(p, "id"), "ANIM_DUP_ID", "중복된 본 ID: " + id);
    boneAt[id] = i;
    parentOf[id] = bones[i].contains("parent") && bones[i].at("parent").is_string() ? bones[i].at("parent").get<std::string>() : "";
    checkTransform(c, bones[i], p);
  }
  if (bones.empty()) c.error("/bones", "ANIM_NO_BONES", "본이 하나 이상 필요합니다.");
  for (const auto& [id, parent] : parentOf) {
    if (parent.empty()) continue;
    const std::string p = at(at("/bones", boneAt[id]), "parent");
    if (!parentOf.count(parent)) { c.error(p, "ANIM_PARENT_MISSING", "없는 부모 본을 참조합니다: " + parent); continue; }
    std::set<std::string> seen{id};
    for (std::string cur = parent; !cur.empty(); cur = parentOf.count(cur) ? parentOf[cur] : "") {
      if (!seen.insert(cur).second) { c.error(p, "ANIM_PARENT_CYCLE", "부모 관계가 순환합니다: " + id); break; }
    }
  }

  // Slots and attachments.
  const auto& slots = arrayOr(data, "slots");
  std::set<std::string> slotIds;
  for (size_t i = 0; i < slots.size(); i++) {
    const std::string p = at("/slots", i), id = text(slots[i], "id"), bone = text(slots[i], "bone");
    if (id.empty()) c.error(at(p, "id"), "ANIM_ID_MISSING", "슬롯 ID가 필요합니다.");
    else if (!slotIds.insert(id).second) c.error(at(p, "id"), "ANIM_DUP_ID", "중복된 슬롯 ID: " + id);
    if (!parentOf.count(bone)) c.error(at(p, "bone"), "ANIM_BONE_MISSING", "슬롯이 없는 본을 참조합니다: " + bone);
    const double alpha = number(c, slots[i], p, "alpha", 1);
    if (alpha < 0 || alpha > 1) c.error(at(p, "alpha"), "ANIM_ALPHA_RANGE", "투명도는 0..1이어야 합니다.");
  }
  using SlotName = std::pair<std::string, std::string>;
  // One attachment entry (default list or a skin's list); `seen` holds the list's (slot, name) pairs.
  auto checkAttachment = [&](const Json& a, const std::string& p, std::set<SlotName>& seen) {
    const std::string slot = text(a, "slot"), name = text(a, "name");
    if (!slotIds.count(slot)) c.error(at(p, "slot"), "ANIM_SLOT_MISSING", "첨부가 없는 슬롯을 참조합니다: " + slot);
    if (name.empty()) c.error(at(p, "name"), "ANIM_ID_MISSING", "첨부 이름이 필요합니다.");
    else if (!seen.insert({slot, name}).second) c.error(at(p, "name"), "ANIM_DUP_ID", "같은 슬롯에 중복된 첨부 이름: " + name);
    if (text(a, "imageAssetId").empty()) c.error(at(p, "imageAssetId"), "ANIM_IMAGE_MISSING", "첨부에 이미지가 지정되지 않았습니다.");
    checkTransform(c, a, p);
    for (const char* k : {"pivotX", "pivotY"}) number(c, a, p, k, 0.5);
    for (const char* k : {"width", "height"}) if (number(c, a, p, k, 0) <= 0) c.error(at(p, k), "ANIM_IMAGE_SIZE", "이미지 크기는 0보다 커야 합니다.");
  };
  const auto& attachments = arrayOr(data, "attachments");
  std::set<SlotName> attachmentIds;
  for (size_t i = 0; i < attachments.size(); i++) checkAttachment(attachments[i], at("/attachments", i), attachmentIds);

  // Skins (plugin 1.2.0): per-skin attachment lists keyed like the default list.
  const auto& skins = arrayOr(data, "skins");
  std::set<std::string> skinIds;
  std::vector<std::set<SlotName>> skinNames(skins.size());
  std::set<SlotName> anySkin;  // (slot, name) provided by at least one skin
  for (size_t i = 0; i < skins.size(); i++) {
    const std::string p = at("/skins", i), id = text(skins[i], "id");
    if (id.empty()) c.error(at(p, "id"), "ANIM_ID_MISSING", "스킨 ID가 필요합니다.");
    else if (!skinIds.insert(id).second) c.error(at(p, "id"), "ANIM_DUP_ID", "중복된 스킨 ID: " + id);
    const auto& list = arrayOr(skins[i], "attachments");
    for (size_t k = 0; k < list.size(); k++) checkAttachment(list[k], at(at(p, "attachments"), k), skinNames[i]);
    anySkin.insert(skinNames[i].begin(), skinNames[i].end());
  }
  // A name is usable when the default list or any skin provides it (an outfit-only part).
  auto knownName = [&](const std::string& slot, const std::string& name) { return attachmentIds.count({slot, name}) || anySkin.count({slot, name}); };
  std::set<SlotName> referenced;  // names that setup/keys/expressions show: every skin should resolve them
  for (size_t i = 0; i < slots.size(); i++) {
    if (!slots[i].contains("attachment") || !slots[i].at("attachment").is_string()) continue;
    const auto name = slots[i].at("attachment").get<std::string>();
    if (!knownName(text(slots[i], "id"), name)) c.error(at(at("/slots", i), "attachment"), "ANIM_ATTACHMENT_UNKNOWN", "슬롯 기본 첨부가 없습니다: " + name);
    else referenced.insert({text(slots[i], "id"), name});
  }

  // Expressions (plugin 1.2.0): {id, name, slots: {slot: name|null}, bones: {bone: {channel: value}}}.
  const auto& expressions = arrayOr(data, "expressions");
  std::set<std::string> expressionIds;
  for (size_t i = 0; i < expressions.size(); i++) {
    const auto& e = expressions[i];
    const std::string p = at("/expressions", i), id = text(e, "id");
    if (id.empty()) c.error(at(p, "id"), "ANIM_ID_MISSING", "표정 ID가 필요합니다.");
    else if (!expressionIds.insert(id).second) c.error(at(p, "id"), "ANIM_DUP_ID", "중복된 표정 ID: " + id);
    if (e.is_object() && e.contains("slots") && e.at("slots").is_object())
      for (auto it = e.at("slots").begin(); it != e.at("slots").end(); ++it) {
        const std::string sp = at(at(p, "slots"), it.key());
        if (!slotIds.count(it.key())) { c.error(sp, "ANIM_SLOT_MISSING", "표정이 없는 슬롯을 참조합니다: " + it.key()); continue; }
        if (it.value().is_null()) continue;
        if (!it.value().is_string()) c.error(sp, "ANIM_KEY_VALUE", "표정의 슬롯 값은 첨부 이름 또는 null입니다.");
        else if (!knownName(it.key(), it.value().get<std::string>())) c.error(sp, "ANIM_ATTACHMENT_UNKNOWN", "슬롯에 없는 첨부를 가리킵니다: " + it.value().get<std::string>());
        else referenced.insert({it.key(), it.value().get<std::string>()});
      }
    else if (e.is_object() && e.contains("slots")) c.error(at(p, "slots"), "ANIM_STRUCTURE", "표정 slots는 객체입니다.");
    if (e.is_object() && e.contains("bones") && e.at("bones").is_object())
      for (auto it = e.at("bones").begin(); it != e.at("bones").end(); ++it) {
        const std::string bp = at(at(p, "bones"), it.key());
        if (!parentOf.count(it.key())) { c.error(bp, "ANIM_BONE_MISSING", "표정이 없는 본을 참조합니다: " + it.key()); continue; }
        if (!it.value().is_object()) { c.error(bp, "ANIM_STRUCTURE", "표정의 본 값은 {rotation, x, y, scaleX, scaleY} 객체입니다."); continue; }
        for (auto ch = it.value().begin(); ch != it.value().end(); ++ch) {
          const auto prop = parseProperty(ch.key());
          if (!prop || *prop == Property::Alpha || *prop == Property::Attachment || *prop == Property::Expression)
            c.error(at(bp, ch.key()), "ANIM_PROPERTY", "표정의 본 속성은 x, y, rotation, scaleX, scaleY입니다: " + ch.key());
          else if (!finiteNumber(ch.value())) c.error(at(bp, ch.key()), "ANIM_NONFINITE", ch.key() + " 값은 유한한 숫자여야 합니다.");
        }
      }
    else if (e.is_object() && e.contains("bones")) c.error(at(p, "bones"), "ANIM_STRUCTURE", "표정 bones는 객체입니다.");
  }

  // Clips.
  const auto& clips = arrayOr(data, "clips");
  std::set<std::string> clipIds;
  for (size_t ci = 0; ci < clips.size(); ci++) {
    const auto& clip = clips[ci];
    const std::string p = at("/clips", ci), id = text(clip, "id");
    if (id.empty()) c.error(at(p, "id"), "ANIM_ID_MISSING", "클립 ID가 필요합니다.");
    else if (!clipIds.insert(id).second) c.error(at(p, "id"), "ANIM_DUP_ID", "중복된 클립 ID: " + id);
    int64_t duration = 0;
    if (!clip.contains("durationMs") || !clip.at("durationMs").is_number_integer() || clip.at("durationMs").get<int64_t>() <= 0 || clip.at("durationMs").get<int64_t>() > MAX_DURATION_MS)
      c.error(at(p, "durationMs"), "ANIM_DURATION", "클립 길이는 1..600000ms 정수여야 합니다.");
    else duration = clip.at("durationMs").get<int64_t>();
    const auto& tracks = arrayOr(clip, "tracks");
    std::set<std::string> targets;
    for (size_t ti = 0; ti < tracks.size(); ti++) {
      const auto& track = tracks[ti];
      const std::string tp = at(at(p, "tracks"), ti), target = text(track, "target"), propertyText = text(track, "property");
      const auto property = parseProperty(propertyText);
      const bool isBone = target.rfind("bone:", 0) == 0, isSlot = target.rfind("slot:", 0) == 0, isRig = target == "rig";
      const std::string targetId = target.size() > 5 ? target.substr(5) : "";
      if (!property) { c.error(at(tp, "property"), "ANIM_PROPERTY", "알 수 없는 트랙 속성: " + propertyText); continue; }
      const bool slotProperty = *property == Property::Alpha || *property == Property::Attachment;
      if (isRig != (*property == Property::Expression))
        c.error(at(tp, "property"), "ANIM_PROPERTY", isRig ? "rig 트랙의 속성은 expression뿐입니다." : "expression 속성은 rig 트랙에만 씁니다.");
      else if (isRig) {}
      else if ((!isBone && !isSlot) || (isBone && !parentOf.count(targetId)) || (isSlot && !slotIds.count(targetId)))
        c.error(at(tp, "target"), "ANIM_TARGET_MISSING", "트랙 대상이 없습니다: " + target);
      else if (isBone == slotProperty) c.error(at(tp, "property"), "ANIM_PROPERTY", "대상 종류에 맞지 않는 속성입니다: " + propertyText);
      if (!targets.insert(target + "#" + propertyText).second) c.error(tp, "ANIM_DUP_TRACK", "같은 대상·속성 트랙이 중복됩니다.");
      const auto& keys = arrayOr(track, "keys");
      if (keys.empty()) c.warn(at(tp, "keys"), "ANIM_EMPTY_TRACK", "키가 없는 트랙입니다.");
      int64_t previous = -1;
      for (size_t ki = 0; ki < keys.size(); ki++) {
        const auto& key = keys[ki];
        const std::string kp = at(at(tp, "keys"), ki);
        if (!key.contains("timeMs") || !key.at("timeMs").is_number_integer()) { c.error(at(kp, "timeMs"), "ANIM_KEY_TIME", "키 시간은 정수 ms여야 합니다."); continue; }
        const int64_t t = key.at("timeMs").get<int64_t>();
        if (t < 0 || (duration > 0 && t > duration)) c.error(at(kp, "timeMs"), "ANIM_KEY_TIME_RANGE", "키 시간이 클립 범위(0..길이)를 벗어났습니다.");
        if (t <= previous) c.error(at(kp, "timeMs"), "ANIM_KEY_UNSORTED", "키는 시간순으로 중복 없이 정렬되어야 합니다.");
        previous = t;
        const std::string interp = text(key, "interp");
        if (!interp.empty() && interp != "step" && interp != "linear" && interp != "bezier") c.error(at(kp, "interp"), "ANIM_INTERP", "보간은 step, linear, bezier 중 하나입니다.");
        else if (interp == "bezier" && (*property == Property::Attachment || *property == Property::Expression))
          c.error(at(kp, "interp"), "ANIM_INTERP", "첨부·표정 키는 항상 계단(step) 전환입니다.");
        if (interp == "bezier") checkCurve(c, key, kp);
        else if (key.contains("curve")) c.warn(at(kp, "curve"), "ANIM_CURVE_UNUSED", "bezier가 아닌 키의 curve는 쓰이지 않습니다.");
        if (*property == Property::Expression) {
          if (!key.contains("value") || (!key.at("value").is_null() && !key.at("value").is_string())) c.error(at(kp, "value"), "ANIM_KEY_VALUE", "표정 키 값은 표정 ID 또는 null입니다.");
          else if (key.at("value").is_string() && !expressionIds.count(key.at("value").get<std::string>()))
            c.error(at(kp, "value"), "ANIM_EXPRESSION_UNKNOWN", "없는 표정을 가리킵니다: " + key.at("value").get<std::string>());
        } else if (*property == Property::Attachment) {
          if (!key.contains("value") || (!key.at("value").is_null() && !key.at("value").is_string())) c.error(at(kp, "value"), "ANIM_KEY_VALUE", "첨부 키 값은 이름 또는 null입니다.");
          else if (key.at("value").is_string() && !knownName(targetId, key.at("value").get<std::string>()))
            c.error(at(kp, "value"), "ANIM_ATTACHMENT_UNKNOWN", "슬롯에 없는 첨부를 가리킵니다: " + key.at("value").get<std::string>());
          else if (key.at("value").is_string()) referenced.insert({targetId, key.at("value").get<std::string>()});
        } else if (!key.contains("value") || !finiteNumber(key.at("value"))) c.error(at(kp, "value"), "ANIM_NONFINITE", "키 값은 유한한 숫자여야 합니다.");
        else if (*property == Property::Alpha && (key.at("value").get<double>() < 0 || key.at("value").get<double>() > 1)) c.error(at(kp, "value"), "ANIM_ALPHA_RANGE", "투명도는 0..1이어야 합니다.");
      }
    }
    const auto& events = arrayOr(clip, "events");
    for (size_t ei = 0; ei < events.size(); ei++) {
      const std::string ep = at(at(p, "events"), ei);
      if (!events[ei].contains("timeMs") || !events[ei].at("timeMs").is_number_integer() || events[ei].at("timeMs").get<int64_t>() < 0 || (duration > 0 && events[ei].at("timeMs").get<int64_t>() > duration))
        c.error(at(ep, "timeMs"), "ANIM_KEY_TIME_RANGE", "이벤트 시간이 클립 범위를 벗어났습니다.");
      if (text(events[ei], "name").empty()) c.error(at(ep, "name"), "ANIM_ID_MISSING", "이벤트 이름이 필요합니다.");
    }
  }
  // A skin that cannot resolve a shown name (neither it nor the default list has it) draws
  // nothing there while active: allowed (a deliberately bare part), but worth a warning.
  for (size_t i = 0; i < skins.size(); i++)
    for (const auto& [slot, name] : referenced)
      if (!attachmentIds.count({slot, name}) && !skinNames[i].count({slot, name}))
        c.warn(at("/skins", i), "ANIM_SKIN_MISSING_ATTACHMENT", "스킨 「" + text(skins[i], "id") + "」에 쓰이는 첨부가 없습니다(그 자리는 비어 보입니다): " + slot + "/" + name);
  if (data.contains("mix") && data.at("mix").is_object()) {
    const auto& mix = data.at("mix");
    if (mix.contains("defaultMs") && (!mix.at("defaultMs").is_number_integer() || mix.at("defaultMs").get<int64_t>() < 0 || mix.at("defaultMs").get<int64_t>() > 10000))
      c.error("/mix/defaultMs", "ANIM_MIX", "기본 전환 시간은 0..10000ms입니다.");
    const auto& pairs = arrayOr(mix, "pairs");
    for (size_t i = 0; i < pairs.size(); i++) {
      const std::string pp = at("/mix/pairs", i);
      for (const char* k : {"from", "to"}) if (!clipIds.count(text(pairs[i], k))) c.error(at(pp, k), "ANIM_CLIP_UNKNOWN", "전환 표가 없는 클립을 참조합니다: " + text(pairs[i], k));
      if (!pairs[i].contains("durationMs") || !pairs[i].at("durationMs").is_number_integer() || pairs[i].at("durationMs").get<int64_t>() < 0 || pairs[i].at("durationMs").get<int64_t>() > 10000)
        c.error(at(pp, "durationMs"), "ANIM_MIX", "전환 시간은 0..10000ms입니다.");
    }
  }
  return c.out;
}

Rig parseRig(const std::string& id, const Json& data) {
  for (const auto& d : validateRig(data))
    if (d.severity == "error") throw std::runtime_error(d.code + " " + d.pointer + ": " + d.message);
  Rig rig;
  rig.id = id;
  if (data.contains("canvas")) {
    const auto& cv = data.at("canvas");
    rig.width = cv.value("width", 0.0); rig.height = cv.value("height", 0.0);
    rig.originX = cv.value("originX", 0.0); rig.originY = cv.value("originY", 0.0);
  }
  for (const auto& b : data.at("bones")) {
    Bone bone; bone.id = b.at("id"); bone.setup = readTransform(b);
    if (b.contains("parent") && b.at("parent").is_string()) bone.parent = b.at("parent");
    rig.bones.push_back(bone);
  }
  for (auto& b : rig.bones) b.parentIndex = b.parent.empty() ? -1 : rig.boneIndex(b.parent);
  // Parents-first evaluation order; validation guarantees no cycles.
  std::vector<char> placed(rig.bones.size(), 0);
  while (rig.evalOrder.size() < rig.bones.size())
    for (size_t i = 0; i < rig.bones.size(); i++)
      if (!placed[i] && (rig.bones[i].parentIndex < 0 || placed[rig.bones[i].parentIndex])) { placed[i] = 1; rig.evalOrder.push_back(int(i)); }
  for (const auto& s : arrayOr(data, "slots")) {
    Slot slot; slot.id = s.at("id"); slot.bone = s.at("bone"); slot.alpha = s.value("alpha", 1.0);
    if (s.contains("attachment") && s.at("attachment").is_string()) slot.attachment = s.at("attachment").get<std::string>();
    slot.boneIndex = rig.boneIndex(slot.bone);
    rig.slots.push_back(slot);
  }
  auto readAttachment = [](const Json& a) {
    Attachment att; att.slot = a.at("slot"); att.name = a.at("name"); att.imageAssetId = a.at("imageAssetId");
    att.offset = readTransform(a); att.pivotX = a.value("pivotX", 0.5); att.pivotY = a.value("pivotY", 0.5);
    att.width = a.at("width"); att.height = a.at("height");
    return att;
  };
  for (const auto& a : arrayOr(data, "attachments")) rig.attachments.push_back(readAttachment(a));
  for (const auto& s : arrayOr(data, "skins")) {
    Skin skin; skin.id = s.at("id"); skin.name = s.value("name", skin.id);
    for (const auto& a : arrayOr(s, "attachments")) skin.attachments.push_back(readAttachment(a));
    rig.skins.push_back(skin);
  }
  for (const auto& e : arrayOr(data, "expressions")) {
    Expression x; x.id = e.at("id"); x.name = e.value("name", x.id);
    if (e.contains("slots") && e.at("slots").is_object())
      for (auto it = e.at("slots").begin(); it != e.at("slots").end(); ++it)
        x.slots.push_back({rig.slotIndex(it.key()), it.value().is_string() ? std::optional<std::string>(it.value().get<std::string>()) : std::nullopt});
    if (e.contains("bones") && e.at("bones").is_object())
      for (auto it = e.at("bones").begin(); it != e.at("bones").end(); ++it) {
        BoneOverride o; o.bone = rig.boneIndex(it.key());
        auto get = [&](const char* k) { return it.value().contains(k) ? std::optional<double>(it.value().at(k).get<double>()) : std::nullopt; };
        o.x = get("x"); o.y = get("y"); o.rotation = get("rotation"); o.scaleX = get("scaleX"); o.scaleY = get("scaleY");
        x.bones.push_back(o);
      }
    rig.expressions.push_back(x);
  }
  for (const auto& c : arrayOr(data, "clips")) {
    Clip clip; clip.id = c.at("id"); clip.name = c.value("name", clip.id); clip.durationMs = c.at("durationMs"); clip.loop = c.value("loop", true);
    for (const auto& t : arrayOr(c, "tracks")) {
      Track track; const std::string target = t.at("target");
      track.rig = target == "rig";
      track.bone = target.rfind("bone:", 0) == 0; track.target = track.rig ? "" : target.substr(5);
      track.targetIndex = track.rig ? -1 : track.bone ? rig.boneIndex(track.target) : rig.slotIndex(track.target);
      track.property = *parseProperty(t.at("property"));
      for (const auto& k : arrayOr(t, "keys")) {
        Key key; key.timeMs = k.at("timeMs");
        if (track.property == Property::Attachment || track.property == Property::Expression) { key.interp = Interp::Step; if (k.at("value").is_string()) key.name = k.at("value").get<std::string>(); }
        else {
          key.value = k.at("value");
          const std::string interp = text(k, "interp");
          key.interp = interp == "step" ? Interp::Step : interp == "bezier" ? Interp::Bezier : Interp::Linear;
          if (key.interp == Interp::Bezier) for (size_t i = 0; i < 4; i++) key.curve[i] = k.at("curve")[i].get<double>();
        }
        track.keys.push_back(key);
      }
      clip.tracks.push_back(track);
    }
    for (const auto& e : arrayOr(c, "events")) clip.events.push_back({e.at("timeMs").get<int64_t>(), e.at("name").get<std::string>()});
    rig.clips.push_back(clip);
  }
  if (data.contains("mix") && data.at("mix").is_object()) {
    rig.defaultMixMs = data.at("mix").value("defaultMs", int64_t(0));
    for (const auto& m : arrayOr(data.at("mix"), "pairs")) rig.mixes.push_back({m.at("from"), m.at("to"), m.at("durationMs").get<int64_t>()});
  }
  return rig;
}
}  // namespace cutout
