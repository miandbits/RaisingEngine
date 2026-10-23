#include "studio.hpp"
#include "canvas-state.hpp"
#include "canvas-player.hpp"
#include "renderer-preview.hpp"
#include <QtTest>
#include <QtWidgets>

class CanvasLiveTest : public QObject {
  Q_OBJECT
private slots:
  void isolatedPlaybackRoundtrip(){
    QTimer watchdog;connect(&watchdog,&QTimer::timeout,this,[]{if(auto m=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){qWarning()<<"Unexpected modal:"<<m->text();m->reject();}});watchdog.start(2000);
    QFile configFile(qEnvironmentVariable("RAISING_CANVAS_LIVE_PROOF"));
    if(!configFile.exists())QSKIP("Requires a disposable Windows project with the playback proof buttons");
    QVERIFY(configFile.open(QIODevice::ReadOnly));const auto cfg=QJsonDocument::fromJson(configFile.readAll()).object();
    const auto root=cfg["root"].toString(),out=cfg["output"].toString(),engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());QDir().mkpath(out);
    QFile save(cfg["save"].toString());QVERIFY(save.open(QIODevice::ReadOnly));const auto originalSave=save.readAll();save.close();
    auto window=std::make_unique<StudioWindow>(engine);window->resize(1400,950);window->show();window->openProject(root);
    QTRY_VERIFY_WITH_TIMEOUT(!window->busy()&&!window->projectData().isEmpty(),30000);
    qInfo()<<"Project open";
    auto openScreen=[&](StudioWindow* w){
      w->findChild<QAction*>("showAuthoringWorkspace")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w->busy(),20000);
      auto workspace=w->findChild<QDockWidget*>("authoringWorkspaceDock");auto tree=workspace->findChild<QTreeWidget*>("workspaceEntities");QVERIFY(tree);
      QTreeWidgetItem* selected=nullptr;for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole).toJsonObject()["key"]=="screenElement:home/heading"){selected=*it;break;}
      QVERIFY(selected);tree->setCurrentItem(selected);workspace->findChild<QPushButton*>("workspaceCanvas")->click();QTRY_VERIFY_WITH_TIMEOUT(!w->busy(),10000);workspace->hide();
    };openScreen(window.get());QVERIFY(!QTest::currentTestFailed());qInfo()<<"Home canvas open";
    auto bar=window->findChild<CanvasStateBar*>("canvasStateBar");auto player=window->findChild<CanvasPlayer*>("canvasLivePlayer");auto preview=bar->renderer();
    QTimer progress;connect(&progress,&QTimer::timeout,window.get(),[&]{qInfo()<<"Progress"<<window->busy()<<preview->running()<<bar->findChild<QLabel*>("canvasStateStatus")->text();});progress.start(10000);
    QVERIFY(player);QSignalSpy failures(player,&CanvasPlayer::failed);
    QTimer::singleShot(30,preview,[&]{
      auto d=preview->findChild<QDialog*>("authoringPreviewDialog");QVERIFY(d);
      d->findChild<QComboBox*>("previewStateSource")->setCurrentIndex(1);d->findChild<QLineEdit*>("previewSavePath")->setText(cfg["save"].toString());
      auto actors=d->findChild<QComboBox*>("previewActor");actors->setCurrentIndex(actors->findData("hero"));
      d->findChild<QDialogButtonBox*>("previewScenarioButtons")->button(QDialogButtonBox::Apply)->click();
    });bar->findChild<QPushButton*>("canvasStateConfigure")->click();qInfo()<<"Scenario applied";
    auto canvas=window->findChild<ScreenCanvas*>("screenCanvas");
    QTRY_VERIFY_WITH_TIMEOUT(!window->busy()&&!preview->running()&&canvas->hasRuntimePreview(),90000);
    qInfo()<<"Snapshot ready";
    const auto before=window->currentView();const QJsonValue baseline=canvas->runtimeResult()["state"];
    auto start=[&]{bar->findChild<QPushButton*>("canvasPlay")->click();QTRY_VERIFY_WITH_TIMEOUT(player->isVisible()&&!player->lastReport().isEmpty(),90000);};
    start();QVERIFY(!QTest::currentTestFailed());QVERIFY(failures.isEmpty());qInfo()<<"Embedded live player ready";
    auto command=[&](QJsonObject value){qInfo()<<"Send"<<value;const auto serial=player->command(value);QElapsedTimer time;time.start();while(player->lastReport()["serial"].toInteger()<serial&&time.elapsed()<10000)QTest::qWait(20);qInfo()<<"Received"<<serial<<player->lastReport()["serial"];QFile latest(out+"/latest.json");if(latest.open(QIODevice::WriteOnly))latest.write(QJsonDocument(player->lastReport()).toJson());return player->lastReport();};
    auto inspect=[&]{return command({{"type","inspect"}});};
    QCOMPARE(inspect()["ui"].toObject()["state"],baseline);qInfo()<<"Initial state matches saved state";
    const auto event=command({{"type","click"},{"id","proof-event"}});const QJsonValue eventState=event["ui"].toObject()["state"];QVERIFY(eventState!=baseline);
    QVERIFY(event["ui"].toObject()["audio"].toObject()["loadedSounds"].toInt()>=2);qInfo()<<"Event and audio active";
    command({{"type","click"},{"id","proof-motion"}});QTest::qWait(300);auto moving=inspect();
    QVERIFY(moving["ui"].toObject()["direction"].toObject()["elapsedMs"].toDouble()>0);
    bar->findChild<QPushButton*>("canvasPause")->click();QTest::qWait(150);const auto paused=inspect();QTest::qWait(300);const auto still=inspect();
    QCOMPARE(still["elapsedMs"],paused["elapsedMs"]);QCOMPARE(still["ui"].toObject()["direction"],paused["ui"].toObject()["direction"]);
    QCOMPARE(still["ui"].toObject()["audio"].toObject()["musicCursor"],paused["ui"].toObject()["audio"].toObject()["musicCursor"]);
    qInfo()<<"Clocks and audio paused";
    bar->findChild<QPushButton*>("canvasPause")->click();QTest::qWait(300);const auto resumed=inspect();QVERIFY(resumed["elapsedMs"].toDouble()>still["elapsedMs"].toDouble());
    QVERIFY(resumed["ui"].toObject()["audio"].toObject()["musicCursor"]!=still["ui"].toObject()["audio"].toObject()["musicCursor"]);
    command({{"type","click"},{"id","directionSave"}});QFile temporary(player->sessionDirectory()+"/save.json");QVERIFY(temporary.open(QIODevice::ReadOnly));const auto envelope=QJsonDocument::fromJson(temporary.readAll()).object();QVERIFY(envelope.contains("inspectionSession"));
    command({{"type","click"},{"id","directionStop"}});const auto loaded=command({{"type","click"},{"id","load"}});
    QCOMPARE(loaded["ui"].toObject()["state"],eventState);QVERIFY(!loaded["ui"].toObject()["direction"].isNull());
    const auto oldSession=player->sessionDirectory();bar->findChild<QPushButton*>("canvasReset")->click();QTRY_VERIFY_WITH_TIMEOUT(player->isVisible()&&!player->lastReport().isEmpty()&&player->sessionDirectory()!=oldSession,30000);
    QCOMPARE(inspect()["ui"].toObject()["state"],baseline);QVERIFY(!QFile::exists(player->sessionDirectory()+"/save.json"));
    for(auto key:{"documents","dirty","revision","undo","redo"})QCOMPARE(window->currentView()[key],before[key]);
    bar->findChild<QPushButton*>("canvasStop")->click();QTRY_VERIFY(!player->active());
    canvas->select("heading");canvas->setFocus();QTest::keyClick(canvas,Qt::Key_Right);QTRY_VERIFY_WITH_TIMEOUT(!window->busy()&&window->currentView()["dirty"].toBool(),10000);
    window->findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!window->busy()&&!window->currentView()["dirty"].toBool(),30000);const auto authored=window->projectData();
    QVERIFY(window->close());window.reset();
    StudioWindow reopened(engine);reopened.resize(1400,950);reopened.show();reopened.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!reopened.busy()&&!reopened.projectData().isEmpty(),30000);openScreen(&reopened);QVERIFY(!QTest::currentTestFailed());QCOMPARE(reopened.projectData(),authored);
    auto restoredCanvas=reopened.findChild<ScreenCanvas*>("screenCanvas");QTRY_VERIFY_WITH_TIMEOUT(!reopened.busy()&&restoredCanvas->hasRuntimePreview(),90000);
    const QJsonValue reopenedState=restoredCanvas->runtimeResult()["state"];
    reopened.findChild<QPushButton*>("canvasPlay")->click();auto restored=reopened.findChild<CanvasPlayer*>("canvasLivePlayer");QTRY_VERIFY_WITH_TIMEOUT(restored->isVisible()&&!restored->lastReport().isEmpty(),90000);
    QCOMPARE(restored->lastReport()["ui"].toObject()["state"],reopenedState);qInfo()<<"Saved project reopened and live state restored";
    QVERIFY(save.open(QIODevice::ReadOnly));QCOMPARE(save.readAll(),originalSave);
    QFile result(out+"/live-result.json");QVERIFY(result.open(QIODevice::WriteOnly));result.write(QJsonDocument(QJsonObject{{"paused",paused},{"resumed",resumed},{"event",event},{"reopened",restored->lastReport()},{"originalSavePreserved",true}}).toJson());
    reopened.findChild<QPushButton*>("canvasStop")->click();QVERIFY(failures.isEmpty());
  }
};
QTEST_MAIN(CanvasLiveTest)
#include "canvas-live-test.moc"
