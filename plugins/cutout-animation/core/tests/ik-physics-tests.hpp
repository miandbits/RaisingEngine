// IK chains + stretch and physics constraints (plugin 1.13.0), included by core-tests.cpp after its
// check() / test() helpers. Also the --physics-parity script (tests/plugins/cutout-physics-parity.test.mjs).
#pragma once
#include <cstring>

namespace ikphys {
using cutout::Json;
constexpr double PI_ = 3.14159265358979323846;

/** root (100, 100) -> b0 -> b1 -> b2 -> b3, each 40 px long on its parent's +x; target `t` under the root. */
Json chainRig(double tx, double ty) {
  Json d = Json::parse(R"({"schemaVersion": 1, "canvas": {"width": 400, "height": 400, "originX": 0, "originY": 0},
    "bones": [
      {"id": "root", "parent": null, "x": 100, "y": 100},
      {"id": "b0", "parent": "root", "x": 0, "y": 0, "length": 40},
      {"id": "b1", "parent": "b0", "x": 40, "y": 0, "length": 40},
      {"id": "b2", "parent": "b1", "x": 40, "y": 0, "length": 40},
      {"id": "b3", "parent": "b2", "x": 40, "y": 0, "length": 40},
      {"id": "t", "parent": "root", "x": 0, "y": 0}],
    "slots": [{"id": "s2", "bone": "b2", "attachment": "seg"}],
    "attachments": [{"slot": "s2", "name": "seg", "imageAssetId": "img-seg", "width": 40, "height": 10, "pivotX": 0, "pivotY": 0.5}],
    "clips": [],
    "ik": [{"id": "tail", "bones": ["b0", "b1", "b2", "b3"], "target": "t"}]})");
  d["bones"][5]["x"] = tx - 100; d["bones"][5]["y"] = ty - 100;  // (tx, ty) in rig space
  return d;
}
/** Rig-space tip of bone `b` (its length along +x). */
std::pair<double, double> tipOf(const cutout::Rig& r, const cutout::Pose& p, const std::string& id) {
  const int b = r.boneIndex(id);
  double x, y;
  p.world[size_t(b)].apply(r.bones[size_t(b)].length, 0, x, y);
  return {x, y};
}
double distance(std::pair<double, double> a, double x, double y) { return std::hypot(a.first - x, a.second - y); }

/**
 * Character head with three hair segments (rotate physics, the hair preset), a ribbon that bobs
 * (x/y + scaleX) and a swaying head clip; `jump` teleports the root. The physics determinism fixture.
 */
Json hairRig() {
  return Json::parse(R"({"schemaVersion": 1, "canvas": {"width": 400, "height": 400, "originX": 0, "originY": 0},
    "bones": [
      {"id": "root", "parent": null, "x": 200, "y": 100},
      {"id": "head", "parent": "root", "x": 0, "y": 0, "length": 30},
      {"id": "hair0", "parent": "head", "x": 0, "y": 20, "rotation": 80, "length": 30},
      {"id": "hair1", "parent": "hair0", "x": 30, "y": 0, "length": 30},
      {"id": "hair2", "parent": "hair1", "x": 30, "y": 0, "length": 30},
      {"id": "ribbon", "parent": "head", "x": -15, "y": -10, "rotation": 20, "length": 20}],
    "slots": [
      {"id": "h0", "bone": "hair0", "attachment": "strand"}, {"id": "h1", "bone": "hair1", "attachment": "strand"},
      {"id": "h2", "bone": "hair2", "attachment": "strand"}, {"id": "rb", "bone": "ribbon", "attachment": "bow"}],
    "attachments": [
      {"slot": "h0", "name": "strand", "imageAssetId": "img-strand", "width": 30, "height": 8, "pivotX": 0, "pivotY": 0.5},
      {"slot": "h1", "name": "strand", "imageAssetId": "img-strand", "width": 30, "height": 8, "pivotX": 0, "pivotY": 0.5},
      {"slot": "h2", "name": "strand", "imageAssetId": "img-strand", "width": 30, "height": 8, "pivotX": 0, "pivotY": 0.5},
      {"slot": "rb", "name": "bow", "imageAssetId": "img-bow", "width": 20, "height": 14, "pivotX": 0, "pivotY": 0.5}],
    "clips": [
      {"id": "sway", "durationMs": 1000, "loop": true, "tracks": [
        {"target": "bone:head", "property": "x", "keys": [{"timeMs": 0, "value": 0}, {"timeMs": 500, "value": 60, "interp": "bezier", "curve": [0.42, 0, 0.58, 1]}, {"timeMs": 1000, "value": 0}]},
        {"target": "bone:head", "property": "rotation", "keys": [{"timeMs": 0, "value": -10}, {"timeMs": 300, "value": 15}, {"timeMs": 1000, "value": -10}]}]},
      {"id": "still", "durationMs": 500, "loop": true, "tracks": []},
      {"id": "jump", "durationMs": 1000, "loop": false, "tracks": [
        {"target": "bone:root", "property": "x", "keys": [{"timeMs": 0, "value": 200, "interp": "step"}, {"timeMs": 400, "value": 1200}]}]}],
    "physics": [
      {"id": "hair0-physics", "bone": "hair0", "rotate": 1, "inertia": 0.7, "strength": 60, "damping": 0.8, "gravity": 400},
      {"id": "hair1-physics", "bone": "hair1", "rotate": 1, "inertia": 0.7, "strength": 60, "damping": 0.8, "gravity": 400, "wind": 50},
      {"id": "hair2-physics", "bone": "hair2", "rotate": 1, "inertia": 0.7, "strength": 60, "damping": 0.8, "gravity": 400, "step": 0.008333333333333333},
      {"id": "ribbon-physics", "bone": "ribbon", "x": 0.6, "y": 0.6, "scaleX": 0.5, "inertia": 0.9, "strength": 50, "damping": 0.92, "mass": 0.8, "gravity": 300}]})");
}
std::shared_ptr<const cutout::Rig> parsed(const Json& d) { return std::make_shared<const cutout::Rig>(cutout::parseRig("r", d)); }
/** Exact bits of everything an instance draws plus its world matrices. */
std::string bitsOf(const cutout::Instance& inst) {
  std::string out;
  auto put = [&](double v) { uint64_t b; std::memcpy(&b, &v, sizeof b); out += std::to_string(b) + ","; };
  for (const auto& w : inst.pose().world) for (double v : {w.a, w.b, w.c, w.d, w.e, w.f}) put(v);
  for (const auto& item : inst.drawList()) for (double v : {item.matrix.a, item.matrix.b, item.matrix.c, item.matrix.d, item.matrix.e, item.matrix.f}) put(v);
  return out;
}
/** Run `dts` (cut so the partial sums meet every checkpoint) and record the bits at each checkpoint. */
std::vector<std::string> runPartition(const Json& rig, const std::vector<double>& checkpoints, const std::function<double(int)>& dtAt) {
  cutout::Instance inst(parsed(rig));
  inst.play(0, "sway");
  std::vector<std::string> out;
  double now = 0;
  int i = 0;
  for (double cp : checkpoints) {
    // Fixed-rate partitions land on checkpoints within float noise; irregular ones are cut there.
    while (cp - now > 1e-7) {
      double dt = dtAt(i++);
      if (now + dt > cp + 1e-7) dt = cp - now;
      inst.update(dt);
      now += dt;
    }
    out.push_back(bitsOf(inst));
  }
  return out;
}

/** The --physics-parity script: hair rig + a stretching IK chain through init/mount/irregular updates/frames. */
Json parityRig() {
  Json d = hairRig();
  Json chain = chainRig(150, 40);
  // A tail chain on the root (FABRIK, stretch), its target keyed far and near.
  for (size_t i = 1; i < chain["bones"].size(); i++) {
    Json b = chain["bones"][i];
    b["id"] = "tail-" + b["id"].get<std::string>();
    if (b["parent"] == "root") b["parent"] = "root"; else b["parent"] = "tail-" + b["parent"].get<std::string>();
    d["bones"].push_back(b);
  }
  d["ik"] = Json::array({{{"id", "tail"}, {"bones", {"tail-b0", "tail-b1", "tail-b2", "tail-b3"}}, {"target", "tail-t"}, {"stretch", true}}});
  d["slots"].push_back({{"id", "tailseg"}, {"bone", "tail-b2"}, {"attachment", "seg"}});
  d["attachments"].push_back({{"slot", "tailseg"}, {"name", "seg"}, {"imageAssetId", "img-seg"}, {"width", 40}, {"height", 10}, {"pivotX", 0}, {"pivotY", 0.5}});
  d["clips"][0]["tracks"].push_back(Json::parse(R"({"target": "bone:tail-t", "property": "x", "keys": [{"timeMs": 0, "value": 90}, {"timeMs": 500, "value": 230}, {"timeMs": 1000, "value": 90}]})"));
  d["clips"][0]["tracks"].push_back(Json::parse(R"({"target": "bone:tail-t", "property": "y", "keys": [{"timeMs": 0, "value": 70}, {"timeMs": 500, "value": -20}, {"timeMs": 1000, "value": 70}]})"));
  d["clips"][0]["tracks"].push_back(Json::parse(R"({"target": "physics:ribbon-physics", "property": "mix", "keys": [{"timeMs": 0, "value": 1}, {"timeMs": 700, "value": 0.3}, {"timeMs": 1000, "value": 1}]})"));
  return d;
}
Json parityScript() {
  const Json controller = {{"schemaVersion", 1}, {"rig", "hair"}, {"defaultClip", "sway"}};
  Json script = Json::array({{{"op", "init"}, {"frameFormats", {2}}, {"documents", Json::array({{{"type", "rig"}, {"id", "hair"}, {"data", parityRig()}}, {{"type", "controller"}, {"id", "hair-main"}, {"data", controller}}})}}});
  for (const char* id : {"a", "b"})
    script.push_back({{"op", "mount"}, {"instance", id}, {"element", {{"type", "character"}, {"ref", "hair-main"}, {"width", 400}, {"height", 400}, {"props", {{"scale", 1}}}}}});
  // Instance a at 60 Hz, b at irregular pacing: their frames must be the same at the shared times.
  const double irregular[] = {7.3, 11.1, 3.9, 20.0, 0.25, 33.0, 5.45, 19.0, 0.0, 17.0, 16.0, 30.0};
  double ta = 0, tb = 0;
  int ib = 0;
  for (int checkpoint = 1; checkpoint <= 6; checkpoint++) {
    const double cp = 250.0 * checkpoint;
    while (cp - ta > 1e-7) { const double dt = std::min(1000.0 / 60, cp - ta); script.push_back({{"op", "update"}, {"instance", "a"}, {"dtMs", dt}}); ta += dt; }
    while (cp - tb > 1e-7) { const double dt = std::min(irregular[ib++ % 12], cp - tb); script.push_back({{"op", "update"}, {"instance", "b"}, {"dtMs", dt}}); tb += dt; }
    for (const char* id : {"a", "b"}) {
      script.push_back({{"op", "frame"}, {"instance", id}});
      script.push_back({{"op", "frame"}, {"instance", id}, {"format", 2}});
    }
  }
  script.push_back({{"op", "setPhysicsEnvironment"}, {"wind", 120}, {"gravity", -50}});
  for (int k = 0; k < 20; k++) script.push_back({{"op", "update"}, {"dtMs", 12.5}});
  script.push_back({{"op", "frame"}, {"instance", "a"}});
  script.push_back({{"op", "describeAll"}});
  for (double ms : {0.0, 250.0, 650.0}) script.push_back({{"op", "sample"}, {"rig", parityRig()}, {"clip", "sway"}, {"timeMs", ms}});
  return script;
}

void run() {
  using namespace cutout;
  // ---------------------------------------------------------------- IK stretch
  test("ik stretch: aim bone reaches, compress, uniform, mix, no-op rules", [] {
    Json d = chainRig(180, 100);  // b0 alone aims: 80 px away, 40 px long
    d["ik"][0]["bones"] = {"b0"};
    d["ik"][0]["stretch"] = true;
    Rig r = parseRig("a", d);
    Pose p = samplePose(r, -1, 0);
    check(distance(tipOf(r, p, "b0"), 180, 100) < 1e-9 && near(p.local(1).scaleX, 2, 1e-12) && p.bones[1].scaleX == 1, "stretch: the tip lands on the target, scaleX 2 (animated value untouched)");
    check(near(p.ikSolve[0].stretch, 2, 1e-12) && p.ikSolve[0].solved, "ikSolve reports the factor");
    d["ik"][0]["mix"] = 0.5;
    p = samplePose(parseRig("a", d), -1, 0);
    check(near(p.local(1).scaleX, 1.5, 1e-12), "mix 0.5: half the stretch");
    d["ik"][0]["mix"] = 1;
    d["ik"][0]["uniform"] = true;
    p = samplePose(parseRig("a", d), -1, 0);
    check(near(p.local(1).scaleY, 2, 1e-12), "uniform: scaleY too");
    Json near20 = chainRig(120, 100);
    near20["ik"][0]["bones"] = {"b0"};
    near20["ik"][0]["stretch"] = true;
    p = samplePose(parseRig("a", near20), -1, 0);
    check(p.local(1).scaleX == 1, "stretch alone never shrinks");
    near20["ik"][0]["compress"] = true;
    r = parseRig("a", near20);
    p = samplePose(r, -1, 0);
    check(near(p.local(1).scaleX, 0.5, 1e-12) && distance(tipOf(r, p, "b0"), 120, 100) < 1e-9, "compress: a near target shrinks the bone onto it");
    // Exact under a rotated, non-uniformly scaled, mirrored parent (parent space reasoning).
    Json skew = d;
    skew["ik"][0].erase("uniform");
    skew["bones"][0]["rotation"] = 25; skew["bones"][0]["scaleX"] = -1.4; skew["bones"][0]["scaleY"] = 0.7;
    r = parseRig("a", skew);
    p = samplePose(r, -1, 0);
    check(distance(tipOf(r, p, "b0"), p.world[5].e, p.world[5].f) < 1e-9, "stretch exact under a skewing, mirrored parent");
    // No-ops: a bone without length, an oblique aim axis.
    Json noLength = d;
    noLength["bones"][1].erase("length");
    check(hasCode(validateRig(noLength), "ANIM_IK_STRETCH_LENGTH") && samplePose(parseRig("a", noLength), -1, 0).local(1).scaleX == 1, "no length: warned, no stretch");
    Json axis = d;
    axis["ik"][0]["aimAxis"] = 30;
    check(hasCode(validateRig(axis), "ANIM_IK_STRETCH_AXIS") && samplePose(parseRig("a", axis), -1, 0).local(1).scaleX == 1, "aim axis: warned, no stretch");
    Json two = chainRig(100, 100);
    two["ik"][0]["bones"] = {"b0", "b1"};
    two["ik"][0]["compress"] = true;
    check(hasCode(validateRig(two), "ANIM_IK_COMPRESS"), "compress on two bones: warned");
    two["ik"][0].erase("compress");
    two["ik"][0]["uniform"] = true;
    check(hasCode(validateRig(two), "ANIM_IK_UNIFORM"), "uniform without stretch: warned");
  });

  test("ik stretch: two-bone limb reaches an unreachable target exactly", [] {
    Json d = chainRig(240, 100);  // [b0, b1]: 80 px of reach, the target 140 px away
    d["ik"][0]["bones"] = {"b0", "b1"};
    Rig r = parseRig("a", d);
    Pose p = samplePose(r, -1, 0);
    check(near(distance(tipOf(r, p, "b1"), 240, 100), 60, 1e-9) && p.local(1).scaleX == 1, "without stretch: straight at it, 60 px short");
    d["ik"][0]["stretch"] = true;
    r = parseRig("a", d);
    p = samplePose(r, -1, 0);
    check(distance(tipOf(r, p, "b1"), 240, 100) < 1e-9 && near(p.local(1).scaleX, 1.75, 1e-12), "stretch: upper x 1.75, the tip lands");
    Json reach = chainRig(150, 140);
    reach["ik"][0]["bones"] = {"b0", "b1"};
    reach["ik"][0]["stretch"] = true;
    r = parseRig("a", reach);
    p = samplePose(r, -1, 0);
    check(p.local(1).scaleX == 1 && distance(tipOf(r, p, "b1"), 150, 140) < 1e-9, "reachable: no stretch, analytic solve");
    // Rotated, uniformly scaled root: still exact.
    d["bones"][0]["rotation"] = -35; d["bones"][0]["scaleX"] = 1.3; d["bones"][0]["scaleY"] = 1.3;
    r = parseRig("a", d);
    p = samplePose(r, -1, 0);
    check(distance(tipOf(r, p, "b1"), p.world[5].e, p.world[5].f) < 1e-9, "stretch exact under a rotated, scaled root");
  });

  // ---------------------------------------------------------------- chains (FABRIK)
  test("ik chain: FABRIK converges within tolerance, deterministic, exact joints", [] {
    int maxIterations = 0;
    for (const auto& [tx, ty] : std::vector<std::pair<double, double>>{{200, 160}, {130, 180}, {60, 130}, {210, 60}, {120, 60}, {40, 200}}) {
      const Rig r = parseRig("c", chainRig(tx, ty));
      const Pose p = samplePose(r, -1, 0);
      const double err = distance(tipOf(r, p, "b3"), tx, ty);
      check(p.ikSolve[0].solved && p.ikSolve[0].iterations <= IK_CHAIN_ITERATIONS && err <= IK_CHAIN_TOLERANCE + 1e-9, "target " + std::to_string(tx) + "," + std::to_string(ty) + " error " + std::to_string(err));
      check(near(err, p.ikSolve[0].error, 1e-9), "reported error is the drawn effector's");
      for (const char* b : {"b0", "b1", "b2", "b3"}) check(near(std::hypot(p.world[size_t(r.boneIndex(b))].a, p.world[size_t(r.boneIndex(b))].b), 1, 1e-12), "rigid bones (no scale)");
      maxIterations = std::max(maxIterations, p.ikSolve[0].iterations);
      // Same input, same bits.
      const Pose q = samplePose(r, -1, 0);
      bool same = true;
      for (size_t i = 0; i < p.world.size(); i++) same = same && std::memcmp(&p.world[i], &q.world[i], sizeof(Affine)) == 0;
      check(same, "deterministic");
    }
    check(maxIterations > 0, "FABRIK iterated");
    // Unreachable: straight at the target, error = the gap; stretch closes it (first bone's x).
    Json far = chainRig(400, 100);
    Rig r = parseRig("c", far);
    Pose p = samplePose(r, -1, 0);
    check(near(p.ikSolve[0].error, 140, 1e-9) && p.ikSolve[0].iterations == 0, "unreachable: straight, 140 px short");
    far["ik"][0]["stretch"] = true;
    r = parseRig("c", far);
    p = samplePose(r, -1, 0);
    check(distance(tipOf(r, p, "b3"), 400, 100) < 1e-9 && near(p.local(1).scaleX, 300.0 / 160, 1e-12), "stretch: whole chain scales along the first bone's x");
    // Mirrored, rotated, scaled root: directions are exact in each parent space.
    Json m = chainRig(160, 170);
    m["bones"][0]["rotation"] = 40; m["bones"][0]["scaleX"] = -1.5; m["bones"][0]["scaleY"] = 1.5;
    r = parseRig("c", m);
    p = samplePose(r, -1, 0);
    check(distance(tipOf(r, p, "b3"), p.world[5].e, p.world[5].f) <= IK_CHAIN_TOLERANCE + 1e-9, "mirrored scaled root: converges");
    // mix 0 = FK; tip bone as effector; a straight chain bends to the bendPositive side.
    Json zero = chainRig(130, 180);
    zero["ik"][0]["mix"] = 0;
    p = samplePose(parseRig("c", zero), -1, 0);
    check(near(p.world[4].e, 220, 1e-9) && near(p.world[4].f, 100, 1e-9), "mix 0: forward kinematics");
    Json straight = chainRig(180, 100);  // on the chain's own line, closer than its length
    r = parseRig("c", straight);
    p = samplePose(r, -1, 0);
    // Positive = the joints turn clockwise on screen (two-bone convention): the first joint rises (-y).
    check(distance(tipOf(r, p, "b3"), 180, 100) <= IK_CHAIN_TOLERANCE + 1e-9 && p.world[2].f - 100 < -1, "straight chain: bends to the bendPositive side");
    straight["ik"][0]["bendPositive"] = false;
    p = samplePose(parseRig("c", straight), -1, 0);
    check(p.world[2].f - 100 > 1, "bendPositive false: the other side");
    Json tip = chainRig(150, 170);
    tip["bones"].push_back({{"id", "claw"}, {"parent", "b3"}, {"x", 20}, {"y", 10}});
    tip["ik"][0]["tip"] = "claw";
    r = parseRig("c", tip);
    p = samplePose(r, -1, 0);
    check(std::hypot(p.world[6].e - 150, p.world[6].f - 170) <= IK_CHAIN_TOLERANCE + 1e-9, "explicit tip bone is the effector");
  });

  test("ik chain: validation and editing (addIk run, stretch toggles)", [] {
    Json d = chainRig(150, 150);
    check(validateRig(d).empty(), "4-bone chain validates");
    Json gap = d; gap["ik"][0]["bones"] = {"b0", "b2", "b3"};
    check(hasCode(validateRig(gap), "ANIM_IK_CHAIN"), "gap in the chain");
    Json tip = d; tip["ik"][0]["tip"] = "b2";
    check(hasCode(validateRig(tip), "ANIM_IK_TIP"), "tip must hang from the last bone");
    Json target = d; target["ik"][0]["target"] = "b3";
    check(hasCode(validateRig(target), "ANIM_IK_TARGET"), "target inside the chain");
    // addIk with a run: the target at the effector, nothing moves.
    Json base = chainRig(0, 0);
    base["ik"] = Json::array();
    base["bones"][2]["rotation"] = 30; base["bones"][3]["rotation"] = 25;
    const auto before = samplePose(parseRig("c", base), -1, 0).world;
    auto r = applyEdit(base, {{"op", "addIk"}, {"bones", {"b0", "b1", "b2", "b3"}}, {"createTarget", true}, {"stretch", true}});
    const Json& k = r.data["ik"][0];
    check(k["bones"].size() == 4 && k["stretch"] == true && !k.contains("bendPositive"), "chain constraint written: " + k.dump());
    const auto after = samplePose(parseRig("c", r.data), -1, 0).world;
    bool unchanged = true;
    for (size_t i = 0; i < before.size(); i++) unchanged = unchanged && near(before[i].e, after[i].e, 1e-6) && near(before[i].f, after[i].f, 1e-6) && near(before[i].a, after[i].a, 1e-7);
    check(unchanged, "creating it moves nothing");
    r = applyEdit(r.data, {{"op", "setIk"}, {"ik", k["id"]}, {"stretch", false}, {"uniform", true}});
    check(!r.data["ik"][0].contains("stretch") && r.data["ik"][0]["uniform"] == true, "setIk toggles");
    bool refused = false;
    try { applyEdit(base, {{"op", "addIk"}, {"bones", {"b0", "b2"}}, {"createTarget", true}}); } catch (const EditError&) { refused = true; }
    check(refused, "non-adjacent run refused");
    refused = false;
    try { applyEdit(base, {{"op", "addIk"}, {"bones", {"b0", "b1", "b2"}}, {"createTarget", true}, {"stretch", 1}}); } catch (const EditError&) { refused = true; }
    check(refused, "stretch must be a boolean");
  });

  // ---------------------------------------------------------------- physics
  test("physics: validation", [] {
    check(validateRig(hairRig()).empty(), "hair fixture validates");
    auto bad = [](const std::function<void(Json&)>& edit, const std::string& code) { Json d = hairRig(); edit(d); return hasCode(validateRig(d), code); };
    check(bad([](Json& d) { d["physics"].push_back(d["physics"][0]); }, "ANIM_PHYSICS_DUP_ID"), "duplicate id");
    check(bad([](Json& d) { d["physics"][0]["bone"] = "nope"; }, "ANIM_PHYSICS_BONE"), "unknown bone");
    check(bad([](Json& d) { d["physics"][0]["rotate"] = 1.5; }, "ANIM_PHYSICS_RANGE"), "channel range");
    check(bad([](Json& d) { d["physics"][0]["damping"] = -0.1; }, "ANIM_PHYSICS_RANGE"), "damping range");
    check(bad([](Json& d) { d["physics"][0]["mass"] = 0; }, "ANIM_PHYSICS_MASS"), "zero mass");
    check(bad([](Json& d) { d["physics"][0]["step"] = 0.5; }, "ANIM_PHYSICS_STEP") && bad([](Json& d) { d["physics"][0]["step"] = 0.001; }, "ANIM_PHYSICS_STEP"), "step range");
    check(bad([](Json& d) { d["physics"][0]["mix"] = 2; }, "ANIM_PHYSICS_MIX"), "mix range");
    check(bad([](Json& d) { d["physics"][0]["rotate"] = 0; }, "ANIM_PHYSICS_CHANNELS"), "no channel: warning");
    check(bad([](Json& d) { d["bones"][2].erase("length"); }, "ANIM_PHYSICS_LENGTH"), "rotate on a point bone: warning");
    check(bad([](Json& d) { d["physics"][1]["bone"] = "hair0"; }, "ANIM_PHYSICS_SAME_BONE"), "two on one bone: warning");
    check(bad([](Json& d) { d["physics"][0]["resetOnStart"] = "yes"; }, "ANIM_STRUCTURE"), "resetOnStart bool");
    check(bad([](Json& d) { d["clips"][0]["tracks"].push_back({{"target", "physics:nope"}, {"property", "mix"}, {"keys", {{{"timeMs", 0}, {"value", 1}}}}}); }, "ANIM_TARGET_MISSING"), "unknown physics track");
    check(bad([](Json& d) { d["clips"][0]["tracks"].push_back({{"target", "physics:hair0-physics"}, {"property", "x"}, {"keys", {{{"timeMs", 0}, {"value", 1}}}}}); }, "ANIM_PROPERTY"), "physics tracks key mix only");
    check(bad([](Json& d) { d["clips"][0]["tracks"].push_back({{"target", "physics:hair0-physics"}, {"property", "mix"}, {"keys", {{{"timeMs", 0}, {"value", 3}}}}}); }, "ANIM_PHYSICS_MIX"), "mix keys 0..1");
    const Rig r = parseRig("h", hairRig());
    check(r.physics.size() == 4 && r.physicsOrder.size() == 4 && r.physics[2].stepMs > 8.33 && r.physics[2].stepMs < 8.34 && near(r.physics[0].decay, 0.8, 1e-12), "parsed: steps, decay");
    check(r.physics[0].affected.size() == 3 && r.physics[0].lever == 30, "affected subtree, lever = bone length");
  });

  test("physics: frame-rate independence (30/60/144/irregular/binary partitions, identical bits)", [] {
    const Json rig = hairRig();
    const std::vector<double> checkpoints = {500, 1000, 1500, 2000, 3000};
    const auto at60 = runPartition(rig, checkpoints, [](int) { return 1000.0 / 60; });
    const auto at30 = runPartition(rig, checkpoints, [](int) { return 1000.0 / 30; });
    const auto at144 = runPartition(rig, checkpoints, [](int) { return 1000.0 / 144; });
    const double binary[] = {15.625, 7.8125, 31.25, 3.90625, 62.5, 0.48828125};
    const auto exact = runPartition(rig, checkpoints, [&](int i) { return binary[i % 6]; });
    uint32_t seed = 12345;
    const auto irregular = runPartition(rig, checkpoints, [&](int) {
      seed = seed * 1664525u + 1013904223u;
      const uint32_t v = seed >> 8;
      return v % 7 == 0 ? 0.0 : v % 11 == 0 ? 0.001 : 0.5 + double(v % 40000) / 1000.0;  // 0, tiny and 0.5..40.5 ms
    });
    for (size_t i = 0; i < checkpoints.size(); i++) {
      const std::string t = std::to_string(int(checkpoints[i])) + "ms";
      check(at30[i] == at60[i], "30 Hz == 60 Hz at " + t);
      check(at144[i] == at60[i], "144 Hz == 60 Hz at " + t);
      check(exact[i] == at60[i], "binary-exact partition == 60 Hz at " + t);
      check(irregular[i] == at60[i], "irregular partition == 60 Hz at " + t);
    }
    // And the physics really moves the hair (not a trivially static pose).
    Instance moving(parsed(rig)), still(parsed([&] { Json d = rig; d.erase("physics"); return d; }()));
    moving.play(0, "sway"); still.play(0, "sway");
    for (int k = 0; k < 30; k++) { moving.update(1000.0 / 60); still.update(1000.0 / 60); }
    const auto& a = moving.pose().world[4];
    const auto& b = still.pose().world[4];
    check(std::hypot(a.e - b.e, a.f - b.f) > 1, "hair tip trails the sway: " + std::to_string(std::hypot(a.e - b.e, a.f - b.f)) + " px");
  });

  test("physics: settled start (translation equilibrium, rotation sag), stays at rest", [] {
    Json d = hairRig();
    d["clips"] = Json::array();
    d["physics"] = Json::array({{{"id", "drop"}, {"bone", "ribbon"}, {"y", 1}, {"strength", 100}, {"gravity", 500}},
                                {{"id", "swing"}, {"bone", "hair1"}, {"rotate", 1}, {"strength", 60}, {"gravity", 400}, {"damping", 0.8}}});
    d["bones"][2]["rotation"] = 0;  // hair horizontal: gravity turns it down
    Json bare = d; bare.erase("physics");
    Instance inst(parsed(d)), plain(parsed(bare));
    const Pose p = inst.pose(), q = plain.pose();
    check(near(p.world[5].f - q.world[5].f, 5, 1e-6) && near(p.world[5].e, q.world[5].e, 1e-12), "translation settled at gravity / strength = 5 px: " + std::to_string(p.world[5].f - q.world[5].f) + " / " + std::to_string(p.world[5].e - q.world[5].e));
    const double r = std::atan2(p.world[3].b, p.world[3].a) - std::atan2(q.world[3].b, q.world[3].a), lever = 30;
    check(r > 0.05 && near(400 * std::cos(r) / lever, 60 * r, 1e-4), "rotation settled where gravity torque = spring: " + std::to_string(r));
    check(inst.physics().settles == 2 && inst.physics().steps == 0, "settled once each at construction");
    for (int k = 0; k < 60; k++) inst.update(1000.0 / 60);
    const Pose later = inst.pose();
    check(near(later.world[5].f, p.world[5].f, 1e-6) && near(later.world[4].e, p.world[4].e, 1e-6), "at rest: nothing drifts after a second");
    check(inst.physics().steps == 60 * 2 - 2 || inst.physics().steps >= 110, "steps counted: " + std::to_string(inst.physics().steps));
  });

  test("physics: reset rules (resetOnStart, teleport, restore, seek, hitch)", [] {
    Json d = hairRig();
    d["physics"][0]["resetOnStart"] = true;
    Instance inst(parsed(d));
    inst.play(0, "sway");
    for (int k = 0; k < 20; k++) inst.update(1000.0 / 60);
    check(inst.physics().bodies[0].vr != 0 && inst.physics().bodies[1].vr != 0, "swinging");
    const int64_t settles = inst.physics().settles;
    inst.play(0, "still");
    check(inst.physics().bodies[0].vr == 0 && inst.physics().settles == settles + 1 && inst.physics().bodies[1].vr != 0, "resetOnStart: only that constraint settles at a base clip start");
    inst.play(1, "sway");
    check(inst.physics().settles == settles + 1, "layer tracks never reset");
    // Teleport: the root jumps 1000 px within one step.
    Instance jumper(parsed(hairRig()));
    jumper.play(0, "jump");
    for (int k = 0; k < 40; k++) jumper.update(1000.0 / 60);
    const Pose jp = jumper.pose();
    check(jumper.physics().teleports >= 4 && jp.world[0].e == 1200, "teleport: every body settles instead of flying");
    Json bare = hairRig(); bare.erase("physics");
    Instance plainJump(parsed(bare));
    plainJump.play(0, "jump");
    for (int k = 0; k < 40; k++) plainJump.update(1000.0 / 60);
    check(std::hypot(jp.world[4].e - plainJump.pose().world[4].e, jp.world[4].f - plainJump.pose().world[4].f) < 40, "no whip after the jump");
    // Restore: not saved; starts settled (same as a fresh instance restored the same way).
    Instance a(parsed(hairRig())), b(parsed(hairRig()));
    a.play(0, "jump"); b.play(0, "jump");
    for (int k = 0; k < 10; k++) a.update(16);
    const Json saved = a.saveState();
    check(saved.dump().find("physics") == std::string::npos, "physics state is not saved");
    a.restoreState(saved);
    b.restoreState(saved);
    check(bitsOf(a) == bitsOf(b), "restored instances start from the same settled state");
    // Seek (editor scrub) settles: no simulation while scrubbing.
    Instance s(parsed(hairRig()));
    s.play(0, "sway");
    for (int k = 0; k < 10; k++) s.update(16);
    const int64_t before = s.physics().settles;
    s.seek(0, 700);
    check(s.physics().settles == before + 4 && s.physics().bodies[0].vr == 0, "seek settles every body");
    // A hitch of seconds: steps are capped, the bodies settle.
    Instance h(parsed(hairRig()));
    h.play(0, "sway");
    const int64_t hs = h.physics().settles;
    h.update(9000);
    check(h.physics().steps <= PHYSICS_MAX_STEPS_PER_UPDATE * 4 && h.physics().settles == hs + 4 && std::isfinite(h.pose().world[4].e), "9 s hitch: capped, settled: " + h.describe()["physics"].dump());
    h.resetPhysics();
    check(h.physics().settles == hs + 8, "resetPhysics");
  });

  test("physics: mix (constraint, keyed track, 0 = the animated pose), environment, applied after IK", [] {
    Json d = hairRig();
    d["physics"] = Json::array({{{"id", "bob"}, {"bone", "ribbon"}, {"x", 1}, {"y", 1}, {"strength", 40}, {"gravity", 200}}});
    Json bare = d; bare.erase("physics");
    auto run = [&](const Json& rig) {
      Instance inst(parsed(rig));
      inst.play(0, "sway");
      for (int k = 0; k < 25; k++) inst.update(15.625);
      return inst.pose().world[5];
    };
    const Affine base = run(bare), full = run(d);
    d["physics"][0]["mix"] = 0.5;
    const Affine half = run(d);
    check(std::hypot(full.e - base.e, full.f - base.f) > 1 && near(full.e - base.e, 2 * (half.e - base.e), 1e-9) && near(full.f - base.f, 2 * (half.f - base.f), 1e-9), "mix 0.5 = half the offset");
    d["physics"][0]["mix"] = 1;
    d["clips"][0]["tracks"].push_back({{"target", "physics:bob"}, {"property", "mix"}, {"keys", {{{"timeMs", 0}, {"value", 0}}}}});
    const Affine keyed = run(d);
    check(std::memcmp(&keyed, &base, sizeof(Affine)) == 0, "keyed mix 0: exactly the animated pose");
    auto r = applyEdit(hairRig(), {{"op", "setKey"}, {"clip", "sway"}, {"target", "physics:hair0-physics"}, {"property", "mix"}, {"timeMs", 500}, {"value", 0.25}});
    const Rig parsedRig = parseRig("h", r.data);
    check(near(samplePose(parsedRig, 0, 500).physicsMix[0], 0.25, 1e-12), "setKey physics mix, sampled");
    // Environment: wind pushes the bob along +x.
    Instance calm(parsed(hairRig())), windy(parsed(hairRig()));
    windy.setPhysicsEnvironment(400, 0);
    windy.resetPhysics();
    calm.resetPhysics();
    check(windy.pose().world[5].e > calm.pose().world[5].e + 1, "environment wind moves the ribbon");
    // After IK: a physics bone on an IK chain end still swings from the solved pose.
    Json ik = parityRig();
    Instance withIk(parsed(ik));
    withIk.play(0, "sway");
    for (int k = 0; k < 20; k++) withIk.update(16.0);
    check(withIk.pose().ikSolve.size() == 1 && withIk.pose().ikSolve[0].solved && std::isfinite(withIk.pose().world.back().e), "IK and physics together");
  });

  test("physics: editing ops (addPhysics presets, setPhysics, rename, delete, deleteBone)", [] {
    Json d = hairRig();
    d.erase("physics");
    auto r = applyEdit(d, {{"op", "addPhysics"}, {"bones", {"hair0", "hair1"}}, {"preset", "hair"}});
    check(r.extra["physics"] == Json::array({"hair0-physics", "hair1-physics"}) && r.data["physics"].size() == 2, "one constraint per bone");
    const Json& k = r.data["physics"][0];
    check(k["rotate"] == 1 && k["gravity"] == 400 && k["strength"] == 60 && k["bone"] == "hair0", "hair preset fields: " + k.dump());
    r = applyEdit(r.data, {{"op", "setPhysics"}, {"physics", "hair0-physics"}, {"preset", "accessory"}, {"strength", 75}});
    check(r.data["physics"][0]["x"] == 0.4 && r.data["physics"][0]["strength"] == 75 && r.data["physics"][0]["damping"] == 0.92, "preset then explicit field");
    r = applyEdit(r.data, {{"op", "setPhysics"}, {"physics", "hair0-physics"}, {"strength", nullptr}, {"resetOnStart", true}, {"step", 1.0 / 120}});
    check(!r.data["physics"][0].contains("strength") && r.data["physics"][0]["resetOnStart"] == true && r.data["physics"][0]["step"] == 1.0 / 120, "null = default, flags, exact step");
    r = applyEdit(r.data, {{"op", "setKey"}, {"clip", "sway"}, {"target", "physics:hair1-physics"}, {"property", "mix"}, {"timeMs", 0}, {"value", 0.5}});
    r = applyEdit(r.data, {{"op", "renamePhysics"}, {"physics", "hair1-physics"}, {"id", "tip-sway"}, {"name", "끝 흔들림"}});
    bool renamed = false;
    for (const auto& t : r.data["clips"][0]["tracks"]) renamed = renamed || t["target"] == "physics:tip-sway";
    check(renamed && r.extra["tracks"] == 1 && r.data["physics"][1]["name"] == "끝 흔들림", "rename follows the tracks");
    auto del = applyEdit(r.data, {{"op", "deletePhysics"}, {"physics", "tip-sway"}});
    bool left = false;
    for (const auto& t : del.data["clips"][0]["tracks"]) left = left || t["target"] == "physics:tip-sway";
    check(!left && del.extra["keys"] == 1 && del.data["physics"].size() == 1, "delete drops its keys");
    del = applyEdit(del.data, {{"op", "deleteBone"}, {"bone", "hair0"}, {"cascade", true}});
    check(!del.data.contains("physics") && hasCode(del.diagnostics, "ANIM_PHYSICS_REMOVED"), "deleteBone drops the bone's physics");
    auto refuse = [&](const Json& op) { try { applyEdit(d, op); } catch (const EditError&) { return true; } return false; };
    check(refuse({{"op", "addPhysics"}, {"bone", "nope"}}), "unknown bone");
    check(refuse({{"op", "addPhysics"}, {"bone", "hair0"}, {"preset", "cape"}}), "unknown preset");
    check(refuse({{"op", "addPhysics"}, {"bone", "hair0"}, {"damping", 3}}), "out of range");
    check(refuse({{"op", "setKey"}, {"clip", "sway"}, {"target", "physics:none"}, {"property", "mix"}, {"timeMs", 0}, {"value", 1}}), "unknown constraint key");
    r = applyEdit(d, {{"op", "addPhysics"}, {"bone", "ribbon"}});
    check(r.data["physics"][0]["rotate"] == 1, "no preset, no channel: turns");
    check(physicsPresets().size() == 3 && physicsPresets()[1]["label"] == "치마", "presets listed");
  });
}
}  // namespace ikphys
