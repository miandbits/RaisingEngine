#include "studio.hpp"
#include "battle-hud.hpp"
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
QString freshHudId(const QJsonArray &rows,const QString &prefix){QSet<QString> ids;for(auto v:rows)ids.insert(v.toObject().value("id").toString());if(!ids.contains(prefix))return prefix;int n=2;while(ids.contains(prefix+QString::number(n)))++n;return prefix+QString::number(n);}
QJsonObject hudPart(QString id,QString kind,int x,int y,int w,int h,QString text={}){
  QJsonObject e{{"id",id},{"kind",kind},{"x",x},{"y",y},{"width",w},{"height",h},{"anchorX","stretch"},{"color","#eff6f5"},{"fontSize",20}};
  if(!text.isEmpty())e["text"]=text;
  if(kind=="panel")e["fill"]="#20323fee";
  if(kind=="button"){e["fill"]="#365e70";e["action"]="sound";}
  return e;
}
QJsonObject hudBinding(QString id,QString prop,QString field){return {{"elementId",id},{"property",prop},{"field",field}};}
QJsonObject hudComponent(QString id,QString source,double scale=1){
  const bool actors=source=="battleActors",select=source=="battleActions"||source=="battleTargets";
  const int height=actors?124:select?100:64;
  QJsonArray children{hudPart("background","panel",0,0,360,height)};
  auto title=hudPart(select?"button":"title",select?"button":"text",12,8,336,select?48:36,select?QString::fromUtf8("선택"):QString::fromUtf8("{name}"));children.append(title);
  if(actors){auto meter=hudPart("hp","meter",12,52,336,28);meter["value"]=75;meter["maxValue"]=100;meter["fill"]="#16242e";meter["color"]="#68c6a0";children.append(meter);children.append(hudPart("value","text",12,86,336,30,"{hpText}"));}
  if(select){auto reason=hudPart("reason","text",12,58,336,34,"{reason}");reason["fontSize"]=16;children.append(reason);}
  if(!actors&&!select){auto t=children[1].toObject();t["height"]=48;t["fontSize"]=18;t["align"]="left";children[1]=t;}
  for(int i=0;i<children.size();i++){auto e=children[i].toObject();for(auto key:{"x","y","width","height"})e[key]=qMax(QString(key)=="width"||QString(key)=="height"?16:0,qRound(e.value(key).toDouble()*scale));e["fontSize"]=qMax(10,qRound(e.value("fontSize").toInt(20)*scale));children[i]=e;}
  return {{"id",id},{"label",QString::fromUtf8("전투 카드 · ")+source},{"width",qMax(16,qRound(360*scale))},{"height",qMax(16,qRound(height*scale))},{"elements",children}};
}
QJsonArray hudBindings(QString source){
  QJsonArray bindings;
  if(source=="battleActors"){bindings.append(hudBinding("title","text","name"));bindings.append(hudBinding("hp","value","hp"));bindings.append(hudBinding("hp","maxValue","maxHp"));bindings.append(hudBinding("value","text","hpText"));}
  else if(source=="battleActions"||source=="battleTargets"){
    for(auto pair:QList<QPair<QString,QString>>{{"text","displayName"},{"action","action"},{"enabled","enabled"}})bindings.append(hudBinding("button",pair.first,pair.second));
    if(source=="battleActions"){bindings.append(hudBinding("button","battleAction","battleAction"));bindings.append(hudBinding("button","itemId","itemId"));}
    else bindings.append(hudBinding("button","battleTarget","battleTarget"));
    bindings.append(hudBinding("reason","text","reason"));
  }else bindings.append(hudBinding("title","text","text"));
  return bindings;
}
}

void StudioWindow::editBattleHud(){
  if(bridge.busy()||currentScreen().empty())return;
  auto project=projectData(),visual=project.value("visual").toObject();auto components=visual.value("components").toArray();auto elements=currentScreen().value("elements").toArray();
  QDialog dialog(this);dialog.setObjectName("battleHudDialog");dialog.setWindowTitle(tr("전투 HUD 제작 · 데이터와 화면 연결"));dialog.resize(1150,760);auto outer=new QVBoxLayout(&dialog);
  auto help=new QLabel(tr("현재 화면에 전투 데이터를 표시하는 재사용 부품을 배치합니다. 기본 배치 후 캔버스에서 이동·크기·앵커를 수정하고, ‘데이터 · 반복 UI 연결’에서 필터와 각 속성을 바꿀 수 있습니다. 실제 전투 단계에는 ID가 battle인 화면이 사용됩니다. 현재 전투 규칙은 아군 1명 / 적 1명입니다."));help->setWordWrap(true);outer->addWidget(help);
  auto bar=new QHBoxLayout;outer->addLayout(bar);auto preset=new QPushButton(tr("기본 전투 HUD 배치"));preset->setObjectName("battleHudPreset");bar->addWidget(preset);auto source=new QComboBox;source->setObjectName("battleHudSource");
  for(auto p:QList<QPair<QString,QString>>{{"아군·적 상태","battleActors"},{"행동 선택","battleActions"},{"대상 선택","battleTargets"},{"전투 로그","battleLog"},{"실제 지급 보상","battleRewards"}})source->addItem(p.first,p.second);bar->addWidget(source);
  auto add=new QPushButton(tr("목록 추가"));add->setObjectName("battleHudAdd");bar->addWidget(add);auto remove=new QPushButton(tr("선택 목록 삭제"));remove->setObjectName("battleHudRemove");bar->addWidget(remove);
  auto table=new QTableWidget(0,8);table->setObjectName("battleHudLayout");table->setHorizontalHeaderLabels({tr("ID"),tr("이름"),tr("연결된 데이터"),"X","Y",tr("너비"),tr("높이"),tr("행 높이")});table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);table->setSelectionBehavior(QAbstractItemView::SelectRows);outer->addWidget(table,1);
  auto detail=new QLabel;detail->setObjectName("battleHudDetail");detail->setWordWrap(true);outer->addWidget(detail);
  auto logLimit=new QSpinBox;logLimit->setObjectName("battleHudHistoryLimit");logLimit->setRange(1,256);logLimit->setValue(project.value("systems").toObject().value("battleHistoryLimit").toInt(64));auto opts=new QFormLayout;opts->addRow(tr("저장할 최대 전투 로그 수"),logLimit);outer->addLayout(opts);
  auto error=new QLabel;error->setObjectName("battleHudError");error->setWordWrap(true);error->setStyleSheet("color:#ee8e77");outer->addWidget(error);auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);outer->addWidget(buttons);
  auto commit=[&]{for(int r=0;r<table->rowCount();r++){int index=table->item(r,0)->data(Qt::UserRole).toInt();auto e=elements[index].toObject();e["label"]=table->item(r,1)->text();const QStringList keys{"x","y","width","height","rowHeight"};for(int c=3;c<8;c++)e[keys[c-3]]=qobject_cast<QSpinBox*>(table->cellWidget(r,c))->value();elements[index]=e;}};
  auto rowHeight=[&](const QJsonObject&e){if(e.contains("rowHeight"))return e.value("rowHeight").toInt();for(auto v:components)if(v.toObject().value("id")==e.value("componentId"))return v.toObject().value("height").toInt(180);return 180;};
  auto refill=[&]{QSignalBlocker blocker(table);table->setRowCount(0);for(int i=0;i<elements.size();i++){auto e=elements[i].toObject();const auto type=e.value("source").toObject().value("type").toString();if(e.value("kind")!="collection"||battleHudFields(type).empty())continue;int r=table->rowCount();table->insertRow(r);auto id=new QTableWidgetItem(e.value("id").toString());id->setData(Qt::UserRole,i);id->setFlags(id->flags()&~Qt::ItemIsEditable);table->setItem(r,0,id);table->setItem(r,1,new QTableWidgetItem(e.value("label").toString()));auto src=new QTableWidgetItem(type);src->setFlags(src->flags()&~Qt::ItemIsEditable);table->setItem(r,2,src);const QStringList keys{"x","y","width","height","rowHeight"};for(int c=3;c<8;c++){auto spin=new QSpinBox;spin->setRange(c<5?0:16,c==4||c>=6?2160:3840);spin->setValue(c==7?rowHeight(e):e.value(keys[c-3]).toInt(100));table->setCellWidget(r,c,spin);}}};
  auto append=[&](QString type,QString id,QString name,int x,int y,int width,int height,QString side={}){
    const double scale=qMin(1.0,qMin(visual.value("width").toDouble(1280)/1280,visual.value("height").toDouble(720)/720));
    auto expected=hudComponent("",type,scale);QString cid;for(auto value:components){auto c=value.toObject();if(c.value("id")!="hud-card-"+type.toLower()||c.value("width")!=expected.value("width")||c.value("height")!=expected.value("height"))continue;bool valid=true;for(auto required:expected.value("elements").toArray()){auto r=required.toObject();bool found=false;for(auto child:c.value("elements").toArray())if(child.toObject().value("id")==r.value("id")&&child.toObject().value("kind")==r.value("kind"))found=true;if(!found)valid=false;}if(valid)cid=c.value("id").toString();}
    // Presets never overwrite designer edits to shared components.
    if(cid.isEmpty()){cid=freshHudId(components,"hud-card-"+type.toLower());components.append(hudComponent(cid,type,scale));}
    auto e=QJsonObject{{"id",freshHudId(elements,id)},{"kind","collection"},{"label",name},{"x",x},{"y",y},{"width",width},{"height",height},{"source",QJsonObject{{"type",type}}},{"componentId",cid},{"layout","list"},{"gap",8},{"rowHeight",expected.value("height")},{"bindings",hudBindings(type)}};
    if(!side.isEmpty())e["rowFilter"]=QJsonObject{{"field","side"},{"operator","eq"},{"value",side}};elements.append(e);
  };
  connect(add,&QPushButton::clicked,&dialog,[&]{commit();append(source->currentData().toString(),"hud-list",source->currentText(),40,100,360,240);refill();});
  connect(remove,&QPushButton::clicked,&dialog,[&]{if(table->currentRow()<0)return;commit();int index=table->item(table->currentRow(),0)->data(Qt::UserRole).toInt();elements.removeAt(index);refill();});
  connect(preset,&QPushButton::clicked,&dialog,[&]{commit();for(auto e:elements)if(e.toObject().value("source").toObject().value("type")=="battleActions"){error->setText(tr("이미 전투 HUD가 있습니다. 목록 추가 또는 기존 목록을 수정하세요."));return;}
    QJsonArray retained;for(auto e:elements)if(e.toObject().value("kind")!="battle")retained.append(e);elements=retained;const int firstAdded=elements.size();
    append("battleActors","hud-ally",tr("아군 상태"),32,88,380,124,"ally");append("battleActors","hud-enemy",tr("적 상태"),868,88,380,124,"enemy");
    append("battleActions","hud-actions",tr("행동 선택"),32,242,380,424);append("battleTargets","hud-targets",tr("대상 선택"),444,88,392,124);
    append("battleLog","hud-log",tr("전투 로그"),444,302,392,364);append("battleRewards","hud-rewards",tr("지급된 보상"),868,242,380,424);
    auto confirm=hudPart(freshHudId(elements,"hud-confirm"),"button",444,236,244,50,tr("선택한 행동 실행"));confirm["action"]="confirmBattleAction";elements.append(confirm);
    auto cancel=hudPart(freshHudId(elements,"hud-cancel"),"button",700,236,136,50,tr("선택 취소"));cancel["action"]="cancelBattleAction";cancel["anchorX"]="right";elements.append(cancel);
    for(auto p:QList<QPair<QString,QString>>{{"save",tr("저장")},{"load",tr("불러오기")}}){bool exists=false;for(auto e:elements)if(e.toObject().value("kind")=="button"&&e.toObject().value("action")==p.first)exists=true;if(exists)continue;auto b=hudPart(freshHudId(elements,"hud-"+p.first),"button",p.first=="save"?940:1096,20,148,48,p.second);b["action"]=p.first;b["anchorX"]="right";elements.append(b);}
    for(int i=firstAdded;i<elements.size();i++){auto e=elements[i].toObject();const auto type=e.value("source").toObject().value("type").toString();if(e.value("x").toInt()>=868)e["anchorX"]="right";if(type=="battleTargets"||type=="battleLog"||e.value("action")=="confirmBattleAction")e["anchorX"]="stretch";if(type=="battleLog"||type=="battleRewards"||type=="battleActions")e["anchorY"]="stretch";for(auto key:{"x","y","width","height"}){bool horizontal=QString(key)=="x"||QString(key)=="width";double factor=horizontal?visual.value("width").toDouble(1280)/1280:visual.value("height").toDouble(720)/720;e[key]=qMax(QString(key)=="width"||QString(key)=="height"?16:0,qRound(e.value(key).toDouble()*factor));}elements[i]=e;}error->clear();refill();
  });
  connect(table,&QTableWidget::currentCellChanged,&dialog,[&](int r,int,int,int){if(r<0)return;const auto type=table->item(r,2)->text();detail->setText(tr("데이터 열: ")+battleHudFields(type).join(" · ")+tr("\n연결: 행동 선택 → 유효한 대상 선택 → 실행. 선택 자체는 턴이나 난수를 소비하지 않습니다."));});
  connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{commit();if(elements.size()>100){error->setText(tr("화면당 최대 100개 요소입니다."));return;}if(project.value("systems").toObject().isEmpty()){error->setText(tr("프로젝트에 전투 시스템 설정이 필요합니다. 먼저 게임 규칙의 전투 능력·적·탐험을 설정하세요."));return;}dialog.accept();});
  refill();if(dialog.exec()!=QDialog::Accepted)return;
  auto screens=visual.value("screens").toArray();for(int i=0;i<screens.size();i++){auto s=screens[i].toObject();if(s.value("id")==screenId){s["elements"]=elements;screens[i]=s;}}visual["screens"]=screens;visual["components"]=components;project["visual"]=visual;
  auto systems=project.value("systems").toObject();systems["battleHistoryLimit"]=logLimit->value();project["systems"]=systems;replaceProject(project);
}
