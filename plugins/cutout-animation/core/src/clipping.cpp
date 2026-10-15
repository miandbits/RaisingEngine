#include "cutout/clipping.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>

namespace cutout {
namespace {
constexpr double INSIDE_EPS = 1e-9;  // px: points this close to a clip edge count as inside
// Positions are snapped to 1/4096 px before cutting. Bone matrices come from sin/cos, which may
// differ in the last bit between toolchains; without the snap a vertex lying on a clip edge could
// be kept on one runtime and cut on another. Snapped inputs make every later step (plain IEEE
// arithmetic, sqrt only) bit-identical, so native and Wasm emit the same triangles. 0.00025 px is
// far below anything visible.
constexpr double SNAP = 4096;
double snap(double v) { return std::round(v * SNAP) / SNAP; }
double cross(double ox, double oy, double ax, double ay, double bx, double by) { return (ax - ox) * (by - oy) - (ay - oy) * (bx - ox); }
double signedArea(const std::vector<double>& p) {
  double s = 0;
  const size_t n = p.size() / 2;
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    s += p[2 * i] * p[2 * j + 1] - p[2 * j] * p[2 * i + 1];
  }
  return s / 2;
}
/** Signed distance (times nothing: already normalized by the edge length) of (x, y) from edge a -> b; > 0 = left (inside). */
double side(double ax, double ay, double bx, double by, double x, double y) {
  const double len = std::sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay));  // sqrt is exactly rounded, hypot is libm
  return len > 0 ? cross(ax, ay, bx, by, x, y) / len : 0;
}
bool insidePart(const std::vector<double>& part, double x, double y) {
  const size_t n = part.size() / 2;
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    if (side(part[2 * i], part[2 * i + 1], part[2 * j], part[2 * j + 1], x, y) < -INSIDE_EPS) return false;
  }
  return true;
}
bool lessPoint(const ClipVertex& a, const ClipVertex& b) { return a.x < b.x || (a.x == b.x && a.y < b.y); }
}  // namespace

std::vector<std::vector<double>> convexParts(const std::vector<double>& input) {
  // Distinct consecutive points, positive orientation.
  std::vector<double> p;
  for (size_t i = 0; i + 1 < input.size(); i += 2) {
    const size_t n = p.size();
    if (n >= 2 && p[n - 2] == input[i] && p[n - 1] == input[i + 1]) continue;
    p.push_back(input[i]);
    p.push_back(input[i + 1]);
  }
  while (p.size() >= 4 && p[0] == p[p.size() - 2] && p[1] == p[p.size() - 1]) p.resize(p.size() - 2);
  if (p.size() < 6) return {};
  double minX = p[0], maxX = p[0], minY = p[1], maxY = p[1];
  for (size_t i = 0; i < p.size(); i += 2) minX = std::min(minX, p[i]), maxX = std::max(maxX, p[i]), minY = std::min(minY, p[i + 1]), maxY = std::max(maxY, p[i + 1]);
  const double extent = std::max(maxX - minX, maxY - minY), tiny = 1e-12 * extent * extent;
  const double area = signedArea(p);
  if (!(std::fabs(area) > tiny)) return {};
  const size_t n = p.size() / 2;
  std::vector<int> ring(n);
  for (size_t i = 0; i < n; i++) ring[i] = int(area > 0 ? i : n - 1 - i);
  auto X = [&](int i) { return p[2 * size_t(i)]; };
  auto Y = [&](int i) { return p[2 * size_t(i) + 1]; };
  auto turn = [&](int a, int b, int c) { return cross(X(a), Y(a), X(b), Y(b), X(c), Y(c)); };
  // Ear clipping: the first convex corner whose triangle holds no other ring vertex.
  std::vector<std::array<int, 3>> tris;
  while (ring.size() > 3) {
    const size_t m = ring.size();
    bool cut = false;
    for (size_t k = 0; k < m && !cut; k++) {
      const int a = ring[(k + m - 1) % m], b = ring[k], c = ring[(k + 1) % m];
      const double t = turn(a, b, c);
      if (std::fabs(t) <= tiny) { ring.erase(ring.begin() + int64_t(k)); cut = true; break; }  // collinear: the vertex adds nothing
      if (t < 0) continue;  // reflex
      bool empty = true;
      for (size_t j = 0; j < m && empty; j++) {
        const int q = ring[j];
        if (q == a || q == b || q == c || (X(q) == X(a) && Y(q) == Y(a)) || (X(q) == X(b) && Y(q) == Y(b)) || (X(q) == X(c) && Y(q) == Y(c))) continue;
        empty = !(turn(a, b, q) >= -tiny && turn(b, c, q) >= -tiny && turn(c, a, q) >= -tiny);
      }
      if (!empty) continue;
      tris.push_back({a, b, c});
      ring.erase(ring.begin() + int64_t(k));
      cut = true;
    }
    if (!cut) {
      // No ear: a pose folded the polygon over itself. Fan the rest (bounded, deterministic).
      for (size_t k = 1; k + 1 < ring.size(); k++) if (turn(ring[0], ring[k], ring[k + 1]) > tiny) tris.push_back({ring[0], ring[k], ring[k + 1]});
      ring.clear();
    }
  }
  if (ring.size() == 3 && turn(ring[0], ring[1], ring[2]) > tiny) tris.push_back({ring[0], ring[1], ring[2]});
  // Hertel–Mehlhorn style merge: a triangle joins a part across a shared edge when the union stays convex.
  std::vector<std::vector<int>> parts;
  for (const auto& t : tris) {
    bool merged = false;
    for (auto& part : parts) {
      const size_t m = part.size();
      for (size_t k = 0; k < m && !merged; k++) {
        const int u = part[k], v = part[(k + 1) % m];
        int w = -1;
        for (int e = 0; e < 3; e++) if (t[size_t(e)] == v && t[size_t((e + 1) % 3)] == u) w = t[size_t((e + 2) % 3)];
        if (w < 0) continue;
        const int before = part[(k + m - 1) % m], after = part[(k + 2) % m];
        if (turn(before, u, w) < -tiny || turn(u, w, v) < -tiny || turn(w, v, after) < -tiny) continue;
        part.insert(part.begin() + int64_t(k) + 1, w);
        merged = true;
      }
      if (merged) break;
    }
    if (!merged) parts.push_back({t[0], t[1], t[2]});
  }
  std::vector<std::vector<double>> out;
  for (const auto& part : parts) {
    std::vector<double> xy;
    for (int i : part) { xy.push_back(X(i)); xy.push_back(Y(i)); }
    out.push_back(std::move(xy));
  }
  return out;
}

std::vector<ClipVertex> clipConvex(const std::vector<ClipVertex>& subject, const std::vector<double>& clip) {
  std::vector<ClipVertex> out = subject, in;
  const size_t n = clip.size() / 2;
  for (size_t e = 0; e < n && out.size() >= 3; e++) {
    const double ax = clip[2 * e], ay = clip[2 * e + 1], bx = clip[2 * ((e + 1) % n)], by = clip[2 * ((e + 1) % n) + 1];
    in.swap(out);
    out.clear();
    const size_t m = in.size();
    for (size_t i = 0; i < m; i++) {
      const ClipVertex& cur = in[i];
      const ClipVertex& prev = in[(i + m - 1) % m];
      const double dc = side(ax, ay, bx, by, cur.x, cur.y), dp = side(ax, ay, bx, by, prev.x, prev.y);
      const bool curIn = dc >= -INSIDE_EPS, prevIn = dp >= -INSIDE_EPS;
      if (curIn != prevIn) {
        // Canonical endpoint order: the neighbour triangle cutting the same edge gets the same point.
        ClipVertex p = prev, q = cur;
        double sp = dp, sq = dc;
        if (lessPoint(q, p)) { std::swap(p, q); std::swap(sp, sq); }
        const double t = std::clamp(sp / (sp - sq), 0.0, 1.0);
        out.push_back({p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t, p.u + (q.u - p.u) * t, p.v + (q.v - p.v) * t});
      }
      if (curIn) out.push_back(cur);
    }
    // Repeated points (a vertex on the edge) would only make empty triangles.
    std::vector<ClipVertex> unique;
    for (const auto& v : out) if (unique.empty() || v.x != unique.back().x || v.y != unique.back().y) unique.push_back(v);
    while (unique.size() > 1 && unique.front().x == unique.back().x && unique.front().y == unique.back().y) unique.pop_back();
    out.swap(unique);
  }
  if (out.size() < 3) out.clear();
  return out;
}

ClipRegion clipRegion(const std::vector<double>& polygonIn) {
  ClipRegion r;
  std::vector<double> polygon = polygonIn;
  for (double& v : polygon) v = snap(v);
  r.parts = convexParts(polygon);
  bool first = true;
  for (const auto& part : r.parts)
    for (size_t i = 0; i + 1 < part.size(); i += 2) {
      if (first) { r.minX = r.maxX = part[i]; r.minY = r.maxY = part[i + 1]; first = false; continue; }
      r.minX = std::min(r.minX, part[i]); r.maxX = std::max(r.maxX, part[i]);
      r.minY = std::min(r.minY, part[i + 1]); r.maxY = std::max(r.maxY, part[i + 1]);
    }
  return r;
}

void clipItem(const DrawItem& item, const ClipRegion& region, std::vector<DrawItem>& out) {
  if (region.parts.empty()) return;  // a polygon without area shows nothing
  std::vector<double> pos, uv;
  std::vector<int> idx;
  if (item.mesh()) { pos = item.positions; uv = item.uvs; idx = item.indices; }
  else {
    // A region is its picture rectangle: two triangles over uvs 0..1 (of the atlas sub-image when packed).
    const double corners[4][2] = {{0, 0}, {item.width, 0}, {item.width, item.height}, {0, item.height}}, uvs[8] = {0, 0, 1, 0, 1, 1, 0, 1};
    for (const auto& c : corners) { double x, y; item.matrix.apply(c[0], c[1], x, y); pos.push_back(x); pos.push_back(y); }
    uv.assign(uvs, uvs + 8);
    idx = {0, 1, 2, 0, 2, 3};
  }
  if (pos.size() < 6 || uv.size() != pos.size()) return;
  for (double& v : pos) v = snap(v);
  double minX = pos[0], maxX = pos[0], minY = pos[1], maxY = pos[1];
  for (size_t i = 0; i + 1 < pos.size(); i += 2) minX = std::min(minX, pos[i]), maxX = std::max(maxX, pos[i]), minY = std::min(minY, pos[i + 1]), maxY = std::max(maxY, pos[i + 1]);
  if (maxX < region.minX - INSIDE_EPS || minX > region.maxX + INSIDE_EPS || maxY < region.minY - INSIDE_EPS || minY > region.maxY + INSIDE_EPS) return;
  auto vertex = [&](int i) { return ClipVertex{pos[2 * size_t(i)], pos[2 * size_t(i) + 1], uv[2 * size_t(i)], uv[2 * size_t(i) + 1]}; };
  std::vector<std::vector<ClipVertex>> pieces;
  bool whole = true;
  for (size_t t = 0; t + 2 < idx.size(); t += 3) {
    const ClipVertex a = vertex(idx[t]), b = vertex(idx[t + 1]), c = vertex(idx[t + 2]);
    bool inside = false;
    for (const auto& part : region.parts)
      if (insidePart(part, a.x, a.y) && insidePart(part, b.x, b.y) && insidePart(part, c.x, c.y)) { inside = true; break; }
    if (inside) { pieces.push_back({a, b, c}); continue; }
    whole = false;
    // The parts tile the polygon without overlap: the pieces of one triangle never double-draw.
    for (const auto& part : region.parts) {
      auto piece = clipConvex({a, b, c}, part);
      if (piece.size() >= 3) pieces.push_back(std::move(piece));
    }
  }
  if (whole) { out.push_back(item); return; }
  if (pieces.empty()) return;
  DrawItem chunk = item;
  std::map<std::array<double, 4>, int> shared;
  auto begin = [&] {
    chunk.positions.clear(); chunk.uvs.clear(); chunk.indices.clear();
    chunk.clipped = true;
    shared.clear();
  };
  begin();
  for (const auto& piece : pieces) {
    const size_t tris = piece.size() - 2;
    if (chunk.positions.size() / 2 + piece.size() > MAX_MESH_VERTICES || chunk.indices.size() / 3 + tris > MAX_MESH_TRIANGLES) { out.push_back(chunk); begin(); }
    std::vector<int> at;
    for (const auto& v : piece) {
      auto [it, added] = shared.insert({{v.x, v.y, v.u, v.v}, int(chunk.positions.size() / 2)});
      if (added) { chunk.positions.push_back(v.x); chunk.positions.push_back(v.y); chunk.uvs.push_back(v.u); chunk.uvs.push_back(v.v); }
      at.push_back(it->second);
    }
    for (size_t k = 1; k + 1 < at.size(); k++) { chunk.indices.push_back(at[0]); chunk.indices.push_back(at[k]); chunk.indices.push_back(at[k + 1]); }
  }
  if (!chunk.indices.empty()) out.push_back(chunk);
}
}  // namespace cutout
