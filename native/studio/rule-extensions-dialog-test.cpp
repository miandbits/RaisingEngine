#include "rule-extensions.hpp"
#include "rule-package-authoring-test.hpp"
#include "rule-package-editor.hpp"
#include "logic-editor.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QFontDatabase>
#include <iostream>

int main(int argc,char **argv) {
  QApplication app(argc,argv);
  const auto fontId=QFontDatabase::addApplicationFont("C:/Windows/Fonts/malgun.ttf");
  if(fontId>=0)app.setFont(QFont(QFontDatabase::applicationFontFamilies(fontId).first(),10));
  if(qgetenv("QT_QPA_PLATFORM")!="offscreen"||argc!=4){std::cerr<<"Requires offscreen, base project, package, output directory\n";return 2;}
  int checks=0;auto require=[&](bool ok,const char *message){++checks;if(!ok){std::cerr<<message<<'\n';std::exit(1);}};
  auto read=[&](QString path){QFile f(path);require(f.open(QIODevice::ReadOnly),"read");return QJsonDocument::fromJson(f.readAll()).object();};
  auto project=read(QString::fromLocal8Bit(argv[1])+"/project.json");
  QJsonArray documents;
  documents.append(QJsonObject{{"kind","project"},{"data",project}});
  documents.append(QJsonObject{{"kind","attributes"},{"data",read(QString::fromLocal8Bit(argv[1])+"/attributes.json")}});
  QJsonObject context{{"documents",documents}};
  const bool workshop=qgetenv("RULE_WORKSHOP")=="1";
  QDir().mkpath(QString::fromLocal8Bit(argv[3]));
  if(workshop)authorRulePackage(context,QString::fromLocal8Bit(argv[3]));
  RuleExtensionDialog dialog(project,context,"schedule");dialog.show();app.processEvents();
  dialog.findChild<QLineEdit*>("rulePackagePath")->setText(workshop?QString::fromLocal8Bit(argv[3])+"/authored.raiserules":QString::fromLocal8Bit(argv[2]));dialog.findChild<QPushButton*>("importRulePackage")->click();
  require(dialog.value().value("ruleExtensions").toObject().value("packages").toArray().size()==1,"package imported via form");
  dialog.findChild<QPushButton*>("importRulePackage")->click();require(dialog.value().value("ruleExtensions").toObject().value("packages").toArray().size()==1,"duplicate import unchanged");
  dialog.findChild<QPushButton*>("addRuleProgression")->click();app.processEvents();
  auto gain=dialog.findChild<QSpinBox*>(workshop?"ruleArg-param-2":"ruleArg-gain");require(gain!=nullptr,"typed parameter form");gain->setValue(12);
  require(dialog.findChild<QComboBox*>(workshop?"ruleArg-param-1":"ruleArg-stat")->currentText()==QStringLiteral("검술"),"attribute is selected by designer label");
  dialog.findChild<QLineEdit*>("ruleProgressionName")->setText(QStringLiteral("막내의 돌파 수련"));
  dialog.findChild<QPushButton*>("placeRuleActions")->click();dialog.findChild<QPushButton*>("placeRuleActions")->click();
  auto lib=dialog.value().value("ruleExtensions").toObject();require(lib.value("progressions").toArray()[0].toObject().value("args").toObject().value(workshop?"param-2":"gain")==12,"edited args persist");
  QDir().mkpath(QString::fromLocal8Bit(argv[3]));require(dialog.grab().save(QString::fromLocal8Bit(argv[3])+"/editor.png"),"editor screenshot");
  dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();require(dialog.result()==QDialog::Accepted,"editor accepts draft");project=dialog.value();
  auto upgradedProject=project;auto upgradedLib=upgradedProject.value("ruleExtensions").toObject();auto upgradedPackages=upgradedLib.value("packages").toArray();
  RulePackageEditor upgradeEditor(upgradedPackages[0].toObject(),context);upgradeEditor.findChild<QLineEdit*>("package-version")->setText("1.1.0");upgradedPackages[0]=upgradeEditor.value();upgradedLib["packages"]=upgradedPackages;upgradedProject["ruleExtensions"]=upgradedLib;
  RuleExtensionDialog upgradeDialog(upgradedProject,context,"schedule");
  require(upgradeDialog.findChild<QSpinBox*>(workshop?"ruleArg-param-2":"ruleArg-gain")==nullptr,"old pinned progression never uses new definition silently");
  upgradeDialog.findChild<QPushButton*>("reconnectRuleProgression")->click();
  require(upgradeDialog.value().value("ruleExtensions").toObject().value("progressions").toArray()[0].toObject().value("version")=="1.1.0","explicit progression reconnect updates version");
  require(project.value("ruleExtensions").toObject().value("progressions").toArray()[0].toObject().value("version")=="1.0.0","reconnect is a detached draft");
  documents=context.value("documents").toArray();documents[0]=QJsonObject{{"kind","project"},{"data",project}};context["documents"]=documents;
  LogicEditor condition(QJsonObject{{"op","always"}},"condition",context);condition.show();app.processEvents();
  auto type=condition.findChild<QComboBox*>("logicType");int index=type->findData("ruleCondition");require(index>=0,"extension condition exposed");type->setCurrentIndex(index);QMetaObject::invokeMethod(type,"activated",Q_ARG(int,index));app.processEvents();
  require(condition.findChild<QSpinBox*>(workshop?"ruleArg-param-2":"ruleArg-minimum")!=nullptr,"condition parameter form");condition.findChild<QSpinBox*>(workshop?"ruleArg-param-2":"ruleArg-minimum")->setValue(12);
  condition.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();require(condition.result()==QDialog::Accepted,"condition saved");
  LogicEditor reopen(condition.value(),"condition",context);require(reopen.findChild<QSpinBox*>(workshop?"ruleArg-param-2":"ruleArg-minimum")->value()==12,"condition restore");
  auto oldCall=condition.value().toObject();oldCall["version"]="0.9.0";LogicEditor oldVersion(oldCall,"condition",context);
  auto definition=oldVersion.findChild<QComboBox*>("ruleDefinition");definition->setCurrentIndex(0);QMetaObject::invokeMethod(definition,"activated",Q_ARG(int,0));app.processEvents();
  require(oldVersion.value().toObject().value("version")=="1.0.0","explicitly reselect installed version");
  require(oldVersion.findChild<QSpinBox*>(workshop?"ruleArg-param-2":"ruleArg-minimum")!=nullptr,"version replacement refreshes argument fields");
  LogicEditor stateCondition(QJsonObject{{"op","stateIs"},{"stateGroupId","rx-flow-1"},{"stateValueId",workshop?"state-3":"mastered"}},"condition",context);
  require(stateCondition.findChild<QComboBox*>("logic-stateGroupId")->currentText()==QStringLiteral("막내의 돌파 수련"),"generated progression available by name in standard conditions");
  stateCondition.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();require(stateCondition.result()==QDialog::Accepted,"generated state condition accepted");
  QJsonObject defaultEffect=defaultRuleCall(ruleDefinitions(project,"effects").first().toObject(),"ruleEffect");
  LogicEditor effects(QJsonArray{defaultEffect},"effects",context);effects.show();app.processEvents();
  auto tree=effects.findChild<QTreeWidget*>("logicTree");tree->setCurrentItem(tree->topLevelItem(0)->child(0));app.processEvents();
  auto amount=effects.findChild<QSpinBox*>(workshop?"ruleArg-param-2":"ruleArg-gain");require(amount!=nullptr,"effect parameter form");amount->setValue(4);
  effects.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();require(effects.result()==QDialog::Accepted,"effect saved");
  auto gameplay=project.value("gameplay").toObject();auto actions=gameplay.value("actions").toArray();actions.append(QJsonObject{{"id","celebrate"},{"name","돌파의 깨달음"},{"costs",QJsonArray{}},{"requirements",condition.value()},{"effects",effects.value()}});gameplay["actions"]=actions;project["gameplay"]=gameplay;
  auto visual=project.value("visual").toObject();auto screens=visual.value("screens").toArray();
  for(int i=0;i<screens.size();i++){auto screen=screens[i].toObject();if(screen.value("id")!="schedule")continue;auto elements=screen.value("elements").toArray();elements.append(QJsonObject{{"id","celebrate"},{"kind","button"},{"text","돌파의 깨달음"},{"action","executeAction"},{"actionId","celebrate"},{"x",780},{"y",550},{"width",240},{"height",50},{"fill","#375a63"},{"color","#ffffff"}});screen["elements"]=elements;screens[i]=screen;}
  visual["screens"]=screens;project["visual"]=visual;
  // Return authored data to the shared StudioEditor service; the test does not fake a runtime state.
  QFile output(QString::fromLocal8Bit(argv[3])+"/authored-project.json");require(output.open(QIODevice::WriteOnly),"output");output.write(QJsonDocument(project).toJson());
  std::cout<<"{\"ok\":true,\"checks\":"<<checks<<"}\n";
}
