#pragma once
#include <QGraphicsView>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QPixmap>
#include <functional>

class ScreenCanvas : public QGraphicsView {
  Q_OBJECT
public:
  explicit ScreenCanvas(QWidget *parent = nullptr);
  ~ScreenCanvas() override { scene.blockSignals(true); }
  void setScreen(const QJsonObject &project, const QJsonObject &screen,
                 const QJsonArray &assets, const QString &selected,
                 bool fit = false);
  void select(const QString &id);
  void selectMany(const QStringList &ids);
  QStringList selectedIds() const;
  void startSelectionDrag();
  void previewSelectionDrag(QPoint delta);
  void finishSelectionDrag();
  void fitCanvas();
  void setRuntimePreview(const QJsonObject& ui,const QImage& image);
  void clearRuntimePreview();
  bool hasRuntimePreview() const {return runtimeActive;}
  QJsonObject runtimeResult() const {return runtimeUi;}
  bool snap = false;
  /** Kind "plugin": empty caption means the owning plugin may paint; otherwise draw a placeholder. */
  std::function<QString(const QJsonObject &)> pluginPlaceholder;
  /** Kind "plugin": returns false to fall back to the host placeholder. */
  std::function<bool(QPainter *, const QJsonObject &, const QRectF &)> pluginPaint;
signals:
  void selectionChanged(QString id);
  void selectionsChanged(QStringList ids);
  void selectionMoved(QPoint delta);
  void geometryEdited(QString id, QRect rectangle);
  void deleteSelected();
  void duplicateSelected();

protected:
  void wheelEvent(QWheelEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void keyReleaseEvent(QKeyEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  QGraphicsScene scene;
  QJsonArray sourceElements;
  QMap<QGraphicsItem*,QPointF> dragStart;
  QPoint dragDelta;
  QSize canvasSize{1280, 720};
  QMap<QString, QPixmap> imageCache;
  bool refreshing = false, autoFit = true, panning = false;
  bool runtimeActive=false;
  QJsonObject runtimeUi,cachedProject,cachedScreen;
  QJsonArray cachedAssets;
};
