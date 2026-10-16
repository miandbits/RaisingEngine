// Weight painting and linked meshes (plugin 1.18.0), included by core-tests.cpp after the other
// fixtures. Also the --weight-parity script (tests/plugins/cutout-mesh-parity.test.mjs).
#pragma once

namespace weights {
using cutout::Json;

/** 300x300 canvas, origin (150, 280). A 200x200 cape (4x4 grid, 25 vertices, 50 px apart) centred
 *  on `root` at (150, 150); bones in left/right pairs: arm-l / arm-r (±60 px), ruka-levyy /
 *  ruka-pravyy (±60, +60: transliterated names), spine above. A 40x30 flag (2x2 grid) hangs from
 *  `spine` (pivot bottom-left), a 20x15 badge picture rides on arm-r. */
Json rig() {
  using namespace cutout;
  Json d = Json::parse(R"({
    "schemaVersion": 1, "canvas": {"width": 300, "height": 300, "originX": 150, "originY": 280},
    "bones": [
      {"id": "root", "parent": null, "x": 150, "y": 150},
      {"id": "spine", "parent": "root", "y": -60},
      {"id": "arm-l", "parent": "root", "x": -60},
      {"id": "arm-r", "parent": "root", "x": 60},
      {"id": "ruka-levyy", "parent": "root", "x": -60, "y": 60},
      {"id": "ruka-pravyy", "parent": "root", "x": 60, "y": 60}
    ],
    "slots": [
      {"id": "cape", "bone": "root", "attachment": "cape"},
      {"id": "flag", "bone": "spine", "attachment": "flag"},
      {"id": "badge", "bone": "arm-r", "attachment": "badge"}
    ],
    "attachments": [
      {"slot": "cape", "name": "cape", "imageAssetId": "img-cape", "width": 200, "height": 200},
      {"slot": "flag", "name": "flag", "imageAssetId": "img-flag", "width": 40, "height": 30, "pivotX": 0, "pivotY": 1},
      {"slot": "badge", "name": "badge", "imageAssetId": "img-badge", "width": 20, "height": 15}
    ]
  })");
  d = applyEdit(d, {{"op", "convertToMesh"}, {"slot", "cape"}, {"rows", 4}, {"cols", 4}}).data;
  return applyEdit(d, {{"op", "convertToMesh"}, {"slot", "flag"}, {"rows", 2}, {"cols", 2}}).data;
}
Json* entry(Json& d, const std::string& slot, const std::string& name, const std::string& skin = "") {
  Json* list = &d["attachments"];
  if (!skin.empty()) for (auto& s : d["skins"]) if (s["id"] == skin) list = &s["attachments"];
  for (auto& a : *list) if (a["slot"] == slot && a["name"] == name) return &a;
  return nullptr;
}
/** Index of the vertex at image px (x, y) of mesh entry `a` (-1 = none). */
int vertexAt(const Json& a, double x, double y) {
  const Json& v = a["vertices"];
  for (size_t i = 0; i + 1 < v.size(); i += 2) if (std::fabs(v[i].get<double>() - x) < 1e-6 && std::fabs(v[i + 1].get<double>() - y) < 1e-6) return int(i / 2);
  return -1;
}
std::map<std::string, double> sharesOf(const Json& a, int v) {
  std::map<std::string, double> out;
  if (a.contains("weights") && v >= 0) for (const auto& w : a["weights"][size_t(v)]) out[w["bone"].get<std::string>()] = w["weight"].get<double>();
  return out;
}
double share(const Json& a, int v, const std::string& bone) { auto s = sharesOf(a, v); return s.count(bone) ? s[bone] : 0.0; }
/** Every vertex: 1..4 shares summing to 1. */
bool settled(const Json& a, size_t maxInfluences = 4) {
  for (const auto& list : a["weights"]) {
    double sum = 0;
    for (const auto& w : list) sum += w["weight"].get<double>();
    if (list.empty() || list.size() > maxInfluences || std::fabs(sum - 1) > 1e-6) return false;
  }
  return true;
}
Json edit(const Json& d, const Json& op) { return cutout::applyEdit(d, op).data; }
bool refused(const Json& d, const Json& op) {
  try { cutout::applyEdit(d, op); } catch (const cutout::EditError&) { return true; }
  return false;
}
bool errors(const std::vector<cutout::Diagnostic>& ds) { for (const auto& x : ds) if (x.severity == "error") return true; return false; }
/** Linear left -> right gradient on the cape: arm-r share = (rig x - 50) / 200, arm-l the rest. */
Json gradient(Json d) {
  Json weights = Json::array(), vertices = Json::array();
  const Json& v = (*entry(d, "cape", "cape"))["vertices"];
  for (size_t i = 0; i < v.size() / 2; i++) {
    const double t = v[2 * i].get<double>() / 200;
    vertices.push_back(i);
    Json list = Json::array();
    if (t < 1) list.push_back({{"bone", "arm-l"}, {"weight", 1 - t}});
    if (t > 0) list.push_back({{"bone", "arm-r"}, {"weight", t}});
    weights.push_back(list);
  }
  return edit(d, {{"op", "setWeights"}, {"slot", "cape"}, {"vertices", vertices}, {"weights", weights}});
}
/** The linked-mesh story: weighted cape with a sway clip (arm-l turns, the cape waves by deform
 *  keys); skin `red` swaps a half-size picture linked to the cape (deform inherited); slot `ghost`
 *  (on arm-r) shows a linked cape without deform; slot `copy` (on arm-r) shows a linked flag (the
 *  flag is unweighted: placed on copy's bone) inheriting the deform keys addressed to "flag". */
Json linkedRig() {
  Json d = gradient(rig());
  d["slots"].push_back({{"id", "ghost"}, {"bone", "arm-r"}, {"attachment", "ghost"}});
  d["slots"].push_back({{"id", "copy"}, {"bone", "arm-r"}, {"attachment", "flag2"}});
  d["attachments"].push_back(Json::parse(R"({"slot": "ghost", "name": "ghost", "type": "mesh", "imageAssetId": "img-ghost", "width": 200, "height": 200, "linkedMesh": {"parent": "cape/cape", "deform": false}})"));
  d["attachments"].push_back(Json::parse(R"({"slot": "copy", "name": "flag2", "type": "mesh", "imageAssetId": "img-flag-2", "width": 80, "height": 60, "linkedMesh": {"parent": "flag/flag"}})"));
  d["skins"] = Json::parse(R"([{"id": "red", "attachments": [{"slot": "cape", "name": "cape", "type": "mesh", "imageAssetId": "img-cape-red", "width": 100, "height": 100, "linkedMesh": {"parent": "cape/cape"}}]}])");
  std::vector<double> wave(50, 0.0), flagWave(18, 0.0);
  for (size_t i = 0; i < 25; i++) { wave[2 * i] = 3.0 * double(i % 5); wave[2 * i + 1] = -4.0 * double(i % 3); }
  for (size_t i = 0; i < 9; i++) flagWave[2 * i + 1] = 2.5 * double(i % 3);
  d["clips"] = Json::array({{{"id", "sway"}, {"durationMs", 1000}, {"loop", true}, {"tracks", Json::array({
    {{"target", "bone:arm-l"}, {"property", "rotation"}, {"keys", Json::array({{{"timeMs", 0}, {"value", 0}}, {{"timeMs", 500}, {"value", 30}, {"interp", "bezier"}, {"curve", {0.42, 0, 0.58, 1}}}, {{"timeMs", 1000}, {"value", 0}}})}},
    {{"target", "bone:arm-r"}, {"property", "rotation"}, {"keys", Json::array({{{"timeMs", 0}, {"value", 0}}, {{"timeMs", 500}, {"value", -20}}, {{"timeMs", 1000}, {"value", 0}}})}},
    {{"target", "slot:cape"}, {"property", "deform"}, {"attachment", "cape"}, {"keys", Json::array({{{"timeMs", 0}, {"offsets", std::vector<double>(50, 0.0)}}, {{"timeMs", 500}, {"offsets", wave}}, {{"timeMs", 1000}, {"offsets", std::vector<double>(50, 0.0)}}})}},
    {{"target", "slot:copy"}, {"property", "deform"}, {"attachment", "flag"}, {"keys", Json::array({{{"timeMs", 0}, {"offsets", std::vector<double>(18, 0.0)}}, {{"timeMs", 600}, {"offsets", flagWave}}})}}})}}});
  return d;
}
const cutout::DrawItem* itemOf(const std::vector<cutout::DrawItem>& items, const std::string& slot) {
  for (const auto& i : items) if (i.slot == slot) return &i;
  return nullptr;
}
/** Rig-space positions in instance space (draw items put the rig origin at 0, 0). */
std::vector<double> local(std::vector<double> xy, const cutout::Rig& r) {
  for (size_t i = 0; i + 1 < xy.size(); i += 2) { xy[i] -= r.originX; xy[i + 1] -= r.originY; }
  return xy;
}
bool samePositions(const std::vector<double>& a, const std::vector<double>& b, double eps = 1e-6) {
  if (a.size() != b.size() || a.empty()) return false;
  for (size_t i = 0; i < a.size(); i++) if (std::fabs(a[i] - b[i]) > eps) return false;
  return true;
}
std::vector<cutout::DrawItem> itemsAt(const Json& d, double ms, const std::string& skin = "") {
  static std::vector<std::shared_ptr<const cutout::Rig>> keep;  // draw items point into the rig
  keep.push_back(std::make_shared<const cutout::Rig>(cutout::parseRig("w", d)));
  cutout::Look look;
  look.skin = keep.back()->skinIndex(skin);
  return cutout::drawItems(*keep.back(), cutout::samplePose(*keep.back(), keep.back()->clipIndex("sway"), ms, look));
}

/** Host op script for the Wasm parity test: the linked story with painted (and mirrored)
 *  weights, two instances (b mirrored and larger), uneven steps, the red skin, then samples. */
Json parityRig() {
  Json d = linkedRig();
  // Painted on top of the gradient: brush strokes, smoothing and a mirror, as the editor would.
  const Json* cape = entry(d, "cape", "cape");
  std::vector<int> left;
  for (size_t i = 0; i < (*cape)["vertices"].size() / 2; i++) if ((*cape)["vertices"][2 * i].get<double>() < 100) left.push_back(int(i));
  d = edit(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"mode", "add"}, {"bone", "ruka-levyy"}, {"vertices", left}, {"amounts", std::vector<double>(left.size(), 0.6)}, {"strength", 0.5}});
  d = edit(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"mode", "smooth"}, {"vertices", left}, {"strength", 0.5}});
  d = edit(d, {{"op", "mirrorWeights"}, {"slot", "cape"}, {"axis", "x"}, {"from", "left"}});
  d["clips"][0]["tracks"].push_back({{"target", "bone:ruka-pravyy"}, {"property", "y"}, {"keys", Json::array({{{"timeMs", 0}, {"value", 60}}, {{"timeMs", 700}, {"value", 90}}})}});
  return d;
}
Json parityScript() {
  const Json controller = {{"schemaVersion", 1}, {"rig", "cape"}, {"defaultClip", "sway"}};
  auto mount = [](const std::string& id, bool flip, int w) {
    return Json{{"op", "mount"}, {"instance", id}, {"element", {{"type", "character"}, {"ref", "cape-main"}, {"width", w}, {"height", w}, {"props", {{"scale", 1}, {"flipX", flip}}}}}};
  };
  Json script = Json::array({{{"op", "init"}, {"frameFormats", {2}}, {"documents", Json::array({{{"type", "rig"}, {"id", "cape"}, {"data", parityRig()}}, {{"type", "controller"}, {"id", "cape-main"}, {"data", controller}}})}}});
  script.push_back(mount("a", false, 300));
  script.push_back(mount("b", true, 450));
  for (double dt : {0.0, 16.7, 133.0, 250.5, 199.0, 310.0}) {
    if (dt > 0) script.push_back({{"op", "update"}, {"dtMs", dt}});
    for (const char* id : {"a", "b"}) script.push_back({{"op", "frame"}, {"instance", id}});
  }
  script.push_back({{"op", "setSkin"}, {"instance", "a"}, {"skin", "red"}});
  script.push_back({{"op", "frame"}, {"instance", "a"}});
  script.push_back({{"op", "frame"}, {"instance", "a"}, {"format", 2}});
  for (double ms : {0.0, 250.0, 500.0, 800.0}) script.push_back({{"op", "sample"}, {"rig", parityRig()}, {"clip", "sway"}, {"timeMs", ms}});
  return script;
}

void run() {
  using namespace cutout;
  test("weights: mirror bone names (left/right, -l/-r, transliterated pairs)", [] {
    const std::pair<const char*, const char*> cases[] = {
      {"arm-l", "arm-r"}, {"l-arm", "r-arm"}, {"leg-left", "leg-right"}, {"hand-lt", "hand-rt"}, {"ruka-levyy", "ruka-pravyy"}, {"ruka-pravyy", "ruka-levyy"},
      {"noga-levaya", "noga-pravaya"}, {"glaz-levyj", "glaz-pravyj"}, {"uho-lev", "uho-prav"}, {"armleft", "armright"}, {"upperright", "upperleft"},
      {"spine", "spine"}, {"left", "right"}, {"l", "r"}, {"arm-l-2", "arm-r-2"}, {"level", "level"}};
    for (const auto& [from, to] : cases) check(mirrorBoneId(from) == to, std::string(from) + " -> " + to + " (got " + mirrorBoneId(from) + ")");
  });

  test("weights: paint add / subtract / replace (amounts, strength, sum 1, slot bone rule)", [] {
    Json d = edit(rig(), {{"op", "autoWeights"}, {"slot", "cape"}, {"bones", {"arm-l", "arm-r"}}});
    const int c = vertexAt(*entry(d, "cape", "cape"), 100, 100), e = vertexAt(*entry(d, "cape", "cape"), 50, 100);
    check(c >= 0 && e >= 0, "grid vertices found");
    const double c0 = share(*entry(d, "cape", "cape"), c, "arm-l"), e0 = share(*entry(d, "cape", "cape"), e, "arm-l");
    const auto added = applyEdit(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"mode", "add"}, {"bone", "arm-l"}, {"vertices", {c, e}}, {"amounts", {1, 0.5}}, {"strength", 0.4}});
    Json a = *entry(const_cast<Json&>(added.data), "cape", "cape");
    check(near(share(a, c, "arm-l"), std::min(1.0, c0 + 0.4), 1e-6) && near(share(a, e, "arm-l"), std::min(1.0, e0 + 0.2), 1e-6), "add: a = strength · amount");
    check(settled(a) && added.extra["vertices"] == 2, "every vertex sums to 1");
    d = added.data;
    d = edit(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"mode", "subtract"}, {"bone", "arm-l"}, {"vertices", {c}}, {"strength", 0.3}});
    check(near(share(*entry(d, "cape", "cape"), c, "arm-l"), c0 + 0.1, 1e-6) && near(share(*entry(d, "cape", "cape"), c, "arm-r"), 1 - c0 - 0.1, 1e-6), "subtract: the other bone takes the rest");
    d = edit(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"attachment", "cape"}, {"mode", "replace"}, {"bone", "arm-l"}, {"weight", 0.25}, {"vertices", {c}}});
    check(near(share(*entry(d, "cape", "cape"), c, "arm-l"), 0.25, 1e-6), "replace at strength 1 sets the weight (attachment alias)");
    d = edit(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"bone", "arm-l"}, {"weight", 1}, {"strength", 0.5}, {"vertices", {c}}});
    check(near(share(*entry(d, "cape", "cape"), c, "arm-l"), 0.625, 1e-6), "replace is the default mode, halfway at strength 0.5");
    // Unweighted flag: the rest goes to the slot bone; a vertex only on the slot bone cannot lose it.
    Json f = edit(rig(), {{"op", "paintWeights"}, {"slot", "flag"}, {"bone", "arm-r"}, {"weight", 0.6}, {"vertices", {0}}});
    check(near(share(*entry(f, "flag", "flag"), 0, "arm-r"), 0.6, 1e-6) && near(share(*entry(f, "flag", "flag"), 0, "spine"), 0.4, 1e-6), "rest to the slot bone");
    const auto keep = applyEdit(rig(), {{"op", "paintWeights"}, {"slot", "flag"}, {"mode", "subtract"}, {"bone", "spine"}, {"vertices", {1}}});
    check(keep.extra["vertices"] == 0 && keep.extra["skipped"] == 1 && near(share(*entry(const_cast<Json&>(keep.data), "flag", "flag"), 1, "spine"), 1), "the only bone stays");
    check(refused(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"mode", "blur"}, {"bone", "arm-l"}, {"vertices", {c}}}), "unknown mode refused");
    check(refused(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"bone", "arm-l"}, {"vertices", {c}}, {"amounts", {1, 1}}}), "amounts per vertex");
    check(refused(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"bone", "arm-l"}, {"vertices", {c}}, {"strength", 2}}), "strength 0..1");
    check(refused(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"vertices", {c}}}), "a bone is needed (not smooth)");
    check(refused(d, {{"op", "paintWeights"}, {"slot", "badge"}, {"bone", "arm-l"}, {"vertices", {0}}}), "regions refused");
  });

  test("weights: locked bones keep their shares", [] {
    const Json base = rig();
    const int c = vertexAt(*entry(const_cast<Json&>(base), "cape", "cape"), 100, 100);
    const Json three = Json::array({Json::array({{{"bone", "arm-l"}, {"weight", 0.5}}, {{"bone", "arm-r"}, {"weight", 0.3}}, {{"bone", "spine"}, {"weight", 0.2}}})});
    const Json d = edit(base, {{"op", "setWeights"}, {"slot", "cape"}, {"vertices", {c}}, {"weights", three}});
    Json p = edit(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"mode", "add"}, {"bone", "arm-l"}, {"strength", 0.3}, {"vertices", {c}}, {"locked", {"spine"}}});
    auto s = sharesOf(*entry(p, "cape", "cape"), c);
    check(near(s["spine"], 0.2, 1e-6) && near(s["arm-l"], 0.8, 1e-6) && !s.count("arm-r"), "locked spine kept, arm-r pruned at 0");
    check(refused(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"bone", "spine"}, {"vertices", {c}}, {"locked", {"spine"}}}), "a locked bone cannot be painted");
    check(refused(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"bone", "arm-l"}, {"vertices", {c}}, {"locked", {"nope"}}}), "locked names bones");
    const auto full = applyEdit(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"mode", "add"}, {"bone", "arm-l"}, {"strength", 0.3}, {"vertices", {c}}, {"locked", {"arm-r", "spine"}}});
    check(full.extra["vertices"] == 0 && near(share(*entry(const_cast<Json&>(full.data), "cape", "cape"), c, "arm-l"), 0.5, 1e-6), "everything else locked: arm-l cannot grow");
    p = edit(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"bone", "arm-l"}, {"weight", 0.2}, {"vertices", {c}}, {"locked", {"arm-r", "spine"}}});
    s = sharesOf(*entry(p, "cape", "cape"), c);
    check(near(s["arm-l"], 0.2, 1e-6) && near(s["arm-r"], 0.3, 1e-6) && near(s["spine"], 0.2, 1e-6) && near(s["root"], 0.3, 1e-6), "the rest goes to the (unlocked) slot bone");
    p = edit(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"mode", "smooth"}, {"strength", 1}, {"vertices", {c}}, {"locked", {"spine"}}});
    check(near(share(*entry(p, "cape", "cape"), c, "spine"), 0.2, 1e-6) && settled(*entry(p, "cape", "cape")), "smooth keeps locked shares");
  });

  test("weights: normalizeWeights prunes, limits influences, respects locks (op from the LLM path)", [] {
    const Json base = rig();
    const int c = vertexAt(*entry(const_cast<Json&>(base), "cape", "cape"), 100, 100);
    const Json four = Json::array({Json::array({{{"bone", "arm-l"}, {"weight", 0.6}}, {{"bone", "arm-r"}, {"weight", 0.3}}, {{"bone", "spine"}, {"weight", 0.095}}, {{"bone", "root"}, {"weight", 0.005}}})});
    const Json d = edit(base, {{"op", "setWeights"}, {"slot", "cape"}, {"vertices", {c}}, {"weights", four}});
    check(sharesOf(*entry(const_cast<Json&>(d), "cape", "cape"), c).size() == 4, "four shares to start with");
    Host host;
    const Json reply = host.handle({{"op", "edit"}, {"rig", d}, {"operation", {{"op", "normalizeWeights"}, {"slot", "cape"}, {"attachment", "cape"}}}});
    check(reply["ok"] == true, "normalizeWeights through the host edit op");
    Json n = reply["rig"];
    auto s = sharesOf(*entry(n, "cape", "cape"), c);
    check(s.size() == 3 && !s.count("root") && near(s["arm-l"], 0.6 / 0.995, 1e-6) && settled(*entry(n, "cape", "cape")), "default prune 0.01 drops the 0.5 % share");
    n = edit(d, {{"op", "normalizeWeights"}, {"slot", "cape"}, {"maxInfluences", 2}});
    s = sharesOf(*entry(n, "cape", "cape"), c);
    check(s.size() == 2 && near(s["arm-l"], 0.6 / 0.9, 1e-6) && near(s["arm-r"], 0.3 / 0.9, 1e-6), "two largest kept");
    n = edit(d, {{"op", "normalizeWeights"}, {"slot", "cape"}, {"maxInfluences", 2}, {"locked", {"spine"}}});
    s = sharesOf(*entry(n, "cape", "cape"), c);
    check(s.size() == 2 && near(s["spine"], 0.095, 1e-6) && near(s["arm-l"], 0.905, 1e-6), "a locked share survives the limit");
    n = edit(d, {{"op", "normalizeWeights"}, {"slot", "cape"}, {"maxInfluences", 1}});
    check(settled(*entry(n, "cape", "cape"), 1), "one influence everywhere");
    n = edit(d, {{"op", "normalizeWeights"}, {"slot", "cape"}, {"prune", 0.2}});
    check(sharesOf(*entry(n, "cape", "cape"), c).size() == 2, "a larger prune threshold");
    check(refused(rig(), {{"op", "normalizeWeights"}, {"slot", "cape"}}), "unweighted refused");
    check(refused(d, {{"op", "normalizeWeights"}, {"slot", "cape"}, {"maxInfluences", 5}}) && refused(d, {{"op", "normalizeWeights"}, {"slot", "cape"}, {"prune", 0.9}}), "ranges");
  });

  test("weights: smooth averages the mesh neighbours", [] {
    Json d = rig();
    const Json* cape = entry(d, "cape", "cape");
    const size_t n = (*cape)["vertices"].size() / 2;
    const int c = vertexAt(*cape, 100, 100);
    Json vertices = Json::array(), list = Json::array();
    for (size_t i = 0; i < n; i++) { vertices.push_back(i); list.push_back(Json::array({{{"bone", int(i) == c ? "arm-l" : "arm-r"}, {"weight", 1}}})); }
    d = edit(d, {{"op", "setWeights"}, {"slot", "cape"}, {"vertices", vertices}, {"weights", list}});
    Json s = edit(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"mode", "smooth"}, {"strength", 0.5}, {"vertices", {c}}});
    check(near(share(*entry(s, "cape", "cape"), c, "arm-l"), 0.5, 1e-6) && near(share(*entry(s, "cape", "cape"), c, "arm-r"), 0.5, 1e-6), "halfway to the neighbours' mean");
    s = edit(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"mode", "smooth"}, {"strength", 1}, {"vertices", {c}}});
    check(sharesOf(*entry(s, "cape", "cape"), c) == std::map<std::string, double>({{"arm-r", 1.0}}), "full strength = the mean (arm-l pruned)");
    s = edit(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"mode", "smooth"}, {"strength", 1}, {"vertices", {c}}, {"locked", {"arm-l"}}});
    check(sharesOf(*entry(s, "cape", "cape"), c) == std::map<std::string, double>({{"arm-l", 1.0}}), "a locked full share stays");
    // A neighbour's own value is read from before the stroke (Jacobi): order of the list does not matter.
    const int e = vertexAt(*entry(d, "cape", "cape"), 150, 100);
    const Json ab = edit(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"mode", "smooth"}, {"strength", 0.5}, {"vertices", {c, e}}});
    const Json ba = edit(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"mode", "smooth"}, {"strength", 0.5}, {"vertices", {e, c}}});
    check((*entry(const_cast<Json&>(ab), "cape", "cape"))["weights"] == (*entry(const_cast<Json&>(ba), "cape", "cape"))["weights"], "order independent");
  });

  test("weights: mirrorWeights maps bone pairs across the axis (auto names, transliterated, map override)", [] {
    Json d = rig();
    const Json* cape = entry(d, "cape", "cape");
    const size_t n = (*cape)["vertices"].size() / 2;
    Json vertices = Json::array(), list = Json::array();
    for (size_t i = 0; i < n; i++) {
      const double x = (*cape)["vertices"][2 * i].get<double>();
      vertices.push_back(i);
      list.push_back(x < 100 ? Json::array({{{"bone", "arm-l"}, {"weight", 0.7}}, {{"bone", "ruka-levyy"}, {"weight", 0.3}}})
                     : x > 100 ? Json::array({{{"bone", "root"}, {"weight", 1}}}) : Json::array({{{"bone", "spine"}, {"weight", 1}}}));
    }
    d = edit(d, {{"op", "setWeights"}, {"slot", "cape"}, {"vertices", vertices}, {"weights", list}});
    const auto m = applyEdit(d, {{"op", "mirrorWeights"}, {"slot", "cape"}, {"axis", "x"}});
    const Json& a = *entry(const_cast<Json&>(m.data), "cape", "cape");
    bool right = true, centre = true, leftKept = true;
    for (size_t i = 0; i < n; i++) {
      const double x = a["vertices"][2 * i].get<double>();
      const auto s = sharesOf(a, int(i));
      if (x > 100) right = right && near(s.at("arm-r"), 0.7, 1e-6) && near(s.at("ruka-pravyy"), 0.3, 1e-6) && s.size() == 2;
      else if (x == 100) centre = centre && s == std::map<std::string, double>({{"spine", 1.0}});
      else leftKept = leftKept && near(s.at("arm-l"), 0.7, 1e-6);
    }
    check(right && centre && leftKept, "right side mirrored from the left, centre line and source side untouched");
    check(m.extra["vertices"] == 10 && m.extra["mapping"]["arm-l"] == "arm-r" && m.extra["mapping"]["ruka-levyy"] == "ruka-pravyy" && near(m.extra["center"].get<double>(), 150), "report: 10 vertices, pairs, centre at the mesh middle");
    const auto back = applyEdit(m.data, {{"op", "mirrorWeights"}, {"slot", "cape"}, {"from", "right"}});
    check((*entry(const_cast<Json&>(back.data), "cape", "cape"))["weights"] == a["weights"], "mirroring back from the right is a no-op on a symmetric result");
    const Json o = edit(d, {{"op", "mirrorWeights"}, {"slot", "cape"}, {"map", {{"arm-l", "spine"}}}});
    const int r = vertexAt(*entry(const_cast<Json&>(o), "cape", "cape"), 200, 50);
    check(near(share(*entry(const_cast<Json&>(o), "cape", "cape"), r, "spine"), 0.7, 1e-6) && near(share(*entry(const_cast<Json&>(o), "cape", "cape"), r, "ruka-pravyy"), 0.3, 1e-6), "explicit map wins over names");
    // A bone without a counterpart keeps its name (reported); axis y mirrors top -> bottom.
    const auto y = applyEdit(d, {{"op", "mirrorWeights"}, {"slot", "cape"}, {"axis", "y"}, {"from", "top"}});
    check(y.extra["vertices"] == 10 && std::find(y.extra["unmapped"].begin(), y.extra["unmapped"].end(), Json("root")) != y.extra["unmapped"].end(), "y axis: 10 vertices, root unmapped");
    check(refused(d, {{"op", "mirrorWeights"}, {"slot", "cape"}, {"axis", "z"}}) && refused(d, {{"op", "mirrorWeights"}, {"slot", "cape"}, {"from", "top"}}) &&
          refused(d, {{"op", "mirrorWeights"}, {"slot", "cape"}, {"map", {{"arm-l", "nope"}}}}), "bad axis / side / map refused");
  });

  test("weights: copyWeights takes the source mesh's weights at the same place", [] {
    const Json d = gradient(rig());
    const auto c = applyEdit(d, {{"op", "copyWeights"}, {"slot", "flag"}, {"from", {{"slot", "cape"}}}});
    const Json& flag = *entry(const_cast<Json&>(c.data), "flag", "flag");
    bool exact = true;
    for (size_t i = 0; i < flag["vertices"].size() / 2; i++) {
      // flag image px -> rig: spine (150, 90), pivot bottom-left of 40x30: x = 150 + px.
      const double rx = 150 + flag["vertices"][2 * i].get<double>(), t = (rx - 50) / 200;
      exact = exact && near(share(flag, int(i), "arm-r"), t, 1e-6) && near(share(flag, int(i), "arm-l"), 1 - t, 1e-6);
    }
    check(exact && c.extra["vertices"] == 9 && settled(flag), "linear gradient sampled exactly (barycentric)");
    const Json l = edit(linkedRig(), {{"op", "copyWeights"}, {"slot", "flag"}, {"from", {{"slot", "ghost"}}}});
    check((*entry(const_cast<Json&>(l), "flag", "flag"))["weights"] == flag["weights"], "a linked source reads its parent's weights");
    check(refused(d, {{"op", "copyWeights"}, {"slot", "cape"}, {"from", {{"slot", "cape"}}}}) && refused(d, {{"op", "copyWeights"}, {"slot", "cape"}, {"from", {{"slot", "badge"}}}}) &&
          refused(d, {{"op", "copyWeights"}, {"slot", "cape"}}), "itself / a region / no source refused");
  });

  test("linked meshes: validation (parent, skin, type, cycles, uvs, sequences, unused fields)", [] {
    const Json d = linkedRig();
    check(!errors(validateRig(d)), "the linked fixture is valid");
    auto with = [&](const std::function<void(Json&)>& change) { Json x = d; change(x); return validateRig(x); };
    auto red = [](Json& x) -> Json& { return x["skins"][0]["attachments"][0]; };
    check(hasCode(with([&](Json& x) { red(x)["linkedMesh"]["parent"] = "cape/nope"; }), "ANIM_LINKED_PARENT"), "missing parent");
    check(hasCode(with([&](Json& x) { red(x)["linkedMesh"]["parent"] = "cape"; }), "ANIM_LINKED_PARENT"), "parent needs slot/name");
    check(hasCode(with([&](Json& x) { red(x)["linkedMesh"]["skin"] = "nope"; }), "ANIM_LINKED_PARENT"), "unknown parent skin");
    check(hasCode(with([&](Json& x) { red(x)["linkedMesh"]["parent"] = "badge/badge"; }), "ANIM_LINKED_PARENT"), "a region is no parent");
    check(hasCode(with([&](Json& x) { red(x)["linkedMesh"]["deform"] = "yes"; }), "ANIM_LINKED_PARENT"), "deform is a boolean");
    check(hasCode(with([&](Json& x) { red(x).erase("type"); }), "ANIM_LINKED_TYPE"), "only meshes link");
    check(hasCode(with([&](Json& x) { red(x)["uvs"] = std::vector<double>(10, 0.5); }), "ANIM_LINKED_VERTICES"), "own uvs must match the vertex count");
    check(!errors(with([&](Json& x) { red(x)["uvs"] = std::vector<double>(50, 0.5); })), "matching own uvs are fine");
    check(hasCode(with([&](Json& x) { (*entry(x, "cape", "cape"))["linkedMesh"] = {{"parent", "cape/cape"}, {"skin", "red"}}; }), "ANIM_LINKED_CYCLE"), "a cycle through a skin");
    check(hasCode(with([&](Json& x) { (*entry(x, "ghost", "ghost"))["linkedMesh"]["parent"] = "ghost/ghost"; }), "ANIM_LINKED_CYCLE"), "linked to itself");
    check(hasCode(with([&](Json& x) { red(x)["sequence"] = {{"frames", {"img-cape-red"}}}; }), "ANIM_LINKED_SEQUENCE"), "no sequence on a linked mesh");
    check(hasCode(with([&](Json& x) { (*entry(x, "cape", "cape"))["sequence"] = {{"frames", {"img-cape"}}}; }), "ANIM_LINKED_SEQUENCE"), "no sequence parent");
    const auto unused = with([&](Json& x) { red(x)["x"] = 5; red(x)["vertices"] = {0, 0}; });
    check(hasCode(unused, "ANIM_LINKED_UNUSED") && !errors(unused), "own shape / placement fields only warn");
    check(hasCode(with([&](Json& x) { red(x).erase("imageAssetId"); }), "ANIM_IMAGE_MISSING"), "a linked mesh has its own picture");
    // Deform keys: inherited through the parent name (also in another slot), own-name keys allowed, counts checked.
    check(hasCode(with([&](Json& x) { x["clips"][0]["tracks"][3]["keys"][1]["offsets"] = std::vector<double>(4, 1.0); }), "ANIM_MESH_DEFORM"), "deform counts checked for linked meshes");
    check(!errors(with([&](Json& x) { x["clips"][0]["tracks"].push_back({{"target", "slot:ghost"}, {"property", "deform"}, {"attachment", "ghost"}, {"keys", Json::array({{{"timeMs", 0}, {"offsets", std::vector<double>(50, 1.0)}}})}}); })), "own deform keys of a non-inheriting linked mesh");
    // Chains resolve to the first own mesh.
    const auto chain = with([&](Json& x) { x["skins"][0]["attachments"].push_back(Json::parse(R"({"slot": "badge", "name": "b2", "type": "mesh", "imageAssetId": "img-b", "width": 100, "height": 100, "linkedMesh": {"parent": "cape/cape", "skin": "red"}})")); });
    check(!errors(chain), "a linked mesh of a linked mesh");
  });

  test("linked meshes: the parent's shape, weights and deform keys (skins, other slots, deform off)", [] {
    const Json d = linkedRig();
    const Rig r = parseRig("w", d);
    const Attachment* parent = r.attachment("cape", "cape");
    const Attachment* red = r.attachment("cape", "cape", r.skinIndex("red"));
    check(parent && red && red != parent && red->linked && red->mesh && red->vertices == parent->vertices && red->uvs == parent->uvs && red->triangles == parent->triangles, "skin variant: the parent's geometry");
    check(red->binds.size() == parent->binds.size() && red->weights.size() == parent->weights.size() && red->deformName == "cape" && near(red->linkedWidth, 100) && near(red->width, 200), "weights / binds / placement shared, own picture size kept");
    const Attachment* ghost = r.attachment("ghost", "ghost");
    check(ghost && ghost->linked && !ghost->linkedDeform && ghost->deformName == "ghost", "deform off: its own deform name");
    for (double ms : {0.0, 250.0, 500.0, 750.0}) {
      const auto plain = itemsAt(d, ms), skinned = itemsAt(d, ms, "red");
      const DrawItem *p = itemOf(plain, "cape"), *s = itemOf(skinned, "cape"), *g = itemOf(plain, "ghost"), *f = itemOf(plain, "copy");
      check(p && s && samePositions(p->positions, s->positions) && p->uvs == s->uvs && p->indices == s->indices && s->imageAssetId == "img-cape-red", "red skin draws the cape's deformed shape at " + std::to_string(ms));
      const Pose pose = samplePose(r, r.clipIndex("sway"), ms);
      Pose still = pose;
      still.deform.clear();
      check(g && samePositions(g->positions, local(meshPositions(r, still, r.slotIndex("cape"), *parent), r)) && g->imageAssetId == "img-ghost", "ghost: the bones' shape without the deform keys");
      check(f && samePositions(f->positions, local(meshPositions(r, pose, r.slotIndex("copy"), *r.attachment("flag", "flag")), r)), "unweighted linked flag: placed on its own slot's bone with the deform keys addressed to the parent's name");
    }
    const auto at500 = itemsAt(d, 500);
    check(!samePositions(itemOf(at500, "cape")->positions, itemOf(at500, "ghost")->positions), "the deform keys really move the cape");
    // Editing the parent moves every linked mesh with it; shape / weight ops on a linked mesh are refused.
    const Json moved = edit(d, {{"op", "setMeshVertices"}, {"slot", "cape"}, {"vertices", {0}}, {"dx", 7}, {"dy", -3}});
    const Rig m = parseRig("m", moved);
    check(m.attachment("cape", "cape", m.skinIndex("red"))->vertices == m.attachment("cape", "cape")->vertices && m.attachment("ghost", "ghost")->vertices[0] == parent->vertices[0] + 7, "parent vertex edits reach every linked mesh");
    const Json painted = edit(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"bone", "spine"}, {"weight", 1}, {"vertices", {0}}});
    check(parseRig("p", painted).attachment("ghost", "ghost")->weights[0].size() == 1, "parent weights reach the linked mesh");
    check(refused(d, {{"op", "paintWeights"}, {"slot", "ghost"}, {"bone", "spine"}, {"vertices", {0}}}) && refused(d, {{"op", "setMeshVertices"}, {"slot", "ghost"}, {"dx", 1}}) &&
          refused(d, {{"op", "autoWeights"}, {"slot", "ghost"}}) && refused(d, {{"op", "convertToMesh"}, {"slot", "ghost"}}), "shape / weight ops refused on a linked mesh");
    check(refused(d, {{"op", "setAttachment"}, {"slot", "ghost"}, {"name", "ghost"}, {"x", 4}}) && refused(d, {{"op", "setSkinAttachment"}, {"skin", "red"}, {"slot", "cape"}, {"x", 4}}), "placement edits refused");
    const Json swapped = edit(d, {{"op", "setSkinAttachment"}, {"skin", "red"}, {"slot", "cape"}, {"imageAssetId", "img-cape-blue"}});
    check(itemOf(itemsAt(swapped, 0, "red"), "cape")->imageAssetId == "img-cape-blue", "its picture swaps like any skin picture");
    check(refused(d, {{"op", "setSequence"}, {"slot", "ghost"}, {"frames", {"a", "b"}}}), "no sequence on a linked mesh");
  });

  test("linked meshes: linkMesh / unlinkMesh and parents that go away", [] {
    Json d = gradient(rig());
    d = edit(d, {{"op", "addSkin"}, {"id", "red"}});
    const auto made = applyEdit(d, {{"op", "linkMesh"}, {"slot", "cape"}, {"skin", "red"}, {"parent", "cape/cape"}, {"imageAssetId", "img-cape-red"}, {"width", 100}, {"height", 100}});
    const Json* red = entry(const_cast<Json&>(made.data), "cape", "cape", "red");
    check(made.extra["created"] == true && red && (*red)["linkedMesh"] == Json({{"parent", "cape/cape"}}) && (*red)["type"] == "mesh" && !red->contains("vertices"), "outfit variant created in the skin");
    check(!errors(made.diagnostics), "valid");
    check(refused(d, {{"op", "linkMesh"}, {"slot", "cape"}, {"skin", "red"}, {"parent", "cape/cape"}, {"imageAssetId", "img-x"}, {"width", 100}, {"height", 50}}), "another aspect refused");
    check(refused(d, {{"op", "linkMesh"}, {"slot", "cape"}, {"parent", "cape/cape"}}), "itself refused");
    check(refused(d, {{"op", "linkMesh"}, {"slot", "flag"}, {"parent", "badge/badge"}}), "a region parent refused");
    check(refused(made.data, {{"op", "linkMesh"}, {"slot", "cape"}, {"parent", "cape/cape"}, {"parentSkin", "red"}}), "a cycle refused (validation)");
    // A region becomes a linked mesh of a mesh of the same aspect (4:3 badge -> 4:3 flag), deform off.
    const auto badge = applyEdit(d, {{"op", "linkMesh"}, {"slot", "badge"}, {"parent", "flag/flag"}, {"deform", false}});
    const Json* b = entry(const_cast<Json&>(badge.data), "badge", "badge");
    check(b && (*b)["linkedMesh"] == Json({{"parent", "flag/flag"}, {"deform", false}}) && (*b)["imageAssetId"] == "img-badge" && !b->contains("x"), "region -> linked mesh keeps its picture");
    // Unlink: an independent copy drawing the same (half-size picture: vertices scaled, offset scale compensated).
    Json lr = linkedRig();
    for (const auto& [slot, skin] : {std::pair<std::string, std::string>{"cape", "red"}, {"ghost", ""}, {"copy", ""}}) {
      const std::string name = slot == "copy" ? "flag2" : slot;
      Json op = {{"op", "unlinkMesh"}, {"slot", slot}, {"name", name}};
      if (!skin.empty()) op["skin"] = skin;
      const Json un = edit(lr, op);
      const Json* a = entry(const_cast<Json&>(un), slot, name, skin);
      check(a && !a->contains("linkedMesh") && a->contains("vertices") && a->contains("triangles"), slot + ": independent mesh");
      for (double ms : {0.0, 500.0, 800.0}) {
        const auto before = itemsAt(lr, ms, skin), after = itemsAt(un, ms, skin);
        const DrawItem *x = itemOf(before, slot), *y = itemOf(after, slot);
        check(x && y && samePositions(x->positions, y->positions, 1e-6) && x->uvs == y->uvs && x->imageAssetId == y->imageAssetId, slot + ": draws the same after unlinking at " + std::to_string(ms));
      }
    }
    // Half-size picture: its own pixels (vertices scaled, offset scale 2) unless deform keys already
    // address its name (shared with the default cape: then the pivot carries the difference).
    Json noKeys = lr;
    noKeys["clips"][0]["tracks"].erase(2);
    const Json redCopy = edit(noKeys, {{"op", "unlinkMesh"}, {"slot", "cape"}, {"skin", "red"}});
    check(near((*entry(const_cast<Json&>(redCopy), "cape", "cape", "red"))["vertices"][2].get<double>(), 25) && near((*entry(const_cast<Json&>(redCopy), "cape", "cape", "red"))["scaleX"].get<double>(), 2), "half-size picture: vertices in its own pixels");
    check(samePositions(itemOf(itemsAt(noKeys, 400, "red"), "cape")->positions, itemOf(itemsAt(redCopy, 400, "red"), "cape")->positions), "and draws the same");
    const Json keyed = edit(lr, {{"op", "unlinkMesh"}, {"slot", "cape"}, {"skin", "red"}});
    check(near((*entry(const_cast<Json&>(keyed), "cape", "cape", "red"))["vertices"][2].get<double>(), 50) && near((*entry(const_cast<Json&>(keyed), "cape", "cape", "red"))["pivotX"].get<double>(), 1), "shared deform keys: parent pixels, pivot scaled");
    const Json flagCopy = edit(lr, {{"op", "unlinkMesh"}, {"slot", "copy"}, {"name", "flag2"}});
    bool copied = false;
    for (const auto& t : flagCopy["clips"][0]["tracks"]) copied = copied || (t["target"] == "slot:copy" && t["attachment"] == "flag2");
    check(copied, "inherited deform keys copied to its own name");
    check(refused(lr, {{"op", "unlinkMesh"}, {"slot", "cape"}}), "unlinking a plain mesh refused");
    // Parents that go away: dependents become copies (warning), drawing the same.
    const auto gone = applyEdit(lr, {{"op", "deleteSlot"}, {"slot", "flag"}});
    check(hasCode(gone.diagnostics, "ANIM_LINKED_UNLINKED") && !errors(gone.diagnostics) && !entry(const_cast<Json&>(gone.data), "copy", "flag2")->contains("linkedMesh"), "deleting the parent's slot unlinks");
    check(samePositions(itemOf(itemsAt(lr, 300), "copy")->positions, itemOf(itemsAt(gone.data, 300), "copy")->positions), "the copy draws as before");
    const auto region = applyEdit(lr, {{"op", "convertToRegion"}, {"slot", "cape"}});
    check(!errors(region.diagnostics) && !entry(const_cast<Json&>(region.data), "ghost", "ghost")->contains("linkedMesh") && !entry(const_cast<Json&>(region.data), "cape", "cape", "red")->contains("linkedMesh"), "a parent turned into a picture unlinks its meshes");
    const Json ghostRegion = edit(lr, {{"op", "convertToRegion"}, {"slot", "ghost"}});
    const Json* gr = entry(const_cast<Json&>(ghostRegion), "ghost", "ghost");
    check(!gr->contains("linkedMesh") && !gr->contains("type") && !gr->contains("vertices"), "a linked mesh back to a picture (the parent's placement)");
    // Skins: rename follows, delete unlinks the meshes linked into it.
    Json sk = lr;
    sk["attachments"].push_back(Json::parse(R"({"slot": "badge", "name": "red-cape", "type": "mesh", "imageAssetId": "img-r", "width": 100, "height": 100, "linkedMesh": {"parent": "cape/cape", "skin": "red"}})"));
    check(!errors(validateRig(sk)), "a default attachment linked to a skin mesh");
    const Json renamed = edit(sk, {{"op", "renameSkin"}, {"skin", "red"}, {"id", "crimson"}});
    check((*entry(const_cast<Json&>(renamed), "badge", "red-cape"))["linkedMesh"]["skin"] == "crimson", "renameSkin follows");
    const auto deleted = applyEdit(sk, {{"op", "deleteSkin"}, {"skin", "red"}});
    check(!errors(deleted.diagnostics) && !entry(const_cast<Json&>(deleted.data), "badge", "red-cape")->contains("linkedMesh"), "deleteSkin unlinks");
    const auto removed = applyEdit(sk, {{"op", "removeSkinAttachment"}, {"skin", "red"}, {"slot", "cape"}, {"name", "cape"}});
    check(!errors(removed.diagnostics) && hasCode(removed.diagnostics, "ANIM_LINKED_UNLINKED"), "removeSkinAttachment unlinks");
  });

  test("weights: painting a 2000-vertex mesh stays fast (core op)", [] {
    Json d = rig();
    d = edit(d, {{"op", "convertToMesh"}, {"slot", "cape"}, {"rows", 44}, {"cols", 44}});
    d = edit(d, {{"op", "autoWeights"}, {"slot", "cape"}, {"bones", {"arm-l", "arm-r", "spine", "ruka-levyy", "ruka-pravyy"}}});
    const size_t n = (*entry(d, "cape", "cape"))["vertices"].size() / 2;
    std::vector<int> stroke;
    for (size_t i = 0; i < n; i += 5) stroke.push_back(int(i));
    double best = 1e9;
    for (const char* mode : {"add", "smooth"})
      for (int run = 0; run < 3; run++) {
        const auto t0 = std::chrono::steady_clock::now();
        const auto r = applyEdit(d, {{"op", "paintWeights"}, {"slot", "cape"}, {"mode", mode}, {"bone", "spine"}, {"vertices", stroke}, {"strength", 0.3}});
        best = std::min(best, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
        check(settled(*entry(const_cast<Json&>(r.data), "cape", "cape")), "settled");
      }
    std::cout << "paintWeights " << n << " vertices (" << stroke.size() << " painted) in " << best << " ms\n";
    check(n >= 2000 && best < 250, "2000+ vertices, one stroke op well under 250 ms");
  });
}
}  // namespace weights
