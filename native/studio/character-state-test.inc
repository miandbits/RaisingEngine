#include <QInputDialog>

namespace {
void characterChoose(QComboBox *combo,const QString &value){QVERIFY(combo);const int index=combo->findData(value);QVERIFY2(index>=0,qPrintable(value));combo->setCurrentIndex(index);QMetaObject::invokeMethod(combo,"activated",Q_ARG(int,index));QCoreApplication::processEvents();}
void characterNode(QDialog *dialog,const QStringList &path){auto tree=dialog->findChild<QTreeWidget*>("logicTree");QVERIFY(tree);QTreeWidgetItemIterator it(tree);while(*it){if((*it)->data(0,Qt::UserRole).toStringList()==path){tree->setCurrentItem(*it);QCoreApplication::processEvents();return;}++it;}QFAIL(qPrintable(path.join('/')));}
void characterTarget(QDialog *dialog,const QStringList &path,const QString &kind,const QString &id){
  characterNode(dialog,path);characterChoose(dialog->findChild<QComboBox*>("logicType"),kind);
  if(kind=="inventory")characterChoose(dialog->findChild<QComboBox*>("logic-itemId"),id);
  if(kind=="attribute")characterChoose(dialog->findChild<QComboBox*>("logic-attributeId"),id);
  if(kind=="relationship"){characterChoose(dialog->findChild<QComboBox*>("logic-characterId"),"mentor");characterChoose(dialog->findChild<QComboBox*>("logic-relationshipId"),id);}
  auto owner=dialog->findChild<QComboBox*>("logicOwner");QVERIFY(owner);owner->setCurrentIndex(1);QMetaObject::invokeMethod(owner,"activated",Q_ARG(int,1));QCoreApplication::processEvents();
}
void characterNumber(QDialog *dialog,const QStringList &path,int value){characterNode(dialog,path);auto spin=dialog->findChild<QSpinBox*>("logic-value");QVERIFY(spin);spin->setValue(value);QMetaObject::invokeMethod(spin,"editingFinished");QCoreApplication::processEvents();}
void characterCondition(QDialog *parent,QPushButton *button,const QString &kind,const QString &id,int value){
  QVERIFY(button);QTimer::singleShot(40,parent,[=]{auto d=parent->findChild<QDialog*>("logicEditor");QVERIFY(d);characterNode(d,{});characterChoose(d->findChild<QComboBox*>("logicType"),"compare");characterNode(d,{"left"});characterChoose(d->findChild<QComboBox*>("logicType"),"read");characterTarget(d,{"left","target"},kind,id);characterNumber(d,{"right"},value);d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});button->click();
}
void characterEffects(QDialog *parent,const QList<QPair<QString,QPair<QString,int>>> &effects){
  QTimer::singleShot(40,parent,[=]{auto d=parent->findChild<QDialog*>("logicEditor");QVERIFY(d);for(int i=0;i<effects.size();i++){characterNode(d,{});d->findChild<QPushButton*>("logicAdd")->click();characterTarget(d,{QString::number(i),"target"},effects[i].first,effects[i].second.first);characterNumber(d,{QString::number(i),"value"},effects[i].second.second);}d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});parent->findChild<QPushButton*>("editRuleEffectTree")->click();
}
}

void StudioTests::characterStateAuthoringRoundtrip(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());QFile input(engine+"/.cache/roster-authoring-proof.json");QVERIFY(input.open(QIODevice::ReadOnly));const auto root=QJsonDocument::fromJson(input.readAll()).object().value("project").toString();
  StudioWindow w(engine);w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);QVERIFY(w.projectData().value("roster").toObject().value("stateModel").isUndefined());
  const auto evidence=engine+"/docs/evidence/character-state";QDir().mkpath(evidence);
  QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("characterStateDialog");QVERIFY(d);auto items=d->findChild<QTableWidget*>("characterItems");
    for(int i=0;i<2;i++){d->findChild<QPushButton*>("characterAddItem")->click();items->item(i,0)->setText(i==0?"flower":"book");items->item(i,1)->setText(i==0?QString::fromUtf8("꽃"):QString::fromUtf8("책"));qobject_cast<QSpinBox*>(items->cellWidget(i,2))->setValue(10);for(int col=3;col<6;col++)qobject_cast<QSpinBox*>(items->cellWidget(i,col))->setValue(i==0&&col>3?(col==4?2:1):0);}
    auto rel=d->findChild<QTableWidget*>("characterRelationships");QCOMPARE(rel->rowCount(),2);qobject_cast<QSpinBox*>(rel->cellWidget(0,3))->setValue(3);qobject_cast<QSpinBox*>(rel->cellWidget(1,3))->setValue(7);
    QTimer::singleShot(30,d,[&]{auto definition=d->findChild<QDialog*>("characterRelationshipDefinition");QVERIFY(definition);auto other=definition->findChild<QComboBox*>("relationshipCharacter");other->setCurrentIndex(other->findData("mentor"));definition->findChild<QLineEdit*>("relationshipId")->setText("respect");definition->findChild<QLineEdit*>("relationshipName")->setText(QString::fromUtf8("존경"));definition->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});
    d->findChild<QPushButton*>("characterAddRelationship")->click();QCOMPARE(rel->rowCount(),4);
    d->grab().save(evidence+"/initial-inventory.png");d->findChild<QDialogButtonBox*>("characterStateButtons")->button(QDialogButtonBox::Save)->click();});
  w.findChild<QAction*>("editCharacterState")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  for(int index=0;index<2;index++){
    bool filled=false;QTimer poll;poll.setInterval(30);connect(&poll,&QTimer::timeout,&w,[&]{auto d=w.findChild<QDialog*>("contentEditor");if(!d||filled)return;filled=true;poll.stop();
      d->findChild<QLineEdit*>("contentLabel")->setText(index==0?QString::fromUtf8("민서의 선물 사건"):QString::fromUtf8("하린의 공부 사건"));auto scope=d->findChild<QComboBox*>("content-eventScope");scope->setCurrentIndex(scope->findData("actor"));d->findChild<QSpinBox*>("content-throughPeriod")->setValue(35);auto lines=d->findChild<QTableWidget*>("dialogueTable");QVERIFY(lines->rowCount()>0);lines->item(0,1)->setText(index==0?QString::fromUtf8("민서가 준비한 꽃을 받았습니다."):QString::fromUtf8("하린의 공부 성과를 확인했습니다."));
      characterCondition(d,d->findChild<QPushButton*>("contentLogic-requirements"),"relationship",index==0?"trust":"respect",index==0?15:20);
      d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
    });poll.start();
    QTimer::singleShot(30,&w,[&]{auto d=w.findChild<QInputDialog*>();QVERIFY(d);d->setTextValue(index==0?"personal-gift":"personal-study");d->accept();});w.findChild<QAction*>("createContent-event")->trigger();QTRY_VERIFY_WITH_TIMEOUT(filled&&!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  }
  auto screens=w.findChild<QListWidget*>("projectList");QVERIFY(screens);for(int i=0;i<screens->count();i++)if(screens->item(i)->data(Qt::UserRole).toString()=="schedule"){screens->setCurrentRow(i);QMetaObject::invokeMethod(screens,"itemDoubleClicked",Q_ARG(QListWidgetItem*,screens->item(i)));}QCoreApplication::processEvents();
  QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("rulesDialog");QVERIFY(d);
    for(int i=0;i<2;i++){
      d->findChild<QPushButton*>("addRule")->click();d->findChild<QLineEdit*>("ruleName")->setText(i==0?QString::fromUtf8("꽃 선물하기"):QString::fromUtf8("함께 공부하기"));auto event=d->findChild<QComboBox*>("ruleEvent");event->setCurrentIndex(event->findData(i==0?"personal-gift":"personal-study"));
      if(i==0){characterCondition(d,d->findChild<QPushButton*>("editRuleConditionTree"),"inventory","flower",1);characterEffects(d,{{"inventory",{"flower",-1}},{"relationship",{"trust",15}}});}
      else characterEffects(d,{{"attribute",{"skill",5}},{"inventory",{"book",1}},{"relationship",{"trust",20}},{"relationship",{"respect",20}}});
    }
    d->findChild<QPushButton*>("placeRules")->click();
  });
  w.findChild<QAction*>("editRules")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  // Position the real actions widget with inspector fields, not project JSON.
  for(const auto &pair:QList<QPair<QString,int>>{{"elementX",340},{"elementY",180},{"elementWidth",888},{"elementHeight",230}}){auto field=w.findChild<QSpinBox*>(pair.first);QVERIFY(field);field->setValue(pair.second);QMetaObject::invokeMethod(field,"editingFinished");QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);}
  const auto actions=w.projectData().value("gameplay").toObject().value("actions").toArray();QCOMPARE(actions.size(),2);
  QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("characterStateDialog");QVERIFY(d);auto table=d->findChild<QTableWidget*>("characterActions");for(int row=0;row<2;row++){auto list=qobject_cast<QListWidget*>(table->cellWidget(row,1));QCOMPARE(list->count(),2);list->item(row)->setCheckState(Qt::Checked);}d->findChild<QTabWidget*>("characterStateTabs")->setCurrentIndex(2);d->grab().save(evidence+"/action-bindings.png");d->findChild<QDialogButtonBox*>("characterStateButtons")->button(QDialogButtonBox::Save)->click();});
  w.findChild<QAction*>("editCharacterState")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);
  const auto authored=w.currentView().value("documents");w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.currentView().value("documents"),authored);
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(w.projectData().value("roster").toObject().value("stateModel").toString(),QString("character-v1"));
  const auto beforeRules=w.projectData().value("gameplay"),beforeRoster=w.projectData().value("roster");
  QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("rulesDialog");QVERIFY(d);d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});w.findChild<QAction*>("editRules")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(w.projectData().value("gameplay"),beforeRules);
  QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("rosterAuthoringDialog");QVERIFY(d);d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});w.findChild<QAction*>("editRoster")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(w.projectData().value("roster"),beforeRoster);
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  w.grab().save(evidence+"/editor.png");QFile proof(engine+"/.cache/character-state-authoring-proof.json");QVERIFY(proof.open(QIODevice::WriteOnly));proof.write(QJsonDocument(QJsonObject{{"project",root},{"guiCreated",true},{"logicOwnerWidgets",true},{"undoRedo",true},{"saveReopen",true}}).toJson());
}

void StudioTests::characterStatePlayIntegration(){
 const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());QFile proof(engine+"/.cache/character-state-authoring-proof.json");QVERIFY(proof.open(QIODevice::ReadOnly));const auto root=QJsonDocument::fromJson(proof.readAll()).object().value("project").toString();
 StudioWindow w(engine);w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
 w.playAutomation=engine+"/docs/evidence/character-state/play-steps.json";w.playReport=engine+"/docs/evidence/character-state/editor-play-report.json";QVERIFY(QFileInfo::exists(w.playAutomation));QFile::remove(w.playReport);
 QSignalSpy finished(&w,&StudioWindow::playerFinished);w.findChild<QAction*>("playProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(finished.count()>0,60000);QCOMPARE(finished.first().first().toInt(),0);
 QFile report(w.playReport);QVERIFY(report.open(QIODevice::ReadOnly));const auto result=QJsonDocument::fromJson(report.readAll()).object();QVERIFY(result.value("ok").toBool());
 int matches=0;for(const auto &v:result.value("steps").toArray()){const auto step=v.toObject(),command=step.value("command").toObject();if(command.value("checkpoint")=="both-complete"){const auto state=step.value("ui").toObject().value("state").toObject(),events=state.value("roster").toObject().value("completedEvents").toObject();QVERIFY(events.value("apprentice").toArray().contains("personal-gift"));QVERIFY(events.value("trainee-2").toArray().contains("personal-study"));++matches;}}
 QCOMPARE(matches,1);w.grab().save(engine+"/docs/evidence/character-state/editor-play-completed.png");
}
