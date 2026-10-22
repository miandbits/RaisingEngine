#include <QObject>
#include "logic-editor.hpp"
#include "rule-extensions.hpp"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QSplitter>
#include <QTimer>
#include <QSignalBlocker>
#include <limits>
#include <cmath>

namespace {
QJsonValue at(QJsonValue value,const QStringList &path,int depth=0){
  if(depth==path.size())return value;
  return at(value.isArray()?value.toArray().at(path[depth].toInt()):value.toObject().value(path[depth]),path,depth+1);
}

QJsonValue replaced(QJsonValue value,const QStringList &path,QJsonValue next,int depth=0){
  if(depth==path.size())return next;
  if(value.isArray()){auto a=value.toArray();int i=path[depth].toInt();if(i>=0&&i<a.size())a[i]=replaced(a[i],path,next,depth+1);return a;}
  auto o=value.toObject();o[path[depth]]=replaced(o.value(path[depth]),path,next,depth+1);return o;
}
QString title(QString op){if(op=="ruleCondition")return QObject::tr("확장 조건");if(op=="ruleEffect")return QObject::tr("확장 효과");static const QMap<QString,QString> names{{"stateIs",QObject::tr("상태 확인")},{"setState",QObject::tr("상태 변경")},{"always",QObject::tr("항상")},{"all",QObject::tr("모두 충족 AND")},{"any",QObject::tr("하나 이상 OR")},{"not",QObject::tr("반전 NOT")},{"compare",QObject::tr("수치 비교")},{"flag",QObject::tr("플래그 확인")},{"eventCompleted",QObject::tr("사건 완료")},{"literal",QObject::tr("고정 숫자")},{"read",QObject::tr("값 읽기")},{"time",QObject::tr("시간 값")},{"add",QObject::tr("더하기 / 증감")},{"subtract",QObject::tr("빼기")},{"multiply",QObject::tr("곱하기")},{"clamp",QObject::tr("범위 제한")},{"set",QObject::tr("값 지정")},{"setFlag",QObject::tr("플래그 설정")},{"attribute",QObject::tr("능력 · 자원")},{"relationship",QObject::tr("관계")},{"inventory",QObject::tr("소지품 수량")}};return names.value(op,op);}
}

QJsonValue logicValueAt(QJsonValue value,const QStringList &path){return at(value,path);}
QJsonValue logicReplaceAt(QJsonValue value,const QStringList &path,QJsonValue next){return replaced(value,path,next);}

LogicEditor::LogicEditor(QJsonValue value,QString kind,QJsonObject ctx,QWidget *parent):QDialog(parent),root(value),rootKind(kind),context(ctx){
  setObjectName("logicEditor");setWindowTitle(tr("조건 · 효과 · 수식 편집"));resize(1020,700);
  if(root.isUndefined()||root.isNull())root=kind=="effects"?QJsonValue(QJsonArray{}):QJsonValue(defaults(kind,kind=="condition"?"always":"literal"));
  auto outer=new QVBoxLayout(this);auto help=new QLabel(tr("왼쪽에서 규칙을 선택하고 오른쪽에서 대상과 값을 설정하세요. 비교식의 양쪽, 계산식과 AND/OR 조건을 중첩할 수 있습니다. 저장 전까지 원본은 바뀌지 않습니다."));help->setWordWrap(true);outer->addWidget(help);
  auto split=new QSplitter;outer->addWidget(split,1);tree=new QTreeWidget;tree->setObjectName("logicTree");tree->setHeaderLabels({tr("규칙 구조"),tr("현재 값")});split->addWidget(tree);properties=new QWidget;form=new QFormLayout(properties);split->addWidget(properties);split->setStretchFactor(0,3);split->setStretchFactor(1,2);
  auto row=new QHBoxLayout;for(auto spec:QList<QPair<QString,QString>>{{"logicAdd",tr("하위 항목 추가")},{"logicRemove",tr("선택 항목 삭제")},{"logicUp",tr("위로")},{"logicDown",tr("아래로")}}){auto b=new QPushButton(spec.second);b->setObjectName(spec.first);row->addWidget(b);if(spec.first=="logicAdd")connect(b,&QPushButton::clicked,this,[this]{addChild();});else if(spec.first=="logicRemove")connect(b,&QPushButton::clicked,this,[this]{removeChild();});else connect(b,&QPushButton::clicked,this,[this,spec]{moveChild(spec.first=="logicUp"?-1:1);});}outer->addLayout(row);
  error=new QLabel;error->setObjectName("logicError");error->setWordWrap(true);outer->addWidget(error);auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);outer->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,this,[this]{const auto problem=validationError(root,rootKind,"/");if(!problem.isEmpty()){error->setText(problem);return;}accept();});connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
  connect(tree,&QTreeWidget::currentItemChanged,this,[this]{editSelected();});refresh();
  if (rootKind == "condition" || rootKind == "effects") outer->insertWidget(1, quickEditor());
}
QJsonValue LogicEditor::node(const QStringList &path)const{return at(root,path);}
void LogicEditor::update(const QStringList &path,QJsonValue value){root=replaced(root,path,value);refresh(path);}
QJsonArray LogicEditor::references(QString kind,QString character)const{
  QJsonArray out;
  for(auto d:context.value("documents").toArray()){const auto doc=d.toObject();const auto data=doc.value("data").toObject();const auto type=doc.value("kind").toString();
    if(kind=="attribute"&&type=="attributes")out=data.value("attributes").toArray();
    if(kind=="flag"&&type=="flags")out=data.value("flags").toArray();
    if((kind=="character"&&type=="character")||(kind=="event"&&type=="event"))out.append(data);
    if(kind=="relationship"&&type=="character"&&data.value("id")==character)out=data.value("relationships").toArray();
    if(kind=="stateGroup"&&type=="project")out=data.value("stateGroups").toArray();
    if(kind=="stateValue"&&type=="project")for(const auto &g:data.value("stateGroups").toArray())if(g.toObject().value("id")==character)out=g.toObject().value("values").toArray();
    if(kind=="inventory"&&type=="project")out=data.value("gameplay").toObject().value("items").toArray();
  }
  for(const auto &g:ruleProgressionGroups(ruleProject(context))){if(kind=="stateGroup")out.append(g);if(kind=="stateValue"&&g.toObject().value("id")==character)out=g.toObject().value("values").toArray();}
  return out;
}
QJsonObject LogicEditor::defaults(QString kind,QString op)const{
  if(op=="ruleCondition"||op=="ruleEffect") {const auto defs=ruleDefinitions(ruleProject(context),op=="ruleCondition"?"conditions":"effects");return defs.empty()?QJsonObject{{"op",op}}:defaultRuleCall(defs.first().toObject(),op);}
  auto first=[&](QString k,QString c=QString()){const auto values=references(k,c);return values.isEmpty()?QString():values.first().toObject().value("id").toString();};
  QJsonObject zero{{"op","literal"},{"value",0}},always{{"op","always"}},target{{"kind","attribute"},{"attributeId",first("attribute")}};
  if(op=="stateIs"||op=="setState"){
    const auto gs=references("stateGroup");const auto g=gs.isEmpty()?QJsonObject():gs.first().toObject();
    QJsonObject rule{{"op",op},{"stateGroupId",g.value("id").toString()},{"stateValueId",g.value("initialValueId").toString()}};
    if(g.value("scope")=="character")rule["owner"]=QJsonObject{{"kind","actor"}};return rule;
  }
  if(kind=="target"){if(op=="inventory")return {{"kind",op},{"itemId",first(op)}};if(op=="relationship"){QString c;for(const auto &v:references("character")){const auto id=v.toObject().value("id").toString();if(!references(op,id).isEmpty()){c=id;break;}}return {{"kind",op},{"characterId",c},{"relationshipId",first(op,c)}};}return target;}
  if(kind=="condition"){
    if(op=="all"||op=="any")return {{"op",op},{"conditions",QJsonArray{always}}};
    if(op=="not")return {{"op",op},{"condition",always}};
    if(op=="compare")return {{"op",op},{"comparator","gte"},{"left",QJsonObject{{"op","read"},{"target",target}}},{"right",zero}};
    if(op=="flag")return {{"op",op},{"flagId",first("flag")},{"equals",true}};
    if(op=="eventCompleted")return {{"op",op},{"eventId",first("event")}};
    return always;
  }
  if(kind=="effect"){if(op=="setFlag")return {{"op",op},{"flagId",first("flag")},{"value",true}};return {{"op",op},{"target",target},{"value",zero}};}
  if(op=="read")return {{"op",op},{"target",target}};
  if(op=="time")return {{"op",op},{"field","periodIndex"}};
  if(op=="clamp")return {{"op",op},{"value",zero},{"min",0},{"max",100}};
  if(op=="add"||op=="subtract"||op=="multiply")return {{"op",op},{"left",zero},{"right",zero}};
  return zero;
}
void LogicEditor::addTree(QTreeWidgetItem *parent,QString label,QJsonValue value,QString kind,QStringList path){
  auto item=parent?new QTreeWidgetItem(parent):new QTreeWidgetItem(tree);item->setData(0,Qt::UserRole,path);item->setData(0,Qt::UserRole+1,kind);
  const auto o=value.toObject();QString op=o.value(kind=="target"?"kind":"op").toString();item->setText(0,label+" · "+(value.isArray()?tr("목록 / 순서"):title(op)));
  QString detail;for(const auto &key:{"attributeId","characterId","relationshipId","itemId","flagId","eventId","field","stateGroupId","stateValueId"})if(o.contains(key)){const auto id=o.value(key).toString();QString name=id;QString ref=QString(key);if(ref.endsWith("Id"))ref.chop(2);const auto scope=ref=="relationship"?o.value("characterId").toString():ref=="stateValue"?o.value("stateGroupId").toString():QString();for(const auto &v:references(ref,scope)){const auto r=v.toObject();if(r.value("id")==id){name=r.value("label").toString(r.value("name").toString(id));break;}}detail+=name+" ";}if(o.value("value").isDouble())detail+=QString::number(o.value("value").toInt());if(op=="stateIs"||op=="setState"){detail.clear();for(const auto &g:references("stateGroup"))if(g.toObject().value("id")==o.value("stateGroupId"))detail=g.toObject().value("name").toString();for(const auto &v:references("stateValue",o.value("stateGroupId").toString()))if(v.toObject().value("id")==o.value("stateValueId"))detail+=" · "+v.toObject().value("name").toString();}if(op=="ruleCondition"||op=="ruleEffect")for(const auto &v:ruleDefinitions(ruleProject(context),op=="ruleCondition"?"conditions":"effects")){const auto d=v.toObject();if(d.value("packageId")==o.value("packageId")&&d.value("id")==o.value("ruleId"))detail=d.value("packageName").toString()+" · "+d.value("name").toString();}item->setText(1,detail);
  if(path==selectedPath)tree->setCurrentItem(item);
  auto child=[&](QString key,QString label,QString k){if(o.contains(key))addTree(item,label,o.value(key),k,path+QStringList{key});};
  if(value.isArray()){int i=0;for(auto v:value.toArray()){addTree(item,QString::number(i+1),v,kind=="effects"?"effect":"condition",path+QStringList{QString::number(i)});i++;}}
  else if(kind=="condition"){if(op=="all"||op=="any")child("conditions",tr("조건들"),"conditions");if(op=="not")child("condition",tr("반전할 조건"),"condition");if(op=="compare"){child("left",tr("왼쪽 값"),"number");child("right",tr("오른쪽 값"),"number");}}
  else if(kind=="effect"||kind=="number"){child("target",tr("대상"),"target");if(o.value("value").isObject())child("value",tr("결과 수식"),"number");child("left",tr("왼쪽"),"number");child("right",tr("오른쪽"),"number");}
  item->setExpanded(true);
}
void LogicEditor::refresh(const QStringList &select){selectedPath=select;QSignalBlocker block(tree);tree->clear();addTree(nullptr,tr("규칙"),root,rootKind,{});if(!tree->currentItem())tree->setCurrentItem(tree->topLevelItem(0));tree->resizeColumnToContents(0);editSelected();}
void LogicEditor::property(QString key,QJsonValue value){propertyAt(selectedPath,key,value);}
void LogicEditor::propertyAt(const QStringList &path,QString key,QJsonValue value){
  auto o=node(path).toObject();o[key]=value;
  if(key=="stateGroupId")for(const auto &v:references("stateGroup")){const auto g=v.toObject();if(g.value("id")==value){o["stateValueId"]=g.value("initialValueId");if(g.value("scope")=="character")o["owner"]=QJsonObject{{"kind","actor"}};else o.remove("owner");}}

  if(key=="characterId"&&o.value("kind")=="relationship"){
    const auto choices=references("relationship",value.toString());bool found=false;for(const auto &choice:choices)if(choice.toObject().value("id")==o.value("relationshipId"))found=true;
    if(!found)o["relationshipId"]=choices.isEmpty()?QString():choices.first().toObject().value("id").toString();
  }
  root=replaced(root,path,o);error->clear();QTimer::singleShot(0,this,[this,path]{if(selectedPath==path)refresh(path);});
}
void LogicEditor::changeType(QString op){
  const auto path=selectedPath;const auto current=node(path).toObject();const auto key=selectedKind=="target"?QStringLiteral("kind"):QStringLiteral("op");const auto previous=current.value(key).toString();if(previous==op)return;
  auto next=defaults(selectedKind,op);
  const auto family=[](QString value,QStringList values){return values.contains(value);};
  if((selectedKind=="condition"&&family(previous,{"all","any"})&&family(op,{"all","any"}))||(selectedKind=="effect"&&family(previous,{"add","set"})&&family(op,{"add","set"}))||(selectedKind=="number"&&family(previous,{"add","subtract","multiply"})&&family(op,{"add","subtract","multiply"}))){next=current;next[key]=op;}
  root=replaced(root,path,next);error->clear();QTimer::singleShot(0,this,[this,path]{if(selectedPath==path)refresh(path);});
}
void LogicEditor::field(QString key,QString label,QStringList values,QStringList labels){auto f=new QComboBox;f->setObjectName("logic-"+key);for(int i=0;i<values.size();i++)f->addItem(i<labels.size()?labels[i]:title(values[i]),values[i]);const auto path=selectedPath;auto current=node(path).toObject().value(key).toString();if(!values.contains(current)&&!current.isEmpty())f->addItem(tr("없는 원본: ")+current,current);f->setCurrentIndex(f->findData(current));form->addRow(label,f);connect(f,qOverload<int>(&QComboBox::activated),this,[this,f,key,path]{propertyAt(path,key,f->currentData().toString());});}
void LogicEditor::reference(QString key,QString label,QString kind,QString character){QStringList ids,labels;for(auto v:references(kind,character)){auto o=v.toObject();if(key=="characterId"&&node(selectedPath).toObject().value("kind")=="relationship"&&references("relationship",o.value("id").toString()).isEmpty())continue;ids<<o.value("id").toString();labels<<o.value("label").toString(o.value("name").toString(o.value("id").toString()));}field(key,label,ids,labels);}
void LogicEditor::integer(QString key,QString label){auto f=new QSpinBox;f->setObjectName("logic-"+key);f->setRange(std::numeric_limits<int>::min(),std::numeric_limits<int>::max());const auto path=selectedPath;f->setValue(node(path).toObject().value(key).toInt());form->addRow(label,f);connect(f,&QSpinBox::editingFinished,this,[this,f,key,path]{propertyAt(path,key,f->value());});}
void LogicEditor::boolean(QString key,QString label){auto f=new QCheckBox;f->setObjectName("logic-"+key);const auto path=selectedPath;f->setChecked(node(path).toObject().value(key).toBool());form->addRow(label,f);connect(f,&QCheckBox::clicked,this,[this,key,path](bool on){propertyAt(path,key,on);});}
void LogicEditor::editSelected(){
  for(auto child:properties->findChildren<QWidget*>())child->disconnect(this);while(form->rowCount())form->removeRow(0);auto item=tree->currentItem();if(!item)return;selectedPath=item->data(0,Qt::UserRole).toStringList();selectedKind=item->data(0,Qt::UserRole+1).toString();const auto o=node(selectedPath).toObject();const auto op=o.value(selectedKind=="target"?"kind":"op").toString();
  if(node(selectedPath).isArray()){form->addRow(new QLabel(tr("하위 항목을 추가하거나 순서를 바꿀 수 있습니다.")));return;}
  QStringList ops=selectedKind=="condition"?QStringList{"always","all","any","not","compare","flag","eventCompleted","stateIs","ruleCondition"}:selectedKind=="effect"?QStringList{"add","set","setFlag","setState","ruleEffect"}:selectedKind=="target"?QStringList{"attribute","relationship","inventory"}:QStringList{"literal","read","time","add","subtract","multiply","clamp"};
  auto type=new QComboBox;type->setObjectName("logicType");for(auto value:ops)type->addItem(title(value),value);if(!ops.contains(op))type->addItem(tr("지원하지 않는 원본 · 유지"),op);type->setCurrentIndex(type->findData(op));form->addRow(tr("종류"),type);connect(type,qOverload<int>(&QComboBox::activated),this,[this,type]{changeType(type->currentData().toString());});
  if(op=="ruleCondition"||op=="ruleEffect") {const auto path=selectedPath;form->addRow(ruleCallForm(o,op=="ruleCondition"?"conditions":"effects",context,[this,path](QJsonObject value){const bool definitionChanged=node(path).toObject().value("ruleId")!=value.value("ruleId")||node(path).toObject().value("packageId")!=value.value("packageId")||node(path).toObject().value("version")!=value.value("version");root=replaced(root,path,value);if(definitionChanged)QTimer::singleShot(0,this,[this,path]{refresh(path);});},properties));return;}
  if(selectedKind=="target"){ownerField();if(op=="attribute")reference("attributeId",tr("능력 / 자원"),"attribute");if(op=="inventory")reference("itemId",tr("소지품"),"inventory");if(op=="relationship"){reference("characterId",tr("관계 상대"),"character");reference("relationshipId",tr("관계"),"relationship",o.value("characterId").toString());}return;}
  if(op=="stateIs"||op=="setState"){
    reference("stateGroupId",tr("상태"),"stateGroup");reference("stateValueId",tr("값"),"stateValue",o.value("stateGroupId").toString());
    for(const auto &g:references("stateGroup"))if(g.toObject().value("id")==o.value("stateGroupId")&&g.toObject().value("scope")=="character")ownerField();
  }
  if(op=="literal")integer("value",tr("숫자"));if(op=="clamp"){integer("min",tr("최솟값"));integer("max",tr("최댓값"));}
  if(op=="time")field("field",tr("시간"),{"periodIndex","year","month","periodInMonth"},{tr("전체 일정 칸"),tr("연도"),tr("월"),tr("월 내 일정 칸")});
  if(op=="compare")field("comparator",tr("비교"),{"eq","ne","gt","gte","lt","lte"},{"=","≠",">","≥","<","≤"});
  if(op=="flag"||op=="setFlag"){ownerField();reference("flagId",tr("플래그"),"flag");boolean(op=="flag"?"equals":"value",tr("켜짐"));}
  if(op=="eventCompleted"){ownerField();reference("eventId",tr("사건"),"event");}
}
void LogicEditor::ownerField(){
  auto field=new QComboBox;field->setObjectName("logicOwner");field->addItem(tr("공유 / 기존 대상"),QJsonObject{});field->addItem(tr("행동 실행자"),QJsonObject{{"kind","actor"}});
  QJsonObject project;for(const auto &v:context.value("documents").toArray())if(v.toObject().value("kind")=="project")project=v.toObject().value("data").toObject();
  for(const auto &v:project.value("roster").toObject().value("members").toArray()){
    const auto id=v.toObject().value("characterId").toString();QString label=id;
    for(const auto &c:references("character"))if(c.toObject().value("id")==id)label=c.toObject().value("name").toString(c.toObject().value("label").toString(id));
    field->addItem(tr("지정 캐릭터 · ")+label,QJsonObject{{"kind","character"},{"characterId",id}});
  }
  const auto path=selectedPath;const auto owner=node(path).toObject().value("owner").toObject();
  for(int i=0;i<field->count();i++)if(field->itemData(i).toJsonObject()==owner)field->setCurrentIndex(i);
  field->setToolTip(tr("실행자 대상은 캐릭터 행동·활동·대상별 사건에서 사용할 수 있습니다. 공유 항목은 기존 의미를 유지합니다."));form->addRow(tr("소유자"),field);
  connect(field,qOverload<int>(&QComboBox::activated),this,[this,field,path]{auto object=node(path).toObject();const auto owner=field->currentData().toJsonObject();if(owner.empty())object.remove("owner");else object["owner"]=owner;root=replaced(root,path,object);error->clear();
    // Keep the combo alive until Qt finishes closing its popup and delivering input.
    QTimer::singleShot(0,this,[this,path]{if(selectedPath==path)refresh(path);});
  });
}
void LogicEditor::addChild(){auto path=selectedPath;auto value=node(path);if(value.isObject()&&(value.toObject().value("op")=="all"||value.toObject().value("op")=="any")){path<<"conditions";value=node(path);}if(!value.isArray()){error->setText(tr("효과 목록이나 AND/OR 조건 목록을 선택하세요."));return;}auto a=value.toArray();a.append(defaults(rootKind=="effects"&&path.empty()?"effect":"condition",rootKind=="effects"&&path.empty()?"add":"always"));update(path,a);error->clear();}
void LogicEditor::removeChild(){if(selectedPath.empty())return;auto parent=selectedPath;int i=parent.takeLast().toInt();auto value=node(parent);if(!value.isArray())return;auto a=value.toArray();if(a.size()==1&&!(rootKind=="effects"&&parent.empty())){error->setText(tr("AND/OR에는 조건이 하나 이상 필요합니다."));return;}a.removeAt(i);update(parent,a);}
void LogicEditor::moveChild(int offset){if(selectedPath.empty())return;auto parent=selectedPath;int i=parent.takeLast().toInt();auto value=node(parent);if(!value.isArray())return;auto a=value.toArray();int to=i+offset;if(to<0||to>=a.size())return;auto v=a.takeAt(i);a.insert(to,v);root=replaced(root,parent,a);refresh(parent+QStringList{QString::number(to)});}

QString LogicEditor::validationError(QJsonValue value,QString kind,QString path,int depth) const {
  const auto fail=[&](QString message){return path+": "+message;};
  const auto childPath=[&](QString key){return path+(path.endsWith('/')?QString():QStringLiteral("/"))+key;};
  if(depth>32)return fail(tr("규칙 중첩은 32단계를 넘을 수 없습니다."));
  if(kind=="effects"||kind=="conditions"){
    if(!value.isArray())return fail(tr("목록이 필요합니다."));const auto values=value.toArray();
    if(kind=="conditions"&&values.isEmpty())return fail(tr("AND/OR에는 조건이 하나 이상 필요합니다."));
    for(int i=0;i<values.size();++i){const auto issue=validationError(values[i],kind=="effects"?"effect":"condition",childPath(QString::number(i)),depth);if(!issue.isEmpty())return issue;}
    return {};
  }
  if(!value.isObject())return fail(tr("규칙을 설정하세요."));
  const auto object=value.toObject();const auto op=object.value(kind=="target"?"kind":"op").toString();
  if((kind=="condition"&&op=="ruleCondition")||(kind=="effect"&&op=="ruleEffect")) {
    for(const auto &v:ruleDefinitions(ruleProject(context),kind=="condition"?"conditions":"effects")){const auto d=v.toObject();if(d.value("packageId")==object.value("packageId")&&d.value("id")==object.value("ruleId")&&d.value("version")==object.value("version"))return {};}
    return fail(tr("규칙 패키지·정의·버전을 선택하세요. 적용 후 공통 검증에서 매개변수를 확인합니다."));
  }
  const auto ref=[&](QString field,QString refKind,QString character=QString()){
    const auto id=object.value(field).toString();bool found=false;for(const auto &entry:references(refKind,character))if(entry.toObject().value("id").toString()==id&&!id.isEmpty())found=true;
    return found?QString():childPath(field)+": "+tr("존재하는 대상을 선택하세요.");
  };
  const auto child=[&](QString field,QString childKind){return validationError(object.value(field),childKind,childPath(field),depth+1);};
  const auto integerField=[&](QString field){const auto number=object.value(field);const double n=number.toDouble();return number.isDouble()&&n>=std::numeric_limits<int>::min()&&n<=std::numeric_limits<int>::max()&&std::floor(n)==n?QString():childPath(field)+": "+tr("32비트 정수를 입력하세요.");};
  if((kind=="condition"&&op=="stateIs")||(kind=="effect"&&op=="setState")){
    const auto issue=ref("stateGroupId","stateGroup");if(!issue.isEmpty())return issue;
    for(const auto &g:references("stateGroup"))if(g.toObject().value("id")==object.value("stateGroupId")&&((g.toObject().value("scope")=="character")!=object.contains("owner")))return fail(tr("공용/캐릭터별 대상 설정을 확인하세요."));
    return ref("stateValueId","stateValue",object.value("stateGroupId").toString());
  }
  if(kind=="target"){
    if(op=="attribute")return ref("attributeId","attribute");
    if(op=="inventory")return ref("itemId","inventory");
    if(op=="relationship"){const auto issue=ref("characterId","character");return issue.isEmpty()?ref("relationshipId","relationship",object.value("characterId").toString()):issue;}
    return fail(tr("수치 대상을 선택하세요."));
  }
  if(kind=="condition"){
    if(op=="always")return {};
    if(op=="all"||op=="any")return child("conditions","conditions");
    if(op=="not")return child("condition","condition");
    if(op=="eventCompleted")return ref("eventId","event");
    if(op=="flag"){const auto issue=ref("flagId","flag");if(!issue.isEmpty())return issue;return object.value("equals").isBool()?QString():fail(tr("플래그 비교 값을 선택하세요."));}
    if(op=="compare"){
      if(!QStringList{"eq","ne","gt","gte","lt","lte"}.contains(object.value("comparator").toString()))return fail(tr("비교 연산자를 선택하세요."));
      const auto issue=child("left","number");return issue.isEmpty()?child("right","number"):issue;
    }
    return fail(tr("조건 종류를 선택하세요."));
  }
  if(kind=="effect"){
    if(op=="setFlag"){const auto issue=ref("flagId","flag");if(!issue.isEmpty())return issue;return object.value("value").isBool()?QString():fail(tr("플래그 값을 선택하세요."));}
    if(op=="add"||op=="set"){const auto issue=child("target","target");return issue.isEmpty()?child("value","number"):issue;}
    return fail(tr("효과 종류를 선택하세요."));
  }
  if(op=="literal")return integerField("value");
  if(op=="read")return child("target","target");
  if(op=="time")return QStringList{"periodIndex","year","month","periodInMonth"}.contains(object.value("field").toString())?QString():fail(tr("시간 값을 선택하세요."));
  if(op=="clamp"){
    for(const auto &field:{QStringLiteral("min"),QStringLiteral("max")}){const auto issue=integerField(field);if(!issue.isEmpty())return issue;}
    if(object.value("min").toInt()>object.value("max").toInt())return fail(tr("최솟값은 최댓값 이하여야 합니다."));
    return child("value","number");
  }
  if(op=="add"||op=="subtract"||op=="multiply"){const auto issue=child("left","number");return issue.isEmpty()?child("right","number"):issue;}
  return fail(tr("수식 종류를 선택하세요."));
}
