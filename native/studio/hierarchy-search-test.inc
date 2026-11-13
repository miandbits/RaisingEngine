#include <QTreeWidgetItemIterator>
#include "authoring-labels.hpp"

void StudioTests::hierarchyBoundTextLabels(){
  const auto root=qEnvironmentVariable("RAISING_MAGICAL_PROJECT");
  if(root.isEmpty())QSKIP("Set RAISING_MAGICAL_PROJECT to the childhood reference");
  StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()));w.show();w.openProject(root);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().empty(),20000);
  auto tree=w.findChild<QTreeWidget*>("hierarchy");auto search=w.findChild<QLineEdit*>("hierarchySearch");QVERIFY(tree);QVERIFY(search);
  const auto before=w.projectData();
  int count=0;QTreeWidgetItem *money=nullptr;
  for(QTreeWidgetItemIterator i(tree);*i;++i){auto id=(*i)->data(0,Qt::UserRole).toString();if(id.isEmpty())continue;++count;QVERIFY2(!(*i)->text(0).trimmed().isEmpty(),qPrintable(id));if(id=="hud-money")money=*i;}
  QVERIFY(count>=26);QVERIFY(money);QMap<QString,QJsonObject> siblings;for(auto screen:before.value("visual").toObject().value("screens").toArray())if(screen.toObject().value("id").toString()==money->data(0,Qt::UserRole+1).toString())for(auto value:screen.toObject().value("elements").toArray()){const auto element=value.toObject();siblings.insert(element.value("id").toString(),element);}QCOMPARE(money->text(0),authoringDistinctLabel(siblings.value("hud-money"),siblings));QVERIFY(money->toolTip(0).endsWith(" · hud-money"));
  search->setText("hud-money");
  money=nullptr;for(QTreeWidgetItemIterator i(tree);*i;++i)if((*i)->data(0,Qt::UserRole)=="hud-money"&&(*i)->data(0,Qt::UserRole+1)=="home")money=*i;
  QVERIFY(money);QVERIFY(!money->isHidden());tree->setCurrentItem(money);QCOMPARE(w.selectedId(),QString("hud-money"));
  QCOMPARE(w.projectData(),before);QVERIFY(!w.currentView().value("dirty").toBool());
}

void StudioTests::hierarchySearchAcrossScreens(){
  const auto root=qEnvironmentVariable("RAISING_HIERARCHY_SEARCH_PROJECT");
  if(root.isEmpty())QSKIP("Set RAISING_HIERARCHY_SEARCH_PROJECT to the isolated search fixture");
  StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()));w.resize(1480,960);w.show();w.openProject(root);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().empty(),20000);
  auto search=w.findChild<QLineEdit*>("hierarchySearch");auto tree=w.findChild<QTreeWidget*>("hierarchy");auto tabs=w.findChild<QTabBar*>("documentTabs");
  QVERIFY(search);QVERIFY(tree);QVERIFY(tabs);
  auto find=[&](QString screen,QString id){for(QTreeWidgetItemIterator i(tree);*i;++i)if((*i)->data(0,Qt::UserRole+1)==screen&&(*i)->data(0,Qt::UserRole).toString()==id)return *i;return static_cast<QTreeWidgetItem*>(nullptr);};
  auto choose=[&](QTreeWidgetItem* item){tree->scrollToItem(item);QTest::mouseClick(tree->viewport(),Qt::LeftButton,Qt::NoModifier,tree->visualItemRect(item).center());};
  QVERIFY(find("home","flowers"));choose(find("home","flowers"));QCOMPARE(w.selectedId(),QString("flowers"));
  const auto project=w.projectData();const auto revision=w.currentView().value("revision");const auto tabCount=tabs->count();
  search->setFocus();QTest::keyClicks(search,"FLOWERS");QTest::keyClick(search,Qt::Key_Return);
  QVERIFY(find("market","flowers"));QVERIFY(!find("market","flowers")->isHidden());
  QVERIFY(find("market","shelf"));QVERIFY(!find("market","shelf")->isHidden());
  QVERIFY(find("market","unrelated")->isHidden());QVERIFY(!tree->dragEnabled());
  QCOMPARE(w.selectedId(),QString("flowers"));QCOMPARE(tabs->count(),tabCount);QCOMPARE(w.projectData(),project);QCOMPARE(w.currentView().value("revision"),revision);
  choose(find("market","flowers"));QCOMPARE(tabs->tabData(tabs->currentIndex()).toString(),QString("market"));QCOMPARE(w.selectedId(),QString("flowers"));
  QCOMPARE(tree->selectedItems().size(),1);QCOMPARE(tree->selectedItems().first()->data(0,Qt::UserRole+1).toString(),QString("market"));
  QCOMPARE(search->text(),QString("FLOWERS"));QVERIFY(find("market","unrelated")->isHidden());
  search->setText("꽃 가격");QVERIFY(!find("market","flowers")->isHidden());QVERIFY(find("home","flowers")->isHidden());
  search->setText("장식 선반");QVERIFY(!find("market","unrelated")->isHidden());
  search->setText("없는 검색어");QCOMPARE(w.selectedId(),QString("flowers"));
  bool empty=false;for(int i=0;i<tree->topLevelItemCount();i++)if(!tree->topLevelItem(i)->isHidden()&&tree->topLevelItem(i)->text(0)==tr("검색 결과 없음"))empty=true;QVERIFY(empty);
  search->clear();QCOMPARE(tree->topLevelItemCount(),1);QCOMPARE(tree->topLevelItem(0)->data(0,Qt::UserRole+1).toString(),QString("market"));QVERIFY(tree->dragEnabled());
  QCOMPARE(w.selectedId(),QString("flowers"));QCOMPARE(w.projectData(),project);QCOMPARE(w.currentView().value("revision"),revision);
  // A normal refresh while filtering must reapply the query instead of showing the full tree.
  search->setText("flowers");auto name=w.findChild<QLineEdit*>("elementName");name->setText("새 꽃 가격");QMetaObject::invokeMethod(name,"editingFinished");
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(search->text(),QString("flowers"));QVERIFY(find("market","unrelated")->isHidden());QCOMPARE(w.selectedId(),QString("flowers"));
}
