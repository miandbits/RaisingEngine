void StudioTests::releasePackageWorkflow(){
  if(qEnvironmentVariableIsEmpty("RAISING_RELEASE_EVIDENCE"))QSKIP("Requires an isolated installed preview");
  const auto evidence=qEnvironmentVariable("RAISING_RELEASE_EVIDENCE"),engine=studioEngineRoot();
  QVERIFY(QFileInfo::exists(engine+"/node.exe"));QVERIFY(engine.startsWith(QCoreApplication::applicationDirPath()));
  QDir().mkpath(evidence);qputenv("RAISING_PROJECT_HISTORY",(evidence+"/history.ini").toUtf8());
  const auto root=evidence+"/new game 한글";
  ProjectManager manager(engine);manager.show();
  auto base=manager.findChild<QComboBox*>("launcherTemplate");QVERIFY(base);base->setCurrentIndex(base->findData("academy-adventure"));
  manager.findChild<QLineEdit*>("launcherDestination")->setText(root);
  manager.findChild<QPushButton*>("launcherCreate")->click();
  QTRY_COMPARE_WITH_TIMEOUT(manager.result(),int(QDialog::Accepted),30000);QVERIFY(QFileInfo::exists(root+"/project.json"));
  StudioWindow window(engine);window.show();window.openProject(root);
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy()&&!window.projectData().isEmpty(),30000);QVERIFY2(window.lastError.isEmpty(),qPrintable(window.lastError));
  QTimer::singleShot(0,&window,[&]{auto d=window.findChild<QDialog*>("contentEditor");QVERIFY(d);d->findChild<QLineEdit*>("contentLabel")->setText("설치본에서 만든 휴식");d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});
  QVERIFY(openContentDocument(window,"activities/rest.json"));QTRY_VERIFY_WITH_TIMEOUT(!window.busy(),10000);
  window.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!window.busy(),30000);QVERIFY2(window.lastError.isEmpty(),qPrintable(window.lastError));
  window.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!window.busy(),30000);
  window.grab().save(evidence+"/installed-editor.png");
  for(const auto &targetId:QStringList{"windows","web"}){
    bool exported=false;
    QTimer::singleShot(0,&window,[&]{
      auto d=window.findChild<QDialog*>("exportGameDialog");QVERIFY(d);
      auto target=d->findChild<QComboBox*>("exportTarget");target->setCurrentIndex(target->findData(targetId));
      d->findChild<QLineEdit*>("exportParent")->setText(evidence);d->findChild<QLineEdit*>("exportFolderName")->setText(targetId);
      d->findChild<QPushButton*>("exportStart")->click();QTRY_VERIFY_WITH_TIMEOUT(!window.busy(),60000);
      QVERIFY2(d->findChild<QPushButton*>("exportOpenFolder")->isEnabled(),qPrintable(d->findChild<QLabel*>("exportResult")->text()));
      d->grab().save(evidence+"/export-"+targetId+".png");exported=true;d->reject();
    });
    window.findChild<QAction*>("exportGame")->trigger();QVERIFY(exported);
    QFile game(evidence+"/"+targetId+"/game.json");QVERIFY(game.open(QIODevice::ReadOnly));
    auto content=QJsonDocument::fromJson(game.readAll()).object().value("content").toObject();bool found=false;
    for(const auto &value:content.value("activities").toArray()){auto a=value.toObject();if(a.value("id")=="rest"){QCOMPARE(a.value("label").toString(),QString("설치본에서 만든 휴식"));found=true;}}QVERIFY(found);
  }
  QFile result(evidence+"/qt-workflow.json");QVERIFY(result.open(QIODevice::WriteOnly));result.write(QJsonDocument(QJsonObject{{"ok",true},{"engine",engine},{"project",root},{"createdInGui",true},{"editedInGui",true},{"exportedWindowsAndWeb",true}}).toJson());
}
