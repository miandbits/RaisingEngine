#pragma once
inline std::string battle_target(const std::string& action){return action=="attack"?"enemy":action=="flee"?"none":"self";}
inline Json battle_command(const Json& selection){Json command={{"type","battleAction"},{"action",selection.at("battleAction")}};if(selection.at("battleAction")=="useItem")command["itemId"]=selection.at("itemId");return command;}
inline bool validate_battle_selection(const Json& content,const Json& state,const Json& selection){
  if(!selection.is_object()||state.is_null()||state.value("phase","")!="battle"||!state.contains("systems")||state.at("systems").value("battle",Json()).is_null())return false;
  for(auto it=selection.begin();it!=selection.end();++it)if(it.key()!="stateRevision"&&it.key()!="battleAction"&&it.key()!="itemId"&&it.key()!="target")return false;
  if(!selection.contains("stateRevision")||selection.at("stateRevision")!=state.at("stateRevision")||!selection.contains("battleAction")||!selection.at("battleAction").is_string())return false;
  const auto action=selection.at("battleAction").get<std::string>();if(action!="attack"&&action!="guard"&&action!="flee"&&action!="useItem")return false;
  if(action=="useItem") {if(!selection.contains("itemId")||!selection.at("itemId").is_string())return false;bool found=false;for(const auto& item:content.at("project").at("systems").at("consumables"))if(item.at("itemId")==selection.at("itemId"))found=true;if(!found||state.at("gameplay").at("inventory").value(selection.at("itemId").get<std::string>(),0)<1)return false;}
  else if(selection.contains("itemId"))return false;
  if(selection.contains("target")&&(battle_target(action)=="none"||selection.at("target")!=battle_target(action)))return false;
  return true;
}
