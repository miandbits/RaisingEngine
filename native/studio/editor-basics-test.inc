#include <QClipboard>
#include <QMimeData>

void StudioTests::editorBasicsClipboardLayers(){
  const auto root=qEnvironmentVariable("RAISING_BASICS_PROJECT");if(root.isEmpty())QSKIP("Set RAISING_BASICS_PROJECT to an isolated project");
  StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()));w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().empty(),20000);
  auto tree=w.findChild<QTreeWidget*>("hierarchy");QCOMPARE(tree->columnCount(),3);QVERIFY(!tree->isHeaderHidden());
  auto select=[&](QString id){for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole)==id){tree->setCurrentItem(*it);return;}QFAIL("Missing element");};
  auto elements=[&](QString screen){for(auto v:w.projectData()["visual"].toObject()["screens"].toArray())if(v.toObject()["id"]==screen)return v.toObject()["elements"].toArray();return QJsonArray();};
  auto panel=w.findChild<QDockWidget*>("hierarchyDock");panel->hide();w.findChild<QAction*>("showHierarchy")->trigger();QVERIFY(panel->isVisible());
  const auto beforeLayer=w.projectData();select("child-a");w.findChild<QAction*>("layerForward")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  QStringList childOrder;for(auto e:elements("home"))if(e.toObject()["parentId"]=="group-a")childOrder<<e.toObject()["id"].toString();QCOMPARE(childOrder,QStringList({"child-b","child-a"}));
  w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),beforeLayer);
  select("group-a");const auto original=w.projectData();
  auto copy=w.findChild<QAction*>("copyElements"),paste=w.findChild<QAction*>("pasteElements");QVERIFY(copy->isEnabled());
  tree->setFocus();QTest::keyClick(tree,Qt::Key_C,Qt::ControlModifier);QVERIFY(QApplication::clipboard()->mimeData()->hasFormat("application/x-raising-studio-elements"));
  auto list=w.findChild<QListWidget*>("projectList");QVERIFY(list);QListWidgetItem *target=nullptr;for(int i=0;i<list->count();i++)if(list->item(i)->data(Qt::UserRole)=="other")target=list->item(i);QVERIFY(target);
  QMetaObject::invokeMethod(list,"itemActivated",Q_ARG(QListWidgetItem*,target));QTest::qWait(30);
  const int before=elements("other").size();paste->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&elements("other").size()==before+3,10000);
  const auto parent=w.selectedId();QVERIFY(parent!="group-a");int children=0;QSet<QString> ids;for(auto v:elements("other")){auto e=v.toObject();QVERIFY(!ids.contains(e["id"].toString()));ids.insert(e["id"].toString());if(e["parentId"]==parent)children++;}QCOMPARE(children,2);
  const auto pasted=w.projectData();w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),original);
  w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),pasted);
  const auto evidence=qEnvironmentVariable("RAISING_BASICS_EVIDENCE");if(!evidence.isEmpty()){QDir().mkpath(evidence);QVERIFY(w.grab().save(evidence+"/hierarchy-clipboard.png"));}
  select(parent);w.findChild<QAction*>("layerBack")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(elements("other")[0].toObject()["id"].toString(),parent);
  w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),pasted);
  // Native text controls must retain text copy/paste instead of cloning UI elements.
  select(parent);auto name=w.findChild<QLineEdit*>("elementName");name->setFocus();name->setText("Text clipboard");name->selectAll();QTest::keyClick(name,Qt::Key_C,Qt::ControlModifier);QCOMPARE(QApplication::clipboard()->text(),QString("Text clipboard"));
  name->setText("");QTest::keyClick(name,Qt::Key_V,Qt::ControlModifier);QCOMPARE(name->text(),QString("Text clipboard"));name->clearFocus();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(elements("other").size(),before+3);
  // Foreign-project clipboard cannot silently introduce broken asset/action references.
  auto bad=new QMimeData;bad->setData("application/x-raising-studio-elements",R"({"format":1,"project":"another-project","elements":[{"id":"foreign"}],"rootId":"foreign"})");QApplication::clipboard()->setMimeData(bad);const auto unchanged=w.projectData();paste->trigger();QCOMPARE(w.projectData(),unchanged);
  w.findChild<QAction*>("validateProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY(w.findChild<QLabel*>("validationSummary")->text().contains("오류 0건"));
}

void StudioTests::editorBasicsSaveProtection(){
  const auto root=qEnvironmentVariable("RAISING_BASICS_PROJECT");if(root.isEmpty())QSKIP("Set RAISING_BASICS_PROJECT");
  auto answer=[](QMessageBox::StandardButton button){auto timer=new QTimer(qApp);timer->setInterval(30);QObject::connect(timer,&QTimer::timeout,timer,[timer,button]{auto box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());if(box&&box->objectName()=="unsavedChangesDialog"){timer->stop();timer->deleteLater();const auto evidence=qEnvironmentVariable("RAISING_BASICS_EVIDENCE");if(!evidence.isEmpty())box->grab().save(evidence+"/unsaved-changes.png");box->button(button)->click();}});QTimer::singleShot(15000,timer,&QObject::deleteLater);timer->start();};
  StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()));w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().empty(),20000);
  auto tree=w.findChild<QTreeWidget*>("hierarchy");for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole)=="child-a"){tree->setCurrentItem(*it);break;}
  auto field=w.findChild<QLineEdit*>("elementName");field->setFocus();field->setText("Close without leaving field");answer(QMessageBox::Cancel);
  QVERIFY(!w.close());QVERIFY(w.isVisible());QVERIFY(w.currentView()["dirty"].toBool());
  answer(QMessageBox::Save);QVERIFY(w.close());QVERIFY(!w.currentView()["dirty"].toBool());
  StudioWindow reopened(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()));reopened.show();reopened.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!reopened.busy()&&!reopened.projectData().empty(),20000);
  bool persisted=false;for(auto s:reopened.projectData()["visual"].toObject()["screens"].toArray())for(auto e:s.toObject()["elements"].toArray())if(e.toObject()["id"]=="child-a")persisted=e.toObject()["label"]=="Close without leaving field";QVERIFY(persisted);
  tree=reopened.findChild<QTreeWidget*>("hierarchy");for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole)=="child-a"){tree->setCurrentItem(*it);break;}
  field=reopened.findChild<QLineEdit*>("elementName");field->setFocus();field->setText("Focused save");reopened.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!reopened.busy()&&!reopened.currentView()["dirty"].toBool(),10000);
  // A disk conflict must fail Save-and-close, keeping the edited document alive.
  QFile file(root+"/project.json");QVERIFY(file.open(QIODevice::ReadOnly));auto disk=QJsonDocument::fromJson(file.readAll()).object();file.close();disk["title"]="External change";QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(disk).toJson());file.close();
  field=reopened.findChild<QLineEdit*>("elementName");field->setFocus();field->setText("Must not be lost");answer(QMessageBox::Save);QVERIFY(!reopened.close());QVERIFY(reopened.isVisible());QVERIFY(reopened.currentView()["dirty"].toBool());QVERIFY(!reopened.lastError.isEmpty());
  answer(QMessageBox::Discard);QVERIFY(reopened.close());QVERIFY(file.open(QIODevice::ReadOnly));QVERIFY(!file.readAll().contains("Must not be lost"));file.close();
}

void StudioTests::editorBasicsVersionControl(){
  const auto root=qEnvironmentVariable("RAISING_BASICS_PROJECT");if(root.isEmpty())QSKIP("Set RAISING_BASICS_PROJECT to a disposable Git repository");
  StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()));w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().empty(),20000);
  bool checked=false;QTimer timer;timer.setInterval(50);connect(&timer,&QTimer::timeout,&w,[&]{auto dialog=w.findChild<QDialog*>("versionControlDialog");if(!dialog)return;auto out=dialog->findChild<QPlainTextEdit*>("versionOutput");if(!out->toPlainText().contains("project.json"))return;checked=true;timer.stop();const auto evidence=qEnvironmentVariable("RAISING_BASICS_EVIDENCE");if(!evidence.isEmpty())dialog->grab().save(evidence+"/version-control.png");dialog->reject();});timer.start();
  QTimer::singleShot(10000,&w,[&]{if(auto dialog=w.findChild<QDialog*>("versionControlDialog"))dialog->reject();});w.findChild<QAction*>("versionControl")->trigger();QVERIFY(checked);QVERIFY(!w.currentView()["dirty"].toBool());
}
