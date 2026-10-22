#include "story-followup.hpp"
#include "story-connections.hpp"
#include "script-continuation.hpp"
#include "script-identity.hpp"
#include <QComboBox>
#include <QCryptographicHash>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
int atFile(const QJsonArray &docs, const QString &file) {
  for (int i=0;i<docs.size();++i) if (docs[i].toObject().value("file")==file) return i;
  return -1;
}
QJsonObject content(const QJsonArray &docs, int at) { return docs.at(at).toObject().value("data").toObject(); }
void put(QJsonArray &docs, int at, const QJsonObject &data) {
  auto doc=docs.at(at).toObject();doc["data"]=data;docs[at]=doc;
}
QJsonObject conjunction(QJsonObject old, const QJsonObject &condition) {
  if(old==condition) return old;
  if(old.isEmpty()||old.value("op")=="always") return condition;
  auto children=old.value("op")=="all"?old.value("conditions").toArray():QJsonArray{old};
  if(!children.contains(condition))children.append(condition);
  return {{"op","all"},{"conditions",children}};
}
QString label(QJsonObject d) { return d.value("label").toString(d.value("name").toString(d.value("id").toString())); }
}

QJsonArray storyFollowupPromises(const QJsonArray &docs, const QString &source) {
  const auto model=buildStoryConnections(docs);const auto n=model.nodes.value(source);
  if(n.kind!="event"&&n.kind!="choice")return {};
  const int at=atFile(docs,n.file);if(at<0)return {};
  const auto data=content(docs,at);QMap<QString,QJsonObject> nodes;QJsonArray effects;
  for(auto v:data.value("nodes").toArray())nodes[v.toObject().value("id").toString()]=v.toObject();
  QStringList queue{data.value("entryNodeId").toString()};
  if(n.kind=="choice")for(auto v:nodes.value(n.scriptNode).value("choices").toArray())if(v.toObject().value("id")==n.choice){effects=v.toObject().value("effects").toArray();queue={v.toObject().value("next").toString()};}
  QSet<QString> seen;
  for(int i=0;i<queue.size();++i){const auto id=queue[i];if(seen.contains(id)||!nodes.contains(id))continue;seen.insert(id);const auto node=nodes[id];
    for(auto e:node.value("effects").toArray())effects.append(e);
    for(const auto &key:{"next","then","else"})if(node.value(key).isString())queue.append(node.value(key).toString());
    // Stop at the next choice: its promises belong to a different player decision.
  }
  QMap<QString,QString> names;for(auto v:docs)if(v.toObject().value("kind")=="flags")for(auto f:v.toObject().value("data").toObject().value("flags").toArray())names[f.toObject().value("id").toString()]=label(f.toObject());
  QJsonArray result;QSet<QString> added;
  for(auto v:effects){auto e=v.toObject();if(e.value("op")!="setFlag")continue;const auto id=e.value("flagId").toString();if(!names.contains(id)||id.startsWith("story-choice-"))continue;
    const auto owner=e.value("owner").toObject();const bool personal=data.value("scope")=="actor";
    if(personal?owner.value("kind")!="actor":!owner.isEmpty())continue;
    const bool value=e.value("value").toBool();const auto key=id+(value?":true":":false");if(added.contains(key))continue;added.insert(key);
    QJsonObject condition{{"op","flag"},{"flagId",id},{"equals",value}};if(personal)condition["owner"]=owner;
    result.append(QJsonObject{{"label",names[id]+(personal?QObject::tr(" · 실행자별"):QString())+(value?QObject::tr(" · 성립"):QObject::tr(" · 해제"))},{"condition",condition}});
  }return result;
}

StoryFollowupResult makeStoryFollowup(const QJsonArray &input, const QString &source, const QJsonObject &s) {
  StoryFollowupResult out;auto fail=[&](QString message){out.error=message;return out;};
  const auto model=buildStoryConnections(input);const auto n=model.nodes.value(source);
  if((n.kind!="event"&&n.kind!="choice")||!n.id.startsWith("event:"))return fail(QObject::tr("선행 사건이나 선택 카드를 선택하세요."));
  QJsonArray docs=input;const int sourceAt=atFile(docs,n.file);auto before=content(docs,sourceAt);
  const bool personal=before.value("scope").toString("shared")=="actor";
  int projectAt=-1,flagsAt=-1;for(int i=0;i<docs.size();++i){auto kind=docs[i].toObject().value("kind");if(kind=="project")projectAt=i;if(kind=="flags")flagsAt=i;}
  if(projectAt<0)return fail(QObject::tr("프로젝트 설정을 찾을 수 없습니다."));
  auto project=content(docs,projectAt);
  if(personal&&project.value("roster").toObject().value("stateModel")!="character-v1")return fail(QObject::tr("캐릭터별 연결에는 캐릭터 소유 상태 설정이 필요합니다."));
  const int total=project.value("durationMonths").toInt()*project.value("periodsPerMonth").toInt();
  const auto targetFile=s.value("targetFile").toString();int targetAt=atFile(docs,targetFile);QJsonObject target;QString file=targetFile;
  if(!targetFile.isEmpty()){
    if(targetAt<0||docs[targetAt].toObject().value("kind")!="event")return fail(QObject::tr("연결할 후속 사건을 찾을 수 없습니다."));
    target=content(docs,targetAt);
    if((target.value("scope").toString("shared")=="actor")!=personal)return fail(QObject::tr("선행·후속 사건의 기록 범위가 다릅니다. 같은 범위의 사건을 선택하세요."));
    const auto trigger=target.value("trigger").toString("period-end");
    if(!(personal?QStringList{"interaction","after-event"}:QStringList{"period-end","after-event"}).contains(trigger))return fail(QObject::tr("후속 사건의 실행 방식을 확인하세요."));
    if(continuationCycle(docs,target.value("id").toString(),before.value("id").toString()))return fail(QObject::tr("서로의 완료를 기다리는 순환 연결입니다. 다른 후속 사건을 선택하세요."));
  }else{
    const int from=s.value("fromPeriod").toInt(-1),through=s.value("throughPeriod").toInt(-1);
    if(from<0||through<from||through>=total)return fail(QObject::tr("발생 기간을 프로젝트 일정 범위 안에서 설정하세요."));
    if(s.value("title").toString().trimmed().isEmpty()||s.value("dialogue").toString().trimmed().isEmpty())return fail(QObject::tr("후속 사건 이름과 첫 대사를 입력하세요."));
    auto speaker=s.value("speakerId");if(!speaker.isNull()&&!speaker.isUndefined()){
      bool found=false;for(auto v:docs)if(v.toObject().value("kind")=="character"&&v.toObject().value("data").toObject().value("id")==speaker)found=true;
      if(!found)return fail(QObject::tr("대사를 말할 인물을 다시 선택하세요."));
    }
    QSet<QString> ids;for(auto v:docs)ids.insert(v.toObject().value("data").toObject().value("id").toString());
    int number=1;QString id;do{id="followup-"+QString::number(number++);file="events/"+id+".json";}while(ids.contains(id)||atFile(docs,file)>=0);
    QJsonArray nodes;QString entry="dialogue-1";
    // Preserve the source's authored visual setup without copying rewards or branch logic.
    for(auto v:before.value("nodes").toArray())if(v.toObject().value("id")==before.value("entryNodeId")&&v.toObject().value("type")=="scene"){
      nodes.append(QJsonObject{{"id","scene"},{"type","scene"},{"scene",v.toObject().value("scene")},{"next","dialogue-1"}});entry="scene";
    }
    nodes.append(QJsonObject{{"id","dialogue-1"},{"type","dialogue"},{"speakerId",speaker.isUndefined()?QJsonValue::Null:speaker},{"text",s.value("dialogue").toString().trimmed()},{"next","end"}});
    nodes.append(QJsonObject{{"id","end"},{"type","end"}});
    target={{"formatVersion","0.1.0"},{"id",id},{"label",s.value("title").toString().trimmed()},{"window",QJsonObject{{"fromPeriod",from},{"throughPeriod",through}}},{"priority",50},{"trigger",personal?"after-event":"period-end"},{"requirements",QJsonObject{{"op","always"}}},{"entryNodeId",entry},{"nodes",nodes}};
  }
  if(personal)target["scope"]="actor";
  QJsonObject completed{{"op","eventCompleted"},{"eventId",before.value("id")}};if(personal)completed["owner"]=QJsonObject{{"kind","actor"}};
  auto requirements=conjunction(target.value("requirements").toObject(),completed);
  const auto promise=s.value("promise").toObject();
  if(!promise.isEmpty()){
    bool found=false;for(auto v:storyFollowupPromises(input,source))if(v.toObject().value("condition")==promise)found=true;
    if(!found)return fail(QObject::tr("선택한 약속이 변경되었습니다. 다시 선택하세요."));
    requirements=conjunction(requirements,promise);
  }
  if(n.kind=="choice"){
    if(flagsAt<0)return fail(QObject::tr("선택 기록을 저장할 약속 정의 문서를 찾을 수 없습니다."));
    const auto signature=n.file+"/"+n.scriptNode+"/"+n.choice+(personal?"/actor":"");
    const auto marker="story-choice-"+QString(QCryptographicHash::hash(signature.toUtf8(),QCryptographicHash::Sha256).toHex().left(24));
    const auto provenance="Story connection choice: "+signature;
    auto flags=content(docs,flagsAt);auto definitions=flags.value("flags").toArray();bool existing=false;
    for(auto v:definitions)if(v.toObject().value("id")==marker){if(v.toObject().value("description")!=provenance)return fail(QObject::tr("선택 기록 이름이 기존 데이터와 충돌합니다. 내용을 확인하세요."));existing=true;}
    auto initial=project.value("initialState").toObject(),values=initial.value("flags").toObject();
    if(!existing&&values.contains(marker))return fail(QObject::tr("선택 기록 이름이 기존 데이터와 충돌합니다. 내용을 확인하세요."));
    if(!existing)definitions.append(QJsonObject{{"id",marker},{"label",label(before)+" · "+n.title},{"initial",false},{"description",provenance}});
    if(!values.contains(marker))values[marker]=false;initial["flags"]=values;project["initialState"]=initial;put(docs,projectAt,project);flags["flags"]=definitions;put(docs,flagsAt,flags);
    auto nodes=before.value("nodes").toArray();bool found=false;
    for(int i=0;i<nodes.size();++i){auto node=nodes[i].toObject();if(node.value("id")!=n.scriptNode)continue;auto choices=node.value("choices").toArray();
      for(int j=0;j<choices.size();++j){auto choice=choices[j].toObject();if(choice.value("id")!=n.choice)continue;auto effects=choice.value("effects").toArray();QJsonObject record{{"op","setFlag"},{"flagId",marker},{"value",true}};if(personal)record["owner"]=QJsonObject{{"kind","actor"}};if(!effects.contains(record))effects.append(record);choice["effects"]=effects;choices[j]=choice;found=true;}
      node["choices"]=choices;nodes[i]=node;
    }
    if(!found)return fail(QObject::tr("선택이 변경되었습니다. 지도를 다시 여세요."));
    before["nodes"]=nodes;put(docs,sourceAt,before);QJsonObject recorded{{"op","flag"},{"flagId",marker},{"equals",true}};if(personal)recorded["owner"]=QJsonObject{{"kind","actor"}};requirements=conjunction(requirements,recorded);
  }
  target["requirements"]=requirements;
  if(targetAt<0)docs.append(QJsonObject{{"file",file},{"kind","event"},{"data",target}});else put(docs,targetAt,target);
  out.documents=docs;out.file=file;return out;
}

StoryFollowupDialog::StoryFollowupDialog(QJsonArray docs,QString source,QWidget *parent):QDialog(parent){
  setObjectName("storyFollowupDialog");setWindowTitle(tr("후속 사건 만들기 · 연결"));resize(780,650);
  const auto model=buildStoryConnections(docs);const auto n=model.nodes.value(source);const bool personal=content(docs,atFile(docs,n.file)).value("scope")=="actor";auto outer=new QVBoxLayout(this);
  auto summary=new QLabel(tr("선행: %1\n사건 완료 + 선택 기록 + 고른 약속을 모두 만족할 때 후속 사건이 열립니다. 선택 기록은 한 번이라도 선택했는지를 기억합니다.").arg(n.title));summary->setWordWrap(true);outer->addWidget(summary);
  auto form=new QFormLayout;outer->addLayout(form);auto target=new QComboBox;target->setObjectName("followupTarget");target->addItem(tr("새 후속 사건 만들기"),"");
  int total=1;QJsonObject before;for(auto v:docs){auto doc=v.toObject(),data=doc.value("data").toObject();if(doc.value("file")==n.file)before=data;
    if(doc.value("kind")=="project")total=qMax(1,data.value("durationMonths").toInt()*data.value("periodsPerMonth").toInt());
    if(doc.value("kind")=="event"&&doc.value("file")!=n.file&&(data.value("scope").toString("shared")=="actor")==personal&&(personal?QStringList{"interaction","after-event"}:QStringList{"period-end","after-event"}).contains(data.value("trigger").toString("period-end")))target->addItem(label(data)+" · "+scriptIdentity(doc,docs,docs),doc.value("file").toString());}
  form->addRow(tr("후속 사건"),target);auto title=new QLineEdit;title->setObjectName("followupTitle");form->addRow(tr("새 사건 이름"),title);
  auto speaker=new QComboBox;speaker->setObjectName("followupSpeaker");speaker->addItem(tr("해설"),QString());for(auto v:docs)if(v.toObject().value("kind")=="character"){auto d=v.toObject().value("data").toObject();speaker->addItem(label(d),d.value("id").toString());}form->addRow(tr("말하는 인물"),speaker);
  auto dialogue=new QPlainTextEdit;dialogue->setObjectName("followupDialogue");dialogue->setMaximumHeight(150);form->addRow(tr("첫 대사"),dialogue);
  auto from=new QSpinBox;from->setObjectName("followupFrom");from->setRange(1,total);from->setValue(qMin(total,before.value("window").toObject().value("fromPeriod").toInt()+(personal?1:2)));
  auto through=new QSpinBox;through->setObjectName("followupThrough");through->setRange(1,total);through->setValue(total);form->addRow(tr("발생 시작 주기 (1부터)"),from);form->addRow(tr("발생 마지막 주기"),through);
  auto promise=new QComboBox;promise->setObjectName("followupPromise");promise->addItem(tr("약속 조건 추가 안 함"),QJsonObject{});for(auto v:storyFollowupPromises(docs,source)){auto p=v.toObject();promise->addItem(p.value("label").toString(),p.value("condition").toObject());}form->addRow(tr("함께 필요한 약속"),promise);
  auto preview=new QLabel;preview->setObjectName("followupPreview");preview->setWordWrap(true);outer->addWidget(preview);
  auto update=[=]{const bool create=target->currentData().toString().isEmpty();for(auto w:QList<QWidget*>{title,speaker,dialogue,from,through})w->setEnabled(create);
    preview->setText(tr("자동 조건: %1 완료").arg(label(before))+(n.kind=="choice"?tr("\n선택 기록: %1").arg(n.title):QString())+(promise->currentIndex()>0?tr("\n약속: %1").arg(promise->currentText()):QString())+(personal?tr("\n같은 실행자의 완료·선택·약속만 검사합니다. 새 사건은 대본 종료 후 같은 캐릭터로 이어집니다. 기존 사건의 실행 방식은 유지합니다."):tr("\n기존 조건과 효과는 유지합니다. 일정 끝에 검사하며 즉시 대본으로 뛰어들지 않습니다.")));};
  connect(target,qOverload<int>(&QComboBox::currentIndexChanged),this,[=]{update();});connect(promise,qOverload<int>(&QComboBox::currentIndexChanged),this,[=]{update();});update();
  auto error=new QLabel;error->setObjectName("followupError");error->setWordWrap(true);error->setStyleSheet("color:#ed987f");outer->addWidget(error);auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons->setObjectName("followupButtons");outer->addWidget(buttons);
  connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);connect(buttons,&QDialogButtonBox::accepted,this,[=]{
    QJsonObject settings{{"targetFile",target->currentData().toString()},{"title",title->text()},{"dialogue",dialogue->toPlainText()},{"speakerId",speaker->currentData().toString().isEmpty()?QJsonValue::Null:QJsonValue(speaker->currentData().toString())},{"fromPeriod",from->value()-1},{"throughPeriod",through->value()-1},{"promise",promise->currentData().value<QJsonObject>()}};
    auto plan=makeStoryFollowup(docs,source,settings);if(!plan.error.isEmpty()){error->setText(plan.error);return;}resultPlan=plan;accept();
  });
}
