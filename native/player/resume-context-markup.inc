  bool resume_context_enabled() const {
    return !replay_controls&&!game.gallery.active&&!game.state.is_null()&&display_content.at("project").at("visual").contains("resumeContext");
  }
  std::string resume_context_label() const {return settings_words("현재 상황","Current reminders","現在のメモ");}
  std::string resume_context_phase() const {
    const auto current=game.state.is_null()?"title":game.state.value("phase","planning");return current=="narrative"?phase():current;
  }
  std::string resume_context_text(const Json& element) const {
    const auto binding=element.value("binding","none");
    if(binding=="none")return element.value("text","");
    const auto& project=display_content.at("project");
    if(binding.rfind("calendar-",0)==0&&!project.contains("calendar")){
      const auto date=raising::dates::date(project,game.state.value("periodIndex",0));const auto grid=raising::dates::grid(project,date);
      if(binding=="calendar-year")return std::to_string(date.year);
      if(binding=="calendar-month")return settings_words("월","Month","月")+std::string(" ")+std::to_string(date.month);
      if(binding=="calendar-day")return std::to_string(grid.day);
      if(binding=="calendar-weekday"){
        static const char* ko[]={"일","월","화","수","목","금","토"};static const char* en[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};static const char* ja[]={"日","月","火","水","木","金","土"};
        const auto weekday=(grid.offset+grid.day-1)%7;return settings_words(ko[weekday],en[weekday],ja[weekday]);
      }
    }
    return bind(binding,element);
  }
  Json resume_context_projection() const {
    if(!resume_context_enabled())return {{"entries",Json::array()},{"utf8Bytes",0},{"omittedEntries",0},{"truncated",false}};
    return raising_player::resolve_resume_context(display_content.at("project").at("visual"),resume_context_phase(),
      [this](const Json& condition){return visual_condition(condition);},
      [this](const Json& element){return resume_context_text(element);},
      [this](const Json& element){return raising_player::presentation_variant(element,context_activity(game.state,element),[this](const Json& condition){return visual_condition(condition);});});
  }
  std::string resume_context_body() const {
    const auto words=[&](const char* ko,const char* en,const char* ja){return settings_words(ko,en,ja);};
    const auto projection=resume_context_projection();
    std::string out="<div id=\"engine-resume-context-panel\" class=\"reading-item\"><p><b>"+escape(resume_context_label())+"</b></p>";
    std::string current=bind("calendar"),place=bind("location");if(!place.empty())current+=(current.empty()?"":" · ")+place;
    const auto p=resume_context_phase();const auto phaseLabel=p=="choice"?words("선택 대기","Waiting for your choice","選択待ち"):p=="dialogue"?words("이야기","Story","物語"):p=="planning"?words("계획","Planning","計画"):p=="ready"||p=="auto-ready"?words("활동 준비","Ready to continue","続行の準備完了"):p=="activity-result"||p=="period-end"?words("활동 결과","Activity result","活動の結果"):p=="finished"?words("끝맺음","Ending","結末"):words("현재 지점","Current point","現在の地点");
    current+=(current.empty()?"":" · ")+phaseLabel;out+="<p id=\"engine-resume-context-current\" style=\"tab-index:0;\">"+escape(current)+"</p>";
    if(projection.at("entries").empty())out+="<p id=\"engine-resume-context-empty\">"+escape(words("이 지점에 표시할 안내가 없습니다.","There are no reminders to show at this point.","この地点で表示するメモはありません。"))+"</p>";
    std::size_t index=0;for(const auto& entry:projection.at("entries"))out+="<div class=\"reading-paragraph\" id=\"engine-resume-context-entry-"+std::to_string(index++)+"\">"+escape(entry.at("text").get<std::string>())+"</div>";
    if(projection.value("truncated",false))out+="<p id=\"engine-resume-context-truncated\">"+escape(words("일부 안내를 줄이거나 생략했습니다.","Some reminders were shortened or omitted.","一部のメモを短縮または省略しました。"))+"</p>";
    out+="<p>"+escape(words("현재 진행에 맞춘 안내입니다.","Notes for your current progress.","現在の進行に合わせたメモです。"))+"</p>";
    return out+"</div>";
  }
