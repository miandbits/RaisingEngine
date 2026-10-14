#include "logic-editor.hpp"

void StudioTests::elementVisibilityConditionRoundtrip() {
  const auto root = qEnvironmentVariable("RAISING_VISIBILITY_PROJECT");
  if (root.isEmpty()) QSKIP("Set RAISING_VISIBILITY_PROJECT to a disposable project copy");
  StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath()));
  w.resize(1480, 960); w.show(); w.openProject(root);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && !w.projectData().empty(), 20000);
  w.findChild<QAction *>("addButton")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  const auto id = w.selectedId();
  auto element = [&] { for (auto s : w.projectData().value("visual").toObject().value("screens").toArray())
    if (s.toObject().value("id") == "home") for (auto v : s.toObject().value("elements").toArray())
      if (v.toObject().value("id") == id) return v.toObject(); return QJsonObject(); };
  auto edit = w.findChild<QPushButton *>("elementVisibleCondition");
  auto clear = w.findChild<QPushButton *>("elementVisibleConditionClear");
  QVERIFY(edit); QVERIFY(clear); QVERIFY(edit->isEnabled()); QVERIFY(!clear->isEnabled());
  const auto original = w.projectData(); const auto revision = w.currentView().value("revision");
  auto accept = [](QDialog *dialog) { dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click(); };
  auto choose = [](QDialog *dialog, QString name, QString value) {
    auto box = dialog->findChild<QComboBox *>(name); if (!box) return false;
    int index = box->findData(value); if (index < 0) return false;
    box->setCurrentIndex(index); QMetaObject::invokeMethod(box, "activated", Q_ARG(int, index));
    QTest::qWait(10); return true;
  };
  auto select = [](QDialog *dialog, QStringList path) {
    auto tree = dialog->findChild<QTreeWidget *>("logicTree");
    for (QTreeWidgetItemIterator i(tree); *i; ++i) if ((*i)->data(0, Qt::UserRole).toStringList() == path) {
      tree->setCurrentItem(*i); return true;
    } return false;
  };
  QTimer::singleShot(0, [&] { auto d = w.findChild<QDialog *>("elementVisibilityEditor"); QVERIFY(d); accept(d); });
  edit->click(); QCOMPARE(w.projectData(), original); QCOMPARE(w.currentView().value("revision"), revision);
  QTimer::singleShot(0, [&] {
    auto d = w.findChild<QDialog *>("elementVisibilityEditor"); QVERIFY(d);
    QTimer::singleShot(5000, d, &QDialog::reject);
    QVERIFY(choose(d, "logicType", "compare")); QVERIFY(choose(d, "logic-comparator", "gte"));
    QVERIFY(select(d, {"left"})); QVERIFY(choose(d, "logicType", "read"));
    QVERIFY(select(d, {"left", "target"})); QVERIFY(choose(d, "logic-attributeId", "money"));
    QVERIFY(select(d, {"right"})); auto value = d->findChild<QSpinBox *>("logic-value"); QVERIFY(value);
    value->setValue(10); QMetaObject::invokeMethod(value, "editingFinished"); accept(d);
  });
  edit->click(); QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  const auto condition = element().value("visibleCondition"); QVERIFY(condition.isObject());
  QCOMPARE(condition.toObject().value("comparator").toString(), QString("gte")); QVERIFY(clear->isEnabled());
  auto unchanged = element(); unchanged.remove("visibleCondition");
  QJsonObject beforeElement; for (auto s : original.value("visual").toObject().value("screens").toArray())
    if (s.toObject().value("id") == "home") for (auto v : s.toObject().value("elements").toArray())
      if (v.toObject().value("id") == id) beforeElement = v.toObject();
  QCOMPARE(unchanged, beforeElement);
  const auto configured = w.projectData();
  QTimer::singleShot(0, [&] { auto d = w.findChild<QDialog *>("elementVisibilityEditor"); QVERIFY(d);
    QVERIFY(choose(d, "logicType", "always")); d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Cancel)->click(); });
  edit->click(); QCOMPARE(w.projectData(), configured);
  w.findChild<QAction *>("undoEdit")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000); QCOMPARE(w.projectData(), original);
  w.findChild<QAction *>("redoEdit")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000); QCOMPARE(w.projectData(), configured);
  clear->click(); QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000); QVERIFY(!element().contains("visibleCondition"));
  w.findChild<QAction *>("undoEdit")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000); QCOMPARE(element().value("visibleCondition"), condition);
  w.findChild<QAction *>("saveProject")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  const auto saved = w.projectData(); w.openProject(root); QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000); QCOMPARE(w.projectData(), saved);
  QVERIFY(w.currentView().value("diagnostics").toObject().value("errors").toInt() == 0);
  // The same condition editor must block an unknown reference without losing the draft.
  auto bad = condition.toObject(); auto left = bad.value("left").toObject();
  left["target"] = QJsonObject{{"kind", "attribute"}, {"attributeId", "missing-attribute"}}; bad["left"] = left;
  LogicEditor invalid(bad, "condition", w.currentView()); invalid.show(); accept(&invalid);
  QVERIFY(invalid.isVisible()); QVERIFY(!invalid.findChild<QLabel *>("logicError")->text().isEmpty());
  QCOMPARE(invalid.value().toObject(), bad); invalid.reject();
}
