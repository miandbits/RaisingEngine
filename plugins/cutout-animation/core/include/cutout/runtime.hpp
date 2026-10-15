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
  std::string clip;                           // empty = no clip (a skin/expression-only binding)
  std::optional<std::string> skin;            // set this skin on match ("" never: validation needs an id)
  bool setsExpression = false;                // `expression` present: id, or null (= none)
  std::optional<std::string> expression;
  int track = 0;
  std::optional<bool> loop;                   // absent = clip's own loop flag
  double speed = 1;
  Then then = Then::Default;
  std::vector<std::string> stopOn;            // signals that end this binding's clip
};
struct Layer { int track = 1; std::string clip; };
struct Controller {
  std::string id, rig, defaultClip;
  std::string defaultSkin, defaultExpression;  // optional ("" = default attachments / no expression)
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

/**
 * One textured item. Regions: `matrix` maps image pixels. Meshes (plugin 1.4.0) also carry
 * `positions` (x, y per vertex, same space as `matrix`'s output), `uvs` (0..1) and `indices`
 * (triangles); `matrix`/`width`/`height` stay the undeformed image rectangle as a fallback for
 * hosts without mesh support (ABI: CONTRACT.md §7, capability `plugin-mesh-v1`).
 */
struct DrawItem {
  std::string slot, imageAssetId;
  Affine matrix;  // image pixel space -> instance space (origin = rig origin)
  double width = 0, height = 0, alpha = 1;
  std::vector<double> positions, uvs;
  std::vector<int> indices;
  bool mesh() const { return !indices.empty(); }
};

/**
 * Pose: channel values plus the skin its attachment names resolve in (-1 = default list only)
 * and the expression that was applied (-1 = none). Drawing always looks attachments up with
 * Rig::attachment(slot, name, pose.skin).
 * IK (plugin 1.3.0): `bones` stay the animated (keyable) local values; `world` includes the
 * solved constraints and `applied` holds the local values it was built from (empty when the rig
 * has no IK: then it equals `bones`). ikMix/ikBend are the sampled constraint settings.
 * Deform (plugin 1.4.0): keyed vertex offsets per (slot index, mesh attachment name); a mesh
 * uses them only when their size is 2 × its vertex count.
 */
struct Pose {
  std::vector<Transform> bones; std::vector<Affine> world; std::vector<std::optional<std::string>> attachments; std::vector<double> alpha;
  int skin = -1, expression = -1;
  std::vector<double> ikMix; std::vector<char> ikBend;
  std::vector<Transform> applied;
  std::map<std::pair<int, std::string>, std::vector<double>> deform;
  const Transform& local(size_t bone) const { return applied.size() > bone ? applied[bone] : bones[bone]; }
};
/** Skin/expression for samplePose. expressions=false skips every expression (raw clip values);
 *  ik=false skips the IK constraints (world = plain forward kinematics of the animated values). */
struct Look { int skin = -1; int expression = -1; bool expressions = true; bool ik = true; };

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
  /** "" = default attachments only. Unknown ids are refused (false) and change nothing. */
  bool setSkin(const std::string& skin);
  /** nullopt = no expression. Clip expression keys still override it while they play. */
  bool setExpression(const std::optional<std::string>& expression);
  std::string skin() const;
  std::optional<std::string> expression() const;

  Pose pose() const;
  std::vector<DrawItem> drawList() const;
  std::vector<Notification> takeNotifications();

  Json saveState() const;
  /** Restores clip, skin and expression state; looping clips restart at 0 by policy. */
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
  int skin_ = -1, expression_ = -1;  // instance look (controller defaults, bindings, setSkin/setExpression)

  void advance(int track, TrackState& state, double dtMs);
  void finish(int track, TrackState& state);
  void startDefaults();
  void sampleClip(const Entry& entry, Pose& pose, std::vector<char>* touched, double weight, bool attachments) const;
  int activeExpression() const;
};

/** Sample one clip at a time without an instance (editor/test helper). The clip plays as the
 *  base track: `look.expression` (overridden by the clip's own expression keys) applies after it. */
Pose samplePose(const Rig& rig, int clip, double timeMs, const Look& look = {});
/** Apply expression `index` (>= 0) onto `pose`: listed slots/bone channels are set absolutely. */
void applyExpression(const Rig& rig, int index, Pose& pose);
/**
 * Solve the rig's IK constraints on a pose whose world matrices are computed (Rig::ikOrder; each
 * rotates its chain so the effector reaches the target's world origin, blended by mix) and
 * recompute the world of every bone they move. Fills pose.applied. No-op without constraints.
 */
void applyIk(const Rig& rig, Pose& pose);
/** Unit rig-space direction of aim constraint `ik`'s axis (`aimAxis` on its bone) in `pose`: after
 *  solving at full mix it points at the target. false for two-bone chains or a degenerate bone. */
bool ikAimDirection(const Rig& rig, const Pose& pose, int ik, double& dx, double& dy);
double sampleTrack(const Track& track, double timeMs, double fallback);
/** Deform offsets of a deform track at `timeMs` (step/linear/bezier between keys, ends held). */
std::vector<double> sampleDeform(const Track& track, double timeMs);

// ---- Mesh attachments (plugin 1.4.0). Shared by every runtime, the editor canvas and tests.
/** Image pixels -> slot bone space: T(x,y)·R·S of the attachment offset, then -pivot·size. */
Affine attachmentPlacement(const Attachment& attachment);
/** The pose's deform offsets for (slot, attachment) when they fit its vertex count, else nullptr. */
const std::vector<double>* deformOf(const Pose& pose, int slot, const Attachment& attachment);
/**
 * Rig-space positions [x, y, ...] of the mesh `attachment` shown on slot `slot`: deform offsets
 * added to the setup vertices, then the slot bone's world · placement (unweighted) or the
 * weighted sum over its bones' world · bind (skinning, world matrices after IK). Empty for regions.
 */
std::vector<double> meshPositions(const Rig& rig, const Pose& pose, int slot, const Attachment& attachment);
/**
 * The affine map image px -> rig space that moves vertex `vertex` (deformed position in, rig
 * position out): weighted meshes blend their bones' matrices. The editor inverts it to turn a
 * canvas drag into image-space vertex moves and deform offsets.
 */
Affine meshVertexMatrix(const Rig& rig, const Pose& pose, int slot, const Attachment& attachment, size_t vertex);
}  // namespace cutout
