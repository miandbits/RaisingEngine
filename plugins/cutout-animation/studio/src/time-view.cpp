#include "time-view.hpp"
#include <QPainter>
#include <QPolygonF>
#include <climits>
#include <cmath>

namespace cutout_studio {
int TimeView::snap(double ms, bool fine) const {
  const int step = fine ? 1 : 10;
  return std::clamp(int(std::lround(ms / step)) * step, 0, duration_);
}
void TimeView::setDuration(int ms) {
  ms = std::max(1, ms);
  if (ms == duration_) return;
  duration_ = ms;
  setLoop(loopIn_, loopOut_);
  if (fitted_) fit(); else { clampScroll(); Q_EMIT changed(); }
}
void TimeView::setUiScale(double scale) {
  scale = std::clamp(scale, 0.5, 3.0);
  if (std::fabs(scale - ui_) < 1e-6) return;
  ui_ = scale;
  // The label column changed: a fitted view refits, a zoomed one keeps its zoom.
  if (fitted_) fit(); else { clampScroll(); Q_EMIT changed(); }
}
void TimeView::addView(QWidget* view) {
  views_ << view;
  viewResized();
}
void TimeView::viewResized() {
  int width = INT_MAX;
  for (const auto& v : views_) if (v && v->isVisible() && v->width() > 0) width = std::min(width, v->width());
  if (width == INT_MAX) for (const auto& v : views_) if (v && v->width() > 0) width = std::min(width, v->width());
  if (width == INT_MAX || width == width_) return;
  width_ = width;
  if (fitted_) fit(); else { clampScroll(); Q_EMIT changed(); }
}
void TimeView::fit() {
  px_ = fitPx();
  scroll_ = 0;
  fitted_ = true;
  Q_EMIT changed();
}
void TimeView::zoomAround(double ms, double factor) {
  // Zooming out stops at half the fitted scale: the whole clip plus empty space is enough.
  const double next = std::clamp(px_ * factor, fitPx() * 0.5, MAX_PX_PER_MS);
  if (next == px_) return;
  const double x = (ms - scroll_) * px_;  // keep the time under the cursor in place
  px_ = next;
  scroll_ = ms - x / px_;
  fitted_ = false;
  clampScroll();
  Q_EMIT changed();
}
void TimeView::zoomToRange(double fromMs, double toMs) {
  const double span = std::max(1.0, toMs - fromMs);
  px_ = std::clamp(plotWidth() / span, fitPx() * 0.5, MAX_PX_PER_MS);
  scroll_ = (fromMs + toMs) / 2 - visibleMs() / 2;
  fitted_ = false;
  clampScroll();
  Q_EMIT changed();
}
void TimeView::setScroll(double ms) {
  const double before = scroll_;
  scroll_ = ms;
  clampScroll();
  if (scroll_ != before) { fitted_ = false; Q_EMIT changed(); }
}
void TimeView::reveal(double ms) {
  if (ms >= scroll_ && ms <= scroll_ + visibleMs()) return;
  setScroll(ms - visibleMs() * 0.1);
}
void TimeView::clampScroll() {
  const double maxScroll = std::max(0.0, duration_ - visibleMs());
  scroll_ = std::clamp(scroll_, 0.0, maxScroll);
}

void TimeView::setPlayhead(int ms) {
  ms = std::clamp(ms, 0, duration_);
  if (ms == playhead_) return;
  playhead_ = ms;
  Q_EMIT changed();
}
void TimeView::setLoop(int in, int out) {
  if (out < 0) out = duration_;
  in = std::clamp(in, 0, duration_ - 1);
  out = std::clamp(out, in + 1, duration_);
  const int storedOut = in == 0 && out == duration_ ? -1 : out;
  if (in == loopIn_ && storedOut == loopOut_) return;
  loopIn_ = in;
  loopOut_ = storedOut;
  Q_EMIT changed();
}
int TimeView::loopMarkerAt(double x, double rulerY) const {
  if (rulerY < ruler() - 10 * ui_ || rulerY > ruler()) return 0;
  const double din = std::fabs(x - timeToX(loopIn())), dout = std::fabs(x - timeToX(loopOut()));
  if (std::min(din, dout) > 6 * ui_) return 0;
  return din < dout ? 1 : 2;
}

void TimeView::setSelection(const QVector<KeyRef>& keys) {
  QVector<KeyRef> unique;
  QSet<KeyRef> set;
  for (const auto& k : keys) if (!set.contains(k)) { set.insert(k); unique << k; }
  if (unique == selection_) return;
  selection_ = unique;
  selected_ = set;
  Q_EMIT changed();
}
void TimeView::toggle(const KeyRef& k) {
  QVector<KeyRef> next = selection_;
  if (!next.removeOne(k)) next << k;
  setSelection(next);
}
void TimeView::add(const QVector<KeyRef>& keys) { setSelection(selection_ + keys); }
void TimeView::pruneSelection(const QSet<KeyRef>& existing) {
  QVector<KeyRef> keep;
  for (const auto& k : selection_) if (existing.contains(k)) keep << k;
  setSelection(keep);
}

void TimeView::paintRuler(QPainter& p, int width) const {
  p.save();
  const int LABEL = label(), RULER = ruler();
  const double u = ui_;
  p.fillRect(QRect(0, 0, width, RULER), QColor("#2a3038"));
  // Left of the plot: the visible span, so the zoom level is always readable.
  p.setPen(QColor("#7d8894"));
  const int from = int(std::floor(std::max(0.0, scroll_))), to = int(std::ceil(std::min<double>(duration_, scroll_ + visibleMs())));
  p.drawText(QRect(6, 0, LABEL - 10, RULER), Qt::AlignLeft | Qt::AlignVCenter, QString("%1–%2 ms").arg(from).arg(to));
  p.setClipRect(QRect(LABEL, 0, width - LABEL, RULER));
  // Past the clip end is darker: keys can never go there.
  const double end = timeToX(duration_);
  if (end < width) p.fillRect(QRectF(end, 0, width - end, RULER), QColor("#20252b"));
  if (loopActive()) p.fillRect(QRectF(timeToX(loopIn()), 0, timeToX(loopOut()) - timeToX(loopIn()), RULER), QColor(74, 222, 128, 48));
  // Label spacing adapts to the zoom (1 ms ticks when zoomed in, seconds when zoomed out).
  int tick = 60000;
  for (int c : {1, 2, 5, 10, 20, 50, 100, 200, 250, 500, 1000, 2000, 5000, 10000, 30000, 60000}) if (c * px_ >= 56 * u) { tick = c; break; }
  int minor = 0;
  for (int div : {5, 2}) if (tick % div == 0 && tick / div * px_ >= 6) { minor = tick / div; break; }
  const double lo = std::max(0.0, scroll_), hi = std::min<double>(duration_, scroll_ + visibleMs() + 1);
  if (minor > 0) {
    p.setPen(QColor("#56606c"));
    for (long long t = (long long)(lo / minor) * minor; t <= hi; t += minor) { const double x = timeToX(double(t)); p.drawLine(QPointF(x, RULER - 3 * u), QPointF(x, RULER)); }
  }
  p.setPen(QColor("#8a949e"));
  for (long long t = (long long)(lo / tick) * tick; t <= hi; t += tick) {
    const double x = timeToX(double(t));
    p.drawLine(QPointF(x, RULER - 7 * u), QPointF(x, RULER));
    p.drawText(QRectF(x + 2, 2, 70 * u, RULER - 6 * u), Qt::AlignLeft | Qt::AlignVCenter, QString::number(t));
  }
  // Loop markers are always drawn (grey when the range is the whole clip) so they can be dragged.
  p.setRenderHint(QPainter::Antialiasing, true);
  const QColor marker = loopActive() ? QColor("#4ade80") : QColor("#6b7580");
  p.setPen(Qt::NoPen);
  p.setBrush(marker);
  const double xi = timeToX(loopIn()), xo = timeToX(loopOut());
  p.drawPolygon(QPolygonF({QPointF(xi, RULER - 9 * u), QPointF(xi + 7 * u, RULER - 4.5 * u), QPointF(xi, RULER)}));
  p.drawPolygon(QPolygonF({QPointF(xo, RULER - 9 * u), QPointF(xo - 7 * u, RULER - 4.5 * u), QPointF(xo, RULER)}));
  p.restore();
}
}  // namespace cutout_studio
