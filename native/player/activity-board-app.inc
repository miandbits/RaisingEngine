  bool activity_board_action(const Json& action) {
    const auto type=action.value("type","");const bool board=type.rfind("activityBoard",0)==0;
    if(type=="uiBack"&&markup.activity_board_open){const auto id=markup.activity_board_element;const auto slot=markup.activity_board_slot;markup.close_activity_board();markup.activity_board_focus=id+"__board-slot-"+std::to_string(slot);dirty=true;return true;}
    if(!board)return false;
    if(preview()||recalling()||editor_paused)return true;
    const auto id=action.value("elementId","");
    const auto* e=markup.activity_board_widget(id);
    if(!e||action.value("screenId","")!=markup.screen_id||!markup.activity_board_eligible(id)||action.value("token",Json())!=markup.activity_board_token()||action.value("serial",uint64_t(0))!=markup.activity_board_serial){markup.close_activity_board();dirty=true;return true;}
    const bool current=markup.activity_board_valid()&&markup.activity_board_element==id;
    if(type=="activityBoardSlot"||type=="activityBoardGroup"||type=="activityBoardInspect"){
      const int slot=type=="activityBoardSlot"?action.value("slot",-1):current?markup.activity_board_slot:markup.activity_board_display_slot();
      if(slot<markup.activity_board_first()||slot>=game.periods())return true;
      if(!current||type=="activityBoardSlot"){
        markup.close_activity_board();markup.activity_board_open=true;markup.activity_board_reading=markup.readable_active;markup.activity_board_owner=markup.screen_id;markup.activity_board_element=id;markup.activity_board_slot=slot;markup.activity_board_snapshot=markup.activity_board_token();
        markup.activity_board_activity=(markup.plan_selected&(1<<slot))?markup.plan[slot]:"";
      }
      if(type=="activityBoardGroup"){
        const auto group=action.value("group","");bool found=group.empty();for(const auto& g:e->value("scheduleGroups",Json::array()))if(g.value("id","")==group)found=true;
        if(found){markup.activity_board_group=group;++markup.activity_board_serial;}
        markup.activity_board_focus=id+"__board-group-"+(group.empty()?std::string("all"):"option-"+group);
      }else if(type=="activityBoardInspect"){
        const auto activity=action.value("activityId","");for(const auto& a:game.content.at("activities"))if(a.value("id","")==activity){markup.activity_board_activity=activity;++markup.activity_board_serial;markup.activity_board_focus=id+"__board-details";break;}
      }else markup.activity_board_focus=markup.activity_board_reading?id+"__board-group-all":id+"__board-slot-"+std::to_string(slot);
    }else if(current&&(type=="activityBoardApply"||type=="activityBoardCancel")){
      const int slot=markup.activity_board_slot;
      if(type=="activityBoardApply"&&!markup.activity_board_activity.empty()){
        bool found=false;for(const auto& a:game.content.at("activities"))if(a.value("id","")==markup.activity_board_activity)found=true;
        if(found){markup.plan[slot]=markup.activity_board_activity;markup.plan_selected|=1<<slot;markup.plan_slot=slot;}
      }
      markup.close_activity_board();markup.activity_board_focus=id+"__board-slot-"+std::to_string(slot);
    }
    dirty=true;return true;
  }
