#include "cutout-plugin.hpp"
#include <QAction>
#include <QDockWidget>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace cutout_studio {
void CutoutStudioPlugin::activate(raising::StudioHost* host) {
  host_ = host;
  panel_ = new CutoutPanel(host);
  // The dock owns a body widget, not the panel: a workspace host later moves the panel out
  // (createWorkspaces) and the body then shows the launcher instead.
  dockBody_ = new QWidget;
  dockBody_->setObjectName("cutoutDockBody");
  auto* body = new QVBoxLayout(dockBody_);
  body->setContentsMargins(0, 0, 0, 0);
  body->addWidget(panel_);
  host->addDock("캐릭터 애니메이션", "cutoutAnimationDock", dockBody_, Qt::BottomDockWidgetArea);
  if (QMenu* menu = host->pluginMenu(PLUGIN_ID)) {
    auto add = [&](const QString& text, const char* name, std::function<void()> fn) {
      QAction* a = menu->addAction(text);
      a->setObjectName(name);
      QObject::connect(a, &QAction::triggered, panel_, fn);
      actions_ << a;
    };
    add("캐릭터 애니메이션 창 열기", "cutoutOpenPanelAction", [this] { showDock(); });
    add("새 캐릭터…", "cutoutNewCharacterAction", [this] { showDock(); panel_->newCharacter(); });
    add("조립 모드", "cutoutSetupModeAction", [this] { showDock(); panel_->setMode(CutoutPanel::Mode::Setup); });
    add("애니메이션 모드", "cutoutAnimateModeAction", [this] { showDock(); panel_->setMode(CutoutPanel::Mode::Animate); });
    add("게임 연결 편집", "cutoutControllerAction", [this] { showDock(); panel_->showControllerTab(); });
  }
  viewChanged();
}

void CutoutStudioPlugin::deactivate() {
  for (const auto& a : actions_) if (a) delete a.data();
  actions_.clear();
  // The host owns (and deletes, before unloading this DLL) the dock and the workspace page
  // holding the panel; a deleteLater() here could run after unload. Just make the panel inert.
  if (panel_) { panel_->detachHost(); panel_->setEnabled(false); }
  cache_.clear();
  host_ = nullptr;
  workspaceHost_ = nullptr;
}

QList<raising::StudioWorkspace> CutoutStudioPlugin::createWorkspaces(raising::StudioWorkspaceHost* host) {
  if (!panel_ || !dockBody_ || workspaceHost_) return {};
  workspaceHost_ = host;
  // Same panel, new home: the host reparents it into its central workspace page.
  dockBody_->layout()->removeWidget(panel_);
  panel_->setParent(nullptr);
  launcher_ = new QWidget;
  launcher_->setObjectName("cutoutWorkspaceLauncher");
  auto* row = new QHBoxLayout(launcher_);
  row->setContentsMargins(6, 4, 6, 4);
  launcherPreview_ = new QLabel;
  launcherPreview_->setObjectName("cutoutLauncherPreview");
  launcherPreview_->setFixedSize(72, 72);
  launcherPreview_->setAlignment(Qt::AlignCenter);
  launcherPreview_->setStyleSheet("background:#1f2937;border-radius:4px;");
  auto* column = new QVBoxLayout;
  launcherSummary_ = new QLabel;
  launcherSummary_->setObjectName("cutoutLauncherSummary");
  launcherSummary_->setWordWrap(true);
  launcherSummary_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  auto* open = new QPushButton("작업 공간 열기 (Ctrl+Shift+A)");
  open->setObjectName("cutoutOpenWorkspace");
  open->setToolTip("캐릭터 애니메이션 전용 작업 공간으로 바꿉니다. 같은 단축키로 화면 편집으로 돌아옵니다.");
  QObject::connect(open, &QPushButton::clicked, launcher_, [this] { showDock(); });
  column->addWidget(launcherSummary_);
  column->addWidget(open, 0, Qt::AlignLeft);
  column->addStretch();
  row->addWidget(launcherPreview_);
  row->addLayout(column, 1);
  dockBody_->layout()->addWidget(launcher_);
  updateLauncher();
  return {raising::StudioWorkspace{WORKSPACE_ID, "캐릭터 애니메이션", panel_, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A)}};
}

void CutoutStudioPlugin::workspaceChanged(const QString& id, bool shown) {
  if (id != WORKSPACE_ID || !panel_) return;
  // Panel shortcuts (V/R/S/P/K/Space, Tab) are widget-with-children: start with canvas focus.
  if (shown) panel_->canvas()->setFocus(Qt::OtherFocusReason);
  else updateLauncher();
}

void CutoutStudioPlugin::updateLauncher() {
  if (!launcher_ || !panel_) return;
  const QString rig = panel_->rigId();
  launcherSummary_->setText(rig.isEmpty() ? "캐릭터가 없습니다. 작업 공간에서 「새 캐릭터」로 시작하세요."
                                          : QString("캐릭터 %1 · 클립 %2").arg(rig, panel_->clipId().isEmpty() ? "없음" : panel_->clipId()));
  launcherPreview_->setPixmap(QPixmap::fromImage(panel_->thumbnail(launcherPreview_->size())));
}

void CutoutStudioPlugin::viewChanged() {
  if (!host_) return;
  images_.setAssets(host_->assets());
  cache_.clear();
  if (panel_) panel_->refreshFromView();
  if (workspaceHost_ && workspaceHost_->currentWorkspace() != WORKSPACE_ID) updateLauncher();
}

void CutoutStudioPlugin::showDock() {
  if (!panel_) return;
  if (workspaceHost_) { workspaceHost_->showWorkspace(WORKSPACE_ID); return; }
  for (QWidget* w = panel_; w; w = w->parentWidget())
    if (auto* dock = qobject_cast<QDockWidget*>(w)) { dock->show(); dock->raise(); break; }
  panel_->show();
}

bool CutoutStudioPlugin::paintElement(QPainter* painter, const QJsonObject& element, const QRectF& rect) {
  if (!host_ || !painter || rect.width() <= 0 || rect.height() <= 0) return false;
  // Accept the full screen element ({extension:{...}}) or the extension object itself.
  const QJsonObject ext = element.contains("extension") ? element.value("extension").toObject() : element;
  const QString ref = ext.value("ref").toString();
  if (ref.isEmpty()) return false;
  const QJsonObject props = ext.value("props").toObject();
  // props.skin picks this placed instance's outfit (as the runtime does on mount).
  const QString skin = props.value("skin").toString();
  const QString key = ref + "\n" + skin;
  auto it = cache_.find(key);
  if (it == cache_.end()) {
    QJsonObject controller, rig;
    const QJsonArray docs = host_->view().value("documents").toArray();
    for (const auto& v : docs) {
      const QJsonObject env = v.toObject().value("data").toObject();
      if (v.toObject().value("kind") == "extension" && env.value("plugin") == PLUGIN_ID && env.value("type") == "controller" && env.value("id") == ref) controller = env.value("data").toObject();
    }
    if (controller.isEmpty()) return false;
    for (const auto& v : docs) {
      const QJsonObject env = v.toObject().value("data").toObject();
      if (v.toObject().value("kind") == "extension" && env.value("plugin") == PLUGIN_ID && env.value("type") == "rig" && env.value("id") == controller.value("rig")) rig = env.value("data").toObject();
    }
    if (rig.isEmpty()) return false;
    Cached c;
    try {
      c.rig = std::make_shared<const cutout::Rig>(cutout::parseRig(ss(controller.value("rig").toString()), toJson(rig)));
      // First frame of the default clip with the instance look, as the runtime shows right after mount.
      cutout::Look look;
      look.skin = c.rig->skinIndex(ss(controller.value("defaultSkin").toString()));
      if (const int s = c.rig->skinIndex(ss(skin)); s >= 0) look.skin = s;
      look.expression = c.rig->expressionIndex(ss(controller.value("defaultExpression").toString()));
      c.pose = cutout::samplePose(*c.rig, c.rig->clipIndex(ss(controller.value("defaultClip").toString())), 0, look);
    } catch (const std::exception&) { return false; }
    it = cache_.insert(key, c);
  }
  painter->save();
  painter->setClipRect(rect, Qt::IntersectClip);
  painter->setTransform(elementPlacement(*it->rig, rect, props.value("scale").toDouble(1), props.value("flipX").toBool(false)), true);
  drawRig(*painter, *it->rig, it->pose, images_);
  painter->restore();
  return true;
}

void CutoutStudioPlugin::openEditor(const QString& type, const QString& id) {
  if (!panel_) return;
  showDock();
  panel_->refreshFromView();
  if (type == "rig") panel_->selectRig(id);
  else if (type == "controller" || type == "character") { panel_->selectController(id); panel_->showControllerTab(); }
}
}  // namespace cutout_studio
