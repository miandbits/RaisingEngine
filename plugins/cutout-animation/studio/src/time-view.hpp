// Shared timeline state of the dopesheet and the graph editor: zoom, scroll, playhead,
// preview loop range and the key selection live here once, so both views always draw the
// same key at the same x and show the same selection. Editor-only: nothing here is stored
// in documents (the loop range is per clip and per session).
#pragma once
#include <QHash>
#include <algorithm>
#include <cmath>
#include <QObject>
#include <QPointer>
#include <QWidget>
#include <QSet>
#include <QString>
#include <QVector>

class QPainter;

namespace cutout_studio {
/** Key identity that survives refreshes (clip-local). */
struct KeyRef {
  QString target, property;
  int timeMs = 0;
  bool operator==(const KeyRef& o) const { return timeMs == o.timeMs && target == o.target && property == o.property; }
};
inline size_t qHash(const KeyRef& k, size_t seed = 0) { return qHashMulti(seed, k.target, k.property, k.timeMs); }

class TimeView : public QObject {
  Q_OBJECT
 public:
  /** Label column, ruler band and margins shared by both views at UI scale 1 (x = label() + pad()
   *  at scroll 0). The views use the scaled accessors below. */
  static constexpr int LABEL = 170, RULER = 22, RIGHT = 20, PAD = 8;
  /** Workspace UI scale (plugin 1.10.0, editor-only: 작업 공간 글자 크기): label column, ruler and the
   *  views' rows, keys and hit radii grow with it so text never outgrows its band. */
  void setUiScale(double scale);
  double uiScale() const { return ui_; }
  int label() const { return int(std::lround(LABEL * ui_)); }
  int ruler() const { return int(std::lround(RULER * ui_)); }
  int right() const { return int(std::lround(RIGHT * ui_)); }
  int pad() const { return int(std::lround(PAD * ui_)); }
  /** 40 px per ms: a single millisecond is a comfortable click target. */
  static constexpr double MAX_PX_PER_MS = 40;
  explicit TimeView(QObject* parent = nullptr) : QObject(parent) {}

  // ---- axis
  int duration() const { return duration_; }
  /** Keeps the zoom unless the view is still fitted to the clip (then it refits). */
  void setDuration(int ms);
  /** Views register so the fitted span matches the narrowest visible one (the dopesheet loses
   *  a scrollbar's width to the graph); call viewResized() from their resizeEvent. */
  void addView(QWidget* view);
  void viewResized();
  double pxPerMs() const { return px_; }
  double scrollMs() const { return scroll_; }
  double visibleMs() const { return plotWidth() / px_; }
  bool fitted() const { return fitted_; }
  double timeToX(double ms) const { return label() + pad() + (ms - scroll_) * px_; }
  double xToTime(double x) const { return (x - label() - pad()) / px_ + scroll_; }
  /** 10 ms grid by default keeps keys on readable times; `fine` (Shift) gives single-ms placement. */
  int snap(double ms, bool fine) const;
  void fit();
  /** Zoom keeping `ms` under the same x (Ctrl+wheel at the cursor). */
  void zoomAround(double ms, double factor);
  /** Show [from, to] across the plot (cluster double-click). */
  void zoomToRange(double fromMs, double toMs);
  void setScroll(double ms);
  void panPixels(double dx) { setScroll(scroll_ - dx / px_); }
  /** Scroll by pages so `ms` is on screen (playback and stepping follow the playhead). */
  void reveal(double ms);

  // ---- playhead and preview loop range
  int playhead() const { return playhead_; }
  void setPlayhead(int ms);
  int loopIn() const { return loopIn_; }
  int loopOut() const { return loopOut_ < 0 ? duration_ : loopOut_; }
  /** A range narrower than the whole clip is set: playback loops inside it. */
  bool loopActive() const { return loopIn_ > 0 || (loopOut_ >= 0 && loopOut_ < duration_); }
  void setLoop(int in, int out);
  void clearLoop() { setLoop(0, -1); }
  /** 1 = in marker, 2 = out marker near (x, y in the ruler band), 0 = none. Only the marker
   *  triangles at the bottom of the ruler grab: a click on the labels always scrubs, even at 0
   *  and at the clip end where the (inactive) markers sit. */
  int loopMarkerAt(double x, double rulerY) const;

  // ---- selection (shared by both views)
  const QVector<KeyRef>& selection() const { return selection_; }
  void setSelection(const QVector<KeyRef>& keys);
  bool isSelected(const KeyRef& k) const { return selected_.contains(k); }
  void toggle(const KeyRef& k);
  void add(const QVector<KeyRef>& keys);
  void clearSelection() { setSelection({}); }
  /** Drop selected keys that no longer exist (after an edit, undo or clip switch). */
  void pruneSelection(const QSet<KeyRef>& existing);

  void paintRuler(QPainter& p, int width) const;

 Q_SIGNALS:
  void changed();

 private:
  int duration_ = 1000, width_ = 600, playhead_ = 0, loopIn_ = 0, loopOut_ = -1;
  double px_ = 0.4, scroll_ = 0, ui_ = 1;
  bool fitted_ = true;
  QVector<KeyRef> selection_;
  QSet<KeyRef> selected_;
  QVector<QPointer<QWidget>> views_;
  // Fitted, the clip spans label + pad .. width - right + pad (same mapping as plugin 1.1.0).
  double plotWidth() const { return std::max(40, width_ - label() - right()); }
  double fitPx() const { return plotWidth() / std::max(1, duration_); }
  void clampScroll();
};
}  // namespace cutout_studio
