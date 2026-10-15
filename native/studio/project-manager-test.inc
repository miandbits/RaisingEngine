void StudioTests::projectManagerRoundtrip() {
  applyStudioTheme(*qobject_cast<QApplication*>(QApplication::instance()));
  // The Windows test runner suppresses the first native ShowWindow via STARTUPINFO.
  auto expose = [](QWidget &widget) { widget.show(); widget.hide(); widget.show(); return QTest::qWaitForWindowExposed(&widget); };
  QTemporaryDir temp;
  QVERIFY(temp.isValid());
  const auto oldHistory = qgetenv("RAISING_PROJECT_HISTORY");
  qputenv("RAISING_PROJECT_HISTORY", (temp.path() + "/history.ini").toUtf8());
  const auto restore = qScopeGuard([&] {
    if (oldHistory.isNull()) qunsetenv("RAISING_PROJECT_HISTORY"); else qputenv("RAISING_PROJECT_HISTORY", oldHistory);
  });
  const auto engine = qEnvironmentVariable("RAISING_ENGINE_ROOT");
  const auto created = temp.path() + "/new game 한글";
  {
    ProjectManager manager(engine);
    QVERIFY(expose(manager));
    QVERIFY(manager.findChild<QListWidget*>("recentProjects")->count() == 0);
    auto destination = manager.findChild<QLineEdit*>("launcherDestination");
    auto create = manager.findChild<QPushButton*>("launcherCreate");
    // A failed create must stay in the launcher and permit a corrected retry.
    destination->setText(temp.path());
    QFile marker(temp.path() + "/preserve.txt"); QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.write("keep"); marker.close();
    QTest::mouseClick(create, Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(create->isEnabled(), 15000);
    QVERIFY(manager.isVisible());
    QVERIFY(manager.selectedProject().isEmpty());
    QVERIFY(!manager.findChild<QLabel*>("launcherMessage")->text().isEmpty());
    QVERIFY(marker.open(QIODevice::ReadOnly)); QCOMPARE(marker.readAll(), QByteArray("keep")); marker.close();
    destination->setText(created);
    QTest::mouseClick(create, Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(!manager.isVisible(), 15000);
    QCOMPARE(manager.result(), int(QDialog::Accepted));
    QCOMPARE(manager.selectedProject(), QDir::fromNativeSeparators(created));
    QVERIFY(QFileInfo::exists(created + "/project.json"));
  }
  ProjectManager::remember(created + "/project.json");
  QCOMPARE(ProjectManager::recentProjects().size(), 1);
  {
    StudioWindow editor(engine);
    editor.show(); editor.openProject(created);
    QTRY_VERIFY_WITH_TIMEOUT(!editor.busy() && !editor.currentView().value("root").toString().isEmpty(), 15000);
    QCOMPARE(QDir::fromNativeSeparators(editor.currentView().value("root").toString()), created);
    QVERIFY(!editor.projectData().isEmpty());
    QVERIFY(editor.findChild<QAction*>("showProjectManager"));
    editor.close();
  }
  {
    ProjectManager manager(engine);
    QVERIFY(expose(manager));
    auto list = manager.findChild<QListWidget*>("recentProjects");
    auto search = manager.findChild<QLineEdit*>("projectSearch");
    QCOMPARE(list->count(), 1);
    search->setText("not-a-project"); QCOMPARE(list->count(), 0);
    search->setText("한글"); QCOMPARE(list->count(), 1);
    QVERIFY(manager.grab().save(engine + "/.cache/project-manager.png"));
    QTest::mouseClick(manager.findChild<QPushButton*>("openRecentProject"), Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(manager.result(), int(QDialog::Accepted), 15000);
    QCOMPARE(manager.selectedProject(), created);
  }
  {
    ProjectManager manager(engine);
    QVERIFY(expose(manager));
    QTest::mouseClick(manager.findChild<QPushButton*>("removeRecentProject"), Qt::LeftButton);
    QVERIFY(ProjectManager::recentProjects().isEmpty());
    QVERIFY(QFileInfo::exists(created + "/project.json"));
    auto path = manager.findChild<QLineEdit*>("projectLocation");
    auto open = manager.findChild<QPushButton*>("importProject");
    path->setText(temp.path() + "/moved-away");
    QTest::mouseClick(open, Qt::LeftButton);
    QVERIFY(manager.isVisible()); QVERIFY(manager.selectedProject().isEmpty());
    QVERIFY(manager.findChild<QLabel*>("launcherMessage")->text().contains("project.json"));
    path->setText(created + "/project.json");
    QTest::mouseClick(open, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(manager.result(), int(QDialog::Accepted), 15000);
    QCOMPARE(ProjectManager::recentProjects().size(), 1);
  }
}
