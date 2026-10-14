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
    return {{"revision", revision}, {"documents", documents}, {"diagnostics", diagnostics()}, {"undo", !history.isEmpty()}, {"redo", !future.isEmpty()}, {"dirty", !history.isEmpty()}};
  }
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

    // Pose copy/paste: head + its child arm at joy 600 ms become keys at timing 4000 ms.
    panel->selectItem("bone:head");
    panel->setPlayhead(600);
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
    nextPart = CutoutPanel::PartChoice{"img-eye-open", QString(), "eyes", "closed", QString()};
    QVERIFY(gesture("assign closed-eye image", 1, [&] { click("cutoutSkinAssign"); }));
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
    panel->dopesheet()->onRowSelected(rowOf("ik:elbow-ik", "mix"));
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
    for (const QSize size : {QSize(1280, 720), QSize(3840, 1080), QSize(5120, 1440), QSize(1080, 1920)}) {
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
      // Side panels keep their width on wide screens: the canvas gets the extra space.
      if (size.width() >= 3840) {
        if (sideWidth < 0) sideWidth = panel->leftPane()->width();
        QCOMPARE(panel->leftPane()->width(), sideWidth);
        QVERIFY2(panel->canvas()->width() > size.width() - 700, qPrintable(QString::number(panel->canvas()->width())));
      }
    }
    host.mainWindow.resize(1700, 1200);
    pump();
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
