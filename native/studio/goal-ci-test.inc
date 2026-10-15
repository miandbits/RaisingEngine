void StudioTests::goalCiReview(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()),root=qEnvironmentVariable("RAISING_CI_PROJECT"),liveRun=qEnvironmentVariable("RAISING_GITHUB_RUN"),evidence=engine+(liveRun.isEmpty()?"/docs/evidence/goal-ci":"/docs/evidence/goal-github");
  QVERIFY(!root.isEmpty());QDir().mkpath(evidence);
  QFile source(root+"/reports/report.json");QVERIFY(source.open(QIODevice::ReadOnly));const auto original=source.readAll();source.close();
  StudioWindow w(engine);w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  w.findChild<QAction*>("goalCiOpen")->trigger();auto ci=w.findChild<QDialog*>("goalCiDialog");QVERIFY(ci);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  if(liveRun.isEmpty()){
    ci->findChild<QLineEdit*>("goalCiFile")->setText(root+"/reports/report.json");ci->findChild<QPushButton*>("goalCiLoad")->click();
  }else{
    ci->findChild<QLineEdit*>("goalGitHubRepository")->setText("miandbits/RaisingStudio");
    ci->findChild<QPushButton*>("goalGitHubRefresh")->click();auto runs=ci->findChild<QComboBox*>("goalGitHubRuns");
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&runs->count()>0,100000);
    int selected=-1;for(int page=0;page<5&&selected<0;page++){
      for(int i=0;i<runs->count();i++)if(QString::number(runs->itemData(i).toJsonObject().value("id").toDouble(),'f',0)==liveRun){selected=i;break;}
      if(selected<0){auto next=ci->findChild<QPushButton*>("goalGitHubNext");QVERIFY(next->isEnabled());next->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&runs->count()>0,100000);}
    }
    QVERIFY(selected>=0);runs->setCurrentIndex(selected);ci->findChild<QPushButton*>("goalGitHubFetch")->click();
    auto reports=ci->findChild<QComboBox*>("goalGitHubReports");QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&reports->count()>0,180000);
    QVERIFY(reports->itemData(0).toJsonObject().value("primary").toBool());
    QVERIFY(reports->itemText(0).contains("PASS"));
    int failed=-1;for(int i=0;i<reports->count();i++){auto r=reports->itemData(i).toJsonObject();if(r.value("entry").toString().startsWith("goal-ci-")&&r.value("entry").toString().endsWith("/reports/report.json")&&!r.value("ok").toBool()){failed=i;break;}}
    QVERIFY(failed>=0);reports->setCurrentIndex(failed);ci->grab().save(evidence+"/github-runs-and-reports.png");
    ci->findChild<QPushButton*>("goalGitHubOpen")->click();
  }
  auto routes=ci->findChild<QTableWidget*>("goalCiTable");QTRY_VERIFY_WITH_TIMEOUT(routes->rowCount()==4&&!w.busy()&&!w.lastGoalImpact.isEmpty(),60000);
  int row=-1;for(int i=0;i<routes->rowCount();i++)if(routes->item(i,0)->text().endsWith(" · ordinary")){row=i;break;}QVERIFY(row>=0);routes->selectRow(row);
  QCOMPARE(routes->item(row,1)->text(),tr("변화 있음"));QCOMPARE(routes->item(row,2)->text(),tr("변화 있음"));QVERIFY(routes->item(row,6)->text().startsWith("PASS"));
  if(!liveRun.isEmpty()){
    auto details=ci->findChild<QPlainTextEdit*>("goalCiDetails")->toPlainText();QVERIFY(details.contains("github.com/miandbits/RaisingStudio/actions/runs/"+liveRun));QVERIFY(details.contains(tr("테스트/기타 보고서")));
    auto filter=ci->findChild<QCheckBox*>("goalCiOnlyFailed");filter->setChecked(true);QVERIFY(!routes->isRowHidden(row));filter->setChecked(false);
  }
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
