  std::string tactics_skill,tactics_actor;
  std::string tactics_body(const std::string& panel="all"){
    const auto& definition=display_content.at("project").at("systems").at("tactics");
    const auto t=game.state.at("systems").value("tactics",Json{{"active",nullptr},{"last",nullptr},{"growth",Json::object()},{"equipment",Json::object()}});
    const auto a=t.at("active");std::string out;
    if(panel=="growth"||a.is_null()){
      out+="<h3>전투원 성장 · 장비</h3>";
      for(const auto& u:definition.at("units"))if(u.at("control")=="player"){
        const auto id=u.at("id").get<std::string>();const auto g=t.at("growth").value(id,Json{{"level",1},{"experience",0}});
        out+="<p>"+escape(u.value("name",id))+" · Lv "+std::to_string(g.value("level",1))+" · EXP "+std::to_string(g.value("experience",0))+"</p>";
        for(const auto& e:display_content.at("project").at("systems").at("equipment")){const auto slot=e.value("slot","");bool equipped=t.at("equipment").value(id,Json::object()).value(slot,"")==e.value("id","");if(equipped)out+=checked_button("party-unequip-"+id+"-"+slot,e.value("name","")+" · 장착 중 / 해제",{{"type","tacticsUnequip"},{"unitId",id},{"slot",slot}});else out+=checked_button("party-equip-"+id+"-"+e.value("id",""),e.value("name","")+" 장착",{{"type","tacticsEquip"},{"unitId",id},{"equipmentId",e.at("id")}});}
      }
      if(!t.at("last").is_null()){const auto last=t.at("last");out+="<h3>"+escape(last.value("outcome",""))+" · EXP +"+std::to_string(last.value("experience",0))+"</h3>";for(const auto& line:last.at("log"))out+="<p>"+escape(line.get<std::string>())+"</p>";}
      return out;
    }
    const auto actorId=a.at("order").at(a.at("cursor").get<int>()).get<std::string>();Json actor;
    for(const auto& u:a.at("units"))if(u.at("id")==actorId)actor=u;
    if(tactics_actor!=actorId){tactics_actor=actorId;tactics_skill.clear();}
    if(panel=="all"||panel=="units"){
      out+="<h3>웨이브 "+std::to_string(a.value("wave",0)+1)+" · 차례 "+escape(actorId)+"</h3>";
      for(const auto& u:a.at("units")){std::string name=u.value("unitId","");for(const auto& d:definition.at("units"))if(d.at("id")==u.at("unitId"))name=d.value("name",name);
        out+="<div class=\"row\">"+escape(name)+" ["+escape(u.value("faction",""))+" "+std::to_string(u.value("row",0))+":"+std::to_string(u.value("column",0))+"] · HP "+std::to_string(u.value("hp",0))+" / "+std::to_string(u.at("stats").value("maxHp",1));
        for(const auto& status:u.at("statuses"))out+=" · "+escape(status.value("id",""))+"("+std::to_string(status.value("remaining",0))+")";
        if(!u.at("casting").is_null())out+=" · 시전 "+escape(u.at("casting").value("skillId",""))+"("+std::to_string(u.at("casting").value("remaining",0))+")";out+="</div>";
      }
    }
    if(panel=="all"||panel=="actions"){
      Json skills=Json::array();for(const auto& u:definition.at("units"))if(u.at("id")==actor.at("unitId"))skills=u.at("skillIds");
      out+="<h3>스킬 → 대상 선택</h3>";
      for(const auto& id:skills)for(const auto& skill:definition.at("skills"))if(skill.at("id")==id){bool enabled=false;for(const auto& target:a.at("units"))if(game.available({{"type","tacticsAction"},{"unitId",actorId},{"skillId",id},{"targetId",target.at("id")}}))enabled=true;
        out+=button("party-skill-"+id.get<std::string>(),(tactics_skill==id?"▶ ":"")+skill.value("name","")+" · 시전 "+std::to_string(skill.value("castTurns",0)),{{"type","chooseTacticsSkill"},{"skillId",id}},enabled);
      }
      if(!tactics_skill.empty())for(const auto& target:a.at("units"))out+=checked_button("party-target-"+target.value("id",""),target.value("id","")+" · HP "+std::to_string(target.value("hp",0)),{{"type","tacticsAction"},{"unitId",actorId},{"skillId",tactics_skill},{"targetId",target.at("id")}});
      out+=checked_button("party-flee","탐험에서 철수",{{"type","tacticsFlee"}});
    }
    if(panel=="all"||panel=="log"){out+="<h3>전투 기록</h3>";for(const auto& line:a.at("log"))out+="<p>"+escape(line.get<std::string>())+"</p>";}
    return out;
  }
