#include "plugin-host.hpp"
#include <QLibraryInfo>
#include <QMenuBar>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTreeWidgetItemIterator>

namespace {
// A plugin state as the service sends it; no "store" key so the manager does not
// ask the (possibly old) service for a fresh pluginStatus during the test.
QJsonObject samplePluginState(const QString &authoring = {}) {
  const QJsonArray elementTypes{QJsonObject{{"type", "character"}, {"label", QString::fromUtf8("애니메이션 캐릭터")}, {"refType", "controller"}}};
  return QJsonObject{
      {"engineApi", 1},
      {"installed", QJsonArray{QJsonObject{{"id", "cutout-animation"}, {"version", "1.0.0"}, {"name", QString::fromUtf8("컷아웃 캐릭터 애니메이션")}, {"directory", "C:/store/cutout-animation/1.0.0"}, {"elementTypes", elementTypes}},
                               QJsonObject{{"id", "spare"}, {"version", "2.0.0"}, {"name", "Spare"}, {"directory", "C:/store/spare/2.0.0"}}}},
      {"project", QJsonArray{QJsonObject{{"id", "cutout-animation"}, {"version", "1.0.0"}, {"enabled", true}, {"state", "active"}, {"directory", "C:/store/cutout-animation/1.0.0"}, {"authoring", authoring.isEmpty() ? QJsonValue() : QJsonValue(authoring)}, {"diagnostics", QJsonArray{}}},
                             QJsonObject{{"id", "old-plugin"}, {"version", "0.1.0"}, {"enabled", false}, {"state", "disabled"}, {"directory", ""}, {"authoring", QJsonValue()}, {"diagnostics", QJsonArray{QJsonObject{{"code", "PLUGIN_DISABLED"}, {"severity", "warning"}, {"message", "old-plugin disabled"}}}}},
                             QJsonObject{{"id", "gone"}, {"version", "1.0.0"}, {"enabled", true}, {"state", "missing"}, {"directory", ""}, {"authoring", QJsonValue()}, {"diagnostics", QJsonArray{}}}}}};
}
/**
 * Layout sanity of a workspace page at the current window size: the panel fills the page
 * (nothing pushed off-screen by a large minimum) and every visible splitter child keeps
 * at least its minimum along the splitter axis. Returns the problems found.
 */
QStringList workspaceLayoutProblems(QWidget *page, QWidget *panel) {
  QStringList problems;
  auto text = [](const QRect &r) { return QString("%1,%2 %3x%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height()); };
  if (panel->geometry() != page->rect()) problems << "panel " + text(panel->geometry()) + " page " + text(page->rect());
  const QSize need = panel->minimumSizeHint();
  if (need.width() > page->width() || need.height() > page->height()) problems << QString("panel minimum %1x%2").arg(need.width()).arg(need.height());
  for (auto split : panel->findChildren<QSplitter *>()) {
    if (!split->isVisible()) continue;
    for (int i = 0; i < split->count(); ++i) {
      auto child = split->widget(i);
      if (!child->isVisible()) continue;
      const bool horizontal = split->orientation() == Qt::Horizontal;
      const int size = horizontal ? child->width() : child->height();
      const int explicitMin = horizontal ? child->minimumWidth() : child->minimumHeight();
      const int min = explicitMin > 0 ? explicitMin : (horizontal ? child->minimumSizeHint().width() : child->minimumSizeHint().height());
      if (size < min) problems << QString("%1[%2] %3 < %4").arg(split->objectName()).arg(i).arg(size).arg(min);
    }
  }
  for (const char *name : {"cutoutModeBanner", "cutoutStatus", "cutoutClipList", "cutoutMixTable", "cutoutKeyButton"}) {
    auto widget = panel->findChild<QWidget *>(name);
    if (!widget || !widget->isVisible()) { problems << QString(name) + " hidden"; continue; }
    const QRect r(widget->mapTo(panel, QPoint(0, 0)), widget->size());
    // The key button lives in a scroll area; everything else must be fully inside the panel.
    if (QString(name) != "cutoutKeyButton" && !panel->rect().contains(r)) problems << QString(name) + " cut off " + text(r);
  }
  return problems;
}
/**
 * The cutout panel's 스킨 · 표정 · IK · 메시 pages, in both modes: every action button and list of
 * each tab is shown, unsqueezed and inside the page (and inside the viewport of any scroll area
 * on the way) without scrolling; rows the other mode uses (property cutoutModeRow) are skipped.
 */
QStringList lookPageProblems(QWidget *panel) {
  QStringList problems;
  auto tabs = panel->findChild<QTabWidget *>("cutoutLookTabs");
  if (!tabs) return {"cutoutLookTabs missing"};
  const int keep = tabs->currentIndex();
  for (const char *mode : {"cutoutModeAnimate", "cutoutModeSetup"}) {
    panel->findChild<QPushButton *>(mode)->click();
    for (int t = 0; t < tabs->count(); ++t) {
      tabs->setCurrentIndex(t);
      QCoreApplication::processEvents();
      QWidget *page = tabs->widget(t);
      const QString where = QString(mode).mid(10) + ":" + page->objectName();
      if (qobject_cast<QAbstractScrollArea *>(page)) problems << where + " scrolls as a whole";
      QList<QWidget *> widgets;
      for (auto b : page->findChildren<QAbstractButton *>()) {
        if (!b->objectName().startsWith("cutout") || !(qobject_cast<QPushButton *>(b) || qobject_cast<QToolButton *>(b))) continue;
        bool otherMode = false;
        for (QWidget *p = b->parentWidget(); p && p != page; p = p->parentWidget()) otherMode = otherMode || (p->property("cutoutModeRow").isValid() && p->isHidden());
        if (!otherMode) widgets << b;
      }
      for (auto v : page->findChildren<QAbstractItemView *>())
        if (v->objectName().startsWith("cutout")) widgets << v;
      if (widgets.size() < 4) problems << where + QString(" has %1 controls").arg(widgets.size());
      for (QWidget *w : widgets) {
        const QString what = where + "/" + w->objectName();
        if (!w->isVisible()) { problems << what + " hidden"; continue; }
        const QRect r(w->mapTo(page, QPoint(0, 0)), w->size());
        if (qobject_cast<QAbstractButton *>(w) && w->width() < w->sizeHint().width()) problems << what + QString(" squeezed %1 < %2").arg(w->width()).arg(w->sizeHint().width());
        if (!page->rect().contains(r) || r.height() < qMin(w->minimumSizeHint().height(), 20)) problems << what + QString(" clipped %1,%2 %3x%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
        for (QWidget *p = w->parentWidget(); p && p != page; p = p->parentWidget())
          if (auto scroll = qobject_cast<QScrollArea *>(p))
            if (!scroll->viewport()->rect().contains(QRect(w->mapTo(scroll->viewport(), QPoint(0, 0)), w->size()))) problems << what + " scrolled out of " + scroll->objectName();
      }
    }
  }
  tabs->setCurrentIndex(keep);
  return problems;
}
QList<QAction *> pluginPaletteActions(StudioWindow &w) {
  QList<QAction *> out;
  for (auto action : w.findChild<QToolButton *>("elementPalette")->menu()->actions())
    if (action->objectName().startsWith("pluginElement-")) out << action;
  return out;
}
}

void StudioTests::pluginNoPluginsPalette() {
  StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath()));
  w.show();
  w.openProject(qEnvironmentVariable("RAISING_QT_PROJECT"));
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && !w.projectData().isEmpty(), 20000);
  QVERIFY(w.findChild<QAction *>("pluginManagerAction"));
  QVERIFY(w.findChild<QMenu *>("pluginsMenu"));
  QVERIFY(pluginPaletteActions(w).isEmpty());
  QVERIFY(w.pluginManager()->loadedIds().isEmpty());
  QVERIFY(!w.findChild<QWidget *>("pluginElementGroup")->isVisibleTo(w.findChild<QWidget *>("inspector")));
  // Active element types appear in the palette and disappear with the plugin.
  w.applyPluginState(samplePluginState());
  QCOMPARE(pluginPaletteActions(w).size(), 1);
  QCOMPARE(pluginPaletteActions(w).first()->text(), QString::fromUtf8("애니메이션 캐릭터"));
  w.applyPluginState({});
  QVERIFY(pluginPaletteActions(w).isEmpty());
}

void StudioTests::pluginMissingPlaceholder() {
  const auto engine = qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath());
  QTemporaryDir temp(engine + "/.cache/qt-plugin-XXXXXX");
  QVERIFY(temp.isValid());
  QVERIFY(copyScreenSettingsFixture(engine + "/examples/academy-visual", temp.path()));
  QFile file(temp.path() + "/project.json");
  QVERIFY(file.open(QIODevice::ReadOnly));
  auto project = QJsonDocument::fromJson(file.readAll()).object();
  file.close();
  auto visual = project.value("visual").toObject();
  auto screens = visual.value("screens").toArray();
  auto first = screens[0].toObject();
  auto elements = first.value("elements").toArray();
  const QJsonObject placed{{"id", "hero-on-title"}, {"kind", "plugin"}, {"label", "Hero"}, {"x", 40}, {"y", 120}, {"width", 300}, {"height", 450},
                           {"extension", QJsonObject{{"plugin", "cutout-animation"}, {"type", "character"}, {"ref", "hero-main"}, {"props", QJsonObject{{"scale", 1}}}}}};
  elements.append(placed);
  first["elements"] = elements;
  screens[0] = first;
  visual["screens"] = screens;
  project["visual"] = visual;
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  file.write(QJsonDocument(project).toJson());
  file.close();
  StudioWindow w(engine);
  w.resize(1480, 960);
  w.show();
  w.openProject(temp.path());
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && !w.projectData().isEmpty(), 20000);
  QVERIFY2(w.lastError.isEmpty(), qPrintable(w.lastError));
  auto canvas = w.findChild<ScreenCanvas *>("screenCanvas");
  QVERIFY(canvas && canvas->pluginPlaceholder);
  QVERIFY2(canvas->pluginPlaceholder(placed).contains(QString::fromUtf8("플러그인 필요: cutout-animation")), qPrintable(canvas->pluginPlaceholder(placed)));
  QGraphicsItem *item = nullptr;
  for (auto candidate : static_cast<QGraphicsView *>(canvas)->scene()->items())
    if (candidate->data(0).toString() == "hero-on-title") item = candidate;
  QVERIFY(item);
  QVERIFY(item->flags() & QGraphicsItem::ItemIsSelectable);
  canvas->fitCanvas();
  const auto region = canvas->mapFromScene(item->sceneBoundingRect()).boundingRect().intersected(canvas->viewport()->rect());
  QVERIFY(!region.isEmpty());
  const auto image = canvas->viewport()->grab(region).toImage();
  int hatch = 0;
  for (int y = 0; y < image.height(); ++y)
    for (int x = 0; x < image.width(); ++x) {
      const QColor c = image.pixelColor(x, y);
      if (c.red() > 190 && c.green() > 130 && c.green() < 185 && c.blue() < 100) ++hatch;
    }
  QVERIFY2(hatch > 50, qPrintable(QString::number(hatch)));
  // The placeholder element stays selectable and editable like any other element.
  auto tree = w.findChild<QTreeWidget *>("hierarchy");
  QTreeWidgetItemIterator it(tree);
  while (*it && (*it)->data(0, Qt::UserRole).toString() != "hero-on-title") ++it;
  QVERIFY(*it);
  tree->setCurrentItem(*it);
  QTRY_COMPARE(w.selectedId(), QString("hero-on-title"));
  QVERIFY(w.findChild<QWidget *>("pluginElementGroup")->isVisibleTo(w.findChild<QWidget *>("inspector")));
  auto ref = w.findChild<QComboBox *>("pluginRef");
  QVERIFY(ref->currentText().contains("hero-main"));
  QVERIFY(!w.findChild<QPushButton *>("pluginEdit")->isEnabled());
  auto x = w.findChild<QSpinBox *>("elementX");
  x->setValue(64);
  emit x->editingFinished();
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  QJsonObject moved;
  for (auto e : w.projectData().value("visual").toObject().value("screens").toArray()[0].toObject().value("elements").toArray())
    if (e.toObject().value("id") == "hero-on-title") moved = e.toObject();
  auto expected = placed;
  expected["x"] = 64;
  QCOMPARE(moved, expected);
  // Plugin-aware service only (it always sends `plugins`): lock + extension
  // documents load, are listed, and keep their bytes although no plugin is installed.
  if (w.pluginManager()->state().isEmpty()) return;
  QVERIFY(QDir().mkpath(temp.path() + "/plugins/cutout-animation/controller"));
  QFile lock(temp.path() + "/plugins/lock.json");
  QVERIFY(lock.open(QIODevice::WriteOnly));
  lock.write(R"({"formatVersion":"0.1.0","plugins":[{"id":"cutout-animation","version":"999.0.0","enabled":true}]})");
  lock.close();
  const QByteArray documentBytes = R"({"formatVersion":"0.1.0","plugin":"cutout-animation","type":"controller","id":"hero-main","data":{"name":"Hero"}})";
  QFile document(temp.path() + "/plugins/cutout-animation/controller/hero-main.json");
  QVERIFY(document.open(QIODevice::WriteOnly));
  document.write(documentBytes);
  document.close();
  w.findChild<QAction *>("saveProject")->trigger();
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  w.openProject(temp.path());
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 20000);
  QVERIFY2(w.lastError.isEmpty(), qPrintable(w.lastError));
  QVERIFY(canvas->pluginPlaceholder(placed).contains(QString::fromUtf8("플러그인 필요: cutout-animation")));
  auto content = w.findChild<QListWidget *>("contentList");
  bool shown = false;
  for (int i = 0; i < content->count(); ++i) shown = shown || content->item(i)->text().contains("cutout-animation/controller");
  QVERIFY(shown);
  QVERIFY(document.open(QIODevice::ReadOnly));
  QCOMPARE(document.readAll(), documentBytes);
}

void StudioTests::pluginManagerStates() {
  StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath()));
  w.show();
  w.openProject(qEnvironmentVariable("RAISING_QT_PROJECT"));
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && !w.projectData().isEmpty(), 20000);
  w.applyPluginState(samplePluginState());
  bool inspected = false;
  QTimer::singleShot(100, &w, [&] {
    auto dialog = w.findChild<QDialog *>("pluginManager");
    QVERIFY(dialog);
    QTimer::singleShot(15000, dialog, &QDialog::reject);
    auto table = dialog->findChild<QTableWidget *>("pluginTable");
    QCOMPARE(table->rowCount(), 4);
    QMap<QString, QString> states;
    for (int r = 0; r < table->rowCount(); ++r) states[table->item(r, 0)->text()] = table->item(r, 3)->text();
    QVERIFY(states["cutout-animation"].startsWith(QString::fromUtf8("사용 중")));
    QVERIFY(states["cutout-animation"].contains(QString::fromUtf8("편집 도구 없음")));
    QCOMPARE(states["old-plugin"], QString::fromUtf8("사용 중지됨"));
    QCOMPARE(states["gone"], QString::fromUtf8("설치 필요"));
    QCOMPARE(states["spare"], QString::fromUtf8("설치됨 · 프로젝트 미사용"));
    for (int r = 0; r < table->rowCount(); ++r)
      if (table->item(r, 0)->text() == "old-plugin") table->setCurrentCell(r, 0);
    QVERIFY(dialog->findChild<QPlainTextEdit *>("pluginDiagnostics")->toPlainText().contains("PLUGIN_DISABLED"));
    QVERIFY(dialog->findChild<QPushButton *>("pluginEnable")->isEnabled());
    QVERIFY(!dialog->findChild<QPushButton *>("pluginDisable")->isEnabled());
    for (int r = 0; r < table->rowCount(); ++r)
      if (table->item(r, 0)->text() == "cutout-animation") table->setCurrentCell(r, 0);
    QVERIFY(!dialog->findChild<QPushButton *>("pluginEnable")->isEnabled());
    QVERIFY(dialog->findChild<QPushButton *>("pluginDisable")->isEnabled());
    inspected = true;
    dialog->reject();
  });
  w.findChild<QAction *>("pluginManagerAction")->trigger();
  QVERIFY(inspected);
  // Placing an element of an active plugin links the first document of its refType.
  const auto actions = pluginPaletteActions(w);
  QCOMPARE(actions.size(), 1);
  actions.first()->trigger();
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && w.selectedId().startsWith("character-"), 10000);
  QJsonObject added;
  for (auto s : w.projectData().value("visual").toObject().value("screens").toArray())
    for (auto e : s.toObject().value("elements").toArray())
      if (e.toObject().value("id") == w.selectedId()) added = e.toObject();
  QCOMPARE(added.value("kind").toString(), QString("plugin"));
  QCOMPARE(added.value("extension").toObject(), (QJsonObject{{"plugin", "cutout-animation"}, {"type", "character"}, {"ref", ""}, {"props", QJsonObject{}}}));
  QCOMPARE(added.value("width").toInt(), 300);
  w.findChild<QAction *>("undoEdit")->trigger();
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
}

void StudioTests::pluginWorkspaceSwitch() {
  // A real workspace plugin: the cutout authoring module from its own build tree.
  const auto engine = qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath());
  const auto dll = qEnvironmentVariable("RAISING_CUTOUT_STUDIO_DLL", engine + "/dist/cutout/build-win/studio/cutout-studio.dll");
  if (!QFileInfo::exists(dll)) QSKIP("Build plugins/cutout-animation (tools/build.mjs win studio) or set RAISING_CUTOUT_STUDIO_DLL");
  const auto projectA = qEnvironmentVariable("RAISING_QT_PROJECT");
  StudioWindow w(engine);
  w.resize(1280, 720);
  w.show();
  QVERIFY(QTest::qWaitForWindowExposed(&w));
  w.activateWindow();  // window shortcuts (Ctrl+Shift+A) need the active window
  QVERIFY(QTest::qWaitForWindowActive(&w));
  w.openProject(projectA);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && !w.projectData().isEmpty(), 20000);
  const bool pluginService = !w.pluginManager()->state().isEmpty();  // replies carry `plugins`
  // Without a workspace plugin the studio looks as before: no switcher, no 작업 공간 menu.
  auto switcher = w.findChild<QTabBar *>("studioWorkspaceSwitcher");
  QVERIFY(switcher && !switcher->isVisibleTo(&w));
  QVERIFY(!w.findChild<QMenu *>("studioWorkspaceMenu")->menuAction()->isVisible());
  QVERIFY(w.workspaceKeys().isEmpty());
  QVERIFY(!w.showWorkspace("cutout-animation/character"));
  // A non-default dock layout, so restoring it is not just "the default comes back".
  w.findChild<QDockWidget *>("projectDock")->hide();
  w.applyPluginState(samplePluginState(dll));
  QVERIFY2(w.pluginManager()->loadedIds().contains("cutout-animation"), qPrintable(w.pluginManager()->loadError("cutout-animation")));
  QCOMPARE(w.workspaceKeys(), QStringList{"cutout-animation/character"});
  QVERIFY(switcher->isVisibleTo(&w));
  QCOMPARE(switcher->count(), 2);
  auto action = w.findChild<QAction *>("studioWorkspace-cutout-animation-character");
  QVERIFY(action && w.findChild<QAction *>("studioWorkspace-screen"));
  QCOMPARE(action->shortcut(), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
  auto dock = w.findChild<QDockWidget *>("cutoutAnimationDock");
  QVERIFY(dock && dock->findChild<QWidget *>("cutoutWorkspaceLauncher") && !dock->findChild<QWidget *>("cutoutPanel"));
  QTest::qWait(100);
  const auto revision = w.currentView().value("revision"), undo = w.currentView().value("undo"), dirty = w.currentView().value("dirty");
  auto dockGeometry = [&](const char *name) { return w.findChild<QDockWidget *>(name)->geometry(); };
  const auto hierarchyAt = dockGeometry("hierarchyDock"), launcherAt = dockGeometry("cutoutAnimationDock"), inspectorAt = dockGeometry("inspectorDock");
  QStringList docks;
  for (auto d : w.findChildren<QDockWidget *>(Qt::FindDirectChildrenOnly))
    if (d->isVisible()) docks << d->objectName();
  QVERIFY(docks.contains("cutoutAnimationDock") && !docks.contains("projectDock"));
  auto shownDocks = [&] {
    QStringList out;
    for (auto d : w.findChildren<QDockWidget *>(Qt::FindDirectChildrenOnly))
      if (d->isVisible()) out << d->objectName();
    return out;
  };

  // Enter through the switcher tab: the panel is the whole central area, every dock hidden.
  QTest::mouseClick(switcher, Qt::LeftButton, Qt::NoModifier, switcher->tabRect(1).center());
  QCOMPARE(w.currentWorkspace(), QString("cutout-animation/character"));
  QVERIFY(action->isChecked());
  QVERIFY(shownDocks().isEmpty());
  auto page = w.findChild<QWidget *>("studioWorkspacePage-cutout-animation-character");
  auto panel = w.findChild<QWidget *>("cutoutPanel");
  QVERIFY(page && panel && panel->parentWidget() == page);
  QCOMPARE(w.findChild<QStackedWidget *>("studioCentralStack")->currentWidget(), page);
  QVERIFY(!w.findChild<QWidget *>("screenCanvas")->isVisible());
  // Host menus and the undo/save/play tool bar stay; screen-only tools are hidden.
  QVERIFY(w.menuBar()->isVisible() && w.findChild<QToolBar *>("mainToolbar")->isVisible());
  for (const char *name : {"undoEdit", "redoEdit", "saveProject", "playProject"}) QVERIFY2(w.findChild<QAction *>(name)->isVisible(), name);
  QVERIFY(!w.findChild<QAction *>("addButton")->isVisible());
  QTest::qWait(100);
  QCOMPARE(w.size(), QSize(1280, 720));  // the workspace minimum fits a 1280x720 window
  QStringList problems = workspaceLayoutProblems(page, panel);
  QVERIFY2(problems.isEmpty(), qPrintable(problems.join(" | ")));
  problems = lookPageProblems(panel);  // 1280x720: the look pages keep every button
  QVERIFY2(problems.isEmpty(), qPrintable(problems.join(" | ")));
  auto canvas = panel->findChild<QWidget *>("cutoutCanvas");
  QVERIFY2(canvas->width() >= 240 && canvas->height() >= 150, qPrintable(QString("%1x%2").arg(canvas->width()).arg(canvas->height())));
  // Ctrl+Shift+A toggles back to 화면 편집 with exactly the previous dock layout.
  QTest::keyClick(canvas, Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier);
  QCOMPARE(w.currentWorkspace(), QString());
  QVERIFY(w.findChild<QWidget *>("screenCanvas")->isVisible());
  QVERIFY(w.findChild<QAction *>("addButton")->isVisible());
  QCOMPARE(shownDocks(), docks);
  QTest::qWait(100);
  QCOMPARE(dockGeometry("hierarchyDock"), hierarchyAt);
  QCOMPARE(dockGeometry("cutoutAnimationDock"), launcherAt);
  // Tab groups re-measure their tab bar once: from then on every round trip is byte-exact.
  QVERIFY(qAbs(dockGeometry("inspectorDock").height() - inspectorAt.height()) <= 8);
  const auto settled = w.saveState(1);
  QTest::keyClick(&w, Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier);
  QCOMPARE(w.currentWorkspace(), QString("cutout-animation/character"));
  QTest::keyClick(canvas, Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier);
  QCOMPARE(w.currentWorkspace(), QString());
  QTest::qWait(100);
  QCOMPARE(w.saveState(1), settled);
  QTest::keyClick(&w, Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier);
  QCOMPARE(w.currentWorkspace(), QString("cutout-animation/character"));
  const int leftWidth = panel->findChild<QWidget *>("cutoutLeftSplitter")->width();
  // Ultrawide and very tall: only the canvas / time area grow, side panels keep their width.
  for (const QSize size : {QSize(1920, 1080), QSize(3840, 1080), QSize(5120, 1440), QSize(1440, 2560)}) {
    w.resize(size);
    QTest::qWait(120);
    QCOMPARE(w.size(), size);
    problems = workspaceLayoutProblems(page, panel) + lookPageProblems(panel);
    QVERIFY2(problems.isEmpty(), qPrintable(QString("%1x%2: ").arg(size.width()).arg(size.height()) + problems.join(" | ")));
    QCOMPARE(panel->findChild<QWidget *>("cutoutLeftSplitter")->width(), leftWidth);
    if (size.width() >= 3840) QVERIFY2(canvas->width() > size.width() - 700, qPrintable(QString::number(canvas->width())));
    if (size.height() >= 2560) QVERIFY2(panel->findChild<QWidget *>("cutoutBottomTabs")->height() > 700, "tall screens grow the timeline too");
  }
  w.resize(1280, 720);
  QTest::qWait(120);

  // 보기 → 작업 공간 → 화면 편집 also returns; switching never touched the documents.
  w.findChild<QAction *>("studioWorkspace-screen")->trigger();
  QCOMPARE(w.currentWorkspace(), QString());
  QCOMPARE(shownDocks(), docks);
  QCOMPARE(w.currentView().value("revision"), revision);
  QCOMPARE(w.currentView().value("undo"), undo);
  QCOMPARE(w.currentView().value("dirty"), dirty);
  QVERIFY(!w.busy());

  // Project reopen: leaving the project inside the workspace comes back to it.
  QVERIFY(w.showWorkspace("cutout-animation/character"));
  if (!pluginService) return;  // old service: opening another project would not unload the DLL
  QTemporaryDir temp(engine + "/.cache/qt-workspace-XXXXXX");
  QVERIFY(copyScreenSettingsFixture(engine + "/examples/academy-visual", temp.path()));
  w.openProject(temp.path());
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && QDir::cleanPath(w.currentView().value("root").toString()).compare(QDir::cleanPath(temp.path()), Qt::CaseInsensitive) == 0, 20000);
  QVERIFY(!w.pluginManager()->loadedIds().contains("cutout-animation"));
  QCOMPARE(w.currentWorkspace(), QString());  // the plugin left with its project
  QVERIFY(!switcher->isVisibleTo(&w));
  QCOMPARE(shownDocks(), QStringList(docks).filter(QRegularExpression("^(?!cutoutAnimationDock$)")));
  w.openProject(projectA);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && QDir::cleanPath(w.currentView().value("root").toString()).compare(QDir::cleanPath(projectA), Qt::CaseInsensitive) == 0, 20000);
  w.applyPluginState(samplePluginState(dll));
  QCOMPARE(w.currentWorkspace(), QString("cutout-animation/character"));
  QVERIFY(w.findChild<QWidget *>("cutoutPanel")->isVisible());
  QVERIFY(shownDocks().isEmpty());
  // Unloading inside the workspace (plugin disabled) restores the screen editor.
  w.applyPluginState({});
  QCOMPARE(w.currentWorkspace(), QString());
  QVERIFY(w.findChild<QWidget *>("screenCanvas")->isVisible());
  QVERIFY(!w.findChild<QWidget *>("cutoutPanel"));
}

void StudioTests::pluginLoaderRejectsNonPlugin() {
  const auto engine = qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath());
  StudioWindow w(engine);
  w.show();
  w.openProject(qEnvironmentVariable("RAISING_QT_PROJECT"));
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && !w.projectData().isEmpty(), 20000);
  auto manager = w.pluginManager();
  QString error;
  auto qtCore = QCoreApplication::applicationDirPath() + "/Qt6Core.dll";
  if (!QFileInfo::exists(qtCore)) qtCore = QLibraryInfo::path(QLibraryInfo::BinariesPath) + "/Qt6Core.dll";
  QVERIFY(QFileInfo::exists(qtCore));
  QVERIFY(!manager->load("cutout-animation", qtCore, &error));
  QVERIFY(!error.isEmpty());
  QVERIFY(!manager->load("cutout-animation", engine + "/no-such-plugin.dll", &error));
  QVERIFY(error.contains("no-such-plugin.dll"));
  QTemporaryDir temp(engine + "/.cache/qt-plugin-XXXXXX");
  QFile fake(temp.path() + "/fake.dll");
  QVERIFY(fake.open(QIODevice::WriteOnly));
  fake.write("not a library");
  fake.close();
  QVERIFY(!manager->load("cutout-animation", fake.fileName(), &error));
  // A real Qt plugin with another interface is refused from metadata, never instantiated.
  const auto platforms = QDir(QCoreApplication::applicationDirPath() + "/platforms").entryInfoList({"*.dll"}, QDir::Files);
  if (!platforms.isEmpty()) {
    QVERIFY(!manager->load("cutout-animation", platforms.first().absoluteFilePath(), &error));
    QVERIFY2(error.contains("org.qt-project"), qPrintable(error));
  }
  QVERIFY(manager->loadedIds().isEmpty());
  // Through the service-state path: a notice, a placeholder, and no retry loop.
  auto log = w.findChild<QPlainTextEdit *>("executionLog");
  const auto before = log->toPlainText().count(QString::fromUtf8("불러오지 못했습니다"));
  w.applyPluginState(samplePluginState(qtCore));
  QVERIFY(manager->loadedIds().isEmpty());
  QVERIFY(!manager->loadError("cutout-animation").isEmpty());
  QCOMPARE(log->toPlainText().count(QString::fromUtf8("불러오지 못했습니다")), before + 1);
  w.applyPluginState(samplePluginState(qtCore));
  QCOMPARE(log->toPlainText().count(QString::fromUtf8("불러오지 못했습니다")), before + 1);
  const QJsonObject element{{"kind", "plugin"}, {"extension", QJsonObject{{"plugin", "cutout-animation"}, {"type", "character"}}}};
  QVERIFY(manager->placeholder(element).contains(QString::fromUtf8("불러오기 실패")));
  QCOMPARE(pluginPaletteActions(w).size(), 1);  // placing elements does not need the DLL
  QVERIFY(!w.busy());
}
