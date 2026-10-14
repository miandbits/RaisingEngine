#include "onion-skin.hpp"
#include <QSettings>
#include <algorithm>

namespace cutout_studio {
namespace {
constexpr const char* GROUP = "plugins/cutout-animation/onionSkin";
QString colorName(const QColor& c) { return c.isValid() ? c.name() : QString(); }
}  // namespace

OnionSettings OnionSettings::load() {
  OnionSettings s;
  QSettings q;
  q.beginGroup(GROUP);
  s.enabled = q.value("enabled", s.enabled).toBool();
  s.trail = q.value("trail", s.trail).toBool();
  s.before = std::clamp(q.value("before", s.before).toInt(), 0, MAX_GHOSTS);
  s.after = std::clamp(q.value("after", s.after).toInt(), 0, MAX_GHOSTS);
  s.stepMs = std::clamp(q.value("stepMs", s.stepMs).toInt(), 1, 10000);
  s.keysOnly = q.value("keysOnly", s.keysOnly).toBool();
  s.opacity = std::clamp(q.value("opacity", s.opacity).toDouble(), 0.05, 1.0);
  s.falloff = std::clamp(q.value("falloff", s.falloff).toDouble(), 0.0, 1.0);
  // Stored as a color name; an empty string is 원래 색 (untinted ghost).
  if (q.contains("beforeColor")) s.beforeColor = QColor(q.value("beforeColor").toString());
  if (q.contains("afterColor")) s.afterColor = QColor(q.value("afterColor").toString());
  s.includeCurrent = q.value("includeCurrent", s.includeCurrent).toBool();
  s.subtreeOnly = q.value("subtreeOnly", s.subtreeOnly).toBool();
  return s;
}
void OnionSettings::save() const {
  QSettings q;
  q.beginGroup(GROUP);
  q.setValue("enabled", enabled);
  q.setValue("trail", trail);
  q.setValue("before", before);
  q.setValue("after", after);
  q.setValue("stepMs", stepMs);
  q.setValue("keysOnly", keysOnly);
  q.setValue("opacity", opacity);
  q.setValue("falloff", falloff);
  q.setValue("beforeColor", colorName(beforeColor));
  q.setValue("afterColor", colorName(afterColor));
  q.setValue("includeCurrent", includeCurrent);
  q.setValue("subtreeOnly", subtreeOnly);
}
bool OnionSettings::operator==(const OnionSettings& o) const {
  return enabled == o.enabled && trail == o.trail && before == o.before && after == o.after && stepMs == o.stepMs && keysOnly == o.keysOnly &&
         opacity == o.opacity && falloff == o.falloff && beforeColor == o.beforeColor && afterColor == o.afterColor &&
         includeCurrent == o.includeCurrent && subtreeOnly == o.subtreeOnly;
}

QVector<GhostTime> onionTimes(const OnionSettings& s, double p, double d, bool loop, QVector<int> keys) {
  if (!(d > 0) || !std::isfinite(p)) return {};
  const int wanted[2] = {std::clamp(s.before, 0, OnionSettings::MAX_GHOSTS), std::clamp(s.after, 0, OnionSettings::MAX_GHOSTS)};
  auto wrap = [&](double t) {
    if (!loop) return std::clamp(t, 0.0, d);
    const double m = std::fmod(t, d);
    return m < 0 ? m + d : m;
  };
  struct Candidate { double sample, distance; int side; };  // side 0 = before, 1 = after
  std::vector<Candidate> candidates;
  if (!s.keysOnly) {
    const double step = std::max(1, s.stepMs);
    for (int side = 0; side < 2; side++)
      for (int k = 1; k <= wanted[side]; k++) {
        const double offset = k * step;
        if (loop && offset >= d) break;  // a full cycle away is the current pose again
        candidates.push_back({wrap(p + (side ? offset : -offset)), offset, side});
      }
  } else {
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    for (int k : keys) {
      if (k < 0 || k > d || std::abs(k - p) < 0.5) continue;
      const double direct = std::abs(k - p);
      const int side = k < p ? 0 : 1;
      // Looping: the key may be nearer going around the clip the other way.
      if (loop && d - direct < direct) candidates.push_back({double(k), d - direct, 1 - side});
      else candidates.push_back({double(k), direct, side});
    }
  }
  std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
    return a.distance != b.distance ? a.distance < b.distance : a.side < b.side;
  });
  QVector<double> used{wrap(p)};
  int count[2] = {0, 0};
  QVector<GhostTime> out;
  for (const auto& c : candidates) {
    if (count[c.side] >= wanted[c.side]) continue;
    if (std::any_of(used.begin(), used.end(), [&](double u) { return std::abs(u - c.sample) < 1e-6; })) continue;
    used.push_back(c.sample);
    const int rank = ++count[c.side];
    out.push_back({c.sample, c.side ? rank : -rank});
  }
  std::sort(out.begin(), out.end(), [](const GhostTime& a, const GhostTime& b) { return a.offset < b.offset; });
  return out;
}

double ghostAlpha(const OnionSettings& s, int offset, int count) {
  const int k = std::abs(offset);
  if (k == 0) return std::clamp(s.opacity, 0.0, 1.0);
  return std::clamp(s.opacity * (1 - s.falloff * double(k - 1) / std::max(1, count)), 0.0, 1.0);
}

std::shared_ptr<const cutout::Pose> GhostPoseCache::pose(const cutout::Rig& rig, quint64 generation, int clip, double timeMs) {
  if (generation != generation_ || clip != clip_) {
    poses_.clear();
    generation_ = generation;
    clip_ = clip;
  }
  if (poses_.size() > 512) poses_.clear();  // long playback sessions: keep memory bounded
  const qint64 key = qint64(std::llround(timeMs * 1000));
  auto it = poses_.constFind(key);
  if (it != poses_.constEnd()) return *it;
  samples_++;
  auto pose = std::make_shared<const cutout::Pose>(cutout::samplePose(rig, clip, timeMs));
  poses_.insert(key, pose);
  return pose;
}

QVector<double> trailTimes(double d) {
  QVector<double> out;
  if (!(d > 0)) return {0};
  // ~300 segments are smooth at any zoom; 5 ms floor keeps short clips cheap.
  const double step = std::max(5.0, d / 300.0);
  for (double t = 0; t < d - 1e-9; t += step) out.push_back(t);
  out.push_back(d);
  return out;
}
}  // namespace cutout_studio
