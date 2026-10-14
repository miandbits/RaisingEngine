// Zoom/pan canvas that renders a core Pose and turns mouse drags into gesture deltas.
// It never edits documents itself: the panel converts a finished drag into ONE edit.
// Onion-skin ghosts and the motion trail are handed in ready-made (editor-only overlays).
#pragma once
#include <QHash>
#include <QMimeData>
#include <QPolygonF>
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
  /** Onion-skin ghost: a pose of `rig` drawn behind the live pose, tinted (invalid tint =
   *  원래 색) at `alpha`. `key` identifies (rig data, clip, time) so its layer is cached. */
  struct Ghost {
    std::shared_ptr<const cutout::Rig> rig;
    std::shared_ptr<const cutout::Pose> pose;
    QColor tint;
    double alpha = 0.5;
    int offset = 0;  // < 0 before, > 0 after, 0 current
    quint64 key = 0;
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
  /** Ghosts (any order) and the slots they skip (eye toggles + "selected subtree only"). */
  void setGhosts(const QVector<Ghost>& ghosts, const QSet<QString>& hiddenSlots);
  const QVector<Ghost>& ghosts() const { return ghosts_; }
  /** Motion trail in rig space: polyline + key dots (empty = none). */
  void setTrail(const QPolygonF& points, const QPolygonF& keyPoints);
  /** Ghost layers rendered so far (cache misses; tests check redraws reuse them). */
  int ghostRenders() const { return ghostRenders_; }
  /** IK (plugin 1.3.0): target bones are drawn as crosshairs, chains of `selectedIk` highlighted. */
  void setSelectedIk(const QString& id) { selectedIk_ = id; update(); }
  bool isIkTarget(int bone) const;
  static QColor ikColor();
  /** Fit the authoring canvas into the widget (also re-enables auto fit on resize). */
  void fitToCanvas();
  QTransform viewTransform() const;
  QPointF widgetToRig(const QPointF& p) const;
  QPointF rigToWidget(const QPointF& p) const;
  /** Ref under a widget point; bones=false skips bone handles (parts only). */
  QString hitTest(const QPointF& widgetPos, bool bones = true) const;
  bool dragging() const { return drag_.has_value(); }

  // ---- Mesh editing (plugin 1.4.0). While a mode is on, left-button gestures edit the mesh shown
  // on `slot` instead of selecting/moving bones and parts (right/middle button still pans).
  enum MeshMode { MeshOff = 0, MeshVertices = 1, MeshWeights = 2 };
  /** Vertices: wireframe + handles (`selected` highlighted); Weights: heat map of `bone` (-1 none) and a brush of `brushRadius` widget px. */
  void setMeshEdit(int mode, const QString& slot, const QSet<int>& selected, int bone, double brushRadius);
  int meshMode() const { return meshMode_; }
  QString meshSlot() const { return meshSlot_; }
  /** Rig-space vertices of the edited mesh as drawn now (empty = none / not a mesh). */
  QVector<QPointF> meshVertices() const;
  /** Vertex handle under a widget point (-1 = none). */
  int vertexAt(const QPointF& widgetPos) const;
  /** Click/band selection (add = Shift). An empty click clears. */
  std::function<void(const QVector<int>& vertices, bool add)> onMeshSelect;
  /** Drag of the selected vertices by `rigDelta` (live while !finished; one edit on finish). */
  std::function<void(const QPointF& rigDelta, bool finished)> onMeshDrag;
  /** Weight brush stroke: every vertex the brush touched so far (one edit on finish). */
  std::function<void(const QVector<int>& vertices, bool finished)> onMeshPaint;

  std::function<void(const QString&)> onSelect;
  std::function<QPointF(const QString&)> originOf;
  std::function<void(const Drag&, bool finished)> onDrag;
  std::function<void(int)> onTool;
  std::function<void()> onKey;
  std::function<void()> onOnion;
  /** PNG files / asset ids dropped on the canvas; `slotRef` = part under the cursor ("" = none). */
  std::function<void(const QMimeData* mime, const QString& slotRef)> onDropImage;

 protected:
  void dragEnterEvent(QDragEnterEvent*) override;
  void dragMoveEvent(QDragMoveEvent*) override;
  void dropEvent(QDropEvent*) override;
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
  // Onion skin: each ghost renders once into a tinted layer (widget pixels) reused while the
  // view, size and filters stay the same, so scrubbing between keys and hover repaints are cheap.
  struct GhostLayer { QPoint origin; QImage image; };
  QVector<Ghost> ghosts_;
  QSet<QString> ghostHidden_;
  QHash<quint64, GhostLayer> layers_;
  QTransform layerView_;
  QSize layerSize_;
  qreal layerDpr_ = 0;
  int ghostRenders_ = 0;
  QPolygonF trail_, trailKeys_;
  QString selectedIk_;
  // mesh editing state (editor-only)
  int meshMode_ = MeshOff, meshBone_ = -1;
  QString meshSlot_;
  QSet<int> meshSelected_, meshStroke_;
  double brushRadius_ = 24;
  enum class MeshGesture { None, Vertices, Band, Paint };
  MeshGesture meshGesture_ = MeshGesture::None;
  QPointF meshStart_, meshCurrent_, hover_;
  bool meshMoved_ = false, meshAdd_ = false;
  bool meshPress(QMouseEvent* e);
  void meshMove(const QPointF& widgetPos);
  void meshRelease(const QPointF& widgetPos);
  void paintMesh(QPainter& p);
  void paintGhosts(QPainter& p);
  void paintIk(QPainter& p);
  GhostLayer renderGhost(const Ghost& ghost) const;
};
}  // namespace cutout_studio
