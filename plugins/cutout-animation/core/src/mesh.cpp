#include "cutout/mesh.hpp"
#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>

namespace cutout {
namespace {
using Str = std::string;
double cross(double ax, double ay, double bx, double by, double cx, double cy) { return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax); }
// sqrt only (no hypot): IEEE-exact everywhere, so native and Wasm triangulate identically.
double length(double dx, double dy) { return std::sqrt(dx * dx + dy * dy); }
/** Bounding-box diagonal of x, y pairs (>= 1): tolerances scale with the drawing. */
double scaleOf(const std::vector<double>& xy, size_t count) {
  if (count == 0) return 1;
  double x0 = xy[0], x1 = xy[0], y0 = xy[1], y1 = xy[1];
  for (size_t i = 1; i < count; i++) {
    x0 = std::min(x0, xy[2 * i]); x1 = std::max(x1, xy[2 * i]);
    y0 = std::min(y0, xy[2 * i + 1]); y1 = std::max(y1, xy[2 * i + 1]);
  }
  return std::max(1.0, length(x1 - x0, y1 - y0));
}
double segmentDistance(double px, double py, double ax, double ay, double bx, double by) {
  const double vx = bx - ax, vy = by - ay, l2 = vx * vx + vy * vy;
  const double u = l2 > 0 ? std::clamp(((px - ax) * vx + (py - ay) * vy) / l2, 0.0, 1.0) : 0;
  return length(px - (ax + u * vx), py - (ay + u * vy));
}
/** Signed distance of c from the line a->b (positive = left in math terms). */
double lineDistance(double ax, double ay, double bx, double by, double cx, double cy) {
  const double l = length(bx - ax, by - ay);
  return l > 0 ? cross(ax, ay, bx, by, cx, cy) / l : length(cx - ax, cy - ay);
}
/** Closed segments ab and cd cross or touch (within tol). */
bool segmentsTouch(const double* a, const double* b, const double* c, const double* d, double tol) {
  if (std::max(a[0], b[0]) + tol < std::min(c[0], d[0]) || std::max(c[0], d[0]) + tol < std::min(a[0], b[0]) ||
      std::max(a[1], b[1]) + tol < std::min(c[1], d[1]) || std::max(c[1], d[1]) + tol < std::min(a[1], b[1])) return false;
  const double o1 = lineDistance(a[0], a[1], b[0], b[1], c[0], c[1]), o2 = lineDistance(a[0], a[1], b[0], b[1], d[0], d[1]);
  const double o3 = lineDistance(c[0], c[1], d[0], d[1], a[0], a[1]), o4 = lineDistance(c[0], c[1], d[0], d[1], b[0], b[1]);
  auto opposite = [&](double p, double q) { return (p > tol && q < -tol) || (p < -tol && q > tol); };
  if (opposite(o1, o2) && opposite(o3, o4)) return true;
  return segmentDistance(c[0], c[1], a[0], a[1], b[0], b[1]) <= tol || segmentDistance(d[0], d[1], a[0], a[1], b[0], b[1]) <= tol ||
         segmentDistance(a[0], a[1], c[0], c[1], d[0], d[1]) <= tol || segmentDistance(b[0], b[1], c[0], c[1], d[0], d[1]) <= tol;
}
}  // namespace

double polygonArea(const std::vector<double>& p, size_t n) {
  double s = 0;
  for (size_t i = 0; i < n; i++) {
    const size_t j = (i + 1) % n;
    s += p[2 * i] * p[2 * j + 1] - p[2 * j] * p[2 * i + 1];
  }
  return s / 2;
}

std::string polygonProblem(const std::vector<double>& p) {
  const size_t n = p.size() / 2;
  if (p.size() % 2 || n < 3) return "윤곽에는 점이 3개 이상 필요합니다.";
  const double tol = 1e-6 * scaleOf(p, n);
  for (size_t i = 0; i < n; i++)
    for (size_t j = i + 1; j < n; j++)
      if (length(p[2 * i] - p[2 * j], p[2 * i + 1] - p[2 * j + 1]) <= tol) return "윤곽의 점 " + std::to_string(i) + "과 " + std::to_string(j) + "이 같은 자리입니다.";
  for (size_t i = 0; i < n; i++) {
    const size_t a = (i + n - 1) % n, b = (i + 1) % n;
    const double* pa = &p[2 * a];
    const double* pi = &p[2 * i];
    const double* pb = &p[2 * b];
    if (std::fabs(lineDistance(pa[0], pa[1], pi[0], pi[1], pb[0], pb[1])) <= tol && (pa[0] - pi[0]) * (pb[0] - pi[0]) + (pa[1] - pi[1]) * (pb[1] - pi[1]) > 0)
      return "윤곽이 점 " + std::to_string(i) + "에서 되돌아갑니다(겹치는 가장자리).";
  }
  for (size_t i = 0; i < n; i++)
    for (size_t j = i + 2; j < n; j++) {
      if ((j + 1) % n == i) continue;  // adjacent through the closing edge
      if (segmentsTouch(&p[2 * i], &p[2 * ((i + 1) % n)], &p[2 * j], &p[2 * ((j + 1) % n)], tol))
        return "윤곽이 스스로 교차합니다(가장자리 " + std::to_string(i) + "-" + std::to_string((i + 1) % n) + "와 " + std::to_string(j) + "-" + std::to_string((j + 1) % n) + ").";
    }
  if (std::fabs(polygonArea(p, n)) <= tol * scaleOf(p, n)) return "윤곽의 넓이가 0입니다(점들이 한 줄에 있습니다).";
  return "";
}

bool insidePolygon(const std::vector<double>& p, double x, double y, double margin) {
  const size_t n = p.size() / 2;
  bool inside = false;
  for (size_t i = 0, j = n - 1; i < n; j = i++) {
    const double xi = p[2 * i], yi = p[2 * i + 1], xj = p[2 * j], yj = p[2 * j + 1];
    if (segmentDistance(x, y, xi, yi, xj, yj) <= margin) return false;
    if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) inside = !inside;
  }
  return inside;
}

std::vector<double> interiorGrid(const std::vector<double>& p, double spacing) {
  std::vector<double> out;
  const size_t n = p.size() / 2;
  if (n < 3 || !(spacing > 0)) return out;
  double x0 = p[0], x1 = p[0], y0 = p[1], y1 = p[1];
  for (size_t i = 1; i < n; i++) { x0 = std::min(x0, p[2 * i]); x1 = std::max(x1, p[2 * i]); y0 = std::min(y0, p[2 * i + 1]); y1 = std::max(y1, p[2 * i + 1]); }
  // Centred on the bounding box so the grid sits symmetrically inside symmetric outlines.
  const double ox = x0 + std::fmod(x1 - x0, spacing) / 2, oy = y0 + std::fmod(y1 - y0, spacing) / 2;
  for (double y = oy; y <= y1; y += spacing)
    for (double x = ox; x <= x1; x += spacing)
      if (insidePolygon(p, x, y, spacing / 2)) { out.push_back(x); out.push_back(y); }
  return out;
}

// ---------------------------------------------------------------- alpha outline
namespace {
/** Square dilation by r pixels (separable running counts). */
std::vector<uint8_t> dilate(const std::vector<uint8_t>& in, int w, int h, int r) {
  if (r <= 0) return in;
  std::vector<uint8_t> mid(in.size()), out(in.size());
  std::vector<int> run(size_t(std::max(w, h)) + 1);
  for (int y = 0; y < h; y++) {
    run[0] = 0;
    for (int x = 0; x < w; x++) run[size_t(x) + 1] = run[size_t(x)] + in[size_t(y) * size_t(w) + size_t(x)];
    for (int x = 0; x < w; x++) mid[size_t(y) * size_t(w) + size_t(x)] = run[size_t(std::min(w, x + r + 1))] - run[size_t(std::max(0, x - r))] > 0;
  }
  for (int x = 0; x < w; x++) {
    run[0] = 0;
    for (int y = 0; y < h; y++) run[size_t(y) + 1] = run[size_t(y)] + mid[size_t(y) * size_t(w) + size_t(x)];
    for (int y = 0; y < h; y++) out[size_t(y) * size_t(w) + size_t(x)] = run[size_t(std::min(h, y + r + 1))] - run[size_t(std::max(0, y - r))] > 0;
  }
  return out;
}
/**
 * Marching squares over pixel-centre samples (outside the image = empty), saddles kept apart, so
 * every loop is simple. Returns the loop enclosing the largest area, positive orientation.
 */
std::vector<double> largestLoop(const std::vector<uint8_t>& bin, int w, int h) {
  auto s = [&](int i, int j) { return i >= 0 && j >= 0 && i < w && j < h && bin[size_t(j) * size_t(w) + size_t(i)]; };
  const int64_t W = int64_t(w) + 2;
  auto hId = [&](int i, int j) { return ((int64_t(j) + 1) * W + (int64_t(i) + 1)) * 2; };      // between (i,j) and (i+1,j)
  auto vId = [&](int i, int j) { return ((int64_t(j) + 1) * W + (int64_t(i) + 1)) * 2 + 1; };  // between (i,j) and (i,j+1)
  std::unordered_map<int64_t, std::array<int64_t, 2>> next;
  std::vector<int64_t> order;  // first appearance (scan order): deterministic loop starts
  auto link = [&](int64_t a, int64_t b) {
    for (const auto& [x, y] : {std::pair<int64_t, int64_t>{a, b}, {b, a}}) {
      auto it = next.find(x);
      if (it == next.end()) { next[x] = {y, -1}; order.push_back(x); }
      else it->second[1] = y;
    }
  };
  for (int j = -1; j < h; j++)
    for (int i = -1; i < w; i++) {
      const bool tl = s(i, j), tr = s(i + 1, j), br = s(i + 1, j + 1), bl = s(i, j + 1);
      if (tl == tr && tr == br && br == bl) continue;
      const int64_t top = hId(i, j), bottom = hId(i, j + 1), left = vId(i, j), right = vId(i + 1, j);
      if (tl && br && !tr && !bl) { link(top, left); link(bottom, right); continue; }
      if (tr && bl && !tl && !br) { link(top, right); link(bottom, left); continue; }
      std::vector<int64_t> cut;
      if (tl != tr) cut.push_back(top);
      if (tr != br) cut.push_back(right);
      if (bl != br) cut.push_back(bottom);
      if (tl != bl) cut.push_back(left);
      if (cut.size() == 2) link(cut[0], cut[1]);
    }
  auto coords = [&](int64_t id, double& x, double& y) {
    const int64_t cell = id / 2, i = cell % W - 1, j = cell / W - 1;
    if (id % 2 == 0) { x = double(i) + 1; y = double(j) + 0.5; }
    else { x = double(i) + 0.5; y = double(j) + 1; }
  };
  std::unordered_map<int64_t, bool> seen;
  std::vector<double> best;
  double bestArea = 0;
  for (int64_t start : order) {
    if (seen.count(start)) continue;
    std::vector<double> loop;
    int64_t prev = -1, at = start;
    while (at >= 0 && !seen.count(at)) {
      seen[at] = true;
      double x, y;
      coords(at, x, y);
      loop.push_back(x);
      loop.push_back(y);
      const auto& n = next[at];
      const int64_t to = n[0] != prev ? n[0] : n[1];
      prev = at;
      at = to;
    }
    const double area = polygonArea(loop, loop.size() / 2);
    if (std::fabs(area) > std::fabs(bestArea)) { bestArea = area; best = loop; }
  }
  if (bestArea < 0) {
    std::vector<double> r;
    const size_t n = best.size() / 2;
    for (size_t k = 0; k < n; k++) { const size_t i = (n - k) % n; r.push_back(best[2 * i]); r.push_back(best[2 * i + 1]); }
    best = r;
  }
  return best;
}
/** Douglas-Peucker on a closed loop, split at the point farthest from the first one. */
std::vector<double> simplifyClosed(const std::vector<double>& p, double tol) {
  const size_t n = p.size() / 2;
  if (n <= 4) return p;
  size_t far = 0;
  double farD = -1;
  for (size_t i = 1; i < n; i++) {
    const double d = length(p[2 * i] - p[0], p[2 * i + 1] - p[1]);
    if (d > farD) { farD = d; far = i; }
  }
  std::vector<bool> keep(n + 1, false);
  keep[0] = keep[far] = keep[n] = true;
  auto pt = [&](size_t i) { return &p[2 * (i % n)]; };
  std::vector<std::pair<size_t, size_t>> stack = {{0, far}, {far, n}};
  while (!stack.empty()) {
    const auto [a, b] = stack.back();
    stack.pop_back();
    double worst = -1;
    size_t at = a;
    for (size_t i = a + 1; i < b; i++) {
      const double d = segmentDistance(pt(i)[0], pt(i)[1], pt(a)[0], pt(a)[1], pt(b)[0], pt(b)[1]);
      if (d > worst) { worst = d; at = i; }
    }
    if (worst > tol) { keep[at] = true; stack.push_back({a, at}); stack.push_back({at, b}); }
  }
  std::vector<double> out;
  for (size_t i = 0; i < n; i++) if (keep[i]) { out.push_back(p[2 * i]); out.push_back(p[2 * i + 1]); }
  return out;
}
}  // namespace

std::vector<double> traceAlphaOutline(const std::vector<uint8_t>& alpha, int w, int h, double threshold, double detail, size_t maxPoints) {
  if (w <= 0 || h <= 0 || alpha.size() != size_t(w) * size_t(h)) throw EditError("알파 마스크 크기가 맞지 않습니다.");
  threshold = std::clamp(threshold, 0.0, 1.0);
  detail = std::clamp(detail, 0.0, 1.0);
  std::vector<uint8_t> bin(alpha.size());
  bool any = false;
  for (size_t i = 0; i < alpha.size(); i++) any |= (bin[i] = double(alpha[i]) > threshold * 255 ? 1 : 0);
  if (!any) throw EditError("기준보다 불투명한 픽셀이 없습니다(알파 기준을 낮추세요).");
  double tol = 0.5 + (1 - detail) * 0.04 * double(std::max(w, h));
  for (int attempt = 0; attempt < 24; attempt++) {
    // Grown by the tolerance: the simplified outline then stays around the picture, not inside it.
    const std::vector<double> loop = largestLoop(dilate(bin, w, h, int(std::ceil(tol))), w, h);
    std::vector<double> out = simplifyClosed(loop, tol);
    if (out.size() / 2 > maxPoints) { tol *= 1.5; continue; }
    if (out.size() / 2 >= 3 && polygonProblem(out).empty()) return out;
    if (tol <= 0.5) break;
    tol = std::max(0.5, tol * 0.7);  // a simplification that crosses itself: keep more points
  }
  throw EditError("알파 윤곽을 단순한 다각형으로 만들지 못했습니다. 디테일을 바꿔 보세요.");
}

// ---------------------------------------------------------------- PlanarMesh
void PlanarMesh::setScale() { tol_ = 1e-7 * scaleOf(xy_, xy_.size() / 2); }
int PlanarMesh::side(int a, int b, double px, double py) const {
  const double c = cross(x(a), y(a), x(b), y(b), px, py), l = length(x(b) - x(a), y(b) - y(a));
  if (std::fabs(c) <= tol_ * l) return 0;
  return c > 0 ? 1 : -1;
}
int PlanarMesh::addTriangle(int a, int b, int c) {
  if (cross(x(a), y(a), x(b), y(b), x(c), y(c)) <= 0) throw EditError("삼각분할에 실패했습니다(뒤집힌 삼각형).");
  const int t = int(tris_.size());
  for (const auto& [u, v] : {std::pair<int, int>{a, b}, {b, c}, {c, a}})
    if (!half_.emplace(key(u, v), t).second) throw EditError("삼각형이 겹칩니다(같은 가장자리를 같은 방향으로 쓰는 삼각형이 둘).");
  tris_.push_back({a, b, c});
  alive_.push_back(true);
  return t;
}
void PlanarMesh::killTriangle(int t) {
  const auto& v = tris_[size_t(t)];
  for (int e = 0; e < 3; e++) half_.erase(key(v[size_t(e)], v[size_t((e + 1) % 3)]));
  alive_[size_t(t)] = false;
}
int PlanarMesh::third(int t, int a, int b) const {
  for (int v : tris_[size_t(t)]) if (v != a && v != b) return v;
  return -1;
}
// t1 = (a, b, c) and t2 = (b, a, d) become (a, d, c) and (d, b, c): the quad a, d, b, c keeps its orientation.
void PlanarMesh::flip(int a, int b) {
  const int t1 = half_.at(key(a, b)), t2 = half_.at(key(b, a));
  const int c = third(t1, a, b), d = third(t2, b, a);
  killTriangle(t1);
  killTriangle(t2);
  addTriangle(a, d, c);
  addTriangle(d, b, c);
}
bool PlanarMesh::flippable(int a, int b, int c, int d) const {
  const int s1 = side(c, d, x(a), y(a)), s2 = side(c, d, x(b), y(b));
  return s1 != 0 && s2 != 0 && s1 != s2;
}
bool PlanarMesh::crosses(int a, int b, int u, int v) const {
  if (u == a || u == b || v == a || v == b) return false;
  return side(a, b, x(u), y(u)) * side(a, b, x(v), y(v)) < 0 && side(u, v, x(a), y(a)) * side(u, v, x(b), y(b)) < 0;
}
void PlanarMesh::legalize(std::vector<std::pair<int, int>> stack) {
  size_t flips = 0;
  const size_t limit = 200000 + 64 * tris_.size();
  while (!stack.empty()) {
    const auto [a, b] = stack.back();
    stack.pop_back();
    if (fixed_.count(edgeKey(a, b))) continue;
    const auto i1 = half_.find(key(a, b)), i2 = half_.find(key(b, a));
    if (i1 == half_.end() || i2 == half_.end()) continue;  // outline edge
    const int c = third(i1->second, a, b), d = third(i2->second, b, a);
    // In-circle test of d against (a, b, c), relative to the points' spread: cocircular points
    // (grids) never flip back and forth.
    const double adx = x(a) - x(d), ady = y(a) - y(d), bdx = x(b) - x(d), bdy = y(b) - y(d), cdx = x(c) - x(d), cdy = y(c) - y(d);
    const double det = (adx * adx + ady * ady) * (bdx * cdy - cdx * bdy) + (bdx * bdx + bdy * bdy) * (cdx * ady - adx * cdy) + (cdx * cdx + cdy * cdy) * (adx * bdy - bdx * ady);
    const double spread = std::max({std::fabs(adx), std::fabs(ady), std::fabs(bdx), std::fabs(bdy), std::fabs(cdx), std::fabs(cdy)});
    if (!(det > 1e-9 * spread * spread * spread * spread) || !flippable(a, b, c, d)) continue;
    flip(a, b);
    if (++flips > limit) return;  // never loops forever on numerically hostile input
    stack.push_back({a, d});
    stack.push_back({d, b});
    stack.push_back({b, c});
    stack.push_back({c, a});
  }
}
void PlanarMesh::earClip(const std::vector<int>& polygon) {
  std::vector<int> ring = polygon;
  std::vector<std::pair<int, int>> created;
  auto inside = [&](int a, int b, int c, int v) { return side(a, b, x(v), y(v)) >= 0 && side(b, c, x(v), y(v)) >= 0 && side(c, a, x(v), y(v)) >= 0; };
  size_t start = 0;
  while (ring.size() > 3) {
    const size_t n = ring.size();
    bool clipped = false;
    for (size_t k = 0; k < n && !clipped; k++) {
      const size_t i = (start + k) % n;
      const int a = ring[(i + n - 1) % n], b = ring[i], c = ring[(i + 1) % n];
      if (side(a, c, x(b), y(b)) >= 0) continue;  // reflex or flat corner: a convex one lies right of a->c
      bool empty = true;
      for (int v : ring) if (v != a && v != b && v != c && inside(a, b, c, v)) { empty = false; break; }
      if (!empty) continue;
      addTriangle(a, b, c);
      ring.erase(ring.begin() + std::ptrdiff_t(i));
      start = i % ring.size();
      clipped = true;
    }
    if (!clipped) throw EditError("윤곽을 삼각형으로 나누지 못했습니다(겹치거나 한 줄에 놓인 점).");
  }
  addTriangle(ring[0], ring[1], ring[2]);
}
void PlanarMesh::insertAt(int q, bool allowOutline, std::pair<int, int>* split) {
  if (split) *split = {-1, -1};
  const double qx = x(q), qy = y(q);
  for (size_t j = 0; j < removed_.size(); j++)
    if (int(j) != q && !removed_[j] && length(xy_[2 * j] - qx, xy_[2 * j + 1] - qy) <= 10 * tol_)
      throw EditError("같은 자리에 정점이 이미 있습니다(정점 " + std::to_string(j) + ").");
  int found = -1, onEdge = -1;
  for (size_t t = 0; t < tris_.size() && found < 0; t++) {
    if (!alive_[t]) continue;
    const auto& v = tris_[t];
    int zeros = 0, zeroEdge = -1;
    bool out = false;
    for (int e = 0; e < 3 && !out; e++) {
      const int s = side(v[size_t(e)], v[size_t((e + 1) % 3)], qx, qy);
      if (s < 0) out = true;
      else if (s == 0) { zeros++; if (zeroEdge < 0) zeroEdge = e; }
    }
    if (out) continue;
    found = int(t);
    onEdge = zeros ? zeroEdge : -1;
  }
  if (found < 0) throw EditError("메시 밖입니다. 정점은 메시 안쪽에 추가하세요.");
  const auto v = tris_[size_t(found)];
  if (onEdge < 0) {
    killTriangle(found);
    addTriangle(v[0], v[1], q);
    addTriangle(v[1], v[2], q);
    addTriangle(v[2], v[0], q);
    legalize({{v[0], v[1]}, {v[1], v[2]}, {v[2], v[0]}});
    return;
  }
  const int a = v[size_t(onEdge)], b = v[size_t((onEdge + 1) % 3)], c = v[size_t((onEdge + 2) % 3)];
  const bool pinned = fixed_.erase(edgeKey(a, b)) > 0;
  if (pinned) { fixed_.insert(edgeKey(a, q)); fixed_.insert(edgeKey(q, b)); }
  const auto rev = half_.find(key(b, a));
  if (rev == half_.end()) {
    if (!allowOutline) {
      if (pinned) { fixed_.erase(edgeKey(a, q)); fixed_.erase(edgeKey(q, b)); fixed_.insert(edgeKey(a, b)); }
      throw EditError("윤곽 위입니다. 정점은 윤곽 안쪽에 추가하세요.");
    }
    if (split) *split = {a, b};
    killTriangle(found);
    addTriangle(a, q, c);
    addTriangle(q, b, c);
    legalize({{c, a}, {b, c}});
    return;
  }
  const int t2 = rev->second, d = third(t2, b, a);
  killTriangle(found);
  killTriangle(t2);
  addTriangle(a, q, c);
  addTriangle(q, b, c);
  addTriangle(b, q, d);
  addTriangle(q, a, d);
  legalize({{c, a}, {b, c}, {d, b}, {a, d}});
}
int PlanarMesh::addPoint(double px, double py, bool allowOutline, std::pair<int, int>* split) {
  xy_.push_back(px);
  xy_.push_back(py);
  removed_.push_back(false);
  const int q = int(removed_.size()) - 1;
  try { insertAt(q, allowOutline, split); }
  catch (...) { xy_.resize(xy_.size() - 2); removed_.pop_back(); throw; }
  return q;
}
void PlanarMesh::addConstraint(int a, int b) {
  const int n = int(removed_.size());
  if (a < 0 || b < 0 || a >= n || b >= n || a == b || removed_[size_t(a)] || removed_[size_t(b)]) throw EditError("가장자리는 서로 다른 두 정점을 잇습니다.");
  if (isEdge(a, b)) { fixed_.insert(edgeKey(a, b)); return; }
  for (int v = 0; v < n; v++) {
    if (v == a || v == b || removed_[size_t(v)]) continue;
    const double ax = x(a), ay = y(a), bx = x(b), by = y(b), l2 = (bx - ax) * (bx - ax) + (by - ay) * (by - ay);
    const double u = ((x(v) - ax) * (bx - ax) + (y(v) - ay) * (by - ay)) / l2;
    if (u > 0 && u < 1 && segmentDistance(x(v), y(v), ax, ay, bx, by) <= 10 * tol_)
      throw EditError("가장자리 " + std::to_string(a) + "-" + std::to_string(b) + "가 다른 정점(" + std::to_string(v) + ")을 지납니다.");
  }
  std::deque<std::pair<int, int>> crossing;
  std::set<uint64_t> listed;
  for (size_t t = 0; t < tris_.size(); t++) {
    if (!alive_[t]) continue;
    for (int e = 0; e < 3; e++) {
      const int u = tris_[t][size_t(e)], v = tris_[t][size_t((e + 1) % 3)];
      if (!crosses(a, b, u, v) || !listed.insert(edgeKey(u, v)).second) continue;
      if (fixed_.count(edgeKey(u, v)) || outlineEdge(u, v))
        throw EditError("가장자리 " + std::to_string(a) + "-" + std::to_string(b) + "가 다른 가장자리나 윤곽(" + std::to_string(u) + "-" + std::to_string(v) + ")과 교차합니다.");
      crossing.push_back({u, v});
    }
  }
  if (crossing.empty()) throw EditError("가장자리 " + std::to_string(a) + "-" + std::to_string(b) + "가 메시 밖을 지납니다.");
  std::vector<std::pair<int, int>> fresh;
  for (size_t guard = 0; !crossing.empty(); guard++) {
    if (guard > 1000000) throw EditError("가장자리를 넣지 못했습니다.");
    const auto [u, v] = crossing.front();
    crossing.pop_front();
    const auto i1 = half_.find(key(u, v)), i2 = half_.find(key(v, u));
    if (i1 == half_.end() || i2 == half_.end()) continue;
    const int c = third(i1->second, u, v), d = third(i2->second, v, u);
    if (!flippable(u, v, c, d)) { crossing.push_back({u, v}); continue; }  // Sloan: retry once its quad turns convex
    flip(u, v);
    if (crosses(a, b, c, d)) crossing.push_back({c, d});
    else fresh.push_back({c, d});
  }
  fixed_.insert(edgeKey(a, b));
  legalize(fresh);
}
void PlanarMesh::removeConstraint(int a, int b) {
  fixed_.erase(edgeKey(a, b));
  legalize({{a, b}});
}
bool PlanarMesh::outlineVertex(int v) const {
  for (size_t t = 0; t < tris_.size(); t++) {
    if (!alive_[t]) continue;
    for (int e = 0; e < 3; e++) {
      const int a = tris_[t][size_t(e)], b = tris_[t][size_t((e + 1) % 3)];
      if ((a == v || b == v) && !half_.count(key(b, a))) return true;
    }
  }
  return false;
}
void PlanarMesh::removeVertex(int v) {
  int start = -1;
  for (size_t t = 0; t < tris_.size() && start < 0; t++) if (alive_[t] && (tris_[t][0] == v || tris_[t][1] == v || tris_[t][2] == v)) start = int(t);
  if (start < 0) throw EditError("삼각형에 쓰이지 않는 정점입니다: " + std::to_string(v));
  // Walk the fan around v counter-clockwise: (v, x, y) -> the triangle holding v->y.
  std::vector<int> ring, fan;
  int t = start;
  for (size_t guard = 0;; guard++) {
    const auto& tv = tris_[size_t(t)];
    const int k = tv[0] == v ? 0 : tv[1] == v ? 1 : 2;
    const int xv = tv[size_t((k + 1) % 3)], yv = tv[size_t((k + 2) % 3)];
    fan.push_back(t);
    ring.push_back(xv);
    const auto it = half_.find(key(v, yv));
    if (it == half_.end() || guard > tris_.size()) throw EditError("윤곽 위의 정점(" + std::to_string(v) + ")은 이렇게 지울 수 없습니다.");
    t = it->second;
    if (yv == ring[0]) break;
  }
  for (int u : ring) fixed_.erase(edgeKey(v, u));
  for (int f : fan) killTriangle(f);
  const size_t before = tris_.size();
  earClip(ring);
  removed_[size_t(v)] = true;
  std::vector<std::pair<int, int>> edges;
  for (size_t k = before; k < tris_.size(); k++)
    for (int e = 0; e < 3; e++) edges.push_back({tris_[k][size_t(e)], tris_[k][size_t((e + 1) % 3)]});
  legalize(edges);
}
void PlanarMesh::delaunay() {
  std::vector<std::pair<int, int>> edges;
  for (size_t t = 0; t < tris_.size(); t++)
    if (alive_[t]) for (int e = 0; e < 3; e++) edges.push_back({tris_[t][size_t(e)], tris_[t][size_t((e + 1) % 3)]});
  std::reverse(edges.begin(), edges.end());  // the stack pops the first triangles first
  legalize(edges);
}
std::vector<int> PlanarMesh::triangles() const {
  std::vector<int> out;
  for (size_t t = 0; t < tris_.size(); t++) if (alive_[t]) out.insert(out.end(), tris_[t].begin(), tris_[t].end());
  return out;
}
std::vector<int> PlanarMesh::constraints() const {
  std::vector<int> out;
  for (uint64_t k : fixed_) {
    const int a = int(k >> 32), b = int(k & 0xffffffffu);
    if (half_.count(key(a, b)) && half_.count(key(b, a))) { out.push_back(a); out.push_back(b); }
  }
  return out;
}

PlanarMesh::PlanarMesh(const std::vector<double>& points, const std::vector<int>& triangles, const std::vector<int>& constraints) {
  xy_ = points;
  removed_.assign(points.size() / 2, false);
  setScale();
  const int n = int(points.size() / 2);
  for (size_t t = 0; t + 2 < triangles.size(); t += 3) {
    int a = triangles[t], b = triangles[t + 1], c = triangles[t + 2];
    if (a < 0 || b < 0 || c < 0 || a >= n || b >= n || c >= n) throw EditError("삼각형의 정점 번호가 범위를 벗어났습니다.");
    double area = cross(x(a), y(a), x(b), y(b), x(c), y(c));
    if (area < 0) { std::swap(b, c); area = -area; }
    if (area <= tol_ * std::max({length(x(b) - x(a), y(b) - y(a)), length(x(c) - x(b), y(c) - y(b)), length(x(a) - x(c), y(a) - y(c))}))
      throw EditError("넓이가 0인 삼각형이 있습니다(정점 " + std::to_string(a) + ", " + std::to_string(b) + ", " + std::to_string(c) + "). 「다시 삼각분할」 전에 정점을 옮기세요.");
    addTriangle(a, b, c);
  }
  // Vertices no triangle uses cannot take part in edits.
  std::vector<bool> used(size_t(n), false);
  for (int v : triangles) if (v >= 0 && v < n) used[size_t(v)] = true;
  for (int v = 0; v < n; v++) if (!used[size_t(v)]) removed_[size_t(v)] = true;
  for (size_t k = 0; k + 1 < constraints.size(); k += 2)
    if (isEdge(constraints[k], constraints[k + 1]) && !outlineEdge(constraints[k], constraints[k + 1])) fixed_.insert(edgeKey(constraints[k], constraints[k + 1]));
}

PlanarMesh PlanarMesh::fromOutline(const std::vector<double>& points, size_t hull, const std::vector<int>& constraints) {
  const size_t n = points.size() / 2;
  if (hull < 3 || hull > n) throw EditError("윤곽에는 점이 3개 이상 필요합니다.");
  const std::vector<double> outline(points.begin(), points.begin() + std::ptrdiff_t(2 * hull));
  if (const Str problem = polygonProblem(outline); !problem.empty()) throw EditError(problem);
  PlanarMesh m;
  m.xy_ = points;
  m.removed_.assign(n, false);
  m.setScale();
  for (size_t i = hull; i < n; i++) {
    if (!insidePolygon(outline, points[2 * i], points[2 * i + 1], 10 * m.tol_))
      throw EditError("정점 " + std::to_string(i) + "이 윤곽 밖(또는 윤곽 위)에 있습니다.");
    for (size_t j = 0; j < i; j++)
      if (length(points[2 * i] - points[2 * j], points[2 * i + 1] - points[2 * j + 1]) <= 10 * m.tol_)
        throw EditError("정점 " + std::to_string(j) + "과 " + std::to_string(i) + "이 같은 자리입니다.");
  }
  // Either drawing direction works: the ring is walked counter-clockwise, indices stay as given.
  std::vector<int> ring;
  for (size_t i = 0; i < hull; i++) ring.push_back(int(i));
  if (polygonArea(outline, hull) < 0) std::reverse(ring.begin(), ring.end());
  for (size_t i = hull; i < n; i++) m.removed_[i] = true;  // not in the triangulation yet
  m.earClip(ring);
  for (size_t i = 0; i < hull; i++) m.fixed_.insert(edgeKey(int(i), int((i + 1) % hull)));
  m.delaunay();
  for (size_t i = hull; i < n; i++) {
    m.removed_[i] = false;
    m.insertAt(int(i), false, nullptr);
  }
  for (size_t k = 0; k + 1 < constraints.size(); k += 2) m.addConstraint(constraints[k], constraints[k + 1]);
  return m;
}
}  // namespace cutout
