#pragma once
#include <QGraphicsView>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <functional>

class RouteTreeView final : public QGraphicsView {
  Q_OBJECT
public:
  using Reply=std::function<void(QJsonObject)>;
  std::function<void(QJsonArray,int,Reply)> fetch;
  std::function<void(QJsonArray,QString)> selected;
  explicit RouteTreeView(QWidget *parent=nullptr);
  void reset(int count);
  void clearPathSelection();
  void collapseAll();
  void fitTree();
protected:
  void wheelEvent(QWheelEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
private:
  struct Node {
    QJsonArray prefix;QString label,detail,kind;QStringList children;
    int count=0,total=0,offset=0,depth=0;double ratio=100;
    bool expanded=false,loaded=false,loading=false,terminal=false,more=false;
    QString owner;
  };
  QMap<QString,Node> nodes;
  QString current;
  bool drawing=false;
  int generation=0;
  void expand(const QString &key,int offset=0,int automatic=0);
  void toggle(const QString &key);
  void redraw(bool center=false);
  QString keyAt(const QPoint &point) const;
};
