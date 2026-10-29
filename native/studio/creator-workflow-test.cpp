#include "studio.hpp"
#include "canvas-state.hpp"
#include "renderer-preview.hpp"
#include "creator-tasks.hpp"
#include "authoring-labels.hpp"
#include "editor-language.hpp"
#include "external-tool.hpp"
#include <QtTest>
#include <QtWidgets>

namespace {
bool copyProject(const QString& source,const QString& target){QDirIterator it(source,QDir::Files|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);while(it.hasNext()){const auto f=it.next(),to=target+"/"+QDir(source).relativeFilePath(f);if(!QDir().mkpath(QFileInfo(to).absolutePath())||!QFile::copy(f,to))return false;}return true;}
QString engineRoot(){return qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());}
void openScreen(StudioWindow& w,QString id){auto list=w.findChild<QListWidget*>("projectList");for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole)==id){QMetaObject::invokeMethod(list,"itemActivated",Q_ARG(QListWidgetItem*,list->item(i)));return;}}
}
class CreatorWorkflowTest:public QObject {
 Q_OBJECT
 QTemporaryDir settings;
private slots:
 void externalToolsKeepLiteralProjectPaths(){
  QTemporaryDir dir;QVERIFY(dir.isValid());const auto project=dir.path()+"/Game spaces ' quotes $(literal)";QVERIFY(QDir().mkpath(project));
  const auto app=dir.path()+"/Version Tool.app";QVERIFY(QDir().mkpath(app+"/Contents/MacOS"));
  QVERIFY(externalToolLaunch(app,project,ExternalToolPlatform::MacOS).program.isEmpty());
  QFile plist(app+"/Contents/Info.plist");QVERIFY(plist.open(QIODevice::WriteOnly));plist.write("<plist/>");plist.close();
  const auto mac=externalToolLaunch(app,project,ExternalToolPlatform::MacOS);QCOMPARE(mac.program,QString("/usr/bin/open"));QCOMPARE(mac.arguments,QStringList({"-a",app,project}));
  QVERIFY(externalToolLaunch(app,project,ExternalToolPlatform::Windows).program.isEmpty());
  QFile exe(dir.path()+"/Version Tool.EXE");QVERIFY(exe.open(QIODevice::WriteOnly));exe.close();
  const auto win=externalToolLaunch(exe.fileName(),project,ExternalToolPlatform::Windows);QCOMPARE(win.program,exe.fileName());QCOMPARE(win.arguments,QStringList{project});
  QFile command(dir.path()+"/tool");QVERIFY(command.open(QIODevice::WriteOnly));command.close();
  QVERIFY(externalToolLaunch(command.fileName(),project,ExternalToolPlatform::Windows).program.isEmpty());
#ifndef Q_OS_WIN
  QVERIFY(externalToolLaunch(command.fileName(),project,ExternalToolPlatform::Unix).program.isEmpty());
  QVERIFY(command.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
  const auto unixTool=externalToolLaunch(command.fileName(),project,ExternalToolPlatform::Unix);QCOMPARE(unixTool.program,command.fileName());QCOMPARE(unixTool.arguments,QStringList{project});
#endif
  QVERIFY(externalToolLaunch(dir.path()+"/missing",project).program.isEmpty());
 }
 void initTestCase(){QVERIFY(settings.isValid());qputenv("RAISING_EDITOR_SETTINGS",(settings.path()+"/settings.ini").toUtf8());qputenv("RAISING_PROJECT_HISTORY",(settings.path()+"/history.ini").toUtf8());QVERIFY(installEditorLanguage(*qApp,qEnvironmentVariable("RAISING_TEST_EDITOR_LOCALE","en")));}
 void namesDescribeBindingsAndDisambiguateDuplicates(){
  const QJsonObject calendar{{"id","calendar"},{"kind","text"},{"binding","calendar"}},plan{{"id","schedule"},{"kind","schedule"}},second{{"id","alternate-plan"},{"kind","schedule"}},named{{"id","advance"},{"kind","button"},{"text","Continue"}};
  QCOMPARE(authoringLabel(calendar),QObject::tr("달력"));QCOMPARE(authoringLabel(plan),QObject::tr("활동 계획"));QCOMPARE(authoringLabel(named),QString("Continue"));
  const QMap<QString,QJsonObject> siblings{{"schedule",plan},{"alternate-plan",second},{"calendar",calendar},{"advance",named}};
  QCOMPARE(authoringDistinctLabel(plan,siblings),QObject::tr("활동 계획")+" · schedule");QCOMPARE(authoringDistinctLabel(second,siblings),QObject::tr("활동 계획")+" · alternate-plan");QCOMPARE(authoringDistinctLabel(calendar,siblings),QObject::tr("달력"));QCOMPARE(authoringLabel(QJsonObject{{"id","untitled-screen"}}),QString("untitled-screen"));
 }
 void hierarchyNamesRemainSearchableWithoutChangingIdentifiers(){
  QTemporaryDir project;QVERIFY(project.isValid());QVERIFY(copyProject(engineRoot()+"/examples/my-first-raising-game",project.path()));
  QFile f(project.path()+"/project.json");QVERIFY(f.open(QIODevice::ReadOnly));auto data=QJsonDocument::fromJson(f.readAll()).object();f.close();auto visual=data["visual"].toObject();auto screens=visual["screens"].toArray();
  for(int i=0;i<screens.size();++i){auto screen=screens[i].toObject();if(screen["id"]!="schedule")continue;auto elements=screen["elements"].toArray();for(auto value:elements){auto element=value.toObject();if(element["id"]!="stats")continue;element["id"]="alternate-stats";elements.append(element);break;}screen["elements"]=elements;screens[i]=screen;}
  visual["screens"]=screens;data["visual"]=visual;QVERIFY(f.open(QIODevice::WriteOnly|QIODevice::Truncate));f.write(QJsonDocument(data).toJson());f.close();
  StudioWindow w(engineRoot());w.show();w.findChild<CanvasStateBar*>("canvasStateBar")->showAllElements();w.openProject(project.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),30000);openScreen(w,"schedule");
  const auto before=w.currentView();auto tree=w.findChild<QTreeWidget*>("hierarchy");auto search=w.findChild<QLineEdit*>("hierarchySearch");QVERIFY(tree&&search);
  auto find=[&](const QString& id){for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole)==id&&(*it)->data(0,Qt::UserRole+1)==QString("schedule"))return *it;return static_cast<QTreeWidgetItem*>(nullptr);};
  QVERIFY(find("calendar")&&find("stats")&&find("alternate-stats"));QCOMPARE(find("calendar")->text(0),QObject::tr("달력"));QVERIFY(find("calendar")->toolTip(0).endsWith(" · calendar"));QCOMPARE(find("calendar")->text(2),QObject::tr("글"));
  QCOMPARE(find("stats")->text(0),QObject::tr("능력치 목록")+" · stats");QCOMPARE(find("alternate-stats")->text(0),QObject::tr("능력치 목록")+" · alternate-stats");
  search->setText(QObject::tr("능력치 목록"));QVERIFY(!find("stats")->isHidden()&&!find("alternate-stats")->isHidden());QVERIFY(find("calendar")->isHidden());
  auto row=find("alternate-stats");tree->scrollToItem(row);QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(row).center());QCOMPARE(w.selectedId(),QString("alternate-stats"));
  search->setText("calendar");QVERIFY(!find("calendar")->isHidden());QVERIFY(find("stats")->isHidden());search->clear();
  for(auto key:{"documents","revision","dirty","undo","redo"})QCOMPARE(w.currentView()[key],before[key]);
 }
 void taskLauncherUsesLiveCommands(){
  QMainWindow w;w.show();bool ready=false;auto action=new QAction("A named screen tool",&w);action->setObjectName("readableLayout");auto search=new QAction("Find commands",&w);search->setObjectName("findAuthoringTool");QSignalSpy called(action,&QAction::triggered),searched(search,&QAction::triggered);
  QString context="Home · Title";showCreatorTasks(&w,[&]{return ready;},[&]{return context;});
  auto d=w.findChild<QDockWidget*>("creatorTasksDock");QVERIFY(d);QVERIFY(!QApplication::activeModalWidget());auto tabs=d->findChild<QTabWidget*>("creatorTaskTabs");QCOMPARE(tabs->count(),4);tabs->setCurrentIndex(1);
  auto button=d->findChild<QPushButton*>("creatorTask-readableLayout");QVERIFY(button&&!button->isEnabled());ready=true;QTRY_VERIFY(button->isEnabled());action->setEnabled(false);QVERIFY(!button->isEnabled());action->setEnabled(true);QVERIFY(button->isEnabled());QTest::mouseClick(button,Qt::LeftButton);QCOMPARE(called.count(),1);QVERIFY(d->isVisible());QCOMPARE(tabs->currentIndex(),1);
  QTest::mouseClick(d->findChild<QPushButton*>("creatorFindTools"),Qt::LeftButton);QCOMPARE(searched.count(),1);QCOMPARE(called.count(),1);
  context="Plan · Schedule";QTRY_COMPARE(d->findChild<QLabel*>("creatorTaskContext")->text(),context);d->close();showCreatorTasks(&w,[&]{return ready;});QCOMPARE(w.findChildren<QDockWidget*>("creatorTasksDock").size(),1);QVERIFY(d->isVisible());QCOMPARE(tabs->currentIndex(),1);QCOMPARE(called.count(),1);
  ready=false;QTRY_VERIFY(!button->isEnabled());QTest::mouseClick(button,Qt::LeftButton);QCOMPARE(called.count(),1);
 }
 void inspectorShowsApplicableFieldsWithoutEditing(){
  QTemporaryDir project;QVERIFY(copyProject(engineRoot()+"/examples/my-first-raising-game",project.path()));
  StudioWindow w(engineRoot());w.show();w.findChild<CanvasStateBar*>("canvasStateBar")->showAllElements();w.openProject(project.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),30000);openScreen(w,"schedule");const auto before=w.currentView();auto tree=w.findChild<QTreeWidget*>("hierarchy");
  auto select=[&](QString id){for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole)==id&&(*it)->data(0,Qt::UserRole+1)==QString("schedule")){tree->setCurrentItem(*it,0,QItemSelectionModel::ClearAndSelect);return;}QFAIL("Missing fixture element");};
  select("calendar");QVERIFY(!w.findChild<QComboBox*>("textBinding")->isHidden());QVERIFY(w.findChild<QComboBox*>("imageAsset")->isHidden());QVERIFY(w.findChild<QComboBox*>("bindingAttribute")->isHidden());QVERIFY(w.findChild<QWidget*>("elementActionGroup")->isHidden());
  select("advance");QVERIFY(w.findChild<QComboBox*>("textBinding")->isHidden());QVERIFY(!w.findChild<QWidget*>("elementActionGroup")->isHidden());select("calendar");
  for(auto key:{"documents","revision","dirty","undo","redo"})QCOMPARE(w.currentView()[key],before[key]);
 }
 void initialStateIsDefaultAndAllElementsIsExplicit(){
  QTemporaryDir project;QVERIFY(project.isValid());QVERIFY(copyProject(engineRoot()+"/examples/my-first-raising-game",project.path()));
  QFile f(project.path()+"/project.json");QVERIFY(f.open(QIODevice::ReadOnly));auto data=QJsonDocument::fromJson(f.readAll()).object();f.close();auto visual=data["visual"].toObject();auto screens=visual["screens"].toArray();
  for(int i=0;i<screens.size();++i){auto s=screens[i].toObject();if(s["id"]!="schedule")continue;auto es=s["elements"].toArray();for(auto phase:{QString("planning"),QString("result")})es.append(QJsonObject{{"id","proof-"+phase},{"kind","text"},{"text",phase},{"x",20},{"y",20},{"width",240},{"height",40},{"visibleWhen",phase}});s["elements"]=es;screens[i]=s;}
  visual["screens"]=screens;data["visual"]=visual;QVERIFY(f.open(QIODevice::WriteOnly|QIODevice::Truncate));f.write(QJsonDocument(data).toJson());f.close();
  StudioWindow w(engineRoot());w.resize(1280,800);w.show();w.openProject(project.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),30000);auto canvas=w.findChild<ScreenCanvas*>("screenCanvas");auto bar=w.findChild<CanvasStateBar*>("canvasStateBar");QVERIFY(canvas&&bar);QVERIFY(bar->findChild<QCheckBox*>("canvasStateEnabled")->isChecked());
  auto tabs=w.findChild<QTabBar*>("documentTabs");QCOMPARE(tabs->tabData(tabs->currentIndex()).toString(),QString("title"));openScreen(w,"schedule");const auto before=w.currentView();
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!bar->renderer()->running()&&canvas->hasRuntimePreview(),90000);const auto result=canvas->runtimeResult();const auto pixels=bar->renderer()->snapshotImage();QVERIFY(!pixels.isNull());bool planning=false,wrongResult=false;for(auto v:result["nodes"].toArray()){auto n=v.toObject();const bool shown=n["visible"].toBool(true)&&n["rendered"].toBool(true);if(n["id"]=="proof-planning")planning=shown;if(n["id"]=="proof-result")wrongResult=shown;}QVERIFY(planning);QVERIFY(!wrongResult);QVERIFY(result["commands"].toArray().isEmpty());
  for(auto key:{"documents","revision","dirty","undo","redo"})QCOMPARE(w.currentView()[key],before[key]);
  auto all=w.findChild<QAction*>("conditionPreviewClear");QVERIFY(all);all->trigger();QVERIFY(!canvas->hasRuntimePreview());QVERIFY(!bar->findChild<QCheckBox*>("canvasStateEnabled")->isChecked());
  bar->showGameState();QTRY_VERIFY_WITH_TIMEOUT(canvas->hasRuntimePreview()&&!w.busy(),90000);QCOMPARE(bar->renderer()->snapshotImage(),pixels);for(auto key:{"documents","revision","dirty","undo","redo"})QCOMPARE(w.currentView()[key],before[key]);
  const auto out=engineRoot()+"/.cache/creator-workflow";QVERIFY(QDir().mkpath(out));QVERIFY(w.grab().save(out+"/actual-state-"+editorLanguage()+".png"));
  bar->showAllElements();w.openProject(project.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),30000);QVERIFY(!bar->findChild<QCheckBox*>("canvasStateEnabled")->isChecked());
 }
 void playCommandIsNotSwallowedByPassiveSnapshot(){
  QMainWindow w;auto body=new QWidget;auto layout=new QVBoxLayout(body);auto canvas=new ScreenCanvas;auto bar=new CanvasStateBar(canvas,&w);layout->addWidget(bar);layout->addWidget(canvas);w.setCentralWidget(body);w.resize(1000,600);w.show();
  bool editing=false;bar->busy=[&]{return editing;};bar->snapshotBusy=[]{return true;};const QJsonObject project{{"visual",QJsonObject{{"width",1280},{"height",720},{"screens",QJsonArray{QJsonObject{{"id","title"},{"elements",QJsonArray{}}}}}}}};
  bar->updateContext(QJsonObject{{"root","isolated-play-command"},{"revision",8},{"documents",QJsonArray{QJsonObject{{"kind","project"},{"data",project}}}}},"title",{},QSize(640,360));bar->renderer()->begin();QVERIFY(bar->renderer()->running());QSignalSpy played(bar,&CanvasStateBar::playRequested);auto play=bar->findChild<QPushButton*>("canvasPlay");QTest::mouseClick(play,Qt::LeftButton);QCOMPARE(played.count(),1);const auto request=played.first().first().toJsonObject();QCOMPARE(request["revision"].toInt(),8);QVERIFY(request["interactive"].toBool());QCOMPARE(request["authoringContext"].toObject()["screenId"].toString(),QString("title"));
  const auto status=bar->findChild<QLabel*>("canvasStateStatus")->text();bar->renderer()->fail("delayed snapshot failure");QCOMPARE(bar->findChild<QLabel*>("canvasStateStatus")->text(),status);play->click();QCOMPARE(played.count(),1);bar->failPlay("request boundary verified");editing=true;play->click();QCOMPARE(played.count(),1);
 }
 void automaticSnapshotDoesNotDropAcceptedDialogEdits(){
  QTemporaryDir project;QVERIFY(project.isValid());QVERIFY(copyProject(engineRoot()+"/examples/my-first-raising-game",project.path()));
  const auto options=qgetenv("NODE_OPTIONS");qputenv("NODE_OPTIONS",options+" --import="+QUrl::fromLocalFile(engineRoot()+"/tests/qt-studio/delay-canvas-preview.mjs").toEncoded());
  StudioWindow w(engineRoot());qputenv("NODE_OPTIONS",options);w.show();auto bar=w.findChild<CanvasStateBar*>("canvasStateBar");QVERIFY(bar);QSignalSpy started(bar,&CanvasStateBar::snapshotRequested);bool staleRejected=false;connect(bar->renderer(),&RendererPreviewDock::snapshotReady,&w,[&](bool ok){if(!ok&&bar->renderer()->message()==QObject::tr("준비 중 프로젝트 버전이 변경되었습니다. 다시 렌더링하세요."))staleRejected=true;});w.openProject(project.path());
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),30000);QTRY_VERIFY_WITH_TIMEOUT(!started.isEmpty(),5000);QVERIFY(bar->renderer()->running());QVERIFY(!w.busy());
  const auto before=w.projectData();const auto revision=w.currentView()["revision"].toInt();bool saved=false;
  QTimer::singleShot(30,&w,[&]{auto d=w.findChild<QDialog*>("projectSettingsDialog");QVERIFY(d);QTimer::singleShot(5000,d,&QDialog::reject);d->findChild<QLineEdit*>("projectTitle")->setText("Saved while canvas refreshes");d->findChild<QDialogButtonBox*>("projectSettingsButtons")->button(QDialogButtonBox::Ok)->click();saved=true;});w.findChild<QAction*>("projectSettings")->trigger();QVERIFY(saved);QVERIFY(w.busy());
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),30000);QCOMPARE(w.projectData()["title"].toString(),QString("Saved while canvas refreshes"));QCOMPARE(w.currentView()["revision"].toInt(),revision+1);w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),30000);QCOMPARE(w.projectData(),before);
  const auto afterUndo=w.currentView();auto dock=w.findChild<RendererPreviewDock*>("rendererPreviewDock");QSignalSpy inspected(dock,&RendererPreviewDock::snapshotReady);dock->begin();QTRY_VERIFY_WITH_TIMEOUT(!inspected.isEmpty(),30000);QVERIFY2(inspected.last().first().toBool(),qPrintable(dock->message()));
  QTRY_VERIFY_WITH_TIMEOUT(staleRejected,10000);QTRY_VERIFY_WITH_TIMEOUT(!bar->renderer()->running()&&bar->renderer()->snapshotCurrent(),30000);for(auto key:{"documents","revision","dirty","undo","redo"})QCOMPARE(w.currentView()[key],afterUndo[key]);
 }
 void readingBasicsUseNamesAndDisclosureDoesNotEdit(){
  QTemporaryDir project;QVERIFY(project.isValid());QVERIFY(copyProject(engineRoot()+"/examples/my-first-raising-game",project.path()));
  StudioWindow w(engineRoot());w.show();w.openProject(project.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),30000);w.findChild<CanvasStateBar*>("canvasStateBar")->showAllElements();const auto before=w.projectData();const auto revision=w.currentView()["revision"];bool visited=false;
  QTimer::singleShot(100,&w,[&]{auto d=w.findChild<QDialog*>("readableLayoutDialog");QVERIFY(d);QTimer::singleShot(15000,d,&QDialog::reject);auto toggle=d->findChild<QToolButton*>("readableAdvancedToggle");auto advanced=d->findChild<QWidget*>("readableAdvancedSettings");QVERIFY(toggle&&advanced&&!toggle->isChecked()&&!advanced->isVisible());auto screen=d->findChild<QComboBox*>("readableScreen");screen->setCurrentIndex(screen->findData("schedule"));auto actions=d->findChild<QListWidget*>("readableActions");auto primary=d->findChild<QComboBox*>("readablePrimaryAction");QVERIFY(actions&&actions->count()>0);for(int i=0;i<actions->count();++i){auto row=actions->item(i);const auto id=row->data(Qt::UserRole).toJsonObject()["elementId"].toString();QCOMPARE(row->toolTip(),id);const int option=primary->findData(id);QVERIFY(option>=0);QCOMPARE(primary->itemData(option,Qt::ToolTipRole).toString(),id);QVERIFY(!primary->itemText(option).isEmpty());if(id=="advance")QCOMPARE(primary->itemText(option),QString("Continue"));}
   QTest::mouseClick(toggle,Qt::LeftButton);QVERIFY(advanced->isVisible());QTest::mouseClick(toggle,Qt::LeftButton);QVERIFY(!advanced->isVisible());d->resize(940,600);QTest::qWait(30);auto buttons=d->findChild<QDialogButtonBox*>("readableButtons");QVERIFY(d->rect().contains(QRect(buttons->mapTo(d,QPoint()),buttons->size())));const auto out=engineRoot()+"/.cache/creator-workflow";QVERIFY(QDir().mkpath(out));QVERIFY(d->grab().save(out+"/reading-basics-"+editorLanguage()+".png"));QTest::mouseClick(buttons->button(QDialogButtonBox::Save),Qt::LeftButton);visited=true;
  });w.findChild<QAction*>("readableLayout")->trigger();QVERIFY(visited);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),before);QCOMPARE(w.currentView()["revision"],revision);
 }
};
QTEST_MAIN(CreatorWorkflowTest)
#include "creator-workflow-test.moc"
