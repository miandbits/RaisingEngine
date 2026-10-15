#pragma once
#include <chrono>
#include <ctime>
#include <thread>

// Cross-run records are independent from command-replayed run saves. No replay command
// is executed by the gallery: it only reads immutable snapshots of completed narratives.
inline std::string gallery_timestamp() {
  const auto now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm utc{};
#ifdef _WIN32
  gmtime_s(&utc,&now);
#else
  gmtime_r(&now,&utc);
#endif
  char text[32];std::strftime(text,sizeof(text),"%Y-%m-%dT%H:%M:%SZ",&utc);return text;
}

struct GalleryCapture {
  Json pending=nullptr;
  Json observe(const Json& content,const Json& before,const Json& after,const Json& command,const Json& narrative,const Json& extensions=nullptr) {
    const auto cursor=before.value("activeNarrative",Json());
    if(cursor.is_null()||narrative.is_null())return nullptr;
    const auto type=command.value("type","");if(type!="advanceNarrative"&&type!="choose")return nullptr;
    const auto kind=cursor.value("kind",""),id=cursor.value("id","");
    if(pending.is_null()||pending.at("kind")!=kind||pending.at("sourceId")!=id)
      pending={{"kind",kind},{"sourceId",id},{"frames",Json::array()},{"contentFingerprint",content.at("contentFingerprint")},{"content",content},{"replayFrames",Json::array()},{"_sounds",Json::array()}};
    const auto& node=narrative.at("node");const auto nodeType=node.value("type","");
    if(nodeType=="scene"){
      if(node.contains("scene")&&node.at("scene").contains("soundAssetId"))pending["_sounds"].push_back(node.at("scene").at("soundAssetId"));
      pending["_terminalScene"]={{"state",after},{"extensions",extensions},{"sounds",pending.at("_sounds")}};
    }
    if(nodeType=="dialogue"||nodeType=="choice") {
      Json frame={{"nodeId",cursor.at("nodeId")},{"text",node.value("text",node.value("prompt",""))},{"speaker",""},{"scene",before.value("presentation",Json::object())}};
      for(const auto& character:content.at("characters"))if(character.at("id")==node.value("speakerId",Json()))frame["speaker"]=character.value("name","");
      if(nodeType=="choice")for(const auto& choice:node.at("choices"))if(choice.at("id")==command.value("choiceId",Json())){frame["choiceId"]=choice.at("id");frame["choice"]=choice.value("text",choice.value("label",""));}
      if(pending["frames"].size()<10000){pending["frames"].push_back(frame);Json replayFrame={{"state",before},{"extensions",extensions},{"sounds",pending.at("_sounds")}};if(frame.contains("choiceId"))replayFrame["choiceId"]=frame.at("choiceId");pending["replayFrames"].push_back(replayFrame);pending["_sounds"]=Json::array();pending.erase("_terminalScene");}
    }
    if(nodeType!="end")return nullptr;
    if(pending.contains("_terminalScene")&&pending["replayFrames"].size()<10000){pending["_terminalScene"]["extensions"]=extensions;pending["replayFrames"].push_back(pending.at("_terminalScene"));}
    auto result=pending;result.erase("_sounds");result.erase("_terminalScene");pending=nullptr;
    result["attributes"]=after.value("attributes",Json::object());result["relationships"]=after.value("relationships",Json::object());
    result["presentation"]=after.value("presentation",Json::object());result["periodIndex"]=after.value("periodIndex",0);
    result["attributeLabels"]=Json::object();for(const auto& attribute:content.at("attributes"))result["attributeLabels"][attribute.at("id").get<std::string>()]=attribute.value("label",attribute.at("id").get<std::string>());
    return result;
  }
};

class GalleryProfile {
  fs::path path;
  std::string game_id;
  Json empty() const {return {{"version",1},{"gameId",game_id},{"entries",Json::object()}};}
  Json read() const {
    const auto backup=fs::path(path.u8string()+".bak");
    if(!fs::exists(path)&&!fs::exists(backup))return empty();
    const auto value=read_json(fs::exists(path)?path:backup,64*1024*1024);
    if(!value.is_object()||value.value("version",0)!=1||value.value("gameId","")!=game_id||!value.contains("entries")||!value.at("entries").is_object())throw std::runtime_error("컬렉션 기록 형식이 올바르지 않습니다. 원본 파일을 보존했습니다.");
    for(const auto& [id,entry]:value.at("entries").items()){
      if(!entry.is_object()||!entry.contains("records")||!entry.at("records").is_array()||entry.at("records").size()>32)throw std::runtime_error("컬렉션 항목이 올바르지 않습니다.");
      for(const auto& record:entry.at("records")){
        for(const auto key:{"kind","sourceId","title","unlockedAt","contentFingerprint"})if(!record.value(key,Json()).is_string())throw std::runtime_error("회상 기록 식별자가 올바르지 않습니다.");
        if(!record.is_object()||!record.contains("frames")||!record.at("frames").is_array()||record.at("frames").size()>10000||!record.contains("attributes")||!record.at("attributes").is_object()||!record.contains("attributeLabels")||!record.at("attributeLabels").is_object())throw std::runtime_error("회상 기록이 올바르지 않습니다.");
        for(const auto& frame:record.at("frames"))if(!frame.is_object()||!frame.value("text",Json()).is_string()||!frame.value("speaker",Json()).is_string()||!frame.value("scene",Json()).is_object())throw std::runtime_error("회상 대사가 올바르지 않습니다.");
        for(const auto& [key,value]:record.at("attributeLabels").items())if(!value.is_string())throw std::runtime_error("회상 능력치 이름이 올바르지 않습니다.");
        for(const auto& frame:record.at("frames"))if(frame.contains("choice")&&!frame.at("choice").is_string())throw std::runtime_error("회상 선택 기록이 올바르지 않습니다.");
      }
    }
    return value;
  }
public:
  Json data=Json::object();
  std::string error;
  bool active=false;
  void open(const fs::path& file,const std::string& id){path=file;game_id=id;data=empty();reload();}
  void reload(){try{data=read();error.clear();}catch(const std::exception& e){error=e.what();}}
  Json records(const Json& entry) const {
    Json out=Json::array();const auto entries=data.value("entries",Json::object());
    if(!entries.contains(entry.at("id").get<std::string>()))return out;
    for(const auto& record:entries.at(entry.at("id").get<std::string>()).at("records"))if(record.value("kind","")==entry.value("kind","")&&record.value("sourceId","")==entry.value("sourceId",""))out.push_back(record);
    return out;
  }
  void unlock(const Json& config,Json record) {
    if(record.is_null()||config.is_null())return;
    const auto entryIds=record.value("_entryIds",Json());record.erase("_entryIds");
    // Read-merge-write under a cross-process lock. Failed/corrupt files are never reset.
    fs::create_directories(path.parent_path());const auto lock=fs::path(path.u8string()+".lock");bool locked=false;
    for(int i=0;i<20&&!locked;i++){locked=fs::create_directory(lock);if(!locked)std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    if(!locked)throw std::runtime_error("다른 실행에서 컬렉션을 저장 중입니다. 도감을 다시 열어 저장을 재시도하세요.");
    struct Unlock {fs::path path;~Unlock(){std::error_code ec;fs::remove(path,ec);}} unlock{lock};
    auto merged=read();bool changed=false;
    for(const auto& entry:config.at("entries"))if(entry.at("kind")==record.at("kind")&&entry.at("sourceId")==record.at("sourceId")&&(entryIds.is_null()||std::find(entryIds.begin(),entryIds.end(),entry.at("id"))!=entryIds.end())) {
      const auto id=entry.at("id").get<std::string>();auto& item=merged["entries"][id];if(item.is_null())item={{"records",Json::array()}};
      bool duplicate=false;for(auto& old:item.at("records"))if(old.value("kind","")==record.at("kind")&&old.value("sourceId","")==record.at("sourceId")&&old.at("frames")==record.at("frames")){duplicate=true;if(!old.contains("replayFrames")&&record.contains("replayFrames")){old["replayFrames"]=record.at("replayFrames");old["content"]=record.at("content");changed=true;}}
      if(!duplicate){if(item["records"].size()>=32)throw std::runtime_error("한 항목의 회상 기록 한도(32개)에 도달했습니다. 기존 기록을 보존했습니다.");record["title"]=entry.at("title");record["unlockedAt"]=gallery_timestamp();item["records"].push_back(record);changed=true;}
    }
    if(changed){
      const auto temp=fs::path(path.u8string()+".tmp"),backup=fs::path(path.u8string()+".bak");
      const auto serialized=merged.dump(2);if(serialized.size()>64*1024*1024)throw std::runtime_error("컬렉션 기록 용량 한도를 초과했습니다. 기존 파일을 보존했습니다.");
      {std::ofstream out(temp,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("컬렉션 기록을 쓸 수 없습니다.");out<<serialized;out.flush();if(!out)throw std::runtime_error("컬렉션 기록 저장에 실패했습니다.");}
      if(fs::exists(path)){std::error_code ec;fs::remove(backup,ec);fs::rename(path,backup);}
      try{fs::rename(temp,path);}catch(...){if(fs::exists(backup)&&!fs::exists(path))fs::rename(backup,path);throw;}
    }
    data=std::move(merged);error.clear();
  }
};
