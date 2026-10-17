#include "relationship-editor.hpp"

void StudioTests::narrativeMetadataAndReferenceRepair() {
  auto context = scriptTestContext(); auto docs = context["documents"].toArray();
  auto doc = docs[0].toObject(), story = doc["data"].toObject();
  story["window"] = QJsonObject{{"fromPeriod",0},{"throughPeriod",5}}; story["priority"] = 20;
  auto nodes = story["nodes"].toArray();
  nodes.append(QJsonObject{{"id","question"},{"type","choice"},{"prompt","Choose"},{"choices",QJsonArray{QJsonObject{{"id","yes"},{"text","Yes"},{"next","end"},{"requirements",QJsonObject{{"op","always"}}},{"effects",QJsonArray{}}}}}});
  story["nodes"] = nodes; doc["data"] = story; docs[0] = doc;
  const QJsonObject choice{{"eventId","test-event"},{"nodeId","question"},{"choiceId","yes"}};
  auto other = choice; other["eventId"] = "different-event";
  const QJsonObject scenario{{"commands",QJsonArray{QJsonObject{{"type","advancePeriod"},{"choices",QJsonArray{choice,other}}}}},{"description","question yes"}};
  docs.append(QJsonObject{{"kind","scenario"},{"file","scenarios/route.json"},{"data",scenario}}); context["documents"] = docs;
  ScriptAuthoringDialog dialog(context, {}); dialog.show();
  dialog.findChild<QLineEdit *>("scriptLabel")->setText("A new story");
  dialog.findChild<QSpinBox *>("scriptFromPeriod")->setValue(2);
  dialog.findChild<QSpinBox *>("scriptThroughPeriod")->setValue(4);
  dialog.findChild<QSpinBox *>("scriptPriority")->setValue(30);
  bool conditionEdited = false;
  QTimer::singleShot(0, [&] {
    auto logic = qobject_cast<QDialog *>(QApplication::activeModalWidget()); QVERIFY(logic);
    logic->findChild<QSpinBox *>("quickAmount")->setValue(48);
    logic->findChild<QPushButton *>("quickApply")->click();
    logic->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click(); conditionEdited = true;
  });
  dialog.findChild<QPushButton *>("scriptRequirements")->click(); QVERIFY(conditionEdited);
  scriptSelectNode(dialog,"question");
  dialog.findChild<QLineEdit *>("scriptNodeId")->setText("decision"); dialog.findChild<QPushButton *>("scriptNodeRename")->click();
  auto table = dialog.findChild<QTableWidget *>("scriptChoices"); QVERIFY(table);
  table->item(0,0)->setText("accept");
  auto changes = dialog.edits(); QCOMPARE(changes.size(),2);
  auto updated = changes[0].toObject()["data"].toObject();
  QCOMPARE(updated["window"].toObject()["fromPeriod"].toInt(),1);
  QCOMPARE(updated["window"].toObject()["throughPeriod"].toInt(),3);
  QCOMPARE(updated["requirements"].toObject()["right"].toObject()["value"].toInt(),48);
  auto updatedScenario = changes[1].toObject()["data"].toObject();
  auto choices = updatedScenario["commands"].toArray()[0].toObject()["choices"].toArray();
  QCOMPARE(choices[0].toObject()["nodeId"].toString(),QString("decision"));
  QCOMPARE(choices[0].toObject()["choiceId"].toString(),QString("accept"));
  QCOMPARE(choices[1].toObject(),other); QCOMPARE(updatedScenario["description"],scenario["description"]);
  // Removing a referenced choice/node is refused, rather than stranding a route.
  dialog.findChild<QPushButton *>("scriptNodeRemove")->click();
  QVERIFY(dialog.findChild<QLabel *>("scriptError")->text().contains("scenarios/route.json"));
  dialog.findChild<QSpinBox *>("scriptFromPeriod")->setValue(5);
  dialog.findChild<QDialogButtonBox *>("scriptButtons")->button(QDialogButtonBox::Save)->click();
  QVERIFY(dialog.result()!=QDialog::Accepted); QVERIFY(dialog.findChild<QLabel *>("scriptError")->text().contains("발생 시작"));
  dialog.findChild<QSpinBox *>("scriptFromPeriod")->setValue(2);
  dialog.findChild<QDialogButtonBox *>("scriptButtons")->button(QDialogButtonBox::Save)->click();
  QCOMPARE(dialog.result(),int(QDialog::Accepted)); QCOMPARE(context["documents"].toArray()[0],doc);
}

void StudioTests::relationshipFormsPreserveAndGuard() {
  QJsonObject relation{{"id","trust"},{"label","Trust"},{"min",0},{"max",100},{"initial",1},{"description","keep description"}};
  QJsonObject character{{"id","mentor"},{"relationships",QJsonArray{relation}}};
  QJsonObject context{{"documents",QJsonArray{QJsonObject{{"file","events/help.json"},{"data",QJsonObject{{"target",QJsonObject{{"kind","relationship"},{"characterId","mentor"},{"relationshipId","trust"}}}}}}}}};
  RelationshipEditor editor(character,context); auto table=editor.findChild<QTableWidget *>("relationshipTable");
  QString error; QCOMPARE(editor.value(&error),character["relationships"].toArray()); QVERIFY(error.isEmpty());
  table->item(0,1)->setText("신뢰"); editor.findChild<QPushButton *>("relationshipAdd")->click();
  table->item(1,0)->setText("respect"); table->item(1,1)->setText("존경");
  auto values=editor.value(&error); QCOMPARE(values.size(),2); QCOMPARE(values[0].toObject()["description"],relation["description"]);
  table->model()->setData(table->model()->index(0,4),"2",Qt::EditRole);
  table->model()->setData(table->model()->index(0,5),"30",Qt::EditRole);
  values=editor.value(&error); QCOMPARE(values[0].toObject()["initial"].toInt(),2); QCOMPARE(values[0].toObject()["max"].toInt(),30);
  table->item(1,4)->setText("101"); editor.value(&error); QVERIFY(error.contains("최소"));
  table->item(1,4)->setText("invalid"); error.clear(); editor.value(&error); QVERIFY(error.contains("정수"));
  table->item(1,4)->setText("2147483648"); error.clear(); editor.value(&error); QVERIFY(error.contains("정수"));
  table->item(1,4)->setText("5"); error.clear(); QCOMPARE(editor.value(&error).size(),2); QVERIFY(error.isEmpty());
  table->setCurrentCell(0,0); editor.findChild<QPushButton *>("relationshipRemove")->click();
  editor.value(&error); QVERIFY(error.contains("events/help.json")); QCOMPARE(character["relationships"].toArray().size(),1);
}

void StudioTests::contentFormsEntryAndAtomicReferences() {
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  QTemporaryDir temp(engine+"/.cache/qt-first-time-XXXXXX"); QVERIFY(temp.isValid()); QVERIFY(copyContentFixture(engine+"/examples/academy-demo",temp.path()));
  StudioWindow window(engine); window.show(); window.openProject(temp.path());
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy()&&!window.projectData().isEmpty(),20000);
  const auto originalEvent=contentDocument(window,"events/exam-invitation.json");
  const auto originalRoute=contentDocument(window,"scenarios/certified-route.json");
  QVERIFY(!originalEvent.isEmpty()); QVERIFY(!originalRoute.isEmpty());
  auto list=window.findChild<QListWidget *>("contentList"); QVERIFY(list);
  for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toString().endsWith("events/exam-invitation.json"))list->setCurrentRow(i);
  bool edited=false;
  QTimer::singleShot(30,&window,[&]{
    auto dialog=window.findChild<QDialog *>("scriptAuthoringDialog"); QVERIFY(dialog);
    QVERIFY(dialog->findChild<QComboBox *>("scriptDocument")->currentText().contains("exam-invitation.json"));
    dialog->findChild<QLineEdit *>("scriptLabel")->setText("독맥의 비밀");
    auto nodes=dialog->findChild<QListWidget *>("scriptNodes");
    for(int i=0;i<nodes->count();++i)if(nodes->item(i)->data(Qt::UserRole)=="registration")nodes->setCurrentRow(i);
    dialog->findChild<QLineEdit *>("scriptNodeId")->setText("decision"); dialog->findChild<QPushButton *>("scriptNodeRename")->click();
    auto choices=dialog->findChild<QTableWidget *>("scriptChoices"); QVERIFY(choices); choices->item(0,0)->setText("accept");
    dialog->findChild<QDialogButtonBox *>("scriptButtons")->button(QDialogButtonBox::Save)->click(); edited=true;
  });
  window.findChild<QPushButton *>("contentEdit")->click(); QVERIFY(edited);
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy(),10000); QVERIFY2(window.lastError.isEmpty(),qPrintable(window.lastError));
  QVERIFY(contentDocument(window,"scenarios/certified-route.json")!=originalRoute);
  window.findChild<QAction *>("undoEdit")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(),10000);
  QCOMPARE(contentDocument(window,"events/exam-invitation.json"),originalEvent); QCOMPARE(contentDocument(window,"scenarios/certified-route.json"),originalRoute);
  window.findChild<QAction *>("redoEdit")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(),10000);
  const auto expectedRoute=contentDocument(window,"scenarios/certified-route.json");
  window.findChild<QAction *>("saveProject")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(),10000);
  window.openProject(temp.path()); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(),20000); QCOMPARE(contentDocument(window,"scenarios/certified-route.json"),expectedRoute);
  bool effectEdited=false;
  QTimer::singleShot(30,&window,[&]{
    auto dialog=window.findChild<QDialog *>("contentEditor"); QVERIFY(dialog);
    auto tabs=dialog->findChild<QTabWidget *>(); QVERIFY(tabs); const int advanced=tabs->count()-1;
    tabs->setCurrentIndex(advanced); QVERIFY(tabs->isTabEnabled(0)); tabs->setCurrentIndex(0);
    QVERIFY(dialog->findChild<QLineEdit *>("contentLabel")->isEnabled());
    QTimer::singleShot(0,dialog,[&]{
      auto logic=window.findChild<QDialog *>("logicEditor"); QVERIFY(logic);
      auto target=logic->findChild<QComboBox *>("quickTarget"); int relation=-1;
      for(int i=0;i<target->count();++i)if(target->itemData(i).toJsonObject()["kind"]=="relationship")relation=i;
      QVERIFY(relation>=0); target->setCurrentIndex(relation); logic->findChild<QSpinBox *>("quickAmount")->setValue(2);
      logic->findChild<QPushButton *>("quickApply")->click(); logic->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    });
    dialog->findChild<QPushButton *>("contentEffectsQuick")->click();
    dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click(); effectEdited=true;
  });
  QVERIFY(openContentDocument(window,"activities/rest.json")); QVERIFY(effectEdited); QTRY_VERIFY_WITH_TIMEOUT(!window.busy(),10000);
  const auto effects=contentDocument(window,"activities/rest.json")["baseEffects"].toArray();
  QCOMPARE(effects.last().toObject()["target"].toObject()["kind"].toString(),QString("relationship"));
  QCOMPARE(effects.last().toObject()["value"].toObject()["value"].toInt(),2);
}
