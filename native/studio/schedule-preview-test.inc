#include <QScrollArea>
#include <QScrollBar>

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
