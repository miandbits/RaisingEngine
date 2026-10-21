#pragma once
#include <QGraphicsView>
#include <QJsonObject>
#include <QMap>
#include <QRectF>
#include <QSet>
#include <QStringList>
#include <functional>

struct ScriptGraphLayout {
  QMap<QString, QRectF> rectangles;
  QStringList order;
  QSet<QString> unreachable;
};
ScriptGraphLayout layoutScriptGraph(const QJsonObject &script);

class ScriptGraphView final : public QGraphicsView {
public:
  using QGraphicsView::QGraphicsView;
  std::function<void()> viewChanged;
  void zoomBy(double factor);
protected:
  void wheelEvent(QWheelEvent *event) override;
};

class ScriptGraphOverview final : public QGraphicsView {
public:
  explicit ScriptGraphOverview(QGraphicsView *target, QWidget *parent = nullptr);
protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
private:
  QGraphicsView *target;
  void navigate(QPoint point);
};
