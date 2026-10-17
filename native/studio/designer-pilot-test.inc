namespace {
void designerQuick(QDialog *dialog, const QString &search, int amount, bool append=false) {
  auto field=dialog->findChild<QLineEdit*>("quickTargetSearch");QVERIFY(field);field->setText(search);
  auto targets=dialog->findChild<QComboBox*>("quickTarget");QCOMPARE(targets->count(),1);
  dialog->findChild<QSpinBox*>("quickAmount")->setValue(amount);
  dialog->findChild<QPushButton*>(append?"quickAppend":"quickApply")->click();
}
}

void StudioTests::designerQuickRules() {
  auto ctx=scriptTestContext();
  QJsonObject initial{{"op","flag"},{"flagId","keep-original"},{"equals",true}};
  LogicEditor condition(initial,"condition",ctx);condition.show();
  designerQuick(&condition,"skill",30,true);
  auto result=condition.value().toObject();QCOMPARE(result.value("op").toString(),QString("all"));
  QCOMPARE(result.value("conditions").toArray()[0].toObject(),initial);
  QCOMPARE(result.value("conditions").toArray()[1].toObject().value("right").toObject().value("value").toInt(),30);
  auto search=condition.findChild<QLineEdit*>("quickTargetSearch");search->setText("missing");
  QVERIFY(!condition.findChild<QPushButton*>("quickApply")->isEnabled());QCOMPARE(condition.value().toObject(),result);
  search->setText("Skill");QVERIFY(condition.findChild<QPushButton*>("quickApply")->isEnabled());
  condition.reject();QCOMPARE(condition.result(),int(QDialog::Rejected)); // Caller must not commit a rejected draft.
  LogicEditor effects(QJsonArray{},"effects",ctx);effects.show();
  effects.findChild<QComboBox*>("quickOwner")->setCurrentIndex(1);
  designerQuick(&effects,"skill",-4);
  auto value=effects.value().toArray().first().toObject();
  QCOMPARE(value.value("target").toObject().value("owner").toObject().value("kind").toString(),QString("actor"));
  QCOMPARE(value.value("value").toObject().value("value").toInt(),-4);
  designerQuick(&effects,"skill",8);QCOMPARE(effects.value().toArray().size(),2);
}

void StudioTests::designerSmallGameAuthoring() {
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  const auto root=qEnvironmentVariable("RAISING_DESIGNER_PROJECT",engine+"/.cache/last-grandson-"+QString::number(QDateTime::currentMSecsSinceEpoch()));
  QVERIFY(!QFileInfo::exists(root));
  const auto evidence=engine+"/.cache/designer-pilot";QDir().mkpath(evidence);
  StudioWindow w(engine);w.show();w.hide();w.show();QVERIFY(QTest::qWaitForWindowExposed(&w));
  QTimer::singleShot(70,&w,[&]{auto d=w.findChild<QDialog*>("newProjectDialog");QVERIFY(d);auto base=d->findChild<QComboBox*>("newProjectTemplate");base->setCurrentIndex(base->findData("academy-demo"));d->findChild<QLineEdit*>("newProjectDestination")->setText(root);d->findChild<QDialogButtonBox*>("newProjectButtons")->button(QDialogButtonBox::Ok)->click();});
  w.findChild<QAction*>("newProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  w.findChild<QAction*>("initializeStarterScreens")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  QCOMPARE(w.projectData().value("visual").toObject().value("screens").toArray().size(),4);
  const auto originalVisual=w.projectData().value("visual");w.findChild<QAction*>("initializeStarterScreens")->trigger();QCOMPARE(w.projectData().value("visual"),originalVisual);
  w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY(!w.projectData().contains("visual"));
  w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(w.projectData().value("visual"),originalVisual);
  // Follow the same entry point exposed on the empty canvas.
  QTimer::singleShot(70,&w,[&]{auto d=w.findChild<QDialog*>("gettingStartedDialog");QVERIFY(d);d->grab().save(evidence+"/01-guide.png");d->findChild<QPushButton*>("gettingStarted-settings")->click();QTimer::singleShot(70,&w,[&]{auto p=w.findChild<QDialog*>("projectSettingsDialog");QVERIFY(p);p->findChild<QLineEdit*>("projectTitle")->setText(tr("잔설 아래 막내손자"));p->findChild<QLineEdit*>("projectGameId")->setText("last-grandson");p->findChild<QPlainTextEdit*>("projectDescription")->setPlainText(tr("남궁가에서 천대받는 막내 남궁설. 태어날 때부터 막힌 혈맥과 한독 때문에 남은 시간은 두 달뿐이다. 비급 수련으로 내공을 쌓고 약방 심부름으로 약값을 벌며 운기조식으로 기력을 지켜라. 노인의 약속을 받아들이고 마지막 날 내공 30·기력 20·신뢰 1 이상을 갖추면 운명을 넘어설 수 있다."));p->findChild<QSpinBox*>("projectDurationMonths")->setValue(2);p->findChild<QSpinBox*>("projectPeriodsPerMonth")->setValue(3);p->findChild<QDialogButtonBox*>("projectSettingsButtons")->button(QDialogButtonBox::Ok)->click();});});
  w.findChild<QAction*>("showGettingStarted")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(w.projectData().value("gameId").toString(),QString("last-grandson"));
  auto edit=[&](QString file,QString label,QString description,QStringList text,std::function<void(QDialog*)> more){
    w.findChild<QLineEdit*>("contentSearch")->clear();
    QTimer::singleShot(60,&w,[&]{auto d=w.findChild<QDialog*>("scriptAuthoringDialog");const bool narrative=d!=nullptr;if(!d)d=w.findChild<QDialog*>("contentEditor");QVERIFY(d);if(!label.isEmpty())d->findChild<QLineEdit*>(narrative?"scriptLabel":"contentLabel")->setText(label);if(!description.isEmpty())d->findChild<QPlainTextEdit*>("contentDescription")->setPlainText(description);if(narrative){auto list=d->findChild<QListWidget*>("scriptNodes");int t=0;for(int i=0;i<list->count()&&t<text.size();++i){list->setCurrentRow(i);auto field=d->findChild<QPlainTextEdit*>("script-text");if(!field)field=d->findChild<QPlainTextEdit*>("script-prompt");if(field)field->setPlainText(text[t++]);}}else{auto lines=d->findChild<QTableWidget*>("dialogueTable");if(lines)for(int i=0;i<lines->rowCount()&&i<text.size();++i)lines->item(i,1)->setText(text[i]);}more(d);d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});
    QVERIFY(openContentDocument(w,file));QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  };
  const auto none=[](QDialog*){};
  edit("attributes.json",{},{},{},[&](QDialog*d){auto table=d->findChild<QTableWidget*>("attributeTable");QVERIFY(table);const QMap<QString,QString> names{{"skill",tr("내공")},{"money",tr("약값")},{"stamina",tr("기력")},{"stress",tr("한독")}};for(int i=0;i<table->rowCount();++i)table->item(i,1)->setText(names.value(table->item(i,0)->text()));
    // Verify an actionable error and recover without losing the draft.
    table->item(0,2)->setText("101");d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();QVERIFY(d->isVisible());QVERIFY(!d->findChild<QLabel*>("contentError")->text().isEmpty());d->grab().save(evidence+"/02-range-error.png");table->item(0,2)->setText("0");});
  edit("characters/apprentice.json",tr("남궁설"),tr("가문에서 쓸모없다며 멸시받는 시한부 막내손자. 아직 자신이 살고 싶은 이유를 놓지 않았다."),{},none);
  edit("characters/mentor.json",tr("약방의 진 노인"),{},{},none);
  edit("activities/study.json",tr("서고의 비급 수련"),tr("약값 6·기력 10을 써서 내공 12를 얻는다. 무리한 수련으로 한독이 5 오른다."),{},[&](QDialog*d){auto cost=d->findChild<QTableWidget*>("content-costs");auto effects=d->findChild<QTableWidget*>("content-baseEffects");QVERIFY(cost);QVERIFY(effects);qobject_cast<QSpinBox*>(cost->cellWidget(0,2))->setValue(6);qobject_cast<QSpinBox*>(effects->cellWidget(0,2))->setValue(12);});
  edit("activities/work.json",tr("약방 심부름"),tr("기력 10을 써서 약값 12를 번다. 찬바람을 맞아 한독이 8 오른다."),{},none);
  edit("activities/rest.json",tr("운기조식"),tr("기력 25를 회복하고 한독을 15 낮춘다. 휴식만으로 막힌 혈맥이 열리지는 않는다."),{},none);
  edit("events/exam-invitation.json",tr("버려진 아이에게 내민 손"),{}, {tr("사람들은 남궁가의 막내가 봄을 보지 못할 거라 했다. 진 노인은 약봉지를 내밀었다. '설아, 네 혈맥은 두 달 뒤 닫힌다. 하지만 네가 스스로 쌓은 내공이라면 길을 낼 수 있어. 약값 다섯 냥을 맡기고, 마지막 날 다시 오겠느냐?'"),tr("진 노인의 손을 잡을까?")},[&](QDialog*d){d->findChild<QSpinBox*>("scriptFromPeriod")->setValue(1);d->findChild<QSpinBox*>("scriptThroughPeriod")->setValue(1);QTimer::singleShot(60,&w,[&]{auto logic=w.findChild<QDialog*>("logicEditor");QVERIFY(logic);designerQuick(logic,"[skill]",0);logic->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});d->findChild<QPushButton*>("scriptRequirements")->click();});
  edit("events/final-exam.json",tr("마지막 밤의 문턱"),{}, {tr("눈발이 처마를 때리는 마지막 밤. 설은 기어이 약방 문을 밀었다. 진 노인은 떨리는 손목을 붙잡았다. '내가 열 수 있는 것은 문뿐이다. 그 너머로 걸어가는 힘은 네가 길렀구나.' 막힌 혈맥 속에서 작은 온기가 움직였다.")},none);
  edit("endings/certified.json",tr("생존 · 봄을 훔친 아이"),{}, {tr("자정을 넘겨도 숨이 끊어지지 않았다. 설은 자신을 버린 가문의 문패를 돌아보지 않았다. 찬밥과 멸시 속에서도 지킨 의지, 스스로 번 약값, 단 한 사람과 맺은 약속이 봄을 데려왔다. 강호에서 가장 약했던 막내의 이야기는 이제 시작이었다.")},[&](QDialog*d){QTimer::singleShot(60,&w,[&]{auto logic=w.findChild<QDialog*>("logicEditor");QVERIFY(logic);designerQuick(logic,"[skill]",30);designerQuick(logic,"[stamina]",20,true);designerQuick(logic,"[mentor/trust]",1,true);logic->grab().save(evidence+"/03-ending-conditions.png");logic->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});d->findChild<QPushButton*>("scriptRequirements")->click();});
  edit("endings/ordinary.json",tr("이별 · 눈 속에 남은 이름"),{}, {tr("마지막 눈이 내리던 밤, 설의 숨은 작은 새처럼 잦아들었다. 가문은 여전히 그를 실패한 막내라 불렀다. 그러나 약방의 노인은 묘비에 남궁설이라는 이름을 또박또박 새겼다. 쓸모없는 목숨은 없었다. 이 아이가 살고자 했다는 사실만은 봄에도 지워지지 않았다.")},none);
  QTimer::singleShot(60,&w,[&]{auto d=static_cast<ScriptAuthoringDialog*>(w.findChild<QDialog*>("scriptAuthoringDialog"));QVERIFY(d);auto docs=d->findChild<QComboBox*>("scriptDocument");for(int i=0;i<docs->count();++i)if(docs->itemText(i).contains("exam-invitation.json"))docs->setCurrentIndex(i);scriptSelectNode(*d,"registration");auto rows=d->findChild<QTableWidget*>("scriptChoices");QVERIFY(rows);rows->item(0,1)->setText(tr("약값 5를 맡기고, 살아서 돌아오겠다고 약속한다."));rows->item(1,1)->setText(tr("남의 도움은 받지 않겠다며 돌아선다."));d->grab().save(evidence+"/04-story-choices.png");d->findChild<QDialogButtonBox*>("scriptButtons")->button(QDialogButtonBox::Save)->click();});w.findChild<QAction*>("scriptAuthoringAction")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  QTimer::singleShot(60,&w,[&]{auto d=w.findChild<QDialog*>("componentsDialog");QVERIFY(d);auto library=d->findChild<QListWidget*>("componentLibrary");library->setCurrentRow(0);auto children=d->findChild<QListWidget*>("componentChildren");children->setCurrentRow(0);d->findChild<QLineEdit*>("child-text")->setText(tr("기록"));children->setCurrentRow(1);d->findChild<QLineEdit*>("child-text")->setText(tr("이어가기"));d->grab().save(evidence+"/05-common-component.png");d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});w.findChild<QAction*>("editComponents")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  const auto toolbar=w.projectData().value("visual").toObject().value("components").toArray().first().toObject();QCOMPARE(toolbar.value("elements").toArray()[0].toObject().value("text").toString(),tr("기록"));
  // File/ID search must work after editing/rebuilding the list too.
  auto search=w.findChild<QLineEdit*>("contentSearch");search->setText("activities/study.json");auto list=w.findChild<QListWidget*>("contentList");int visible=0;for(int i=0;i<list->count();++i)if(!list->item(i)->isHidden())++visible;QCOMPARE(visible,1);
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(w.projectData().value("title").toString(),tr("잔설 아래 막내손자"));
  w.findChild<QAction*>("validateProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));w.grab().save(evidence+"/05-editor-validated.png");
  for(const auto &diagnostic:w.currentView().value("diagnostics").toArray())QVERIFY2(diagnostic.toObject().value("severity")!="error",qPrintable(QString::fromUtf8(QJsonDocument(diagnostic.toObject()).toJson())));
  QFile proof(engine+"/.cache/designer-project.txt");QVERIFY(proof.open(QIODevice::WriteOnly));proof.write(root.toUtf8());proof.close();
}

void StudioTests::designerSmallGamePlay() {
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  QFile proof(engine+"/.cache/designer-project.txt");
  const auto root=proof.open(QIODevice::ReadOnly)?QString::fromUtf8(proof.readAll()).trimmed():engine+"/examples/last-grandson";
  // Generate fresh inputs and verify native checkpoints; never depend on a previous test run's cache.
  QProcess prepare;auto environment=QProcessEnvironment::systemEnvironment();environment.insert("RAISING_DESIGNER_PROJECT",root);prepare.setProcessEnvironment(environment);prepare.setWorkingDirectory(engine);
  prepare.start("node",{"--test","tests/native-player/designer-pilot.test.mjs"});QVERIFY(prepare.waitForStarted());QVERIFY(prepare.waitForFinished(180000));
  QCOMPARE(prepare.exitStatus(),QProcess::NormalExit);QVERIFY2(prepare.exitCode()==0,qPrintable(QString::fromUtf8(prepare.readAllStandardOutput()+prepare.readAllStandardError())));
  StudioWindow w(engine);w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  const auto evidence=engine+"/.cache/designer-pilot";
  for(const auto route:{QString("survival"),QString("farewell")}){
    w.playAutomation=evidence+"/"+route+"-steps.json";w.playReport=evidence+"/"+route+"-report.json";
    QSignalSpy finished(&w,&StudioWindow::playerFinished);w.findChild<QAction*>("playProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(finished.count()>0,120000);QCOMPARE(finished.first().first().toInt(),0);
    QFile report(w.playReport);QVERIFY(report.open(QIODevice::ReadOnly));auto result=QJsonDocument::fromJson(report.readAll()).object();QVERIFY2(result.value("ok").toBool(),qPrintable(QString::fromUtf8(QJsonDocument(result).toJson()).right(1500)));
    QJsonObject final;for(const auto &step:result.value("steps").toArray())if(step.toObject().value("command").toObject().value("checkpoint")=="finished")final=step.toObject().value("ui").toObject().value("state").toObject();
    QCOMPARE(final.value("phase").toString(),QString("finished"));QCOMPARE(final.value("endingId").toString(),route=="survival"?QString("certified"):QString("ordinary"));
  }
}

void StudioTests::designerSampleCleanup() {
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  QFile proof(engine+"/.cache/designer-project.txt");QVERIFY(proof.open(QIODevice::ReadOnly));const auto root=QString::fromUtf8(proof.readAll()).trimmed();
  StudioWindow w(engine);w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  QCOMPARE(w.projectData().value("gameId").toString(),QString("last-grandson"));
  bool found=false;
  QTimer::singleShot(60,&w,[&]{auto d=w.findChild<QDialog*>("contentOrganizer");QVERIFY(d);auto table=d->findChild<QTableWidget*>("organizerTable");QVERIFY(table);
    for(int row=0;row<table->rowCount();++row)if(QStringList{"scenarios/certified-route.json","scenarios/ordinary-route.json"}.contains(table->item(row,0)->data(Qt::UserRole).toString())){table->item(row,0)->setCheckState(Qt::Checked);found=true;}
    d->findChild<QDialogButtonBox*>()->button(found?QDialogButtonBox::Save:QDialogButtonBox::Cancel)->click();
  });w.findChild<QAction*>("organizeContent")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  for(const auto &doc:w.currentView().value("documents").toArray())QVERIFY(doc.toObject().value("kind")!="scenario");
}

void StudioTests::designerPublishSample() {
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  const auto destination=qEnvironmentVariable("RAISING_DESIGNER_PUBLISH");
  if(destination.isEmpty())QSKIP("Set RAISING_DESIGNER_PUBLISH to a new sample directory.");
  QVERIFY(!QFileInfo::exists(destination));QFile proof(engine+"/.cache/designer-project.txt");QVERIFY(proof.open(QIODevice::ReadOnly));const auto source=QString::fromUtf8(proof.readAll()).trimmed();proof.close();
  StudioWindow w(engine);w.show();
  QTimer::singleShot(60,&w,[&]{auto d=w.findChild<QDialog*>("newProjectDialog");QVERIFY(d);auto base=d->findChild<QComboBox*>("newProjectTemplate");base->setCurrentIndex(base->findData("existing"));d->findChild<QLineEdit*>("newProjectSource")->setText(source);d->findChild<QLineEdit*>("newProjectDestination")->setText(destination);d->findChild<QDialogButtonBox*>("newProjectButtons")->button(QDialogButtonBox::Ok)->click();});w.findChild<QAction*>("newProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));QCOMPARE(w.projectData().value("gameId").toString(),QString("last-grandson"));
  QVERIFY(proof.open(QIODevice::WriteOnly|QIODevice::Truncate));proof.write(destination.toUtf8());
}
