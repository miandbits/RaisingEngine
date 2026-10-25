  // Match the advance handler: draft changes are confirmed before execution.
  // Roster scheduling keeps its separate assign/execute contract.
  std::string advance_label() const {
    const auto raw_phase=game.state.is_null()?"title":game.state.value("phase","");
    if((raw_phase=="planning"||raw_phase=="ready")&&game.state.value("activityIntermission",false))return settings_words("결과 확인 · 다음","Continue","次へ");
    const bool changed=raw_phase=="ready"&&!display_content.at("project").contains("roster")&&plan.size()==size_t(game.periods())&&game.state.value("schedule",Json())!=Json(plan);
    if(raw_phase=="planning"||changed)return settings_words("일정 확정","Confirm schedule","予定を確定");
    if(raw_phase=="ready")return settings_words("활동 시작","Start activity","活動を開始");
    if(raw_phase=="activity-result")return settings_words("결과 확인 · 다음","Continue","次へ");
    if(raw_phase=="narrative")return settings_words("이야기 계속","Continue story","物語を続ける");
    return settings_words("다음","Next","次へ");
  }
  // A single full-width detail region avoids independent, narrow prose scroll
  // panes. Reviewing a period changes presentation only, never the plan mask.
  std::string schedule_detail(const Json& e,int slot) const {
    const Json* activity=nullptr;
    if(slot>=0&&slot<int(plan.size()))for(const auto& a:display_content.at("activities"))if(a.at("id")==plan[slot]){activity=&a;break;}
    if(!activity)return "";
    const auto period=display_content.at("project").contains("calendar")?raising::dates::spec(display_content.at("project"),locale).period+" "+std::to_string(slot+1):settings_words("일정 "+std::to_string(slot+1),"Period "+std::to_string(slot+1),"予定 "+std::to_string(slot+1));
    std::string out="<div class=\"schedule-detail-title\"><b>"+escape(period+" · "+activity->value("label",""))+"</b></div>";
    if(e.value("showActivityDescription",false)&&!activity->value("description","").empty())out+="<div id=\"plan-description-"+std::to_string(slot)+"\" class=\"schedule-detail-text\">"+escape(activity->value("description",""))+"</div>";
    if(e.value("showActivityRules",false))out+="<div id=\"plan-rules-"+std::to_string(slot)+"\" class=\"schedule-detail-text\">"+activity_rules(*activity)+"</div>";
    const auto summary=activity_preview(*activity);
    if(!summary.empty())out+="<div id=\"plan-summary-"+std::to_string(slot)+"\" class=\"schedule-detail-text\">"+escape(summary)+"</div>";
    return out;
  }
  std::string detailed_schedule_columns(const Json& e) {
    ensure_plan();if(game.state.is_null())return "";
    for(auto& id:plan)if(id.empty())id=display_content.at("activities").at(0).value("id","");
    plan_selected=(1<<game.periods())-1;
    const int period=game.state.value("periodIndex",0)%game.periods();
    const bool editable=!progress_pending()&&(game.state.value("phase","")=="planning"||game.state.value("phase","")=="ready");
    plan_slot=std::clamp(plan_slot,0,game.periods()-1);
    // The authored rectangle includes padding and borders. Using its outer
    // width made otherwise fitting periods overflow the scroll viewport.
    const double padding=e.value("padding",display_content.at("project").at("visual").value("widgetTheme","")=="ornate"?0.0:6.0);
    const double width=std::max(1.0,e.value("width",888.0)-2*padding-2*e.value("borderWidth",0.0));
    const double height=e.value("height",400.0),cw=std::max(180.0,(width-4)/game.periods());
    std::string out="<div class=\"schedule-detail-planner\"><div id=\"plan-periods-"+escape(e.value("id",""))+"\" class=\"schedule-detail-periods\" style=\"width:100%;height:128px;overflow-x:auto;overflow-y:visible;\"><div style=\"width:"+number(cw*game.periods())+"px;\">";
    for(int i=0;i<game.periods();++i){
      const auto label=display_content.at("project").contains("calendar")?raising::dates::spec(display_content.at("project"),locale).period+" "+std::to_string(i+1):settings_words("일정 "+std::to_string(i+1),"Period "+std::to_string(i+1),"予定 "+std::to_string(i+1));
      out+="<div class=\"schedule-row\" style=\"width:"+number(cw)+"px;\">";
      auto review=button("plan-review-"+std::to_string(i),label+settings_words(" · 자세히"," · Details"," · 詳細"),{{"type","planInspect"},{"slot",i}});
      review.insert(review.find('>')," class=\"schedule-period-review"+std::string(i==plan_slot?" selected":"")+"\"");out+=review;
      out+="<select style=\"width:"+number(cw-16)+"px;\" id=\"plan-"+std::to_string(i)+"\""+(i<period||!editable?" disabled=\"disabled\"":"")+">";
      for(const auto& a:display_content.at("activities"))out+="<option id=\"plan-"+std::to_string(i)+"-option-"+escape(a.value("id",""))+"\" value=\""+escape(a.value("id",""))+"\""+(plan[i]==a.at("id")?" selected=\"selected\"":"")+">"+escape(a.value("label",""))+"</option>";
      out+="</select></div>";
    }
    out+="</div></div><div class=\"schedule-draft-note\">"+escape(settings_words("활동을 바꾸면 계획만 수정됩니다. 진행 버튼을 눌러 확정하세요.","Changes edit your draft only. Use the game's progression control to confirm.","活動の変更は下書きだけに反映されます。進行ボタンで確定してください。"))+"</div>";
    out+="<div id=\"plan-details-"+escape(e.value("id",""))+"\" class=\"schedule-detail-pane\" style=\"height:"+number(std::max(40.0,height-186))+"px;\">"+schedule_detail(e,plan_slot)+"</div></div>";
    return out;
  }
