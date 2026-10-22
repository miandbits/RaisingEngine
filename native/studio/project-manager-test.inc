void StudioTests::rosterTemplateRoundtrip() {
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT");QTemporaryDir temp(engine+"/.cache/roster-template-XXXXXX");QVERIFY(temp.isValid());
  const auto oldHistory=qgetenv("RAISING_PROJECT_HISTORY");qputenv("RAISING_PROJECT_HISTORY",(temp.path()+"/history.ini").toUtf8());
  const auto restore=qScopeGuard([&]{if(oldHistory.isNull())qunsetenv("RAISING_PROJECT_HISTORY");else qputenv("RAISING_PROJECT_HISTORY",oldHistory);});
  ProjectManager manager(engine);manager.show();auto picker=manager.findChild<QComboBox*>("launcherTemplate");
  const int index=picker->findData("genre-suite/roster");QVERIFY(index>=0);picker->setCurrentIndex(index);
  manager.findChild<QLineEdit*>("launcherDestination")->setText(temp.path()+"/launcher-game");manager.findChild<QPushButton*>("launcherCreate")->click();
  QTRY_VERIFY_WITH_TIMEOUT(manager.findChild<QPushButton*>("launcherCreate")->isEnabled()||manager.result()==int(QDialog::Accepted),20000);
  QVERIFY2(manager.result()==int(QDialog::Accepted),qPrintable(manager.findChild<QLabel*>("launcherMessage")->text()));
  StudioWindow editor(engine);editor.show();editor.openProject(manager.selectedProject());QTRY_VERIFY_WITH_TIMEOUT(!editor.busy()&&!editor.projectData().isEmpty(),15000);
  QCOMPARE(editor.projectData().value("roster").toObject().value("members").toArray().size(),2);
  QTimer::singleShot(100,&editor,[&]{auto d=editor.findChild<QDialog*>("newProjectDialog");QVERIFY(d);auto p=d->findChild<QComboBox*>("newProjectTemplate");QVERIFY(p->findData("genre-suite/roster")>=0);p->setCurrentIndex(p->findData("genre-suite/roster"));d->findChild<QLineEdit*>("newProjectDestination")->setText(temp.path()+"/wizard-game");d->findChild<QDialogButtonBox*>("newProjectButtons")->button(QDialogButtonBox::Ok)->click();});
  editor.findChild<QAction*>("newProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!editor.busy(),15000);
  QVERIFY2(editor.lastError.isEmpty(),qPrintable(editor.lastError));QCOMPARE(QDir::fromNativeSeparators(editor.currentView().value("root").toString()),temp.path()+"/wizard-game");
  QCOMPARE(editor.projectData().value("roster").toObject().value("members").toArray().size(),2);
}

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
