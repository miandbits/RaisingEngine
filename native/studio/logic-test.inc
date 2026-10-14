#include "logic-editor.hpp"
#include <QTreeWidgetItemIterator>
#include <QDoubleSpinBox>

namespace {
bool logicTestChoose(QDialog &dialog, const QString &name, const QString &value) {
  auto field = dialog.findChild<QComboBox *>(name);
  if (!field) return false;
  int index = field->findData(value);
  if (index < 0) return false;
  field->setCurrentIndex(index);
  QMetaObject::invokeMethod(field, "activated", Q_ARG(int, index));
  QCoreApplication::processEvents();
  return true;
}
bool logicTestSelect(QDialog &dialog, const QStringList &path) {
  auto tree = dialog.findChild<QTreeWidget *>("logicTree");
  if (!tree) return false;
  for (QTreeWidgetItemIterator i(tree); *i; ++i) {
    if ((*i)->data(0, Qt::UserRole).toStringList() == path) {
      tree->setCurrentItem(*i); QCoreApplication::processEvents(); return true;
    }
  }
  return false;
}
bool logicTestNumber(QDialog &dialog, const QString &name, int value) {
  auto field = dialog.findChild<QSpinBox *>("logic-" + name);
  if (!field) return false;
  field->setValue(value); QMetaObject::invokeMethod(field, "editingFinished");
  QCoreApplication::processEvents(); return true;
}
QString logicTestState(QTreeWidget *tree, const QStringList &path) {
  if (!tree || path.empty()) return {};
  QTreeWidgetItem *node = nullptr;
  for (const auto &part : path) {
    QTreeWidgetItem *next = nullptr;
    const int count = node ? node->childCount() : tree->topLevelItemCount();
    for (int i = 0; i < count; ++i) {
      auto candidate = node ? node->child(i) : tree->topLevelItem(i);
      if (candidate->text(0) == part) { next = candidate; break; }
    }
    if (!next) return {}; node = next;
  }
  return node->text(1);
}
}

void StudioTests::logicAstEditor() {
  const auto context=QJsonDocument::fromJson(R"json({"documents":[
    {"kind":"attributes","data":{"attributes":[{"id":"money","label":"Money"},{"id":"stamina","label":"Stamina"}]}},
    {"kind":"flags","data":{"flags":[{"id":"blessed","label":"Blessed"}]}},
    {"kind":"character","data":{"id":"mentor","name":"Mentor","relationships":[{"id":"trust","label":"Trust"}]}},
    {"kind":"project","data":{"gameplay":{"items":[{"id":"flower","name":"Flower"}]}}}
  ]})json").object();
  QVERIFY(!context.empty());
  const QJsonObject original{{"op","always"}};
  LogicEditor editor(original, "condition", context); editor.show();
  QVERIFY(logicTestChoose(editor,"logicType","all"));
  QVERIFY(logicTestSelect(editor,{"conditions","0"})); QVERIFY(logicTestChoose(editor,"logicType","compare"));
  QVERIFY(logicTestSelect(editor,{"conditions","0","left"})); QVERIFY(logicTestChoose(editor,"logicType","multiply"));
  QVERIFY(logicTestSelect(editor,{"conditions","0","left","left"})); QVERIFY(logicTestChoose(editor,"logicType","read"));
  QVERIFY(logicTestSelect(editor,{"conditions","0","left","left","target"})); QVERIFY(logicTestChoose(editor,"logicType","inventory"));
  QVERIFY(logicTestChoose(editor,"logic-itemId","flower"));
  QVERIFY(logicTestSelect(editor,{"conditions","0","left","right"})); QVERIFY(logicTestNumber(editor,"value",2));
  QVERIFY(logicTestSelect(editor,{"conditions","0","right"})); QVERIFY(logicTestNumber(editor,"value",3));
  QVERIFY(logicTestSelect(editor,{})); editor.findChild<QPushButton *>("logicAdd")->click();
  QVERIFY(logicTestSelect(editor,{"conditions","1"})); QVERIFY(logicTestChoose(editor,"logicType","not"));
  QVERIFY(logicTestSelect(editor,{"conditions","1","condition"})); QVERIFY(logicTestChoose(editor,"logicType","flag"));
  QVERIFY(logicTestChoose(editor,"logic-flagId","blessed"));
  const auto conditions = editor.value().toObject().value("conditions").toArray(); QCOMPARE(conditions.size(),2);
  QVERIFY(logicTestSelect(editor,{})); QVERIFY(logicTestChoose(editor,"logicType","any"));
  QCOMPARE(editor.value().toObject().value("conditions").toArray(),conditions);
  QVERIFY(logicTestSelect(editor,{"conditions","0","left"}));
  const auto expression = logicValueAt(editor.value(),{"conditions","0","left"}).toObject();
  QVERIFY(logicTestChoose(editor,"logicType","add"));
  const auto changed = logicValueAt(editor.value(),{"conditions","0","left"}).toObject();
  QCOMPARE(changed.value("left"),expression.value("left")); QCOMPARE(changed.value("right"),expression.value("right"));
  editor.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click(); QCOMPARE(editor.result(),int(QDialog::Accepted));
  const auto accepted = editor.value(); LogicEditor cancelled(accepted,"condition",context); cancelled.show();
  QVERIFY(logicTestChoose(cancelled,"logicType","always")); cancelled.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Cancel)->click();
  QCOMPARE(cancelled.result(),int(QDialog::Rejected)); QCOMPARE(editor.value(),accepted); QCOMPARE(original,QJsonObject({{"op","always"}}));

  LogicEditor effects(QJsonArray{},"effects",context); effects.show(); effects.findChild<QPushButton *>("logicAdd")->click();
  QVERIFY(logicTestSelect(effects,{"0","target"})); QVERIFY(logicTestChoose(effects,"logicType","relationship"));
  QVERIFY(logicTestChoose(effects,"logic-characterId","mentor")); QVERIFY(logicTestChoose(effects,"logic-relationshipId","trust"));
  QVERIFY(logicTestSelect(effects,{"0","value"})); QVERIFY(logicTestChoose(effects,"logicType","clamp"));
  QVERIFY(logicTestNumber(effects,"min",0)); QVERIFY(logicTestNumber(effects,"max",10));
  QVERIFY(logicTestSelect(effects,{"0","value","value"})); QVERIFY(logicTestNumber(effects,"value",25));
  QVERIFY(logicTestSelect(effects,{})); effects.findChild<QPushButton *>("logicAdd")->click();
  QVERIFY(logicTestSelect(effects,{"1"})); QVERIFY(logicTestChoose(effects,"logicType","setFlag"));
  QVERIFY(logicTestChoose(effects,"logic-flagId","blessed"));
  auto flag = effects.findChild<QCheckBox *>("logic-value"); QVERIFY(flag); if (flag->isChecked()) flag->click(); QCoreApplication::processEvents();
  QCOMPARE(logicValueAt(effects.value(),{"0","target","kind"}).toString(),QString("relationship"));
  QCOMPARE(logicValueAt(effects.value(),{"0","value","value","value"}).toInt(),25);
  QCOMPARE(logicValueAt(effects.value(),{"1","value"}).toBool(),false);
  effects.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click(); QCOMPARE(effects.result(),int(QDialog::Accepted));
}

void StudioTests::logicAuthoringAndDebug() {
  const auto root=qEnvironmentVariable("RAISING_LOGIC_PROJECT"); if(root.isEmpty())QSKIP("Set RAISING_LOGIC_PROJECT to a dedicated project copy");
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  StudioWindow w(engine);w.resize(1440,1000);w.show();w.openProject(root);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().empty(),20000);
  QStringList actionIds;
  QTimer::singleShot(100,&w,[&]{auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(d);QTimer::singleShot(15000,d,&QDialog::reject);
    auto kind=d->findChild<QComboBox*>("ruleKind");kind->setCurrentIndex(kind->findData("actions"));
    for(int k=0;k<3;k++){
      d->findChild<QPushButton*>("addRule")->click();actionIds.append(d->findChild<QLineEdit*>("ruleId")->text());
      d->findChild<QLineEdit*>("ruleName")->setText(QStringList{"Logic purchase chain","Logic reward","Logic blocked"}[k]);
      if(k==0)d->findChild<QCheckBox*>("ruleConfirm-enabled")->setChecked(false);
      if(k==0){d->findChild<QPushButton*>("addRule-costs")->click();auto t=d->findChild<QTableWidget*>("rule-costs");auto a=qobject_cast<QComboBox*>(t->cellWidget(0,0));a->setCurrentIndex(a->findData("money"));qobject_cast<QSpinBox*>(t->cellWidget(0,1))->setValue(5);}
      if(k==1){d->findChild<QPushButton*>("addRule-effects")->click();auto t=d->findChild<QTableWidget*>("rule-effects");auto a=qobject_cast<QComboBox*>(t->cellWidget(0,0));a->setCurrentIndex(a->findData("money"));qobject_cast<QSpinBox*>(t->cellWidget(0,2))->setValue(3);}
      if(k==2){d->findChild<QPushButton*>("addRule-conditions")->click();auto t=d->findChild<QTableWidget*>("rule-conditions");auto a=qobject_cast<QComboBox*>(t->cellWidget(0,0));a->setCurrentIndex(a->findData("money"));qobject_cast<QSpinBox*>(t->cellWidget(0,2))->setValue(999999);}
    }
    d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
  });w.findChild<QAction*>("editRules")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(actionIds.size(),3);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  QTimer::singleShot(100,&w,[&]{auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(d);QCOMPARE(d->objectName(),QString("workflowEditor"));QTimer::singleShot(15000,d,&QDialog::reject);
    auto owner=d->findChild<QComboBox*>("workflowOwner");owner->setCurrentIndex(owner->findData(actionIds[0]));
    auto tabs=d->findChild<QTabWidget*>();tabs->setCurrentIndex(1);d->findChild<QPushButton*>("workflowStepAdd")->click();auto steps=d->findChild<QTableWidget*>("workflowSteps");auto target=qobject_cast<QComboBox*>(steps->cellWidget(0,1));target->setCurrentIndex(target->findData(actionIds[1]));
    auto chance=qobject_cast<QPushButton*>(steps->cellWidget(0,5));QVERIFY(chance);
    QTimer::singleShot(50,d,[&]{auto c=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(c);QCOMPARE(c->objectName(),QString("workflowChanceDialog"));c->findChild<QCheckBox*>("workflowChanceEnabled")->setChecked(true);c->findChild<QDoubleSpinBox*>("workflowChancePercent")->setValue(37.25);c->findChild<QDialogButtonBox*>("workflowChanceButtons")->button(QDialogButtonBox::Cancel)->click();});chance->click();
    QVERIFY(chance->property("chance").toJsonValue().isUndefined()||chance->property("chance").toJsonValue().isNull());
    QTimer::singleShot(50,d,[&]{auto c=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(c);c->findChild<QCheckBox*>("workflowChanceEnabled")->setChecked(true);c->findChild<QDoubleSpinBox*>("workflowChancePercent")->setValue(100);c->findChild<QDialogButtonBox*>("workflowChanceButtons")->button(QDialogButtonBox::Ok)->click();});chance->click();
    QCOMPARE(chance->property("chance").toJsonValue().toObject().value("value").toInt(),10000);
    tabs->setCurrentIndex(0);d->findChild<QPushButton*>("workflowRuleAdd")->click();auto rules=d->findChild<QTableWidget*>("workflowRules");int row=rules->rowCount()-1;rules->item(row,0)->setText("Purchase bonus chain");auto hook=qobject_cast<QComboBox*>(rules->cellWidget(row,1));hook->setCurrentIndex(hook->findData("purchaseComplete"));target=qobject_cast<QComboBox*>(rules->cellWidget(row,2));target->setCurrentIndex(target->findData(actionIds[0]));
    d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
  });w.findChild<QAction*>("editWorkflows")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  for(auto v:w.currentView().value("diagnostics").toArray())QVERIFY2(v.toObject().value("severity")!="error",qPrintable(QString::fromUtf8(QJsonDocument(v.toObject()).toJson())));
  w.findChild<QAction*>("addButton")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);const auto buttonId=w.selectedId();
  auto clickAction=w.findChild<QComboBox*>("buttonAction");QVERIFY(clickAction);int actionIndex=clickAction->findData("executeAction");QVERIFY(actionIndex>=0);clickAction->setCurrentIndex(actionIndex);QMetaObject::invokeMethod(clickAction,"activated",Q_ARG(int,actionIndex));QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  auto clickTarget=w.findChild<QComboBox*>("actionTarget");QVERIFY(clickTarget);int targetIndex=clickTarget->findData(actionIds[0]);QVERIFY(targetIndex>=0);clickTarget->setCurrentIndex(targetIndex);QMetaObject::invokeMethod(clickTarget,"activated",Q_ARG(int,targetIndex));QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  auto documentTabs=w.findChild<QTabBar*>("documentTabs");QVERIFY(documentTabs);const auto ownerScreen=documentTabs->tabData(documentTabs->currentIndex()).toString();
  bool buttonFound=false;for(auto screen:w.projectData().value("visual").toObject().value("screens").toArray()){if(screen.toObject().value("id")!=ownerScreen)continue;for(auto value:screen.toObject().value("elements").toArray()){auto e=value.toObject();if(e.value("id")==buttonId){buttonFound=true;QCOMPARE(e.value("action").toString(),QString("executeAction"));QCOMPARE(e.value("actionId").toString(),actionIds[0]);QVERIFY(!e.contains("screenId"));}}}QVERIFY(buttonFound);
  const auto authored=w.projectData();w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY(!w.currentView().value("dirty").toBool());
  bool immediateFound=false;for(auto value:authored.value("gameplay").toObject().value("actions").toArray()){auto a=value.toObject();if(a.value("id")==actionIds[0]){immediateFound=true;QVERIFY(!a.value("confirmation").toObject().value("enabled").toBool(true));}}QVERIFY(immediateFound);
  StudioWindow reopened(engine);reopened.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!reopened.busy()&&!reopened.projectData().empty(),20000);QCOMPARE(reopened.projectData(),authored);
  w.findChild<QAction*>("showRuleDebugger")->trigger();auto dock=w.findChild<QDockWidget*>("ruleDebugDock");QVERIFY(dock);
  auto press=[&](const QString &name){auto b=dock->findChild<QPushButton*>(name);QVERIFY(b);b->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));};
  press("debugStart");auto state=dock->findChild<QTreeWidget*>("debugState");QVERIFY(state);QCOMPARE(logicTestState(state,{"stateRevision"}),QString("0"));
  auto subject=dock->findChild<QComboBox*>("debugSubject");subject->setCurrentIndex(subject->findData(actionIds[2]));press("debugInspect");
  auto conditions=dock->findChild<QTreeWidget*>("debugConditions");QVERIFY(conditions->topLevelItemCount()>0);QVERIFY(conditions->topLevelItem(0)->text(0).contains("차단"));QCOMPARE(logicTestState(state,{"stateRevision"}),QString("0"));
  auto command=dock->findChild<QComboBox*>("debugCommandType");auto target=dock->findChild<QComboBox*>("debugCommandTarget");
  command->setCurrentIndex(command->findData("executeAction"));target->setCurrentIndex(target->findData(actionIds[0]));const auto money=logicTestState(state,{"attributes","money"}).toInt();press("debugExecute");QCOMPARE(logicTestState(state,{"attributes","money"}).toInt(),money-2);
  QVERIFY(dock->findChild<QTreeWidget*>("debugTrace")->topLevelItemCount()>=3);QVERIFY(dock->findChild<QTreeWidget*>("debugDiff")->topLevelItemCount()>0);
  const auto g=authored.value("gameplay").toObject();const auto shop=g.value("shops").toArray().first().toObject();QVERIFY(!shop.empty());
  const auto destination=shop.value("locationId").toString(), start=g.value("startLocationId").toString();QMap<QString,QStringList> paths;paths[start]={};QStringList queue{start};
  for(int i=0;i<queue.size()&&!paths.contains(destination);i++)for(auto v:g.value("locations").toArray()){const auto location=v.toObject();if(location.value("id")!=queue[i])continue;for(auto next:location.value("connections").toArray()){auto id=next.toString();if(paths.contains(id))continue;auto p=paths[queue[i]];p.append(id);paths[id]=p;queue.append(id);}}
  QVERIFY(paths.contains(destination));for(const auto &location:paths[destination]){command->setCurrentIndex(command->findData("visit"));target->setCurrentIndex(target->findData(location));press("debugExecute");}
  const auto offer=shop.value("offers").toArray().first().toObject();const int beforeBuy=logicTestState(state,{"attributes","money"}).toInt();
  command->setCurrentIndex(command->findData("buy"));target->setCurrentIndex(target->findData(shop.value("id").toString()));auto item=dock->findChild<QComboBox*>("debugCommandItem");item->setCurrentIndex(item->findData(offer.value("itemId").toString()));press("debugExecute");QCOMPARE(logicTestState(state,{"attributes","money"}).toInt(),beforeBuy-offer.value("price").toInt()-2);
  bool sawRule=false,sawFlow=false;auto trace=dock->findChild<QTreeWidget*>("debugTrace");for(int i=0;i<trace->topLevelItemCount();i++){sawRule|=trace->topLevelItem(i)->text(0).startsWith("rule");sawFlow|=trace->topLevelItem(i)->text(0).startsWith("flow");}QVERIFY(sawRule);QVERIFY(sawFlow);
  subject->setCurrentIndex(subject->findData(actionIds[0]));press("debugImpact");QVERIFY(dock->findChild<QTreeWidget*>("debugLinks")->topLevelItemCount()>=2);
  const auto afterImpact=logicTestState(state,{"attributes","money"}).toInt();const auto revisionAfterImpact=logicTestState(state,{"stateRevision"}).toInt();
  command->setCurrentIndex(command->findData("executeAction"));target->setCurrentIndex(target->findData(actionIds[0]));press("debugExecute");
  QCOMPARE(logicTestState(state,{"attributes","money"}).toInt(),afterImpact-2);QCOMPARE(logicTestState(state,{"stateRevision"}).toInt(),revisionAfterImpact+1);
  QCOMPARE(w.projectData(),authored);QVERIFY(!w.currentView().value("dirty").toBool());
  QDir().mkpath(engine+"/.cache/qt-logic-evidence");QVERIFY(dock->grab().save(engine+"/.cache/qt-logic-evidence/debugger.png"));
}
