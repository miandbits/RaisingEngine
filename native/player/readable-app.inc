  struct ReadingPosition {
    std::string anchor,focus,text;
    float fraction=0,scroll=0,actions_scroll=0;
  };
  std::map<std::string,ReadingPosition> reading_positions;
  std::string reading_position_key,reading_jump,reading_focus;
  ReadingPosition current_reading_position() const {
    ReadingPosition value;if(!document)return value;
    auto* main=document->GetElementById("reading-main");if(!main)return value;
    value.scroll=main->GetScrollTop();
    if(auto* actions=document->GetElementById("reading-actions"))value.actions_scroll=actions->GetScrollTop();
    if(context&&context->GetFocusElement())value.focus=context->GetFocusElement()->GetId();
    const float top=main->GetAbsoluteOffset().y;
    Rml::ElementList anchors;main->QuerySelectorAll(anchors,"[id]");
    // Prefer the smallest containing logical element. A fraction within that
    // element survives a new line count, font size or orientation.
    float best=std::numeric_limits<float>::max();
    for(auto* e:anchors){
      if(!e->IsVisible(true))continue;
      const float y=e->GetAbsoluteOffset().y,h=e->GetBox().GetSize().y;
      if(h<=0||y+h<=top)continue;
      const float score=y<=top+.5f?h:1000000.f+y-top;
      if(score<best){best=score;value.anchor=e->GetId();value.text=e->GetInnerRML();value.fraction=(top-y)/h;}
    }
    return value;
  }
  void remember_reading_position() {
    if(!document||!active_markup().readable_active||reading_position_key.empty())return;
    if(reading_positions.size()>=128&&!reading_positions.count(reading_position_key))reading_positions.erase(reading_positions.begin());
    reading_positions[reading_position_key]=current_reading_position();
  }
  void restore_reading_position() {
    auto& m=active_markup();if(!m.readable_active){reading_position_key.clear();return;}
    const auto& state=recalling()?recollection->game.state:game.state;
    const std::string story=state.is_object()?state.value("activeNarrative",Json()).dump()+":"+std::to_string(state.value("periodIndex",0))+":"+state.value("phase",""):"title";
    reading_position_key=m.readable_screen_id+":"+m.readable_view+":"+story+":"+m.bind("story");
    const auto found=reading_positions.find(reading_position_key);
    auto* main=document->GetElementById("reading-main");
    if(found!=reading_positions.end()){
      const auto& value=found->second;
      if(!value.focus.empty())if(auto* focus=document->GetElementById(value.focus);focus&&!focus->HasAttribute("disabled"))focus->Focus();
      float scroll=value.scroll;
      if(main&&!value.anchor.empty())if(auto* anchor=document->GetElementById(value.anchor))
        scroll=main->GetScrollTop()+anchor->GetAbsoluteOffset().y-main->GetAbsoluteOffset().y+value.fraction*anchor->GetBox().GetSize().y;
      if(main)main->SetScrollTop(scroll);
      if(auto* footer=document->GetElementById("reading-actions"))footer->SetScrollTop(value.actions_scroll);
    }else if(main)main->SetScrollTop(0);
    if(!reading_jump.empty()){
      if(auto* target=document->GetElementById(reading_jump))target->ScrollIntoView(true);
      reading_jump.clear();
    }
    if(!reading_focus.empty()){if(auto* e=document->GetElementById(reading_focus))e->Focus();reading_focus.clear();}
    context->Update();
  }
  bool readable_action(const Json& action) {
    const auto type=action.value("type","");
    if(type=="planningGuideOpen") {
      for(const auto& e:markup.render_elements)if(e.value("id","")==action.value("elementId","")&&markup.planning_guide_eligible(e)){
        markup.reading_menu=markup.reading_options=markup.compact_picker=false;markup.planning_guide=true;markup.planning_guide_seen=true;markup.planning_guide_step=0;markup.planning_guide_element=e.at("id");markup.planning_guide_owner=markup.screen_id;break;
      }
      dirty=true;return true;
    }
    if(type=="planningGuideStep") {if(markup.planning_guide)markup.planning_guide_step=std::clamp(markup.planning_guide_step+action.value("delta",0),0,2);dirty=true;return true;}
    if(type=="planningGuideDismiss") {if(markup.planning_guide)acknowledge_planning_guidance();dirty=true;return true;}
    if(type=="readingMenu") {markup.planning_guide=false;markup.compact_picker=false;markup.reading_options=false;markup.reading_menu=action.value("open",false);dirty=true;return true;}
    if(type=="readingOptions") {markup.planning_guide=false;markup.compact_picker=false;markup.reading_menu=false;markup.reading_options=action.value("open",false);dirty=true;return true;}
    if(type=="readingPlanCancel") {markup.compact_picker=false;dirty=true;return true;}
    if(type=="uiBack"&&markup.reading_overlay()) {
      if(markup.planning_guide)acknowledge_planning_guidance();
      else if(markup.reading_options)markup.reading_options=false;
      else if(markup.compact_picker)markup.compact_picker=false;
      else markup.reading_menu=false;
      dirty=true;return true;
    }
    if(type.rfind("readingPlan",0)!=0)return false;
    if(game.state.is_null()||(game.state.value("phase","")!="planning"&&game.state.value("phase","")!="ready")||markup.progress_pending())return true;
    markup.ensure_plan();
    if(type=="readingPlanSlot"){
      markup.planning_guide=markup.reading_menu=markup.reading_options=false;
      markup.compact_slot=std::clamp(action.value("slot",0),game.state.value("periodIndex",0)%game.periods(),game.periods()-1);
      markup.compact_activity=(markup.plan_selected&(1<<markup.compact_slot))?markup.plan[markup.compact_slot]:"";
      markup.compact_group.clear();markup.compact_element=action.at("elementId");markup.compact_owner=markup.screen_id;markup.compact_revision=game.state.value("stateRevision",0);markup.compact_picker=true;
    }else if(markup.compact_picker){
      if(!markup.compact_picker_valid()){markup.compact_picker=false;dirty=true;return true;}
      if(type=="readingPlanGroup") {markup.compact_group=action.value("group","");reading_jump="board-group-all";}
      if(type=="readingPlanActivity")for(const auto& a:game.content.at("activities"))if(a.at("id")==action.value("activityId",Json())){markup.compact_activity=a.at("id");reading_jump="board-details";break;}
      if(type=="readingPlanApply"&&!markup.compact_activity.empty()){
        markup.plan_slot=markup.compact_slot;markup.plan[markup.plan_slot]=markup.compact_activity;markup.plan_selected|=1<<markup.plan_slot;markup.compact_picker=false;
      }
    }
    dirty=true;return true;
  }
