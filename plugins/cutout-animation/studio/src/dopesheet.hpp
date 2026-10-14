// Dopesheet: one row per track (target/property), keys as diamonds.
// Key drags commit on release; the panel refuses collisions through the core moveKey op.
#pragma once
#include <QWidget>
#include <functional>
#include "cutout/curve.hpp"

class QPainter;

namespace cutout_studio {
/** Time axis shared by the dopesheet and the graph editor, so keys line up in both views. */
namespace timeline {
constexpr int LABEL = 170, RULER = 22, RIGHT = 20;
int timeToX(int width, int durationMs, double ms);
double xToTime(int width, int durationMs, double x);
/** 10 ms grid by default keeps keys on readable times; `fine` (Shift) gives single-ms placement. */
int snap(double ms, bool fine, int durationMs);
/** Ruler band with adaptive tick spacing so labels never overlap. */
void paintRuler(QPainter& painter, int width, int durationMs);
}  // namespace timeline

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
  explicit Dopesheet(QWidget* parent = nullptr);
  void setRows(const QVector<Row>& rows, int durationMs);
  void setPlayhead(int ms) { playhead_ = ms; update(); }
  const QVector<Row>& rows() const { return rows_; }
  int selectedRow() const { return selRow_; }
  int selectedTime() const { return selTime_; }
  void selectKey(int row, int ms) { selRow_ = row; selTime_ = ms; update(); }
  /** Widget x of a time (used by automation to drag keys). */
  int timeToX(double ms) const;
  int rowCenterY(int row) const;

  std::function<void(int ms)> onScrub;
  std::function<bool(int row, int fromMs, int toMs)> onMoveKey;
  std::function<bool(int row, int ms)> onDeleteKey;
  /** interp = "step" | "linear" | a core preset name (easeIn, easeOut, easeInOut, auto, back). */
  std::function<bool(int row, int ms, const QString& interp)> onInterp;
  std::function<void(int row)> onRowSelected;

 protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
  void contextMenuEvent(QContextMenuEvent*) override;

 private:
  static constexpr int LABEL = timeline::LABEL, RULER = timeline::RULER, ROW = 20;
  double xToTime(double x) const;
  int rowAt(int y) const;
  int keyAt(const QPoint& p, int* row) const;
  int snap(double ms, bool fine) const;
  void paintSegments(QPainter& p, const Row& row, int r, int y) const;
  QVector<Row> rows_;
  int duration_ = 1000, playhead_ = 0, selRow_ = -1, selTime_ = -1;
  enum class DragMode { None, Scrub, Key } dragMode_ = DragMode::None;
  int dragRow_ = -1, dragFrom_ = 0, dragTo_ = 0;
  bool dragMoved_ = false;
};
}  // namespace cutout_studio
