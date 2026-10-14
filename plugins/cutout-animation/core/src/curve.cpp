#include "cutout/curve.hpp"
#include <algorithm>
#include <cmath>

namespace cutout {
namespace {
constexpr double TOLERANCE = 1e-7;
// One coordinate of the cubic Bezier with fixed endpoints 0 and 1.
double bez(double p1, double p2, double s) {
  const double r = 1 - s;
  return 3 * r * r * s * p1 + 3 * r * s * s * p2 + s * s * s;
}
double bezSlope(double p1, double p2, double s) {
  const double r = 1 - s;
  return 3 * r * r * p1 + 6 * r * s * (p2 - p1) + 3 * s * s * (1 - p2);
}
double slopeAt(const std::vector<double>& t, const std::vector<double>& v, size_t k) {
  // Track ends and local extrema get flat tangents: a held pose never wobbles past itself.
  if (k == 0 || k + 1 >= t.size()) return 0;
  const double d0 = (v[k] - v[k - 1]) / (t[k] - t[k - 1]), d1 = (v[k + 1] - v[k]) / (t[k + 1] - t[k]);
  if (!(d0 * d1 > 0)) return 0;
  return (v[k + 1] - v[k - 1]) / (t[k + 1] - t[k - 1]);
}
}  // namespace

Curve clampCurve(Curve c) {
  for (size_t i = 0; i < 4; i++) if (!std::isfinite(c[i])) c[i] = LINEAR_CURVE[i];
  c[0] = std::clamp(c[0], 0.0, 1.0);
  c[2] = std::clamp(c[2], 0.0, 1.0);
  c[1] = std::clamp(c[1], CURVE_Y_MIN, CURVE_Y_MAX);
  c[3] = std::clamp(c[3], CURVE_Y_MIN, CURVE_Y_MAX);
  return c;
}

void curvePoint(const Curve& curve, double s, double& x, double& y) {
  const Curve c = clampCurve(curve);
  x = bez(c[0], c[2], s);
  y = bez(c[1], c[3], s);
}

double evalCurve(const Curve& curve, double u) {
  if (!(u > 0)) return 0;
  if (u >= 1) return 1;
  const Curve c = clampCurve(curve);
  // With both x handles in [0,1] x(s) is monotonic, so x(s)=u has exactly one solution.
  // Newton converges in a few steps for ordinary handles; bisection covers flat x'(s).
  // Fixed iteration rules (no time or frame input) keep native and Wasm results identical.
  double s = u;
  bool solved = false;
  for (int i = 0; i < 8; i++) {
    const double err = bez(c[0], c[2], s) - u;
    if (std::fabs(err) < TOLERANCE) { solved = true; break; }
    const double d = bezSlope(c[0], c[2], s);
    if (std::fabs(d) < 1e-6) break;
    const double next = s - err / d;
    if (!(next >= 0 && next <= 1)) break;
    s = next;
  }
  if (!solved) {
    double lo = 0, hi = 1;
    s = u;
    for (int i = 0; i < 64; i++) {
      const double x = bez(c[0], c[2], s);
      if (std::fabs(x - u) < TOLERANCE) break;
      if (x < u) lo = s; else hi = s;
      s = (lo + hi) / 2;
    }
  }
  return bez(c[1], c[3], s);
}

std::optional<Curve> presetCurve(const std::string& name) {
  if (name == "easeIn") return Curve{0.42, 0, 1, 1};
  if (name == "easeOut") return Curve{0, 0, 0.58, 1};
  if (name == "easeInOut") return Curve{0.42, 0, 0.58, 1};
  if (name == "flat") return Curve{1.0 / 3.0, 0, 2.0 / 3.0, 1};
  if (name == "back") return Curve{0.34, 1.56, 0.64, 1};  // overshoots ~10% then settles
  return std::nullopt;
}
const std::vector<std::string>& curvePresetNames() {
  static const std::vector<std::string> names = {"easeIn", "easeOut", "easeInOut", "auto", "flat", "back"};
  return names;
}

Curve autoCurve(const std::vector<double>& t, const std::vector<double>& v, size_t i) {
  if (i + 1 >= t.size() || t.size() != v.size()) return *presetCurve("flat");
  const double dt = t[i + 1] - t[i], dv = v[i + 1] - v[i];
  if (!(dt > 0) || dv == 0) return *presetCurve("flat");
  const double delta = dv / dt;
  double a = slopeAt(t, v, i) / delta, b = slopeAt(t, v, i + 1) / delta;
  // Fritsch-Carlson: keeps the eased segment monotonic between its two key values.
  if (a * a + b * b > 9) { const double tau = 3 / std::sqrt(a * a + b * b); a *= tau; b *= tau; }
  return {1.0 / 3.0, a / 3, 2.0 / 3.0, 1 - b / 3};
}
}  // namespace cutout
