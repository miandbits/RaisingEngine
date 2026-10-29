#pragma once
#include <QJsonObject>
#include <QSize>
#include <QTransform>
#include <cmath>

// Runtime nodes use logical viewport coordinates; presentation uses raster pixels.
// Keep picking and selection overlays on the same transform as the rendered image.
inline QTransform rendererRasterTransform(const QJsonObject& ui,QSize pixels) {
  const auto viewport=ui.value("viewport").toObject();
  const double w=viewport.value("width").toDouble(pixels.width()),h=viewport.value("height").toDouble(pixels.height());
  if(w<=0||h<=0||pixels.isEmpty()||!std::isfinite(w)||!std::isfinite(h))return {};
  const auto presentation=ui.value("presentation").toObject();
  // SDL_RenderReadPixels can return the logical presentation target, excluding
  // window letterboxing. Its PNG then differs from GetRenderOutputSize. Do not
  // add the window's offset to an already cropped capture.
  const QSize output(presentation.value("pixelWidth").toInt(),presentation.value("pixelHeight").toInt());
  const QSize presented(qRound(presentation.value("width").toDouble()),qRound(presentation.value("height").toDouble()));
  if(!output.isEmpty()&&pixels!=output&&pixels==presented)
    return QTransform::fromScale(pixels.width()/w,pixels.height()/h);
  const double fit=presentation.isEmpty()?qMin(pixels.width()/w,pixels.height()/h):presentation.value("width").toDouble()/w;
  if(fit<=0||!std::isfinite(fit))return {};
  const double x=presentation.isEmpty()?(pixels.width()-w*fit)/2:presentation.value("x").toDouble();
  const double y=presentation.isEmpty()?(pixels.height()-h*fit)/2:presentation.value("y").toDouble();
  return QTransform(fit,0,0,fit,x,y);
}
inline QRectF rendererNodeRectangle(const QJsonObject& node) {
  return {node.value("x").toDouble(),node.value("y").toDouble(),node.value("width").toDouble(),node.value("height").toDouble()};
}
