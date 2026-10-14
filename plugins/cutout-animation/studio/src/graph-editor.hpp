// Graph editor: value-over-time curves of the selected bone/slot tracks (one colored channel
// per property), keys as points and Bezier tangent handles. It shares the dopesheet's time
// axis. Drags only preview locally; each gesture reports ONE commit on release, which the
// panel turns into core edit ops (one host edit, so undo/redo and LLM ops stay identical).
#pragma once
#include <QColor>
#include <QSet>
#include <QWidget>
#include <functional>
#include <optional>
#include "cutout/curve.hpp"

namespace cutout_studio {
class GraphEditor : public QWidget {
  Q_OBJECT
 public:
  struct Key { int timeMs = 0; double value = 0; QString interp = "linear"; cutout::Curve curve = cutout::LINEAR_CURVE; };
  struct Channel { QString target, property, label; QColor color; QVector<Key> keys; };
  /** Key identity that survives refreshes. */
  struct KeyRef {
    QString target, property;
    int timeMs = 0;
    bool operator==(const KeyRef& o) const { return timeMs == o.timeMs && target == o.target && property == o.property; }
  };
  /** Out = handle toward the next key (this key's curve x1,y1); In = from the previous key (its x2,y2). */
  enum class Side { Out, In };

  explicit GraphEditor(QWidget* parent = nullptr);
  /** Replace the shown curves; keeps the selection of keys that still exist and refits the value range. */
  void setChannels(const QVector<Channel>& channels, int durationMs);
  void setPlayhead(int ms) { playhead_ = ms; update(); }
  void setMessage(const QString& text) { message_ = text; update(); }
  /** Each channel fitted to its own range (shapes of small and large values side by side). */
  void setNormalized(bool on);
  bool normalized() const { return normalized_; }
  const QVector<Channel>& channels() const { return channels_; }
  QVector<KeyRef> selectedKeys() const { return selection_; }
  void setSelectedKeys(const QVector<KeyRef>& keys);
  int channelIndex(const QString& target, const QString& property) const;

  // Geometry (shared with automation/tests).
  int timeToX(double ms) const;
  double xToTime(double x) const;
  double valueToY(int channel, double value) const;
  double yToValue(int channel, double y) const;
  QPointF keyPoint(int channel, int key) const;
  /** Handle position, or nullopt when not editable (no neighbour key, or a step segment). */
  std::optional<QPointF> handlePoint(int channel, int key, Side side) const;
  /** Curve the segment gets when that handle is dropped at `pos` (rounded and clamped like a commit). */
  cutout::Curve curveForHandle(int channel, int key, Side side, QPointF pos) const;

  std::function<void(int ms)> onScrub;
  /** Key drag released: time and/or value changed. Return false when refused (collision). */
  std::function<bool(const KeyRef& key, int toMs, double value)> onMoveKey;
  /** Handle drag released: new curve for the segment starting at `segment`. */
  std::function<bool(const KeyRef& segment, const cutout::Curve& curve)> onCurve;
  /** Context menu preset (core name) for the selected keys. */
  std::function<void(const QString& preset)> onPreset;

 protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
  void contextMenuEvent(QContextMenuEvent*) override;
  void resizeEvent(QResizeEvent*) override { update(); }

 private:
  enum class Drag { None, Scrub, Key, Handle };
  QVector<Channel> channels_;
  QVector<KeyRef> selection_;
  QSet<QString> hidden_;  // legend toggles, editor-only
  QVector<std::pair<double, double>> ranges_;
  int duration_ = 1000, playhead_ = 0;
  bool normalized_ = false;
  QString message_;
  Drag drag_ = Drag::None;
  int dragChannel_ = -1, dragKey_ = -1;
  Side dragSide_ = Side::Out;
  Channel dragOriginal_;
  QPointF pressPos_;
  enum class Axis { Undecided, Time, Value, Both } axis_ = Axis::Undecided;
  bool dragMoved_ = false;
  int dragTime_ = 0;
  double dragValue_ = 0;

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
};
}  // namespace cutout_studio
