#include "studio.hpp"
#include <QtWidgets>
#include <QTest>

// Exercises the actual editor widgets and shared service. The external plan is
// test input, never an edit operation or a replacement project document.
class StoryMonthJourney:public QObject {
  Q_OBJECT
private slots:
  void authorMonth() {
    const auto planFile=qEnvironmentVariable("RAISING_STORY_MONTH_PLAN");
    if(planFile.isEmpty())QSKIP("Set RAISING_STORY_MONTH_PLAN to a disposable authoring trial");
    QFile input(planFile);QVERIFY(input.open(QIODevice::ReadOnly));
    const auto plan=QJsonDocument::fromJson(input.readAll()).object();
    const auto root=plan["root"].toString(),evidence=plan["evidence"].toString();
    QVERIFY(!root.isEmpty()&&!evidence.isEmpty());QDir().mkpath(evidence);
    StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()));
    w.resize(1440,1000);w.show();w.openProject(root);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().empty(),30000);
    QJsonArray created;
    for(auto value:plan["stories"].toArray()) {
      const auto story=value.toObject();const QJsonValue before=w.currentView()["documents"];
      w.findChild<QAction*>("storyFlowAction")->trigger();
      QPointer<QDialog> dialog=w.findChild<QDialog*>("storyFlowDialog");QVERIFY(dialog);
      auto line=[&](const char *name,QJsonValue value){auto field=dialog->findChild<QLineEdit*>(name);QVERIFY(field);field->setText(value.toString());};
      auto prose=[&](const char *name,QJsonValue value){auto field=dialog->findChild<QPlainTextEdit*>(name);QVERIFY(field);field->setPlainText(value.toString());};
      auto select=[&](const char *name,QJsonValue value){auto field=dialog->findChild<QComboBox*>(name);QVERIFY(field);const int index=field->findData(value.toVariant());QVERIFY2(index>=0,name);field->setCurrentIndex(index);};
      auto steps=dialog->findChild<QListWidget*>("storyFlowSteps");QVERIFY(steps);
      line("storyName",story["name"]);select("storyCharacter",story["characterId"]);select("storyRelationship",story["relationshipId"]);
      steps->setCurrentRow(1);select("storyActivity",story["activityId"]);
      steps->setCurrentRow(2);line("storyEncounterTitle",story["title"]);prose("storyEncounterText",story["text"]);line("storyPrompt",story["prompt"]);
      if(!story["predecessorName"].toString().isEmpty()) {
        auto previous=dialog->findChild<QComboBox*>("storyPredecessor");QVERIFY(previous);const int index=previous->findText(story["predecessorName"].toString());QVERIFY(index>0);previous->setCurrentIndex(index);
      }
      select("storyFrom",story["from"]);select("storyThrough",story["through"]);select("storyMeetingTrigger","after-activity");
      QVERIFY(dialog->grab().save(evidence+QString("/story-%1-calendar.png").arg(created.size())));
      steps->setCurrentRow(3);auto choices=dialog->findChild<QListWidget*>("storyChoices");QVERIFY(choices);QCOMPARE(story["choices"].toArray().size(),2);
      for(int i=0;i<2;++i) {
        const auto c=story["choices"].toArray()[i].toObject();choices->setCurrentRow(i);
        line("storyChoiceText",c["label"]);prose("storyChoiceReply",c["reply"]);
        dialog->findChild<QSpinBox*>("storyChoiceDelta")->setValue(c["delta"].toInt());
        dialog->findChild<QCheckBox*>("storyFollowupEnabled")->setChecked(true);
        line("storyFollowupTitle",c["title"]);prose("storyFollowupText",c["text"]);
        dialog->findChild<QSpinBox*>("storyFollowupMinimum")->setValue(c["minimum"].toInt());
        select("storyFollowupFrom",c["from"]);select("storyFollowupThrough",c["through"]);select("storyFollowupTrigger","after-activity");
      }
      choices->setCurrentRow(0);QCOMPARE(dialog->findChild<QComboBox*>("storyFollowupThrough")->currentData().toInt(),story["choices"].toArray()[0].toObject()["through"].toInt());
      steps->setCurrentRow(4);QVERIFY2(dialog->findChild<QLabel*>("storyFlowError")->text().isEmpty(),qPrintable(dialog->findChild<QLabel*>("storyFlowError")->text()));
      QVERIFY(dialog->grab().save(evidence+QString("/story-%1-preview.png").arg(created.size())));
      dialog->findChild<QPushButton*>("storyApply")->click();
      QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),30000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));QTRY_VERIFY(!dialog);
      const QJsonValue after=w.currentView()["documents"];
      w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),30000);QCOMPARE(w.currentView()["documents"],before);
      w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),30000);QCOMPARE(w.currentView()["documents"],after);
      created.append(w.projectData()["authoring"].toObject()["storyFlows"].toArray().last());
    }
    w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.currentView()["dirty"].toBool(),30000);
    const auto contents=[](QJsonValue documents){QJsonObject result;for(auto document:documents.toArray()){const auto doc=document.toObject();result.insert(doc["file"].toString(),doc["data"]);}return result;};
    const auto saved=contents(w.currentView()["documents"]);w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),30000);QCOMPARE(contents(w.currentView()["documents"]),saved);
    w.findChild<QAction*>("validateProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),30000);
    for(auto diagnostic:w.currentView()["diagnostics"].toArray())QVERIFY2(diagnostic.toObject()["severity"]!="error",qPrintable(diagnostic.toObject()["message"].toString()));
    QVERIFY(w.grab().save(evidence+"/saved-project.png"));
    QFile output(evidence+"/verification.json");QVERIFY(output.open(QIODevice::WriteOnly));
    output.write(QJsonDocument(QJsonObject{{"ok",true},{"method","Qt widget authoring automation"},{"stories",created},{"undoRedo",true},{"saveReopen",true},{"errors",0}}).toJson());
  }
};
QTEST_MAIN(StoryMonthJourney)
#include "story-month-journey.moc"
