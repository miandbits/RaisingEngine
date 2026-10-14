#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QPixmap>
#include <QPainter>

QString timelineLayerFrame(const QJsonObject &layer, qint64 elapsedMs);
void drawTimelineStage(QPainter &p, const QRectF &area, const QJsonObject &stage,
                       const QMap<QString,QPixmap> &images, qint64 elapsedMs);
bool editTimelineStage(QWidget *parent, QJsonObject &stage, const QJsonArray &assets,
                       int durationMs);
