// Core tests: validation, evaluation, playback, multi-instance, persistence, signals.
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include "cutout/host.hpp"
#include "cutout/mesh.hpp"
#include "cutout/signals.hpp"
#include "cutout/templates.hpp"
#include "bench.hpp"

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
/** rigData() + skins (outfits) + expressions + a clip keying the rig expression track (plugin 1.2.0). */
Json lookRig() {
  Json d = rigData();
  d["skins"] = Json::parse(R"([
    {"id": "winter", "name": "겨울", "attachments": [
      {"slot": "torso", "name": "torso", "imageAssetId": "img-torso-winter", "width": 90, "height": 130, "pivotX": 0.5, "pivotY": 1},
      {"slot": "eyes", "name": "closed", "imageAssetId": "img-eyes-closed-winter", "width": 40, "height": 10}]},
    {"id": "summer", "attachments": [
      {"slot": "torso", "name": "torso", "imageAssetId": "img-torso-summer", "width": 70, "height": 110, "pivotX": 0.5, "pivotY": 1, "x": 2}]}
  ])");
  d["expressions"] = Json::parse(R"([
    {"id": "happy", "name": "기쁨", "slots": {"eyes": "closed"}, "bones": {"head": {"rotation": 10}}},
    {"id": "surprised", "slots": {"eyes": "open", "face": null}, "bones": {"head": {"y": -90, "scaleY": 1.1}}}
  ])");
  d["clips"].push_back(Json::parse(R"({"id": "talk-face", "durationMs": 1000, "loop": false, "tracks": [
    {"target": "rig", "property": "expression", "keys": [{"timeMs": 0, "value": "happy"}, {"timeMs": 500, "value": null}]},
    {"target": "bone:body", "property": "x", "keys": [{"timeMs": 0, "value": 0}, {"timeMs": 1000, "value": 10}]}
  ]})"));
  return d;
}
Json lookController() {
  Json c = controllerData();
  c["defaultSkin"] = "winter";
  c["bindings"].push_back(Json::parse(R"({"id": "face-happy", "signal": "portraitChanged", "match": {"portraitId": "happy"}, "expression": "happy"})"));
  c["bindings"].push_back(Json::parse(R"({"id": "face-clear", "signal": "portraitChanged", "match": {"portraitId": "none"}, "expression": null})"));
  c["bindings"].push_back(Json::parse(R"({"id": "dress-summer", "signal": "phaseChanged", "match": {"phase": "summer"}, "skin": "summer"})"));
  return c;
}
/** Host op script for the native/Wasm look parity check (tests/plugins/cutout-look-parity.test.mjs). */
Json lookScript() {
  Json c = lookController();
  c.erase("defaultSkin");
  c["defaultExpression"] = "surprised";
  const Json element = {{"type", "character"}, {"ref", "hero-main"}, {"width", 300}, {"height", 450}, {"props", {{"skin", "winter"}, {"scale", 1}}}};
  const auto frame = Json{{"op", "frame"}, {"instance", "a"}};
  return Json::array({
    {{"op", "init"}, {"documents", Json::array({{{"type", "rig"}, {"id", "hero"}, {"data", lookRig()}}, {{"type", "controller"}, {"id", "hero-main"}, {"data", c}}})}},
    {{"op", "mount"}, {"instance", "a"}, {"element", element}}, frame,
    {{"op", "update"}, {"dtMs", 1850}}, frame,  // blink layer closes the eyes over the expression
    {{"op", "signal"}, {"instance", "a"}, {"name", "portraitChanged"}, {"params", {{"portraitId", "happy"}}}}, {{"op", "update"}, {"dtMs", 10}}, frame,
    {{"op", "setSkin"}, {"instance", "a"}, {"skin", "summer"}}, frame,
    {{"op", "setExpression"}, {"instance", "a"}, {"expression", nullptr}}, frame,
    {{"op", "play"}, {"instance", "a"}, {"clip", "talk-face"}, {"mixMs", 0}, {"then", "hold"}}, {{"op", "update"}, {"dtMs", 250}}, frame,
    {{"op", "update"}, {"dtMs", 300}}, frame,
    {{"op", "setSkin"}, {"instance", "a"}, {"skin", ""}}, frame,
  });
}
/** rigData() + event definitions with payloads (plugin 1.8.0): `step` has an int, string and audio
 *  default, `hit` a float; the walk clip keys both (one key overrides the int) and loops. */
Json eventRig() {
  Json d = rigData();
  d["events"] = Json::parse(R"([
    {"name": "step", "int": 1, "string": "left", "audio": "sfx-step", "volume": 0.5, "balance": -0.25},
    {"name": "hit", "float": 2.5},
    {"name": "breath"}, {"name": "peak"}, {"name": "done"}
  ])");
  d["clips"].push_back(Json::parse(R"({"id": "walk", "durationMs": 400, "loop": true, "tracks": [],
    "events": [{"timeMs": 0, "name": "step"}, {"timeMs": 100, "name": "hit", "string": "fist"}, {"timeMs": 200, "name": "step", "int": 2, "string": "right"}]})"));
  return d;
}
/** Host op script of the native/Wasm event parity check (tests/plugins/cutout-event-parity.test.mjs):
 *  loops of the walk clip at uneven frame steps, a one-shot with events at its end, payloads. */
Json eventScript() {
  const Json controller = Json::parse(R"({"schemaVersion": 1, "rig": "hero", "defaultClip": "walk", "layers": [], "bindings": [
    {"id": "cheer", "signal": "actionResult", "match": {"outcome": "success"}, "clip": "joy", "then": "default"}]})");
  const Json element = {{"type", "character"}, {"ref", "walker"}, {"width", 200}, {"height", 300}, {"props", Json::object()}};
  Json script = Json::array({
    {{"op", "init"}, {"documents", Json::array({{{"type", "rig"}, {"id", "hero"}, {"data", eventRig()}}, {{"type", "controller"}, {"id", "walker"}, {"data", controller}}})}},
    {{"op", "mount"}, {"instance", "a"}, {"element", element}},
  });
  for (double dt : {16.0, 84.0, 0.5, 99.5, 250.0, 950.0}) script.push_back({{"op", "update"}, {"dtMs", dt}});
  script.push_back({{"op", "signal"}, {"instance", "a"}, {"name", "actionResult"}, {"params", {{"outcome", "success"}}}});
  for (double dt : {150.0, 50.0, 200.0, 33.0}) script.push_back({{"op", "update"}, {"dtMs", dt}});
  return script;
}
/**
 * Two-bone arm for IK (plugin 1.3.0): root -> upper (shoulder) -> lower (elbow, 50 px) -> hand
 * (wrist, 50 px); target `t` hangs from the root. `ik` holds one constraint `arm` on [upper, lower].
 */
Json armRig(double tx, double ty) {
  Json d = Json::parse(R"({"schemaVersion": 1, "canvas": {"width": 400, "height": 400, "originX": 200, "originY": 200},
    "bones": [
      {"id": "root", "parent": null, "x": 0, "y": 0},
      {"id": "upper", "parent": "root", "x": 0, "y": 0},
      {"id": "lower", "parent": "upper", "x": 50, "y": 0},
      {"id": "hand", "parent": "lower", "x": 50, "y": 0},
      {"id": "t", "parent": "root", "x": 0, "y": 0}],
    "slots": [{"id": "forearm", "bone": "lower", "attachment": "forearm"}, {"id": "palm", "bone": "hand", "attachment": "palm"}],
    "attachments": [
      {"slot": "forearm", "name": "forearm", "imageAssetId": "img-forearm", "width": 50, "height": 10, "pivotX": 0, "pivotY": 0.5},
      {"slot": "palm", "name": "palm", "imageAssetId": "img-palm", "width": 12, "height": 12}],
    "clips": [],
    "ik": [{"id": "arm", "bones": ["upper", "lower"], "target": "t", "mix": 1, "bendPositive": true}]})");
  d["bones"][4]["x"] = tx; d["bones"][4]["y"] = ty;
  return d;
}
/** armRig under a rotated, scaled root plus a head aiming at the hand target and a clip that moves
 *  the target and keys mix/bend: the IK parity fixture (tests/plugins/cutout-ik-parity.test.mjs). */
Json ikParityRig() {
  Json d = armRig(70, 40);
  d["bones"][0] = {{"id", "root"}, {"parent", nullptr}, {"x", 200}, {"y", 200}, {"rotation", 15}, {"scaleX", 1.3}, {"scaleY", 1.3}};
  d["bones"][2]["rotation"] = 25;
  d["bones"].push_back({{"id", "head"}, {"parent", "root"}, {"x", -20}, {"y", -60}, {"scaleX", -1}});
  d["ik"].push_back({{"id", "look"}, {"bones", {"head"}}, {"target", "t"}, {"mix", 0.8}, {"order", 1}, {"aimAxis", 35}});
  d["slots"].push_back({{"id", "face"}, {"bone", "head"}, {"attachment", "face"}});
  d["attachments"].push_back({{"slot", "face"}, {"name", "face"}, {"imageAssetId", "img-face"}, {"width", 30}, {"height", 20}, {"x", 10}});
  d["clips"].push_back(Json::parse(R"({"id": "reach", "durationMs": 1000, "loop": true, "tracks": [
    {"target": "bone:t", "property": "x", "keys": [{"timeMs": 0, "value": 70}, {"timeMs": 500, "value": 160, "interp": "bezier", "curve": [0.42, 0, 0.58, 1]}, {"timeMs": 1000, "value": 70}]},
    {"target": "bone:t", "property": "y", "keys": [{"timeMs": 0, "value": 40}, {"timeMs": 500, "value": -30}, {"timeMs": 1000, "value": 40}]},
    {"target": "ik:arm", "property": "mix", "keys": [{"timeMs": 0, "value": 1}, {"timeMs": 700, "value": 0.25}, {"timeMs": 1000, "value": 1}]},
    {"target": "ik:arm", "property": "bendPositive", "keys": [{"timeMs": 0, "value": true}, {"timeMs": 600, "value": false}]}
  ]})"));
  return d;
}
Json ikParityScript() {
  const Json controller = {{"schemaVersion", 1}, {"rig", "arm"}, {"defaultClip", "reach"}};
  const Json element = {{"type", "character"}, {"ref", "arm-main"}, {"width", 400}, {"height", 400}, {"props", {{"scale", 1}}}};
  const Json frame = {{"op", "frame"}, {"instance", "a"}};
  Json script = Json::array({
    {{"op", "init"}, {"documents", Json::array({{{"type", "rig"}, {"id", "arm"}, {"data", ikParityRig()}}, {{"type", "controller"}, {"id", "arm-main"}, {"data", controller}}})}},
    {{"op", "mount"}, {"instance", "a"}, {"element", element}}, frame});
  for (double dt : {16.0, 125.5, 233.25, 180.0, 60.0, 90.0, 210.0, 333.0}) { script.push_back({{"op", "update"}, {"dtMs", dt}}); script.push_back(frame); }
  for (double ms : {0.0, 250.0, 650.0, 1000.0}) script.push_back({{"op", "sample"}, {"rig", ikParityRig()}, {"clip", "reach"}, {"timeMs", ms}});
  return script;
}
/**
 * Cutout limb as rigs usually make it (plugin 1.4.1 aim axis): bone `arm` keeps rotation 0 while
 * its 20x100 image hangs straight down from the shoulder (pivot top centre). Target `t` under the
 * root; `ik` is left to the tests.
 */
Json hangingArm() {
  return Json::parse(R"({"schemaVersion": 1, "canvas": {"width": 400, "height": 400, "originX": 0, "originY": 0},
    "bones": [
      {"id": "root", "parent": null, "x": 200, "y": 100},
      {"id": "arm", "parent": "root", "x": 30, "y": 0},
      {"id": "t", "parent": "root", "x": 120, "y": 60}],
    "slots": [{"id": "sleeve", "bone": "arm", "attachment": "sleeve"}],
    "attachments": [{"slot": "sleeve", "name": "sleeve", "imageAssetId": "img-sleeve", "width": 20, "height": 100, "pivotX": 0.5, "pivotY": 0}],
    "clips": []})");
}
/**
 * Arm for meshes (plugin 1.4.0): root (100,150) -> upper -> lower (+100 px) -> hand (+100 px),
 * target `t` under the root. Slot `arm` (on upper) shows a 200x40 image from the shoulder along
 * +x (pivot 0, 0.5); slot `flag` (on hand) a 40x30 image standing on the wrist.
 */
Json meshArm() {
  return Json::parse(R"({"schemaVersion": 1, "canvas": {"width": 400, "height": 300, "originX": 200, "originY": 250},
    "bones": [
      {"id": "root", "parent": null, "x": 100, "y": 150},
      {"id": "upper", "parent": "root", "x": 0, "y": 0},
      {"id": "lower", "parent": "upper", "x": 100, "y": 0},
      {"id": "hand", "parent": "lower", "x": 100, "y": 0},
      {"id": "t", "parent": "root", "x": 200, "y": 0}],
    "slots": [{"id": "arm", "bone": "upper", "attachment": "arm"}, {"id": "flag", "bone": "hand", "attachment": "flag"}],
    "attachments": [
      {"slot": "arm", "name": "arm", "imageAssetId": "img-arm", "width": 200, "height": 40, "pivotX": 0, "pivotY": 0.5},
      {"slot": "flag", "name": "flag", "imageAssetId": "img-flag", "width": 40, "height": 30, "pivotX": 0, "pivotY": 1}],
    "clips": []})");
}
/** meshArm with `arm` a 1x4 grid mesh weighted by image x: upper | 50/50 at the elbow | lower. */
Json weightedArm() {
  Json d = applyEdit(meshArm(), {{"op", "convertToMesh"}, {"slot", "arm"}, {"rows", 1}, {"cols", 4}}).data;
  Json weights = Json::array(), vertices = Json::array();
  const Json& v = d["attachments"][0]["vertices"];
  for (size_t i = 0; i < v.size() / 2; i++) {
    const double x = v[2 * i].get<double>();
    vertices.push_back(i);
    weights.push_back(x < 100 ? Json::array({{{"bone", "upper"}, {"weight", 1}}}) : x > 100 ? Json::array({{{"bone", "lower"}, {"weight", 1}}})
                                                                                          : Json::array({{{"bone", "upper"}, {"weight", 0.5}}, {{"bone", "lower"}, {"weight", 0.5}}}));
  }
  return applyEdit(d, {{"op", "setWeights"}, {"slot", "arm"}, {"vertices", vertices}, {"weights", weights}}).data;
}
/** The mesh parity fixture (tests/plugins/cutout-mesh-parity.test.mjs): the weighted arm under a
 *  rotated root with IK on [upper, lower] (tip hand), the flag a deformed grid mesh in a skin too,
 *  and a looping clip moving the target and keying deform with every interp. */
Json meshParityRig() {
  Json d = weightedArm();
  d = applyEdit(d, {{"op", "convertToMesh"}, {"slot", "flag"}, {"rows", 2}, {"cols", 2}}).data;
  d["bones"][0]["rotation"] = 12;
  d["skins"] = Json::array({{{"id", "red"}, {"attachments", Json::array({d["attachments"][1]})}}});
  d["skins"][0]["attachments"][0]["imageAssetId"] = "img-flag-red";
  d["ik"] = Json::array({{{"id", "arm"}, {"bones", {"upper", "lower"}}, {"target", "t"}, {"tip", "hand"}, {"bendPositive", false}}});
  std::vector<double> wave(18, 0.0), tilt(18, 0.0);
  for (size_t i = 0; i < 9; i++) { wave[2 * i] = double(i % 3) * 4.0; wave[2 * i + 1] = -6.0 * double(i / 3); tilt[2 * i + 1] = 3.0 * double(i % 3); }
  const Json zero = std::vector<double>(18, 0.0);
  d["clips"] = Json::array({{{"id", "wave"}, {"durationMs", 1000}, {"loop", true}, {"tracks", Json::array({
    {{"target", "bone:t"}, {"property", "x"}, {"keys", Json::array({{{"timeMs", 0}, {"value", 200}}, {{"timeMs", 500}, {"value", 120}, {"interp", "bezier"}, {"curve", {0.42, 0, 0.58, 1}}}, {{"timeMs", 1000}, {"value", 200}}})}},
    {{"target", "bone:t"}, {"property", "y"}, {"keys", Json::array({{{"timeMs", 0}, {"value", 0}}, {{"timeMs", 500}, {"value", -90}}, {{"timeMs", 1000}, {"value", 0}}})}},
    {{"target", "slot:flag"}, {"property", "deform"}, {"attachment", "flag"}, {"keys", Json::array({
      {{"timeMs", 0}, {"offsets", zero}, {"interp", "bezier"}, {"curve", {0.42, 0, 0.58, 1}}},
      {{"timeMs", 400}, {"offsets", wave}, {"interp", "step"}}, {{"timeMs", 700}, {"offsets", tilt}}, {{"timeMs", 1000}, {"offsets", zero}}})}}})}}});
  return d;
}
Json meshParityScript() {
  const Json controller = {{"schemaVersion", 1}, {"rig", "arm"}, {"defaultClip", "wave"}};
  const Json element = {{"type", "character"}, {"ref", "arm-main"}, {"width", 300}, {"height", 240}, {"props", {{"scale", 1}, {"flipX", true}}}};
  const Json frame = {{"op", "frame"}, {"instance", "a"}};
  Json script = Json::array({
    {{"op", "init"}, {"documents", Json::array({{{"type", "rig"}, {"id", "arm"}, {"data", meshParityRig()}}, {{"type", "controller"}, {"id", "arm-main"}, {"data", controller}}})}},
    {{"op", "mount"}, {"instance", "a"}, {"element", element}}, frame});
  for (double dt : {16.0, 125.5, 233.25, 180.0, 60.0, 90.0, 210.0}) { script.push_back({{"op", "update"}, {"dtMs", dt}}); script.push_back(frame); }
  script.push_back({{"op", "setSkin"}, {"instance", "a"}, {"skin", "red"}});
  script.push_back(frame);
  for (double ms : {0.0, 250.0, 550.0, 850.0}) script.push_back({{"op", "sample"}, {"rig", meshParityRig()}, {"clip", "wave"}, {"timeMs", ms}});
  return script;
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
bool hasErrors(const std::vector<Diagnostic>& ds) {
  for (const auto& d : ds) if (d.severity == "error") return true;
  return false;
}
/** One 100x100 image part on a root + two child bones (freeform mesh tests, plugin 1.6.0). */
Json outlineRig() {
  return Json::parse(R"({"schemaVersion": 1, "canvas": {"width": 300, "height": 300, "originX": 150, "originY": 250},
    "bones": [{"id": "root", "parent": null, "x": 100, "y": 100}, {"id": "left", "parent": "root", "x": 0, "y": 50}, {"id": "right", "parent": "root", "x": 100, "y": 50}],
    "slots": [{"id": "cape", "bone": "root", "attachment": "cape"}],
    "attachments": [{"slot": "cape", "name": "cape", "imageAssetId": "img-cape", "width": 100, "height": 100, "pivotX": 0, "pivotY": 0}],
    "clips": []})");
}
std::string base64(const std::vector<uint8_t>& bytes) {
  static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  for (size_t i = 0; i < bytes.size(); i += 3) {
    const uint32_t v = uint32_t(bytes[i]) << 16 | (i + 1 < bytes.size() ? uint32_t(bytes[i + 1]) << 8 : 0) | (i + 2 < bytes.size() ? bytes[i + 2] : 0);
    out += t[(v >> 18) & 63];
    out += t[(v >> 12) & 63];
    out += i + 1 < bytes.size() ? t[(v >> 6) & 63] : '=';
    out += i + 2 < bytes.size() ? t[v & 63] : '=';
  }
  return out;
}
/** Alpha mask w x h: an opaque disc (centre cx, cy, radius r) plus a small square blob. */
Json discMask(int w, int h, double cx, double cy, double r) {
  std::vector<uint8_t> a(size_t(w) * size_t(h), 0);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      const double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
      if (dx * dx + dy * dy <= r * r) a[size_t(y) * size_t(w) + size_t(x)] = 255;
      if (x >= w - 4 && y < 3) a[size_t(y) * size_t(w) + size_t(x)] = 200;  // a far-away speck: not the largest region
    }
  return {{"width", w}, {"height", h}, {"alpha", base64(a)}};
}
const Json* meshEntryOf(const Json& d, const char* slot) {
  for (const auto& a : d["attachments"]) if (a["slot"] == slot) return &a;
  return nullptr;
}
std::vector<double> numbers(const Json& list) { std::vector<double> v; for (const auto& x : list) v.push_back(x.get<double>()); return v; }
/**
 * Why the mesh's triangles are not a valid tiling of its outline ("" = valid): every triangle
 * positive and non-degenerate, inside the outline, no edge used by more than two triangles, the
 * once-used edges exactly the outline cycle, every vertex used, total area = outline area.
 */
std::string tilingProblem(const Json& a, double expectArea = -1) {
  const std::vector<double> xy = numbers(a["vertices"]);
  std::vector<int> tris;
  for (const auto& v : a["triangles"]) tris.push_back(v.get<int>());
  const size_t n = xy.size() / 2, hull = a.value("hull", size_t(0));
  const std::vector<double> outline(xy.begin(), xy.begin() + std::ptrdiff_t(2 * hull));
  double sum = 0;
  std::map<std::pair<int, int>, int> uses;
  std::vector<bool> used(n, false);
  for (size_t t = 0; t + 2 < tris.size(); t += 3) {
    const int i = tris[t], j = tris[t + 1], k = tris[t + 2];
    const double area = ((xy[2 * j] - xy[2 * i]) * (xy[2 * k + 1] - xy[2 * i + 1]) - (xy[2 * j + 1] - xy[2 * i + 1]) * (xy[2 * k] - xy[2 * i])) / 2;
    if (!(area > 1e-9)) return "triangle " + std::to_string(t / 3) + " not positive (" + std::to_string(area) + ")";
    sum += area;
    for (int v : {i, j, k}) used[size_t(v)] = true;
    for (const auto& [x, y] : {std::pair<int, int>{i, j}, {j, k}, {k, i}}) uses[{std::min(x, y), std::max(x, y)}]++;
    const double cx = (xy[2 * i] + xy[2 * j] + xy[2 * k]) / 3, cy = (xy[2 * i + 1] + xy[2 * j + 1] + xy[2 * k + 1]) / 3;
    if (hull >= 3 && !insidePolygon(outline, cx, cy)) return "triangle " + std::to_string(t / 3) + " outside the outline";
  }
  std::set<std::pair<int, int>> once;
  for (const auto& [e, c] : uses) {
    if (c > 2) return "edge used " + std::to_string(c) + " times";
    if (c == 1) once.insert(e);
  }
  for (size_t v = 0; v < n; v++) if (!used[v]) return "vertex " + std::to_string(v) + " unused";
  if (hull >= 3) {
    std::set<std::pair<int, int>> cycle;
    for (size_t i = 0; i < hull; i++) cycle.insert({int(std::min(i, (i + 1) % hull)), int(std::max(i, (i + 1) % hull))});
    if (cycle != once) return "outline edges differ from the hull cycle";
    if (expectArea < 0) expectArea = std::fabs(polygonArea(outline, hull));
  }
  if (expectArea >= 0 && std::fabs(sum - expectArea) > 1e-6 * std::max(1.0, expectArea)) return "area " + std::to_string(sum) + " != " + std::to_string(expectArea);
  return "";
}
/** Unpinned interior edges whose opposite vertex lies inside the neighbour's circumcircle. */
int delaunayViolations(const Json& a) {
  const std::vector<double> xy = numbers(a["vertices"]);
  std::vector<int> tris;
  for (const auto& v : a["triangles"]) tris.push_back(v.get<int>());
  std::set<std::pair<int, int>> pinned;
  for (size_t i = 0; i + 1 < a.value("edges", Json::array()).size(); i += 2) pinned.insert({a["edges"][i].get<int>(), a["edges"][i + 1].get<int>()});
  std::map<std::pair<int, int>, int> opposite;  // directed edge -> third vertex
  for (size_t t = 0; t + 2 < tris.size(); t += 3)
    for (int e = 0; e < 3; e++) opposite[{tris[t + size_t(e)], tris[t + size_t((e + 1) % 3)]}] = tris[t + size_t((e + 2) % 3)];
  int bad = 0;
  for (const auto& [e, c] : opposite) {
    auto it = opposite.find({e.second, e.first});
    if (it == opposite.end() || pinned.count({std::min(e.first, e.second), std::max(e.first, e.second)})) continue;
    const int A = e.first, B = e.second, C = c, D = it->second;
    const double adx = xy[2 * A] - xy[2 * D], ady = xy[2 * A + 1] - xy[2 * D + 1], bdx = xy[2 * B] - xy[2 * D], bdy = xy[2 * B + 1] - xy[2 * D + 1], cdx = xy[2 * C] - xy[2 * D], cdy = xy[2 * C + 1] - xy[2 * D + 1];
    const double det = (adx * adx + ady * ady) * (bdx * cdy - cdx * bdy) + (bdx * bdx + bdy * bdy) * (cdx * ady - adx * cdy) + (cdx * cdx + cdy * cdy) * (adx * bdy - bdx * ady);
    const double s = std::max({std::fabs(adx), std::fabs(ady), std::fabs(bdx), std::fabs(bdy), std::fabs(cdx), std::fabs(cdy)});
    bad += det > 1e-6 * s * s * s * s;
  }
  return bad;
}
bool hasEdge(const Json& a, int x, int y) {
  for (size_t t = 0; t + 2 < a["triangles"].size(); t += 3)
    for (int e = 0; e < 3; e++) {
      const int p = a["triangles"][t + size_t(e)].get<int>(), q = a["triangles"][t + size_t((e + 1) % 3)].get<int>();
      if ((p == x && q == y) || (p == y && q == x)) return true;
    }
  return false;
}
/** Outline parity fixture (plugin 1.6.0): a concave outline with an interior grid, an alpha-traced
 *  skin mesh, a vertex added / removed, a pinned edge, weights by bone and a deform key; the script
 *  replays the edit ops (identical rig JSON expected) and then plays the result. */
Json outlineParityScript() {
  Json d = outlineRig();
  d["skins"] = Json::array({{{"id", "alt"}, {"attachments", Json::array({d["attachments"][0]})}}});
  const Json concave = {{"op", "createMeshFromOutline"}, {"slot", "cape"}, {"hull", {0, 0, 100, 0, 100, 45, 55, 45, 55, 100, 0, 100}}, {"spacing", 12.5}};
  const Json traced = {{"op", "autoOutline"}, {"slot", "cape"}, {"skin", "alt"}, {"mask", discMask(50, 50, 25, 25, 18)}, {"detail", 0.7}};
  Json script = Json::array();
  Host host;
  auto edit = [&](const Json& op) {
    const Json step = {{"op", "edit"}, {"rig", d}, {"operation", op}};
    script.push_back(step);
    d = host.handle(step)["rig"];
  };
  edit(concave);
  edit(traced);
  edit({{"op", "addMeshVertex"}, {"slot", "cape"}, {"position", {20.25, 70.5}}});
  edit({{"op", "addMeshVertex"}, {"slot", "cape"}, {"position", {77.5, 0}}});
  edit({{"op", "removeMeshVertex"}, {"slot", "cape"}, {"vertices", {9}}});
  edit({{"op", "addMeshEdge"}, {"slot", "cape"}, {"edge", {0, 3}}});
  Json weights = Json::array(), vertices = Json::array();
  const Json& v = d["attachments"][0]["vertices"];
  for (size_t i = 0; i < v.size() / 2; i++) {
    const double x = v[2 * i].get<double>() / 100;
    vertices.push_back(i);
    weights.push_back(Json::array({{{"bone", "left"}, {"weight", 1 - x}}, {{"bone", "right"}, {"weight", x}}}));
  }
  edit({{"op", "setWeights"}, {"slot", "cape"}, {"vertices", vertices}, {"weights", weights}});
  edit({{"op", "retriangulate"}, {"slot", "cape"}, {"skin", "alt"}});
  const size_t n = d["attachments"][0]["vertices"].size() / 2;
  std::vector<double> wave(2 * n);
  for (size_t i = 0; i < n; i++) wave[2 * i + 1] = 4 * std::sin(double(i));
  d["clips"] = Json::array({{{"id", "sway"}, {"durationMs", 800}, {"loop", true}, {"tracks", Json::array({
    {{"target", "bone:right"}, {"property", "rotation"}, {"keys", Json::array({{{"timeMs", 0}, {"value", 0}}, {{"timeMs", 400}, {"value", 30}}, {{"timeMs", 800}, {"value", 0}}})}},
    {{"target", "slot:cape"}, {"property", "deform"}, {"attachment", "cape"}, {"keys", Json::array({{{"timeMs", 0}, {"offsets", std::vector<double>(2 * n, 0.0)}}, {{"timeMs", 400}, {"offsets", wave}}})}}})}}});
  const Json controller = {{"schemaVersion", 1}, {"rig", "cape"}, {"defaultClip", "sway"}};
  script.push_back({{"op", "init"}, {"documents", Json::array({{{"type", "rig"}, {"id", "cape"}, {"data", d}}, {{"type", "controller"}, {"id", "cape-main"}, {"data", controller}}})}});
  script.push_back({{"op", "mount"}, {"instance", "a"}, {"element", {{"type", "character"}, {"ref", "cape-main"}, {"width", 300}, {"height", 300}, {"props", Json::object()}}}});
  for (double dt : {0.0, 133.0, 250.5, 199.0}) {
    if (dt > 0) script.push_back({{"op", "update"}, {"dtMs", dt}});
    script.push_back({{"op", "frame"}, {"instance", "a"}});
  }
  script.push_back({{"op", "setSkin"}, {"instance", "a"}, {"skin", "alt"}});
  script.push_back({{"op", "frame"}, {"instance", "a"}});
  return script;
}
}  // namespace

#include "clip-sequence-tests.hpp"
#include "ik-golden.hpp"
#include "ik-physics-tests.hpp"
#include "golden-1-15.hpp"
#include "constraint-tests.hpp"
#include "box-tests.hpp"
#include "weight-tests.hpp"

int main(int argc, char** argv) {
  if (argc > 1 && std::string(argv[1]) == "--physics-parity") {
    // Native reference for IK chains + physics frames at two pacings (plugin 1.13.0).
    Host host;
    Json replies = Json::array();
    const Json script = ikphys::parityScript();
    for (const auto& op : script) replies.push_back(Json::parse(host.requestText(op.dump())));
    std::cout << Json({{"script", script}, {"replies", replies}}).dump() << "\n";
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--transform-parity") {
    // Native reference for shear, inherit and transform constraints at two pacings (plugin 1.16.0).
    Host host;
    Json replies = Json::array();
    const Json script = xform::parityScript();
    for (const auto& op : script) replies.push_back(Json::parse(host.requestText(op.dump())));
    std::cout << Json({{"script", script}, {"replies", replies}}).dump() << "\n";
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--box-parity") {
    // Native reference for bounding boxes / points / hit tests and partClicked (plugin 1.17.0).
    Host host;
    Json replies = Json::array();
    const Json script = boxes::parityScript();
    for (const auto& op : script) replies.push_back(Json::parse(host.requestText(op.dump())));
    std::cout << Json({{"script", script}, {"replies", replies}}).dump() << "\n";
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--weight-parity") {
    // Native reference for painted weights and linked meshes at two pacings (plugin 1.18.0).
    Host host;
    Json replies = Json::array();
    const Json script = weights::parityScript();
    for (const auto& op : script) replies.push_back(Json::parse(host.requestText(op.dump())));
    std::cout << Json({{"script", script}, {"replies", replies}}).dump() << "\n";
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--golden-1-15") {
    // Hashes of every pre-1.16 scenario (golden-1-15-data.hpp was written by the 1.15.0 build).
    std::cout << golden115::dump().dump() << "\n";
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--ik-golden") {
    // Bit patterns of the one/two-bone IK poses (ik-golden.json was written by the 1.12.0 build).
    std::cout << ikgolden::dump().dump() << "\n";
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--clip-parity") {
    // Native reference for clipping + sequence draw lists, compact frames and samples (plugin 1.12.0).
    Host host;
    Json replies = Json::array();
    const Json script = clipseq::clipParityScript();
    for (const auto& op : script) replies.push_back(Json::parse(host.requestText(op.dump())));
    std::cout << Json({{"script", script}, {"replies", replies}}).dump() << "\n";
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--stress-rig") {
    // The generated large character for the node/player/service benchmarks (plugin 1.9.0).
    const Json rig = cutout_stress::rig();
    std::cout << Json({{"rig", rig}, {"controller", cutout_stress::controller()}, {"images", cutout_stress::imageIds(rig)}}).dump() << "\n";
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--frame-parity") {
    // Native reference for compact frames (plugin 1.9.0) through the ABI string path: deltas, the
    // mesh table, skins/expressions, mirrored placement (tests/plugins/cutout-frame-parity.test.mjs).
    const Json rig = cutout_stress::rig();
    Json script = Json::array({{{"op", "init"}, {"frameFormats", {2}}, {"documents", {{{"type", "rig"}, {"id", "stress"}, {"data", rig}}, {{"type", "controller"}, {"id", "c"}, {"data", cutout_stress::controller()}}}}}});
    for (int i = 0; i < 3; i++)
      script.push_back({{"op", "mount"}, {"instance", "i" + std::to_string(i)}, {"element", {{"type", "character"}, {"ref", "c"}, {"width", 200 + 100 * i}, {"height", 300}, {"props", {{"flipX", i == 2}, {"scale", 1 + 0.25 * i}}}}}});
    const double steps[] = {0, 0, 16.7, 33.4, 250, 1200};
    for (double dt : steps) {
      script.push_back({{"op", "update"}, {"dtMs", dt}});
      for (int i = 0; i < 3; i++) script.push_back({{"op", "frame"}, {"instance", "i" + std::to_string(i)}, {"format", 2}, {"since", "?"}});
    }
    script.push_back({{"op", "setSkin"}, {"instance", "i1"}, {"skin", "skin-2"}});
    script.push_back({{"op", "setExpression"}, {"instance", "i1"}, {"expression", "face-3"}});
    script.push_back({{"op", "frame"}, {"instance", "i1"}, {"format", 2}, {"since", "?"}});
    script.push_back({{"op", "meshes"}, {"ids", {0, 7, 39}}});
    // "?" = the frame number of that instance's previous reply (what a host sends back).
    Host host;
    std::map<std::string, Json> last;
    Json replies = Json::array();
    for (auto& op : script) {
      if (op.contains("since")) op["since"] = last.count(op["instance"]) ? last[op["instance"]] : Json(0);
      const Json reply = Json::parse(host.requestText(op.dump()));
      if (op["op"] == "frame") last[op["instance"]] = reply["frame"];
      replies.push_back(reply);
    }
    std::cout << Json({{"script", script}, {"replies", replies}}).dump() << "\n";
    return 0;
  }
  if (argc > 1 && (std::string(argv[1]) == "--bench" || std::string(argv[1]) == "--perf")) {
    // Large-rig medians; --perf also checks the regression budgets (exit 1 when one is exceeded).
    const Json result = cutout_bench::run();
    std::cout << result.dump() << "\n";
    if (std::string(argv[1]) == "--bench") return 0;
    const auto failed = cutout_bench::checkBudgets(result);
    for (const auto& f : failed) std::cerr << "PERF BUDGET " << f << "\n";
    return failed.empty() ? 0 : 1;
  }
  if (argc > 1 && std::string(argv[1]) == "--bezier-parity") {
    // Native reference samples for tests/plugins/cutout-bezier-parity.test.mjs (Wasm `sample`).
    Host host;
    Json samples = Json::array();
    for (double t : parityTimes())
      samples.push_back({{"timeMs", t}, {"pose", host.handle({{"op", "sample"}, {"rig", bezierRig()}, {"clip", "ease"}, {"timeMs", t}})["pose"]}});
    std::cout << Json({{"rig", bezierRig()}, {"clip", "ease"}, {"samples", samples}}).dump() << "\n";
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--ik-parity") {
    // Native reference for IK draw lists and sampled poses (replayed through the Wasm runtime).
    Host host;
    Json replies = Json::array();
    for (const auto& op : ikParityScript()) replies.push_back(host.handle(op));
    std::cout << Json({{"script", ikParityScript()}, {"replies", replies}}).dump() << "\n";
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--mesh-parity") {
    // Native reference for mesh draw items (skinning after IK, deform keys, skins) and samples.
    Host host;
    Json replies = Json::array();
    for (const auto& op : meshParityScript()) replies.push_back(host.handle(op));
    std::cout << Json({{"script", meshParityScript()}, {"replies", replies}}).dump() << "\n";
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--outline-parity") {
    // Native reference for freeform mesh edits (outline, alpha trace, vertex/edge ops) and their draw items.
    Host host;
    Json replies = Json::array();
    const Json script = outlineParityScript();
    for (const auto& op : script) replies.push_back(host.handle(op));
    std::cout << Json({{"script", script}, {"replies", replies}}).dump() << "\n";
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--event-parity") {
    // Native reference for event notifications with payloads (replayed through the Wasm runtime).
    Host host;
    Json replies = Json::array();
    for (const auto& op : eventScript()) replies.push_back(host.handle(op));
    std::cout << Json({{"script", eventScript()}, {"replies", replies}}).dump() << "\n";
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--look-parity") {
    // Native reference for skin/expression draw lists (replayed through the Wasm runtime).
    Host host;
    Json replies = Json::array();
    for (const auto& op : lookScript()) replies.push_back(host.handle(op));
    std::cout << Json({{"script", lookScript()}, {"replies", replies}}).dump() << "\n";
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

  // ---- Skins and expressions (plugin 1.2.0) ----
  auto imageOf = [](const std::vector<DrawItem>& items, const std::string& slot) {
    for (const auto& i : items) if (i.slot == slot) return i.imageAssetId;
    return std::string("-");
  };
  test("skins: lookup order and draw lists", [&] {
    check(validateRig(lookRig()).empty(), "look fixture validates");
    check(validateController(lookController(), std::make_shared<const Rig>(parseRig("hero", lookRig())).get()).empty(), "look controller validates");
    const auto r = std::make_shared<const Rig>(parseRig("hero", lookRig()));
    check(r->skins.size() == 2 && r->expressions.size() == 2 && r->skinIndex("summer") == 1 && r->expressionIndex("surprised") == 1, "parsed");
    check(r->attachment("torso", "torso", 0)->imageAssetId == "img-torso-winter" && r->attachment("torso", "torso")->imageAssetId == "img-torso", "skin first, then default");
    check(r->attachment("face", "face", 0)->imageAssetId == "img-face" && r->attachment("eyes", "closed", 1)->imageAssetId == "img-eyes-closed", "missing in skin falls back to default");
    Instance inst(r);
    check(imageOf(inst.drawList(), "torso") == "img-torso" && inst.skin().empty(), "no skin = default attachments");
    check(inst.setSkin("winter") && inst.skin() == "winter", "setSkin");
    auto items = inst.drawList();
    check(imageOf(items, "torso") == "img-torso-winter" && items[0].width == 90 && imageOf(items, "face") == "img-face", "outfit overrides only what it provides");
    check(!inst.setSkin("nope") && inst.skin() == "winter", "unknown skin refused, nothing changes");
    inst.play(0, "blink", true, 1, Then::Default, 0);
    inst.update(1850);
    check(imageOf(inst.drawList(), "eyes") == "img-eyes-closed-winter", "clips key names; the skin resolves them (no new animation)");
    check(inst.setSkin("") && imageOf(inst.drawList(), "eyes") == "img-eyes-closed", "back to default attachments");
    check(samplePose(*r, -1, 0, {1}).skin == 1 && samplePose(*r, -1, 0).skin == -1, "samplePose carries the skin");
  });

  test("skins and expressions: validation codes", [&] {
    auto bad = [](const std::function<void(Json&)>& change, const std::string& code) {
      Json d = lookRig();
      change(d);
      return hasCode(validateRig(d), code);
    };
    check(bad([](Json& d) { d["skins"][0]["attachments"][0]["slot"] = "tail"; }, "ANIM_SLOT_MISSING"), "skin attachment on unknown slot");
    check(bad([](Json& d) { d["skins"][0]["attachments"].push_back(d["skins"][0]["attachments"][0]); }, "ANIM_DUP_ID"), "duplicate (slot,name) inside a skin");
    check(!bad([](Json& d) { d["skins"][1]["attachments"].push_back(d["skins"][0]["attachments"][1]); }, "ANIM_DUP_ID"), "same (slot,name) in two skins is fine");
    check(bad([](Json& d) { d["skins"][0]["attachments"][0]["imageAssetId"] = ""; }, "ANIM_IMAGE_MISSING"), "skin attachment without image");
    check(bad([](Json& d) { d["skins"][0]["attachments"][0]["width"] = 0; }, "ANIM_IMAGE_SIZE"), "skin attachment size");
    check(bad([](Json& d) { d["skins"][1]["id"] = "winter"; }, "ANIM_DUP_ID"), "duplicate skin id");
    check(bad([](Json& d) { d["expressions"][0]["slots"]["tail"] = "x"; }, "ANIM_SLOT_MISSING"), "expression on unknown slot");
    check(bad([](Json& d) { d["expressions"][0]["slots"]["eyes"] = "wink"; }, "ANIM_ATTACHMENT_UNKNOWN"), "expression names an unknown attachment");
    check(bad([](Json& d) { d["expressions"][0]["bones"]["tail"] = {{"rotation", 1}}; }, "ANIM_BONE_MISSING"), "expression on unknown bone");
    check(bad([](Json& d) { d["expressions"][0]["bones"]["head"]["alpha"] = 1; }, "ANIM_PROPERTY"), "expression bone channel");
    check(bad([](Json& d) { d["expressions"][1]["id"] = "happy"; }, "ANIM_DUP_ID"), "duplicate expression id");
    check(bad([](Json& d) { d["clips"][3]["tracks"][0]["keys"][1]["value"] = "angry"; }, "ANIM_EXPRESSION_UNKNOWN"), "expression key names an unknown expression");
    check(bad([](Json& d) { d["clips"][3]["tracks"][0]["keys"][0]["interp"] = "bezier"; d["clips"][3]["tracks"][0]["keys"][0]["curve"] = {0, 0, 1, 1}; }, "ANIM_INTERP"), "expression keys are step only");
    check(bad([](Json& d) { d["clips"][3]["tracks"][0]["property"] = "x"; }, "ANIM_PROPERTY"), "rig track only keys expression");
    check(bad([](Json& d) { d["clips"][3]["tracks"][1]["property"] = "expression"; }, "ANIM_PROPERTY"), "expression only on the rig track");
    // An outfit-only part: valid (some skin provides it); a skin lacking it gets a warning.
    Json d = lookRig();
    d["slots"].push_back({{"id", "hat"}, {"bone", "head"}, {"attachment", "cap"}});
    d["skins"][0]["attachments"].push_back({{"slot", "hat"}, {"name", "cap"}, {"imageAssetId", "img-cap"}, {"width", 30}, {"height", 20}});
    auto ds = validateRig(d);
    bool error = false, warnSummer = false, warnWinter = false;
    for (const auto& x : ds) {
      error |= x.severity == "error";
      if (x.code == "ANIM_SKIN_MISSING_ATTACHMENT") { warnSummer |= x.pointer == "/skins/1"; warnWinter |= x.pointer == "/skins/0"; }
    }
    check(!error && warnSummer && !warnWinter, "ANIM_SKIN_MISSING_ATTACHMENT only for the skin lacking a shown name");
    d["slots"].back()["attachment"] = "crown";
    check(hasCode(validateRig(d), "ANIM_ATTACHMENT_UNKNOWN"), "a name no list provides is an error");
    Instance bare(std::make_shared<const Rig>(parseRig("h", [&] { Json x = lookRig(); x["slots"].push_back({{"id", "hat"}, {"bone", "head"}, {"attachment", "cap"}}); x["skins"][0]["attachments"].push_back({{"slot", "hat"}, {"name", "cap"}, {"imageAssetId", "img-cap"}, {"width", 30}, {"height", 20}}); return x; }())));
    check(imageOf(bare.drawList(), "hat") == "-" && bare.setSkin("winter") && imageOf(bare.drawList(), "hat") == "img-cap", "outfit-only part appears with its skin");
  });

  test("expression layer order: after track 0, before layer tracks (blink still blinks)", [&] {
    const auto r = std::make_shared<const Rig>(parseRig("hero", lookRig()));
    Json c = lookController();
    c["defaultExpression"] = "happy";
    Instance inst(r, parseController("hero-main", c));
    auto eyes = [&] { return *inst.pose().attachments[2]; };
    auto headRotation = [&] { return inst.pose().bones[2].rotation; };
    check(inst.expression() == std::optional<std::string>("happy"), "controller defaultExpression");
    check(eyes() == "open" && near(headRotation(), 10), "blink layer (track 1) keys the eyes over the expression; head follows the expression");
    inst.update(1850);
    check(eyes() == "closed", "blink closes");
    Json noBlink = c;
    noBlink["layers"] = Json::array();
    Instance calm(r, parseController("hero-main", noBlink));
    check(*calm.pose().attachments[2] == "closed" && calm.pose().expression == 0, "without a layer the expression shows");
    // Track 0 action keys head rotation: the expression still wins (it applies after track 0)...
    calm.signal("actionResult", {{"outcome", "success"}});
    calm.update(300);  // joy alone would turn the head to 15
    check(calm.describe()["tracks"][0]["clip"] == "joy" && near(calm.pose().bones[2].rotation, 10), "expression overrides track 0");
    check(near(calm.pose().bones[1].scaleY, 1.2), "unlisted channels keep animating");
    // ...but a layer track keying the same channel wins over the expression.
    calm.play(2, "joy", false, 1, Then::Hold, 0);
    calm.update(300);
    check(near(calm.pose().bones[2].rotation, 15), "track 2 joy overrides the expression's head rotation");
  });

  test("expression keys: step, override the instance expression", [&] {
    const auto r = std::make_shared<const Rig>(parseRig("hero", lookRig()));
    Instance inst(r);
    check(inst.setExpression(std::string("surprised")) && inst.pose().expression == 1 && !inst.pose().attachments[1], "setExpression (surprised hides the face)");
    check(near(inst.pose().bones[2].y, -90) && near(inst.pose().bones[2].scaleY, 1.1), "bone overrides");
    check(!inst.setExpression(std::string("angry")) && inst.expression() == std::optional<std::string>("surprised"), "unknown expression refused");
    inst.play(0, "talk-face", false, 1, Then::Hold, 0);
    inst.update(250);
    check(inst.describe()["activeExpression"] == "happy" && *inst.pose().attachments[2] == "closed" && near(inst.pose().bones[2].rotation, 10), "clip key overrides the instance expression");
    inst.update(249);
    check(inst.describe()["activeExpression"] == "happy", "step: still happy at 499 ms");
    inst.update(2);
    check(inst.describe()["activeExpression"].is_null() && *inst.pose().attachments[2] == "open" && near(inst.pose().bones[2].y, -80), "null key = no expression from 500 ms");
    check(inst.describe()["expression"] == "surprised", "instance expression itself unchanged");
    inst.stop(0, 0);
    check(inst.pose().expression == 1, "clip gone: instance expression again");
    // samplePose: look expression, overridden by the clip's keys; expressions=false = raw clip.
    check(samplePose(*r, -1, 0, {-1, 1}).expression == 1 && samplePose(*r, 3, 100, {-1, 1}).expression == 0 && samplePose(*r, 3, 700, {-1, 1}).expression == -1, "samplePose look + clip keys");
    Look raw; raw.expressions = false;
    check(*samplePose(*r, 3, 100, raw).attachments[2] == "open", "expressions=false samples raw values");
  });

  test("controller skin/expression bindings and save/restore", [&] {
    const auto r = std::make_shared<const Rig>(parseRig("hero", lookRig()));
    Instance inst(r, parseController("hero-main", lookController()));
    check(inst.skin() == "winter" && !inst.expression(), "defaultSkin applied");
    auto matched = inst.signal("portraitChanged", {{"portraitId", "happy"}, {"characterId", "alice"}});
    check(matched == std::vector<std::string>{"face-happy"} && inst.expression() == std::optional<std::string>("happy"), "expression-only binding");
    check(inst.describe()["tracks"].size() == 2 && inst.describe()["tracks"][0]["clip"] == "idle", "it takes no track");
    check(inst.signal("phaseChanged", {{"phase", "summer"}}) == std::vector<std::string>{"dress-summer"} && inst.skin() == "summer", "skin binding");
    inst.signal("actionResult", {{"outcome", "success"}});
    inst.update(100);
    const Json saved = inst.saveState();
    check(saved["skin"] == "summer" && saved["expression"] == "happy", "saveState includes the look");
    Instance other(r, parseController("hero-main", lookController()));
    other.restoreState(saved);
    check(other.skin() == "summer" && other.expression() == std::optional<std::string>("happy") && other.describe()["tracks"][0]["clip"] == "joy", "restore both (and the clip)");
    check(other.signal("portraitChanged", {{"portraitId", "none"}}) == std::vector<std::string>{"face-clear"} && !other.expression(), "expression: null clears it");
    Json stale = saved;
    stale["skin"] = "gone";
    stale["expression"] = "gone";
    other.restoreState(stale);
    check(other.skin() == "winter" && !other.expression(), "ids removed since the save fall back to the controller defaults");
    Json old = saved;
    old.erase("skin"); old.erase("expression");
    other.setSkin("summer");
    other.restoreState(old);
    check(other.skin() == "summer", "1.x saves without a look keep the current one");
    // Validation.
    auto codes = [&](const std::function<void(Json&)>& change) { Json c = lookController(); change(c); return validateController(c, r.get()); };
    check(hasCode(codes([](Json& c) { c["defaultSkin"] = "autumn"; }), "ANIM_SKIN_UNKNOWN"), "unknown defaultSkin");
    check(hasCode(codes([](Json& c) { c["defaultExpression"] = "angry"; }), "ANIM_EXPRESSION_UNKNOWN"), "unknown defaultExpression");
    check(hasCode(codes([](Json& c) { c["bindings"][2]["expression"] = "angry"; }), "ANIM_EXPRESSION_UNKNOWN"), "unknown binding expression");
    check(hasCode(codes([](Json& c) { c["bindings"][4]["skin"] = "autumn"; }), "ANIM_SKIN_UNKNOWN"), "unknown binding skin");
    check(hasCode(codes([](Json& c) { c["bindings"][2].erase("expression"); }), "ANIM_CLIP_UNKNOWN"), "binding needs a clip, skin or expression");
    check(codes([](Json& c) { c["bindings"][0]["expression"] = "surprised"; }).empty(), "clip + expression binding");
    check(validateController(lookController(), nullptr).empty(), "structure-only check without the rig");
  });

  test("host protocol: mount props.skin, setSkin/setExpression, sample look", [&] {
    Host host;
    const Json docs = Json::array({{{"type", "rig"}, {"id", "hero"}, {"data", lookRig()}}, {{"type", "controller"}, {"id", "hero-main"}, {"data", controllerData()}}});
    check(host.handle({{"op", "init"}, {"documents", docs}})["diagnostics"].empty(), "init");
    Json element = {{"type", "character"}, {"ref", "hero-main"}, {"width", 100}, {"height", 150}, {"props", {{"skin", "winter"}}}};
    auto torso = [&] { return host.handle({{"op", "frame"}, {"instance", "s/hero"}})["items"][0]["imageAssetId"]; };
    check(host.handle({{"op", "mount"}, {"instance", "s/hero"}, {"element", element}}).value("ok", false) && torso() == "img-torso-winter", "props.skin picks the outfit");
    check(host.handle({{"op", "setSkin"}, {"instance", "s/hero"}, {"skin", "summer"}}).value("ok", false) && torso() == "img-torso-summer", "setSkin op");
    host.handle({{"op", "mount"}, {"instance", "s/hero"}, {"element", element}});
    check(torso() == "img-torso-summer", "re-mount with the same props keeps the instance look");
    element["props"]["skin"] = "winter-2";
    check(host.handle({{"op", "mount"}, {"instance", "s/hero"}, {"element", element}}).value("ok", false) && torso() == "img-torso-summer", "unknown props.skin ignored (service reports it)");
    element["props"]["skin"] = "winter";
    host.handle({{"op", "mount"}, {"instance", "s/hero"}, {"element", element}});
    check(torso() == "img-torso-winter", "a changed props.skin is applied");
    check(!host.request(Json({{"op", "setSkin"}, {"instance", "s/hero"}, {"skin", "nope"}}).dump()).value("ok", true), "unknown skin rejected");
    check(host.handle({{"op", "setExpression"}, {"instance", "s/hero"}, {"expression", "happy"}}).value("ok", false), "setExpression");
    check(host.handle({{"op", "describe"}, {"instance", "s/hero"}})["state"]["expression"] == "happy", "describe shows it");
    const Json saved = host.handle({{"op", "saveAll"}})["state"];
    check(saved["instances"]["s/hero"]["state"]["skin"] == "winter" && saved["instances"]["s/hero"]["state"]["expression"] == "happy", "saveAll carries the look");
    check(host.handle({{"op", "setExpression"}, {"instance", "s/hero"}, {"expression", nullptr}}).value("ok", false) &&
              host.handle({{"op", "describe"}, {"instance", "s/hero"}})["state"]["expression"].is_null(), "null clears");
    host.handle({{"op", "restoreAll"}, {"state", saved}});
    check(host.handle({{"op", "describe"}, {"instance", "s/hero"}})["state"]["expression"] == "happy", "restoreAll restores it");
    const Json pose = host.handle({{"op", "sample"}, {"rig", lookRig()}, {"skin", "winter"}, {"expression", "happy"}})["pose"];
    check(pose["skin"] == "winter" && pose["expression"] == "happy" && pose["slots"]["eyes"]["attachment"] == "closed" && near(pose["bones"]["head"]["rotation"].get<double>(), 10), "sample with a look");
    check(!host.request(Json({{"op", "sample"}, {"rig", lookRig()}, {"skin", "nope"}}).dump()).value("ok", true), "sample refuses unknown skin");
    const Json script = lookScript();
    Host replay;
    std::vector<Json> frames;
    for (const auto& op : script) { const Json reply = replay.handle(op); check(reply.value("ok", false), "script op " + op.value("op", std::string())); if (op["op"] == "frame") frames.push_back(reply["items"]); }
    auto item = [&](size_t f, const char* slot) { for (const auto& i : frames[f]) if (i["slot"] == slot) return i; return Json(); };
    auto image = [&](size_t f, const char* slot) { const Json i = item(f, slot); return i.is_null() ? std::string("-") : i["imageAssetId"].get<std::string>(); };
    check(frames.size() == 8 && image(0, "torso") == "img-torso-winter" && image(0, "face") == "-" && image(0, "eyes") == "img-eyes-open", "parity script: props.skin + defaultExpression (face hidden)");
    check(image(1, "eyes") == "img-eyes-closed-winter" && image(2, "eyes") == "img-eyes-closed-winter" && image(2, "face") == "img-face", "blink over the expression; portrait binding");
    check(image(3, "torso") == "img-torso-summer" && image(3, "eyes") == "img-eyes-closed", "setSkin: summer has no closed eyes, default used");
    check(item(4, "face")["matrix"][1] == 0 && item(5, "face")["matrix"][1] != 0 && item(6, "face")["matrix"][1] == 0, "clip expression key (head turn) then null");
    check(image(7, "torso") == "img-torso", "back to default attachments");
  });

  test("edit ops: skins", [&] {
    const Json base = lookRig();
    const Json ctrls = Json::array({{{"id", "hero-main"}, {"data", lookController()}}});
    auto r = applyEdit(base, {{"op", "addSkin"}, {"id", "autumn"}, {"name", "가을"}});
    check(r.data["skins"][2] == Json({{"id", "autumn"}, {"name", "가을"}, {"attachments", Json::array()}}) && validateRig(r.data).empty(), "addSkin");
    check(refused(base, {{"op", "addSkin"}, {"id", "winter"}}) && refused(base, {{"op", "addSkin"}, {"id", "Bad Id"}}), "duplicate / invalid id refused");
    r = applyEdit(r.data, {{"op", "setSkinAttachment"}, {"skin", "autumn"}, {"slot", "torso"}, {"imageAssetId", "img-torso-autumn"}, {"width", 84}, {"height", 126}});
    const Json made = r.data["skins"][2]["attachments"][0];
    check(made["name"] == "torso" && made["pivotY"] == 1 && made["x"] == 0 && r.extra["created"] == true, "name = slot's setup attachment, placement copied from the default");
    r = applyEdit(r.data, {{"op", "setSkinAttachment"}, {"skin", "autumn"}, {"slot", "torso"}, {"imageAssetId", "img-torso-autumn-2"}, {"x", 3}});
    check(r.data["skins"][2]["attachments"].size() == 1 && r.data["skins"][2]["attachments"][0]["imageAssetId"] == "img-torso-autumn-2" && r.data["skins"][2]["attachments"][0]["x"] == 3, "update in place");
    check(refused(r.data, {{"op", "setSkinAttachment"}, {"skin", "autumn"}, {"slot", "eyes"}, {"name", "wink"}}), "new entry needs an image and size");
    r = applyEdit(r.data, {{"op", "removeSkinAttachment"}, {"skin", "autumn"}, {"slot", "torso"}, {"name", "torso"}});
    check(r.data["skins"][2]["attachments"].empty(), "removeSkinAttachment");
    r = applyEdit(base, {{"op", "duplicateSkin"}, {"skin", "winter"}, {"id", "winter-b"}});
    check(r.data["skins"][2]["attachments"] == base["skins"][0]["attachments"] && r.data["skins"][2]["name"] == "겨울 복사", "duplicateSkin");
    r = applyEdit(base, {{"op", "renameSkin"}, {"skin", "summer"}, {"id", "beach"}}, ctrls);
    check(r.data["skins"][1]["id"] == "beach" && r.extra["controllerRefs"] == Json::array({"hero-main"}), "renameSkin reports controllers");
    check(r.extra["controllers"][0]["data"]["bindings"][4]["skin"] == "beach", "binding skin rewritten");
    r = applyEdit(base, {{"op", "renameSkin"}, {"skin", "winter"}, {"id", "snow"}}, ctrls);
    check(r.extra["controllers"][0]["data"]["defaultSkin"] == "snow", "defaultSkin rewritten");
    check(refused(base, {{"op", "deleteSkin"}, {"skin", "winter"}}, ctrls), "delete of a used skin refused");
    r = applyEdit(base, {{"op", "deleteSkin"}, {"skin", "summer"}, {"force", true}}, ctrls);
    const Json forced = r.extra["controllers"][0]["data"];
    check(r.data["skins"].size() == 1 && forced["bindings"].size() == 4 && validateController(forced, std::make_shared<const Rig>(parseRig("x", r.data)).get()).empty(), "force removes the skin-only binding");
    r = applyEdit(applyEdit(base, {{"op", "deleteSkin"}, {"skin", "summer"}}).data, {{"op", "deleteSkin"}, {"skin", "winter"}});
    check(!r.data.contains("skins"), "last skin gone: no empty skins list");
    // Deleting a slot/bone leaves no dangling skin attachments or expression entries.
    r = applyEdit(base, {{"op", "deleteSlot"}, {"slot", "eyes"}});
    check(r.data["skins"][0]["attachments"].size() == 1 && !r.data["expressions"][0].contains("slots") && validateRig(r.data).empty(), "deleteSlot purges skins/expressions");
    r = applyEdit(base, {{"op", "deleteBone"}, {"bone", "head"}, {"cascade", true}});
    check(!r.data["expressions"][0].contains("bones") && validateRig(r.data).empty(), "deleteBone purges expression bones");
    // Attachment names from skins are keyable; 1.x documents never grow skins/expressions keys.
    Json d = base;
    d["slots"].push_back({{"id", "hat"}, {"bone", "head"}, {"attachment", nullptr}});
    d["skins"][0]["attachments"].push_back({{"slot", "hat"}, {"name", "cap"}, {"imageAssetId", "img-cap"}, {"width", 30}, {"height", 20}});
    check(applyEdit(d, {{"op", "setKey"}, {"clip", "joy"}, {"target", "slot:hat"}, {"property", "attachment"}, {"timeMs", 0}, {"value", "cap"}}).data != d, "skin-only name keyable");
    r = applyEdit(rigData(), {{"op", "setKey"}, {"clip", "joy"}, {"target", "bone:head"}, {"property", "x"}, {"timeMs", 100}, {"value", 2}});
    check(!r.data.contains("skins") && !r.data.contains("expressions"), "unrelated edits keep 1.x documents unchanged in shape");
  });

  test("edit ops: expressions, keys, capture", [&] {
    const Json base = lookRig();
    const Json ctrls = Json::array({{{"id", "hero-main"}, {"data", lookController()}}});
    auto r = applyEdit(base, {{"op", "addExpression"}, {"id", "sleepy"}, {"slots", {{"eyes", "closed"}}}});
    check(r.data["expressions"][2] == Json({{"id", "sleepy"}, {"name", "sleepy"}, {"slots", {{"eyes", "closed"}}}}), "addExpression");
    check(refused(base, {{"op", "addExpression"}, {"id", "x"}, {"slots", {{"eyes", "wink"}}}}), "invalid expression content refused");
    r = applyEdit(r.data, {{"op", "setExpressionSlot"}, {"expression", "sleepy"}, {"slot", "face"}, {"attachment", nullptr}});
    check(r.data["expressions"][2]["slots"]["face"].is_null(), "slot hidden by the expression");
    r = applyEdit(r.data, {{"op", "setExpressionSlot"}, {"expression", "sleepy"}, {"slot", "face"}, {"remove", true}});
    check(!r.data["expressions"][2]["slots"].contains("face"), "slot removed from the expression");
    check(refused(r.data, {{"op", "setExpressionSlot"}, {"expression", "sleepy"}, {"slot", "eyes"}, {"attachment", "wink"}}), "unknown attachment refused");
    r = applyEdit(r.data, {{"op", "setExpressionBone"}, {"expression", "sleepy"}, {"bone", "head"}, {"rotation", -8}, {"y", -78}});
    check(r.data["expressions"][2]["bones"]["head"] == Json({{"rotation", -8}, {"y", -78}}), "setExpressionBone");
    r = applyEdit(r.data, {{"op", "setExpressionBone"}, {"expression", "sleepy"}, {"bone", "head"}, {"rotation", nullptr}});
    check(r.data["expressions"][2]["bones"]["head"] == Json({{"y", -78}}), "null drops a channel");
    r = applyEdit(r.data, {{"op", "setExpressionBone"}, {"expression", "sleepy"}, {"bone", "head"}, {"remove", true}});
    check(!r.data["expressions"][2].contains("bones"), "bone override removed");
    // Rig expression keys.
    r = applyEdit(base, {{"op", "setKey"}, {"clip", "joy"}, {"target", "rig"}, {"property", "expression"}, {"timeMs", 200}, {"value", "surprised"}, {"interp", "linear"}});
    check(trackOf(r.data, "joy", "rig", "expression") == Json::array({{{"timeMs", 200}, {"value", "surprised"}}}) && validateRig(r.data).empty(), "expression key (no interp stored)");
    check(refused(base, {{"op", "setKey"}, {"clip", "joy"}, {"target", "rig"}, {"property", "expression"}, {"timeMs", 0}, {"value", "angry"}}), "unknown expression key refused");
    check(refused(base, {{"op", "setKey"}, {"clip", "joy"}, {"target", "rig"}, {"property", "x"}, {"timeMs", 0}, {"value", 1}}), "rig only keys expression");
    check(refused(base, {{"op", "setInterp"}, {"clip", "talk-face"}, {"target", "rig"}, {"property", "expression"}, {"timeMs", 0}, {"interp", "linear"}}), "expression interp refused");
    const Json board = applyEdit(base, {{"op", "copyKeys"}, {"clip", "talk-face"}, {"keys", Json::array({ref("rig", "expression", 0), ref("rig", "expression", 500)})}}).extra["clipboard"];
    check(!board["keys"][0].contains("interp") && board["keys"][1]["value"].is_null(), "copied expression keys carry no interp");
    r = applyEdit(base, {{"op", "pasteKeys"}, {"clip", "idle"}, {"timeMs", 0}, {"keys", board["keys"]}});
    check(trackOf(r.data, "idle", "rig", "expression").size() == 2, "paste expression keys");
    r = applyEdit(base, {{"op", "moveKeys"}, {"clip", "talk-face"}, {"keys", Json::array({ref("rig", "expression", 500)})}, {"deltaMs", 100}});
    check(trackOf(r.data, "talk-face", "rig", "expression")[1]["timeMs"] == 600, "move expression keys");
    // Rename/delete keep clips and controllers consistent.
    Json c = lookController();
    c["defaultExpression"] = "happy";
    const Json withDefault = Json::array({{{"id", "hero-main"}, {"data", c}}});
    r = applyEdit(base, {{"op", "renameExpression"}, {"expression", "happy"}, {"id", "joyful"}, {"name", "즐거움"}}, withDefault);
    check(trackOf(r.data, "talk-face", "rig", "expression")[0]["value"] == "joyful" && r.extra["keys"] == 1 && r.data["expressions"][0]["name"] == "즐거움", "renameExpression rewrites clip keys");
    check(r.extra["controllers"][0]["data"]["defaultExpression"] == "joyful" && r.extra["controllers"][0]["data"]["bindings"][2]["expression"] == "joyful", "and controllers");
    check(refused(base, {{"op", "deleteExpression"}, {"expression", "happy"}}, ctrls), "used (keys + binding) expression refused");
    r = applyEdit(base, {{"op", "deleteExpression"}, {"expression", "happy"}, {"force", true}}, ctrls);
    check(trackOf(r.data, "talk-face", "rig", "expression").size() == 1 && r.extra["controllers"][0]["data"]["bindings"].size() == 4 && validateRig(r.data).empty(), "force removes keys and the expression-only binding");
    check(applyEdit(base, {{"op", "deleteExpression"}, {"expression", "surprised"}}).data["expressions"].size() == 1, "unused expression deletes");
    // Capture: the pending editor change (from) at a clip time becomes the expression.
    Json pending = applyEdit(base, {{"op", "setKey"}, {"clip", "idle"}, {"target", "slot:eyes"}, {"property", "attachment"}, {"timeMs", 500}, {"value", "closed"}}).data;
    pending = applyEdit(pending, {{"op", "setKey"}, {"clip", "idle"}, {"target", "bone:head"}, {"property", "rotation"}, {"timeMs", 500}, {"value", -6}}).data;
    r = applyEdit(base, {{"op", "captureExpression"}, {"id", "wink"}, {"name", "윙크"}, {"clip", "idle"}, {"timeMs", 500}, {"from", pending}});
    check(r.data["expressions"][2] == Json({{"id", "wink"}, {"name", "윙크"}, {"slots", {{"eyes", "closed"}}}, {"bones", {{"head", {{"rotation", -6}}}}}}), "only the pending differences (idle's body bob is not captured)");
    check(trackOf(r.data, "idle", "slot:eyes", "attachment").is_null(), "pending keys themselves are not committed");
    r = applyEdit(base, {{"op", "captureExpression"}, {"id", "joy-pose"}, {"clip", "joy"}, {"timeMs", 400}});
    check(r.data["expressions"][2]["bones"]["head"] == Json({{"rotation", 20}}) && r.data["expressions"][2]["bones"]["body"] == Json({{"scaleY", 1.2}}) && !r.data["expressions"][2].contains("slots"), "without from: differences from the setup pose");
    r = applyEdit(base, {{"op", "captureExpression"}, {"id", "eyes-only"}, {"slots", {"eyes"}}, {"bones", Json::array()}});
    check(r.data["expressions"][2] == Json({{"id", "eyes-only"}, {"name", "eyes-only"}, {"slots", {{"eyes", "open"}}}}), "listed slots captured whole");
    check(refused(base, {{"op", "captureExpression"}, {"id", "same"}}), "nothing differs: refused");
    check(refused(base, {{"op", "captureExpression"}, {"id", "happy"}, {"clip", "joy"}, {"timeMs", 400}}), "existing id refused without replace");
    r = applyEdit(base, {{"op", "captureExpression"}, {"id", "happy"}, {"clip", "joy"}, {"timeMs", 400}, {"replace", true}});
    check(r.data["expressions"][0]["name"] == "기쁨" && !r.data["expressions"][0].contains("slots"), "replace keeps the name");
    // LLM path = editor path.
    Host host;
    const Json op = {{"op", "captureExpression"}, {"id", "wink"}, {"clip", "idle"}, {"timeMs", 500}, {"from", pending}};
    check(host.handle({{"op", "edit"}, {"rig", base}, {"operation", op}})["rig"] == applyEdit(base, op).data, "host edit = core edit");
  });

  test("1.x documents: no skins/expressions, same behaviour", [&] {
    check(validateRig(rigData()).empty() && validateRig(bezierRig()).empty(), "1.0/1.1 rigs validate unchanged");
    const auto r = rig();
    check(r->skins.empty() && r->expressions.empty(), "nothing parsed");
    Instance inst(r, parseController("hero-main", controllerData()));
    const Json s = inst.saveState();
    check(s["skin"] == "" && s["expression"].is_null() && inst.pose().skin == -1 && inst.pose().expression == -1, "default look");
    Json old = s;
    old.erase("skin"); old.erase("expression");
    Instance back(r, parseController("hero-main", controllerData()));
    back.restoreState(old);
    check(back.describe()["tracks"].size() == s["tracks"].size(), "1.x save restores");
  });

  // ---- IK constraints (plugin 1.3.0) ----
  auto ikPose = [](const Json& d, int clip = -1, double timeMs = 0) {
    const Rig r = parseRig("arm", d);
    return samplePose(r, clip, timeMs);
  };
  auto at = [](const Pose& p, size_t bone) { return std::pair<double, double>{p.world[bone].e, p.world[bone].f}; };
  auto reaches = [&](const Pose& p, double x, double y, double eps = 1e-9) { return near(at(p, 3).first, x, eps) && near(at(p, 3).second, y, eps); };
  // Bend side on screen: cross(elbow - shoulder, wrist - elbow) (> 0 = clockwise on a y-down screen).
  auto bendCross = [&](const Pose& p) {
    const auto s = at(p, 1), e = at(p, 2), w = at(p, 3);
    return (e.first - s.first) * (w.second - e.second) - (e.second - s.second) * (w.first - e.first);
  };
  test("ik validation", [&] {
    check(validateRig(armRig(60, 40)).empty(), "arm fixture validates");
    check(validateRig(ikParityRig()).empty(), "parity fixture validates");
    auto bad = [&](const std::function<void(Json&)>& edit, const std::string& code) {
      Json d = armRig(60, 40);
      edit(d);
      return hasCode(validateRig(d), code);
    };
    check(bad([](Json& d) { d["ik"].push_back(d["ik"][0]); }, "ANIM_IK_DUP_ID"), "duplicate id");
    check(bad([](Json& d) { d["ik"][0]["bones"] = {"upper", "elbow"}; }, "ANIM_IK_BONE_MISSING"), "unknown chain bone");
    check(bad([](Json& d) { d["ik"][0]["target"] = "nobody"; }, "ANIM_IK_BONE_MISSING"), "unknown target");
    check(bad([](Json& d) { d["ik"][0]["bones"] = {"upper", "hand"}; }, "ANIM_IK_CHAIN"), "non-adjacent chain");
    check(bad([](Json& d) { d["ik"][0]["bones"] = {"lower", "upper"}; }, "ANIM_IK_CHAIN"), "child first");
    check(bad([](Json& d) { d["ik"][0]["bones"] = Json::array(); }, "ANIM_IK_CHAIN"), "empty chain");
    check(!bad([](Json& d) { d["ik"][0]["bones"] = {"root", "upper", "lower"}; }, "ANIM_IK_CHAIN"), "three bones: a chain (plugin 1.13.0)");
    check(bad([](Json& d) { d["ik"][0]["bones"] = {"root", "upper", "hand"}; }, "ANIM_IK_CHAIN"), "three bones with a gap");
    check(bad([](Json& d) { d["ik"][0]["target"] = "lower"; }, "ANIM_IK_TARGET"), "target in the chain");
    check(bad([](Json& d) { d["ik"][0]["target"] = "hand"; }, "ANIM_IK_TARGET"), "target below the chain end");
    check(bad([](Json& d) { d["ik"][0]["mix"] = 1.5; }, "ANIM_IK_MIX"), "mix range");
    check(bad([](Json& d) { d["ik"][0]["bendPositive"] = 1; }, "ANIM_STRUCTURE"), "bend must be bool");
    {
      Json s = armRig(60, 40);
      s["ik"][0]["stretch"] = true;
      const auto ds = validateRig(s);
      // Plugin 1.13.0: stretch works along bone lengths; the arm fixture has none, so it warns.
      check(ds.size() == 1 && ds[0].code == "ANIM_IK_STRETCH_LENGTH" && ds[0].severity == "warning", "stretch without bone lengths: warned, no-op");
    }
    // Cycle: `a` aims upper at a bone under head, `b` aims head at a bone under upper.
    check(bad([](Json& d) {
      d["bones"].push_back({{"id", "head"}, {"parent", "root"}, {"x", 0}, {"y", -40}});
      d["bones"].push_back({{"id", "eye"}, {"parent", "head"}, {"x", 5}, {"y", 0}});
      d["ik"] = Json::array({{{"id", "a"}, {"bones", {"upper"}}, {"target", "eye"}}, {{"id", "b"}, {"bones", {"head"}}, {"target", "lower"}}});
    }, "ANIM_IK_CYCLE"), "dependency cycle");
    // Order: `look` aims at the hand that `arm` moves, so it must be solved after it.
    Json d = armRig(60, 40);
    d["bones"].push_back({{"id", "head"}, {"parent", "root"}, {"x", 0}, {"y", -40}});
    d["ik"].push_back({{"id", "look"}, {"bones", {"head"}}, {"target", "hand"}});
    check(validateRig(d).empty(), "dependent constraint after its source: fine");
    d["ik"][1]["order"] = -1;
    const auto ds = validateRig(d);
    check(hasCode(ds, "ANIM_IK_ORDER") && !hasCode(ds, "ANIM_IK_CYCLE") && parseRig("x", d).ikOrder == std::vector<int>({1, 0}), "order before its source: warning, order honoured");
    // Tracks.
    auto track = [&](const Json& tr, const std::string& code) {
      Json x = armRig(60, 40);
      x["clips"] = Json::array({{{"id", "c"}, {"durationMs", 1000}, {"loop", false}, {"tracks", Json::array({tr})}}});
      return hasCode(validateRig(x), code);
    };
    check(!track({{"target", "ik:arm"}, {"property", "mix"}, {"keys", {{{"timeMs", 0}, {"value", 0.5}}}}}, "ANIM_PROPERTY") &&
          track({{"target", "ik:arm"}, {"property", "mix"}, {"keys", {{{"timeMs", 0}, {"value", 1.5}}}}}, "ANIM_IK_MIX"), "mix keys 0..1");
    check(track({{"target", "ik:nope"}, {"property", "mix"}, {"keys", {{{"timeMs", 0}, {"value", 1}}}}}, "ANIM_TARGET_MISSING"), "unknown constraint track");
    check(track({{"target", "ik:arm"}, {"property", "x"}, {"keys", {{{"timeMs", 0}, {"value", 1}}}}}, "ANIM_PROPERTY"), "ik tracks only key mix/bend");
    check(track({{"target", "bone:upper"}, {"property", "mix"}, {"keys", {{{"timeMs", 0}, {"value", 1}}}}}, "ANIM_PROPERTY"), "mix only on ik tracks");
    check(track({{"target", "ik:arm"}, {"property", "bendPositive"}, {"keys", {{{"timeMs", 0}, {"value", 1}}}}}, "ANIM_KEY_VALUE"), "bend keys are bool");
    check(track({{"target", "ik:arm"}, {"property", "bendPositive"}, {"keys", {{{"timeMs", 0}, {"value", true}, {"interp", "bezier"}, {"curve", {0, 0, 1, 1}}}}}}, "ANIM_INTERP"), "bend keys are step");
  });

  test("ik two-bone: reach, unreachable, fold, bend flip, mix", [&] {
    Pose p = ikPose(armRig(60, 40));
    check(reaches(p, 60, 40), "reachable target: the wrist lands on it");
    check(near(std::hypot(at(p, 2).first, at(p, 2).second), 50, 1e-9), "upper link keeps its length");
    check(p.bones[1].rotation == 0 && p.bones[2].rotation == 0, "animated (keyable) values untouched");
    check(!p.applied.empty() && !near(p.applied[1].rotation, 0), "solved rotations in applied");
    check(bendCross(p) > 0, "bendPositive: clockwise bend on screen");
    Json flip = armRig(60, 40);
    flip["ik"][0]["bendPositive"] = false;
    const Pose q = ikPose(flip);
    check(reaches(q, 60, 40) && bendCross(q) < 0, "bend flip: same wrist, elbow on the other side");
    // The two elbows are mirror images across the shoulder -> target line.
    const double ux = 60 / std::hypot(60, 40), uy = 40 / std::hypot(60, 40);
    const auto e1 = at(p, 2), e2 = at(q, 2);
    check(near(e1.first * ux + e1.second * uy, e2.first * ux + e2.second * uy, 1e-9) && near(e1.first * uy - e1.second * ux, -(e2.first * uy - e2.second * ux), 1e-9), "mirrored elbows");
    p = ikPose(armRig(0, 300));
    check(near(at(p, 2).first, 0, 1e-9) && near(at(p, 2).second, 50, 1e-9) && reaches(p, 0, 100), "unreachable: straight at the target");
    Json shortArm = armRig(10, 0);
    shortArm["bones"][3]["x"] = 30;
    p = ikPose(shortArm);
    check(near(at(p, 2).first, 50, 1e-6) && reaches(p, 20, 0, 1e-6), "too close: folded towards the target");
    Json zero = armRig(0, 0);
    p = ikPose(zero);
    check(reaches(p, 100, 0) && p.applied[1].rotation == 0, "target on the shoulder: no direction, FK pose");
    // mix
    Json m = armRig(60, 40);
    m["ik"][0]["mix"] = 0;
    check(reaches(ikPose(m), 100, 0), "mix 0 = forward kinematics");
    const Pose full = ikPose(armRig(60, 40));
    m["ik"][0]["mix"] = 0.5;
    p = ikPose(m);
    check(near(p.applied[1].rotation, full.applied[1].rotation / 2, 1e-9) && near(p.applied[2].rotation, full.applied[2].rotation / 2, 1e-9), "mix 0.5 = halfway rotations");
    // No effector (end bone without children): the child origin aims at the target.
    Json noTip = armRig(0, 80);
    noTip["bones"].erase(3);
    noTip["slots"].erase(1); noTip["attachments"].erase(1);
    p = ikPose(noTip);
    check(near(at(p, 2).first, 0, 1e-9) && near(at(p, 2).second, 50, 1e-9), "no tip: elbow points at the target");
  });

  test("ik respects rotated, scaled and mirrored parents", [&] {
    for (const Json& rootT : {Json{{"rotation", 30}, {"scaleX", 2}, {"scaleY", 0.5}}, Json{{"rotation", -70}, {"scaleX", -1.5}, {"scaleY", 1}}}) {
      Json d = armRig(60, 40);
      for (auto it = rootT.begin(); it != rootT.end(); ++it) d["bones"][0][it.key()] = it.value();
      d["bones"][0]["x"] = 10; d["bones"][0]["y"] = 20;
      const Pose p = ikPose(d);
      const auto tw = at(p, 4);
      check(reaches(p, tw.first, tw.second, 1e-9), "any ancestor transform is exact: " + rootT.dump());
    }
    for (const Json& upperT : {Json{{"scaleX", 1.5}, {"scaleY", 1.5}}, Json{{"scaleX", -1.2}, {"scaleY", -1.2}}, Json{{"scaleX", -1}, {"scaleY", 1}, {"rotation", 40}}}) {
      Json d = armRig(60, 50);
      for (auto it = upperT.begin(); it != upperT.end(); ++it) d["bones"][1][it.key()] = it.value();
      d["bones"][2]["scaleX"] = 0.8;  // the child's own scale shortens the forearm link exactly
      const Pose p = ikPose(d);
      check(reaches(p, 60, 50, 1e-9), "uniform / mirrored upper bone is exact: " + upperT.dump());
    }
    Json d = armRig(60, 40);
    d["bones"][0]["scaleX"] = -1;  // a mirrored parent space keeps bendPositive's side on screen
    d["bones"][4]["x"] = -60;
    check(bendCross(ikPose(d)) > 0 && reaches(ikPose(d), 60, 40), "mirrored root: same screen side");
  });

  test("ik one-bone aim", [&] {
    Json d = armRig(0, 80);
    d["ik"][0]["bones"] = {"upper"};
    Pose p = ikPose(d);
    check(near(std::atan2(p.world[1].b, p.world[1].a) * 180 / 3.14159265358979323846, 90, 1e-9), "x axis points at the target");
    d["bones"][0]["rotation"] = 30; d["bones"][0]["scaleX"] = 2; d["bones"][0]["scaleY"] = 0.5;
    d["bones"][1]["scaleX"] = -1;  // a flipped bone aims its (flipped) +x axis
    p = ikPose(d);
    const double ax = p.world[1].a, ay = p.world[1].b, tx = p.world[4].e - p.world[1].e, ty = p.world[4].f - p.world[1].f;
    check(near(ax * ty - ay * tx, 0, 1e-9) && ax * tx + ay * ty > 0, "aim exact under a skewing parent and a flipped bone");
    d["ik"][0]["mix"] = 0.25;
    check(near(ikPose(d).applied[1].rotation, p.applied[1].rotation * 0.25, 1e-9), "aim mix");
  });

  test("ik aim axis (plugin 1.4.1)", [&] {
    const double DEGREES = 180 / 3.14159265358979323846;
    // The image's long axis (its +y through the attachment placement) in rig space.
    auto imageAxis = [](const Rig& r, const Pose& p) {
      const Affine m = p.world[size_t(r.boneIndex("arm"))] * attachmentPlacement(r.attachments[0]);
      return std::pair<double, double>{m.c, m.d};
    };
    auto angleTo = [&](std::pair<double, double> axis, const Pose& p, const Rig& r) {
      const auto& o = p.world[size_t(r.boneIndex("arm"))];
      const auto& t = p.world[size_t(r.boneIndex("t"))];
      return std::fabs(std::remainder(std::atan2(axis.second, axis.first) - std::atan2(t.f - o.f, t.e - o.e), 2 * 3.14159265358979323846)) * DEGREES;
    };
    Json d = hangingArm();
    d["ik"] = Json::array({{{"id", "aim"}, {"bones", {"arm"}}, {"target", "t"}, {"aimAxis", 90}}});
    check(validateRig(d).empty(), "aimAxis 90 validates");
    Rig r = parseRig("arm", d);
    Pose p = samplePose(r, -1, 0);
    check(angleTo(imageAxis(r, p), p, r) < 1e-9, "axis 90: the hanging image points at the target");
    double dx = 0, dy = 0;
    check(ikAimDirection(r, p, 0, dx, dy) && angleTo({dx, dy}, p, r) < 1e-9, "ikAimDirection is the solved axis");
    // Exact under a rotated, non-uniformly scaled parent and bone, oblique axis.
    d["bones"][0]["rotation"] = 25; d["bones"][0]["scaleX"] = 1.5; d["bones"][0]["scaleY"] = 0.6;
    d["bones"][1]["scaleX"] = -0.8; d["bones"][1]["scaleY"] = 1.7;
    d["ik"][0]["aimAxis"] = -130;
    r = parseRig("arm", d);
    p = samplePose(r, -1, 0);
    check(ikAimDirection(r, p, 0, dx, dy) && angleTo({dx, dy}, p, r) < 1e-9, "oblique axis exact under skew and a flipped, stretched bone");
    // Old documents: no field == aimAxis 0, bit for bit, and the +x axis still aims.
    Json old = armRig(0, 80);
    old["ik"][0]["bones"] = {"upper"};
    old["bones"][0]["rotation"] = 30; old["bones"][0]["scaleX"] = 2; old["bones"][1]["scaleX"] = -1;
    Json zero = old;
    zero["ik"][0]["aimAxis"] = 0;
    const Pose a = samplePose(parseRig("arm", old), -1, 0), b = samplePose(parseRig("arm", zero), -1, 0);
    bool same = a.world.size() == b.world.size();
    for (size_t i = 0; same && i < a.world.size(); i++)
      same = a.world[i].a == b.world[i].a && a.world[i].b == b.world[i].b && a.world[i].e == b.world[i].e && a.world[i].f == b.world[i].f && a.applied[i].rotation == b.applied[i].rotation;
    check(same, "no aimAxis == aimAxis 0, bit for bit");
    // Validation.
    auto code = [&](const Json& axis, bool twoBone = false) {
      Json x = twoBone ? armRig(60, 40) : hangingArm();
      if (!twoBone) x["ik"] = Json::array({{{"id", "aim"}, {"bones", {"arm"}}, {"target", "t"}}});
      x["ik"][0]["aimAxis"] = axis;
      return hasCode(validateRig(x), "ANIM_IK_AIM_AXIS");
    };
    check(code(400) && code(-360.5) && code("down") && code(90, true) && !code(360) && !code(-12.5), "range ±360, a number, aim chains only");
  });

  test("ik aim axis: addIk infers it and never moves the limb", [&] {
    auto worlds = [](const Json& d) { return samplePose(parseRig("arm", d), -1, 0).world; };
    auto unchanged = [&](const Json& before, const Json& after) {
      const auto a = worlds(before), b = worlds(after);
      for (size_t i = 0; i < a.size(); i++)
        // (aimAxis is stored rounded to 1e-6°: the solve may differ by that much, nothing visible)
        if (!(near(a[i].a, b[i].a, 1e-7) && near(a[i].b, b[i].b, 1e-7) && near(a[i].e, b[i].e, 1e-5) && near(a[i].f, b[i].f, 1e-5))) return false;
      return true;
    };
    // Hanging image, no child bone: axis = down (+y, 90°), target at the image's far end.
    Json base = hangingArm();
    auto r = applyEdit(base, {{"op", "addIk"}, {"bones", {"arm"}}, {"createTarget", true}});
    const Json& k = r.data["ik"][0];
    check(near(k["aimAxis"].get<double>(), 90, 1e-9), "hanging image: aimAxis 90 (" + k.dump() + ")");
    const auto setup = setupWorld(r.data);
    check(near(setup.at("arm-target").e, 230, 1e-6) && near(setup.at("arm-target").f, 200, 1e-6), "target at the image's far end");
    check(unchanged(base, r.data), "creating it moves nothing");
    // Dragging the target then turns the image's long axis onto it.
    Json moved = r.data;
    for (auto& b : moved["bones"]) if (b["id"] == "arm-target") { b["x"] = 150; b["y"] = 40; }
    {
      const Rig rr = parseRig("arm", moved);
      const Pose p = samplePose(rr, -1, 0);
      const Affine m = p.world[size_t(rr.boneIndex("arm"))] * attachmentPlacement(rr.attachments[0]);
      const auto& o = p.world[size_t(rr.boneIndex("arm"))];
      const auto& t = p.world[size_t(rr.boneIndex("arm-target"))];
      check(std::fabs(m.c * (t.f - o.f) - m.d * (t.e - o.e)) < 1e-6 && m.c * (t.e - o.e) + m.d * (t.f - o.f) > 0, "the image follows the target");
    }
    // A child bone (wrist) wins over the images; a centred image keeps the old +x behaviour.
    Json child = hangingArm();
    child["bones"].push_back({{"id", "hand"}, {"parent", "arm"}, {"x", -40}, {"y", 40}});
    r = applyEdit(child, {{"op", "addIk"}, {"bones", {"arm"}}, {"createTarget", true}});
    check(near(r.data["ik"][0]["aimAxis"].get<double>(), 135, 1e-9) && unchanged(child, r.data), "toward the first child bone");
    Json centred = hangingArm();
    centred["attachments"][0]["pivotY"] = 0.5;
    r = applyEdit(centred, {{"op", "addIk"}, {"bones", {"arm"}}, {"createTarget", true}});
    check(!r.data["ik"][0].contains("aimAxis") && unchanged(centred, r.data), "centred image: +x, no field (the 1.3.0 document)");
    check(near(setupWorld(r.data).at("arm-target").e, 290, 1e-6), "... target 60 px along +x as before");
    // Under a rotated, scaled parent and a scaled bone it still holds the pose.
    Json skew = hangingArm();
    skew["bones"][0]["rotation"] = -35; skew["bones"][0]["scaleX"] = 1.4; skew["bones"][0]["scaleY"] = 0.7;
    skew["bones"][1]["rotation"] = 12; skew["bones"][1]["scaleY"] = 1.3;
    skew["attachments"][0]["x"] = 6; skew["attachments"][0]["rotation"] = 20;
    r = applyEdit(skew, {{"op", "addIk"}, {"bones", {"arm"}}, {"createTarget", true}});
    check(r.data["ik"][0].contains("aimAxis") && unchanged(skew, r.data), "skewed rig: no jump");
    // Explicit axis, setIk, fit to the current direction.
    r = applyEdit(base, {{"op", "addIk"}, {"id", "aim"}, {"bones", {"arm"}}, {"createTarget", true}, {"aimAxis", 0}});
    check(!r.data["ik"][0].contains("aimAxis") && unchanged(base, r.data), "explicit +x: target on +x, nothing moves");
    check(refused(base, {{"op", "addIk"}, {"bones", {"arm"}}, {"createTarget", true}, {"aimAxis", 720}}), "addIk aimAxis range");
    Json far = r.data;
    for (auto& b : far["bones"]) if (b["id"] == "arm-target") { b["x"] = 30; b["y"] = 150; }  // straight below the shoulder
    r = applyEdit(far, {{"op", "setIk"}, {"ik", "aim"}, {"aimAxis", 45}});
    check(r.data["ik"][0]["aimAxis"] == 45, "setIk aimAxis");
    r = applyEdit(r.data, {{"op", "setIk"}, {"ik", "aim"}, {"aimAxis", nullptr}});
    check(!r.data["ik"][0].contains("aimAxis"), "setIk aimAxis null = +x");
    r = applyEdit(far, {{"op", "setIk"}, {"ik", "aim"}, {"fitAimAxis", true}});
    check(near(r.data["ik"][0]["aimAxis"].get<double>(), 90, 1e-9), "fit: the target is below the bone");
    {
      Json noIk = r.data;
      noIk.erase("ik");
      check(unchanged(noIk, r.data), "fit: the constraint holds the bone's own pose");
    }
    check(refused(far, {{"op", "setIk"}, {"ik", "aim"}, {"aimAxis", "down"}}) && refused(armRig(60, 40), {{"op", "setIk"}, {"ik", "arm"}, {"aimAxis", 30}}) &&
              refused(armRig(60, 40), {{"op", "setIk"}, {"ik", "arm"}, {"fitAimAxis", true}}) && refused(far, {{"op", "setIk"}, {"ik", "aim"}, {"fitAimAxis", true}, {"clip", "nope"}}),
          "setIk refusals: type, two-bone chains, unknown clip");
  });

  test("ik in clips, instances, frame-rate determinism, save/restore", [&] {
    const Json d = ikParityRig();
    const auto r = std::make_shared<const Rig>(parseRig("arm", d));
    const int clip = r->clipIndex("reach");
    Pose p = samplePose(*r, clip, 350);
    check(near(p.ikMix[0], 1 - 0.75 * 350.0 / 700) && p.ikBend[0] == 1, "mix keys sampled, bend before its key");
    check(samplePose(*r, clip, 650).ikBend[0] == 0, "bend step key");
    p = samplePose(*r, clip, 0);
    check(reaches(p, p.world[4].e, p.world[4].f, 1e-9), "full mix in the clip: the wrist follows the animated target");
    Look noIk;
    noIk.ik = false;
    const Pose fk = samplePose(*r, clip, 0, noIk);
    check(fk.applied.empty() && !near(fk.world[3].e, p.world[3].e), "look.ik = false: plain FK");
    Instance a(r, parseController("c", {{"schemaVersion", 1}, {"rig", "arm"}, {"defaultClip", "reach"}}));
    Instance b(r, parseController("c", {{"schemaVersion", 1}, {"rig", "arm"}, {"defaultClip", "reach"}}));
    for (int i = 0; i < 60; i++) a.update(10);
    for (int i = 0; i < 24; i++) b.update(25);
    const auto da = a.drawList(), db = b.drawList();
    bool same = da.size() == db.size();
    for (size_t i = 0; same && i < da.size(); i++) same = da[i].matrix.a == db[i].matrix.a && da[i].matrix.e == db[i].matrix.e && da[i].matrix.f == db[i].matrix.f;
    check(same, "60 x 10 ms == 24 x 25 ms: identical draw lists");
    const Pose ip = a.pose(), sp = samplePose(*r, clip, 600);
    check(near(ip.world[3].e, sp.world[3].e, 1e-9) && near(ip.world[2].f, sp.world[2].f, 1e-9), "Instance::pose == samplePose (IK after the expression layer)");
    const Json saved = a.saveState();
    check(!saved.contains("ik") && saved["tracks"].size() == 1, "IK adds no save state");
    Instance c(r, parseController("c", {{"schemaVersion", 1}, {"rig", "arm"}, {"defaultClip", "reach"}}));
    c.restoreState(saved);
    Instance fresh(r, parseController("c", {{"schemaVersion", 1}, {"rig", "arm"}, {"defaultClip", "reach"}}));
    check(c.drawList().size() == fresh.drawList().size() && near(c.drawList()[0].matrix.e, fresh.drawList()[0].matrix.e, 1e-9), "restore: looping clip restarts at 0, IK applied the same");
    const Pose x = samplePose(*r, -1, 0);
    check(x.ikMix.size() == 2 && x.ikMix[1] == 0.8 && x.ikBend[0] == 1, "setup values");
  });

  test("ik edit ops", [&] {
    Json base = armRig(60, 40);
    base.erase("ik");
    base["bones"][2]["rotation"] = 30;  // a bent elbow (clockwise on screen)
    const Pose before = ikPose(base);
    auto r = applyEdit(base, {{"op", "addIk"}, {"bones", {"upper", "lower"}}, {"createTarget", true}});
    const Json& k = r.data["ik"][0];
    check(k["id"] == "lower-ik" && k["target"] == "lower-target" && k["bendPositive"] == true && r.extra["target"] == "lower-target", "defaults: ids, inferred bend side");
    check(r.data["bones"].back()["id"] == "lower-target" && r.affected.size() == 2, "target bone created");
    const Pose after = ikPose(r.data);
    check(near(after.world[3].e, before.world[3].e, 1e-6) && near(after.world[3].f, before.world[3].f, 1e-6) && near(after.applied[2].rotation, 30, 1e-6), "new constraint holds the current pose");
    check(r.data["bones"].back()["parent"] == "root", "target under the chain's top ancestor");
    r = applyEdit(base, {{"op", "addIk"}, {"id", "aim"}, {"bones", {"upper"}}, {"createTarget", true}, {"targetId", "aim-at"}, {"mix", 0.5}});
    check(near(ikPose(r.data).applied[1].rotation, 0, 1e-6) && r.data["ik"][0]["mix"] == 0.5, "aim target on the x axis: no jump");
    check(refused(base, {{"op", "addIk"}, {"bones", {"upper", "hand"}}, {"createTarget", true}}), "non-adjacent chain refused");
    check(refused(base, {{"op", "addIk"}, {"bones", {"upper", "lower"}}, {"target", "hand"}}), "target below the chain refused");
    check(refused(base, {{"op", "addIk"}, {"bones", {"upper", "lower"}}}), "needs a target");
    Json d = armRig(60, 40);
    check(refused(d, {{"op", "addIk"}, {"id", "arm"}, {"bones", {"upper"}}, {"target", "t"}}), "duplicate id refused");
    r = applyEdit(d, {{"op", "setIk"}, {"ik", "arm"}, {"mix", 0.5}, {"bendPositive", false}, {"order", 3}});
    check(r.data["ik"][0]["mix"] == 0.5 && r.data["ik"][0]["bendPositive"] == false && r.data["ik"][0]["order"] == 3, "setIk");
    check(refused(d, {{"op", "setIk"}, {"ik", "arm"}, {"mix", 2}}) && refused(d, {{"op", "setIk"}, {"ik", "arm"}, {"target", "hand"}}) && refused(d, {{"op", "setIk"}, {"ik", "nope"}, {"mix", 1}}), "setIk refusals");
    // Keys.
    d["clips"] = Json::array({{{"id", "c"}, {"durationMs", 1000}, {"loop", false}, {"tracks", Json::array()}}});
    r = applyEdit(d, {{"op", "batch"}, {"operations", {
      {{"op", "setKey"}, {"clip", "c"}, {"target", "ik:arm"}, {"property", "mix"}, {"timeMs", 0}, {"value", 0}, {"interp", "easeIn"}},
      {{"op", "setKey"}, {"clip", "c"}, {"target", "ik:arm"}, {"property", "mix"}, {"timeMs", 500}, {"value", 1}},
      {{"op", "setKey"}, {"clip", "c"}, {"target", "ik:arm"}, {"property", "bendPositive"}, {"timeMs", 200}, {"value", false}, {"interp", "linear"}}}}});
    check(trackOf(r.data, "c", "ik:arm", "mix")[0]["interp"] == "bezier" && trackOf(r.data, "c", "ik:arm", "bendPositive") == Json::array({{{"timeMs", 200}, {"value", false}}}) && validateRig(r.data).empty(), "mix keys with curves, bend keys step");
    const Json keyed = r.data;
    check(refused(d, {{"op", "setKey"}, {"clip", "c"}, {"target", "ik:arm"}, {"property", "mix"}, {"timeMs", 0}, {"value", 1.2}}), "mix key range");
    check(refused(d, {{"op", "setKey"}, {"clip", "c"}, {"target", "ik:arm"}, {"property", "rotation"}, {"timeMs", 0}, {"value", 1}}), "ik keys mix/bend only");
    check(refused(keyed, {{"op", "setInterp"}, {"clip", "c"}, {"target", "ik:arm"}, {"property", "bendPositive"}, {"timeMs", 200}, {"interp", "linear"}}), "bend interp refused");
    const Json board = applyEdit(keyed, {{"op", "copyKeys"}, {"clip", "c"}, {"keys", Json::array({ref("ik:arm", "bendPositive", 200), ref("ik:arm", "mix", 500)})}}).extra["clipboard"];
    check(!board["keys"][0].contains("interp") && board["keys"][1]["interp"] == "linear", "copied bend keys carry no interp");
    // Rename / delete.
    r = applyEdit(keyed, {{"op", "renameIk"}, {"ik", "arm"}, {"id", "left-arm"}, {"name", "왼팔"}});
    check(r.data["ik"][0]["id"] == "left-arm" && r.data["ik"][0]["name"] == "왼팔" && trackOf(r.data, "c", "ik:left-arm", "mix").size() == 2 && r.extra["tracks"] == 2 && validateRig(r.data).empty(), "renameIk renames tracks");
    r = applyEdit(keyed, {{"op", "deleteIk"}, {"ik", "arm"}, {"deleteTarget", true}});
    check(!r.data.contains("ik") && r.data["clips"][0]["tracks"].empty() && r.extra["keys"] == 3 && r.extra["deletedTarget"] == "t" && validateRig(r.data).empty(), "deleteIk removes keys, list and the unused target");
    // Hierarchy edits keep constraints consistent.
    check(refused(keyed, {{"op", "reparentBone"}, {"bone", "lower"}, {"parent", "root"}}), "reparent breaking the chain refused");
    check(refused(keyed, {{"op", "reparentBone"}, {"bone", "t"}, {"parent", "lower"}}), "reparent of the target into the chain refused");
    check(validateRig(applyEdit(keyed, {{"op", "reparentBone"}, {"bone", "t"}, {"parent", nullptr}}).data).empty(), "harmless reparent of the target allowed");
    check(refused(keyed, {{"op", "deleteBone"}, {"bone", "t"}}), "deleting a target needs cascade");
    r = applyEdit(keyed, {{"op", "deleteBone"}, {"bone", "lower"}, {"cascade", true}});
    check(!r.data.contains("ik") && r.data["clips"][0]["tracks"].empty() && hasCode(r.diagnostics, "ANIM_IK_REMOVED") && validateRig(r.data).empty(), "cascade removes the constraint and its keys");
    r = applyEdit(d, {{"op", "setPivot"}, {"bone", "lower"}, {"localX", 10}, {"localY", 0}});
    check(validateRig(r.data).empty() && reaches(ikPose(r.data), 60, 40, 1e-9), "setPivot keeps refs; still reaches");
    // Bake: the solved rotations become keys (FK pose kept with mix 0).
    r = applyEdit(keyed, {{"op", "bakeIk"}, {"clip", "c"}, {"timeMs", 700}, {"disable", true}});
    const Pose solved = ikPose(keyed, 0, 700), baked = ikPose(r.data, 0, 700);
    check(near(baked.bones[1].rotation, solved.applied[1].rotation, 1e-5) && near(baked.world[3].e, solved.world[3].e, 1e-3) && near(baked.world[3].f, solved.world[3].f, 1e-3) && baked.ikMix[0] == 0, "bakeIk keys the solved pose");
    check(r.extra["keys"].size() == 3, "two rotations + mix 0");
    Host host;
    const Json op = {{"op", "addIk"}, {"bones", {"upper", "lower"}}, {"createTarget", true}};
    check(host.handle({{"op", "edit"}, {"rig", base}, {"operation", op}})["rig"] == applyEdit(base, op).data, "host edit = core edit");
  });

  test("ik host sample reply", [&] {
    Host host;
    const auto reply = host.handle({{"op", "sample"}, {"rig", ikParityRig()}, {"clip", "reach"}, {"timeMs", 650}});
    check(reply["pose"]["ik"]["arm"]["bendPositive"] == false && near(reply["pose"]["ik"]["look"]["mix"].get<double>(), 0.8), "constraint values in the pose");
    const Rig r = parseRig("arm", ikParityRig());
    const Pose p = samplePose(r, r.clipIndex("reach"), 650);
    check(reply["pose"]["bones"]["upper"]["appliedRotation"].get<double>() == p.applied[1].rotation && reply["pose"]["bones"]["upper"]["world"][4].get<double>() == p.world[1].e, "solved rotation and world");
    check(!host.handle({{"op", "sample"}, {"rig", rigData()}, {"timeMs", 0}})["pose"].contains("ik"), "rigs without IK keep the 1.2 shape");
  });

  test("host sample op evaluates bezier like samplePose", [] {
    Host host;
    const auto reply = host.handle({{"op", "sample"}, {"rig", bezierRig()}, {"clip", "ease"}, {"timeMs", 300}});
    const Rig r = parseRig("b", bezierRig());
    check(reply.value("ok", false) && reply["pose"]["bones"]["body"]["y"].get<double>() == samplePose(r, r.clipIndex("ease"), 300).bones[1].y, "same number through the JSON host");
  });

  // ---- Explicit IK tip (plugin 1.4.0) ----
  test("ik explicit tip picks the effector among several children", [&] {
    // A thumb bone listed before the hand: the implicit first-child rule would aim the thumb.
    Json d = armRig(60, 40);
    d["bones"].insert(d["bones"].begin() + 3, Json{{"id", "thumb"}, {"parent", "lower"}, {"x", 20}, {"y", 15}});
    auto origin = [&](const Json& data, const char* id) {
      const Rig r = parseRig("arm", data);
      const Pose p = samplePose(r, -1, 0);
      return std::pair<double, double>{p.world[size_t(r.boneIndex(id))].e, p.world[size_t(r.boneIndex(id))].f};
    };
    check(validateRig(d).empty() && parseRig("arm", d).ik[0].tip == 3 && !parseRig("arm", d).ik[0].explicitTip, "implicit: first child (thumb)");
    check(near(origin(d, "thumb").first, 60, 1e-9) && near(origin(d, "thumb").second, 40, 1e-9), "implicit tip reaches with the thumb");
    d["ik"][0]["tip"] = "hand";
    check(validateRig(d).empty() && parseRig("arm", d).ik[0].tip == 4 && parseRig("arm", d).ik[0].explicitTip, "explicit tip parsed");
    check(near(origin(d, "hand").first, 60, 1e-9) && near(origin(d, "hand").second, 40, 1e-9), "explicit tip: the hand reaches");
    auto bad = [&](const Json& tip, bool aim = false) {
      Json x = d;
      x["ik"][0]["tip"] = tip;
      if (aim) x["ik"][0]["bones"] = {"lower"};
      return hasCode(validateRig(x), "ANIM_IK_TIP");
    };
    check(bad("nobody") && bad("upper") && bad("t") && bad(7) && bad("hand", true), "unknown, not a child of the lower bone, aim chain");
    // Editing: addIk names the known end, setIk can change or clear it, deleting it falls back.
    Json plain = d;
    plain.erase("ik");
    auto r = applyEdit(plain, {{"op", "addIk"}, {"bones", {"upper", "lower"}}, {"createTarget", true}});
    check(r.data["ik"][0]["tip"] == "thumb", "addIk stores the end it placed the target at");
    r = applyEdit(plain, {{"op", "addIk"}, {"bones", {"upper", "lower"}}, {"tip", "hand"}, {"createTarget", true}});
    check(r.data["ik"][0]["tip"] == "hand" && validateRig(r.data).empty(), "addIk with an explicit tip");
    const Pose made = samplePose(parseRig("x", r.data), -1, 0);
    const Rig mr = parseRig("x", r.data);
    check(near(made.world[size_t(mr.boneIndex("hand"))].e, made.world[size_t(mr.boneIndex(r.extra["target"].get<std::string>()))].e, 1e-6), "target created at the explicit tip");
    check(refused(plain, {{"op", "addIk"}, {"bones", {"upper", "lower"}}, {"tip", "upper"}, {"createTarget", true}}) &&
          refused(plain, {{"op", "addIk"}, {"bones", {"lower"}}, {"tip", "hand"}, {"createTarget", true}}), "addIk tip refusals");
    check(!applyEdit(d, {{"op", "setIk"}, {"ik", "arm"}, {"tip", nullptr}}).data["ik"][0].contains("tip"), "setIk tip null = implicit rule");
    check(applyEdit(d, {{"op", "setIk"}, {"ik", "arm"}, {"tip", "thumb"}}).data["ik"][0]["tip"] == "thumb" && refused(d, {{"op", "setIk"}, {"ik", "arm"}, {"tip", "root"}}), "setIk tip");
    r = applyEdit(d, {{"op", "deleteBone"}, {"bone", "hand"}, {"cascade", true}});
    check(!r.data["ik"][0].contains("tip") && hasCode(r.diagnostics, "ANIM_IK_TIP_REMOVED") && validateRig(r.data).empty(), "deleting the tip falls back to the implicit rule");
    check(refused(d, {{"op", "reparentBone"}, {"bone", "hand"}, {"parent", "root"}}), "reparenting the tip away is refused");
  });

  // ---- Mesh attachments and weights (plugin 1.4.0) ----
  auto meshItem = [](const std::vector<DrawItem>& items, const char* slot) {
    for (const auto& i : items) if (i.slot == slot) return i;
    return DrawItem{};
  };
  auto vertexAt = [](const Json& d, const char* slot, size_t v) {
    const Rig r = parseRig("m", d);
    const Pose p = samplePose(r, -1, 0);
    const int s = r.slotIndex(slot);
    const auto pos = meshPositions(r, p, s, *r.attachment(slot, *p.attachments[size_t(s)]));
    return std::pair<double, double>{pos[2 * v], pos[2 * v + 1]};
  };
  test("mesh validation", [&] {
    check(validateRig(weightedArm()).empty(), "weighted arm validates");
    check(validateRig(meshParityRig()).empty(), "parity fixture validates");
    auto bad = [&](const std::function<void(Json&)>& edit, const std::string& code, const char* severity = "error") {
      Json d = weightedArm();
      edit(d["attachments"][0]);
      for (const auto& x : validateRig(d)) if (x.code == code && x.severity == severity) return true;
      return false;
    };
    check(bad([](Json& a) { a["type"] = "bone"; }, "ANIM_MESH_TYPE"), "unknown type");
    check(bad([](Json& a) { a["vertices"].push_back(1); }, "ANIM_MESH_VERTICES"), "odd coordinate count");
    check(bad([](Json& a) { a["vertices"] = {0, 0, 1, 1}; }, "ANIM_MESH_VERTICES"), "fewer than 3 vertices");
    check(bad([](Json& a) { a["vertices"] = Json(std::vector<double>(2 * (MAX_MESH_VERTICES + 1), 0.0)); }, "ANIM_MESH_VERTICES"), "vertex limit");
    check(bad([](Json& a) { a["uvs"].erase(a["uvs"].begin()); }, "ANIM_MESH_UVS"), "uv count");
    check(bad([](Json& a) { a["uvs"][3] = 1.5; }, "ANIM_MESH_UVS"), "uv range");
    check(bad([](Json& a) { a["triangles"].push_back(0); }, "ANIM_MESH_TRIANGLES"), "triangle triples");
    check(bad([](Json& a) { a["triangles"][4] = 10; }, "ANIM_MESH_TRIANGLES"), "index out of range");
    check(bad([](Json& a) { a["triangles"] = Json(std::vector<int>(3 * (MAX_MESH_TRIANGLES + 1), 0)); }, "ANIM_MESH_TRIANGLES"), "triangle limit");
    check(bad([](Json& a) { a.erase("triangles"); }, "ANIM_MESH_TRIANGLES"), "triangles required");
    check(bad([](Json& a) { a["hull"] = 99; }, "ANIM_MESH_HULL") && bad([](Json& a) { a["edges"] = {0, 42}; }, "ANIM_MESH_EDGES"), "hull and edges");
    check(bad([](Json& a) { a["weights"].erase(a["weights"].begin()); }, "ANIM_MESH_WEIGHTS"), "weights per vertex");
    check(bad([](Json& a) { a["weights"][0][0]["bone"] = "tail"; }, "ANIM_MESH_BONE_MISSING"), "unknown weight bone");
    check(bad([](Json& a) { a["weights"][0] = Json::array({{{"bone", "root"}, {"weight", 0.2}}, {{"bone", "upper"}, {"weight", 0.2}}, {{"bone", "lower"}, {"weight", 0.2}},
                                                            {{"bone", "hand"}, {"weight", 0.2}}, {{"bone", "t"}, {"weight", 0.2}}}); }, "ANIM_MESH_INFLUENCES"), "max 4 influences");
    check(bad([](Json& a) { a["weights"][0] = Json::array({{{"bone", "upper"}, {"weight", 0.5}}, {{"bone", "upper"}, {"weight", 0.5}}}); }, "ANIM_MESH_WEIGHTS"), "duplicate bone");
    check(bad([](Json& a) { a["weights"][0][0]["weight"] = 0; }, "ANIM_MESH_WEIGHTS"), "zero sum");
    check(bad([](Json& a) { a["weights"][0][0]["weight"] = 0.5; }, "ANIM_MESH_WEIGHT_SUM", "warning"), "sum off: warning");
    {
      Json d = weightedArm();
      d["attachments"][0]["weights"][0][0]["weight"] = 0.5;  // normalized at load: still 100 % upper
      const Rig r = parseRig("m", d);
      check(near(r.attachments[0].weights[0][0].weight, 1, 1e-12), "normalized at load");
    }
    Json region = meshArm();
    region["attachments"][1]["vertices"] = {0, 0, 1, 1, 2, 2};
    check(hasCode(validateRig(region), "ANIM_MESH_UNUSED") && !hasCode(validateRig(meshArm()), "ANIM_MESH_UNUSED"), "mesh fields on a region warn");
    // Deform tracks.
    auto deform = [&](const Json& track, const std::string& code) {
      Json d = weightedArm();
      d["clips"] = Json::array({{{"id", "c"}, {"durationMs", 1000}, {"loop", false}, {"tracks", Json::array({track})}}});
      return hasCode(validateRig(d), code);
    };
    const Json ten = std::vector<double>(20, 1.0);
    check(!deform({{"target", "slot:arm"}, {"property", "deform"}, {"attachment", "arm"}, {"keys", Json::array({{{"timeMs", 0}, {"offsets", ten}}})}}, "ANIM_MESH_DEFORM"), "valid deform track");
    check(deform({{"target", "slot:arm"}, {"property", "deform"}, {"keys", Json::array({{{"timeMs", 0}, {"offsets", ten}}})}}, "ANIM_MESH_DEFORM"), "attachment required");
    check(deform({{"target", "slot:flag"}, {"property", "deform"}, {"attachment", "flag"}, {"keys", Json::array({{{"timeMs", 0}, {"offsets", ten}}})}}, "ANIM_MESH_DEFORM"), "region cannot deform");
    check(deform({{"target", "slot:arm"}, {"property", "deform"}, {"attachment", "arm"}, {"keys", Json::array({{{"timeMs", 0}, {"offsets", {1, 2}}}})}}, "ANIM_MESH_DEFORM"), "offset count");
    check(deform({{"target", "slot:arm"}, {"property", "deform"}, {"attachment", "arm"}, {"keys", Json::array({{{"timeMs", 0}, {"value", 1}}})}}, "ANIM_MESH_DEFORM"), "offsets required");
    check(deform({{"target", "bone:upper"}, {"property", "deform"}, {"attachment", "arm"}, {"keys", Json::array({{{"timeMs", 0}, {"offsets", ten}}})}}, "ANIM_PROPERTY"), "deform only on slots");
  });

  test("mesh: a grid on one bone draws exactly like the region", [&] {
    Json d = applyEdit(rigData(), {{"op", "convertToMesh"}, {"slot", "torso"}, {"rows", 3}, {"cols", 2}}).data;
    check(validateRig(d).empty(), "converted rig validates");
    const Json& a = d["attachments"][0];
    check(a["type"] == "mesh" && a["vertices"].size() == 24 && a["triangles"].size() == 36 && a["hull"] == 10 && a["edges"].size() == 20, "3x2 grid: 12 vertices, 12 triangles, outline first");
    // Documents round to 1e-6 (thirds): the texture lands within 1e-4 px of the region's pixels.
    for (size_t i = 0; i < 12; i++) check(near(a["uvs"][2 * i].get<double>() * 80, a["vertices"][2 * i].get<double>(), 1e-4) && near(a["uvs"][2 * i + 1].get<double>() * 120, a["vertices"][2 * i + 1].get<double>(), 1e-4), "uv * size = vertex");
    const Json bound = applyEdit(d, {{"op", "bindMesh"}, {"slot", "torso"}, {"bones", {"body"}}}).data;
    check(validateRig(bound).empty() && bound["attachments"][0]["weights"][5] == Json::array({{{"bone", "body"}, {"weight", 1}}}), "bound 100 % to the slot bone");
    for (const Json* data : {static_cast<const Json*>(&d), &bound}) {
      const auto r = std::make_shared<const Rig>(parseRig("hero", *data));
      Instance inst(r, parseController("hero-main", controllerData()));
      inst.update(250);  // idle moves the body; joy below rotates the head and scales the body
      inst.play(0, "joy", std::nullopt, 1, Then::Hold, 0);
      inst.update(300);
      const DrawItem item = meshItem(inst.drawList(), "torso");
      check(item.mesh() && item.positions.size() == 24 && item.uvs == r->attachments[0].uvs && item.indices == r->attachments[0].triangles, "mesh item fields");
      bool same = true;
      for (size_t v = 0; v < 12; v++) {
        double x, y;
        item.matrix.apply(r->attachments[0].vertices[2 * v], r->attachments[0].vertices[2 * v + 1], x, y);
        same = same && near(item.positions[2 * v], x, 1e-9) && near(item.positions[2 * v + 1], y, 1e-9);
      }
      check(same, data == &d ? "unweighted mesh = region matrix" : "weighted (one bone) mesh = region matrix");
    }
  });

  test("mesh: two-bone 50/50 bend and IK skinning are analytic", [&] {
    Json d = weightedArm();
    check(near(vertexAt(d, "arm", 2).first, 200) && near(vertexAt(d, "arm", 2).second, 130) && near(vertexAt(d, "arm", 4).first, 300), "setup pose = bind pose");
    d["clips"] = Json::array({{{"id", "bend"}, {"durationMs", 100}, {"loop", false}, {"tracks", Json::array({
      {{"target", "bone:lower"}, {"property", "rotation"}, {"keys", Json::array({{{"timeMs", 0}, {"value", 90}}})}}})}}});
    const Rig r = parseRig("m", d);
    const Pose p = samplePose(r, 0, 0);
    const auto pos = meshPositions(r, p, 0, r.attachments[0]);
    // Elbow vertex (image 100,0) = setup world (200,130): 50 % upper (unmoved) + 50 % lower
    // (rotated 90° clockwise about the elbow at (200,150): (220,150)) = (210,140).
    check(near(pos[4], 210, 1e-9) && near(pos[5], 140, 1e-9), "50/50 vertex halfway between the two bones' motion");
    check(near(pos[8], 220, 1e-9) && near(pos[9], 250, 1e-9), "lower vertex (image 200,0) turned with the forearm");
    check(near(pos[0], 100, 1e-9) && near(pos[1], 130, 1e-9), "upper vertex unmoved");
    const Affine m = meshVertexMatrix(r, p, 0, r.attachments[0], 2);
    double mx, my;
    m.apply(100, 0, mx, my);
    check(near(mx, 210, 1e-9) && near(my, 140, 1e-9), "vertex matrix maps the vertex like skinning");
    // IK: the forearm-weighted vertex at the wrist edge follows the solved world (after IK).
    Json ik = weightedArm();
    ik["bones"][4]["x"] = 120; ik["bones"][4]["y"] = 90;
    ik["ik"] = Json::array({{{"id", "arm"}, {"bones", {"upper", "lower"}}, {"target", "t"}, {"tip", "hand"}}});
    const Rig ri = parseRig("m", ik);
    const Pose pi = samplePose(ri, -1, 0);
    check(near(pi.world[3].e, 220, 1e-9) && near(pi.world[3].f, 240, 1e-9), "IK puts the wrist on the target");
    const auto ipos = meshPositions(ri, pi, 0, ri.attachments[0]);
    double ex, ey;
    pi.world[2].apply(100, -20, ex, ey);  // vertex 4 in the forearm's setup space
    check(near(ipos[8], ex, 1e-9) && near(ipos[9], ey, 1e-9), "weighted vertex skinned with the solved forearm");
    Look noIk; noIk.ik = false;
    const auto fk = meshPositions(ri, samplePose(ri, -1, 0, noIk), 0, ri.attachments[0]);
    check(!near(fk[8], ipos[8], 1) && near(fk[8], 300, 1e-9), "without IK the mesh stays in the bind pose");
  });

  test("mesh deform keys interpolate (step, linear, bezier) and cross-fade", [&] {
    Json d = applyEdit(meshArm(), {{"op", "convertToMesh"}, {"slot", "flag"}, {"rows", 1}, {"cols", 1}}).data;
    d["clips"] = Json::array({{{"id", "flap"}, {"durationMs", 1000}, {"loop", false}, {"tracks", Json::array()}}, {{"id", "rest"}, {"durationMs", 1000}, {"loop", true}, {"tracks", Json::array()}}});
    std::vector<double> up(8, 0.0);
    up[0] = 10; up[1] = -8;
    auto r = applyEdit(d, {{"op", "batch"}, {"operations", {
      {{"op", "setKey"}, {"clip", "flap"}, {"target", "slot:flag"}, {"property", "deform"}, {"attachment", "flag"}, {"timeMs", 0}, {"interp", "easeInOut"}},
      {{"op", "setKey"}, {"clip", "flap"}, {"target", "slot:flag"}, {"property", "deform"}, {"attachment", "flag"}, {"timeMs", 600}, {"offsets", up}, {"interp", "step"}},
      {{"op", "setKey"}, {"clip", "flap"}, {"target", "slot:flag"}, {"property", "deform"}, {"attachment", "flag"}, {"timeMs", 800}}}}});
    check(validateRig(r.data).empty(), "deform keys validate");
    const Json keys = r.data["clips"][0]["tracks"][0]["keys"];
    check(r.data["clips"][0]["tracks"][0]["attachment"] == "flag" && keys[0]["offsets"] == Json(std::vector<double>(8, 0.0)) && keys[0]["interp"] == "bezier" && !keys[0].contains("value"), "rest key (no offsets) stored as zeros with a curve");
    const Rig rig = parseRig("m", r.data);
    const Attachment& flag = *rig.attachment("flag", "flag");
    auto at = [&](double t) { return samplePose(rig, 0, t).deform.at({1, "flag"}); };
    check(near(at(300)[0], 10 * evalCurve(presetCurve("easeInOut").value(), 0.5)) && near(at(300)[1], -8 * evalCurve(presetCurve("easeInOut").value(), 0.5)), "bezier segment eases every vertex by one curve");
    check(at(700)[0] == 10 && at(799)[1] == -8 && at(800)[0] == 0 && at(1000)[0] == 0 && at(0)[0] == 0, "step holds, last key holds");
    const Pose p = samplePose(rig, 0, 300);
    const auto pos = meshPositions(rig, p, 1, flag), rest = meshPositions(rig, samplePose(rig, -1, 0), 1, flag);
    check(near(pos[0] - rest[0], at(300)[0], 1e-9) && near(pos[1] - rest[1], at(300)[1], 1e-9) && near(pos[2], rest[2], 1e-12), "offsets move the vertex in image space (hand unrotated)");
    check(deformOf(p, 1, flag) && !deformOf(p, 0, flag), "deform lookup by (slot, attachment)");
    // Cross-fade: rest (no deform) mixed in over 200 ms halves the offsets at 100 ms.
    const auto shared = std::make_shared<const Rig>(rig);
    Instance inst(shared);
    inst.play(0, "flap", std::nullopt, 1, Then::Hold, 0);
    inst.update(650);
    check(near(inst.pose().deform.at({1, "flag"})[0], 10), "playing flap");
    inst.play(0, "rest", std::nullopt, 1, Then::Default, 200);
    inst.update(100);
    check(near(inst.pose().deform.at({1, "flag"})[0], 5, 1e-9), "cross-fade lerps offsets towards none");
    // Editing refusals and multi-key ops keep the attachment.
    check(refused(r.data, {{"op", "setKey"}, {"clip", "flap"}, {"target", "slot:flag"}, {"property", "deform"}, {"timeMs", 0}}), "attachment required");
    check(refused(r.data, {{"op", "setKey"}, {"clip", "flap"}, {"target", "slot:arm"}, {"property", "deform"}, {"attachment", "arm"}, {"timeMs", 0}}), "regions refuse deform keys");
    check(refused(r.data, {{"op", "setKey"}, {"clip", "flap"}, {"target", "slot:flag"}, {"property", "deform"}, {"attachment", "flag"}, {"timeMs", 0}, {"offsets", {1, 2}}}), "offset count refused");
    Json sel = Json::array({{{"target", "slot:flag"}, {"property", "deform"}, {"attachment", "flag"}, {"timeMs", 600}}});
    auto moved = applyEdit(r.data, {{"op", "moveKeys"}, {"clip", "flap"}, {"keys", sel}, {"deltaMs", 50}});
    check(moved.data["clips"][0]["tracks"][0]["keys"][1]["timeMs"] == 650 && moved.extra["keys"][0]["attachment"] == "flag", "moveKeys with deform refs");
    const Json board = applyEdit(r.data, {{"op", "copyKeys"}, {"clip", "flap"}, {"keys", sel}}).extra["clipboard"];
    check(board["keys"][0]["offsets"] == Json(up) && board["keys"][0]["attachment"] == "flag" && !board["keys"][0].contains("value"), "copied deform keys carry offsets");
    auto pasted = applyEdit(r.data, {{"op", "pasteKeys"}, {"clip", "rest"}, {"timeMs", 100}, {"keys", board["keys"]}});
    check(pasted.data["clips"][1]["tracks"][0]["keys"][0]["offsets"] == Json(up) && validateRig(pasted.data).empty(), "pasted into another clip");
    auto deleted = applyEdit(r.data, {{"op", "deleteKey"}, {"clip", "flap"}, {"target", "slot:flag"}, {"property", "deform"}, {"attachment", "flag"}, {"timeMs", 800}});
    check(deleted.data["clips"][0]["tracks"][0]["keys"].size() == 2, "deleteKey by attachment");
    // Re-meshing changes the vertex count: the old deform keys go (warning), the rig stays valid.
    auto remeshed = applyEdit(r.data, {{"op", "convertToMesh"}, {"slot", "flag"}, {"rows", 2}, {"cols", 2}});
    check(remeshed.data["clips"][0]["tracks"].empty() && remeshed.extra["keys"] == 3 && hasCode(remeshed.diagnostics, "ANIM_MESH_DEFORM_REMOVED"), "re-mesh drops stale deform keys");
    auto region = applyEdit(r.data, {{"op", "convertToRegion"}, {"slot", "flag"}});
    check(!region.data["attachments"][1].contains("type") && !region.data["attachments"][1].contains("vertices") && region.data["clips"][0]["tracks"].empty(), "convertToRegion");
  });

  test("mesh edit ops: grid trim, vertices, auto weights, brush, bind", [&] {
    // Alpha-trimmed grid: only the marked cells (an L shape) are meshed.
    auto r = applyEdit(meshArm(), {{"op", "convertToMesh"}, {"slot", "arm"}, {"rows", 2}, {"cols", 2}, {"cells", "1011"}});
    const Json& a = r.data["attachments"][0];
    check(a["vertices"].size() == 16 && a["triangles"].size() == 18 && !a.contains("hull") && validateRig(r.data).empty(), "3 cells: 8 vertices, 6 triangles");
    check(refused(meshArm(), {{"op", "convertToMesh"}, {"slot", "arm"}, {"rows", 2}, {"cols", 2}, {"cells", "10"}}) &&
          refused(meshArm(), {{"op", "convertToMesh"}, {"slot", "arm"}, {"rows", 2}, {"cols", 2}, {"cells", "0000"}}) &&
          refused(meshArm(), {{"op", "convertToMesh"}, {"slot", "arm"}, {"rows", 0}}) && refused(meshArm(), {{"op", "convertToMesh"}, {"slot", "arm"}, {"rows", 64}, {"cols", 64}}), "grid refusals");
    // setMeshVertices: uvs follow by default, keepUvs stretches.
    Json d = weightedArm();
    r = applyEdit(d, {{"op", "setMeshVertices"}, {"slot", "arm"}, {"vertices", {1}}, {"positions", {60, -4}}});
    check(r.data["attachments"][0]["vertices"][2] == 60 && r.data["attachments"][0]["vertices"][3] == -4 && r.data["attachments"][0]["uvs"][2] == 0.3 && r.data["attachments"][0]["uvs"][3] == 0, "uvs follow (clamped)");
    r = applyEdit(d, {{"op", "setMeshVertices"}, {"slot", "arm"}, {"vertices", {1, 2}}, {"dx", 5}, {"dy", 1}, {"keepUvs", true}});
    check(r.data["attachments"][0]["vertices"][2] == 55 && r.data["attachments"][0]["vertices"][5] == 1 && r.data["attachments"][0]["uvs"] == d["attachments"][0]["uvs"], "dx/dy with fixed uvs");
    check(refused(d, {{"op", "setMeshVertices"}, {"slot", "arm"}, {"vertices", {99}}, {"dx", 1}}) && refused(d, {{"op", "setMeshVertices"}, {"slot", "flag"}, {"dx", 1}}), "out of range / region refused");
    // Auto weights: shoulder vertices on upper, the forearm end on lower, the elbow shared.
    Json plain = applyEdit(meshArm(), {{"op", "convertToMesh"}, {"slot", "arm"}, {"rows", 1}, {"cols", 4}}).data;
    r = applyEdit(plain, {{"op", "bindMesh"}, {"slot", "arm"}, {"bones", {"upper", "lower"}}});
    const Json& w = r.data["attachments"][0]["weights"];
    auto share = [&](size_t v, const char* b) { for (const auto& x : w[v]) if (x["bone"] == b) return x["weight"].get<double>(); return 0.0; };
    check(validateRig(r.data).empty() && w.size() == 10, "bindMesh writes one list per vertex");
    // Vertex order of the full 1x4 grid: top row x = 0,50,100,150,200 then bottom 200..0.
    check(share(0, "upper") > 0.9 && share(4, "lower") > 0.9 && share(1, "upper") > share(1, "lower") && share(3, "lower") > share(3, "upper"), "nearest bone dominates");
    check(near(share(2, "upper") + share(2, "lower"), 1, 1e-6) && share(2, "lower") > 0.3, "elbow vertex shared");
    for (const auto& list : w) { double s = 0; for (const auto& x : list) s += x["weight"].get<double>(); check(near(s, 1, 1e-9) && list.size() <= 2, "each vertex sums to exactly 1"); }
    check(refused(plain, {{"op", "bindMesh"}, {"slot", "arm"}}) && refused(plain, {{"op", "autoWeights"}, {"slot", "arm"}, {"bones", {"upper"}}, {"influences", 5}}), "bindMesh needs bones; influences 1..4");
    auto one = applyEdit(plain, {{"op", "autoWeights"}, {"slot", "arm"}, {"bones", {"upper", "lower", "hand"}}, {"influences", 1}});
    bool single = true;
    for (const auto& list : one.data["attachments"][0]["weights"]) single = single && list.size() == 1 && list[0]["weight"] == 1;
    check(single, "influences 1 = hard binding");
    auto dflt = applyEdit(plain, {{"op", "autoWeights"}, {"slot", "arm"}});
    check(dflt.extra["bones"] == Json::array({"upper", "root", "lower"}) && validateRig(dflt.data).empty(), "default bones: slot bone, parent, children");
    // Weight brush: paint the elbow fully onto lower, then half way back with strength.
    r = applyEdit(d, {{"op", "setWeights"}, {"slot", "arm"}, {"vertices", {2}}, {"bone", "lower"}, {"weight", 1}});
    check(r.data["attachments"][0]["weights"][2] == Json::array({{{"bone", "lower"}, {"weight", 1}}}), "brush weight 1 replaces the others");
    r = applyEdit(r.data, {{"op", "setWeights"}, {"slot", "arm"}, {"vertices", {2}}, {"bone", "lower"}, {"weight", 0}, {"strength", 0.5}});
    check(r.data["attachments"][0]["weights"][2] == Json::array({{{"bone", "lower"}, {"weight", 0.5}}, {{"bone", "upper"}, {"weight", 0.5}}}), "strength 0.5 towards 0: the rest goes to the slot bone");
    r = applyEdit(meshArm(), {{"op", "convertToMesh"}, {"slot", "flag"}, {"rows", 1}, {"cols", 1}});
    r = applyEdit(r.data, {{"op", "setWeights"}, {"slot", "flag"}, {"vertices", {0, 1}}, {"bone", "lower"}, {"weight", 0.25}});
    check(r.data["attachments"][1]["weights"][0] == Json::array({{{"bone", "hand"}, {"weight", 0.75}}, {{"bone", "lower"}, {"weight", 0.25}}}) && r.data["attachments"][1]["weights"][3] == Json::array({{{"bone", "hand"}, {"weight", 1}}}),
          "painting an unweighted mesh starts from 100 % slot bone");
    check(validateRig(r.data).empty(), "painted rig validates");
    check(near(vertexAt(r.data, "flag", 0).first, vertexAt(applyEdit(meshArm(), {{"op", "convertToMesh"}, {"slot", "flag"}, {"rows", 1}, {"cols", 1}}).data, "flag", 0).first, 1e-9), "binding never moves the setup shape");
    check(refused(d, {{"op", "setWeights"}, {"slot", "arm"}, {"vertices", {0}}, {"bone", "tail"}, {"weight", 1}}) && refused(d, {{"op", "setWeights"}, {"slot", "arm"}, {"bone", "upper"}, {"weight", 1}}) &&
          refused(d, {{"op", "setWeights"}, {"slot", "arm"}, {"vertices", {0}}, {"weights", Json::array({Json::array({{{"bone", "upper"}, {"weight", 0}}})})}}), "brush refusals");
    // Unbind / skins / bone deletion.
    check(!applyEdit(d, {{"op", "unbindMesh"}, {"slot", "arm"}}).data["attachments"][0].contains("weights"), "unbindMesh");
    Json skinned = meshArm();
    skinned["skins"] = Json::array({{{"id", "gold"}, {"attachments", Json::array({skinned["attachments"][0]})}}});
    r = applyEdit(skinned, {{"op", "convertToMesh"}, {"slot", "arm"}, {"skin", "gold"}, {"rows", 1}, {"cols", 2}});
    check(r.data["skins"][0]["attachments"][0]["type"] == "mesh" && !r.data["attachments"][0].contains("type") && validateRig(r.data).empty(), "a skin's attachment becomes a mesh on its own");
    r = applyEdit(d, {{"op", "deleteBone"}, {"bone", "lower"}, {"cascade", true}});
    bool off = true;
    for (const auto& list : r.data["attachments"][0]["weights"]) for (const auto& x : list) off = off && x["bone"] != "lower";
    check(off && hasCode(r.diagnostics, "ANIM_MESH_WEIGHTS_MOVED") && validateRig(r.data).empty() && r.data["attachments"][0]["weights"][4] == Json::array({{{"bone", "upper"}, {"weight", 1}}}), "deleteBone moves weights to the remaining / slot bone");
    Host host;
    const Json op = {{"op", "bindMesh"}, {"slot", "arm"}, {"bones", {"upper", "lower"}}};
    check(host.handle({{"op", "edit"}, {"rig", plain}, {"operation", op}})["rig"] == applyEdit(plain, op).data, "host edit = core edit");
  });

  test("host frame emits mesh items in element pixels", [&] {
    Host host;
    const Json controller = {{"schemaVersion", 1}, {"rig", "arm"}, {"defaultClip", "wave"}};
    check(host.handle({{"op", "init"}, {"documents", Json::array({{{"type", "rig"}, {"id", "arm"}, {"data", meshParityRig()}}, {{"type", "controller"}, {"id", "c"}, {"data", controller}}})}})["ok"] == true, "init");
    check(host.handle({{"op", "mount"}, {"instance", "a"}, {"element", {{"type", "character"}, {"ref", "c"}, {"width", 800}, {"height", 600}, {"props", Json::object()}}}})["ok"] == true, "mount");
    const Json items = host.handle({{"op", "frame"}, {"instance", "a"}})["items"];
    check(items.size() == 2 && items[0].contains("mesh") && items[0]["mesh"]["positions"].size() == 20 && items[0]["mesh"]["indices"].size() == 24 && items[0].contains("matrix"), "mesh + fallback matrix");
    // 400x300 rig contain-fit into 800x600 = scale 2, rig origin at (400, 500): x' = 2 * (x - 200) + 400.
    const Rig r = parseRig("arm", meshParityRig());
    const auto pos = meshPositions(r, samplePose(r, 0, 0), 0, r.attachments[0]);
    check(near(items[0]["mesh"]["positions"][0].get<double>(), 2 * pos[0], 1e-9) && near(items[0]["mesh"]["positions"][1].get<double>(), 2 * pos[1], 1e-9), "placement applied to positions");
    const Json sample = host.handle({{"op", "sample"}, {"rig", meshParityRig()}, {"clip", "wave"}, {"timeMs", 550}});
    check(sample["pose"]["deform"]["flag/flag"].size() == 18 && !host.handle({{"op", "sample"}, {"rig", rigData()}, {"timeMs", 0}})["pose"].contains("deform"), "sample reports deform only when keyed");
  });

  // ---- Bone length, drawn bones, orientation (plugin 1.5.0) ----
  // Largest difference between the draw lists of `a` and `b` (matrices and mesh positions) over the
  // setup pose, every clip alone at many times, pairs of clips layered (track 0 + 1), cross-fades,
  // and every skin x expression: the "nothing moves" check of orientBones.
  auto drawDiff = [](const Json& a, const Json& b) {
    const auto ra = std::make_shared<const Rig>(parseRig("a", a)), rb = std::make_shared<const Rig>(parseRig("b", b));
    double worst = 0;
    auto compare = [&](const std::function<void(Instance&)>& setup) {
      Instance ia(ra), ib(rb);
      setup(ia);
      setup(ib);
      const auto da = ia.drawList(), db = ib.drawList();
      if (da.size() != db.size()) { worst = 1e9; return; }
      for (size_t i = 0; i < da.size(); i++) {
        const Affine& x = da[i].matrix;
        const Affine& y = db[i].matrix;
        for (double dv : {x.a - y.a, x.b - y.b, x.c - y.c, x.d - y.d, x.e - y.e, x.f - y.f}) worst = std::max(worst, std::fabs(dv));
        if (da[i].positions.size() != db[i].positions.size() || da[i].imageAssetId != db[i].imageAssetId || da[i].alpha != db[i].alpha) { worst = 1e9; return; }
        for (size_t k = 0; k < da[i].positions.size(); k++) worst = std::max(worst, std::fabs(da[i].positions[k] - db[i].positions[k]));
      }
    };
    std::vector<std::string> skins{""}, expressions{""};
    for (const auto& s : ra->skins) skins.push_back(s.id);
    for (const auto& e : ra->expressions) expressions.push_back(e.id);
    for (const auto& skin : skins)
      for (const auto& expression : expressions) {
        auto look = [&](Instance& inst) {
          inst.setSkin(skin);
          inst.setExpression(expression.empty() ? std::nullopt : std::optional<std::string>(expression));
        };
        compare(look);
        for (const auto& c : ra->clips)
          for (int step = 0; step <= 24; step++) {
            const double t = double(c.durationMs) * step / 24.0 + (step % 5) * 0.37;
            compare([&](Instance& inst) { look(inst); inst.play(0, c.id, false); inst.seek(0, t); });
            for (const auto& layer : ra->clips)
              if (layer.id != c.id && step % 6 == 0)
                compare([&](Instance& inst) { look(inst); inst.play(0, c.id, false); inst.seek(0, t); inst.play(1, layer.id, false); inst.seek(1, t * 0.7); });
          }
        for (size_t i = 0; i + 1 < ra->clips.size(); i++)
          compare([&](Instance& inst) { look(inst); inst.play(0, ra->clips[i].id); inst.seek(0, 130); inst.play(0, ra->clips[i + 1].id, std::nullopt, 1, Then::Default, 240); inst.update(90); });
      }
    return worst;
  };
  auto boneJson = [](const Json& d, const std::string& id) { for (const auto& b : d["bones"]) if (b["id"] == id) return b; return Json(); };
  auto ikJson = [](const Json& d, const std::string& id) { for (const auto& k : d["ik"]) if (k["id"] == id) return k; return Json(); };
  auto tipOf = [](const Rig& r, const Pose& p, const std::string& bone, double len) {
    double x, y;
    p.world[size_t(r.boneIndex(bone))].apply(len, 0, x, y);
    return std::make_pair(x, y);
  };

  test("bone length and color: validation, parse, old documents unchanged", [&] {
    Json d = rigData();
    check(validateRig(d).empty() && parseRig("r", d).bones[1].length == 0 && parseRig("r", d).bones[1].color.empty(), "1.4 documents: point bones");
    for (const Json& bad : {Json(-1), Json("10"), Json(100001), Json(nullptr)}) {
      Json x = rigData();
      x["bones"][1]["length"] = bad;
      check(hasCode(validateRig(x), "ANIM_BONE_LENGTH"), "length " + bad.dump() + " refused");
    }
    for (const char* bad : {"#12", "red", "#12345g", "#1234567"}) {
      Json x = rigData();
      x["bones"][1]["color"] = bad;
      check(hasCode(validateRig(x), "ANIM_BONE_COLOR"), std::string("color ") + bad + " refused");
    }
    d["bones"][1]["length"] = 96.5;
    d["bones"][1]["color"] = "#a1B2c3";
    d["bones"][2]["color"] = "#a1b2c3ff";
    check(validateRig(d).empty(), "length + colors valid");
    const Rig r = parseRig("r", d);
    check(r.bones[1].length == 96.5 && r.bones[1].color == "#a1B2c3" && r.bones[2].color == "#a1b2c3ff", "parsed");
    check(drawDiff(rigData(), d) == 0, "length is not a transform: identical draw lists");
    EditResult e = applyEdit(rigData(), {{"op", "addBone"}, {"id", "tail"}, {"parent", "body"}, {"x", 10}, {"rotation", 30}, {"length", 40.25}, {"color", "#ff8800"}});
    const Json tail = boneJson(e.data, "tail");
    check(tail["length"] == 40.25 && tail["color"] == "#ff8800" && tail["rotation"] == 30, "addBone length/color");
    e = applyEdit(e.data, {{"op", "setBone"}, {"bone", "tail"}, {"length", nullptr}, {"color", nullptr}});
    check(!boneJson(e.data, "tail").contains("length") && !boneJson(e.data, "tail").contains("color"), "null clears (documents keep the 1.4 shape)");
    bool refused = false;
    try { applyEdit(rigData(), {{"op", "setBone"}, {"bone", "body"}, {"length", -3}}); } catch (const EditError&) { refused = true; }
    check(refused, "negative length refused");
    refused = false;
    try { applyEdit(rigData(), {{"op", "setBone"}, {"bone", "body"}, {"color", "blue"}}); } catch (const EditError&) { refused = true; }
    check(refused, "malformed color refused");
  });

  test("ik effector precedence: tip bone > lower length tip > first child > origin", [&] {
    // Target 80 px from the shoulder: reachable by upper 50 + forearm length 30 or hand 50.
    Json d = armRig(64, 48);
    {
      const Rig r = parseRig("arm", d);
      const auto hand = tipOf(r, samplePose(r, -1, 0), "hand", 0);
      check(near(hand.first, 64) && near(hand.second, 48) && r.ik[0].tip == r.boneIndex("hand") && r.ik[0].tipLength == 0, "no length: first child (1.4 rule)");
    }
    d["bones"][2]["length"] = 30;
    {
      const Rig r = parseRig("arm", d);
      const auto end = tipOf(r, samplePose(r, -1, 0), "lower", 30);
      check(r.ik[0].tip < 0 && r.ik[0].tipLength == 30 && near(end.first, 64) && near(end.second, 48), "lower length tip beats the first child");
    }
    d["ik"][0]["tip"] = "hand";
    {
      const Rig r = parseRig("arm", d);
      const auto hand = tipOf(r, samplePose(r, -1, 0), "hand", 0);
      check(r.ik[0].tipLength == 0 && near(hand.first, 64) && near(hand.second, 48), "explicit tip beats the length");
    }
    Json bare = armRig(64, 48);
    bare["bones"].erase(3);  // no hand: the lower origin aims at the target
    bare["slots"].erase(1);
    bare["attachments"].erase(1);
    {
      const Rig r = parseRig("arm", bare);
      const Pose p = samplePose(r, -1, 0);
      const auto low = tipOf(r, p, "lower", 0);
      check(r.ik[0].tip < 0 && std::fabs(low.first * 48 - low.second * 64) < 1e-6 && low.first > 0, "no child, no length: the origin aims (1.4 rule)");
    }
    // Aim: a bone with a length aims its tip (+x · length) at the target, aimAxis unchanged.
    Json aim = hangingArm();
    aim["bones"][1]["length"] = 100;
    aim["ik"] = Json::array({{{"id", "aim"}, {"bones", {"arm"}}, {"target", "t"}}});
    const Rig r = parseRig("arm", aim);
    const Pose p = samplePose(r, -1, 0);
    const auto o = tipOf(r, p, "arm", 0), end = tipOf(r, p, "arm", 100);
    const double tx = p.world[size_t(r.boneIndex("t"))].e, ty = p.world[size_t(r.boneIndex("t"))].f;
    check(std::fabs((end.first - o.first) * (ty - o.second) - (end.second - o.second) * (tx - o.first)) < 1e-6 && (end.first - o.first) * (tx - o.first) > 0, "aim: the tip lies on the line to the target");
  });

  test("ik with lengths: addIk prefers the length, setBone keeps the effector", [&] {
    // Aim chain on a bone with a length: no aimAxis (its +x), target created at its tip.
    Json aim = hangingArm();
    aim["bones"][1]["length"] = 100;
    EditResult e = applyEdit(aim, {{"op", "addIk"}, {"bones", {"arm"}}, {"createTarget", true}});
    const Json k = ikJson(e.data, e.extra["ik"]);
    const auto w = setupWorld(e.data);
    check(!k.contains("aimAxis") && near(w.at(e.extra["target"].get<std::string>()).e, 330) && near(w.at(e.extra["target"].get<std::string>()).f, 100), "aim: +x at the length, no axis");
    check(drawDiff(aim, e.data) < 1e-6, "creating the aim constraint moves nothing");
    // Two-bone chain whose lower bone has a length (and a child): the length tip is the effector.
    Json arm = armRig(64, 48);
    arm.erase("ik");
    arm["bones"][2]["length"] = 40;
    e = applyEdit(arm, {{"op", "addIk"}, {"bones", {"upper", "lower"}}, {"createTarget", true}});
    const Json limb = ikJson(e.data, e.extra["ik"]);
    const Json target = boneJson(e.data, e.extra["target"]);
    check(!limb.contains("tip") && near(target["x"].get<double>(), 90) && near(target["y"].get<double>(), 0), "two-bone: no tip, target at the lower tip");
    check(drawDiff(arm, e.data) < 1e-6, "creating the limb constraint moves nothing");
    // setBone length on a lower bone that used its first child: the child is pinned as tip.
    const Json before = armRig(64, 48);
    e = applyEdit(before, {{"op", "setBone"}, {"bone", "lower"}, {"length", 30}});
    check(e.data["ik"][0]["tip"] == "hand" && hasCode(validateRig(e.data), "ANIM_IK_TIP") == false, "first child pinned as tip");
    check(drawDiff(before, e.data) < 1e-9, "the limb does not move");
    check(std::find(e.affected.begin(), e.affected.end(), "ik:arm") != e.affected.end(), "ik reported as affected");
  });

  test("fitBoneLengths: distance to the first child", [&] {
    const Json before = armRig(64, 48);
    EditResult e = applyEdit(before, {{"op", "fitBoneLengths"}});
    check(boneJson(e.data, "upper")["length"] == 50 && boneJson(e.data, "lower")["length"] == 50 && !boneJson(e.data, "hand").contains("length") && !boneJson(e.data, "root").contains("length"), "upper/lower get 50, childless and zero-distance bones none");
    check(e.extra["lengths"] == Json({{"lower", 50}, {"upper", 50}}) && e.data["ik"][0]["tip"] == "hand", "extra.lengths, tip pinned");
    check(drawDiff(before, e.data) < 1e-9, "nothing moves");
    Json tilted = before;
    tilted["bones"][3]["x"] = 30; tilted["bones"][3]["y"] = 40;
    check(applyEdit(tilted, {{"op", "fitBoneLengths"}, {"bones", {"lower"}}}).data["bones"][2]["length"] == 50, "an off-axis child: its distance");
    bool refused = false;
    try { applyEdit(before, {{"op", "fitBoneLengths"}, {"bones", {"hand"}}}); } catch (const EditError&) { refused = true; }
    check(refused, "nothing to fit: refused");
    refused = false;
    try { applyEdit(before, {{"op", "fitBoneLengths"}, {"bones", {"nope"}}}); } catch (const EditError&) { refused = true; }
    check(refused, "unknown bone refused");
  });

  test("orientBones: hanging arm becomes a Spine bone, aimAxis 90 disappears", [&] {
    Json d = hangingArm();
    d["ik"] = Json::array({{{"id", "aim"}, {"bones", {"arm"}}, {"target", "t"}, {"aimAxis", 90}}});
    EditResult e = applyEdit(d, {{"op", "orientBones"}, {"bones", {"arm"}}});
    const Json arm = boneJson(e.data, "arm");
    check(arm["rotation"] == 90 && arm["length"] == 100 && !e.data["ik"][0].contains("aimAxis"), "rotation 90, length 100, no aim axis");
    check(e.extra["oriented"] == Json({{"arm", 90}}), "extra.oriented");
    check(drawDiff(d, e.data) < 1e-6, "draws identically (IK solved)");
    check(validateRig(e.data).empty(), "valid");
    // Already has a length -> skipped (listed: warning); nothing at all -> refused.
    bool refused = false;
    try { applyEdit(e.data, {{"op", "orientBones"}, {"bones", {"arm"}}}); } catch (const EditError&) { refused = true; }
    check(refused, "an oriented bone is not turned again");
    e = applyEdit(d, {{"op", "orientBones"}, {"fitLength", false}});
    check(!boneJson(e.data, "arm").contains("length") && e.extra["skipped"].contains("t") && !e.extra["oriented"].contains("root") && !hasCode(e.diagnostics, "ANIM_ORIENT_SKIPPED"), "fitLength false; unlisted skips are silent");
  });

  test("orientBones keeps animation identical (demo-like rig, IK, meshes, skins, expressions)", [&] {
    // The demo character's shape: arms hang down (image along +y), a breathing body (non-uniform
    // scale: kept), head/arm keys, an aim arm with aimAxis, a hand keyed on x only, a skin and an
    // expression that override the arm and head.
    Json demo = Json::parse(R"({"schemaVersion": 1, "canvas": {"width": 512, "height": 768, "originX": 256, "originY": 720},
      "bones": [
        {"id": "root", "parent": null, "x": 256, "y": 720},
        {"id": "body", "parent": "root", "x": 0, "y": -80},
        {"id": "head", "parent": "body", "x": 0, "y": -170},
        {"id": "arm-left", "parent": "body", "x": -42.825279, "y": -148.936803},
        {"id": "arm-right", "parent": "body", "x": 42.825279, "y": -151.360595, "rotation": 4, "scaleX": 1.1, "scaleY": 1.1},
        {"id": "hand", "parent": "arm-right", "x": 2, "y": 104, "rotation": -7},
        {"id": "t", "parent": "root", "x": -120, "y": -150}],
      "slots": [
        {"id": "arm-left", "bone": "arm-left", "attachment": "arm-left"}, {"id": "arm-right", "bone": "arm-right", "attachment": "arm-right"},
        {"id": "body", "bone": "body", "attachment": "body"}, {"id": "hand", "bone": "hand", "attachment": "hand"},
        {"id": "head", "bone": "head", "attachment": "head"}, {"id": "eyes", "bone": "head", "attachment": "open"}],
      "attachments": [
        {"slot": "body", "name": "body", "imageAssetId": "img-body", "width": 130, "height": 170, "pivotX": 0.5, "pivotY": 1},
        {"slot": "arm-left", "name": "arm-left", "imageAssetId": "img-arm", "x": 0.825279, "y": 53.936803, "width": 40, "height": 120},
        {"slot": "arm-right", "name": "arm-right", "imageAssetId": "img-arm", "width": 40, "height": 120, "pivotY": 0.08, "rotation": 3},
        {"slot": "hand", "name": "hand", "imageAssetId": "img-hand", "width": 30, "height": 30, "y": 12},
        {"slot": "head", "name": "head", "imageAssetId": "img-head", "y": -70, "width": 150, "height": 150},
        {"slot": "eyes", "name": "open", "imageAssetId": "img-eyes-open", "y": -65, "width": 90, "height": 30},
        {"slot": "eyes", "name": "closed", "imageAssetId": "img-eyes-closed", "y": -65, "width": 90, "height": 30}],
      "skins": [{"id": "winter", "attachments": [
        {"slot": "arm-left", "name": "arm-left", "imageAssetId": "img-arm-winter", "x": 2, "y": 50, "rotation": -6, "scaleX": 1.2, "width": 44, "height": 124}]}],
      "expressions": [{"id": "wave", "slots": {"eyes": "closed"}, "bones": {"head": {"rotation": 8}, "arm-right": {"rotation": -40}, "hand": {"rotation": 15}}}],
      "ik": [{"id": "look", "bones": ["arm-left"], "target": "t", "aimAxis": 90, "mix": 0.7}],
      "clips": [
        {"id": "idle", "durationMs": 2400, "loop": true, "tracks": [
          {"target": "bone:body", "property": "scaleY", "keys": [{"timeMs": 0, "value": 1}, {"timeMs": 1200, "value": 1.03, "interp": "bezier", "curve": [0.42, 0, 0.58, 1]}, {"timeMs": 2400, "value": 1}]},
          {"target": "bone:head", "property": "rotation", "keys": [{"timeMs": 0, "value": 0}, {"timeMs": 1200, "value": 3}, {"timeMs": 2400, "value": 0}]},
          {"target": "bone:t", "property": "y", "keys": [{"timeMs": 0, "value": -150}, {"timeMs": 1200, "value": -60}, {"timeMs": 2400, "value": -150}]}]},
        {"id": "joy", "durationMs": 900, "loop": false, "tracks": [
          {"target": "bone:head", "property": "x", "keys": [{"timeMs": 0, "value": 0}]},
          {"target": "bone:head", "property": "y", "keys": [{"timeMs": 0, "value": -170}]},
          {"target": "bone:head", "property": "rotation", "keys": [{"timeMs": 0, "value": 0}, {"timeMs": 300, "value": -8}, {"timeMs": 600, "value": 6, "interp": "step"}, {"timeMs": 900, "value": 0}]},
          {"target": "bone:head", "property": "scaleX", "keys": [{"timeMs": 0, "value": 1}]},
          {"target": "bone:head", "property": "scaleY", "keys": [{"timeMs": 0, "value": 1}]},
          {"target": "bone:arm-left", "property": "x", "keys": [{"timeMs": 0, "value": -42.825279}]},
          {"target": "bone:arm-left", "property": "y", "keys": [{"timeMs": 0, "value": -148.936803}]},
          {"target": "bone:arm-left", "property": "rotation", "keys": [{"timeMs": 0, "value": 0}, {"timeMs": 450, "value": 35, "interp": "bezier", "curve": [0.3, 0, 0.2, 1]}, {"timeMs": 900, "value": 0}]},
          {"target": "bone:arm-right", "property": "rotation", "keys": [{"timeMs": 0, "value": 4}, {"timeMs": 450, "value": -60}, {"timeMs": 900, "value": 4}]},
          {"target": "bone:arm-right", "property": "scaleX", "keys": [{"timeMs": 0, "value": 1.1}, {"timeMs": 450, "value": 1.25, "interp": "bezier", "curve": [0.42, 0, 0.58, 1]}, {"timeMs": 900, "value": 1.1}]},
          {"target": "bone:arm-right", "property": "scaleY", "keys": [{"timeMs": 0, "value": 1.1}, {"timeMs": 450, "value": 1.25, "interp": "bezier", "curve": [0.42, 0, 0.58, 1]}, {"timeMs": 900, "value": 1.1}]},
          {"target": "bone:hand", "property": "x", "keys": [{"timeMs": 0, "value": 2}, {"timeMs": 450, "value": 14, "interp": "bezier", "curve": [0.42, 0, 0.58, 1]}, {"timeMs": 900, "value": 2}]},
          {"target": "slot:eyes", "property": "attachment", "keys": [{"timeMs": 0, "value": "open"}, {"timeMs": 500, "value": "closed"}]}]}]})");
    check(validateRig(demo).empty(), "fixture valid");
    EditResult e = applyEdit(demo, {{"op", "orientBones"}});
    const double diff = drawDiff(demo, e.data);
    check(diff < 1e-6, "draw lists identical (max diff " + std::to_string(diff) + ")");
    check(validateRig(e.data).empty(), "result valid");
    const Json& o = e.extra["oriented"];
    check(o.contains("root") && o.contains("head") && o.contains("arm-left") && o.contains("arm-right") && o.contains("hand"), "root, head, arms, hand turned: " + o.dump());
    check(e.extra["skipped"].contains("body") && e.extra["skipped"].contains("t"), "breathing body (non-uniform scale) and target skipped: " + e.extra["skipped"].dump());
    check(!ikJson(e.data, "look").contains("aimAxis") || std::fabs(ikJson(e.data, "look")["aimAxis"].get<double>()) < 15, "aim axis absorbed by the turn");
    check(near(o["arm-left"].get<double>(), 90, 1.0) && boneJson(e.data, "arm-right")["length"].get<double>() > 100, "arms point down with a length");
    // hand.x keyed alone: its y track is made at the same times (y is never keyed elsewhere).
    bool handY = false;
    for (const auto& c : e.data["clips"]) for (const auto& t : c["tracks"]) handY = handY || (t["target"] == "bone:hand" && t["property"] == "y" && t["keys"].size() == 3);
    check(handY, "hand y track made for the pair");
    // A child keyed on x in one clip and on y in another cannot turn: its parent is skipped.
    Json split = demo;
    split["clips"][0]["tracks"].push_back({{"target", "bone:hand"}, {"property", "y"}, {"keys", Json::array({{{"timeMs", 0}, {"value", 104}}, {{"timeMs", 900}, {"value", 90}}})}});
    e = applyEdit(split, {{"op", "orientBones"}});
    check(e.extra["skipped"].contains("arm-right") && drawDiff(split, e.data) < 1e-6, "split x/y keys: parent skipped, still identical");
    // Fixtures with two-bone IK (rotated/scaled parents, mix/bend keys) and weighted meshes.
    Json ik = ikParityRig();
    ik["bones"][2]["x"] = 35; ik["bones"][2]["y"] = 35;  // lower off the upper's +x: upper turns 45°
    e = applyEdit(ik, {{"op", "orientBones"}});
    check(e.extra["oriented"].contains("upper") && e.extra["skipped"].contains("head") && drawDiff(ik, e.data) < 1e-6, "two-bone IK fixture identical, mirrored head skipped");
    Json mesh = meshParityRig();
    mesh["bones"][2]["x"] = 70.710678; mesh["bones"][2]["y"] = 70.710678;  // a weighted arm whose frame turns
    e = applyEdit(mesh, {{"op", "orientBones"}});
    check(e.extra["oriented"].contains("upper") && e.extra["oriented"].contains("hand") && drawDiff(mesh, e.data) < 1e-6, "weighted mesh + deform + skin fixture identical");
  });

  // ---- Freeform meshes: outlines, vertex / edge edits, alpha tracing (plugin 1.6.0) ----
  test("outline mesh: concave polygon is tiled exactly (constrained Delaunay)", [&] {
    const std::vector<double> L = {0, 0, 100, 0, 100, 40, 40, 40, 40, 100, 0, 100};
    auto r = applyEdit(outlineRig(), {{"op", "createMeshFromOutline"}, {"slot", "cape"}, {"hull", L}});
    const Json& a = r.data["attachments"][0];
    check(validateRig(r.data).empty() && a["type"] == "mesh" && a["hull"] == 6 && a["vertices"].size() == 12, "region -> 6-vertex outline mesh");
    check(tilingProblem(a, 6400).empty(), "L shape: " + tilingProblem(a, 6400));
    check(a["triangles"].size() == 12 && r.extra["triangles"] == 4, "n - 2 triangles");
    check(a["uvs"][2] == 1 && a["uvs"][3] == 0 && a["uvs"][6] == 0.4, "uvs = position / image size");
    check(a["edges"] == Json::array({0, 1, 0, 5, 1, 2, 2, 3, 3, 4, 4, 5}), "edges = the outline cycle (sorted pairs)");
    // Interior grid: denser, still an exact tiling, Delaunay away from the outline, every point used.
    auto dense = applyEdit(outlineRig(), {{"op", "createMeshFromOutline"}, {"slot", "cape"}, {"hull", L}, {"spacing", 10}});
    const Json& b = dense.data["attachments"][0];
    check(b["vertices"].size() / 2 > 40 && tilingProblem(b, 6400).empty() && delaunayViolations(b) == 0, "spacing grid: " + tilingProblem(b, 6400));
    // Drawn the other way round: kept as drawn (index 0 = the first click), same area.
    std::vector<double> back;
    for (size_t i = 0; i < 6; i++) { const size_t k = (6 - i) % 6; back.push_back(L[2 * k]); back.push_back(L[2 * k + 1]); }
    auto cw = applyEdit(outlineRig(), {{"op", "createMeshFromOutline"}, {"slot", "cape"}, {"hull", back}, {"points", {20, 20, 20, 80}}});
    const Json& c = cw.data["attachments"][0];
    check(c["vertices"][2] == 0 && c["vertices"][3] == 100 && tilingProblem(c, 6400).empty() && c["vertices"].size() == 16, "either direction, interior points appended");
    // Collinear outline points (a rectangle with points on its sides) and a comb (deeply concave).
    auto rect = applyEdit(outlineRig(), {{"op", "createMeshFromOutline"}, {"slot", "cape"}, {"hull", {0, 0, 50, 0, 100, 0, 100, 50, 100, 100, 50, 100, 0, 100, 0, 50}}, {"points", {50, 50}}});
    check(tilingProblem(rect.data["attachments"][0], 10000).empty(), "collinear outline points: " + tilingProblem(rect.data["attachments"][0], 10000));
    std::vector<double> comb = {0, 100, 0, 0};
    for (int t = 0; t < 5; t++) { const double x = t * 20.0; comb.insert(comb.end(), {x + 5, 0, x + 10, 90, x + 15, 0, x + 20, 0}); }
    comb.resize(comb.size() - 2);
    comb.insert(comb.end(), {100, 0, 100, 100});
    auto combed = applyEdit(outlineRig(), {{"op", "createMeshFromOutline"}, {"slot", "cape"}, {"hull", comb}});
    check(tilingProblem(combed.data["attachments"][0]).empty(), "comb outline: " + tilingProblem(combed.data["attachments"][0]));
    // Refusals with a clear reason: self-intersection, doubled point, flat, too few, interior point outside.
    auto reason = [&](const Json& op) { try { applyEdit(outlineRig(), op); } catch (const EditError& e) { return std::string(e.what()); } return std::string(); };
    const Json base = {{"op", "createMeshFromOutline"}, {"slot", "cape"}};
    Json bow = base; bow["hull"] = {0, 0, 100, 100, 100, 0, 0, 100};
    check(reason(bow).find("교차") != std::string::npos, "bow tie refused: " + reason(bow));
    Json dup = base; dup["hull"] = {0, 0, 100, 0, 100, 100, 100, 0, 0, 100};
    check(!reason(dup).empty(), "doubled point refused");
    Json flat = base; flat["hull"] = {0, 0, 50, 0, 100, 0};
    check(!reason(flat).empty(), "flat outline refused");
    Json two = base; two["hull"] = {0, 0, 10, 10};
    Json out = base; out["hull"] = L; out["points"] = {80, 80};
    check(!reason(two).empty() && reason(out).find("윤곽 밖") != std::string::npos, "two points / interior point outside refused");
    Json bad = base; bad["hull"] = L; bad["edges"] = {0, 9};
    check(!reason(bad).empty(), "edge index out of range refused");
    Host host;
    const Json op = {{"op", "createMeshFromOutline"}, {"slot", "cape"}, {"hull", L}, {"spacing", 15}};
    check(host.handle({{"op", "edit"}, {"rig", outlineRig()}, {"operation", op}})["rig"] == applyEdit(outlineRig(), op).data, "host edit = core edit");
    // A large mesh stays within the limits and is fast enough for an editor gesture.
    const auto start = std::chrono::steady_clock::now();
    Json big = outlineRig();
    big["attachments"][0]["width"] = 1024; big["attachments"][0]["height"] = 1024;
    auto large = applyEdit(big, {{"op", "createMeshFromOutline"}, {"slot", "cape"}, {"hull", {0, 0, 1024, 0, 1024, 1024, 0, 1024}}, {"spacing", 25}});
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::cout << "outline mesh " << large.data["attachments"][0]["vertices"].size() / 2 << " vertices / " << large.extra["triangles"] << " triangles in " << ms << " ms\n";
    check(tilingProblem(large.data["attachments"][0], 1024.0 * 1024).empty() && large.extra["triangles"].get<int>() > 3000 && ms < 3000, "1600+ vertices triangulated");
  });

  test("mesh vertex add/remove interpolates weights, uvs and deform keys", [&] {
    Json d = applyEdit(outlineRig(), {{"op", "createMeshFromOutline"}, {"slot", "cape"}, {"hull", {0, 0, 100, 0, 100, 100, 0, 100}}}).data;
    // Weights by x: left at x = 0, right at x = 100 (linear in x on any triangulation of the square).
    d = applyEdit(d, {{"op", "setWeights"}, {"slot", "cape"}, {"vertices", {0, 1, 2, 3}}, {"weights", Json::array({
      Json::array({{{"bone", "left"}, {"weight", 1}}}), Json::array({{{"bone", "right"}, {"weight", 1}}}),
      Json::array({{{"bone", "right"}, {"weight", 1}}}), Json::array({{{"bone", "left"}, {"weight", 1}}})})}}).data;
    // A skin's same-named mesh with another vertex count keeps its own deform keys.
    d["skins"] = Json::array({{{"id", "alt"}, {"attachments", Json::array({d["attachments"][0]})}}});
    d = applyEdit(d, {{"op", "createMeshFromOutline"}, {"slot", "cape"}, {"skin", "alt"}, {"hull", {0, 0, 50, 0, 100, 0, 100, 100, 0, 100, 0, 50}}}).data;
    const std::vector<double> four = {0, 0, 10, 0, 10, 0, 0, 0}, six(12, 3.0);
    d["clips"] = Json::array({{{"id", "c"}, {"durationMs", 1000}, {"tracks", Json::array({{{"target", "slot:cape"}, {"property", "deform"}, {"attachment", "cape"},
      {"keys", Json::array({{{"timeMs", 0}, {"offsets", four}}, {{"timeMs", 500}, {"offsets", six}}})}}})}}});
    check(validateRig(d).size() == 1 && hasCode(validateRig(d), "ANIM_MESH_DEFORM_SKIN"), "fixture: two same-named meshes, keys for both");
    auto r = applyEdit(d, {{"op", "addMeshVertex"}, {"slot", "cape"}, {"position", {25, 50}}});
    const Json& a = r.data["attachments"][0];
    check(r.extra["vertex"] == 4 && a["vertices"].size() == 10 && a["hull"] == 4 && tilingProblem(a, 10000).empty(), "interior vertex appended, re-triangulated");
    check(a["weights"][4] == Json::array({{{"bone", "left"}, {"weight", 0.75}}, {{"bone", "right"}, {"weight", 0.25}}}), "weights interpolated barycentrically: " + a["weights"][4].dump());
    check(a["uvs"][8] == 0.25 && a["uvs"][9] == 0.5, "uv interpolated (image stays put)");
    const Json keys = r.data["clips"][0]["tracks"][0]["keys"];
    check(keys[0]["offsets"].size() == 10 && near(keys[0]["offsets"][8].get<double>(), 2.5) && keys[0]["offsets"][2] == 10 && keys[1]["offsets"] == Json(six), "deform: kept vertices keep, new blends; the skin mesh's key untouched");
    check(hasCode(r.diagnostics, "ANIM_MESH_DEFORM_REMAPPED") && r.extra["remappedKeys"] == 1 && !hasErrors(validateRig(r.data)), "remap reported, rig valid");
    check(r.data["skins"][0]["attachments"][0]["vertices"].size() == 12, "skin mesh unchanged");
    // On an outline edge: a new outline vertex right after the edge's start; later indices shift.
    auto e = applyEdit(r.data, {{"op", "addMeshVertex"}, {"slot", "cape"}, {"position", {100, 30}}});
    const Json& b = e.data["attachments"][0];
    check(e.extra["vertex"] == 2 && b["hull"] == 5 && b["vertices"][4] == 100 && b["vertices"][5] == 30 && tilingProblem(b, 10000).empty(), "outline vertex inserted at 2");
    check(b["weights"][2] == Json::array({{{"bone", "right"}, {"weight", 1}}}) && b["weights"][5] == a["weights"][4], "weights follow the renumbering");
    check(e.data["clips"][0]["tracks"][0]["keys"][0]["offsets"][4] == 10 && e.data["clips"][0]["tracks"][0]["keys"][0]["offsets"][10] == 2.5, "deform follows the renumbering");
    check(refused(r.data, {{"op", "addMeshVertex"}, {"slot", "cape"}, {"position", {150, 50}}}) && refused(r.data, {{"op", "addMeshVertex"}, {"slot", "cape"}, {"position", {25, 50}}}) &&
          refused(r.data, {{"op", "addMeshVertex"}, {"slot", "cape"}, {"position", {1}}}), "outside / on a vertex / bad position refused");
    // Remove: the interior vertex goes back to the original data exactly.
    auto back = applyEdit(r.data, {{"op", "removeMeshVertex"}, {"slot", "cape"}, {"vertices", {4}}});
    const Json& c = back.data["attachments"][0];
    check(c["vertices"] == d["attachments"][0]["vertices"] && c["weights"] == d["attachments"][0]["weights"] && back.data["clips"][0]["tracks"][0]["keys"][0]["offsets"] == Json(four), "remove restores vertices, weights, deform");
    // Outline vertices: removable while the outline stays valid and holds every interior vertex.
    auto pent = applyEdit(e.data, {{"op", "removeMeshVertex"}, {"slot", "cape"}, {"vertices", {2}}});
    check(pent.data["attachments"][0]["hull"] == 4 && tilingProblem(pent.data["attachments"][0], 10000).empty(), "outline vertex removed");
    auto tri = applyEdit(r.data, {{"op", "removeMeshVertex"}, {"slot", "cape"}, {"vertices", {1}}});
    check(tri.data["attachments"][0]["hull"] == 3 && tilingProblem(tri.data["attachments"][0], 5000).empty(), "square -> triangle, the interior vertex still inside");
    check(refused(r.data, {{"op", "removeMeshVertex"}, {"slot", "cape"}, {"vertices", {0}}}), "interior vertex left outside refused");
    check(refused(tri.data, {{"op", "removeMeshVertex"}, {"slot", "cape"}, {"vertices", {0}}}) && refused(r.data, {{"op", "removeMeshVertex"}, {"slot", "cape"}, {"vertices", {0, 1}}}), "fewer than 3 outline vertices refused");
    // Meshes without an outline (alpha-trimmed grids): local edits only, outline vertices stay.
    Json grid = applyEdit(meshArm(), {{"op", "convertToMesh"}, {"slot", "arm"}, {"rows", 3}, {"cols", 3}, {"cells", "111111110"}}).data;
    const Json& g = grid["attachments"][0];
    int centre = -1;
    for (size_t i = 0; i < g["vertices"].size() / 2; i++) if (near(g["vertices"][2 * i].get<double>(), 200.0 / 3, 1e-4) && near(g["vertices"][2 * i + 1].get<double>(), 40.0 / 3, 1e-4)) centre = int(i);
    const double gridArea = 200.0 * 40 * 8 / 9;
    auto split = applyEdit(grid, {{"op", "addMeshVertex"}, {"slot", "arm"}, {"position", {40, 20}}});
    check(split.extra["vertex"] == 15 && tilingProblem(split.data["attachments"][0], gridArea).empty() && !split.data["attachments"][0].contains("hull"), "no outline: local split");
    auto gone = applyEdit(grid, {{"op", "removeMeshVertex"}, {"slot", "arm"}, {"vertices", {centre}}});
    check(centre >= 0 && gone.data["attachments"][0]["vertices"].size() == 28 && tilingProblem(gone.data["attachments"][0], gridArea).empty(), "no outline: interior vertex removed");
    auto reason = [&](const Json& data, const Json& op) { try { applyEdit(data, op); } catch (const EditError& x) { return std::string(x.what()); } return std::string(); };
    check(reason(grid, {{"op", "removeMeshVertex"}, {"slot", "arm"}, {"vertices", {0}}}).find("윤곽 그리기") != std::string::npos, "no outline: outline vertex refused with a hint");
    check(refused(grid, {{"op", "addMeshVertex"}, {"slot", "arm"}, {"position", {190, 35}}}), "no outline: a point in the trimmed cell is outside");
  });

  test("mesh edges: pin, keep through re-triangulation, refuse crossings", [&] {
    Json d = applyEdit(outlineRig(), {{"op", "createMeshFromOutline"}, {"slot", "cape"}, {"hull", {0, 0, 100, 0, 100, 100, 0, 100}}, {"points", {30, 30, 70, 30, 70, 70, 30, 70, 50, 50}}}).data;
    check(!hasEdge(d["attachments"][0], 0, 5), "fixture: 0-5 is not a Delaunay edge");
    auto r = applyEdit(d, {{"op", "addMeshEdge"}, {"slot", "cape"}, {"edge", {5, 0}}});
    const Json& a = r.data["attachments"][0];
    bool listed = false;
    for (size_t i = 0; i + 1 < a["edges"].size(); i += 2) listed = listed || (a["edges"][i] == 0 && a["edges"][i + 1] == 5);
    check(hasEdge(a, 0, 5) && listed && r.extra["pinnedEdges"] == 1 && tilingProblem(a, 10000).empty() && delaunayViolations(a) == 0, "pinned edge forced in, rest Delaunay");
    auto again = applyEdit(r.data, {{"op", "retriangulate"}, {"slot", "cape"}});
    check(hasEdge(again.data["attachments"][0], 0, 5) && again.data["attachments"][0]["edges"] == a["edges"], "re-triangulation keeps the pinned edge");
    auto moved = applyEdit(r.data, {{"op", "addMeshVertex"}, {"slot", "cape"}, {"position", {10, 80}}});
    check(hasEdge(moved.data["attachments"][0], 0, 5), "vertex edits keep pinned edges");
    auto reason = [&](const Json& data, const Json& op) { try { applyEdit(data, op); } catch (const EditError& x) { return std::string(x.what()); } return std::string(); };
    check(reason(r.data, {{"op", "addMeshEdge"}, {"slot", "cape"}, {"edge", {1, 4}}}).find("교차") != std::string::npos, "crossing a pinned edge refused");
    check(reason(d, {{"op", "addMeshEdge"}, {"slot", "cape"}, {"edge", {4, 6}}}).find("지납니다") != std::string::npos, "through another vertex refused");
    check(refused(r.data, {{"op", "addMeshEdge"}, {"slot", "cape"}, {"edge", {0, 5}}}) && refused(d, {{"op", "addMeshEdge"}, {"slot", "cape"}, {"edge", {0, 1}}}) &&
          refused(d, {{"op", "addMeshEdge"}, {"slot", "cape"}, {"edge", {2, 2}}}), "already pinned / outline edge / same vertex refused");
    Json concave = applyEdit(outlineRig(), {{"op", "createMeshFromOutline"}, {"slot", "cape"}, {"hull", {0, 0, 100, 0, 100, 40, 40, 40, 40, 100, 0, 100}}}).data;
    check(reason(concave, {{"op", "addMeshEdge"}, {"slot", "cape"}, {"edge", {2, 4}}}).find("밖") != std::string::npos, "an edge across the notch (outside) refused");
    auto unpinned = applyEdit(r.data, {{"op", "removeMeshEdge"}, {"slot", "cape"}, {"edge", {0, 5}}});
    check(unpinned.data["attachments"][0]["edges"] == d["attachments"][0]["edges"] && unpinned.data["attachments"][0]["triangles"] == d["attachments"][0]["triangles"], "unpinned: back to the Delaunay mesh");
    check(refused(d, {{"op", "removeMeshEdge"}, {"slot", "cape"}, {"edge", {0, 1}}}) && refused(d, {{"op", "removeMeshEdge"}, {"slot", "cape"}, {"edge", {4, 8}}}), "outline / unpinned edges cannot be removed");
    // Without an outline the edge is forced in by flips (Sloan).
    Json grid = applyEdit(meshArm(), {{"op", "convertToMesh"}, {"slot", "arm"}, {"rows", 2}, {"cols", 4}, {"cells", "11111110"}}).data;
    const Json& g = grid["attachments"][0];
    int p = -1, q = -1;
    for (size_t i = 0; i < g["vertices"].size() / 2; i++) {
      if (g["vertices"][2 * i] == 50 && g["vertices"][2 * i + 1] == 0) p = int(i);
      if (g["vertices"][2 * i] == 100 && g["vertices"][2 * i + 1] == 40) q = int(i);
    }
    check(p >= 0 && q >= 0 && !hasEdge(g, p, q), "grid fixture");
    auto pinned = applyEdit(grid, {{"op", "addMeshEdge"}, {"slot", "arm"}, {"edge", {p, q}}});
    check(hasEdge(pinned.data["attachments"][0], p, q) && tilingProblem(pinned.data["attachments"][0], 200.0 * 40 * 7 / 8).empty(), "no outline: edge flipped in");
    auto flat = applyEdit(pinned.data, {{"op", "retriangulate"}, {"slot", "arm"}});
    check(hasEdge(flat.data["attachments"][0], p, q) && delaunayViolations(flat.data["attachments"][0]) == 0 && tilingProblem(flat.data["attachments"][0], 200.0 * 40 * 7 / 8).empty(), "no outline: retriangulate keeps it");
  });

  test("retriangulate repairs a folded mesh; validation of outline data", [&] {
    Json d = applyEdit(outlineRig(), {{"op", "createMeshFromOutline"}, {"slot", "cape"}, {"hull", {0, 0, 100, 0, 100, 100, 0, 100}}, {"spacing", 20}}).data;
    const size_t n = d["attachments"][0]["vertices"].size() / 2;
    // Drag an interior vertex across its neighbours: triangles fold over (still inside the outline).
    auto folded = applyEdit(d, {{"op", "setMeshVertices"}, {"slot", "cape"}, {"vertices", {4}}, {"positions", {85, 85}}});
    check(!tilingProblem(folded.data["attachments"][0], 10000).empty(), "fixture folds");
    auto fixed = applyEdit(folded.data, {{"op", "retriangulate"}, {"slot", "cape"}});
    check(tilingProblem(fixed.data["attachments"][0], 10000).empty() && fixed.data["attachments"][0]["vertices"].size() / 2 == n && delaunayViolations(fixed.data["attachments"][0]) == 0, "re-triangulated from the outline");
    check(fixed.data["attachments"][0]["vertices"] == folded.data["attachments"][0]["vertices"] && fixed.data["attachments"][0]["uvs"] == folded.data["attachments"][0]["uvs"], "vertices and uvs untouched");
    // Outline crossing itself: a warning (drawing is unaffected), and re-triangulation explains why it cannot.
    auto twisted = applyEdit(d, {{"op", "setMeshVertices"}, {"slot", "cape"}, {"vertices", {1}}, {"positions", {-20, 100}}});
    check(hasCode(twisted.diagnostics, "ANIM_MESH_HULL_SELF") && !hasErrors(twisted.diagnostics), "self-crossing outline warned");
    check(refused(twisted.data, {{"op", "retriangulate"}, {"slot", "cape"}}), "retriangulate refused on a crossing outline");
    Json e = d;
    e["attachments"][0]["edges"].push_back(3);
    e["attachments"][0]["edges"].push_back(3);
    check(hasCode(validateRig(e), "ANIM_MESH_EDGES") && !hasErrors(validateRig(e)), "self-loop edge warned");
    e = d;
    e["attachments"][0]["hull"] = 2;
    check(hasCode(validateRig(e), "ANIM_MESH_HULL") && !hasErrors(validateRig(e)), "hull of 2 warned");
    // Grid meshes of plugin 1.4.0 keep loading and can be re-triangulated (outline = the grid hull).
    Json grid = applyEdit(meshArm(), {{"op", "convertToMesh"}, {"slot", "arm"}, {"rows", 2}, {"cols", 4}}).data;
    auto g = applyEdit(grid, {{"op", "retriangulate"}, {"slot", "arm"}});
    check(tilingProblem(g.data["attachments"][0], 8000).empty() && g.data["attachments"][0]["vertices"] == grid["attachments"][0]["vertices"], "grid re-triangulated in place");
  });

  test("autoOutline traces the largest opaque region of an alpha mask", [&] {
    Json d = outlineRig();
    d["attachments"][0]["width"] = 128; d["attachments"][0]["height"] = 96;
    // 64x48 mask (half the image size): a disc of radius 20 at (30, 24) -> 40 px in image space.
    const Json mask = discMask(64, 48, 30, 24, 20);
    auto r = applyEdit(d, {{"op", "autoOutline"}, {"slot", "cape"}, {"mask", mask}});
    const Json& a = r.data["attachments"][0];
    const std::vector<double> outline = numbers(r.extra["outline"]);
    const size_t hull = outline.size() / 2;
    check(validateRig(r.data).empty() && a["hull"] == hull && hull >= 6 && hull <= 256 && polygonProblem(outline).empty() && tilingProblem(a).empty(), "simple outline, tiled: " + std::to_string(hull) + " points");
    bool covered = true, bounded = true;
    for (int k = 0; k < 64; k++) {
      const double t = k * 2 * 3.14159265358979 / 64;
      covered = covered && insidePolygon(outline, 60 + 39 * std::cos(t), 48 + 39 * std::sin(t));  // just inside the disc's edge
    }
    for (size_t i = 0; i < hull; i++) bounded = bounded && outline[2 * i] >= 0 && outline[2 * i] <= 128 && outline[2 * i + 1] >= 0 && outline[2 * i + 1] <= 96;
    const double area = std::fabs(polygonArea(outline, hull)), disc = 3.14159265358979 * 40 * 40;
    check(covered && bounded && area > disc && area < disc * 1.6, "covers the disc, stays in the image, not much bigger (area " + std::to_string(area) + ")");
    check(!insidePolygon(outline, 125, 2), "the far speck is left out");
    auto fine = applyEdit(d, {{"op", "autoOutline"}, {"slot", "cape"}, {"mask", mask}, {"detail", 1}});
    auto coarse = applyEdit(d, {{"op", "autoOutline"}, {"slot", "cape"}, {"mask", mask}, {"detail", 0}});
    check(fine.extra["hull"].get<int>() > r.extra["hull"].get<int>() && coarse.extra["hull"].get<int>() < r.extra["hull"].get<int>(), "detail controls the point count");
    auto threshold = applyEdit(d, {{"op", "autoOutline"}, {"slot", "cape"}, {"mask", mask}, {"alphaThreshold", 0.9}});
    check(insidePolygon(numbers(threshold.extra["outline"]), 60, 48), "threshold above the speck's alpha still finds the disc");
    auto withGrid = applyEdit(d, {{"op", "autoOutline"}, {"slot", "cape"}, {"mask", mask}, {"spacing", 12}});
    check(withGrid.data["attachments"][0]["vertices"].size() / 2 > hull + 10 && tilingProblem(withGrid.data["attachments"][0]).empty(), "interior grid inside the traced outline");
    Json empty = mask;
    empty["alpha"] = base64(std::vector<uint8_t>(64 * 48, 0));
    Json shortMask = mask;
    shortMask["alpha"] = base64(std::vector<uint8_t>(10, 255));
    check(refused(d, {{"op", "autoOutline"}, {"slot", "cape"}}) && refused(d, {{"op", "autoOutline"}, {"slot", "cape"}, {"mask", empty}}) &&
          refused(d, {{"op", "autoOutline"}, {"slot", "cape"}, {"mask", shortMask}}) && refused(d, {{"op", "autoOutline"}, {"slot", "cape"}, {"mask", mask}, {"detail", 2}}), "missing / empty / wrong-size mask, bad detail refused");
    // Tracing is the exported core function (Qt previews with it before the edit).
    std::vector<uint8_t> raw(64 * 48, 0);
    for (int y = 10; y < 30; y++) for (int x = 5; x < 50; x++) raw[size_t(y) * 64 + size_t(x)] = 255;
    const std::vector<double> box = traceAlphaOutline(raw, 64, 48, 0.5, 1);
    check(polygonProblem(box).empty() && insidePolygon(box, 5.5, 10.5) && insidePolygon(box, 49.5, 29.5) && std::fabs(polygonArea(box, box.size() / 2)) < 46.0 * 21 + 50, "rectangle traced around its pixels");
  });

  // ---- Multi-bone selection: transformBones, compensation, axis spaces (plugin 1.7.0) ----
  auto chainRig = [] {
    return Json::parse(R"({
      "schemaVersion": 1,
      "canvas": {"width": 400, "height": 400, "originX": 200, "originY": 380},
      "bones": [
        {"id": "root", "parent": null, "x": 200, "y": 300},
        {"id": "arm", "parent": "root", "x": 20, "y": -100, "rotation": 30, "length": 40},
        {"id": "fore", "parent": "arm", "x": 40, "y": 0, "rotation": 20, "length": 30},
        {"id": "hand", "parent": "fore", "x": 30, "y": 0},
        {"id": "leg", "parent": "root", "x": -30, "y": 20, "rotation": 90}
      ],
      "slots": [{"id": "sleeve", "bone": "arm", "attachment": "sleeve"}, {"id": "glove", "bone": "hand", "attachment": "glove"}],
      "attachments": [
        {"slot": "sleeve", "name": "sleeve", "imageAssetId": "img-sleeve", "width": 40, "height": 20, "x": 10, "y": 3, "rotation": 5},
        {"slot": "glove", "name": "glove", "imageAssetId": "img-glove", "width": 10, "height": 10}
      ],
      "skins": [{"id": "armor", "attachments": [{"slot": "sleeve", "name": "sleeve", "imageAssetId": "img-plate", "width": 40, "height": 20, "x": -4, "y": 1, "scaleX": 1.2, "scaleY": 1.2}]}],
      "clips": [{"id": "wave", "durationMs": 1000, "loop": true, "tracks": [
        {"target": "bone:fore", "property": "rotation", "keys": [{"timeMs": 0, "value": 20}, {"timeMs": 1000, "value": 60}]}]}]
    })");
  };
  auto angleOf = [](const Affine& m) { return std::atan2(m.b, m.a) * 180 / 3.14159265358979323846; };
  auto sameWorld = [](const Affine& a, const Affine& b, double eps) {
    return near(a.a, b.a, eps) && near(a.b, b.b, eps) && near(a.c, b.c, eps) && near(a.d, b.d, eps) && near(a.e, b.e, eps) && near(a.f, b.f, eps);
  };
  // World placement of attachment (slot, skin) in the setup pose: bone world * offset.
  auto placed = [](const Json& d, const std::string& slot, const std::string& skin) {
    const Json* list = &d["attachments"];
    if (!skin.empty()) for (const auto& s : d["skins"]) if (s["id"] == skin) list = &s["attachments"];
    std::string boneId;
    for (const auto& s : d["slots"]) if (s["id"] == slot) boneId = s["bone"];
    for (const auto& a : *list)
      if (a["slot"] == slot)
        return setupWorld(d).at(boneId) * Affine::compose(a.value("x", 0.0), a.value("y", 0.0), a.value("rotation", 0.0), a.value("scaleX", 1.0), a.value("scaleY", 1.0));
    return Affine{};
  };

  test("transformBones: translation moves only the topmost selected bones, in world px", [&] {
    const Json d = chainRig();
    auto r = applyEdit(d, {{"op", "transformBones"}, {"bones", {"fore", "arm"}}, {"delta", {{"x", 10}, {"y", -5}}}});
    const auto w0 = setupWorld(d), w1 = setupWorld(r.data);
    check(near(w1.at("arm").e - w0.at("arm").e, 10) && near(w1.at("arm").f - w0.at("arm").f, -5), "arm origin moved by the world delta");
    check(near(w1.at("fore").e - w0.at("fore").e, 10) && near(w1.at("fore").f - w0.at("fore").f, -5) && r.data["bones"][2] == d["bones"][2], "fore follows its parent, its own data untouched");
    check(r.extra["bones"] == Json::array({"arm"}) && r.affected == std::vector<std::string>{"bone:arm"}, "only arm changed");
    auto p = applyEdit(d, {{"op", "transformBones"}, {"bones", {"fore"}}, {"delta", {{"x", 3}}}, {"space", "parent"}});
    check(near(p.data["bones"][2]["x"].get<double>(), 43) && near(p.data["bones"][2]["y"].get<double>(), 0), "space parent adds to the local x/y");
    check(validateRig(r.data).empty(), "valid");
  });

  test("transformBones: rotation about each origin curls a selected chain (mirrors flip the sign)", [&] {
    Json d = chainRig();
    auto r = applyEdit(d, {{"op", "transformBones"}, {"bones", {"arm", "fore"}}, {"delta", {{"rotation", 15}}}});
    check(near(r.data["bones"][1]["rotation"].get<double>(), 45) && near(r.data["bones"][2]["rotation"].get<double>(), 35), "both +15 (local)");
    const auto w0 = setupWorld(d), w1 = setupWorld(r.data);
    check(near(w1.at("arm").e, w0.at("arm").e) && near(w1.at("arm").f, w0.at("arm").f), "arm turns about its own origin");
    check(near(angleOf(w1.at("fore")) - angleOf(w0.at("fore")), 30), "fore turned twice on screen (curl)");
    d["bones"][0]["scaleX"] = -1;  // mirrored root: the on-screen turn still follows the delta
    auto m = applyEdit(d, {{"op", "transformBones"}, {"bones", {"arm"}}, {"delta", {{"rotation", 15}}}});
    check(near(m.data["bones"][1]["rotation"].get<double>(), 15), "mirrored parent: local -15");
    const auto m0 = setupWorld(d), m1 = setupWorld(m.data);
    double turn = angleOf(m1.at("arm")) - angleOf(m0.at("arm"));
    while (turn > 180) turn -= 360;
    while (turn < -180) turn += 360;
    check(near(turn, 15, 1e-5), "mirrored parent: +15 on screen (" + std::to_string(turn) + ")");
    auto s = applyEdit(chainRig(), {{"op", "transformBones"}, {"bones", {"arm", "leg"}}, {"delta", {{"scaleX", 2}, {"scaleY", 0.5}}}});
    check(near(s.data["bones"][1]["scaleX"].get<double>(), 2) && near(s.data["bones"][4]["scaleY"].get<double>(), 0.5) && s.data["bones"][1]["x"] == 20, "scale about each origin: local factors, origins stay");
  });

  test("transformBones: centroid / point pivot turns the topmost bones as a group", [&] {
    const Json d = chainRig();
    const auto w0 = setupWorld(d);
    auto r = applyEdit(d, {{"op", "transformBones"}, {"bones", {"arm", "leg", "hand"}}, {"delta", {{"rotation", 30}}}, {"pivot", "centroid"}});
    const auto w1 = setupWorld(r.data);
    const double cx = (w0.at("arm").e + w0.at("leg").e + w0.at("hand").e) / 3, cy = (w0.at("arm").f + w0.at("leg").f + w0.at("hand").f) / 3;
    auto turned = [&](const std::string& id) {
      const double c = std::cos(3.14159265358979323846 / 6), s = std::sin(3.14159265358979323846 / 6), x = w0.at(id).e - cx, y = w0.at(id).f - cy;
      return near(w1.at(id).e, cx + c * x - s * y, 1e-5) && near(w1.at(id).f, cy + s * x + c * y, 1e-5) && near(angleOf(w1.at(id)) - angleOf(w0.at(id)), 30, 1e-5);
    };
    check(turned("arm") && turned("leg") && turned("hand"), "every selected bone turned rigidly about the centroid");
    check(r.data["bones"][3] == d["bones"][3] && r.extra["bones"] == Json::array({"arm", "leg"}), "hand (under selected arm) follows, not edited");
    auto p = applyEdit(d, {{"op", "transformBones"}, {"bones", {"arm"}}, {"delta", {{"rotation", 90}, {"scaleX", 2}, {"scaleY", 2}}}, {"pivot", {{"x", 200}, {"y", 300}}}});
    const auto wp = setupWorld(p.data);
    // arm origin (220, 200) relative to (200, 300) = (20, -100); scaled 2 and turned 90 deg clockwise -> (200, 40).
    check(near(wp.at("arm").e, 400, 1e-5) && near(wp.at("arm").f, 340, 1e-5) && near(p.data["bones"][1]["scaleX"].get<double>(), 2), "explicit pivot with scale");
  });

  test("transformBones in a clip keys the changed channel groups, setup untouched", [&] {
    const Json d = chainRig();
    auto r = applyEdit(d, {{"op", "transformBones"}, {"bones", {"fore"}}, {"delta", {{"rotation", 10}}}, {"clip", "wave"}, {"timeMs", 500}});
    const Json& tracks = r.data["clips"][0]["tracks"];
    check(tracks.size() == 1 && tracks[0]["keys"].size() == 3 && near(tracks[0]["keys"][1]["value"].get<double>(), 50), "rotation key = sampled 40 + 10");
    check(r.data["bones"] == d["bones"], "setup pose unchanged");
    auto g = applyEdit(d, {{"op", "transformBones"}, {"bones", {"arm", "leg"}}, {"delta", {{"rotation", 20}}}, {"pivot", "centroid"}, {"clip", "wave"}, {"timeMs", 0},
                           {"compensateBones", true}});
    size_t keys = 0;
    for (const auto& t : g.data["clips"][0]["tracks"]) keys += t["target"] != "bone:fore";
    check(keys == 6 && hasCode(g.diagnostics, "ANIM_COMPENSATE_SETUP_ONLY") && g.data["bones"] == d["bones"], "group turn keys x, y, rotation of both bones; compensation ignored with a warning");
    check(refused(d, {{"op", "transformBones"}, {"bones", {"fore"}}, {"delta", {{"x", 1}}}, {"clip", "wave"}, {"timeMs", 2000}}), "time out of range refused");
  });

  test("compensation keeps children and images in place (setBone and transformBones)", [&] {
    const Json d = chainRig();
    const auto w0 = setupWorld(d);
    auto b = applyEdit(d, {{"op", "setBone"}, {"bone", "arm"}, {"rotation", 75}, {"x", 5}, {"compensateBones", true}});
    auto w1 = setupWorld(b.data);
    check(sameWorld(w1.at("fore"), w0.at("fore"), 1e-6) && sameWorld(w1.at("hand"), w0.at("hand"), 1e-6) && !sameWorld(w1.at("arm"), w0.at("arm"), 1e-3), "setBone: children stay, the bone moved");
    check(!sameWorld(placed(b.data, "sleeve", ""), placed(d, "sleeve", ""), 1e-3), "images follow without compensateImages");
    auto i = applyEdit(d, {{"op", "setBone"}, {"bone", "arm"}, {"rotation", 75}, {"scaleX", 1.5}, {"scaleY", 1.5}, {"compensateImages", true}});
    check(sameWorld(placed(i.data, "sleeve", ""), placed(d, "sleeve", ""), 1e-6) && sameWorld(placed(i.data, "sleeve", "armor"), placed(d, "sleeve", "armor"), 1e-6), "compensateImages: default and skin attachments stay");
    check(!sameWorld(setupWorld(i.data).at("fore"), w0.at("fore"), 1e-3), "children follow without compensateBones");
    // A group turn + scale of arm and leg with both flags: every unselected child and image stays.
    auto t = applyEdit(d, {{"op", "transformBones"}, {"bones", {"arm", "leg"}}, {"delta", {{"rotation", -40}, {"scaleX", 1.25}, {"scaleY", 1.25}, {"x", 12}}}, {"pivot", "centroid"},
                           {"compensateBones", true}, {"compensateImages", true}});
    const auto w2 = setupWorld(t.data);
    check(!sameWorld(w2.at("arm"), w0.at("arm"), 1e-3) && sameWorld(w2.at("fore"), w0.at("fore"), 1e-6) && sameWorld(w2.at("hand"), w0.at("hand"), 1e-6) &&
          sameWorld(placed(t.data, "sleeve", ""), placed(d, "sleeve", ""), 1e-6) && sameWorld(placed(t.data, "sleeve", "armor"), placed(d, "sleeve", "armor"), 1e-6) &&
          sameWorld(placed(t.data, "glove", ""), placed(d, "glove", ""), 1e-6), "transformBones: world pose of children and images unchanged within 1e-6");
    check(validateRig(t.data).empty() && !hasCode(t.diagnostics, "ANIM_COMPENSATE_SKEW"), "valid, no skew (uniform scale)");
    // A selected child is moved on purpose: it is not compensated.
    auto s = applyEdit(d, {{"op", "transformBones"}, {"bones", {"arm", "fore"}}, {"delta", {{"rotation", 10}}}, {"compensateBones", true}});
    check(!sameWorld(setupWorld(s.data).at("fore"), w0.at("fore"), 1e-3) && sameWorld(setupWorld(s.data).at("hand"), w0.at("hand"), 1e-6), "selected fore turns, its unselected child hand stays");
    // Keys of a compensated child are absolute values of the old frame: warned.
    auto k = applyEdit(d, {{"op", "transformBones"}, {"bones", {"arm"}}, {"delta", {{"rotation", 10}}}, {"compensateBones", true}});
    check(hasCode(k.diagnostics, "ANIM_COMPENSATE_KEYS"), "fore has rotation keys: warning");
    auto n = applyEdit(d, {{"op", "setBone"}, {"bone", "arm"}, {"scaleX", 2}, {"compensateBones", true}});
    check(hasCode(n.diagnostics, "ANIM_COMPENSATE_SKEW"), "non-uniform scale under a turned child: skew warning");
  });

  test("axis spaces: world/parent values round-trip and setBone axes", [&] {
    const std::vector<Affine> parents = {Affine{}, Affine::compose(30, -12, 37, 1, 1), Affine::compose(-5, 8, -110, 2.5, 0.4), Affine::compose(10, 10, 200, -1, 1),
                                         Affine::compose(1, 2, 15, -0.7, -1.9)};
    const std::vector<Transform> locals = {{0, 0, 0, 1, 1}, {12, -7, 45, 1, 1}, {-40, 3, 395, 1.5, -2}, {0.5, 100, -170, 0.2, 0.2}};
    bool roundTrip = true, worldOk = true, parentOk = true;
    for (const auto& p : parents)
      for (const auto& t : locals)
        for (Axes a : {Axes::Local, Axes::Parent, Axes::World}) {
          const Transform shown = toAxes(t, p, a);
          const auto back = fromAxes(shown, p, a, t);
          roundTrip = roundTrip && back && near(back->x, t.x, 1e-9) && near(back->y, t.y, 1e-9) && near(back->rotation, t.rotation, 1e-9) && back->scaleX == t.scaleX && back->scaleY == t.scaleY;
          if (a == Axes::World) {
            const Affine w = p * Affine::compose(t.x, t.y, t.rotation, 1, 1);
            const double turn = std::fmod(std::fabs(shown.rotation - angleOf(w)), 360.0);
            worldOk = worldOk && near(shown.x, w.e, 1e-9) && near(shown.y, w.f, 1e-9) && (near(turn, 0, 1e-9) || near(turn, 360, 1e-9));
          }
          if (a == Axes::Parent && std::fabs(std::hypot(p.a, p.b) - 1) < 1e-12 && std::fabs(std::hypot(p.c, p.d) - 1) < 1e-12 && p.a * p.d - p.b * p.c > 0)
            parentOk = parentOk && near(shown.x, t.x, 1e-9) && near(shown.y, t.y, 1e-9) && near(shown.rotation, t.rotation, 1e-9);
        }
    check(roundTrip, "fromAxes(toAxes(t)) == t (local, parent, world; rotated, scaled, mirrored parents)");
    check(worldOk, "world = rig-space origin and on-screen angle");
    check(parentOk, "parent = local under an unscaled parent");
    check(!fromAxes({1, 2, 3, 1, 1}, Affine{0, 0, 0, 0, 0, 0}, Axes::World, {}).has_value(), "singular parent");
    check(parseAxes("world") == Axes::World && !parseAxes("screen"), "parseAxes");
    const Json d = chainRig();
    auto r = applyEdit(d, {{"op", "setBone"}, {"bone", "fore"}, {"axes", "world"}, {"x", 150}, {"y", 120}, {"rotation", 0}});
    const Affine w = setupWorld(r.data).at("fore");
    check(near(w.e, 150, 1e-5) && near(w.f, 120, 1e-5) && near(angleOf(w), 0, 1e-5), "setBone axes world: origin and angle land in rig space");
    auto px = applyEdit(d, {{"op", "setBone"}, {"bone", "fore"}, {"axes", "parent"}, {"x", 50}});
    check(near(px.data["bones"][2]["x"].get<double>(), 50) && near(px.data["bones"][2]["y"].get<double>(), 0), "setBone axes parent (unscaled parent) = local");
    check(refused(d, {{"op", "setBone"}, {"bone", "fore"}, {"axes", "screen"}, {"x", 1}}), "bad axes refused");
  });

  test("transformBones refusals", [&] {
    const Json d = chainRig();
    check(refused(d, {{"op", "transformBones"}, {"delta", {{"x", 1}}}}) && refused(d, {{"op", "transformBones"}, {"bones", Json::array()}, {"delta", {{"x", 1}}}}), "bones required");
    check(refused(d, {{"op", "transformBones"}, {"bones", {"nope"}}, {"delta", {{"x", 1}}}}), "unknown bone");
    check(refused(d, {{"op", "transformBones"}, {"bones", {"arm"}}, {"delta", {{"scaleX", 0}}}}), "zero scale");
    check(refused(d, {{"op", "transformBones"}, {"bones", {"arm"}}, {"delta", {{"rotation", 1}}}, {"pivot", "middle"}}), "bad pivot");
    check(refused(d, {{"op", "transformBones"}, {"bones", {"arm"}}, {"delta", {{"x", 1}}}, {"space", "local"}}), "bad space");
    check(refused(d, {{"op", "transformBones"}, {"bones", {"arm"}}, {"delta", Json::object()}}), "empty delta");
    auto b = applyEdit(d, {{"op", "batch"}, {"operations", {{{"op", "transformBones"}, {"bones", {"arm"}}, {"delta", {{"x", 1}}}}, {{"op", "transformBones"}, {"bones", {"leg"}}, {"delta", {{"x", 1}}}}}}});
    check(b.extra["bones"] == Json::array({"leg"}) && b.affected.size() == 2, "batchable");
  });

  // ---- Clip events (plugin 1.8.0) ----
  test("event definitions validate names, payload types and key names", [] {
    check(validateRig(eventRig()).empty(), "event fixture validates");
    check(validateRig(rigData()).empty(), "keys without definitions stay valid (any name)");
    auto with = [](const std::function<void(Json&)>& edit) { Json d = eventRig(); edit(d); return validateRig(d); };
    check(hasCode(with([](Json& d) { d["clips"][3]["events"][0]["name"] = "jump"; }), "ANIM_EVENT_UNKNOWN"), "undefined key name once definitions exist");
    check(hasCode(with([](Json& d) { d["events"][0]["int"] = 1.5; }), "ANIM_EVENT_PAYLOAD"), "int must be an integer");
    check(hasCode(with([](Json& d) { d["events"][0]["int"] = int64_t(1) << 40; }), "ANIM_EVENT_PAYLOAD"), "int is 32-bit");
    check(hasCode(with([](Json& d) { d["events"][1]["float"] = "2"; }), "ANIM_EVENT_PAYLOAD"), "float must be a number");
    check(hasCode(with([](Json& d) { d["clips"][3]["events"][1]["string"] = 3; }), "ANIM_EVENT_PAYLOAD"), "key string override must be a string");
    check(hasCode(with([](Json& d) { d["events"][0]["audio"] = ""; }), "ANIM_EVENT_AUDIO"), "audio is an asset id");
    check(hasCode(with([](Json& d) { d["events"][0]["volume"] = 1.5; }), "ANIM_EVENT_PAYLOAD"), "volume 0..1");
    check(hasCode(with([](Json& d) { d["events"][0]["balance"] = -2; }), "ANIM_EVENT_PAYLOAD"), "balance -1..1");
    check(hasCode(with([](Json& d) { d["events"][1]["volume"] = 0.2; }), "ANIM_EVENT_AUDIO_UNUSED"), "volume without audio warns");
    check(hasCode(with([](Json& d) { d["events"][1]["name"] = "step"; }), "ANIM_DUP_ID"), "duplicate definition");
    check(hasCode(with([](Json& d) { d["events"][1]["name"] = "a b"; }), "ANIM_EVENT_NAME"), "definition name format");
    check(hasCode(with([](Json& d) { d["clips"][3]["events"][1]["timeMs"] = 401; }), "ANIM_EVENT_TIME"), "key past the clip end");
    check(hasCode(with([](Json& d) { d["clips"][3]["events"][1]["timeMs"] = 1.5; }), "ANIM_EVENT_TIME"), "fractional key time");
    const auto dup = with([](Json& d) { d["clips"][3]["events"].push_back({{"timeMs", 0}, {"name", "step"}}); });
    check(hasCode(dup, "ANIM_EVENT_DUPLICATE") && !hasCode(dup, "ANIM_EVENT_UNKNOWN"), "same key twice only warns");
  });

  test("event payload: definition defaults, key overrides, notification JSON", [] {
    const Rig r = parseRig("hero", eventRig());
    const auto& walk = r.clips[size_t(r.clipIndex("walk"))];
    check(walk.events[0].payload == Json({{"int", 1}, {"string", "left"}, {"audio", "sfx-step"}, {"volume", 0.5}, {"balance", -0.25}}), "defaults of the definition");
    check(walk.events[1].payload == Json({{"float", 2.5}, {"string", "fist"}}), "key string added to a float definition");
    check(walk.events[2].payload["int"] == 2 && walk.events[2].payload["string"] == "right" && walk.events[2].payload["audio"] == "sfx-step", "key overrides int/string, audio stays");
    Instance inst(std::make_shared<const Rig>(r));
    inst.play(0, "walk");
    inst.takeNotifications();
    inst.update(250);
    std::vector<Json> events;
    for (const auto& n : inst.takeNotifications()) if (n.kind == "event") events.push_back(n.toJson());
    check(events.size() == 3, "0, 100 and 200 ms fire once each");
    check(events.size() == 3 && events[0] == Json({{"kind", "event"}, {"track", 0}, {"clip", "walk"}, {"serial", 1}, {"name", "step"}, {"int", 1}, {"string", "left"},
                                                   {"audio", "sfx-step"}, {"volume", 0.5}, {"balance", -0.25}}), "notification carries the payload");
    check(events.size() == 3 && events[2]["int"] == 2 && events[2]["string"] == "right", "override reaches the notification");
    // Without definitions/payloads the plugin 1.7 notification shape is unchanged.
    Instance old(rig());
    old.play(0, "idle");
    old.takeNotifications();
    old.update(1);
    const auto n = old.takeNotifications();
    check(n.size() == 1 && n[0].toJson() == Json({{"kind", "event"}, {"track", 0}, {"clip", "idle"}, {"serial", 1}, {"name", "breath"}}), "backward compatible JSON");
    // Scrubbing (seek) never fires events.
    Instance scrub(std::make_shared<const Rig>(r));
    scrub.play(0, "walk");
    scrub.takeNotifications();
    scrub.seek(0, 350);
    check(scrub.takeNotifications().empty(), "seek fires nothing");
  });

  test("event notifications are deterministic across frame pacing", [] {
    auto run = [](const std::vector<double>& pattern) {
      Instance inst(std::make_shared<const Rig>(parseRig("hero", eventRig())));
      inst.play(0, "walk");
      std::vector<std::string> log;
      double t = 0; size_t i = 0;
      while (t < 2000) {
        const double step = pattern[i++ % pattern.size()];
        inst.update(step); t += step;
        for (const auto& n : inst.takeNotifications()) log.push_back(n.toJson().dump());
      }
      return log;
    };
    const auto a = run({31.25}), b = run({7.8125}), c = run({7.8125, 23.4375, 15.625, 15.625});
    check(a == b && b == c, "same payload notifications at 32/128 fps and uneven frames");
    size_t steps = 0;
    for (const auto& line : a) steps += line.find("\"name\":\"step\"") != std::string::npos;
    check(steps == 10, "2 steps per 400 ms loop over 2000 ms");
  });

  test("edit event definitions: add, set, rename (keys follow), delete", [&] {
    // The first definition also defines the names keys already use: the rig stays valid.
    auto a = applyEdit(rigData(), {{"op", "addEventDef"}, {"name", "step"}, {"int", 3}, {"audio", "sfx-step"}, {"volume", 0.8}});
    check(a.extra["defined"] == Json::array({"breath", "peak", "done"}), "used names defined");
    check(a.data["events"].size() == 4 && a.data["events"][3] == Json({{"name", "step"}, {"int", 3}, {"audio", "sfx-step"}, {"volume", 0.8}}), "definition stored");
    check(!hasErrors(a.diagnostics), "valid after the first definition");
    check(refused(a.data, {{"op", "addEventDef"}, {"name", "step"}}), "duplicate refused");
    check(refused(a.data, {{"op", "addEventDef"}, {"name", "bad name"}}), "name format refused");
    check(refused(a.data, {{"op", "addEventDef"}, {"name", "x"}, {"int", 1.5}}), "int payload type refused");
    check(refused(a.data, {{"op", "addEventDef"}, {"name", "x"}, {"volume", 2}}), "volume range refused");
    auto s = applyEdit(a.data, {{"op", "setEventDef"}, {"event", "step"}, {"int", nullptr}, {"string", "L"}, {"volume", 1}, {"balance", 0.5}});
    check(s.data["events"][3] == Json({{"name", "step"}, {"string", "L"}, {"audio", "sfx-step"}, {"balance", 0.5}}), "null removes, volume 1 is the default");
    auto keyed = applyEdit(s.data, {{"op", "setEventKey"}, {"clip", "idle"}, {"timeMs", 500}, {"name", "step"}});
    keyed = applyEdit(keyed.data, {{"op", "setEventKey"}, {"clip", "joy"}, {"timeMs", 100}, {"name", "step"}, {"int", 7}});
    auto rn = applyEdit(keyed.data, {{"op", "renameEventDef"}, {"event", "step"}, {"name", "foot"}});
    check(rn.extra["renamedKeys"] == 2 && rn.data["events"][3]["name"] == "foot", "renamed with its keys");
    check(rn.data["clips"][0]["events"][1] == Json({{"timeMs", 500}, {"name", "foot"}}) && rn.data["clips"][2]["events"][0] == Json({{"timeMs", 100}, {"name", "foot"}, {"int", 7}}),
          "keys in every clip follow, payload kept");
    check(refused(rn.data, {{"op", "renameEventDef"}, {"event", "foot"}, {"name", "peak"}}), "rename onto another definition refused");
    check(refused(rn.data, {{"op", "deleteEventDef"}, {"event", "foot"}}), "delete refused while keys use it");
    auto del = applyEdit(rn.data, {{"op", "deleteEventDef"}, {"event", "foot"}, {"force", true}});
    check(del.extra["removedKeys"] == 2 && del.data["clips"][0]["events"].size() == 1 && del.data["clips"][2]["events"].size() == 2, "force removes its keys");
    auto empty = applyEdit(rigData(), {{"op", "addEventDef"}, {"name", "solo"}});
    for (const char* n : {"breath", "peak", "done"}) empty = applyEdit(empty.data, {{"op", "deleteEventDef"}, {"event", n}, {"force", true}});
    empty = applyEdit(empty.data, {{"op", "deleteEventDef"}, {"event", "solo"}});
    check(!empty.data.contains("events") && !empty.data["clips"][0].contains("events"), "last definition removes the list (keys free again)");
  });

  test("edit event keys: set, override, rename, move, delete", [&] {
    const Json d = eventRig();
    auto add = applyEdit(d, {{"op", "setEventKey"}, {"clip", "walk"}, {"timeMs", 300}, {"name", "hit"}, {"float", 4}});
    const Json& ev = add.data["clips"][3]["events"];
    check(ev.size() == 4 && ev[3] == Json({{"timeMs", 300}, {"name", "hit"}, {"float", 4}}), "new key sorted by time with its override");
    check(add.extra["keys"] == Json::array({{{"target", "event"}, {"property", "hit"}, {"timeMs", 300}}}), "extra.keys ref");
    check(refused(d, {{"op", "setEventKey"}, {"clip", "walk"}, {"timeMs", 50}, {"name", "jump"}}), "undefined name refused");
    check(refused(d, {{"op", "setEventKey"}, {"clip", "walk"}, {"timeMs", 401}, {"name", "hit"}}), "time range refused");
    auto edit = applyEdit(d, {{"op", "setEventKey"}, {"clip", "walk"}, {"timeMs", 200}, {"name", "step"}, {"int", nullptr}, {"string", "R"}});
    check(edit.data["clips"][3]["events"][2] == Json({{"timeMs", 200}, {"name", "step"}, {"string", "R"}}), "null drops the int override");
    auto renamed = applyEdit(d, {{"op", "setEventKey"}, {"clip", "walk"}, {"timeMs", 100}, {"name", "hit"}, {"newName", "step"}});
    check(renamed.data["clips"][3]["events"][1]["name"] == "step" && renamed.data["clips"][3]["events"][1]["string"] == "fist", "one key renamed, payload kept");
    check(refused(d, {{"op", "setEventKey"}, {"clip", "walk"}, {"timeMs", 100}, {"name", "hit"}, {"newName", "jump"}}), "rename to an undefined name refused");
    auto mv = applyEdit(d, {{"op", "moveEventKey"}, {"clip", "walk"}, {"name", "step"}, {"fromMs", 200}, {"toMs", 50}});
    check(mv.data["clips"][3]["events"][1] == Json({{"timeMs", 50}, {"name", "step"}, {"int", 2}, {"string", "right"}}), "moved and re-sorted");
    check(refused(d, {{"op", "moveEventKey"}, {"clip", "walk"}, {"name", "step"}, {"fromMs", 200}, {"toMs", 0}}), "same-name collision refused");
    auto shared = applyEdit(d, {{"op", "moveEventKey"}, {"clip", "walk"}, {"name", "hit"}, {"fromMs", 100}, {"toMs", 200}});
    check(shared.data["clips"][3]["events"].size() == 3, "different events may share a time");
    check(refused(d, {{"op", "moveEventKey"}, {"clip", "walk"}, {"name", "hit"}, {"fromMs", 150}, {"toMs", 200}}), "missing key refused");
    auto rm = applyEdit(d, {{"op", "deleteEventKey"}, {"clip", "walk"}, {"name", "hit"}, {"timeMs", 100}});
    check(rm.data["clips"][3]["events"].size() == 2, "deleted");
    auto last = applyEdit(rigData(), {{"op", "deleteEventKey"}, {"clip", "idle"}, {"name", "breath"}, {"timeMs", 0}});
    check(!last.data["clips"][0].contains("events"), "empty events list removed");
    check(applyEdit(rigData(), {{"op", "setEventKey"}, {"clip", "idle"}, {"timeMs", 300}, {"name", "anything"}}).data["clips"][0]["events"].size() == 2, "no definitions: any valid name");
  });

  test("multi-key ops take event refs with track keys", [&] {
    Json d = eventRig();
    d["clips"][3]["tracks"] = Json::parse(R"([{"target": "bone:body", "property": "x", "keys": [{"timeMs": 0, "value": 0}, {"timeMs": 200, "value": 5}]}])");
    const Json mixed = Json::array({{{"target", "bone:body"}, {"property", "x"}, {"timeMs", 0}}, {{"target", "event"}, {"property", "step"}, {"timeMs", 0}},
                                    {{"target", "event"}, {"property", "hit"}, {"timeMs", 100}}});
    auto mv = applyEdit(d, {{"op", "moveKeys"}, {"clip", "walk"}, {"keys", mixed}, {"deltaMs", 50}});
    const Json& ev = mv.data["clips"][3]["events"];
    check(ev[0] == Json({{"timeMs", 50}, {"name", "step"}}) && ev[1]["timeMs"] == 150 && ev[2]["timeMs"] == 200, "events moved with the bone key, sorted");
    check(mv.data["clips"][3]["tracks"][0]["keys"][0]["timeMs"] == 50 && mv.extra["keys"].size() == 3, "bone key moved too, refs reported");
    check(refused(d, {{"op", "moveKeys"}, {"clip", "walk"}, {"keys", mixed}, {"deltaMs", 200}}), "step 0 -> 200 hits the other step key");
    check(refused(d, {{"op", "moveKeys"}, {"clip", "walk"}, {"keys", mixed}, {"deltaMs", -1}}), "range refused");
    auto clamp = applyEdit(d, {{"op", "moveKeys"}, {"clip", "walk"}, {"keys", Json::array({mixed[2]})}, {"deltaMs", 900}, {"clamp", true}});
    check(clamp.extra["deltaMs"] == 300 && clamp.data["clips"][3]["events"][2]["timeMs"] == 400, "clamp stops at the clip end");
    const Json steps = Json::array({{{"target", "event"}, {"property", "step"}, {"timeMs", 0}}, {{"target", "event"}, {"property", "step"}, {"timeMs", 200}}});
    auto sc = applyEdit(d, {{"op", "scaleKeys"}, {"clip", "walk"}, {"keys", steps}, {"factor", 1.5}});
    check(sc.data["clips"][3]["events"][2] == Json({{"timeMs", 300}, {"name", "step"}, {"int", 2}, {"string", "right"}}), "scaled about the first key");
    auto del = applyEdit(d, {{"op", "deleteKeys"}, {"clip", "walk"}, {"keys", mixed}});
    check(del.data["clips"][3]["events"].size() == 1 && del.data["clips"][3]["tracks"][0]["keys"].size() == 1, "mixed delete");
    auto board = applyEdit(d, {{"op", "copyKeys"}, {"clip", "walk"}, {"keys", Json::array({mixed[2], steps[1]})}}).extra["clipboard"];
    check(board["fromMs"] == 100 && board["keys"].size() == 2 && board["keys"][1] == Json({{"target", "event"}, {"property", "step"}, {"offsetMs", 100}, {"int", 2}, {"string", "right"}}),
          "clipboard carries event payload overrides");
    auto pasted = applyEdit(d, {{"op", "pasteKeys"}, {"clip", "idle"}, {"timeMs", 600}, {"keys", board["keys"]}});
    check(pasted.data["clips"][0]["events"].size() == 3 && pasted.data["clips"][0]["events"][2] == Json({{"timeMs", 700}, {"name", "step"}, {"int", 2}, {"string", "right"}}), "pasted into another clip");
    // Pasting over the same key replaces its payload.
    auto over = applyEdit(d, {{"op", "pasteKeys"}, {"clip", "walk"}, {"timeMs", 0}, {"keys", Json::array({{{"target", "event"}, {"property", "step"}, {"offsetMs", 0}, {"float", 1}}})}});
    check(over.data["clips"][3]["events"].size() == 3 && over.data["clips"][3]["events"][0] == Json({{"timeMs", 0}, {"name", "step"}, {"float", 1}}), "same key overwritten");
    check(refused(d, {{"op", "setInterp"}, {"clip", "walk"}, {"target", "event"}, {"property", "step"}, {"timeMs", 0}, {"interp", "step"}}), "event keys have no interpolation");
    // Batch of event ops = one edit (LLM/MCP path), undo is the host's.
    auto b = applyEdit(rigData(), {{"op", "batch"}, {"operations", {{{"op", "addEventDef"}, {"name", "cue"}, {"string", "x"}},
                                                                     {{"op", "setEventKey"}, {"clip", "joy"}, {"timeMs", 300}, {"name", "cue"}}}}});
    check(b.data["clips"][2]["events"].size() == 3 && !hasErrors(b.diagnostics), "batchable");
    check(applyEdit(rigData(), {{"op", "setClip"}, {"clip", "joy"}, {"durationMs", 300}, {"trim", true}}).data["clips"][2]["events"].size() == 1, "clip trim drops later events");
  });

  test("host op: update notifications carry the event payload", [] {
    Host host;
    Json replies = Json::array();
    for (const auto& op : eventScript()) replies.push_back(host.handle(op));
    size_t events = 0, audio = 0;
    for (const auto& r : replies)
      if (r.contains("notifications") && r["notifications"].contains("a"))
        for (const auto& n : r["notifications"]["a"]) if (n["kind"] == "event") { events++; audio += n.contains("audio"); }
    check(events == 14 && audio == 8, "walk loops + joy events through the generic update op");
  });

  // ---- Large rigs (plugin 1.9.0): the benchmark character, compact frames, batched validation.
  test("stress rig validates cleanly; parseValidatedRig builds the same rig", [] {
    const Json data = cutout_stress::rig();
    check(validateRig(data).empty(), "no diagnostics at all");
    const auto a = std::make_shared<const Rig>(parseRig("s", data)), b = std::make_shared<const Rig>(parseValidatedRig("s", data));
    check(validateController(cutout_stress::controller(), a.get()).empty(), "controller fits");
    check(a->bones.size() == 200 && a->slots.size() == 150 && a->clips.size() == 30 && a->skins.size() == 4 && a->ik.size() == 6, "counts");
    Instance x(a, parseController("c", cutout_stress::controller())), y(b, parseController("c", cutout_stress::controller()));
    x.update(333); y.update(333);
    const auto p = x.drawList(), q = y.drawList();
    bool same = p.size() == q.size() && p.size() > 140;
    for (size_t i = 0; same && i < p.size(); i++) same = p[i].imageAssetId == q[i].imageAssetId && p[i].positions == q[i].positions && near(p[i].matrix.e, q[i].matrix.e, 0);
    check(same, "identical draw lists");
  });

  test("appendFixed: compact frame numbers", [] {
    auto f = [](double v, int d) { std::string s; appendFixed(s, v, d); return s; };
    check(f(0, 3) == "0" && f(-0.0, 3) == "0" && f(-0.0004, 3) == "0" && f(-0.0006, 3) == "-0.001", "zero and sign");
    check(f(1.5, 3) == "1.5" && f(84.00000001, 3) == "84" && f(123.4567, 3) == "123.457" && f(0.1, 6) == "0.1" && f(-2.0000006, 6) == "-2.000001", "rounding, trailing zeros");
    check(f(std::nan(""), 3) == "0" && f(1e300, 3) == "1000000000000", "non-finite and clamped");
    check(f(0.001, 3) == "0.001" && f(10.25, 1) == "10.3" && f(7, 0) == "7", "leading zeros, few decimals");
  });

  test("host op: compact frames (format 2) carry the documented items, deltas and the mesh table", [] {
    Host host;
    const Json rig = cutout_stress::rig();
    Json init = host.handle({{"op", "init"}, {"frameFormats", {2}}, {"documents", {{{"type", "rig"}, {"id", "stress"}, {"data", rig}}, {{"type", "controller"}, {"id", "c"}, {"data", cutout_stress::controller()}}}}});
    check(init["ok"] == true && init["frameFormat"] == 2 && init["meshes"].size() == 40, "negotiated with the topology of every mesh");
    check(!host.handle({{"op", "init"}, {"documents", Json::array()}}).contains("frameFormat"), "a host that does not ask keeps the documented format");
    init = host.handle({{"op", "init"}, {"frameFormats", {1, 2}}, {"documents", {{{"type", "rig"}, {"id", "stress"}, {"data", rig}}, {{"type", "controller"}, {"id", "c"}, {"data", cutout_stress::controller()}}}}});
    check(host.handle({{"op", "mount"}, {"instance", "a"}, {"element", {{"type", "character"}, {"ref", "c"}, {"width", 400}, {"height", 300}, {"props", Json::object()}}}})["ok"] == true, "mounted");
    host.handle({{"op", "update"}, {"dtMs", 250}});
    const Json full = Json::parse(host.requestText(Json({{"op", "frame"}, {"instance", "a"}, {"format", 2}}).dump()));
    const Json documented = host.handle({{"op", "frame"}, {"instance", "a"}});
    check(full["format"] == 2 && full["items"].size() == documented["items"].size(), "same item count");
    bool same = true;
    size_t meshes = 0;
    for (size_t k = 0; k < full["items"].size(); k++) {
      const Json& e = full["items"][k];
      const Json& d = documented["items"][k];
      same = same && e[0] == d["imageAssetId"] && near(e[7].get<double>(), d["width"].get<double>(), 6e-4) && near(e[9].get<double>(), d["alpha"].get<double>(), 6e-5);
      for (int i = 0; i < 6; i++) same = same && near(e[1 + i].get<double>(), d["matrix"][i].get<double>(), i < 4 ? 6e-7 : 6e-4);
      if (!d.contains("mesh")) { same = same && e.size() == 10; continue; }
      meshes++;
      const Json& t = init["meshes"][e[11].get<size_t>()];
      same = same && e.size() == 13 && e[10].is_null() && t["indices"] == d["mesh"]["indices"] && t["uvs"] == d["mesh"]["uvs"] && e[12].size() == d["mesh"]["positions"].size();
      for (size_t v = 0; same && v < e[12].size(); v++) same = near(e[12][v].get<double>(), d["mesh"]["positions"][v].get<double>(), 6e-4);
    }
    check(same && meshes == 40, "every compact item matches its documented item (1/1000 px)");
    // Nothing advanced: every item is "0" against that frame; another `since` gets everything again.
    const uint64_t serial = full["frame"].get<uint64_t>();
    const Json still = Json::parse(host.requestText(Json({{"op", "frame"}, {"instance", "a"}, {"format", 2}, {"since", serial}}).dump()));
    bool zeros = still["items"].size() == full["items"].size();
    for (const auto& e : still["items"]) zeros = zeros && e == 0;
    check(zeros && still["frame"].get<uint64_t>() > serial, "unchanged items are 0");
    const Json stale = host.handle({{"op", "frame"}, {"instance", "a"}, {"format", 2}, {"since", serial}});
    check(stale["items"][0].is_array(), "a since that is not the last frame gets every item");
    host.handle({{"op", "update"}, {"dtMs", 40}});
    const Json moved = host.handle({{"op", "frame"}, {"instance", "a"}, {"format", 2}, {"since", stale["frame"]}});
    size_t changed = 0;
    for (const auto& e : moved["items"]) changed += e.is_array();
    check(changed > 0 && changed <= moved["items"].size(), "an advanced pose sends the changed items");
    const Json table = host.handle({{"op", "meshes"}, {"ids", {3, 999}}});
    check(table["meshes"].size() == 1 && table["meshes"][0]["id"] == 3 && table["meshEpoch"] == init["meshEpoch"], "meshes op: known ids only");
    host.handle({{"op", "loadRig"}, {"id", "other"}, {"data", rigData()}});
    check(host.handle({{"op", "meshes"}})["meshEpoch"] != init["meshEpoch"], "loading a rig starts a new mesh epoch");
  });

  test("atlas regions (plugin 1.9.0): validation and draw items naming the page sub-image", [] {
    Json d = rigData();
    d["attachments"][0]["atlas"] = {{"page", "cutout-atlas-1"}, {"x", 10}, {"y", 20}, {"width", 60}, {"height", 100}, {"trim", {{"x", 10}, {"y", 15}, {"width", 60}, {"height", 100}}}};
    d["attachments"][1]["atlas"] = {{"page", "cutout-atlas-1"}, {"x", 80}, {"y", 20}, {"width", 60}, {"height", 60}};
    check(validateRig(d).empty(), "valid regions");
    auto bad = d;
    bad["attachments"][0]["atlas"]["trim"]["width"] = 75;  // 10 + 75 > 80
    check(hasCode(validateRig(bad), "ANIM_ATLAS"), "trim outside the picture");
    bad = d; bad["attachments"][1]["atlas"].erase("page");
    check(hasCode(validateRig(bad), "ANIM_ATLAS"), "page required");
    bad = d; bad["attachments"][1]["atlas"]["width"] = 0;
    check(hasCode(validateRig(bad), "ANIM_ATLAS"), "positive size");
    bad = meshParityRig(); bad["attachments"][0]["atlas"] = {{"page", "p"}, {"x", 0}, {"y", 0}, {"width", 10}, {"height", 10}, {"trim", {{"x", 0}, {"y", 0}, {"width", 5}, {"height", 5}}}};
    check(hasCode(validateRig(bad), "ANIM_ATLAS"), "meshes keep their whole picture");
    Instance plain(std::make_shared<const Rig>(parseRig("a", rigData()))), packed(std::make_shared<const Rig>(parseRig("a", d)));
    const auto a = plain.drawList(), b = packed.drawList();
    check(b[0].imageAssetId == "cutout-atlas-1" && b[0].hasRegion && b[0].region[0] == 10 && b[0].region[3] == 100 && b[0].width == 60 && b[0].height == 100, "page + region, trimmed size");
    double ax, ay, bx, by;
    a[0].matrix.apply(10, 15, ax, ay);
    b[0].matrix.apply(0, 0, bx, by);
    check(near(ax, bx) && near(ay, by), "the kept rectangle stays where it was");
    check(b[1].hasRegion && b[1].width == a[1].width && near(b[1].matrix.e, a[1].matrix.e) && b[2].imageAssetId == a[2].imageAssetId && !b[2].hasRegion, "untrimmed region keeps its rectangle; unpacked parts unchanged");
    const Json j = drawItemJson(b[0]);
    check(j["region"] == Json::array({10, 20, 60, 100}) && !drawItemJson(a[0]).contains("region"), "documented items gain region only when packed");
  });

  test("host op: validate checks every controller of a rig in one request", [] {
    Host host;
    Json bad = controllerData();
    bad["defaultClip"] = "nope";
    const Json r = host.handle({{"op", "validate"}, {"rig", rigData()}, {"rigPointer", "/data"}, {"controllers", {{{"data", controllerData()}, {"pointer", "/data"}}, {{"data", bad}, {"pointer", "/data"}}}}});
    check(r["ok"] == true && r["diagnostics"].empty() && r["controllerDiagnostics"].size() == 2, "rig once, one list per controller");
    check(r["controllerDiagnostics"][0].empty() && r["controllerDiagnostics"][1][0]["code"] == "ANIM_CLIP_UNKNOWN" && r["controllerDiagnostics"][1][0]["pointer"] == "/data/defaultClip", "clip names checked against the rig");
  });

  clipseq::run();
  ikgolden::run();
  ikphys::run();
  golden115::run();
  xform::run();
  boxes::run();
  weights::run();

  std::cout << (checks - failures) << "/" << checks << " checks passed\n";
  return failures ? 1 : 0;
}
