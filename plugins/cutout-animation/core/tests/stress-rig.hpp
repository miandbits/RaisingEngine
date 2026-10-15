// Deterministic large-rig generator for performance tests (plugin 1.9.0). No external assets:
// image ids follow the slot/attachment names, so tests that need pixels generate one PNG per id.
// Shared by core-tests (`--stress-rig` prints it for the node/player/service benchmarks),
// and the Qt panel tests, so every layer measures the same character.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "cutout/rig.hpp"

namespace cutout_stress {
using cutout::Json;

struct Spec {
  int bones = 200;           // >= 32 (skeleton core) ; the rest are 6-bone hair/cloth chains
  int slots = 150;           // draw-order slots, one default attachment each
  int meshes = 40;           // the first `meshes` slots carry grid meshes
  int weighted = 30;         // of those, weighted between the slot bone and its parent
  int outlined = 10;         // of those, with an outline (`hull` + `edges`) so retriangulate rebuilds it
  int gridCols = 6, gridRows = 5;
  int clips = 30;            // clip-0 is the dense dopesheet clip, the others are ordinary
  int bigTracks = 150, bigKeys = 20;
  int clipTracks = 40, clipKeys = 5;
  int skins = 4, skinParts = 25;
  int expressions = 6;
};

/** Small LCG: identical output on every platform and compiler (no <random> distributions). */
struct Lcg {
  uint32_t s;
  explicit Lcg(uint32_t seed) : s(seed) {}
  uint32_t next() { s = s * 1664525u + 1013904223u; return s >> 8; }
  /** Uniform in [lo, hi], rounded to 1/100 so the JSON stays short and exact. */
  double range(double lo, double hi) { return std::round((lo + (hi - lo) * (next() % 10001) / 10000.0) * 100) / 100; }
};

inline std::string boneName(const Spec& s, int i) {
  // 0 root, 1..10 spine, 11 head, 12..27 limbs (4 × upper/lower/hand/target), 28..31 aims, then chains.
  if (i == 0) return "root";
  if (i <= 10) return "spine-" + std::to_string(i - 1);
  if (i == 11) return "head";
  if (i < 28) { static const char* part[] = {"upper", "lower", "hand", "target"}; return "limb-" + std::to_string((i - 12) / 4) + "-" + part[(i - 12) % 4]; }
  if (i < 32) return i < 30 ? "aim-" + std::to_string(i - 28) : "aim-target-" + std::to_string(i - 30);
  (void)s;
  return "chain-" + std::to_string((i - 32) / 6) + "-" + std::to_string((i - 32) % 6);
}
inline std::string slotName(int i) { return "part-" + std::to_string(i); }
/** Bone a slot hangs on: meshes on chain bones that have a chain parent (so weights can blend), others spread out. */
inline int slotBone(const Spec& s, int slot) {
  const int chains = (s.bones - 32) / 6;
  if (slot < s.meshes && chains > 0) return 32 + (slot % chains) * 6 + 1 + (slot / chains) % 5;
  return (slot * 7 + 3) % s.bones;
}

inline Json bones(const Spec& s, Lcg& rng) {
  Json out = Json::array();
  for (int i = 0; i < s.bones; i++) {
    Json b{{"id", boneName(s, i)}};
    if (i == 0) { b["parent"] = nullptr; b["x"] = 512; b["y"] = 900; }
    else if (i <= 10) { b["parent"] = boneName(s, i == 1 ? 0 : i - 1); b["y"] = -45; b["rotation"] = rng.range(-3, 3); b["length"] = 45; }
    else if (i == 11) { b["parent"] = "spine-9"; b["y"] = -60; b["length"] = 80; }
    else if (i < 28) {
      const int limb = (i - 12) / 4, part = (i - 12) % 4;
      const double side = limb % 2 ? 1 : -1;
      if (part == 0) { b["parent"] = limb < 2 ? "spine-8" : "spine-0"; b["x"] = side * 40; b["rotation"] = 90 - side * 20; b["length"] = 110; }
      else if (part == 1) { b["parent"] = boneName(s, i - 1); b["x"] = 110; b["length"] = 100; }
      else if (part == 2) { b["parent"] = boneName(s, i - 1); b["x"] = 100; }
      else { b["parent"] = "root"; b["x"] = side * 150; b["y"] = limb < 2 ? -420 : -10; }
    } else if (i < 30) { b["parent"] = "head"; b["x"] = (i - 28) ? 25 : -25; b["y"] = -40; b["length"] = 12; }
    else if (i < 32) { b["parent"] = "root"; b["x"] = (i - 30) ? 300 : -300; b["y"] = -700; }
    else {
      const int chain = (i - 32) / 6, link = (i - 32) % 6;
      b["parent"] = link ? boneName(s, i - 1) : boneName(s, 1 + chain % 12);  // spine-* or head roots
      b["x"] = link ? 30 : rng.range(-30, 30);
      b["rotation"] = link ? rng.range(-15, 15) : rng.range(60, 120);
      b["length"] = 30;
    }
    out.push_back(b);
  }
  return out;
}

/** gridCols × gridRows cells over a w × h image; outlined meshes list the perimeter first (hull). */
inline void meshInto(const Spec& s, Json& a, bool weighted, bool outlined, const std::string& bone, const std::string& parent) {
  const int cols = s.gridCols, rows = s.gridRows, n = (cols + 1) * (rows + 1);
  const double w = a["width"].get<double>(), h = a["height"].get<double>();
  std::vector<int> order;  // new index -> grid index
  if (outlined) {
    for (int c = 0; c <= cols; c++) order.push_back(c);
    for (int r = 1; r <= rows; r++) order.push_back(r * (cols + 1) + cols);
    for (int c = cols - 1; c >= 0; c--) order.push_back(rows * (cols + 1) + c);
    for (int r = rows - 1; r >= 1; r--) order.push_back(r * (cols + 1));
    for (int r = 1; r < rows; r++) for (int c = 1; c < cols; c++) order.push_back(r * (cols + 1) + c);
  } else for (int g = 0; g < n; g++) order.push_back(g);
  std::vector<int> index(size_t(n), 0);
  for (size_t k = 0; k < order.size(); k++) index[size_t(order[k])] = int(k);
  Json vertices = Json::array(), uvs = Json::array(), triangles = Json::array(), weights = Json::array();
  for (int g : order) {
    const int r = g / (cols + 1), c = g % (cols + 1);
    vertices.push_back(w * c / cols); vertices.push_back(h * r / rows);
    uvs.push_back(double(c) / cols); uvs.push_back(double(r) / rows);
    const double t = double(c) / cols;
    if (t <= 0) weights.push_back(Json::array({{{"bone", parent}, {"weight", 1}}}));
    else if (t >= 1) weights.push_back(Json::array({{{"bone", bone}, {"weight", 1}}}));
    else weights.push_back(Json::array({{{"bone", parent}, {"weight", 1 - t}}, {{"bone", bone}, {"weight", t}}}));
  }
  for (int r = 0; r < rows; r++)
    for (int c = 0; c < cols; c++) {
      const int p = r * (cols + 1) + c, q = p + 1, u = p + cols + 1, v = u + 1;
      for (int x : {p, q, v, p, v, u}) triangles.push_back(index[size_t(x)]);
    }
  a["type"] = "mesh";
  a["vertices"] = vertices;
  a["uvs"] = uvs;
  a["triangles"] = triangles;
  if (weighted) a["weights"] = weights;
  if (outlined) {
    const int hull = 2 * (cols + rows);
    a["hull"] = hull;
    Json edges = Json::array();
    for (int k = 0; k < hull; k++) { edges.push_back(k); edges.push_back((k + 1) % hull); }
    a["edges"] = edges;
  }
}

inline Json bezier(int64_t t, double v, int k) {
  Json key{{"timeMs", t}, {"value", v}};
  if (k % 2 == 0) { key["interp"] = "bezier"; key["curve"] = Json::array({0.25, 0.1, 0.25, 1}); }
  return key;
}

/** The stress rig: validates without errors (warnings allowed, none expected). */
inline Json rig(const Spec& s = {}) {
  Lcg rng(0x5eed);
  Json out{{"schemaVersion", 1}, {"name", "Stress"}, {"canvas", {{"width", 1024}, {"height", 1024}, {"originX", 512}, {"originY", 960}}}};
  out["bones"] = bones(s, rng);
  Json slots = Json::array(), attachments = Json::array();
  for (int i = 0; i < s.slots; i++) {
    const int bone = slotBone(s, i);
    slots.push_back({{"id", slotName(i)}, {"bone", boneName(s, bone)}, {"attachment", "a"}});
    const bool mesh = i < s.meshes;
    Json a{{"slot", slotName(i)}, {"name", "a"}, {"imageAssetId", "stress-" + slotName(i) + "-a"},
           {"width", mesh ? 120 : 24 + int(rng.next() % 76)}, {"height", mesh ? 80 : 24 + int(rng.next() % 76)},
           {"pivotX", mesh ? 0 : 0.5}, {"pivotY", 0.5}, {"rotation", rng.range(-10, 10)}};
    if (mesh) {
      // Weighted meshes blend from the parent bone (left edge) to the slot bone (right edge).
      const std::string parent = out["bones"][size_t(bone)]["parent"].get<std::string>();
      meshInto(s, a, i < s.weighted, i < s.outlined, boneName(s, bone), parent);
    }
    attachments.push_back(a);
    // Alternate pictures for attachment keys and expressions.
    if (i >= 100 && i < 120)
      attachments.push_back({{"slot", slotName(i)}, {"name", "b"}, {"imageAssetId", "stress-" + slotName(i) + "-b"}, {"width", 32}, {"height", 32}});
  }
  out["slots"] = slots;
  out["attachments"] = attachments;
  // Outfits replace region pictures (same geometry, other images).
  Json skins = Json::array();
  for (int k = 0; k < s.skins; k++) {
    Json list = Json::array();
    for (int j = 0; j < s.skinParts; j++) {
      const int slot = s.meshes + (k * s.skinParts + j) % std::max(1, s.slots - s.meshes);
      Json a;
      for (const auto& x : attachments) if (x["slot"] == slotName(slot) && x["name"] == "a") a = x;
      a["imageAssetId"] = "stress-skin-" + std::to_string(k) + "-" + slotName(slot);
      list.push_back(a);
    }
    skins.push_back({{"id", "skin-" + std::to_string(k)}, {"attachments", list}});
  }
  out["skins"] = skins;
  Json expressions = Json::array();
  for (int k = 0; k < s.expressions; k++) {
    Json slotsOf = Json::object(), bonesOf = Json::object();
    for (int j = 0; j < 5; j++) slotsOf[slotName(100 + (k * 5 + j) % 20)] = j % 4 == 3 ? Json(nullptr) : Json("b");
    for (int j = 0; j < 3; j++) bonesOf[boneName(s, 32 + (k * 3 + j) % std::max(1, s.bones - 32))] = {{"rotation", rng.range(-30, 30)}};
    expressions.push_back({{"id", "face-" + std::to_string(k)}, {"slots", slotsOf}, {"bones", bonesOf}});
  }
  out["expressions"] = expressions;
  Json ik = Json::array();
  for (int limb = 0; limb < 4; limb++)
    ik.push_back({{"id", "leg-" + std::to_string(limb)}, {"bones", {boneName(s, 12 + limb * 4), boneName(s, 13 + limb * 4)}},
                  {"target", boneName(s, 15 + limb * 4)}, {"mix", 1}, {"bendPositive", limb % 2 == 0}});
  for (int k = 0; k < 2; k++) ik.push_back({{"id", "look-" + std::to_string(k)}, {"bones", {boneName(s, 28 + k)}}, {"target", boneName(s, 30 + k)}, {"mix", 0.8}});
  out["ik"] = ik;
  out["events"] = Json::array({{{"name", "step"}, {"int", 1}}, {{"name", "cue"}, {"string", "go"}}, {{"name", "flash"}, {"float", 0.5}}});
  Json clips = Json::array();
  for (int c = 0; c < s.clips; c++) {
    const bool big = c == 0;
    const int64_t duration = big ? 4000 : 1000 + 100 * (c % 20);
    const int tracks = big ? s.bigTracks : s.clipTracks, keys = big ? s.bigKeys : s.clipKeys;
    Json list = Json::array();
    for (int t = 0; t < tracks && t < s.bones; t++) {
      const int bone = big ? t : (c * 7 + t * 5) % s.bones;
      bool dup = false;  // one rotation track per bone and clip
      for (const auto& x : list) dup |= x["target"] == "bone:" + boneName(s, bone);
      if (dup) continue;
      Json k = Json::array();
      for (int i = 0; i < keys; i++) k.push_back(bezier(duration * i / keys, rng.range(-25, 25), i));
      list.push_back({{"target", "bone:" + boneName(s, bone)}, {"property", "rotation"}, {"keys", k}});
    }
    if (!big) {
      list.push_back({{"target", "bone:spine-0"}, {"property", "y"}, {"keys", {bezier(0, -45, 0), bezier(duration / 2, -50, 1), bezier(duration, -45, 2)}}});
      list.push_back({{"target", "slot:" + slotName(100 + c % 20)}, {"property", "attachment"},
                      {"keys", {{{"timeMs", 0}, {"value", "a"}}, {{"timeMs", duration / 2}, {"value", "b"}}}}});
      list.push_back({{"target", "ik:leg-" + std::to_string(c % 4)}, {"property", "mix"}, {"keys", {bezier(0, 1, 0), bezier(duration, 0.5, 1)}}});
      list.push_back({{"target", "rig"}, {"property", "expression"},
                      {"keys", {{{"timeMs", 0}, {"value", nullptr}}, {{"timeMs", duration / 2}, {"value", "face-" + std::to_string(c % s.expressions)}}}}});
      // Deform keys on an unweighted mesh: a wave across the grid.
      const int mesh = s.weighted + c % std::max(1, s.meshes - s.weighted);
      if (mesh < s.meshes) {
        const size_t n = size_t((s.gridCols + 1) * (s.gridRows + 1));
        Json k = Json::array();
        for (int i = 0; i < 3; i++) {
          Json offsets = Json::array();
          for (size_t v = 0; v < n; v++) { offsets.push_back(0); offsets.push_back(i == 1 ? std::round(8 * std::sin(double(v)) * 100) / 100 : 0); }
          Json key{{"timeMs", duration * i / 2}, {"offsets", offsets}};
          if (i == 0) { key["interp"] = "bezier"; key["curve"] = Json::array({0.42, 0, 0.58, 1}); }
          k.push_back(key);
        }
        list.push_back({{"target", "slot:" + slotName(mesh)}, {"property", "deform"}, {"attachment", "a"}, {"keys", k}});
      }
    }
    Json clip{{"id", "clip-" + std::to_string(c)}, {"durationMs", duration}, {"loop", c % 5 != 4}, {"tracks", list}};
    clip["events"] = Json::array({{{"timeMs", 0}, {"name", "step"}}, {{"timeMs", duration / 2}, {"name", "cue"}, {"string", "half"}}, {{"timeMs", duration * 3 / 4}, {"name", "flash"}}});
    clips.push_back(clip);
  }
  out["clips"] = clips;
  out["mix"] = {{"defaultMs", 100}};
  return out;
}

/** Controller for the stress rig: an idle clip, a layer and a few bindings. */
inline Json controller(const std::string& rigId = "stress") {
  return {{"schemaVersion", 1}, {"rig", rigId}, {"defaultClip", "clip-1"}, {"layers", {{{"track", 1}, {"clip", "clip-2"}}}},
          {"bindings", {{{"id", "joy"}, {"signal", "actionResult"}, {"match", {{"outcome", "success"}}}, {"clip", "clip-4"}, {"then", "default"}},
                        {{"id", "dress"}, {"signal", "dayStarted"}, {"skin", "skin-1"}}}}};
}

/** Every image asset id the rig references (default attachments and skins), sorted, unique. */
inline std::vector<std::string> imageIds(const Json& rig) {
  std::vector<std::string> ids;
  auto add = [&](const Json& list) { for (const auto& a : list) ids.push_back(a.value("imageAssetId", std::string())); };
  add(rig.value("attachments", Json::array()));
  for (const auto& s : rig.value("skins", Json::array())) add(s.value("attachments", Json::array()));
  std::sort(ids.begin(), ids.end());
  ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
  return ids;
}
}  // namespace cutout_stress
