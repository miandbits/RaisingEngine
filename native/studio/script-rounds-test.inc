#include "script-rounds.hpp"

namespace {
QJsonObject roundsTestSettings(){
  auto effect=[](int amount){return QJsonArray{QJsonObject{{"op","add"},{"target",QJsonObject{{"kind","attribute"},{"attributeId","money"}}},{"value",QJsonObject{{"op","literal"},{"value",amount}}}}};};
  auto check=[](QString key,int value){return QJsonObject{{"op","compare"},{"comparator","gte"},{"left",QJsonObject{{"op","read"},{"target",QJsonObject{{"kind","attribute"},{"attributeId",key}}}}},{"right",QJsonObject{{"op","literal"},{"value",value}}}};};
  return {{"title","Synthetic rounds"},{"enterText","Enter"},{"leaveText","Leave"},{"requirements",check("money",10)},{"entryEffects",effect(-10)},{"winText","Won"},{"lossText","Lost"},{"withdrawText","Withdrew"},{"winEffects",effect(50)},{"lossEffects",effect(3)},
    {"rounds",QJsonArray{QJsonObject{{"label","First round"},{"choiceText","Attempt first"},{"choiceEffects",effect(-1)},{"condition",check("skill",0)},{"effects",effect(7)},{"resultText","First passed"}},QJsonObject{{"label","Second round"},{"choiceText","Attempt second"},{"choiceEffects",QJsonArray{}},{"condition",check("skill",5)},{"effects",effect(11)},{"resultText","Second passed"}}}}};
}
QJsonObject roundsTestStory(){return {{"formatVersion","0.1.0"},{"id","rounds-test"},{"label","Synthetic rounds fixture"},{"priority",100},{"requirements",QJsonObject{{"op","always"}}},{"window",QJsonObject{{"fromPeriod",0},{"throughPeriod",5}}},{"entryNodeId","original"},{"nodes",QJsonArray{QJsonObject{{"id","original"},{"type","dialogue"},{"speakerId",QJsonValue::Null},{"text","Existing continuation"},{"next","end"}},QJsonObject{{"id","end"},{"type","end"}}}}};}
QJsonObject roundsDocument(const StudioWindow &w,QString file){for(auto v:w.currentView().value("documents").toArray())if(v.toObject().value("file")==file)return v.toObject().value("data").toObject();return {};}
}

void StudioTests::scriptRoundsBuildAndPreserve(){
  auto story=roundsTestStory();const auto compiled=appendScriptRounds(story,roundsTestSettings());QVERIFY(!compiled.isEmpty());QVERIFY(scriptGraphIssues(compiled).isEmpty());QCOMPARE(compiled.value("nodes").toArray().size(),16);QCOMPARE(compiled.value("entryNodeId").toString(),QString("rounds-1-entry"));
  for(int i=0;i<2;++i)QCOMPARE(compiled.value("nodes").toArray()[i],story.value("nodes").toArray()[i]);
  auto unknown=story;unknown["futureMetadata"]="keep";auto nodes=unknown.value("nodes").toArray();auto old=nodes[0].toObject();old["futureNodeField"]=42;nodes[0]=old;unknown["nodes"]=nodes;
  const auto spliced=appendScriptRounds(unknown,roundsTestSettings(),"original");QCOMPARE(spliced.value("entryNodeId"),unknown.value("entryNodeId"));QCOMPARE(spliced.value("futureMetadata"),unknown.value("futureMetadata"));auto actual=spliced.value("nodes").toArray()[0].toObject();old["next"]="rounds-1-entry";QCOMPARE(actual,old);QVERIFY(scriptGraphIssues(spliced).isEmpty());
  const auto second=appendScriptRounds(compiled,roundsTestSettings());QCOMPARE(second.value("entryNodeId").toString(),QString("rounds-2-entry"));QVERIFY(scriptGraphIssues(second).isEmpty());QVERIFY(appendScriptRounds(story,roundsTestSettings(),"end").isEmpty());QVERIFY(appendScriptRounds(story,roundsTestSettings(),"missing").isEmpty());auto bad=roundsTestSettings();bad["rounds"]=QJsonArray{};QVERIFY(appendScriptRounds(story,bad).isEmpty());
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());const auto fixture=engine+"/tests/fixtures/script-rounds.json";
  if(qEnvironmentVariableIsSet("RAISING_EXPORT_ROUNDS_FIXTURE")){QDir().mkpath(QFileInfo(fixture).absolutePath());QFile f(fixture);QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(compiled).toJson());}
  QFile f(fixture);QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(QJsonDocument::fromJson(f.readAll()).object(),compiled);
}

void StudioTests::scriptRoundsDialogAndTransaction(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());QTemporaryDir temp(engine+"/.cache/script-rounds-XXXXXX");QVERIFY(temp.isValid());QVERIFY(copyScreenSettingsFixture(engine+"/examples/academy-visual",temp.path()));
  StudioWindow w(engine);w.show();w.openProject(temp.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));const auto before=w.currentView().value("documents");const int revision=w.currentView().value("revision").toInt();QString file;QJsonObject changed;
  QTimer::singleShot(100,&w,[&]{auto d=w.findChild<QDialog*>("scriptAuthoringDialog");QVERIFY(d);QTimer::singleShot(20000,d,&QDialog::reject);
    // The helper's cancellation cannot change the outer draft.
    auto author=static_cast<ScriptAuthoringDialog*>(d);QVERIFY(author->edits().isEmpty());QTimer::singleShot(0,[&]{auto r=w.findChild<QDialog*>("scriptRoundsDialog");QVERIFY(r);r->findChild<QLineEdit*>("rounds-title")->setText("Canceled title");r->reject();});d->findChild<QPushButton*>("scriptRounds")->click();QVERIFY(author->edits().isEmpty());
    QTimer::singleShot(0,[&]{auto r=w.findChild<QDialog*>("scriptRoundsDialog");QVERIFY(r);QTimer::singleShot(15000,r,&QDialog::reject);r->findChild<QComboBox*>("roundsAttachment")->setCurrentIndex(0);r->findChild<QLineEdit*>("rounds-title")->setText("Authored two-round flow");r->findChild<QPushButton*>("roundsAdd")->click();auto table=r->findChild<QTableWidget*>("roundsTable");QCOMPARE(table->rowCount(),2);table->item(1,0)->setText("Second authored round");
      QTimer::singleShot(0,[&]{auto logic=w.findChild<QDialog*>("logicEditor");QVERIFY(logic);logic->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});r->findChild<QPushButton*>("rounds-requirements")->click();
      r->findChild<QLineEdit*>("rounds-title")->clear();r->findChild<QDialogButtonBox*>("roundsButtons")->button(QDialogButtonBox::Apply)->click();QVERIFY(r->isVisible());QVERIFY(!r->findChild<QLabel*>("roundsError")->text().isEmpty());r->findChild<QLineEdit*>("rounds-title")->setText("Authored two-round flow");r->findChild<QDialogButtonBox*>("roundsButtons")->button(QDialogButtonBox::Apply)->click();});
    d->findChild<QPushButton*>("scriptRounds")->click();QCOMPARE(author->edits().size(),1);file=author->edits()[0].toObject().value("file").toString();changed=author->edits()[0].toObject().value("data").toObject();QVERIFY(scriptGraphIssues(changed).isEmpty());d->findChild<QDialogButtonBox*>("scriptButtons")->button(QDialogButtonBox::Save)->click();});
  w.findChild<QAction*>("scriptAuthoringAction")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));QCOMPARE(w.currentView().value("revision").toInt(),revision+1);QCOMPARE(roundsDocument(w,file),changed);
  // A real service revision change while the workshop is open rejects its draft.
  QTimer::singleShot(100,&w,[&]{auto d=w.findChild<QDialog*>("scriptAuthoringDialog");QVERIFY(d);QTimer::singleShot(20000,d,&QDialog::reject);auto author=static_cast<ScriptAuthoringDialog*>(d);
    QTimer::singleShot(0,[&]{auto r=w.findChild<QDialog*>("scriptRoundsDialog");QVERIFY(r);r->findChild<QDialogButtonBox*>("roundsButtons")->button(QDialogButtonBox::Apply)->click();});d->findChild<QPushButton*>("scriptRounds")->click();QCOMPARE(author->edits().size(),1);const auto draft=author->edits();
    const auto previousRevision=w.currentView().value("revision");w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY(w.currentView().value("revision")!=previousRevision);d->findChild<QDialogButtonBox*>("scriptButtons")->button(QDialogButtonBox::Save)->click();QVERIFY(d->isVisible());QVERIFY(!d->findChild<QLabel*>("scriptError")->text().isEmpty());QCOMPARE(author->edits(),draft);d->reject();});
  w.findChild<QAction*>("scriptAuthoringAction")->trigger();w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(roundsDocument(w,file),changed);
  w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);for(auto v:before.toArray()){const auto doc=v.toObject();QCOMPARE(roundsDocument(w,doc.value("file").toString()),doc.value("data").toObject());}
  w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(roundsDocument(w,file),changed);w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);w.openProject(temp.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(roundsDocument(w,file),changed);
}
