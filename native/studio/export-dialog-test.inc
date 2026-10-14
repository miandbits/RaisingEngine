void StudioTests::exportGameDialogRoundtrip() {
  if (qEnvironmentVariableIsEmpty("RAISING_EXPORT_RUNTIME")) QSKIP("Requires the final native export runtime");
  qputenv("RAISING_NATIVE_PLAYER_RUNTIME", qgetenv("RAISING_EXPORT_RUNTIME"));
  const auto engine = qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath());
  QTemporaryDir temp(engine + "/.cache/qt-export-dialog-XXXXXX"); QVERIFY(temp.isValid()); const auto project = temp.path() + "/project";
  const auto runtime = temp.path() + "/runtime";
  QVERIFY(copyContentFixture(qEnvironmentVariable("RAISING_EXPORT_RUNTIME"), runtime));
  qputenv("RAISING_NATIVE_PLAYER_RUNTIME", runtime.toUtf8());
  QVERIFY(copyContentFixture(engine + "/examples/academy-adventure", project));
  StudioWindow window(engine); window.show(); window.openProject(project);
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy() && !window.projectData().isEmpty(), 20000);
  QFile original(project + "/activities/rest.json"); QVERIFY(original.open(QIODevice::ReadOnly)); const auto originalBytes = original.readAll(); original.close();
  QTimer::singleShot(0, &window, [&] { auto dialog = window.findChild<QDialog *>("contentEditor"); QVERIFY(dialog); dialog->findChild<QLineEdit *>("contentLabel")->setText("Unsaved exported activity"); dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click(); });
  QVERIFY(openContentDocument(window, "activities/rest.json")); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000); QVERIFY(window.currentView().value("dirty").toBool());
  const auto before = window.currentView();
  QTimer watchdog; watchdog.setInterval(45000); connect(&watchdog, &QTimer::timeout, &window, [&] { if (auto dialog = window.findChild<QDialog *>("exportGameDialog")) dialog->reject(); }); watchdog.start();
  for (const auto &targetId : QStringList{"windows", "web"}) {
    bool complete = false; const auto destination = temp.path() + "/" + targetId;
    QTimer::singleShot(0, &window, [&] {
      auto dialog = window.findChild<QDialog *>("exportGameDialog"); QVERIFY(dialog);
      auto target = dialog->findChild<QComboBox *>("exportTarget"); target->setCurrentIndex(target->findData(targetId));
      dialog->findChild<QLineEdit *>("exportParent")->setText(temp.path()); auto name = dialog->findChild<QLineEdit *>("exportFolderName");
      auto start = dialog->findChild<QPushButton *>("exportStart"); auto result = dialog->findChild<QLabel *>("exportResult");
      name->setText("project"); start->click(); QVERIFY(result->text().contains("이미")); QVERIFY(!window.busy());
      name->setText(targetId);
      if (targetId == "windows") {
        // Force a failure after staging has started without changing the installed runtime.
        QFile collision(runtime + "/game.json"); QVERIFY(collision.open(QIODevice::WriteOnly)); collision.write("owned test collision"); collision.close();
        start->click(); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 40000);
        QVERIFY(start->isEnabled()); QVERIFY(result->text().contains("다시 내보내세요"));
        QVERIFY(!dialog->findChild<QPushButton *>("exportOpenFolder")->isEnabled());
        QVERIFY(!QFileInfo::exists(destination));
        QVERIFY(QDir(temp.path()).entryList({".raising-export-*"}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
        QCOMPARE(window.currentView().value("revision"), before.value("revision"));
        QCOMPARE(window.currentView().value("documents"), before.value("documents"));
        QVERIFY(QFile::remove(collision.fileName()));
      }
      start->click();
      QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 40000); auto open = dialog->findChild<QPushButton *>("exportOpenFolder"); QVERIFY2(open->isEnabled(), qPrintable(result->text()));
      const auto exportedDirectory = open->property("directory").toString(); QCOMPARE(QDir::fromNativeSeparators(exportedDirectory), QDir::fromNativeSeparators(destination)); QVERIFY(result->text().contains(exportedDirectory)); complete = true; dialog->reject();
    });
    window.findChild<QAction *>("exportGame")->trigger(); QVERIFY(complete);
    QVERIFY(QFileInfo::exists(destination + (targetId == "windows" ? "/RaisingPlayer.exe" : "/index.html")));
    QFile game(destination + "/game.json"); QVERIFY(game.open(QIODevice::ReadOnly)); const auto bundle = QJsonDocument::fromJson(game.readAll()).object(); game.close();
    bool found = false; for (auto activity : bundle.value("content").toObject().value("activities").toArray()) if (activity.toObject().value("id") == "rest") { QCOMPARE(activity.toObject().value("label").toString(), QString("Unsaved exported activity")); found = true; } QVERIFY(found);
    QCOMPARE(window.currentView().value("revision"), before.value("revision")); QCOMPARE(window.currentView().value("documents"), before.value("documents")); QCOMPARE(window.currentView().value("dirty"), before.value("dirty")); QCOMPARE(window.currentView().value("undo"), before.value("undo"));
  }
  QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(), originalBytes);
}
