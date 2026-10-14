#include "graph-editor.hpp"
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
#include "dopesheet.hpp"

namespace cutout_studio {
namespace {
constexpr int LABEL = timeline::LABEL, RULER = timeline::RULER, HIT = 7, LEGEND_ROW = 16;
// Handles snap to 0.001 and values to 0.0001: readable, diff-friendly documents.
double round3(double v) { return std::round(v * 1000) / 1000; }
double roundValue(double v) { return std::round(v * 1e4) / 1e4; }
double round6(double v) { const double r = std::round(v * 1e6) / 1e6; return r == 0 ? 0.0 : r; }
QString idOf(const QString& target, const QString& property) { return target + "#" + property; }
double niceStep(double span) {
  const double raw = span / 4, mag = std::pow(10.0, std::floor(std::log10(raw)));
  for (double m : {1.0, 2.0, 5.0, 10.0}) if (m * mag >= raw) return m * mag;
  return 10 * mag;
}
}  // namespace

GraphEditor::GraphEditor(QWidget* parent) : QWidget(parent) {
  setObjectName("cutoutGraphEditor");
  setFocusPolicy(Qt::StrongFocus);
  setMinimumHeight(70);
}

void GraphEditor::setChannels(const QVector<Channel>& channels, int durationMs) {
  channels_ = channels;
  duration_ = std::max(1, durationMs);
  drag_ = Drag::None;
  setSelectedKeys(selection_);
  fit();
  update();
}
void GraphEditor::setNormalized(bool on) {
  normalized_ = on;
  fit();
  update();
}
void GraphEditor::setSelectedKeys(const QVector<KeyRef>& keys) {
  QVector<KeyRef> keep;
  for (const auto& ref : keys) {
    const int c = channelIndex(ref.target, ref.property);
    if (c < 0 || keep.contains(ref)) continue;
    for (const auto& k : channels_[c].keys) if (k.timeMs == ref.timeMs) { keep << ref; break; }
  }
  selection_ = keep;
  update();
}
int GraphEditor::channelIndex(const QString& target, const QString& property) const {
  for (int i = 0; i < channels_.size(); i++) if (channels_[i].target == target && channels_[i].property == property) return i;
  return -1;
}

// ---------------------------------------------------------------- geometry
int GraphEditor::timeToX(double ms) const { return timeline::timeToX(width(), duration_, ms); }
double GraphEditor::xToTime(double x) const { return timeline::xToTime(width(), duration_, x); }
double GraphEditor::plotTop() const { return RULER + 10; }
double GraphEditor::plotBottom() const { return std::max(plotTop() + 20, double(height() - 12)); }
double GraphEditor::valueToY(int c, double v) const {
  const auto [lo, hi] = ranges_.value(c, {0.0, 1.0});
  return plotBottom() - (v - lo) / (hi - lo) * (plotBottom() - plotTop());
}
double GraphEditor::yToValue(int c, double y) const {
  const auto [lo, hi] = ranges_.value(c, {0.0, 1.0});
  return lo + (plotBottom() - y) / (plotBottom() - plotTop()) * (hi - lo);
}
// Exact (unrounded) x so handle drags map back to the same fraction they were drawn at.
static double timeToXF(int width, int duration, double ms) { return LABEL + 8 + ms * (width - LABEL - timeline::RIGHT) / double(std::max(1, duration)); }
QPointF GraphEditor::keyPoint(int c, int k) const {
  const Key& key = channels_[c].keys[k];
  return {timeToXF(width(), duration_, key.timeMs), valueToY(c, key.value)};
}
bool GraphEditor::segment(int c, int seg, double& t0, double& t1, double& v0, double& v1, cutout::Curve& curve) const {
  if (c < 0 || c >= channels_.size() || seg < 0 || seg + 1 >= channels_[c].keys.size()) return false;
  const Key &a = channels_[c].keys[seg], &b = channels_[c].keys[seg + 1];
  t0 = a.timeMs; t1 = b.timeMs; v0 = a.value; v1 = b.value;
  curve = a.interp == "bezier" ? a.curve : cutout::LINEAR_CURVE;
  return t1 > t0;
}
std::optional<QPointF> GraphEditor::handlePoint(int c, int k, Side side) const {
  const int seg = side == Side::Out ? k : k - 1;
  double t0, t1, v0, v1;
  cutout::Curve cv;
  if (!segment(c, seg, t0, t1, v0, v1, cv) || channels_[c].keys[seg].interp == "step") return std::nullopt;
  const int i = side == Side::Out ? 0 : 2;
  const cutout::Curve cc = cutout::clampCurve(cv);
  return QPointF(timeToXF(width(), duration_, t0 + cc[i] * (t1 - t0)), valueToY(c, v0 + cc[i + 1] * (v1 - v0)));
}
cutout::Curve GraphEditor::curveForHandle(int c, int k, Side side, QPointF pos) const {
  const int seg = side == Side::Out ? k : k - 1;
  double t0, t1, v0, v1;
  cutout::Curve out;
  if (!segment(c, seg, t0, t1, v0, v1, out)) return cutout::LINEAR_CURVE;
  const int i = side == Side::Out ? 0 : 2;
  out[i] = round3((xToTime(pos.x()) - t0) / (t1 - t0));
  // A flat segment (same values) has no value scale: keep its y handle as it was.
  if (v1 != v0) out[i + 1] = round3((yToValue(c, pos.y()) - v0) / (v1 - v0));
  for (double& v : out) v = round6(v);
  return cutout::clampCurve(out);
}

double GraphEditor::sample(const Channel& ch, double t) const {
  const auto& keys = ch.keys;
  if (keys.isEmpty()) return 0;
  if (t <= keys.front().timeMs) return keys.front().value;
  if (t >= keys.back().timeMs) return keys.back().value;
  for (int i = 0; i + 1 < keys.size(); i++) {
    if (t >= keys[i + 1].timeMs) continue;
    const Key &a = keys[i], &b = keys[i + 1];
    if (a.interp == "step") return a.value;
    const double u = (t - a.timeMs) / double(b.timeMs - a.timeMs);
    // Same evaluation as the runtime (core evalCurve), so the drawn curve is what plays.
    return a.value + (b.value - a.value) * (a.interp == "bezier" ? cutout::evalCurve(a.curve, u) : u);
  }
  return keys.back().value;
}

void GraphEditor::fit() {
  // Sample curves and handles so overshoot (back) and dragged handles stay on screen.
  QVector<std::pair<double, double>> own(channels_.size(), {0.0, 1.0});
  double glo = INFINITY, ghi = -INFINITY;
  for (int c = 0; c < channels_.size(); c++) {
    const auto& keys = channels_[c].keys;
    if (keys.isEmpty()) continue;
    double lo = INFINITY, hi = -INFINITY;
    auto add = [&](double v) { lo = std::min(lo, v); hi = std::max(hi, v); };
    for (int i = 0; i < keys.size(); i++) {
      add(keys[i].value);
      double t0, t1, v0, v1;
      cutout::Curve cv;
      if (!segment(c, i, t0, t1, v0, v1, cv)) continue;
      for (int s = 1; s < 24; s++) add(sample(channels_[c], t0 + (t1 - t0) * s / 24.0));
      if (keys[i].interp == "bezier") { add(v0 + cv[1] * (v1 - v0)); add(v0 + cv[3] * (v1 - v0)); }
    }
    own[c] = {lo, hi};
    if (visible(c)) { glo = std::min(glo, lo); ghi = std::max(ghi, hi); }
  }
  ranges_.resize(channels_.size());
  for (int c = 0; c < channels_.size(); c++) {
    auto [lo, hi] = normalized_ || !(glo <= ghi) ? own[c] : std::make_pair(glo, ghi);
    const double span = hi - lo;
    if (span < 1e-9) { const double pad = std::max(1e-3, std::fabs(lo) * 0.1); lo -= pad; hi += pad; }
    else { lo -= span * 0.08; hi += span * 0.08; }
    ranges_[c] = {lo, hi};
  }
}

bool GraphEditor::visible(int c) const { return c >= 0 && c < channels_.size() && !hidden_.contains(idOf(channels_[c].target, channels_[c].property)); }
GraphEditor::KeyRef GraphEditor::refOf(int c, int k) const { return {channels_[c].target, channels_[c].property, channels_[c].keys[k].timeMs}; }
bool GraphEditor::isSelected(int c, int k) const { return selection_.contains(refOf(c, k)); }
int GraphEditor::keyAt(QPointF p, int* channel) const {
  for (int c = int(channels_.size()) - 1; c >= 0; c--) {
    if (!visible(c)) continue;
    for (int k = 0; k < channels_[c].keys.size(); k++)
      if (QLineF(keyPoint(c, k), p).length() <= HIT) { *channel = c; return k; }
  }
  return -1;
}
bool GraphEditor::handleAt(QPointF p, int* channel, int* key, Side* side) const {
  for (const auto& ref : selection_) {
    const int c = channelIndex(ref.target, ref.property);
    if (!visible(c)) continue;
    for (int k = 0; k < channels_[c].keys.size(); k++) {
      if (channels_[c].keys[k].timeMs != ref.timeMs) continue;
      for (Side s : {Side::Out, Side::In})
        if (auto hp = handlePoint(c, k, s); hp && QLineF(*hp, p).length() <= HIT) { *channel = c; *key = k; *side = s; return true; }
    }
  }
  return false;
}
int GraphEditor::legendAt(QPointF p) const {
  if (p.x() >= 6 + legendWidth() || p.y() < RULER + 4) return -1;
  const int row = int((p.y() - RULER - 4) / LEGEND_ROW);
  return row < channels_.size() ? row : -1;
}

// ---------------------------------------------------------------- painting
void GraphEditor::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), QColor("#1f242a"));
  p.fillRect(QRectF(LABEL, plotTop(), width() - LABEL, plotBottom() - plotTop()), QColor("#1a1f24"));
  timeline::paintRuler(p, width(), duration_);
  int shared = -1;
  for (int c = 0; c < channels_.size() && shared < 0; c++) if (visible(c)) shared = c;
  if (shared >= 0 && !normalized_) {
    // Value grid of the shared range.
    const auto [lo, hi] = ranges_.value(shared);
    const double step = niceStep(hi - lo);
    p.setPen(QColor("#2c343d"));
    for (double v = std::ceil(lo / step) * step; v <= hi; v += step) {
      const double y = valueToY(shared, v);
      p.setPen(QColor("#2c343d"));
      p.drawLine(QPointF(LABEL, y), QPointF(width(), y));
      p.setPen(QColor("#7d8894"));
      p.drawText(QRectF(LABEL - 64, y - 8, 60, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(std::fabs(v) < step * 1e-6 ? 0.0 : v, 'g', 5));
    }
  }
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setClipRect(QRectF(LABEL, RULER, width() - LABEL, height() - RULER));
  for (int c = 0; c < channels_.size(); c++) {
    if (!visible(c) || channels_[c].keys.isEmpty()) continue;
    // Pixel samples plus each key time (and just before it) keep step jumps and corners sharp.
    std::vector<double> times;
    for (double x = LABEL + 8; x <= timeToX(duration_) + 0.5; x += 2) times.push_back(xToTime(x));
    for (const auto& k : channels_[c].keys) { times.push_back(k.timeMs); times.push_back(k.timeMs - 1e-6); }
    std::sort(times.begin(), times.end());
    QPainterPath path;
    bool first = true;
    for (double t : times) {
      if (t < 0 || t > duration_) continue;
      const QPointF pt(timeToXF(width(), duration_, t), valueToY(c, sample(channels_[c], t)));
      if (first) path.moveTo(pt); else path.lineTo(pt);
      first = false;
    }
    p.setPen(QPen(channels_[c].color, 2));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
  }
  for (int c = 0; c < channels_.size(); c++) {
    if (!visible(c)) continue;
    for (int k = 0; k < channels_[c].keys.size(); k++) {
      const QPointF pt = keyPoint(c, k);
      const bool sel = isSelected(c, k);
      if (sel)
        for (Side s : {Side::Out, Side::In})
          if (auto hp = handlePoint(c, k, s)) {
            p.setPen(QPen(QColor("#cbd5e1"), 1));
            p.drawLine(pt, *hp);
            p.setBrush(QColor("#0f172a"));
            p.setPen(QPen(QColor("#f8fafc"), 1.5));
            p.drawEllipse(*hp, 4, 4);
          }
      const Key& key = channels_[c].keys[k];
      p.setBrush(sel ? QColor("#ffd34d") : key.interp == "bezier" ? channels_[c].color : QColor("#1f242a"));
      p.setPen(QPen(sel ? QColor("#ffffff") : channels_[c].color, 1.5));
      const double r = sel ? 5.5 : 4.5;
      if (key.interp == "step") p.drawRect(QRectF(pt.x() - r, pt.y() - r, 2 * r, 2 * r));
      else if (key.interp == "bezier") p.drawEllipse(pt, r, r);
      else p.drawPolygon(QPolygonF({pt + QPointF(0, -r - 1), pt + QPointF(r + 1, 0), pt + QPointF(0, r + 1), pt + QPointF(-r - 1, 0)}));
    }
  }
  p.setClipping(false);
  p.setRenderHint(QPainter::Antialiasing, false);
  // Legend: click toggles a channel (editor-only).
  for (int c = 0; c < channels_.size(); c++) {
    const QRectF row(6, RULER + 4 + c * LEGEND_ROW, legendWidth(), LEGEND_ROW), label = row.adjusted(20, 0, 0, 0);
    p.fillRect(QRectF(row.x(), row.center().y() - 4, 14, 8), visible(c) ? channels_[c].color : QColor("#3a424b"));
    p.setPen(visible(c) ? QColor("#d0d6dc") : QColor("#6b7580"));
    QString text = channels_[c].label;
    if (normalized_ && c < ranges_.size()) text += QString(" %1~%2").arg(ranges_[c].first, 0, 'g', 3).arg(ranges_[c].second, 0, 'g', 3);
    p.drawText(label, Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(text, Qt::ElideRight, int(label.width())));
  }
  if (channels_.isEmpty() || shared < 0) {
    p.setPen(QColor("#8a949e"));
    p.drawText(rect().adjusted(LABEL, RULER, 0, 0), Qt::AlignCenter | Qt::TextWordWrap,
               !message_.isEmpty() ? message_ : QString("숨긴 채널만 있습니다. 왼쪽 범례를 눌러 다시 켜세요."));
  }
  p.setPen(QColor("#56606c"));
  p.drawLine(LABEL, 0, LABEL, height());
  const int x = timeToX(playhead_);
  p.setPen(QPen(QColor("#ef4444"), 2));
  p.drawLine(x, 0, x, height());
}

// ---------------------------------------------------------------- interaction
void GraphEditor::mousePressEvent(QMouseEvent* e) {
  setFocus(Qt::MouseFocusReason);
  if (e->button() != Qt::LeftButton) return;
  const QPointF pos = e->position();
  pressPos_ = pos;
  dragMoved_ = false;
  if (const int l = legendAt(pos); l >= 0) {
    const QString id = idOf(channels_[l].target, channels_[l].property);
    if (!hidden_.remove(id)) hidden_.insert(id);
    fit();
    update();
    return;
  }
  int c = -1, k = -1;
  Side side = Side::Out;
  if (handleAt(pos, &c, &k, &side)) {
    drag_ = Drag::Handle; dragChannel_ = c; dragKey_ = k; dragSide_ = side; dragOriginal_ = channels_[c];
    return;
  }
  if ((k = keyAt(pos, &c)) >= 0) {
    const KeyRef ref = refOf(c, k);
    if (e->modifiers() & Qt::ControlModifier) {  // Ctrl+click toggles multi-selection, no drag
      if (!selection_.removeOne(ref)) selection_ << ref;
      update();
      return;
    }
    if (!selection_.contains(ref)) selection_ = {ref};
    drag_ = Drag::Key; dragChannel_ = c; dragKey_ = k; dragOriginal_ = channels_[c]; axis_ = Axis::Undecided;
    dragTime_ = ref.timeMs; dragValue_ = channels_[c].keys[k].value;
    update();
    return;
  }
  if (!(e->modifiers() & Qt::ControlModifier)) selection_.clear();
  if (pos.x() >= LABEL) {
    // Like the dopesheet ruler: scrubbing samples poses only and never fires clip events.
    drag_ = Drag::Scrub;
    if (onScrub) onScrub(timeline::snap(xToTime(pos.x()), e->modifiers() & Qt::ShiftModifier, duration_));
  }
  update();
}
void GraphEditor::mouseMoveEvent(QMouseEvent* e) {
  const QPointF pos = e->position();
  const bool fine = e->modifiers() & Qt::ShiftModifier;
  if (drag_ == Drag::Scrub && onScrub) onScrub(timeline::snap(xToTime(pos.x()), fine, duration_));
  if (drag_ == Drag::Handle) {
    const int seg = dragSide_ == Side::Out ? dragKey_ : dragKey_ - 1;
    auto& key = channels_[dragChannel_].keys[seg];
    key.curve = curveForHandle(dragChannel_, dragKey_, dragSide_, pos);
    key.interp = "bezier";
    dragMoved_ = true;
    update();
  }
  if (drag_ == Drag::Key) {
    const QPointF d = pos - pressPos_;
    if (axis_ == Axis::Undecided) {
      if (std::fabs(d.x()) + std::fabs(d.y()) < 4) return;
      // The first movement picks the axis so a value nudge never shifts time by accident (Alt = both).
      axis_ = e->modifiers() & Qt::AltModifier ? Axis::Both : std::fabs(d.x()) > std::fabs(d.y()) ? Axis::Time : Axis::Value;
    }
    const Key& orig = dragOriginal_.keys[dragKey_];
    dragTime_ = axis_ == Axis::Value ? orig.timeMs : timeline::snap(xToTime(pos.x()), fine, duration_);
    dragValue_ = axis_ == Axis::Time ? orig.value : roundValue(yToValue(dragChannel_, pos.y()));
    if (dragOriginal_.property == "alpha") dragValue_ = std::clamp(dragValue_, 0.0, 1.0);
    Channel preview = dragOriginal_;
    preview.keys[dragKey_].timeMs = dragTime_;
    preview.keys[dragKey_].value = dragValue_;
    std::stable_sort(preview.keys.begin(), preview.keys.end(), [](const Key& a, const Key& b) { return a.timeMs < b.timeMs; });
    channels_[dragChannel_] = preview;
    dragMoved_ = dragTime_ != orig.timeMs || dragValue_ != orig.value;
    update();
  }
}
void GraphEditor::mouseReleaseEvent(QMouseEvent*) {
  const Drag mode = drag_;
  drag_ = Drag::None;
  if (mode == Drag::Handle && dragMoved_) {
    const int c = dragChannel_, seg = dragSide_ == Side::Out ? dragKey_ : dragKey_ - 1;
    const KeyRef ref{dragOriginal_.target, dragOriginal_.property, dragOriginal_.keys[seg].timeMs};
    const cutout::Curve curve = channels_[c].keys[seg].curve;
    const Channel original = dragOriginal_;
    if (!onCurve || !onCurve(ref, curve)) {
      const int now = channelIndex(original.target, original.property);
      if (now >= 0) channels_[now] = original;
    }
  } else if (mode == Drag::Key && dragMoved_) {
    const Channel original = dragOriginal_;
    const KeyRef from{original.target, original.property, original.keys[dragKey_].timeMs}, to{from.target, from.property, dragTime_};
    const QVector<KeyRef> before = selection_;
    // Follow the key to its new time first: a synchronous refresh then keeps it selected.
    if (const int at = int(selection_.indexOf(from)); at >= 0) selection_.replace(at, to); else selection_ << to;
    if (!onMoveKey || !onMoveKey(from, dragTime_, dragValue_)) {
      const int now = channelIndex(original.target, original.property);
      if (now >= 0) channels_[now] = original;
      selection_ = before;
    }
  }
  update();
}
void GraphEditor::keyPressEvent(QKeyEvent* e) {
  if (e->matches(QKeySequence::SelectAll)) {
    selection_.clear();
    for (int c = 0; c < channels_.size(); c++) if (visible(c)) for (int k = 0; k < channels_[c].keys.size(); k++) selection_ << refOf(c, k);
    update();
    return;
  }
  if (e->key() == Qt::Key_Escape) { selection_.clear(); update(); return; }
  QWidget::keyPressEvent(e);
}
void GraphEditor::contextMenuEvent(QContextMenuEvent* e) {
  int c = -1;
  const int k = keyAt(e->pos(), &c);
  if (k >= 0 && !isSelected(c, k)) selection_ = {refOf(c, k)};
  if (selection_.isEmpty()) return;
  update();
  QMenu menu(this);
  menu.setObjectName("cutoutGraphMenu");
  QMap<QAction*, QString> choices;
  for (const auto& preset : interpPresets()) {
    QAction* a = menu.addAction(QString("%1 · %2").arg(preset.label, preset.tip));
    a->setObjectName(QString("cutoutGraphPreset") + preset.suffix);
    choices.insert(a, preset.id);
  }
  QAction* chosen = menu.exec(e->globalPos());
  if (chosen && onPreset) onPreset(choices.value(chosen));
}
}  // namespace cutout_studio
