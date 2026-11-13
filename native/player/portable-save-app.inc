  // This host owns every state transition. UIKit callbacks contain only owned
  // file leases/results and are polled on the player thread while foregrounded.
  std::unique_ptr<raising_player::PortableSaveFiles> portable_files;
  std::shared_ptr<raising_player::PortableSaveStagedFile> portable_file;
  uint64_t portable_serial=0,portable_request=0;
  Json portable_token=nullptr;
  std::string portable_owner,portable_bytes,portable_return_focus,portable_focus_pending;
  fs::path portable_staging_root() const {return save_file.parent_path()/"portable-staging";}
  bool portable_active() const {return markup.portable_open();}
  Json portable_identity() const {
    return {{"generation",markup.reuse_generation},{"fingerprint",game.content.at("contentFingerprint")},
      {"state",game.state},{"commands",game.commands.size()},{"screen",markup.screen_id},{"modal",markup.ui_modal},
      {"draft",markup.plan_save()},{"roster",markup.roster_draft},{"userDraft",markup.user_values_draft},
      {"selections",markup.ui_selections},{"quantities",markup.quantities},{"locale",markup.locale},
      {"timeline",game.timeline_cursor},{"direction",game.direction_cursor},{"battleSelection",game.battle_selection}};
  }
  bool portable_owner_available(const std::string& id,bool fresh=false) {
    if(preview()||editor_live||recalling()||game.gallery.active||!markup.slot_confirmation.empty()||!markup.pending_purchase.is_null())return false;
    // Re-resolve the authored owner only at terminal/confirm boundaries. The
    // overlay has its own render list, so it cannot stand in for its owner.
    if(fresh){
      ScreenMarkup probe(game,markup.assets,markup.bundle);probe.locale=markup.locale;probe.ui_modal=markup.ui_modal;
      probe.viewport_width=markup.viewport_width;probe.viewport_height=markup.viewport_height;probe.window_width=markup.window_width;probe.window_height=markup.window_height;probe.safe_area=markup.safe_area;probe.player_preferences=markup.player_preferences;probe.planning_guide_suppressed=true;
      probe.document();
      for(const auto& e:probe.render_elements)if(e.value("id","")==id&&(e.value("kind","")=="saveSlots"||e.value("kind","")=="loadSlots"))return (markup.ui_modal.empty()?!e.value("_authoredModal",false):e.value("_authoredModal",false))&&probe.visible(e)&&e.value("_boundEnabled",true)&&(!e.contains("enabledCondition")||probe.visual_condition(e.at("enabledCondition")))&&e.value("opacity",1.0)>0;
      return false;
    }
    // The entry point must be a live, visible record widget. Merely submitting
    // the private action name does not grant access from another screen.
    for(const auto& e:markup.render_elements)if(e.value("id","")==id&&(e.value("kind","")=="saveSlots"||e.value("kind","")=="loadSlots"))
      return (markup.ui_modal.empty()?!e.value("_authoredModal",false):e.value("_authoredModal",false))&&markup.visible(e)&&e.value("_boundEnabled",true)&&(!e.contains("enabledCondition")||markup.visual_condition(e.at("enabledCondition")))&&e.value("opacity",1.0)>0;
    return false;
  }
  bool portable_current(bool owner=false) {return portable_active()&&portable_token==portable_identity()&&(!owner||portable_owner_available(portable_owner,true));}
  std::string portable_read_bytes(const fs::path& file) const {
    std::error_code error;
    if(fs::is_symlink(fs::symlink_status(file,error))||error||!fs::is_regular_file(file,error)||error)throw raising_player::portable_input::Error(raising_player::portable_input::Code::unreadable);
    std::ifstream input(file,std::ios::binary);if(!input)throw raising_player::portable_input::Error(raising_player::portable_input::Code::unreadable);
    return raising_player::portable_input::bounded_read(input);
  }
  std::shared_ptr<raising_player::PortableSaveStagedFile> portable_lease(const std::string& purpose) {
    return raising_player::PortableSaveStagedFile::create(portable_staging_root(),purpose+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(++portable_serial));
  }
  void portable_write(const fs::path& path,const std::string& bytes) const {
    std::ofstream stream(path,std::ios::binary|std::ios::trunc);stream.write(bytes.data(),std::streamsize(bytes.size()));stream.flush();if(!stream)throw std::runtime_error("write-failed");
  }
  Json portable_metadata(const Json& state) const {
    if(state.is_null())return Json::object();const auto& project=markup.display_content.at("project");
    const int index=state.value("periodIndex",0),month=project.value("startMonth",1)-1+index/game.periods();
    const auto date=project.contains("calendar")?raising::dates::text(project,index,markup.locale):std::to_string(project.value("startYear",1)+month/12)+" / "+std::to_string(month%12+1)+" · "+std::to_string(index%game.periods()+1);
    std::string place;for(const auto& location:project.value("gameplay",Json::object()).value("locations",Json::array()))if(location.at("id")==state.value("gameplay",Json::object()).value("locationId",""))place=location.value("name","");
    return {{"date",date},{"place",place},{"phase",markup.slot_phase(state.value("phase",""))}};
  }
  void portable_finish(const std::string& notice,bool restore_focus=true) {
    if(portable_files&&portable_files->busy())portable_files->cancel(portable_request);
    markup.portable_status="idle";markup.portable_notice=notice;markup.portable_incoming=Json::object();markup.portable_current=Json::object();markup.portable_filename.clear();
    portable_file.reset();portable_bytes.clear();portable_token=nullptr;portable_owner.clear();
    if(!notice.empty())game.message=notice;
    if(restore_focus){reading_focus=portable_return_focus;portable_focus_pending=portable_return_focus;}portable_return_focus.clear();dirty=true;
  }
  std::string portable_failure(const std::string& code) const {
    if(code=="stale")return markup.settings_words("진행이나 화면이 바뀌었습니다. 백업을 다시 선택하세요.","Progress or the screen changed. Choose the backup again.","進行または画面が変わりました。バックアップを選び直してください。");
    if(code=="too-large")return markup.settings_words("백업은 16 MiB 이하여야 합니다.","Backups must be no larger than 16 MiB.","バックアップは16 MiB以下にしてください。");
    if(code=="changed")return markup.settings_words("검토 중 백업 파일이 바뀌었습니다. 다시 선택하세요.","The backup changed during review. Choose it again.","確認中にバックアップが変わりました。選び直してください。");
    if(code=="incompatible")return markup.settings_words("이 게임에서 검증할 수 없는 백업입니다. 현재 진행은 그대로입니다.","This backup could not be validated for this game. Current progress is unchanged.","このゲームで検証できないバックアップです。現在の進行は変わりません。");
    return markup.settings_words("백업 작업을 완료하지 못했습니다. 파일을 확인하고 다시 시도하세요.","The backup operation could not finish. Check the file and try again.","バックアップ操作を完了できませんでした。ファイルを確認して再試行してください。");
  }
  void portable_begin(bool importing,const std::string& owner) {
    if(!portable_files||!portable_files->available()||portable_active()||markup.reading_overlay()||backgrounded||!portable_owner_available(owner)||(!importing&&game.state.is_null()))return;
    portable_owner=owner;portable_return_focus=owner+(importing?"-portable-import":"-portable-export");
    portable_request=++portable_serial;portable_token=portable_identity();markup.portable_current=portable_metadata(game.state);
    markup.portable_incoming=Json::object();markup.portable_filename.clear();markup.portable_notice.clear();markup.portable_status=importing?"picking-import":"picking-export";
    pending.clear();extension_dt=0;reading_focus="engine-portable-cancel";dirty=true;
    try {
      bool accepted=false;
      if(importing)accepted=portable_files->begin_import(portable_request);
      else {
        auto snapshot=portable_lease("export");const auto message=game.message;
        try{game.save(snapshot->path);}catch(...){game.message=message;throw;}game.message=message;
        // One immutable snapshot of current progress, never the selected slot.
        portable_read_bytes(snapshot->path);accepted=portable_files->begin_export(portable_request,snapshot->path);
      }
      if(!accepted)portable_finish(portable_failure("unavailable"));
    }catch(const raising_player::portable_input::Error& e){portable_finish(portable_failure(raising_player::portable_input::code_name(e.code)));}
    catch(...){portable_finish(portable_failure("unavailable"));}
  }
  void poll_portable() {
    if(backgrounded||!portable_files)return;
    if(portable_active()&&!portable_current()){portable_finish(portable_failure("stale"));}
    while(auto result=portable_files->poll()) {
      if(!portable_active()||result->requestId!=portable_request)continue;
      const bool importing=markup.portable_status=="picking-import";
      if((importing&&result->kind!=raising_player::PortableSaveKind::import_file)||(!importing&&result->kind!=raising_player::PortableSaveKind::export_file))continue;
      if(!portable_current(true)){portable_finish(portable_failure("stale"));continue;}
      if(result->status==raising_player::PortableSaveStatus::cancelled){portable_finish(markup.settings_words("백업 작업을 취소했습니다.","Backup operation cancelled.","バックアップ操作をキャンセルしました。"));continue;}
      if(result->status==raising_player::PortableSaveStatus::error){portable_finish(portable_failure(result->error));continue;}
      if(!importing){portable_finish(markup.settings_words("현재 진행의 백업을 내보냈습니다.","Exported a backup of current progress.","現在の進行のバックアップを書き出しました。"));continue;}
      try {
        if(!result->file)throw std::runtime_error("No staged file");
        auto bytes=portable_read_bytes(result->file->path);raising_player::portable_input::parse(bytes);
        // Provider files never reach GameSession. Preflight a separately owned
        // byte-for-byte copy with no host observers, callbacks or profile writes.
        auto owned=portable_lease("review");portable_write(owned->path,bytes);
        GameSession candidate;candidate.content=game.content;candidate.gallery.data=game.gallery.data;
        candidate.gallery_pending=game.gallery_pending;candidate.gallery_manifest=game.gallery_manifest;candidate.gallery_recording=game.gallery_recording;candidate.gallery_persistence=false;
        const auto localized=markup.display_content;candidate.gallery_content=[localized]{return localized;};
        candidate.load(owned->path);
        if(!portable_current(true)){portable_finish(portable_failure("stale"));continue;}
        portable_file=std::move(owned);portable_bytes=std::move(bytes);markup.portable_incoming=portable_metadata(candidate.state);
        markup.portable_filename=result->displayName.empty()?"Backup":result->displayName;
        markup.portable_status="review";reading_focus="engine-portable-cancel";dirty=true;
      }catch(const raising_player::portable_input::Error& e){portable_finish(portable_failure(raising_player::portable_input::code_name(e.code)));}
      catch(...){portable_finish(portable_failure("incompatible"));}
    }
  }
  bool portable_action(const Json& action) {
    const auto type=action.value("type","");
    if(type=="portableImport"||type=="portableExport"){if(!portable_active())portable_begin(type=="portableImport",action.value("ownerId",""));return true;}
    if(!portable_active())return type=="portableConfirm"||type=="portableCancel";
    // Consume all queued gameplay and other modal actions throughout the transaction.
    if(backgrounded)return true;
    if(type=="portableCancel"||type=="uiBack"){portable_finish(markup.settings_words("백업 작업을 취소했습니다.","Backup operation cancelled.","バックアップ操作をキャンセルしました。"));return true;}
    if(type!="portableConfirm"||markup.portable_status!="review")return true;
    if(!portable_current(true)){portable_finish(portable_failure("stale"));return true;}
    const auto accepted_generation=markup.reuse_generation;
    try {
      if(!portable_file||portable_read_bytes(portable_file->path)!=portable_bytes){portable_finish(portable_failure("changed"));return true;}
      // Load a fresh private snapshot of the exact reviewed bytes. This avoids a
      // second read of the review path between the equality check and restore.
      auto accepted=portable_lease("accepted");portable_write(accepted->path,portable_bytes);
      game.load(accepted->path);finish_loaded_slot();
      portable_finish("",false);game.message=markup.settings_words("백업에서 이어갑니다.","Continued from the backup.","バックアップから再開しました。");
    }catch(...){if(markup.reuse_generation==accepted_generation)portable_finish(portable_failure("incompatible"));else{portable_finish("",false);game.message=markup.settings_words("백업을 불러왔지만 화면을 준비하지 못했습니다.","The backup loaded, but its display could not finish.","バックアップを読み込みましたが、画面の準備を完了できませんでした。");}}
    return true;
  }
  void restore_portable_focus() {
    if(portable_focus_pending.empty())return;
    if(!markup.readable_active&&document)if(auto* target=document->GetElementById(portable_focus_pending);target&&!target->HasAttribute("disabled")){target->Focus();context->Update();}
    portable_focus_pending.clear();
  }
  Json portable_report() const {return {{"available",markup.portable_available},{"status",markup.portable_status},{"requestId",portable_request},{"incoming",markup.portable_incoming},{"current",markup.portable_current},{"filename",markup.portable_filename},{"notice",markup.portable_notice}};}
