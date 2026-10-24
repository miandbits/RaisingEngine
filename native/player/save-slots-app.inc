  std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> slot_frame{nullptr,SDL_DestroySurface};
  Json slot_frame_record=Json::object();
  int slot_auto_revision=-1;
  size_t slot_auto_commands=0;
  std::string slot_auto_phase;
  std::string slot_expected_bytes;
  bool slots_enabled() const {return has_save_slots(game.content.at("project").at("visual"));}
  Json current_slot_record() const {
    if(game.state.is_null())return Json::object();
    const auto& project=game.content.at("project");const int p=game.state.value("periodIndex",0),m=project.value("startMonth",1)-1+p/game.periods();
    std::string place;
    for(const auto& s:project.at("visual").at("screens"))if(s.at("id")==markup.screen_id)place=s.value("label",markup.screen_id);
    if(place.empty())for(const auto& l:project.value("gameplay",Json::object()).value("locations",Json::array()))if(l.at("id")==game.state.value("gameplay",Json::object()).value("locationId",""))place=l.value("name","");
    const auto date=project.contains("calendar")?raising::dates::text(markup.display_content.at("project"),p,markup.locale):std::to_string(project.value("startYear",1)+m/12)+markup.slot_words("년 "," / ","年 ")+std::to_string(m%12+1)+markup.slot_words("월 · "," · Week ","月 · 第")+std::to_string(p%game.periods()+1)+markup.slot_words("주","","週");
    return {{"date",date},{"place",place},{"screenId",markup.screen_id},{"phase",game.state.value("phase","")}};
  }
  void capture_slot_frame(bool rendered=false) {
    if(!slots_enabled()||game.state.is_null()||!markup.ui_modal.empty()||!markup.pending_purchase.is_null()||!document)return;
    // Present invalidates the backbuffer on some SDL drivers. Repaint the last
    // gameplay document before capturing an input-triggered modal transition.
    if(!rendered&&context){
      auto* render=static_cast<RenderInterface_SDL*>(Rml::GetRenderInterface());
      SDL_SetRenderDrawColor(renderer,19,35,28,255);SDL_RenderClear(renderer);
      render->BeginFrame();context->Render();render->EndFrame();
    }
    slot_frame.reset(SDL_RenderReadPixels(renderer,nullptr));slot_frame_record=current_slot_record();markup.current_save_record=slot_frame_record;
  }
  void refresh_slot_catalog() {
    if(!slots_enabled())return;
    markup.save_slots=Json::array();const auto& visual=game.content.at("project").at("visual");
    for(bool automatic:{false,true})for(int i=0;i<slot_count(visual,automatic);++i){
      auto row=slot_preview(slot_path(save_file,automatic,i),game.content);row["automatic"]=automatic;row["index"]=i;
      const auto thumbnail=row.value("record",Json::object()).value("thumbnail","");
      // Only engine-generated basenames are admitted; never resolve saved paths.
      if(thumbnail.size()<120&&thumbnail.rfind("frame-",0)==0&&thumbnail.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-.")==std::string::npos&&fs::path(thumbnail).extension()==".png"){
        const auto path=slot_directory(save_file)/thumbnail;
        if(fs::is_regular_file(path))row["thumbnail"]=path.generic_u8string();
      }
      markup.save_slots.push_back(std::move(row));
    }
    std::vector<size_t> automatic;
    for(size_t n=0;n<markup.save_slots.size();++n)if(markup.save_slots[n].at("automatic").get<bool>())automatic.push_back(n);
    std::stable_sort(automatic.begin(),automatic.end(),[&](size_t a,size_t b){return markup.save_slots[a].value("record",Json::object()).value("sequence",int64_t(0))>markup.save_slots[b].value("record",Json::object()).value("sequence",int64_t(0));});
    for(size_t n=0;n<automatic.size();++n)markup.save_slots[automatic[n]]["displayNumber"]=n+1;
  }
  void write_slot(bool automatic,int index) {
    const auto& visual=game.content.at("project").at("visual");
    if(index<0||index>=slot_count(visual,automatic))throw std::runtime_error("Invalid save slot");
    if(game.state.is_null())throw std::runtime_error("저장할 게임이 없습니다.");
    if(!slot_frame)throw std::runtime_error("저장할 화면을 준비하지 못했습니다. 기록 창을 닫고 다시 열어 주세요.");
    const auto old_notice=game.message;const auto dir=slot_directory(save_file);fs::create_directories(dir);
    int64_t sequence=1;for(const auto& row:markup.save_slots)sequence=std::max(sequence,row.value("record",Json::object()).value("sequence",int64_t(0))+1);
    const auto ticks=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const auto thumbnail="frame-"+std::to_string(ticks)+"-"+std::to_string(sequence)+".png";
    const auto path=dir/thumbnail;
    auto* small=SDL_CreateSurface(480,270,SDL_PIXELFORMAT_RGBA32);
    if(!small)throw std::runtime_error(SDL_GetError());
    SDL_SetSurfaceBlendMode(slot_frame.get(),SDL_BLENDMODE_NONE);SDL_ClearSurface(small,0,0,0,1);
    const double scale=std::min(480.0/slot_frame->w,270.0/slot_frame->h);
    SDL_Rect destination{int((480-slot_frame->w*scale)/2),int((270-slot_frame->h*scale)/2),int(slot_frame->w*scale),int(slot_frame->h*scale)};
    const bool scaled=SDL_BlitSurfaceScaled(slot_frame.get(),nullptr,small,&destination,SDL_SCALEMODE_LINEAR);
    const bool written=scaled&&IMG_SavePNG(small,path.u8string().c_str());SDL_DestroySurface(small);
    if(!written){std::error_code ec;fs::remove(path,ec);throw std::runtime_error("저장 화면을 기록하지 못했습니다.");}
    auto record=slot_frame_record;record["version"]=1;record["sequence"]=sequence;record["thumbnail"]=thumbnail;record["automatic"]=automatic;
    std::time_t now=std::time(nullptr);std::tm local{};
#ifdef _WIN32
    localtime_s(&local,&now);
#else
    localtime_r(&now,&local);
#endif
    std::ostringstream stamp;stamp<<std::put_time(&local,"%Y-%m-%d %H:%M:%S");record["savedAt"]=stamp.str();
    try {game.save(slot_path(save_file,automatic,index),record);}catch(...){std::error_code ec;fs::remove(path,ec);throw;}
    refresh_slot_catalog();refresh_save_preview();
    if(automatic)game.message=old_notice;
    else {game.message=markup.slot_words("기록 ","Record ","記録 ")+std::to_string(index+1)+markup.slot_words("에 지금의 이야기를 남겼습니다."," saved.","に現在の物語を保存しました。");recording.checkpoint(game);}
    // Keep screenshots referenced by current records and atomic-save backups.
    try {
      std::set<std::string> keep;bool uncertain=false;
      auto preserve=[&](const fs::path& f){try{if(fs::exists(f)){const auto j=read_json(f);keep.insert(j.value("playerPresentation",Json::object()).value("saveRecord",Json::object()).value("thumbnail",""));}}catch(...){uncertain=true;}};
      preserve(save_file);auto backup=save_file;backup+=".bak";preserve(backup);
      // Include dormant slots when authors reduce the configured slot count.
      for(const auto& entry:fs::directory_iterator(dir))if(entry.is_regular_file()){
        const auto name=entry.path().filename().u8string();if((name.rfind("manual-",0)==0||name.rfind("auto-",0)==0)&&(entry.path().extension()==".json"||entry.path().extension()==".bak"))preserve(entry.path());
      }
      if(!uncertain)for(const auto& entry:fs::directory_iterator(dir))if(entry.is_regular_file()){
        const auto name=entry.path().filename().u8string();if(name.rfind("frame-",0)==0&&entry.path().extension()==".png"&&!keep.count(name)){std::error_code ec;fs::remove(entry.path(),ec);}
      }
    }catch(...){/* Best-effort pruning must not turn a committed save into a failure. */}
  }
  void after_slot_frame() {
    if(!slots_enabled()||preview()||recalling()||game.gallery.active||game.state.is_null()||!markup.ui_modal.empty()||!markup.pending_purchase.is_null())return;
    const int revision=game.state.value("stateRevision",0);if(revision==slot_auto_revision)return;
    // settle() may append automatic effect/branch commands after a submitted
    // choice. Preserve that choice checkpoint even when the last command is a
    // narrative advance, while still avoiding saves on ordinary dialogue lines.
    bool chose=false;
    for(size_t i=std::min(slot_auto_commands,game.commands.size());i<game.commands.size();++i)
      if(game.commands[i].value("type","")=="choose")chose=true;
    slot_auto_commands=game.commands.size();
    const auto previous=slot_auto_revision;slot_auto_revision=revision;
    const auto previous_phase=slot_auto_phase;slot_auto_phase=game.state.value("phase","");
    const auto& visual=game.content.at("project").at("visual");const int count=slot_count(visual,true);if(!count)return;
    const auto command=game.commands.empty()?std::string("start"):game.commands.back().value("type","");
    // Pure screen/location navigation is not a gameplay checkpoint.
    if(previous>=0&&(command=="navigate"||command=="visit"))return;
    // Preserve a choice checkpoint through subsequent dialogue lines. Save again
    // when the story finishes, rather than replacing the ring on every sentence.
    if(previous>=0&&!chose&&command=="advanceNarrative"&&slot_auto_phase==previous_phase)return;
    try {
      capture_slot_frame(true);refresh_slot_catalog();int target=0;int64_t oldest=INT64_MAX;
      for(const auto& row:markup.save_slots)if(row.at("automatic").get<bool>()){const auto seq=row.value("record",Json::object()).value("sequence",int64_t(0));if(seq<oldest){oldest=seq;target=row.at("index");}}
      write_slot(true,target);
    }catch(const std::exception& error){game.message=std::string("자동 저장 실패: ")+error.what();}
  }
  bool slot_action(const Json& action) {
    if(!slots_enabled())return false;
    const auto type=action.value("type","");
    if(type=="save"||type=="load"){
      capture_slot_frame();refresh_slot_catalog();markup.slot_confirmation.clear();markup.slot_auto=false;markup.slot_page=0;markup.slot_selected=0;
      const auto kind=type=="save"?"saveSlots":"loadSlots";
      for(const auto& screen:game.content.at("project").at("visual").at("screens"))if(screen.value("modal",false))for(const auto& e:screen.at("elements"))if(e.value("kind","")==kind){markup.ui_modal=screen.at("id");dirty=true;return true;}
      throw std::runtime_error("저장 목록 화면을 프로젝트에 연결해 주세요.");
    }
    if(type=="cancelSlot"){markup.slot_confirmation.clear();dirty=true;return true;}
    if(type=="slotGroup"){markup.slot_confirmation.clear();markup.slot_auto=action.value("automatic",false);markup.slot_selected=0;markup.slot_page=0;dirty=true;return true;}
    if(type=="slotPage"){markup.slot_page+=action.value("delta",0);dirty=true;return true;}
    if(type=="selectSaveSlot"){
      const int i=action.value("index",-1);const bool a=action.value("automatic",false);
      if(i<0||i>=slot_count(game.content.at("project").at("visual"),a))throw std::runtime_error("Invalid save slot");
      markup.slot_selected=i;markup.slot_auto=a;markup.slot_confirmation.clear();dirty=true;return true;
    }
    if(type!="requestSlotSave"&&type!="requestSlotLoad"&&type!="confirmSlot")return false;
    const bool saving=type=="requestSlotSave"||(type=="confirmSlot"&&markup.slot_confirmation=="save");
    if(type=="confirmSlot"&&markup.slot_confirmation.empty())return true;
    refresh_slot_catalog();const auto file=slot_path(save_file,markup.slot_auto,markup.slot_selected);const auto row=slot_preview(file,game.content);
    if(saving&&markup.slot_auto)throw std::runtime_error("자동 기록은 직접 덮어쓸 수 없습니다.");
    if(!saving&&row.value("status","")!="ready")throw std::runtime_error("이 기록은 불러올 수 없습니다.");
    if(type=="confirmSlot"){
      std::ifstream in(file,std::ios::binary);const std::string now((std::istreambuf_iterator<char>(in)),{});
      if(now!=slot_expected_bytes){markup.slot_confirmation.clear();dirty=true;throw std::runtime_error("확인하는 동안 기록이 바뀌었습니다. 목록에서 다시 선택해 주세요.");}
    }
    if(type!="confirmSlot"&&(!saving||row.value("status","")!="empty")){
      std::ifstream in(file,std::ios::binary);slot_expected_bytes=std::string((std::istreambuf_iterator<char>(in)),{});
      markup.slot_confirmation=saving?"save":"load";dirty=true;return true;
    }
    if(saving)write_slot(false,markup.slot_selected);
    else {
      game.load(file);game.message=markup.settings_words("저장한 지점에서 이어갑니다.","Continued from your saved game.","保存したところから再開します。");dialogue_key.clear();markup.roster_revision=-1;auto_attempted=-1;auto_revision=-1;slot_auto_revision=game.state.value("stateRevision",0);slot_auto_commands=game.commands.size();slot_auto_phase=game.state.value("phase","");
      markup.tactics_skill.clear();markup.tactics_actor.clear();markup.ui_selections.clear();markup.ui_modal.clear();collection_scroll.clear();extensions.reset();extensions.pending_restore=game.loaded_extensions;
      markup.user_values_draft=Json::object();markup.quantities.clear();markup.plan_group.clear();markup.pending_purchase=nullptr;
      audio.sync_music(markup.asset(game.state.at("presentation").value("musicAssetId",Json())),muted);
    }
    markup.slot_confirmation.clear();dirty=true;return true;
  }
