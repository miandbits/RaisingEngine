// Optional discrete game clock. Legacy calendar/state are untouched when absent.
void progressionGuard(const J &s,const J &cmd){
  const auto type=str(cmd,"type");
  if(s.contains("progression")&&s["progression"].contains("sessions")){
    need(s["progression"]["sessions"].empty()||type=="advanceSession"||type=="cancelSession"||type=="cancelActorSession"||type=="advanceNarrative"||type=="choose"||type=="navigate"||type=="executeActorAction","SESSION_BUSY","진행 중인 세션을 마치거나 다른 캐릭터의 시간 행동을 시작하세요.","/progression/sessions");return;
  }
  need(!s.contains("progression")||s["progression"]["pending"].is_null()||type=="advanceSession"||type=="cancelSession"||type=="advanceNarrative"||type=="choose","SESSION_BUSY","진행 중인 행동을 이어가거나 취소하세요.","/progression/pending");
}
void progressionExecute(const J &c,J &s,const J &cmd,J &sounds){
  if(cmd.is_null())return;
  if(cmd.contains("actorId"))beginActorAction(c,s,cmd);
  auto action=cmd;action.erase("actorId");gameplay(c,s,action,sounds);
}
#include "unified-time.inc"
bool startTimedAction(const J &c,J &s,const J &cmd){
  if(!c["project"].contains("progression"))return false;
  const auto &config=c["project"]["progression"];const J *rule=nullptr;
  for(const auto &v:config["actions"])if(v["actionId"]==cmd["actionId"]){rule=&v;break;}
  if(!rule){need(!s["progression"].contains("sessions")||s["progression"]["sessions"].empty(),"SESSION_BUSY","병렬 진행에는 시간 설정된 행동이 필요합니다.","/progression/sessions");return false;}
  need(s["mode"]=="full"&&(s["phase"]=="planning"||s["phase"]=="ready"),"WRONG_PHASE","행동을 시작할 수 있는 상태가 아닙니다.","/phase");
  auto &p=s["progression"];const auto cost=number((*rule)["actionPointCost"])*number(cmd["quantity"]),duration=number((*rule)["duration"])*number(cmd["quantity"]);
  if(!unifiedTime(c))need(number(p["actionPoints"])>=cost,"INSUFFICIENT_ACTION_POINTS","행동력이 부족합니다.","/progression/actionPoints");
  need(duration<=2147483647LL,"DURATION_OVERFLOW","행동 소요 시간 한도를 초과합니다.","/command/quantity");
  auto action=cmd;action["type"]="executeAction";J probe=s,sounds=J::array();const auto saved=*activeRules;
  try{TraceScope quiet(nullptr);progressionExecute(c,probe,action,sounds);}catch(...){*activeRules=saved;throw;}*activeRules=saved;
  if(unifiedTime(c)){reserveTimedAction(c,s,action,duration,cost);return true;}
  p["actionPoints"]=number(p["actionPoints"])-cost;p["pending"]={{"remaining",duration},{"command",action}};traceExecution({{"kind","session-start"},{"actionId",cmd["actionId"]},{"path","/progression/pending"},{"status","pending"},{"value",p["pending"]}});return true;
}
bool progressSession(const J &c,J &s,const J &cmd,J &sounds){
  need(c["project"].contains("progression")&&s.contains("progression"),"PROGRESSION_DISABLED","시간·진행 세션 설정이 없습니다.","/project/progression");
  if(unifiedTime(c))return advanceUnifiedTime(c,s,cmd,sounds);
  need(cmd["type"]!="cancelActorSession","PROGRESSION_MODEL","개인 세션 취소에는 통합 시간 모드가 필요합니다.","/project/progression/model");
  need(s["mode"]=="full"&&(s["phase"]=="planning"||s["phase"]=="ready"),"WRONG_PHASE","대본이나 진행 결과를 먼저 마치세요.","/phase");
  auto &p=s["progression"];const auto &config=c["project"]["progression"];const auto type=str(cmd,"type");
  if(type=="waitTime"){p["pending"]={{"remaining",cmd["ticks"]},{"command",nullptr}};return false;}
  need(!p["pending"].is_null(),"NO_PROGRESS_SESSION","이어갈 진행 세션이 없습니다.","/progression/pending");
  if(type=="cancelSession"){p["pending"]=nullptr;traceExecution({{"kind","session-cancel"},{"path","/progression/pending"},{"status","cancelled"}});return false;}
  const auto pending=p["pending"];J next=nullptr;
  for(const auto &a:config["appointments"]){bool handled=false;for(const auto &h:p["handled"])if(h["id"]==a["id"])handled=true;
    if(handled||number(a["atTick"])>number(p["tick"])+number(pending["remaining"]))continue;
    if(next.is_null()||a["atTick"]<next["atTick"]||(a["atTick"]==next["atTick"]&&a["id"]<next["id"]))next=a;
  }
  const auto delta=next.is_null()?number(pending["remaining"]):std::max<int64_t>(0,number(next["atTick"])-number(p["tick"]));
  need(number(p["tick"])+delta<=2147483647LL,"CLOCK_OVERFLOW","게임 시간 한도를 초과합니다.","/progression/tick");
  p["tick"]=number(p["tick"])+delta;p["pending"]["remaining"]=number(pending["remaining"])-delta;
  p["actionPoints"]=std::min(number(config["actionPoints"]["max"]),number(p["actionPoints"])+delta*number(config["actionPoints"]["recoveryPerTick"]));
  if(!next.is_null()){
    const auto event=find(c["events"],next["eventId"]);need(event,"UNKNOWN_NARRATIVE","예약 사건을 찾을 수 없습니다.","/project/progression/appointments");
    if(next.contains("actorId"))s["roster"]["actorId"]=next["actorId"];
    const bool eligible=!eventCompleted(c,s,next["eventId"])&&s["periodIndex"]>=(*event)["window"]["fromPeriod"]&&s["periodIndex"]<=(*event)["window"]["throughPeriod"]&&condition(c,s,(*event)["requirements"],"/events/"+str(*event,"id")+"/requirements");
    p["handled"].push_back({{"id",next["id"]},{"outcome",eligible?"fired":"skipped"}});traceExecution({{"kind","appointment"},{"path","/project/progression/appointments"},{"status",eligible?"fired":"skipped"},{"value",next["id"]}});
    if(eligible){const auto phase=s["phase"],presentation=s["presentation"];enter(c,s,"event",next["eventId"]);s["activeNarrative"]["returnPhase"]=phase;s["activeNarrative"]["returnPresentation"]=presentation;}return false;
  }
  p["pending"]=nullptr;traceExecution({{"kind","session-complete"},{"path","/progression"},{"status","settled"},{"value",p["tick"]}});progressionExecute(c,s,pending["command"],sounds);
  return false;
}
