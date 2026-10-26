#include "studio.hpp"
#include "renderer-preview.hpp"
#include "editor-language.hpp"
#include "theme.hpp"
#include <QDirIterator>
#include <QDialog>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {
bool copyFixture(QString source,QString target){QDirIterator it(source,QDir::Files|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);while(it.hasNext()){auto file=it.next(),dest=target+"/"+QDir(source).relativeFilePath(file);if(!QDir().mkpath(QFileInfo(dest).absolutePath())||!QFile::copy(file,dest))return false;}return true;}
}
class LayoutDiagnosticsTests:public QObject {
 Q_OBJECT
private slots:
 void initTestCase(){applyStudioTheme(*qApp);QVERIFY(installEditorLanguage(*qApp,qEnvironmentVariable("RAISING_TEST_EDITOR_LOCALE","en")));}
 void presetsAndPreparationCancellation(){
  RendererPreviewDock dock;dock.setContext({{"visual",QJsonObject{{"screens",QJsonArray{QJsonObject{{"id","title"}}}}}}},"title",{},1,"/fixture",{});
  auto preset=dock.findChild<QComboBox*>("rendererLayoutPreset");QVERIFY(preset);preset->setCurrentIndex(3);QMetaObject::invokeMethod(preset,"activated",Q_ARG(int,3));
  QCOMPARE(dock.findChild<QSpinBox*>("rendererWidth")->value(),390);QCOMPARE(dock.findChild<QSpinBox*>("rendererHeight")->value(),844);QCOMPARE(dock.findChild<QComboBox*>("rendererTextScale")->currentData().toDouble(),2.);
  QSignalSpy requested(&dock,&RendererPreviewDock::snapshotRequested),ready(&dock,&RendererPreviewDock::snapshotReady);dock.begin();QCOMPARE(requested.count(),1);QVERIFY(dock.running());auto cancel=dock.findChild<QPushButton*>("rendererCancel");QVERIFY(cancel&&cancel->isEnabled());cancel->click();QVERIFY(!dock.running());QCOMPARE(ready.count(),1);QVERIFY(!ready.first().first().toBool());
 }
 void legacyRendererShowsDiagnosticsUnavailable(){
  QTemporaryDir temp;QVERIFY(temp.isValid());RendererPreviewDock dock;dock.setContext({{"visual",QJsonObject{{"screens",QJsonArray{QJsonObject{{"id","title"}}}}}}},"title",{},1,temp.path(),{});QSignalSpy ready(&dock,&RendererPreviewDock::snapshotReady);dock.begin();dock.launch({{"revision",1},{"directory",temp.path()},{"executable",QCoreApplication::applicationFilePath()},{"save",temp.path()+"/unused-save.json"},{"report",temp.path()+"/report.json"}});QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(),10000);QVERIFY2(ready.first().first().toBool(),qPrintable(dock.message()));QVERIFY(!dock.snapshotImage().isNull());QVERIFY(dock.result().value("layoutDiagnostics").isUndefined());QCOMPARE(dock.message(),QObject::tr("이 런타임에는 배치 진단이 없습니다. 런타임을 다시 빌드하세요."));QVERIFY(dock.findChild<QTreeWidget*>("rendererLayoutIssues")->topLevelItemCount()==0);
 }
 void renderedFindingsNavigateWithoutMutatingDraft(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()),evidence=engine+"/.cache/qt-layout-diagnostics";QDir().mkpath(evidence);QTemporaryDir temp(engine+"/.cache/qt-layout-fixture-XXXXXX");QVERIFY(temp.isValid());QVERIFY(copyFixture(engine+"/examples/my-first-raising-game",temp.path()));
  QFile file(temp.path()+"/project.json");QVERIFY(file.open(QIODevice::ReadOnly));auto project=QJsonDocument::fromJson(file.readAll()).object();file.close();auto visual=project.value("visual").toObject();auto screens=visual.value("screens").toArray();
  auto title=screens[0].toObject();title["elements"]=QJsonArray{QJsonObject{{"id","clipped"},{"kind","text"},{"x",20},{"y",50},{"width",140},{"height",24},{"fontSize",20},{"fill","#ffffff"},{"color","#dddddd"},{"text","Initial text"}},QJsonObject{{"id","tiny"},{"kind","button"},{"x",20},{"y",100},{"width",30},{"height",30},{"text","Go"},{"action","start"}}};title["readableLayout"]=QJsonObject{{"items",QJsonArray{QJsonObject{{"elementId","clipped"}}}},{"actions",QJsonArray{"tiny"}}};screens[0]=title;visual["screens"]=screens;visual["readableLayout"]=QJsonObject{};project["visual"]=visual;QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::Truncate));file.write(QJsonDocument(project).toJson());file.close();QVERIFY(file.open(QIODevice::ReadOnly));const auto disk=file.readAll();file.close();
  StudioWindow w(engine);w.resize(1280,900);w.show();w.openProject(temp.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  auto list=w.findChild<QListWidget*>("projectList");QListWidgetItem* titleItem=nullptr;for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole)=="title")titleItem=list->item(i);QVERIFY(titleItem);QMetaObject::invokeMethod(list,"itemActivated",Q_ARG(QListWidgetItem*,titleItem));
  auto hierarchy=w.findChild<QTreeWidget*>("hierarchy");QTreeWidgetItem* clipped=nullptr;for(QTreeWidgetItemIterator it(hierarchy);*it;++it)if((*it)->data(0,Qt::UserRole)=="clipped")clipped=*it;QVERIFY(clipped);hierarchy->setCurrentItem(clipped);clipped->setSelected(true);QCOMPARE(w.selectedId(),QString("clipped"));
  auto text=w.findChild<QLineEdit*>("buttonText");QVERIFY(text);text->setFocus();text->selectAll();QTest::keyClicks(text,"Unsaved long text that must wrap well beyond this deliberately short text box.");QTest::keyClick(text,Qt::Key_Return);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&w.currentView().value("dirty").toBool(),10000);const auto before=w.currentView();
  bool discoverable=false;QTimer::singleShot(50,&w,[&]{auto dialog=w.findChild<QDialog*>("authoringToolSearch");QVERIFY(dialog);auto query=dialog->findChild<QLineEdit*>("authoringToolQuery");query->setText(editorLanguage()=="ko"?QString::fromUtf8("배치"):editorLanguage()=="ja"?QString::fromUtf8("配置"):QString("layout"));auto results=dialog->findChild<QListWidget*>("authoringToolResults");for(int i=0;i<results->count();++i)if(results->item(i)->data(Qt::UserRole+1)=="inspectRenderer")discoverable=true;dialog->reject();});w.findChild<QAction*>("findAuthoringTool")->trigger();QVERIFY(discoverable);
  auto dock=w.findChild<RendererPreviewDock*>("rendererPreviewDock");QVERIFY(dock);dock->setFloating(true);dock->resize(1100,900);dock->show();auto preset=dock->findChild<QComboBox*>("rendererLayoutPreset");preset->setCurrentIndex(1);QMetaObject::invokeMethod(preset,"activated",Q_ARG(int,1));
  QSignalSpy ready(dock,&RendererPreviewDock::snapshotReady);w.findChild<QAction*>("inspectRenderer")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(),60000);QVERIFY2(ready.last().first().toBool(),qPrintable(dock->message()+"\n"+QString::fromUtf8(QJsonDocument(w.currentView().value("diagnostics").toArray()).toJson())));auto diagnostics=dock->result().value("layoutDiagnostics").toObject();QCOMPARE(diagnostics.value("measurement").toString(),QString("rmlui-rendered-layout"));QVERIFY(dock->result().value("preview").toObject().value("isolated").toBool());
  auto issues=dock->findChild<QTreeWidget*>("rendererLayoutIssues");QVERIFY(issues);QTreeWidgetItem* tiny=nullptr;bool hasClip=false;for(int i=0;i<issues->topLevelItemCount();++i){auto row=issues->topLevelItem(i);auto issue=row->data(0,Qt::UserRole+1).toJsonObject();if(issue.value("code")=="CLIPPED_TEXT"&&issue.value("elementId")=="clipped")hasClip=true;if(issue.value("code")=="SMALL_TOUCH_TARGET"&&issue.value("elementId")=="tiny")tiny=row;}QVERIFY(hasClip);QVERIFY(tiny);QMetaObject::invokeMethod(issues,"itemActivated",Q_ARG(QTreeWidgetItem*,tiny),Q_ARG(int,0));QCOMPARE(w.selectedId(),QString("tiny"));
  for(const auto field:{"revision","dirty","undo","redo","draftVersion","diskVersion","documents"})QCOMPARE(w.currentView().value(field),before.value(field));QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),disk);file.close();
  QCOMPARE(issues->headerItem()->text(0),editorLanguage()=="ko"?QString::fromUtf8("요소"):editorLanguage()=="ja"?QString::fromUtf8("要素"):QString("Element"));QVERIFY(dock->findChild<QTreeWidget*>("previewReasons")->isHidden());QVERIFY(dock->grab().save(evidence+"/inspector-desktop-"+editorLanguage()+".png"));QFile::remove(evidence+"/report-desktop-"+editorLanguage()+".json");QVERIFY(QFile::copy(dock->reportPath(),evidence+"/report-desktop-"+editorLanguage()+".json"));
  QSignalSpy picked(dock,&RendererPreviewDock::elementPicked);dock->setContext(w.projectData(),"title","tiny",before.value("revision").toInt()+1,temp.path(),"en");QMetaObject::invokeMethod(issues,"itemActivated",Q_ARG(QTreeWidgetItem*,tiny),Q_ARG(int,0));QVERIFY(picked.isEmpty());QVERIFY(!dock->snapshotCurrent());dock->setContext(w.projectData(),"title","tiny",before.value("revision").toInt(),temp.path(),"en");
  preset->setCurrentIndex(3);QMetaObject::invokeMethod(preset,"activated",Q_ARG(int,3));ready.clear();dock->begin();QTRY_VERIFY_WITH_TIMEOUT(!ready.isEmpty(),60000);QVERIFY2(ready.last().first().toBool(),qPrintable(dock->message()+"\n"+QString::fromUtf8(QJsonDocument(w.currentView().value("diagnostics").toArray()).toJson())));const auto config=dock->result().value("layoutDiagnostics").toObject().value("configuration").toObject();QCOMPARE(config.value("windowWidth").toInt(),390);QCOMPARE(config.value("windowHeight").toInt(),844);QCOMPARE(config.value("textScale").toDouble(),2.);QVERIFY(config.value("readableActive").toBool());QVERIFY(dock->result().value("commands").toArray().isEmpty());QCOMPARE(w.currentView().value("documents"),before.value("documents"));QVERIFY(dock->grab().save(evidence+"/inspector-"+editorLanguage()+".png"));QFile::remove(evidence+"/report-"+editorLanguage()+".json");QVERIFY(QFile::copy(dock->reportPath(),evidence+"/report-"+editorLanguage()+".json"));
 }
};
int main(int argc,char** argv){
 QApplication app(argc,argv);const auto args=QCoreApplication::arguments();
 // A deliberately old renderer protocol fixture. It tests missing diagnostics,
 // not rendered-layout correctness; the actual player is exercised above.
 if(args.contains("--bundle")){
  const auto path=args.value(args.indexOf("--report")+1);QImage image(32,32,QImage::Format_ARGB32);image.fill(Qt::white);if(!image.save(QFileInfo(path).absolutePath()+"/preview.png"))return 2;
  QFile report(path);if(!report.open(QIODevice::WriteOnly))return 3;report.write(QJsonDocument(QJsonObject{{"ok",true},{"steps",QJsonArray{QJsonObject{{"command",QJsonObject{{"checkpoint","preview"}}},{"ui",QJsonObject{{"screenId","title"},{"nodes",QJsonArray{}}}}}}}}).toJson());return 0;
 }
 LayoutDiagnosticsTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "layout-diagnostics-test.moc"
