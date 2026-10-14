#include "script-authoring.hpp"
#include <QGraphicsView>
#include <QGraphicsScene>

namespace {
QJsonObject scriptTestContext(){
  const QJsonArray nodes{QJsonObject{{"id","line"},{"type","dialogue"},{"speakerId",QJsonValue::Null},{"text","Original line"},{"next","end"}},QJsonObject{{"id","end"},{"type","end"}}};
  const QJsonObject story{{"id","test-event"},{"label","Test story"},{"entryNodeId","line"},{"requirements",QJsonObject{{"op","always"}}},{"nodes",nodes}};
  const QJsonArray attributes{QJsonObject{{"id","skill"},{"label","Skill"},{"min",0},{"max",100}}};
  const QJsonArray documents{QJsonObject{{"kind","event"},{"file","events/test-event.json"},{"data",story}},QJsonObject{{"kind","attributes"},{"file","attributes.json"},{"data",QJsonObject{{"attributes",attributes}}}}};
  return {{"documents",documents}};
}
void scriptSelectNode(ScriptAuthoringDialog &dialog,QString id){auto list=dialog.findChild<QListWidget*>("scriptNodes");for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole)==id){list->setCurrentRow(i);return;}}
}
void StudioTests::scriptAuthoringCreateConnectRoundTrip(){
  ScriptAuthoringDialog dialog(scriptTestContext(),{});dialog.show();
  dialog.resize(1380,1100);QApplication::processEvents();auto typeLabel=dialog.findChild<QLabel*>("scriptNodeType");auto dialogueNext=dialog.findChild<QComboBox*>("script-next");QVERIFY(typeLabel->height()<50);QVERIFY(typeLabel->y()<180);QVERIFY(dialogueNext->geometry().bottom()<450);dialog.resize(1380,880);QApplication::processEvents();
  auto kind=dialog.findChild<QComboBox*>("scriptNodeKind");auto add=dialog.findChild<QPushButton*>("scriptNodeAdd");
  auto create=[&](QString type){kind->setCurrentIndex(kind->findData(type));add->click();};
  create("choice");dialog.findChild<QPlainTextEdit*>("script-prompt")->setPlainText("Choose a route");dialog.findChild<QPushButton*>("scriptChoiceAdd")->click();
  auto choices=dialog.findChild<QTableWidget*>("scriptChoices");QCOMPARE(choices->rowCount(),2);choices->item(0,1)->setText("Study");choices->item(1,1)->setText("Rest");
  create("dialogue");dialog.findChild<QPlainTextEdit*>("script-text")->setPlainText("Study result");dialog.findChild<QLineEdit*>("scriptNodeId")->setText("study-line");dialog.findChild<QPushButton*>("scriptNodeRename")->click();
  create("branch");auto thenField=dialog.findChild<QComboBox*>("script-then");thenField->setCurrentIndex(thenField->findData("study-line"));
  QTimer::singleShot(0,[&]{auto editor=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(editor);QCOMPARE(editor->objectName(),QString("logicEditor"));editor->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});
  dialog.findChild<QPushButton*>("script-condition")->click();
  create("effect");create("scene");
  scriptSelectNode(dialog,"line");auto next=dialog.findChild<QComboBox*>("script-next");next->setCurrentIndex(next->findData("choice-1"));
  scriptSelectNode(dialog,"choice-1");choices=dialog.findChild<QTableWidget*>("scriptChoices");auto first=qobject_cast<QComboBox*>(choices->cellWidget(0,2));first->setCurrentIndex(first->findData("branch-1"));
  dialog.findChild<QPushButton*>("scriptNodeDuplicate")->click();QCOMPARE(dialog.findChild<QLineEdit*>("scriptNodeId")->text(),QString("copy-1"));
  dialog.findChild<QDialogButtonBox*>("scriptButtons")->button(QDialogButtonBox::Save)->click();QCOMPARE(dialog.result(),int(QDialog::Accepted));
  const auto edits=dialog.edits();QCOMPARE(edits.size(),1);const auto data=edits.first().toObject().value("data").toObject();QVERIFY(scriptGraphIssues(data).isEmpty());QCOMPARE(data.value("nodes").toArray().size(),8);
  QJsonObject branch;for(auto v:data.value("nodes").toArray())if(v.toObject().value("id")=="branch-1")branch=v.toObject();QCOMPARE(branch.value("then").toString(),QString("study-line"));QCOMPARE(branch.value("condition").toObject().value("op").toString(),QString("always"));
  auto context=scriptTestContext();auto docs=context.value("documents").toArray();auto doc=docs[0].toObject();doc["data"]=data;docs[0]=doc;context["documents"]=docs;ScriptAuthoringDialog reopened(context,{});QVERIFY(reopened.edits().isEmpty());QVERIFY(!reopened.findChild<QGraphicsView*>("scriptGraph")->scene()->items().isEmpty());
}
void StudioTests::scriptAuthoringPreservesAndRejectsBrokenLinks(){
  auto context=scriptTestContext();auto docs=context.value("documents").toArray();auto doc=docs[0].toObject(),story=doc.value("data").toObject();story["futureMetadata"]=QJsonObject{{"keep",7}};auto nodes=story.value("nodes").toArray();auto line=nodes[0].toObject();line["futureField"]=QJsonArray{1,2,3};nodes[0]=line;story["nodes"]=nodes;doc["data"]=story;docs[0]=doc;context["documents"]=docs;
  ScriptAuthoringDialog dialog(context,{});dialog.show();QVERIFY(dialog.edits().isEmpty());
  dialog.findChild<QPushButton*>("scriptNodeRemove")->click();QVERIFY(dialog.findChild<QLabel*>("scriptError")->text().contains("시작 노드"));QVERIFY(dialog.edits().isEmpty());
  auto next=dialog.findChild<QComboBox*>("script-next");next->setCurrentIndex(next->findData("line"));dialog.findChild<QDialogButtonBox*>("scriptButtons")->button(QDialogButtonBox::Save)->click();QCOMPARE(dialog.result(),0);QVERIFY(dialog.findChild<QLabel*>("scriptError")->text().contains("순환"));
  next->setCurrentIndex(next->findData("end"));dialog.findChild<QPlainTextEdit*>("script-text")->setPlainText("Edited without deleting unknown fields");dialog.findChild<QLineEdit*>("scriptNodeId")->setText("renamed");dialog.findChild<QPushButton*>("scriptNodeRename")->click();
  dialog.findChild<QDialogButtonBox*>("scriptButtons")->button(QDialogButtonBox::Save)->click();QCOMPARE(dialog.result(),int(QDialog::Accepted));const auto updated=dialog.edits().first().toObject().value("data").toObject();QCOMPARE(updated.value("futureMetadata"),story.value("futureMetadata"));QCOMPARE(updated.value("entryNodeId").toString(),QString("renamed"));QCOMPARE(updated.value("nodes").toArray()[0].toObject().value("futureField"),line.value("futureField"));
  auto broken=story;nodes[0]=QJsonObject{{"id","line"},{"type","dialogue"},{"speakerId",QJsonValue::Null},{"text","Missing"},{"next","missing"}};broken["nodes"]=nodes;QVERIFY(scriptGraphIssues(broken).join('\n').contains("없는 연결"));
  bool stale=true;ScriptAuthoringDialog guarded(scriptTestContext(),{},nullptr,[&]{return stale?QString("프로젝트가 변경되었습니다"):QString{};});guarded.show();guarded.findChild<QPlainTextEdit*>("script-text")->setPlainText("Retain this draft");guarded.findChild<QDialogButtonBox*>("scriptButtons")->button(QDialogButtonBox::Save)->click();QCOMPARE(guarded.result(),0);QVERIFY(guarded.isVisible());QCOMPARE(guarded.edits().size(),1);QCOMPARE(guarded.findChild<QPlainTextEdit*>("script-text")->toPlainText(),QString("Retain this draft"));QVERIFY(guarded.findChild<QLabel*>("scriptError")->text().contains("변경"));stale=false;guarded.findChild<QDialogButtonBox*>("scriptButtons")->button(QDialogButtonBox::Save)->click();QCOMPARE(guarded.result(),int(QDialog::Accepted));
  auto manyContext=scriptTestContext();QJsonArray manyDocs;const auto base=manyContext.value("documents").toArray()[0].toObject();for(int i=0;i<101;++i){auto item=base;item["file"]=QString("events/event-%1.json").arg(i);auto d=item.value("data").toObject();d["id"]=QString("event-%1").arg(i);item["data"]=d;manyDocs.append(item);}manyContext["documents"]=manyDocs;ScriptAuthoringDialog limited(manyContext,{});limited.show();auto picker=limited.findChild<QComboBox*>("scriptDocument");for(int i=0;i<101;++i){picker->setCurrentIndex(i);limited.findChild<QPlainTextEdit*>("script-text")->setPlainText("Changed");}limited.findChild<QDialogButtonBox*>("scriptButtons")->button(QDialogButtonBox::Save)->click();QCOMPARE(limited.result(),0);QVERIFY(limited.isVisible());QCOMPARE(limited.edits().size(),101);QVERIFY(limited.findChild<QLabel*>("scriptError")->text().contains("100"));limited.findChild<QPlainTextEdit*>("script-text")->setPlainText("Original line");limited.findChild<QDialogButtonBox*>("scriptButtons")->button(QDialogButtonBox::Save)->click();QCOMPARE(limited.edits().size(),100);QCOMPARE(limited.result(),int(QDialog::Accepted));
}
