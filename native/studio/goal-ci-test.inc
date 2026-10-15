void StudioTests::goalCiReview(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()),root=qEnvironmentVariable("RAISING_CI_PROJECT"),evidence=engine+"/docs/evidence/goal-ci";
  QVERIFY(!root.isEmpty());QDir().mkpath(evidence);
  QFile source(root+"/reports/report.json");QVERIFY(source.open(QIODevice::ReadOnly));const auto original=source.readAll();source.close();
  StudioWindow w(engine);w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  w.findChild<QAction*>("goalCiOpen")->trigger();auto ci=w.findChild<QDialog*>("goalCiDialog");QVERIFY(ci);
  ci->findChild<QLineEdit*>("goalCiFile")->setText(root+"/reports/report.json");ci->findChild<QPushButton*>("goalCiLoad")->click();
  auto routes=ci->findChild<QTableWidget*>("goalCiTable");QTRY_VERIFY_WITH_TIMEOUT(routes->rowCount()==4&&!w.busy()&&!w.lastGoalImpact.isEmpty(),60000);
  int row=-1;for(int i=0;i<routes->rowCount();i++)if(routes->item(i,0)->text().endsWith(" · ordinary")){row=i;break;}QVERIFY(row>=0);routes->selectRow(row);
  QCOMPARE(routes->item(row,1)->text(),tr("변화 있음"));QCOMPARE(routes->item(row,2)->text(),tr("변화 있음"));QVERIFY(routes->item(row,6)->text().startsWith("PASS"));
  ci->grab().save(evidence+"/imported-ci.png");auto jump=ci->findChild<QPushButton*>("goalCiGraph");QVERIFY(jump->isEnabled());jump->click();
  QTRY_VERIFY_WITH_TIMEOUT(w.findChild<QDialog*>("goalCompareDialog")&&!w.busy(),20000);auto graph=w.findChild<QDialog*>("goalCompareDialog");auto values=graph->findChild<QTableWidget*>("goalGraphReviewValues");
  QTRY_VERIFY_WITH_TIMEOUT(values->rowCount()>0&&!w.busy(),20000);QCOMPARE(graph->findChild<QSpinBox*>("goalCompareRightStep")->value(),0);
  int money=-1;for(int i=0;i<values->rowCount();i++)if(values->item(i,2)->text()==tr("소지금")){money=i;break;}QVERIFY(money>=0);
  QCOMPARE(values->item(money,5)->text().toInt(),values->item(money,3)->text().toInt()+1);values->item(money,0)->setCheckState(Qt::Checked);
  auto approve=graph->findChild<QPushButton*>("goalGraphApprove");QVERIFY(approve->isEnabled());graph->findChild<QLineEdit*>("goalGraphReviewNote")->setText(tr("CI에서 확인한 초기 자금 조정"));approve->click();
  QTRY_VERIFY_WITH_TIMEOUT(routes->item(row,2)->text()==tr("동일")&&!w.busy(),60000);QCOMPARE(routes->item(row,1)->text(),tr("변화 있음"));QVERIFY(routes->item(row,6)->text().startsWith("PASS"));
  QTRY_VERIFY_WITH_TIMEOUT(values->item(money,1)->text()==tr("승인됨")&&!w.busy(),20000);graph->grab().save(evidence+"/approved-graph.png");
  auto history=graph->findChild<QComboBox*>("goalGraphHistory");int old=-1;for(int i=0;i<history->count();i++)if(history->itemText(i).startsWith(tr("부분 승인 0개"))){old=i;break;}QVERIFY(old>=0);history->setCurrentIndex(old);
  auto restore=graph->findChild<QPushButton*>("goalGraphRestore");QVERIFY(restore->isEnabled());restore->click();
  QTRY_VERIFY_WITH_TIMEOUT(routes->item(row,2)->text()==tr("변화 있음")&&!w.busy(),60000);graph->close();
  const auto previous=w.lastGoalImpact.value("id");ci->findChild<QPushButton*>("goalCiRecheck")->click();
  QTRY_VERIFY_WITH_TIMEOUT(w.lastGoalImpact.value("id")!=previous&&!w.busy(),60000);QCOMPARE(routes->item(row,2)->text(),tr("변화 있음"));QVERIFY(routes->item(row,6)->text().startsWith("PASS"));ci->grab().save(evidence+"/restored-revalidated.png");
  QVERIFY(source.open(QIODevice::ReadOnly));QCOMPARE(source.readAll(),original);
  QFile out(evidence+"/gui-verification.json");QVERIFY(out.open(QIODevice::WriteOnly));out.write(QJsonDocument(QJsonObject{{"ok",true},{"project",root},{"importReport",true},{"jumpToFirstDifference",true},{"graphPartialApproval",true},{"restoreBaseline",true},{"recheckSaveLoad",true},{"immutableCiReport",true}}).toJson());
}
