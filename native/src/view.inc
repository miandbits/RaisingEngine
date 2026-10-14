J narrativeView(const J &c, const J &s) {
  if (s.at("phase") != "narrative")
    return nullptr;
  const auto &node = currentNode(c, s);
  J choices = J::array();
  if (node.at("type") == "choice")
    for (const auto &ch : node.at("choices"))
      choices.push_back({{"id", ch.at("id")},
                         {"text", ch.at("text")},
                         {"enabled", condition(c, s, ch.at("requirements"),
                                               nodePath(s.at("activeNarrative")) + "/choices/" +
                                                   str(ch, "id") + "/requirements")}});
  return {{"cursor", s.at("activeNarrative")}, {"node", node}, {"choices", choices}};
}
J availability(const J &c, const J &s, const J &cmd) {
  try {
    J draft = s, sounds = J::array();
    gameplay(c, draft, cmd, sounds);
    return {{"enabled", true}, {"reason", nullptr}};
  } catch (const Fault &e) {
    return {{"enabled", false}, {"reason", e.what()}};
  }
}
J gameplayView(const J &c, const J &s) {
  if (s.is_null() || !c.at("project").contains("gameplay") || !s.contains("gameplay"))
    return nullptr;
  const auto &g = c.at("project").at("gameplay");
  const auto &data = s.at("gameplay");
  auto location = find(g.at("locations"), data.at("locationId"));
  need(location, "UNKNOWN_LOCATION", "현재 장소가 없습니다.", "/gameplay/locationId");
  J locations = J::array(), shops = J::array(), interactions = J::array(), inventory = J::array();
  for (const auto &x : g.at("locations"))
    if (contains(location->at("connections"), x.at("id")) || x.at("id") == location->at("id")) {
      auto item = availability(c, s, {{"type", "visit"}, {"locationId", x.at("id")}});
      item["id"] = x.at("id");
      item["name"] = x.at("name");
      if (x.contains("screenId"))
        item["screenId"] = x.at("screenId");
      locations.push_back(item);
    }
  for (const auto &shop : g.at("shops"))
    if (shop.at("locationId") == data.at("locationId")) {
      J offers = J::array();
      auto shopId = str(shop, "id");
      for (const auto &offer : shop.at("offers")) {
        auto itemId = str(offer, "itemId");
        auto definition = find(g.at("items"), itemId);
        need(definition, "UNKNOWN_ITEM", "물품 정의가 없습니다.", "/project/gameplay/items");
        int64_t bought = data.at("purchased").contains(shopId)
                             ? data.at("purchased").at(shopId).value(itemId, int64_t(0))
                             : 0;
        auto item = availability(
            c, s,
            {{"type", "buy"}, {"shopId", shop.at("id")}, {"itemId", itemId}, {"quantity", 1}});
        item["itemId"] = itemId;
        item["name"] = definition->at("name");
        item["price"] = offer.at("price");
        item["maxQuantity"] = offer.value("maxQuantity", 99);
        if (offer.contains("confirmation")) item["confirmation"] = offer["confirmation"];
        item["stock"] =
            offer.at("stock").is_null() ? J(nullptr) : J(number(offer.at("stock")) - bought);
        item["owned"] = data.at("inventory").value(itemId, int64_t(0));
        offers.push_back(item);
      }
      J sellOffers = J::array();
      for (const auto &offer : shop.value("sellOffers", J::array())) {
        auto id = str(offer, "itemId");
        auto definition = find(g.at("items"), id);
        need(definition, "UNKNOWN_ITEM", "물품 정의가 없습니다.", "/project/gameplay/items");
        auto item = availability(c, s, {{"type","sell"},{"shopId",shop["id"]},{"itemId",id},{"quantity",1}});
        item["itemId"] = id;
        item["name"] = (*definition)["name"];
        item["price"] = offer["price"];
        item["owned"] = data.at("inventory").value(id, int64_t(0));
        item["maxQuantity"] = offer.value("maxQuantity", 99);
        if (offer.contains("confirmation")) item["confirmation"] = offer["confirmation"];
        sellOffers.push_back(item);
      }
      shops.push_back({{"id", shop.at("id")},
                       {"name", shop.at("name")},
                       {"currencyAttributeId", shop.at("currencyAttributeId")},
                       {"offers", offers}, {"sellOffers",sellOffers}});
    }
  for (const auto &x : g.at("interactions"))
    if (x.at("locationId") == data.at("locationId")) {
      auto item = availability(c, s, {{"type", "interact"}, {"interactionId", x.at("id")}});
      item["id"] = x.at("id");
      item["name"] = x.at("name");
      for (auto field : {"characterId", "itemId"}) {
        if (x.contains(field))
          item[field] = x.at(field);
      }
      interactions.push_back(item);
    }
  for (const auto &item : g.at("items")) {
    auto quantity = data.at("inventory").value(str(item, "id"), int64_t(0));
    if (quantity > 0)
      inventory.push_back(
          {{"id", item.at("id")}, {"name", item.at("name")}, {"quantity", quantity}});
  }
  J diets = J::array(), services = J::array();
  for (const auto &x : g.value("diets", J::array())) if (x["locationId"] == data["locationId"]) {
    auto item = availability(c, s, {{"type","setDiet"},{"dietId",x["id"]}});
    for (auto key : {"id","name","costs","effects","confirmation"}) if (x.contains(key)) item[key] = x[key];
    item["intervalPeriods"] = x.value("intervalPeriods", 1);
    item["selected"] = data.value("dietId", J(nullptr)) == x["id"];
    diets.push_back(item);
  }
  for (const auto &x : g.value("services", J::array())) if (x["locationId"] == data["locationId"]) {
    auto item = availability(c, s, {{"type","useService"},{"serviceId",x["id"]},{"quantity",1}});
    for (auto key : {"id","name","costs","effects","confirmation"}) if (x.contains(key)) item[key] = x[key];
    item["maxQuantity"] = x.value("maxQuantity", 1);
    services.push_back(item);
  }
  J actions = J::array(), groups = J::array(), budgets = J::array();
  auto definitions = g.value("actions", J::array());
  for (const auto &action : definitions) if (genericAtLocation(action, data["locationId"])) {
    auto item = availability(c, s, {{"type","executeAction"},{"actionId",action["id"]},{"quantity",1}});
    for (auto key : {"id","name","costs","effects","confirmation"}) if (action.contains(key)) item[key] = action[key];
    item["maxQuantity"] = action.value("maxQuantity", 1);
    actions.push_back(item);
  }
  auto selections = data.value("policySelections", J::object());
  for (const auto &group : g.value("policyGroups", J::array())) if (genericAtLocation(group, data["locationId"])) {
    auto item = availability(c, s, {{"type","selectPolicy"},{"groupId",group["id"]},{"optionId",nullptr}});
    for (auto key : {"id","name","trigger"}) item[key] = group[key];
    item["options"] = J::array();
    for (const auto &option : group["options"]) {
      auto action = find(definitions, option["actionId"]);
      need(action, "UNKNOWN_ACTION", "행동을 찾을 수 없습니다.", "/command/actionId");
      auto entry = availability(c, s, {{"type","selectPolicy"},{"groupId",group["id"]},{"optionId",option["id"]}});
      for (auto key : {"id","name","actionId","budgetUses"}) if (option.contains(key)) entry[key] = option[key];
      for (auto key : {"costs","effects","confirmation"}) if (action->contains(key)) entry[key] = (*action)[key];
      entry["selected"] = selections.value(str(group, "id"), J(nullptr)) == option["id"];
      item["options"].push_back(entry);
    }
    groups.push_back(item);
  }
  for (const auto &budget : g.value("policyBudgets", J::array())) {
    auto item = budget;
    int64_t used = 0;
    for (const auto &group : g.value("policyGroups", J::array())) {
      auto option = find(group["options"], selections.value(str(group, "id"), J(nullptr)));
      if (option) for (const auto &entry : option->value("budgetUses", J::array()))
        if (entry["budgetId"] == budget["id"]) used += number(entry["amount"]);
    }
    item["used"] = used;
    budgets.push_back(item);
  }
  return {{"location", {{"id", location->at("id")}, {"name", location->at("name")}}},
          {"screenId", data.at("screenId")},
          {"locations", locations},
          {"shops", shops},
          {"interactions", interactions},
          {"inventory", inventory}, {"diets",diets}, {"services",services},
          {"actions",actions}, {"policyGroups",groups}, {"policyBudgets",budgets}};
}
