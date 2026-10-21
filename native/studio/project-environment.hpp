#pragma once
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

// Machine-local dependency paths never enter a shared project or its fingerprint.
inline QString projectPluginStoreKey(const QString &root) {
  const QFileInfo info(root);
  auto path = info.canonicalFilePath();
  if (path.isEmpty()) path = info.absoluteFilePath();
#ifdef Q_OS_WIN
  path = path.toCaseFolded();
#endif
  return "projectPluginStores/" + QCryptographicHash::hash(QDir::cleanPath(path).toUtf8(), QCryptographicHash::Sha256).toHex();
}
inline QString projectPluginStore(const QString &root) {
  return QSettings().value(projectPluginStoreKey(root)).toString();
}
