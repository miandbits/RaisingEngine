J systemsAvailability(const J &c, const J &s, const J &cmd) {
  try { J draft = s, sounds = J::array(); systems(c, draft, cmd, sounds); return {{"enabled", true}, {"reason", nullptr}}; }
  catch (const Fault &e) { return {{"enabled", false}, {"reason", e.what()}}; }
}
J systemsView(const J &c, const J &s) {
  if (!c["project"].contains("systems") || s.is_null() || !s.contains("systems")) return nullptr;
  const auto &d = c["project"]["systems"], &sys = s["systems"];
  J equipment = J::array(), quests = J::array(), consumables = J::array(), stages = J::array(),battleActions=J::array();
  for(const auto& action:std::vector<std::string>{"attack","guard","flee"}){auto row=systemsAvailability(c,s,{{"type","battleAction"},{"action",action}});row["id"]=action;row["name"]=action=="attack"?"공격":action=="guard"?"방어":"도주";row["battleAction"]=action;row["target"]=action=="attack"?"enemy":action=="guard"?"self":"none";battleActions.push_back(row);}
  for (const auto &e : d["equipment"]) {
    auto row = systemsAvailability(c, s, {{"type", "equip"}, {"equipmentId", e["id"]}});
    row["id"] = e["id"]; row["name"] = e["name"]; row["itemId"] = e["itemId"]; row["slot"] = e["slot"];
    row["equipped"] = sys["equipment"][str(e,"slot")] == e["id"];
    row["canUnequip"] = row["equipped"].get<bool>() && systemsAvailability(c, s, {{"type", "unequip"}, {"slot", e["slot"]}})["enabled"].get<bool>();
    row["owned"] = s["gameplay"]["inventory"].value(str(e,"itemId"), int64_t(0)); equipment.push_back(row);
  }
  for (const auto &q : d["quests"]) {
    auto accept = systemsAvailability(c, s, {{"type", "acceptQuest"}, {"questId", q["id"]}}), claim = systemsAvailability(c, s, {{"type", "claimQuest"}, {"questId", q["id"]}});
    auto id = str(q,"id"); bool saved = sys["quests"].contains(id);
    quests.push_back({{"id", id}, {"name", q["name"]}, {"status", saved ? sys["quests"][id]["status"] : J(accept["enabled"].get<bool>() ? "available" : "locked")}, {"progress", saved ? sys["quests"][id]["progress"] : J(0)}, {"targetCount", q["targetCount"]}, {"canAccept", accept["enabled"]}, {"canClaim", claim["enabled"]}, {"reason", saved ? claim["reason"] : accept["reason"]}});
  }
  for (const auto &item : d["consumables"]) {
    auto definition = find(c["project"]["gameplay"]["items"], item["itemId"]);
    consumables.push_back({{"itemId", item["itemId"]}, {"name", (*definition)["name"]}, {"quantity", s["gameplay"]["inventory"].value(str(item,"itemId"), int64_t(0))}, {"heal", item["heal"]}, {"enabled", systemsAvailability(c, s, {{"type", "battleAction"}, {"action", "useItem"}, {"itemId", item["itemId"]}})["enabled"]}});
    auto row=systemsAvailability(c,s,{{"type","battleAction"},{"action","useItem"},{"itemId",item["itemId"]}});row["id"]="use-item--"+str(item,"itemId");row["name"]=(*definition)["name"];row["battleAction"]="useItem";row["itemId"]=item["itemId"];row["target"]="self";battleActions.push_back(row);
  }
  for (const auto &stage : d["relationshipStages"]) stages.push_back({{"id", stage["id"]}, {"name", stage["name"]}, {"unlocked", contains(sys["unlockedStages"], stage["id"])}, {"characterId", stage["characterId"]}, {"relationshipId", stage["relationshipId"]}, {"threshold", stage["threshold"]}});
  J expedition = nullptr, battle = nullptr;
  if (!sys["expedition"].is_null()) {
    const auto &trip = sys["expedition"]; auto definition = find(d["expeditions"], trip["expeditionId"]); auto node = find((*definition)["nodes"], trip["nodeId"]); J connections = J::array();
    for (const auto &id : (*node)["connections"]) connections.push_back({{"id", id}, {"name", (*find((*definition)["nodes"], id))["name"]}, {"enabled", s["phase"] == "expedition"}});
    expedition = {{"expeditionId", trip["expeditionId"]}, {"name", (*definition)["name"]}, {"nodeId", trip["nodeId"]}, {"nodeName", (*node)["name"]}, {"heroHp", trip["heroHp"]}, {"stats", trip["stats"]}, {"connections", connections}, {"canReturn", s["phase"] == "expedition"}};
    if (!node->value("description", "").empty()) expedition["nodeDescription"] = (*node)["description"];
    if (!sys["battle"].is_null()) { const auto &b = sys["battle"]; auto enemy = find(d["enemies"], b["enemyId"]); battle = {{"enemyId", b["enemyId"]}, {"enemyName", (*enemy)["name"]}, {"enemyHp", b["enemyHp"]}, {"enemyMaxHp", (*enemy)["maxHp"]}, {"turn", b["turn"]}, {"heroHp", trip["heroHp"]}, {"heroMaxHp", trip["stats"]["maxHp"]}};if(b.contains("log"))battle["log"]=b["log"]; }
  }
  return {{"heroStats", heroStats(c,s)}, {"equipment", equipment}, {"quests", quests}, {"consumables", consumables},{"battleActions",battleActions}, {"expedition", expedition}, {"battle", battle}, {"stages", stages}, {"lastBattle", sys["lastBattle"]}, {"lastExpedition", sys["lastExpedition"]}};
}
