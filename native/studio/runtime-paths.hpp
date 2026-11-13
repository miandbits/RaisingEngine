#pragma once
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

inline QString studioResourceRoot() {
#ifdef Q_OS_MACOS
  const auto resources=QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../Resources");
  if(QFileInfo(resources).isDir())return resources;
#endif
  return QCoreApplication::applicationDirPath();
}

inline QString studioEngineRoot() {
  const QDir app(QCoreApplication::applicationDirPath());
  const auto macPackaged=app.absoluteFilePath("../Resources/engine");
  if(QFileInfo::exists(macPackaged+"/apps/qt-studio/service.mjs"))return macPackaged;
  const auto packaged=app.absoluteFilePath("engine");
  if(QFileInfo::exists(packaged+"/apps/qt-studio/service.mjs"))return packaged;
  const auto development=app.absoluteFilePath("../..");
  return QFileInfo::exists(development+"/apps/qt-studio/service.mjs")?development:QDir::currentPath();
}
