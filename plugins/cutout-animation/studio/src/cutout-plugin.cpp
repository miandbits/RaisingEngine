#include "cutout-plugin.hpp"
#include <QAction>
#include <QDockWidget>
#include <QMenu>
#include <QPainter>

namespace cutout_studio {
void CutoutStudioPlugin::activate(raising::StudioHost* host) {
  host_ = host;
  panel_ = new CutoutPanel(host);
  host->addDock("캐릭터 애니메이션", "cutoutAnimationDock", panel_, Qt::BottomDockWidgetArea);
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
  // The host owns (and deletes, before unloading this DLL) the dock holding the panel; a
  // deleteLater() here could run after unload. Just make the panel inert.
  if (panel_) { panel_->detachHost(); panel_->setEnabled(false); }
  cache_.clear();
  host_ = nullptr;
}

void CutoutStudioPlugin::viewChanged() {
  if (!host_) return;
  images_.setAssets(host_->assets());
  cache_.clear();
  if (panel_) panel_->refreshFromView();
}

void CutoutStudioPlugin::showDock() {
  if (!panel_) return;
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
  auto it = cache_.find(ref);
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
      // First frame of the default clip, as the runtime shows right after mount.
      c.pose = cutout::samplePose(*c.rig, c.rig->clipIndex(ss(controller.value("defaultClip").toString())), 0);
    } catch (const std::exception&) { return false; }
    it = cache_.insert(ref, c);
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
