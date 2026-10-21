#include "script-authoring.hpp"
#include "script-continuation.hpp"
#include "project-environment.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <iostream>

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  int checks=0; auto check=[&](bool ok,const char *label){if(!ok){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);}++checks;};
  QJsonObject guard{{"op","flag"},{"flagId","promised"},{"equals",true}};
  const auto linked=afterEvent(guard,"meeting");
  check(linked.value("conditions").toArray().first()==guard,"preserve activity and choice guards");
  check(afterEvent(linked,"meeting")==linked,"idempotent prerequisite");
  auto event=[](QString id,QString label,QJsonObject requirements){return QJsonObject{{"kind","event"},{"file","events/"+id+".json"},{"data",QJsonObject{{"formatVersion","0.1.0"},{"id",id},{"label",label},{"requirements",requirements},{"window",QJsonObject{{"fromPeriod",0},{"throughPeriod",7}}},{"priority",70},{"entryNodeId","line"},{"nodes",QJsonArray{QJsonObject{{"id","line"},{"type","dialogue"},{"speakerId",QJsonValue::Null},{"text","Original line"},{"next","reward"}},QJsonObject{{"id","reward"},{"type","effect"},{"effects",QJsonArray{QJsonObject{{"op","setFlag"},{"flagId","rewarded"},{"value",true}}}},{"next","end"}},QJsonObject{{"id","end"},{"type","end"}}}}}}};};
  QJsonArray docs{event("meeting","First meeting",{{"op","always"}}),event("after","Remembered promise",guard)};
  check(continuationCycle(docs,"after","after"),"self completion rejected");
  auto cyclic=docs;auto first=cyclic[0].toObject();auto firstData=first.value("data").toObject();firstData["requirements"]=afterEvent({{"op","always"}},"after");first["data"]=firstData;cyclic[0]=first;
  check(continuationCycle(cyclic,"after","meeting"),"mutual completion rejected");
  bool stale=false;
  ScriptAuthoringDialog dialog({{"documents",docs}}, {}, nullptr,[&]{return stale?QString("stale draft"):QString();});
  dialog.findChild<QLineEdit*>("scriptSearch")->setText("Remembered");
  check(dialog.findChild<QComboBox*>("scriptDocument")->currentIndex()==1,"search selects existing event");
  dialog.findChild<QPushButton*>("scriptConnectPredecessor")->click();
  auto predecessor=dialog.findChild<QDialog*>("storyPredecessorDialog");check(predecessor!=nullptr,"named dependency dialog opens");
  auto list=predecessor->findChild<QListWidget*>("predecessorEvents");check(list->count()==1,"self event excluded");list->setCurrentRow(0);
  predecessor->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
  dialog.findChild<QPushButton*>("scriptInsertAfter")->click();
  auto text=dialog.findChild<QPlainTextEdit*>("script-text");check(text!=nullptr,"inserted dialogue selected");text->setPlainText("The promise has a consequence.");
  const auto edits=dialog.edits();check(edits.size()==1,"one ordinary document transaction");
  const auto result=edits.first().toObject().value("data").toObject();
  check(result.value("requirements")==linked,"named prerequisite preserves existing condition");
  const auto nodes=result.value("nodes").toArray();
  check(nodes.size()==4&&nodes[0].toObject().value("next")=="dialogue-1","previous line connects to inserted dialogue");
  check(nodes[3].toObject().value("next")=="reward","inserted dialogue preserves reward continuation");
  check(nodes[1]==docs[1].toObject().value("data").toObject().value("nodes").toArray()[1],"effect byte structure preserved");
  check(scriptGraphIssues(result).isEmpty(),"result graph remains valid");
  dialog.findChild<QLineEdit*>("scriptSearch")->setText("First");dialog.findChild<QLineEdit*>("scriptSearch")->setText("Remembered");
  check(dialog.edits()==edits,"search never discards edited draft");
  stale=true;dialog.findChild<QDialogButtonBox*>("scriptButtons")->button(QDialogButtonBox::Save)->click();
  check(dialog.findChild<QLabel*>("scriptError")->text()=="stale draft","stale edits stay open");
  stale=false;
  std::function<void(QString)> finish;
  dialog.setApplyHandler([&](QJsonArray submitted,std::function<void(QString)> done){check(submitted==edits,"shared service receives the complete draft");finish=std::move(done);});
  dialog.findChild<QDialogButtonBox*>("scriptButtons")->button(QDialogButtonBox::Save)->click();
  check(!dialog.isEnabled(),"pending apply blocks changes that would be lost");
  finish("service unavailable");
  check(dialog.isEnabled()&&dialog.edits()==edits&&dialog.findChild<QLabel*>("scriptError")->text()=="service unavailable","failed apply preserves the editable draft");
  dialog.findChild<QDialogButtonBox*>("scriptButtons")->button(QDialogButtonBox::Save)->click();
  finish({});check(dialog.result()==QDialog::Accepted,"successful retry accepts the same draft");
  QTemporaryDir root;check(projectPluginStoreKey(root.path())==projectPluginStoreKey(root.path()+"/./"),"canonical project setting key");
  std::cout<<"PASS "<<checks<<" continuation checks\n";
}
