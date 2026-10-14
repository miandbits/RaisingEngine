#include "cutout/runtime.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace cutout {
namespace {
constexpr double MAX_DT_MS = 10000;
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
  if (kind == "event") j["name"] = name;
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
static std::optional<std::string> sampleAttachment(const Track& track, double t) {
  const auto& keys = track.keys;
  const Key* chosen = &keys.front();
  for (const auto& k : keys) { if (double(k.timeMs) <= t) chosen = &k; else break; }
  return chosen->name;
}

static Pose setupPose(const Rig& rig) {
  Pose pose;
  for (const auto& b : rig.bones) pose.bones.push_back(b.setup);
  for (const auto& s : rig.slots) { pose.attachments.push_back(s.attachment); pose.alpha.push_back(s.alpha); }
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
    if (tr.bone) { auto& value = field(pose.bones[tr.targetIndex], tr.property); value = sampleTrack(tr, t, value); }
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
  // Attachments cannot interpolate: switch halfway through the mix.
  if (alpha < 0.5) out.attachments = a.attachments;
  return out;
}

Pose samplePose(const Rig& rig, int clip, double timeMs) {
  Pose pose = setupPose(rig);
  if (clip >= 0 && clip < int(rig.clips.size())) applyClip(rig, rig.clips[clip], clipTime(rig.clips[clip], timeMs, rig.clips[clip].loop), pose);
  computeWorld(rig, pose);
  return pose;
}

std::vector<Diagnostic> validateController(const Json& data, const Rig* rig) {
  std::vector<Diagnostic> out;
  auto error = [&](const std::string& p, const std::string& code, const std::string& m) { out.push_back({code, "error", p, m}); };
  if (!data.is_object()) { error("", "ANIM_STRUCTURE", "컨트롤러 데이터는 객체여야 합니다."); return out; }
  if (data.value("schemaVersion", 0) != SCHEMA_VERSION) error("/schemaVersion", "ANIM_SCHEMA_VERSION", "지원하는 컨트롤러 schemaVersion은 1입니다.");
  auto clipKnown = [&](const Json& v) { return v.is_string() && (!rig || rig->clipIndex(v.get<std::string>()) >= 0); };
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
    if (!b.contains("clip") || !clipKnown(b.at("clip"))) error(p + "/clip", "ANIM_CLIP_UNKNOWN", "연결 클립이 리그에 없습니다.");
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
  for (const auto& l : arrayOr(data, "layers")) c.layers.push_back({l.value("track", 1), l.at("clip").get<std::string>()});
  for (const auto& b : arrayOr(data, "bindings")) {
    Binding x; x.id = b.at("id"); x.signal = b.at("signal"); x.clip = b.at("clip"); x.track = b.value("track", 0);
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
  pending_.clear();  // construction is not an observable transition
}
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
  std::set<int> used;
  for (const auto& b : controller_->bindings) {
    if (b.signal != name || used.count(b.track)) continue;
    bool ok = true;
    for (const auto& [k, v] : b.match) { auto it = params.find(k); if (it == params.end() || it->second != v) { ok = false; break; } }
    if (!ok) continue;
    if (play(b.track, b.clip, b.loop, b.speed, b.then, -1)) {
      auto& state = tracks_[b.track];
      state.current->binding = b.id; state.stopOn = b.stopOn;
      used.insert(b.track); matched.push_back(b.id);
    }
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
        if (e.timeMs < clip.durationMs && occ >= prev && occ < cur) pending_.push_back({"event", track, clip.id, e.name, serial});
      }
    for (int64_t k = first + 1; k <= last && k - first <= MAX_COMPLETES_PER_UPDATE; k++) pending_.push_back({"complete", track, clip.id, "", serial, k - 1});
    entry.loops = last;
  } else {
    for (const auto& e : clip.events) {
      const double occ = double(e.timeMs);
      if (occ >= prev && (occ < cur || (cur >= d && occ == d && prev < d))) pending_.push_back({"event", track, clip.id, e.name, serial});
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

Pose Instance::pose() const {
  Pose pose = setupPose(*rig_);
  for (const auto& [track, state] : tracks_) {
    Pose to = pose;
    if (state.current) sampleClip(*state.current, to, nullptr, 1, true);
    if (state.from && state.mixDuration > 0) {
      Pose from = pose;
      sampleClip(*state.from, from, nullptr, 1, true);
      pose = blend(from, to, std::clamp(state.mixElapsed / state.mixDuration, 0.0, 1.0));
    } else pose = to;
  }
  computeWorld(*rig_, pose);
  return pose;
}

std::vector<DrawItem> Instance::drawList() const {
  const Pose pose = this->pose();
  std::vector<DrawItem> out;
  const Affine origin = Affine::compose(-rig_->originX, -rig_->originY, 0, 1, 1);
  for (size_t i = 0; i < rig_->slots.size(); i++) {
    const auto& slot = rig_->slots[i];
    if (!pose.attachments[i] || pose.alpha[i] <= 0 || slot.boneIndex < 0) continue;
    const Attachment* att = rig_->attachment(slot.id, *pose.attachments[i]);
    if (!att) continue;
    const auto& o = att->offset;
    const Affine m = origin * pose.world[slot.boneIndex] * Affine::compose(o.x, o.y, o.rotation, o.scaleX, o.scaleY)
                     * Affine::compose(-att->pivotX * att->width, -att->pivotY * att->height, 0, 1, 1);
    out.push_back({slot.id, att->imageAssetId, m, att->width, att->height, std::clamp(pose.alpha[i], 0.0, 1.0)});
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
  return {{"version", 1}, {"timeScale", timeScale_}, {"paused", paused_}, {"tracks", tracks}};
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
}

Json Instance::describe() const {
  Json tracks = Json::array();
  for (const auto& [track, state] : tracks_) {
    Json t{{"track", track}};
    if (state.current) t["clip"] = rig_->clips[state.current->clip].id, t["timeMs"] = state.current->timeMs, t["serial"] = state.current->serial, t["ended"] = state.current->ended;
    if (state.from) t["mixFrom"] = rig_->clips[state.from->clip].id, t["mixElapsedMs"] = state.mixElapsed, t["mixDurationMs"] = state.mixDuration;
    tracks.push_back(t);
  }
  return {{"tracks", tracks}, {"paused", paused_}, {"timeScale", timeScale_}};
}
}  // namespace cutout
