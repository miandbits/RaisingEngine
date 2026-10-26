  struct ReadingPosition {
    std::string anchor,focus,text;
    Json geometry=nullptr,anchor_geometry=nullptr;
    float fraction=0,scroll=0,actions_scroll=0;
  };
  std::map<std::string,ReadingPosition> reading_positions;
  std::string reading_position_key,reading_jump,reading_focus;
  Json reading_geometry() const {
    const auto& m=recalling()?recollection->markup:markup;
    return {{"width",m.window_width},{"height",m.window_height},{"safeArea",m.safe_area},{"font",m.readable_font},{"locale",m.locale}};
  }
  Json reading_anchor_geometry(Rml::Element* main,Rml::Element* anchor) const {
    const auto size=anchor->GetBox().GetSize();
    return Json::array({main->GetScrollTop()+anchor->GetAbsoluteOffset().y-main->GetAbsoluteOffset().y,size.x,size.y,main->GetClientWidth(),main->GetClientHeight(),main->GetScrollHeight()});
  }
  bool reading_same_anchor_geometry(const Json& before,const Json& after) const {
    if(!before.is_array()||before.size()!=after.size())return false;
    for(size_t i=0;i<before.size();++i)if(std::abs(before[i].get<float>()-after[i].get<float>())>.5f)return false;
    return true;
  }
  ReadingPosition current_reading_position() const {
    ReadingPosition value;if(!document)return value;
    auto* main=document->GetElementById("reading-main");if(!main)return value;
    value.geometry=reading_geometry();value.scroll=main->GetScrollTop();
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
      if(score<best){best=score;value.anchor=e->GetId();value.text=e->GetInnerRML();value.fraction=(top-y)/h;value.anchor_geometry=reading_anchor_geometry(main,e);}
    }
    return value;
  }
  void remember_reading_position() {
    if(!document||!active_markup().readable_active||reading_position_key.empty())return;
    if(reading_positions.size()>=128&&!reading_positions.count(reading_position_key))reading_positions.erase(reading_positions.begin());
    reading_positions[reading_position_key]=current_reading_position();
  }
  void restore_reading_position() {
    auto& m=active_markup();if(!m.readable_active){reading_position_key.clear();if(!reading_focus.empty()){if(auto* e=document->GetElementById(reading_focus))e->Focus();reading_focus.clear();}return;}
    const auto& state=recalling()?recollection->game.state:game.state;
    const std::string story=state.is_object()?state.value("activeNarrative",Json()).dump()+":"+std::to_string(state.value("periodIndex",0))+":"+state.value("phase",""):"title";
    reading_position_key=m.readable_screen_id+":"+m.readable_view+":"+story+":"+m.bind("story");
    const auto found=reading_positions.find(reading_position_key);
    auto* main=document->GetElementById("reading-main");
    if(found!=reading_positions.end()){
      const auto& value=found->second;
      if(!value.focus.empty())if(auto* focus=document->GetElementById(value.focus);focus&&!focus->HasAttribute("disabled"))focus->Focus();
      // Focus borders can reflow before measurement. Exact offsets require the
      // same anchor bounds and preceding layout, not merely the same viewport.
      context->Update();float scroll=value.scroll;
      if(main&&!value.anchor.empty())if(auto* anchor=document->GetElementById(value.anchor))
        if(value.geometry!=reading_geometry()||value.text!=anchor->GetInnerRML()||!reading_same_anchor_geometry(value.anchor_geometry,reading_anchor_geometry(main,anchor)))scroll=main->GetScrollTop()+anchor->GetAbsoluteOffset().y-main->GetAbsoluteOffset().y+value.fraction*anchor->GetBox().GetSize().y;
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
    if(markup.phase_forecast_open){
      if(type=="phaseForecastClose"||type=="uiBack"){markup.phase_forecast_open=false;reading_focus="phase-forecast-"+markup.phase_forecast_element;dirty=true;}
      return true;
    }
    if(type=="phaseForecastOpen"){
      const auto id=action.value("elementId","");
      if(!markup.reading_overlay()&&!recalling()&&markup.phase_forecast_eligible(id)){markup.open_phase_forecast(id);reading_focus="reading-main";reading_jump="engine-phase-forecast-panel";}
      dirty=true;return true;
    }
    if(markup.plan_reuse_open){
      if(type=="readingReuseCancel"||type=="uiBack"){markup.plan_reuse_open=false;reading_focus="board-reuse";dirty=true;return true;}
      if(type=="readingReuseApply"){
        if(markup.reuse_review_valid()){
          markup.plan=markup.reuse_candidate.at("activityIds").get<std::vector<std::string>>();
          markup.plan_selected=(1<<game.periods())-1;markup.plan_slot=markup.reuse_candidate.at("firstEditableSlot");markup.plan_group.clear();
        }
        markup.plan_reuse_open=false;reading_focus="board-reuse";dirty=true;return true;
      }
      // A visible review is a presentation modal. Background actions, shortcuts
      // and delayed authored controls cannot confirm, load or alter its draft.
      return true;
    }
    if(type=="readingReuseOpen"){
      const auto id=action.value("elementId","");
      if(!markup.reading_overlay()&&!preview()&&!recalling()&&!game.gallery.active&&markup.reuse_element_eligible(id)){
        markup.ensure_plan();const auto candidate=markup.reuse_draft();
        if(candidate.value("status","")=="ready"){
          markup.reuse_candidate=candidate;markup.reuse_element=id;markup.reuse_owner=markup.screen_id;markup.reuse_snapshot=markup.reuse_token();markup.plan_reuse_open=true;
          reading_focus="board-reuse-cancel";reading_jump="board-reuse-review";
        }
      }dirty=true;return true;
    }
    if(type=="readingReuseApply"||type=="readingReuseCancel")return true;
    if(type=="readingHistory") {
      if(!markup.history_enabled()||preview()||editor_live||recalling()||game.gallery.active)return true;
      history_return_view=markup.reading_options?"options":markup.reading_menu?"menu":"game";
      history_return_focus=context&&context->GetFocusElement()?context->GetFocusElement()->GetId():"";
      markup.planning_guide=markup.compact_picker=markup.reading_menu=markup.reading_options=false;markup.reading_history_open=true;reading_focus="engine-reading-history-close";
      const auto entries=reading_history.snapshot().at("entries");reading_jump=entries.empty()?"engine-reading-history-current":"engine-reading-history-entry-"+std::to_string(entries.back().at("sequence").get<uint64_t>());
      dirty=true;return true;
    }
    if(type=="readingHistoryClose"||(type=="uiBack"&&markup.reading_history_open)) {
      markup.reading_history_open=false;markup.reading_options=history_return_view=="options";markup.reading_menu=history_return_view=="menu";reading_focus=history_return_focus;dirty=true;return true;
    }
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
