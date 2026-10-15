bool unifiedTime(const J &c){return c["project"].value("progression",J::object()).value("model","")=="unified-v2";}
J timeActors(const J &c){J ids=J::array();if(c["project"].contains("roster"))for(const auto &m:c["project"]["roster"]["members"])ids.push_back(m["characterId"]);else ids.push_back(c["project"]["protagonistId"]);return ids;}
J timePool(const J &c,const std::string &id){const auto &cfg=c["project"]["progression"];for(const auto &p:cfg.value("actorPools",J::array()))if(p["actorId"]==id)return p;return cfg["actionPoints"];}
void syncTimePending(J &p){p["pending"]=p["sessions"].empty()?J(nullptr):J{{"remaining",p["sessions"][0]["remaining"]},{"command",p["sessions"][0]["command"]}};}
void fitTimePeriod(const J &c,const J &s,int64_t duration){need(number(s["progression"]["tick"])+duration<=(number(s["periodIndex"])+1)*number(c["project"]["progression"]["ticksPerPeriod"]),"PERIOD_TIME_EXCEEDED","이번 일정 칸의 남은 시간을 초과합니다. 월 일정을 실행·완료하세요.","/progression/tick");}
void reserveTimedAction(const J &c,J &s,const J &cmd,int64_t duration,int64_t cost){
  auto &p=s["progression"],&sessions=p["sessions"];const auto &cfg=c["project"]["progression"];const auto id=cmd.value("actorId",std::string("$shared"));
  bool can=sessions.empty();if(!can&&cfg.value("parallel",false)&&cmd.contains("actorId")){can=true;for(const auto &x:sessions)if(x["kind"]!="action"||x["command"].is_null()||!x["command"].contains("actorId")||x["id"]==id)can=false;}
  need(can,"ACTOR_SESSION_BUSY","이 캐릭터 또는 공용 시간이 이미 사용 중입니다.","/progression/sessions");fitTimePeriod(c,s,duration);
  need(!cmd.contains("actorId")||p["actors"].contains(id),"ACTOR_REQUIRED","시간을 사용할 육성 대상이 없습니다.","/command/actorId");
  auto &account=cmd.contains("actorId")?p["actors"][id]:p;
  need(number(account["actionPoints"])>=cost,"INSUFFICIENT_ACTION_POINTS","행동력이 부족합니다.","/progression/actionPoints");account["actionPoints"]=number(account["actionPoints"])-cost;
  sessions.push_back({{"id",id},{"kind","action"},{"remaining",duration},{"command",cmd}});std::sort(sessions.begin(),sessions.end(),[](const J&a,const J&b){return a["id"]<b["id"];});syncTimePending(p);
}
bool startTimedCalendar(const J &c,J &s){
  if(!unifiedTime(c))return false;
  need(s["phase"]=="ready"&&!s["schedule"].is_null(),"WRONG_PHASE","실행할 일정이 준비되지 않았습니다.","/phase");
  auto &p=s["progression"];const auto &cfg=c["project"]["progression"];need(p["sessions"].empty(),"SESSION_BUSY","다른 세션을 먼저 완료하세요.","/progression/sessions");
  const auto remaining=(number(s["periodIndex"])+1)*number(cfg["ticksPerPeriod"])-number(p["tick"]);need(remaining>=0,"PERIOD_TIME_EXCEEDED","일정과 게임 시간이 일치하지 않습니다.","/progression/tick");
  for(const auto &actor:timeActors(c)){const auto id=actor.get<std::string>();const auto activity=s.contains("roster")?s["roster"]["assignments"][id]:s["schedule"][number(s["periodIndex"])%periods(c)];int64_t cost=0;for(const auto &r:cfg.value("activities",J::array()))if(r["activityId"]==activity)cost=number(r["actionPointCost"]);
    auto &account=p["actors"][id];need(number(account["actionPoints"])>=cost,"INSUFFICIENT_ACTION_POINTS","일정 행동력이 부족합니다.","/progression/actors/"+id);account["actionPoints"]=number(account["actionPoints"])-cost;
  }
  p["sessions"].push_back({{"id","$calendar"},{"kind","calendar"},{"remaining",remaining},{"command",nullptr}});syncTimePending(p);return true;
}
// Returns true when the monthly activity must settle in the outer transaction.
bool advanceUnifiedTime(const J &c,J &s,const J &cmd,J &sounds){
  auto &p=s["progression"],&sessions=p["sessions"];const auto &cfg=c["project"]["progression"];const auto type=str(cmd,"type");
  need(s["phase"]=="planning"||s["phase"]=="ready","WRONG_PHASE","대본이나 진행 결과를 먼저 마치세요.","/phase");
  if(type=="waitTime"){need(sessions.empty(),"SESSION_BUSY","진행 중인 세션을 먼저 마치세요.","/progression/sessions");fitTimePeriod(c,s,number(cmd["ticks"]));sessions.push_back({{"id","$wait"},{"kind","wait"},{"remaining",cmd["ticks"]},{"command",nullptr}});syncTimePending(p);return false;}
  need(!sessions.empty(),"NO_PROGRESS_SESSION","이어갈 진행 세션이 없습니다.","/progression/sessions");
  if(type=="cancelSession"){sessions=J::array();syncTimePending(p);return false;}
  if(type=="cancelActorSession"){size_t index=0;while(index<sessions.size()&&(sessions[index]["command"].is_null()||sessions[index]["command"].value("actorId","")!=cmd["actorId"]))++index;need(index<sessions.size(),"NO_ACTOR_SESSION","이 캐릭터의 진행 세션이 없습니다.","/command/actorId");sessions.erase(index);syncTimePending(p);return false;}
  int64_t remaining=2147483647;for(const auto &x:sessions)remaining=std::min(remaining,number(x["remaining"]));const auto horizon=number(p["tick"])+remaining;J next=nullptr;
  for(const auto &a:cfg["appointments"]){int count=0;for(const auto &h:p["handled"])if(h["id"]==a["id"])++count;const auto at=number(a["atTick"])+count*a.value("repeatEvery",int64_t(0));if(count>=a.value("repeatCount",1)||at>horizon)continue;
    if(next.is_null()||at<number(next["atTick"])||(at==number(next["atTick"])&&a["id"]<next["id"])){next=a;next["occurrence"]=count;next["atTick"]=at;}
  }
  const auto delta=next.is_null()?remaining:std::max<int64_t>(0,number(next["atTick"])-number(p["tick"]));need(number(p["tick"])+delta<=2147483647,"CLOCK_OVERFLOW","게임 시간 한도를 초과합니다.","/progression/tick");
  p["tick"]=number(p["tick"])+delta;p["actionPoints"]=std::min(number(cfg["actionPoints"]["max"]),number(p["actionPoints"])+delta*number(cfg["actionPoints"]["recoveryPerTick"]));
  for(const auto &actor:timeActors(c)){const auto id=actor.get<std::string>();auto &account=p["actors"][id];const auto settings=timePool(c,id);account["actionPoints"]=std::min(number(settings["max"]),number(account["actionPoints"])+delta*number(settings["recoveryPerTick"]));bool busy=false;for(const auto &x:sessions)if(x["kind"]=="calendar"||(!x["command"].is_null()&&x["command"].value("actorId","")==id))busy=true;if(busy)account["elapsed"]=number(account["elapsed"])+delta;}
  for(auto &x:sessions)x["remaining"]=number(x["remaining"])-delta;
  syncTimePending(p);
  if(!next.is_null()){
    const auto event=find(c["events"],next["eventId"]);need(event,"UNKNOWN_NARRATIVE","예약 사건을 찾을 수 없습니다.","/project/progression/appointments");if(next.contains("actorId"))s["roster"]["actorId"]=next["actorId"];
    const bool eligible=(next.contains("repeatEvery")||!eventCompleted(c,s,next["eventId"]))&&s["periodIndex"]>=(*event)["window"]["fromPeriod"]&&s["periodIndex"]<=(*event)["window"]["throughPeriod"]&&condition(c,s,(*event)["requirements"],"/events/"+str(*event,"id")+"/requirements");
    p["handled"].push_back({{"id",next["id"]},{"occurrence",next["occurrence"]},{"atTick",next["atTick"]},{"outcome",eligible?"fired":"skipped"}});
    if(eligible){const auto phase=s["phase"],presentation=s["presentation"];enter(c,s,"event",next["eventId"]);s["activeNarrative"]["returnPhase"]=phase;s["activeNarrative"]["returnPresentation"]=presentation;}return false;
  }
  size_t index=0;while(number(sessions[index]["remaining"])!=0)++index;const auto done=sessions[index];sessions.erase(index);syncTimePending(p);
  if(done["kind"]=="calendar")return true;
  progressionExecute(c,s,done["command"],sounds);return false;
}
