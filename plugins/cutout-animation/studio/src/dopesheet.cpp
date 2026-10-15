#include "dopesheet.hpp"
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <climits>
#include <cmath>

namespace cutout_studio {
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
  setTimeView(new TimeView(this));
  setMinimumHeight(rowTop(3));
}
void Dopesheet::setTimeView(TimeView* view) {
  if (view_) disconnect(view_, nullptr, this, nullptr);
  view_ = view;
  connect(view_, &TimeView::changed, this, qOverload<>(&QWidget::update));
  view_->addView(this);
  update();
}
void Dopesheet::setRows(const QVector<Row>& rows, int durationMs) {
  rows_ = rows;
  drag_ = Drag::None;
  view_->setDuration(durationMs);
  const QVector<KeyRef> all = allKeys();
  view_->pruneSelection(QSet<KeyRef>(all.begin(), all.end()));
  setMinimumHeight(rowTop(std::max<int>(3, int(rows_.size()))) + 6);
  update();
}
int Dopesheet::selectedRow() const {
  const auto& sel = view_->selection();
  return sel.isEmpty() ? -1 : rowOf(sel.front().target, sel.front().property);
}
int Dopesheet::selectedTime() const { return selectedRow() < 0 ? -1 : view_->selection().front().timeMs; }
void Dopesheet::selectKey(int row, int ms) {
  if (row < 0 || row >= rows_.size()) view_->clearSelection(); else view_->setSelection({refOf(row, ms)});
}
KeyRef Dopesheet::refOf(int row, int ms) const {
  const int i = int(rows_[row].times.indexOf(ms));
  return i >= 0 ? keyRef(row, i) : KeyRef{rows_[row].target, rows_[row].property, ms};
}
int Dopesheet::eventRow() const {
  for (int i = 0; i < rows_.size(); i++) if (rows_[i].events) return i;
  return -1;
}
QVector<KeyRef> Dopesheet::rowKeys(int row) const {
  QVector<KeyRef> out;
  if (row >= 0 && row < rows_.size()) for (int i = 0; i < rows_[row].times.size(); i++) out << keyRef(row, i);
  return out;
}
QVector<KeyRef> Dopesheet::keysAt(int ms) const {
  QVector<KeyRef> out;
  for (int r = 0; r < rows_.size(); r++)
    for (int i = 0; i < rows_[r].times.size(); i++) if (rows_[r].times[i] == ms) out << keyRef(r, i);
  return out;
}
QVector<KeyRef> Dopesheet::allKeys() const {
  QVector<KeyRef> out;
  for (int r = 0; r < rows_.size(); r++) out << rowKeys(r);
  return out;
}
int Dopesheet::rowOf(const QString& target, const QString& property) const {
  if (target == "event") return eventRow();  // one row for every event name
  for (int i = 0; i < rows_.size(); i++) if (rows_[i].target == target && rows_[i].property == property) return i;
  return -1;
}
int Dopesheet::timeToX(double ms) const { return int(std::lround(view_->timeToX(ms))); }
int Dopesheet::rowCenterY(int row) const { return rowTop(row) + rowH() / 2; }
int Dopesheet::rowAt(int y) const {
  const int r = (y - ruler() - strip()) / rowH();
  return y < headerTop() + ruler() + strip() || r >= rows_.size() ? -1 : r;
}
QVector<QVector<int>> Dopesheet::groups(const QVector<int>& times) const {
  // Keys closer than clusterPx() would draw on the same pixels: they become one cluster marker.
  QVector<QVector<int>> out;
  for (int i = 0; i < times.size(); i++) {
    if (!out.isEmpty() && view_->timeToX(times[i]) - view_->timeToX(times[out.back().back()]) < clusterPx()) out.back() << i;
    else out.push_back({i});
  }
  return out;
}
QVector<QVector<int>> Dopesheet::clusters(int row) const { return row >= 0 && row < rows_.size() ? groups(rows_[row].times) : QVector<QVector<int>>{}; }
Dopesheet::Hit Dopesheet::hitAt(QPointF p) const {
  const int r = rowAt(int(p.y()));
  if (r < 0 || p.x() < label()) return {};
  const auto& times = rows_[r].times;
  for (const auto& g : groups(times)) {
    const double x0 = view_->timeToX(times[g.front()]), x1 = view_->timeToX(times[g.back()]);
    if (p.x() < x0 - 6 * ui() || p.x() > x1 + 6 * ui()) continue;
    return Hit{r, g};
  }
  return {};
}
KeyRef Dopesheet::pickInCluster(const Hit& hit) const {
  // Clicking a cluster cycles through its keys, so keys a few ms apart stay separately
  // selectable at any zoom; with several already selected the click grabs the selection.
  QVector<int> selected;
  for (int i = 0; i < hit.keys.size(); i++) if (view_->isSelected(keyRef(hit.row, hit.keys[i]))) selected << i;
  if (hit.keys.size() == 1 || selected.isEmpty()) return keyRef(hit.row, hit.keys.front());
  if (selected.size() == 1) return keyRef(hit.row, hit.keys[(selected.front() + 1) % hit.keys.size()]);
  return keyRef(hit.row, hit.keys[selected.front()]);
}
bool Dopesheet::selectionExtent(int& lo, int& hi) const {
  lo = INT_MAX; hi = INT_MIN;
  for (const auto& k : view_->selection()) { lo = std::min(lo, k.timeMs); hi = std::max(hi, k.timeMs); }
  return lo <= hi;
}
int Dopesheet::shownTime(int row, int i) const {
  const int t = rows_[row].times[i];
  if ((drag_ != Drag::Keys && drag_ != Drag::ScaleLeft && drag_ != Drag::ScaleRight) || !view_->isSelected(keyRef(row, i))) return t;
  if (drag_ == Drag::Keys) return t + delta_;
  return pivot_ + int(std::llround(double(t - pivot_) * factor_));  // same rounding as core scaleKeys
}
std::optional<QPointF> Dopesheet::scaleHandle(bool right) const {
  int lo, hi;
  if (!selectionExtent(lo, hi) || lo == hi) return std::nullopt;
  return QPointF(view_->timeToX(right ? hi : lo), headerTop() + ruler() + strip() / 2.0);
}

// Segments show their interpolation: straight = linear, dotted = step, and a small teal
// curve for bezier drawn from the real normalized handles (shape only, not direction).
void Dopesheet::paintSegments(QPainter& p, const Row& row, const QVector<int>& times, int y) const {
  const double mid = y + rowH() / 2.0, half = rowH() / 2.0 - 4 * ui();
  QVector<int> order(times.size());
  for (int i = 0; i < order.size(); i++) order[i] = i;
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return times[a] < times[b]; });
  for (int s = 0; s + 1 < order.size(); s++) {
    const int i = order[s];
    const double x0 = view_->timeToX(times[i]), x1 = view_->timeToX(times[order[s + 1]]);
    if (x1 - x0 < 3 || x1 < label() || x0 > width()) continue;
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

// Event names right of their pins, cut where the next pin starts (a cluster lists its names).
void Dopesheet::paintEventLabels(QPainter& p, int r, const QVector<int>& shown, const QVector<int>& order) const {
  const Row& row = rows_[r];
  QVector<int> sorted;
  for (int i : order) sorted << shown[i];
  const auto gs = groups(sorted);
  QFont small = font();
  small.setPixelSize(int(std::lround(10 * ui())));
  p.setFont(small);
  const QFontMetrics metrics(small);
  for (int gi = 0; gi < gs.size(); gi++) {
    const auto& g = gs[gi];
    QStringList names;
    for (int k : g) if (!names.contains(row.names.value(order[k]))) names << row.names.value(order[k]);
    const double x = view_->timeToX(sorted[g.back()]) + 9 * ui();
    const double next = gi + 1 < gs.size() ? view_->timeToX(sorted[gs[gi + 1].front()]) - 8 * ui() : width();
    if (next - x < 12 * ui()) continue;
    p.setPen(QColor("#f9c6e0"));
    p.drawText(QRectF(x, rowTop(r), next - x, rowH()), Qt::AlignLeft | Qt::AlignVCenter, metrics.elidedText(names.join(", "), Qt::ElideRight, int(next - x)));
  }
  p.setFont(font());
}

void Dopesheet::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), QColor("#1f242a"));
  for (int r = 0; r < rows_.size(); r++) {
    const int y = rowTop(r);
    p.fillRect(QRect(0, y, width(), rowH()), r % 2 ? QColor("#22282f") : QColor("#262c33"));
    // Rows of the canvas/tree selection (plugin 1.7.0, shared editor selection) are marked.
    if (highlighted_.contains(rows_[r].target)) p.fillRect(QRect(0, y, label(), rowH()), QColor(255, 211, 77, 48));
    p.setPen(QColor("#d0d6dc"));
    p.drawText(QRect(6, y, label() - 10, rowH()), Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(rows_[r].label, Qt::ElideMiddle, label() - 10));
  }
  p.save();
  p.setClipRect(QRect(label() + 1, ruler() + strip(), width() - label() - 1, height()));
  if (view_->loopActive()) {
    // Outside the preview loop range is dimmed (the range itself is editor-only).
    const QColor dim(0, 0, 0, 60);
    p.fillRect(QRectF(label(), ruler() + strip(), view_->timeToX(view_->loopIn()) - label(), height()), dim);
    p.fillRect(QRectF(view_->timeToX(view_->loopOut()), ruler() + strip(), width(), height()), dim);
  }
  p.setRenderHint(QPainter::Antialiasing, true);
  QFont small = font();
  small.setPixelSize(int(std::lround(10 * ui())));
  small.setBold(true);
  const double u = ui();
  for (int r = 0; r < rows_.size(); r++) {
    const Row& row = rows_[r];
    const int y = rowTop(r);
    QVector<int> shown = row.times;
    for (int i = 0; i < shown.size(); i++) shown[i] = shownTime(r, i);
    if (!row.events) paintSegments(p, row, shown, y);
    QVector<int> order(shown.size());
    for (int i = 0; i < order.size(); i++) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return shown[a] < shown[b]; });
    QVector<int> sortedTimes;
    for (int i : order) sortedTimes << shown[i];
    for (const auto& g : groups(sortedTimes)) {
      const double cy = y + rowH() / 2.0;
      if (g.size() > 1) {
        // Cluster marker: keys too close to tell apart at this zoom, with their count.
        int selected = 0;
        for (int gi : g) selected += view_->isSelected(keyRef(r, order[gi]));
        const double x0 = view_->timeToX(sortedTimes[g.front()]) - 6 * u, x1 = view_->timeToX(sortedTimes[g.back()]) + 6 * u;
        const QRectF box(x0, cy - 6 * u, std::max(14.0 * u, x1 - x0), 12 * u);
        p.setBrush(selected ? QColor("#ffd34d") : QColor("#8fa3b8"));
        p.setPen(QPen(selected == g.size() ? QColor("#ffffff") : QColor("#111"), 1));
        p.drawRoundedRect(box, 6 * u, 6 * u);
        p.setFont(small);
        p.setPen(QColor("#111"));
        p.drawText(box, Qt::AlignCenter, selected && selected < g.size() ? QString("%1/%2").arg(selected).arg(g.size()) : QString::number(g.size()));
        p.setFont(font());
        continue;
      }
      const int i = order[g.front()];
      const QPointF c(view_->timeToX(shown[i]), cy);
      const bool selected = view_->isSelected(keyRef(r, i));
      if (row.events) {
        // Event key: a pin (stem + flag) so it never reads as a value key.
        p.setPen(QPen(selected ? QColor("#ffd34d") : QColor("#f472b6"), 1.5));
        p.drawLine(QPointF(c.x(), y + 2), QPointF(c.x(), y + rowH() - 2));
        p.setBrush(selected ? QColor("#ffd34d") : QColor("#f472b6"));
        p.setPen(QPen(selected ? QColor("#ffffff") : QColor("#111"), 1));
        p.drawPolygon(QPolygonF({QPointF(c.x(), y + 3 * u), QPointF(c.x() + 7 * u, y + 6.5 * u), QPointF(c.x(), y + 10 * u)}));
        continue;
      }
      const bool bezier = !row.attachment && row.interps.value(i) == "bezier";
      QColor fill = row.attachment ? QColor("#c49bff") : row.step.value(i) ? QColor("#f0b35a") : bezier ? QColor("#5eead4") : QColor("#9fd0ff");
      if (selected) fill = QColor("#ffd34d");
      p.setBrush(fill);
      p.setPen(QPen(selected ? QColor("#ffffff") : QColor("#111"), 1));
      if (row.step.value(i) || row.attachment) p.drawRect(QRectF(c.x() - 4.5 * u, c.y() - 4.5 * u, 9 * u, 9 * u));  // step = square
      else if (bezier) p.drawEllipse(c, 5.5 * u, 5.5 * u);                                             // eased = round
      else p.drawPolygon(QPolygonF({c + QPointF(0, -6 * u), c + QPointF(6 * u, 0), c + QPointF(0, 6 * u), c + QPointF(-6 * u, 0)}));
    }
    if (row.events) paintEventLabels(p, r, shown, order);
  }
  p.setRenderHint(QPainter::Antialiasing, false);
  if (drag_ == Drag::Band && moved_) {
    p.setPen(QPen(QColor("#ffd34d"), 1, Qt::DashLine));
    p.setBrush(QColor(255, 211, 77, 30));
    p.drawRect(band_);
  }
  p.restore();
  // Header last, at the top of the visible area: it stays usable while rows scroll.
  const int top = headerTop();
  p.translate(0, top);
  view_->paintRuler(p, width());
  // Selection strip: extent of the selection (preview included) with scale handles.
  p.fillRect(QRect(0, ruler(), width(), strip()), QColor("#191d22"));
  int lo, hi;
  if (selectionExtent(lo, hi)) {
    int slo = INT_MAX, shi = INT_MIN;
    for (const auto& k : view_->selection()) {
      const int r = rowOf(k.target, k.property);
      int at = -1;
      if (r >= 0) for (int i = 0; i < rows_[r].times.size() && at < 0; i++) if (keyRef(r, i) == k) at = i;
      const int t = at >= 0 ? shownTime(r, at) : k.timeMs;
      slo = std::min(slo, t); shi = std::max(shi, t);
    }
    p.save();
    p.setClipRect(QRect(label(), ruler(), width() - label(), strip()));
    const double x0 = view_->timeToX(slo), x1 = view_->timeToX(shi);
    p.fillRect(QRectF(x0, ruler() + 3 * ui(), std::max(2.0, x1 - x0), strip() - 6 * ui()), QColor(255, 211, 77, 110));
    if (lo != hi)
      for (double x : {x0, x1}) p.fillRect(QRectF(x - 3 * ui(), ruler() + 1, 6 * ui(), strip() - 2), QColor("#ffd34d"));
    p.restore();
  }
  p.setPen(QColor("#6b7580"));
  p.drawText(QRect(6, ruler(), label() - 10, strip()), Qt::AlignLeft | Qt::AlignVCenter,
             view_->selection().isEmpty() ? QString() : QString("선택 %1개").arg(view_->selection().size()));
  p.translate(0, -top);
  bool anyKey = false;
  for (const auto& row : rows_) anyKey |= !row.times.isEmpty();
  if (!anyKey) {
    p.setPen(QColor("#8a949e"));
    p.drawText(rect().adjusted(label(), ruler() + strip(), 0, 0), Qt::AlignCenter, "키가 없습니다. 캔버스에서 포즈를 바꾸고 K(키 추가)를 누르세요.");
  }
  p.setPen(QColor("#56606c"));
  p.drawLine(label(), 0, label(), height());
  const double x = view_->timeToX(view_->playhead());
  if (x >= label()) {
    p.setPen(QPen(QColor("#ef4444"), 2));
    p.drawLine(QPointF(x, 0), QPointF(x, height()));
  }
}

void Dopesheet::mousePressEvent(QMouseEvent* e) {
  setFocus(Qt::MouseFocusReason);
  const QPointF pos = e->position();
  pressPos_ = lastPos_ = pos;
  moved_ = false;
  pendingSingle_.reset();
  if (e->button() == Qt::MiddleButton) { drag_ = Drag::Pan; return; }
  if (e->button() != Qt::LeftButton) return;
  const bool ctrl = e->modifiers() & Qt::ControlModifier, shift = e->modifiers() & Qt::ShiftModifier, alt = e->modifiers() & Qt::AltModifier;
  pressTime10_ = view_->snap(xToTime(pos.x()), false);
  pressTime1_ = view_->snap(xToTime(pos.x()), true);
  delta_ = 0;
  factor_ = 1;
  const int top = headerTop();
  if (pos.y() < top + ruler()) {
    if (pos.x() < label()) return;
    if (const int marker = view_->loopMarkerAt(pos.x(), pos.y() - top)) { drag_ = marker == 1 ? Drag::LoopIn : Drag::LoopOut; return; }
    // Ruler: scrubbing samples poses only; it never fires clip events.
    drag_ = Drag::Scrub;
    if (onScrub) onScrub(shift ? pressTime1_ : pressTime10_);
    return;
  }
  if (pos.y() < top + ruler() + strip()) {
    int lo, hi;
    if (!selectionExtent(lo, hi)) return;
    for (bool right : {true, false})
      if (auto h = scaleHandle(right); h && std::fabs(h->x() - pos.x()) <= 6 * ui()) {
        // Scale around the opposite end (Alt: around the playhead).
        drag_ = right ? Drag::ScaleRight : Drag::ScaleLeft;
        edge_ = right ? hi : lo;
        pivot_ = alt ? view_->playhead() : right ? lo : hi;
        return;
      }
    if (pos.x() >= view_->timeToX(lo) - 4 * ui() && pos.x() <= view_->timeToX(hi) + 4 * ui()) drag_ = Drag::Keys;  // drag the strip = move all
    return;
  }
  if (pos.x() < label()) {
    // Row header: the whole row (Ctrl/Shift adds).
    const int r = rowAt(int(pos.y()));
    if (r < 0) return;
    if (ctrl || shift) view_->add(rowKeys(r)); else view_->setSelection(rowKeys(r));
    if (onRowSelected) onRowSelected(r, ctrl || shift);
    return;
  }
  const Hit hit = hitAt(pos);
  if (hit.row < 0) {
    drag_ = Drag::Band;
    bandAdd_ = ctrl || shift;
    band_ = QRectF(pos, pos);
    return;
  }
  QVector<KeyRef> refs;
  for (int i : hit.keys) refs << keyRef(hit.row, i);
  if (alt) {
    // Column: every key at that time across all rows.
    const QVector<KeyRef> column = keysAt(pickInCluster(hit).timeMs);
    if (ctrl || shift) view_->add(column); else view_->setSelection(column);
    if (onRowSelected) onRowSelected(hit.row, ctrl || shift);
    return;
  }
  if (ctrl) {
    // Toggle the key (a cluster toggles as a whole: all on unless all are already on).
    bool all = true;
    for (const auto& k : refs) all &= view_->isSelected(k);
    QVector<KeyRef> next = view_->selection();
    for (const auto& k : refs) { next.removeAll(k); if (!all) next << k; }
    view_->setSelection(next);
    return;
  }
  const KeyRef ref = pickInCluster(hit);
  int inCluster = 0;
  for (const auto& k : refs) inCluster += view_->isSelected(k);
  if (shift) view_->add({ref});
  else if (hit.keys.size() > 1 && inCluster >= 2) {}           // grab the selected cluster keys as they are
  else if (hit.keys.size() > 1 || !view_->isSelected(ref)) view_->setSelection({ref});  // cluster click cycles
  else if (view_->selection().size() > 1) pendingSingle_ = ref;  // click (no drag) on a selected key keeps only it
  drag_ = Drag::Keys;
  if (onRowSelected) onRowSelected(hit.row, ctrl || shift);
}
void Dopesheet::mouseMoveEvent(QMouseEvent* e) {
  const QPointF pos = e->position();
  const bool fine = e->modifiers() & Qt::ShiftModifier;
  const int t = view_->snap(xToTime(pos.x()), fine);
  moved_ |= std::fabs(pos.x() - pressPos_.x()) + std::fabs(pos.y() - pressPos_.y()) >= 3;
  switch (drag_) {
    case Drag::Pan: view_->panPixels(pos.x() - lastPos_.x()); break;
    case Drag::Scrub: if (onScrub) onScrub(t); break;
    case Drag::LoopIn: view_->setLoop(std::min(t, view_->loopOut() - 1), view_->loopOut()); break;
    case Drag::LoopOut: view_->setLoop(view_->loopIn(), std::max(t, view_->loopIn() + 1)); break;
    case Drag::Keys: {
      int lo, hi;
      if (!selectionExtent(lo, hi)) break;
      // Grid-snapped delta keeps relative spacing; the group stops at the clip edges.
      delta_ = std::clamp(t - (fine ? pressTime1_ : pressTime10_), -lo, view_->duration() - hi);
      update();
      break;
    }
    case Drag::ScaleLeft:
    case Drag::ScaleRight: {
      const bool right = drag_ == Drag::ScaleRight;
      const double span = right ? edge_ - pivot_ : pivot_ - edge_;
      if (span <= 0) { factor_ = 1; break; }
      const int to = right ? std::clamp(t, pivot_ + 1, view_->duration()) : std::clamp(t, 0, pivot_ - 1);
      factor_ = (right ? to - pivot_ : pivot_ - to) / span;
      update();
      break;
    }
    case Drag::Band: band_ = QRectF(pressPos_, pos).normalized(); update(); break;
    case Drag::None: break;
  }
  lastPos_ = pos;
}
void Dopesheet::mouseReleaseEvent(QMouseEvent* e) {
  const Drag mode = drag_;
  const int delta = delta_;
  const double factor = factor_;
  // Clear the preview before committing: the commit may rebuild the rows synchronously.
  drag_ = Drag::None;
  delta_ = 0;
  factor_ = 1;
  const QVector<KeyRef> keys = view_->selection();
  if (mode == Drag::Keys) {
    if (delta != 0 && onMoveKeys) onMoveKeys(keys, delta);
    else if (!moved_ && pendingSingle_) view_->setSelection({*pendingSingle_});
  } else if ((mode == Drag::ScaleLeft || mode == Drag::ScaleRight) && factor != 1 && onScaleKeys) {
    onScaleKeys(keys, pivot_, factor);
  } else if (mode == Drag::Band) {
    if (!moved_) {
      // A plain click on empty space: deselect and place the playhead there.
      if (!bandAdd_) view_->clearSelection();
      if (onScrub) onScrub(view_->snap(xToTime(e->position().x()), e->modifiers() & Qt::ShiftModifier));
    } else {
      QVector<KeyRef> inside;
      for (int r = 0; r < rows_.size(); r++) {
        if (rowTop(r) + rowH() < band_.top() || rowTop(r) > band_.bottom()) continue;
        for (int i = 0; i < rows_[r].times.size(); i++)
          if (view_->timeToX(rows_[r].times[i]) >= band_.left() - 3 && view_->timeToX(rows_[r].times[i]) <= band_.right() + 3) inside << keyRef(r, i);
      }
      if (bandAdd_) view_->add(inside); else view_->setSelection(inside);
    }
  }
  pendingSingle_.reset();
  update();
}
void Dopesheet::mouseDoubleClickEvent(QMouseEvent* e) {
  const Hit hit = hitAt(e->position());
  const int row = rowAt(int(e->position().y()));
  if (e->button() == Qt::LeftButton && row >= 0 && rows_[row].events && e->position().x() >= label()) {
    // Event row: double-click a pin edits it, empty space adds an event there.
    if (hit.row == row && hit.keys.size() == 1) { if (onEditEvent) onEditEvent(keyRef(row, hit.keys.front())); return; }
    if (hit.row < 0) { if (onAddEvent) onAddEvent(view_->snap(xToTime(e->position().x()), e->modifiers() & Qt::ShiftModifier)); return; }
  }
  if (hit.row < 0 || hit.keys.size() < 2) { mousePressEvent(e); return; }
  // Double-click a cluster: zoom in until its keys are ~24 px apart.
  const auto& times = rows_[hit.row].times;
  int gap = INT_MAX;
  for (int i = 1; i < hit.keys.size(); i++) gap = std::min(gap, times[hit.keys[i]] - times[hit.keys[i - 1]]);
  const double center = (times[hit.keys.front()] + times[hit.keys.back()]) / 2.0;
  view_->zoomAround(center, 24.0 / std::max(1, gap) / view_->pxPerMs());
  view_->setScroll(center - view_->visibleMs() / 2);
}
void Dopesheet::wheelEvent(QWheelEvent* e) {
  const QPoint a = e->angleDelta();
  if (e->modifiers() & Qt::ControlModifier) {
    view_->zoomAround(xToTime(e->position().x()), std::pow(1.25, a.y() / 120.0));
    e->accept();
    return;
  }
  // Plain wheel scrolls rows when they overflow the scroll area; otherwise (or with Shift) it pans time.
  const bool overflow = parentWidget() && height() > parentWidget()->height();
  if (!(e->modifiers() & Qt::ShiftModifier) && a.x() == 0 && overflow) { e->ignore(); return; }
  view_->panPixels((a.x() ? a.x() : a.y()) / 120.0 * 80);
  e->accept();
}
void Dopesheet::keyPressEvent(QKeyEvent* e) {
  const auto& sel = view_->selection();
  if ((e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) && !sel.isEmpty() && onDeleteKeys) { onDeleteKeys(sel); return; }
  if (e->matches(QKeySequence::SelectAll)) { view_->setSelection(allKeys()); return; }
  if (e->matches(QKeySequence::Copy) && onCommand) { onCommand("copy"); return; }
  if (e->matches(QKeySequence::Paste) && onCommand) { onCommand("paste"); return; }
  if (e->key() == Qt::Key_D && e->modifiers() == Qt::ControlModifier && onCommand) { onCommand("duplicate"); return; }
  if (e->key() == Qt::Key_Escape) { view_->clearSelection(); return; }
  if ((e->key() == Qt::Key_Left || e->key() == Qt::Key_Right) && onStep) {
    onStep((e->key() == Qt::Key_Left ? -1 : 1) * (e->modifiers() & Qt::ShiftModifier ? 10 : 1));
    return;
  }
  QWidget::keyPressEvent(e);
}
void Dopesheet::contextMenuEvent(QContextMenuEvent* e) {
  const Hit hit = hitAt(e->pos());
  const int row = hit.row;
  // Resolved before exec(): the menu runs an event loop and rows may be rebuilt meanwhile.
  const int columnMs = row >= 0 ? pickInCluster(hit).timeMs : -1;
  const bool attachmentRow = row >= 0 && rows_[row].attachment;
  const int under = rowAt(e->pos().y());
  const bool eventRowHit = under >= 0 && rows_[under].events && e->pos().x() >= label();
  const int clickMs = view_->snap(xToTime(e->pos().x()), false);
  const std::optional<KeyRef> eventKey = row >= 0 && rows_[row].events ? std::optional<KeyRef>(pickInCluster(hit)) : std::nullopt;
  if (row >= 0) {
    const KeyRef ref = pickInCluster(hit);
    if (!view_->isSelected(ref)) view_->setSelection({ref});
  }
  QMenu menu(this);
  menu.setObjectName("cutoutKeyMenu");
  QMap<QAction*, QString> choices;
  QAction *addEvent = nullptr, *editEvent = nullptr;
  if (eventRowHit) {
    // Event row: events have no interpolation; add / edit instead.
    addEvent = menu.addAction(QString("여기에 이벤트 추가 (%1ms)").arg(eventKey ? eventKey->timeMs : clickMs));
    addEvent->setObjectName("cutoutEventAddHere");
    if (eventKey) {
      editEvent = menu.addAction(QString("이벤트 「%1」 편집… (더블클릭)").arg(eventKey->property));
      editEvent->setObjectName("cutoutEventEdit");
    }
    menu.addSeparator();
  } else if (row >= 0) {
    for (const auto& preset : interpPresets()) {
      QAction* a = menu.addAction(QString("%1 · %2").arg(preset.label, preset.tip));
      a->setObjectName(QString("cutoutInterp") + preset.suffix);
      a->setEnabled(!attachmentRow);  // attachment keys always switch (step)
      choices.insert(a, preset.id);
      if (QString(preset.id) == "step") menu.addSeparator();
    }
    menu.addSeparator();
  }
  QMap<QAction*, QString> commands;
  auto command = [&](const char* text, const char* name, const char* id, bool enabled) {
    QAction* a = menu.addAction(text);
    a->setObjectName(name);
    a->setEnabled(enabled);
    commands.insert(a, id);
  };
  command("복사 (Ctrl+C)", "cutoutKeyCopy", "copy", !view_->selection().isEmpty());
  command("재생 헤드에 붙여넣기 (Ctrl+V)", "cutoutKeyPaste", "paste", true);
  command("재생 헤드에 복제 (Ctrl+D)", "cutoutKeyDuplicate", "duplicate", !view_->selection().isEmpty());
  QAction* column = row >= 0 ? menu.addAction("이 시간의 키 모두 선택 (Alt+클릭)") : nullptr;
  if (column) column->setObjectName("cutoutKeySelectColumn");
  QAction* del = row >= 0 ? menu.addAction("선택한 키 삭제 (Delete)") : nullptr;
  if (del) del->setObjectName("cutoutKeyDelete");
  update();
  QAction* chosen = menu.exec(e->globalPos());
  if (!chosen) return;
  if (addEvent && chosen == addEvent) { if (onAddEvent) onAddEvent(eventKey ? eventKey->timeMs : clickMs); }
  else if (editEvent && chosen == editEvent) { if (onEditEvent) onEditEvent(*eventKey); }
  else if (choices.contains(chosen) && onPreset) onPreset(choices.value(chosen));
  else if (commands.contains(chosen) && onCommand) onCommand(commands.value(chosen));
  else if (chosen == column) view_->setSelection(keysAt(columnMs));
  else if (chosen == del && onDeleteKeys) onDeleteKeys(view_->selection());
}
void Dopesheet::resizeEvent(QResizeEvent* e) {
  QWidget::resizeEvent(e);
  view_->viewResized();
}
}  // namespace cutout_studio
