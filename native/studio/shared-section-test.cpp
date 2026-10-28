#include "studio.hpp"
#include "shared-section.hpp"
#include "components.hpp"
#include "editor-language.hpp"
#include "project-manager.hpp"
#include <QtTest>
#include <QtWidgets>

namespace {
QJsonObject row(const QJsonArray& rows,QString id){for(auto v:rows)if(v.toObject().value("id")==id)return v.toObject();return {};}
QJsonObject screen(const QJsonObject& p,QString id){return row(p.value("visual").toObject().value("screens").toArray(),id);}
QJsonObject element(QString id,QString kind){return {{"id",id},{"kind",kind},{"x",0},{"y",0},{"width",200},{"height",44}};}
QJsonObject simple(){
 auto group=element("hud","group");group["x"]=20;group["y"]=16;group["width"]=600;group["height"]=100;group["anchorX"]="stretch";group["visibleCondition"]=QJsonObject{{"op","always"}};
 auto text=element("calendar","text");text["parentId"]="hud";text["binding"]="calendar";text["text"]="Calendar";text["styleId"]="ink";
 auto save=element("save","button");save["parentId"]="hud";save["x"]=220;save["action"]="save";save["text"]="Save";save["enabledCondition"]=QJsonObject{{"op","always"}};
 auto reading=QJsonObject{{"items",QJsonArray{QJsonObject{{"elementId","calendar"},{"fontSize",22}}}},{"actions",QJsonArray{"save"}},{"actionLayout","grouped"},{"primaryAction","save"}};
 return {{"visual",QJsonObject{{"width",1280},{"height",720},{"readableLayout",QJsonObject{}},{"styles",QJsonArray{QJsonObject{{"id","ink"},{"color","#123456"}}}},{"resumeContext",QJsonObject{{"screenId","home"},{"elementIds",QJsonArray{"calendar"}},{"showAfterLoad",true}}},{"screens",QJsonArray{QJsonObject{{"id","home"},{"elements",QJsonArray{group,text,save}},{"readableLayout",reading}},QJsonObject{{"id","status"},{"elements",QJsonArray{element("shared-section-1-1","panel")}},{"readableLayout",QJsonObject{{"items",QJsonArray{}}}}},QJsonObject{{"id","plain"},{"elements",QJsonArray{}}}}}}},{"localization",QJsonObject{{"entries",QJsonArray{QJsonObject{{"key","/project/visual/screens/@home/elements/@save/text"},{"source","Save"},{"translations",QJsonObject{{"ja","保存"}}}}}}}}};
}
bool copyFixture(QString source,QString target){QDirIterator it(source,QDir::Files|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);while(it.hasNext()){auto path=it.next();if(path.contains("/.studio/"))continue;auto out=target+"/"+QDir(source).relativeFilePath(path);if(!QDir().mkpath(QFileInfo(out).absolutePath())||!QFile::copy(path,out))return false;}return true;}
void openScreen(StudioWindow& w,QString id){auto list=w.findChild<QListWidget*>("projectList");QVERIFY(list);QListWidgetItem* target=nullptr;for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole)==id)target=list->item(i);QVERIFY(target);QVERIFY(QMetaObject::invokeMethod(list,"itemActivated",Q_ARG(QListWidgetItem*,target)));}
void select(StudioWindow& w,QString screenId,QString id){auto tree=w.findChild<QTreeWidget*>("hierarchy");QVERIFY(tree);tree->clearSelection();QTreeWidgetItem* target=nullptr;for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole)==id&&(*it)->data(0,Qt::UserRole+1)==screenId)target=*it;QVERIFY(target);tree->setCurrentItem(target);target->setSelected(true);QCOMPARE(w.selectedId(),id);}
void target(QDialog* d,QString id){auto table=d->findChild<QTableWidget*>("sharedSectionTargets");QVERIFY(table);for(int i=0;i<table->rowCount();++i)if(table->item(i,0)->data(Qt::UserRole)==id){table->item(i,0)->setCheckState(Qt::Checked);return;}QFAIL(qPrintable(id));}
}
class SharedSectionTests:public QObject {
 Q_OBJECT
 QTemporaryDir preferences;
private slots:
 void initTestCase(){
  QVERIFY(preferences.isValid());
  qputenv("RAISING_EDITOR_SETTINGS",(preferences.path()+"/settings.ini").toUtf8());
  qputenv("RAISING_PROJECT_HISTORY",(preferences.path()+"/history.ini").toUtf8());
  QVERIFY(ProjectManager::recentProjects().isEmpty());
  QVERIFY(installEditorLanguage(*qApp,qEnvironmentVariable("RAISING_TEST_EDITOR_LOCALE","en")));
 }
 void preservesGeometryBindingsConditionsReadingAndTranslations(){
  const auto original=simple();SharedSection::Source source;QString error;QVERIFY2(SharedSection::prepare(original,"home",{"hud"},source,error),qPrintable(error));QVERIFY(source.convert);QCOMPARE(source.selected.size(),3);
  SharedSection::Review result;QVERIFY2(SharedSection::apply(original,source,"Common HUD",{{"home",true},{"status",true},{"plain",false}},result,error),qPrintable(error));QCOMPARE(result.added,3);QCOMPARE(original,simple());
  const auto visual=result.project.value("visual").toObject();const auto definition=visual.value("components").toArray().first().toObject();QCOMPARE(definition.value("elements"),screen(original,"home").value("elements"));
  const auto sourceId=result.sourceInstanceId;QCOMPARE(sourceId,QString("shared-section-1-1"));const auto home=screen(result.project,"home"),status=screen(result.project,"status");
  QCOMPARE(home.value("readableLayout").toObject().value("primaryAction").toString(),sourceId+"__save");QCOMPARE(home.value("readableLayout").toObject().value("items").toArray().first().toObject().value("fontSize").toInt(),22);
  QCOMPARE(visual.value("resumeContext").toObject().value("elementIds").toArray().first().toString(),sourceId+"__calendar");
  QCOMPARE(status.value("elements").toArray().last().toObject().value("id").toString(),QString("shared-section-1-2"));QCOMPARE(status.value("readableLayout").toObject().value("items").toArray().size(),2);QVERIFY(!status.value("readableLayout").toObject().contains("actions"));QVERIFY(!screen(result.project,"plain").contains("readableLayout"));
  QCOMPARE(result.project.value("localization").toObject().value("entries").toArray().first().toObject().value("key").toString(),QString("/project/visual/components/@shared-section-1/elements/@save/text"));
  for(auto size:{QSize(1280,720),QSize(1600,900)}){const auto before=canvasElements(original.value("visual").toObject(),screen(original,"home").value("elements").toArray(),size);const auto after=canvasElements(visual,home.value("elements").toArray(),size);for(auto v:before){auto e=v.toObject();auto actual=row(after,sourceId+"__"+e.value("id").toString());for(auto key:{"x","y","width","height","visibleCondition","enabledCondition","action","binding","styleId"})QCOMPARE(actual.value(key),e.value(key));}}
  SharedSection::Source reused;QVERIFY(SharedSection::prepare(result.project,"home",{sourceId},reused,error));QVERIFY(!reused.convert);SharedSection::Review duplicate;QVERIFY(!SharedSection::apply(result.project,reused,"ignored",{{"status",true}},duplicate,error));QCOMPARE(duplicate.skipped,1);QVERIFY(duplicate.project.isEmpty());
 }
 void nestedInstanceAndCalendarKeepExpandedReadingReferences(){
  auto p=simple(),v=p.value("visual").toObject();auto nested=element("save-part","component");nested["componentId"]="saved-controls";nested["x"]=220;auto button=element("save-button","button");button["text"]="Save";button["action"]="save";v["components"]=QJsonArray{QJsonObject{{"id","saved-controls"},{"label","Save controls"},{"width",200},{"height",44},{"elements",QJsonArray{button}}}};
  auto ss=v.value("screens").toArray();auto home=ss[0].toObject();auto calendar=element("calendar","text");calendar["binding"]="calendar";home["elements"]=QJsonArray{calendar,nested};home["readableLayout"]=QJsonObject{{"items",QJsonArray{QJsonObject{{"elementId","calendar"}}}},{"actions",QJsonArray{"save-part__save-button"}},{"actionLayout","grouped"},{"primaryAction","save-part__save-button"}};ss[0]=home;v["screens"]=ss;p["visual"]=v;
  SharedSection::Source source;SharedSection::Review result;QString error;QVERIFY2(SharedSection::prepare(p,"home",{"calendar","save-part"},source,error),qPrintable(error));QVERIFY(source.leaves.contains("save-part__save-button"));QVERIFY(SharedSection::apply(p,source,"HUD",{{"home",true},{"status",true}},result,error));const auto reading=screen(result.project,"home").value("readableLayout").toObject();QCOMPARE(reading.value("primaryAction").toString(),result.sourceInstanceId+"__save-part__save-button");QCOMPARE(reading.value("items").toArray().size(),1);QCOMPARE(screen(result.project,"status").value("readableLayout").toObject().value("items").toArray().size(),2);
 }
 void rejectsUnsupportedAndUnsafeSelectionsWithoutMutation(){
  const auto original=simple();SharedSection::Source source;QString error;QVERIFY(!SharedSection::prepare(original,"home",{"calendar"},source,error));QVERIFY(!error.isEmpty());
  for(auto kind:{"schedule","stats","result","saveSlots"}){auto p=original,v=p.value("visual").toObject();auto ss=v.value("screens").toArray();auto s=ss[0].toObject();auto es=s.value("elements").toArray();auto e=es[1].toObject();e["kind"]=kind;es[1]=e;s["elements"]=es;ss[0]=s;v["screens"]=ss;p["visual"]=v;const auto before=p;QVERIFY(!SharedSection::prepare(p,"home",{"hud"},source,error));QCOMPARE(p,before);}
  auto p=original,v=p.value("visual").toObject();auto ss=v.value("screens").toArray();auto s=ss[0].toObject();auto es=s.value("elements").toArray();auto e=es[1].toObject();e["selectionKey"]="outside-list";es[1]=e;s["elements"]=es;ss[0]=s;v["screens"]=ss;p["visual"]=v;QVERIFY(!SharedSection::prepare(p,"home",{"hud"},source,error));
  s["elements"]=QJsonArray{element("first","text"),element("middle","text"),element("last","text")};ss[0]=s;v["screens"]=ss;p["visual"]=v;QVERIFY(!SharedSection::prepare(p,"home",{"first","last"},source,error));
 }
 void rejectsSelectionDependentInstanceOverrides(){
  for(const auto field:{"selectionKey","selectionBindings"})for(const bool nested:{false,true}){
   auto p=simple(),v=p.value("visual").toObject();auto button=element("save-button","button");button["action"]="save";
   auto owner=element("owner","component");owner["componentId"]="save-controls";
   const QJsonValue value=QString(field)=="selectionKey"?QJsonValue("outside-list"):QJsonValue(QJsonArray{QJsonObject{{"property","screenId"},{"field","destination"}}});
   owner["overrides"]=QJsonArray{QJsonObject{{"elementId","save-button"},{"values",QJsonObject{{field,value}}}}};
   QJsonArray definitions{QJsonObject{{"id","save-controls"},{"elements",QJsonArray{button}}}};
   if(nested){definitions.append(QJsonObject{{"id","wrapper"},{"elements",QJsonArray{owner}}});owner=element("wrapper-instance","component");owner["componentId"]="wrapper";}
   v["components"]=definitions;v["screens"]=QJsonArray{QJsonObject{{"id","home"},{"elements",QJsonArray{owner}}}};p["visual"]=v;
   const auto before=p;SharedSection::Source source;QString error;QVERIFY(!SharedSection::prepare(p,"home",{owner.value("id").toString()},source,error));QVERIFY(!error.isEmpty());QCOMPARE(p,before);
  }
 }
 void sourceReadingMembershipIsNotExpanded(){
  auto p=simple(),v=p.value("visual").toObject();auto ss=v.value("screens").toArray();auto home=ss[0].toObject();auto es=home.value("elements").toArray();for(auto kind:{"text","image","button"}){auto e=element(QString("omitted-")+kind,kind);e["parentId"]="hud";es.append(e);}home["elements"]=es;ss[0]=home;v["screens"]=ss;p["visual"]=v;
  SharedSection::Source source;SharedSection::Review result;QString error;QVERIFY(SharedSection::prepare(p,"home",{"hud"},source,error));QVERIFY(SharedSection::apply(p,source,"HUD",{{"home",true},{"status",true}},result,error));const auto reading=screen(result.project,"home").value("readableLayout").toObject();QCOMPARE(reading.value("items").toArray().size(),1);QCOMPARE(reading.value("actions").toArray().size(),1);QCOMPARE(reading.value("items").toArray().first().toObject().value("elementId").toString(),result.sourceInstanceId+"__calendar");QCOMPARE(screen(result.project,"status").value("readableLayout").toObject().value("items").toArray().size(),5);
 }
 void nestedExpansionIsBounded(){
  auto p=simple(),v=p.value("visual").toObject();QJsonArray leaves,instances;for(int i=0;i<100;++i){leaves.append(element("text-"+QString::number(i),"text"));auto e=element("instance-"+QString::number(i),"component");e["componentId"]="leaf-set";instances.append(e);}v["components"]=QJsonArray{QJsonObject{{"id","leaf-set"},{"elements",leaves}},QJsonObject{{"id","large-set"},{"elements",instances}}};auto owner=element("owner","component");owner["componentId"]="large-set";v["screens"]=QJsonArray{QJsonObject{{"id","home"},{"elements",QJsonArray{owner}}}};p["visual"]=v;SharedSection::Source source;QString error;QVERIFY(!SharedSection::prepare(p,"home",{"owner"},source,error));QVERIFY(!error.isEmpty());
 }
 void explicitCanvasOnlyWarnsAndKeepsPrimary(){
  const auto original=simple();SharedSection::Source source;QString error;QVERIFY(SharedSection::prepare(original,"home",{"hud"},source,error));SharedSection::Review out;QVERIFY(SharedSection::apply(original,source,"HUD",{{"home",true},{"status",false}},out,error));QCOMPARE(screen(out.project,"status").value("readableLayout"),screen(original,"status").value("readableLayout"));QVERIFY(out.lines.join(" ").contains("2"));
 }
 void actualDialogCancelReviewUndoPropagationAndReopen(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());QTemporaryDir temp(QDir::tempPath()+"/qt-shared-section-XXXXXX");QVERIFY(temp.isValid());QVERIFY(copyFixture(engine+"/examples/my-first-raising-game",temp.path()));
  QFile file(temp.path()+"/project.json");QVERIFY(file.open(QIODevice::ReadOnly));auto p=QJsonDocument::fromJson(file.readAll()).object();file.close();auto visual=p.value("visual").toObject(),sample=simple().value("visual").toObject();auto screens=visual.value("screens").toArray();
  auto title=screens[0].toObject();auto es=title.value("elements").toArray();for(auto v:screen(simple(),"home").value("elements").toArray()){auto e=v.toObject();e.remove("styleId");es.append(e);}title["elements"]=es;auto reading=title.value("readableLayout").toObject();auto items=reading.value("items").toArray();items.append(QJsonObject{{"elementId","calendar"},{"fontSize",22}});reading["items"]=items;auto actions=reading.value("actions").toArray();actions.append("save");reading["actions"]=actions;title["readableLayout"]=reading;screens[0]=title;visual["screens"]=screens;p["visual"]=visual;
  QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(p).toJson());file.close();
  StudioWindow w(engine);w.show();w.openProject(temp.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);QCOMPARE(ProjectManager::recentProjects(),QStringList{QFileInfo(temp.path()).canonicalFilePath()});openScreen(w,"title");select(w,"title","hud");const auto before=w.projectData();const auto revision=w.currentView().value("revision");
  auto invoke=[&](bool accept){bool visited=false;QTimer::singleShot(60,&w,[&]{auto d=w.findChild<QDialog*>("sharedSectionDialog");QVERIFY(d);QTimer::singleShot(10000,d,&QDialog::reject);d->resize(850,600);target(d,"schedule");target(d,"story");auto box=d->findChild<QDialogButtonBox*>("sharedSectionButtons");QVERIFY(!box->button(QDialogButtonBox::Apply)->isEnabled());d->findChild<QLineEdit*>("sharedSectionName")->setText("Shared condition HUD");d->findChild<QPushButton*>("reviewSharedSection")->click();QVERIFY2(box->button(QDialogButtonBox::Apply)->isEnabled(),qPrintable(d->findChild<QLabel*>("sharedSectionError")->text()));QVERIFY(d->findChild<QPlainTextEdit*>("sharedSectionReview")->toPlainText().contains("schedule"));QTest::qWait(20);QVERIFY(d->rect().contains(QRect(box->mapTo(d,QPoint()),box->size())));const auto evidence=engine+"/.cache/shared-section";QDir().mkpath(evidence);QVERIFY(d->grab().save(evidence+"/review-"+editorLanguage()+".png"));visited=true;box->button(accept?QDialogButtonBox::Apply:QDialogButtonBox::Cancel)->click();});w.findChild<QAction*>("shareScreenSection")->trigger();QVERIFY(visited);};
  invoke(false);QCOMPARE(w.projectData(),before);QCOMPARE(w.currentView().value("revision"),revision);invoke(true);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));const auto shared=w.projectData();QVERIFY(shared!=before);QCOMPARE(shared.value("visual").toObject().value("components").toArray().size(),before.value("visual").toObject().value("components").toArray().size()+1);
  w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),before);w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),shared);
  const auto instance=screen(shared,"title").value("elements").toArray().last().toObject();const auto instanceId=instance.value("id").toString(),componentId=instance.value("componentId").toString();select(w,"title",instanceId);
  QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("sharedSectionDialog");QVERIFY(d);QTimer::singleShot(10000,d,&QDialog::reject);auto table=d->findChild<QTableWidget*>("sharedSectionTargets");for(int i=0;i<table->rowCount();++i)if(table->item(i,0)->data(Qt::UserRole)=="schedule")QVERIFY(!(table->item(i,0)->flags()&Qt::ItemIsEnabled));d->reject();});w.findChild<QAction*>("shareScreenSection")->trigger();QCOMPARE(w.projectData(),shared);
  QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("componentsDialog");QVERIFY(d);QTimer::singleShot(10000,d,&QDialog::reject);auto children=d->findChild<QListWidget*>("componentChildren");for(int i=0;i<children->count();++i)if(children->item(i)->text().startsWith("save ·"))children->setCurrentRow(i);d->findChild<QLineEdit*>("child-text")->setText("Keep this memory");d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});w.findChild<QPushButton*>("componentSource")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  const auto updated=w.projectData(),uv=updated.value("visual").toObject();for(auto id:{"title","schedule","story"}){const auto s=screen(updated,id);QString owner;for(auto v:s.value("elements").toArray())if(v.toObject().value("componentId")==componentId)owner=v.toObject().value("id").toString();QVERIFY(!owner.isEmpty());QCOMPARE(row(canvasElements(uv,s.value("elements").toArray(),{1280,720}),owner+"__save").value("text").toString(),QString("Keep this memory"));}
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.currentView().value("dirty").toBool(),20000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));w.openProject(temp.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);QCOMPARE(w.projectData(),updated);
  for(auto d:w.currentView().value("diagnostics").toArray())QVERIFY2(d.toObject().value("severity")!="error",qPrintable(QString::fromUtf8(QJsonDocument(d.toObject()).toJson())));
 }
};
QTEST_MAIN(SharedSectionTests)
#include "shared-section-test.moc"
