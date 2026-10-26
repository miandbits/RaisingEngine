  std::set<std::string> reuse_activity_ids() const {
    std::set<std::string> ids;for(const auto& activity:game.content.at("activities"))ids.insert(activity.at("id").get<std::string>());return ids;
  }
  Json reuse_source() const {return raising_player::derive_last_committed_plan(game.commands,game.periods(),reuse_activity_ids());}
  Json reuse_draft() const {return raising_player::plan_reuse_draft(reuse_source(),game.state,game.periods(),reuse_activity_ids());}
  bool reuse_element_eligible(const std::string& id) const {
    if(display_content.at("project").contains("roster")||!ui_modal.empty()||progress_pending())return false;
    bool opted=false;for(const auto& screen:display_content.at("project").at("visual").at("screens"))if(screen.at("id")==screen_id&&screen.contains("readableLayout"))
      for(const auto& item:screen.at("readableLayout").at("items"))if(item.value("elementId","")==id&&(item.value("schedulePresentation","")=="compact"||activity_board_widget(id))&&item.value("planReuse","")=="last-committed")opted=true;
    if(!opted)return false;
    for(const auto& e:render_elements)if(e.value("id","")==id&&e.value("kind","")=="schedule")return planning_guide_eligible(e);
    return false;
  }
  Json reuse_token() const {
    return {{"generation",reuse_generation},{"revision",game.state.value("stateRevision",-1)},{"period",game.state.value("periodIndex",-1)},{"intermission",game.state.value("activityIntermission",false)},{"schedule",game.state.value("schedule",Json())},{"source",reuse_source()},{"periods",game.periods()},{"activities",reuse_activity_ids()},{"draft",plan_save()}};
  }
  bool reuse_review_valid() const {
    return plan_reuse_open&&!game.state.is_null()&&screen_id==reuse_owner&&reuse_element_eligible(reuse_element)&&reuse_token()==reuse_snapshot;
  }
  std::string reuse_label() const {return settings_words("마지막 확정 계획 사용","Use last confirmed plan","最後に確定した計画を使う");}
  std::string reuse_reason(const Json& candidate) const {
    const auto reason=candidate.value("reason","");
    if(reason=="none")return settings_words("아직 확정한 계획이 없습니다.","No confirmed plan yet.","確定した計画はまだありません。");
    if(reason=="finished")return settings_words("이번 단계에서 바꿀 일정이 남아 있지 않습니다.","No remaining periods to edit in this phase.","この段階で変更できる予定は残っていません。");
    if(reason=="phase")return settings_words("계획을 편집할 때 사용할 수 있습니다.","Available while editing a plan.","計画を編集するときに使えます。");
    return settings_words("마지막 계획을 이 일정에 사용할 수 없습니다.","The last plan is unavailable for this schedule.","最後の計画はこの予定には使えません。");
  }
  std::string reuse_launcher(const Json& e) {
    reuse_projection=reuse_source();const auto candidate=raising_player::plan_reuse_draft(reuse_projection,game.state,game.periods(),reuse_activity_ids());
    const bool ready=candidate.value("status","")=="ready"&&reuse_element_eligible(e.at("id"));
    std::string out=button(e.value("scheduleLayout","")=="activity-board"?e.at("id").get<std::string>()+"__board-reuse":"board-reuse",reuse_label(),{{"type","readingReuseOpen"},{"elementId",e.at("id")}},ready);
    if(!ready&&e.value("scheduleLayout","")!="activity-board")out+="<p id=\"board-reuse-notice\">"+escape(reuse_reason(candidate))+"</p>";return out;
  }
  std::string reuse_review_body() {
    std::string out="<div id=\"board-reuse-review\" class=\"reading-item\"><p><b>"+escape(settings_words("계획 검토","Review plan","計画を確認"))+"</b></p><p>"+escape(settings_words("초안만 바꿉니다.","Changes the draft only.","下書きだけを変更します。"))+"</p>";
    const std::string notice=escape(settings_words("가장 최근에 확정한 배정이며 이번 단계의 계획일 수도 있습니다. 적용하면 초안만 바뀌며 시간이나 자원은 변하지 않습니다. 실행할 때 게임의 조건과 비용이 적용됩니다.","These are the most recently confirmed assignments, possibly from this phase. Applying changes only the draft, without spending time or resources. The game's conditions and costs still apply when activities run.","最後に確定した割り当てで、今回の段階の計画かもしれません。適用しても下書きだけが変わり、時間や資源は消費しません。活動の実行時にはゲームの条件と費用が適用されます。"));
    const int first=reuse_candidate.at("firstEditableSlot");bool replaces=false;
    for(int i=first;i<game.periods();++i)if((plan_selected&(1<<i))&&plan[i]!=reuse_candidate.at("activityIds")[i])replaces=true;

    for(int i=0;i<game.periods();++i){const std::string id=reuse_candidate.at("activityIds")[i];std::string name=id;for(const auto& a:display_content.at("activities"))if(a.at("id")==id){name=a.value("label",id);break;}
      out+="<p id=\"board-reuse-slot-"+std::to_string(i)+"\"><b>"+escape(compact_period(i))+"</b><br/>"+escape(name)+(i<first?"<br/>"+escape(settings_words("이미 진행한 일정 · 유지","Already progressed · kept","進行済み・維持")):std::string())+"</p>";}
    out+="<p id=\"board-reuse-notice\">"+notice+"</p>";
    if(replaces)out+="<p>"+escape(settings_words("남은 일정의 편집한 초안을 바꿉니다.","This replaces your edited draft assignments for the remaining periods.","残りの予定について、編集した下書きの割り当てを置き換えます。"))+"</p>";
    return out+"</div>";
  }
