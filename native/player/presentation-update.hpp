#pragma once
#include "presentation-policy.generated.hpp"

inline Json presentation_semantics(Json visual) {
  if(!visual.is_object())return visual;
  const auto policy=Json::parse(raising_presentation_policy);
  auto erase=[](Json& object,const Json& keys){if(object.is_object())for(const auto& key:keys)object.erase(key.get<std::string>());};
  auto elements=[&](Json& list){if(!list.is_array())return;for(auto& element:list)if(element.is_object()){
    erase(element,policy.at("element"));
    if(!element.contains("binding"))for(const auto& kind:policy.at("staticTextKinds"))if(element.value("kind",Json())==kind)element.erase("text");
  }};
  erase(visual,policy.at("root"));
  if(visual.contains("defaultStyle")&&visual.at("defaultStyle").is_object()){
    erase(visual["defaultStyle"],policy.at("style"));if(visual.at("defaultStyle").empty())visual.erase("defaultStyle");
  }
  if(visual.contains("styles")&&visual.at("styles").is_array())for(auto& style:visual["styles"]){erase(style,policy.at("style"));if(style.is_object())style.erase("label");}
  if(visual.contains("screens")&&visual.at("screens").is_array())for(auto& screen:visual["screens"])if(screen.is_object()){
    screen.erase("label");if(screen.contains("elements"))elements(screen["elements"]);
  }
  if(visual.contains("components")&&visual.at("components").is_array())for(auto& component:visual["components"])if(component.is_object()){
    for(const auto* key:{"label","width","height"})component.erase(key);if(component.contains("elements"))elements(component["elements"]);
  }
  return visual;
}
inline void verify_presentation_visual(const Json& baseline,const Json& next) {
  if(!baseline.is_object()||!next.is_object()||presentation_semantics(baseline)!=presentation_semantics(next))
    throw std::runtime_error("PRESENTATION_CONTENT_CHANGED: unsupported presentation update");
}
