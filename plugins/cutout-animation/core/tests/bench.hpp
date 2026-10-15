// Large-rig benchmarks of the core (plugin 1.9.0): `cutout-core-tests --bench` prints them as
// JSON, `--perf` also checks them against budgets (tests/plugins/cutout-perf.test.mjs runs both
// with the Wasm, player and service measurements). Medians of N runs on this machine.
#pragma once
#include <algorithm>
#include <chrono>
#include <functional>
#include "cutout/editing.hpp"
#include "cutout/host.hpp"
#include "stress-rig.hpp"

namespace cutout_bench {
using cutout::Json;
using Clock = std::chrono::steady_clock;

/** Median wall time (ms) of `runs` calls of `body` after one warm-up call. */
inline double median(int runs, const std::function<void()>& body) {
  body();
  std::vector<double> ms;
  for (int i = 0; i < runs; i++) {
    const auto t0 = Clock::now();
    body();
    ms.push_back(std::chrono::duration<double, std::milli>(Clock::now() - t0).count());
  }
  std::sort(ms.begin(), ms.end());
  return ms[ms.size() / 2];
}

/** Counts that describe the generated character (shown in the report next to the timings). */
inline Json describe(const Json& rig) {
  size_t meshes = 0, vertices = 0, weighted = 0, keys = 0, bigKeys = 0;
  for (const auto& a : rig["attachments"])
    if (a.value("type", std::string()) == "mesh") { meshes++; vertices += a["vertices"].size() / 2; if (a.contains("weights")) weighted += a["vertices"].size() / 2; }
  for (const auto& c : rig["clips"])
    for (const auto& t : c["tracks"]) { keys += t["keys"].size(); if (c["id"] == "clip-0") bigKeys += t["keys"].size(); }
  return {{"bytes", rig.dump().size()}, {"bones", rig["bones"].size()}, {"slots", rig["slots"].size()}, {"meshes", meshes}, {"meshVertices", vertices},
          {"weightedVertices", weighted}, {"clips", rig["clips"].size()}, {"keys", keys}, {"denseClipKeys", bigKeys}, {"skins", rig["skins"].size()},
          {"expressions", rig["expressions"].size()}, {"ik", rig["ik"].size()}};
}

/** Host ops of `n` mounted instances (the ABI path every player takes). */
inline void mountAll(cutout::Host& host, const Json& rig, int n) {
  host.handle({{"op", "init"}, {"documents", {{{"type", "rig"}, {"id", "stress"}, {"data", rig}}, {{"type", "controller"}, {"id", "stress-main"}, {"data", cutout_stress::controller()}}}}});
  for (int i = 0; i < n; i++)
    host.handle({{"op", "mount"}, {"instance", "i" + std::to_string(i)}, {"element", {{"type", "character"}, {"ref", "stress-main"}, {"width", 400}, {"height", 400}, {"props", Json::object()}}}});
}

/**
 * One presentation frame through the C ABI strings (Host::requestText = what the DLL / Wasm export
 * runs): update once, frame every instance, as hosts do. `since` (format 2) holds each instance's
 * last frame number, read from the reply like a host does, so deltas are measured too.
 */
inline size_t hostFrame(cutout::Host& host, int n, const Json& frameExtra, std::vector<std::string>* replies = nullptr, std::vector<uint64_t>* since = nullptr) {
  size_t bytes = 0;
  (void)host.requestText(Json({{"op", "update"}, {"dtMs", 16.7}}).dump());
  for (int i = 0; i < n; i++) {
    Json request{{"op", "frame"}, {"instance", "i" + std::to_string(i)}};
    for (auto it = frameExtra.begin(); it != frameExtra.end(); ++it) request[it.key()] = it.value();
    if (since && size_t(i) < since->size() && (*since)[size_t(i)]) request["since"] = (*since)[size_t(i)];
    const std::string reply = host.requestText(request.dump());
    bytes += reply.size();
    if (since) {
      if (since->size() <= size_t(i)) since->resize(size_t(i) + 1, 0);
      const size_t at = reply.find("\"frame\":");
      (*since)[size_t(i)] = at == std::string::npos ? 0 : std::stoull(reply.substr(at + 8, 24));
    }
    if (replies) replies->push_back(reply);
  }
  return bytes;
}

inline Json run(int scale = 1) {
  const Json rig = cutout_stress::rig();
  Json out{{"rig", describe(rig)}};
  const int few = 5 * scale, many = 60 * scale;
  out["validateMs"] = median(few, [&] { (void)cutout::validateRig(rig); });
  out["parseMs"] = median(few, [&] { (void)cutout::parseRig("stress", rig); });
  auto shared = std::make_shared<const cutout::Rig>(cutout::parseRig("stress", rig));
  for (int n : {1, 20}) {
    std::vector<std::unique_ptr<cutout::Instance>> instances;
    for (int i = 0; i < n; i++) instances.push_back(std::make_unique<cutout::Instance>(shared, cutout::parseController("c", cutout_stress::controller())));
    size_t items = 0;
    out["evaluate" + std::to_string(n) + "Ms"] = median(many, [&] {
      items = 0;
      for (auto& inst : instances) { inst->update(16.7); items += inst->drawList().size(); }
    });
    out["items" + std::to_string(n)] = items;
  }
  // The ABI path: JSON request strings in, reply strings out (what the DLL / Wasm export does).
  // "" = the documented items (format 1), "V2" = compact frames with deltas (plugin 1.9.0).
  for (const auto& [name, extra] : std::vector<std::pair<std::string, Json>>{{"", Json::object()}, {"V2", {{"format", 2}}}}) {
    for (int n : {1, 20}) {
      cutout::Host host;
      mountAll(host, rig, n);
      size_t bytes = 0;
      std::vector<uint64_t> since;
      out["host" + std::to_string(n) + name + "Ms"] = median(many, [&] { bytes = hostFrame(host, n, extra, nullptr, name.empty() ? nullptr : &since); });
      if (n == 1) {
        out["framePayload" + name + "Bytes"] = bytes;
        std::vector<std::string> replies;
        hostFrame(host, 1, extra, &replies, name.empty() ? nullptr : &since);
        // The native player parses every frame reply with the same nlohmann::json.
        out["nativeParse" + name + "Ms"] = median(many, [&] { (void)Json::parse(replies[0]); });
        if (!name.empty()) {
          // A full compact frame (no delta) and an idle one (paused instance: every item "0").
          std::vector<std::string> full;
          hostFrame(host, 1, extra, &full);
          out["framePayloadV2FullBytes"] = full[0].size();
          out["nativeParseV2FullMs"] = median(many, [&] { (void)Json::parse(full[0]); });
          host.handle({{"op", "setPaused"}, {"instance", "i0"}, {"paused", true}});
          std::vector<std::string> idle;
          hostFrame(host, 1, extra, &idle, &since);
          hostFrame(host, 1, extra, &idle, &since);
          out["framePayloadV2IdleBytes"] = idle.back().size();
        }
      }
    }
  }
  // Typical authoring operations on the whole document (what one editor gesture costs in the core).
  Json keys = Json::array();
  for (int t = 0; t < 25; t++)
    for (int k = 0; k < 20; k++) keys.push_back({{"target", "bone:" + cutout_stress::boneName({}, t)}, {"property", "rotation"}, {"timeMs", 200 * k}});
  Json bones = Json::array();
  for (int i = 0; i < 50; i++) bones.push_back(cutout_stress::boneName({}, 32 + i * 3));
  const std::vector<std::pair<std::string, Json>> ops{
      {"setKey", {{"op", "setKey"}, {"clip", "clip-1"}, {"target", "bone:head"}, {"property", "rotation"}, {"timeMs", 123}, {"value", 12.5}}},
      {"moveKeys500", {{"op", "moveKeys"}, {"clip", "clip-0"}, {"keys", keys}, {"deltaMs", 10}}},
      {"transformBones50", {{"op", "transformBones"}, {"bones", bones}, {"delta", {{"rotation", 5}}}}},
      {"retriangulate", {{"op", "retriangulate"}, {"slot", "part-0"}}}};
  Json edits = Json::object();
  for (const auto& [name, op] : ops) {
    const auto probe = cutout::applyEdit(rig, op);  // must be accepted (throws otherwise)
    edits[name + "Ms"] = median(few, [&] { (void)cutout::applyEdit(rig, op); });
    (void)probe;
  }
  out["edit"] = edits;
  return out;
}

/**
 * Budgets: ~3x the medians measured on the development machine (plugin 1.9.0, Release MinGW,
 * see tests/plugins/cutout-perf.test.mjs for the table), so a regression of the hot paths fails CI
 * while machine noise does not. Returns failures.
 */
inline std::vector<std::string> checkBudgets(const Json& r) {
  const std::vector<std::pair<std::string, double>> budgets{
      {"/validateMs", 20}, {"/parseMs", 40}, {"/evaluate1Ms", 0.25}, {"/evaluate20Ms", 4.5}, {"/host1Ms", 3.5}, {"/host20Ms", 70},
      {"/host1V2Ms", 1}, {"/host20V2Ms", 18}, {"/nativeParseMs", 9}, {"/nativeParseV2Ms", 3.5},
      {"/edit/setKeyMs", 50}, {"/edit/moveKeys500Ms", 60}, {"/edit/transformBones50Ms", 100}, {"/edit/retriangulateMs", 50}};
  std::vector<std::string> failed;
  for (const auto& [pointer, budget] : budgets) {
    const Json::json_pointer p(pointer);
    if (!r.contains(p)) { failed.push_back(pointer + " missing"); continue; }
    const double v = r.at(p).get<double>();
    if (!(v <= budget)) failed.push_back(pointer + " = " + std::to_string(v) + " ms > budget " + std::to_string(budget) + " ms");
  }
  return failed;
}
}  // namespace cutout_bench
