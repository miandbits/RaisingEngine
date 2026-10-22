#include "studio.hpp"
#include "panel-navigation.hpp"
#include <QApplication>
#include <QClipboard>
#include <QDockWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QTest>
#include <QTimer>
#include <QTreeWidgetItemIterator>

class EditorInteractionTests : public QObject {
  Q_OBJECT
private slots:
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
    name->setFocus(); name->selectAll(); QTest::keyClick(name, Qt::Key_C, Qt::ControlModifier);
    QCOMPARE(QApplication::clipboard()->text(), name->text());
    const QString renamed = name->text() + " edited";
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
