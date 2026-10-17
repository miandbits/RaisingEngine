#pragma once
#include "../vendor/json.hpp"
#include <set>
#include <functional>
#include <string>

inline nlohmann::json apply_ui_context(nlohmann::json element,const nlohmann::json& inherited=nlohmann::json::object()) {
  using J=nlohmann::json;auto context=inherited;
  const auto own=element.value("uiContext",J::object());
  for(const auto& [key,value]:own.items())context[key]=value;
  const auto bindings=element.value("contextBindings",J::array());
  if(context.empty()&&bindings.empty())return element;
  element["uiContext"]=context;std::set<std::string> fields;for(const auto& key:bindings)fields.insert(key.get<std::string>());
  std::function<void(J&)> bind=[&](J& value){
    if(value.is_array()){for(auto& child:value)bind(child);return;}if(!value.is_object())return;
    if(value.contains("owner")&&value["owner"].value("kind","")=="actor"&&context.contains("actorId"))value["owner"]={{"kind","character"},{"characterId",context.at("actorId")}};
    for(auto& [key,child]:value.items()){if(key=="owner")continue;if(fields.count(key))child=context.value(key,"");else bind(child);}
  };
  for(const auto key:{"dataBinding","visibleCondition","enabledCondition","variants"})if(element.contains(key))bind(element[key]);
  for(const auto& key:fields){element[key]=context.value(key,"");if(!context.contains(key))element["_boundEnabled"]=false;}
  if((element.value("action","")=="executeActorAction"||element.value("action","")=="assignActivity")&&!element.contains("actorId")&&context.contains("actorId"))element["actorId"]=context.at("actorId");
  return element;
}

inline std::string context_activity(const nlohmann::json& state,const nlohmann::json& element) {
  const auto context=element.value("uiContext",nlohmann::json::object());
  if(context.contains("activityId"))return context.at("activityId");
  if(!state.is_object()||state.value("phase","")!="activity-result")return "";
  const auto result=state.value("lastActivityResult",nlohmann::json::object());
  if(!context.contains("actorId"))return result.value("activityId","");
  for(const auto& row:result.value("actorResults",nlohmann::json::array()))if(row.at("characterId")==context.at("actorId"))return row.at("result").value("activityId","");
  return "";
}
