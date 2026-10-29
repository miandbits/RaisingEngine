#pragma once
#include <QDir>
#include <QFileInfo>
#include <QString>

namespace StudioPaths {
// Compare real files across Qt and host-service path spellings. Native separators
// are platform-specific: a backslash remains a literal filename character on Unix.
inline QString existingFileIdentity(const QString& path) {
  const auto normalized=QDir::fromNativeSeparators(path);
  if(normalized.isEmpty()||!QDir::isAbsolutePath(normalized))return {};
  const QFileInfo file(normalized);
  if(!file.isFile())return {};
  return file.canonicalFilePath();
}
inline bool sameExistingFile(const QString& expected,const QString& actual) {
  const auto identity=existingFileIdentity(expected);
  return !identity.isEmpty()&&identity==existingFileIdentity(actual);
}
}
