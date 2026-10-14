#include "progression-authoring.hpp"

namespace {
QJsonObject progressionTestSettings() {
  return {{"id", "training-test"}, {"name", "Training test"}, {"targetAttributeId", "skill"}, {"currencyAttributeId", "money"}, {"experienceGain", 1},
          {"tiers", QJsonArray{QJsonObject{{"minimumExperience", 0}, {"cost", 7}, {"gain", 2}, {"chance", 10000}},
                               QJsonObject{{"minimumExperience", 1}, {"cost", 11}, {"gain", 4}, {"chance", 10000}},
                               QJsonObject{{"minimumExperience", 2}, {"cost", 13}, {"gain", 6}, {"chance", 10000}}}}};
}
void fillProgressionDialog(QDialog *dialog) {
  const auto settings = progressionTestSettings();
  dialog->findChild<QLineEdit *>("progressionId")->setText(settings.value("id").toString());
  dialog->findChild<QLineEdit *>("progressionName")->setText(settings.value("name").toString());
  auto target = dialog->findChild<QComboBox *>("progressionTarget"), currency = dialog->findChild<QComboBox *>("progressionCurrency");
  target->setCurrentIndex(target->findData("skill")); currency->setCurrentIndex(currency->findData("money"));
  auto schedule = dialog->findChild<QComboBox *>("progressionScheduleLink");
  for (int i = 1; i < schedule->count(); ++i) if (schedule->itemData(i).toJsonObject().value("groupId") == "wizard-training") schedule->setCurrentIndex(i);
  auto table = dialog->findChild<QTableWidget *>("progressionTiers");
  while (table->rowCount() < 3) dialog->findChild<QPushButton *>("progressionAddTier")->click();
  const QStringList keys{"minimumExperience", "cost", "gain", "chance"};
  const auto tiers = settings.value("tiers").toArray();
  for (int r = 0; r < 3; ++r) for (int c = 0; c < 4; ++c) table->item(r, c)->setText(QString::number(tiers[r].toObject().value(keys[c]).toInt()));
}
}

void StudioTests::progressionAuthoringRoundtrip() {
  const auto engine = qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath());
  QTemporaryDir temp(engine + "/.cache/qt-progression-proof-XXXXXX"); QVERIFY(temp.isValid());
  QVERIFY(copyContentFixture(engine + "/examples/academy-adventure", temp.path()));
  QFile projectFile(temp.path() + "/project.json"); QVERIFY(projectFile.open(QIODevice::ReadOnly));
  auto project = QJsonDocument::fromJson(projectFile.readAll()).object(); projectFile.close();
  auto visual = project.value("visual").toObject(); auto screens = visual.value("screens").toArray(); bool prepared = false;
  for (int s = 0; s < screens.size() && !prepared; ++s) { auto screen = screens[s].toObject(); auto elements = screen.value("elements").toArray();
    for (int e = 0; e < elements.size(); ++e) { auto element = elements[e].toObject(); if (element.value("kind") != "schedule") continue;
      element["scheduleGroups"] = QJsonArray{QJsonObject{{"id", "wizard-training"}, {"label", "Training"}, {"activityIds", QJsonArray{"rest"}}}};
      elements[e] = element; prepared = true; break;
    }
    screen["elements"] = elements; screens[s] = screen;
  }
  QVERIFY(prepared); visual["screens"] = screens; project["visual"] = visual;
  QVERIFY(projectFile.open(QIODevice::WriteOnly)); projectFile.write(QJsonDocument(project).toJson()); projectFile.close();
  StudioWindow window(engine); window.show(); window.openProject(temp.path());
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy() && !window.projectData().isEmpty(), 20000);
  const auto before = window.currentView().value("documents");
  const int revision = window.currentView().value("revision").toInt();
  const auto plan = makeProgressionPlan(window.currentView(), progressionTestSettings());
  QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error)); QCOMPARE(plan.edits.size(), 4);
  bool applied = false;
  QTimer::singleShot(50, &window, [&] {
    auto dialog = window.findChild<QDialog *>("progressionAuthoring"); QVERIFY(dialog);
    QTimer::singleShot(10000, dialog, &QDialog::reject);
    fillProgressionDialog(dialog);
    dialog->findChild<QPushButton *>("progressionPreview")->click();
    QVERIFY2(dialog->findChild<QLabel *>("progressionError")->text().isEmpty(), qPrintable(dialog->findChild<QLabel *>("progressionError")->text()));
    QVERIFY(dialog->findChild<QPlainTextEdit *>("progressionPreviewText")->toPlainText().contains("training-test-profile"));
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click(); applied = true;
  });
  window.findChild<QAction *>("progressionAuthoringAction")->trigger(); QVERIFY(applied);
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000); QVERIFY2(window.lastError.isEmpty(), qPrintable(window.lastError));
  QCOMPARE(window.currentView().value("revision").toInt(), revision + 1);
  QCOMPARE(contentDocument(window, "activities/training-test.json"), plan.preview.value("activity").toObject());
  bool linked = false;
  for (auto s : window.projectData().value("visual").toObject().value("screens").toArray()) for (auto e : s.toObject().value("elements").toArray())
    for (auto g : e.toObject().value("scheduleGroups").toArray()) if (g.toObject().value("id") == "wizard-training") {
      QCOMPARE(g.toObject().value("activityIds").toArray(), QJsonArray({"rest", "training-test"})); linked = true;
    }
  QVERIFY(linked);
  const auto after = window.currentView().value("documents");
  window.findChild<QAction *>("undoEdit")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000);
  QCOMPARE(window.currentView().value("documents"), before);
  window.findChild<QAction *>("redoEdit")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000);
  QCOMPARE(window.currentView().value("documents"), after);
  window.findChild<QAction *>("saveProject")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000);
  QVERIFY2(window.lastError.isEmpty(), qPrintable(window.lastError));
  window.openProject(temp.path()); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 20000);
  QCOMPARE(contentDocument(window, "activities/training-test.json"), plan.preview.value("activity").toObject());
  // Preserve this isolated GUI-authored fixture for independent runtime tests.
  QFile evidence(engine + "/.cache/qt-progression-authoring-proof.json"); QVERIFY(evidence.open(QIODevice::WriteOnly));
  evidence.write(QJsonDocument(QJsonObject{{"root", temp.path()}, {"preview", plan.preview}}).toJson()); evidence.close();
  temp.setAutoRemove(false);
}

void StudioTests::progressionAuthoringGuards() {
  const auto engine = qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath());
  QTemporaryDir temp(engine + "/.cache/qt-progression-guards-XXXXXX"); QVERIFY(temp.isValid()); QVERIFY(copyContentFixture(engine + "/examples/academy-adventure", temp.path()));
  StudioWindow window(engine); window.show(); window.openProject(temp.path()); QTRY_VERIFY_WITH_TIMEOUT(!window.busy() && !window.projectData().isEmpty(), 20000);
  const auto before = window.currentView().value("documents");
  auto settings = progressionTestSettings(); settings["id"] = "study";
  QVERIFY(!makeProgressionPlan(window.currentView(), settings).error.isEmpty());
  settings = progressionTestSettings(); auto rows = settings.value("tiers").toArray(); auto row = rows[1].toObject(); row["minimumExperience"] = 0; rows[1] = row; settings["tiers"] = rows;
  QVERIFY(!makeProgressionPlan(window.currentView(), settings).error.isEmpty());
  settings = progressionTestSettings(); rows = settings.value("tiers").toArray(); row = rows[0].toObject(); row["chance"] = 10001; rows[0] = row; settings["tiers"] = rows;
  QVERIFY(!makeProgressionPlan(window.currentView(), settings).error.isEmpty());
  settings = progressionTestSettings(); settings["targetAttributeId"] = "missing";
  QVERIFY(!makeProgressionPlan(window.currentView(), settings).error.isEmpty());
  bool guarded = false;
  QTimer::singleShot(50, &window, [&] {
    auto dialog = window.findChild<QDialog *>("progressionAuthoring"); QVERIFY(dialog); fillProgressionDialog(dialog);
    auto id = dialog->findChild<QLineEdit *>("progressionId"); id->setText("study");
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click(); QVERIFY(dialog->isVisible());
    QVERIFY(!dialog->findChild<QLabel *>("progressionError")->text().isEmpty());
    id->setText("training-test"); dialog->reject(); guarded = true;
  });
  window.findChild<QAction *>("progressionAuthoringAction")->trigger(); QVERIFY(guarded);
  QCOMPARE(window.currentView().value("documents"), before);
}
