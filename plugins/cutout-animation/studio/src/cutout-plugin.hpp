// QPluginLoader entry point of the cutout animation authoring module (cutout-studio.dll).
#pragma once
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <memory>
#include "cutout-panel.hpp"
#include "plugin-api.hpp"

class QAction;

namespace cutout_studio {
class CutoutStudioPlugin : public QObject, public raising::StudioPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID RaisingStudioPlugin_iid FILE "../plugin.json")
  Q_INTERFACES(raising::StudioPlugin)
 public:
  QString pluginId() const override { return PLUGIN_ID; }
  void activate(raising::StudioHost* host) override;
  void deactivate() override;
  void viewChanged() override;
  bool paintElement(QPainter* painter, const QJsonObject& element, const QRectF& rect) override;
  void openEditor(const QString& documentType, const QString& documentId) override;
  CutoutPanel* panel() const { return panel_; }

 private:
  struct Cached { QString key; std::shared_ptr<const cutout::Rig> rig; cutout::Pose pose; };
  raising::StudioHost* host_ = nullptr;
  QPointer<CutoutPanel> panel_;
  QList<QPointer<QAction>> actions_;
  ImageCache images_;
  QHash<QString, Cached> cache_;  // controller id -> parsed rig + first default frame
  void showDock();
};
}  // namespace cutout_studio
