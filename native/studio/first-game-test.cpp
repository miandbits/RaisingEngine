#include "studio.hpp"
#include "project-manager.hpp"
#include "editor-language.hpp"
#include "theme.hpp"
#include <QtTest>
#include <QtWidgets>

class FirstGameTest : public QObject {
  Q_OBJECT
private slots:
  void createCustomizePlayAndExport() {
    const auto out=QDir::fromNativeSeparators(qEnvironmentVariable("RAISING_FIRST_GAME_PROOF"));
    if(out.isEmpty())QSKIP("Run scripts/test-first-game-ui.mjs with a built editor and player");
    installEditorLanguage(*qApp,"en");applyStudioTheme(*qApp);
    const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
    const auto root=out+"/designer-game";
    QTimer watchdog;connect(&watchdog,&QTimer::timeout,this,[]{if(auto m=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){qWarning()<<m->text();m->reject();}});watchdog.start(3000);
    ProjectManager launcher(engine);launcher.show();
    QCOMPARE(launcher.findChild<QComboBox*>("launcherTemplate")->currentData().toString(),"my-first-raising-game");
    launcher.findChild<QLineEdit*>("launcherDestination")->setText(root);
    QTest::qWait(100);launcher.findChild<QPushButton*>("launcherCreate")->click();
    QTRY_COMPARE_WITH_TIMEOUT(launcher.result(),int(QDialog::Accepted),30000);
    QCOMPARE(launcher.selectedProject(),root);
    StudioWindow window(engine);window.resize(1600,1000);window.show();window.openProject(root);
    QTRY_VERIFY_WITH_TIMEOUT(!window.busy()&&!window.projectData().isEmpty(),30000);
    QTRY_VERIFY(window.findChild<QDockWidget*>("firstGameChecklist"));
    auto step=[&](int n){return window.findChild<QPushButton*>("firstGameStep"+QString::number(n));};
    for(int n=0;n<8;++n){QVERIFY(step(n));QVERIFY(!step(n)->property("complete").toBool());}
    QTimer::singleShot(50,&window,[&]{
      auto dialog=window.findChild<QDialog*>("projectSettingsDialog");QVERIFY(dialog);
      dialog->findChild<QLineEdit*>("projectTitle")->setText("Willow Days");
      dialog->findChild<QDialogButtonBox*>("projectSettingsButtons")->button(QDialogButtonBox::Ok)->click();
    });step(1)->click();
    QTRY_VERIFY_WITH_TIMEOUT(!window.busy()&&step(1)->property("complete").toBool(),10000);
    QCOMPARE(window.projectData()["title"].toString(),"Willow Days");
    bool activityEdited=false;
    QTimer::singleShot(50,&window,[&]{
      auto dialog=window.findChild<QDialog*>("contentEditor");QVERIFY(dialog);
      dialog->findChild<QLineEdit*>("contentLabel")->setText("My garden activity");
      dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();activityEdited=true;
    });step(2)->click();QVERIFY(activityEdited);
    QTRY_VERIFY_WITH_TIMEOUT(!window.busy()&&step(2)->property("complete").toBool(),10000);
    window.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!window.busy()&&!step(2)->property("complete").toBool(),10000);
    window.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!window.busy()&&step(2)->property("complete").toBool(),10000);
    // Restore accepted edits into a separate project through the actual recovery form.
    window.findChild<QAction*>("showDraftRecovery")->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(!window.busy()&&window.findChild<QComboBox*>("recoveryChoice"),10000);
    auto recovery=window.findChild<QComboBox*>("recoveryChoice")->parentWidget();
    window.findChild<QLineEdit*>("recoveryDestination")->setText(out+"/recovered-game");
    window.findChild<QPushButton*>("recoveryRestore")->click();
    QTRY_VERIFY_WITH_TIMEOUT(!window.busy()&&window.findChild<QPushButton*>("recoveryOpen")->isEnabled(),10000);
    QVERIFY(QFileInfo::exists(out+"/recovered-game/project.json"));
    if(auto d=qobject_cast<QDialog*>(recovery))d->reject();else window.findChild<QPushButton*>("recoveryOpen")->window()->close();
    window.findChild<QAction*>("showAuthoringWorkspace")->trigger();
    auto workspace=window.findChild<QDockWidget*>("authoringWorkspaceDock");QVERIFY(workspace);
    QTRY_VERIFY_WITH_TIMEOUT(!window.busy()&&workspace->findChild<QComboBox*>("workspaceAddKind"),10000);
    auto kinds=workspace->findChild<QComboBox*>("workspaceAddKind");kinds->setCurrentIndex(kinds->findData("activity"));
    workspace->findChild<QPushButton*>("workspaceAdd")->click();
    auto newName=workspace->findChild<QLineEdit*>("workspaceNewName");QVERIFY(newName);newName->setText("Pottery practice");
    qobject_cast<QDialog*>(newName->parentWidget())->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
    QTRY_VERIFY_WITH_TIMEOUT(!window.busy(),10000);QTest::qWait(100);
    auto tree=workspace->findChild<QTreeWidget*>("workspaceEntities");QVERIFY(tree);
    bool selected=false;QTreeWidgetItemIterator item(tree);while(*item){if((*item)->text(0).contains("Pottery practice")){tree->setCurrentItem(*item);selected=true;break;}++item;}QVERIFY(selected);
    workspace->findChild<QPushButton*>("workspaceEncounter")->click();
    newName=workspace->findChild<QLineEdit*>("workspaceNewName");QVERIFY(newName);newName->setText("A bowl for a friend");
    workspace->findChild<QPlainTextEdit*>("workspaceNewDialogue")->setPlainText("Mira admires your bowl.");
    workspace->findChild<QLineEdit*>("workspaceNewPrompt")->setText("Give it to Mira?");
    workspace->findChild<QLineEdit*>("workspaceNewChoice1")->setText("Share the gift");
    workspace->findChild<QLineEdit*>("workspaceNewChoice2")->setText("Keep practicing");
    qobject_cast<QDialog*>(newName->parentWidget())->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
    QTRY_VERIFY_WITH_TIMEOUT(!window.busy()&&step(3)->property("complete").toBool(),10000);
    bool found=false;for(auto v:window.currentView()["documents"].toArray()){const auto d=v.toObject()["data"].toObject();if(d["label"]=="A bowl for a friend"){QVERIFY(!d["activityId"].toString().isEmpty());for(auto n:d["nodes"].toArray())if(n.toObject()["type"]=="choice")found=n.toObject()["choices"].toArray().size()==2;}}QVERIFY(found);
    window.findChild<QPushButton*>("firstGameWhy")->click();QTRY_VERIFY_WITH_TIMEOUT(!window.busy(),10000);
    auto debug=window.findChild<QDockWidget*>("ruleDebugDock");QVERIFY(debug);
    auto subjects=debug->findChild<QComboBox*>("debugSubjectKind");subjects->setCurrentIndex(subjects->findData("ending"));
    auto endings=debug->findChild<QComboBox*>("debugSubject");endings->setCurrentIndex(endings->findData("together"));
    debug->findChild<QPushButton*>("debugInspect")->click();QTRY_VERIFY_WITH_TIMEOUT(!window.busy(),10000);
    auto conditions=debug->findChild<QTreeWidget*>("debugConditions");QVERIFY(conditions->topLevelItemCount()>1);
    bool namedValue=false;QTreeWidgetItemIterator condition(conditions);while(*condition){if((*condition)->text(1).contains("Skill is 0; requires at least 30"))namedValue=true;++condition;}QVERIFY(namedValue);
    window.playAutomation=out+"/route.json";window.playReport=out+"/play-report.json";
    QSignalSpy completed(&window,&StudioWindow::playerFinished);step(0)->click();
    QTRY_VERIFY_WITH_TIMEOUT(!completed.isEmpty(),90000);QCOMPARE(completed.first().first().toInt(),0);
    QTRY_VERIFY_WITH_TIMEOUT(!window.busy()&&step(6)->property("complete").toBool(),10000);
    QVERIFY(step(0)->property("complete").toBool());
    QFile report(window.playReport);QVERIFY(report.open(QIODevice::ReadOnly));
    const auto proof=QJsonDocument::fromJson(report.readAll()).object();QVERIFY(proof["ok"].toBool());
    bool finished=false;for(auto item:proof["steps"].toArray()){auto s=item.toObject();if(s["command"].toObject()["checkpoint"]=="ending")finished=s["ui"].toObject()["state"].toObject()["endingId"]=="together";}QVERIFY(finished);
    QTimer exportPoll;connect(&exportPoll,&QTimer::timeout,&window,[&]{
      auto dialog=window.findChild<QDialog*>("exportGameDialog");if(!dialog)return;
      auto open=dialog->findChild<QPushButton*>("exportOpenFolder");
      if(open&&open->isEnabled()){exportPoll.stop();dialog->accept();}
    });exportPoll.start(100);
    QTimer::singleShot(50,&window,[&]{
      auto dialog=window.findChild<QDialog*>("exportGameDialog");QVERIFY(dialog);
      auto target=dialog->findChild<QComboBox*>("exportTarget");
#ifdef Q_OS_MACOS
      QCOMPARE(target->currentData().toString(),"macos");
      QVERIFY(target->findData("ios")>=0);
#endif
      const auto desktopIndex=target->currentIndex();
      target->setCurrentIndex(target->findData("web"));
      QVERIFY(dialog->findChild<QLabel*>("exportCompatibility")->text().startsWith("Web:"));
      target->setCurrentIndex(desktopIndex);
      QVERIFY(!window.busy()); // A rapid platform change must not swallow Export.
      dialog->findChild<QLineEdit*>("exportParent")->setText(out);
      dialog->findChild<QLineEdit*>("exportFolderName")->setText("shipped-game");
      // Invalid version leaves the same form and destination available for correction.
      auto version=dialog->findChild<QLineEdit*>("exportVersion");version->setText("invalid");
      dialog->findChild<QPushButton*>("exportStart")->click();
      QTRY_VERIFY_WITH_TIMEOUT(!window.busy()&&dialog->findChild<QPushButton*>("exportStart")->isEnabled(),20000);
      QVERIFY(dialog->findChild<QLabel*>("exportResult")->text().contains("release version"));
      QCOMPARE(dialog->findChild<QLineEdit*>("exportFolderName")->text(),"shipped-game");
      version->setText("0.2.0");dialog->findChild<QPushButton*>("exportStart")->click();
    });
    QTimer::singleShot(45000,&window,[&]{if(auto d=window.findChild<QDialog*>("exportGameDialog")){qWarning()<<d->findChild<QLabel*>("exportResult")->text();d->reject();}});
    step(7)->click();
    QTRY_VERIFY_WITH_TIMEOUT(!window.busy()&&step(7)->property("complete").toBool(),10000);
    QVERIFY(QFileInfo::exists(out+"/shipped-game/Game.zip"));
    QVERIFY(QFileInfo::exists(out+"/shipped-game/Project-backup.zip"));
    QVERIFY(window.grab().save(out+"/checklist.png"));
    window.findChild<QAction*>("saveProject")->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(!window.busy()&&!window.currentView()["dirty"].toBool(),10000);
    QVERIFY(window.close());
    StudioWindow reopened(engine);reopened.show();reopened.openProject(root);
    QTRY_VERIFY_WITH_TIMEOUT(!reopened.busy()&&reopened.findChild<QPushButton*>("firstGameStep7"),30000);
    QTRY_VERIFY(reopened.findChild<QPushButton*>("firstGameStep7")->property("complete").toBool());
    QVERIFY(reopened.findChild<QPushButton*>("firstGameStep2")->property("complete").toBool());
    QVERIFY(reopened.close());
  }
};
QTEST_MAIN(FirstGameTest)
#include "first-game-test.moc"
