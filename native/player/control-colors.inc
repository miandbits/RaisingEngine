  // Named control styles affect colors only, never control geometry or authored
  // action buttons. Missing references deliberately retain legacy appearance.
  mutable std::string control_fallback_css;
  Json control_colors(const Json& e,bool reading=false) const {
    const auto& visual=display_content.at("project").at("visual");
    const auto id=e.value("controlStyleId",reading?visual.value("readableLayout",Json::object()).value("controlStyleId",""):"");
    Json result=Json::object();if(id.empty())return result;
    for(const auto& style:visual.value("styles",Json::array()))if(style.value("id","")==id){
      for(const auto* key:{"fill","color","borderColor","hoverFill","pressedFill","disabledColor"})if(style.contains(key))result[key]=style.at(key);break;
    }return result;
  }
  Json reading_control_colors() const {
    const auto owner=phase_forecast_open?phase_forecast_element:compact_picker?compact_element:plan_reuse_open?reuse_element:std::string();
    if(!owner.empty())for(const auto& e:render_elements)if(e.value("id","")==owner)return control_colors(e,true);
    return control_colors(Json::object(),true);
  }
  std::string control_color_css(const Json& colors,const std::string& scope,const std::string& marker="engine-control") const {
    if(colors.empty())return "";
    std::string base,rules;const auto selector=scope+" ."+marker;
    for(const auto& pair:std::vector<std::pair<std::string,std::string>>{{"fill","background-color"},{"color","color"},{"borderColor","border-color"}})if(colors.contains(pair.first))base+=pair.second+":"+css_color(colors.at(pair.first))+";";
    rules+=selector+","+selector+" selectvalue,"+selector+" selectarrow,"+selector+" selectbox{"+base+"}";
    for(const auto& pair:std::vector<std::pair<std::string,std::string>>{{"hoverFill",":hover:not([disabled])"},{"pressedFill",":active:not([disabled])"}})if(colors.contains(pair.first))rules+=selector+pair.second+","+selector+pair.second+" selectvalue,"+selector+pair.second+" selectarrow{background-color:"+css_color(colors.at(pair.first))+";}";
    if(colors.contains("disabledColor"))rules+=selector+"[disabled],"+selector+"[disabled] selectvalue,"+selector+"[disabled] selectarrow{color:"+css_color(colors.at("disabledColor"))+";}";
    // The selected/focus cues remain structural and distinct from normal paint.
    rules+=selector+".slot-selected,"+selector+".record-selected{border-color:#345c50;}"+selector+":focus{border-color:#b38b49;}";
    if(colors.contains("fill"))rules+=scope+" scrollbarvertical slidertrack,"+scope+" scrollbarhorizontal slidertrack{background-color:"+css_color(colors.at("fill"))+";}";
    if(colors.contains("borderColor"))rules+=scope+" scrollbarvertical sliderbar,"+scope+" scrollbarhorizontal sliderbar{background-color:"+css_color(colors.at("borderColor"))+";}";
    return rules;
  }
  std::string control_colors_css(const Json& elements,bool reading=false) const {
    std::string out;if(reading)out+=control_color_css(reading_control_colors(),"body","engine-reading-control");
    for(const auto& e:elements){const auto kind=e.value("kind","");if(kind=="schedule"||kind=="saveSlots"||kind=="loadSlots"||(reading&&kind=="playerSettings"))out+=control_color_css(control_colors(e,reading),"#node-"+e.value("id",""));}
    return out;
  }
  std::string colored_controls(std::string body,const Json& colors,bool global=false) const {
    if(colors.empty())return body;
    // Existing fragments contain inline fallback colors. Remove only fields
    // explicitly overridden by this palette, leaving all sizes/spacing intact.
    for(const std::string tag:{"button","select","option","input"})for(size_t at=0;(at=body.find("<"+tag+" ",at))!=std::string::npos;){
      const auto end=body.find('>',at);if(end==std::string::npos)break;auto open=body.substr(at,end-at);const auto id_at=open.find("id=\"");std::string id;
      if(id_at!=std::string::npos){const auto begin=id_at+4;id=open.substr(begin,open.find('"',begin)-begin);}
      const auto action=actions.find(id);const bool destructive=action!=actions.end()&&action->second.value("type","")=="confirmSlot"&&slot_confirmation=="save";
      bool authored=false;if(global)for(const auto& e:render_elements)if(e.value("kind","")=="button"&&e.value("id","")==id){authored=true;break;}
      if(open.find("engine-control")!=std::string::npos||open.find("engine-reading-control")!=std::string::npos||destructive||authored||(global&&id.rfind("engine-",0)!=0&&id.rfind("board-",0)!=0)){at=end+1;continue;}
      for(const auto& pair:std::vector<std::pair<std::string,std::string>>{{"fill","background-color:"},{"color","color:"},{"borderColor","border-color:"}}){
        const bool state_override=pair.first=="fill"?(colors.contains("hoverFill")||colors.contains("pressedFill")):pair.first=="color"&&colors.contains("disabledColor");
        if(!colors.contains(pair.first)&&(!state_override||id.empty()))continue;
        size_t p=0;while((p=open.find(pair.second,p))!=std::string::npos){if(p&&open[p-1]!=';'&&open[p-1]!='"'&&open[p-1]!=' '){p+=pair.second.size();continue;}const auto stop=open.find(';',p);if(stop==std::string::npos)break;
          if(!colors.contains(pair.first))control_fallback_css+="#"+id+(global?".engine-reading-control":".engine-control")+"{"+open.substr(p,stop-p+1)+"}";
          open.erase(p,stop-p+1);
        }
      }
      // When a generated reading control had inline defaults, its per-ID
      // fallback also needs equally specific state selectors.
      if(global&&!id.empty()){
        for(const auto& pair:std::vector<std::pair<std::string,std::string>>{{"hoverFill",":hover:not([disabled])"},{"pressedFill",":active:not([disabled])"}})if(colors.contains(pair.first))control_fallback_css+="#"+id+".engine-reading-control"+pair.second+"{background-color:"+css_color(colors.at(pair.first))+";}";
        if(colors.contains("disabledColor"))control_fallback_css+="#"+id+".engine-reading-control[disabled]{color:"+css_color(colors.at("disabledColor"))+";}";
      }
      const std::string name=global?"engine-reading-control":"engine-control";const auto cls=open.find("class=\"");if(cls==std::string::npos)open+=" class=\""+name+"\"";else open.insert(cls+7,name+" ");
      body.replace(at,end-at,open);at+=open.size()+1;
    }return body;
  }
