// QPluginLoader entry point of the cutout animation authoring module (cutout-studio.dll).
// Also implements the optional StudioWorkspacePlugin interface: on a host that supports it
// the panel becomes the full-window "캐릭터 애니메이션" workspace and the dock turns into a
// small launcher (mini preview + 작업 공간 열기); older hosts keep the panel in the dock.
#pragma once
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <memory>
#include "cutout-panel.hpp"
#include "plugin-api.hpp"

class QAction;
class QLabel;
class QMenu;

namespace cutout_studio {
constexpr const char* WORKSPACE_ID = "character";

class CutoutStudioPlugin : public QObject, public raising::StudioPlugin, public raising::StudioWorkspacePlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID RaisingStudioPlugin_iid FILE "../plugin.json")
  Q_INTERFACES(raising::StudioPlugin raising::StudioWorkspacePlugin)
 public:
  QString pluginId() const override { return PLUGIN_ID; }
  void activate(raising::StudioHost* host) override;
  void deactivate() override;
  void viewChanged() override;
  bool paintElement(QPainter* painter, const QJsonObject& element, const QRectF& rect) override;
  void openEditor(const QString& documentType, const QString& documentId) override;
  QList<raising::StudioWorkspace> createWorkspaces(raising::StudioWorkspaceHost* host) override;
  void workspaceChanged(const QString& id, bool shown) override;
  CutoutPanel* panel() const { return panel_; }
  /** Dock content: holds the panel (host without workspaces) or the launcher. */
  QWidget* dockBody() const { return dockBody_; }

 private:
  struct Cached { QString key; std::shared_ptr<const cutout::Rig> rig; cutout::Pose pose; };
  raising::StudioHost* host_ = nullptr;
  raising::StudioWorkspaceHost* workspaceHost_ = nullptr;
  QPointer<CutoutPanel> panel_;
  QPointer<QWidget> dockBody_, launcher_;
  QPointer<QLabel> launcherPreview_, launcherSummary_;
  QList<QPointer<QAction>> actions_;
  QPointer<QMenu> sizeMenu_;  // 작업 공간 글자 크기 submenu of the plugin menu
  ImageCache images_;
  QHash<QString, Cached> cache_;  // controller id + "\n" + props.skin -> parsed rig + first default frame
  void showDock();
  void updateLauncher();
};
}  // namespace cutout_studio
