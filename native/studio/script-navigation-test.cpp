#include "script-authoring.hpp"
#include "script-graph.hpp"
#include "script-identity.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QElapsedTimer>
#include <iostream>

int main(int argc,char **argv){
  QApplication app(argc,argv);int checks=0;
  auto check=[&](bool ok,const char *label){if(!ok){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);}++checks;};
  auto dialogue=[](QString id,QString next,QString text){return QJsonObject{{"id",id},{"type","dialogue"},{"speakerId","sage"},{"text",text},{"next",next}};};
  QJsonArray nodes;
  for(int i=0;i<600;++i)nodes.append(dialogue("line-"+QString::number(i),i==599?"end":"line-"+QString::number(i+1),"Dialogue "+QString::number(i)));
  nodes.append(QJsonObject{{"id","end"},{"type","end"}});
  auto makeEvent=[&](QString id,QString speaker,int from,QString before){
    auto n=nodes;auto first=n[0].toObject();first["speakerId"]=speaker;n[0]=first;
    return QJsonObject{{"kind","event"},{"file","events/"+id+".json"},{"data",QJsonObject{{"id",id},{"label","Same title"},{"window",QJsonObject{{"fromPeriod",from},{"throughPeriod",from+3}}},{"requirements",QJsonObject{{"op","eventCompleted"},{"eventId",before}}},{"entryNodeId","line-0"},{"nodes",n}}}};
  };
  const auto a=makeEvent("a","sage",0,"intro"),b=makeEvent("b","warrior",4,"oath");
  QJsonArray docs{a,b,QJsonObject{{"kind","event"},{"file","events/intro.json"},{"data",QJsonObject{{"id","intro"},{"label","First meeting"}}}},QJsonObject{{"kind","event"},{"file","events/oath.json"},{"data",QJsonObject{{"id","oath"},{"label","The oath"}}}}};
  QJsonArray contextDocs=docs;contextDocs.append(QJsonObject{{"kind","character"},{"data",QJsonObject{{"id","sage"},{"name","Doctor"}}}});contextDocs.append(QJsonObject{{"kind","character"},{"data",QJsonObject{{"id","warrior"},{"name","Sword master"}}}});
  const auto identity=scriptIdentity(b,docs,contextDocs);
  check(identity.contains("Sword master")&&identity.contains("5–8")&&identity.contains("The oath"),"identity uses character name, one-based period and predecessor label");
  check(identity!=scriptIdentity(a,docs,contextDocs),"duplicate labels have distinct contexts");
  QElapsedTimer clock;clock.start();
  auto linear=layoutScriptGraph(a.value("data").toObject());
  check(linear.rectangles.size()==601,"long graph has no first-500 truncation");
  check(linear.rectangles["line-599"].top()>linear.rectangles["line-0"].bottom(),"linear flow reads down");
  check(linear.rectangles["line-599"].left()==linear.rectangles["line-0"].left(),"linear flow does not drift horizontally");
  QJsonObject fork{{"entryNodeId","fork"},{"nodes",QJsonArray{QJsonObject{{"id","end"},{"type","end"}},dialogue("long-2","end","L2"),dialogue("short","end","S"),dialogue("long-1","long-2","L1"),QJsonObject{{"id","fork"},{"type","choice"},{"choices",QJsonArray{QJsonObject{{"text","Long"},{"next","long-1"}},QJsonObject{{"text","Short"},{"next","short"}}}}}}}};
  auto branched=layoutScriptGraph(fork);
  check(branched.rectangles["end"].top()>branched.rectangles["long-2"].bottom(),"merge placed after longest branch, even when serialized first");
  check(branched.rectangles["long-1"].right()<branched.rectangles["short"].left(),"choice siblings occupy distinct lanes");
  check(branched.order.first()=="fork","discovery begins at entry");
  auto cyclic=fork;auto invalid=cyclic["nodes"].toArray();invalid.append(dialogue("island","island","repair me"));cyclic["nodes"]=invalid;
  auto invalidLayout=layoutScriptGraph(cyclic);
  check(invalidLayout.rectangles.contains("island")&&invalidLayout.unreachable.contains("island"),"unreachable cycle remains visible and terminates");
  ScriptAuthoringDialog dialog({{"documents",contextDocs}},{});dialog.show();app.processEvents();
  const auto combo=dialog.findChild<QComboBox*>("scriptDocument");
  check(combo->itemText(0)==combo->itemText(1)&&combo->itemData(0,Qt::UserRole+4)!=combo->itemData(1,Qt::UserRole+4),"picker separates same labels without changing identifiers");
  dialog.findChild<QLineEdit*>("scriptSearch")->setText("Sword master");
  check(combo->currentIndex()==1,"event search accepts character context");
  dialog.findChild<QLineEdit*>("scriptSearch")->setText("The oath");
  check(combo->currentIndex()==1,"event search accepts predecessor context");
  auto search=dialog.findChild<QLineEdit*>("scriptNodeSearch");search->setText("Dialogue 599");app.processEvents();
  const auto list=dialog.findChild<QListWidget*>("scriptNodes");
  check(list->currentItem()->data(Qt::UserRole)=="line-599","long script search selects node after 500");
  auto view=dialog.findChild<QGraphicsView*>("scriptGraph");
  bool present=false;QRectF rect;for(auto item:view->scene()->items())if(item->data(0)=="line-599"){present=true;rect=item->sceneBoundingRect();}
  check(present,"node after 500 rendered");
  check(view->mapToScene(view->viewport()->rect()).boundingRect().intersects(rect),"search reveals actual selected card");
  const auto center=view->mapToScene(view->viewport()->rect().center());const auto scale=view->transform().m11();
  dialog.findChild<QPlainTextEdit*>("script-text")->setPlainText("Edited last narration");app.processEvents();
  check(qAbs(view->transform().m11()-scale)<.001&&QLineF(center,view->mapToScene(view->viewport()->rect().center())).length()<3,"typing retains zoom and viewport");
  check(dialog.edits().size()==1&&dialog.edits()[0].toObject().value("file")=="events/b.json","same-label edit affects only chosen document");
  check(dialog.edits()[0].toObject().value("data").toObject().value("id")=="b","event id preserved");
  dialog.findChild<QPushButton*>("scriptGraphFit")->click();check(view->transform().m11()<.1,"full graph overview available");
  dialog.findChild<QPushButton*>("scriptGraphFocus")->click();check(qAbs(view->transform().m11()-1.)<.001,"readable 100 percent restored");
  check(dialog.findChild<QGraphicsView*>("scriptGraphOverview")!=nullptr,"clickable overview present");
  dialog.findChild<QSpinBox*>("scriptFromPeriod")->setValue(6);check(combo->itemData(1,Qt::UserRole+4).toString().contains("6–8"),"identity updates after metadata edit");
  dialog.findChild<QLineEdit*>("scriptLabel")->setText("Renamed continuation");
  check(dialog.findChild<QLabel*>("scriptPredecessorSummary")->text().endsWith("Renamed continuation"),"predecessor summary follows a live rename without reloading fields");
  dialog.findChild<QPushButton*>("scriptConnectPredecessor")->click();auto picker=dialog.findChild<QDialog*>("storyPredecessorDialog");
  picker->findChild<QLineEdit*>("predecessorSearch")->setText("First meeting");
  auto candidates=picker->findChild<QListWidget*>("predecessorEvents");int found=-1;for(int i=0;i<candidates->count();++i)if(candidates->item(i)->data(Qt::UserRole)=="a")found=i;
  check(found>=0&&!candidates->item(found)->isHidden()&&candidates->item(found)->text().contains("Doctor"),"predecessor picker searches and displays distinguishing context");
  picker->reject();
  QJsonObject choiceScript{{"id","choices"},{"label","Choice search"},{"entryNodeId","choose"},{"nodes",QJsonArray{
    QJsonObject{{"id","choose"},{"type","choice"},{"prompt","What now?"},{"choices",QJsonArray{
      QJsonObject{{"id","yes"},{"text","Accept the invitation and study together"},{"next","end"}},
      QJsonObject{{"id","no"},{"text","Decline the invitation politely"},{"next","end"}}}}},
    QJsonObject{{"id","end"},{"type","end"}}}}};
  ScriptAuthoringDialog choiceDialog({{"documents",QJsonArray{QJsonObject{{"kind","event"},{"file","events/choices.json"},{"data",choiceScript}}}}},{});
  choiceDialog.findChild<QLineEdit*>("scriptNodeSearch")->setText("invitation");
  check(!choiceDialog.findChild<QListWidget*>("scriptNodes")->item(0)->isHidden(),"choice search includes option text, not only the prompt");
  int captions=0;QString captionTooltip;
  for(auto item:choiceDialog.findChild<QGraphicsView*>("scriptGraph")->scene()->items())if(item->data(2)=="edge-caption"){++captions;captionTooltip=item->toolTip();}
  check(captions==1&&captionTooltip.contains("and study together")&&captionTooltip.contains("politely"),"converging choices share one non-overlapping caption with complete option tooltips");
  std::cout<<"PASS "<<checks<<" checks; 601-node interactive fixture "<<clock.elapsed()<<" ms\n";
}
