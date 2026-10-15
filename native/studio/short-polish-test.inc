// Real widget editing of the authored short; the base artwork/layout is data-authored.
void StudioTests::shortPolishAuthoring(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  const auto root=qEnvironmentVariable("RAISING_SHORT_EDITOR_PROJECT",engine+"/.cache/short-editor-"+QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(!QFileInfo::exists(root));
  const auto evidence=engine+"/.cache/short-polish";QDir().mkpath(evidence);
  StudioWindow w(engine);w.resize(1600,1000);w.show();
  QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("newProjectDialog");QVERIFY(d);auto mode=d->findChild<QComboBox*>("newProjectTemplate");mode->setCurrentIndex(mode->findData("existing"));d->findChild<QLineEdit*>("newProjectSource")->setText(engine+"/examples/last-grandson-polished");d->findChild<QLineEdit*>("newProjectDestination")->setText(root);d->findChild<QDialogButtonBox*>("newProjectButtons")->button(QDialogButtonBox::Ok)->click();});
  w.findChild<QAction*>("newProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),30000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  QTRY_VERIFY_WITH_TIMEOUT(w.pluginManager()->loadedIds().contains("cutout-animation"),15000);
  for(bool enabled:{false,true}){
    QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("projectSettingsDialog");QVERIFY(d);auto playback=d->findChild<QComboBox*>("projectSchedulePlayback");playback->setCurrentIndex(playback->findData("continuous"));auto check=d->findChild<QCheckBox*>("projectResultPlayback");QVERIFY(check);check->setChecked(enabled);auto change=d->findChild<QSpinBox*>("projectResult-changeMs");QVERIFY(change);QCOMPARE(change->isEnabled(),enabled);if(enabled){d->findChild<QTabWidget*>()->setCurrentIndex(1);cutoutE2e::typeSpin(change,"800");d->grab().save(evidence+"/editor-result-playback.png");}d->findChild<QDialogButtonBox*>("projectSettingsButtons")->button(QDialogButtonBox::Ok)->click();});
    w.findChild<QAction*>("projectSettings")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(w.projectData().value("visual").toObject().contains("resultPlayback"),enabled);
  }
  auto search=w.findChild<QLineEdit*>("hierarchySearch");QVERIFY(search);search->setText("schedule");
  auto tree=w.findChild<QTreeWidget*>("hierarchy");QVERIFY(tree);bool selected=false;
  for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole).toString()=="schedule"&&(*it)->data(0,Qt::UserRole+1).toString()=="schedule"){tree->setCurrentItem(*it);selected=true;break;}QVERIFY(selected);
  auto element=[&]{for(const auto& s:w.projectData().value("visual").toObject().value("screens").toArray())if(s.toObject().value("id")=="schedule")for(const auto& e:s.toObject().value("elements").toArray())if(e.toObject().value("id")=="schedule")return e.toObject();return QJsonObject();};
  for(const bool enabled:{false,true}){
    QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("widgetSettingsDialog");QVERIFY(d);auto check=d->findChild<QCheckBox*>("showActivityDescription");QVERIFY(check);check->setChecked(enabled);d->grab().save(evidence+"/editor-activity-details.png");d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();});
    auto settings=w.findChild<QPushButton*>("widgetSettings");QVERIFY(settings);settings->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(element().value("showActivityDescription").toBool(),enabled);
  }
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);
  if(!w.lastError.isEmpty()){QFile diagnostic(evidence+"/editor-error.json");if(diagnostic.open(QIODevice::WriteOnly))diagnostic.write(QJsonDocument(w.currentView()).toJson());}
  QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  search->setText("sd-character");for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole).toString()=="sd-character"&&(*it)->data(0,Qt::UserRole+1).toString()=="schedule"){tree->setCurrentItem(*it);break;}
  auto launch=w.findChild<QPushButton*>("cutoutOpenWorkspace");QVERIFY(launch);launch->click();QTest::qWait(300);
  auto panel=w.findChild<QWidget*>("cutoutPanel");QVERIFY(panel&&panel->isVisible());
  auto characters=panel->findChild<QTreeWidget*>("cutoutCharacterList");QVERIFY(characters);bool sdSelected=false;
  for(QTreeWidgetItemIterator it(characters);*it;++it)if((*it)->data(0,Qt::UserRole).toString()=="rig:seol-sd"){characters->setCurrentItem(*it);sdSelected=true;break;}QVERIFY(sdSelected);QTest::qWait(150);
  auto clips=panel->findChild<QListWidget*>("cutoutClipList");QVERIFY(clips);bool found=false;for(int i=0;i<clips->count();i++)if(clips->item(i)->data(Qt::UserRole).toString()=="study"){clips->setCurrentRow(i);found=true;break;}QVERIFY(found);
  QTest::qWait(150);auto duration=panel->findChild<QSpinBox*>("cutoutClipDuration");QVERIFY(duration);
  const auto before=duration->value();QCOMPARE(before,2400);cutoutE2e::typeSpin(duration,"2600");QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  bool animate=false;for(auto button:panel->findChildren<QAbstractButton*>())if(button->text().contains("(Animate)")){button->click();animate=button->isChecked();break;}QVERIFY(animate);
  auto play=panel->findChild<QPushButton*>("cutoutPlay");QVERIFY(play&&play->isEnabled());play->click();QTest::qWait(650);
  auto time=panel->findChild<QSpinBox*>("cutoutTimeField");QVERIFY(time&&time->value()>0);w.grab().save(evidence+"/editor-animation.png");play->click();
  w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);
  QCOMPARE(w.projectData().value("schedulePlayback").toString(),QString("continuous"));QCOMPARE(w.projectData().value("visual").toObject().value("resultPlayback").toObject().value("changeMs").toInt(),800);
  const auto data=cutoutE2e::pluginData(w.currentView(),"plugins/cutout-animation/rig/seol-sd.json");bool persisted=false;for(const auto& c:data.value("clips").toArray())if(c.toObject().value("id")=="study")persisted=c.toObject().value("durationMs")==2600;QVERIFY(persisted);
  w.findChild<QAction*>("validateProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);for(const auto& d:w.currentView().value("diagnostics").toArray())QVERIFY2(d.toObject().value("severity")!="error",qPrintable(QString::fromUtf8(QJsonDocument(d.toObject()).toJson())));
  QFile proof(evidence+"/editor-project.txt");QVERIFY(proof.open(QIODevice::WriteOnly));proof.write(root.toUtf8());
}
