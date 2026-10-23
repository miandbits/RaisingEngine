#include "studio.hpp"
#include "renderer-preview.hpp"
#include <QtTest>
#include <QtWidgets>

class StatePreviewTest : public QObject {
  Q_OBJECT
private slots:
  void savedStateRoundtrip() {
    const auto configPath=qEnvironmentVariable("RAISING_STATE_PREVIEW_PROOF");
    if(configPath.isEmpty())QSKIP("Requires a disposable project and replay-validated save proof configuration.");
    QFile configFile(configPath);QVERIFY(configFile.open(QIODevice::ReadOnly));
    const auto config=QJsonDocument::fromJson(configFile.readAll()).object();
    const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
    const auto root=config["root"].toString(),output=config["output"].toString();QDir().mkpath(output);
    QFile saved(config["save"].toString());QVERIFY(saved.open(QIODevice::ReadOnly));const auto savedBytes=saved.readAll();saved.close();
    const auto savedState=QJsonDocument::fromJson(savedBytes).object().value("state");
    auto window=std::make_unique<StudioWindow>(engine);window->resize(1280,900);window->show();window->openProject(root);
    QTRY_VERIFY_WITH_TIMEOUT(!window->busy()&&!window->projectData().isEmpty(),30000);
    window->findChild<QAction*>("showAuthoringWorkspace")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!window->busy(),20000);
    auto workspace=window->findChild<QDockWidget*>("authoringWorkspaceDock");QVERIFY(workspace);
    auto tree=workspace->findChild<QTreeWidget*>("workspaceEntities");QVERIFY(tree);
    const auto key="screenElement:"+config["screen"].toString()+"/"+config["element"].toString();
    QTreeWidgetItem* selected=nullptr;for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole).toJsonObject()["key"]==key){selected=*it;break;}
    QVERIFY2(selected,qPrintable(key));tree->setCurrentItem(selected);workspace->findChild<QPushButton*>("workspaceCanvas")->click();QTRY_VERIFY_WITH_TIMEOUT(!window->busy(),10000);
    auto dock=window->findChild<RendererPreviewDock*>("rendererPreviewDock");QVERIFY(dock);dock->setFloating(true);dock->resize(1180,900);dock->show();dock->raise();
    auto targets=dock->findChild<QComboBox*>("rendererElement");QVERIFY(targets);targets->setCurrentIndex(0);
    const auto before=window->currentView();
    auto configure=[&](QString actorId,bool changed){
      QTimer::singleShot(30,dock,[&,actorId,changed]{
        auto dialog=dock->findChild<QDialog*>("authoringPreviewDialog");QVERIFY(dialog);
        QVERIFY(dialog->findChild<QCheckBox*>("previewScenarioEnabled"));dialog->findChild<QCheckBox*>("previewScenarioEnabled")->setChecked(true);
        dialog->findChild<QComboBox*>("previewStateSource")->setCurrentIndex(1);
        dialog->findChild<QLineEdit*>("previewSavePath")->setText(config["save"].toString());
        dialog->findChild<QCheckBox*>("previewFollowRouting")->setChecked(true);
        dialog->findChild<QComboBox*>("previewFlagScope")->setCurrentIndex(1);
        auto actor=dialog->findChild<QComboBox*>("previewActor");const auto index=actor->findData(actorId);QVERIFY(index>=0);actor->setCurrentIndex(index);
        auto event=dialog->findChild<QComboBox*>("previewEvent");QVERIFY(event->findData(config["event"].toString())>=0);event->setCurrentIndex(event->findData(config["event"].toString()));
        const auto attribute=config["attribute"].toString();QVERIFY(dialog->findChild<QCheckBox*>("previewUseAttribute-"+attribute));dialog->findChild<QCheckBox*>("previewUseAttribute-"+attribute)->setChecked(changed);
        if(changed)dialog->findChild<QSpinBox*>("previewAttribute-"+attribute)->setValue(config["override"].toInt());
        dialog->grab().save(output+"/state-form-"+actorId+".png");
        dialog->findChild<QDialogButtonBox*>("previewScenarioButtons")->button(QDialogButtonBox::Apply)->click();
      });dock->findChild<QPushButton*>("configureAuthoringPreview")->click();
    };
    auto render=[&]{QSignalSpy ready(dock,&RendererPreviewDock::snapshotReady);dock->findChild<QPushButton*>("renderPreview")->click();QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(),90000);QVERIFY2(ready.last().first().toBool(),qPrintable(dock->findChild<QLabel*>("rendererStatus")->text()));};
    for(auto value:config["actors"].toArray()){
      const auto actor=value.toObject();configure(actor["id"].toString(),false);render();QVERIFY(!QTest::currentTestFailed());
      QFile report(dock->reportPath());QVERIFY(report.open(QIODevice::ReadOnly));const auto reportObject=QJsonDocument::fromJson(report.readAll()).object();
      QCOMPARE(dock->result()["state"],savedState);
      auto reasons=dock->findChild<QTreeWidget*>("previewReasons");QVERIFY(reasons);bool found=false;for(QTreeWidgetItemIterator it(reasons);*it;++it)if((*it)->text(0)==actor["rowLabel"].toString()){QCOMPARE((*it)->text(1),actor["value"].toString()+" / "+actor["value"].toString());found=true;}QVERIFY(found);
      const auto expectedTexts=config["texts"].toObject(),actualTexts=dock->result()["texts"].toObject();
      for(auto field:expectedTexts.keys())QCOMPARE(actualTexts[field],expectedTexts[field]);
      if(actor.contains("requirements")){
        const auto rows=reasons->findItems(QStringLiteral("모든 조건"),Qt::MatchExactly|Qt::MatchRecursive,0);QVERIFY(!rows.isEmpty());
        QCOMPARE(rows.first()->text(1),actor["requirements"].toBool()?QStringLiteral("충족"):QStringLiteral("미충족"));
        reasons->scrollToItem(rows.first(),QAbstractItemView::PositionAtTop);
      }
      dock->grab().save(output+"/state-render-"+actor["id"].toString()+".png");
      QFile proof(output+"/qt-render-"+actor["id"].toString()+".json");QVERIFY(proof.open(QIODevice::WriteOnly));proof.write(QJsonDocument(reportObject).toJson());
    }
    const auto actor=config["actors"].toArray().first().toObject()["id"].toString();configure(actor,true);render();
    QCOMPARE(dock->result()["state"].toObject()["roster"].toObject()["attributes"].toObject()[actor].toObject()[config["attribute"].toString()].toInt(),config["override"].toInt());
    for(auto field:{"documents","revision","dirty","undo","redo"})QCOMPARE(window->currentView()[field],before[field]);
    QVERIFY(saved.open(QIODevice::ReadOnly));QCOMPARE(saved.readAll(),savedBytes);saved.close();
    dock->close();auto name=window->findChild<QLineEdit*>("elementName");QVERIFY(name);name->setFocus();name->selectAll();QTest::keyClicks(name,"State preview roundtrip");QTest::keyClick(name,Qt::Key_Return);QTRY_VERIFY_WITH_TIMEOUT(!window->busy()&&window->currentView()["dirty"].toBool(),10000);
    window->findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!window->busy()&&!window->currentView()["dirty"].toBool(),30000);
    const auto authored=window->projectData();QVERIFY(window->close());window.reset();
    StudioWindow reopened(engine);reopened.show();reopened.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!reopened.busy()&&!reopened.projectData().isEmpty(),30000);
    QCOMPARE(reopened.projectData(),authored);QVERIFY(!reopened.currentView()["dirty"].toBool());reopened.grab().save(output+"/project-reopened.png");
  }
};
QTEST_MAIN(StatePreviewTest)
#include "state-preview-test.moc"
