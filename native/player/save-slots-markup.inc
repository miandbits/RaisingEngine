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
  std::string slots_body(const Json& e) {
    const bool saving=e.value("kind","")=="saveSlots";
    const std::string id=e.at("id"),ink=e.value("color","#263C36"),paper=e.value("fill","#F5EFDF"),line=e.value("borderColor","#C7B99A"),accent=e.value("hoverFill","#345C50");
    const double w=e.value("width",1100.0),h=e.value("height",530.0),gap=14,cardw=(w-gap)/2,cardh=(h-150)/3;
    auto box=[&](double x,double y,double width,double height,const std::string& body,const std::string& extra=""){
      return "<div style=\"position:absolute;box-sizing:border-box;left:"+number(x)+"px;top:"+number(y)+"px;width:"+number(width)+"px;height:"+number(height)+"px;"+extra+"\">"+body+"</div>";
    };
    auto control=[&](const std::string& suffix,const std::string& label,Json action,double x,double y,double width,bool enabled=true,bool primary=false){
      const auto bid=id+"-"+suffix;actions[bid]=action;labels[bid]=label;
      return box(x,y,width,42,"<button id=\""+escape(bid)+"\""+(enabled?"":" disabled=\"disabled\"")+" style=\"box-sizing:border-box;width:100%;height:100%;min-height:0;margin:0;padding:4px 12px;font-size:20px;line-height:28px;text-align:center;border-radius:3px;color:"+(primary&&enabled?paper:ink)+";background-color:"+(!enabled?"#E8E2D4":primary?(suffix=="confirm"&&slot_confirmation=="save"?std::string("#8A3A4A"):accent):paper)+";\">"+escape(label)+"</button>");
    };
    std::string out;
    const bool confirming=!slot_confirmation.empty();
    // Cancel is deliberately the first focusable control when confirmation is open.
    if(confirming){
      const auto selected=std::find_if(save_slots.begin(),save_slots.end(),[&](const Json& r){return r.at("automatic")==slot_auto&&r.at("index")==slot_selected;});
      const auto slot_label=(slot_auto?slot_words("자동 기록 ","Auto ","自動記録 "):slot_words("기록 ","Record ","記録 "))+std::to_string(selected==save_slots.end()?slot_selected+1:selected->value("displayNumber",slot_selected+1));
      out+=control("cancel",slot_words("취소 · 기록 목록으로","Cancel · back to records","キャンセル · 記録へ"),{{"type","cancelSlot"}},w/2-270,h/2+130,240,true);
      out+=control("confirm",slot_label+(slot_confirmation=="save"?slot_words("에 덮어쓰기"," · overwrite","に上書き"):slot_words(" 불러오기"," · load","を読み込む")),{{"type","confirmSlot"}},w/2-10,h/2+130,280,true,true);
      const auto title=slot_confirmation=="save"?slot_label+slot_words("을 지금의 진행으로 덮어쓸까요?",": overwrite with current progress?","を現在の進行で上書きしますか？"):slot_words("이날의 이야기로 돌아갈까요?","Return to this moment?","この日の物語に戻りますか？");
      const auto warning=slot_confirmation=="save"?slot_words("선택한 기록이 지금의 진행으로 바뀝니다.","The selected record will be replaced by your current progress.","選んだ記録を現在の進行で上書きします。"):slot_words("저장하지 않은 진행은 사라집니다.","Unsaved progress will be lost.","保存していない進行は失われます。");
      out+=box(40,h/2-150,w-80,44,escape(title),"font-size:28px;text-align:center;color:"+ink+";");
      if(selected!=save_slots.end()){
        const auto thumbnail=selected->value("thumbnail","");
        out+=box(w/2-290,h/2-86,580,122,"","background-color:#E8E2D4;border-width:1px;border-color:"+line+";");
        if(!thumbnail.empty())out+=box(w/2-280,h/2-76,181,102,"<img src=\""+escape(thumbnail)+"\" style=\"width:100%;height:100%;\"/>");
        out+=box(w/2-80,h/2-76,350,30,escape((slot_confirmation=="save"?slot_words("덮어쓸 ","Replacing ","上書きする "):std::string())+slot_label),"font-size:22px;color:"+(slot_confirmation=="save"?std::string("#8A3A4A"):accent)+";");
        const auto record=selected->value("record",Json::object());
        out+=box(w/2-80,h/2-42,350,74,escape(slot_date(*selected)+" · "+record.value("place","")+"\n"+slot_words("남긴 시각  ","Saved  ","保存日時  ")+record.value("savedAt","")),"font-size:18px;line-height:25px;color:"+ink+";");
      }
      if(!current_save_record.empty())out+=box(w/2-290,h/2+42,580,36,escape((slot_confirmation=="save"?slot_words("새로 남길 진행 · ","Saving now · ","新しく保存する進行 · "):slot_words("현재 진행 · ","Current progress · ","現在の進行 · "))+current_save_record.value("date","")+" · "+current_save_record.value("place","")),"font-size:18px;text-align:left;color:"+ink+";");
      out+=box(40,h/2+80,w-80,42,escape(warning),"font-size:18px;line-height:24px;text-align:center;color:#8A3A4A;");
      return out;
    }
    out+=control("manual",slot_words("내가 남긴 기록","Manual records","手動の記録"),{{"type","slotGroup"},{"automatic",false}},0,0,210,true,!slot_auto);
    out+=control("auto",slot_words("자동으로 남긴 기록","Automatic records","自動の記録"),{{"type","slotGroup"},{"automatic",true}},222,0,228,true,slot_auto);
    const auto hint=slot_auto?slot_words("이야기의 갈림길과 일정 진행 뒤 · 최근 기록부터","After choices and schedule progress · newest first","選択や予定の進行後 · 新しい順"):saving?slot_words("마음에 남은 순간을 따로 간직하세요","Keep the moments you want to return to","戻りたい瞬間を残しましょう"):slot_words("이야기를 이어 갈 기록을 골라 주세요","Choose a record to continue your story","物語を続ける記録を選んでください");
    out+=box(470,8,w-470,34,escape(hint),"font-size:19px;color:"+ink+";");
    std::vector<Json> rows;for(const auto& r:save_slots)if(r.at("automatic")==slot_auto)rows.push_back(r);
    if(slot_auto)std::stable_sort(rows.begin(),rows.end(),[](const Json& a,const Json& b){return a.value("record",Json::object()).value("sequence",int64_t(0))>b.value("record",Json::object()).value("sequence",int64_t(0));});
    const int pages=std::max(1,int((rows.size()+5)/6));slot_page=std::clamp(slot_page,0,pages-1);
    for(int n=slot_page*6;n<std::min(int(rows.size()),slot_page*6+6);++n){
      const auto& r=rows[n];const int i=r.at("index"),cell=n%6;const double x=(cell%2)*(cardw+gap),y=58+(cell/2)*(cardh+gap);
      const auto status=r.value("status","empty");const bool selected=slot_selected==i,ready=status=="ready",empty=status=="empty";
      const auto label=(slot_auto?slot_words("자동 기록 ","Auto ","自動 "):slot_words("기록 ","Record ","記録 "))+std::to_string(r.value("displayNumber",i+1));
      auto bid=id+"-slot-"+std::to_string(i);actions[bid]={{"type","selectSaveSlot"},{"index",i},{"automatic",slot_auto}};labels[bid]=label;
      std::string card="<button id=\""+escape(bid)+"\" class=\""+(selected?"slot-selected":"slot-card")+"\" style=\"position:absolute;width:100%;height:100%;box-sizing:border-box;margin:0;padding:0;border-radius:3px;background-color:"+paper+";color:"+ink+";\"></button>";
      const auto thumb=r.value("thumbnail","");
      const double th=cardh-20,tw=th*16/9,tx=tw+26;
      if(!thumb.empty())card+=box(10,10,tw,th,"<img src=\""+escape(thumb)+"\" style=\"width:100%;height:100%;\"/>","pointer-events:none;overflow:hidden;");
      else card+=box(10,10,tw,th,escape(empty?slot_words("아직 비어 있는\n한 장","An unwritten\npage","まだ白紙の\n一頁"):slot_words("화면 기록 없음","No captured image","画面の記録なし")),"pointer-events:none;background-color:#E8E2D4;text-align:center;padding-top:24px;font-size:17px;line-height:24px;");
      std::string detail=empty?slot_words("새 이야기를 남길 자리","A place for a new story","新しい物語を残す場所"):slot_date(r);
      if(status=="invalid")detail=slot_words("손상된 기록 · 원본 보존","Unreadable · original kept","読めない記録 · 原本を保持");
      if(status=="incompatible")detail=slot_words("다른 버전 · 불러오기 불가","Different version · cannot load","異なる版 · 読み込み不可");
      const auto place=r.value("record",Json::object()).value("place","");
      card+=box(tx,7,cardw-tx-12,27,escape(label+(selected?slot_words(" · 선택됨"," · selected"," · 選択中"):"")),"pointer-events:none;font-size:20px;line-height:26px;color:"+accent+";");
      card+=box(tx,36,cardw-tx-12,24,escape(detail),"pointer-events:none;font-size:19px;line-height:23px;overflow:hidden;");
      card+=box(tx,60,cardw-tx-12,24,escape(place+(slot_auto&&n==0&&!empty?slot_words(" · 가장 최근"," · latest"," · 最新"):"")),"pointer-events:none;font-size:19px;line-height:23px;overflow:hidden;");
      card+=box(tx,84,cardw-tx-12,24,escape(r.value("record",Json::object()).value("savedAt","")),"pointer-events:none;font-size:19px;line-height:23px;overflow:hidden;");
      out+=box(x,y,cardw,cardh,card);
    }
    const auto chosen=std::find_if(save_slots.begin(),save_slots.end(),[&](const Json& r){return r.at("automatic")==slot_auto&&r.at("index")==slot_selected;});
    bool enabled=chosen!=save_slots.end()&&(saving?(!slot_auto&&!game.state.is_null()):chosen->value("status","")=="ready");
    std::string title=saving?(slot_auto?slot_words("자동 기록은 직접 바꿀 수 없습니다","Automatic records are protected","自動記録は上書きできません"):slot_words("선택한 곳에 기록하기","Save in selected slot","選んだ枠に保存")):slot_words("선택한 기록 이어 읽기","Continue selected record","選んだ記録から続ける");
    if(saving&&!slot_auto&&chosen!=save_slots.end())title=slot_words("기록 ","Record ","記録 ")+std::to_string(slot_selected+1)+(chosen->value("status","")=="empty"?slot_words("에 새로 남기기"," · save new","に新しく保存"):slot_words(" 덮어쓰기 확인"," · confirm overwrite"," · 上書きを確認"));
    out+=control("request",title,{{"type",saving?"requestSlotSave":"requestSlotLoad"}},w-360,h-44,360,enabled,true);
    if(pages>1){out+=control("prev","‹",{{"type","slotPage"},{"delta",-1}},0,h-44,48,slot_page>0);out+=box(60,h-36,80,30,std::to_string(slot_page+1)+" / "+std::to_string(pages));out+=control("next","›",{{"type","slotPage"},{"delta",1}},145,h-44,48,slot_page+1<pages);}
    if(chosen!=save_slots.end()){const auto date=chosen->value("record",Json::object()).value("savedAt","");out+=box(pages>1?215:0,h-36,w-590,30,escape(date.empty()?"":slot_words("남긴 시각  ","Recorded  ","保存時刻  ")+date),"font-size:19px;color:"+ink+";");}
    return out;
  }
