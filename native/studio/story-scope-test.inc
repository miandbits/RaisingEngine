void StudioTests::storyScopeConversion(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  const auto root=engine+"/.cache/scope-authored-"+QString::number(QDateTime::currentMSecsSinceEpoch());
  QVERIFY(copyWorkspaceSample(engine+"/tests/fixtures/projects/time-session-demo",root));
  auto write=[&](QString file,QJsonObject data){QFile f(root+"/"+file);QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(data).toJson());};
  auto source=QJsonDocument::fromJson(R"({"formatVersion":"0.1.0","id":"scope-source","label":"Shared workshop meeting","trigger":"after-activity","activityId":"study","window":{"fromPeriod":0,"throughPeriod":1},"priority":90,"requirements":{"op":"always"},"entryNodeId":"effect","nodes":[{"id":"effect","type":"effect","effects":[{"op":"add","target":{"kind":"attribute","attributeId":"skill"},"value":{"op":"literal","value":3}}],"next":"end"},{"id":"end","type":"end"}]})").object();
  auto follow=source;follow["id"]="scope-follow";follow["label"]="Follow-up workshop";follow["requirements"]=QJsonObject{{"op","eventCompleted"},{"eventId","scope-source"}};
  write("events/scope-source.json",source);write("events/scope-follow.json",follow);
  StudioWindow w(engine);w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  w.findChild<QAction*>("showAuthoringWorkspace")->trigger();auto dock=w.findChild<QDockWidget*>("authoringWorkspaceDock");QVERIFY(dock);auto tree=dock->findChild<QTreeWidget*>("workspaceEntities");QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&tree->topLevelItemCount()>3,20000);
  auto document=[&](QString id){for(auto v:w.currentView().value("documents").toArray())if(v.toObject().value("data").toObject().value("id")==id)return v.toObject().value("data").toObject();return QJsonObject{};};
  selectWorkspaceItem(dock,"event:scope-source");auto button=[&](QString n){auto b=dock->findChild<QPushButton*>(n);Q_ASSERT(b);return b;};
  const auto revision=w.currentView().value("revision");button("workspaceScopePreview")->click();auto preview=dock->findChild<QDialog*>("storyScopePreview");QVERIFY(preview);QVERIFY(!preview->isWindow());QCOMPARE(preview->findChild<QTreeWidget*>("scopeChanges")->topLevelItemCount(),2);QCOMPARE(w.currentView().value("revision"),revision);
  preview->reject();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(!dock->property("workspaceInputPending").toBool());QCOMPARE(document("scope-source"),source);
  // Reopening changed disk content can reuse a revision number. Compare the
  // entire preview snapshot too, so a stale plan cannot overwrite new content.
  button("workspaceScopePreview")->click();preview=dock->findChild<QDialog*>("storyScopePreview");QVERIFY(preview);
  auto external=source;external["priority"]=91;write("events/scope-source.json",external);w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);
  preview->findChild<QDialogButtonBox*>("scopeButtons")->button(QDialogButtonBox::Apply)->click();QCOMPARE(document("scope-source"),external);QCOMPARE(document("scope-follow"),follow);QVERIFY(!preview->findChild<QLabel*>("scopeProblems")->text().isEmpty());preview->reject();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
  write("events/scope-source.json",source);w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);button("workspaceReload")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);selectWorkspaceItem(dock,"event:scope-source");
  // The ordinary scope selector also routes through review, never a silent edit.
  characterChoose(dock->findChild<QComboBox*>("workspaceEventScope"),"actor");button("workspaceApply")->click();preview=dock->findChild<QDialog*>("storyScopePreview");QVERIFY(preview);QCOMPARE(document("scope-source"),source);
  preview->findChild<QDialogButtonBox*>("scopeButtons")->button(QDialogButtonBox::Apply)->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&document("scope-follow").value("scope")=="actor",15000);QTest::qWait(100);
  QCOMPARE(document("scope-source").value("nodes").toArray()[0].toObject().value("effects").toArray()[0].toObject().value("target").toObject().value("owner").toObject().value("kind"),QJsonValue("actor"));QCOMPARE(document("scope-follow").value("requirements").toObject().value("owner").toObject().value("kind"),QJsonValue("actor"));
  button("workspace-undoEdit")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&document("scope-source")==source,15000);QCOMPARE(document("scope-follow"),follow);QTest::qWait(100);
  button("workspace-redoEdit")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&document("scope-follow").value("scope")=="actor",15000);QTest::qWait(100);
  button("workspace-saveProject")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));const auto expectedSource=document("scope-source"),expectedFollow=document("scope-follow");
  w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(document("scope-source"),expectedSource);QCOMPARE(document("scope-follow"),expectedFollow);
}
