// Dopesheet: one row per track (target/property), keys as diamonds on the shared TimeView
// (zoom, scroll, playhead and selection are the graph editor's too). Gestures only preview
// locally; each reports ONE commit on release (move/scale/delete of the whole selection),
// which the panel turns into one core edit op, so undo/redo and LLM ops stay identical.
#pragma once
#include <QSet>
#include <QWidget>
#include <functional>
#include <optional>
#include "cutout/curve.hpp"
#include "time-view.hpp"

class QPainter;

namespace cutout_studio {
/** Interpolation choices of the dopesheet menu, graph menu and preset bar (core preset names). */
struct InterpPreset { const char* id; const char* label; const char* suffix; const char* tip; };
const QVector<InterpPreset>& interpPresets();

class Dopesheet : public QWidget {
  Q_OBJECT
 public:
  struct Row {
    QString target, property, label;
    QVector<int> times;
    QVector<bool> step;
    QStringList interps;            // per key: "linear" | "step" | "bezier"
    QVector<cutout::Curve> curves;  // per key, used when interps[i] == "bezier"
    bool attachment = false;
  };
  /** Row height, selection strip (extent + scale handles) and the key distance below which keys cluster. */
  static constexpr int ROW = 20, STRIP = 10, CLUSTER_PX = 7;

  explicit Dopesheet(QWidget* parent = nullptr);
  /** Share zoom/scroll/playhead/selection with other views (the panel's TimeView). */
  void setTimeView(TimeView* view);
  TimeView* timeView() const { return view_; }
  /** Replace rows; selected keys that no longer exist are dropped from the shared selection. */
  void setRows(const QVector<Row>& rows, int durationMs);
  void setPlayhead(int ms) { view_->setPlayhead(ms); }
  const QVector<Row>& rows() const { return rows_; }
  /** First selected key's row/time (-1 when nothing is selected). */
  int selectedRow() const;
  int selectedTime() const;
  /** Select exactly one key (row < 0 clears the selection). */
  void selectKey(int row, int ms);
  QVector<KeyRef> selectedKeys() const { return view_->selection(); }
  KeyRef refOf(int row, int ms) const { return {rows_[row].target, rows_[row].property, ms}; }
  QVector<KeyRef> rowKeys(int row) const;
  QVector<KeyRef> keysAt(int ms) const;  // column: every key at that time
  QVector<KeyRef> allKeys() const;
  int rowOf(const QString& target, const QString& property) const;

  // Geometry (shared with automation/tests).
  int timeToX(double ms) const;
  int rowCenterY(int row) const;
  /** Key-index groups of a row closer than CLUSTER_PX at the current zoom (size 1 = lone key). */
  QVector<QVector<int>> clusters(int row) const;
  /** Center of the selection strip's scale handle (left/right end), nullopt below 2 distinct times. */
  std::optional<QPointF> scaleHandle(bool right) const;

  std::function<void(int ms)> onScrub;
  /** Release of a selection drag: every key moves by deltaMs. Return false when refused. */
  std::function<bool(const QVector<KeyRef>& keys, int deltaMs)> onMoveKeys;
  /** Release of a scale-handle drag: times become pivot + (t - pivot) * factor. */
  std::function<bool(const QVector<KeyRef>& keys, int pivotMs, double factor)> onScaleKeys;
  std::function<bool(const QVector<KeyRef>& keys)> onDeleteKeys;
  /** Interp/preset (core name) for the selected keys (context menu). */
  std::function<void(const QString& preset)> onPreset;
  /** "copy" | "paste" | "duplicate" (Ctrl+C / Ctrl+V / Ctrl+D, menu). */
  std::function<void(const QString& command)> onCommand;
  /** Arrow keys: step the playhead by `steps` (Shift = 10 steps). */
  std::function<void(int steps)> onStep;
  /** Row header / key click; add = Ctrl or Shift held (the target joins the canvas selection). */
  std::function<void(int row, bool add)> onRowSelected;
  /** Targets ("bone:<id>", "slot:<id>") of the editor selection: their row labels are marked. */
  void setHighlightTargets(const QSet<QString>& targets) { if (targets != highlighted_) { highlighted_ = targets; update(); } }
  const QSet<QString>& highlightTargets() const { return highlighted_; }

 protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void mouseDoubleClickEvent(QMouseEvent*) override;
  void wheelEvent(QWheelEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
  void contextMenuEvent(QContextMenuEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void moveEvent(QMoveEvent*) override { update(); }  // the sticky header must repaint on scroll

 private:
  static constexpr int LABEL = TimeView::LABEL, RULER = TimeView::RULER;
  enum class Drag { None, Pan, Scrub, LoopIn, LoopOut, Keys, ScaleLeft, ScaleRight, Band };
  struct Hit { int row = -1; QVector<int> times; };  // a lone key or a cluster
  TimeView* view_ = nullptr;
  QVector<Row> rows_;
  QSet<QString> highlighted_;
  Drag drag_ = Drag::None;
  QPointF pressPos_, lastPos_;
  int pressTime10_ = 0, pressTime1_ = 0, delta_ = 0, pivot_ = 0, edge_ = 0;
  double factor_ = 1;
  bool moved_ = false, bandAdd_ = false;
  std::optional<KeyRef> pendingSingle_;
  QRectF band_;

  double xToTime(double x) const { return view_->xToTime(x); }
  /** Ruler + selection strip stay at the top of the visible area when the rows scroll. */
  int headerTop() const { return std::max(0, -y()); }
  int rowTop(int row) const { return RULER + STRIP + row * ROW; }
  int rowAt(int y) const;
  Hit hitAt(QPointF p) const;
  KeyRef pickInCluster(const Hit& hit) const;
  bool selectionExtent(int& lo, int& hi) const;  // over the selected keys' committed times
  int shownTime(int row, int t) const;           // drag/scale preview of a key time
  QVector<QVector<int>> groups(const QVector<int>& times) const;
  void paintSegments(QPainter& p, const Row& row, const QVector<int>& times, int y) const;
};
}  // namespace cutout_studio
