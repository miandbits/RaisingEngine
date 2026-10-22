#include <QTemporaryDir>

void StudioTests::narrativeActorBinding() {
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  QTemporaryDir temporary(engine+"/.cache/narrative-actor-XXXXXX");QVERIFY(temporary.isValid());
  const auto root=temporary.path()+"/game";StudioWindow w(engine);w.show();
  QTimer::singleShot(80,&w,[&]{auto d=w.findChild<QDialog*>("newProjectDialog");QVERIFY(d);auto preset=d->findChild<QComboBox*>("newProjectTemplate");preset->setCurrentIndex(preset->findData("academy-visual"));d->findChild<QLineEdit*>("newProjectDestination")->setText(root);d->findChild<QDialogButtonBox*>("newProjectButtons")->button(QDialogButtonBox::Ok)->click();});
  w.findChild<QAction*>("newProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  QSet<QString> originalIds;for(const auto &screen:w.projectData().value("visual").toObject().value("screens").toArray())for(const auto &item:screen.toObject().value("elements").toArray())originalIds.insert(item.toObject().value("id").toString());
  auto addedElement=[&]{for(const auto &screen:w.projectData().value("visual").toObject().value("screens").toArray())for(const auto &item:screen.toObject().value("elements").toArray())if(!originalIds.contains(item.toObject().value("id").toString()))return item.toObject();return QJsonObject{};};
  auto palette=w.findChild<QToolButton*>("elementPalette");QVERIFY(palette);bool added=false;
  for(auto action:palette->menu()->actions())if(action->text()=="텍스트"){action->trigger();added=true;break;}QVERIFY(added);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(addedElement().value("kind").toString(),QString("text"));
  auto field=w.findChild<QComboBox*>("textBinding");QVERIFY(field);QVERIFY(field->findData("narrative-actor")>=0);
  w.findChild<QPushButton*>("dataBindingPicker")->click();auto dialog=w.findChild<QDialog*>("dataBindingDialog");QVERIFY(dialog);
  auto list=dialog->findChild<QListWidget*>("dataBindingSources");QListWidgetItem* source=nullptr;
  for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toJsonObject().value("binding")=="narrative-actor")source=list->item(i);
  QVERIFY(source);QCOMPARE(source->text(),QString("현재 사건의 대상 인물"));list->setCurrentItem(source);
  dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  QCOMPARE(addedElement().value("binding").toString(),QString("narrative-actor"));
  // A designer must find actual choice consequences without knowing a binding ID.
  w.findChild<QPushButton*>("dataBindingPicker")->click();dialog=w.findChild<QDialog*>("dataBindingDialog");QVERIFY(dialog);
  list=dialog->findChild<QListWidget*>("dataBindingSources");source=nullptr;
  dialog->findChild<QLineEdit*>("dataBindingSearch")->setText("선택 결과");
  for(int i=0;i<list->count();++i)if(!list->item(i)->isHidden()&&list->item(i)->text()=="선택 결과 · 실제 변화")source=list->item(i);
  QVERIFY(source);list->setCurrentItem(source);
  dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  QCOMPARE(addedElement().value("binding").toString(),QString("choice-result"));
  const auto elementId=addedElement().value("id");
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  bool found=false;for(const auto &screen:w.projectData().value("visual").toObject().value("screens").toArray())for(const auto &item:screen.toObject().value("elements").toArray())if(item.toObject().value("id")==elementId){found=true;QCOMPARE(item.toObject().value("binding").toString(),QString("choice-result"));}QVERIFY(found);
}

void StudioTests::rosterNamedMembers() {
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  QTemporaryDir temporary(engine+"/.cache/roster-names-XXXXXX");QVERIFY(temporary.isValid());
  const auto root=temporary.path()+"/game";StudioWindow w(engine);w.show();
  QTimer::singleShot(80,&w,[&]{auto d=w.findChild<QDialog*>("newProjectDialog");QVERIFY(d);auto preset=d->findChild<QComboBox*>("newProjectTemplate");preset->setCurrentIndex(preset->findData("academy-visual"));d->findChild<QLineEdit*>("newProjectDestination")->setText(root);d->findChild<QDialogButtonBox*>("newProjectButtons")->button(QDialogButtonBox::Ok)->click();});
  w.findChild<QAction*>("newProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  auto character=[&](const QString &id){for(const auto &v:w.currentView()["documents"].toArray()){const auto d=v.toObject();if(d["kind"]=="character"&&d["data"].toObject()["id"]==id)return d["data"].toObject();}return QJsonObject{};};
  const auto originalMentor=character("mentor");QVERIFY(!originalMentor.isEmpty());
  QString firstGenerated,secondGenerated;
  QTimer::singleShot(80,&w,[&]{auto d=w.findChild<QDialog*>("rosterAuthoringDialog");QVERIFY(d);QTimer::singleShot(10000,d,&QDialog::reject);
    auto table=d->findChild<QTableWidget*>("rosterMembers");auto picker=d->findChild<QComboBox*>("rosterExistingCharacter");QVERIFY(table);QVERIFY(picker);QVERIFY(table->isColumnHidden(0));
    const int mentor=picker->findData("mentor");QVERIFY(mentor>=0);QCOMPARE(picker->itemText(mentor),originalMentor["name"].toString());picker->setCurrentIndex(mentor);d->findChild<QPushButton*>("rosterAddExisting")->click();QCOMPARE(table->rowCount(),2);QCOMPARE(table->item(1,0)->text(),QString("mentor"));QVERIFY(!(table->item(1,0)->flags()&Qt::ItemIsEditable));QCOMPARE(picker->findData("mentor"),-1);
    table->item(1,1)->setText("Second apprentice");
    auto local=d->findChild<QListWidget*>("rosterLocalAttributes");for(int i=0;i<local->count();++i)if(local->item(i)->data(Qt::UserRole)=="money"){local->item(i)->setCheckState(Qt::Unchecked);QVERIFY(table->isColumnHidden(i+3));QVERIFY(!table->cellWidget(1,i+3)->isEnabled());local->item(i)->setCheckState(Qt::Checked);QVERIFY(!table->isColumnHidden(i+3));local->item(i)->setCheckState(Qt::Unchecked);}
    d->findChild<QCheckBox*>("rosterShowIds")->setChecked(true);QVERIFY(!table->isColumnHidden(0));
    d->findChild<QPushButton*>("rosterAddMember")->click();QCOMPARE(table->rowCount(),3);firstGenerated=table->item(2,0)->text();table->item(2,1)->setText("New apprentice");
    d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});
  w.findChild<QAction*>("editRoster")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  auto expectedMentor=originalMentor;expectedMentor["name"]="Second apprentice";QCOMPARE(character("mentor"),expectedMentor);QVERIFY(!character(firstGenerated).isEmpty());
  const auto firstRoster=w.projectData().value("roster");
  QTimer::singleShot(80,&w,[&]{auto d=w.findChild<QDialog*>("rosterAuthoringDialog");QVERIFY(d);auto table=d->findChild<QTableWidget*>("rosterMembers");table->setCurrentCell(0,1);QVERIFY(!d->findChild<QPushButton*>("rosterRemoveMember")->isEnabled());table->setCurrentCell(2,1);d->findChild<QPushButton*>("rosterRemoveMember")->click();QCOMPARE(table->rowCount(),2);QVERIFY(d->findChild<QComboBox*>("rosterExistingCharacter")->findData(firstGenerated)>=0);d->findChild<QPushButton*>("rosterAddMember")->click();secondGenerated=table->item(2,0)->text();QVERIFY(secondGenerated!=firstGenerated);d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});
  w.findChild<QAction*>("editRoster")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  QVERIFY(!character(firstGenerated).isEmpty());QVERIFY(!character(secondGenerated).isEmpty());QCOMPARE(character("mentor"),expectedMentor);
  const auto secondRoster=w.projectData().value("roster");w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData()["roster"],firstRoster);w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData()["roster"],secondRoster);
  QTimer::singleShot(60,&w,[&]{auto d=w.findChild<QDialog*>("rosterAuthoringDialog");QVERIFY(d);d->findChild<QPushButton*>("rosterAddMember")->click();d->reject();});w.findChild<QAction*>("editRoster")->trigger();QCOMPARE(w.projectData()["roster"],secondRoster);
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(w.projectData()["roster"],secondRoster);QCOMPARE(character("mentor"),expectedMentor);
}

void StudioTests::rosterAuthoringRoundtrip() {
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  const auto root=qEnvironmentVariable("RAISING_ROSTER_PROJECT",engine+"/.cache/roster-authored-"+QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(!QFileInfo::exists(root));StudioWindow w(engine);w.show();
  bool created=false;
  QTimer::singleShot(100,&w,[&]{auto d=w.findChild<QDialog*>("newProjectDialog");QVERIFY(d);auto mode=d->findChild<QComboBox*>("newProjectTemplate");mode->setCurrentIndex(mode->findData("academy-visual"));d->findChild<QLineEdit*>("newProjectDestination")->setText(root);created=true;d->findChild<QDialogButtonBox*>("newProjectButtons")->button(QDialogButtonBox::Ok)->click();});
  w.findChild<QAction*>("newProject")->trigger();QVERIFY(created);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  const auto before=w.currentView().value("documents");
  QDir().mkpath(engine+"/docs/evidence/roster");
  bool authored=false;
  QTimer::singleShot(100,&w,[&]{auto d=w.findChild<QDialog*>("rosterAuthoringDialog");QVERIFY(d);QTimer::singleShot(10000,d,&QDialog::reject);
    auto locals=d->findChild<QListWidget*>("rosterLocalAttributes");QVERIFY(locals);
    for(int i=0;i<locals->count();i++){auto item=locals->item(i);const auto id=item->data(Qt::UserRole).toString();item->setCheckState(id=="skill"||id=="stamina"||id=="stress"?Qt::Checked:Qt::Unchecked);}
    auto table=d->findChild<QTableWidget*>("rosterMembers");QVERIFY(table);d->findChild<QPushButton*>("rosterAddMember")->click();QCOMPARE(table->rowCount(),2);
    table->item(0,1)->setText(QString::fromUtf8("민서"));table->item(1,1)->setText(QString::fromUtf8("하린"));
    for(int row=0;row<2;row++){auto combo=qobject_cast<QComboBox*>(table->cellWidget(row,2));combo->setCurrentIndex(combo->findData(row==0?"study":"rest"));}
    // These are visible controls, not generated project JSON.
    for(int col=3;col<table->columnCount();col++){const auto label=table->horizontalHeaderItem(col)->text();
      if(label==QString::fromUtf8("검술")){qobject_cast<QSpinBox*>(table->cellWidget(0,col))->setValue(10);qobject_cast<QSpinBox*>(table->cellWidget(1,col))->setValue(40);}
      if(label==QString::fromUtf8("체력")){qobject_cast<QSpinBox*>(table->cellWidget(0,col))->setValue(80);qobject_cast<QSpinBox*>(table->cellWidget(1,col))->setValue(30);}
      if(label==QString::fromUtf8("스트레스"))qobject_cast<QSpinBox*>(table->cellWidget(1,col))->setValue(20);
    }
    d->grab().save(engine+"/docs/evidence/roster/authoring.png");authored=true;d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
  });
  w.findChild<QAction*>("editRoster")->trigger();QVERIFY(authored);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  const auto roster=w.projectData()["roster"].toObject();QCOMPARE(roster["members"].toArray().size(),2);QVERIFY(!roster["localAttributeIds"].toArray().contains("money"));
  w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.currentView()["documents"],before);
  w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData()["roster"].toObject(),roster);
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData()["roster"].toObject(),roster);
  w.grab().save(engine+"/docs/evidence/roster/editor-reopened.png");
  QFile proof(engine+"/.cache/roster-authoring-proof.json");QVERIFY(proof.open(QIODevice::WriteOnly));proof.write(QJsonDocument(QJsonObject{{"project",root},{"createdThroughWizard",true},{"authoredThroughWidgets",true},{"undoRedo",true},{"saveReopen",true},{"roster",roster}}).toJson());
}

void StudioTests::rosterPlayIntegration() {
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  QFile proof(engine+"/.cache/roster-authoring-proof.json");QVERIFY(proof.open(QIODevice::ReadOnly));
  const auto root=QJsonDocument::fromJson(proof.readAll()).object().value("project").toString();
  StudioWindow w(engine);w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  w.playAutomation=engine+"/docs/evidence/roster/play-steps.json";
  w.playReport=engine+"/docs/evidence/roster/editor-play-report.json";
  QVERIFY(QFileInfo::exists(w.playAutomation));QFile::remove(w.playReport);
  QSignalSpy finished(&w,&StudioWindow::playerFinished);w.findChild<QAction*>("playProject")->trigger();
  QTRY_VERIFY_WITH_TIMEOUT(finished.count()>0,60000);QVERIFY(QFileInfo::exists(w.playReport));
  QFile report(w.playReport);QVERIFY(report.open(QIODevice::ReadOnly));
  const auto result=QJsonDocument::fromJson(report.readAll()).object();QVERIFY2(result.value("ok").toBool(),report.readAll().constData());
  int checkpoints=0;
  for(const auto entry:result.value("steps").toArray()){
    const auto e=entry.toObject();if(!e.value("command").toObject().contains("checkpoint"))continue;
    QCOMPARE(e.value("ui").toObject().value("state").toObject().value("roster").toObject().value("attributes").toObject().size(),2);++checkpoints;
  }
  QVERIFY(checkpoints>=6);w.grab().save(engine+"/docs/evidence/roster/editor-play-completed.png");
}
