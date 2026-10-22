#include "story-scope.hpp"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <functional>
#include <memory>

namespace {
QString name(const QJsonObject &v){return v.value("label").toString(v.value("name").toString(v.value("id").toString()));}
QString escaped(QString s){return s.replace("~","~0").replace("/","~1");}
QString location(const QJsonObject &event,const QString &path){
  const auto parts=path.split('/');if(parts.value(1)=="requirements")return QObject::tr("발생 조건");
  if(parts.value(1)!="nodes")return {};
  const int index=parts.value(2).toInt();auto nodes=event.value("nodes").toArray();if(index<0||index>=nodes.size())return {};
  auto node=nodes[index].toObject();QString label=QObject::tr("대본 %1").arg(index+1);
  if(parts.value(3)=="choices"){auto choices=node.value("choices").toArray();int i=parts.value(4).toInt();if(i>=0&&i<choices.size())label+=" · "+choices[i].toObject().value("text").toString(QObject::tr("선택 %1").arg(i+1));}
  return label+" · "+(path.contains("/effects")?QObject::tr("효과"):QObject::tr("조건"));
}
QString rendered(const QJsonValue &v){if(v.isUndefined())return QObject::tr("공용 (기본값)");if(v.isString())return v.toString();if(v.isObject()&&v.toObject().value("kind")=="actor")return QObject::tr("행동 실행자");auto s=QString::fromUtf8(QJsonDocument(QJsonArray{v}).toJson(QJsonDocument::Compact));return s.mid(1,s.size()-2);}
using Visit=std::function<void(QJsonObject&,const QString&)>;
// Visit executable logic only. Scene assets, text, metadata and extension payloads
// must never be rewritten by coincidental property names.
void logic(QJsonValue &v,const QString &path,const Visit &visit){
  if(v.isArray()){auto a=v.toArray();for(int i=0;i<a.size();++i){QJsonValue x=a[i];logic(x,path+"/"+QString::number(i),visit);a[i]=x;}v=a;return;}
  if(!v.isObject())return;auto o=v.toObject();visit(o,path);
  const auto op=o.value("op").toString();
  const QSet<QString> ops{"all","any","not","compare","read","add","set","literal","flag","setFlag","eventCompleted","stateIs","setState","subtract","multiply","divide","min","max","clamp","negate"};
  if(ops.contains(op))for(auto it=o.begin();it!=o.end();++it)if(it.key()!="owner"){QJsonValue child=it.value();logic(child,path+"/"+escaped(it.key()),visit);it.value()=child;}
  v=o;
}
void eventLogic(QJsonObject &e,const Visit &visit){
  auto requirements=e.value("requirements");logic(requirements,"/requirements",visit);e["requirements"]=requirements;
  auto nodes=e.value("nodes").toArray();
  for(int i=0;i<nodes.size();++i){auto n=nodes[i].toObject();auto p="/nodes/"+QString::number(i);
    for(const auto &key:QStringList{"condition","effects"})if(n.contains(key)){auto v=n.value(key);logic(v,p+"/"+key,visit);n[key]=v;}
    if(n.value("type")=="choice"){auto choices=n.value("choices").toArray();for(int j=0;j<choices.size();++j){auto c=choices[j].toObject();for(const auto &key:QStringList{"requirements","effects"})if(c.contains(key)){auto v=c.value(key);logic(v,p+"/choices/"+QString::number(j)+"/"+key,visit);c[key]=v;}choices[j]=c;}n["choices"]=choices;}
    nodes[i]=n;
  }e["nodes"]=nodes;
}
bool shared(const QJsonObject &o){return !o.contains("owner");}
QString token(const QJsonObject &o){
  const auto kind=o.value("kind").toString(),op=o.value("op").toString();
  if(kind=="attribute")return "attribute:"+o.value("attributeId").toString();
  if(kind=="relationship")return "relationship:"+o.value("characterId").toString()+":"+o.value("relationshipId").toString();
  if(kind=="inventory")return "inventory:"+o.value("itemId").toString();
  if(op=="flag"||op=="setFlag")return "flag:"+o.value("flagId").toString();
  if(op=="stateIs"||op=="setState")return "state:"+o.value("stateGroupId").toString();
  if(op=="eventCompleted")return "event:"+o.value("eventId").toString();
  return {};
}
}

StoryScopePlan planPersonalStory(const QJsonArray &input,const QString &file,const QJsonObject &draft,const QMap<QString,bool> &owners){
  StoryScopePlan out;QMap<QString,QJsonObject> events;QMap<QString,int> indices;QMap<QString,QString> names;QJsonObject project;int rootAt=-1;
  for(int i=0;i<input.size();++i){auto d=input[i].toObject(),v=d.value("data").toObject();auto kind=d.value("kind").toString();
    if(kind=="project")project=v;
    if(kind=="event"){events[v.value("id").toString()]=v;indices[v.value("id").toString()]=i;if(d.value("file")==file)rootAt=i;}
    if(v.contains("id"))names[kind+":"+v.value("id").toString()]=name(v);
    for(const auto &pair:QList<QPair<QString,QString>>{{"attributes","attribute"},{"flags","flag"},{"relationships","relationship"}})for(auto a:v.value(pair.first).toArray())names[pair.second+":"+a.toObject().value("id").toString()]=name(a.toObject());
  }
  if(rootAt<0){out.errors<<QObject::tr("전환할 사건을 찾을 수 없습니다.");return out;}
  if(project.value("roster").toObject().value("stateModel")!="character-v1"){out.errors<<QObject::tr("캐릭터 소유 상태를 먼저 설정하세요.");return out;}
  const auto root=input[rootAt].toObject().value("data").toObject().value("id").toString();
  if(!draft.isEmpty()){if(draft.value("id")!=root){out.errors<<QObject::tr("사건 식별자가 변경되었습니다. 다시 여세요.");return out;}events[root]=draft;}
  QSet<QString> selected{root},writtenFlags;bool grew=true;
  while(grew){grew=false;
    for(const auto &id:selected){auto e=events[id];eventLogic(e,[&](QJsonObject &o,const QString&){if(o.value("op")=="setFlag"&&(shared(o)||o.value("owner").toObject().value("kind")=="actor"))writtenFlags.insert(o.value("flagId").toString());});}
    for(auto it=events.begin();it!=events.end();++it)if(!selected.contains(it.key())){auto e=it.value();bool linked=false;
      eventLogic(e,[&](QJsonObject &o,const QString&){if(!shared(o)&&o.value("owner").toObject().value("kind")!="actor")return;
        if(o.value("op")=="eventCompleted"&&selected.contains(o.value("eventId").toString()))linked=true;
        if(o.value("op")=="flag"&&writtenFlags.contains(o.value("flagId").toString()))linked=true;
      });if(linked){selected.insert(it.key());grew=true;}
    }
  }
  out.eventIds=selected.values();out.eventIds.sort();QSet<QString> local;for(auto v:project.value("roster").toObject().value("localAttributeIds").toArray())local.insert(v.toString());
  QMap<QString,bool> defaults;QMap<QString,QString> reasons;QSet<QString> fixed,externalWrites;
  // Outside events/actions may also produce these flags. Preserve their behavior;
  // make the changed meaning explicit instead of silently retargeting the world.
  std::function<void(const QJsonValue&)> outside=[&](const QJsonValue &v){if(v.isArray()){for(auto x:v.toArray())outside(x);return;}if(!v.isObject())return;const auto o=v.toObject();if(o.value("op")=="setFlag"&&shared(o)&&writtenFlags.contains(o.value("flagId").toString()))externalWrites.insert(o.value("flagId").toString());for(auto x:o)outside(x);};
  for(auto v:input){auto d=v.toObject();if(d.value("kind")=="event"&&selected.contains(d.value("data").toObject().value("id").toString()))continue;outside(d.value("data"));}
  for(const auto &id:out.eventIds){auto e=events[id];const auto trigger=e.value("trigger").toString("period-end");
    if(!QStringList{"after-activity","location-enter","period-start","after-event","interaction"}.contains(trigger))out.errors<<QObject::tr("%1: 캐릭터별 발생 시점이 필요합니다. 활동 완료·장소 입장·일정 시작·사건 후·행동 실행 중에서 선택하세요.").arg(name(e));
    eventLogic(e,[&](QJsonObject &o,const QString &path){const auto key=token(o);if(key.isEmpty()){
        if(o.value("op").toString().startsWith("extension"))out.warnings<<QObject::tr("%1 %2: 확장 규칙의 대상은 자동 전환하지 않습니다.").arg(name(e),path);return;}
      if(!shared(o)){if(o.value("owner").toObject().value("kind")!="actor")fixed.insert(name(e)+" · "+names.value(key,key));return;}
      bool convert=true;QString reason=QObject::tr("개인별 조건·효과");
      if(key.startsWith("attribute:")){convert=local.contains(o.value("attributeId").toString());if(!convert)reason=QObject::tr("공동 자원 유지 · 개인 능력으로 등록되지 않음");}
      if(key.startsWith("flag:")){convert=writtenFlags.contains(o.value("flagId").toString());reason=convert?QObject::tr("선택·약속 기록과 후속 조건 함께 전환"):QObject::tr("이 연결 밖에서 만든 공용 선행 기록 유지");}
      if(key.startsWith("event:")){convert=selected.contains(o.value("eventId").toString());reason=convert?QObject::tr("후속 사건의 개인별 완료 조건"):QObject::tr("이 연결 밖의 공용 선행 사건 유지");}
      if(key.startsWith("relationship:"))names[key]=names.value("character:"+o.value("characterId").toString(),o.value("characterId").toString())+" · "+names.value("relationship:"+o.value("relationshipId").toString(),o.value("relationshipId").toString());
      defaults[key]=convert;reasons[key]=reason;
    });
  }
  for(const auto &id:externalWrites)out.warnings<<QObject::tr("%1: 다른 사건·행동에도 공용 기록 쓰기가 있습니다. 바깥 기록은 유지되며 개인 기록을 대신 충족하지 않습니다.").arg(names.value("flag:"+id,id));
  for(const auto &f:fixed)out.warnings<<QObject::tr("%1: 특정 인물로 지정된 대상은 유지합니다.").arg(f);
  for(auto it=defaults.begin();it!=defaults.end();++it){const auto key=it.key();const bool locked=key.startsWith("attribute:")&&!it.value()||key.startsWith("event:");
    out.options.append(QJsonObject{{"key",key},{"label",names.value(key,key)},{"personal",locked?it.value():owners.value(key,it.value())},{"locked",locked},{"reason",reasons[key]}});
  }
  QMap<QString,bool> chosen;for(auto v:out.options){auto o=v.toObject();chosen[o.value("key").toString()]=o.value("personal").toBool();}
  out.documents=input;
  for(const auto &id:out.eventIds){auto e=events[id];auto d=input[indices[id]].toObject();const auto eventName=name(e),eventFile=d.value("file").toString();
    auto change=[&](QString path,QJsonValue before,QJsonValue after,QString label){out.changes.append(QJsonObject{{"file",eventFile},{"event",eventName},{"path",path},{"label",label},{"before",rendered(before)},{"after",rendered(after)}});};
    const auto original=d.value("data").toObject();if(original.value("scope")!="actor")change("/scope",QObject::tr("공용"),QObject::tr("캐릭터별"),QObject::tr("사건 기록 범위"));e["scope"]="actor";
    eventLogic(e,[&](QJsonObject &o,const QString &path){const auto key=token(o);if(!key.isEmpty()&&shared(o)&&chosen.value(key,false)){QJsonObject owner{{"kind","actor"}};change(path+"/owner",QJsonValue(QJsonValue::Undefined),owner,location(e,path)+" · "+names.value(key,key));o["owner"]=owner;}});
    d["data"]=e;out.documents[indices[id]]=d;
  }
  // Draft edits made before opening the preview must also be reviewable.
  if(!draft.isEmpty()){
    auto old=input[rootAt].toObject().value("data").toObject();auto keys=old.keys()+draft.keys();keys.removeDuplicates();
    for(const auto &key:keys)if(key!="scope"&&draft.value(key)!=old.value(key))out.changes.prepend(QJsonObject{{"file",file},{"event",name(draft)},{"path","/"+escaped(key)},{"label",QObject::tr("미적용 사건 편집")+" · "+key},{"before",old.contains(key)?rendered(old.value(key)):QStringLiteral("—")},{"after",draft.contains(key)?rendered(draft.value(key)):QStringLiteral("—")}});
  }
  out.warnings.removeDuplicates();out.errors.removeDuplicates();return out;
}

StoryScopeDialog::StoryScopeDialog(const QJsonArray &docs,const QString &file,const QJsonObject &draft,QWidget *parent):QDialog(parent){
  setObjectName("storyScopePreview");setWindowTitle(tr("캐릭터별 사건 전환 미리보기"));auto box=new QVBoxLayout(this);
  auto intro=new QLabel(tr("선택·약속과 후속 사건을 함께 전환합니다. 개인 능력·관계·소지품은 실행자에게, 공동 자금은 공용으로 유지합니다. 기존 저장의 공용 기록을 어느 인물에게도 임의 복제하지 않습니다."));intro->setWordWrap(true);box->addWidget(intro);
  auto options=new QTableWidget;options->setObjectName("scopeOwnerOptions");options->setColumnCount(3);options->setHorizontalHeaderLabels({tr("개인별"),tr("연결 데이터"),tr("처리 이유")});options->setColumnWidth(0,65);options->setColumnWidth(1,350);options->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch);options->setMaximumHeight(220);box->addWidget(options);
  auto summary=new QLabel;summary->setObjectName("scopeSummary");summary->setWordWrap(true);box->addWidget(summary);
  auto changes=new QTreeWidget;changes->setObjectName("scopeChanges");changes->setHeaderLabels({tr("사건 · 항목"),tr("변경 전"),tr("변경 후")});changes->setColumnWidth(0,550);changes->setColumnWidth(1,180);box->addWidget(changes,1);
  auto problem=new QLabel;problem->setObjectName("scopeProblems");problem->setWordWrap(true);problem->setTextFormat(Qt::PlainText);problem->setTextInteractionFlags(Qt::TextSelectableByMouse);box->addWidget(problem);
  auto acknowledge=new QCheckBox(tr("위의 유지 항목과 외부 연결 영향을 확인했습니다."));acknowledge->setObjectName("scopeAcknowledge");box->addWidget(acknowledge);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel);buttons->setObjectName("scopeButtons");buttons->button(QDialogButtonBox::Apply)->setText(tr("검토한 전환 적용"));box->addWidget(buttons);
  resultPlan=planPersonalStory(docs,file,draft);options->setRowCount(resultPlan.options.size());
  for(int i=0;i<resultPlan.options.size();++i){auto o=resultPlan.options[i].toObject();auto c=new QCheckBox;c->setChecked(o.value("personal").toBool());c->setEnabled(!o.value("locked").toBool());c->setProperty("key",o.value("key").toString());options->setCellWidget(i,0,c);options->setItem(i,1,new QTableWidgetItem(o.value("label").toString()));options->setItem(i,2,new QTableWidgetItem(o.value("reason").toString()));}
  options->setEditTriggers(QAbstractItemView::NoEditTriggers);
  auto refresh=[=]{QMap<QString,bool> owners;for(int i=0;i<options->rowCount();++i){auto c=qobject_cast<QCheckBox*>(options->cellWidget(i,0));owners[c->property("key").toString()]=c->isChecked();}resultPlan=planPersonalStory(docs,file,draft,owners);changes->clear();QMap<QString,QTreeWidgetItem*> groups;
    for(auto v:resultPlan.changes){auto c=v.toObject();auto key=c.value("file").toString();if(!groups.contains(key)){groups[key]=new QTreeWidgetItem(changes,{c.value("event").toString()});groups[key]->setToolTip(0,key);groups[key]->setExpanded(true);}auto row=new QTreeWidgetItem(groups[key],{c.value("label").toString(),c.value("before").toString(),c.value("after").toString()});row->setToolTip(0,c.value("path").toString());}
    summary->setText(tr("사건 %1개 · 변경 %2개 · 전체를 한 번에 적용하고 되돌릴 수 있습니다.").arg(resultPlan.eventIds.size()).arg(resultPlan.changes.size()));problem->setText((resultPlan.errors+resultPlan.warnings).join("\n"));acknowledge->setVisible(!resultPlan.warnings.isEmpty());buttons->button(QDialogButtonBox::Apply)->setEnabled(resultPlan.errors.isEmpty()&&(resultPlan.warnings.isEmpty()||acknowledge->isChecked()));
  };
  for(int i=0;i<options->rowCount();++i)connect(qobject_cast<QCheckBox*>(options->cellWidget(i,0)),&QCheckBox::toggled,this,[=]{acknowledge->setChecked(false);refresh();});
  connect(acknowledge,&QCheckBox::toggled,this,refresh);connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,this,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);refresh();resize(1000,720);
}
