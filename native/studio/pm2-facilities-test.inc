#include <QDialogButtonBox>

void StudioTests::pm2FacilitiesPolish() {
  const auto root=qEnvironmentVariable("RAISING_QT_FACILITY_PM2");
  if(root.isEmpty())QSKIP("Set RAISING_QT_FACILITY_PM2 to the GUI-authored PM2 facilities sample.");
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  StudioWindow w(engine);w.resize(1440,980);w.show();w.openProject(root);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  QVERIFY(!w.projectData()["gameplay"].toObject()["services"].toArray().isEmpty());
  const auto evidence=engine+"/.cache/pm2-facilities-evidence";
  auto openScreen=[&](QString id){auto list=w.findChild<QListWidget*>("projectList");for(int i=0;i<list->count();i++)if(list->item(i)->data(Qt::UserRole)==id){auto entry=list->item(i);list->scrollToItem(entry);QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(entry).center());QTest::mouseDClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(entry).center());return;}QFAIL("Missing PM2 screen");};
  auto select=[&](QString id){auto tree=w.findChild<QTreeWidget*>("hierarchy");for(int r=0;r<tree->topLevelItemCount();r++)for(int i=0;i<tree->topLevelItem(r)->childCount();i++){auto item=tree->topLevelItem(r)->child(i);if(item->data(0,Qt::UserRole)==id){tree->scrollToItem(item);QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(item).center());QCOMPARE(w.selectedId(),id);return;}}QFAIL("Missing PM2 element");};
  auto text=[&](const char* key,QString value){auto field=w.findChild<QLineEdit*>(key);QVERIFY(field);field->setText(value);QMetaObject::invokeMethod(field,"editingFinished");QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);};
  auto number=[&](const char* key,int value){auto field=w.findChild<QSpinBox*>(key);QVERIFY(field);field->setFocus();field->selectAll();QTest::keyClicks(field,QString::number(value));QTest::keyClick(field,Qt::Key_Return);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);};
  auto mode=w.findChild<QComboBox*>("viewportMode");QVERIFY(mode);const int fit=mode->findData("fit");QVERIFY(fit>=0);mode->setCurrentIndex(fit);QMetaObject::invokeMethod(mode,"activated",Q_ARG(int,fit));QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  const auto screens=w.projectData()["visual"].toObject()["screens"].toArray();
  for(auto s:screens){const auto screen=s.toObject();for(auto e:screen["elements"].toArray()){
    const auto element=e.toObject();if(!QStringList{"diets","services","sell"}.contains(element["kind"].toString()))continue;
    openScreen(screen["id"].toString());select(element["id"].toString());
    text("elementFill","#201b2bee");text("elementColor","#fff2d0");number("elementFont",24);
    if(element["kind"]=="sell"){number("elementX",160);number("elementY",160);number("elementWidth",960);number("elementHeight",400);}
    w.grab().save(evidence+"/editor-"+element["kind"].toString()+"-polished.png");
  }}
  openScreen("diet");select("prompt");text("buttonText",QStringLiteral("식단을 선택하면 다음 일정부터\n매 기간 10G를 지불하고\n체력이 5 회복됩니다."));number("elementFont",25);
  openScreen("hospital");select("prompt");text("buttonText",QStringLiteral("진료 1회당 25G입니다.\n체력 +15, 스트레스 0으로 회복.\n수량을 선택한 뒤 확인해 주세요."));number("elementFont",25);
  // Diet selection belongs to home. The city's old screen-only shortcut left
  // the character in the city and produced an empty, unavailable diet list.
  openScreen("city");select("town-diet");
  auto action=w.findChild<QComboBox*>("buttonAction");int visit=action->findData("visit");QVERIFY(visit>=0);action->setCurrentIndex(visit);QMetaObject::invokeMethod(action,"activated",Q_ARG(int,visit));QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  auto target=w.findChild<QComboBox*>("actionTarget");int home=target->findData("home");QVERIFY(home>=0);target->setCurrentIndex(home);QMetaObject::invokeMethod(target,"activated",Q_ARG(int,home));QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  text("buttonText",QStringLiteral("집에서 식단 관리"));
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY(!w.currentView()["dirty"].toBool());
  QCOMPARE(w.projectData()["visual"].toObject()["viewportMode"].toString(),QString("fit"));
  const auto authored=w.projectData();QFile content(evidence+"/polished-project.json");QVERIFY(content.open(QIODevice::WriteOnly));content.write(QJsonDocument(authored).toJson());content.close();
  {StudioWindow reopened(engine);reopened.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!reopened.busy()&&!reopened.projectData().isEmpty(),20000);QCOMPARE(reopened.projectData(),authored);}
  QFile original(evidence+"/steps.json");QVERIFY(original.open(QIODevice::ReadOnly));const auto prior=QJsonDocument::fromJson(original.readAll()).array();QJsonArray steps;
  for(auto v:prior){auto step=v.toObject();if(step.contains("screenshot")){auto file=step["screenshot"].toString();file.replace(".png","-polished.png");step["screenshot"]=file;}steps.append(step);const auto checkpoint=step["checkpoint"].toString();if(checkpoint=="before-sale"||checkpoint=="before-treatment")steps.append(QJsonObject{{"screenshot",evidence+"/"+checkpoint+"-polished.png"}});}
  w.playAutomation=evidence+"/steps-polished.json";w.playReport=evidence+"/player-report-polished.json";QFile stepFile(w.playAutomation);QVERIFY(stepFile.open(QIODevice::WriteOnly));stepFile.write(QJsonDocument(steps).toJson());stepFile.close();
  QSignalSpy finished(&w,&StudioWindow::playerFinished);w.findChild<QAction*>("playProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(),60000);QCOMPARE(finished.first().first().toInt(),0);
  QFile result(w.playReport);QVERIFY(result.open(QIODevice::ReadOnly));const auto report=QJsonDocument::fromJson(result.readAll()).object();QVERIFY(report["ok"].toBool());
  auto state=[&](QString name){for(auto s:report["steps"].toArray())if(s.toObject()["command"].toObject()["checkpoint"]==name)return s.toObject()["ui"].toObject()["state"].toObject();return QJsonObject{};};
  QCOMPARE(state("before-diet"),state("diet-cancelled"));QCOMPARE(state("before-treatment"),state("treatment-cancelled"));QCOMPARE(state("before-buy"),state("buy-cancelled"));QCOMPARE(state("before-sale"),state("sale-cancelled"));
  QCOMPARE(state("treated")["attributes"].toObject()["money"].toInt(),450);QCOMPARE(state("treated")["attributes"].toObject()["stamina"].toInt(),110);QCOMPARE(state("sold")["attributes"].toObject()["money"].toInt(),418);QCOMPARE(state("saved"),state("restored"));
  openScreen("home");w.grab().save(evidence+"/editor-home-polished.png");
}

void StudioTests::pm2FacilitiesSample() {
  const auto root=qEnvironmentVariable("RAISING_QT_FACILITY_PM2");
  if(root.isEmpty())QSKIP("Set RAISING_QT_FACILITY_PM2 to a disposable PM2 components project copy to author the permanent sample.");
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
  StudioWindow w(engine);w.resize(1440,980);w.show();w.openProject(root);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  QVERIFY2(w.projectData()["gameplay"].toObject()["services"].toArray().isEmpty(),"Refusing to overwrite an already authored facilities sample");
  const auto evidence=engine+"/.cache/pm2-facilities-evidence";QDir().mkpath(evidence);
  auto screen=[&](QString id){for(auto s:w.projectData()["visual"].toObject()["screens"].toArray())if(s.toObject()["id"]==id)return s.toObject();return QJsonObject{};};
  auto openScreen=[&](QString id){auto list=w.findChild<QListWidget*>("projectList");for(int i=0;i<list->count();i++)if(list->item(i)->data(Qt::UserRole)==id){auto entry=list->item(i);list->scrollToItem(entry);QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(entry).center());QTest::mouseDClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(entry).center());return;}QFAIL("Missing PM2 screen");};
  auto activate=[&](const char* key,QString data){auto field=w.findChild<QComboBox*>(key);QVERIFY(field);const int i=field->findData(data);QVERIFY(i>=0);field->setCurrentIndex(i);QMetaObject::invokeMethod(field,"activated",Q_ARG(int,i));QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);};
  auto number=[&](const char* key,int value){auto field=w.findChild<QSpinBox*>(key);QVERIFY(field);field->setFocus();field->selectAll();QTest::keyClicks(field,QString::number(value));QTest::keyClick(field,Qt::Key_Return);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);};
  auto link=[&](QString target,QString label,int x,int y,QString* id){w.findChild<QAction*>("addButton")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);*id=w.selectedId();auto field=w.findChild<QLineEdit*>("buttonText");field->setText(label);QMetaObject::invokeMethod(field,"editingFinished");QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);activate("actionTarget",target);number("elementX",x);number("elementY",y);number("elementWidth",280);number("elementHeight",56);};
  auto author=[&](QString kind,QString target,bool place){
    openScreen(target);
    QTimer::singleShot(100,&w,[&,kind,place]{
      auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(d);QCOMPARE(d->objectName(),QString("facilitiesDialog"));QTimer::singleShot(15000,d,&QDialog::reject);
      auto mode=d->findChild<QComboBox*>("facilityKind");mode->setCurrentIndex(mode->findData(kind));
      if(kind=="sell"||kind=="buy"){auto shop=d->findChild<QComboBox*>("facilityShop");shop->setCurrentIndex(shop->findData("general-store"));}
      if(kind!="buy")QTest::mouseClick(d->findChild<QPushButton*>("addFacility"),Qt::LeftButton);
      auto location=d->findChild<QComboBox*>("facilityLocation");location->setCurrentIndex(location->findData(kind=="diet"?"home":kind=="service"?"city":"shop"));
      if(kind=="buy"||kind=="sell"){
        auto item=d->findChild<QComboBox*>("facilityItem");item->setCurrentIndex(item->findData("tea"));
        d->findChild<QSpinBox*>("facilityPrice")->setValue(kind=="buy"?20:8);d->findChild<QSpinBox*>("facilityMaxQuantity")->setValue(9);
      }else{
        d->findChild<QLineEdit*>("facilityName")->setText(kind=="diet"?QStringLiteral("균형 잡힌 식단"):QStringLiteral("병원 진료"));
        d->findChild<QSpinBox*>("facilityMaxQuantity")->setValue(3);d->findChild<QSpinBox*>("facilityInterval")->setValue(1);
        QTest::mouseClick(d->findChild<QPushButton*>("addFacilityCost"),Qt::LeftButton);
        auto costs=d->findChild<QTableWidget*>("facilityCosts");auto attr=qobject_cast<QComboBox*>(costs->cellWidget(0,0));attr->setCurrentIndex(attr->findData("money"));qobject_cast<QSpinBox*>(costs->cellWidget(0,1))->setValue(kind=="diet"?10:25);
        QTest::mouseClick(d->findChild<QPushButton*>("addFacilityEffect"),Qt::LeftButton);
        auto effects=d->findChild<QTableWidget*>("facilityEffects");auto effect=qobject_cast<QComboBox*>(effects->cellWidget(0,0));effect->setCurrentIndex(effect->findData("stamina"));qobject_cast<QSpinBox*>(effects->cellWidget(0,2))->setValue(kind=="diet"?5:15);
        if(kind=="service"){
          QTest::mouseClick(d->findChild<QPushButton*>("addFacilityEffect"),Qt::LeftButton);
          auto stress=qobject_cast<QComboBox*>(effects->cellWidget(1,0));stress->setCurrentIndex(stress->findData("stress"));auto op=qobject_cast<QComboBox*>(effects->cellWidget(1,1));op->setCurrentIndex(op->findData("set"));qobject_cast<QSpinBox*>(effects->cellWidget(1,2))->setValue(0);
        }
      }
      const auto title=kind=="buy"?QStringLiteral("구매 확인"):kind=="sell"?QStringLiteral("판매 확인"):kind=="diet"?QStringLiteral("식단 변경"):QStringLiteral("진료 확인");
      d->findChild<QLineEdit*>("facilityConfirm-title")->setText(title);
      d->findChild<QLineEdit*>("facilityConfirm-message")->setText(kind=="diet"?QStringLiteral("다음 일정부터 체력 +5, 소지금 10G가 매 기간 적용됩니다."):QStringLiteral("수량과 비용·효과를 확인한 뒤 진행하세요."));
      d->findChild<QLineEdit*>("facilityConfirm-confirmLabel")->setText(QStringLiteral("진행"));d->findChild<QLineEdit*>("facilityConfirm-cancelLabel")->setText(QStringLiteral("취소"));
      d->grab().save(evidence+"/author-"+kind+".png");
      if(place)QTest::mouseClick(d->findChild<QPushButton*>("placeFacility"),Qt::LeftButton);
      else QTest::mouseClick(d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save),Qt::LeftButton);
    });
    w.findChild<QAction*>("editFacilities")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
    if(place){number("elementX",280);number("elementY",160);number("elementWidth",640);number("elementHeight",400);}
  };
  author("diet","diet",true);author("service","hospital",true);
  // Create a separate sale screen and link it using the same inspector as a designer.
  QTimer::singleShot(100,&w,[]{auto d=qobject_cast<QInputDialog*>(QApplication::activeModalWidget());QVERIFY(d);d->setTextValue(QStringLiteral("물품 판매"));d->accept();});
  w.findChild<QAction*>("newScreen")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  QString saleScreen;for(auto s:w.projectData()["visual"].toObject()["screens"].toArray())if(s.toObject()["label"].toString()==QStringLiteral("물품 판매"))saleScreen=s.toObject()["id"].toString();QVERIFY(!saleScreen.isEmpty());
  author("sell",saleScreen,true);QString saleBack;link("shop",QStringLiteral("상점으로"),40,620,&saleBack);
  openScreen("shop");QString saleLink;link(saleScreen,QStringLiteral("물품 판매"),964,660,&saleLink);
  author("buy","shop-buy",false);
  for(auto d:w.currentView()["diagnostics"].toArray())QVERIFY2(d.toObject()["severity"]!="error",qPrintable(d.toObject()["message"].toString()));
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY(!w.currentView()["dirty"].toBool());
  const auto authored=w.projectData();{StudioWindow reopened(engine);reopened.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!reopened.busy()&&!reopened.projectData().isEmpty(),20000);QCOMPARE(reopened.projectData(),authored);}
  QFile content(evidence+"/authored-project.json");QVERIFY(content.open(QIODevice::WriteOnly));content.write(QJsonDocument(authored).toJson());content.close();
  QJsonArray steps;auto click=[&](QString id){steps.append(QJsonObject{{"click",id}});steps.append(QJsonObject{{"waitFrames",3}});};auto checkpoint=[&](QString id){steps.append(QJsonObject{{"checkpoint",id}});};
  click("start");checkpoint("initial");click("go-diet");checkpoint("before-diet");click("diet-diet-1");click("cancel-purchase");checkpoint("diet-cancelled");click("diet-diet-1");click("confirm-purchase");checkpoint("diet-selected");steps.append(QJsonObject{{"screenshot",evidence+"/diet.png"}});
  click("back");click("go-city");click("town-hospital");checkpoint("before-treatment");click("more-service-service-1");click("service-service-1");click("cancel-purchase");checkpoint("treatment-cancelled");click("service-service-1");steps.append(QJsonObject{{"screenshot",evidence+"/hospital-confirm.png"}});click("confirm-purchase");checkpoint("treated");
  steps.append(QJsonObject{{"key","F5"}});steps.append(QJsonObject{{"waitFrames",3}});checkpoint("saved");click("back");click("town-shop");click("shop-buy");checkpoint("before-buy");click("more-buy-general-store-tea");click("buy-general-store-tea");click("cancel-purchase");checkpoint("buy-cancelled");click("buy-general-store-tea");click("confirm-purchase");checkpoint("bought");click("back");click(saleLink);checkpoint("before-sale");click("sell-general-store-tea");click("cancel-purchase");checkpoint("sale-cancelled");click("sell-general-store-tea");steps.append(QJsonObject{{"screenshot",evidence+"/sale-confirm.png"}});click("confirm-purchase");checkpoint("sold");
  steps.append(QJsonObject{{"key","F9"}});steps.append(QJsonObject{{"waitFrames",3}});checkpoint("restored");steps.append(QJsonObject{{"screenshot",evidence+"/restored.png"}});
  w.playAutomation=evidence+"/steps.json";w.playReport=evidence+"/player-report.json";QFile stepFile(w.playAutomation);QVERIFY(stepFile.open(QIODevice::WriteOnly));stepFile.write(QJsonDocument(steps).toJson());stepFile.close();
  QSignalSpy finished(&w,&StudioWindow::playerFinished);w.findChild<QAction*>("playProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(),60000);QCOMPARE(finished.first().first().toInt(),0);
  QFile result(w.playReport);QVERIFY(result.open(QIODevice::ReadOnly));const auto report=QJsonDocument::fromJson(result.readAll()).object();QVERIFY(report["ok"].toBool());
  auto state=[&](QString name){for(auto s:report["steps"].toArray())if(s.toObject()["command"].toObject()["checkpoint"]==name)return s.toObject()["ui"].toObject()["state"].toObject();return QJsonObject{};};
  QCOMPARE(state("before-diet"),state("diet-cancelled"));QCOMPARE(state("before-treatment"),state("treatment-cancelled"));QCOMPARE(state("before-buy"),state("buy-cancelled"));QCOMPARE(state("before-sale"),state("sale-cancelled"));
  QCOMPARE(state("diet-selected")["gameplay"].toObject()["dietId"].toString(),QString("diet-1"));QCOMPARE(state("treated")["attributes"].toObject()["money"].toInt(),450);QCOMPARE(state("treated")["attributes"].toObject()["stamina"].toInt(),110);
  QCOMPARE(state("bought")["attributes"].toObject()["money"].toInt(),410);QCOMPARE(state("sold")["attributes"].toObject()["money"].toInt(),418);QCOMPARE(state("sold")["gameplay"].toObject()["inventory"].toObject()["tea"].toInt(),1);QCOMPARE(state("saved"),state("restored"));
  openScreen("home");w.grab().save(evidence+"/editor-home.png");
}
