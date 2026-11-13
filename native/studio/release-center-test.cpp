#include "release-center.hpp"
#include "editor-language.hpp"
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidget>

class ReleaseCenterTests:public QObject {
  Q_OBJECT
  QJsonObject entry()const{return{{"id","release-1"},{"name","Earlier game"},{"directory","/synthetic/release"},{"entryRevision",1},
    {"identity",QJsonObject{{"version","1.2.0"},{"target","web"},{"contentFingerprint","sha256:release-content"},{"engineIdentity",QJsonObject{{"engineVersion","0.2.0"},{"source",QJsonObject{{"commit","release-source"}}}}}}},
    {"cases",QJsonArray{}},{"checks",QJsonObject{{"release",QJsonObject{{"status","pending"}}},{"recovery",QJsonObject{{"status","pending"}}}}},
    {"distribution",QJsonArray{QJsonObject{{"id","device"},{"label","Physical device"},{"status","pending"},{"note",""},{"kind","manual"}}}}};}
  static QJsonObject identity(){return{{"engineVersion","0.3.0"},{"source",QJsonObject{{"commit","current-service-source"},{"binaryVerified",false}}},{"distribution",QJsonObject{{"kind","source"},{"releaseVersion",QJsonValue::Null},{"channel","source"}}},{"launch",QJsonObject{{"entryPoint","package.json#scripts.studio"},{"command","npm run studio"}}}};}
  template<class T>T* find(QDialog* d,const char* name){auto widget=d->findChild<T*>(name);Q_ASSERT(widget);return widget;}
private slots:
  void initTestCase(){QVERIFY(installEditorLanguage(*qApp,qEnvironmentVariable("RAISING_TEST_EDITOR_LOCALE","en")));}
  void registrationBackgroundAndPersistentSamples(){
    QWidget host;QLineEdit authoring(&host);host.show();QJsonArray releases;QJsonObject job;QStringList methods;int polls=0;QJsonObject lastStart,lastCase;
    auto rpc=[&](QString method,QJsonObject p,StudioReleaseCenter::Done done){methods<<method;QJsonObject reply;
      if(method=="editorIdentity")reply["editorIdentity"]=identity();
      else {QCOMPARE(p.value("root").toString(),QString("/synthetic/current-project"));
        if(method=="releaseWorkflowList")reply["releaseWorkflow"]=QJsonObject{{"version",1},{"releases",releases},{"jobs",QJsonArray{}}};
        else if(method=="releaseWorkflowRegister"){QCOMPARE(p.value("directory").toString(),QString("/synthetic/release"));releases=QJsonArray{entry()};reply["releaseEntry"]=releases.first();}
        else if(method=="releaseWorkflowAddCase"){lastCase=p;auto e=releases.first().toObject();e["entryRevision"]=2;e["cases"]=QJsonArray{QJsonObject{{"id","case-1"},{"name",p["name"]},{"file",p["file"]},{"kind",p["kind"]},{"check",QJsonObject{{"status","pending"}}}}};releases=QJsonArray{e};reply["releaseEntry"]=e;}
        else if(method=="releaseWorkflowStart"){lastStart=p;job={{"id","job-1"},{"releaseId","release-1"},{"status","running"},{"message","Replaying named sample"}};reply["releaseJob"]=job;}
        else if(method=="releaseWorkflowStatus"){if(++polls>=2){job["status"]="completed";auto e=releases.first().toObject(),c=e["cases"].toArray().first().toObject();c["check"]=QJsonObject{{"status","passed"},{"message","Exact command replay"}};e["cases"]=QJsonArray{c};e["entryRevision"]=3;releases=QJsonArray{e};}reply["releaseJob"]=job;}
        else QFAIL(qPrintable(method));
      }QTimer::singleShot(15,&host,[done,reply]{done(reply);});
    };
    QPointer<QDialog> d=StudioReleaseCenter::create(&host,"/synthetic/engine","/synthetic/current-project",rpc,[]{});d->show();QVERIFY(!d->isModal());
    QTRY_VERIFY(find<QPushButton>(d,"releaseRegister")->isEnabled());find<QLineEdit>(d,"releaseDirectory")->setText("/synthetic/release");find<QLineEdit>(d,"releaseName")->setText("Earlier game");find<QPushButton>(d,"releaseRegister")->click();
    QTRY_COMPARE(find<QComboBox>(d,"releaseSelection")->count(),1);QVERIFY(find<QLabel>(d,"releaseSelectedIdentity")->text().contains("release-source"));QVERIFY(!find<QLabel>(d,"releaseSelectedIdentity")->text().contains("current-service-source"));QVERIFY(find<QLabel>(d,"releaseCurrentEditorIdentity")->text().contains("current-service-source"));
    find<QLineEdit>(d,"releaseCaseName")->setText("Early game");find<QLineEdit>(d,"releaseCaseFile")->setText("/original/early.json");find<QPushButton>(d,"releaseAddCase")->click();QTRY_COMPARE(find<QTreeWidget>(d,"releaseSaveCases")->topLevelItemCount(),1);QCOMPARE(lastCase["kind"].toString(),QString("original"));QCOMPARE(lastCase["entryRevision"].toInt(),1);
    find<QPushButton>(d,"releaseCheckAllCases")->click();QTRY_VERIFY(find<QPushButton>(d,"releaseCancelJob")->isEnabled());QCOMPARE(lastStart["entryRevision"].toInt(),2);QVERIFY(!lastStart.contains("caseId"));
    bool otherWork=false;QTimer::singleShot(30,&host,[&]{authoring.setText("Still editable");otherWork=true;});QTRY_VERIFY(otherWork);QCOMPARE(authoring.text(),QString("Still editable"));QTRY_COMPARE_WITH_TIMEOUT(find<QLabel>(d,"releaseWorkflowStatus")->property("jobStatus").toString(),QString("completed"),5000);QTRY_VERIFY(find<QPushButton>(d,"releaseCheckAllCases")->isEnabled());
    auto table=find<QTreeWidget>(d,"releaseSaveCases");QCOMPARE(table->topLevelItem(0)->text(3),QString("Exact command replay"));QVERIFY(find<QTreeWidget>(d,"releaseDistributionChecks")->topLevelItem(0)->text(1)!=table->topLevelItem(0)->text(2));
    d->close();QTRY_VERIFY(d.isNull());d=StudioReleaseCenter::create(&host,"/synthetic/engine","/synthetic/current-project",rpc,[]{});d->show();QTRY_COMPARE(find<QTreeWidget>(d,"releaseSaveCases")->topLevelItemCount(),1);QCOMPARE(find<QTreeWidget>(d,"releaseSaveCases")->topLevelItem(0)->text(0),QString("Early game"));QCOMPARE(find<QTreeWidget>(d,"releaseSaveCases")->topLevelItem(0)->text(3),QString("Exact command replay"));d->close();
  }
  void migrationUsesRestoredSelectedReleaseAndManualChecksStayExplicit(){
    QWidget host;auto e=entry();e["cases"]=QJsonArray{QJsonObject{{"id","old"},{"name","Old original"},{"kind","original"},{"file","/original/old.json"},{"sourceProject","/original/old-project"},{"check",QJsonObject{{"status","migration-required"}}}}};QJsonObject start,manual;
    auto rpc=[&](QString method,QJsonObject p,StudioReleaseCenter::Done done){QJsonObject reply;
      if(method=="editorIdentity")reply["editorIdentity"]=identity();
      else if(method=="releaseWorkflowList")reply["releaseWorkflow"]=QJsonObject{{"releases",QJsonArray{e}},{"jobs",QJsonArray{}}};
      else if(method=="releaseWorkflowStart"){start=p;QCOMPARE(p["kind"].toString(),QString("recovery"));auto checks=e["checks"].toObject();checks["release"]=QJsonObject{{"status","passed"}};checks["recovery"]=QJsonObject{{"status","passed"},{"restored",true},{"directory","/restored/selected-release"}};e["checks"]=checks;e["entryRevision"]=2;reply["releaseJob"]=QJsonObject{{"id","restore"},{"status","completed"}};}
      else if(method=="releaseWorkflowDistribution"){manual=p;e["distribution"]=QJsonArray{QJsonObject{{"id","device"},{"label","Physical device"},{"status",p["status"]},{"note",p["note"]},{"kind","manual"}}};reply["releaseEntry"]=e;}
      else QFAIL(qPrintable(method));done(reply);
    };
    auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());QPointer<QDialog> d=StudioReleaseCenter::create(&host,engine,"/different/current-project",rpc,[]{});d->show();QTRY_COMPARE(find<QTreeWidget>(d,"releaseSaveCases")->topLevelItemCount(),1);auto cases=find<QTreeWidget>(d,"releaseSaveCases");cases->setCurrentItem(cases->topLevelItem(0));QVERIFY(!find<QPushButton>(d,"releaseMigrateCase")->isEnabled());
    find<QTabWidget>(d,"releaseTabs")->setCurrentIndex(1);find<QPushButton>(d,"releaseRestoreBackup")->click();QVERIFY(start.isEmpty());find<QLineEdit>(d,"releaseRestoreDestination")->setText("/restored/selected-release");find<QPushButton>(d,"releaseRestoreBackup")->click();QCOMPARE(start["destination"].toString(),QString("/restored/selected-release"));QTRY_VERIFY(find<QPushButton>(d,"releaseMigrateCase")->isEnabled());
    find<QTabWidget>(d,"releaseTabs")->setCurrentIndex(0);find<QPushButton>(d,"releaseMigrateCase")->click();auto migration=d->findChild<QDialog*>("saveMigrationDialog");QVERIFY(migration);QCOMPARE(find<QLineEdit>(migration,"migrationTo")->text(),QString("/restored/selected-release"));QCOMPARE(find<QLineEdit>(migration,"migrationFrom")->text(),QString("/original/old-project"));QCOMPARE(find<QLineEdit>(migration,"migrationSave")->text(),QString("/original/old.json"));QVERIFY(!find<QCheckBox>(migration,"migrationReset")->isChecked());migration->close();
    auto releaseIdentity=e["identity"].toObject();releaseIdentity["sourceContentFingerprint"]="sha256:authored-source";e["identity"]=releaseIdentity;find<QPushButton>(d,"releaseRefresh")->click();QVERIFY(!find<QPushButton>(d,"releaseMigrateCase")->isEnabled());QVERIFY(find<QLabel>(d,"releaseMigrationHelp")->text().contains(QObject::tr("이 릴리스는 화면 변경 뒤에도 이전 저장 지문을 유지합니다. 복원 원본의 지문이 달라 저장 이전 검토를 열 수 없습니다. 등록한 저장의 직접 호환성 검사는 가능합니다.")));QVERIFY(find<QPushButton>(d,"releaseCheckAllCases")->isEnabled());
    find<QTabWidget>(d,"releaseTabs")->setCurrentIndex(2);auto checks=find<QTreeWidget>(d,"releaseDistributionChecks");checks->setCurrentItem(checks->topLevelItem(0));find<QComboBox>(d,"releaseDistributionStatus")->setCurrentIndex(1);find<QPlainTextEdit>(d,"releaseDistributionNote")->setPlainText("Designer reviewed device report A");QVERIFY(manual.isEmpty());find<QPushButton>(d,"releaseRecordDistribution")->click();QCOMPARE(manual["checkId"].toString(),QString("device"));QCOMPARE(manual["entryRevision"].toInt(),2);QCOMPARE(manual["status"].toString(),QString("passed"));QCOMPARE(manual["note"].toString(),QString("Designer reviewed device report A"));d->close();
  }
  void closureDoesNotCancelAndUnknownIdentityIsNotFabricated(){
    QWidget host;auto e=entry();QStringList calls;QJsonObject job{{"id","job"},{"releaseId","release-1"},{"status","running"}};
    auto rpc=[&](QString method,QJsonObject,StudioReleaseCenter::Done done){calls<<method;if(method=="editorIdentity"){done({{"editorIdentity",QJsonObject{{"engineVersion","0.3.0"},{"source",QJsonObject{{"commit",QJsonValue::Null},{"binaryVerified",false}}}}}});return;}if(method=="releaseWorkflowList")done({{"releaseWorkflow",QJsonObject{{"releases",QJsonArray{e}},{"jobs",QJsonArray{job}}}}});else if(method=="releaseWorkflowStatus")done({{"releaseJob",job}});else if(method=="releaseWorkflowCancel"){job["status"]="cancelled";done({{"releaseJob",job}});}else QFAIL(qPrintable(method));};
    QPointer<QDialog> d=StudioReleaseCenter::create(&host,"/engine","/project",rpc,[]{});d->show();QTRY_VERIFY(find<QPushButton>(d,"releaseCancelJob")->isEnabled());d->close();QTRY_VERIFY(d.isNull());QVERIFY(!calls.contains("releaseWorkflowCancel"));d=StudioReleaseCenter::create(&host,"/engine","/project",rpc,[]{});d->show();QTRY_VERIFY(find<QPushButton>(d,"releaseCancelJob")->isEnabled());find<QPushButton>(d,"releaseCancelJob")->click();QVERIFY(calls.contains("releaseWorkflowCancel"));d->close();
    auto info=StudioReleaseCenter::showIdentity(&host,rpc);QCOMPARE(find<QLabel>(info,"editorIdentitySummary")->property("sourceCommit").toString(),QString());QVERIFY(find<QLabel>(info,"editorIdentitySummary")->text().contains(QObject::tr("확인되지 않음")));QVERIFY(find<QLabel>(info,"editorIdentityScope")->text().contains(QObject::tr("서비스 소스 정보는 실행 파일 자체의 빌드 출처 검증을 뜻하지 않습니다.")));info->close();
  }
  void selectingAnotherReleaseRestoresItsRunningJob(){
    QWidget host;auto first=entry(),second=entry();second["id"]="release-2";second["name"]="Second release";second["directory"]="/synthetic/second-release";QJsonObject job{{"id","job-second"},{"releaseId","release-2"},{"status","running"}};QString cancelled;
    auto rpc=[&](QString method,QJsonObject p,StudioReleaseCenter::Done done){
      if(method=="editorIdentity")done({{"editorIdentity",identity()}});
      else if(method=="releaseWorkflowList")done({{"releaseWorkflow",QJsonObject{{"releases",QJsonArray{first,second}},{"jobs",QJsonArray{job}}}}});
      else if(method=="releaseWorkflowStatus")done({{"releaseJob",job}});
      else if(method=="releaseWorkflowStart"){job={{"id","new-job"},{"releaseId",p["releaseId"]},{"status","running"}};done({{"releaseJob",job}});}
      else if(method=="releaseWorkflowCancel"){cancelled=p["jobId"].toString();job["status"]="cancelled";done({{"releaseJob",job}});}else QFAIL(qPrintable(method));
    };
    QPointer<QDialog> d=StudioReleaseCenter::create(&host,"/engine","/project",rpc,[]{});d->show();QTRY_COMPARE(find<QComboBox>(d,"releaseSelection")->count(),2);auto selection=find<QComboBox>(d,"releaseSelection");auto selectedPath=find<QLineEdit>(d,"releaseSelectedDirectory");QVERIFY(selectedPath->isReadOnly());QCOMPARE(selectedPath->text(),QString("/synthetic/release"));find<QLineEdit>(d,"releaseDirectory")->setText("/unregistered/other-export");QVERIFY(!find<QPushButton>(d,"releaseCancelJob")->isEnabled());selection->setCurrentIndex(1);QCOMPARE(selectedPath->text(),QString("/synthetic/second-release"));QVERIFY(find<QPushButton>(d,"releaseOpenFolder")->isEnabled());QVERIFY(find<QPushButton>(d,"releaseCancelJob")->isEnabled());QVERIFY(selection->isEnabled());selection->setCurrentIndex(0);QCOMPARE(selectedPath->text(),QString("/synthetic/release"));QVERIFY(!find<QPushButton>(d,"releaseCancelJob")->isEnabled());selection->setCurrentIndex(1);find<QPushButton>(d,"releaseCancelJob")->click();QCOMPARE(cancelled,QString("job-second"));QVERIFY(!find<QPushButton>(d,"releaseCancelJob")->isEnabled());
    selection->setCurrentIndex(0);find<QPushButton>(d,"releaseCheckIntegrity")->click();QVERIFY(find<QPushButton>(d,"releaseCancelJob")->isEnabled());selection->setCurrentIndex(1);QVERIFY(!find<QPushButton>(d,"releaseCancelJob")->isEnabled());QVERIFY(find<QLabel>(d,"releaseWorkflowStatus")->property("jobStatus").toString().isEmpty());selection->setCurrentIndex(0);QVERIFY(find<QPushButton>(d,"releaseCancelJob")->isEnabled());QCOMPARE(find<QLabel>(d,"releaseWorkflowStatus")->property("jobStatus").toString(),QString("running"));find<QPushButton>(d,"releaseCancelJob")->click();QCOMPARE(cancelled,QString("new-job"));d->close();
  }
  void distributionKeyboardSelectionDoesNotInvokeAnotherAction(){
    QWidget host;auto e=entry();int lists=0;QJsonObject recorded;
    auto rpc=[&](QString method,QJsonObject p,StudioReleaseCenter::Done done){
      if(method=="editorIdentity")done({{"editorIdentity",identity()}});
      else if(method=="releaseWorkflowList"){++lists;done({{"releaseWorkflow",QJsonObject{{"releases",QJsonArray{e}},{"jobs",QJsonArray{}}}}});}
      else if(method=="releaseWorkflowDistribution"){recorded=p;auto rows=e["distribution"].toArray();auto row=rows[0].toObject();row["status"]=p["status"];row["note"]=p["note"];rows[0]=row;e["distribution"]=rows;done({{"releaseEntry",e}});}
      else QFAIL(qPrintable(method));
    };
    QPointer<QDialog> d=StudioReleaseCenter::create(&host,"/engine","/project",rpc,[]{});d->show();
    auto tree=find<QTreeWidget>(d,"releaseDistributionChecks");QTRY_COMPARE(tree->topLevelItemCount(),1);find<QTabWidget>(d,"releaseTabs")->setCurrentIndex(2);
    QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(tree->topLevelItem(0)).center());QVERIFY(tree->currentItem());
    auto combo=find<QComboBox>(d,"releaseDistributionStatus");combo->setFocus();QTest::mouseClick(combo,Qt::LeftButton);QTRY_VERIFY(combo->view()->isVisible());
    QTest::keyClick(combo->view(),Qt::Key_Down);QTest::keyClick(combo->view(),Qt::Key_Return);QTRY_VERIFY(!combo->view()->isVisible());QCOMPARE(combo->currentData().toString(),QString("passed"));
    // Return can also reach the focused combo after its popup closes; it must not refresh or publish.
    QTest::keyClick(combo,Qt::Key_Return);QCOMPARE(lists,1);QVERIFY(tree->currentItem());QCOMPARE(tree->currentItem()->data(0,Qt::UserRole).toJsonObject()["id"].toString(),QString("device"));QVERIFY(recorded.isEmpty());
    auto note=find<QPlainTextEdit>(d,"releaseDistributionNote");note->setFocus();QTest::keyClicks(note,"Actual report A");QTest::keyClick(note,Qt::Key_Return);QTest::keyClicks(note,"Pending physical acceptance");QCOMPARE(note->toPlainText(),QString("Actual report A\nPending physical acceptance"));QCOMPARE(lists,1);QVERIFY(recorded.isEmpty());
    QTest::mouseClick(find<QPushButton>(d,"releaseRecordDistribution"),Qt::LeftButton);QCOMPARE(recorded["checkId"].toString(),QString("device"));QCOMPARE(recorded["status"].toString(),QString("passed"));QCOMPARE(recorded["note"].toString(),note->toPlainText());d->close();
  }
  void sameReleaseRefreshAndJobCompletionPreserveDistributionDraft(){
    QWidget host;auto e=entry(),other=entry();other["id"]="release-2";other["name"]="Other release";QJsonObject recorded,job;int lists=0;
    auto rpc=[&](QString method,QJsonObject p,StudioReleaseCenter::Done done){
      if(method=="editorIdentity")done({{"editorIdentity",identity()}});
      else if(method=="releaseWorkflowList"){++lists;QTimer::singleShot(20,&host,[&,done]{done({{"releaseWorkflow",QJsonObject{{"releases",QJsonArray{e,other}},{"jobs",QJsonArray{}}}}});});}
      else if(method=="releaseWorkflowStart"){job={{"id","integrity-job"},{"releaseId","release-1"},{"status","running"}};done({{"releaseJob",job}});}
      else if(method=="releaseWorkflowStatus"){job["status"]="completed";e["entryRevision"]=3;done({{"releaseJob",job}});}
      else if(method=="releaseWorkflowDistribution"){recorded=p;auto rows=e["distribution"].toArray();auto row=rows[0].toObject();row["status"]=p["status"];row["note"]=p["note"];rows[0]=row;e["distribution"]=rows;e["entryRevision"]=4;done({{"releaseEntry",e}});}
      else QFAIL(qPrintable(method));
    };
    QPointer<QDialog> d=StudioReleaseCenter::create(&host,"/engine","/project",rpc,[]{});d->show();auto tree=find<QTreeWidget>(d,"releaseDistributionChecks");QTRY_COMPARE(tree->topLevelItemCount(),1);find<QTabWidget>(d,"releaseTabs")->setCurrentIndex(2);tree->setCurrentItem(tree->topLevelItem(0));
    auto combo=find<QComboBox>(d,"releaseDistributionStatus");auto note=find<QPlainTextEdit>(d,"releaseDistributionNote");combo->setCurrentIndex(combo->findData("passed"));note->setPlainText("Reviewed report");e["entryRevision"]=2;
    find<QPushButton>(d,"releaseRefresh")->click();note->setPlainText("Reviewed report with in-flight typing");auto cursor=note->textCursor();cursor.setPosition(8);note->setTextCursor(cursor);QTRY_VERIFY(find<QPushButton>(d,"releaseRefresh")->isEnabled());QCOMPARE(lists,2);QVERIFY(tree->currentItem());QCOMPARE(combo->currentData().toString(),QString("passed"));QCOMPARE(note->toPlainText(),QString("Reviewed report with in-flight typing"));QCOMPARE(note->textCursor().position(),8);QVERIFY(recorded.isEmpty());
    find<QPushButton>(d,"releaseCheckIntegrity")->click();QTRY_COMPARE_WITH_TIMEOUT(find<QLabel>(d,"releaseWorkflowStatus")->property("jobStatus").toString(),QString("completed"),3000);QTRY_VERIFY(find<QPushButton>(d,"releaseRefresh")->isEnabled());QVERIFY(tree->currentItem());QCOMPARE(combo->currentData().toString(),QString("passed"));QCOMPARE(note->toPlainText(),QString("Reviewed report with in-flight typing"));QVERIFY(recorded.isEmpty());
    find<QPushButton>(d,"releaseRecordDistribution")->click();QCOMPARE(recorded["entryRevision"].toInt(),3);QCOMPARE(recorded["checkId"].toString(),QString("device"));QCOMPARE(recorded["status"].toString(),QString("passed"));QCOMPARE(recorded["note"].toString(),QString("Reviewed report with in-flight typing"));QVERIFY(tree->currentItem());
    // With no local edits, refreshed evidence replaces the saved values instead of looking frozen.
    auto rows=e["distribution"].toArray();auto row=rows[0].toObject();row["status"]="failed";row["note"]="External review found a problem";rows[0]=row;e["distribution"]=rows;find<QPushButton>(d,"releaseRefresh")->click();QTRY_VERIFY(find<QPushButton>(d,"releaseRefresh")->isEnabled());QCOMPARE(combo->currentData().toString(),QString("failed"));QCOMPARE(note->toPlainText(),QString("External review found a problem"));
    // A matching checklist id in another release must never inherit this release's unsaved claim.
    combo->setCurrentIndex(combo->findData("passed"));note->setPlainText("Unsaved first-release claim");find<QComboBox>(d,"releaseSelection")->setCurrentIndex(1);QVERIFY(!tree->currentItem());QVERIFY(!find<QPushButton>(d,"releaseRecordDistribution")->isEnabled());QCOMPARE(combo->currentData().toString(),QString("pending"));QVERIFY(note->toPlainText().isEmpty());tree->setCurrentItem(tree->topLevelItem(0));QCOMPARE(combo->currentData().toString(),QString("pending"));QVERIFY(note->toPlainText().isEmpty());d->close();
  }
};
QTEST_MAIN(ReleaseCenterTests)
#include "release-center-test.moc"
