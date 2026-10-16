#include "rig-canvas.hpp"
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QToolTip>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace cutout_studio {
namespace {
constexpr double PI = 3.14159265358979323846;
const QColor SETUP_COLOR("#3b82f6"), ANIMATE_COLOR("#ef4444"), SELECT_COLOR("#ffd34d"), TRAIL_COLOR("#38bdf8"), IK_COLOR("#f97316");
const QColor TRANSFORM_COLOR("#f472b6");  // transform constraints (plugin 1.16.0): pink, apart from IK orange and physics teal
const QColor PHYSICS_COLOR("#2dd4bf");  // physics bones (plugin 1.13.0): teal, apart from IK orange and trail blue
// Mesh edit (plugin 1.6.0): outline edges, pinned inside edges, outline being drawn / previewed.
const QColor OUTLINE_COLOR("#f59e0b"), PINNED_COLOR("#22d3ee"), DRAFT_COLOR("#e879f9");
// Bounding boxes and points (plugin 1.17.0): lime / violet, apart from every other overlay colour.
const QColor BOX_COLOR("#84cc16"), POINT_COLOR("#a78bfa");
constexpr double POINT_ARM = 9, POINT_HANDLE = 24;  // crosshair arm / direction line (widget px)
double normalize(double a) { while (a > PI) a -= 2 * PI; while (a < -PI) a += 2 * PI; return a; }
// Pick radii in widget px, matched to what is drawn: the IK crosshair's arms reach 12 px, so a
// press anywhere on it grabs the target (the old 7 px circle let the body image under its ring win).
constexpr double VERTEX_RADIUS = 7, IK_RADIUS = 12, BONE_RADIUS = 8, SELECTED_RADIUS = 9, AXIS_LENGTH = 18, AXIS_RADIUS = 5;
// Bone bodies (plugin 1.5.0): a press within BODY_RADIUS of the origin->tip line (or inside the
// drawn diamond) grabs the body; shorter than MIN_BODY on screen a bone is drawn as a point.
constexpr double BODY_RADIUS = 4, MIN_BODY = 6;
constexpr int HINT_PAD = 8;  // hint line text inset in its band (widget px)
/** Weight heat map (blue 0 -> red 1). */
QColor heatColor(double w, int alpha) { w = std::clamp(w, 0.0, 1.0); return QColor::fromHsvF((1 - w) * 0.66, 0.85, 0.95, alpha / 255.0); }
double segmentDistance(const QPointF& p, const QPointF& a, const QPointF& b) {
  const QPointF ab = b - a;
  const double len2 = QPointF::dotProduct(ab, ab);
  const double t = len2 > 1e-12 ? std::clamp(QPointF::dotProduct(p - a, ab) / len2, 0.0, 1.0) : 0.0;
  const QPointF d = p - (a + ab * t);
  return std::hypot(d.x(), d.y());
}
}  // namespace

RigCanvas::RigCanvas(QWidget* parent) : QWidget(parent) {
  setObjectName("cutoutCanvas");
  setMinimumSize(200, 150);  // small: the workspace splitters must fit 1280x720 windows
  setMouseTracking(true);  // hover shows what a click would pick
  setFocusPolicy(Qt::StrongFocus);
  setAttribute(Qt::WA_OpaquePaintEvent);
  setAcceptDrops(true);  // PNG/asset drops onto a part (skin images)
}

void RigCanvas::setScene(std::shared_ptr<const cutout::Rig> rig, const cutout::Pose& pose) {
  const bool changedRig = !rig_ || !rig || rig_->id != rig->id;
  rig_ = std::move(rig);
  pose_ = pose;
  if (changedRig && autoFit_) fitToCanvas();
  update();
}

void RigCanvas::setGhosts(const QVector<Ghost>& ghosts, const QSet<QString>& hiddenSlots) {
  if (hiddenSlots != ghostHidden_) layers_.clear();
  ghostHidden_ = hiddenSlots;
  ghosts_ = ghosts;
  update();
}
void RigCanvas::setTrail(const QPolygonF& points, const QPolygonF& keyPoints) {
  trail_ = points;
  trailKeys_ = keyPoints;
  update();
}

RigCanvas::GhostLayer RigCanvas::renderGhost(const Ghost& g) const {
  const QTransform view = viewTransform();
  QRectF bounds;
  for (size_t i = 0; i < g.rig->slots.size(); i++) {
    if (ghostHidden_.contains(qs(g.rig->slots[i].id))) continue;
    const QRectF b = attachmentBounds(*g.rig, *g.pose, int(i));  // a bent mesh can leave its image rectangle
    if (!b.isNull()) bounds |= view.mapRect(b);
  }
  // Only the ghost's own footprint is allocated (not the whole widget): ten ghosts a side stay small.
  const QRect r = bounds.toAlignedRect().adjusted(-2, -2, 2, 2) & rect();
  if (r.isEmpty()) return {};
  const qreal dpr = devicePixelRatioF();
  QImage img(r.size() * dpr, QImage::Format_ARGB32_Premultiplied);
  img.setDevicePixelRatio(dpr);
  img.fill(Qt::transparent);
  QPainter lp(&img);
  lp.setTransform(view * QTransform::fromTranslate(-r.x(), -r.y()));
  drawRig(lp, *g.rig, *g.pose, *images_, ghostHidden_);
  if (g.tint.isValid()) {
    // SourceAtop recolors only what the ghost covers: silhouette kept, a little shading shows through.
    lp.resetTransform();
    lp.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    QColor c = g.tint;
    c.setAlpha(190);
    lp.fillRect(QRectF(0, 0, r.width(), r.height()), c);
  }
  lp.end();
  return {r.topLeft(), img};
}

void RigCanvas::paintGhosts(QPainter& p) {
  if (ghosts_.isEmpty() || !images_ || !rig_) return;
  const QTransform view = viewTransform();
  if (view != layerView_ || size() != layerSize_ || devicePixelRatioF() != layerDpr_) {
    layers_.clear();
    layerView_ = view;
    layerSize_ = size();
    layerDpr_ = devicePixelRatioF();
  }
  // Farthest first so nearer ghosts sit on top; the current-time ghost last. All stay behind the live pose.
  QVector<int> order(ghosts_.size());
  for (int i = 0; i < order.size(); i++) order[i] = i;
  auto rank = [&](int i) { return ghosts_[i].offset == 0 ? -1 : std::abs(ghosts_[i].offset); };
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return rank(a) > rank(b); });
  QSet<quint64> used;
  p.save();
  p.resetTransform();
  for (int i : order) {
    const Ghost& g = ghosts_[i];
    if (!g.rig || !g.pose || g.pose->world.size() != g.rig->bones.size()) continue;
    const quint64 key = qHashMulti(0, g.key, g.tint.isValid() ? g.tint.rgba() : 0u);
    auto it = layers_.find(key);
    if (it == layers_.end()) {
      it = layers_.insert(key, renderGhost(g));
      ghostRenders_++;
    }
    used.insert(key);
    if (it->image.isNull()) continue;
    p.setOpacity(std::clamp(g.alpha, 0.0, 1.0));
    p.drawImage(it->origin, it->image);
  }
  p.restore();
  // Playback keeps producing new times: drop layers that are no longer shown once the cache grows.
  if (layers_.size() > 64)
    for (auto it = layers_.begin(); it != layers_.end();) it = used.contains(it.key()) ? std::next(it) : layers_.erase(it);
}

// ---- Mesh editing (plugin 1.4.0) ----
void RigCanvas::setMeshEdit(int mode, const QString& slot, const QSet<int>& selected, int bone, double brushRadius) {
  meshMode_ = mode;
  meshSlot_ = slot;
  meshSelected_ = selected;
  meshBone_ = bone;
  brushRadius_ = brushRadius;
  if (mode == MeshOff) meshGesture_ = MeshGesture::None;
  update();
}
QVector<QPointF> RigCanvas::meshVertices() const {
  if (!rig_ || meshSlot_.isEmpty() || pose_.world.size() != rig_->bones.size()) return {};
  return meshPoints(*rig_, pose_, rig_->slotIndex(ss(meshSlot_)));
}
int RigCanvas::vertexAt(const QPointF& w) const {
  const QVector<Hit> hits = hitCandidates(w);
  return !hits.isEmpty() && hits[0].vertex >= 0 ? hits[0].vertex : -1;  // vertices rank first
}
bool RigCanvas::meshPress(QMouseEvent* e) {
  if (meshMode_ == MeshOff || meshVertices().isEmpty()) return false;
  meshStart_ = meshCurrent_ = e->position();
  meshMoved_ = false;
  meshAdd_ = e->modifiers() & Qt::ShiftModifier;
  if (meshMode_ == MeshWeights) {
    meshGesture_ = MeshGesture::Paint;
    meshStroke_.clear();
    meshReach_.clear();
    strokeModifiers_ = e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier);
    hoverVertex_ = -1;
    QToolTip::hideText();
    meshMove(e->position());
    return true;
  }
  if (meshTool_ == MeshAddVertex) {
    if (onMeshAddVertex) onMeshAddVertex(widgetToRig(e->position()));
    return true;
  }
  const int v = vertexAt(e->position());
  if (meshTool_ == MeshAddEdge) {
    // Press on a vertex, release on another: one pinned edge (a press elsewhere does nothing).
    edgeFrom_ = v;
    meshGesture_ = v >= 0 ? MeshGesture::Edge : MeshGesture::None;
    update();
    return true;
  }
  if (v >= 0) {
    // Pressing a vertex outside the selection selects it first, so a click-drag moves just that one.
    if (!meshSelected_.contains(v) && onMeshSelect) onMeshSelect({v}, meshAdd_);
    meshGesture_ = MeshGesture::Vertices;
  } else meshGesture_ = MeshGesture::Band;
  update();
  return true;
}
void RigCanvas::meshMove(const QPointF& w) {
  hover_ = w;
  if (meshGesture_ == MeshGesture::None) { updateWeightHover(w); update(); return; }
  meshCurrent_ = w;
  if ((w - meshStart_).manhattanLength() > 2) meshMoved_ = true;
  if (meshGesture_ == MeshGesture::Vertices && meshMoved_ && onMeshDrag) onMeshDrag(widgetToRig(w) - widgetToRig(meshStart_), false);
  if (meshGesture_ == MeshGesture::Paint) {
    // Each vertex keeps its closest approach to the brush centre over the stroke (the falloff):
    // a stroke paints every vertex once, at most as strongly as the brush came near it.
    const QVector<QPointF> pts = meshVertices();
    bool changed = false;
    for (int i = 0; i < pts.size(); i++) {
      const QPointF o = rigToWidget(pts[i]);
      const double d = std::hypot(o.x() - w.x(), o.y() - w.y());
      if (d > brushRadius_) continue;
      const double reach = brushRadius_ > 0 ? d / brushRadius_ : 0;
      auto it = meshReach_.find(i);
      if (it == meshReach_.end()) { meshReach_.insert(i, reach); meshStroke_.insert(i); changed = true; }
      else if (reach < *it - 0.02) { *it = reach; changed = true; }  // 2 %: no preview for sub-pixel creeps
    }
    if (changed && onMeshPaint) {
      PaintStroke s;
      for (int v : meshStroke_) s.vertices << v;
      std::sort(s.vertices.begin(), s.vertices.end());
      for (int v : s.vertices) s.reach << meshReach_.value(v);
      s.modifiers = strokeModifiers_;
      onMeshPaint(s, false);
    }
  }
  update();
}
void RigCanvas::meshRelease(const QPointF& w) {
  const MeshGesture g = meshGesture_;
  meshGesture_ = MeshGesture::None;
  meshCurrent_ = w;
  if (g == MeshGesture::Vertices) {
    if (meshMoved_ && onMeshDrag) onMeshDrag(widgetToRig(w) - widgetToRig(meshStart_), true);
  } else if (g == MeshGesture::Edge) {
    const int to = vertexAt(w), from = edgeFrom_;
    edgeFrom_ = -1;
    if (to >= 0 && from >= 0 && to != from && onMeshAddEdge) onMeshAddEdge(from, to);
  } else if (g == MeshGesture::Band && onMeshSelect) {
    QVector<int> inside;
    if (meshMoved_) {
      const QRectF band = QRectF(meshStart_, w).normalized();
      const QVector<QPointF> pts = meshVertices();
      for (int i = 0; i < pts.size(); i++) if (band.contains(rigToWidget(pts[i]))) inside << i;
    }
    onMeshSelect(inside, meshAdd_ && meshMoved_);
  } else if (g == MeshGesture::Paint) {
    PaintStroke s;
    for (int v : meshStroke_) s.vertices << v;
    std::sort(s.vertices.begin(), s.vertices.end());
    for (int v : s.vertices) s.reach << meshReach_.value(v);
    s.modifiers = strokeModifiers_;
    meshStroke_.clear();
    meshReach_.clear();
    if (!s.vertices.isEmpty() && onMeshPaint) onMeshPaint(s, true);
  }
  update();
}
void RigCanvas::setOutlineDrawing(const QString& slot) {
  outlineSlot_ = slot;
  outline_.clear();
  if (!slot.isEmpty()) { hoverRef_.clear(); unsetCursor(); setCursor(Qt::CrossCursor); }
  else unsetCursor();
  update();
}
bool RigCanvas::outlinePress(QMouseEvent* e) {
  if (!outlineDrawing()) return false;
  const QPointF w = e->position();
  // A click on the first point closes the outline (Spine-like); a second click on the last point is
  // the first half of a double-click, not a new point.
  if (outline_.size() >= 3 && QLineF(rigToWidget(outline_[0]), w).length() <= VERTEX_RADIUS * ui_) { finishOutline(); return true; }
  if (!outline_.isEmpty() && QLineF(rigToWidget(outline_.back()), w).length() <= 2) return true;
  outline_ << widgetToRig(w);
  update();
  return true;
}
void RigCanvas::finishOutline() {
  if (outline_.size() < 3) return;  // keeps drawing: a closed outline needs three points
  const QPolygonF points = outline_;
  outline_.clear();
  update();
  if (onMeshOutline) onMeshOutline(points);
}
// The outline being drawn (points, the edge to the cursor, the closing edge) and the 자동 윤곽 preview.
void RigCanvas::paintOutline(QPainter& p) {
  if (!outlinePreview_.isEmpty()) {
    QPolygonF poly;
    for (const QPointF& q : outlinePreview_) poly << rigToWidget(q);
    p.setBrush(QColor(232, 121, 249, 40));
    p.setPen(QPen(DRAFT_COLOR, 1.5, Qt::DashLine));
    p.drawPolygon(poly);
    p.setBrush(DRAFT_COLOR);
    p.setPen(Qt::NoPen);
    for (const QPointF& q : poly) p.drawEllipse(q, 2.5 * ui_, 2.5 * ui_);
    p.setBrush(Qt::NoBrush);
  }
  if (!outlineDrawing()) return;
  QPolygonF line;
  for (const QPointF& q : outline_) line << rigToWidget(q);
  p.setPen(QPen(DRAFT_COLOR, 2));
  p.drawPolyline(line);
  if (!line.isEmpty() && rect().contains(hover_.toPoint())) {
    p.setPen(QPen(DRAFT_COLOR, 1.5, Qt::DashLine));
    p.drawLine(line.back(), hover_);
    if (line.size() >= 2) p.drawLine(hover_, line.front());
  }
  p.setPen(QPen(QColor("#0b1220"), 1));
  for (int i = 0; i < line.size(); i++) {
    p.setBrush(i == 0 ? SELECT_COLOR : DRAFT_COLOR);
    p.drawEllipse(line[i], (i == 0 ? 5.0 : 3.5) * ui_, (i == 0 ? 5.0 : 3.5) * ui_);
  }
  p.setBrush(Qt::NoBrush);
}
// Wireframe, weight heat map (blue 0 -> red 1 for the chosen bone), vertex handles, band and brush.
void RigCanvas::paintMesh(QPainter& p) {
  if (meshMode_ == MeshOff || !rig_) return;
  const int si = rig_->slotIndex(ss(meshSlot_));
  const cutout::Attachment* att = nullptr;
  const QVector<QPointF> pts = si >= 0 ? meshPoints(*rig_, pose_, si, &att) : QVector<QPointF>();
  if (!att || pts.isEmpty()) return;
  auto heat = heatColor;
  QVector<double> weight(pts.size(), 0.0);
  if (meshMode_ == MeshWeights && meshBone_ >= 0)
    for (int i = 0; i < pts.size(); i++) weight[i] = vertexWeight(*rig_, si, *att, size_t(i), meshBone_);
  // Dense meshes (plugin 1.18.0: up to 2048 vertices painted live): widget points once, the heat
  // fill without antialiasing (adjacent triangles would show seams anyway), the wireframe as one
  // batch of unique edges.
  QVector<QPointF> wp(pts.size());
  for (int i = 0; i < pts.size(); i++) wp[i] = rigToWidget(pts[i]);
  QVector<QLineF> wire;
  QSet<quint64> seen;
  const bool aa = p.testRenderHint(QPainter::Antialiasing);
  if (meshMode_ == MeshWeights) { p.setRenderHint(QPainter::Antialiasing, false); p.setPen(Qt::NoPen); }
  for (size_t t = 0; t + 2 < att->triangles.size(); t += 3) {
    const int a = att->triangles[t], b = att->triangles[t + 1], c = att->triangles[t + 2];
    if (a < 0 || b < 0 || c < 0 || a >= pts.size() || b >= pts.size() || c >= pts.size()) continue;
    if (meshMode_ == MeshWeights) {
      p.setBrush(heat((weight[a] + weight[b] + weight[c]) / 3, 120));
      const QPointF tri[3] = {wp[a], wp[b], wp[c]};
      p.drawPolygon(tri, 3);
    }
    for (const auto& [u, v] : {std::pair<int, int>{a, b}, {b, c}, {c, a}})
      if (const quint64 k = (quint64(std::min(u, v)) << 32) | quint64(std::max(u, v)); !seen.contains(k)) { seen.insert(k); wire << QLineF(wp[u], wp[v]); }
  }
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(QColor(255, 255, 255, meshMode_ == MeshVertices ? 90 : 120), 1));
  p.drawLines(wire);
  p.setRenderHint(QPainter::Antialiasing, aa);
  if (meshMode_ == MeshVertices && att->boundingBox) {
    // A bounding box (plugin 1.17.0) has no triangles: its closed polygon is the outline.
    p.setPen(QPen(OUTLINE_COLOR, 2));
    p.setBrush(Qt::NoBrush);
    QPolygonF poly;
    for (const QPointF& q : pts) poly << rigToWidget(q);
    p.drawPolygon(poly);
  }
  if (meshMode_ == MeshVertices) {
    // Outline (edges of one triangle) in orange, the author's pinned inside edges in cyan: the
    // two kinds re-triangulation keeps.
    QSet<QPair<int, int>> outline;
    p.setPen(QPen(OUTLINE_COLOR, 2));
    for (const auto& e : meshOutline(att->triangles)) {
      outline.insert(e);
      if (e.first < pts.size() && e.second < pts.size()) p.drawLine(rigToWidget(pts[e.first]), rigToWidget(pts[e.second]));
    }
    p.setPen(QPen(PINNED_COLOR, 2));
    for (size_t k = 0; k + 1 < att->edges.size(); k += 2) {
      const int a = std::min(att->edges[k], att->edges[k + 1]), b = std::max(att->edges[k], att->edges[k + 1]);
      if (a != b && !outline.contains({a, b}) && b < pts.size()) p.drawLine(rigToWidget(pts[a]), rigToWidget(pts[b]));
    }
    if (meshGesture_ == MeshGesture::Edge && edgeFrom_ >= 0 && edgeFrom_ < pts.size()) {
      p.setPen(QPen(PINNED_COLOR, 1.5, Qt::DashLine));
      p.drawLine(rigToWidget(pts[edgeFrom_]), meshCurrent_);
    }
  }
  if (meshMode_ == MeshWeights) {
    // Heat dots as cached sprites (32 heat steps x normal / in-stroke size): blits, not 2000 paths.
    const qreal dpr = devicePixelRatioF();
    if (dotScale_ != ui_ * dpr) { dotSprites_.clear(); dotScale_ = ui_ * dpr; }
    for (int i = 0; i < pts.size(); i++) {
      const int big = meshGesture_ == MeshGesture::Paint && meshStroke_.contains(i) ? 1 : 0, step = int(std::lround(std::clamp(weight[i], 0.0, 1.0) * 31));
      const int key = step * 2 + big;
      auto it = dotSprites_.find(key);
      if (it == dotSprites_.end()) {
        const double r = (big ? 4.5 : 3.0) * ui_;
        const int side = int(std::ceil(2 * r + 3));
        QImage img(QSize(side, side) * dpr, QImage::Format_ARGB32_Premultiplied);
        img.setDevicePixelRatio(dpr);
        img.fill(Qt::transparent);
        QPainter q(&img);
        q.setRenderHint(QPainter::Antialiasing);
        q.setPen(QPen(QColor("#0b1220"), 1));
        q.setBrush(heat(step / 31.0, 255));
        q.drawEllipse(QPointF(side / 2.0, side / 2.0), r, r);
        q.end();
        it = dotSprites_.insert(key, img);
      }
      const double half = it->width() / dpr / 2.0;
      p.drawImage(wp[i] - QPointF(half, half), *it);
    }
  } else
    for (int i = 0; i < pts.size(); i++) {
      const bool picked = meshSelected_.contains(i);
      p.setPen(QPen(size_t(i) < att->hull ? OUTLINE_COLOR.darker(160) : QColor("#0b1220"), 1));
      p.setBrush(picked ? SELECT_COLOR : QColor("#f8fafc"));
      p.drawEllipse(wp[i], (picked ? 4.5 : 3.0) * ui_, (picked ? 4.5 : 3.0) * ui_);
    }
  p.setBrush(Qt::NoBrush);
  if (meshGesture_ == MeshGesture::Band && meshMoved_) {
    p.setPen(QPen(SELECT_COLOR, 1, Qt::DashLine));
    p.drawRect(QRectF(meshStart_, meshCurrent_).normalized());
  }
  if (meshMode_ == MeshWeights) paintWeightOverlay(p, wp, weight);
}
// ---- Weight painting view (plugin 1.18.0): influence counts, brush, ramp legend, value under the cursor.
double RigCanvas::falloffAt(int falloff, double reach) {
  const double r = std::clamp(reach, 0.0, 1.0);
  if (falloff == FalloffHard) return 1;
  if (falloff == FalloffLinear) return 1 - r;
  return 1 - r * r * (3 - 2 * r);  // smoothstep: full near the centre, soft edge
}
int RigCanvas::influenceCount(int v) const {
  if (!rig_ || meshSlot_.isEmpty() || pose_.world.size() != rig_->bones.size()) return 0;
  const cutout::Attachment* att = nullptr;
  attachmentMatrix(*rig_, pose_, rig_->slotIndex(ss(meshSlot_)), &att);
  if (!att || !att->mesh || v < 0 || size_t(v) >= att->vertexCount()) return 0;
  return att->weights.empty() || size_t(v) >= att->weights.size() ? 1 : int(att->weights[size_t(v)].size());
}
QString RigCanvas::vertexWeightsText(int v) const {
  if (!rig_ || meshSlot_.isEmpty() || pose_.world.size() != rig_->bones.size()) return {};
  const int si = rig_->slotIndex(ss(meshSlot_));
  const cutout::Attachment* att = nullptr;
  attachmentMatrix(*rig_, pose_, si, &att);
  if (!att || !att->mesh || v < 0 || size_t(v) >= att->vertexCount()) return {};
  QVector<QPair<double, int>> shares;
  if (att->weights.empty() || size_t(v) >= att->weights.size()) shares << qMakePair(1.0, rig_->slots[size_t(si)].boneIndex);
  else for (const auto& w : att->weights[size_t(v)]) shares << qMakePair(w.weight, att->binds[size_t(w.bind)].first);
  std::stable_sort(shares.begin(), shares.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
  QStringList parts;
  for (const auto& [w, b] : shares)
    if (b >= 0) parts << QString("%1 %2%3").arg(qs(rig_->bones[size_t(b)].id)).arg(w, 0, 'f', 3).arg(weightView_.locked.contains(b) ? " (잠금)" : "");
  return QString("정점 %1 · ").arg(v) + parts.join(" · ");
}
double RigCanvas::weightAt(const QPointF& w) const {
  if (!rig_ || meshSlot_.isEmpty() || meshBone_ < 0) return -1;
  const int si = rig_->slotIndex(ss(meshSlot_));
  const cutout::Attachment* att = nullptr;
  const QVector<QPointF> pts = si >= 0 ? meshPoints(*rig_, pose_, si, &att) : QVector<QPointF>();
  if (!att || !att->mesh) return -1;
  for (size_t t = 0; t + 2 < att->triangles.size(); t += 3) {
    const int i[3] = {att->triangles[t], att->triangles[t + 1], att->triangles[t + 2]};
    if (i[0] >= pts.size() || i[1] >= pts.size() || i[2] >= pts.size()) continue;
    const QPointF a = rigToWidget(pts[i[0]]), b = rigToWidget(pts[i[1]]), c = rigToWidget(pts[i[2]]);
    const double det = (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
    if (std::fabs(det) < 1e-9) continue;
    const double l0 = ((b.x() - w.x()) * (c.y() - w.y()) - (b.y() - w.y()) * (c.x() - w.x())) / det;
    const double l1 = ((c.x() - w.x()) * (a.y() - w.y()) - (c.y() - w.y()) * (a.x() - w.x())) / det, l2 = 1 - l0 - l1;
    if (l0 < -1e-9 || l1 < -1e-9 || l2 < -1e-9) continue;
    return l0 * vertexWeight(*rig_, si, *att, size_t(i[0]), meshBone_) + l1 * vertexWeight(*rig_, si, *att, size_t(i[1]), meshBone_) + l2 * vertexWeight(*rig_, si, *att, size_t(i[2]), meshBone_);
  }
  return -1;
}
void RigCanvas::updateWeightHover(const QPointF& w) {
  int best = -1;
  if (meshMode_ == MeshWeights) {
    double bestD = VERTEX_RADIUS * ui_;
    const QVector<QPointF> pts = meshVertices();
    for (int i = 0; i < pts.size(); i++) {
      const QPointF o = rigToWidget(pts[i]);
      if (const double d = std::hypot(o.x() - w.x(), o.y() - w.y()); d <= bestD) { bestD = d; best = i; }
    }
  }
  if (best == hoverVertex_) return;
  hoverVertex_ = best;
  if (best >= 0 && isVisible()) QToolTip::showText(mapToGlobal(w.toPoint()), vertexWeightsText(best), this);
  else QToolTip::hideText();
}
void RigCanvas::paintWeightOverlay(QPainter& p, const QVector<QPointF>& pts, const QVector<double>& weight) {
  // Influence counts beside the dots (2+ bones), legible while the mesh is not too dense.
  if (pts.size() <= 400) {
    QFont f = p.font();
    f.setPixelSize(std::max(8, int(9 * ui_)));
    p.setFont(f);
    for (int i = 0; i < pts.size(); i++) {
      const int n = influenceCount(i);
      if (n < 2) continue;
      p.setPen(QColor("#0b1220"));
      p.drawText(pts[i] + QPointF(4 * ui_ + 1, -3 * ui_ + 1), QString::number(n));
      p.setPen(QColor("#f8fafc"));
      p.drawText(pts[i] + QPointF(4 * ui_, -3 * ui_), QString::number(n));
    }
  }
  Q_UNUSED(weight);
  // Colour ramp legend of the heat-map bone (bottom-left, above the hint line).
  const int hintHeight = fontMetrics().height() + 4;
  const QRectF ramp(12, height() - hintHeight - 26 * ui_, 120 * ui_, 8 * ui_);
  QLinearGradient g(ramp.topLeft(), ramp.topRight());
  for (double t : {0.0, 0.25, 0.5, 0.75, 1.0}) g.setColorAt(t, heatColor(t, 255));
  p.setPen(QPen(QColor("#0b1220"), 1));
  p.setBrush(g);
  p.drawRect(ramp);
  p.setBrush(Qt::NoBrush);
  p.setPen(QColor("#e2e8f0"));
  const QString bone = rig_ && meshBone_ >= 0 && size_t(meshBone_) < rig_->bones.size() ? qs(rig_->bones[size_t(meshBone_)].id) : QString("본 없음");
  p.drawText(QPointF(ramp.left(), ramp.top() - 3), QString("0 → 1 · %1").arg(bone));
  if (!rect().contains(hover_.toPoint())) return;
  // Brush: the outer ring is its reach, the inner one where the falloff starts to fade; the mode
  // the stroke would use (Shift = smooth, Ctrl = subtract) beside it.
  int mode = weightView_.mode;
  const Qt::KeyboardModifiers mods = meshGesture_ == MeshGesture::Paint ? strokeModifiers_ : QGuiApplication::keyboardModifiers();
  if (mods & Qt::ShiftModifier) mode = WeightSmooth;
  else if (mods & Qt::ControlModifier) mode = WeightSubtract;
  p.setPen(QPen(QColor(255, 255, 255, 200), 1.5, Qt::DashLine));
  p.drawEllipse(hover_, brushRadius_, brushRadius_);
  if (weightView_.falloff != FalloffHard) {
    p.setPen(QPen(QColor(255, 255, 255, 110), 1, Qt::DotLine));
    p.drawEllipse(hover_, brushRadius_ * 0.5, brushRadius_ * 0.5);
  }
  static const char* glyphs[] = {"+", "−", "=", "~"};
  p.setPen(QColor("#f8fafc"));
  QString label = glyphs[std::clamp(mode, 0, 3)];
  if (weightView_.showValue) if (const double v = weightAt(hover_); v >= 0) label += QString(" %1").arg(v, 0, 'f', 2);
  p.drawText(hover_ + QPointF(brushRadius_ * 0.72 + 4, -brushRadius_ * 0.72), label);
}

QColor RigCanvas::ikColor() { return IK_COLOR; }
QColor RigCanvas::boxColor() { return BOX_COLOR; }
QColor RigCanvas::pointColor() { return POINT_COLOR; }
QPointF RigCanvas::pointHandle(const QPointF& at, double rotation) const {
  const double r = rotation * PI / 180;
  return rigToWidget(at) + QPointF(std::cos(r), std::sin(r)) * POINT_HANDLE * ui_;
}
void RigCanvas::setPointPlacing(bool on) {
  placingPoint_ = on;
  if (on) { hoverRef_.clear(); setCursor(Qt::CrossCursor); } else unsetCursor();
  update();
}
// Bounding boxes (plugin 1.17.0): translucent fill + dashed outline in the box's colour; the
// selected one in the selection colour. Editor-hidden slots (eye toggle) are not drawn.
void RigCanvas::paintBoxes(QPainter& p) {
  if (!showBoxes_ || !rig_ || pose_.world.size() != rig_->bones.size()) return;
  for (size_t i = 0; i < rig_->slots.size(); i++) {
    if (hidden_.contains(qs(rig_->slots[i].id))) continue;
    const QPolygonF polygon = boxPolygon(*rig_, pose_, int(i));
    if (polygon.size() < 3) continue;
    const cutout::Attachment* att = nullptr;
    attachmentMatrix(*rig_, pose_, int(i), &att);
    const bool selected = isSelected("slot:" + qs(rig_->slots[i].id));
    QColor c = dataColor(att ? att->color : std::string(), BOX_COLOR), fill = c;
    fill.setAlpha(selected ? 70 : 40);
    p.setBrush(fill);
    p.setPen(QPen(selected ? SELECT_COLOR : c, (selected ? 2.25 : 1.5) * ui_, Qt::DashLine));
    p.drawPolygon(viewTransform().map(polygon));
  }
  p.setBrush(Qt::NoBrush);
}
// Points (plugin 1.17.0): a crosshair with a circle, and the direction as a line to a round handle.
void RigCanvas::paintPoints(QPainter& p) {
  if (!showPoints_ || !rig_ || pose_.world.size() != rig_->bones.size()) return;
  for (size_t i = 0; i < rig_->slots.size(); i++) {
    if (hidden_.contains(qs(rig_->slots[i].id))) continue;
    QPointF at;
    double rotation = 0;
    if (!pointOf(*rig_, pose_, int(i), at, rotation)) continue;
    const bool selected = isSelected("slot:" + qs(rig_->slots[i].id));
    const QColor c = selected ? SELECT_COLOR : POINT_COLOR;
    const QPointF w = rigToWidget(at), handle = pointHandle(at, rotation);
    const double arm = POINT_ARM * ui_;
    p.setPen(QPen(QColor(11, 18, 32, 200), 3.5 * ui_));
    p.drawLine(w - QPointF(arm, 0), w + QPointF(arm, 0));
    p.drawLine(w - QPointF(0, arm), w + QPointF(0, arm));
    p.setPen(QPen(c, (selected ? 2.25 : 1.5) * ui_));
    p.drawLine(w - QPointF(arm, 0), w + QPointF(arm, 0));
    p.drawLine(w - QPointF(0, arm), w + QPointF(0, arm));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(w, 4.5 * ui_, 4.5 * ui_);
    p.drawLine(w, handle);
    p.setBrush(c);
    p.drawEllipse(handle, 3.5 * ui_, 3.5 * ui_);
    p.setBrush(Qt::NoBrush);
  }
}
QColor RigCanvas::physicsColor() { return PHYSICS_COLOR; }
QColor RigCanvas::transformColor() { return TRANSFORM_COLOR; }
bool RigCanvas::isIkTarget(int bone) const {
  if (!rig_) return false;
  for (const auto& k : rig_->ik) if (k.target == bone) return true;
  return false;
}
// IK overlay: a dashed line from each chain's effector to its target (where the limb is pulled),
// the selected constraint's chain links drawn thick, so solved bones read as "driven", and on
// aim chains the aim axis as an arrow (which part of the bone faces the target).
void RigCanvas::paintIk(QPainter& p) {
  if (!rig_ || rig_->ik.empty() || pose_.world.size() != rig_->bones.size()) return;
  auto origin = [&](int b) { return rigToWidget(QPointF(pose_.world[size_t(b)].e, pose_.world[size_t(b)].f)); };
  for (const auto& k : rig_->ik) {
    if (k.target < 0 || k.bones.empty() || k.bones[0] < 0) continue;
    const bool selected = qs(k.id) == selectedIk_;
    QColor line = IK_COLOR;
    line.setAlpha(selected ? 255 : 150);
    if (selected) {
      p.setPen(QPen(line, 4, Qt::SolidLine, Qt::RoundCap));
      QPolygonF chain;
      for (int b : k.bones) chain << origin(b);
      if (k.tip >= 0) chain << origin(k.tip);
      else if (k.tipLength > 0) chain << rigToWidget(boneTip(k.bones.back()));
      p.drawPolyline(chain);
    }
    double dx = 0, dy = 0;
    if (k.bones.size() == 1 && cutout::ikAimDirection(*rig_, pose_, int(&k - rig_->ik.data()), dx, dy)) {
      // Screen length: the target's distance, kept readable (16..60 px).
      const QPointF from = origin(k.bones[0]), aimAt = origin(k.target);
      const double len = std::clamp(std::hypot(aimAt.x() - from.x(), aimAt.y() - from.y()), 16.0 * ui_, 60.0 * ui_);
      const QPointF dir(dx, dy), side(-dy, dx), to = from + dir * len;
      p.setPen(QPen(line, selected ? 3 : 2, Qt::SolidLine, Qt::RoundCap));
      p.drawLine(from, to);
      p.drawLine(to, to - dir * 8 * ui_ + side * 4 * ui_);
      p.drawLine(to, to - dir * 8 * ui_ - side * 4 * ui_);
    }
    // From the effector: the tip bone, the last bone's length tip, else the chain end.
    const QPointF from = k.tip >= 0 ? origin(k.tip) : k.bones.size() >= 2 && k.tipLength > 0 ? rigToWidget(boneTip(k.bones.back())) : origin(k.bones.back());
    p.setPen(QPen(line, 1.5, Qt::DashLine));
    p.drawLine(from, origin(k.target));
  }
}

// Transform constraint overlay (plugin 1.16.0): from each constrained bone's origin a dashed pink
// line to the target's origin, which gets a diamond; the selected constraint is drawn thicker.
void RigCanvas::paintTransforms(QPainter& p) {
  if (!rig_ || rig_->transforms.empty() || pose_.world.size() != rig_->bones.size()) return;
  auto origin = [&](int b) { return rigToWidget(QPointF(pose_.world[size_t(b)].e, pose_.world[size_t(b)].f)); };
  for (const auto& k : rig_->transforms) {
    if (k.target < 0) continue;
    const bool selected = qs(k.id) == selectedTransform_;
    QColor c = TRANSFORM_COLOR;
    c.setAlpha(selected ? 255 : 160);
    const QPointF to = origin(k.target);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(c, (selected ? 2.25 : 1.25) * ui_, Qt::DashLine));
    for (int b : k.bones) if (b >= 0) p.drawLine(origin(b), to);
    const double r = (selected ? 7 : 5) * ui_;
    p.setPen(QPen(c, (selected ? 2.5 : 1.5) * ui_));
    p.drawPolygon(QPolygonF({to + QPointF(0, -r), to + QPointF(r, 0), to + QPointF(0, r), to + QPointF(-r, 0)}));
  }
}

// Physics overlay (plugin 1.13.0): a teal spring ring (a "~" in a circle) at the tip of every
// physics bone, the selected constraint's drawn thicker; while preview playback simulates, a dashed
// tether runs from where the animation alone would put the tip (the rest point) to where it swings.
void RigCanvas::paintPhysics(QPainter& p) {
  if (!rig_ || rig_->physics.empty() || pose_.world.size() != rig_->bones.size()) return;
  for (const auto& k : rig_->physics) {
    if (k.bone < 0 || size_t(k.bone) >= rig_->bones.size()) continue;
    const bool selected = qs(k.id) == selectedPhysics_;
    QColor c = PHYSICS_COLOR;
    c.setAlpha(selected ? 255 : 170);
    const auto& w = pose_.world[size_t(k.bone)];
    const QPointF tip = rigToWidget(rig_->bones[size_t(k.bone)].length > 0 ? boneTip(k.bone) : QPointF(w.e, w.f));
    if (auto rest = physicsRest_.find(k.bone); rest != physicsRest_.end()) {
      const QPointF from = rigToWidget(rest.value());
      p.setPen(QPen(c, 1.25 * ui_, Qt::DashLine));
      p.setBrush(Qt::NoBrush);
      p.drawLine(from, tip);
      p.drawEllipse(from, 2.5 * ui_, 2.5 * ui_);
    }
    const double r = (selected ? 7 : 5) * ui_;
    p.setPen(QPen(c, (selected ? 2.5 : 1.5) * ui_));
    p.setBrush(QColor(11, 18, 32, 140));
    p.drawEllipse(tip, r, r);
    QPainterPath wave;
    wave.moveTo(tip + QPointF(-r * 0.6, 0));
    wave.cubicTo(tip + QPointF(-r * 0.3, -r * 0.55), tip + QPointF(0, -r * 0.1), tip);
    wave.cubicTo(tip + QPointF(0, r * 0.1), tip + QPointF(r * 0.3, r * 0.55), tip + QPointF(r * 0.6, 0));
    p.setBrush(Qt::NoBrush);
    p.drawPath(wave);
  }
}

void RigCanvas::fitToCanvas() {
  autoFit_ = true;
  const double rw = rig_ && rig_->width > 0 ? rig_->width : 512, rh = rig_ && rig_->height > 0 ? rig_->height : 512;
  zoom_ = std::max(0.02, std::min((width() - 40) / rw, (height() - 40) / rh));
  pan_ = QPointF(width() / 2.0 - rw * zoom_ / 2, height() / 2.0 - rh * zoom_ / 2);
  update();
}
void RigCanvas::frameSelection() {
  if (!rig_ || pose_.world.size() != rig_->bones.size()) { fitToCanvas(); return; }
  double x0 = 1e18, y0 = 1e18, x1 = -1e18, y1 = -1e18;
  auto add = [&](const QPointF& q) { x0 = std::min(x0, q.x()); y0 = std::min(y0, q.y()); x1 = std::max(x1, q.x()); y1 = std::max(y1, q.y()); };
  for (size_t i = 0; i < rig_->bones.size(); i++)
    if (isSelected("bone:" + qs(rig_->bones[i].id))) { add(QPointF(pose_.world[i].e, pose_.world[i].f)); add(boneTip(int(i))); }
  for (size_t i = 0; i < rig_->slots.size(); i++) {
    if (!isSelected("slot:" + qs(rig_->slots[i].id))) continue;
    const QRectF b = attachmentBounds(*rig_, pose_, int(i));
    if (!b.isNull()) { add(b.topLeft()); add(b.bottomRight()); }
    else if (const int bi = rig_->slots[i].boneIndex; bi >= 0) add(QPointF(pose_.world[size_t(bi)].e, pose_.world[size_t(bi)].f));  // nothing drawn: its bone
  }
  if (x0 > x1) { fitToCanvas(); return; }
  // A point bone alone would zoom without end: at least 80 rig px are framed, zoom stays <= 8.
  const QPointF c((x0 + x1) / 2, (y0 + y1) / 2);
  const double rw = std::max(80.0, x1 - x0), rh = std::max(80.0, y1 - y0), margin = 48 * ui_;
  zoom_ = std::clamp(std::min((width() - 2 * margin) / rw, (height() - 2 * margin) / rh), 0.02, 8.0);
  pan_ = QPointF(width() / 2.0, height() / 2.0) - c * zoom_;
  autoFit_ = false;
  refreshHover();
  update();
}
QTransform RigCanvas::viewTransform() const {
  QTransform t;
  t.translate(pan_.x(), pan_.y());
  t.scale(zoom_, zoom_);
  return t;
}
QPointF RigCanvas::widgetToRig(const QPointF& p) const { return QPointF((p.x() - pan_.x()) / zoom_, (p.y() - pan_.y()) / zoom_); }
QPointF RigCanvas::rigToWidget(const QPointF& p) const { return QPointF(p.x() * zoom_ + pan_.x(), p.y() * zoom_ + pan_.y()); }

QVector<RigCanvas::Hit> RigCanvas::hitCandidates(const QPointF& w, bool handles) const {
  QVector<Hit> out;
  if (!rig_ || pose_.world.size() != rig_->bones.size()) return out;
  auto dist = [&](const QPointF& o) { return std::hypot(o.x() - w.x(), o.y() - w.y()); };
  if (handles && meshMode_ == MeshVertices) {
    const QVector<QPointF> pts = meshVertices();
    for (int i = 0; i < pts.size(); i++)
      if (const double d = dist(rigToWidget(pts[i])); d <= VERTEX_RADIUS * ui_) out.push_back({"slot:" + meshSlot_, i, HitVertex, d});
  }
  // Bones back to front so equally near handles keep the later (drawn on top) bone first. Bones
  // the 본 표시 mode does not draw are never picked (plugin 1.20.0).
  const QVector<bool> shown = handles ? shownBones() : QVector<bool>();
  for (size_t i = handles ? rig_->bones.size() : 0; i-- > 0;) {
    if (!shown[int(i)]) continue;
    const QString ref = "bone:" + qs(rig_->bones[i].id);
    const auto& m = pose_.world[i];
    const QPointF at = rigToWidget(QPointF(m.e, m.f));
    const double d = dist(at);
    if (isIkTarget(int(i))) {
      if (d <= IK_RADIUS * ui_) out.push_back({ref, -1, HitIkTarget, d});
      continue;  // only its origin matters: no body
    }
    const QPolygonF shape = boneShape(int(i));
    if (ref == selection_) {
      // The selected bone's own gizmo: its origin and (point bones) its drawn x axis grab it;
      // the axis is its "body": dragging it turns the bone.
      const double len = std::hypot(m.a, m.b);
      const QPointF axis = len > 1e-9 ? QPointF(m.a / len, m.b / len) * AXIS_LENGTH * ui_ : QPointF(AXIS_LENGTH * ui_, 0);
      const double onAxis = shape.isEmpty() ? segmentDistance(w, at, at + axis) : 1e9;
      if (d <= SELECTED_RADIUS * ui_ || onAxis <= AXIS_RADIUS * ui_) out.push_back({ref, -1, HitSelectedBone, std::min(d, onAxis), d > SELECTED_RADIUS * ui_});
    } else if (d <= BONE_RADIUS * ui_) {
      out.push_back({ref, -1, HitBone, d});
    }
    // Bone bodies rank after every handle and before images (Spine: the bone over its image).
    if (!shape.isEmpty()) {
      const double line = segmentDistance(w, at, rigToWidget(boneTip(int(i))));
      if (line <= BODY_RADIUS * ui_ || shape.containsPoint(w, Qt::OddEvenFill)) out.push_back({ref, -1, HitBoneBody, line, true});
    }
  }
  // Points and box outlines (plugin 1.17.0): handles like bones, only while shown. A point's centre
  // grabs it (move), its direction handle turns it (body); a box is grabbed by its outline, so the
  // pictures inside stay clickable.
  for (size_t i = handles ? rig_->slots.size() : 0; i-- > 0;) {
    const QString ref = "slot:" + qs(rig_->slots[i].id);
    if (hidden_.contains(qs(rig_->slots[i].id))) continue;
    QPointF at;
    double rotation = 0;
    if (showPoints_ && pointOf(*rig_, pose_, int(i), at, rotation)) {
      if (const double d = dist(rigToWidget(at)); d <= BONE_RADIUS * ui_) out.push_back({ref, -1, HitBone, d});
      if (const double d = dist(pointHandle(at, rotation)); d <= (AXIS_RADIUS + 1) * ui_) out.push_back({ref, -1, HitBone, d, true});
      continue;
    }
    if (!showBoxes_) continue;
    const QPolygonF polygon = boxPolygon(*rig_, pose_, int(i));
    double edge = 1e9;
    for (int k = 0; k < polygon.size(); k++) edge = std::min(edge, segmentDistance(w, rigToWidget(polygon[k]), rigToWidget(polygon[(k + 1) % polygon.size()])));
    if (polygon.size() >= 3 && edge <= boxOutlineRadius()) out.push_back({ref, -1, HitBoneBody, edge});
  }
  const QTransform view = viewTransform();
  int depth = 0;
  for (size_t i = rig_->slots.size(); i-- > 0;) {
    if (hidden_.contains(qs(rig_->slots[i].id))) continue;  // not drawn = never picked
    const cutout::Attachment* att = nullptr;
    const auto m = attachmentMatrix(*rig_, pose_, int(i), &att);
    if (!att) continue;
    bool inside = false;
    if (att->mesh) {
      // Meshes catch clicks inside their (deformed) triangles.
      const QVector<QPointF> pts = meshPoints(*rig_, pose_, int(i));
      for (size_t t = 0; !inside && t + 2 < att->triangles.size(); t += 3) {
        const int a = att->triangles[t], b = att->triangles[t + 1], c = att->triangles[t + 2];
        if (a < pts.size() && b < pts.size() && c < pts.size())
          inside = QPolygonF({rigToWidget(pts[a]), rigToWidget(pts[b]), rigToWidget(pts[c])}).containsPoint(w, Qt::OddEvenFill);
      }
    } else {
      bool invertible = false;
      const QTransform inv = (qtTransform(m) * view).inverted(&invertible);
      if (!invertible) continue;
      const QPointF local = inv.map(w);
      inside = local.x() >= 0 && local.y() >= 0 && local.x() < att->width && local.y() < att->height;
      // Transparent pixels do not catch clicks, so overlapping parts pick the visible one.
      if (inside && images_) {
        const QImage img = const_cast<ImageCache*>(images_)->image(qs(att->imageAssetId));
        if (!img.isNull()) {
          const int px = int(local.x() * img.width() / att->width), py = int(local.y() * img.height() / att->height);
          if (img.valid(px, py) && qAlpha(img.pixel(px, py)) < 16) inside = false;
        }
      }
    }
    if (inside) out.push_back({"slot:" + qs(rig_->slots[i].id), -1, HitPart, double(depth++)});
  }
  // Locked items (plugin 1.21.0) are never picked; the vertices of a mesh being edited still are.
  if (!locked_.isEmpty()) out.erase(std::remove_if(out.begin(), out.end(), [this](const Hit& h) { return h.vertex < 0 && locked_.contains(h.ref); }), out.end());
  std::stable_sort(out.begin(), out.end(), [](const Hit& a, const Hit& b) { return a.rank != b.rank ? a.rank < b.rank : a.distance < b.distance; });
  return out;
}
double RigCanvas::boxOutlineRadius() const { return (BODY_RADIUS + 1) * ui_; }
QString RigCanvas::hitTest(const QPointF& w, bool bones) const {
  for (const Hit& h : hitCandidates(w, bones)) if (h.vertex < 0) return h.ref;
  return {};
}
RigCanvas::Hit RigCanvas::pickHit(const QPointF& w, bool cycle) const {
  QStringList refs;  // distinct items in candidate order (a mesh vertex is not an item)
  QVector<Hit> first;  // each item's best candidate: a bone's origin before its body
  for (const Hit& h : hitCandidates(w)) if (h.vertex < 0 && !refs.contains(h.ref)) { refs << h.ref; first << h; }
  if (refs.isEmpty()) return {};
  if (!cycle) return first[0];
  // From the selection (or the default pick when it is not under the cursor) to the next one:
  // the first Alt+click already reaches what lies beneath, repeated ones wrap around the stack.
  return first[(std::max(0, int(refs.indexOf(selection_))) + 1) % refs.size()];
}
QPointF RigCanvas::boneTip(int bone) const {
  if (!rig_ || bone < 0 || size_t(bone) >= pose_.world.size() || size_t(bone) >= rig_->bones.size()) return {};
  double x, y;
  pose_.world[size_t(bone)].apply(rig_->bones[size_t(bone)].length, 0, x, y);
  return {x, y};
}
QPolygonF RigCanvas::boneShape(int bone) const {
  if (!rig_ || bone < 0 || size_t(bone) >= pose_.world.size() || size_t(bone) >= rig_->bones.size() || !(rig_->bones[size_t(bone)].length > 0) || isIkTarget(bone)) return {};
  const auto& m = pose_.world[size_t(bone)];
  const QPointF o = rigToWidget(QPointF(m.e, m.f)), t = rigToWidget(boneTip(bone)), v = t - o;
  const double len = std::hypot(v.x(), v.y());
  if (len < MIN_BODY * ui_) return {};
  // Spine-like: widest a little after the origin, tapering to a point at the tip.
  const QPointF dir = v / len, side(-dir.y(), dir.x());
  // Many bones (plugin 1.20.0): thinner bodies so the art between them stays readable.
  const double thin = thinBones() ? 0.55 : 1.0;
  const double half = std::clamp(len * 0.1 * thin, 2.5 * ui_ * thin, 8.0 * ui_ * thin), knee = std::min(len * 0.2, half * 2.5);
  return QPolygonF({o, o + dir * knee + side * half, t, o + dir * knee - side * half});
}
QPointF RigCanvas::createEnd(const Drag& d) {
  const QPointF v = d.current - d.start;
  const double len = std::hypot(v.x(), v.y());
  if (!d.shift || len < 1e-9) return d.current;
  const double step = PI / 12, a = std::round(std::atan2(v.y(), v.x()) / step) * step;
  return d.start + QPointF(std::cos(a), std::sin(a)) * len;
}
RigCanvas::GestureValue RigCanvas::gestureValue(const Drag& d) {
  GestureValue v;
  const double fine = d.ctrl ? 0.1 : 1.0;
  const QPointF raw = d.current - d.start;
  v.delta = raw * fine;
  // Constrain: the dominant axis of the mouse motion (rig axes), so a slightly wobbly drag stays straight.
  if (d.shift && d.tool == Move) { if (std::fabs(raw.x()) >= std::fabs(raw.y())) v.delta.setY(0); else v.delta.setX(0); }
  v.angle = d.angle * fine;
  v.snapRotation = d.shift;
  v.snapStep = d.ctrl ? 1 : 15;
  v.ratio = 1 + (d.ratio - 1) * fine;
  if (d.shift && d.tool == Scale) {
    const double step = d.ctrl ? 0.01 : 0.1;
    v.ratio = std::round(v.ratio / step) * step;
    if (std::fabs(v.ratio) < step) v.ratio = v.ratio < 0 ? -step : step;  // never a zero scale
  }
  if (d.typed.isEmpty()) return v;
  // Typed value (plugin 1.19.0): numbers separated by "," (or ";"), "-" and "." allowed.
  QVector<double> n;
  for (const QString& part : d.typed.split(QRegularExpression("[,;]"))) {
    bool ok = false;
    const double x = part.trimmed().toDouble(&ok);
    n << (ok ? x : 0.0);
  }
  v.typed = true;
  v.snapRotation = false;
  if (d.tool == Move) {
    if (n.size() >= 2) v.delta = QPointF(n[0], n[1]);
    else if (std::fabs(raw.y()) > std::fabs(raw.x())) v.delta = QPointF(0, n.value(0));
    else v.delta = QPointF(n.value(0), 0);
  } else if (d.tool == Rotate) v.angle = n.value(0);
  else if (d.tool == Scale) v.ratio = std::fabs(n.value(0)) > 1e-9 ? n.value(0) : 1.0;
  return v;
}
QString RigCanvas::readoutText() const {
  if (!drag_ || !drag_->moved) return {};
  const Drag& d = *drag_;
  if (d.tool == Create) {
    const QPointF v = createEnd(d) - d.start;
    return QString("길이 %1 · 각도 %2°").arg(std::hypot(v.x(), v.y()), 0, 'f', 1).arg(std::atan2(v.y(), v.x()) * 180 / PI, 0, 'f', 1);
  }
  if (d.tool == Pivot) return {};
  const GestureValue g = gestureValue(d);
  QString text;
  if (d.tool == Move) text = QString("Δx %1 · Δy %2").arg(g.delta.x(), 0, 'f', 1).arg(g.delta.y(), 0, 'f', 1);
  else if (d.tool == Rotate) text = QString("회전 %1°").arg(g.snapRotation ? std::round(g.angle / g.snapStep) * g.snapStep : g.angle, 0, 'f', 1);
  else text = QString("크기 ×%1").arg(g.ratio, 0, 'f', 3);
  if (g.typed) return text + QString(" · 입력 %1▏ · Enter 확정 · Esc 취소").arg(d.typed);
  QStringList tags;
  if (d.shift) tags << (d.tool == Move ? QString("한 축") : d.tool == Rotate ? QString("%1° 단위").arg(g.snapStep) : QString(d.ctrl ? "1 % 단위" : "10 % 단위"));
  if (d.ctrl) tags << "미세 ×0.1";
  return text + (tags.isEmpty() ? QString() : " (" + tags.join(" · ") + ")") + " · 숫자 입력 · Esc 취소";
}
void RigCanvas::paintReadout(QPainter& p) {
  const QString text = readoutText();
  if (text.isEmpty()) return;
  const QFontMetrics fm = fontMetrics();
  const QRectF box(0, 0, fm.horizontalAdvance(text) + 12, fm.height() + 6);
  QPointF at = rigToWidget(drag_->current) + QPointF(16, 18) * ui_;
  at.setX(std::clamp(at.x(), 4.0, std::max(4.0, width() - box.width() - 4)));
  at.setY(std::clamp(at.y(), 4.0, std::max(4.0, height() - box.height() - 4)));
  const QRectF r = box.translated(at);
  p.setPen(QPen(SELECT_COLOR, 1));
  p.setBrush(QColor(11, 18, 32, 220));
  p.drawRoundedRect(r, 4, 4);
  p.setPen(QColor("#f8fafc"));
  p.drawText(r, Qt::AlignCenter, text);
  p.setBrush(Qt::NoBrush);
}
// Canvas keys (plugin 1.19.0). Letters that switch tools (V R S P B C, M W G, K, O) are the panel's
// QShortcuts (listed from there); these are the keys only the focused canvas owns, plus the mouse
// gestures, so the shortcut overlay is built from this table and the panel's shortcuts and cannot drift.
const QVector<RigCanvas::KeyBinding>& RigCanvas::keyBindings() {
  static const QVector<KeyBinding> list = {
    {"frame", "F", "보기", "선택 맞춤: 고른 본·조각이 화면을 채움 (선택이 없으면 전체)", {Qt::Key_F}},
    {"fitAll", "Shift+F", "보기", "전체 맞춤: 캐릭터 캔버스 전체", {Qt::Key_F}, Qt::ShiftModifier},
    {"home", "Home", "보기", "전체 맞춤", {Qt::Key_Home}, 0, true},
    {"pan", "Space 누른 채 끌기", "보기", "잠깐 화면 이동(손 도구) · 짧게 누르면 애니메이션 모드에서 재생/정지", {Qt::Key_Space}},
    {"wheel", "휠 · 가운데/오른쪽 버튼 끌기", "보기", "확대·축소 · 화면 이동", {}},
    {"fullView", "Tab", "보기", "캔버스 크게 (옆 패널 숨기기)", {}},
    {"escape", "Esc", "도구", "끄는 중이면 취소(원래대로) · 그리기 취소 · 메시/정점 편집 끄기 · 선택 도구로 · 선택 해제", {Qt::Key_Escape}, 0, true},
    {"nudge", "← ↑ → ↓", "변형", "고른 본·조각을 1 px (Shift: 10 px) 숫자 축 공간으로 옮김 · 이어 누르면 한 번의 편집 · 애니메이션 모드: 키", {Qt::Key_Left, Qt::Key_Right, Qt::Key_Up, Qt::Key_Down}, 0, true},
    {"dragShift", "끄는 중 Shift", "변형", "이동 한 축 · 회전 15° 단위 · 크기 10 % 단위", {}},
    {"dragCtrl", "끄는 중 Ctrl", "변형", "미세 조정 ×0.1 (Shift와 함께: 1° · 1 %)", {}},
    {"dragType", "끄는 중 숫자 → Enter", "변형", "정확한 값: 이동 dx,dy · 회전 각도 · 크기 배율 (Backspace 고치기, Esc 취소)", {}},
    {"selectAll", "Ctrl+A", "선택", "모든 본 선택", {Qt::Key_A}, int(Qt::ControlModifier)},
    {"multi", "Ctrl/Shift+클릭 · 빈 곳 끌기", "선택", "여러 개 고르기 · 상자 선택", {}},
    {"altPick", "Alt+클릭", "선택", "겹친 항목을 차례로 고르기", {}},
    {"doubleClick", "더블클릭", "선택", "조각 → 그 본 · 본 → 그 본의 첫 조각", {}},
    {"find", "Ctrl+F", "선택", "계층 찾기 칸으로 (계층·그리는 순서 목록에서는 그 목록의 찾기 칸): ID·PSD 레이어 이름으로 찾고 Enter로 첫 결과 고르기", {}},
    {"context", "오른쪽 클릭", "편집", "가리킨 항목의 메뉴 (선택·부모·키·방향·판정 상자·메시·숨기기·복제·삭제)", {}},
    {"delete", "Delete", "편집", "고른 본·조각 삭제 (확인 후, 삭제 버튼과 같음)", {Qt::Key_Delete, Qt::Key_Backspace}},
    {"duplicate", "Ctrl+D", "편집", "복제: 고른 본(자식·조각까지) 또는 조각 (조립 모드)", {Qt::Key_D}, int(Qt::ControlModifier)},
    {"meshDelete", "Delete (정점 편집)", "메시", "고른 정점 삭제", {}},
    {"outline", "Enter · Backspace (윤곽 그리기)", "메시", "윤곽 닫기 · 마지막 점 지우기", {}},
    {"overlay", "?", "도움말", "이 단축키 모음 열기·닫기", {Qt::Key_Question}, 0, true},
  };
  return list;
}
QString RigCanvas::bindingFor(int key, Qt::KeyboardModifiers modifiers) {
  modifiers &= ~Qt::KeypadModifier;  // arrows / digits of the keypad are the same keys
  if (key == Qt::Key_Slash && (modifiers & Qt::ShiftModifier)) key = Qt::Key_Question;  // US layout ?
  for (const KeyBinding& b : keyBindings()) {
    if (!b.codes.contains(key)) continue;
    const Qt::KeyboardModifiers want = Qt::KeyboardModifiers(b.modifiers);
    if (b.anyShift ? (modifiers & ~Qt::ShiftModifier) == want : modifiers == want) return b.id;
  }
  return {};
}
/** "#rrggbb" / "#rrggbbaa" bone colour (data), else the mode colour. */
QColor RigCanvas::boneColor(int bone) const {
  const std::string& c = rig_->bones[size_t(bone)].color;
  if (c.size() == 7 || c.size() == 9) {
    bool ok = true;
    const uint v = QString::fromStdString(c.substr(1)).toUInt(&ok, 16);
    if (ok) return c.size() == 7 ? QColor((v >> 16) & 255, (v >> 8) & 255, v & 255) : QColor((v >> 24) & 255, (v >> 16) & 255, (v >> 8) & 255, v & 255);
  }
  return QColor(animate_ ? "#f3a3a3" : "#9cc3ff");
}

void RigCanvas::updateHover(const QPointF& w, bool alt) {
  hoverAt_ = w;
  hoverAlt_ = alt;
  hoverInside_ = true;
  if (spaceHeld_) return;  // the hand cursor stays while Space is held
  // Mesh modes own the left button (vertices/brush): nothing else would be picked there.
  const Hit hit = meshMode_ != MeshOff || outlineDrawing() || placingPoint_ ? Hit{} : pickHit(w, alt);
  const QString ref = hit.ref;
  if (ref == hoverRef_ && hit.body == hoverBody_) return;
  hoverRef_ = ref;
  hoverBody_ = hit.body;
  if (ref.isEmpty()) unsetCursor();
  else setCursor(ref.startsWith("bone:") ? Qt::PointingHandCursor : Qt::OpenHandCursor);
  update();
}
void RigCanvas::outlineSlot(QPainter& p, int si) const {
  const cutout::Attachment* att = nullptr;
  const auto m = attachmentMatrix(*rig_, pose_, si, &att);
  if (!att) return;
  if (att->clipping) { p.drawPolygon(viewTransform().map(clipPolygon(*rig_, pose_, si))); return; }
  if (att->boundingBox) { p.drawPolygon(viewTransform().map(boxPolygon(*rig_, pose_, si))); return; }
  if (att->point) {
    QPointF at;
    double rotation = 0;
    if (pointOf(*rig_, pose_, si, at, rotation)) p.drawEllipse(rigToWidget(at), (POINT_ARM + 2) * ui_, (POINT_ARM + 2) * ui_);
    return;
  }
  if (att->mesh) {
    const QVector<QPointF> pts = meshPoints(*rig_, pose_, si);
    for (const auto& [a, b] : meshOutline(att->triangles)) if (a < pts.size() && b < pts.size()) p.drawLine(rigToWidget(pts[a]), rigToWidget(pts[b]));
    return;
  }
  p.drawPolygon((qtTransform(m) * viewTransform()).map(QPolygonF(QRectF(0, 0, att->width, att->height))));
}
/** Clip polygons of the shown clipping slots (plugin 1.12.0): magenta dashes, so the area a clip
 *  keeps is visible even where nothing is drawn yet; the selected one is drawn by outlineSlot. */
void RigCanvas::paintClips(QPainter& p) {
  if (!showClips_ || pose_.world.size() != rig_->bones.size()) return;
  p.setBrush(Qt::NoBrush);
  for (size_t i = 0; i < rig_->slots.size(); i++) {
    if (hidden_.contains(qs(rig_->slots[i].id))) continue;
    const QPolygonF polygon = clipPolygon(*rig_, pose_, int(i));
    if (polygon.size() < 3) continue;
    p.setPen(QPen(QColor(255, 122, 217, 220), 1.25 * ui_, Qt::DashDotLine));
    p.drawPolygon(viewTransform().map(polygon));
  }
}
// Hover cue: a thin light outline (part) or ring (handle) on what a click would pick, so a
// crosshair over an image visibly wins before the press.
void RigCanvas::paintHover(QPainter& p) {
  if (hoverRef_.isEmpty() || isSelected(hoverRef_) || drag_ || panning_ || band_) return;
  const QString id = hoverRef_.section(':', 1);
  p.setPen(QPen(QColor(255, 255, 255, 200), 1.5));
  p.setBrush(Qt::NoBrush);
  if (hoverRef_.startsWith("slot:")) {
    if (const int si = rig_->slotIndex(ss(id)); si >= 0) outlineSlot(p, si);
  } else if (const int bi = rig_->boneIndex(ss(id)); bi >= 0) {
    const auto& m = pose_.world[size_t(bi)];
    const QPolygonF shape = boneShape(bi);
    if (hoverBody_ && !shape.isEmpty()) { p.drawPolygon(shape); return; }  // the body a drag would turn
    const double r = (isIkTarget(bi) ? IK_RADIUS + 2 : BONE_RADIUS + 1) * ui_;
    p.drawEllipse(rigToWidget(QPointF(m.e, m.f)), r, r);
  }
}
// Bones (plugin 1.5.0, Spine-like): a link to the parent, then a tapered body from origin to tip
// for bones with a length (fill = bone colour), else the origin circle and local x axis tick.
void RigCanvas::paintBones(QPainter& p) {
  const QVector<bool> shown = shownBones();
  // Many bones: smaller origins and thinner lines (bodies: boneShape); 본 흐리게: unselected at 40 %.
  const double thin = thinBones() ? 0.7 : 1.0;
  for (size_t i = 0; i < rig_->bones.size(); i++) {
    if (!shown[int(i)]) continue;
    const auto& w = pose_.world[i];
    const QPointF at = rigToWidget(QPointF(w.e, w.f));
    const QString ref = "bone:" + qs(rig_->bones[i].id);
    const bool selected = isSelected(ref), hovered = ref == hoverRef_ && !drag_;
    p.setOpacity(bonesDim_ && !selected && !hovered ? 0.4 : 1.0);
    const int parent = rig_->bones[i].parentIndex;
    if (parent >= 0) {
      // A child starting at its parent's tip needs no link (the bodies already join).
      const bool long_ = rig_->bones[size_t(parent)].length > 0;
      const QPointF from = rigToWidget(long_ ? boneTip(parent) : QPointF(pose_.world[size_t(parent)].e, pose_.world[size_t(parent)].f));
      if (std::hypot(from.x() - at.x(), from.y() - at.y()) > 2) {
        p.setPen(QPen(QColor(255, 255, 255, 90), 1, long_ ? Qt::DotLine : Qt::SolidLine));
        p.drawLine(from, at);
      }
    }
    if (isIkTarget(int(i))) {
      // IK target: a crosshair (no axis: only its origin matters) that reads as "drag me".
      p.setPen(QPen(selected ? SELECT_COLOR : IK_COLOR, (selected ? 2.5 : 2) * ui_));
      p.drawEllipse(at, 8.0 * ui_, 8.0 * ui_);
      p.drawLine(at - QPointF(12 * ui_, 0), at + QPointF(12 * ui_, 0));
      p.drawLine(at - QPointF(0, 12 * ui_), at + QPointF(0, 12 * ui_));
      continue;
    }
    const QColor color = boneColor(int(i));
    const QPolygonF shape = boneShape(int(i));
    if (!shape.isEmpty()) {
      QColor fill = selected ? SELECT_COLOR : color;
      fill.setAlpha(selected ? 190 : hovered ? 170 : 125);
      p.setPen(QPen(selected ? SELECT_COLOR : color.darker(160), selected ? 2 : 1.2));
      p.setBrush(fill);
      p.drawPolygon(shape);
      p.setPen(QPen(QColor("#0b1220"), 1));
      p.setBrush(selected ? SELECT_COLOR : color.lighter(115));
      p.drawEllipse(at, (selected ? 4.5 : 3.5 * thin) * ui_, (selected ? 4.5 : 3.5 * thin) * ui_);
      p.setBrush(Qt::NoBrush);
      continue;
    }
    const double len = std::hypot(w.a, w.b);
    const QPointF axis = len > 1e-9 ? QPointF(w.a / len, w.b / len) * AXIS_LENGTH * ui_ * thin : QPointF(AXIS_LENGTH * ui_ * thin, 0);
    p.setPen(QPen(selected ? SELECT_COLOR : QColor("#c8d2dc"), (selected ? 2.5 : 1.5 * thin) * ui_));
    p.drawLine(at, at + axis);
    p.setBrush(selected ? SELECT_COLOR : color);
    p.drawEllipse(at, (selected ? 6.0 : 4.5 * thin) * ui_, (selected ? 6.0 : 4.5 * thin) * ui_);
    p.setBrush(Qt::NoBrush);
  }
  p.setOpacity(1.0);
}
QVector<bool> RigCanvas::shownBones() const {
  const int n = rig_ ? int(rig_->bones.size()) : 0;
  QVector<bool> shown(n, boneDisplay_ == BonesAll);
  if (boneDisplay_ == BonesAll || !n) return shown;
  // The selected bones (a selected part = its bone) are always drawn.
  QVector<int> picked;
  QSet<QString> refs = multi_;
  if (!selection_.isEmpty()) refs.insert(selection_);
  for (const QString& ref : refs) {
    int b = -1;
    if (ref.startsWith("bone:")) b = rig_->boneIndex(ss(ref.section(':', 1)));
    else if (ref.startsWith("slot:")) if (const int si = rig_->slotIndex(ss(ref.section(':', 1))); si >= 0) b = rig_->slots[size_t(si)].boneIndex;
    if (b >= 0 && b < n) { shown[b] = true; picked << b; }
  }
  if (boneDisplay_ == BonesHidden) return shown;
  // 선택·관련만: parents, children and the bones sharing an IK or transform constraint.
  for (const int b : picked) {
    if (const int parent = rig_->bones[size_t(b)].parentIndex; parent >= 0) shown[parent] = true;
    for (int c = 0; c < n; c++) if (rig_->bones[size_t(c)].parentIndex == b) shown[c] = true;
    auto partners = [&](const std::vector<int>& bones, int target, int tip) {
      if (std::find(bones.begin(), bones.end(), b) == bones.end() && target != b && tip != b) return;
      for (int o : bones) if (o >= 0 && o < n) shown[o] = true;
      if (target >= 0 && target < n) shown[target] = true;
      if (tip >= 0 && tip < n) shown[tip] = true;
    };
    for (const auto& k : rig_->ik) partners(k.bones, k.target, k.tip);
    for (const auto& t : rig_->transforms) partners(t.bones, t.target, -1);
  }
  return shown;
}

void RigCanvas::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), QColor("#1b1f24"));
  const QTransform view = viewTransform();
  if (rig_) {
    p.setTransform(view);
    const QRectF area(0, 0, rig_->width, rig_->height);
    p.fillRect(area, QColor("#2b3139"));
    p.setPen(QPen(QColor("#56606c"), 0));
    p.drawRect(area);
    paintGhosts(p);
    if (images_ && pose_.world.size() == rig_->bones.size()) drawRig(p, *rig_, pose_, *images_, hidden_);
    p.resetTransform();
    p.setRenderHint(QPainter::Antialiasing, true);
    if (trail_.size() > 1) {
      // Motion trail of the selected bone origin over the clip; dots mark key times.
      QPolygonF line;
      for (const QPointF& pt : trail_) line << rigToWidget(pt);
      p.setPen(QPen(TRAIL_COLOR, 1.5));
      p.drawPolyline(line);
      p.setPen(QPen(QColor("#0b1220"), 1));
      p.setBrush(TRAIL_COLOR);
      for (const QPointF& k : trailKeys_) p.drawEllipse(rigToWidget(k), 3.5 * ui_, 3.5 * ui_);
      p.setBrush(Qt::NoBrush);
    }
    // Instance anchor (rig origin): where the character stands on screen.
    const QPointF o = rigToWidget(QPointF(rig_->originX, rig_->originY));
    p.setPen(QPen(QColor("#8fd18f"), 1, Qt::DashLine));
    p.drawLine(o - QPointF(12 * ui_, 0), o + QPointF(12 * ui_, 0));
    p.drawLine(o - QPointF(0, 12 * ui_), o + QPointF(0, 12 * ui_));
    paintClips(p);
    paintBoxes(p);
    if (pose_.world.size() == rig_->bones.size()) {
      // Selected part outline.
      p.setPen(QPen(SELECT_COLOR, 1.5, Qt::DashLine));
      for (size_t i = 0; i < rig_->slots.size(); i++) if (isSelected("slot:" + qs(rig_->slots[i].id))) outlineSlot(p, int(i));
      if (boneDisplay_ != BonesHidden) { paintIk(p); paintTransforms(p); }
      paintBones(p);
      if (boneDisplay_ != BonesHidden) paintPhysics(p);
      paintPoints(p);
    }
    if (pose_.world.size() == rig_->bones.size()) paintHover(p);
    paintMesh(p);
    paintOutline(p);
    if (band_) {
      p.setPen(QPen(SELECT_COLOR, 1, Qt::DashLine));
      p.setBrush(QColor(255, 211, 77, 30));
      p.drawRect(band_->normalized());
      p.setBrush(Qt::NoBrush);
    }
    if (drag_ && drag_->moved && (drag_->tool == Rotate || drag_->tool == Scale)) {
      p.setPen(QPen(SELECT_COLOR, 1, Qt::DotLine));
      p.drawLine(rigToWidget(drag_->origin), rigToWidget(drag_->current));
    }
    if (tool_ == Create && drag_) {
      // The bone being drawn (the panel previews the real one; this guide also shows a click).
      const QPointF a = rigToWidget(drag_->start), b = rigToWidget(createEnd(*drag_));
      p.setPen(QPen(SELECT_COLOR, 1.5, Qt::DashLine));
      p.drawLine(a, b);
      p.drawEllipse(a, 4 * ui_, 4 * ui_);
    }
    if (tool_ == Pivot && drag_) {
      const QPointF c = rigToWidget(drag_->current);
      p.setPen(QPen(SELECT_COLOR, 2));
      p.drawEllipse(c, 8 * ui_, 8 * ui_);
      p.drawLine(c - QPointF(12 * ui_, 0), c + QPointF(12 * ui_, 0));
      p.drawLine(c - QPointF(0, 12 * ui_), c + QPointF(0, 12 * ui_));
    }
    paintReadout(p);
  }
  p.resetTransform();
  if (!message_.isEmpty()) {
    p.setPen(QColor("#f0c0c0"));
    p.drawText(rect().adjusted(16, 16, -16, -16), Qt::AlignCenter | Qt::TextWordWrap, message_);
  }
  // The hint line (plugin 1.20.0): a dark band under it so it reads over any artwork, elided to
  // the canvas width instead of running past it (보기 ▾ 캔버스 안내 줄 hides it).
  if (const QString hint = hintLineText(); !hint.isEmpty()) {
    const QRectF band = hintLineRect();
    p.fillRect(band, QColor(12, 16, 22, 190));
    p.setPen(QColor("#b4bec8"));
    p.drawText(band.adjusted(HINT_PAD, 0, -HINT_PAD, 0), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, hint);
  }
  // Strong mode cue: the whole canvas is framed in the mode color.
  p.setPen(QPen(animate_ ? ANIMATE_COLOR : SETUP_COLOR, 4));
  p.drawRect(QRectF(rect()).adjusted(2, 2, -2, -2));
}

QString RigCanvas::hintLine() const {
  static const char* names[] = {"이동(V)", "회전(R)", "크기(S)", "회전 중심(P)", "본 만들기(B)"};
  QString hint = placingPoint_ ? QString("기준점 놓기: 캔버스를 클릭한 곳에 고른 본의 기준점이 생깁니다 · 끌어서 옮기고 끝의 둥근 손잡이로 방향을 돌림 · Esc 취소")
                 : outlineDrawing() ? QString("윤곽 그리기: 그림 둘레를 차례로 클릭 · 첫 점 클릭·더블클릭·Enter로 닫기(메시 만들기) · Backspace 마지막 점 지우기 · Esc 취소")
                 : meshMode_ == MeshVertices && meshTool_ == MeshAddVertex ? QString("정점 추가: 메시 안(안쪽 정점)이나 윤곽 가까이(윤곽 정점)를 클릭 · 다시 삼각분할됩니다")
                 : meshMode_ == MeshVertices && meshTool_ == MeshAddEdge ? QString("가장자리 추가: 정점에서 다른 정점으로 끌면 그 안쪽 가장자리가 고정됩니다(하늘색) · 주황 = 윤곽")
                 : meshMode_ == MeshVertices ? QString("메시 정점: 클릭·Shift+클릭 선택 · 빈 곳 끌기 상자 선택 · 정점 끌기로 이동") + (animate_ ? " (K로 변형 키)" : " · Delete 정점 삭제")
                       : meshMode_ == MeshWeights ? QString("웨이트 칠하기: 끌어서 붓으로 칠함 · Shift+끌기 부드럽게 · Ctrl+끌기 빼기 · [ ] 붓 크기 · 정점에 올리면 웨이트 · 색 = 고른 본(파랑 0 → 빨강 1)")
                       : tool_ == Create ? QString("본 만들기: 끌어서 고른 본의 자식 본(길이·방향) · 클릭 = 점 본 · 본 원점 클릭 = 부모 고르기 · Shift 15° 스냅")
                                         : QString("도구: %1 · 본 몸통 끌기 = 회전, 원점 끌기 = 이동 · Ctrl/Shift+클릭 여러 개 선택 · 빈 곳 끌기 상자 선택 · V·R·S 도구 · H 본 표시 · 끄는 중 Shift 한 축/15°, Ctrl 미세, 숫자+Enter 정확히, Esc 취소 · 화살표 1 px · Space+끌기 화면 이동 · Alt+클릭 겹친 항목 · ? 단축키").arg(names[std::clamp(tool_, 0, TOOL_COUNT - 1)]);
  // Game-connection preview (plugin 1.21.0): what a click does there, not the edit tools.
  if (previewClicks_) hint = QString("게임 연결 미리 보기: 판정 상자(안이나 윤곽선)를 클릭하면 partClicked 신호 · Ctrl+클릭 = 선택");
  // What a click would pick (handles beat images): named so an overlap is never a surprise.
  const QString kind = hoverRef_.startsWith("slot:") ? "조각" : rig_ && isIkTarget(rig_->boneIndex(ss(hoverRef_.section(':', 1)))) ? "IK 대상" : "본";
  if (!hoverRef_.isEmpty()) hint = QString("가리킴: %1 %2 · ").arg(kind, hoverRef_.section(':', 1)) + hint;
  return hint + " · 휠 확대 · 가운데·오른쪽 버튼 끌기로 화면 이동";
}
QRectF RigCanvas::hintLineRect() const {
  const int h = fontMetrics().height() + 6;  // follows the (scaled) font
  return QRectF(6, height() - h - 6, std::max(0, width() - 12), h);
}
QString RigCanvas::hintLineText() const {
  if (!showHint_ || !rig_) return {};
  return fontMetrics().elidedText(hintLine(), Qt::ElideRight, std::max(0, int(hintLineRect().width()) - 2 * HINT_PAD));
}

void RigCanvas::mousePressEvent(QMouseEvent* e) {
  setFocus(Qt::MouseFocusReason);
  if (drag_) return;  // a second button during a drag changes nothing
  pressWidget_ = e->position();
  // Space held (plugin 1.19.0, Photoshop-like hand tool): a left drag pans whatever the tool or mode.
  const bool hand = spaceHeld_ && e->button() == Qt::LeftButton;
  if (e->button() == Qt::MiddleButton || e->button() == Qt::RightButton || hand) {
    panning_ = true; panStart_ = e->position(); panOrigin_ = pan_;
    rightPan_ = e->button() == Qt::RightButton;
    suppressContext_ = false;
    if (hand) { spacePanned_ = true; setCursor(Qt::ClosedHandCursor); }
    return;
  }
  if (e->button() != Qt::LeftButton) return;
  if (placingPoint_) { if (onPlacePoint) onPlacePoint(widgetToRig(e->position())); return; }
  if (previewClicks_ && !(e->modifiers() & Qt::ControlModifier) && onPreviewClick && onPreviewClick(widgetToRig(e->position()), boxOutlineRadius() / std::max(zoom_, 1e-6))) return;
  if (outlinePress(e)) return;
  if (meshPress(e)) return;
  // One pick for every tool: handles first, Alt = the next candidate under the cursor.
  const Hit picked = pickHit(e->position(), e->modifiers() & Qt::AltModifier);
  const QString hit = picked.ref;
  if (tool_ == Create) {
    // A bone origin picks the parent of the next bone; a press anywhere else starts drawing one
    // (from the tip of the last bone, the next chain link). Release always commits (click = point bone).
    if (hit.startsWith("bone:") && !picked.body) { if (hit != selection_ && onSelect) onSelect(hit); return; }
    Drag d;
    d.ref = selection_;
    d.tool = Create;
    d.start = d.current = d.origin = widgetToRig(e->position());
    d.shift = e->modifiers() & Qt::ShiftModifier;
    lastAngle_ = 0;
    drag_ = d;
    update();
    return;
  }
  // Multi-selection (plugin 1.7.0): Ctrl/Shift+click toggles an item; with the Move tool a press
  // on empty space starts a box selection (Ctrl/Shift = add), so left-drag no longer pans there.
  const bool adding = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
  if (tool_ != Pivot && adding && !hit.isEmpty()) { if (onToggleSelect) onToggleSelect(hit); return; }
  if (tool_ == Move && hit.isEmpty()) {
    band_ = QRectF(e->position(), e->position());
    bandAdd_ = adding;
    update();
    return;
  }
  // Move picks what is under the cursor; rotate/scale keep working on the selection unless a
  // bone handle is clicked, so a drag that starts over another image still turns the chosen bone.
  QString target = selection_;
  if (tool_ == Move) target = hit;
  else if (tool_ != Pivot && !hit.isEmpty() && (hit.startsWith("bone:") || selection_.isEmpty())) target = hit;
  // A press on a member of a multi-selection keeps the whole selection (the drag moves all of
  // it); a plain click there (no drag) keeps only that item, on release.
  pendingSingle_.clear();
  if (multi_.size() > 1 && multi_.contains(target)) pendingSingle_ = target;
  else if (target != selection_ && onSelect) onSelect(target);
  if (tool_ == Pivot && selection_.isEmpty() && !hit.isEmpty()) return;  // first click only selects
  if (target.isEmpty()) {
    panning_ = true; panStart_ = e->position(); panOrigin_ = pan_;
    return;
  }
  Drag d;
  d.ref = target; d.tool = tool_;
  // Spine "pose" behaviour in both Move and Rotate: a bone's body turns it around its origin,
  // its origin moves it.
  if ((tool_ == Move || tool_ == Rotate) && target == hit && hit.startsWith("bone:")) {
    d.body = picked.body;
    d.tool = picked.body ? Rotate : Move;
  } else if ((tool_ == Move || tool_ == Rotate) && target == hit && hit.startsWith("slot:")) {
    // A point (plugin 1.17.0): its handle turns it, its centre moves it, whichever tool is on.
    QPointF at;
    double rotation = 0;
    if (rig_ && pointOf(*rig_, pose_, rig_->slotIndex(ss(hit.section(':', 1))), at, rotation)) { d.body = picked.body; d.tool = picked.body ? Rotate : Move; }
  }
  d.start = d.current = widgetToRig(e->position());
  d.origin = originOf ? originOf(target) : d.start;
  d.shift = e->modifiers() & Qt::ShiftModifier;
  lastAngle_ = std::atan2(d.start.y() - d.origin.y(), d.start.x() - d.origin.x());
  drag_ = d;
  update();
}
void RigCanvas::mouseMoveEvent(QMouseEvent* e) {
  if (band_) { band_->setBottomRight(e->position()); update(); return; }
  if (panning_) {
    pan_ = panOrigin_ + (e->position() - panStart_);
    autoFit_ = false;
    update();
    return;
  }
  if (outlineDrawing() && !drag_) { hover_ = e->position(); update(); return; }
  if (meshGesture_ != MeshGesture::None || (meshMode_ == MeshWeights && !drag_)) { meshMove(e->position()); return; }
  if (!drag_) { if (!(e->buttons() & Qt::LeftButton)) updateHover(e->position(), e->modifiers() & Qt::AltModifier); return; }
  Drag& d = *drag_;
  d.current = widgetToRig(e->position());
  d.shift = e->modifiers() & Qt::ShiftModifier;
  d.ctrl = e->modifiers() & Qt::ControlModifier;
  const double a = std::atan2(d.current.y() - d.origin.y(), d.current.x() - d.origin.x());
  d.angle += normalize(a - lastAngle_) * 180 / PI;
  lastAngle_ = a;
  const double r0 = std::hypot(d.start.x() - d.origin.x(), d.start.y() - d.origin.y());
  d.ratio = r0 > 1e-6 ? std::hypot(d.current.x() - d.origin.x(), d.current.y() - d.origin.y()) / r0 : 1;
  if ((e->position() - pressWidget_).manhattanLength() > 2) d.moved = true;
  if (d.moved && onDrag) onDrag(d, false);
  update();
}
void RigCanvas::mouseReleaseEvent(QMouseEvent* e) {
  if (band_) {
    const QRectF r = band_->normalized();
    band_.reset();
    // A click (no drag) on empty space clears the selection; a real box selects the bones inside.
    if (r.width() + r.height() <= 4) { if (!bandAdd_ && onSelect) onSelect(QString()); }
    else if (onBoxSelect) onBoxSelect(bonesIn(r), bandAdd_);
    updateHover(e->position(), e->modifiers() & Qt::AltModifier);
    update();
    return;
  }
  if (panning_) {
    panning_ = false;
    // A right drag that moved the view is a pan: the context menu that follows the release stays shut.
    if (rightPan_ && e->button() == Qt::RightButton) suppressContext_ = (e->position() - pressWidget_).manhattanLength() > 3;
    rightPan_ = false;
    if (spaceHeld_) setCursor(Qt::OpenHandCursor);
    return;
  }
  if (meshGesture_ != MeshGesture::None) { meshRelease(e->position()); return; }
  if (!drag_) return;
  Drag d = *drag_;
  d.current = widgetToRig(e->position());
  if (d.tool == Create) d.shift = e->modifiers() & Qt::ShiftModifier;
  drag_.reset();
  // Pivot placement and bone creation are clicks too; other tools commit only real drags (a click just selects).
  if ((d.moved || d.tool == Pivot || d.tool == Create) && onDrag) onDrag(d, true);
  else if (!d.moved && !pendingSingle_.isEmpty() && onSelect) onSelect(pendingSingle_);
  pendingSingle_.clear();
  updateHover(e->position(), e->modifiers() & Qt::AltModifier);
  update();
}
void RigCanvas::mouseDoubleClickEvent(QMouseEvent* e) {
  if (outlineDrawing() && e->button() == Qt::LeftButton) { finishOutline(); return; }
  // Plugin 1.19.0: with the select/transform tools a double-click hops between a part and its bone
  // (the panel decides which); the first click of the pair already picked the item.
  if (e->button() == Qt::LeftButton && !spaceHeld_ && !drag_ && meshMode_ == MeshOff && !placingPoint_ && (tool_ == Move || tool_ == Rotate || tool_ == Scale) && onDoubleClick) {
    const QString ref = pickHit(e->position(), e->modifiers() & Qt::AltModifier).ref;
    if (!ref.isEmpty()) { band_.reset(); onDoubleClick(ref); update(); return; }
  }
  QWidget::mouseDoubleClickEvent(e);  // = a second press for every other tool
}
void RigCanvas::wheelEvent(QWheelEvent* e) {
  const double factor = std::pow(1.0015, e->angleDelta().y());
  const QPointF anchor = widgetToRig(e->position());
  zoom_ = std::clamp(zoom_ * factor, 0.02, 40.0);
  pan_ = e->position() - anchor * zoom_;
  autoFit_ = false;
  update();
}
void RigCanvas::resizeEvent(QResizeEvent*) { if (autoFit_) fitToCanvas(); }
static bool imageDrop(const QMimeData* m) { return m && (m->hasUrls() || m->hasText()); }
void RigCanvas::dragEnterEvent(QDragEnterEvent* e) { if (onDropImage && imageDrop(e->mimeData())) e->acceptProposedAction(); }
void RigCanvas::dragMoveEvent(QDragMoveEvent* e) { if (onDropImage && imageDrop(e->mimeData())) e->acceptProposedAction(); }
void RigCanvas::dropEvent(QDropEvent* e) {
  if (!onDropImage || !imageDrop(e->mimeData())) return;
  e->acceptProposedAction();
  // The part under the cursor (bone handles ignored: an image always lands on a part).
  onDropImage(e->mimeData(), hitTest(e->position(), false));
}
void RigCanvas::setSpaceHeld(bool on) {
  if (spaceHeld_ == on) return;
  spaceHeld_ = on;
  if (on) { spacePanned_ = false; setCursor(panning_ ? Qt::ClosedHandCursor : Qt::OpenHandCursor); return; }
  unsetCursor();
  const QString keep = hoverRef_;
  hoverRef_.clear();  // re-derive the hover cursor (it was not touched while the hand showed)
  if (hoverInside_) updateHover(hoverAt_, hoverAlt_);
  if (hoverRef_.isEmpty() && !keep.isEmpty()) update();
}
void RigCanvas::refreshDragModifiers(bool shift, bool ctrl) {
  if (!drag_ || (drag_->shift == shift && drag_->ctrl == ctrl)) return;
  drag_->shift = shift;
  drag_->ctrl = ctrl;
  if (drag_->moved && onDrag) onDrag(*drag_, false);
  update();
}
void RigCanvas::cancelDrag() {
  drag_.reset();
  pendingSingle_.clear();
  if (onDragCancel) onDragCancel();
  update();
}
// Exact values while dragging (plugin 1.19.0): digits, "-", "." and "," build Drag::typed, which
// replaces the mouse in gestureValue(); Enter commits that ONE edit at once, Esc cancels.
bool RigCanvas::typeIntoDrag(QKeyEvent* e) {
  if (!drag_ || (drag_->tool != Move && drag_->tool != Rotate && drag_->tool != Scale)) return false;
  const int k = e->key();
  Drag& d = *drag_;
  if (k == Qt::Key_Return || k == Qt::Key_Enter) {
    Drag done = d;
    done.moved = true;
    drag_.reset();
    pendingSingle_.clear();
    if (onDrag) onDrag(done, true);
    update();
    return true;
  }
  QString add;
  if (k >= Qt::Key_0 && k <= Qt::Key_9) add = QChar('0' + (k - Qt::Key_0));
  else if (k == Qt::Key_Minus) add = "-";
  else if (k == Qt::Key_Period) add = ".";
  else if (k == Qt::Key_Comma || k == Qt::Key_Semicolon) add = ",";
  if (add.isEmpty() && !(k == Qt::Key_Backspace && !d.typed.isEmpty())) return false;
  if (add.isEmpty()) d.typed.chop(1);
  else if (d.typed.size() < 32) d.typed += add;
  d.moved = true;
  if (onDrag) onDrag(d, false);
  update();
  return true;
}
void RigCanvas::keyPressEvent(QKeyEvent* e) {
  const int key = e->key();
  if (drag_) {
    if (key == Qt::Key_Escape) { cancelDrag(); return; }
    if (key == Qt::Key_Shift || key == Qt::Key_Control) { refreshDragModifiers(e->modifiers() & Qt::ShiftModifier || key == Qt::Key_Shift, e->modifiers() & Qt::ControlModifier || key == Qt::Key_Control); return; }
    if (typeIntoDrag(e)) return;
    e->accept();  // nothing else acts in the middle of a drag
    return;
  }
  if (band_ && key == Qt::Key_Escape) { band_.reset(); update(); return; }
  if (meshGesture_ != MeshGesture::None && key == Qt::Key_Escape) {
    // A vertex drag / stroke in progress: dropped, its preview goes away (no edit).
    meshGesture_ = MeshGesture::None;
    meshStroke_.clear();
    meshReach_.clear();
    edgeFrom_ = -1;
    if (onDragCancel) onDragCancel();
    update();
    return;
  }
  if (placingPoint_ && key == Qt::Key_Escape) { setPointPlacing(false); if (onPlaceCancel) onPlaceCancel(); return; }
  if (outlineDrawing()) {
    if (key == Qt::Key_Return || key == Qt::Key_Enter) { finishOutline(); return; }
    if (key == Qt::Key_Backspace && !outline_.isEmpty()) { outline_.removeLast(); update(); return; }
    if (key == Qt::Key_Escape) { setOutlineDrawing(QString()); if (onOutlineCancel) onOutlineCancel(); return; }
  }
  if (meshMode_ == MeshVertices && (key == Qt::Key_Delete || key == Qt::Key_Backspace) && onMeshDelete) { onMeshDelete(); return; }
  if (meshMode_ == MeshWeights && (key == Qt::Key_BracketLeft || key == Qt::Key_BracketRight) && onBrushStep) { onBrushStep(key == Qt::Key_BracketRight ? 1 : -1); return; }
  if (key == Qt::Key_Space && e->modifiers() == Qt::NoModifier) { if (!e->isAutoRepeat()) setSpaceHeld(true); return; }
  // Tool letters normally arrive as the panel's shortcuts; a canvas on its own handles them here.
  const int tools[] = {Qt::Key_V, Qt::Key_R, Qt::Key_S, Qt::Key_P, Qt::Key_C};
  for (int i = 0; i < TOOL_COUNT; i++) if (key == tools[i] && !(e->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) { if (onTool) onTool(i); return; }
  if (key == Qt::Key_B && !(e->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) { if (onTool) onTool(Create); return; }
  if (key == Qt::Key_K && !(e->modifiers() & Qt::ControlModifier) && onKey) { onKey(); return; }
  if (key == Qt::Key_O && !(e->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) && onOnion) { onOnion(); return; }
  if (key == Qt::Key_Alt && hoverInside_) { updateHover(hoverAt_, true); return; }  // show the Alt pick at once
  const QString id = bindingFor(key, e->modifiers());
  if (id == "frame") { frameSelection(); return; }
  if (id == "fitAll" || id == "home") { fitToCanvas(); return; }
  if (id == "selectAll") { if (onSelectAll) onSelectAll(); return; }
  if (id == "escape") { if (onEscape) onEscape(); return; }
  if (id == "nudge") {
    // Mesh modes own the canvas: arrows never move bones behind an edited mesh.
    if (meshMode_ != MeshOff || outlineDrawing() || placingPoint_) return;
    const int step = e->modifiers() & Qt::ShiftModifier ? 10 : 1;
    const QPoint dir = key == Qt::Key_Left ? QPoint(-1, 0) : key == Qt::Key_Right ? QPoint(1, 0) : key == Qt::Key_Up ? QPoint(0, -1) : QPoint(0, 1);
    if (onNudge) onNudge(dir * step);
    return;
  }
  if (id == "delete") { if (onDeleteSelection) onDeleteSelection(); return; }
  if (id == "duplicate") { if (onDuplicate) onDuplicate(); return; }
  if (id == "overlay") { if (onShortcutOverlay) onShortcutOverlay(); return; }
  QWidget::keyPressEvent(e);
}
void RigCanvas::keyReleaseEvent(QKeyEvent* e) {
  if (drag_ && (e->key() == Qt::Key_Shift || e->key() == Qt::Key_Control)) {
    refreshDragModifiers((e->modifiers() & Qt::ShiftModifier) && e->key() != Qt::Key_Shift, (e->modifiers() & Qt::ControlModifier) && e->key() != Qt::Key_Control);
    return;
  }
  if (e->key() == Qt::Key_Space && !e->isAutoRepeat() && spaceHeld_) {
    // A tap (no hand drag) is still the panel's Space: play/stop in Animate mode.
    const bool tap = !spacePanned_;
    setSpaceHeld(false);
    if (tap && onSpaceTap) onSpaceTap();
    return;
  }
  if (e->key() == Qt::Key_Alt && hoverInside_) { updateHover(hoverAt_, false); return; }
  QWidget::keyReleaseEvent(e);
}
void RigCanvas::contextMenuEvent(QContextMenuEvent* e) {
  if (suppressContext_) { suppressContext_ = false; return; }
  if (!onContextMenu || drag_ || panning_) return;
  // The menu key acts on the selection; a click on what is under the cursor (mesh / outline modes:
  // their own gestures own the canvas, so only the selection).
  QString ref = selection_;
  if (e->reason() != QContextMenuEvent::Keyboard && meshMode_ == MeshOff && !outlineDrawing() && !placingPoint_) ref = pickHit(e->pos(), false).ref;
  onContextMenu(ref, e->globalPos());
}
void RigCanvas::focusOutEvent(QFocusEvent* e) {
  setSpaceHeld(false);  // the release may go to another widget
  QWidget::focusOutEvent(e);
}
// Ctrl+A (all bones) belongs to the focused canvas. Ctrl+Shift+A is deliberately left to the host:
// it is the workspace toggle, which must work from the canvas (it has the focus in the workspace);
// 자식 본까지 선택 is Ctrl+Shift+A in the hierarchy and the 선택 menu. Plugin 1.19.0: the canvas also
// claims its own keys (Space, F, Home, Esc, arrows, Delete, Ctrl+D, ?) and, during a drag, the
// digits of a typed value, so neither a panel shortcut (Space = play, , . = step) nor a host
// shortcut takes them while the canvas has the focus.
bool RigCanvas::event(QEvent* e) {
  if (e->type() == QEvent::ShortcutOverride) {
    auto* k = static_cast<QKeyEvent*>(e);
    const bool typing = drag_ && ((k->key() >= Qt::Key_0 && k->key() <= Qt::Key_9) || k->key() == Qt::Key_Minus || k->key() == Qt::Key_Period || k->key() == Qt::Key_Comma ||
                                  k->key() == Qt::Key_Semicolon || k->key() == Qt::Key_Backspace || k->key() == Qt::Key_Return || k->key() == Qt::Key_Enter);
    if (typing || !bindingFor(k->key(), k->modifiers()).isEmpty()) { e->accept(); return true; }
  }
  return QWidget::event(e);
}
QStringList RigCanvas::bonesIn(const QRectF& r) const {
  QStringList out;
  if (!rig_ || pose_.world.size() != rig_->bones.size()) return out;
  const QVector<bool> shown = shownBones();
  for (size_t i = 0; i < rig_->bones.size(); i++)
    if (shown[int(i)] && !locked_.contains("bone:" + qs(rig_->bones[i].id)) && r.contains(rigToWidget(QPointF(pose_.world[i].e, pose_.world[i].f)))) out << "bone:" + qs(rig_->bones[i].id);
  return out;
}
void RigCanvas::leaveEvent(QEvent*) {
  hoverInside_ = false;
  if (hoverVertex_ >= 0) { hoverVertex_ = -1; QToolTip::hideText(); }
  if (hoverRef_.isEmpty()) return;
  hoverRef_.clear();
  unsetCursor();
  update();
}
}  // namespace cutout_studio
