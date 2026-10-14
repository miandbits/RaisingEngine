#include <functional>
#pragma once
#include "raising/runtime.hpp"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
using Json = nlohmann::json;
namespace fs = std::filesystem;
#include "timeline.hpp"
#include "battle-selection.hpp"

inline Json read_json(const fs::path& file, uintmax_t limit = 64 * 1024 * 1024) {
  if (!fs::is_regular_file(file) || fs::file_size(file) > limit) throw std::runtime_error("File missing or too large: " + file.u8string());
  std::ifstream stream(file, std::ios::binary); if (!stream) throw std::runtime_error("Cannot read " + file.u8string());
  return Json::parse(stream);
}

class GameSession {
public:
  Json content, state = nullptr, commands = Json::array(), projection;
  Json extension_state=Json::object();
  std::function<void(const Json&,const Json&,const Json&,const std::string&)> on_transition;
  std::function<void()> before_save,after_load,after_start;
  Json observed() const{return {{"state",state},{"narrative",projection.value("narrative",Json())}};}

  std::string message;
  std::vector<std::string> sound_cues;
  Json timeline_plan=nullptr,timeline_cursor=nullptr;
  Json battle_selection=nullptr;
  int periods() const {return content.at("project").value("periodsPerMonth",3);}
  bool continuous() const {return content.at("project").value("schedulePlayback","")=="continuous";}
  std::string schedule_step() const {
    if(!continuous()||state.is_null()||state.at("schedule").is_null())return "";
    const auto phase=state.value("phase","");
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
  void sync_timeline() {
    timeline_plan=plan_activity_timeline(content,state);
    if(timeline_plan.is_null())timeline_cursor=nullptr;
    else if(!validate_timeline_cursor(timeline_plan,timeline_cursor))timeline_cursor=create_timeline_cursor(timeline_plan);
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
    state = call({{"operation", "create"}, {"seed", seed}, {"options", {{"mode", "full"},{"userValues",userValues}}}}).at("state");
    extension_state=Json::object();if(after_start)after_start();
    commands = Json::array(); message.clear(); sound_cues.clear(); battle_selection=nullptr;refresh(); sync_timeline();
    origin = {{"seed",seed},{"mode","full"},{"initialOverrides",{{"attributes",Json::object()},{"flags",Json::object()},{"relationships",Json::object()}}}};
    if(!content.at("project").value("userFields",Json::array()).empty())origin["userValues"]=userValues;
  }
  Json send(const Json& command) {
    if (state.is_null()) throw std::runtime_error("먼저 새 게임을 시작해 주세요.");
    const auto before=observed();Json result;
    try{result = call({{"operation", "command"}, {"state", state}, {"command", command}, {"expectedRevision", state.at("stateRevision")}});}
    catch(const std::exception& error){if(on_transition)on_transition(before,observed(),command,error.what());throw;}
    state = result.at("state"); commands.push_back(command);
    for(const auto& cue:result.at("transition").value("soundCues",Json::array()))sound_cues.push_back(cue.get<std::string>());
    refresh(); sync_timeline();sync_battle_selection(); if(on_transition)on_transition(before,observed(),command,""); return result.at("transition");
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
  void save(const fs::path& file) {
    if (state.is_null()) throw std::runtime_error("저장할 게임이 없습니다.");
    if(before_save)before_save();
    Json envelope = {{"saveVersion", "1.0.0"}, {"coreVersion", state.at("coreVersion")}, {"contentFingerprint", content.at("contentFingerprint")},
      {"origin", origin},
      {"commands", commands}, {"state", state}};
    if(timeline_active()||!battle_selection.is_null()||!extension_state.empty()){envelope["playerPresentation"]={{"version",1},{"timeline",timeline_cursor}};if(!battle_selection.is_null())envelope["playerPresentation"]["battleSelection"]=battle_selection;}
    if(!extension_state.empty())envelope["playerPresentation"]["extensions"]=extension_state;
    fs::create_directories(file.parent_path());
    auto temp = file; temp += ".tmp";
    { std::ofstream out(temp, std::ios::binary | std::ios::trunc); if (!out) throw std::runtime_error("저장 파일을 열 수 없습니다."); out << envelope.dump(2); out.flush(); if (!out) throw std::runtime_error("저장 파일 쓰기에 실패했습니다."); }
    auto backup = file; backup += ".bak";
    if (fs::exists(backup)) fs::remove(backup);
    if (fs::exists(file)) fs::rename(file, backup);
    try { fs::rename(temp, file); } catch (...) { if (fs::exists(backup) && !fs::exists(file)) fs::rename(backup, file); throw; }
    message = "게임을 저장했습니다.";
  }
  void load(const fs::path& file) {
    const auto saved = read_json(file);
    if(!saved.is_object())throw std::runtime_error("저장 파일은 객체여야 합니다.");
    for(auto it=saved.begin();it!=saved.end();++it)if(it.key()!="saveVersion"&&it.key()!="coreVersion"&&it.key()!="contentFingerprint"&&it.key()!="origin"&&it.key()!="commands"&&it.key()!="state"&&it.key()!="playerPresentation")throw std::runtime_error("저장 파일에 알 수 없는 항목이 있습니다.");
    if (saved.value("saveVersion", "") != "1.0.0" || saved.at("contentFingerprint") != content.at("contentFingerprint")
      || saved.at("origin").value("mode", "") != "full" || !saved.at("commands").is_array() || saved.at("commands").size() > 100000)
      throw std::runtime_error("이 게임과 호환되는 저장 파일이 아닙니다.");
    const auto& savedOrigin=saved.at("origin");const bool hasFields=!content.at("project").value("userFields",Json::array()).empty();
    if(!savedOrigin.is_object()||!savedOrigin.at("initialOverrides").is_object())throw std::runtime_error("저장 생성 정보가 올바르지 않습니다.");
    for(auto it=savedOrigin.at("initialOverrides").begin();it!=savedOrigin.at("initialOverrides").end();++it)if(it.key()!="attributes"&&it.key()!="flags"&&it.key()!="relationships")throw std::runtime_error("저장 초기 값에 알 수 없는 항목이 있습니다.");
    for(auto it=savedOrigin.begin();it!=savedOrigin.end();++it)if(it.key()!="seed"&&it.key()!="mode"&&it.key()!="initialOverrides"&&!(hasFields&&it.key()=="userValues"))throw std::runtime_error("저장 생성 정보에 알 수 없는 항목이 있습니다.");
    if(hasFields&&!savedOrigin.contains("userValues"))throw std::runtime_error("저장 생성 정보에 사용자 입력이 없습니다.");
    Json options={{"mode","full"},{"initialOverrides",savedOrigin.at("initialOverrides")}};if(hasFields)options["userValues"]=savedOrigin.at("userValues");
    Json replay = call({{"operation", "create"}, {"seed", savedOrigin.at("seed")},{"options", options}}).at("state");
    if (saved.at("coreVersion") != replay.at("coreVersion")) throw std::runtime_error("실행 규칙 버전이 다릅니다.");
    for (const auto& command : saved.at("commands")) replay = call({{"operation", "command"}, {"state", replay}, {"command", command}, {"expectedRevision", replay.at("stateRevision")}}).at("state");
    if (replay != saved.at("state")) throw std::runtime_error("저장 데이터와 명령 재현 결과가 다릅니다.");
    const auto restored_plan=plan_activity_timeline(content,replay);Json restored_cursor=restored_plan.is_null()?Json():create_timeline_cursor(restored_plan);
    Json restored_battle=nullptr,restored_extensions=Json::object();
    if(saved.contains("playerPresentation")) {
      const auto presentation=saved.at("playerPresentation");
      if(!presentation.is_object()||!presentation.contains("version")||presentation.at("version")!=1||!presentation.contains("timeline"))throw std::runtime_error("Invalid player presentation save");
      for(auto it=presentation.begin();it!=presentation.end();++it)if(it.key()!="version"&&it.key()!="timeline"&&it.key()!="battleSelection"&&it.key()!="extensions")throw std::runtime_error("Invalid player presentation save");
      if(presentation.contains("extensions")){if(!presentation.at("extensions").is_object())throw std::runtime_error("Invalid extension presentation state");restored_extensions=presentation.at("extensions");}
      const auto cursor=presentation.at("timeline");
      if(cursor.is_null()){if(!restored_plan.is_null())throw std::runtime_error("Missing timeline cursor");restored_cursor=nullptr;}
      else {if(!validate_timeline_cursor(restored_plan,cursor))throw std::runtime_error("Invalid timeline cursor");restored_cursor=cursor;}
      if(presentation.contains("battleSelection")&&!presentation.at("battleSelection").is_null()){
        restored_battle=presentation.at("battleSelection");if(!validate_battle_selection(content,replay,restored_battle))throw std::runtime_error("Invalid battle selection");
        call({{"operation","command"},{"state",replay},{"command",battle_command(restored_battle)},{"expectedRevision",replay.at("stateRevision")}});
      }
    }
    state = replay; commands = saved.at("commands"); origin = saved.at("origin"); seed = origin.at("seed").get<uint32_t>(); sound_cues.clear(); timeline_plan=restored_plan;timeline_cursor=restored_cursor;battle_selection=restored_battle;refresh(); settle();
    extension_state=restored_extensions;if(after_load)after_load();
    message = "저장한 지점에서 이어갑니다.";
  }
};
