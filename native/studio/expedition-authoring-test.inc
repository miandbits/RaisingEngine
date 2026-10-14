#include <QDirIterator>
#include <QTemporaryDir>

namespace {
bool copyExpeditionFixture(const QString &source,const QString &target){
  QDirIterator it(source,QDir::Files|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);
  while(it.hasNext()){const auto file=it.next();const auto destination=target+"/"+QDir(source).relativeFilePath(file);if(!QDir().mkpath(QFileInfo(destination).absolutePath())||!QFile::copy(file,destination))return false;}
  return true;
}
QJsonObject expeditionActivity(const StudioWindow &window,const QString &id){
  for(auto value:window.currentView().value("documents").toArray()){auto doc=value.toObject(),data=doc.value("data").toObject();if(doc.value("kind")=="activity"&&data.value("id")==id)return data;}return {};
}
}

void StudioTests::expeditionAuthoringRoundtrip(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());QTemporaryDir temporary(engine+"/.cache/qt-expeditions-XXXXXX");QVERIFY(temporary.isValid());QVERIFY(copyExpeditionFixture(engine+"/examples/academy-adventure",temporary.path()));
  StudioWindow w(engine);w.show();w.openProject(temporary.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  const auto before=w.projectData(),oldActivity=expeditionActivity(w,"rest");QVERIFY(!oldActivity.empty());const int oldExpeditions=before.value("systems").toObject().value("expeditions").toArray().size(),oldEnemies=before.value("systems").toObject().value("enemies").toArray().size();
  bool authored=false;QString newEnemy,newExpedition;
  QTimer::singleShot(100,&w,[&]{
    auto d=w.findChild<QDialog*>("expeditionAuthoringDialog");QVERIFY(d);QTimer::singleShot(15000,d,&QDialog::reject);
    auto tabs=d->findChild<QTabWidget*>("expeditionTabs");tabs->setCurrentIndex(1);d->findChild<QPushButton*>("expeditionEnemyAdd")->click();d->findChild<QLineEdit*>("expeditionEnemyName")->setText("Workshop Enemy");d->findChild<QSpinBox*>("expeditionEnemy-maxHp")->setValue(41);d->findChild<QPushButton*>("expeditionRewardAdd")->click();auto rewards=d->findChild<QTableWidget*>("expeditionRewards");QCOMPARE(rewards->rowCount(),1);qobject_cast<QSpinBox*>(rewards->cellWidget(0,1))->setValue(3);
    tabs->setCurrentIndex(0);auto enemy=d->findChild<QComboBox*>("expeditionNodeEnemy");for(int i=0;i<enemy->count();++i)if(enemy->itemText(i)=="Workshop Enemy")newEnemy=enemy->itemData(i).toString();QVERIFY(!newEnemy.isEmpty());
    d->findChild<QPushButton*>("expeditionAdd")->click();d->findChild<QLineEdit*>("expeditionName")->setText("Workshop Trail");d->findChild<QLineEdit*>("expeditionDescription")->setText("Preserved description");
    d->findChild<QPushButton*>("expeditionNodeAdd")->click();d->findChild<QLineEdit*>("expeditionNodeName")->setText("Clearing");enemy->setCurrentIndex(enemy->findData(newEnemy));auto backgrounds=d->findChild<QComboBox*>("expeditionNodeBackground");backgrounds->setCurrentIndex(backgrounds->findData(QStringLiteral("@clear")));
    auto nodes=d->findChild<QListWidget*>("expeditionNodes");QCOMPARE(nodes->count(),2);auto links=d->findChild<QListWidget*>("expeditionConnections");links->item(0)->setCheckState(Qt::Checked);nodes->setCurrentRow(0);links->item(1)->setCheckState(Qt::Checked);
    // Duplicating a whole graph preserves local node references and its description.
    d->findChild<QPushButton*>("expeditionDuplicate")->click();QCOMPARE(d->findChild<QLineEdit*>("expeditionDescription")->text(),QString("Preserved description"));QCOMPARE(nodes->count(),2);d->findChild<QPushButton*>("expeditionRemove")->click();
    tabs->setCurrentIndex(2);auto map=d->findChild<QTableWidget*>("expeditionActivityMappings");bool mapped=false;for(int row=0;row<map->rowCount();++row)if(map->item(row,0)->text()==oldActivity.value("label").toString()){auto combo=qobject_cast<QComboBox*>(map->cellWidget(row,1));for(int i=0;i<combo->count();++i)if(combo->itemText(i)=="Workshop Trail"){newExpedition=combo->itemData(i).toString();combo->setCurrentIndex(i);mapped=true;break;}}
    QVERIFY(mapped);authored=true;d->findChild<QDialogButtonBox*>("expeditionAuthoringButtons")->button(QDialogButtonBox::Save)->click();
  });
  w.findChild<QAction*>("editExpeditions")->trigger();QVERIFY(authored);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  auto changed=w.projectData(),systems=changed.value("systems").toObject();QCOMPARE(systems.value("expeditions").toArray().size(),oldExpeditions+1);QCOMPARE(systems.value("enemies").toArray().size(),oldEnemies+1);auto createdEnemy=systems.value("enemies").toArray().last().toObject();QCOMPARE(createdEnemy.value("maxHp").toInt(),41);QCOMPARE(createdEnemy.value("rewards").toObject().value("items").toArray()[0].toObject().value("quantity").toInt(),3);QCOMPARE(expeditionActivity(w,"rest").value("expeditionId").toString(),newExpedition);
  for(auto key:{"equipment","consumables","quests","relationshipStages","hero"})QCOMPARE(systems.value(key),before.value("systems").toObject().value(key));
  auto added=systems.value("expeditions").toArray().last().toObject();auto nodes=added.value("nodes").toArray();QCOMPARE(nodes[0].toObject().value("connections").toArray()[0],nodes[1].toObject().value("id"));QCOMPARE(nodes[1].toObject().value("connections").toArray()[0],nodes[0].toObject().value("id"));QCOMPARE(nodes[1].toObject().value("enemyId").toString(),newEnemy);QVERIFY(nodes[1].toObject().value("presentation").toObject().value("backgroundAssetId").isNull());
  for(auto value:w.currentView().value("diagnostics").toArray())QVERIFY2(value.toObject().value("severity")!="error",qPrintable(QString::fromUtf8(QJsonDocument(value.toObject()).toJson())));
  w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),before);QCOMPARE(expeditionActivity(w,"rest"),oldActivity);
  w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),changed);QCOMPARE(expeditionActivity(w,"rest").value("expeditionId").toString(),newExpedition);
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));w.openProject(temporary.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),changed);QCOMPARE(expeditionActivity(w,"rest").value("expeditionId").toString(),newExpedition);
  QTimer::singleShot(100,&w,[&]{auto d=w.findChild<QDialog*>("expeditionAuthoringDialog");QVERIFY(d);d->findChild<QPushButton*>("expeditionAdd")->click();d->reject();});w.findChild<QAction*>("editExpeditions")->trigger();QCOMPARE(w.projectData(),changed);
}

void StudioTests::expeditionAuthoringGuards(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());QTemporaryDir temporary(engine+"/.cache/qt-expeditions-guards-XXXXXX");QVERIFY(temporary.isValid());QVERIFY(copyExpeditionFixture(engine+"/examples/academy-adventure",temporary.path()));StudioWindow w(engine);w.show();w.openProject(temporary.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);const auto before=w.projectData();bool checked=false;
  QTimer::singleShot(100,&w,[&]{auto d=w.findChild<QDialog*>("expeditionAuthoringDialog");QVERIFY(d);QTimer::singleShot(15000,d,&QDialog::reject);d->findChild<QPushButton*>("expeditionAdd")->click();auto exit=d->findChild<QCheckBox*>("expeditionNodeExit");exit->setChecked(false);auto save=d->findChild<QDialogButtonBox*>("expeditionAuthoringButtons")->button(QDialogButtonBox::Save);save->click();QVERIFY(d->isVisible());QVERIFY(d->findChild<QLabel*>("expeditionAuthoringError")->text().contains(QString::fromUtf8("출구")));exit->setChecked(true);
    auto nodes=d->findChild<QListWidget*>("expeditionNodes");d->findChild<QPushButton*>("expeditionNodeRemove")->click();QCOMPARE(nodes->count(),1);d->findChild<QPushButton*>("expeditionNodeAdd")->click();nodes->setCurrentRow(0);d->findChild<QListWidget*>("expeditionConnections")->item(1)->setCheckState(Qt::Checked);nodes->setCurrentRow(1);d->findChild<QPushButton*>("expeditionNodeRemove")->click();QCOMPARE(nodes->count(),2);QVERIFY(d->findChild<QLabel*>("expeditionAuthoringError")->text().contains(QString::fromUtf8("연결")));
    nodes->setCurrentRow(0);d->findChild<QListWidget*>("expeditionConnections")->item(1)->setCheckState(Qt::Unchecked);nodes->setCurrentRow(1);d->findChild<QPushButton*>("expeditionNodeRemove")->click();QCOMPARE(nodes->count(),1);w.openProject(engine+"/examples/academy-adventure");QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);save->click();QVERIFY(d->isVisible());QVERIFY(d->findChild<QLabel*>("expeditionAuthoringError")->text().contains(QString::fromUtf8("변경되었습니다")));checked=true;d->reject();});
  w.findChild<QAction*>("editExpeditions")->trigger();QVERIFY(checked);QCOMPARE(w.projectData(),before);QVERIFY(!w.currentView().value("dirty").toBool());
}
