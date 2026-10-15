// Clipping and image sequence tests (plugin 1.12.0), included by core-tests.cpp after its check()
// / test() helpers. Also the --clip-parity script (tests/plugins/cutout-clip-parity.test.mjs).
#pragma once
#include "cutout/clipping.hpp"

namespace clipseq {
using cutout::Json;

double area(const std::vector<double>& p) {
  double s = 0;
  for (size_t i = 0, n = p.size() / 2; i < n; i++) s += p[2 * i] * p[2 * ((i + 1) % n) + 1] - p[2 * ((i + 1) % n)] * p[2 * i + 1];
  return s / 2;
}
bool convex(const std::vector<double>& p) {
  const size_t n = p.size() / 2;
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n, k = (i + 2) % n;
    if ((p[2 * j] - p[2 * i]) * (p[2 * k + 1] - p[2 * i + 1]) - (p[2 * j + 1] - p[2 * i + 1]) * (p[2 * k] - p[2 * i]) < -1e-9) return false;
  }
  return true;
}
/** Total area of a mesh item's triangles (absolute, any orientation). */
double meshArea(const cutout::DrawItem& item) {
  double s = 0;
  for (size_t t = 0; t + 2 < item.indices.size(); t += 3) {
    const double* p = item.positions.data();
    const size_t a = 2 * size_t(item.indices[t]), b = 2 * size_t(item.indices[t + 1]), c = 2 * size_t(item.indices[t + 2]);
    s += std::fabs((p[b] - p[a]) * (p[c + 1] - p[a + 1]) - (p[b + 1] - p[a + 1]) * (p[c] - p[a])) / 2;
  }
  return s;
}
/** Inside the polygon or within 1e-6 px of its outline (clipped vertices lie on it). */
bool onOrInside(const std::vector<double>& p, double x, double y) {
  const size_t n = p.size() / 2;
  bool inside = false;
  for (size_t i = 0, j = n - 1; i < n; j = i++) {
    const double xi = p[2 * i], yi = p[2 * i + 1], xj = p[2 * j], yj = p[2 * j + 1];
    const double dx = xj - xi, dy = yj - yi, l = dx * dx + dy * dy, t = l > 0 ? std::clamp(((x - xi) * dx + (y - yi) * dy) / l, 0.0, 1.0) : 0;
    if (std::hypot(xi + dx * t - x, yi + dy * t - y) <= 1e-6) return true;
    if ((yi > y) != (yj > y) && x < dx * (y - yi) / dy + xi) inside = !inside;
  }
  return inside;
}
bool errors(const std::vector<cutout::Diagnostic>& ds) { for (const auto& d : ds) if (d.severity == "error") return true; return false; }
int slotAt(const Json& d, const std::string& id) { for (size_t i = 0; i < d["slots"].size(); i++) if (d["slots"][i]["id"] == id) return int(i); return -1; }
/** Square item of `size` px at (x, y) (instance space), a region drawn 1:1. */
cutout::DrawItem square(double x, double y, double size) {
  cutout::DrawItem item;
  item.slot = "s"; item.imageAssetId = "img";
  item.matrix = cutout::Affine::compose(x, y, 0, 1, 1);
  item.width = item.height = size;
  return item;
}
const char* const L_SHAPE = "[0,0, 100,0, 100,40, 40,40, 40,100, 0,100]";  // concave, area 6400

/** The clip/sequence fixture: an L-shaped (concave) clip on its own bone clipping a rotated panel
 *  and a 3x3 mesh grid, a region below it and a 4-frame sequence part above the range. */
Json clipRig() {
  Json d = Json::parse(R"({
    "schemaVersion": 1, "canvas": {"width": 300, "height": 300, "originX": 150, "originY": 150},
    "bones": [{"id": "root", "parent": null, "x": 150, "y": 150}, {"id": "mask", "parent": "root", "x": 3, "y": -2}, {"id": "body", "parent": "root"}],
    "slots": [{"id": "back", "bone": "body", "attachment": "back"}, {"id": "clip", "bone": "mask", "attachment": "clip"},
              {"id": "panel", "bone": "body", "attachment": "panel"}, {"id": "grid", "bone": "body", "attachment": "grid"},
              {"id": "film", "bone": "body", "attachment": "film"}],
    "attachments": [
      {"slot": "back", "name": "back", "imageAssetId": "img-back", "width": 200, "height": 200},
      {"slot": "clip", "name": "clip", "type": "clipping", "end": "grid", "vertices": [-70.5, -60.25, 50.75, -55.5, 45.25, -5.5, -4.75, 0.25, -10.5, 65.75, -72.25, 60.5]},
      {"slot": "panel", "name": "panel", "imageAssetId": "img-panel", "width": 160, "height": 120, "x": 15.5, "y": -10.25, "rotation": 7},
      {"slot": "grid", "name": "grid", "imageAssetId": "img-grid", "width": 120, "height": 120, "x": -20.5, "y": 30.25},
      {"slot": "film", "name": "film", "imageAssetId": "seq-0", "width": 40, "height": 40, "x": 100, "y": 100,
       "sequence": {"frames": ["seq-0", "seq-1", "seq-2", "seq-3"], "setupIndex": 2}}],
    "clips": [{"id": "sway", "durationMs": 1000, "loop": true, "tracks": [
      {"target": "bone:mask", "property": "rotation", "keys": [{"timeMs": 0, "value": 0}, {"timeMs": 500, "value": 25}, {"timeMs": 1000, "value": 0}]},
      {"target": "bone:mask", "property": "x", "keys": [{"timeMs": 0, "value": 3}, {"timeMs": 500, "value": 21.5}, {"timeMs": 1000, "value": 3}]},
      {"target": "slot:film", "property": "sequence", "keys": [{"timeMs": 100, "mode": "loop", "index": 0, "delay": 100}, {"timeMs": 600, "mode": "pingpong", "index": 1, "delay": 50}]}]},
      {"id": "still", "durationMs": 1000, "loop": true, "tracks": []}]
  })");
  return cutout::applyEdit(d, {{"op", "convertToMesh"}, {"slot", "grid"}, {"rows", 3}, {"cols", 3}}).data;
}
Json clipParityScript() {
  const Json controller = {{"schemaVersion", 1}, {"rig", "clip"}, {"defaultClip", "sway"}};
  const Json element = {{"type", "character"}, {"ref", "clip-main"}, {"width", 450}, {"height", 300}, {"props", {{"scale", 1}, {"flipX", true}}}};
  const Json frame = {{"op", "frame"}, {"instance", "a"}}, compact = {{"op", "frame"}, {"instance", "a"}, {"format", 2}};
  Json script = Json::array({
    {{"op", "init"}, {"frameFormats", {2}}, {"documents", Json::array({{{"type", "rig"}, {"id", "clip"}, {"data", clipRig()}}, {{"type", "controller"}, {"id", "clip-main"}, {"data", controller}}})}},
    {{"op", "mount"}, {"instance", "a"}, {"element", element}}, frame, compact});
  for (double dt : {16.0, 99.5, 140.25, 233.0, 180.0, 75.0, 211.0}) { script.push_back({{"op", "update"}, {"dtMs", dt}}); script.push_back(frame); script.push_back(compact); }
  script.push_back({{"op", "meshes"}});
  for (double ms : {0.0, 99.0, 100.0, 350.0, 599.0, 600.0, 649.0, 650.0, 900.0}) script.push_back({{"op", "sample"}, {"rig", clipRig()}, {"clip", "sway"}, {"timeMs", ms}});
  return script;
}

void run() {
  using namespace cutout;
  test("clipping: convex parts of convex and concave polygons", [] {
    const auto square = convexParts({0, 0, 10, 0, 10, 10, 0, 10});
    check(square.size() == 1 && near(area(square[0]), 100), "a square is one part");
    const auto reversed = convexParts({0, 10, 10, 10, 10, 0, 0, 0});
    check(reversed.size() == 1 && near(area(reversed[0]), 100), "clockwise input: one positive part");
    const auto ell = convexParts(Json::parse(L_SHAPE).get<std::vector<double>>());
    double total = 0;
    bool allConvex = true;
    for (const auto& p : ell) { total += area(p); allConvex = allConvex && convex(p) && area(p) > 0; }
    check(ell.size() == 2 && near(total, 6400) && allConvex, "the L splits into 2 convex parts covering it exactly");
    const auto collinear = convexParts({0, 0, 5, 0, 10, 0, 10, 10, 10, 10, 0, 10});
    check(collinear.size() == 1 && near(area(collinear[0]), 100) && convex(collinear[0]), "collinear and repeated points: still one convex part");
    check(convexParts({0, 0, 10, 0, 20, 0}).empty() && convexParts({1, 1, 2, 2}).empty(), "no area = no parts");
    // A star (5 reflex corners) still covers its area exactly.
    std::vector<double> star;
    for (int i = 0; i < 10; i++) { const double r = i % 2 ? 20 : 50, a = i * 3.14159265358979323846 / 5; star.push_back(r * std::cos(a)); star.push_back(r * std::sin(a)); }
    double starArea = 0;
    bool starConvex = true;
    for (const auto& p : convexParts(star)) { starArea += area(p); starConvex = starConvex && convex(p); }
    check(near(starArea, std::fabs(area(star)), 1e-6) && starConvex, "star: convex parts, same area");
  });

  test("clipping: Sutherland-Hodgman with uv interpolation", [] {
    const std::vector<double> box = {0, 0, 10, 0, 10, 10, 0, 10};
    const std::vector<ClipVertex> inside = {{1, 1, 0, 0}, {9, 1, 1, 0}, {5, 9, 0.5, 1}};
    check(clipConvex(inside, box).size() == 3, "inside: unchanged");
    check(clipConvex({{20, 20, 0, 0}, {30, 20, 1, 0}, {25, 30, 0, 1}}, box).empty(), "outside: nothing");
    // A triangle sticking out of the right edge: the cut points carry interpolated uvs.
    const auto cut = clipConvex({{5, 2, 0, 0}, {15, 2, 1, 0}, {5, 8, 0, 1}}, box);
    double s = 0;
    for (size_t i = 0; i < cut.size(); i++) { const auto& a = cut[i]; const auto& b = cut[(i + 1) % cut.size()]; s += a.x * b.y - b.x * a.y; }
    check(cut.size() == 4 && near(std::fabs(s) / 2, 30 - 0.5 * 5 * 3), "area of the kept part (30 - corner 7.5)");
    bool uvOk = true;
    for (const auto& v : cut) uvOk = uvOk && near(v.u, (v.x - 5) / 10) && near(v.v, (v.y - 2) / 6) && v.x <= 10 + 1e-9;
    check(uvOk, "uvs follow the positions linearly; nothing past the edge");
    // Shared edge, opposite directions: bit-identical cut points (no seams).
    const auto left = clipConvex({{2, 3, 0, 0}, {14, 7, 1, 1}, {2, 7, 0, 1}}, box), right = clipConvex({{14, 7, 1, 1}, {2, 3, 0, 0}, {14, 3, 1, 0}}, box);
    auto onEdge = [](const std::vector<ClipVertex>& p) { for (const auto& v : p) if (v.x == 10) return v; return ClipVertex{}; };
    const ClipVertex a = onEdge(left), b = onEdge(right);
    bool same = false;
    for (const auto& v : left) for (const auto& w : right) same = same || (v.x == w.x && v.y == w.y && v.x != 10 && v.y > 3 && v.y < 7);
    check(same || (a.x == 10 && b.x == 10), "the diagonal is cut at the same point from both sides");
  });

  test("clipping: items inside, outside and across the polygon", [] {
    const ClipRegion box = clipRegion({0, 0, 100, 0, 100, 100, 0, 100});
    std::vector<DrawItem> out;
    clipItem(square(10, 10, 20), box, out);
    check(out.size() == 1 && !out[0].mesh() && !out[0].clipped, "fully inside: the region stays a region");
    out.clear();
    clipItem(square(150, 10, 20), box, out);
    check(out.empty(), "fully outside: nothing");
    out.clear();
    clipItem(square(90, 90, 20), box, out);
    check(out.size() == 1 && out[0].mesh() && out[0].clipped && near(meshArea(out[0]), 100), "across a corner: a 10x10 mesh piece");
    bool mapped = true;
    for (size_t v = 0; v < out[0].positions.size() / 2; v++) {
      double x, y;
      out[0].matrix.apply(out[0].uvs[2 * v] * 20, out[0].uvs[2 * v + 1] * 20, x, y);
      mapped = mapped && near(x, out[0].positions[2 * v]) && near(y, out[0].positions[2 * v + 1]) && out[0].positions[2 * v] <= 100 + 1e-9;
    }
    check(mapped, "uvs map back through the item's picture to the clipped positions");
    // Concave clip: a big square covering the L is cut to exactly the L.
    out.clear();
    clipItem(square(-10, -10, 130), clipRegion(Json::parse(L_SHAPE).get<std::vector<double>>()), out);
    check(out.size() == 1 && near(meshArea(out[0]), 6400, 1e-6), "concave L: exactly its area");
    // A mesh item fully inside keeps its own topology (hosts keep their cached mesh id).
    DrawItem mesh = square(0, 0, 10);
    mesh.positions = {20, 20, 40, 20, 40, 40, 20, 40}; mesh.uvs = {0, 0, 1, 0, 1, 1, 0, 1}; mesh.indices = {0, 1, 2, 0, 2, 3};
    out.clear();
    clipItem(mesh, box, out);
    check(out.size() == 1 && !out[0].clipped && out[0].indices == mesh.indices, "mesh inside: unchanged");
    // An empty (zero-area) clip polygon shows nothing of its range.
    out.clear();
    clipItem(square(10, 10, 20), clipRegion({0, 0, 50, 0, 100, 0}), out);
    check(out.empty(), "degenerate clip: nothing drawn");
  });

  test("clipping: large results split under the host mesh limits", [] {
    DrawItem strip = square(0, 0, 1);
    for (int i = 0; i < int(MAX_MESH_TRIANGLES); i++) {
      // Separate thin triangles crossing x = 0: each becomes a quad piece (2 triangles).
      const double y = i * 2.0;
      strip.positions.insert(strip.positions.end(), {-5, y, 5, y, 5, y + 1});
      strip.uvs.insert(strip.uvs.end(), {0, 0, 1, 0, 1, 1});
      strip.indices.insert(strip.indices.end(), {3 * i, 3 * i + 1, 3 * i + 2});
    }
    std::vector<DrawItem> out;
    clipItem(strip, clipRegion({0, -1, 100, -1, 100, 9000, 0, 9000}), out);
    size_t tris = 0;
    bool limits = true;
    for (const auto& item : out) {
      tris += item.indices.size() / 3;
      limits = limits && item.positions.size() / 2 <= MAX_MESH_VERTICES && item.indices.size() / 3 <= MAX_MESH_TRIANGLES && item.clipped;
      for (int i : item.indices) limits = limits && i >= 0 && size_t(i) < item.positions.size() / 2;
    }
    check(out.size() >= 3 && limits, "several items, each within 2048 vertices / 4096 triangles");
    check(tris == 2 * MAX_MESH_TRIANGLES, "every piece kept (" + std::to_string(tris) + " triangles)");
  });

  test("clipping: validation", [] {
    const Json d = clipRig();
    check(validateRig(d).empty(), "fixture validates (no image on the clipping attachment)");
    Json bad = d;
    bad["attachments"][1].erase("end");
    check(hasCode(validateRig(bad), "ANIM_CLIP_END"), "end required");
    bad = d; bad["attachments"][1]["end"] = "nope";
    check(hasCode(validateRig(bad), "ANIM_CLIP_END"), "end slot must exist");
    bad = d; bad["attachments"][1]["end"] = "back";
    check(hasCode(validateRig(bad), "ANIM_CLIP_END"), "end must come after the clipping slot");
    bad = d; bad["attachments"][1]["end"] = "clip";
    check(hasCode(validateRig(bad), "ANIM_CLIP_END"), "end is not the clipping slot itself");
    bad = d; bad["attachments"][1]["vertices"] = {0, 0, 10, 0};
    check(hasCode(validateRig(bad), "ANIM_CLIP_VERTICES"), ">= 3 points");
    bad = d; bad["attachments"][1]["vertices"] = {0, 0, 10, 10, 10, 0, 0, 10};
    check(hasCode(validateRig(bad), "ANIM_CLIP_POLYGON"), "self-intersecting (bow tie) refused");
    bad = d; bad["attachments"][1]["imageAssetId"] = "img";
    check(hasCode(validateRig(bad), "ANIM_CLIP_UNUSED") && !errors(validateRig(bad)), "image fields on a clip only warn");
    bad = d; bad["attachments"][1]["weights"] = Json::array();
    check(hasCode(validateRig(bad), "ANIM_MESH_WEIGHTS"), "weights are checked like a mesh's");
    bad = d; bad["slots"][0]["attachment"] = "back"; bad["attachments"][0]["end"] = "grid";
    check(hasCode(validateRig(bad), "ANIM_CLIP_UNUSED"), "end on a picture warns");
  });

  test("clipping: draw list ranges (end inclusive, before/after unclipped, no nesting)", [] {
    const auto rig = std::make_shared<const Rig>(parseRig("c", clipRig()));
    const Pose pose = samplePose(*rig, rig->clipIndex("still"), 0);
    const auto items = drawItems(*rig, pose);
    std::vector<std::string> slots;
    for (const auto& i : items) slots.push_back(i.slot);
    check(slots == std::vector<std::string>({"back", "panel", "grid", "film"}), "the clipping slot draws nothing; every other part draws once");
    check(!items[0].clipped && !items[0].mesh() && !items[3].clipped && !items[3].mesh(), "below and above the range: untouched regions");
    check(items[1].clipped && items[2].clipped, "panel and grid (end, inclusive) are cut");
    // Every clipped vertex lies inside the (setup-pose) L polygon in rig space.
    std::vector<double> polygon = meshPositions(*rig, pose, 1, *rig->attachment("clip", "clip"));
    for (size_t v = 0; v < polygon.size(); v += 2) { polygon[v] -= rig->originX; polygon[v + 1] -= rig->originY; }
    bool inside = true;
    for (size_t k = 1; k <= 2; k++)
      for (size_t v = 0; v < items[k].positions.size(); v += 2) inside = inside && onOrInside(polygon, items[k].positions[v], items[k].positions[v + 1]);
    check(inside, "clipped vertices stay inside the polygon");
    check(meshArea(items[1]) + 1e-6 < 160 * 120 && meshArea(items[1]) > 100, "the panel lost the part outside");
    // Hidden end slot: the range still ends there (film stays whole).
    Json d = clipRig();
    d["slots"][3]["attachment"] = nullptr;
    const auto hidden = std::make_shared<const Rig>(parseRig("c", d));
    const auto h = drawItems(*hidden, samplePose(*hidden, -1, 0));
    check(h.size() == 3 && h.back().slot == "film" && !h.back().clipped, "an empty end slot still ends the range");
    // Nested clipping (a second clip inside the range) is ignored, as in Spine.
    d = clipRig();
    d["slots"].insert(d["slots"].begin() + 3, Json{{"id", "inner"}, {"bone", "body"}, {"attachment", "inner"}});
    d["attachments"].push_back({{"slot", "inner"}, {"name", "inner"}, {"type", "clipping"}, {"end", "film"}, {"vertices", {0, 0, 5, 0, 5, 5}}});
    const auto nested = std::make_shared<const Rig>(parseRig("c", d));
    const auto n = drawItems(*nested, samplePose(*nested, -1, 0));
    check(n.size() == 4 && n[2].slot == "grid" && n[2].clipped && !n[3].clipped && n[3].slot == "film", "inner clipping ignored, outer range unchanged");
    // Alpha 0 on the clipping slot does not matter.
    d = clipRig();
    d["slots"][1]["alpha"] = 0;
    const auto faded = std::make_shared<const Rig>(parseRig("c", d));
    check(drawItems(*faded, samplePose(*faded, -1, 0))[1].clipped, "a transparent clipping slot still clips");
  });

  test("clipping: the polygon follows its bone, weights and deform keys", [] {
    const auto rig = std::make_shared<const Rig>(parseRig("c", clipRig()));
    const auto a = drawItems(*rig, samplePose(*rig, rig->clipIndex("sway"), 0)), b = drawItems(*rig, samplePose(*rig, rig->clipIndex("sway"), 500));
    check(a[1].positions != b[1].positions, "the mask bone turns: the panel is cut elsewhere");
    // Weighted clip on the body bone: moving body moves the polygon with the panel (same cut).
    Json d = clipRig();
    Json weights = Json::array();
    for (int i = 0; i < 6; i++) weights.push_back(Json::array({{{"bone", "body"}, {"weight", 1}}}));
    d["attachments"][1]["weights"] = weights;
    d["clips"][1]["tracks"] = Json::array({{{"target", "bone:body"}, {"property", "x"}, {"keys", Json::array({{{"timeMs", 0}, {"value", 30}}})}}});
    const auto w = std::make_shared<const Rig>(parseRig("c", d));
    const auto still = drawItems(*w, samplePose(*w, -1, 0)), moved = drawItems(*w, samplePose(*w, w->clipIndex("still"), 0));
    bool shifted = still[1].positions.size() == moved[1].positions.size();
    for (size_t v = 0; shifted && v < still[1].positions.size(); v += 2) {
      // Unweighted the polygon sits on `mask` (bind = setup pose): body x +30 must move every cut vertex by 30 exactly.
      bool found = false;
      for (size_t u = 0; u < moved[1].positions.size() && !found; u += 2) found = near(moved[1].positions[u], still[1].positions[v] + 30, 1e-6) && near(moved[1].positions[u + 1], still[1].positions[v + 1], 1e-6);
      shifted = found;
    }
    check(shifted, "weighted to body: polygon and panel move together");
    // Deform keys move clip points (meshCounts include clip polygons).
    d = clipRig();
    std::vector<double> offsets(12, 0.0);
    offsets[2] = 40;  // second point 40 px right
    d["clips"][1]["tracks"] = Json::array({{{"target", "slot:clip"}, {"property", "deform"}, {"attachment", "clip"}, {"keys", Json::array({{{"timeMs", 0}, {"offsets", offsets}}})}}});
    check(validateRig(d).empty(), "deform keys on a clip polygon validate");
    const auto def = std::make_shared<const Rig>(parseRig("c", d));
    const Pose dp = samplePose(*def, def->clipIndex("still"), 0);
    check(near(meshPositions(*def, dp, 1, *def->attachment("clip", "clip"))[2], 150 + 3 + 50.75 + 40), "deformed clip point");
  });

  test("clipping: deterministic draw lists and compact frame topology", [] {
    const auto rig = std::make_shared<const Rig>(parseRig("c", clipRig()));
    Instance a(rig), b(rig);
    a.play(0, "sway"); b.play(0, "sway");
    a.update(123.25); b.update(123.25);
    Json ja = Json::array(), jb = Json::array();
    for (const auto& i : a.drawList()) ja.push_back(drawItemJson(i));
    for (const auto& i : b.drawList()) jb.push_back(drawItemJson(i));
    check(ja.dump() == jb.dump(), "same input, same items (bit for bit)");
    Host host;
    const Json controller = {{"schemaVersion", 1}, {"rig", "clip"}, {"defaultClip", "sway"}};
    const Json init = host.handle({{"op", "init"}, {"frameFormats", {2}}, {"documents", {{{"type", "rig"}, {"id", "clip"}, {"data", clipRig()}}, {{"type", "controller"}, {"id", "m"}, {"data", controller}}}}});
    host.handle({{"op", "mount"}, {"instance", "x"}, {"element", {{"type", "character"}, {"ref", "m"}, {"width", 300}, {"height", 300}}}});
    check(init["meshes"].size() == 1, "init table: the grid mesh only");
    auto frame = [&] { return Json::parse(host.requestText(Json({{"op", "frame"}, {"instance", "x"}, {"format", 2}}).dump())); };
    const Json f1 = frame();
    check(f1["items"].size() == 4 && f1["items"][1].size() == 13 && f1["items"][1][11].get<int>() >= 1, "clipped panel: a mesh item with a new topology id");
    const Json f2 = frame();
    check(f2["items"][1][11] == f1["items"][1][11] && f2["meshEpoch"] == f1["meshEpoch"], "unchanged cut: the same id again");
    const Json table = host.handle({{"op", "meshes"}, {"ids", {f1["items"][1][11]}}});
    const Json documented = host.handle({{"op", "frame"}, {"instance", "x"}})["items"][1]["mesh"];
    check(table["meshes"].size() == 1 && table["meshes"][0]["indices"] == documented["indices"] && table["meshes"][0]["uvs"] == documented["uvs"], "meshes op serves the clipped topology");
    host.handle({{"op", "update"}, {"dtMs", 250}});
    const Json f3 = frame();
    check(f3["items"][1][11] != f1["items"][1][11], "the clip moved: another topology id");
    // Thousands of distinct cuts: the table restarts under a new epoch instead of growing forever.
    uint64_t epoch = f3["meshEpoch"].get<uint64_t>();
    bool restarted = false;
    for (int i = 0; i < 2100 && !restarted; i++) {
      host.handle({{"op", "update"}, {"dtMs", 1.37}});
      const Json f = frame();
      restarted = f["meshEpoch"].get<uint64_t>() != epoch;
    }
    check(restarted, "the dynamic topology table is bounded (new epoch)");
  });

  test("sequence: frame index of every mode at boundary times", [] {
    auto frame = [](SequenceMode m, int index, double elapsed, size_t count = 4, double delay = 100) {
      SequenceState s; s.keyed = true; s.mode = m; s.index = index; s.elapsedMs = elapsed; s.delay = delay;
      return sequenceFrame(s, count, 0);
    };
    const double times[] = {0, 99.999, 100, 199, 300, 399, 400, 700, 1000};
    auto row = [&](SequenceMode m, int index = 0) { std::vector<int> out; for (double t : times) out.push_back(frame(m, index, t)); return out; };
    check(row(SequenceMode::Hold, 2) == std::vector<int>({2, 2, 2, 2, 2, 2, 2, 2, 2}), "hold");
    check(row(SequenceMode::Once) == std::vector<int>({0, 0, 1, 1, 3, 3, 3, 3, 3}), "once");
    check(row(SequenceMode::Loop) == std::vector<int>({0, 0, 1, 1, 3, 3, 0, 3, 2}), "loop");
    check(row(SequenceMode::Pingpong) == std::vector<int>({0, 0, 1, 1, 3, 3, 2, 1, 2}), "pingpong");
    check(row(SequenceMode::OnceReverse) == std::vector<int>({3, 3, 2, 2, 0, 0, 0, 0, 0}), "onceReverse");
    check(row(SequenceMode::LoopReverse) == std::vector<int>({3, 3, 2, 2, 0, 0, 3, 0, 1}), "loopReverse");
    check(row(SequenceMode::PingpongReverse) == std::vector<int>({3, 3, 2, 2, 0, 0, 1, 2, 1}), "pingpongReverse");
    check(frame(SequenceMode::Loop, 1, 0) == 1 && frame(SequenceMode::Loop, 3, 100) == 0, "start index offsets the frames");
    check(frame(SequenceMode::Loop, 0, 250, 12, 1000.0 / 12) == 3, "83.333 ms frames: 250 ms is frame 3 (no rounding down)");
    check(frame(SequenceMode::Pingpong, 0, 500, 1) == 0 && frame(SequenceMode::PingpongReverse, 0, 500, 1) == 0, "one frame: pingpong stays");
    check(frame(SequenceMode::Hold, 9, 0) == 3, "hold past the end clamps");
    SequenceState none;
    check(sequenceFrame(none, 4, 2) == 2 && sequenceFrame(none, 4, 9) == 3, "unkeyed: setupIndex (clamped)");
  });

  test("sequence: keys pick the drawn image over clip time, mixes switch halfway", [] {
    const auto rig = std::make_shared<const Rig>(parseRig("c", clipRig()));
    const int sway = rig->clipIndex("sway");
    auto image = [&](double t) { return drawItems(*rig, samplePose(*rig, sway, t)).back().imageAssetId; };
    check(image(0) == "seq-2" && image(99) == "seq-2", "before the first key: setupIndex");
    check(image(100) == "seq-0" && image(199.5) == "seq-0" && image(200) == "seq-1" && image(450) == "seq-3" && image(500) == "seq-0", "loop, 100 ms frames from 100 ms");
    check(image(600) == "seq-1" && image(650) == "seq-2" && image(700) == "seq-3" && image(750) == "seq-2" && image(850) == "seq-0", "pingpong from index 1, 50 ms frames");
    Host host;
    const Json p = host.handle({{"op", "sample"}, {"rig", clipRig()}, {"clip", "sway"}, {"timeMs", 450}})["pose"];
    check(p["slots"]["film"]["sequence"] == 3 && !p["slots"]["panel"].contains("sequence"), "sample reports the frame of sequence slots only");
    // Cross-fade: the incoming clip's frame wins from halfway (like attachments).
    Json d = clipRig();
    d["mix"] = {{"defaultMs", 200}};
    const auto mixed = std::make_shared<const Rig>(parseRig("c", d));
    Instance inst(mixed);
    inst.play(0, "sway");
    inst.update(420);
    inst.play(0, "still");
    inst.update(50);
    check(inst.drawList().back().imageAssetId == "seq-3", "first half: the old clip's frame");
    inst.update(60);
    check(inst.drawList().back().imageAssetId == "seq-2", "second half: the new clip (setup frame)");
    // Packed frames name their own page region.
    d = clipRig();
    d["attachments"][4]["sequence"]["atlas"] = Json::array({nullptr, {{"page", "pg"}, {"x", 2}, {"y", 2}, {"width", 40}, {"height", 40}}, nullptr, {{"page", "pg"}, {"x", 44}, {"y", 2}, {"width", 30}, {"height", 20}, {"trim", {{"x", 5}, {"y", 10}, {"width", 30}, {"height", 20}}}}});
    check(validateRig(d).empty(), "per-frame atlas validates");
    const auto packed = std::make_shared<const Rig>(parseRig("c", d));
    const auto f1 = drawItems(*packed, samplePose(*packed, sway, 200)).back(), f3 = drawItems(*packed, samplePose(*packed, sway, 400)).back(), f0 = drawItems(*packed, samplePose(*packed, sway, 100)).back();
    check(f1.imageAssetId == "pg" && f1.hasRegion && f1.region[0] == 2, "frame 1 from the page");
    check(f3.imageAssetId == "pg" && f3.region[0] == 44 && f3.width == 30 && near(f3.matrix.e, f0.matrix.e + 5), "trimmed frame: kept rectangle in place");
    check(f0.imageAssetId == "seq-0" && !f0.hasRegion, "an unpacked frame keeps its own image");
  });

  test("sequence: validation", [] {
    const Json d = clipRig();
    Json bad = d;
    bad["attachments"][4]["sequence"]["frames"] = Json::array();
    check(hasCode(validateRig(bad), "ANIM_SEQUENCE_FRAMES"), "frames required");
    bad = d; bad["attachments"][4]["imageAssetId"] = "seq-1";
    check(hasCode(validateRig(bad), "ANIM_SEQUENCE_IMAGE"), "imageAssetId = frames[0]");
    bad = d; bad["attachments"][4]["sequence"]["setupIndex"] = 4;
    check(hasCode(validateRig(bad), "ANIM_SEQUENCE_INDEX"), "setupIndex in range");
    bad = d; bad["clips"][0]["tracks"][2]["keys"][0]["index"] = 4;
    check(hasCode(validateRig(bad), "ANIM_SEQUENCE_INDEX"), "key index in range");
    bad = d; bad["clips"][0]["tracks"][2]["keys"][0]["delay"] = 0;
    check(hasCode(validateRig(bad), "ANIM_SEQUENCE_DELAY"), "delay > 0");
    bad = d; bad["clips"][0]["tracks"][2]["keys"][0]["mode"] = "bounce";
    check(hasCode(validateRig(bad), "ANIM_SEQUENCE_KEY"), "known modes only");
    bad = d; bad["clips"][0]["tracks"][2]["keys"][0]["interp"] = "linear";
    check(hasCode(validateRig(bad), "ANIM_INTERP"), "step only");
    bad = d; bad["clips"][0]["tracks"][2]["target"] = "slot:panel";
    check(hasCode(validateRig(bad), "ANIM_SEQUENCE_TARGET"), "the slot needs a sequence attachment");
    bad = d; bad["clips"][0]["tracks"][2]["target"] = "bone:body";
    check(hasCode(validateRig(bad), "ANIM_PROPERTY"), "slot tracks only");
    bad = d; bad["attachments"][1]["sequence"] = {{"frames", {"a"}}};
    check(hasCode(validateRig(bad), "ANIM_CLIP_UNUSED"), "clips have no sequence (warned)");
    bad = d; bad["attachments"][4]["sequence"]["atlas"] = Json::array({nullptr});
    check(hasCode(validateRig(bad), "ANIM_ATLAS"), "one region per frame");
  });

  test("editing: clipping ops", [] {
    Json d = clipRig();
    // A second clip over the film part, drawn in rig space at the setup pose.
    EditResult r = applyEdit(d, {{"op", "addClipping"}, {"slot", "film-clip"}, {"before", "film"}, {"worldVertices", {240, 240, 300, 240, 270, 290}}});
    const int at = slotAt(r.data, "film-clip");
    check(at == 4 && r.data["slots"][4]["bone"] == "body" && r.data["slots"][4]["attachment"] == "clip", "inserted before film on its bone");
    const Json& clip = r.data["attachments"].back();
    check(clip["type"] == "clipping" && clip["end"] == "film" && clip["vertices"] == Json::array({90, 90, 150, 90, 120, 140}), "rig px -> bone px, end = before");
    check(validateRig(r.data).empty(), "valid");
    bool refused = false;
    try { applyEdit(d, {{"op", "addClipping"}, {"slot", "x"}, {"before", "film"}, {"vertices", {0, 0, 10, 10, 10, 0, 0, 10}}}); } catch (const EditError&) { refused = true; }
    check(refused, "a bow tie is refused");
    refused = false;
    try { applyEdit(d, {{"op", "convertToMesh"}, {"slot", "clip"}}); } catch (const EditError&) { refused = true; }
    check(refused, "a clip has no picture to turn into a mesh");
    r = applyEdit(r.data, {{"op", "setClipping"}, {"slot", "clip"}, {"end", "panel"}, {"vertices", {0, 0, 50, 0, 50, 50, 0, 50}}});
    check(r.data["attachments"][1]["end"] == "panel" && r.data["attachments"][1]["vertices"].size() == 8, "setClipping: end and polygon");
    refused = false;
    try { applyEdit(r.data, {{"op", "moveSlot"}, {"slot", "panel"}, {"index", 0}}); } catch (const EditError&) { refused = true; }
    check(refused, "moving the end below the clip is refused (ANIM_CLIP_END)");
    // Deleting the end slot shrinks the range; deleting the last clipped slot removes the clip.
    Json e = applyEdit(clipRig(), {{"op", "deleteSlot"}, {"slot", "grid"}}).data;
    check(e["attachments"][1]["end"] == "panel", "end moves to the slot below");
    EditResult gone = applyEdit(e, {{"op", "deleteSlot"}, {"slot", "panel"}});
    check(gone.data["attachments"].size() == 2 && slotAt(gone.data, "clip") < 0 && hasCode(gone.diagnostics, "ANIM_CLIP_REMOVED") && validateRig(gone.data).empty(), "empty range: the clipping slot goes too");
  });

  test("editing: sequence ops and keys", [] {
    Json d = clipRig();
    EditResult r = applyEdit(d, {{"op", "setSequence"}, {"slot", "panel"}, {"frames", {"p-0", "p-1", "p-2"}}, {"setupIndex", 1}});
    const Json& panel = r.data["attachments"][2];
    check(panel["imageAssetId"] == "p-0" && panel["sequence"]["frames"].size() == 3 && panel["sequence"]["setupIndex"] == 1, "frames, imageAssetId = frame 0");
    r = applyEdit(r.data, {{"op", "setKey"}, {"clip", "still"}, {"target", "slot:panel"}, {"property", "sequence"}, {"timeMs", 200}, {"mode", "pingpong"}, {"index", 2}, {"delay", 40}});
    const Json& key = r.data["clips"][1]["tracks"][0]["keys"][0];
    check(key == Json({{"timeMs", 200}, {"mode", "pingpong"}, {"index", 2}, {"delay", 40}}), "sequence key {mode, index, delay}");
    bool refused = false;
    try { applyEdit(r.data, {{"op", "setKey"}, {"clip", "still"}, {"target", "slot:back"}, {"property", "sequence"}, {"timeMs", 0}}); } catch (const EditError&) { refused = true; }
    check(refused, "no sequence on the slot: refused");
    refused = false;
    try { applyEdit(r.data, {{"op", "setKey"}, {"clip", "still"}, {"target", "slot:panel"}, {"property", "sequence"}, {"timeMs", 0}, {"index", 3}}); } catch (const EditError&) { refused = true; }
    check(refused, "index past the frames: refused");
    // Copy / paste / move carry the key fields.
    const Json refs = {{{"target", "slot:panel"}, {"property", "sequence"}, {"timeMs", 200}}};
    const Json board = applyEdit(r.data, {{"op", "copyKeys"}, {"clip", "still"}, {"keys", refs}}).extra["clipboard"];
    check(board["keys"][0]["mode"] == "pingpong" && board["keys"][0]["delay"] == 40 && !board["keys"][0].contains("interp"), "copied with mode/index/delay, no interp");
    Json pasted = applyEdit(r.data, {{"op", "pasteKeys"}, {"clip", "sway"}, {"timeMs", 300}, {"keys", board["keys"]}, {"target", "slot:panel"}}).data;
    const Json* track = nullptr;
    for (const auto& t : pasted["clips"][0]["tracks"]) if (t["target"] == "slot:panel") track = &t;
    check(track && (*track)["keys"][0] == Json({{"timeMs", 300}, {"mode", "pingpong"}, {"index", 2}, {"delay", 40}}), "pasted elsewhere");
    Json moved = applyEdit(r.data, {{"op", "moveKeys"}, {"clip", "still"}, {"keys", refs}, {"deltaMs", 100}}).data;
    check(moved["clips"][1]["tracks"][0]["keys"][0]["timeMs"] == 300 && moved["clips"][1]["tracks"][0]["keys"][0]["mode"] == "pingpong", "moved with its fields");
    // Fewer frames clamp the keys; no frames left removes the sequence and its keys.
    r = applyEdit(r.data, {{"op", "setSequence"}, {"slot", "panel"}, {"frames", {"p-0", "p-1"}}});
    check(r.data["clips"][1]["tracks"][0]["keys"][0]["index"] == 1 && hasCode(r.diagnostics, "ANIM_SEQUENCE_INDEX_CLAMPED") && r.data["attachments"][2]["sequence"]["setupIndex"] == 1, "clamped to the last frame");
    r = applyEdit(r.data, {{"op", "setSequence"}, {"slot", "panel"}, {"frames", nullptr}});
    check(!r.data["attachments"][2].contains("sequence") && r.data["clips"][1]["tracks"].empty() && hasCode(r.diagnostics, "ANIM_SEQUENCE_KEYS_REMOVED"), "removed with its keys");
    check(validateRig(r.data).empty(), "still valid");
  });
}
}  // namespace clipseq
