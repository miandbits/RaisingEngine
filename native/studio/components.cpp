#include "studio.hpp"
#include <QDoubleSpinBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QSignalBlocker>
#include <QSet>
#include <QVBoxLayout>

namespace {
QSet<QString> componentSubtree(const QJsonArray &elements,QString id){QSet<QString> result{id};bool changed=true;while(changed){changed=false;for(auto value:elements){auto e=value.toObject();auto key=e.value("id").toString();if(result.contains(e.value("parentId").toString())&&!result.contains(key)){result.insert(key);changed=true;}}}return result;}
QPoint componentOrigin(const QJsonArray &elements,QString id){QPoint p;QSet<QString> seen;while(!id.isEmpty()&&!seen.contains(id)){seen.insert(id);QJsonObject e;for(auto value:elements)if(value.toObject().value("id")==id)e=value.toObject();if(e.empty())break;p+=QPoint(e.value("x").toInt(),e.value("y").toInt());id=e.value("parentId").toString();}return p;}
QString componentHierarchyError(const QJsonArray &elements){QMap<QString,QJsonObject> byId;for(auto value:elements){auto e=value.toObject();auto id=e.value("id").toString();if(byId.contains(id))return QString::fromUtf8("중복 요소 ID: ")+id;byId[id]=e;}for(auto value:elements){auto e=value.toObject();auto id=e.value("id").toString(),parent=e.value("parentId").toString();QSet<QString> seen{id};int depth=0;while(!parent.isEmpty()){if(seen.contains(parent))return QString::fromUtf8("부모 순환: ")+id;if(!byId.contains(parent)||byId[parent].value("kind")!="group")return QString::fromUtf8("없는 부모 그룹: ")+parent;if(++depth>32)return QString::fromUtf8("부모 깊이는 32단계까지입니다: ")+id;seen.insert(parent);parent=byId[parent].value("parentId").toString();}}return {};}
QStringList overrideKeys(){return {"text","label","imageAssetId","action","screenId","locationId","shopId","itemId","interactionId","actionId","quantity","styleId","fill","color","fontSize","value","maxValue","battleAction","battleTarget"};}
QJsonObject primitive(QString id,QString kind,int x,int y,int w,int h,QString text={}){
  QJsonObject e{{"id",id},{"kind",kind},{"x",x},{"y",y},{"width",w},{"height",h}};
  if(!text.isEmpty())e["text"]=text;
  if(kind=="meter"){e["value"]=75;e["maxValue"]=100;}
  if(kind=="button"){e["action"]="navigate";e["screenId"]="home";e["fill"]="#365e52";e["color"]="#ffffff";}
  return e;
}
QJsonObject componentTemplate(QString id,QString type){
  QJsonArray elements;
  auto panel=primitive("background","panel",0,0,380,240);panel["fill"]="#f5eddb";panel["radius"]=12;panel["anchorX"]="stretch";panel["anchorY"]="stretch";elements.append(panel);
  auto title=primitive("title","text",20,20,340,44,type=="confirm"?QString::fromUtf8("구매하시겠습니까?"):QString::fromUtf8("상품 이름"));title["fontSize"]=26;title["color"]="#243b33";title["anchorX"]="stretch";elements.append(title);
  auto body=primitive("description","text",20,72,340,80,type=="confirm"?QString::fromUtf8("가격과 내용을 확인하세요."):QString::fromUtf8("상품 설명 · 가격"));body["color"]="#334e42";body["anchorX"]="stretch";body["anchorY"]="stretch";elements.append(body);
  auto button=primitive("confirm","button",20,172,type=="confirm"?164:340,48,type=="confirm"?QString::fromUtf8("확인"):QString::fromUtf8("자세히 보기"));button["anchorY"]="bottom";if(type!="confirm")button["anchorX"]="stretch";elements.append(button);
  if(type=="confirm"){auto cancel=primitive("cancel","button",196,172,164,48,QString::fromUtf8("취소"));cancel["anchorX"]="right";cancel["anchorY"]="bottom";elements.append(cancel);}
  if(type=="empty")elements={};
  return {{"id",id},{"label",type=="confirm"?QString::fromUtf8("확인창"):type=="empty"?QString::fromUtf8("새 부품"):QString::fromUtf8("상품 카드")},{"width",380},{"height",240},{"elements",elements}};
}

// Shared form for definition children and per-instance overrides. No JSON entry.
class ChildForm : public QWidget {
public:
  QMap<QString,QWidget*> fields;
  QJsonObject original;
  bool overrides;
  ChildForm(const QJsonObject& project,const QJsonArray& assets,bool instance,QWidget* parent=nullptr):QWidget(parent),overrides(instance){
    auto form=new QFormLayout(this);form->setContentsMargins(6,6,6,6);
    auto line=[&](QString key,QString label){auto f=new QLineEdit;f->setObjectName("child-"+key);fields[key]=f;form->addRow(label,f);};
    auto number=[&](QString key,QString label,int min,int max){auto f=new QSpinBox;f->setRange(min,max);f->setObjectName("child-"+key);fields[key]=f;form->addRow(label,f);};
    auto combo=[&](QString key,QString label){auto f=new QComboBox;f->setObjectName("child-"+key);f->addItem(QString::fromUtf8("지정 없음"),QString());fields[key]=f;form->addRow(label,f);return f;};
    line("label",tr("편집기 이름"));line("text",tr("문구"));
    if(!instance){auto parent=combo("parentId",tr("부모 그룹 ID"));auto clip=new QCheckBox(tr("그룹 밖 자식 자르기"));clip->setObjectName("child-clipChildren");fields["clipChildren"]=clip;form->addRow(clip);}
    if(!instance){number("x","X",0,3840);number("y","Y",0,2160);number("width",tr("너비"),16,3840);number("height",tr("높이"),16,2160);
      auto ax=combo("anchorX",tr("가로 앵커"));for(auto s:{"left","center","right","stretch"})ax->addItem(s,s);
      auto ay=combo("anchorY",tr("세로 앵커"));for(auto s:{"top","center","bottom","stretch"})ay->addItem(s,s);
    }
    number("fontSize",tr("글자 크기 (9=상속)"),9,120);line("fill",tr("배경색"));line("color",tr("글자색"));
    auto style=combo("styleId",tr("공통 스타일"));for(auto s:project["visual"].toObject()["styles"].toArray())style->addItem(s.toObject()["label"].toString(),s.toObject()["id"].toString());
    auto image=combo("imageAssetId",tr("이미지"));for(auto a:assets)if(a.toObject()["kind"]=="image")image->addItem(a.toObject()["id"].toString(),a.toObject()["id"].toString());
    auto action=combo("action",tr("클릭 행동"));for(auto p:QList<QPair<QString,QString>>{{"화면 이동","navigate"},{"장소 방문","visit"},{"상품 구매","buy"},{"교류","interact"},{"저장","save"},{"불러오기","load"},{"새 게임","start"},{"진행","advance"},{"소리","sound"},{"공통 행동 실행","executeAction"},{"전투 행동 선택","selectBattleAction"},{"전투 대상 선택","selectBattleTarget"},{"선택한 전투 행동 실행","confirmBattleAction"},{"전투 선택 취소","cancelBattleAction"}})action->addItem(p.first,p.second);
    const auto visual=project["visual"].toObject(),gameplay=project["gameplay"].toObject();
    for(auto p:QList<QPair<QString,QString>>{{"screenId","화면"},{"locationId","장소"},{"shopId","상점"},{"itemId","아이템"},{"interactionId","교류"},{"actionId","공통 행동"}}){auto f=combo(p.first,p.second);auto rows=p.first=="screenId"?visual["screens"].toArray():gameplay[p.first=="locationId"?"locations":p.first=="shopId"?"shops":p.first=="itemId"?"items":p.first=="actionId"?"actions":"interactions"].toArray();for(auto row:rows){auto o=row.toObject();f->addItem(o["label"].toString(o["name"].toString(o["id"].toString())),o["id"].toString());}}
    number("quantity",tr("실행 수량"),1,999);
    for(auto key:{"value","maxValue"}){auto f=new QDoubleSpinBox;f->setDecimals(12);f->setRange(QString(key)=="value"?-1e15:1e-12,1e15);f->setObjectName("child-"+QString(key));fields[key]=f;form->addRow(QString(key)=="value"?tr("게이지 값"):tr("게이지 최대값"),f);}
    auto battleAction=combo("battleAction",tr("전투 행동"));for(auto a:{"attack","guard","flee","useItem"})battleAction->addItem(a,a);
    auto battleTarget=combo("battleTarget",tr("전투 대상"));battleTarget->addItem(tr("자신"),"self");battleTarget->addItem(tr("적"),"enemy");
  }
  void load(QJsonObject e){
    original=e;setEnabled(!e.isEmpty());
    for(auto it=fields.begin();it!=fields.end();++it){const auto key=it.key();
      if(auto f=qobject_cast<QCheckBox*>(it.value())){f->setChecked(e.value(key).toBool());f->setVisible(e.value("kind")=="group");}
      if(auto f=qobject_cast<QLineEdit*>(it.value()))f->setText(e[key].toString());
      if(auto f=qobject_cast<QDoubleSpinBox*>(it.value())){f->setValue(e.value(key).toDouble(key=="maxValue"?100:0));f->setProperty("loadedValue",f->value());}
      if(auto f=qobject_cast<QSpinBox*>(it.value())){if(key=="x"||key=="y")f->setMinimum(e.contains("parentId")?(key=="x"?-3840:-2160):0);f->setValue(e[key].toInt(key=="fontSize"?9:key=="quantity"?1:key=="maxValue"?100:0));}
      if(auto f=qobject_cast<QComboBox*>(it.value())){auto id=e[key].toString();int at=f->findData(id);if(at<0){f->addItem(id,id);at=f->count()-1;}f->setCurrentIndex(at);}
    }
  }
  QJsonObject value() const {
    auto out=original;if(out.isEmpty())return out;
    for(auto it=fields.begin();it!=fields.end();++it){const auto key=it.key();QJsonValue value;
      if(auto f=qobject_cast<QCheckBox*>(it.value())){if(out.value("kind")!="group")continue;value=f->isChecked();}
      if(auto f=qobject_cast<QLineEdit*>(it.value()))value=f->text();
      if(auto f=qobject_cast<QComboBox*>(it.value()))value=f->isEditable()&&f->currentIndex()<0?f->currentText():f->currentData().toString();
      if(auto f=qobject_cast<QSpinBox*>(it.value()))value=f->value();
      if(auto f=qobject_cast<QDoubleSpinBox*>(it.value())){if(original.contains(key)&&f->value()==f->property("loadedValue").toDouble())value=original.value(key);else value=f->value();}
      if((value.isString()&&value.toString().isEmpty()&&key!="text")||(key=="fontSize"&&value.toInt()==9)||(key=="quantity"&&out["kind"]!="button")){out.remove(key);continue;}
      if((key=="value"||key=="maxValue")&&out["kind"]!="meter")continue;
      if(key=="text"&&!original.contains(key)&&value.toString().isEmpty())continue;
      if(key=="quantity"&&!original.contains(key)&&value.toInt()==1)continue;
      out[key]=value;
    }
    return out;
  }
};
}

void StudioWindow::editComponents(){
  if(bridge.busy()||projectData().isEmpty())return;
  auto project=projectData(),visual=project["visual"].toObject();auto components=visual["components"].toArray();
  const auto screenRows=visual["screens"].toArray();QString defaultTarget=screenId;
  if(defaultTarget.isEmpty()&&!screenRows.isEmpty())defaultTarget=screenRows.first().toObject()["id"].toString();
  for(auto s:screenRows)if(s.toObject()["id"]=="home")defaultTarget="home";
  auto defaultAction=[&](QJsonObject e){if(e["kind"]=="button"){if(project.contains("gameplay"))e["screenId"]=defaultTarget;else{e["action"]="sound";e.remove("screenId");}}return e;};
  QDialog dialog(this);dialog.setObjectName("componentsDialog");dialog.setWindowTitle(tr("재사용 UI 부품 작업실"));dialog.resize(1280,840);
  auto layout=new QVBoxLayout(&dialog);auto note=new QLabel(tr("원본을 수정하면 모든 배치본에 반영됩니다. 배치본에서 덮어쓴 문구·행동은 유지됩니다. 부품 내부는 패널·문구·이미지·버튼으로 구성합니다."));note->setWordWrap(true);layout->addWidget(note);
  auto split=new QSplitter;layout->addWidget(split,1);
  auto library=new QWidget;auto left=new QVBoxLayout(library);left->setContentsMargins(0,0,0,0);
  auto list=new QListWidget;list->setObjectName("componentLibrary");left->addWidget(list,1);
  auto type=new QComboBox;type->setObjectName("componentTemplate");type->addItem(tr("상품 카드"),"card");type->addItem(tr("확인창"),"confirm");type->addItem(tr("빈 부품"),"empty");left->addWidget(type);
  auto add=new QPushButton(tr("＋ 부품 만들기"));add->setObjectName("addComponent");left->addWidget(add);
  auto remove=new QPushButton(tr("부품 삭제"));remove->setObjectName("deleteComponent");left->addWidget(remove);split->addWidget(library);
  auto center=new QWidget;auto middle=new QVBoxLayout(center);auto metadata=new QHBoxLayout;
  auto name=new QLineEdit;name->setObjectName("componentName");metadata->addWidget(name,1);
  auto width=new QSpinBox;auto height=new QSpinBox;width->setRange(16,3840);height->setRange(16,2160);width->setObjectName("componentWidth");height->setObjectName("componentHeight");metadata->addWidget(width);metadata->addWidget(height);middle->addLayout(metadata);
  auto preview=new ScreenCanvas;preview->setObjectName("componentCanvas");middle->addWidget(preview,1);
  auto children=new QListWidget;children->setObjectName("componentChildren");children->setMaximumHeight(150);middle->addWidget(children);
  auto childBar=new QHBoxLayout;auto kind=new QComboBox;kind->setObjectName("componentChildKind");for(auto p:QList<QPair<QString,QString>>{{"문구","text"},{"버튼","button"},{"패널","panel"},{"이미지","image"},{"게이지","meter"},{"그룹","group"}})kind->addItem(p.first,p.second);childBar->addWidget(kind);
  auto addChild=new QPushButton(tr("＋ 요소"));addChild->setObjectName("addComponentChild");childBar->addWidget(addChild);auto deleteChild=new QPushButton(tr("요소 삭제"));deleteChild->setObjectName("deleteComponentChild");childBar->addWidget(deleteChild);auto duplicateChild=new QPushButton(tr("요소 복제"));duplicateChild->setObjectName("duplicateComponentChild");childBar->addWidget(duplicateChild);auto applyChild=new QPushButton(tr("속성 반영"));applyChild->setObjectName("applyComponentChild");childBar->addWidget(applyChild);middle->addLayout(childBar);split->addWidget(center);
  auto form=new ChildForm(project,assets,false);auto scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setWidget(form);split->addWidget(scroll);split->setSizes({200,740,310});
  auto error=new QLabel;error->setObjectName("componentError");error->setWordWrap(true);layout->addWidget(error);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);auto place=buttons->addButton(tr("저장하고 현재 화면에 배치"),QDialogButtonBox::ActionRole);place->setObjectName("placeComponent");layout->addWidget(buttons);
  int current=-1,child=-1;bool fillingDialog=false;QString placed;
  auto commit=[&]{if(fillingDialog||current<0)return;auto c=components[current].toObject();c["label"]=name->text();c["width"]=width->value();c["height"]=height->value();auto elements=c["elements"].toArray();if(child>=0&&child<elements.size())elements[child]=form->value();c["elements"]=elements;components[current]=c;};
  std::function<void(bool)> render=[&](bool fit){if(current<0)return;auto p=project;auto v=visual;auto c=components[current].toObject();v["width"]=c["width"];v["height"]=c["height"];p["visual"]=v;p["_defaultFontPath"]=engineRoot+"/dist/native-player-app/fonts/NotoSansKR.ttf";preview->setScreen(p,c,assets,child<0?QString():c["elements"].toArray()[child].toObject()["id"].toString(),fit);};
  auto loadChildren=[&]{fillingDialog=true;children->clear();if(current>=0){auto c=components[current].toObject();name->setText(c["label"].toString());width->setValue(c["width"].toInt());height->setValue(c["height"].toInt());for(auto e:c["elements"].toArray())children->addItem(e.toObject()["id"].toString()+" · "+e.toObject()["kind"].toString());}child=-1;form->load({});fillingDialog=false;render(true);};
  for(auto c:components)list->addItem(c.toObject()["label"].toString());
  connect(list,&QListWidget::currentRowChanged,&dialog,[&](int row){commit();current=row;loadChildren();});
  connect(children,&QListWidget::currentRowChanged,&dialog,[&](int row){if(fillingDialog)return;commit();child=row;if(auto parents=qobject_cast<QComboBox*>(form->fields.value("parentId"))){parents->clear();parents->addItem(tr("최상위"),QString());if(current>=0&&row>=0){auto es=components[current].toObject().value("elements").toArray();auto excluded=componentSubtree(es,es[row].toObject().value("id").toString());for(auto v:es){auto g=v.toObject();if(g.value("kind")=="group"&&!excluded.contains(g.value("id").toString()))parents->addItem(g.value("id").toString(),g.value("id").toString());}}}form->load(current>=0&&row>=0?components[current].toObject()["elements"].toArray()[row].toObject():QJsonObject{});render(false);});
  connect(preview,&ScreenCanvas::selectionChanged,&dialog,[&](QString id){if(current<0)return;auto es=components[current].toObject()["elements"].toArray();for(int i=0;i<es.size();i++)if(es[i].toObject()["id"]==id)children->setCurrentRow(i);});
  connect(preview,&ScreenCanvas::geometryEdited,&dialog,[&](QString id,QRect r){commit();auto c=components[current].toObject();auto es=c["elements"].toArray();for(int i=0;i<es.size();i++){auto e=es[i].toObject();if(e["id"]!=id)continue;e["x"]=r.x();e["y"]=r.y();e["width"]=r.width();e["height"]=r.height();es[i]=e;if(i==child)form->load(e);}c["elements"]=es;components[current]=c;render(false);});
  if(auto parents=qobject_cast<QComboBox*>(form->fields.value("parentId")))connect(parents,qOverload<int>(&QComboBox::activated),&dialog,[&](int){if(fillingDialog||current<0||child<0)return;auto c=components[current].toObject();auto es=c.value("elements").toArray();auto old=es[child].toObject(),draft=form->value();auto oldParent=old.value("parentId").toString(),newParent=draft.value("parentId").toString();if(oldParent==newParent)return;auto position=QPoint(draft.value("x").toInt(),draft.value("y").toInt())+componentOrigin(es,oldParent)-componentOrigin(es,newParent);draft["x"]=position.x();draft["y"]=position.y();es[child]=draft;auto problem=componentHierarchyError(es);if(problem.isEmpty()&&(position.x()<-3840||position.x()>3840||position.y()<-2160||position.y()>2160||(newParent.isEmpty()&&(position.x()<0||position.y()<0))))problem=tr("좌표 범위를 벗어납니다. 먼저 요소를 화면 안으로 이동하세요.");if(!problem.isEmpty()){error->setText(problem);form->load(old);return;}form->load(draft);commit();error->clear();render(false);});
  connect(applyChild,&QPushButton::clicked,&dialog,[&]{commit();render(false);});
  connect(add,&QPushButton::clicked,&dialog,[&]{commit();int n=1;auto exists=[&](QString id){for(auto c:components)if(c.toObject()["id"]==id)return true;return false;};while(exists("component-"+QString::number(n)))++n;auto c=componentTemplate("component-"+QString::number(n),type->currentData().toString());auto es=c["elements"].toArray();for(int i=0;i<es.size();i++)es[i]=defaultAction(es[i].toObject());c["elements"]=es;components.append(c);list->addItem(c["label"].toString());list->setCurrentRow(components.size()-1);});
  connect(addChild,&QPushButton::clicked,&dialog,[&]{if(current<0)return;commit();auto c=components[current].toObject();auto es=c["elements"].toArray();QSet<QString> ids;for(auto e:es)ids.insert(e.toObject()["id"].toString());int n=1;while(ids.contains("element-"+QString::number(n)))++n;auto e=primitive("element-"+QString::number(n),kind->currentData().toString(),0,0,qMin(180,c["width"].toInt()),qMin(48,c["height"].toInt()),tr("새 요소"));es.append(defaultAction(e));c["elements"]=es;components[current]=c;loadChildren();children->setCurrentRow(es.size()-1);});
  connect(deleteChild,&QPushButton::clicked,&dialog,[&]{if(current<0||child<0)return;commit();auto c=components[current].toObject();auto es=c.value("elements").toArray();auto removed=componentSubtree(es,es[child].toObject().value("id").toString());for(auto screen:visual.value("screens").toArray())for(auto value:screen.toObject().value("elements").toArray()){auto e=value.toObject();if(e.value("componentId")!=c.value("id"))continue;for(auto key:{"overrides","bindings"})for(auto reference:e.value(key).toArray())if(removed.contains(reference.toObject().value("elementId").toString())){error->setText(tr("삭제할 요소를 참조하는 배치본 덮어쓰기·데이터 연결을 먼저 해제하세요."));return;}}if(removed.size()>1&&QMessageBox::question(&dialog,tr("그룹 삭제"),tr("그룹과 자식 %1개를 삭제할까요?").arg(removed.size()-1))!=QMessageBox::Yes)return;QJsonArray kept;for(auto value:es)if(!removed.contains(value.toObject().value("id").toString()))kept.append(value);c["elements"]=kept;components[current]=c;loadChildren();});
  connect(duplicateChild,&QPushButton::clicked,&dialog,[&]{if(current<0||child<0)return;commit();auto c=components[current].toObject();auto es=c.value("elements").toArray();auto original=es[child].toObject();auto rootId=original.value("id").toString();auto subtree=componentSubtree(es,rootId);if(es.size()+subtree.size()>100){error->setText(tr("부품당 최대 100개 요소입니다."));return;}QSet<QString> ids;for(auto value:es)ids.insert(value.toObject().value("id").toString());QMap<QString,QString> copies;for(auto value:es){auto id=value.toObject().value("id").toString();if(!subtree.contains(id))continue;auto base=id.left(45)+"-copy";int n=1;while(ids.contains(base+QString::number(n)))++n;copies[id]=base+QString::number(n);ids.insert(copies[id]);}QJsonArray added;for(auto value:es){auto e=value.toObject();auto id=e.value("id").toString();if(!copies.contains(id))continue;e["id"]=copies[id];if(copies.contains(e.value("parentId").toString()))e["parentId"]=copies[e.value("parentId").toString()];if(id==rootId)e["label"]=e.value("label").toString(id)+tr(" 복사");added.append(e);}for(auto value:added)es.append(value);c["elements"]=es;components[current]=c;loadChildren();for(int i=0;i<es.size();++i)if(es[i].toObject().value("id")==copies[rootId])children->setCurrentRow(i);});
  connect(remove,&QPushButton::clicked,&dialog,[&]{if(current<0)return;auto id=components[current].toObject()["id"];for(auto s:visual["screens"].toArray())for(auto e:s.toObject()["elements"].toArray())if(e.toObject()["componentId"]==id){error->setText(tr("사용 중인 부품입니다. 화면의 배치본을 먼저 삭제하세요."));return;}components.removeAt(current);current=-1;child=-1;const QSignalBlocker block(list);list->clear();for(auto c:components)list->addItem(c.toObject()["label"].toString());form->load({});children->clear();});
  auto accept=[&]{commit();for(auto c:components){auto o=c.toObject();auto hierarchyError=componentHierarchyError(o.value("elements").toArray());if(!hierarchyError.isEmpty()){error->setText(hierarchyError);return;}for(auto e:o["elements"].toArray()){auto r=e.toObject();if(r.contains("parentId"))continue;if(r.value("x").toInt()<0||r.value("y").toInt()<0||r["x"].toInt()+r["width"].toInt()>o["width"].toInt()||r["y"].toInt()+r["height"].toInt()>o["height"].toInt()){error->setText(tr("부품 크기 밖의 요소가 있습니다: ")+r["id"].toString());return;}}}dialog.accept();};
  connect(buttons,&QDialogButtonBox::accepted,&dialog,accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  connect(place,&QPushButton::clicked,&dialog,[&]{if(current<0||currentScreen().isEmpty()){error->setText(tr("배치할 화면과 부품을 선택하세요."));return;}accept();if(dialog.result()==QDialog::Accepted)placed=components[current].toObject()["id"].toString();});
  if(list->count())list->setCurrentRow(0);else loadChildren();
  if(dialog.exec()!=QDialog::Accepted)return;
  visual["components"]=components;
  if(!placed.isEmpty()){
    auto screens=visual["screens"].toArray();for(int i=0;i<screens.size();i++){auto s=screens[i].toObject();if(s["id"]!=screenId)continue;auto es=s["elements"].toArray();QSet<QString> ids;for(auto e:es)ids.insert(e.toObject()["id"].toString());int n=1;while(ids.contains("instance-"+QString::number(n)))++n;selection="instance-"+QString::number(n);for(auto c:components)if(c.toObject()["id"]==placed){auto o=c.toObject();es.append(QJsonObject{{"id",selection},{"kind","component"},{"componentId",placed},{"label",o["label"]},{"x",0},{"y",0},{"width",o["width"]},{"height",o["height"]}});}s["elements"]=es;screens[i]=s;}visual["screens"]=screens;
  }
  project["visual"]=visual;replaceProject(project);
}

void StudioWindow::editComponentInstance(){
  if(bridge.busy()||currentElement()["kind"]!="component")return;
  auto instance=currentElement();QJsonObject component;for(auto c:projectData()["visual"].toObject()["components"].toArray())if(c.toObject()["id"]==instance["componentId"])component=c.toObject();if(component.isEmpty())return;
  QDialog dialog(this);dialog.setObjectName("componentOverridesDialog");dialog.setWindowTitle(tr("배치본 문구·행동 · 원본과 다른 값만 저장"));dialog.resize(640,820);auto layout=new QVBoxLayout(&dialog);
  auto source=new QLabel(tr("원본: ")+component["label"].toString()+tr(" · 초기화하면 원본을 다시 따릅니다."));layout->addWidget(source);
  auto list=new QComboBox;list->setObjectName("overrideChild");for(auto e:component["elements"].toArray())list->addItem(e.toObject()["id"].toString(),e.toObject()["id"].toString());layout->addWidget(list);
  auto form=new ChildForm(projectData(),assets,true);auto scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setWidget(form);layout->addWidget(scroll,1);
  auto reset=new QPushButton(tr("이 요소의 덮어쓰기 초기화"));reset->setObjectName("resetComponentOverride");layout->addWidget(reset);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);
  auto overrides=instance["overrides"].toArray();int current=-1;
  auto commit=[&]{if(current<0)return;auto base=component["elements"].toArray()[current].toObject(),edited=form->value();QJsonObject diff;for(auto key:overrideKeys())if(edited.contains(key)&&edited[key]!=base[key])diff[key]=edited[key];QJsonArray next;for(auto o:overrides)if(o.toObject()["elementId"]!=base["id"])next.append(o);if(!diff.isEmpty())next.append(QJsonObject{{"elementId",base["id"]},{"values",diff}});overrides=next;};
  auto load=[&](int row){commit();current=row;if(row<0){form->load({});return;}auto e=component["elements"].toArray()[row].toObject();for(auto o:overrides)if(o.toObject()["elementId"]==e["id"]){auto values=o.toObject()["values"].toObject();for(auto it=values.begin();it!=values.end();++it)e[it.key()]=it.value();}form->load(e);};
  connect(list,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,load);
  connect(reset,&QPushButton::clicked,&dialog,[&]{if(current>=0)form->load(component["elements"].toArray()[current].toObject());});
  connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{commit();dialog.accept();});connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);load(list->currentIndex());
  if(dialog.exec()==QDialog::Accepted)updateElement([&](QJsonObject&e){if(overrides.isEmpty())e.remove("overrides");else e["overrides"]=overrides;});
}
