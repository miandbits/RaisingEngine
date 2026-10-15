#pragma once

// A read-only presentation of the actual visited states. No gameplay command is
// dispatched, so effects, RNG, unlocks, route recording and run saves cannot fire.
class GalleryRecollection {
public:
  GameSession game;
  ExtensionHost extensions;
  ScreenMarkup markup;
  Json record;
  size_t cursor=0;
  bool active=false;
  std::string notice;
  GalleryRecollection(const Json& bundle,const fs::path& root,const Json& value,const std::string& locale)
    :markup(game,bundle.at("assets"),root),record(value) {
    if(!record.contains("content")||!record.contains("replayFrames")||!record.at("replayFrames").is_array()||record.at("replayFrames").empty())throw std::runtime_error("이전 형식의 기록입니다. 대사 기록을 보거나 본편에서 다시 감상해 주세요.");
    const auto& original=record.at("content");
    if(original.value("assetHashes",Json())!=bundle.at("content").value("assetHashes",Json())||record.value("extensionManifest",Json())!=bundle.value("extensions",Json()))throw std::runtime_error("원본 에셋 또는 애니메이션 플러그인이 변경되어 원본 연출을 재생할 수 없습니다. 대사 기록은 보존됩니다.");
    game.content=original;game.content["project"].erase("gallery");game.gallery_recording=false;
    markup.locale=locale;extensions.isolated_modules=true;extensions.load(bundle,root);
    for(const auto& plugin:extensions.plugins)if(!plugin.available)throw std::runtime_error("회상용 애니메이션 플러그인을 불러오지 못했습니다: "+plugin.error);
    extensions.asset_path=[this](const std::string& id){return markup.asset(Json(id));};markup.extensions=&extensions;
    seek(0);active=true;
  }
  void seek(size_t index) {
    if(index>=record.at("replayFrames").size())throw std::runtime_error("회상 위치가 올바르지 않습니다.");
    const auto& frame=record.at("replayFrames").at(index);
    game.state=frame.at("state");game.refresh();cursor=index;
    extensions.pending_restore=frame.value("extensions",Json());notice.clear();
  }
  std::string document() {
    auto source=markup.document();
    // Only the recorded path is navigable. Other authored gameplay actions stay disabled.
    for(auto it=markup.actions.begin();it!=markup.actions.end();){const auto type=it->second.value("type","");
      bool allowed=type=="advance"||type=="advanceNarrative"||(type=="choose"&&it->second.value("choiceId",Json())==record.at("replayFrames").at(cursor).value("choiceId",Json()));
      if(allowed){it->second={{"type","recollectionNext"}};++it;}else it=markup.actions.erase(it);
    }
    std::string bar="<div id=\"recollection-toolbar\" style=\"position:absolute;left:16px;top:12px;right:16px;height:58px;background-color:#15232ff5;z-index:30000;\">";
    const std::vector<std::tuple<std::string,std::string,std::string>> buttons={{"recollection-close","회상 종료","recollectionClose"},{"recollection-prev","이전","recollectionPrev"},{"recollection-next",cursor+1==record.at("replayFrames").size()?"회상 끝내기":"다음","recollectionNext"},{"recollection-save","회상 저장","save"},{"recollection-load","회상 불러오기","load"}};
    int x=8;for(const auto& [id,label,type]:buttons){bar+="<div style=\"position:absolute;left:"+number(x)+"px;top:8px;width:144px;height:40px;\">"+markup.button(id,label,{{"type",type}},type!="recollectionPrev"||cursor>0)+"</div>";x+=152;}
    bar+="<div style=\"position:absolute;left:"+number(x)+"px;top:14px;color:#eee4c9;\">"+std::to_string(cursor+1)+" / "+std::to_string(record.at("replayFrames").size())+" "+escape(notice)+"</div></div>";
    source.insert(source.rfind("</body>"),bar);return source;
  }
};
