  #include "compact-planning.inc"
  #include "activity-board.inc"
  // Reading order is a presentation of the same resolved leaves and commands.
  // RmlUi measures natural text height; no character-count estimates are used.
  bool readable_requested(const Json& visual,const Json& screen) const {
    if(!visual.contains("readableLayout")||!screen.contains("readableLayout")||window_width<=0||window_height<=0)return false;
    return reading_overlay()||screen.at("readableLayout").value("activation","auto")=="always"||player_preferences.value("readingLayout","auto")=="on"||std::min(safe_area.value("width",window_width),safe_area.value("height",window_height))<=visual.at("readableLayout").value("maxShortEdge",600);
  }
  #include "reading-history-markup.inc"
  #include "resume-context-markup.inc"
  std::string readable_settings(const Json& e) {
    const std::string id=e.value("id","settings");std::string out;
    const auto words=[&](const char* ko,const char* en,const char* ja){return settings_words(ko,en,ja);};
    auto choice=[&](const std::string& suffix,const std::string& label,const std::string& key,Json value,bool enabled=true){
      return button(id+"-"+suffix,(player_preferences.at(key)==value?"● ":"")+label,{{"type","setPlayerSetting"},{"key",key},{"value",value}},enabled);
    };
    auto heading=[&](const std::string& value){out+="<p><b>"+escape(value)+"</b></p>";};
    heading(words("읽기 화면","Reading layout","読みやすい画面"));
    out+=choice("reading-auto",words("화면에 맞춰 자동","Automatic for this screen","画面に合わせて自動"),"readingLayout","auto");
    out+=choice("reading-on",words("항상 읽기 화면","Always use reading layout","常に読みやすい画面"),"readingLayout","on");
    heading(words("글자 크기","Text size","文字サイズ"));
    for(double scale:{1.0,1.25,1.5,2.0})out+=choice("text-"+std::to_string(int(scale*100)),std::to_string(int(scale*100))+"%","textScale",scale);
    if(history_enabled())out+="<div class=\"reading-item\">"+button("engine-reading-history",history_label(),{{"type","readingHistory"}})+"</div>";
    if(resume_context_enabled())out+="<div class=\"reading-item\">"+button("engine-resume-context",resume_context_label(),{{"type","resumeContext"}})+"</div>";
    if(e.value("_readingOnly",false))return out;
    for(const std::string key:{"musicVolume","effectsVolume"}){
      const int value=player_preferences.at(key);heading((key=="musicVolume"?words("음악 음량","Music volume","音楽の音量"):words("효과음 음량","Sound effects","効果音の音量"))+std::string(" · ")+std::to_string(value)+"%");
      out+=choice(key+"-down","−",key,std::max(0,value-10),value>0)+choice(key+"-up","+",key,std::min(100,value+10),value<100);
    }
#if !defined(SDL_PLATFORM_IOS) && !defined(SDL_PLATFORM_ANDROID)
    heading(words("화면 모드","Display mode","画面モード"));
    out+=choice("windowed",words("창 화면","Windowed","ウィンドウ"),"fullscreen",false)+choice("fullscreen",words("전체화면","Fullscreen","全画面"),"fullscreen",true);
#endif
    heading(words("대사 표시 속도","Dialogue speed","台詞の表示速度"));
    for(const auto& speed:std::vector<std::pair<int,std::string>>{{20,words("느리게","Slow","遅い")},{40,words("보통","Normal","普通")},{70,words("빠르게","Fast","速い")},{0,words("즉시","Instant","即時")}})out+=choice("speed-"+std::to_string(speed.first),speed.second,"textSpeed",speed.first);
    heading(words("대사 자동 진행","Automatic dialogue","台詞の自動送り"));
    out+=choice("auto-off",words("끄기","Off","オフ"),"autoAdvance",false)+choice("auto-on",words("켜기","On","オン"),"autoAdvance",true);
    const int delay=player_preferences.at("autoDelay");const bool enabled=player_preferences.at("autoAdvance");
    heading(words("다 읽은 뒤 기다리기","Wait after text","表示後の待ち時間")+std::string(" · ")+std::to_string(delay)+" s");
    out+=choice("delay-down","−","autoDelay",std::max(1,delay-1),enabled&&delay>1)+choice("delay-up","+","autoDelay",std::min(6,delay+1),enabled&&delay<6);
    out+="<p>"+escape(settings_notice.empty()?words("설정은 즉시 저장됩니다. 선택지는 직접 고릅니다.","Settings save immediately. Choices always wait for you.","設定はすぐに保存されます。選択肢では待機します。"):settings_notice)+"</p>";
    return out;
  }
  std::string readable_slots(const Json& e) {
    const bool saving=e.value("kind","")=="saveSlots";const std::string id=e.at("id");
    auto control=[&](const std::string& suffix,const std::string& label,Json action,bool enabled=true){return button(id+"-"+suffix,label,action,enabled);};
    const auto selected=std::find_if(save_slots.begin(),save_slots.end(),[&](const Json& r){return r.at("automatic")==slot_auto&&r.at("index")==slot_selected;});
    const auto selected_identity=[&](){if(selected==save_slots.end())return std::string();return "<div id=\""+escape(id)+"-selected\"><p><b>"+escape(slot_identity(*selected))+"</b></p><p class=\"record-saved\">"+escape(slot_saved_at(*selected))+"</p></div>";};
    if(!slot_confirmation.empty()){
      std::string out="<p><b>"+slot_words("이 기록으로 진행할까요?","Use this record?","この記録を使いますか？")+"</b></p>";
      out+=selected_identity();
      out+="<p>"+std::string(slot_confirmation=="save"?slot_words("선택한 기록이 지금의 진행으로 바뀝니다.","This record will be replaced by your current progress.","この記録を現在の進行で上書きします。"):slot_words("저장하지 않은 진행은 사라집니다.","Unsaved progress will be lost.","保存していない進行は失われます。"))+"</p>";
      out+=control("cancel",slot_words("취소 · 기록 목록으로","Cancel · back to records","キャンセル · 記録へ"),{{"type","cancelSlot"}});
      out+=control("confirm",(slot_confirmation=="save"?slot_words("덮어쓰기 · ","Overwrite · ","上書き · "):slot_words("불러오기 · ","Load · ","読み込む · "))+(selected==save_slots.end()?std::string():slot_label(*selected)),{{"type","confirmSlot"}});return out;
    }
    auto group=[&](const std::string& suffix,const std::string& label,bool automatic){
      auto item=control(suffix,label,{{"type","slotGroup"},{"automatic",automatic}});
      item.insert(item.find('>')," class=\"record-group"+std::string(slot_auto==automatic?" record-selected":"")+"\" aria-pressed=\""+(slot_auto==automatic?"true":"false")+"\"");return item;
    };
    std::string out="<div class=\"record-groups\">"+group("manual",slot_words("수동","Manual","手動"),false)+group("auto",slot_words("자동","Auto","自動"),true)+"</div>";
    std::vector<Json> rows;for(const auto& r:save_slots)if(r.at("automatic")==slot_auto)rows.push_back(r);
    if(slot_auto)std::stable_sort(rows.begin(),rows.end(),[](const Json& a,const Json& b){return a.value("record",Json::object()).value("sequence",int64_t(0))>b.value("record",Json::object()).value("sequence",int64_t(0));});
    // All records remain in one vertical list; scrolling replaces desktop paging.
    for(const auto& r:rows){const int i=r.at("index");const auto label=(slot_auto?slot_words("자동 ","Auto ","自動 "):slot_words("기록 ","Record ","記録 "))+std::to_string(r.value("displayNumber",i+1));
      auto card=control("slot-"+std::to_string(i),slot_identity(r),{{"type","selectSaveSlot"},{"index",i},{"automatic",slot_auto}});
      card.insert(card.find('>')," class=\"record-slot"+std::string(i==slot_selected?" record-selected":"")+"\" aria-pressed=\""+(i==slot_selected?"true":"false")+"\"");
      const auto begin=card.find('>')+1,end=card.rfind("</button>");
      std::string detail="<span class=\"record-identity\">"+escape(slot_identity(r))+"</span>";
      if(!slot_saved_at(r).empty())detail+="<span class=\"record-saved\">"+escape(slot_saved_at(r))+"</span>";
      const auto thumb=r.value("thumbnail","");if(!thumb.empty())detail+="<img class=\"record-thumbnail\" src=\""+escape(thumb)+"\"/>";
      card.replace(begin,end-begin,detail);out+=card;
    }
    const bool enabled=selected!=save_slots.end()&&(saving?(!slot_auto&&!game.state.is_null()):selected->value("status","")=="ready");
    out+=selected_identity();
    out+=control("request",saving?slot_words("선택한 곳에 저장","Save selected record","選んだ記録に保存"):slot_words("선택한 기록 이어 읽기","Continue selected record","選んだ記録から続ける"),{{"type",saving?"requestSlotSave":"requestSlotLoad"}},enabled);
    if(portable_available)out+="<div class=\"reading-item\">"+portable_tools(e)+"</div>";return out;
  }
  std::string readable_schedule(const Json& e) {
    if(display_content.at("project").contains("roster")){
      if(game.state.is_null())return "";
      const auto phase=game.state.value("phase","");const bool editable=!progress_pending()&&(phase=="planning"||phase=="ready");
      if(roster_revision!=game.state.value("stateRevision",0)){roster_draft=game.state.at("roster").at("assignments");roster_revision=game.state.value("stateRevision",0);}
      std::string out;
      for(const auto& member:display_content.at("project").at("roster").at("members")){
        const std::string actor=member.at("characterId");std::string name=actor;for(const auto& c:display_content.at("characters"))if(c.at("id")==actor)name=c.value("name",actor);
        out+="<p><b>"+escape(name)+"</b></p>";
        for(const auto& activity:display_content.at("activities"))if(!activity.contains("expeditionId")){
          const std::string id=activity.at("id");std::string label=(roster_draft.value(actor,"")==id?"● ":"")+activity.value("label",id);
          if(e.value("showActivityDescription",false))label+="\n"+activity.value("description","");
          out+=button("reading-roster-"+actor+"-"+id,label,{{"type","readingRosterActivity"},{"actorId",actor},{"activityId",id}},editable);
        }
      }
      out+=button("roster-assign",settings_words("활동 배정 적용","Apply assignments","活動を割り当てる"),{{"type","rosterCommit"}},editable);
      out+=button("roster-execute",settings_words("배정한 활동 함께 실행","Run assigned activities","割り当てた活動を実行"),{{"type","executeActivity"}},editable&&phase=="ready"&&roster_draft==game.state.at("roster").at("assignments"));return out;
    }
    ensure_plan();if(game.state.is_null())return "";
    const bool editable=!progress_pending()&&(game.state.value("phase","")=="planning"||game.state.value("phase","")=="ready");
    const int first=game.state.value("periodIndex",0)%game.periods();std::string out;
    const auto config=raising::dates::spec(display_content.at("project"),locale);
    for(int i=0;i<game.periods();++i){std::string label=(config.custom?config.period:settings_words("일정","Period","予定"))+" "+std::to_string(i+1)+" · ";
      std::string activity=settings_words("활동 선택","Choose activity","活動を選ぶ");if(plan_selected&(1<<i))for(const auto& a:display_content.at("activities"))if(a.at("id")==plan[i])activity=a.value("label",plan[i]);
      out+=button("board-slot-"+std::to_string(i),(i==plan_slot?"● ":"")+label+activity,{{"type","boardSlot"},{"slot",i}},editable&&i>=first);
    }
    out+="<p><b>"+escape((config.custom?config.period:settings_words("일정","Period","予定"))+" "+std::to_string(plan_slot+1))+"</b></p>";
    for(const auto& a:display_content.at("activities")){
      const std::string id=a.at("id");std::string label=a.value("label",id),summary=activity_preview(a);
      if(!summary.empty())label+="\n"+summary;if(e.value("showActivityDescription",false))label+="\n"+a.value("description","");
      auto row=button("board-activity-"+id,label,{{"type","boardActivity"},{"activityId",id}},editable);
      if(e.value("showActivityRules",false))row.insert(row.rfind("</button>"),"<div>"+activity_rules(a)+"</div>");out+=row;
    }
    if(plan_selected&(1<<plan_slot))out+=button("board-clear",settings_words("이 일정 비우기","Clear this period","この予定を空ける"),{{"type","boardClear"}},editable);
    return out;
  }
  std::string readable_text(const std::string& id,const std::string& text,size_t visible_bytes=std::string::npos) const {
    if(text.empty())return "";
    std::string out;size_t start=0,index=0;
    while(start<text.size()){
      const auto end=text.find("\n\n",start),length=(end==std::string::npos?text.size():end)-start;
      const auto shown=visible_bytes<=start?0:std::min(length,visible_bytes-start);
      out+="<div class=\"reading-paragraph\" id=\"reading-text-"+escape(id)+"-"+std::to_string(index++)+"\">"+history_text(text.substr(start,shown));
      if(shown<length)out+="<span style=\"visibility:hidden;\">"+escape(text.substr(start+shown,length-shown))+"</span>";
      out+="</div>";if(end==std::string::npos)break;start=end+2;
    }
    return out;
  }
  std::string readable_node(Json e,const Json& item,bool action=false) {
    if(!visible(e)||e.value("opacity",1.0)<=0)return "";
    const auto& visual=display_content.at("project").at("visual");e=resolved_style(visual,e);
    const std::string kind=e.value("kind","");const double font=std::max(visual.at("readableLayout").value("fontSize",18.0),item.value("fontSize",visual.at("readableLayout").value("fontSize",18.0)))*player_preferences.value("textScale",1.0);
    e["fontSize"]=font;e["width"]=std::max(44.0,std::min(double(safe_area.value("width",window_width)),visual.at("readableLayout").value("maxWidth",760.0))-2*visual.at("readableLayout").value("padding",16.0));
    e["statsLayout"]="bars";e["x"]=0;e["y"]=0;e["height"]=item.value("imageHeight",160.0);
    std::string body=kind=="saveSlots"||kind=="loadSlots"?readable_slots(e):kind=="playerSettings"?readable_settings(e):kind=="schedule"?(e.value("scheduleLayout","")=="activity-board"?activity_board(e):item.value("schedulePresentation","")=="compact"?compact_schedule(e,item.value("planningGuidance","")=="first-use",item.value("planReuse","")=="last-committed"):readable_schedule(e)):element_body(e);
    if(kind=="schedule"&&e.value("scheduleLayout","")!="activity-board")body=phase_forecast_launcher(e)+body;
    if(kind=="schedule"||kind=="saveSlots"||kind=="loadSlots"||kind=="playerSettings")body=colored_controls(body,control_colors(e,true));
    if(kind=="text"||kind=="dialogue")body=readable_text(e.value("id",""),kind=="dialogue"?bind("story"):e.value("binding","none")=="none"?e.value("text",""):bind(e.value("binding","none"),e));
    if(body.empty())return "";
    // Legacy result/choice fragments contain fixed line metrics. Reading mode
    // lets them inherit the selected size and use their measured content height.
    if(kind=="result"||kind=="choices")for(const std::string property:{"font-size:","line-height:","height:","width:"}){
      size_t at=0;while((at=body.find(property,at))!=std::string::npos){if(at&&body[at-1]!=';'&&body[at-1]!='"'&&body[at-1]!=' '){at+=property.size();continue;}const auto end=body.find(';',at);if(end==std::string::npos)break;body.erase(at,end-at+1);}
    }
    if(!e.value("_boundEnabled",true)||(e.contains("enabledCondition")&&!visual_condition(e.at("enabledCondition"))))
      for(const std::string tag:{"<button ","<select ","<input "})for(size_t at=0;(at=body.find(tag,at))!=std::string::npos;){const auto end=body.find('>',at);if(body.substr(at,end-at).find("disabled=")==std::string::npos)body.insert(at+tag.size(),"disabled=\"disabled\" ");at=body.find('>',at)+1;}
    std::string style="font-size:"+number(font)+"px;line-height:1.45;opacity:"+number(e.value("opacity",1.0))+";text-align:"+e.value("align","left")+";";
    if(e.contains("fontAssetId"))style+="font-family:engine-"+e.at("fontAssetId").get<std::string>()+";";
    for(const auto* key:{"color","fill"})if(e.contains(key))style+=(std::string(key)=="fill"?"background-color:":"color:")+css_color(e.at(key))+";";
    if(e.contains("padding"))style+="padding:"+number(e.at("padding"))+"px;";
    if(e.contains("borderWidth"))style+="border-width:"+number(e.at("borderWidth"))+"px;border-color:"+css_color(e.value("borderColor","#91a196"))+";";
    if(e.contains("radius"))style+="border-radius:"+number(e.at("radius"))+"px;";
    if(kind=="image")style+="height:"+number(item.value("imageHeight",160.0))+"px;";
    if(kind=="meter")style+="height:"+number(std::max(12.0,e.value("meterThickness",12.0)))+"px;";
    const auto id=e.value("id","");
    return "<div id=\"node-"+escape(id)+"\""+SurfacePaint::marker(e.value("surface",Json()))+" class=\"reading-item "+kind+(action?" reading-action":"")+"\" style=\""+style+"\">"+body+"</div>";
  }
  std::string readable_document(const Json& visual,const Json& screen) {
    readable_active=true;readable_screen_id=screen.at("id");width=window_width;height=window_height;
    const auto config=visual.at("readableLayout");readable_font=config.value("fontSize",18.0)*player_preferences.value("textScale",1.0);readable_target=std::max(44.0,config.value("minTargetSize",44.0));
    const std::string background=config.value("background","#172c29");const auto rgb=std::stoul(background.substr(1),nullptr,16);const std::string ink=(.2126*((rgb>>16)&255)+.7152*((rgb>>8)&255)+.0722*(rgb&255)>145)?"#263c36":"#f5efdf";
    const double pad=config.value("padding",16.0),gap=config.value("gap",12.0),sw=safe_area.value("width",width),sh=safe_area.value("height",height),cw=std::min(sw,config.value("maxWidth",760.0));
    const double left=safe_area.value("x",0)+(sw-cw)/2,top=safe_area.value("y",0);
    const auto layout=screen.at("readableLayout");const std::string prefix=screen.value("modal",false)?screen.at("id").get<std::string>()+"__":"";
    const bool grouped=layout.value("actionLayout","")=="grouped";
    if(!grouped)reading_menu=false;
    if(compact_picker&&!compact_picker_valid())compact_picker=false;
    reuse_projection=nullptr;
    if(plan_reuse_open&&!reuse_review_valid())plan_reuse_open=false;
    if(phase_forecast_open&&!phase_forecast_valid())phase_forecast_open=false;

    Json selected=Json::array();std::string main,footer;
    const auto wrap=[&](const std::string& body){return "<div class=\"reading-item reading-action\">"+body+"</div>";};
    const auto resolved=[&](const Json& item)->Json{const auto id=prefix+item.at("elementId").get<std::string>();for(auto e:render_elements)if(e.at("id")==id){
      for(const auto* key:{"_clipRect","_scrollParent","_collectionParent","parentId"})e.erase(key);return e;}return nullptr;};
    const auto append=[&](const Json& item,bool action){const auto e=resolved(item);if(e.is_null())return;
      auto html=readable_node(e,item,action);if(!html.empty()){selected.push_back(e);(action?footer:main)+=html;}};
    Json guided=nullptr;
    for(const auto& item:layout.at("items"))if(item.value("schedulePresentation","")=="compact"&&item.value("planningGuidance","")=="first-use"){
      const auto e=resolved(item);if(!e.is_null()&&planning_guide_eligible(e)){guided=e;break;}
    }
    if(!guided.is_null())ensure_plan();
    if(guided.is_null()&&planning_guide)if(const auto* board=activity_board_widget(planning_guide_element);board&&planning_guide_eligible(*board))guided=*board;
    if(planning_guide&&(guided.is_null()||screen_id!=planning_guide_owner))planning_guide=false;
    if(!guided.is_null()&&!reading_overlay()&&!planning_guide_seen&&!player_preferences.value("guidance",Json::object()).value("compact-planning-v1",false)&&!planning_guide_suppressed){
      planning_guide=true;planning_guide_seen=true;planning_guide_step=0;planning_guide_element=guided.at("id");planning_guide_owner=screen_id;
    }
    readable_view=portable_open()?"portable:"+portable_status:resume_context_open?"resume-context":phase_forecast_open?"phase-forecast":plan_reuse_open?"reuse":reading_history_open?"history":planning_guide?"guide:"+std::to_string(planning_guide_step):reading_options?"options":reading_menu?"menu":activity_board_open&&activity_board_reading?"activity-board:"+activity_board_element+":"+std::to_string(activity_board_slot):compact_picker?"picker:"+std::to_string(compact_slot):readable_timeline_visible()?"timeline:"+std::to_string(game.timeline_cursor.value("clipIndex",0)):"game";
    if(portable_open()){
      main=portable_review_body();
      footer=wrap(button("engine-portable-cancel",settings_words("취소","Cancel","キャンセル"),{{"type","portableCancel"}}));
      if(portable_status=="review")footer+=wrap(button("engine-portable-confirm",settings_words("백업 이어 하기","Continue backup","バックアップを続ける"),{{"type","portableConfirm"}}));
    }else if(resume_context_open){
      main=resume_context_body();footer=wrap(button("engine-resume-context-close",settings_words("계속하기","Continue","続ける"),{{"type","resumeContextClose"}}));
    }else if(phase_forecast_open){
      main=phase_forecast_body();footer=wrap(button("engine-phase-forecast-close",settings_words("계획으로 돌아가기","Back to plan","計画に戻る"),{{"type","phaseForecastClose"}}));
      for(const auto& e:render_elements)if(e.value("id","")==phase_forecast_element){selected.push_back(e);break;}
    }else if(plan_reuse_open){
      reuse_projection=reuse_snapshot.at("source");main=reuse_review_body();
      footer=wrap(button("board-reuse-apply",settings_words("초안에 적용","Apply to draft","下書きに適用"),{{"type","readingReuseApply"}}));
      footer+=wrap(button("board-reuse-cancel",settings_words("취소","Cancel","キャンセル"),{{"type","readingReuseCancel"}}));
      for(const auto& e:render_elements)if(e.at("id")==reuse_element){selected.push_back(e);break;}
    }else if(reading_history_open){
      main=reading_history_body();footer=wrap(button("engine-reading-history-close",settings_words("닫기","Close","閉じる"),{{"type","readingHistoryClose"}}));
    }else if(planning_guide){
      main=planning_guide_body(layout,prefix);
      footer=planning_guide_step==0?wrap(button("engine-planning-guide-skip",settings_words("건너뛰기","Skip","スキップ"),{{"type","planningGuideDismiss"}})):wrap(button("engine-planning-guide-back",settings_words("뒤로","Back","戻る"),{{"type","planningGuideStep"},{"delta",-1}}));
      footer+=wrap(button(planning_guide_step==2?"engine-planning-guide-done":"engine-planning-guide-next",planning_guide_step==2?settings_words("알겠습니다","Done","完了"):settings_words("다음","Next","次へ"),planning_guide_step==2?Json{{"type","planningGuideDismiss"}}:Json{{"type","planningGuideStep"},{"delta",1}}));
    }else if(reading_options){
      selected=Json::array({{{"id","engine-reading-settings"},{"kind","playerSettings"}}});
      main="<div id=\"node-engine-reading-settings\" class=\"reading-item\">"+readable_settings({{"id","engine-reading-settings"},{"_readingOnly",true}})+"</div>";
      footer=wrap(button("engine-reading-close",settings_words("게임으로 돌아가기","Back to game","ゲームへ戻る"),{{"type","readingOptions"},{"open",false}}));
    }else if(readable_timeline_visible()){
      // A presentation replaces the authored result only while its cursor still owns progression.
      // It is deliberately not a reading_overlay(): the presentation clock must keep running.
      main=readable_timeline_body(cw-2*pad-12,sh-2*pad);
      const bool completed=game.timeline_cursor.value("completed",false);
      if(!completed)footer=wrap(button("timeline-pause",game.timeline_cursor.value("paused",false)?settings_words("계속 재생","Resume","再開"):settings_words("일시정지","Pause","一時停止"),{{"type","timelinePause"}}));
      footer+=wrap(button(completed?"timeline-continue":"timeline-skip",completed?settings_words("결과 확인 · 계속","Review result · continue","結果を確認 · 続ける"):settings_words("건너뛰기","Skip","スキップ"),{{"type",completed?"advance":"timelineSkip"}}));
    }else if(activity_board_open&&activity_board_reading&&activity_board_valid()){
      if(const auto* raw=activity_board_widget(activity_board_element)){auto e=resolved_style(visual,*raw);e["fontSize"]=readable_font;selected.push_back(e);main="<div id=\"node-"+escape(activity_board_element)+"\" class=\"reading-item schedule\">"+colored_controls(activity_board_groups(e)+activity_board_cards(e)+activity_board_details(e),control_colors(e,true))+"</div>";footer="<div id=\""+escape(activity_board_element)+"__board-footer\" style=\"display:flex;flex-wrap:wrap;width:100%;\">"+colored_controls(activity_board_footer(e),control_colors(e,true))+"</div>";}
    }else if(compact_picker){
      for(const auto& e:render_elements)if(e.at("id")==compact_element){selected.push_back(e);main="<div id=\"node-"+escape(compact_element)+"\" class=\"reading-item schedule\">"+compact_activity_picker(e)+"</div>";break;}
      footer=wrap(button("board-apply",settings_words("이 활동 적용","Apply activity","この活動を適用"),{{"type","readingPlanApply"}},!compact_activity.empty()));
      footer+=wrap(button("board-cancel",settings_words("취소","Cancel","キャンセル"),{{"type","readingPlanCancel"}}));
    }else{
      if(!reading_menu)for(const auto& item:layout.at("items"))append(item,false);
      for(const auto& id:layout.value("actions",Json::array())){
        const Json item={{"elementId",id}};const auto e=resolved(item);if(e.is_null())continue;
        const auto html=readable_node(e,item,!reading_menu);if(html.empty())continue;
        const bool primary=grouped&&layout.value("primaryAction","")==id&&html.find("disabled=\"disabled\"")==std::string::npos;
        if(!grouped||(!reading_menu&&primary)){selected.push_back(e);footer+=html;}
        else if(reading_menu&&!primary){selected.push_back(e);main+=html;}
      }
      if(reading_menu){
        if(history_enabled())main+="<div class=\"reading-item\">"+button("engine-reading-history",history_label(),{{"type","readingHistory"}})+"</div>";
        if(resume_context_enabled())main+="<div class=\"reading-item\">"+button("engine-resume-context",resume_context_label(),{{"type","resumeContext"}})+"</div>";
        main="<p><b>"+settings_words("메뉴","Menu","メニュー")+"</b></p>"+main;
        main+="<div class=\"reading-item\">"+button("engine-reading-options",settings_words("읽기 설정","Reading options","読み方の設定"),{{"type","readingOptions"},{"open",true}})+"</div>";
        footer=wrap(button("engine-reading-menu-close",settings_words("게임으로 돌아가기","Back to game","ゲームへ戻る"),{{"type","readingMenu"},{"open",false}}));
      }else if(grouped)footer+=wrap(button("engine-reading-menu",settings_words("메뉴","Menu","メニュー"),{{"type","readingMenu"},{"open",true}}));
      else footer+=wrap(button("engine-reading-options",settings_words("읽기 설정","Reading options","読み方の設定"),{{"type","readingOptions"},{"open",true}}));
    }
    render_elements=selected;
    std::string css="body{font-family:Noto Sans KR;font-size:"+number(readable_font)+"px;color:"+ink+";background-color:"+css_color(background)+";margin:0;width:"+std::to_string(width)+"px;height:"+std::to_string(height)+"px;} div,p{display:block;box-sizing:border-box;} p{margin:0 0 "+number(gap)+"px;} span,b{display:inline;} .reading-root{position:absolute;left:"+number(left)+"px;top:"+number(top)+"px;width:"+number(cw)+"px;height:"+number(sh)+"px;display:flex;flex-direction:column;padding:"+number(pad)+"px;} .reading-main{flex:1;min-height:0;overflow-x:hidden;overflow-y:auto;} .reading-item{position:relative;width:100%;height:auto;margin-bottom:"+number(gap)+"px;white-space:normal;word-break:break-word;} .reading-item button,.reading-item input,.reading-item select,.reading-item option{box-sizing:border-box;display:block;width:100%;min-width:"+number(readable_target)+"px;min-height:"+number(readable_target)+"px;height:auto;padding:10px 12px;margin:0 0 "+number(gap)+"px;line-height:1.45;white-space:normal;word-break:break-word;background-color:#e3e8df;color:#263c36;border-width:1px;border-color:#91a196;border-radius:4px;tab-index:auto;} button:focus{border-width:3px;border-color:#b38b49;} button:disabled{opacity:0.5;} .reading-actions{display:flex;flex-wrap:wrap;align-items:flex-start;flex-shrink:0;max-height:45%;overflow-y:auto;overflow-x:hidden;padding-top:"+number(gap)+"px;} .reading-action{flex-grow:1;flex-basis:"+number(140*player_preferences.value("textScale",1.0))+"px;min-width:"+number(readable_target)+"px;margin-right:8px;} .reading-action>button{margin-bottom:0;} .choices button>span{margin-right:0.5em;} .stat{height:auto;padding:8px 0;} .value{float:right;} .bar{height:5px;background-color:#d8e3d8;} .bar>div{height:5px;background-color:#608c69;} .reading-item .heading,.reading-item .row{height:auto;} .reading-item .reason{display:block;} scrollbarvertical{width:12px;} scrollbarvertical slidertrack{background-color:#e1e0d6;} scrollbarvertical sliderbar{min-height:44px;background-color:#718879;} scrollbarvertical sliderarrowdec,scrollbarvertical sliderarrowinc{height:0;}";
    css+=".record-groups{display:flex;flex-wrap:wrap;} .reading-item .record-group{flex-grow:1;flex-basis:auto;width:auto;word-break:normal;margin-right:8px;padding-left:8px;padding-right:8px;} .reading-item .record-group:last-child{margin-right:0;} .reading-item .record-slot,.reading-item .record-group{border-width:3px;} .reading-item .record-selected{border-color:#345c50;background-color:#d6e2d8;}.reading-item .record-slot:focus,.reading-item .record-group:focus{border-color:#b38b49;} .record-identity{display:block;} .record-saved{display:block;font-size:0.85em;margin-top:6px;} .record-thumbnail{display:block;width:144px;height:81px;margin-top:8px;} .reading-notice{white-space:normal;word-break:break-word;} .reading-paragraph{margin-bottom:"+number(gap)+"px;}";
    if(phase_forecast_open||portable_open()||resume_context_open||grouped||(activity_board_open&&activity_board_reading)||compact_picker||planning_guide||reading_history_open||plan_reuse_open)css+=".reading-actions{max-height:none;overflow-y:visible;} .reading-action{flex-basis:"+number(120*player_preferences.value("textScale",1.0))+"px;}";
    for(const auto& raw:render_elements){const auto e=resolved_style(visual,raw);const auto id=e.value("id","");const auto selector="#node-"+id+" button";css+=selector+"{color:"+css_color(e.value("color","#263c36"))+";background-color:"+css_color(e.value("fill","#e3e8df"))+";}";}
    css+=surface_styles(visual,render_elements,config.value("surface",Json()))+control_colors_css(render_elements,true);
    if(activity_board_open&&activity_board_reading)if(const auto* e=activity_board_widget(activity_board_element))css+=control_color_css(control_colors(*e,true),"#"+activity_board_element+"__board-footer");
    main=colored_controls(main,reading_control_colors(),true);footer=colored_controls(footer,reading_control_colors(),true);
    // Put notices into flow so they cannot cover the final action on short phones.
    if(!game.message.empty()&&!portable_open()&&!resume_context_open)main+="<p class=\"reading-notice\">"+escape(game.message)+"</p>";
    css+=control_fallback_css;
    const auto modal=screen.value("modal",false);return "<rml><head><style>"+css+"</style></head><body"+SurfacePaint::marker(config.value("surface",Json()))+"><div "+std::string(modal?"id=\"ui-modal\" ":"")+"class=\"reading-root\"><div id=\"reading-main\" class=\"reading-main\""+std::string((phase_forecast_open||readable_timeline_visible())?" style=\"tab-index:auto;\"":"")+">"+main+"</div><div id=\"reading-actions\" class=\"reading-actions\">"+footer+"</div></div></body></rml>";
  }
