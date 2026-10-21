void StudioTests::freeScreenAuthoring() {
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  const auto project=qEnvironmentVariable("RAISING_QT_PROJECT");QVERIFY(!project.isEmpty());
  StudioWindow w(engine);w.show();w.openProject(project);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  auto action=[&](const char* name){auto a=w.findChild<QAction*>(name);QVERIFY(a&&a->isEnabled());a->trigger();};
  auto settle=[&]{QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));};
  auto combo=[&](const char* name,const QString& value){auto f=w.findChild<QComboBox*>(name);QVERIFY(f);const int i=f->findData(value);QVERIFY2(i>=0,qPrintable(QString(name)+": "+value));f->setCurrentIndex(i);QMetaObject::invokeMethod(f,"activated",Q_ARG(int,i));settle();};
  auto text=[&](const char* name,const QString& value){auto f=w.findChild<QLineEdit*>(name);QVERIFY(f);f->setText(value);QMetaObject::invokeMethod(f,"editingFinished");settle();};
  auto geometry=[&](int x,int y,int width,int height){for(auto pair:QList<QPair<const char*,int>>{{"elementX",x},{"elementY",y},{"elementWidth",width},{"elementHeight",height}}){auto f=w.findChild<QSpinBox*>(pair.first);QVERIFY(f);f->setValue(pair.second);QMetaObject::invokeMethod(f,"editingFinished");settle();}};
  auto add=[&](const QString& caption){auto palette=w.findChild<QToolButton*>("elementPalette");QVERIFY(palette);bool found=false;for(auto a:palette->menu()->actions())if(a->text()==caption){a->trigger();found=true;break;}QVERIFY(found);settle();};
  QTimer::singleShot(150,&w,[]{auto d=qobject_cast<QInputDialog*>(QApplication::activeModalWidget());QVERIFY(d);d->setTextValue("Free Canvas");QTest::keyClick(d->findChild<QLineEdit*>(),Qt::Key_Return);});
  action("newScreen");settle();
  auto tabs=w.findChild<QTabBar*>("documentTabs");const auto canvas=tabs->tabData(tabs->currentIndex()).toString();QVERIFY(canvas.startsWith("screen-"));
  action("screenRouting");auto routing=w.findChild<QDialog*>("screenRoutingDialog");QVERIFY(routing);combo("routingDefault",canvas);
  QTest::mouseClick(routing->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok),Qt::LeftButton);settle();
  QCOMPARE(w.projectData().value("visual").toObject().value("routing").toObject().value("defaultScreenId").toString(),canvas);
  add("패널");geometry(0,0,1280,720);
  add("텍스트");geometry(40,24,700,50);combo("textBinding","title");
  add("텍스트");geometry(40,100,700,45);
  auto picker=w.findChild<QPushButton*>("dataBindingPicker");QVERIFY(picker);QTest::mouseClick(picker,Qt::LeftButton);
  auto dialog=w.findChild<QDialog*>("dataBindingDialog");QVERIFY(dialog);auto sources=dialog->findChild<QListWidget*>("dataBindingSources");QVERIFY(sources);QListWidgetItem* relation=nullptr;bool attributes=false;
  for(int i=0;i<sources->count();++i){const auto target=sources->item(i)->data(Qt::UserRole).toJsonObject().value("dataBinding").toObject();if(target.value("kind")=="attribute")attributes=true;if(target.value("kind")=="relationship"&&!relation)relation=sources->item(i);}
  QVERIFY(attributes);QVERIFY(relation);sources->setCurrentItem(relation);QTest::mouseClick(sources->viewport(),Qt::LeftButton,Qt::NoModifier,sources->visualItemRect(relation).center());dialog->findChild<QLineEdit*>("dataBindingFormat")->setText("{label}: {value}");
  QTest::mouseClick(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok),Qt::LeftButton);settle();
  add("게이지 · 수치 데이터");geometry(820,490,350,24);
  QTest::mouseClick(picker,Qt::LeftButton);dialog=w.findChild<QDialog*>("dataBindingDialog");QVERIFY(dialog);
  sources=dialog->findChild<QListWidget*>("dataBindingSources");QVERIFY(sources);QVERIFY(sources->count()>0);
  for(int i=0;i<sources->count();++i)QVERIFY(sources->item(i)->data(Qt::UserRole).toJsonObject().contains("dataBinding"));
  sources->setCurrentRow(0);auto maximum=dialog->findChild<QSpinBox*>("dataMeterMaximum"),thickness=dialog->findChild<QSpinBox*>("dataMeterThickness");QVERIFY(maximum&&thickness);maximum->setValue(100);thickness->setValue(4);
  QTest::mouseClick(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok),Qt::LeftButton);settle();
  QJsonObject authoredMeter;for(const auto& row:w.projectData().value("visual").toObject().value("screens").toArray())if(row.toObject().value("id")==canvas)for(const auto& value:row.toObject().value("elements").toArray())if(value.toObject().value("kind")=="meter")authoredMeter=value.toObject();
  QVERIFY(authoredMeter.contains("dataBinding"));QCOMPARE(authoredMeter.value("meterThickness").toInt(),4);
  add("일정 선택");geometry(40,170,740,170);combo("visibilityPhase","planning");
  add("활동 결과");geometry(40,350,740,100);combo("visibilityPhase","result");
  add("대사");geometry(40,470,740,120);
  add("선택지");geometry(820,170,420,300);combo("visibilityPhase","choice");
  auto button=[&](QString label,QString type,int x,int y){action("addButton");settle();text("buttonText",label);geometry(x,y,200,56);combo("buttonAction",type);};
  button("처음 시작","start",40,620);combo("visibilityPhase","title");
  button("새로 시작","start",40,620);combo("visibilityPhase","finished");
  button("계속","advance",1040,620);
  button("저장","save",600,620);
  button("불러오기","load",820,620);
  // A normal screen can also serve as a menu during narrative or after the ending.
  button("다른 화면","navigate",820,24);combo("actionTarget","title");
  add("텍스트");geometry(40,470,740,120);combo("textBinding","ending");combo("visibilityPhase","finished");
  auto list=w.findChild<QListWidget*>("projectList");QVERIFY(list);
  for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole)=="title"){list->setCurrentRow(i);QTest::keyClick(list,Qt::Key_Return);break;}
  button("플레이로 돌아가기","navigate",820,620);combo("actionTarget",canvas);
  button("저장","save",600,620);
  for(int i=0;i<tabs->count();++i)if(tabs->tabData(i)==canvas){tabs->setCurrentIndex(i);break;}
  action("saveProject");settle();QVERIFY(!w.currentView().value("dirty").toBool());
  QVERIFY(QDir().mkpath(QDir(engine).filePath(".cache/free-screens")));
  const auto evidence=QDir(engine).filePath(".cache/free-screens/editor-authored.png");QVERIFY(w.grab().save(evidence));
  QFile info(QDir(engine).filePath(".cache/free-screens/gui-result.json"));QVERIFY(info.open(QIODevice::WriteOnly));info.write(QJsonDocument(QJsonObject{{"screenId",canvas},{"project",project},{"view",w.currentView()}}).toJson());
  // Exercise the production Play action: the shared service packages this saved draft.
  const auto smoke=QDir(engine).filePath(".cache/free-screens/editor-smoke.json");QFile steps(smoke);QVERIFY(steps.open(QIODevice::WriteOnly));steps.write("[{\"checkpoint\":\"authored-start\"}]");steps.close();w.playAutomation=smoke;w.playReport=QDir(engine).filePath(".cache/free-screens/editor-smoke-report.json");
  action("playProject");QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),30000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(w.playReport),15000);
}
