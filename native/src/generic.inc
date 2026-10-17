// Data-defined actions and independent persistent policy groups. Legacy facilities
// remain separate so existing replay logs and save states retain their semantics.
bool genericAtLocation(const J &definition, const J &locationId) {
  return !definition.contains("locationIds") || definition["locationIds"].empty() ||
         contains(definition["locationIds"], locationId);
}
struct RuleContext {
  std::vector<std::string> stack;
  std::map<std::string, std::vector<std::string>> fired;
  std::map<std::string, int> counts;
  std::string occurrence = "command";
  J relationships = nullptr;
  int total = 0;
};
thread_local RuleContext *activeRules = nullptr;
struct RuleScope {
  RuleContext local;
  bool owns;
  RuleScope() : owns(!activeRules) { if (owns) activeRules = &local; }
  ~RuleScope() { if (owns) activeRules = nullptr; }
};
struct ActionFrame {
  bool active = true;
  explicit ActionFrame(const std::string &id) {
    need(std::find(activeRules->stack.begin(), activeRules->stack.end(), id) == activeRules->stack.end(),
         "FLOW_CYCLE", "행동 흐름이 순환합니다.", "/project/gameplay/actions/" + id + "/flow");
    need(activeRules->stack.size() < 32 && ++activeRules->counts[activeRules->occurrence] <= 128 && ++activeRules->total <= 4096,
         "FLOW_LIMIT", "행동 흐름 실행 한도를 초과합니다.", "/project/gameplay/actions/" + id + "/flow");
    activeRules->stack.push_back(id);
  }
  void pop() { if (active) { activeRules->stack.pop_back(); active = false; } }
  ~ActionFrame() { pop(); }
};
void runRules(const J &c, J &s, const std::string &type, const J &context, J &changes, const std::string &occurrence = "");
void relationshipRules(const J &c, J &s, const J &before, J &changes);
void executeDefinedAction(const J &c, J &s, const J &action, int64_t quantity,
                          J &changes, bool scheduled) {
  RuleScope scope;
  if (activeRules->relationships.is_null()) activeRules->relationships = s["relationships"];
  auto path = "/project/gameplay/actions/" + str(action, "id");
  traceExecution({{"kind","action"},{"id",action["id"]},{"path",path},{"label",action["name"]},{"status","passed"}});
  need(scheduled || genericAtLocation(action, s["gameplay"]["locationId"]),
       "WRONG_LOCATION", "행동할 수 있는 장소로 이동해 주세요.", "/command/actionId");
  need(quantity <= action.value("maxQuantity", 1), "QUANTITY_LIMIT",
       "설정된 수량 한도를 초과합니다.", "/command/quantity");
  need(!scheduled || !action.contains("eventId"), "ACTION_TRIGGER_EVENT",
       "사건 행동은 일정 트리거에서 실행할 수 없습니다.", "/command/actionId");
  ActionFrame frame(str(action, "id"));
  need(!action.contains("expeditionId") || (!scheduled && !action.contains("eventId") && quantity == 1),
       "ACTION_EXPEDITION_CONTEXT", "탐험은 사건 연결 없이 직접 한 번 실행해야 합니다.", "/command/actionId");
  auto changeStart = changes.size();
  need(!action.contains("requirements") || condition(c, s, action["requirements"], path + "/requirements"),
       "ACTION_LOCKED", "행동 조건을 만족하지 않습니다.", "/command/actionId");
  if (action.value("rejectNoEffect", false) && action.value("flow", J::array()).empty()) {
    J probe = s, probeChanges = J::array();
    TraceScope suppressProbe(nullptr);
    for (int64_t i = 0; i < quantity; ++i)
      effects(c, probe, action["effects"], path + "/effects", probeChanges);
    need(probe["attributes"] != s["attributes"] || probe["flags"] != s["flags"] ||
             probe["relationships"] != s["relationships"] || probe["gameplay"]["inventory"] != s["gameplay"]["inventory"] || probe.value("roster",J()) != s.value("roster",J()) || probe.value("customStates",J()) != s.value("customStates",J()),
         "ACTION_NO_EFFECT", "현재 상태에서는 적용할 효과가 없습니다.", "/command/actionId");
  }
  size_t costIndex = 0;
  for (const auto &cost : action["costs"]) {
    auto key = str(cost, "attributeId"), source = path + "/costs/" + std::to_string(costIndex++);
    auto definition = find(c["attributes"], key);
    need(definition, "UNKNOWN_TARGET", "통화가 없습니다.", source + "/attributeId");
    auto amount = i32(number(cost["amount"]) * quantity, "/command/quantity");
    auto before = number(s["attributes"][key]), after = i32(before - amount, "/attributes/" + key);
    need(after >= number((*definition)["min"]), "INSUFFICIENT_FUNDS", "비용이 부족합니다.", "/attributes/" + key);
    s["attributes"][key] = after;
    changes.push_back({{"kind","numeric"},{"source",source},{"target",{{"kind","attribute"},{"attributeId",key}}},
      {"before",before},{"operand",-amount},{"requested",after},{"after",after},{"delta",-amount},{"clamped",false}});
    traceExecution({{"kind","cost"},{"path",source},{"status","applied"},{"before",before},{"after",after},{"amount",amount}});
  }
  for (int64_t i = 0; i < quantity; ++i)
    effects(c, s, action["effects"], path + "/effects", changes);
  for (const auto &step : action.value("flow", J::array())) {
    auto stepPath = path + "/flow/" + str(step, "id");
    bool branch = !step.contains("condition") || condition(c, s, step["condition"], stepPath + "/condition");
    if (branch && step.contains("chance")) {
      auto requested = evaluate(c, s, step["chance"], stepPath + "/chance");
      auto chance = std::min(int64_t(10000), std::max(int64_t(0), requested));
      auto roll = sample(s["rng"], 10000);
      branch = roll < uint64_t(chance);
      traceExecution({{"kind","flow"},{"id",step["id"]},{"path",stepPath+"/chance"},{"status",branch?"passed":"failed"},{"requestedChance",requested},{"chance",chance},{"roll",roll}});
    }
    auto nextId = branch ? step["thenActionId"] : step.value("elseActionId", J(nullptr));
    traceExecution({{"kind","flow"},{"id",step["id"]},{"path",stepPath},{"status",nextId.is_null() ? "skipped" : "passed"},{"after",nextId}});
    if (nextId.is_null()) continue;
    auto actions = c["project"]["gameplay"].value("actions", J::array());
    auto child = find(actions, nextId);
    need(child, "UNKNOWN_ACTION", "행동을 찾을 수 없습니다.", "/command/actionId");
    executeDefinedAction(c, s, *child, step.value("quantity", 1), changes, true);
  }
  if (action.value("rejectNoEffect", false) && !action.value("flow", J::array()).empty()) {
    bool changed = false;
    for (size_t i = changeStart; i < changes.size(); ++i)
      if (str(changes[i], "source").find("/costs/") == std::string::npos && changes[i]["before"] != changes[i]["after"]) changed = true;
    need(changed, "ACTION_NO_EFFECT", "현재 상태에서는 적용할 효과가 없습니다.", "/command/actionId");
  }
  frame.pop();
  relationshipRules(c, s, s["relationships"], changes);
  if (!scheduled && action.contains("eventId")) {
    auto event = find(c["events"], action["eventId"]);
    need(event, "UNKNOWN_NARRATIVE", "사건이 없습니다.", path + "/eventId");
    if ((event->value("repeatable", false) || !eventCompleted(c,s,(*event)["id"])) &&
        s["periodIndex"] >= (*event)["window"]["fromPeriod"] &&
        s["periodIndex"] <= (*event)["window"]["throughPeriod"] &&
        condition(c, s, (*event)["requirements"], "/events/" + str(*event, "id") + "/requirements")) {
      auto phase = s["phase"], presentation = s["presentation"];
      enter(c, s, "event", (*event)["id"]);
      s["activeNarrative"]["returnPhase"] = phase;
      s["activeNarrative"]["returnPresentation"] = presentation;
    }
  }
}
void runRules(const J &c, J &s, const std::string &type, const J &context, J &changes, const std::string &occurrence) {
  if (!c["project"].contains("gameplay") || !s.contains("gameplay")) return;
  RuleScope scope;
  auto previous = activeRules->occurrence;
  auto key = occurrence.empty() ? previous : type + ":" + occurrence;
  activeRules->occurrence = key;
  struct Restore { std::string value; ~Restore() { activeRules->occurrence = value; } } restore{previous};
  for (const auto &rule : c["project"]["gameplay"].value("rules", J::array())) {
    const auto &trigger = rule["trigger"];
    if (trigger["type"] != type) continue;
    bool matches = true;
    for (auto field : {"locationId","shopId","itemId","characterId","relationshipId"})
      if (trigger.contains(field) && (!context.contains(field) || trigger[field] != context[field])) matches = false;
    if (!matches) continue;
    auto id = str(rule, "id"), path = "/project/gameplay/rules/" + id;
    auto &fired = activeRules->fired[key];
    if (std::find(fired.begin(), fired.end(), id) != fired.end()) {
      traceExecution({{"kind","rule"},{"id",id},{"path",path},{"status","skipped"},{"reason","already-fired"}});
      continue;
    }
    fired.push_back(id);
    if (rule.contains("condition") && !condition(c, s, rule["condition"], path + "/condition")) {
      traceExecution({{"kind","rule"},{"id",id},{"path",path},{"status","skipped"},{"reason","condition"}});
      continue;
    }
    traceExecution({{"kind","rule"},{"id",id},{"path",path},{"label",rule["name"]},{"status","passed"},{"trigger",type}});
    auto actions = c["project"]["gameplay"].value("actions", J::array());
    auto action = find(actions, rule["actionId"]);
    need(action, "UNKNOWN_ACTION", "행동을 찾을 수 없습니다.", "/command/actionId");
    executeDefinedAction(c, s, *action, rule.value("quantity", 1), changes, true);
  }
}
void relationshipRules(const J &c, J &s, const J &before, J &changes) {
  RuleScope scope;
  auto baseline = activeRules->relationships.is_null() ? before : activeRules->relationships;
  activeRules->relationships = s["relationships"];
  for (const auto &character : c["characters"])
    for (const auto &relation : character["relationships"]) {
      auto characterId = str(character, "id"), relationshipId = str(relation, "id");
      if (s["relationships"][characterId][relationshipId] != baseline[characterId][relationshipId])
        runRules(c, s, "relationshipChanged", {{"characterId",characterId},{"relationshipId",relationshipId}}, changes);
    }
}
void boundaryRules(const J &c, J &s, J &changes, bool start) {
  if (!s.contains("gameplay") || c["project"]["gameplay"].value("rules", J::array()).empty()) return;
  auto key = start ? "periodStart" : "periodEnd";
  auto period = number(s["periodIndex"]);
  if (s["gameplay"].value("rulePeriods", J::object()).value(key, int64_t(-1)) == period) return;
  if (!s["gameplay"].contains("rulePeriods")) s["gameplay"]["rulePeriods"] = J::object();
  s["gameplay"]["rulePeriods"][key] = period;
  if (start && period % periods(c) == 0) runRules(c, s, "monthStart", J::object(), changes);
  runRules(c, s, key, J::object(), changes);
  if (!start && period % periods(c) == periods(c)-1) runRules(c, s, "monthEnd", J::object(), changes);
}
void dayRules(const J &c, J &s, J &changes) {
  if (!c["project"].contains("gameplay") || !c["project"]["gameplay"].contains("ruleCalendar")) return;
  int days = c["project"]["gameplay"]["ruleCalendar"]["daysPerMonth"];
  int period = number(s["periodIndex"]), slot = period % periods(c);
  for (int day = (periods(c)==3?slot*days/3:slot * (days / periods(c))); day < (periods(c)==3?(slot+1)*days/3:(slot == periods(c)-1 ? days : (slot + 1) * (days / periods(c)))); ++day) {
    auto index = std::to_string(period / periods(c) * days + day);
    runRules(c, s, "dayStart", J::object(), changes, index);
    runRules(c, s, "dayEnd", J::object(), changes, index);
  }
}
void policyBudgets(const J &c, const J &selections) {
  const auto &g = c["project"]["gameplay"];
  auto groups = g.value("policyGroups", J::array());
  for (const auto &budget : g.value("policyBudgets", J::array())) {
    int64_t used = 0;
    for (const auto &group : groups) {
      auto selected = selections.value(str(group, "id"), J(nullptr));
      if (selected.is_null()) continue;
      auto option = find(group["options"], selected);
      need(option, "UNKNOWN_POLICY_OPTION", "정책 선택지를 찾을 수 없습니다.", "/command/optionId");
      for (const auto &use : option->value("budgetUses", J::array()))
        if (use["budgetId"] == budget["id"]) used += number(use["amount"]);
    }
    need(used <= number(budget["capacity"]), "POLICY_BUDGET_EXCEEDED",
         "정책 자원 한도를 초과합니다.", "/command/optionId");
  }
}
void selectPolicy(const J &c, J &s, const J &cmd) {
  const auto &g = c["project"]["gameplay"];
  auto groups = g.value("policyGroups", J::array());
  auto group = find(groups, cmd["groupId"]);
  need(group, "UNKNOWN_POLICY_GROUP", "정책 그룹을 찾을 수 없습니다.", "/command/groupId");
  need(genericAtLocation(*group, s["gameplay"]["locationId"]),
       "WRONG_LOCATION", "정책을 설정할 수 있는 장소로 이동해 주세요.", "/command/groupId");
  auto selections = s["gameplay"].value("policySelections", J::object());
  need(selections.value(str(*group, "id"), J(nullptr)) != cmd["optionId"],
       "POLICY_UNCHANGED", "이미 선택한 정책입니다.", "/command/optionId");
  if (!cmd["optionId"].is_null())
    need(find((*group)["options"], cmd["optionId"]), "UNKNOWN_POLICY_OPTION",
         "정책 선택지를 찾을 수 없습니다.", "/command/optionId");
  selections[str(*group, "id")] = cmd["optionId"];
  policyBudgets(c, selections);
  s["gameplay"]["policySelections"] = selections;
}
void applyPolicies(const J &c, J &s, J &changes, const std::string &trigger) {
  if (!s.contains("gameplay") || !s["gameplay"].contains("policySelections")) return;
  const auto &g = c["project"]["gameplay"];
  auto &data = s["gameplay"];
  auto period = number(s["periodIndex"]);
  for (const auto &group : g.value("policyGroups", J::array())) {
    auto id = str(group, "id");
    auto selected = data["policySelections"].value(id, J(nullptr));
    if (selected.is_null() || group["trigger"]["type"] != trigger) continue;
    if (data.value("policyAppliedPeriods", J::object()).value(id, int64_t(-1)) == period ||
        data.value("policyNextPeriods", J::object()).value(id, int64_t(0)) > period) continue;
    auto option = find(group["options"], selected);
    need(option, "UNKNOWN_POLICY_OPTION", "정책 선택지를 찾을 수 없습니다.", "/gameplay/policySelections/" + id);
    auto actions = g.value("actions", J::array());
    auto action = find(actions, (*option)["actionId"]);
    need(action, "UNKNOWN_ACTION", "행동을 찾을 수 없습니다.", "/project/gameplay/policyGroups/" + id);
    executeDefinedAction(c, s, *action, 1, changes, true);
    traceExecution({{"kind","policy"},{"id",id},{"path","/project/gameplay/policyGroups/" + id},
                    {"label",group["name"]},{"status","applied"}});
    if (!data.contains("policyAppliedPeriods")) data["policyAppliedPeriods"] = J::object();
    if (!data.contains("policyNextPeriods")) data["policyNextPeriods"] = J::object();
    data["policyAppliedPeriods"][id] = period;
    data["policyNextPeriods"][id] = period + group["trigger"].value("intervalPeriods", 1);
  }
}
