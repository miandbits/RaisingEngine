void StudioTests::scriptScenePreviewDraftIsolation() {
  const QJsonObject portrait{{"characterId", "actor"}, {"portraitId", "dance"}, {"position", "center"}};
  auto context = portraitTransformContext(portrait); auto documents = context.value("documents").toArray();
  QJsonObject visual{{"width", 1280}, {"height", 960}, {"screens", QJsonArray{QJsonObject{{"id", "home"}, {"label", "Home"}, {"elements", QJsonArray{}}}, QJsonObject{{"id", "story"}, {"label", "Story"}, {"elements", QJsonArray{}}}}}};
  documents.append(QJsonObject{{"kind", "project"}, {"file", "project.json"}, {"data", QJsonObject{{"id", "example"}, {"visual", visual}}}});
  context["documents"] = documents; context["root"] = "fixture"; context["revision"] = 7;
  QJsonObject requestedScene; int requests = 0; ScriptAuthoringDialog *owner = nullptr;
  ScriptAuthoringDialog dialog(context, {}, nullptr, {}, [&](QJsonObject scene, std::function<void(QJsonObject)> done) {
    requestedScene = scene; ++requests;
    auto preview = owner->findChild<QDialog *>("scriptScenePreviewDialog"); QVERIFY(preview);
    QVERIFY(preview->windowTitle().contains("게임 진행 검증 아님"));
    auto screen = preview->findChild<QComboBox *>("scriptPreviewScreen"); QVERIFY(screen); QCOMPARE(screen->currentData().toString(), QString("story")); QCOMPARE(screen->count(), 2);
    done(QJsonObject{{"error", "fixture renderer is intentionally unavailable"}});
    QTimer::singleShot(0, preview, &QDialog::reject);
  });
  owner = &dialog; dialog.show(); scriptSelectNode(dialog, "scene");
  QTimer::singleShot(0, &dialog, [&] {
    auto child = dialog.findChild<QDialog *>("scriptPortraitDialog"); QVERIFY(child);
    child->findChild<QDoubleSpinBox *>("scriptPortrait-scale")->setValue(20);
    child->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
  });
  dialog.findChild<QPushButton *>("scriptPortraits")->click(); const auto draft = dialog.edits(); QCOMPARE(draft.size(), 1);
  QTimer watchdog; watchdog.setInterval(10000); connect(&watchdog, &QTimer::timeout, &dialog, [&] { if (auto preview = dialog.findChild<QDialog *>("scriptScenePreviewDialog")) preview->reject(); }); watchdog.start();
  auto previewButton = dialog.findChild<QPushButton *>("scriptScenePreview"); QVERIFY(previewButton); QVERIFY(previewButton->isEnabled()); previewButton->click();
  QCOMPARE(requests, 1); QCOMPARE(requestedScene.value("portraits").toArray()[0].toObject().value("scale").toDouble(), .2); QCOMPARE(dialog.edits(), draft);
  ScriptAuthoringDialog unavailable(context, {}); unavailable.show(); scriptSelectNode(unavailable, "scene"); QVERIFY(!unavailable.findChild<QPushButton *>("scriptScenePreview")->isEnabled());
}

void StudioTests::scriptScenePreviewNativeRoundtrip() {
  if (qEnvironmentVariableIsEmpty("RAISING_SCENE_PREVIEW_RUNTIME")) QSKIP("Requires a native runtime with scene-preview-v1");
  qputenv("RAISING_NATIVE_PLAYER_RUNTIME", qgetenv("RAISING_SCENE_PREVIEW_RUNTIME"));
  const auto engine = qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath());
  QTemporaryDir temp(engine + "/.cache/qt-scene-preview-native-XXXXXX"); QVERIFY(temp.isValid()); QVERIFY(copyContentFixture(engine + "/examples/academy-adventure", temp.path()));
  QTemporaryDir runtimeCopy(engine + "/.cache/qt-scene-preview-runtime-XXXXXX"); QVERIFY(runtimeCopy.isValid());
  QVERIFY(copyContentFixture(qEnvironmentVariable("RAISING_SCENE_PREVIEW_RUNTIME"), runtimeCopy.path()));
  QFile capabilityFile(runtimeCopy.path() + "/capabilities.json"); QVERIFY(capabilityFile.open(QIODevice::ReadOnly));
  const auto originalCapabilities = capabilityFile.readAll(); capabilityFile.close();
  auto capabilities = QJsonDocument::fromJson(originalCapabilities).object(); QJsonArray legacyFeatures;
  for (const auto &feature : capabilities.value("visualFeatures").toArray()) if (feature.toString() != "scene-portrait-transform-v1") legacyFeatures.append(feature);
  capabilities["visualFeatures"] = legacyFeatures;
  QVERIFY(capabilityFile.open(QIODevice::WriteOnly | QIODevice::Truncate)); capabilityFile.write(QJsonDocument(capabilities).toJson()); capabilityFile.close();
  qputenv("RAISING_NATIVE_PLAYER_RUNTIME", runtimeCopy.path().toUtf8());
  StudioWindow window(engine); window.show(); window.openProject(temp.path());
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy() && !window.projectData().isEmpty(), 20000);
  const auto before = window.currentView(); const auto beforeProject = window.projectData();
  bool rendered = false; QString evidence;
  QTimer watchdog; watchdog.setInterval(45000); connect(&watchdog, &QTimer::timeout, &window, [&] { for (auto d : window.findChildren<QDialog *>()) if (d->isVisible()) d->reject(); }); watchdog.start();
  QTimer::singleShot(0, &window, [&] {
    auto workshop = dynamic_cast<ScriptAuthoringDialog *>(window.findChild<QDialog *>("scriptAuthoringDialog")); QVERIFY(workshop);
    auto documents = workshop->findChild<QComboBox *>("scriptDocument"); QVERIFY(documents);
    int index = -1; for (int i = 0; i < documents->count(); ++i) if (documents->itemText(i).contains("events/month-01.json")) index = i;
    QVERIFY(index >= 0); documents->setCurrentIndex(index); scriptSelectNode(*workshop, "visual-intro");
    QTimer::singleShot(0, workshop, [&] {
      auto child = workshop->findChild<QDialog *>("scriptPortraitDialog"); QVERIFY(child);
      child->findChild<QDoubleSpinBox *>("scriptPortrait-scale")->setValue(20);
      child->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    });
    workshop->findChild<QPushButton *>("scriptPortraits")->click(); QVERIFY(!workshop->edits().isEmpty());
    QTimer::singleShot(0, workshop, [&] {
      auto preview = workshop->findChild<QDialog *>("scriptScenePreviewDialog"); QVERIFY(preview);
      auto renderer = preview->findChild<RendererPreviewDock *>("scriptSceneRenderer"); QVERIFY(renderer);
      QSignalSpy ready(renderer, &RendererPreviewDock::snapshotReady);
      QTRY_VERIFY_WITH_TIMEOUT(ready.count() > 0, 40000); QVERIFY(!ready.takeFirst()[0].toBool());
      QVERIFY(renderer->findChild<QLabel *>("rendererStatus")->text().contains("scene-portrait-transform-v1"));
      QCOMPARE(window.currentView().value("revision"), before.value("revision"));
      QCOMPARE(window.currentView().value("documents"), before.value("documents"));
      // Restore capability information only in this owned runtime and retry the still-unsaved scene.
      QVERIFY(capabilityFile.open(QIODevice::WriteOnly | QIODevice::Truncate)); capabilityFile.write(originalCapabilities); capabilityFile.close();
      renderer->findChild<QPushButton *>("renderPreview")->click();
      QTRY_VERIFY_WITH_TIMEOUT(ready.count() > 0, 40000); QVERIFY(ready.takeFirst()[0].toBool());
      evidence = renderer->reportPath(); const auto ui = renderer->result();
      QVERIFY(ui.value("preview").toObject().value("sceneOverride").toBool());
      const auto scene = ui.value("preview").toObject().value("displayScene").toObject();
      QCOMPARE(scene.value("portraits").toArray()[0].toObject().value("scale").toDouble(), .2);
      rendered = true; preview->reject();
    });
    workshop->findChild<QPushButton *>("scriptScenePreview")->click(); workshop->reject();
  });
  window.findChild<QAction *>("scriptAuthoringAction")->trigger(); QVERIFY(rendered); QVERIFY(!evidence.isEmpty());
  QCOMPARE(window.currentView().value("revision"), before.value("revision")); QCOMPARE(window.currentView().value("documents"), before.value("documents")); QCOMPARE(window.currentView().value("dirty"), before.value("dirty")); QCOMPARE(window.projectData(), beforeProject);
  qInfo().noquote() << "Native scene preview evidence:" << evidence;
}
