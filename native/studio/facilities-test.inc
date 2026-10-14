  void StudioTests::facilitiesAuthoringAndPlay() {
    const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
    const auto root=qEnvironmentVariable("RAISING_QT_PROJECT");
    StudioWindow w(engine);w.resize(1440,980);w.show();w.openProject(root);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
    const auto baseline=w.projectData();
    auto restore=qScopeGuard([&]{
      for(int i=0;i<150&&w.projectData()!=baseline;i++){
        if(auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget()))d->reject();
        w.findChild<QAction*>("undoEdit")->trigger();QElapsedTimer t;t.start();while(w.busy()&&t.elapsed()<10000)QTest::qWait(20);
      }
      w.findChild<QAction*>("saveProject")->trigger();QElapsedTimer t;t.start();while(w.busy()&&t.elapsed()<10000)QTest::qWait(20);
    });
    auto openScreen=[&](QString id){auto list=w.findChild<QListWidget*>("projectList");for(int i=0;i<list->count();i++)if(list->item(i)->data(Qt::UserRole)==id){auto entry=list->item(i);list->scrollToItem(entry);QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(entry).center());QTest::mouseDClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(entry).center());return;}QFAIL("Missing screen");};
    auto activate=[&](const char* key,QString data){auto field=w.findChild<QComboBox*>(key);QVERIFY(field);int i=field->findData(data);QVERIFY(i>=0);field->setCurrentIndex(i);QMetaObject::invokeMethod(field,"activated",Q_ARG(int,i));QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);};
    auto number=[&](const char* key,int value){auto field=w.findChild<QSpinBox*>(key);QVERIFY(field);field->setFocus();field->selectAll();QTest::keyClicks(field,QString::number(value));QTest::keyClick(field,Qt::Key_Return);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);};
    auto link=[&](QString target,QString label,int x,int y,QString *id){w.findChild<QAction*>("addButton")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);*id=w.selectedId();auto text=w.findChild<QLineEdit*>("buttonText");text->setFocus();text->selectAll();QTest::keyClicks(text,label);QTest::keyClick(text,Qt::Key_Return);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);activate("actionTarget",target);number("elementX",x);number("elementY",y);};
    const auto evidence=engine+"/.cache/qt-facilities-evidence";QDir().mkpath(evidence);
    QStringList links,backs;const QStringList kinds{"buy","sell","service","diet"};
    for(int k=0;k<4;k++){
      QTimer::singleShot(100,&w,[k]{auto d=qobject_cast<QInputDialog*>(QApplication::activeModalWidget());QVERIFY(d);d->setTextValue(QString("Facility %1").arg(k+1));d->accept();});
      w.findChild<QAction*>("newScreen")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
      QTimer::singleShot(100,&w,[&,k]{
        auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(d);QCOMPARE(d->objectName(),QString("facilitiesDialog"));QTimer::singleShot(15000,d,&QDialog::reject);
        auto kind=d->findChild<QComboBox*>("facilityKind");kind->setCurrentIndex(kind->findData(kinds[k]));
        if(k>0)QTest::mouseClick(d->findChild<QPushButton*>("addFacility"),Qt::LeftButton);
        auto location=d->findChild<QComboBox*>("facilityLocation");location->setCurrentIndex(location->findData("home"));
        d->findChild<QSpinBox*>("facilityMaxQuantity")->setValue(3);
        if(k<2){d->findChild<QSpinBox*>("facilityPrice")->setValue(k==0?5:3);}
        else{
          d->findChild<QLineEdit*>("facilityName")->setText(k==2?"Recovery treatment":"Balanced diet");
          QTest::mouseClick(d->findChild<QPushButton*>("addFacilityCost"),Qt::LeftButton);
          auto costs=d->findChild<QTableWidget*>("facilityCosts");auto attr=qobject_cast<QComboBox*>(costs->cellWidget(0,0));attr->setCurrentIndex(attr->findData("money"));qobject_cast<QSpinBox*>(costs->cellWidget(0,1))->setValue(k==2?4:2);
          QTest::mouseClick(d->findChild<QPushButton*>("addFacilityEffect"),Qt::LeftButton);
          auto effects=d->findChild<QTableWidget*>("facilityEffects");auto effect=qobject_cast<QComboBox*>(effects->cellWidget(0,0));effect->setCurrentIndex(effect->findData(k==2?"stamina":"knowledge"));qobject_cast<QSpinBox*>(effects->cellWidget(0,2))->setValue(k==2?10:1);
          if(k==3)d->findChild<QSpinBox*>("facilityInterval")->setValue(2);
        }
        d->findChild<QLineEdit*>("facilityConfirm-title")->setText(QString("Confirm %1").arg(kinds[k]));
        d->findChild<QLineEdit*>("facilityConfirm-message")->setText("Check quantity and cost before continuing.");
        d->findChild<QLineEdit*>("facilityConfirm-confirmLabel")->setText("Apply");d->findChild<QLineEdit*>("facilityConfirm-cancelLabel")->setText("Keep current state");
        if(k==2)d->grab().save(evidence+"/authoring.png");
        QTest::mouseClick(d->findChild<QPushButton*>("placeFacility"),Qt::LeftButton);
      });
      w.findChild<QAction*>("editFacilities")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
      QString back;link("home","Home",20,20,&back);backs.append(back);
      openScreen("home");QString button;link(QString("screen-%1").arg(k+1),QString("Open %1").arg(kinds[k]),40+k*190,630,&button);number("elementWidth",180);links.append(button);
    }
    for(auto d:w.currentView()["diagnostics"].toArray())QVERIFY2(d.toObject()["severity"]!="error",qPrintable(d.toObject()["message"].toString()));
    const auto authored=w.projectData();QCOMPARE(authored["gameplay"].toObject()["diets"].toArray().first().toObject()["intervalPeriods"].toInt(),2);
    QCOMPARE(authored["gameplay"].toObject()["services"].toArray().first().toObject()["costs"].toArray().first().toObject()["amount"].toInt(),4);
    w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY(!w.currentView()["dirty"].toBool());
    // A second editor session proves the GUI-authored records survive a fresh load.
    {StudioWindow reopened(engine);reopened.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!reopened.busy()&&!reopened.projectData().isEmpty(),20000);QCOMPARE(reopened.projectData(),authored);}
    QFile file(evidence+"/authored-project.json");QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(authored).toJson());file.close();
    QJsonArray steps;auto click=[&](QString id){steps.append(QJsonObject{{"click",id}});steps.append(QJsonObject{{"waitFrames",3}});};auto checkpoint=[&](QString id){steps.append(QJsonObject{{"checkpoint",id}});};
    click("start");checkpoint("initial");click(links[0]);checkpoint("before-buy");
    // Stable runtime IDs are shared by authored facility widgets and automation.
    click("more-buy-florist-flower");click("buy-florist-flower");checkpoint("buy-dialog");steps.append(QJsonObject{{"screenshot",evidence+"/buy-confirm.png"}});click("cancel-purchase");checkpoint("buy-cancelled");
    click("buy-florist-flower");click("confirm-purchase");checkpoint("bought");click(backs[0]);click(links[1]);checkpoint("before-sell");click("sell-florist-flower");click("cancel-purchase");checkpoint("sell-cancelled");click("sell-florist-flower");click("confirm-purchase");checkpoint("sold");
    click(backs[1]);click(links[2]);checkpoint("before-treatment");click("more-service-service-1");click("service-service-1");click("cancel-purchase");checkpoint("treatment-cancelled");click("service-service-1");steps.append(QJsonObject{{"screenshot",evidence+"/treatment-confirm.png"}});click("confirm-purchase");checkpoint("treated");
    click(backs[2]);click(links[3]);checkpoint("before-diet");click("diet-diet-1");click("cancel-purchase");checkpoint("diet-cancelled");click("diet-diet-1");click("confirm-purchase");checkpoint("diet-selected");steps.append(QJsonObject{{"key","F5"}});steps.append(QJsonObject{{"waitFrames",3}});checkpoint("saved");
    click(backs[3]);click(links[2]);click("service-service-1");click("confirm-purchase");checkpoint("after-save-treatment");steps.append(QJsonObject{{"key","F9"}});steps.append(QJsonObject{{"waitFrames",3}});checkpoint("restored");steps.append(QJsonObject{{"screenshot",evidence+"/restored.png"}});
    w.playAutomation=evidence+"/steps.json";w.playReport=evidence+"/player-report.json";QFile stepFile(w.playAutomation);QVERIFY(stepFile.open(QIODevice::WriteOnly));stepFile.write(QJsonDocument(steps).toJson());stepFile.close();
    QSignalSpy finished(&w,&StudioWindow::playerFinished);w.findChild<QAction*>("playProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(),60000);QCOMPARE(finished.first().first().toInt(),0);
    QFile resultFile(w.playReport);QVERIFY(resultFile.open(QIODevice::ReadOnly));const auto report=QJsonDocument::fromJson(resultFile.readAll()).object();QVERIFY(report["ok"].toBool());
    auto state=[&](QString name){for(auto s:report["steps"].toArray())if(s.toObject()["command"].toObject()["checkpoint"]==name)return s.toObject()["ui"].toObject()["state"].toObject();return QJsonObject{};};
    QCOMPARE(state("before-buy"),state("buy-cancelled"));QCOMPARE(state("before-treatment"),state("treatment-cancelled"));QCOMPARE(state("before-diet"),state("diet-cancelled"));
    QCOMPARE(state("before-sell"),state("sell-cancelled"));
    QCOMPARE(state("bought")["attributes"].toObject()["money"].toInt(),50);QCOMPARE(state("sold")["attributes"].toObject()["money"].toInt(),53);
    QCOMPARE(state("treated")["attributes"].toObject()["money"].toInt(),45);QCOMPARE(state("treated")["attributes"].toObject()["stamina"].toInt(),80);
    QCOMPARE(state("diet-selected")["gameplay"].toObject()["dietId"].toString(),QString("diet-1"));QCOMPARE(state("diet-selected")["attributes"],state("treated")["attributes"]);
    QCOMPARE(state("saved"),state("restored"));QVERIFY(state("after-save-treatment")["attributes"]!=state("saved")["attributes"]);
  }

  void StudioTests::facilityAuthoringGuards() {
    StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()));w.show();w.openProject(qEnvironmentVariable("RAISING_QT_PROJECT"));
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);const auto baseline=w.projectData();
    auto restore=qScopeGuard([&]{if(w.projectData()!=baseline){w.findChild<QAction*>("undoEdit")->trigger();QElapsedTimer t;t.start();while(w.busy()&&t.elapsed()<10000)QTest::qWait(20);}});
    QTimer::singleShot(100,&w,[]{auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(d);QTimer::singleShot(5000,d,&QDialog::reject);d->findChild<QSpinBox*>("facilityPrice")->setValue(1500000000);QTest::mouseClick(d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save),Qt::LeftButton);});
    w.findChild<QAction*>("editFacilities")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
    QCOMPARE(w.projectData()["gameplay"].toObject()["shops"].toArray().first().toObject()["offers"].toArray().first().toObject()["price"].toInt(),1500000000);
    const auto authored=w.projectData();
    QTimer::singleShot(100,&w,[]{
      auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(d);QTimer::singleShot(5000,d,&QDialog::reject);auto price=d->findChild<QSpinBox*>("facilityPrice");QCOMPARE(price->value(),1500000000);
      auto item=d->findChild<QComboBox*>("facilityItem");item->setCurrentIndex(item->findData("training-sword"));
      QTest::mouseClick(d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save),Qt::LeftButton);QVERIFY(d->isVisible());QVERIFY(!d->findChild<QLabel*>("facilityError")->text().isEmpty());
      item->setCurrentIndex(item->findData("flower"));d->findChild<QLineEdit*>("facilityShopName")->clear();
      QTest::mouseClick(d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save),Qt::LeftButton);QVERIFY(d->isVisible());QVERIFY(!d->findChild<QLabel*>("facilityError")->text().isEmpty());d->reject();
    });
    w.findChild<QAction*>("editFacilities")->trigger();QCOMPARE(w.projectData(),authored);
  }
