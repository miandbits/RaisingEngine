// Graph editor: value-over-time curves of the selected bone/slot tracks (one colored channel
// per property), keys as points and Bezier tangent handles. It shares the dopesheet's
// TimeView (zoom, scroll, playhead, selection). Drags only preview locally; each gesture
// reports ONE commit on release, which the panel turns into core edit ops (one host edit,
// so undo/redo and LLM ops stay identical).
#pragma once
#include <QColor>
#include <QHash>
#include <QSet>
#include <QWidget>
#include <functional>
#include <optional>
#include "cutout/curve.hpp"
#include "time-view.hpp"

namespace cutout_studio {
class GraphEditor : public QWidget {
  Q_OBJECT
 public:
  struct Key { int timeMs = 0; double value = 0; QString interp = "linear"; cutout::Curve curve = cutout::LINEAR_CURVE; };
  struct Channel { QString target, property, label; QColor color; QVector<Key> keys; };
  using KeyRef = cutout_studio::KeyRef;
  /** Out = handle toward the next key (this key's curve x1,y1); In = from the previous key (its x2,y2). */
  enum class Side { Out, In };

  explicit GraphEditor(QWidget* parent = nullptr);
  void setTimeView(TimeView* view);
  TimeView* timeView() const { return view_; }
  /** Replace the shown curves and refit the value range (the shared selection is kept). A value
   *  zoom/pan of the user is kept while the same channels come back (edits, undo). */
  void setChannels(const QVector<Channel>& channels, int durationMs);
  void setPlayhead(int ms) { view_->setPlayhead(ms); }
  void setMessage(const QString& text) { message_ = text; update(); }
  /** Each channel fitted to its own range (shapes of small and large values side by side). */
  void setNormalized(bool on);
  bool normalized() const { return normalized_; }
  // ---- Value axis (plugin 1.8.0, editor-only view state; the time axis stays the shared TimeView)
  /** Zoom the value axis keeping the value under `y` in place (factor > 1 = closer); every
   *  channel's range scales the same way, so normalized channels stay aligned. */
  void zoomValues(double y, double factor);
  /** Scroll the value axis by `dy` pixels: content follows the cursor (middle-drag). */
  void panValues(double dy);
  /** 값 맞춤: refit the value range to the visible channels and drop the manual zoom. */
  void fitValues();
  bool valuesZoomed() const { return valueManual_; }
  std::pair<double, double> valueRange(int channel) const { return ranges_.value(channel, {0.0, 1.0}); }
  const QVector<Channel>& channels() const { return channels_; }
  QVector<KeyRef> selectedKeys() const { return view_->selection(); }
  void setSelectedKeys(const QVector<KeyRef>& keys) { view_->setSelection(keys); }
  int channelIndex(const QString& target, const QString& property) const;

  // Geometry (shared with automation/tests).
  int timeToX(double ms) const;
  double xToTime(double x) const { return view_->xToTime(x); }
  double valueToY(int channel, double value) const;
  double yToValue(int channel, double y) const;
  QPointF keyPoint(int channel, int key) const;
  /** Handle position, or nullopt when not editable (no neighbour key, or a step segment). */
  std::optional<QPointF> handlePoint(int channel, int key, Side side) const;
  /** Curve the segment gets when that handle is dropped at `pos` (rounded and clamped like a commit). */
  cutout::Curve curveForHandle(int channel, int key, Side side, QPointF pos) const;

  std::function<void(int ms)> onScrub;
  /** Single key drag released: time and/or value changed. Return false when refused (collision). */
  std::function<bool(const KeyRef& key, int toMs, double value)> onMoveKey;
  /** Drag of a multi-key selection: all keys move by deltaMs; `values` = new values of keys
   *  whose value changed (by their original ref). Return false when refused. */
  std::function<bool(const QVector<KeyRef>& keys, int deltaMs, const QHash<KeyRef, double>& values)> onMoveKeys;
  /** Handle drag released: new curve for the segment starting at `segment`. */
  std::function<bool(const KeyRef& segment, const cutout::Curve& curve)> onCurve;
  /** Context menu preset (core name) for the selected keys. */
  std::function<void(const QString& preset)> onPreset;
  std::function<bool(const QVector<KeyRef>& keys)> onDeleteKeys;
  /** "copy" | "paste" | "duplicate". */
  std::function<void(const QString& command)> onCommand;
  std::function<void(int steps)> onStep;

 protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void wheelEvent(QWheelEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
  void contextMenuEvent(QContextMenuEvent*) override;
  void resizeEvent(QResizeEvent*) override;

 private:
  enum class Drag { None, Pan, Scrub, LoopIn, LoopOut, Key, Handle, Band };
  TimeView* view_ = nullptr;
  QVector<Channel> channels_;
  QSet<QString> hidden_;  // legend toggles, editor-only
  QVector<std::pair<double, double>> ranges_;
  bool normalized_ = false, valueManual_ = false;  // valueManual_: ranges_ come from zoomValues/panValues
  QStringList rangeIds_;                           // channel ids the (manual) ranges_ belong to
  QString message_;
  Drag drag_ = Drag::None;
  int dragChannel_ = -1, dragKey_ = -1;
  Side dragSide_ = Side::Out;
  QVector<Channel> originals_;  // channels at drag start (preview is rebuilt from them)
  QPointF pressPos_, lastPos_;
  enum class Axis { Undecided, Time, Value, Both } axis_ = Axis::Undecided;
  bool dragMoved_ = false, group_ = false, bandAdd_ = false;
  int dragTime_ = 0, groupDelta_ = 0;
  double dragValue_ = 0;
  QVector<double> groupDv_;      // per channel value delta of a group drag
  QSet<KeyRef> previewSelected_;  // selected keys at their preview times (group drag)
  std::optional<KeyRef> pendingSingle_;
  QRectF band_;

  double legendWidth() const { return normalized_ ? 158 : 98; }  // value ticks use the rest of the label column
  double plotTop() const;
  double plotBottom() const;
  bool visible(int channel) const;
  bool isSelected(int channel, int key) const;
  KeyRef refOf(int channel, int key) const;
  void fit();
  /** Segment `seg` of a channel: x/values of its keys and the handles in effect. */
  bool segment(int channel, int seg, double& t0, double& t1, double& v0, double& v1, cutout::Curve& curve) const;
  double sample(const Channel& c, double t) const;
  int keyAt(QPointF p, int* channel) const;
  bool handleAt(QPointF p, int* channel, int* key, Side* side) const;
  int legendAt(QPointF p) const;
  void previewGroup(QPointF pos, bool fine);
};
}  // namespace cutout_studio
