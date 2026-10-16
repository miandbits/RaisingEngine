// Bounding boxes and points (plugin 1.17.0), included by core-tests.cpp after the other fixtures.
// Also the --box-parity script (tests/plugins/cutout-box-parity.test.mjs).
#pragma once

namespace boxes {
using cutout::Json;
using cutout::Affine;
constexpr double D2R = 3.14159265358979323846 / 180;

/** 300x300 canvas, origin (150, 280). `body` points up; `head` at its end, `arm` sideways (+x)
 *  from its middle, raised into the head box by `wave` (300 ms). Slots back to
 *  front: torso (picture), arm (picture), head-box (box on head), arm-box (box on arm, overlapping
 *  head-box where the arm crosses), hand (point on arm), face (point on head). */
Json boxRig() {
  return Json::parse(R"({
    "schemaVersion": 1, "canvas": {"width": 300, "height": 300, "originX": 150, "originY": 280},
    "bones": [
      {"id": "root", "parent": null, "x": 150, "y": 280},
      {"id": "body", "parent": "root", "y": -100, "length": 60, "rotation": -90},
      {"id": "head", "parent": "body", "x": 80},
      {"id": "arm", "parent": "body", "x": 30, "rotation": 90, "length": 70}
    ],
    "slots": [
      {"id": "torso", "bone": "body", "attachment": "torso"},
      {"id": "arm", "bone": "arm", "attachment": "arm"},
      {"id": "head-box", "bone": "head", "attachment": "head"},
      {"id": "arm-box", "bone": "arm", "attachment": "arm"},
      {"id": "hand", "bone": "arm", "attachment": "hand"},
      {"id": "face", "bone": "head", "attachment": "face"}
    ],
    "attachments": [
      {"slot": "torso", "name": "torso", "imageAssetId": "img-torso", "width": 60, "height": 100, "rotation": 90},
      {"slot": "arm", "name": "arm", "imageAssetId": "img-arm", "width": 70, "height": 16, "pivotX": 0},
      {"slot": "head-box", "name": "head", "type": "boundingBox", "vertices": [-30, -30, 30, -30, 30, 30, -30, 30], "color": "#22c55e"},
      {"slot": "head-box", "name": "big", "type": "boundingBox", "vertices": [-60, -60, 60, -60, 60, 60, -60, 60]},
      {"slot": "arm-box", "name": "arm", "type": "boundingBox", "vertices": [0, -10, 70, -10, 70, 10, 0, 10]},
      {"slot": "hand", "name": "hand", "type": "point", "x": 70, "rotation": 15},
      {"slot": "face", "name": "face", "type": "point", "x": 10, "y": 5, "rotation": -90}
    ],
    "expressions": [{"id": "shy", "slots": {"head-box": null}}],
    "clips": [
      {"id": "idle", "durationMs": 1000, "loop": true, "tracks": [{"target": "bone:body", "property": "y", "keys": [{"timeMs": 0, "value": -100}, {"timeMs": 500, "value": -104}, {"timeMs": 1000, "value": -100}]}]},
      {"id": "wave", "durationMs": 600, "loop": false, "tracks": [{"target": "bone:arm", "property": "rotation", "keys": [{"timeMs": 0, "value": 90}, {"timeMs": 300, "value": 30}, {"timeMs": 600, "value": 150}]}]},
      {"id": "nod", "durationMs": 400, "loop": false, "tracks": [{"target": "bone:head", "property": "rotation", "keys": [{"timeMs": 0, "value": 0}, {"timeMs": 400, "value": 20}]}]},
      {"id": "hide", "durationMs": 400, "loop": false, "tracks": [{"target": "slot:head-box", "property": "attachment", "keys": [{"timeMs": 0, "value": null}]}]}
    ]
  })");
}
Json boxController() {
  return Json::parse(R"({"schemaVersion": 1, "rig": "boxes", "defaultClip": "idle",
    "bindings": [{"id": "pat-head", "signal": "partClicked", "match": {"box": "head"}, "clip": "nod", "track": 1},
                 {"id": "poke-arm", "signal": "partClicked", "match": {"slot": "arm-box"}, "clip": "wave", "track": 2}]})");
}
/** Rig-space point of bone `bone` local (x, y) in `pose`. */
std::pair<double, double> worldPoint(const cutout::Rig& rig, const cutout::Pose& pose, const std::string& bone, double x, double y) {
  double wx, wy;
  pose.world[size_t(rig.boneIndex(bone))].apply(x, y, wx, wy);
  return {wx, wy};
}
const cutout::BoxWorld* boxOf(const std::vector<cutout::BoxWorld>& list, const std::string& slot) {
  for (const auto& b : list) if (b.attachment && b.attachment->slot == slot) return &b;
  return nullptr;
}
bool errors(const std::vector<cutout::Diagnostic>& ds) { for (const auto& d : ds) if (d.severity == "error") return true; return false; }
Json without(Json d, const std::set<std::string>& slots) {
  auto& s = d["slots"];
  s.erase(std::remove_if(s.begin(), s.end(), [&](const Json& x) { return slots.count(x["id"].get<std::string>()) > 0; }), s.end());
  auto& a = d["attachments"];
  a.erase(std::remove_if(a.begin(), a.end(), [&](const Json& x) { return slots.count(x["slot"].get<std::string>()) > 0; }), a.end());
  d.erase("expressions");
  for (auto& c : d["clips"]) { auto& t = c["tracks"]; t.erase(std::remove_if(t.begin(), t.end(), [](const Json& x) { return x["target"] == "slot:head-box"; }), t.end()); }
  return d;
}
Json mountOp(const std::string& id, bool flip) {
  return {{"op", "mount"}, {"instance", id}, {"element", {{"type", "character"}, {"ref", "boxes-main"}, {"width", 600}, {"height", 300}, {"props", {{"scale", 1}, {"flipX", flip}}}}}};
}

/** Host op script for the Wasm parity test: two mounted instances (b mirrored and larger), clicks
 *  that hit, miss and dispatch, points/boxes at several times, compact frames. */
Json parityScript() {
  Json rig = boxRig();
  // A weighted box (follows head and arm half each) and a deform key on the arm box.
  rig["attachments"].push_back(Json::parse(R"({"slot": "torso", "name": "blend", "type": "boundingBox", "vertices": [0, 0, 20, 0, 20, 20], "weights": [
    [{"bone": "head", "weight": 0.5}, {"bone": "arm", "weight": 0.5}], [{"bone": "arm", "weight": 1}], [{"bone": "head", "weight": 0.7}, {"bone": "body", "weight": 0.6}]]})"));
  rig["slots"].push_back({{"id", "blend"}, {"bone", "body"}, {"attachment", "blend"}});
  rig["attachments"].back()["slot"] = "blend";
  rig["clips"][1]["tracks"].push_back(Json::parse(R"({"target": "slot:arm-box", "property": "deform", "attachment": "arm", "keys": [{"timeMs": 0, "offsets": [0,0,0,0,0,0,0,0]}, {"timeMs": 600, "offsets": [0,-5,10,-5,10,5,0,5]}]})"));
  Json script = Json::array({{{"op", "init"}, {"frameFormats", {2}}, {"documents", Json::array({{{"type", "rig"}, {"id", "boxes"}, {"data", rig}}, {{"type", "controller"}, {"id", "boxes-main"}, {"data", boxController()}}})}}});
  script.push_back(mountOp("a", false));
  Json mb = mountOp("b", true);
  mb["element"]["width"] = 900; mb["element"]["height"] = 450;
  script.push_back(mb);
  for (const char* id : {"a", "b"}) { script.push_back({{"op", "points"}, {"instance", id}}); script.push_back({{"op", "boxes"}, {"instance", id}}); }
  // Element px clicks: a's head box centre (rig (150, 100) = element (300, 100)), a miss, b's mirrored head.
  script.push_back({{"op", "hitTest"}, {"instance", "a"}, {"x", 300}, {"y", 100}});
  script.push_back({{"op", "hitTest"}, {"instance", "a"}, {"x", 20}, {"y", 20}, {"dispatch", true}});
  script.push_back({{"op", "hitTest"}, {"instance", "a"}, {"x", 300}, {"y", 100}, {"dispatch", true}});
  script.push_back({{"op", "hitTest"}, {"instance", "b"}, {"x", 450}, {"y", 150}, {"dispatch", true}});
  for (double dt : {16.7, 133.0, 200.0}) {
    script.push_back({{"op", "update"}, {"dtMs", dt}});
    for (const char* id : {"a", "b"}) {
      script.push_back({{"op", "points"}, {"instance", id}});
      script.push_back({{"op", "boxes"}, {"instance", id}});
      script.push_back({{"op", "frame"}, {"instance", id}, {"format", 2}});
    }
  }
  // The arm (raised by now) over the head box: the topmost box wins, and the arm's binding fires.
  script.push_back({{"op", "boxes"}, {"instance", "a"}});
  script.push_back({{"op", "describeAll"}});
  return script;
}

void run() {
  using namespace cutout;
  test("boxes: validation (vertices, finite, weights, names, unused fields, colour, sequences)", [] {
    check(!errors(validateRig(boxRig())), "the fixture is valid: no image needed for boxes / points");
    auto with = [](const std::function<void(Json&)>& change) { Json d = boxRig(); change(d); return validateRig(d); };
    auto box = [](Json& d) -> Json& { return d["attachments"][2]; };
    check(hasCode(with([&](Json& d) { box(d)["vertices"] = {0, 0, 1, 1}; }), "ANIM_BOX_VERTICES"), "fewer than 3 points refused");
    check(hasCode(with([&](Json& d) { box(d).erase("vertices"); }), "ANIM_BOX_VERTICES"), "vertices required");
    check(hasCode(with([&](Json& d) { box(d)["vertices"][3] = "x"; }), "ANIM_NONFINITE"), "non-numbers refused");
    check(hasCode(with([&](Json& d) { box(d)["vertices"][3] = 1e300; }), "ANIM_NONFINITE"), "huge values refused");
    const auto crossing = with([&](Json& d) { box(d)["vertices"] = {0, 0, 10, 10, 10, 0, 0, 10}; });
    check(hasCode(crossing, "ANIM_BOX_POLYGON") && !errors(crossing), "a crossing box is only a warning (even-odd hit test)");
    check(hasCode(with([&](Json& d) { d["attachments"][3]["name"] = "head"; }), "ANIM_DUP_ID"), "names unique per slot");
    check(hasCode(with([&](Json& d) { box(d)["imageAssetId"] = "x"; }), "ANIM_BOX_UNUSED"), "image fields on a box warn");
    check(hasCode(with([&](Json& d) { box(d)["end"] = "face"; }), "ANIM_BOX_UNUSED"), "end on a box warns");
    check(hasCode(with([&](Json& d) { box(d)["color"] = "green"; }), "ANIM_BOX_COLOR"), "colour format");
    check(hasCode(with([&](Json& d) { d["attachments"][5]["vertices"] = {0, 0, 1, 0, 1, 1}; }), "ANIM_POINT_UNUSED"), "point shape fields warn");
    check(hasCode(with([&](Json& d) { d["attachments"][5]["sequence"] = {{"frames", {"a"}}}; }), "ANIM_SEQUENCE_TYPE"), "no sequences without a picture");
    const auto heavy = with([&](Json& d) { box(d)["weights"] = Json::parse(R"([[{"bone":"head","weight":0.6},{"bone":"arm","weight":0.6}],[{"bone":"head","weight":1}],[{"bone":"head","weight":1}],[{"bone":"head","weight":1}]])"); });
    check(hasCode(heavy, "ANIM_MESH_WEIGHT_SUM") && !errors(heavy), "weights normalised (warning)");
    check(hasCode(with([&](Json& d) { box(d)["weights"] = Json::parse(R"([[{"bone":"nope","weight":1}],[{"bone":"head","weight":1}],[{"bone":"head","weight":1}],[{"bone":"head","weight":1}]])"); }), "ANIM_MESH_BONE_MISSING"), "weights name bones");
    check(hasCode(with([&](Json& d) { box(d)["weights"] = Json::array({Json::array({{{"bone", "head"}, {"weight", 1}}})}); }), "ANIM_MESH_WEIGHTS"), "one weight list per point");
    check(hasCode(with([&](Json& d) { box(d)["type"] = "hitbox"; }), "ANIM_MESH_TYPE"), "unknown types refused");
    const Rig r = parseRig("b", boxRig());
    const Attachment* a = r.attachment("head-box", "head");
    check(a && a->boundingBox && a->pictureless() && !a->mesh && a->color == "#22c55e" && a->width == 0 && a->imageAssetId.empty(), "parsed box");
    const Attachment* p = r.attachment("hand", "hand");
    check(p && p->point && p->pictureless() && p->vertices.empty() && near(p->offset.x, 70) && near(p->offset.rotation, 15), "parsed point");
    const Rig heavyRig = parseRig("h", [] { Json d = boxRig(); d["attachments"][2]["weights"] = Json::parse(R"([[{"bone":"head","weight":0.6},{"bone":"arm","weight":0.6}],[{"bone":"head","weight":1}],[{"bone":"head","weight":1}],[{"bone":"head","weight":1}]])"); return d; }());
    const auto& w = heavyRig.attachment("head-box", "head")->weights[0];
    check(w.size() == 2 && near(w[0].weight + w[1].weight, 1) && near(w[0].weight, 0.5), "normalised at load");
  });

  test("boxes: never drawn — draw lists and compact frames identical to the rig without them", [] {
    const Json withBoxes = boxRig(), plain = without(boxRig(), {"head-box", "arm-box", "hand", "face"});
    for (const char* clip : {"idle", "wave"}) {
      Instance a(std::make_shared<const Rig>(parseRig("a", withBoxes))), b(std::make_shared<const Rig>(parseRig("b", plain)));
      a.play(0, clip); b.play(0, clip);
      for (double dt : {0.0, 120.0, 260.0}) {
        a.update(dt); b.update(dt);
        Json ja = Json::array(), jb = Json::array();
        for (const auto& i : a.drawList()) ja.push_back(drawItemJson(i));
        for (const auto& i : b.drawList()) jb.push_back(drawItemJson(i));
        check(ja == jb && ja.size() == 2, std::string(clip) + ": the same two items");
      }
    }
    Host ha, hb;
    for (auto [h, d] : {std::pair<Host*, Json>{&ha, withBoxes}, {&hb, plain}}) {
      Json c = boxController();
      c["bindings"] = Json::array();
      h->handle({{"op", "init"}, {"documents", {{{"type", "rig"}, {"id", "boxes"}, {"data", d}}, {{"type", "controller"}, {"id", "boxes-main"}, {"data", c}}}}});
      h->handle(mountOp("i", false));
    }
    const std::string fa = ha.requestText(R"({"op":"frame","instance":"i","format":2})"), fb = hb.requestText(R"({"op":"frame","instance":"i","format":2})");
    check(fa == fb, "compact frame bytes unchanged");
    const Json mountA = ha.handle(mountOp("j", false)), mountB = hb.handle(mountOp("j", false));
    check(mountA.value("interactive", false) && !mountB.contains("interactive"), "mount reports interactive only for rigs with boxes");
    const Json init = hb.handle({{"op", "init"}, {"documents", Json::array()}});
    check(init["ops"] == Json::array({"points", "hitTest"}), "init lists the optional ops");
  });

  test("boxes: world polygons and points under bones, rotation, scale, mirror", [] {
    Json d = boxRig();
    d["bones"][2]["rotation"] = 30; d["bones"][2]["scaleX"] = 2; d["bones"][2]["scaleY"] = -1;  // head: turned, stretched, mirrored
    const Rig r = parseRig("b", d);
    const Pose pose = samplePose(r, -1, 0);
    const auto list = boxPolygons(r, pose);
    check(list.size() == 2 && list[0].attachment->slot == "head-box" && list[1].attachment->slot == "arm-box", "shown boxes in draw order");
    const auto [x0, y0] = worldPoint(r, pose, "head", -30, -30);
    check(near(list[0].polygon[0], x0) && near(list[0].polygon[1], y0) && list[0].polygon.size() == 8, "head box = head world · vertices");
    const auto pts = pointWorlds(r, pose);
    check(pts.size() == 2 && pts[0].attachment->name == "hand" && pts[1].attachment->name == "face", "points in draw order");
    const auto [fx, fy] = worldPoint(r, pose, "head", 10, 5);
    check(near(pts[1].x, fx) && near(pts[1].y, fy), "face point place");
    // Direction through the head's linear part (scale -1 mirrors it): compare with the mapped unit vector.
    const Affine& hw = pose.world[size_t(r.boneIndex("head"))];
    const double dx = hw.a * std::cos(-90 * D2R) + hw.c * std::sin(-90 * D2R), dy = hw.b * std::cos(-90 * D2R) + hw.d * std::sin(-90 * D2R);
    check(near(pts[1].rotation, std::atan2(dy, dx) / D2R), "face direction follows scale and mirror");
    const auto [hx, hy] = worldPoint(r, pose, "arm", 70, 0);
    check(near(pts[0].x, hx) && near(pts[0].y, hy) && near(std::remainder(pts[0].rotation - 15, 360.0), 0), "hand at the arm tip, arm direction (0°) + 15°");
  });

  test("boxes: weights, deform keys, IK, transform constraints and physics move them like meshes", [] {
    Json d = boxRig();
    // Weighted: point 0 half head half arm; deform key on the arm box.
    d["attachments"][2]["weights"] = Json::parse(R"([[{"bone":"head","weight":0.5},{"bone":"arm","weight":0.5}],[{"bone":"head","weight":1}],[{"bone":"head","weight":1}],[{"bone":"arm","weight":1}]])");
    d["clips"][1]["tracks"].push_back(Json::parse(R"({"target": "slot:arm-box", "property": "deform", "attachment": "arm", "keys": [{"timeMs": 0, "offsets": [0,0,0,0,0,0,0,0]}, {"timeMs": 600, "offsets": [0,0,20,0,20,0,0,0]}]})"));
    check(!errors(validateRig(d)), "weighted / deformed boxes are valid");
    const Rig r = parseRig("b", d);
    const Pose setup = samplePose(r, -1, 0), waved = samplePose(r, r.clipIndex("wave"), 300);
    const auto s0 = boxPolygons(r, setup), s1 = boxPolygons(r, waved);
    const auto [ex, ey] = worldPoint(r, setup, "head", -30, -30);
    check(near(s0[0].polygon[0], ex, 1e-9) && near(s0[0].polygon[1], ey, 1e-9), "bind pose = setup pose: a weighted point starts where it was drawn");
    // After the wave the arm moved: point 3 (all arm) follows the arm, point 0 halfway.
    const int arm = r.boneIndex("arm");
    const Affine armSkin = waved.world[size_t(arm)] * *setup.world[size_t(arm)].inverse();
    double ax, ay;
    armSkin.apply(s0[0].polygon[6], s0[0].polygon[7], ax, ay);
    check(near(s1[0].polygon[6], ax, 1e-9) && near(s1[0].polygon[7], ay, 1e-9), "an all-arm point follows the arm");
    double hx, hy;
    armSkin.apply(s0[0].polygon[0], s0[0].polygon[1], hx, hy);
    check(near(s1[0].polygon[0], (hx + s0[0].polygon[0]) / 2, 1e-9) && near(s1[0].polygon[1], (hy + s0[0].polygon[1]) / 2, 1e-9), "a half/half point moves half way (head did not move)");
    const auto [dx2, dy2] = worldPoint(r, waved, "arm", 70 + 10, -10);
    check(near(s1[1].polygon[2], dx2, 1e-9) && near(s1[1].polygon[3], dy2, 1e-9), "deform keys move box points (halfway: +10 px)");
    // IK: aim the arm at a target; the box follows the solved world.
    Json ik = boxRig();
    ik["bones"].push_back({{"id", "goal"}, {"parent", "root"}, {"x", 120}, {"y", -150}});
    ik["ik"] = Json::array({{{"id", "aim"}, {"bones", {"arm"}}, {"target", "goal"}}});
    const Rig ri = parseRig("i", ik);
    const Pose pi = samplePose(ri, -1, 0);
    const auto bi = boxPolygons(ri, pi);
    const auto [ix, iy] = worldPoint(ri, pi, "arm", 70, -10);
    check(near(boxOf(bi, "arm-box")->polygon[2], ix, 1e-9) && near(boxOf(bi, "arm-box")->polygon[3], iy, 1e-9), "IK-solved arm carries its box");
    const Affine& aw = pi.world[size_t(ri.boneIndex("arm"))];
    const double tox = 270 - aw.e, toy = 130 - aw.f;
    check(std::fabs(aw.a * toy - aw.b * tox) < 1e-9 * std::hypot(tox, toy) * std::hypot(aw.a, aw.b) && aw.a * tox + aw.b * toy > 0, "the arm aims at the goal");
    const auto hand = pointWorlds(ri, pi)[0];
    const auto [tx, ty] = worldPoint(ri, pi, "arm", 70, 0);
    check(near(hand.x, tx, 1e-9) && near(hand.y, ty, 1e-9), "point follows the solved arm");
    // Transform constraint: the head copies the arm's rotation.
    Json xf = boxRig();
    xf["transform"] = Json::array({{{"id", "copy"}, {"bones", {"head"}}, {"target", "arm"}, {"mixX", 0}, {"mixY", 0}}});
    const Rig rx = parseRig("x", xf);
    const Pose px = samplePose(rx, rx.clipIndex("wave"), 300);
    const auto [cx, cy] = worldPoint(rx, px, "head", 30, 30);
    check(near(boxOf(boxPolygons(rx, px), "head-box")->polygon[4], cx, 1e-9) && near(boxOf(boxPolygons(rx, px), "head-box")->polygon[5], cy, 1e-9), "constrained head carries its box");
    // Physics: the instance pose (with the simulation) is what boxes use.
    Json ph = boxRig();
    ph["physics"] = Json::array({{{"id", "swing"}, {"bone", "arm"}, {"rotate", 1}, {"inertia", 1}, {"strength", 40}, {"damping", 0.9}, {"gravity", 400}}});
    Instance inst(std::make_shared<const Rig>(parseRig("p", ph)));
    inst.play(0, "wave");
    for (int i = 0; i < 20; i++) inst.update(16.7);
    const Pose pp = inst.pose();
    const auto [qx, qy] = worldPoint(inst.rig(), pp, "arm", 70, 10);
    check(near(boxOf(boxPolygons(inst.rig(), pp), "arm-box")->polygon[4], qx, 1e-9) && near(boxOf(boxPolygons(inst.rig(), pp), "arm-box")->polygon[5], qy, 1e-9), "physics-swung arm carries its box");
    const Pose plainWave = samplePose(inst.rig(), inst.rig().clipIndex("wave"), 334);
    check(std::fabs(pp.world[size_t(inst.rig().boneIndex("arm"))].a - plainWave.world[size_t(inst.rig().boneIndex("arm"))].a) > 1e-6, "(the simulation really moved the arm)");
  });

  test("boxes: hit test = topmost by draw order; hidden slots, expressions, keys and skins", [] {
    const Rig r = parseRig("b", boxRig());
    const Pose pose = samplePose(r, -1, 0);
    const auto list = boxPolygons(r, pose);
    const auto [hx, hy] = worldPoint(r, pose, "head", 0, 0);
    check(hitBox(list, hx, hy) == 0, "head centre hits the head box");
    const auto [ax, ay] = worldPoint(r, pose, "arm", 60, 0);
    check(hitBox(list, ax, ay) == 1, "arm box alone");
    check(hitBox(list, 5, 5) < 0, "nothing far away");
    // The arm box over the head box: rotate the arm up into the head (wave at 300 ms = +60°).
    const Pose up = samplePose(r, r.clipIndex("wave"), 300);
    const auto upList = boxPolygons(r, up);
    bool overlap = false;
    for (double t = 5; t < 70 && !overlap; t += 1) {
      const auto [x, y] = worldPoint(r, up, "arm", t, 0);
      if (polygonContains(upList[0].polygon, x, y) && polygonContains(upList[1].polygon, x, y)) {
        overlap = true;
        check(hitBox(upList, x, y) == 1, "where both contain the point, the later slot (front) wins");
      }
    }
    check(overlap, "(the raised arm crosses the head box)");
    check(polygonContains({0, 0, 10, 0, 10, 10, 0, 10}, 0, 5) && !polygonContains({0, 0, 10, 0, 10, 10, 0, 10}, 10, 5), "half-open edges: left in, right out");
    check(polygonContains({0, 0, 10, 10, 10, 0, 0, 10}, 2, 5) && !polygonContains({0, 0, 10, 10, 10, 0, 0, 10}, 5, 2), "crossing polygon: even-odd");
    Look shy; shy.expression = r.expressionIndex("shy");
    const Pose hidden = samplePose(r, -1, 0, shy);
    check(boxPolygons(r, hidden).size() == 1 && boxPolygons(r, hidden)[0].attachment->slot == "arm-box", "an expression hiding the slot hides its box");
    check(boxPolygons(r, samplePose(r, r.clipIndex("hide"), 100)).size() == 1, "an attachment key to null hides it");
    // Swapping the shown attachment by key / skin: the bigger box, or a skin's own polygon.
    Json d = boxRig();
    d["clips"][3]["tracks"][0]["keys"][0]["value"] = "big";
    d["skins"] = Json::parse(R"([{"id": "tall", "attachments": [{"slot": "head-box", "name": "head", "type": "boundingBox", "vertices": [-10, -90, 10, -90, 10, 90, -10, 90]}]}])");
    const Rig rs = parseRig("s", d);
    const auto bigList = boxPolygons(rs, samplePose(rs, rs.clipIndex("hide"), 100));
    check(bigList[0].attachment->name == "big" && near(bigList[0].polygon.size(), 8), "attachment keys swap boxes");
    Look tall; tall.skin = rs.skinIndex("tall");
    const auto skinList = boxPolygons(rs, samplePose(rs, -1, 0, tall));
    const auto [kx, ky] = worldPoint(rs, samplePose(rs, -1, 0, tall), "head", 10, 90);
    check(near(skinList[0].polygon[4], kx) && near(skinList[0].polygon[5], ky), "the skin's own polygon");
    // Alpha does not hide a box (Spine semantics).
    Json faded = boxRig();
    faded["slots"][2]["alpha"] = 0;
    check(boxPolygons(parseRig("f", faded), samplePose(parseRig("f", faded), -1, 0)).size() == 2, "slot alpha 0 keeps the box");
  });

  test("boxes: host ops points / boxes / hitTest in frame space; partClicked dispatch", [] {
    Host host;
    host.handle({{"op", "init"}, {"documents", {{{"type", "rig"}, {"id", "boxes"}, {"data", boxRig()}}, {{"type", "controller"}, {"id", "boxes-main"}, {"data", boxController()}}}}});
    host.handle(mountOp("a", false));
    host.handle(mountOp("m", true));
    host.handle({{"op", "create"}, {"rig", "boxes"}, {"instance", "raw"}});
    // a: 600x300 element, 300x300 canvas centred: rig x + 150 = element x; head origin (150, 100) rig -> (300, 100).
    const Json pa = host.handle({{"op", "points"}, {"instance", "a"}});
    const Json face = pa["points"][1];
    check(pa["ok"] == true && face["slot"] == "face" && face["name"] == "face" && near(face["x"].get<double>(), 300 + 5) && near(face["y"].get<double>(), 100 - 10), "points: element px");
    check(near(face["rotation"].get<double>(), -180) || near(face["rotation"].get<double>(), 180), "points: rotation in element space (head up, -90° more)");
    const Json pm = host.handle({{"op", "points"}, {"instance", "m"}});
    check(near(pm["points"][1]["x"].get<double>(), 600 - 305) && near(pm["points"][1]["y"].get<double>(), 90) && near(pm["points"][1]["rotation"].get<double>(), 0), "flipX mirrors place and direction");
    const Json pr = host.handle({{"op", "points"}, {"instance", "raw"}});
    check(near(pr["points"][1]["x"].get<double>(), 5) && near(pr["points"][1]["y"].get<double>(), -190), "raw instance: rig-origin space like frame");
    const Json bx = host.handle({{"op", "boxes"}, {"instance", "a"}});
    check(bx["boxes"].size() == 2 && bx["boxes"][0]["slot"] == "head-box" && bx["boxes"][0]["polygon"].size() == 8, "boxes: element px polygons");
    const Json miss = host.handle({{"op", "hitTest"}, {"instance", "a"}, {"x", 5}, {"y", 5}, {"dispatch", true}});
    check(miss["ok"] == true && miss["hit"].is_null() && miss["hits"].empty() && miss["matched"].empty(), "a miss dispatches nothing");
    const Json hit = host.handle({{"op", "hitTest"}, {"instance", "a"}, {"x", 300}, {"y", 100}});
    check(hit["hit"] == Json({{"slot", "head-box"}, {"name", "head"}}) && !hit.contains("matched"), "hit without dispatch changes nothing");
    check(host.instance("a")->describe()["tracks"].size() == 1, "(still only the idle track)");
    const Json pat = host.handle({{"op", "hitTest"}, {"instance", "a"}, {"x", 300}, {"y", 100}, {"dispatch", true}});
    check(pat["matched"] == Json::array({"pat-head"}), "partClicked {box: head} matched");
    const Json tracks = host.instance("a")->describe()["tracks"];
    check(tracks.size() == 2 && tracks[1]["track"] == 1 && tracks[1]["clip"] == "nod", "the bound clip plays on its track");
    check(host.instance("m")->describe()["tracks"].size() == 1, "only the clicked instance reacts");
    const Json mirrored = host.handle({{"op", "hitTest"}, {"instance", "m"}, {"x", 300}, {"y", 100}, {"dispatch", true}});
    check(mirrored["hit"]["slot"] == "head-box", "mirrored element: the same box under the same (mirror-symmetric) point");
    const Json arm = host.handle({{"op", "hitTest"}, {"instance", "m"}, {"x", 600 - 340}, {"y", 150}, {"dispatch", true}});
    check(arm["hit"]["slot"] == "arm-box" && arm["matched"] == Json::array({"poke-arm"}), "match by slot");
    check(host.request(Json({{"op", "hitTest"}, {"instance", "a"}, {"x", "1"}, {"y", 2}}).dump())["ok"] == false, "x, y must be numbers");
    // Signals stay presentation: the dispatching hit only reaches this instance's controller.
    Instance free(std::make_shared<const Rig>(parseRig("b", boxRig())), parseController("c", boxController()));
    check(free.signal("partClicked", {{"box", "head"}, {"slot", "head-box"}}) == std::vector<std::string>{"pat-head"}, "the same signal through Instance::signal");
    Json c = boxController();
    c["bindings"][0]["match"]["box"] = "tail";
    const Rig r = parseRig("b", boxRig());
    const auto ds = validateController(c, &r);
    check(hasCode(ds, "ANIM_PART_UNKNOWN") && !errors(ds), "a partClicked match on a missing box warns");
    check(!hasCode(validateController(boxController(), &r), "ANIM_PART_UNKNOWN"), "known boxes do not");
  });

  test("boxes: editing ops addBoundingBox / setBoundingBox / addPoint / setPoint", [] {
    Json d = boxRig();
    auto edit = [&](const Json& op) { EditResult r = applyEdit(d, op); d = r.data; return r; };
    // From a part with its alpha mask (a 70x16 picture: opaque except the first 10 columns).
    std::string alpha(70 * 16, '\xff');
    for (int y = 0; y < 16; y++) for (int x = 0; x < 10; x++) alpha[size_t(y * 70 + x)] = 0;
    static const char* B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string b64;
    for (size_t i = 0; i < alpha.size(); i += 3) {
      const uint32_t v = (uint8_t(alpha[i]) << 16) | (i + 1 < alpha.size() ? uint8_t(alpha[i + 1]) << 8 : 0) | (i + 2 < alpha.size() ? uint8_t(alpha[i + 2]) : 0);
      b64 += B64[(v >> 18) & 63]; b64 += B64[(v >> 12) & 63];
      b64 += i + 1 < alpha.size() ? B64[(v >> 6) & 63] : '='; b64 += i + 2 < alpha.size() ? B64[v & 63] : '=';
    }
    const EditResult made = edit({{"op", "addBoundingBox"}, {"slot", "arm-hit"}, {"from", "arm"}, {"mask", {{"width", 70}, {"height", 16}, {"alpha", b64}}}, {"color", "#ff0000"}});
    const int at = clipseq::slotAt(d, "arm-hit");
    check(at == clipseq::slotAt(d, "arm") + 1 && d["slots"][size_t(at)]["bone"] == "arm" && made.extra["attachment"] == "box", "a new slot right above the part, on its bone");
    const Json* a = nullptr;
    for (const auto& x : d["attachments"]) if (x["slot"] == "arm-hit") a = &x;
    check(a && (*a)["type"] == "boundingBox" && (*a)["color"] == "#ff0000" && (*a)["vertices"].size() >= 6, "traced outline");
    double minX = 1e9;
    for (size_t i = 0; a && i < (*a)["vertices"].size(); i += 2) minX = std::min(minX, (*a)["vertices"][i].get<double>());
    check(minX > 6 && minX < 12, "the transparent columns are left out (" + std::to_string(minX) + ")");
    edit({{"op", "addBoundingBox"}, {"slot", "torso-hit"}, {"from", "torso"}});
    const Rig r = parseRig("r", d);
    const Pose pose = samplePose(r, -1, 0);
    const auto list = boxPolygons(r, pose);
    const cutout::BoxWorld* t = boxOf(list, "torso-hit");
    const cutout::Affine m = pose.world[size_t(r.boneIndex("body"))] * attachmentPlacement(*r.attachment("torso", "torso"));
    double cx, cy;
    m.apply(60, 100, cx, cy);
    check(t && near(t->polygon[4], cx, 1e-6) && near(t->polygon[5], cy, 1e-6), "no mask: the picture's rectangle, where the part is");
    edit({{"op", "addBoundingBox"}, {"slot", "free"}, {"bone", "root"}});
    check(d["attachments"].back()["vertices"] == Json::array({-32, -32, 32, -32, 32, 32, -32, 32}), "default square around the bone");
    // setBoundingBox: world vertices round trip, colour removal, weights / deform dropped on a new count.
    edit({{"op", "setBoundingBox"}, {"slot", "free"}, {"worldVertices", {150, 280, 160, 280, 160, 290}}, {"color", nullptr}});
    check(d["attachments"].back()["vertices"] == Json::array({0, 0, 10, 0, 10, 10}) && !d["attachments"].back().contains("color"), "world vertices -> bone px; colour removed");
    d["attachments"][2]["weights"] = Json::parse(R"([[{"bone":"head","weight":1}],[{"bone":"head","weight":1}],[{"bone":"head","weight":1}],[{"bone":"head","weight":1}]])");
    d["clips"][0]["tracks"].push_back(Json::parse(R"({"target": "slot:head-box", "property": "deform", "attachment": "head", "keys": [{"timeMs": 0, "offsets": [0,0,0,0,0,0,0,0]}]})"));
    const EditResult dropped = edit({{"op", "setBoundingBox"}, {"slot", "head-box"}, {"vertices", {0, 0, 5, 0, 5, 5}}});
    check(!d["attachments"][2].contains("weights") && hasCode(dropped.diagnostics, "ANIM_BOX_WEIGHTS_DROPPED") && hasCode(dropped.diagnostics, "ANIM_MESH_DEFORM_REMOVED"), "new point count drops weights and deform keys");
    bool threw = false;
    try { edit({{"op", "setBoundingBox"}, {"slot", "hand"}, {"vertices", {0, 0, 5, 0, 5, 5}}}); } catch (const EditError&) { threw = true; }
    check(threw, "setBoundingBox refuses a point");
    threw = false;
    try { edit({{"op", "addBoundingBox"}, {"slot", "x2"}, {"bone", "root"}, {"vertices", {0, 0, 1, 1}}}); } catch (const EditError&) { threw = true; }
    check(threw, "fewer than 3 points refused");
    for (const char* op : {"convertToMesh", "autoWeights"}) {
      threw = false;
      try { edit({{"op", op}, {"slot", "arm-box"}}); } catch (const EditError&) { threw = true; }
      check(threw, std::string(op) + " refuses a box");
    }
    threw = false;
    try { edit({{"op", "setSequence"}, {"slot", "hand"}, {"frames", {"a", "b"}}}); } catch (const EditError&) { threw = true; }
    check(threw, "setSequence refuses a point");
    // Points: world place and direction at the setup pose, then back out through pointWorlds.
    edit({{"op", "addPoint"}, {"slot", "anchor"}, {"bone", "head"}, {"worldX", 170}, {"worldY", 60}, {"worldRotation", 45}});
    check(d["slots"].back()["id"] == "anchor" && d["attachments"].back()["type"] == "point", "a new point slot on top");
    const Rig rp = parseRig("p", d);
    const auto pts = pointWorlds(rp, samplePose(rp, -1, 0));
    const auto& anchor = pts.back();
    check(anchor.attachment->name == "point" && near(anchor.x, 170, 1e-6) && near(anchor.y, 60, 1e-6) && near(anchor.rotation, 45, 1e-6), "world place and direction kept");
    edit({{"op", "setPoint"}, {"slot", "anchor"}, {"x", 1}, {"y", 2}, {"rotation", 0}});
    check(d["attachments"].back() == Json({{"slot", "anchor"}, {"name", "point"}, {"type", "point"}, {"x", 1}, {"y", 2}}), "bone-space fields (0 drops the key)");
    edit({{"op", "setAttachment"}, {"slot", "anchor"}, {"name", "point"}, {"x", 4}, {"rotation", 10}});
    check(d["attachments"].back()["x"] == 4 && d["attachments"].back()["rotation"] == 10, "canvas drags (setAttachment) move / turn points");
    edit({{"op", "deleteSlot"}, {"slot", "anchor"}});
    check(clipseq::slotAt(d, "anchor") < 0, "deleteSlot removes a point slot");
    check(!errors(validateRig(d)), "every result valid");
  });

  test("boxes: dt partitions and instances agree; describe/save unaffected", [] {
    auto run = [](const std::vector<double>& dts) {
      Instance inst(std::make_shared<const Rig>(parseRig("b", boxRig())), parseController("c", boxController()));
      inst.signal("partClicked", {{"box", "arm"}, {"slot", "arm-box"}});
      for (double dt : dts) inst.update(dt);
      const Pose p = inst.pose();
      Json out = Json::array();
      for (const auto& b : boxPolygons(inst.rig(), p)) out.push_back(b.polygon);
      for (const auto& q : pointWorlds(inst.rig(), p)) out.push_back({q.x, q.y, q.rotation});
      return out;
    };
    check(run({100, 100, 100}) == run({50, 50, 12.5, 87.5, 100}), "same time, different pieces: identical polygons and points");
  });
}
}  // namespace boxes
