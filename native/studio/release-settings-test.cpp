#include "studio.hpp"
#include "release-settings.hpp"
#include "editor-language.hpp"
#include <QAction>
#include <QDirIterator>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
namespace {
bool copyProject(const QString& source,const QString& target){QDirIterator it(source,QDir::Files|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);while(it.hasNext()){auto from=it.next(),to=target+"/"+QDir(source).relativeFilePath(from);if(!QDir().mkpath(QFileInfo(to).absolutePath())||!QFile::copy(from,to))return false;}return true;}
QJsonObject manifest(StudioWindow& w){for(auto value:w.currentView()["documents"].toArray()){const auto doc=value.toObject();if(doc["kind"]=="assets")return doc["data"].toObject();}return {};}
}
class ReleaseSettingsTests:public QObject {
 Q_OBJECT
private slots:
 void initTestCase(){QVERIFY(installEditorLanguage(*qApp,qEnvironmentVariable("RAISING_TEST_EDITOR_LOCALE","en")));QDir().mkpath(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath())+"/.cache/qt-release-settings");}
 void settingsRoundtripUndoCancel(){
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());QTemporaryDir temp(engine+"/.cache/qt-release-project-XXXXXX");QVERIFY(temp.isValid());QVERIFY(copyProject(engine+"/examples/my-first-raising-game",temp.path()));
  QFile notice(temp.path()+"/notice-input.txt");QVERIFY(notice.open(QIODevice::WriteOnly));const QByteArray original="Copyright Synthetic Author\r\nSynthetic permission.\r\n";QCOMPARE(notice.write(original),original.size());notice.close();QImage icon(1024,1024,QImage::Format_RGB32);icon.fill(QColor("#a94832"));QVERIFY(icon.save(temp.path()+"/icon-input.png"));
  StudioWindow w(engine);w.show();w.openProject(temp.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);const auto before=manifest(w);const auto project=w.projectData();
  auto action=w.findChild<QAction*>("projectSettings");QVERIFY(action);bool edited=false;
  QTimer::singleShot(100,&w,[&]{auto d=w.findChild<QDialog*>("projectSettingsDialog");QVERIFY(d);QTimer::singleShot(15000,d,&QDialog::reject);auto settings=static_cast<StudioRelease::Settings*>(d->findChild<QScrollArea*>("projectReleaseSettings"));QVERIFY(settings);auto tabs=d->findChild<QTabWidget*>();tabs->setCurrentWidget(settings);settings->name->setText("The Small Garden");settings->selectIcon(temp.path()+"/icon-input.png");auto item=new QListWidgetItem("Synthetic notice",settings->notices);item->setData(Qt::UserRole,QJsonObject{{"source",temp.path()+"/notice-input.txt"}});QVERIFY(settings->backup->isEnabled());QTest::qWait(50);QCOMPARE(settings->iconLabel->text(),QString("icon-input.png"));QVERIFY(settings->iconLabel->height()>=settings->iconLabel->fontMetrics().height());QVERIFY(d->grab().save(engine+"/.cache/qt-release-settings/settings-"+editorLanguage()+".png"));settings->ensureWidgetVisible(settings->backup);QTest::qWait(50);const auto buttonRect=QRect(settings->backup->mapTo(settings->viewport(),QPoint()),settings->backup->size());QVERIFY(settings->viewport()->rect().contains(buttonRect));QVERIFY(d->grab().save(engine+"/.cache/qt-release-settings/backup-reachable-"+editorLanguage()+".png"));edited=true;d->findChild<QDialogButtonBox*>("projectSettingsButtons")->button(QDialogButtonBox::Ok)->click();});action->trigger();QVERIFY(edited);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));auto after=manifest(w);QCOMPARE(after["release"].toObject()["displayName"].toString(),QString("The Small Garden"));QCOMPARE(after["assets"],before["assets"]);QCOMPARE(w.projectData(),project);
  w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(manifest(w),before);w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(manifest(w),after);
  w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.currentView()["dirty"].toBool(),20000);w.openProject(temp.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);QCOMPARE(manifest(w),after);const auto saved=after["release"].toObject()["notices"].toArray().first().toObject();QFile savedNotice(temp.path()+"/"+saved["path"].toString());QVERIFY(savedNotice.open(QIODevice::ReadOnly));QCOMPARE(savedNotice.readAll(),original);
  QTimer::singleShot(100,&w,[&]{auto d=w.findChild<QDialog*>("projectSettingsDialog");QVERIFY(d);static_cast<StudioRelease::Settings*>(d->findChild<QScrollArea*>("projectReleaseSettings"))->name->setText("Cancelled");d->reject();});action->trigger();QCOMPARE(manifest(w),after);QVERIFY(!w.currentView()["dirty"].toBool());
 }
 void verificationSelectionInvalidatesReceipt(){
  QWidget host;bool requested=false,completed=false;QTimer::singleShot(100,&host,[&]{auto d=host.findChild<QDialog*>("releaseBackupDialog");QVERIFY(d);QTimer::singleShot(10000,d,&QDialog::reject);auto file=d->findChild<QLineEdit*>("releaseBackupFile");auto restore=d->findChild<QPushButton*>("releaseBackupRestore");QVERIFY(!restore->isEnabled());file->setText("/synthetic/backup.zip");d->findChild<QPushButton*>("releaseBackupVerify")->click();QVERIFY(restore->isEnabled());file->setText("/synthetic/changed.zip");QVERIFY(!restore->isEnabled());completed=true;d->reject();});
  StudioRelease::restoreDialog(&host,[&](QString method,QJsonObject params,std::function<void(QJsonObject)> done){QCOMPARE(method,QString("releaseBackupVerify"));QCOMPARE(params["file"].toString(),QString("/synthetic/backup.zip"));requested=true;done({{"backupVerification",QJsonObject{{"ok",true},{"archiveSha256",QString(64,'a')},{"sourceFilesVerified",10},{"documentsReopened",5},{"pluginValidation","not-needed"}}}});});QVERIFY(requested&&completed);
 }
};
QTEST_MAIN(ReleaseSettingsTests)
#include "release-settings-test.moc"
