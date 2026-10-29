#include <QDockWidget>
#include "studio.hpp"
#include "validation-location.hpp"
#include <QTest>
#include <QApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QPointer>
#include <QPushButton>
#include <QJsonDocument>
#include <QTreeWidgetItemIterator>

class ValidationLocationTests:public QObject {
  Q_OBJECT
private slots:
  void pointerTransactions(){
    QJsonValue root=QJsonObject{{"a/b",QJsonObject{{"~key",QJsonArray{1,2}}}},{"kept","unchanged"}};
    QCOMPARE(ValidationLocation::value(root,"/a~1b/~0key/1"),QJsonValue(2));
    QVERIFY(ValidationLocation::replace(root,"/a~1b/~0key/1",3));QCOMPARE(ValidationLocation::value(root,"/a~1b/~0key/1"),QJsonValue(3));
    const auto before=root;QVERIFY(!ValidationLocation::replace(root,"/a~1b/~0key/-1",4));QVERIFY(!ValidationLocation::replace(root,"/a~1b/~0key/02",4));QVERIFY(!ValidationLocation::replace(root,"/a~2b",4));QVERIFY(!ValidationLocation::replace(root,"/missing/child",4));QCOMPARE(root,before);
    QVERIFY(ValidationLocation::replace(root,"/a~1b/new",true));QVERIFY(ValidationLocation::replace(root,"/a~1b/~0key/0",{},true));QCOMPARE(ValidationLocation::value(root,"/a~1b/~0key/0"),QJsonValue(3));QCOMPARE(ValidationLocation::value(root,"/kept"),QJsonValue("unchanged"));
  }
  void editorJourney(){
    const auto planFile=qEnvironmentVariable("RAISING_VALIDATION_PLAN");if(planFile.isEmpty())QSKIP("Set RAISING_VALIDATION_PLAN to a disposable project plan");
    QFile f(planFile);QVERIFY(f.open(QIODevice::ReadOnly));const auto plan=QJsonDocument::fromJson(f.readAll()).object();const auto evidence=plan["evidence"].toString();QDir().mkpath(evidence);
    StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()));w.resize(1440,900);w.show();w.openProject(plan["root"].toString());QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().empty(),30000);
    auto validate=w.findChild<QPushButton*>("validationRecheck");auto problems=w.findChild<QListWidget*>("problems");QVERIFY(validate&&problems);validate->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),30000);
    const QJsonValue original=w.currentView()["documents"];
    auto doc=[&](QString file){for(auto d:w.currentView()["documents"].toArray())if(d.toObject()["file"]==file)return d.toObject()["data"].toObject();return QJsonObject{};};
    int step=0;for(auto v:plan["repairs"].toArray()){
      auto repair=v.toObject();const auto reported=repair.value("diagnosticPointer").toString(repair["pointer"].toString());QListWidgetItem *item=nullptr;for(int i=0;i<problems->count();++i){auto d=problems->item(i)->data(Qt::UserRole).toJsonObject();if(d["file"]==repair["file"]&&d["pointer"]==reported){item=problems->item(i);break;}}
      QVERIFY2(item,qPrintable(repair["file"].toString()+repair["pointer"].toString()));
      QListWidgetItem stale;for(int role=Qt::UserRole;role<=Qt::UserRole+2;++role)stale.setData(role,item->data(role));
      QMetaObject::invokeMethod(problems,"itemActivated",Q_ARG(QListWidgetItem*,item));
      QPointer<QDialog> d=w.findChild<QDialog*>("validationLocationEditor");QVERIFY(d);QCOMPARE(d->findChild<QLabel*>("validationFieldPath")->text(),reported);
      if(reported!=repair["pointer"].toString()){auto tree=d->findChild<QTreeWidget*>("validationLocationTree");auto sibling=tree->currentItem()->parent();QVERIFY(sibling);QTreeWidgetItem *target=nullptr;for(int i=0;i<sibling->childCount();++i)if(sibling->child(i)->data(0,Qt::UserRole).toString()==repair["pointer"].toString())target=sibling->child(i);QVERIFY(target);tree->setCurrentItem(target);}
      if(step==0){d->reject();QTRY_VERIFY(!d);QCOMPARE(w.currentView()["documents"],original);QMetaObject::invokeMethod(problems,"itemActivated",Q_ARG(QListWidgetItem*,&stale));d=w.findChild<QDialog*>("validationLocationEditor");QVERIFY(d);}
      auto value=repair["value"];
      if(value.isDouble()){
        auto field=d->findChild<QLineEdit*>("validationNumberValue");field->setText("not-a-number");d->findChild<QPushButton*>("validationApplyRecheck")->click();QVERIFY(d->findChild<QPushButton*>("validationApplyRecheck")->isEnabled());QVERIFY(d->findChild<QLabel*>("validationRepairStatus")->text().contains("숫자"));field->setText(QString::number(value.toDouble(),'g',17));
      }else {
        auto choices=d->findChild<QComboBox*>("validationReference");const int at=choices->findData(value.toString());QVERIFY2(at>0,qPrintable(repair["pointer"].toString()));choices->setCurrentIndex(at);QMetaObject::invokeMethod(choices,"activated",Q_ARG(int,at));QCOMPARE(d->findChild<QPlainTextEdit*>("validationTextValue")->toPlainText(),value.toString());
      }
      QVERIFY(d->grab().save(evidence+QString("/field-%1.png").arg(step)));
      const auto before=doc(repair["file"].toString());d->findChild<QPushButton*>("validationApplyRecheck")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),30000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));QCOMPARE(ValidationLocation::value(doc(repair["file"].toString()),repair["pointer"].toString()),value);QVERIFY(d->findChild<QLabel*>("validationRepairStatus")->text().contains("검증 완료"));d->reject();QTRY_VERIFY(!d);
      const auto after=doc(repair["file"].toString());w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),30000);QCOMPARE(doc(repair["file"].toString()),before);w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),30000);QCOMPARE(doc(repair["file"].toString()),after);
      QMetaObject::invokeMethod(problems,"itemActivated",Q_ARG(QListWidgetItem*,&stale));QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),30000);QVERIFY(!w.findChild<QDialog*>("validationLocationEditor"));QVERIFY(w.findChild<QLabel*>("validationSummary")->text().contains("다시 선택"));++step;
    }
    for(auto d:w.currentView()["diagnostics"].toArray())QVERIFY2(d.toObject()["severity"]!="error",qPrintable(d.toObject()["message"].toString()));
    // Toolbar/panel validation must commit a still-focused property before validating.
    auto hierarchy=w.findChild<QTreeWidget*>("hierarchy");const auto firstScreen=w.projectData()["visual"].toObject()["screens"].toArray().first().toObject();QString unlocked;
    for(auto e:firstScreen["elements"].toArray())if(!e.toObject()["locked"].toBool()){unlocked=e.toObject()["id"].toString();break;}
    QVERIFY(!unlocked.isEmpty());hierarchy->clearSelection();for(QTreeWidgetItemIterator it(hierarchy);*it;++it)if((*it)->data(0,Qt::UserRole).toString()==unlocked&&(*it)->data(0,Qt::UserRole+1).toString()==firstScreen["id"].toString())(*it)->setSelected(true);
    auto width=w.findChild<QSpinBox*>("elementWidth");QVERIFY(width&&width->isEnabled());{
      const auto prior=w.projectData();const int n=width->value();if(auto dock=w.findChild<QDockWidget*>("inspectorDock")){dock->show();dock->raise();}w.raise();w.activateWindow();QApplication::setActiveWindow(&w);width->setFocus();QTRY_VERIFY_WITH_TIMEOUT(width->hasFocus(),5000);QTest::keyClick(width,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(width,QString::number(n-1));validate->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),30000);QVERIFY(w.projectData()!=prior);QVERIFY(w.findChild<QLabel*>("validationSummary")->text().contains("오류 0건"));w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),30000);QCOMPARE(w.projectData(),prior);validate->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),30000);
    }
    w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.currentView()["dirty"].toBool(),30000);
    const QJsonValue saved=w.currentView()["documents"];w.openProject(plan["root"].toString());QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),30000);QCOMPARE(w.currentView()["documents"],saved);validate->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),30000);QVERIFY(w.findChild<QLabel*>("validationSummary")->text().contains("오류 0건"));QVERIFY(w.grab().save(evidence+"/validated.png"));
    QFile result(evidence+"/verification.json");QVERIFY(result.open(QIODevice::WriteOnly));result.write(QJsonDocument(QJsonObject{{"repairs",step},{"exactLocations",true},{"cancel",true},{"undoRedo",true},{"staleResultsRejected",true},{"saveReopen",true},{"errors",0}}).toJson());
  }
  void missingFieldAndRemoval(){
    const QJsonObject source{{"id","test"},{"flags",QJsonObject{{"a/b",true},{"keep",7}}}};QJsonObject applied;int calls=0;
    auto apply=[&](QJsonObject data,std::function<void(QString)> done){applied=data;++calls;done("verified");};
    QPointer<QDialog> d=ValidationLocation::editor(nullptr,{{"file","test.json"},{"data",source}}, {},{{"pointer","/missing"}},apply);d->open();
    QCOMPARE(d->findChild<QLabel*>("validationFieldPath")->text(),QString("/missing"));d->findChild<QComboBox*>("validationValueType")->setCurrentIndex(2);d->findChild<QCheckBox*>("validationBooleanValue")->setChecked(true);d->findChild<QPushButton*>("validationApplyRecheck")->click();QCOMPARE(calls,1);QCOMPARE(applied["missing"],QJsonValue(true));QCOMPARE(applied["flags"],source["flags"]);d->reject();QTRY_VERIFY(!d);
    d=ValidationLocation::editor(nullptr,{{"file","test.json"},{"data",source}}, {},{{"pointer","/flags/a~1b"}},apply);d->open();d->findChild<QPushButton*>("validationRemoveRecheck")->click();QCOMPARE(calls,2);QCOMPARE(applied["flags"].toObject().size(),1);QCOMPARE(applied["flags"].toObject()["keep"],QJsonValue(7));d->reject();QTRY_VERIFY(!d);
  }
};
QTEST_MAIN(ValidationLocationTests)
#include "validation-location-test.moc"
