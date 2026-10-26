#include "studio.hpp"
#include "editor-language.hpp"
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QJsonDocument>
#include <QPushButton>
#include <QScrollArea>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidgetItemIterator>
namespace {
bool copyProject(QString from,QString to){QDirIterator it(from,QDir::Files|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);while(it.hasNext()){auto source=it.next(),target=to+"/"+QDir(from).relativeFilePath(source);if(!QDir().mkpath(QFileInfo(target).absolutePath())||!QFile::copy(source,target))return false;}return true;}
QJsonObject read(QString p){QFile f(p);if(!f.open(QIODevice::ReadOnly))return {};return QJsonDocument::fromJson(f.readAll()).object();}
QJsonObject element(QJsonObject p,QString id){for(auto s:p["visual"].toObject()["screens"].toArray())for(auto e:s.toObject()["elements"].toArray())if(e.toObject()["id"]==id)return e.toObject();return {};}
void select(StudioWindow& w,QString id){auto list=w.findChild<QListWidget*>("projectList");for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole)=="schedule")QMetaObject::invokeMethod(list,"itemActivated",Q_ARG(QListWidgetItem*,list->item(i)));auto tree=w.findChild<QTreeWidget*>("hierarchy");for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole)==id){tree->clearSelection();tree->setCurrentItem(*it);(*it)->setSelected(true);break;}QCOMPARE(w.selectedId(),id);}
}
class GrowthAuthoringTests:public QObject {
Q_OBJECT
private slots:
 void initTestCase(){QVERIFY(installEditorLanguage(*qApp,qEnvironmentVariable("RAISING_TEST_EDITOR_LOCALE","en")));}
 void roundtripGrowthAndPlanner(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());QDir().mkpath(engine+"/.cache/growth-summary/qt");QTemporaryDir temp(engine+"/.cache/qt-growth-XXXXXX");QVERIFY(temp.isValid());QVERIFY(copyProject(engine+"/examples/my-first-raising-game",temp.path()));auto p=read(temp.path()+"/project.json"),v=p["visual"].toObject();auto screens=v["screens"].toArray();
  for(int i=0;i<screens.size();++i){auto screen=screens[i].toObject();if(screen["id"]!="schedule")continue;auto elements=screen["elements"].toArray();for(int j=0;j<elements.size();++j){auto e=elements[j].toObject();if(e["kind"]=="schedule"){e["scheduleLayout"]="activity-board";e["showPhaseForecast"]=true;e["showActivityRules"]=true;e["scheduleGroups"]=QJsonArray{QJsonObject{{"id","learn"},{"label","Learning"},{"activityIds",QJsonArray{"study"}}}};elements[j]=e;}}
   elements.append(QJsonObject{{"id","planner-entry"},{"kind","button"},{"x",10},{"y",10},{"width",100},{"height",50},{"text","Open plan"},{"action","navigate"},{"screenId","schedule"}});screen["elements"]=elements;screens[i]=screen;}
  v["screens"]=screens;v["routing"]=QJsonObject{{"defaultScreenId","schedule"},{"phases",QJsonArray{}}};p["visual"]=v;QFile f(temp.path()+"/project.json");QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(p).toJson());f.close();
  StudioWindow w(engine);w.resize(1050,680);w.show();w.openProject(temp.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);const auto before=w.projectData();
  auto edit=[&](QString id,std::function<void(QDialog*)> callback,bool apply=true){select(w,id);bool ran=false;QTimer::singleShot(80,&w,[&]{auto d=w.findChild<QDialog*>("widgetSettingsDialog");QVERIFY(d);QTimer::singleShot(15000,d,&QDialog::reject);callback(d);d->resize(720,480);QTest::qWait(20);QCOMPARE(d->size(),QSize(720,480));auto scroll=d->findChild<QScrollArea*>("widgetSettingsScroll");auto buttons=d->findChild<QDialogButtonBox*>("widgetSettingsButtons");QVERIFY(!scroll->isAncestorOf(buttons));QVERIFY(d->rect().contains(QRect(buttons->mapTo(d,QPoint()),buttons->size())));QVERIFY(d->grab().save(engine+"/.cache/growth-summary/qt/"+id+"-"+editorLanguage()+".png"));buttons->button(apply?QDialogButtonBox::Ok:QDialogButtonBox::Cancel)->click();ran=true;});w.findChild<QPushButton*>("widgetSettings")->click();QVERIFY(ran);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));};
  auto growth=[](QDialog* d){auto mode=d->findChild<QComboBox*>("widgetLayout");QVERIFY(mode);QVERIFY(mode->findData("growth")>=0);mode->setCurrentIndex(mode->findData("growth"));};
  edit("result",growth,false);QCOMPARE(w.projectData(),before);edit("result",growth);QCOMPARE(element(w.projectData(),"result")["resultLayout"].toString(),QString("growth"));
  w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),before);w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  edit("schedule",[](QDialog* d){for(auto name:{"boardCompact","boardSummary","boardAdvance","boardInlineForecast"}){auto check=d->findChild<QCheckBox*>(name);QVERIFY(check);check->setChecked(true);}auto list=d->findChild<QListWidget*>("boardSummaryAttributes");QVERIFY(list&&list->count());list->item(0)->setCheckState(Qt::Checked);});
  edit("planner-entry",[](QDialog* d){d->findChild<QCheckBox*>("openPlannerEnabled")->setChecked(true);auto group=d->findChild<QComboBox*>("openPlannerGroup");QVERIFY(group->findData("learn")>=0);group->setCurrentIndex(group->findData("learn"));QCOMPARE(group->currentData().metaType(),QMetaType::fromType<QString>());QMetaObject::invokeMethod(group,"activated",Q_ARG(int,group->currentIndex()));auto activity=d->findChild<QComboBox*>("openPlannerActivity");QVERIFY(activity->findData("study")>=0);activity->setCurrentIndex(activity->findData("study"));QCOMPARE(activity->currentData().metaType(),QMetaType::fromType<QString>());});
  auto entry=element(w.projectData(),"planner-entry");QCOMPARE(entry["action"].toString(),QString("openPlanner"));QCOMPARE(entry["plannerElementId"].toString(),QString("schedule"));QCOMPARE(entry["plannerActivityId"].toString(),QString("study"));QVERIFY(element(w.projectData(),"schedule")["activityBoard"].toObject()["advanceAfterApply"].toBool());
  bool configured=false;QTimer::singleShot(80,&w,[&]{auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(d);QTimer::singleShot(15000,d,&QDialog::reject);auto check=d->findChild<QCheckBox*>("projectRevealActivityResult");QVERIFY(check);check->setChecked(true);d->findChild<QDialogButtonBox*>("projectSettingsButtons")->button(QDialogButtonBox::Ok)->click();configured=true;});w.findChild<QAction*>("projectSettings")->trigger();QVERIFY(configured);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);QCOMPARE(w.projectData()["visual"].toObject()["activityTimelineCompletion"].toString(),QString("reveal-result"));
  const auto after=w.projectData();w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);w.openProject(temp.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);QCOMPARE(w.projectData(),after);select(w,"result");QVERIFY(w.findChild<ScreenCanvas*>()->grab().save(engine+"/.cache/growth-summary/qt/canvas-"+editorLanguage()+".png"));
 }
};
QTEST_MAIN(GrowthAuthoringTests)
#include "growth-authoring-test.moc"
