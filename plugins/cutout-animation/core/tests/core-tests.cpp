// Core tests: validation, evaluation, playback, multi-instance, persistence, signals.
#include <cmath>
#include <functional>
#include <iostream>
#include "cutout/host.hpp"
#include "cutout/signals.hpp"
#include "cutout/templates.hpp"

using namespace cutout;
namespace {
int failures = 0, checks = 0;
std::string current;
void check(bool ok, const std::string& what) {
  checks++;
  if (!ok) { failures++; std::cerr << "FAIL [" << current << "] " << what << "\n"; }
}
bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) <= eps; }
bool hasCode(const std::vector<Diagnostic>& ds, const std::string& code) {
  for (const auto& d : ds) if (d.code == code) return true;
  return false;
}
void test(const std::string& name, const std::function<void()>& body) {
  current = name;
  try { body(); } catch (const std::exception& e) { failures++; std::cerr << "FAIL [" << name << "] threw: " << e.what() << "\n"; }
}

Json rigData() {
  return Json::parse(R"({
    "schemaVersion": 1,
    "canvas": {"width": 200, "height": 300, "originX": 100, "originY": 280},
    "bones": [
      {"id": "root", "parent": null, "x": 100, "y": 280},
      {"id": "body", "parent": "root", "x": 0, "y": -100},
      {"id": "head", "parent": "body", "x": 0, "y": -80}
    ],
    "slots": [
      {"id": "torso", "bone": "body", "attachment": "torso"},
      {"id": "face", "bone": "head", "attachment": "face"},
      {"id": "eyes", "bone": "head", "attachment": "open"}
    ],
    "attachments": [
      {"slot": "torso", "name": "torso", "imageAssetId": "img-torso", "width": 80, "height": 120, "pivotX": 0.5, "pivotY": 1},
      {"slot": "face", "name": "face", "imageAssetId": "img-face", "width": 60, "height": 60},
      {"slot": "eyes", "name": "open", "imageAssetId": "img-eyes-open", "width": 40, "height": 10},
      {"slot": "eyes", "name": "closed", "imageAssetId": "img-eyes-closed", "width": 40, "height": 10}
    ],
    "clips": [
      {"id": "idle", "name": "Idle", "durationMs": 1000, "loop": true,
       "tracks": [{"target": "bone:body", "property": "y", "keys": [
         {"timeMs": 0, "value": -100}, {"timeMs": 500, "value": -104}, {"timeMs": 1000, "value": -100}]}],
       "events": [{"timeMs": 0, "name": "breath"}]},
      {"id": "blink", "durationMs": 2000, "loop": true,
       "tracks": [{"target": "slot:eyes", "property": "attachment", "keys": [
         {"timeMs": 0, "value": "open"}, {"timeMs": 1800, "value": "closed"}, {"timeMs": 1900, "value": "open"}]}]},
      {"id": "joy", "durationMs": 400, "loop": false,
       "tracks": [
         {"target": "bone:head", "property": "rotation", "keys": [{"timeMs": 0, "value": 0}, {"timeMs": 400, "value": 20}]},
         {"target": "bone:body", "property": "scaleY", "keys": [{"timeMs": 0, "value": 1, "interp": "step"}, {"timeMs": 200, "value": 1.2}]}
       ],
       "events": [{"timeMs": 200, "name": "peak"}, {"timeMs": 400, "name": "done"}]}
    ],
    "mix": {"defaultMs": 100, "pairs": [{"from": "joy", "to": "idle", "durationMs": 200}]}
  })");
}
Json controllerData() {
  return Json::parse(R"({
    "schemaVersion": 1, "rig": "hero", "defaultClip": "idle", "characterId": "alice",
    "layers": [{"track": 1, "clip": "blink"}],
    "bindings": [
      {"id": "on-success", "signal": "actionResult", "match": {"outcome": "success"}, "clip": "joy", "then": "default"},
      {"id": "talk", "signal": "dialogueLine", "match": {"self": "true"}, "clip": "idle", "track": 2, "loop": true, "stopOn": ["dialogueEnded"]}
    ]
  })");
}
std::shared_ptr<const Rig> rig() { return std::make_shared<const Rig>(parseRig("hero", rigData())); }
/** rigData() + a one-shot clip whose tracks use every bezier flavour (also the Wasm parity fixture). */
Json bezierRig() {
  Json d = rigData();
  d["clips"].push_back(Json::parse(R"({"id": "ease", "durationMs": 1000, "loop": false, "tracks": [
    {"target": "bone:body", "property": "y", "keys": [
      {"timeMs": 0, "value": -100, "interp": "bezier", "curve": [0.42, 0, 0.58, 1]}, {"timeMs": 600, "value": -112, "interp": "bezier", "curve": [0, 0, 0.58, 1]}, {"timeMs": 1000, "value": -100}]},
    {"target": "bone:head", "property": "rotation", "keys": [
      {"timeMs": 0, "value": 0, "interp": "bezier", "curve": [0.34, 1.56, 0.64, 1]}, {"timeMs": 400, "value": 20, "interp": "step"}, {"timeMs": 700, "value": 5}, {"timeMs": 1000, "value": -10}]},
    {"target": "bone:body", "property": "scaleY", "keys": [
      {"timeMs": 0, "value": 1, "interp": "bezier", "curve": [0.3333333333333333, 0, 0.6666666666666666, 1]}, {"timeMs": 1000, "value": 1.2}]},
    {"target": "slot:face", "property": "alpha", "keys": [
      {"timeMs": 100, "value": 1, "interp": "bezier", "curve": [0.9, -0.4, 0.1, 1.4]}, {"timeMs": 900, "value": 0.5}]}
  ]})"));
  return d;
}
const std::vector<double>& parityTimes() {
  static const std::vector<double> t = {0, 1, 33.25, 100, 125, 250, 333.3333, 399.9, 400, 512.5, 600, 650.75, 777.7, 899, 999.99, 1000};
  return t;
}
std::vector<std::string> kinds(const std::vector<Notification>& ns) {
  std::vector<std::string> out;
  for (const auto& n : ns) out.push_back(n.kind + ":" + n.clip + (n.kind == "event" ? ":" + n.name : ""));
  return out;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc > 1 && std::string(argv[1]) == "--bezier-parity") {
    // Native reference samples for tests/plugins/cutout-bezier-parity.test.mjs (Wasm `sample`).
    Host host;
    Json samples = Json::array();
    for (double t : parityTimes())
      samples.push_back({{"timeMs", t}, {"pose", host.handle({{"op", "sample"}, {"rig", bezierRig()}, {"clip", "ease"}, {"timeMs", t}})["pose"]}});
    std::cout << Json({{"rig", bezierRig()}, {"clip", "ease"}, {"samples", samples}}).dump() << "\n";
    return 0;
  }
  test("valid fixture has no diagnostics", [] {
    const auto ds = validateRig(rigData());
    check(ds.empty(), "fixture rig validates");
    check(validateController(controllerData(), rig().get()).empty(), "fixture controller validates");
  });

  test("parent cycle, missing parent, missing image", [] {
    auto d = rigData();
    d["bones"][0]["parent"] = "head";
    check(hasCode(validateRig(d), "ANIM_PARENT_CYCLE"), "cycle detected");
    d = rigData(); d["bones"][2]["parent"] = "neck";
    check(hasCode(validateRig(d), "ANIM_PARENT_MISSING"), "missing parent detected");
    d = rigData(); d["attachments"][1]["imageAssetId"] = "";
    check(hasCode(validateRig(d), "ANIM_IMAGE_MISSING"), "missing image detected");
    d = rigData(); d["slots"][0]["bone"] = "tail";
    check(hasCode(validateRig(d), "ANIM_BONE_MISSING"), "slot bone missing");
    d = rigData(); d["bones"][1]["id"] = "root";
    check(hasCode(validateRig(d), "ANIM_DUP_ID"), "duplicate bone id");
  });

  test("invalid times and numbers", [] {
    auto d = rigData();
    d["clips"][0]["tracks"][0]["keys"][1]["timeMs"] = 1500;
    check(hasCode(validateRig(d), "ANIM_KEY_TIME_RANGE"), "key past clip end");
    d = rigData(); d["clips"][0]["tracks"][0]["keys"][1]["timeMs"] = 0;
    check(hasCode(validateRig(d), "ANIM_KEY_UNSORTED"), "duplicate key time");
    d = rigData(); d["clips"][0]["tracks"][0]["keys"][1]["timeMs"] = 12.5;
    check(hasCode(validateRig(d), "ANIM_KEY_TIME"), "fractional key time");
    d = rigData(); d["clips"][0]["durationMs"] = 0;
    check(hasCode(validateRig(d), "ANIM_DURATION"), "zero duration");
    d = rigData(); d["bones"][1]["rotation"] = 1e300;
    check(hasCode(validateRig(d), "ANIM_NONFINITE"), "huge number rejected");
    d = rigData(); d["slots"][0]["alpha"] = 2;
    check(hasCode(validateRig(d), "ANIM_ALPHA_RANGE"), "alpha range");
    d = rigData(); d["clips"][1]["tracks"][0]["keys"][1]["value"] = "squint";
    check(hasCode(validateRig(d), "ANIM_ATTACHMENT_UNKNOWN"), "unknown attachment key");
    d = rigData(); d["clips"][1]["tracks"][0]["property"] = "rotation";
    check(hasCode(validateRig(d), "ANIM_PROPERTY"), "bone property on slot target");
    auto c = controllerData(); c["defaultClip"] = "sleep";
    check(hasCode(validateController(c, rig().get()), "ANIM_CLIP_UNKNOWN"), "controller unknown clip");
    bool threw = false;
    try { parseRig("bad", Json::parse(R"({"schemaVersion":1,"bones":[]})")); } catch (const std::exception&) { threw = true; }
    check(threw, "parseRig refuses invalid data");
  });

  test("keyframe interpolation and world transform", [] {
    const auto r = rig();
    auto p = samplePose(*r, r->clipIndex("idle"), 250);
    check(near(p.bones[1].y, -102), "linear midpoint y=-102");
    p = samplePose(*r, r->clipIndex("joy"), 100);
    check(near(p.bones[1].scaleY, 1), "step holds first value before next key");
    check(near(p.bones[2].rotation, 5), "rotation linear 5deg at 100ms");
    p = samplePose(*r, r->clipIndex("joy"), 400);
    check(near(p.bones[1].scaleY, 1.2) && near(p.bones[2].rotation, 20), "end values");
    // World: head = root(100,280) * body(0,-104 at 500ms) * head(0,-80)
    p = samplePose(*r, r->clipIndex("idle"), 500);
    check(near(p.world[2].e, 100) && near(p.world[2].f, 280 - 104 - 80), "world translation of head");
    // Parent rotation rotates child offset: rotate body 90deg -> head offset (0,-80) becomes (80,0).
    auto d = rigData(); d["bones"][1]["rotation"] = 90;
    const Rig rotated = parseRig("r", d);
    p = samplePose(rotated, -1, 0);
    check(near(p.world[2].e, 100 + 80, 1e-9) && near(p.world[2].f, 180, 1e-9), "child follows parent rotation");
  });

  test("draw list order, pivots and attachment swap", [] {
    Instance inst(rig());
    inst.play(0, "blink");
    auto items = inst.drawList();
    check(items.size() == 3 && items[0].slot == "torso" && items[2].slot == "eyes", "draw order follows slots");
    // torso pivot (0.5,1) sits on body bone: body world (100,180) minus origin (100,280) => (0,-100)
    double x, y; items[0].matrix.apply(40, 120, x, y);
    check(near(x, 0) && near(y, -100), "pivot maps to bone position in instance space");
    inst.update(1850);
    items = inst.drawList();
    check(items[2].imageAssetId == "img-eyes-closed", "attachment key swaps image");
    inst.update(100);
    check(inst.drawList()[2].imageAssetId == "img-eyes-open", "eyes reopen");
  });

  test("loop boundaries and events", [] {
    Instance inst(rig());
    inst.play(0, "idle");
    inst.takeNotifications();
    inst.update(1);
    auto n = kinds(inst.takeNotifications());
    check(n.size() == 1 && n[0] == "event:idle:breath", "event at 0 fires on first advance");
    inst.update(999);
    n = kinds(inst.takeNotifications());
    check(n.size() == 1 && n[0] == "complete:idle", "boundary at exactly 1000 completes once");
    inst.update(1);
    n = kinds(inst.takeNotifications());
    check(n.size() == 1 && n[0] == "event:idle:breath", "next loop's 0ms event fires on the following advance (half-open interval)");
    inst.update(2499);  // 1001 -> 3500 crosses 2000 and 3000
    auto raw = inst.takeNotifications();
    int completes = 0; int64_t lastLoop = -1;
    for (const auto& x : raw) if (x.kind == "complete") { completes++; lastLoop = x.loop; }
    check(completes == 2 && lastLoop == 2, "large dt reports each completed loop once");
  });

  test("one-shot completes once and returns to default with mix", [] {
    Instance inst(rig(), parseController("c", controllerData()));
    inst.update(300);
    inst.takeNotifications();
    inst.signal("actionResult", {{"outcome", "success"}, {"actionId", "work"}});
    auto n = kinds(inst.takeNotifications());
    check(n.size() == 2 && n[0] == "interrupt:idle" && n[1] == "start:joy", "interrupt then start");
    inst.update(201);
    n = kinds(inst.takeNotifications());
    check(n.size() == 2 && n[0] == "end:idle" && n[1] == "event:joy:peak", "old clip ends after 100ms mix; peak at 200");
    inst.update(199);
    n = kinds(inst.takeNotifications());
    // done event, complete, then automatic return to idle (interrupt joy, start idle)
    check(n.size() == 4 && n[0] == "event:joy:done" && n[1] == "complete:joy" && n[2] == "interrupt:joy" && n[3] == "start:idle", "complete then default");
    inst.update(1000);
    for (const auto& x : inst.takeNotifications()) check(!(x.kind == "complete" && x.clip == "joy"), "joy never completes twice");
  });

  test("mix blends halfway", [] {
    Instance inst(rig());
    inst.play(0, "joy", false, 1, Then::Hold, 0);
    inst.update(400);  // head rotation 20, held
    inst.play(0, "idle", std::nullopt, 1, Then::Hold, 200);
    inst.update(100);
    const auto p = inst.pose();
    check(near(p.bones[2].rotation, 10), "rotation halfway between 20 and setup 0");
  });

  test("multiple instances are independent", [] {
    Host host;
    auto ok = [](const Json& r) { return r.value("ok", false); };
    check(ok(host.handle({{"op", "loadRig"}, {"id", "hero"}, {"data", rigData()}})), "load");
    check(ok(host.handle({{"op", "create"}, {"instance", "a"}, {"rig", "hero"}, {"controller", controllerData()}})), "create a");
    check(ok(host.handle({{"op", "create"}, {"instance", "b"}, {"rig", "hero"}, {"controller", controllerData()}})), "create b");
    host.handle({{"op", "update"}, {"dtMs", 100}});
    host.handle({{"op", "signal"}, {"instance", "a"}, {"name", "actionResult"}, {"params", {{"outcome", "success"}}}});
    host.handle({{"op", "update"}, {"dtMs", 150}});
    const auto a = host.handle({{"op", "describe"}, {"instance", "a"}})["state"]["tracks"][0];
    const auto b = host.handle({{"op", "describe"}, {"instance", "b"}})["state"]["tracks"][0];
    check(a["clip"] == "joy" && b["clip"] == "idle", "only instance a reacted");
    check(near(b["timeMs"].get<double>(), 250), "b keeps its own clock");
    check(!ok(host.request(Json({{"op", "unloadRig"}, {"id", "hero"}}).dump())), "shared rig cannot unload while used");
    check(!host.request("{not json").value("ok", true), "bad json is an error reply, not a crash");
  });

  test("frame rate does not change notifications or final pose", [] {
    // Binary-exact steps (32/64/128 fps and an uneven mix) so the signal lands on the same instant.
    auto run = [](std::vector<double> pattern) {
      Instance inst(rig(), parseController("c", controllerData()));
      std::vector<std::string> log;
      double t = 0; size_t i = 0;
      bool fired = false;
      while (t < 3000) {
        if (!fired && t >= 1000) { inst.signal("actionResult", {{"outcome", "success"}}); fired = true; }
        const double step = pattern[i++ % pattern.size()];
        inst.update(step); t += step;
        for (const auto& n : inst.takeNotifications()) log.push_back(n.kind + n.clip + n.name);
      }
      return std::make_pair(log, inst.pose());
    };
    const auto a = run({31.25}), b = run({7.8125}), c = run({7.8125, 23.4375, 15.625, 15.625});
    check(a.first == b.first && b.first == c.first, "same notification sequence at 32/128 fps and uneven frames");
    check(near(a.second.bones[1].y, b.second.bones[1].y, 1e-6), "same final pose");
  });

  test("save and restore policy", [] {
    Instance inst(rig(), parseController("c", controllerData()));
    inst.update(700);
    inst.signal("actionResult", {{"outcome", "success"}});
    inst.update(150);
    const Json saved = inst.saveState();
    Instance loaded(rig(), parseController("c", controllerData()));
    loaded.restoreState(saved);
    const auto d = loaded.describe()["tracks"];
    check(d[0]["clip"] == "joy" && near(d[0]["timeMs"].get<double>(), 150), "one-shot resumes at saved time");
    check(d[1]["clip"] == "blink" && near(d[1]["timeMs"].get<double>(), 0), "decorative loop restarts at 0");
    check(loaded.takeNotifications().empty(), "restore emits no notifications");
    Json bad = saved; bad["version"] = 9;
    bool threw = false; try { loaded.restoreState(bad); } catch (const std::exception&) { threw = true; }
    check(threw, "unknown state version rejected");
  });

  test("signals derived from transitions", [] {
    const Json before = {{"state", {{"phase", "narrative"}, {"presentation", {{"portraits", Json::array({{{"characterId", "alice"}, {"portraitId", "face"}}})}}}}},
                         {"narrative", {{"cursor", {{"node", "a"}}}, {"node", {{"type", "dialogue"}, {"id", "a"}, {"speakerId", "bob"}}}}}};
    const Json after = {{"state", {{"phase", "narrative"}, {"presentation", {{"portraits", Json::array({{{"characterId", "alice"}, {"portraitId", "happy"}}})}}}}},
                        {"narrative", {{"cursor", {{"node", "b"}}}, {"node", {{"type", "dialogue"}, {"id", "b"}, {"speakerId", "alice"}}}}}};
    const auto s = deriveSignals(before, after, {{"type", "advance"}}, "");
    check(s.size() == 2 && s[0].name == "portraitChanged" && s[0].params.at("portraitId") == "happy" && s[1].name == "dialogueLine" && s[1].params.at("speakerId") == "alice", "portrait + line");
    const auto f = deriveSignals(before, before, {{"type", "executeAction"}, {"actionId", "study"}}, "[RULE] no money");
    check(f.size() == 1 && f[0].params.at("outcome") == "failure", "failed action only reports failure");
    const Json actA = {{"state", {{"lastActivityResult", {{"activityId", "farm"}, {"time", 1}, {"success", {{"passed", false}}}}}}}};
    const auto g = deriveSignals(Json::object(), actA, {{"type", "scheduleStep"}}, "");
    check(g.size() == 2 && g[1].params.at("outcome") == "failure", "activity failure");
    check(deriveSignals(actA, actA, {{"type", "advance"}}, "").empty(), "same result is not repeated");
    Instance inst(rig(), parseController("c", controllerData()));
    inst.signal("dialogueLine", {{"speakerId", "bob"}});
    check(inst.describe()["tracks"].size() == 2, "other speaker does not start talk");
    inst.signal("dialogueLine", {{"speakerId", "alice"}});
    check(inst.describe()["tracks"].size() == 3, "self speaker starts talk track");
    inst.signal("dialogueEnded");
    inst.update(500);
    check(inst.describe()["tracks"].size() == 2, "dialogueEnded stops talk track");
  });

  test("generic extension protocol: init, mount, frame, saveAll", [] {
    Host host;
    const Json docs = Json::array({{{"type", "rig"}, {"id", "hero"}, {"data", rigData()}}, {{"type", "controller"}, {"id", "hero-main"}, {"data", controllerData()}}});
    check(host.handle({{"op", "init"}, {"documents", docs}})["diagnostics"].empty(), "init loads rig and controller");
    // Element 100x150 is half the 200x300 canvas: scale 0.5, rig origin (100,280) -> (50,140).
    const Json element = {{"type", "character"}, {"ref", "hero-main"}, {"width", 100}, {"height", 150}};
    check(host.handle({{"op", "mount"}, {"instance", "title/hero"}, {"element", element}}).value("ok", false), "mount");
    const auto items = host.handle({{"op", "frame"}, {"instance", "title/hero"}})["items"];
    // torso pivot point (40,120) of image sits at body bone (instance 0,-100) => element (50, 140-50)
    const auto m = items[0]["matrix"];
    const double x = m[0].get<double>() * 40 + m[2].get<double>() * 120 + m[4].get<double>();
    const double y = m[1].get<double>() * 40 + m[3].get<double>() * 120 + m[5].get<double>();
    check(near(x, 50) && near(y, 90), "element-local placement with contain fit");
    host.handle({{"op", "observe"}, {"before", Json::object()}, {"after", Json::object()}, {"command", {{"type", "executeAction"}, {"actionId", "study"}}}});
    host.handle({{"op", "update"}, {"dtMs", 120}});
    const Json saved = host.handle({{"op", "saveAll"}})["state"];
    check(saved["instances"]["title/hero"]["state"]["tracks"][0]["clip"] == "joy", "observe routed action success to binding");
    Host other;
    other.handle({{"op", "init"}, {"documents", docs}});
    other.handle({{"op", "mount"}, {"instance", "title/hero"}, {"element", element}});
    const auto restored = other.handle({{"op", "restoreAll"}, {"state", saved}});
    check(restored["skipped"].empty(), "restore into same mount");
    check(other.handle({{"op", "describe"}, {"instance", "title/hero"}})["state"]["tracks"][0]["clip"] == "joy", "restored clip");
    check(!other.request(Json({{"op", "mount"}, {"instance", "x"}, {"element", {{"type", "character"}, {"ref", "missing"}, {"width", 10}, {"height", 10}}}}).dump()).value("ok", true), "unknown controller rejected");
  });

  // ---- Authoring operations (editing.cpp / templates.cpp) ----
  auto worldOf = [](const Json& data, const std::string& bone) { return setupWorld(data).at(bone); };
  auto drawOf = [](const Json& data) {
    Instance inst(std::make_shared<const Rig>(parseRig("e", data)));
    return inst.drawList();
  };
  auto sameDraw = [](const std::vector<DrawItem>& a, const std::vector<DrawItem>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
      const auto &x = a[i].matrix, &y = b[i].matrix;
      if (a[i].slot != b[i].slot || !near(x.a, y.a, 1e-5) || !near(x.b, y.b, 1e-5) || !near(x.c, y.c, 1e-5) || !near(x.d, y.d, 1e-5) || !near(x.e, y.e, 1e-4) || !near(x.f, y.f, 1e-4)) return false;
    }
    return true;
  };
  auto refused = [](const Json& data, const Json& op, const Json& controllers = Json::array()) {
    try { applyEdit(data, op, controllers); } catch (const EditError&) { return true; }
    return false;
  };

  test("edit addBone and reparentBone keep world pose, refuse cycles", [&] {
    auto r = applyEdit(rigData(), {{"op", "addBone"}, {"id", "arm"}, {"parent", "body"}, {"worldX", 130}, {"worldY", 170}});
    check(near(worldOf(r.data, "arm").e, 130) && near(worldOf(r.data, "arm").f, 170), "addBone at world point");
    check(r.affected.size() == 1 && r.affected[0] == "bone:arm", "affected bone");
    auto d = r.data; d["bones"][1]["rotation"] = 30;  // rotated parent makes the compensation non-trivial
    const Affine before = worldOf(d, "arm");
    r = applyEdit(d, {{"op", "reparentBone"}, {"bone", "arm"}, {"parent", "head"}});
    const Affine after = worldOf(r.data, "arm");
    check(near(before.e, after.e, 1e-5) && near(before.f, after.f, 1e-5) && near(before.a, after.a, 1e-5) && near(before.b, after.b, 1e-5), "world pose kept");
    check(r.data["bones"][3]["parent"] == "head" && validateRig(r.data).empty(), "parent changed and valid");
    check(refused(rigData(), {{"op", "reparentBone"}, {"bone", "body"}, {"parent", "head"}}), "descendant parent refused (cycle)");
    check(refused(rigData(), {{"op", "reparentBone"}, {"bone", "body"}, {"parent", "body"}}), "self parent refused");
    check(refused(rigData(), {{"op", "addBone"}, {"id", "body"}}), "duplicate bone id refused");
    r = applyEdit(rigData(), {{"op", "reparentBone"}, {"bone", "head"}, {"parent", nullptr}});
    check(r.data["bones"][2]["parent"].is_null() && near(worldOf(r.data, "head").f, 100), "reparent to root keeps world");
  });

  test("edit setPivot moves origin without visual change", [&] {
    Json d = rigData(); d["bones"][1]["rotation"] = 20;
    const auto drawBefore = drawOf(d);
    const Rig rigBefore = parseRig("a", d);
    auto r = applyEdit(d, {{"op", "setPivot"}, {"bone", "body"}, {"worldX", 110}, {"worldY", 200}});
    check(near(worldOf(r.data, "body").e, 110) && near(worldOf(r.data, "body").f, 200), "bone origin at new pivot");
    check(sameDraw(drawBefore, drawOf(r.data)), "setup draw list unchanged");
    const Rig rigAfter = parseRig("b", r.data);
    bool same = true;
    for (double t : {0.0, 250.0, 500.0, 900.0}) {
      const auto a = samplePose(rigBefore, rigBefore.clipIndex("idle"), t), b = samplePose(rigAfter, rigAfter.clipIndex("idle"), t);
      same &= near(a.world[2].e, b.world[2].e, 1e-4) && near(a.world[2].f, b.world[2].f, 1e-4);
    }
    check(same, "animated child stays in place (keys compensated)");
    check(refused(rigData(), {{"op", "setPivot"}, {"bone", "tail"}, {"localX", 1}}), "unknown bone refused");
  });

  test("edit deleteBone refuses parts unless cascade", [&] {
    check(refused(rigData(), {{"op", "deleteBone"}, {"bone", "head"}}), "bone with slots refused");
    auto r = applyEdit(rigData(), {{"op", "deleteBone"}, {"bone", "head"}, {"cascade", true}});
    check(r.data["slots"].size() == 1 && r.data["attachments"].size() == 1, "slots and attachments cascaded");
    check(r.data["clips"][1]["tracks"].empty() && r.data["clips"][2]["tracks"].size() == 1, "tracks of removed targets dropped");
    Json d = rigData(); d["slots"][0]["bone"] = "root";
    const double headY = worldOf(d, "head").f;
    r = applyEdit(d, {{"op", "deleteBone"}, {"bone", "body"}, {"cascade", false}});
    check(r.data["bones"].size() == 2 && r.data["bones"][1]["parent"] == "root" && near(worldOf(r.data, "head").f, headY), "child re-parented with world pose");
    Json one = Json::parse(R"({"schemaVersion":1,"canvas":{"width":10,"height":10,"originX":0,"originY":0},"bones":[{"id":"root","parent":null}],"slots":[],"attachments":[],"clips":[]})");
    check(refused(one, {{"op", "deleteBone"}, {"bone", "root"}}), "last bone refused");
  });

  test("edit addPart, setSlot, moveSlot", [&] {
    auto r = applyEdit(rigData(), {{"op", "addPart"}, {"slot", "mouth"}, {"bone", "head"}, {"imageAssetId", "img-mouth"}, {"width", 20}, {"height", 8}, {"index", 2}});
    check(r.data["slots"][2]["id"] == "mouth" && r.data["slots"][2]["attachment"] == "mouth", "new slot inserted at index with default attachment");
    r = applyEdit(r.data, {{"op", "addPart"}, {"slot", "mouth"}, {"imageAssetId", "img-mouth-smile"}, {"name", "smile"}, {"width", 20}, {"height", 10}});
    check(r.data["attachments"].size() == 6 && r.extra["attachment"] == "smile", "second attachment on existing slot");
    check(refused(r.data, {{"op", "addPart"}, {"slot", "mouth"}, {"imageAssetId", "x"}, {"name", "smile"}, {"width", 1}, {"height", 1}}), "duplicate attachment name refused");
    check(refused(rigData(), {{"op", "addPart"}, {"slot", "hand"}, {"imageAssetId", "x"}, {"width", 0}, {"height", 1}, {"bone", "body"}}), "zero size refused");
    r = applyEdit(r.data, {{"op", "moveSlot"}, {"slot", "torso"}, {"delta", 10}});
    check(r.data["slots"].back()["id"] == "torso", "moveSlot clamps to front");
    r = applyEdit(r.data, {{"op", "setSlot"}, {"slot", "mouth"}, {"attachment", "smile"}, {"alpha", 0.5}});
    check(r.data["slots"][1]["attachment"] == "smile" && near(r.data["slots"][1]["alpha"].get<double>(), 0.5), "setSlot attachment/alpha");
    check(refused(r.data, {{"op", "setSlot"}, {"slot", "mouth"}, {"attachment", "frown"}}), "unknown attachment refused");
    const auto before = drawOf(rigData());
    r = applyEdit(rigData(), {{"op", "setSlot"}, {"slot", "face"}, {"bone", "root"}});
    check(sameDraw(before, drawOf(r.data)), "moving a slot to another bone keeps placement");
  });

  test("edit keys: setKey, deleteKey, moveKey, setInterp", [&] {
    auto r = applyEdit(rigData(), {{"op", "setKey"}, {"clip", "idle"}, {"target", "bone:body"}, {"property", "y"}, {"timeMs", 250}, {"value", -110}});
    auto keys = r.data["clips"][0]["tracks"][0]["keys"];
    check(keys.size() == 4 && keys[1]["timeMs"] == 250 && keys[1]["value"] == -110, "inserted sorted");
    r = applyEdit(r.data, {{"op", "setKey"}, {"clip", "idle"}, {"target", "bone:body"}, {"property", "y"}, {"timeMs", 250}, {"value", -120}, {"interp", "step"}});
    keys = r.data["clips"][0]["tracks"][0]["keys"];
    check(keys.size() == 4 && keys[1]["value"] == -120 && keys[1]["interp"] == "step", "replaced at same time");
    r = applyEdit(r.data, {{"op", "setKey"}, {"clip", "idle"}, {"target", "bone:head"}, {"property", "rotation"}, {"timeMs", 0}, {"value", 5}});
    check(r.data["clips"][0]["tracks"].size() == 2, "new track created");
    r = applyEdit(r.data, {{"op", "deleteKey"}, {"clip", "idle"}, {"target", "bone:head"}, {"property", "rotation"}, {"timeMs", 0}});
    check(r.data["clips"][0]["tracks"].size() == 1, "empty track removed");
    check(refused(r.data, {{"op", "moveKey"}, {"clip", "idle"}, {"target", "bone:body"}, {"property", "y"}, {"fromMs", 250}, {"toMs", 500}}), "move collision refused");
    check(refused(r.data, {{"op", "moveKey"}, {"clip", "idle"}, {"target", "bone:body"}, {"property", "y"}, {"fromMs", 250}, {"toMs", 1500}}), "move out of range refused");
    r = applyEdit(r.data, {{"op", "moveKey"}, {"clip", "idle"}, {"target", "bone:body"}, {"property", "y"}, {"fromMs", 250}, {"toMs", 750}});
    keys = r.data["clips"][0]["tracks"][0]["keys"];
    check(keys[2]["timeMs"] == 750 && keys[3]["timeMs"] == 1000 && validateRig(r.data).empty(), "moved and re-sorted");
    r = applyEdit(r.data, {{"op", "setInterp"}, {"clip", "idle"}, {"target", "bone:body"}, {"property", "y"}, {"timeMs", 750}, {"interp", "linear"}});
    check(!r.data["clips"][0]["tracks"][0]["keys"][2].contains("interp"), "linear is the default (field removed)");
    check(refused(rigData(), {{"op", "setInterp"}, {"clip", "blink"}, {"target", "slot:eyes"}, {"property", "attachment"}, {"timeMs", 0}, {"interp", "linear"}}), "attachment interp refused");
    r = applyEdit(rigData(), {{"op", "setKey"}, {"clip", "joy"}, {"target", "slot:eyes"}, {"property", "attachment"}, {"timeMs", 100}, {"value", "closed"}});
    check(r.data["clips"][2]["tracks"][2]["keys"][0]["value"] == "closed", "attachment swap key");
    check(refused(rigData(), {{"op", "setKey"}, {"clip", "joy"}, {"target", "slot:eyes"}, {"property", "attachment"}, {"timeMs", 100}, {"value", "wink"}}), "unknown attachment key refused");
    check(refused(rigData(), {{"op", "setKey"}, {"clip", "joy"}, {"target", "bone:body"}, {"property", "alpha"}, {"timeMs", 0}, {"value", 1}}), "slot property on bone refused");
    check(refused(rigData(), {{"op", "setKey"}, {"clip", "joy"}, {"target", "bone:body"}, {"property", "x"}, {"timeMs", 401}, {"value", 1}}), "key past end refused");
  });

  test("edit clips: add, duplicate, rename, set, delete", [&] {
    const Json controllers = Json::array({{{"id", "hero-main"}, {"data", controllerData()}}});
    auto r = applyEdit(rigData(), {{"op", "addClip"}, {"id", "walk"}, {"durationMs", 800}, {"loop", true}});
    check(r.data["clips"].size() == 4 && r.data["clips"][3]["durationMs"] == 800, "addClip");
    r = applyEdit(r.data, {{"op", "duplicateClip"}, {"clip", "joy"}, {"id", "joy-big"}});
    check(r.data["clips"][4]["tracks"].size() == 2 && r.data["clips"][4]["name"] == "joy 복사", "duplicate copies tracks");
    check(refused(r.data, {{"op", "duplicateClip"}, {"clip", "joy"}, {"id", "idle"}}), "duplicate id refused");
    r = applyEdit(rigData(), {{"op", "renameClip"}, {"clip", "joy"}, {"id", "happy"}}, controllers);
    check(r.data["mix"]["pairs"][0]["from"] == "happy", "mix pair renamed");
    check(r.extra["controllerRefs"].size() == 1 && r.extra["controllers"][0]["data"]["bindings"][0]["clip"] == "happy", "controller refs reported and rewritten");
    check(refused(rigData(), {{"op", "deleteClip"}, {"clip", "blink"}}, controllers), "delete of referenced clip refused");
    r = applyEdit(rigData(), {{"op", "deleteClip"}, {"clip", "joy"}});
    check(r.data["clips"].size() == 2 && r.data["mix"]["pairs"].empty(), "delete removes mix pairs");
    check(refused(rigData(), {{"op", "setClip"}, {"clip", "idle"}, {"durationMs", 600}}), "shrinking past keys refused");
    r = applyEdit(rigData(), {{"op", "setClip"}, {"clip", "idle"}, {"durationMs", 600}, {"trim", true}, {"loop", false}});
    check(r.data["clips"][0]["tracks"][0]["keys"].size() == 2 && r.data["clips"][0]["loop"] == false, "trim drops keys past end");
    r = applyEdit(rigData(), {{"op", "setMix"}, {"defaultMs", 50}, {"pairs", Json::array({{{"from", "idle"}, {"to", "joy"}, {"durationMs", 80}}})}});
    check(r.data["mix"]["defaultMs"] == 50 && r.data["mix"]["pairs"][0]["to"] == "joy", "setMix");
    Json bad = {{"id", "bad"}, {"durationMs", 100}, {"loop", true}, {"tracks", Json::array({{{"target", "bone:body"}, {"property", "x"}, {"keys", Json::array({{{"timeMs", 500}, {"value", 1}}})}}})}};
    check(refused(rigData(), {{"op", "addClip"}, {"clip", bad}}), "edit producing an invalid rig is refused");
  });

  test("edit applySetupDeltaToKeys and batch", [&] {
    auto r = applyEdit(rigData(), {{"op", "applySetupDeltaToKeys"}, {"bone", "body"}, {"delta", {{"y", -5}}}});
    check(r.extra["keys"] == 3 && r.data["clips"][0]["tracks"][0]["keys"][1]["value"] == -109, "keys rebased by setup delta");
    r = applyEdit(rigData(), {{"op", "batch"}, {"operations", Json::array({
      {{"op", "setBone"}, {"bone", "body"}, {"y", -90}},
      {{"op", "applySetupDeltaToKeys"}, {"bone", "body"}, {"delta", {{"y", 10}}}, {"clips", Json::array({"idle"})}}})}});
    check(r.data["bones"][1]["y"] == -90 && r.data["clips"][0]["tracks"][0]["keys"][0]["value"] == -90, "batch applies in order");
  });

  test("templates breathing and blink produce valid clips", [&] {
    Json d = rigData();
    const Json breath = breathingClip(d, "breathing", {{"bone", "body"}, {"property", "scaleY"}, {"amplitude", 0.04}, {"periodMs", 3200}});
    const Json& bk = breath["tracks"][0]["keys"];
    check(breath["durationMs"] == 3200 && bk.size() == 3, "three keys over one period (rest, peak, rest)");
    check(bk[1]["timeMs"] == 1600 && near(bk[1]["value"].get<double>(), 1.04) && bk[2]["value"] == 1, "peak at half period, back to base");
    check(bk[0]["interp"] == "bezier" && bk[0]["curve"] == Json::array({0.42, 0, 0.58, 1}) && bk[1]["curve"] == bk[0]["curve"], "easeInOut segments");
    {
      // Eased keys stay close to the sine the old 9 linear keys approximated.
      Json withBreath = d; withBreath["clips"].push_back(breath);
      const Rig br = parseRig("br", withBreath);
      double worst = 0;
      for (int t = 0; t <= 3200; t += 100) {
        const double sine = 1 + 0.04 * (1 - std::cos(2 * 3.14159265358979323846 * t / 3200)) / 2;
        worst = std::max(worst, std::fabs(samplePose(br, br.clipIndex("breathing"), t).bones[1].scaleY - sine));
      }
      check(worst < 0.04 * 0.03, "within 3% of amplitude from a sine");
    }
    auto r = applyEdit(d, {{"op", "addClip"}, {"clip", breath}});
    const Json blink = blinkClip(r.data, "blink-auto", {{"slot", "eyes"}, {"open", "open"}, {"closed", "closed"}, {"intervalMs", 4000}});
    r = applyEdit(r.data, {{"op", "addClip"}, {"clip", blink}});
    check(validateRig(r.data).empty(), "template clips validate");
    Instance inst(std::make_shared<const Rig>(parseRig("t", r.data)));
    inst.play(0, "blink-auto");
    inst.update(3900);
    check(inst.drawList()[2].imageAssetId == "img-eyes-closed", "blink closes near the end");
    inst.update(80);
    check(inst.drawList()[2].imageAssetId == "img-eyes-open", "blink reopens");
    bool threw = false;
    try { blinkClip(d, "b", {{"slot", "eyes"}, {"open", "open"}, {"closed", "shut"}}); } catch (const EditError&) { threw = true; }
    check(threw, "unknown closed attachment refused");
    threw = false;
    try { breathingClip(d, "b", {{"bone", "body"}, {"property", "rotation"}}); } catch (const EditError&) { threw = true; }
    check(threw, "unsupported breathing property refused");
  });

  test("host ops template and edit", [&] {
    Host host;
    const auto t = host.handle({{"op", "template"}, {"kind", "breathing"}, {"rig", rigData()}, {"params", {{"bone", "body"}}}, {"clipId", "breath"}});
    check(t.value("ok", false) && t["clip"]["id"] == "breath" && t["clip"]["tracks"][0]["property"] == "scaleY", "template op");
    const auto e = host.handle({{"op", "edit"}, {"rig", rigData()}, {"operation", {{"op", "addClip"}, {"clip", t["clip"]}}}});
    check(e.value("ok", false) && e["rig"]["clips"].size() == 4 && e["diagnostics"].empty() && e["affected"][0] == "clip:breath", "edit op");
    const auto bad = host.request(Json({{"op", "edit"}, {"rig", rigData()}, {"operation", {{"op", "deleteBone"}, {"bone", "head"}}}}).dump());
    check(!bad.value("ok", true) && !bad.value("error", std::string()).empty(), "refused edit is an error reply");
    const auto rn = host.handle({{"op", "edit"}, {"rig", rigData()}, {"operation", {{"op", "renameClip"}, {"clip", "idle"}, {"id", "rest"}}},
                                 {"controllers", Json::array({{{"id", "c"}, {"data", controllerData()}}})}});
    check(rn["controllers"][0]["data"]["defaultClip"] == "rest", "edit op returns rewritten controllers");
    check(!host.request(Json({{"op", "template"}, {"kind", "wave"}, {"rig", rigData()}, {"clipId", "w"}}).dump()).value("ok", true), "unknown template rejected");
  });

  // ---- Bezier curves (curve.cpp + key interp "bezier") ----
  test("bezier evaluation matches analytic curves", [] {
    // x handles at 1/3 and 2/3 make x(s) = s, so y is the plain cubic: flat = smoothstep.
    const Curve flat = *presetCurve("flat");
    bool ok = true;
    for (int i = 0; i <= 20; i++) { const double u = i / 20.0; ok &= near(evalCurve(flat, u), 3 * u * u - 2 * u * u * u, 1e-12); }
    check(ok, "flat = 3u^2 - 2u^3");
    ok = true;
    for (int i = 0; i <= 20; i++) { const double u = i / 20.0; ok &= near(evalCurve(LINEAR_CURVE, u), u, 1e-12); }
    check(ok, "linear handles are the identity");
    // General handles: choose the Bezier parameter s, evaluate at u = x(s), expect y(s).
    for (const char* name : {"easeIn", "easeOut", "easeInOut", "back"}) {
      const Curve c = *presetCurve(name);
      double worst = 0;
      for (int i = 1; i < 50; i++) { double x, y; curvePoint(c, i / 50.0, x, y); worst = std::max(worst, std::fabs(evalCurve(c, x) - y)); }
      check(worst < 1e-6, std::string(name) + " solves x(s)=u");
    }
    const Curve io = *presetCurve("easeInOut");
    check(near(evalCurve(io, 0.5), 0.5, 1e-9) && near(evalCurve(io, 0.2) + evalCurve(io, 0.8), 1, 1e-6), "easeInOut is symmetric");
    check(evalCurve(*presetCurve("easeIn"), 0.25) < 0.2 && evalCurve(*presetCurve("easeOut"), 0.25) > 0.3, "ease in starts slow, ease out starts fast");
    check(evalCurve(io, 0) == 0 && evalCurve(io, 1) == 1 && evalCurve(io, -3) == 0 && evalCurve(io, 7) == 1, "endpoints exact, outside clamps");
    double peak = 0;
    for (int i = 0; i <= 100; i++) peak = std::max(peak, evalCurve(*presetCurve("back"), i / 100.0));
    check(peak > 1.05 && peak < 1.2, "back overshoots then settles");
    // Out-of-range x handles (invalid data) are clamped: segment time can never run backwards.
    const Curve wild{1.8, 0.2, -0.6, 0.9};
    bool monotone = true;
    double last = -1;
    for (int i = 0; i <= 200; i++) { const double v = evalCurve(wild, i / 200.0); monotone &= v >= last - 1e-9; last = v; }
    check(monotone && clampCurve(wild) == Curve{1, 0.2, 0, 0.9}, "clamped x keeps evaluation monotone");
    check(clampCurve({0.5, 9, 0.5, -9}) == Curve{0.5, 5, 0.5, -4}, "y clamped to -4..5");
    check(presetCurve("easeIn") == Curve{0.42, 0, 1, 1} && presetCurve("easeOut") == Curve{0, 0, 0.58, 1} && !presetCurve("wobble"), "preset table");
  });

  test("bezier keys sample through the runtime, step/linear unchanged", [] {
    const Json d = bezierRig();
    check(validateRig(d).empty(), "bezier fixture validates");
    const Rig r = parseRig("b", d);
    const int ease = r.clipIndex("ease");
    check(r.clips[size_t(ease)].tracks[0].keys[0].interp == Interp::Bezier && r.clips[size_t(ease)].tracks[1].keys[1].interp == Interp::Step, "parsed interp kinds");
    auto at = [&](double t) { return samplePose(r, ease, t); };
    check(near(at(300).bones[1].y, -100 - 12 * evalCurve({0.42, 0, 0.58, 1}, 0.5), 1e-12) && near(at(300).bones[1].y, -106, 1e-9), "easeInOut midpoint");
    check(near(at(800).bones[1].y, -112 + 12 * evalCurve({0, 0, 0.58, 1}, 0.5), 1e-12), "second segment uses its own key's curve");
    double peak = 0;
    for (int t = 0; t <= 400; t += 5) peak = std::max(peak, at(t).bones[2].rotation);
    check(peak > 21 && near(at(400).bones[2].rotation, 20), "back overshoots past 20 and lands on the key");
    check(near(at(550).bones[2].rotation, 20) && near(at(850).bones[2].rotation, -2.5), "step then linear segments unchanged");
    check(near(at(250).bones[1].scaleY, 1 + 0.2 * 0.15625, 1e-12), "flat handles = smoothstep (u=0.25 -> 0.15625)");
    const double alpha = at(500).alpha[1];
    check(near(alpha, 1 - 0.5 * evalCurve({0.9, -0.4, 0.1, 1.4}, 0.5), 1e-12) && near(at(50).alpha[1], 1) && near(at(950).alpha[1], 0.5), "slot alpha bezier; holds outside keys");
    // Existing linear/step fixture keeps its numbers (no bezier anywhere).
    const auto plain = rig();
    check(near(samplePose(*plain, plain->clipIndex("idle"), 250).bones[1].y, -102) && near(samplePose(*plain, plain->clipIndex("joy"), 100).bones[1].scaleY, 1), "linear and step unchanged");
  });

  test("bezier playback is frame-rate independent", [] {
    const auto r = std::make_shared<const Rig>(parseRig("b", bezierRig()));
    auto run = [&](std::vector<double> pattern) {
      Instance inst(r);
      inst.play(0, "ease", false, 1, Then::Hold, 0);
      std::vector<double> samples;
      double t = 0;
      size_t i = 0;
      while (t < 1000) {
        const double step = pattern[i++ % pattern.size()];
        inst.update(step);
        t += step;
        if (std::fmod(t, 125) == 0) { const auto p = inst.pose(); samples.insert(samples.end(), {p.bones[1].y, p.bones[2].rotation, p.bones[1].scaleY, p.alpha[1]}); }
      }
      return samples;
    };
    const auto a = run({31.25}), b = run({7.8125}), c = run({7.8125, 23.4375, 15.625, 15.625});
    check(a.size() == 32 && a == b && b == c, "bit-identical bezier poses at 32/128 fps and uneven frames");
  });

  test("bezier validation codes", [] {
    auto with = [](const Json& key) { Json d = rigData(); d["clips"][2]["tracks"][0]["keys"][0] = key; return validateRig(d); };
    const Json k = {{"timeMs", 0}, {"value", 0}, {"interp", "bezier"}};
    auto curve = [&](const Json& c) { Json x = k; x["curve"] = c; return x; };
    check(hasCode(with(k), "ANIM_CURVE"), "bezier without curve");
    check(hasCode(with(curve({0.1, 0, 0.9})), "ANIM_CURVE"), "three numbers");
    check(hasCode(with(curve({1.2, 0, 0.9, 1})), "ANIM_CURVE") && hasCode(with(curve({0.2, 0, -0.1, 1})), "ANIM_CURVE"), "x outside 0..1");
    check(hasCode(with(curve({0.2, 6, 0.9, 1})), "ANIM_CURVE") && hasCode(with(curve({0.2, 0, 0.9, -4.5})), "ANIM_CURVE"), "y outside -4..5");
    check(hasCode(with(curve({0.2, "a", 0.9, 1})), "ANIM_CURVE"), "non-number handle");
    check(with(curve({0, -2, 1, 3})).empty(), "overshooting y within range is valid");
    Json cubic = k; cubic["interp"] = "cubic";
    check(hasCode(with(cubic), "ANIM_INTERP"), "unknown interp");
    Json d = rigData();
    d["clips"][1]["tracks"][0]["keys"][0]["interp"] = "bezier";
    d["clips"][1]["tracks"][0]["keys"][0]["curve"] = {0.4, 0, 0.6, 1};
    check(hasCode(validateRig(d), "ANIM_INTERP"), "bezier attachment key refused");
    Json stale = {{"timeMs", 0}, {"value", 0}, {"curve", {0.4, 0, 0.6, 1}}};
    const auto ds = with(stale);
    check(ds.size() == 1 && ds[0].code == "ANIM_CURVE_UNUSED" && ds[0].severity == "warning", "curve on a linear key is only a warning");
  });

  test("edit curves: presets, setCurve, auto, batch", [&] {
    const Json base = rigData();
    const Json head = {{"clip", "joy"}, {"target", "bone:head"}, {"property", "rotation"}, {"timeMs", 0}};
    auto op = [&](const char* name, const Json& extra) { Json o = head; o["op"] = name; for (auto it = extra.begin(); it != extra.end(); ++it) o[it.key()] = it.value(); return o; };
    auto key0 = [](const Json& d) { return d["clips"][2]["tracks"][0]["keys"][0]; };
    auto r = applyEdit(base, op("setInterp", {{"interp", "easeIn"}}));
    check(key0(r.data)["interp"] == "bezier" && key0(r.data)["curve"] == Json::array({0.42, 0, 1, 1}), "preset name as interp");
    check(r.affected.size() == 1 && r.affected[0] == "key:joy/bone:head/rotation@0", "affected key");
    r = applyEdit(base, op("setInterp", {{"preset", "back"}}));
    check(key0(r.data)["curve"] == Json::array({0.34, 1.56, 0.64, 1}), "preset field");
    r = applyEdit(base, op("setInterp", {{"interp", "bezier"}, {"curve", {0.25, 0.1, 0.25, 1}}}));
    check(key0(r.data)["curve"] == Json::array({0.25, 0.1, 0.25, 1}), "explicit bezier curve");
    const Json eased = r.data;
    r = applyEdit(eased, op("setInterp", {{"interp", "linear"}}));
    check(!key0(r.data).contains("interp") && !key0(r.data).contains("curve"), "linear drops interp and curve");
    r = applyEdit(eased, op("setInterp", {{"interp", "step"}}));
    check(key0(r.data)["interp"] == "step" && !key0(r.data).contains("curve"), "step drops curve");
    r = applyEdit(base, op("setCurve", {{"curve", {1.4, -7, -0.2, 2}}}));
    check(key0(r.data)["interp"] == "bezier" && key0(r.data)["curve"] == Json::array({1, -4, 0, 2}) && validateRig(r.data).empty(), "setCurve clamps x to 0..1 and y to -4..5");
    check(refused(base, op("setInterp", {{"interp", "bezier"}})), "bezier needs a curve");
    check(refused(base, op("setInterp", {{"interp", "wobble"}})), "unknown preset refused");
    check(refused(base, op("setCurve", {{"curve", {0.1, 0, 0.9}}})), "short curve refused");
    check(refused(base, {{"op", "setCurve"}, {"clip", "blink"}, {"target", "slot:eyes"}, {"property", "attachment"}, {"timeMs", 0}, {"curve", {0.4, 0, 0.6, 1}}}), "attachment curve refused");
    // Value edits and moves keep the key's handles (they are normalized to the segment).
    r = applyEdit(eased, {{"op", "setKey"}, {"clip", "joy"}, {"target", "bone:head"}, {"property", "rotation"}, {"timeMs", 0}, {"value", 4}});
    check(key0(r.data)["value"] == 4 && key0(r.data)["curve"] == Json::array({0.25, 0.1, 0.25, 1}), "setKey keeps curve");
    r = applyEdit(eased, {{"op", "moveKey"}, {"clip", "joy"}, {"target", "bone:head"}, {"property", "rotation"}, {"fromMs", 0}, {"toMs", 50}});
    check(key0(r.data)["timeMs"] == 50 && key0(r.data)["interp"] == "bezier", "moveKey keeps curve");
    r = applyEdit(base, {{"op", "setKey"}, {"clip", "joy"}, {"target", "bone:head"}, {"property", "rotation"}, {"timeMs", 200}, {"value", 30}, {"interp", "easeOut"}});
    check(r.data["clips"][2]["tracks"][0]["keys"][1]["curve"] == Json::array({0, 0, 0.58, 1}), "setKey with a preset");
    // auto: idle y 0:-100, 500:-104, 1000:-100 -> 500 is an extremum -> flat tangents.
    r = applyEdit(base, {{"op", "setInterp"}, {"clip", "idle"}, {"target", "bone:body"}, {"property", "y"}, {"timeMs", 0}, {"interp", "auto"}});
    check(r.data["clips"][0]["tracks"][0]["keys"][0]["curve"] == Json::array({0.333333, 0, 0.666667, 1}), "auto at extremum is flat");
    // Rising keys 0, 10, 30: Catmull-Rom slope at 500 = 0.03/ms vs segment 0.02/ms -> y2 = 1 - 1.5/3.
    r = applyEdit(base, {{"op", "batch"}, {"operations", Json::array({
      {{"op", "setKey"}, {"clip", "idle"}, {"target", "bone:body"}, {"property", "y"}, {"timeMs", 0}, {"value", 0}},
      {{"op", "setKey"}, {"clip", "idle"}, {"target", "bone:body"}, {"property", "y"}, {"timeMs", 500}, {"value", 10}},
      {{"op", "setKey"}, {"clip", "idle"}, {"target", "bone:body"}, {"property", "y"}, {"timeMs", 1000}, {"value", 30}},
      {{"op", "setInterp"}, {"clip", "idle"}, {"target", "bone:body"}, {"property", "y"}, {"timeMs", 0}, {"interp", "auto"}},
      {{"op", "setInterp"}, {"clip", "idle"}, {"target", "bone:body"}, {"property", "y"}, {"timeMs", 500}, {"interp", "auto"}}})}});
    const Json& yk = r.data["clips"][0]["tracks"][0]["keys"];
    check(yk[0]["curve"] == Json::array({0.333333, 0, 0.666667, 0.5}) && yk[1]["curve"] == Json::array({0.333333, 0.25, 0.666667, 1}), "auto Catmull-Rom handles, flat at ends");
    const Rig smooth = parseRig("s", r.data);
    bool rising = true;
    double prev = -1;
    for (int t = 0; t < 1000; t += 10) { const double v = samplePose(smooth, 0, t).bones[1].y; rising &= v >= prev; prev = v; }  // idle loops: 1000 wraps to 0
    check(rising, "auto never overshoots between rising keys");
    // Many keys in one batch = one edit.
    r = applyEdit(base, {{"op", "batch"}, {"operations", Json::array({op("setInterp", {{"interp", "easeOut"}}), {{"op", "setInterp"}, {"clip", "joy"}, {"target", "bone:body"}, {"property", "scaleY"}, {"timeMs", 0}, {"interp", "easeOut"}}})}});
    check(key0(r.data)["interp"] == "bezier" && r.data["clips"][2]["tracks"][1]["keys"][0]["curve"] == Json::array({0, 0, 0.58, 1}) && r.affected.size() == 2, "batch applies a preset to many keys");
    // setPivot compensation samples bezier rotation keys like the runtime does.
    Json pivoted = bezierRig();
    // Both position axes keyed at 200 ms so setPivot compensates the head origin exactly there.
    pivoted["clips"][3]["tracks"].push_back({{"target", "bone:head"}, {"property", "x"}, {"keys", Json::array({{{"timeMs", 0}, {"value", 0}}, {{"timeMs", 200}, {"value", 0}}})}});
    pivoted["clips"][3]["tracks"].push_back({{"target", "bone:head"}, {"property", "y"}, {"keys", Json::array({{{"timeMs", 0}, {"value", -80}}, {{"timeMs", 200}, {"value", -80}}})}});
    const Rig before = parseRig("a", pivoted);
    r = applyEdit(pivoted, {{"op", "setPivot"}, {"bone", "head"}, {"localX", 10}, {"localY", 0}});
    const Rig after = parseRig("b", r.data);
    const auto pa = samplePose(before, before.clipIndex("ease"), 200), pb = samplePose(after, after.clipIndex("ease"), 200);
    check(near(pb.world[2].e, pa.world[2].e + pa.world[2].a * 10, 1e-4) && near(pb.world[2].f, pa.world[2].f + pa.world[2].b * 10, 1e-4), "pivot key compensation at a bezier key time");
  });

  // ---- Key selections (dopesheet multi-key gestures) ----
  auto ref = [](const char* target, const char* property, int t) { return Json{{"target", target}, {"property", property}, {"timeMs", t}}; };
  auto easeKeys = [](const Json& d, size_t track) { return d["clips"][3]["tracks"][track]["keys"]; };
  auto trackOf = [](const Json& d, const char* clip, const char* target, const char* property) {
    for (const auto& c : d["clips"]) if (c["id"] == clip) for (const auto& t : c["tracks"]) if (t["target"] == target && t["property"] == property) return t["keys"];
    return Json();
  };
  test("edit moveKeys keeps spacing and curves, refuses collisions and range", [&] {
    const Json base = bezierRig();  // clip 3 "ease": body y 0/600/1000, head rotation 0/400/700/1000, face alpha 100/900
    const Json sel = Json::array({ref("bone:body", "y", 600), ref("bone:head", "rotation", 400), ref("bone:head", "rotation", 700)});
    auto r = applyEdit(base, {{"op", "moveKeys"}, {"clip", "ease"}, {"keys", sel}, {"deltaMs", 50}});
    check(easeKeys(r.data, 0)[1]["timeMs"] == 650 && easeKeys(r.data, 0)[1]["curve"] == Json::array({0, 0, 0.58, 1}), "bezier key moves with its curve");
    check(easeKeys(r.data, 1)[1]["timeMs"] == 450 && easeKeys(r.data, 1)[1]["interp"] == "step" && easeKeys(r.data, 1)[2]["timeMs"] == 750, "relative spacing kept");
    check(easeKeys(r.data, 1)[0]["timeMs"] == 0 && easeKeys(r.data, 1)[3]["timeMs"] == 1000, "unselected keys stay");
    check(r.extra["keys"].size() == 3 && r.extra["keys"][0] == ref("bone:body", "y", 650) && validateRig(r.data).empty(), "extra.keys = moved keys, still valid");
    check(refused(base, {{"op", "moveKeys"}, {"clip", "ease"}, {"keys", Json::array({ref("bone:head", "rotation", 400)})}, {"deltaMs", 300}}), "collision with an unselected key refused");
    check(refused(base, {{"op", "moveKeys"}, {"clip", "ease"}, {"keys", sel}, {"deltaMs", 400}}), "out of range refused (no partial move)");
    check(refused(base, {{"op", "moveKeys"}, {"clip", "ease"}, {"keys", Json::array({ref("bone:head", "rotation", 401)})}, {"deltaMs", 10}}), "unknown key refused");
    // Selected keys landing on each other's old times is fine (all move together).
    r = applyEdit(base, {{"op", "moveKeys"}, {"clip", "ease"}, {"keys", Json::array({ref("bone:head", "rotation", 400), ref("bone:head", "rotation", 700)})}, {"deltaMs", -300}});
    check(easeKeys(r.data, 1)[1]["timeMs"] == 100 && easeKeys(r.data, 1)[2]["timeMs"] == 400 && easeKeys(r.data, 1)[2]["value"] == 5, "group lands on its own old time");
    // clamp stops the group at the clip edge.
    r = applyEdit(base, {{"op", "moveKeys"}, {"clip", "ease"}, {"keys", Json::array({ref("slot:face", "alpha", 100), ref("slot:face", "alpha", 900)})}, {"deltaMs", 500}, {"clamp", true}});
    check(r.extra["deltaMs"] == 100 && easeKeys(r.data, 3)[0]["timeMs"] == 200 && easeKeys(r.data, 3)[1]["timeMs"] == 1000, "clamp keeps spacing at the end");
    check(easeKeys(r.data, 3)[0]["curve"] == Json::array({0.9, -0.4, 0.1, 1.4}), "overshoot curve kept");
    r = applyEdit(base, {{"op", "moveKeys"}, {"clip", "ease"}, {"keys", Json::array({ref("bone:body", "y", 0)})}, {"deltaMs", -40}, {"clamp", true}});
    check(r.data == base && r.extra["deltaMs"] == 0, "clamped to zero = no change");
  });

  test("edit scaleKeys and deleteKeys", [&] {
    const Json base = bezierRig();
    const Json rot = Json::array({ref("bone:head", "rotation", 0), ref("bone:head", "rotation", 400), ref("bone:head", "rotation", 700)});
    auto r = applyEdit(base, {{"op", "scaleKeys"}, {"clip", "ease"}, {"keys", rot}, {"factor", 0.5}});
    check(easeKeys(r.data, 1)[1]["timeMs"] == 200 && easeKeys(r.data, 1)[2]["timeMs"] == 350 && easeKeys(r.data, 1)[3]["timeMs"] == 1000, "scaled around the first key");
    bool xValid = true;
    for (const auto& k : easeKeys(r.data, 1)) if (k.contains("curve")) for (size_t i : {0, 2}) xValid &= k["curve"][i].get<double>() >= 0 && k["curve"][i].get<double>() <= 1;
    check(easeKeys(r.data, 1)[0]["curve"] == Json::array({0.34, 1.56, 0.64, 1}) && xValid && validateRig(r.data).empty(), "curves preserved, x stays in 0..1");
    // Normalized handles: the shape follows the new timing (half the time = same fraction).
    const Rig before = parseRig("a", base), after = parseRig("b", r.data);
    check(near(samplePose(after, 3, 100).bones[2].rotation, samplePose(before, 3, 200).bones[2].rotation, 1e-9), "scaled segment samples the same fraction");
    r = applyEdit(base, {{"op", "scaleKeys"}, {"clip", "ease"}, {"keys", Json::array({ref("bone:head", "rotation", 400), ref("bone:head", "rotation", 700)})}, {"factor", 0.5}, {"pivotMs", 1000}});
    check(easeKeys(r.data, 1)[1]["timeMs"] == 700 && easeKeys(r.data, 1)[1]["value"] == 20 && easeKeys(r.data, 1)[2]["timeMs"] == 850, "pivot at the playhead, lands on a selected key's old time");
    check(refused(base, {{"op", "scaleKeys"}, {"clip", "ease"}, {"keys", Json::array({ref("bone:head", "rotation", 700), ref("bone:head", "rotation", 1000)})}, {"factor", 0.001}}), "keys merging refused");
    check(refused(base, {{"op", "scaleKeys"}, {"clip", "ease"}, {"keys", Json::array({ref("bone:head", "rotation", 700)})}, {"factor", 0.0001}, {"pivotMs", 1000}}), "landing on an unselected key refused");
    check(refused(base, {{"op", "scaleKeys"}, {"clip", "ease"}, {"keys", rot}, {"factor", 2}}), "scaling past the end refused");
    check(refused(base, {{"op", "scaleKeys"}, {"clip", "ease"}, {"keys", rot}, {"factor", -1}}), "negative factor refused");
    r = applyEdit(base, {{"op", "deleteKeys"}, {"clip", "ease"}, {"keys", Json::array({ref("slot:face", "alpha", 100), ref("slot:face", "alpha", 900), ref("bone:body", "y", 600)})}});
    check(r.data["clips"][3]["tracks"].size() == 3 && easeKeys(r.data, 0).size() == 2 && r.affected.size() == 3, "deleteKeys removes keys and empty tracks");
  });

  test("edit copyKeys / pasteKeys across clips", [&] {
    const Json base = bezierRig();
    auto r = applyEdit(base, {{"op", "copyKeys"}, {"clip", "ease"}, {"keys", Json::array({ref("bone:head", "rotation", 400), ref("bone:head", "rotation", 700), ref("bone:body", "y", 600)})}});
    const Json board = r.extra["clipboard"];
    check(r.data == base && board["fromMs"] == 400 && board["keys"].size() == 3, "copy changes nothing, offsets from the earliest key");
    check(board["keys"][2] == Json({{"target", "bone:body"}, {"property", "y"}, {"offsetMs", 200}, {"value", -112}, {"interp", "bezier"}, {"curve", {0, 0, 0.58, 1}}}), "entry carries value, interp and curve");
    // Paste into another clip at 0: overwrites joy's rotation key at 0 and keeps the curve.
    r = applyEdit(base, {{"op", "pasteKeys"}, {"clip", "joy"}, {"timeMs", 0}, {"keys", board["keys"]}});
    const Json rotKeys = trackOf(r.data, "joy", "bone:head", "rotation");
    check(rotKeys.size() == 3 && rotKeys[0]["value"] == 20 && rotKeys[0]["interp"] == "step" && rotKeys[1]["timeMs"] == 300 && rotKeys[1]["value"] == 5, "overwrite same time + insert");
    check(trackOf(r.data, "joy", "bone:body", "y")[0]["timeMs"] == 200 && trackOf(r.data, "joy", "bone:body", "y")[0]["curve"] == Json::array({0, 0, 0.58, 1}), "new track with the bezier curve");
    check(r.extra["keys"].size() == 3 && validateRig(r.data).empty(), "pasted keys reported");
    check(refused(base, {{"op", "pasteKeys"}, {"clip", "joy"}, {"timeMs", 200}, {"keys", board["keys"]}}), "paste past the clip end refused");
    check(refused(base, {{"op", "pasteKeys"}, {"clip", "joy"}, {"timeMs", 0}, {"keys", board["keys"]}, {"target", "bone:root"}}), "multi-target clipboard cannot retarget");
    // Single-row copy pasted onto another bone.
    const Json row = applyEdit(base, {{"op", "copyKeys"}, {"clip", "ease"}, {"keys", Json::array({ref("bone:head", "rotation", 0), ref("bone:head", "rotation", 400)})}}).extra["clipboard"]["keys"];
    r = applyEdit(base, {{"op", "pasteKeys"}, {"clip", "ease"}, {"timeMs", 500}, {"keys", row}, {"target", "bone:body"}});
    const Json bodyRot = trackOf(r.data, "ease", "bone:body", "rotation");
    check(bodyRot.size() == 2 && bodyRot[0]["timeMs"] == 500 && bodyRot[0]["curve"] == Json::array({0.34, 1.56, 0.64, 1}) && bodyRot[1]["timeMs"] == 900, "row retargeted");
    // Attachment keys: value names are checked on the target slot.
    const Json blink = applyEdit(base, {{"op", "copyKeys"}, {"clip", "blink"}, {"keys", Json::array({ref("slot:eyes", "attachment", 1800), ref("slot:eyes", "attachment", 1900)})}}).extra["clipboard"]["keys"];
    check(!blink[0].contains("interp"), "attachment entries have no interp");
    r = applyEdit(base, {{"op", "pasteKeys"}, {"clip", "joy"}, {"timeMs", 100}, {"keys", blink}});
    check(trackOf(r.data, "joy", "slot:eyes", "attachment")[1] == Json({{"timeMs", 200}, {"value", "open"}}), "attachment keys pasted");
    check(refused(base, {{"op", "pasteKeys"}, {"clip", "joy"}, {"timeMs", 100}, {"keys", blink}, {"target", "slot:face"}}), "unknown attachment on the new slot refused");
    check(refused(base, {{"op", "pasteKeys"}, {"clip", "joy"}, {"timeMs", 100}, {"keys", row}, {"target", "slot:face"}}), "bone property onto a slot refused");
    // The LLM path (host op `edit`, plugin_call) returns the same clipboard and the same paste.
    Host host;
    const Json copyOp = {{"op", "copyKeys"}, {"clip", "ease"}, {"keys", Json::array({ref("bone:head", "rotation", 0), ref("bone:head", "rotation", 400)})}};
    const Json copied = host.handle({{"op", "edit"}, {"rig", base}, {"operation", copyOp}});
    check(copied.value("ok", false) && copied["clipboard"]["keys"] == row && copied["rig"] == base, "host edit returns the clipboard");
    const Json pasteOp = {{"op", "pasteKeys"}, {"clip", "ease"}, {"timeMs", 500}, {"keys", row}, {"target", "bone:body"}};
    const Json pastedReply = host.handle({{"op", "edit"}, {"rig", base}, {"operation", pasteOp}});
    check(pastedReply["rig"] == applyEdit(base, pasteOp).data && pastedReply["keys"].size() == 2, "host paste = editor paste");
  });

  test("edit copyPose / pastePose", [&] {
    Json base = bezierRig();
    // joy's rotation key at 0 is eased: pasting a pose there must keep its interpolation.
    base = applyEdit(base, {{"op", "setInterp"}, {"clip", "joy"}, {"target", "bone:head"}, {"property", "rotation"}, {"timeMs", 0}, {"interp", "easeIn"}}).data;
    auto r = applyEdit(base, {{"op", "copyPose"}, {"clip", "ease"}, {"timeMs", 300}, {"bones", {"body", "head"}}});
    const Json pose = r.extra["pose"];
    const Rig parsed = parseRig("p", base);
    const auto sampled = samplePose(parsed, 3, 300);
    check(r.data == base && pose.size() == 2 && pose[0]["bone"] == "body", "copyPose changes nothing");
    check(near(pose[0]["y"].get<double>(), sampled.bones[1].y, 1e-6) && near(pose[1]["rotation"].get<double>(), sampled.bones[2].rotation, 1e-6), "sampled like the runtime");
    check(pose[0]["x"] == 0 && pose[0]["scaleX"] == 1, "unanimated channels = setup");
    check(applyEdit(base, {{"op", "copyPose"}}).extra["pose"].size() == 3, "no clip = setup pose of every bone");
    r = applyEdit(base, {{"op", "pastePose"}, {"clip", "joy"}, {"timeMs", 0}, {"pose", pose}});
    auto joyKey = [&](const Json& d, const char* target, const char* property, int t) {
      for (const auto& k : trackOf(d, "joy", target, property)) if (k["timeMs"] == t) return k;
      return Json();
    };
    check(near(joyKey(r.data, "bone:head", "rotation", 0)["value"].get<double>(), pose[1]["rotation"].get<double>()) && joyKey(r.data, "bone:head", "rotation", 0)["interp"] == "bezier", "overwrites value, keeps interp");
    check(!joyKey(r.data, "bone:body", "y", 0).is_null() && joyKey(r.data, "bone:body", "x", 0).is_null(), "setup-valued unanimated channels skipped");
    check(!joyKey(r.data, "bone:body", "scaleY", 0).is_null(), "animated channel keyed");
    check(validateRig(r.data).empty() && r.extra["keys"].size() == 3, "pose paste reports its keys (body y, body scaleY, head rotation)");
    check(refused(base, {{"op", "pastePose"}, {"clip", "joy"}, {"timeMs", 500}, {"pose", pose}}), "pose past the end refused");
  });

  test("host sample op evaluates bezier like samplePose", [] {
    Host host;
    const auto reply = host.handle({{"op", "sample"}, {"rig", bezierRig()}, {"clip", "ease"}, {"timeMs", 300}});
    const Rig r = parseRig("b", bezierRig());
    check(reply.value("ok", false) && reply["pose"]["bones"]["body"]["y"].get<double>() == samplePose(r, r.clipIndex("ease"), 300).bones[1].y, "same number through the JSON host");
  });

  std::cout << (checks - failures) << "/" << checks << " checks passed\n";
  return failures ? 1 : 0;
}
