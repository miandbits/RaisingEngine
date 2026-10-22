#include "story-scope.hpp"
#include "script-authoring.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QPushButton>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTimer>
#include <iostream>

int main(int argc,char **argv){
  QApplication app(argc,argv);int checks=0;auto check=[&](bool pass,const char *message){++checks;if(!pass){std::cerr<<"FAIL: "<<message<<std::endl;std::exit(1);}};
  auto doc=[](QString kind,QString file,QJsonObject data){return QJsonObject{{"kind",kind},{"file",file},{"data",data}};};
  const auto parse=[](const char *json){return QJsonDocument::fromJson(json).object();};
  auto project=parse(R"({"roster":{"stateModel":"character-v1","localAttributeIds":["skill"]},"initialState":{"flags":{"promise":false}}})");
  auto first=parse(R"({"id":"first","label":"First meeting","trigger":"after-activity","activityId":"train","requirements":{"op":"compare","comparator":"gte","left":{"op":"read","target":{"kind":"attribute","attributeId":"skill"}},"right":{"op":"literal","value":0}},"nodes":[{"id":"choice","type":"choice","choices":[{"id":"yes","requirements":{"op":"not","condition":{"op":"flag","flagId":"promise","equals":true}},"effects":[{"op":"setFlag","flagId":"promise","value":true},{"op":"add","target":{"kind":"attribute","attributeId":"skill"},"value":{"op":"add","left":{"op":"literal","value":3},"right":{"op":"read","target":{"kind":"attribute","attributeId":"skill"}}}},{"op":"add","target":{"kind":"attribute","attributeId":"money"},"value":{"op":"literal","value":-2}},{"op":"add","target":{"kind":"relationship","characterId":"mentor","relationshipId":"trust"},"value":{"op":"literal","value":1}},{"op":"setState","stateGroupId":"mood","stateValueId":"happy"}]}]},{"id":"gift","type":"effect","effects":[{"op":"add","target":{"kind":"inventory","itemId":"bowl"},"value":{"op":"literal","value":1}}]}]})");
  auto next=parse(R"({"id":"next","label":"Promised bowl","trigger":"after-activity","activityId":"train","requirements":{"op":"all","conditions":[{"op":"eventCompleted","eventId":"first"},{"op":"flag","flagId":"promise","equals":true},{"op":"eventCompleted","eventId":"festival"}]},"nodes":[]})");
  auto last=parse(R"({"id":"last","trigger":"after-event","requirements":{"op":"eventCompleted","eventId":"next"},"nodes":[]})");
  QJsonArray docs{doc("project","project.json",project),doc("event","events/first.json",first),doc("event","events/next.json",next),doc("event","events/last.json",last)};
  auto get=[](const StoryScopePlan &p,int i){return p.documents[i].toObject().value("data").toObject();};
  const auto before=QJsonDocument(docs).toJson();auto plan=planPersonalStory(docs,"events/first.json");
  check(plan.errors.isEmpty(),"valid actor conversion");check(plan.eventIds==QStringList{"first","last","next"},"transitive followups included");check(QJsonDocument(docs).toJson()==before,"preview never mutates input");
  auto removed=first;removed["trigger"]="interaction";removed.remove("activityId");auto removedPlan=planPersonalStory(docs,"events/first.json",removed);bool removalShown=false;for(auto v:removedPlan.changes)if(v.toObject().value("path")=="/activityId"&&v.toObject().value("after")==QStringLiteral("—"))removalShown=true;check(removalShown&&!get(removedPlan,1).contains("activityId"),"removed draft fields are previewed");
  auto e=get(plan,1);auto effects=e.value("nodes").toArray()[0].toObject().value("choices").toArray()[0].toObject().value("effects").toArray();
  check(e.value("requirements").toObject().value("left").toObject().value("target").toObject().value("owner").toObject().value("kind")=="actor","event numeric condition");
  check(effects[0].toObject().value("owner").toObject().value("kind")=="actor","choice record");check(effects[1].toObject().value("target").toObject().value("owner").toObject().value("kind")=="actor","local growth effect");
  check(effects[1].toObject().value("value").toObject().value("right").toObject().value("target").toObject().value("owner").toObject().value("kind")=="actor","nested expression read");
  check(!effects[2].toObject().value("target").toObject().contains("owner"),"shared money retained");check(effects[3].toObject().value("target").toObject().contains("owner"),"relationship converted");check(effects[4].toObject().contains("owner"),"custom state converted");
  check(e.value("nodes").toArray()[1].toObject().value("effects").toArray()[0].toObject().value("target").toObject().contains("owner"),"linked automatic effect converted");
  auto conditions=get(plan,2).value("requirements").toObject().value("conditions").toArray();check(conditions[0].toObject().value("owner").toObject().value("kind")=="actor","followup completed scoped");check(conditions[1].toObject().contains("owner"),"followup flag scoped");check(!conditions[2].toObject().contains("owner"),"external prerequisite retained");
  check(get(plan,0)==project,"initial values and old save history not fabricated");auto again=planPersonalStory(plan.documents,"events/first.json");check(again.documents==plan.documents&&again.changes.isEmpty(),"idempotent conversion");
  auto keep=planPersonalStory(docs,"events/first.json",{},{{"flag:promise",false},{"attribute:money",true}});auto kept=get(keep,1).value("nodes").toArray()[0].toObject().value("choices").toArray()[0].toObject().value("effects").toArray();check(!kept[0].toObject().contains("owner"),"review option retains flag");check(!get(keep,2).value("requirements").toObject().value("conditions").toArray()[1].toObject().contains("owner"),"review option consistent across chain");check(!kept[2].toObject().value("target").toObject().contains("owner"),"cannot target unregistered personal attribute");
  auto unsupported=docs;auto bad=next;bad["trigger"]="period-end";unsupported[2]=doc("event","events/next.json",bad);check(!planPersonalStory(unsupported,"events/first.json").errors.isEmpty(),"unsupported followup stops whole conversion");
  auto legacy=docs;legacy[0]=doc("project","project.json",QJsonObject{});check(!planPersonalStory(legacy,"events/first.json").errors.isEmpty(),"legacy model not silently changed");check(!planPersonalStory(docs,"missing.json").errors.isEmpty(),"missing source blocked");
  auto fixed=first;auto fixedRequirement=parse(R"({"op":"flag","flagId":"promise","equals":true,"owner":{"kind":"character","characterId":"hana"}})");fixed["requirements"]=fixedRequirement;auto fixedPlan=planPersonalStory(docs,"events/first.json",fixed);check(get(fixedPlan,1).value("requirements")==fixedRequirement&&!fixedPlan.warnings.isEmpty(),"fixed owners preserved and reviewed");
  auto sharedWriter=docs;sharedWriter.append(doc("activity","activities/train.json",parse(R"({"effects":[{"op":"setFlag","flagId":"promise","value":true}]})")));check(!planPersonalStory(sharedWriter,"events/first.json").warnings.isEmpty(),"external writer warned");
  StoryScopeDialog dialog(docs,"events/first.json",first);check(dialog.findChild<QTreeWidget*>("scopeChanges")->topLevelItemCount()==3,"named preview groups");auto buttons=dialog.findChild<QDialogButtonBox*>("scopeButtons");check(buttons->button(QDialogButtonBox::Apply)->isEnabled(),"valid preview can apply");dialog.reject();check(QJsonDocument(docs).toJson()==before,"preview cancel unchanged");
  StoryScopeDialog warning(sharedWriter,"events/first.json",first);auto wb=warning.findChild<QDialogButtonBox*>("scopeButtons")->button(QDialogButtonBox::Apply);check(!wb->isEnabled(),"external impact requires review");warning.findChild<QCheckBox*>("scopeAcknowledge")->setChecked(true);check(wb->isEnabled(),"reviewed warnings can apply");
  StoryScopeDialog invalid(unsupported,"events/first.json",first);check(!invalid.findChild<QDialogButtonBox*>("scopeButtons")->button(QDialogButtonBox::Apply)->isEnabled(),"unsupported preview disabled");
  // Advanced script metadata uses the same review and retains dialog-level
  // cancellation: there is no remaining silent scope-only conversion here.
  auto scriptDocs=docs;for(int i=1;i<scriptDocs.size();++i){auto d=scriptDocs[i].toObject(),e=d.value("data").toObject();e["window"]=QJsonObject{{"fromPeriod",0},{"throughPeriod",1}};d["data"]=e;scriptDocs[i]=d;}
  ScriptAuthoringDialog script(QJsonObject{{"documents",scriptDocs}},{});script.selectDocument("events/first.json");auto scope=script.findChild<QComboBox*>("scriptScope");check(scope!=nullptr,"script scope selector exists");
  bool reviewed=false;QTimer::singleShot(0,&script,[&]{auto p=script.findChild<QDialog*>("storyScopePreview");check(p!=nullptr,"script conversion preview opened");reviewed=true;p->reject();});scope->setCurrentIndex(scope->findData("actor"));check(reviewed&&script.edits().isEmpty(),"script cancel leaves all drafts unchanged");
  QTimer::singleShot(0,&script,[&]{auto p=script.findChild<QDialog*>("storyScopePreview");check(p!=nullptr,"script conversion preview reopened");p->accept();});scope->setCurrentIndex(scope->findData("actor"));check(script.edits().size()==3,"script review converts whole chain in one batch");
  std::cout<<"story scope conversion: "<<checks<<" checks passed\n";
}
