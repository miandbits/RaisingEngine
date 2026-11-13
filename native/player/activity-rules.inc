  // Presentation of authored rules only; never executes rules or consumes RNG.
  std::string activity_rules(const Json& activity,const std::string& actor_id="") const {
    auto actor_name=[&](const std::string& id){for(const auto& c:display_content.at("characters"))if(c.value("id","")==id)return c.value("name",id);return id;};
    auto scoped=[&](const std::string& id,const Json& owner=Json::object()){
      std::string who;
      if(owner.value("kind","")=="character")who=owner.value("characterId","");
      else if(owner.value("kind","")=="actor")who=actor_id;
      else if(!actor_id.empty())for(const auto& local:display_content.at("project").value("roster",Json::object()).value("localAttributeIds",Json::array()))if(local==id)who=actor_id;
      const auto prefix=!who.empty()?actor_name(who):!actor_id.empty()?settings_words("공용","Shared","共通"):std::string();
      return (prefix.empty()?"":prefix+" · ")+attribute_name(id);
    };
    std::string costs,effects;bool other=false;
    for(const auto& cost:activity.value("costs",Json::array())){if(!costs.empty())costs+=" / ";costs+=scoped(cost.value("attributeId",""))+" "+number(cost.value("amount",0.0));}
    for(const auto& effect:activity.value("baseEffects",Json::array())){
      const auto op=effect.value("op","");const auto target=effect.value("target",Json::object());
      if((op=="add"||op=="set")&&target.value("kind","")=="attribute"){
        if(!effects.empty())effects+=" / ";effects+=scoped(target.value("attributeId",""),target.value("owner",Json::object()))+" ";
        const auto value=effect.value("value",Json::object());
        if(value.value("op","")=="literal"){const auto n=value.value("value",0.0);effects+=(op=="set"?"= ":n>=0?"+":"")+number(n);}else effects+=settings_words("실행 시 계산","Calculated when run","実行時に計算");
      }else other=true;
    }
    std::string out=settings_words("비용 · ","Costs · ","費用 · ")+escape(costs.empty()?settings_words("없음","None","なし"):costs)+"<br/>"+settings_words("기본 효과 · ","Base effects · ","基本効果 · ")+escape(effects.empty()?settings_words("수치 변화 없음","No numeric change","数値変化なし"):effects);
    if(other)out+="<br/>"+settings_words("그 밖의 상태·관계·물품 규칙이 있습니다.","Other state, relationship or item rules apply.","ほかに状態・関係・アイテムのルールがあります。");
    if(activity.contains("successCheck"))out+="<br/>"+settings_words("성공·실패에 따른 추가 효과가 있습니다.","Success or failure can add effects.","成功・失敗による追加効果があります。");
    return out+"<br/>"+settings_words("규칙 요약 · 최종 결과 예측 아님","Rule summary · actual results may differ","ルールの概要 · 最終結果の予測ではありません");
  }
