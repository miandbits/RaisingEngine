void StudioTests::rosterAuthoringRoundtrip() {
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  const auto root=qEnvironmentVariable("RAISING_ROSTER_PROJECT",engine+"/.cache/roster-authored-"+QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(!QFileInfo::exists(root));StudioWindow w(engine);w.show();
  bool created=false;
  QTimer::singleShot(100,&w,[&]{auto d=w.findChild<QDialog*>("newProjectDialog");QVERIFY(d);auto mode=d->findChild<QComboBox*>("newProjectTemplate");mode->setCurrentIndex(mode->findData("academy-visual"));d->findChild<QLineEdit*>("newProjectDestination")->setText(root);created=true;d->findChild<QDialogButtonBox*>("newProjectButtons")->button(QDialogButtonBox::Ok)->click();});
  w.findChild<QAction*>("newProject")->trigger();QVERIFY(created);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  const auto before=w.currentView().value("documents");
  QDir().mkpath(engine+"/docs/evidence/roster");
  bool authored=false;
  QTimer::singleShot(100,&w,[&]{auto d=w.findChild<QDialog*>("rosterAuthoringDialog");QVERIFY(d);QTimer::singleShot(10000,d,&QDialog::reject);
    auto locals=d->findChild<QListWidget*>("rosterLocalAttributes");QVERIFY(locals);
    for(int i=0;i<locals->count();i++){auto item=locals->item(i);const auto id=item->data(Qt::UserRole).toString();item->setCheckState(id=="skill"||id=="stamina"||id=="stress"?Qt::Checked:Qt::Unchecked);}
    auto table=d->findChild<QTableWidget*>("rosterMembers");QVERIFY(table);d->findChild<QPushButton*>("rosterAddMember")->click();QCOMPARE(table->rowCount(),2);
    table->item(0,1)->setText(QString::fromUtf8("민서"));table->item(1,1)->setText(QString::fromUtf8("하린"));
    for(int row=0;row<2;row++){auto combo=qobject_cast<QComboBox*>(table->cellWidget(row,2));combo->setCurrentIndex(combo->findData(row==0?"study":"rest"));}
    // These are visible controls, not generated project JSON.
    for(int col=3;col<table->columnCount();col++){const auto label=table->horizontalHeaderItem(col)->text();
      if(label==QString::fromUtf8("검술")){qobject_cast<QSpinBox*>(table->cellWidget(0,col))->setValue(10);qobject_cast<QSpinBox*>(table->cellWidget(1,col))->setValue(40);}
      if(label==QString::fromUtf8("체력")){qobject_cast<QSpinBox*>(table->cellWidget(0,col))->setValue(80);qobject_cast<QSpinBox*>(table->cellWidget(1,col))->setValue(30);}
      if(label==QString::fromUtf8("스트레스"))qobject_cast<QSpinBox*>(table->cellWidget(1,col))->setValue(20);
    }
    d->grab().save(engine+"/docs/evidence/roster/authoring.png");authored=true;d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
  });
  w.findChild<QAction*>("editRoster")->trigger();QVERIFY(authored);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  const auto roster=w.projectData()["roster"].toObject();QCOMPARE(roster["members"].toArray().size(),2);QVERIFY(!roster["localAttributeIds"].toArray().contains("money"));
  w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.currentView()["documents"],before);
  w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData()["roster"].toObject(),roster);
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData()["roster"].toObject(),roster);
  w.grab().save(engine+"/docs/evidence/roster/editor-reopened.png");
  QFile proof(engine+"/.cache/roster-authoring-proof.json");QVERIFY(proof.open(QIODevice::WriteOnly));proof.write(QJsonDocument(QJsonObject{{"project",root},{"createdThroughWizard",true},{"authoredThroughWidgets",true},{"undoRedo",true},{"saveReopen",true},{"roster",roster}}).toJson());
}

void StudioTests::rosterPlayIntegration() {
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  QFile proof(engine+"/.cache/roster-authoring-proof.json");QVERIFY(proof.open(QIODevice::ReadOnly));
  const auto root=QJsonDocument::fromJson(proof.readAll()).object().value("project").toString();
  StudioWindow w(engine);w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  w.playAutomation=engine+"/docs/evidence/roster/play-steps.json";
  w.playReport=engine+"/docs/evidence/roster/editor-play-report.json";
  QVERIFY(QFileInfo::exists(w.playAutomation));QFile::remove(w.playReport);
  QSignalSpy finished(&w,&StudioWindow::playerFinished);w.findChild<QAction*>("playProject")->trigger();
  QTRY_VERIFY_WITH_TIMEOUT(finished.count()>0,60000);QVERIFY(QFileInfo::exists(w.playReport));
  QFile report(w.playReport);QVERIFY(report.open(QIODevice::ReadOnly));
  const auto result=QJsonDocument::fromJson(report.readAll()).object();QVERIFY2(result.value("ok").toBool(),report.readAll().constData());
  int checkpoints=0;
  for(const auto entry:result.value("steps").toArray()){
    const auto e=entry.toObject();if(!e.value("command").toObject().contains("checkpoint"))continue;
    QCOMPARE(e.value("ui").toObject().value("state").toObject().value("roster").toObject().value("attributes").toObject().size(),2);++checkpoints;
  }
  QVERIFY(checkpoints>=6);w.grab().save(engine+"/docs/evidence/roster/editor-play-completed.png");
}
