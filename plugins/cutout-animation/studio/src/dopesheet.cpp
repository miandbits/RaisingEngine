#include "dopesheet.hpp"
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

namespace cutout_studio {
namespace timeline {
int timeToX(int width, int duration, double ms) { return LABEL + 8 + int(std::lround(ms * (width - LABEL - RIGHT) / double(std::max(1, duration)))); }
double xToTime(int width, int duration, double x) { return (x - LABEL - 8) * double(std::max(1, duration)) / std::max(1, width - LABEL - RIGHT); }
int snap(double ms, bool fine, int duration) {
  const int step = fine ? 1 : 10;
  return std::clamp(int(std::lround(ms / step)) * step, 0, std::max(1, duration));
}
void paintRuler(QPainter& p, int width, int duration) {
  duration = std::max(1, duration);
  p.fillRect(QRect(0, 0, width, RULER), QColor("#2a3038"));
  const double pxPerMs = double(width - LABEL - RIGHT) / duration;
  int tick = 10;
  for (int c : {10, 50, 100, 250, 500, 1000, 2000, 5000, 10000}) { tick = c; if (c * pxPerMs >= 48) break; }
  p.setPen(QColor("#8a949e"));
  for (int t = 0; t <= duration; t += tick) {
    const int x = timeToX(width, duration, t);
    p.drawLine(x, RULER - 6, x, RULER);
    p.drawText(QRect(x + 2, 2, 60, RULER - 6), Qt::AlignLeft | Qt::AlignVCenter, QString::number(t));
  }
}
}  // namespace timeline

const QVector<InterpPreset>& interpPresets() {
  static const QVector<InterpPreset> presets = {
    {"linear", "선형", "Linear", "일정한 속도로 다음 키까지 변합니다."},
    {"step", "계단", "Step", "다음 키까지 값을 유지하다가 한 번에 바뀝니다."},
    {"easeIn", "천천히 시작", "EaseIn", "느리게 출발해 다음 키 쪽에서 빨라집니다."},
    {"easeOut", "천천히 끝", "EaseOut", "빠르게 출발해 다음 키에 부드럽게 멈춥니다."},
    {"easeInOut", "천천히 시작·끝", "EaseInOut", "양 끝이 느리고 가운데가 빠릅니다."},
    {"auto", "부드럽게", "Smooth", "앞뒤 키를 보고 이어지는 곡선을 만듭니다. 꼭대기·바닥 키에서는 넘치지 않습니다."},
    {"back", "튕김", "Back", "다음 키를 살짝 지나쳤다가 돌아옵니다."},
  };
  return presets;
}

Dopesheet::Dopesheet(QWidget* parent) : QWidget(parent) {
  setObjectName("cutoutDopesheet");
  setFocusPolicy(Qt::StrongFocus);
  setMinimumHeight(RULER + ROW * 3);
}
void Dopesheet::setRows(const QVector<Row>& rows, int durationMs) {
  rows_ = rows;
  duration_ = std::max(1, durationMs);
  if (selRow_ >= rows_.size() || (selRow_ >= 0 && !rows_[selRow_].times.contains(selTime_))) selRow_ = selTime_ = -1;
  setMinimumHeight(RULER + ROW * std::max<int>(3, int(rows_.size())) + 6);
  update();
}
int Dopesheet::timeToX(double ms) const { return timeline::timeToX(width(), duration_, ms); }
double Dopesheet::xToTime(double x) const { return timeline::xToTime(width(), duration_, x); }
int Dopesheet::rowCenterY(int row) const { return RULER + row * ROW + ROW / 2; }
int Dopesheet::rowAt(int y) const { const int r = (y - RULER) / ROW; return y < RULER || r >= rows_.size() ? -1 : r; }
int Dopesheet::snap(double ms, bool fine) const { return timeline::snap(ms, fine, duration_); }
int Dopesheet::keyAt(const QPoint& p, int* row) const {
  const int r = rowAt(p.y());
  *row = r;
  if (r < 0) return -1;
  for (int t : rows_[r].times) if (std::abs(timeToX(t) - p.x()) <= 6) return t;
  return -1;
}

// Segments show their interpolation: straight = linear, dotted = step, and a small teal
// curve for bezier drawn from the real normalized handles (shape only, not direction).
void Dopesheet::paintSegments(QPainter& p, const Row& row, int r, int y) const {
  QVector<int> times = row.times;
  if (dragMode_ == DragMode::Key && r == dragRow_)
    for (int& t : times) if (t == dragFrom_) t = dragTo_;
  const double mid = y + ROW / 2.0, half = ROW / 2.0 - 4;
  for (int i = 0; i + 1 < times.size(); i++) {
    const double x0 = timeToX(times[i]), x1 = timeToX(times[i + 1]);
    if (x1 - x0 < 3) continue;
    const QString interp = row.attachment ? "step" : row.interps.value(i, row.step.value(i) ? "step" : "linear");
    if (interp == "bezier") {
      const cutout::Curve c = row.curves.value(i, cutout::LINEAR_CURVE);
      QPainterPath path;
      const int n = std::clamp(int((x1 - x0) / 3), 4, 64);
      for (int k = 0; k <= n; k++) {
        double u = 0, v = 0;
        cutout::curvePoint(c, double(k) / n, u, v);
        const QPointF pt(x0 + (x1 - x0) * u, mid + half - 2 * half * std::clamp(v, -0.2, 1.2));
        if (k == 0) path.moveTo(pt); else path.lineTo(pt);
      }
      p.setPen(QPen(QColor("#2dd4bf"), 1.6));
      p.setBrush(Qt::NoBrush);
      p.drawPath(path);
    } else if (interp == "step") {
      p.setPen(QPen(QColor(row.attachment ? "#8f78b8" : "#c58a3a"), 1, Qt::DotLine));
      p.drawLine(QPointF(x0, mid), QPointF(x1, mid));
    } else {
      p.setPen(QPen(QColor("#5f6e7c"), 1.4));
      p.drawLine(QPointF(x0, mid), QPointF(x1, mid));
    }
  }
}

void Dopesheet::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), QColor("#1f242a"));
  timeline::paintRuler(p, width(), duration_);
  for (int r = 0; r < rows_.size(); r++) {
    const int y = RULER + r * ROW;
    p.fillRect(QRect(0, y, width(), ROW), r % 2 ? QColor("#22282f") : QColor("#262c33"));
    p.setPen(QColor("#d0d6dc"));
    p.drawText(QRect(6, y, LABEL - 10, ROW), Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(rows_[r].label, Qt::ElideMiddle, LABEL - 10));
    p.setRenderHint(QPainter::Antialiasing, true);
    paintSegments(p, rows_[r], r, y);
    for (int i = 0; i < rows_[r].times.size(); i++) {
      int t = rows_[r].times[i];
      const bool dragged = dragMode_ == DragMode::Key && r == dragRow_ && t == dragFrom_;
      if (dragged) t = dragTo_;
      const QPointF c(timeToX(t), y + ROW / 2.0);
      const bool selected = r == selRow_ && rows_[r].times[i] == selTime_;
      const bool bezier = !rows_[r].attachment && rows_[r].interps.value(i) == "bezier";
      QColor fill = rows_[r].attachment ? QColor("#c49bff") : rows_[r].step.value(i) ? QColor("#f0b35a") : bezier ? QColor("#5eead4") : QColor("#9fd0ff");
      if (selected) fill = QColor("#ffd34d");
      p.setBrush(fill);
      p.setPen(QPen(QColor("#111"), 1));
      if (rows_[r].step.value(i) || rows_[r].attachment) p.drawRect(QRectF(c.x() - 4.5, c.y() - 4.5, 9, 9));  // step = square
      else if (bezier) p.drawEllipse(c, 5.5, 5.5);                                                          // eased = round
      else p.drawPolygon(QPolygonF({c + QPointF(0, -6), c + QPointF(6, 0), c + QPointF(0, 6), c + QPointF(-6, 0)}));
    }
    p.setRenderHint(QPainter::Antialiasing, false);
  }
  if (rows_.isEmpty()) {
    p.setPen(QColor("#8a949e"));
    p.drawText(rect().adjusted(LABEL, RULER, 0, 0), Qt::AlignCenter, "키가 없습니다. 캔버스에서 포즈를 바꾸고 K(키 추가)를 누르세요.");
  }
  p.setPen(QColor("#56606c"));
  p.drawLine(LABEL, 0, LABEL, height());
  const int x = timeToX(playhead_);
  p.setPen(QPen(QColor("#ef4444"), 2));
  p.drawLine(x, 0, x, height());
}

void Dopesheet::mousePressEvent(QMouseEvent* e) {
  setFocus(Qt::MouseFocusReason);
  if (e->button() != Qt::LeftButton || e->position().x() < LABEL) {
    const int r = rowAt(int(e->position().y()));
    if (e->button() == Qt::LeftButton && r >= 0 && onRowSelected) onRowSelected(r);
    return;
  }
  int row = -1;
  const int t = keyAt(e->position().toPoint(), &row);
  if (t >= 0) {
    selRow_ = row; selTime_ = t;
    dragMode_ = DragMode::Key; dragRow_ = row; dragFrom_ = dragTo_ = t; dragMoved_ = false;
    if (onRowSelected) onRowSelected(row);
  } else {
    // Scrubbing samples poses only; it never fires clip events.
    dragMode_ = DragMode::Scrub;
    if (onScrub) onScrub(snap(xToTime(e->position().x()), e->modifiers() & Qt::ShiftModifier));
  }
  update();
}
void Dopesheet::mouseMoveEvent(QMouseEvent* e) {
  const bool fine = e->modifiers() & Qt::ShiftModifier;
  if (dragMode_ == DragMode::Scrub && onScrub) onScrub(snap(xToTime(e->position().x()), fine));
  if (dragMode_ == DragMode::Key) {
    const int to = snap(xToTime(e->position().x()), fine);
    dragMoved_ |= to != dragFrom_;
    dragTo_ = to;
    update();
  }
}
void Dopesheet::mouseReleaseEvent(QMouseEvent*) {
  const DragMode mode = dragMode_;
  dragMode_ = DragMode::None;
  if (mode == DragMode::Key && dragMoved_ && dragTo_ != dragFrom_ && onMoveKey) {
    const int row = dragRow_, from = dragFrom_, to = dragTo_;
    if (onMoveKey(row, from, to)) { selRow_ = row; selTime_ = to; }
  }
  update();
}
void Dopesheet::keyPressEvent(QKeyEvent* e) {
  if ((e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) && selRow_ >= 0 && onDeleteKey) {
    const int row = selRow_, t = selTime_;
    if (onDeleteKey(row, t)) selRow_ = selTime_ = -1;
    update();
    return;
  }
  QWidget::keyPressEvent(e);
}
void Dopesheet::contextMenuEvent(QContextMenuEvent* e) {
  int row = -1;
  const int t = keyAt(e->pos(), &row);
  if (t < 0) return;
  selRow_ = row; selTime_ = t;
  update();
  QMenu menu(this);
  menu.setObjectName("cutoutKeyMenu");
  QMap<QAction*, QString> choices;
  for (const auto& preset : interpPresets()) {
    QAction* a = menu.addAction(QString("%1 · %2").arg(preset.label, preset.tip));
    a->setObjectName(QString("cutoutInterp") + preset.suffix);
    a->setEnabled(!rows_[row].attachment);  // attachment keys always switch (step)
    choices.insert(a, preset.id);
    if (QString(preset.id) == "step") menu.addSeparator();
  }
  menu.addSeparator();
  auto del = menu.addAction("키 삭제 (Delete)");
  del->setObjectName("cutoutKeyDelete");
  QAction* chosen = menu.exec(e->globalPos());
  if (chosen && choices.contains(chosen) && onInterp) onInterp(row, t, choices.value(chosen));
  else if (chosen == del && onDeleteKey) onDeleteKey(row, t);
}
}  // namespace cutout_studio
