void StudioTests::advanceButtonAuthoring() {
  StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath()));
  const auto root = qEnvironmentVariable("RAISING_QT_PROJECT");
  QVERIFY(!root.isEmpty());
  w.openProject(root);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && !w.projectData().isEmpty(), 20000);
  w.findChild<QAction *>("addButton")->trigger();
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  const auto id = w.selectedId();
  auto button = [&]() {
    for (auto screen : w.projectData()["visual"].toObject()["screens"].toArray())
      if (screen.toObject()["id"] == "home")
        for (auto element : screen.toObject()["elements"].toArray())
          if (element.toObject()["id"] == id) return element.toObject();
    return QJsonObject();
  };
  auto action = w.findChild<QComboBox *>("buttonAction");
  const int advanceIndex = action->findData("advance");
  QVERIFY(advanceIndex >= 0);
  action->setCurrentIndex(advanceIndex);
  QVERIFY(QMetaObject::invokeMethod(action, "activated", Q_ARG(int, advanceIndex)));
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  QCOMPARE(button()["action"].toString(), QString("advance"));
  QVERIFY(!button().contains("screenId"));
  QVERIFY(!button().contains("locationId"));
  QVERIFY(!w.findChild<QComboBox *>("actionTarget")->isEnabled());
  QVERIFY(action->findData("cancelBattleAction") >= 0);
  w.findChild<QAction *>("undoEdit")->trigger();
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  QCOMPARE(button()["action"].toString(), QString("navigate"));
  w.findChild<QAction *>("redoEdit")->trigger();
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  QCOMPARE(button()["action"].toString(), QString("advance"));
  w.findChild<QAction *>("saveProject")->trigger();
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  QVERIFY2(w.lastError.isEmpty(), qPrintable(w.lastError));
  w.openProject(root);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  QCOMPARE(button()["action"].toString(), QString("advance"));
}
