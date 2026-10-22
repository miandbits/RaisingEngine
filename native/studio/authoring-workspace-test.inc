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
  dock->findChild<QPushButton*>("workspaceLogic-effects")->click();QVERIFY(effectEdited);apply();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&w.projectData().value("gameplay").toObject().value("actions").toArray()[2].toObject().value("effects").toArray().size()==1,10000);QTest::qWait(100);
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
