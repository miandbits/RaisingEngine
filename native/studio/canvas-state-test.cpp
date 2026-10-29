#include "studio.hpp"
#include "canvas-state.hpp"
#include "renderer-preview.hpp"
#include <QtTest>
#include <QtWidgets>

class CanvasStateTest : public QObject {
  Q_OBJECT
private slots:
  void runtimeSelectionMatchesRenderedPixels_data(){
    QTest::addColumn<QSize>("pixels");QTest::addColumn<QRectF>("presentation");QTest::addColumn<QSize>("output");QTest::addColumn<QRectF>("expected");
    QTest::newRow("retina")<<QSize(1280,720)<<QRectF(0,0,1280,720)<<QSize(1280,720)<<QRectF(100,80,200,60);
    QTest::newRow("letterbox")<<QSize(1600,1000)<<QRectF(160,140,1280,720)<<QSize(1600,1000)<<QRectF(144,108,160,43.2);
    QTest::newRow("small-raster")<<QSize(320,180)<<QRectF(0,0,320,180)<<QSize(320,180)<<QRectF(100,80,200,60);
    QTest::newRow("sdl-cropped-letterbox")<<QSize(1280,720)<<QRectF(100,0,1280,720)<<QSize(1480,720)<<QRectF(100,80,200,60);
  }
  void runtimeSelectionMatchesRenderedPixels(){
    QFETCH(QSize,pixels);QFETCH(QRectF,presentation);QFETCH(QSize,output);QFETCH(QRectF,expected);
    ScreenCanvas canvas;canvas.resize(800,500);canvas.show();
    QJsonObject element{{"id","value"},{"kind","text"},{"text","design"},{"x",10},{"y",20},{"width",120},{"height",40}};
    QJsonObject screen{{"id","home"},{"elements",QJsonArray{element}}};
    canvas.setScreen(QJsonObject{{"visual",QJsonObject{{"width",640},{"height",360}}}},screen,{},"value",true);
    QImage image(pixels,QImage::Format_ARGB32);image.fill(Qt::black);
    QJsonObject node{{"id","value"},{"x",100},{"y",80},{"width",200},{"height",60},{"visible",true},{"rendered",true}};
    QJsonObject ui{{"screenId","home"},{"viewport",QJsonObject{{"width",640},{"height",360}}},
      {"presentation",QJsonObject{{"x",presentation.x()},{"y",presentation.y()},{"width",presentation.width()},{"height",presentation.height()},{"pixelWidth",output.width()},{"pixelHeight",output.height()}}},{"nodes",QJsonArray{node}}};
    canvas.setRuntimePreview(ui,image);
    auto scene=static_cast<QGraphicsView&>(canvas).scene();auto item=scene->selectedItems().value(0);QVERIFY(item);
    QCOMPARE(item->mapRectToScene(item->shape().boundingRect()),expected);
    canvas.select({});QTest::mouseClick(canvas.viewport(),Qt::LeftButton,Qt::NoModifier,canvas.mapFromScene(expected.center()));
    QCOMPARE(canvas.selectedIds(),QStringList{"value"});
    QSignalSpy edits(&canvas,&ScreenCanvas::geometryEdited);canvas.setFocus();QTest::keyClick(&canvas,Qt::Key_Right);
    QCOMPARE(edits.size(),1);QCOMPARE(edits.first().at(1).toRect(),QRect(11,20,120,40));
    // A 10-by-5 logical drag must retain the authored geometry even when the
    // rendered node has a different size or the screenshot is letterboxed.
    const QPointF scale(expected.width()/200,expected.height()/60);
    const auto from=canvas.mapFromScene(expected.center()),to=canvas.mapFromScene(expected.center()+QPointF(10*scale.x(),5*scale.y()));
    QTest::mousePress(canvas.viewport(),Qt::LeftButton,Qt::NoModifier,from);QTest::mouseMove(canvas.viewport(),to);QTest::mouseRelease(canvas.viewport(),Qt::LeftButton,Qt::NoModifier,to);
    QCOMPARE(edits.size(),2);const auto moved=edits.last().at(1).toRect();QVERIFY(qAbs(moved.x()-20)<=1&&qAbs(moved.y()-25)<=1);QCOMPARE(moved.size(),QSize(120,40));
    canvas.setRuntimePreview(ui,image);QSignalSpy moves(&canvas,&ScreenCanvas::selectionMoved);canvas.startSelectionDrag();canvas.previewSelectionDrag(QPoint(qRound(10*scale.x()),qRound(10*scale.y())));canvas.finishSelectionDrag();QCOMPARE(moves.size(),1);QCOMPARE(moves.first().first().toPoint(),QPoint(10,10));
    canvas.clearRuntimePreview();QCOMPARE(canvas.selectedIds(),QStringList{"value"});
    QCOMPARE(scene->selectedItems().first()->mapRectToScene(scene->selectedItems().first()->shape().boundingRect()),QRectF(10,20,120,40));
  }
  void runtimeGeometryKeepsAuthoredCoordinates(){
    ScreenCanvas canvas;canvas.resize(800,500);canvas.show();
    QJsonObject element{{"id","value"},{"kind","text"},{"text","design"},{"x",10},{"y",20},{"width",120},{"height",40}};
    QJsonObject screen{{"id","home"},{"elements",QJsonArray{element}}};QJsonObject project{{"visual",QJsonObject{{"width",640},{"height",360},{"screens",QJsonArray{screen}}}}};
    canvas.setScreen(project,screen,{},"value");QImage image(640,360,QImage::Format_ARGB32);image.fill(QColor("#335544"));
    QJsonObject ui{{"screenId","home"},{"texts",QJsonObject{{"value","42"}}},{"nodes",QJsonArray{QJsonObject{{"id","value"},{"x",100},{"y",80},{"width",200},{"height",60},{"visible",true},{"rendered",true}}}}};
    canvas.setRuntimePreview(ui,image);QVERIFY(canvas.hasRuntimePreview());QSignalSpy edits(&canvas,&ScreenCanvas::geometryEdited);
    canvas.setFocus();QTest::keyClick(&canvas,Qt::Key_Right);QCOMPARE(edits.size(),1);QCOMPARE(edits.first().at(1).toRect(),QRect(11,20,120,40));
    canvas.clearRuntimePreview();QVERIFY(!canvas.hasRuntimePreview());QCOMPARE(canvas.selectedIds(),QStringList{"value"});
    QTest::keyClick(&canvas,Qt::Key_Right);QCOMPARE(edits.last().at(1).toRect(),QRect(11,20,120,40));
  }
  void savedCanvasRoundtrip(){
    QTimer watchdog;connect(&watchdog,&QTimer::timeout,this,[]{auto modal=QApplication::activeModalWidget();qWarning()<<"Canvas proof watchdog"<<(modal?modal->windowTitle():QString("no modal"));});watchdog.start(15000);
    QFile configFile(qEnvironmentVariable("RAISING_CANVAS_STATE_PROOF"));if(!configFile.exists())QSKIP("Requires disposable project proof configuration");
    QVERIFY(configFile.open(QIODevice::ReadOnly));const auto config=QJsonDocument::fromJson(configFile.readAll()).object();
    const auto root=config["root"].toString(),out=config["output"].toString(),engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());QDir().mkpath(out);
    QFile save(config["save"].toString());QVERIFY(save.open(QIODevice::ReadOnly));const auto bytes=save.readAll();save.close();const auto savedState=QJsonDocument::fromJson(bytes).object().value("state");
    auto window=std::make_unique<StudioWindow>(engine);window->resize(1280,900);window->show();window->openProject(root);
    QTRY_VERIFY_WITH_TIMEOUT(!window->busy()&&!window->projectData().isEmpty(),30000);
    auto openScreen=[&](StudioWindow* w){
      w->findChild<QAction*>("showAuthoringWorkspace")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w->busy(),20000);
      auto workspace=w->findChild<QDockWidget*>("authoringWorkspaceDock");auto tree=workspace->findChild<QTreeWidget*>("workspaceEntities");QVERIFY(tree);
      QTreeWidgetItem* selected=nullptr;for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole).toJsonObject()["key"]=="screenElement:"+config["screen"].toString()+"/"+config["element"].toString()){selected=*it;break;}
      QVERIFY(selected);tree->setCurrentItem(selected);workspace->findChild<QPushButton*>("workspaceCanvas")->click();QTRY_VERIFY_WITH_TIMEOUT(!w->busy(),10000);workspace->hide();
    };openScreen(window.get());QVERIFY(!QTest::currentTestFailed());
    auto bar=window->findChild<CanvasStateBar*>("canvasStateBar");auto canvas=window->findChild<ScreenCanvas*>("screenCanvas");QVERIFY(bar);QVERIFY(canvas);auto preview=bar->renderer();
    auto configure=[&](QString actorId,bool changed){
      QTimer::singleShot(30,preview,[&,actorId,changed]{
        auto d=preview->findChild<QDialog*>("authoringPreviewDialog");QVERIFY(d);
        d->findChild<QComboBox*>("previewStateSource")->setCurrentIndex(1);d->findChild<QLineEdit*>("previewSavePath")->setText(config["save"].toString());
        auto a=d->findChild<QComboBox*>("previewActor");a->setCurrentIndex(a->findData(actorId));
        d->findChild<QComboBox*>("previewFlagScope")->setCurrentIndex(1);
        auto use=d->findChild<QCheckBox*>("previewUseAttribute-"+config["attribute"].toString());QVERIFY(use);use->setChecked(changed);
        d->findChild<QSpinBox*>("previewAttribute-"+config["attribute"].toString())->setValue(config["override"].toInt());
        auto flag=d->findChild<QComboBox*>("previewFlag-"+config["flag"].toString());QVERIFY(flag);flag->setCurrentIndex(changed?1:0);
        d->findChild<QDialogButtonBox*>("previewScenarioButtons")->button(QDialogButtonBox::Apply)->click();
      });bar->findChild<QPushButton*>("canvasStateConfigure")->click();
    };
    auto wait=[&]{QTRY_VERIFY_WITH_TIMEOUT(!window->busy()&&!preview->running()&&canvas->hasRuntimePreview(),90000);};
    const auto before=window->currentView();const auto first=config["actors"].toArray().first().toObject()["id"].toString();
    configure(first,false);wait();QVERIFY(!QTest::currentTestFailed());QCOMPARE(canvas->runtimeResult()["state"],savedState);qInfo()<<"Canvas baseline ready";
    const auto initialTexts=canvas->runtimeResult()["texts"].toObject();for(auto id:config["texts"].toObject().keys())QCOMPARE(initialTexts[id],config["texts"].toObject()[id]);
    // The editable scene displays the actual player image, not a second text/condition interpreter.
    canvas->selectMany({});const auto image=preview->snapshotImage().convertToFormat(QImage::Format_ARGB32);QImage drawn(image.size(),QImage::Format_ARGB32);drawn.fill(Qt::transparent);
    {QPainter painter(&drawn);static_cast<QGraphicsView*>(canvas)->scene()->render(&painter,QRectF(drawn.rect()),QRectF(0,0,image.width(),image.height()));}
    QCOMPARE(drawn,image);window->grab().save(out+"/saved-canvas.png");
    // A failed refresh must not make an older successful image current again.
    preview->fail("Canvas proof: failed refresh");QVERIFY(!canvas->hasRuntimePreview());QVERIFY(!preview->snapshotCurrent());
    bar->updateContext(window->currentView(),config["screen"].toString(),"ko",image.size());QVERIFY(!canvas->hasRuntimePreview());
    bar->findChild<QPushButton*>("canvasStateRefresh")->click();wait();QVERIFY(!QTest::currentTestFailed());QCOMPARE(canvas->runtimeResult()["state"],savedState);
    configure(first,true);wait();QVERIFY(!QTest::currentTestFailed());
    QCOMPARE(canvas->runtimeResult()["state"].toObject()["roster"].toObject()["attributes"].toObject()[first].toObject()[config["attribute"].toString()].toInt(),config["override"].toInt());
    QVERIFY(canvas->runtimeResult()["texts"].toObject()[config["variantElement"].toString()]!=initialTexts[config["variantElement"].toString()]);
    window->grab().save(out+"/virtual-canvas.png");
    // Quick actor change also applies attribute/flag overrides only to the newly chosen actor.
    auto actors=bar->findChild<QComboBox*>("canvasStateActor");const auto second=config["actors"].toArray().at(1).toObject()["id"].toString();const int index=actors->findData(second);QVERIFY(index>=0);actors->setCurrentIndex(index);emit actors->activated(index);wait();QVERIFY(!QTest::currentTestFailed());
    QCOMPARE(canvas->runtimeResult()["state"].toObject()["roster"].toObject()["attributes"].toObject()[second].toObject()[config["attribute"].toString()].toInt(),config["override"].toInt());
    QCOMPARE(canvas->runtimeResult()["texts"].toObject()[config["variantElement"].toString()],initialTexts[config["variantElement"].toString()]);
    for(auto field:{"documents","revision","dirty","undo","redo"})QCOMPARE(window->currentView()[field],before[field]);
    configure(first,false);wait();QVERIFY(!QTest::currentTestFailed());qInfo()<<"Canvas actor and variant checks ready";
    canvas->select(config["element"].toString());canvas->setFocus();QTest::keyClick(canvas,Qt::Key_Right);
    QTRY_VERIFY_WITH_TIMEOUT(!window->busy()&&window->currentView()["dirty"].toBool(),10000);wait();QVERIFY(!QTest::currentTestFailed());
    window->findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!window->busy()&&!window->currentView()["dirty"].toBool(),30000);wait();QVERIFY(!QTest::currentTestFailed());
    qInfo()<<"Canvas edit saved";const auto authored=window->projectData();QVERIFY(window->close());qInfo()<<"Canvas window closed";window.reset();qInfo()<<"Canvas window destroyed";
    StudioWindow reopened(engine);reopened.resize(1280,900);reopened.show();reopened.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!reopened.busy()&&!reopened.projectData().isEmpty(),30000);openScreen(&reopened);QVERIFY(!QTest::currentTestFailed());
    auto reopenedCanvas=reopened.findChild<ScreenCanvas*>("screenCanvas");QTRY_VERIFY_WITH_TIMEOUT(reopenedCanvas->hasRuntimePreview()&&!reopened.busy(),90000);
    QCOMPARE(reopened.projectData(),authored);auto expected=savedState.toObject();expected["contentFingerprint"]=reopenedCanvas->runtimeResult()["state"].toObject()["contentFingerprint"];QCOMPARE(reopenedCanvas->runtimeResult()["state"].toObject(),expected);reopened.grab().save(out+"/reopened-canvas.png");
    QVERIFY(save.open(QIODevice::ReadOnly));QCOMPARE(save.readAll(),bytes);
    QFile proof(out+"/canvas-result.json");QVERIFY(proof.open(QIODevice::WriteOnly));proof.write(QJsonDocument(reopenedCanvas->runtimeResult()).toJson());
  }
};
QTEST_MAIN(CanvasStateTest)
#include "canvas-state-test.moc"
