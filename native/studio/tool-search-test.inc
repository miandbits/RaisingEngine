void StudioTests::authoringToolSearch() {
  QWidget owner;QMenu menu(&owner);
  auto characters=menu.addMenu("인물 · 성장");auto roster=characters->addAction("다인 육성 · 활동 배정");roster->setToolTip("일정과 개인 능력");
  auto unavailable=characters->addAction("잠긴 도구");unavailable->setEnabled(false);
  QSignalSpy opened(roster,&QAction::triggered),blocked(unavailable,&QAction::triggered);
  QTimer::singleShot(30,&owner,[&]{
    auto dialog=owner.findChild<QDialog*>("authoringToolSearch");QVERIFY(dialog);
    auto query=dialog->findChild<QLineEdit*>("authoringToolQuery");auto results=dialog->findChild<QListWidget*>("authoringToolResults");QVERIFY(query&&results);
    query->setText("인물 일정");QCOMPARE(results->count(),1);QVERIFY(results->currentItem()->text().contains("활동 배정"));
    QTest::keyClick(query,Qt::Key_Return);
  });
  showAuthoringToolSearch(&owner,&menu);QCOMPARE(opened.count(),1);
  for(const QString query:{QString("not-a-tool"),QString("잠긴")}){
    QTimer::singleShot(30,&owner,[&]{
      auto dialog=owner.findChild<QDialog*>("authoringToolSearch");QVERIFY(dialog);
      dialog->findChild<QLineEdit*>("authoringToolQuery")->setText(query);
      auto buttons=dialog->findChild<QDialogButtonBox*>();QVERIFY(!buttons->button(QDialogButtonBox::Open)->isEnabled());buttons->button(QDialogButtonBox::Cancel)->click();
    });
    showAuthoringToolSearch(&owner,&menu);
  }
  QCOMPARE(opened.count(),1);QCOMPARE(blocked.count(),0);
  auto inspector=new QPushButton("조건별 표현",&owner);inspector->setObjectName("presentationVariants");inspector->hide();
  QSignalSpy inspectorOpened(inspector,&QPushButton::clicked);
  for(const bool enabled:{true,false}){
    inspector->setEnabled(enabled);
    QTimer::singleShot(30,&owner,[&]{
      auto dialog=owner.findChild<QDialog*>("authoringToolSearch");QVERIFY(dialog);
      auto query=dialog->findChild<QLineEdit*>("authoringToolQuery");query->setText("조건별");
      auto results=dialog->findChild<QListWidget*>("authoringToolResults");QCOMPARE(results->count(),1);
      auto buttons=dialog->findChild<QDialogButtonBox*>();QCOMPARE(buttons->button(QDialogButtonBox::Open)->isEnabled(),enabled);
      if(enabled)QTest::keyClick(query,Qt::Key_Return);else buttons->button(QDialogButtonBox::Cancel)->click();
    });
    showAuthoringToolSearch(&owner,&menu);
  }
  QCOMPARE(inspectorOpened.count(),1);
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());StudioWindow studio(engine);
  auto design=studio.findChild<QMenu*>("designToolsMenu");QVERIFY(design);QVERIFY(design->actions().size()<=12);
  auto group=design->findChild<QMenu*>("authoringCharacters");QVERIFY(group);
  QVERIFY(group->actions().contains(studio.findChild<QAction*>("editRoster")));
  auto search=studio.findChild<QAction*>("findAuthoringTool");QVERIFY(search);QCOMPARE(search->shortcut(),QKeySequence("Ctrl+K"));
  QVERIFY(design->actions().contains(studio.findChild<QAction*>("storyFlowAction")));
}
