#pragma once
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

inline QString studioEngineRoot() {
  const QDir app(QCoreApplication::applicationDirPath());
  const auto packaged=app.absoluteFilePath("engine");
  if(QFileInfo::exists(packaged+"/apps/qt-studio/service.mjs"))return packaged;
  const auto development=app.absoluteFilePath("../..");
  return QFileInfo::exists(development+"/apps/qt-studio/service.mjs")?development:QDir::currentPath();
}
