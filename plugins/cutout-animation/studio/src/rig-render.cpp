#include "rig-render.hpp"
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace cutout_studio {
cutout::Json toJsonValue(const QJsonValue& v) {
  switch (v.type()) {
    case QJsonValue::Bool: return v.toBool();
    case QJsonValue::Double: {
      const double d = v.toDouble();
      if (!std::isfinite(d)) return nullptr;
      // Qt keeps parsed integers as integers; an integral double printed without an exponent came
      // back from the old text path as an integer too (keys' timeMs must stay integers).
      if (std::trunc(d) == d && std::fabs(d) < 1e15) return int64_t(d);
      return d;
    }
    case QJsonValue::String: return v.toString().toStdString();
    case QJsonValue::Array: {
      const QJsonArray a = v.toArray();
      cutout::Json out = cutout::Json::array();
      out.get_ref<cutout::Json::array_t&>().reserve(size_t(a.size()));
      for (const auto& x : a) out.push_back(toJsonValue(x));
      return out;
    }
    case QJsonValue::Object: {
      const QJsonObject o = v.toObject();
      cutout::Json out = cutout::Json::object();
      for (auto it = o.begin(); it != o.end(); ++it) out[it.key().toStdString()] = toJsonValue(it.value());
      return out;
    }
    default: return nullptr;
  }
}
QJsonValue toQtValue(const cutout::Json& j) {
  switch (j.type()) {
    case cutout::Json::value_t::boolean: return j.get<bool>();
    case cutout::Json::value_t::number_integer: return qint64(j.get<int64_t>());
    case cutout::Json::value_t::number_unsigned: return j.get<uint64_t>() <= uint64_t(INT64_MAX) ? QJsonValue(qint64(j.get<uint64_t>())) : QJsonValue(double(j.get<uint64_t>()));
    case cutout::Json::value_t::number_float: { const double d = j.get<double>(); return std::isfinite(d) ? QJsonValue(d) : QJsonValue(); }
    case cutout::Json::value_t::string: return QString::fromStdString(j.get_ref<const std::string&>());
    case cutout::Json::value_t::array: {
      QJsonArray out;
      for (const auto& x : j) out.append(toQtValue(x));
      return out;
    }
    case cutout::Json::value_t::object: {
      QJsonObject out;
      for (auto it = j.begin(); it != j.end(); ++it) out.insert(QString::fromStdString(it.key()), toQtValue(it.value()));
      return out;
    }
    default: return QJsonValue();
  }
}

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
QPolygonF clipPolygon(const cutout::Rig& rig, const cutout::Pose& pose, int i) {
  const cutout::Attachment* att = nullptr;
  attachmentMatrix(rig, pose, i, &att);
  QPolygonF out;
  if (!att || !att->clipping) return out;
  const auto pos = cutout::meshPositions(rig, pose, i, *att);
  for (size_t v = 0; v + 1 < pos.size(); v += 2) out << QPointF(pos[v], pos[v + 1]);
  return out;
}
QRectF attachmentBounds(const cutout::Rig& rig, const cutout::Pose& pose, int i) {
  const cutout::Attachment* att = nullptr;
  const cutout::Affine m = attachmentMatrix(rig, pose, i, &att);
  if (!att) return {};
  if (att->clipping) return clipPolygon(rig, pose, i).boundingRect();
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
 * Image px -> rig-space map of triangle (i, j, k): the affine transform taking its uv triangle
 * (scaled to image pixels) onto its drawn triangle. False for a degenerate uv triangle.
 */
static bool triangleMap(const QVector<QPointF>& pts, const cutout::Attachment& att, double iw, double ih, int i, int j, int k, QTransform& out) {
  const QPointF a = pts[i], b = pts[j], c = pts[k];
  const double u0 = att.uvs[2 * size_t(i)] * iw, v0 = att.uvs[2 * size_t(i) + 1] * ih;
  const double du1 = att.uvs[2 * size_t(j)] * iw - u0, dv1 = att.uvs[2 * size_t(j) + 1] * ih - v0;
  const double du2 = att.uvs[2 * size_t(k)] * iw - u0, dv2 = att.uvs[2 * size_t(k) + 1] * ih - v0;
  const double det = du1 * dv2 - du2 * dv1;
  if (std::fabs(det) < 1e-9) return false;  // a degenerate uv triangle maps no image area
  const double ma = ((b.x() - a.x()) * dv2 - (c.x() - a.x()) * dv1) / det, mc = ((c.x() - a.x()) * du1 - (b.x() - a.x()) * du2) / det;
  const double mb = ((b.y() - a.y()) * dv2 - (c.y() - a.y()) * dv1) / det, md = ((c.y() - a.y()) * du1 - (b.y() - a.y()) * du2) / det;
  out = QTransform(ma, mb, mc, md, a.x() - ma * u0 - mc * v0, a.y() - mb * u0 - md * v0);
  return true;
}
/**
 * Outline of the triangles as closed loops (outer outline and holes), each triangle taken
 * counter-clockwise so a hole runs the other way and a winding fill covers exactly the mesh.
 * Empty when the boundary is not a set of simple loops (a vertex touching two boundary runs).
 */
static QPainterPath outlinePath(const QVector<QPointF>& pts, const std::vector<int>& tris) {
  QHash<quint64, int> directed;  // (from << 32 | to) -> count
  auto key = [](int a, int b) { return (quint64(quint32(a)) << 32) | quint32(b); };
  for (size_t t = 0; t + 2 < tris.size(); t += 3) {
    int v[3] = {tris[t], tris[t + 1], tris[t + 2]};
    const QPointF a = pts[v[0]], b = pts[v[1]], c = pts[v[2]];
    if ((b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x()) < 0) std::swap(v[1], v[2]);
    for (int e = 0; e < 3; e++) directed[key(v[e], v[(e + 1) % 3])]++;
  }
  QHash<int, int> next;
  for (auto it = directed.begin(); it != directed.end(); ++it) {
    const int a = int(it.key() >> 32), b = int(it.key() & 0xffffffffu);
    if (directed.contains(key(b, a))) continue;  // shared by two triangles: inside
    if (it.value() != 1 || next.contains(a)) return {};
    next.insert(a, b);
  }
  QPainterPath path;
  path.setFillRule(Qt::WindingFill);
  while (!next.isEmpty()) {
    int at = next.begin().key();
    QPolygonF loop;
    while (next.contains(at)) { loop << pts[at]; at = next.take(at); }
    if (loop.size() >= 3) path.addPolygon(loop), path.closeSubpath();
  }
  return path;
}
/**
 * One mesh with QPainter, two paths (the old clip-path-per-triangle renderer cost ~20 us per
 * triangle, 40+ ms for a 2000-triangle part):
 *  - rigid: every vertex is one affine image of its uv (an undeformed mesh, or one whose bones
 *    move it as a whole): the image is filled once through the mesh outline, exactly like a
 *    region clipped to its triangles, without seams;
 *  - deformed: each triangle is a textured-brush fill (brush transform = its uv -> drawn map);
 *    see below for the antialiasing split.
 * Same geometry as the players either way.
 */
static void drawMesh(QPainter& p, const QTransform& base, const QVector<QPointF>& pts, const cutout::Attachment& att, const QImage& img) {
  const double scale = std::sqrt(std::fabs(base.determinant()));
  const double grow = scale > 1e-9 ? 0.5 / scale : 0.5;
  const double iw = img.isNull() ? att.width : img.width(), ih = img.isNull() ? att.height : img.height();
  const int n = int(pts.size());
  for (int v : att.triangles) if (v < 0 || v >= n) return;  // validated data never gets here
  if (att.uvs.size() < size_t(2 * n)) return;
  // Callers (canvas overlays) keep drawing with their pen and brush after the parts.
  struct Keep { QPainter& p; QPen pen; QBrush brush; ~Keep() { p.setPen(pen); p.setBrush(brush); } } keep{p, p.pen(), p.brush()};
  p.setTransform(base);
  if (img.isNull()) {
    for (size_t t = 0; t + 2 < att.triangles.size(); t += 3) {
      const QPolygonF tri({pts[att.triangles[t]], pts[att.triangles[t + 1]], pts[att.triangles[t + 2]]});
      p.setPen(Qt::NoPen);
      p.setBrush(QBrush(QColor(200, 80, 80, 90), Qt::BDiagPattern));
      p.drawPolygon(tri);
      p.setPen(QPen(QColor(220, 90, 90), 0));
      p.setBrush(Qt::NoBrush);
      p.drawPolygon(tri);
    }
    return;
  }
  // Rigid test: the map of the first usable triangle must place every vertex (within 1/4 device px).
  QTransform rigid;
  bool isRigid = false;
  for (size_t t = 0; t + 2 < att.triangles.size() && !isRigid; t += 3)
    isRigid = triangleMap(pts, att, iw, ih, att.triangles[t], att.triangles[t + 1], att.triangles[t + 2], rigid);
  const double tolerance = 0.25 / std::max(scale, 1e-9);
  for (int v = 0; v < n && isRigid; v++) {
    const QPointF q = rigid.map(QPointF(att.uvs[2 * size_t(v)] * iw, att.uvs[2 * size_t(v) + 1] * ih)) - pts[v];
    isRigid = std::fabs(q.x()) <= tolerance && std::fabs(q.y()) <= tolerance;
  }
  p.setPen(Qt::NoPen);
  if (isRigid) {
    const QPainterPath outline = outlinePath(pts, att.triangles);
    if (!outline.isEmpty()) {
      QBrush brush(img);
      brush.setTransform(rigid);
      p.fillPath(outline, brush);
      return;
    }
  }
  // Interior triangles tile exactly without antialiasing (the raster fill rule gives every pixel
  // to one triangle: no seams, no double blending) and that is ~6x cheaper; only triangles on the
  // outline are antialiased (grown so their inner edges still meet the aliased neighbours).
  QHash<quint64, int> uses;
  auto edge = [](int a, int b) { return (quint64(quint32(std::min(a, b))) << 32) | quint32(std::max(a, b)); };
  for (size_t t = 0; t + 2 < att.triangles.size(); t += 3)
    for (int e = 0; e < 3; e++) uses[edge(att.triangles[t + size_t(e)], att.triangles[t + size_t((e + 1) % 3)])]++;
  const bool antialias = p.testRenderHint(QPainter::Antialiasing);
  QBrush brush(img);
  for (int pass = 0; pass < 2; pass++) {
    p.setRenderHint(QPainter::Antialiasing, pass == 1 && antialias);
    for (size_t t = 0; t + 2 < att.triangles.size(); t += 3) {
      const int i = att.triangles[t], j = att.triangles[t + 1], k = att.triangles[t + 2];
      const bool outer = uses.value(edge(i, j)) == 1 || uses.value(edge(j, k)) == 1 || uses.value(edge(k, i)) == 1;
      if (outer != (pass == 1)) continue;
      QTransform m;
      if (!triangleMap(pts, att, iw, ih, i, j, k, m)) continue;
      const QPointF a = pts[i], b = pts[j], c = pts[k], center = (a + b + c) / 3;
      auto out = [&](QPointF q) { const QPointF d = q - center; const double l = std::sqrt(d.x() * d.x() + d.y() * d.y()); return l > 1e-12 && outer ? q + d / l * grow : q; };
      const QPointF tri[3] = {out(a), out(b), out(c)};
      brush.setTransform(m);
      p.setBrush(brush);
      p.drawConvexPolygon(tri, 3);
    }
  }
  p.setRenderHint(QPainter::Antialiasing, antialias);
}

void drawRig(QPainter& p, const cutout::Rig& rig, const cutout::Pose& pose, ImageCache& images, const QSet<QString>& hidden) {
  const QTransform base = p.transform();
  const double baseOpacity = p.opacity();
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
  p.setRenderHint(QPainter::Antialiasing, true);
  int clipEnd = -1;  // >= 0 while a clip range is open (the painter state was saved for it)
  for (size_t i = 0; i < rig.slots.size(); i++) {
    const bool shown = !hidden.contains(qs(rig.slots[i].id));
    const cutout::Attachment* att = nullptr;
    const cutout::Affine m = attachmentMatrix(rig, pose, int(i), &att);
    const double alpha = i < pose.alpha.size() ? std::clamp(pose.alpha[i], 0.0, 1.0) : 1.0;
    if (shown && att && att->clipping) {
      // Same rules as cutout::drawItems: alpha does not matter, no nesting, end slot inclusive.
      if (clipEnd < 0) {
        QPainterPath path;
        path.addPolygon(clipPolygon(rig, pose, int(i)));
        path.closeSubpath();
        p.save();
        p.setTransform(base);
        p.setClipPath(path, Qt::IntersectClip);
        clipEnd = att->clipEndIndex >= 0 ? att->clipEndIndex : int(rig.slots.size()) - 1;
      }
    } else if (shown && att && alpha > 0) {
      p.setOpacity(baseOpacity * alpha);
      // Sequences (plugin 1.12.0): the frame the pose shows (clip time picks it while scrubbing).
      const QImage img = images.image(qs(cutout::attachmentImage(pose, int(i), *att)));
      if (att->mesh) drawMesh(p, base, meshPoints(rig, pose, int(i)), *att, img);
      else {
        p.setTransform(qtTransform(m) * base);
        const QRectF target(0, 0, att->width, att->height);
        if (!img.isNull()) p.drawImage(target, img);
        else {
          p.fillRect(target, QBrush(QColor(200, 80, 80, 90), Qt::BDiagPattern));
          p.setPen(QPen(QColor(220, 90, 90), 0));
          p.drawRect(target);
        }
      }
    }
    if (clipEnd >= 0 && int(i) >= clipEnd) { p.restore(); clipEnd = -1; }
  }
  if (clipEnd >= 0) p.restore();
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
