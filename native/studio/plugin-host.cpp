#include "plugin-host.hpp"
#include "studio.hpp"
#include <QActionGroup>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPluginLoader>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <vector>

// One host per loaded plugin so everything the DLL handed us (callbacks, docks,
// menus, workspace widgets) can be destroyed while its code is still mapped, before unload().
// It also serves the optional workspace interface; the plugin only ever sees the
// StudioWorkspaceHost* it was given, so no cast crosses the DLL boundary.
class StudioPluginManager::Host final : public raising::StudioHost, public raising::StudioWorkspaceHost {
public:
  using Pending = std::shared_ptr<std::function<void(QJsonObject)>>;
  Host(StudioPluginManager *owner, QString pluginId) : manager(owner), id(std::move(pluginId)) {}
  ~Host() override { shutdown(); }
  QJsonObject view() const override { return manager->window->view; }
  QJsonArray assets() const override { return manager->window->assets; }
  bool busy() const override { return manager->window->bridge.busy(); }
  QWidget *window() const override { return manager->window; }
  QString engineRoot() const override { return manager->window->engineRoot; }
  QString projectRoot() const override { return manager->window->view.value("root").toString(); }
  QJsonObject selectedElement() const override { return manager->window->currentElement(); }
  QString pluginDirectory(const QString &pluginId) const override {
    const auto entry = manager->projectEntry(pluginId);
    if (!entry.value("directory").toString().isEmpty()) return entry.value("directory").toString();
    return manager->installedEntry(pluginId, entry.value("version").toString()).value("directory").toString();
  }
  void request(const QString &method, const QJsonObject &params, std::function<void(QJsonObject)> done) override {
    auto slot = std::make_shared<std::function<void(QJsonObject)>>(std::move(done));
    pending.erase(std::remove_if(pending.begin(), pending.end(), [](const Pending &p) { return !*p; }), pending.end());
    pending.push_back(slot);
    std::weak_ptr<std::function<void(QJsonObject)>> weak = slot;
    auto deliver = [owner = QPointer<StudioPluginManager>(manager), plugin = id, weak](QJsonObject reply) {
      auto alive = weak.lock();
      // The plugin may have been unloaded while the request was in flight.
      if (!owner || !alive) return;
      auto call = std::move(*alive);
      *alive = nullptr;
      if (call) owner->guarded(plugin, [&] { call(reply); });
    };
    // StudioWindow::request silently drops a call while busy; plugins always get an answer.
    if (manager->window->bridge.busy()) {
      QTimer::singleShot(0, manager, [deliver] { deliver({{"error", QObject::tr("이전 작업이 끝난 뒤 다시 시도해 주세요.")}}); });
      return;
    }
    manager->window->request(method, params, deliver);
  }
  void addDock(const QString &title, const QString &objectName, QWidget *widget, Qt::DockWidgetArea area) override {
    docks.append(manager->window->addStudioDock(title, objectName, widget, area));
  }
  void showWorkspace(const QString &workspace) override {
    manager->window->showWorkspace(workspace.isEmpty() ? QString() : id + "/" + workspace);
  }
  QString currentWorkspace() const override {
    const auto active = manager->window->currentWorkspace();
    return active.startsWith(id + "/") ? active.mid(id.size() + 1) : QString();
  }
  QMenu *pluginMenu(const QString &pluginId) override {
    if (!menu) {
      const auto entry = manager->installedEntry(pluginId, manager->projectEntry(pluginId).value("version").toString());
      menu = manager->window->pluginsMenu->addMenu(entry.value("name").toString(pluginId));
      menu->setObjectName("pluginMenu-" + pluginId);
    }
    return menu;
  }
  void shutdown() {
    // Destroying std::function / widgets runs code from the plugin DLL: do it now.
    pending.clear();
    // Back to the screen editor (exact dock layout) before the workspace pages die.
    manager->window->removeWorkspaces(id);
    for (auto &dock : docks) delete dock.data();
    docks.clear();
    delete menu.data();
  }
  StudioPluginManager *manager;
  QString id;
  std::vector<Pending> pending;
  QList<QPointer<QDockWidget>> docks;
  QPointer<QMenu> menu;
};

struct StudioPluginManager::Record {
  QString file;
  std::unique_ptr<QPluginLoader> loader;
  raising::StudioPlugin *plugin = nullptr;
  raising::StudioWorkspacePlugin *workspaces = nullptr;  // optional second interface
  QObject *root = nullptr;
  std::unique_ptr<Host> host;
};
static QSet<QObject *> &activeRoots() {
  static QSet<QObject *> roots;
  return roots;
}

StudioPluginManager::StudioPluginManager(StudioWindow *owner) : QObject(owner), window(owner) {}
StudioPluginManager::~StudioPluginManager() { unloadAll(); }

template <class F> bool StudioPluginManager::guarded(const QString &id, F &&call) {
  ++depth;
  bool ok = true;
  try { call(); } catch (...) { ok = false; }
  --depth;
  if (!ok) emit notice(tr("플러그인 %1 처리 중 오류가 발생했습니다. 편집은 계속할 수 있습니다.").arg(id));
  if (!depth && !deferredUnloads.isEmpty()) QTimer::singleShot(0, this, &StudioPluginManager::flushDeferred);
  return ok;
}
void StudioPluginManager::flushDeferred() {
  if (depth) return;
  const auto ids = deferredUnloads;
  deferredUnloads.clear();
  for (const auto &id : ids) unload(id);
}

QJsonObject StudioPluginManager::projectEntry(const QString &id) const {
  for (auto v : pluginState.value("project").toArray())
    if (v.toObject().value("id") == id) return v.toObject();
  return {};
}
QJsonObject StudioPluginManager::installedEntry(const QString &id, const QString &version) const {
  QJsonObject any;
  for (auto v : pluginState.value("installed").toArray()) {
    const auto e = v.toObject();
    if (e.value("id") != id) continue;
    if (version.isEmpty() || e.value("version") == version) return e;
    if (any.isEmpty()) any = e;
  }
  // A manifest of another installed version still gives a readable label.
  return version.isEmpty() ? any : QJsonObject{{"id", id}, {"name", any.value("name")}};
}
raising::StudioPlugin *StudioPluginManager::plugin(const QString &id) const {
  const auto it = loaded.find(id);
  return it == loaded.end() || deferredUnloads.contains(id) ? nullptr : it.value()->plugin;
}
QJsonArray StudioPluginManager::activeElementTypes() const {
  QJsonArray out;
  for (auto v : pluginState.value("project").toArray()) {
    const auto entry = v.toObject();
    if (entry.value("state") != "active") continue;
    const auto id = entry.value("id").toString();
    for (auto t : installedEntry(id, entry.value("version").toString()).value("elementTypes").toArray()) {
      const auto type = t.toObject();
      if (type.value("type").toString().isEmpty()) continue;
      out.append(QJsonObject{{"plugin", id}, {"type", type.value("type")},
                             {"label", type.value("label").toString(type.value("type").toString())},
                             {"refType", type.value("refType")}});
    }
  }
  return out;
}
QString StudioPluginManager::refType(const QString &plugin, const QString &type) const {
  for (auto t : installedEntry(plugin, projectEntry(plugin).value("version").toString()).value("elementTypes").toArray())
    if (t.toObject().value("type") == type) return t.toObject().value("refType").toString();
  return {};
}
QJsonArray StudioPluginManager::rows() const {
  QJsonArray out;
  QSet<QString> pinned;
  for (auto v : pluginState.value("project").toArray()) {
    auto entry = v.toObject();
    const auto id = entry.value("id").toString();
    pinned.insert(id + "@" + entry.value("version").toString());
    entry["name"] = installedEntry(id, entry.value("version").toString()).value("name").toString(id);
    entry["inProject"] = true;
    entry["loaded"] = loaded.contains(id);
    if (!loadError(id).isEmpty()) entry["loadError"] = loadError(id);
    out.append(entry);
  }
  for (auto v : pluginState.value("installed").toArray()) {
    auto entry = v.toObject();
    if (pinned.contains(entry.value("id").toString() + "@" + entry.value("version").toString())) continue;
    entry["state"] = "available";
    entry["inProject"] = false;
    out.append(entry);
  }
  return out;
}
QString StudioPluginManager::placeholder(const QJsonObject &element) const {
  const auto id = element.value("extension").toObject().value("plugin").toString();
  if (id.isEmpty()) return tr("플러그인 정보 없음");
  const auto entry = projectEntry(id);
  const auto state = entry.value("state").toString();
  if (entry.isEmpty() || state == "missing" || state == "notLocked") return tr("플러그인 필요: %1").arg(id);
  if (state == "disabled") return tr("사용 중지됨: %1").arg(id);
  if (state != "active") return tr("호환되지 않는 플러그인: %1").arg(id);
  if (plugin(id)) return {};
  if (!loadError(id).isEmpty()) return tr("플러그인 불러오기 실패: %1").arg(id);
  return tr("편집 미리보기 없음: %1").arg(id);
}
bool StudioPluginManager::paint(QPainter *painter, const QJsonObject &element, const QRectF &rect) {
  const auto id = element.value("extension").toObject().value("plugin").toString();
  auto target = plugin(id);
  if (!target) return false;
  bool painted = false;
  painter->save();
  const bool ok = guarded(id, [&] { painted = target->paintElement(painter, element, rect); });
  painter->restore();
  return ok && painted;
}
void StudioPluginManager::openEditor(const QString &pluginId, const QString &documentType, const QString &documentId) {
  if (auto target = plugin(pluginId)) guarded(pluginId, [&] { target->openEditor(documentType, documentId); });
}
void StudioPluginManager::notifyViewChanged() {
  for (const auto &id : loaded.keys())
    if (auto target = plugin(id)) guarded(id, [&] { target->viewChanged(); });
}

bool StudioPluginManager::load(const QString &id, const QString &file, QString *error) {
  auto fail = [&](const QString &message) {
    failures[id] = {file, message};
    if (error) *error = message;
    emit notice(tr("플러그인 %1 편집 도구를 불러오지 못했습니다: %2").arg(id, message));
    return false;
  };
  if (loaded.contains(id)) unload(id);
  if (loaded.contains(id)) return fail(tr("이전 편집 도구를 닫는 중입니다. 잠시 후 다시 시도하세요"));
  if (!QFileInfo(file).isFile()) return fail(tr("파일이 없습니다 (%1)").arg(file));
  auto record = std::make_shared<Record>();
  record->file = file;
  record->loader = std::make_unique<QPluginLoader>(file);
  // metaData() is read without running any code from the file.
  const auto iid = record->loader->metaData().value("IID").toString();
  if (iid != QLatin1String(RaisingStudioPlugin_iid))
    return fail(iid.isEmpty() ? tr("Qt 스튜디오 플러그인이 아닙니다") : tr("지원하지 않는 플러그인 인터페이스 %1").arg(iid));
  auto root = record->loader->instance();
  auto target = qobject_cast<raising::StudioPlugin *>(root);
  // Qt keeps one root instance per library and process; it cannot serve two windows.
  if (target && activeRoots().contains(root)) {
    record->loader->unload();
    return fail(tr("다른 편집기 창에서 이미 사용 중입니다"));
  }
  if (!target) {
    const auto message = record->loader->errorString();
    record->loader->unload();
    return fail(message);
  }
  QString pluginId;
  int api = 0;
  if (!guarded(id, [&] { pluginId = target->pluginId(); api = target->apiVersion(); })) {
    record->loader->unload();
    return fail(tr("플러그인 정보를 읽을 수 없습니다"));
  }
  if (pluginId != id || api != raising::STUDIO_PLUGIN_API) {
    record->loader->unload();
    return fail(pluginId != id ? tr("플러그인 ID가 다릅니다 (%1)").arg(pluginId)
                               : tr("편집 API %1을 지원하지 않습니다").arg(api));
  }
  record->plugin = target;
  record->root = root;
  activeRoots().insert(root);
  record->host = std::make_unique<Host>(this, id);
  failures.remove(id);
  loaded[id] = record;
  if (!guarded(id, [&] { target->activate(record->host.get()); })) {
    unload(id);
    return fail(tr("활성화 중 오류"));
  }
  // Workspaces are optional: a raising-studio-plugin-1 DLL without the interface keeps its docks.
  if (auto workspaces = qobject_cast<raising::StudioWorkspacePlugin *>(root)) {
    QList<raising::StudioWorkspace> offered;
    if (guarded(id, [&] { offered = workspaces->createWorkspaces(record->host.get()); })) {
      record->workspaces = workspaces;
      for (const auto &workspace : offered) window->addWorkspace(id, workspace);
    }
  }
  return true;
}
void StudioPluginManager::workspaceChanged(const QString &key, bool shown) {
  const auto id = key.section('/', 0, 0);
  const auto it = loaded.find(id);
  if (it == loaded.end() || !plugin(id) || !it.value()->workspaces) return;
  auto target = it.value()->workspaces;
  guarded(id, [&] { target->workspaceChanged(key.section('/', 1), shown); });
}
void StudioPluginManager::unload(const QString &id) {
  if (!loaded.contains(id)) return;
  if (depth) {
    if (!deferredUnloads.contains(id)) deferredUnloads.append(id);
    return;
  }
  deferredUnloads.removeAll(id);
  auto record = loaded.take(id);
  ++depth;
  try { record->plugin->deactivate(); } catch (...) {}
  --depth;
  // Order matters: plugin widgets and callbacks die first, then the DLL.
  record->host.reset();
  record->plugin = nullptr;
  activeRoots().remove(record->root);
  record->loader->unload();
}
void StudioPluginManager::unloadAll() {
  for (const auto &id : loaded.keys()) {
    deferredUnloads.removeAll(id);
    const int saved = depth;
    depth = 0;  // closing the project/editor must not leave a DLL behind
    unload(id);
    depth = saved;
  }
}
void StudioPluginManager::sync(const QString &projectRoot) {
  if (projectRoot != loadedRoot) {
    unloadAll();
    failures.clear();
    loadedRoot = projectRoot;
  }
  QMap<QString, QString> wanted;
  if (!projectRoot.isEmpty())
    for (auto v : pluginState.value("project").toArray()) {
      const auto entry = v.toObject();
      const auto dll = entry.value("authoring").toString();
      if (entry.value("state") == "active" && !dll.isEmpty()) wanted[entry.value("id").toString()] = dll;
    }
  for (const auto &id : loaded.keys())
    if (wanted.value(id) != loaded[id]->file) unload(id);
  for (auto it = wanted.begin(); it != wanted.end(); ++it) {
    if (loaded.contains(it.key()) && !deferredUnloads.contains(it.key())) continue;
    // Retry a failed DLL only when the service points at another file.
    if (failures.contains(it.key()) && failures[it.key()].first == it.value()) continue;
    if (!deferredUnloads.contains(it.key())) load(it.key(), it.value());
  }
  for (const auto &id : failures.keys())
    if (!wanted.contains(id)) failures.remove(id);
}

// ---- StudioWindow plugin UI ---------------------------------------------------

void StudioWindow::setupWorkspaceUi() {
  // Spine-like switch in the menu bar corner; hidden until a plugin offers a workspace so a
  // studio without such plugins looks exactly as before.
  workspaceSwitcher = new QTabBar;
  workspaceSwitcher->setObjectName("studioWorkspaceSwitcher");
  workspaceSwitcher->setDrawBase(false);
  workspaceSwitcher->setExpanding(false);
  workspaceSwitcher->setDocumentMode(true);
  workspaceSwitcher->setFocusPolicy(Qt::NoFocus);
  workspaceSwitcher->hide();
  menuBar()->setCornerWidget(workspaceSwitcher, Qt::TopRightCorner);
  connect(workspaceSwitcher, &QTabBar::tabBarClicked, this, [this](int index) {
    if (index >= 0) showWorkspace(workspaceSwitcher->tabData(index).toString());
  });
  workspaceMenu = new QMenu(tr("작업 공간"), this);
  workspaceMenu->setObjectName("studioWorkspaceMenu");
  viewMenu->insertMenu(viewMenu->actions().value(0), workspaceMenu);
  viewMenu->insertSeparator(viewMenu->actions().value(1));
  workspaceGroup = new QActionGroup(this);
  workspaceGroup->setExclusive(true);
  // Remembered per project so reopening it comes back to the same work area.
  if (!qEnvironmentVariableIsSet("RAISING_QT_TEST")) {
    QSettings settings;
    wantedWorkspace = settings.value("workspace").toString();
    wantedWorkspaceRoot = settings.value("workspaceRoot").toString();
  }
  rebuildWorkspaceUi();
}
QStringList StudioWindow::workspaceKeys() const {
  QStringList keys;
  for (const auto &w : workspaces) keys << w.key;
  return keys;
}
void StudioWindow::rebuildWorkspaceUi() {
  for (auto action : workspaceGroup->actions()) { workspaceGroup->removeAction(action); removeAction(action); action->deleteLater(); }
  workspaceMenu->clear();
  {
    const QSignalBlocker block(workspaceSwitcher);
    while (workspaceSwitcher->count()) workspaceSwitcher->removeTab(0);
  }
  struct Row { QString key, title, name; QKeySequence shortcut; };
  QList<Row> rows{{QString(), tr("화면 편집"), "studioWorkspace-screen", QKeySequence()}};
  for (const auto &w : workspaces)
    rows.append({w.key, w.title, "studioWorkspace-" + QString(w.key).replace('/', '-'), w.shortcut});
  for (const auto &row : rows) {
    auto action = workspaceMenu->addAction(row.title);
    action->setObjectName(row.name);
    action->setCheckable(true);
    action->setChecked(row.key == activeWorkspace);
    action->setData(row.key);
    workspaceGroup->addAction(action);
    if (!row.shortcut.isEmpty()) {
      action->setShortcut(row.shortcut);
      // Window-level so the key works whichever area has focus (docks are hidden in a workspace).
      QWidget::addAction(action);  // (the member `addAction` is the ＋ 버튼 action)
    }
    // A plugin workspace shortcut toggles back to the screen editor when pressed again.
    connect(action, &QAction::triggered, this, [this, key = row.key] { showWorkspace(key == activeWorkspace && !key.isEmpty() ? QString() : key); });
    const int tab = workspaceSwitcher->addTab(row.title);
    workspaceSwitcher->setTabData(tab, row.key);
    workspaceSwitcher->setTabToolTip(tab, row.shortcut.isEmpty() ? row.title : row.title + " (" + row.shortcut.toString(QKeySequence::NativeText) + ")");
    if (row.key == activeWorkspace) {
      const QSignalBlocker block(workspaceSwitcher);
      workspaceSwitcher->setCurrentIndex(tab);
    }
  }
  workspaceSwitcher->setVisible(!workspaces.isEmpty());
  workspaceMenu->menuAction()->setVisible(!workspaces.isEmpty());
}
QWidget *StudioWindow::addWorkspace(const QString &plugin, const raising::StudioWorkspace &workspace) {
  if (!workspace.widget || workspace.id.isEmpty() || workspace.id.contains('/')) return nullptr;
  const auto key = plugin + "/" + workspace.id;
  if (workspaceKeys().contains(key)) return nullptr;
  auto page = new QWidget;
  page->setObjectName("studioWorkspacePage-" + plugin + "-" + workspace.id);
  auto layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(workspace.widget);
  centralStack->addWidget(page);
  workspaces.append({key, plugin, workspace.title.isEmpty() ? workspace.id : workspace.title, page, workspace.shortcut});
  rebuildWorkspaceUi();
  // Reopening the project that was left inside this workspace comes back to it.
  const auto root = QDir::cleanPath(view.value("root").toString());
  if (activeWorkspace.isEmpty() && wantedWorkspace == key && !root.isEmpty() && root.compare(QDir::cleanPath(wantedWorkspaceRoot), Qt::CaseInsensitive) == 0)
    showWorkspace(key);
  return page;
}
void StudioWindow::removeWorkspaces(const QString &plugin) {
  if (activeWorkspace.section('/', 0, 0) == plugin) {
    // The plugin is going away (project closed, disabled): not the user's choice, so the
    // wanted workspace is kept for the next time this project loads the plugin.
    const auto keep = wantedWorkspace, keepRoot = wantedWorkspaceRoot;
    showWorkspace({});
    wantedWorkspace = keep;
    wantedWorkspaceRoot = keepRoot;
  }
  bool changed = false;
  for (int i = workspaces.size() - 1; i >= 0; --i) {
    if (workspaces[i].plugin != plugin) continue;
    delete workspaces[i].page.data();  // runs plugin widget destructors while the DLL is mapped
    workspaces.removeAt(i);
    changed = true;
  }
  if (changed) rebuildWorkspaceUi();
}
bool StudioWindow::showWorkspace(const QString &key) {
  QWidget *page = screenEditor;
  if (!key.isEmpty()) {
    page = nullptr;
    for (const auto &w : workspaces)
      if (w.key == key) page = w.page;
    if (!page) return false;
  }
  if (key != activeWorkspace) {
    const auto previous = activeWorkspace;
    if (previous.isEmpty()) {
      screenLayout = saveState(1);
      // Docks (plugin ones too) belong to the screen editor; floating ones would cover the workspace.
      for (auto dock : findChildren<QDockWidget *>(Qt::FindDirectChildrenOnly)) dock->hide();
    }
    for (auto action : std::as_const(screenOnlyActions)) action->setVisible(key.isEmpty());
    centralStack->setCurrentWidget(page);
    activeWorkspace = key;
    if (key.isEmpty()) {
      // The re-shown tool bar entries change the tool bar height: lay it out first so the
      // saved dock sizes land on the same central area they were taken from.
      layout()->activate();
      restoreState(screenLayout, 1);
    }
    if (!previous.isEmpty()) plugins->workspaceChanged(previous, false);
    if (!key.isEmpty()) plugins->workspaceChanged(key, true);
  }
  wantedWorkspace = key;
  wantedWorkspaceRoot = key.isEmpty() ? QString() : view.value("root").toString();
  for (auto action : workspaceGroup->actions()) action->setChecked(action->data().toString() == activeWorkspace);
  const QSignalBlocker block(workspaceSwitcher);
  for (int i = 0; i < workspaceSwitcher->count(); ++i)
    if (workspaceSwitcher->tabData(i).toString() == activeWorkspace) workspaceSwitcher->setCurrentIndex(i);
  return true;
}

QDockWidget *StudioWindow::addStudioDock(const QString &title, const QString &name, QWidget *child, Qt::DockWidgetArea area) {
  auto d = new QDockWidget(title, this);
  d->setObjectName(name);
  d->setWidget(child);
  // Plugin docks appear after restoreState(); reuse a saved position when one exists.
  if (!restoreDockWidget(d)) addDockWidget(area, d);
  viewMenu->addAction(d->toggleViewAction());
  return d;
}
void StudioWindow::applyPluginState(const QJsonObject &state, bool load) {
  plugins->setState(state);
  rebuildPluginUi();
  if (!load) return;
  const auto before = plugins->loadedIds();
  plugins->sync(view.value("root").toString());
  rebuildPluginUi();
  // Only a (un)loaded DLL changes the inspector; refilling it needlessly would
  // disturb the focus/cursor that request() just restored.
  if (plugins->loadedIds() != before && currentElement().value("kind") == "plugin") updateInspector();
  canvas->viewport()->update();
  plugins->notifyViewChanged();
}
void StudioWindow::rebuildPluginUi() {
  for (auto action : paletteMenu->actions())
    if (action->property("pluginElement").toBool()) { paletteMenu->removeAction(action); action->deleteLater(); }
  const auto types = plugins->activeElementTypes();
  if (!types.isEmpty()) {
    auto separator = paletteMenu->addSeparator();
    separator->setProperty("pluginElement", true);
  }
  for (auto v : types) {
    const auto t = v.toObject();
    const auto plugin = t.value("plugin").toString(), type = t.value("type").toString();
    auto action = paletteMenu->addAction(t.value("label").toString(), this, [this, plugin, type] { addPluginElement(plugin, type); });
    action->setObjectName("pluginElement-" + plugin + "-" + type);
    action->setProperty("pluginElement", true);
    action->setToolTip(tr("플러그인 %1").arg(plugin));
  }
}
void StudioWindow::addPluginElement(const QString &plugin, const QString &type) {
  if (bridge.busy() || currentScreen().isEmpty()) return;
  auto p = projectData();
  auto visual = p.value("visual").toObject();
  auto screens = visual.value("screens").toArray();
  auto elements = currentScreen().value("elements").toArray();
  if (elements.size() >= 100) { errorLabel->setText(tr("화면당 최대 100개 요소를 지원합니다.")); return; }
  QString label = type;
  for (auto v : plugins->activeElementTypes())
    if (v.toObject().value("plugin") == plugin && v.toObject().value("type") == type) label = v.toObject().value("label").toString();
  // Start linked to the first existing document so the element previews immediately.
  const auto refType = plugins->refType(plugin, type);
  QString ref;
  for (auto d : view.value("documents").toArray()) {
    const auto doc = d.toObject(), data = doc.value("data").toObject();
    if (doc.value("kind") == "extension" && data.value("plugin") == plugin && data.value("type") == refType) { ref = data.value("id").toString(); break; }
  }
  QSet<QString> ids;
  for (auto e : elements) ids.insert(e.toObject().value("id").toString());
  int n = 1;
  while (ids.contains(type + "-" + QString::number(n))) ++n;
  selection = type + "-" + QString::number(n);
  const int height = qMin(450, qMax(64, visual.value("height").toInt(720) - 100));
  elements.append(QJsonObject{{"id", selection}, {"kind", "plugin"}, {"label", label}, {"x", 80}, {"y", 100}, {"width", 300}, {"height", height},
                              {"extension", QJsonObject{{"plugin", plugin}, {"type", type}, {"ref", ref}, {"props", QJsonObject{}}}}});
  for (int i = 0; i < screens.size(); ++i) {
    auto s = screens[i].toObject();
    if (s.value("id") == screenId) { s["elements"] = elements; screens[i] = s; }
  }
  visual["screens"] = screens;
  p["visual"] = visual;
  replaceProject(p);
}
void StudioWindow::updatePluginInspector(const QJsonObject &e) {
  const bool isPlugin = e.value("kind") == "plugin";
  pluginGroup->setVisible(isPlugin);
  pluginRefField->clear();
  if (!isPlugin) return;
  const auto ext = e.value("extension").toObject();
  const auto plugin = ext.value("plugin").toString(), type = ext.value("type").toString(), ref = ext.value("ref").toString();
  const auto refType = plugins->refType(plugin, type);
  const auto caption = plugins->placeholder(e);
  pluginStatusLabel->setText(QString("%1 · %2\n%3").arg(plugin, type, caption.isEmpty() ? tr("편집 도구 사용 가능") : caption));
  pluginRefField->addItem(tr("연결 없음"), QString());
  for (auto d : view.value("documents").toArray()) {
    const auto doc = d.toObject(), data = doc.value("data").toObject();
    if (doc.value("kind") != "extension" || data.value("plugin") != plugin || refType.isEmpty() || data.value("type") != refType) continue;
    const auto id = data.value("id").toString(), name = data.value("data").toObject().value("name").toString();
    pluginRefField->addItem(name.isEmpty() ? id : name + " · " + id, id);
  }
  if (!ref.isEmpty() && pluginRefField->findData(ref) < 0) pluginRefField->addItem(tr("없는 문서: ") + ref, ref);
  pluginRefField->setCurrentIndex(qMax(0, pluginRefField->findData(ref)));
  pluginRefField->setEnabled(!refType.isEmpty());
  const auto props = QString::fromUtf8(QJsonDocument(ext.value("props").toObject()).toJson(QJsonDocument::Compact));
  if (pluginPropsField->text() != props) pluginPropsField->setText(props);
  pluginEditButton->setEnabled(plugins->plugin(plugin) != nullptr);
}
void StudioWindow::editPluginElement() {
  const auto ext = currentElement().value("extension").toObject();
  const auto plugin = ext.value("plugin").toString();
  if (!plugins->plugin(plugin)) { errorLabel->setText(plugins->placeholder(currentElement())); return; }
  plugins->openEditor(plugin, plugins->refType(plugin, ext.value("type").toString()), ext.value("ref").toString());
}
void StudioWindow::editPluginProps() {
  if (filling) return;
  QJsonParseError parse;
  const auto doc = QJsonDocument::fromJson(pluginPropsField->text().toUtf8(), &parse);
  bool valid = parse.error == QJsonParseError::NoError && doc.isObject() && doc.object().size() <= 32;
  if (valid)
    for (auto v : doc.object()) valid = valid && !v.isObject() && !v.isArray();
  if (!valid) { errorLabel->setText(tr("속성은 32개 이하의 문자·숫자·참/거짓 값을 가진 JSON 객체로 입력하세요.")); return; }
  const auto props = doc.object();
  updateElement([props](QJsonObject &e) { auto ext = e.value("extension").toObject(); ext["props"] = props; e["extension"] = ext; });
}
void StudioWindow::showPluginManager() {
  QDialog dialog(this);
  dialog.setObjectName("pluginManager");
  dialog.setWindowTitle(tr("플러그인 관리"));
  dialog.resize(820, 520);
  auto layout = new QVBoxLayout(&dialog);
  auto summary = new QLabel;
  summary->setObjectName("pluginManagerStatus");
  summary->setWordWrap(true);
  layout->addWidget(summary);
  auto table = new QTableWidget(0, 5);
  table->setObjectName("pluginTable");
  table->setHorizontalHeaderLabels({tr("ID"), tr("이름"), tr("버전"), tr("상태"), tr("설치 위치")});
  table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  table->horizontalHeader()->setStretchLastSection(true);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::SingleSelection);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  layout->addWidget(table, 1);
  auto diagnostics = new QPlainTextEdit;
  diagnostics->setObjectName("pluginDiagnostics");
  diagnostics->setReadOnly(true);
  diagnostics->setMaximumHeight(140);
  layout->addWidget(diagnostics);
  auto row = new QHBoxLayout;
  auto install = new QPushButton(tr("패키지 설치…"));
  install->setObjectName("pluginInstall");
  auto enable = new QPushButton(tr("이 프로젝트에서 사용"));
  enable->setObjectName("pluginEnable");
  auto disable = new QPushButton(tr("사용 중지"));
  disable->setObjectName("pluginDisable");
  auto close = new QDialogButtonBox(QDialogButtonBox::Close);
  row->addWidget(install);
  row->addWidget(enable);
  row->addWidget(disable);
  row->addStretch();
  row->addWidget(close);
  layout->addLayout(row);
  connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  static const QMap<QString, QString> states{{"active", tr("사용 중")}, {"disabled", tr("사용 중지됨")}, {"missing", tr("설치 필요")},
                                             {"incompatible", tr("호환되지 않음")}, {"notLocked", tr("잠금 파일에 없음 · 문서 보존")},
                                             {"available", tr("설치됨 · 프로젝트 미사용")}};
  auto selected = [table] { auto item = table->currentRow() < 0 ? nullptr : table->item(table->currentRow(), 0); return item ? item->data(Qt::UserRole).toJsonObject() : QJsonObject(); };
  auto describe = [this, diagnostics, enable, disable, selected] {
    const auto entry = selected();
    QStringList lines;
    auto add = [&lines](const QJsonValue &v) {
      const auto d = v.toObject();
      lines << (d.isEmpty() ? v.toString() : QStringList{d.value("severity").toString(), d.value("code").toString(), d.value("message").toString()}.join(" · "));
    };
    if (!entry.isEmpty()) {
      for (auto d : entry.value("diagnostics").toArray()) add(d);
      if (!entry.value("loadError").toString().isEmpty()) lines << tr("편집 도구: ") + entry.value("loadError").toString();
    }
    for (auto d : view.value("diagnostics").toArray()) {
      const auto v = d.toObject();
      if (!v.value("code").toString().startsWith("PLUGIN_")) continue;
      if (entry.isEmpty() || (v.value("message").toString() + v.value("file").toString()).contains(entry.value("id").toString())) add(v);
    }
    diagnostics->setPlainText(lines.isEmpty() ? tr("진단 없음") : lines.join('\n'));
    const bool ready = !bridge.busy() && !view.value("root").toString().isEmpty() && !entry.isEmpty();
    enable->setEnabled(ready && entry.value("state") != "active" && entry.value("state") != "missing");
    disable->setEnabled(ready && entry.value("inProject").toBool() && entry.value("enabled").toBool(entry.value("state") == "active"));
  };
  auto fill = [this, table, summary, describe, selected, install] {
    const auto keep = selected();
    const QSignalBlocker block(table);
    table->setRowCount(0);
    for (auto v : plugins->rows()) {
      const auto entry = v.toObject();
      const int r = table->rowCount();
      table->insertRow(r);
      auto state = states.value(entry.value("state").toString(), entry.value("state").toString());
      if (entry.value("state") == "active")
        state += entry.value("loaded").toBool() ? tr(" · 편집 도구 실행 중") : !entry.value("loadError").toString().isEmpty() ? tr(" · 편집 도구 오류") : entry.value("authoring").toString().isEmpty() ? tr(" · 편집 도구 없음") : QString();
      const QStringList cells{entry.value("id").toString(), entry.value("name").toString(), entry.value("version").toString(), state, entry.value("directory").toString()};
      for (int c = 0; c < cells.size(); ++c) table->setItem(r, c, new QTableWidgetItem(cells[c]));
      table->item(r, 0)->setData(Qt::UserRole, entry);
      if (entry.value("id") == keep.value("id") && entry.value("version") == keep.value("version")) table->setCurrentCell(r, 0);
    }
    const auto store = plugins->state().value("store").toString();
    summary->setText(plugins->state().isEmpty() ? tr("프로젝트 서비스가 플러그인 정보를 보내지 않았습니다. 플러그인 없이 편집합니다.")
                                                : tr("플러그인 저장소: %1\n엔진 플러그인 API %2").arg(store.isEmpty() ? tr("(알 수 없음)") : store).arg(plugins->state().value("engineApi").toVariant().toString()));
    install->setEnabled(!bridge.busy() && !plugins->state().isEmpty());
    describe();
  };
  connect(table, &QTableWidget::itemSelectionChanged, &dialog, describe);
  connect(this, &StudioWindow::updated, &dialog, fill);
  connect(install, &QPushButton::clicked, &dialog, [this, &dialog, summary] {
    const auto file = !pluginPackagePath.isEmpty() ? pluginPackagePath
                      : QFileDialog::getOpenFileName(&dialog, tr("플러그인 패키지 설치"), {}, tr("Raising 플러그인 (*.raiseplugin)"));
    if (file.isEmpty() || bridge.busy()) return;
    request("pluginInstall", {{"file", file}}, [summary](QJsonObject reply) {
      const auto done = reply.value("pluginInstalled").toObject();
      if (reply.value("error").toString().isEmpty() && !done.isEmpty())
        summary->setText(tr("설치됨: %1 %2 · ‘이 프로젝트에서 사용’으로 연결하세요.").arg(done.value("id").toString(), done.value("version").toString()));
    });
  });
  connect(enable, &QPushButton::clicked, &dialog, [this, selected] {
    const auto entry = selected();
    if (entry.isEmpty() || bridge.busy()) return;
    QJsonObject params{{"id", entry.value("id")}};
    if (!entry.value("version").toString().isEmpty()) params["version"] = entry.value("version");
    request("pluginEnable", params);
  });
  connect(disable, &QPushButton::clicked, &dialog, [this, selected] {
    const auto entry = selected();
    if (!entry.isEmpty() && !bridge.busy()) request("pluginDisable", {{"id", entry.value("id")}});
  });
  fill();
  // Refresh the store listing; an old service without plugin support is not asked.
  if (!bridge.busy() && plugins->state().contains("store")) request("pluginStatus");
  dialog.exec();
}
