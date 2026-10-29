  std::string growth_markup(const Json& element) const {
    if(game.state.is_null()||game.state.value("lastActivityResult",Json()).is_null())return "";
    const auto& result=game.state.at("lastActivityResult");
    const double available=readable_active?std::max(0,window_width-64):element.value("width",600.0),font=readable_active?readable_font:element.value("fontSize",18.0);const bool columns=available>=28*font;
    const std::string before=settings_words("이전","Before","前"),after=settings_words("이후","After","後"),net=settings_words("최종 변화","Net change","差分");
    auto cells=[&](const std::string& label,const std::string& values,const std::string& delta){return "<div style=\"display:flex;width:100%;\"><div style=\"width:40%;padding-right:8px;\">"+label+"</div><div style=\"width:35%;padding-right:8px;\">"+values+"</div><div style=\"width:25%;\">"+delta+"</div></div>";};
    auto heading=[&](const Json& value,const std::string& actor){std::string activity=value.value("activityId","");for(const auto& a:display_content.at("activities"))if(a.at("id")==value.at("activityId"))activity=a.value("label",activity);return (actor.empty()?"":actor+" · ")+activity;};
    auto body=[&](const Json& value,const std::string& actor){
      const auto rows=activity_growth_rows(value);std::string out="<div class=\"growth-heading\" style=\"margin-bottom:8px;\"><span style=\"font-weight:bold;\">"+escape(heading(value,actor))+"</span>"+(columns?" · ":"</div><div style=\"margin-bottom:12px;\">")+settings_words("이번 활동","This activity","今回の活動");
      if(!value.value("success",Json()).is_null())out+=" · "+settings_words(value.at("success").value("passed",false)?"성공":"실패",value.at("success").value("passed",false)?"Success":"Failure",value.at("success").value("passed",false)?"成功":"失敗");out+="</div>";
      if(rows.empty())return out+"<div>"+settings_words("최종 수치 변화 없음","No net numeric changes","数値の差分なし")+"</div>";
      if(columns)out+="<div class=\"growth-columns\" style=\"padding-bottom:6px;\">"+cells(settings_words("항목","Attribute","項目"),before+" → "+after,net)+"</div>";
      for(const auto& row:rows){const auto& target=row.at("target");std::string label=target_name(target),owner=target.value("actorId","");if(owner.empty()&&target.value("owner",Json::object()).value("kind","")=="character")owner=target.at("owner").value("characterId","");if(!owner.empty()&&actor.empty()){std::string name=owner;for(const auto& c:display_content.at("characters"))if(c.at("id")==owner)name=c.value("name",owner);label=name+" / "+label;}const auto delta=row.at("delta").get<int64_t>();
        const auto first=std::to_string(row.at("before").get<int64_t>()),last=std::to_string(row.at("after").get<int64_t>()),change=(delta>0?"+":"")+std::to_string(delta);
        out+="<div class=\"growth-row\" style=\"width:100%;box-sizing:border-box;padding:"+std::string(columns?"3":"10")+"px 0;border-top-width:1px;border-top-color:"+css_color(element.value("borderColor",element.value("color","#355c4c")))+";\">";
        if(columns)out+=cells("<span style=\"font-weight:bold;\">"+escape(label)+"</span>",first+" → "+last,"<span style=\"font-weight:bold;\">"+change+"</span>");
        else out+="<div style=\"font-weight:bold;\">"+escape(label)+"</div><div>"+before+" "+first+" → "+after+" "+last+"</div><div>"+net+" "+change+"</div>";out+="</div>";
      }return out;
    };
    std::string out="<div data-growth-summary=\"activity\" style=\"width:100%;box-sizing:border-box;line-height:"+std::string(columns?"1.3":"1.45")+";\">";
    if(result.contains("actorResults")){for(const auto& actor:result.at("actorResults")){std::string name=actor.value("characterId","");for(const auto& c:display_content.at("characters"))if(c.at("id")==actor.at("characterId"))name=c.value("name",name);out+=body(actor.at("result"),name);}}
    else out+=body(result,"");return out+"</div>";
  }
