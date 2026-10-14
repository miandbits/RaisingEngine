#include "route-tree.hpp"
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QGraphicsTextItem>
#include <QJsonDocument>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QScrollBar>
#include <QFontMetrics>
#include <cmath>

RouteTreeView::RouteTreeView(QWidget *parent):QGraphicsView(parent){
  setObjectName("analysisTree");setScene(new QGraphicsScene(this));
  setDragMode(ScrollHandDrag);setRenderHint(QPainter::Antialiasing);
  setBackgroundBrush(QColor("#1e262e"));
  connect(scene(),&QGraphicsScene::selectionChanged,this,[this]{
    if(drawing||scene()->selectedItems().empty())return;
    const auto key=scene()->selectedItems().first()->data(0).toString();
    if(!nodes.contains(key))return;const auto node=nodes.value(key);
    if(node.more)return;
    current=key;
    for(auto item:scene()->items())if(item->data(1)=="tree-node")
      if(auto card=qgraphicsitem_cast<QGraphicsRectItem*>(item))card->setPen(QPen(item->data(0)==current?QColor("#e9c46a"):QColor("#76a4b4"),item->data(0)==current?3:1));
    if(selected)selected(node.prefix,node.detail);
  });
}
void RouteTreeView::clearPathSelection(){
  current.clear();drawing=true;scene()->clearSelection();
  for(auto item:scene()->items())if(item->data(1)=="tree-node")
    if(auto card=qgraphicsitem_cast<QGraphicsRectItem*>(item))card->setPen(QPen(QColor("#76a4b4"),1));
  drawing=false;
}
void RouteTreeView::reset(int count){
  ++generation;nodes.clear();current.clear();Node root;root.label=tr("전체 회차 · 시작");root.count=count;
  root.detail=tr("실제로 관측된 행동·사건 순서를 공유한 회차만 같은 가지에 묶습니다.\n분기 후 다시 만난 사건도 서로 다른 가지로 유지합니다.");
  nodes.insert("$",root);resetTransform();redraw(true);if(count&&fetch)expand("$",0,2);
}
void RouteTreeView::expand(const QString &key,int offset,int automatic){
  if(!nodes.contains(key)||!fetch)return;auto &node=nodes[key];if(node.loading||node.terminal)return;
  if(node.loaded&&offset==0){node.expanded=true;redraw();return;}
  node.loading=true;node.expanded=true;redraw();const int epoch=generation;
  fetch(node.prefix,offset,[this,key,offset,automatic,epoch](QJsonObject page){
    if(epoch!=generation||!nodes.contains(key))return;auto &parent=nodes[key];parent.loading=false;
    if(page.contains("error")){parent.detail=page.value("error").toString();parent.loaded=false;redraw();return;}
    const auto received=page.value("children").toArray();
    if(key=="$"&&offset==0&&received.size()==1&&received.first().toObject().value("trail").toArray().size()==1&&received.first().toObject().value("kind")=="start"&&!received.first().toObject().value("terminal").toBool()){
      parent.prefix=received.first().toObject().value("prefix").toArray();parent.loaded=false;
      QTimer::singleShot(0,this,[this,epoch]{if(epoch==generation)expand("$",0,2);});return;
    }
    parent.loaded=true;parent.count=page.value("count").toInt();parent.total=page.value("totalBranches").toInt();
    if(offset==0)parent.children.clear();else parent.children.removeAll(key+"/more");
    QStringList expandNext;
    for(auto value:page.value("children").toArray()){
      const auto child=value.toObject();Node n;n.prefix=child.value("prefix").toArray();
      const auto childKey=QString::fromUtf8(QJsonDocument(n.prefix).toJson(QJsonDocument::Compact));
      n.label=child.value("label").toString();n.count=child.value("count").toInt();n.kind=child.value("kind").toString();n.ratio=child.value("observedPercent").toDouble();if(child.value("passed").isBool())n.kind=child.value("passed").toBool()?"success":"failure";n.depth=parent.depth+1;
      n.terminal=child.value("terminal").toBool();QStringList trail;for(auto step:child.value("trail").toArray())trail.append(step.toString());
      if(trail.size()>1)n.label+=tr("\n… %1단계 → %2").arg(trail.size()).arg(trail.last());
      n.detail=tr("이 전체 경로를 지난 회차 %1개 / 부모 %2개 (%3%)\n관측된 분기 비율이며 고정 성공 확률이 아닙니다.\n묶인 실제 경로:\n%4").arg(n.count).arg(parent.count).arg(child.value("observedPercent").toDouble(),0,'f',1).arg(trail.join(" → "));
      nodes.insert(childKey,n);parent.children.append(childKey);
      if(automatic>0&&!n.terminal)expandNext.append(childKey);
    }
    parent.offset=offset+page.value("children").toArray().size();
    if(parent.offset<parent.total){Node more;more.more=true;more.owner=key;more.depth=parent.depth+1;more.label=tr("추가 %1갈래 보기").arg(parent.total-parent.offset);nodes.insert(key+"/more",more);parent.children.append(key+"/more");}
    redraw(key=="$"||automatic>0);
    // Only open the shared trunk automatically. User expands divergent branches.
    if(expandNext.size()==1&&page.value("children").toArray().size()==1)QTimer::singleShot(0,this,[this,childKey=expandNext.first(),automatic,epoch]{if(epoch==generation)expand(childKey,0,automatic-1);});
  });
}
void RouteTreeView::toggle(const QString &key){
  if(!nodes.contains(key))return;const auto n=nodes.value(key);
  if(n.more){expand(n.owner,nodes.value(n.owner).offset);return;}
  if(n.terminal)return;
  if(n.expanded&&n.loaded){nodes[key].expanded=false;redraw();}else expand(key);
}
void RouteTreeView::collapseAll(){for(auto it=nodes.begin();it!=nodes.end();++it)it->expanded=false;nodes["$"].expanded=true;redraw(true);}
void RouteTreeView::fitTree(){fitInView(scene()->itemsBoundingRect().adjusted(-16,-16,16,16),Qt::KeepAspectRatio);}
QString RouteTreeView::keyAt(const QPoint &point)const{auto item=itemAt(point);while(item&&!item->data(0).isValid())item=item->parentItem();return item?item->data(0).toString():QString();}
void RouteTreeView::mousePressEvent(QMouseEvent *event){
  const auto key=keyAt(event->position().toPoint());
  if(event->button()==Qt::LeftButton&&nodes.contains(key)&&nodes.value(key).more){event->accept();return;}
  const bool reselect=event->button()==Qt::LeftButton&&!key.isEmpty()&&key==current;
  QGraphicsView::mousePressEvent(event);
  // An explicit click also reapplies the active path after other filters changed.
  if(reselect&&selected){const auto node=nodes.value(key);selected(node.prefix,node.detail);}
}
void RouteTreeView::mouseDoubleClickEvent(QMouseEvent *event){const auto key=keyAt(event->position().toPoint());if(!key.isEmpty()){toggle(key);event->accept();}else QGraphicsView::mouseDoubleClickEvent(event);}
void RouteTreeView::keyPressEvent(QKeyEvent *event){if(event->key()==Qt::Key_Return||event->key()==Qt::Key_Enter||event->key()==Qt::Key_Space){toggle(current);event->accept();}else QGraphicsView::keyPressEvent(event);}
void RouteTreeView::wheelEvent(QWheelEvent *event){if(event->modifiers()&Qt::ControlModifier){const double factor=event->angleDelta().y()>0?1.15:1/1.15;if(transform().m11()*factor>=0.12&&transform().m11()*factor<=2.5)scale(factor,factor);event->accept();}else QGraphicsView::wheelEvent(event);}
void RouteTreeView::redraw(bool center){
  drawing=true;scene()->clear();QMap<QString,QPointF> positions;double leaf=0;
  std::function<double(QString)> layout=[&](QString key){const auto n=nodes.value(key);double y;if(n.expanded&&!n.children.empty()){double first=0,last=0;for(int i=0;i<n.children.size();i++){const double child=layout(n.children[i]);if(i==0)first=child;last=child;}y=(first+last)/2;}else y=leaf++*92;positions[key]=QPointF(n.depth*340,y);return y;};
  if(nodes.contains("$"))layout("$");
  for(auto it=positions.begin();it!=positions.end();++it){const auto n=nodes.value(it.key());if(!n.expanded)continue;for(const auto &child:n.children){const auto a=it.value()+QPointF(270,39),b=positions.value(child)+QPointF(0,39);const double mid=a.x()+35;QPainterPath path(a);path.lineTo(mid,a.y());path.lineTo(mid,b.y());path.lineTo(b);auto edge=scene()->addPath(path,QPen(QColor("#729daf"),2));edge->setZValue(-1);edge->setAcceptedMouseButtons(Qt::NoButton);auto arrow=scene()->addPolygon(QPolygonF{b,b+QPointF(-8,-4),b+QPointF(-8,4)},QPen(Qt::NoPen),QBrush(QColor("#729daf")));arrow->setAcceptedMouseButtons(Qt::NoButton);}}
  for(auto it=positions.begin();it!=positions.end();++it){const auto key=it.key();const auto n=nodes.value(key);const QColor color=n.more?QColor("#3c414b"):n.terminal?QColor("#494161"):n.kind=="failure"?QColor("#593b3e"):n.kind=="success"?QColor("#2f5147"):QColor("#29434e");auto card=scene()->addRect(QRectF(0,0,270,78),QPen(key==current?QColor("#e9c46a"):QColor("#76a4b4"),key==current?3:1),QBrush(color));card->setPos(it.value());card->setFlag(QGraphicsItem::ItemIsSelectable,!n.more);card->setData(0,key);card->setData(1,"tree-node");card->setData(2,n.prefix.toVariantList());card->setData(3,n.count);card->setData(4,n.more);card->setToolTip(n.detail);if(key==current)card->setSelected(true);
    QString marker=n.more?QStringLiteral("＋ "):n.loading?QStringLiteral("… "):n.terminal?QStringLiteral("● "):n.expanded?QStringLiteral("− "):QStringLiteral("＋ ");
    auto text=new QGraphicsTextItem(card);text->setDefaultTextColor(Qt::white);text->setTextWidth(250);QStringList parts;const QFontMetrics metrics(text->font());for(const auto &part:n.label.split("\n"))parts.append(metrics.elidedText(part,Qt::ElideRight,228));QString title=parts.join("\n");text->setPlainText(marker+title+(n.more?QString():tr("\n%1회차 · 부모 대비 %2%").arg(n.count).arg(n.ratio,0,'f',1)));text->setPos(8,5);text->setAcceptedMouseButtons(Qt::NoButton);}
  scene()->setSceneRect(scene()->itemsBoundingRect().adjusted(-30,-30,30,30));drawing=false;
  if(center&&positions.contains("$"))centerOn(positions.value("$")+QPointF(330,39));else if(positions.contains(current))centerOn(positions.value(current)+QPointF(330,39));
}
