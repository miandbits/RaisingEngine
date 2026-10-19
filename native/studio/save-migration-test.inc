void StudioTests::saveMigrationReview(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()),evidence=qEnvironmentVariable("QT_DURABILITY_EVIDENCE",engine+"/.cache/durability");
  QDir().mkpath(evidence);
  QFile fixture(qEnvironmentVariable("QT_MIGRATION_FIXTURE",evidence+"/migration-ui.json"));QVERIFY(fixture.open(QIODevice::ReadOnly));auto paths=QJsonDocument::fromJson(fixture.readAll()).object();
  QFile original(paths.value("save").toString());QVERIFY(original.open(QIODevice::ReadOnly));auto bytes=original.readAll();original.close();
  StudioWindow w(engine);w.show();w.findChild<QAction*>("saveMigration")->trigger();
  auto dialog=w.findChild<QDialog*>("saveMigrationDialog");QVERIFY(dialog);
  for(auto pair:QList<QPair<QString,QString>>{{"migrationFrom","from"},{"migrationTo","to"},{"migrationSave","save"},{"migrationOutput","output"}})dialog->findChild<QLineEdit*>(pair.first)->setText(paths.value(pair.second).toString());
  auto preview=dialog->findChild<QPushButton*>("migrationPreview"),publish=dialog->findChild<QPushButton*>("migrationPublish");auto summary=dialog->findChild<QLabel*>("migrationSummary");
  QTest::mouseClick(preview,Qt::LeftButton);QTRY_VERIFY_WITH_TIMEOUT(preview->isEnabled(),30000);QVERIFY2(publish->isEnabled(),qPrintable(summary->text()));
  auto tree=dialog->findChild<QTreeWidget*>("migrationChanges");bool money=false;for(int i=0;i<tree->topLevelItemCount();++i)money|=tree->topLevelItem(i)->text(0)=="/attributes/money";QVERIFY(money);dialog->grab().save(evidence+"/migration-review.png");
  QTest::mouseClick(publish,Qt::LeftButton);QTRY_VERIFY_WITH_TIMEOUT(preview->isEnabled(),30000);QVERIFY2(summary->text().contains("새 저장 완료"),qPrintable(summary->text()));QVERIFY(QFileInfo::exists(paths.value("output").toString()));dialog->grab().save(evidence+"/migration-published.png");
  dialog->findChild<QLineEdit*>("migrationOutput")->setText(paths.value("save").toString());QVERIFY(!publish->isEnabled());QTest::mouseClick(preview,Qt::LeftButton);QTRY_VERIFY_WITH_TIMEOUT(preview->isEnabled(),30000);QVERIFY(publish->isEnabled());QTest::mouseClick(publish,Qt::LeftButton);QTRY_VERIFY_WITH_TIMEOUT(preview->isEnabled(),30000);QVERIFY2(summary->text().contains("MIGRATION_IO"),qPrintable(summary->text()));
  QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(original.readAll(),bytes);dialog->close();
}

void StudioTests::durabilityLargeProject(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()),evidence=qEnvironmentVariable("QT_DURABILITY_EVIDENCE",engine+"/.cache/durability");
  QDir().mkpath(evidence);
  QFile report(qEnvironmentVariable("QT_DURABILITY_REPORT",evidence+"/optimized-10000.json"));QVERIFY(report.open(QIODevice::ReadOnly));auto root=QJsonDocument::fromJson(report.readAll()).object().value("root").toString();QVERIFY(!root.isEmpty());
  StudioWindow w(engine);w.resize(1400,900);w.show();QElapsedTimer clock;clock.start();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),60000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));const auto openMs=clock.elapsed();
  qint64 searchMs=-1;int visible=0,rows=0;
  QTimer::singleShot(100,&w,[&]{auto d=w.findChild<QDialog*>("contentOrganizer");QVERIFY(d);auto search=d->findChild<QLineEdit*>("organizerSearch");auto table=d->findChild<QTableWidget*>("organizerTable");QVERIFY(search&&table);rows=table->rowCount();QVERIFY(rows>=3000);QElapsedTimer elapsed;elapsed.start();search->setText("durable-event-00999");searchMs=elapsed.elapsed();for(int i=0;i<rows;i++)if(!table->isRowHidden(i))++visible;QCOMPARE(visible,1);d->grab().save(evidence+"/qt-large-search.png");d->reject();});
  w.findChild<QAction*>("organizeContent")->trigger();QVERIFY(searchMs>=0);QVERIFY2(searchMs<2000,"Large-project GUI filtering exceeded 2 seconds");
  bool ruleEdited=false;
  if(!w.projectData().value("ruleExtensions").toObject().isEmpty()){
    QTimer::singleShot(100,&w,[&]{
      auto d=w.findChild<QDialog*>("ruleExtensionDialog");QVERIFY(d);
      auto name=d->findChild<QLineEdit*>("ruleProgressionName");auto gain=d->findChild<QSpinBox*>("ruleArg-gain");QVERIFY(name&&gain);
      name->setText("대규모 진행 편집 확인");gain->setValue(7);
      d->grab().save(evidence+"/qt-large-rule-edit.png");
      d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
    });
    w.findChild<QAction*>("editRuleExtensions")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),30000);
    auto first=w.projectData().value("ruleExtensions").toObject().value("progressions").toArray().first().toObject();
    QCOMPARE(first.value("name").toString(),QString("대규모 진행 편집 확인"));QCOMPARE(first.value("args").toObject().value("gain").toInt(),7);
    w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),60000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
    w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),60000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
    first=w.projectData().value("ruleExtensions").toObject().value("progressions").toArray().first().toObject();
    QCOMPARE(first.value("name").toString(),QString("대규모 진행 편집 확인"));QCOMPARE(first.value("args").toObject().value("gain").toInt(),7);ruleEdited=true;
  }
  QFile output(evidence+"/qt-large-project.json");QVERIFY(output.open(QIODevice::WriteOnly));output.write(QJsonDocument(QJsonObject{{"openMs",double(openMs)},{"searchMs",double(searchMs)},{"documentRows",rows},{"visibleRows",visible},{"ruleEditedSavedReopened",ruleEdited},{"ok",true}}).toJson());
}
