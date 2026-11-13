#pragma once
#include <QImage>
#include <QJsonObject>
#include <QPainter>
#include <QPixmap>

// Paint decoration only: callers keep their existing layout, border and hit area.
inline void paintSurface(QPainter* painter, const QRectF& box, const QImage& image, const QJsonObject& surface) {
  if(image.isNull() || box.isEmpty()) return;
  const auto mode=surface.value("mode").toString();
  if(mode=="tile") {
    painter->save(); painter->setClipRect(box,Qt::IntersectClip);
    auto pixmap=QPixmap::fromImage(image); pixmap.setDevicePixelRatio(1);
    painter->drawTiledPixmap(box,pixmap); painter->restore(); return;
  }
  if(mode!="nine-slice") return;
  const auto insets=surface.value("insets").toObject();
  const double left=insets.value("left").toDouble(),right=insets.value("right").toDouble();
  const double top=insets.value("top").toDouble(),bottom=insets.value("bottom").toDouble();
  if(left<0||right<0||top<0||bottom<0||left+right>=image.width()||top+bottom>=image.height()) return;
  const auto scaleX=left+right>box.width()?box.width()/(left+right):1.;
  const auto scaleY=top+bottom>box.height()?box.height()/(top+bottom):1.;
  const double sx[]{0,left,double(image.width())-right,double(image.width())};
  const double sy[]{0,top,double(image.height())-bottom,double(image.height())};
  const double dx[]{box.left(),box.left()+left*scaleX,box.right()-right*scaleX,box.right()};
  const double dy[]{box.top(),box.top()+top*scaleY,box.bottom()-bottom*scaleY,box.bottom()};
  painter->save(); painter->setClipRect(box,Qt::IntersectClip);
  for(int y=0;y<3;++y)for(int x=0;x<3;++x){
    const QRectF source(sx[x],sy[y],sx[x+1]-sx[x],sy[y+1]-sy[y]);
    const QRectF target(dx[x],dy[y],dx[x+1]-dx[x],dy[y+1]-dy[y]);
    if(!source.isEmpty()&&!target.isEmpty())painter->drawImage(target,image,source);
  }
  painter->restore();
}
