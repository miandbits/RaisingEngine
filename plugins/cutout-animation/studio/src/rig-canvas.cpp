#include "rig-canvas.hpp"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <cmath>

namespace cutout_studio {
namespace {
constexpr double PI = 3.14159265358979323846;
const QColor SETUP_COLOR("#3b82f6"), ANIMATE_COLOR("#ef4444"), SELECT_COLOR("#ffd34d");
double normalize(double a) { while (a > PI) a -= 2 * PI; while (a < -PI) a += 2 * PI; return a; }
}  // namespace

RigCanvas::RigCanvas(QWidget* parent) : QWidget(parent) {
  setObjectName("cutoutCanvas");
  setMinimumSize(320, 260);
  setMouseTracking(false);
  setFocusPolicy(Qt::StrongFocus);
  setAttribute(Qt::WA_OpaquePaintEvent);
}

void RigCanvas::setScene(std::shared_ptr<const cutout::Rig> rig, const cutout::Pose& pose) {
  const bool changedRig = !rig_ || !rig || rig_->id != rig->id;
  rig_ = std::move(rig);
  pose_ = pose;
  if (changedRig && autoFit_) fitToCanvas();
  update();
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

QString RigCanvas::hitTest(const QPointF& w) const {
  if (!rig_ || pose_.world.size() != rig_->bones.size()) return {};
  // Bone handles win over images so a bone buried under its part stays selectable.
  for (size_t i = rig_->bones.size(); i-- > 0;) {
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
    if (images_ && pose_.world.size() == rig_->bones.size()) drawRig(p, *rig_, pose_, *images_, hidden_);
    p.resetTransform();
    p.setRenderHint(QPainter::Antialiasing, true);
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
void RigCanvas::keyPressEvent(QKeyEvent* e) {
  const int tools[] = {Qt::Key_V, Qt::Key_R, Qt::Key_S, Qt::Key_P};
  for (int i = 0; i < 4; i++) if (e->key() == tools[i] && !(e->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) { if (onTool) onTool(i); return; }
  if (e->key() == Qt::Key_K && onKey) { onKey(); return; }
  if (e->key() == Qt::Key_F) { fitToCanvas(); return; }
  QWidget::keyPressEvent(e);
}
}  // namespace cutout_studio
