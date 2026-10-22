#include "story-flow.hpp"
#include <QtWidgets>
#include <iostream>

int main(int argc,char **argv) {
  QApplication app(argc,argv);
  auto document=[](QString kind,QString file,QJsonObject data){return QJsonObject{{"kind",kind},{"file",file},{"data",data}};};
  QJsonObject project{{"durationMonths",1},{"periodsPerMonth",3},{"protagonistId","hero"},{"initialState",QJsonObject{{"flags",QJsonObject{}}}}};
  QJsonObject context{{"documents",QJsonArray{
    document("project","project.json",project),document("flags","flags.json",{{"formatVersion","0.1.0"},{"flags",QJsonArray{}}}),
    document("character","characters/hero.json",{{"id","hero"},{"name","연"},{"portraits",QJsonObject{}},{"relationships",QJsonArray{}}}),
    document("attributes","attributes.json",{{"attributes",QJsonArray{QJsonObject{{"id","skill"},{"label","기교"}}}}})}}};
  int checks=0;auto check=[&](bool ok,const char *label){if(!ok){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);}++checks;};
  bool stale=false,applied=false;QJsonArray received;
  StoryFlowDialog dialog(context,nullptr,[&]{return stale?QString("revision changed"):QString();},[&](QJsonArray edits,std::function<void(QString)> done){applied=true;received=edits;done({});});
  auto line=[&](const char *name,QString value){auto w=dialog.findChild<QLineEdit*>(name);check(w!=nullptr,"field exists");w->setText(value);};
  auto multiline=[&](const char *name,QString value){auto w=dialog.findChild<QPlainTextEdit*>(name);check(w!=nullptr,"multiline exists");w->setPlainText(value);};
  auto spin=[&](const char *name,int value){auto w=dialog.findChild<QSpinBox*>(name);check(w!=nullptr,"number exists");w->setValue(value);};
  auto dates=[&](const char *name,int value){auto w=dialog.findChild<QComboBox*>(name);check(w!=nullptr,"calendar exists");w->setCurrentIndex(w->findData(value));};
  auto choices=dialog.findChild<QListWidget*>("storyChoices");
  check(dialog.findChild<QComboBox*>("storyFrom")->itemText(0)=="1년 1월 · 1번째 일정","calendar labels ordinal without changing stored value");
  check(dialog.findChild<QComboBox*>("storyMeetingTrigger")->currentData()=="after-activity","new meetings follow the assigned activity");
  check(choices->count()==2,"new story starts with two visible choices");
  for(int i=0;i<2;++i)check(choices->item(i)->text().startsWith(QString::number(i+1)+". ")&&choices->item(i)->text().contains("미작성"),"empty choices have numbered placeholders");
  dialog.findChild<QPushButton*>("storyChoiceAdd")->click();
  check(choices->count()==3&&choices->item(2)->text().startsWith("3. "),"new choice has a visible numbered placeholder");
  choices->setCurrentRow(1);
  dialog.findChild<QPushButton*>("storyChoiceRemove")->click();
  check(choices->count()==2&&choices->item(1)->text().startsWith("2. "),"removal renumbers remaining choices");
  line("storyName","마음의 약초");line("storyCharacterName","소하");line("storyActivityName","약초 손질");line("storyEncounterTitle","약초밭에서");multiline("storyEncounterText","같이 일할래?");
  line("storyChoiceText","함께 돕는다");multiline("storyChoiceReply","고마워");spin("storyChoiceDelta",3);
  dialog.findChild<QCheckBox*>("storyFollowupEnabled")->setChecked(true);line("storyFollowupTitle","약속의 답례");multiline("storyFollowupText","이건 네 몫이야");spin("storyFollowupMinimum",3);dates("storyFollowupFrom",2);dates("storyFollowupThrough",3);
  choices->setCurrentRow(1);line("storyChoiceText","오늘은 쉰다");multiline("storyChoiceReply","다음에 만나자");spin("storyChoiceDelta",-1);
  choices->setCurrentRow(0);
  check(dialog.findChild<QLineEdit*>("storyChoiceText")->text()=="함께 돕는다","branch text round-trip");
  check(dialog.findChild<QPlainTextEdit*>("storyFollowupText")->toPlainText()=="이건 네 몫이야","follow-up draft survives branch switching");
  check(dialog.findChild<QSpinBox*>("storyChoiceDelta")->value()==3,"branch relation round-trip");
  choices->selectionModel()->select(choices->model()->index(1,0),QItemSelectionModel::ClearAndSelect);
  check(dialog.findChild<QLineEdit*>("storyChoiceText")->text()=="오늘은 쉰다","accessible selection changes branch form");
  choices->selectionModel()->select(choices->model()->index(0,0),QItemSelectionModel::ClearAndSelect);
  check(dialog.findChild<QSpinBox*>("storyChoiceDelta")->value()==3,"accessible selection preserves other branch");
  auto steps=dialog.findChild<QListWidget*>("storyFlowSteps");
  steps->selectionModel()->select(steps->model()->index(2,0),QItemSelectionModel::ClearAndSelect);
  check(dialog.findChild<QStackedWidget*>("storyFlowPages")->currentIndex()==2,"accessible selection changes authoring page");
  dialog.findChild<QListWidget*>("storyFlowSteps")->setCurrentRow(4);
  check(dialog.findChild<QLabel*>("storyFlowError")->text().isEmpty(),"valid draft preview");
  const auto tree=dialog.findChild<QTreeWidget*>("storyFlowPreview");
  check(tree->topLevelItem(0)->child(0)->child(0)->childCount()==2,"two visible branches");
  stale=true;dialog.findChild<QPushButton*>("storyApply")->click();check(!applied,"stale revision cannot apply");
  check(dialog.findChild<QLabel*>("storyFlowError")->text()=="revision changed","conflict visible without closing draft");
  stale=false;dialog.findChild<QPushButton*>("storyApply")->click();check(applied&&!received.empty(),"valid draft applies a batch");
  QJsonObject saved;for(auto v:received)if(v.toObject().value("file")=="project.json")saved=v.toObject().value("data").toObject();
  auto flow=saved.value("authoring").toObject().value("storyFlows").toArray().first().toObject();
  check(flow.value("choices").toArray()[0].toObject().value("delta")==3,"saved positive effect");
  check(flow.value("choices").toArray()[1].toObject().value("delta")==-1,"saved negative effect");
  check(flow.value("meetingTrigger")=="after-activity","saved meeting occurrence");
  check(flow.value("choices").toArray()[0].toObject().value("followupThrough")==3,"saved bounded follow-up date");
  QJsonObject legacy=flow;legacy.remove("meetingTrigger");auto oldRows=legacy.value("choices").toArray();auto oldRow=oldRows[0].toObject();oldRow.remove("followupTrigger");oldRow.remove("followupThrough");oldRows[0]=oldRow;legacy["choices"]=oldRows;
  auto oldContext=context;auto oldDocs=oldContext.value("documents").toArray();auto oldProject=oldDocs[0].toObject();auto oldData=oldProject.value("data").toObject();oldData["authoring"]=QJsonObject{{"storyFlows",QJsonArray{legacy}}};oldData["startYear"]=4;oldData["startMonth"]=12;oldData["durationMonths"]=2;oldProject["data"]=oldData;oldDocs[0]=oldProject;oldContext["documents"]=oldDocs;
  StoryFlowDialog reopened(oldContext,nullptr,[]{return QString();},[](QJsonArray,std::function<void(QString)> done){done({});});
  reopened.findChild<QComboBox*>("storyFlowSelector")->setCurrentIndex(1);
  check(reopened.findChild<QComboBox*>("storyMeetingTrigger")->currentData()=="period-end","legacy meeting timing is unchanged");
  check(reopened.findChild<QComboBox*>("storyFollowupTrigger")->currentData()=="period-end","legacy follow-up timing is unchanged");
  check(reopened.findChild<QComboBox*>("storyFollowupThrough")->currentData()==6,"legacy unbounded follow-up keeps project end");
  check(reopened.findChild<QComboBox*>("storyFrom")->itemText(3)=="5년 1월 · 1번째 일정","calendar wraps year");
  std::cout<<"PASS "<<checks<<" dialog checks\n";return 0;
}
