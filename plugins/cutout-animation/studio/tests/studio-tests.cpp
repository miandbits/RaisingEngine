// Drives the real authoring panel through a MockStudioHost (in-memory documents, edit
// create/replace/delete/batch with revision++ and undo/redo, importImage stub).
// Every gesture must issue exactly one edit and leave documents valid for the core.
#include <QtTest>
#include <QtWidgets>
#include <functional>
#include "cutout-plugin.hpp"
#include "cutout/editing.hpp"
#include "dopesheet.hpp"

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

class StudioTests : public QObject {
  Q_OBJECT
  MockHost host;
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
  int rowOf(const QString& target, const QString& property) const {
    const auto& rows = panel->dopesheet()->rows();
    for (int i = 0; i < rows.size(); i++) if (rows[i].target == target && rows[i].property == property) return i;
    return -1;
  }

 private Q_SLOTS:
  void initTestCase() {
    QVERIFY(dir.isValid());
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
