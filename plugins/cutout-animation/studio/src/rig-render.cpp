#include "rig-render.hpp"
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <algorithm>

namespace cutout_studio {
void ImageCache::setAssets(const QJsonArray& assets) {
  QHash<QString, QString> next;
  QStringList order;
  for (const auto& v : assets) {
    const auto a = v.toObject();
    const QString kind = a.value("kind").toString("image");
    if (kind != "image") continue;
    next.insert(a.value("id").toString(), a.value("absolutePath").toString());
    order.push_back(a.value("id").toString());
  }
  // Drop decoded images whose path changed (re-import replaces the file under the same id).
  for (auto it = images_.begin(); it != images_.end();) it = next.value(it.key()) != paths_.value(it.key()) ? images_.erase(it) : std::next(it);
  paths_ = next;
  order_ = order;
}
QImage ImageCache::image(const QString& assetId) {
  auto it = images_.find(assetId);
  if (it != images_.end()) return *it;
  QImage img;
  const QString p = paths_.value(assetId);
  if (!p.isEmpty()) img.load(p);
  if (!img.isNull()) img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  images_.insert(assetId, img);
  return img;
}
QSize ImageCache::pixelSize(const QString& assetId) const {
  const QString p = paths_.value(assetId);
  if (p.isEmpty() || !QFileInfo::exists(p)) return {};
  return QImageReader(p).size();
}
QStringList ImageCache::imageAssetIds() const { return order_; }

cutout::Affine attachmentMatrix(const cutout::Rig& rig, const cutout::Pose& pose, int i, const cutout::Attachment** out) {
  *out = nullptr;
  if (i < 0 || i >= int(rig.slots.size()) || size_t(i) >= pose.attachments.size()) return {};
  const auto& slot = rig.slots[size_t(i)];
  if (!pose.attachments[size_t(i)] || slot.boneIndex < 0 || size_t(slot.boneIndex) >= pose.world.size()) return {};
  const cutout::Attachment* att = rig.attachment(slot.id, *pose.attachments[size_t(i)], pose.skin);  // skin first, then default
  if (!att) return {};
  *out = att;
  const auto& o = att->offset;
  // Same formula as Instance::drawList, without the instance-origin shift (editor works in rig space).
  return pose.world[size_t(slot.boneIndex)] * cutout::Affine::compose(o.x, o.y, o.rotation, o.scaleX, o.scaleY) *
         cutout::Affine::compose(-att->pivotX * att->width, -att->pivotY * att->height, 0, 1, 1);
}

void drawRig(QPainter& p, const cutout::Rig& rig, const cutout::Pose& pose, ImageCache& images, const QSet<QString>& hidden) {
  const QTransform base = p.transform();
  const double baseOpacity = p.opacity();
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
  p.setRenderHint(QPainter::Antialiasing, true);
  for (size_t i = 0; i < rig.slots.size(); i++) {
    if (hidden.contains(qs(rig.slots[i].id))) continue;
    const cutout::Attachment* att = nullptr;
    const cutout::Affine m = attachmentMatrix(rig, pose, int(i), &att);
    const double alpha = i < pose.alpha.size() ? std::clamp(pose.alpha[i], 0.0, 1.0) : 1.0;
    if (!att || alpha <= 0) continue;
    p.setTransform(qtTransform(m) * base);
    p.setOpacity(baseOpacity * alpha);
    const QRectF target(0, 0, att->width, att->height);
    const QImage img = images.image(qs(att->imageAssetId));
    if (!img.isNull()) p.drawImage(target, img);
    else {
      p.fillRect(target, QBrush(QColor(200, 80, 80, 90), Qt::BDiagPattern));
      p.setPen(QPen(QColor(220, 90, 90), 0));
      p.drawRect(target);
    }
  }
  p.setTransform(base);
  p.setOpacity(baseOpacity);
}

QTransform elementPlacement(const cutout::Rig& rig, const QRectF& box, double scale, bool flipX) {
  // Mirrors cutout::Host::placement so the screen editor shows what the player shows.
  const double rw = rig.width > 0 ? rig.width : box.width(), rh = rig.height > 0 ? rig.height : box.height();
  const double s = (rw > 0 && rh > 0 ? std::min(box.width() / rw, box.height() / rh) : 1) * scale;
  const double tx = (box.width() - rw * s) / 2, ty = (box.height() - rh * s) / 2;
  QTransform t;
  t.translate(box.x(), box.y());
  if (flipX) { t.translate(box.width(), 0); t.scale(-1, 1); }
  t.translate(tx, ty);
  t.scale(s, s);
  return t;
}
}  // namespace cutout_studio
