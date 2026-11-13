  Json save_slots=Json::array();
  Json current_save_record=Json::object();
  bool slot_auto=false;
  int slot_selected=0,slot_page=0;
  std::string slot_confirmation;
  std::string slot_words(const char* ko,const char* en,const char* ja) const {return locale.rfind("en",0)==0?en:locale.rfind("ja",0)==0?ja:ko;}
  std::string slot_date(const Json& row) const {
    if(!row.contains("state"))return slot_words("빈 기록","Empty record","空の記録");
    const auto record=row.value("record",Json::object());
    if(display_content.at("project").contains("calendar"))return raising::dates::text(display_content.at("project"),row.at("state").value("periodIndex",0),locale);
    if(record.contains("date"))return record.at("date").get<std::string>();
    const int p=row.at("state").value("periodIndex",0),m=display_content.at("project").value("startMonth",1)-1+p/game.periods();
    return std::to_string(display_content.at("project").value("startYear",1)+m/12)+" / "+std::to_string(m%12+1)+" · "+std::to_string(p%game.periods()+1);
  }
  std::string slot_label(const Json& row) const {
    return (row.value("automatic",false)?slot_words("자동 ","Auto ","自動 "):slot_words("기록 ","Record ","記録 "))+std::to_string(row.value("displayNumber",row.value("index",0)+1));
  }
  std::string slot_phase(const std::string& phase) const {
    if(phase=="planning")return slot_words("계획 중","Planning","計画中");
    if(phase=="ready")return slot_words("활동 준비","Ready","準備完了");
    if(phase=="activity-result")return slot_words("활동 결과","Activity result","活動結果");
    if(phase=="narrative")return slot_words("이야기","Story","物語");
    if(phase=="finished")return slot_words("결말","Ending","エンディング");
    if(phase=="battle")return slot_words("전투","Battle","戦闘");
    if(phase=="expedition")return slot_words("탐험","Expedition","探索");
    return phase;
  }
  std::string slot_identity(const Json& row) const {
    const auto status=row.value("status","empty");std::string out=slot_label(row);
    if(status=="empty")return out+" · "+slot_words("비어 있음","Empty","空き");
    if(status=="invalid")return out+" · "+slot_words("읽을 수 없음 · 원본 보존","Unreadable · original kept","読めません · 原本を保持");
    if(status=="incompatible")return out+" · "+slot_words("다른 버전 · 불러오기 불가","Different version · cannot load","異なる版 · 読み込み不可");
    const auto meta=row.value("record",Json::object());out+="\n"+slot_date(row);
    for(const auto& detail:{meta.value("place",""),slot_phase(meta.value("phase",""))})if(!detail.empty())out+=" · "+detail;
    return out;
  }
  std::string slot_saved_at(const Json& row) const {
    const auto meta=row.value("record",Json::object());const auto raw=meta.value("savedAt","");
    if(raw.empty())return "";
    std::string value=raw;
    // Never reinterpret an old timezone-less wall clock using today's zone.
    // ISO Z is explicitly UTC; other offsets/formats remain exactly as recorded.
    const bool shape=raw.size()>=19&&raw[4]=='-'&&raw[7]=='-'&&(raw[10]=='T'||raw[10]==' ')&&raw[13]==':'&&raw[16]==':';
    bool digits=shape;for(size_t i=0;digits&&i<19;++i)if(i!=4&&i!=7&&i!=10&&i!=13&&i!=16)digits=raw[i]>='0'&&raw[i]<='9';
    if(digits&&(raw.size()==19||(raw[10]=='T'&&raw.back()=='Z'))){
      value=raw.substr(0,10)+" · "+raw.substr(11,8);
      value+=raw.back()=='Z'?" UTC":slot_words(" (기록 당시 현지 시각)"," (local time as recorded)","（記録時の現地時刻）");
    }
    auto out=slot_words("저장 · ","Saved · ","保存 · ")+value;
    if(meta.contains("sequence")&&meta.at("sequence").is_number_integer())out+=" · #"+meta.at("sequence").dump();
    return out;
  }
  std::string portable_tools(const Json& e) {
    const std::string id=e.at("id");
    std::string out="<p><b>"+slot_words("파일 백업","File backups","ファイルのバックアップ")+"</b></p>";
    out+=button(id+"-portable-import",slot_words("백업 가져오기","Import backup","バックアップを読み込む"),{{"type","portableImport"},{"ownerId",id}});
    out+=button(id+"-portable-export",slot_words("현재 진행 내보내기","Export current progress","現在の進行を書き出す"),{{"type","portableExport"},{"ownerId",id}},!game.state.is_null());
    if(!portable_notice.empty())out+="<p>"+escape(portable_notice)+"</p>";return out;
  }
  std::string portable_review_body() const {
    const bool review=portable_status=="review",exporting=portable_status=="picking-export";
    std::string out="<div id=\"engine-portable-panel\" class=\"reading-item\"><p><b>"+slot_words("백업 검토","Review backup","バックアップの確認")+"</b></p>";
    auto identity=[&](const Json& record){std::string text=record.value("date","");for(const auto* key:{"place","phase"})if(!record.value(key,"").empty()){if(!text.empty())text+=" · ";text+=record.at(key).get<std::string>();}return escape(text);};
    if(review){
      out+="<p>"+escape(portable_filename)+"</p><p><b>"+identity(portable_incoming)+"</b></p>";
      if(!portable_current.empty())out+="<p>"+slot_words("현재 진행 · ","Current progress · ","現在の進行 · ")+identity(portable_current)+"</p>";
      out+="<p>"+slot_words("이어 하면 저장하지 않은 현재 진행이 바뀝니다. 수동 기록은 그대로 남습니다.","Continuing replaces unsaved current progress. Your manual records stay unchanged.","続けると未保存の現在の進行が置き換わります。手動の記録は変わりません。")+"</p>";
    }else out+="<p>"+std::string(exporting?slot_words("파일 앱에서 현재 진행의 백업을 저장할 위치를 선택하세요.","Choose where to save a backup of current progress in Files.","ファイルで現在の進行のバックアップの保存先を選んでください。"):slot_words("파일 앱에서 백업 하나를 선택하세요. 이어 하기 전에 검증하고 검토합니다.","Choose one backup in Files. It will be validated and reviewed before continuing.","ファイルでバックアップを1つ選んでください。再開前に検証して確認します。"))+"</p>";
    return out+"</div>";
  }
  std::string slots_body(const Json& e) {
    const bool saving=e.value("kind","")=="saveSlots";
    const std::string id=e.at("id"),ink=e.value("color","#263C36"),paper=e.value("fill","#F5EFDF"),line=e.value("borderColor","#C7B99A"),accent=e.value("hoverFill","#345C50");
    const auto palette=control_colors(e);const std::string card_ink=palette.value("color",ink),card_accent=palette.value("color",accent);
    // Buttons retain at least 44 window points after canvas-fit scaling. Rows
    // grow with their wrapped labels; the authored widget owns any scrolling.
    const double target=minimum_control_extent(),inset=e.value("padding",6.0)+e.value("borderWidth",0.0),w=std::max(1.0,e.value("width",1100.0)-2*inset-40),h=e.value("height",530.0)-2*inset,gap=14,cardh=std::max({124.0,target,(h-180)/3});
    const int columns=w<680?1:2;const double cardw=(w-gap*(columns-1))/columns;
    auto box=[&](double x,double y,double width,double height,const std::string& body,const std::string& extra=""){
      return "<div style=\"position:absolute;box-sizing:border-box;left:"+number(x)+"px;top:"+number(y)+"px;width:"+number(width)+"px;height:"+number(height)+"px;"+extra+"\">"+body+"</div>";
    };
    auto control=[&](const std::string& suffix,const std::string& label,Json action,double basis,bool enabled=true,bool primary=false){
      const auto bid=id+"-"+suffix;actions[bid]=action;labels[bid]=label;
      return "<button id=\""+escape(bid)+"\""+(enabled?"":" disabled=\"disabled\"")+" style=\"box-sizing:border-box;flex-grow:1;flex-shrink:0;flex-basis:"+number(basis)+"px;width:"+number(basis)+"px;max-width:100%;min-width:"+number(target)+"px;min-height:"+number(target)+"px;height:auto;margin:0 12px 12px 0;padding:8px 12px;font-size:20px;line-height:28px;white-space:normal;word-break:break-word;text-align:center;border-width:3px;border-radius:3px;color:"+(primary&&enabled?paper:ink)+";background-color:"+(!enabled?"#E8E2D4":primary?(suffix=="confirm"&&slot_confirmation=="save"?std::string("#8A3A4A"):accent):paper)+";\">"+escape(label)+"</button>";
    };
    const auto row=[](const std::string& content){return "<div style=\"display:flex;flex-wrap:wrap;align-items:stretch;flex-shrink:0;width:100%;\">"+content+"</div>";};
    const auto paragraph=[&](const std::string& text,const std::string& style=""){return "<p style=\"margin:0 0 12px;white-space:normal;word-break:break-word;line-height:28px;"+style+"\">"+escape(text)+"</p>";};
    std::string out="<div id=\""+escape(id)+"-content\" style=\"box-sizing:border-box;width:100%;height:100%;padding-right:20px;overflow-x:hidden;overflow-y:auto;display:flex;flex-direction:column;color:"+ink+";\">";
    if(!slot_confirmation.empty()){
      const auto selected=std::find_if(save_slots.begin(),save_slots.end(),[&](const Json& r){return r.at("automatic")==slot_auto&&r.at("index")==slot_selected;});
      const auto selected_label=(slot_auto?slot_words("자동 기록 ","Auto ","自動記録 "):slot_words("기록 ","Record ","記録 "))+std::to_string(selected==save_slots.end()?slot_selected+1:selected->value("displayNumber",slot_selected+1));
      const auto title=slot_confirmation=="save"?selected_label+slot_words("을 지금의 진행으로 덮어쓸까요?",": overwrite with current progress?","を現在の進行で上書きしますか？"):slot_words("이날의 이야기로 돌아갈까요?","Return to this moment?","この日の物語に戻りますか？");
      const auto warning=slot_confirmation=="save"?slot_words("선택한 기록이 지금의 진행으로 바뀝니다.","The selected record will be replaced by your current progress.","選んだ記録を現在の進行で上書きします。"):slot_words("저장하지 않은 진행은 사라집니다.","Unsaved progress will be lost.","保存していない進行は失われます。");
      out+="<div style=\"flex-shrink:0;padding:16px;\">"+paragraph(title,"font-size:28px;line-height:36px;");
      if(selected!=save_slots.end()){
        const auto record=selected->value("record",Json::object());const auto thumbnail=selected->value("thumbnail","");
        out+="<div style=\"padding:12px;margin-bottom:12px;border-width:1px;border-color:"+line+";background-color:#E8E2D4;\">";
        if(!thumbnail.empty())out+="<img src=\""+escape(thumbnail)+"\" style=\"width:181px;height:102px;max-width:100%;margin-bottom:12px;\"/>";
        out+=paragraph((slot_confirmation=="save"?slot_words("덮어쓸 ","Replacing ","上書きする "):std::string())+selected_label,"font-size:22px;");
        out+=paragraph(slot_date(*selected)+" · "+record.value("place","")+"\n"+slot_words("남긴 시각  ","Saved  ","保存日時  ")+record.value("savedAt",""),"font-size:18px;")+"</div>";
      }
      if(!current_save_record.empty())out+=paragraph((slot_confirmation=="save"?slot_words("새로 남길 진행 · ","Saving now · ","新しく保存する進行 · "):slot_words("현재 진행 · ","Current progress · ","現在の進行 · "))+current_save_record.value("date","")+" · "+current_save_record.value("place",""),"font-size:18px;");
      out+=paragraph(warning,"font-size:18px;color:#8A3A4A;");
      // Cancel remains the first focusable control, even when the flow scrolls.
      out+=row(control("cancel",slot_words("취소 · 기록 목록으로","Cancel · back to records","キャンセル · 記録へ"),{{"type","cancelSlot"}},180)+control("confirm",selected_label+(slot_confirmation=="save"?slot_words("에 덮어쓰기"," · overwrite","に上書き"):slot_words(" 불러오기"," · load","を読み込む")),{{"type","confirmSlot"}},180,true,true));
      return out+"</div></div>";
    }
    std::string toolbar=control("manual",slot_words("내가 남긴 기록","Manual records","手動の記録"),{{"type","slotGroup"},{"automatic",false}},210,true,!slot_auto)+control("auto",slot_words("자동으로 남긴 기록","Automatic records","自動の記録"),{{"type","slotGroup"},{"automatic",true}},228,true,slot_auto);
    const auto hint=slot_auto?slot_words("이야기의 갈림길과 일정 진행 뒤 · 최근 기록부터","After choices and schedule progress · newest first","選択や予定の進行後 · 新しい順"):saving?slot_words("마음에 남은 순간을 따로 간직하세요","Keep the moments you want to return to","戻りたい瞬間を残しましょう"):slot_words("이야기를 이어 갈 기록을 골라 주세요","Choose a record to continue your story","物語を続ける記録を選んでください");
    if(portable_available){
      toolbar+=control("portable-import",slot_words("백업 가져오기","Import backup","バックアップを読み込む"),{{"type","portableImport"},{"ownerId",id}},220);
      toolbar+=control("portable-export",slot_words("현재 진행 내보내기","Export current progress","現在の進行を書き出す"),{{"type","portableExport"},{"ownerId",id}},220,!game.state.is_null());
    }
    out+=row(toolbar);
    if(!portable_available)out+="<div style=\"flex-shrink:0;\">"+paragraph(hint,"font-size:19px;")+"</div>";
    std::vector<Json> rows;for(const auto& r:save_slots)if(r.at("automatic")==slot_auto)rows.push_back(r);
    if(slot_auto)std::stable_sort(rows.begin(),rows.end(),[](const Json& a,const Json& b){return a.value("record",Json::object()).value("sequence",int64_t(0))>b.value("record",Json::object()).value("sequence",int64_t(0));});
    const int pages=std::max(1,int((rows.size()+5)/6));slot_page=std::clamp(slot_page,0,pages-1);
    out+="<div id=\""+escape(id)+"-cards\" style=\"box-sizing:border-box;flex-grow:1;flex-shrink:1;flex-basis:0px;min-height:"+number(target)+"px;padding-right:20px;overflow-x:hidden;overflow-y:auto;pointer-events:auto;\"><div style=\"position:relative;height:"+number(std::ceil(std::min(6,int(rows.size())-slot_page*6)/double(columns))*(cardh+gap)-gap)+"px;width:100%;\">";
    for(int n=slot_page*6;n<std::min(int(rows.size()),slot_page*6+6);++n){
      const auto& r=rows[n];const int i=r.at("index"),cell=n%6;const double x=(cell%columns)*(cardw+gap),y=(cell/columns)*(cardh+gap);
      const auto status=r.value("status","empty");const bool selected=slot_selected==i,ready=status=="ready",empty=status=="empty";
      const auto label=(slot_auto?slot_words("자동 기록 ","Auto ","自動 "):slot_words("기록 ","Record ","記録 "))+std::to_string(r.value("displayNumber",i+1));
      auto bid=id+"-slot-"+std::to_string(i);actions[bid]={{"type","selectSaveSlot"},{"index",i},{"automatic",slot_auto}};labels[bid]=label;
      std::string card="<button id=\""+escape(bid)+"\" class=\""+(selected?"slot-selected":"slot-card")+"\" style=\"position:absolute;width:100%;height:100%;box-sizing:border-box;margin:0;padding:0;border-radius:3px;background-color:"+paper+";color:"+ink+";\"></button>";
      const auto thumb=r.value("thumbnail","");
      const double tw=std::max(40.0,std::min((cardh-20)*16/9,cardw-190)),th=tw*9/16,tx=tw+26;
      if(!thumb.empty())card+=box(10,10,tw,th,"<img src=\""+escape(thumb)+"\" style=\"width:100%;height:100%;\"/>","pointer-events:none;overflow:hidden;");
      else card+=box(10,10,tw,th,escape(empty?slot_words("아직 비어 있는\n한 장","An unwritten\npage","まだ白紙の\n一頁"):slot_words("화면 기록 없음","No captured image","画面の記録なし")),"pointer-events:none;background-color:#E8E2D4;text-align:center;padding-top:24px;font-size:17px;line-height:24px;");
      std::string detail=empty?slot_words("새 이야기를 남길 자리","A place for a new story","新しい物語を残す場所"):slot_date(r);
      if(status=="invalid")detail=slot_words("손상된 기록 · 원본 보존","Unreadable · original kept","読めない記録 · 原本を保持");
      if(status=="incompatible")detail=slot_words("다른 버전 · 불러오기 불가","Different version · cannot load","異なる版 · 読み込み不可");
      const auto place=r.value("record",Json::object()).value("place","");
      card+=box(tx,7,cardw-tx-12,27,escape(label+(selected?slot_words(" · 선택됨"," · selected"," · 選択中"):"")),"pointer-events:none;font-size:20px;line-height:26px;color:"+card_accent+";");
      card+=box(tx,36,cardw-tx-12,24,escape(detail),"pointer-events:none;font-size:19px;line-height:23px;overflow:hidden;color:"+card_ink+";");
      card+=box(tx,60,cardw-tx-12,24,escape(place+(slot_auto&&n==0&&!empty?slot_words(" · 가장 최근"," · latest"," · 最新"):"")),"pointer-events:none;font-size:19px;line-height:23px;overflow:hidden;color:"+card_ink+";");
      card+=box(tx,84,cardw-tx-12,24,escape(r.value("record",Json::object()).value("savedAt","")),"pointer-events:none;font-size:19px;line-height:23px;overflow:hidden;color:"+card_ink+";");
      out+=box(x,y,cardw,cardh,card);
    }
    out+="</div></div>";
    const auto chosen=std::find_if(save_slots.begin(),save_slots.end(),[&](const Json& r){return r.at("automatic")==slot_auto&&r.at("index")==slot_selected;});
    const bool enabled=chosen!=save_slots.end()&&(saving?(!slot_auto&&!game.state.is_null()):chosen->value("status","")=="ready");
    std::string title=saving?(slot_auto?slot_words("자동 기록은 직접 바꿀 수 없습니다","Automatic records are protected","自動記録は上書きできません"):slot_words("선택한 곳에 기록하기","Save in selected slot","選んだ枠に保存")):slot_words("선택한 기록 이어 읽기","Continue selected record","選んだ記録から続ける");
    if(saving&&!slot_auto&&chosen!=save_slots.end())title=slot_words("기록 ","Record ","記録 ")+std::to_string(slot_selected+1)+(chosen->value("status","")=="empty"?slot_words("에 새로 남기기"," · save new","に新しく保存"):slot_words(" 덮어쓰기 확인"," · confirm overwrite"," · 上書きを確認"));
    std::string footer;
    if(pages>1){footer+=control("prev","‹",{{"type","slotPage"},{"delta",-1}},target,slot_page>0);footer+="<span style=\"padding:12px;\">"+std::to_string(slot_page+1)+" / "+std::to_string(pages)+"</span>";footer+=control("next","›",{{"type","slotPage"},{"delta",1}},target,slot_page+1<pages);}
    footer+=control("request",title,{{"type",saving?"requestSlotSave":"requestSlotLoad"}},360,enabled,true);
    out+="<div style=\"flex-shrink:0;margin-top:14px;\">"+row(footer);
    if(chosen!=save_slots.end()){const auto date=chosen->value("record",Json::object()).value("savedAt","");if(!date.empty())out+=paragraph(slot_words("남긴 시각  ","Recorded  ","保存時刻  ")+date,"font-size:19px;");}
    return out+"</div></div>";
  }
