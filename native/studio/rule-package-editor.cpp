#include <QObject>
#include "rule-package-editor.hpp"
#include "rule-extensions.hpp"
#include "logic-editor.hpp"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QSaveFile>
#include <QJsonDocument>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpinBox>
#include <QSignalBlocker>
#include <QSet>
#include <QSplitter>
#include <QUuid>
#include <QVBoxLayout>
#include <QTreeWidgetItemIterator>
#include <limits>

namespace {
QString fresh(const QJsonArray &rows,const QString &prefix) {
  for(int n=1;;++n) {
    const auto id=prefix+QString::number(n);bool used=false;
    for(const auto &row:rows)if(row.toObject().value("id")==id)used=true;
    if(!used)return id;
  }
}
QString label(const QString &key) {
  static const QMap<QString,QString> names{{"body",QObject::tr("본문")},{"requirements",QObject::tr("실행 조건")},{"effects",QObject::tr("효과")},{"costs",QObject::tr("비용")},
    {"left",QObject::tr("왼쪽")},{"right",QObject::tr("오른쪽")},{"target",QObject::tr("대상")},{"value",QObject::tr("값")},{"amount",QObject::tr("수량")},{"attributeId",QObject::tr("능력·자원")},
    {"characterId",QObject::tr("인물")},{"itemId",QObject::tr("아이템")},{"eventId",QObject::tr("사건")},{"flagId",QObject::tr("플래그")},{"stateGroupId",QObject::tr("상태")},{"stateValueId",QObject::tr("상태 값")},
    {"transitions",QObject::tr("전이")},{"args",QObject::tr("입력 값")},{"conditions",QObject::tr("조건")},{"condition",QObject::tr("조건")},{"min",QObject::tr("최솟값")},{"max",QObject::tr("최댓값")}};
  return names.value(key,key);
}
QJsonValue materialize(QJsonValue value,const QJsonObject &params) {
  if(value.isArray()){QJsonArray out;for(const auto &v:value.toArray())out.append(materialize(v,params));return out;}
  if(!value.isObject())return value;
  auto o=value.toObject();if(o.contains("$param"))return params.value(o.value("$param").toString()).toObject().value("default");
  for(auto it=o.begin();it!=o.end();++it)it.value()=materialize(it.value(),params);
  return o;
}
// Preserve only bindings whose concrete value and parent shape were not changed by the logic editor.
QJsonValue preserveBindings(QJsonValue old,QJsonValue edited,const QJsonObject &params) {
  if(old.isObject()&&old.toObject().contains("$param"))return materialize(old,params)==edited?old:edited;
  if(old.isArray()&&edited.isArray()) {
    auto out=edited.toArray();const auto before=old.toArray();QSet<int> consumed,matched;
    // Match unchanged rows first so moving an effect carries its bindings with it.
    for(int i=0;i<out.size();++i)for(int j=0;j<before.size();++j)if(!consumed.contains(j)&&materialize(before[j],params)==out[i]){out[i]=before[j];consumed.insert(j);matched.insert(i);break;}
    for(int i=0;i<qMin(out.size(),before.size());++i)if(!matched.contains(i)&&!consumed.contains(i)) {
      const auto concrete=materialize(before[i],params).toObject();const auto changed=out[i].toObject();
      if(!concrete.isEmpty()&&concrete.value("op")==changed.value("op")&&concrete.value("target")==changed.value("target"))out[i]=preserveBindings(before[i],out[i],params);
    }
    return out;
  }
  if(old.isObject()&&edited.isObject()) {
    const auto before=old.toObject();auto out=edited.toObject();
    if(before.value("op")!=out.value("op")||before.value("kind")!=out.value("kind"))return edited;
    for(auto it=out.begin();it!=out.end();++it)if(before.contains(it.key()))it.value()=preserveBindings(before.value(it.key()),it.value(),params);
    return out;
  }
  return edited;
}
QString fieldType(QString key,QJsonValue value) {
  static const QMap<QString,QString> refs{{"attributeId","attribute"},{"characterId","character"},{"itemId","item"},{"eventId","event"},
    {"flagId","flag"},{"stateGroupId","stateGroup"},{"stateValueId","stateValue"}};
  if(refs.contains(key))return refs.value(key);
  if(QStringList{"value","amount","min","max","equals"}.contains(key))return value.isBool()?"boolean":value.isDouble()?"integer":QString();
  return {};
}
QJsonValue repinInternal(QJsonValue value,QString packageId,QString from,QString to) {
  if(value.isArray()){auto a=value.toArray();for(int i=0;i<a.size();++i)a[i]=repinInternal(a[i],packageId,from,to);return a;}
  if(!value.isObject())return value;
  auto o=value.toObject();if((o.value("op")=="ruleCondition"||o.value("op")=="ruleEffect")&&o.value("packageId")==packageId&&o.value("version")==from)o["version"]=to;
  for(auto it=o.begin();it!=o.end();++it)if(it.value().isArray()||it.value().isObject())it.value()=repinInternal(it.value(),packageId,from,to);
  return o;
}
void dependencies(QJsonValue value,QSet<QString> &result,QString own) {
  if(value.isArray()){for(const auto &v:value.toArray())dependencies(v,result,own);return;}
  if(!value.isObject())return;
  const auto o=value.toObject();
  if((o.value("op")=="ruleCondition"||o.value("op")=="ruleEffect")&&o.value("packageId").toString()!=own)result.insert(o.value("packageId").toString()+" @ "+o.value("version").toString());
  for(auto it=o.begin();it!=o.end();++it)dependencies(it.value(),result,own);
}
bool usesParameter(QJsonValue value,const QString &key) {
  if(value.isArray()){for(const auto &v:value.toArray())if(usesParameter(v,key))return true;}
  if(value.isObject()){const auto o=value.toObject();if(o.value("$param")==key)return true;for(auto it=o.begin();it!=o.end();++it)if(it.key()!="parameters"&&usesParameter(it.value(),key))return true;}
  return false;
}
void leaves(QTreeWidgetItem *parent,QJsonValue value,QStringList path,const QJsonObject &params,const QJsonObject &catalog,QJsonObject types={}) {
  if(value.isArray()) {int n=0;for(const auto &v:value.toArray()){auto item=new QTreeWidgetItem(parent,{QString::number(n+1)});leaves(item,v,path+QStringList{QString::number(n++)},params,catalog);}return;}
  if(!value.isObject())return;
  const auto object=value.toObject();
  for(auto it=object.begin();it!=object.end();++it) {
    if(QStringList{"parameters","id","name","initial","states","from","to","op","kind","packageId","ruleId","version"}.contains(it.key()))continue;
    const auto concrete=materialize(it.value(),params);const auto bound=it.value().toObject().value("$param").toString();
    const auto type=!bound.isEmpty()?params.value(bound).toObject().value("type").toString():types.contains(it.key())?types.value(it.key()).toObject().value("type").toString():fieldType(it.key(),concrete);
    auto item=new QTreeWidgetItem(parent,{types.contains(it.key())?types.value(it.key()).toObject().value("name").toString(label(it.key())):label(it.key())});
    if(!type.isEmpty()) {
      item->setData(0,Qt::UserRole,path+QStringList{it.key()});item->setData(0,Qt::UserRole+1,type);
      const auto key=it.value().toObject().value("$param").toString();
      item->setText(1,key.isEmpty()?(concrete.isString()?ruleReferenceName(catalog,type,concrete.toString()):concrete.toVariant().toString()):QObject::tr("입력: ")+params.value(key).toObject().value("name").toString(key));
    } else {
      QJsonObject argumentTypes;
      if(it.key()=="args")for(const auto &v:ruleDefinitions(ruleProject(catalog),object.value("op")=="ruleCondition"?"conditions":"effects")){const auto d=v.toObject();if(d.value("packageId")==object.value("packageId")&&d.value("id")==object.value("ruleId")&&d.value("version")==object.value("version"))argumentTypes=d.value("parameters").toObject();}
      leaves(item,it.value(),path+QStringList{it.key()},params,catalog,argumentTypes);
    }
  }
}
QPushButton *button(QFormLayout *form,QString text,QString id) {
  auto b=new QPushButton(text);b->setObjectName(id);form->addRow(b);return b;
}
}

RulePackageEditor::RulePackageEditor(QJsonObject initial,QJsonObject ctx,QWidget *parent):QDialog(parent),package(initial),context(ctx) {
  if(package.isEmpty())package={{"apiVersion",1},{"id","pack-"+QUuid::createUuid().toString(QUuid::Id128)},
    {"name",tr("새 규칙 패키지")},{"version","1.0.0"},{"conditions",QJsonArray{}},{"effects",QJsonArray{}},{"progressions",QJsonArray{}}};
  setObjectName("rulePackageEditor");setWindowTitle(tr("규칙 패키지 제작실"));resize(1050,760);
  auto box=new QVBoxLayout(this);auto help=new QLabel(tr("조건·효과는 기존 규칙 편집기로, 진행 방식은 상태와 전이로 만듭니다. ‘조절 값 연결’에서 값을 매개변수로 묶으면 여러 게임·인물이 재사용할 수 있습니다. 프로젝트에 적용하기 전까지 원본은 바뀌지 않습니다."));help->setWordWrap(true);box->addWidget(help);
  auto meta=new QFormLayout;box->addLayout(meta);
  for(const auto &key:QStringList{"name","version"}) {auto f=new QLineEdit(package.value(key).toString());f->setObjectName("package-"+key);f->setMaxLength(200);meta->addRow(key=="name"?tr("패키지 이름"):tr("버전"),f);connect(f,&QLineEdit::textChanged,this,[this,key](const QString &v){if(key=="version")package=repinInternal(package,package.value("id").toString(),package.value("version").toString(),v).toObject();package[key]=v;});}
  auto dependencyView=new QPushButton(tr("의존 패키지 · 버전 변경 안내"));dependencyView->setObjectName("packageDependencies");meta->addRow(dependencyView);
  connect(dependencyView,&QPushButton::clicked,this,[this]{QSet<QString> found;dependencies(package,found,package.value("id").toString());auto names=found.values();names.sort();status->setText(tr("외부 규칙 의존: ")+(names.isEmpty()?tr("없음"):names.join(", "))+tr("\n동일 패키지 내부 호출은 새 버전을 따릅니다. 프로젝트와 다른 패키지의 호출은 이전 버전에 고정됩니다. 적용 후 참조를 다시 선택하고 검증하세요. 기존 저장은 내용 지문이 달라지므로 자동으로 호환 처리하지 않습니다."));});
  auto row=new QHBoxLayout;box->addLayout(row);
  for(const auto &kind:QStringList{"conditions","effects","progressions"}) {auto b=new QPushButton(kind=="conditions"?tr("조건 추가"):kind=="effects"?tr("효과 추가"):tr("진행 방식 추가"));b->setObjectName("add-package-"+kind);row->addWidget(b);connect(b,&QPushButton::clicked,this,[this,kind]{add(kind);});}
  auto del=new QPushButton(tr("선택 삭제"));del->setObjectName("remove-package-item");row->addWidget(del);connect(del,&QPushButton::clicked,this,[this]{removeSelected();});
  auto split=new QSplitter;box->addWidget(split,1);tree=new QTreeWidget;tree->setObjectName("packageTree");tree->setHeaderLabel(tr("규칙 · 상태 · 전이"));split->addWidget(tree);
  auto scroll=new QScrollArea;scroll->setWidgetResizable(true);auto page=new QWidget;form=new QFormLayout(page);scroll->setWidget(page);split->addWidget(scroll);split->setStretchFactor(1,2);
  connect(tree,&QTreeWidget::currentItemChanged,this,[this]{auto item=tree->currentItem();selected=item?item->data(0,Qt::UserRole).toStringList():QStringList{};details();});
  status=new QLabel;status->setObjectName("packageStatus");status->setWordWrap(true);box->addWidget(status);
  auto exportButton=new QPushButton(tr("패키지 파일로 내보내기…"));exportButton->setObjectName("exportRulePackage");box->addWidget(exportButton);
  connect(exportButton,&QPushButton::clicked,this,[this]{const auto error=validate(package);if(!error.isEmpty()){status->setText(error);return;}auto path=QFileDialog::getSaveFileName(this,tr("규칙 패키지 내보내기"),package.value("id").toString()+".raiserules",tr("규칙 패키지 (*.raiserules)"),nullptr,QFileDialog::DontUseNativeDialog);if(path.isEmpty())return;if(!path.endsWith(".raiserules"))path+=".raiserules";QSaveFile file(path);const auto bytes=QJsonDocument(package).toJson();if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()){status->setText(tr("내보내기 실패: ")+file.errorString());return;}status->setText(tr("내보냈습니다. 연결 대상은 가져온 프로젝트의 공통 검증에서 확인합니다."));});
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);box->addWidget(buttons);
  connect(buttons,&QDialogButtonBox::accepted,this,[this]{const auto problem=validate(package);if(!problem.isEmpty()){status->setText(problem);return;}accept();});connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);refresh();
}
void RulePackageEditor::put(QStringList path,QJsonValue value){package=logicReplaceAt(package,path,value).toObject();}
QJsonObject RulePackageEditor::editorContext() const {
  auto ctx=context;auto docs=ctx.value("documents").toArray();
  for(int i=0;i<docs.size();++i){auto d=docs[i].toObject();if(d.value("kind")!="project")continue;auto p=d.value("data").toObject();auto lib=p.value("ruleExtensions").toObject();auto packs=lib.value("packages").toArray();bool found=false;for(int j=0;j<packs.size();++j)if(packs[j].toObject().value("id")==package.value("id")){packs[j]=package;found=true;}if(!found)packs.append(package);lib["packages"]=packs;lib["apiVersion"]=1;p["ruleExtensions"]=lib;d["data"]=p;docs[i]=d;}
  ctx["documents"]=docs;return ctx;
}
void RulePackageEditor::refresh(QStringList path) {
  QSignalBlocker block(tree);tree->clear();QTreeWidgetItem *select=nullptr;
  for(const auto &kind:QStringList{"conditions","effects","progressions"}) {
    auto group=new QTreeWidgetItem(tree,{kind=="conditions"?tr("조건"):kind=="effects"?tr("효과"):tr("진행 방식")});
    int i=0;for(const auto &v:package.value(kind).toArray()) {
      auto d=v.toObject();const QStringList p{kind,QString::number(i++)};auto item=new QTreeWidgetItem(group,{d.value("name").toString()});item->setData(0,Qt::UserRole,p);if(p==path)select=item;
      if(kind=="progressions")for(const auto &sub:QStringList{"states","transitions"}) {auto branch=new QTreeWidgetItem(item,{sub=="states"?tr("상태"):tr("전이 행동")});int n=0;for(const auto &entry:d.value(sub).toArray()){auto child=new QTreeWidgetItem(branch,{entry.toObject().value("name").toString()});auto cp=p+QStringList{sub,QString::number(n++)};child->setData(0,Qt::UserRole,cp);if(cp==path)select=child;}}
    }
  }
  tree->expandAll();tree->setCurrentItem(select);selected=select?path:QStringList{};details();
}
void RulePackageEditor::add(QString kind) {
  auto rows=package.value(kind).toArray();auto id=fresh(rows,kind=="conditions"?"condition-":kind=="effects"?"effect-":"progression-");
  QJsonObject d{{"id",id},{"name",kind=="conditions"?tr("새 조건"):kind=="effects"?tr("새 효과"):tr("새 진행 방식")},{"parameters",QJsonObject{}}};
  if(kind=="conditions")d["body"]=QJsonObject{{"op","always"}};
  else if(kind=="effects")d["body"]=QJsonArray{};
  else {d["initial"]="state-1";d["states"]=QJsonArray{QJsonObject{{"id","state-1"},{"name",tr("시작")}}};d["transitions"]=QJsonArray{};}
  rows.append(d);package[kind]=rows;refresh({kind,QString::number(rows.size()-1)});
}
void RulePackageEditor::removeSelected() {
  if(selected.size()!=2&&selected.size()!=4)return;
  const auto path=selected;auto parent=path;const auto index=parent.takeLast().toInt();auto rows=logicValueAt(package,parent).toArray();
  if(path.size()==4&&path[2]=="states") {
    const auto d=logicValueAt(package,path.mid(0,2)).toObject();const auto id=rows[index].toObject().value("id");
    bool used=d.value("initial")==id;for(const auto &v:d.value("transitions").toArray()){auto t=v.toObject();if(t.value("to")==id||t.value("from").toArray().contains(id))used=true;}
    if(used){status->setText(tr("시작 상태 또는 전이에서 사용하는 상태입니다. 연결을 바꾼 후 삭제하세요."));return;}
  }
  rows.removeAt(index);put(parent,rows);refresh(path.mid(0,path.size()==4?2:0));status->setText(tr("삭제 초안입니다. 적용 후 참조 검증을 실행하세요."));
}
void RulePackageEditor::details() {
  while(form->rowCount())form->removeRow(0);
  if(selected.size()<2)return;
  const auto path=selected;const auto d=logicValueAt(package,path).toObject();const auto defPath=path.mid(0,2);
  auto name=new QLineEdit(d.value("name").toString());name->setObjectName("package-item-name");name->setMaxLength(200);form->addRow(tr("이름"),name);
  connect(name,&QLineEdit::textChanged,this,[this,path](const QString &s){put(path+QStringList{"name"},s);if(tree->currentItem())tree->currentItem()->setText(0,s);});
  if(path.size()==2) {
    auto bind=button(form,tr("조절 값 연결…"),"package-bindings");connect(bind,&QPushButton::clicked,this,[this,defPath]{editParameters(defPath);});
    if(path[0]!="progressions") {const auto kind=path[0]=="conditions"?"condition":"effects";auto edit=button(form,tr("본문 편집…"),"package-body");connect(edit,&QPushButton::clicked,this,[this,path,kind,defPath]{editBody(path+QStringList{"body"},kind,defPath);});return;}
    auto initial=new QComboBox;initial->setObjectName("package-initial");for(const auto &v:d.value("states").toArray())initial->addItem(v.toObject().value("name").toString(),v.toObject().value("id").toString());initial->setCurrentIndex(initial->findData(d.value("initial").toString()));form->addRow(tr("시작 상태"),initial);connect(initial,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,path,initial]{put(path+QStringList{"initial"},initial->currentData().toString());});
    for(const auto &sub:QStringList{"states","transitions"}) {auto b=button(form,sub=="states"?tr("상태 추가"):tr("전이 행동 추가"),"package-add-"+sub);connect(b,&QPushButton::clicked,this,[this,path,sub]{const auto current=logicValueAt(package,path).toObject();auto list=current.value(sub).toArray();QJsonObject entry{{"id",fresh(list,sub=="states"?"state-":"transition-")},{"name",sub=="states"?tr("새 상태"):tr("새 전이")}};if(sub=="transitions"){entry["from"]=QJsonArray{current.value("initial")};entry["to"]=current.value("initial");entry["requirements"]=QJsonObject{{"op","always"}};entry["costs"]=QJsonArray{};entry["effects"]=QJsonArray{};}list.append(entry);put(path+QStringList{sub},list);refresh(path+QStringList{sub,QString::number(list.size()-1)});});}
  } else if(path[2]=="transitions") {
    const auto def=logicValueAt(package,defPath).toObject();auto from=new QListWidget;from->setObjectName("transition-from");from->setMaximumHeight(120);auto to=new QComboBox;to->setObjectName("transition-to");
    for(const auto &v:def.value("states").toArray()){const auto state=v.toObject();auto item=new QListWidgetItem(state.value("name").toString(),from);item->setData(Qt::UserRole,state.value("id").toString());item->setCheckState(d.value("from").toArray().contains(state.value("id"))?Qt::Checked:Qt::Unchecked);to->addItem(state.value("name").toString(),state.value("id").toString());}
    to->setCurrentIndex(to->findData(d.value("to").toString()));form->addRow(tr("출발 상태"),from);form->addRow(tr("도착 상태"),to);
    connect(from,&QListWidget::itemChanged,this,[this,path,from]{QJsonArray list;for(int i=0;i<from->count();++i)if(from->item(i)->checkState()==Qt::Checked)list.append(from->item(i)->data(Qt::UserRole).toString());put(path+QStringList{"from"},list);});
    connect(to,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,path,to]{put(path+QStringList{"to"},to->currentData().toString());});
    for(const auto &key:QStringList{"requirements","effects"}){auto edit=button(form,key=="requirements"?tr("실행 조건 편집…"):tr("효과 편집…"),"transition-"+key);connect(edit,&QPushButton::clicked,this,[this,path,key,defPath]{editBody(path+QStringList{key},key=="requirements"?"condition":"effects",defPath);});}
    auto costs=button(form,tr("비용 편집…"),"transition-costs");connect(costs,&QPushButton::clicked,this,[this,path,defPath]{
      const auto params=logicValueAt(package,defPath+QStringList{"parameters"}).toObject();const auto original=logicValueAt(package,path+QStringList{"costs"});const auto concrete=materialize(original,params).toArray();
      QDialog dialog(this);dialog.setObjectName("ruleCosts");dialog.setWindowTitle(tr("전이의 공유 비용"));auto layout=new QVBoxLayout(&dialog);auto note=new QLabel(tr("이 비용은 게임 전체의 공유 능력·자원에서 차감합니다. 캐릭터별 진행을 선택해도 비용의 대상은 바뀌지 않습니다. 개인 능력의 차감은 실행자 대상 조건과 효과로 설정하세요. 0은 비용 없음입니다."));note->setWordWrap(true);layout->addWidget(note);auto rows=new QFormLayout;layout->addLayout(rows);QList<QPair<QComboBox*,QSpinBox*>> inputs;
      QJsonArray attrs;for(const auto &v:context.value("documents").toArray())if(v.toObject().value("kind")=="attributes")attrs=v.toObject().value("data").toObject().value("attributes").toArray();
      QSet<QString> available,seen;for(const auto &v:attrs)available.insert(v.toObject().value("id").toString());
      for(const auto &v:concrete){const auto cost=v.toObject();const auto id=cost.value("attributeId").toString();const auto amount=cost.value("amount");if(!available.contains(id)||seen.contains(id)||!amount.isDouble()||amount.toDouble()<0||amount.toDouble()>std::numeric_limits<int>::max()||amount.toDouble()!=amount.toInt()||cost.size()!=2){status->setText(tr("기존 비용에 없는 대상·중복·확장 형식이 있습니다. 값을 보존했습니다. 공통 검증에서 참조를 먼저 수정하세요."));return;}seen.insert(id);}
      for(const auto &v:attrs){const auto attr=v.toObject();auto combo=new QComboBox;combo->addItem(attr.value("name").toString(attr.value("label").toString()),attr.value("id").toString());auto amount=new QSpinBox;amount->setObjectName("cost-"+attr.value("id").toString());amount->setRange(0,std::numeric_limits<int>::max());for(const auto &c:concrete)if(c.toObject().value("attributeId")==attr.value("id"))amount->setValue(c.toObject().value("amount").toInt());rows->addRow(combo,amount);inputs.append({combo,amount});}
      auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
      if(dialog.exec()==QDialog::Accepted){QJsonArray result;for(const auto &input:inputs)if(input.second->value()>0)result.append(QJsonObject{{"attributeId",input.first->currentData().toString()},{"amount",input.second->value()}});put(path+QStringList{"costs"},preserveBindings(original,result,params));}
    });
    auto event=new QComboBox;event->setObjectName("transition-event");event->addItem(tr("사건 없음"),QString());for(const auto &v:context.value("documents").toArray())if(v.toObject().value("kind")=="event"){auto e=v.toObject().value("data").toObject();event->addItem(e.value("name").toString(e.value("title").toString()),e.value("id").toString());}const auto eventId=d.value("eventId").toString();if(!eventId.isEmpty()&&event->findData(eventId)<0)event->addItem(tr("없는 사건: ")+eventId,eventId);event->setCurrentIndex(event->findData(eventId));form->addRow(tr("후속 사건"),event);connect(event,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,path,event]{auto transition=logicValueAt(package,path).toObject();if(event->currentData().toString().isEmpty())transition.remove("eventId");else transition["eventId"]=event->currentData().toString();put(path,transition);});
  }
}
void RulePackageEditor::editBody(QStringList path,QString kind,QStringList defPath) {
  const auto params=logicValueAt(package,defPath+QStringList{"parameters"}).toObject();const auto original=logicValueAt(package,path);
  for(auto it=params.begin();it!=params.end();++it)if(!it.value().toObject().contains("default")){status->setText(tr("본문 편집 전에 조절 값 연결에서 입력의 기본값을 지정하세요: ")+it.value().toObject().value("name").toString(it.key()));return;}
  LogicEditor dialog(materialize(original,params),kind,editorContext(),this);
  if(dialog.exec()==QDialog::Accepted){put(path,preserveBindings(original,dialog.value(),params));status->setText(tr("본문을 반영했습니다. 구조·값을 바꾼 부분의 조절 값 연결을 확인하세요."));}
}
void RulePackageEditor::editParameters(QStringList defPath) {
  auto draft=logicValueAt(package,defPath).toObject();QDialog dialog(this);dialog.setObjectName("ruleBindings");dialog.setWindowTitle(tr("조절 값 연결"));dialog.resize(760,650);auto box=new QVBoxLayout(&dialog);
  auto help=new QLabel(tr("본문의 수치·대상을 선택해 이름 있는 입력으로 만드세요. 같은 입력을 여러 위치에 연결할 수 있습니다. 연결 해제는 기본값으로 돌아갑니다."));help->setWordWrap(true);box->addWidget(help);
  auto values=new QTreeWidget;values->setObjectName("bindingValues");values->setHeaderLabels({tr("규칙의 값"),tr("고정값 / 연결된 입력")});box->addWidget(values,1);
  auto name=new QLineEdit;name->setObjectName("bindingName");name->setPlaceholderText(tr("예: 성장량, 검사할 능력"));box->addWidget(name);
  auto choices=new QComboBox;choices->setObjectName("bindingParameter");box->addWidget(choices);
  auto add=new QPushButton(tr("선택 값을 새 입력으로 만들기"));add->setObjectName("bindingCreate");box->addWidget(add);
  auto bind=new QPushButton(tr("선택한 입력에 연결"));bind->setObjectName("bindingConnect");box->addWidget(bind);
  auto detach=new QPushButton(tr("연결 해제 · 기본값 사용"));detach->setObjectName("bindingDetach");box->addWidget(detach);
  auto settings=new QPushButton(tr("선택 입력의 이름 · 기본값 편집…"));settings->setObjectName("bindingSettings");box->addWidget(settings);
  auto removeParameter=new QPushButton(tr("선택 입력 삭제 (사용 중이면 유지)"));removeParameter->setObjectName("bindingRemove");box->addWidget(removeParameter);
  auto message=new QLabel;message->setWordWrap(true);box->addWidget(message);
  auto fill=[&](QStringList restore=QStringList{}){values->clear();auto root=new QTreeWidgetItem(values,{draft.value("name").toString()});const auto params=draft.value("parameters").toObject();leaves(root,draft,{},params,editorContext());values->expandAll();values->resizeColumnToContents(0);choices->clear();for(auto it=params.begin();it!=params.end();++it)choices->addItem(it.value().toObject().value("name").toString(),it.key());if(!restore.isEmpty())for(QTreeWidgetItemIterator it(values);*it;++it)if((*it)->data(0,Qt::UserRole).toStringList()==restore){values->setCurrentItem(*it);break;}};
  connect(values,&QTreeWidget::currentItemChanged,&dialog,[&]{choices->clear();const auto item=values->currentItem();if(!item)return;const auto type=item->data(0,Qt::UserRole+1).toString();const auto params=draft.value("parameters").toObject();for(auto it=params.begin();it!=params.end();++it)if(it.value().toObject().value("type")==type)choices->addItem(it.value().toObject().value("name").toString(),it.key());const auto key=logicValueAt(draft,item->data(0,Qt::UserRole).toStringList()).toObject().value("$param").toString();if(!key.isEmpty())choices->setCurrentIndex(choices->findData(key));});
  connect(add,&QPushButton::clicked,&dialog,[&]{const auto item=values->currentItem();if(!item||item->data(0,Qt::UserRole+1).toString().isEmpty())return;if(name->text().trimmed().isEmpty()){message->setText(tr("입력 이름을 정하세요."));return;}auto params=draft.value("parameters").toObject();if(params.size()>=32){message->setText(tr("한 규칙의 입력은 32개 이하여야 합니다."));return;}const auto path=item->data(0,Qt::UserRole).toStringList();QString key;for(int n=1;;++n){key="param-"+QString::number(n);if(!params.contains(key))break;}const auto concrete=materialize(logicValueAt(draft,path),params);params[key]=QJsonObject{{"name",name->text().trimmed()},{"type",item->data(0,Qt::UserRole+1).toString()},{"default",concrete}};draft=logicReplaceAt(draft,path,QJsonObject{{"$param",key}}).toObject();draft["parameters"]=params;fill(path);});
  connect(bind,&QPushButton::clicked,&dialog,[&]{const auto item=values->currentItem();if(!item||choices->currentIndex()<0||item->data(0,Qt::UserRole+1).toString().isEmpty())return;const auto path=item->data(0,Qt::UserRole).toStringList();draft=logicReplaceAt(draft,path,QJsonObject{{"$param",choices->currentData().toString()}}).toObject();fill(path);});
  connect(detach,&QPushButton::clicked,&dialog,[&]{const auto item=values->currentItem();if(!item||item->data(0,Qt::UserRole+1).toString().isEmpty())return;const auto path=item->data(0,Qt::UserRole).toStringList();draft=logicReplaceAt(draft,path,materialize(logicValueAt(draft,path),draft.value("parameters").toObject())).toObject();fill(path);});
  connect(removeParameter,&QPushButton::clicked,&dialog,[&]{const auto key=choices->currentData().toString();auto params=draft.value("parameters").toObject();if(!params.contains(key))return;if(usesParameter(draft,key)){message->setText(tr("사용 중인 입력입니다. 모든 연결을 해제한 후 삭제하세요."));return;}params.remove(key);draft["parameters"]=params;fill();});
  connect(settings,&QPushButton::clicked,&dialog,[&]{
    const auto item=values->currentItem();QString key=choices->currentData().toString();
    if(item){const auto bound=logicValueAt(draft,item->data(0,Qt::UserRole).toStringList()).toObject().value("$param").toString();if(!bound.isEmpty())key=bound;}
    auto params=draft.value("parameters").toObject();if(!params.contains(key))return;auto parameter=params.value(key).toObject();
    QDialog edit(&dialog);edit.setObjectName("ruleParameterSettings");edit.setWindowTitle(tr("입력 이름 · 기본값 · 범위"));auto layout=new QFormLayout(&edit);
    auto caption=new QLineEdit(parameter.value("name").toString());caption->setObjectName("parameterName");caption->setMaxLength(200);layout->addRow(tr("입력 이름"),caption);
    QJsonValue defaultValue=parameter.value("default");const auto type=parameter.value("type").toString();
    if(defaultValue.isUndefined()){if(type=="integer")defaultValue=qBound(parameter.value("min").toInt(std::numeric_limits<int>::min()),0,parameter.value("max").toInt(std::numeric_limits<int>::max()));else if(type=="boolean")defaultValue=false;}
    ruleArgumentFields(layout,QJsonObject{{"parameters",QJsonObject{{key,parameter}}}},QJsonObject{{key,defaultValue}},editorContext(),[&](QString,QJsonValue v){defaultValue=v;});
    QSpinBox *minimum=nullptr,*maximum=nullptr;
    if(type=="integer") {minimum=new QSpinBox;maximum=new QSpinBox;for(auto spin:{minimum,maximum})spin->setRange(std::numeric_limits<int>::min(),std::numeric_limits<int>::max());minimum->setObjectName("parameterMin");maximum->setObjectName("parameterMax");minimum->setValue(parameter.value("min").toInt(std::numeric_limits<int>::min()));maximum->setValue(parameter.value("max").toInt(std::numeric_limits<int>::max()));layout->addRow(tr("최솟값"),minimum);layout->addRow(tr("최댓값"),maximum);}
    auto error=new QLabel;error->setWordWrap(true);layout->addRow(error);
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&edit,[&]{
      if(caption->text().trimmed().isEmpty()||defaultValue.isUndefined()||(defaultValue.isString()&&defaultValue.toString().isEmpty())){error->setText(tr("이름과 기본값을 지정하세요."));return;}
      if(minimum&&(minimum->value()>maximum->value()||defaultValue.toInt()<minimum->value()||defaultValue.toInt()>maximum->value())){error->setText(tr("최소 ≤ 기본값 ≤ 최대가 되도록 설정하세요."));return;}
      edit.accept();
    });
    connect(buttons,&QDialogButtonBox::rejected,&edit,&QDialog::reject);
    if(edit.exec()==QDialog::Accepted){parameter["name"]=caption->text().trimmed();parameter["default"]=defaultValue;if(minimum){parameter["min"]=minimum->value();parameter["max"]=maximum->value();}params[key]=parameter;draft["parameters"]=params;fill(item?item->data(0,Qt::UserRole).toStringList():QStringList{});}
  });
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);box->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);fill();if(dialog.exec()==QDialog::Accepted)put(defPath,draft);
}
QString RulePackageEditor::validate(const QJsonObject &pack) {
  if(pack.value("apiVersion")!=1||!QRegularExpression("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$").match(pack.value("id").toString()).hasMatch()||pack.value("id").toString().size()>48)return tr("지원하는 규칙 API 1 패키지와 식별자가 필요합니다.");
  if(pack.value("name").toString().trimmed().isEmpty()||pack.value("name").toString().size()>200)return tr("패키지 이름은 1~200자여야 합니다.");
  if(!QRegularExpression("^[0-9]+\\.[0-9]+\\.[0-9]+$").match(pack.value("version").toString()).hasMatch())return tr("버전은 1.0.0 형식이어야 합니다.");
  for(const auto &kind:QStringList{"conditions","effects","progressions"}) {
    if(pack.value(kind).toArray().size()>128)return tr("한 종류의 규칙은 128개 이하여야 합니다.");
    for(const auto &v:pack.value(kind).toArray()) {
      const auto d=v.toObject();if(d.value("name").toString().trimmed().isEmpty())return tr("규칙 이름이 비어 있습니다.");
      if(d.value("name").toString().size()>200)return tr("규칙 이름은 200자 이하여야 합니다.");
      if(d.value("parameters").toObject().size()>32)return tr("규칙 입력은 32개 이하여야 합니다.");
      if(kind!="progressions")continue;
      if(d.value("states").toArray().size()>128||d.value("transitions").toArray().size()>128)return tr("상태와 전이는 각각 128개 이하여야 합니다.");
      QSet<QString> states;for(const auto &s:d.value("states").toArray()){auto state=s.toObject();if(state.value("name").toString().trimmed().isEmpty())return tr("상태 이름이 비어 있습니다.");states.insert(state.value("id").toString());}
      if(!states.contains(d.value("initial").toString()))return tr("시작 상태가 없습니다.");
      if(d.value("transitions").toArray().isEmpty())return tr("진행 방식에 전이 행동이 필요합니다.");
      for(const auto &t:d.value("transitions").toArray()){const auto trans=t.toObject();if(trans.value("name").toString().trimmed().isEmpty())return tr("전이 이름이 비어 있습니다.");if(!states.contains(trans.value("to").toString())||trans.value("from").toArray().isEmpty())return tr("전이의 출발·도착 상태를 연결하세요.");for(const auto &s:trans.value("from").toArray())if(!states.contains(s.toString()))return tr("삭제된 상태를 가리키는 전이가 있습니다.");}
    }
  }
  return {};
}
