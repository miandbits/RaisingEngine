// duplicateBones (plugin 1.19.0, 복제 / Ctrl+D), included by core-tests.cpp after the other fixtures.
#pragma once

namespace duplicate {
using cutout::Json;

/** root > arm > hand (+ hand-goal, an aim target inside the arm subtree) and root > aim (outside).
 *  Parts: body (root), sleeve (arm: a weighted mesh on arm + root), glove (hand), cuff (hand: a
 *  linked copy of sleeve), clip (hand: clipping glove..cuff), top (root, drawn last). A skin gives
 *  glove a second picture. IK: aim on hand -> hand-goal (inside), arm-hand two-bone -> aim (outside);
 *  physics on hand; a transform constraint hand -> root (outside). */
Json rig() {
  using namespace cutout;
  Json d = Json::parse(R"({
    "schemaVersion": 1, "canvas": {"width": 400, "height": 400, "originX": 200, "originY": 380},
    "bones": [
      {"id": "root", "parent": null, "x": 200, "y": 200},
      {"id": "arm", "parent": "root", "x": 40, "rotation": 30, "length": 60},
      {"id": "hand", "parent": "arm", "x": 60, "length": 20},
      {"id": "hand-goal", "parent": "arm", "x": 90, "y": 10},
      {"id": "aim", "parent": "root", "x": 120, "y": 80}
    ],
    "slots": [
      {"id": "body", "bone": "root", "attachment": "body"},
      {"id": "sleeve", "bone": "arm", "attachment": "sleeve"},
      {"id": "clip", "bone": "hand", "attachment": "clip"},
      {"id": "glove", "bone": "hand", "attachment": "glove"},
      {"id": "cuff", "bone": "hand", "attachment": "cuff"},
      {"id": "top", "bone": "root", "attachment": "top"}
    ],
    "attachments": [
      {"slot": "body", "name": "body", "imageAssetId": "img-body", "width": 80, "height": 120},
      {"slot": "sleeve", "name": "sleeve", "imageAssetId": "img-sleeve", "width": 60, "height": 20},
      {"slot": "clip", "name": "clip", "type": "clipping", "vertices": [0, -20, 40, -20, 40, 20, 0, 20], "end": "cuff"},
      {"slot": "glove", "name": "glove", "imageAssetId": "img-glove", "width": 20, "height": 20},
      {"slot": "cuff", "name": "cuff", "imageAssetId": "img-cuff", "width": 60, "height": 20},
      {"slot": "top", "name": "top", "imageAssetId": "img-top", "width": 10, "height": 10}
    ],
    "skins": [{"id": "winter", "attachments": [{"slot": "glove", "name": "glove", "imageAssetId": "img-mitten", "width": 22, "height": 22}]}],
    "ik": [
      {"id": "hand-aim", "bones": ["hand"], "target": "hand-goal"},
      {"id": "arm-ik", "bones": ["arm", "hand"], "target": "aim"}
    ],
    "physics": [{"id": "hand-physics", "bone": "hand", "rotate": 1}],
    "transform": [{"id": "hand-follow", "bones": ["hand"], "target": "root", "mixRotate": 0.5}],
    "clips": [{"id": "wave", "durationMs": 500, "loop": true, "tracks": [{"target": "bone:arm", "property": "rotation", "keys": [{"timeMs": 0, "value": 30}, {"timeMs": 250, "value": 50}]}]}]
  })");
  d = applyEdit(d, {{"op", "convertToMesh"}, {"slot", "sleeve"}, {"rows", 1}, {"cols", 2}}).data;
  d = applyEdit(d, {{"op", "autoWeights"}, {"slot", "sleeve"}, {"bones", {"arm", "root"}}}).data;
  return applyEdit(d, {{"op", "linkMesh"}, {"slot", "cuff"}, {"parent", "sleeve/sleeve"}}).data;
}
const Json* byId(const Json& d, const char* list, const std::string& id) {
  if (d.contains(list)) for (const auto& x : d[list]) if (x.value("id", std::string()) == id) return &x;
  return nullptr;
}
std::vector<std::string> slotOrder(const Json& d) {
  std::vector<std::string> out;
  for (const auto& s : d["slots"]) out.push_back(s["id"].get<std::string>());
  return out;
}
const Json* attachmentOf(const Json& list, const std::string& slot) {
  for (const auto& a : list) if (a["slot"] == slot) return &a;
  return nullptr;
}
bool refused(const Json& d, const Json& op) {
  try { cutout::applyEdit(d, op); } catch (const cutout::EditError&) { return true; }
  return false;
}

void run() {
  using namespace cutout;
  test("duplicate: a bone subtree with its parts, skins, weights, links and inside constraints", [] {
    const Json base = rig();
    const auto r = applyEdit(base, {{"op", "duplicateBones"}, {"bones", {"arm"}}});
    const Json& d = r.data;
    check(r.extra["bones"] == Json({{"arm", "arm-2"}, {"hand", "hand-2"}, {"hand-goal", "hand-goal-2"}}), "bones arm, hand, hand-goal -> -2 (" + r.extra["bones"].dump() + ")");
    check(r.extra["slots"] == Json({{"sleeve", "sleeve-2"}, {"clip", "clip-2"}, {"glove", "glove-2"}, {"cuff", "cuff-2"}}), "every part on the subtree");
    check(byId(d, "bones", "arm-2") && (*byId(d, "bones", "arm-2"))["parent"] == "root" && (*byId(d, "bones", "hand-2"))["parent"] == "arm-2", "the copy hangs where the original does");
    check(d["bones"].size() == 8 && base["bones"].size() == 5, "three bones added");
    // Same setup world: the copy sits exactly on top of the original.
    const auto w = setupWorld(d);
    for (const char* b : {"arm", "hand", "hand-goal"}) {
      const auto& a = w.at(b);
      const auto& c = w.at(std::string(b) + "-2");
      check(std::fabs(a.a - c.a) + std::fabs(a.b - c.b) + std::fabs(a.e - c.e) + std::fabs(a.f - c.f) < 1e-9, std::string(b) + " copy in place");
    }
    // Draw order: the copies (in their own order) right after the last original of the set (cuff), before top.
    check(slotOrder(d) == std::vector<std::string>({"body", "sleeve", "clip", "glove", "cuff", "sleeve-2", "clip-2", "glove-2", "cuff-2", "top"}), "copied parts above the originals, top stays on top");
    const Json* sleeve = attachmentOf(d["attachments"], "sleeve-2");
    bool remapped = sleeve && sleeve->contains("weights");
    if (remapped) for (const auto& v : (*sleeve)["weights"]) for (const auto& x : v) remapped = remapped && (x["bone"] == "arm-2" || x["bone"] == "root");
    check(remapped, "mesh weights follow the copied bones (root stays root)");
    const Json* cuff = attachmentOf(d["attachments"], "cuff-2");
    check(cuff && (*cuff)["linkedMesh"]["parent"] == "sleeve-2/sleeve", "a linked mesh links to the copied parent");
    const Json* clip = attachmentOf(d["attachments"], "clip-2");
    check(clip && (*clip)["end"] == "cuff-2", "a clip's end follows into the copy");
    const Json* mitten = attachmentOf(d["skins"][0]["attachments"], "glove-2");
    check(mitten && (*mitten)["imageAssetId"] == "img-mitten", "skin entries are copied too");
    check(byId(d, "ik", "hand-aim-2") && (*byId(d, "ik", "hand-aim-2"))["target"] == "hand-goal-2" && (*byId(d, "ik", "hand-aim-2"))["bones"] == Json({"hand-2"}), "an IK inside the copy comes along");
    check(!byId(d, "ik", "arm-ik-2") && d["ik"].size() == 2 + 1, "an IK reaching outside (target aim) does not");
    check(byId(d, "physics", "hand-physics-2") && (*byId(d, "physics", "hand-physics-2"))["bone"] == "hand-2", "physics of a copied bone");
    check(!byId(d, "transform", "hand-follow-2"), "a transform constraint with an outside target stays single");
    check(d["clips"] == base["clips"], "clips and keys untouched (the copy starts unanimated)");
    check(validateRig(d).empty() || std::none_of(validateRig(d).begin(), validateRig(d).end(), [](const Diagnostic& x) { return x.severity == "error"; }), "valid");
    // Nothing of the original changed.
    for (const char* list : {"bones", "slots"})
      for (const auto& x : base[list]) check(*byId(d, list, x["id"].get<std::string>()) == x, std::string("original ") + list + " " + x["id"].get<std::string>() + " unchanged");
  });

  test("duplicate: next free suffix, parts alone, nested roots, refusals", [] {
    Json d = applyEdit(rig(), {{"op", "duplicateBones"}, {"bones", {"hand"}}}).data;
    const auto again = applyEdit(d, {{"op", "duplicateBones"}, {"bones", {"hand-2"}}});
    check(again.extra["bones"]["hand-2"] == "hand-3" && again.extra["slots"]["glove-2"] == "glove-3", "a copy of a copy counts on (hand-2 -> hand-3)");
    // Listing a bone and its own descendant copies the subtree once.
    const auto nested = applyEdit(rig(), {{"op", "duplicateBones"}, {"bones", {"arm", "hand"}}});
    check(nested.extra["bones"].size() == 3, "nested roots: one copy each");
    // A part alone: on the same bone, right after itself.
    const auto part = applyEdit(rig(), {{"op", "duplicateBones"}, {"slots", {"body"}}});
    check(part.extra["slots"] == Json({{"body", "body-2"}}) && part.extra["bones"].empty(), "slot copy only");
    check(slotOrder(part.data)[1] == "body-2" && (*byId(part.data, "slots", "body-2"))["bone"] == "root", "drawn right above the original on its bone");
    // A clip whose end is not copied stays behind (warning); the rest is copied.
    const auto clipped = applyEdit(rig(), {{"op", "duplicateBones"}, {"slots", {"clip", "glove"}}});
    bool warned = false;
    for (const auto& x : clipped.diagnostics) warned = warned || x.code == "ANIM_DUPLICATE_CLIP_SKIPPED";
    check(warned && clipped.extra["slots"] == Json({{"glove", "glove-2"}}), "clip without its end skipped");
    check(refused(rig(), {{"op", "duplicateBones"}}), "nothing to copy refused");
    check(refused(rig(), {{"op", "duplicateBones"}, {"bones", {"nope"}}}), "unknown bone refused");
    check(refused(rig(), {{"op", "duplicateBones"}, {"slots", {"nope"}}}), "unknown slot refused");
    check(refused(rig(), {{"op", "duplicateBones"}, {"bones", "arm"}}), "bones must be an array");
    // In one batch with other ops (what the LLM `edit` op sends): one result.
    const auto batch = applyEdit(rig(), {{"op", "batch"}, {"operations", {{{"op", "duplicateBones"}, {"bones", {"aim"}}}, {{"op", "setBone"}, {"bone", "aim-2"}, {"x", 10}}}}});
    check(byId(batch.data, "bones", "aim-2") && (*byId(batch.data, "bones", "aim-2"))["x"] == 10, "batch: the copy is editable in the same edit");
  });
}
}  // namespace duplicate
