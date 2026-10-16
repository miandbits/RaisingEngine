void StudioTests::galleryUiAuthoring(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  const auto root=engine+"/.cache/gallery-ui-editor-"+QString::number(QDateTime::currentMSecsSinceEpoch());
  const auto evidence=engine+"/.cache/gallery-ui";QDir().mkpath(evidence);
  StudioWindow w(engine);w.resize(1500,960);w.show();
  QTimer::singleShot(100,&w,[&]{auto d=w.findChild<QDialog*>("newProjectDialog");QVERIFY(d);auto mode=d->findChild<QComboBox*>("newProjectTemplate");mode->setCurrentIndex(mode->findData("existing"));d->findChild<QLineEdit*>("newProjectSource")->setText(engine+"/examples/last-grandson-polished");d->findChild<QLineEdit*>("newProjectDestination")->setText(root);d->findChild<QDialogButtonBox*>("newProjectButtons")->button(QDialogButtonBox::Ok)->click();});
  w.findChild<QAction*>("newProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),30000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  const auto original=w.projectData();
  QTimer::singleShot(100,&w,[&]{auto d=w.findChild<QDialog*>("projectSettingsDialog");QVERIFY(d);QTimer::singleShot(20000,d,&QDialog::reject);d->resize(1160,800);d->findChild<QTabWidget*>()->setCurrentIndex(2);d->findChild<QLineEdit*>("projectGalleryTitle")->setText("설이의 추억");d->findChild<QPushButton*>("projectGalleryCreateUi")->click();QCOMPARE(d->findChild<QComboBox*>("projectGalleryScreen")->currentData().toString(),QString("gallery"));QCOMPARE(d->findChild<QComboBox*>("projectGalleryDetailScreen")->currentData().toString(),QString("gallery-detail"));d->grab().save(evidence+"/editor-settings.png");d->findChild<QDialogButtonBox*>("projectSettingsButtons")->button(QDialogButtonBox::Ok)->click();});
  w.findChild<QAction*>("projectSettings")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  const auto generated=w.projectData();QCOMPARE(generated.value("gallery").toObject().value("screenId").toString(),QString("gallery"));
  w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),original);
  w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),generated);
  auto open=[&](QString id){auto l=w.findChild<QListWidget*>("projectList");for(int i=0;i<l->count();i++)if(l->item(i)->data(Qt::UserRole)==id){auto item=l->item(i);l->scrollToItem(item);QTest::mouseClick(l->viewport(),Qt::LeftButton,Qt::NoModifier,l->visualItemRect(item).center());QTest::mouseDClick(l->viewport(),Qt::LeftButton,Qt::NoModifier,l->visualItemRect(item).center());return;}QFAIL("Missing gallery screen");};
  auto element=[&](QString id){for(auto v:w.projectData().value("visual").toObject().value("screens").toArray())if(v.toObject().value("id")=="gallery")for(auto e:v.toObject().value("elements").toArray())if(e.toObject().value("id")==id)return e.toObject();return QJsonObject();};
  auto select=[&](QString id){auto tree=w.findChild<QTreeWidget*>("hierarchy");QVERIFY(tree);QTreeWidgetItemIterator it(tree);while(*it){if((*it)->data(0,Qt::UserRole).toString()==id){tree->scrollToItem(*it);QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(*it).center());return;}++it;}QFAIL("Missing authored gallery element");};
  open("gallery");
  select("tab-cg");auto action=w.findChild<QComboBox*>("buttonAction"),target=w.findChild<QComboBox*>("actionTarget");QCOMPARE(action->currentData().toString(),QString("galleryFilter"));QCOMPARE(target->currentData().toString(),QString("kind:cg"));
  // Rebind a normal button through the inspector, and undo the edit.
  target->setCurrentIndex(target->findData("status:unlocked"));QMetaObject::invokeMethod(target,"activated",Q_ARG(int,target->currentIndex()));QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(element("tab-cg").value("galleryFilter").toString(),QString("status:unlocked"));
  w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  // Add a category filter from the standard toolbar and action inspector.
  w.findChild<QAction*>("addButton")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  auto label=w.findChild<QLineEdit*>("buttonText");label->setText("인연 분류");QMetaObject::invokeMethod(label,"editingFinished");QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  action->setCurrentIndex(action->findData("galleryFilter"));QMetaObject::invokeMethod(action,"activated",Q_ARG(int,action->currentIndex()));QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  QVERIFY(target->findData("category:인연")>=0);target->setCurrentIndex(target->findData("category:인연"));QMetaObject::invokeMethod(target,"activated",Q_ARG(int,target->currentIndex()));QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  for(auto p:QList<QPair<QString,int>>{{"elementX",820},{"elementY",120},{"elementWidth",160},{"elementHeight",44}}){auto f=w.findChild<QSpinBox*>(p.first);QVERIFY(f);f->setValue(p.second);QMetaObject::invokeMethod(f,"editingFinished");QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);}
  select("entries");
  QTimer::singleShot(100,&w,[&]{auto d=w.findChild<QDialog*>("dataUiDialog");QVERIFY(d);QTimer::singleShot(20000,d,&QDialog::reject);QCOMPARE(d->findChild<QComboBox*>("dataUiSource")->currentData().toString(),QString("galleryEntries"));auto fields=d->findChild<QComboBox*>("dataUiFilterField");QVERIFY(fields->findData("unlocked")>=0);QVERIFY(fields->findData("kind")>=0);d->findChild<QSpinBox*>("dataUiColumns")->setValue(2);d->grab().save(evidence+"/editor-bindings.png");d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});
  w.findChild<QAction*>("editDataUi")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  // Edit the reusable card definition, not the generated screen data on disk.
  QTimer::singleShot(100,&w,[&]{auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(d);QTimer::singleShot(20000,d,&QDialog::reject);auto library=d->findChild<QListWidget*>("componentLibrary");library->setCurrentRow(library->count()-2);auto children=d->findChild<QListWidget*>("componentChildren");QCOMPARE(children->count(),5);children->setCurrentRow(0);d->findChild<QLineEdit*>("child-fill")->setText("#29474c");children->setCurrentRow(4);auto ca=d->findChild<QComboBox*>("child-action");QCOMPARE(ca->currentData().toString(),QString("galleryView"));QVERIFY(ca->findData("galleryReplay")>=0);d->grab().save(evidence+"/editor-card.png");d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});
  w.findChild<QAction*>("editComponents")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  const auto authored=w.projectData();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);QCOMPARE(w.projectData(),authored);
  w.findChild<QAction*>("validateProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);for(auto v:w.currentView().value("diagnostics").toArray())QVERIFY2(v.toObject().value("severity")!="error",qPrintable(QString::fromUtf8(QJsonDocument(v.toObject()).toJson())));
  open("gallery");select("entries");QTest::qWait(500);w.grab().save(evidence+"/editor-canvas.png");
  QFile proof(evidence+"/editor-project.txt");QVERIFY(proof.open(QIODevice::WriteOnly));proof.write(root.toUtf8());
}
