// Designer states are optional and never alter the engine phase or legacy save shape.
void createCustomStates(const J &c, J &s) {
  const auto groups = c["project"].value("stateGroups", J::array());
  if (groups.empty()) return;
  J shared = J::object(), characters = J::object();
  for (const auto &g : groups) {
    if (g["scope"] == "shared") shared[str(g,"id")] = g["initialValueId"];
    else for (const auto &m : c["project"]["roster"]["members"])
      characters[str(m,"characterId")][str(g,"id")] = g["initialValueId"];
  }
  s["customStates"] = {{"shared",shared},{"characters",characters}};
}
struct CustomStateSlot { std::string owner, before; };
CustomStateSlot customStateSlot(const J &c,const J &s,const J &rule,const std::string &p) {
  const auto groups=c["project"].value("stateGroups",J::array());
  const auto id=str(rule,"stateGroupId"), value=str(rule,"stateValueId");
  const auto g=find(groups,id);
  need(g,"UNKNOWN_STATE_GROUP","상태 그룹을 찾을 수 없습니다.",p+"/stateGroupId");
  need(find((*g)["values"],value),"UNKNOWN_STATE_VALUE","그룹에 속한 상태 값을 선택하세요.",p+"/stateValueId");
  need(((*g)["scope"]=="character")==rule.contains("owner"),"STATE_SCOPE","상태 그룹의 소유 범위와 대상이 다릅니다.",p+"/owner");
  const auto owner=ownerId(c,s,rule.value("owner",J()),p+"/owner");
  need(s.contains("customStates"),"STATE_STORAGE","저장된 상태가 없습니다.",p);
  const auto &storage=s["customStates"];
  need(owner.empty()||storage["characters"].contains(owner),"STATE_STORAGE","캐릭터 상태가 없습니다.",p);
  const auto &values=owner.empty()?storage["shared"]:storage["characters"][owner];
  need(values.contains(id),"STATE_STORAGE","저장된 상태가 없습니다.",p);
  return {owner,values[id].get<std::string>()};
}
