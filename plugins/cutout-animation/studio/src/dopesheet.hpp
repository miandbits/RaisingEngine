// Dopesheet: one row per track (target/property), keys as diamonds.
// Key drags commit on release; the panel refuses collisions through the core moveKey op.
#pragma once
#include <QWidget>
#include <functional>

namespace cutout_studio {
class Dopesheet : public QWidget {
  Q_OBJECT
 public:
  struct Row {
    QString target, property, label;
    QVector<int> times;
    QVector<bool> step;
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
  static constexpr int LABEL = 170, RULER = 22, ROW = 20;
  double xToTime(double x) const;
  int rowAt(int y) const;
  int keyAt(const QPoint& p, int* row) const;
  int snap(double ms, bool fine) const;
  QVector<Row> rows_;
  int duration_ = 1000, playhead_ = 0, selRow_ = -1, selTime_ = -1;
  enum class DragMode { None, Scrub, Key } dragMode_ = DragMode::None;
  int dragRow_ = -1, dragFrom_ = 0, dragTo_ = 0;
  bool dragMoved_ = false;
};
}  // namespace cutout_studio
