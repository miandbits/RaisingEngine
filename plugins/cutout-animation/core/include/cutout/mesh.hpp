// Planar mesh geometry for authoring (plugin 1.6.0): constrained Delaunay triangulation of an
// outline polygon with interior points and constraint edges, local edits of an existing
// triangulation, and alpha-outline tracing. Image-pixel coordinates, no Qt, deterministic (no
// unordered iteration, plain IEEE arithmetic) so the native core and Wasm agree bit for bit.
#pragma once
#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "editing.hpp"

namespace cutout {
/** Signed area of the first `count` x, y pairs (positive = the orientation meshes store their
 *  outline in: clockwise on a y-down screen, like convertToMesh's grid hull). */
double polygonArea(const std::vector<double>& points, size_t count);
/** Why the closed polygon (x, y pairs) is not usable as a mesh outline, as user-facing Korean
 *  text; "" when it is simple: >= 3 distinct points, no edge crossing or touching a non-adjacent
 *  edge, no spike (an edge folding back on the previous one), non-zero area. */
std::string polygonProblem(const std::vector<double>& points);
/** True when (x, y) is strictly inside the polygon (farther than `margin` from every edge). */
bool insidePolygon(const std::vector<double>& polygon, double x, double y, double margin = 0);
/** Interior points every `spacing` px on a square grid, kept at least spacing / 2 from the outline. */
std::vector<double> interiorGrid(const std::vector<double>& polygon, double spacing);
/**
 * Outline of the largest opaque region of an alpha mask (one byte per pixel, row-major): pixels
 * with alpha >= threshold * 255 (threshold 0..1), grown by the simplification tolerance so the
 * simplified polygon does not cut into the picture, traced with marching squares (pixel-centre
 * samples, so points lie on [0, w] x [0, h]) and simplified with Douglas-Peucker. `detail` 0..1:
 * tolerance = 0.5 + (1 - detail) * 4 % of the larger side px. At most `maxPoints` points (the
 * tolerance grows until it fits). Positive orientation. Throws EditError for an empty mask.
 */
std::vector<double> traceAlphaOutline(const std::vector<uint8_t>& alpha, int width, int height, double threshold, double detail, size_t maxPoints = 256);

/**
 * A triangulation being edited. Triangles are kept counter-clockwise in math terms (positive
 * cross product); edges used by one triangle are the mesh outline and are never flipped, nor are
 * `constraints` (internal edges the author pinned, plus the hull edges of fromOutline).
 */
class PlanarMesh {
 public:
  /** An existing triangulation (index triples into points). Throws EditError when triangles are
   *  degenerate or overlap (a directed edge used twice). Constraints that are not edges are dropped. */
  PlanarMesh(const std::vector<double>& points, const std::vector<int>& triangles, const std::vector<int>& constraints);
  /** Constrained Delaunay triangulation: the first `hull` points are the outline (simple, either
   *  direction), the rest interior points; `constraints` = internal edge index pairs. Throws
   *  EditError for a bad outline, a point outside / on it or doubled, an edge that cannot be pinned. */
  static PlanarMesh fromOutline(const std::vector<double>& points, size_t hull, const std::vector<int>& constraints);

  /** Insert a new point (index = previous count). On an outline edge it splits that edge only
   *  when allowOutline (else refused); `split` receives the outline edge it landed on (or -1, -1). */
  int addPoint(double x, double y, bool allowOutline, std::pair<int, int>* split = nullptr);
  /** Pin an internal edge between two vertices (flipping crossing edges away). */
  void addConstraint(int a, int b);
  /** Unpin an internal edge (it may flip away afterwards). */
  void removeConstraint(int a, int b);
  /** Remove an interior vertex: its neighbourhood is re-triangulated; the index stays unused. */
  void removeVertex(int v);
  /** Flip every unpinned edge until the triangulation is (constrained) Delaunay. */
  void delaunay();

  bool outlineVertex(int v) const;
  bool isEdge(int a, int b) const { return half_.count(key(a, b)) || half_.count(key(b, a)); }
  bool outlineEdge(int a, int b) const { return isEdge(a, b) && !(half_.count(key(a, b)) && half_.count(key(b, a))); }
  /** Live triangles (creation order) and pinned internal edges (sorted pairs). */
  std::vector<int> triangles() const;
  std::vector<int> constraints() const;
  const std::vector<double>& points() const { return xy_; }

 private:
  PlanarMesh() = default;
  static uint64_t key(int a, int b) { return (uint64_t(uint32_t(a)) << 32) | uint32_t(b); }
  static uint64_t edgeKey(int a, int b) { return a < b ? key(a, b) : key(b, a); }
  double x(int i) const { return xy_[2 * size_t(i)]; }
  double y(int i) const { return xy_[2 * size_t(i) + 1]; }
  int side(int a, int b, double px, double py) const;
  void setScale();
  int addTriangle(int a, int b, int c);
  void killTriangle(int t);
  int third(int t, int a, int b) const;
  void flip(int a, int b);
  bool flippable(int a, int b, int c, int d) const;
  bool crosses(int a, int b, int u, int v) const;
  void legalize(std::vector<std::pair<int, int>> stack);
  void insertAt(int index, bool allowOutline, std::pair<int, int>* split);
  void earClip(const std::vector<int>& polygon);
  std::vector<double> xy_;
  std::vector<std::array<int, 3>> tris_;
  std::vector<bool> alive_;
  std::vector<bool> removed_;               // per point: not (or no longer) part of the triangulation
  std::unordered_map<uint64_t, int> half_;  // directed edge -> triangle (lookups only, never iterated)
  std::set<uint64_t> fixed_;                // pinned undirected edges
  double tol_ = 1e-7;                       // distance below which points are on a line / coincide
};
}  // namespace cutout
