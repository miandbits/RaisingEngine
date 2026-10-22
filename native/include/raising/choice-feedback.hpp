#pragma once
#include "json.hpp"
#include <set>

namespace raising {
inline const nlohmann::json* feedback_script(const nlohmann::json& content,const nlohmann::json& cursor) {
  if(cursor.is_null())return nullptr;
  for(const auto& script:content.at(cursor.value("kind","")=="ending"?"endings":"events"))if(script.at("id")==cursor.at("id"))return &script;
  return nullptr;
}
inline const nlohmann::json* feedback_node(const nlohmann::json& script,const std::string& id) {
  for(const auto& node:script.at("nodes"))if(node.at("id")==id)return &node;
  return nullptr;
}
// Preview only the unconditional effects before the next visible narrative step.
inline nlohmann::json choice_immediate_effects(const nlohmann::json& content,const nlohmann::json& cursor,const nlohmann::json& choice) {
  auto effects=choice.value("effects",nlohmann::json::array());const auto script=feedback_script(content,cursor);if(!script)return effects;
  std::set<std::string> visited;auto next=choice.value("next","");
  while(visited.insert(next).second){const auto node=feedback_node(*script,next);if(!node)break;const auto type=node->value("type","");if(type!="effect"&&type!="scene")break;
    for(const auto& effect:node->value("effects",nlohmann::json::array()))effects.push_back(effect);next=node->value("next","");}
  return effects;
}
inline bool choice_feedback_pending(const nlohmann::json& content,const nlohmann::json& state) {
  const auto cursor=state.value("activeNarrative",nlohmann::json());const auto script=feedback_script(content,cursor);if(!script)return false;
  const auto node=feedback_node(*script,cursor.value("nodeId",""));if(!node)return false;const auto type=node->value("type","");return type!="dialogue"&&type!="choice"&&type!="end";
}
// Presentation-only information reconstructed from validated command replay.
inline void track_choice_feedback(nlohmann::json& feedback,const nlohmann::json& content,const nlohmann::json& before,
                                  const nlohmann::json& command,const nlohmann::json& result) {
  using Json=nlohmann::json;
  const auto cursor=before.value("activeNarrative",Json());
  if(command.value("type","")=="choose"&&!cursor.is_null()) {
    feedback={{"cursor",cursor},{"choiceId",command.at("choiceId")},{"consequenceText",""},
              {"collecting",choice_feedback_pending(content,result.at("state"))},
              {"changes",result.at("transition").at("narrative").value("changes",Json::array())}};
    for(const auto& script:content.at(cursor.value("kind","")=="ending"?"endings":"events"))
      if(script.at("id")==cursor.at("id"))for(const auto& node:script.at("nodes"))
        if(node.at("id")==cursor.at("nodeId"))for(const auto& choice:node.value("choices",Json::array()))
          if(choice.at("id")==command.at("choiceId"))feedback["consequenceText"]=choice.value("consequenceText","");
    return;
  }
  if(feedback.is_null())return;
  const auto next=result.at("state").value("activeNarrative",Json());
  const auto owner=feedback.at("cursor");
  if(next.is_null()||next.value("kind","")!=owner.value("kind","")||next.value("id","")!=owner.value("id","")||
     next.value("actorId","")!=owner.value("actorId","")){feedback=nullptr;return;}
  if(feedback.value("collecting",false)&&command.value("type","")=="advanceNarrative"){
    for(const auto& change:result.at("transition").at("narrative").value("changes",Json::array()))feedback["changes"].push_back(change);
    feedback["collecting"]=choice_feedback_pending(content,result.at("state"));
  }
}
}
