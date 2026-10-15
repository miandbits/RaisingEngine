// Drives the real authoring panel through a MockStudioHost (in-memory documents, edit
// create/replace/delete/batch with revision++ and undo/redo, importImage stub).
// Every gesture must issue exactly one edit and leave documents valid for the core.
#include <QtTest>
#include <QtWidgets>
#include <functional>
#include <set>
#include "cutout-plugin.hpp"
#include "cutout/editing.hpp"
#include "dopesheet.hpp"
#include "graph-editor.hpp"
#include "../../core/tests/stress-rig.hpp"

using namespace cutout_studio;
using cutout::Json;

class MockHost : public raising::StudioHost {
 public:
  QJsonArray documents, assetList;
  QList<QJsonArray> history, future;
  int revision = 1, edits = 0, imports = 0;
  bool isBusy = false;
  QJsonObject lastEdit;
  QString lastError;
  QMainWindow mainWindow;
  QMap<QString, QMenu*> menus;
  raising::StudioPlugin* plugin = nullptr;
  QString root;

  QJsonObject view() const override {
    // Diagnostics are recomputed only when the documents change (a real host caches its view too),
    // so the large-rig timings measure the panel rather than this mock's validation.
    if (cachedRevision != revision || cachedDocuments != documents) { cachedDiagnostics = skipDiagnostics ? QJsonArray() : diagnostics(); cachedRevision = revision; cachedDocuments = documents; }
    return {{"revision", revision}, {"documents", documents}, {"diagnostics", cachedDiagnostics}, {"undo", !history.isEmpty()}, {"redo", !future.isEmpty()}, {"dirty", !history.isEmpty()}};
  }
  mutable int cachedRevision = -1;
  bool skipDiagnostics = false;  // large-rig timings: the real host's diagnostics come with its reply
  mutable QJsonArray cachedDocuments, cachedDiagnostics;
  QJsonArray diagnostics() const {
    QJsonArray out;
    std::map<std::string, std::shared_ptr<cutout::Rig>> rigs;
    auto push = [&](const QString& file, const std::vector<cutout::Diagnostic>& ds) {
      for (const auto& d : ds) out.append(QJsonObject{{"severity", qs(d.severity)}, {"code", qs(d.code)}, {"message", qs(d.message)}, {"file", file}, {"pointer", "/data" + qs(d.pointer)}});
    };
    for (const auto& v : documents) {
      const QJsonObject env = v.toObject().value("data").toObject();
      if (env.value("type") != "rig") continue;
      const Json data = toJson(env.value("data").toObject());
      const auto ds = cutout::validateRig(data);
      push(v.toObject().value("file").toString(), ds);
      try { rigs[ss(env.value("id").toString())] = std::make_shared<cutout::Rig>(cutout::parseRig("r", data)); } catch (...) {}
    }
    for (const auto& v : documents) {
      const QJsonObject env = v.toObject().value("data").toObject();
      if (env.value("type") != "controller") continue;
      const Json data = toJson(env.value("data").toObject());
      auto it = rigs.find(data.value("rig", std::string()));
      push(v.toObject().value("file").toString(), cutout::validateController(data, it == rigs.end() ? nullptr : it->second.get()));
    }
    return out;
  }
  QJsonArray assets() const override { return assetList; }
  void request(const QString& method, const QJsonObject& params, std::function<void(QJsonObject)> done) override {
    QJsonObject reply;
    if (method == "edit") {
      const QJsonObject edit = params.value("edit").toObject();
      if (edit.value("type") != "undo" && edit.value("type") != "redo") { edits++; lastEdit = edit; }
      lastError = apply(edit);
      if (!lastError.isEmpty()) reply["error"] = lastError;
    } else if (method == "importImage") {
      imports++;
      const QString file = params.value("file").toString();
      const QString id = "img-" + QFileInfo(file).completeBaseName().toLower();
      assetList.append(QJsonObject{{"id", id}, {"kind", "image"}, {"path", "assets/" + QFileInfo(file).fileName()}, {"absolutePath", file}});
      reply["importedAssetId"] = id;
    }
    reply["view"] = view();
    if (plugin) plugin->viewChanged();
    if (done) done(reply);
  }
  QString apply(const QJsonObject& edit) {
    const QString type = edit.value("type").toString();
    if (type == "undo" || type == "redo") {
      auto& from = type == "undo" ? history : future;
      auto& to = type == "undo" ? future : history;
      if (from.isEmpty()) return {};
      to.append(documents);
      documents = from.takeLast();
      revision++;
      return {};
    }
    const QJsonArray list = type == "batch" ? edit.value("edits").toArray() : QJsonArray{edit};
    QJsonArray next = documents;
    for (const auto& v : list) {
      const QJsonObject e = v.toObject();
      const QString file = e.value("file").toString();
      int at = -1;
      for (int i = 0; i < next.size(); i++) if (next[i].toObject().value("file") == file) at = i;
      if (e.value("type") == "create") {
        if (at >= 0) return "이미 있는 파일입니다: " + file;
        if (!file.startsWith("plugins/")) return "확장 문서 경로가 아닙니다.";
        next.append(QJsonObject{{"file", file}, {"kind", "extension"}, {"data", e.value("data")}});
      } else if (e.value("type") == "replace") {
        if (at < 0) return "문서를 찾을 수 없습니다.";
        QJsonObject doc = next[at].toObject();
        doc["data"] = e.value("data");
        next[at] = doc;
      } else if (e.value("type") == "delete") {
        if (at < 0) return "이 문서는 삭제할 수 없습니다.";
        next.removeAt(at);
      } else return "알 수 없는 편집";
    }
    if (next == documents) return {};
    history.append(documents);
    future.clear();
    documents = next;
    revision++;
    return {};
  }
  bool busy() const override { return isBusy; }
  QWidget* window() const override { return const_cast<QMainWindow*>(&mainWindow); }
  QString engineRoot() const override { return root; }
  QString projectRoot() const override { return root; }
  QString pluginDirectory(const QString&) const override { return root; }
  void addDock(const QString& title, const QString& name, QWidget* widget, Qt::DockWidgetArea area) override {
    auto* dock = new QDockWidget(title, &mainWindow);
    dock->setObjectName(name);
    dock->setWidget(widget);
    mainWindow.addDockWidget(area, dock);
  }
  QMenu* pluginMenu(const QString& id) override {
    if (!menus.contains(id)) menus[id] = mainWindow.menuBar()->addMenu(id);
    return menus[id];
  }
  QJsonObject selectedElement() const override { return {}; }

  Json data(const QString& type, const QString& id) const {
    for (const auto& v : documents) {
      const QJsonObject env = v.toObject().value("data").toObject();
      if (env.value("type") == type && env.value("id") == id) return toJson(env.value("data").toObject());
    }
    return Json();
  }
};

/** Optional workspace interface of a newer host (the plain MockHost above is an older host). */
class MockWorkspaceHost : public raising::StudioWorkspaceHost {
 public:
  QString current;
  QStringList requests;
  void showWorkspace(const QString& id) override { requests << id; current = id; }
  QString currentWorkspace() const override { return current; }
};

class StudioTests : public QObject {
  Q_OBJECT
  MockHost host;
  MockWorkspaceHost workspaceHost;
  CutoutStudioPlugin plugin;
  CutoutPanel* panel = nullptr;
  QStringList answers;
  std::optional<CutoutPanel::PartChoice> nextPart;
  std::optional<CutoutPanel::SkinImageChoice> nextSkinImage, skinPrefill;  // scripted 그림 지정 / what it was offered
  QTemporaryDir dir;
  int gestures = 0;

  void pump() { for (int i = 0; i < 6; i++) QCoreApplication::processEvents(); }
  QString png(const QString& name, QSize size, QColor color) {
    QImage img(size, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(QPointF(0, 0), QSizeF(size)));
    p.end();
    const QString path = dir.filePath(name + ".png");
    img.save(path);
    return path;
  }
  /** Run one user gesture; it must issue `expected` edits and leave all documents valid. */
  bool gesture(const char* what, int expected, const std::function<void()>& fn) {
    const int before = host.edits;
    try { fn(); pump(); } catch (const std::exception& e) { qWarning("%s: exception %s", what, e.what()); return false; }
    gestures++;
    if (host.edits - before != expected) {
      qWarning("%s: expected %d edit(s), got %d (status: %s)", what, expected, host.edits - before, qPrintable(panel->status()));
      return false;
    }
    if (!host.lastError.isEmpty()) { qWarning("%s: host refused edit: %s", what, qPrintable(host.lastError)); return false; }
    for (const auto& d : host.diagnostics())
      if (d.toObject().value("severity") == "error") { qWarning("%s: invalid document %s", what, qPrintable(QJsonDocument(d.toObject()).toJson())); return false; }
    return true;
  }
  template <class T> T* find(const char* name) { return panel->window()->findChild<T*>(name); }
  void click(const char* name) {
    auto* b = find<QAbstractButton>(name);
    QVERIFY2(b, name);
    b->click();
  }
  void mouse(QWidget* w, QEvent::Type type, QPointF pos, Qt::MouseButton button, Qt::MouseButtons buttons) {
    QMouseEvent e(type, pos, w->mapToGlobal(pos), button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &e);
  }
  void drag(QWidget* w, QPointF from, QPointF to) {
    mouse(w, QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
    for (int i = 1; i <= 4; i++) mouse(w, QEvent::MouseMove, from + (to - from) * (i / 4.0), Qt::NoButton, Qt::LeftButton);
    mouse(w, QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
  }
  void setSpin(const char* name, double v) {
    auto* s = find<QDoubleSpinBox>(name);
    QVERIFY2(s, name);
    s->setValue(v);
    Q_EMIT s->editingFinished();
  }
  Json rig() const { return host.data("rig", "hero"); }
  Json controller() const { return host.data("controller", "hero"); }
  QPointF world(const std::string& bone) const { const auto w = cutout::setupWorld(rig()).at(bone); return {w.e, w.f}; }
  std::vector<cutout::DrawItem> drawList() const {
    cutout::Instance inst(std::make_shared<const cutout::Rig>(cutout::parseRig("hero", rig())));
    return inst.drawList();
  }
  const Json* track(const char* clip, const char* target, const char* property) const {
    static Json keep;
    keep = rig();
    for (const auto& c : keep["clips"]) if (c["id"] == clip) for (const auto& t : c["tracks"]) if (t["target"] == target && t["property"] == property) return &t;
    return nullptr;
  }
  Json keyOf(const char* clip, const char* target, const char* property, int ms) const {
    if (const Json* t = track(clip, target, property))
      for (const auto& k : (*t)["keys"]) if (k["timeMs"] == ms) return k;
    return Json();
  }
  /** Right-click `at` on `w` and pick the context-menu action `name` (menus run their own loop). */
  void contextPick(QWidget* w, QPoint at, const char* name) {
    QTimer::singleShot(50, [name] {
      auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
      QAction* action = menu ? menu->findChild<QAction*>(name) : nullptr;
      if (!action) { if (menu) menu->close(); return; }
      menu->setActiveAction(action);
      QTest::keyClick(menu, Qt::Key_Return);
    });
    QTimer::singleShot(3000, [] { if (auto* m = QApplication::activePopupWidget()) m->close(); });  // never hang
    QContextMenuEvent e(QContextMenuEvent::Mouse, at, w->mapToGlobal(at));
    QCoreApplication::sendEvent(w, &e);
  }
  int rowOf(const QString& target, const QString& property) const {
    const auto& rows = panel->dopesheet()->rows();
    for (int i = 0; i < rows.size(); i++) if (rows[i].target == target && rows[i].property == property) return i;
    return -1;
  }
  void tap(QWidget* w, QPointF at, Qt::KeyboardModifiers mods = Qt::NoModifier) {
    QMouseEvent press(QEvent::MouseButtonPress, at, w->mapToGlobal(at), Qt::LeftButton, Qt::LeftButton, mods);
    QCoreApplication::sendEvent(w, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, at, w->mapToGlobal(at), Qt::LeftButton, Qt::NoButton, mods);
    QCoreApplication::sendEvent(w, &release);
  }
  void wheel(QWidget* w, QPointF at, int steps, Qt::KeyboardModifiers mods) {
    for (int i = 0; i < std::abs(steps); i++) {
      QWheelEvent e(at, w->mapToGlobal(at), QPoint(), QPoint(0, steps > 0 ? 120 : -120), Qt::NoButton, mods, Qt::NoScrollPhase, false);
      QCoreApplication::sendEvent(w, &e);
    }
  }
  void undo() { host.request("edit", {{"edit", QJsonObject{{"type", "undo"}}}}, {}); pump(); }
  void redo() { host.request("edit", {{"edit", QJsonObject{{"type", "redo"}}}}, {}); pump(); }
  QVector<int> timesOf(const char* clip, const char* target, const char* property) const {
    QVector<int> out;
    if (const Json* t = track(clip, target, property)) for (const auto& k : (*t)["keys"]) out << k["timeMs"].get<int>();
    return out;
  }
  static QSet<KeyRef> asSet(const QVector<KeyRef>& keys) { return QSet<KeyRef>(keys.begin(), keys.end()); }
  /** Once the modal dialog `name` is open (exec() blocks the test), run `drive` on it; true =
   *  confirm with its OK button, false = cancel. A dialog that never shows is not waited for. */
  void onDialog(const QString& name, std::function<bool(QDialog*)> drive) {
    auto tries = std::make_shared<int>(0);
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [name, drive, tries, poll] {
      auto* d = qobject_cast<QDialog*>(QApplication::activeModalWidget());
      if (d && d->objectName() == name) {
        const bool ok = drive(d);
        if (!d->isVisible()) return;
        auto* box = d->findChild<QDialogButtonBox*>();
        if (ok && box) box->button(QDialogButtonBox::Ok)->click();
        if (d->isVisible()) d->reject();  // refused confirm (or cancel): never leave exec() running
        return;
      }
      if (++*tries > 150) { if (d) d->reject(); return; }
      QTimer::singleShot(20, *poll);
    };
    QTimer::singleShot(0, *poll);
  }
  static void clickRow(QAbstractItemView* view, const QModelIndex& index) {
    view->scrollTo(index);
    const QRect r = view->visualRect(index);
    QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(r.left() + qMin(30, r.width() / 2), r.center().y()));
  }
  static bool clickListItem(QListWidget* list, const QString& data) {
    for (int i = 0; i < list->count(); i++)
      if (list->item(i)->data(Qt::UserRole).toString() == data) { clickRow(list, list->model()->index(i, 0)); return true; }
    return false;
  }
  bool clickTreeRow(const QString& ref) {
    auto* tree = find<QTreeWidget>("cutoutBoneTree");
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
      if ((*it)->data(0, Qt::UserRole).toString() == ref) { clickRow(tree, tree->indexFromItem(*it)); pump(); return true; }
    return false;
  }
  Json skinAttachment(const char* skin, const char* slot, const char* name) const {
    const Json r = rig();  // (a temporary's subobject would dangle in the range-for)
    for (const auto& s : r["skins"]) if (s["id"] == skin) for (const auto& a : s["attachments"]) if (a["slot"] == slot && a["name"] == name) return a;
    return Json();
  }
  /** Each look tab in turn: its buttons and lists are shown, unclipped by the page or by a scroll
   *  area's viewport (what the user sees without scrolling), and the page itself never scrolls. */
  QStringList lookPageProblems() {
    QStringList problems;
    auto* tabs = find<QTabWidget>("cutoutLookTabs");
    const int keep = tabs->currentIndex();
    for (int t = 0; t < tabs->count(); t++) {
      tabs->setCurrentIndex(t);
      pump();
      QWidget* page = tabs->widget(t);
      if (qobject_cast<QAbstractScrollArea*>(page)) problems << page->objectName() + " scrolls as a whole";
      QList<QWidget*> widgets;
      // Action buttons (form check boxes are values) except the rows the other mode uses.
      for (auto* b : page->findChildren<QAbstractButton*>()) {
        if (!b->objectName().startsWith("cutout") || !(qobject_cast<QPushButton*>(b) || qobject_cast<QToolButton*>(b))) continue;
        bool otherMode = false;
        for (QWidget* p = b->parentWidget(); p && p != page; p = p->parentWidget()) otherMode = otherMode || (p->property("cutoutModeRow").isValid() && p->isHidden());
        if (!otherMode) widgets << b;
      }
      for (auto* l : page->findChildren<QAbstractItemView*>()) if (l->objectName().startsWith("cutout")) widgets << l;
      if (widgets.size() < 4) problems << page->objectName() + QString(" has %1 controls").arg(widgets.size());
      for (QWidget* w : widgets) {
        const QString what = page->objectName() + "/" + w->objectName();
        if (!w->isVisible()) { problems << what + " hidden"; continue; }
        const QRect r(w->mapTo(page, QPoint(0, 0)), w->size());
        if (qobject_cast<QAbstractButton*>(w) && w->width() < w->sizeHint().width()) problems << what + QString(" squeezed to %1 < %2").arg(w->width()).arg(w->sizeHint().width());
        if (!page->rect().contains(r) || r.height() < qMin(w->minimumSizeHint().height(), 20)) problems << what + QString(" clipped %1,%2 %3x%4 in %5x%6").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height()).arg(page->width()).arg(page->height());
        for (QWidget* p = w->parentWidget(); p && p != page; p = p->parentWidget())
          if (auto* scroll = qobject_cast<QScrollArea*>(p)) {
            const QRect inView(w->mapTo(scroll->viewport(), QPoint(0, 0)), w->size());
            if (!scroll->viewport()->rect().contains(inView)) problems << what + " scrolled out of " + scroll->objectName();
          }
      }
    }
    tabs->setCurrentIndex(keep);
    return problems;
  }

 private Q_SLOTS:
  void initTestCase() {
    QVERIFY(dir.isValid());
    // Onion-skin preferences live in QSettings: keep them in the temp dir, never the user's registry.
    QCoreApplication::setOrganizationName("RaisingEngineCutoutTests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    host.root = dir.path();
    host.plugin = &plugin;
    QCOMPARE(plugin.pluginId(), QString("cutout-animation"));
    plugin.activate(&host);
    panel = plugin.panel();
    QVERIFY(panel);
    QVERIFY(host.mainWindow.findChild<QDockWidget*>("cutoutAnimationDock"));
    QVERIFY(host.menus.value("cutout-animation") && host.mainWindow.findChild<QAction*>("cutoutNewCharacterAction"));
    panel->askText = [this](const QString&, const QString&, const QString& value) { return answers.isEmpty() ? value : answers.takeFirst(); };
    panel->askConfirm = [](const QString&, const QString&) { return true; };
    panel->askPart = [this](CutoutPanel::PartChoice& c) {
      if (!nextPart) return false;
      c = *nextPart;
      nextPart.reset();
      return true;
    };
    panel->askSkinImage = [this](CutoutPanel::SkinImageChoice& c) {
      skinPrefill = c;
      if (!nextSkinImage) return false;
      c = *nextSkinImage;
      nextSkinImage.reset();
      return true;
    };
    host.mainWindow.resize(1700, 1200);
    host.mainWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&host.mainWindow));
  }

  void createCharacter() {
    answers = {"hero"};
    QVERIFY(gesture("new character", 1, [&] { click("cutoutNewCharacter"); }));
    QCOMPARE(host.lastEdit.value("type").toString(), QString("batch"));
    QCOMPARE(host.lastEdit.value("edits").toArray().size(), 2);
    QVERIFY(rig().is_object() && controller().is_object());
    QCOMPARE(qs(controller()["defaultClip"].get<std::string>()), QString("idle"));
    QCOMPARE(panel->rigId(), QString("hero"));
    QVERIFY(find<QTreeWidget>("cutoutCharacterList")->topLevelItemCount() == 1);
  }

  void addPartsFromGeneratedPngs() {
    const QString body = png("body", QSize(60, 100), QColor("#e07a5f"));
    const QString open = png("eye-open", QSize(30, 12), QColor("#3d405b"));
    const QString closed = png("eye-closed", QSize(30, 4), QColor("#3d405b"));
    panel->selectItem("bone:root");
    nextPart = CutoutPanel::PartChoice{QString(), body, "body", QString(), "root"};
    QVERIFY(gesture("add body part", 1, [&] { click("cutoutAddPart"); }));
    QCOMPARE(host.imports, 1);
    QCOMPARE(rig()["attachments"][0]["width"].get<double>(), 60.0);
    QCOMPARE(rig()["attachments"][0]["height"].get<double>(), 100.0);
    answers = {"head"};
    QVERIFY(gesture("add head bone", 1, [&] { click("cutoutAddBone"); }));
    QCOMPARE(qs(rig()["bones"][1]["parent"].get<std::string>()), QString("root"));
    nextPart = CutoutPanel::PartChoice{QString(), open, "eyes", "open", "head"};
    QVERIFY(gesture("add eyes (open)", 1, [&] { click("cutoutAddPart"); }));
    // Same slot id + new attachment name = swappable expression image (eyes open/closed).
    QVERIFY(gesture("import closed eye", 1, [&] { panel->importAndAddPart(closed, "eyes", "closed", "head"); }));
    QCOMPARE(rig()["attachments"].size(), size_t(3));
    QCOMPARE(rig()["slots"].size(), size_t(2));
    QCOMPARE(qs(rig()["slots"][1]["attachment"].get<std::string>()), QString("open"));
  }

  void reparentKeepsWorldPose() {
    panel->selectItem("bone:root");
    answers = {"arm"};
    QVERIFY(gesture("add arm bone", 1, [&] { click("cutoutAddBone"); }));
    const QPointF before = world("arm");
    QVERIFY(gesture("reparent arm -> head", 1, [&] { QVERIFY(panel->requestReparent("arm", "head")); }));
    QCOMPARE(qs(rig()["bones"][2]["parent"].get<std::string>()), QString("head"));
    QVERIFY(std::hypot(world("arm").x() - before.x(), world("arm").y() - before.y()) < 1e-6);
    QVERIFY(gesture("cycle refused", 0, [&] { QVERIFY(!panel->requestReparent("head", "arm")); }));
    QVERIFY(panel->status().contains("순환"));
  }

  void setPivotWithCanvas() {
    panel->setMode(CutoutPanel::Mode::Setup);
    panel->selectItem("bone:head");
    panel->setTool(RigCanvas::Pivot);
    const auto drawBefore = drawList();
    const QPointF head = world("head"), target = head + QPointF(12, 8);
    RigCanvas* canvas = panel->canvas();
    QVERIFY(canvas->isVisible() && canvas->width() > 100);
    const QPointF at = canvas->rigToWidget(target);
    QVERIFY(gesture("pivot click", 1, [&] {
      mouse(canvas, QEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton);
      mouse(canvas, QEvent::MouseButtonRelease, at, Qt::LeftButton, Qt::NoButton);
    }));
    QVERIFY(std::hypot(world("head").x() - target.x(), world("head").y() - target.y()) < 1e-3);
    const auto drawAfter = drawList();
    QCOMPARE(drawAfter.size(), drawBefore.size());
    for (size_t i = 0; i < drawAfter.size(); i++)
      QVERIFY(std::fabs(drawAfter[i].matrix.e - drawBefore[i].matrix.e) < 1e-3 && std::fabs(drawAfter[i].matrix.f - drawBefore[i].matrix.f) < 1e-3);
  }

  void canvasMoveAndBusy() {
    panel->setTool(RigCanvas::Move);
    RigCanvas* canvas = panel->canvas();
    // Grab the body image away from the root handle at its center.
    const QPointF grab = canvas->rigToWidget(world("root") + QPointF(0, 35));
    QCOMPARE(canvas->hitTest(grab), QString("slot:body"));
    const double x0 = rig()["attachments"][0].value("x", 0.0);
    host.isBusy = true;
    QVERIFY(gesture("drag while busy", 0, [&] { drag(canvas, grab, grab + QPointF(30, 0)); }));
    host.isBusy = false;
    QVERIFY(gesture("drag body part", 1, [&] { drag(canvas, grab, grab + QPointF(30, 0)); }));
    QCOMPARE(panel->selection(), QString("slot:body"));
    QVERIFY(rig()["attachments"][0].value("x", 0.0) > x0 + 5);
  }

  void propertyPanelCommitsOnEditingFinished() {
    panel->selectItem("bone:arm");
    QVERIFY(gesture("rotation spin", 1, [&] { setSpin("cutoutPropRotation", 30); }));
    QCOMPARE(rig()["bones"][2]["rotation"].get<double>(), 30.0);
    QVERIFY(gesture("focus-out without change", 0, [&] { Q_EMIT find<QDoubleSpinBox>("cutoutPropRotation")->editingFinished(); }));
  }

  void drawOrder() {
    panel->selectItem("slot:eyes");
    QVERIFY(gesture("already front", 0, [&] { click("cutoutDrawDown"); }));
    QVERIFY(gesture("move eyes back", 1, [&] { click("cutoutDrawUp"); }));
    QCOMPARE(qs(rig()["slots"][0]["id"].get<std::string>()), QString("eyes"));
    QVERIFY(gesture("move eyes front", 1, [&] { click("cutoutDrawDown"); }));
  }

  void templates() {
    click("cutoutModeAnimate");
    QCOMPARE(find<QLabel>("cutoutModeBanner")->property("mode").toString(), QString("animate"));
    panel->selectItem("bone:root");
    QVERIFY(gesture("breathing template", 1, [&] { click("cutoutTemplateBreathing"); }));
    QVERIFY(track("breathing", "bone:root", "scaleY"));
    panel->selectItem("slot:eyes");
    QVERIFY(gesture("blink template", 1, [&] { click("cutoutTemplateBlink"); }));
    QCOMPARE(host.lastEdit.value("type").toString(), QString("batch"));  // rig + controller layer
    QVERIFY(track("blink", "slot:eyes", "attachment"));
    QCOMPARE(qs(controller()["layers"][0]["clip"].get<std::string>()), QString("blink"));
  }

  void keyframingJoyClip() {
    answers = {"joy"};
    QVERIFY(gesture("new clip", 1, [&] { click("cutoutNewClip"); }));
    QCOMPARE(panel->clipId(), QString("joy"));
    QVERIFY(gesture("clip duration", 1, [&] {
      auto* s = find<QSpinBox>("cutoutClipDuration");
      s->setValue(600);
      Q_EMIT s->editingFinished();
    }));
    QVERIFY(gesture("clip loop off", 1, [&] { click("cutoutClipLoop"); }));
    QVERIFY(!rig()["clips"][3]["loop"].get<bool>());
    panel->setPlayhead(0);
    panel->selectItem("bone:head");
    QCOMPARE(panel->keyState(), QString("none"));
    QVERIFY(gesture("K on head at 0", 1, [&] { click("cutoutKeyButton"); }));
    QCOMPARE(panel->keyState(), QString("keyed"));
    panel->setPlayhead(300);
    QVERIFY(gesture("rotate head (pending)", 0, [&] { setSpin("cutoutPropRotation", 15); }));
    QVERIFY(panel->hasPending());
    QCOMPARE(panel->keyState(), QString("modified"));
    QCOMPARE(find<QPushButton>("cutoutKeyButton")->property("keyState").toString(), QString("modified"));
    QVERIFY(gesture("K commits pending", 1, [&] { click("cutoutKeyButton"); }));
    QVERIFY(!panel->hasPending());
    const Json* rot = track("joy", "bone:head", "rotation");
    QVERIFY(rot && (*rot)["keys"].size() == 2 && (*rot)["keys"][1]["timeMs"] == 300 && (*rot)["keys"][1]["value"] == 15);
    // Expression: attachment swap key through the attachment combo.
    panel->selectItem("slot:eyes");
    panel->setPlayhead(200);
    QVERIFY(gesture("eyes closed (pending)", 0, [&] {
      auto* combo = find<QComboBox>("cutoutPropAttachment");
      combo->setCurrentIndex(combo->findData("closed"));
      Q_EMIT combo->activated(combo->currentIndex());
    }));
    QVERIFY(gesture("K attachment", 1, [&] { panel->keySelected(); }));
    const Json* eyes = track("joy", "slot:eyes", "attachment");
    QVERIFY(eyes && (*eyes)["keys"][0]["timeMs"] == 200 && (*eyes)["keys"][0]["value"] == "closed");
    // Auto-key commits immediately.
    panel->setAutoKey(true);
    panel->selectItem("bone:head");
    panel->setPlayhead(600);
    QVERIFY(gesture("auto-key rotation", 1, [&] { setSpin("cutoutPropRotation", -5); }));
    panel->setAutoKey(false);
    // Canvas rotate drag in Animate mode -> pending pose -> K.
    panel->selectItem("bone:arm");
    panel->setTool(RigCanvas::Rotate);
    RigCanvas* canvas = panel->canvas();
    // Joy rotates the head at 600ms, so the arm's live origin differs from setup: use the canvas' own origin.
    const QPointF live = canvas->rigToWidget(canvas->originOf("bone:arm"));
    QVERIFY(gesture("rotate arm drag (pending)", 0, [&] {
      mouse(canvas, QEvent::MouseButtonPress, live + QPointF(60, 0), Qt::LeftButton, Qt::LeftButton);
      for (int i = 1; i <= 8; i++) { const double a = i * 3.14159265358979 / 16; mouse(canvas, QEvent::MouseMove, live + QPointF(60 * std::cos(a), 60 * std::sin(a)), Qt::NoButton, Qt::LeftButton); }
      mouse(canvas, QEvent::MouseButtonRelease, live + QPointF(0, 60), Qt::LeftButton, Qt::NoButton);
    }));
    QVERIFY(panel->hasPending());
    QVERIFY(gesture("K arm", 1, [&] { panel->keySelected(); }));
    const Json* arm = track("joy", "bone:arm", "rotation");
    QVERIFY(arm && std::fabs((*arm)["keys"][0]["value"].get<double>() - 120) < 1.0);
    panel->setTool(RigCanvas::Move);
  }

  void dopesheetEdits() {
    panel->setPlayhead(0);
    const int row = rowOf("bone:head", "rotation");
    QVERIFY(row >= 0);
    QVERIFY(gesture("move key onto another (refused)", 0, [&] { QVERIFY(!panel->moveKey(row, 300, 0)); }));
    QVERIFY(panel->status().contains("이미 키"));
    Dopesheet* sheet = panel->dopesheet();
    QVERIFY(sheet->isVisible() && sheet->width() > 300);
    const QPointF from(sheet->timeToX(300), sheet->rowCenterY(row)), to(sheet->timeToX(400), sheet->rowCenterY(row));
    QVERIFY(gesture("drag key 300 -> 400", 1, [&] { drag(sheet, from, to); }));
    const Json* rot = track("joy", "bone:head", "rotation");
    QVERIFY(rot && (*rot)["keys"][1]["timeMs"] == 400);
    QVERIFY(gesture("interp step", 1, [&] { QVERIFY(panel->setKeyInterp(row, 0, "step")); }));
    QCOMPARE(qs((*track("joy", "bone:head", "rotation"))["keys"][0]["interp"].get<std::string>()), QString("step"));
    const int xrow = rowOf("bone:head", "x");
    QVERIFY(xrow >= 0);
    sheet->selectKey(xrow, 0);
    QVERIFY(gesture("Delete key", 1, [&] { QTest::keyClick(sheet, Qt::Key_Delete); }));
    QVERIFY(!track("joy", "bone:head", "x"));
  }

  void graphEditorCurves() {
    GraphEditor* graph = panel->graph();
    QVERIFY(graph && graph->objectName() == "cutoutGraphEditor");
    panel->selectClip("joy");
    panel->selectItem("bone:head");
    pump();
    QVERIFY2(graph->isVisible() && graph->isEnabled() && graph->width() > 300 && graph->height() > 120,
             qPrintable(QString("graph %1x%2 visible %3 enabled %4").arg(graph->width()).arg(graph->height()).arg(graph->isVisible()).arg(graph->isEnabled())));
    // View switch: graph only, then both stacked again (the default).
    click("cutoutViewGraph");
    QVERIFY(graph->isVisible() && !panel->dopesheet()->isVisible());
    click("cutoutViewBoth");
    pump();
    QVERIFY(graph->isVisible() && panel->dopesheet()->isVisible() && graph->height() > 120);
    // Curves of the selected bone's numeric tracks, one color per channel, with a legend label.
    const int rot = graph->channelIndex("bone:head", "rotation");
    QVERIFY(rot >= 0);
    QSet<QString> colors;
    for (const auto& c : graph->channels()) { QCOMPARE(c.target, QString("bone:head")); QVERIFY(c.property != "attachment"); colors << c.color.name(); }
    QCOMPARE(colors.size(), int(graph->channels().size()));
    QCOMPARE(graph->channels()[rot].label, QString("회전"));
    QCOMPARE(graph->channels()[rot].keys.size(), 3);  // 0, 400, 600 from the dopesheet steps
    // Legend click hides the other channels (editor-only) so only rotation keys are hit below.
    for (int c = int(graph->channels().size()) - 1; c >= 0; c--)
      if (c != rot) QVERIFY(gesture("legend toggle", 0, [&] { mouse(graph, QEvent::MouseButtonPress, QPointF(12, 22 + 4 + c * 16 + 8), Qt::LeftButton, Qt::LeftButton); mouse(graph, QEvent::MouseButtonRelease, QPointF(12, 22 + 4 + c * 16 + 8), Qt::LeftButton, Qt::NoButton); }));
    // Clicking an empty spot only moves the playhead (sampling, no events, no edit).
    QVERIFY(gesture("graph click scrubs", 0, [&] {
      const QPointF at(graph->timeToX(250), graph->height() - 14);
      mouse(graph, QEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton);
      mouse(graph, QEvent::MouseButtonRelease, at, Qt::LeftButton, Qt::NoButton);
    }));
    QCOMPARE(panel->playhead(), 250);
    // A dopesheet key click becomes the graph selection.
    Dopesheet* sheet = panel->dopesheet();
    const int row = rowOf("bone:head", "rotation");
    QVERIFY(gesture("dopesheet key click (no move)", 0, [&] {
      const QPointF at(sheet->timeToX(600), sheet->rowCenterY(row));
      mouse(sheet, QEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton);
      mouse(sheet, QEvent::MouseButtonRelease, at, Qt::LeftButton, Qt::NoButton);
    }));
    QCOMPARE(graph->selectedKeys(), (QVector<GraphEditor::KeyRef>{{"bone:head", "rotation", 600}}));

    // Preset buttons: one key, then two keys in ONE edit.
    graph->setSelectedKeys({{"bone:head", "rotation", 400}});
    QVERIFY(gesture("preset 천천히 시작", 1, [&] { click("cutoutPresetEaseIn"); }));
    QVERIFY(keyOf("joy", "bone:head", "rotation", 400)["interp"] == "bezier" && keyOf("joy", "bone:head", "rotation", 400)["curve"] == Json::array({0.42, 0, 1, 1}));
    QCOMPARE(graph->selectedKeys().size(), 1);  // selection survives the rebuild
    QCOMPARE(graph->channels()[rot].keys[1].interp, QString("bezier"));
    graph->setSelectedKeys({{"bone:head", "rotation", 0}, {"bone:head", "rotation", 400}});
    QVERIFY(gesture("preset 부드럽게 on two keys", 1, [&] { click("cutoutPresetSmooth"); }));
    QCOMPARE(host.lastEdit.value("type").toString(), QString("replace"));
    // 400 is a peak (0 -> 15 -> -5): auto gives flat tangents there, so no overshoot.
    QVERIFY(keyOf("joy", "bone:head", "rotation", 0)["curve"] == Json::array({0.333333, 0, 0.666667, 1}));
    QVERIFY(keyOf("joy", "bone:head", "rotation", 400)["curve"] == Json::array({0.333333, 0, 0.666667, 1}));
    // Dopesheet: the interp gesture accepts presets, segments reflect the interp, menu has presets.
    QVERIFY(gesture("dopesheet preset easeOut", 1, [&] { QVERIFY(panel->setKeyInterp(row, 0, "easeOut")); }));
    QVERIFY(keyOf("joy", "bone:head", "rotation", 0)["curve"] == Json::array({0, 0, 0.58, 1}));
    QCOMPARE(sheet->rows()[row].interps.value(0), QString("bezier"));
    QVERIFY(gesture("dopesheet right-click 튕김", 1, [&] { contextPick(sheet, QPoint(sheet->timeToX(400), sheet->rowCenterY(row)), "cutoutInterpBack"); }));
    QVERIFY(keyOf("joy", "bone:head", "rotation", 400)["curve"] == Json::array({0.34, 1.56, 0.64, 1}));

    // Handle drag on the selected key: exactly one setCurve edit with the dropped numbers.
    graph->setSelectedKeys({{"bone:head", "rotation", 400}});
    pump();
    const auto handle = graph->handlePoint(rot, 1, GraphEditor::Side::Out);
    QVERIFY(handle);
    const QPointF drop = *handle + QPointF(24, 16);
    const cutout::Curve expected = graph->curveForHandle(rot, 1, GraphEditor::Side::Out, drop);
    QVERIFY(expected != (cutout::Curve{0.34, 1.56, 0.64, 1}) && expected[2] == 0.64 && expected[3] == 1);
    QVERIFY(gesture("drag out handle", 1, [&] { drag(graph, *handle, drop); }));
    const Json dragged = keyOf("joy", "bone:head", "rotation", 400)["curve"];
    for (size_t i = 0; i < 4; i++) QVERIFY2(std::fabs(dragged[i].get<double>() - expected[i]) < 1e-9, dragged.dump().c_str());
    // Undo/redo round trip; the graph is rebuilt from the view each time.
    const Json afterDrag = rig();
    host.request("edit", {{"edit", QJsonObject{{"type", "undo"}}}}, {});
    pump();
    QVERIFY(keyOf("joy", "bone:head", "rotation", 400)["curve"] == Json::array({0.34, 1.56, 0.64, 1}));
    QVERIFY(graph->channels()[rot].keys[1].curve == (cutout::Curve{0.34, 1.56, 0.64, 1}));
    host.request("edit", {{"edit", QJsonObject{{"type", "redo"}}}}, {});
    pump();
    QVERIFY(rig() == afterDrag);
    QVERIFY(graph->channels()[rot].keys[1].curve == expected);

    // Key drags: vertical = value only; horizontal onto another key is refused; elsewhere moves with its curve.
    graph->setSelectedKeys({});
    const QPointF k600 = graph->keyPoint(rot, 2);
    const double newValue = std::round(graph->yToValue(rot, k600.y() - 30) * 1e4) / 1e4;
    QVERIFY(gesture("drag key value", 1, [&] { drag(graph, k600, k600 + QPointF(0, -30)); }));
    QVERIFY(std::fabs(keyOf("joy", "bone:head", "rotation", 600)["value"].get<double>() - newValue) < 1e-9 && newValue > -5 + 1);
    const QPointF k400 = graph->keyPoint(rot, 1);
    QVERIFY(gesture("drag key onto 600 (refused)", 0, [&] { drag(graph, k400, QPointF(graph->timeToX(600), k400.y())); }));
    QVERIFY(panel->status().contains("이미 키"));
    QCOMPARE(graph->channels()[rot].keys[1].timeMs, 400);  // preview restored
    QVERIFY(gesture("drag key time 400 -> 450", 1, [&] { drag(graph, k400, QPointF(graph->timeToX(450), k400.y())); }));
    QVERIFY(keyOf("joy", "bone:head", "rotation", 400).is_null());
    QVERIFY(keyOf("joy", "bone:head", "rotation", 450)["curve"] == dragged);  // handles are normalized: they move with the key
    QVERIFY(graph->selectedKeys().contains(GraphEditor::KeyRef{"bone:head", "rotation", 450}));
    // Ctrl+A + preset buttons: linear/step drop curves, ease presets write them, one edit each.
    QVERIFY(gesture("select all + 선형", 1, [&] { QTest::keyClick(graph, Qt::Key_A, Qt::ControlModifier); click("cutoutPresetLinear"); }));
    for (int ms : {0, 450, 600}) QVERIFY(!keyOf("joy", "bone:head", "rotation", ms).contains("interp") && !keyOf("joy", "bone:head", "rotation", ms).contains("curve"));
    QVERIFY(gesture("select all + 천천히 시작·끝", 1, [&] { QTest::keyClick(graph, Qt::Key_A, Qt::ControlModifier); click("cutoutPresetEaseInOut"); }));
    for (int ms : {0, 450}) QVERIFY(keyOf("joy", "bone:head", "rotation", ms)["curve"] == Json::array({0.42, 0, 0.58, 1}));
    // Runtime plays what the graph draws (core evaluation of the committed curve).
    const auto parsed = cutout::parseRig("hero", rig());
    const double v0 = keyOf("joy", "bone:head", "rotation", 0)["value"].get<double>(), v1 = keyOf("joy", "bone:head", "rotation", 450)["value"].get<double>();
    QVERIFY(std::fabs(cutout::samplePose(parsed, parsed.clipIndex("joy"), 225).bones[size_t(parsed.boneIndex("head"))].rotation - (v0 + (v1 - v0) * 0.5)) < 1e-9);
    // A part with only attachment keys has no curves (message instead), and presets refuse.
    panel->selectItem("slot:eyes");
    QVERIFY(graph->channels().isEmpty());
    graph->setSelectedKeys({});
    sheet->selectKey(-1, -1);
    QVERIFY(gesture("preset without keys (refused)", 0, [&] { click("cutoutPresetBack"); }));
    panel->selectItem("bone:head");
  }

  void dopesheetZoomSelectionAndClipboard() {
    Dopesheet* sheet = panel->dopesheet();
    GraphEditor* graph = panel->graph();
    TimeView* view = panel->timeView();
    QVERIFY(sheet->timeView() == view && graph->timeView() == view);
    auto* rowsScroll = find<QScrollArea>("cutoutDopesheetScroll")->verticalScrollBar();
    rowsScroll->setValue(0);
    const KeyRef r1580{"bone:head", "rotation", 1580}, r1600{"bone:head", "rotation", 1600}, x1580{"bone:head", "x", 1580};
    // A long clip with two keys 20 ms apart: the case that used to draw both on one pixel.
    answers = {"timing"};
    QVERIFY(gesture("new clip timing", 1, [&] { click("cutoutNewClip"); }));
    QVERIFY(gesture("timing 8000 ms", 1, [&] { auto* s = find<QSpinBox>("cutoutClipDuration"); s->setValue(8000); Q_EMIT s->editingFinished(); }));
    panel->selectItem("bone:head");
    panel->setAutoKey(true);
    for (const auto& [t, v] : {std::pair{0, 10.0}, std::pair{1580, 20.0}, std::pair{1600, 30.0}, std::pair{6000, 5.0}}) {
      panel->setPlayhead(t);
      QVERIFY(gesture("auto-key rotation", 1, [&] { setSpin("cutoutPropRotation", v); }));
    }
    for (int t : {1580, 6000}) { panel->setPlayhead(t); QVERIFY(gesture("auto-key x", 1, [&] { setSpin("cutoutPropX", 3 + t / 1000.0); })); }
    panel->setAutoKey(false);
    const int rot = rowOf("bone:head", "rotation"), xr = rowOf("bone:head", "x");
    QVERIFY(rot >= 0 && xr >= 0);
    QCOMPARE(timesOf("timing", "bone:head", "rotation"), (QVector<int>{0, 1580, 1600, 6000}));

    // Fitted to 8000 ms the two keys are ~2 px apart: one cluster marker, clicks cycle through it.
    click("cutoutTimeFit");
    QVERIFY(view->fitted() && view->pxPerMs() * 20 < Dopesheet::CLUSTER_PX);
    bool clustered = false;
    for (const auto& g : sheet->clusters(rot)) clustered |= g == QVector<int>{1, 2};
    QVERIFY(clustered);
    const QPointF cluster(sheet->timeToX(1590), sheet->rowCenterY(rot));
    QVERIFY(gesture("cluster click selects the first key", 0, [&] { tap(sheet, cluster); }));
    QCOMPARE(view->selection(), QVector<KeyRef>{r1580});
    QVERIFY(gesture("second click cycles to the next key", 0, [&] { tap(sheet, cluster); }));
    QCOMPARE(view->selection(), QVector<KeyRef>{r1600});
    QCOMPARE(graph->selectedKeys(), QVector<KeyRef>{r1600});  // one selection for both views
    // Ctrl+wheel zooms around the cursor until the keys separate; the time under it stays put.
    const double underCursor = view->xToTime(cluster.x());
    QVERIFY(gesture("ctrl+wheel zoom", 0, [&] { wheel(sheet, cluster, 12, Qt::ControlModifier); }));
    QVERIFY(!view->fitted() && view->pxPerMs() * 20 >= 20);
    QVERIFY(std::fabs(view->xToTime(cluster.x()) - underCursor) < 1e-6);
    for (const auto& g : sheet->clusters(rot)) QCOMPARE(g.size(), 1);
    QVERIFY(gesture("click 1580 alone", 0, [&] { tap(sheet, QPointF(sheet->timeToX(1580), sheet->rowCenterY(rot))); }));
    QCOMPARE(view->selection(), QVector<KeyRef>{r1580});
    QVERIFY(gesture("click 1600 alone", 0, [&] { tap(sheet, QPointF(sheet->timeToX(1600), sheet->rowCenterY(rot))); }));
    QCOMPARE(view->selection(), QVector<KeyRef>{r1600});
    // Pan: Shift+wheel and the horizontal scrollbar drive the same view (graph follows).
    const double scroll0 = view->scrollMs();
    QVERIFY(gesture("shift+wheel pans", 0, [&] { wheel(sheet, cluster, -2, Qt::ShiftModifier); }));
    QVERIFY(view->scrollMs() > scroll0);
    auto* bar = find<QScrollBar>("cutoutTimeScroll");
    QVERIFY(bar && bar->maximum() > 0 && bar->value() == int(std::lround(view->scrollMs() * 10)));
    view->zoomAround(1600, 1.0 / view->pxPerMs());  // 1 px = 1 ms for exact drags below
    bar->setValue(13000);
    QVERIFY(std::fabs(view->scrollMs() - 1300) < 0.2 && std::fabs(view->pxPerMs() - 1) < 1e-9);
    QCOMPARE(graph->timeToX(1700), sheet->timeToX(1700));  // both views share the mapping

    // Precise playhead: numeric field, steps, ruler snapping (10 ms, Shift 1 ms), arrow keys.
    auto* field = find<QSpinBox>("cutoutTimeField");
    QVERIFY(gesture("time field", 0, [&] { field->setValue(1587); Q_EMIT field->editingFinished(); }));
    QCOMPARE(panel->playhead(), 1587);
    QVERIFY(gesture("step forward", 0, [&] { click("cutoutStepForward"); }));
    QCOMPARE(panel->playhead(), 1597);
    find<QSpinBox>("cutoutStepSize")->setValue(1);
    QVERIFY(gesture("step back 1 ms", 0, [&] { click("cutoutStepBack"); }));
    QCOMPARE(panel->playhead(), 1596);
    QCOMPARE(field->value(), 1596);
    find<QSpinBox>("cutoutStepSize")->setValue(10);
    QVERIFY(gesture("ruler click snaps to 10 ms", 0, [&] { tap(sheet, QPointF(sheet->timeToX(1643), 10)); }));
    QCOMPARE(panel->playhead(), 1640);
    QVERIFY(gesture("shift ruler click = 1 ms", 0, [&] { tap(sheet, QPointF(sheet->timeToX(1643), 10), Qt::ShiftModifier); }));
    QCOMPARE(panel->playhead(), 1643);
    QTest::keyClick(sheet, Qt::Key_Right);
    QCOMPARE(panel->playhead(), 1653);

    // Selection: row header, Alt+click column, Ctrl+A, rubber band.
    QVERIFY(gesture("row header selects the row", 0, [&] { tap(sheet, QPointF(40, sheet->rowCenterY(rot))); }));
    QCOMPARE(view->selection().size(), 4);
    QVERIFY(gesture("alt+click selects the column", 0, [&] { tap(sheet, QPointF(sheet->timeToX(1580), sheet->rowCenterY(rot)), Qt::AltModifier); }));
    QCOMPARE(asSet(view->selection()), (QSet<KeyRef>{r1580, x1580}));
    QTest::keyClick(sheet, Qt::Key_A, Qt::ControlModifier);
    QCOMPARE(view->selection().size(), 6);
    QVERIFY(gesture("ctrl+click toggles", 0, [&] { tap(sheet, QPointF(sheet->timeToX(1600), sheet->rowCenterY(rot)), Qt::ControlModifier); }));
    QVERIFY(view->selection().size() == 5 && !view->isSelected(r1600));
    const int top = std::min(rot, xr), bottom = std::max(rot, xr);
    QVERIFY(gesture("box select", 0, [&] {
      drag(sheet, QPointF(sheet->timeToX(1500), sheet->rowCenterY(top) - 8), QPointF(sheet->timeToX(1700), sheet->rowCenterY(bottom) + 8));
    }));
    QCOMPARE(asSet(view->selection()), (QSet<KeyRef>{r1580, r1600, x1580}));
    QCOMPARE(asSet(graph->selectedKeys()), asSet(view->selection()));

    // Dragging one selected key moves the whole selection: ONE edit, spacing kept.
    QVERIFY(gesture("drag selection +100 ms = one edit", 1, [&] {
      drag(sheet, QPointF(sheet->timeToX(1600), sheet->rowCenterY(rot)), QPointF(sheet->timeToX(1700), sheet->rowCenterY(rot)));
    }));
    QCOMPARE(timesOf("timing", "bone:head", "rotation"), (QVector<int>{0, 1680, 1700, 6000}));
    QCOMPARE(timesOf("timing", "bone:head", "x"), (QVector<int>{1680, 6000}));
    const KeyRef r1680{"bone:head", "rotation", 1680}, r1700{"bone:head", "rotation", 1700}, x1680{"bone:head", "x", 1680};
    QCOMPARE(asSet(view->selection()), (QSet<KeyRef>{r1680, r1700, x1680}));  // selection follows the keys
    undo();
    QCOMPARE(timesOf("timing", "bone:head", "rotation"), (QVector<int>{0, 1580, 1600, 6000}));
    redo();
    QCOMPARE(timesOf("timing", "bone:head", "rotation"), (QVector<int>{0, 1680, 1700, 6000}));
    // Offset field: collisions and the clip end refuse the whole move; otherwise one edit.
    view->setSelection({r1680, r1700, x1680});
    find<QSpinBox>("cutoutKeyOffset")->setValue(4300);
    QVERIFY(gesture("offset onto an existing key (refused)", 0, [&] { click("cutoutKeyOffsetApply"); }));
    QVERIFY(panel->status().contains("이미 키"));
    find<QSpinBox>("cutoutKeyOffset")->setValue(7000);
    QVERIFY(gesture("offset past the end (refused)", 0, [&] { click("cutoutKeyOffsetApply"); }));
    QVERIFY(panel->status().contains("범위"));
    QCOMPARE(timesOf("timing", "bone:head", "rotation"), (QVector<int>{0, 1680, 1700, 6000}));
    find<QSpinBox>("cutoutKeyOffset")->setValue(20);
    QVERIFY(gesture("offset +20 ms", 1, [&] { click("cutoutKeyOffsetApply"); }));
    QCOMPARE(timesOf("timing", "bone:head", "rotation"), (QVector<int>{0, 1700, 1720, 6000}));
    const KeyRef r1720{"bone:head", "rotation", 1720}, x1700{"bone:head", "x", 1700};

    // Timing scale: eased keys keep their curves (normalized handles) when the spacing changes.
    QVERIFY(gesture("ease the selection", 1, [&] { click("cutoutPresetEaseInOut"); }));
    const auto handle = sheet->scaleHandle(true);
    QVERIFY(handle && std::fabs(handle->x() - sheet->timeToX(1720)) <= 1);
    QVERIFY(gesture("scale handle x2 = one edit", 1, [&] { drag(sheet, *handle, QPointF(sheet->timeToX(1740), handle->y())); }));
    QCOMPARE(timesOf("timing", "bone:head", "rotation"), (QVector<int>{0, 1700, 1740, 6000}));
    QCOMPARE(timesOf("timing", "bone:head", "x"), (QVector<int>{1700, 6000}));
    QVERIFY(keyOf("timing", "bone:head", "rotation", 1700)["curve"] == Json::array({0.42, 0, 0.58, 1}));
    QVERIFY(view->isSelected({"bone:head", "rotation", 1740}));
    find<QSpinBox>("cutoutKeyScale")->setValue(50);
    QVERIFY(gesture("scale field 50%", 1, [&] { click("cutoutKeyScaleApply"); }));
    QCOMPARE(timesOf("timing", "bone:head", "rotation"), (QVector<int>{0, 1700, 1720, 6000}));
    undo();
    QCOMPARE(timesOf("timing", "bone:head", "rotation"), (QVector<int>{0, 1700, 1740, 6000}));
    redo();
    QCOMPARE(timesOf("timing", "bone:head", "rotation"), (QVector<int>{0, 1700, 1720, 6000}));
    // Delete removes the whole selection in one edit.
    view->setSelection({r1700, r1720, x1700});
    QVERIFY(gesture("Delete selection", 1, [&] { QTest::keyClick(sheet, Qt::Key_Delete); }));
    QCOMPARE(timesOf("timing", "bone:head", "rotation"), (QVector<int>{0, 6000}));
    QCOMPARE(timesOf("timing", "bone:head", "x"), (QVector<int>{6000}));
    undo();
    QCOMPARE(timesOf("timing", "bone:head", "rotation"), (QVector<int>{0, 1700, 1720, 6000}));

    // Copy in one clip, paste at the playhead of another (same target), then onto another bone.
    view->setSelection({r1700, r1720});
    QVERIFY(gesture("Ctrl+C (no edit)", 0, [&] { QTest::keyClick(sheet, Qt::Key_C, Qt::ControlModifier); }));
    QCOMPARE(panel->clipboardKeys(), 2);
    const Json v1700 = keyOf("timing", "bone:head", "rotation", 1700)["value"], v1720 = keyOf("timing", "bone:head", "rotation", 1720)["value"];
    panel->selectClip("joy");
    QVERIFY(view->selection().isEmpty() && view->fitted());  // a clip switch refits and clears the selection
    panel->setPlayhead(100);
    QVERIFY(gesture("Ctrl+V into joy = one edit", 1, [&] { QTest::keyClick(sheet, Qt::Key_V, Qt::ControlModifier); }));
    QVERIFY(keyOf("joy", "bone:head", "rotation", 100)["value"] == v1700 && keyOf("joy", "bone:head", "rotation", 120)["value"] == v1720);
    QVERIFY(keyOf("joy", "bone:head", "rotation", 100)["curve"] == Json::array({0.42, 0, 0.58, 1}));
    QCOMPARE(asSet(view->selection()), (QSet<KeyRef>{{"bone:head", "rotation", 100}, {"bone:head", "rotation", 120}}));
    undo();
    QVERIFY(keyOf("joy", "bone:head", "rotation", 100).is_null());
    redo();
    QVERIFY(!keyOf("joy", "bone:head", "rotation", 120).is_null());
    panel->selectItem("bone:arm");
    panel->setPlayhead(300);
    QVERIFY(gesture("paste a row onto the selected bone", 1, [&] { click("cutoutPasteKeys"); }));
    QVERIFY(panel->status().contains("arm"));
    QVERIFY(keyOf("joy", "bone:arm", "rotation", 300)["value"] == v1700 && !keyOf("joy", "bone:arm", "rotation", 320).is_null());
    panel->setPlayhead(590);
    QVERIFY(gesture("paste past the clip end (refused)", 0, [&] { click("cutoutPasteKeys"); }));
    QVERIFY(panel->status().contains("범위"));
    // Ctrl+D duplicates the selection (the pasted arm keys) at the playhead; clipboard untouched.
    panel->setPlayhead(400);
    QVERIFY(gesture("Ctrl+D duplicates at the playhead", 1, [&] { QTest::keyClick(sheet, Qt::Key_D, Qt::ControlModifier); }));
    QVERIFY(!keyOf("joy", "bone:arm", "rotation", 400).is_null() && !keyOf("joy", "bone:arm", "rotation", 420).is_null());
    QCOMPARE(panel->clipboardKeys(), 2);

    // Pose copy/paste: head + its child arm at joy 600 ms become keys at timing 4000 ms. The copy
    // takes exactly the selection (plugin 1.7.0): one bone alone, then 자식 본까지 (hierarchy Ctrl+Shift+A).
    panel->selectItem("bone:head");
    panel->setPlayhead(600);
    QVERIFY(gesture("copy pose of one bone (no edit)", 0, [&] { click("cutoutCopyPose"); }));
    QVERIFY(panel->status().contains("본 1개"));
    QTest::keyClick(find<QTreeWidget>("cutoutBoneTree"), Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier);  // hierarchy: 자식 본까지
    QCOMPARE(panel->selectedBones(), QStringList({"head", "arm"}));
    QVERIFY(gesture("copy pose (no edit)", 0, [&] { click("cutoutCopyPose"); }));
    QVERIFY(panel->status().contains("본 2개"));
    const auto parsed = cutout::parseRig("hero", rig());
    const auto pose = cutout::samplePose(parsed, parsed.clipIndex("joy"), 600);
    panel->selectClip("timing");
    panel->setPlayhead(4000);
    QVERIFY(gesture("paste pose = one edit", 1, [&] { click("cutoutPastePose"); }));
    QVERIFY(std::fabs(keyOf("timing", "bone:head", "rotation", 4000)["value"].get<double>() - pose.bones[size_t(parsed.boneIndex("head"))].rotation) < 1e-6);
    QVERIFY(std::fabs(keyOf("timing", "bone:arm", "rotation", 4000)["value"].get<double>() - pose.bones[size_t(parsed.boneIndex("arm"))].rotation) < 1e-6);
    undo();
    QVERIFY(keyOf("timing", "bone:arm", "rotation", 4000).is_null());
    redo();
    QVERIFY(!keyOf("timing", "bone:arm", "rotation", 4000).is_null());

    // Graph: dragging one of two selected keys sideways moves both (one edit).
    view->zoomAround(1700, 1.0 / view->pxPerMs());
    view->setScroll(1300);
    view->setSelection({r1700, r1720});
    const int ch = graph->channelIndex("bone:head", "rotation");
    QVERIFY(ch >= 0);
    int k1700 = -1;
    for (int k = 0; k < graph->channels()[ch].keys.size(); k++) if (graph->channels()[ch].keys[k].timeMs == 1700) k1700 = k;
    const QPointF kp = graph->keyPoint(ch, k1700);
    QVERIFY(gesture("graph group drag = one edit", 1, [&] { drag(graph, kp, kp + QPointF(50 * view->pxPerMs(), 0)); }));
    QCOMPARE(timesOf("timing", "bone:head", "rotation"), (QVector<int>{0, 1750, 1770, 4000, 6000}));

    // Preview loop range: editor-only (no edit), markers drag on the ruler, playback loops inside, per clip.
    click("cutoutTimeFit");
    QVERIFY(gesture("loop range buttons", 0, [&] { panel->setPlayhead(1000); click("cutoutLoopIn"); panel->setPlayhead(2000); click("cutoutLoopOut"); }));
    QVERIFY(view->loopActive() && view->loopIn() == 1000 && view->loopOut() == 2000);
    QVERIFY(gesture("drag the out marker", 0, [&] { drag(sheet, QPointF(view->timeToX(2000), 18), QPointF(view->timeToX(2500), 18)); }));
    QCOMPARE(view->loopOut(), 2500);
    panel->setPlayhead(3000);
    click("cutoutPlay");
    QVERIFY(panel->playhead() >= 1000 && panel->playhead() < 1200);  // starts at the range start
    panel->setPlayhead(2450);
    QTest::qWait(150);
    QVERIFY2(panel->playhead() >= 1000 && panel->playhead() < 2400, qPrintable(QString::number(panel->playhead())));  // wrapped inside the range
    click("cutoutPlay");
    panel->selectClip("joy");
    QVERIFY(!view->loopActive());
    panel->selectClip("timing");
    QVERIFY(view->loopActive() && view->loopIn() == 1000 && view->loopOut() == 2500);
    click("cutoutLoopClear");
    QVERIFY(!view->loopActive());
    // The inactive out marker sits at the clip end: a click on the ruler labels there still scrubs.
    QVERIFY(gesture("ruler click at the clip end", 0, [&] { tap(sheet, QPointF(view->timeToX(8000), 8)); }));
    QCOMPARE(panel->playhead(), 8000);
    QVERIFY(rig().dump().find("loopIn") == std::string::npos);  // editor-only: never stored

    panel->selectClip("joy");
    pump();
    // The ruler stays at the top of the visible rows: scrolled down, a click there still scrubs.
    if (rowsScroll->maximum() > 0) {
      rowsScroll->setValue(rowsScroll->maximum());
      pump();
      QVERIFY(sheet->y() < 0);
      QVERIFY(gesture("sticky ruler click", 0, [&] { tap(sheet, QPointF(sheet->timeToX(250), -sheet->y() + 10)); }));
      QCOMPARE(panel->playhead(), 250);
      rowsScroll->setValue(0);
    }
    panel->setPlayhead(0);
  }

  void onionSkinAndMotionTrail() {
    panel->setMode(CutoutPanel::Mode::Animate);
    find<QTabWidget>("cutoutBottomTabs")->setCurrentIndex(0);
    answers = {"onion"};
    QVERIFY(gesture("onion clip", 1, [&] { click("cutoutNewClip"); }));
    QCOMPARE(panel->clipId(), QString("onion"));
    double x0 = 0, y0 = 0;
    for (const auto& b : rig()["bones"]) if (b["id"] == "root") { x0 = b.value("x", 0.0); y0 = b.value("y", 0.0); }
    // root x: x0-150 at 0 -> x0+150 at 1000 (looping, 1000 ms); root y key at 300; head rotation key at 700.
    panel->setAutoKey(true);
    panel->selectItem("bone:root");
    panel->setPlayhead(0);
    QVERIFY(gesture("root x at 0", 1, [&] { setSpin("cutoutPropX", x0 - 150); }));
    panel->setPlayhead(1000);
    QVERIFY(gesture("root x at 1000", 1, [&] { setSpin("cutoutPropX", x0 + 150); }));
    panel->setPlayhead(300);
    QVERIFY(gesture("root y at 300", 1, [&] { setSpin("cutoutPropY", y0 + 10); }));
    panel->selectItem("bone:head");
    panel->setPlayhead(700);
    QVERIFY(gesture("head rotation at 700", 1, [&] { setSpin("cutoutPropRotation", 4); }));
    panel->setAutoKey(false);
    panel->selectItem("bone:root");

    // Toggle: button and O, never an edit.
    QVERIFY(!panel->onionSettings().enabled && panel->ghosts().isEmpty());
    QVERIFY(gesture("onion button", 0, [&] { click("cutoutOnionSkin"); }));
    QVERIFY(panel->onionSettings().enabled && find<QToolButton>("cutoutOnionSkin")->isChecked());
    QVERIFY(!panel->ghosts().isEmpty());
    QVERIFY(gesture("O toggles off", 0, [&] { QTest::keyClick(panel->canvas(), Qt::Key_O); }));
    QVERIFY(!panel->onionSettings().enabled && !find<QToolButton>("cutoutOnionSkin")->isChecked() && panel->ghosts().isEmpty());
    QVERIFY(panel->canvas()->ghosts().isEmpty());
    QVERIFY(gesture("O toggles on", 0, [&] { QTest::keyClick(panel->canvas(), Qt::Key_O); }));
    QVERIFY(panel->onionSettings().enabled);
    QVERIFY(find<QFrame>("cutoutOnionSettings"));

    using G = QVector<QPair<double, int>>;
    auto times = [&] { G out; for (const auto& g : panel->ghosts()) out << qMakePair(g.timeMs, g.offset); return out; };
    // Step mode through the settings popup controls.
    QVERIFY(gesture("onion settings", 0, [&] {
      find<QSpinBox>("cutoutOnionBefore")->setValue(2);
      find<QSpinBox>("cutoutOnionAfter")->setValue(1);
      find<QSpinBox>("cutoutOnionStep")->setValue(250);
      find<QSpinBox>("cutoutOnionOpacity")->setValue(60);
      find<QSpinBox>("cutoutOnionFalloff")->setValue(100);
    }));
    QCOMPARE(QSettings().value("plugins/cutout-animation/onionSkin/stepMs").toInt(), 250);  // per-user preference
    panel->setPlayhead(500);
    QCOMPARE(times(), (G{{0, -2}, {250, -1}, {750, 1}}));
    QCOMPARE(panel->ghosts()[0].alpha, 0.3);  // falloff 100%: the farther of two fades to half
    QCOMPARE(panel->ghosts()[1].alpha, 0.6);
    QCOMPARE(panel->ghosts()[0].tint, QColor("#ef4444"));
    QCOMPARE(panel->ghosts()[2].tint, QColor("#22c55e"));
    panel->setPlayhead(100);  // looping clip: before-ghosts wrap around the end
    QCOMPARE(times(), (G{{600, -2}, {850, -1}, {350, 1}}));

    // Keys-only: neighbouring keys of the selected target, wrapping around the loop.
    QVERIFY(gesture("keys-only mode", 0, [&] {
      auto* mode = find<QComboBox>("cutoutOnionMode");
      mode->setCurrentIndex(mode->findData(true));
      find<QSpinBox>("cutoutOnionAfter")->setValue(2);
    }));
    QVERIFY(panel->onionSettings().keysOnly && !find<QSpinBox>("cutoutOnionStep")->isEnabled());
    panel->setPlayhead(500);
    QCOMPARE(times(), (G{{0, -2}, {300, -1}, {1000, 1}}));  // 0 is as near either way: stays before
    panel->setPlayhead(100);
    QCOMPARE(times(), (G{{1000, -2}, {0, -1}, {300, 1}}));  // 1000 is 100 ms back around the loop
    panel->selectItem("bone:head");
    panel->setPlayhead(500);
    QCOMPARE(times(), (G{{700, 1}}));  // only the head's key
    panel->selectItem("slot:body");
    QCOMPARE(times(), (G{{0, -2}, {300, -1}, {700, 1}, {1000, 2}}));  // nothing keyed there: all visible tracks
    QVERIFY(gesture("subtree filter", 0, [&] { find<QCheckBox>("cutoutOnionSubtree")->setChecked(true); }));
    panel->selectItem("bone:head");
    QVERIFY(panel->ghostHiddenSlots().contains("body") && !panel->ghostHiddenSlots().contains("eyes"));
    QCOMPARE(times(), (G{{700, 1}}));
    panel->selectItem("bone:root");  // root subtree = every bone
    QVERIFY(panel->ghostHiddenSlots().isEmpty());
    QCOMPARE(times(), (G{{0, -2}, {300, -1}, {700, 1}, {1000, 2}}));
    // One-shot: no wrap, step ghosts clamp to the clip (one ghost at the boundary).
    QVERIFY(gesture("clip loop off", 1, [&] { click("cutoutClipLoop"); }));
    panel->setPlayhead(100);
    QCOMPARE(times(), (G{{0, -1}, {300, 1}, {700, 2}}));
    QVERIFY(gesture("step mode", 0, [&] { auto* mode = find<QComboBox>("cutoutOnionMode"); mode->setCurrentIndex(mode->findData(false)); }));
    QCOMPARE(times(), (G{{0, -1}, {350, 1}, {600, 2}}));
    QVERIFY(gesture("clip loop on", 1, [&] { click("cutoutClipLoop"); }));

    // Tint: pixel checks on an offscreen render of the canvas.
    OnionSettings s = panel->onionSettings();
    s.before = 1; s.after = 1; s.stepMs = 250; s.opacity = 0.9; s.falloff = 0; s.subtreeOnly = false; s.includeCurrent = false;
    s.beforeColor = QColor("#3b82f6");
    s.afterColor = QColor("#22c55e");
    QVERIFY(gesture("tint settings", 0, [&] { panel->setOnionSettings(s); }));
    panel->setPlayhead(500);
    auto parsed = std::make_shared<const cutout::Rig>(cutout::parseRig("hero", rig()));
    const int clip = parsed->clipIndex("onion"), body = parsed->slotIndex("body"), root = parsed->boneIndex("root");
    auto bodyPoint = [&](double t) {
      const cutout::Pose pose = cutout::samplePose(*parsed, clip, t);
      const cutout::Attachment* att = nullptr;
      const auto m = attachmentMatrix(*parsed, pose, body, &att);
      return panel->canvas()->rigToWidget(qtTransform(m).map(QPointF(att->width / 2, att->height * 0.85))).toPoint();
    };
    auto pixel = [&](double t) { pump(); return QColor(panel->canvas()->grab().toImage().pixel(bodyPoint(t))); };
    const QColor before = pixel(250), after = pixel(750), live = pixel(500);
    qInfo("ghost pixels: before %s after %s live %s", qPrintable(before.name()), qPrintable(after.name()), qPrintable(live.name()));
    QVERIFY(before.blue() > before.red() + 40);
    QVERIFY(after.green() > after.red() + 40 && after.green() > after.blue() + 20);
    QVERIFY(live.red() > live.blue() + 40);  // the live pose is never tinted
    s.beforeColor = QColor();  // 원래 색
    panel->setOnionSettings(s);
    QCOMPARE(find<QComboBox>("cutoutOnionBeforeColor")->currentData().toString(), QString());
    const QColor plain = pixel(250);
    QVERIFY(plain.red() > plain.blue() + 40);
    s.beforeColor = QColor("#3b82f6");
    s.subtreeOnly = true;
    panel->setOnionSettings(s);
    panel->selectItem("bone:head");  // the body part hangs on root: not in the head's subtree
    const QColor filtered = pixel(250);
    QVERIFY(filtered.blue() < filtered.red() + 20);
    panel->selectItem("bone:root");
    QVERIFY(pixel(250).blue() > pixel(250).red() + 40);
    panel->setOnionSkin(false);
    const QColor off = pixel(250);
    qInfo("onion off pixel %s, filtered %s", qPrintable(off.name()), qPrintable(filtered.name()));
    QVERIFY(off == filtered);  // nothing there but the backdrop
    panel->setOnionSkin(true);

    // Cache: redraws and revisiting a time never resample; an edit (and its undo) does.
    panel->setPlayhead(500);
    pump();
    panel->canvas()->grab();
    const int samples = panel->ghostSamples(), renders = panel->canvas()->ghostRenders();
    panel->setPlayhead(500);
    panel->canvas()->grab();
    QCOMPARE(panel->ghostSamples(), samples);
    QCOMPARE(panel->canvas()->ghostRenders(), renders);
    panel->setPlayhead(510);
    panel->setPlayhead(500);
    QCOMPARE(panel->ghostSamples(), samples + 2);  // only 260 and 760 were new
    auto ghostX = [&](int offset) {
      for (const auto& g : panel->canvas()->ghosts()) if (g.offset == offset) return g.pose->world[size_t(root)].e;
      return qQNaN();
    };
    const double oldAfter = ghostX(1);
    QVERIFY(std::abs(oldAfter - cutout::samplePose(*parsed, clip, 750).world[size_t(root)].e) < 1e-9);
    panel->setAutoKey(true);
    panel->setPlayhead(1000);
    QVERIFY(gesture("re-key root x at 1000", 1, [&] { setSpin("cutoutPropX", x0 + 350); }));
    panel->setAutoKey(false);
    panel->setPlayhead(500);
    const cutout::Rig edited = cutout::parseRig("hero", rig());
    QVERIFY(std::abs(ghostX(1) - cutout::samplePose(edited, clip, 750).world[size_t(root)].e) < 1e-9);
    QVERIFY(std::abs(ghostX(1) - oldAfter) > 1);
    undo();
    panel->setPlayhead(500);
    QVERIFY(std::abs(ghostX(1) - oldAfter) < 1e-9);

    // Include current: the saved pose at the playhead next to an uncommitted change.
    s = panel->onionSettings();
    s.includeCurrent = true;
    s.subtreeOnly = false;
    panel->setOnionSettings(s);
    QVERIFY(gesture("pending x (no edit)", 0, [&] { setSpin("cutoutPropX", x0 + 40); }));
    QVERIFY(panel->hasPending());
    const auto& infos = panel->ghosts();
    QVERIFY(std::any_of(infos.begin(), infos.end(), [](const CutoutPanel::GhostInfo& g) { return g.committed && g.offset == 0 && g.timeMs == 500; }));
    QVERIFY(std::abs(ghostX(0) - cutout::samplePose(*parsed, clip, 500).world[size_t(root)].e) < 1e-9);
    QVERIFY(std::abs(ghostX(0) - (x0 + 40)) > 1);
    QVERIFY(gesture("discard pending", 0, [&] { panel->discardPending(); }));

    // Motion trail: the selected bone's world origin over the clip, dots at its (and its parents') keys.
    QVERIFY(gesture("trail toggle", 0, [&] { click("cutoutMotionTrail"); }));
    panel->selectItem("bone:head");
    cutout::Rig scrub = cutout::parseRig("hero", rig());
    for (auto& c : scrub.clips) c.loop = false;  // editor scrubbing samples the clip end itself
    const int head = scrub.boneIndex("head");
    const auto& tt = panel->trailTimes();
    QVERIFY(tt.size() > 100 && tt.front() == 0 && tt.back() == 1000);
    QCOMPARE(panel->trailPoints().size(), tt.size());
    for (int i = 0; i < tt.size(); i++) {
      const auto w = cutout::samplePose(scrub, clip, tt[i]).world[size_t(head)];
      QVERIFY(std::hypot(panel->trailPoints()[i].x() - w.e, panel->trailPoints()[i].y() - w.f) < 1e-9);
    }
    QCOMPARE(panel->trailKeyTimes(), (QVector<int>{0, 300, 700, 1000}));
    for (int i = 0; i < panel->trailKeyTimes().size(); i++) {
      const auto w = cutout::samplePose(scrub, clip, panel->trailKeyTimes()[i]).world[size_t(head)];
      QVERIFY(std::hypot(panel->trailKeyPoints()[i].x() - w.e, panel->trailKeyPoints()[i].y() - w.f) < 1e-9);
    }
    panel->selectItem("bone:root");
    QCOMPARE(panel->trailKeyTimes(), (QVector<int>{0, 300, 1000}));

    // Editor-only: nothing reached the documents; defaults restore, the test clip goes away.
    QVERIFY(rig().dump().find("trail") == std::string::npos && rig().dump().find("ghost") == std::string::npos);
    QVERIFY(gesture("onion defaults", 0, [&] { panel->setOnionSettings(OnionSettings{}); }));
    QVERIFY(panel->ghosts().isEmpty() && panel->trailPoints().isEmpty() && panel->canvas()->ghosts().isEmpty());
    QVERIFY(gesture("delete onion clip", 1, [&] { QVERIFY(panel->deleteClip()); }));
    panel->selectClip("joy");
  }

  void mixTable() {
    QVERIFY(gesture("add mix pair", 1, [&] { click("cutoutMixAdd"); }));
    QCOMPARE(rig()["mix"]["pairs"].size(), size_t(1));
    QVERIFY(gesture("mix default", 1, [&] {
      auto* s = find<QSpinBox>("cutoutMixDefault");
      s->setValue(200);
      Q_EMIT s->editingFinished();
    }));
    QCOMPARE(rig()["mix"]["defaultMs"].get<int>(), 200);
  }

  void controllerBindingsAndPreview() {
    panel->showControllerTab();
    QVERIFY(gesture("add binding", 1, [&] { click("cutoutBindingAdd"); }));
    QCOMPARE(controller()["bindings"].size(), size_t(1));
    QCOMPARE(qs(controller()["bindings"][0]["clip"].get<std::string>()), QString("joy"));
    QVERIFY(gesture("binding then=hold", 1, [&] {
      auto* then = find<QComboBox>("cutoutBindingThen0");
      QVERIFY(then);
      then->setCurrentIndex(then->findData("hold"));
      Q_EMIT then->activated(then->currentIndex());
    }));
    QCOMPARE(qs(controller()["bindings"][0]["then"].get<std::string>()), QString("hold"));
    QVERIFY(gesture("binding stopOn", 1, [&] {
      auto* stop = find<QLineEdit>("cutoutBindingStopOn0");
      QVERIFY(stop);
      stop->setText("dialogueEnded");
      Q_EMIT stop->editingFinished();
    }));
    QCOMPARE(qs(controller()["bindings"][0]["stopOn"][0].get<std::string>()), QString("dialogueEnded"));
    QVERIFY(gesture("add layer", 1, [&] { click("cutoutLayerAdd"); }));
    QCOMPARE(controller()["layers"].size(), size_t(2));
    QVERIFY(gesture("preview signal (no edit)", 0, [&] {
      QCOMPARE(panel->previewSignal("actionResult", {{"outcome", "success"}}), QStringList{"binding"});
      QVERIFY(panel->previewSignal("actionResult", {{"outcome", "failure"}}).isEmpty());
      auto* signal = find<QComboBox>("cutoutPreviewSignalName");
      signal->setCurrentIndex(signal->findData("actionResult"));
      find<QLineEdit>("cutoutPreviewSignalParams")->setText("outcome=success");
      click("cutoutPreviewSignal");
    }));
    QVERIFY(find<QLabel>("cutoutPreviewResult")->text().contains("binding"));
  }

  void undoRedoRestoresDocuments() {
    const QJsonArray now = host.documents, previous = host.history.last();
    host.request("edit", {{"edit", QJsonObject{{"type", "undo"}}}}, {});
    pump();
    QCOMPARE(host.documents, previous);
    QCOMPARE(find<QTableWidget>("cutoutLayerTable")->rowCount(), 1);  // panel rebuilt from the view
    host.request("edit", {{"edit", QJsonObject{{"type", "redo"}}}}, {});
    pump();
    QCOMPARE(host.documents, now);
    QCOMPARE(find<QTableWidget>("cutoutLayerTable")->rowCount(), 2);
  }

  void renameDeleteAndOpenEditor() {
    answers = {"주인공"};
    QVERIFY(gesture("rename character", 1, [&] { click("cutoutRenameCharacter"); }));
    QCOMPARE(qs(rig()["name"].get<std::string>()), QString("주인공"));
    answers = {"extra"};
    QVERIFY(gesture("second character", 1, [&] { click("cutoutNewCharacter"); }));
    QCOMPARE(panel->rigId(), QString("extra"));
    QVERIFY(gesture("delete character", 1, [&] { click("cutoutDeleteCharacter"); }));
    QVERIFY(host.data("rig", "extra").is_null() && host.data("controller", "extra").is_null());
    plugin.openEditor("controller", "hero");
    pump();
    QCOMPARE(panel->controllerId(), QString("hero"));
    QCOMPARE(find<QTabWidget>("cutoutBottomTabs")->currentIndex(), 1);
  }

  void skinsAndExpressions() {
    panel->selectRig("hero");
    find<QTabWidget>("cutoutBottomTabs")->setCurrentIndex(0);  // the canvas shows the edited rig, not the controller preview
    panel->setMode(CutoutPanel::Mode::Setup);
    pump();
    auto image = [](const std::vector<cutout::DrawItem>& items, const std::string& slot) {
      for (const auto& i : items) if (i.slot == slot) return qs(i.imageAssetId);
      return QString("-");
    };
    auto drawWith = [&](const std::string& skin) {
      cutout::Instance inst(std::make_shared<const cutout::Rig>(cutout::parseRig("hero", rig())));
      inst.setSkin(skin);
      return inst.drawList();
    };
    auto same = [](const Json& a, const Json& b) {
      if (a != b) qWarning("%s != %s", a.dump().c_str(), b.dump().c_str());
      return a == b;
    };
    auto painted = [&](const QImage& img) { qint64 sum = 0; for (int y = 0; y < img.height(); y++) for (int x = 0; x < img.width(); x++) sum += qBlue(img.pixel(x, y)) * qAlpha(img.pixel(x, y)); return sum; };
    // 스킨: create, fill a part with an imported PNG, drop another PNG on the canvas.
    answers = {"winter"};
    QVERIFY(gesture("new skin", 1, [&] { click("cutoutNewSkin"); }));
    QCOMPARE(qs(rig()["skins"][0]["id"].get<std::string>()), QString("winter"));
    QCOMPARE(panel->previewSkin(), QString("winter"));
    const QImage plain = [&] { const QString keep = panel->previewSkin(); panel->setPreviewSkin(""); const QImage i = panel->thumbnail(QSize(160, 240)); panel->setPreviewSkin(keep); return i; }();
    const QString coat = png("body-winter", QSize(70, 110), QColor("#2563eb"));
    QVERIFY(gesture("import outfit image for the body", 1, [&] { panel->importAndAssignSkinImage(coat, "body"); }));
    const Json made = rig()["skins"][0]["attachments"][0];
    QCOMPARE(qs(made["slot"].get<std::string>() + "/" + made["name"].get<std::string>()), QString("body/body"));  // the part's shown name
    QCOMPARE(made["width"].get<double>(), 70.0);
    QCOMPARE(image(drawWith("winter"), "body"), QString("img-body-winter"));
    QCOMPARE(image(drawWith(""), "body"), QString("img-body"));
    // Canvas drop: a PNG file dropped on the eyes goes into the previewed skin for that part.
    const QString goggles = png("eyes-winter", QSize(30, 12), QColor("#0ea5e9"));
    // Center of the eyes image on the canvas (from the core draw list: instance space + rig origin).
    cutout::DrawItem eyeItem;
    for (const auto& i : drawWith("")) if (i.slot == "eyes") eyeItem = i;
    double ex = 0, ey = 0;
    eyeItem.matrix.apply(eyeItem.width / 2, eyeItem.height / 2, ex, ey);
    const QPointF eyes = panel->canvas()->rigToWidget(QPointF(ex + rig()["canvas"]["originX"].get<double>(), ey + rig()["canvas"]["originY"].get<double>()));
    QVERIFY(gesture("drop PNG on the eyes", 1, [&] {
      QMimeData mime;
      mime.setUrls({QUrl::fromLocalFile(goggles)});
      QDragEnterEvent enter(eyes.toPoint(), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
      QCoreApplication::sendEvent(panel->canvas(), &enter);
      QVERIFY(enter.isAccepted());
      QDropEvent drop(eyes, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
      QCoreApplication::sendEvent(panel->canvas(), &drop);
    }));
    QCOMPARE(image(drawWith("winter"), "eyes"), QString("img-eyes-winter"));
    QCOMPARE(panel->selection(), QString("slot:eyes"));
    QVERIFY(painted(panel->thumbnail(QSize(160, 240))) != painted(plain));  // the canvas shows the outfit
    // A placed element picks its outfit with props.skin (paintElement = what the runtime mounts).
    auto element = [](const QJsonObject& props) {
      return QJsonObject{{"id", "hero-on-title"}, {"kind", "plugin"}, {"width", 300}, {"height", 450},
                         {"extension", QJsonObject{{"plugin", "cutout-animation"}, {"type", "character"}, {"ref", "hero"}, {"props", props}}}};
    };
    auto paintProps = [&](const QJsonObject& props) {
      QImage img(300, 450, QImage::Format_ARGB32_Premultiplied);
      img.fill(Qt::transparent);
      QPainter p(&img);
      if (!plugin.paintElement(&p, element(props), QRectF(0, 0, 300, 450))) return qint64(-1);
      p.end();
      return painted(img);
    };
    const qint64 basePaint = paintProps({}), winterPaint = paintProps({{"skin", "winter"}});
    QVERIFY(basePaint > 0 && winterPaint > basePaint);  // the blue coat
    // 그림 지정… with an explicit name (the closed eyes of this outfit), then 빼기.
    nextSkinImage = CutoutPanel::SkinImageChoice{"winter", "eyes", "closed", "img-eye-open", QString()};
    QVERIFY(gesture("assign closed-eye image", 1, [&] { click("cutoutSkinAssign"); }));
    QVERIFY(skinPrefill && skinPrefill->skin == "winter" && skinPrefill->slot == "eyes" && skinPrefill->name == "open");
    QCOMPARE(rig()["skins"][0]["attachments"].size(), size_t(3));
    auto* list = find<QListWidget>("cutoutSkinAttachments");
    QCOMPARE(list->count(), 3);
    list->setCurrentRow(2);
    QVERIFY(gesture("remove skin attachment", 1, [&] { click("cutoutSkinRemoveAttachment"); }));
    QCOMPARE(rig()["skins"][0]["attachments"].size(), size_t(2));
    QVERIFY(gesture("no skin: drop refused", 0, [&] { panel->setPreviewSkin(""); QMimeData mime; mime.setText("img-body"); QVERIFY(!panel->dropImages(&mime, "slot:eyes")); }));
    // Switching the preview is editor-only.
    auto* skins = find<QListWidget>("cutoutSkinList");
    QVERIFY(gesture("preview skin from the list", 0, [&] { skins->setCurrentRow(1); }));
    QCOMPARE(panel->previewSkin(), QString("winter"));
    answers = {"winter-b"};
    QVERIFY(gesture("duplicate skin", 1, [&] { click("cutoutDuplicateSkin"); }));
    answers = {"snow"};
    QVERIFY(gesture("rename skin", 1, [&] { click("cutoutRenameSkin"); }));
    QCOMPARE(qs(rig()["skins"][1]["id"].get<std::string>()), QString("snow"));
    QVERIFY(same(rig()["skins"][1]["attachments"], rig()["skins"][0]["attachments"]));
    QVERIFY(gesture("delete skin", 1, [&] { click("cutoutDeleteSkin"); }));
    QCOMPARE(rig()["skins"].size(), size_t(1));
    QCOMPARE(panel->previewSkin(), QString());  // deleted skin stops being previewed

    // 표정: change the eyes and the head in Animate mode (pending), capture them as an expression.
    panel->setAutoKey(false);
    panel->setMode(CutoutPanel::Mode::Animate);
    panel->selectClip("idle");
    panel->setPlayhead(0);
    panel->selectItem("slot:eyes");
    QVERIFY(gesture("eyes closed (pending)", 0, [&] {
      auto* combo = find<QComboBox>("cutoutPropAttachment");
      combo->setCurrentIndex(combo->findData("closed"));
      Q_EMIT combo->activated(combo->currentIndex());
    }));
    panel->selectItem("bone:head");
    QVERIFY(gesture("head tilt (pending)", 0, [&] { setSpin("cutoutPropRotation", 12); }));
    QVERIFY(panel->hasPending());
    const Json idleBefore = [&] { for (const auto& c : rig()["clips"]) if (c["id"] == "idle") return c; return Json(); }();
    answers = {"smile"};
    QVERIFY(gesture("capture expression", 1, [&] { click("cutoutCaptureExpression"); }));
    const Json smile = rig()["expressions"][0];
    QCOMPARE(qs(smile["id"].get<std::string>()), QString("smile"));
    QVERIFY(same(smile["slots"], Json({{"eyes", "closed"}})));
    QVERIFY(same(smile["bones"]["head"], Json({{"rotation", 12}})));
    QVERIFY(!panel->hasPending());
    QVERIFY(same([&] { for (const auto& c : rig()["clips"]) if (c["id"] == "idle") return c; return Json(); }(), idleBefore));  // pending keys not committed
    QCOMPARE(panel->previewExpression(), QString("smile"));
    QVERIFY(gesture("nothing changed: capture refused", 0, [&] { panel->setMode(CutoutPanel::Mode::Setup); answers = {"same"}; QVERIFY(!panel->captureExpression()); }));
    panel->setMode(CutoutPanel::Mode::Animate);
    panel->selectClip("idle");
    // Expression table: slot value and bone override.
    QVERIFY(gesture("expression hides the eyes", 1, [&] {
      auto* combo = find<QComboBox>("cutoutExpressionSloteyes");
      QVERIFY(combo);
      combo->setCurrentIndex(combo->findData(QString()));
      Q_EMIT combo->activated(combo->currentIndex());
    }));
    QVERIFY(rig()["expressions"][0]["slots"]["eyes"].is_null());
    QVERIFY(gesture("expression bone text", 1, [&] {
      auto* edit = find<QLineEdit>("cutoutExpressionBonehead");
      QVERIFY(edit);
      edit->setText("rotation=8, y=-70");
      Q_EMIT edit->editingFinished();
    }));
    QVERIFY(same(rig()["expressions"][0]["bones"]["head"], Json({{"rotation", 8}, {"y", -70}})));
    QVERIFY(gesture("expression eyes back", 1, [&] {
      auto* combo = find<QComboBox>("cutoutExpressionSloteyes");
      combo->setCurrentIndex(combo->findData("closed"));
      Q_EMIT combo->activated(combo->currentIndex());
    }));
    // 표정 키 at the playhead; the dopesheet shows the rig expression row.
    panel->setPlayhead(300);
    QVERIFY(gesture("key expression", 1, [&] { click("cutoutKeyExpression"); }));
    QVERIFY(same(*track("idle", "rig", "expression"), Json({{"target", "rig"}, {"property", "expression"}, {"keys", {{{"timeMs", 300}, {"value", "smile"}}}}})));
    const int row = rowOf("rig", "expression");
    QVERIFY(row >= 0);
    QCOMPARE(panel->dopesheet()->rows()[row].label, QString("표정 (리그)"));
    auto* expressions = find<QListWidget>("cutoutExpressionList");
    QVERIFY(gesture("preview no expression", 0, [&] { expressions->setCurrentRow(0); }));
    QCOMPARE(panel->previewExpression(), QString());
    panel->setPlayhead(600);
    QVERIFY(gesture("key no expression", 1, [&] { click("cutoutKeyExpression"); }));
    QVERIFY(keyOf("idle", "rig", "expression", 600)["value"].is_null());
    const auto parsed = std::make_shared<const cutout::Rig>(cutout::parseRig("hero", rig()));
    QCOMPARE(cutout::samplePose(*parsed, parsed->clipIndex("idle"), 400).expression, 0);
    QCOMPARE(cutout::samplePose(*parsed, parsed->clipIndex("idle"), 700).expression, -1);
    // 게임 연결: default skin, and a clip-less binding portraitChanged(portraitId=smile) -> expression smile.
    panel->showControllerTab();
    QVERIFY(gesture("default skin", 1, [&] {
      auto* combo = find<QComboBox>("cutoutControllerDefaultSkin");
      combo->setCurrentIndex(combo->findData("winter"));
      Q_EMIT combo->activated(combo->currentIndex());
    }));
    QCOMPARE(qs(controller()["defaultSkin"].get<std::string>()), QString("winter"));
    QVERIFY(gesture("add binding", 1, [&] { click("cutoutBindingAdd"); }));
    const QString n = QString::number(controller()["bindings"].size() - 1);
    auto cellCombo = [&](const char* name, const QVariant& value) {
      auto* combo = panel->findChild<QComboBox*>(name + n);
      QVERIFY2(combo, name);
      combo->setCurrentIndex(combo->findData(value));
      QVERIFY2(combo->currentIndex() >= 0, name);
      Q_EMIT combo->activated(combo->currentIndex());
    };
    QVERIFY(gesture("binding signal", 1, [&] { cellCombo("cutoutBindingSignal", "portraitChanged"); }));
    QVERIFY(gesture("binding match", 1, [&] { auto* e = panel->findChild<QLineEdit*>("cutoutBindingMatch" + n); e->setText("portraitId=smile"); Q_EMIT e->editingFinished(); }));
    QVERIFY(gesture("binding expression", 1, [&] { cellCombo("cutoutBindingExpression", "smile"); }));
    QVERIFY(gesture("binding without clip", 1, [&] { cellCombo("cutoutBindingClip", QString()); }));
    const Json binding = controller()["bindings"].back();
    QVERIFY(same(binding, Json({{"id", binding["id"]}, {"signal", "portraitChanged"}, {"match", {{"portraitId", "smile"}}}, {"expression", "smile"}})));
    QVERIFY(gesture("binding skin column", 1, [&] { cellCombo("cutoutBindingSkin", "winter"); }));
    QCOMPARE(qs(controller()["bindings"].back()["skin"].get<std::string>()), QString("winter"));
    QVERIFY(gesture("preview the expression binding (no edit)", 0, [&] {
      QCOMPARE(panel->previewSignal("portraitChanged", {{"portraitId", "smile"}}), QStringList{qs(binding["id"].get<std::string>())});
    }));
    // Undo walks back through the same edits; the placed element can pick the skin (props.skin).
    undo();
    QVERIFY(!controller()["bindings"].back().contains("skin"));
    redo();
    // The controller default skin now dresses elements without props.skin (and unknown ids).
    QCOMPARE(paintProps({}), paintProps({{"skin", "winter"}}));
    QCOMPARE(paintProps({{"skin", "nope"}}), paintProps({{"skin", "winter"}}));
    find<QTabWidget>("cutoutBottomTabs")->setCurrentIndex(0);
    panel->setMode(CutoutPanel::Mode::Setup);
  }

  // The real 그림 지정… and 이미지 조각 추가… dialogs driven by mouse: prefill from the tree
  // selection, picking an image never rewrites slot/name, one edit per confirm, undo/redo.
  void imageDialogs() {
    panel->selectRig("hero");
    find<QTabWidget>("cutoutBottomTabs")->setCurrentIndex(0);
    panel->setMode(CutoutPanel::Mode::Setup);
    panel->setPreviewSkin("winter");
    panel->setPreviewExpression("");
    pump();
    const auto scriptedSkin = panel->askSkinImage;
    const auto scriptedPart = panel->askPart;
    panel->askSkinImage = [this](CutoutPanel::SkinImageChoice& c) { return panel->runSkinImageDialog(c); };
    panel->askPart = [this](CutoutPanel::PartChoice& c) { return panel->runPartDialog(c); };
    // Tree row clicks select bones and parts (slot rows) alike.
    QVERIFY(clickTreeRow("bone:root"));
    QCOMPARE(panel->selection(), QString("bone:root"));
    QVERIFY(clickTreeRow("slot:eyes"));
    QCOMPARE(panel->selection(), QString("slot:eyes"));
    const Json before = skinAttachment("winter", "eyes", "open");
    QVERIFY(before.is_object());
    QString title, slot, name, slotAfter, nameAfter, startImage;
    bool slotFree = true, previewShown = false;
    auto observe = [&](QDialog* d, const QString& pick) {
      auto* slots = d->findChild<QComboBox*>("cutoutSkinImageSlot");
      auto* names = d->findChild<QComboBox*>("cutoutSkinImageName");
      auto* assets = d->findChild<QListWidget*>("cutoutSkinImageAssets");
      title = d->windowTitle();
      slot = slots->currentData().toString();
      name = names->currentText();
      slotFree = slots->isEditable();
      startImage = assets->currentItem() ? assets->currentItem()->data(Qt::UserRole).toString() : QString();
      if (!pick.isEmpty() && !clickListItem(assets, pick)) return false;
      slotAfter = slots->currentData().toString();
      nameAfter = names->currentText();
      previewShown = !d->findChild<QLabel*>("cutoutSkinImagePreview")->pixmap().isNull();
      return true;
    };
    onDialog("cutoutSkinImageDialog", [&](QDialog* d) { return observe(d, "img-eye-closed"); });
    QVERIFY(gesture("skin image dialog on the clicked part", 1, [&] { click("cutoutSkinAssign"); }));
    QCOMPARE(title, QString("스킨 그림 지정 — winter"));
    QCOMPARE(slot, QString("eyes"));
    QCOMPARE(name, QString("open"));  // the name the part shows now
    QVERIFY(!slotFree);
    QCOMPARE(startImage, qs(before["imageAssetId"].get<std::string>()));  // the skin's current image
    QCOMPARE(slotAfter, QString("eyes"));
    QCOMPARE(nameAfter, QString("open"));
    QVERIFY(previewShown);
    QCOMPARE(qs(skinAttachment("winter", "eyes", "open")["imageAssetId"].get<std::string>()), QString("img-eye-closed"));
    QCOMPARE(host.lastEdit.value("type").toString(), QString("replace"));
    undo();
    QVERIFY(skinAttachment("winter", "eyes", "open") == before);
    redo();
    QCOMPARE(qs(skinAttachment("winter", "eyes", "open")["imageAssetId"].get<std::string>()), QString("img-eye-closed"));
    undo();
    // A selected bone offers its first part; cancel is no edit.
    QVERIFY(clickTreeRow("bone:head"));
    onDialog("cutoutSkinImageDialog", [&](QDialog* d) { observe(d, QString()); return false; });
    QVERIFY(gesture("skin image dialog from a bone, cancelled", 0, [&] { click("cutoutSkinAssign"); }));
    QCOMPARE(slot, QString("eyes"));
    // Another slot from the combo + a new attachment name typed in: the image keeps both.
    QVERIFY(clickTreeRow("slot:body"));
    onDialog("cutoutSkinImageDialog", [&](QDialog* d) {
      auto* names = d->findChild<QComboBox*>("cutoutSkinImageName");
      names->lineEdit()->selectAll();
      QTest::keyClicks(names->lineEdit(), "coat-open");
      return observe(d, "img-body-winter") && nameAfter == "coat-open" && slotAfter == "body";
    });
    QVERIFY(gesture("skin image with a new name", 1, [&] { click("cutoutSkinAssign"); }));
    QCOMPARE(slot, QString("body"));
    QCOMPARE(qs(skinAttachment("winter", "body", "coat-open")["imageAssetId"].get<std::string>()), QString("img-body-winter"));
    undo();
    QVERIFY(skinAttachment("winter", "body", "coat-open").is_null());

    // 이미지 조각 추가…: the clicked part prefills 조각 ID and picking images never rewrites it.
    QVERIFY(clickTreeRow("slot:eyes"));
    const size_t parts = rig()["attachments"].size(), slotCount = rig()["slots"].size();
    QString prefilled, afterPick;
    onDialog("cutoutPartDialog", [&](QDialog* d) {
      auto* slots = d->findChild<QComboBox*>("cutoutPartSlot");
      auto* assets = d->findChild<QListWidget*>("cutoutPartAssetList");
      prefilled = slots->currentText();
      if (!clickListItem(assets, "img-body") || !clickListItem(assets, "img-eye-open")) return false;
      afterPick = slots->currentText();
      QTest::keyClicks(d->findChild<QLineEdit*>("cutoutPartName"), "half");
      return true;
    });
    QVERIFY(gesture("part dialog keeps the selected part", 1, [&] { click("cutoutAddPart"); }));
    QCOMPARE(prefilled, QString("eyes"));
    QCOMPARE(afterPick, QString("eyes"));
    QCOMPARE(rig()["attachments"].size(), parts + 1);
    QCOMPARE(rig()["slots"].size(), slotCount);
    const Json half = rig()["attachments"].back();
    QVERIFY(half["slot"] == "eyes" && half["name"] == "half" && half["imageAssetId"] == "img-eye-open");
    undo();
    QCOMPARE(rig()["attachments"].size(), parts);
    redo();
    QCOMPARE(rig()["attachments"].size(), parts + 1);
    undo();
    // From a bone: an empty 조각 ID follows the picked image; a typed one is kept.
    QVERIFY(clickTreeRow("bone:root"));
    QStringList seen;
    onDialog("cutoutPartDialog", [&](QDialog* d) {
      auto* slots = d->findChild<QComboBox*>("cutoutPartSlot");
      auto* assets = d->findChild<QListWidget*>("cutoutPartAssetList");
      seen << slots->currentText();
      clickListItem(assets, "img-eye-open");
      seen << slots->currentText();
      clickListItem(assets, "img-body");
      seen << slots->currentText();
      slots->lineEdit()->selectAll();
      QTest::keyClicks(slots->lineEdit(), "cape");
      clickListItem(assets, "img-eye-open");
      seen << slots->currentText();
      return false;
    });
    QVERIFY(gesture("part dialog auto id, cancelled", 0, [&] { click("cutoutAddPart"); }));
    // (seen[0] may already follow the row the focused list made current: that is an auto value too.)
    QCOMPARE(seen.mid(1), (QStringList{"img-eye-open", "img-body", "cape"}));
    panel->askSkinImage = scriptedSkin;
    panel->askPart = scriptedPart;
    panel->setPreviewSkin("");
  }

  void ikConstraints() {
    panel->selectRig("hero");
    find<QTabWidget>("cutoutBottomTabs")->setCurrentIndex(0);
    panel->setMode(CutoutPanel::Mode::Setup);
    panel->setPreviewSkin("");
    panel->setPreviewExpression("");
    panel->setTool(RigCanvas::Move);
    pump();
    // An arm: shoulder -> elbow -> wrist (each bone 60 px above its parent), elbow bent 40°.
    panel->selectItem("bone:root");
    answers = {"shoulder"};
    QVERIFY(gesture("add shoulder", 1, [&] { click("cutoutAddBone"); }));
    answers = {"elbow"};
    QVERIFY(gesture("add elbow", 1, [&] { click("cutoutAddBone"); }));
    answers = {"wrist"};
    QVERIFY(gesture("add wrist", 1, [&] { click("cutoutAddBone"); }));
    panel->selectItem("bone:elbow");
    QVERIFY(gesture("bend the elbow", 1, [&] { setSpin("cutoutPropRotation", 40); }));
    auto solved = [&](const Json& data) {
      const cutout::Rig r = cutout::parseRig("hero", data);
      return std::make_pair(r, cutout::samplePose(r, -1, 0));
    };
    auto worldOf = [&](const Json& data, const char* bone) {
      const auto [r, pose] = solved(data);
      const auto& w = pose.world[size_t(r.boneIndex(bone))];
      return QPointF(w.e, w.f);
    };
    auto bendSide = [&](const Json& data) {
      const QPointF s = worldOf(data, "shoulder"), e = worldOf(data, "elbow"), w = worldOf(data, "wrist");
      return (e.x() - s.x()) * (w.y() - e.y()) - (e.y() - s.y()) * (w.x() - e.x());
    };
    auto closeTo = [](QPointF a, QPointF b, double eps = 1e-6) { return std::hypot(a.x() - b.x(), a.y() - b.y()) <= eps; };
    const QPointF wristBefore = worldOf(rig(), "wrist");
    const double sideBefore = bendSide(rig());
    // 팔·다리 IK from the selected forearm bone: one edit, a target bone at the wrist, nothing jumps.
    QVERIFY(gesture("new two-bone IK", 1, [&] { click("cutoutNewIk"); }));
    QCOMPARE(rig()["ik"].size(), size_t(1));
    QVERIFY(rig()["ik"][0]["bones"] == Json::array({"shoulder", "elbow"}));
    const std::string target = rig()["ik"][0]["target"].get<std::string>();
    QCOMPARE(qs(target), QString("elbow-target"));
    QCOMPARE(panel->ikId(), QString("elbow-ik"));
    QCOMPARE(panel->selection(), QString("bone:elbow-target"));
    QVERIFY(closeTo(worldOf(rig(), target.c_str()), wristBefore) && closeTo(worldOf(rig(), "wrist"), wristBefore));
    QVERIFY(bendSide(rig()) * sideBefore > 0);  // the inferred bend keeps the elbow where it was
    QCOMPARE(find<QListWidget>("cutoutIkList")->count(), 1);
    QVERIFY(find<QWidget>("cutoutIkMix")->isEnabled() && !find<QWidget>("cutoutIkKey")->isEnabled());  // keys only in Animate mode
    // The target is drawn as an orange crosshair (deselected: the selection color would win).
    const cutout::Rig parsed = cutout::parseRig("hero", rig());
    QVERIFY(panel->canvas()->isIkTarget(parsed.boneIndex(target)) && !panel->canvas()->isIkTarget(parsed.boneIndex("wrist")));
    panel->selectItem("");
    pump();
    const QPointF at = panel->canvas()->rigToWidget(world(target));
    const QImage shot = panel->canvas()->grab().toImage();
    const qreal dpr = shot.devicePixelRatio();
    int orange = 0;
    for (int dx = 9; dx <= 11; dx++)
      for (int dy = -1; dy <= 1; dy++) {
        const QColor c = shot.pixelColor(int((at.x() + dx) * dpr), int((at.y() + dy) * dpr));
        orange += c.red() > 200 && c.green() > 80 && c.green() < 160 && c.blue() < 90;
      }
    QVERIFY2(orange > 0, "IK target crosshair");
    // Drag the target in Setup mode: the arm follows live (preview only), release = ONE edit.
    const QPointF shoulderAt = panel->canvas()->rigToWidget(world("shoulder"));
    const QPointF to = at + (shoulderAt - at) * 0.3 + QPointF(6, 0);
    const int editsBefore = host.edits;
    mouse(panel->canvas(), QEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton);
    for (int i = 1; i <= 4; i++) mouse(panel->canvas(), QEvent::MouseMove, at + (to - at) * (i / 4.0), Qt::NoButton, Qt::LeftButton);
    QCOMPARE(host.edits, editsBefore);
    QVERIFY(closeTo(worldOf(panel->displayData(), "wrist"), worldOf(panel->displayData(), target.c_str()), 1e-6));
    QVERIFY(!closeTo(worldOf(panel->displayData(), "wrist"), wristBefore, 1));
    QVERIFY(gesture("drag IK target (setup)", 1, [&] { mouse(panel->canvas(), QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton); }));
    QCOMPARE(host.edits, editsBefore + 1);
    QCOMPARE(host.lastEdit.value("type").toString(), QString("replace"));
    const QPointF moved = worldOf(rig(), target.c_str());
    QVERIFY(!closeTo(moved, wristBefore, 1) && closeTo(worldOf(rig(), "wrist"), moved, 1e-6));  // the limb follows the target
    for (const auto& b : rig()["bones"]) if (b["id"] == "elbow") QCOMPARE(b["rotation"].get<double>(), 40.0);  // ... the arm's own data untouched
    // Flip the bend (Setup: the constraint's setup value).
    panel->selectIk("elbow-ik");
    QVERIFY(gesture("toggle bend", 1, [&] { click("cutoutIkBend"); }));
    QCOMPARE(rig()["ik"][0]["bendPositive"].get<bool>(), false);
    QVERIFY(bendSide(rig()) * sideBefore < 0 && closeTo(worldOf(rig(), "wrist"), moved, 1e-6));
    // Animate mode: dragging the target keys its position when K is pressed (auto-key off).
    panel->setMode(CutoutPanel::Mode::Animate);
    panel->selectClip("joy");
    panel->setAutoKey(false);
    panel->setPlayhead(200);
    pump();
    QVERIFY(find<QWidget>("cutoutIkKey")->isEnabled() && find<QWidget>("cutoutBakeIk")->isEnabled());
    const QPointF from = panel->canvas()->rigToWidget(moved);
    QVERIFY(gesture("drag target in animate (pending)", 0, [&] { drag(panel->canvas(), from, from + QPointF(-8, 10)); }));
    QVERIFY(panel->hasPending());
    QVERIFY(closeTo(worldOf(panel->displayData(), "wrist"), worldOf(panel->displayData(), target.c_str()), 1e-6));
    QVERIFY(gesture("K keys the target", 1, [&] { QVERIFY(panel->keySelected()); }));
    QVERIFY(!keyOf("joy", "bone:elbow-target", "x", 200).is_null() && !keyOf("joy", "bone:elbow-target", "y", 200).is_null());
    // Mix and bend become keys of the constraint (auto-key on: each change commits).
    panel->setAutoKey(true);
    panel->setPlayhead(100);
    QVERIFY(gesture("mix key", 1, [&] {
      auto* mix = find<QDoubleSpinBox>("cutoutIkMix");
      mix->setValue(0.5);
      Q_EMIT mix->editingFinished();
    }));
    QCOMPARE(keyOf("joy", "ik:elbow-ik", "mix", 100)["value"].get<double>(), 0.5);
    QCOMPARE(rig()["ik"][0].value("mix", 1.0), 1.0);  // the setup value stays
    QVERIFY(gesture("bend key", 1, [&] { click("cutoutIkBend"); }));
    QCOMPARE(keyOf("joy", "ik:elbow-ik", "bendPositive", 100)["value"].get<bool>(), true);
    QVERIFY(!keyOf("joy", "ik:elbow-ik", "bendPositive", 100).contains("interp"));
    pump();
    QVERIFY(rowOf("ik:elbow-ik", "mix") >= 0 && rowOf("ik:elbow-ik", "bendPositive") >= 0);
    QVERIFY(panel->dopesheet()->rows()[rowOf("ik:elbow-ik", "bendPositive")].attachment);  // drawn as step keys
    QVERIFY(panel->dopesheet()->rows()[rowOf("ik:elbow-ik", "mix")].label.contains("혼합"));
    // Clicking an IK row shows its constraint (IK page) and its mix curve in the graph.
    panel->dopesheet()->selectKey(rowOf("ik:elbow-ik", "mix"), 100);
    panel->dopesheet()->onRowSelected(rowOf("ik:elbow-ik", "mix"), false);
    pump();
    QVERIFY(panel->selection().isEmpty() && panel->ikId() == "elbow-ik");
    QCOMPARE(find<QTabWidget>("cutoutLookTabs")->currentIndex(), 2);
    QVERIFY(!panel->graph()->channels().isEmpty() && panel->graph()->channels()[0].property == "mix");
    {
      const auto [r, pose] = solved(rig());
      const cutout::Pose at100 = cutout::samplePose(r, r.clipIndex("joy"), 100);
      QCOMPARE(at100.ikMix[0], 0.5);
    }
    panel->setPlayhead(300);
    QVERIFY(gesture("IK key", 1, [&] { click("cutoutIkKey"); }));
    QVERIFY(!keyOf("joy", "ik:elbow-ik", "mix", 300).is_null() && !keyOf("joy", "ik:elbow-ik", "bendPositive", 300).is_null());
    // Bake: the solved rotations become keys and mix goes to 0 there (the pose stays).
    const auto bakeBefore = solved(rig());
    const cutout::Pose solvedAt = cutout::samplePose(bakeBefore.first, bakeBefore.first.clipIndex("joy"), 300);
    QVERIFY(gesture("bake IK", 1, [&] { click("cutoutBakeIk"); }));
    QCOMPARE(keyOf("joy", "ik:elbow-ik", "mix", 300)["value"].get<double>(), 0.0);
    {
      const auto [r, pose] = solved(rig());
      const cutout::Pose baked = cutout::samplePose(r, r.clipIndex("joy"), 300);
      const size_t wrist = size_t(r.boneIndex("wrist"));
      QVERIFY(std::hypot(baked.world[wrist].e - solvedAt.world[wrist].e, baked.world[wrist].f - solvedAt.world[wrist].f) < 1e-3);
      QVERIFY(!keyOf("joy", "bone:shoulder", "rotation", 300).is_null() && !keyOf("joy", "bone:elbow", "rotation", 300).is_null());
    }
    undo();
    QVERIFY(keyOf("joy", "bone:shoulder", "rotation", 300).is_null());
    redo();
    // Rename keeps the keys attached; delete (with its unused target) and undo.
    answers = {"left-arm"};
    QVERIFY(gesture("rename IK", 1, [&] { click("cutoutRenameIk"); }));
    QCOMPARE(panel->ikId(), QString("left-arm"));
    QVERIFY(!keyOf("joy", "ik:left-arm", "mix", 100).is_null() && keyOf("joy", "ik:elbow-ik", "mix", 100).is_null());
    panel->setAutoKey(false);
    panel->setMode(CutoutPanel::Mode::Setup);
    QVERIFY(gesture("delete IK", 1, [&] { click("cutoutDeleteIk"); }));
    QVERIFY(!rig().contains("ik") && track("joy", "ik:left-arm", "mix") == nullptr && !cutout::setupWorld(rig()).count("elbow-target"));
    undo();
    QCOMPARE(rig()["ik"][0]["id"].get<std::string>(), std::string("left-arm"));
    // Hierarchy edits that would break the constraint are refused (no edit, a status instead).
    QVERIFY(gesture("reparent breaking the chain", 0, [&] { QVERIFY(!panel->requestReparent("elbow", "root")); }));
    QVERIFY(panel->status().contains("IK"));
    QVERIFY(gesture("target into its own chain", 0, [&] { panel->selectIk("left-arm"); QVERIFY(!panel->setIkTarget("wrist")); }));
  }

  // 조준 IK on a limb as cutout rigs make it (rotation 0, image hanging down): the aim axis follows
  // the image, creating the constraint moves nothing, dragging the target turns the image's long
  // axis onto it; 조준 축 and 현재 방향으로 맞추기 are one edit each; undo restores everything.
  void aimIkOnHangingArm() {
    panel->selectRig("hero");
    find<QTabWidget>("cutoutBottomTabs")->setCurrentIndex(0);
    panel->setMode(CutoutPanel::Mode::Setup);
    panel->setPreviewSkin("");
    panel->setPreviewExpression("");
    panel->setTool(RigCanvas::Move);
    pump();
    const Json start = rig();
    panel->selectItem("bone:root");
    answers = {"forearm"};
    QVERIFY(gesture("add a limb bone", 1, [&] { click("cutoutAddBone"); }));
    nextPart = CutoutPanel::PartChoice{"img-body", QString(), "sleeve", QString(), "forearm"};  // 60x100: long axis = image y
    QVERIFY(gesture("limb image", 1, [&] { click("cutoutAddPart"); }));
    panel->selectItem("slot:sleeve");
    QVERIFY(gesture("the image hangs from the shoulder", 1, [&] { setSpin("cutoutPropPivotY", 0); }));
    const Json made = rig();  // (iterating rig()["..."] directly would read a destroyed temporary)
    for (const auto& b : made["bones"]) if (b["id"] == "forearm") QCOMPARE(b.value("rotation", 0.0), 0.0);
    // Angle (degrees) between the image's long axis (its +y in rig space) and the bone -> target line.
    auto imageError = [&](const Json& data, const std::string& target) {
      const cutout::Rig r = cutout::parseRig("hero", data);
      const cutout::Pose p = cutout::samplePose(r, -1, 0);
      const int si = r.slotIndex("sleeve");
      const cutout::Affine m = p.world[size_t(r.boneIndex("forearm"))] * cutout::attachmentPlacement(*r.attachment("sleeve", *r.slots[size_t(si)].attachment));
      const auto& o = p.world[size_t(r.boneIndex("forearm"))];
      const auto& t = p.world[size_t(r.boneIndex(target))];
      return std::fabs(std::remainder(std::atan2(m.d, m.c) - std::atan2(t.f - o.f, t.e - o.e), 2 * M_PI)) * 180 / M_PI;
    };
    const auto before = cutout::setupWorld(rig());
    panel->selectItem("bone:forearm");
    QVERIFY(gesture("new aim IK on the hanging arm", 1, [&] { click("cutoutNewAimIk"); }));
    Json k;
    const Json withIk = rig();
    for (const auto& x : withIk["ik"]) if (x["bones"] == Json::array({"forearm"})) k = x;
    QVERIFY2(k.is_object() && std::fabs(k.value("aimAxis", 0.0) - 90) < 1e-6, k.dump().c_str());
    const std::string target = k["target"].get<std::string>();
    {
      const cutout::Rig r = cutout::parseRig("hero", rig());
      const cutout::Pose p = cutout::samplePose(r, -1, 0);
      QVERIFY(std::fabs(p.applied[size_t(r.boneIndex("forearm"))].rotation) < 1e-6);  // creating it turned nothing
      const auto& w = p.world[size_t(r.boneIndex("forearm"))];
      QVERIFY(std::hypot(w.e - before.at("forearm").e, w.f - before.at("forearm").f) < 1e-6 && std::fabs(w.b - before.at("forearm").b) < 1e-9);
      double dx = 0, dy = 0;
      QVERIFY(cutout::ikAimDirection(r, p, r.ikIndex(k["id"].get<std::string>()), dx, dy) && std::fabs(dx) < 1e-6 && dy > 0.999);  // the canvas arrow points down
    }
    QVERIFY(imageError(rig(), target) < 1e-6);
    QCOMPARE(panel->ikId(), qs(k["id"].get<std::string>()));
    QVERIFY(find<QWidget>("cutoutIkAimAxis")->isEnabled() && find<QWidget>("cutoutIkFitAimAxis")->isEnabled());
    QCOMPARE(find<QDoubleSpinBox>("cutoutIkAimAxis")->value(), 90.0);
    // Drag the target sideways and up on the canvas: one edit, the image points at it.
    panel->selectItem("");
    pump();
    const QPointF at = panel->canvas()->rigToWidget(world(target));
    QVERIFY(gesture("drag the aim target", 1, [&] { drag(panel->canvas(), at, at + QPointF(90, -40)); }));
    QVERIFY(std::hypot(world(target).x() - before.at("forearm").e, world(target).y() - before.at("forearm").f) > 20);
    const double error = imageError(rig(), target);
    QVERIFY2(error < 1.0, qPrintable(QString::number(error)));
    // 조준 축 0: the bone's +x aims instead (the 1.3.0 behaviour), one edit.
    panel->selectIk(qs(k["id"].get<std::string>()));
    QVERIFY(gesture("aim axis 0", 1, [&] { setSpin("cutoutIkAimAxis", 0); }));
    QVERIFY(!rig()["ik"].back().contains("aimAxis"));
    QVERIFY(std::fabs(imageError(rig(), target) - 90) < 1e-3);
    // 현재 방향으로 맞추기: the axis becomes where the bone sees the target on its own, so the
    // constraint stops turning it (rotation 0 = its setup pose).
    QVERIFY(gesture("fit the aim axis", 1, [&] { click("cutoutIkFitAimAxis"); }));
    {
      const cutout::Rig r = cutout::parseRig("hero", rig());
      const cutout::Pose p = cutout::samplePose(r, -1, 0);
      QVERIFY(std::fabs(p.applied[size_t(r.boneIndex("forearm"))].rotation) < 1e-4);
      QVERIFY(rig()["ik"].back().contains("aimAxis"));
    }
    QCOMPARE(find<QDoubleSpinBox>("cutoutIkAimAxis")->value(), std::round(rig()["ik"].back()["aimAxis"].get<double>() * 10) / 10);
    // A two-bone IK has no aim axis: the row is disabled.
    panel->selectIk("left-arm");
    pump();
    QVERIFY(!find<QWidget>("cutoutIkAimAxis")->isEnabled() && !find<QWidget>("cutoutIkFitAimAxis")->isEnabled());
    QVERIFY(gesture("aim axis on a two-bone IK refused", 0, [&] { QVERIFY(!panel->setIkAimAxis(30)); }));
    for (int i = 0; i < 7; i++) undo();
    QVERIFY(rig() == start);
    redo();
    undo();
    QVERIFY(rig() == start);
  }

  void meshEditing() {
    panel->selectRig("hero");
    find<QTabWidget>("cutoutBottomTabs")->setCurrentIndex(0);
    panel->setMode(CutoutPanel::Mode::Setup);
    panel->setPreviewSkin("");
    panel->setPreviewExpression("");
    panel->setTool(RigCanvas::Move);
    pump();
    // Two bones (upper -> lower, 60 px apart) and a 40x140 bar whose bottom-right corner is transparent.
    panel->selectItem("bone:root");
    answers = {"mesh-upper"};
    QVERIFY(gesture("add mesh-upper", 1, [&] { click("cutoutAddBone"); }));
    answers = {"mesh-lower"};
    QVERIFY(gesture("add mesh-lower", 1, [&] { click("cutoutAddBone"); }));
    QImage bar(40, 140, QImage::Format_ARGB32);
    bar.fill(QColor("#2a9d8f"));
    for (int y = 105; y < 140; y++) for (int x = 20; x < 40; x++) bar.setPixelColor(x, y, Qt::transparent);
    const QString file = dir.filePath("bar.png");
    QVERIFY(bar.save(file));
    QVERIFY(gesture("add bar part", 1, [&] { panel->importAndAddPart(file, "bar", "bar", "mesh-upper"); }));
    panel->selectItem("slot:bar");
    find<QTabWidget>("cutoutLookTabs")->setCurrentIndex(3);
    pump();
    QVERIFY(find<QWidget>("cutoutConvertMesh")->isEnabled() && !find<QWidget>("cutoutMeshVertices")->isEnabled());
    // 메시로 변환 (alpha-trimmed 4x2 grid): the transparent cell is left out, the part looks the same.
    find<QSpinBox>("cutoutMeshRows")->setValue(4);
    find<QSpinBox>("cutoutMeshCols")->setValue(2);
    find<QCheckBox>("cutoutMeshTrim")->setChecked(true);
    QVERIFY(gesture("convert to mesh", 1, [&] { click("cutoutConvertMesh"); }));
    auto barOf = [](const Json& data) { for (const auto& a : data["attachments"]) if (a["slot"] == "bar") return a; return Json(); };
    QCOMPARE(qs(barOf(rig())["type"].get<std::string>()), QString("mesh"));
    QCOMPARE(barOf(rig())["vertices"].size(), size_t(28));   // 15 grid vertices minus the corner only the empty cell used
    QCOMPARE(barOf(rig())["triangles"].size(), size_t(42));  // 7 cells x 2
    QCOMPARE(panel->meshSlot(), QString("bar"));
    // The canvas draws the mesh with the image (a cell centre shows the bar colour).
    const cutout::Rig parsed = cutout::parseRig("hero", rig());
    const cutout::Pose setup = cutout::samplePose(parsed, -1, 0);
    const cutout::Attachment* att = nullptr;
    const cutout::Affine place = attachmentMatrix(parsed, setup, parsed.slotIndex("bar"), &att);
    QVERIFY(att && att->mesh);
    double cx, cy;
    place.apply(10, 52, cx, cy);
    {
      const QImage shot = panel->canvas()->grab().toImage();
      const QPointF at = panel->canvas()->rigToWidget(QPointF(cx, cy)) * shot.devicePixelRatio();
      const QColor c = shot.pixelColor(at.toPoint());
      QVERIFY2(std::abs(c.red() - 0x2a) < 30 && std::abs(c.green() - 0x9d) < 30 && std::abs(c.blue() - 0x8f) < 30, qPrintable(c.name()));
    }
    // 정점 편집: click selects a vertex, a band selects several, dragging moves them (setup: uvs follow).
    QVERIFY(gesture("vertex mode", 0, [&] { click("cutoutMeshVertices"); }));
    QCOMPARE(panel->meshEdit(), CutoutPanel::MeshEdit::Vertices);
    QCOMPARE(panel->canvas()->meshMode(), int(RigCanvas::MeshVertices));
    RigCanvas* canvas = panel->canvas();
    QVector<QPointF> pts = canvas->meshVertices();
    QCOMPARE(pts.size(), 14);
    QVERIFY(gesture("select vertex", 0, [&] { tap(canvas, canvas->rigToWidget(pts[0])); }));
    QCOMPARE(panel->selectedVertices(), QVector<int>({0}));
    QRectF box;
    for (const QPointF& p : pts) box |= QRectF(canvas->rigToWidget(p), QSizeF(0.01, 0.01));
    QVERIFY(gesture("band select", 0, [&] { drag(canvas, box.topLeft() - QPointF(15, 15), QPointF(box.right() + 15, box.center().y() + 2)); }));
    QVERIFY(panel->selectedVertices().size() >= 4 && panel->selectedVertices().size() < 14);
    QVERIFY(gesture("select vertex again", 0, [&] { tap(canvas, canvas->rigToWidget(pts[0])); }));
    const Json before = barOf(rig());
    const double zoom = canvas->rigToWidget(QPointF(1, 0)).x() - canvas->rigToWidget(QPointF(0, 0)).x();
    QVERIFY(gesture("drag vertex (setup)", 1, [&] { drag(canvas, canvas->rigToWidget(pts[0]), canvas->rigToWidget(pts[0]) + QPointF(-8, 0) * zoom); }));
    QVERIFY(std::fabs(barOf(rig())["vertices"][0].get<double>() - (before["vertices"][0].get<double>() - 8)) < 0.5);
    QVERIFY(barOf(rig())["uvs"][0] == before["uvs"][0]);  // dragged left of the image: the uv stays clamped at 0
    QCOMPARE(host.lastEdit.value("type").toString(), QString("replace"));
    // 자동 웨이트 over the two checked bones.
    auto* bones = find<QListWidget>("cutoutMeshBones");
    for (int i = 0; i < bones->count(); i++) {
      const QString id = bones->item(i)->data(Qt::UserRole).toString();
      bones->item(i)->setCheckState(id == "mesh-upper" || id == "mesh-lower" ? Qt::Checked : Qt::Unchecked);
    }
    QVERIFY(gesture("auto weights", 1, [&] { click("cutoutMeshAutoWeights"); }));
    std::set<std::string> used;
    const Json weights = barOf(rig())["weights"];  // a named copy: ranging over a temporary's member dangles
    for (const auto& list : weights) for (const auto& w : list) used.insert(w["bone"].get<std::string>());
    QVERIFY2(used == std::set<std::string>({"mesh-upper", "mesh-lower"}), weights.dump().c_str());
    QCOMPARE(barOf(rig())["weights"].size(), size_t(14));
    // 웨이트 칠하기: one stroke over a vertex paints it fully onto mesh-lower (one edit), heat map red.
    QVERIFY(gesture("weights mode", 0, [&] { click("cutoutMeshWeights"); }));
    QCOMPARE(canvas->meshMode(), int(RigCanvas::MeshWeights));
    panel->setBrush("mesh-lower", 1, 1, 6);
    pts = canvas->meshVertices();
    const int paintAt = 13;
    QVERIFY(gesture("paint stroke", 1, [&] { drag(canvas, canvas->rigToWidget(pts[paintAt]), canvas->rigToWidget(pts[paintAt]) + QPointF(1, 1)); }));
    QVERIFY(barOf(rig())["weights"][paintAt] == Json::array({{{"bone", "mesh-lower"}, {"weight", 1}}}));
    {
      pump();
      const QImage shot = canvas->grab().toImage();
      const QColor c = shot.pixelColor((canvas->rigToWidget(canvas->meshVertices()[paintAt]) * shot.devicePixelRatio()).toPoint());
      QVERIFY2(c.red() > 200 && c.green() < 100, qPrintable(c.name()));  // weight 1 = red dot
      if (const QString evidence = qEnvironmentVariable("CUTOUT_STUDIO_EVIDENCE"); !evidence.isEmpty())
        QVERIFY(QDir().mkpath(evidence) && panel->window()->grab().save(evidence + "/studio-mesh-weights.png"));
    }
    // Animate: dragging a vertex is a pending deform (no edit); K keys it; dopesheet shows the row.
    panel->setMode(CutoutPanel::Mode::Animate);
    panel->selectClip("joy");
    panel->setAutoKey(false);
    panel->setPlayhead(100);
    panel->selectItem("slot:bar");
    pump();
    QCOMPARE(panel->meshEdit(), CutoutPanel::MeshEdit::Off);  // weights are setup data: the mode ended
    QVERIFY(find<QWidget>("cutoutMeshDeformKey")->isEnabled() && !find<QWidget>("cutoutMeshWeights")->isEnabled());
    QVERIFY(gesture("vertex mode (animate)", 0, [&] { click("cutoutMeshVertices"); }));
    pts = canvas->meshVertices();
    QVERIFY(gesture("deform drag (pending)", 0, [&] { drag(canvas, canvas->rigToWidget(pts[2]), canvas->rigToWidget(pts[2]) + QPointF(10, 0) * zoom); }));
    QVERIFY(panel->hasPending());
    QVERIFY(gesture("K keys the deform", 1, [&] { QVERIFY(panel->keySelected()); }));
    const Json key = keyOf("joy", "slot:bar", "deform", 100);
    QVERIFY(!key.is_null() && key["offsets"].size() == 28);
    QVERIFY(std::hypot(key["offsets"][4].get<double>(), key["offsets"][5].get<double>()) > 5);  // vertex 2 moved ~10 px
    QCOMPARE(track("joy", "slot:bar", "deform")->value("attachment", std::string()), std::string("bar"));
    pump();
    const int row = rowOf("slot:bar", "deform:bar");
    QVERIFY(row >= 0 && panel->dopesheet()->rows()[row].label.contains("변형") && !panel->dopesheet()->rows()[row].attachment);
    // The deformed vertex is drawn where it was dragged (the canvas pose samples the key).
    QVERIFY(std::hypot(canvas->meshVertices()[2].x() - pts[2].x(), canvas->meshVertices()[2].y() - pts[2].y()) > 5);
    if (const QString evidence = qEnvironmentVariable("CUTOUT_STUDIO_EVIDENCE"); !evidence.isEmpty())
      QVERIFY(panel->window()->grab().save(evidence + "/studio-mesh-deform.png"));
    // Dopesheet key gestures carry the attachment: move the deform key, then undo/redo.
    QVERIFY(gesture("move deform key", 1, [&] { QVERIFY(panel->moveKeys({KeyRef{"slot:bar", "deform:bar", 100}}, 50)); }));
    QVERIFY(keyOf("joy", "slot:bar", "deform", 100).is_null() && !keyOf("joy", "slot:bar", "deform", 150).is_null());
    undo();
    QVERIFY(!keyOf("joy", "slot:bar", "deform", 100).is_null());
    undo();
    QVERIFY(track("joy", "slot:bar", "deform") == nullptr);
    redo();
    QVERIFY(!keyOf("joy", "slot:bar", "deform", 100).is_null());
    panel->setPlayhead(100);
    QVERIFY(gesture("delete deform key", 1, [&] { click("cutoutMeshDeleteDeform"); }));
    QVERIFY(track("joy", "slot:bar", "deform") == nullptr);
    // Setup: unbind, then back to a plain image.
    panel->setMode(CutoutPanel::Mode::Setup);
    panel->selectItem("slot:bar");
    QVERIFY(gesture("unbind", 1, [&] { click("cutoutMeshUnbind"); }));
    QVERIFY(!barOf(rig()).contains("weights"));
    QVERIFY(gesture("back to region", 1, [&] { click("cutoutConvertRegion"); }));
    QVERIFY(!barOf(rig()).contains("type") && !barOf(rig()).contains("vertices"));
    QCOMPARE(panel->meshEdit(), CutoutPanel::MeshEdit::Off);
    undo();
    QCOMPARE(qs(barOf(rig())["type"].get<std::string>()), QString("mesh"));
  }

  // Freeform meshes (plugin 1.6.0): 윤곽 그리기 (clicks + double-click = one edit, Esc cancels),
  // 자동 윤곽 from a generated PNG of known shape with a canvas preview, 정점 추가 inside / on the
  // outline, 가장자리 추가 by dragging, Delete, 가장자리 삭제, 다시 삼각분할, and undo/redo of it all.
  void freeformMeshEditing() {
    panel->selectRig("hero");
    find<QTabWidget>("cutoutBottomTabs")->setCurrentIndex(0);
    panel->setMode(CutoutPanel::Mode::Setup);
    panel->setPreviewSkin("");
    panel->setPreviewExpression("");
    panel->setTool(RigCanvas::Move);
    pump();
    // An 80x60 picture: an opaque ellipse filling it, transparent corners.
    QImage blob(80, 60, QImage::Format_ARGB32);
    blob.fill(Qt::transparent);
    for (int y = 0; y < 60; y++)
      for (int x = 0; x < 80; x++)
        if (std::pow((x + 0.5 - 40) / 40, 2) + std::pow((y + 0.5 - 30) / 30, 2) <= 1) blob.setPixelColor(x, y, QColor("#e76f51"));
    const QString file = dir.filePath("blob.png");
    QVERIFY(blob.save(file));
    QVERIFY(gesture("add blob part", 1, [&] { panel->importAndAddPart(file, "blob", "blob", "root"); }));
    const Json start = rig();
    panel->selectItem("slot:blob");
    find<QTabWidget>("cutoutLookTabs")->setCurrentIndex(3);
    RigCanvas* canvas = panel->canvas();
    canvas->fitToCanvas();
    pump();
    auto blobOf = [](const Json& data) { for (const auto& a : data["attachments"]) if (a["slot"] == "blob") return a; return Json(); };
    // Image pixel -> widget point of the part as shown (setup pose).
    auto widgetAt = [&](double x, double y) {
      const cutout::Rig parsed = cutout::parseRig("hero", rig());
      const cutout::Attachment* att = nullptr;
      const cutout::Affine m = attachmentMatrix(parsed, cutout::samplePose(parsed, -1, 0), parsed.slotIndex("blob"), &att);
      double rx, ry;
      m.apply(x, y, rx, ry);
      return canvas->rigToWidget(QPointF(rx, ry));
    };
    auto area = [](const Json& a) {
      double sum = 0;
      for (size_t t = 0; t + 2 < a["triangles"].size(); t += 3) {
        const auto& v = a["vertices"];
        const int i = a["triangles"][t], j = a["triangles"][t + 1], k = a["triangles"][t + 2];
        sum += std::fabs((v[2 * j].get<double>() - v[2 * i].get<double>()) * (v[2 * k + 1].get<double>() - v[2 * i + 1].get<double>()) -
                         (v[2 * j + 1].get<double>() - v[2 * i + 1].get<double>()) * (v[2 * k].get<double>() - v[2 * i].get<double>())) / 2;
      }
      return sum;
    };
    QVERIFY(find<QWidget>("cutoutMeshDrawOutline")->isEnabled() && find<QWidget>("cutoutMeshAutoOutline")->isEnabled());
    // Esc cancels a started outline: no edit.
    QVERIFY(gesture("outline: Esc cancels", 0, [&] {
      click("cutoutMeshDrawOutline");
      QVERIFY(canvas->outlineDrawing());
      tap(canvas, widgetAt(0, 0));
      tap(canvas, widgetAt(80, 0));
      QCOMPARE(canvas->outlineDraft().size(), 2);
      QTest::keyClick(canvas, Qt::Key_Escape);
    }));
    QVERIFY(!canvas->outlineDrawing() && blobOf(rig()).value("type", std::string()) != "mesh");
    // 윤곽 그리기: an L outline clicked point by point, closed by a double-click = ONE edit.
    const std::vector<QPointF> L = {{0, 0}, {80, 0}, {80, 30}, {40, 30}, {40, 60}, {0, 60}};
    QVERIFY(gesture("draw an outline", 1, [&] {
      click("cutoutMeshDrawOutline");
      for (const QPointF& p : L) tap(canvas, widgetAt(p.x(), p.y()));
      const QPointF last = widgetAt(L.back().x(), L.back().y());
      QMouseEvent dbl(QEvent::MouseButtonDblClick, last, canvas->mapToGlobal(last), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
      QCoreApplication::sendEvent(canvas, &dbl);
      mouse(canvas, QEvent::MouseButtonRelease, last, Qt::LeftButton, Qt::NoButton);
    }));
    Json a = blobOf(rig());
    QCOMPARE(qs(a["type"].get<std::string>()), QString("mesh"));
    QCOMPARE(a["hull"].get<int>(), 6);
    QCOMPARE(a["vertices"].size(), size_t(12));
    for (size_t i = 0; i < L.size(); i++)
      QVERIFY2(std::fabs(a["vertices"][2 * i].get<double>() - L[i].x()) < 0.2 && std::fabs(a["vertices"][2 * i + 1].get<double>() - L[i].y()) < 0.2, a["vertices"].dump().c_str());
    QVERIFY2(std::fabs(area(a) - 3600) < 20, qPrintable(QString::number(area(a))));
    QVERIFY(!canvas->outlineDrawing() && panel->meshEdit() == CutoutPanel::MeshEdit::Vertices);
    QVERIFY(find<QWidget>("cutoutMeshShapeTools")->isVisible() && !find<QWidget>("cutoutMeshWeightTools")->isVisible());
    undo();
    QVERIFY(blobOf(rig()).value("type", std::string()) != "mesh");
    redo();
    QCOMPARE(blobOf(rig())["hull"].get<int>(), 6);
    // 자동 윤곽 with the preview on: the dashed preview is what the edit makes; it hugs the ellipse.
    panel->selectItem("slot:blob");
    panel->setAutoOutlineParams(0.1, 0.6, 0, true);
    pump();
    const QPolygonF preview = canvas->outlinePreview();
    QVERIFY(preview.size() >= 8);
    QVERIFY(gesture("auto outline", 1, [&] { click("cutoutMeshAutoOutline"); }));
    a = blobOf(rig());
    const int hull = a["hull"].get<int>();
    QCOMPARE(hull, int(preview.size()));
    double x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9;
    for (int i = 0; i < hull; i++) {
      const double x = a["vertices"][2 * i].get<double>(), y = a["vertices"][2 * i + 1].get<double>();
      x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y);
    }
    QVERIFY2(x0 >= 0 && x0 < 3 && x1 > 77 && x1 <= 80 && y0 >= 0 && y0 < 3 && y1 > 57 && y1 <= 60, qPrintable(QString("%1 %2 %3 %4").arg(x0).arg(x1).arg(y0).arg(y1)));
    QVERIFY2(area(a) > 3770 && area(a) < 4800, qPrintable(QString::number(area(a))));  // between the ellipse and its box
    // Weights first, so the new vertices below must get interpolated weights.
    auto* bones = find<QListWidget>("cutoutMeshBones");
    for (int i = 0; i < bones->count(); i++) bones->item(i)->setCheckState(bones->item(i)->data(Qt::UserRole).toString() == "root" ? Qt::Checked : Qt::Unchecked);
    QVERIFY(gesture("auto weights", 1, [&] { QVERIFY(panel->autoWeights()); }));
    // 정점 추가: inside -> an interior vertex (selected), near the outline -> an outline vertex.
    QVERIFY(gesture("add-vertex tool", 0, [&] { click("cutoutMeshToolAddVertex"); }));
    QCOMPARE(canvas->meshTool(), int(RigCanvas::MeshAddVertex));
    const size_t before = blobOf(rig())["vertices"].size() / 2;
    QVERIFY(gesture("add interior vertex", 1, [&] { tap(canvas, widgetAt(40, 30)); }));
    a = blobOf(rig());
    QCOMPARE(a["vertices"].size() / 2, before + 1);
    QCOMPARE(panel->selectedVertices(), QVector<int>({int(before)}));
    QVERIFY(std::fabs(a["vertices"][2 * before].get<double>() - 40) < 0.5 && std::fabs(a["vertices"][2 * before + 1].get<double>() - 30) < 0.5);
    QCOMPARE(a["weights"].size(), before + 1);
    const QPointF h0 = widgetAt(a["vertices"][0].get<double>(), a["vertices"][1].get<double>()), h1 = widgetAt(a["vertices"][2].get<double>(), a["vertices"][3].get<double>());
    const QPointF mid = (h0 + h1) / 2, normal = QPointF(h1.y() - h0.y(), h0.x() - h1.x()) / QLineF(h0, h1).length();
    QVERIFY(gesture("add outline vertex", 1, [&] { tap(canvas, mid + normal * 2); }));
    a = blobOf(rig());
    QCOMPARE(a["hull"].get<int>(), hull + 1);
    QCOMPARE(panel->selectedVertices(), QVector<int>({1}));
    // 가장자리 추가: drag from outline vertex 0 to the interior vertex (now index before + 1).
    QVERIFY(gesture("add-edge tool", 0, [&] { click("cutoutMeshToolAddEdge"); }));
    const int centre = int(before) + 1;
    auto vertexWidget = [&](int v) { const Json m = blobOf(rig()); return widgetAt(m["vertices"][2 * v].get<double>(), m["vertices"][2 * v + 1].get<double>()); };
    const int far = (hull + 1) / 2;
    QVERIFY(gesture("pin an edge", 1, [&] { drag(canvas, vertexWidget(far), vertexWidget(centre)); }));
    auto pinnedEdge = [&](int x, int y) {
      const Json m = blobOf(rig());
      for (size_t i = 0; i + 1 < m["edges"].size(); i += 2) if (m["edges"][i] == std::min(x, y) && m["edges"][i + 1] == std::max(x, y)) return true;
      return false;
    };
    QVERIFY(pinnedEdge(far, centre));
    QVERIFY(panel->findChild<QLabel*>("cutoutMeshInfo")->text().contains("고정 가장자리 1"));
    if (const QString evidence = qEnvironmentVariable("CUTOUT_STUDIO_EVIDENCE"); !evidence.isEmpty())
      QVERIFY(panel->window()->grab().save(evidence + "/studio-mesh-freeform.png"));
    // 가장자리 삭제 on the two selected vertices.
    QVERIFY(gesture("select tool", 0, [&] { click("cutoutMeshToolSelect"); }));
    panel->selectVertices({far, centre});
    QVERIFY(gesture("unpin the edge", 1, [&] { click("cutoutMeshRemoveEdge"); }));
    QVERIFY(!pinnedEdge(far, centre));
    // Delete key removes the selected vertex (the interior one).
    panel->selectVertices({centre});
    const size_t count = blobOf(rig())["vertices"].size() / 2;
    QVERIFY(gesture("Delete vertex", 1, [&] { canvas->setFocus(); QTest::keyClick(canvas, Qt::Key_Delete); }));
    QCOMPARE(blobOf(rig())["vertices"].size() / 2, count - 1);
    QCOMPARE(blobOf(rig())["weights"].size(), count - 1);
    // 다시 삼각분할 after dragging an outline vertex across the mesh (its triangles fold).
    panel->selectVertices({far});
    QVERIFY(gesture("drag a vertex", 1, [&] { drag(canvas, vertexWidget(far), (vertexWidget(far) + vertexWidget(0)) / 2 + (vertexWidget(0) - vertexWidget(far)) * 0.05); }));
    const Json folded = blobOf(rig());
    QVERIFY(gesture("retriangulate", 1, [&] { click("cutoutMeshRetriangulate"); }));
    QVERIFY(blobOf(rig())["triangles"] != folded["triangles"] && blobOf(rig())["vertices"] == folded["vertices"]);
    // Refusals keep the documents unchanged (status explains).
    QVERIFY(gesture("unpin without a pinned edge", 0, [&] { panel->selectVertices({0, 1}); click("cutoutMeshRemoveEdge"); }));
    QVERIFY(panel->status().contains("윤곽"));
    // Undo walks every gesture back to the plain picture; redo replays the outline.
    for (int i = 0; i < 10; i++) undo();
    QVERIFY(rig() == start);
    redo();
    QCOMPARE(blobOf(rig())["hull"].get<int>(), 6);
    undo();
    QVERIFY(rig() == start);
    panel->selectItem("slot:blob");
    QVERIFY(gesture("remove the blob part", 1, [&] { QVERIFY(panel->deleteSelected()); }));
    panel->setMeshTool(RigCanvas::MeshSelect);
    panel->setAutoOutlineParams(0.05, 0.5, 0, false);
  }

  // Canvas hit priority (Spine-like, one hit test for click, drag and hover): handles — an IK
  // crosshair, bone origins, mesh vertices in vertex mode — beat the image parts under them; a part
  // away from handles still drags; Alt+click walks the stack; editor-hidden parts are never picked.
  void canvasHitPriority() {
    panel->selectRig("hero");
    find<QTabWidget>("cutoutBottomTabs")->setCurrentIndex(0);
    panel->setMode(CutoutPanel::Mode::Setup);
    panel->setPreviewSkin("");
    panel->setPreviewExpression("");
    panel->setMeshEdit(CutoutPanel::MeshEdit::Off);
    panel->setTool(RigCanvas::Move);
    pump();
    RigCanvas* canvas = panel->canvas();
    const Json start = rig();
    const int editsBefore = host.edits;
    const double zoom = canvas->rigToWidget(QPointF(1, 0)).x() - canvas->rigToWidget(QPointF(0, 0)).x();
    auto attachmentOf = [&](const char* slot) { const Json data = rig(); for (const auto& a : data["attachments"]) if (a["slot"] == slot) return a; return Json(); };
    auto bonesOf = [&] { return rig()["bones"]; };
    auto refsAt = [&](QPointF at) { QStringList refs; for (const auto& h : canvas->hitCandidates(at)) if (h.vertex < 0 && !refs.contains(h.ref)) refs << h.ref; return refs; };
    auto hover = [&](QPointF at, Qt::KeyboardModifiers mods = Qt::NoModifier) {
      QMouseEvent e(QEvent::MouseMove, at, canvas->mapToGlobal(at), Qt::NoButton, Qt::NoButton, mods);
      QCoreApplication::sendEvent(canvas, &e);
      return canvas->hoverRef();
    };
    // Setup x/y of a bone so its origin lands on `at` (rig space): one property edit per axis.
    auto place = [&](const std::string& bone, QPointF at) {
      std::string parent;
      const Json data = rig();
      for (const auto& b : data["bones"]) if (b["id"] == bone && b.contains("parent")) parent = b["parent"].get<std::string>();
      double lx = at.x(), ly = at.y();
      if (!parent.empty()) cutout::setupWorld(data).at(parent).inverse()->apply(at.x(), at.y(), lx, ly);
      panel->selectItem(qs("bone:" + bone));
      pump();
      // One gesture each: the second commit must see the first one's result.
      if (!gesture("place x", 1, [&] { setSpin("cutoutPropX", lx); }) || !gesture("place y", 1, [&] { setSpin("cutoutPropY", ly); })) return false;
      const bool ok = std::hypot(world(bone).x() - at.x(), world(bone).y() - at.y()) < 0.05;
      if (!ok) qWarning("place %s at %g,%g (local %g,%g): now %g,%g (status %s)", bone.c_str(), at.x(), at.y(), lx, ly, world(bone).x(), world(bone).y(), qPrintable(panel->status()));
      return ok;
    };
    QPointF center;
    {
      const cutout::Rig r = cutout::parseRig("hero", rig());
      const cutout::Pose pose = cutout::samplePose(r, -1, 0);
      const cutout::Attachment* att = nullptr;
      const auto m = attachmentMatrix(r, pose, r.slotIndex("body"), &att);
      QVERIFY(att);
      center = canvas->rigToWidget(qtTransform(m).map(QPointF(att->width / 2, att->height / 2)));  // widget px
    }
    // A hand bone over the body with an aim IK; its crosshair also sits on the body.
    panel->selectItem("bone:root");
    answers = {"hit-hand"};
    QVERIFY(gesture("add hand bone", 1, [&] { click("cutoutAddBone"); }));
    QVERIFY(place("hit-hand", canvas->widgetToRig(center + QPointF(0, -18))));
    panel->selectItem("bone:hit-hand");
    QVERIFY(gesture("aim IK on the hand", 1, [&] { click("cutoutNewAimIk"); }));
    std::string target;
    {
      const Json data = rig();
      for (const auto& k : data["ik"]) if (k["bones"] == Json::array({"hit-hand"})) target = k["target"].get<std::string>();
    }
    QVERIFY(!target.empty());
    const QString targetRef = qs("bone:" + target);
    QVERIFY(place(target, canvas->widgetToRig(center + QPointF(0, 14))));

    // (4) Alt+click walks the stack under the cursor (crosshair -> body -> ... -> crosshair);
    // a plain click picks the handle again. Clicks select only: no edits.
    panel->selectItem("");
    pump();
    const QPointF stack = canvas->rigToWidget(world(target));
    const QStringList refs = refsAt(stack);
    QVERIFY2(refs.size() >= 2 && refs[0] == targetRef && refs.contains("slot:body"), qPrintable(refs.join(", ")));
    QCOMPARE(canvas->pick(stack, false), targetRef);
    QCOMPARE(hover(stack, Qt::AltModifier), refs[1]);  // the Alt pick is previewed too
    for (int i = 1; i <= refs.size(); i++) {
      QVERIFY(gesture("alt-click cycles", 0, [&] { tap(canvas, stack, Qt::AltModifier); }));
      QCOMPARE(panel->selection(), refs[i % refs.size()]);
    }
    QVERIFY(gesture("alt-click once more", 0, [&] { tap(canvas, stack, Qt::AltModifier); }));
    QCOMPARE(panel->selection(), refs[1 % refs.size()]);
    QVERIFY(gesture("plain click", 0, [&] { tap(canvas, stack); }));
    QCOMPARE(panel->selection(), targetRef);
    // A point on the body away from every handle (only the body under it; handles move below).
    auto findBodyOnly = [&] {
      for (int dy = -36; dy <= 36; dy += 4)
        for (int dx = -16; dx <= 16; dx += 4)
          if (const auto hits = canvas->hitCandidates(center + QPointF(dx, dy)); hits.size() == 1 && hits[0].ref == "slot:body") return center + QPointF(dx, dy);
      return QPointF(-1, -1);
    };
    QPointF bodyOnly = findBodyOnly();
    QVERIFY(bodyOnly.x() >= 0);
    // (5) Editor-hidden parts (eye toggle) are never picked, hovered or cycled to.
    auto setEye = [&](bool visible) {
      for (QTreeWidgetItemIterator it(find<QTreeWidget>("cutoutBoneTree")); *it; ++it)
        if ((*it)->data(0, Qt::UserRole).toString() == "slot:body") { (*it)->setCheckState(1, visible ? Qt::Checked : Qt::Unchecked); break; }
      pump();
    };
    setEye(false);
    QVERIFY(canvas->hitCandidates(bodyOnly).isEmpty());
    QVERIFY(!refsAt(stack).contains("slot:body"));
    QCOMPARE(hover(bodyOnly), QString());
    panel->selectItem("");
    QVERIFY(gesture("click a hidden part", 0, [&] { tap(canvas, bodyOnly); }));
    QCOMPARE(panel->selection(), QString());
    QVERIFY(gesture("alt-click over a hidden part", 0, [&] { tap(canvas, stack, Qt::AltModifier); tap(canvas, stack, Qt::AltModifier); }));
    QVERIFY(panel->selection() != "slot:body");
    setEye(true);
    // (1) The reported case: target NOT selected, press on the crosshair's ring (9 px from its
    // centre, over the body image) and drag: the target moves, one edit, the body is untouched.
    panel->selectItem("");
    pump();
    const QPointF ring = canvas->rigToWidget(world(target)) + QPointF(9, 0);
    QCOMPARE(canvas->hitTest(ring, false), QString("slot:body"));  // the image is right under it
    QCOMPARE(canvas->hitTest(ring), targetRef);
    QCOMPARE(canvas->hitCandidates(ring).first().rank, int(RigCanvas::HitIkTarget));
    QCOMPARE(hover(ring), targetRef);  // hover shows what the press will pick
    {
      const Json body = attachmentOf("body");
      const QPointF before = world(target);
      QVERIFY(gesture("drag the unselected crosshair over the body", 1, [&] { drag(canvas, ring, ring + QPointF(24, 16)); }));
      QVERIFY(attachmentOf("body") == body);
      QCOMPARE(panel->selection(), targetRef);
      const QPointF moved = world(target) - before;
      QVERIFY2(std::hypot(moved.x() - 24 / zoom, moved.y() - 16 / zoom) < 1, qPrintable(QString("%1,%2").arg(moved.x()).arg(moved.y())));
    }
    // (3) A bone origin handle beats the part under it (5 px off the origin).
    panel->selectItem("");
    pump();
    {
      const QPointF at = canvas->rigToWidget(world("hit-hand")) + QPointF(4, 3);
      QCOMPARE(canvas->hitTest(at, false), QString("slot:body"));
      QCOMPARE(canvas->hitTest(at), QString("bone:hit-hand"));
      const Json body = attachmentOf("body");
      const QPointF before = world("hit-hand");
      QVERIFY(gesture("drag the bone origin over the body", 1, [&] { drag(canvas, at, at + QPointF(0, -12)); }));
      QVERIFY(attachmentOf("body") == body);
      QCOMPARE(panel->selection(), QString("bone:hit-hand"));
      QVERIFY(std::fabs(world("hit-hand").y() - before.y() + 12 / zoom) < 1);
    }
    bodyOnly = findBodyOnly();
    QVERIFY(bodyOnly.x() >= 0);
    // (2) A press on the part away from handles still selects and drags the part.
    panel->selectItem("");
    pump();
    QCOMPARE(hover(bodyOnly), QString("slot:body"));
    {
      const Json bones = bonesOf();
      const double x0 = attachmentOf("body").value("x", 0.0);
      QVERIFY(gesture("drag the part", 1, [&] { drag(canvas, bodyOnly, bodyOnly + QPointF(20, 0)); }));
      QCOMPARE(panel->selection(), QString("slot:body"));
      QVERIFY(attachmentOf("body").value("x", 0.0) > x0 + 5);
      QVERIFY(bonesOf() == bones);
    }
    // (6) Mesh vertex mode: a vertex wins over a bone handle and the mesh part on the same spot.
    QPointF vertex;
    {
      panel->selectItem("slot:bar");
      panel->setMeshEdit(CutoutPanel::MeshEdit::Vertices);
      pump();
      const QVector<QPointF> pts = canvas->meshVertices();
      QVERIFY(pts.size() > 5);
      panel->setMeshEdit(CutoutPanel::MeshEdit::Off);
      QVERIFY(place("mesh-lower", pts[5]));  // the unweighted mesh only follows mesh-upper: it stays put
      vertex = canvas->rigToWidget(pts[5]);
    }
    panel->selectItem("slot:bar");
    panel->setMeshEdit(CutoutPanel::MeshEdit::Vertices);
    pump();
    QCOMPARE(canvas->meshMode(), int(RigCanvas::MeshVertices));
    {
      const auto hits = canvas->hitCandidates(vertex);
      QVERIFY(!hits.isEmpty() && hits[0].vertex == 5 && hits[0].rank == RigCanvas::HitVertex);
      QVERIFY(std::any_of(hits.begin(), hits.end(), [](const RigCanvas::Hit& h) { return h.ref == "bone:mesh-lower"; }));
    }
    QVERIFY(gesture("vertex beats bone handle", 0, [&] { tap(canvas, vertex); }));
    QCOMPARE(panel->selectedVertices(), QVector<int>({5}));
    QCOMPARE(panel->selection(), QString("slot:bar"));
    panel->setMeshEdit(CutoutPanel::MeshEdit::Off);
    pump();
    QCOMPARE(canvas->hitTest(vertex), QString("bone:mesh-lower"));  // out of vertex mode the handle wins
    for (int i = host.edits - editsBefore; i > 0; i--) undo();
    QVERIFY(rig() == start);
  }

  // Bone length (plugin 1.5.0), Spine-like: the create tool draws bones (one edit, length and
  // rotation in the parent's space), a bone's body turns it and its origin moves it (Move and
  // Rotate tools), bodies rank between handles and parts, 자동 길이 / 본 방향 맞추기 are one edit
  // each and never move a drawn pixel, the length field is setup-only. Everything undoes.
  void boneLengthCreateAndPose() {
    panel->selectRig("hero");
    find<QTabWidget>("cutoutBottomTabs")->setCurrentIndex(0);
    panel->setMode(CutoutPanel::Mode::Setup);
    panel->setPreviewSkin("");
    panel->setPreviewExpression("");
    panel->setMeshEdit(CutoutPanel::MeshEdit::Off);
    pump();
    RigCanvas* canvas = panel->canvas();
    const Json start = rig();
    const int editsBefore = host.edits;
    constexpr double PI = 3.14159265358979323846;
    auto boneOf = [&](const QString& id) { const Json data = rig(); for (const auto& b : data["bones"]) if (b["id"] == ss(id)) return b; return Json(); };
    auto tipOf = [&](const QString& id) {
      const auto w = cutout::setupWorld(rig()).at(ss(id));
      double x, y;
      w.apply(boneOf(id).value("length", 0.0), 0, x, y);
      return QPointF(x, y);
    };
    auto newBone = [&](const Json& before) {
      const Json data = rig();
      for (const auto& b : data["bones"]) {
        bool old = false;
        for (const auto& o : before["bones"]) old = old || o["id"] == b["id"];
        if (!old) return qs(b["id"].get<std::string>());
      }
      return QString();
    };
    auto shiftDrag = [&](QPointF from, QPointF to) {
      auto send = [&](QEvent::Type type, QPointF at, Qt::MouseButton b, Qt::MouseButtons bs) {
        QMouseEvent e(type, at, canvas->mapToGlobal(at), b, bs, Qt::ShiftModifier);
        QCoreApplication::sendEvent(canvas, &e);
      };
      send(QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
      for (int i = 1; i <= 4; i++) send(QEvent::MouseMove, from + (to - from) * (i / 4.0), Qt::NoButton, Qt::LeftButton);
      send(QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
    };
    // A widget point near `around` where no bone origin is pickable (create presses there draw).
    auto freeSpot = [&](QPointF around) {
      for (int r = 0; r <= 60; r += 6)
        for (int a = 0; a < 8; a++) {
          const QPointF at = around + QPointF(std::cos(a * PI / 4), std::sin(a * PI / 4)) * r;
          bool handle = false;
          for (const auto& h : canvas->hitCandidates(at)) handle = handle || (h.ref.startsWith("bone:") && !h.body);
          if (!handle && canvas->rect().contains(at.toPoint())) return at;
        }
      return QPointF(-1, -1);
    };

    // (1) Create tool (button + C): a drag from a free point under a rotated parent. The bone is a
    // child of the selection; the press is its origin and the release its tip, in any parent space.
    click("cutoutToolCreate");
    QCOMPARE(panel->tool(), int(RigCanvas::Create));
    panel->selectItem("bone:arm");
    pump();
    const double armRotation = boneOf("arm").value("rotation", 0.0);
    QVERIFY(std::fabs(armRotation) > 1);  // rotated by the property test: compensation is visible
    const QPointF press = freeSpot(canvas->rigToWidget(world("arm") + QPointF(40, 30)));
    QVERIFY(press.x() >= 0);
    const QPointF release = press + QPointF(70, -25);
    Json before = rig();
    QVERIFY(gesture("create tool drag", 1, [&] { drag(canvas, press, release); }));
    const QString made = newBone(before);
    QVERIFY(!made.isEmpty());
    QCOMPARE(panel->selection(), "bone:" + made);
    {
      const Json b = boneOf(made);
      QCOMPARE(qs(b["parent"].get<std::string>()), QString("arm"));
      // Expected in the parent's space: origin = inverse(arm world) · press, direction/length of the drag vector.
      const auto inv = *cutout::setupWorld(before).at("arm").inverse();
      const QPointF a = canvas->widgetToRig(press), z = canvas->widgetToRig(release);
      double ax, ay, zx, zy;
      inv.apply(a.x(), a.y(), ax, ay);
      inv.apply(z.x(), z.y(), zx, zy);
      QVERIFY2(std::fabs(b["x"].get<double>() - ax) < 1e-3 && std::fabs(b["y"].get<double>() - ay) < 1e-3, qPrintable(qs(b.dump())));
      QVERIFY(std::fabs(b.value("rotation", 0.0) - std::atan2(zy - ay, zx - ax) * 180 / PI) < 1e-3);
      QVERIFY(std::fabs(b["length"].get<double>() - std::hypot(zx - ax, zy - ay)) < 1e-3);
      const QPointF tip = tipOf(made);
      QVERIFY(std::hypot(tip.x() - z.x(), tip.y() - z.y()) < 1e-3);  // the tip is where the drag ended
      QVERIFY(!canvas->boneShape(cutout::parseRig("h", rig()).boneIndex(ss(made))).isEmpty());  // drawn as a body
    }
    // Chaining: the new bone is selected, so a drag from its tip draws its child.
    const QString first = made;
    const QPointF chainFrom = canvas->rigToWidget(tipOf(first));
    before = rig();
    QVERIFY(gesture("create the next link from the tip (Shift: 15°)", 1, [&] { shiftDrag(chainFrom, chainFrom + QPointF(60, 22)); }));
    const QString second = newBone(before);
    QCOMPARE(qs(boneOf(second)["parent"].get<std::string>()), first);
    {
      // Shift snaps the on-screen angle (atan2(22, 60) ≈ 20°) to 15°.
      const QPointF o = world(ss(second)), t = tipOf(second);
      QVERIFY(std::fabs(std::atan2(t.y() - o.y(), t.x() - o.x()) * 180 / PI - 15) < 1e-3);
      QVERIFY(std::hypot(o.x() - tipOf(first).x(), o.y() - tipOf(first).y()) < 1e-3);
    }
    // A click without a drag makes a point bone; a press on a bone origin only picks the parent.
    const QPointF clickAt = freeSpot(chainFrom + QPointF(-30, 60));
    QVERIFY(clickAt.x() >= 0);
    before = rig();
    QVERIFY(gesture("create tool click", 1, [&] { tap(canvas, clickAt); }));
    const QString point = newBone(before);
    QVERIFY(!point.isEmpty() && !boneOf(point).contains("length") && !boneOf(point).contains("rotation"));
    QCOMPARE(qs(boneOf(point)["parent"].get<std::string>()), second);
    QVERIFY(gesture("create tool on a bone origin selects the parent", 0, [&] { tap(canvas, canvas->rigToWidget(world(ss(second)))); }));
    QCOMPARE(panel->selection(), "bone:" + second);
    undo();
    QVERIFY(boneOf(point).is_null());
    redo();
    QVERIFY(!boneOf(point).is_null());
    // Animate mode: no creating (the tool is refused, the button disabled).
    panel->setMode(CutoutPanel::Mode::Animate);
    QCOMPARE(panel->tool(), int(RigCanvas::Move));
    panel->setTool(RigCanvas::Create);
    QCOMPARE(panel->tool(), int(RigCanvas::Move));
    QVERIFY(!find<QToolButton>("cutoutToolCreate")->isEnabled());
    panel->setMode(CutoutPanel::Mode::Setup);

    // (2) Hit priority with bodies: handles > bone bodies > parts, Alt walks body -> part.
    panel->setTool(RigCanvas::Move);
    panel->selectItem("");
    pump();
    const int firstIndex = cutout::parseRig("h", rig()).boneIndex(ss(first));
    const QPointF origin = canvas->rigToWidget(world(ss(first))), tip = canvas->rigToWidget(tipOf(first));
    const QPointF mid = origin + (tip - origin) * 0.55;
    {
      const auto hits = canvas->hitCandidates(mid);
      QVERIFY(!hits.isEmpty() && hits[0].ref == "bone:" + first && hits[0].rank == RigCanvas::HitBoneBody && hits[0].body);
      QVERIFY(canvas->boneShape(firstIndex).containsPoint(mid, Qt::OddEvenFill));
      const auto atOrigin = canvas->hitCandidates(origin + QPointF(2, 1));
      QVERIFY(!atOrigin.isEmpty() && atOrigin[0].rank < RigCanvas::HitBoneBody && !atOrigin[0].body);  // the origin handle first
    }
    // A bone drawn across the body image: at its middle the bone body beats the part, Alt reaches the part.
    QPointF overPart(-1, -1);
    {
      const cutout::Rig r = cutout::parseRig("hero", rig());
      const cutout::Pose pose = cutout::samplePose(r, -1, 0);
      const cutout::Attachment* att = nullptr;
      const auto m = attachmentMatrix(r, pose, r.slotIndex("body"), &att);
      QVERIFY(att);
      const QPointF centre = canvas->rigToWidget(qtTransform(m).map(QPointF(att->width / 2, att->height / 2)));
      for (int dy = -30; dy <= 30 && overPart.x() < 0; dy += 3)
        for (int dx = -14; dx <= 14 && overPart.x() < 0; dx += 3)
          if (const auto h = canvas->hitCandidates(centre + QPointF(dx, dy)); h.size() == 1 && h[0].ref == "slot:body") overPart = centre + QPointF(dx, dy);
    }
    QVERIFY(overPart.x() >= 0);
    panel->setTool(RigCanvas::Create);
    panel->selectItem("");
    pump();
    const QPointF across = freeSpot(overPart - QPointF(0, 26));
    QVERIFY(across.x() >= 0);
    before = rig();
    QVERIFY(gesture("draw a root bone across the body image", 1, [&] { drag(canvas, across, across + (overPart - across) * 2); }));
    const QString overBone = newBone(before);
    QVERIFY(!overBone.isEmpty());
    panel->setTool(RigCanvas::Move);
    panel->selectItem("");
    pump();
    {
      QStringList refs;
      for (const auto& h : canvas->hitCandidates(overPart)) if (!refs.contains(h.ref)) refs << h.ref;
      QVERIFY2(refs.size() >= 2 && refs[0] == "bone:" + overBone && refs.contains("slot:body"), qPrintable(refs.join(", ")));
      QVERIFY(gesture("click the bone body over the part", 0, [&] { tap(canvas, overPart); }));
      QCOMPARE(panel->selection(), "bone:" + overBone);
      QVERIFY(gesture("alt-click reaches the part", 0, [&] { tap(canvas, overPart, Qt::AltModifier); }));
      QCOMPARE(panel->selection(), refs[1]);
    }

    // (3) Move tool: dragging the body turns the bone around its origin; the origin moves it.
    panel->selectItem("");
    pump();
    {
      const Json b0 = boneOf(first);
      const QPointF from = mid, pivot = origin;
      const double turn = 25 * PI / 180;
      const QPointF v = from - pivot;
      const QPointF to = pivot + QPointF(v.x() * std::cos(turn) - v.y() * std::sin(turn), v.x() * std::sin(turn) + v.y() * std::cos(turn));
      QVERIFY(gesture("drag the bone body (Move tool) turns it", 1, [&] { drag(canvas, from, to); }));
      const Json b1 = boneOf(first);
      QCOMPARE(panel->selection(), "bone:" + first);
      QVERIFY2(std::fabs(b1.value("rotation", 0.0) - b0.value("rotation", 0.0) - 25) < 0.5, qPrintable(qs(b1.dump())));
      QVERIFY(b1["x"] == b0["x"] && b1["y"] == b0["y"] && b1["length"] == b0["length"]);
      const QPointF at = canvas->rigToWidget(world(ss(first))) + QPointF(2, 2);
      QVERIFY(gesture("drag the origin moves it", 1, [&] { drag(canvas, at, at + QPointF(16, 10)); }));
      const Json b2 = boneOf(first);
      QVERIFY(b2.value("rotation", 0.0) == b1.value("rotation", 0.0) && (b2["x"] != b1["x"] || b2["y"] != b1["y"]));
      // Rotate tool: the same split (origin = move, body = turn).
      panel->setTool(RigCanvas::Rotate);
      const QPointF at2 = canvas->rigToWidget(world(ss(first))) + QPointF(1, 2);
      QVERIFY(gesture("rotate tool: origin drag moves", 1, [&] { drag(canvas, at2, at2 + QPointF(-12, 6)); }));
      const Json b3 = boneOf(first);
      QVERIFY(b3.value("rotation", 0.0) == b2.value("rotation", 0.0) && (b3["x"] != b2["x"] || b3["y"] != b2["y"]));
      const QPointF o3 = canvas->rigToWidget(world(ss(first))), t3 = canvas->rigToWidget(tipOf(first)), m3 = o3 + (t3 - o3) * 0.6;
      const QPointF w3 = m3 - o3, to3 = o3 + QPointF(w3.x() * std::cos(-turn) - w3.y() * std::sin(-turn), w3.x() * std::sin(-turn) + w3.y() * std::cos(-turn));
      QVERIFY(gesture("rotate tool: body drag turns", 1, [&] { drag(canvas, m3, to3); }));
      QVERIFY(std::fabs(boneOf(first).value("rotation", 0.0) - b3.value("rotation", 0.0) + 25) < 0.5);
      undo();
      QVERIFY(boneOf(first) == b3);
      redo();
      panel->setTool(RigCanvas::Move);
    }

    // (4) The length field (setup only) and the helpers: one edit each, nothing drawn moves.
    panel->selectItem("bone:" + point);
    pump();
    QVERIFY(find<QDoubleSpinBox>("cutoutPropLength")->isEnabled());
    QVERIFY(gesture("length field", 1, [&] { setSpin("cutoutPropLength", 33); }));
    QCOMPARE(boneOf(point)["length"].get<double>(), 33.0);
    panel->selectItem("bone:" + first);
    pump();
    QVERIFY(gesture("shorten by the field", 1, [&] { setSpin("cutoutPropLength", 12); }));
    QCOMPARE(boneOf(first)["length"].get<double>(), 12.0);
    {
      const auto drawBefore = drawList();
      QVERIFY(gesture("자동 길이 on a bone with a child", 1, [&] { click("cutoutFitBoneLengths"); }));
      const QPointF o = world(ss(first)), c = world(ss(second));
      QVERIFY(std::fabs(boneOf(first)["length"].get<double>() - std::hypot(c.x() - o.x(), c.y() - o.y())) < 1e-3);  // unscaled chain
      const auto drawAfter = drawList();
      QCOMPARE(drawAfter.size(), drawBefore.size());
      for (size_t i = 0; i < drawAfter.size(); i++) QVERIFY(std::fabs(drawAfter[i].matrix.e - drawBefore[i].matrix.e) < 1e-9);
    }
    panel->selectItem("");
    pump();
    {
      // Every clip at a few times: the migration keeps the animation (draw lists within 1e-6).
      const Json data0 = rig();
      auto sampled = [&](const Json& data) {
        const auto r = std::make_shared<const cutout::Rig>(cutout::parseRig("hero", data));
        std::vector<cutout::DrawItem> all;
        cutout::Instance setup(r);
        for (const auto& x : setup.drawList()) all.push_back(x);
        for (const auto& c : r->clips)
          for (double f : {0.0, 0.25, 0.5, 0.8, 1.0}) {
            cutout::Instance inst(r);
            inst.play(0, c.id, false);
            inst.seek(0, f * double(c.durationMs));
            for (const auto& x : inst.drawList()) all.push_back(x);
          }
        return all;
      };
      const auto a = sampled(data0);
      int lengthsBefore = 0;
      for (const auto& x : data0["bones"]) lengthsBefore += x.contains("length");
      QVERIFY(gesture("본 방향 맞추기 (all bones)", 1, [&] { click("cutoutOrientBones"); }));
      QVERIFY(panel->status().contains("방향"));
      const auto b = sampled(rig());
      QCOMPARE(b.size(), a.size());
      double worst = 0;
      for (size_t i = 0; i < a.size(); i++)
        for (double d : {a[i].matrix.a - b[i].matrix.a, a[i].matrix.b - b[i].matrix.b, a[i].matrix.c - b[i].matrix.c, a[i].matrix.d - b[i].matrix.d, a[i].matrix.e - b[i].matrix.e, a[i].matrix.f - b[i].matrix.f})
          worst = std::max(worst, std::fabs(d));
      QVERIFY2(worst < 1e-6, qPrintable(QString::number(worst)));
      int lengths = 0;
      const Json oriented = rig();  // (a temporary's subobject would dangle in the range-for)
      for (const auto& x : oriented["bones"]) lengths += x.contains("length");
      QVERIFY2(lengths > lengthsBefore, qPrintable(panel->status()));
      undo();
      QVERIFY(rig() == data0);
      redo();
    }
    // Animate mode: the length is not a key.
    panel->setMode(CutoutPanel::Mode::Animate);
    panel->selectItem("bone:" + first);
    pump();
    QVERIFY(!find<QDoubleSpinBox>("cutoutPropLength")->isEnabled());
    QVERIFY(gesture("helpers refused while animating", 0, [&] { click("cutoutFitBoneLengths"); click("cutoutOrientBones"); }));
    panel->setMode(CutoutPanel::Mode::Setup);
    if (const QString evidence = qEnvironmentVariable("CUTOUT_STUDIO_EVIDENCE"); !evidence.isEmpty())
      QVERIFY(panel->window()->grab().save(evidence + "/studio-bone-length.png"));
    for (int i = host.edits - editsBefore; i > 0; i--) undo();
    QVERIFY(rig() == start);
    panel->setTool(RigCanvas::Move);
  }

  // Multi-selection (plugin 1.7.0): Ctrl/Shift/box selection shared by canvas, tree, properties
  // and dopesheet; one transformBones edit per multi-bone drag (own origins or the centroid),
  // compensation toggles, mixed/relative/world fields, multi K and pose copy, undo/redo.
  void multiSelectionAndCompensation() {
    panel->selectRig("hero");
    find<QTabWidget>("cutoutBottomTabs")->setCurrentIndex(0);
    panel->setMode(CutoutPanel::Mode::Setup);
    panel->setPreviewSkin("");
    panel->setPreviewExpression("");
    panel->setMeshEdit(CutoutPanel::MeshEdit::Off);
    panel->setTool(RigCanvas::Move);
    pump();
    RigCanvas* canvas = panel->canvas();
    auto* tree = find<QTreeWidget>("cutoutBoneTree");
    canvas->fitToCanvas();
    pump();
    const Json start = rig();
    const int editsBefore = host.edits;
    constexpr double PI = 3.14159265358979323846;
    auto boneOf = [&](const char* id) { const Json data = rig(); for (const auto& b : data["bones"]) if (b["id"] == id) return b; return Json(); };
    auto widgetOf = [&](const char* id) { return canvas->rigToWidget(world(id)); };
    auto press = [&](QPointF at, Qt::KeyboardModifiers mods) {
      QMouseEvent p(QEvent::MouseButtonPress, at, canvas->mapToGlobal(at), Qt::LeftButton, Qt::LeftButton, mods);
      QCoreApplication::sendEvent(canvas, &p);
      QMouseEvent r(QEvent::MouseButtonRelease, at, canvas->mapToGlobal(at), Qt::LeftButton, Qt::NoButton, mods);
      QCoreApplication::sendEvent(canvas, &r);
    };
    auto boxDrag = [&](QPointF from, QPointF to, Qt::KeyboardModifiers mods) {
      QMouseEvent p(QEvent::MouseButtonPress, from, canvas->mapToGlobal(from), Qt::LeftButton, Qt::LeftButton, mods);
      QCoreApplication::sendEvent(canvas, &p);
      for (int i = 1; i <= 4; i++) {
        const QPointF at = from + (to - from) * (i / 4.0);
        QMouseEvent m(QEvent::MouseMove, at, canvas->mapToGlobal(at), Qt::NoButton, Qt::LeftButton, mods);
        QCoreApplication::sendEvent(canvas, &m);
      }
      QVERIFY(canvas->boxSelecting());
      QMouseEvent r(QEvent::MouseButtonRelease, to, canvas->mapToGlobal(to), Qt::LeftButton, Qt::NoButton, mods);
      QCoreApplication::sendEvent(canvas, &r);
    };
    auto empty = [&](QPointF at) { return canvas->hitCandidates(at).isEmpty() && canvas->rect().contains(at.toPoint()); };
    // An arc of `degrees` around `center` (widget px) starting where nothing is pickable.
    auto arc = [&](QPointF center, double degrees) {
      double a0 = 0;
      for (int k = 0; k < 24; k++) {
        a0 = k * PI / 12;
        bool ok = true;
        for (int i = 0; i <= 8 && ok; i++) {
          const double a = a0 + i * degrees * PI / 180 / 8;
          ok = canvas->rect().contains((center + QPointF(std::cos(a), std::sin(a)) * 70).toPoint());
        }
        if (ok && empty(center + QPointF(std::cos(a0), std::sin(a0)) * 70)) break;
      }
      const QPointF from = center + QPointF(std::cos(a0), std::sin(a0)) * 70;
      mouse(canvas, QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
      for (int i = 1; i <= 8; i++) {
        const double a = a0 + i * degrees * PI / 180 / 8;
        mouse(canvas, QEvent::MouseMove, center + QPointF(std::cos(a), std::sin(a)) * 70, Qt::NoButton, Qt::LeftButton);
      }
      const double a = a0 + degrees * PI / 180;
      mouse(canvas, QEvent::MouseButtonRelease, center + QPointF(std::cos(a), std::sin(a)) * 70, Qt::NoButton, Qt::NoButton);
    };
    auto treeClick = [&](const QString& ref, Qt::KeyboardModifiers mods) {
      for (QTreeWidgetItemIterator it(tree); *it; ++it)
        if ((*it)->data(0, Qt::UserRole).toString() == ref) {
          tree->scrollToItem(*it);
          const QRect r = tree->visualItemRect(*it);
          QTest::mouseClick(tree->viewport(), Qt::LeftButton, mods, QPoint(r.left() + 16, r.center().y()));
          return;
        }
      QFAIL(qPrintable("no tree item " + ref));
    };
    auto treeSelected = [&] {
      QStringList refs;
      for (auto* item : tree->selectedItems()) refs << item->data(0, Qt::UserRole).toString();
      std::sort(refs.begin(), refs.end());
      return refs;
    };
    auto sorted = [](QStringList l) { std::sort(l.begin(), l.end()); return l; };

    // Three bones away from the character: ma (with child mb) and mc, all under root.
    panel->selectItem("bone:root");
    answers = {"ma"};
    QVERIFY(gesture("add ma", 1, [&] { click("cutoutAddBone"); }));
    QVERIFY(gesture("ma x", 1, [&] { setSpin("cutoutPropX", -150); }));
    QVERIFY(gesture("ma y", 1, [&] { setSpin("cutoutPropY", -170); }));
    QVERIFY(gesture("ma rotation", 1, [&] { setSpin("cutoutPropRotation", 20); }));
    answers = {"mb"};
    QVERIFY(gesture("add mb", 1, [&] { click("cutoutAddBone"); }));
    QVERIFY(gesture("mb x", 1, [&] { setSpin("cutoutPropX", 60); }));
    QVERIFY(gesture("mb y", 1, [&] { setSpin("cutoutPropY", 0); }));
    panel->selectItem("bone:root");
    answers = {"mc"};
    QVERIFY(gesture("add mc", 1, [&] { click("cutoutAddBone"); }));
    QVERIFY(gesture("mc x", 1, [&] { setSpin("cutoutPropX", 150); }));
    QVERIFY(gesture("mc y", 1, [&] { setSpin("cutoutPropY", -170); }));
    canvas->fitToCanvas();
    pump();
    for (const char* id : {"ma", "mb", "mc"})
      QVERIFY2(canvas->pick(widgetOf(id), false) == QString("bone:") + id, qPrintable(QString(id) + " not pickable at its origin: " + canvas->pick(widgetOf(id), false)));

    // (1) Canvas Ctrl/Shift+click, tree Ctrl+click: one selection everywhere, no edits.
    QVERIFY(gesture("click + ctrl/shift click", 0, [&] {
      press(widgetOf("ma"), Qt::NoModifier);
      press(widgetOf("mc"), Qt::ControlModifier);
    }));
    QCOMPARE(sorted(panel->selectedItems()), QStringList({"bone:ma", "bone:mc"}));
    QCOMPARE(panel->selection(), QString("bone:mc"));
    QCOMPARE(treeSelected(), QStringList({"bone:ma", "bone:mc"}));
    QVERIFY(canvas->isSelected("bone:ma") && canvas->isSelected("bone:mc") && !canvas->isSelected("bone:mb"));
    QVERIFY(panel->dopesheet()->highlightTargets().contains("bone:ma") && panel->dopesheet()->highlightTargets().contains("bone:mc"));
    QVERIFY(find<QLabel>("cutoutSelectionLabel")->text().startsWith("본 2개"));
    treeClick("bone:mb", Qt::ControlModifier);
    QCOMPARE(sorted(panel->selectedItems()), QStringList({"bone:ma", "bone:mb", "bone:mc"}));
    QCOMPARE(panel->selection(), QString("bone:mb"));
    QVERIFY(canvas->isSelected("bone:mb"));
    press(widgetOf("mb"), Qt::ShiftModifier);  // Shift+click toggles it off again
    QCOMPARE(sorted(panel->selectedItems()), QStringList({"bone:ma", "bone:mc"}));
    QCOMPARE(treeSelected(), QStringList({"bone:ma", "bone:mc"}));
    press(widgetOf("ma"), Qt::NoModifier);  // a plain click on a member keeps only it
    QCOMPARE(panel->selectedItems(), QStringList({"bone:ma"}));

    // (2) Box selection (Move tool, drag on empty space); Shift adds; an empty click clears.
    {
      const QPointF a = widgetOf("ma"), b = widgetOf("mb");
      const QRectF box = QRectF(a, b).normalized().adjusted(-14, -14, 14, 14);
      QPointF from, to;
      for (const auto& [p, q] : {std::pair{box.topLeft(), box.bottomRight()}, {box.bottomRight(), box.topLeft()}, {box.topRight(), box.bottomLeft()}, {box.bottomLeft(), box.topRight()}})
        if (empty(p)) { from = p; to = q; break; }
      QVERIFY2(!from.isNull(), "no free corner for the box");
      QVERIFY(gesture("box select ma + mb", 0, [&] { boxDrag(from, to, Qt::NoModifier); }));
      QCOMPARE(sorted(panel->selectedItems()), QStringList({"bone:ma", "bone:mb"}));
      const QPointF c = widgetOf("mc");
      QPointF cf;
      for (const QPointF& p : {c + QPointF(-16, -16), c + QPointF(16, 16), c + QPointF(16, -16), c + QPointF(-16, 16)}) if (empty(p)) { cf = p; break; }
      QVERIFY(!cf.isNull());
      QVERIFY(gesture("shift box adds mc", 0, [&] { boxDrag(cf, c + (c - cf), Qt::ShiftModifier); }));
      QCOMPARE(sorted(panel->selectedItems()), QStringList({"bone:ma", "bone:mb", "bone:mc"}));
      QVERIFY(gesture("click on empty clears", 0, [&] { press(from, Qt::NoModifier); }));
      QVERIFY(panel->selectedItems().isEmpty() && tree->selectedItems().isEmpty());
    }
    // Ctrl+A on the canvas and in the hierarchy, the 선택 menu: bones only (no parts).
    QTest::keyClick(canvas, Qt::Key_A, Qt::ControlModifier);
    QCOMPARE(panel->selectedBones().size(), int(rig()["bones"].size()));
    QCOMPARE(panel->selectedItems().size(), int(rig()["bones"].size()));
    panel->selectItem("");
    QTest::keyClick(tree, Qt::Key_A, Qt::ControlModifier);
    QCOMPARE(panel->selectedItems().size(), int(rig()["bones"].size()));
    QCOMPARE(int(tree->selectedItems().size()), int(rig()["bones"].size()));
    panel->selectItem("bone:ma");
    find<QAction>("cutoutSelectChildren")->trigger();
    QCOMPARE(panel->selectedBones(), QStringList({"ma", "mb"}));

    // (3) Multi-rotate about each origin: ONE transformBones edit turns ma and mc by the same angle.
    panel->setSelection({"bone:ma", "bone:mc"}, "bone:ma");
    panel->setTool(RigCanvas::Rotate);
    pump();
    const Json beforeRotate = rig();
    QVERIFY(gesture("multi rotate (own origins)", 1, [&] { arc(widgetOf("ma"), 30); }));
    {
      const double ra = boneOf("ma").value("rotation", 0.0) - 20, rc = boneOf("mc").value("rotation", 0.0);
      QVERIFY2(std::fabs(ra - 30) < 0.5 && std::fabs(ra - rc) < 1e-6, qPrintable(QString("%1 %2").arg(ra).arg(rc)));
      QVERIFY(boneOf("mb") == [&] { for (const auto& b : beforeRotate["bones"]) if (b["id"] == "mb") return b; return Json(); }());
      QCOMPARE(boneOf("ma")["x"].get<double>(), -150.0);  // own origin: nothing moved
    }
    undo();
    QVERIFY(rig() == beforeRotate);
    redo();
    QVERIFY(rig() != beforeRotate);
    undo();

    // (4) Centroid pivot (toolbar): the pair turns as one piece around the middle of their origins.
    auto* pivot = find<QComboBox>("cutoutPivotMode");
    pivot->setCurrentIndex(1);
    Q_EMIT pivot->activated(1);
    QCOMPARE(int(panel->pivotMode()), int(CutoutPanel::Pivot::Centroid));
    {
      const QPointF a0 = world("ma"), c0 = world("mc"), mid = (a0 + c0) / 2;
      QVERIFY(gesture("multi rotate (centroid)", 1, [&] { arc(canvas->rigToWidget(mid), 90); }));
      const double turn = (boneOf("ma").value("rotation", 0.0) - 20) * PI / 180;
      QVERIFY2(std::fabs(turn - PI / 2) < 0.01, qPrintable(QString::number(turn)));
      QCOMPARE(boneOf("mc").value("rotation", 0.0), boneOf("ma").value("rotation", 0.0) - 20);
      auto turned = [&](QPointF p) { const QPointF v = p - mid; return mid + QPointF(v.x() * std::cos(turn) - v.y() * std::sin(turn), v.x() * std::sin(turn) + v.y() * std::cos(turn)); };
      const QPointF a1 = world("ma"), c1 = world("mc");
      QVERIFY2(QLineF(a1, turned(a0)).length() < 1e-3 && QLineF(c1, turned(c0)).length() < 1e-3, qPrintable(QString("%1,%2").arg(a1.x()).arg(a1.y())));
      QVERIFY(QLineF((a1 + c1) / 2, mid).length() < 1e-3);
      QCOMPARE(host.lastEdit.value("type").toString(), QString("replace"));
    }
    undo();
    pivot->setCurrentIndex(0);
    Q_EMIT pivot->activated(0);
    panel->setTool(RigCanvas::Move);

    // (5) Compensation toggles (setup pose): moving ma keeps its child mb where it was.
    QVERIFY(!panel->compensateBones() && !panel->compensateImages());
    click("cutoutCompensateBones");
    QVERIFY(panel->compensateBones());
    panel->selectItem("bone:ma");
    pump();
    {
      const auto w0 = cutout::setupWorld(rig());
      const QPointF at = widgetOf("ma");
      QVERIFY(gesture("drag ma with bone compensation", 1, [&] { drag(canvas, at, at + QPointF(30, 18)); }));
      const auto w1 = cutout::setupWorld(rig());
      QVERIFY(std::hypot(w1.at("ma").e - w0.at("ma").e, w1.at("ma").f - w0.at("ma").f) > 10);
      const auto& b0 = w0.at("mb");
      const auto& b1 = w1.at("mb");
      for (double d : {b0.a - b1.a, b0.b - b1.b, b0.c - b1.c, b0.d - b1.d, b0.e - b1.e, b0.f - b1.f}) QVERIFY2(std::fabs(d) < 1e-6, qPrintable(QString::number(d)));
      undo();
    }
    // Images: head carries the eyes part; with 이미지 보정 too, turning head moves neither its
    // child arm nor the eyes image (draw list unchanged for them).
    click("cutoutCompensateImages");
    QVERIFY(panel->compensateImages());
    panel->selectItem("bone:head");
    panel->setTool(RigCanvas::Rotate);
    pump();
    {
      const auto drawBefore = drawList();
      const auto w0 = cutout::setupWorld(rig());
      QVERIFY(gesture("rotate head with both compensations", 1, [&] { arc(widgetOf("head"), 25); }));
      QVERIFY(std::fabs(boneOf("head").value("rotation", 0.0) - [&] { for (const auto& b : start["bones"]) if (b["id"] == "head") return b.value("rotation", 0.0); return 0.0; }()) > 5);
      const auto w1 = cutout::setupWorld(rig());
      QVERIFY(std::fabs(w1.at("arm").e - w0.at("arm").e) < 1e-6 && std::fabs(w1.at("arm").a - w0.at("arm").a) < 1e-6);
      const auto drawAfter = drawList();
      QCOMPARE(drawAfter.size(), drawBefore.size());
      for (size_t i = 0; i < drawAfter.size(); i++)
        if (drawAfter[i].slot == "eyes") QVERIFY(std::fabs(drawAfter[i].matrix.e - drawBefore[i].matrix.e) < 1e-6 && std::fabs(drawAfter[i].matrix.a - drawBefore[i].matrix.a) < 1e-6);
      undo();
    }
    // Off again: the head turn carries the eyes along (the toggle really did the work).
    click("cutoutCompensateBones");
    click("cutoutCompensateImages");
    QVERIFY(!panel->compensateBones() && !panel->compensateImages());
    {
      const auto drawBefore = drawList();
      QVERIFY(gesture("rotate head without compensation", 1, [&] { arc(widgetOf("head"), 25); }));
      bool moved = false;
      const auto drawAfter = drawList();
      for (size_t i = 0; i < drawAfter.size(); i++) if (drawAfter[i].slot == "eyes") moved = moved || std::fabs(drawAfter[i].matrix.a - drawBefore[i].matrix.a) > 1e-3;
      QVERIFY(moved);
      undo();
    }
    panel->setTool(RigCanvas::Move);

    // (6) Fields of a multi-selection: mixed "—", absolute, relative, world axes; one edit each.
    panel->setSelection({"bone:ma", "bone:mc"}, "bone:ma");
    pump();
    auto* spinX = find<QDoubleSpinBox>("cutoutPropX");
    QVERIFY(spinX->property("mixed").toBool() && spinX->text() == "—");
    QVERIFY(!find<QDoubleSpinBox>("cutoutPropY")->property("mixed").toBool());
    QCOMPARE(find<QDoubleSpinBox>("cutoutPropY")->value(), -170.0);
    QVERIFY(gesture("absolute X for both", 1, [&] { setSpin("cutoutPropX", 10); }));
    QCOMPARE(boneOf("ma")["x"].get<double>(), 10.0);
    QCOMPARE(boneOf("mc")["x"].get<double>(), 10.0);
    QVERIFY(!spinX->property("mixed").toBool() && spinX->value() == 10.0);
    click("cutoutPropRelative");
    QVERIFY(panel->relativeEdit() && spinX->value() == 0);
    QVERIFY(gesture("relative rotation +12 for both", 1, [&] { setSpin("cutoutPropRotation", 12); }));
    QCOMPARE(boneOf("ma")["rotation"].get<double>(), 32.0);
    QCOMPARE(boneOf("mc")["rotation"].get<double>(), 12.0);
    click("cutoutPropRelative");
    auto* axes = find<QComboBox>("cutoutAxesMode");
    axes->setCurrentIndex(2);
    Q_EMIT axes->activated(2);
    QCOMPARE(int(panel->axes()), int(cutout::Axes::World));
    panel->setSelection({"bone:ma", "bone:mb"}, "bone:mb");
    pump();
    QVERIFY(std::fabs(find<QDoubleSpinBox>("cutoutPropX")->value() - world("mb").x()) < 0.01 || find<QDoubleSpinBox>("cutoutPropX")->property("mixed").toBool());
    QVERIFY(gesture("world X = 300 for a parent and its child", 1, [&] { setSpin("cutoutPropX", 300); }));
    QVERIFY2(std::fabs(world("ma").x() - 300) < 1e-4 && std::fabs(world("mb").x() - 300) < 1e-4, qPrintable(QString("%1 %2").arg(world("ma").x()).arg(world("mb").x())));
    axes->setCurrentIndex(0);
    Q_EMIT axes->activated(0);

    // (7) Animate: K keys every selected bone (one edit); the drag of a selection keys all of it.
    panel->setMode(CutoutPanel::Mode::Animate);
    panel->selectClip("joy");
    panel->setPlayhead(200);
    panel->setSelection({"bone:ma", "bone:mc"}, "bone:mc");
    pump();
    QVERIFY(gesture("K keys both bones", 1, [&] { QTest::keyClick(canvas, Qt::Key_K); }));
    for (const char* target : {"bone:ma", "bone:mc"})
      for (const char* p : {"x", "y", "rotation", "scaleX", "scaleY"}) QVERIFY2(!keyOf("joy", target, p, 200).is_null(), target);
    QVERIFY(keyOf("joy", "bone:mb", "x", 200).is_null());
    panel->setAutoKey(true);
    click("cutoutCompensateBones");  // ignored in Animate (setup-pose compensation only)
    {
      const Json bonesBefore = rig()["bones"];
      const QPointF at = widgetOf("mc");
      QVERIFY(gesture("drag the selection (auto key)", 1, [&] { drag(canvas, at, at + QPointF(0, 24)); }));
      QVERIFY(rig()["bones"] == bonesBefore);
      QVERIFY(keyOf("joy", "bone:ma", "y", 200)["value"].get<double>() > boneOf("ma")["y"].get<double>() + 5);
      QVERIFY(keyOf("joy", "bone:mc", "y", 200)["value"].get<double>() > boneOf("mc")["y"].get<double>() + 5);
      QVERIFY(keyOf("joy", "bone:mb", "y", 200).is_null());
    }
    click("cutoutCompensateBones");
    panel->setAutoKey(false);
    // (8) Pose copy takes the multi-selection: exactly ma and mc arrive at timing 4000 ms.
    QVERIFY(gesture("copy pose of the selection (no edit)", 0, [&] { click("cutoutCopyPose"); }));
    QVERIFY(panel->status().contains("본 2개"));
    panel->selectClip("timing");
    panel->setPlayhead(4000);
    QVERIFY(gesture("paste pose", 1, [&] { click("cutoutPastePose"); }));
    QVERIFY(!keyOf("timing", "bone:ma", "y", 4000).is_null() && !keyOf("timing", "bone:mc", "y", 4000).is_null() && track("timing", "bone:mb", "y") == nullptr);
    // Dopesheet: Ctrl on a row adds its bone to the selection.
    panel->selectItem("bone:ma");
    const int row = rowOf("bone:mc", "y");
    QVERIFY(row >= 0);
    panel->dopesheet()->onRowSelected(row, true);
    QCOMPARE(sorted(panel->selectedItems()), QStringList({"bone:ma", "bone:mc"}));
    panel->setMode(CutoutPanel::Mode::Setup);
    panel->selectItem("");
    if (const QString evidence = qEnvironmentVariable("CUTOUT_STUDIO_EVIDENCE"); !evidence.isEmpty()) {
      panel->setSelection({"bone:ma", "bone:mb", "bone:mc"});
      pump();
      QVERIFY(panel->window()->grab().save(evidence + "/studio-multi-select.png"));
      panel->selectItem("");
    }
    for (int i = 0; i < host.edits - editsBefore && rig() != start; i++) undo();
    QVERIFY(rig() == start);
  }

  // Clip events (plugin 1.8.0): the dopesheet 이벤트 row, the 이벤트 page (definitions with payload
  // and sound), key editing (add / popover / drag / Delete) and the preview log.
  void clipEvents() {
    panel->selectRig("hero");
    find<QTabWidget>("cutoutBottomTabs")->setCurrentIndex(0);
    panel->setMode(CutoutPanel::Mode::Animate);
    panel->selectItem("");
    pump();
    answers = {"cue"};
    QVERIFY(gesture("new clip for events", 1, [&] { click("cutoutNewClip"); }));
    QCOMPARE(panel->clipId(), QString("cue"));
    QVERIFY(gesture("clip duration 800", 1, [&] { auto* s = find<QSpinBox>("cutoutClipDuration"); s->setValue(800); Q_EMIT s->editingFinished(); }));
    auto events = [&]() { const Json r = rig(); for (const auto& c : r["clips"]) if (c["id"] == "cue") return c.value("events", Json::array()); return Json::array(); };
    Dopesheet* sheet = panel->dopesheet();
    const int er = sheet->eventRow();
    QVERIFY(er >= 0 && sheet->rows()[er].label == "이벤트" && sheet->rows()[er].times.isEmpty());  // the row is there before any key
    auto* tabs = find<QTabWidget>("cutoutLookTabs");
    QWidget* page = find<QWidget>("cutoutEventPage");
    QVERIFY(page && tabs->indexOf(page) >= 0);
    // No definitions yet: 이벤트 키 asks a name, any valid name works.
    panel->setPlayhead(100);
    answers = {"footstep"};
    QVERIFY(gesture("event key without definitions", 1, [&] { click("cutoutAddEventKey"); }));
    QVERIFY(events() == Json::array({{{"timeMs", 100}, {"name", "footstep"}}}));
    QCOMPARE(panel->timeView()->selection(), (QVector<KeyRef>{{"event", "footstep", 100}}));
    QCOMPARE(sheet->rows()[sheet->eventRow()].names, QStringList({"footstep"}));
    // The first definition also defines the name keys already use.
    answers = {"hit"};
    QVERIFY(gesture("new event definition", 1, [&] { click("cutoutNewEvent"); }));
    QVERIFY(rig()["events"] == Json::array({{{"name", "footstep"}}, {{"name", "hit"}}}));
    QCOMPARE(panel->eventId(), QString("hit"));
    QCOMPARE(find<QListWidget>("cutoutEventList")->count(), 2);
    // Payload defaults and the sound: one edit per field.
    QVERIFY(gesture("event int", 1, [&] { auto* s = find<QSpinBox>("cutoutEventInt"); s->setValue(5); Q_EMIT s->editingFinished(); }));
    QVERIFY(gesture("event string", 1, [&] { auto* e = find<QLineEdit>("cutoutEventString"); e->setText("punch"); Q_EMIT e->editingFinished(); }));
    host.assetList.append(QJsonObject{{"id", "sfx-hit"}, {"kind", "audio"}, {"path", "assets/hit.wav"}});
    panel->refreshFromView();
    pump();
    auto* audio = find<QComboBox>("cutoutEventAudio");
    QVERIFY(!find<QDoubleSpinBox>("cutoutEventVolume")->isEnabled());  // no sound yet
    QVERIFY(gesture("event sound", 1, [&] { const int i = audio->findData("sfx-hit"); QVERIFY(i > 0); audio->setCurrentIndex(i); Q_EMIT audio->activated(i); }));
    QVERIFY(find<QDoubleSpinBox>("cutoutEventVolume")->isEnabled());
    QVERIFY(gesture("event volume", 1, [&] { setSpin("cutoutEventVolume", 0.5); }));
    QVERIFY(gesture("event int back to 0 removes it", 1, [&] { auto* s = find<QSpinBox>("cutoutEventInt"); s->setValue(0); Q_EMIT s->editingFinished(); }));
    QVERIFY(rig()["events"][1] == Json({{"name", "hit"}, {"string", "punch"}, {"audio", "sfx-hit"}, {"volume", 0.5}}));
    QVERIFY(gesture("same value: no edit", 0, [&] { auto* e = find<QLineEdit>("cutoutEventString"); Q_EMIT e->editingFinished(); }));
    // Context menu on the event row adds the shown definition at the clicked time.
    QVERIFY(gesture("context menu: add event here", 1, [&] { contextPick(sheet, QPoint(sheet->timeToX(200), sheet->rowCenterY(er)), "cutoutEventAddHere"); }));
    QVERIFY(events()[1] == Json({{"timeMs", 200}, {"name", "hit"}}));
    QVERIFY(gesture("double-click on empty event row adds", 1, [&] {
      const QPointF at(sheet->timeToX(500), sheet->rowCenterY(er));
      mouse(sheet, QEvent::MouseButtonDblClick, at, Qt::LeftButton, Qt::LeftButton);
      mouse(sheet, QEvent::MouseButtonRelease, at, Qt::LeftButton, Qt::NoButton);
    }));
    QVERIFY(events().size() == 3 && events()[2] == Json({{"timeMs", 500}, {"name", "hit"}}));
    QVERIFY(gesture("undefined name refused", 0, [&] { QVERIFY(!panel->addEventKey(300, "jump")); }));
    QVERIFY(panel->status().contains("정의되지 않은"));
    QVERIFY(gesture("same key twice refused", 0, [&] { QVERIFY(!panel->addEventKey(200, "hit")); }));
    // Payload override and rename of one key through the popover (askEventKey = the real dialog).
    std::optional<CutoutPanel::EventKeyChoice> scripted;
    QString offered;
    panel->askEventKey = [&](CutoutPanel::EventKeyChoice& c) {
      if (!scripted) return panel->runEventKeyDialog(c);
      offered = c.name;
      c = *scripted;
      return true;
    };
    scripted = CutoutPanel::EventKeyChoice{"hit", 9, std::nullopt, std::nullopt};
    QVERIFY(gesture("key int override", 1, [&] { QVERIFY(panel->editEventKeyInteractive({"event", "hit", 200})); }));
    QVERIFY(offered == "hit" && events()[1] == Json({{"timeMs", 200}, {"name", "hit"}, {"int", 9}}));
    scripted.reset();
    QString defaultsShown;
    bool prefilled = false;
    onDialog("cutoutEventKeyDialog", [&](QDialog* d) {
      defaultsShown = d->findChild<QLabel*>("cutoutEventKeyDefaults")->text();
      prefilled = d->findChild<QCheckBox*>("cutoutEventKeyIntOn")->isChecked() && d->findChild<QSpinBox*>("cutoutEventKeyInt")->value() == 9;
      d->findChild<QCheckBox*>("cutoutEventKeyIntOn")->setChecked(false);  // back to the definition's value
      d->findChild<QCheckBox*>("cutoutEventKeyStringOn")->setChecked(true);
      d->findChild<QLineEdit*>("cutoutEventKeyString")->setText("kick");
      d->findChild<QComboBox*>("cutoutEventKeyName")->setCurrentText("footstep");
      return true;
    });
    QVERIFY(gesture("double-click pin: popover rename + string override", 1, [&] {
      const QPointF at(sheet->timeToX(200), sheet->rowCenterY(er));
      mouse(sheet, QEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton);
      mouse(sheet, QEvent::MouseButtonRelease, at, Qt::LeftButton, Qt::NoButton);
      mouse(sheet, QEvent::MouseButtonDblClick, at, Qt::LeftButton, Qt::LeftButton);
      mouse(sheet, QEvent::MouseButtonRelease, at, Qt::LeftButton, Qt::NoButton);
    }));
    QVERIFY2(prefilled && defaultsShown.contains("punch") && defaultsShown.contains("sfx-hit"), qPrintable(defaultsShown));
    QVERIFY2(events()[1] == Json({{"timeMs", 200}, {"name", "footstep"}, {"string", "kick"}}), events().dump().c_str());
    QCOMPARE(panel->timeView()->selection(), (QVector<KeyRef>{{"event", "footstep", 200}}));
    // Two names share one row; a drag moves the pin (one moveKeys edit), Delete removes it.
    QCOMPARE(sheet->rows()[sheet->eventRow()].names, QStringList({"footstep", "footstep", "hit"}));
    QVERIFY(gesture("drag event key", 1, [&] { drag(sheet, QPointF(sheet->timeToX(500), sheet->rowCenterY(er)), QPointF(sheet->timeToX(560), sheet->rowCenterY(er))); }));
    QVERIFY(events()[2] == Json({{"timeMs", 560}, {"name", "hit"}}));
    QCOMPARE(panel->timeView()->selection(), (QVector<KeyRef>{{"event", "hit", 560}}));
    QVERIFY(gesture("drag onto a same-name key refused", 0, [&] {
      drag(sheet, QPointF(sheet->timeToX(200), sheet->rowCenterY(er)), QPointF(sheet->timeToX(100), sheet->rowCenterY(er)));
    }));
    QCOMPARE(events()[1]["timeMs"].get<int>(), 200);
    // Mixed selection: a bone key and an event key move together in one edit.
    panel->setPlayhead(300);
    panel->selectItem("bone:head");
    QVERIFY(gesture("bone key in cue", 1, [&] { setSpin("cutoutPropRotation", 12); if (!panel->hasPending()) return; click("cutoutKeyButton"); }));
    panel->timeView()->setSelection({{"bone:head", "rotation", 300}, {"event", "hit", 560}});
    QVERIFY(gesture("move mixed keys", 1, [&] { QVERIFY(panel->moveKeys(panel->timeView()->selection(), 40)); }));
    QVERIFY(events()[2]["timeMs"] == 600 && keyOf("cue", "bone:head", "rotation", 340).is_object());
    panel->timeView()->setSelection({{"event", "hit", 600}});
    sheet->setFocus();
    QVERIFY(gesture("Delete event key", 1, [&] { QTest::keyClick(sheet, Qt::Key_Delete); }));
    QCOMPARE(events().size(), size_t(2));
    // Rename the definition: keys of every clip follow; undo / redo restore exactly.
    panel->selectEvent("footstep");
    answers = {"step"};
    const Json beforeRename = rig();
    QVERIFY(gesture("rename event definition", 1, [&] { click("cutoutRenameEvent"); }));
    QVERIFY(rig()["events"][0]["name"] == "step" && events()[0]["name"] == "step" && events()[1]["name"] == "step");
    undo();
    QVERIFY(rig() == beforeRename);
    redo();
    QVERIFY(events()[1] == Json({{"timeMs", 200}, {"name", "step"}, {"string", "kick"}}));
    QCOMPARE(panel->eventId(), QString("step"));

    // Preview log: playback fires each event once per pass, scrubbing never does.
    QVERIFY(gesture("event at the clip end", 1, [&] { QVERIFY(panel->addEventKey(800, "hit")); }));
    panel->setManualClock(true);
    panel->clearEventLog();
    find<QCheckBox>("cutoutLoopPreview")->setChecked(true);
    panel->setPlayhead(0);
    QVERIFY(gesture("loop playback, two passes", 0, [&] {
      click("cutoutPlay");
      for (int i = 0; i < 16; i++) panel->advancePlayback(100);  // 1600 ms = exactly two passes
    }));
    const QStringList twice = panel->eventLog();
    QCOMPARE(twice.size(), 4);  // 100, 200 per pass; the 800 ms end event never fires while looping
    QVERIFY2(twice[0].contains("cue 100ms · step") && twice[1].contains("cue 200ms · step (\"kick\")") && twice[2].contains("100ms") && twice[3].contains("200ms"), qPrintable(twice.join(" | ")));
    QVERIFY(find<QListWidget>("cutoutEventLog")->count() == 4 && find<QLabel>("cutoutEventFlash")->text().contains("step"));
    click("cutoutPlay");  // stop
    QVERIFY(gesture("scrubbing fires nothing", 0, [&] {
      for (int t : {100, 200, 150, 800, 0}) panel->setPlayhead(t);
      drag(sheet, QPointF(sheet->timeToX(50), 8), QPointF(sheet->timeToX(250), 8));  // ruler scrub
      panel->advancePlayback(500);                                                        // stopped: no time passes
    }));
    QCOMPARE(panel->eventLog().size(), 4);
    find<QCheckBox>("cutoutLoopPreview")->setChecked(false);
    panel->setPlayhead(700);
    click("cutoutPlay");
    for (int i = 0; i < 4; i++) panel->advancePlayback(50);
    QVERIFY(!find<QPushButton>("cutoutPlay")->isChecked());  // stopped at the end
    QVERIFY2(panel->eventLog().size() == 5 && panel->eventLog().last().contains("800ms · hit (\"punch\" · 🔊 sfx-hit)"), qPrintable(panel->eventLog().join(" | ")));
    find<QCheckBox>("cutoutLoopPreview")->setChecked(true);
    panel->setManualClock(false);
    // Delete the definition: asked first because keys use it, then they go too.
    panel->selectEvent("hit");
    QVERIFY(gesture("delete event definition with keys", 1, [&] { click("cutoutDeleteEvent"); }));
    QVERIFY(rig()["events"] == Json::array({{{"name", "step"}}}) && events().size() == 2);
    panel->askEventKey = [this](CutoutPanel::EventKeyChoice& c) { return panel->runEventKeyDialog(c); };
    if (const QString evidence = qEnvironmentVariable("CUTOUT_STUDIO_EVIDENCE"); !evidence.isEmpty()) {
      tabs->setCurrentWidget(page);
      pump();
      QVERIFY(panel->window()->grab().save(evidence + "/studio-events.png"));
    }
    panel->selectClip("joy");
    panel->setMode(CutoutPanel::Mode::Setup);
  }

  // Graph value axis (plugin 1.8.0): Ctrl+Shift+wheel / Alt+wheel zoom at the cursor, middle-drag
  // vertical pan, 값 맞춤; the time zoom is untouched and keys stay draggable at any zoom.
  void graphValueZoom() {
    panel->selectRig("hero");
    find<QTabWidget>("cutoutBottomTabs")->setCurrentIndex(0);
    panel->setMode(CutoutPanel::Mode::Animate);
    panel->selectClip("joy");
    panel->selectItem("bone:head");
    click("cutoutViewBoth");
    pump();
    GraphEditor* graph = panel->graph();
    const int rot = graph->channelIndex("bone:head", "rotation");
    QVERIFY(rot >= 0 && graph->channels()[rot].keys.size() >= 2);
    click("cutoutGraphFitValues");
    QVERIFY(!graph->valuesZoomed());
    const double pxPerMs = panel->timeView()->pxPerMs(), scroll = panel->timeView()->scrollMs();
    const QPointF key = graph->keyPoint(rot, 1);
    const auto [lo0, hi0] = graph->valueRange(rot);
    const double under = graph->yToValue(rot, key.y());
    auto near = [](double a, double b) { return std::fabs(a - b) <= 1e-9 * std::max(1.0, std::fabs(b)); };
    QVERIFY(gesture("Ctrl+Shift+wheel value zoom", 0, [&] { wheel(graph, QPointF(key.x() + 30, key.y()), 1, Qt::ControlModifier | Qt::ShiftModifier); }));
    auto [lo1, hi1] = graph->valueRange(rot);
    QVERIFY2(near(hi1 - lo1, (hi0 - lo0) / 1.25) && near(graph->yToValue(rot, key.y()), under), qPrintable(QString("%1..%2 -> %3..%4").arg(lo0).arg(hi0).arg(lo1).arg(hi1)));
    QVERIFY(graph->valuesZoomed());
    QVERIFY(gesture("Alt+wheel value zoom (horizontal delta)", 0, [&] {
      QWheelEvent e(QPointF(key.x() + 30, key.y()), graph->mapToGlobal(QPointF(key.x() + 30, key.y())), QPoint(), QPoint(120, 0), Qt::NoButton, Qt::AltModifier, Qt::NoScrollPhase, false);
      QCoreApplication::sendEvent(graph, &e);
    }));
    auto [lo2, hi2] = graph->valueRange(rot);
    QVERIFY(near(hi2 - lo2, (hi0 - lo0) / 1.5625) && near(graph->yToValue(rot, key.y()), under));
    QVERIFY(gesture("Alt+wheel out", 0, [&] { wheel(graph, QPointF(key.x() + 30, key.y()), -1, Qt::AltModifier); }));
    auto [lo3, hi3] = graph->valueRange(rot);
    QVERIFY(near(hi3 - lo3, hi1 - lo1) && near(graph->yToValue(rot, key.y()), under));
    QCOMPARE(panel->timeView()->pxPerMs(), pxPerMs);  // the shared time axis never moved
    // Middle-drag 30 px down: the value under the cursor follows it.
    const double y0 = graph->height() / 2.0, v0 = graph->yToValue(rot, y0);
    QVERIFY(gesture("middle-drag vertical pan", 0, [&] {
      const QPointF from(key.x() + 40, y0);
      mouse(graph, QEvent::MouseButtonPress, from, Qt::MiddleButton, Qt::MiddleButton);
      mouse(graph, QEvent::MouseMove, from + QPointF(0, 30), Qt::NoButton, Qt::MiddleButton);
      mouse(graph, QEvent::MouseButtonRelease, from + QPointF(0, 30), Qt::MiddleButton, Qt::NoButton);
    }));
    QVERIFY(near(graph->yToValue(rot, y0 + 30), v0));
    auto [lo4, hi4] = graph->valueRange(rot);
    QVERIFY(near(hi4 - lo4, hi3 - lo3) && near(panel->timeView()->scrollMs(), scroll));
    // A key drag at this zoom: the value is read with the zoomed mapping; the zoom survives the commit.
    const int ms = graph->channels()[rot].keys[1].timeMs;
    const QPointF at = graph->keyPoint(rot, 1);
    graph->setSelectedKeys({});
    const double expected = std::round(graph->yToValue(rot, at.y() - 20) * 1e4) / 1e4;
    QVERIFY(gesture("key drag at value zoom", 1, [&] { drag(graph, at, at + QPointF(0, -20)); }));
    QVERIFY2(std::fabs(keyOf("joy", "bone:head", "rotation", ms)["value"].get<double>() - expected) < 1e-9, keyOf("joy", "bone:head", "rotation", ms).dump().c_str());
    QVERIFY(graph->valuesZoomed() && graph->valueRange(rot) == std::make_pair(lo4, hi4));
    QVERIFY(std::fabs(graph->keyPoint(rot, 1).y() - (at.y() - 20)) < 0.5);  // the key stays under the cursor
    // A handle drag at this zoom commits exactly the curve the zoomed mapping predicts.
    graph->setSelectedKeys({{"bone:head", "rotation", ms}});
    pump();
    if (const auto handle = graph->handlePoint(rot, 1, GraphEditor::Side::Out)) {
      const QPointF drop = *handle + QPointF(10, 6);
      const cutout::Curve curve = graph->curveForHandle(rot, 1, GraphEditor::Side::Out, drop);
      QVERIFY(gesture("handle drag at value zoom", 1, [&] { drag(graph, *handle, drop); }));
      const Json k = keyOf("joy", "bone:head", "rotation", ms);
      for (size_t i = 0; i < 4; i++) QVERIFY2(std::fabs(k["curve"][i].get<double>() - curve[i]) < 1e-9, k.dump().c_str());
      undo();
    }
    undo();
    QVERIFY(graph->valuesZoomed());  // undo rebuilt the same channels: the view stays
    redo();
    undo();
    // Normalized channels zoom together: each keeps its own value under the cursor.
    find<QCheckBox>("cutoutGraphNormalize")->setChecked(true);
    QVERIFY(!graph->valuesZoomed());
    QVector<double> before;
    for (int c = 0; c < int(graph->channels().size()); c++) before << graph->yToValue(c, key.y());
    wheel(graph, QPointF(key.x(), key.y()), 2, Qt::ControlModifier | Qt::ShiftModifier);
    for (int c = 0; c < int(graph->channels().size()); c++) QVERIFY(near(graph->yToValue(c, key.y()), before[c]));
    find<QCheckBox>("cutoutGraphNormalize")->setChecked(false);
    wheel(graph, QPointF(key.x(), key.y()), 3, Qt::AltModifier);
    QVERIFY(gesture("값 맞춤", 0, [&] { click("cutoutGraphFitValues"); }));
    QVERIFY(!graph->valuesZoomed() && graph->valueRange(rot) == std::make_pair(lo0, hi0));
    panel->setMode(CutoutPanel::Mode::Setup);
  }

  // Mesh repaint budget (plugin 1.6.0): a 2048-triangle mesh (32x32 grid over a 512x512 image)
  // drawn offscreen at 1.6x, undeformed (one textured fill) and deformed (per-triangle textured
  // brushes, no clip paths). The rigid fill must also look like the plain region it replaces.
  void meshRenderBenchmark() {
    QImage tex(512, 512, QImage::Format_ARGB32);
    for (int y = 0; y < 512; y++) for (int x = 0; x < 512; x++) tex.setPixelColor(x, y, QColor((x * 7) & 255, (y * 5) & 255, ((x + y) * 3) & 255, 255));
    const QString file = dir.filePath("bench.png");
    QVERIFY(tex.save(file));
    ImageCache images;
    images.setAssets(QJsonArray{QJsonObject{{"id", "img-bench"}, {"kind", "image"}, {"absolutePath", file}}});
    Json d = Json::parse(R"({"schemaVersion": 1, "canvas": {"width": 600, "height": 600, "originX": 300, "originY": 300},
      "bones": [{"id": "root", "parent": null, "x": 300, "y": 300}],
      "slots": [{"id": "sheet", "bone": "root", "attachment": "sheet"}],
      "attachments": [{"slot": "sheet", "name": "sheet", "imageAssetId": "img-bench", "width": 512, "height": 512}], "clips": []})");
    const Json region = d;
    d = cutout::applyEdit(d, {{"op", "convertToMesh"}, {"slot", "sheet"}, {"rows", 32}, {"cols", 32}}).data;
    QCOMPARE(d["attachments"][0]["triangles"].size(), size_t(3 * 2048));
    const size_t n = d["attachments"][0]["vertices"].size() / 2;
    std::vector<double> wave(2 * n);
    for (size_t v = 0; v < n; v++) {
      const double x = d["attachments"][0]["vertices"][2 * v].get<double>(), y = d["attachments"][0]["vertices"][2 * v + 1].get<double>();
      wave[2 * v] = 9 * std::sin(y / 40.0);
      wave[2 * v + 1] = 7 * std::cos(x / 55.0);
    }
    d["clips"] = Json::array({{{"id", "wave"}, {"durationMs", 1000}, {"tracks", Json::array({{{"target", "slot:sheet"}, {"property", "deform"}, {"attachment", "sheet"},
                                                                                              {"keys", Json::array({{{"timeMs", 0}, {"offsets", wave}}})}}})}}});
    const cutout::Rig rig = cutout::parseRig("bench", d), plain = cutout::parseRig("bench", region);
    const cutout::Pose rest = cutout::samplePose(rig, -1, 0), bent = cutout::samplePose(rig, 0, 0);
    QImage target(1024, 1024, QImage::Format_ARGB32_Premultiplied);
    auto render = [&](const cutout::Rig& r, const cutout::Pose& pose) {
      target.fill(Qt::black);
      QPainter p(&target);
      p.translate(512, 512);
      p.scale(1.6, 1.6);
      p.translate(-300, -300);
      drawRig(p, r, pose, images);
    };
    auto timeMs = [&](const cutout::Rig& r, const cutout::Pose& pose) {
      render(r, pose);  // warm-up (image decode)
      QVector<double> runs;
      for (int i = 0; i < 9; i++) {
        QElapsedTimer t;
        t.start();
        render(r, pose);
        runs << t.nsecsElapsed() / 1e6;
      }
      std::sort(runs.begin(), runs.end());
      return runs[runs.size() / 2];
    };
    const double rigid = timeMs(rig, rest), deformed = timeMs(rig, bent), regionMs = timeMs(plain, cutout::samplePose(plain, -1, 0));
    qInfo("mesh repaint 2048 triangles, 1024x1024 offscreen (median of 9): rigid %.2f ms, deformed %.2f ms, plain region %.2f ms", rigid, deformed, regionMs);
    // Correctness: the undeformed mesh draws the region's pixels (inside, away from the outer edge).
    render(plain, cutout::samplePose(plain, -1, 0));
    const QImage want = target.copy();
    render(rig, rest);
    double diff = 0;
    int samples = 0;
    for (int y = 140; y < 884; y += 7)
      for (int x = 140; x < 884; x += 7) {
        const QRgb a = want.pixel(x, y), b = target.pixel(x, y);
        diff += std::abs(qRed(a) - qRed(b)) + std::abs(qGreen(a) - qGreen(b)) + std::abs(qBlue(a) - qBlue(b));
        samples++;
      }
    qInfo("rigid mesh vs region: mean channel difference %.3f", diff / samples / 3);
    QVERIFY(diff / samples / 3 < 2.0);
    // Deformed: no seams (no background showing through inside) and the texture really moved.
    render(rig, bent);
    int holes = 0, moved = 0;
    for (int y = 160; y < 864; y += 5)
      for (int x = 160; x < 864; x += 5) {
        holes += target.pixel(x, y) == qRgb(0, 0, 0);
        moved += target.pixel(x, y) != want.pixel(x, y);
      }
    QCOMPARE(holes, 0);
    QVERIFY(moved > 1000);
    QVERIFY2(rigid < 30 && deformed < 30, qPrintable(QString("budget 30 ms: rigid %1, deformed %2").arg(rigid).arg(deformed)));
  }

  void paintElementDrawsPixels() {
    QImage img(300, 450, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    const QJsonObject element{{"id", "hero-on-title"}, {"kind", "plugin"}, {"width", 300}, {"height", 450},
                              {"extension", QJsonObject{{"plugin", "cutout-animation"}, {"type", "character"}, {"ref", "hero"}, {"props", QJsonObject{{"scale", 1}}}}}};
    QVERIFY(plugin.paintElement(&p, element, QRectF(0, 0, 300, 450)));
    QVERIFY(!plugin.paintElement(&p, QJsonObject{{"extension", QJsonObject{{"ref", "missing"}}}}, QRectF(0, 0, 300, 450)));
    p.end();
    int painted = 0;
    for (int y = 0; y < img.height(); y++) for (int x = 0; x < img.width(); x++) painted += qAlpha(img.pixel(x, y)) > 0;
    qInfo("paintElement painted %d pixels", painted);
    QVERIFY(painted > 200);
  }

  void pluginLoaderLoadsDll() {
    // The shipped module must load through QPluginLoader and expose the studio interface.
    QPluginLoader loader(QCoreApplication::applicationDirPath() + "/cutout-studio.dll");
    QCOMPARE(loader.metaData().value("IID").toString(), QString(RaisingStudioPlugin_iid));
    QCOMPARE(loader.metaData().value("MetaData").toObject().value("id").toString(), QString("cutout-animation"));
    QObject* instance = loader.instance();
    QVERIFY2(instance, qPrintable(loader.errorString()));
    auto* loaded = qobject_cast<raising::StudioPlugin*>(instance);
    QVERIFY(loaded && loaded->pluginId() == "cutout-animation" && loaded->apiVersion() == raising::STUDIO_PLUGIN_API);
    // The optional workspace interface is found the way the host looks for it.
    QVERIFY(qobject_cast<raising::StudioWorkspacePlugin*>(instance));
  }

  void workspaceLayout() {
    // Until now the mock host was an old host: the panel lived in the dock.
    auto* dock = host.mainWindow.findChild<QDockWidget*>("cutoutAnimationDock");
    QVERIFY(dock && dock->isAncestorOf(panel));
    QVERIFY(qobject_cast<raising::StudioWorkspacePlugin*>(static_cast<QObject*>(&plugin)));
    const int edits = host.edits, revision = host.revision;
    const auto workspaces = plugin.createWorkspaces(&workspaceHost);
    QCOMPARE(workspaces.size(), 1);
    QCOMPARE(workspaces[0].id, QString("character"));
    QCOMPARE(workspaces[0].widget, static_cast<QWidget*>(panel));
    QCOMPARE(workspaces[0].shortcut, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
    QVERIFY(plugin.createWorkspaces(&workspaceHost).isEmpty());  // offered once
    // The dock keeps a launcher (mini preview + 작업 공간 열기); the panel waits for the host.
    QVERIFY(!dock->isAncestorOf(panel));
    auto* launcher = dock->findChild<QWidget*>("cutoutWorkspaceLauncher");
    QVERIFY(launcher);
    // Host side: the workspace page becomes the central widget, docks are hidden.
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(panel);
    host.mainWindow.setCentralWidget(page);
    dock->hide();
    workspaceHost.current = "character";
    plugin.workspaceChanged("character", true);
    find<QTabWidget>("cutoutBottomTabs")->setCurrentIndex(0);  // the timeline tab (clips | time | mix)
    pump();
    QCOMPARE(host.mainWindow.focusWidget(), static_cast<QWidget*>(panel->canvas()));
    // Every splitter child keeps its minimum and nothing leaves the window, from a small
    // 1280x720 laptop to a 5120x1440 ultrawide and a very tall portrait screen.
    int sideWidth = -1;
    for (const QSize size : {QSize(1280, 720), QSize(1920, 1080), QSize(3840, 1080), QSize(5120, 1440), QSize(1080, 1920), QSize(1440, 2560)}) {
      host.mainWindow.resize(size);
      pump();
      QTest::qWait(30);
      const QString at = QString("%1x%2: ").arg(size.width()).arg(size.height());
      QVERIFY2(host.mainWindow.size() == size, qPrintable(at + "window refused its size (minimum too large)"));
      QVERIFY2(panel->geometry() == page->rect(), qPrintable(at + "panel does not fill the workspace"));
      for (QSplitter* split : panel->splitters()) {
        if (!split->isVisible()) continue;
        for (int i = 0; i < split->count(); i++) {
          QWidget* child = split->widget(i);
          if (!child->isVisible()) continue;
          const bool horizontal = split->orientation() == Qt::Horizontal;
          const int have = horizontal ? child->width() : child->height();
          const int explicitMin = horizontal ? child->minimumWidth() : child->minimumHeight();
          const int need = explicitMin > 0 ? explicitMin : (horizontal ? child->minimumSizeHint().width() : child->minimumSizeHint().height());
          QVERIFY2(have >= need, qPrintable(at + QString("%1[%2] %3 < %4").arg(split->objectName()).arg(i).arg(have).arg(need)));
        }
      }
      for (const char* name : {"cutoutModeBanner", "cutoutStatus", "cutoutClipList", "cutoutMixTable", "cutoutDopesheetScroll", "cutoutGraphEditor"}) {
        auto* w = find<QWidget>(name);
        QVERIFY2(w && w->isVisible(), name);
        QVERIFY2(panel->rect().contains(QRect(w->mapTo(panel, QPoint(0, 0)), w->size())), qPrintable(at + name + " cut off"));
      }
      QVERIFY(panel->canvas()->width() >= 200 && panel->canvas()->height() >= 150);
      // Every page of 스킨 · 표정 · IK · 메시 shows its list and all its buttons by default.
      for (const auto mode : {CutoutPanel::Mode::Setup, CutoutPanel::Mode::Animate}) {
        panel->setMode(mode);
        pump();
        const QStringList clipped = lookPageProblems();
        QVERIFY2(clipped.isEmpty(), qPrintable(at + (mode == CutoutPanel::Mode::Setup ? "setup: " : "animate: ") + clipped.join(" | ")));
      }
      panel->setMode(CutoutPanel::Mode::Setup);
      find<QTabWidget>("cutoutBottomTabs")->setCurrentIndex(0);
      pump();
      // Side panels keep their width on wide screens: the canvas gets the extra space.
      if (size.width() >= 3840) {
        if (sideWidth < 0) sideWidth = panel->leftPane()->width();
        QCOMPARE(panel->leftPane()->width(), sideWidth);
        QVERIFY2(panel->canvas()->width() > size.width() - 700, qPrintable(QString::number(panel->canvas()->width())));
      }
    }
    host.mainWindow.resize(1700, 1200);
    pump();
    // A layout saved by plugin <= 1.4.0 (no version, right column starving the look pages) comes
    // back as the defaults; the same state saved by this version is restored as is.
    {
      auto* right = find<QSplitter>("cutoutRightSplitter");
      const QList<int> defaults = right->sizes();
      right->setSizes({5000, 1, 1});
      pump();
      const QList<int> starved = right->sizes();
      QVERIFY2(starved[1] < defaults[1] - 100, qPrintable(QString("%1 vs %2").arg(starved[1]).arg(defaults[1])));
      QSettings q;
      q.beginGroup("plugins/cutout-animation/layout");
      q.setValue("cutoutRightSplitter", right->saveState());
      q.setValue("version", 1);
      right->setSizes(defaults);
      q.sync();
      panel->restoreLayout();
      pump();
      QCOMPARE(right->sizes(), defaults);
      QVERIFY(lookPageProblems().isEmpty());
      q.setValue("version", 2);
      q.sync();
      panel->restoreLayout();
      pump();
      QCOMPARE(right->sizes(), starved);
      QVERIFY2(lookPageProblems().isEmpty(), "even at its minimum the look section keeps its buttons");
      right->setSizes(defaults);
      panel->saveLayout();
    }
    // Tab on the canvas / dopesheet = full view (side panels hidden); Tab in a form field still
    // moves the focus. Presentation only.
    const int canvasWidth = panel->canvas()->width();
    QTest::keyClick(panel->canvas(), Qt::Key_Tab);
    pump();
    QVERIFY(panel->fullView() && !panel->leftPane()->isVisible() && !panel->rightPane()->isVisible());
    QVERIFY(find<QToolButton>("cutoutFullView")->isChecked());
    QVERIFY(panel->canvas()->width() > canvasWidth + 300);
    QTest::keyClick(panel->dopesheet(), Qt::Key_Tab);
    pump();
    QVERIFY(!panel->fullView() && panel->leftPane()->isVisible());
    find<QToolButton>("cutoutFullView")->click();
    QVERIFY(panel->fullView());
    find<QToolButton>("cutoutFullView")->click();
    QVERIFY(!panel->fullView());
    auto* spin = find<QDoubleSpinBox>("cutoutPropX");
    spin->setFocus();
    QTest::keyClick(spin, Qt::Key_Tab);
    QVERIFY(!panel->fullView());
    // Splitter sizes and collapsed sections persist (QSettings), restored on the next build.
    auto* split = find<QSplitter>("cutoutWorkspaceSplitter");
    QSplitterHandle* handle = split->handle(1);
    const int leftBefore = split->sizes()[0];
    QTest::mousePress(handle, Qt::LeftButton, Qt::NoModifier, QPoint(2, handle->height() / 2));
    QTest::mouseMove(handle, QPoint(82, handle->height() / 2));
    QTest::mouseRelease(handle, Qt::LeftButton, Qt::NoModifier, QPoint(82, handle->height() / 2));
    pump();
    const QList<int> moved = split->sizes();
    QVERIFY2(moved[0] > leftBefore + 40, qPrintable(QString("%1 -> %2").arg(leftBefore).arg(moved[0])));
    QSettings settings;
    QVERIFY(settings.contains("plugins/cutout-animation/layout/cutoutWorkspaceSplitter"));
    find<QToolButton>("cutoutSectionDrawOrderToggle")->click();
    pump();
    QVERIFY(!find<QWidget>("cutoutDrawOrder")->isVisible());
    QVERIFY(find<QWidget>("cutoutSectionDrawOrder")->height() <= find<QToolButton>("cutoutSectionDrawOrderToggle")->height() + 4);
    QVERIFY(QSettings().value("plugins/cutout-animation/layout/collapsed").toStringList().contains("cutoutSectionDrawOrderToggle"));
    split->setSizes({300, 900, 300});
    {
      QSignalBlocker quiet(find<QToolButton>("cutoutSectionDrawOrderToggle"));  // no save: restored below
      find<QToolButton>("cutoutSectionDrawOrderToggle")->setChecked(true);
    }
    panel->restoreLayout();
    pump();
    QCOMPARE(split->sizes(), moved);
    QVERIFY(!find<QWidget>("cutoutDrawOrder")->isVisible());
    find<QToolButton>("cutoutSectionDrawOrderToggle")->click();
    pump();
    QVERIFY(find<QWidget>("cutoutDrawOrder")->isVisible());
    // Back on the dock side: the launcher shows the current pose and opens the workspace.
    workspaceHost.current.clear();
    plugin.workspaceChanged("character", false);
    auto* preview = launcher->findChild<QLabel*>("cutoutLauncherPreview");
    QVERIFY(preview);
    const QImage thumb = preview->pixmap().toImage();
    int painted = 0;
    for (int y = 0; y < thumb.height(); y++) for (int x = 0; x < thumb.width(); x++) painted += qAlpha(thumb.pixel(x, y)) > 0;
    QVERIFY2(painted > 20, qPrintable(QString::number(painted)));  // small parts in a 72 px contain-fit
    QVERIFY(launcher->findChild<QLabel*>("cutoutLauncherSummary")->text().contains("hero"));
    launcher->findChild<QPushButton*>("cutoutOpenWorkspace")->click();
    QCOMPARE(workspaceHost.requests.last(), QString("character"));
    plugin.openEditor("controller", "hero");
    QCOMPARE(workspaceHost.requests.size(), 2);
    QCOMPARE(workspaceHost.current, QString("character"));
    QCOMPARE(find<QTabWidget>("cutoutBottomTabs")->currentIndex(), 1);
    find<QTabWidget>("cutoutBottomTabs")->setCurrentIndex(0);
    // Switching, full view, splitters and the launcher never issued an edit.
    QCOMPARE(host.edits, edits);
    QCOMPARE(host.revision, revision);
  }

  /**
   * Large-rig editor timings (plugin 1.9.0): the generated 200-bone / 150-slot / 40-mesh / 30-clip
   * character (core/tests/stress-rig.hpp, the same one the core and player benchmarks use) loaded
   * into this host. Medians of a few runs; budgets are ~3x the development machine so a regression
   * of the refresh paths fails here. CUTOUT_PERF_OUT=<json> writes the numbers for the perf report.
   */
  void largeRigPerformance() {
    const QJsonArray keepDocuments = host.documents, keepAssets = host.assetList;
    const auto keepHistory = host.history, keepFuture = host.future;
    const QString keepRig = panel->rigId(), keepClip = panel->clipId();
    const auto keepMode = panel->mode();
    auto restore = qScopeGuard([&] {
      host.documents = keepDocuments; host.assetList = keepAssets; host.history = keepHistory; host.future = keepFuture; host.revision++;
      plugin.viewChanged();
      panel->selectRig(keepRig);
      panel->selectClip(keepClip);
      panel->setMode(keepMode);
      pump();
    });
    const Json rigData = cutout_stress::rig();
    // Direct Qt <-> core JSON conversion gives what the old text round trip gave (types included).
    {
      const QJsonObject qt = toQtObject(rigData);
      QCOMPARE(QJsonDocument(qt).toJson(QJsonDocument::Compact), QJsonDocument::fromJson(QByteArray::fromStdString(rigData.dump())).toJson(QJsonDocument::Compact));
      const Json back = toJson(qt), viaText = Json::parse(QJsonDocument(qt).toJson(QJsonDocument::Compact).toStdString());
      QVERIFY(back == viaText && back.dump() == viaText.dump());
      const Json mixed = {{"i", 3}, {"f", 2.5}, {"whole", 4.0}, {"neg", -0.0}, {"big", 1e20}, {"s", "한글"}, {"n", nullptr}, {"b", true}, {"a", Json::array({1, 2.25, Json::object()})}};
      QCOMPARE(toJson(toQtObject(mixed)).dump(), Json::parse(QJsonDocument(toQtObject(mixed)).toJson(QJsonDocument::Compact).toStdString()).dump());
      QVERIFY(toJson(toQtObject(mixed))["whole"].is_number_integer() && toJson(toQtObject(mixed))["f"].is_number_float());
    }
    // One small PNG per referenced image so the canvas draws real pictures (no external assets).
    for (const auto& id : cutout_stress::imageIds(rigData)) {
      const QString file = dir.filePath(qs(id) + ".png");
      if (!QFileInfo::exists(file)) {
        QImage img(32, 32, QImage::Format_ARGB32);
        img.fill(QColor::fromHsv(int(qHash(qs(id)) % 360), 140, 220));
        img.save(file);
      }
      host.assetList.append(QJsonObject{{"id", qs(id)}, {"kind", "image"}, {"path", "assets/" + qs(id) + ".png"}, {"absolutePath", file}});
    }
    auto envelope = [](const char* type, const char* id, const Json& data) {
      return QJsonObject{{"formatVersion", "0.1.0"}, {"plugin", "cutout-animation"}, {"type", type}, {"id", id}, {"data", toQtObject(data)}};
    };
    host.documents.append(QJsonObject{{"file", "plugins/cutout-animation/rig/stress.json"}, {"kind", "extension"}, {"data", envelope("rig", "stress", rigData)}});
    host.documents.append(QJsonObject{{"file", "plugins/cutout-animation/controller/stress-main.json"}, {"kind", "extension"}, {"data", envelope("controller", "stress-main", cutout_stress::controller())}});
    host.revision++;
    host.skipDiagnostics = true;
    auto keepDiagnostics = qScopeGuard([&] { host.skipDiagnostics = false; });
    // Median of `runs` timings of body() (setup() before each run is not timed).
    auto medianOf = [](int runs, const std::function<void()>& setup, const std::function<void()>& body) {
      QVector<double> ms;
      for (int i = 0; i < runs; i++) {
        setup();
        QElapsedTimer t;
        t.start();
        body();
        ms << t.nsecsElapsed() / 1e6;
      }
      std::sort(ms.begin(), ms.end());
      return ms[ms.size() / 2];
    };
    auto median = [&](int runs, const std::function<void()>& body) { return medianOf(runs, [] {}, body); };
    QJsonObject out;
    out["viewChangedFirstMs"] = median(1, [&] { plugin.viewChanged(); });
    panel->selectRig("stress");
    panel->setMode(CutoutPanel::Mode::Animate);
    panel->selectClip("clip-0");
    panel->selectItem("bone:head");
    pump();
    QCOMPARE(panel->rigId(), QString("stress"));
    // A view whose documents are what the panel shows (the reply to its own edit): light refresh.
    const int light = panel->lightRefreshes();
    out["viewChangedMs"] = medianOf(5, [&] { host.revision++; (void)host.view(); }, [&] { plugin.viewChanged(); });
    QVERIFY2(panel->lightRefreshes() - light >= 5, "unchanged documents refresh incrementally");
    // A changed rig (another client / undo): conversion, parse and every widget rebuilt.
    int bump = 0, full = 0;
    out["fullRefreshMs"] = medianOf(3, [&] {
      for (int i = 0; i < host.documents.size(); i++) {
        QJsonObject doc = host.documents[i].toObject(), env = doc.value("data").toObject();
        if (env.value("id") != "stress" || env.value("type") != "rig") continue;
        QJsonObject data = env.value("data").toObject();
        data["name"] = QString("Stress %1").arg(++bump);
        env["data"] = data;
        doc["data"] = env;
        host.documents[i] = doc;
      }
      host.revision++;
      (void)host.view();
      full = panel->fullRefreshes();
    }, [&] { plugin.viewChanged(); });
    QVERIFY(panel->fullRefreshes() == full + 1);
    out["canvasRepaintMs"] = median(9, [&] { panel->canvas()->repaint(); });
    int keys = 0;
    for (const auto& row : panel->dopesheet()->rows()) keys += row.times.size();
    out["dopesheetKeys"] = keys;
    QVERIFY2(keys >= 3000, qPrintable(QString::number(keys)));
    out["dopesheetRepaintMs"] = median(9, [&] { panel->dopesheet()->repaint(); });
    out["graphRepaintMs"] = median(9, [&] { panel->graph()->repaint(); });
    // Edit gestures: the core op, the host edit and the refresh it causes (one edit each).
    QVector<KeyRef> refs;
    QVector<double> moveMs, keyMs;
    for (int t = 0; t < 25; t++)
      for (int k = 0; k < 20; k++) refs.push_back({qs("bone:" + cutout_stress::boneName({}, t)), "rotation", 200 * k});
    // Timed: the gesture until its edit is applied and the panel shows it (the reply's refresh
    // included). Checked after each run, untimed: exactly one accepted edit, documents valid.
    int sign = 1, editsBefore = 0;
    auto checkEdit = [&](const char* what) {
      QVERIFY2(host.edits == editsBefore + 1 && host.lastError.isEmpty(), what);
      for (const auto& d : host.diagnostics()) QVERIFY2(d.toObject().value("severity") != "error", what);
      gestures++;
    };
    for (int run = 0; run < 3; run++) {
      editsBefore = host.edits;
      QElapsedTimer t;
      t.start();
      QVERIFY(panel->moveKeys(refs, 10 * sign));
      pump();
      moveMs << t.nsecsElapsed() / 1e6;
      checkEdit("move 500 keys");
      for (auto& r : refs) r.timeMs += 10 * sign;
      sign = -sign;
    }
    panel->setAutoKey(true);
    double value = 10;
    for (int run = 0; run < 3; run++) {
      editsBefore = host.edits;
      QElapsedTimer t;
      t.start();
      setSpin("cutoutPropRotation", value += 3);
      pump();
      keyMs << t.nsecsElapsed() / 1e6;
      checkEdit("key head rotation");
    }
    std::sort(moveMs.begin(), moveMs.end());
    std::sort(keyMs.begin(), keyMs.end());
    out["moveKeys500Ms"] = moveMs[1];
    out["setKeyMs"] = keyMs[1];
    qInfo("large rig: %s", QJsonDocument(out).toJson(QJsonDocument::Compact).constData());
    if (const QString file = qEnvironmentVariable("CUTOUT_PERF_OUT"); !file.isEmpty()) {
      QFile f(file);
      QVERIFY(f.open(QIODevice::WriteOnly));
      f.write(QJsonDocument(out).toJson());
    }
    // ~3x the medians of the development machine (plugin 1.9.0, offscreen): before the
    // incremental refresh a single key edit took ~450 ms and every view ~205 ms.
    const QList<QPair<const char*, double>> budgets{{"viewChangedMs", 10}, {"fullRefreshMs", 200}, {"canvasRepaintMs", 30}, {"dopesheetRepaintMs", 30},
                                                    {"graphRepaintMs", 10}, {"moveKeys500Ms", 300}, {"setKeyMs", 300}};
    for (const auto& [name, budget] : budgets)
      QVERIFY2(out.value(name).toDouble() <= budget, qPrintable(QString("%1 = %2 ms > budget %3 ms").arg(name).arg(out.value(name).toDouble()).arg(budget)));
  }

  void screenshot() {
    const QString path = qEnvironmentVariable("CUTOUT_STUDIO_SCREENSHOT");
    if (path.isEmpty()) QSKIP("set CUTOUT_STUDIO_SCREENSHOT=<png> to capture the panel");
    panel->setMode(CutoutPanel::Mode::Animate);
    panel->selectClip("joy");
    panel->setPlayhead(300);
    panel->selectItem("bone:head");
    find<QTabWidget>("cutoutBottomTabs")->setCurrentIndex(qEnvironmentVariableIntValue("CUTOUT_STUDIO_TAB"));
    pump();
    QVERIFY(host.mainWindow.grab().save(path));
  }

  void summary() {
    // Final documents round-trip through the core parser and every recorded gesture was checked.
    for (const auto& d : cutout::validateRig(rig())) QVERIFY2(d.severity != "error", d.message.c_str());
    const auto parsed = cutout::parseRig("hero", rig());
    QVERIFY(cutout::validateController(controller(), &parsed).empty());
    qInfo("gestures checked: %d, edit requests: %d, imports: %d, documents: %d, revision: %d", gestures, host.edits, host.imports, int(host.documents.size()), host.revision);
  }
};

QTEST_MAIN(StudioTests)
#include "studio-tests.moc"
