// Shear, bone inheritance and transform constraints (plugin 1.16.0), included by core-tests.cpp
// after the other fixtures. Also the --transform-parity script (tests/plugins/cutout-transform-parity.test.mjs).
#pragma once
#include <cstring>

namespace xform {
using cutout::Json;
using cutout::Affine;
constexpr double D2R = 3.14159265358979323846 / 180;

Affine rot(double deg) { return {std::cos(deg * D2R), std::sin(deg * D2R), -std::sin(deg * D2R), std::cos(deg * D2R), 0, 0}; }
Affine scale(double x, double y) { return {x, 0, 0, y, 0, 0}; }
Affine move(double x, double y) { return {1, 0, 0, 1, x, y}; }
bool sameM(const Affine& a, const Affine& b, double eps = 1e-9) {
  return std::fabs(a.a - b.a) <= eps && std::fabs(a.b - b.b) <= eps && std::fabs(a.c - b.c) <= eps && std::fabs(a.d - b.d) <= eps &&
         std::fabs(a.e - b.e) <= eps * 100 && std::fabs(a.f - b.f) <= eps * 100;
}
bool bitsEqual(const Affine& a, const Affine& b) { return std::memcmp(&a, &b, sizeof a) == 0; }
double angleOf(double x, double y) { return std::atan2(y, x) / D2R; }
double wrap(double d) { return std::remainder(d, 360.0); }
/** Direction (dx, dy) equals the direction of (x, y): parallel and the same way. */
bool pointsAt(double x, double y, double dx, double dy, double eps = 1e-9) {
  const double n = std::hypot(x, y) * std::hypot(dx, dy);
  return n > 0 && std::fabs(x * dy - y * dx) / n < eps && x * dx + y * dy > 0;
}

/** root at (10, 20), rotation 30, scale 2 x 0.5 (`root` overrides), child at (5, 0) rotation 15 with `inherit`. */
Json inheritRig(const std::string& mode, const Json& root = Json::object()) {
  Json d = Json::parse(R"({"schemaVersion": 1, "canvas": {"width": 400, "height": 400, "originX": 0, "originY": 0},
    "bones": [{"id": "root", "parent": null, "x": 10, "y": 20, "rotation": 30, "scaleX": 2, "scaleY": 0.5},
              {"id": "child", "parent": "root", "x": 5, "y": 0, "rotation": 15}],
    "slots": [], "attachments": [], "clips": []})");
  for (auto it = root.begin(); it != root.end(); ++it) d["bones"][0][it.key()] = it.value();
  if (mode != "normal") d["bones"][1]["inherit"] = mode;
  return d;
}
Affine worldOf(const Json& d, const std::string& bone) {
  const cutout::Rig r = cutout::parseRig("x", d);
  return cutout::samplePose(r, -1, 0).world[size_t(r.boneIndex(bone))];
}

/**
 * A character arm for transform constraints: `t` (target, rotated and scaled, sheared), `b` with
 * child `c`, an aim IK turning `b` toward `aim`, and `d` following `b`.
 */
Json constraintRig() {
  return Json::parse(R"({"schemaVersion": 1, "canvas": {"width": 400, "height": 400, "originX": 0, "originY": 0},
    "bones": [
      {"id": "root", "parent": null, "x": 200, "y": 200},
      {"id": "t", "parent": "root", "x": 100, "y": 50, "rotation": 40, "scaleX": 1.5, "scaleY": 0.8, "shearY": 12},
      {"id": "b", "parent": "root", "x": -50, "y": 20, "rotation": -10, "length": 30},
      {"id": "c", "parent": "b", "x": 30, "y": 0, "rotation": 5},
      {"id": "aim", "parent": "root", "x": -50, "y": 120},
      {"id": "d", "parent": "root", "x": 0, "y": -80}],
    "slots": [{"id": "sc", "bone": "c", "attachment": "img"}],
    "attachments": [{"slot": "sc", "name": "img", "imageAssetId": "img-c", "width": 20, "height": 10}],
    "clips": []})");
}

/** The --transform-parity rig: inherit modes, shear keys, transform constraints in every mode, an
 *  IK interleaved by order, physics on a noScale tail, a sheared weighted mesh and a clip. */
Json parityRig() {
  Json d = Json::parse(R"({"schemaVersion": 1, "canvas": {"width": 400, "height": 400, "originX": 200, "originY": 300},
    "bones": [
      {"id": "root", "parent": null, "x": 200, "y": 300, "scaleX": 1.2, "scaleY": 0.9},
      {"id": "body", "parent": "root", "x": 0, "y": -60, "rotation": -90, "length": 60, "shearX": 6},
      {"id": "head", "parent": "body", "x": 60, "y": 0, "length": 30, "inherit": "noScale"},
      {"id": "hat", "parent": "head", "x": 30, "y": 0, "inherit": "onlyTranslation"},
      {"id": "arm", "parent": "body", "x": 40, "y": 10, "rotation": 100, "length": 40, "inherit": "noRotationOrReflection"},
      {"id": "hand", "parent": "arm", "x": 40, "y": 0, "length": 20, "inherit": "noScaleOrReflection"},
      {"id": "tail0", "parent": "root", "x": -30, "y": -10, "rotation": 160, "length": 30, "inherit": "noScale"},
      {"id": "tail1", "parent": "tail0", "x": 30, "y": 0, "length": 30, "shearY": -8},
      {"id": "goal", "parent": "root", "x": 80, "y": -100},
      {"id": "mirror", "parent": "root", "x": -80, "y": -60, "scaleX": -1},
      {"id": "wing", "parent": "mirror", "x": 10, "y": 0, "rotation": 20, "length": 25, "inherit": "noScale"},
      {"id": "wing2", "parent": "mirror", "x": 10, "y": 10, "rotation": 20, "length": 25, "inherit": "noScaleOrReflection"},
      {"id": "badge", "parent": "root", "x": 60, "y": -40}],
    "slots": [
      {"id": "s-body", "bone": "body", "attachment": "torso"}, {"id": "s-head", "bone": "head", "attachment": "face"},
      {"id": "s-hat", "bone": "hat", "attachment": "hat"}, {"id": "s-arm", "bone": "arm", "attachment": "sleeve"},
      {"id": "s-hand", "bone": "hand", "attachment": "palm"}, {"id": "s-tail", "bone": "tail1", "attachment": "tip"},
      {"id": "s-wing", "bone": "wing", "attachment": "feather"}, {"id": "s-wing2", "bone": "wing2", "attachment": "feather"},
      {"id": "s-badge", "bone": "badge", "attachment": "badge"}],
    "attachments": [
      {"slot": "s-body", "name": "torso", "imageAssetId": "img-torso", "width": 60, "height": 40, "pivotX": 0, "pivotY": 0.5,
       "type": "mesh", "vertices": [0, 0, 60, 0, 60, 40, 0, 40], "uvs": [0, 0, 1, 0, 1, 1, 0, 1], "triangles": [0, 1, 2, 0, 2, 3],
       "weights": [[{"bone": "body", "weight": 1}], [{"bone": "head", "weight": 0.6}, {"bone": "body", "weight": 0.4}],
                   [{"bone": "head", "weight": 0.6}, {"bone": "body", "weight": 0.4}], [{"bone": "body", "weight": 1}]]},
      {"slot": "s-head", "name": "face", "imageAssetId": "img-face", "width": 30, "height": 30, "pivotX": 0},
      {"slot": "s-hat", "name": "hat", "imageAssetId": "img-hat", "width": 24, "height": 12},
      {"slot": "s-arm", "name": "sleeve", "imageAssetId": "img-sleeve", "width": 40, "height": 12, "pivotX": 0},
      {"slot": "s-hand", "name": "palm", "imageAssetId": "img-palm", "width": 20, "height": 14, "pivotX": 0},
      {"slot": "s-tail", "name": "tip", "imageAssetId": "img-tip", "width": 30, "height": 8, "pivotX": 0},
      {"slot": "s-wing", "name": "feather", "imageAssetId": "img-feather", "width": 25, "height": 10, "pivotX": 0},
      {"slot": "s-wing2", "name": "feather", "imageAssetId": "img-feather", "width": 25, "height": 10, "pivotX": 0},
      {"slot": "s-badge", "name": "badge", "imageAssetId": "img-badge", "width": 16, "height": 16}],
    "ik": [{"id": "reach", "bones": ["arm", "hand"], "target": "goal", "order": 1}],
    "transform": [
      {"id": "follow", "bones": ["badge"], "target": "hand", "mixScaleX": 0.5, "mixShearY": 1, "offsetX": 8, "order": 2},
      {"id": "copy", "bones": ["wing2"], "target": "wing", "local": true, "mixX": 0, "mixY": 0, "mixScaleX": 1},
      {"id": "nod", "bones": ["hat"], "target": "head", "relative": true, "mixX": 0, "mixY": 0, "offsetRotation": -15, "order": 3},
      {"id": "sway", "bones": ["tail1"], "target": "body", "local": true, "relative": true, "mixRotate": 0.4, "mixX": 0, "mixY": 0}],
    "physics": [{"id": "tail-physics", "bone": "tail0", "rotate": 1, "inertia": 0.6, "strength": 80, "damping": 0.85, "gravity": 300}],
    "clips": [{"id": "walk", "durationMs": 1000, "loop": true, "tracks": [
      {"target": "bone:body", "property": "rotation", "keys": [{"timeMs": 0, "value": -100}, {"timeMs": 500, "value": -75, "interp": "bezier", "curve": [0.42, 0, 0.58, 1]}, {"timeMs": 1000, "value": -100}]},
      {"target": "bone:body", "property": "shearY", "keys": [{"timeMs": 0, "value": 0}, {"timeMs": 400, "value": 18}, {"timeMs": 1000, "value": 0}]},
      {"target": "bone:head", "property": "shearX", "keys": [{"timeMs": 0, "value": -10}, {"timeMs": 600, "value": 10, "interp": "step"}, {"timeMs": 1000, "value": -10}]},
      {"target": "bone:root", "property": "x", "keys": [{"timeMs": 0, "value": 180}, {"timeMs": 500, "value": 230}, {"timeMs": 1000, "value": 180}]},
      {"target": "bone:goal", "property": "y", "keys": [{"timeMs": 0, "value": -100}, {"timeMs": 500, "value": -40}, {"timeMs": 1000, "value": -100}]},
      {"target": "transform:follow", "property": "mixRotate", "keys": [{"timeMs": 0, "value": 1}, {"timeMs": 700, "value": 0.2, "interp": "bezier", "curve": [0.25, 0.1, 0.25, 1]}, {"timeMs": 1000, "value": 1}]},
      {"target": "transform:copy", "property": "mixRotate", "keys": [{"timeMs": 0, "value": 0}, {"timeMs": 500, "value": 1}]}]}]})");
  return d;
}
Json parityScript() {
  const Json controller = {{"schemaVersion", 1}, {"rig", "shape"}, {"defaultClip", "walk"}};
  Json script = Json::array({{{"op", "init"}, {"frameFormats", {2}}, {"documents", Json::array({{{"type", "rig"}, {"id", "shape"}, {"data", parityRig()}}, {{"type", "controller"}, {"id", "shape-main"}, {"data", controller}}})}}});
  for (const char* id : {"a", "b"})
    script.push_back({{"op", "mount"}, {"instance", id}, {"element", {{"type", "character"}, {"ref", "shape-main"}, {"width", 400}, {"height", 400}, {"props", {{"scale", 1}, {"flipX", std::string(id) == "b" ? false : false}}}}}});
  // Instance a at 60 Hz, b at irregular pacing: their frames must be the same at the shared times.
  const double irregular[] = {9.1, 4.4, 27.0, 0.5, 13.3, 31.0, 6.25, 18.0, 0.0, 22.0, 12.5, 40.0};
  double ta = 0, tb = 0;
  int ib = 0;
  for (int checkpoint = 1; checkpoint <= 5; checkpoint++) {
    const double cp = 300.0 * checkpoint;
    while (cp - ta > 1e-7) { const double dt = std::min(1000.0 / 60, cp - ta); script.push_back({{"op", "update"}, {"instance", "a"}, {"dtMs", dt}}); ta += dt; }
    while (cp - tb > 1e-7) { const double dt = std::min(irregular[ib++ % 12], cp - tb); script.push_back({{"op", "update"}, {"instance", "b"}, {"dtMs", dt}}); tb += dt; }
    for (const char* id : {"a", "b"}) {
      script.push_back({{"op", "frame"}, {"instance", id}});
      script.push_back({{"op", "frame"}, {"instance", id}, {"format", 2}});
    }
  }
  for (double ms : {0.0, 333.0, 640.0, 999.0}) script.push_back({{"op", "sample"}, {"rig", parityRig()}, {"clip", "walk"}, {"timeMs", ms}});
  script.push_back({{"op", "describeAll"}});
  return script;
}

std::vector<std::string> runPartition(const Json& rig, const std::string& clip, const std::vector<double>& checkpoints, const std::function<double(int)>& dtAt) {
  cutout::Instance inst(std::make_shared<const cutout::Rig>(cutout::parseRig("r", rig)));
  inst.play(0, clip);
  std::vector<std::string> out;
  double now = 0;
  int i = 0;
  for (double cp : checkpoints) {
    while (cp - now > 1e-7) {
      double dt = dtAt(i++);
      if (now + dt > cp + 1e-7) dt = cp - now;
      inst.update(dt);
      now += dt;
    }
    out.push_back(ikphys::bitsOf(inst));
  }
  return out;
}

void run() {
  using namespace cutout;
  // ---------------------------------------------------------------- shear
  test("shear: local matrix x axis at rotation+shearX, y axis at rotation+90+shearY; none = compose bits", [] {
    Transform t; t.x = 3; t.y = -4; t.rotation = 30; t.scaleX = 2; t.scaleY = 0.5; t.shearX = 10; t.shearY = -20;
    const Affine m = localMatrix(t);
    const Affine want{2 * std::cos(40 * D2R), 2 * std::sin(40 * D2R), -0.5 * std::sin(10 * D2R), 0.5 * std::cos(10 * D2R), 3, -4};
    check(sameM(m, want, 1e-12), "hand-computed sheared matrix");
    check(sameM(m, move(3, -4) * rot(30) * Affine{std::cos(10 * D2R), std::sin(10 * D2R), -std::sin(-20 * D2R), std::cos(-20 * D2R), 0, 0} * scale(2, 0.5), 1e-12),
          "= T · R · K(shearX, shearY) · S");
    t.shearX = t.shearY = 0;
    check(bitsEqual(localMatrix(t), Affine::compose(3, -4, 30, 2, 0.5)), "no shear: Affine::compose bit for bit");
    Json d = inheritRig("normal");
    d["bones"][1]["shearX"] = 10; d["bones"][1]["shearY"] = -20;
    Transform c; c.x = 5; c.rotation = 15; c.shearX = 10; c.shearY = -20;
    check(sameM(worldOf(d, "child"), (move(10, 20) * rot(30) * scale(2, 0.5)) * localMatrix(c)), "sheared child world = parent · local");
    check(validateRig(d).empty(), "shear fields validate");
    d["bones"][1]["shearY"] = "x";
    check(hasCode(validateRig(d), "ANIM_NONFINITE"), "shear must be a number");
  });
  test("shear keys: shearX/shearY tracks interpolate, blend and reach the pose", [] {
    Json d = inheritRig("normal");
    d["clips"] = Json::parse(R"([{"id": "lean", "durationMs": 1000, "loop": false, "tracks": [
      {"target": "bone:child", "property": "shearX", "keys": [{"timeMs": 0, "value": 0}, {"timeMs": 1000, "value": 20}]},
      {"target": "bone:child", "property": "shearY", "keys": [{"timeMs": 0, "value": 0, "interp": "bezier", "curve": [0.42, 0, 0.58, 1]}, {"timeMs": 1000, "value": -30}]}]}])");
    check(validateRig(d).empty(), "shear tracks validate");
    const Rig r = parseRig("s", d);
    const Pose p = samplePose(r, 0, 500);
    check(near(p.bones[1].shearX, 10, 1e-12) && near(p.bones[1].shearY, -15, 1e-9), "linear and bezier (symmetric) halfway");
    Transform c = p.bones[1];
    check(sameM(p.world[1], p.world[0] * localMatrix(c)), "world uses the sampled shear");
    check(Json::parse(Host().handle({{"op", "sample"}, {"rig", d}, {"clip", "lean"}, {"timeMs", 500}}).dump())["pose"]["bones"]["child"]["shearX"] == 10, "sample reply reports shear");
    Json bad = d;
    bad["clips"][0]["tracks"][0]["target"] = "slot:x";
    check(hasCode(validateRig(bad), "ANIM_TARGET_MISSING") || hasCode(validateRig(bad), "ANIM_PROPERTY"), "shear on a slot refused");
  });

  // ---------------------------------------------------------------- inherit
  test("inherit: each mode against hand-computed matrices", [] {
    const Affine P = move(10, 20) * rot(30) * scale(2, 0.5);
    double ox, oy;
    P.apply(5, 0, ox, oy);
    const double phi = 30 + angleOf(2 * std::cos(15 * D2R), 0.5 * std::sin(15 * D2R));  // P's image of the bone's 15° direction
    struct Case { const char* mode; Affine linear; };
    const Case cases[] = {
      {"normal", P * rot(15)},
      {"onlyTranslation", rot(15)},
      {"noRotationOrReflection", (rot(30) * scale(2, 0.5) * rot(-30)) * rot(15)},  // = P2 · R(-15)
      {"noScale", rot(phi)},
      {"noScaleOrReflection", rot(phi)},
    };
    for (const auto& c : cases) {
      Affine want = c.linear;
      want.e = ox; want.f = oy;
      const Affine got = worldOf(inheritRig(c.mode), "child");
      check(sameM(got, want, 1e-12), std::string(c.mode) + ": world matrix " + Json({got.a, got.b, got.c, got.d, got.e, got.f}).dump() + " vs " + Json({want.a, want.b, want.c, want.d, want.e, want.f}).dump());
      check(validateRig(inheritRig(c.mode)).empty(), std::string(c.mode) + ": validates");
    }
    check(sameM(worldOf(inheritRig("noRotationOrReflection"), "child"), [&] { Affine m = rot(30) * scale(2, 0.5) * rot(-15); m.e = ox; m.f = oy; return m; }(), 1e-12),
          "noRotationOrReflection = the parent's scale along its axes (P2 · R(-30) · R(15))");
    // A mirrored parent: noScale keeps the mirror, noScaleOrReflection does not.
    const Json mirror = {{"scaleX", -1}, {"scaleY", 1}};
    const Affine M = move(10, 20) * rot(30) * scale(-1, 1);
    M.apply(5, 0, ox, oy);
    const double th = 30 + angleOf(-std::cos(15 * D2R), std::sin(15 * D2R));
    Affine keep = rot(th) * scale(1, -1), drop = rot(th);
    keep.e = drop.e = ox; keep.f = drop.f = oy;
    check(sameM(worldOf(inheritRig("noScale", mirror), "child"), keep, 1e-12), "noScale under a mirror: mirrored axes");
    check(sameM(worldOf(inheritRig("noScaleOrReflection", mirror), "child"), drop, 1e-12), "noScaleOrReflection under a mirror: a plain rotation");
    Affine nr = rot(15);
    nr.e = ox; nr.f = oy;
    check(sameM(worldOf(inheritRig("noRotationOrReflection", mirror), "child"), nr, 1e-12), "noRotationOrReflection under a unit mirror: the local axes");
    // Shear with noScale: Q · K · S.
    Json sheared = inheritRig("noScale");
    sheared["bones"][1]["shearX"] = 10; sheared["bones"][1]["scaleX"] = 1.5;
    Transform k; k.shearX = 10; k.scaleX = 1.5;
    Affine q = rot(phi) * localMatrix(k);
    P.apply(5, 0, q.e, q.f);
    check(sameM(worldOf(sheared, "child"), q, 1e-12), "noScale with shear and scale: frame · K · S");
    // Roots ignore the mode; an unknown mode is refused.
    Json root = inheritRig("normal");
    root["bones"][0]["inherit"] = "onlyTranslation";
    check(bitsEqual(worldOf(root, "root"), worldOf(inheritRig("normal"), "root")), "a root's mode changes nothing");
    root["bones"][0]["inherit"] = "sideways";
    check(hasCode(validateRig(root), "ANIM_BONE_INHERIT"), "unknown inherit mode");
    // Grandchildren of a non-normal bone inherit normally from its world.
    Json g = inheritRig("onlyTranslation");
    g["bones"].push_back({{"id", "g"}, {"parent", "child"}, {"x", 10}, {"y", 0}, {"rotation", 5}});
    check(sameM(worldOf(g, "g"), worldOf(g, "child") * Affine::compose(10, 0, 5, 1, 1), 1e-12), "descendants of a non-normal bone: normal under its world");
  });
  test("inherit: localFromWorld inverts childWorld for every mode (shear, mirrors, hint kept)", [] {
    uint64_t seed = 7;
    auto rnd = [&](double lo, double hi) { seed = seed * 6364136223846793005ull + 1442695040888963407ull; return lo + (hi - lo) * double(seed >> 11) / double(1ull << 53); };
    const Inherit modes[] = {Inherit::Normal, Inherit::OnlyTranslation, Inherit::NoRotationOrReflection, Inherit::NoScale, Inherit::NoScaleOrReflection};
    int ok = 0, total = 0, hints = 0;
    for (int i = 0; i < 60; i++) {
      Transform pt; pt.x = rnd(-50, 50); pt.y = rnd(-50, 50); pt.rotation = rnd(-180, 180); pt.scaleX = rnd(0.3, 2) * (i % 3 == 0 ? -1 : 1); pt.scaleY = rnd(0.3, 2); pt.shearY = rnd(-20, 20);
      const Affine P = localMatrix(pt);
      Transform w; w.x = rnd(-80, 80); w.y = rnd(-80, 80); w.rotation = rnd(-180, 180); w.scaleX = rnd(0.4, 1.8); w.scaleY = rnd(0.4, 1.8) * (i % 4 == 0 ? -1 : 1); w.shearY = rnd(-30, 30);
      const Affine W = localMatrix(w);
      for (Inherit m : modes) {
        Transform hint; hint.rotation = rnd(-90, 90); hint.shearX = i % 2 ? rnd(-15, 15) : 0; hint.scaleX = i % 5 == 0 ? -1 : 1;
        const auto t = localFromWorld(P, W, m, hint);
        total++;
        if (t && sameM(childWorld(P, *t, m), W, 1e-9)) ok++;
        if (t && t->shearX == hint.shearX && (t->scaleX < 0) == (hint.scaleX < 0) && std::fabs(t->rotation - hint.rotation) <= 180) hints++;
      }
    }
    check(ok == total, "round trips: " + std::to_string(ok) + "/" + std::to_string(total));
    check(hints == total, "hint shearX, mirror sign and winding kept: " + std::to_string(hints) + "/" + std::to_string(total));
    check(!localFromWorld(scale(0, 1), Affine{}, Inherit::Normal, Transform{}), "a singular parent: nullopt");
  });
  test("inherit: rotationToward lays a bone vector on a direction in every mode", [] {
    const Affine P = move(5, 5) * rot(-25) * Affine{1.4, 0.2, -0.3, 0.7, 0, 0};
    const Inherit modes[] = {Inherit::Normal, Inherit::OnlyTranslation, Inherit::NoRotationOrReflection, Inherit::NoScale, Inherit::NoScaleOrReflection};
    int ok = 0;
    for (Inherit m : modes)
      for (double dir : {10.0, 100.0, -150.0}) {
        Transform t; t.x = 3; t.y = 1; t.scaleX = 1.3; t.scaleY = -0.8; t.shearX = 12; t.shearY = -7;
        double r;
        if (!rotationToward(P, t, m, 1, 0.4, std::cos(dir * D2R), std::sin(dir * D2R), r)) continue;
        t.rotation = r;
        const Affine w = childWorld(P, t, m);
        ok += pointsAt(w.a * 1 + w.c * 0.4, w.b * 1 + w.d * 0.4, std::cos(dir * D2R), std::sin(dir * D2R), 1e-9);
      }
    check(ok == 15, "aimed: " + std::to_string(ok) + "/15");
    double r;
    check(!rotationToward(P, Transform{}, Inherit::Normal, 0, 0, 1, 0, r), "a zero vector is degenerate");
  });

  // ---------------------------------------------------------------- IK with shaped bones
  test("ik: aim, two-bone and chains reach through shear and every inherit mode", [] {
    for (const char* mode : {"normal", "onlyTranslation", "noRotationOrReflection", "noScale", "noScaleOrReflection"}) {
      Json d = inheritRig(mode);
      d["bones"][1]["length"] = 20;
      d["bones"][1]["shearX"] = 8;
      d["bones"].push_back({{"id", "t"}, {"parent", nullptr}, {"x", 90}, {"y", 140}});
      d["ik"] = Json::array({{{"id", "aim"}, {"bones", {"child"}}, {"target", "t"}}});
      const Rig r = parseRig("a", d);
      const Pose p = samplePose(r, -1, 0);
      const Affine& w = p.world[1];
      check(pointsAt(w.a, w.b, 90 - w.e, 140 - w.f, 1e-9), std::string(mode) + ": aim axis points at the target");
      d["ik"][0]["stretch"] = true;
      const Rig rs = parseRig("a", d);
      const Pose ps = samplePose(rs, -1, 0);
      double tx, ty;
      ps.world[1].apply(20, 0, tx, ty);
      check(std::hypot(tx - 90, ty - 140) < 1e-6, std::string(mode) + ": stretch lands the tip on the target");
    }
    // Two bones: a sheared upper (normal) and an onlyTranslation / noScale lower.
    for (const char* mode : {"onlyTranslation", "noScale", "noScaleOrReflection"}) {
      Json d = armRig(70, 40);
      d["bones"][0]["rotation"] = 20; d["bones"][0]["scaleX"] = 1.4; d["bones"][0]["scaleY"] = 0.8;
      d["bones"][1]["shearY"] = 15;
      d["bones"][2]["inherit"] = mode;
      const Rig r = parseRig("a", d);
      const Pose p = samplePose(r, -1, 0);
      const Affine& t = p.world[4];
      check(std::hypot(p.world[3].e - t.e, p.world[3].f - t.f) <= IK_CHAIN_TOLERANCE, std::string("two-bone with ") + mode + " lower: the hand reaches the target");
      // Both sides reachable: a uniform root (a non-uniform one changes the reach with the direction).
      Json flip = d;
      flip["bones"][0]["scaleY"] = 1.4;
      const Pose p2 = samplePose(parseRig("a", flip), -1, 0);
      flip["ik"][0]["bendPositive"] = false;
      const Pose q = samplePose(parseRig("a", flip), -1, 0);
      const double side = [&](const Pose& x) { return (x.world[2].e - x.world[1].e) * (x.world[4].f - x.world[1].f) - (x.world[2].f - x.world[1].f) * (x.world[4].e - x.world[1].e); }(p2);
      const double other = [&](const Pose& x) { return (x.world[2].e - x.world[1].e) * (x.world[4].f - x.world[1].f) - (x.world[2].f - x.world[1].f) * (x.world[4].e - x.world[1].e); }(q);
      check(std::hypot(q.world[3].e - q.world[4].e, q.world[3].f - q.world[4].f) <= IK_CHAIN_TOLERANCE && std::hypot(p2.world[3].e - p2.world[4].e, p2.world[3].f - p2.world[4].f) <= IK_CHAIN_TOLERANCE && side * other < 0,
            std::string(mode) + ": bendPositive picks the other side");
    }
    // FABRIK with a noScale and a sheared link.
    Json c = ikphys::chainRig(170, 130);
    c["bones"][2]["inherit"] = "noScale";
    c["bones"][4]["shearX"] = 20;
    c["bones"][0]["scaleX"] = 1.3; c["bones"][0]["scaleY"] = 1.3;
    const Rig rc = parseRig("c", c);
    const Pose pc = samplePose(rc, -1, 0);
    double tipX, tipY;
    pc.world[size_t(rc.boneIndex("b3"))].apply(40, 0, tipX, tipY);
    const Affine& goal = pc.world[size_t(rc.boneIndex("t"))];
    check(std::hypot(tipX - goal.e, tipY - goal.f) <= 0.05 && pc.ikSolve[0].solved, "chain with shaped links: the tip reaches the target");
  });

  // ---------------------------------------------------------------- transform constraints
  test("transform: world absolute follows the target's place and turn (offsets, partial mixes)", [] {
    Json d = constraintRig();
    d["transform"] = Json::array({{{"id", "tc"}, {"bones", {"b"}}, {"target", "t"}}});
    check(validateRig(d).empty(), "validates");
    Rig r = parseRig("t", d);
    Pose p = samplePose(r, -1, 0);
    const Affine& T = p.world[1];
    const Affine& B = p.world[2];
    check(near(B.e, T.e) && near(B.f, T.f), "origin = target origin");
    check(near(wrap(angleOf(B.a, B.b) - angleOf(T.a, T.b)), 0), "x axis turned onto the target's");
    check(near(std::hypot(B.a, B.b), 1) && near(std::hypot(B.c, B.d), 1), "scale mixes default 0: own size kept");
    check(sameM(p.world[3], B * Affine::compose(30, 0, 5, 1, 1)), "the child follows the constrained bone");
    check(sameM(childWorld(p.world[0], p.local(2), Inherit::Normal), B), "applied local values rebuild the constrained world");
    check(p.bones[2].rotation == -10 && p.bones[2].x == -50 && near(p.local(2).x, 100, 1e-9) && near(p.local(2).y, 50, 1e-9), "animated values untouched; applied moved");
    d["transform"][0]["offsetRotation"] = 90; d["transform"][0]["offsetX"] = 10; d["transform"][0]["offsetY"] = -4;
    p = samplePose(parseRig("t", d), -1, 0);
    double ex, ey;
    p.world[1].apply(10, -4, ex, ey);
    check(near(p.world[2].e, ex) && near(p.world[2].f, ey) && near(wrap(angleOf(p.world[2].a, p.world[2].b) - angleOf(T.a, T.b) - 90), 0), "offsets: in the target's space / added angle");
    d["transform"][0].erase("offsetRotation"); d["transform"][0].erase("offsetX"); d["transform"][0].erase("offsetY");
    d["transform"][0]["mixRotate"] = 0.5; d["transform"][0]["mixX"] = 0.25; d["transform"][0]["mixY"] = 0;
    p = samplePose(parseRig("t", d), -1, 0);
    const double before = -10, full = angleOf(T.a, T.b);
    check(near(angleOf(p.world[2].a, p.world[2].b), before + wrap(full - before) * 0.5, 1e-9), "mixRotate 0.5: halfway (short way)");
    check(near(p.world[2].e, 150 + (T.e - 150) * 0.25) && near(p.world[2].f, 220), "mixX 0.25, mixY 0");
    d["transform"][0] = {{"id", "tc"}, {"bones", {"b"}}, {"target", "t"}, {"mixRotate", 0}, {"mixX", 0}, {"mixY", 0}, {"mixScaleX", 1}, {"mixScaleY", 1}, {"mixShearY", 1}};
    p = samplePose(parseRig("t", d), -1, 0);
    const Affine& S = p.world[2];
    auto skew = [](const Affine& m) { return wrap(angleOf(m.c, m.d) - angleOf(m.a, m.b) - 90); };
    check(near(std::hypot(S.a, S.b), 1.5) && near(std::hypot(S.c, S.d), 0.8) && near(skew(S), skew(T), 1e-9) && near(skew(T), 12, 1e-9), "scale and shearY copied (x axis kept)");
    check(near(angleOf(S.a, S.b), -10, 1e-9) && near(S.e, 150) && near(p.local(2).shearY, 12, 1e-9), "rotation/place untouched; skew landed in shearY");
  });
  test("transform: relative world and local / relative local", [] {
    Json d = constraintRig();
    d["transform"] = Json::array({{{"id", "tc"}, {"bones", {"b"}}, {"target", "t"}, {"relative", true}, {"mixScaleX", 1}, {"offsetX", 2}}});
    Rig r = parseRig("t", d);
    Pose p = samplePose(r, -1, 0);
    const Affine& T = p.world[1];
    double ox, oy;
    T.apply(2, 0, ox, oy);
    check(near(angleOf(p.world[2].a, p.world[2].b), wrap(-10 + angleOf(T.a, T.b)), 1e-9), "relative world: adds the target's world angle");
    check(near(p.world[2].e, 150 + ox) && near(p.world[2].f, 220 + oy), "relative world: adds the target's world point (offset in its space)");
    check(near(std::hypot(p.world[2].a, p.world[2].b), 1.5), "relative world scale: × target scale");
    d["transform"][0] = {{"id", "tc"}, {"bones", {"b"}}, {"target", "t"}, {"local", true}, {"mixScaleX", 1}, {"mixShearY", 0.5}, {"offsetRotation", 3}};
    p = samplePose(parseRig("t", d), -1, 0);
    check(near(p.local(2).rotation, 43) && near(p.local(2).x, 100) && near(p.local(2).y, 50) && near(p.local(2).scaleX, 1.5) && near(p.local(2).shearY, 6), "local absolute: the target's local values (+offset)");
    check(sameM(p.world[2], p.world[0] * localMatrix(p.local(2))), "local: world rebuilt from the new local values");
    d["transform"][0] = {{"id", "tc"}, {"bones", {"b"}}, {"target", "t"}, {"local", true}, {"relative", true}, {"mixRotate", 0.5}, {"mixScaleX", 1}, {"offsetX", 1}};
    p = samplePose(parseRig("t", d), -1, 0);
    check(near(p.local(2).rotation, -10 + 20) && near(p.local(2).x, -50 + 101) && near(p.local(2).y, 20 + 50) && near(p.local(2).scaleX, 1.5), "local relative: adds the target's values");
  });
  test("transform: one order with IK (stable: IK first), keys on mixes, several bones", [] {
    Json d = constraintRig();
    d["ik"] = Json::array({{{"id", "aimB"}, {"bones", {"b"}}, {"target", "aim"}}});
    d["transform"] = Json::array({{{"id", "copyB"}, {"bones", {"d"}}, {"target", "b"}, {"local", true}, {"mixX", 0}, {"mixY", 0}}});
    Rig r = parseRig("o", d);
    check(r.constraintOrder.size() == 2 && !r.constraintOrder[0].transform && r.constraintOrder[1].transform, "equal order: IK before the transform constraint");
    Pose p = samplePose(r, -1, 0);
    check(near(p.local(5).rotation, p.local(2).rotation, 1e-9) && !near(p.local(2).rotation, -10), "d copies the IK-solved rotation of b");
    d["transform"][0]["order"] = -1;
    r = parseRig("o", d);
    check(r.constraintOrder[0].transform, "order -1: the transform constraint first");
    p = samplePose(r, -1, 0);
    check(near(p.local(5).rotation, -10, 1e-9), "solved before the IK: the animated rotation");
    check(hasCode(validateRig(d), "ANIM_TRANSFORM_ORDER"), "warning: the IK moves its target later");
    d["ik"][0]["order"] = -2;
    check(!hasCode(validateRig(d), "ANIM_TRANSFORM_ORDER"), "no warning in dependency order");
    // Keys on the mixes.
    Json k = constraintRig();
    k["transform"] = Json::array({{{"id", "tc"}, {"bones", {"b", "d"}}, {"target", "t"}}});
    k["clips"] = Json::parse(R"([{"id": "in", "durationMs": 1000, "loop": false, "tracks": [
      {"target": "transform:tc", "property": "mixRotate", "keys": [{"timeMs": 0, "value": 0}, {"timeMs": 1000, "value": 1}]},
      {"target": "transform:tc", "property": "mixX", "keys": [{"timeMs": 0, "value": 0, "interp": "step"}, {"timeMs": 600, "value": 1}]},
      {"target": "transform:tc", "property": "mixY", "keys": [{"timeMs": 0, "value": 0}]}]}])");
    check(validateRig(k).empty(), "mix tracks validate");
    r = parseRig("k", k);
    p = samplePose(r, 0, 0);
    check(near(p.world[2].e, 150) && near(angleOf(p.world[2].a, p.world[2].b), -10), "mixes keyed 0: nothing moves");
    p = samplePose(r, 0, 500);
    const double target = angleOf(p.world[1].a, p.world[1].b);
    check(near(p.transformMix[0][0], 0.5) && near(angleOf(p.world[2].a, p.world[2].b), -10 + wrap(target + 10) * 0.5, 1e-9) && near(p.world[2].e, 150), "halfway: rotate 0.5, x still 0 (step)");
    check(near(p.world[5].e, 200) && near(angleOf(p.world[5].a, p.world[5].b), wrap(target) * 0.5, 1e-9), "every listed bone follows");
    p = samplePose(r, 0, 700);
    check(near(p.world[2].e, p.world[1].e) && near(p.world[2].f, 220), "x after the step key: 1; y keyed 0");
    const Json reply = Host().handle({{"op", "sample"}, {"rig", k}, {"clip", "in"}, {"timeMs", 500}});
    check(near(reply["pose"]["transform"]["tc"]["mixRotate"].get<double>(), 0.5) && reply["pose"]["bones"]["b"].contains("applied"), "sample reports mixes and applied values");
    // Cross-fade of two clips blends the mixes.
    k["clips"].push_back(Json::parse(R"({"id": "off", "durationMs": 1000, "loop": true, "tracks": [{"target": "transform:tc", "property": "mixRotate", "keys": [{"timeMs": 0, "value": 0}]}]})"));
    Instance mix(std::make_shared<const Rig>(parseRig("k", k)));
    mix.play(0, "off");
    mix.update(100);
    mix.play(0, "in", std::nullopt, 1, Then::Default, 400);
    mix.update(200);
    check(mix.pose().transformMix[0][0] > 0 && mix.pose().transformMix[0][0] < 0.2, "a cross-fade blends the mixes");
  });
  test("transform: validation diagnostics", [] {
    Json d = constraintRig();
    auto with = [&](Json k) { Json x = d; x["transform"] = Json::array({k}); return validateRig(x); };
    const Json base = {{"id", "tc"}, {"bones", {"b"}}, {"target", "t"}};
    check(with(base).empty(), "valid");
    Json k = base; k["bones"] = {"nope"};
    check(hasCode(with(k), "ANIM_TRANSFORM_BONE_MISSING"), "unknown constrained bone");
    k = base; k["target"] = "nope";
    check(hasCode(with(k), "ANIM_TRANSFORM_BONE_MISSING"), "unknown target");
    k = base; k["bones"] = Json::array();
    check(hasCode(with(k), "ANIM_TRANSFORM_BONES"), "no bones");
    k = base; k["bones"] = {"b", "b"};
    check(hasCode(with(k), "ANIM_TRANSFORM_BONES"), "a bone twice");
    k = base; k["target"] = "b";
    check(hasCode(with(k), "ANIM_TRANSFORM_TARGET"), "the target among the bones");
    k = base; k["target"] = "c";
    auto ds = with(k);
    bool errors = false;
    for (const auto& x : ds) errors = errors || x.severity == "error";
    check(hasCode(ds, "ANIM_TRANSFORM_TARGET_DESCENDANT") && !errors, "a target below a constrained bone: warning only");
    k = base; k["mixShearY"] = 1.5;
    check(hasCode(with(k), "ANIM_TRANSFORM_MIX"), "mix range");
    k = base; k["offsetX"] = "x";
    check(hasCode(with(k), "ANIM_NONFINITE"), "offsets are numbers");
    k = base; k["local"] = 1;
    check(hasCode(with(k), "ANIM_STRUCTURE"), "local is a boolean");
    Json two = d;
    two["transform"] = Json::array({base, base});
    check(hasCode(validateRig(two), "ANIM_TRANSFORM_DUP_ID"), "duplicate ids");
    two = d;
    two["transform"] = Json::array({base});
    two["clips"] = Json::parse(R"([{"id": "c", "durationMs": 100, "tracks": [{"target": "transform:tc", "property": "mix", "keys": [{"timeMs": 0, "value": 1}]}]}])");
    check(hasCode(validateRig(two), "ANIM_PROPERTY"), "mix is not a transform property");
    two["clips"][0]["tracks"][0] = Json::parse(R"({"target": "bone:b", "property": "mixRotate", "keys": [{"timeMs": 0, "value": 1}]})");
    check(hasCode(validateRig(two), "ANIM_PROPERTY"), "mixRotate on a bone track");
    two["clips"][0]["tracks"][0] = Json::parse(R"({"target": "transform:zz", "property": "mixRotate", "keys": [{"timeMs": 0, "value": 1}]})");
    check(hasCode(validateRig(two), "ANIM_TARGET_MISSING"), "unknown constraint track");
    two["clips"][0]["tracks"][0] = Json::parse(R"({"target": "transform:tc", "property": "mixX", "keys": [{"timeMs": 0, "value": 2}]})");
    check(hasCode(validateRig(two), "ANIM_TRANSFORM_MIX"), "key value range");
  });
  test("transform: a descendant target is read before the constraint moves it", [] {
    Json d = constraintRig();
    d["transform"] = Json::array({{{"id", "tc"}, {"bones", {"b"}}, {"target", "c"}, {"mixRotate", 0}}});
    const Pose before = samplePose(parseRig("x", constraintRig()), -1, 0);
    const Pose p = samplePose(parseRig("x", d), -1, 0);
    check(near(p.world[2].e, before.world[3].e) && near(p.world[2].f, before.world[3].f), "b reaches where c was");
    check(!near(p.world[3].e, p.world[2].e), "c then moves with b (no cycle, one pass)");
  });

  // ---------------------------------------------------------------- physics, meshes, clipping, frames
  test("physics, weighted meshes and clipping honour shear and inherit", [] {
    Json d = parityRig();
    check(validateRig(d).empty(), "parity rig validates");
    const Rig r = parseRig("p", d);
    // Bind pose: a weighted mesh at the setup pose sits where its unweighted placement does.
    Pose setup = samplePose(r, -1, 0, Look{-1, -1, true, false});
    const int slot = r.slotIndex("s-body");
    const auto* a = r.attachment("s-body", "torso");
    const auto positions = meshPositions(r, setup, slot, *a);
    const Affine rigid = setup.world[size_t(r.slots[size_t(slot)].boneIndex)] * attachmentPlacement(*a);
    bool bind = positions.size() == 8;
    for (size_t v = 0; bind && v < 4; v++) { double x, y; rigid.apply(a->vertices[2 * v], a->vertices[2 * v + 1], x, y); bind = near(positions[2 * v], x, 1e-9) && near(positions[2 * v + 1], y, 1e-9); }
    check(bind, "sheared / noScale bind pose: weighted = rigid at setup");
    // Physics swings the noScale tail; its sheared child is recomputed through the mode.
    Instance inst(std::make_shared<const Rig>(r));
    inst.play(0, "walk");
    for (int i = 0; i < 30; i++) inst.update(1000.0 / 60);
    const Pose p = inst.pose();
    const int t0 = r.boneIndex("tail0"), t1 = r.boneIndex("tail1");
    check(sameM(p.world[size_t(t1)], childWorld(p.world[size_t(t0)], p.local(size_t(t1)), r.bones[size_t(t1)].inherit), 1e-9), "physics descendants: childWorld of the swung bone");
    check(inst.physics().steps > 20, "physics simulated");
    // Clipping polygon on a sheared bone follows its world.
    Json c = clipseq::clipRig();
    c["bones"][1]["shearX"] = 25;
    c["bones"][1]["inherit"] = "noScaleOrReflection";
    const Rig rc = parseRig("c", c);
    const Pose pc = samplePose(rc, -1, 0);
    bool clipFollows = false;
    for (size_t s = 0; s < rc.slots.size(); s++) {
      const auto* att = pc.attachments[s] ? rc.attachment(rc.slots[s].id, *pc.attachments[s]) : nullptr;
      if (!att || !att->clipping) continue;
      const auto poly = meshPositions(rc, pc, int(s), *att);
      double x, y;
      (pc.world[size_t(rc.slots[s].boneIndex)] * attachmentPlacement(*att)).apply(att->vertices[0], att->vertices[1], x, y);
      clipFollows = near(poly[0], x, 1e-9) && near(poly[1], y, 1e-9);
    }
    check(clipFollows, "clip polygon placed by the shaped bone's world");
    // Compact frames agree with documented items.
    Host host;
    host.handle({{"op", "init"}, {"frameFormats", {2}}, {"documents", Json::array({{{"type", "rig"}, {"id", "s"}, {"data", d}}, {{"type", "controller"}, {"id", "c"}, {"data", {{"schemaVersion", 1}, {"rig", "s"}, {"defaultClip", "walk"}}}}})}});
    host.handle({{"op", "mount"}, {"instance", "a"}, {"element", {{"type", "character"}, {"ref", "c"}, {"width", 400}, {"height", 400}}}});
    host.handle({{"op", "update"}, {"dtMs", 250}});
    const Json doc = host.handle({{"op", "frame"}, {"instance", "a"}}), packed = host.handle({{"op", "frame"}, {"instance", "a"}, {"format", 2}});
    bool agree = doc["items"].size() == packed["items"].size() && !doc["items"].empty();
    for (size_t k = 0; agree && k < doc["items"].size(); k++)
      for (int j = 0; j < 6; j++) agree = agree && near(packed["items"][k][1 + j].get<double>(), doc["items"][k]["matrix"][j].get<double>(), j < 4 ? 6e-7 : 6e-4);
    check(agree, "compact frame matrices = documented (1/1000 px)");
  });
  test("transform + physics + shear: frames are identical bits at any pacing", [] {
    const std::vector<double> cps = {250, 500, 1000, 1500};
    const auto a = runPartition(parityRig(), "walk", cps, [](int) { return 1000.0 / 60; });
    const auto b = runPartition(parityRig(), "walk", cps, [](int i) { const double s[] = {7.3, 11.1, 3.9, 20.0, 0.25, 33.0, 5.45}; return s[i % 7]; });
    const auto c = runPartition(parityRig(), "walk", cps, [](int) { return 1000.0 / 144; });
    check(a == b && a == c, "60 Hz = irregular = 144 Hz");
  });

  // ---------------------------------------------------------------- editing ops
  test("edit: setBone shear / inherit, keys, copy/paste pose", [] {
    Json d = inheritRig("normal");
    auto r = applyEdit(d, {{"op", "setBone"}, {"bone", "child"}, {"shearX", 12.5}, {"inherit", "noScale"}});
    const Json& b = r.data["bones"][1];
    check(b["shearX"] == 12.5 && !b.contains("shearY") && b["inherit"] == "noScale", "fields written");
    r = applyEdit(r.data, {{"op", "setBone"}, {"bone", "child"}, {"shearX", 0}, {"inherit", nullptr}});
    check(!r.data["bones"][1].contains("shearX") && !r.data["bones"][1].contains("inherit"), "0 / null drop the keys");
    bool refused = false;
    try { applyEdit(d, {{"op", "setBone"}, {"bone", "child"}, {"inherit", "up"}}); } catch (const EditError&) { refused = true; }
    check(refused, "unknown inherit refused");
    d["clips"] = Json::parse(R"([{"id": "c", "durationMs": 1000, "tracks": []}])");
    r = applyEdit(d, {{"op", "setKey"}, {"clip", "c"}, {"target", "bone:child"}, {"property", "shearY"}, {"timeMs", 200}, {"value", -9}});
    check(r.data["clips"][0]["tracks"][0]["property"] == "shearY", "shear keys");
    const Json pose = applyEdit(r.data, {{"op", "copyPose"}, {"clip", "c"}, {"timeMs", 500}}).extra["pose"];
    check(pose[1]["shearY"] == -9 && !pose[1].contains("shearX") && !pose[0].contains("shearY"), "copyPose carries non-zero shear only");
    r = applyEdit(r.data, {{"op", "pastePose"}, {"clip", "c"}, {"timeMs", 800}, {"pose", Json::array({{{"bone", "child"}, {"shearX", 4}}})}});
    check(r.data["clips"][0]["tracks"].size() == 2, "pastePose keys shear");
  });
  test("edit: world-keeping ops invert the inherit mode", [] {
    for (const char* mode : {"onlyTranslation", "noRotationOrReflection", "noScale", "noScaleOrReflection"}) {
      Json d = inheritRig(mode);
      d["bones"][1]["shearX"] = 7;
      d["bones"].push_back({{"id", "other"}, {"parent", nullptr}, {"x", -30}, {"y", 40}, {"rotation", -50}, {"scaleX", 1.5}, {"scaleY", 0.75}});
      const Affine before = worldOf(d, "child");
      Json moved = applyEdit(d, {{"op", "reparentBone"}, {"bone", "child"}, {"parent", "other"}}).data;
      check(sameM(worldOf(moved, "child"), before, 1e-5), std::string(mode) + ": reparent keeps the world pose");
      Json up = applyEdit(d, {{"op", "reparentBone"}, {"bone", "child"}, {"parent", nullptr}}).data;
      check(sameM(worldOf(up, "child"), before, 1e-5), std::string(mode) + ": to root keeps it");
      // Compensation: turning the parent keeps the child where it was.
      Json comp = applyEdit(d, {{"op", "setBone"}, {"bone", "root"}, {"rotation", 75}, {"scaleX", 1.2}, {"compensateBones", true}}).data;
      check(sameM(worldOf(comp, "child"), before, 1e-5), std::string(mode) + ": compensated child stays");
      // Deleting the parent keeps the child's world.
      Json mid = d;
      mid["bones"][1]["parent"] = "other";
      mid["bones"][2]["parent"] = "root";
      const Affine was = worldOf(mid, "child");
      Json del = applyEdit(mid, {{"op", "deleteBone"}, {"bone", "other"}}).data;
      check(sameM(worldOf(del, "child"), was, 1e-5), std::string(mode) + ": deleteBone keeps the children's world");
      // Moving the pivot keeps the bone's axes and its children.
      Json piv = d;
      piv["bones"].push_back({{"id", "kid"}, {"parent", "child"}, {"x", 9}, {"y", 3}});
      const Affine kid = worldOf(piv, "kid");
      Json pivoted = applyEdit(piv, {{"op", "setPivot"}, {"bone", "child"}, {"localX", 4}, {"localY", -2}}).data;
      const Affine pc = worldOf(pivoted, "child");
      double wx, wy;
      before.apply(4, -2, wx, wy);
      check(near(pc.e, wx, 1e-5) && near(pc.f, wy, 1e-5) && near(pc.a, before.a, 1e-9) && sameM(worldOf(pivoted, "kid"), kid, 1e-5), std::string(mode) + ": setPivot");
      // World axes: the shown world rotation is the on-screen angle of the bone's x axis.
      Json w = applyEdit(d, {{"op", "setBone"}, {"bone", "child"}, {"axes", "world"}, {"rotation", 33}}).data;
      const Rig rw = parseRig("w", w);
      Transform noShear = rw.bones[1].setup;
      noShear.shearX = 0;
      const Affine bare = childWorld(worldOf(w, "root"), noShear, rw.bones[1].inherit);
      check(near(angleOf(bare.a, bare.b), 33, 1e-6), std::string(mode) + ": world axes rotation (the x axis before shear)");
    }
    // transformBones on an onlyTranslation bone under a mirror turns it on screen by +delta.
    Json m = inheritRig("onlyTranslation", {{"scaleX", -1}, {"scaleY", 1}});
    const Affine a0 = worldOf(m, "child");
    Json turned = applyEdit(m, {{"op", "transformBones"}, {"bones", {"child"}}, {"delta", {{"rotation", 20}}}}).data;
    const Affine a1 = worldOf(turned, "child");
    check(near(wrap(angleOf(a1.a, a1.b) - angleOf(a0.a, a0.b)), 20, 1e-9), "a mirror does not reverse an onlyTranslation turn");
    Json n = inheritRig("normal", {{"scaleX", -1}, {"scaleY", 1}});
    const Affine n0 = worldOf(n, "child");
    const Affine n1 = worldOf(applyEdit(n, {{"op", "transformBones"}, {"bones", {"child"}}, {"delta", {{"rotation", 20}}}}).data, "child");
    check(near(wrap(angleOf(n1.a, n1.b) - angleOf(n0.a, n0.b)), 20, 1e-9), "normal bones keep the 1.7 mirrored turn");
    // orientBones skips shaped bones.
    Json o = inheritRig("normal", {{"scaleX", 1}, {"scaleY", 1}});
    o["bones"][1]["shearX"] = 5;
    o["bones"].push_back({{"id", "kid"}, {"parent", "child"}, {"x", 20}, {"y", 10}});
    const Json res = applyEdit(o, {{"op", "orientBones"}, {"bones", {"root", "child"}}}).extra;
    check(res["skipped"].contains("child") && !res["oriented"].contains("child"), "orientBones skips a sheared bone");
  });
  test("edit: transform constraint ops (add keepPose, set, rename, keys, delete, deleteBone)", [] {
    Json d = constraintRig();
    d["clips"] = Json::parse(R"([{"id": "c", "durationMs": 1000, "tracks": []}])");
    const Pose before = samplePose(parseRig("x", d), -1, 0);
    for (const Json& mode : {Json{{"local", false}, {"relative", false}}, Json{{"local", true}, {"relative", false}}, Json{{"local", false}, {"relative", true}}, Json{{"local", true}, {"relative", true}}}) {
      Json op = {{"op", "addTransform"}, {"bones", {"b"}}, {"target", "t"}, {"keepPose", true}, {"mixScaleX", 1}, {"mixScaleY", 1}, {"mixShearY", 1}};
      for (auto it = mode.begin(); it != mode.end(); ++it) op[it.key()] = it.value();
      const auto r = applyEdit(d, op);
      const Pose p = samplePose(parseRig("x", r.data), -1, 0);
      check(r.extra["transform"] == "b-transform" && sameM(p.world[2], before.world[2], 1e-5), "keepPose: nothing moves (" + mode.dump() + ")");
    }
    // keepPose on a mode switch keeps the pose the constraint draws now (not the unconstrained one).
    {
      Json withK = applyEdit(d, {{"op", "addTransform"}, {"bones", {"b"}}, {"target", "t"}, {"mixRotate", 0.5}, {"mixX", 0.75}, {"mixScaleX", 0.3}, {"mixShearY", 1}}).data;
      const Pose drawn = samplePose(parseRig("x", withK), -1, 0);
      for (const Json& flags : {Json{{"local", true}}, Json{{"relative", true}}, Json{{"local", true}, {"relative", true}}}) {
        Json op = {{"op", "setTransform"}, {"transform", "b-transform"}, {"keepPose", true}};
        for (auto it = flags.begin(); it != flags.end(); ++it) op[it.key()] = it.value();
        const Pose p = samplePose(parseRig("x", applyEdit(withK, op).data), -1, 0);
        check(sameM(p.world[2], drawn.world[2], 1e-5), "keepPose switch keeps the drawn pose (" + flags.dump() + ")");
      }
    }
    auto r = applyEdit(d, {{"op", "addTransform"}, {"bones", {"b", "d"}}, {"target", "t"}, {"name", "따라가기"}, {"mixRotate", 0.5}});
    check(r.data["transform"][0]["name"] == "따라가기" && r.data["transform"][0]["mixRotate"] == 0.5 && !r.data["transform"][0].contains("mixX"), "defaults are not written");
    r = applyEdit(r.data, {{"op", "setTransform"}, {"transform", "b-transform"}, {"mixRotate", nullptr}, {"local", true}, {"offsetX", 3}, {"order", 2}});
    const Json& k = r.data["transform"][0];
    check(!k.contains("mixRotate") && k["local"] == true && k["offsetX"] == 3 && k["order"] == 2, "setTransform: null = default, flags, offsets, order");
    bool refused = false;
    try { applyEdit(r.data, {{"op", "setTransform"}, {"transform", "b-transform"}, {"target", "b"}}); } catch (const EditError&) { refused = true; }
    check(refused, "a target among the bones is refused");
    refused = false;
    try { applyEdit(r.data, {{"op", "setTransform"}, {"transform", "b-transform"}, {"mixX", 1.2}}); } catch (const EditError&) { refused = true; }
    check(refused, "mix range");
    r = applyEdit(r.data, {{"op", "setKey"}, {"clip", "c"}, {"target", "transform:b-transform"}, {"property", "mixRotate"}, {"timeMs", 0}, {"value", 0.25}, {"interp", "easeIn"}});
    check(r.data["clips"][0]["tracks"][0]["target"] == "transform:b-transform" && r.data["clips"][0]["tracks"][0]["keys"][0]["interp"] == "bezier", "mix keys (bezier presets)");
    refused = false;
    try { applyEdit(r.data, {{"op", "setKey"}, {"clip", "c"}, {"target", "transform:b-transform"}, {"property", "mixY"}, {"timeMs", 0}, {"value", 2}}); } catch (const EditError&) { refused = true; }
    check(refused, "key range");
    refused = false;
    try { applyEdit(r.data, {{"op", "setKey"}, {"clip", "c"}, {"target", "bone:b"}, {"property", "mixRotate"}, {"timeMs", 0}, {"value", 1}}); } catch (const EditError&) { refused = true; }
    check(refused, "mix keys only on transform targets");
    r = applyEdit(r.data, {{"op", "renameTransform"}, {"transform", "b-transform"}, {"id", "follow"}});
    check(r.data["transform"][0]["id"] == "follow" && r.data["clips"][0]["tracks"][0]["target"] == "transform:follow" && r.extra["tracks"] == 1, "rename renames the tracks");
    // deleteBone: a constrained bone leaves the list; the target needs cascade.
    auto gone = applyEdit(r.data, {{"op", "deleteBone"}, {"bone", "d"}});
    check(gone.data["transform"][0]["bones"] == Json::array({"b"}) && hasCode(gone.diagnostics, "ANIM_TRANSFORM_BONE_REMOVED"), "constrained bone removed from the list");
    refused = false;
    try { applyEdit(r.data, {{"op", "deleteBone"}, {"bone", "t"}}); } catch (const EditError&) { refused = true; }
    check(refused, "deleting the target needs cascade");
    auto cascade = applyEdit(r.data, {{"op", "deleteBone"}, {"bone", "t"}, {"cascade", true}});
    check(!cascade.data.contains("transform") && cascade.data["clips"][0]["tracks"].empty() && hasCode(cascade.diagnostics, "ANIM_TRANSFORM_REMOVED"), "cascade removes the constraint and its keys");
    auto del = applyEdit(r.data, {{"op", "deleteTransform"}, {"transform", "follow"}});
    check(!del.data.contains("transform") && del.extra["keys"] == 1 && del.data["clips"][0]["tracks"].empty(), "deleteTransform");
    // The host edit op (LLM / MCP path) runs the same operations.
    const Json viaHost = Host().handle({{"op", "edit"}, {"rig", d}, {"operation", {{"op", "addTransform"}, {"bones", {"b"}}, {"target", "t"}}}});
    check(viaHost["ok"] == true && viaHost["rig"]["transform"][0]["id"] == "b-transform" && viaHost["transform"] == "b-transform", "host edit op");
  });
}
}  // namespace xform
