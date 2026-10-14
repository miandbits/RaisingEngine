// Qt drawing helpers shared by the authoring canvas and paintElement.
// Geometry comes only from the core (Pose/world matrices) so the editor shows exactly
// what the runtime draws.
#pragma once
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QTransform>
#include "cutout/runtime.hpp"

class QPainter;

namespace cutout_studio {
inline cutout::Json toJson(const QJsonObject& o) { return cutout::Json::parse(QJsonDocument(o).toJson(QJsonDocument::Compact).toStdString()); }
inline cutout::Json toJson(const QJsonArray& a) { return cutout::Json::parse(QJsonDocument(a).toJson(QJsonDocument::Compact).toStdString()); }
inline QJsonObject toQtObject(const cutout::Json& j) { return QJsonDocument::fromJson(QByteArray::fromStdString(j.dump())).object(); }
inline QJsonArray toQtArray(const cutout::Json& j) { return QJsonDocument::fromJson(QByteArray::fromStdString(j.dump())).array(); }
inline QString qs(const std::string& s) { return QString::fromStdString(s); }
inline std::string ss(const QString& s) { return s.toStdString(); }
/** Affine maps (x,y) -> (a x + c y + e, b x + d y + f); QTransform(m11,m12,m21,m22,dx,dy) uses the same layout. */
inline QTransform qtTransform(const cutout::Affine& m) { return QTransform(m.a, m.b, m.c, m.d, m.e, m.f); }

/** Asset id -> decoded image, loaded lazily from StudioHost::assets() absolutePath. */
class ImageCache {
 public:
  void setAssets(const QJsonArray& assets);
  QString path(const QString& assetId) const { return paths_.value(assetId); }
  QImage image(const QString& assetId);
  /** Pixel size read from the file header (no full decode). Invalid size when unreadable. */
  QSize pixelSize(const QString& assetId) const;
  QStringList imageAssetIds() const;

 private:
  QHash<QString, QString> paths_;
  QHash<QString, QImage> images_;
  QStringList order_;
};

/** Image pixel space -> rig space for slot `slotIndex` in `pose`; nullptr attachment when hidden/empty. */
cutout::Affine attachmentMatrix(const cutout::Rig& rig, const cutout::Pose& pose, int slotIndex, const cutout::Attachment** attachment);
/** Draw back->front on the painter's current transform (rig space). Missing images draw a hatched box. */
void drawRig(QPainter& painter, const cutout::Rig& rig, const cutout::Pose& pose, ImageCache& images, const QSet<QString>& hiddenSlots = {});
/** Runtime element placement: contain-fit the authoring canvas into `box`, centered, props scale/flipX. Rig space -> box. */
QTransform elementPlacement(const cutout::Rig& rig, const QRectF& box, double scale, bool flipX);
}  // namespace cutout_studio
