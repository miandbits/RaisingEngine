  std::string direction_overlay(){
    if(game.direction_cursor.is_null())return "";const auto& c=game.direction_cursor;const auto f=direction_frame(direction_config(display_content),c);
    std::string out="<div id=\"direction-overlay\" style=\"position:absolute;left:0;top:0;width:100%;height:100%;z-index:15000;background-color:#202d40dd;\">";
    for(const auto& a:f.at("actors"))for(const auto& id:a.at("assets"))out+="<img style=\"position:absolute;left:"+number(a.at("x"))+"px;top:"+number(a.at("y"))+"px;width:"+number(a.at("width"))+"px;height:"+number(a.at("height"))+"px;opacity:"+number(a.at("opacity"))+";\" src=\""+escape(asset(id))+"\"/>";
    out+="<div style=\"position:absolute;left:24px;top:16px;color:#ffffff;font-size:24px;\">"+escape(f.value("name",""))+" · "+escape(f.value("stage",""))+" · "+escape(c.value("status",""))+" · "+std::to_string(c.value("elapsedMs",0))+"ms</div>";
    out+="<div style=\"position:absolute;left:24px;bottom:16px;width:1000px;height:62px;display:flex;\">";
    for(const auto& spec:std::vector<std::pair<std::string,std::string>>{{"directionPause",c.value("paused",false)?"계속":"일시정지"},{"directionSpeed",c.value("speed",1)==1?"4배속":"1배속"},{"directionStop","중단"},{"directionSkip","끝으로"},{"save","저장"},{"load","불러오기"},{"directionClose","닫기"}})out+="<div style=\"width:132px;margin-right:8px;\">"+button(spec.first=="save"?"directionSave":spec.first=="load"?"directionLoad":spec.first,spec.second,{{"type",spec.first}})+"</div>";
    return out+"</div></div>";
  }
