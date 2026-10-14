#include "hierarchy-lock.hpp"
#include <QTreeWidgetItemIterator>

void StudioTests::workbenchAncestorLocks(){
  const auto root=qEnvironmentVariable("RAISING_LOCK_PROJECT");
  if(root.isEmpty())QSKIP("Set RAISING_LOCK_PROJECT to an isolated project copy");
  StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()));w.resize(1520,1020);w.show();w.openProject(root);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().empty(),20000);
  auto get=[&](const QString& id){for(auto sv:w.projectData().value("visual").toObject().value("screens").toArray())for(auto ev:sv.toObject().value("elements").toArray())if(ev.toObject().value("id")==id)return ev.toObject();return QJsonObject();};
  auto select=[&](const QString& id){auto tree=w.findChild<QTreeWidget*>("hierarchy");QTreeWidgetItemIterator it(tree);while(*it){if((*it)->data(0,Qt::UserRole)==id){tree->setCurrentItem(*it);tree->scrollToItem(*it);QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(*it).center());return;}++it;}QFAIL("Missing lock test element");};
  auto parent=w.findChild<QComboBox*>("elementParent");auto lock=w.findChild<QCheckBox*>("elementLocked");auto canvas=w.findChild<ScreenCanvas*>();auto x=w.findChild<QSpinBox*>("elementX");auto anchor=w.findChild<QComboBox*>("anchorX");
  w.findChild<QAction*>("addButton")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);const auto button=w.selectedId();
  w.findChild<QPushButton*>("addGroup")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);const auto inner=w.selectedId();
  w.findChild<QPushButton*>("addGroup")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);const auto outer=w.selectedId();
  auto reparent=[&](const QString& id,const QString& owner){select(id);parent->setCurrentIndex(parent->findData(owner));QMetaObject::invokeMethod(parent,"activated",Q_ARG(int,parent->currentIndex()));QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);};
  reparent(inner,outer);reparent(button,inner);select(outer);lock->setChecked(true);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  select(button);QVERIFY(!x->isEnabled());QVERIFY(!anchor->isEnabled());QVERIFY(!parent->isEnabled());QVERIFY(!lock->isEnabled());
  const auto before=w.projectData();const auto child=get(button);
  x->setValue(child.value("x").toInt()+5);QMetaObject::invokeMethod(x,"editingFinished");QCOMPARE(w.projectData(),before);
  anchor->setCurrentIndex(anchor->findData("right"));QMetaObject::invokeMethod(anchor,"activated",Q_ARG(int,anchor->currentIndex()));QCOMPARE(w.projectData(),before);
  parent->setCurrentIndex(0);QMetaObject::invokeMethod(parent,"activated",Q_ARG(int,0));QCOMPARE(w.projectData(),before);
  lock->setChecked(true);QCOMPARE(w.projectData(),before);QVERIFY(!lock->isChecked());
  QMetaObject::invokeMethod(canvas,"geometryEdited",Q_ARG(QString,button),Q_ARG(QRect,QRect(55,55,200,50)));QCOMPARE(w.projectData(),before);
  QMetaObject::invokeMethod(canvas,"duplicateSelected");QCOMPARE(w.projectData(),before);
  QMetaObject::invokeMethod(canvas,"deleteSelected");QCOMPARE(w.projectData(),before);
  auto tree=w.findChild<QTreeWidget*>("hierarchy");QVERIFY(!tree->selectedItems().empty());QVERIFY(!(tree->selectedItems().first()->flags()&Qt::ItemIsDragEnabled));
  select(inner);auto clip=w.findChild<QCheckBox*>("clipChildren");QVERIFY(!clip->isEnabled());clip->setChecked(true);QCOMPARE(w.projectData(),before);
  // Moving an unlocked root element into a locked group is blocked too.
  w.findChild<QAction*>("addButton")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);const auto outsider=w.selectedId();const auto outsideBefore=w.projectData();
  parent->setCurrentIndex(parent->findData(outer));QMetaObject::invokeMethod(parent,"activated",Q_ARG(int,parent->currentIndex()));QCOMPARE(w.projectData(),outsideBefore);QVERIFY(!get(outsider).contains("parentId"));
  // The lock owner can explicitly unlock itself; descendants become editable.
  select(outer);QVERIFY(lock->isEnabled());QVERIFY(lock->isChecked());lock->setChecked(false);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);
  select(button);QVERIFY(x->isEnabled());QVERIFY(anchor->isEnabled());QVERIFY(parent->isEnabled());QVERIFY(lock->isEnabled());
  x->setValue(child.value("x").toInt()+5);QMetaObject::invokeMethod(x,"editingFinished");QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(get(button).value("x").toInt(),child.value("x").toInt()+5);
}
