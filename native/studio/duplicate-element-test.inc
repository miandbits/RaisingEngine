#include <QTreeWidgetItemIterator>

void StudioTests::duplicateElementMenuAndShortcut(){
  const auto root=qEnvironmentVariable("RAISING_DUPLICATE_PROJECT");if(root.isEmpty())QSKIP("Set RAISING_DUPLICATE_PROJECT to a disposable project copy");
  StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()));w.resize(1480,960);w.show();w.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().empty(),20000);
  auto duplicate=w.findChild<QAction*>("duplicateElementAction");QVERIFY(duplicate);QCOMPARE(duplicate->shortcut(),QKeySequence("Ctrl+D"));QVERIFY(!duplicate->toolTip().isEmpty());
  auto elements=[&]{for(auto value:w.projectData().value("visual").toObject().value("screens").toArray())if(value.toObject().value("id")=="home")return value.toObject().value("elements").toArray();return QJsonArray();};
  auto get=[&](QString id){for(auto value:elements())if(value.toObject().value("id")==id)return value.toObject();return QJsonObject();};
  auto select=[&](QString id){auto tree=w.findChild<QTreeWidget*>("hierarchy");tree->expandAll();QTreeWidgetItemIterator it(tree);while(*it){if((*it)->data(0,Qt::UserRole).toString()==id){tree->setCurrentItem(*it);tree->scrollToItem(*it);QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(*it).center());return;}++it;}QFAIL("Element not found");};
  w.findChild<QAction*>("addButton")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);const auto childId=w.selectedId();
  w.findChild<QPushButton*>("addGroup")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);const auto groupId=w.selectedId();
  // A QJsonValueRef temporary aliases the array: swapping must preserve both complete values.
  const auto layerBefore=w.projectData(),groupBefore=get(groupId),childBefore=get(childId);
  const auto layerCount=elements().size();auto backward=w.findChild<QAction*>("layerBackward"),forward=w.findChild<QAction*>("layerForward");QVERIFY(backward);QVERIFY(forward);
  backward->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  QCOMPARE(elements().size(),layerCount);QCOMPARE(get(groupId),groupBefore);QCOMPARE(get(childId),childBefore);QCOMPARE(w.selectedId(),groupId);
  QCOMPARE(elements().at(layerCount-2).toObject().value("id").toString(),groupId);QCOMPARE(elements().at(layerCount-1).toObject().value("id").toString(),childId);
  {QSet<QString> unique;for(auto value:elements()){const auto id=value.toObject().value("id").toString();QVERIFY(!unique.contains(id));unique.insert(id);}}
  const auto moved=w.projectData();w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),layerBefore);
  select(groupId);backward->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),moved);
  forward->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),layerBefore);
  select(childId);auto parent=w.findChild<QComboBox*>("elementParent");parent->setCurrentIndex(parent->findData(groupId));QMetaObject::invokeMethod(parent,"activated",Q_ARG(int,parent->currentIndex()));QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  select(groupId);QVERIFY(duplicate->isEnabled());const auto before=w.projectData(),child=get(childId);const auto count=elements().size();
  duplicate->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(elements().size(),count+2);const auto copyId=w.selectedId();QVERIFY(copyId!=groupId);QJsonObject copiedChild;QSet<QString> ids;for(auto value:elements()){auto e=value.toObject();QVERIFY(!ids.contains(e.value("id").toString()));ids.insert(e.value("id").toString());if(e.value("parentId")==copyId)copiedChild=e;}
  QVERIFY(!copiedChild.empty());auto comparable=copiedChild;comparable["id"]=childId;comparable["parentId"]=groupId;QCOMPARE(comparable,child);
  w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),before);
  select(groupId);auto tree=w.findChild<QTreeWidget*>("hierarchy");tree->setFocus();QTest::keyClick(tree,Qt::Key_D,Qt::ControlModifier);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&elements().size()==count+2,10000);QTest::qWait(100);QCOMPARE(elements().size(),count+2);
  w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),before);
  select(groupId);w.findChild<QCheckBox*>("elementLocked")->setChecked(true);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY(!duplicate->isEnabled());select(childId);QVERIFY(!duplicate->isEnabled());
  w.findChild<QAction*>("validateProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY(w.findChild<QLabel*>("validationSummary")->text().contains("오류 0건"));
}
