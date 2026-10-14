// Bezier easing of one key segment (key interp "bezier").
// curve = [cx1, cy1, cx2, cy2] normalized to the segment: x = fraction of segment time,
// y = fraction of value delta (may overshoot). Endpoints are fixed at (0,0) and (1,1).
// Shared by runtime sampling, editing ops, the Qt graph editor and the Wasm build, so
// every host evaluates the exact same numbers.
#pragma once
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace cutout {
using Curve = std::array<double, 4>;

constexpr double CURVE_Y_MIN = -4, CURVE_Y_MAX = 5;
/** Linear-equivalent handles (what a linear key shows in the graph editor). */
constexpr Curve LINEAR_CURVE = {1.0 / 3.0, 1.0 / 3.0, 2.0 / 3.0, 2.0 / 3.0};

/** Eased fraction of the value delta at segment fraction u (0..1). x handles are clamped to [0,1]. */
double evalCurve(const Curve& curve, double u);
/** x and y of the Bezier at parameter s (graph drawing, tests). */
void curvePoint(const Curve& curve, double s, double& x, double& y);
/** Clamp x into [0,1] and y into [CURVE_Y_MIN, CURVE_Y_MAX]; non-finite values become linear handles. */
Curve clampCurve(Curve curve);

/** Fixed presets: easeIn easeOut easeInOut flat back. ("auto" depends on neighbours: autoCurve.) */
std::optional<Curve> presetCurve(const std::string& name);
/** Names accepted by setInterp/setKey `interp` or `preset`, besides "step"/"linear"/"bezier". */
const std::vector<std::string>& curvePresetNames();
/**
 * Smooth "auto" handles for the segment keys[i] -> keys[i+1] from neighbouring keys:
 * Catmull-Rom slopes, flat at local extrema and track ends, Fritsch-Carlson limited so
 * the segment never overshoots its two key values.
 */
Curve autoCurve(const std::vector<double>& times, const std::vector<double>& values, size_t i);
}  // namespace cutout
