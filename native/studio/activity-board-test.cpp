#include "studio.hpp"
#include "editor-language.hpp"
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QJsonDocument>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidgetItemIterator>
#include <QWheelEvent>
namespace {
bool copyProject(QString from,QString to){QDirIterator it(from,QDir::Files|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);while(it.hasNext()){const auto source=it.next(),target=to+"/"+QDir(from).relativeFilePath(source);if(!QDir().mkpath(QFileInfo(target).absolutePath())||!QFile::copy(source,target))return false;}return true;}
QJsonObject readJson(QString path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return QJsonDocument::fromJson(f.readAll()).object();}
bool writeJson(QString path,QJsonObject value){QFile f(path);return f.open(QIODevice::WriteOnly|QIODevice::Truncate)&&f.write(QJsonDocument(value).toJson())>0;}
QJsonObject planner(QJsonObject p){for(auto s:p["visual"].toObject()["screens"].toArray())if(s.toObject()["id"]=="schedule")for(auto e:s.toObject()["elements"].toArray())if(e.toObject()["id"]=="schedule")return e.toObject();return {};}
void selectPlanner(StudioWindow& w){auto list=w.findChild<QListWidget*>("projectList");QVERIFY(list);for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole)=="schedule")QMetaObject::invokeMethod(list,"itemActivated",Q_ARG(QListWidgetItem*,list->item(i)));auto tree=w.findChild<QTreeWidget*>("hierarchy");QVERIFY(tree);for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole)=="schedule"){tree->clearSelection();tree->setCurrentItem(*it);(*it)->setSelected(true);break;}QCOMPARE(w.selectedId(),QString("schedule"));}
void setChoice(QComboBox* field,const QString& value){QVERIFY(field);const auto index=field->findData(value);QVERIFY2(index>=0,qPrintable(value));field->setCurrentIndex(index);}
void configure(QDialog* d){
 setChoice(d->findChild<QComboBox*>("widgetLayout"),"activity-board");QVERIFY(d->findChild<QCheckBox*>("showActivityRules")->isChecked());QVERIFY(d->findChild<QCheckBox*>("showActivityDescription")->isChecked());setChoice(d->findChild<QComboBox*>("widgetControlStyle"),"board-colors");setChoice(d->findChild<QComboBox*>("widget-entryFrameAssetId"),"board-art");
 d->findChild<QCheckBox*>("showPhaseForecast")->setChecked(true);d->findChild<QCheckBox*>("showActivityRules")->setChecked(true);d->findChild<QCheckBox*>("showActivityDescription")->setChecked(true);
 auto table=d->findChild<QTableWidget*>("scheduleGroups");auto list=d->findChild<QListWidget*>("widgetEntries");
 for(const auto& id:{QString("study"),QString("rest")}){d->findChild<QPushButton*>("scheduleGroupAdd")->click();const int row=table->rowCount()-1;table->item(row,0)->setText(row==0?"all":"rest");table->item(row,1)->setText(row==0?"Practice":"Rest");for(int i=0;i<list->count();++i)list->item(i)->setCheckState(list->item(i)->data(Qt::UserRole)==id?Qt::Checked:Qt::Unchecked);setChoice(qobject_cast<QComboBox*>(table->cellWidget(row,2)),"board-art");}
 QTimer::singleShot(50,d,[&]{auto icons=d->findChild<QDialog*>("scheduleIconsDialog");QVERIFY(icons);QTimer::singleShot(10000,icons,&QDialog::reject);setChoice(icons->findChild<QComboBox*>("scheduleIcon-study"),"board-art");icons->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();});d->findChild<QPushButton*>("scheduleIconsButton")->click();
}
}
class ActivityBoardTests:public QObject {
 Q_OBJECT
private slots:
 void initTestCase(){QVERIFY(installEditorLanguage(*qApp,qEnvironmentVariable("RAISING_TEST_EDITOR_LOCALE","en")));QDir().mkpath(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath())+"/.cache/activity-board/qt");}
 void explicitDetailPreferencesSurviveConversion(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());QTemporaryDir temp(engine+"/.cache/qt-board-preferences-XXXXXX");QVERIFY(temp.isValid());QVERIFY(copyProject(engine+"/examples/my-first-raising-game",temp.path()));auto project=readJson(temp.path()+"/project.json"),visual=project["visual"].toObject();auto screens=visual["screens"].toArray();for(int i=0;i<screens.size();++i){auto screen=screens[i].toObject();auto elements=screen["elements"].toArray();for(int j=0;j<elements.size();++j){auto element=elements[j].toObject();if(element["kind"]!="schedule")continue;element["showActivityRules"]=false;element["showActivityDescription"]=false;elements[j]=element;}screen["elements"]=elements;screens[i]=screen;}visual["screens"]=screens;project["visual"]=visual;QVERIFY(writeJson(temp.path()+"/project.json",project));
  StudioWindow w(engine);w.show();w.openProject(temp.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);selectPlanner(w);bool applied=false;
  QTimer::singleShot(100,&w,[&]{auto d=w.findChild<QDialog*>("widgetSettingsDialog");QVERIFY(d);QTimer::singleShot(15000,d,&QDialog::reject);setChoice(d->findChild<QComboBox*>("widgetLayout"),"activity-board");QVERIFY(!d->findChild<QCheckBox*>("showActivityRules")->isChecked());QVERIFY(!d->findChild<QCheckBox*>("showActivityDescription")->isChecked());d->findChild<QDialogButtonBox*>("widgetSettingsButtons")->button(QDialogButtonBox::Ok)->click();applied=true;});w.findChild<QPushButton*>("widgetSettings")->click();QVERIFY(applied);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);auto board=planner(w.projectData());QVERIFY(board.contains("showActivityRules")&&!board["showActivityRules"].toBool());QVERIFY(board.contains("showActivityDescription")&&!board["showActivityDescription"].toBool());w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);w.openProject(temp.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);QCOMPARE(planner(w.projectData()),board);
 }
 void shortWindowRoundtrip(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());QTemporaryDir temp(engine+"/.cache/qt-activity-board-XXXXXX");QVERIFY(temp.isValid());QVERIFY(copyProject(engine+"/examples/my-first-raising-game",temp.path()));
  QImage image(24,24,QImage::Format_RGB32);image.fill(QColor("#bf7e44"));QVERIFY(image.save(temp.path()+"/assets/board.png"));auto manifest=readJson(temp.path()+"/assets/manifest.json");auto assets=manifest["assets"].toArray();assets.append(QJsonObject{{"id","board-art"},{"kind","image"},{"path","assets/board.png"}});manifest["assets"]=assets;QVERIFY(writeJson(temp.path()+"/assets/manifest.json",manifest));
  auto project=readJson(temp.path()+"/project.json"),visual=project["visual"].toObject();visual["styles"]=QJsonArray{QJsonObject{{"id","board-colors"},{"label","Journal colors"},{"fill","#f4ead6"},{"color","#243f36"}}};project["visual"]=visual;QVERIFY(writeJson(temp.path()+"/project.json",project));
  StudioWindow w(engine);w.resize(1100,700);w.show();w.openProject(temp.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);selectPlanner(w);const auto before=w.projectData();const auto revision=w.currentView()["revision"];
  for(bool apply:{false,true}){
   bool ran=false;QTimer::singleShot(100,&w,[&,apply]{auto d=w.findChild<QDialog*>("widgetSettingsDialog");QVERIFY(d);QTimer::singleShot(15000,d,&QDialog::reject);configure(d);d->resize(720,480);QTest::qWait(50);QCOMPARE(d->size(),QSize(720,480));
    auto scroll=d->findChild<QScrollArea*>("widgetSettingsScroll");auto buttons=d->findChild<QDialogButtonBox*>("widgetSettingsButtons");QVERIFY(scroll&&buttons&&!scroll->isAncestorOf(buttons));auto bar=scroll->verticalScrollBar();QVERIFY(bar->maximum()>0);const QRect footer(buttons->mapTo(d,QPoint()),buttons->size());QVERIFY(d->rect().contains(footer));QVERIFY(QRect(scroll->mapTo(d,QPoint()),scroll->size()).bottom()<footer.top());
    auto mode=d->findChild<QComboBox*>("widgetLayout");d->activateWindow();d->raise();QTest::qWait(30);mode->setFocus(Qt::TabFocusReason);scroll->ensureWidgetVisible(mode);QTest::qWait(30);auto last=d->findChild<QListWidget*>("widgetEntries");for(int i=0;i<90&&!last->hasFocus();++i){QVERIFY(qApp->focusWidget());QTest::keyClick(qApp->focusWidget(),Qt::Key_Tab);QTest::qWait(5);}QVERIFY(last->hasFocus());QVERIFY(bar->value()>0);
    bar->setValue(0);const auto center=scroll->viewport()->rect().center();QWheelEvent wheel(center,scroll->viewport()->mapToGlobal(center),QPoint(),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);QCoreApplication::sendEvent(scroll->viewport(),&wheel);QVERIFY(bar->value()>0);QTest::keyClick(bar,Qt::Key_End);QCOMPARE(bar->value(),bar->maximum());QCOMPARE(QRect(buttons->mapTo(d,QPoint()),buttons->size()),footer);
    for(auto button:buttons->buttons()){const QRect bounds(button->mapTo(d,QPoint()),button->size());QVERIFY(d->rect().contains(bounds));QCOMPARE(d->childAt(bounds.center()),button);}
    QVERIFY(d->grab().save(engine+"/.cache/activity-board/qt/settings-bottom-"+editorLanguage()+".png"));
    if(apply){auto table=d->findChild<QTableWidget*>("scheduleGroups");table->item(1,0)->setText("all");QTest::mouseClick(buttons->button(QDialogButtonBox::Ok),Qt::LeftButton);QVERIFY(d->isVisible());QVERIFY(!d->findChild<QLabel*>("widgetSettingsError")->text().isEmpty());QCOMPARE(w.projectData(),before);table->item(1,0)->setText("rest");QTest::mouseClick(buttons->button(QDialogButtonBox::Ok),Qt::LeftButton);}else QTest::mouseClick(buttons->button(QDialogButtonBox::Cancel),Qt::LeftButton);ran=true;
   });w.findChild<QPushButton*>("widgetSettings")->click();QVERIFY(ran);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);if(!apply){QCOMPARE(w.projectData(),before);QCOMPARE(w.currentView()["revision"],revision);}
  }
  QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));const auto after=w.projectData(),board=planner(after);QCOMPARE(board["scheduleLayout"].toString(),QString("activity-board"));QCOMPARE(board["controlStyleId"].toString(),QString("board-colors"));QCOMPARE(board["entryFrameAssetId"].toString(),QString("board-art"));QCOMPARE(board["activityIcons"].toArray().size(),1);QCOMPARE(board["scheduleGroups"].toArray().size(),2);for(const auto& g:board["scheduleGroups"].toArray())QCOMPARE(g.toObject()["iconAssetId"].toString(),QString("board-art"));QVERIFY(board["showPhaseForecast"].toBool());QVERIFY(board["showActivityRules"].toBool());QVERIFY(board["showActivityDescription"].toBool());QCOMPARE(board["width"],planner(before)["width"]);QCOMPARE(board["height"],planner(before)["height"]);
  QVERIFY(w.findChild<ScreenCanvas*>()->grab().save(engine+"/.cache/activity-board/qt/canvas-"+editorLanguage()+".png"));
  w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),before);w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),after);w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);w.openProject(temp.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);QCOMPARE(w.projectData(),after);selectPlanner(w);
  bool reopened=false;QTimer::singleShot(100,&w,[&]{auto d=w.findChild<QDialog*>("widgetSettingsDialog");QVERIFY(d);QTimer::singleShot(15000,d,&QDialog::reject);QCOMPARE(d->findChild<QComboBox*>("widgetLayout")->currentData().toString(),QString("activity-board"));QCOMPARE(d->findChild<QComboBox*>("widgetControlStyle")->currentData().toString(),QString("board-colors"));auto groups=d->findChild<QTableWidget*>("scheduleGroups");QCOMPARE(groups->rowCount(),2);QCOMPARE(qobject_cast<QComboBox*>(groups->cellWidget(0,2))->currentData().toString(),QString("board-art"));d->resize(720,480);QTest::qWait(20);QVERIFY(d->grab().save(engine+"/.cache/activity-board/qt/settings-top-"+editorLanguage()+".png"));reopened=true;d->reject();});w.findChild<QPushButton*>("widgetSettings")->click();QVERIFY(reopened);QCOMPARE(w.projectData(),after);
  w.findChild<QAction*>("validateProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);for(const auto& d:w.currentView()["diagnostics"].toArray())QVERIFY2(d.toObject()["severity"]!="error",qPrintable(d.toObject()["message"].toString()));
 }
};
QTEST_MAIN(ActivityBoardTests)
#include "activity-board-test.moc"
