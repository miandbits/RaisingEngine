#pragma once
#include "raising/runtime.hpp"
#include "raising/relationship-history.hpp"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <functional>
#include <map>
using Json = nlohmann::json;
namespace fs = std::filesystem;
#include "timeline.hpp"
#include "battle-selection.hpp"
#include "direction.hpp"

inline Json read_json(const fs::path& file, uintmax_t limit = 64 * 1024 * 1024) {
  if (!fs::is_regular_file(file) || fs::file_size(file) > limit) throw std::runtime_error("File missing or too large: " + file.u8string());
  std::ifstream stream(file, std::ios::binary); if (!stream) throw std::runtime_error("Cannot read " + file.u8string());
  return Json::parse(stream);
}

#include "gallery.hpp"
#include "choice-feedback.hpp"

class GameSession {
public:
  // Preview saves can only be loaded by the process that created them. Normal saves keep strict replay validation.
  std::string inspection_session;
  Json inspection_baseline=nullptr;
  std::map<fs::path,Json> inspection_saves;
  std::function<Json()> planner_state;
  std::function<void(const Json&)> planner_restore;
  GalleryProfile gallery;
  GalleryCapture gallery_capture;
  Json gallery_pending=Json::array();
  std::function<Json()> gallery_content;
  bool gallery_recording=true;
  Json gallery_manifest=nullptr;
  void queue_gallery(Json record) {
    if(!gallery_recording||record.is_null()||!content.at("project").contains("gallery"))return;
    Json ids=Json::array();
    for(const auto& e:content.at("project").at("gallery").at("entries"))if(e.at("kind")==record.at("kind")&&e.at("sourceId")==record.at("sourceId")){
      bool allowed=true;if(e.contains("unlockCondition"))allowed=call({{"operation","evaluate"},{"state",state},{"condition",e.at("unlockCondition")}}).value("value",false);
      if(allowed)ids.push_back(e.at("id"));
    }
    if(!ids.empty()){if(record.contains("content"))record["extensionManifest"]=gallery_manifest;record["_entryIds"]=ids;gallery_pending.push_back(record);}
  }
  void collect_gallery_media() {
    if(!gallery_recording||state.is_null()||!content.at("project").contains("gallery"))return;
    for(const auto& e:content.at("project").at("gallery").at("entries"))if((e.at("kind")=="cg"||e.at("kind")=="music")&&gallery.records(e).empty()){
      bool queued=false;for(const auto& p:gallery_pending)if(p.at("kind")==e.at("kind")&&p.at("sourceId")==e.at("sourceId"))queued=true;
      if(!queued)queue_gallery({{"kind",e.at("kind")},{"sourceId",e.at("sourceId")},{"frames",Json::array()},{"contentFingerprint",content.at("contentFingerprint")},{"attributes",state.at("attributes")},{"attributeLabels",Json::object()},{"relationships",state.value("relationships",Json::object())},{"presentation",state.value("presentation",Json::object())},{"periodIndex",state.value("periodIndex",0)}});
    }
    flush_gallery();
  }
  void flush_gallery() {
    if(!gallery_recording||!content.at("project").contains("gallery"))return;
    while(!gallery_pending.empty()){
      try{gallery.unlock(content.at("project").at("gallery"),gallery_pending.front());gallery_pending.erase(gallery_pending.begin());}
      catch(const std::exception& e){gallery.error=e.what();message=std::string("컬렉션 저장 실패: ")+e.what();return;}
    }
  }
  Json content, state = nullptr, commands = Json::array(), projection;
  Json relationship_changes = Json::object();
  Json choice_feedback = nullptr;
  std::string message;
  std::vector<std::string> sound_cues;
  Json timeline_plan=nullptr,timeline_cursor=nullptr;
  Json battle_selection=nullptr;
  Json direction_cursor=nullptr;
  // Presentation hooks for runtime extensions. They receive copies and cannot change state.
  std::function<void(const Json&,const Json&,const Json&,const std::string&)> observer;
  std::function<Json()> extension_state;
  Json loaded_extensions=nullptr;
  int periods() const {return content.at("project").value("periodsPerMonth",3);}
  bool continuous() const {return content.at("project").value("schedulePlayback","")=="continuous";}
  std::string schedule_step() const {
    if(!continuous()||state.is_null()||state.at("schedule").is_null())return "";
    const auto phase=state.value("phase","");
    if((phase=="ready"||phase=="planning")&&state.contains("progression")&&state["progression"].contains("sessions")&&!state["progression"]["pending"].is_null())return "advanceSession";
    if(phase=="ready"&&state.value("activityIntermission",false)) {
      const auto flow=content["project"].value("periodFlow",Json::object());
      if(!flow.contains("intermission"))return "";
      return state["flags"].value(flow["intermission"].value("completionFlagId",""),false)?"acknowledgeResult":"";
    }
    if(phase=="ready")return "executeActivity";
    if(phase=="activity-result"&&(!timeline_active()||timeline_cursor.value("completed",false)))return "acknowledgeResult";
    return "";
  }
  void sync_battle_selection(){if(!validate_battle_selection(content,state,battle_selection))battle_selection=nullptr;}
  Json battle_ui(const Json& action) {
    if(state.is_null()||state.value("phase","")!="battle")throw std::runtime_error("진행 중인 전투가 없습니다.");
    const auto type=action.value("type","");if(type=="cancelBattleAction"){battle_selection=nullptr;return nullptr;}
    if(type=="selectBattleAction") {
      Json next={{"stateRevision",state.at("stateRevision")},{"battleAction",action.value("battleAction","")}};if(action.contains("itemId"))next["itemId"]=action.at("itemId");
      if(!validate_battle_selection(content,state,next))throw std::runtime_error("올바른 전투 행동을 선택해 주세요.");
      std::string reason;if(!available(battle_command(next),&reason))throw std::runtime_error(reason);battle_selection=next;return nullptr;
    }
    if(!validate_battle_selection(content,state,battle_selection))throw std::runtime_error("현재 전투에서 행동을 먼저 선택해 주세요.");
    const auto command=battle_command(battle_selection);std::string reason;if(!available(command,&reason))throw std::runtime_error(reason);
    const auto target=battle_target(battle_selection.value("battleAction",""));
    if(type=="selectBattleTarget"){if(target=="none"||action.value("battleTarget","")!=target)throw std::runtime_error("선택한 행동에 사용할 수 없는 대상입니다.");battle_selection["target"]=target;return nullptr;}
    if(target!="none"&&battle_selection.value("target","")!=target)throw std::runtime_error("행동 대상을 선택해 주세요.");
    return command;
  }
  bool battle_ui_available(const Json& action) {
    const auto saved=battle_selection;try{battle_ui(action);battle_selection=saved;return true;}catch(...){battle_selection=saved;return false;}
  }
  int schedule_speed=1;
  void sync_timeline() {
    timeline_plan=plan_activity_timeline(content,state);
    if(timeline_plan.is_null())timeline_cursor=nullptr;
    else if(!validate_timeline_cursor(timeline_plan,timeline_cursor)){timeline_cursor=create_timeline_cursor(timeline_plan);if(timeline_plan.contains("resultPlayback"))timeline_cursor["speed"]=schedule_speed;}
  }
  bool timeline_active() const {return !timeline_plan.is_null()&&!timeline_cursor.is_null();}
  void tick_timeline(double delta) {if(timeline_active())timeline_cursor=advance_timeline(timeline_plan,timeline_cursor,delta);}
  uint32_t seed = 42;
  Json origin = {{"seed",42},{"mode","full"},{"initialOverrides",{{"attributes",Json::object()},{"flags",Json::object()},{"relationships",Json::object()}}}};
  Json call(Json request) const {
    request["content"] = content;
    Json result = Json::parse(raising::process(request.dump()));
    if (!result.value("ok", false)) {
      auto error = result.value("error", Json::object());
      throw std::runtime_error("[" + error.value("code", "RUNTIME") + "] " + error.value("message", "Native runtime failed"));
    }
    return result;
  }
  void refresh() { projection = call({{"operation", "view"}, {"state", state}}); }
  bool available(const Json& command,std::string* reason=nullptr) const {
    if(state.is_null()){if(reason)*reason="새 게임을 시작해 주세요.";return false;}
    try{call({{"operation","command"},{"state",state},{"command",command},{"expectedRevision",state.at("stateRevision")}});return true;}
    catch(const std::exception& error){if(reason)*reason=error.what();return false;}
  }
  void start(const Json& userValues=Json::object()) {
    gallery_capture.pending=nullptr;
    schedule_speed=1;
    relationship_changes=Json::object();
    choice_feedback=nullptr;
    state = call({{"operation", "create"}, {"seed", seed}, {"options", {{"mode", "full"},{"userValues",userValues}}}}).at("state");
    if(!inspection_session.empty())inspection_baseline=state;
    commands = Json::array(); message.clear(); sound_cues.clear(); battle_selection=nullptr;refresh(); sync_timeline();
    origin = {{"seed",seed},{"mode","full"},{"initialOverrides",{{"attributes",Json::object()},{"flags",Json::object()},{"relationships",Json::object()}}}};
    if(!content.at("project").value("userFields",Json::array()).empty())origin["userValues"]=userValues;
    collect_gallery_media();
  }
  Json send(const Json& command) {
    if (state.is_null()) throw std::runtime_error("먼저 새 게임을 시작해 주세요.");
    // Single choke point for submitted commands: extensions observe each reply exactly once.
    const Json before=observer?Json{{"state",state},{"narrative",projection.value("narrative",Json())}}:Json();
    const bool collect=gallery_recording&&content.at("project").contains("gallery")&&!state.value("activeNarrative",Json()).is_null()&&(command.value("type","")=="advanceNarrative"||command.value("type","")=="choose");
    const auto gallery_extensions=collect&&extension_state?extension_state():Json();
    const auto gallery_before=collect?state:Json();
    const auto capture_content=collect?(gallery_content?gallery_content():content):Json();
    Json capture_narrative=nullptr;
    if(collect&&!state.value("activeNarrative",Json()).is_null())capture_narrative=Json::parse(raising::process(Json({{"operation","view"},{"content",capture_content},{"state",state}}).dump())).value("narrative",Json());
    Json result;
    try {result = call({{"operation", "command"}, {"state", state}, {"command", command}, {"expectedRevision", state.at("stateRevision")}});}
    catch(const std::exception& error){if(observer)observer(before,before,command,error.what());throw;}
    raising::track_relationship_changes(relationship_changes,state,result.at("state"));
    track_choice_feedback(choice_feedback,content,state,command,result);
    state = result.at("state"); commands.push_back(command);
    for(const auto& cue:result.at("transition").value("soundCues",Json::array()))sound_cues.push_back(cue.get<std::string>());
    refresh(); sync_timeline();sync_battle_selection();
    if(collect)queue_gallery(gallery_capture.observe(capture_content,gallery_before,state,command,capture_narrative,gallery_extensions));
    collect_gallery_media();
    if(observer)observer(before,{{"state",state},{"narrative",projection.value("narrative",Json())}},command,"");
    return result.at("transition");
  }
  void settle() {
    for (int i=0; i<10000; i++) {
      const auto phase = state.value("phase", "");
      if (phase == "period-end") { send({{"type", "resolvePeriodEnd"}}); continue; }
      const auto narrative = projection.value("narrative", Json());
      if (!narrative.is_null()) {
        const auto type = narrative.at("node").value("type", "");
        if (type != "dialogue" && type != "choice") { send({{"type", "advanceNarrative"}}); continue; }
      }
      return;
    }
    throw std::runtime_error("대본 자동 진행 한도를 초과했습니다.");
  }
  void save(const fs::path& file,const Json& save_record=Json()) {
    if (state.is_null()) throw std::runtime_error("저장할 게임이 없습니다.");
    Json envelope = {{"saveVersion", "1.0.0"}, {"coreVersion", state.at("coreVersion")}, {"contentFingerprint", content.at("contentFingerprint")},
      {"origin", origin},
      {"commands", commands}, {"state", state}};
    if(!inspection_session.empty())envelope["inspectionSession"]=inspection_session;
    const Json extensions=extension_state?extension_state():Json();
    if(schedule_speed!=1||timeline_active()||!battle_selection.is_null()||!extensions.is_null()||!direction_cursor.is_null()){envelope["playerPresentation"]={{"version",1},{"timeline",timeline_cursor}};if(!battle_selection.is_null())envelope["playerPresentation"]["battleSelection"]=battle_selection;if(!extensions.is_null())envelope["playerPresentation"]["extensions"]=extensions;}
    if(!direction_cursor.is_null())envelope["playerPresentation"]["direction"]=direction_cursor;
    if(schedule_speed!=1)envelope["playerPresentation"]["scheduleSpeed"]=schedule_speed;
    const Json draft=planner_state?planner_state():Json();
    if(!draft.is_null()){
      if(!envelope.contains("playerPresentation"))envelope["playerPresentation"]={{"version",1},{"timeline",nullptr}};
      envelope["playerPresentation"]["scheduleDraft"]=draft;
    }
    if(!gallery_capture.pending.is_null()){
      if(!envelope.contains("playerPresentation"))envelope["playerPresentation"]={{"version",1},{"timeline",nullptr}};
      envelope["playerPresentation"]["galleryCapture"]=gallery_capture.pending;
    }
    if(!save_record.is_null()){
      if(!envelope.contains("playerPresentation"))envelope["playerPresentation"]={{"version",1},{"timeline",nullptr}};
      envelope["playerPresentation"]["saveRecord"]=save_record;
    }
    fs::create_directories(file.parent_path());
    auto temp = file; temp += ".tmp";
    { std::ofstream out(temp, std::ios::binary | std::ios::trunc); if (!out) throw std::runtime_error("저장 파일을 열 수 없습니다."); out << envelope.dump(2); out.flush(); if (!out) throw std::runtime_error("저장 파일 쓰기에 실패했습니다."); }
    auto backup = file; backup += ".bak";
    if (fs::exists(backup)) fs::remove(backup);
    if (fs::exists(file)) fs::rename(file, backup);
    try { fs::rename(temp, file); } catch (...) { if (fs::exists(backup) && !fs::exists(file)) fs::rename(backup, file); throw; }
    if(!inspection_session.empty())inspection_saves[file]=inspection_baseline;
    message = "게임을 저장했습니다.";
  }
  void load(const fs::path& file) {
    const auto saved = read_json(file);
    if(!saved.is_object())throw std::runtime_error("저장 파일은 객체여야 합니다.");
    const bool isolated=!inspection_session.empty();
    if(isolated&&(saved.value("inspectionSession","")!=inspection_session||!inspection_saves.count(file)))throw std::runtime_error("This preview save belongs to another session");
    for(auto it=saved.begin();it!=saved.end();++it)if(it.key()!="saveVersion"&&it.key()!="coreVersion"&&it.key()!="contentFingerprint"&&it.key()!="origin"&&it.key()!="commands"&&it.key()!="state"&&it.key()!="playerPresentation"&&!(isolated&&it.key()=="inspectionSession"))throw std::runtime_error("저장 파일에 알 수 없는 항목이 있습니다.");
    if (saved.value("saveVersion", "") != "1.0.0" || saved.at("contentFingerprint") != content.at("contentFingerprint")
      || saved.at("origin").value("mode", "") != "full" || !saved.at("commands").is_array() || saved.at("commands").size() > 100000)
      throw std::runtime_error("이 게임과 호환되는 저장 파일이 아닙니다.");
    const auto& savedOrigin=saved.at("origin");const bool hasFields=!content.at("project").value("userFields",Json::array()).empty();
    if(!savedOrigin.is_object()||!savedOrigin.at("initialOverrides").is_object())throw std::runtime_error("저장 생성 정보가 올바르지 않습니다.");
    for(auto it=savedOrigin.at("initialOverrides").begin();it!=savedOrigin.at("initialOverrides").end();++it)if(it.key()!="attributes"&&it.key()!="flags"&&it.key()!="relationships")throw std::runtime_error("저장 초기 값에 알 수 없는 항목이 있습니다.");
    for(auto it=savedOrigin.begin();it!=savedOrigin.end();++it)if(it.key()!="seed"&&it.key()!="mode"&&it.key()!="initialOverrides"&&!(hasFields&&it.key()=="userValues"))throw std::runtime_error("저장 생성 정보에 알 수 없는 항목이 있습니다.");
    if(hasFields&&!savedOrigin.contains("userValues"))throw std::runtime_error("저장 생성 정보에 사용자 입력이 없습니다.");
    Json options={{"mode","full"},{"initialOverrides",savedOrigin.at("initialOverrides")}};if(hasFields)options["userValues"]=savedOrigin.at("userValues");
    Json replay = isolated?inspection_saves.at(file):call({{"operation", "create"}, {"seed", savedOrigin.at("seed")},{"options", options}}).at("state");
    if (saved.at("coreVersion") != replay.at("coreVersion")) throw std::runtime_error("실행 규칙 버전이 다릅니다.");
    GalleryCapture restored_capture;
    Json restored_changes=Json::object();
    Json restored_choice=nullptr;
    for (const auto& command : saved.at("commands")) {
      const auto result=call({{"operation", "command"}, {"state", replay}, {"command", command}, {"expectedRevision", replay.at("stateRevision")}});
      const auto next=result.at("state");
      track_choice_feedback(restored_choice,content,replay,command,result);
      raising::track_relationship_changes(restored_changes,replay,next);
      if(content.at("project").contains("gallery")&&!replay.value("activeNarrative",Json()).is_null()){
        const auto narrative=call({{"operation","view"},{"state",replay}}).value("narrative",Json());
        restored_capture.observe(content,replay,next,command,narrative);
      }
      replay=next;
    }
    if (replay != saved.at("state")) throw std::runtime_error("저장 데이터와 명령 재현 결과가 다릅니다.");
    const auto restored_plan=plan_activity_timeline(content,replay);Json restored_cursor=restored_plan.is_null()?Json():create_timeline_cursor(restored_plan);
    Json restored_battle=nullptr,restored_extensions=nullptr,restored_direction=nullptr,restored_draft=nullptr;int restored_speed=1;
    if(saved.contains("playerPresentation")) {
      const auto presentation=saved.at("playerPresentation");
      if(!presentation.is_object()||!presentation.contains("version")||presentation.at("version")!=1||!presentation.contains("timeline"))throw std::runtime_error("Invalid player presentation save");
      for(auto it=presentation.begin();it!=presentation.end();++it)if(it.key()!="version"&&it.key()!="timeline"&&it.key()!="battleSelection"&&it.key()!="extensions"&&it.key()!="direction"&&it.key()!="scheduleSpeed"&&it.key()!="galleryCapture"&&it.key()!="scheduleDraft"&&it.key()!="saveRecord")throw std::runtime_error("Invalid player presentation save");
      if(presentation.contains("scheduleDraft")){
        const auto d=presentation.at("scheduleDraft");
        if(!d.is_object()||d.size()!=4||!d.value("month",Json()).is_number_integer()||!d.value("slot",Json()).is_number_integer()||!d.value("selected",Json()).is_number_integer()||!d.value("activities",Json()).is_array())throw std::runtime_error("Invalid schedule draft");
        if(d.at("month")!=replay.value("periodIndex",0)/periods()||d.at("slot")<0||d.at("slot")>=periods()||d.at("selected")<0||d.at("selected")>((1<<periods())-1)||d.at("activities").size()!=size_t(periods()))throw std::runtime_error("Invalid schedule draft");
        for(int i=0;i<periods();++i){
          const auto id=d.at("activities").at(i);bool known=id=="";
          for(const auto& a:content.at("activities"))if(a.at("id")==id)known=true;
          if(!id.is_string()||!known||((d.at("selected").get<int>()&(1<<i))&&id==""))throw std::runtime_error("Invalid schedule draft activity");
        }
        restored_draft=d;
      }
      if(presentation.contains("scheduleSpeed")){const auto speed=presentation.at("scheduleSpeed");if(!speed.is_number_integer()||(speed!=1&&speed!=4))throw std::runtime_error("Invalid schedule speed");restored_speed=speed.get<int>();}
      // Opaque plugin states; a missing plugin later ignores (and preserves) its entry.
      if(presentation.contains("extensions")){if(!presentation.at("extensions").is_object())throw std::runtime_error("Invalid player presentation save");restored_extensions=presentation.at("extensions");}
      if(presentation.contains("direction")){restored_direction=presentation.at("direction");if(!direction_valid(direction_config(content),restored_direction))throw std::runtime_error("Invalid direction cursor");}
      const auto cursor=presentation.at("timeline");
      if(cursor.is_null()){if(!restored_plan.is_null())throw std::runtime_error("Missing timeline cursor");restored_cursor=nullptr;}
      else {if(!validate_timeline_cursor(restored_plan,cursor))throw std::runtime_error("Invalid timeline cursor");restored_cursor=cursor;}
      if(presentation.contains("battleSelection")&&!presentation.at("battleSelection").is_null()){
        restored_battle=presentation.at("battleSelection");if(!validate_battle_selection(content,replay,restored_battle))throw std::runtime_error("Invalid battle selection");
        call({{"operation","command"},{"state",replay},{"command",battle_command(restored_battle)},{"expectedRevision",replay.at("stateRevision")}});
      }
    }
    if(!restored_plan.is_null()&&restored_plan.contains("resultPlayback")){const int speed=restored_cursor.at("speed");if(saved.contains("playerPresentation")&&saved.at("playerPresentation").contains("scheduleSpeed")&&speed!=restored_speed)throw std::runtime_error("Inconsistent schedule speed");restored_speed=speed;}
    if(saved.contains("playerPresentation")&&saved.at("playerPresentation").contains("galleryCapture")){
      auto capture=saved.at("playerPresentation").at("galleryCapture"),expected=restored_capture.pending;
      if(!capture.is_object()||!expected.is_object()||capture.value("frames",Json())!=expected.value("frames",Json())||capture.value("replayFrames",Json()).size()!=expected.value("replayFrames",Json()).size())throw std::runtime_error("회상 진행 기록이 명령 재현과 다릅니다.");
      for(size_t i=0;i<capture.at("replayFrames").size();i++)if(capture.at("replayFrames").at(i).at("state")!=expected.at("replayFrames").at(i).at("state"))throw std::runtime_error("회상 상태가 명령 재현과 다릅니다.");
      // Keep only opaque presentation state; content and game snapshots come from verified replay.
      for(size_t i=0;i<capture.at("replayFrames").size();i++)restored_capture.pending["replayFrames"][i]["extensions"]=capture.at("replayFrames").at(i).value("extensions",Json());
    }
    if(isolated)inspection_baseline=inspection_saves.at(file);
    gallery_capture=std::move(restored_capture);
    relationship_changes=std::move(restored_changes);
    choice_feedback=std::move(restored_choice);
    schedule_speed=restored_speed;state = replay; commands = saved.at("commands"); origin = saved.at("origin"); seed = origin.at("seed").get<uint32_t>(); sound_cues.clear(); timeline_plan=restored_plan;timeline_cursor=restored_cursor;battle_selection=restored_battle;loaded_extensions=restored_extensions;direction_cursor=restored_direction;refresh(); settle();
    collect_gallery_media();
    if(planner_restore)planner_restore(restored_draft);
    message = "저장한 지점에서 이어갑니다.";
  }
};
