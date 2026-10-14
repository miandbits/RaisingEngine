#include "dopesheet.hpp"
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <cmath>

namespace cutout_studio {
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
int Dopesheet::timeToX(double ms) const { return LABEL + 8 + int(std::lround(ms * (width() - LABEL - 20) / double(duration_))); }
double Dopesheet::xToTime(double x) const { return (x - LABEL - 8) * double(duration_) / std::max(1, width() - LABEL - 20); }
int Dopesheet::rowCenterY(int row) const { return RULER + row * ROW + ROW / 2; }
int Dopesheet::rowAt(int y) const { const int r = (y - RULER) / ROW; return y < RULER || r >= rows_.size() ? -1 : r; }
int Dopesheet::snap(double ms, bool fine) const {
  // 10 ms grid by default keeps keys on readable times; Shift gives single-ms placement.
  const int step = fine ? 1 : 10;
  return std::clamp(int(std::lround(ms / step)) * step, 0, duration_);
}
int Dopesheet::keyAt(const QPoint& p, int* row) const {
  const int r = rowAt(p.y());
  *row = r;
  if (r < 0) return -1;
  for (int t : rows_[r].times) if (std::abs(timeToX(t) - p.x()) <= 6) return t;
  return -1;
}

void Dopesheet::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), QColor("#1f242a"));
  p.fillRect(QRect(0, 0, width(), RULER), QColor("#2a3038"));
  // Ruler: adaptive tick spacing so labels never overlap.
  const double pxPerMs = double(width() - LABEL - 20) / duration_;
  int tick = 10;
  for (int c : {10, 50, 100, 250, 500, 1000, 2000, 5000, 10000}) { tick = c; if (c * pxPerMs >= 48) break; }
  p.setPen(QColor("#8a949e"));
  for (int t = 0; t <= duration_; t += tick) {
    const int x = timeToX(t);
    p.drawLine(x, RULER - 6, x, RULER);
    p.drawText(QRect(x + 2, 2, 60, RULER - 6), Qt::AlignLeft | Qt::AlignVCenter, QString::number(t));
  }
  for (int r = 0; r < rows_.size(); r++) {
    const int y = RULER + r * ROW;
    p.fillRect(QRect(0, y, width(), ROW), r % 2 ? QColor("#22282f") : QColor("#262c33"));
    p.setPen(QColor("#d0d6dc"));
    p.drawText(QRect(6, y, LABEL - 10, ROW), Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(rows_[r].label, Qt::ElideMiddle, LABEL - 10));
    p.setRenderHint(QPainter::Antialiasing, true);
    for (int i = 0; i < rows_[r].times.size(); i++) {
      int t = rows_[r].times[i];
      const bool dragged = dragMode_ == DragMode::Key && r == dragRow_ && t == dragFrom_;
      if (dragged) t = dragTo_;
      const QPointF c(timeToX(t), y + ROW / 2.0);
      const bool selected = r == selRow_ && rows_[r].times[i] == selTime_;
      QColor fill = rows_[r].attachment ? QColor("#c49bff") : rows_[r].step.value(i) ? QColor("#f0b35a") : QColor("#9fd0ff");
      if (selected) fill = QColor("#ffd34d");
      p.setBrush(fill);
      p.setPen(QPen(QColor("#111"), 1));
      if (rows_[r].step.value(i) || rows_[r].attachment) p.drawRect(QRectF(c.x() - 4.5, c.y() - 4.5, 9, 9));  // step = square
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
  auto step = menu.addAction("계단 (step) · 다음 키까지 값 유지");
  step->setObjectName("cutoutInterpStep");
  auto linear = menu.addAction("선형 (linear) · 부드럽게 변화");
  linear->setObjectName("cutoutInterpLinear");
  step->setEnabled(!rows_[row].attachment);
  linear->setEnabled(!rows_[row].attachment);
  menu.addSeparator();
  auto del = menu.addAction("키 삭제 (Delete)");
  del->setObjectName("cutoutKeyDelete");
  QAction* chosen = menu.exec(e->globalPos());
  if (chosen == step && onInterp) onInterp(row, t, "step");
  else if (chosen == linear && onInterp) onInterp(row, t, "linear");
  else if (chosen == del && onDeleteKey) onDeleteKey(row, t);
}
}  // namespace cutout_studio
