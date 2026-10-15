#include "cutout/runtime.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace cutout {
namespace {
constexpr double MAX_DT_MS = 10000;
constexpr double PI = 3.14159265358979323846, DEG = 180 / PI, RAD = PI / 180;
constexpr double IK_EPS = 1e-9;  // shorter distances/links are degenerate: no direction to solve for
constexpr int64_t MAX_COMPLETES_PER_UPDATE = 64;
const char* thenName(Then t) { return t == Then::Hold ? "hold" : t == Then::Stop ? "stop" : "default"; }
Then parseThen(const std::string& s) { return s == "hold" ? Then::Hold : s == "stop" ? Then::Stop : Then::Default; }
double lerp(double a, double b, double t) { return a + (b - a) * t; }
double& field(Transform& t, Property p) {
  switch (p) {
    case Property::X: return t.x; case Property::Y: return t.y; case Property::Rotation: return t.rotation;
    case Property::ScaleX: return t.scaleX; default: return t.scaleY;
  }
}
const Json& arrayOr(const Json& obj, const char* key) {
  static const Json empty = Json::array();
  return obj.is_object() && obj.contains(key) && obj.at(key).is_array() ? obj.at(key) : empty;
}
}  // namespace

Json Notification::toJson() const {
  Json j{{"kind", kind}, {"track", track}, {"clip", clip}, {"serial", serial}};
  if (kind == "complete") j["loop"] = loop;
  if (kind == "event") {
    j["name"] = name;
    for (auto it = payload.begin(); it != payload.end(); ++it) j[it.key()] = it.value();
  }
  return j;
}

double sampleTrack(const Track& track, double t, double fallback) {
  const auto& keys = track.keys;
  if (keys.empty()) return fallback;
  if (t <= double(keys.front().timeMs)) return keys.front().value;
  if (t >= double(keys.back().timeMs)) return keys.back().value;
  // Keys are validated as strictly increasing; the key's interp governs the segment after it.
  auto next = std::upper_bound(keys.begin(), keys.end(), t, [](double v, const Key& k) { return v < double(k.timeMs); });
  const Key& b = *next; const Key& a = *(next - 1);
  if (a.interp == Interp::Step) return a.value;
  const double u = (t - double(a.timeMs)) / double(b.timeMs - a.timeMs);
  // Bezier eases only the fraction; endpoints stay exactly the key values.
  if (a.interp == Interp::Bezier) return a.value + (b.value - a.value) * evalCurve(a.curve, u);
  return lerp(a.value, b.value, u);
}
std::vector<double> sampleDeform(const Track& track, double t) {
  const auto& keys = track.keys;
  if (keys.empty()) return {};
  if (t <= double(keys.front().timeMs)) return keys.front().offsets;
  if (t >= double(keys.back().timeMs)) return keys.back().offsets;
  auto next = std::upper_bound(keys.begin(), keys.end(), t, [](double v, const Key& k) { return v < double(k.timeMs); });
  const Key& b = *next; const Key& a = *(next - 1);
  if (a.interp == Interp::Step) return a.offsets;
  const double u = (t - double(a.timeMs)) / double(b.timeMs - a.timeMs);
  // One eased fraction for every vertex: the whole shape follows the key's curve together.
  const double f = a.interp == Interp::Bezier ? evalCurve(a.curve, u) : u;
  std::vector<double> out(std::min(a.offsets.size(), b.offsets.size()));
  for (size_t i = 0; i < out.size(); i++) out[i] = a.offsets[i] + (b.offsets[i] - a.offsets[i]) * f;
  return out;
}
static std::optional<std::string> sampleAttachment(const Track& track, double t) {
  const auto& keys = track.keys;
  const Key* chosen = &keys.front();
  for (const auto& k : keys) { if (double(k.timeMs) <= t) chosen = &k; else break; }
  return chosen->name;
}

/** Expression keyed by `clip` at time t into `out` (-1 = keyed null). False when the clip does not key it. */
static bool clipExpression(const Rig& rig, const Clip& clip, double t, int& out) {
  for (const auto& tr : clip.tracks) {
    if (!tr.rig || tr.property != Property::Expression || tr.keys.empty()) continue;
    const auto name = sampleAttachment(tr, t);
    out = name ? rig.expressionIndex(*name) : -1;
    return true;
  }
  return false;
}
void applyExpression(const Rig& rig, int index, Pose& pose) {
  if (index < 0 || index >= int(rig.expressions.size())) return;
  const Expression& e = rig.expressions[size_t(index)];
  for (const auto& [slot, name] : e.slots) if (slot >= 0 && size_t(slot) < pose.attachments.size()) pose.attachments[size_t(slot)] = name;
  for (const auto& o : e.bones) {
    if (o.bone < 0 || size_t(o.bone) >= pose.bones.size()) continue;
    Transform& t = pose.bones[size_t(o.bone)];
    if (o.x) t.x = *o.x;
    if (o.y) t.y = *o.y;
    if (o.rotation) t.rotation = *o.rotation;
    if (o.scaleX) t.scaleX = *o.scaleX;
    if (o.scaleY) t.scaleY = *o.scaleY;
  }
  pose.expression = index;
}
static Pose setupPose(const Rig& rig) {
  Pose pose;
  for (const auto& b : rig.bones) pose.bones.push_back(b.setup);
  for (const auto& s : rig.slots) { pose.attachments.push_back(s.attachment); pose.alpha.push_back(s.alpha); }
  for (const auto& k : rig.ik) { pose.ikMix.push_back(k.mix); pose.ikBend.push_back(k.bendPositive); }
  return pose;
}
static void computeWorld(const Rig& rig, Pose& pose) {
  pose.world.assign(rig.bones.size(), Affine{});
  for (int i : rig.evalOrder) {
    const auto& t = pose.bones[i];
    const Affine local = Affine::compose(t.x, t.y, t.rotation, t.scaleX, t.scaleY);
    const int p = rig.bones[i].parentIndex;
    pose.world[i] = p < 0 ? local : pose.world[p] * local;
  }
}
static double clipTime(const Clip& clip, double timeMs, bool loop) {
  const double d = double(clip.durationMs);
  if (!loop) return std::clamp(timeMs, 0.0, d);
  const double m = std::fmod(timeMs, d);
  return m < 0 ? m + d : m;
}
static void applyClip(const Rig& rig, const Clip& clip, double t, Pose& pose) {
  for (const auto& tr : clip.tracks) {
    if (tr.targetIndex < 0) continue;
    if (tr.ik) {
      const size_t i = size_t(tr.targetIndex);
      if (i >= pose.ikMix.size()) continue;
      if (tr.property == Property::Mix) pose.ikMix[i] = sampleTrack(tr, t, pose.ikMix[i]);
      else if (!tr.keys.empty()) pose.ikBend[i] = sampleTrack(tr, t, pose.ikBend[i]) >= 0.5;  // step keys: 1/0
      continue;
    }
    if (tr.bone) { auto& value = field(pose.bones[tr.targetIndex], tr.property); value = sampleTrack(tr, t, value); }
    else if (tr.property == Property::Deform) { if (!tr.keys.empty()) pose.deform[{tr.targetIndex, tr.attachment}] = sampleDeform(tr, t); }
    else if (tr.property == Property::Alpha) pose.alpha[tr.targetIndex] = sampleTrack(tr, t, pose.alpha[tr.targetIndex]);
    else if (!tr.keys.empty()) pose.attachments[tr.targetIndex] = sampleAttachment(tr, t);
  }
  (void)rig;
}
static Pose blend(const Pose& a, const Pose& b, double alpha) {
  Pose out = b;
  for (size_t i = 0; i < out.bones.size(); i++) {
    auto& o = out.bones[i]; const auto& x = a.bones[i];
    o.x = lerp(x.x, o.x, alpha); o.y = lerp(x.y, o.y, alpha); o.rotation = lerp(x.rotation, o.rotation, alpha);
    o.scaleX = lerp(x.scaleX, o.scaleX, alpha); o.scaleY = lerp(x.scaleY, o.scaleY, alpha);
  }
  for (size_t i = 0; i < out.alpha.size(); i++) out.alpha[i] = lerp(a.alpha[i], out.alpha[i], alpha);
  for (size_t i = 0; i < out.ikMix.size() && i < a.ikMix.size(); i++) out.ikMix[i] = lerp(a.ikMix[i], out.ikMix[i], alpha);
  // Deform offsets cross-fade per vertex; a side that does not key a mesh contributes no offset.
  for (const auto& [key, from] : a.deform) {
    auto& to = out.deform[key];
    if (to.empty()) to.assign(from.size(), 0.0);
    for (size_t i = 0; i < to.size(); i++) to[i] = lerp(i < from.size() ? from[i] : 0.0, to[i], alpha);
  }
  for (auto& [key, to] : out.deform)
    if (!a.deform.count(key)) for (double& v : to) v *= alpha;
  // Attachments (and the IK bend direction) cannot interpolate: switch halfway through the mix.
  if (alpha < 0.5) { out.attachments = a.attachments; out.ikBend = a.ikBend; }
  return out;
}

// ---- IK constraints (plugin 1.3.0) ----
/** `from` turned towards `to` (degrees) the short way round, by `mix`: mix 1 lands on `to` mod 360. */
static double blendAngle(double from, double to, double mix) {
  double d = std::fmod(to - from, 360.0);
  if (d > 180) d -= 360;
  else if (d < -180) d += 360;
  return from + d * mix;
}
/**
 * One-bone aim: rotate the bone in its parent space so its local direction `aimAxis` (default +x)
 * points at the target. Exact for any parent transform (an affine map keeps directions parallel).
 * The bone's own scale acts before its rotation, so the axis is measured after it: a negative
 * scaleX flips +x, a non-uniform scale bends an oblique axis. aimAxis 0 keeps the 1.3.0 formula
 * bit for bit.
 */
static bool solveAim(const Rig& rig, Pose& pose, const IkConstraint& k, double tx, double ty, double mix) {
  const int b = k.bones[0], parent = rig.bones[size_t(b)].parentIndex;
  const auto inv = (parent < 0 ? Affine{} : pose.world[size_t(parent)]).inverse();
  if (!inv) return false;
  double lx, ly;
  inv->apply(tx, ty, lx, ly);
  Transform& t = pose.applied[size_t(b)];
  const double dx = lx - t.x, dy = ly - t.y;
  if (std::hypot(dx, dy) < IK_EPS) return false;
  const double axis = k.aimAxis == 0 ? (t.scaleX < 0 ? PI : 0) : std::atan2(t.scaleY * std::sin(k.aimAxis * RAD), t.scaleX * std::cos(k.aimAxis * RAD));
  t.rotation = blendAngle(t.rotation, (std::atan2(dy, dx) - axis) * DEG, mix);
  return true;
}
/**
 * Two-bone limb (law of cosines) in the upper bone's parent space, where its local rotation acts
 * rigidly: exact for any ancestor transform. Links: upper origin -> child origin (l1, through the
 * upper bone's scale) and child origin -> effector (l2, the tip bone's origin or the lower bone's
 * own length tip, through the child's scale). A mirrored upper bone (one negative scale) reverses the child's rotation sense; an
 * upper bone with non-uniform scale uses the geometric mean for l2 (approximate, like Spine).
 * Unreachable targets clamp the cosine: the limb points straight at it (too far) or folds (too
 * close). bendPositive picks the side on screen: a mirrored parent space flips it back.
 */
static bool solveTwoBone(const Rig& rig, Pose& pose, const IkConstraint& k, double tx, double ty, double mix, bool bendPositive) {
  const int up = k.bones[0], low = k.bones[1], parent = rig.bones[size_t(up)].parentIndex;
  const Affine pw = parent < 0 ? Affine{} : pose.world[size_t(parent)];
  const auto inv = pw.inverse();
  if (!inv) return false;
  double lx, ly;
  inv->apply(tx, ty, lx, ly);
  Transform& tu = pose.applied[size_t(up)];
  Transform& tl = pose.applied[size_t(low)];
  const double dx = lx - tu.x, dy = ly - tu.y, dist = std::hypot(dx, dy);
  if (dist < IK_EPS) return false;
  const double fx = tu.scaleX < 0 ? -1 : 1, fy = tu.scaleY < 0 ? -1 : 1, sense = fx * fy;
  const double mean = std::sqrt(std::fabs(tu.scaleX * tu.scaleY));
  const double c1x = tu.scaleX * tl.x, c1y = tu.scaleY * tl.y;
  // Effector in the lower bone's space: the tip bone's origin, else its own tip (length; 0 = origin).
  const double ex = k.tip >= 0 ? pose.applied[size_t(k.tip)].x : k.tipLength, ey = k.tip >= 0 ? pose.applied[size_t(k.tip)].y : 0;
  const double c2x = mean * fx * tl.scaleX * ex, c2y = mean * fy * tl.scaleY * ey;
  const double l1 = std::hypot(c1x, c1y), l2 = std::hypot(c2x, c2y);
  const double th1 = std::atan2(c1y, c1x), th2 = std::atan2(c2y, c2x), aim = std::atan2(dy, dx);
  const double b0 = sense * tl.rotation * RAD;
  double a = 0, b = b0;
  if (l1 < IK_EPS && l2 < IK_EPS) return false;
  if (l2 < IK_EPS) a = aim - th1;            // no effector: the child origin aims at the target
  else if (l1 < IK_EPS) a = aim - th2 - b0;  // child on the upper origin: one rotation reaches
  else {
    const double side = (bendPositive ? 1 : -1) * (pw.a * pw.d - pw.b * pw.c < 0 ? -1 : 1);
    const double cosine = std::clamp((dist * dist - l1 * l1 - l2 * l2) / (2 * l1 * l2), -1.0, 1.0);
    const double bend = side * std::acos(cosine);
    const double first = aim - std::atan2(l2 * std::sin(bend), l1 + l2 * std::cos(bend));
    a = first - th1;
    b = bend + th1 - th2;
  }
  tu.rotation = blendAngle(tu.rotation, a * DEG, mix);
  tl.rotation = blendAngle(tl.rotation, sense * b * DEG, mix);
  return true;
}
void applyIk(const Rig& rig, Pose& pose) {
  if (rig.ik.empty() || pose.world.size() != rig.bones.size() || pose.bones.size() != rig.bones.size()) return;
  pose.applied = pose.bones;
  for (int ci : rig.ikOrder) {
    const IkConstraint& k = rig.ik[size_t(ci)];
    const double mix = size_t(ci) < pose.ikMix.size() ? std::clamp(pose.ikMix[size_t(ci)], 0.0, 1.0) : k.mix;
    const bool bend = size_t(ci) < pose.ikBend.size() ? pose.ikBend[size_t(ci)] != 0 : k.bendPositive;
    if (!(mix > 0) || k.target < 0 || k.bones.empty() || k.bones[0] < 0) continue;
    const double tx = pose.world[size_t(k.target)].e, ty = pose.world[size_t(k.target)].f;
    const bool solved = k.bones.size() == 1 ? solveAim(rig, pose, k, tx, ty, mix) : solveTwoBone(rig, pose, k, tx, ty, mix, bend);
    if (!solved) continue;
    // Later constraints see this one's result (a spine solved before an arm moves the shoulder).
    for (int i : k.affected) {
      const auto& t = pose.applied[size_t(i)];
      const Affine local = Affine::compose(t.x, t.y, t.rotation, t.scaleX, t.scaleY);
      const int p = rig.bones[size_t(i)].parentIndex;
      pose.world[size_t(i)] = p < 0 ? local : pose.world[size_t(p)] * local;
    }
  }
}

Pose samplePose(const Rig& rig, int clip, double timeMs, const Look& look) {
  Pose pose = setupPose(rig);
  pose.skin = look.skin >= 0 && look.skin < int(rig.skins.size()) ? look.skin : -1;
  int expression = look.expression;
  if (clip >= 0 && clip < int(rig.clips.size())) {
    const double t = clipTime(rig.clips[clip], timeMs, rig.clips[clip].loop);
    applyClip(rig, rig.clips[clip], t, pose);
    clipExpression(rig, rig.clips[clip], t, expression);
  }
  // The sampled clip plays as the base track, so the expression applies after it (Instance::pose rule).
  if (look.expressions) applyExpression(rig, expression, pose);
  computeWorld(rig, pose);
  if (look.ik) applyIk(rig, pose);
  return pose;
}

std::vector<Diagnostic> validateController(const Json& data, const Rig* rig) {
  std::vector<Diagnostic> out;
  auto error = [&](const std::string& p, const std::string& code, const std::string& m) { out.push_back({code, "error", p, m}); };
  if (!data.is_object()) { error("", "ANIM_STRUCTURE", "컨트롤러 데이터는 객체여야 합니다."); return out; }
  if (data.value("schemaVersion", 0) != SCHEMA_VERSION) error("/schemaVersion", "ANIM_SCHEMA_VERSION", "지원하는 컨트롤러 schemaVersion은 1입니다.");
  auto clipKnown = [&](const Json& v) { return v.is_string() && (!rig || rig->clipIndex(v.get<std::string>()) >= 0); };
  auto skinKnown = [&](const Json& v) { return v.is_string() && !v.get<std::string>().empty() && (!rig || rig->skinIndex(v.get<std::string>()) >= 0); };
  // null = "no expression" wherever an expression may be named.
  auto expressionKnown = [&](const Json& v) { return v.is_null() || (v.is_string() && !v.get<std::string>().empty() && (!rig || rig->expressionIndex(v.get<std::string>()) >= 0)); };
  if (data.contains("defaultSkin") && !skinKnown(data.at("defaultSkin"))) error("/defaultSkin", "ANIM_SKIN_UNKNOWN", "기본 스킨이 리그에 없습니다.");
  if (data.contains("defaultExpression") && !expressionKnown(data.at("defaultExpression"))) error("/defaultExpression", "ANIM_EXPRESSION_UNKNOWN", "기본 표정이 리그에 없습니다.");
  if (!data.contains("rig") || !data.at("rig").is_string()) error("/rig", "ANIM_RIG_MISSING", "컨트롤러가 사용할 리그를 지정하세요.");
  if (!data.contains("defaultClip") || !clipKnown(data.at("defaultClip"))) error("/defaultClip", "ANIM_CLIP_UNKNOWN", "기본 대기 클립이 리그에 없습니다.");
  std::set<int> layerTracks;
  const auto& layers = arrayOr(data, "layers");
  for (size_t i = 0; i < layers.size(); i++) {
    const std::string p = "/layers/" + std::to_string(i);
    if (!layers[i].contains("clip") || !clipKnown(layers[i].at("clip"))) error(p + "/clip", "ANIM_CLIP_UNKNOWN", "레이어 클립이 리그에 없습니다.");
    const int track = layers[i].value("track", 0);
    if (track < 1 || track > 15) error(p + "/track", "ANIM_TRACK_RANGE", "레이어 트랙은 1..15입니다(0은 기본 동작).");
    else if (!layerTracks.insert(track).second) error(p + "/track", "ANIM_DUP_TRACK", "같은 트랙에 레이어가 중복됩니다.");
  }
  std::set<std::string> ids;
  const auto& bindings = arrayOr(data, "bindings");
  for (size_t i = 0; i < bindings.size(); i++) {
    const auto& b = bindings[i];
    const std::string p = "/bindings/" + std::to_string(i);
    const std::string id = b.value("id", std::string());
    if (id.empty()) error(p + "/id", "ANIM_ID_MISSING", "연결 ID가 필요합니다.");
    else if (!ids.insert(id).second) error(p + "/id", "ANIM_DUP_ID", "중복된 연결 ID: " + id);
    if (b.value("signal", std::string()).empty()) error(p + "/signal", "ANIM_SIGNAL", "연결할 게임 신호를 지정하세요.");
    // clip is optional when the binding only changes the skin and/or the expression.
    const bool look = b.contains("skin") || b.contains("expression");
    if (b.contains("clip") ? !clipKnown(b.at("clip")) : !look) error(p + "/clip", "ANIM_CLIP_UNKNOWN", look || b.contains("clip") ? "연결 클립이 리그에 없습니다." : "연결할 클립(또는 스킨·표정)을 지정하세요.");
    if (b.contains("skin") && !skinKnown(b.at("skin"))) error(p + "/skin", "ANIM_SKIN_UNKNOWN", "연결 스킨이 리그에 없습니다.");
    if (b.contains("expression") && !expressionKnown(b.at("expression"))) error(p + "/expression", "ANIM_EXPRESSION_UNKNOWN", "연결 표정이 리그에 없습니다.");
    const int track = b.value("track", 0);
    if (track < 0 || track > 15) error(p + "/track", "ANIM_TRACK_RANGE", "트랙은 0..15입니다.");
    if (b.contains("speed") && (!b.at("speed").is_number() || !std::isfinite(b.at("speed").get<double>()) || b.at("speed").get<double>() <= 0 || b.at("speed").get<double>() > 10))
      error(p + "/speed", "ANIM_SPEED", "속도는 0보다 크고 10 이하입니다.");
    const std::string then = b.value("then", std::string("default"));
    if (then != "default" && then != "hold" && then != "stop") error(p + "/then", "ANIM_THEN", "종료 후 동작은 default, hold, stop 중 하나입니다.");
  }
  return out;
}

Controller parseController(const std::string& id, const Json& data) {
  for (const auto& d : validateController(data, nullptr)) if (d.severity == "error") throw std::runtime_error(d.code + " " + d.pointer + ": " + d.message);
  Controller c; c.id = id; c.rig = data.at("rig"); c.defaultClip = data.at("defaultClip"); c.characterId = data.value("characterId", std::string());
  c.defaultSkin = data.value("defaultSkin", std::string());
  if (data.contains("defaultExpression") && data.at("defaultExpression").is_string()) c.defaultExpression = data.at("defaultExpression").get<std::string>();
  for (const auto& l : arrayOr(data, "layers")) c.layers.push_back({l.value("track", 1), l.at("clip").get<std::string>()});
  for (const auto& b : arrayOr(data, "bindings")) {
    Binding x; x.id = b.at("id"); x.signal = b.at("signal"); x.clip = b.value("clip", std::string()); x.track = b.value("track", 0);
    if (b.contains("skin") && b.at("skin").is_string()) x.skin = b.at("skin").get<std::string>();
    x.setsExpression = b.contains("expression");
    if (x.setsExpression && b.at("expression").is_string()) x.expression = b.at("expression").get<std::string>();
    if (b.contains("loop") && b.at("loop").is_boolean()) x.loop = b.at("loop").get<bool>();
    x.speed = b.value("speed", 1.0); x.then = parseThen(b.value("then", std::string("default")));
    if (b.contains("match") && b.at("match").is_object()) for (auto it = b.at("match").begin(); it != b.at("match").end(); ++it) if (it.value().is_string()) x.match[it.key()] = it.value().get<std::string>();
    for (const auto& s : arrayOr(b, "stopOn")) if (s.is_string()) x.stopOn.push_back(s.get<std::string>());
    c.bindings.push_back(x);
  }
  return c;
}

Instance::Instance(std::shared_ptr<const Rig> rig, std::optional<Controller> controller) : rig_(std::move(rig)), controller_(std::move(controller)) {
  if (!rig_) throw std::runtime_error("rig is required");
  startDefaults();
  if (controller_) {
    setSkin(controller_->defaultSkin);
    setExpression(controller_->defaultExpression.empty() ? std::nullopt : std::optional<std::string>(controller_->defaultExpression));
  }
  pending_.clear();  // construction is not an observable transition
}
bool Instance::setSkin(const std::string& id) {
  const int i = id.empty() ? -1 : rig_->skinIndex(id);
  if (!id.empty() && i < 0) return false;
  skin_ = i;
  return true;
}
bool Instance::setExpression(const std::optional<std::string>& id) {
  const int i = !id || id->empty() ? -1 : rig_->expressionIndex(*id);
  if (id && !id->empty() && i < 0) return false;
  expression_ = i;
  return true;
}
std::string Instance::skin() const { return skin_ >= 0 ? rig_->skins[size_t(skin_)].id : std::string(); }
std::optional<std::string> Instance::expression() const { return expression_ >= 0 ? std::optional<std::string>(rig_->expressions[size_t(expression_)].id) : std::nullopt; }
void Instance::startDefaults() {
  if (!controller_) return;
  if (rig_->clipIndex(controller_->defaultClip) >= 0) play(0, controller_->defaultClip, std::nullopt, 1, Then::Hold, 0);
  for (const auto& layer : controller_->layers) if (rig_->clipIndex(layer.clip) >= 0) play(layer.track, layer.clip, std::nullopt, 1, Then::Hold, 0);
}

uint64_t Instance::play(int track, const std::string& clipId, std::optional<bool> loop, double speed, Then then, int64_t mixMs) {
  const int clip = rig_->clipIndex(clipId);
  if (clip < 0 || track < 0 || track > 15) return 0;
  auto& state = tracks_[track];
  if (state.from) { pending_.push_back({"end", track, rig_->clips[state.from->clip].id, "", state.from->serial}); state.from.reset(); }
  int64_t mix = 0;
  if (state.current) {
    const auto& old = rig_->clips[state.current->clip];
    pending_.push_back({"interrupt", track, old.id, "", state.current->serial});
    mix = mixMs >= 0 ? mixMs : rig_->mixDuration(old.id, clipId);
    if (mix > 0) { state.from = state.current; state.mixElapsed = 0; state.mixDuration = double(mix); }
    else pending_.push_back({"end", track, old.id, "", state.current->serial});
  }
  Entry entry;
  entry.clip = clip; entry.loop = loop.value_or(rig_->clips[clip].loop); entry.speed = speed > 0 ? speed : 1;
  entry.then = then; entry.serial = nextSerial_++;
  state.current = entry; state.stopOn.clear();
  pending_.push_back({"start", track, clipId, "", entry.serial});
  return entry.serial;
}

void Instance::stop(int track, int64_t mixMs) {
  auto it = tracks_.find(track);
  if (it == tracks_.end() || !it->second.current) return;
  auto& state = it->second;
  const auto& old = rig_->clips[state.current->clip];
  pending_.push_back({"interrupt", track, old.id, "", state.current->serial});
  if (state.from) pending_.push_back({"end", track, rig_->clips[state.from->clip].id, "", state.from->serial});
  const int64_t mix = mixMs >= 0 ? mixMs : rig_->defaultMixMs;
  state.from.reset();
  if (mix > 0) { state.from = state.current; state.mixElapsed = 0; state.mixDuration = double(mix); }
  else pending_.push_back({"end", track, old.id, "", state.current->serial});
  state.current.reset(); state.stopOn.clear();
  if (!state.from) tracks_.erase(it);
}

void Instance::finish(int track, TrackState& state) {
  Entry& entry = *state.current;
  if (entry.then == Then::Hold) { entry.ended = true; return; }
  if (entry.then == Then::Default && track == 0 && controller_ && rig_->clipIndex(controller_->defaultClip) >= 0) {
    play(0, controller_->defaultClip, std::nullopt, 1, Then::Hold, -1);
    return;
  }
  stop(track, -1);
}

std::vector<std::string> Instance::signal(const std::string& name, const std::map<std::string, std::string>& input) {
  std::vector<std::string> matched;
  auto params = input;
  if (controller_ && !controller_->characterId.empty()) {
    const bool self = (params.count("speakerId") && params.at("speakerId") == controller_->characterId) ||
                      (params.count("characterId") && params.at("characterId") == controller_->characterId);
    params["self"] = self ? "true" : "false";
  }
  // stopOn first so "dialogueEnded" can end a talk loop before another binding starts on the same track.
  std::vector<int> stopping;
  for (auto& [track, state] : tracks_) if (state.current && std::find(state.stopOn.begin(), state.stopOn.end(), name) != state.stopOn.end()) stopping.push_back(track);
  for (int track : stopping) { auto& state = tracks_[track]; state.current->then = state.current->then == Then::Hold ? Then::Stop : state.current->then; finish(track, state); }
  if (!controller_) return matched;
  // First match wins per track for clips; skin/expression-only bindings never take a track,
  // and the first matching binding that names a skin (an expression) sets it.
  std::set<int> used;
  bool skinSet = false, expressionSet = false;
  for (const auto& b : controller_->bindings) {
    const bool hasClip = !b.clip.empty();
    if (b.signal != name || (hasClip && used.count(b.track))) continue;
    bool ok = true;
    for (const auto& [k, v] : b.match) { auto it = params.find(k); if (it == params.end() || it->second != v) { ok = false; break; } }
    if (!ok) continue;
    bool applied = false;
    if (hasClip && play(b.track, b.clip, b.loop, b.speed, b.then, -1)) {
      auto& state = tracks_[b.track];
      state.current->binding = b.id; state.stopOn = b.stopOn;
      used.insert(b.track);
      applied = true;
    }
    if (b.skin && !skinSet) { skinSet = true; applied = setSkin(*b.skin) || applied; }
    if (b.setsExpression && !expressionSet) { expressionSet = true; applied = setExpression(b.expression) || applied; }
    if (applied) matched.push_back(b.id);
  }
  return matched;
}

void Instance::advance(int track, TrackState& state, double dt) {
  if (state.from) {
    state.from->timeMs += dt * state.from->speed;
    state.mixElapsed += dt;
    if (state.mixElapsed >= state.mixDuration) {
      pending_.push_back({"end", track, rig_->clips[state.from->clip].id, "", state.from->serial});
      state.from.reset();
    }
  }
  if (!state.current || state.current->ended) return;
  Entry& entry = *state.current;
  const Clip& clip = rig_->clips[entry.clip];
  const double d = double(clip.durationMs);
  const double prev = entry.timeMs;
  double cur = prev + dt * entry.speed;
  if (!entry.loop) cur = std::min(cur, d);
  entry.timeMs = cur;
  // Events fire once per occurrence in [prev, cur); only the current (incoming) entry fires.
  const uint64_t serial = entry.serial;
  if (entry.loop) {
    const int64_t first = int64_t(std::floor(prev / d)), last = int64_t(std::floor(cur / d));
    for (int64_t k = first; k <= last && k - first <= MAX_COMPLETES_PER_UPDATE; k++)
      for (const auto& e : clip.events) {
        const double occ = double(k) * d + double(e.timeMs);
        if (e.timeMs < clip.durationMs && occ >= prev && occ < cur) pending_.push_back({"event", track, clip.id, e.name, serial, 0, e.payload});
      }
    for (int64_t k = first + 1; k <= last && k - first <= MAX_COMPLETES_PER_UPDATE; k++) pending_.push_back({"complete", track, clip.id, "", serial, k - 1});
    entry.loops = last;
  } else {
    for (const auto& e : clip.events) {
      const double occ = double(e.timeMs);
      if (occ >= prev && (occ < cur || (cur >= d && occ == d && prev < d))) pending_.push_back({"event", track, clip.id, e.name, serial, 0, e.payload});
    }
    if (prev < d && cur >= d) { pending_.push_back({"complete", track, clip.id, "", serial, 0}); finish(track, state); }
  }
}

void Instance::update(double dtMs) {
  if (paused_ || !std::isfinite(dtMs)) return;
  const double dt = std::clamp(dtMs, 0.0, MAX_DT_MS) * timeScale_;
  if (dt <= 0) return;
  std::vector<int> order;
  for (const auto& [track, _] : tracks_) order.push_back(track);
  for (int track : order) {
    auto it = tracks_.find(track);
    if (it != tracks_.end()) advance(track, it->second, dt);
  }
  for (auto it = tracks_.begin(); it != tracks_.end();) it = !it->second.current && !it->second.from ? tracks_.erase(it) : std::next(it);
}

void Instance::seek(int track, double timeMs) {
  auto it = tracks_.find(track);
  if (it == tracks_.end() || !it->second.current || !std::isfinite(timeMs)) return;
  auto& e = *it->second.current;
  const double d = double(rig_->clips[e.clip].durationMs);
  e.timeMs = e.loop ? std::max(0.0, timeMs) : std::clamp(timeMs, 0.0, d);
  e.ended = !e.loop && e.timeMs >= d && e.then == Then::Hold;
  it->second.from.reset();
}

void Instance::sampleClip(const Entry& entry, Pose& pose, std::vector<char>*, double, bool) const {
  const Clip& clip = rig_->clips[entry.clip];
  applyClip(*rig_, clip, clipTime(clip, entry.timeMs, entry.loop), pose);
}

int Instance::activeExpression() const {
  int expression = expression_;
  for (const auto& [track, state] : tracks_) {  // ascending: the highest track keying it wins
    const Entry* e = state.current ? &*state.current : nullptr;
    // Like attachments, a cross-fade switches a keyed expression at its halfway point.
    if (state.from && state.mixDuration > 0 && state.mixElapsed / state.mixDuration < 0.5) e = &*state.from;
    if (!e) continue;
    const Clip& clip = rig_->clips[size_t(e->clip)];
    clipExpression(*rig_, clip, clipTime(clip, e->timeMs, e->loop), expression);
  }
  return expression;
}

Pose Instance::pose() const {
  Pose pose = setupPose(*rig_);
  pose.skin = skin_;
  const int expression = activeExpression();
  bool expressed = false;
  for (const auto& [track, state] : tracks_) {
    // The expression is a base-layer override: after track 0, before layer tracks 1..15, so a
    // blink layer keying the eyes still blinks over any expression.
    if (!expressed && track >= 1) { applyExpression(*rig_, expression, pose); expressed = true; }
    Pose to = pose;
    if (state.current) sampleClip(*state.current, to, nullptr, 1, true);
    if (state.from && state.mixDuration > 0) {
      Pose from = pose;
      sampleClip(*state.from, from, nullptr, 1, true);
      pose = blend(from, to, std::clamp(state.mixElapsed / state.mixDuration, 0.0, 1.0));
    } else pose = to;
  }
  if (!expressed) applyExpression(*rig_, expression, pose);
  // Order of operations (every runtime and the editor): tracks -> expression -> world -> IK.
  computeWorld(*rig_, pose);
  applyIk(*rig_, pose);
  return pose;
}

bool ikAimDirection(const Rig& rig, const Pose& pose, int ik, double& dx, double& dy) {
  if (ik < 0 || size_t(ik) >= rig.ik.size() || rig.ik[size_t(ik)].bones.size() != 1) return false;
  const int b = rig.ik[size_t(ik)].bones[0];
  if (b < 0 || size_t(b) >= pose.world.size()) return false;
  const Affine& w = pose.world[size_t(b)];
  const double a = rig.ik[size_t(ik)].aimAxis * RAD, cx = std::cos(a), cy = std::sin(a);
  dx = w.a * cx + w.c * cy;
  dy = w.b * cx + w.d * cy;
  const double len = std::hypot(dx, dy);
  if (len < IK_EPS) return false;
  dx /= len;
  dy /= len;
  return true;
}

// ---- Mesh attachments (plugin 1.4.0) ----
Affine attachmentPlacement(const Attachment& a) {
  const auto& o = a.offset;
  return Affine::compose(o.x, o.y, o.rotation, o.scaleX, o.scaleY) * Affine::compose(-a.pivotX * a.width, -a.pivotY * a.height, 0, 1, 1);
}
const std::vector<double>* deformOf(const Pose& pose, int slot, const Attachment& a) {
  auto it = pose.deform.find({slot, a.name});
  return it != pose.deform.end() && it->second.size() == a.vertices.size() ? &it->second : nullptr;
}
std::vector<double> meshPositions(const Rig& rig, const Pose& pose, int slot, const Attachment& a) {
  std::vector<double> out;
  if (!a.mesh || slot < 0 || size_t(slot) >= rig.slots.size() || rig.slots[size_t(slot)].boneIndex < 0 || pose.world.size() != rig.bones.size()) return out;
  const std::vector<double>* d = deformOf(pose, slot, a);
  const Affine rigid = pose.world[size_t(rig.slots[size_t(slot)].boneIndex)] * attachmentPlacement(a);
  // world(bone) · bind per influencing bone, once per call rather than once per vertex.
  std::vector<Affine> skin(a.binds.size());
  for (size_t j = 0; j < a.binds.size(); j++) skin[j] = pose.world[size_t(a.binds[j].first)] * a.binds[j].second;
  out.resize(a.vertices.size());
  for (size_t v = 0; v < a.vertexCount(); v++) {
    const double x = a.vertices[2 * v] + (d ? (*d)[2 * v] : 0), y = a.vertices[2 * v + 1] + (d ? (*d)[2 * v + 1] : 0);
    if (a.weights.empty() || v >= a.weights.size() || a.weights[v].empty()) { rigid.apply(x, y, out[2 * v], out[2 * v + 1]); continue; }
    double sx = 0, sy = 0;
    for (const auto& w : a.weights[v]) {
      double px, py;
      skin[size_t(w.bind)].apply(x, y, px, py);
      sx += w.weight * px; sy += w.weight * py;
    }
    out[2 * v] = sx; out[2 * v + 1] = sy;
  }
  return out;
}
Affine meshVertexMatrix(const Rig& rig, const Pose& pose, int slot, const Attachment& a, size_t v) {
  if (slot < 0 || size_t(slot) >= rig.slots.size() || rig.slots[size_t(slot)].boneIndex < 0 || pose.world.size() != rig.bones.size()) return {};
  const Affine rigid = pose.world[size_t(rig.slots[size_t(slot)].boneIndex)] * attachmentPlacement(a);
  if (a.weights.empty() || v >= a.weights.size() || a.weights[v].empty()) return rigid;
  // A weighted sum of affine maps (weights sum to 1) is itself the vertex's affine map.
  Affine m{0, 0, 0, 0, 0, 0};
  for (const auto& w : a.weights[v]) {
    const Affine s = pose.world[size_t(a.binds[size_t(w.bind)].first)] * a.binds[size_t(w.bind)].second;
    m.a += w.weight * s.a; m.b += w.weight * s.b; m.c += w.weight * s.c; m.d += w.weight * s.d; m.e += w.weight * s.e; m.f += w.weight * s.f;
  }
  return m;
}

std::vector<DrawItem> Instance::drawList() const {
  const Pose pose = this->pose();
  std::vector<DrawItem> out;
  const Affine origin = Affine::compose(-rig_->originX, -rig_->originY, 0, 1, 1);
  for (size_t i = 0; i < rig_->slots.size(); i++) {
    const auto& slot = rig_->slots[i];
    if (!pose.attachments[i] || pose.alpha[i] <= 0 || slot.boneIndex < 0) continue;
    const Attachment* att = rig_->attachment(slot.id, *pose.attachments[i], pose.skin);
    if (!att) continue;
    const Affine m = origin * pose.world[slot.boneIndex] * attachmentPlacement(*att);
    out.push_back({slot.id, att->imageAssetId, m, att->width, att->height, std::clamp(pose.alpha[i], 0.0, 1.0)});
    DrawItem& item = out.back();
    item.source = att;
    if (!att->mesh) continue;
    item.positions = meshPositions(*rig_, pose, int(i), *att);
    for (size_t v = 0; v + 1 < item.positions.size(); v += 2) { item.positions[v] -= rig_->originX; item.positions[v + 1] -= rig_->originY; }  // = origin
    item.uvs = att->uvs;
    item.indices = att->triangles;
  }
  return out;
}

std::vector<Notification> Instance::takeNotifications() {
  std::vector<Notification> out(pending_.begin(), pending_.end());
  pending_.clear();
  return out;
}

Json Instance::saveState() const {
  Json tracks = Json::array();
  for (const auto& [track, state] : tracks_) {
    if (!state.current) continue;
    const auto& e = *state.current;
    tracks.push_back({{"track", track}, {"clip", rig_->clips[e.clip].id}, {"timeMs", e.loop ? 0.0 : e.timeMs}, {"loop", e.loop},
                      {"speed", e.speed}, {"then", thenName(e.then)}, {"binding", e.binding}, {"stopOn", state.stopOn}, {"ended", e.ended}});
  }
  const auto expression = this->expression();
  return {{"version", 1}, {"timeScale", timeScale_}, {"paused", paused_}, {"tracks", tracks},
          {"skin", skin()}, {"expression", expression ? Json(*expression) : Json(nullptr)}};
}

void Instance::restoreState(const Json& state) {
  if (!state.is_object() || state.value("version", 0) != 1) throw std::runtime_error("지원하지 않는 애니메이션 상태 버전입니다.");
  tracks_.clear(); pending_.clear();
  timeScale_ = std::max(0.0, state.value("timeScale", 1.0)); paused_ = state.value("paused", false);
  for (const auto& t : arrayOr(state, "tracks")) {
    const int clip = rig_->clipIndex(t.value("clip", std::string()));
    const int track = t.value("track", -1);
    if (clip < 0 || track < 0 || track > 15) continue;  // clip removed since save: fall back below
    Entry e; e.clip = clip; e.loop = t.value("loop", true); e.speed = std::max(1e-3, t.value("speed", 1.0));
    e.then = parseThen(t.value("then", std::string("default"))); e.binding = t.value("binding", std::string());
    const double d = double(rig_->clips[clip].durationMs);
    // Policy: looping (decorative) clips restart at 0; one-shot clips resume at their saved time.
    e.timeMs = e.loop ? 0.0 : std::clamp(t.value("timeMs", 0.0), 0.0, d);
    e.ended = t.value("ended", false); e.serial = nextSerial_++;
    auto& ts = tracks_[track]; ts.current = e;
    for (const auto& s : arrayOr(t, "stopOn")) if (s.is_string()) ts.stopOn.push_back(s.get<std::string>());
  }
  if (!tracks_.count(0) && controller_) {
    const auto saved = std::move(tracks_); tracks_.clear(); startDefaults();
    for (const auto& [k, v] : saved) if (k != 0) tracks_[k] = v;
    pending_.clear();
  }
  // Policy: skin and expression are restored too (saves without them keep the current look);
  // an id removed since the save falls back to the controller default.
  if (state.contains("skin") && state.at("skin").is_string() && !setSkin(state.at("skin").get<std::string>()))
    setSkin(controller_ ? controller_->defaultSkin : std::string());
  if (state.contains("expression") && (state.at("expression").is_null() || state.at("expression").is_string())) {
    const std::optional<std::string> saved = state.at("expression").is_string() ? std::optional<std::string>(state.at("expression").get<std::string>()) : std::nullopt;
    if (!setExpression(saved)) setExpression(controller_ && !controller_->defaultExpression.empty() ? std::optional<std::string>(controller_->defaultExpression) : std::nullopt);
  }
}

Json Instance::describe() const {
  Json tracks = Json::array();
  for (const auto& [track, state] : tracks_) {
    Json t{{"track", track}};
    if (state.current) t["clip"] = rig_->clips[state.current->clip].id, t["timeMs"] = state.current->timeMs, t["serial"] = state.current->serial, t["ended"] = state.current->ended;
    if (state.from) t["mixFrom"] = rig_->clips[state.from->clip].id, t["mixElapsedMs"] = state.mixElapsed, t["mixDurationMs"] = state.mixDuration;
    tracks.push_back(t);
  }
  const auto expression = this->expression();
  const int active = activeExpression();
  return {{"tracks", tracks}, {"paused", paused_}, {"timeScale", timeScale_}, {"skin", skin()},
          {"expression", expression ? Json(*expression) : Json(nullptr)},
          {"activeExpression", active >= 0 ? Json(rig_->expressions[size_t(active)].id) : Json(nullptr)}};
}
}  // namespace cutout
