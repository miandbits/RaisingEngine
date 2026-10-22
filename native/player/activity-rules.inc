  // Presentation of authored rules only; never executes rules or consumes RNG.
  std::string activity_rules(const Json& activity,const std::string& actor_id="") const {
    auto actor_name=[&](const std::string& id){for(const auto& c:display_content.at("characters"))if(c.value("id","")==id)return c.value("name",id);return id;};
    auto scoped=[&](const std::string& id,const Json& owner=Json::object()){
      std::string who;
      if(owner.value("kind","")=="character")who=owner.value("characterId","");
      else if(owner.value("kind","")=="actor")who=actor_id;
      else if(!actor_id.empty())for(const auto& local:display_content.at("project").value("roster",Json::object()).value("localAttributeIds",Json::array()))if(local==id)who=actor_id;
      const auto prefix=!who.empty()?actor_name(who):!actor_id.empty()?std::string("공용"):std::string();
      return (prefix.empty()?"":prefix+" · ")+attribute_name(id);
    };
    std::string costs,effects;bool other=false;
    for(const auto& cost:activity.value("costs",Json::array())){if(!costs.empty())costs+=" / ";costs+=scoped(cost.value("attributeId",""))+" "+number(cost.value("amount",0.0));}
    for(const auto& effect:activity.value("baseEffects",Json::array())){
      const auto op=effect.value("op","");const auto target=effect.value("target",Json::object());
      if((op=="add"||op=="set")&&target.value("kind","")=="attribute"){
        if(!effects.empty())effects+=" / ";effects+=scoped(target.value("attributeId",""),target.value("owner",Json::object()))+" ";
        const auto value=effect.value("value",Json::object());
        if(value.value("op","")=="literal"){const auto n=value.value("value",0.0);effects+=(op=="set"?"= ":n>=0?"+":"")+number(n);}else effects+="실행 시 계산";
      }else other=true;
    }
    std::string out="비용 · "+escape(costs.empty()?"없음":costs)+"<br/>기본 효과 · "+escape(effects.empty()?"수치 변화 없음":effects);
    if(other)out+="<br/>그 밖의 상태·관계·물품 규칙이 있습니다.";
    if(activity.contains("successCheck"))out+="<br/>성공·실패에 따른 추가 효과가 있습니다.";
    return out+"<br/>규칙 요약 · 최종 결과 예측 아님";
  }
