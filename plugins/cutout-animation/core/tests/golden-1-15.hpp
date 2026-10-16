// Back-compat golden of plugin 1.16.0 (shear, inherit, transform constraints): every rig of the
// core tests that has none of the new fields must draw, sample and describe exactly as the 1.15.0
// build did. `--golden-1-15` prints one FNV-1a hash per scenario (pose world/applied bits, draw
// item bits, host sample/frame reply text); golden-1-15-data.hpp holds the hashes the 1.15.0 build
// printed before the 1.16 code existed. Included by core-tests.cpp after the other fixtures.
#pragma once
#include <cstdio>
#include <cstring>
#include "golden-1-15-data.hpp"

namespace golden115 {
using cutout::Json;

struct Hash {
  uint64_t h = 1469598103934665603ull;
  void bytes(const void* p, size_t n) {
    const auto* c = static_cast<const unsigned char*>(p);
    for (size_t i = 0; i < n; i++) { h ^= c[i]; h *= 1099511628211ull; }
  }
  void num(double v) { uint64_t b; std::memcpy(&b, &v, sizeof b); bytes(&b, sizeof b); }
  void text(const std::string& s) { bytes(s.data(), s.size()); }
  std::string hex() const { char t[24]; std::snprintf(t, sizeof t, "%016llx", static_cast<unsigned long long>(h)); return t; }
};
void pose(Hash& h, const cutout::Pose& p) {
  for (size_t i = 0; i < p.world.size(); i++) {
    const auto& w = p.world[i];
    for (double v : {w.a, w.b, w.c, w.d, w.e, w.f}) h.num(v);
    const auto& t = p.local(i);
    for (double v : {t.x, t.y, t.rotation, t.scaleX, t.scaleY}) h.num(v);
  }
}
void items(Hash& h, const std::vector<cutout::DrawItem>& list) {
  for (const auto& d : list) {
    h.text(d.slot); h.text(d.imageAssetId);
    for (double v : {d.matrix.a, d.matrix.b, d.matrix.c, d.matrix.d, d.matrix.e, d.matrix.f, d.width, d.height, d.alpha}) h.num(v);
    for (double v : d.positions) h.num(v);
    for (double v : d.uvs) h.num(v);
    for (int i : d.indices) h.num(double(i));
  }
}
/** Rigs without any 1.16 field: the IK golden set plus every other fixture family. */
std::vector<std::pair<std::string, Json>> rigs() {
  auto out = ikgolden::rigs();
  out.push_back({"fixture", rigData()});
  out.push_back({"bezier", bezierRig()});
  out.push_back({"look", lookRig()});
  out.push_back({"weighted", weightedArm()});
  out.push_back({"outline", outlineRig()});
  out.push_back({"clip", clipseq::clipRig()});
  out.push_back({"chain", ikphys::chainRig(150, 40)});
  out.push_back({"hair", ikphys::hairRig()});
  out.push_back({"physicsParity", ikphys::parityRig()});
  out.push_back({"stress", cutout_stress::rig()});
  return out;
}
Json dump() {
  Json out = Json::object();
  cutout::Host host;
  for (const auto& [name, data] : rigs()) {
    const cutout::Rig r = cutout::parseRig(name, data);
    Hash h;
    const std::vector<double> times = {0.0, 120.0, 250.0, 333.5, 500.0, 777.25, 999.0};
    if (r.clips.empty()) { const auto p = cutout::samplePose(r, -1, 0); pose(h, p); items(h, cutout::drawItems(r, p)); }
    for (size_t c = 0; c < r.clips.size(); c++)
      for (double t : times) { const auto p = cutout::samplePose(r, int(c), t); pose(h, p); items(h, cutout::drawItems(r, p)); }
    // Host reply text (shape and printed numbers) of a sample: new keys would change it.
    const Json reply = host.handle({{"op", "sample"}, {"rig", data}, {"clip", r.clips.empty() ? Json(nullptr) : Json(r.clips[0].id)}, {"timeMs", 250}});
    h.text(reply.dump());
    // A playing instance (physics rigs simulate, irregular dts): the runtime path.
    cutout::Instance inst(std::make_shared<const cutout::Rig>(r));
    if (!r.clips.empty()) inst.play(0, r.clips[0].id);
    for (double dt : {16.0, 125.5, 233.25, 7.5, 60.0}) { inst.update(dt); pose(h, inst.pose()); items(h, inst.drawList()); }
    out[name] = h.hex();
  }
  return out;
}
void run() {
  test("1.15 golden: rigs without shear/inherit/transform constraints are bit-identical", [] {
    const Json want = Json::parse(GOLDEN_1_15), got = dump();
    size_t same = 0;
    for (auto it = want.begin(); it != want.end(); ++it) {
      const bool ok = got.contains(it.key()) && got.at(it.key()) == it.value();
      check(ok, "scenario " + it.key());
      same += ok;
    }
    check(want.size() >= 30 && same == want.size(), "scenarios identical: " + std::to_string(same) + "/" + std::to_string(want.size()));
  });
}
}  // namespace golden115
