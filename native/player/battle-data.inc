  Json battle_collection_rows(const std::string& kind) const {
    Json rows=Json::array();const auto view=systems();if(game.state.is_null()||view.is_null())return rows;
    const auto battle=view.value("battle",Json()),last=view.value("lastBattle",Json());
    const auto selection=validate_battle_selection(game.content,game.state,game.battle_selection)?game.battle_selection:Json();
    auto add=[&](std::string id,Json values,bool enabled=true,std::string reason=""){values["id"]=id;rows.push_back({{"id",id},{"values",values},{"enabled",enabled},{"reason",reason}});};
    auto reason=[](const Json& value){return value.value("reason",Json()).is_string()?value.at("reason").get<std::string>():"";};
    if(kind=="battleActors"&&!battle.is_null()) {
      add("self",{{"name","주인공"},{"side","ally"},{"hp",battle.at("heroHp")},{"maxHp",battle.at("heroMaxHp")},{"hpText",battle.at("heroHp").dump()+" / "+battle.at("heroMaxHp").dump()},{"battleTarget","self"},{"value",battle.at("heroHp")},{"maxValue",battle.at("heroMaxHp")}});
      add("enemy",{{"name",battle.at("enemyName")},{"side","enemy"},{"hp",battle.at("enemyHp")},{"maxHp",battle.at("enemyMaxHp")},{"hpText",battle.at("enemyHp").dump()+" / "+battle.at("enemyMaxHp").dump()},{"battleTarget","enemy"},{"value",battle.at("enemyHp")},{"maxValue",battle.at("enemyMaxHp")}});
    } else if(kind=="battleActions"&&!battle.is_null())for(const auto& action:view.value("battleActions",Json::array())) {
      const bool selected=!selection.is_null()&&selection.at("battleAction")==action.at("battleAction")&&selection.value("itemId","")==action.value("itemId","");
      const auto name=action.value("name","");add(action.at("id"),{{"name",name},{"displayName",selected?"✓ "+name:name},{"battleAction",action.at("battleAction")},{"itemId",action.value("itemId","")},{"target",action.at("target")},{"selected",selected},{"enabled",action.at("enabled")},{"reason",reason(action)},{"action","selectBattleAction"}},action.value("enabled",false),reason(action));
    } else if(kind=="battleTargets"&&!battle.is_null()&&!selection.is_null())for(const auto& action:view.value("battleActions",Json::array())) {
      if(action.at("battleAction")!=selection.at("battleAction")||action.value("itemId","")!=selection.value("itemId","")||action.at("target")=="none")continue;
      const auto target=action.value("target","");const auto name=target=="self"?std::string("주인공"):battle.value("enemyName","");const bool selected=selection.value("target","")==target;
      add(target,{{"name",name},{"displayName",selected?"✓ "+name:name},{"battleTarget",target},{"selected",selected},{"enabled",action.at("enabled")},{"reason",reason(action)},{"action","selectBattleTarget"}},action.value("enabled",false),reason(action));
    } else if(kind=="battleLog") {
      const auto log=!battle.is_null()?battle.value("log",Json::array()):!last.is_null()?last.value("log",Json::array()):Json::array();
      const std::map<std::string,std::string> names={{"attack","공격"},{"guard","방어"},{"flee","도주"},{"useItem","아이템 사용"}},outcomes={{"victory","승리"},{"defeat","패배"},{"fled","도주"}};
      for(const auto& entry:log){const auto action=entry.value("action","");const auto outcome=battle.is_null()&&!last.is_null()&&last.at("turn")==entry.at("turn")?last.value("outcome",""):"";
        const auto healed=entry.value("heroHeal",0);std::string text=entry.at("turn").dump()+"턴 · "+(names.count(action)?names.at(action):action)+" · 받은 피해 "+entry.at("heroDamage").dump()+" · 준 피해 "+entry.at("enemyDamage").dump();if(healed)text+=" · 회복 "+std::to_string(healed);if(!outcome.empty())text+=" · "+outcomes.at(outcome);
        add(entry.at("id"),{{"text",text},{"turn",entry.at("turn")},{"battleAction",action},{"heroDamage",entry.at("heroDamage")},{"enemyDamage",entry.at("enemyDamage")},{"healed",healed},{"outcome",outcome}});
      }
    } else if(kind=="battleRewards"&&battle.is_null()&&!last.is_null()&&last.value("outcome","")=="victory") {
      std::vector<std::pair<std::string,int>> discarded;for(const auto& item:last.at("discardedItems"))discarded.emplace_back(item.value("itemId",""),item.value("quantity",0));
      size_t index=0;for(const auto& change:last.value("rewardChanges",Json::array())) {
        ++index;std::string name,entryKind,text;int amount=0,lost=0;
        if(change.value("kind","")=="flag") {name=change.value("flagId","");for(const auto& flag:display_content.at("flags"))if(flag.at("id")==change.at("flagId"))name=flag.value("label",name);entryKind="flag";amount=change.value("after",false)?1:0;text=name+": "+(amount?"true":"false");}
        else {const auto target=change.at("target");amount=change.value("delta",0);const auto type=target.value("kind","");entryKind=type=="inventory"?"item":type;name=timeline_target_label(display_content,target);
          if(type=="inventory")for(auto it=discarded.begin();it!=discarded.end();++it)if(it->first==target.at("itemId")){lost=it->second;discarded.erase(it);break;}
          text=name+" "+(amount>=0?"+":"")+std::to_string(amount);if(lost)text+=" (버림 "+std::to_string(lost)+")";
        }add("reward-"+std::to_string(index),{{"name",name},{"kind",entryKind},{"amount",amount},{"discarded",lost},{"text",text}});
      }
      for(const auto& item:discarded){const auto name=timeline_target_label(display_content,{{"kind","inventory"},{"itemId",item.first}});add("discarded--"+item.first,{{"name",name},{"kind","item"},{"amount",0},{"discarded",item.second},{"text",name+" +0 (버림 "+std::to_string(item.second)+")"}});}
    }
    return rows;
  }
