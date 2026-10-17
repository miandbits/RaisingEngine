  std::string data_binding(const Json& element) const {
    const auto target=element.value("dataBinding",Json::object());
    const auto kind=target.value("kind","");
    Json compatibility=element;compatibility.erase("dataBinding");
    compatibility["valueFormat"]=element.value("valueFormat","{label}: {value}");
    compatibility["binding"]=kind=="attribute"?"attribute-value":kind=="relationship"?"relationship-value":"item-count";
    for(const auto key:{"attributeId","characterId","relationshipId","itemId"})if(target.contains(key))compatibility[key]=target.at(key);
    if(!target.contains("owner"))return bind(compatibility.at("binding").get<std::string>(),compatibility);
    const auto owner=target.at("owner").value("characterId","");
    Json value;
    if(!game.state.is_null()) {
      const auto roster=game.state.value("roster",Json::object());
      if(kind=="attribute")value=roster.value("attributes",Json::object()).value(owner,Json::object()).value(target.value("attributeId",""),Json());
      else if(kind=="relationship")value=roster.value("relationships",Json::object()).value(owner,Json::object()).value(target.value("characterId",""),Json::object()).value(target.value("relationshipId",""),Json());
      else value=roster.value("inventory",Json::object()).value(owner,Json::object()).value(target.value("itemId",""),Json());
    }
    compatibility["valueFormat"]="{label}";
    const auto label=bind(compatibility.at("binding").get<std::string>(),compatibility);
    auto format=element.value("valueFormat","{label}: {value}");
    for(const auto& [token,text]:std::vector<std::pair<std::string,std::string>>{{"{label}",label},{"{value}",value.is_number()?value.dump():"—"}}){size_t pos=0;while((pos=format.find(token,pos))!=std::string::npos){format.replace(pos,token.size(),text);pos+=text.size();}}
    return format;
  }
