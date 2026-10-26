  std::string activity_board_expression(const Json& value) const {
    const auto op=value.value("op","");
    if(op=="literal")return value.at("value").dump();
    if(op=="time"){const auto field=value.value("field","");if(field=="year")return settings_words("연도","Year","年");if(field=="month")return settings_words("월","Month","月");return field=="periodIndex"?settings_words("전체 기간 번호 (0부터)","Overall period index (from 0)","通算期間番号（0から）"):settings_words("단계 내 기간 번호 (0부터)","Period index in phase (from 0)","段階内の期間番号（0から）");}
    if(op=="read"){const auto target=value.at("target");const auto kind=target.value("kind","");if(kind=="attribute"){const auto id=target.value("attributeId","");return attribute_name(id)+" ("+settings_words("현재","current","現在")+" "+game.state.at("attributes").value(id,Json()).dump()+")";}return kind=="relationship"?settings_words("관계 수치","Relationship value","関係値"):settings_words("물품 수량","Item quantity","アイテム数");}
    if(op=="clamp")return settings_words("범위 제한","Clamped","範囲制限")+" ("+activity_board_expression(value.at("value"))+", "+value.at("min").dump()+"…"+value.at("max").dump()+")";
    if(op=="add"||op=="subtract"||op=="multiply")return "("+activity_board_expression(value.at("left"))+" "+(op=="add"?"+":op=="subtract"?"−":"×")+" "+activity_board_expression(value.at("right"))+")";
    return settings_words("실행 시 계산","Calculated when run","実行時に計算");
  }
  std::string activity_board_requirement(const Json& value) const {
    const auto op=value.value("op","");if(op=="always")return settings_words("없음","None","なし");
    if(op=="compare"){const std::map<std::string,std::string> signs{{"eq","="},{"ne","≠"},{"gt",">"},{"gte","≥"},{"lt","<"},{"lte","≤"}};const auto found=signs.find(value.value("comparator",""));return activity_board_expression(value.at("left"))+" "+(found==signs.end()?"?":found->second)+" "+activity_board_expression(value.at("right"));}
    if(op=="all"||op=="any"){std::string out;for(const auto& child:value.at("conditions")){if(!out.empty())out+=op=="all"?settings_words(" 그리고 "," and "," かつ "):settings_words(" 또는 "," or "," または ");out+="("+activity_board_requirement(child)+")";}return out;}
    if(op=="not")return settings_words("제외","Not","除外")+": ("+activity_board_requirement(value.at("condition"))+")";
    return settings_words("추가 이야기·상태 조건이 있습니다.","Additional story or state conditions apply.","追加の物語・状態条件があります。");
  }
  // The review is presentation state, separate from the persisted schedule draft.
  bool activity_board_open=false,activity_board_reading=false;
  uint64_t activity_board_serial=0;
  int activity_board_slot=0;
  std::string activity_board_owner,activity_board_element,activity_board_activity,activity_board_group;
  Json activity_board_snapshot=nullptr;
  std::string activity_board_focus;
  int activity_board_first() const {
    if(game.state.is_null())return 0;
    return std::min(game.periods(),game.state.value("periodIndex",0)%game.periods()+(game.state.value("activityIntermission",false)?1:0));
  }
  int activity_board_display_slot() const {return std::min(game.periods()-1,std::max(activity_board_first(),plan_slot));}
  Json activity_board_token() const {
    return {{"generation",reuse_generation},{"revision",game.state.value("stateRevision",-1)},{"period",game.state.value("periodIndex",-1)},{"phase",game.state.value("phase","")},{"intermission",game.state.value("activityIntermission",false)},{"schedule",game.state.value("schedule",Json())},{"draft",plan_save()},{"periods",game.periods()},{"content",game.content.value("contentFingerprint","")}};
  }
  const Json* activity_board_widget(const std::string& id) const {
    for(const auto& e:render_elements)if(e.value("id","")==id&&e.value("kind","")=="schedule"&&e.value("scheduleLayout","")=="activity-board")return &e;return nullptr;
  }
  bool activity_board_eligible(const std::string& id) const {
    if(game.state.is_null()||display_content.at("project").contains("roster")||progress_pending()||!ui_modal.empty()||!pending_purchase.is_null()||replay_controls||game.gallery.active)return false;
    const auto phase=game.state.value("phase","");if(phase!="planning"&&phase!="ready")return false;
    const auto* e=activity_board_widget(id);return e&&visible(*e)&&e->value("opacity",1.0)>0&&e->value("_boundEnabled",true)&&(!e->contains("enabledCondition")||visual_condition(e->at("enabledCondition")));
  }
  bool activity_board_valid() const {
    return activity_board_open&&activity_board_owner==screen_id&&activity_board_eligible(activity_board_element)&&activity_board_snapshot==activity_board_token()&&activity_board_slot>=activity_board_first()&&activity_board_slot<game.periods();
  }
  void close_activity_board() {activity_board_open=false;activity_board_reading=false;activity_board_activity.clear();activity_board_group.clear();activity_board_snapshot=nullptr;++activity_board_serial;}
  Json activity_board_action(const Json& e,const std::string& type,Json extra=Json::object()) const {
    extra["type"]="activityBoard"+type;extra["elementId"]=e.at("id");extra["screenId"]=screen_id;extra["token"]=activity_board_token();extra["serial"]=activity_board_serial;return extra;
  }
  Json activity_board_item(const std::string& id) const {
    for(const auto& s:display_content.at("project").at("visual").at("screens"))if(s.value("id","")==screen_id)for(const auto& item:s.value("readableLayout",Json::object()).value("items",Json::array()))if(item.value("elementId","")==id)return item;return Json::object();
  }
  std::string activity_board_id(const Json& e,const std::string& suffix) const {return e.value("id","")+"__board-"+suffix;}
  std::string activity_board_name(const std::string& id) const {for(const auto& a:display_content.at("activities"))if(a.value("id","")==id)return a.value("label",id);return settings_words("활동 선택","Choose activity","活動を選ぶ");}
  std::string activity_board_icon(const Json& e,const std::string& id) const {for(const auto& icon:e.value("activityIcons",Json::array()))if(icon.value("activityId","")==id)return icon.value("imageAssetId","");return "";}
  std::string activity_board_image(const std::string& id,double size) const {
    if(asset(id).empty())return "";return "<span class=\"activity-board-icon\" style=\"display:block;width:"+number(size)+"px;height:"+number(size)+"px;margin-bottom:8px;pointer-events:none;\">"+image({{"imageAssetId",id},{"fit","contain"}})+"</span>";
  }
  std::string activity_board_inline_image(const std::string& id,double size) const {
    const auto path=asset(id);if(path.empty())return "";
    return "<span style=\"display:inline-block;flex-shrink:0;width:"+number(size)+"px;height:"+number(size)+"px;vertical-align:middle;margin-right:8px;pointer-events:none;decorator:image(&quot;"+escape(path)+"&quot; contain);\"></span>";
  }
  bool activity_board_compact(const Json& e) const {return e.value("activityBoard",Json::object()).value("density","")=="compact";}
  std::string activity_board_summary(const Json& e,const Json& activity) const {
    const auto options=e.value("activityBoard",Json::object());if(!e.value("showActivityRules",false)||!options.value("showSummary",false))return "";
    Json chips=Json::array();bool computed=false,other=false;
    const auto add=[&](const std::string& kind,const std::string& id,double value,const std::string& op){chips.push_back({{"kind",kind},{"id",id},{"value",value},{"op",op}});};
    for(const auto& c:activity.value("costs",Json::array()))add("cost",c.at("attributeId"),-c.at("amount").get<double>(),"add");
    for(const auto& effect:activity.value("baseEffects",Json::array())){
      const auto op=effect.value("op","");const auto target=effect.value("target",Json::object());
      if((op=="add"||op=="set")&&target.value("kind","")=="attribute"&&!target.contains("owner")){
        if(effect.at("value").value("op","")=="literal")add("effect",target.at("attributeId"),effect.at("value").at("value"),op);else computed=true;
      }else other=true;
    }
    std::vector<std::string> order;for(const auto& id:options.value("summaryAttributeIds",Json::array()))order.push_back(id);for(const auto& a:display_content.at("attributes"))if(std::find(order.begin(),order.end(),a.at("id").get<std::string>())==order.end())order.push_back(a.at("id"));
    const auto rank=[&](const Json& chip){return std::find(order.begin(),order.end(),chip.at("id").get<std::string>())-order.begin();};std::stable_sort(chips.begin(),chips.end(),[&](const Json& a,const Json& b){return rank(a)<rank(b);});
    const auto chip=[&](const std::string& text){return "<span style=\"display:inline-block;margin:2px 4px 0 0;padding:1px 4px;border-width:1px;border-color:"+css_color(e.value("borderColor","#8b9e92"))+";\">"+escape(text)+"</span>";};
    std::string out="<span class=\"activity-board-summary\" style=\"display:block;font-size:.78em;line-height:1.35;\">";int shown=0;for(const auto& row:chips){if(shown++>=4)break;const auto n=row.at("value").get<double>();out+=chip((row.at("kind")=="cost"?settings_words("비용 ","Cost ","費用 "):std::string())+attribute_name(row.at("id"))+" "+(row.at("op")=="set"?"= ":n>0?"+":"")+number(n));}
    if(chips.size()>4)out+=chip("+"+std::to_string(chips.size()-4)+settings_words("개 더"," more","件"));if(computed)out+=chip(settings_words("계산 효과","Calculated effects","計算される効果"));if(other)out+=chip(settings_words("기타 규칙","Other rules","その他のルール"));if(activity.contains("successCheck"))out+=chip(settings_words("성공·실패 효과","Outcome effects","成否による効果"));return out+"</span>";
  }
  std::string activity_board_button(const Json& e,const std::string& suffix,const std::string& label,const std::string& type,Json extra=Json::object(),bool enabled=true,const std::string& body="",bool selected=false) {
    const double font=readable_active?readable_font:std::max(16.0,e.value("fontSize",20.0));
    auto html=button(activity_board_id(e,suffix),label,activity_board_action(e,type,extra),enabled&&activity_board_eligible(e.at("id")));
    const auto frame=asset(e.value("entryFrameAssetId",Json()));
    std::string style="display:block;width:100%;box-sizing:border-box;min-width:"+number(minimum_control_extent())+"px;min-height:"+number(minimum_control_extent())+"px;height:auto;margin:0;padding:10px 12px;white-space:normal;word-break:break-word;text-align:left;font-size:"+number(font)+"px;line-height:1.45;color:"+css_color(e.value("color","#203a34"))+";background-color:"+css_color(e.value("fill","#f4f5ee"))+";border-width:3px;border-color:"+css_color(selected?"#b38b49":e.value("borderColor","#8b9e92"))+";";
    if(activity_board_compact(e))style+="padding:5px 8px;line-height:1.35;";
    if(!frame.empty())style+="decorator:image("+frame+");";
    html.insert(html.find('>')," class=\"activity-board-card"+std::string(selected?" board-selected":"")+"\" style=\""+style+"\"");
    if(!body.empty()){const auto start=html.find('>')+1;html.replace(start,html.rfind("</button>")-start,body);}return html;
  }
  std::string activity_board_tickets(const Json& e) {
    const auto id=e.at("id").get<std::string>();const bool own=activity_board_open&&activity_board_element==id;const int selected=own?activity_board_slot:activity_board_display_slot();
    const double width=std::max(1.0,e.value("width",800.0)-2*(e.value("padding",6.0)+e.value("borderWidth",0.0))-16);
    // Reading text grows independently of window points. Grow each ticket
    // with its rendered font, then keep one complete ticket inside the pane.
    const double desired=std::max(minimum_control_extent()+32,std::max(readable_active?144.0*std::max(1.0,readable_font/18.0):160.0,(width-8*(std::min(4,game.periods())-1))/std::min(4,game.periods())));
    const double ticket=readable_active?std::min(width,desired):desired;
    std::string out="<div id=\""+escape(activity_board_id(e,"periods"))+"\" class=\"activity-board-periods\" style=\"flex-shrink:0;width:100%;overflow-x:auto;overflow-y:hidden;pointer-events:auto;tab-index:auto;padding-bottom:10px;\"><div style=\"display:flex;width:"+number((ticket+8)*game.periods()-8)+"px;\">";
    for(int i=0;i<game.periods();++i){const bool assigned=plan_selected&(1<<i);const auto name=assigned?activity_board_name(plan[i]):settings_words("활동 선택","Choose activity","活動を選ぶ");const auto period=compact_period(i)+(activity_board_compact(e)&&i==selected?" · "+settings_words("선택됨","Selected","選択中"):std::string());const auto icon=assigned?activity_board_icon(e,plan[i]):"";
      const auto content="<span style=\"display:block;font-size:.85em;margin-bottom:6px;\">"+escape(activity_board_compact(e)&&readable_active?compact_period(i)+(i==selected?" · ✓":""):period)+"</span>"+"<span style=\"display:flex;align-items:center;\">"+activity_board_inline_image(icon,24)+"<span style=\"display:block;flex:1;min-width:0;\">"+escape(name)+"</span></span>"+(i<activity_board_first()?"<span style=\"display:block;font-size:.85em;\">"+escape(settings_words("진행 완료","Already played","進行済み"))+"</span>":std::string());
      out+="<div style=\"flex-shrink:0;width:"+number(ticket)+"px;margin-right:"+std::string(i+1==game.periods()?"0":"8")+"px;\">"+activity_board_button(e,"slot-"+std::to_string(i),period+" · "+name,"Slot",{{"slot",i}},i>=activity_board_first(),content,i==selected)+"</div>";
    }return out+"</div></div>";
  }
  std::string activity_board_groups(const Json& e) {
    const bool own=activity_board_valid()&&activity_board_element==e.at("id");const auto filter=own?activity_board_group:std::string();
    std::string out="<div id=\""+escape(activity_board_id(e,"groups"))+"\" class=\"activity-board-groups activity-board-periods\" style=\"flex-shrink:0;width:100%;white-space:nowrap;overflow-x:auto;overflow-y:hidden;pointer-events:auto;tab-index:auto;padding-bottom:10px;\">";
    const auto entry=[&](const std::string& suffix,const std::string& label,const std::string& group,const std::string& icon){
      auto control=activity_board_button(e,suffix,label,"Group",{{"group",group}},true,activity_board_inline_image(icon,22)+escape(label),group==filter);
      const auto style=control.find("style=\"")+7;
      // Appended declarations win over the ordinary wrapping card defaults.
      const auto end=control.find('"',style);control.insert(end,"display:inline-block;width:auto;white-space:nowrap;padding:6px 10px;");
      return "<span style=\"display:inline-block;vertical-align:top;margin-right:8px;\">"+control+"</span>";
    };
    out+=entry("group-all",settings_words("모든 활동","All activities","すべての活動"),"","");
    for(const auto& group:e.value("scheduleGroups",Json::array()))out+=entry("group-option-"+group.at("id").get<std::string>(),group.value("label",group.at("id").get<std::string>()),group.at("id"),group.value("iconAssetId",""));
    return out+"</div>";
  }
  std::string activity_board_cards(const Json& e) {
    const bool own=activity_board_valid()&&activity_board_element==e.at("id");const auto filter=own?activity_board_group:std::string();const auto groups=e.value("scheduleGroups",Json::array());
    const Json* group=nullptr;for(const auto& g:groups)if(g.at("id")==filter)group=&g;
    std::string out="<div id=\""+escape(activity_board_id(e,"cards"))+"\" class=\"activity-board-cards\" style=\"display:flex;flex-wrap:wrap;\">";
    const bool compact=activity_board_compact(e);const int columns=!readable_active&&e.value("width",800.0)>=640?2:1;
    for(const auto& a:display_content.at("activities")){const auto aid=a.at("id").get<std::string>();if(group&&std::find(group->at("activityIds").begin(),group->at("activityIds").end(),Json(aid))==group->at("activityIds").end())continue;
      const auto artwork=activity_board_icon(e,aid);
      auto body=(asset(artwork).empty()?std::string():"<div style=\"height:100px;width:100%;margin-bottom:8px;pointer-events:none;\">"+image({{"imageAssetId",artwork},{"fit","cover"}})+"</div>")+"<span style=\"display:block;\"><b>"+escape(a.value("label",aid))+"</b></span>";
      if(compact)body="<span style=\"display:flex;align-items:flex-start;\">"+activity_board_inline_image(artwork,56)+"<span style=\"display:block;flex:1;min-width:0;\"><b>"+escape(a.value("label",aid))+"</b>"+activity_board_summary(e,a)+"</span></span>";
      else body+=activity_board_summary(e,a);
      out+="<div style=\"box-sizing:border-box;width:"+number(100.0/columns)+"%"+";padding:0 8px 10px 0;\">"+activity_board_button(e,"activity-"+aid,a.value("label",aid),"Inspect",{{"activityId",aid}},activity_board_first()<game.periods(),body,own&&activity_board_activity==aid)+"</div>";
    }return out+"</div>";
  }
  std::string activity_board_details(const Json& e) {
    std::string out="<div id=\""+escape(activity_board_id(e,"details"))+"\" class=\"activity-board-details\" style=\"white-space:normal;word-break:break-word;tab-index:auto;"+std::string(readable_active?"":"flex:1;min-height:0;overflow-y:auto;padding-right:14px;")+"\">";
    const bool own=activity_board_valid()&&activity_board_element==e.at("id");const int slot=own?activity_board_slot:activity_board_display_slot();const auto selected=own?activity_board_activity:(plan_selected&(1<<slot))?plan[slot]:std::string();const Json* activity=nullptr;for(const auto& a:display_content.at("activities"))if(a.at("id")==selected)activity=&a;
    out+="<p style=\"margin:0 0 8px;\"><b>"+escape(compact_period(slot))+"</b></p>";
    if(activity){out+="<p style=\"margin:0 0 8px;\">"+activity_board_inline_image(activity_board_icon(e,selected),24)+"<b>"+escape(activity->value("label",selected))+"</b></p>";if(e.value("showActivityDescription",false)&&!activity->value("description","").empty())out+="<p>"+escape(activity->value("description",""))+"</p>";if(e.value("showActivityRules",false))out+="<p>"+escape(settings_words("필요 조건 · ","Requirements · ","必要条件 · ")+activity_board_requirement(activity->value("requirements",Json{{"op","always"}})))+"</p><p>"+activity_rules(*activity)+"</p>";auto summary=activity_preview(*activity);const std::string prefix="현재 ";if(summary.rfind(prefix,0)==0)summary=settings_words("현재 ","Current ","現在 ")+summary.substr(prefix.size());if(!summary.empty())out+="<p>"+escape(summary)+"</p>";}
    else out+="<p>"+escape(settings_words("활동을 골라 설명과 규칙을 확인하세요.","Choose an activity to inspect its description and rules.","活動を選び、説明とルールを確認してください。"))+"</p>";
    out+="<p>"+escape(activity_board_compact(e)?settings_words("초안만 변경 · 실행 시 자원 사용","Draft only · resources used when run","下書きのみ · 実行時に資源を消費"):settings_words("적용은 선택한 일정의 초안만 바꿉니다. 확정하거나 실행할 때까지 시간과 자원은 변하지 않습니다.","Apply changes only this period in your draft. Time and resources stay unchanged until you confirm or run activities.","適用は選んだ予定の下書きだけを変更します。確定または実行するまで時間や資源は変わりません。"))+"</p>";return out+"</div>";
  }
  std::string activity_board_footer(const Json& e) {
    const bool own=activity_board_valid()&&activity_board_element==e.at("id");const bool stacked=readable_active&&width<420;std::string out;
    for(const auto& name:{std::string("apply"),std::string("cancel")})out+="<div class=\"activity-board-action\" style=\"flex:1;"+std::string(stacked?"flex-basis:100%;":"")+"min-width:0;margin:4px;\">"+activity_board_button(e,name,name=="apply"?settings_words("이 활동 적용","Apply activity","この活動を適用"):settings_words("취소","Cancel","キャンセル"),name=="apply"?"Apply":"Cancel",Json::object(),own&&(name=="cancel"||!activity_board_activity.empty()))+"</div>";return out;
  }
  std::string activity_board_tools(const Json& e) {
    const double available=std::max(1.0,e.value("width",800.0)-2*(e.value("padding",6.0)+e.value("borderWidth",0.0))-16);
    const double minimum=readable_active?std::min(available,120.0*std::max(1.0,readable_font/18.0)):120.0;
    const auto wrap=[&](const std::string& html){return html.empty()?std::string():"<div style=\"flex:1;min-width:"+number(minimum)+"px;margin:4px 8px 0 0;\">"+html+"</div>";};
    std::string out="<div style=\"display:flex;flex-wrap:wrap;flex-shrink:0;\">"+(e.value("activityBoard",Json::object()).value("forecast","")=="inline"?std::string():wrap(phase_forecast_launcher(e)));const auto item=activity_board_item(e.at("id"));
    if(item.value("planReuse","")=="last-committed")out+=wrap(reuse_launcher(e));
    out+=wrap(button(activity_board_id(e,"help"),settings_words("계획 방법 보기","How planning works","計画の立て方"),{{"type","planningGuideOpen"},{"elementId",e.at("id")}},planning_guide_eligible(e)));return out+"</div>";
  }
  std::string activity_board(const Json& e) {
    if(game.state.is_null())return "";
    if(display_content.at("project").contains("roster"))return "<p>"+escape(settings_words("활동 보드는 한 인물의 일정용입니다. 여러 인물은 배정 일정 도구를 사용하세요.","The activity board plans one character. Use the assignment planner for multiple characters.","活動ボードは一人の予定用です。複数の人物には割り当てプランナーを使ってください。"))+"</p>";
    ensure_plan();if(activity_board_open&&!activity_board_valid())close_activity_board();
    const auto tickets=activity_board_tickets(e);
    const auto brief=e.value("activityBoard",Json::object()).value("forecast","")=="inline"?phase_forecast_brief(e):std::string();
    if(readable_active)return tickets+brief+activity_board_tools(e);
    const double inset=2*(e.value("padding",6.0)+e.value("borderWidth",0.0));const double h=std::max(1.0,e.value("height",500.0)-inset),w=std::max(1.0,e.value("width",800.0)-inset);
    // Tickets, category strip and tools reserve natural-height rows. Only the
    // catalog and detail scroll on a wide board, keeping review actions visible.
    const bool wide=w>=640;const double left=wide?std::max(300.0,w*(activity_board_compact(e)?.67:.54)):w;
    std::string out="<div id=\""+escape(activity_board_id(e,"viewport"))+"\" class=\"activity-board\" style=\"display:flex;flex-direction:column;height:"+number(h)+"px;width:100%;overflow-y:"+std::string(wide?"hidden":"auto")+";overflow-x:hidden;pointer-events:auto;font-size:"+number(std::max(16.0,e.value("fontSize",20.0)))+"px;line-height:1.45;\">"+tickets+brief+activity_board_groups(e);
    out+="<div style=\"display:"+std::string(wide?"flex":"block")+";width:100%;flex:1;min-height:"+std::string(wide?"0":"210px")+";overflow:"+std::string(wide?"hidden":"visible")+";\"><div id=\""+escape(activity_board_id(e,"catalog"))+"\" class=\"activity-board-catalog\" style=\"width:"+std::string(wide?number(left)+"px":"100%")+";height:"+std::string(wide?"100%":"auto")+";min-height:0;flex-shrink:0;overflow-y:auto;overflow-x:hidden;pointer-events:auto;tab-index:auto;padding-right:14px;box-sizing:border-box;\">"+activity_board_cards(e)+"</div><div style=\"display:flex;flex-direction:column;flex:1;min-width:0;min-height:0;\">"+activity_board_details(e)+"<div style=\"display:flex;flex-wrap:wrap;flex-shrink:0;\">"+activity_board_footer(e)+"</div></div></div>"+activity_board_tools(e)+"</div>";
    return out;
  }
