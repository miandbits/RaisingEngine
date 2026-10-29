#pragma once
#include "layout.hpp"
// Validates a presentation-only board destination before accepting navigation.
inline std::string planner_entry_error(const Json& content,const Json& state,const Json& entry) {
  if(!state.is_object()||(state.value("phase","")!="planning"&&state.value("phase","")!="ready")||state.value("roster",Json()).is_object())return "The planner is unavailable in this game state.";
  const auto design=content.at("project").value("visual",Json::object());
  if(!design.contains("routing"))return "The planner needs explicit screen routing.";
  Json board;
  const auto screens=design.value("screens",Json::array());
  for(const auto& screen:screens)if(screen.value("id","")==entry.value("screenId","")&&!screen.value("modal",false))
    for(const auto& element:expanded_elements(design,screen,design.value("width",1280),design.value("height",720),"planning"))if(element.value("id","")==entry.value("plannerElementId","")&&element.value("kind","")=="schedule"&&element.value("scheduleLayout","")=="activity-board")board=element;
  if(board.is_null())return "The target activity board is unavailable.";
  const auto groupId=entry.value("plannerGroupId","");const Json* group=nullptr;
  const auto groups=board.value("scheduleGroups",Json::array());
  for(const auto& value:groups)if(value.value("id","")==groupId&&!groupId.empty())group=&value;
  if(!groupId.empty()&&!group)return "The planner category is unavailable.";
  const auto activityId=entry.value("plannerActivityId","");
  if(!activityId.empty()) {
    bool found=false;for(const auto& activity:content.at("activities"))if(activity.at("id")==activityId)found=true;
    if(!found)return "The activity is unavailable in this planner category.";
    if(group){bool member=false;for(const auto& id:group->at("activityIds"))if(id==activityId)member=true;if(!member)return "The activity is unavailable in this planner category.";}
  }
  return "";
}
