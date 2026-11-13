#pragma once
#include <QFileInfo>
#include <QImage>
#include <QImageReader>

// Match the shared surface asset limits before allocating decoded pixels.
// Project validation remains authoritative for PNG structure and project containment.
inline bool readableSurfaceImage(const QString& path) {
  const QFileInfo file(path);
  if(path.isEmpty()||file.suffix().toLower()!="png"||!file.isFile()||file.size()>20LL*1024*1024)return false;
  QImageReader reader(path);if(reader.format().toLower()!="png")return false;
  const auto size=reader.size();
  return size.width()>0&&size.height()>0&&size.width()<=8192&&size.height()<=8192&&qint64(size.width())*size.height()<=32LL*1024*1024;
}
inline QImage loadSurfaceImage(const QString& path) {
  if(!readableSurfaceImage(path))return {};
  QImageReader reader(path);return reader.read();
}
