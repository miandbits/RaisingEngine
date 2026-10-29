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
  bool planning_guide_eligible(const Json& e) const {
    if(game.state.is_null()||progress_pending()||!ui_modal.empty())return false;
    const auto phase=game.state.value("phase","");
    return (phase=="planning"||phase=="ready")&&visible(e)&&e.value("opacity",1.0)>0&&e.value("_boundEnabled",true)&&(!e.contains("enabledCondition")||visual_condition(e.at("enabledCondition")));
  }
  Json compact_groups(const Json& e) const {
    auto groups=e.value("scheduleGroups",Json::array());
    if(e.value("scheduleLayout","")!="activity-board"&&groups.empty())for(const auto& screen:display_content.at("project").at("visual").at("screens"))for(const auto& item:expanded_elements(display_content.at("project").at("visual"),screen,width,height,"planning"))
      if(item.value("scheduleLayout","")=="menu"&&!item.value("scheduleGroups",Json::array()).empty()){groups=item.at("scheduleGroups");break;}
    return groups;
  }
  std::string planning_guide_body(const Json& layout,const std::string& prefix) {
    std::string title,body;
    if(planning_guide_step==0){
      title=settings_words("1 / 3 · 일정 고르기","1 / 3 · Choose a period","1 / 3 · 予定を選ぶ");
      const int first=game.state.value("periodIndex",0)%game.periods();
      body=settings_words("계획에는 ","Your plan has ","計画には ")+std::to_string(game.periods()-first)+(first?settings_words("개의 남은 일정이 있습니다. 예: "," remaining periods. For example: "," 個の残り予定があります。例："):settings_words("개의 일정이 있습니다. 예: "," periods. For example: "," 個の予定があります。例："))+compact_period(first)+settings_words(". 바꿀 일정을 누르면 활동 목록이 열립니다.",". Tap the period you want to change to open its activity picker.","。変更したい予定をタップすると活動一覧が開きます。");
    }else if(planning_guide_step==1){
      title=settings_words("2 / 3 · 살펴보고 적용하기","2 / 3 · Inspect, then apply","2 / 3 · 確認して適用");
      body=settings_words("활동을 눌러 설명, 비용과 효과를 살펴보세요. ","Tap an activity to inspect its description, costs and effects. ","活動をタップして説明・費用・効果を確認します。");
      for(const auto& e:render_elements)if(e.value("id","")==planning_guide_element&&!compact_groups(e).empty()){body+=settings_words("분류로 목록을 좁힐 수 있습니다. ","Categories can narrow the list. ","分類で絞り込めます。");break;}
      body+=settings_words("‘이 활동 적용’은 선택한 일정만 바꿉니다. ‘취소’는 기존 계획을 유지합니다. 아직 시간이 흐르지 않습니다.","Apply activity changes only the selected period in your draft. Cancel keeps your previous plan. Neither action advances time.","「この活動を適用」は下書きの選んだ予定だけを変更します。「キャンセル」は前の計画を保ちます。どちらも時間を進めません。");
    }else{
      title=settings_words("3 / 3 · 계획 진행하기","3 / 3 · Continue your plan","3 / 3 · 計画を進める");
      const Json* proceed=nullptr;
      // Prefer the authored primary. A not-yet-complete draft can disable the
      // button; inherited/author-defined availability still must be respected.
      auto ids=layout.value("actions",Json::array());const auto primary=layout.value("primaryAction","");
      if(!primary.empty())ids.insert(ids.begin(),primary);
      for(const auto& id:ids){for(const auto& e:render_elements){
        const auto type=e.value("action","");
        if(e.value("id","")==prefix+id.get<std::string>()&&(type=="advance"||type=="commitPlan")&&visible(e)&&e.value("opacity",1.0)>0&&e.value("_boundEnabled",true)&&(!e.contains("enabledCondition")||visual_condition(e.at("enabledCondition")))){proceed=&e;break;}
      }if(proceed)break;}
      if(proceed){
        const auto type=proceed->value("action","");std::string label=proceed->value("text","");
        if(type=="advance"&&proceed->value("autoLabel",true))label=advance_label();
        body="“"+label+"”"+(type=="commitPlan"?settings_words("은 계획을 확정하고 다음 활동을 바로 실행합니다. 먼저 계획을 완성하세요."," confirms your plan and immediately runs the next activity. Complete your plan first.","は計画を確定して次の活動をすぐ実行します。先に計画を完成させてください。"):settings_words("은 계획 중에는 일정만 확정합니다. 준비가 끝난 뒤 다시 진행하면 다음 활동이 실행됩니다. 계획을 바꾸었다면 먼저 변경된 일정을 확정합니다."," confirms the schedule while planning. Once ready, continuing again runs the next activity. If you edit the draft, continuing confirms that change first.","は計画中なら予定だけを確定します。準備完了後、もう一度進むと次の活動を実行します。下書きを変更した場合は、その変更を先に確定します。"));
        if(type=="advance"&&game.state.value("activityIntermission",false))body="“"+label+"”"+settings_words("은 현재 활동 결과를 확인하고 이어갑니다. ‘이 활동 적용’은 계획만 바꿉니다."," continues after the current activity result. Apply activity still only edits your draft.","は現在の活動結果を確認して続けます。「この活動を適用」は下書きだけを変更します。");
      }else body=settings_words("계획을 마치면 게임의 진행 버튼을 확인하세요. ‘이 활동 적용’은 계획만 바꿉니다. 계획 확정과 활동 시작 방식은 게임의 진행 버튼에 따라 달라집니다.","When your draft is ready, check this game's progression controls. Apply activity only edits the draft. The game's controls determine whether confirming the plan also starts an activity.","下書きができたら、このゲームの進行ボタンを確認してください。「この活動を適用」は下書きだけを変更します。確定時に活動も始まるかはゲームの進行ボタンによります。");
    }
    return "<div id=\"engine-planning-guide\" class=\"reading-item\"><p><b>"+escape(title)+"</b></p><p>"+escape(body)+"</p>"+(planning_guide_step==0?std::string():button("engine-planning-guide-skip",settings_words("안내 건너뛰기","Skip guide","案内をスキップ"),{{"type","planningGuideDismiss"}}))+"</div>";
  }
  #include "plan-reuse.inc"
  std::string compact_schedule(const Json& e,bool guidance=false,bool reuse=false) {
    ensure_plan();if(game.state.is_null())return "";
    const auto phase=game.state.value("phase","");
    const bool editable=!progress_pending()&&(phase=="planning"||phase=="ready");
    const int first=game.state.value("periodIndex",0)%game.periods();std::string out;
    for(int i=0;i<game.periods();++i){
      std::string label=settings_words("활동 선택","Choose activity","活動を選ぶ");
      if(plan_selected&(1<<i))for(const auto& a:display_content.at("activities"))if(a.at("id")==plan[i])label=a.value("label",plan[i]);
      out+=button("board-slot-"+std::to_string(i),compact_period(i)+" · "+label,{{"type","readingPlanSlot"},{"slot",i},{"elementId",e.at("id")}},editable&&i>=first);
    }
    if(reuse)out+=reuse_launcher(e);
    if(guidance)out+=button("board-help",settings_words("계획 방법 보기","How planning works","計画の立て方"),{{"type","planningGuideOpen"},{"elementId",e.at("id")}});
    return out;
  }
  std::string compact_activity_picker(const Json& e) {
    std::string out="<p><b>"+escape(compact_period(compact_slot))+"</b></p>";
    const auto groups=compact_groups(e);
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
