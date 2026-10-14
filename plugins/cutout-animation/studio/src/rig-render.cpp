#include "rig-render.hpp"
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

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

QVector<QPointF> meshPoints(const cutout::Rig& rig, const cutout::Pose& pose, int i, const cutout::Attachment** out) {
  const cutout::Attachment* att = nullptr;
  attachmentMatrix(rig, pose, i, &att);
  if (out) *out = att;
  QVector<QPointF> points;
  if (!att || !att->mesh) return points;
  const auto pos = cutout::meshPositions(rig, pose, i, *att);
  for (size_t v = 0; v + 1 < pos.size(); v += 2) points << QPointF(pos[v], pos[v + 1]);
  return points;
}
QRectF attachmentBounds(const cutout::Rig& rig, const cutout::Pose& pose, int i) {
  const cutout::Attachment* att = nullptr;
  const cutout::Affine m = attachmentMatrix(rig, pose, i, &att);
  if (!att) return {};
  if (!att->mesh) return qtTransform(m).mapRect(QRectF(0, 0, att->width, att->height));
  return QPolygonF(meshPoints(rig, pose, i)).boundingRect();
}
QVector<QPair<int, int>> meshOutline(const std::vector<int>& tris) {
  QHash<QPair<int, int>, int> uses;
  for (size_t t = 0; t + 2 < tris.size(); t += 3)
    for (int k = 0; k < 3; k++) {
      const int a = tris[t + size_t(k)], b = tris[t + size_t((k + 1) % 3)];
      uses[{std::min(a, b), std::max(a, b)}]++;
    }
  QVector<QPair<int, int>> out;
  for (auto it = uses.begin(); it != uses.end(); ++it) if (it.value() == 1) out << it.key();
  return out;
}
double vertexWeight(const cutout::Rig& rig, int slotIndex, const cutout::Attachment& att, size_t v, int bone) {
  if (att.weights.empty() || v >= att.weights.size()) return slotIndex >= 0 && size_t(slotIndex) < rig.slots.size() && rig.slots[size_t(slotIndex)].boneIndex == bone ? 1 : 0;
  double w = 0;
  for (const auto& x : att.weights[v]) if (att.binds[size_t(x.bind)].first == bone) w += x.weight;
  return w;
}

/**
 * One mesh with QPainter: every triangle is drawn as the affine image transform mapping its uv
 * triangle onto its rig-space triangle, clipped to the triangle grown by half a device pixel
 * (an aliased clip would leave hairline seams between neighbours). Same geometry as the players.
 */
static void drawMesh(QPainter& p, const QTransform& base, const QVector<QPointF>& pts, const cutout::Attachment& att, const QImage& img) {
  const double scale = std::sqrt(std::fabs(base.determinant()));
  const double grow = scale > 1e-9 ? 0.5 / scale : 0.5;
  const double iw = img.isNull() ? att.width : img.width(), ih = img.isNull() ? att.height : img.height();
  for (size_t t = 0; t + 2 < att.triangles.size(); t += 3) {
    const int i = att.triangles[t], j = att.triangles[t + 1], k = att.triangles[t + 2];
    if (i >= pts.size() || j >= pts.size() || k >= pts.size()) continue;
    const QPointF a = pts[i], b = pts[j], c = pts[k], center = (a + b + c) / 3;
    auto out = [&](QPointF q) { const QPointF d = q - center; const double l = std::hypot(d.x(), d.y()); return l > 1e-12 ? q + d / l * grow : q; };
    QPainterPath clip;
    clip.addPolygon(QPolygonF({out(a), out(b), out(c)}));
    clip.closeSubpath();
    p.setTransform(base);
    if (img.isNull()) {
      p.fillPath(clip, QBrush(QColor(200, 80, 80, 90), Qt::BDiagPattern));
      p.setPen(QPen(QColor(220, 90, 90), 0));
      p.drawPolygon(QPolygonF({a, b, c}));
      continue;
    }
    const double u0 = att.uvs[2 * size_t(i)] * iw, v0 = att.uvs[2 * size_t(i) + 1] * ih;
    const double du1 = att.uvs[2 * size_t(j)] * iw - u0, dv1 = att.uvs[2 * size_t(j) + 1] * ih - v0;
    const double du2 = att.uvs[2 * size_t(k)] * iw - u0, dv2 = att.uvs[2 * size_t(k) + 1] * ih - v0;
    const double det = du1 * dv2 - du2 * dv1;
    if (std::fabs(det) < 1e-9) continue;  // a degenerate uv triangle maps no image area
    const double ma = ((b.x() - a.x()) * dv2 - (c.x() - a.x()) * dv1) / det, mc = ((c.x() - a.x()) * du1 - (b.x() - a.x()) * du2) / det;
    const double mb = ((b.y() - a.y()) * dv2 - (c.y() - a.y()) * dv1) / det, md = ((c.y() - a.y()) * du1 - (b.y() - a.y()) * du2) / det;
    p.save();
    p.setClipPath(clip, Qt::IntersectClip);
    p.setTransform(QTransform(ma, mb, mc, md, a.x() - ma * u0 - mc * v0, a.y() - mb * u0 - md * v0) * base);
    // Only the uv triangle's bounding box of the source is sampled.
    const QRectF source = QRectF(QPointF(std::min({u0, u0 + du1, u0 + du2}), std::min({v0, v0 + dv1, v0 + dv2})),
                                 QPointF(std::max({u0, u0 + du1, u0 + du2}), std::max({v0, v0 + dv1, v0 + dv2}))).adjusted(-1, -1, 1, 1) & QRectF(0, 0, iw, ih);
    p.drawImage(source.topLeft(), img, source);
    p.restore();
  }
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
    p.setOpacity(baseOpacity * alpha);
    if (att->mesh) {
      drawMesh(p, base, meshPoints(rig, pose, int(i)), *att, images.image(qs(att->imageAssetId)));
      continue;
    }
    p.setTransform(qtTransform(m) * base);
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
