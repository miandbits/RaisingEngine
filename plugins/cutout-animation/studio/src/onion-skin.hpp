// Onion skin (ghosting) and motion trail: editor-only animation aids for the canvas.
// Ghost poses come from the same core sampling as playback (cutout::samplePose on the clip
// being edited), so a ghost is exactly the pose the runtime shows at that time. Nothing here
// edits documents or fires clip events; settings live in QSettings (per user, never in data).
#pragma once
#include <QColor>
#include <QHash>
#include <QVector>
#include <cmath>
#include <memory>
#include "cutout/runtime.hpp"

namespace cutout_studio {
struct OnionSettings {
  bool enabled = false;
  bool trail = false;         // motion trail of the selected bone
  int before = 2, after = 2;  // ghosts on each side, 0..MAX_GHOSTS
  int stepMs = 100;           // step mode spacing
  bool keysOnly = false;      // ghost at neighbouring key times instead of every stepMs
  double opacity = 0.45;      // alpha of the nearest ghost
  double falloff = 0.6;       // 0 = all equal; 1 = the farthest fades to opacity / count
  QColor beforeColor = QColor("#ef4444"), afterColor = QColor("#22c55e");  // invalid = 원래 색
  bool includeCurrent = false;  // also ghost the saved (committed) pose at the playhead
  bool subtreeOnly = false;     // ghost only the selected bone and its children
  static constexpr int MAX_GHOSTS = 10;
  static OnionSettings load();
  void save() const;
  bool operator==(const OnionSettings& o) const;
  bool operator!=(const OnionSettings& o) const { return !(*this == o); }
};

/** One ghost slot: offset < 0 before, > 0 after (rank on that side, 1 = nearest), 0 = current. */
struct GhostTime {
  double sampleMs = 0;
  int offset = 0;
  bool operator==(const GhostTime& o) const { return offset == o.offset && std::abs(sampleMs - o.sampleMs) < 1e-6; }
};

/** Ghost times around `playheadMs` (current excluded; includeCurrent is added by the caller).
 *  Step mode: playhead ± k·stepMs with k·stepMs < duration; looping clips wrap into [0, d)
 *  like the runtime, one-shots clamp to [0, d]. Keys-only: the nearest distinct key times on
 *  each side; for loops the search wraps around the clip and a key belongs to the side where
 *  it is nearer (ties: the unwrapped side). Ghosts at the same time as the playhead or as a
 *  nearer ghost are dropped, so a clamped one-shot shows one ghost at its boundary. */
QVector<GhostTime> onionTimes(const OnionSettings& s, double playheadMs, double durationMs, bool loop, QVector<int> keyTimes);
/** Alpha of the ghost with `offset` among `count` ghosts on its side. */
double ghostAlpha(const OnionSettings& s, int offset, int count);

/** Poses sampled for ghosts, shared until the displayed rig changes (generation) or the clip
 *  changes. Scrubbing back and forth or redrawing never resamples a time already seen. */
class GhostPoseCache {
 public:
  std::shared_ptr<const cutout::Pose> pose(const cutout::Rig& rig, quint64 generation, int clip, double timeMs);
  int samples() const { return samples_; }  // cache misses since creation (tests)
  void clear() { poses_.clear(); }

 private:
  quint64 generation_ = ~quint64(0);
  int clip_ = -1, samples_ = 0;
  QHash<qint64, std::shared_ptr<const cutout::Pose>> poses_;
};

/** Times of the motion trail polyline over [0, duration] (both ends included). */
QVector<double> trailTimes(double durationMs);
}  // namespace cutout_studio
