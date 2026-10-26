  // Root calls once after the target document resolves its actual layout.
  // Navigation is separate; this entry context only opens an owned inspection.
  bool open_activity_board_context(const std::string& elementId,const std::string& groupId,const std::string& activityId) {
    const auto* e=markup.activity_board_widget(elementId);if(!e||!markup.activity_board_eligible(elementId))return false;
    bool groupFound=groupId.empty(),activityFound=activityId.empty();for(const auto& group:e->value("scheduleGroups",Json::array()))if(group.value("id","")==groupId)groupFound=true;
    for(const auto& a:game.content.at("activities"))if(a.value("id","")==activityId)activityFound=true;if(!groupFound||!activityFound)return false;
    markup.close_activity_board();markup.activity_board_open=true;markup.activity_board_reading=markup.readable_active;markup.activity_board_owner=markup.screen_id;markup.activity_board_element=elementId;markup.activity_board_slot=markup.activity_board_display_slot();markup.activity_board_snapshot=markup.activity_board_token();markup.activity_board_group=groupId;
    markup.activity_board_activity=activityId.empty()?((markup.plan_selected&(1<<markup.activity_board_slot))?markup.plan[markup.activity_board_slot]:""):activityId;
    markup.activity_board_focus=elementId+(activityId.empty()?"__board-group-"+(groupId.empty()?std::string("all"):"option-"+groupId):"__board-details");return true;
  }
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
      int slot=markup.activity_board_slot;
      if(type=="activityBoardApply"&&!markup.activity_board_activity.empty()){
        bool found=false;for(const auto& a:game.content.at("activities"))if(a.value("id","")==markup.activity_board_activity)found=true;
        if(found){markup.plan[slot]=markup.activity_board_activity;markup.plan_selected|=1<<slot;markup.plan_slot=slot;
          if(e->value("activityBoard",Json::object()).value("advanceAfterApply",false))for(int n=1;n<=game.periods();++n){const int next=(slot+n)%game.periods();if(next>=markup.activity_board_first()&&!(markup.plan_selected&(1<<next))){markup.plan_slot=next;slot=next;break;}}
        }
      }
      markup.close_activity_board();markup.activity_board_focus=id+"__board-slot-"+std::to_string(slot);
    }
    dirty=true;return true;
  }
