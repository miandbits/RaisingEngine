#pragma once
#include "../vendor/json.hpp"
#include <algorithm>
#include <functional>

namespace raising_player {
inline bool uses_presentation_variants(const nlohmann::json& value) {
  if(!value.is_structured())return false;
  if(value.is_object()&&value.contains("variants")&&value.at("variants").is_array()&&!value.at("variants").empty())return true;
  for(const auto& child:value)if(uses_presentation_variants(child))return true;
  return false;
}
inline nlohmann::json presentation_variant(const nlohmann::json& element, const std::string& activity,
    const std::function<bool(const nlohmann::json&)>& evaluate) {
  for (const auto& rule : element.value("variants", nlohmann::json::array())) {
    const auto ids = rule.value("activityIds", nlohmann::json::array());
    if (!ids.empty() && (activity.empty() || std::find(ids.begin(), ids.end(), activity) == ids.end())) continue;
    if (!evaluate(rule.at("when"))) continue;
    auto result = element;
    const auto& values = rule.at("values");
    for (const auto key : {"text", "imageAssetId", "color", "fill", "opacity", "surface", "controlStyleId"})
      if (values.contains(key)) result[key] = values.at(key);
    if (values.contains("text")) result["binding"] = "none";
    if (values.contains("imageAssetId")) result["imageSource"] = "asset";
    if (values.contains("extensionRef") && result.contains("extension")) result["extension"]["ref"] = values.at("extensionRef");
    return result;
  }
  return element;
}
}
