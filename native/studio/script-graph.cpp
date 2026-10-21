#include "script-graph.hpp"
#include <QGraphicsScene>
#include <QJsonArray>
#include <QMouseEvent>
#include <QPainter>
#include <QQueue>
#include <QScrollBar>
#include <QWheelEvent>
#include <algorithm>

namespace {
QStringList destinations(QJsonObject node) {
  QStringList out;
  const auto type = node.value("type").toString();
  if (type == "branch") out << node.value("then").toString() << node.value("else").toString();
  else if (type == "choice") for (auto value : node.value("choices").toArray()) out << value.toObject().value("next").toString();
  else if (type != "end") out << node.value("next").toString();
  out.removeAll(QString()); out.removeDuplicates(); return out;
}
}

ScriptGraphLayout layoutScriptGraph(const QJsonObject &script) {
  ScriptGraphLayout result;
  QMap<QString, QJsonObject> nodes;
  for (auto value : script.value("nodes").toArray()) {
    auto node = value.toObject(); const auto id = node.value("id").toString();
    if (!nodes.contains(id)) result.order << id;
    nodes[id] = node;
  }
  // Breadth-first discovery gives siblings a stable choice order, independent of
  // serialized node order. Longest-path ranks keep merges below BOTH branches.
  QStringList discovery;
  QSet<QString> seen;
  const auto entry = script.value("entryNodeId").toString();
  if (nodes.contains(entry)) { discovery << entry; seen.insert(entry); }
  for (int i = 0; i < discovery.size(); ++i)
    for (auto next : destinations(nodes[discovery[i]])) if (nodes.contains(next) && !seen.contains(next)) { seen.insert(next); discovery << next; }
  for (auto id : result.order) if (!seen.contains(id)) { result.unreachable.insert(id); discovery << id; }
  result.order = discovery;
  QMap<QString, int> incoming, rank;
  QMap<QString, QStringList> parents;
  for (auto id : discovery) {
    incoming[id] += 0;
    for (auto next : destinations(nodes[id])) if (nodes.contains(next)) { ++incoming[next]; parents[next] << id; }
  }
  QQueue<QString> ready;
  for (auto id : discovery) if (!incoming[id]) ready.enqueue(id);
  QSet<QString> processed;
  while (!ready.isEmpty()) {
    auto id = ready.dequeue(); processed.insert(id);
    for (auto next : destinations(nodes[id])) if (nodes.contains(next)) {
      rank[next] = std::max(rank.value(next), rank.value(id) + 1);
      if (--incoming[next] == 0) ready.enqueue(next);
    }
  }
  int last = 0; for (auto r : rank) last = std::max(last, r);
  // Invalid cycles remain visible and finite so the author can repair them.
  for (auto id : discovery) if (!processed.contains(id)) rank[id] = ++last;
  QMap<int, QStringList> levels;
  for (auto id : discovery) levels[rank.value(id)] << id;
  QMap<QString, double> x;
  for (auto it = levels.begin(); it != levels.end(); ++it) {
    auto layer = it.value();
    auto desired = [&](QString id) { double sum=0; int count=0; for(auto p:parents[id])if(x.contains(p)){sum+=x[p];++count;}return count?sum/count:0.; };
    std::stable_sort(layer.begin(), layer.end(), [&](QString a, QString b){return desired(a)<desired(b);});
    double previous = -1e9, wantedSum = 0, actualSum = 0;
    QMap<QString,double> positions;
    for (int i=0;i<layer.size();++i) {
      const auto id=layer[i]; const auto wanted=desired(id);const auto px=std::max(previous+310.,wanted);
      positions[id]=px;previous=px;wantedSum+=wanted;actualSum+=px;
    }
    const double shift=(wantedSum-actualSum)/layer.size();
    for(auto id:layer){x[id]=positions[id]+shift;result.rectangles[id]=QRectF(x[id],it.key()*185.,270,112);}
  }
  return result;
}

void ScriptGraphView::zoomBy(double factor) {
  const double current=transform().m11(), next=std::clamp(current*factor,0.02,2.5);
  scale(next/current,next/current); if(viewChanged)viewChanged();
}
void ScriptGraphView::wheelEvent(QWheelEvent *event) {
  if(event->modifiers()&Qt::ControlModifier){zoomBy(event->angleDelta().y()>0?1.15:1/1.15);event->accept();}
  else QGraphicsView::wheelEvent(event);
}
ScriptGraphOverview::ScriptGraphOverview(QGraphicsView *view,QWidget *parent):QGraphicsView(parent),target(view){
  setScene(view->scene()); setObjectName("scriptGraphOverview");setAccessibleName(tr("전체 지도 · 클릭하여 이동"));
  setFixedSize(180,118);setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setInteractive(false);setRenderHint(QPainter::Antialiasing);setBackgroundBrush(QColor("#17232a"));
  connect(view->horizontalScrollBar(),&QScrollBar::valueChanged,this,[this]{viewport()->update();});
  connect(view->verticalScrollBar(),&QScrollBar::valueChanged,this,[this]{viewport()->update();});
  connect(scene(),&QGraphicsScene::sceneRectChanged,this,[this]{fitInView(sceneRect(),Qt::KeepAspectRatio);});
}
void ScriptGraphOverview::resizeEvent(QResizeEvent *event){QGraphicsView::resizeEvent(event);fitInView(sceneRect(),Qt::KeepAspectRatio);}
void ScriptGraphOverview::paintEvent(QPaintEvent *event){
  QGraphicsView::paintEvent(event);QPainter painter(viewport());painter.setPen(QPen(QColor("#f3c779"),2));painter.setBrush(QColor(243,199,121,35));
  painter.drawPolygon(mapFromScene(target->mapToScene(target->viewport()->rect())));
}
void ScriptGraphOverview::navigate(QPoint point){target->centerOn(mapToScene(point));viewport()->update();}
void ScriptGraphOverview::mousePressEvent(QMouseEvent *event){if(event->button()==Qt::LeftButton)navigate(event->position().toPoint());}
void ScriptGraphOverview::mouseMoveEvent(QMouseEvent *event){if(event->buttons()&Qt::LeftButton)navigate(event->position().toPoint());}
