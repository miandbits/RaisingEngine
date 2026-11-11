namespace {
void authorStarterForInstalledPilot(StudioWindow &window, const QString &evidence) {
  QImage artwork(320, 180, QImage::Format_RGB32);
  artwork.fill(QColor("#1d2d4d"));
  for (int y = 53; y < 107; ++y)
    for (int x = 108; x < 211; ++x)
      if (x < 144 || x >= 175) artwork.setPixelColor(x, y, QColor("#fbba57"));
  for (int y = 128; y < artwork.height(); ++y)
    for (int x = 0; x < artwork.width(); ++x)
      artwork.setPixelColor(x, y, QColor("#345e59"));
  const auto artPath = evidence + "/lantern-dusk.png";
  QVERIFY(artwork.save(artPath));
  QTimer::singleShot(0, &window, [&] {
    auto dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
    QVERIFY(dialog);
    QTimer::singleShot(5000, dialog, &QDialog::reject);
    dialog->selectFile(artPath);
    QMetaObject::invokeMethod(dialog, "accept");
  });
  window.findChild<QPushButton *>("importImage")->click();
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 20000);
  const auto assets = contentDocument(window, "assets/manifest.json")["assets"].toArray();
  QCOMPARE(assets.size(), 1);
  const auto assetId = assets.first().toObject()["id"].toString();
  QVERIFY(!assetId.isEmpty());

  bool edited = false;
  auto list = window.findChild<QListWidget *>("contentList");
  QVERIFY(list);
  for (int i = 0; i < list->count(); ++i)
    if (list->item(i)->data(Qt::UserRole).toString().endsWith("activities/study.json"))
      list->setCurrentRow(i);
  QVERIFY(list->currentItem());
  QVERIFY(list->currentItem()->data(Qt::UserRole).toString().endsWith("activities/study.json"));
  QTimer::singleShot(50, &window, [&] {
    auto dialog = window.findChild<QDialog *>("contentEditor");
    QVERIFY(dialog);
    QTimer::singleShot(15000, dialog, &QDialog::reject);
    dialog->findChild<QLineEdit *>("contentLabel")->setText("Lantern practice");
    auto effects = dialog->findChild<QTableWidget *>("content-baseEffects");
    QVERIFY(effects && effects->rowCount() > 0);
    auto reward = qobject_cast<QSpinBox *>(effects->cellWidget(0, 2));
    QVERIFY(reward);
    QCOMPARE(reward->value(), 10);
    reward->setValue(12);
    auto tabs = dialog->findChild<QTabWidget *>("contentTabs");
    auto presentation = dialog->findChild<QWidget *>("activityPresentationPage");
    QVERIFY(tabs && presentation);
    tabs->setCurrentWidget(presentation);
    auto background = presentation->findChild<QComboBox *>("activityScene-backgroundAssetId");
    QVERIFY(background && background->findData(assetId) >= 0);
    background->setCurrentIndex(background->findData(assetId));
    bool clipEdited = false;
    QTimer::singleShot(50, dialog, [&] {
      auto timeline = window.findChild<QDialog *>("timelineDialog");
      QVERIFY(timeline);
      QTimer::singleShot(5000, timeline, &QDialog::reject);
      auto clips = timeline->findChild<QTableWidget *>("timelineClips");
      QVERIFY(clips && clips->item(0, 3));
      auto duration = qobject_cast<QSpinBox *>(clips->cellWidget(0, 1));
      QVERIFY(duration);
      duration->setValue(3000);
      clips->item(0, 3)->setText("Lanterns at dusk — {activity}");
      timeline->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
      clipEdited = true;
    });
    presentation->findChild<QPushButton *>("activityTimelineCreate-timelineId")->click();
    QVERIFY(clipEdited);
    QVERIFY(!presentation->findChild<QComboBox *>("activityTimeline-timelineId")->currentData().toString().isEmpty());
    dialog->findChild<QDialogButtonBox *>(QString(), Qt::FindDirectChildrenOnly)->button(QDialogButtonBox::Save)->click();
    edited = true;
  });
  window.findChild<QAction *>("activityWorkflow")->trigger();
  QVERIFY(edited);
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 20000);
  QVERIFY2(window.lastError.isEmpty(), qPrintable(window.lastError));

  window.findChild<QAction *>("raisingScreenFlow")->trigger();
  auto flow = window.findChild<QDialog *>("raisingScreenFlowDialog");
  QVERIFY(flow);
  auto background = flow->findChild<QComboBox *>("raisingFlowBackground");
  QVERIFY(background && background->findData(assetId) >= 0);
  background->setCurrentIndex(background->findData(assetId));
  flow->findChild<QPushButton *>("raisingFlowPreview")->click();
  QCOMPARE(flow->findChild<QComboBox *>("raisingFlowPreviewScreen")->count(), 4);
  flow->findChild<QCheckBox *>("raisingFlowReviewed")->setChecked(true);
  auto apply = flow->findChild<QDialogButtonBox *>("raisingFlowButtons")->button(QDialogButtonBox::Apply);
  QVERIFY(apply->isEnabled());
  apply->click();
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy() && !window.findChild<QDialog *>("raisingScreenFlowDialog"), 20000);
  QVERIFY2(window.lastError.isEmpty(), qPrintable(window.lastError));
}

void restoreInstalledPilotBackup(StudioWindow &window, const QString &evidence, const QString &root) {
  const auto project = window.projectData();
  const auto activity = contentDocument(window, "activities/study.json");
  QCOMPARE(activity["label"].toString(), QString("Lantern practice"));
  window.findChild<QAction *>("showReleaseCenter")->trigger();
  auto center = window.findChild<QDialog *>("releaseCenter");
  QVERIFY(center);
  auto registerRelease = center->findChild<QPushButton *>("releaseRegister");
  QVERIFY(registerRelease);
  center->findChild<QLineEdit *>("releaseDirectory")->setText(evidence + "/web");
  center->findChild<QLineEdit *>("releaseName")->setText("Installed creator pilot");
  QTRY_VERIFY_WITH_TIMEOUT(registerRelease->isEnabled(), 15000);
  registerRelease->click();
  auto restore = center->findChild<QPushButton *>("releaseRestoreBackup");
  QTRY_VERIFY_WITH_TIMEOUT(restore->isEnabled(), 15000);
  auto tabs = center->findChild<QTabWidget *>("releaseTabs");
  tabs->setCurrentWidget(restore->parentWidget());
  const auto restoredRoot = evidence + "/restored starter";
  center->findChild<QLineEdit *>("releaseRestoreDestination")->setText(restoredRoot);
  restore->click();
  auto status = center->findChild<QLabel *>("releaseWorkflowStatus");
  QTRY_VERIFY_WITH_TIMEOUT(status->property("jobStatus") == "completed", 30000);
  QTRY_VERIFY_WITH_TIMEOUT(center->findChild<QPushButton *>("releaseOpenRestored")->isEnabled(), 15000);
  center->close();
  window.openProject(restoredRoot);
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy() &&
    QFileInfo(window.currentView()["root"].toString()).canonicalFilePath() == QFileInfo(restoredRoot).canonicalFilePath(), 30000);
  QVERIFY(QFileInfo::exists(root+"/project.json"));
  QCOMPARE(window.projectData(), project);
  QCOMPARE(contentDocument(window, "activities/study.json"), activity);
}
}
void StudioTests::releasePackageWorkflow(){
  if(qEnvironmentVariableIsEmpty("RAISING_RELEASE_EVIDENCE"))QSKIP("Requires an isolated installed preview");
  const auto evidence=qEnvironmentVariable("RAISING_RELEASE_EVIDENCE"),engine=studioEngineRoot();
#ifdef Q_OS_WIN
  QVERIFY(QFileInfo::exists(engine+"/node.exe"));QVERIFY(engine.startsWith(QCoreApplication::applicationDirPath()));
#endif
  const bool pilot = qEnvironmentVariableIsSet("RAISING_RELEASE_CREATOR_PILOT");
  QDir().mkpath(evidence);qputenv("RAISING_PROJECT_HISTORY",(evidence+"/history.ini").toUtf8());
  const auto root=evidence+"/new game 한글";
  ProjectManager manager(engine);manager.show();
  auto base=manager.findChild<QComboBox*>("launcherTemplate");QVERIFY(base);
  const auto templateIndex = base->findData(pilot ? "my-first-raising-game" : "academy-adventure");
  QVERIFY(templateIndex >= 0);
  base->setCurrentIndex(templateIndex);
  manager.findChild<QLineEdit*>("launcherDestination")->setText(root);
  manager.findChild<QPushButton*>("launcherCreate")->click();
  QTRY_COMPARE_WITH_TIMEOUT(manager.result(),int(QDialog::Accepted),30000);QVERIFY(QFileInfo::exists(root+"/project.json"));
  StudioWindow window(engine);window.show();window.openProject(root);
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy()&&!window.projectData().isEmpty(),30000);QVERIFY2(window.lastError.isEmpty(),qPrintable(window.lastError));
  const auto initialActivity = contentDocument(window, pilot ? "activities/study.json" : "activities/rest.json");
  QJsonObject identity;
  if (pilot) {
    QTRY_VERIFY(window.findChild<QDockWidget *>("firstGameChecklist"));
    window.findChild<QAction *>("showEditorIdentity")->trigger();
    auto dialog = window.findChild<QDialog *>("editorIdentityDialog");
    QVERIFY(dialog);
    auto details = dialog->findChild<QPlainTextEdit *>("editorIdentityDetails");
    QTRY_VERIFY_WITH_TIMEOUT(!details->toPlainText().isEmpty(), 15000);
    identity = QJsonDocument::fromJson(details->toPlainText().toUtf8()).object();
    QVERIFY(!identity.isEmpty());
    if (editorLanguage() == "en") QCOMPARE(dialog->windowTitle(), QString("About Raising Studio"));
    if (editorLanguage() == "ko") QCOMPARE(dialog->windowTitle(), QString("Raising Studio 정보"));
    QVERIFY(dialog->grab().save(evidence+"/editor-identity.png"));
    dialog->close();
    authorStarterForInstalledPilot(window, evidence);
    QVERIFY(!QTest::currentTestFailed());
  } else {
    QTimer::singleShot(0,&window,[&]{auto d=window.findChild<QDialog*>("contentEditor");QVERIFY(d);d->findChild<QLineEdit*>("contentLabel")->setText("설치본에서 만든 휴식");d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});
    QVERIFY(openContentDocument(window,"activities/rest.json"));QTRY_VERIFY_WITH_TIMEOUT(!window.busy(),10000);
  }
  const auto editedProject = window.projectData();
  const auto editedActivity = contentDocument(window, pilot ? "activities/study.json" : "activities/rest.json");
  QVERIFY(editedActivity != initialActivity);
  window.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!window.busy(),30000);QVERIFY2(window.lastError.isEmpty(),qPrintable(window.lastError));
  window.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!window.busy(),30000);
  QCOMPARE(window.projectData(), editedProject);
  QCOMPARE(contentDocument(window, pilot ? "activities/study.json" : "activities/rest.json"), editedActivity);
  window.grab().save(evidence+"/installed-editor.png");
#ifdef Q_OS_MACOS
  const QString desktopTarget = "macos";
#else
  const QString desktopTarget = "windows";
#endif
  for(const auto &targetId:QStringList{desktopTarget,"web"}){
    bool exported=false;
    QTimer::singleShot(0,&window,[&]{
      auto d=window.findChild<QDialog*>("exportGameDialog");QVERIFY(d);
      const auto closeDialog=qScopeGuard([d]{d->reject();});
      auto target=d->findChild<QComboBox*>("exportTarget");target->setCurrentIndex(target->findData(targetId));
      d->findChild<QLineEdit*>("exportParent")->setText(evidence);d->findChild<QLineEdit*>("exportFolderName")->setText(targetId);
      if (pilot) {
        d->findChild<QLineEdit *>("exportVersion")->setText("0.1.0");
        QVERIFY(d->findChild<QCheckBox *>("exportArchives")->isChecked());
      }
      d->findChild<QPushButton*>("exportStart")->click();QTRY_VERIFY_WITH_TIMEOUT(!window.busy()&&d->findChild<QPushButton*>("exportStart")->isEnabled(),60000);
      QFile exportStatus(evidence+"/export-"+targetId+".json");
      QVERIFY(exportStatus.open(QIODevice::WriteOnly));
      exportStatus.write(QJsonDocument(QJsonObject{{"target",targetId},{"message",d->findChild<QLabel*>("exportResult")->text()},{"exported",d->findChild<QPushButton*>("exportOpenFolder")->isEnabled()}}).toJson());exportStatus.close();
      QVERIFY2(d->findChild<QPushButton*>("exportOpenFolder")->isEnabled(),qPrintable(d->findChild<QLabel*>("exportResult")->text()));
      d->grab().save(evidence+"/export-"+targetId+".png");exported=true;d->reject();
    });
    window.findChild<QAction*>("exportGame")->trigger();QVERIFY(exported);
    if (pilot) {
      for (const auto *name : {"Game.zip", "Project-backup.zip", "RELEASE.json"})
        QVERIFY(QFileInfo::exists(evidence+"/"+targetId+"/"+name));
      continue;
    }
    QFile game(evidence+"/"+targetId+"/game.json");QVERIFY(game.open(QIODevice::ReadOnly));
    auto content=QJsonDocument::fromJson(game.readAll()).object().value("content").toObject();bool found=false;
    for(const auto &value:content.value("activities").toArray()){auto a=value.toObject();if(a.value("id")=="rest"){QCOMPARE(a.value("label").toString(),QString("설치본에서 만든 휴식"));found=true;}}QVERIFY(found);
  }
  if (pilot) {
    restoreInstalledPilotBackup(window, evidence, root);
    QVERIFY(!QTest::currentTestFailed());
  }
  QJsonObject proof{{"ok",true},{"engine",engine},{"project",root},{"createdInGui",true},{"editedInGui",true}};
  if (pilot) {
    proof["restoredProject"] = evidence + "/restored starter";
    proof["editorIdentity"] = identity;
    proof["editorLanguage"] = editorLanguage();
    proof["exports"] = QJsonArray{desktopTarget, "web"};
  } else {
    proof["exportedWindowsAndWeb"] = true;
  }
  QFile result(evidence+"/qt-workflow.json");QVERIFY(result.open(QIODevice::WriteOnly));result.write(QJsonDocument(proof).toJson());
}
