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
  /** Create (plugin 1.5.0, C): press-drag draws a new child bone of the selection (Drag::ref). */
  enum Tool { Move = 0, Rotate = 1, Scale = 2, Pivot = 3, Create = 4 };
  static constexpr int TOOL_COUNT = 5;
  struct Drag {
    QString ref;            // "bone:<id>" | "slot:<id>" (Create: the selection at the press = the parent)
    int tool = Move;        // Move/Rotate tools pick per press: bone origin = Move, bone body = Rotate
    QPointF origin, start, current;  // rig space; origin = manipulation center
    double angle = 0;       // accumulated clockwise degrees (multi-turn safe)
    double ratio = 1;       // |current-origin| / |start-origin|
    bool shift = false, moved = false;
    bool body = false;      // the press grabbed a bone's body (length shape / axis): rotates around its origin
  };
  /** Create tool: where the new bone's tip is (rig space), Shift snapping the angle to 15°. */
  static QPointF createEnd(const Drag& d);
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
  const cutout::Pose& pose() const { return pose_; }  // what the canvas draws (tests, overlays)
  void setImages(ImageCache* images) { images_ = images; }
  void setHiddenSlots(const QSet<QString>& hidden) { hidden_ = hidden; refreshHover(); update(); }
  void setSelection(const QString& ref) { selection_ = ref; refreshHover(); update(); }
  /** Multi-selection (plugin 1.7.0): every selected ref is highlighted; setSelection() stays the
   *  primary item (its gizmo, Alt-cycling start). Editor-only, owned by the panel. */
  void setMultiSelection(const QSet<QString>& refs) { multi_ = refs; update(); }
  bool isSelected(const QString& ref) const { return !ref.isEmpty() && (ref == selection_ || multi_.contains(ref)); }
  /** Box selection in progress (widget px, Move tool drag on empty space). */
  bool boxSelecting() const { return band_.has_value(); }
  /** Bones whose origin lies inside `widgetRect` (rig order). */
  QStringList bonesIn(const QRectF& widgetRect) const;
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
  /** Physics (plugin 1.13.0): physics bones get a teal spring mark at their tip, `selected` thicker;
   *  `rest` (bone index -> rig point) = the unsimulated tips while preview playback simulates. */
  void setPhysicsView(const QString& selected, const QHash<int, QPointF>& rest) { selectedPhysics_ = selected; physicsRest_ = rest; update(); }
  const QHash<int, QPointF>& physicsRest() const { return physicsRest_; }
  static QColor physicsColor();
  /** Workspace UI scale (plugin 1.10.0, editor-only): handles, gizmos, pick radii and the hint
   *  line grow with it, so a large-text workspace also gets larger targets. */
  void setUiScale(double scale) { ui_ = scale; refreshHover(); update(); }
  double uiScale() const { return ui_; }
  /** Fit the authoring canvas into the widget (also re-enables auto fit on resize). */
  void fitToCanvas();
  QTransform viewTransform() const;
  QPointF widgetToRig(const QPointF& p) const;
  QPointF rigToWidget(const QPointF& p) const;
  /** Something pickable under the cursor. Handles beat image parts (Spine-like): a crosshair or
   *  bone origin drawn over a body image is what the user aims at, the image is only behind it. */
  enum HitRank { HitVertex = 0, HitIkTarget = 1, HitSelectedBone = 2, HitBone = 3, HitBoneBody = 4, HitPart = 5 };
  struct Hit {
    QString ref;      // "bone:<id>" | "slot:<id>" (a vertex: the edited mesh's slot)
    int vertex = -1;  // mesh vertex index (vertex mode only)
    int rank = HitPart;
    double distance = 0;  // handles/bodies: widget px from the handle/bone line; parts: depth (0 = topmost)
    bool body = false;    // a bone grabbed away from its origin (length shape, selected axis): drags rotate
  };
  /** THE canvas hit test (click-select, drag start, hover and drops all use it): every candidate
   *  under `widgetPos`, best first — rank, then nearest/topmost. handles=false: parts only.
   *  Editor-hidden parts (eye toggle) are never candidates. */
  QVector<Hit> hitCandidates(const QPointF& widgetPos, bool handles = true) const;
  /** Ref under a widget point (the best candidate, vertices aside); bones=false = parts only. */
  QString hitTest(const QPointF& widgetPos, bool bones = true) const;
  /** What a press picks: the best candidate, or with `cycle` (Alt) the one after the current
   *  selection in candidate order (wrapping), so repeated Alt+clicks walk through a stack. */
  QString pick(const QPointF& widgetPos, bool cycle) const { return pickHit(widgetPos, cycle).ref; }
  /** pick() with where on the item the press landed (the best candidate of that ref). */
  Hit pickHit(const QPointF& widgetPos, bool cycle) const;
  /** Drawn body of bone `bone` (plugin 1.5.0, widget px): a tapered diamond from its origin to its
   *  tip (length along +x). Empty for point bones, IK targets and bones shorter than 6 px on screen. */
  QPolygonF boneShape(int bone) const;
  /** Rig-space tip of bone `bone` in the shown pose (its origin for a point bone). */
  QPointF boneTip(int bone) const;
  /** Ref the cursor hovers (what a click would pick; highlighted on the canvas). */
  QString hoverRef() const { return hoverRef_; }
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
  /** Vertex handle under a widget point (-1 = none; vertex mode: the best hitCandidates vertex). */
  int vertexAt(const QPointF& widgetPos) const;
  /** Click/band selection (add = Shift). An empty click clears. */
  std::function<void(const QVector<int>& vertices, bool add)> onMeshSelect;
  /** Drag of the selected vertices by `rigDelta` (live while !finished; one edit on finish). */
  std::function<void(const QPointF& rigDelta, bool finished)> onMeshDrag;
  /** Weight brush stroke: every vertex the brush touched so far (one edit on finish). */
  std::function<void(const QVector<int>& vertices, bool finished)> onMeshPaint;

  // ---- Mesh shape tools (plugin 1.6.0, setup mode). In vertex mode `meshTool` picks what a left
  // press does; outline drawing works on a plain image too (it is how a freeform mesh is made).
  enum MeshTool { MeshSelect = 0, MeshAddVertex = 1, MeshAddEdge = 2 };
  void setMeshTool(int tool) { meshTool_ = tool; meshGesture_ = MeshGesture::None; update(); }
  int meshTool() const { return meshTool_; }
  /** 윤곽 그리기 on `slot` ("" = off): clicks place outline points (rig space); a click on the first
   *  point, a double-click or Enter closes it (onMeshOutline, >= 3 points); Backspace drops the
   *  last point, Esc cancels (onOutlineCancel). */
  void setOutlineDrawing(const QString& slot);
  bool outlineDrawing() const { return !outlineSlot_.isEmpty(); }
  const QPolygonF& outlineDraft() const { return outline_; }
  /** Dashed outline shown before 자동 윤곽 is applied (rig space; empty = none). */
  void setOutlinePreview(const QPolygonF& rigPolygon) { outlinePreview_ = rigPolygon; update(); }
  const QPolygonF& outlinePreview() const { return outlinePreview_; }
  std::function<void(const QPolygonF& rigPoints)> onMeshOutline;
  std::function<void()> onOutlineCancel;
  /** Clipping polygons (plugin 1.12.0): every shown clipping slot's outline, dashed (editor-only toggle). */
  void setShowClipOutlines(bool on) { showClips_ = on; update(); }
  bool showClipOutlines() const { return showClips_; }
  /** 정점 추가: a click (rig space); 가장자리 추가: a drag from vertex `a` to vertex `b`. */
  std::function<void(const QPointF& rigPoint)> onMeshAddVertex;
  std::function<void(int a, int b)> onMeshAddEdge;
  /** Delete / Backspace in vertex mode (the panel deletes the selected vertices). */
  std::function<void()> onMeshDelete;

  std::function<void(const QString&)> onSelect;
  /** Ctrl/Shift+click on an item: toggle it in the multi-selection (no drag starts). */
  std::function<void(const QString& ref)> onToggleSelect;
  /** Box select released: `refs` = bones inside; add = Ctrl/Shift held at the press. */
  std::function<void(const QStringList& refs, bool add)> onBoxSelect;
  /** Ctrl+A (all bones) while the canvas has focus (Ctrl+Shift+A stays the host's workspace toggle). */
  std::function<void()> onSelectAll;
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
  void mouseDoubleClickEvent(QMouseEvent*) override;
  void wheelEvent(QWheelEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
  void keyReleaseEvent(QKeyEvent*) override;
  void leaveEvent(QEvent*) override;
  bool event(QEvent*) override;

 private:
  std::shared_ptr<const cutout::Rig> rig_;
  cutout::Pose pose_;
  ImageCache* images_ = nullptr;
  QSet<QString> hidden_;
  QString selection_, message_;
  QSet<QString> multi_;
  std::optional<QRectF> band_;  // box selection (widget px)
  bool bandAdd_ = false;
  QString pendingSingle_;       // press on a member of a multi-selection: a click (no drag) keeps only it
  int tool_ = Move;
  bool animate_ = false, autoFit_ = true;
  double zoom_ = 1, ui_ = 1;
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
  QString selectedPhysics_;
  QHash<int, QPointF> physicsRest_;
  // hover: what a click at the cursor would pick (Alt held = the cycled pick)
  QString hoverRef_;
  QPointF hoverAt_;
  bool hoverInside_ = false, hoverAlt_ = false, hoverBody_ = false;
  void updateHover(const QPointF& widgetPos, bool alt);
  void refreshHover() { if (hoverInside_) updateHover(hoverAt_, hoverAlt_); }
  void paintHover(QPainter& p);
  void outlineSlot(QPainter& p, int slotIndex) const;
  void paintBones(QPainter& p);
  QColor boneColor(int bone) const;
  // mesh editing state (editor-only)
  int meshMode_ = MeshOff, meshBone_ = -1;
  QString meshSlot_;
  QSet<int> meshSelected_, meshStroke_;
  double brushRadius_ = 24;
  enum class MeshGesture { None, Vertices, Band, Paint, Edge };
  MeshGesture meshGesture_ = MeshGesture::None;
  QPointF meshStart_, meshCurrent_, hover_;
  bool meshMoved_ = false, meshAdd_ = false;
  int meshTool_ = MeshSelect, edgeFrom_ = -1;
  QString outlineSlot_;
  QPolygonF outline_, outlinePreview_;
  bool outlinePress(QMouseEvent* e);
  void finishOutline();
  void paintOutline(QPainter& p);
  bool meshPress(QMouseEvent* e);
  void meshMove(const QPointF& widgetPos);
  void meshRelease(const QPointF& widgetPos);
  void paintMesh(QPainter& p);
  void paintGhosts(QPainter& p);
  void paintIk(QPainter& p);
  void paintPhysics(QPainter& p);
  bool showClips_ = true;
  void paintClips(QPainter& p);
  GhostLayer renderGhost(const Ghost& ghost) const;
};
}  // namespace cutout_studio
