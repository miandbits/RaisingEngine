// Included inside raising namespace; shares the transactional expression helpers.
void facilityCosts(const J &c, J &s, const J &costs, int64_t quantity, const std::string &path, J &changes) {
  size_t i = 0;
  for (const auto &cost : costs) {
    auto key = str(cost, "attributeId"), source = path + "/" + std::to_string(i++);
    auto def = find(c["attributes"], key);
    need(def, "UNKNOWN_TARGET", "통화가 없습니다.", source + "/attributeId");
    auto amount = i32(number(cost["amount"]) * quantity, source + "/amount");
    auto before = number(s["attributes"][key]);
    need(before - amount >= number((*def)["min"]), "INSUFFICIENT_FUNDS", "비용이 부족합니다.", "/attributes/" + key);
    auto after = i32(before - amount, source);
    s["attributes"][key] = after;
    changes.push_back({{"kind","numeric"},{"source",source},{"target",{{"kind","attribute"},{"attributeId",key}}},
      {"before",before},{"operand",-amount},{"requested",after},{"after",after},{"delta",-amount},{"clamped",false}});
  }
}
void applyDiet(const J &c, J &s, J &changes) {
  if (!s.contains("gameplay") || !s["gameplay"].contains("dietId") || s["gameplay"]["dietId"].is_null()) return;
  auto &state = s["gameplay"];
  if (state.value("dietAppliedPeriod", int64_t(-1)) == number(s["periodIndex"]) ||
      state.value("dietNextPeriod", int64_t(0)) > number(s["periodIndex"])) return;
  auto diets = c["project"]["gameplay"].value("diets", J::array());
  auto diet = find(diets, state["dietId"]);
  need(diet, "UNKNOWN_DIET", "식단을 찾을 수 없습니다.", "/gameplay/dietId");
  auto path = "/project/gameplay/diets/" + str(*diet, "id");
  facilityCosts(c, s, diet->value("costs", J::array()), 1, path + "/costs", changes);
  effects(c, s, (*diet)["effects"], path + "/effects", changes);
  state["dietAppliedPeriod"] = s["periodIndex"];
  state["dietNextPeriod"] = number(s["periodIndex"]) + diet->value("intervalPeriods", 1);
}
void gameplay(const J &c, J &s, const J &cmd, J &sounds) {
  need(c["project"].contains("gameplay") && s.contains("gameplay"), "GAMEPLAY_UNAVAILABLE",
       "이 프로젝트에는 생활 규칙이 없습니다.", "/project/gameplay");
  need(s["mode"] == "full" && (s["phase"] == "planning" || s["phase"] == "ready"), "WRONG_PHASE",
       "일정 편성 중에만 장소 이동과 생활 행동을 할 수 있습니다.", "/phase");
  auto &g = c["project"]["gameplay"];
  auto &state = s["gameplay"];
  auto type = str(cmd, "type");
  std::string p = "/project/gameplay";
  if (type == "selectPolicy") {
    selectPolicy(c, s, cmd);
    return;
  }
  if (type == "executeAction") {
    auto actions = g.value("actions", J::array());
    auto action = find(actions, cmd["actionId"]);
    need(action, "UNKNOWN_ACTION", "행동을 찾을 수 없습니다.", "/command/actionId");
    J changes = J::array();
    executeDefinedAction(c, s, *action, number(cmd["quantity"]), changes, false);
    return;
  }
  if (type == "setDiet") {
    auto id = cmd["dietId"];
    need(state.value("dietId", J(nullptr)) != id, "DIET_UNCHANGED", "이미 선택된 식단입니다.", "/command/dietId");
    if (!id.is_null()) {
      auto diets = g.value("diets", J::array());
      auto diet = find(diets, id);
      need(diet, "UNKNOWN_DIET", "식단을 찾을 수 없습니다.", "/command/dietId");
      need((*diet)["locationId"] == state["locationId"], "WRONG_LOCATION", "식단을 선택할 수 있는 장소로 이동해 주세요.", "/command/dietId");
    }
    state["dietId"] = id;
    return;
  }
  if (type == "useService") {
    auto services = g.value("services", J::array());
    auto service = find(services, cmd["serviceId"]);
    need(service, "UNKNOWN_SERVICE", "서비스를 찾을 수 없습니다.", "/command/serviceId");
    auto path = p + "/services/" + str(*service, "id");
    need((*service)["locationId"] == state["locationId"], "WRONG_LOCATION", "서비스 장소로 이동해 주세요.", "/command/serviceId");
    auto qty = number(cmd["quantity"]);
    need(qty <= service->value("maxQuantity", 1), "QUANTITY_LIMIT", "서비스 수량 한도를 초과합니다.", "/command/quantity");
    need(!service->contains("requirements") || condition(c, s, (*service)["requirements"], path + "/requirements"),
         "SERVICE_LOCKED", "서비스 이용 조건을 만족하지 않습니다.", path + "/requirements");
    J changes = J::array();
    J probe = s, probeChanges = J::array();
    for (int64_t i = 0; i < qty; ++i) effects(c, probe, (*service)["effects"], path + "/effects", probeChanges);
    need(probe["attributes"] != s["attributes"] || probe["flags"] != s["flags"] || probe["relationships"] != s["relationships"],
         "SERVICE_NO_EFFECT", "현재 상태에서는 서비스 효과가 없습니다.", "/command/serviceId");
    facilityCosts(c, s, service->value("costs", J::array()), qty, path + "/costs", changes);
    for (int64_t i = 0; i < qty; ++i) effects(c, s, (*service)["effects"], path + "/effects", changes);
    return;
  }
  if (type == "navigate") {
    auto screen = cmd["screenId"];
    need(c["project"].contains("visual") && find(c["project"]["visual"]["screens"], screen),
         "UNKNOWN_SCREEN", "화면을 찾을 수 없습니다.", "/command/screenId");
    need(screen != "title" && screen != "story" && screen != "ending", "SCREEN_RESERVED",
         "시작·대본·엔딩 화면은 게임 상태에 따라 표시됩니다.", "/command/screenId");
    state["screenId"] = screen;
    return;
  }
  if (type == "visit") {
    auto location = find(g["locations"], cmd["locationId"]);
    need(location, "UNKNOWN_LOCATION", "장소를 찾을 수 없습니다.", "/command/locationId");
    auto current = find(g["locations"], state["locationId"]);
    need(current && (contains((*current)["connections"], (*location)["id"]) ||
                     (*current)["id"] == (*location)["id"]),
         "LOCATION_DISCONNECTED", "현재 장소에서 이동할 수 없습니다.", "/command/locationId");
    need(!location->contains("requirements") ||
             condition(c, s, (*location)["requirements"],
                       p + "/locations/" + str(*location, "id") + "/requirements"),
         "LOCATION_LOCKED", "장소 입장 조건을 만족하지 않습니다.", "/command/locationId");
    state["locationId"] = (*location)["id"];
    state["screenId"] = location->value("screenId", std::string("schedule"));
    if (!contains(state["visited"], (*location)["id"]))
      state["visited"].push_back((*location)["id"]);
    if (location->contains("presentation"))
      scene(s, (*location)["presentation"], sounds);
    return;
  }
  if (type == "buy" || type == "sell") {
    bool selling = type == "sell";
    auto shop = find(g["shops"], cmd["shopId"]);
    need(shop, "UNKNOWN_SHOP", "상점을 찾을 수 없습니다.", "/command/shopId");
    need((*shop)["locationId"] == state["locationId"], "WRONG_LOCATION",
         "해당 상점이 있는 장소로 이동해 주세요.", "/command/shopId");
    const J *offer = nullptr;
    auto offers = shop->value(selling ? "sellOffers" : "offers", J::array());
    for (auto &v : offers)
      if (v["itemId"] == cmd["itemId"]) {
        offer = &v;
        break;
      }
    need(offer, "UNKNOWN_OFFER", "상점에서 판매하지 않는 물품입니다.", "/command/itemId");
    auto item = find(g["items"], cmd["itemId"]);
    need(item, "UNKNOWN_ITEM", "물품 정의가 없습니다.", "/command/itemId");
    auto shopId = str(*shop, "id"), itemId = str(*item, "id");
    int64_t bought = state["purchased"].contains(shopId)
                         ? state["purchased"][shopId].value(itemId, int64_t(0))
                         : 0,
            qty = number(cmd["quantity"]), owned = state["inventory"].value(itemId, int64_t(0));
    need(qty <= offer->value("maxQuantity", selling ? int64_t(99) : int64_t(2147483647)), "QUANTITY_LIMIT", "거래 수량 한도를 초과합니다.", "/command/quantity");
    if (selling) {
      need(owned - equipmentReserved(c, s, itemId) >= qty, "INSUFFICIENT_ITEMS", "판매할 물품이 부족합니다.", "/gameplay/inventory/" + itemId);
      auto price = i32(number((*offer)["price"]) * qty, "/command/quantity");
      auto currency = find(c["attributes"], (*shop)["currencyAttributeId"]);
      need(currency, "UNKNOWN_TARGET", "통화가 없습니다.", "/command/shopId");
      auto key = str(*currency, "id");
      auto after = i32(number(s["attributes"][key]) + price, "/attributes/" + key);
      need(after <= number((*currency)["max"]), "CURRENCY_FULL", "통화 보유 한도를 초과합니다.", "/attributes/" + key);
      s["attributes"][key] = after;
      state["inventory"][itemId] = owned - qty;
      return;
    }
    need((*offer)["stock"].is_null() || bought + qty <= number((*offer)["stock"]),
         "INSUFFICIENT_STOCK", "상점 재고가 부족합니다.", "/command/quantity");
    need(owned + qty <= number((*item)["maxStack"]), "INVENTORY_FULL",
         "물품 보유 한도를 초과합니다.", "/command/quantity");
    auto price = i32(number((*offer)["price"]) * qty, "/command/quantity");
    auto currency = find(c["attributes"], (*shop)["currencyAttributeId"]);
    need(currency, "UNKNOWN_TARGET", "통화가 없습니다.", "/command/shopId");
    auto currencyId = str(*currency, "id");
    need(number(s["attributes"][currencyId]) - price >= number((*currency)["min"]),
         "INSUFFICIENT_FUNDS", "구매 비용이 부족합니다.", "/attributes/" + currencyId);
    s["attributes"][currencyId] =
        i32(number(s["attributes"][currencyId]) - price, "/attributes/" + currencyId);
    state["inventory"][itemId] = i32(owned + qty, "/gameplay/inventory/" + itemId);
    if (!state["purchased"].contains(shopId)) {
      state["purchased"][shopId] = J::object();
    }
    state["purchased"][shopId][itemId] =
        i32(bought + qty, "/gameplay/purchased/" + shopId + "/" + itemId);
    return;
  }
  auto interaction = find(g["interactions"], cmd["interactionId"]);
  need(interaction, "UNKNOWN_INTERACTION", "행동을 찾을 수 없습니다.", "/command/interactionId");
  auto id = str(*interaction, "id"), path = p + "/interactions/" + id;
  need((*interaction)["locationId"] == state["locationId"], "WRONG_LOCATION",
       "행동할 수 있는 장소로 이동해 주세요.", "/command/interactionId");
  need(!interaction->value("oncePerPeriod", false) || !state["interactionPeriods"].contains(id) ||
           state["interactionPeriods"][id] != s["periodIndex"],
       "INTERACTION_COOLDOWN", "이번 기간에 이미 한 행동입니다.", "/command/interactionId");
  need(!interaction->contains("requirements") ||
           condition(c, s, (*interaction)["requirements"], path + "/requirements"),
       "INTERACTION_LOCKED", "행동 조건을 만족하지 않습니다.", "/command/interactionId");
  if (interaction->contains("itemId"))
    need(state["inventory"].value(str(*interaction, "itemId"), int64_t(0)) - equipmentReserved(c, s, str(*interaction, "itemId")) >=
             interaction->value("quantity", 1),
         "INSUFFICIENT_ITEMS", "필요한 물품이 부족합니다.",
         "/gameplay/inventory/" + str(*interaction, "itemId"));
  for (auto &cost : interaction->value("costs", J::array())) {
    auto attribute = find(c["attributes"], cost["attributeId"]);
    need(attribute && number(s["attributes"][str(cost, "attributeId")]) - number(cost["amount"]) >=
                          number((*attribute)["min"]),
         "INSUFFICIENT_FUNDS", "행동 비용이 부족합니다.",
         "/attributes/" + str(cost, "attributeId"));
  }
  for (auto &cost : interaction->value("costs", J::array())) {
    auto key = str(cost, "attributeId");
    s["attributes"][key] =
        i32(number(s["attributes"][key]) - number(cost["amount"]), "/attributes/" + key);
  }
  if (interaction->contains("itemId")) {
    auto key = str(*interaction, "itemId");
    state["inventory"][key] = number(state["inventory"][key]) - interaction->value("quantity", 1);
  }
  J changes = J::array();
  effects(c, s, (*interaction)["effects"], path + "/effects", changes);
  state["interactionPeriods"][id] = s["periodIndex"];
  if (interaction->contains("eventId") &&
      !contains(s["completedEvents"], (*interaction)["eventId"])) {
    auto event = find(c["events"], (*interaction)["eventId"]);
    need(event, "UNKNOWN_NARRATIVE", "사건이 없습니다.", path + "/eventId");
    if (s["periodIndex"] >= (*event)["window"]["fromPeriod"] &&
        s["periodIndex"] <= (*event)["window"]["throughPeriod"] &&
        condition(c, s, (*event)["requirements"],
                  "/events/" + str(*event, "id") + "/requirements")) {
      auto phase = s["phase"], presentation = s["presentation"];
      enter(c, s, "event", (*event)["id"]);
      s["activeNarrative"]["returnPhase"] = phase;
      s["activeNarrative"]["returnPresentation"] = presentation;
    }
  }
}
