  std::string requirement_expression(const Json& e) const {
    const auto op=e.value("op","");
    if(op=="literal")return e.at("value").dump();
    if(op=="read"){
      const auto t=e.at("target");const auto kind=t.value("kind","");
      if(kind=="attribute"){
        const auto id=t.value("attributeId","");std::string name=id;
        for(const auto& a:display_content.at("attributes"))if(a.at("id")==id)name=a.value("label",id);
        return name+" (현재 "+game.state.at("attributes").value(id,Json()).dump()+")";
      }
      if(kind=="relationship"){
        const auto character=t.value("characterId",""),id=t.value("relationshipId","");
        return character+" · "+id+" (현재 "+game.state.at("relationships").value(character,Json::object()).value(id,Json()).dump()+")";
      }
      return t.dump();
    }
    return e.dump();
  }
  std::string requirement_hint(const Json& c) const {
    const auto op=c.value("op","");
    if(op=="compare"){
      const std::map<std::string,std::string> signs={{"gte","≥"},{"gt",">"},{"lte","≤"},{"lt","<"},{"eq","="},{"neq","≠"}};
      auto it=signs.find(c.value("comparator",""));
      return requirement_expression(c.at("left"))+" "+(it==signs.end()?c.value("comparator",""):it->second)+" "+requirement_expression(c.at("right"));
    }
    if(op=="all"||op=="any"){std::string text;for(const auto& child:c.at("conditions")){if(!text.empty())text+=op=="all"?" 그리고 ":" 또는 ";text+="("+requirement_hint(child)+")";}return text;}
    if(op=="not")return "제외: "+requirement_hint(c.at("condition"));
    if(op=="flag")return c.value("flagId","")+" = "+c.value("equals",Json()).dump();
    if(op=="eventCompleted")return "사건 완료: "+c.value("eventId","");
    return "추가 진입 조건: "+c.dump();
  }
