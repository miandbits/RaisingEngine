void StudioTests::galleryAuthoring(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  const auto root=engine+"/.cache/gallery-editor-"+QString::number(QDateTime::currentMSecsSinceEpoch());
  const auto evidence=engine+"/.cache/gallery";QDir().mkpath(evidence);
  StudioWindow w(engine);w.resize(1500,960);w.show();
  QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("newProjectDialog");QVERIFY(d);auto mode=d->findChild<QComboBox*>("newProjectTemplate");mode->setCurrentIndex(mode->findData("existing"));d->findChild<QLineEdit*>("newProjectSource")->setText(engine+"/examples/last-grandson-polished");d->findChild<QLineEdit*>("newProjectDestination")->setText(root);d->findChild<QDialogButtonBox*>("newProjectButtons")->button(QDialogButtonBox::Ok)->click();});
  w.findChild<QAction*>("newProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),30000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  const auto original=w.projectData();
  QTimer::singleShot(50,&w,[&]{
    auto d=w.findChild<QDialog*>("projectSettingsDialog");QVERIFY(d);d->resize(1100,720);
    auto tabs=d->findChild<QTabWidget*>();tabs->setCurrentIndex(2);
    d->findChild<QCheckBox*>("projectGalleryEnabled")->setChecked(true);d->findChild<QLineEdit*>("projectGalleryTitle")->setText("설이의 추억 · 제작 확인");
    auto table=d->findChild<QTableWidget*>("projectGalleryEntries");table->setRowCount(0);
    const QStringList ids={"promise","survival","farewell"},sources={"event:exam-invitation","ending:certified","ending:ordinary"},titles={"약방 노인의 제안","겨울을 넘어","조용한 작별"},thumbs={"winter-courtyard","seol-neutral","seol-rest"};
    for(int row=0;row<3;row++){
      d->findChild<QPushButton*>("projectGalleryAdd")->click();
      table->item(row,0)->setText(ids[row]);table->item(row,2)->setText(titles[row]);table->item(row,3)->setText(row==0?"인연":"결말");table->item(row,4)->setText(row==0?"첫 인연의 이야기를 끝까지 보세요.":"두 달의 삶을 마무리하세요.");
      auto source=qobject_cast<QComboBox*>(table->cellWidget(row,1));QVERIFY(source->findData(sources[row])>=0);source->setCurrentIndex(source->findData(sources[row]));
      auto thumb=qobject_cast<QComboBox*>(table->cellWidget(row,5));QVERIFY(thumb->findData(thumbs[row])>=0);thumb->setCurrentIndex(thumb->findData(thumbs[row]));
    }
    table->selectRow(1);d->findChild<QPushButton*>("projectGalleryUp")->click();QCOMPARE(table->item(0,0)->text(),QString("survival"));d->findChild<QPushButton*>("projectGalleryDown")->click();QCOMPARE(table->item(0,0)->text(),QString("promise"));
    d->grab().save(evidence+"/editor-gallery.png");
    auto accept=d->findChild<QDialogButtonBox*>("projectSettingsButtons")->button(QDialogButtonBox::Ok);
    table->item(1,0)->setText("promise");accept->click();QVERIFY(!d->findChild<QLabel*>("projectSettingsError")->text().isEmpty());table->item(1,0)->setText("survival");
    d->findChild<QPushButton*>("projectGalleryAdd")->click();table->selectRow(3);d->findChild<QPushButton*>("projectGalleryRemove")->click();QCOMPARE(table->rowCount(),3);
    accept->click();
  });
  w.findChild<QAction*>("projectSettings")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);
  const auto gallery=w.projectData().value("gallery").toObject();QCOMPARE(gallery.value("entries").toArray().size(),3);QVERIFY(w.projectData()!=original);
  w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),original);
  w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData().value("gallery").toObject(),gallery);
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);QCOMPARE(w.projectData().value("gallery").toObject(),gallery);
  w.findChild<QAction*>("validateProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);for(const auto& d:w.currentView().value("diagnostics").toArray())QVERIFY2(d.toObject().value("severity")!="error",qPrintable(QString::fromUtf8(QJsonDocument(d.toObject()).toJson())));
  QFile proof(evidence+"/editor-project.txt");QVERIFY(proof.open(QIODevice::WriteOnly));proof.write(root.toUtf8());
}
