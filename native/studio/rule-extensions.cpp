#include "rule-extensions.hpp"
#include "rule-package-editor.hpp"
#include "studio.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include <limits>
#include <memory>

QJsonObject ruleProject(const QJsonObject &context) {
  for(const auto &v:context.value("documents").toArray())if(v.toObject().value("kind")=="project")return v.toObject().value("data").toObject();
  return {};
}
QJsonArray ruleDefinitions(const QJsonObject &project,const QString &kind) {
  QJsonArray result;
  for(const auto &p:project.value("ruleExtensions").toObject().value("packages").toArray()) {
    const auto pack=p.toObject();
    for(const auto &d:pack.value(kind).toArray()) {auto def=d.toObject();def["packageId"]=pack.value("id");def["version"]=pack.value("version");def["packageName"]=pack.value("name");result.append(def);}
  }
  return result;
}
// Reference catalog only. Compiled actions remain owned by the shared compiler.
QJsonArray ruleSelectableActions(const QJsonObject &project, const QJsonArray &referenced) {
  auto result=project.value("gameplay").toObject().value("actions").toArray();
  const auto definitions=ruleDefinitions(project,"progressions");
  for(const auto &value:project.value("ruleExtensions").toObject().value("progressions").toArray()) {
    const auto instance=value.toObject();
    for(const auto &entry:definitions) {
      const auto definition=entry.toObject();
      if(definition.value("packageId")!=instance.value("packageId")||definition.value("id")!=instance.value("ruleId")||definition.value("version")!=instance.value("version"))continue;
      for(const auto &value:definition.value("transitions").toArray()) {
        const auto transition=value.toObject();
        result.append(QJsonObject{{"id","rx-"+instance.value("id").toString()+"-"+transition.value("id").toString()},
          {"name",instance.value("name").toString()+" · "+transition.value("name").toString()}});
      }
    }
  }
  for(const auto &id:referenced) {
    bool present=false;for(const auto &entry:result)if(entry.toObject().value("id")==id)present=true;
    if(!present)result.append(QJsonObject{{"id",id},{"name",QObject::tr("누락된 행동 — 연결을 확인하세요")}});
  }
  return result;
}
QJsonObject defaultRuleCall(const QJsonObject &d,const QString &op) {
  QJsonObject args;const auto parameters=d.value("parameters").toObject();
  for(auto it=parameters.begin();it!=parameters.end();++it)if(it.value().toObject().contains("default"))args[it.key()]=it.value().toObject().value("default");
  return {{"op",op},{"packageId",d.value("packageId")},{"version",d.value("version")},{"ruleId",d.value("id")},{"args",args}};
}
QJsonArray ruleProgressionGroups(const QJsonObject &project) {
  QJsonArray result;
  for(const auto &v:project.value("ruleExtensions").toObject().value("progressions").toArray()) {
    const auto instance=v.toObject();
    for(const auto &d:ruleDefinitions(project,"progressions")){const auto definition=d.toObject();if(definition.value("packageId")==instance.value("packageId")&&definition.value("id")==instance.value("ruleId")&&definition.value("version")==instance.value("version"))result.append(QJsonObject{{"id","rx-"+instance.value("id").toString()},{"name",instance.value("name")},{"scope",instance.value("scope")},{"initialValueId",definition.value("initial")},{"values",definition.value("states")}});}
  }
  return result;
}
namespace {
QJsonArray referenceOptions(const QJsonObject &context,const QString &type) {
  QJsonArray out;const auto project=ruleProject(context);
  for(const auto &v:context.value("documents").toArray()) {
    const auto d=v.toObject(), data=d.value("data").toObject();const auto kind=d.value("kind").toString();
    if(kind==type)out.append(data);
    if(kind=="attributes"&&type=="attribute")out=data.value("attributes").toArray();
    if(kind=="flags"&&type=="flag")out=data.value("flags").toArray();
  }
  if(type=="item")out=project.value("gameplay").toObject().value("items").toArray();
  if(type=="stateGroup"){out=project.value("stateGroups").toArray();for(const auto &g:ruleProgressionGroups(project))out.append(g);}
  if(type=="stateValue")for(const auto &g:project.value("stateGroups").toArray())for(const auto &s:g.toObject().value("values").toArray())out.append(s);
  return out;
}
void argumentsForm(QFormLayout *form,const QJsonObject &definition,const QJsonObject &args,const QJsonObject &context,std::function<void(QString,QJsonValue)> changed) {
  const auto params=definition.value("parameters").toObject();
  for(auto it=params.begin();it!=params.end();++it) {
    const auto key=it.key();const auto p=it.value().toObject();const auto type=p.value("type").toString();const auto value=args.value(key);QWidget *widget=nullptr;
    if(type=="integer") {
      auto f=new QSpinBox;f->setRange(p.value("min").toInt(std::numeric_limits<int>::min()),p.value("max").toInt(std::numeric_limits<int>::max()));f->setValue(value.toInt());
      QObject::connect(f,qOverload<int>(&QSpinBox::valueChanged),f,[=](int n){changed(key,n);});widget=f;
    } else if(type=="boolean") {
      auto f=new QCheckBox;f->setChecked(value.toBool());QObject::connect(f,&QCheckBox::toggled,f,[=](bool v){changed(key,v);});widget=f;
    } else if(type=="text") {
      auto f=new QLineEdit(value.toString());f->setMaxLength(200);QObject::connect(f,&QLineEdit::textChanged,f,[=](const QString &s){changed(key,s);});widget=f;
    } else {
      auto f=new QComboBox;f->addItem(QObject::tr("대상을 선택하세요"),QString());
      for(const auto &v:referenceOptions(context,type)){const auto o=v.toObject();f->addItem(o.value("name").toString(o.value("label").toString(o.value("id").toString())),o.value("id").toString());}
      auto selected=f->findData(value.toString());if(selected<0&&!value.toString().isEmpty()){f->addItem(QObject::tr("없는 대상: ")+value.toString(),value.toString());selected=f->count()-1;}f->setCurrentIndex(qMax(0,selected));
      QObject::connect(f,qOverload<int>(&QComboBox::currentIndexChanged),f,[=]{changed(key,f->currentData().toString());});widget=f;
    }
    widget->setObjectName("ruleArg-"+key);form->addRow(p.value("name").toString(key),widget);
  }
}
}
void ruleArgumentFields(QFormLayout *form,const QJsonObject &definition,const QJsonObject &args,const QJsonObject &context,std::function<void(QString,QJsonValue)> changed) {
  argumentsForm(form,definition,args,context,changed);
}
QString ruleReferenceName(const QJsonObject &context,const QString &type,const QString &id) {
  for(const auto &v:referenceOptions(context,type)){const auto o=v.toObject();if(o.value("id")==id)return o.value("name").toString(o.value("label").toString(id));}
  return id;
}
QWidget *ruleCallForm(QJsonObject call,QString kind,QJsonObject context,std::function<void(QJsonObject)> changed,QWidget *parent) {
  auto widget=new QWidget(parent);auto box=new QVBoxLayout(widget);auto choice=new QComboBox;choice->setObjectName("ruleDefinition");box->addWidget(choice);
  const auto definitions=ruleDefinitions(ruleProject(context),kind);int selected=-1;
  for(const auto &v:definitions){const auto d=v.toObject();choice->addItem(d.value("packageName").toString()+" · "+d.value("name").toString()+" ("+d.value("version").toString()+")",d);if(d.value("packageId")==call.value("packageId")&&d.value("id")==call.value("ruleId")&&d.value("version")==call.value("version"))selected=choice->count()-1;}
  if(selected<0){choice->addItem(QObject::tr("규칙을 선택하세요 / 원본 참조 없음"));selected=choice->count()-1;}choice->setCurrentIndex(selected);
  auto page=new QWidget;auto form=new QFormLayout(page);box->addWidget(page);
  const auto definition=choice->currentData().toJsonObject();
  auto current=std::make_shared<QJsonObject>(call);
  argumentsForm(form,definition,call.value("args").toObject(),context,[current,changed](QString key,QJsonValue value){auto args=current->value("args").toObject();args[key]=value;(*current)["args"]=args;changed(*current);});
  QObject::connect(choice,qOverload<int>(&QComboBox::activated),widget,[=]{const auto d=choice->currentData().toJsonObject();if(!d.isEmpty())changed(defaultRuleCall(d,kind=="conditions"?"ruleCondition":"ruleEffect"));});
  return widget;
}

RuleExtensionDialog::RuleExtensionDialog(QJsonObject original,QJsonObject context,QString screenId,QWidget *parent):QDialog(parent),project(original) {
  setObjectName("ruleExtensionDialog");setWindowTitle(tr("규칙 확장 패키지 · 진행 방식"));resize(900,720);auto box=new QVBoxLayout(this);
  auto help=new QLabel(tr("확장 패키지는 조건·효과 편집기의 ‘확장 조건 / 효과’에 나타납니다. 진행 방식을 추가하면 상태와 행동이 자동 생성됩니다. 엔진을 다시 빌드하지 않습니다. 저장·실행 전에 공통 검증으로 참조와 매개변수를 확인합니다."));help->setWordWrap(true);box->addWidget(help);
  auto path=new QLineEdit;path->setObjectName("rulePackagePath");path->setPlaceholderText(tr(".raiserules 패키지 경로"));auto importRow=new QHBoxLayout;importRow->addWidget(path,1);auto browse=new QPushButton(tr("찾기…"));auto install=new QPushButton(tr("패키지 가져오기"));install->setObjectName("importRulePackage");importRow->addWidget(browse);importRow->addWidget(install);box->addLayout(importRow);
  auto packs=new QListWidget;packs->setObjectName("rulePackages");packs->setMaximumHeight(100);box->addWidget(packs);
  auto packRow=new QHBoxLayout;auto create=new QPushButton(tr("새 패키지 제작…"));create->setObjectName("createRulePackage");auto editPack=new QPushButton(tr("선택 패키지 편집…"));editPack->setObjectName("editRulePackage");packRow->addWidget(create);packRow->addWidget(editPack);box->addLayout(packRow);
  auto definitions=new QComboBox;definitions->setObjectName("progressionDefinition");box->addWidget(definitions);
  auto instances=new QListWidget;instances->setObjectName("ruleProgressions");instances->setMaximumHeight(100);box->addWidget(instances);
  auto row=new QHBoxLayout;auto add=new QPushButton(tr("선택 진행 방식 추가"));add->setObjectName("addRuleProgression");auto remove=new QPushButton(tr("선택 진행 삭제"));remove->setObjectName("removeRuleProgression");row->addWidget(add);row->addWidget(remove);box->addLayout(row);
  auto reconnect=new QPushButton(tr("선택 진행을 위 정의·버전에 다시 연결 (입력은 기본값으로)"));reconnect->setObjectName("reconnectRuleProgression");box->addWidget(reconnect);
  auto scroll=new QScrollArea;scroll->setWidgetResizable(true);auto page=new QWidget;auto form=new QFormLayout(page);scroll->setWidget(page);box->addWidget(scroll,1);
  auto error=new QLabel;error->setObjectName("ruleExtensionStatus");error->setWordWrap(true);box->addWidget(error);
  auto place=new QPushButton(tr("선택 진행의 행동 목록을 현재 화면에 배치"));place->setObjectName("placeRuleActions");place->setEnabled(!screenId.isEmpty());box->addWidget(place);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);box->addWidget(buttons);
  auto library=[this]{auto lib=project.value("ruleExtensions").toObject();if(lib.isEmpty())lib={{"apiVersion",1},{"packages",QJsonArray{}},{"progressions",QJsonArray{}}};return lib;};
  auto refill=[=]{packs->clear();definitions->clear();const auto lib=library();for(const auto &p:lib.value("packages").toArray()){const auto d=p.toObject();packs->addItem(d.value("name").toString()+" · "+d.value("version").toString()+tr(" · 프로젝트에 고정"));}for(const auto &d:ruleDefinitions(project,"progressions"))definitions->addItem(d.toObject().value("name").toString(),d.toObject());};
  auto refillInstances=[=](int selected){instances->clear();for(const auto &i:library().value("progressions").toArray())instances->addItem(i.toObject().value("name").toString());instances->setCurrentRow(selected);};
  auto workshop=[=](bool existing){const int index=existing?packs->currentRow():-1;auto lib=library();auto list=lib.value("packages").toArray();if(existing&&(index<0||index>=list.size()))return;const auto originalPack=existing?list[index].toObject():QJsonObject{};auto currentContext=context;auto docs=currentContext.value("documents").toArray();for(int n=0;n<docs.size();++n){auto doc=docs[n].toObject();if(doc.value("kind")=="project"){doc["data"]=project;docs[n]=doc;}}currentContext["documents"]=docs;RulePackageEditor editor(originalPack,currentContext,this);if(editor.exec()!=QDialog::Accepted)return;const auto next=editor.value();if(existing)list[index]=next;else list.append(next);lib["packages"]=list;project["ruleExtensions"]=lib;refill();packs->setCurrentRow(existing?index:list.size()-1);refillInstances(instances->currentRow());error->setText(tr("패키지 초안을 반영했습니다. 적용 후 공통 검증을 실행하세요. 버전을 바꾸면 기존 호출은 이전 버전에 고정되어 오류로 표시됩니다. 호출을 명시적으로 다시 연결하고 기존 저장 파일의 호환성도 확인하세요."));};
  connect(create,&QPushButton::clicked,this,[=]{workshop(false);});connect(editPack,&QPushButton::clicked,this,[=]{workshop(true);});
  connect(reconnect,&QPushButton::clicked,this,[=]{const int index=instances->currentRow();auto lib=library();auto list=lib.value("progressions").toArray();const auto d=definitions->currentData().toJsonObject();if(index<0||index>=list.size()||d.isEmpty())return;auto previous=list[index].toObject();auto next=defaultRuleCall(d,{});next.remove("op");for(const auto &key:QStringList{"id","name","scope"})next[key]=previous.value(key);list[index]=next;lib["progressions"]=list;project["ruleExtensions"]=lib;refillInstances(index);error->setText(tr("진행 정의·버전을 다시 연결했습니다. 입력을 기본값으로 초기화했으니 확인하고 검증하세요. 이전 저장은 자동 변환하지 않습니다."));});
  connect(browse,&QPushButton::clicked,this,[=]{const auto file=QFileDialog::getOpenFileName(this,tr("규칙 패키지"),{},tr("규칙 패키지 (*.raiserules)"),nullptr,QFileDialog::DontUseNativeDialog);if(!file.isEmpty())path->setText(file);});
  connect(install,&QPushButton::clicked,this,[=]{QFile file(path->text());if(!file.open(QIODevice::ReadOnly)||file.size()>2*1024*1024){error->setText(tr("2 MiB 이하 패키지 파일을 읽을 수 없습니다."));return;}QJsonParseError parse;const auto doc=QJsonDocument::fromJson(file.readAll(),&parse);const auto p=doc.object();if(parse.error!=QJsonParseError::NoError||p.value("apiVersion")!=1||p.value("id").toString().isEmpty()||p.value("version").toString().isEmpty()){error->setText(tr("규칙 API 1 패키지 형식이 아닙니다."));return;}auto lib=library();auto list=lib.value("packages").toArray();for(const auto &v:list)if(v.toObject().value("id")==p.value("id")){error->setText(v==p?tr("동일한 패키지가 이미 있습니다."):tr("같은 ID의 패키지가 있습니다. 자동으로 덮어쓰지 않습니다."));return;}list.append(p);lib["packages"]=list;project["ruleExtensions"]=lib;refill();error->setText(tr("가져왔습니다. 적용 후 검증에서 본문·참조·버전을 확인하세요."));});
  connect(add,&QPushButton::clicked,this,[=]{const auto d=definitions->currentData().toJsonObject();if(d.isEmpty())return;auto lib=library();auto list=lib.value("progressions").toArray();QString id;for(int i=1;;i++){id="flow-"+QString::number(i);bool exists=false;for(const auto &v:list)if(v.toObject().value("id")==id)exists=true;if(!exists)break;}auto item=defaultRuleCall(d,{});item.remove("op");item["id"]=id;item["name"]=d.value("name");item["scope"]="shared";list.append(item);lib["progressions"]=list;project["ruleExtensions"]=lib;refillInstances(list.size()-1);});
  connect(remove,&QPushButton::clicked,this,[=]{const int index=instances->currentRow();auto lib=library();auto list=lib.value("progressions").toArray();if(index<0||index>=list.size())return;list.removeAt(index);lib["progressions"]=list;project["ruleExtensions"]=lib;refillInstances(qMin(index,int(list.size())-1));error->setText(tr("삭제 초안입니다. 연결된 화면·조건 참조는 검증에서 확인하세요."));});
  connect(instances,&QListWidget::currentRowChanged,this,[=](int index){while(form->rowCount())form->removeRow(0);const auto list=library().value("progressions").toArray();if(index<0||index>=list.size())return;const auto item=list[index].toObject();auto update=[=](QString key,QJsonValue value){auto lib=library();auto rows=lib.value("progressions").toArray();auto entry=rows[index].toObject();entry[key]=value;rows[index]=entry;lib["progressions"]=rows;project["ruleExtensions"]=lib;};
    auto name=new QLineEdit(item.value("name").toString());name->setObjectName("ruleProgressionName");form->addRow(tr("이름"),name);connect(name,&QLineEdit::textChanged,this,[=](const QString &text){update("name",text);instances->item(index)->setText(text);});
    auto scope=new QComboBox;scope->setObjectName("ruleProgressionScope");scope->addItem(tr("게임 전체 공유"),"shared");scope->addItem(tr("캐릭터마다 독립 진행"),"character");scope->setCurrentIndex(scope->findData(item.value("scope").toString()));form->addRow(tr("범위"),scope);connect(scope,qOverload<int>(&QComboBox::currentIndexChanged),this,[=]{update("scope",scope->currentData().toString());});
    auto scopeHelp=new QLabel(tr("범위는 진행 상태의 소유자를 정합니다. 캐릭터별 능력·소지품·관계는 규칙 본문의 대상을 ‘행동 실행자’로 연결하세요. 전이 비용 양식은 공유 자원입니다."));scopeHelp->setWordWrap(true);form->addRow(scopeHelp);
    QJsonObject definition;for(const auto &d:ruleDefinitions(project,"progressions"))if(d.toObject().value("packageId")==item.value("packageId")&&d.toObject().value("id")==item.value("ruleId")&&d.toObject().value("version")==item.value("version"))definition=d.toObject();
    argumentsForm(form,definition,item.value("args").toObject(),context,[=](QString key,QJsonValue value){const auto rows=library().value("progressions").toArray();auto args=rows[index].toObject().value("args").toObject();args[key]=value;update("args",args);});
    auto stateName=[&](QString id){for(const auto &s:definition.value("states").toArray())if(s.toObject().value("id")==id)return s.toObject().value("name").toString(id);return id;};QStringList transitions;for(const auto &t:definition.value("transitions").toArray()){const auto v=t.toObject();QStringList from;for(const auto &s:v.value("from").toArray())from<<stateName(s.toString());transitions<<v.value("name").toString()+": "+from.join(", ")+" → "+stateName(v.value("to").toString());}auto preview=new QLabel(transitions.join('\n'));preview->setWordWrap(true);form->addRow(tr("상태 전이"),preview);
  });
  connect(place,&QPushButton::clicked,this,[=]{
    const auto list=library().value("progressions").toArray();const int index=instances->currentRow();
    if(index<0||index>=list.size())return;
    const auto instance=list[index].toObject();QJsonArray ids;
    for(const auto &v:ruleDefinitions(project,"progressions")) {
      const auto d=v.toObject();
      if(d.value("packageId")!=instance.value("packageId")||d.value("id")!=instance.value("ruleId")||d.value("version")!=instance.value("version"))continue;
      for(const auto &t:d.value("transitions").toArray())ids.append("rx-"+instance.value("id").toString()+"-"+t.toObject().value("id").toString());
    }
    if(ids.isEmpty()){error->setText(tr("연결된 버전의 전이가 없습니다. 먼저 진행 정의를 다시 연결하세요."));return;}
    auto visual=project.value("visual").toObject();auto screens=visual.value("screens").toArray();
    for(int i=0;i<screens.size();i++) {
      auto screen=screens[i].toObject();if(screen.value("id")!=screenId)continue;
      auto elements=screen.value("elements").toArray();const auto id="rule-actions-"+instance.value("id").toString();
      for(const auto &e:elements)if(e.toObject().value("id")==id){error->setText(tr("이 진행의 목록은 이미 배치되어 있습니다."));return;}
      const int canvasWidth=visual.value("width").toInt(1280),canvasHeight=visual.value("height").toInt(720);
      const int width=qMin(700,qMax(160,(canvasWidth-120)/2)),height=qMin(340,canvasHeight-190);
      QRect slot;bool found=false;
      for(int y=150;y+height<=canvasHeight-20&&!found;y+=height+20)for(int x=40;x+width<=canvasWidth-20;x+=width+40) {
        const QRect candidate(x,y,width,height);bool overlap=false;
        for(const auto &v:elements){const auto e=v.toObject();if(e.value("id").toString().startsWith("rule-actions-")&&candidate.intersects(QRect(e.value("x").toInt(),e.value("y").toInt(),e.value("width").toInt(),e.value("height").toInt())))overlap=true;}
        if(!overlap){slot=candidate;found=true;break;}
      }
      if(!found||height<16){error->setText(tr("추가 목록을 배치할 공간이 없습니다. 기존 목록의 크기·위치를 조정하거나 다른 화면을 선택하세요."));return;}
      elements.append(QJsonObject{{"id",id},{"kind","actions"},{"x",slot.x()},{"y",slot.y()},{"width",slot.width()},{"height",slot.height()},{"color","#ffffff"},{"fill","#192333ee"},{"actionIds",ids}});
      screen["elements"]=elements;screens[i]=screen;visual["screens"]=screens;project["visual"]=visual;
      error->setText(project.value("roster").toObject().value("stateModel")=="character-v1"
        ?tr("목록을 배치했습니다. 저장 후 디자인 → 캐릭터 소유 상태 · 관계 · 행동 연결에서 실행할 인물에게 행동을 연결하세요. 이 목록은 연결된 인물의 행동만 표시합니다.")
        :tr("목록을 배치했습니다. 저장 후 캔버스에서 위치·크기를 조정하세요."));return;
    }
    error->setText(tr("현재 화면을 찾을 수 없습니다."));
  });
  connect(buttons,&QDialogButtonBox::accepted,this,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);refill();refillInstances(0);
}

void StudioWindow::editRuleExtensions() {
  if(bridge.busy()||projectData().isEmpty())return;
  RuleExtensionDialog dialog(projectData(),view,screenId,this);
  if(dialog.exec()==QDialog::Accepted)replaceProject(dialog.value());
}
