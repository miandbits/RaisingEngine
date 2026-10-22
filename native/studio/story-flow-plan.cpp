#include "story-flow.hpp"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QSet>
#include <QRegularExpression>
#include <QUuid>
#include <cmath>

namespace {
QJsonObject literal(int value) { return {{"op", "literal"}, {"value", value}}; }
QJsonObject flag(QString id) { return {{"op", "flag"}, {"flagId", id}, {"equals", true}}; }
QJsonObject setFlag(QString id) { return {{"op", "setFlag"}, {"flagId", id}, {"value", true}}; }
QString digest(QJsonObject data) {
  return QCryptographicHash::hash(QJsonDocument(data).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex();
}
bool integer(QJsonValue v, int min, int max) {
  return v.isDouble() && std::isfinite(v.toDouble()) && v.toDouble() == std::floor(v.toDouble()) && v.toDouble() >= min && v.toDouble() <= max;
}
}

QJsonObject newStoryFlowSettings(const QJsonObject &view) {
  int periods = 1;
  for (auto v : view.value("documents").toArray()) if (v.toObject().value("kind") == "project") {
    const auto p = v.toObject().value("data").toObject(); periods = p.value("durationMonths").toInt(1) * p.value("periodsPerMonth").toInt(1);
  }
  return {{"id", "story-" + QUuid::createUuid().toString(QUuid::Id128).left(12)}, {"name", ""},
    {"newCharacter", true}, {"characterName", ""}, {"newRelationship", true}, {"relationshipLabel", QObject::tr("신뢰")}, {"relationshipInitial", 0},
    {"newActivity", true}, {"activityLabel", ""}, {"cost", 0}, {"gain", 1},
    {"encounterTitle", ""}, {"encounterText", ""}, {"prompt", QObject::tr("어떻게 답할까?")},
    {"meetingTrigger", "after-activity"}, {"fromPeriod", 1}, {"throughPeriod", periods}, {"choices", QJsonArray{}}};
}

StoryFlowPlan makeStoryFlowPlan(const QJsonObject &view, QJsonObject s) {
  StoryFlowPlan out;
  auto fail = [&](QString e) { out.error = e; return out; };
  QJsonObject project, flagDocument; QString projectFile, flagFile;
  QMap<QString, QJsonObject> docs;
  QSet<QString> ids;
  for (auto v : view.value("documents").toArray()) {
    auto d = v.toObject(); docs[d.value("file").toString()] = d;
    ids.insert(d.value("data").toObject().value("id").toString());
    if (d.value("kind") == "project") { project = d.value("data").toObject(); projectFile = d.value("file").toString(); }
    if (d.value("kind") == "flags") { flagDocument = d.value("data").toObject(); flagFile = d.value("file").toString(); }
  }
  if (project.empty()) return fail(QObject::tr("먼저 프로젝트를 여세요."));
  if (flagFile.isEmpty()) return fail(QObject::tr("프로젝트의 상태 정의 문서가 없습니다. 프로젝트 검증에서 누락된 문서를 확인하세요."));
  const int total = project.value("durationMonths").toInt() * project.value("periodsPerMonth").toInt();
  const QString id = s.value("id").toString();
  if (!QRegularExpression("^story-[a-z0-9-]{4,24}$").match(id).hasMatch()) return fail(QObject::tr("이야기 연결 식별 정보가 올바르지 않습니다. 새 이야기로 다시 시작하세요."));
  QJsonObject previous;
  auto flows = project.value("authoring").toObject().value("storyFlows").toArray();
  int flowIndex = -1;
  for (int i = 0; i < flows.size(); ++i) if (flows[i].toObject().value("id") == id) { previous = flows[i].toObject(); flowIndex = i; }
  if (!previous.empty()) {
    // Never flatten an advanced edit made in another authoring tool.
    for (auto v : previous.value("ownedDocuments").toArray()) {
      auto expected = v.toObject(); auto d = docs.value(expected.value("file").toString());
      if (d.empty() || digest(d.value("data").toObject()) != expected.value("digest").toString())
        return fail(QObject::tr("연결된 콘텐츠가 다른 편집기에서 변경되었거나 삭제되었습니다. 덮어쓰지 않았습니다. 대본/콘텐츠 편집기에서 변경 내용을 확인하세요."));
    }
    for (const auto &key : {"newCharacter", "characterId", "newRelationship", "relationshipId", "newActivity", "activityId"})
      if (s.value(key) != previous.value(key)) return fail(QObject::tr("기존 이야기의 연결 대상은 바꿀 수 없습니다. 다른 대상은 새 이야기로 만드세요."));
  }
  for (const auto &key : {"name", "encounterTitle", "encounterText", "prompt"})
    if (s.value(key).toString().trimmed().isEmpty()) return fail(QObject::tr("이야기 이름, 만남 제목, 대사와 질문을 입력하세요."));
  if (!integer(s.value("fromPeriod"), 1, total) || !integer(s.value("throughPeriod"), s.value("fromPeriod").toInt(), total))
    return fail(QObject::tr("만남의 시작·마지막 일정을 프로젝트 기간 안에서 지정하세요."));
  const auto meetingTrigger=s.value("meetingTrigger").toString("period-end");
  if(meetingTrigger!="period-end"&&meetingTrigger!="after-activity")return fail(QObject::tr("만남이 열리는 때를 선택하세요."));
  if (s.value("newCharacter").toBool()) s["characterId"] = id + "-person";
  if (s.value("newRelationship").toBool()) s["relationshipId"] = id + "-bond";
  if (s.value("newActivity").toBool()) s["activityId"] = id + "-activity";
  const auto personId = s.value("characterId").toString(), bondId = s.value("relationshipId").toString(), activityId = s.value("activityId").toString();
  QString personFile, activityFile;
  QJsonObject person, activity;
  for (auto d : docs) {
    if (d.value("kind") == "character" && d.value("data").toObject().value("id") == personId) { personFile = d.value("file").toString(); person = d.value("data").toObject(); }
    if (d.value("kind") == "activity" && d.value("data").toObject().value("id") == activityId) { activityFile = d.value("file").toString(); activity = d.value("data").toObject(); }
  }
  QMap<QString,QJsonObject> replacements;
  QJsonArray adds, owned; QSet<QString> managed;
  for(auto v:previous.value("ownedDocuments").toArray())managed.insert(v.toObject().value("file").toString());
  auto put = [&](QString kind, QString file, QJsonObject data, bool own) -> bool {
    if (!docs.contains(file)) {
      if (ids.contains(data.value("id").toString())) return false;
      adds.append(QJsonObject{{"type","add"},{"kind",kind},{"id",data.value("id")}});
    } else if (own && !managed.contains(file)) return false;
    replacements[file] = data;
    if (own) owned.append(QJsonObject{{"file",file},{"digest",digest(data)}});
    return true;
  };
  if (s.value("newCharacter").toBool()) {
    if (s.value("characterName").toString().trimmed().isEmpty()) return fail(QObject::tr("새 인물의 이름을 입력하세요."));
    if (person.empty()) person = {{"formatVersion","0.1.0"},{"id",personId},{"portraits",QJsonObject{}},{"relationships",QJsonArray{}}};
    person["name"] = s.value("characterName").toString().trimmed(); personFile = "characters/" + personId + ".json";
  } else if (person.empty()) return fail(QObject::tr("만날 인물을 선택하세요."));
  auto bonds = person.value("relationships").toArray(); QJsonObject bond; int bondIndex=-1;
  for (int i=0;i<bonds.size();++i) if(bonds[i].toObject().value("id")==bondId){bond=bonds[i].toObject();bondIndex=i;}
  if(s.value("newRelationship").toBool()) {
    if(s.value("relationshipLabel").toString().trimmed().isEmpty() || !integer(s.value("relationshipInitial"),-100,100)) return fail(QObject::tr("관계 이름과 초기값(-100~100)을 입력하세요."));
    bond={{"id",bondId},{"label",s.value("relationshipLabel").toString().trimmed()},{"min",-100},{"max",100},{"initial",s.value("relationshipInitial")}};
    if(bondIndex<0)bonds.append(bond);else bonds[bondIndex]=bond;
    person["relationships"]=bonds;
  } else if(bond.empty()) return fail(QObject::tr("변화시킬 관계를 선택하세요."));
  if (s.value("newCharacter").toBool() || s.value("newRelationship").toBool())
    if(!put("character",personFile,person,s.value("newCharacter").toBool()))return fail(QObject::tr("인물 연결이 충돌합니다. 새 이야기로 다시 시작하세요."));
  const auto relationTarget=QJsonObject{{"kind","relationship"},{"characterId",personId},{"relationshipId",bondId}};
  QSet<QString> attributes;
  for(auto d:docs)if(d.value("kind")=="attributes")for(auto a:d.value("data").toObject().value("attributes").toArray())attributes.insert(a.toObject().value("id").toString());
  const QString doneFlag=id+"-activity-done";
  if(s.value("newActivity").toBool()) {
    if(s.value("activityLabel").toString().trimmed().isEmpty())return fail(QObject::tr("활동 이름을 입력하세요."));
    if(!integer(s.value("cost"),0,1000000)||!integer(s.value("gain"),-1000000,1000000))return fail(QObject::tr("비용과 효과의 숫자 범위를 확인하세요."));
    QJsonArray costs,effects;
    const auto currency=s.value("costAttributeId").toString(),gain=s.value("gainAttributeId").toString();
    if(s.value("cost").toInt()!=0){if(!attributes.contains(currency))return fail(QObject::tr("비용 자원을 선택하세요."));costs.append(QJsonObject{{"attributeId",currency},{"amount",s.value("cost")}});}
    if(s.value("gain").toInt()!=0){if(!attributes.contains(gain))return fail(QObject::tr("변화시킬 능력을 선택하세요."));effects.append(QJsonObject{{"op","add"},{"target",QJsonObject{{"kind","attribute"},{"attributeId",gain}}},{"value",literal(s.value("gain").toInt())}});}
    activity={{"formatVersion","0.1.0"},{"id",activityId},{"label",s.value("activityLabel").toString().trimmed()},{"description",s.value("name")},{"requirements",QJsonObject{{"op","always"}}},{"costs",costs},{"baseEffects",effects}};
    activityFile="activities/"+activityId+".json";
  } else if(activity.empty())return fail(QObject::tr("일정에 배정할 활동을 선택하세요."));
  auto effects=activity.value("baseEffects").toArray();bool hasMarker=false;
  for(auto e:effects)if(e.toObject().value("op")=="setFlag"&&e.toObject().value("flagId")==doneFlag)hasMarker=true;
  if(!hasMarker) effects.append(setFlag(doneFlag));
  activity["baseEffects"]=effects;
  if(!put("activity",activityFile,activity,s.value("newActivity").toBool()))return fail(QObject::tr("활동 연결이 충돌합니다."));
  // Shared project containers are merged against the current revision, never replaced from an old draft.
  auto initial=project.value("initialState").toObject();auto flags=initial.value("flags").toObject();
  auto flagDefinitions=flagDocument.value("flags").toArray();
  auto declareFlag=[&](QString key,QString label){for(auto v:flagDefinitions)if(v.toObject().value("id")==key)return;flagDefinitions.append(QJsonObject{{"id",key},{"label",label},{"initial",false}});};
  if(previous.empty()&&flags.contains(doneFlag))return fail(QObject::tr("이야기 연결 이름이 충돌합니다. 새 이야기로 다시 시작하세요."));
  if(!flags.contains(doneFlag))flags[doneFlag]=false;
  declareFlag(doneFlag,s.value("name").toString()+QObject::tr(" · 활동 경험"));
  const auto meetingId=id+"-meeting";
  QJsonArray nodes{QJsonObject{{"id","greeting"},{"type","dialogue"},{"speakerId",personId},{"text",s.value("encounterText")},{"next","question"}}};
  QJsonArray choices;
  const auto rows=s.value("choices").toArray();
  if(rows.size()<2||rows.size()>8)return fail(QObject::tr("선택지는 2~8개를 만드세요."));
  QSet<QString> branchIds;
  for(auto v:rows){auto row=v.toObject();const auto cid=row.value("id").toString();
    if(!QRegularExpression("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$").match(cid).hasMatch()||branchIds.contains(cid)||cid.size()>22)
      return fail(QObject::tr("선택지 연결 정보가 중복되었습니다. 해당 선택지를 다시 만드세요."));
    branchIds.insert(cid);
    if(row.value("label").toString().trimmed().isEmpty()||row.value("reply").toString().trimmed().isEmpty()||!integer(row.value("delta"),-1000000,1000000))return fail(QObject::tr("각 선택지의 문구, 응답 대사와 관계 변화량을 입력하세요."));
    const auto branchFlag=id+"-"+cid;
    if(!flags.contains(branchFlag))flags[branchFlag]=false;
    declareFlag(branchFlag,s.value("name").toString()+" · "+row.value("label").toString());
    choices.append(QJsonObject{{"id",cid},{"text",row.value("label")},{"requirements",QJsonObject{{"op","always"}}},{"effects",QJsonArray{}},{"next",cid+"-effect"}});
    nodes.append(QJsonObject{{"id",cid+"-effect"},{"type","effect"},{"effects",QJsonArray{setFlag(branchFlag),QJsonObject{{"op","add"},{"target",relationTarget},{"value",literal(row.value("delta").toInt())}}}},{"next",cid+"-reply"}});
    nodes.append(QJsonObject{{"id",cid+"-reply"},{"type","dialogue"},{"speakerId",personId},{"text",row.value("reply")},{"next","end"}});
    if(row.value("followupEnabled").toBool()) {
      if(row.value("followupTitle").toString().trimmed().isEmpty()||row.value("followupText").toString().trimmed().isEmpty())return fail(QObject::tr("후속 사건을 켠 선택지에는 제목과 대사를 입력하세요."));
      if(!integer(row.value("followupFrom"),s.value("fromPeriod").toInt(),total)||!integer(row.value("minimumRelationship"),bond.value("min").toInt(),bond.value("max").toInt()))return fail(QObject::tr("후속 사건의 시작 일정과 관계 기준이 범위를 벗어났습니다."));
      const int through=row.value("followupThrough").toInt(total);
      const auto trigger=row.value("followupTrigger").toString("period-end");
      if((row.contains("followupThrough")&&!integer(row.value("followupThrough"),row.value("followupFrom").toInt(),total)) || (trigger!="period-end"&&trigger!="after-activity"))return fail(QObject::tr("후속 사건의 마지막 날짜와 발생 시점을 확인하세요."));
      const auto eventId=id+"-"+cid+"-next";
      QJsonObject event{{"formatVersion","0.1.0"},{"id",eventId},{"label",row.value("followupTitle")},{"window",QJsonObject{{"fromPeriod",row.value("followupFrom").toInt()-1},{"throughPeriod",through-1}}},{"priority",70},{"trigger",trigger},
        {"requirements",QJsonObject{{"op","all"},{"conditions",QJsonArray{QJsonObject{{"op","eventCompleted"},{"eventId",meetingId}},flag(branchFlag),QJsonObject{{"op","compare"},{"comparator","gte"},{"left",QJsonObject{{"op","read"},{"target",relationTarget}}},{"right",literal(row.value("minimumRelationship").toInt())}}}}}},
        {"entryNodeId","line"},{"nodes",QJsonArray{QJsonObject{{"id","line"},{"type","dialogue"},{"speakerId",personId},{"text",row.value("followupText")},{"next","end"}},QJsonObject{{"id","end"},{"type","end"}}}}};
      if(trigger=="after-activity")event["activityId"]=activityId;
      if(!put("event","events/"+eventId+".json",event,true))return fail(QObject::tr("후속 사건 파일이 충돌합니다."));
    }
  }
  nodes.append(QJsonObject{{"id","question"},{"type","choice"},{"prompt",s.value("prompt")},{"choices",choices}});nodes.append(QJsonObject{{"id","end"},{"type","end"}});
  QJsonObject meeting{{"formatVersion","0.1.0"},{"id",meetingId},{"label",s.value("encounterTitle")},{"window",QJsonObject{{"fromPeriod",s.value("fromPeriod").toInt()-1},{"throughPeriod",s.value("throughPeriod").toInt()-1}}},{"requirements",flag(doneFlag)},{"priority",80},{"trigger",meetingTrigger},{"entryNodeId","greeting"},{"nodes",nodes}};
  const auto predecessor=s.value("predecessorEventId").toString();
  if(!predecessor.isEmpty()) {
    bool found=false;for(const auto &doc:docs)if(doc.value("kind")=="event"&&doc.value("data").toObject().value("id")==predecessor)found=true;
    if(!found||predecessor==meetingId||predecessor.startsWith(id+"-"))return fail(QObject::tr("선행 사건은 이 이야기 밖의 기존 사건을 선택하세요."));
    meeting["requirements"]=QJsonObject{{"op","all"},{"conditions",QJsonArray{flag(doneFlag),QJsonObject{{"op","eventCompleted"},{"eventId",predecessor}}}}};
  }
  if(meetingTrigger=="after-activity")meeting["activityId"]=activityId;
  if(!put("event","events/"+meetingId+".json",meeting,true))return fail(QObject::tr("만남 사건 파일이 충돌합니다."));
  // Removed branches keep their documents but become unreachable, preserving outside references and saves.
  for(auto v:previous.value("ownedDocuments").toArray()){auto d=v.toObject();const auto file=d.value("file").toString();if(!replacements.contains(file)&&file.startsWith("events/")){
    auto data=docs.value(file).value("data").toObject();data["requirements"]=QJsonObject{{"op","not"},{"condition",QJsonObject{{"op","always"}}}};put("event",file,data,true);
  }}
  initial["flags"]=flags;project["initialState"]=initial;
  flagDocument["flags"]=flagDefinitions;replacements[flagFile]=flagDocument;
  auto visual=project.value("visual").toObject();auto screens=visual.value("screens").toArray();const auto link=s.value("scheduleLink").toObject();bool linked=link.empty();
  bool grouped=false, listed=false;
  for(int i=0;i<screens.size();++i){auto screen=screens[i].toObject();auto elements=screen.value("elements").toArray();
    for(int j=0;j<elements.size();++j){auto e=elements[j].toObject();if(e.value("kind")!="schedule")continue;auto groups=e.value("scheduleGroups").toArray();if(!groups.empty())grouped=true;else listed=true;
      for(int k=0;k<groups.size();++k){auto g=groups[k].toObject();auto activities=g.value("activityIds").toArray();if(activities.contains(activityId))listed=true;
        if(screen.value("id")==link.value("screenId")&&e.value("id")==link.value("elementId")&&g.value("id")==link.value("groupId")){if(!activities.contains(activityId))activities.append(activityId);g["activityIds"]=activities;groups[k]=g;linked=true;listed=true;}}
      if(!groups.empty()) e["scheduleGroups"]=groups;
      elements[j]=e;}
    screen["elements"]=elements;screens[i]=screen;}
  if(!linked||(grouped&&!listed))return fail(QObject::tr("일정 화면의 분류를 골라 활동을 연결하세요. 연결하지 않으면 플레이 목록에 나타나지 않습니다."));
  if(project.value("visual").isObject()){visual["screens"]=screens;project["visual"]=visual;}
  s["ownedDocuments"]=owned;out.settings=s;
  if(flowIndex<0)flows.append(s);else flows[flowIndex]=s;auto authoring=project.value("authoring").toObject();authoring["storyFlows"]=flows;project["authoring"]=authoring;
  out.edits=adds;
  for(auto it=replacements.begin();it!=replacements.end();++it)out.edits.append(QJsonObject{{"type","replace"},{"file",it.key()},{"data",it.value()}});
  out.edits.append(QJsonObject{{"type","replace"},{"file",projectFile},{"data",project}});
  return out;
}
