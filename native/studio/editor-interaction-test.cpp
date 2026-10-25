#include "studio.hpp"
#include "panel-navigation.hpp"
#include "tool-search.hpp"
#include <QApplication>
#include <QClipboard>
#include <QDockWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QTest>
#include <QSignalSpy>
#include <QTimer>
#include <QTreeWidgetItemIterator>

class EditorInteractionTests : public QObject {
  Q_OBJECT
private slots:
  void multilinePlatformCommitBeforeCancelKey() {
    for (bool plain : {false, true}) {
      QWidget owner; auto layout = new QVBoxLayout(&owner);
      QWidget *field = plain ? static_cast<QWidget *>(new QPlainTextEdit(&owner))
                             : static_cast<QWidget *>(new QTextEdit(&owner));
      layout->addWidget(field);
      EditorInputGuard guard(&owner, [] {}, [] { return true; });
      auto contents = [field, plain] {
        return plain ? static_cast<QPlainTextEdit *>(field)->toPlainText()
                     : static_cast<QTextEdit *>(field)->toPlainText();
      };
      if (plain) static_cast<QPlainTextEdit *>(field)->setPlainText("공방");
      else static_cast<QTextEdit *>(field)->setPlainText("공방");
      owner.show(); owner.activateWindow(); field->setFocus(); QCoreApplication::processEvents();
      QTest::keyClick(field, Qt::Key_End); QTest::keyClicks(field, " A");
      for (auto key : {Qt::Key_Escape, Qt::Key_Z}) {
        QInputMethodEvent partial("한", {}); QApplication::sendEvent(field, &partial);
        QInputMethodEvent finish; finish.setCommitString("한"); QApplication::sendEvent(field, &finish);
        QTest::keyClick(field, key, key == Qt::Key_Z ? Qt::ControlModifier : Qt::NoModifier);
        QCOMPARE(contents(), QString("공방 A"));
      }
      QTest::keyClick(field, Qt::Key_Z, Qt::ControlModifier); QCOMPARE(contents(), QString("공방"));
      QTest::keyClick(field, Qt::Key_Y, Qt::ControlModifier); QCOMPARE(contents(), QString("공방 A"));
      QInputMethodEvent partial("한", {}); QApplication::sendEvent(field, &partial);
      QInputMethodEvent finish; finish.setCommitString("한"); QApplication::sendEvent(field, &finish);
      QFocusEvent leaving(QEvent::FocusOut); QApplication::sendEvent(field, &leaving);
      QCOMPARE(contents(), QString("공방 A한"));
    }
  }
  void platformCommitBeforeCancelKey() {
    QWidget owner; owner.show();
    QDialog dialog(&owner); auto layout = new QVBoxLayout(&dialog);
    auto field = new QLineEdit("공방", &dialog); layout->addWidget(field);
    EditorInputGuard guard(&owner, [] {}, [] { return true; });
    dialog.show(); dialog.activateWindow(); field->setFocus(); QCoreApplication::processEvents();
    field->setCursorPosition(field->text().size()); QTest::keyClicks(field, " A");
    for (auto key : {Qt::Key_Escape, Qt::Key_Z}) {
      QInputMethodEvent partial("한", {}); QApplication::sendEvent(field, &partial);
      QInputMethodEvent finish; finish.setCommitString("한"); QApplication::sendEvent(field, &finish);
      QInputMethodEvent end; QApplication::sendEvent(field, &end);
      QCOMPARE(field->text(), QString("공방 A"));
      QVERIFY(field->property("raisingInputComposing").toBool());
      QTest::keyClick(field, key, key == Qt::Key_Z ? Qt::ControlModifier : Qt::NoModifier);
      QCOMPARE(field->text(), QString("공방 A")); QVERIFY(dialog.isVisible());
      QVERIFY(!field->property("raisingInputComposing").toBool());
    }
    QTest::keyClick(field, Qt::Key_Z, Qt::ControlModifier); QCOMPARE(field->text(), QString("공방"));
    QTest::keyClick(field, Qt::Key_Y, Qt::ControlModifier); QCOMPARE(field->text(), QString("공방 A"));
    QInputMethodEvent partial("한", {}); QApplication::sendEvent(field, &partial);
    QInputMethodEvent finish; finish.setCommitString("한"); QApplication::sendEvent(field, &finish);
    QTest::keyRelease(field, Qt::Key_Control);
    QCOMPARE(field->text(), QString("공방 A한"));
    QVERIFY(!field->property("raisingInputComposing").toBool());
    QInputMethodEvent next("글", {}); QApplication::sendEvent(field, &next);
    QInputMethodEvent finishNext; finishNext.setCommitString("글"); QApplication::sendEvent(field, &finishNext);
    QTest::keyClick(field, Qt::Key_S, Qt::ControlModifier);
    QCOMPARE(field->text(), QString("공방 A한글"));
    QInputMethodEvent last("끝", {}); QApplication::sendEvent(field, &last);
    QInputMethodEvent finishLast; finishLast.setCommitString("끝"); QApplication::sendEvent(field, &finishLast);
    QVERIFY(commitEditorInput(&owner));
    QCOMPARE(field->text(), QString("공방 A한글끝"));
  }
  void cancellingDoesNotCommitPlatformPreedit() {
    QWidget owner; owner.show();
    QDialog dialog(&owner); auto layout = new QVBoxLayout(&dialog);
    auto field = new QLineEdit("공방", &dialog); layout->addWidget(field);
    EditorInputGuard guard(&owner, [field] {
      QInputMethodEvent platformReset; platformReset.setCommitString("한");
      QApplication::sendEvent(field, &platformReset);
    });
    dialog.show(); dialog.activateWindow(); field->setFocus(); QCoreApplication::processEvents();
    field->setCursorPosition(field->text().size());
    QTest::keyClicks(field, " A");
    for (auto key : {Qt::Key_Escape, Qt::Key_Z}) {
      QInputMethodEvent partial("한", {}); QApplication::sendEvent(field, &partial);
      QTest::keyClick(field, key, key == Qt::Key_Z ? Qt::ControlModifier : Qt::NoModifier);
      QCOMPARE(field->text(), QString("공방 A")); QVERIFY(dialog.isVisible());
      QVERIFY(!field->property("raisingInputComposing").toBool());
    }
    QTest::keyClick(field, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(field->text(), QString("공방"));
    QTest::keyClick(field, Qt::Key_Y, Qt::ControlModifier);
    QCOMPARE(field->text(), QString("공방 A"));
  }
  void nativeCompositionEscape() {
#ifdef Q_OS_WIN
    QWidget owner; EditorInputGuard guard(&owner); owner.show();
    QDialog dialog(&owner); auto layout = new QVBoxLayout(&dialog);
    auto field = new QLineEdit("공방", &dialog); layout->addWidget(field);
    dialog.show(); dialog.activateWindow(); field->setFocus(); QCoreApplication::processEvents();
    QInputMethodEvent partial("한", {}); QApplication::sendEvent(field, &partial);
    MSG message{}; message.message = WM_KEYDOWN; message.wParam = VK_ESCAPE;
    qintptr result = -1;
    QWidget otherOwner; EditorInputGuard unrelatedGuard(&otherOwner);
    QVERIFY(!unrelatedGuard.nativeEventFilter("windows_generic_MSG", &message, &result));
    QVERIFY(field->property("raisingInputComposing").toBool());
    message.wParam = VK_F6;
    QVERIFY(!guard.nativeEventFilter("windows_generic_MSG", &message, &result));
    QVERIFY(field->property("raisingInputComposing").toBool());
    message.wParam = VK_ESCAPE;
    QVERIFY(guard.nativeEventFilter("windows_generic_MSG", &message, &result));
    QCOMPARE(result, qintptr(0)); QCOMPARE(field->text(), QString("공방"));
    QVERIFY(dialog.isVisible()); QVERIFY(!field->property("raisingInputComposing").toBool());
    QVERIFY(!guard.nativeEventFilter("windows_generic_MSG", &message, &result));
    QInputMethodEvent queuedPartial("한", {}); QApplication::sendEvent(field, &queuedPartial);
    QVERIFY(guard.nativeEventFilter("windows_generic_MSG", &message, nullptr));
    QCOMPARE(field->text(), QString("공방")); QVERIFY(dialog.isVisible());
    QVERIFY(!field->property("raisingInputComposing").toBool());
    QTest::keyClick(field, Qt::Key_Escape); QVERIFY(!dialog.isVisible());
#else
    QSKIP("Windows native message interception");
#endif
  }
  void koreanCompositionAndCancel() {
    QWidget owner; new EditorInputGuard(&owner); owner.show();
    QDialog dialog(&owner); auto layout = new QVBoxLayout(&dialog);
    auto field = new QLineEdit("공방", &dialog); layout->addWidget(field);
    dialog.show(); dialog.activateWindow(); field->setFocus(); QCoreApplication::processEvents();
    field->setCursorPosition(field->text().size());
    QInputMethodEvent committed; committed.setCommitString(" 한글"); QApplication::sendEvent(field, &committed);
    QCOMPARE(field->text(), QString("공방 한글"));
    QInputMethodEvent composing("ㄱ", {}); QApplication::sendEvent(field, &composing);
    QVERIFY(field->property("raisingInputComposing").toBool());
    QTest::keyClick(field, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(field->text(), QString("공방 한글")); QVERIFY(!field->property("raisingInputComposing").toBool());
    QTest::keyClick(field, Qt::Key_Z, Qt::ControlModifier); QCOMPARE(field->text(), QString("공방"));
    QTest::keyClick(field, Qt::Key_Y, Qt::ControlModifier); QCOMPARE(field->text(), QString("공방 한글"));
    field->selectAll(); QTest::keyClick(field, Qt::Key_C, Qt::ControlModifier);
    QCOMPARE(QApplication::clipboard()->text(), QString("공방 한글"));
    field->deselect(); field->setCursorPosition(field->text().size());
    QInputMethodEvent partial("한", {}); QApplication::sendEvent(field, &partial);
    QTest::keyClick(field, Qt::Key_C, Qt::ControlModifier);
    QCOMPARE(field->text(), QString("공방 한글"));
    QCOMPARE(QApplication::clipboard()->text(), QString("공방 한글"));
    QVERIFY(field->property("raisingInputComposing").toBool());
    QTest::keyClick(field, Qt::Key_Escape); QVERIFY(dialog.isVisible());
    QVERIFY(!field->property("raisingInputComposing").toBool());
    QTest::keyClick(field, Qt::Key_Escape); QVERIFY(!dialog.isVisible());
  }
  void toolSearchKeyboardAndIme() {
    QWidget owner; new EditorInputGuard(&owner); owner.show();
    QLineEdit source(&owner); source.show(); owner.activateWindow(); source.setFocus(); QCoreApplication::processEvents();
    QMenu menu(&owner); auto first = menu.addAction("인물");
    auto disabled = menu.addAction("사용 불가"); disabled->setEnabled(false);
    auto relation = menu.addAction("관계"); bool invoked = false;
    connect(relation, &QAction::triggered, &owner, [&] { invoked = true; });
    Q_UNUSED(first);
    QInputMethodEvent pending("ㅎ", {}); QApplication::sendEvent(&source, &pending);
    bool openedDuringComposition = false; QTimer unexpectedDialog; unexpectedDialog.setSingleShot(true);
    connect(&unexpectedDialog, &QTimer::timeout, &owner, [&] {
      if (auto d = owner.findChild<QDialog *>("authoringToolSearch")) { openedDuringComposition = true; d->reject(); }
    });
    unexpectedDialog.start(100); showAuthoringToolSearch(&owner, &menu); unexpectedDialog.stop();
    QVERIFY(!openedDuringComposition); QVERIFY(source.hasFocus()); QVERIFY(source.property("raisingInputComposing").toBool());
    QTest::keyClick(&source, Qt::Key_Escape);
    QTimer::singleShot(5000, &owner, [&] { if (auto d = owner.findChild<QDialog *>("authoringToolSearch")) d->reject(); });
    QTimer::singleShot(50, &owner, [&] {
      auto dialog = owner.findChild<QDialog *>("authoringToolSearch"); QVERIFY(dialog);
      auto search = dialog->findChild<QLineEdit *>("authoringToolQuery");
      auto list = dialog->findChild<QListWidget *>("authoringToolResults"); QVERIFY(search && list);
      QTest::keyClick(search, Qt::Key_Down); QCOMPARE(list->currentRow(), 2); QVERIFY(search->hasFocus());
      QTest::keyClick(search, Qt::Key_Up); QCOMPARE(list->currentRow(), 0);
      QInputMethodEvent partial("ㄱ", {}); QApplication::sendEvent(search, &partial);
      QTest::keyClick(search, Qt::Key_Down); QCOMPARE(list->currentRow(), 0);
      QTest::keyClick(search, Qt::Key_Escape); QVERIFY(dialog->isVisible());
      QInputMethodEvent committed; committed.setCommitString("관계"); QApplication::sendEvent(search, &committed);
      QCOMPARE(list->count(), 1); QTest::keyClick(search, Qt::Key_Return);
    });
    showAuthoringToolSearch(&owner, &menu); QVERIFY(invoked);
  }
  void commandSearchMenusAndOverflow() {
    QMainWindow owner; owner.resize(1000,680);
    QTimer watchdog;watchdog.setInterval(5000);connect(&watchdog,&QTimer::timeout,&owner,[&]{if(auto d=owner.findChild<QDialog*>("authoringToolSearch"))d->reject();});watchdog.start();
    auto source=new QLineEdit("Unapplied draft",&owner);owner.setCentralWidget(source);
    auto design=owner.menuBar()->addMenu("Design");design->addAction("Characters");
    auto file=owner.menuBar()->addMenu("&File");
    auto save=file->addAction("&Save project");save->setObjectName("testSave");save->setShortcut(QKeySequence::Save);
    auto locked=file->addMenu("Locked");locked->setEnabled(false);auto blocked=locked->addAction("Export unavailable");blocked->setObjectName("testBlocked");
    auto hidden=file->addAction("Hidden command");hidden->setObjectName("testHidden");hidden->setVisible(false);
    auto hiddenPage=new QWidget(&owner);hiddenPage->hide();auto hiddenToolbar=new QToolBar("Other workspace",hiddenPage);hiddenToolbar->addAction("Inactive workspace command")->setObjectName("testHiddenWorkspace");
    auto toolbar=owner.addToolBar("Main tools");toolbar->addAction(save);
    auto play=toolbar->addAction("Play project");play->setObjectName("testPlay");
    auto modes=new QComboBox;modes->setObjectName("testMode");modes->setAccessibleName("Preview language");modes->addItems({"English","Japanese"});toolbar->addWidget(modes);
    auto palette=new QToolButton;palette->setText("Elements");auto paletteMenu=new QMenu(palette);auto text=paletteMenu->addAction("Add text");text->setObjectName("testText");palette->setMenu(paletteMenu);toolbar->addWidget(palette);
    auto mirrored=owner.menuBar()->addMenu("Tools");mirrored->setObjectName("toolbarAccessMenu");mirrored->addAction(save);
    QSignalSpy saved(save,&QAction::triggered),opened(blocked,&QAction::triggered),changed(modes,qOverload<int>(&QComboBox::activated));
    bool savedWithContext=false;connect(save,&QAction::triggered,&owner,[&]{savedWithContext=QApplication::focusWidget()==source;});
    owner.show();owner.activateWindow();source->setFocus();QCoreApplication::processEvents();
    QTimer::singleShot(30,&owner,[&]{
      auto dialog=owner.findChild<QDialog*>("authoringToolSearch");QVERIFY(dialog);QVERIFY(dialog->width()<=owner.width());
      auto list=dialog->findChild<QListWidget*>("authoringToolResults");auto query=dialog->findChild<QLineEdit*>("authoringToolQuery");
      QMap<QString,int> counts;for(int row=0;row<list->count();++row)counts[list->item(row)->data(Qt::UserRole+1).toString()]++;
      QCOMPARE(counts["testSave"],1);QCOMPARE(counts["testPlay"],1);QCOMPARE(counts["testText"],1);QCOMPARE(counts["testHidden"],0);QCOMPARE(counts["testHiddenWorkspace"],0);
      query->setText("File Save");QCOMPARE(list->count(),1);QVERIFY(list->currentItem()->text().contains(save->shortcut().toString(QKeySequence::NativeText)));QVERIFY(!list->currentItem()->text().contains('&'));
      save->setEnabled(false);QVERIFY(!dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Open)->isEnabled());
      save->setEnabled(true);QVERIFY(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Open)->isEnabled());
      QTest::keyClick(query,Qt::Key_Return);
    });
    showAuthoringToolSearch(&owner,design);QCOMPARE(saved.count(),1);QVERIFY(savedWithContext);QCOMPARE(source->text(),QString("Unapplied draft"));
    QTimer::singleShot(30,&owner,[&]{
      auto dialog=owner.findChild<QDialog*>("authoringToolSearch");auto query=dialog->findChild<QLineEdit*>("authoringToolQuery");auto list=dialog->findChild<QListWidget*>("authoringToolResults");
      query->setText("unavailable");QCOMPARE(list->count(),1);QVERIFY(!(list->item(0)->flags()&Qt::ItemIsEnabled));QVERIFY(!dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Open)->isEnabled());
      QTest::keyClick(query,Qt::Key_Return);QVERIFY(dialog->isVisible());QTest::keyClick(query,Qt::Key_Escape);
    });
    owner.activateWindow();source->setFocus();QCoreApplication::processEvents();QVERIFY(source->hasFocus());showAuthoringToolSearch(&owner,design);QCOMPARE(opened.count(),0);QTRY_VERIFY(source->hasFocus());QCOMPARE(source->text(),QString("Unapplied draft"));
    QTimer::singleShot(30,&owner,[&]{
      auto dialog=owner.findChild<QDialog*>("authoringToolSearch");auto query=dialog->findChild<QLineEdit*>("authoringToolQuery");auto list=dialog->findChild<QListWidget*>("authoringToolResults");
      query->setText("Preview language Japanese");QCOMPARE(list->count(),1);QTest::keyClick(query,Qt::Key_Return);
    });
    showAuthoringToolSearch(&owner,design);QCOMPARE(modes->currentIndex(),1);QCOMPARE(changed.count(),1);
    modes->setEnabled(false);
    QTimer::singleShot(30,&owner,[&]{
      auto dialog=owner.findChild<QDialog*>("authoringToolSearch");dialog->findChild<QLineEdit*>("authoringToolQuery")->setText("Preview language English");
      QVERIFY(!dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Open)->isEnabled());dialog->reject();
    });
    showAuthoringToolSearch(&owner,design);QCOMPARE(modes->currentIndex(),1);QCOMPARE(changed.count(),1);
  }
  void studioCommandSearchInventory() {
    StudioWindow owner(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()));owner.resize(1000,680);owner.show();
    QTimer::singleShot(5000,&owner,[&]{if(auto d=owner.findChild<QDialog*>("authoringToolSearch"))d->reject();});
    QTimer::singleShot(30,&owner,[&]{
      auto dialog=owner.findChild<QDialog*>("authoringToolSearch");QVERIFY(dialog);auto list=dialog->findChild<QListWidget*>("authoringToolResults");QVERIFY(list);
      QMap<QString,int> counts;for(int row=0;row<list->count();++row)counts[list->item(row)->data(Qt::UserRole+1).toString()]++;
      for(auto id:{"saveProject","exportGame","playProject","validateProject","showHierarchy","newScreen","inspectRenderer","add-element-text","conditionPreviewAction"})QCOMPARE(counts[id],1);
      dialog->reject();
    });
    owner.findChild<QAction*>("findAuthoringTool")->trigger();
  }
  void commandSearchSavesFocusedInspector() {
    const auto root=qEnvironmentVariable("RAISING_INTERACTION_PROJECT");if(root.isEmpty())QSKIP("Set RAISING_INTERACTION_PROJECT to an isolated project copy");
    const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());
    StudioWindow owner(engine);owner.resize(1000,680);owner.show();owner.openProject(root);
    QTRY_VERIFY_WITH_TIMEOUT(!owner.busy()&&!owner.projectData().isEmpty(),20000);
    auto tree=owner.findChild<QTreeWidget*>("hierarchy");QVERIFY(tree);QString id;
    for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->childCount()==0&&!(*it)->data(0,Qt::UserRole).toString().isEmpty()){id=(*it)->data(0,Qt::UserRole).toString();tree->setCurrentItem(*it);break;}
    QVERIFY(!id.isEmpty());auto field=owner.findChild<QLineEdit*>("elementName");QVERIFY(field&&field->isEnabled());
    owner.activateWindow();field->setFocus();QCoreApplication::processEvents();field->setText("Saved through command search 한글");
    QTimer::singleShot(10000,&owner,[&]{if(auto d=owner.findChild<QDialog*>("authoringToolSearch"))d->reject();});
    bool choseSave=false;
    QTimer::singleShot(50,&owner,[&]{
      auto dialog=owner.findChild<QDialog*>("authoringToolSearch");QVERIFY(dialog);auto list=dialog->findChild<QListWidget*>("authoringToolResults");QVERIFY(list);
      QTRY_VERIFY_WITH_TIMEOUT(!owner.busy(),5000);
      for(int row=0;row<list->count();++row)if(list->item(row)->data(Qt::UserRole+1)=="saveProject"){QVERIFY(list->item(row)->flags()&Qt::ItemIsEnabled);list->setCurrentRow(row);choseSave=true;break;}
      QVERIFY(choseSave);QTest::keyClick(dialog->findChild<QLineEdit*>("authoringToolQuery"),Qt::Key_Return);
    });
    owner.findChild<QAction*>("findAuthoringTool")->trigger();QVERIFY(choseSave);
    QTRY_VERIFY_WITH_TIMEOUT(!owner.busy()&&!owner.currentView()["dirty"].toBool(),15000);
    const auto saved=owner.projectData();bool found=false;
    for(auto screen:saved["visual"].toObject()["screens"].toArray())for(auto element:screen.toObject()["elements"].toArray())if(element.toObject()["id"]==id)found=element.toObject()["label"]=="Saved through command search 한글";
    QVERIFY(found);QVERIFY(owner.close());
    StudioWindow reopened(engine);reopened.show();reopened.openProject(root);QTRY_VERIFY_WITH_TIMEOUT(!reopened.busy()&&!reopened.projectData().isEmpty(),20000);QCOMPARE(reopened.projectData(),saved);QVERIFY(reopened.close());
  }
  void panelNavigationWithoutProject() {
    QMainWindow w; w.resize(1000, 680);
    auto center = new QLineEdit(&w); w.setCentralWidget(center);
    auto first = new QDockWidget("First", &w); first->setObjectName("a");
    auto field = new QLineEdit(first); first->setWidget(field); w.addDockWidget(Qt::LeftDockWidgetArea, first);
    auto status = new QDockWidget("Status", &w); status->setObjectName("b");
    status->setWidget(new QLabel("Read only", status)); w.addDockWidget(Qt::RightDockWidgetArea, status);
    installPanelNavigation(&w, w.menuBar()->addMenu("View")); w.show(); w.activateWindow();
    QCoreApplication::processEvents(); center->setFocus();
    for (int round = 0; round < 3; ++round) {
      QTest::keyClick(center, Qt::Key_F6); QCOMPARE(QApplication::focusWidget(), field);
      QTest::keyClick(field, Qt::Key_F6); QCOMPARE(QApplication::focusWidget(), status);
      QTest::keyClick(status, Qt::Key_F6); QCOMPARE(QApplication::focusWidget(), center);
    }
    QTest::keyClick(center, Qt::Key_F6, Qt::ShiftModifier); QCOMPARE(QApplication::focusWidget(), status);
    first->hide(); auto menu = w.findChild<QMenu *>("panelNavigationMenu");
    QMetaObject::invokeMethod(menu, "aboutToShow");
    QCOMPARE(menu->actions().size(), 2);
    for (auto action : menu->actions()) if (action->text() == "First") action->trigger();
    QVERIFY(first->isVisible()); QCOMPARE(QApplication::focusWidget(), field);
    QMetaObject::invokeMethod(menu, "aboutToShow"); QCOMPARE(menu->actions().size(), 2);
  }
  void keyboardSaveReopen() {
    const auto root = qEnvironmentVariable("RAISING_INTERACTION_PROJECT");
    if (root.isEmpty()) QSKIP("Set RAISING_INTERACTION_PROJECT to an isolated project copy");
    const auto engine = qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath());
    QTimer watchdog; watchdog.setInterval(30000);
    connect(&watchdog, &QTimer::timeout, this, [] {
      if (auto box = qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
        qWarning() << "Unexpected pending dialog" << box->objectName(); box->reject();
      }
    }); watchdog.start();
    StudioWindow w(engine); w.resize(1000, 680); w.show(); w.openProject(root);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && !w.projectData().isEmpty(), 20000);
    QCOMPARE(w.size(), QSize(1000, 680));
    auto tree = w.findChild<QTreeWidget *>("hierarchy");
    auto canvas = w.findChild<ScreenCanvas *>("screenCanvas");
    QVERIFY(tree && canvas);
    QTreeWidgetItem *selected = nullptr;
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
      if ((*it)->childCount() == 0 && !(*it)->data(0, Qt::UserRole).toString().isEmpty()) { selected = *it; break; }
    QVERIFY(selected); tree->setCurrentItem(selected); tree->setFocus();
    const auto selectedId = selected->data(0, Qt::UserRole).toString();
    const auto original = w.projectData();
    QTest::keyClick(tree, Qt::Key_C, Qt::ControlModifier);
    QVERIFY(w.findChild<QAction *>("pasteElements")->isEnabled());
    QTest::keyClick(tree, Qt::Key_V, Qt::ControlModifier);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && w.projectData() != original, 15000);
    const auto pasted = w.projectData();
    qInfo() << "Element clipboard passed";
    tree->setFocus(); QTest::keyClick(tree, Qt::Key_Z, Qt::ControlModifier);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && w.projectData() == original, 15000);
    tree->setFocus(); QTest::keyClick(tree, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && w.projectData() == pasted, 15000);
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
      if ((*it)->data(0, Qt::UserRole).toString() == selectedId) { tree->setCurrentItem(*it); break; }
    auto name = w.findChild<QLineEdit *>("elementName"); QVERIFY(name && name->isEnabled());
    name->setFocus();
    const auto beforeComposition = w.projectData();
    QInputMethodEvent preedit("ㅎ", {}); QApplication::sendEvent(name, &preedit);
    w.findChild<QAction *>("saveProject")->trigger();
    QCOMPARE(w.projectData(), beforeComposition); QVERIFY(!w.busy());
    QVERIFY(name->property("raisingInputComposing").toBool());
    w.findChild<QAction *>("focusNextPanel")->trigger(); QVERIFY(name->hasFocus());
    QTest::keyClick(name, Qt::Key_Escape); QVERIFY(w.isVisible());
    QVERIFY(!name->property("raisingInputComposing").toBool());
    name->setFocus(); name->selectAll(); QTest::keyClick(name, Qt::Key_C, Qt::ControlModifier);
    QCOMPARE(QApplication::clipboard()->text(), name->text());
    const QString renamed = name->text() + QString::fromUtf8(" 한글 저장");
    QApplication::clipboard()->setText(renamed);
    QTest::keyClick(name, Qt::Key_V, Qt::ControlModifier);
    QCOMPARE(name->text(), renamed);
    qInfo() << "Text clipboard passed";
    QTest::keyClick(name, Qt::Key_Z, Qt::ControlModifier);
    QVERIFY(name->text() != renamed);
    QTest::keyClick(name, Qt::Key_Y, Qt::ControlModifier);
    QCOMPARE(name->text(), renamed);
    // Leaving a focused field via panel navigation must commit it normally.
    tree->setFocus(); QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
    QTest::keyClick(tree, Qt::Key_F6);
    QTRY_VERIFY(QApplication::focusWidget() && !tree->isAncestorOf(QApplication::focusWidget()) && QApplication::focusWidget() != tree);
    auto next = QApplication::focusWidget(); QTest::keyClick(next, Qt::Key_F6, Qt::ShiftModifier);
    QTRY_VERIFY(w.findChild<QDockWidget *>("hierarchyDock")->isAncestorOf(QApplication::focusWidget()));
    qInfo() << "Panel roundtrip passed";
    auto status = w.findChild<QDockWidget *>("runtimeDock"); QVERIFY(status);
    auto navigation = w.findChild<QMenu *>("panelNavigationMenu"); QVERIFY(navigation);
    QMetaObject::invokeMethod(navigation, "aboutToShow");
    for (auto action : navigation->actions()) if (action->text() == status->windowTitle()) action->trigger();
    QVERIFY(QApplication::focusWidget() == status || status->isAncestorOf(QApplication::focusWidget()));
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_F6);
    QVERIFY(QApplication::focusWidget() != status && !status->isAncestorOf(QApplication::focusWidget()));
    auto dock = w.findChild<QDockWidget *>("hierarchyDock"); QVERIFY(dock); dock->hide();
    auto menu = w.findChild<QMenu *>("panelNavigationMenu"); QVERIFY(menu);
    QMetaObject::invokeMethod(menu, "aboutToShow");
    QAction *restore = nullptr; for (auto a : menu->actions()) if (a->text() == dock->windowTitle()) restore = a;
    QVERIFY(restore); restore->trigger(); QVERIFY(dock->isVisible());
    QVERIFY(dock->isAncestorOf(QApplication::focusWidget()));
    qInfo() << "Hidden panel restore passed";
    QTimer::singleShot(50, &w, [] {
      auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
      QVERIFY(box && box->objectName() == "unsavedChangesDialog");
      QTest::keyClick(box, Qt::Key_Escape);
    });
    QVERIFY(!w.close()); QVERIFY(w.isVisible()); QVERIFY(w.currentView()["dirty"].toBool());
    qInfo() << "Unsaved cancel passed";
    w.activateWindow(); QCoreApplication::processEvents();
    tree->setFocus(); qInfo() << "Saving via Ctrl+S"; QTest::keyClick(tree, Qt::Key_S, Qt::ControlModifier);
    qInfo() << "Save shortcut returned";
    QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && !w.currentView()["dirty"].toBool(), 15000);
    const auto saved = w.projectData(); qInfo() << "Saved, closing"; QVERIFY(w.close());
    StudioWindow reopened(engine); reopened.resize(1000, 680); reopened.show(); reopened.openProject(root);
    QTRY_VERIFY_WITH_TIMEOUT(!reopened.busy() && !reopened.projectData().isEmpty(), 20000);
    QCOMPARE(reopened.projectData(), saved);
    reopened.findChild<QAction *>("validateProject")->trigger();
    QTRY_VERIFY_WITH_TIMEOUT(!reopened.busy(), 15000);
    QVERIFY2(reopened.lastError.isEmpty(), qPrintable(reopened.lastError));
    qInfo() << "1000x680 keyboard copy/paste, text undo/redo, element undo/redo, panel restore, Escape cancel, Ctrl+S and reopen passed";
    QVERIFY(reopened.close());
  }
};
QTEST_MAIN(EditorInteractionTests)
#include "editor-interaction-test.moc"
