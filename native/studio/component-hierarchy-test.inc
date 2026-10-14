void StudioTests::componentHierarchyAuthoring(){
  const auto root=qEnvironmentVariable("RAISING_WORKBENCH_PROJECT");if(root.isEmpty())QSKIP("Set RAISING_WORKBENCH_PROJECT to a disposable fixture");
  StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()));w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().empty(),20000);const auto baseline=w.projectData();
  auto restore=qScopeGuard([&]{if(auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget()))d->reject();for(int i=0;i<5&&w.projectData()!=baseline;i++){w.findChild<QAction*>("undoEdit")->trigger();QElapsedTimer wait;wait.start();while(w.busy()&&wait.elapsed()<10000)QTest::qWait(20);}});
  QTimer::singleShot(100,&w,[&]{
    auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(d);QTimer::singleShot(15000,d,&QDialog::reject);
    auto type=d->findChild<QComboBox*>("componentTemplate");type->setCurrentIndex(type->findData("empty"));d->findChild<QPushButton*>("addComponent")->click();
    auto list=d->findChild<QListWidget*>("componentChildren");auto kind=d->findChild<QComboBox*>("componentChildKind");QVERIFY(kind);
    kind->setCurrentIndex(kind->findData("group"));d->findChild<QPushButton*>("addComponentChild")->click();
    d->findChild<QSpinBox*>("child-x")->setValue(80);d->findChild<QSpinBox*>("child-y")->setValue(20);d->findChild<QSpinBox*>("child-width")->setValue(200);d->findChild<QSpinBox*>("child-height")->setValue(100);
    kind->setCurrentIndex(kind->findData("button"));d->findChild<QPushButton*>("addComponentChild")->click();
    d->findChild<QSpinBox*>("child-x")->setValue(20);d->findChild<QSpinBox*>("child-y")->setValue(40);
    auto parent=d->findChild<QComboBox*>("child-parentId");auto at=parent->findData("element-1");QVERIFY(at>=0);parent->setCurrentIndex(at);QMetaObject::invokeMethod(parent,"activated",Q_ARG(int,at));
    QCOMPARE(d->findChild<QSpinBox*>("child-x")->value(),-60);QCOMPARE(d->findChild<QSpinBox*>("child-y")->value(),20);QVERIFY(d->findChild<QSpinBox*>("child-x")->minimum()<0);
    list->setCurrentRow(0);QCOMPARE(parent->findData("element-1"),-1);d->findChild<QPushButton*>("duplicateComponentChild")->click();QCOMPARE(list->count(),4);
    list->setCurrentRow(0);QTimer::singleShot(50,d,[]{auto box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());QVERIFY(box);auto yes=box->button(QMessageBox::Yes);QVERIFY(yes);QTest::mouseClick(yes,Qt::LeftButton);});d->findChild<QPushButton*>("deleteComponentChild")->click();QCOMPARE(list->count(),2);
    d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
  });
  w.findChild<QAction*>("editComponents")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  const auto before=baseline.value("visual").toObject().value("components").toArray(),after=w.projectData().value("visual").toObject().value("components").toArray();QCOMPARE(after.size(),before.size()+1);auto es=after.last().toObject().value("elements").toArray();QCOMPARE(es.size(),2);QCOMPARE(es[0].toObject().value("id").toString(),QString("element-1-copy1"));QCOMPARE(es[1].toObject().value("parentId").toString(),QString("element-1-copy1"));QCOMPARE(es[1].toObject().value("x").toInt(),-60);
  w.findChild<QAction*>("validateProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY(w.findChild<QLabel*>("validationSummary")->text().contains("오류 0건"));
}
