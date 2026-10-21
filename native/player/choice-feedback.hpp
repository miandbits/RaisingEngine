#pragma once

// Presentation-only information, reconstructed from validated command replay on load.
// No save format change and no second application of effects.
inline void track_choice_feedback(Json& feedback,const Json& content,const Json& before,
                                  const Json& command,const Json& result) {
  const auto cursor=before.value("activeNarrative",Json());
  if(command.value("type","")=="choose"&&!cursor.is_null()) {
    feedback={{"cursor",cursor},{"choiceId",command.at("choiceId")},{"consequenceText",""},
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
     next.value("actorId","")!=owner.value("actorId",""))feedback=nullptr;
}
