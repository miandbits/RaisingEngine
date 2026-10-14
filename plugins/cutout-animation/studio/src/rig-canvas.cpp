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
}  // namespace

RigCanvas::RigCanvas(QWidget* parent) : QWidget(parent) {
  setObjectName("cutoutCanvas");
  setMinimumSize(200, 150);  // small: the workspace splitters must fit 1280x720 windows
  setMouseTracking(false);
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
    const cutout::Attachment* att = nullptr;
    const auto m = attachmentMatrix(*g.rig, *g.pose, int(i), &att);
    if (att) bounds |= (qtTransform(m) * view).mapRect(QRectF(0, 0, att->width, att->height));
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

QColor RigCanvas::ikColor() { return IK_COLOR; }
bool RigCanvas::isIkTarget(int bone) const {
  if (!rig_) return false;
  for (const auto& k : rig_->ik) if (k.target == bone) return true;
  return false;
}
// IK overlay: a dashed line from each chain's effector to its target (where the limb is pulled)
// and the selected constraint's chain links drawn thick, so solved bones read as "driven".
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
      if (k.bones.size() == 1) {
        const auto& w = pose_.world[size_t(k.bones[0])];
        const double len = std::hypot(w.a, w.b);
        if (len > 1e-9) chain << origin(k.bones[0]) + QPointF(w.a / len, w.b / len) * 28;
      }
      p.drawPolyline(chain);
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

QString RigCanvas::hitTest(const QPointF& w, bool bones) const {
  if (!rig_ || pose_.world.size() != rig_->bones.size()) return {};
  // Bone handles win over images so a bone buried under its part stays selectable.
  for (size_t i = bones ? rig_->bones.size() : 0; i-- > 0;) {
    const QPointF o = rigToWidget(QPointF(pose_.world[i].e, pose_.world[i].f));
    if (std::hypot(o.x() - w.x(), o.y() - w.y()) <= 7) return "bone:" + qs(rig_->bones[i].id);
  }
  const QTransform view = viewTransform();
  for (size_t i = rig_->slots.size(); i-- > 0;) {
    if (hidden_.contains(qs(rig_->slots[i].id))) continue;
    const cutout::Attachment* att = nullptr;
    const auto m = attachmentMatrix(*rig_, pose_, int(i), &att);
    if (!att) continue;
    bool invertible = false;
    const QTransform inv = (qtTransform(m) * view).inverted(&invertible);
    if (!invertible) continue;
    const QPointF local = inv.map(w);
    if (local.x() < 0 || local.y() < 0 || local.x() >= att->width || local.y() >= att->height) continue;
    // Transparent pixels do not catch clicks, so overlapping parts pick the visible one.
    if (images_) {
      const QImage img = const_cast<ImageCache*>(images_)->image(qs(att->imageAssetId));
      if (!img.isNull()) {
        const int px = int(local.x() * img.width() / att->width), py = int(local.y() * img.height() / att->height);
        if (img.valid(px, py) && qAlpha(img.pixel(px, py)) < 16) continue;
      }
    }
    return "slot:" + qs(rig_->slots[i].id);
  }
  return {};
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
      for (size_t i = 0; i < rig_->slots.size(); i++) {
        if ("slot:" + qs(rig_->slots[i].id) != selection_) continue;
        const cutout::Attachment* att = nullptr;
        const auto m = attachmentMatrix(*rig_, pose_, int(i), &att);
        if (!att) continue;
        const QTransform t = qtTransform(m) * view;
        p.setPen(QPen(SELECT_COLOR, 1.5, Qt::DashLine));
        p.drawPolygon(t.map(QPolygonF(QRectF(0, 0, att->width, att->height))));
      }
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
  p.drawText(QRectF(10, height() - 24, width() - 20, 20), Qt::AlignLeft | Qt::AlignVCenter,
             QString("도구: %1 · Shift 미세 조정/15° 스냅 · 휠 확대 · 가운데·오른쪽 버튼 끌기로 화면 이동").arg(names[tool_ & 3]));
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
  const QString hit = hitTest(e->position());
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
  if (!drag_) return;
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
  if (!drag_) return;
  Drag d = *drag_;
  d.current = widgetToRig(e->position());
  drag_.reset();
  // Pivot placement is a click; other tools commit only real drags (a click just selects).
  if ((d.moved || d.tool == Pivot) && onDrag) onDrag(d, true);
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
  QWidget::keyPressEvent(e);
}
}  // namespace cutout_studio
