  std::string phase_forecast_label() const {return settings_words("일정 예상","Phase forecast","今期の見通し");}
  bool phase_forecast_enabled() const {
    for(const auto& e:render_elements)if(e.value("kind","")=="schedule"&&e.value("showPhaseForecast",false)&&visible(e))return true;
    return false;
  }
  bool phase_forecast_eligible(const std::string& id) const {
    if(game.state.is_null()||(game.state.value("phase","")!="planning"&&game.state.value("phase","")!="ready")||replay_controls||game.gallery.active||!ui_modal.empty()||!pending_purchase.is_null())return false;
    for(const auto& e:render_elements)if(e.value("id","")==id&&e.value("kind","")=="schedule"&&e.value("showPhaseForecast",false)&&visible(e)&&e.value("_boundEnabled",true)&&(!e.contains("enabledCondition")||visual_condition(e.at("enabledCondition"))))return true;
    return false;
  }
  Json phase_forecast_token() const {
    return {{"generation",reuse_generation},{"revision",game.state.value("stateRevision",-1)},{"period",game.state.value("periodIndex",-1)},{"phase",game.state.value("phase","")},{"schedule",game.state.value("schedule",Json())},{"draft",plan},{"selected",plan_selected},{"locale",locale},{"content",game.content.value("contentFingerprint","")}};
  }
  bool phase_forecast_valid() const {return phase_forecast_open&&!game.state.is_null()&&phase_forecast_owner==screen_id&&phase_forecast_eligible(phase_forecast_element)&&phase_forecast_snapshot==phase_forecast_token();}
  std::string phase_forecast_launcher(const Json& e) {
    if(!e.value("showPhaseForecast",false))return "";
    auto html=button("phase-forecast-"+e.value("id",""),phase_forecast_label(),{{"type","phaseForecastOpen"},{"elementId",e.at("id")}},phase_forecast_eligible(e.at("id")));
    html.insert(html.find('>')," class=\"phase-forecast-launcher\""+(!readable_active?" style=\"color:"+css_color(e.value("color","#203a34"))+";\"":std::string()));return html;
  }
  void open_phase_forecast(const std::string& id) {
    const auto token=phase_forecast_token();
    if(phase_forecast_cache_key!=token){
      auto draft=plan;const int first=game.state.value("periodIndex",0)%game.periods();
      for(int i=first;i<int(draft.size());++i)if(!(plan_selected&(1<<i)))draft[i]="";
      try{phase_forecast_reply=game.call({{"operation","forecastPhase"},{"state",game.state},{"activityIds",draft}});}
      catch(...){phase_forecast_reply={{"ok",false}};}
      phase_forecast_cache_key=token;++phase_forecast_requests;
    }
    phase_forecast_element=id;phase_forecast_owner=screen_id;phase_forecast_snapshot=token;phase_forecast_open=true;
  }
  std::string phase_forecast_issue(const std::string& code) const {
    if(code=="FORECAST_RANDOM")return settings_words("확률에 따라 결과가 달라집니다.","Random outcomes can change what happens next.","確率によって結果が変わります。");
    if(code=="FORECAST_NARRATIVE")return settings_words("이야기가 진행된 뒤 다시 확인하세요.","Check again after the story continues.","物語が進んだ後にもう一度確認してください。");
    if(code=="FORECAST_INTERMISSION")return settings_words("활동 사이의 선택에 따라 이후 결과가 달라집니다.","Choices between activities can change later results.","活動の間の選択によって、その後の結果が変わります。");
    if(code=="FORECAST_TIMED_ACTIVITY")return settings_words("진행 중 활동의 남은 결과는 아직 정해지지 않았습니다.","This activity has unresolved steps.","この活動には未確定の段階があります。");
    if(code=="FORECAST_EXPEDITION")return settings_words("탐험 중 선택과 전투 결과는 미리 정할 수 없습니다.","Expedition choices and battles are not determined yet.","探索中の選択や戦闘の結果はまだ決まっていません。");
    if(code=="FORECAST_ROSTER")return settings_words("여러 인물의 일정 예측은 아직 지원하지 않습니다.","Forecasts for multiple characters are not supported yet.","複数の人物の予定予測にはまだ対応していません。");
    if(code=="FORECAST_INCOMPLETE")return settings_words("남은 일정의 활동을 모두 선택하세요.","Choose an activity for every remaining period.","残りの予定すべてに活動を選んでください。");
    if(code=="INSUFFICIENT_COST")return settings_words("이 계획을 진행할 자원이 부족합니다.","There are not enough resources to continue this plan.","この計画を進めるための資源が足りません。");
    if(code=="REQUIREMENTS_NOT_MET")return settings_words("활동의 조건을 충족하지 못했습니다.","An activity requirement is not met.","活動の条件を満たしていません。");
    if(code=="COMPLETED_SLOT")return settings_words("이미 진행한 일정은 바꿀 수 없습니다.","Periods already played cannot be changed.","進行済みの予定は変更できません。");
    if(code=="UNKNOWN_ACTIVITY")return settings_words("계획에 사용할 수 없는 활동이 있습니다.","The plan contains an unavailable activity.","計画に利用できない活動が含まれています。");
    return settings_words("현재 계획의 이후 결과는 아직 확인할 수 없습니다.","Later results cannot be determined from the current plan yet.","現在の計画では、その後の結果をまだ確認できません。");
  }
  std::string phase_forecast_period(int index) const {return compact_period(std::max(0,index)%game.periods());}
  std::string phase_forecast_projection(const Json& value,bool baseline) const {
    const bool complete=value.value("complete",false);const auto rows=value.value("rows",Json::array());
    std::string title=baseline?settings_words("활동만 고려한 예상","Activity-only estimate","活動のみの見積もり"):complete?settings_words("이번 일정 종료 시","End of this phase","今期の終了時"):settings_words("확인된 변화","Known changes","確認できる変化");
    std::string out="<div id=\"engine-phase-forecast-"+std::string(baseline?"baseline":"determined")+"\" class=\"reading-item\"><p><b>"+escape(title)+"</b></p>";
    if(baseline)out+="<p>"+escape(settings_words("이야기 효과와 활동 사이의 선택, 추가 행동을 제외하고 현재 정책을 유지한다고 가정합니다. 실제 결과와 다를 수 있습니다.","Assumes no story effects, between-activity choices, or optional actions, with current policies unchanged. Actual results may differ.","物語の効果、活動の間の選択、追加の行動を除き、現在の方針を維持すると仮定します。実際の結果とは異なる場合があります。"))+"</p>";
    if(!complete)out+="<p>"+escape(baseline?settings_words("예상은 여기까지입니다. 이후 결과는 포함하지 않습니다.","The estimate stops here. Later results are not included.","見積もりはここまでです。その後の結果は含まれていません。"):settings_words("여기까지 확인된 변화만 표시합니다. 이번 일정의 최종 수치가 아닙니다.","Only the known part of the plan is shown. These are not end-of-phase values.","確認できる部分だけを表示しています。今期の最終値ではありません。"))+"</p>";
    if(!value.value("lastProjectedPeriodIndex",Json()).is_null()){
      const auto period=phase_forecast_period(value.at("lastProjectedPeriodIndex"));const bool finished=!rows.empty()&&rows.back().value("boundaryComplete",false);
      out+="<p>"+escape(finished?settings_words(period+"까지", "Through "+period,period+"まで"):settings_words(period+" 진행 중", "During "+period,period+"の途中まで"))+"</p>";
    }
    const auto signed_value=[](double n){return std::string(n>0?"+":"")+number(n);};
    bool changed=false;
    if(!rows.empty())for(const auto& row:value.value("attributes",Json::array())){
      if(row.value("delta",0.0)==0&&row.value("costs",0.0)==0&&row.value("effects",0.0)==0&&row.value("automatic",0.0)==0)continue;
      changed=true;const auto id=row.value("attributeId","");std::string label=id;for(const auto& a:display_content.at("attributes"))if(a.value("id","")==id){label=a.value("label",id);break;}
      out+="<p id=\"phase-forecast-"+std::string(baseline?"estimate-":"known-")+escape(id)+"\"><b>"+escape(label)+"</b><br/>"+number(row.value("before",0.0))+" → "+number(row.value("after",0.0))+" ("+signed_value(row.value("delta",0.0))+")<br/>";
      out+=escape(settings_words("비용","Costs","費用"))+" "+signed_value(row.value("costs",0.0))+" · "+escape(settings_words("활동 효과","Activity effects","活動の効果"))+" "+signed_value(row.value("effects",0.0))+" · "+escape(settings_words("자동 규칙","Automatic rules","自動ルール"))+" "+signed_value(row.value("automatic",0.0))+"</p>";
    }
    if(!rows.empty()&&!changed)out+="<p>"+escape(settings_words("확인된 능력·자원 변화가 없습니다.","No determined attribute changes.","確認できる能力・資源の変化はありません。"))+"</p>";
    for(const auto& issue:value.value("issues",Json::array())){
      const int period=issue.value("periodIndex",-1),slot=period<0?-1:period%game.periods();std::string context=period<0?std::string():phase_forecast_period(period);
      if(slot>=0&&slot<int(plan.size())&&(plan_selected&(1<<slot)))for(const auto& a:display_content.at("activities"))if(a.value("id","")==plan[slot]){context+=" · "+a.value("label",plan[slot]);break;}
      out+="<p>"+escape((context.empty()?std::string():context+": ")+phase_forecast_issue(issue.value("code","")))+"</p>";
    }
    return out+"</div>";
  }
  std::string phase_forecast_body() const {
    std::string out="<div id=\"engine-phase-forecast-panel\"><div id=\"engine-phase-forecast-summary\" class=\"reading-item\"><p><b>"+escape(phase_forecast_label())+"</b></p><p>"+escape(settings_words("현재 초안을 검토합니다. 시간이나 자원을 사용하지 않습니다.","Reviewing the current draft uses no time or resources.","現在の下書きを確認します。時間や資源は消費しません。"))+"</p></div>";
    if(!phase_forecast_reply.value("ok",false))return out+"<p>"+escape(settings_words("이 화면에서는 예상 결과를 확인할 수 없습니다.","A forecast is unavailable for this view.","この画面では見通しを確認できません。"))+"</p></div>";
    out+=phase_forecast_projection(phase_forecast_reply.at("forecast"),false);
    const auto& baseline=phase_forecast_reply.at("baseline");
    if(!phase_forecast_reply.at("forecast").value("complete",false)&&(!baseline.value("rows",Json::array()).empty()||baseline.value("complete",false)))out+=phase_forecast_projection(baseline,true);
    return out+"</div>";
  }
