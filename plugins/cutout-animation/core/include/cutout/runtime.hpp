// Per-instance playback state, controller bindings and draw-list output.
// Presentation only: nothing here may feed back into game rules, costs or RNG.
#pragma once
#include <array>
#include <deque>
#include <functional>
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
  /** `event` only (plugin 1.8.0): the key's resolved payload (int, float, string, audio, volume,
   *  balance; only fields that are set), merged into toJson() next to `name`. */
  Json payload = Json::object();
  Json toJson() const;
};

/**
 * One textured item. Regions: `matrix` maps image pixels. Meshes (plugin 1.4.0) also carry
 * `positions` (x, y per vertex, same space as `matrix`'s output), `uvs` (0..1) and `indices`
 * (triangles); `matrix`/`width`/`height` stay the undeformed image rectangle as a fallback for
 * hosts without mesh support (ABI: CONTRACT.md §7, capability `plugin-mesh-v1`).
 * Atlas (plugin 1.9.0, capability `plugin-atlas-v1`): `region` = the sub-image (x, y, w, h, page
 * pixels) of `imageAssetId` that is the item's picture; matrix/width/height and mesh uvs address
 * that sub-image exactly as they address a whole image without it.
 */
struct DrawItem {
  std::string slot, imageAssetId;
  Affine matrix;  // image pixel space -> instance space (origin = rig origin)
  double width = 0, height = 0, alpha = 1;
  std::vector<double> positions, uvs;
  std::vector<int> indices;
  bool hasRegion = false;
  double region[4] = {0, 0, 0, 0};
  const Attachment* source = nullptr;  // the drawn attachment (hosts key mesh topology by it)
  /** Clipping (plugin 1.12.0): the triangles were cut by a clip polygon this frame, so uvs and
   *  indices are the item's own (compact frames intern them instead of keying `source`). */
  bool clipped = false;
  bool mesh() const { return !indices.empty(); }
};

/** Sequence playback state of one slot (plugin 1.12.0): the last sequence key at or before the
 *  sampled time (`keyed` false = none: the attachment's setupIndex) and the ms since that key. */
struct SequenceState { bool keyed = false; SequenceMode mode = SequenceMode::Hold; int index = 0; double elapsedMs = 0, delay = 100; };
/** Frame of a `count`-frame sequence shown for `state` (Spine SequenceTimeline rules, 0..count-1). */
int sequenceFrame(const SequenceState& state, size_t count, int setupIndex);

/**
 * Pose: channel values plus the skin its attachment names resolve in (-1 = default list only)
 * and the expression that was applied (-1 = none). Drawing always looks attachments up with
 * Rig::attachment(slot, name, pose.skin).
 * IK (plugin 1.3.0): `bones` stay the animated (keyable) local values; `world` includes the
 * solved constraints and `applied` holds the local values it was built from (empty when the rig
 * has no IK: then it equals `bones`). ikMix/ikBend are the sampled constraint settings.
 * Deform (plugin 1.4.0): keyed vertex offsets per (slot index, mesh attachment name); a mesh
 * uses them only when their size is 2 × its vertex count.
 * IK chains (plugin 1.13.0): ikSolve reports per constraint what the last solve did (FABRIK
 * passes, effector distance, stretch factor). Physics (plugin 1.13.0): physicsMix holds the
 * sampled constraint mixes; the physics offsets are in `world` only (applied stays the IK result).
 * Transform constraints (plugin 1.16.0): transformMix holds the sampled channel mixes (rotate, x,
 * y, scaleX, scaleY, shearY); their results are in `applied` (local values) and `world` like IK's.
 */
struct IkSolve { int iterations = 0; double error = 0, stretch = 1; bool solved = false; };
struct Pose {
  std::vector<Transform> bones; std::vector<Affine> world; std::vector<std::optional<std::string>> attachments; std::vector<double> alpha;
  int skin = -1, expression = -1;
  std::vector<double> ikMix; std::vector<char> ikBend;
  std::vector<Transform> applied;
  std::map<std::pair<int, std::string>, std::vector<double>> deform;
  std::vector<SequenceState> sequence;  // per slot (plugin 1.12.0)
  std::vector<IkSolve> ikSolve;          // per IK constraint (plugin 1.13.0)
  std::vector<double> physicsMix;        // per physics constraint (plugin 1.13.0)
  std::vector<std::array<double, TRANSFORM_MIXES>> transformMix;  // per transform constraint (plugin 1.16.0)
  const Transform& local(size_t bone) const { return applied.size() > bone ? applied[bone] : bones[bone]; }
};
/** Skin/expression for samplePose. expressions=false skips every expression (raw clip values);
 *  ik=false skips the IK and transform constraints (world = plain forward kinematics of the
 *  animated values). */
struct Look { int skin = -1; int expression = -1; bool expressions = true; bool ik = true; };

/** One physics constraint's simulation state (rig space px, radians). Never saved: a restored
 *  instance starts settled (docs/contract.md "Physics"). */
struct PhysicsBody {
  bool ready = false;      // false = settle at the next input before simulating
  int64_t steps = 0;       // steps taken on the clock grid: the next one is due at (steps + 1) · stepMs
  double ox = 0, oy = 0, vx = 0, vy = 0;  // translation offset / velocity
  double r = 0, vr = 0;    // rotation offset (rad) / angular velocity
  double s = 0, vs = 0;    // relative scaleX offset / velocity
  double px = 0, py = 0;   // the bone's input origin at the last step
  double tipX = 0, tipY = 0;  // the simulated tip at the last step (rotation inertia)
};
/**
 * Fixed-step physics of one rig instance (plugin 1.13.0). Steps sit on an absolute grid of the
 * instance clock (k · stepMs per constraint), so any partition of the same time takes the same
 * steps with the same inputs (runPhysics). Editors drive one too (preview playback).
 */
class PhysicsSim {
 public:
  /** Size the bodies for `rig`; every body unsettled. The clock and step grid restart at 0. */
  void init(const Rig& rig);
  /** Settle the bodies (all, or those `only` marks) at `input`: at rest in the integrator's own
   *  equilibrium (the spring balancing wind/gravity), so the sag is already there and nothing jumps. */
  void settle(const Rig& rig, const Pose& input, const std::vector<char>* only = nullptr);
  /** Clock time of the next due step (infinity without constraints). */
  double nextBoundary(const Rig& rig) const;
  /** Step every constraint due at `boundary` (from nextBoundary) with `input`: the pose at that
   *  time after IK, without physics. Constraints see the ones before them (physicsOrder) applied. */
  void step(const Rig& rig, Pose input, double boundary);
  /** Write the current offsets into `pose.world` (bones and their descendants). */
  void apply(const Rig& rig, Pose& pose) const;
  /** Jump the step grid to `clockMs` without simulating (a long hitch; settle after it). */
  void skipTo(const Rig& rig, double clockMs);
  double clock = 0;             // instance ms since init
  double wind = 0, gravity = 0;  // environment added to every constraint (editor preview controls)
  std::vector<PhysicsBody> bodies;
  int64_t steps = 0, settles = 0, teleports = 0;  // counters (describe, tests)
};
constexpr int PHYSICS_SETTLE_STEPS = 64;  // bisection halvings of the settled rotation
constexpr int PHYSICS_MAX_STEPS_PER_UPDATE = 240;
/** Physics inputs sample clip times on a 1/1024 ms grid: sums of the same time in different
 *  pieces (16.6 + 16.7 vs 33.3) differ in the last bits, the grid makes them equal. */
double snapMs(double ms);
/**
 * Advance `sim` by dtMs. `advance(ms)` moves the animation forward (called with the pieces between
 * step boundaries, summing to dtMs), `sample()` returns the physics input at the current time.
 * More than PHYSICS_MAX_STEPS_PER_UPDATE due steps (a hitch of seconds) are skipped: the bodies
 * settle at the end instead.
 */
void runPhysics(PhysicsSim& sim, const Rig& rig, double dtMs, const std::function<void(double)>& advance, const std::function<Pose()>& sample);

class Instance {
 public:
  Instance(std::shared_ptr<const Rig> rig, std::optional<Controller> controller = std::nullopt);
  const Rig& rig() const { return *rig_; }
  const std::shared_ptr<const Rig>& sharedRig() const { return rig_; }

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
  /** Editor scrubbing: set track time without emitting events (physics settles: no simulation). */
  void seek(int track, double timeMs);
  /** Physics (plugin 1.13.0): extra wind/gravity (px/s^2) on every constraint (preview controls). */
  void setPhysicsEnvironment(double wind, double gravity) { physics_.wind = wind; physics_.gravity = gravity; }
  /** Forget the simulation: every body settles at the current pose (a teleport the host knows of). */
  void resetPhysics();
  const PhysicsSim& physics() const { return physics_; }
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
  PhysicsSim physics_;              // plugin 1.13.0 (empty without constraints)

  void advance(int track, TrackState& state, double dtMs);
  void advanceTracks(double dtMs);
  /** The pose before physics; `snap` samples clip times on the snapMs grid (physics rigs). */
  Pose basePose(bool snap) const;
  void finish(int track, TrackState& state);
  void startDefaults();
  void sampleClip(const Entry& entry, Pose& pose, std::vector<char>* touched, double weight, bool attachments, bool snap = false) const;
  int activeExpression(bool snap = false) const;
};

/** Sample one clip at a time without an instance (editor/test helper). The clip plays as the
 *  base track: `look.expression` (overridden by the clip's own expression keys) applies after it. */
Pose samplePose(const Rig& rig, int clip, double timeMs, const Look& look = {});
/** Apply expression `index` (>= 0) onto `pose`: listed slots/bone channels are set absolutely. */
void applyExpression(const Rig& rig, int index, Pose& pose);
/**
 * Solve the rig's IK constraints on a pose whose world matrices are computed (Rig::ikOrder; each
 * rotates its chain so the effector reaches the target's world origin, blended by mix) and
 * recompute the world of every bone they move. Fills pose.applied and pose.ikSolve. One and two
 * bones are analytic (bit for bit the 1.12 results without stretch); chains of 3+ run FABRIK in
 * rig space (IK_CHAIN_ITERATIONS / IK_CHAIN_TOLERANCE). No-op without constraints.
 * Plugin 1.16.0: transform constraints run in the same pass, in Rig::constraintOrder; bones with
 * shear or a non-normal inherit mode solve through rotationToward (exact for every mode).
 */
void applyIk(const Rig& rig, Pose& pose);
/** World matrix of bone `bone` from `world` (its parent's entry must be current) and `local`. */
Affine boneWorld(const Rig& rig, const std::vector<Affine>& world, size_t bone, const Transform& local);
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

// ---- Sequences and clipping (plugin 1.12.0) ----
/** Frame index `attachment` shows on slot `slot` in `pose` (0 without a sequence). */
int attachmentFrame(const Pose& pose, int slot, const Attachment& attachment);
/** Image asset the slot draws for `attachment` in `pose`: its sequence frame, else imageAssetId. */
const std::string& attachmentImage(const Pose& pose, int slot, const Attachment& attachment);
/** The atlas region of that picture (export only): the frame's for sequences, else `atlas`. */
const std::optional<AtlasRegion>& attachmentAtlas(const Pose& pose, int slot, const Attachment& attachment);
/**
 * Draw items of `pose` in instance space (rig origin at 0, 0) back to front: what
 * Instance::drawList returns. Clipping attachments start a clip range; items inside it are cut
 * to the clip polygon (clipItem) and become mesh items, items outside keep their shape.
 */
std::vector<DrawItem> drawItems(const Rig& rig, const Pose& pose);
}  // namespace cutout
