void StudioTests::conditionalRoutingAuthoring() {
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()),project=qEnvironmentVariable("RAISING_QT_PROJECT");QVERIFY(!project.isEmpty());
  StudioWindow w(engine);w.show();w.openProject(project);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  auto settle=[&]{QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);};
  auto openScreen=[&](QString id){auto list=w.findChild<QListWidget*>("projectList");bool found=false;for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole)==id){list->setCurrentRow(i);list->setFocus();QTest::keyClick(list,Qt::Key_Return);found=true;break;}QVERIFY(found);};
  const auto group=w.projectData().value("stateGroups").toArray().first().toObject();const auto values=group.value("values").toArray();QCOMPARE(values.size(),3);
  QStringList created;
  for(const auto& name:{QString("수련 화면"),QString("연회 화면")}){
    openScreen("screen-1");QTimer::singleShot(80,&w,[=]{auto d=qobject_cast<QInputDialog*>(QApplication::activeModalWidget());QVERIFY(d);d->setTextValue(name);QTest::keyClick(d->findChild<QLineEdit*>(),Qt::Key_Return);});
    w.findChild<QAction*>("duplicateScreen")->trigger();settle();auto tabs=w.findChild<QTabBar*>("documentTabs");created<<tabs->tabData(tabs->currentIndex()).toString();
  }
  QVERIFY(created[0]!="screen-1");QVERIFY(created[0]!=created[1]);
  w.findChild<QAction*>("screenRouting")->trigger();auto outer=w.findChild<QDialog*>("screenRoutingDialog");QVERIFY(outer);QVERIFY(outer->findChild<QLabel*>("routingPriority")->text().contains("명시적"));outer->findChild<QTabWidget*>("routingTabs")->setCurrentIndex(1);
  for(int i=0;i<3;++i){
    outer->findChild<QPushButton*>("routeAdd")->click();auto d=outer->findChild<QDialog*>("routingConditionEdit");QVERIFY(d);d->findChild<QLineEdit*>("routeName")->setText(i==0?"수련 우선":i==1?"겹치는 수련 연결":"연회 연결");auto target=d->findChild<QComboBox*>("routeScreen");target->setCurrentIndex(target->findData(i==0?created[0]:i==1?"screen-1":created[1]));
    auto scope=d->findChild<QListWidget*>("routePhases");for(int n=0;n<scope->count();++n)scope->item(n)->setCheckState(Qt::Unchecked);
    d->findChild<QPushButton*>("routeCondition")->click();auto logic=static_cast<LogicEditor*>(d->findChild<QDialog*>("logicEditor"));QVERIFY(logic);QVERIFY(logicTestChoose(*logic,"logicType","stateIs"));QVERIFY(logicTestChoose(*logic,"logic-stateGroupId",group.value("id").toString()));QVERIFY(logicTestChoose(*logic,"logic-stateValueId",values[i<2?1:2].toObject().value("id").toString()));logic->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
    d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();QVERIFY(!d->isVisible());QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
  }
  auto list=outer->findChild<QListWidget*>("routingConditions");QCOMPARE(list->count(),3);list->setCurrentRow(0);outer->findChild<QPushButton*>("routeDown")->click();QVERIFY(list->item(0)->text().contains("겹치는"));outer->findChild<QPushButton*>("routeUp")->click();QVERIFY(list->item(0)->text().contains("수련 우선"));
  outer->findChild<QPushButton*>("routingDebug")->click();settle();QVERIFY(w.findChild<QPushButton*>("debugStart"));const auto routes=w.projectData().value("visual").toObject().value("routing").toObject().value("conditions").toArray();QCOMPARE(routes.size(),3);QCOMPARE(routes[0].toObject().value("when").toObject().value("op").toString(),QString("stateIs"));
  w.findChild<QAction*>("saveProject")->trigger();settle();QVERIFY(!w.currentView().value("dirty").toBool());w.openProject(project);settle();QCOMPARE(w.projectData().value("visual").toObject().value("routing").toObject().value("conditions").toArray(),routes);
  // Inspect the real C++ state through the same debugger a designer uses.
  w.findChild<QAction*>("screenRouting")->trigger();outer=w.findChild<QDialog*>("screenRoutingDialog");outer->findChild<QPushButton*>("routingDebug")->click();settle();
  auto start=w.findChild<QPushButton*>("debugStart");QVERIFY(start);start->click();settle();auto tree=w.findChild<QTreeWidget*>("debugRouting");QVERIFY(tree);QTRY_VERIFY_WITH_TIMEOUT(tree->topLevelItemCount()>=4,10000);QCOMPARE(tree->topLevelItem(3)->text(1),QString("선택"));
  auto command=w.findChild<QComboBox*>("debugCommandType");command->setCurrentIndex(command->findData("setSchedule"));auto target=w.findChild<QComboBox*>("debugCommandTarget");target->setCurrentIndex(target->findData("rest"));w.findChild<QPushButton*>("debugExecute")->click();settle();command->setCurrentIndex(command->findData("executeActivity"));w.findChild<QPushButton*>("debugExecute")->click();settle();QCOMPARE(tree->topLevelItem(0)->text(1),QString("선택"));QCOMPARE(tree->topLevelItem(1)->text(1),QString("일치 · 상위 연결 우선"));
  auto tabs=qobject_cast<QTabWidget*>(tree->parentWidget()->parentWidget());if(tabs)tabs->setCurrentWidget(tree);
  QVERIFY(w.findChild<QDockWidget*>("ruleDebugDock")->grab().save(QDir(engine).filePath(".cache/conditional-routing/debug-routing.png")));
  QFile info(QDir(engine).filePath(".cache/conditional-routing/authoring.json"));QVERIFY(info.open(QIODevice::WriteOnly));info.write(QJsonDocument(QJsonObject{{"project",project},{"trainingScreen",created[0]},{"banquetScreen",created[1]}}).toJson());info.close();
  // Exercise production Play export from this exact editor-authored project.
  const auto smoke=QDir(engine).filePath(".cache/conditional-routing/editor-smoke.json");QFile steps(smoke);QVERIFY(steps.open(QIODevice::WriteOnly));steps.write("[{\"checkpoint\":\"authored-start\"}]");steps.close();w.playAutomation=smoke;w.playReport=QDir(engine).filePath(".cache/conditional-routing/editor-smoke-report.json");QSignalSpy exited(&w,&StudioWindow::playerFinished);w.findChild<QAction*>("playProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),60000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(w.playReport),15000);QTRY_VERIFY_WITH_TIMEOUT(exited.count()>0,10000);
}
