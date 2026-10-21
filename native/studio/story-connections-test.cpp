#include "story-connections.hpp"
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
  a["entryNodeId"]="ask";a["nodes"]=QJsonArray{QJsonObject{{"id","ask"},{"type","choice"},{"choices",QJsonArray{
    QJsonObject{{"id","yes"},{"text","Keep the promise"},{"effects",QJsonArray{set(true)}},{"requirements",QJsonObject{{"op","always"}}},{"next","end"}},
    QJsonObject{{"id","no"},{"text","Decline"},{"effects",QJsonArray{set(false)}},{"requirements",QJsonObject{{"op","always"}}},{"next","end"}}}}},
    QJsonObject{{"id","end"},{"type","end"}},QJsonObject{{"id","orphan"},{"type","effect"},{"effects",QJsonArray{QJsonObject{{"op","setFlag"},{"flagId","unreachable"},{"value",true}}}}}};
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
  auto large=documents;for(int i=0;i<700;++i)large.append(doc("event",script("large-"+QString::number(i),{{"op","eventCompleted"},{"eventId",i?"large-"+QString::number(i-1):"first"}})));
  QElapsedTimer timer;timer.start();auto big=buildStoryConnections(large);check(big.nodes.contains("event:large-699"),"large projects not truncated");check(big.edges.size()>=700,"large dependency chain complete");
  std::cout<<"PASS "<<checks<<" checks; 700-event graph "<<timer.elapsed()<<" ms\n";
}
