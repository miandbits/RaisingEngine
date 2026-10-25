  // Pending picker choices are local presentation state. Only Apply changes the
  // existing planner draft; the normal Confirm/Start actions submit commands.
  bool compact_picker_valid() const {
    if(game.state.is_null()||game.state.value("stateRevision",-1)!=compact_revision||screen_id!=compact_owner||!ui_modal.empty()||progress_pending())return false;
    const auto phase=game.state.value("phase","");if(phase!="planning"&&phase!="ready")return false;
    for(const auto& e:render_elements)if(e.value("id","")==compact_element)
      return visible(e)&&e.value("opacity",1.0)>0&&e.value("_boundEnabled",true)&&(!e.contains("enabledCondition")||visual_condition(e.at("enabledCondition")));
    return false;
  }
  std::string compact_period(int slot) const {
    const auto config=raising::dates::spec(display_content.at("project"),locale);
    return (config.custom?config.period:settings_words("일정","Period","予定"))+" "+std::to_string(slot+1);
  }
  std::string compact_schedule(const Json& e) {
    ensure_plan();if(game.state.is_null())return "";
    const auto phase=game.state.value("phase","");
    const bool editable=!progress_pending()&&(phase=="planning"||phase=="ready");
    const int first=game.state.value("periodIndex",0)%game.periods();std::string out;
    for(int i=0;i<game.periods();++i){
      std::string label=settings_words("활동 선택","Choose activity","活動を選ぶ");
      if(plan_selected&(1<<i))for(const auto& a:display_content.at("activities"))if(a.at("id")==plan[i])label=a.value("label",plan[i]);
      out+=button("board-slot-"+std::to_string(i),compact_period(i)+" · "+label,{{"type","readingPlanSlot"},{"slot",i},{"elementId",e.at("id")}},editable&&i>=first);
    }
    return out;
  }
  std::string compact_activity_picker(const Json& e) {
    std::string out="<p><b>"+escape(compact_period(compact_slot))+"</b></p>";
    auto groups=e.value("scheduleGroups",Json::array());
    if(groups.empty())for(const auto& screen:display_content.at("project").at("visual").at("screens"))for(const auto& item:screen.at("elements"))
      if(item.value("scheduleLayout","")=="menu"&&!item.value("scheduleGroups",Json::array()).empty()){groups=item.at("scheduleGroups");break;}
    if(!groups.empty()){
      out+=button("board-group-all",settings_words("모든 활동","All activities","すべての活動"),{{"type","readingPlanGroup"},{"group",""}});
      for(const auto& group:groups)out+=button("board-group-option-"+group.at("id").get<std::string>(),(compact_group==group.at("id")?"● ":"")+group.value("label",""),{{"type","readingPlanGroup"},{"group",group.at("id")}});
    }
    const Json* group=nullptr;for(const auto& g:groups)if(g.at("id")==compact_group)group=&g;
    const Json* selected=nullptr;
    for(const auto& a:display_content.at("activities")){
      const auto id=a.at("id").get<std::string>();if(id==compact_activity)selected=&a;
      if(group&&std::find(group->at("activityIds").begin(),group->at("activityIds").end(),Json(id))==group->at("activityIds").end())continue;
      out+=button("board-activity-"+id,(compact_activity==id?"● ":"")+a.value("label",id),{{"type","readingPlanActivity"},{"activityId",id}});
    }
    if(selected){
      out+="<div id=\"board-details\"><p><b>"+escape(selected->value("label",compact_activity))+"</b></p>";
      if(!selected->value("description","").empty())out+="<p>"+escape(selected->at("description").get<std::string>())+"</p>";
      out+="<p>"+activity_rules(*selected)+"</p></div>";
    }else out+="<p>"+settings_words("활동을 골라 설명과 규칙을 확인하세요.","Choose an activity to inspect its description and rules.","活動を選び、説明とルールを確認してください。")+"</p>";
    return out;
  }
