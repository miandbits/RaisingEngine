namespace {
bool copyWorkspaceSample(const QString &source,const QString &target){
  if(!QDir().mkpath(target))return false;
  for(const auto &entry:QDir(source).entryInfoList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot)){
    if(entry.isSymLink())continue;
    if(entry.isDir()){if(!copyWorkspaceSample(entry.absoluteFilePath(),target+"/"+entry.fileName()))return false;}
    else if(!QFile::copy(entry.absoluteFilePath(),target+"/"+entry.fileName()))return false;
  }return true;
}
void selectWorkspaceItem(QWidget *dock,const QString &key){
  auto tree=dock->findChild<QTreeWidget*>("workspaceEntities");QVERIFY(tree);QTreeWidgetItemIterator it(tree);
  while(*it){if((*it)->data(0,Qt::UserRole).toJsonObject().value("key")==key){tree->setCurrentItem(*it);tree->scrollToItem(*it);return;}++it;}QFAIL(qPrintable(key));
}
}

void StudioTests::authoringWorkspaceRoundtrip(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  const auto root=qEnvironmentVariable("RAISING_WORKSPACE_PROJECT",engine+"/.cache/workspace-authored-"+QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(!QFileInfo::exists(root));QVERIFY(copyWorkspaceSample(engine+"/tests/fixtures/projects/time-session-demo",root));
  const auto evidence=engine+"/docs/evidence/authoring-workspace";QDir().mkpath(evidence);
  StudioWindow w(engine);w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  w.findChild<QAction*>("showAuthoringWorkspace")->trigger();
  auto dock=w.findChild<QDockWidget*>("authoringWorkspaceDock");QVERIFY(dock);
  auto tree=dock->findChild<QTreeWidget*>("workspaceEntities");QVERIFY(tree);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&tree->topLevelItemCount()>4,20000);QTest::qWait(100);
  auto apply=[&]{dock->findChild<QPushButton*>("workspaceApply")->click();};
  auto settled=[&]{QCoreApplication::processEvents();};
  selectWorkspaceItem(dock,"item:book");auto stack=dock->findChild<QSpinBox*>("workspaceField-maxStack");QVERIFY(stack);QCOMPARE(stack->value(),10);stack->setValue(25);apply();
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&w.projectData().value("gameplay").toObject().value("items").toArray()[1].toObject().value("maxStack").toInt()==25,15000);QTest::qWait(100);
  dock->findChild<QSpinBox*>("workspaceField-maxStack")->setValue(26);w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&w.projectData().value("gameplay").toObject().value("items").toArray()[1].toObject().value("maxStack").toInt()==10,10000);QTest::qWait(100);
  apply();QVERIFY(dock->findChild<QLabel*>("workspaceStatus")->text().contains(QString::fromUtf8("버전")));QCOMPARE(dock->findChild<QSpinBox*>("workspaceField-maxStack")->value(),26);dock->findChild<QPushButton*>("workspaceReload")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QTest::qWait(100);
  dock->findChild<QPushButton*>("workspace-redoEdit")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&w.projectData().value("gameplay").toObject().value("items").toArray()[1].toObject().value("maxStack").toInt()==25,10000);QTest::qWait(100);
  selectWorkspaceItem(dock,"timing:action-1");dock->findChild<QSpinBox*>("workspaceField-duration")->setValue(6);apply();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&w.projectData().value("progression").toObject().value("actions").toArray()[0].toObject().value("duration").toInt()==6,10000);QTest::qWait(100);
  selectWorkspaceItem(dock,"appointment:visit-after-two");dock->findChild<QSpinBox*>("workspaceField-atTick")->setValue(3);apply();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&w.projectData().value("progression").toObject().value("appointments").toArray()[0].toObject().value("atTick").toInt()==3,10000);QTest::qWait(100);
  characterChoose(dock->findChild<QComboBox*>("workspaceAddKind"),"action");dock->findChild<QPushButton*>("workspaceAdd")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&w.projectData().value("gameplay").toObject().value("actions").toArray().size()==3,10000);QTest::qWait(100);
  selectWorkspaceItem(dock,"action:action-3");auto name=dock->findChild<QLineEdit*>("workspaceField-name");QVERIFY(name);name->selectAll();QTest::keyClicks(name,"Follow-up training");
  bool effectEdited=false;QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("logicEditor");QVERIFY(d);characterNode(d,{});d->findChild<QPushButton*>("logicAdd")->click();characterTarget(d,{"0","target"},"attribute","skill");characterNumber(d,{"0","value"},2);effectEdited=true;d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});
  dock->findChild<QPushButton*>("workspaceLogic-effects")->click();QTRY_VERIFY(effectEdited);apply();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&w.projectData().value("gameplay").toObject().value("actions").toArray()[2].toObject().value("effects").toArray().size()==1,10000);QTest::qWait(100);
  selectWorkspaceItem(dock,"action:action-1");dock->findChild<QPushButton*>("workspaceAddBranch")->click();auto branches=dock->findChild<QTableWidget*>("workspaceBranches");QCOMPARE(branches->rowCount(),1);characterChoose(qobject_cast<QComboBox*>(branches->cellWidget(0,1)),"action-3");
  QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("workspaceChanceDialog");QVERIFY(d);d->findChild<QCheckBox*>("workspaceChanceEnabled")->setChecked(true);d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});qobject_cast<QPushButton*>(branches->cellWidget(0,5))->click();apply();
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&w.projectData().value("gameplay").toObject().value("actions").toArray()[0].toObject().value("flow").toArray().size()==1,10000);QTest::qWait(100);
  QCOMPARE(w.projectData().value("gameplay").toObject().value("actions").toArray()[0].toObject().value("flow").toArray()[0].toObject().value("chance").toObject().value("value").toInt(),10000);
  auto links=dock->findChild<QTreeWidget*>("workspaceReferences");QVERIFY(links);bool foundBranch=false;QTreeWidgetItemIterator link(links);while(*link){if((*link)->data(0,Qt::UserRole).toString()=="action:action-3"){foundBranch=true;break;}++link;}QVERIFY(foundBranch);
  // The same signal emitted by a double click performs source navigation.
  QMetaObject::invokeMethod(links,"itemDoubleClicked",Q_ARG(QTreeWidgetItem*,*link),Q_ARG(int,0));settled();QCOMPARE(tree->currentItem()->data(0,Qt::UserRole).toJsonObject().value("key").toString(),QString("action:action-3"));
  selectWorkspaceItem(dock,"action:action-1");QTRY_VERIFY(dock->findChild<QLineEdit*>("workspaceField-name")->isVisible());QTest::qWait(100);dock->grab().save(evidence+"/workspace.png");
  dock->findChild<QPushButton*>("workspace-saveProject")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));const auto authored=w.projectData();
  dock->findChild<QPushButton*>("workspaceDebug")->click();dock->findChild<QPushButton*>("debugStart")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  characterChoose(dock->findChild<QComboBox*>("debugActor"),"apprentice");characterChoose(dock->findChild<QComboBox*>("debugSubject"),"action-1");dock->findChild<QPushButton*>("debugInspect")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY(dock->findChild<QTreeWidget*>("debugConditions")->topLevelItemCount()>0);dock->grab().save(evidence+"/condition-debugger.png");
  auto execute=[&](QString command){characterChoose(dock->findChild<QComboBox*>("debugCommandType"),command);dock->findChild<QPushButton*>("debugExecute")->click();};
  characterChoose(dock->findChild<QComboBox*>("debugCommandType"),"executeActorAction");characterChoose(dock->findChild<QComboBox*>("debugCommandTarget"),"action-1");execute("executeActorAction");QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  execute("advanceSession");QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY(dock->findChild<QTreeWidget*>("debugTrace")->topLevelItemCount()>0);dock->grab().save(evidence+"/appointment-debugger.png");
  w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(w.projectData(),authored);
  QFile proof(engine+"/.cache/authoring-workspace-proof.json");QVERIFY(proof.open(QIODevice::WriteOnly));proof.write(QJsonDocument(QJsonObject{{"project",root},{"sourceSample","tests/fixtures/projects/time-session-demo"},{"guiEdited",true},{"saveReopen",true},{"undoRedo",true},{"referenceNavigation",true},{"debugger",true}}).toJson());
}

void StudioTests::authoringWorkspacePlayIntegration(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());QFile proof(engine+"/.cache/authoring-workspace-proof.json");QVERIFY(proof.open(QIODevice::ReadOnly));const auto root=QJsonDocument::fromJson(proof.readAll()).object().value("project").toString();
  StudioWindow w(engine);w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);w.playAutomation=engine+"/docs/evidence/authoring-workspace/play-steps.json";w.playReport=engine+"/docs/evidence/authoring-workspace/editor-play-report.json";QVERIFY(QFileInfo::exists(w.playAutomation));QFile::remove(w.playReport);
  QSignalSpy finished(&w,&StudioWindow::playerFinished);w.findChild<QAction*>("playProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(finished.count()>0,60000);QCOMPARE(finished.first().first().toInt(),0);QFile report(w.playReport);QVERIFY(report.open(QIODevice::ReadOnly));const auto result=QJsonDocument::fromJson(report.readAll()).object();QVERIFY(result.value("ok").toBool());int found=0;for(const auto &v:result.value("steps").toArray())if(v.toObject().value("command").toObject().value("checkpoint")=="complete"){const auto state=v.toObject().value("ui").toObject().value("state").toObject();QCOMPARE(state.value("progression").toObject().value("tick").toInt(),6);QVERIFY(state.value("progression").toObject().value("pending").isNull());found++;}QCOMPARE(found,1);w.grab().save(engine+"/docs/evidence/authoring-workspace/editor-play.png");
}

void StudioTests::unifiedConnectionAuthoring(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  QTemporaryDir temporary(engine+"/.cache/workspace-unified-XXXXXX");QVERIFY(temporary.isValid());const auto root=temporary.path()+"/game";
  QVERIFY(copyWorkspaceSample(engine+"/tests/fixtures/projects/time-session-demo",root));
  StudioWindow w(engine);w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  w.findChild<QAction*>("showAuthoringWorkspace")->trigger();auto dock=w.findChild<QDockWidget*>("authoringWorkspaceDock");QVERIFY(dock);
  auto tree=dock->findChild<QTreeWidget*>("workspaceEntities");QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&tree->topLevelItemCount()>3,20000);
  auto button=[&](QString name){auto b=dock->findChild<QPushButton*>(name);Q_ASSERT(b);return b;};
  auto document=[&](QString kind,QString label){for(auto v:w.currentView().value("documents").toArray()){auto d=v.toObject(),data=d.value("data").toObject();if(d.value("kind")==kind&&(data.value("label")==label||data.value("name")==label))return d;}return QJsonObject{};};
  selectWorkspaceItem(dock,"character:apprentice");button("workspaceActor")->click();QCOMPARE(tree->currentItem()->data(0,Qt::UserRole).toJsonObject().value("key"),QJsonValue("actor:apprentice"));button("workspaceAssignedActivity")->click();QCOMPARE(tree->currentItem()->data(0,Qt::UserRole).toJsonObject().value("key"),QJsonValue("activity:study"));
  // Creation and rule editing stay in the workspace. Cancel has no project edit.
  characterChoose(dock->findChild<QComboBox*>("workspaceAddKind"),"character");button("workspaceAdd")->click();auto create=dock->findChild<QLineEdit*>("workspaceNewName");QVERIFY(create);QVERIFY(!create->window()->isModal());
  QTest::keyClicks(create,"Kiln mentor");auto dialog=qobject_cast<QDialog*>(create->parentWidget());QVERIFY(dialog);QVERIFY(!dialog->isWindow());dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!document("character","Kiln mentor").empty(),10000);QTest::qWait(100);const auto mentor=document("character","Kiln mentor").value("data").toObject().value("id").toString();
  selectWorkspaceItem(dock,"activity:study");button("workspaceEncounter")->click();auto input=dock->findChild<QLineEdit*>("workspaceNewName");QVERIFY(input);input->setText("Kiln encounter");dock->findChild<QPlainTextEdit*>("workspaceNewDialogue")->setPlainText("Choose how to finish the bowl.");dock->findChild<QLineEdit*>("workspaceNewPrompt")->setText("Will you help?");dock->findChild<QLineEdit*>("workspaceNewChoice1")->setText("Help together");dock->findChild<QLineEdit*>("workspaceNewChoice2")->setText("Work alone");dock->findChild<QCheckBox*>("workspaceNewPersonal")->setChecked(true);characterChoose(dock->findChild<QComboBox*>("workspaceNewSpeaker"),mentor);
  dialog=qobject_cast<QDialog*>(input->parentWidget());dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!document("event","Kiln encounter").empty(),10000);QTest::qWait(100);
  auto encounter=document("event","Kiln encounter");const auto eventId=encounter.value("data").toObject().value("id").toString();QCOMPARE(encounter.value("data").toObject().value("scope"),QJsonValue("actor"));QCOMPARE(encounter.value("data").toObject().value("activityId"),QJsonValue("study"));
  selectWorkspaceItem(dock,"scriptChoice:event/"+eventId+"/decision/first");button("workspaceLogic-effects")->click();auto logic=dock->findChild<QDialog*>("logicEditor");QVERIFY(logic);QVERIFY(!logic->isWindow());QVERIFY(dock->property("workspaceInputPending").toBool());
  auto owner=logic->findChild<QComboBox*>("quickOwner");owner->setCurrentIndex(1);auto target=logic->findChild<QComboBox*>("quickTarget");for(int i=0;i<target->count();++i)if(target->itemData(i).toJsonObject().value("attributeId")=="skill")target->setCurrentIndex(i);logic->findChild<QSpinBox*>("quickAmount")->setValue(7);logic->findChild<QPushButton*>("quickApply")->click();logic->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);button("workspaceApply")->click();
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QTest::qWait(100);encounter=document("event","Kiln encounter");auto effect=encounter.value("data").toObject().value("nodes").toArray()[1].toObject().value("choices").toArray()[0].toObject().value("effects").toArray()[0].toObject();QCOMPARE(effect.value("target").toObject().value("owner").toObject().value("kind"),QJsonValue("actor"));QCOMPARE(effect.value("value").toObject().value("value").toInt(),7);
  const auto revision=w.currentView().value("revision");button("workspaceLogic-effects")->click();logic=dock->findChild<QDialog*>("logicEditor");QVERIFY(logic);logic->findChild<QPushButton*>("quickApply")->click();logic->reject();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QCOMPARE(w.currentView().value("revision"),revision);QVERIFY(!dock->property("workspaceInputPending").toBool());
  button("workspaceFollowup")->click();auto follow=dock->findChild<QDialog*>("storyFollowupDialog");QVERIFY(follow);QVERIFY(!follow->isWindow());follow->findChild<QLineEdit*>("followupTitle")->setText("Promised bowl");follow->findChild<QPlainTextEdit*>("followupDialogue")->setPlainText("You kept your promise.");follow->findChild<QDialogButtonBox*>("followupButtons")->button(QDialogButtonBox::Save)->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!document("event","Promised bowl").empty(),10000);QTest::qWait(100);
  const auto after=document("event","Promised bowl").value("data").toObject();QCOMPARE(after.value("scope"),QJsonValue("actor"));const auto condition=QJsonDocument(after.value("requirements").toObject()).toJson(QJsonDocument::Compact);QVERIFY(condition.contains("eventCompleted"));QVERIFY(condition.contains("story-choice-"));QVERIFY(condition.contains("\"kind\":\"actor\""));
  button("workspace-saveProject")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  auto snapshot=[&]{QJsonObject out;for(auto v:w.currentView().value("documents").toArray()){auto d=v.toObject();out[d.value("file").toString()]=d.value("data");}return out;};const auto saved=snapshot();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(snapshot(),saved);
  // Unapplied in-place input cannot disappear when closing the editor.
  QTest::qWait(100);selectWorkspaceItem(dock,"event:"+after.value("id").toString());button("workspaceLogic-requirements")->click();
  QTimer::singleShot(50,&w,[&]{auto guard=w.findChild<QMessageBox*>("workspaceUnsavedDialog");QVERIFY(guard);guard->button(QMessageBox::Cancel)->click();});QVERIFY(!w.close());QVERIFY(w.isVisible());QVERIFY(dock->property("workspaceInputPending").toBool());dock->findChild<QDialog*>("logicEditor")->reject();
  auto label=dock->findChild<QLineEdit*>("workspaceField-label");QVERIFY(label);label->selectAll();QTest::keyClicks(label,"Discard this input");QVERIFY(dock->property("workspaceInputPending").toBool());
  QTimer::singleShot(50,&w,[&]{auto guard=w.findChild<QMessageBox*>("workspaceUnsavedDialog");QVERIFY(guard);guard->button(QMessageBox::Discard)->click();});QVERIFY(w.close());QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(snapshot(),saved);QVERIFY(!dock->property("workspaceInputPending").toBool());QCOMPARE(dock->findChild<QLineEdit*>("workspaceField-label")->text(),QString("Promised bowl"));
}
