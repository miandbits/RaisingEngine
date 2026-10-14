#include <QDoubleSpinBox>
namespace {
QJsonObject portraitTransformContext(QJsonObject portrait) {
  auto context = scriptTestContext(); auto docs = context.value("documents").toArray(); auto doc = docs[0].toObject(); auto story = doc.value("data").toObject(); auto nodes = story.value("nodes").toArray();
  nodes.prepend(QJsonObject{{"id", "scene"}, {"type", "scene"}, {"scene", QJsonObject{{"portraits", QJsonArray{portrait}}}}, {"next", "line"}});
  story["nodes"] = nodes; story["entryNodeId"] = "scene"; doc["data"] = story; docs[0] = doc;
  docs.append(QJsonObject{{"kind", "character"}, {"file", "characters/actor.json"}, {"data", QJsonObject{{"id", "actor"}, {"name", "Actor"}, {"portraits", QJsonObject{{"dance", "image-dance"}}}, {"relationships", QJsonArray{}}}}});
  context["documents"] = docs; return context;
}
QJsonObject transformedPortrait(const QJsonArray &edits) {
  const auto nodes = edits[0].toObject().value("data").toObject().value("nodes").toArray();
  return nodes[0].toObject().value("scene").toObject().value("portraits").toArray()[0].toObject();
}
}
void StudioTests::scriptPortraitTransformRoundtrip() {
  const QJsonObject original{{"characterId", "actor"}, {"portraitId", "dance"}, {"position", "center"}};
  auto context = portraitTransformContext(original); ScriptAuthoringDialog dialog(context, {}); dialog.show(); scriptSelectNode(dialog, "scene");
  QTimer watchdog; watchdog.setInterval(10000); connect(&watchdog, &QTimer::timeout, &dialog, [&] { if (auto d = dialog.findChild<QDialog *>("scriptPortraitDialog")) d->reject(); }); watchdog.start();
  QTimer::singleShot(0, &dialog, [&] { auto child = dialog.findChild<QDialog *>("scriptPortraitDialog"); QVERIFY(child); child->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click(); });
  dialog.findChild<QPushButton *>("scriptPortraits")->click(); QVERIFY(dialog.edits().isEmpty());
  bool changed = false;
  QTimer::singleShot(0, &dialog, [&] {
    auto child = dialog.findChild<QDialog *>("scriptPortraitDialog"); QVERIFY(child);
    child->findChild<QDoubleSpinBox *>("scriptPortrait-scale")->setValue(20);
    child->findChild<QDoubleSpinBox *>("scriptPortrait-offsetX")->setValue(10);
    child->findChild<QDoubleSpinBox *>("scriptPortrait-offsetY")->setValue(-5);
    child->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click(); changed = true;
  });
  dialog.findChild<QPushButton *>("scriptPortraits")->click(); QVERIFY(changed); QCOMPARE(dialog.edits().size(), 1);
  auto expected = original; expected["scale"] = .2; expected["offsetX"] = .1; expected["offsetY"] = -.05; QCOMPARE(transformedPortrait(dialog.edits()), expected);
  const auto before = dialog.edits();
  QTimer::singleShot(0, &dialog, [&] { auto child = dialog.findChild<QDialog *>("scriptPortraitDialog"); QVERIFY(child); child->findChild<QDoubleSpinBox *>("scriptPortrait-scale")->setValue(300); child->reject(); });
  dialog.findChild<QPushButton *>("scriptPortraits")->click(); QCOMPARE(dialog.edits(), before);
  dialog.findChild<QDialogButtonBox *>("scriptButtons")->button(QDialogButtonBox::Save)->click(); QCOMPARE(dialog.result(), int(QDialog::Accepted));
  auto docs = context.value("documents").toArray(); auto doc = docs[0].toObject(); doc["data"] = dialog.edits()[0].toObject().value("data"); docs[0] = doc; context["documents"] = docs;
  ScriptAuthoringDialog reopened(context, {}); reopened.show(); scriptSelectNode(reopened, "scene");
  QTimer::singleShot(0, &reopened, [&] { auto child = reopened.findChild<QDialog *>("scriptPortraitDialog"); QVERIFY(child); QCOMPARE(child->findChild<QDoubleSpinBox *>("scriptPortrait-scale")->value(), 20.0); QCOMPARE(child->findChild<QDoubleSpinBox *>("scriptPortrait-offsetX")->value(), 10.0); QCOMPARE(child->findChild<QDoubleSpinBox *>("scriptPortrait-offsetY")->value(), -5.0); child->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click(); });
  reopened.findChild<QPushButton *>("scriptPortraits")->click(); QVERIFY(reopened.edits().isEmpty());
}

void StudioTests::scriptPortraitTransformPreservesPrecision() {
  const QJsonObject original{{"characterId", "actor"}, {"portraitId", "dance"}, {"position", "center"}, {"scale", .123456789}, {"offsetX", -.987654321}, {"offsetY", .000012345}};
  ScriptAuthoringDialog dialog(portraitTransformContext(original), {}); dialog.show(); scriptSelectNode(dialog, "scene");
  QTimer::singleShot(0, &dialog, [&] {
    auto child = dialog.findChild<QDialog *>("scriptPortraitDialog"); QVERIFY(child);
    child->findChild<QPushButton *>("scriptPortraitAdd")->click();
    auto table = child->findChild<QTableWidget *>("scriptPortraitRows"); QCOMPARE(table->rowCount(), 2); table->setCurrentCell(1, 0); child->findChild<QPushButton *>("scriptPortraitRemove")->click();
    child->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
  });
  dialog.findChild<QPushButton *>("scriptPortraits")->click(); QVERIFY(dialog.edits().isEmpty());
  QTimer::singleShot(0, &dialog, [&] {
    auto child = dialog.findChild<QDialog *>("scriptPortraitDialog"); QVERIFY(child);
    auto scale = child->findChild<QDoubleSpinBox *>("scriptPortrait-scale"); QCOMPARE(scale->minimum(), 10.0); QCOMPARE(scale->maximum(), 400.0); scale->setValue(100);
    auto offset = child->findChild<QDoubleSpinBox *>("scriptPortrait-offsetX"); QCOMPARE(offset->minimum(), -200.0); QCOMPARE(offset->maximum(), 200.0);
    child->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
  });
  dialog.findChild<QPushButton *>("scriptPortraits")->click(); auto expected = original; expected["scale"] = 1; QCOMPARE(transformedPortrait(dialog.edits()), expected);
}
