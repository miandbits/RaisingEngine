#include "cutout/templates.hpp"
#include <cmath>
#include <set>

namespace cutout {
namespace {
const Json* find(const Json& rig, const char* list, const std::string& id) {
  if (!rig.is_object() || !rig.contains(list) || !rig.at(list).is_array()) return nullptr;
  for (const auto& x : rig.at(list)) if (x.is_object() && x.value("id", std::string()) == id) return &x;
  return nullptr;
}
double param(const Json& p, const char* k, double def) {
  if (!p.contains(k) || p.at(k).is_null()) return def;
  if (!p.at(k).is_number() || !std::isfinite(p.at(k).get<double>())) throw EditError(std::string(k) + " 값은 숫자여야 합니다.");
  return p.at(k).get<double>();
}
std::string text(const Json& p, const char* k) { return p.contains(k) && p.at(k).is_string() ? p.at(k).get<std::string>() : ""; }
}  // namespace

Json breathingClip(const Json& rig, const std::string& clipId, const Json& p) {
  if (!validId(clipId)) throw EditError("클립 ID 형식이 올바르지 않습니다: " + clipId);
  const std::string boneId = text(p, "bone"), property = p.value("property", std::string("scaleY"));
  const Json* bone = find(rig, "bones", boneId);
  if (!bone) throw EditError("숨쉬기를 적용할 본을 선택하세요.");
  if (property != "scaleY" && property != "y") throw EditError("숨쉬기 속성은 scaleY 또는 y입니다.");
  // y is screen-down, so the default rise for y is negative.
  const double amplitude = param(p, "amplitude", property == "scaleY" ? 0.03 : -3);
  const int64_t period = int64_t(std::llround(param(p, "periodMs", 3200)));
  if (period < 200 || period > 600000) throw EditError("숨쉬기 주기는 200..600000ms입니다.");
  const double base = bone->value(property, property == "scaleY" ? 1.0 : 0.0);
  // Rest -> peak -> rest with easeInOut segments: a sine-like breath in three ordinary keys
  // (fewer keys to edit than a sampled sine; the curve handles stay editable data too).
  const Curve ease = *presetCurve("easeInOut");
  const Json curve = Json::array({ease[0], ease[1], ease[2], ease[3]});
  const double peak = std::round((base + amplitude) * 1e6) / 1e6;
  Json keys = Json::array({{{"timeMs", 0}, {"value", base}, {"interp", "bezier"}, {"curve", curve}},
                           {{"timeMs", period / 2}, {"value", peak}, {"interp", "bezier"}, {"curve", curve}},
                           {{"timeMs", period}, {"value", base}}});
  return {{"id", clipId}, {"name", "숨쉬기"}, {"durationMs", period}, {"loop", true},
          {"tracks", Json::array({{{"target", "bone:" + boneId}, {"property", property}, {"keys", keys}}})}};
}

Json blinkClip(const Json& rig, const std::string& clipId, const Json& p) {
  if (!validId(clipId)) throw EditError("클립 ID 형식이 올바르지 않습니다: " + clipId);
  const std::string slotId = text(p, "slot"), open = text(p, "open"), half = text(p, "half"), closed = text(p, "closed");
  if (!find(rig, "slots", slotId)) throw EditError("눈깜빡임을 적용할 조각(슬롯)을 선택하세요.");
  std::set<std::string> names;
  if (rig.contains("attachments") && rig.at("attachments").is_array())
    for (const auto& a : rig.at("attachments")) if (a.value("slot", std::string()) == slotId) names.insert(a.value("name", std::string()));
  for (const auto& n : {open, closed}) if (n.empty() || !names.count(n)) throw EditError("눈 조각에 뜬 눈/감은 눈 첨부가 필요합니다: " + n);
  if (!half.empty() && !names.count(half)) throw EditError("반쯤 감은 눈 첨부가 조각에 없습니다: " + half);
  const int64_t interval = int64_t(std::llround(param(p, "intervalMs", 4000)));
  if (interval < 600 || interval > 600000) throw EditError("깜빡임 간격은 600..600000ms입니다.");
  // Open for most of the loop, then a ~150 ms close/open at the end (attachment keys are step).
  Json keys = Json::array({{{"timeMs", 0}, {"value", open}}});
  if (!half.empty()) {
    keys.push_back({{"timeMs", interval - 200}, {"value", half}});
    keys.push_back({{"timeMs", interval - 150}, {"value", closed}});
    keys.push_back({{"timeMs", interval - 100}, {"value", half}});
    keys.push_back({{"timeMs", interval - 50}, {"value", open}});
  } else {
    keys.push_back({{"timeMs", interval - 160}, {"value", closed}});
    keys.push_back({{"timeMs", interval - 60}, {"value", open}});
  }
  return {{"id", clipId}, {"name", "눈깜빡임"}, {"durationMs", interval}, {"loop", true},
          {"tracks", Json::array({{{"target", "slot:" + slotId}, {"property", "attachment"}, {"keys", keys}}})}};
}

Json templateClip(const std::string& kind, const Json& rig, const Json& params, const std::string& clipId) {
  if (kind == "breathing") return breathingClip(rig, clipId, params);
  if (kind == "blink") return blinkClip(rig, clipId, params);
  throw EditError("알 수 없는 템플릿입니다: " + kind);
}
}  // namespace cutout
