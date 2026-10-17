void StudioTests::authoringStatePreview(){
  if(!qEnvironmentVariableIsSet("RAISING_QT_AUTHORING_PREVIEW"))QSKIP("Requires a disposable component-context project.");
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()),root=qEnvironmentVariable("RAISING_QT_PROJECT");
  StudioWindow w(engine);w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  w.findChild<QAction*>("showAuthoringWorkspace")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);
  auto workspace=w.findChild<QDockWidget*>("authoringWorkspaceDock");QVERIFY(workspace);selectWorkspaceItem(workspace,"screenElement:screen-1/instance-1");workspace->findChild<QPushButton*>("workspaceCanvas")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);
  // Author a real draft through the same palette/property widgets used by designers.
  w.findChild<QToolButton*>("elementPalette")->menu()->actions().first()->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&w.selectedId().startsWith("text-"),10000);
  auto text=w.findChild<QLineEdit*>("buttonText");QVERIFY(text);text->setFocus();text->selectAll();QTest::keyClicks(text,"Preview authoring proof");QTest::keyClick(text,Qt::Key_Return);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  const auto before=w.currentView();QVERIFY(before["dirty"].toBool());const auto newText=w.selectedId();
  auto dock=w.findChild<RendererPreviewDock*>("rendererPreviewDock");QVERIFY(dock);dock->show();dock->raise();
  auto target=dock->findChild<QComboBox*>("rendererElement");target->setCurrentIndex(target->findData("instance-1__element-1"));
  auto configure=[&](int skill,QString activity){QTimer::singleShot(30,&w,[&,skill,activity]{auto dialog=dock->findChild<QDialog*>("authoringPreviewDialog");QVERIFY(dialog);dialog->findChild<QCheckBox*>("previewScenarioEnabled")->setChecked(true);auto actor=dialog->findChild<QComboBox*>("previewActor");actor->setCurrentIndex(actor->findData("trainee-2"));auto field=dialog->findChild<QComboBox*>("previewActivity");field->setCurrentIndex(field->findData(activity));dialog->findChild<QCheckBox*>("previewUseAttribute-skill")->setChecked(true);dialog->findChild<QSpinBox*>("previewAttribute-skill")->setValue(skill);auto group=w.projectData()["stateGroups"].toArray().first().toObject();auto state=dialog->findChild<QComboBox*>("previewState-"+group["id"].toString());state->setCurrentIndex(state->findData(group["values"].toArray().last().toObject()["id"].toString()));dialog->grab().save(engine+"/.cache/authoring-preview-form.png");dialog->findChild<QDialogButtonBox*>("previewScenarioButtons")->button(QDialogButtonBox::Apply)->click();});dock->findChild<QPushButton*>("configureAuthoringPreview")->click();};
  QString banquet,training;for(auto value:w.currentView()["documents"].toArray()){auto doc=value.toObject(),data=doc["data"].toObject();if(doc["kind"]=="activity"&&data["label"].toString().contains("연회"))banquet=data["id"].toString();}training=w.projectData()["roster"].toObject()["members"].toArray().first().toObject()["activityId"].toString();QVERIFY(!banquet.isEmpty());
  auto render=[&]{QSignalSpy ready(dock,&RendererPreviewDock::snapshotReady);dock->findChild<QPushButton*>("renderPreview")->click();QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(),60000);QVERIFY2(ready.last().first().toBool(),qPrintable(dock->findChild<QLabel*>("rendererStatus")->text()));};
  configure(15,banquet);render();auto result=dock->result();QCOMPARE(result["preview"].toObject()["stateSource"].toString(),QString("authoring"));
  QCOMPARE(result["texts"].toObject()["instance-1__element-2"].toString(),QString("연화"));QCOMPARE(result["texts"].toObject()["instance-1__element-3"].toString(),QString("검술: 15"));QCOMPARE(result["texts"].toObject()[newText].toString(),QString("Preview authoring proof"));
  auto enabled=[&](QString id){for(auto value:dock->result()["elements"].toArray())if(value.toObject()["id"]==id)return value.toObject()["enabled"].toBool();return false;};QVERIFY(!enabled("instance-1__element-5"));
  auto firstText=result["texts"].toObject()["instance-1__element-1"].toString();QVERIFY(!firstText.isEmpty());QVERIFY(dock->findChild<QTreeWidget*>("previewReasons")->topLevelItemCount()>1);
  configure(45,training);render();QVERIFY(enabled("instance-1__element-5"));QVERIFY(dock->result()["texts"].toObject()["instance-1__element-1"].toString()!=firstText);
  auto size=dock->findChild<QComboBox*>("rendererResolution");size->setCurrentIndex(3);QMetaObject::invokeMethod(size,"activated",Q_ARG(int,3));render();QCOMPARE(QImage(QFileInfo(dock->reportPath()).absolutePath()+"/preview.png").width(),720);
  for(auto field:{"revision","dirty","undo","redo","documents"})QCOMPARE(w.currentView()[field],before[field]);
  dock->grab().save(engine+"/.cache/authoring-preview-render.png");
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.currentView()["dirty"].toBool(),20000);
}
