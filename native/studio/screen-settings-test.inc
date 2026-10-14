#include <QDirIterator>
#include <QTemporaryDir>

namespace {
bool copyScreenSettingsFixture(const QString &source,const QString &target){
  QDirIterator it(source,QDir::Files|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);
  while(it.hasNext()){const auto file=it.next(),destination=target+"/"+QDir(source).relativeFilePath(file);if(!QDir().mkpath(QFileInfo(destination).absolutePath())||!QFile::copy(file,destination))return false;}return true;
}
QJsonObject settingsScreen(const StudioWindow &w,const QString &id){
  for(auto value:w.projectData().value("visual").toObject().value("screens").toArray())if(value.toObject().value("id")==id)return value.toObject();return {};
}
void selectSettingsScreen(StudioWindow &w,const QString &id){
  auto list=w.findChild<QListWidget*>("projectList");for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole)==id){list->setCurrentRow(i);return;}
}
}

void StudioTests::screenSettingsRoundtrip(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());QTemporaryDir temp(engine+"/.cache/qt-screen-settings-XXXXXX");QVERIFY(temp.isValid());QVERIFY(copyScreenSettingsFixture(engine+"/examples/academy-visual",temp.path()));
  StudioWindow w(engine);w.show();w.openProject(temp.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);selectSettingsScreen(w,"title");
  const auto before=w.projectData(),oldScreen=settingsScreen(w,"title");const auto oldElements=oldScreen.value("elements").toArray();QVERIFY(!oldElements.empty());
  bool edited=false;QTimer::singleShot(100,&w,[&]{
    auto d=w.findChild<QDialog*>("screenSettingsDialog");QVERIFY(d);QTimer::singleShot(15000,d,&QDialog::reject);
    d->findChild<QLineEdit*>("screenSettingsName")->setText("Sales screen");auto mode=d->findChild<QComboBox*>("screenBackgroundMode");mode->setCurrentIndex(mode->findData("color"));d->findChild<QLineEdit*>("screenBackgroundColor")->setText("#221133");
    d->findChild<QSpinBox*>("screenReferenceWidth")->setValue(1440);d->findChild<QSpinBox*>("screenReferenceHeight")->setValue(900);edited=true;d->findChild<QDialogButtonBox*>("screenSettingsButtons")->button(QDialogButtonBox::Save)->click();
  });w.findChild<QAction*>("screenSettings")->trigger();QVERIFY(edited);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  const auto changed=w.projectData(),newScreen=settingsScreen(w,"title");const auto newElements=newScreen.value("elements").toArray();QCOMPARE(newElements.size(),oldElements.size());QCOMPARE(newScreen.value("label").toString(),QString("Sales screen"));
  auto expected=oldElements[0].toObject();expected["kind"]="panel";expected["fill"]="#221133";expected["width"]=1440;expected["height"]=900;QCOMPARE(newElements[0].toObject(),expected);
  for(int i=1;i<oldElements.size();i++)QCOMPARE(newElements[i],oldElements[i]);
  QCOMPARE(changed.value("visual").toObject().value("width").toInt(),1440);QCOMPARE(changed.value("visual").toObject().value("height").toInt(),900);
  for(auto s:before.value("visual").toObject().value("screens").toArray())if(s.toObject().value("id")!="title")QCOMPARE(settingsScreen(w,s.toObject().value("id").toString()),s.toObject());
  w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),before);w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),changed);
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));w.openProject(temp.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),changed);selectSettingsScreen(w,"title");
  const auto revision=w.currentView().value("revision");QTimer::singleShot(100,&w,[&]{auto d=w.findChild<QDialog*>("screenSettingsDialog");QVERIFY(d);d->findChild<QDialogButtonBox*>("screenSettingsButtons")->button(QDialogButtonBox::Save)->click();});w.findChild<QAction*>("screenSettings")->trigger();QCOMPARE(w.currentView().value("revision"),revision);QCOMPARE(w.projectData(),changed);
}

void StudioTests::screenSettingsGuards(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());QTemporaryDir temp(engine+"/.cache/qt-screen-settings-XXXXXX");QVERIFY(temp.isValid());QVERIFY(copyScreenSettingsFixture(engine+"/examples/academy-visual",temp.path()));
  const auto file=temp.path()+"/project.json";QFile source(file);QVERIFY(source.open(QIODevice::ReadOnly));auto project=QJsonDocument::fromJson(source.readAll()).object();source.close();auto visual=project.value("visual").toObject();auto screens=visual.value("screens").toArray();
  screens.append(QJsonObject{{"id","settings-empty"},{"label","Empty"},{"elements",QJsonArray{QJsonObject{{"id","screen-background"},{"kind","panel"},{"x",0},{"y",0},{"width",visual.value("width")},{"height",visual.value("height")},{"visibleWhen","choice"},{"fill","#331122"}}}}});visual["screens"]=screens;project["visual"]=visual;QVERIFY(source.open(QIODevice::WriteOnly|QIODevice::Truncate));source.write(QJsonDocument(project).toJson());source.close();
  StudioWindow w(engine);w.show();w.openProject(temp.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);selectSettingsScreen(w,"settings-empty");const auto before=w.projectData();
  QTimer::singleShot(100,&w,[&]{auto d=w.findChild<QDialog*>("screenSettingsDialog");QVERIFY(d);QTimer::singleShot(15000,d,&QDialog::reject);auto mode=d->findChild<QComboBox*>("screenBackgroundMode");mode->setCurrentIndex(mode->findData("color"));d->findChild<QLineEdit*>("screenBackgroundColor")->setText("invalid");auto save=d->findChild<QDialogButtonBox*>("screenSettingsButtons")->button(QDialogButtonBox::Save);save->click();QVERIFY(!d->findChild<QLabel*>("screenSettingsError")->text().isEmpty());QVERIFY(d->isVisible());mode->setCurrentIndex(mode->findData("asset"));save->click();QVERIFY(d->isVisible());d->reject();});w.findChild<QAction*>("screenSettings")->trigger();QCOMPARE(w.projectData(),before);
  QTimer::singleShot(100,&w,[&]{auto d=w.findChild<QDialog*>("screenSettingsDialog");QVERIFY(d);auto mode=d->findChild<QComboBox*>("screenBackgroundMode");mode->setCurrentIndex(mode->findData("asset"));auto image=d->findChild<QComboBox*>("screenBackgroundAsset");QVERIFY(image->count()>1);image->setCurrentIndex(1);auto fit=d->findChild<QComboBox*>("screenBackgroundFit");fit->setCurrentIndex(fit->findData("contain"));d->findChild<QDialogButtonBox*>("screenSettingsButtons")->button(QDialogButtonBox::Save)->click();});w.findChild<QAction*>("screenSettings")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
  const auto elements=settingsScreen(w,"settings-empty").value("elements").toArray();QCOMPARE(elements.size(),2);QCOMPARE(elements[0].toObject().value("id").toString(),QString("screen-background-1"));QCOMPARE(elements[0].toObject().value("kind").toString(),QString("image"));QCOMPARE(elements[0].toObject().value("imageSource").toString(),QString("asset"));QCOMPARE(elements[0].toObject().value("fit").toString(),QString("contain"));QCOMPARE(elements[1],screens.last().toObject().value("elements").toArray()[0]);
  w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),before);
}
