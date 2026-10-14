// Qt authoring plugin ABI (raising-studio-plugin-1).
// Plugins are QPluginLoader DLLs built with the same Qt/MinGW as the studio.
// Every document change must go through StudioHost::request("edit", ...):
// the shared service (also used by MCP) owns validation, undo/redo and saving.
//
// Lifecycle (host side, native/studio/plugin-host.cpp):
// * Loaded when the service reports the project entry `state:"active"` with an
//   `authoring` DLL; activate() is called once per load.
// * deactivate() is called when the project closes/changes, the plugin becomes
//   disabled/missing, or the editor closes. Right after it returns the host deletes
//   every dock and the plugin menu created through this host (keep QPointer, do
//   not delete them yourself afterwards), drops pending request callbacks and then
//   unloads the DLL. Objects not owned that way must be deleted in deactivate(),
//   including connections from host objects to plugin lambdas.
// * Exceptions thrown from any call are caught; the element falls back to the host
//   placeholder and the editor keeps running.
#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QRectF>
#include <QString>
#include <QtPlugin>
#include <functional>

class QMenu;
class QPainter;
class QWidget;

namespace raising {
constexpr int STUDIO_PLUGIN_API = 1;

class StudioHost {
 public:
  virtual ~StudioHost() = default;
  /** Latest service view: {revision, documents[], diagnostics[], dirty, undo, redo, ...}. */
  virtual QJsonObject view() const = 0;
  /** Project assets with absolutePath (images, fonts). */
  virtual QJsonArray assets() const = 0;
  /** Send one request to the shared service. `revision` is injected by the host. */
  virtual void request(const QString& method, const QJsonObject& params, std::function<void(QJsonObject)> done) = 0;
  virtual bool busy() const = 0;
  virtual QWidget* window() const = 0;
  virtual QString engineRoot() const = 0;
  virtual QString projectRoot() const = 0;
  /** Directory of this plugin's installed files (schemas, templates). */
  virtual QString pluginDirectory(const QString& pluginId) const = 0;
  /** Adds a dock to the main window and a toggle to the 보기 menu. */
  virtual void addDock(const QString& title, const QString& objectName, QWidget* widget, Qt::DockWidgetArea area) = 0;
  /** Per-plugin submenu under 플러그인. */
  virtual QMenu* pluginMenu(const QString& pluginId) = 0;
  /** Selected screen element (empty object when none). */
  virtual QJsonObject selectedElement() const = 0;
};

class StudioPlugin {
 public:
  virtual ~StudioPlugin() = default;
  virtual QString pluginId() const = 0;
  virtual int apiVersion() const { return STUDIO_PLUGIN_API; }
  virtual void activate(StudioHost* host) = 0;
  virtual void deactivate() = 0;
  /** Called after every service reply (view/documents may have changed). */
  virtual void viewChanged() = 0;
  /** Paint a placed element of this plugin on the screen canvas. Return false to use the host placeholder. */
  virtual bool paintElement(QPainter* painter, const QJsonObject& element, const QRectF& rect) = 0;
  /** Open the authoring UI focused on a document or element ref (e.g. controller id). */
  virtual void openEditor(const QString& documentType, const QString& documentId) = 0;
};
}  // namespace raising

#define RaisingStudioPlugin_iid "com.raising.StudioPlugin/1"
Q_DECLARE_INTERFACE(raising::StudioPlugin, RaisingStudioPlugin_iid)
