// Optional actor ownership. Legacy targets keep their original storage paths.
std::string actorContext(const J &s) {
  if(!s.at("activeNarrative").is_null()&&s.at("activeNarrative").contains("actorId"))return str(s.at("activeNarrative"),"actorId");
  return s.contains("roster")?s.at("roster").value("actorId",std::string()):std::string();
}
std::string ownerId(const J &c,const J &s,const J &owner,const std::string &p) {
  if(owner.is_null())return "";
  need(c.at("project").contains("roster")&&c.at("project").at("roster").value("stateModel","")=="character-v1"&&s.contains("roster")&&s.at("roster").contains("inventory"),"CHARACTER_STATE_REQUIRED","캐릭터 소유 상태 설정이 필요합니다.",p);
  const auto id=owner.value("kind","")=="character"?str(owner,"characterId"):actorContext(s);
  bool valid=false;for(const auto &m:c.at("project").at("roster").at("members"))if(m.at("characterId")==id)valid=true;
  need(!id.empty()&&valid,"ACTOR_REQUIRED","유효한 실행자 또는 지정 캐릭터가 필요합니다.",p);return id;
}
bool eventCompleted(const J &c,const J &s,const J &id,const J &owner=nullptr) {
  if(!owner.is_null()){const auto actor=ownerId(c,s,owner,"/eventCompleted/owner");return contains(s.at("roster").at("completedEvents").at(actor),id);}
  const auto event=find(c.at("events"),id);
  if(event&&event->value("scope","")=="actor"){
    const auto actor=actorContext(s);return !actor.empty()&&s.contains("roster")&&s.at("roster").contains("completedEvents")&&contains(s.at("roster").at("completedEvents").at(actor),id);
  }
  return contains(s.at("completedEvents"),id);
}
void createCharacterState(const J &c,J &s){
  if(!c["project"].contains("roster")||c["project"]["roster"].value("stateModel","")!="character-v1")return;
  auto &r=s["roster"];r["inventory"]=J::object();r["relationships"]=J::object();r["completedEvents"]=J::object();
  for(const auto &m:c["project"]["roster"]["members"]){const auto id=str(m,"characterId");
    r["inventory"][id]=J::object();for(const auto &item:c["project"].value("gameplay",J::object()).value("items",J::array()))r["inventory"][id][str(item,"id")]=m.value("initialInventory",J::object()).value(str(item,"id"),item.value("initialQuantity",0));
    r["relationships"][id]=s["relationships"];
    const auto initial=m.value("initialRelationships",J::object());for(const auto &[other,values]:initial.items())for(const auto &[key,value]:values.items())r["relationships"][id][other][key]=value;
    r["completedEvents"][id]=J::array();
  }
}
void beginActorAction(const J &c,J &s,const J &cmd){
  const auto actor=ownerId(c,s,{{"kind","character"},{"characterId",cmd["actorId"]}},"/command/actorId");
  bool bound=false;for(const auto &m:c["project"]["roster"]["members"])if(m["characterId"]==actor&&contains(m.value("actionIds",J::array()),cmd["actionId"]))bound=true;
  need(bound,"ACTOR_ACTION_UNBOUND","이 캐릭터에게 연결되지 않은 행동입니다.","/command/actionId");
  const auto actions=c["project"].value("gameplay",J::object()).value("actions",J::array());const auto action=find(actions,cmd["actionId"]);
  need(!action||!action->contains("expeditionId"),"ACTOR_SESSION_UNSUPPORTED","대상 지정 행동의 탐험 진입은 별도 세션 계약이 필요합니다.","/command/actionId");s["roster"]["actorId"]=actor;
}
