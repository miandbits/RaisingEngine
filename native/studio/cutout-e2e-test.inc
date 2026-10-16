// End-to-end GUI scenario for the cutout animation plugin (real StudioWindow, real
// service.mjs, real cutout-studio.dll, real exported player). Every authoring step goes
// through visible widgets: buttons are clicked, values are typed, menus are opened and
// modal dialogs (input / message / file / part / plugin manager) are answered by polling
// QApplication::activeModalWidget(). Combo boxes are set with setCurrentIndex + activated
// (the signal a user selection emits). Places where no widget path was available are
// recorded in `hooks` and written to the evidence folder.
//
// Requires RAISING_PLUGIN_STORE (isolated store) and the built package in dist/plugins.
#include "canvas.hpp"
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QHeaderView>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QTabBar>
#include <QTreeWidgetItemIterator>

namespace cutoutE2e {
const QString RIG_FILE = "plugins/cutout-animation/rig/hero.json";
const QString CONTROLLER_FILE = "plugins/cutout-animation/controller/hero.json";
const QString WORKSPACE = "cutout-animation/character";

bool waitFor(const std::function<bool()> &condition, int ms = 20000) {
  QElapsedTimer clock;
  clock.start();
  while (!condition()) {
    if (clock.elapsed() > ms) return false;
    QTest::qWait(20);
  }
  return true;
}

/**
 * Polls for the first application-modal widget accepted by `match` and runs `act` on it.
 * Timers are owned by `owner` and never deleted from their own slot (QTest::qWait inside
 * `act` flushes deferred deletes). On timeout the active modal is rejected so the
 * blocking gesture returns and the test can fail instead of hanging.
 */
void onModal(QObject *owner, QStringList *errors, const QString &what, std::function<bool(QWidget *)> match,
             std::function<void(QWidget *)> act, int timeoutMs = 25000) {
  auto timer = new QTimer(owner);
  auto clock = std::make_shared<QElapsedTimer>();
  auto done = std::make_shared<bool>(false);
  clock->start();
  timer->setInterval(25);
  QObject::connect(timer, &QTimer::timeout, owner, [=] {
    if (*done) return;
    QWidget *modal = QApplication::activeModalWidget();
    if (modal && modal->isVisible() && match(modal)) {
      *done = true;
      timer->stop();
      QTest::qWait(60);  // let the dialog finish showing (focus, default button)
      act(modal);
      return;
    }
    if (clock->elapsed() > timeoutMs) {
      *done = true;
      timer->stop();
      errors->append("timeout waiting for " + what);
      if (auto dialog = qobject_cast<QDialog *>(modal)) dialog->reject();
      else if (modal) modal->close();
    }
  });
  timer->start();
}
bool isInput(QWidget *w) { return qobject_cast<QInputDialog *>(w) != nullptr; }
bool isMessage(QWidget *w) { return qobject_cast<QMessageBox *>(w) != nullptr; }
bool isFileDialog(QWidget *w) { return qobject_cast<QFileDialog *>(w) != nullptr; }

/**
 * Key press/release per character carrying the character as event text (QTest::keyClicks
 * only knows a subset of ASCII and asserts on Hangul or path characters).
 */
void typeText(QWidget *target, const QString &text) {
  for (const QChar ch : text) {
    const int key = ch.isLetterOrNumber() && ch.unicode() < 128 ? ch.toUpper().unicode() : 0;
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, QString(ch));
    QApplication::sendEvent(target, &press);
    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, QString(ch));
    QApplication::sendEvent(target, &release);
  }
}
/** Types into a line edit like a user (select all, keys, optional Return). */
void typeInto(QLineEdit *edit, const QString &text, bool enter = true) {
  edit->setFocus();
  edit->selectAll();
  typeText(edit, text);
  if (enter) QTest::keyClick(edit, Qt::Key_Return);
}
void typeSpin(QAbstractSpinBox *spin, const QString &text) {
  spin->setFocus();
  spin->selectAll();
  typeText(spin, text);
  QTest::keyClick(spin, Qt::Key_Return);
}
/** A user selection in a combo box: current index plus the activated() signal it emits. */
bool choose(QComboBox *combo, const QString &value) {
  if (!combo) return false;
  int index = combo->findData(value);
  if (index < 0) index = combo->findText(value);
  if (index < 0) return false;
  combo->setFocus();
  combo->setCurrentIndex(index);
  emit combo->activated(index);
  return true;
}
void answerText(QWidget *dialog, const QString &text) {
  auto edit = dialog->findChild<QLineEdit *>();
  typeInto(edit, text, false);
  QTest::mouseClick(dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok), Qt::LeftButton);
}
void answerYes(QWidget *dialog) {
  QTest::mouseClick(static_cast<QMessageBox *>(dialog)->button(QMessageBox::Yes), Qt::LeftButton);
}
/** Non-native QFileDialog: type the path in the file name field and press the accept button. */
void chooseFile(QWidget *dialog, const QString &path, QStringList *hooks) {
  auto fd = static_cast<QFileDialog *>(dialog);
  auto edit = fd->findChild<QLineEdit *>("fileNameEdit");
  auto box = fd->findChild<QDialogButtonBox *>("buttonBox");
  QPushButton *ok = box ? box->button(QDialogButtonBox::Open) : nullptr;
  if (!ok && box) ok = box->button(QDialogButtonBox::Save);
  if (edit) edit->setText(QDir::toNativeSeparators(path));
  QTest::qWait(50);
  if (edit && ok && ok->isEnabled()) { QTest::mouseClick(ok, Qt::LeftButton); return; }
  hooks->append("QFileDialog accepted with selectFile()+accept() (accept button unavailable): " + path);
  fd->selectFile(path);
  static_cast<QDialog *>(fd)->accept();
}

bool openMenu(QMenu *menu) {
  if (menu->isVisible()) return true;
  QAction *entry = menu->menuAction();
  for (QObject *o : entry->associatedObjects()) {
    if (auto bar = qobject_cast<QMenuBar *>(o)) {
      const QRect r = bar->actionGeometry(entry);
      if (!r.isValid()) return false;
      QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, r.center());
      return waitFor([menu] { return menu->isVisible(); }, 2000);
    }
    if (auto parent = qobject_cast<QMenu *>(o)) {
      if (!openMenu(parent)) return false;
      const QRect r = parent->actionGeometry(entry);
      QTest::mouseClick(parent, Qt::LeftButton, Qt::NoModifier, r.center());
      return waitFor([menu] { return menu->isVisible(); }, 2000);
    }
  }
  return false;
}
/** Opens the action's menu from the menu bar and clicks the entry; falls back to trigger(). */
void menuClick(QAction *action, QStringList *hooks, const QString &label) {
  QMenu *menu = nullptr;
  for (QObject *o : action->associatedObjects())
    if (auto m = qobject_cast<QMenu *>(o)) { menu = m; break; }
  if (menu && openMenu(menu)) {
    const QRect r = menu->actionGeometry(action);
    if (r.isValid()) {
      QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, r.center());
      return;
    }
  }
  while (auto popup = QApplication::activePopupWidget()) popup->close();
  hooks->append(label + ": QAction::trigger() because its menu could not be opened by mouse");
  action->trigger();
}
/** Clicks the tool bar button of an action (falls back to its menu entry). */
void toolClick(QAction *action, QStringList *hooks, const QString &label) {
  for (QObject *o : action->associatedObjects())
    if (auto button = qobject_cast<QToolButton *>(o))
      if (button->isVisible() && button->isEnabled()) { QTest::mouseClick(button, Qt::LeftButton); return; }
  menuClick(action, hooks, label);
}

/** Clicks a tab of the host workspace switcher (menu bar corner) and waits for the switch. */
bool switchWorkspace(StudioWindow *w, const QString &key) {
  auto bar = w->findChild<QTabBar *>("studioWorkspaceSwitcher");
  if (!bar || !bar->isVisible()) return false;
  for (int i = 0; i < bar->count(); ++i)
    if (bar->tabData(i).toString() == key) QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, bar->tabRect(i).center());
  return waitFor([&] { return w->currentWorkspace() == key; }, 3000);
}
QStringList visibleDocks(StudioWindow *w) {
  QStringList out;
  for (auto d : w->findChildren<QDockWidget *>(Qt::FindDirectChildrenOnly))
    if (d->isVisible()) out << d->objectName();
  out.sort();
  return out;
}
QJsonObject pluginData(const QJsonObject &view, const QString &file) {
  for (auto d : view.value("documents").toArray())
    if (d.toObject().value("file") == file) return d.toObject().value("data").toObject().value("data").toObject();
  return {};
}
QJsonObject byId(const QJsonArray &list, const QString &id, const QString &key = "id") {
  for (auto v : list)
    if (v.toObject().value(key) == id) return v.toObject();
  return {};
}
QJsonObject attachment(const QJsonObject &rig, const QString &slot, const QString &name) {
  for (auto v : rig.value("attachments").toArray())
    if (v.toObject().value("slot") == slot && v.toObject().value("name") == name) return v.toObject();
  return {};
}
QStringList slotOrder(const QJsonObject &rig) {
  QStringList out;
  for (auto v : rig.value("slots").toArray()) out << v.toObject().value("id").toString();
  return out;
}
QJsonObject keyAt(const QJsonObject &rig, const QString &clip, const QString &target, const QString &property, int ms) {
  for (auto t : byId(rig.value("clips").toArray(), clip).value("tracks").toArray())
    if (t.toObject().value("target") == target && t.toObject().value("property") == property)
      for (auto k : t.toObject().value("keys").toArray())
        if (k.toObject().value("timeMs").toInt() == ms) return k.toObject();
  return {};
}
/** Rig canvas fit mapping (RigCanvas::fitToCanvas): rig space -> widget pixels. */
QPoint rigPoint(QWidget *canvas, double x, double y, double rw = 512, double rh = 768) {
  const double zoom = std::max(0.02, std::min((canvas->width() - 40) / rw, (canvas->height() - 40) / rh));
  const QPointF pan(canvas->width() / 2.0 - rw * zoom / 2, canvas->height() / 2.0 - rh * zoom / 2);
  return QPointF(x * zoom + pan.x(), y * zoom + pan.y()).toPoint();
}
void sendMove(QWidget *w, const QPoint &p) {
  QMouseEvent move(QEvent::MouseMove, QPointF(p), w->mapToGlobal(QPointF(p)), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
  QApplication::sendEvent(w, &move);
}
void drag(QWidget *w, const QPoint &from, const QPoint &to) {
  QTest::mousePress(w, Qt::LeftButton, Qt::NoModifier, from);
  for (int i = 1; i <= 12; ++i) { sendMove(w, from + (to - from) * i / 12); QTest::qWait(8); }
  QTest::mouseRelease(w, Qt::LeftButton, Qt::NoModifier, to);
}
int tableRow(QTableWidget *table, const QString &id) {
  for (int r = 0; r < table->rowCount(); ++r)
    if (table->item(r, 0) && table->item(r, 0)->text() == id) return r;
  return -1;
}
QJsonArray readSteps(const QString &file) {
  QFile f(file);
  if (!f.open(QIODevice::ReadOnly)) return {};
  return QJsonDocument::fromJson(f.readAll()).object().value("steps").toArray();
}
QJsonObject checkpoint(const QJsonArray &steps, const QString &id) {
  for (auto s : steps)
    if (s.toObject().value("command").toObject().value("checkpoint") == id) return s.toObject().value("ui").toObject();
  return {};
}
QJsonObject instanceIn(const QJsonObject &ui, const QString &instance) {
  for (auto v : ui.value("extensions").toObject().value("instances").toArray())
    if (v.toObject().value("instance") == instance) return v.toObject();
  return {};
}
QJsonObject trackOf(const QJsonObject &instance, int track) {
  for (auto v : instance.value("state").toObject().value("tracks").toArray())
    if (v.toObject().value("track").toInt() == track) return v.toObject();
  return {};
}
/**
 * Minimal layered PSD (8-bit RGB, raw channels) for the PSD import step (plugin 1.11.0): a body,
 * a "head" group (section dividers) with a face and two eye variants, the closed one hidden.
 * Record layout as in Adobe's file format spec; the service's reader is tested far more widely
 * in tests/plugins/cutout-layer-import.test.mjs.
 */
QByteArray layeredPsd(int width, int height) {
  struct Layer { QString name; QRect rect; QColor color; bool hidden; int section; };  // section: 0 layer, 3 divider, 1 folder
  const QList<Layer> layers{{"body", QRect(60, 140, 80, 160), QColor(200, 60, 40), false, 0},
                            {"</Layer group>", QRect(), QColor(), false, 3},
                            {"face", QRect(70, 40, 60, 70), QColor(240, 200, 160), false, 0},
                            {"eyes/closed", QRect(80, 72, 40, 6), QColor(10, 10, 10), true, 0},
                            {"eyes/open", QRect(80, 66, 40, 12), QColor(30, 30, 200), false, 0},
                            {"head", QRect(), QColor(), false, 1}};
  QByteArray records, channels;
  QDataStream r(&records, QIODevice::WriteOnly), c(&channels, QIODevice::WriteOnly);
  for (const auto &l : layers) {
    const int w = l.rect.width(), h = l.rect.height();
    r << qint32(l.rect.top()) << qint32(l.rect.left()) << qint32(l.rect.top() + h) << qint32(l.rect.left() + w) << quint16(4);
    for (qint16 id : {qint16(-1), qint16(0), qint16(1), qint16(2)}) r << id << quint32(2 + w * h);
    r.writeRawData("8BIM", 4);
    r.writeRawData(l.section == 1 ? "pass" : "norm", 4);
    r << quint8(255) << quint8(0) << quint8(l.hidden ? 2 : 0) << quint8(0);
    QByteArray name = l.name.toLatin1(), extra;
    QDataStream e(&extra, QIODevice::WriteOnly);
    e << quint32(0) << quint32(0) << quint8(name.size());
    e.writeRawData(name.constData(), int(name.size()));
    for (int pad = (4 - (1 + int(name.size())) % 4) % 4; pad > 0; pad--) e << quint8(0);
    if (l.section) { e.writeRawData("8BIM", 4); e.writeRawData("lsct", 4); e << quint32(4) << quint32(l.section); }
    r << quint32(extra.size());
    r.writeRawData(extra.constData(), int(extra.size()));
    for (int channel : {3, 0, 1, 2}) {
      c << quint16(0);
      const int value = channel == 3 ? 255 : channel == 0 ? l.color.red() : channel == 1 ? l.color.green() : l.color.blue();
      for (int i = 0; i < w * h; i++) c << quint8(value);
    }
  }
  QByteArray info;
  QDataStream i(&info, QIODevice::WriteOnly);
  i << qint16(-int(layers.size()));
  i.writeRawData(records.constData(), int(records.size()));
  i.writeRawData(channels.constData(), int(channels.size()));
  if (info.size() % 2) i << quint8(0);
  QByteArray out;
  QDataStream o(&out, QIODevice::WriteOnly);
  o.writeRawData("8BPS", 4);
  o << quint16(1) << quint32(0) << quint16(0) << quint16(3) << quint32(height) << quint32(width) << quint16(8) << quint16(3) << quint32(0) << quint32(0);
  o << quint32(4 + info.size() + 4) << quint32(info.size());
  o.writeRawData(info.constData(), int(info.size()));
  o << quint32(0) << quint16(0);  // no global mask info; raw merged image (unused by the import)
  for (int k = 0; k < 3 * width * height; k++) o << quint8(255);
  return out;
}
bool copyTree(const QString &from, const QString &to) {
  QDir source(from);
  if (!QDir().mkpath(to)) return false;
  for (const auto &info : source.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)) {
    const auto target = to + "/" + info.fileName();
    if (info.isDir()) { if (!copyTree(info.absoluteFilePath(), target)) return false; }
    else if (!QFile::copy(info.absoluteFilePath(), target)) return false;
  }
  return true;
}
}  // namespace cutoutE2e

void StudioTests::cutoutEndToEnd() {
  using namespace cutoutE2e;
  const auto engine = qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath());
  const auto store = QDir::fromNativeSeparators(qEnvironmentVariable("RAISING_PLUGIN_STORE"));
  if (store.isEmpty()) QSKIP("Set RAISING_PLUGIN_STORE to an isolated plugin store for the cutout end-to-end scenario");
  // Package name follows plugin.json so a version bump never leaves the scenario on a stale build.
  const auto manifestVersion = [&] {
    QFile manifest(engine + "/plugins/cutout-animation/plugin.json");
    return manifest.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(manifest.readAll()).object().value("version").toString() : QString();
  }();
  const auto package = engine + "/dist/plugins/cutout-animation-" + manifestVersion + ".raiseplugin";
  QVERIFY2(QFileInfo::exists(package), qPrintable(package));
  const auto parts = engine + "/plugins/cutout-animation/demo/parts";
  const auto stamp = QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss");
  const auto work = engine + "/.cache/anim-e2e";
  const auto projectRoot = work + "/project-" + stamp;
  const auto exportParent = work + "/export-" + stamp;
  const auto evidence = engine + "/.cache/anim-evidence/e2e";
  QVERIFY(QDir().mkpath(evidence));
  QVERIFY(QDir().mkpath(exportParent));
  QVERIFY(!QFileInfo::exists(projectRoot));
  // Qt's own file dialog (not the Win32 one) so the scenario can answer it with widgets.
  QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
  auto restoreDialogs = qScopeGuard([] { QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, false); });
  QObject pollers;
  const bool trace = qEnvironmentVariableIsSet("CUTOUT_E2E_TRACE");
  QStringList errors, hooks, log;
  hooks << "Qt::AA_DontUseNativeDialogs set so QFileDialog is a Qt widget dialog (answered by typing the path + accept button)";
  hooks << "Combo selections: setCurrentIndex + activated(index) signal (what a user pick emits)";
  hooks << "Play: StudioWindow::playAutomation/playReport test properties make the real Play pass --automation/--report to RaisingPlayer.exe";
  hooks << "Rules dialog (common action): confirmation checkbox/effect row set with widget APIs (setChecked/click/setValue) like logicAuthoringAndDebug";
  auto step = [&](const QString &text) { log << QDateTime::currentDateTime().toString("hh:mm:ss.zzz ") + text; qInfo("%s", qPrintable(text)); };

  std::unique_ptr<StudioWindow> owner = std::make_unique<StudioWindow>(engine);
  StudioWindow *w = owner.get();
  w->resize(1760, 1080);
  w->show();
  QVERIFY(QTest::qWaitForWindowExposed(w));
  auto settle = [&](int ms = 30000) { const bool ok = waitFor([&] { return !w->busy(); }, ms); QTest::qWait(40); return ok && !w->busy(); };
  auto rig = [&] { return pluginData(w->currentView(), RIG_FILE); };
  auto controller = [&] { return pluginData(w->currentView(), CONTROLLER_FILE); };
  auto shot = [&](const QString &name) { QTest::qWait(250); const bool ok = w->grab().save(evidence + "/" + name); log << "evidence " + name; return ok; };
  auto noErrors = [&] { return errors.isEmpty() ? QString() : errors.join("; "); };

  // ---- 1. fresh project from the 탐험·전투 template, through 파일 → 새 프로젝트 ------------------
  step("1 new project");
  onModal(&pollers, &errors, "new project dialog", [](QWidget *m) { return m->objectName() == "newProjectDialog"; }, [&](QWidget *d) {
    auto base = d->findChild<QComboBox *>("newProjectTemplate");
    base->setCurrentIndex(base->findData("academy-adventure"));
    typeInto(d->findChild<QLineEdit *>("newProjectDestination"), projectRoot, false);
    QTest::mouseClick(d->findChild<QDialogButtonBox *>("newProjectButtons")->button(QDialogButtonBox::Ok), Qt::LeftButton);
  });
  menuClick(w->findChild<QAction *>("newProject"), &hooks, "새 프로젝트");
  QVERIFY2(waitFor([&] { return !w->busy() && QDir::cleanPath(QDir::fromNativeSeparators(w->currentView().value("root").toString())).compare(QDir::cleanPath(projectRoot), Qt::CaseInsensitive) == 0; }, 40000),
           qPrintable(w->lastError + noErrors()));
  QVERIFY(settle());
  QVERIFY(QFileInfo::exists(projectRoot + "/project.json"));

  // Common action "응원하기" (money +5, no confirmation) through 정책 · 행동 작업실.
  QString actionId;
  onModal(&pollers, &errors, "rules dialog", [](QWidget *m) { return m->objectName() == "rulesDialog"; }, [&](QWidget *d) {
    // A missing widget (renamed / not an editable row) fails the scenario instead of crashing it.
    auto missing = [&](const char *what) { errors.append(QString("rules dialog: missing ") + what); static_cast<QDialog *>(d)->reject(); };
    auto kind = d->findChild<QComboBox *>("ruleKind");
    auto add = d->findChild<QPushButton *>("addRule");
    if (!kind || !add) return missing("ruleKind/addRule");
    kind->setCurrentIndex(kind->findData("actions"));
    QTest::mouseClick(add, Qt::LeftButton);
    auto id = d->findChild<QLineEdit *>("ruleId");
    auto name = d->findChild<QLineEdit *>("ruleName");
    if (!id || !name) return missing("ruleId/ruleName");
    actionId = id->text();
    typeInto(name, QString::fromUtf8("응원하기"), false);
    // Widgets on the (possibly hidden) confirmation/effects pages: widget APIs like the logic test.
    auto confirm = d->findChild<QCheckBox *>("ruleConfirm-enabled");
    auto addEffect = d->findChild<QPushButton *>("addRule-effects");
    if (!confirm || !addEffect) return missing("ruleConfirm-enabled/addRule-effects");
    confirm->setChecked(false);
    addEffect->click();
    auto table = d->findChild<QTableWidget *>("rule-effects");
    if (!table || table->rowCount() != 1) return missing("rule-effects row");
    auto attribute = qobject_cast<QComboBox *>(table->cellWidget(0, 0));
    auto amount = qobject_cast<QSpinBox *>(table->cellWidget(0, 2));
    if (!attribute || !amount) return missing("editable effect row (attribute combo / amount spin)");
    attribute->setCurrentIndex(attribute->findData("money"));
    amount->setValue(5);
    auto buttons = d->findChild<QDialogButtonBox *>();
    if (!buttons || !buttons->button(QDialogButtonBox::Save)) return missing("Save button");
    QTest::mouseClick(buttons->button(QDialogButtonBox::Save), Qt::LeftButton);
  });
  menuClick(w->findChild<QAction *>("editRules"), &hooks, "정책 · 행동 작업실");
  QVERIFY(settle());
  QVERIFY2(!actionId.isEmpty() && errors.isEmpty(), qPrintable(noErrors()));
  const auto action = byId(w->projectData().value("gameplay").toObject().value("actions").toArray(), actionId);
  QCOMPARE(action.value("name").toString(), QString::fromUtf8("응원하기"));
  QCOMPARE(action.value("effects").toArray().size(), 1);

  // Button on 홈 that runs it.
  toolClick(w->findChild<QAction *>("addButton"), &hooks, "＋ 버튼");
  QVERIFY(waitFor([&] { return !w->busy() && w->selectedId().startsWith("button-"); }));
  const auto buttonId = w->selectedId();
  auto element = [&](const QString &id) {
    for (auto s : w->projectData().value("visual").toObject().value("screens").toArray())
      if (s.toObject().value("id") == "home") return byId(s.toObject().value("elements").toArray(), id);
    return QJsonObject();
  };
  QVERIFY2(!element(buttonId).isEmpty(), "button must be on the home screen");
  typeInto(w->findChild<QLineEdit *>("buttonText"), QString::fromUtf8("응원하기"));
  QVERIFY(settle());
  QVERIFY(choose(w->findChild<QComboBox *>("buttonAction"), "executeAction"));
  QVERIFY(settle());
  QVERIFY(choose(w->findChild<QComboBox *>("actionTarget"), actionId));
  QVERIFY(settle());
  for (auto [name, value] : QList<QPair<const char *, QString>>{{"elementX", "60"}, {"elementY", "624"}, {"elementWidth", "260"}, {"elementHeight", "52"}}) {
    typeSpin(w->findChild<QSpinBox *>(name), value);
    QVERIFY2(settle(), name);
  }
  QCOMPARE(element(buttonId).value("action").toString(), QString("executeAction"));
  QCOMPARE(element(buttonId).value("actionId").toString(), actionId);
  QCOMPARE(element(buttonId).value("y").toInt(), 624);

  // ---- 2. 플러그인 관리: install the package and use it in this project ----------------------
  step("2 plugin manager");
  bool managed = false;
  QString managerStatus;
  onModal(&pollers, &errors, "plugin manager", [](QWidget *m) { return m->objectName() == "pluginManager"; }, [&](QWidget *d) {
    auto table = d->findChild<QTableWidget *>("pluginTable");
    onModal(&pollers, &errors, "package file dialog", isFileDialog, [&](QWidget *f) { chooseFile(f, package, &hooks); });
    QTest::mouseClick(d->findChild<QPushButton *>("pluginInstall"), Qt::LeftButton);
    if (!waitFor([&] { return !w->busy() && tableRow(table, "cutout-animation") >= 0; }, 60000)) { errors << "install did not list the package: " + w->lastError; static_cast<QDialog *>(d)->reject(); return; }
    const int row = tableRow(table, "cutout-animation");
    table->scrollToItem(table->item(row, 0));
    QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, table->visualItemRect(table->item(row, 0)).center());
    QTest::mouseClick(d->findChild<QPushButton *>("pluginEnable"), Qt::LeftButton);
    if (!waitFor([&] { const int r = tableRow(table, "cutout-animation"); return !w->busy() && r >= 0 && table->item(r, 3)->text().contains(QString::fromUtf8("편집 도구 실행 중")); }, 60000)) {
      errors << "enable did not load the authoring DLL: " + w->lastError + " / " + d->findChild<QPlainTextEdit *>("pluginDiagnostics")->toPlainText();
    }
    managerStatus = d->findChild<QLabel *>("pluginManagerStatus")->text();
    QTest::qWait(200);
    d->grab().save(evidence + "/05-plugin-manager.png");
    managed = true;
    QTest::mouseClick(d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Close), Qt::LeftButton);
  });
  menuClick(w->findChild<QAction *>("pluginManagerAction"), &hooks, "플러그인 관리");
  QVERIFY(settle());
  QVERIFY2(managed && errors.isEmpty(), qPrintable(noErrors()));
  // The service honours RAISING_PLUGIN_STORE: the store shown and the installed directory are inside it.
  QVERIFY2(QDir::fromNativeSeparators(managerStatus).contains(store, Qt::CaseInsensitive), qPrintable(managerStatus));
  QVERIFY2(QDir::fromNativeSeparators(w->pluginManager()->projectEntry("cutout-animation").value("directory").toString()).startsWith(store, Qt::CaseInsensitive),
           qPrintable(w->pluginManager()->projectEntry("cutout-animation").value("directory").toString()));
  QVERIFY(w->pluginManager()->loadedIds().contains("cutout-animation"));
  // The dock is only a launcher now; the panel is the full-window animation workspace.
  auto dock = w->findChild<QDockWidget *>("cutoutAnimationDock");
  QVERIFY(dock && dock->findChild<QWidget *>("cutoutWorkspaceLauncher") && !dock->findChild<QWidget *>("cutoutPanel"));
  const auto screenDocks = visibleDocks(w);
  {
    auto open = dock->findChild<QPushButton *>("cutoutOpenWorkspace");
    QVERIFY(open);
    if (open->isVisible()) QTest::mouseClick(open, Qt::LeftButton);
    else QVERIFY(switchWorkspace(w, WORKSPACE));
  }
  QVERIFY(waitFor([&] { return w->currentWorkspace() == WORKSPACE; }, 3000));
  auto panel = w->findChild<QWidget *>("cutoutPanel");
  QVERIFY(panel && panel->isVisible());
  QVERIFY2(visibleDocks(w).isEmpty(), qPrintable(visibleDocks(w).join(",")));
  QTest::qWait(200);
  auto button = [&](const char *name) { return panel->findChild<QAbstractButton *>(name); };
  auto click = [&](const char *name) {
    if (trace) qInfo("  click %s", name);
    auto b = button(name);
    if (!b) { errors << QString("missing button ") + name; return; }
    // A check box only reacts on its indicator/label, not on the empty stretch of the widget.
    if (qobject_cast<QCheckBox *>(b)) QTest::mouseClick(b, Qt::LeftButton, Qt::NoModifier, QPoint(8, b->height() / 2));
    else QTest::mouseClick(b, Qt::LeftButton);
  };
  auto tree = panel->findChild<QTreeWidget *>("cutoutBoneTree");
  auto canvas = panel->findChild<QWidget *>("cutoutCanvas");
  auto status = panel->findChild<QLabel *>("cutoutStatus");
  auto selectionLabel = panel->findChild<QLabel *>("cutoutSelectionLabel");
  QVERIFY(tree && canvas && status && selectionLabel);
  auto treeItem = [&](const QString &ref) -> QTreeWidgetItem * {
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
      if ((*it)->data(0, Qt::UserRole).toString() == ref) return *it;
    return nullptr;
  };
  auto select = [&](const QString &ref) {
    if (trace) qInfo("  select %s", qPrintable(ref));
    auto item = treeItem(ref);
    if (!item) { errors << "tree item missing " + ref; return false; }
    tree->scrollToItem(item);
    const QRect r = tree->visualItemRect(item);
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(r.left() + 16, r.center().y()));
    // Panel label: "본 <id> · …" for bones, "조각 <id> · …" for parts.
    const auto prefix = (ref.startsWith("bone:") ? QString::fromUtf8("본 ") : QString::fromUtf8("조각 ")) + ref.section(':', 1) + " ";
    if (!waitFor([&] { return selectionLabel->text().startsWith(prefix); }, 3000)) { errors << "selection did not reach " + ref + " (label " + selectionLabel->text() + ")"; return false; }
    return true;
  };
  auto prop = [&](const char *name) { return panel->findChild<QDoubleSpinBox *>(QString("cutoutProp") + name); };
  auto setProp = [&](const char *name, const QString &value) {
    if (trace) qInfo("  prop %s = %s", name, qPrintable(value));
    auto spin = prop(name);
    if (!spin || !spin->isEnabled()) { errors << QString("property not editable: ") + name; return false; }
    typeSpin(spin, value);
    return settle();
  };
  auto ask = [&](const QString &what, const QString &answer) {
    onModal(&pollers, &errors, what, isInput, [answer](QWidget *d) { answerText(d, answer); });
  };
  auto confirm = [&](const QString &what) { onModal(&pollers, &errors, what, isMessage, [](QWidget *d) { answerYes(d); }); };
  auto attachments = panel->findChild<QComboBox *>("cutoutPropAttachment");

  // ---- 3. character: new, parts, hierarchy, pivots, positions, draw order, visibility ---------
  step("3 character setup");
  ask("character id", "hero");
  click("cutoutNewCharacter");
  QVERIFY2(waitFor([&] { return !w->busy() && !rig().isEmpty() && !controller().isEmpty(); }), qPrintable(status->text() + w->lastError + noErrors()));
  QVERIFY(settle());
  QVERIFY(waitFor([&] { return panel->findChild<QTreeWidget *>("cutoutCharacterList")->topLevelItemCount() == 1; }, 5000));
  auto addBone = [&](const QString &parent, const QString &id) {
    if (!select("bone:" + parent)) return false;
    ask("bone id " + id, id);
    click("cutoutAddBone");
    return waitFor([&] { return !w->busy() && !byId(rig().value("bones").toArray(), id).isEmpty(); }) && settle() &&
           waitFor([&] { return selectionLabel->text().contains(id); }, 3000);
  };
  auto bone = [&](const QString &id) { return byId(rig().value("bones").toArray(), id); };
  QVERIFY2(addBone("root", "body"), qPrintable(noErrors() + status->text()));
  QVERIFY(setProp("Y", "-80"));
  QCOMPARE(bone("body").value("y").toDouble(), -80.0);
  QCOMPARE(bone("body").value("parent").toString(), QString("root"));
  QVERIFY(addBone("body", "head"));
  QVERIFY(setProp("Y", "-170"));
  QVERIFY(addBone("body", "arm-left"));
  QVERIFY(setProp("X", "-42"));
  QVERIFY(setProp("Y", "-95"));
  QVERIFY(addBone("body", "arm-right"));
  QCOMPARE(bone("head").value("parent").toString(), QString("body"));
  QCOMPARE(bone("head").value("y").toDouble(), -170.0);
  QCOMPARE(bone("arm-left").value("x").toDouble(), -42.0);
  // Canvas drag (Move tool) of the arm-right bone handle from its default spot to the shoulder.
  click("cutoutToolSelect");
  click("cutoutFitView");
  QTest::qWait(100);
  {
    const auto before = bone("arm-right");
    QCOMPARE(before.value("y").toDouble(), -60.0);  // created 60px above the body origin (256,640)
    drag(canvas, rigPoint(canvas, 256, 580), rigPoint(canvas, 298, 488));
    QVERIFY2(waitFor([&] { return !w->busy() && bone("arm-right") != before; }), qPrintable(status->text()));
    QVERIFY(settle());
    const auto after = bone("arm-right");
    QVERIFY2(std::abs(after.value("x").toDouble() - 42) < 4 && std::abs(after.value("y").toDouble() + 152) < 4, qPrintable(QJsonDocument(after).toJson(QJsonDocument::Compact)));
  }
  // Parts: 13 PNGs through 이미지 조각 추가… → PNG 가져오기… (file dialog) on the chosen bone.
  auto addPart = [&](const QString &file, const QString &slot, const QString &name, const QString &boneId) {
    if (trace) qInfo("  part %s -> %s/%s on %s", qPrintable(file), qPrintable(slot), qPrintable(name), qPrintable(boneId));
    if (!select("bone:" + boneId)) return false;
    onModal(&pollers, &errors, "part dialog " + file, [](QWidget *m) { return m->objectName() == "cutoutPartDialog"; }, [&](QWidget *d) {
      d->findChild<QComboBox *>("cutoutPartSlot")->lineEdit()->setText(slot);
      typeInto(d->findChild<QLineEdit *>("cutoutPartName"), name, false);
      auto bones = d->findChild<QComboBox *>("cutoutPartBone");
      bones->setCurrentIndex(bones->findText(boneId));
      onModal(&pollers, &errors, "png file dialog " + file, isFileDialog, [&](QWidget *f) { chooseFile(f, parts + "/" + file, &hooks); });
      QTest::mouseClick(d->findChild<QPushButton *>("cutoutPartImport"), Qt::LeftButton);
    });
    click("cutoutAddPart");
    const bool ok = waitFor([&] { return !w->busy() && !attachment(rig(), slot, name.isEmpty() ? slot : name).isEmpty(); }, 40000) && settle();
    if (!ok) errors << "part not added " + file + " · " + status->text() + " · " + w->lastError;
    return ok && waitFor([&] { return selectionLabel->text().contains(slot); }, 3000);
  };
  // Attachment placement of the shown attachment (Setup mode): offsets and image pivot.
  auto showAttachment = [&](const QString &slot, const QString &name) {
    if (!select("slot:" + slot)) return false;
    if (attachments->currentData().toString() == name) return true;
    return choose(attachments, name) && waitFor([&] { return !w->busy() && byId(rig().value("slots").toArray(), slot).value("attachment") == name; }) && settle();
  };
  QVERIFY2(addPart("body.png", "body", "", "body"), qPrintable(noErrors()));
  QVERIFY(setProp("PivotY", "1"));
  QCOMPARE(attachment(rig(), "body", "body").value("pivotY").toDouble(), 1.0);
  QVERIFY(addPart("arm-left.png", "arm-left", "", "arm-left"));
  QVERIFY(addPart("arm-right.png", "arm-right", "", "arm-right"));
  QVERIFY(setProp("PivotY", "0.08"));
  QVERIFY(addPart("head.png", "head", "", "head"));
  QVERIFY(setProp("Y", "-70"));
  QVERIFY(addPart("hair-back.png", "hair-back", "", "head"));
  QVERIFY(setProp("Y", "-65"));
  QVERIFY(addPart("eyes-open.png", "eyes", "open", "head"));
  QVERIFY(setProp("Y", "-65"));
  QVERIFY(addPart("eyes-half.png", "eyes", "half", "head"));
  QVERIFY(showAttachment("eyes", "half"));
  QVERIFY(setProp("Y", "-65"));
  QVERIFY(addPart("eyes-closed.png", "eyes", "closed", "head"));
  QVERIFY(showAttachment("eyes", "closed"));
  QVERIFY(setProp("Y", "-65"));
  QVERIFY(showAttachment("eyes", "open"));
  QVERIFY(addPart("mouth-neutral.png", "mouth", "neutral", "head"));
  QVERIFY(setProp("Y", "-28"));
  QVERIFY(addPart("mouth-smile.png", "mouth", "smile", "head"));
  QVERIFY(showAttachment("mouth", "smile"));
  QVERIFY(setProp("Y", "-28"));
  QVERIFY(addPart("mouth-open.png", "mouth", "open", "head"));
  QVERIFY(showAttachment("mouth", "open"));
  QVERIFY(setProp("Y", "-28"));
  QVERIFY(showAttachment("mouth", "neutral"));
  QVERIFY(addPart("blush.png", "blush", "", "head"));
  QVERIFY(setProp("Y", "-47"));
  QVERIFY(addPart("hair-front.png", "hair-front", "", "head"));
  QVERIFY(setProp("Y", "-120"));
  QCOMPARE(attachment(rig(), "eyes", "closed").value("y").toDouble(), -65.0);
  QCOMPARE(attachment(rig(), "mouth", "open").value("y").toDouble(), -28.0);
  QCOMPARE(int(rig().value("attachments").toArray().size()), 13);
  // Pivot tool: click the shoulder so arm-left rotates there; the image must stay in place.
  {
    QVERIFY(select("bone:arm-left"));
    click("cutoutToolPivot");
    click("cutoutFitView");
    QTest::qWait(80);
    const auto before = bone("arm-left");
    const QPoint at = rigPoint(canvas, 214, 490);
    QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, at);
    QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, at);
    QVERIFY2(waitFor([&] { return !w->busy() && bone("arm-left") != before; }), qPrintable(status->text()));
    QVERIFY(settle());
    const auto after = bone("arm-left");
    // New origin at the shoulder (world 214,490 = body-local -42,-150); the attachment shifted by the opposite amount.
    QVERIFY2(std::abs(after.value("y").toDouble() + 150) < 3 && std::abs(after.value("x").toDouble() + 42) < 3, qPrintable(QJsonDocument(after).toJson(QJsonDocument::Compact)));
    QVERIFY(std::abs(attachment(rig(), "arm-left", "arm-left").value("y").toDouble() - 55) < 3);
    click("cutoutToolSelect");
  }
  // Draw order: hair-back to the very back, body in front of both arms.
  auto order = panel->findChild<QListWidget *>("cutoutDrawOrder");
  auto pickOrder = [&](const QString &slot) {
    for (int i = 0; i < order->count(); ++i)
      if (order->item(i)->data(Qt::UserRole).toString() == slot) {
        order->scrollToItem(order->item(i));
        QTest::mouseClick(order->viewport(), Qt::LeftButton, Qt::NoModifier, order->visualItemRect(order->item(i)).center());
        return waitFor([&] { return selectionLabel->text().contains(slot); }, 3000);
      }
    return false;
  };
  QCOMPARE(slotOrder(rig()), (QStringList{"body", "arm-left", "arm-right", "head", "hair-back", "eyes", "mouth", "blush", "hair-front"}));
  for (int i = 0; i < 4; ++i) { QVERIFY(pickOrder("hair-back")); click("cutoutDrawUp"); QVERIFY(settle()); }
  for (int i = 0; i < 2; ++i) { QVERIFY(pickOrder("body")); click("cutoutDrawDown"); QVERIFY(settle()); }
  QCOMPARE(slotOrder(rig()), (QStringList{"hair-back", "arm-left", "arm-right", "body", "head", "eyes", "mouth", "blush", "hair-front"}));
  // Editor-only eye toggle on the blush part: no document change.
  {
    const auto revision = w->currentView().value("revision");
    const auto data = rig();
    auto toggle = [&](Qt::CheckState wanted) {
      auto item = treeItem("slot:blush");
      if (!item) return false;
      tree->scrollToItem(item);
      const QRect r = tree->visualItemRect(item);
      const int x = tree->header()->sectionViewportPosition(1) + 11;
      QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(x, r.center().y()));
      if (waitFor([&] { auto i = treeItem("slot:blush"); return i && i->checkState(1) == wanted; }, 1500)) return true;
      hooks << "bone tree eye toggle: setCheckState() (click on the check indicator did not toggle)";
      treeItem("slot:blush")->setCheckState(1, wanted);
      return waitFor([&] { auto i = treeItem("slot:blush"); return i && i->checkState(1) == wanted; }, 1500);
    };
    QVERIFY(toggle(Qt::Unchecked));
    QTest::qWait(150);
    canvas->grab().save(evidence + "/01b-canvas-blush-hidden.png");
    QVERIFY(toggle(Qt::Checked));
    QVERIFY(settle());
    QCOMPARE(w->currentView().value("revision"), revision);
    QCOMPARE(rig(), data);
  }
  QVERIFY(select("bone:root"));
  click("cutoutFitView");
  QVERIFY(shot("01-panel-setup.png"));
  canvas->grab().save(evidence + "/01a-canvas-setup.png");

  // ---- 4. clips: idle (breathing + edit), blink, joy, face-happy, mix, preview -------------
  step("4 clips");
  auto clipList = panel->findChild<QListWidget *>("cutoutClipList");
  auto clips = [&] { QStringList ids; for (auto c : rig().value("clips").toArray()) ids << c.toObject().value("id").toString(); return ids; };
  auto clip = [&](const QString &id) { return byId(rig().value("clips").toArray(), id); };
  auto pickClip = [&](const QString &id) {
    for (int i = 0; i < clipList->count(); ++i)
      if (clipList->item(i)->data(Qt::UserRole).toString() == id) {
        clipList->scrollToItem(clipList->item(i));
        QTest::mouseClick(clipList->viewport(), Qt::LeftButton, Qt::NoModifier, clipList->visualItemRect(clipList->item(i)).center());
        return waitFor([&] { return clipList->currentItem() && clipList->currentItem()->data(Qt::UserRole).toString() == id; }, 3000);
      }
    errors << "clip missing in list " + id;
    return false;
  };
  auto bottomTabs = panel->findChild<QTabWidget *>("cutoutBottomTabs");
  auto showTab = [&](int index) { QTest::mouseClick(bottomTabs->tabBar(), Qt::LeftButton, Qt::NoModifier, bottomTabs->tabBar()->tabRect(index).center()); return waitFor([&] { return bottomTabs->currentIndex() == index; }, 2000); };
  QVERIFY(showTab(0));
  QVERIFY(select("bone:body"));
  click("cutoutTemplateBreathing");
  QVERIFY(waitFor([&] { return !w->busy() && clips().contains("breathing"); }));
  QVERIFY(settle());
  // The new character's empty "idle" makes way for the breathing loop under the same name.
  QVERIFY(pickClip("idle"));
  ask("rename idle", "idle-old");
  click("cutoutRenameClip");
  QVERIFY(waitFor([&] { return !w->busy() && clips().contains("idle-old"); }));
  QVERIFY(settle());
  QCOMPARE(controller().value("defaultClip").toString(), QString("idle-old"));  // rename follows into the controller
  QVERIFY(pickClip("breathing"));
  ask("rename breathing", "idle");
  click("cutoutRenameClip");
  QVERIFY(waitFor([&] { return !w->busy() && clips().contains("idle") && !clips().contains("breathing"); }));
  QVERIFY(settle());
  QVERIFY(clip("idle").value("loop").toBool(true));
  // Blink template on the eyes part (registers a layer on track 1 in the same gesture).
  QVERIFY(select("slot:eyes"));
  click("cutoutTemplateBlink");
  QVERIFY2(waitFor([&] { return !w->busy() && clips().contains("blink"); }), qPrintable(status->text()));
  QVERIFY(settle());
  QCOMPARE(int(controller().value("layers").toArray().size()), 1);
  QCOMPARE(controller().value("layers").toArray()[0].toObject(), (QJsonObject{{"track", 1}, {"clip", "blink"}}));
  {
    QSet<QString> swapped;
    for (auto t : clip("blink").value("tracks").toArray())
      if (t.toObject().value("target") == "slot:eyes" && t.toObject().value("property") == "attachment")
        for (auto k : t.toObject().value("keys").toArray()) swapped.insert(k.toObject().value("value").toString());
    QCOMPARE(swapped, (QSet<QString>{"open", "half", "closed"}));
  }
  // joy: one-shot 1200 ms.
  ask("new clip joy", "joy");
  click("cutoutNewClip");
  QVERIFY(waitFor([&] { return !w->busy() && clips().contains("joy"); }));
  QVERIFY(settle());
  QVERIFY(pickClip("joy"));
  typeSpin(panel->findChild<QSpinBox *>("cutoutClipDuration"), "1200");
  QVERIFY(waitFor([&] { return !w->busy() && clip("joy").value("durationMs").toInt() == 1200; }));
  QVERIFY(settle());
  click("cutoutClipLoop");
  QVERIFY(waitFor([&] { return !w->busy() && !clip("joy").value("loop").toBool(true); }));
  QVERIFY(settle());
  click("cutoutModeAnimate");
  auto banner = panel->findChild<QLabel *>("cutoutModeBanner");
  QVERIFY(waitFor([&] { return banner->property("mode").toString() == "animate"; }, 3000));
  auto dopesheet = panel->findChild<QWidget *>("cutoutDopesheet");
  auto timeLabel = panel->findChild<QLabel *>("cutoutTimeLabel");
  auto seek = [&](int ms, int duration) {
    // Ruler click (Dopesheet::timeToX: LABEL 170 + 8, width - LABEL - 20 px for the clip).
    const int x = 178 + int(std::lround(ms * (dopesheet->width() - 190) / double(duration)));
    QTest::mouseClick(dopesheet, Qt::LeftButton, Qt::NoModifier, QPoint(x, 10));
    return waitFor([&] { return timeLabel->text().startsWith(QString::number(ms) + " /"); }, 3000);
  };
  auto keyNow = [&](bool viaButton) {
    if (viaButton) click("cutoutKeyButton");
    else { canvas->setFocus(); QTest::keyClick(canvas, Qt::Key_K); }
    return settle();
  };
  // Pose a bone property in Animate mode (auto-key off: pending change) then commit it with K.
  auto poseKey = [&](const QString &target, const char *property, const QString &value, bool viaButton) {
    if (!select(target)) return false;
    if (!setProp(property, value)) return false;
    const auto before = rig();
    if (!keyNow(viaButton)) return false;
    return waitFor([&] { return rig() != before; }, 5000);
  };
  auto swapKey = [&](const QString &slot, const QString &name, bool viaButton) {
    if (!select("slot:" + slot)) return false;
    if (!choose(attachments, name)) { errors << "attachment option missing " + name; return false; }
    QTest::qWait(30);
    const auto before = rig();
    if (!keyNow(viaButton)) return false;
    return waitFor([&] { return rig() != before; }, 5000);
  };
  auto keyPose = [&](const QString &target, bool viaButton) {
    if (!select(target)) return false;
    const auto before = rig();
    return keyNow(viaButton) && waitFor([&] { return rig() != before; }, 5000);
  };
  QVERIFY(pickClip("joy"));
  QVERIFY(seek(0, 1200));
  for (const auto &target : {"bone:head", "bone:arm-left", "bone:arm-right", "slot:mouth", "slot:eyes"}) QVERIFY2(keyPose(target, false), target);
  QVERIFY(seek(300, 1200));
  QVERIFY(poseKey("bone:head", "Rotation", "-12", false));
  QVERIFY(poseKey("bone:arm-left", "Rotation", "120", true));
  QVERIFY(poseKey("bone:arm-right", "Rotation", "-120", true));
  QVERIFY(swapKey("mouth", "smile", false));
  QVERIFY(swapKey("eyes", "half", true));
  QVERIFY(seek(600, 1200));
  QVERIFY(poseKey("bone:head", "Rotation", "10", false));
  QVERIFY(swapKey("mouth", "open", false));
  QVERIFY(seek(1200, 1200));
  QVERIFY(poseKey("bone:head", "Rotation", "0", false));
  QVERIFY(poseKey("bone:arm-left", "Rotation", "0", true));
  QVERIFY(poseKey("bone:arm-right", "Rotation", "0", true));
  QVERIFY(swapKey("mouth", "neutral", false));
  QVERIFY(swapKey("eyes", "open", false));
  QCOMPARE(keyAt(rig(), "joy", "bone:head", "rotation", 300).value("value").toDouble(), -12.0);
  QCOMPARE(keyAt(rig(), "joy", "bone:arm-left", "rotation", 300).value("value").toDouble(), 120.0);
  QCOMPARE(keyAt(rig(), "joy", "slot:mouth", "attachment", 300).value("value").toString(), QString("smile"));
  QCOMPARE(keyAt(rig(), "joy", "slot:mouth", "attachment", 600).value("value").toString(), QString("open"));
  QCOMPARE(keyAt(rig(), "joy", "slot:eyes", "attachment", 300).value("value").toString(), QString("half"));
  // idle edit: a small head nod on top of the breathing template.
  QVERIFY(pickClip("idle"));
  const int idleMs = clip("idle").value("durationMs").toInt();
  QVERIFY(idleMs >= 1000);
  QVERIFY(seek(0, idleMs));
  QVERIFY(keyPose("bone:head", false));
  QVERIFY(seek(idleMs / 2 / 10 * 10, idleMs));
  QVERIFY(poseKey("bone:head", "Rotation", "3", false));
  QVERIFY(seek(idleMs, idleMs));
  QVERIFY(poseKey("bone:head", "Rotation", "0", false));
  QCOMPARE(keyAt(rig(), "idle", "bone:head", "rotation", idleMs / 2 / 10 * 10).value("value").toDouble(), 3.0);
  // face-happy expression clip (looping attachment pose), made through the command palette
  // (plugin 1.10.0): Ctrl+K, type the Korean name, Enter = exactly what the button does (one edit).
  {
    const int revisionBefore = w->currentView().value("revision").toInt();
    w->activateWindow();  // window shortcuts need the active window
    QVERIFY(QTest::qWaitForWindowActive(w));
    canvas->setFocus();
    QTest::keyClick(canvas, Qt::Key_K, Qt::ControlModifier);
    auto palette = panel->findChild<QWidget *>("cutoutCommandPalette");
    auto search = panel->findChild<QLineEdit *>("cutoutCommandSearch");
    QVERIFY(palette && palette->isVisible() && search && search->hasFocus());
    typeText(search, QString::fromUtf8("새 클립"));
    auto list = panel->findChild<QListWidget *>("cutoutCommandList");
    QVERIFY(list && list->currentItem());
    QCOMPARE(list->currentItem()->data(Qt::UserRole).toString(), QString("cutoutNewClip"));
    ask("new clip face-happy", "face-happy");
    QTest::keyClick(search, Qt::Key_Return);
    QVERIFY(waitFor([&] { return !w->busy() && clips().contains("face-happy"); }));
    QVERIFY(settle());
    QVERIFY(!palette->isVisible());
    QCOMPARE(w->currentView().value("revision").toInt(), revisionBefore + 1);
    step("palette: 새 클립 face-happy (one edit)");
  }
  QVERIFY(pickClip("face-happy"));
  QVERIFY(seek(0, 1000));
  QVERIFY(swapKey("mouth", "smile", false));
  QVERIFY(swapKey("eyes", "half", false));
  // Mix: default 120 ms and joy -> idle 250 ms.
  typeSpin(panel->findChild<QSpinBox *>("cutoutMixDefault"), "120");
  QVERIFY(waitFor([&] { return !w->busy() && rig().value("mix").toObject().value("defaultMs").toInt() == 120; }));
  QVERIFY(settle());
  click("cutoutMixAdd");
  QVERIFY(waitFor([&] { return !w->busy() && rig().value("mix").toObject().value("pairs").toArray().size() == 1; }));
  QVERIFY(settle());
  QVERIFY(choose(panel->findChild<QComboBox *>("cutoutMixFrom0"), "joy"));
  QVERIFY(settle());
  QVERIFY(choose(panel->findChild<QComboBox *>("cutoutMixTo0"), "idle"));
  QVERIFY(settle());
  typeSpin(panel->findChild<QSpinBox *>("cutoutMixMs0"), "250");
  QVERIFY(settle());
  QCOMPARE(rig().value("mix").toObject().value("pairs").toArray()[0].toObject(), (QJsonObject{{"from", "joy"}, {"to", "idle"}, {"durationMs", 250}}));
  // Preview transport: play, pause, seek.
  QVERIFY(pickClip("idle"));
  auto play = panel->findChild<QPushButton *>("cutoutPlay");
  QTest::mouseClick(play, Qt::LeftButton);
  QVERIFY(play->isChecked());
  QTest::qWait(700);
  const auto playing = timeLabel->text();
  QVERIFY2(!playing.startsWith("0 /"), qPrintable(playing));
  QTest::mouseClick(play, Qt::LeftButton);
  QVERIFY(!play->isChecked());
  const auto paused = timeLabel->text();
  QTest::qWait(300);
  QCOMPARE(timeLabel->text(), paused);
  QVERIFY(seek(1000, idleMs));
  QVERIFY(pickClip("joy"));
  QVERIFY(seek(300, 1200));
  QVERIFY(shot("02-dopesheet-joy.png"));

  // ---- 5. 게임 연결 ---------------------------------------------------------------------
  step("5 game connection");
  QVERIFY(showTab(1));
  QVERIFY(choose(panel->findChild<QComboBox *>("cutoutControllerDefaultClip"), "idle"));
  QVERIFY(waitFor([&] { return !w->busy() && controller().value("defaultClip") == "idle"; }));
  QVERIFY(settle());
  QVERIFY(showTab(0));
  QVERIFY(pickClip("idle-old"));
  confirm("delete idle-old");
  click("cutoutDeleteClip");
  QVERIFY(waitFor([&] { return !w->busy() && !clips().contains("idle-old"); }));
  QVERIFY(settle());
  QVERIFY(showTab(1));
  QCOMPARE(panel->findChild<QTableWidget *>("cutoutLayerTable")->rowCount(), 1);
  QCOMPARE(panel->findChild<QComboBox *>("cutoutLayerClip0")->currentText(), QString("blink"));
  QCOMPARE(panel->findChild<QSpinBox *>("cutoutLayerTrack0")->value(), 1);
  QVERIFY(choose(panel->findChild<QComboBox *>("cutoutControllerCharacter"), w->projectData().value("protagonistId").toString()));
  QVERIFY(settle());
  click("cutoutBindingAdd");
  QVERIFY(waitFor([&] { return !w->busy() && controller().value("bindings").toArray().size() == 1; }));
  QVERIFY(settle());
  auto cell = [&](const QString &name) { return panel->findChild<QWidget *>(name); };
  typeInto(qobject_cast<QLineEdit *>(cell("cutoutBindingId0")), "cheer-joy");
  QVERIFY(settle());
  typeInto(qobject_cast<QLineEdit *>(cell("cutoutBindingMatch0")), "outcome=success, actionId=" + actionId);
  QVERIFY(settle());
  QVERIFY(choose(qobject_cast<QComboBox *>(cell("cutoutBindingClip0")), "joy"));
  QVERIFY(settle());
  QVERIFY(choose(qobject_cast<QComboBox *>(cell("cutoutBindingLoop0")), "false"));
  QVERIFY(settle());
  QVERIFY(choose(qobject_cast<QComboBox *>(cell("cutoutBindingThen0")), "default"));
  QVERIFY(settle());
  click("cutoutBindingAdd");
  QVERIFY(waitFor([&] { return !w->busy() && controller().value("bindings").toArray().size() == 2; }));
  QVERIFY(settle());
  typeInto(qobject_cast<QLineEdit *>(cell("cutoutBindingId1")), "happy-face");
  QVERIFY(settle());
  QVERIFY(choose(qobject_cast<QComboBox *>(cell("cutoutBindingSignal1")), "portraitChanged"));
  QVERIFY(settle());
  typeInto(qobject_cast<QLineEdit *>(cell("cutoutBindingMatch1")), "self=true");
  QVERIFY(settle());
  QVERIFY(choose(qobject_cast<QComboBox *>(cell("cutoutBindingClip1")), "face-happy"));
  QVERIFY(settle());
  typeSpin(qobject_cast<QSpinBox *>(cell("cutoutBindingTrack1")), "2");
  QVERIFY(settle());
  QVERIFY(choose(qobject_cast<QComboBox *>(cell("cutoutBindingThen1")), "stop"));
  QVERIFY(settle());
  typeInto(qobject_cast<QLineEdit *>(cell("cutoutBindingStopOn1")), "dialogueEnded");
  QVERIFY(settle());
  {
    const auto c = controller();
    const auto joy = byId(c.value("bindings").toArray(), "cheer-joy");
    QCOMPARE(joy.value("signal").toString(), QString("actionResult"));
    QCOMPARE(joy.value("match").toObject(), (QJsonObject{{"outcome", "success"}, {"actionId", actionId}}));
    QCOMPARE(joy.value("clip").toString(), QString("joy"));
    QCOMPARE(joy.value("loop").toBool(true), false);
    const auto happy = byId(c.value("bindings").toArray(), "happy-face");
    QCOMPARE(happy.value("signal").toString(), QString("portraitChanged"));
    QCOMPARE(happy.value("track").toInt(), 2);
    QCOMPARE(happy.value("then").toString(), QString("stop"));
    QCOMPARE(happy.value("stopOn").toArray(), (QJsonArray{"dialogueEnded"}));
    QCOMPARE(c.value("defaultClip").toString(), QString("idle"));
  }
  // Editor-only signal preview (never reaches the game).
  QVERIFY(choose(panel->findChild<QComboBox *>("cutoutPreviewSignalName"), "actionResult"));
  typeInto(panel->findChild<QLineEdit *>("cutoutPreviewSignalParams"), "outcome=success, actionId=" + actionId, false);
  {
    const auto revision = w->currentView().value("revision");
    click("cutoutPreviewSignal");
    auto result = panel->findChild<QLabel *>("cutoutPreviewResult");
    QVERIFY2(waitFor([&] { return result->text().contains("cheer-joy"); }, 3000), qPrintable(result->text()));
    QTest::qWait(400);
    QCOMPARE(w->currentView().value("revision"), revision);
  }
  QVERIFY(shot("03-game-connection.png"));
  QVERIFY2(errors.isEmpty(), qPrintable(noErrors()));

  // ---- 6. screen: place the character next to the action button -------------------------
  step("6 screen element");
  // Back to 화면 편집 through the switcher: the docks of before come back as they were.
  // 편집… on the placed element re-opens the animation workspace.
  QVERIFY(switchWorkspace(w, QString()));
  QCOMPARE(visibleDocks(w), screenDocks);
  QTest::qWait(150);
  {
    auto palette = w->findChild<QToolButton *>("elementPalette");
    QAction *placeAction = w->findChild<QAction *>("pluginElement-cutout-animation-character");
    QVERIFY(palette && placeAction);
    bool clicked = false;
    auto popupTimer = new QTimer(&pollers);
    popupTimer->setInterval(30);
    connect(popupTimer, &QTimer::timeout, &pollers, [&, popupTimer] {
      auto menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
      if (!menu || !menu->isVisible() || clicked) return;
      const QRect r = menu->actionGeometry(placeAction);
      if (!r.isValid()) return;
      clicked = true;
      popupTimer->stop();
      QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, r.center());
    });
    popupTimer->start();
    QTest::mouseClick(palette, Qt::LeftButton);
    if (!waitFor([&] { return clicked; }, 3000)) {
      popupTimer->stop();
      while (auto p = QApplication::activePopupWidget()) p->close();
      hooks << "palette entry triggered with QAction::trigger() (popup menu did not open)";
      placeAction->trigger();
    }
  }
  QVERIFY(waitFor([&] { return !w->busy() && w->selectedId().startsWith("character-"); }));
  QVERIFY(settle());
  const auto characterId = w->selectedId();
  const auto instanceId = "home/" + characterId;
  QCOMPARE(element(characterId).value("extension").toObject().value("ref").toString(), QString("hero"));  // auto-linked
  auto ref = w->findChild<QComboBox *>("pluginRef");
  QVERIFY(choose(ref, ""));  // unlink, then link again through the inspector
  QVERIFY(waitFor([&] { return !w->busy() && element(characterId).value("extension").toObject().value("ref").toString().isEmpty(); }));
  QVERIFY(settle());
  QVERIFY(choose(w->findChild<QComboBox *>("pluginRef"), "hero"));
  QVERIFY(waitFor([&] { return !w->busy() && element(characterId).value("extension").toObject().value("ref") == "hero"; }));
  QVERIFY(settle());
  for (auto [name, value] : QList<QPair<const char *, QString>>{{"elementX", "860"}, {"elementY", "120"}, {"elementWidth", "380"}, {"elementHeight", "560"}}) {
    typeSpin(w->findChild<QSpinBox *>(name), value);
    QVERIFY2(settle(), name);
  }
  QCOMPARE(element(characterId).value("x").toInt(), 860);
  QCOMPARE(element(characterId).value("height").toInt(), 560);
  QVERIFY(w->findChild<QPushButton *>("pluginEdit")->isEnabled());
  {
    auto screen = w->findChild<ScreenCanvas *>("screenCanvas");
    QVERIFY(screen && screen->pluginPlaceholder);
    QVERIFY(screen->pluginPlaceholder(element(characterId)).isEmpty());
    screen->fitCanvas();
    QTest::qWait(200);
    QGraphicsItem *item = nullptr;
    for (auto candidate : static_cast<QGraphicsView *>(screen)->scene()->items())
      if (candidate->data(0).toString() == characterId) item = candidate;
    QVERIFY(item);
    const auto region = screen->mapFromScene(item->sceneBoundingRect()).boundingRect().intersected(screen->viewport()->rect());
    const auto image = screen->viewport()->grab(region).toImage();
    int dressPixels = 0, hairPixels = 0;
    for (int y = 0; y < image.height(); ++y)
      for (int x = 0; x < image.width(); ++x) {
        const QColor c = image.pixelColor(x, y);
        if (std::abs(c.red() - 120) < 18 && std::abs(c.green() - 170) < 18 && std::abs(c.blue() - 230) < 18) ++dressPixels;
        if (std::abs(c.red() - 92) < 14 && std::abs(c.green() - 64) < 14 && std::abs(c.blue() - 120) < 14) ++hairPixels;
      }
    QVERIFY2(dressPixels > 60 && hairPixels > 30, qPrintable(QString("dress %1 hair %2").arg(dressPixels).arg(hairPixels)));
    image.save(evidence + "/04b-screen-element-crop.png");
  }
  QVERIFY(shot("04-screen-canvas.png"));
  // 편집… on the element opens the animation workspace on its controller.
  QTest::mouseClick(w->findChild<QPushButton *>("pluginEdit"), Qt::LeftButton);
  QVERIFY(waitFor([&] { return w->currentWorkspace() == WORKSPACE && panel->isVisible() && bottomTabs->currentIndex() == 1; }, 3000));
  QVERIFY(shot("04c-element-edit-opens-panel.png"));

  // ---- 7. save, then Play (service play -> exported game -> RaisingPlayer.exe) -----------
  step("7 save + play");
  auto saveProject = [&] {
    menuClick(w->findChild<QAction *>("saveProject"), &hooks, "프로젝트 저장");
    return settle() && waitFor([&] { return !w->currentView().value("dirty").toBool(); }, 10000);
  };
  QVERIFY2(saveProject(), qPrintable(w->lastError));
  for (auto d : w->currentView().value("diagnostics").toArray())
    QVERIFY2(d.toObject().value("severity") != "error", qPrintable(QJsonDocument(d.toObject()).toJson(QJsonDocument::Compact)));
  QVERIFY(QFileInfo::exists(projectRoot + "/" + RIG_FILE));
  QVERIFY(QFileInfo::exists(projectRoot + "/" + CONTROLLER_FILE));
  QVERIFY(QFileInfo::exists(projectRoot + "/plugins/lock.json"));
  QSet<QString> imported;
  for (auto a : rig().value("attachments").toArray()) imported.insert(a.toObject().value("imageAssetId").toString());
  QVERIFY(imported.size() >= 12);  // arm-left/arm-right are identical PNGs and share one asset
  for (const auto &id : imported) {
    bool listed = false;
    for (auto asset : w->currentView().value("documents").toArray())
      for (auto a : asset.toObject().value("data").toObject().value("assets").toArray()) listed = listed || a.toObject().value("id") == id;
    QVERIFY2(listed, qPrintable(id));
  }
  auto runPlay = [&](const QString &name, const QJsonArray &steps) -> QJsonArray {
    const auto stepsFile = evidence + "/" + name + "-steps.json";
    QFile f(stepsFile);
    if (!f.open(QIODevice::WriteOnly)) return {};
    f.write(QJsonDocument(steps).toJson());
    f.close();
    w->playAutomation = stepsFile;
    w->playReport = evidence + "/" + name + "-report.json";
    QFile::remove(w->playReport);
    QSignalSpy finished(w, &StudioWindow::playerFinished);
    toolClick(w->findChild<QAction *>("playProject"), &hooks, "실행");
    if (!waitFor([&] { return !finished.isEmpty(); }, 120000)) { errors << name + ": player did not finish · " + w->lastError; return {}; }
    if (finished.first().first().toInt() != 0) errors << name + QString(": player exit %1").arg(finished.first().first().toInt());
    QFile r(w->playReport);
    if (!r.open(QIODevice::ReadOnly)) { errors << name + ": no report"; return {}; }
    const auto report = QJsonDocument::fromJson(r.readAll()).object();
    if (!report.value("ok").toBool()) errors << name + ": " + report.value("error").toString();
    return report.value("steps").toArray();
  };
  const QJsonArray firstSteps{
      QJsonObject{{"click", "start"}}, QJsonObject{{"waitFrames", 3}}, QJsonObject{{"expectScreen", "home"}},
      QJsonObject{{"extensionAdvanceMs", 500}, {"extensionSteps", 5}}, QJsonObject{{"checkpoint", "idle"}},
      QJsonObject{{"screenshot", evidence + "/play1-idle.png"}},
      QJsonObject{{"click", buttonId}}, QJsonObject{{"waitFrames", 3}}, QJsonObject{{"checkpoint", "joy"}},
      QJsonObject{{"extensionAdvanceMs", 300}, {"extensionSteps", 3}}, QJsonObject{{"checkpoint", "joy-300"}},
      QJsonObject{{"screenshot", evidence + "/play1-joy-300.png"}},
      QJsonObject{{"extensionAdvanceMs", 1500}, {"extensionSteps", 15}}, QJsonObject{{"checkpoint", "back-idle"}},
      QJsonObject{{"screenshot", evidence + "/play1-back-idle.png"}}};
  const auto first = runPlay("play1", firstSteps);
  QVERIFY2(errors.isEmpty() && !first.isEmpty(), qPrintable(noErrors()));
  {
    const auto idle = checkpoint(first, "idle"), joy = checkpoint(first, "joy"), peak = checkpoint(first, "joy-300"), back = checkpoint(first, "back-idle");
    const auto mounted = instanceIn(idle, instanceId);
    QVERIFY2(mounted.value("available").toBool(), qPrintable(QJsonDocument(idle.value("extensions").toObject()).toJson(QJsonDocument::Compact)));
    QCOMPARE(mounted.value("element").toObject().value("ref").toString(), QString("hero"));
    QSet<QString> drawn;
    for (auto item : mounted.value("items").toArray()) drawn.insert(item.toObject().value("imageAssetId").toString());
    QVERIFY2(drawn.size() >= 8 && drawn.contains(attachment(rig(), "body", "body").value("imageAssetId").toString()), qPrintable(QStringList(drawn.values()).join(",")));
    for (const auto &id : drawn) QVERIFY2(imported.contains(id), qPrintable(id));
    QCOMPARE(trackOf(mounted, 0).value("clip").toString(), QString("idle"));
    QCOMPARE(trackOf(mounted, 1).value("clip").toString(), QString("blink"));
    QCOMPARE(trackOf(mounted, 0).value("timeMs").toDouble(), 500.0);
    const auto observed = joy.value("extensions").toObject().value("lastObserve").toObject();
    QCOMPARE(observed.value("command").toObject().value("type").toString(), QString("executeAction"));
    QCOMPARE(observed.value("plugins").toObject().value("cutout-animation").toObject().value("matched").toArray(), (QJsonArray{instanceId + ":cheer-joy"}));
    QCOMPARE(trackOf(instanceIn(joy, instanceId), 0).value("clip").toString(), QString("joy"));
    QCOMPARE(trackOf(instanceIn(peak, instanceId), 0).value("clip").toString(), QString("joy"));
    QCOMPARE(trackOf(instanceIn(peak, instanceId), 0).value("timeMs").toDouble(), 300.0);
    QCOMPARE(trackOf(instanceIn(back, instanceId), 0).value("clip").toString(), QString("idle"));
    // The game state changed exactly once (the action's +5 money) and never by animation time.
    const auto money = [](const QJsonObject &ui) { return ui.value("state").toObject().value("attributes").toObject().value("money").toInt(); };
    QCOMPARE(money(joy), money(idle) + 5);
    QCOMPARE(joy.value("state"), peak.value("state"));
    QCOMPARE(joy.value("state"), back.value("state"));
    QCOMPARE(back.value("extensions").toObject().value("observations").toInt() - idle.value("extensions").toObject().value("observations").toInt(), 1);
    // Different pose at the joy peak than at rest.
    QVERIFY(instanceIn(peak, instanceId).value("items") != instanceIn(joy, instanceId).value("items"));
  }

  // ---- 8. reopen through the UI, then Undo/Redo a panel edit via the 편집 menu ----------
  step("8 reopen + undo/redo");
  const auto rigSaved = rig(), controllerSaved = controller();
  QVERIFY(!w->currentView().value("dirty").toBool());
  w->close();  // the window's close event (no unsaved changes → closes)
  QVERIFY(!w->isVisible());
  owner.reset();
  owner = std::make_unique<StudioWindow>(engine);
  w = owner.get();
  w->resize(1760, 1080);
  w->show();
  QVERIFY(QTest::qWaitForWindowExposed(w));
  {
    QAction *open = nullptr;
    for (auto top : w->menuBar()->actions())
      if (top->menu())
        for (auto a : top->menu()->actions())
          if (a->text().startsWith(QString::fromUtf8("프로젝트 열기"))) open = a;
    QVERIFY(open);
    onModal(&pollers, &errors, "open project folder", isFileDialog, [&](QWidget *f) { chooseFile(f, projectRoot, &hooks); });
    menuClick(open, &hooks, "프로젝트 열기");
  }
  QVERIFY2(waitFor([&] { return !w->busy() && !w->projectData().isEmpty() && w->pluginManager()->loadedIds().contains("cutout-animation"); }, 40000), qPrintable(w->lastError + noErrors()));
  QVERIFY(settle());
  QCOMPARE(rig(), rigSaved);
  QCOMPARE(controller(), controllerSaved);
  // Keyboard this time: Ctrl+Shift+A toggles the animation workspace (window shortcut: the
  // freshly created window must be the active one).
  w->activateWindow();
  QTest::qWaitForWindowActive(w);
  QTest::keyClick(w, Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier);
  if (!waitFor([&] { return w->currentWorkspace() == WORKSPACE; }, 2000)) {
    hooks << "workspace shortcut: QAction::trigger() because Ctrl+Shift+A did not reach the window";
    w->findChild<QAction *>("studioWorkspace-cutout-animation-character")->trigger();
  }
  QVERIFY(waitFor([&] { return w->currentWorkspace() == WORKSPACE; }, 3000));
  panel = w->findChild<QWidget *>("cutoutPanel");
  QVERIFY(panel && panel->isVisible());
  tree = panel->findChild<QTreeWidget *>("cutoutBoneTree");
  canvas = panel->findChild<QWidget *>("cutoutCanvas");
  status = panel->findChild<QLabel *>("cutoutStatus");
  selectionLabel = panel->findChild<QLabel *>("cutoutSelectionLabel");
  attachments = panel->findChild<QComboBox *>("cutoutPropAttachment");
  clipList = panel->findChild<QListWidget *>("cutoutClipList");
  bottomTabs = panel->findChild<QTabWidget *>("cutoutBottomTabs");
  dopesheet = panel->findChild<QWidget *>("cutoutDopesheet");
  timeLabel = panel->findChild<QLabel *>("cutoutTimeLabel");
  {
    auto characters = panel->findChild<QTreeWidget *>("cutoutCharacterList");
    QVERIFY(waitFor([&] { return characters->topLevelItemCount() == 1; }, 5000));
    QVERIFY(characters->topLevelItem(0)->text(0).contains("hero"));
    QCOMPARE(characters->topLevelItem(0)->childCount(), 1);
    QVERIFY(treeItem("slot:hair-front") && treeItem("bone:arm-right"));
    QVERIFY(shot("07-reopened-panel.png"));
  }
  {
    ask("rename character", QString::fromUtf8("히어로"));
    click("cutoutRenameCharacter");
    QVERIFY(waitFor([&] { return !w->busy() && rig().value("name") == QString::fromUtf8("히어로"); }));
    QVERIFY(settle());
    auto renamed = rigSaved;
    renamed["name"] = QString::fromUtf8("히어로");
    QCOMPARE(rig(), renamed);
    menuClick(w->findChild<QAction *>("undoEdit"), &hooks, "되돌리기");
    QVERIFY(waitFor([&] { return !w->busy() && rig() == rigSaved; }));
    QVERIFY(settle());
    QVERIFY(waitFor([&] { return panel->findChild<QTreeWidget *>("cutoutCharacterList")->topLevelItem(0)->text(0) == "hero"; }, 3000));
    menuClick(w->findChild<QAction *>("redoEdit"), &hooks, "다시 실행");
    QVERIFY(waitFor([&] { return !w->busy() && rig() == renamed; }));
    QVERIFY(settle());
    QCOMPARE(controller(), controllerSaved);
  }

  // ---- 9. modify (joy 1200 -> 1600 ms), save, play again ----------------------------------
  step("9 modify + play again");
  QVERIFY(showTab(0));
  QVERIFY(pickClip("joy"));
  typeSpin(panel->findChild<QSpinBox *>("cutoutClipDuration"), "1600");
  QVERIFY(waitFor([&] { return !w->busy() && clip("joy").value("durationMs").toInt() == 1600; }));
  QVERIFY(settle());
  QVERIFY2(saveProject(), qPrintable(w->lastError));
  const QJsonArray secondSteps{
      QJsonObject{{"click", "start"}}, QJsonObject{{"waitFrames", 3}}, QJsonObject{{"expectScreen", "home"}},
      QJsonObject{{"extensionAdvanceMs", 500}, {"extensionSteps", 5}}, QJsonObject{{"checkpoint", "idle"}},
      QJsonObject{{"click", buttonId}}, QJsonObject{{"waitFrames", 3}}, QJsonObject{{"checkpoint", "joy"}},
      QJsonObject{{"extensionAdvanceMs", 1300}, {"extensionSteps", 13}}, QJsonObject{{"checkpoint", "joy-1300"}},
      QJsonObject{{"screenshot", evidence + "/play2-joy-1300.png"}},
      QJsonObject{{"extensionAdvanceMs", 700}, {"extensionSteps", 7}}, QJsonObject{{"checkpoint", "back-idle"}}};
  const auto second = runPlay("play2", secondSteps);
  QVERIFY2(errors.isEmpty() && !second.isEmpty(), qPrintable(noErrors()));
  {
    const auto late = instanceIn(checkpoint(second, "joy-1300"), instanceId);
    QCOMPARE(trackOf(late, 0).value("clip").toString(), QString("joy"));  // was already idle with 1200 ms
    QCOMPARE(trackOf(late, 0).value("timeMs").toDouble(), 1300.0);
    QCOMPARE(trackOf(instanceIn(checkpoint(second, "back-idle"), instanceId), 0).value("clip").toString(), QString("idle"));
    const auto money = [](const QJsonObject &ui) { return ui.value("state").toObject().value("attributes").toObject().value("money").toInt(); };
    QCOMPARE(money(checkpoint(second, "joy")), money(checkpoint(second, "idle")) + 5);
  }

  // ---- 10. export Windows + Web through 게임 내보내기 ----------------------------------------
  step("10 export");
  QMap<QString, QString> exported;
  for (const auto &target : QStringList{"windows", "web"}) {
    bool complete = false;
    onModal(&pollers, &errors, "export dialog " + target, [](QWidget *m) { return m->objectName() == "exportGameDialog"; }, [&, target](QWidget *d) {
      auto targetBox = d->findChild<QComboBox *>("exportTarget");
      targetBox->setCurrentIndex(targetBox->findData(target));
      typeInto(d->findChild<QLineEdit *>("exportParent"), exportParent, false);
      typeInto(d->findChild<QLineEdit *>("exportFolderName"), target, false);
      QTest::mouseClick(d->findChild<QPushButton *>("exportStart"), Qt::LeftButton);
      auto openFolder = d->findChild<QPushButton *>("exportOpenFolder");
      if (!waitFor([&] { return !w->busy() && openFolder->isEnabled(); }, 90000)) errors << "export " + target + ": " + d->findChild<QLabel *>("exportResult")->text();
      else { exported[target] = openFolder->property("directory").toString(); complete = true; }
      if (target == "web") d->grab().save(evidence + "/06-export-dialog.png");
      static_cast<QDialog *>(d)->reject();
    });
    menuClick(w->findChild<QAction *>("exportGame"), &hooks, "게임 내보내기");
    QVERIFY2(waitFor([&] { return complete || !errors.isEmpty(); }, 100000) && complete, qPrintable(noErrors()));
    QVERIFY(settle());
  }
  QVERIFY(QFileInfo::exists(exported["windows"] + "/RaisingPlayer.exe"));
  QVERIFY(QFileInfo::exists(exported["web"] + "/index.html"));
  for (const auto &target : exported.keys()) {
    QFile game(exported[target] + "/game.json");
    QVERIFY(game.open(QIODevice::ReadOnly));
    const auto plugins = QJsonDocument::fromJson(game.readAll()).object().value("extensions").toObject().value("plugins").toArray();
    QCOMPARE(int(plugins.size()), 1);
    QCOMPARE(plugins[0].toObject().value("id").toString(), QString("cutout-animation"));
    QVERIFY(QFileInfo::exists(exported[target] + "/" + plugins[0].toObject().value("entry").toString()));
    // Atlas packing (plugin 1.9.0, on by default for these players): the rig names page regions.
    int regions = 0;
    for (const auto &doc : plugins[0].toObject().value("documents").toArray())
      if (doc.toObject().value("type") == "rig")
        for (const auto &a : doc.toObject().value("data").toObject().value("attachments").toArray()) {
          const auto atlas = a.toObject().value("atlas").toObject();
          if (atlas.isEmpty()) continue;
          regions++;
          QVERIFY2(QFileInfo::exists(exported[target] + "/assets/" + atlas.value("page").toString() + ".png"), qPrintable(atlas.value("page").toString()));
        }
    QVERIFY2(regions >= 8, qPrintable(QString("%1 atlas regions in the %2 export").arg(regions).arg(target)));
  }

  // ---- 11. PSD 가져오기 (plugin 1.11.0): layered art -> a new character in ONE edit ---------
  step("11 psd import");
  {
    if (w->currentWorkspace() != WORKSPACE) w->findChild<QAction *>("studioWorkspace-cutout-animation-character")->trigger();
    QVERIFY(waitFor([&] { return w->currentWorkspace() == WORKSPACE; }, 3000));
    const QString psdFile = work + "/art-" + stamp + "/psd-hero.psd", psdRig = "plugins/cutout-animation/rig/psd-hero.json";
    QVERIFY(QDir().mkpath(QFileInfo(psdFile).path()));
    {
      QFile f(psdFile);
      QVERIFY(f.open(QIODevice::WriteOnly));
      f.write(layeredPsd(200, 320));
    }
    const int revision = w->currentView().value("revision").toInt();
    bool previewed = false;
    onModal(&pollers, &errors, "psd file dialog", isFileDialog, [&](QWidget *f) { chooseFile(f, psdFile, &hooks); });
    onModal(&pollers, &errors, "layer import dialog", [](QWidget *m) { return m->objectName() == "cutoutLayerImportDialog"; }, [&](QWidget *d) {
      auto layersTree = d->findChild<QTreeWidget *>("cutoutLayerImportTree");
      auto id = d->findChild<QLineEdit *>("cutoutLayerImportId");
      int rows = 0;
      for (QTreeWidgetItemIterator it(layersTree); *it; ++it) rows++;
      previewed = rows == 5 && id && id->text() == "psd-hero" && layersTree->topLevelItem(0)->text(0) == "head" &&
                  d->findChild<QLabel *>("cutoutLayerImportSummary")->text().contains(QString::fromUtf8("첨부 4(숨김 1)"));
      if (!previewed) errors << "layer import preview: rows " + QString::number(rows) + " / " + d->findChild<QLabel *>("cutoutLayerImportSummary")->text();
      d->grab().save(evidence + "/09-layer-import-dialog.png");
      QTest::mouseClick(d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok), Qt::LeftButton);
    });
    click("cutoutImportLayers");
    QVERIFY2(waitFor([&] { return !w->busy() && !pluginData(w->currentView(), psdRig).isEmpty(); }, 40000), qPrintable(w->lastError + noErrors()));
    QVERIFY(settle());
    QVERIFY2(previewed && errors.isEmpty(), qPrintable(noErrors()));
    QCOMPARE(w->currentView().value("revision").toInt(), revision + 1);  // the preview changed nothing, the import is one edit
    const auto imported = pluginData(w->currentView(), psdRig);
    QCOMPARE(slotOrder(imported), QStringList({"body", "face", "eyes"}));
    QCOMPARE(byId(imported.value("slots").toArray(), "eyes").value("attachment").toString(), QString("open"));
    QVERIFY(!attachment(imported, "eyes", "closed").isEmpty());
    QCOMPARE(byId(imported.value("bones").toArray(), "head").value("parent").toString(), QString("root"));
    QCOMPARE(attachment(imported, "body", "body").value("width").toInt(), 82);  // trimmed + 1 px ring
    for (auto a : imported.value("attachments").toArray())
      QVERIFY2(QFileInfo::exists(projectRoot + "/assets/" + a.toObject().value("imageAssetId").toString() + ".png"), qPrintable(a.toObject().value("imageAssetId").toString()));
    QVERIFY(!pluginData(w->currentView(), "plugins/cutout-animation/controller/psd-hero.json").isEmpty());
    QVERIFY2(status->text().contains(QString::fromUtf8("가져옴")), qPrintable(status->text()));
    QVERIFY(waitFor([&] { return panel->findChild<QTreeWidget *>("cutoutCharacterList")->topLevelItemCount() == 2; }, 3000));
    for (auto d : w->currentView().value("diagnostics").toArray())
      QVERIFY2(!(d.toObject().value("severity") == "error" && d.toObject().value("file").toString().contains("psd-hero")), qPrintable(d.toObject().value("message").toString()));
    QVERIFY(shot("10-psd-imported.png"));
    // One 되돌리기 removes the character (rig, controller, asset entries); 다시 실행 brings it back.
    menuClick(w->findChild<QAction *>("undoEdit"), &hooks, "되돌리기");
    QVERIFY(waitFor([&] { return !w->busy() && pluginData(w->currentView(), psdRig).isEmpty(); }));
    QVERIFY(settle());
    menuClick(w->findChild<QAction *>("redoEdit"), &hooks, "다시 실행");
    QVERIFY(waitFor([&] { return !w->busy() && pluginData(w->currentView(), psdRig) == imported; }));
    QVERIFY(settle());
    QVERIFY2(saveProject(), qPrintable(w->lastError));
  }

  // ---- 12. 클리핑 · 시퀀스 (plugin 1.12.0) on the imported character: one edit per gesture ----
  step("12 clipping and sequence");
  {
    const QString psdRig = "plugins/cutout-animation/rig/psd-hero.json";
    auto psd = [&] { return pluginData(w->currentView(), psdRig); };
    order = panel->findChild<QListWidget *>("cutoutDrawOrder");
    auto characters = panel->findChild<QTreeWidget *>("cutoutCharacterList");
    QTreeWidgetItem *psdItem = nullptr;
    for (int i = 0; i < characters->topLevelItemCount(); ++i)
      if (characters->topLevelItem(i)->data(0, Qt::UserRole).toString() == "rig:psd-hero") psdItem = characters->topLevelItem(i);
    QVERIFY(psdItem);
    if (characters->isVisible() && !characters->visualItemRect(psdItem).isEmpty())
      QTest::mouseClick(characters->viewport(), Qt::LeftButton, Qt::NoModifier, characters->visualItemRect(psdItem).center());
    if (characters->currentItem() != psdItem) { hooks << "character list: setCurrentItem() (the list is in a collapsed section)"; characters->setCurrentItem(psdItem); }
    QVERIFY(waitFor([&] { return treeItem("slot:face") != nullptr; }, 3000));
    click("cutoutModeSetup");
    QVERIFY(settle());
    auto tabs = panel->findChild<QTabWidget *>("cutoutLookTabs");
    auto showPage = [&](const char *name) {
      // Plugin 1.20.0: the page selector's button (the tab bar is hidden), clicked like a user.
      QWidget *page = panel->findChild<QWidget *>(name);
      auto button = panel->findChild<QToolButton *>(QString(name) + "Tab");
      if (button && button->isVisible()) QTest::mouseClick(button, Qt::LeftButton, Qt::NoModifier, button->rect().center());
      if (tabs->currentWidget() != page) { hooks << QString("look page %1 chosen with setCurrentWidget() (its selector button was not clickable)").arg(name); tabs->setCurrentWidget(page); }
      return waitFor([&] { return page->isVisible(); }, 2000);
    };
    QVERIFY(showPage("cutoutClippingPage"));
    QVERIFY(pickOrder("face"));
    click("cutoutFitView");
    QTest::qWait(80);
    // 클리핑 만들기: four canvas clicks around the left of the face (x 70..130), the first point again closes.
    int revision = w->currentView().value("revision").toInt();
    click("cutoutClippingCreate");
    const QList<QPointF> polygon{{60, 30}, {100, 30}, {100, 120}, {60, 120}, {60, 30}};
    for (const QPointF &p : polygon) { QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, rigPoint(canvas, p.x(), p.y(), 200, 320)); QTest::qWait(30); }
    QVERIFY2(waitFor([&] { return !w->busy() && w->currentView().value("revision").toInt() == revision + 1; }, 10000), qPrintable(status->text() + noErrors()));
    QVERIFY(settle());
    QCOMPARE(slotOrder(psd()), QStringList({"body", "face-clip", "face", "eyes"}));
    QCOMPARE(attachment(psd(), "face-clip", "clip").value("type").toString(), QString("clipping"));
    QCOMPARE(attachment(psd(), "face-clip", "clip").value("end").toString(), QString("face"));
    // The canvas preview cuts the face at x = 100 (rig px), like the players.
    auto canvasAt = [&](double x, double y) { QTest::qWait(60); return canvas->grab().toImage().pixelColor(rigPoint(canvas, x, y, 200, 320)); };
    const QColor kept = canvasAt(85, 50), cut = canvasAt(118, 50);
    QVERIFY2(std::abs(kept.red() - 240) < 30 && std::abs(kept.green() - 200) < 30, qPrintable(kept.name()));
    QVERIFY2(std::abs(cut.red() - 240) > 60 || std::abs(cut.green() - 200) > 60, qPrintable(cut.name()));
    // 끝 조각 → eyes (keyboard on the combo), then one 되돌리기 / 다시 실행.
    auto end = panel->findChild<QComboBox *>("cutoutClippingEnd");
    QVERIFY(end && end->isEnabled());
    revision = w->currentView().value("revision").toInt();
    end->setFocus();
    QTest::keyClick(end, Qt::Key_Down);
    QVERIFY2(waitFor([&] { return !w->busy() && attachment(psd(), "face-clip", "clip").value("end").toString() == "eyes"; }, 10000), qPrintable(status->text()));
    QVERIFY(settle());
    QCOMPARE(w->currentView().value("revision").toInt(), revision + 1);
    canvas->grab().save(evidence + "/11-clipping-canvas.png");
    menuClick(w->findChild<QAction *>("undoEdit"), &hooks, "되돌리기");
    QVERIFY(waitFor([&] { return !w->busy() && attachment(psd(), "face-clip", "clip").value("end").toString() == "face"; }));
    QVERIFY(settle());
    menuClick(w->findChild<QAction *>("redoEdit"), &hooks, "다시 실행");
    QVERIFY(waitFor([&] { return !w->busy() && attachment(psd(), "face-clip", "clip").value("end").toString() == "eyes"; }));
    QVERIFY(settle());

    // PNG로 시퀀스…: three frames picked at once in the file dialog become the body's picture.
    QVERIFY(showPage("cutoutSequencePage"));
    QVERIFY(pickOrder("body"));
    const QString frameDir = work + "/frames-" + stamp;
    QVERIFY(QDir().mkpath(frameDir));
    const QList<QColor> colors{QColor(200, 60, 40), QColor(60, 160, 60), QColor(60, 80, 200)};
    for (int i = 0; i < colors.size(); ++i) {
      QImage frame(82, 162, QImage::Format_ARGB32);
      frame.fill(colors[i]);
      QVERIFY(frame.save(frameDir + QString("/walk-%1.png").arg(i + 1)));
    }
    onModal(&pollers, &errors, "sequence frames dialog", isFileDialog, [&](QWidget *d) {
      auto fd = static_cast<QFileDialog *>(d);
      fd->setDirectory(frameDir);
      QTest::qWait(80);
      auto edit = fd->findChild<QLineEdit *>("fileNameEdit");
      if (edit) edit->setText("\"walk-3.png\" \"walk-1.png\" \"walk-2.png\"");
      QTest::qWait(50);
      auto box = fd->findChild<QDialogButtonBox *>("buttonBox");
      QPushButton *open = box ? box->button(QDialogButtonBox::Open) : nullptr;
      if (edit && open && open->isEnabled()) { QTest::mouseClick(open, Qt::LeftButton); return; }
      hooks << "sequence QFileDialog accepted with selectFile()+accept()";
      fd->selectFile(frameDir + "/walk-1.png");
      static_cast<QDialog *>(fd)->accept();
    });
    click("cutoutSequenceImport");
    QVERIFY2(waitFor([&] { return !w->busy() && attachment(psd(), "body", "body").value("sequence").toObject().value("frames").toArray().size() == 3; }, 30000),
             qPrintable(status->text() + w->lastError + noErrors()));
    QVERIFY(settle());
    const auto body = attachment(psd(), "body", "body");
    const QJsonArray frames = body.value("sequence").toObject().value("frames").toArray();
    QCOMPARE(body.value("imageAssetId").toString(), frames[0].toString());
    for (auto f : frames) QVERIFY2(QFileInfo::exists(projectRoot + "/assets/" + f.toString() + ".png"), qPrintable(f.toString()));
    QCOMPARE(panel->findChild<QListWidget *>("cutoutSequenceFrames")->count(), 3);
    // 시퀀스 키 in the idle clip: loop, 120 ms frames from frame 0.
    click("cutoutModeAnimate");
    QVERIFY(settle());
    QVERIFY(pickOrder("body"));
    typeSpin(panel->findChild<QSpinBox *>("cutoutTimeField"), "0");
    QVERIFY(settle());
    QVERIFY(choose(panel->findChild<QComboBox *>("cutoutSequenceMode"), "loop"));
    typeSpin(panel->findChild<QDoubleSpinBox *>("cutoutSequenceDelay"), "120");
    revision = w->currentView().value("revision").toInt();
    click("cutoutSequenceKey");
    QVERIFY2(waitFor([&] { return !w->busy() && !keyAt(psd(), "idle", "slot:body", "sequence", 0).isEmpty(); }, 10000), qPrintable(status->text()));
    QVERIFY(settle());
    QCOMPARE(w->currentView().value("revision").toInt(), revision + 1);
    const auto key = keyAt(psd(), "idle", "slot:body", "sequence", 0);
    QCOMPARE(key.value("mode").toString(), QString("loop"));
    QCOMPARE(key.value("delay").toDouble(), 120.0);
    QVERIFY(shot("11-clipping-sequence.png"));
    menuClick(w->findChild<QAction *>("undoEdit"), &hooks, "되돌리기");
    QVERIFY(waitFor([&] { return !w->busy() && keyAt(psd(), "idle", "slot:body", "sequence", 0).isEmpty(); }));
    QVERIFY(settle());
    menuClick(w->findChild<QAction *>("redoEdit"), &hooks, "다시 실행");
    QVERIFY(waitFor([&] { return !w->busy() && !keyAt(psd(), "idle", "slot:body", "sequence", 0).isEmpty(); }));
    QVERIFY(settle());
    for (auto d : w->currentView().value("diagnostics").toArray())
      QVERIFY2(!(d.toObject().value("severity") == "error" && d.toObject().value("file").toString().contains("psd-hero")), qPrintable(d.toObject().value("message").toString()));
    click("cutoutModeSetup");
    QVERIFY(settle());
    QVERIFY2(saveProject(), qPrintable(w->lastError));
  }

  // ---- 13. 사슬 IK · 늘이기 · 물리 (plugin 1.13.0) on the imported character: one edit per gesture ----
  step("13 chain IK and physics");
  {
    const QString psdRig = "plugins/cutout-animation/rig/psd-hero.json";
    auto psd = [&] { return pluginData(w->currentView(), psdRig); };
    auto tabs = panel->findChild<QTabWidget *>("cutoutLookTabs");
    auto showPage = [&](const char *name) {
      // Plugin 1.20.0: the page selector's button (the tab bar is hidden), clicked like a user.
      QWidget *page = panel->findChild<QWidget *>(name);
      auto button = panel->findChild<QToolButton *>(QString(name) + "Tab");
      if (button && button->isVisible()) QTest::mouseClick(button, Qt::LeftButton, Qt::NoModifier, button->rect().center());
      if (tabs->currentWidget() != page) { hooks << QString("look page %1 chosen with setCurrentWidget() (its selector button was not clickable)").arg(name); tabs->setCurrentWidget(page); }
      return waitFor([&] { return page->isVisible(); }, 2000);
    };
    click("cutoutModeSetup");
    QVERIFY(settle());
    QVERIFY(showPage("cutoutIkPage"));
    // root + Ctrl+click head in the hierarchy: the run becomes a chain (two bones = 팔·다리 solve).
    QVERIFY(select("bone:root"));
    auto head = treeItem("bone:head");
    QVERIFY(head);
    tree->scrollToItem(head);
    const QRect hr = tree->visualItemRect(head);
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::ControlModifier, QPoint(hr.left() + 16, hr.center().y()));
    QTest::qWait(60);
    int revision = w->currentView().value("revision").toInt();
    click("cutoutNewChainIk");
    QVERIFY2(waitFor([&] { return !w->busy() && w->currentView().value("revision").toInt() == revision + 1; }, 10000), qPrintable(status->text() + noErrors()));
    QVERIFY(settle());
    const QJsonArray ik = psd().value("ik").toArray();
    QVERIFY2(!ik.isEmpty() && ik.last().toObject().value("bones").toArray() == QJsonArray({"root", "head"}), qPrintable(QJsonDocument(psd().value("ik").toArray()).toJson()));
    const QString ikId = ik.last().toObject().value("id").toString();
    auto ikOf = [&] { for (auto k : psd().value("ik").toArray()) if (k.toObject().value("id").toString() == ikId) return k.toObject(); return QJsonObject(); };
    revision = w->currentView().value("revision").toInt();
    click("cutoutIkStretch");
    QVERIFY2(waitFor([&] { return !w->busy() && ikOf().value("stretch").toBool(); }, 10000), qPrintable(status->text()));
    QVERIFY(settle());
    QCOMPARE(w->currentView().value("revision").toInt(), revision + 1);
    // 물리 추가 on the head with the 장식 흔들림 preset, then one field.
    QVERIFY(showPage("cutoutPhysicsPage"));
    QVERIFY(select("bone:head"));
    QVERIFY(choose(panel->findChild<QComboBox *>("cutoutPhysicsPreset"), "accessory"));
    revision = w->currentView().value("revision").toInt();
    click("cutoutAddPhysics");
    QVERIFY2(waitFor([&] { return !w->busy() && psd().value("physics").toArray().size() == 1; }, 10000), qPrintable(status->text() + noErrors()));
    QVERIFY(settle());
    QCOMPARE(w->currentView().value("revision").toInt(), revision + 1);
    auto physics = [&] { return psd().value("physics").toArray().first().toObject(); };
    QCOMPARE(physics().value("bone").toString(), QString("head"));
    QCOMPARE(physics().value("damping").toDouble(), 0.92);
    typeSpin(panel->findChild<QDoubleSpinBox *>("cutoutPhysicsStrength"), "80");
    QVERIFY2(waitFor([&] { return !w->busy() && physics().value("strength").toDouble() == 80; }, 10000), qPrintable(status->text()));
    QVERIFY(settle());
    // Preview playback simulates (the info line reports the steps); stopping goes back to the clip pose.
    click("cutoutModeAnimate");
    QVERIFY(settle());
    click("cutoutPlay");
    auto info = panel->findChild<QLabel *>("cutoutPhysicsInfo");
    QVERIFY2(waitFor([&] { return info->text().contains(QString::fromUtf8("미리 보기")); }, 5000), qPrintable(info->text()));
    QTest::qWait(300);
    canvas->grab().save(evidence + "/12-physics-canvas.png");
    click("cutoutPlay");
    QVERIFY(waitFor([&] { return !info->text().contains(QString::fromUtf8("미리 보기")); }, 5000));
    menuClick(w->findChild<QAction *>("undoEdit"), &hooks, "되돌리기");
    QVERIFY(waitFor([&] { return !w->busy() && physics().value("strength").toDouble() != 80; }));
    QVERIFY(settle());
    menuClick(w->findChild<QAction *>("redoEdit"), &hooks, "다시 실행");
    QVERIFY(waitFor([&] { return !w->busy() && physics().value("strength").toDouble() == 80; }));
    QVERIFY(settle());
    for (auto d : w->currentView().value("diagnostics").toArray())
      QVERIFY2(!(d.toObject().value("severity") == "error" && d.toObject().value("file").toString().contains("psd-hero")), qPrintable(d.toObject().value("message").toString()));
    click("cutoutModeSetup");
    QVERIFY(settle());
    QVERIFY2(saveProject(), qPrintable(w->lastError));
  }

  // Hand-off for the browser / exported-player checks and the reusable demo copy.
  QJsonObject manifest{{"projectRoot", projectRoot}, {"windows", exported["windows"]}, {"web", exported["web"]}, {"buttonId", buttonId},
                       {"actionId", actionId}, {"instance", instanceId}, {"characterElement", characterId}, {"evidence", evidence},
                       {"importedAssetIds", QJsonArray::fromStringList(QStringList(imported.values()))}, {"hooks", QJsonArray::fromStringList(hooks)},
                       {"log", QJsonArray::fromStringList(log)}};
  QFile latest(work + "/latest.json");
  QVERIFY(latest.open(QIODevice::WriteOnly));
  latest.write(QJsonDocument(manifest).toJson());
  latest.close();
  QFile scenario(evidence + "/scenario-log.json");
  QVERIFY(scenario.open(QIODevice::WriteOnly));
  scenario.write(QJsonDocument(manifest).toJson());
  scenario.close();
  QVERIFY2(errors.isEmpty(), qPrintable(noErrors()));
}
