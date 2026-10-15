#include "studio.hpp"
#include "project-manager.hpp"
#include "theme.hpp"
#include "renderer-preview.hpp"
#include "script-authoring.hpp"
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QInputDialog>
#include <QJsonDocument>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScopeGuard>
#include <QSplitter>
#include <QTableWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QTimer>
#include <QTemporaryDir>
#include <QToolBar>
#include <QToolButton>
#include <QtTest>

class StudioTests : public QObject {
  Q_OBJECT
private slots:
  void projectManagerRoundtrip();
  void designerQuickRules();
  void designerSmallGameAuthoring();
  void designerSampleCleanup();
  void designerSmallGamePlay();
  void designerPublishSample();
  void rosterAuthoringRoundtrip();
  void rosterPlayIntegration();
  void characterStateAuthoringRoundtrip();
  void characterStatePlayIntegration();
  void timeSessionAuthoringRoundtrip();
  void authoringWorkspaceRoundtrip();
  void uiCompositionAuthoringRoundtrip();
  void uiCompositionPlayIntegration();
  void commercialPilotAuthoring();
  void commercialPilotPolish();
  void commercialPilotPlay();
  void goalRoutesRecordAndReplay();
  void goalSearchAndRegister();
  void goalCompareGraphReplay();
  void goalHistoricalBaseline();
  void goalSaveImpact();
  void goalPartialApproval();
  void goalGraphApproval();
  void goalCiReview();
  void benchmarkAuthoringSuite();
  void benchmarkPlayAndAssurance();
  void directionAuthoringRoundtrip();
  void directionPlayIntegration();
  void tacticsAuthoringRoundtrip();
  void tacticsPlayIntegration();
  void authoringWorkspacePlayIntegration();
  void timeSessionPlayIntegration();
  void screenSettingsRoundtrip();
  void screenSettingsGuards();
  void userFieldsAuthoringRoundtrip();
  void userFieldsAuthoringGuards();
  void textBindingBatchRoundtrip();
  void textBindingBatchKindsAndGuards();
  void duplicateElementMenuAndShortcut();
  void elementVisibilityConditionRoundtrip();
  void hierarchySearchAcrossScreens();
  void hierarchyBoundTextLabels();
  void expeditionAuthoringRoundtrip();
  void expeditionAuthoringGuards();
  void scriptAuthoringCreateConnectRoundTrip();
  void scriptRoundsBuildAndPreserve();
  void scriptRoundsDialogAndTransaction();
  void scriptAuthoringPreservesAndRejectsBrokenLinks();
  void timelineStageAuthoring();
  void inspectorFocusRoundtrip();
  void inspectorFocusDoesNotSteal();
  void inspectorWheelScrollsWithoutEditing();
  void inspectorWheelDoesNotChangeProject();
  void contentActivityLogicRoundtrip();
  void contentAttributesAuthoringGuards();
  void contentActivityPreviewRoundtrip();
  void contentCharacterPortraitRoundtrip();
  void contentCharacterPortraitGuards();
  void contentCharacterSpeakerPortraitGuard();
  void scriptPortraitTransformRoundtrip();
  void scriptPortraitTransformPreservesPrecision();
  void scriptScenePreviewDraftIsolation();
  void scriptScenePreviewNativeRoundtrip();
  void exportGameDialogRoundtrip();
  void progressionAuthoringRoundtrip();
  void progressionAuthoringGuards();
  void advanceButtonAuthoring();
  void projectSetupAuthoring();
  void workbenchValidationHierarchy();
  void workbenchAnalysis5000();
  void workbenchAnalysisHistory();
  void workbenchBranchTree();
  void workbenchAncestorLocks();
  void componentHierarchyAuthoring();
  void battleHudAuthoring();
  void battleHudRoundtrip();
  void battleHudGuards();
  void timelineAuthoring();
  void timelineLegacyProject();
  void dataUiAuthoring();
  void dataUiEditRoundtrip();
  void facilitiesAuthoringAndPlay();
  void genericRulesAuthoringAndPlay();
  void genericRulesMigration();
  void logicAstEditor();
  void logicAuthoringAndDebug();
  void genericRulesLegacyProject();
  void facilityAuthoringGuards();
  void pm2FacilitiesSample();
  void pm2FacilitiesPolish();
  void pluginNoPluginsPalette();
  void pluginMissingPlaceholder();
  void pluginManagerStates();
  void pluginLoaderRejectsNonPlugin();
  void pluginWorkspaceSwitch();
  void cutoutEndToEnd();
  void initTestCase() {
    applyStudioTheme(
        *qobject_cast<QApplication *>(QCoreApplication::instance()));
  }
  void dockingLayout() {
    StudioWindow w(
        qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath()));
    w.show();
    auto dock = w.findChild<QDockWidget *>("hierarchyDock");
    QVERIFY(dock);
    const auto saved = w.saveState(1);
    QTest::mouseDClick(dock, Qt::LeftButton, Qt::NoModifier, QPoint(80, 10));
    QTRY_VERIFY(dock->isFloating());
    QVERIFY(w.restoreState(saved, 1));
    QTRY_VERIFY(!dock->isFloating());
    QCOMPARE(w.dockWidgetArea(dock), Qt::LeftDockWidgetArea);
  }
  void assetAndContentAuthoring() {
    StudioWindow w(
        qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath()));
    w.show();
    w.openProject(qEnvironmentVariable("RAISING_QT_PROJECT"));
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && !w.projectData().isEmpty(), 20000);
    auto tabs = w.findChild<QTabWidget *>();
    tabs->setCurrentIndex(1);
    auto assets = w.findChild<QListWidget *>("assetList");
    QVERIFY(assets->count() > 0);
    auto asset = assets->item(0);
    assets->scrollToItem(asset);
    QTest::mouseClick(assets->viewport(),Qt::LeftButton,Qt::NoModifier,assets->visualItemRect(asset).center());
    QTest::mouseDClick(assets->viewport(), Qt::LeftButton, Qt::NoModifier,
                       assets->visualItemRect(asset).center());
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && w.selectedId().startsWith("image-"),
                             10000);
    auto imageId = w.selectedId();
    auto find = [&w](QString id) {
      for (auto s : w.projectData()["visual"].toObject()["screens"].toArray())
        for (auto e : s.toObject()["elements"].toArray())
          if (e.toObject()["id"] == id)
            return e.toObject();
      return QJsonObject();
    };
    QCOMPARE(find(imageId)["kind"].toString(), QString("image"));
    QVERIFY(!find(imageId)["imageAssetId"].toString().isEmpty());
    auto palette = w.findChild<QToolButton *>("elementPalette");
    QVERIFY(palette);
    palette->menu()->actions().first()->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && w.selectedId().startsWith("text-"),
                             10000);
    const auto textId = w.selectedId();
    auto binding = w.findChild<QComboBox *>("textBinding");
    QVERIFY(binding);
    binding->setFocus();
    QTest::keyClick(binding, Qt::Key_Home);
    QTest::keyClick(binding, Qt::Key_Down);
    QTRY_VERIFY_WITH_TIMEOUT(
        !w.busy() && find(textId)["binding"] == "attribute-value", 10000);
    QVERIFY(!find(textId)["attributeId"].toString().isEmpty());
    tabs->setCurrentIndex(4);
    auto content = w.findChild<QListWidget *>("contentList");
    QListWidgetItem *activity = nullptr;
    for (int i = 0; i < content->count(); ++i)
      if (content->item(i)
              ->data(Qt::UserRole)
              .toString()
              .contains("activities/")) {
        activity = content->item(i);
        break;
      }
    QVERIFY(activity);
    const QString file = activity->data(Qt::UserRole).toString();
    content->scrollToItem(activity);
    QTimer::singleShot(150, &w, [] {
      auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
      QVERIFY(dialog);
      auto label = dialog->findChild<QLineEdit *>("contentLabel");
      QVERIFY(label);
      label->setText(QString::fromUtf8("편집 테스트 활동"));
      auto effects = dialog->findChild<QTableWidget *>("content-baseEffects");
      QVERIFY(effects);
      auto box = dialog->findChild<QDialogButtonBox *>();
      QTest::mouseClick(box->button(QDialogButtonBox::Save), Qt::LeftButton);
    });
    QTest::mouseClick(content->viewport(), Qt::LeftButton, Qt::NoModifier,
                      content->visualItemRect(activity).center());
    QTest::mouseDClick(content->viewport(), Qt::LeftButton, Qt::NoModifier,
                       content->visualItemRect(activity).center());
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    auto label = [&] {
      for (auto d : w.currentView()["documents"].toArray())
        if (d.toObject()["file"] == file)
          return d.toObject()["data"].toObject()["label"].toString();
      return QString();
    };
    QTRY_COMPARE(label(), QString::fromUtf8("편집 테스트 활동"));
    w.findChild<QAction *>("undoEdit")->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(
        !w.busy() && label() != QString::fromUtf8("편집 테스트 활동"), 10000);
    w.findChild<QAction *>("undoEdit")->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(
        !w.busy() && find(textId)["binding"] != "attribute-value", 10000);
    w.findChild<QAction *>("undoEdit")->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && find(textId).isEmpty(), 10000);
    w.findChild<QAction *>("undoEdit")->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && find(imageId).isEmpty(), 10000);
  }
  void calendarWidgetAuthoring() {
    StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()));w.show();w.openProject(qEnvironmentVariable("RAISING_QT_PROJECT"));
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
    auto palette=w.findChild<QToolButton*>("elementPalette");QVERIFY(palette);
    palette->menu()->actions().at(4)->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&w.selectedId().startsWith("schedule-"),10000);
    const auto id=w.selectedId();
    QTimer::singleShot(100,&w,[]{
      auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(d);
      auto mode=d->findChild<QComboBox*>("widgetLayout");QVERIFY(mode);mode->setCurrentIndex(mode->findData("menu"));
      for(auto b:d->findChildren<QPushButton*>())if(b->text()==QString::fromUtf8("분류 추가")){QTest::mouseClick(b,Qt::LeftButton);break;}
      auto table=d->findChild<QTableWidget*>("scheduleGroups");QVERIFY(table);QCOMPARE(table->rowCount(),1);
      auto entries=d->findChild<QListWidget*>("widgetEntries");QVERIFY(entries);QVERIFY(entries->count()>0);entries->item(0)->setCheckState(Qt::Checked);
      auto theme=d->findChild<QComboBox*>("widgetTheme");theme->setCurrentIndex(theme->findData("ornate"));
      QTest::mouseClick(d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok),Qt::LeftButton);
    });
    QTest::mouseClick(w.findChild<QPushButton*>("widgetSettings"),Qt::LeftButton);
    auto element=[&] {for(auto s:w.projectData()["visual"].toObject()["screens"].toArray())for(auto e:s.toObject()["elements"].toArray())if(e.toObject()["id"]==id)return e.toObject();return QJsonObject();};
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&element()["scheduleLayout"]=="menu",10000);
    QCOMPARE(element()["scheduleGroups"].toArray().size(),1);QCOMPARE(element()["scheduleGroups"].toArray()[0].toObject()["activityIds"].toArray().size(),1);
    QCOMPARE(w.projectData()["visual"].toObject()["widgetTheme"].toString(),QString("ornate"));
    w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&element()["scheduleLayout"]!="menu",10000);
  }
  void sharedStyleAuthoring() {
    StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()));w.show();w.openProject(qEnvironmentVariable("RAISING_QT_PROJECT"));
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
    w.findChild<QToolButton*>("elementPalette")->menu()->actions().first()->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&w.selectedId().startsWith("text-"),10000);
    const auto id=w.selectedId();
    auto element=[&]{for(auto s:w.projectData()["visual"].toObject()["screens"].toArray())for(auto e:s.toObject()["elements"].toArray())if(e.toObject()["id"]==id)return e.toObject();return QJsonObject();};
    QTimer::singleShot(100,&w,[]{
      auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(d);QCOMPARE(d->objectName(),QString("designSystemDialog"));
      QTest::mouseClick(d->findChild<QPushButton*>("addSharedStyle"),Qt::LeftButton);
      d->findChild<QLineEdit*>("styleLabel")->setText(QString::fromUtf8("상점 공통 모양"));
      d->findChild<QSpinBox*>("style-fontSize")->setValue(32);
      d->findChild<QLineEdit*>("style-fill")->setText("#543c74");
      auto scope=d->findChild<QComboBox*>("styleApplyScope");scope->setCurrentIndex(scope->findData("selected"));
      QTest::mouseClick(d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok),Qt::LeftButton);
    });
    w.findChild<QAction*>("editDesignSystem")->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&element()["styleId"]=="style-1",10000);
    QVERIFY(!element().contains("fontSize"));
    QCOMPARE(w.projectData()["visual"].toObject()["styles"].toArray()[0].toObject()["fontSize"].toInt(),32);
    auto field=w.findChild<QComboBox*>("elementStyle");QCOMPARE(field->currentData().toString(),QString("style-1"));
    w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!element().contains("styleId"),10000);
    QVERIFY(w.projectData()["visual"].toObject()["styles"].toArray().isEmpty());
    w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&element()["styleId"]=="style-1",10000);
    w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!element().contains("styleId"),10000);
    w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&element().isEmpty(),10000);
  }
  void localizationAuthoringAndPlay() {
    StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()));w.show();w.openProject(qEnvironmentVariable("RAISING_QT_PROJECT"));
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
    QTimer::singleShot(150,&w,[]{
      auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(d);QCOMPARE(d->objectName(),QString("localizationDialog"));
      QTimer::singleShot(10000,d,&QDialog::reject);
      QTimer::singleShot(100,d,[]{auto input=qobject_cast<QInputDialog*>(QApplication::activeModalWidget());QVERIFY(input);input->setTextValue("en");input->accept();});
      QTest::mouseClick(d->findChild<QPushButton*>("addLocale"),Qt::LeftButton);
      auto locale=d->findChild<QComboBox*>("translationLocale");QCOMPARE(locale->currentData().toString(),QString("en"));
      d->findChild<QLineEdit*>("localeLabel")->setText("English");
      auto defaults=d->findChild<QComboBox*>("defaultLocale");defaults->setCurrentIndex(defaults->findData("en"));
      auto table=d->findChild<QTableWidget*>("translationTable");QVERIFY(table->rowCount()>10);int row=-1;
      for(int i=0;i<table->rowCount();i++)if(table->item(i,0)->text()=="/project/visual/screens/@title/elements/@start/text")row=i;
      QVERIFY(row>=0);table->setCurrentCell(row,2);d->findChild<QPlainTextEdit*>("translationText")->setPlainText("Begin your story");
      QCOMPARE(table->item(row,2)->text(),QString("Begin your story"));
      QTest::mouseClick(d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save),Qt::LeftButton);
    });
    w.findChild<QAction*>("editLocalization")->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&w.projectData()["localization"].toObject()["defaultLocale"]=="en",10000);
    QCOMPARE(w.findChild<QComboBox*>("previewLocale")->currentData().toString(),QString("en"));
    const auto root=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
    w.playAutomation=root+"/.cache/qt-localization-steps.json";w.playReport=root+"/.cache/qt-localization-report.json";
    QFile steps(w.playAutomation);QVERIFY(steps.open(QIODevice::WriteOnly));steps.write(R"([{"waitFrames":8},{"checkpoint":"english"},{"key":"F2"},{"waitFrames":3},{"checkpoint":"korean"}])");steps.close();
    QSignalSpy finished(&w,&StudioWindow::playerFinished);w.findChild<QAction*>("playProject")->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(finished.count()>0,30000);QCOMPARE(finished.first().first().toInt(),0);
    QFile report(w.playReport);QVERIFY(report.open(QIODevice::ReadOnly));const auto result=QJsonDocument::fromJson(report.readAll()).object();QVERIFY(result["ok"].toBool());
    bool checked=false;for(auto step:result["steps"].toArray()){auto row=step.toObject();if(row["command"].toObject()["checkpoint"]!="english")continue;auto ui=row["ui"].toObject();QCOMPARE(ui["locale"].toString(),QString("en"));for(auto el:ui["elements"].toArray())if(el.toObject()["id"]=="start"){QCOMPARE(el.toObject()["label"].toString(),QString("Begin your story"));checked=true;}}
    QVERIFY(checked);
    w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().contains("localization"),10000);
    w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  }
  void reusableComponentsAndResponsivePlay() {
    const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
    StudioWindow w(engine);w.show();w.openProject(qEnvironmentVariable("RAISING_QT_PROJECT"));
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
    const auto baseline=w.projectData();
    // A failed assertion must not leave shared fixture edits for the following tests.
    auto restore=qScopeGuard([&]{
      for(int i=0;i<30&&w.projectData()!=baseline;i++){
        if(auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget()))d->reject();
        w.findChild<QAction*>("undoEdit")->trigger();
        QElapsedTimer timer;timer.start();while(w.busy()&&timer.elapsed()<10000)QTest::qWait(20);
      }
      w.findChild<QAction*>("saveProject")->trigger();
      QElapsedTimer timer;timer.start();while(w.busy()&&timer.elapsed()<10000)QTest::qWait(20);
    });
    auto screen=[&](QString id){for(auto s:w.projectData()["visual"].toObject()["screens"].toArray())if(s.toObject()["id"]==id)return s.toObject();return QJsonObject{};};
    auto element=[&](QString sid,QString id){for(auto e:screen(sid)["elements"].toArray())if(e.toObject()["id"]==id)return e.toObject();return QJsonObject{};};
    auto openScreen=[&](QString id){
      auto list=w.findChild<QListWidget*>("projectList");
      for(int i=0;i<list->count();i++)if(list->item(i)->data(Qt::UserRole)==id){auto item=list->item(i);list->scrollToItem(item);QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(item).center());QTest::mouseDClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(item).center());return;}
      QFAIL("Screen missing in project panel");
    };
    auto edit=[&](std::function<void(QDialog*)> action){
      QTimer::singleShot(100,&w,[action]{auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(d);QCOMPARE(d->objectName(),QString("componentsDialog"));QTimer::singleShot(10000,d,&QDialog::reject);action(d);});
      w.findChild<QAction*>("editComponents")->trigger();
      QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
      QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
    };
    auto activate=[&](const char* name,QString value){auto field=w.findChild<QComboBox*>(name);QVERIFY(field);int index=field->findData(value);QVERIFY(index>=0);field->setCurrentIndex(index);QMetaObject::invokeMethod(field,"activated",Q_ARG(int,index));QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);};
    openScreen("home");
    edit([](QDialog* d){
      QTest::mouseClick(d->findChild<QPushButton*>("addComponent"),Qt::LeftButton);
      d->findChild<QLineEdit*>("componentName")->setText("Product card");
      d->findChild<QListWidget*>("componentChildren")->setCurrentRow(1);
      d->findChild<QLineEdit*>("child-text")->setText("Shared Product");
      QTest::mouseClick(d->findChild<QPushButton*>("placeComponent"),Qt::LeftButton);
    });
    QCOMPARE(element("home","instance-1")["componentId"].toString(),QString("component-1"));
    auto resizeInstance=[&](const char* name,int value){auto field=w.findChild<QSpinBox*>(name);field->setFocus();field->selectAll();QTest::keyClicks(field,QString::number(value));QTest::keyClick(field,Qt::Key_Return);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);};
    resizeInstance("elementWidth",480);resizeInstance("elementHeight",280);
    QCOMPARE(element("home","instance-1")["width"].toInt(),480);QCOMPARE(element("home","instance-1")["height"].toInt(),280);
    activate("anchorX","right");activate("anchorY","bottom");activate("viewportMode","responsive");
    QCOMPARE(element("home","instance-1")["anchorX"].toString(),QString("right"));
    QCOMPARE(element("home","instance-1")["anchorY"].toString(),QString("bottom"));
    openScreen("title");
    edit([](QDialog* d){QTest::mouseClick(d->findChild<QPushButton*>("placeComponent"),Qt::LeftButton);});
    QCOMPARE(element("title","instance-1")["componentId"].toString(),QString("component-1"));
    QTimer::singleShot(100,&w,[]{
      auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(d);QCOMPARE(d->objectName(),QString("componentOverridesDialog"));QTimer::singleShot(10000,d,&QDialog::reject);
      auto child=d->findChild<QComboBox*>("overrideChild");child->setCurrentIndex(child->findData("description"));
      d->findChild<QLineEdit*>("child-text")->setText("Title-only description");
      child->setCurrentIndex(child->findData("confirm"));
      auto action=d->findChild<QComboBox*>("child-action");action->setCurrentIndex(action->findData("start"));
      d->findChild<QLineEdit*>("child-text")->setText("Start from component");
      QTest::mouseClick(d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save),Qt::LeftButton);
    });
    QTest::mouseClick(w.findChild<QPushButton*>("componentOverrides"),Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&element("title","instance-1")["overrides"].toArray().size()==2,10000);
    // Change the shared original after placing both instances. Overrides survive.
    edit([](QDialog* d){
      auto children=d->findChild<QListWidget*>("componentChildren");children->setCurrentRow(1);d->findChild<QLineEdit*>("child-text")->setText("Shared Product Revised");
      children->setCurrentRow(2);d->findChild<QLineEdit*>("child-text")->setText("Shared description revised");
      QTest::mouseClick(d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save),Qt::LeftButton);
    });
    openScreen("home");
    edit([](QDialog* d){
      auto type=d->findChild<QComboBox*>("componentTemplate");type->setCurrentIndex(type->findData("confirm"));
      QTest::mouseClick(d->findChild<QPushButton*>("addComponent"),Qt::LeftButton);
      d->findChild<QListWidget*>("componentChildren")->setCurrentRow(3);
      d->findChild<QLineEdit*>("child-text")->setText("Open schedule");
      auto target=d->findChild<QComboBox*>("child-screenId");target->setCurrentIndex(target->findData("schedule"));
      QTest::mouseClick(d->findChild<QPushButton*>("placeComponent"),Qt::LeftButton);
    });
    QCOMPARE(element("home","instance-2")["componentId"].toString(),QString("component-2"));
    auto x=w.findChild<QSpinBox*>("elementX");x->setFocus();x->selectAll();QTest::keyClicks(x,"520");QTest::keyClick(x,Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&element("home","instance-2")["x"].toInt()==520,10000);
    for(auto d:w.currentView()["diagnostics"].toArray())QVERIFY2(d.toObject()["severity"]!="error",qPrintable(d.toObject()["message"].toString()));
    w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
    // The following Play action exports the saved project through a fresh loader.
    // Keep this editor session open so undo can restore the shared test fixture.
    const auto evidence=engine+"/.cache/qt-components-evidence";QDir().mkpath(evidence);w.grab().save(evidence+"/editor.png");
    QFile authored(evidence+"/authored-project.json");QVERIFY(authored.open(QIODevice::WriteOnly));authored.write(QJsonDocument(w.projectData()).toJson());authored.close();
    QJsonArray steps{
      QJsonObject{{"resize",QJsonObject{{"width",1280},{"height",720}}}},QJsonObject{{"waitFrames",5}},QJsonObject{{"checkpoint","title"}},QJsonObject{{"screenshot",evidence+"/title.png"}},
      QJsonObject{{"click","instance-1__confirm"}},QJsonObject{{"waitFrames",3}},QJsonObject{{"expectScreen","home"}},QJsonObject{{"checkpoint","home"}},
      QJsonObject{{"resize",QJsonObject{{"width",1600},{"height",720}}}},QJsonObject{{"waitFrames",5}},QJsonObject{{"checkpoint","wide"}},QJsonObject{{"screenshot",evidence+"/wide.png"}},
      QJsonObject{{"resize",QJsonObject{{"width",720},{"height",1000}}}},QJsonObject{{"waitFrames",5}},QJsonObject{{"checkpoint","portrait"}},QJsonObject{{"screenshot",evidence+"/portrait.png"}},
      QJsonObject{{"key","F5"}},QJsonObject{{"waitFrames",3}},QJsonObject{{"key","F9"}},QJsonObject{{"waitFrames",3}},QJsonObject{{"checkpoint","restored"}},
      QJsonObject{{"resize",QJsonObject{{"width",1280},{"height",720}}}},QJsonObject{{"waitFrames",5}},QJsonObject{{"click","instance-2__confirm"}},QJsonObject{{"waitFrames",3}},QJsonObject{{"expectScreen","schedule"}}
    };
    w.playAutomation=evidence+"/steps.json";w.playReport=evidence+"/player-report.json";
    QFile stepsFile(w.playAutomation);QVERIFY(stepsFile.open(QIODevice::WriteOnly));stepsFile.write(QJsonDocument(steps).toJson());stepsFile.close();
    QSignalSpy finished(&w,&StudioWindow::playerFinished);w.findChild<QAction*>("playProject")->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(),60000);QCOMPARE(finished.first().first().toInt(),0);
    QFile reportFile(w.playReport);QVERIFY(reportFile.open(QIODevice::ReadOnly));const auto report=QJsonDocument::fromJson(reportFile.readAll()).object();QVERIFY(report["ok"].toBool());
    auto checkpoint=[&](QString id){for(auto s:report["steps"].toArray())if(s.toObject()["command"].toObject()["checkpoint"]==id)return s.toObject()["ui"].toObject();return QJsonObject{};};
    auto node=[](QJsonObject ui,QString id){for(auto n:ui["nodes"].toArray())if(n.toObject()["id"]==id)return n.toObject();return QJsonObject{};};
    const auto title=checkpoint("title"),home=checkpoint("home"),wide=checkpoint("wide"),portrait=checkpoint("portrait");
    QCOMPARE(title["texts"].toObject()["instance-1__title"].toString(),QString("Shared Product Revised"));
    QCOMPARE(home["texts"].toObject()["instance-1__title"].toString(),QString("Shared Product Revised"));
    QCOMPARE(title["texts"].toObject()["instance-1__description"].toString(),QString("Title-only description"));
    QCOMPARE(home["texts"].toObject()["instance-1__description"].toString(),QString("Shared description revised"));
    QVERIFY(!home["state"].isNull());QCOMPARE(home["state"],wide["state"]);QCOMPARE(home["state"],portrait["state"]);QCOMPARE(home["state"],checkpoint("restored")["state"]);
    const auto base=node(home,"instance-1__background"),right=node(wide,"instance-1__background"),bottom=node(portrait,"instance-1__background");
    QVERIFY(!base.isEmpty());QCOMPARE(right["width"],base["width"]);QCOMPARE(bottom["height"],base["height"]);
    QCOMPARE(base["width"].toDouble(),480.0);QCOMPARE(base["height"].toDouble(),280.0);
    QCOMPARE(node(home,"instance-1__title")["width"].toDouble()-node(title,"instance-1__title")["width"].toDouble(),100.0);
    QCOMPARE(node(home,"instance-1__confirm")["y"].toDouble()-node(title,"instance-1__confirm")["y"].toDouble(),40.0);
    QCOMPARE(right["x"].toDouble()-base["x"].toDouble(),wide["viewport"].toObject()["width"].toDouble()-home["viewport"].toObject()["width"].toDouble());
    QCOMPARE(bottom["y"].toDouble()-base["y"].toDouble(),portrait["viewport"].toObject()["height"].toDouble()-home["viewport"].toObject()["height"].toDouble());
  }
  void realRendererPreview() {
    const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
    const auto project=qEnvironmentVariable("RAISING_QT_PROJECT");
    StudioWindow w(engine);w.show();w.openProject(project);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
    const auto baseline=w.projectData();
    QFile original(project+"/project.json");QVERIFY(original.open(QIODevice::ReadOnly));const auto disk=original.readAll();original.close();
    auto restore=qScopeGuard([&]{
      for(int i=0;i<10&&w.projectData()!=baseline;i++){
        w.findChild<QAction*>("undoEdit")->trigger();
        QElapsedTimer timer;timer.start();while(w.busy()&&timer.elapsed()<10000)QTest::qWait(20);
      }
      w.findChild<QAction*>("saveProject")->trigger();
      QElapsedTimer timer;timer.start();while(w.busy()&&timer.elapsed()<10000)QTest::qWait(20);
    });
    w.findChild<QToolButton*>("elementPalette")->menu()->actions().first()->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&w.selectedId().startsWith("text-"),10000);
    const auto textId=w.selectedId();
    auto text=w.findChild<QLineEdit*>("buttonText");QVERIFY(text);text->setFocus();text->selectAll();
    QTest::keyClicks(text,"Unsaved renderer preview");QTest::keyClick(text,Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
    const auto before=w.currentView();QVERIFY(before["dirty"].toBool());
    auto dock=w.findChild<RendererPreviewDock*>("rendererPreviewDock");QVERIFY(dock);
    QSignalSpy ready(dock,&RendererPreviewDock::snapshotReady);
    w.findChild<QAction*>("inspectRenderer")->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(),60000);
    QVERIFY2(ready.last().first().toBool(),qPrintable(dock->findChild<QLabel*>("rendererStatus")->text()));
    QVERIFY(!dock->running());
    const auto first=dock->result();QCOMPARE(first["screenId"].toString(),QString("home"));
    QCOMPARE(first["texts"].toObject()[textId].toString(),QString("Unsaved renderer preview"));
    QVERIFY(first["preview"].toObject()["active"].toBool());QVERIFY(first["preview"].toObject()["isolated"].toBool());
    QCOMPARE(first["preview"].toObject()["stateSource"].toString(),QString("initial"));
    bool textNode=false;for(auto n:first["nodes"].toArray())if(n.toObject()["id"]==textId)textNode=true;QVERIFY(textNode);
    for(const auto field:{"revision","dirty","undo","redo","draftVersion","diskVersion","documents"})QCOMPARE(w.currentView()[field],before[field]);
    QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(original.readAll(),disk);original.close();
    auto pin=dock->findChild<QPushButton*>("pinRendererPreview");QVERIFY(pin&&pin->isEnabled());
    QTest::mouseClick(pin,Qt::LeftButton);
    auto split=dock->findChild<QSplitter*>();QVERIFY(split);QVERIFY(!split->widget(0)->isHidden());
    auto resolution=dock->findChild<QComboBox*>("rendererResolution");QVERIFY(resolution);
    const int wide=resolution->findData(QSize(1600,900));QVERIFY(wide>=0);resolution->setCurrentIndex(wide);
    QMetaObject::invokeMethod(resolution,"activated",Q_ARG(int,wide));
    QCOMPARE(dock->findChild<QSpinBox*>("rendererWidth")->value(),1600);
    QCOMPARE(dock->findChild<QSpinBox*>("rendererHeight")->value(),900);
    auto element=dock->findChild<QComboBox*>("rendererElement");QVERIFY(element);const int save=element->findData("save");QVERIFY(save>=0);element->setCurrentIndex(save);
    auto interaction=dock->findChild<QComboBox*>("rendererInteraction");QVERIFY(interaction);interaction->setCurrentIndex(interaction->findData("hover"));
    ready.clear();QTest::mouseClick(dock->findChild<QPushButton*>("renderPreview"),Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(),60000);
    QVERIFY2(ready.last().first().toBool(),qPrintable(dock->findChild<QLabel*>("rendererStatus")->text()));
    const auto second=dock->result();const auto preview=second["preview"].toObject();
    QCOMPARE(preview["state"].toString(),QString("hover"));QCOMPARE(preview["elementId"].toString(),QString("save"));
    QCOMPARE(second["state"],first["state"]);
    QImage pixels(QFileInfo(dock->reportPath()).absolutePath()+"/preview.png");QCOMPARE(pixels.size(),QSize(1600,900));
    auto table=dock->findChild<QTreeWidget*>("rendererDiagnostics");QVERIFY(table);QTreeWidgetItem* selected=nullptr;
    for(int i=0;i<table->topLevelItemCount();i++)if(table->topLevelItem(i)->data(0,Qt::UserRole)=="save")selected=table->topLevelItem(i);
    QVERIFY(selected);table->scrollToItem(selected);QTest::mouseClick(table->viewport(),Qt::LeftButton,Qt::NoModifier,table->visualItemRect(selected).center());
    QTRY_COMPARE(w.selectedId(),QString("save"));
    QString runtimeId,ownerId;
    for(auto node:second["nodes"].toArray()){auto n=node.toObject();if(n["kind"]=="runtime-button"&&n["enabled"].toBool()){runtimeId=n["id"].toString();ownerId=n["ownerId"].toString();break;}}
    QVERIFY(!runtimeId.isEmpty());
    QTreeWidgetItem* runtimeRow=nullptr;for(int i=0;i<table->topLevelItemCount();i++)if(table->topLevelItem(i)->data(0,Qt::UserRole)==runtimeId)runtimeRow=table->topLevelItem(i);
    QVERIFY(runtimeRow);table->scrollToItem(runtimeRow);QTest::mouseClick(table->viewport(),Qt::LeftButton,Qt::NoModifier,table->visualItemRect(runtimeRow).center());
    QTRY_COMPARE(w.selectedId(),ownerId);QCOMPARE(element->currentData().toString(),runtimeId);
    ready.clear();QTest::mouseClick(dock->findChild<QPushButton*>("renderPreview"),Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(),60000);QVERIFY2(ready.last().first().toBool(),qPrintable(dock->findChild<QLabel*>("rendererStatus")->text()));
    QCOMPARE(dock->result()["preview"].toObject()["elementId"].toString(),runtimeId);
    QCOMPARE(dock->result()["state"],first["state"]);
    for(const auto field:{"revision","dirty","undo","redo","draftVersion","diskVersion","documents"})QCOMPARE(w.currentView()[field],before[field]);
    QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(original.readAll(),disk);original.close();
    dock->setFloating(true);dock->resize(1280,850);dock->show();QTest::qWait(100);
    QVERIFY(dock->grab().save(engine+"/.cache/renderer-preview-gui.png"));
  }
  void screenLifecycleAndPlay() {
    const auto engine =
        qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath());
    const auto project = qEnvironmentVariable("RAISING_QT_PROJECT");
    QVERIFY(!project.isEmpty());
    StudioWindow w(engine);
    w.show();
    w.openProject(project);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && !w.projectData().isEmpty(), 20000);
    QVERIFY2(w.lastError.isEmpty(), qPrintable(w.lastError));
    auto tabs = w.findChild<QTabBar *>("documentTabs");
    auto list = w.findChild<QListWidget *>("projectList");
    auto click = [&w](const char *name) {
      auto action = w.findChild<QAction *>(name);
      QVERIFY(action && action->isEnabled());
      QWidget *button = nullptr;
      for (auto bar : w.findChildren<QToolBar *>())
        if (bar->isVisible())
          if (auto widget = bar->widgetForAction(action))
            button = widget;
      QVERIFY(button);
      QTest::mouseClick(button, Qt::LeftButton);
    };
    auto screen = [&w](const QString &id) {
      for (auto s : w.projectData()["visual"].toObject()["screens"].toArray())
        if (s.toObject()["id"].toString() == id)
          return s.toObject();
      return QJsonObject();
    };
    auto selectProject = [list](const QString &id, bool open) {
      for (int i = 0; i < list->count(); ++i) {
        auto item = list->item(i);
        if (item->data(Qt::UserRole).toString() != id)
          continue;
        list->scrollToItem(item);
        auto point = list->visualItemRect(item).center();
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                          point);
        if (open)
          QTest::mouseDClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                             point);
        return;
      }
      QFAIL("Missing project screen");
    };
    auto enterName = [&w](const QString &name, bool accept = true) {
      QTimer::singleShot(100, &w, [name, accept] {
        auto dialog =
            qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        if (!accept) {
          QTest::keyClick(dialog, Qt::Key_Escape);
          return;
        }
        auto field = dialog->findChild<QLineEdit *>();
        field->selectAll();
        QTest::keyClicks(field, name);
        QTest::keyClick(field, Qt::Key_Return);
      });
    };
    auto closeTab = [tabs](int i) {
      auto button = tabs->tabButton(i, QTabBar::RightSide);
      if (!button)
        button = tabs->tabButton(i, QTabBar::LeftSide);
      QVERIFY(button);
      QTest::mouseClick(button, Qt::LeftButton);
    };
    auto text = [&w](const QString &value) {
      auto field = w.findChild<QLineEdit *>("buttonText");
      field->setFocus();
      field->selectAll();
      QTest::keyClicks(field, value);
      QTest::keyClick(field, Qt::Key_Return);
    };
    auto target = [&w](const QString &id) {
      auto field = w.findChild<QComboBox *>("actionTarget");
      const int index = field->findData(id);
      QVERIFY(index >= 0);
      field->setFocus();
      // Closed combo keyboard navigation activates exactly one final target.
      field->setCurrentIndex(index);
      QMetaObject::invokeMethod(field, "activated", Q_ARG(int, index));
    };
    auto answer = [&w](QMessageBox::StandardButton result,
                       const QString &expected, QString *details = nullptr) {
      QTimer::singleShot(100, &w, [result, expected, details] {
        auto box =
            qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        QVERIFY(box);
        QVERIFY2(box->text().contains(expected), qPrintable(box->text()));
        if (details)
          *details = box->detailedText();
        QTest::mouseClick(box->button(result), Qt::LeftButton);
      });
    };
    QCOMPARE(tabs->count(), 1);
    QCOMPARE(tabs->tabData(0).toString(), QString("home"));
    const int originalCount = list->count();
    enterName({}, false);
    click("newScreen");
    QCOMPARE(list->count(), originalCount);
    QVERIFY(!w.currentView()["dirty"].toBool());
    enterName("Travel room");
    click("newScreen");
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    const auto id = tabs->tabData(tabs->currentIndex()).toString();
    QVERIFY(id.startsWith("screen-"));
    QCOMPARE(screen(id)["label"].toString(), QString("Travel room"));
    QCOMPARE(tabs->count(), 2);
    click("undoEdit");
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QVERIFY(screen(id).isEmpty());
    QCOMPARE(tabs->count(), 1);
    QTest::keyClick(&w, Qt::Key_Y, Qt::ControlModifier);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QVERIFY(!screen(id).isEmpty());
    selectProject(id, true);
    click("addButton");
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    const auto back = w.selectedId();
    enterName("Copied room");
    click("duplicateScreen");
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    const auto copyId = tabs->tabData(tabs->currentIndex()).toString();
    QVERIFY(copyId != id);
    QCOMPARE(screen(copyId)["elements"]
                 .toArray()[0]
                 .toObject()["screenId"]
                 .toString(),
             copyId);
    QCOMPARE(
        screen(id)["elements"].toArray()[0].toObject()["screenId"].toString(),
        id);
    QCOMPARE(tabs->count(), 3);
    enterName("Renamed copy");
    click("renameScreen");
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QCOMPARE(screen(copyId)["label"].toString(), QString("Renamed copy"));
    answer(QMessageBox::Cancel, "Renamed copy");
    click("deleteScreen");
    QVERIFY(!screen(copyId).isEmpty());
    answer(QMessageBox::Yes, "Renamed copy");
    click("deleteScreen");
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QVERIFY(screen(copyId).isEmpty());
    QCOMPARE(tabs->count(), 2);
    click("undoEdit");
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QCOMPARE(screen(copyId)["label"].toString(), QString("Renamed copy"));
    QCOMPARE(tabs->count(), 2); // Undo does not reopen a closed document.
    QTest::keyClick(&w, Qt::Key_Y, Qt::ControlModifier);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QVERIFY(screen(copyId).isEmpty());
    selectProject(id, true);
    auto tree = w.findChild<QTreeWidget *>("hierarchy");
    auto buttonItem = tree->topLevelItem(0)->child(0);
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier,
                      tree->visualItemRect(buttonItem).center());
    text("Return home");
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    target("home");
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    while (tabs->count())
      closeTab(0);
    QCOMPARE(tabs->count(), 0);
    QVERIFY(!w.findChild<QAction *>("addButton")->isEnabled());
    click("saveProject");
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QCOMPARE(tabs->count(), 0);
    QVERIFY2(w.lastError.isEmpty(), qPrintable(w.lastError));
    selectProject("home", true);
    QCOMPARE(tabs->count(), 1);
    click("addButton");
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    const auto link = w.selectedId();
    text("Travel room");
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    target(id);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QTest::mouseClick(w.findChild<QPushButton *>("openActionTarget"),
                      Qt::LeftButton);
    QCOMPARE(tabs->count(), 2);
    QCOMPARE(tabs->tabData(tabs->currentIndex()).toString(), id);
    QString details;
    answer(QMessageBox::Ok, QString::fromUtf8("참조"), &details);
    click("deleteScreen");
    QVERIFY(details.contains("/screenId"));
    QVERIFY(!screen(id).isEmpty());
    selectProject("home", false);
    answer(QMessageBox::Ok, QString::fromUtf8("참조"), &details);
    click("deleteScreen");
    QVERIFY(details.contains("/gameplay/locations/"));
    selectProject("title", false);
    answer(QMessageBox::Ok, QString::fromUtf8("필수"));
    click("deleteScreen");
    QVERIFY(!screen("title").isEmpty());
    selectProject(id, true);
    enterName("Garden");
    click("renameScreen");
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QCOMPARE(screen(id)["label"].toString(), QString("Garden"));
    click("undoEdit");
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QCOMPARE(screen(id)["label"].toString(), QString("Travel room"));
    QTest::keyClick(&w, Qt::Key_Y, Qt::ControlModifier);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QCOMPARE(screen(id)["label"].toString(), QString("Garden"));
    QVERIFY(
        screen("home")["elements"].toArray().last().toObject()["screenId"] ==
        id);
    click("saveProject");
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QVERIFY2(w.lastError.isEmpty(), qPrintable(w.lastError));
    for (auto d : w.currentView()["diagnostics"].toArray())
      QVERIFY2(d.toObject()["severity"] != "error",
               qPrintable(d.toObject()["message"].toString()));
    auto evidence = engine + "/.cache/qt-screen-evidence";
    QDir().mkpath(evidence);
    w.grab().save(evidence + "/editor.png");
    QJsonArray steps{QJsonObject{{"click", "start"}},
                     QJsonObject{{"expectScreen", "home"}},
                     QJsonObject{{"click", link}},
                     QJsonObject{{"expectScreen", id}},
                     QJsonObject{{"screenshot", evidence + "/new-screen.png"}},
                     QJsonObject{{"click", back}},
                     QJsonObject{{"expectScreen", "home"}}};
    QFile stepsFile(evidence + "/steps.json");
    QVERIFY(stepsFile.open(QIODevice::WriteOnly));
    stepsFile.write(QJsonDocument(steps).toJson());
    stepsFile.close();
    w.playAutomation = stepsFile.fileName();
    w.playReport = evidence + "/player-report.json";
    QSignalSpy finished(&w, &StudioWindow::playerFinished);
    click("playProject");
    QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 60000);
    QCOMPARE(finished.first().first().toInt(), 0);
    QFile reportFile(w.playReport);
    QVERIFY(reportFile.open(QIODevice::ReadOnly));
    const auto report = QJsonDocument::fromJson(reportFile.readAll()).object();
    QVERIFY(report["ok"].toBool());
    QCOMPARE(report["final"].toObject()["screenId"].toString(),
             QString("home"));
    QVERIFY(w.findChild<QPlainTextEdit *>("executionLog")
                ->toPlainText()
                .contains(id));
    w.openProject(project);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QCOMPARE(tabs->count(), 1);
    QCOMPARE(screen(id)["label"].toString(), QString("Garden"));
    QCOMPARE(list->count(), originalCount + 1);
  }
  void authorAndPlay() {
    const auto engine =
        qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath());
    const auto project = qEnvironmentVariable("RAISING_QT_PROJECT");
    QVERIFY2(!project.isEmpty(),
             "RAISING_QT_PROJECT must point to a disposable sample copy");
    StudioWindow w(engine);
    w.resize(1440, 920);
    w.show();
    QTest::qWait(250);
    w.openProject(project);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && !w.projectData().isEmpty(), 20000);
    QVERIFY2(w.lastError.isEmpty(), qPrintable(w.lastError));
    auto clickAction = [&w](const char *name) {
      auto action = w.findChild<QAction *>(name);
      QVERIFY(action);
      QWidget *button = nullptr;
      for (auto bar : w.findChildren<QToolBar *>())
        if (auto widget = bar->widgetForAction(action))
          button = widget;
      QVERIFY(button);
      QTest::mouseClick(button, Qt::LeftButton);
    };
    clickAction("addButton");
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    const auto id = w.selectedId();
    QVERIFY(id.startsWith("button-"));
    auto editText = [&w](const char *name, const QString &value) {
      auto field = w.findChild<QLineEdit *>(name);
      QVERIFY(field);
      field->setFocus();
      field->selectAll();
      QTest::keyClicks(field, value);
      QTest::keyClick(field, Qt::Key_Return);
    };
    editText("buttonText", "Shop");
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    auto x = w.findChild<QSpinBox *>("elementX");
    x->setFocus();
    x->selectAll();
    QTest::keyClicks(x, "980");
    QTest::keyClick(x, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    auto y = w.findChild<QSpinBox *>("elementY");
    y->setFocus();
    y->selectAll();
    QTest::keyClicks(y, "600");
    QTest::keyClick(y, Qt::Key_Return);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    auto action = w.findChild<QComboBox *>("buttonAction");
    QVERIFY(action);
    action->setFocus();
    QTest::keyClick(action, Qt::Key_Down);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    auto findButton = [&]() {
      for (auto s : w.projectData()["visual"].toObject()["screens"].toArray())
        if (s.toObject()["id"] == "home")
          for (auto el : s.toObject()["elements"].toArray())
            if (el.toObject()["id"].toString() == id)
              return el.toObject();
      return QJsonObject();
    };
    QCOMPARE(findButton()["action"].toString(), QString("visit"));
    QCOMPARE(findButton()["locationId"].toString(), QString("market"));
    QCOMPARE(findButton()["x"].toInt(), 980);
    clickAction("undoEdit");
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QCOMPARE(findButton()["action"].toString(), QString("navigate"));
    auto a = w.findChild<QAction *>("undoEdit");
    Q_UNUSED(a);
    QTest::keyClick(&w, Qt::Key_Y, Qt::ControlModifier);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QCOMPARE(findButton()["action"].toString(), QString("visit"));
    auto canvas = w.findChild<ScreenCanvas *>("screenCanvas");
    QVERIFY(canvas);
    canvas->setFocus();
    QTest::keyClick(canvas, Qt::Key_Left);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QCOMPARE(findButton()["x"].toInt(), 979);
    const auto dragFrom = canvas->mapFromScene(QPointF(1089, 628));
    const auto dragTo = canvas->mapFromScene(QPointF(1025, 596));
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier,
                      dragFrom);
    QTest::mouseMove(canvas->viewport(), dragTo, 40);
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier,
                        dragTo);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QVERIFY(findButton()["x"].toInt() < 950);
    QVERIFY(findButton()["y"].toInt() < 600);
    auto rectangle = findButton();
    const auto resizeFrom = canvas->mapFromScene(
        QPointF(rectangle["x"].toInt() + 218, rectangle["y"].toInt() + 54));
    const auto resizeTo = resizeFrom + QPoint(18, 12);
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier,
                      resizeFrom);
    QTest::mouseMove(canvas->viewport(), resizeTo, 40);
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier,
                        resizeTo);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QVERIFY(findButton()["width"].toInt() > 220);
    QVERIFY(findButton()["height"].toInt() > 56);
    const int savedX = findButton()["x"].toInt();
    clickAction("saveProject");
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QVERIFY2(w.lastError.isEmpty(), qPrintable(w.lastError));
    QVERIFY(!w.currentView()["dirty"].toBool());
    const auto evidence = engine + "/.cache/qt-ui-evidence";
    QDir().mkpath(evidence);
    w.grab().save(evidence + "/editor.png");
    QJsonArray steps{QJsonObject{{"click", "start"}},
                     QJsonObject{{"expectScreen", "home"}},
                     QJsonObject{{"click", id}},
                     QJsonObject{{"expectScreen", "market"}},
                     QJsonObject{{"click", "buy-florist-flower"}},
                     QJsonObject{{"click", "save"}},
                     QJsonObject{{"screenshot", evidence + "/shop.png"}},
                     QJsonObject{{"click", "buy-florist-training-sword"}},
                     QJsonObject{{"expectScreen", "market"},
                                 {"checkpoint", "changed-after-save"}},
                     QJsonObject{{"click", "load"}},
                     QJsonObject{{"click", "visit-home"}},
                     QJsonObject{{"expectScreen", "home"}}};
    QFile stepsFile(evidence + "/steps.json");
    QVERIFY(stepsFile.open(QIODevice::WriteOnly));
    stepsFile.write(QJsonDocument(steps).toJson());
    stepsFile.close();
    w.playAutomation = stepsFile.fileName();
    w.playReport = evidence + "/player-report.json";
    QSignalSpy finished(&w, &StudioWindow::playerFinished);
    clickAction("playProject");
    QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 60000);
    QCOMPARE(finished.first().first().toInt(), 0);
    QFile reportFile(w.playReport);
    QVERIFY(reportFile.open(QIODevice::ReadOnly));
    auto report = QJsonDocument::fromJson(reportFile.readAll()).object();
    QVERIFY(report["ok"].toBool());
    auto state = report["final"].toObject()["state"].toObject();
    QCOMPARE(
        state["gameplay"].toObject()["inventory"].toObject()["flower"].toInt(),
        1);
    QCOMPARE(state["attributes"].toObject()["money"].toInt(), 20);
    QCOMPARE(state["gameplay"]
                 .toObject()["inventory"]
                 .toObject()["training-sword"]
                 .toInt(),
             0);
    for (auto step : report["steps"].toArray())
      if (step.toObject()["command"].toObject()["checkpoint"] ==
          "changed-after-save")
        QCOMPARE(step.toObject()["ui"]
                     .toObject()["state"]
                     .toObject()["attributes"]
                     .toObject()["money"]
                     .toInt(),
                 10);
    QVERIFY(w.findChild<QPlainTextEdit *>("executionLog")
                ->toPlainText()
                .contains("flower"));
    w.grab().save(evidence + "/results.png");
    w.openProject(project);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QCOMPARE(findButton()["locationId"].toString(), QString("market"));
    QCOMPARE(findButton()["x"].toInt(), savedX);
  }
};
#include "facilities-test.inc"
#include "rules-test.inc"
#include "rules-migration-test.inc"
#include "logic-test.inc"
#include "data-ui-test.inc"
#include "timeline-test.inc"
#include "battle-hud-test.inc"
#include "workbench-test.inc"
#include "route-tree-test.inc"
#include "hierarchy-lock-test.inc"
#include "component-hierarchy-test.inc"
#include "pm2-facilities-test.inc"
#include "project-settings-test.inc"
#include "roster-test.inc"
#include "character-state-test.inc"
#include "time-session-test.inc"
#include "authoring-workspace-test.inc"
#include "ui-composition-test.inc"
#include "tactics-test.inc"
#include "direction-test.inc"
#include "assurance-test.inc"
#include "commercial-pilot-test.inc"
#include "goal-routes-test.inc"
#include "goal-search-test.inc"
#include "goal-compare-test.inc"
#include "goal-history-test.inc"
#include "goal-impact-test.inc"
#include "goal-partial-test.inc"
#include "goal-review-graph-test.inc"
#include "goal-ci-test.inc"
#include "screen-settings-test.inc"
#include "user-fields-test.inc"
#include "text-binding-batch-test.inc"
#include "duplicate-element-test.inc"
#include "visibility-condition-test.inc"
#include "hierarchy-search-test.inc"
#include "advance-button-test.inc"
#include "inspector-focus-test.inc"
#include "inspector-wheel-test.inc"
#include "content-activity-test.inc"
#include "content-character-test.inc"
#include "progression-authoring-test.inc"
#include "timeline-stage-test.inc"
#include "expedition-authoring-test.inc"
#include "script-authoring-test.inc"
#include "script-portrait-transform-test.inc"
#include "script-scene-preview-test.inc"
#include "export-dialog-test.inc"
#include "script-rounds-test.inc"
#include "plugin-test.inc"
#include "cutout-e2e-test.inc"
#include "project-manager-test.inc"
#include "designer-pilot-test.inc"
QTEST_MAIN(StudioTests)
#include "tests.moc"
