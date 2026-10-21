#include "story-connections.hpp"
#include "story-followup.hpp"
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include "script-authoring.hpp"
#include <QApplication>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QTimer>
#include <QElapsedTimer>
#include <QTreeWidget>
#include <QTest>
#include <iostream>

int main(int argc,char **argv){
  QApplication app(argc,argv);int checks=0;auto check=[&](bool pass,const char *message){++checks;if(!pass){std::cerr<<"FAIL: "<<message<<std::endl;std::exit(1);}};
  auto doc=[](QString kind,QJsonObject d){return QJsonObject{{"kind",kind},{"file",kind+"s/"+d.value("id").toString()+".json"},{"data",d}};};
  auto flag=[](bool value){return QJsonObject{{"op","flag"},{"flagId","promise"},{"equals",value}};};
  auto set=[](bool value){return QJsonObject{{"op","setFlag"},{"flagId","promise"},{"value",value}};};
  auto script=[](QString id,QJsonObject requirements){return QJsonObject{{"id",id},{"label",id},{"requirements",requirements},{"entryNodeId","end"},{"nodes",QJsonArray{QJsonObject{{"id","end"},{"type","end"}}}}};};
  auto a=script("first",{{"op","stateIs"},{"stateGroupId","activity"},{"stateValueId","rest"}});
  a["entryNodeId"]="ask";a["nodes"]=QJsonArray{QJsonObject{{"id","ask"},{"type","choice"},{"prompt","Will you keep your promise?"},{"choices",QJsonArray{
    QJsonObject{{"id","yes"},{"text","Keep the promise"},{"effects",QJsonArray{set(true)}},{"requirements",QJsonObject{{"op","always"}}},{"next","end"}},
    QJsonObject{{"id","no"},{"text","Decline"},{"effects",QJsonArray{set(false)}},{"requirements",QJsonObject{{"op","always"}}},{"next","end"}}}}},
    QJsonObject{{"id","end"},{"type","end"}},QJsonObject{{"id","orphan"},{"type","effect"},{"next","end"},{"effects",QJsonArray{QJsonObject{{"op","setFlag"},{"flagId","unreachable"},{"value",true}}}}}};
  auto b=script("follow",{{"op","all"},{"conditions",QJsonArray{flag(true),QJsonObject{{"op","eventCompleted"},{"eventId","first"}}}}});
  auto c=script("alternative",{{"op","any"},{"conditions",QJsonArray{flag(false),QJsonObject{{"op","eventCompleted"},{"eventId","follow"}}}}});
  auto d=script("blocked",{{"op","not"},{"condition",QJsonObject{{"op","eventCompleted"},{"eventId","first"}}}});
  QJsonObject activity{{"id","rest"},{"label","Rest"},{"baseEffects",QJsonArray{QJsonObject{{"op","setState"},{"stateGroupId","activity"},{"stateValueId","rest"}}}}};
  QJsonObject gameplay{{"locations",QJsonArray{QJsonObject{{"id","clinic"},{"name","Clinic"}}}},
    {"interactions",QJsonArray{QJsonObject{{"id","talk"},{"name","Talk"},{"locationId","clinic"},{"eventId","first"}}}}};
  QJsonArray documents{doc("event",a),doc("event",b),doc("event",c),doc("event",d),doc("activity",activity),doc("project",{{"id","game"},{"gameplay",gameplay}})};
  auto graph=buildStoryConnections(documents);
  auto find=[&](QString from,QString to){QStringList reasons;for(const auto &e:graph.edges)if(e.from==from&&e.to==to)reasons<<e.reason;return reasons.join('\n');};
  check(!find("activity:rest","event:first").isEmpty(),"activity state connects to event");
  check(!find("location:clinic","interactions:talk").isEmpty(),"location to interaction");
  check(!find("interactions:talk","event:first").isEmpty(),"interaction starts event");
  check(find("event:first/ask/yes","event:follow").contains("필수 조건"),"promise true producer links to required consumer");
  check(find("event:first/ask/no","event:follow").isEmpty(),"false producer must not satisfy true flag");
  check(find("event:first/ask/no","event:alternative").contains("대안 조건"),"OR remains optional");
  check(find("event:first","event:blocked").contains("배제 조건"),"NOT remains exclusion");
  check(find("event:first","event:follow").contains("필수 조건"),"completion prerequisite");
  auto deMorganDocs=documents;deMorganDocs.append(doc("event",script("demorgan",{{"op","not"},{"condition",QJsonObject{{"op","all"},{"conditions",QJsonArray{
    QJsonObject{{"op","not"},{"condition",QJsonObject{{"op","eventCompleted"},{"eventId","first"}}}},
    QJsonObject{{"op","not"},{"condition",QJsonObject{{"op","eventCompleted"},{"eventId","follow"}}}}}}}}})));
  auto deMorgan=buildStoryConnections(deMorganDocs);bool alternatives=true;for(const auto &e:deMorgan.edges)if(e.to=="event:demorgan")alternatives&=e.reason.contains("대안 조건");check(alternatives,"negated AND with negated leaves is OR, not mandatory");
  auto deferred=a;auto deferredNodes=deferred["nodes"].toArray();auto ask=deferredNodes[0].toObject();auto options=ask["choices"].toArray();auto yes=options[0].toObject();yes["effects"]=QJsonArray{};yes["next"]="later";options[0]=yes;ask["choices"]=options;deferredNodes[0]=ask;
  deferredNodes.append(QJsonObject{{"id","later"},{"type","effect"},{"effects",QJsonArray{set(true)}},{"next","end"}});deferred["nodes"]=deferredNodes;
  auto deferredDocs=documents;deferredDocs[0]=doc("event",deferred);auto deferredGraph=buildStoryConnections(deferredDocs);bool keptOrigin=false;for(const auto &e:deferredGraph.edges)if(e.from=="event:first/ask/yes"&&e.to=="event:follow")keptOrigin=true;
  check(keptOrigin,"effect after choice preserves choice origin");
  auto orphanDocs=documents;orphanDocs.append(doc("event",script("orphan-consumer",{{"op","flag"},{"flagId","unreachable"},{"equals",true}})));auto orphanGraph=buildStoryConnections(orphanDocs);bool orphanEdge=false;for(const auto &e:orphanGraph.edges)if(e.to=="event:orphan-consumer")orphanEdge=true;check(!orphanEdge,"unreachable script effects excluded");
  auto local=storyConnectionNeighborhood(graph,"event:follow",3);check(local.contains("activity:rest"),"upstream activity visible");check(local.contains("location:clinic"),"upstream place visible");
  check(storyConnectionNeighborhood(graph,"missing",3).isEmpty(),"unknown focus empty");
  graph.edges.append({"event:follow","event:first","cycle",true});check(storyConnectionNeighborhood(graph,"event:first",100).size()<=graph.nodes.size(),"cycles bounded");
  QJsonObject ctx{{"documents",documents}};int edits=0;
  StoryConnectionsDialog dialog(ctx,"events/follow.json",nullptr,[&](QString file,QString node,QString choice,QWidget*){
    ++edits;check(file=="events/follow.json"&&node.isEmpty()&&choice.isEmpty(),"correct condition edit target");
    auto updated=documents;auto item=updated[1].toObject();auto data=item.value("data").toObject();data["requirements"]=flag(false);item["data"]=data;updated[1]=item;return QJsonObject{{"documents",updated}};
  });dialog.show();app.processEvents();
  auto details=dialog.findChild<QLabel*>("connectionsDetails");check(details&&details->text().contains("follow"),"selected event details");
  auto overview=dialog.findChild<QGraphicsView*>("scriptGraphOverview");check(overview&&overview->transform().m11()<.5,"overview fits actual graph bounds");
  auto reasons=dialog.findChild<QTreeWidget*>("connectionsReasons");check(reasons&&reasons->topLevelItem(0)->sizeHint(0).height()>=80,"connection reason rows show multiline context");
  auto condition=dialog.findChild<QPushButton*>("connectionsConditions");check(condition&&condition->isEnabled(),"condition button enabled");condition->click();app.processEvents();check(edits==1,"condition callback invoked");
  auto view=dialog.findChild<QGraphicsView*>("connectionsGraph");check(view&&!view->scene()->items().isEmpty(),"graph rendered");
  dialog.findChild<QPushButton*>("connectionsScript")->click();check(dialog.result()==QDialog::Accepted&&dialog.selectedFile=="events/follow.json","script navigation exact file");
  // Both graph edits and ordinary authoring use the same draft; rejecting the authoring dialog never applies it.
  ScriptAuthoringDialog author(ctx,{});author.selectDocument("events/follow.json");auto label=author.findChild<QLineEdit*>("scriptLabel");check(label!=nullptr,"authoring label");label->setFocus();label->selectAll();QTest::keyClicks(label,"Draft followup");
  QTimer::singleShot(0,&author,[&]{auto map=author.findChild<QDialog*>("storyConnectionsDialog");check(map!=nullptr,"map opens within authoring");QTimer::singleShot(0,map,[&,map]{check(map->findChild<QLabel*>("connectionsDetails")->text().contains("Draft followup"),"unsaved draft visible in map");map->reject();});});
  author.findChild<QPushButton*>("scriptConnections")->click();check(author.edits().size()==1,"map preserves existing draft");author.reject();
  auto creationDocs=documents;auto project=creationDocs[5].toObject();auto projectData=project.value("data").toObject();projectData["durationMonths"]=2;projectData["periodsPerMonth"]=4;projectData["initialState"]=QJsonObject{{"flags",QJsonObject{{"promise",false}}}};project["data"]=projectData;creationDocs[5]=project;
  QJsonObject flagDocument{{"formatVersion","0.1.0"},{"flags",QJsonArray{QJsonObject{{"id","promise"},{"label","Promise"},{"initial",false}}}}};
  creationDocs.append(QJsonObject{{"kind","flags"},{"file","flags.json"},{"data",flagDocument}});
  const QJsonObject settings{{"title","A promise remembered"},{"dialogue","You returned."},{"speakerId",QJsonValue::Null},{"fromPeriod",1},{"throughPeriod",7},{"promise",flag(true)}};
  const auto originalDocs=creationDocs;auto plan=makeStoryFollowup(creationDocs,"event:first/ask/yes",settings);
  check(plan.error.isEmpty()&&plan.documents.size()==creationDocs.size()+1,"create followup with detached documents");
  check(creationDocs==originalDocs,"creation leaves input untouched");
  auto created=plan.documents.last().toObject().value("data").toObject();auto conditions=created.value("requirements").toObject().value("conditions").toArray();
  check(conditions.size()==3&&conditions.contains(flag(true)),"completion plus chosen promise plus exact choice");
  check(created.value("window").toObject().value("fromPeriod")==1&&created.value("trigger")=="period-end","explicit window and period-end trigger");
  check(scriptGraphIssues(created).isEmpty(),"generated script links valid");
  auto changed=plan.documents[0].toObject().value("data").toObject();auto createdOptions=changed.value("nodes").toArray()[0].toObject().value("choices").toArray();
  check(createdOptions[1]==a.value("nodes").toArray()[0].toObject().value("choices").toArray()[1],"other choice untouched");
  const auto marker=createdOptions[0].toObject().value("effects").toArray().last().toObject().value("flagId").toString();
  check(!marker.isEmpty()&&marker!="promise","dedicated choice marker avoids shared-promise ambiguity");
  check(!plan.documents[5].toObject().value("data").toObject().value("initialState").toObject().value("flags").toObject().value(marker).toBool(true),"choice marker starts false");
  auto repeated=settings;repeated["targetFile"]=plan.file;auto relink=makeStoryFollowup(plan.documents,"event:first/ask/yes",repeated);
  check(relink.error.isEmpty()&&relink.documents==plan.documents,"repeated linking is idempotent");
  auto other=settings;other["targetFile"]="events/follow.json";auto linked=makeStoryFollowup(creationDocs,"event:first/ask/yes",other);
  check(linked.error.isEmpty()&&linked.documents.size()==creationDocs.size(),"link existing event without duplicate");
  check(linked.documents[1].toObject().value("data").toObject().value("nodes")==b.value("nodes"),"existing script preserved");
  other["targetFile"]="events/first.json";check(!makeStoryFollowup(creationDocs,"event:first/ask/yes",other).error.isEmpty(),"self dependency rejected");
  other["targetFile"]="events/first.json";other.remove("promise");check(!makeStoryFollowup(creationDocs,"event:follow",other).error.isEmpty(),"completion cycle rejected");
  auto invalid=settings;invalid["throughPeriod"]=8;check(!makeStoryFollowup(creationDocs,"event:first/ask/yes",invalid).error.isEmpty(),"out-of-project window rejected");
  invalid=settings;invalid["promise"]=flag(false);check(!makeStoryFollowup(creationDocs,"event:first/ask/yes",invalid).error.isEmpty(),"unrelated promise cannot be inferred");
  invalid=settings;invalid["title"]=" ";check(!makeStoryFollowup(creationDocs,"event:first/ask/yes",invalid).error.isEmpty(),"empty title rejected");
  auto fromEvent=settings;fromEvent.remove("promise");auto simple=makeStoryFollowup(creationDocs,"event:first",fromEvent);
  check(simple.error.isEmpty()&&simple.documents[0]==creationDocs[0]&&simple.documents[6]==creationDocs[6],"event-only link needs no choice marker");
  auto createdGraph=buildStoryConnections(plan.documents);bool exactEdge=false;for(auto e:createdGraph.edges)if(e.from=="event:first/ask/yes"&&e.to=="event:followup-1")exactEdge=true;check(exactEdge,"new choice connection immediately visible");
  ScriptAuthoringDialog creator(QJsonObject{{"documents",creationDocs}},{});creator.selectDocument("events/first.json");
  QTimer::singleShot(0,&creator,[&]{auto map=creator.findChild<QDialog*>("storyConnectionsDialog");check(map!=nullptr,"creation map opens");map->findChild<QComboBox*>("connectionsFocus")->setCurrentIndex(map->findChild<QComboBox*>("connectionsFocus")->findData("event:first/ask/yes"));
    QTimer::singleShot(0,map,[&,map]{auto form=map->findChild<QDialog*>("storyFollowupDialog");check(form!=nullptr,"followup form opens");form->findChild<QLineEdit*>("followupTitle")->setText("From the graph");form->findChild<QPlainTextEdit*>("followupDialogue")->setPlainText("You kept your word.");form->findChild<QComboBox*>("followupPromise")->setCurrentIndex(1);form->findChild<QDialogButtonBox*>("followupButtons")->button(QDialogButtonBox::Save)->click();});
    map->findChild<QPushButton*>("connectionsFollowup")->click();check(map->findChild<QLabel*>("connectionsDetails")->text().contains("From the graph"),"new event focused after draft mutation");map->reject();});
  creator.findChild<QPushButton*>("scriptConnections")->click();auto changes=creator.edits();check(changes.size()==5,"source flags project add and replacement in same batch");
  bool addBeforeReplace=false;for(auto v:changes){auto edit=v.toObject();if(edit.value("type")=="add")addBeforeReplace=true;if(edit.value("file")=="events/followup-1.json")check(addBeforeReplace,"new document added before replacement");}
  bool applied=false;creator.setApplyHandler([&](QJsonArray batch,std::function<void(QString)> done){applied=true;check(batch==changes,"save applies the complete draft atomically");done({});});
  creator.findChild<QDialogButtonBox*>("scriptButtons")->button(QDialogButtonBox::Save)->click();if(!applied)std::cerr<<creator.findChild<QLabel*>("scriptError")->text().toStdString()<<std::endl;check(applied&&creator.result()==QDialog::Accepted,"parent save accepts new event");
  auto large=documents;for(int i=0;i<700;++i)large.append(doc("event",script("large-"+QString::number(i),{{"op","eventCompleted"},{"eventId",i?"large-"+QString::number(i-1):"first"}})));
  QElapsedTimer timer;timer.start();auto big=buildStoryConnections(large);check(big.nodes.contains("event:large-699"),"large projects not truncated");check(big.edges.size()>=700,"large dependency chain complete");
  std::cout<<"PASS "<<checks<<" checks; 700-event graph "<<timer.elapsed()<<" ms\n";
}
