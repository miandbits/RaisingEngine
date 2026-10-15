// Optional bounded transport cache; run state and RNG are never cached implicitly.
// A native renderer can continue using self-contained requests without registration.
thread_local std::map<std::string, J> registeredContent;
const J &requestContent(const J &request) {
  if (request.contains("content"))
    return request.at("content");
  auto id = request.value("contentId", std::string());
  auto found = registeredContent.find(id);
  need(found != registeredContent.end(), "UNKNOWN_CONTENT", "등록된 콘텐츠가 없습니다.",
       "/contentId");
  return found->second;
}
std::string process(const std::string &text) {
  J request;
  try {
    request = J::parse(text);
    auto op =
        request.is_object() && request.contains("operation") && request["operation"].is_string()
            ? str(request, "operation")
            : std::string();
    if (op == "info")
      return J({{"ok", true},
                {"runtime", "raising-native"},
                {"protocolVersion", 1},
                {"coreVersion", VERSION},
                {"implementation", "C++17"}})
          .dump();
    if (op == "registerContent") {
      auto id = request.at("contentId").get<std::string>();
      need(!id.empty() && id.size() <= 128, "INVALID_CONTENT_ID",
           "콘텐츠 캐시 ID가 잘못되었습니다.", "/contentId");
      checkContent(request.at("content"));
      need(registeredContent.size() < 32 || registeredContent.count(id), "CONTENT_CACHE_FULL",
           "콘텐츠 캐시는 32개까지 등록할 수 있습니다.", "/contentId");
      registeredContent[id] = request.at("content");
      return J({{"ok", true}}).dump();
    }
    if (op == "releaseContent") {
      registeredContent.erase(request.at("contentId").get<std::string>());
      return J({{"ok", true}}).dump();
    }
    if (op == "create")
      return create(requestContent(request), request.at("seed"), request.at("options")).dump();
    if (op == "command")
      return command(requestContent(request), request.at("state"), request.at("command"),
                     request.at("expectedRevision"))
          .dump();
    if (op == "inspect") {
      J trace = J::array();
      TraceScope tracing(&trace);
      try {
        if (request.contains("subject") && request["subject"].value("kind", "") == "event") {
          const auto &c = requestContent(request), &s = request.at("state");
          checkContent(c);
          auto id = request["subject"].at("id");
          need(find(c["events"], id), "UNKNOWN_NARRATIVE", "사건이 없습니다.", "/subject/id");
          const auto event=find(c["events"],id);
          if(event->value("trigger","")=="interaction"){
            const bool completed=eventCompleted(c,s,id),inWindow=s["periodIndex"]>=(*event)["window"]["fromPeriod"]&&s["periodIndex"]<=(*event)["window"]["throughPeriod"];
            const bool matches=condition(c,s,(*event)["requirements"],"/events/"+id.get<std::string>()+"/requirements");
            const bool ready=s["mode"]=="full"&&(s["phase"]=="planning"||s["phase"]=="ready");
            const bool eligible=ready&&!completed&&inWindow&&matches;
            const auto reason=!ready?"wrong-phase":completed?"already-completed":!inWindow?"outside-window":!matches?"requirements-not-met":"interaction-ready";
            return J({{"ok",true},{"eligible",eligible},{"reason",reason},{"checks",J::array({{{"kind","invocation"},{"status","information"},{"reason","행동 또는 예약 연결에서만 발생합니다. 조건 충족은 도달 가능성 증명이 아닙니다."},{"path","/events/"+id.get<std::string>()}}})},{"trace",trace},{"eventDecisions",J::array()}}).dump();
          }
          auto decisions = inspect(c, s);
          auto decision = J::object();
          for (const auto &entry : decisions) if (entry["eventId"] == id) decision = entry;
          bool ready = s["mode"] == "full" && s["phase"] == "period-end";
          auto reason = ready ? decision["reason"] : J("wrong-phase");
          J checks = J::array({{{"path","/phase"},{"status",ready ? "passed" : "blocked"},{"reason",ready ? "period-end" : "wrong-phase"}},
                              {{"path","/events/" + id.get<std::string>()},{"status",decision["reason"] == "selected" ? "passed" : "blocked"},{"reason",decision["reason"]}}});
          return J({{"ok",true},{"state",s},{"eligible",ready && decision["reason"] == "selected"},
                    {"reason",reason},{"checks",checks},{"trace",trace},{"eventDecisions",decisions}}).dump();
        }
        auto out = command(requestContent(request), request.at("state"), request.at("command"),
                           request.at("expectedRevision"));
        out["trace"] = trace;
        return out.dump();
      } catch (const Fault &e) {
        traceExecution({{"kind","error"},{"path",e.path},{"status","blocked"},{"reason",e.what()},{"code",e.code}});
        return J({{"ok",false},{"state",request.at("state")},{"error",e.json()},{"trace",trace}}).dump();
      }
    }
    if (op == "replay") {
      const auto &c = requestContent(request);
      const auto &origin = request.at("origin");
      const auto &commands = request.at("commands");
      need(commands.is_array() && commands.size() <= 100000, "COMMAND_LIMIT",
           "명령 기록은 100000개를 넘을 수 없습니다.", "/commands");
      J state = create(c, origin.at("seed"), origin).at("state");
      size_t index = 0;
      for (const auto &cmd : commands) {
        try {
          state = command(c, state, cmd, state.at("stateRevision")).at("state");
        } catch (const Fault &e) {
          return J({{"ok", false},
                    {"error",
                     {{"code", "SAVE_REPLAY"},
                      {"message", "명령 기록을 재현할 수 없습니다: " + e.code + " — " + e.what()},
                      {"path", "/commands/" + std::to_string(index)}}}})
              .dump();
        }
        ++index;
      }
      return J({{"ok", true}, {"state", state}}).dump();
    }
    if (op == "evaluate") {
      const auto &c = requestContent(request);
      const auto &s = request.at("state");
      checkContent(c);
      if (request.contains("condition"))
        return J({{"ok", true}, {"value", condition(c, s, request["condition"])}}).dump();
      return J({{"ok", true}, {"value", evaluate(c, s, request.at("expression"))}}).dump();
    }
    if (op == "view") {
      const auto &c = requestContent(request);
      const auto &s = request.at("state");
      checkContent(c);
      return J({{"ok", true},
                {"time", s.is_null() ? J(nullptr) : calendar(c, s)},
                {"narrative", s.is_null() ? J(nullptr) : narrativeView(c, s)},
                {"gameplay", gameplayView(c, s)},
                {"systems", systemsView(c, s)}})
          .dump();
    }
    if (op == "random") {
      J rng = request.contains("rng") ? request["rng"] : seedRandom(request.at("seed"));
      J values = J::array();
      J countValue = request.value("count", J(1));
      need(integer(countValue) && number(countValue) >= 0 && number(countValue) <= 100000,
           "INVALID_COUNT", "난수 개수는 0..100000이어야 합니다.", "/count");
      uint64_t bound = 0;
      if (request.contains("bound")) {
        need(integer(request["bound"]) && number(request["bound"]) >= 1 &&
                 number(request["bound"]) <= 4294967296LL,
             "INVALID_RANDOM_BOUND", "난수 상한은 1..2^32 정수여야 합니다.", "/bound");
        bound = uint64_t(number(request["bound"]));
      }
      for (int64_t i = 0; i < number(countValue); i++)
        values.push_back(bound ? sample(rng, bound) : draw(rng));
      return J({{"ok", true}, {"rng", rng}, {"values", values}}).dump();
    }
    throw Fault("UNKNOWN_OPERATION", "지원하지 않는 런타임 요청입니다.", "/operation");
  } catch (const Fault &e) {
    J out = {{"ok", false}, {"error", e.json()}};
    if (request.is_object() && request.contains("operation") && request["operation"] == "command" &&
        request.contains("state"))
      out["state"] = request["state"];
    return out.dump();
  } catch (const std::exception &e) {
    J out = {{"ok", false},
             {"error", {{"code", "INVALID_REQUEST"}, {"message", e.what()}, {"path", "/"}}}};
    if (request.is_object() && request.contains("operation") && request["operation"] == "command" &&
        request.contains("state"))
      out["state"] = request["state"];
    return out.dump();
  }
}
