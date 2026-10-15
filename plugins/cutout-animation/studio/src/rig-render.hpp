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
/**
 * Qt JSON <-> core JSON, value by value (plugin 1.9.0; the text round trip through dump/parse cost
 * ~15 ms per conversion of a large rig, twice per edit). Numbers follow what the text round trip
 * produced: integral values (and Qt integers) become core integers, others doubles; non-finite
 * doubles become null.
 */
cutout::Json toJsonValue(const QJsonValue& v);
QJsonValue toQtValue(const cutout::Json& j);
inline cutout::Json toJson(const QJsonObject& o) { return toJsonValue(o); }
inline cutout::Json toJson(const QJsonArray& a) { return toJsonValue(a); }
inline QJsonObject toQtObject(const cutout::Json& j) { return j.is_object() ? toQtValue(j).toObject() : QJsonObject(); }
inline QJsonArray toQtArray(const cutout::Json& j) { return j.is_array() ? toQtValue(j).toArray() : QJsonArray(); }
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

/** Image pixel space -> rig space for slot `slotIndex` in `pose`; nullptr attachment when hidden/empty.
 *  For a mesh this is its undeformed rectangle: draw/hit-test meshes with meshPoints. */
cutout::Affine attachmentMatrix(const cutout::Rig& rig, const cutout::Pose& pose, int slotIndex, const cutout::Attachment** attachment);
/** Rig-space vertices of the mesh shown on `slotIndex` (cutout::meshPositions: skinned after IK,
 *  deform applied); empty when the slot shows a region or nothing. */
QVector<QPointF> meshPoints(const cutout::Rig& rig, const cutout::Pose& pose, int slotIndex, const cutout::Attachment** attachment = nullptr);
/** Rig-space bounds of what the slot draws (mesh vertices or the region rectangle); empty when hidden. */
QRectF attachmentBounds(const cutout::Rig& rig, const cutout::Pose& pose, int slotIndex);
/** Outline of a mesh: edges used by exactly one triangle (pairs of vertex indices). */
QVector<QPair<int, int>> meshOutline(const std::vector<int>& triangles);
/** Weight of `bone` on vertex `vertex` (unweighted meshes: 1 on the slot bone). */
double vertexWeight(const cutout::Rig& rig, int slotIndex, const cutout::Attachment& attachment, size_t vertex, int bone);
/** Draw back->front on the painter's current transform (rig space). Missing images draw a hatched box. */
void drawRig(QPainter& painter, const cutout::Rig& rig, const cutout::Pose& pose, ImageCache& images, const QSet<QString>& hiddenSlots = {});
/** Runtime element placement: contain-fit the authoring canvas into `box`, centered, props scale/flipX. Rig space -> box. */
QTransform elementPlacement(const cutout::Rig& rig, const QRectF& box, double scale, bool flipX);
}  // namespace cutout_studio
