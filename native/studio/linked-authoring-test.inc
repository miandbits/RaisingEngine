// An external plan contains only names and edits to perform through widgets.
// Private game contents stay outside the public repository.
void StudioTests::linkedAuthoringRoundtrip(){
  const auto config=qEnvironmentVariable("RAISING_LINKED_PLAN");if(config.isEmpty())QSKIP("Set RAISING_LINKED_PLAN to an isolated authoring plan");
  QFile f(config);QVERIFY(f.open(QIODevice::ReadOnly));const auto plan=QJsonDocument::fromJson(f.readAll()).object();
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()),root=plan.value("project").toString(),evidence=plan.value("evidence").toString();
  if(!evidence.isEmpty())QDir().mkpath(evidence);
  StudioWindow w(engine);w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),30000);
  auto list=w.findChild<QListWidget*>("projectList");QVERIFY(list);QListWidgetItem *screen=nullptr;
  for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole)==plan.value("screen").toString())screen=list->item(i);
  QVERIFY(screen);QMetaObject::invokeMethod(list,"itemActivated",Q_ARG(QListWidgetItem*,screen));
  auto hierarchy=w.findChild<QTreeWidget*>("hierarchy");QTreeWidgetItem *element=nullptr;
  for(QTreeWidgetItemIterator it(hierarchy);*it;++it)if((*it)->data(0,Qt::UserRole)==plan.value("element").toString())element=*it;
  QVERIFY(element);hierarchy->setCurrentItem(element);w.findChild<QPushButton*>("elementConnections")->click();
  auto dock=w.findChild<QDockWidget*>("authoringWorkspaceDock");QVERIFY(dock);auto tree=dock->findChild<QTreeWidget*>("workspaceEntities");
  auto key=[&]{return tree->currentItem()?tree->currentItem()->data(0,Qt::UserRole).toJsonObject().value("key").toString():QString();};
  const auto source="screenElement:"+plan.value("screen").toString()+"/"+plan.value("element").toString();
  QTRY_COMPARE_WITH_TIMEOUT(key(),source,30000);
  auto connections=dock->findChild<QTreeWidget*>("workspaceConnections");QVERIFY(connections);
  int index=0;
  for(auto value:plan.value("steps").toArray()){
    const auto step=value.toObject(),previous=tree->currentItem()->data(0,Qt::UserRole).toJsonObject();
    const auto target=step.value("target").toString();QTreeWidgetItem *link=nullptr;
    for(QTreeWidgetItemIterator it(connections);*it;++it)if((*it)->data(0,Qt::UserRole)==target){link=*it;break;}
    QVERIFY2(link,qPrintable("Missing connection: "+target));
    QMetaObject::invokeMethod(connections,"itemDoubleClicked",Q_ARG(QTreeWidgetItem*,link),Q_ARG(int,0));QCOMPARE(key(),target);QTest::qWait(60);
    if(step.contains("text")){
      const auto fieldName=step.value("field").toString();
      if(auto field=dock->findChild<QLineEdit*>(fieldName)){field->setFocus();field->selectAll();QTest::keyClicks(field,"x");field->setText(step.value("text").toString());}
      else if(auto field=dock->findChild<QPlainTextEdit*>(fieldName))field->setPlainText(step.value("text").toString());
      else QFAIL(qPrintable("Missing field: "+fieldName));
      // Moving away while drafting must neither switch item nor erase text.
      dock->findChild<QPushButton*>("workspaceBack")->click();QCOMPARE(key(),target);
    }
    if(step.contains("number")){auto field=dock->findChild<QSpinBox*>(step.value("field").toString());QVERIFY(field);field->setValue(step.value("number").toInt());}
    if(step.contains("logic")){
      const auto settings=step.value("logic").toObject();bool edited=false;
      QTimer::singleShot(50,&w,[&]{auto dialog=w.findChild<QDialog*>("logicEditor");QVERIFY(dialog);QStringList path;for(auto v:settings.value("path").toArray())path<<v.toString();characterNumber(dialog,path,settings.value("number").toInt());edited=true;dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});
      auto button=dock->findChild<QPushButton*>(settings.value("button").toString());QVERIFY(button);button->click();QVERIFY(edited);
    }
    if(step.contains("text")||step.contains("number")||step.contains("logic")){
      const auto revision=w.currentView().value("revision").toInt();dock->findChild<QPushButton*>("workspaceApply")->click();
      QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&w.currentView().value("revision").toInt()>revision,15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));QTest::qWait(100);QCOMPARE(key(),target);
    }
    if(step.value("checkScript").toBool()){
      dock->findChild<QPushButton*>("workspaceScript")->click();
      QTRY_VERIFY(w.findChild<QLineEdit*>("scriptNodeId"));
      QCOMPARE(w.findChild<QLineEdit*>("scriptNodeId")->text(),target.mid(target.indexOf(':')+1).split('/').value(2));
      auto dialog=qobject_cast<QDialog*>(w.findChild<QLineEdit*>("scriptNodeId")->window());QVERIFY(dialog);dialog->reject();QTest::qWait(50);
    }
    if(!evidence.isEmpty())QVERIFY(dock->grab().save(evidence+"/step-"+QString::number(++index)+".png"));
    dock->findChild<QPushButton*>("workspaceBack")->click();QCOMPARE(key(),previous.value("key").toString());
    dock->findChild<QPushButton*>("workspaceForward")->click();QCOMPARE(key(),target);
  }
  dock->findChild<QPushButton*>("workspaceOrigin")->click();QCOMPARE(key(),source);
  dock->findChild<QPushButton*>("workspace-saveProject")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.currentView().value("dirty").toBool(),20000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  const auto saved=w.currentView().value("documents");
  w.findChild<QAction*>("validateProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY(w.findChild<QLabel*>("validationSummary")->text().contains("오류 0건"));
  w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);QCOMPARE(w.currentView().value("documents"),saved);
  if(!plan.value("playAutomation").toString().isEmpty()){
    dock->hide();w.playAutomation=plan.value("playAutomation").toString();w.playReport=plan.value("playReport").toString();
    QSignalSpy finished(&w,&StudioWindow::playerFinished);w.findChild<QAction*>("playProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(finished.count()>0,180000);QCOMPARE(finished.first().first().toInt(),0);
  }
}
