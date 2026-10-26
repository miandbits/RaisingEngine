// Forecasts run canonical transactions against private JSON copies. They expose
// numerical projections, never speculative narrative text, RNG, or a save state.
struct ForecastScope {
  bool previous = forecastNoRandom;
  ForecastScope() { forecastNoRandom = true; }
  ~ForecastScope() { forecastNoRandom = previous; }
};
J forecastAttributeDeltas(const J &before, const J &after) {
  J rows = J::array();
  for (auto it = before.begin(); it != before.end(); ++it) {
    const auto a = number(it.value()), b = number(after.at(it.key()));
    if (a != b) rows.push_back({{"attributeId",it.key()},{"before",a},{"after",b},{"delta",b-a}});
  }
  return rows;
}
J forecastProjection(const J &c, const J &original, const J &draft, bool baseline) {
  const auto first = number(original.at("periodIndex"));
  const auto count = periods(c), firstSlot = int(first % count);
  J out = {{"basis",baseline ? "activities-only" : "deterministic-prefix"},
    {"status","blocked"},{"complete",false},{"fromPeriodIndex",first},
    {"throughPeriodIndex",first+count-firstSlot-1},{"lastProjectedPeriodIndex",nullptr},
    {"rows",J::array()},{"attributes",J::array()},{"issues",J::array()},
    {"assumptions",baseline ? J::array({"EXCLUDES_NARRATIVE","EXCLUDES_INTERMISSIONS","NO_OPTIONAL_ACTIONS","CURRENT_POLICIES"}) : J::array({"NO_OPTIONAL_ACTIONS","CURRENT_POLICIES"})}};
  J state = original;
  if (baseline) state["mode"] = "activities-only";
  int64_t issuePeriod = first;
  std::string issueStage = "draft";
  bool advanced = false;
  auto stop = [&](const std::string &code, const std::string &path) {
    out["issues"].push_back({{"code",code},{"path",path},{"periodIndex",issuePeriod},{"stage",issueStage}});
  };
  auto send = [&](const J &cmd) {
    auto reply = command(c, state, cmd, state.at("stateRevision"));
    state = std::move(reply.at("state"));
    return reply.at("transition");
  };
  auto settleNarrative = [&]() {
    int steps = 0;
    while (state["phase"] == "narrative" && number(state["periodIndex"]) <= number(out["throughPeriodIndex"])) {
      const auto type = str(currentNode(c,state),"type");
      need(type == "scene" || type == "effect" || type == "branch" || type == "end",
        "FORECAST_NARRATIVE", "Story outcomes after dialogue or choices are not predicted.", "/phase");
      need(++steps <= 256, "FORECAST_NARRATIVE_LIMIT", "The automatic story prefix exceeds the forecast limit.", "/phase");
      send({{"type","advanceNarrative"}});
    }
  };
  try {
    need(!c["project"].contains("roster"), "FORECAST_ROSTER", "Roster planning is not a single-character phase draft.", "/project/roster");
    need(original["phase"] == "planning" || original["phase"] == "ready", "WRONG_PHASE", "A forecast requires an editable schedule.", "/phase");
    need(!original.value("activityIntermission",false), "FORECAST_INTERMISSION", "Finish the current intermission before forecasting the next activities.", "/activityIntermission");
    need(draft.is_array() && draft.size() == size_t(count), "FORECAST_INCOMPLETE", "Fill every draft period before forecasting.", "/activityIds");
    for (int slot = 0; slot < count; ++slot) if (!draft.at(slot).is_string() || draft.at(slot).get<std::string>().empty()) {
      issuePeriod = first + slot - firstSlot;
      need(false, "FORECAST_INCOMPLETE", "Fill every draft period before forecasting.", "/activityIds/"+std::to_string(slot));
    }
    send({{"type","setSchedule"},{"activityIds",draft}});
    for (int slot = firstSlot; slot < count; ++slot) {
      issuePeriod = first + slot - firstSlot;
      issueStage = "activity";
      const auto id = draft.at(slot);
      const auto activity = find(c["activities"],id);
      J row = {{"slot",slot},{"periodIndex",issuePeriod},{"activityId",id},
        {"beforeAttributes",state.at("attributes")},{"afterActivityAttributes",state.at("attributes")},
        {"afterAttributes",state.at("attributes")},{"activityChanges",J::array()},
        {"boundaryChanges",J::array()},{"activityComplete",false},{"boundaryComplete",false}};
      bool rowAdvanced = false;
      try {
        need(activity && !activity->contains("expeditionId"), "FORECAST_EXPEDITION", "Expedition decisions are outside a schedule forecast.", "/activities/"+id.get<std::string>()+"/expeditionId");
        need(!unifiedTime(c), "FORECAST_TIMED_ACTIVITY", "Timed sessions require additional progression decisions.", "/project/progression");
        auto transition = send({{"type","executeActivity"}});
        advanced = rowAdvanced = true;
        // Authored paid preparation is a distinct canonical transaction. Reuse
        // it exactly once, including persisted preparation from the live state.
        if (transition["activity"].is_null() && state["phase"] == "ready" && state.contains("periodPreparation"))
          transition = send({{"type","executeActivity"}});
        row["afterActivityAttributes"] = state.at("attributes");
        if (!transition["activity"].is_null()) {
          row["activityChanges"] = transition["activity"]["changes"];
          row["activityComplete"] = true;
        }
        settleNarrative();
        need(state["phase"] == "activity-result", "FORECAST_NARRATIVE", "Further progression requires a decision.", "/phase");
        issueStage = "boundary";
        send({{"type","acknowledgeResult"}});
        settleNarrative();
        need(!state.value("activityIntermission",false), "FORECAST_INTERMISSION", "Intermission decisions are not predictable.", "/activityIntermission");
        need(state["phase"] == "period-end", "FORECAST_NARRATIVE", "Story outcomes are not predictable.", "/phase");
        send({{"type",baseline || state["mode"] == "activities-only" ? "completePeriodWithoutEvents" : "resolvePeriodEnd"}});
        settleNarrative();
        // Even the last slot executes its period/month boundary, so shared
        // upkeep and other authored rules are included in a complete result.
        need(state["phase"] != "narrative" || number(state["periodIndex"]) > number(out["throughPeriodIndex"]), "FORECAST_NARRATIVE", "Story outcomes are not predictable.", "/phase");
        row["boundaryComplete"] = true;
      } catch (const Fault &e) {
        if (rowAdvanced) {
          if (!row["activityComplete"].get<bool>()) row["afterActivityAttributes"] = state.at("attributes");
          row["afterAttributes"] = state.at("attributes");
          row["boundaryChanges"] = forecastAttributeDeltas(row["afterActivityAttributes"],state.at("attributes"));
          out["rows"].push_back(row);
          out["lastProjectedPeriodIndex"] = issuePeriod;
        }
        throw;
      }
      row["afterAttributes"] = state.at("attributes");
      row["boundaryChanges"] = forecastAttributeDeltas(row["afterActivityAttributes"],state.at("attributes"));
      out["rows"].push_back(row);
      out["lastProjectedPeriodIndex"] = issuePeriod;
    }
    out["complete"] = true;
    out["status"] = baseline ? "projected" : "exact";
  } catch (const Fault &e) {
    stop(e.code,e.path.find("/events/") == 0 || e.path.find("/endings/") == 0 ? "/phase" : e.path);
    out["status"] = advanced ? "partial" : "blocked";
  }
  for (const auto &def : c["attributes"]) {
    const auto id = str(def,"id");
    const auto before = number(original.at("attributes").at(id)), after = number(state.at("attributes").at(id));
    int64_t costs = 0, effects = 0;
    for (const auto &row : out["rows"]) for (const auto &change : row["activityChanges"]) {
      if (change.value("kind","") != "numeric" || change["target"].value("kind","") != "attribute" || change["target"].value("attributeId","") != id) continue;
      const auto source = str(change,"source"), prefix = "/activities/"+row["activityId"].get<std::string>();
      if (source.find(prefix+"/costs/") == 0) costs += number(change["delta"]);
      else if (source.find(prefix+"/baseEffects/") == 0) effects += number(change["delta"]);
    }
    out["attributes"].push_back({{"attributeId",id},{"before",before},{"after",after},
      {"delta",after-before},{"costs",costs},{"effects",effects},{"automatic",after-before-costs-effects}});
  }
  return out;
}
J phaseForecast(const J &c, const J &state, const J &draft) {
  checkContent(c);
  need(state.is_object() && state.value("coreVersion","") == VERSION && state.contains("attributes"), "INCOMPATIBLE_STATE", "A compatible run state is required.", "/state");
  need(state.at("contentFingerprint") == c["contentFingerprint"], "CONTENT_MISMATCH", "The state belongs to different content.", "/contentFingerprint");
  need(state.value("mode","") == "full" || state.value("mode","") == "activities-only", "INCOMPATIBLE_STATE", "A supported run mode is required.", "/mode");
  calendar(c,state);
  ForecastScope predicting;
  TraceScope quiet(nullptr);
  return {{"ok",true},{"forecast",forecastProjection(c,state,draft,false)},
    {"baseline",forecastProjection(c,state,draft,true)}};
}
