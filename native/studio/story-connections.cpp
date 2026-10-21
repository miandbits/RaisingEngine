#include "story-connections.hpp"
#include "script-identity.hpp"
#include "script-graph.hpp"
#include <QComboBox>
#include <QCheckBox>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QHeaderView>
#include <QPainterPath>
#include <QJsonDocument>
#include <QSignalBlocker>
#include <QTimer>
#include <algorithm>
#include <memory>

namespace {
QString name(QJsonValue v) { return v.isString()?v.toString():v.toObject().value("fallback").toString(); }
QString text(const QJsonObject &d) { auto s=name(d.value("label"));if(s.isEmpty())s=name(d.value("name"));return s.isEmpty()?d.value("id").toString():s; }
QString atom(const QJsonObject &o) {
  const auto op=o.value("op").toString();
  if(op=="flag"||op=="setFlag")return "flag:"+o.value("flagId").toString()+":"+(o.value(op=="flag"?"equals":"value").toBool()?"true":"false");
  if(op=="stateIs"||op=="setState")return "state:"+o.value("stateGroupId").toString()+":"+o.value("stateValueId").toString();
  return {};
}
QStringList links(const QJsonObject &n) {
  QStringList result;for(const auto &key:{"next","then","else"})if(n.value(key).isString())result<<n.value(key).toString();
  for(auto c:n.value("choices").toArray())result<<c.toObject().value("next").toString();return result;
}
struct Producer {QString node,detail;};
struct Consumer {QString node;QJsonObject condition;QString path;};
QString kindName(QString kind) {
  if(kind=="event")return QObject::tr("사건");if(kind=="ending")return QObject::tr("결말");
  if(kind=="choice")return QObject::tr("선택");if(kind=="activity")return QObject::tr("활동");
  if(kind=="location")return QObject::tr("장소");return QObject::tr("행동 · 교류");
}
}

StoryConnections buildStoryConnections(const QJsonArray &documents) {
  StoryConnections result;
  QMap<QString,QList<Producer>> producers;
  QList<Consumer> consumers;
  QMap<QString,QString> events,atomNames;
  QSet<QString> edgeKeys;
  auto edge=[&](QString from,QString to,QString why,bool conditional=false){
    if(from.isEmpty()||to.isEmpty()||!result.nodes.contains(from)||!result.nodes.contains(to))return;
    const auto key=from+"\n"+to+"\n"+why;if(edgeKeys.contains(key))return;edgeKeys.insert(key);
    result.edges.append({from,to,why,conditional});
  };
  auto add=[&](QString id,QString kind,QJsonObject d,QString file,QString detail=QString(),QString node=QString(),QString choice=QString()){
    result.nodes[id]={id,kind,text(d),detail,file,node,choice};
  };
  auto effects=[&](QJsonArray a,QString source,QString detail){
    for(auto v:a){auto e=v.toObject();auto key=atom(e);if(!key.isEmpty())producers[key].append({source,detail});}
  };
  QJsonArray scripts;
  for(auto v:documents){const auto doc=v.toObject(),d=doc.value("data").toObject();const auto kind=doc.value("kind").toString();
    if(kind=="event"||kind=="ending"){scripts.append(doc);auto id=kind+":"+d.value("id").toString();add(id,kind,d,doc.value("file").toString());if(kind=="event")events[d.value("id").toString()]=id;}
    if(kind=="flags")for(auto f:d.value("flags").toArray()){auto flag=f.toObject();auto label=text(flag);atomNames["flag:"+flag.value("id").toString()+":true"]=label+" = true";atomNames["flag:"+flag.value("id").toString()+":false"]=label+" = false";}
    if(kind=="project")for(auto g:d.value("stateGroups").toArray())for(auto val:g.toObject().value("values").toArray())atomNames["state:"+g.toObject().value("id").toString()+":"+val.toObject().value("id").toString()]=text(g.toObject())+" / "+text(val.toObject());
  }
  for(auto v:documents){const auto doc=v.toObject(),d=doc.value("data").toObject();const auto kind=doc.value("kind").toString(),file=doc.value("file").toString();
    if(kind=="activity"){
      auto id="activity:"+d.value("id").toString();add(id,kind,d,file,name(d.value("description")));
      effects(d.value("baseEffects").toArray(),id,QObject::tr("활동 효과"));
      for(auto outcome:d.value("outcomes").toArray())effects(outcome.toObject().value("effects").toArray(),id,QObject::tr("성공 판정에 따른 효과"));
      consumers.append({id,d.value("requirements").toObject(),QObject::tr("활동 조건")});
    }
    if(kind=="project"){
      const auto gameplay=d.value("gameplay").toObject();
      for(auto lv:gameplay.value("locations").toArray()){auto l=lv.toObject();add("location:"+l.value("id").toString(),"location",l,file);}
      for(const auto &collection:{"actions","interactions"})for(auto av:gameplay.value(collection).toArray()){
        auto a=av.toObject();auto id=QString(collection)+":"+a.value("id").toString();add(id,"action",a,file);
        edge("location:"+a.value("locationId").toString(),id,QObject::tr("이 장소에서 교류"));
        edge(id,events.value(a.value("eventId").toString()),QObject::tr("행동 실행 → 사건 시작"),true);
        effects(a.value("effects").toArray(),id,QObject::tr("행동 효과"));consumers.append({id,a.value("requirements").toObject(),QObject::tr("행동 조건")});
      }
    }
  }
  for(auto v:scripts){auto doc=v.toObject(),d=doc.value("data").toObject();auto id=doc.value("kind").toString()+":"+d.value("id").toString(),file=doc.value("file").toString();
    result.nodes[id].detail=scriptIdentity(doc,scripts,documents);
    consumers.append({id,d.value("requirements").toObject(),QObject::tr("발생 조건")});
    QMap<QString,QJsonObject> nodes;for(auto nv:d.value("nodes").toArray())nodes[nv.toObject().value("id").toString()]=nv.toObject();
    QSet<QString> reachable;QStringList queue{d.value("entryNodeId").toString()};
    for(int i=0;i<queue.size();++i){auto key=queue[i];if(reachable.contains(key)||!nodes.contains(key))continue;reachable.insert(key);queue.append(links(nodes[key]));}
    for(auto it=nodes.begin();it!=nodes.end();++it){if(!reachable.contains(it.key()))continue;const auto n=it.value();
      // Branch predicates are internal script gates, not event activation requirements.
      if(n.value("type")=="choice")for(auto cv:n.value("choices").toArray()){
        auto c=cv.toObject();auto cid=id+"/"+it.key()+"/"+c.value("id").toString();
        auto title=c;title["label"]=c.value("text");add(cid,"choice",title,file,text(d)+"\n"+name(c.value("consequenceText")),it.key(),c.value("id").toString());
        edge(id,cid,QObject::tr("대본 안의 선택 · 실행 경로에 따름"),true);
        effects(c.value("effects").toArray(),cid,name(c.value("consequenceText")));
        consumers.append({cid,c.value("requirements").toObject(),QObject::tr("선택 표시 조건")});
      }
    }
    // Carry the nearest choice through subsequent effect nodes, including branches and merges.
    // A visited (node, origin) pair also bounds malformed cycles without dropping other origins.
    QList<QPair<QString,QString>> paths{{d.value("entryNodeId").toString(),id}};QSet<QString> visited;
    for(int i=0;i<paths.size();++i){const auto [key,origin]=paths[i];const auto pair=key+"\n"+origin;if(visited.contains(pair)||!nodes.contains(key))continue;visited.insert(pair);const auto n=nodes[key];
      effects(n.value("effects").toArray(),origin,QObject::tr("대본 효과 · %1").arg(key));
      if(n.value("type")=="choice")for(auto cv:n.value("choices").toArray()){auto c=cv.toObject();paths.append({c.value("next").toString(),id+"/"+key+"/"+c.value("id").toString()});}
      else for(const auto &next:links(n))paths.append({next,origin});
    }
  }
  std::function<void(const Consumer&,QJsonObject,QString,bool,bool)> scan;
  scan=[&](const Consumer &c,QJsonObject condition,QString path,bool alternative,bool negated){
    const auto op=condition.value("op").toString();
    if(op=="all"||op=="any"){
      const auto group=op=="all"?QObject::tr("모두 충족 AND"):QObject::tr("하나 이상 OR");int i=0;
      const bool alternatives=negated?op=="all":op=="any";
      for(auto child:condition.value("conditions").toArray())scan(c,child.toObject(),path+" / "+group+" "+QString::number(++i),alternative||alternatives,negated);return;
    }
    if(op=="not"){scan(c,condition.value("condition").toObject(),path+" / "+QObject::tr("반전 NOT"),alternative,!negated);return;}
    QString role=negated?QObject::tr("배제 조건"):alternative?QObject::tr("대안 조건"):QObject::tr("필수 조건");
    if(op=="eventCompleted")edge(events.value(condition.value("eventId").toString()),c.node,role+" · "+path,alternative||negated);
    auto key=atom(condition);if(key.isEmpty())return;
    auto label=atomNames.value(key,key.mid(key.indexOf(':')+1));
    for(const auto &p:producers.value(key))edge(p.node,c.node,role+" · "+label+"\n"+p.detail+"\n"+path,true);
  };
  for(const auto &c:consumers)scan(c,c.condition,c.path,false,false);
  return result;
}

QSet<QString> storyConnectionNeighborhood(const StoryConnections &graph,const QString &focus,int depth) {
  if(!graph.nodes.contains(focus))return {};
  QSet<QString> seen{focus},frontier{focus};
  // Traverse upstream and downstream separately: sharing a gate does not make two events successors.
  for(bool forward:{false,true}){frontier={focus};QSet<QString> directionSeen{focus};
    for(int d=0;d<depth&&!frontier.isEmpty();++d){QSet<QString> next;
      for(const auto &e:graph.edges){auto from=forward?e.from:e.to,to=forward?e.to:e.from;if(frontier.contains(from)&&!directionSeen.contains(to))next.insert(to);}
      seen.unite(next);directionSeen.unite(next);frontier=next;
    }
  }return seen;
}

StoryConnectionsDialog::StoryConnectionsDialog(QJsonObject context,QString file,QWidget *parent,
    std::function<QJsonObject(QString,QString,QString,QWidget*)> editConditions):QDialog(parent) {
  setObjectName("storyConnectionsDialog");setWindowTitle(tr("사건 연결 지도"));resize(1380,900);
  auto outer=new QVBoxLayout(this);auto help=new QLabel(tr("활동·장소 → 사건·선택 → 약속·상태 조건 → 후속 사건. 점선은 실행 경로와 다른 조건에 따라 달라지는 연결입니다. 도달 가능성은 플레이 검증으로 확인하세요."));help->setWordWrap(true);outer->addWidget(help);
  auto row=new QHBoxLayout;auto search=new QLineEdit;search->setObjectName("connectionsSearch");search->setStyleSheet("QLineEdit { placeholder-text-color: #a5b3bb; }");search->setPlaceholderText(tr("사건·인물·활동·장소 검색"));auto focus=new QComboBox;focus->setObjectName("connectionsFocus");focus->setMinimumWidth(360);auto depth=new QComboBox;depth->setObjectName("connectionsDepth");for(int i=1;i<=4;++i)depth->addItem(tr("%1단계 연결").arg(i),i);depth->setCurrentIndex(1);auto all=new QCheckBox(tr("프로젝트 전체"));row->addWidget(search,1);row->addWidget(focus,1);row->addWidget(depth);row->addWidget(all);outer->addLayout(row);
  auto split=new QSplitter;outer->addWidget(split,1);auto graph=new ScriptGraphView;graph->setObjectName("connectionsGraph");auto scene=new QGraphicsScene(graph);graph->setScene(scene);graph->setBackgroundBrush(QColor("#17232a"));graph->setRenderHint(QPainter::Antialiasing);graph->setDragMode(QGraphicsView::ScrollHandDrag);split->addWidget(graph);
  auto side=new QWidget;auto sideLayout=new QVBoxLayout(side);auto details=new QLabel;details->setObjectName("connectionsDetails");details->setTextFormat(Qt::PlainText);details->setWordWrap(true);sideLayout->addWidget(details);auto reasons=new QTreeWidget;reasons->setObjectName("connectionsReasons");reasons->setHeaderLabels({tr("연결 대상 / 조건")});reasons->setWordWrap(true);sideLayout->addWidget(reasons,1);
  auto condition=new QPushButton(tr("선택한 사건·선택의 조건 편집…"));condition->setObjectName("connectionsConditions");auto script=new QPushButton(tr("선택한 대본 편집…"));script->setObjectName("connectionsScript");auto center=new QPushButton(tr("선택한 항목을 중심으로 보기"));center->setObjectName("connectionsCenter");sideLayout->addWidget(condition);sideLayout->addWidget(script);sideLayout->addWidget(center);split->addWidget(side);split->setSizes({990,350});
  auto bottom=new QHBoxLayout;auto overview=new ScriptGraphOverview(graph);bottom->addWidget(overview);auto info=new QLabel;info->setObjectName("connectionsCount");info->setWordWrap(true);bottom->addWidget(info,1);auto fit=new QPushButton(tr("흐름 전체 보기"));bottom->addWidget(fit);auto close=new QPushButton(tr("이야기 편집기로 돌아가기"));bottom->addWidget(close);outer->addLayout(bottom);
  struct State {QJsonObject context;StoryConnections model;QString selected;QMap<QString,QGraphicsRectItem*> items;};auto state=std::make_shared<State>();state->context=context;state->model=buildStoryConnections(context.value("documents").toArray());
  auto selection=[=](QString id){state->selected=id;const auto n=state->model.nodes.value(id);details->setText(kindName(n.kind)+" · "+n.title+"\n"+n.detail);reasons->clear();
    for(const auto &e:state->model.edges)if(e.from==id||e.to==id){const auto other=e.from==id?e.to:e.from;auto item=new QTreeWidgetItem(reasons,{(e.from==id?tr("다음 → "):tr("이전 ← "))+state->model.nodes.value(other).title+"\n"+e.reason});item->setData(0,Qt::UserRole,other);item->setToolTip(0,e.reason);item->setSizeHint(0,QSize(300,110));}
    const bool editable=n.kind=="event"||n.kind=="ending"||n.kind=="choice";condition->setEnabled(editable);script->setEnabled(editable);center->setEnabled(!id.isEmpty());
    for(auto it=state->items.begin();it!=state->items.end();++it)it.value()->setPen(QPen(it.key()==id?QColor("#ffdc94"):QColor("#607782"),it.key()==id?3:1));
  };
  auto render=[=]{QSignalBlocker blocker(scene);state->items.clear();scene->clear();const auto root=focus->currentData().toString();QSet<QString> visible;if(all->isChecked())for(auto it=state->model.nodes.begin();it!=state->model.nodes.end();++it)visible.insert(it.key());else visible=storyConnectionNeighborhood(state->model,root,depth->currentData().toInt());
    // Stable shortest-distance columns, with incoming paths on the left. Cycles stay finite.
    QMap<QString,int> ranks;QStringList queue{root};ranks[root]=0;
    for(bool forward:{false,true}){queue={root};QSet<QString> seen{root};for(int i=0;i<queue.size();++i)for(const auto &e:state->model.edges){auto a=forward?e.from:e.to,b=forward?e.to:e.from;if(a!=queue[i]||!visible.contains(b)||seen.contains(b))continue;seen.insert(b);queue<<b;if(!ranks.contains(b))ranks[b]=ranks[a]+(forward?1:-1);}}
    QMap<int,QStringList> columns;for(auto id:visible)columns[ranks.value(id,3)]<<id;for(auto it=columns.begin();it!=columns.end();++it){auto ids=it.value();std::sort(ids.begin(),ids.end());int r=0;for(auto id:ids){const auto n=state->model.nodes.value(id);QRectF rect(it.key()*360,r++*156,285,126);QColor color=n.kind=="choice"?QColor("#49374f"):n.kind=="activity"?QColor("#354938"):n.kind=="location"?QColor("#494432"):QColor("#24404a");auto card=scene->addRect(rect,QPen(QColor("#607782")),color);card->setFlag(QGraphicsItem::ItemIsSelectable);card->setFlag(QGraphicsItem::ItemClipsChildrenToShape);card->setData(0,id);card->setZValue(1);card->setToolTip(n.title+"\n"+n.detail+"\n"+n.file);state->items[id]=card;
      auto label=new QGraphicsTextItem(card);label->setPos(rect.topLeft()+QPointF(10,5));label->setTextWidth(263);label->setDefaultTextColor(QColor("#f1eee5"));QFont f=font();f.setPointSize(10);label->setFont(f);auto lines=n.detail.split('\n');QString summary=lines.value(0);if(summary.size()>60)summary=summary.left(59)+"…";QString title=n.title;if(title.size()>70)title=title.left(69)+"…";label->setPlainText(kindName(n.kind)+"\n"+title+"\n"+summary);label->setAcceptedMouseButtons(Qt::NoButton);}}
    int edgeCount=0;for(const auto &e:state->model.edges){if(!visible.contains(e.from)||!visible.contains(e.to))continue;++edgeCount;auto a=state->items[e.from]->rect(),b=state->items[e.to]->rect();bool right=b.center().x()>a.center().x();QPointF start(right?a.right():a.left(),a.center().y()),end(right?b.left():b.right(),b.center().y());if(e.from==e.to){start=QPointF(a.right(),a.top()+30);end=QPointF(a.right(),a.bottom()-25);}const double bend=(right?1:-1)*qMax(45.0,qAbs(end.x()-start.x())/2);QPainterPath path(start);path.cubicTo(start+QPointF(bend,0),end-QPointF(bend,0),end);auto line=scene->addPath(path,QPen(e.conditional?QColor("#ae99c3"):QColor("#8fbea6"),1.6,e.conditional?Qt::DashLine:Qt::SolidLine));line->setToolTip(e.reason);line->setData(0,e.to);auto arrow=scene->addPolygon(QPolygonF{end,end+QPointF(right?-9:9,-5),end+QPointF(right?-9:9,5)},Qt::NoPen,QBrush("#c1cbd0"));arrow->setZValue(.5);}
    blocker.unblock();scene->setSceneRect(scene->itemsBoundingRect().adjusted(-40,-40,40,40));graph->fitInView(scene->sceneRect(),Qt::KeepAspectRatio);overview->viewport()->update();selection(visible.contains(state->selected)?state->selected:root);info->setText(tr("%1개 항목 · %2개 연결\n카드를 선택하면 연결 근거를 확인합니다. Ctrl+휠로 확대/축소 · 연결 목록을 두 번 누르면 이동합니다.\n수치·확장 조건과 실행 중 덮어쓰기는 조건 편집기 및 플레이 검증에서 확인하세요.").arg(visible.size()).arg(edgeCount));
  };
  auto populate=[=](QString wanted){QSignalBlocker blocker(focus);focus->clear();for(auto it=state->model.nodes.begin();it!=state->model.nodes.end();++it){const auto n=it.value();if(!(n.title+" "+n.detail).contains(search->text(),Qt::CaseInsensitive))continue;focus->addItem(kindName(n.kind)+" · "+n.title+" · "+n.detail.section('\n',0,0),it.key());}focus->setCurrentIndex(qMax(0,focus->findData(wanted)));};
  QString initial;for(const auto &n:state->model.nodes)if(n.file==file&&(n.kind=="event"||n.kind=="ending")){initial=n.id;break;}populate(initial);state->selected=initial;
  connect(scene,&QGraphicsScene::selectionChanged,this,[=]{auto items=scene->selectedItems();if(!items.isEmpty())selection(items.first()->data(0).toString());});
  connect(focus,qOverload<int>(&QComboBox::currentIndexChanged),this,[=]{state->selected=focus->currentData().toString();render();});connect(depth,qOverload<int>(&QComboBox::currentIndexChanged),this,[=]{render();});connect(all,&QCheckBox::toggled,this,[=]{render();});
  connect(search,&QLineEdit::textChanged,this,[=]{populate(state->selected);render();});connect(fit,&QPushButton::clicked,this,[=]{graph->fitInView(scene->sceneRect(),Qt::KeepAspectRatio);});
  connect(center,&QPushButton::clicked,this,[=]{const auto id=state->selected;search->clear();focus->setCurrentIndex(focus->findData(id));});
  connect(reasons,&QTreeWidget::itemDoubleClicked,this,[=](QTreeWidgetItem *item,int){auto id=item->data(0,Qt::UserRole).toString();selection(id);if(state->items.contains(id)){graph->resetTransform();graph->centerOn(state->items[id]);}else {search->clear();focus->setCurrentIndex(focus->findData(id));}});
  connect(condition,&QPushButton::clicked,this,[=]{const auto n=state->model.nodes.value(state->selected);state->context=editConditions(n.file,n.scriptNode,n.choice,this);state->model=buildStoryConnections(state->context.value("documents").toArray());auto root=focus->currentData().toString();populate(root);render();});
  connect(script,&QPushButton::clicked,this,[=]{const auto n=state->model.nodes.value(state->selected);selectedFile=n.file;selectedNode=n.scriptNode;accept();});connect(close,&QPushButton::clicked,this,&QDialog::reject);QTimer::singleShot(0,this,render);
}
