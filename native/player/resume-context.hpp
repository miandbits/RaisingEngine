#pragma once
#include "layout.hpp"
#include "reading-history.hpp"
#include <functional>

namespace raising_player {
inline bool resume_context_eligible(const Json& element) {
  static const std::set<std::string> bindings={"none","calendar","calendar-year","calendar-month","calendar-day","calendar-weekday","location","user-field","attribute-value","relationship-value","item-count"};
  return element.value("kind","")=="text"&&bindings.count(element.value("binding","none"))&&!(element.value("binding","")=="relationship-value"&&element.value("valueFormat","").find("{delta}")!=std::string::npos)&&!element.contains("dataBinding")&&element.value("selectionBindings",Json::array()).empty();
}
// This projects designer-selected current-state text. It neither replays commands
// nor reconstructs previously read narrative, and it does not alter save data.
inline Json resolve_resume_context(const Json& visual,const std::string& phase,
    const std::function<bool(const Json&)>& evaluate,
    const std::function<std::string(const Json&)>& text,
    const std::function<Json(const Json&)>& variant) {
  Json result={{"entries",Json::array()},{"utf8Bytes",0},{"omittedEntries",0},{"truncated",false}};
  if(!visual.contains("resumeContext"))return result;
  const auto& config=visual.at("resumeContext");Json screen;
  for(const auto& candidate:visual.at("screens"))if(candidate.at("id")==config.at("screenId")){screen=candidate;break;}
  if(screen.is_null())return result;
  const auto presentation_phase=phase=="ready"?"planning":phase=="activity-result"||phase=="period-end"?"result":phase;
  const auto leaves=expanded_elements(visual,screen,visual.value("width",1280),visual.value("height",720),presentation_phase);
  std::size_t used=0,omitted=0;bool shortened=false;std::set<std::string> seen;
  for(const auto& selected:config.at("elementIds")){
    const auto id=selected.get<std::string>();if(seen.size()>=64)break;if(!seen.insert(id).second)continue;
    for(const auto& raw:leaves)if(raw.value("id","")==id){
      if(!resume_context_eligible(raw))break;
      auto element=variant(raw);
      if(!resume_context_eligible(element)||element.value("hidden",false)||element.value("opacity",1.0)<=0||!element.value("_boundEnabled",true))break;
      const auto when=element.value("visibleWhen","always");if(when!="always"&&when!=presentation_phase)break;
      if(element.contains("visibleCondition")&&!evaluate(element.at("visibleCondition")))break;
      const auto value=text(element);if(!history_has_text(value))break;
      const std::size_t remaining=used+id.size()<65536?65536-used-id.size():0;
      const auto bounded=reading_history_prefix(value,remaining);
      if(bounded.text.empty()){++omitted;shortened=true;break;}
      result["entries"].push_back({{"elementId",id},{"text",bounded.text},{"truncated",bounded.truncated}});
      used+=id.size()+bounded.bytes;shortened=shortened||bounded.truncated;break;
    }
  }
  result["utf8Bytes"]=used;result["omittedEntries"]=omitted;result["truncated"]=shortened;return result;
}
}
