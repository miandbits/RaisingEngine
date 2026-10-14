#include "studio.hpp"
#include "logic-editor.hpp"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QQueue>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>
#include <functional>

namespace {
QString nextId(const QJsonArray &rows,const QString &prefix) {
  QSet<QString> ids;for(auto row:rows)ids.insert(row.toObject().value("id").toString());
  int n=1;while(ids.contains(prefix+QString::number(n)))++n;return prefix+QString::number(n);
}
void selectReference(QComboBox *combo,const QString &id) {
  if(combo->findData(id)<0)combo->addItem(QObject::tr("누락된 참조: ")+id,id);
  combo->setCurrentIndex(combo->findData(id));
}
void optionalText(QJsonObject &object,const QString &key,const QString &value) {
  if(value.isEmpty())object.remove(key);else object[key]=value;
}
class ExpeditionGraph final:public QWidget {
public:
  QJsonObject expedition;int selected=-1;
  std::function<void(int)> choose;
  explicit ExpeditionGraph(QWidget *parent=nullptr):QWidget(parent){setObjectName("expeditionGraph");setMinimumSize(360,260);}
protected:
  QVector<QRectF> boxes;
  void paintEvent(QPaintEvent*)override {
    QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);painter.fillRect(rect(),QColor("#172a30"));
    const auto nodes=expedition.value("nodes").toArray();boxes.clear();if(nodes.empty())return;
    QMap<QString,int> indexes;for(int i=0;i<nodes.size();++i)indexes[nodes[i].toObject().value("id").toString()]=i;
    QVector<int> depth(nodes.size(),-1);QQueue<int> queue;
    const auto entry=expedition.value("entryNodeId").toString();if(indexes.contains(entry)){depth[indexes[entry]]=0;queue.enqueue(indexes[entry]);}
    int maxDepth=0;while(!queue.empty()){const int i=queue.dequeue();for(auto ref:nodes[i].toObject().value("connections").toArray()){
      auto id=ref.toString();if(indexes.contains(id)&&depth[indexes[id]]<0){depth[indexes[id]]=depth[i]+1;maxDepth=qMax(maxDepth,depth[indexes[id]]);queue.enqueue(indexes[id]);}}}
    QMap<int,int> counts,rowSlots;for(int &d:depth){if(d<0)d=maxDepth+1;counts[d]++;}
    const int columns=counts.lastKey()+1;const qreal column=qMax(110.0,(width()-20.0)/columns);
    setMinimumWidth(qMax(360,columns*135));
    for(int i=0;i<nodes.size();++i){int d=depth[i],slot=rowSlots[d]++;qreal y=30+(height()-60.0)*(slot+.5)/counts[d]-24;boxes.append(QRectF(10+d*column,y,qMin(125.0,column-15),48));}
    painter.setPen(QPen(QColor("#78989c"),1.5));
    for(int i=0;i<nodes.size();++i)for(auto ref:nodes[i].toObject().value("connections").toArray())if(indexes.contains(ref.toString())){
      const int j=indexes[ref.toString()];if(i==j)continue;auto a=boxes[i].center(),b=boxes[j].center();QLineF line(a,b);painter.drawLine(line);
      const auto angle=std::atan2(b.y()-a.y(),b.x()-a.x());auto tip=a+(b-a)*.65;
      painter.drawLine(tip,tip-QPointF(std::cos(angle-.5)*9,std::sin(angle-.5)*9));painter.drawLine(tip,tip-QPointF(std::cos(angle+.5)*9,std::sin(angle+.5)*9));}
    for(int i=0;i<nodes.size();++i){auto n=nodes[i].toObject();painter.setBrush(i==selected?QColor("#527e6b"):QColor("#263d45"));painter.setPen(QPen(n.value("exit").toBool()?QColor("#b7dd82"):QColor("#84a8b3"),i==selected?2:1));painter.drawRoundedRect(boxes[i],6,6);painter.setPen(Qt::white);
      auto text=n.value("name").toString();if(n.value("id")==entry)text=QStringLiteral("▶ ")+text;if(n.value("exit").toBool())text+=QObject::tr(" · 출구");if(!n.value("enemyId").toString().isEmpty())text+=QObject::tr("\n전투");painter.drawText(boxes[i].adjusted(3,2,-3,-2),Qt::AlignCenter|Qt::TextWordWrap,text);}
  }
  void mousePressEvent(QMouseEvent *event)override {if(event->button()!=Qt::LeftButton)return;for(int i=0;i<boxes.size();++i)if(boxes[i].contains(event->position())){if(choose)choose(i);break;}}
};
}

void StudioWindow::editExpeditions() {
  if(bridge.busy()||projectData().isEmpty())return;
  const auto originalRevision=view.value("revision"),originalRoot=view.value("root");
  auto project=projectData(),systems=project.value("systems").toObject();const auto gameplay=project.value("gameplay").toObject();
  auto expeditions=systems.value("expeditions").toArray(),enemies=systems.value("enemies").toArray();
  const auto locations=gameplay.value("locations").toArray(),items=gameplay.value("items").toArray();
  if(locations.empty()){errorLabel->setText(tr("탐험을 작성하려면 프로젝트에 장소가 필요합니다."));return;}
  QJsonArray attributes,activities;for(auto d:view.value("documents").toArray()){
    auto doc=d.toObject();if(doc.value("kind")=="attributes")attributes=doc.value("data").toObject().value("attributes").toArray();
    if(doc.value("kind")=="activity")activities.append(doc);
  }
  QDialog dialog(this);dialog.setObjectName("expeditionAuthoringDialog");dialog.setWindowTitle(tr("탐험 · 조우 작성실"));dialog.resize(1280,900);
  auto outer=new QVBoxLayout(&dialog);auto help=new QLabel(tr("지점과 이동 연결을 만든 뒤 활동에 연결하세요. 출구 밖에서도 철수할 수 있습니다. 변경은 저장할 때 한 번에 적용됩니다."));help->setWordWrap(true);outer->addWidget(help);
  auto tabs=new QTabWidget;tabs->setObjectName("expeditionTabs");outer->addWidget(tabs,1);
  auto mapPage=new QWidget;auto mapBox=new QHBoxLayout(mapPage);tabs->addTab(mapPage,tr("탐험 지도"));
  auto expeditionColumn=new QVBoxLayout;mapBox->addLayout(expeditionColumn);auto expeditionList=new QListWidget;expeditionList->setObjectName("expeditionList");expeditionList->setMaximumWidth(190);expeditionColumn->addWidget(expeditionList,1);
  auto button=[&](QBoxLayout *box,QString id,QString label){auto b=new QPushButton(label);b->setObjectName(id);box->addWidget(b);return b;};
  auto addExpedition=button(expeditionColumn,"expeditionAdd",tr("+ 탐험"));auto duplicateExpedition=button(expeditionColumn,"expeditionDuplicate",tr("복제"));auto removeExpedition=button(expeditionColumn,"expeditionRemove",tr("삭제"));
  auto mapEditor=new QVBoxLayout;mapBox->addLayout(mapEditor,1);auto expeditionForm=new QFormLayout;mapEditor->addLayout(expeditionForm);
  auto expeditionName=new QLineEdit;expeditionName->setObjectName("expeditionName");expeditionName->setMaxLength(2000);auto expeditionDescription=new QLineEdit;expeditionDescription->setObjectName("expeditionDescription");expeditionDescription->setMaxLength(4000);
  auto location=new QComboBox;location->setObjectName("expeditionLocation");for(auto v:locations){auto l=v.toObject();location->addItem(l.value("name").toString(),l.value("id").toString());}
  auto entry=new QComboBox;entry->setObjectName("expeditionEntry");expeditionForm->addRow(tr("탐험 이름"),expeditionName);expeditionForm->addRow(tr("설명"),expeditionDescription);expeditionForm->addRow(tr("시작 장소"),location);expeditionForm->addRow(tr("시작 지점"),entry);
  auto mapBody=new QHBoxLayout;mapEditor->addLayout(mapBody,1);auto nodeColumn=new QVBoxLayout;mapBody->addLayout(nodeColumn);auto nodeList=new QListWidget;nodeList->setObjectName("expeditionNodes");nodeList->setMaximumWidth(190);nodeColumn->addWidget(nodeList,1);
  auto addNode=button(nodeColumn,"expeditionNodeAdd",tr("+ 지점")),duplicateNode=button(nodeColumn,"expeditionNodeDuplicate",tr("지점 복제")),removeNode=button(nodeColumn,"expeditionNodeRemove",tr("지점 삭제"));
  auto graph=new ExpeditionGraph;auto graphScroll=new QScrollArea;graphScroll->setWidgetResizable(true);graphScroll->setWidget(graph);mapBody->addWidget(graphScroll,1);
  auto nodeProperties=new QFormLayout;mapEditor->addLayout(nodeProperties);auto nodeName=new QLineEdit;nodeName->setObjectName("expeditionNodeName");nodeName->setMaxLength(2000);auto nodeDescription=new QLineEdit;nodeDescription->setObjectName("expeditionNodeDescription");nodeDescription->setMaxLength(4000);
  auto exitNode=new QCheckBox(tr("이 지점은 완료 출구"));exitNode->setObjectName("expeditionNodeExit");
  auto enemy=new QComboBox;enemy->setObjectName("expeditionNodeEnemy");auto background=new QComboBox;background->setObjectName("expeditionNodeBackground");background->addItem(tr("배경 유지"),QString());background->addItem(tr("배경 지우기"),QStringLiteral("@clear"));for(auto a:assets){auto v=a.toObject();if(v.value("kind")=="image")background->addItem(v.value("id").toString(),v.value("id").toString());}
  nodeProperties->addRow(tr("지점 이름"),nodeName);nodeProperties->addRow(tr("지점 설명"),nodeDescription);nodeProperties->addRow(exitNode);nodeProperties->addRow(tr("조우할 적"),enemy);nodeProperties->addRow(tr("배경"),background);
  auto connections=new QListWidget;connections->setObjectName("expeditionConnections");connections->setMaximumHeight(130);nodeProperties->addRow(tr("이동 가능한 지점\n(방향 연결)"),connections);

  auto enemyPage=new QWidget;auto enemyBox=new QHBoxLayout(enemyPage);tabs->addTab(enemyPage,tr("적 · 전투 · 보상"));auto enemyColumn=new QVBoxLayout;enemyBox->addLayout(enemyColumn);auto enemyList=new QListWidget;enemyList->setObjectName("expeditionEnemies");enemyList->setMaximumWidth(190);enemyColumn->addWidget(enemyList,1);
  auto addEnemy=button(enemyColumn,"expeditionEnemyAdd",tr("+ 적")),duplicateEnemy=button(enemyColumn,"expeditionEnemyDuplicate",tr("복제")),removeEnemy=button(enemyColumn,"expeditionEnemyRemove",tr("삭제"));auto enemyEditor=new QVBoxLayout;enemyBox->addLayout(enemyEditor,1);
  auto heroForm=new QFormLayout;enemyEditor->addLayout(heroForm);QMap<QString,QComboBox*> heroFields;auto hero=systems.value("hero").toObject();
  for(auto p:QList<QPair<QString,QString>>{{"maxHpAttributeId",tr("주인공 최대 HP")},{"attackAttributeId",tr("주인공 공격력")},{"defenseAttributeId",tr("주인공 방어력")}}){auto combo=new QComboBox;combo->setObjectName("expeditionHero-"+p.first);combo->addItem(tr("능력 선택"),QString());for(auto a:attributes){auto v=a.toObject();combo->addItem(v.value("label").toString(),v.value("id").toString());}selectReference(combo,hero.value(p.first).toString());heroFields[p.first]=combo;heroForm->addRow(p.second,combo);}
  auto enemyForm=new QFormLayout;enemyEditor->addLayout(enemyForm);auto enemyName=new QLineEdit;enemyName->setObjectName("expeditionEnemyName");enemyName->setMaxLength(2000);auto enemyDescription=new QLineEdit;enemyDescription->setObjectName("expeditionEnemyDescription");enemyDescription->setMaxLength(4000);enemyForm->addRow(tr("적 이름"),enemyName);enemyForm->addRow(tr("설명"),enemyDescription);
  QMap<QString,QSpinBox*> enemyStats;for(auto p:QList<QPair<QString,QString>>{{"maxHp",tr("최대 HP")},{"attack",tr("공격력")},{"defense",tr("방어력")}}){auto field=new QSpinBox;field->setObjectName("expeditionEnemy-"+p.first);field->setRange(p.first=="maxHp"?1:0,1000000);enemyStats[p.first]=field;enemyForm->addRow(p.second,field);}
  auto rewards=new QTableWidget(0,2);rewards->setObjectName("expeditionRewards");rewards->setHorizontalHeaderLabels({tr("보상 물품"),tr("수량")});rewards->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);enemyEditor->addWidget(rewards,1);auto rewardButtons=new QHBoxLayout;enemyEditor->addLayout(rewardButtons);auto addReward=button(rewardButtons,"expeditionRewardAdd",tr("+ 물품 보상")),removeReward=button(rewardButtons,"expeditionRewardRemove",tr("보상 삭제")),editEffects=button(rewardButtons,"expeditionRewardEffects",tr("능력 · 자원 보상 효과…"));

  auto mappingPage=new QWidget;auto mappingBox=new QVBoxLayout(mappingPage);tabs->addTab(mappingPage,tr("활동 연결"));auto mappingHelp=new QLabel(tr("탐험은 전투로 결과를 결정합니다. 성공 확률 판정이 있는 활동은 연결할 수 없습니다. 새 활동을 만들거나 해당 판정을 먼저 정리하세요."));mappingHelp->setWordWrap(true);mappingBox->addWidget(mappingHelp);auto mappings=new QTableWidget(0,2);mappings->setObjectName("expeditionActivityMappings");mappings->setHorizontalHeaderLabels({tr("활동"),tr("진입할 탐험")});mappings->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);mappingBox->addWidget(mappings);
  auto error=new QLabel;error->setObjectName("expeditionAuthoringError");error->setWordWrap(true);error->setStyleSheet("color:#f3a18a");outer->addWidget(error);auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons->setObjectName("expeditionAuthoringButtons");outer->addWidget(buttons);
  int selectedExpedition=-1,selectedNode=-1,selectedEnemy=-1;bool filling=false;
  QMap<QString,QString> activityLinks;for(auto a:activities){auto d=a.toObject();activityLinks[d.value("file").toString()]=d.value("data").toObject().value("expeditionId").toString();}
  auto activeExpedition=[&]{return selectedExpedition>=0&&selectedExpedition<expeditions.size()?expeditions[selectedExpedition].toObject():QJsonObject{};};
  auto refreshGraph=[&]{graph->expedition=activeExpedition();graph->selected=selectedNode;graph->update();};
  auto commitNode=[&]{if(filling||selectedNode<0||selectedExpedition<0)return;auto e=activeExpedition();auto nodes=e.value("nodes").toArray();if(selectedNode>=nodes.size())return;auto n=nodes[selectedNode].toObject();n["name"]=nodeName->text();optionalText(n,"description",nodeDescription->text());n["exit"]=exitNode->isChecked();optionalText(n,"enemyId",enemy->currentData().toString());auto presentation=n.value("presentation").toObject();if(background->currentData().toString()=="@clear")presentation["backgroundAssetId"]=QJsonValue::Null;else optionalText(presentation,"backgroundAssetId",background->currentData().toString());if(presentation.empty())n.remove("presentation");else n["presentation"]=presentation;QJsonArray refs;for(int r=0;r<connections->count();++r)if(connections->item(r)->checkState()==Qt::Checked)refs.append(connections->item(r)->data(Qt::UserRole).toString());n["connections"]=refs;nodes[selectedNode]=n;e["nodes"]=nodes;expeditions[selectedExpedition]=e;if(nodeList->item(selectedNode))nodeList->item(selectedNode)->setText(n.value("name").toString());refreshGraph();};
  auto commitExpedition=[&]{if(filling||selectedExpedition<0)return;commitNode();auto e=activeExpedition();e["name"]=expeditionName->text();optionalText(e,"description",expeditionDescription->text());e["locationId"]=location->currentData().toString();e["entryNodeId"]=entry->currentData().toString();expeditions[selectedExpedition]=e;if(expeditionList->item(selectedExpedition))expeditionList->item(selectedExpedition)->setText(e.value("name").toString());refreshGraph();};
  auto refillEnemyChoices=[&](QString id){QSignalBlocker block(enemy);enemy->clear();enemy->addItem(tr("전투 없음"),QString());for(auto v:enemies){auto e=v.toObject();enemy->addItem(e.value("name").toString(),e.value("id").toString());}selectReference(enemy,id);};
  auto loadNode=[&](int row){filling=true;selectedNode=row;auto nodes=activeExpedition().value("nodes").toArray();auto n=row>=0&&row<nodes.size()?nodes[row].toObject():QJsonObject{};nodeName->setText(n.value("name").toString());nodeDescription->setText(n.value("description").toString());exitNode->setChecked(n.value("exit").toBool());refillEnemyChoices(n.value("enemyId").toString());const auto bg=n.value("presentation").toObject().value("backgroundAssetId");selectReference(background,bg.isNull()?QStringLiteral("@clear"):bg.toString());connections->clear();QSet<QString> refs;for(auto r:n.value("connections").toArray())refs.insert(r.toString());for(auto v:nodes){auto other=v.toObject();auto item=new QListWidgetItem(other.value("name").toString(),connections);item->setData(Qt::UserRole,other.value("id"));item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(refs.remove(other.value("id").toString())?Qt::Checked:Qt::Unchecked);}for(auto id:refs){auto item=new QListWidgetItem(tr("누락: ")+id,connections);item->setData(Qt::UserRole,id);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(Qt::Checked);}for(auto widget:QList<QWidget*>{nodeName,nodeDescription,exitNode,enemy,background,connections,duplicateNode,removeNode})widget->setEnabled(row>=0);filling=false;refreshGraph();};
  auto refillNodes=[&](int row){filling=true;QSignalBlocker b1(nodeList),b2(entry);auto e=activeExpedition();nodeList->clear();entry->clear();for(auto v:e.value("nodes").toArray()){auto n=v.toObject();nodeList->addItem(n.value("name").toString());entry->addItem(n.value("name").toString(),n.value("id").toString());}if(!e.empty())selectReference(entry,e.value("entryNodeId").toString());nodeList->setCurrentRow(row);filling=false;loadNode(row);};
  auto loadExpedition=[&](int row){filling=true;selectedExpedition=row;selectedNode=-1;auto e=activeExpedition();expeditionName->setText(e.value("name").toString());expeditionDescription->setText(e.value("description").toString());selectReference(location,e.value("locationId").toString());for(auto widget:QList<QWidget*>{expeditionName,expeditionDescription,location,entry,addNode,duplicateExpedition,removeExpedition})widget->setEnabled(row>=0);filling=false;refillNodes(e.value("nodes").toArray().empty()?-1:0);};
  auto refillExpeditions=[&](int row){QSignalBlocker block(expeditionList);expeditionList->clear();for(auto v:expeditions)expeditionList->addItem(v.toObject().value("name").toString());expeditionList->setCurrentRow(row);loadExpedition(row);};
  auto fillMappings=[&]{QSignalBlocker block(mappings);mappings->setRowCount(0);for(auto a:activities){auto doc=a.toObject(),data=doc.value("data").toObject();const auto file=doc.value("file").toString();int row=mappings->rowCount();mappings->insertRow(row);auto label=new QTableWidgetItem(data.value("label").toString());label->setFlags(label->flags()&~Qt::ItemIsEditable);mappings->setItem(row,0,label);auto combo=new QComboBox;combo->addItem(tr("일반 활동"),QString());for(auto v:expeditions){auto e=v.toObject();combo->addItem(e.value("name").toString(),e.value("id").toString());}selectReference(combo,activityLinks[file]);if(data.contains("successCheck")){combo->setToolTip(tr("성공 확률 판정이 있는 활동은 탐험과 함께 사용할 수 없습니다."));}mappings->setCellWidget(row,1,combo);connect(combo,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&,combo,file]{activityLinks[file]=combo->currentData().toString();});}};
  auto collectRewards=[&]{QJsonArray values;for(int r=0;r<rewards->rowCount();++r){auto c=qobject_cast<QComboBox*>(rewards->cellWidget(r,0));auto value=c->property("original").toJsonObject();value["itemId"]=c->currentData().toString();value["quantity"]=qobject_cast<QSpinBox*>(rewards->cellWidget(r,1))->value();values.append(value);}return values;};
  auto commitEnemy=[&]{if(filling||selectedEnemy<0||selectedEnemy>=enemies.size())return;auto e=enemies[selectedEnemy].toObject();e["name"]=enemyName->text();optionalText(e,"description",enemyDescription->text());for(auto it=enemyStats.begin();it!=enemyStats.end();++it)e[it.key()]=it.value()->value();auto r=e.value("rewards").toObject();r["items"]=collectRewards();e["rewards"]=r;enemies[selectedEnemy]=e;if(enemyList->item(selectedEnemy))enemyList->item(selectedEnemy)->setText(e.value("name").toString());};
  auto appendReward=[&](QJsonObject value){int row=rewards->rowCount();rewards->insertRow(row);auto item=new QComboBox;item->setProperty("original",value);for(auto v:items){auto d=v.toObject();item->addItem(d.value("name").toString(),d.value("id").toString());}selectReference(item,value.value("itemId").toString(item->currentData().toString()));auto count=new QSpinBox;count->setRange(1,1000000);count->setValue(value.value("quantity").toInt(1));rewards->setCellWidget(row,0,item);rewards->setCellWidget(row,1,count);connect(item,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{commitEnemy();});connect(count,qOverload<int>(&QSpinBox::valueChanged),&dialog,[&]{commitEnemy();});};
  auto loadEnemy=[&](int row){filling=true;selectedEnemy=row;auto e=row>=0&&row<enemies.size()?enemies[row].toObject():QJsonObject{};enemyName->setText(e.value("name").toString());enemyDescription->setText(e.value("description").toString());for(auto it=enemyStats.begin();it!=enemyStats.end();++it)it.value()->setValue(e.value(it.key()).toInt());rewards->setRowCount(0);for(auto v:e.value("rewards").toObject().value("items").toArray())appendReward(v.toObject());for(auto widget:QList<QWidget*>{enemyName,enemyDescription,rewards,addReward,removeReward,editEffects,duplicateEnemy,removeEnemy})widget->setEnabled(row>=0);for(auto spin:enemyStats)spin->setEnabled(row>=0);filling=false;};
  auto refillEnemies=[&](int row){QSignalBlocker block(enemyList);enemyList->clear();for(auto e:enemies)enemyList->addItem(e.toObject().value("name").toString());enemyList->setCurrentRow(row);loadEnemy(row);};
  connect(expeditionList,&QListWidget::currentRowChanged,&dialog,[&](int row){commitExpedition();loadExpedition(row);});connect(nodeList,&QListWidget::currentRowChanged,&dialog,[&](int row){commitNode();loadNode(row);});graph->choose=[&](int row){nodeList->setCurrentRow(row);};
  for(auto text:{expeditionName,expeditionDescription})connect(text,&QLineEdit::textEdited,&dialog,[&]{commitExpedition();});for(auto combo:{location,entry})connect(combo,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{commitExpedition();});
  for(auto text:{nodeName,nodeDescription})connect(text,&QLineEdit::textEdited,&dialog,[&]{commitNode();});for(auto combo:{enemy,background})connect(combo,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{commitNode();});connect(exitNode,&QCheckBox::toggled,&dialog,[&]{commitNode();});connect(connections,&QListWidget::itemChanged,&dialog,[&]{commitNode();});
  connect(enemyList,&QListWidget::currentRowChanged,&dialog,[&](int row){commitEnemy();loadEnemy(row);});for(auto text:{enemyName,enemyDescription})connect(text,&QLineEdit::textEdited,&dialog,[&]{commitEnemy();});for(auto spin:enemyStats)connect(spin,qOverload<int>(&QSpinBox::valueChanged),&dialog,[&]{commitEnemy();});
  connect(tabs,&QTabWidget::currentChanged,&dialog,[&]{commitExpedition();commitEnemy();refillEnemyChoices(enemy->currentData().toString());fillMappings();});
  connect(addExpedition,&QPushButton::clicked,&dialog,[&]{commitExpedition();if(expeditions.size()>=1000){error->setText(tr("탐험은 최대 1000개입니다."));return;}expeditions.append(QJsonObject{{"id",nextId(expeditions,"expedition-")},{"name",tr("새 탐험")},{"locationId",locations.first().toObject().value("id")},{"entryNodeId","node-1"},{"nodes",QJsonArray{QJsonObject{{"id","node-1"},{"name",tr("입구")},{"exit",true},{"connections",QJsonArray{}}}}}});refillExpeditions(expeditions.size()-1);fillMappings();});
  connect(duplicateExpedition,&QPushButton::clicked,&dialog,[&]{if(selectedExpedition<0||expeditions.size()>=1000)return;commitExpedition();auto e=activeExpedition();e["id"]=nextId(expeditions,"expedition-");e["name"]=e.value("name").toString()+tr(" 복사");expeditions.append(e);refillExpeditions(expeditions.size()-1);fillMappings();});
  connect(removeExpedition,&QPushButton::clicked,&dialog,[&]{if(selectedExpedition<0)return;const auto id=activeExpedition().value("id").toString();if(activityLinks.values().contains(id)){error->setText(tr("활동 연결 탭에서 이 탐험 연결을 먼저 해제하세요."));return;}expeditions.removeAt(selectedExpedition);refillExpeditions(qMin(selectedExpedition,int(expeditions.size())-1));fillMappings();});
  auto newNode=[&](bool copy){if(selectedExpedition<0)return;commitExpedition();auto e=activeExpedition();auto nodes=e.value("nodes").toArray();if(nodes.size()>=1000)return;auto n=copy&&selectedNode>=0?nodes[selectedNode].toObject():QJsonObject{{"name",tr("새 지점")},{"connections",QJsonArray{}}};n["id"]=nextId(nodes,"node-");if(copy)n["name"]=n.value("name").toString()+tr(" 복사");nodes.append(n);e["nodes"]=nodes;expeditions[selectedExpedition]=e;refillNodes(nodes.size()-1);};connect(addNode,&QPushButton::clicked,&dialog,[&]{newNode(false);});connect(duplicateNode,&QPushButton::clicked,&dialog,[&]{newNode(true);});
  connect(removeNode,&QPushButton::clicked,&dialog,[&]{if(selectedNode<0)return;commitExpedition();auto e=activeExpedition();auto nodes=e.value("nodes").toArray();auto id=nodes[selectedNode].toObject().value("id").toString();if(e.value("entryNodeId")==id){error->setText(tr("시작 지점은 삭제할 수 없습니다. 다른 시작 지점을 선택하세요."));return;}for(auto n:nodes)for(auto ref:n.toObject().value("connections").toArray())if(ref==id){error->setText(tr("이 지점을 향하는 이동 연결을 먼저 해제하세요."));return;}nodes.removeAt(selectedNode);e["nodes"]=nodes;expeditions[selectedExpedition]=e;refillNodes(qMin(selectedNode,int(nodes.size())-1));});
  connect(addEnemy,&QPushButton::clicked,&dialog,[&]{commitEnemy();if(enemies.size()>=1000)return;enemies.append(QJsonObject{{"id",nextId(enemies,"enemy-")},{"name",tr("새 적")},{"maxHp",30},{"attack",5},{"defense",0},{"rewards",QJsonObject{{"items",QJsonArray{}},{"effects",QJsonArray{}}}}});refillEnemies(enemies.size()-1);refillEnemyChoices(enemy->currentData().toString());});
  connect(duplicateEnemy,&QPushButton::clicked,&dialog,[&]{if(selectedEnemy<0||enemies.size()>=1000)return;commitEnemy();auto e=enemies[selectedEnemy].toObject();e["id"]=nextId(enemies,"enemy-");e["name"]=e.value("name").toString()+tr(" 복사");enemies.append(e);refillEnemies(enemies.size()-1);refillEnemyChoices(enemy->currentData().toString());});
  connect(removeEnemy,&QPushButton::clicked,&dialog,[&]{if(selectedEnemy<0)return;commitExpedition();const auto id=enemies[selectedEnemy].toObject().value("id");for(auto e:expeditions)for(auto n:e.toObject().value("nodes").toArray())if(n.toObject().value("enemyId")==id){error->setText(tr("탐험 지점에서 이 적 참조를 먼저 해제하세요."));return;}for(auto q:systems.value("quests").toArray())if(q.toObject().value("enemyId")==id){error->setText(tr("의뢰에서 이 적을 참조하고 있어 삭제할 수 없습니다."));return;}enemies.removeAt(selectedEnemy);refillEnemies(qMin(selectedEnemy,int(enemies.size())-1));refillEnemyChoices(enemy->currentData().toString());});
  connect(addReward,&QPushButton::clicked,&dialog,[&]{if(selectedEnemy<0||items.empty())return;if(rewards->rowCount()>=1000)return;appendReward({});commitEnemy();});connect(removeReward,&QPushButton::clicked,&dialog,[&]{if(rewards->currentRow()<0)return;rewards->removeRow(rewards->currentRow());commitEnemy();});
  connect(editEffects,&QPushButton::clicked,&dialog,[&]{if(selectedEnemy<0)return;commitEnemy();auto e=enemies[selectedEnemy].toObject(),r=e.value("rewards").toObject();LogicEditor editor(r.value("effects"),"effects",view,&dialog);if(editor.exec()!=QDialog::Accepted)return;r["effects"]=editor.value();e["rewards"]=r;enemies[selectedEnemy]=e;});
  QJsonArray edits;
  connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
    if(view.value("revision")!=originalRevision||view.value("root")!=originalRoot){error->setText(tr("프로젝트가 변경되었습니다. 창을 닫고 다시 여세요."));return;}
    commitExpedition();commitEnemy();QSet<QString> expeditionIds,enemyIds,locationIds,assetIds,itemIds;for(auto v:locations)locationIds.insert(v.toObject().value("id").toString());for(auto v:items)itemIds.insert(v.toObject().value("id").toString());for(auto v:assets)if(v.toObject().value("kind")=="image")assetIds.insert(v.toObject().value("id").toString());
    for(auto it=heroFields.begin();it!=heroFields.end();++it){auto id=it.value()->currentData().toString();bool found=false;for(auto a:attributes)if(a.toObject().value("id")==id)found=true;if(!found){error->setText(tr("주인공 HP·공격·방어 속성을 선택하세요."));tabs->setCurrentIndex(1);return;}hero[it.key()]=id;}
    for(auto v:enemies){auto e=v.toObject();if(e.value("name").toString().trimmed().isEmpty()){error->setText(tr("적 이름을 입력하세요."));return;}enemyIds.insert(e.value("id").toString());for(auto r:e.value("rewards").toObject().value("items").toArray())if(!itemIds.contains(r.toObject().value("itemId").toString())){error->setText(tr("보상 물품 참조를 수정하세요."));return;}}
    for(auto v:expeditions){auto e=v.toObject();const auto id=e.value("id").toString();expeditionIds.insert(id);if(e.value("name").toString().trimmed().isEmpty()||!locationIds.contains(e.value("locationId").toString())){error->setText(tr("탐험 이름과 시작 장소를 확인하세요: ")+id);return;}auto nodes=e.value("nodes").toArray();QMap<QString,QJsonObject> byId;for(auto n:nodes)byId[n.toObject().value("id").toString()]=n.toObject();if(!byId.contains(e.value("entryNodeId").toString())){error->setText(tr("시작 지점을 선택하세요: ")+id);return;}for(auto n:nodes){auto node=n.toObject();if(node.value("name").toString().trimmed().isEmpty()){error->setText(tr("지점 이름을 입력하세요."));return;}if(node.contains("enemyId")&&!enemyIds.contains(node.value("enemyId").toString())){error->setText(tr("누락된 적 참조를 수정하세요."));return;}auto bg=node.value("presentation").toObject().value("backgroundAssetId").toString();if(!bg.isEmpty()&&!assetIds.contains(bg)){error->setText(tr("지점 배경 이미지를 다시 선택하세요."));return;}for(auto r:node.value("connections").toArray())if(!byId.contains(r.toString())){error->setText(tr("누락된 지점 연결을 수정하세요."));return;}}
      QSet<QString> seen;QStringList pending{e.value("entryNodeId").toString()};bool hasExit=false;while(!pending.empty()){auto n=pending.takeLast();if(seen.contains(n))continue;seen.insert(n);auto node=byId.value(n);hasExit=hasExit||node.value("exit").toBool();for(auto r:node.value("connections").toArray())pending.append(r.toString());}if(!hasExit){error->setText(tr("시작 지점에서 도달할 수 있는 출구가 필요합니다: ")+id);return;}
    }
    edits={};for(auto v:activities){auto doc=v.toObject(),data=doc.value("data").toObject();auto ref=activityLinks.value(doc.value("file").toString());if(!ref.isEmpty()&&!expeditionIds.contains(ref)){error->setText(tr("활동의 탐험 참조가 없습니다: ")+data.value("id").toString());return;}if(!ref.isEmpty()&&data.contains("successCheck")){error->setText(tr("탐험 활동은 별도 성공 판정을 함께 사용할 수 없습니다."));return;}if(ref!=data.value("expeditionId").toString()){optionalText(data,"expeditionId",ref);edits.append(QJsonObject{{"type","replace"},{"file",doc.value("file")},{"data",data}});}}
    if(edits.size()>99){error->setText(tr("한 번에 연결을 변경할 수 있는 활동은 99개입니다."));return;}
    systems["hero"]=hero;systems["expeditions"]=expeditions;systems["enemies"]=enemies;for(auto key:{"equipment","consumables","quests","relationshipStages"})if(!systems.contains(key))systems[key]=QJsonArray{};project["systems"]=systems;
    edits.prepend(QJsonObject{{"type","replace"},{"file",projectFile},{"data",project}});dialog.accept();
  });
  refillExpeditions(expeditions.empty()?-1:0);refillEnemies(enemies.empty()?-1:0);fillMappings();
  if(dialog.exec()!=QDialog::Accepted)return;
  request("edit",{{"edit",QJsonObject{{"type","batch"},{"edits",edits}}}});
}
