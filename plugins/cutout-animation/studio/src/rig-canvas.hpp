// Zoom/pan canvas that renders a core Pose and turns mouse drags into gesture deltas.
// It never edits documents itself: the panel converts a finished drag into ONE edit.
#pragma once
#include <QWidget>
#include <functional>
#include <memory>
#include <optional>
#include "rig-render.hpp"

namespace cutout_studio {
class RigCanvas : public QWidget {
  Q_OBJECT
 public:
  enum Tool { Move = 0, Rotate = 1, Scale = 2, Pivot = 3 };
  struct Drag {
    QString ref;            // "bone:<id>" | "slot:<id>"
    int tool = Move;
    QPointF origin, start, current;  // rig space; origin = manipulation center
    double angle = 0;       // accumulated clockwise degrees (multi-turn safe)
    double ratio = 1;       // |current-origin| / |start-origin|
    bool shift = false, moved = false;
  };
  explicit RigCanvas(QWidget* parent = nullptr);

  void setScene(std::shared_ptr<const cutout::Rig> rig, const cutout::Pose& pose);
  void setImages(ImageCache* images) { images_ = images; }
  void setHiddenSlots(const QSet<QString>& hidden) { hidden_ = hidden; update(); }
  void setSelection(const QString& ref) { selection_ = ref; update(); }
  void setTool(int tool) { tool_ = tool; update(); }
  int tool() const { return tool_; }
  void setAnimateMode(bool animate) { animate_ = animate; update(); }
  void setMessage(const QString& text) { message_ = text; update(); }
  /** Fit the authoring canvas into the widget (also re-enables auto fit on resize). */
  void fitToCanvas();
  QTransform viewTransform() const;
  QPointF widgetToRig(const QPointF& p) const;
  QPointF rigToWidget(const QPointF& p) const;
  QString hitTest(const QPointF& widgetPos) const;
  bool dragging() const { return drag_.has_value(); }

  std::function<void(const QString&)> onSelect;
  std::function<QPointF(const QString&)> originOf;
  std::function<void(const Drag&, bool finished)> onDrag;
  std::function<void(int)> onTool;
  std::function<void()> onKey;

 protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void wheelEvent(QWheelEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void keyPressEvent(QKeyEvent*) override;

 private:
  std::shared_ptr<const cutout::Rig> rig_;
  cutout::Pose pose_;
  ImageCache* images_ = nullptr;
  QSet<QString> hidden_;
  QString selection_, message_;
  int tool_ = Move;
  bool animate_ = false, autoFit_ = true;
  double zoom_ = 1;
  QPointF pan_;
  std::optional<Drag> drag_;
  bool panning_ = false;
  QPointF panStart_, panOrigin_, pressWidget_;
  double lastAngle_ = 0;
};
}  // namespace cutout_studio
