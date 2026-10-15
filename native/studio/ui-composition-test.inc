void StudioTests::uiCompositionAuthoringRoundtrip(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  const auto root=qEnvironmentVariable("RAISING_UI5_PROJECT",engine+"/.cache/ui5-authored-"+QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(!QFileInfo::exists(root));QVERIFY(copyWorkspaceSample(engine+"/tests/fixtures/projects/data-ui-lab",root));
  const auto evidence=engine+"/docs/evidence/ui-composition";QDir().mkpath(evidence);
  StudioWindow w(engine);w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);const auto original=w.projectData();bool authored=false;
  QTimer::singleShot(80,&w,[&]{auto d=w.findChild<QDialog*>("uiCompositionDialog");QVERIFY(d);QTimer::singleShot(30000,d,&QDialog::reject);
    auto pickOwner=[&](QString key){characterChoose(d->findChild<QComboBox*>("uiOwner"),key);};
    auto pick=[&](QString id){auto list=d->findChild<QListWidget*>("uiElements");for(int i=0;i<list->count();i++)if(list->item(i)->data(Qt::UserRole)==id){list->setCurrentRow(i);return;}QFAIL(qPrintable(id));};
    auto text=[&](QString key,QString value){auto f=d->findChild<QLineEdit*>("ui-"+key);QVERIFY(f);f->setText(value);};
    auto num=[&](QString key,int value){auto f=d->findChild<QSpinBox*>("ui-"+key);QVERIFY(f);f->setValue(value);};
    auto choice=[&](QString key,QString value){characterChoose(d->findChild<QComboBox*>("ui-"+key),value);};
    auto add=[&](QString kind){characterChoose(d->findChild<QComboBox*>("uiAddKind"),kind);d->findChild<QPushButton*>("uiAddElement")->click();};
    auto table=[&](QString name,QList<QStringList> rows){auto t=d->findChild<QTableWidget*>(name);QVERIFY(t);t->setRowCount(rows.size());for(int r=0;r<rows.size();r++)for(int c=0;c<t->columnCount();c++)t->setItem(r,c,new QTableWidgetItem(rows[r].value(c)));};
    d->findChild<QCheckBox*>("uiResponsive")->setChecked(true);d->findChild<QPushButton*>("uiAddModal")->click();
    add("group");num("x",320);num("y",180);num("width",640);num("height",160);choice("anchorX","center");choice("anchorY","center");choice("overflow","scroll");choice("flowDirection","column");num("flowPadding",12);num("flowGap",10);text("fill","#253e48");text("color","#ffffff");
    add("text");choice("parentId","group-1");choice("binding","selected-field");text("selectionKey","service");text("selectionField","title");text("valueFormat","Service: {value}");text("color","#ffffff");num("height",50);
    add("button");choice("parentId","group-1");choice("action","executeAction");text("text","Run selected service");text("selectionKey","service");text("tooltip","Runs only the selected row's action and quantity.");num("height",50);table("uiSelectionBindings",{{"actionId","target"},{"quantity","quantity"}});
    add("button");choice("parentId","group-1");choice("action","closeModal");text("text","Close details");num("height",50);
    pickOwner("component:data-card-1");d->findChild<QPushButton*>("uiDuplicateComponent")->click();pick("button");choice("action","selectRow");choice("screenId","modal-1");text("tooltip","Open details for this service");
    d->findChild<QPushButton*>("uiAddComponent")->click();add("component");choice("componentId","ui-part-1");num("x",0);num("y",0);num("width",360);num("height",180);
    pickOwner("screen:screen-6");pick("collection-1");choice("componentId","ui-part-2");text("selectionKey","service");text("sortField","title");choice("sortDirection","asc");text("filterField","show");choice("filterOperator","eq");text("filterValue","true");choice("layout","list");choice("anchorY","");num("height",200);num("width",1000);table("uiRowBindings",{{"component-1__title","text","title",""},{"component-1__value","text","target","Action: {value}"},{"component-1__button","text","title",""}});
    pickOwner("screen:home");add("button");text("text","Preview details");choice("action","openModal");choice("screenId","modal-1");num("x",1000);num("y",660);num("width",240);num("height",44);
    pickOwner("screen:screen-6");pick("collection-1");QTest::qWait(80);d->grab().save(evidence+"/authoring-list.png");
    pickOwner("screen:modal-1");pick("group-1");QTest::qWait(80);d->grab().save(evidence+"/authoring-modal.png");authored=true;d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
  });w.findChild<QAction*>("editUiComposition")->trigger();QVERIFY(authored);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));const auto authoredProject=w.projectData();QVERIFY(authoredProject!=original);
  w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),original);w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),authoredProject);
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError+QString::fromUtf8(QJsonDocument(w.currentView().value("diagnostics").toArray()).toJson())));w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(w.projectData(),authoredProject);
  QFile proof(engine+"/.cache/ui-composition-proof.json");QVERIFY(proof.open(QIODevice::WriteOnly));proof.write(QJsonDocument(QJsonObject{{"project",root},{"sourceSample","tests/fixtures/projects/data-ui-lab"},{"guiEdited",true},{"saveReopen",true},{"undoRedo",true}}).toJson());
}

void StudioTests::uiCompositionPlayIntegration(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());QFile proof(engine+"/.cache/ui-composition-proof.json");QVERIFY(proof.open(QIODevice::ReadOnly));const auto root=QJsonDocument::fromJson(proof.readAll()).object().value("project").toString();
  StudioWindow w(engine);w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  w.playAutomation=engine+"/docs/evidence/ui-composition/play-1280x720-steps.json";w.playReport=engine+"/docs/evidence/ui-composition/editor-play-report.json";QVERIFY(QFileInfo::exists(w.playAutomation));
  QSignalSpy finished(&w,&StudioWindow::playerFinished);w.findChild<QAction*>("playProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(finished.count()>0,60000);QCOMPARE(finished.first().first().toInt(),0);
  QFile report(w.playReport);QVERIFY(report.open(QIODevice::ReadOnly));const auto result=QJsonDocument::fromJson(report.readAll()).object();QVERIFY(result.value("ok").toBool());QJsonObject saved,loaded;bool prayer=false;
  for(const auto& v:result.value("steps").toArray()){const auto step=v.toObject();const auto tag=step.value("command").toObject().value("checkpoint").toString();const auto state=step.value("ui").toObject().value("state").toObject();if(tag=="prayer"){QCOMPARE(state.value("attributes").toObject().value("reputation").toInt(),2);prayer=true;}if(tag=="saved")saved=state;if(tag=="loaded")loaded=state;}
  QVERIFY(prayer);QVERIFY(!saved.isEmpty());QCOMPARE(loaded,saved);w.grab().save(engine+"/docs/evidence/ui-composition/editor-play.png");
}
