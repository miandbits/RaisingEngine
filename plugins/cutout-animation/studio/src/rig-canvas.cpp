#include "rig-canvas.hpp"
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace cutout_studio {
namespace {
constexpr double PI = 3.14159265358979323846;
const QColor SETUP_COLOR("#3b82f6"), ANIMATE_COLOR("#ef4444"), SELECT_COLOR("#ffd34d"), TRAIL_COLOR("#38bdf8"), IK_COLOR("#f97316");
// Mesh edit (plugin 1.6.0): outline edges, pinned inside edges, outline being drawn / previewed.
const QColor OUTLINE_COLOR("#f59e0b"), PINNED_COLOR("#22d3ee"), DRAFT_COLOR("#e879f9");
double normalize(double a) { while (a > PI) a -= 2 * PI; while (a < -PI) a += 2 * PI; return a; }
// Pick radii in widget px, matched to what is drawn: the IK crosshair's arms reach 12 px, so a
// press anywhere on it grabs the target (the old 7 px circle let the body image under its ring win).
constexpr double VERTEX_RADIUS = 7, IK_RADIUS = 12, BONE_RADIUS = 8, SELECTED_RADIUS = 9, AXIS_LENGTH = 18, AXIS_RADIUS = 5;
// Bone bodies (plugin 1.5.0): a press within BODY_RADIUS of the origin->tip line (or inside the
// drawn diamond) grabs the body; shorter than MIN_BODY on screen a bone is drawn as a point.
constexpr double BODY_RADIUS = 4, MIN_BODY = 6;
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
  if (meshGesture_ == MeshGesture::None) { update(); return; }
  meshCurrent_ = w;
  if ((w - meshStart_).manhattanLength() > 2) meshMoved_ = true;
  if (meshGesture_ == MeshGesture::Vertices && meshMoved_ && onMeshDrag) onMeshDrag(widgetToRig(w) - widgetToRig(meshStart_), false);
  if (meshGesture_ == MeshGesture::Paint) {
    const QVector<QPointF> pts = meshVertices();
    const int before = int(meshStroke_.size());
    for (int i = 0; i < pts.size(); i++) {
      const QPointF o = rigToWidget(pts[i]);
      if (std::hypot(o.x() - w.x(), o.y() - w.y()) <= brushRadius_) meshStroke_.insert(i);
    }
    if (int(meshStroke_.size()) != before && onMeshPaint) onMeshPaint(QVector<int>(meshStroke_.begin(), meshStroke_.end()), false);
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
    QVector<int> stroke(meshStroke_.begin(), meshStroke_.end());
    std::sort(stroke.begin(), stroke.end());
    meshStroke_.clear();
    if (!stroke.isEmpty() && onMeshPaint) onMeshPaint(stroke, true);
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
  auto heat = [](double w, int alpha) { w = std::clamp(w, 0.0, 1.0); return QColor::fromHsvF((1 - w) * 0.66, 0.85, 0.95, alpha / 255.0); };
  QVector<double> weight(pts.size(), 0.0);
  if (meshMode_ == MeshWeights && meshBone_ >= 0)
    for (int i = 0; i < pts.size(); i++) weight[i] = vertexWeight(*rig_, si, *att, size_t(i), meshBone_);
  for (size_t t = 0; t + 2 < att->triangles.size(); t += 3) {
    const int a = att->triangles[t], b = att->triangles[t + 1], c = att->triangles[t + 2];
    if (a >= pts.size() || b >= pts.size() || c >= pts.size()) continue;
    const QPolygonF tri({rigToWidget(pts[a]), rigToWidget(pts[b]), rigToWidget(pts[c])});
    if (meshMode_ == MeshWeights) {
      p.setPen(Qt::NoPen);
      p.setBrush(heat((weight[a] + weight[b] + weight[c]) / 3, 120));
      p.drawPolygon(tri);
    }
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, meshMode_ == MeshVertices ? 90 : 120), 1));
    p.drawPolygon(tri);
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
  for (int i = 0; i < pts.size(); i++) {
    const QPointF o = rigToWidget(pts[i]);
    const bool picked = meshSelected_.contains(i) || (meshGesture_ == MeshGesture::Paint && meshStroke_.contains(i));
    p.setPen(QPen(meshMode_ == MeshVertices && size_t(i) < att->hull ? OUTLINE_COLOR.darker(160) : QColor("#0b1220"), 1));
    p.setBrush(meshMode_ == MeshWeights ? heat(weight[i], 255) : picked ? SELECT_COLOR : QColor("#f8fafc"));
    p.drawEllipse(o, (picked ? 4.5 : 3.0) * ui_, (picked ? 4.5 : 3.0) * ui_);
  }
  p.setBrush(Qt::NoBrush);
  if (meshGesture_ == MeshGesture::Band && meshMoved_) {
    p.setPen(QPen(SELECT_COLOR, 1, Qt::DashLine));
    p.drawRect(QRectF(meshStart_, meshCurrent_).normalized());
  }
  if (meshMode_ == MeshWeights && rect().contains(hover_.toPoint())) {
    p.setPen(QPen(QColor(255, 255, 255, 200), 1.5, Qt::DashLine));
    p.drawEllipse(hover_, brushRadius_, brushRadius_);
  }
}

QColor RigCanvas::ikColor() { return IK_COLOR; }
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
    // From the effector: the tip bone, the lower bone's length tip, else the chain end.
    const QPointF from = k.tip >= 0 ? origin(k.tip) : k.bones.size() == 2 && k.tipLength > 0 ? rigToWidget(boneTip(k.bones.back())) : origin(k.bones.back());
    p.setPen(QPen(line, 1.5, Qt::DashLine));
    p.drawLine(from, origin(k.target));
  }
}

void RigCanvas::fitToCanvas() {
  autoFit_ = true;
  const double rw = rig_ && rig_->width > 0 ? rig_->width : 512, rh = rig_ && rig_->height > 0 ? rig_->height : 512;
  zoom_ = std::max(0.02, std::min((width() - 40) / rw, (height() - 40) / rh));
  pan_ = QPointF(width() / 2.0 - rw * zoom_ / 2, height() / 2.0 - rh * zoom_ / 2);
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
  // Bones back to front so equally near handles keep the later (drawn on top) bone first.
  for (size_t i = handles ? rig_->bones.size() : 0; i-- > 0;) {
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
  std::stable_sort(out.begin(), out.end(), [](const Hit& a, const Hit& b) { return a.rank != b.rank ? a.rank < b.rank : a.distance < b.distance; });
  return out;
}
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
  const double half = std::clamp(len * 0.1, 2.5 * ui_, 8.0 * ui_), knee = std::min(len * 0.2, half * 2.5);
  return QPolygonF({o, o + dir * knee + side * half, t, o + dir * knee - side * half});
}
QPointF RigCanvas::createEnd(const Drag& d) {
  const QPointF v = d.current - d.start;
  const double len = std::hypot(v.x(), v.y());
  if (!d.shift || len < 1e-9) return d.current;
  const double step = PI / 12, a = std::round(std::atan2(v.y(), v.x()) / step) * step;
  return d.start + QPointF(std::cos(a), std::sin(a)) * len;
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
  // Mesh modes own the left button (vertices/brush): nothing else would be picked there.
  const Hit hit = meshMode_ != MeshOff || outlineDrawing() ? Hit{} : pickHit(w, alt);
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
  if (att->mesh) {
    const QVector<QPointF> pts = meshPoints(*rig_, pose_, si);
    for (const auto& [a, b] : meshOutline(att->triangles)) if (a < pts.size() && b < pts.size()) p.drawLine(rigToWidget(pts[a]), rigToWidget(pts[b]));
    return;
  }
  p.drawPolygon((qtTransform(m) * viewTransform()).map(QPolygonF(QRectF(0, 0, att->width, att->height))));
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
  for (size_t i = 0; i < rig_->bones.size(); i++) {
    const auto& w = pose_.world[i];
    const QPointF at = rigToWidget(QPointF(w.e, w.f));
    const QString ref = "bone:" + qs(rig_->bones[i].id);
    const bool selected = isSelected(ref), hovered = ref == hoverRef_ && !drag_;
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
      p.drawEllipse(at, (selected ? 4.5 : 3.5) * ui_, (selected ? 4.5 : 3.5) * ui_);
      p.setBrush(Qt::NoBrush);
      continue;
    }
    const double len = std::hypot(w.a, w.b);
    const QPointF axis = len > 1e-9 ? QPointF(w.a / len, w.b / len) * AXIS_LENGTH * ui_ : QPointF(AXIS_LENGTH * ui_, 0);
    p.setPen(QPen(selected ? SELECT_COLOR : QColor("#c8d2dc"), (selected ? 2.5 : 1.5) * ui_));
    p.drawLine(at, at + axis);
    p.setBrush(selected ? SELECT_COLOR : color);
    p.drawEllipse(at, (selected ? 6.0 : 4.5) * ui_, (selected ? 6.0 : 4.5) * ui_);
    p.setBrush(Qt::NoBrush);
  }
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
    if (pose_.world.size() == rig_->bones.size()) {
      // Selected part outline.
      p.setPen(QPen(SELECT_COLOR, 1.5, Qt::DashLine));
      for (size_t i = 0; i < rig_->slots.size(); i++) if (isSelected("slot:" + qs(rig_->slots[i].id))) outlineSlot(p, int(i));
      paintIk(p);
      paintBones(p);
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
  }
  p.resetTransform();
  if (!message_.isEmpty()) {
    p.setPen(QColor("#f0c0c0"));
    p.drawText(rect().adjusted(16, 16, -16, -16), Qt::AlignCenter | Qt::TextWordWrap, message_);
  }
  static const char* names[] = {"이동(V)", "회전(R)", "크기(S)", "회전 중심(P)", "본 만들기(C)"};
  p.setPen(QColor("#9aa4ae"));
  QString hint = outlineDrawing() ? QString("윤곽 그리기: 그림 둘레를 차례로 클릭 · 첫 점 클릭·더블클릭·Enter로 닫기(메시 만들기) · Backspace 마지막 점 지우기 · Esc 취소")
                 : meshMode_ == MeshVertices && meshTool_ == MeshAddVertex ? QString("정점 추가: 메시 안(안쪽 정점)이나 윤곽 가까이(윤곽 정점)를 클릭 · 다시 삼각분할됩니다")
                 : meshMode_ == MeshVertices && meshTool_ == MeshAddEdge ? QString("가장자리 추가: 정점에서 다른 정점으로 끌면 그 안쪽 가장자리가 고정됩니다(하늘색) · 주황 = 윤곽")
                 : meshMode_ == MeshVertices ? QString("메시 정점: 클릭·Shift+클릭 선택 · 빈 곳 끌기 상자 선택 · 정점 끌기로 이동") + (animate_ ? " (K로 변형 키)" : " · Delete 정점 삭제")
                       : meshMode_ == MeshWeights ? QString("웨이트 칠하기: 끌어서 붓으로 칠함 · 색 = 고른 본의 웨이트(파랑 0 → 빨강 1)")
                       : tool_ == Create ? QString("본 만들기: 끌어서 고른 본의 자식 본(길이·방향) · 클릭 = 점 본 · 본 원점 클릭 = 부모 고르기 · Shift 15° 스냅")
                                         : QString("도구: %1 · 본 몸통 끌기 = 회전, 원점 끌기 = 이동 · Ctrl/Shift+클릭 여러 개 선택 · 빈 곳 끌기 상자 선택 · 끄는 중 Shift 미세 조정/15° 스냅 · Alt+클릭 겹친 항목 차례로").arg(names[std::clamp(tool_, 0, TOOL_COUNT - 1)]);
  // What a click would pick (handles beat images): named so an overlap is never a surprise.
  const QString kind = hoverRef_.startsWith("slot:") ? "조각" : rig_ && isIkTarget(rig_->boneIndex(ss(hoverRef_.section(':', 1)))) ? "IK 대상" : "본";
  if (!hoverRef_.isEmpty()) hint = QString("가리킴: %1 %2 · ").arg(kind, hoverRef_.section(':', 1)) + hint;
  const int hintHeight = fontMetrics().height() + 4;  // the hint line follows the (scaled) font
  p.drawText(QRectF(10, height() - hintHeight - 4, width() - 20, hintHeight), Qt::AlignLeft | Qt::AlignVCenter, hint + " · 휠 확대 · 가운데·오른쪽 버튼 끌기로 화면 이동");
  // Strong mode cue: the whole canvas is framed in the mode color.
  p.setPen(QPen(animate_ ? ANIMATE_COLOR : SETUP_COLOR, 4));
  p.drawRect(QRectF(rect()).adjusted(2, 2, -2, -2));
}

void RigCanvas::mousePressEvent(QMouseEvent* e) {
  setFocus(Qt::MouseFocusReason);
  pressWidget_ = e->position();
  if (e->button() == Qt::MiddleButton || e->button() == Qt::RightButton) {
    panning_ = true; panStart_ = e->position(); panOrigin_ = pan_;
    return;
  }
  if (e->button() != Qt::LeftButton) return;
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
  if (panning_) { panning_ = false; return; }
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
void RigCanvas::keyPressEvent(QKeyEvent* e) {
  if (outlineDrawing()) {
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) { finishOutline(); return; }
    if (e->key() == Qt::Key_Backspace && !outline_.isEmpty()) { outline_.removeLast(); update(); return; }
    if (e->key() == Qt::Key_Escape) { setOutlineDrawing(QString()); if (onOutlineCancel) onOutlineCancel(); return; }
  }
  if (meshMode_ == MeshVertices && (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) && onMeshDelete) { onMeshDelete(); return; }
  const int tools[] = {Qt::Key_V, Qt::Key_R, Qt::Key_S, Qt::Key_P, Qt::Key_C};
  for (int i = 0; i < TOOL_COUNT; i++) if (e->key() == tools[i] && !(e->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) { if (onTool) onTool(i); return; }
  if (e->key() == Qt::Key_A && e->modifiers() == Qt::ControlModifier && onSelectAll) { onSelectAll(); return; }
  if (e->key() == Qt::Key_K && onKey) { onKey(); return; }
  if (e->key() == Qt::Key_O && !(e->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) && onOnion) { onOnion(); return; }
  if (e->key() == Qt::Key_F) { fitToCanvas(); return; }
  if (e->key() == Qt::Key_Alt && hoverInside_) { updateHover(hoverAt_, true); return; }  // show the Alt pick at once
  QWidget::keyPressEvent(e);
}
void RigCanvas::keyReleaseEvent(QKeyEvent* e) {
  if (e->key() == Qt::Key_Alt && hoverInside_) { updateHover(hoverAt_, false); return; }
  QWidget::keyReleaseEvent(e);
}
// Ctrl+A (all bones) belongs to the focused canvas. Ctrl+Shift+A is deliberately left to the host:
// it is the workspace toggle, which must work from the canvas (it has the focus in the workspace);
// 자식 본까지 선택 is Ctrl+Shift+A in the hierarchy and the 선택 menu.
bool RigCanvas::event(QEvent* e) {
  if (e->type() == QEvent::ShortcutOverride) {
    auto* k = static_cast<QKeyEvent*>(e);
    if (k->key() == Qt::Key_A && k->modifiers() == Qt::ControlModifier) { e->accept(); return true; }
  }
  return QWidget::event(e);
}
QStringList RigCanvas::bonesIn(const QRectF& r) const {
  QStringList out;
  if (!rig_ || pose_.world.size() != rig_->bones.size()) return out;
  for (size_t i = 0; i < rig_->bones.size(); i++)
    if (r.contains(rigToWidget(QPointF(pose_.world[i].e, pose_.world[i].f)))) out << "bone:" + qs(rig_->bones[i].id);
  return out;
}
void RigCanvas::leaveEvent(QEvent*) {
  hoverInside_ = false;
  if (hoverRef_.isEmpty()) return;
  hoverRef_.clear();
  unsetCursor();
  update();
}
}  // namespace cutout_studio
