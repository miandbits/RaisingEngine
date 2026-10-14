void StudioTests::contentCharacterPortraitRoundtrip() {
  const auto engine = qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath());
  QTemporaryDir temp(engine + "/.cache/qt-character-portraits-XXXXXX");
  QVERIFY(temp.isValid()); QVERIFY(copyContentFixture(engine + "/examples/academy-adventure", temp.path()));
  StudioWindow window(engine); window.show(); window.openProject(temp.path());
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy() && !window.projectData().isEmpty(), 20000);
  const QString file = "characters/mentor.json";
  const auto original = contentDocument(window, file); QVERIFY(!original.isEmpty()); QVERIFY(!original.contains("description"));
  QTimer watchdog; watchdog.setInterval(10000); connect(&watchdog, &QTimer::timeout, &window, [&] { if (auto d = window.findChild<QDialog *>("contentEditor")) d->reject(); }); watchdog.start();
  bool authored = false;
  QTimer::singleShot(50, &window, [&] {
    auto dialog = window.findChild<QDialog *>("contentEditor"); QVERIFY(dialog);
    auto table = dialog->findChild<QTableWidget *>("contentPortraits"); QVERIFY(table);
    const auto before = table->rowCount(); dialog->findChild<QPushButton *>("contentPortraitAdd")->click(); QCOMPARE(table->rowCount(), before + 1);
    table->item(before, 0)->setText("dance");
    auto asset = qobject_cast<QComboBox *>(table->cellWidget(before, 1)); QVERIFY(asset); QVERIFY(asset->findData("visual-haru") >= 0);
    asset->setCurrentIndex(asset->findData("visual-haru"));
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click(); authored = true;
  });
  QVERIFY(openContentDocument(window, file)); QVERIFY(authored); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000);
  QVERIFY2(window.lastError.isEmpty(), qPrintable(window.lastError));
  auto expected = original; auto map = original.value("portraits").toObject(); map["dance"] = "visual-haru"; expected["portraits"] = map;
  QCOMPARE(contentDocument(window, file), expected);
  window.findChild<QAction *>("undoEdit")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000); QCOMPARE(contentDocument(window, file), original);
  window.findChild<QAction *>("redoEdit")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000); QCOMPARE(contentDocument(window, file), expected);
  window.findChild<QAction *>("saveProject")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000);
  window.openProject(temp.path()); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 20000); QCOMPARE(contentDocument(window, file), expected);
  QTimer::singleShot(50, &window, [&] { auto dialog = window.findChild<QDialog *>("contentEditor"); QVERIFY(dialog); dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click(); });
  QVERIFY(openContentDocument(window, file)); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000); QCOMPARE(contentDocument(window, file), expected);
  QTimer::singleShot(50, &window, [&] {
    auto dialog = window.findChild<QDialog *>("contentEditor"); QVERIFY(dialog); auto table = dialog->findChild<QTableWidget *>("contentPortraits");
    for (int row = 0; row < table->rowCount(); ++row) if (table->item(row, 0)->text() == "dance") {
      auto asset = qobject_cast<QComboBox *>(table->cellWidget(row, 1)); asset->setCurrentIndex(asset->findData("visual-yuna")); break;
    }
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
  });
  QVERIFY(openContentDocument(window, file)); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000);
  map["dance"] = "visual-yuna"; expected["portraits"] = map; QCOMPARE(contentDocument(window, file), expected);
  QTimer::singleShot(50, &window, [&] {
    auto dialog = window.findChild<QDialog *>("contentEditor"); QVERIFY(dialog); auto table = dialog->findChild<QTableWidget *>("contentPortraits");
    for (int row = 0; row < table->rowCount(); ++row) if (table->item(row, 0)->text() == "dance") { table->setCurrentCell(row, 0); dialog->findChild<QPushButton *>("contentPortraitRemove")->click(); break; }
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
  });
  QVERIFY(openContentDocument(window, file)); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000); QCOMPARE(contentDocument(window, file), original);
}

void StudioTests::contentCharacterPortraitGuards() {
  const auto engine = qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath());
  QTemporaryDir temp(engine + "/.cache/qt-character-portrait-guards-XXXXXX");
  QVERIFY(temp.isValid()); QVERIFY(copyContentFixture(engine + "/examples/academy-adventure", temp.path()));
  StudioWindow window(engine); window.show(); window.openProject(temp.path());
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy() && !window.projectData().isEmpty(), 20000);
  const QString file = "characters/mentor.json"; const auto original = contentDocument(window, file);
  QTimer watchdog; watchdog.setInterval(10000); connect(&watchdog, &QTimer::timeout, &window, [&] { if (auto d = window.findChild<QDialog *>("contentEditor")) d->reject(); }); watchdog.start();
  bool checked = false;
  QTimer::singleShot(50, &window, [&] {
    auto dialog = window.findChild<QDialog *>("contentEditor"); QVERIFY(dialog);
    auto table = dialog->findChild<QTableWidget *>("contentPortraits"); QVERIFY(table); QCOMPARE(table->rowCount(), 1);
    auto save = dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save);
    auto error = dialog->findChild<QLabel *>("contentError"); QVERIFY(error);
    dialog->findChild<QPushButton *>("contentPortraitAdd")->click(); table->item(1, 0)->setText("visual-neutral");
    auto asset = qobject_cast<QComboBox *>(table->cellWidget(1, 1)); asset->setCurrentIndex(asset->findData("visual-haru"));
    save->click(); QVERIFY(dialog->isVisible()); QVERIFY(!error->text().isEmpty());
    table->item(1, 0)->setText("dance"); asset->setCurrentIndex(0); error->clear(); save->click(); QVERIFY(dialog->isVisible()); QVERIFY(!error->text().isEmpty());
    table->setCurrentCell(1, 0); dialog->findChild<QPushButton *>("contentPortraitRemove")->click();
    table->item(0, 0)->setText("renamed"); error->clear(); save->click(); QVERIFY(dialog->isVisible()); QVERIFY(error->text().contains("사용 중"));
    table->setCurrentCell(0, 0); dialog->findChild<QPushButton *>("contentPortraitRemove")->click(); error->clear(); save->click(); QVERIFY(dialog->isVisible()); QVERIFY(error->text().contains("사용 중"));
    checked = true; dialog->reject();
  });
  QVERIFY(openContentDocument(window, file)); QVERIFY(checked); QVERIFY(!window.busy()); QCOMPARE(contentDocument(window, file), original);
}

void StudioTests::contentCharacterSpeakerPortraitGuard() {
  const auto engine = qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath());
  QTemporaryDir temp(engine + "/.cache/qt-speaker-portrait-guard-XXXXXX");
  QVERIFY(temp.isValid()); QVERIFY(copyContentFixture(engine + "/examples/academy-adventure", temp.path()));
  QFile file(temp.path() + "/project.json"); QVERIFY(file.open(QIODevice::ReadOnly));
  auto project = QJsonDocument::fromJson(file.readAll()).object(); file.close();
  auto visual = project.value("visual").toObject(); auto screens = visual.value("screens").toArray(); auto screen = screens[0].toObject(); auto elements = screen.value("elements").toArray();
  QJsonObject image{{"id", "speaker-default"}, {"kind", "image"}, {"x", 0}, {"y", 0}, {"width", 100}, {"height", 100}, {"imageSource", "speaker-portrait"}};
  elements.append(image); image["id"] = "speaker-custom"; image["portraitId"] = "smile"; elements.append(image);
  screen["elements"] = elements; screens[0] = screen; visual["screens"] = screens; project["visual"] = visual;
  QVERIFY(file.open(QIODevice::WriteOnly)); file.write(QJsonDocument(project).toJson()); file.close();
  StudioWindow window(engine); window.show(); window.openProject(temp.path());
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy() && !window.projectData().isEmpty(), 20000);
  const QString characterFile = "characters/mentor.json"; const auto original = contentDocument(window, characterFile);
  QVERIFY(!original.value("portraits").toObject().contains("face"));
  QTimer watchdog; watchdog.setInterval(10000); connect(&watchdog, &QTimer::timeout, &window, [&] { if (auto d = window.findChild<QDialog *>("contentEditor")) d->reject(); }); watchdog.start();
  QTimer::singleShot(50, &window, [&] { auto dialog = window.findChild<QDialog *>("contentEditor"); QVERIFY(dialog); dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click(); });
  QVERIFY(openContentDocument(window, characterFile)); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000);
  QCOMPARE(contentDocument(window, characterFile), original); QVERIFY2(window.lastError.isEmpty(), qPrintable(window.lastError)); // A legitimately absent face remains allowed.
  QTimer::singleShot(50, &window, [&] {
    auto dialog = window.findChild<QDialog *>("contentEditor"); QVERIFY(dialog); auto table = dialog->findChild<QTableWidget *>("contentPortraits");
    for (const auto &id : QStringList{"face", "smile"}) {
      dialog->findChild<QPushButton *>("contentPortraitAdd")->click(); const int row = table->rowCount() - 1; table->item(row, 0)->setText(id);
      auto asset = qobject_cast<QComboBox *>(table->cellWidget(row, 1)); asset->setCurrentIndex(asset->findData("visual-yuna"));
    }
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
  });
  QVERIFY(openContentDocument(window, characterFile)); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000);
  auto expected = original; auto portraits = original.value("portraits").toObject(); portraits["face"] = "visual-yuna"; portraits["smile"] = "visual-yuna"; expected["portraits"] = portraits;
  QCOMPARE(contentDocument(window, characterFile), expected);
  bool checked = false;
  QTimer::singleShot(50, &window, [&] {
    auto dialog = window.findChild<QDialog *>("contentEditor"); QVERIFY(dialog); auto table = dialog->findChild<QTableWidget *>("contentPortraits");
    auto error = dialog->findChild<QLabel *>("contentError"); auto save = dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save);
    for (const auto &id : QStringList{"face", "smile"}) {
      int target = -1; for (int row = 0; row < table->rowCount(); ++row) if (table->item(row, 0)->text() == id) target = row;
      QVERIFY(target >= 0); table->item(target, 0)->setText("renamed"); error->clear(); save->click();
      QVERIFY(dialog->isVisible()); QVERIFY(error->text().contains(id)); table->item(target, 0)->setText(id);
    }
    int target = -1; for (int row = 0; row < table->rowCount(); ++row) if (table->item(row, 0)->text() == "face") target = row;
    table->setCurrentCell(target, 0); dialog->findChild<QPushButton *>("contentPortraitRemove")->click(); error->clear(); save->click();
    QVERIFY(dialog->isVisible()); QVERIFY(error->text().contains("face")); checked = true; dialog->reject();
  });
  QVERIFY(openContentDocument(window, characterFile)); QVERIFY(checked); QCOMPARE(contentDocument(window, characterFile), expected);
}
