// Physics constraints (plugin 1.13.0): a deterministic fixed-step spring-damper per constraint that
// lets a bone (hair, skirt, accessories) trail its animated pose. Presentation only.
//
// Determinism: steps sit on an absolute grid of the instance clock (constraint c steps at
// k · stepMs_c), runPhysics cuts the animation at those times and each step's input is sampled
// on the snapMs grid, so a frame rate (30, 60, 144 Hz or irregular dts) only changes where the
// animation is cut, never what a step sees. Every runtime runs this same code (native, Wasm).
#include <algorithm>
#include <cmath>
#include <limits>
#include "cutout/runtime.hpp"

namespace cutout {
namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double EPS = 1e-9;

/** The bone's input in rig space: origin, x-axis angle and lever (length × world x scale). */
struct Input { double x = 0, y = 0, angle = 0, lever = 1; };
Input inputOf(const PhysicsConstraint& k, const Affine& w) {
  Input in{w.e, w.f, std::atan2(w.b, w.a), k.lever * std::hypot(w.a, w.b)};
  if (!(in.lever >= 1)) in.lever = 1;  // a collapsed bone still needs a lever to divide by
  return in;
}
/**
 * One step of `h` seconds. (dx, dy) = how far the input origin moved since the last step, already
 * capped by `limit`; inertia keeps that much of the old position (translation) and of the old tip
 * (rotation, scaleX). Then semi-implicit Euler with the spring toward 0 offset, wind (+x) and
 * gravity (+y, screens are y-down) per unit mass, and the per-step velocity decay.
 */
void integrate(const PhysicsConstraint& k, PhysicsBody& b, const Input& in, double dx, double dy, double wind, double gravity) {
  const double h = k.step, m = h / k.mass, e = k.strength, inertia = k.inertia, d = k.decay;
  if (k.x > 0 || k.y > 0) {
    b.ox -= dx * inertia;
    b.oy -= dy * inertia;
    b.vx += (wind - b.ox * e) * m;
    b.vy += (gravity - b.oy * e) * m;
    b.ox += b.vx * h;
    b.oy += b.vy * h;
    b.vx *= d;
    b.vy *= d;
  }
  double theta = in.angle;
  if (k.rotate > 0) {
    // Where the simulated tip was, seen from the moved origin: the bone turns toward it.
    const double rx = b.tipX - (b.px + dx), ry = b.tipY - (b.py + dy);
    if (std::hypot(rx, ry) > EPS) b.r += std::remainder(std::atan2(ry, rx) - (in.angle + b.r), 2 * PI) * inertia;
    theta = in.angle + b.r;
    // Torque of gravity / wind on a rod of the lever's length (angular acceleration g·cos/L).
    b.vr += ((gravity * std::cos(theta) - wind * std::sin(theta)) / in.lever - b.r * e) * m;
    b.r += b.vr * h;
    b.vr *= d;
    b.r = std::remainder(b.r, 2 * PI);
    theta = in.angle + b.r;
  }
  if (k.scaleX > 0) {
    const double c = std::cos(theta), s = std::sin(theta);
    b.s -= (dx * c + dy * s) / in.lever * inertia;
    b.vs += ((wind * c + gravity * s) / in.lever - b.s * e) * m;
    b.s += b.vs * h;
    b.vs *= d;
    b.s = std::max(b.s, -0.9);  // never folds the bone through itself
  }
  b.px = in.x;
  b.py = in.y;
  b.tipX = in.x + in.lever * std::cos(theta);
  b.tipY = in.y + in.lever * std::sin(theta);
}
/**
 * The settled state at `in`: the integrator's own equilibrium (velocities 0, the spring balancing
 * wind/gravity), solved directly so any damping (even none) starts at rest. Translation and
 * scale are linear; the rotation solves (g·cos θ − w·sin θ) / L = strength · r, θ = angle + r, by
 * bisection on [−π, π] (a fixed PHYSICS_SETTLE_STEPS halvings) when the spring is strong enough to
 * have a root there; a weak or absent spring hangs the bone along the force instead. Without
 * strength translation and scale have no rest offset (0).
 */
void settleBody(const PhysicsConstraint& k, PhysicsBody& b, const Input& in, double wind, double gravity) {
  const int64_t steps = b.steps;
  b = PhysicsBody{};
  b.steps = steps;
  const double e = k.strength, L = in.lever;
  if (e > 0 && (k.x > 0 || k.y > 0)) { b.ox = wind / e; b.oy = gravity / e; }
  if (k.rotate > 0 && (wind != 0 || gravity != 0)) {
    auto f = [&](double r) { const double t = in.angle + r; return (gravity * std::cos(t) - wind * std::sin(t)) / L - e * r; };
    double lo = -PI, hi = PI;
    if (e > 0 && f(lo) > 0 && f(hi) < 0) {
      for (int i = 0; i < PHYSICS_SETTLE_STEPS; i++) {
        const double mid = (lo + hi) / 2;
        (f(mid) > 0 ? lo : hi) = mid;
      }
      b.r = (lo + hi) / 2;
    } else b.r = std::remainder(std::atan2(gravity, wind) - in.angle, 2 * PI);
  }
  const double theta = in.angle + b.r;
  if (e > 0 && k.scaleX > 0) b.s = std::max(-0.9, (wind * std::cos(theta) + gravity * std::sin(theta)) / L / e);
  b.px = in.x;
  b.py = in.y;
  b.tipX = in.x + L * std::cos(theta);
  b.tipY = in.y + L * std::sin(theta);
  b.ready = true;
}
/** Constraint `c`'s offsets onto `pose.world`: its bone, then every descendant from its local. */
void applyBody(const Rig& rig, const PhysicsConstraint& k, const PhysicsBody& b, double mix, Pose& pose) {
  if (!b.ready || !(mix > 0) || k.bone < 0 || size_t(k.bone) >= pose.world.size()) return;
  const double tx = b.ox * mix * k.x, ty = b.oy * mix * k.y, rot = b.r * mix * k.rotate, sc = b.s * mix * k.scaleX;
  if (tx == 0 && ty == 0 && rot == 0 && sc == 0) return;
  Affine& w = pose.world[size_t(k.bone)];
  if (sc != 0) { w.a *= 1 + sc; w.b *= 1 + sc; }  // along the bone's own x axis
  if (rot != 0) {                                 // about its origin, on screen
    const double c = std::cos(rot), s = std::sin(rot);
    const Affine linear = w;
    w.a = c * linear.a - s * linear.b; w.b = s * linear.a + c * linear.b;
    w.c = c * linear.c - s * linear.d; w.d = s * linear.c + c * linear.d;
  }
  w.e += tx;
  w.f += ty;
  for (int i : k.affected) {
    if (i == k.bone) continue;
    const Transform& t = pose.local(size_t(i));
    pose.world[size_t(i)] = pose.world[size_t(rig.bones[size_t(i)].parentIndex)] * Affine::compose(t.x, t.y, t.rotation, t.scaleX, t.scaleY);
  }
}
double mixOf(const Rig& rig, const Pose& pose, size_t c) {
  return std::clamp(c < pose.physicsMix.size() ? pose.physicsMix[c] : rig.physics[c].mix, 0.0, 1.0);
}
double boundaryOf(const PhysicsConstraint& k, const PhysicsBody& b) { return double(b.steps + 1) * k.stepMs; }
}  // namespace

double snapMs(double ms) { return std::round(ms * 1024.0) / 1024.0; }

void PhysicsSim::init(const Rig& rig) {
  bodies.assign(rig.physics.size(), PhysicsBody{});
  clock = 0;
  steps = settles = teleports = 0;
}
void PhysicsSim::settle(const Rig& rig, const Pose& input, const std::vector<char>* only) {
  if (bodies.size() != rig.physics.size() || input.world.size() != rig.bones.size()) return;
  Pose pose = input;
  for (int ci : rig.physicsOrder) {
    const size_t c = size_t(ci);
    const PhysicsConstraint& k = rig.physics[c];
    if (!only || (c < only->size() && (*only)[c])) {
      settleBody(k, bodies[c], inputOf(k, pose.world[size_t(k.bone)]), k.wind + wind, k.gravity + gravity);
      settles++;
    }
    applyBody(rig, k, bodies[c], mixOf(rig, pose, c), pose);  // later constraints settle on it
  }
}
double PhysicsSim::nextBoundary(const Rig& rig) const {
  double next = std::numeric_limits<double>::infinity();
  for (size_t c = 0; c < rig.physics.size() && c < bodies.size(); c++) next = std::min(next, boundaryOf(rig.physics[c], bodies[c]));
  return next;
}
void PhysicsSim::step(const Rig& rig, Pose pose, double boundary) {
  if (bodies.size() != rig.physics.size() || pose.world.size() != rig.bones.size()) return;
  for (int ci : rig.physicsOrder) {
    const size_t c = size_t(ci);
    const PhysicsConstraint& k = rig.physics[c];
    PhysicsBody& b = bodies[c];
    // Exact equality: nextBoundary computed this very expression for the due constraints.
    if (boundaryOf(k, b) == boundary) {
      b.steps++;
      steps++;
      const Input in = inputOf(k, pose.world[size_t(k.bone)]);
      const double wx = k.wind + wind, gy = k.gravity + gravity;
      const double dx = in.x - b.px, dy = in.y - b.py;
      if (!b.ready) settleBody(k, b, in, wx, gy), settles++;
      else if (std::hypot(dx, dy) > k.teleport) settleBody(k, b, in, wx, gy), teleports++;  // a jump, not a motion
      else {
        const double q = k.limit * k.step;
        integrate(k, b, in, std::clamp(dx, -q, q), std::clamp(dy, -q, q), wx, gy);
      }
    }
    applyBody(rig, k, b, mixOf(rig, pose, c), pose);
  }
}
void PhysicsSim::apply(const Rig& rig, Pose& pose) const {
  if (bodies.size() != rig.physics.size() || pose.world.size() != rig.bones.size()) return;
  for (int ci : rig.physicsOrder) applyBody(rig, rig.physics[size_t(ci)], bodies[size_t(ci)], mixOf(rig, pose, size_t(ci)), pose);
}
void PhysicsSim::skipTo(const Rig& rig, double clockMs) {
  for (size_t c = 0; c < rig.physics.size() && c < bodies.size(); c++) {
    const PhysicsConstraint& k = rig.physics[c];
    PhysicsBody& b = bodies[c];
    b.steps = std::max<int64_t>(b.steps, int64_t(std::floor(clockMs / k.stepMs)) - 1);
    while (boundaryOf(k, b) <= clockMs) b.steps++;
  }
}

void runPhysics(PhysicsSim& sim, const Rig& rig, double dtMs, const std::function<void(double)>& advance, const std::function<Pose()>& sample) {
  if (!(dtMs > 0)) return;
  const double end = sim.clock + dtMs, limit = snapMs(end);
  for (int taken = 0;; taken++) {
    const double next = sim.nextBoundary(rig);
    if (!(next <= limit)) break;
    if (taken >= PHYSICS_MAX_STEPS_PER_UPDATE) {
      // A hitch of seconds: simulating it all would stall the frame. Settle at the end instead.
      advance(std::max(0.0, end - sim.clock));
      sim.clock = std::max(sim.clock, end);
      sim.skipTo(rig, limit);
      sim.settle(rig, sample());
      return;
    }
    advance(std::max(0.0, next - sim.clock));
    sim.clock = std::max(sim.clock, next);
    sim.step(rig, sample(), next);
  }
  advance(std::max(0.0, end - sim.clock));
  sim.clock = std::max(sim.clock, end);
}
}  // namespace cutout
