#pragma once
#include "json.hpp"
#include <algorithm>
#include <set>
#include <string>

namespace raising_player {
// Pure presentation projection. Only accepted canonical history is a source;
// an invalid newest assignment must never fall back to an older one.
inline nlohmann::json derive_last_committed_plan(const nlohmann::json& commands,int periods,const std::set<std::string>& known) {
  using J=nlohmann::json;
  if(!commands.is_array())return {{"status","none"}};
  for(std::size_t i=commands.size();i>0;--i){const auto& command=commands[i-1];
    if(!command.is_object()||command.value("type",J())!="setSchedule")continue;
    const auto ids=command.value("activityIds",J());
    if(periods<1||!ids.is_array()||ids.size()!=std::size_t(periods))return {{"status","unavailable"},{"reason","length"}};
    for(const auto& id:ids)if(!id.is_string()||!known.count(id.get<std::string>()))return {{"status","unavailable"},{"reason","activity"}};
    return {{"status","ready"},{"commandIndex",i-1},{"activityIds",ids}};
  }
  return {{"status","none"}};
}
inline nlohmann::json plan_reuse_draft(const nlohmann::json& source,const nlohmann::json& state,int periods,const std::set<std::string>& known) {
  using J=nlohmann::json;const auto unavailable=[](const std::string& reason)->J{return {{"status","unavailable"},{"reason",reason}};};
  if(!state.is_object()||(state.value("phase","")!="planning"&&state.value("phase","")!="ready"))return unavailable("phase");
  if(!source.is_object()||source.value("status","")=="none")return unavailable("none");
  if(source.value("status","")!="ready")return unavailable(source.value("reason","none"));
  auto ids=source.value("activityIds",J());
  if(periods<1||!ids.is_array()||ids.size()!=std::size_t(periods))return unavailable("length");
  for(const auto& id:ids)if(!id.is_string()||!known.count(id.get<std::string>()))return unavailable("activity");
  const auto period=state.value("periodIndex",J());
  if(!period.is_number_integer()||period.get<int>()<0)return unavailable("phase");
  const int first=std::min(periods,period.get<int>()%periods+(state.value("activityIntermission",false)?1:0));
  if(first>=periods)return unavailable("finished");
  const auto schedule=state.value("schedule",J());
  for(int i=0;i<first;++i){if(!schedule.is_array()||schedule.size()<=std::size_t(i)||!schedule[i].is_string()||!known.count(schedule[i].get<std::string>()))return unavailable("prefix");ids[i]=schedule[i];}
  return {{"status","ready"},{"activityIds",ids},{"firstEditableSlot",first}};
}
}
