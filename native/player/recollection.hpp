#pragma once

// A read-only presentation of the actual visited states. No gameplay command is
// dispatched, so effects, RNG, unlocks, route recording and run saves cannot fire.
class GalleryRecollection {
public:
  GameSession game;
  ExtensionHost extensions;
  ScreenMarkup markup,controls;
  Json controls_visual;
  std::string controls_screen;
  Json record;
  size_t cursor=0;
  bool active=false,autoplay=false;
  int speed=1;
  double elapsed_ms=0;
  static constexpr double frame_ms=3000;
  Json playback() const {return {{"autoplay",autoplay},{"speed",speed},{"elapsedMs",elapsed_ms}};}
  static Json checked_playback(const Json& value){
    const auto p=value.value("playback",Json{{"autoplay",false},{"speed",1},{"elapsedMs",0}});
    if(!p.is_object()||!p.contains("autoplay")||!p.at("autoplay").is_boolean()||!p.contains("speed")||!p.at("speed").is_number_integer()||!(p.at("speed")==1||p.at("speed")==2||p.at("speed")==4)||!p.contains("elapsedMs")||!p.at("elapsedMs").is_number()||!std::isfinite(p.at("elapsedMs").get<double>())||p.at("elapsedMs").get<double>()<0||p.at("elapsedMs").get<double>()>=frame_ms)throw std::runtime_error("회상 재생 설정이 올바르지 않습니다.");
    return p;
  }
  std::string notice;
  GalleryRecollection(const Json& bundle,const fs::path& root,const Json& value,const std::string& locale)
    :markup(game,bundle.at("assets"),root),controls(game,bundle.at("assets"),root),record(value) {
    if(!record.contains("content")||!record.contains("replayFrames")||!record.at("replayFrames").is_array()||record.at("replayFrames").empty())throw std::runtime_error("이전 형식의 기록입니다. 대사 기록을 보거나 본편에서 다시 감상해 주세요.");
    const auto& original=record.at("content");
    if(original.value("assetHashes",Json())!=bundle.at("content").value("assetHashes",Json())||record.value("extensionManifest",Json())!=bundle.value("extensions",Json()))throw std::runtime_error("원본 에셋 또는 애니메이션 플러그인이 변경되어 원본 연출을 재생할 수 없습니다. 대사 기록은 보존됩니다.");
    game.content=original;game.content["project"].erase("gallery");game.gallery_recording=false;
    controls.display_content=localized_content(bundle.at("content"),locale);
    controls_visual=controls.display_content.at("project").at("visual");
    controls_screen=bundle.at("content").at("project").value("gallery",Json::object()).value("replayControlsScreenId","");
    for(const auto& entry:controls.display_content.at("project").value("gallery",Json::object()).value("entries",Json::array()))if(entry.value("kind","")==record.value("kind","")&&entry.value("sourceId","")==record.value("sourceId","")){controls.replay_title=entry.value("title","");break;}
    controls.replay_controls=true;controls.locale=locale;
    markup.locale=locale;extensions.isolated_modules=true;extensions.load(bundle,root);
    for(const auto& plugin:extensions.plugins)if(!plugin.available)throw std::runtime_error("회상용 애니메이션 플러그인을 불러오지 못했습니다: "+plugin.error);
    extensions.asset_path=[this](const std::string& id){return markup.asset(Json(id));};markup.extensions=&extensions;
    seek(0);active=true;
  }
  void seek(size_t index) {
    if(index>=record.at("replayFrames").size())throw std::runtime_error("회상 위치가 올바르지 않습니다.");
    const auto& frame=record.at("replayFrames").at(index);
    game.state=frame.at("state");game.refresh();cursor=index;elapsed_ms=0;
    extensions.pending_restore=frame.value("extensions",Json());notice.clear();
  }
  std::string document() {
    auto source=markup.document();
    // Only the recorded path is navigable. Other authored gameplay actions stay disabled.
    for(auto it=markup.actions.begin();it!=markup.actions.end();){const auto type=it->second.value("type","");
      bool allowed=type=="advance"||type=="advanceNarrative"||(type=="choose"&&it->second.value("choiceId",Json())==record.at("replayFrames").at(cursor).value("choiceId",Json()));
      if(allowed){it->second={{"type","recollectionNext"}};++it;}else it=markup.actions.erase(it);
    }
    if(!controls_screen.empty()){
      const Json* screen=nullptr;for(const auto& candidate:controls_visual.at("screens"))if(candidate.at("id")==controls_screen)screen=&candidate;
      if(!screen)throw std::runtime_error("회상 조작 화면을 찾을 수 없습니다: "+controls_screen);
      controls.width=markup.width;controls.height=markup.height;controls.screen_id=controls_screen;
      controls.display_projection=markup.display_projection;controls.replay_cursor=cursor;controls.replay_count=record.at("replayFrames").size();controls.replay_notice=notice;controls.replay_autoplay=autoplay;controls.replay_speed=speed;
      controls.actions.clear();controls.labels.clear();
      controls.render_elements=expanded_elements(controls_visual,*screen,markup.width,markup.height,markup.phase());
      // A namespace avoids collisions with IDs in the historical scene. Only UI primitives are rendered.
      Json safe=Json::array();for(auto element:controls.render_elements){const auto kind=element.value("kind","");if(kind!="panel"&&kind!="text"&&kind!="button"&&kind!="image"&&kind!="group"&&kind!="meter")continue;
        for(const auto key:{"id","parentId","_scrollParent","_collectionParent"})if(element.contains(key))element[key]="replay-ui__"+element.at(key).get<std::string>();safe.push_back(element);}
      controls.render_elements=safe;
      std::string css;
      for(const auto& raw:safe){const auto e=resolved_style(controls_visual,raw);const auto selector="#node-"+e.value("id","");std::string typography;
        if(e.value("kind","")=="button")css+=selector+">button{color:"+css_color(e.value("color","#ffffff"))+";}";
        if(e.contains("fontAssetId"))typography+="font-family:engine-"+e.at("fontAssetId").get<std::string>()+";";
        if(e.contains("fontSize"))typography+="font-size:"+number(e.at("fontSize"))+"px;";
        if(e.contains("color"))typography+="color:"+css_color(e.at("color"))+";";
        css+=selector+","+selector+" button{"+typography+"}";
        for(const auto& [field,pseudo]:std::vector<std::pair<std::string,std::string>>{{"hoverFill",":hover"},{"pressedFill",":active"}})if(e.contains(field))css+=selector+" button"+pseudo+"{background-color:"+css_color(e.at(field))+";}";
        if(e.contains("disabledColor"))css+=selector+" button:disabled{color:"+css_color(e.at("disabledColor"))+";}";
      }
      source.insert(source.find("</style>"),css);
      std::string overlay="<div id=\"recollection-authored-controls\" style=\"position:absolute;left:0;top:0;width:100%;height:100%;z-index:30000;pointer-events:none;\">";
      for(const auto& e:safe)if(!e.contains("_scrollParent")&&!e.contains("_collectionParent"))overlay+=controls.render_node(e);
      overlay+="</div>";
      for(const auto& [id,action]:controls.actions){const auto type=action.value("type","");if(type=="recollectionPrev"||type=="recollectionNext"||type=="recollectionClose"||type=="recollectionSave"||type=="recollectionLoad"||type=="recollectionAuto"||type=="recollectionSpeed"||type=="recollectionSkip"){markup.actions[id]=action;markup.labels[id]=controls.labels[id];}}
      source.insert(source.rfind("</body>"),overlay);return source;
    }
    std::string bar="<div id=\"recollection-toolbar\" style=\"position:absolute;left:16px;top:12px;right:16px;height:116px;background-color:#15232ff5;z-index:30000;\">";
    const std::vector<std::tuple<std::string,std::string,std::string>> buttons={{"recollection-close","회상 종료","recollectionClose"},{"recollection-prev","이전","recollectionPrev"},{"recollection-next",cursor+1==record.at("replayFrames").size()?"회상 끝내기":"다음","recollectionNext"},{"recollection-save","회상 저장","save"},{"recollection-load","회상 불러오기","load"}};
    int x=8;for(const auto& [id,label,type]:buttons){bar+="<div style=\"position:absolute;left:"+number(x)+"px;top:8px;width:144px;height:40px;\">"+markup.button(id,label,{{"type",type}},type!="recollectionPrev"||cursor>0)+"</div>";x+=152;}
    bar+="<div style=\"position:absolute;left:"+number(x)+"px;top:14px;color:#eee4c9;\">"+std::to_string(cursor+1)+" / "+std::to_string(record.at("replayFrames").size())+" "+escape(notice)+"</div></div>";
    bar.resize(bar.size()-6); // Extend the toolbar before its closing div.
    const std::vector<std::tuple<std::string,std::string,std::string>> playback_buttons={{"recollection-auto",autoplay?"자동 멈춤":"자동 재생","recollectionAuto"},{"recollection-speed",std::to_string(speed)+"배속","recollectionSpeed"},{"recollection-skip","끝으로 건너뛰기","recollectionSkip"}};
    x=8;for(const auto& [id,label,type]:playback_buttons){bar+="<div style=\"position:absolute;left:"+number(x)+"px;top:64px;width:180px;height:40px;\">"+markup.button(id,label,{{"type",type}})+"</div>";x+=188;}bar+="</div>";
    source.insert(source.rfind("</body>"),bar);return source;
  }
};
