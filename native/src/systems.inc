J createSystems() {
  return {{"equipment", {{"weapon", nullptr}, {"armor", nullptr}, {"accessory", nullptr}}},
          {"quests", J::object()}, {"unlockedStages", J::array()}, {"pendingStageEvents", J::array()},
          {"expedition", nullptr}, {"battle", nullptr}, {"lastBattle", nullptr}, {"lastExpedition", nullptr}};
}
int equipmentReserved(const J &c, const J &s, const std::string &itemId) {
  if (!s.contains("systems") || !c["project"].contains("systems")) return 0;
  int count = 0;
  for (auto &id : s["systems"]["equipment"]) if (!id.is_null()) {
    auto e = find(c["project"]["systems"]["equipment"], id);
    if (e && (*e)["itemId"] == itemId) ++count;
  }
  return count;
}
J heroStats(const J &c, const J &s) {
  const auto &d = c["project"]["systems"], &hero = d["hero"];
  J result = {{"maxHp", s["attributes"][str(hero, "maxHpAttributeId")]}, {"attack", s["attributes"][str(hero, "attackAttributeId")]}, {"defense", s["attributes"][str(hero, "defenseAttributeId")]}};
  for (const auto &id : s["systems"]["equipment"]) if (!id.is_null()) {
    auto e = find(d["equipment"], id);
    if (e) for (auto key : {"maxHp", "attack", "defense"}) result[key] = i32(number(result[key]) + number((*e)["modifiers"][key]), std::string("/systems/equipment/") + key);
  }
  result["maxHp"] = std::max<int64_t>(1, number(result["maxHp"]));
  result["attack"] = std::max<int64_t>(0, number(result["attack"]));
  result["defense"] = std::max<int64_t>(0, number(result["defense"]));
  return result;
}
J systemsReward(const J &c, J &s, const J &value, const std::string &source, bool discard, J *recorded=nullptr) {
  J discarded = J::array();
  for (const auto &entry : value["items"]) {
    auto id = str(entry, "itemId"); const auto *item = find(c["project"]["gameplay"]["items"], id);
    auto before = s["gameplay"]["inventory"].value(id, int64_t(0));
    auto after = i32(before + number(entry["quantity"]), source), maximum = number((*item)["maxStack"]);
    need(discard || after <= maximum, "INVENTORY_FULL", "보상을 받을 소지품 공간이 부족합니다.", "/gameplay/inventory/" + id);
    s["gameplay"]["inventory"][id] = std::min(maximum, after);
    if(recorded)recorded->push_back({{"kind","numeric"},{"source",source+"/items/"+id},{"target",{{"kind","inventory"},{"itemId",id}}},{"before",before},{"operand",entry["quantity"]},{"requested",after},{"after",std::min(maximum,after)},{"delta",std::min(maximum,after)-before},{"clamped",after>maximum?J("max"):J(false)}});
    if (after > maximum) discarded.push_back({{"itemId", id}, {"quantity", after - maximum}});
  }
  J changes = J::array(); effects(c, s, value["effects"], source + "/effects", changes);
  if(recorded)for(const auto& change:changes)recorded->push_back(change);
  return discarded;
}
void enterExpeditionNode(const J &c, J &s, const J &nodeId, J &sounds) {
  auto &sys = s["systems"], &trip = sys["expedition"];
  const auto &d = c["project"]["systems"];
  auto definition = find(d["expeditions"], trip["expeditionId"]);
  auto node = find((*definition)["nodes"], nodeId);
  trip["nodeId"] = (*node)["id"];
  if (node->contains("presentation")) scene(s, (*node)["presentation"], sounds);
  s["phase"] = "expedition";
  if (node->contains("enemyId") && !contains(trip["clearedNodes"], (*node)["id"])) {
    auto enemy = find(d["enemies"], (*node)["enemyId"]);
    sys["battle"] = {{"enemyId", (*enemy)["id"]}, {"enemyHp", (*enemy)["maxHp"]}, {"turn", 0}};
    if(d.contains("battleHistoryLimit"))sys["battle"]["log"]=J::array();
    s["phase"] = "battle";
  }
}
void startExpedition(const J &c, J &s, const J &id, J &sounds) {
  need(c["project"].contains("systems") && s.contains("systems") && s["mode"] == "full", "SYSTEMS_UNAVAILABLE", "탐험 시스템이 있는 전체 실행 모드가 필요합니다.", "/project/systems");
  const auto &d = c["project"]["systems"]; auto &sys = s["systems"];
  auto expedition = find(d["expeditions"], id);
  need(expedition, "UNKNOWN_EXPEDITION", "탐험을 찾을 수 없습니다.", "/activity/expeditionId");
  need(s.contains("gameplay") && s["gameplay"]["locationId"] == (*expedition)["locationId"], "WRONG_LOCATION", "탐험 출발 장소로 이동해 주세요.", "/gameplay/locationId");
  need(sys["expedition"].is_null(), "EXPEDITION_ACTIVE", "이미 진행 중인 탐험이 있습니다.", "/systems/expedition");
  auto stats = heroStats(c, s);
  sys["expedition"] = {{"expeditionId", id}, {"nodeId", (*expedition)["entryNodeId"]}, {"clearedNodes", J::array()}, {"heroHp", stats["maxHp"]}, {"stats", stats}, {"returnPresentation", s["presentation"]}};
  sys["battle"] = nullptr; sys["lastBattle"] = nullptr; sys["lastExpedition"] = nullptr;
  enterExpeditionNode(c, s, (*expedition)["entryNodeId"], sounds);
}
void finishExpedition(J &s, const std::string &outcome) {
  auto &sys = s["systems"]; const auto trip = sys["expedition"];
  sys["lastExpedition"] = {{"expeditionId", trip["expeditionId"]}, {"outcome", outcome}, {"clearedNodes", trip["clearedNodes"]}};
  s["presentation"] = trip["returnPresentation"]; sys["expedition"] = nullptr; sys["battle"] = nullptr; s["phase"] = trip.value("returnPhase", "activity-result");
}
void systems(const J &c, J &s, const J &cmd, J &sounds) {
  need(c["project"].contains("systems") && s.contains("systems") && s.contains("gameplay") && s["mode"] == "full", "SYSTEMS_UNAVAILABLE", "확장 시스템이 있는 전체 실행 모드가 필요합니다.", "/project/systems");
  const auto &d = c["project"]["systems"]; auto &sys = s["systems"]; auto type = str(cmd, "type");
  if (type == "equip" || type == "unequip" || type == "acceptQuest" || type == "claimQuest") {
    need(s["phase"] == "planning" || s["phase"] == "ready", "WRONG_PHASE", "일정 편성 중에만 장비·퀘스트를 변경할 수 있습니다.", "/phase");
    if (type == "equip") {
      auto e = find(d["equipment"], cmd["equipmentId"]); need(e, "UNKNOWN_EQUIPMENT", "장비를 찾을 수 없습니다.", "/command/equipmentId");
      auto slot = str(*e, "slot"), item = str(*e, "itemId");
      need(sys["equipment"][slot] != (*e)["id"], "ALREADY_EQUIPPED", "이미 장착한 장비입니다.", "/command/equipmentId");
      need(s["gameplay"]["inventory"].value(item, int64_t(0)) - equipmentReserved(c, s, item) > 0, "INSUFFICIENT_ITEMS", "장착 가능한 물품이 없습니다.", "/gameplay/inventory/" + item);
      sys["equipment"][slot] = (*e)["id"]; heroStats(c, s); return;
    }
    if (type == "unequip") {
      auto slot = str(cmd, "slot"); need(!sys["equipment"][slot].is_null(), "SLOT_EMPTY", "장착한 장비가 없습니다.", "/command/slot"); sys["equipment"][slot] = nullptr; return;
    }
    auto quest = find(d["quests"], cmd["questId"]); need(quest, "UNKNOWN_QUEST", "퀘스트를 찾을 수 없습니다.", "/command/questId");
    need(s["gameplay"]["locationId"] == (*quest)["locationId"], "WRONG_LOCATION", "퀘스트 게시판이 있는 장소로 이동해 주세요.", "/gameplay/locationId");
    auto id = str(*quest, "id");
    if (type == "acceptQuest") {
      need(!sys["quests"].contains(id), "QUEST_ACCEPTED", "이미 수락한 퀘스트입니다.", "/command/questId");
      need(condition(c, s, (*quest)["requirements"], "/systems/quests/" + id + "/requirements"), "QUEST_LOCKED", "퀘스트 수락 조건을 만족하지 않습니다.", "/command/questId");
      sys["quests"][id] = {{"status", "accepted"}, {"progress", 0}};
    } else {
      need(sys["quests"].contains(id) && sys["quests"][id]["status"] == "completed", "QUEST_NOT_COMPLETED", "보상을 받을 완료 퀘스트가 아닙니다.", "/command/questId");
      systemsReward(c, s, (*quest)["rewards"], "/systems/quests/" + id + "/rewards", false); sys["quests"][id]["status"] = "claimed";
    }
    return;
  }
  need(!sys["expedition"].is_null(), "NO_EXPEDITION", "진행 중인 탐험이 없습니다.", "/systems/expedition");
  auto &trip = sys["expedition"]; auto expedition = find(d["expeditions"], trip["expeditionId"]); auto node = find((*expedition)["nodes"], trip["nodeId"]);
  if (type == "moveExpedition" || type == "returnFromExpedition") {
    need(s["phase"] == "expedition", "WRONG_PHASE", "전투가 끝난 뒤 탐험 이동·귀환을 할 수 있습니다.", "/phase");
    if (type == "returnFromExpedition") { finishExpedition(s, node->value("exit", false) ? "completed" : "retreated"); return; }
    need(contains((*node)["connections"], cmd["nodeId"]), "NODE_DISCONNECTED", "현재 탐험 지점과 연결되지 않았습니다.", "/command/nodeId");
    need(find((*expedition)["nodes"], cmd["nodeId"]), "UNKNOWN_NODE", "탐험 지점을 찾을 수 없습니다.", "/command/nodeId"); enterExpeditionNode(c, s, cmd["nodeId"], sounds); return;
  }
  need(s["phase"] == "battle" && !sys["battle"].is_null(), "WRONG_PHASE", "진행 중인 전투가 없습니다.", "/phase");
  auto &battle = sys["battle"]; auto enemy = find(d["enemies"], battle["enemyId"]); auto id = str(*enemy, "id"), action = str(cmd, "action");
  auto turn = i32(number(battle["turn"]) + 1, "/systems/battle/turn"); int64_t heroDamage = 0, enemyDamage = 0,heroHeal=0;
  const auto heroHpBefore=trip["heroHp"],enemyHpBefore=battle["enemyHp"];
  auto record=[&](){if(battle.contains("log")){J entry={{"id","turn-"+std::to_string(turn)},{"turn",turn},{"action",action},{"target",action=="attack"?"enemy":action=="flee"?"none":"self"},{"heroHpBefore",heroHpBefore},{"heroHpAfter",trip["heroHp"]},{"enemyHpBefore",enemyHpBefore},{"enemyHpAfter",battle["enemyHp"]},{"heroDamage",heroDamage},{"enemyDamage",enemyDamage},{"heroHeal",heroHeal}};if(action=="useItem")entry["itemId"]=cmd["itemId"];auto& log=battle["log"];log.push_back(entry);const size_t limit=d.value("battleHistoryLimit",128);if(log.size()>limit)log.erase(log.begin(),log.begin()+(log.size()-limit));}};
  auto result = [&](const std::string &outcome, J discarded = J::array(),J rewardChanges=J::array()) { sys["lastBattle"] = {{"enemyId", id}, {"outcome", outcome}, {"turn", turn}, {"heroDamage", heroDamage}, {"enemyDamage", enemyDamage}, {"discardedItems", discarded}};if(battle.contains("log")){sys["lastBattle"]["log"]=battle["log"];sys["lastBattle"]["rewardChanges"]=rewardChanges;} };
  if (action == "flee") { record();result("fled"); finishExpedition(s, "retreated"); return; }
  if (action == "useItem") {
    const J *item = nullptr; for (const auto &i : d["consumables"]) if (i["itemId"] == cmd["itemId"]) item = &i;
    need(item, "UNKNOWN_CONSUMABLE", "전투 회복 물품이 아닙니다.", "/command/itemId"); auto itemId = str(*item, "itemId");
    need(s["gameplay"]["inventory"].value(itemId, int64_t(0)) - equipmentReserved(c, s, itemId) > 0, "INSUFFICIENT_ITEMS", "사용 가능한 회복 물품이 없습니다.", "/gameplay/inventory/" + itemId);
    need(number(trip["heroHp"]) < number(trip["stats"]["maxHp"]), "HP_FULL", "체력이 이미 가득 찼습니다.", "/systems/expedition/heroHp");
    s["gameplay"]["inventory"][itemId] = number(s["gameplay"]["inventory"][itemId]) - 1;
    trip["heroHp"] = std::min(number(trip["stats"]["maxHp"]), i32(number(trip["heroHp"]) + number((*item)["heal"]), "/systems/expedition/heroHp"));
    heroHeal=number(trip["heroHp"])-number(heroHpBefore);
  } else if (action == "attack") {
    auto roll = sample(s["rng"], 3);
    enemyDamage = std::min(number(battle["enemyHp"]), std::max<int64_t>(1, number(trip["stats"]["attack"]) - number((*enemy)["defense"]) + int64_t(roll) - 1)); battle["enemyHp"] = number(battle["enemyHp"]) - enemyDamage;
  }
  battle["turn"] = turn;
  if (number(battle["enemyHp"]) == 0) {
    J rewardChanges=J::array();auto discarded = systemsReward(c, s, (*enemy)["rewards"], "/systems/enemies/" + id + "/rewards", true,battle.contains("log")?&rewardChanges:nullptr);
    trip["clearedNodes"].push_back((*node)["id"]);
    for (const auto &quest : d["quests"]) { auto qid = str(quest, "id"); if (quest["enemyId"] == id && sys["quests"].contains(qid) && sys["quests"][qid]["status"] == "accepted") {
      auto &q = sys["quests"][qid]; q["progress"] = std::min(number(quest["targetCount"]), number(q["progress"]) + 1); if (q["progress"] >= quest["targetCount"]) q["status"] = "completed";
    } }
    record();result("victory", discarded,rewardChanges); sys["battle"] = nullptr; s["phase"] = "expedition"; return;
  }
  auto roll = sample(s["rng"], 3); auto damage = std::max<int64_t>(1, number((*enemy)["attack"]) - number(trip["stats"]["defense"]) + int64_t(roll) - 1);
  heroDamage = std::min(number(trip["heroHp"]), action == "guard" ? (damage + 1) / 2 : damage); trip["heroHp"] = number(trip["heroHp"]) - heroDamage;
  record();
  if (number(trip["heroHp"]) == 0) { result("defeat"); finishExpedition(s, "defeated"); }
}
void syncRelationshipStages(const J &c, J &s, bool finishingPeriod) {
  if (!c["project"].contains("systems") || !s.contains("systems") || s["mode"] != "full") return;
  const auto &d = c["project"]["systems"]; auto &sys = s["systems"]; auto stages = d["relationshipStages"];
  std::sort(stages.begin(), stages.end(), [](const J &a, const J &b) { return a["threshold"] != b["threshold"] ? number(a["threshold"]) < number(b["threshold"]) : str(a,"id") < str(b,"id"); });
  for (const auto &stage : stages) if (!contains(sys["unlockedStages"], stage["id"]) && number(s["relationships"][str(stage,"characterId")][str(stage,"relationshipId")]) >= number(stage["threshold"])) {
    sys["unlockedStages"].push_back(stage["id"]);
    if (stage.contains("eventId") && !contains(sys["pendingStageEvents"], stage["eventId"]) && !contains(s["completedEvents"], stage["eventId"])) sys["pendingStageEvents"].push_back(stage["eventId"]);
  }
  if (s["phase"] != "planning" && s["phase"] != "ready" && s["phase"] != "period-end") return;
  auto pending = sys["pendingStageEvents"];
  auto remove = [&](const J &id) { auto &q = sys["pendingStageEvents"]; q.erase(std::remove(q.begin(), q.end(), id), q.end()); };
  for (const auto &id : pending) {
    auto event = find(c["events"], id);
    if (contains(s["completedEvents"], id) || s["periodIndex"] > (*event)["window"]["throughPeriod"]) { remove(id); continue; }
    if (s["periodIndex"] < (*event)["window"]["fromPeriod"] || !condition(c, s, (*event)["requirements"], "/events/" + id.get<std::string>() + "/requirements")) continue;
    auto phase = s["phase"], presentation = s["presentation"]; remove(id); enter(c, s, "event", id); s["activeNarrative"]["returnPhase"] = phase; s["activeNarrative"]["returnPresentation"] = presentation;
    if (finishingPeriod) s["activeNarrative"]["finishPeriod"] = true;
    break;
  }
}
