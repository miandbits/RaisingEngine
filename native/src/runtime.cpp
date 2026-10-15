#include "raising/runtime.hpp"
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <regex>
#include <stdexcept>
#include <vector>

namespace raising {
using J = nlohmann::json;
constexpr int64_t SAFE = 9007199254740991LL;
const std::string VERSION = "0.2.0";
thread_local J *executionTrace = nullptr;
void traceExecution(J row) {
  if (executionTrace && executionTrace->size() < 10000) executionTrace->push_back(std::move(row));
}
struct TraceScope {
  J *previous;
  explicit TraceScope(J *trace) : previous(executionTrace) { executionTrace = trace; }
  ~TraceScope() { executionTrace = previous; }
};
struct Fault : std::runtime_error {
  std::string code, path;
  Fault(std::string c, std::string m, std::string p)
      : std::runtime_error(m), code(std::move(c)), path(std::move(p)) {}
  J json() const { return {{"code", code}, {"message", what()}, {"path", path}}; }
};
void need(bool condition, const std::string &code, const std::string &message,
          const std::string &path) {
  if (!condition)
    throw Fault(code, message, path);
}
bool integer(const J &j) {
  if (!j.is_number())
    return false;
  double v = j.get<double>();
  return std::isfinite(v) && std::floor(v) == v && v >= -double(SAFE) && v <= double(SAFE);
}
int64_t number(const J &j) { return j.get<int64_t>(); }
int64_t i32(int64_t v, const std::string &p) {
  need(v >= -2147483648LL && v <= 2147483647LL, "INTEGER_OVERFLOW",
       "계산 결과가 int32 범위를 벗어났습니다.", p);
  return v;
}
std::string str(const J &j, const char *key) { return j.at(key).get<std::string>(); }
bool contains(const J &arr, const J &value) {
  return std::find(arr.begin(), arr.end(), value) != arr.end();
}
const J *find(const J &arr, const J &id) {
  for (auto &v : arr)
    if (v.at("id") == id)
      return &v;
  return nullptr;
}
void checkContent(const J &c) {
  bool valid = c.is_object() && c.contains("snapshotVersion") && c["snapshotVersion"] == "1" &&
               c.contains("contentFingerprint") && c["contentFingerprint"].is_string();
  if (valid) {
    const auto &fingerprint = c["contentFingerprint"].get_ref<const std::string &>();
    valid = fingerprint.size() == 71 && fingerprint.compare(0, 7, "sha256:") == 0 &&
            std::all_of(fingerprint.begin() + 7, fingerprint.end(),
                        [](char v) { return (v >= '0' && v <= '9') || (v >= 'a' && v <= 'f'); });
  }
  need(valid, "INCOMPATIBLE_CONTENT", "지원하는 buildSnapshot 결과가 필요합니다.", "/content");
}
int periods(const J &c) {
  const auto value=c.at("project").value("periodsPerMonth",J(3));
  need(integer(value)&&number(value)>=1&&number(value)<=12,"INCOMPATIBLE_CONTENT","월별 일정 칸 수는 1..12 정수여야 합니다.","/project/periodsPerMonth");
  return int(number(value));
}
J calendar(const J &c, const J &s) {
  auto p = s.at("periodIndex");
  need(integer(p) && number(p) >= 0 && number(p) < number(c.at("project").at("durationMonths")) * periods(c),
       "INVALID_TIME", "기간 인덱스가 프로젝트 범위를 벗어났습니다.", "/periodIndex");
  int64_t n = number(p), month = number(c.at("project").at("startMonth")) - 1 + n / periods(c);
  return {{"periodIndex", n},
          {"year", number(c.at("project").at("startYear")) + month / 12},
          {"month", month % 12 + 1},
          {"periodInMonth", n % periods(c)}};
}
#include "character-state.inc"
struct Slot {
  const J *definition;
  int64_t value;
  std::string key;
};
int equipmentReserved(const J &c, const J &s, const std::string &itemId);
Slot slot(const J &c, const J &s, const J &t, const std::string &p) {
  const J *def = nullptr;
  const J *vals = nullptr;
  std::string key;
  const auto owner=ownerId(c,s,t.value("owner",J()),p+"/owner");
  if (t.at("kind") == "attribute") {
    key = str(t, "attributeId");
    def = find(c.at("attributes"), key);
    vals = &s.at("attributes");
    if(!owner.empty()){
      need(contains(c["project"]["roster"]["localAttributeIds"],key),"SHARED_ATTRIBUTE","공용 능력은 캐릭터 소유 대상으로 지정할 수 없습니다.",p);
      if(owner!=s.at("roster").value("activityActorId",str(c["project"],"protagonistId")))vals=&s.at("roster").at("attributes").at(owner);
    }
  } else if (t.at("kind") == "inventory") {
    key = str(t, "itemId");
    if (c.at("project").contains("gameplay")) def = find(c.at("project").at("gameplay").at("items"), key);
    if (s.contains("gameplay")) vals = &s.at("gameplay").at("inventory");
    if(!owner.empty())vals=&s.at("roster").at("inventory").at(owner);
  } else {
    auto ch = find(c.at("characters"), t.at("characterId"));
    if (ch)
      def = find(ch->at("relationships"), t.at("relationshipId"));
    key = str(t, "relationshipId");
    auto it = s.at("relationships").find(str(t, "characterId"));
    if (it != s.at("relationships").end())
      vals = &*it;
    if(!owner.empty())vals=&s.at("roster").at("relationships").at(owner).at(str(t,"characterId"));
  }
  need(def && vals && vals->contains(key), "UNKNOWN_TARGET", "수치 대상이 없습니다.", p);
  return {def, number(vals->at(key)), key};
}
int64_t evaluate(const J &c, const J &s, const J &e, const std::string &p = "/expression",
                 int d = 0) {
  need(d < 32, "EXPRESSION_DEPTH", "수식 중첩은 32단계를 넘을 수 없습니다.", p);
  auto next = [&](const char *f) { return evaluate(c, s, e.at(f), p + "/" + f, d + 1); };
  const auto op = str(e, "op");
  int64_t v = 0;
  if (op == "literal") {
    need(integer(e.at("value")), "INTEGER_OVERFLOW", "정수 수식이 필요합니다.", p);
    v = number(e.at("value"));
  } else if (op == "read")
    v = slot(c, s, e.at("target"), p + "/target").value;
  else if (op == "time")
    v = number(calendar(c, s).at(str(e, "field")));
  else if (op == "add") {
    auto a = next("left");
    auto b = next("right");
    v = a + b;
  } else if (op == "subtract") {
    auto a = next("left");
    auto b = next("right");
    v = a - b;
  } else if (op == "multiply") {
    auto a = next("left");
    auto b = next("right");
    v = a * b;
  } else if (op == "clamp")
    v = std::min(number(e.at("max")), std::max(number(e.at("min")), next("value")));
  else
    throw Fault("INVALID_EXPRESSION", "지원하지 않는 수식입니다.", p);
  return i32(v, p);
}
bool condition(const J &c, const J &s, const J &e, const std::string &p = "/condition", int d = 0);
bool conditionValue(const J &c, const J &s, const J &e, const std::string &p, int d) {
  need(d < 32, "CONDITION_DEPTH", "조건 중첩은 32단계를 넘을 수 없습니다.", p);
  const auto op = str(e, "op");
  if (op == "always")
    return true;
  if (op == "all" || op == "any") {
    size_t i = 0;
    for (auto &v : e.at("conditions")) {
      bool b = condition(c, s, v, p + "/conditions/" + std::to_string(i++), d + 1);
      if (op == "all" && !b)
        return false;
      if (op == "any" && b)
        return true;
    }
    return op == "all";
  }
  if (op == "not")
    return !condition(c, s, e.at("condition"), p + "/condition", d + 1);
  if (op == "flag") {
    need(s.at("flags").contains(str(e, "flagId")), "UNKNOWN_FLAG", "플래그가 없습니다.",
         p + "/flagId");
    return s.at("flags").at(str(e, "flagId")) == e.at("equals");
  }
  if (op == "eventCompleted")
    return eventCompleted(c,s,e.at("eventId"),e.value("owner",J()));
  if (op == "compare") {
    auto a = evaluate(c, s, e.at("left"), p + "/left", d + 1);
    auto b = evaluate(c, s, e.at("right"), p + "/right", d + 1);
    auto cmp = str(e, "comparator");
    if (cmp == "eq")
      return a == b;
    if (cmp == "ne")
      return a != b;
    if (cmp == "gt")
      return a > b;
    if (cmp == "gte")
      return a >= b;
    if (cmp == "lt")
      return a < b;
    if (cmp == "lte")
      return a <= b;
  }
  throw Fault("INVALID_CONDITION", "지원하지 않는 조건입니다.", p);
}
bool condition(const J &c, const J &s, const J &e, const std::string &p, int d) {
  auto result = conditionValue(c, s, e, p, d);
  if (executionTrace) {
    J row = {{"kind","condition"},{"path",p},{"op",e.at("op")},
             {"status",result ? "passed" : "blocked"},{"after",result}};
    if (e.at("op") == "compare") {
      row["leftValue"] = evaluate(c, s, e.at("left"), p + "/left", d + 1);
      row["rightValue"] = evaluate(c, s, e.at("right"), p + "/right", d + 1);
      row["comparator"] = e.at("comparator");
    }
    traceExecution(row);
  }
  return result;
}
void effects(const J &c, J &s, const J &list, const std::string &src, J &changes) {
  size_t i = 0;
  for (auto &e : list) {
    auto p = src + "/" + std::to_string(i++);
    if (e.at("op") == "setFlag") {
      auto id = str(e, "flagId");
      need(s.at("flags").contains(id), "UNKNOWN_FLAG", "플래그가 없습니다.", p + "/flagId");
      changes.push_back({{"kind", "flag"},
                         {"source", p},
                         {"flagId", id},
                         {"before", s["flags"][id]},
                         {"after", e.at("value")}});
      s["flags"][id] = e.at("value");
      traceExecution({{"kind","effect"},{"path",p},{"status","applied"},
                      {"before",changes.back()["before"]},{"after",e.at("value")}});
      continue;
    }
    auto x = slot(c, s, e.at("target"), p + "/target");
    auto operand = evaluate(c, s, e.at("value"), p + "/value", 1);
    auto requested = i32(e.at("op") == "add" ? x.value + operand : operand, p);
    bool inventory = e.at("target").at("kind") == "inventory";
    auto low = inventory ? int64_t(0) : number(x.definition->at("min"));
    auto high = inventory ? number(x.definition->at("maxStack")) : number(x.definition->at("max"));
    if (inventory) {
      need(requested >= 0 && requested <= high, "INVENTORY_RANGE", "물품 수량이 보유 가능 범위를 벗어났습니다.", p);
      need(requested >= (e.at("target").contains("owner")?0:equipmentReserved(c, s, x.key)), "EQUIPPED_ITEM", "장착 중인 물품은 소비할 수 없습니다.", p);
    }
    auto after = std::min(high, std::max(low, requested));
    const auto owner=ownerId(c,s,e.at("target").value("owner",J()),p+"/target/owner");
    if(!owner.empty() && inventory)s["roster"]["inventory"][owner][x.key]=after;
    else if(!owner.empty() && e.at("target").at("kind")=="relationship")s["roster"]["relationships"][owner][str(e.at("target"),"characterId")][x.key]=after;
    else if(!owner.empty() && owner!=s["roster"].value("activityActorId",str(c["project"],"protagonistId")))s["roster"]["attributes"][owner][x.key]=after;
    else if (e.at("target").at("kind") == "attribute")
      s["attributes"][x.key] = after;
    else if (inventory)
      s["gameplay"]["inventory"][x.key] = after;
    else
      s["relationships"][str(e.at("target"), "characterId")][x.key] = after;
    J clamped = false;
    if (requested < low)
      clamped = "min";
    else if (requested > high)
      clamped = "max";
    changes.push_back({{"kind", "numeric"},
                       {"source", p},
                       {"target", e.at("target")},
                       {"before", x.value},
                       {"operand", operand},
                       {"requested", requested},
                       {"after", after},
                       {"delta", after - x.value},
                       {"clamped", clamped}});
    traceExecution({{"kind","effect"},{"path",p},{"status","applied"},
                    {"before",x.value},{"after",after},{"operand",operand},{"requested",requested},{"clamped",clamped}});
  }
}
uint64_t advance(uint64_t state) { return state * 6364136223846793005ULL + 109ULL; }
J seedRandom(const J &seed) {
  need(integer(seed) && number(seed) >= 0 && number(seed) <= 4294967295LL, "INVALID_SEED",
       "seed는 uint32 정수여야 합니다.", "/seed");
  return {{"algorithm", "pcg32-xsh-rr-v1"},
          {"seed", seed},
          {"state", std::to_string(advance(advance(0) + number(seed)))},
          {"draws", 0}};
}
uint32_t draw(J &r) {
  bool valid = r.value("algorithm", "") == "pcg32-xsh-rr-v1" && r.contains("state") &&
               r["state"].is_string() &&
               std::regex_match(str(r, "state"), std::regex("(0|[1-9][0-9]{0,19})")) &&
               integer(r.at("draws")) && number(r.at("draws")) >= 0 && number(r.at("draws")) < SAFE;
  uint64_t old = 0;
  try {
    if (valid)
      old = std::stoull(str(r, "state"));
  } catch (...) {
    valid = false;
  }
  need(valid, "INVALID_RANDOM_STATE", "지원하지 않거나 손상된 난수 상태입니다.", "/rng");
  uint32_t x = uint32_t(((old >> 18) ^ old) >> 27), rot = uint32_t(old >> 59);
  uint32_t value = (x >> rot) | (x << ((-rot) & 31));
  r["state"] = std::to_string(advance(old));
  r["draws"] = number(r.at("draws")) + 1;
  return value;
}
uint64_t sample(J &r, uint64_t bound) {
  need(bound >= 1 && bound <= 4294967296ULL, "INVALID_RANDOM_BOUND",
       "난수 상한은 1..2^32 정수여야 합니다.", "/bound");
  auto limit = (4294967296ULL / bound) * bound;
  uint64_t value;
  do {
    value = draw(r);
  } while (value >= limit);
  return value % bound;
}
void merge(const J &c, J &s, const J &o, const std::string &p) {
  need(o.is_object() && o.size() == 3 && o.contains("attributes") && o["attributes"].is_object() &&
           o.contains("flags") && o["flags"].is_object() && o.contains("relationships") &&
           o["relationships"].is_object(),
       "INVALID_OVERRIDES", "attributes·flags·relationships 객체가 필요합니다.", p);
  for (auto it = o["attributes"].begin(); it != o["attributes"].end(); ++it) {
    auto def = find(c["attributes"], it.key());
    auto path = p + "/attributes/" + it.key();
    need(def, "UNKNOWN_TARGET", "알 수 없는 능력치: " + it.key(), path);
    need(integer(it.value()) && number(it.value()) >= number((*def)["min"]) &&
             number(it.value()) <= number((*def)["max"]),
         "INITIAL_RANGE", "초기 값이 범위를 벗어났습니다.", path);
    s["attributes"][it.key()] = it.value();
  }
  for (auto it = o["flags"].begin(); it != o["flags"].end(); ++it) {
    auto path = p + "/flags/" + it.key();
    need(s["flags"].contains(it.key()), "UNKNOWN_FLAG", "알 수 없는 플래그: " + it.key(), path);
    need(it.value().is_boolean(), "INITIAL_TYPE", "플래그 초기 값은 boolean이어야 합니다.", path);
    s["flags"][it.key()] = it.value();
  }
  for (auto ch = o["relationships"].begin(); ch != o["relationships"].end(); ++ch) {
    auto def = find(c["characters"], ch.key());
    auto path = p + "/relationships/" + ch.key();
    need(def, "UNKNOWN_TARGET", "알 수 없는 인물: " + ch.key(), path);
    need(ch.value().is_object(), "INVALID_OVERRIDES", "관계 초기 값은 객체여야 합니다.", path);
    for (auto it = ch.value().begin(); it != ch.value().end(); ++it) {
      auto rel = find((*def)["relationships"], it.key());
      need(rel, "UNKNOWN_TARGET", "알 수 없는 관계: " + it.key(), path + "/" + it.key());
      need(integer(it.value()) && number(it.value()) >= number((*rel)["min"]) &&
               number(it.value()) <= number((*rel)["max"]),
           "INITIAL_RANGE", "초기 값이 범위를 벗어났습니다.", path + "/" + it.key());
      s["relationships"][ch.key()][it.key()] = it.value();
    }
  }
}
J createSystems();
void syncRelationshipStages(const J &c, J &s, bool finishingPeriod = false);
int equipmentReserved(const J &c, const J &s, const std::string &itemId);
#include "user-fields.inc"
J create(const J &c, const J &seed, const J &options) {
  checkContent(c);
  auto mode = options.is_object() && options.contains("mode") && options["mode"].is_string()
                  ? str(options, "mode")
                  : std::string();
  need(mode == "full" || mode == "activities-only", "UNSUPPORTED_MODE",
       "activities-only 또는 full 모드를 명시해야 합니다.", "/mode");
  const auto userValues=resolveUserValues(c["project"],options);
  J scene = J::object();
  if (c["project"].contains("visual"))
    scene = c["project"]["visual"].value("defaultScene", J::object());
  J s = {{"coreVersion", VERSION},
         {"contentFingerprint", c["contentFingerprint"]},
         {"mode", mode},
         {"stateRevision", 0},
         {"periodIndex", 0},
         {"phase", "planning"},
         {"schedule", nullptr},
         {"rng", seedRandom(seed)},
         {"attributes", J::object()},
         {"flags", J::object()},
         {"relationships", J::object()},
         {"completedEvents", J::array()},
         {"lastActivityResult", nullptr},
         {"activeNarrative", nullptr},
         {"endingId", nullptr},
         {"presentation",
          {{"backgroundAssetId", scene.value("backgroundAssetId", J(nullptr))},
           {"musicAssetId", scene.value("musicAssetId", J(nullptr))},
           {"portraits", scene.value("portraits", J::array())}}}};
  for (auto &a : c["attributes"])
    s["attributes"][str(a, "id")] = a["initial"];
  if(!c["project"].value("userFields",J::array()).empty())s["userValues"]=userValues;
  for (auto &f : c["flags"])
    s["flags"][str(f, "id")] = f["initial"];
  for (auto &ch : c["characters"]) {
    auto id = str(ch, "id");
    s["relationships"][id] = J::object();
    for (auto &r : ch["relationships"])
      s["relationships"][id][str(r, "id")] = r["initial"];
  }
  merge(c, s, c["project"]["initialState"], "/project/initialState");
  if (c["project"].contains("gameplay")) {
    auto &g = c["project"]["gameplay"];
    auto location = find(g["locations"], g["startLocationId"]);
    need(location, "UNKNOWN_LOCATION", "시작 장소를 찾을 수 없습니다.",
         "/project/gameplay/startLocationId");
    J inventory = J::object();
    for (auto &item : g["items"])
      inventory[str(item, "id")] = item.value("initialQuantity", 0);
    s["gameplay"] = {{"locationId", (*location)["id"]},
                     {"screenId", location->value("screenId", std::string("schedule"))},
                     {"inventory", inventory},
                     {"purchased", J::object()},
                     {"interactionPeriods", J::object()},
                     {"visited", J::array({(*location)["id"]})}};
    if (location->contains("presentation"))
      for (auto key : {"backgroundAssetId", "musicAssetId", "portraits"})
        if ((*location)["presentation"].contains(key))
          s["presentation"][key] = (*location)["presentation"][key];
  }
  if (c["project"].contains("systems")) s["systems"] = createSystems();
  if (options.contains("initialOverrides")) {
    merge(c, s, options["initialOverrides"], "/initialOverrides");
  }
  if(c["project"].contains("roster")) {
    const auto &r=c["project"]["roster"];
    s["roster"]={{"attributes",J::object()},{"assignments",J::object()}};
    for(const auto &m:r["members"]) {
      const auto id=str(m,"characterId");
      s["roster"]["attributes"][id]=J::object();
      for(const auto &key:r["localAttributeIds"]) {const auto k=key.get<std::string>();s["roster"]["attributes"][id][k]=m["initialAttributes"].value(k,s["attributes"][k]);}
      s["roster"]["assignments"][id]=m["activityId"];
    }
    for(const auto &key:r["localAttributeIds"])s["attributes"][key.get<std::string>()]=s["roster"]["attributes"][str(c["project"],"protagonistId")][key.get<std::string>()];
  }
  createCharacterState(c,s);
  if(c["project"].contains("progression"))s["progression"]={{"tick",0},{"actionPoints",c["project"]["progression"]["actionPoints"]["initial"]},{"handled",J::array()},{"pending",nullptr}};
  if(c["project"].value("progression",J::object()).value("model","")=="unified-v2"){
    const auto &cfg=c["project"]["progression"];auto &p=s["progression"];p["sessions"]=J::array();p["actors"]=J::object();
    auto members=c["project"].contains("roster")?c["project"]["roster"]["members"]:J::array({J{{"characterId",c["project"]["protagonistId"]}}});
    for(const auto &m:members){const auto id=str(m,"characterId");auto initial=cfg["actionPoints"]["initial"];for(const auto &pool:cfg.value("actorPools",J::array()))if(pool["actorId"]==id)initial=pool["initial"];p["actors"][id]={{"elapsed",0},{"actionPoints",initial}};}
  }
  return {{"ok", true}, {"state", s}};
}
void scene(J &s, const J &v, J &sounds) {
  for (auto key : {"backgroundAssetId", "musicAssetId", "portraits"})
    if (v.contains(key))
      s["presentation"][key] = v[key];
  if (v.contains("soundAssetId"))
    sounds.push_back(v["soundAssetId"]);
}
std::string nodePath(const J &cursor) {
  return "/" + std::string(cursor["kind"] == "event" ? "events" : "endings") + "/" +
         str(cursor, "id") + "/nodes/" + str(cursor, "nodeId");
}
const J &currentNode(const J &c, const J &s) {
  need(s["phase"] == "narrative" && !s["activeNarrative"].is_null(), "WRONG_PHASE",
       "진행 중인 대본이 없습니다.", "/phase");
  auto &cur = s["activeNarrative"];
  auto n = find(c[cur["kind"] == "event" ? "events" : "endings"], cur["id"]);
  const J *node = n ? find((*n)["nodes"], cur["nodeId"]) : nullptr;
  need(node, "INVALID_CURSOR", "현재 대본 노드를 찾을 수 없습니다.", "/activeNarrative");
  return *node;
}
void available(const J &c, const J &s) {
  if (s["phase"] != "narrative")
    return;
  auto &n = currentNode(c, s);
  if (n["type"] != "choice")
    return;
  bool any = false;
  std::string ids;
  for (auto &ch : n["choices"]) {
    if (condition(c, s, ch["requirements"],
                  nodePath(s["activeNarrative"]) + "/choices/" + str(ch, "id") + "/requirements"))
      any = true;
    if (!ids.empty())
      ids += ", ";
    ids += str(ch, "id");
  }
  need(any, "NO_AVAILABLE_CHOICE", "활성 선택지가 없습니다: " + ids,
       nodePath(s["activeNarrative"]));
}
void enter(const J &c, J &s, const std::string &kind, const J &id) {
  auto def = find(c[kind == "event" ? "events" : "endings"], id);
  need(def, "UNKNOWN_NARRATIVE", "알 수 없는 대본: " + id.get<std::string>(), "/activeNarrative");
  s["phase"] = "narrative";
  s["activeNarrative"] = {{"kind", kind}, {"id", id}, {"nodeId", (*def)["entryNodeId"]}};
  if(kind=="event"&&def->value("scope","")=="actor")s["activeNarrative"]["actorId"]=ownerId(c,s,{{"kind","actor"}},"/activeNarrative/actorId");
  available(c, s);
}
std::vector<J> ordered(const J &list) {
  std::vector<J> out = list.get<std::vector<J>>();
  std::sort(out.begin(), out.end(), [](const J &a, const J &b) {
    return a["priority"] == b["priority"] ? a["id"] < b["id"] : a["priority"] > b["priority"];
  });
  return out;
}
void finish(const J &c, J &s) {
  if (s.contains("systems")) {
    s["phase"] = "period-end"; s["activeNarrative"] = nullptr;
    syncRelationshipStages(c, s, true);
    if (!s["activeNarrative"].is_null()) return;
  }
  s["activeNarrative"] = nullptr;
  s["lastActivityResult"] = nullptr;
  if (number(s["periodIndex"]) == number(c["project"]["durationMonths"]) * periods(c) - 1) {
    if (s["mode"] == "activities-only") {
      s["phase"] = "finished";
      return;
    }
    J id = c["project"]["defaultEndingId"];
    for (auto &e : ordered(c["endings"]))
      if (e["id"] != c["project"]["defaultEndingId"] &&
          condition(c, s, e["requirements"], "/endings/" + str(e, "id") + "/requirements")) {
        id = e["id"];
        break;
      }
    s["endingId"] = id;
    enter(c, s, "ending", id);
  } else {
    s["periodIndex"] = number(s["periodIndex"]) + 1;
    s["phase"] = number(s["periodIndex"]) % periods(c) == 0 ? "planning" : "ready";
    if (s["phase"] == "planning")
      s["schedule"] = nullptr;
  }
}
J inspect(const J &c, const J &s) {
  J out = J::array();
  bool selected = false;
  for (auto &e : ordered(c["events"])) {
    std::string reason;
    if (eventCompleted(c,s,e["id"]))
      reason = "completed";
    else if (e.value("trigger", "") == "interaction")
      reason = "interaction-only";
    else if (s["periodIndex"] < e["window"]["fromPeriod"])
      reason = "before-window";
    else if (s["periodIndex"] > e["window"]["throughPeriod"])
      reason = "expired";
    else if (!condition(c, s, e["requirements"], "/events/" + str(e, "id") + "/requirements"))
      reason = "requirements";
    else if (selected)
      reason = "priority";
    else {
      selected = true;
      reason = "selected";
    }
    out.push_back({{"eventId", e["id"]}, {"priority", e["priority"]}, {"reason", reason}});
  }
  return out;
}
J step(const J &c, J &s, const J &choice, J &sounds) {
  need(s["mode"] == "full", "WRONG_PHASE", "전체 게임 모드가 필요합니다.", "/mode");
  auto n = currentNode(c, s);
  auto cursor = s["activeNarrative"];
  auto src = nodePath(cursor);
  J out = {
      {"cursor", cursor}, {"nodeType", n["type"]}, {"choiceId", choice}, {"changes", J::array()}};
  if (!choice.is_null()) {
    need(n["type"] == "choice", "WRONG_PHASE", "선택 입력을 기다리지 않습니다.",
         "/activeNarrative");
    auto ch = find(n["choices"], choice);
    need(ch, "UNKNOWN_CHOICE", "선택지를 찾을 수 없습니다: " + choice.get<std::string>(),
         "/command/choiceId");
    auto path = src + "/choices/" + choice.get<std::string>();
    need(condition(c, s, (*ch)["requirements"], path + "/requirements"), "CHOICE_DISABLED",
         "선택 조건을 만족하지 않습니다: " + choice.get<std::string>(), path + "/requirements");
    effects(c, s, (*ch)["effects"], path + "/effects", out["changes"]);
    s["activeNarrative"]["nodeId"] = (*ch)["next"];
  } else {
    auto type = str(n, "type");
    if (type == "choice")
      throw Fault("CHOICE_REQUIRED", "선택지 ID를 지정해야 합니다.", "/command");
    if (type == "dialogue")
      s["activeNarrative"]["nodeId"] = n["next"];
    else if (type == "scene") {
      scene(s, n["scene"], sounds);
      s["activeNarrative"]["nodeId"] = n["next"];
    } else if (type == "branch")
      s["activeNarrative"]["nodeId"] =
          n[condition(c, s, n["condition"], src + "/condition") ? "then" : "else"];
    else if (type == "effect") {
      effects(c, s, n["effects"], src + "/effects", out["changes"]);
      s["activeNarrative"]["nodeId"] = n["next"];
    } else if (type == "end") {
      if (cursor["kind"] == "event") {
        auto &completed=cursor.contains("actorId")?s["roster"]["completedEvents"][str(cursor,"actorId")]:s["completedEvents"];
        if(std::find(completed.begin(),completed.end(),cursor["id"])==completed.end())completed.push_back(cursor["id"]);
        if (cursor.contains("returnPhase")) {
          s["activeNarrative"] = nullptr;
          s["phase"] = cursor["returnPhase"];
          if (cursor.contains("returnPresentation"))
            s["presentation"] = cursor["returnPresentation"];
          if (cursor.value("finishPeriod", false)) finish(c, s);
        } else
          finish(c, s);
      } else {
        s["activeNarrative"] = nullptr;
        s["phase"] = "finished";
      }
    }
  }
  available(c, s);
  return out;
}
void applyDiet(const J &c, J &s, J &changes);
#include "generic.inc"
J executeRoster(const J &c,J &s);
J execute(const J &c, const J &before, J &s, bool individual = false) {
  need(before["phase"] == "ready" && !before["schedule"].is_null(), "WRONG_PHASE",
       "실행할 일정이 준비되지 않았습니다.", "/phase");
  const auto &schedule = before.at("schedule");
  need(schedule.is_array() && schedule.size() == size_t(periods(c)) &&
           std::all_of(schedule.begin(), schedule.end(), [](const J &id) { return id.is_string(); }),
       "INCOMPATIBLE_STATE", "저장된 일정에는 세 기간의 활동 ID가 필요합니다.", "/schedule");
  auto id = schedule.at(size_t(number(before["periodIndex"]) % periods(c)));
  auto a = find(c["activities"], id);
  need(a, "UNKNOWN_ACTIVITY", "알 수 없는 활동: " + id.get<std::string>(), "/schedule");
  auto p = "/activities/" + id.get<std::string>();
  J dietChanges = J::array();
  if(!individual) {
    boundaryRules(c,s,dietChanges,true);applyPolicies(c,s,dietChanges,"beforeActivity");
    if(c["project"].contains("roster")) {
      J result=executeRoster(c,s);
      result["changes"].insert(result["changes"].begin(),dietChanges.begin(),dietChanges.end());
      applyPolicies(c,s,result["changes"],"afterActivity");dayRules(c,s,result["changes"]);
      s["lastActivityResult"]=result;return result;
    }
  }
  // Generic pre-activity hooks shape the activity input. Preserve the legacy
  // diet timing so old command logs retain their requirement/chance semantics.
  const J activityInput = s;
  need(condition(c, activityInput, (*a)["requirements"], p + "/requirements"), "REQUIREMENTS_NOT_MET",
       "활동 조건을 만족하지 않습니다: " + id.get<std::string>(), p + "/requirements");
  if(!individual) applyDiet(c, s, dietChanges);
  size_t i = 0;
  for (auto &cost : (*a)["costs"]) {
    auto def = find(c["attributes"], cost["attributeId"]);
    need(def && number(s["attributes"][str(cost, "attributeId")]) - number(cost["amount"]) >=
                    number((*def)["min"]),
         "INSUFFICIENT_COST", "활동 비용을 지불할 수 없습니다.",
         p + "/costs/" + std::to_string(i++));
  }
  J out = {{"activityId", id},
           {"time", calendar(c, before)},
           {"changes", dietChanges},
           {"success", nullptr}};
  if (a->contains("successCheck")) {
    auto requested =
        evaluate(c, activityInput, (*a)["successCheck"]["chance"], p + "/successCheck/chance");
    auto chance = std::min(int64_t(10000), std::max(int64_t(0), requested));
    auto draws = number(s["rng"]["draws"]);
    auto roll = sample(s["rng"], 10000);
    out["success"] = {{"requestedChance", requested},
                      {"chance", chance},
                      {"roll", roll},
                      {"passed", roll < uint64_t(chance)},
                      {"rawDraws", number(s["rng"]["draws"]) - draws}};
  }
  i = 0;
  for (auto &cost : (*a)["costs"]) {
    auto key = str(cost, "attributeId");
    auto prev = number(s["attributes"][key]), amount = number(cost["amount"]);
    auto path = p + "/costs/" + std::to_string(i++);
    auto after = i32(prev - amount, path);
    s["attributes"][key] = after;
    out["changes"].push_back({{"kind", "numeric"},
                              {"source", path},
                              {"target", {{"kind", "attribute"}, {"attributeId", key}}},
                              {"before", prev},
                              {"operand", -amount},
                              {"requested", after},
                              {"after", after},
                              {"delta", -amount},
                              {"clamped", false}});
  }
  effects(c, s, (*a)["baseEffects"], p + "/baseEffects", out["changes"]);
  if (!out["success"].is_null()) {
    auto branch = out["success"]["passed"].get<bool>() ? "successEffects" : "failureEffects";
    effects(c, s, (*a)[branch], p + "/" + branch, out["changes"]);
  }
  if(!individual){applyPolicies(c,s,out["changes"],"afterActivity");dayRules(c,s,out["changes"]);}
  s["phase"] = "activity-result";
  s["lastActivityResult"] = out;
  return out;
}
#include "roster.inc"
#include "gameplay.inc"
#include "systems.inc"
#include "progression.inc"
void parseCommand(const J &cmd, int count) {
  need(cmd.is_object(), "INVALID_COMMAND", "명령은 객체여야 합니다.", "/command");
  auto type = cmd.contains("type") && cmd["type"].is_string() ? str(cmd, "type") : std::string();
  if(type=="tacticsUnequip"||type=="tacticsAction"||type=="tacticsEquip"||type=="tacticsFlee"){std::vector<std::string> fields=type=="tacticsAction"?std::vector<std::string>{"unitId","skillId","targetId"}:type=="tacticsEquip"?std::vector<std::string>{"unitId","equipmentId"}:type=="tacticsUnequip"?std::vector<std::string>{"unitId","slot"}:std::vector<std::string>{};need(cmd.size()==fields.size()+1,"INVALID_COMMAND","전투 명령의 필드를 확인하세요.","/command");for(const auto& k:fields)need(cmd.contains(k)&&cmd[k].is_string()&&!cmd[k].get<std::string>().empty(),"INVALID_COMMAND","전투 명령의 ID가 필요합니다.","/command/"+k);
  } else if (type == "acceptQuest" || type == "claimQuest" || type == "equip" || type == "unequip" || type == "moveExpedition") {
    std::string field = type == "equip" ? "equipmentId" : type == "unequip" ? "slot" : type == "moveExpedition" ? "nodeId" : "questId";
    need(cmd.size() == 2 && cmd.contains(field) && cmd[field].is_string() && !cmd[field].get<std::string>().empty(), "INVALID_COMMAND", "시스템 명령의 ID가 필요합니다.", "/command/" + field);
    if (type == "unequip") need(cmd["slot"] == "weapon" || cmd["slot"] == "armor" || cmd["slot"] == "accessory", "INVALID_COMMAND", "장비 슬롯이 올바르지 않습니다.", "/command/slot");
  } else if (type == "battleAction") {
    auto action = cmd.value("action", J(nullptr));
    need((action == "attack" || action == "guard" || action == "flee" || action == "useItem") && cmd.size() == (action == "useItem" ? 3 : 2) && (action != "useItem" || (cmd.contains("itemId") && cmd["itemId"].is_string() && !cmd["itemId"].get<std::string>().empty())), "INVALID_COMMAND", "전투 행동과 물품 ID를 확인해 주세요.", "/command");
  } else if (type == "returnFromExpedition") {
    need(cmd.size() == 1, "INVALID_COMMAND", "귀환 명령에 추가 필드를 사용할 수 없습니다.", "/command");
  } else if (type == "navigate" || type == "visit" || type == "interact") {
    std::string field = type == "navigate" ? "screenId"
                        : type == "visit"  ? "locationId"
                                           : "interactionId";
    need(cmd.size() == 2 && cmd.contains(field) && cmd[field].is_string() &&
             !cmd[field].get<std::string>().empty(),
         "INVALID_COMMAND", type + "에는 " + field + "가 필요합니다.", "/command/" + field);
  } else if (type == "selectPolicy") {
    need(cmd.size() == 3 && cmd.contains("groupId") && cmd["groupId"].is_string() &&
         !cmd["groupId"].get<std::string>().empty() && cmd.contains("optionId") &&
         (cmd["optionId"].is_null() || (cmd["optionId"].is_string() && !cmd["optionId"].get<std::string>().empty())),
         "INVALID_COMMAND", "정책 그룹과 선택지 또는 null이 필요합니다.", "/command");
  } else if(type=="cancelActorSession") {
    need(cmd.size()==2&&cmd.contains("actorId")&&cmd["actorId"].is_string()&&!cmd["actorId"].get<std::string>().empty(),"INVALID_COMMAND","취소할 실행자가 필요합니다.","/command/actorId");
  } else if(type=="waitTime") {
    need(cmd.size()==2&&cmd.contains("ticks")&&integer(cmd["ticks"])&&number(cmd["ticks"])>0&&number(cmd["ticks"])<=1000000,"INVALID_COMMAND","대기 시간은 1..1000000 정수입니다.","/command/ticks");
  } else if (type == "executeActorAction") {
    need(cmd.size()==4&&cmd.contains("actorId")&&cmd["actorId"].is_string()&&!cmd["actorId"].get<std::string>().empty()&&cmd.contains("actionId")&&cmd["actionId"].is_string()&&!cmd["actionId"].get<std::string>().empty()&&cmd.contains("quantity")&&integer(cmd["quantity"])&&number(cmd["quantity"])>0&&number(cmd["quantity"])<=2147483647LL,"INVALID_COMMAND","실행자·행동 ID와 양의 정수 수량이 필요합니다.","/command");
  } else if (type == "executeAction") {
    need(cmd.size() == 3 && cmd.contains("actionId") && cmd["actionId"].is_string() &&
         !cmd["actionId"].get<std::string>().empty() && cmd.contains("quantity") && integer(cmd["quantity"]) &&
         number(cmd["quantity"]) > 0 && number(cmd["quantity"]) <= 2147483647LL,
         "INVALID_COMMAND", "행동 ID와 양의 정수 수량이 필요합니다.", "/command");
  } else if (type == "setDiet") {
    need(cmd.size() == 2 && cmd.contains("dietId") && (cmd["dietId"].is_null() ||
         (cmd["dietId"].is_string() && !cmd["dietId"].get<std::string>().empty())),
         "INVALID_COMMAND", "setDiet에는 dietId 또는 null이 필요합니다.", "/command/dietId");
  } else if (type == "useService") {
    need(cmd.size() == 3 && cmd.contains("serviceId") && cmd["serviceId"].is_string() &&
         !cmd["serviceId"].get<std::string>().empty() && cmd.contains("quantity") &&
         integer(cmd["quantity"]) && number(cmd["quantity"]) > 0 && number(cmd["quantity"]) <= 2147483647LL,
         "INVALID_COMMAND", "서비스 ID와 양의 정수 수량이 필요합니다.", "/command");
  } else if (type == "buy" || type == "sell") {
    need(cmd.size() == 4 && cmd.contains("shopId") && cmd["shopId"].is_string() &&
             !cmd["shopId"].get<std::string>().empty() && cmd.contains("itemId") &&
             cmd["itemId"].is_string() && !cmd["itemId"].get<std::string>().empty() &&
             cmd.contains("quantity") && integer(cmd["quantity"]) && number(cmd["quantity"]) > 0 &&
             number(cmd["quantity"]) <= 2147483647LL,
         "INVALID_COMMAND", "buy에는 shopId, itemId와 양의 정수 quantity가 필요합니다.",
         "/command");
  } else if(type=="assignActivities") {
    need(cmd.size()==2 && cmd.contains("assignments") && cmd["assignments"].is_object(),"INVALID_COMMAND","대상별 활동 배정이 필요합니다.","/command/assignments");
    for(const auto &id:cmd["assignments"])need(id.is_string(),"INVALID_COMMAND","활동 ID가 필요합니다.","/command/assignments");
  } else if (type == "setSchedule") {
    bool good = cmd.size() == 2 && cmd.contains("activityIds") && cmd["activityIds"].is_array() &&
                cmd["activityIds"].size() == size_t(count);
    if (good)
      for (auto &id : cmd["activityIds"])
        good = good && id.is_string();
    need(good, "INVALID_COMMAND", "월별 일정 칸 수와 같은 개수의 활동 ID가 필요합니다.",
         "/command/activityIds");
  } else if (type == "choose")
    need(cmd.size() == 2 && cmd.contains("choiceId") && cmd["choiceId"].is_string() &&
             !cmd["choiceId"].get<std::string>().empty(),
         "INVALID_COMMAND", "choose에는 choiceId가 필요합니다.", "/command/choiceId");
  else
    need(cmd.size() == 1 && (type == "advanceSession" || type == "cancelSession" || type == "executeActivity" || type == "acknowledgeResult" ||
                             type == "completePeriodWithoutEvents" || type == "resolvePeriodEnd" ||
                             type == "advanceNarrative"),
         "INVALID_COMMAND", "지원하지 않는 명령 또는 추가 필드입니다.", "/command/type");
}
J command(const J &c, const J &old, const J &cmd, const J &rev) {
  checkContent(c);
  need(old.value("coreVersion", "") == VERSION &&
           (old["mode"] == "full" || old["mode"] == "activities-only"),
       "INCOMPATIBLE_STATE", "실행 규칙 버전 또는 모드가 다릅니다.", "/coreVersion");
  need(old["contentFingerprint"] == c["contentFingerprint"], "CONTENT_MISMATCH",
       "실행 중인 콘텐츠와 스냅샷이 다릅니다.", "/contentFingerprint");
  need(integer(rev) && number(rev) >= 0 && number(rev) <= SAFE, "INVALID_REVISION",
       "예상 버전은 음수가 아닌 안전한 정수여야 합니다.", "/expectedRevision");
  need(rev == old["stateRevision"], "REVISION_CONFLICT", "예상 버전과 현재 버전이 다릅니다.",
       "/expectedRevision");
  need(integer(old["stateRevision"]) && number(old["stateRevision"]) < SAFE, "REVISION_OVERFLOW",
       "상태 버전을 증가시킬 수 없습니다.", "/stateRevision");
  calendar(c, old);
  parseCommand(cmd, periods(c));
  progressionGuard(old,cmd);
  RuleScope rulesScope;
  activeRules->relationships = old["relationships"];
  J s = old, activity = nullptr, narrative = nullptr, decisions = J::array(), sounds = J::array();
  auto type = str(cmd, "type");
  if(type=="advanceSession"||type=="cancelSession"||type=="cancelActorSession"||type=="waitTime"){
    if(progressSession(c,s,cmd,sounds)){const auto before=s;activity=execute(c,before,s);auto a=find(c["activities"],activity["activityId"]);if(a->contains("presentation"))scene(s,(*a)["presentation"],sounds);}
  }
  else if((type=="executeAction"||type=="executeActorAction")&&startTimedAction(c,s,cmd)){}
  else if (type == "tacticsUnequip" || type == "tacticsAction" || type == "tacticsFlee" || type == "tacticsEquip" || type == "acceptQuest" || type == "claimQuest" || type == "equip" || type == "unequip" || type == "moveExpedition" || type == "returnFromExpedition" || type == "battleAction")
    systems(c, s, cmd, sounds);
  else if (type == "navigate" || type == "visit" || type == "buy" || type == "sell" ||
           type == "useService" || type == "setDiet" || type == "interact" ||
           type == "executeAction" || type == "selectPolicy")
  {
    gameplay(c, s, cmd, sounds);
    if (type == "executeAction") {
      const auto actions = c["project"]["gameplay"].value("actions", J::array());
      const auto action = find(actions, cmd["actionId"]);
      if (action && action->contains("expeditionId")) {
        startExpedition(c, s, (*action)["expeditionId"], sounds);
        s["systems"]["expedition"]["returnPhase"] = old["phase"];
      }
    }
  }
  else if(type=="executeActorAction") {beginActorAction(c,s,cmd);auto action=cmd;action["type"]="executeAction";action.erase("actorId");gameplay(c,s,action,sounds);}
  else if(type=="assignActivities")assignActivities(c,old,s,cmd["assignments"]);
  else if (type == "setSchedule") {
    need(!c["project"].contains("roster"),"ROSTER_ASSIGNMENTS","다인 프로젝트는 대상별 활동 배정을 사용하세요.","/command");
    need(old["phase"] == "planning" || old["phase"] == "ready", "WRONG_PHASE",
         "일정 편집을 기다리는 상태가 아닙니다.", "/phase");
    size_t i = 0;
    for (auto &id : cmd["activityIds"]) {
      auto p = "/command/activityIds/" + std::to_string(i);
      need(find(c["activities"], id), "UNKNOWN_ACTIVITY",
           "알 수 없는 활동: " + id.get<std::string>(), p);
      need(i >= size_t(number(old["periodIndex"]) % periods(c)) ||
               (old["schedule"].is_array() && i < old["schedule"].size() &&
                old["schedule"].at(i) == id),
           "COMPLETED_SLOT", "완료한 일정은 변경할 수 없습니다.", p);
      i++;
    }
    s["schedule"] = cmd["activityIds"];
    s["phase"] = "ready";
  } else if (type == "executeActivity") {
    if(!startTimedCalendar(c,s)){
    activity = execute(c, old, s);
    auto a = find(c["activities"], activity["activityId"]);
    if (a->contains("presentation"))
      scene(s, (*a)["presentation"], sounds);
    if (a->contains("expeditionId")) startExpedition(c, s, (*a)["expeditionId"], sounds);
    }
  } else if (type == "acknowledgeResult") {
    need(old["phase"] == "activity-result", "WRONG_PHASE", "확인할 활동 결과가 없습니다.",
         "/phase");
    s["phase"] = "period-end";
  } else if (type == "completePeriodWithoutEvents") {
    need(old["phase"] == "period-end" && old["mode"] == "activities-only", "WRONG_PHASE",
         "활동 전용 모드의 기간 종료 경계가 아닙니다.", "/phase");
    J changes = J::array(); boundaryRules(c, s, changes, false);
    finish(c, s);
  } else if (type == "resolvePeriodEnd") {
    need(s["mode"] == "full" && s["phase"] == "period-end", "WRONG_PHASE",
         "전체 게임 모드의 기간 종료 경계가 아닙니다.", "/phase");
    J changes = J::array(); boundaryRules(c, s, changes, false);
    decisions = inspect(c, s);
    bool selected = false;
    for (auto &d : decisions)
      if (d["reason"] == "selected") {
        enter(c, s, "event", d["eventId"]);
        selected = true;
        break;
      }
    if (!selected)
      finish(c, s);
  } else if (type == "advanceNarrative")
    narrative = step(c, s, nullptr, sounds);
  else if (type == "choose")
    narrative = step(c, s, cmd["choiceId"], sounds);
  J ruleChanges = J::array();
  if (type == "visit") runRules(c, s, "locationEnter", {{"locationId",cmd["locationId"]}}, ruleChanges);
  if (type == "buy") runRules(c, s, "purchaseComplete", {{"shopId",cmd["shopId"]},{"itemId",cmd["itemId"]}}, ruleChanges);
  syncRelationshipStages(c, s);
  relationshipRules(c, s, old["relationships"], ruleChanges);
  if(s.contains("roster")){s["roster"].erase("actorId");s["roster"].erase("activityActorId");}
  if(s.contains("roster"))for(const auto &key:c["project"]["roster"]["localAttributeIds"])s["roster"]["attributes"][str(c["project"],"protagonistId")][key.get<std::string>()]=s["attributes"][key.get<std::string>()];
  s["stateRevision"] = number(s["stateRevision"]) + 1;
  J transition = {{"command", cmd},
                  {"fromRevision", old["stateRevision"]},
                  {"toRevision", s["stateRevision"]},
                  {"fromPhase", old["phase"]},
                  {"toPhase", s["phase"]},
                  {"time", calendar(c, s)},
                  {"activity", activity},
                  {"narrative", narrative},
                  {"eventDecisions", decisions},
                  {"endingSelected", s["endingId"] != old["endingId"] ? s["endingId"] : J(nullptr)},
                  {"soundCues", sounds}};
  return {{"ok", true}, {"state", s}, {"transition", transition}};
}
// clang-format off
#include "view.inc"
#include "systems-view.inc"
#include "protocol.inc"
// clang-format on
} // namespace raising
