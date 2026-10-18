#include "rule-package-authoring-test.hpp"
#include "rule-package-editor.hpp"
#include "logic-editor.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QJsonDocument>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <iostream>

namespace {
void check(bool ok,const char *message){if(!ok){std::cerr<<"workshop: "<<message<<'\n';std::exit(1);}}
template<class T> T *widget(QWidget *parent,const char *id){auto w=parent->findChild<T*>(id);check(w!=nullptr,id);return w;}
void click(QWidget *w,const char *id){widget<QPushButton>(w,id)->click();QApplication::processEvents();}
void select(QComboBox *combo,QString value){int i=combo->findData(value);if(i<0)std::cerr<<"Missing "<<combo->objectName().toStdString()<<" value "<<value.toStdString()<<" count "<<combo->count()<<std::endl;check(i>=0,"combo value");combo->setCurrentIndex(i);QMetaObject::invokeMethod(combo,"activated",Q_ARG(int,i));QApplication::processEvents();}
QTreeWidgetItem *find(QTreeWidgetItem *item,const QStringList &path){if(item->data(0,Qt::UserRole).toStringList()==path)return item;for(int i=0;i<item->childCount();++i)if(auto found=find(item->child(i),path))return found;return nullptr;}
void selectPath(QWidget *owner,const char *id,QStringList path){auto tree=widget<QTreeWidget>(owner,id);QTreeWidgetItem *item=nullptr;for(int i=0;i<tree->topLevelItemCount()&&!item;++i)item=find(tree->topLevelItem(i),path);check(item!=nullptr,"tree path");tree->setCurrentItem(item);QApplication::processEvents();}
void save(QDialog *dialog){widget<QDialogButtonBox>(dialog,"")->button(QDialogButtonBox::Save)->click();check(dialog->result()==QDialog::Accepted,"dialog accepts");}
void number(QWidget *owner,int amount){auto spin=widget<QSpinBox>(owner,"logic-value");spin->setValue(amount);QMetaObject::invokeMethod(spin,"editingFinished");QApplication::processEvents();}
void body(RulePackageEditor &editor,const char *button,const std::function<void(LogicEditor*)> &edit){QTimer::singleShot(0,[edit]{auto dialog=dynamic_cast<LogicEditor*>(QApplication::activeModalWidget());check(dialog!=nullptr,"logic modal");edit(dialog);save(dialog);});click(&editor,button);}
void bind(RulePackageEditor &editor,QStringList path,QString name){QTimer::singleShot(0,[path,name]{auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());check(dialog&&dialog->objectName()=="ruleBindings","binding modal");selectPath(dialog,"bindingValues",path);widget<QLineEdit>(dialog,"bindingName")->setText(name);click(dialog,"bindingCreate");save(dialog);});click(&editor,"package-bindings");}
void setName(QWidget &editor,QString text){widget<QLineEdit>(&editor,"package-item-name")->setText(text);}
}

QJsonObject authorRulePackage(const QJsonObject &context,const QString &output) {
  RulePackageEditor editor({},context);editor.show();QApplication::processEvents();
  widget<QLineEdit>(&editor,"package-name")->setText(QStringLiteral("직접 만든 무공 규칙"));
  click(&editor,"add-package-conditions");setName(editor,QStringLiteral("능력이 기준 이상"));
  body(editor,"package-body",[](LogicEditor *dialog){
    select(widget<QComboBox>(dialog,"logicType"),"compare");select(widget<QComboBox>(dialog,"logic-comparator"),"gte");
    selectPath(dialog,"logicTree",{"left"});select(widget<QComboBox>(dialog,"logicType"),"read");selectPath(dialog,"logicTree",{"left","target"});select(widget<QComboBox>(dialog,"logic-attributeId"),"skill");
    selectPath(dialog,"logicTree",{"right"});number(dialog,10);
  });
  bind(editor,{"body","left","target","attributeId"},QStringLiteral("검사할 능력"));bind(editor,{"body","right","value"},QStringLiteral("필요 수치"));
  click(&editor,"add-package-effects");setName(editor,QStringLiteral("수련과 피로"));
  body(editor,"package-body",[](LogicEditor *dialog){
    click(dialog,"logicAdd");selectPath(dialog,"logicTree",{"0","target"});select(widget<QComboBox>(dialog,"logic-attributeId"),"skill");selectPath(dialog,"logicTree",{"0","value"});number(dialog,12);
    selectPath(dialog,"logicTree",{});click(dialog,"logicAdd");selectPath(dialog,"logicTree",{"1","target"});select(widget<QComboBox>(dialog,"logic-attributeId"),"stress");selectPath(dialog,"logicTree",{"1","value"});number(dialog,2);
  });
  bind(editor,{"body","0","target","attributeId"},QStringLiteral("성장 능력"));bind(editor,{"body","0","value","value"},QStringLiteral("성장량"));
  QTimer::singleShot(0,[]{auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());selectPath(dialog,"bindingValues",{"body","0","value","value"});QTimer::singleShot(0,[]{auto settings=qobject_cast<QDialog*>(QApplication::activeModalWidget());check(settings&&settings->objectName()=="ruleParameterSettings","parameter settings modal");widget<QSpinBox>(settings,"parameterMin")->setValue(20);widget<QDialogButtonBox>(settings,"")->button(QDialogButtonBox::Save)->click();check(settings->result()!=QDialog::Accepted,"default outside range rejected");widget<QSpinBox>(settings,"parameterMin")->setValue(1);widget<QSpinBox>(settings,"parameterMax")->setValue(50);save(settings);});click(dialog,"bindingSettings");save(dialog);});click(&editor,"package-bindings");
  check(editor.value().value("effects").toArray()[0].toObject().value("parameters").toObject().value("param-2").toObject().value("min")==1,"parameter range saved");
  // Editing a concrete body without changing it must retain all template bindings.
  const auto prior=editor.value().value("effects");body(editor,"package-body",[](LogicEditor*){});check(editor.value().value("effects")==prior,"template reopen preserves binding");
  RulePackageEditor changed(editor.value(),context);selectPath(&changed,"packageTree",{"effects","0"});
  body(changed,"package-body",[](LogicEditor *dialog){selectPath(dialog,"logicTree",{"0","value"});number(dialog,13);});
  const auto changedEffect=changed.value().value("effects").toArray()[0].toObject().value("body").toArray()[0].toObject();
  check(changedEffect.value("target").toObject().value("attributeId").isObject(),"editing gain retains independent target binding");
  check(changedEffect.value("value").toObject().value("value")==13,"edited scalar becomes fixed value");
  RulePackageEditor reordered(editor.value(),context);selectPath(&reordered,"packageTree",{"effects","0"});
  body(reordered,"package-body",[](LogicEditor *dialog){selectPath(dialog,"logicTree",{"0"});click(dialog,"logicDown");});
  check(reordered.value().value("effects").toArray()[0].toObject().value("body").toArray()[1]==prior.toArray()[0].toObject().value("body").toArray()[0],"moving effect retains its bindings");
  click(&editor,"add-package-progressions");setName(editor,QStringLiteral("준비 → 수련 → 돌파"));
  click(&editor,"package-add-states");setName(editor,QStringLiteral("수련"));selectPath(&editor,"packageTree",{"progressions","0"});click(&editor,"package-add-states");setName(editor,QStringLiteral("돌파"));
  const QStringList labels{QStringLiteral("수련 준비"),QStringLiteral("호흡 수련"),QStringLiteral("경지 돌파"),QStringLiteral("다음 수련 준비")};
  for(int n=0;n<4;++n) {
    selectPath(&editor,"packageTree",{"progressions","0"});click(&editor,"package-add-transitions");setName(editor,labels[n]);
    auto from=widget<QListWidget>(&editor,"transition-from");for(int i=0;i<from->count();++i)from->item(i)->setCheckState(i==(n==0?0:n==3?2:1)?Qt::Checked:Qt::Unchecked);
    select(widget<QComboBox>(&editor,"transition-to"),n==2?"state-3":n==3?"state-1":"state-2");
    if(n<2){QTimer::singleShot(0,[n]{auto costs=qobject_cast<QDialog*>(QApplication::activeModalWidget());check(costs&&costs->objectName()=="ruleCosts","cost form");widget<QSpinBox>(costs,"cost-money")->setValue(n==0?2:3);save(costs);});click(&editor,"transition-costs");}
    if(n==1)body(editor,"transition-effects",[](LogicEditor *dialog){click(dialog,"logicAdd");selectPath(dialog,"logicTree",{"0"});select(widget<QComboBox>(dialog,"logicType"),"ruleEffect");});
    if(n==2)body(editor,"transition-requirements",[](LogicEditor *dialog){select(widget<QComboBox>(dialog,"logicType"),"ruleCondition");});
  }
  selectPath(&editor,"packageTree",{"progressions","0"});
  bind(editor,{"transitions","1","effects","0","args","param-1"},QStringLiteral("돌파할 능력"));
  bind(editor,{"transitions","1","effects","0","args","param-2"},QStringLiteral("수련 성장량"));
  // Referenced states cannot be removed, including the initial state.
  selectPath(&editor,"packageTree",{"progressions","0","states","1"});const auto stable=editor.value();click(&editor,"remove-package-item");check(editor.value()==stable,"referenced state deletion blocked");
  // Local validation shows an actionable error and keeps the draft open.
  selectPath(&editor,"packageTree",{"progressions","0","transitions","0"});auto from=widget<QListWidget>(&editor,"transition-from");for(int i=0;i<from->count();++i)from->item(i)->setCheckState(Qt::Unchecked);
  widget<QDialogButtonBox>(&editor,"")->button(QDialogButtonBox::Save)->click();check(editor.result()!=QDialog::Accepted&&!widget<QLabel>(&editor,"packageStatus")->text().isEmpty(),"unconnected transition rejected");from->item(0)->setCheckState(Qt::Checked);
  // Export through the actual nonnative file dialog; no external package writer.
  QTimer::singleShot(0,[output]{auto picker=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());check(picker!=nullptr,"export file dialog");picker->selectFile(output+"/authored.raiserules");QMetaObject::invokeMethod(picker,"accept");});click(&editor,"exportRulePackage");
  QFile exported(output+"/authored.raiserules");check(exported.open(QIODevice::ReadOnly),"package exported");check(QJsonDocument::fromJson(exported.readAll()).object()==editor.value(),"export round trip");
  check(editor.grab().save(output+"/workshop.png"),"workshop screenshot");save(&editor);
  // Cancel is a detached draft, so it cannot mutate the accepted package.
  const auto pack=editor.value();RulePackageEditor cancelled(pack,context);widget<QLineEdit>(&cancelled,"package-name")->setText("cancelled");cancelled.reject();check(editor.value()==pack,"cancel preserves source");
  RulePackageEditor upgrade(pack,context);widget<QLineEdit>(&upgrade,"package-version")->setText("1.1.0");
  const auto ownCall=upgrade.value().value("progressions").toArray()[0].toObject().value("transitions").toArray()[1].toObject().value("effects").toArray()[0].toObject();
  check(ownCall.value("version")=="1.1.0","internal calls follow package version");check(editor.value()==pack,"upgrade does not mutate source");
  widget<QLineEdit>(&upgrade,"package-version")->setText("invalid");widget<QDialogButtonBox>(&upgrade,"")->button(QDialogButtonBox::Save)->click();check(upgrade.result()!=QDialog::Accepted,"invalid version rejected");
  std::cout<<"Visual package creation, conditions, effects, parameters, states, transitions, costs, validation, export and cancellation verified\n";
  return pack;
}
