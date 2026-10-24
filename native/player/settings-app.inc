  Json preferences=default_player_preferences();
  fs::path preferences_file;
  bool preferences_enabled=false;
  std::string dialogue_key,dialogue_text;
  double dialogue_characters=0,dialogue_wait=0;
  std::string dialogue_rendered;
  uint64_t dialogue_render_generation=0;
  void apply_player_preferences(const Json& value) {
    const bool fullscreen=value.at("fullscreen");
    if(!editor_live&&bool(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)!=fullscreen&&!SDL_SetWindowFullscreen(window,fullscreen))throw std::runtime_error(SDL_GetError());
    audio.set_levels(value.at("musicVolume").get<float>()/100.f,value.at("effectsVolume").get<float>()/100.f);
    gallery_audio.set_levels(value.at("musicVolume").get<float>()/100.f,value.at("effectsVolume").get<float>()/100.f);
    markup.player_preferences=value;
  }
  void initialize_player_preferences() {
    preferences_enabled=has_player_settings(game.content.at("project").at("visual"));if(!preferences_enabled)return;
    preferences_file=save_file.parent_path()/("settings-"+game.content.at("project").value("gameId","game")+".json");
    try{if(fs::exists(preferences_file))preferences=checked_player_preferences(read_json(preferences_file));}
    catch(...){markup.settings_notice=markup.settings_words("설정 기록을 읽지 못해 기본값으로 열었습니다. 원본은 보존됩니다.","Settings could not be read. Defaults are active; the original file is preserved.","設定を読み込めなかったため初期値を使用します。元のファイルは保持されます。");}
    try{apply_player_preferences(preferences);}catch(...){preferences["fullscreen"]=false;apply_player_preferences(preferences);markup.settings_notice="전체화면을 적용하지 못해 창 화면으로 열었습니다.";}
    audio.sync_music(markup.asset(game.content.at("project").at("visual").value("defaultScene",Json::object()).value("musicAssetId",Json())),muted);
  }
  bool setting_action(const Json& action) {
    const bool toggle=action.value("type","")=="dialogueAuto";
    if(!toggle&&action.value("type","")!="setPlayerSetting")return false;
    if(!preferences_enabled||preview())return true;
    const auto key=toggle?std::string("autoAdvance"):action.value("key","");if(!preferences.contains(key)||key=="version")throw std::runtime_error("Unknown player setting");
    if(editor_live&&key=="fullscreen")return true; // The editor controls the embedded viewport.
    auto next=preferences;next[key]=toggle?Json(!preferences.value("autoAdvance",false)):action.at("value");next=checked_player_preferences(next);
    try{apply_player_preferences(next);write_player_preferences(preferences_file,next);preferences=next;dialogue_wait=0;markup.settings_notice.clear();}
    catch(...){apply_player_preferences(preferences);markup.settings_notice=markup.settings_words("설정을 저장하지 못했습니다. 이전 설정을 유지합니다.","Could not save settings. Previous values are retained.","設定を保存できませんでした。以前の設定を維持します。");}
    dirty=true;return true;
  }
  bool dialogue_active() const {
    const auto n=game.projection.value("narrative",Json());return preferences_enabled&&has_player_settings(game.content.at("project").at("visual"),false)&&!preview()&&!recalling()&&!game.gallery.active&&!game.state.is_null()&&game.state.value("phase","")=="narrative"&&n.is_object()&&n.at("node").value("type","")=="dialogue";
  }
  void sync_dialogue() {
    if(!dialogue_active()){dialogue_key.clear();dialogue_text.clear();dialogue_characters=dialogue_wait=0;return;}
    const auto text=markup.bind("story");
    const auto key=game.state.value("activeNarrative",Json()).dump()+":"+std::to_string(game.state.value("stateRevision",0))+":"+text;
    if(key!=dialogue_key){dialogue_key=key;dialogue_text=text;dialogue_characters=dialogue_wait=0;}
    if(preferences.at("textSpeed")==0)dialogue_characters=dialogue_text.size();
  }
  bool dialogue_complete() const {return dialogue_prefix_bytes(dialogue_text,size_t(dialogue_characters))==dialogue_text.size();}
  bool reveal_before_advance(const std::string& type) {
    if(type!="advance"&&type!="advanceNarrative")return false;
    sync_dialogue();if(dialogue_active()&&!dialogue_complete()){dialogue_characters=dialogue_text.size();dialogue_wait=0;return true;}return false;
  }
  void update_dialogue_visual() {
    sync_dialogue();if(!document||!dialogue_active())return;
    const size_t bytes=dialogue_prefix_bytes(dialogue_text,size_t(dialogue_characters));
    const auto value=escape(dialogue_text.substr(0,bytes))+"<span style=\"visibility:hidden;\">"+escape(dialogue_text.substr(bytes))+"</span>";
    if(value==dialogue_rendered&&dialogue_render_generation==document_generation)return;
    dialogue_rendered=value;dialogue_render_generation=document_generation;
    for(const auto& e:markup.render_elements)if(e.value("kind","")=="dialogue"||(e.value("kind","")=="text"&&e.value("binding","")=="story"))
      if(auto* element=document->GetElementById("node-"+e.value("id",""));element&&element->IsVisible(true))element->SetInnerRML(value);
    for(const auto& e:markup.render_elements)if(e.value("kind","")=="button"&&e.value("action","")=="advance"&&e.value("autoLabel",true)) {
      const auto id=e.value("id","");
      const auto label=dialogue_complete()?markup.settings_words("다음 문장","Next line","次の台詞"):markup.settings_words("끝까지 보기","Show full line","全文表示");
      if(auto* element=document->GetElementById(id);element&&element->IsVisible(true)){element->SetInnerRML(escape(label));markup.labels[id]=label;}
    }
  }
  void advance_dialogue_clock(double delta) {
    if(!std::isfinite(delta)||delta<0||delta>60000)throw std::runtime_error("Invalid dialogue clock delta");
    sync_dialogue();if(!dialogue_active()||backgrounded||markup.reading_options||!markup.ui_modal.empty()||!markup.pending_purchase.is_null()||game.timeline_active()||!game.direction_cursor.is_null())return;
    const bool complete=dialogue_complete();if(!complete)dialogue_characters+=delta*preferences.at("textSpeed").get<int>()/1000.0;
    if(complete&&preferences.at("autoAdvance").get<bool>()){
      dialogue_wait+=delta;if(dialogue_wait>=preferences.at("autoDelay").get<int>()*1000){dialogue_wait=0;execute({{"type","advance"}});}
    }
  }
