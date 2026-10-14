// Optional multi-trainee extension; legacy state and command replay remain unchanged.
void assignActivities(const J &c, const J &old, J &s, const J &assignments) {
  need(c["project"].contains("roster") && s.contains("roster"), "ROSTER_REQUIRED", "다인 육성 설정이 필요합니다.", "/project/roster");
  need(old["phase"] == "planning" || old["phase"] == "ready", "WRONG_PHASE", "활동 배정을 기다리는 상태가 아닙니다.", "/phase");
  const auto &members=c["project"]["roster"]["members"];
  need(assignments.size()==members.size(), "ROSTER_ASSIGNMENTS", "모든 육성 대상에게 활동을 하나씩 배정해야 합니다.", "/command/assignments");
  for(const auto &m:members) {
    const auto id=str(m,"characterId"),p="/command/assignments/"+id;
    need(assignments.contains(id),"ROSTER_ASSIGNMENTS","모든 육성 대상에게 활동을 하나씩 배정해야 합니다.",p);
    const auto a=find(c["activities"],assignments[id]);
    need(a,"UNKNOWN_ACTIVITY",id+": 활동을 찾을 수 없습니다.",p);
    need(!a->contains("expeditionId"),"ROSTER_SESSION_UNSUPPORTED","다인 활동에서 탐험 진입은 아직 지원하지 않습니다.",p);
  }
  s["roster"]["assignments"]=assignments;
  s["schedule"]=J::array();for(int i=0;i<periods(c);i++)s["schedule"].push_back(assignments[str(c["project"],"protagonistId")]);
  s["phase"]="ready";
}

J executeRoster(const J &c,J &s) {
  const auto &r=c["project"]["roster"];
  const auto primary=str(c["project"],"protagonistId");
  for(const auto &key:r["localAttributeIds"])s["roster"]["attributes"][primary][key.get<std::string>()]=s["attributes"][key.get<std::string>()];
  J results=J::array();
  for(const auto &m:r["members"]) {
    const auto id=str(m,"characterId");
    for(const auto &key:r["localAttributeIds"])s["attributes"][key.get<std::string>()]=s["roster"]["attributes"][id][key.get<std::string>()];
    s["schedule"]=J::array();for(int i=0;i<periods(c);i++)s["schedule"].push_back(s["roster"]["assignments"][id]);
    s["phase"]="ready";
    const J before=s;
    const J result=execute(c,before,s,true);
    for(const auto &key:r["localAttributeIds"])s["roster"]["attributes"][id][key.get<std::string>()]=s["attributes"][key.get<std::string>()];
    results.push_back({{"characterId",id},{"result",result}});
  }
  for(const auto &key:r["localAttributeIds"])s["attributes"][key.get<std::string>()]=s["roster"]["attributes"][primary][key.get<std::string>()];
  s["schedule"]=J::array();for(int i=0;i<periods(c);i++)s["schedule"].push_back(s["roster"]["assignments"][primary]);
  J result=results[0]["result"];result["actorResults"]=results;
  s["lastActivityResult"]=result;s["phase"]="activity-result";
  return result;
}
