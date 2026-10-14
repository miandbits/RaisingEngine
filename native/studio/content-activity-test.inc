#include <QCheckBox>
#include <QTemporaryDir>

namespace {
bool copyContentFixture(const QString &source, const QString &target) {
  QDirIterator files(source, QDir::Files, QDirIterator::Subdirectories);
  while (files.hasNext()) {
    const auto path = files.next(), dest = target + "/" + QDir(source).relativeFilePath(path);
    if (!QDir().mkpath(QFileInfo(dest).path()) || !QFile::copy(path, dest)) return false;
  }
  return true;
}
QJsonObject contentDocument(StudioWindow &window, const QString &suffix) {
  for (auto value : window.currentView().value("documents").toArray()) {
    const auto doc = value.toObject();
    if (doc.value("file").toString().endsWith(suffix)) return doc.value("data").toObject();
  }
  return {};
}
bool openContentDocument(StudioWindow &window, const QString &suffix) {
  auto list = window.findChild<QListWidget *>("contentList");
  if (!list) return false;
  for (int i = 0; i < list->count(); ++i) {
    auto item = list->item(i);
    if (!item->data(Qt::UserRole).toString().endsWith(suffix)) continue;
    list->setCurrentItem(item);
    return QMetaObject::invokeMethod(list, "itemDoubleClicked", Qt::DirectConnection, Q_ARG(QListWidgetItem *, item));
  }
  return false;
}
}

void StudioTests::contentActivityLogicRoundtrip() {
  const auto engine = qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath());
  QTemporaryDir temp(engine + "/.cache/qt-content-activity-XXXXXX");
  QVERIFY(temp.isValid()); QVERIFY(copyContentFixture(engine + "/examples/academy-adventure", temp.path()));
  StudioWindow window(engine); window.show(); window.openProject(temp.path());
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy() && !window.projectData().isEmpty(), 20000);
  const auto original = contentDocument(window, "activities/rest.json");
  QVERIFY(!original.isEmpty()); QVERIFY(!original.contains("successCheck"));
  bool edited = false, chanceEdited = false;
  QTimer::singleShot(50, &window, [&] {
    auto dialog = window.findChild<QDialog *>("contentEditor"); QVERIFY(dialog);
    auto enabled = dialog->findChild<QCheckBox *>("contentSuccessEnabled"); QVERIFY(enabled);
    auto chance = dialog->findChild<QPushButton *>("contentLogic-chance"); QVERIFY(chance);
    QVERIFY(!chance->isEnabled()); enabled->setChecked(true); QVERIFY(chance->isEnabled());
    QTimer::singleShot(20, dialog, [&] {
      auto logic = window.findChild<QDialog *>("logicEditor"); QVERIFY(logic);
      auto number = logic->findChild<QSpinBox *>("logic-value"); QVERIFY(number);
      number->setValue(7500); QMetaObject::invokeMethod(number, "editingFinished", Qt::DirectConnection);
      logic->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click(); chanceEdited = true;
    });
    chance->click(); QVERIFY(chanceEdited);
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click(); edited = true;
  });
  QVERIFY(openContentDocument(window, "activities/rest.json")); QVERIFY(edited);
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000);
  QVERIFY2(window.lastError.isEmpty(), qPrintable(window.lastError));
  auto actual = contentDocument(window, "activities/rest.json");
  auto expected = original;
  expected["successCheck"] = QJsonObject{{"chance", QJsonObject{{"op", "literal"}, {"value", 7500}}}};
  expected["successEffects"] = QJsonArray(); expected["failureEffects"] = QJsonArray();
  QCOMPARE(actual, expected); // Preserve presentation and every unrelated field.
  window.findChild<QAction *>("undoEdit")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000);
  QCOMPARE(contentDocument(window, "activities/rest.json"), original);
  window.findChild<QAction *>("redoEdit")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000);
  window.findChild<QAction *>("saveProject")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000);
  window.openProject(temp.path()); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 20000);
  QCOMPARE(contentDocument(window, "activities/rest.json"), expected);
  // Disabling a check removes its dependent effects, but Cancel keeps the saved draft.
  QTimer::singleShot(50, &window, [&] {
    auto dialog = window.findChild<QDialog *>("contentEditor"); QVERIFY(dialog);
    dialog->findChild<QCheckBox *>("contentSuccessEnabled")->setChecked(false); dialog->reject();
  });
  QVERIFY(openContentDocument(window, "activities/rest.json"));
  QCOMPARE(contentDocument(window, "activities/rest.json"), expected);
}

void StudioTests::contentAttributesAuthoringGuards() {
  const auto engine = qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath());
  QTemporaryDir temp(engine + "/.cache/qt-content-attributes-XXXXXX");
  QVERIFY(temp.isValid()); QVERIFY(copyContentFixture(engine + "/examples/academy-adventure", temp.path()));
  StudioWindow window(engine); window.show(); window.openProject(temp.path());
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy() && !window.projectData().isEmpty(), 20000);
  const auto original = contentDocument(window, "attributes.json");
  bool edited = false;
  QTimer::singleShot(50, &window, [&] {
    auto dialog = window.findChild<QDialog *>("contentEditor"); QVERIFY(dialog);
    auto table = dialog->findChild<QTableWidget *>("attributeTable"); QVERIFY(table);
    auto save = dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save);
    const int before = table->rowCount();
    // Existing referenced skill cannot disappear through a row-delete shortcut.
    table->setCurrentCell(0, 0); dialog->findChild<QPushButton *>("contentAttributeRemove")->click();
    QCOMPARE(table->rowCount(), before); QVERIFY(!dialog->findChild<QLabel *>("contentError")->text().isEmpty());
    dialog->findChild<QPushButton *>("contentAttributeAdd")->click();
    QCOMPARE(table->rowCount(), before + 1);
    table->item(before, 0)->setText("money"); table->item(before, 1)->setText("Education experience");
    save->click(); QVERIFY(dialog->isVisible()); // Duplicate IDs keep the draft open.
    table->item(before, 0)->setText("education-xp");
    table->item(before, 2)->setText("1.5"); save->click(); QVERIFY(dialog->isVisible());
    table->item(before, 2)->setText("5"); table->item(before, 4)->setText("4");
    save->click(); QVERIFY(dialog->isVisible());
    table->item(before, 4)->setText("1000");
    save->click(); edited = true;
  });
  QVERIFY(openContentDocument(window, "attributes.json")); QVERIFY(edited);
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000);
  QVERIFY2(window.lastError.isEmpty(), qPrintable(window.lastError));
  auto actual = contentDocument(window, "attributes.json");
  auto entries = actual.value("attributes").toArray();
  QCOMPARE(entries.size(), original.value("attributes").toArray().size() + 1);
  for (int i = 0; i < entries.size() - 1; ++i) QCOMPARE(entries[i], original.value("attributes").toArray()[i]);
  QCOMPARE(entries.last().toObject().value("id").toString(), QString("education-xp"));
  QCOMPARE(entries.last().toObject().value("initial").toInt(), 5);
  auto restoredHeader = actual; restoredHeader["attributes"] = original.value("attributes");
  QCOMPARE(restoredHeader, original);
  window.findChild<QAction *>("undoEdit")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000);
  QCOMPARE(contentDocument(window, "attributes.json"), original);
}

void StudioTests::contentActivityPreviewRoundtrip() {
  const auto engine = qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath());
  QTemporaryDir temp(engine + "/.cache/qt-activity-preview-XXXXXX"); QVERIFY(temp.isValid());
  QVERIFY(copyContentFixture(engine + "/examples/academy-adventure", temp.path()));
  StudioWindow window(engine); window.show(); window.openProject(temp.path());
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy() && !window.projectData().isEmpty(), 20000);
  const auto original = contentDocument(window, "activities/rest.json");
  bool edited = false;
  QTimer::singleShot(50, &window, [&] {
    auto dialog = window.findChild<QDialog *>("contentEditor"); QVERIFY(dialog);
    QTimer::singleShot(10000, dialog, &QDialog::reject);
    auto table = dialog->findChild<QTableWidget *>("contentPreviewAttributes"); QVERIFY(table);
    QCOMPARE(table->rowCount(), 0);
    auto add = dialog->findChild<QPushButton *>("contentPreviewAdd"); add->click(); add->click();
    auto ref = qobject_cast<QComboBox *>(table->cellWidget(0, 0)); ref->setCurrentIndex(ref->findData("knowledge"));
    table->item(0, 1)->setText("Current level");
    ref = qobject_cast<QComboBox *>(table->cellWidget(1, 0)); ref->setCurrentIndex(ref->findData("money"));
    table->item(1, 1)->setText("Cost <literal>"); table->item(1, 2)->setText("$& {value} G");
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click(); edited = true;
  });
  QVERIFY(openContentDocument(window, "activities/rest.json")); QVERIFY(edited);
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000); QVERIFY2(window.lastError.isEmpty(), qPrintable(window.lastError));
  auto expected = original;
  expected["previewAttributes"] = QJsonArray{QJsonObject{{"attributeId", "knowledge"}, {"label", "Current level"}},
                                            QJsonObject{{"attributeId", "money"}, {"label", "Cost <literal>"}, {"valueFormat", "$& {value} G"}}};
  QCOMPARE(contentDocument(window, "activities/rest.json"), expected);
  window.findChild<QAction *>("saveProject")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000);
  window.openProject(temp.path()); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 20000);
  QCOMPARE(contentDocument(window, "activities/rest.json"), expected);
  QTimer::singleShot(50, &window, [&] {
    auto dialog = window.findChild<QDialog *>("contentEditor"); QVERIFY(dialog);
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
  });
  QVERIFY(openContentDocument(window, "activities/rest.json")); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000);
  QCOMPARE(contentDocument(window, "activities/rest.json"), expected); // Optional format stays absent when untouched.
  QTimer::singleShot(50, &window, [&] {
    auto dialog = window.findChild<QDialog *>("contentEditor"); QVERIFY(dialog);
    auto table = dialog->findChild<QTableWidget *>("contentPreviewAttributes");
    while (table->rowCount()) { table->setCurrentCell(0, 0); dialog->findChild<QPushButton *>("contentPreviewRemove")->click(); }
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
  });
  QVERIFY(openContentDocument(window, "activities/rest.json")); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000);
  QCOMPARE(contentDocument(window, "activities/rest.json"), original);
}
