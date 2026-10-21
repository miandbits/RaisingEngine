  // Presentation-only planner. Forecasts run a copy through the same core command
  // contract in activities-only mode; no seed, revision, collection or save mutates.
  void ensure_plan() {
    if(game.state.is_null())return;
    const int month=game.state.value("periodIndex",0)/game.periods();
    if(plan_month!=month||plan.size()!=size_t(game.periods())) {
      plan_month=month;plan_slot=game.state.value("periodIndex",0)%game.periods();plan_group.clear();
      plan.assign(game.periods(),"");plan_selected=0;
      if(!game.state.at("schedule").is_null()){
        plan=game.state.at("schedule").get<std::vector<std::string>>();
        plan_selected=(1<<game.periods())-1;
      }
    }
  }
  Json plan_save() const {
    if(game.state.is_null()||plan_month!=game.state.value("periodIndex",0)/game.periods()||plan.size()!=size_t(game.periods()))return nullptr;
    return {{"month",plan_month},{"slot",plan_slot},{"selected",plan_selected},{"activities",plan}};
  }
  void restore_plan(const Json& draft) {
    plan.clear();plan_month=-1;plan_selected=0;plan_group.clear();plan_slot=0;
    if(!draft.is_null()){plan=draft.at("activities").get<std::vector<std::string>>();plan_month=draft.at("month");plan_slot=draft.at("slot");plan_selected=draft.at("selected");}
  }
  Json plan_forecast() const {
    Json out={{"ok",false},{"complete",plan_selected==((1<<game.periods())-1)},{"rows",Json::array()}};
    if(game.state.is_null()||plan.size()!=size_t(game.periods()))return out;
    if(!out.at("complete").get<bool>()){out["reason"]="빈 주를 골라 활동을 배정해 주세요.";return out;}
    const std::string key=game.state.dump()+Json(plan).dump();
    if(key==forecast_key)return forecast_cache;
    try {
      for(const auto& id:plan)for(const auto& a:game.content.at("activities"))if(a.at("id")==id)
        if(a.contains("successCheck")||a.contains("expeditionId"))throw std::runtime_error("확률 판정·탐험 결과는 확정해서 예측하지 않습니다.");
      auto state=game.state;state["mode"]="activities-only";
      auto command=[&](const Json& cmd){
        state=game.call({{"operation","command"},{"state",state},{"command",cmd},{"expectedRevision",state.at("stateRevision")}}).at("state");
      };
      command({{"type","setSchedule"},{"activityIds",plan}});
      for(int i=game.state.value("periodIndex",0)%game.periods();i<game.periods();++i){
        command({{"type","executeActivity"}});
        // Timed activities / preparation require additional decisions. Do not
        // invent results for them or publish a partial sum as a full forecast.
        if(state.value("phase","")!="activity-result")throw std::runtime_error("이 활동은 진행 중의 선택에 따라 달라집니다.");
        out["rows"].push_back({{"slot",i},{"attributes",state.at("attributes")}});
        command({{"type","acknowledgeResult"}});
        if(i+1<game.periods())command({{"type","completePeriodWithoutEvents"}});
      }
      out["attributes"]=state.at("attributes");out["ok"]=true;
    }catch(const std::exception& e){out["reason"]=e.what();}
    forecast_key=key;forecast_cache=out;return out;
  }
  std::string schedule_board(const Json& e) {
    ensure_plan();
    if(game.state.is_null())return "";
    const auto phase=game.state.value("phase","");
    const bool editable=!progress_pending()&&(phase=="planning"||phase=="ready");
    const int first=game.state.value("periodIndex",0)%game.periods();
    const double w=e.value("width",1184.0),h=e.value("height",510.0),left=w*.32,gap=28,right=w-left-gap;
    const double fs=e.value("fontSize",20.0);
    auto at=[](double x,double y,double w,double h){return "position:absolute;box-sizing:border-box;left:"+number(x)+"px;top:"+number(y)+"px;width:"+number(w)+"px;height:"+number(h)+"px;";};
    auto title=[&](const std::string& text,double x,double y,double width){return "<div style=\""+at(x,y,width,32)+"font-size:"+number(fs)+"px;\">"+escape(text)+"</div>";};
    auto card=[&](std::string id,std::string label,Json action,double x,double y,double cw,double ch,bool selected,bool enabled=true){
      auto html=button(id,label,action,editable&&enabled);
      html.insert(html.find('>')," style=\""+at(x,y,cw,ch)+"padding:10px 14px;margin:0;text-align:left;white-space:pre-wrap;font-size:"+number(fs)+"px;line-height:"+number(fs*1.45)+"px;color:"+css_color(e.value("color","#273F3B"))+";background-color:"+std::string(selected?"#E1EADF":"#F9F5EA")+";border-radius:3px;border-width:"+std::string(selected?"3":"1")+"px;border-color:"+std::string(selected?"#476960":"#C9BFA9")+";\"");
      return html;
    };
    auto icon_id=[&](const std::string& activity){for(const auto& icon:e.value("activityIcons",Json::array()))if(icon.value("activityId","")==activity)return icon.value("imageAssetId","");return std::string();};
    auto icon_markup=[&](const std::string& asset,double x,double y,double size){if(asset.empty())return std::string();return "<div style=\""+at(x,y,size,size)+"pointer-events:none;\">"+image({{"imageAssetId",asset},{"fit","contain"}})+"</div>";};
    auto body=[&](std::string html,const std::string& content){const auto start=html.find('>')+1,end=html.rfind("</button>");if(end!=std::string::npos)html.replace(start,end-start,content);return html;};
    std::string out="<div class=\"schedule-board\" style=\"position:relative;pointer-events:auto;width:100%;height:"+number(h)+"px;\">";
    out+=title(std::to_string(plan_slot+1)+"주에 담을 활동",0,0,left);
    out+=title(e.value("label","이번 달의 계획"),left+gap,0,right);
    out+="<div style=\""+at(0,46,left,h-46)+"overflow-y:auto;pointer-events:auto;\">";
    double ay=0;
    for(const auto& a:display_content.at("activities")){
      const auto id=a.at("id").get<std::string>();
      std::string label=a.value("label",id);
      const auto cost=cost_summary(a);
      label+="  ·  "+(cost.empty()?"비용 없음":"비용 "+cost);
      const bool chosen=plan_slot<int(plan.size())&&plan[plan_slot]==id&&(plan_selected&(1<<plan_slot));
      auto html=card("board-activity-"+id,label,{{"type","boardActivity"},{"activityId",id}},0,ay,left-16,94,chosen);
      if(chosen){
        const auto at=html.find("background-color:#E1EADF");if(at!=std::string::npos)html.replace(at,std::string("background-color:#E1EADF").size(),"background-color:#F9F5EA");
        const auto at2=html.find("border-width:3px");if(at2!=std::string::npos)html.replace(at2,std::string("border-width:3px").size(),"border-width:1px;border-left-width:5px");
      }
      const auto end=html.rfind("</button>");
      if(end!=std::string::npos)html.insert(end,"<div style=\"font-size:16px;line-height:23px;margin-top:5px;\">"+escape(effect_summary({{"effects",a.value("baseEffects",Json::array())}}))+"</div>");
      if(e.contains("activityIcons")){
        const auto art=icon_id(id);const double tx=art.empty()?14:66;
        std::string content=icon_markup(art,14,13,38);
        content+="<div style=\""+at(tx,10,left-tx-34,30)+"font-size:"+number(fs+1)+"px;\">"+escape(a.value("label",id))+"</div>";
        content+="<div style=\""+at(tx,38,left-tx-34,25)+"font-size:15px;color:#70675D;\">"+escape(cost.empty()?"비용 없음":"비용 "+cost)+"</div>";
        content+="<div style=\""+at(14,66,left-44,24)+"font-size:15px;\">"+escape(effect_summary({{"effects",a.value("baseEffects",Json::array())}}))+"</div>";
        html=body(html,content);
      }
      out+=html;ay+=104;
    }
    out+="</div>";
    // A horizontal viewport keeps every slot usable with calendars of up to 12 periods.
    const double sw=(right-12*(std::min(4,game.periods())-1))/std::min(4,game.periods());
    out+="<div style=\""+at(left+gap,46,right,130)+"overflow-x:auto;overflow-y:hidden;pointer-events:auto;\"><div style=\"position:relative;width:"+number((sw+12)*game.periods()-12)+"px;height:116px;\">";
    for(int i=0;i<game.periods();++i){
      std::string label=std::to_string(i+1)+"주\n";
      const bool assigned=plan_selected&(1<<i);std::string activity=assigned?plan[i]:"활동 선택";
      for(const auto& a:display_content.at("activities"))if(a.at("id")==activity)activity=a.value("label",activity);
      label+=activity;
      auto slot=card("board-slot-"+std::to_string(i),label,{{"type","boardSlot"},{"slot",i}},double(i)*(sw+12),0,sw,116,i==plan_slot,i>=first);
      if(e.contains("activityIcons")){
        const auto art=assigned?icon_id(plan[i]):std::string();
        slot=body(slot,"<div style=\""+at(14,12,sw-28,24)+"font-size:16px;color:#70675D;\">"+std::to_string(i+1)+"주</div>"+icon_markup(art,sw-42,10,24)+"<div style=\""+at(14,46,sw-28,60)+"font-size:"+number(fs)+"px;line-height:27px;\">"+escape(activity)+"</div>");
      }
      out+=slot;
    }
    out+="</div></div>";
    std::string description=std::to_string(plan_slot+1)+"주를 선택했습니다.\n왼쪽에서 활동을 골라 주세요.";
    if(plan_slot<int(plan.size())&&(plan_selected&(1<<plan_slot)))
      for(const auto& a:display_content.at("activities"))if(a.at("id")==plan[plan_slot])description=a.value("description","");
    out+="<div style=\""+at(left+gap,184,right-150,66)+"overflow-y:auto;pointer-events:auto;font-size:"+number(fs)+"px;line-height:"+number(fs*1.55)+"px;\">"+escape(description)+"</div>";
    if(plan_selected&(1<<plan_slot))out+=card("board-clear",std::to_string(plan_slot+1)+"주 비우기",{{"type","boardClear"}},w-136,184,136,44,false);
    const auto forecast=plan_forecast();
    const bool predicted=forecast.value("ok",false);
    const bool incomplete=!forecast.value("complete",false);
    out+=title(predicted?"현재 → 활동 후 예상":incomplete?"현재 상태 · 모든 주를 채우면 예상 표시":"활동 예상",left+gap,253,right);
    if(predicted||incomplete){
      int index=0;
      for(const auto& a:display_content.at("attributes")){
        if(e.contains("attributeIds")&&std::find(e["attributeIds"].begin(),e["attributeIds"].end(),a["id"])==e["attributeIds"].end())continue;
        const auto id=a.at("id").get<std::string>();const auto before=game.state.at("attributes").value(id,0),after=predicted?forecast.at("attributes").value(id,0):before;
        const int delta=after-before;
        bool decreaseGood=false;for(const auto& v:e.value("decreasingAttributeIds",Json::array()))if(v==id)decreaseGood=true;
        const std::string color=delta==0?"#596D62":((delta>0)!=decreaseGood?"#2F5D50":"#7A3E57");
        const double cx=left+gap+(index%3)*(right/3),cy=293+(index/3)*60;
        out+="<div style=\""+at(cx,cy,right/3-10,58)+"border-top-width:1px;border-color:#C9BFA9;font-size:18px;padding-top:5px;\">"+escape(a.value("label",id))+"  "+std::to_string(before)+(predicted?" → "+std::to_string(after)+"<div style=\"color:"+color+";font-size:20px;\"><b>"+(delta>0?"+":"")+std::to_string(delta)+"</b></div>":"")+"</div>";++index;
      }
    }else out+="<div style=\""+at(left+gap,293,right,105)+"font-size:"+number(fs)+"px;white-space:pre-wrap;overflow-y:auto;pointer-events:auto;\">"+escape(forecast.value("reason","계획을 정하면 예상 변화가 표시됩니다."))+"</div>";
    if(predicted)out+="<div style=\""+at(left+gap,418,right,52)+"font-size:16px;line-height:23px;color:#455C50;\">사건·월말 정산을 제외한 순서대로의 활동 예상입니다.<br/>실제 변화는 만남과 선택에 따라 달라집니다.</div>";
    return out+"</div>";
  }
