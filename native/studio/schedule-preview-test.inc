#include <QScrollArea>
#include <QScrollBar>
#include "choice-preview.hpp"

void StudioTests::initialChoiceCanvasPreview() {
  const QJsonObject node{{"id","choose"},{"type","choice"},{"choices",QJsonArray{QJsonObject{{"text","Work together"},{"consequenceText","A personal promise"}},QJsonObject{{"text","Work alone"}}}}};
  const QJsonArray docs{QJsonObject{{"kind","activity"},{"data",QJsonObject{{"nodes",QJsonArray{node}}}}},QJsonObject{{"kind","event"},{"data",QJsonObject{{"id","meeting"},{"label","First meeting"},{"nodes",QJsonArray{node}}}}}};
  const auto original=docs;const auto sample=initialChoicePreview(docs);QCOMPARE(sample.value("rows").toArray().size(),2);QCOMPARE(sample.value("source").toString(),QString("First meeting / choose"));QCOMPARE(docs,original);QVERIFY(initialChoicePreview({}).isEmpty());
  QImage image(700,320,QImage::Format_ARGB32);image.fill(Qt::magenta);QPainter p(&image);
  QJsonObject data{{"_choicePreview",sample},{"choiceLayout","cards"},{"showChoiceEffects",true}};
  paintChoicePreview(&p,QRectF(0,0,640,280),data);p.end();QCOMPARE(image.pixelColor(680,300),QColor(Qt::magenta));QVERIFY(image.pixelColor(10,35)!=QColor(Qt::magenta));
  const auto before=image;data["showChoiceEffects"]=false;image.fill(Qt::magenta);p.begin(&image);paintChoicePreview(&p,QRectF(0,0,640,280),data);p.end();QVERIFY(image!=before);
  image.fill(Qt::magenta);p.begin(&image);paintChoicePreview(&p,QRectF(0,0,80,35),data);p.end();QCOMPARE(image.pixelColor(90,40),QColor(Qt::magenta));
}

void StudioTests::widgetSettingsPreserveRoster() {
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT");QTemporaryDir temp(engine+"/.cache/widget-roster-XXXXXX");QVERIFY(temp.isValid());
  StudioWindow w(engine);w.resize(1280,800);w.show();
  QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("newProjectDialog");QVERIFY(d);auto p=d->findChild<QComboBox*>("newProjectTemplate");p->setCurrentIndex(p->findData("genre-suite/roster"));d->findChild<QLineEdit*>("newProjectDestination")->setText(temp.path()+"/game");d->findChild<QDialogButtonBox*>("newProjectButtons")->button(QDialogButtonBox::Ok)->click();});
  w.findChild<QAction*>("newProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  const auto roster=w.projectData()["roster"].toObject();QCOMPARE(roster["stateModel"].toString(),QString("character-v1"));
  w.findChild<QAction*>("add-element-choices")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  auto before=w.projectData();before.remove("visual");
  QTimer::singleShot(40,&w,[&]{auto d=w.findChild<QDialog*>("widgetSettingsDialog");QVERIFY(d);auto mode=d->findChild<QComboBox*>("widgetLayout");mode->setCurrentIndex(mode->findData("cards"));d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();});
  w.findChild<QPushButton*>("widgetSettings")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  QCOMPARE(w.projectData()["roster"].toObject(),roster);auto after=w.projectData();after.remove("visual");QCOMPARE(after,before);
  bool found=false;for(const auto& screen:w.projectData()["visual"].toObject()["screens"].toArray())for(const auto& entry:screen.toObject()["elements"].toArray())if(entry.toObject()["id"]==w.selectedId()){
    QCOMPARE(entry.toObject()["choiceLayout"].toString(),QString("cards"));QVERIFY(!entry.toObject().contains("decreasingAttributeIds"));found=true;
  }QVERIFY(found);
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  QFile f(temp.path()+"/game/project.json");QVERIFY(f.open(QIODevice::ReadOnly));const auto saved=QJsonDocument::fromJson(f.readAll()).object();QCOMPARE(saved["roster"].toObject(),roster);QCOMPARE(saved["visual"],w.projectData()["visual"]);
}

void StudioTests::pinnedWidgetTools() {
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT");QTemporaryDir temp(engine+"/.cache/pinned-widget-XXXXXX");QVERIFY(temp.isValid());
  StudioWindow w(engine);w.resize(1280,800);w.show();
  QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("newProjectDialog");QVERIFY(d);auto p=d->findChild<QComboBox*>("newProjectTemplate");p->setCurrentIndex(p->findData("genre-suite/roster"));d->findChild<QLineEdit*>("newProjectDestination")->setText(temp.path()+"/game");d->findChild<QDialogButtonBox*>("newProjectButtons")->button(QDialogButtonBox::Ok)->click();});
  w.findChild<QAction*>("newProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  auto tools=w.findChild<QWidget*>("inspectorQuickTools");auto configure=w.findChild<QPushButton*>("widgetSettings");auto follow=w.findChild<QPushButton*>("elementConnections");QVERIFY(tools&&configure&&follow);
  w.findChild<QAction*>("add-element-schedule")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY(configure->isEnabled());QVERIFY(follow->isEnabled());
  auto dock=w.findChild<QDockWidget*>("inspectorDock");auto scroll=dock->findChild<QScrollArea*>();QVERIFY(scroll);QVERIFY(!scroll->isAncestorOf(configure));QVERIFY(!scroll->isAncestorOf(follow));
  const auto before=configure->mapToGlobal(QPoint());scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());QTest::qWait(20);QCOMPARE(configure->mapToGlobal(QPoint()),before);QVERIFY(configure->isVisible());
  const auto project=w.projectData();bool opened=false;
  QTimer::singleShot(30,&w,[&]{auto d=w.findChild<QDialog*>("widgetSettingsDialog");QVERIFY(d);opened=true;QVERIFY(d->findChild<QCheckBox*>("showActivityRules"));d->reject();});
  configure->click();QVERIFY(opened);QCOMPARE(w.projectData(),project);
  auto canvas=w.findChild<ScreenCanvas*>();QVERIFY(canvas);const auto selection=canvas->selectedIds();QVERIFY(selection.size()==1);
  canvas->selectionsChanged({});QVERIFY(!configure->isEnabled());QVERIFY(!follow->isEnabled());
  QString other;for(const auto& screen:project["visual"].toObject()["screens"].toArray()){
    const auto entries=screen.toObject()["elements"].toArray();bool owns=false;for(const auto& entry:entries)if(entry.toObject()["id"]==selection.front())owns=true;
    if(owns)for(const auto& entry:entries)if(entry.toObject()["id"]!=selection.front()){other=entry.toObject()["id"].toString();break;}
  }QVERIFY(!other.isEmpty());
  canvas->selectionsChanged({selection.front(),other});QVERIFY(!configure->isEnabled());QVERIFY(!follow->isEnabled());
  canvas->selectionsChanged(selection);QVERIFY(configure->isEnabled());
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  QVERIFY(!QJsonDocument(w.projectData()).toJson().contains("_preview"));
}

void StudioTests::initialRosterCanvasPreview() {
  QJsonObject project{{"roster",QJsonObject{{"members",QJsonArray{QJsonObject{{"characterId","one"},{"activityId","work"}},QJsonObject{{"characterId","two"},{"activityId","rest"}}}}}},
    {"_previewContextNames",QJsonObject{{"characterId",QJsonObject{{"one","One"},{"two","Two"}}}}},
    {"_previewAttributes",QJsonObject{{"money",QJsonObject{{"label","Funds"}}}}},
    {"_previewActivities",QJsonArray{QJsonObject{{"id","work"},{"label","Form pottery"},{"costs",QJsonArray{QJsonObject{{"attributeId","money"},{"amount",4}}}}},QJsonObject{{"id","rest"},{"label","Rest"}}}}};
  const auto original=project;auto rows=initialRosterPreview(project);QCOMPARE(rows.size(),2);QCOMPARE(rows[0].toObject()["name"].toString(),QString("One"));QCOMPARE(rows[1].toObject()["activity"].toString(),QString("Rest"));QVERIFY(rows[0].toObject()["costs"].toString().contains("Funds 4"));QCOMPARE(project,original);
  QImage image(700,300,QImage::Format_ARGB32);image.fill(Qt::magenta);QPainter p(&image);paintRosterPreview(&p,QRectF(0,0,640,240),QJsonObject{{"_rosterPreview",rows},{"showActivityRules",true}});p.end();
  QCOMPARE(image.pixelColor(680,280),QColor(Qt::magenta));QVERIFY(image.pixelColor(5,5)!=QColor(Qt::magenta));
  const auto before=image;auto activities=project["_previewActivities"].toArray();auto activity=activities[0].toObject();activity["label"]="Glaze pottery";activities[0]=activity;project["_previewActivities"]=activities;rows=initialRosterPreview(project);
  QCOMPARE(rows[0].toObject()["activity"].toString(),QString("Glaze pottery"));image.fill(Qt::magenta);p.begin(&image);paintRosterPreview(&p,QRectF(0,0,640,240),QJsonObject{{"_rosterPreview",rows},{"showActivityRules",true}});p.end();QVERIFY(image!=before);
}
