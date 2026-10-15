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
double normalize(double a) { while (a > PI) a -= 2 * PI; while (a < -PI) a += 2 * PI; return a; }
// Pick radii in widget px, matched to what is drawn: the IK crosshair's arms reach 12 px, so a
// press anywhere on it grabs the target (the old 7 px circle let the body image under its ring win).
constexpr double VERTEX_RADIUS = 7, IK_RADIUS = 12, BONE_RADIUS = 8, SELECTED_RADIUS = 9, AXIS_LENGTH = 18, AXIS_RADIUS = 5;
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
  const int v = vertexAt(e->position());
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
    p.setPen(QPen(QColor(255, 255, 255, 120), 1));
    p.drawPolygon(tri);
  }
  for (int i = 0; i < pts.size(); i++) {
    const QPointF o = rigToWidget(pts[i]);
    const bool picked = meshSelected_.contains(i) || (meshGesture_ == MeshGesture::Paint && meshStroke_.contains(i));
    p.setPen(QPen(QColor("#0b1220"), 1));
    p.setBrush(meshMode_ == MeshWeights ? heat(weight[i], 255) : picked ? SELECT_COLOR : QColor("#f8fafc"));
    p.drawEllipse(o, picked ? 4.5 : 3.0, picked ? 4.5 : 3.0);
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
      p.drawPolyline(chain);
    }
    double dx = 0, dy = 0;
    if (k.bones.size() == 1 && cutout::ikAimDirection(*rig_, pose_, int(&k - rig_->ik.data()), dx, dy)) {
      // Screen length: the target's distance, kept readable (16..60 px).
      const QPointF from = origin(k.bones[0]), aimAt = origin(k.target);
      const double len = std::clamp(std::hypot(aimAt.x() - from.x(), aimAt.y() - from.y()), 16.0, 60.0);
      const QPointF dir(dx, dy), side(-dy, dx), to = from + dir * len;
      p.setPen(QPen(line, selected ? 3 : 2, Qt::SolidLine, Qt::RoundCap));
      p.drawLine(from, to);
      p.drawLine(to, to - dir * 8 + side * 4);
      p.drawLine(to, to - dir * 8 - side * 4);
    }
    const int end = k.tip >= 0 ? k.tip : k.bones.back();
    p.setPen(QPen(line, 1.5, Qt::DashLine));
    p.drawLine(origin(end), origin(k.target));
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
      if (const double d = dist(rigToWidget(pts[i])); d <= VERTEX_RADIUS) out.push_back({"slot:" + meshSlot_, i, HitVertex, d});
  }
  // Bones back to front so equally near handles keep the later (drawn on top) bone first.
  for (size_t i = handles ? rig_->bones.size() : 0; i-- > 0;) {
    const QString ref = "bone:" + qs(rig_->bones[i].id);
    const auto& m = pose_.world[i];
    const QPointF at = rigToWidget(QPointF(m.e, m.f));
    const double d = dist(at);
    if (isIkTarget(int(i))) {
      if (d <= IK_RADIUS) out.push_back({ref, -1, HitIkTarget, d});
    } else if (ref == selection_) {
      // The selected bone's own gizmo: its origin and its drawn x axis both grab it.
      const double len = std::hypot(m.a, m.b);
      const QPointF axis = len > 1e-9 ? QPointF(m.a / len, m.b / len) * AXIS_LENGTH : QPointF(AXIS_LENGTH, 0);
      const double onAxis = segmentDistance(w, at, at + axis);
      if (d <= SELECTED_RADIUS || onAxis <= AXIS_RADIUS) out.push_back({ref, -1, HitSelectedBone, std::min(d, onAxis)});
    } else if (d <= BONE_RADIUS) {
      out.push_back({ref, -1, HitBone, d});
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
QString RigCanvas::pick(const QPointF& w, bool cycle) const {
  QStringList refs;  // distinct items in candidate order (a mesh vertex is not an item)
  for (const Hit& h : hitCandidates(w)) if (h.vertex < 0 && !refs.contains(h.ref)) refs << h.ref;
  if (refs.isEmpty()) return {};
  if (!cycle) return refs[0];
  // From the selection (or the default pick when it is not under the cursor) to the next one:
  // the first Alt+click already reaches what lies beneath, repeated ones wrap around the stack.
  return refs[(std::max(0, int(refs.indexOf(selection_))) + 1) % refs.size()];
}

void RigCanvas::updateHover(const QPointF& w, bool alt) {
  hoverAt_ = w;
  hoverAlt_ = alt;
  hoverInside_ = true;
  // Mesh modes own the left button (vertices/brush): nothing else would be picked there.
  const QString ref = meshMode_ != MeshOff ? QString() : pick(w, alt);
  if (ref == hoverRef_) return;
  hoverRef_ = ref;
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
  if (hoverRef_.isEmpty() || hoverRef_ == selection_ || drag_ || panning_) return;
  const QString id = hoverRef_.section(':', 1);
  p.setPen(QPen(QColor(255, 255, 255, 200), 1.5));
  p.setBrush(Qt::NoBrush);
  if (hoverRef_.startsWith("slot:")) {
    if (const int si = rig_->slotIndex(ss(id)); si >= 0) outlineSlot(p, si);
  } else if (const int bi = rig_->boneIndex(ss(id)); bi >= 0) {
    const auto& m = pose_.world[size_t(bi)];
    const double r = isIkTarget(bi) ? IK_RADIUS + 2 : BONE_RADIUS + 1;
    p.drawEllipse(rigToWidget(QPointF(m.e, m.f)), r, r);
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
      for (const QPointF& k : trailKeys_) p.drawEllipse(rigToWidget(k), 3.5, 3.5);
      p.setBrush(Qt::NoBrush);
    }
    // Instance anchor (rig origin): where the character stands on screen.
    const QPointF o = rigToWidget(QPointF(rig_->originX, rig_->originY));
    p.setPen(QPen(QColor("#8fd18f"), 1, Qt::DashLine));
    p.drawLine(o - QPointF(12, 0), o + QPointF(12, 0));
    p.drawLine(o - QPointF(0, 12), o + QPointF(0, 12));
    if (pose_.world.size() == rig_->bones.size()) {
      // Selected part outline.
      p.setPen(QPen(SELECT_COLOR, 1.5, Qt::DashLine));
      for (size_t i = 0; i < rig_->slots.size(); i++) if ("slot:" + qs(rig_->slots[i].id) == selection_) outlineSlot(p, int(i));
      paintIk(p);
      // Bone gizmos: link to parent, origin circle (= rotation center) and local x axis.
      for (size_t i = 0; i < rig_->bones.size(); i++) {
        const auto& w = pose_.world[i];
        const QPointF at = rigToWidget(QPointF(w.e, w.f));
        const bool selected = "bone:" + qs(rig_->bones[i].id) == selection_;
        const int parent = rig_->bones[i].parentIndex;
        if (parent >= 0) {
          p.setPen(QPen(QColor(255, 255, 255, 90), 1));
          p.drawLine(rigToWidget(QPointF(pose_.world[size_t(parent)].e, pose_.world[size_t(parent)].f)), at);
        }
        if (isIkTarget(int(i))) {
          // IK target: a crosshair (no axis: only its origin matters) that reads as "drag me".
          p.setPen(QPen(selected ? SELECT_COLOR : IK_COLOR, selected ? 2.5 : 2));
          p.drawEllipse(at, 8.0, 8.0);
          p.drawLine(at - QPointF(12, 0), at + QPointF(12, 0));
          p.drawLine(at - QPointF(0, 12), at + QPointF(0, 12));
          continue;
        }
        const double len = std::hypot(w.a, w.b);
        const QPointF axis = len > 1e-9 ? QPointF(w.a / len, w.b / len) * 18 : QPointF(18, 0);
        p.setPen(QPen(selected ? SELECT_COLOR : QColor("#c8d2dc"), selected ? 2.5 : 1.5));
        p.drawLine(at, at + axis);
        p.setBrush(selected ? SELECT_COLOR : QColor(animate_ ? "#f3a3a3" : "#9cc3ff"));
        p.drawEllipse(at, selected ? 6.0 : 4.5, selected ? 6.0 : 4.5);
        p.setBrush(Qt::NoBrush);
      }
    }
    if (pose_.world.size() == rig_->bones.size()) paintHover(p);
    paintMesh(p);
    if (drag_ && drag_->moved && (drag_->tool == Rotate || drag_->tool == Scale)) {
      p.setPen(QPen(SELECT_COLOR, 1, Qt::DotLine));
      p.drawLine(rigToWidget(drag_->origin), rigToWidget(drag_->current));
    }
    if (tool_ == Pivot && drag_) {
      const QPointF c = rigToWidget(drag_->current);
      p.setPen(QPen(SELECT_COLOR, 2));
      p.drawEllipse(c, 8, 8);
      p.drawLine(c - QPointF(12, 0), c + QPointF(12, 0));
      p.drawLine(c - QPointF(0, 12), c + QPointF(0, 12));
    }
  }
  p.resetTransform();
  if (!message_.isEmpty()) {
    p.setPen(QColor("#f0c0c0"));
    p.drawText(rect().adjusted(16, 16, -16, -16), Qt::AlignCenter | Qt::TextWordWrap, message_);
  }
  static const char* names[] = {"이동(V)", "회전(R)", "크기(S)", "회전 중심(P)"};
  p.setPen(QColor("#9aa4ae"));
  QString hint = meshMode_ == MeshVertices ? QString("메시 정점: 클릭·Shift+클릭 선택 · 빈 곳 끌기 상자 선택 · 정점 끌기로 이동") + (animate_ ? " (K로 변형 키)" : "")
                       : meshMode_ == MeshWeights ? QString("웨이트 칠하기: 끌어서 붓으로 칠함 · 색 = 고른 본의 웨이트(파랑 0 → 빨강 1)")
                                                  : QString("도구: %1 · Shift 미세 조정/15° 스냅 · Alt+클릭 겹친 항목 차례로").arg(names[tool_ & 3]);
  // What a click would pick (handles beat images): named so an overlap is never a surprise.
  const QString kind = hoverRef_.startsWith("slot:") ? "조각" : rig_ && isIkTarget(rig_->boneIndex(ss(hoverRef_.section(':', 1)))) ? "IK 대상" : "본";
  if (!hoverRef_.isEmpty()) hint = QString("가리킴: %1 %2 · ").arg(kind, hoverRef_.section(':', 1)) + hint;
  p.drawText(QRectF(10, height() - 24, width() - 20, 20), Qt::AlignLeft | Qt::AlignVCenter, hint + " · 휠 확대 · 가운데·오른쪽 버튼 끌기로 화면 이동");
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
  if (meshPress(e)) return;
  // One pick for every tool: handles first, Alt = the next candidate under the cursor.
  const QString hit = pick(e->position(), e->modifiers() & Qt::AltModifier);
  // Move picks what is under the cursor; rotate/scale keep working on the selection unless a
  // bone handle is clicked, so a drag that starts over another image still turns the chosen bone.
  QString target = selection_;
  if (tool_ == Move) target = hit;
  else if (tool_ != Pivot && !hit.isEmpty() && (hit.startsWith("bone:") || selection_.isEmpty())) target = hit;
  if (target != selection_ && onSelect) onSelect(target);
  if (tool_ == Pivot && selection_.isEmpty() && !hit.isEmpty()) return;  // first click only selects
  if (target.isEmpty()) {
    panning_ = true; panStart_ = e->position(); panOrigin_ = pan_;
    return;
  }
  Drag d;
  d.ref = target; d.tool = tool_;
  d.start = d.current = widgetToRig(e->position());
  d.origin = originOf ? originOf(target) : d.start;
  d.shift = e->modifiers() & Qt::ShiftModifier;
  lastAngle_ = std::atan2(d.start.y() - d.origin.y(), d.start.x() - d.origin.x());
  drag_ = d;
  update();
}
void RigCanvas::mouseMoveEvent(QMouseEvent* e) {
  if (panning_) {
    pan_ = panOrigin_ + (e->position() - panStart_);
    autoFit_ = false;
    update();
    return;
  }
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
  if (panning_) { panning_ = false; return; }
  if (meshGesture_ != MeshGesture::None) { meshRelease(e->position()); return; }
  if (!drag_) return;
  Drag d = *drag_;
  d.current = widgetToRig(e->position());
  drag_.reset();
  // Pivot placement is a click; other tools commit only real drags (a click just selects).
  if ((d.moved || d.tool == Pivot) && onDrag) onDrag(d, true);
  updateHover(e->position(), e->modifiers() & Qt::AltModifier);
  update();
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
  const int tools[] = {Qt::Key_V, Qt::Key_R, Qt::Key_S, Qt::Key_P};
  for (int i = 0; i < 4; i++) if (e->key() == tools[i] && !(e->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) { if (onTool) onTool(i); return; }
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
void RigCanvas::leaveEvent(QEvent*) {
  hoverInside_ = false;
  if (hoverRef_.isEmpty()) return;
  hoverRef_.clear();
  unsetCursor();
  update();
}
}  // namespace cutout_studio
