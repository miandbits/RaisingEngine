// Per-instance playback state, controller bindings and draw-list output.
// Presentation only: nothing here may feed back into game rules, costs or RNG.
#pragma once
#include <deque>
#include <memory>
#include "rig.hpp"

namespace cutout {

enum class Then { Default, Hold, Stop };

struct Binding {
  std::string id, signal;
  std::map<std::string, std::string> match;  // all listed params must equal; absent = any
  std::string clip;
  int track = 0;
  std::optional<bool> loop;                   // absent = clip's own loop flag
  double speed = 1;
  Then then = Then::Default;
  std::vector<std::string> stopOn;            // signals that end this binding's clip
};
struct Layer { int track = 1; std::string clip; };
struct Controller {
  std::string id, rig, defaultClip;
  std::string characterId;  // optional: adds param self="true" when a signal names this character
  std::vector<Layer> layers;
  std::vector<Binding> bindings;
};
std::vector<Diagnostic> validateController(const Json& data, const Rig* rig);
Controller parseController(const std::string& id, const Json& data);

struct Notification {
  std::string kind;  // start | interrupt | complete | end | event
  int track = 0;
  std::string clip, name;
  uint64_t serial = 0;  // unique per started entry within an instance
  int64_t loop = 0;     // completed loop index for `complete`
  Json toJson() const;
};

struct DrawItem {
  std::string slot, imageAssetId;
  Affine matrix;  // image pixel space -> instance space (origin = rig origin)
  double width = 0, height = 0, alpha = 1;
};

struct Pose { std::vector<Transform> bones; std::vector<Affine> world; std::vector<std::optional<std::string>> attachments; std::vector<double> alpha; };

class Instance {
 public:
  Instance(std::shared_ptr<const Rig> rig, std::optional<Controller> controller = std::nullopt);
  const Rig& rig() const { return *rig_; }

  /** Start a clip. mixMs < 0 uses rig mix table. Returns entry serial (0 when clip unknown). */
  uint64_t play(int track, const std::string& clip, std::optional<bool> loop = std::nullopt, double speed = 1,
                Then then = Then::Default, int64_t mixMs = -1);
  void stop(int track, int64_t mixMs = -1);
  /** Route a game-side signal through controller bindings. Returns ids of matched bindings. */
  std::vector<std::string> signal(const std::string& name, const std::map<std::string, std::string>& params = {});
  /** Advance presentation time. dt is clamped to [0, 10 s]; huge gaps just complete loops. */
  void update(double dtMs);
  void setTimeScale(double scale) { timeScale_ = scale < 0 ? 0 : scale; }
  void setPaused(bool paused) { paused_ = paused; }
  bool paused() const { return paused_; }
  /** Editor scrubbing: set track time without emitting events. */
  void seek(int track, double timeMs);

  Pose pose() const;
  std::vector<DrawItem> drawList() const;
  std::vector<Notification> takeNotifications();

  Json saveState() const;
  /** Restores clip/expression state; looping clips restart at 0 by policy. */
  void restoreState(const Json& state);
  Json describe() const;

 private:
  struct Entry {
    int clip = -1; double timeMs = 0; bool loop = true; double speed = 1; Then then = Then::Default;
    uint64_t serial = 0; int64_t loops = 0; bool ended = false; std::string binding;
  };
  struct TrackState {
    std::optional<Entry> current, from;
    double mixElapsed = 0, mixDuration = 0;
    std::vector<std::string> stopOn;
  };
  std::shared_ptr<const Rig> rig_;
  std::optional<Controller> controller_;
  std::map<int, TrackState> tracks_;
  std::deque<Notification> pending_;
  uint64_t nextSerial_ = 1;
  double timeScale_ = 1;
  bool paused_ = false;

  void advance(int track, TrackState& state, double dtMs);
  void finish(int track, TrackState& state);
  void startDefaults();
  void sampleClip(const Entry& entry, Pose& pose, std::vector<char>* touched, double weight, bool attachments) const;
};

/** Sample one clip at a time without an instance (editor/test helper). */
Pose samplePose(const Rig& rig, int clip, double timeMs);
double sampleTrack(const Track& track, double timeMs, double fallback);
}  // namespace cutout
