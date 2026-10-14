// Host side of the Qt authoring plugin ABI (raising-studio-plugin-1).
// The service reply (`plugins`) is the only source of truth for which plugins a
// project uses; this manager only mirrors it by loading/unloading authoring DLLs.
// A broken or missing plugin degrades to placeholders and notices, never a crash.
#pragma once
#include "plugin-api.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QStringList>
#include <memory>

class StudioWindow;

class StudioPluginManager final : public QObject {
  Q_OBJECT
public:
  explicit StudioPluginManager(StudioWindow *window);
  ~StudioPluginManager() override;
  /** Replaces the mirrored `plugins` reply object (does not load anything). */
  void setState(const QJsonObject &plugins) { pluginState = plugins; }
  QJsonObject state() const { return pluginState; }
  /** Loads newly active authoring DLLs and unloads disabled/missing ones or those of another project. */
  void sync(const QString &projectRoot);
  void notifyViewChanged();
  void unloadAll();
  raising::StudioPlugin *plugin(const QString &id) const;
  QJsonObject projectEntry(const QString &id) const;
  QJsonObject installedEntry(const QString &id, const QString &version = {}) const;
  /** Manifest element types of active project plugins: [{plugin, type, label, refType}]. */
  QJsonArray activeElementTypes() const;
  QString refType(const QString &plugin, const QString &type) const;
  /** Rows for the manager dialog (project entries first, then other installed packages). */
  QJsonArray rows() const;
  /** Empty when the owning plugin is loaded and may paint; otherwise the placeholder caption. */
  QString placeholder(const QJsonObject &element) const;
  bool paint(QPainter *painter, const QJsonObject &element, const QRectF &rect);
  void openEditor(const QString &pluginId, const QString &documentType, const QString &documentId);
  QString loadError(const QString &id) const { return failures.value(id).second; }
  QStringList loadedIds() const { return loaded.keys(); }
  /** Loads one DLL as plugin `id` (used by sync; public so tests can probe rejection paths). */
  bool load(const QString &id, const QString &file, QString *error = nullptr);
  void unload(const QString &id);
signals:
  void notice(QString message);

private:
  class Host;
  struct Record;
  friend class Host;
  StudioWindow *window;
  QJsonObject pluginState;
  QString loadedRoot;
  QMap<QString, std::shared_ptr<Record>> loaded;
  QMap<QString, QPair<QString, QString>> failures;  // id -> (dll, message)
  int depth = 0;  // >0 while plugin code is on the stack; unloading is deferred until it returns
  QStringList deferredUnloads;
  template <class F> bool guarded(const QString &id, F &&call);
  void flushDeferred();
};
