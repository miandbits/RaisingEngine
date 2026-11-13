#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <QFontMetricsF>
#include <QImageReader>
#include <QObject>
#include <functional>

// Representative authoring preview only. The renderer inspector measures actual play.
inline void paintActivityBoardPreview(QPainter* p,const QRectF& box,const QJsonObject& data,std::function<void()> decoration={}) {
  p->save();p->setClipRect(box,Qt::IntersectClip);
  p->fillRect(box,QColor(data.value("fill").toString("#eee5d1")));if(decoration)decoration();
  const auto palette=data.value("_boardControlStyle").toObject();
  const QColor paper(palette.value("fill").toString("#fffaf0")),ink(palette.value("color").toString("#253e37")),edge(palette.value("borderColor").toString("#a79673"));
  QFont font(data.value("_fontFamily").toString());font.setPixelSize(16);p->setFont(font);
  const auto assets=data.value("_boardAssets").toObject();
  auto artwork=[&](const QString& id,QRectF target,bool stretch=false){if(id.isEmpty()||!assets.contains(id))return;QImageReader reader(assets[id].toString());const auto size=reader.size();if(!size.isValid())return;reader.setScaledSize(size.scaled(target.size().toSize(),Qt::KeepAspectRatio));const auto image=reader.read();if(image.isNull())return;if(!stretch){auto fit=QSizeF(image.size());fit.scale(target.size(),Qt::KeepAspectRatio);target=QRectF(target.center()-QPointF(fit.width()/2,fit.height()/2),fit);}p->drawImage(target,image);};
  const qreal margin=16,inner=qMax(1.0,box.width()-2*margin);p->setPen(ink);
  p->drawText(QRectF(margin,8,inner,24),Qt::AlignLeft|Qt::AlignVCenter,QObject::tr("활동 보드 · 대표 미리보기"));
  const int periods=qBound(1,data.value("_calendarProject").toObject().value("periodsPerMonth").toInt(3),12);
  const qreal slotWidth=qMax(48.0,(inner-8*(periods-1))/periods);
  for(int i=0;i<periods;++i){QRectF rect(margin+i*(slotWidth+8),38,slotWidth,48);p->setPen(edge);p->setBrush(paper);p->drawRoundedRect(rect,5,5);p->setPen(ink);p->drawText(rect.adjusted(6,4,-6,-4),Qt::AlignCenter,QFontMetricsF(font).elidedText(QObject::tr("일정 %1").arg(i+1),Qt::ElideRight,rect.width()-12));}
  QJsonArray groups{QJsonObject{{"label",QObject::tr("모든 활동")}}};for(const auto& group:data.value("scheduleGroups").toArray())groups.append(group);
  qreal x=margin;
  for(int i=0;i<groups.size()&&i<4;++i){const auto group=groups[i].toObject();const qreal width=qMin(180.0,qMax(90.0,inner/4-8));QRectF rect(x,98,width,34);p->setPen(edge);p->setBrush(paper);p->drawRoundedRect(rect,5,5);const auto id=group.value("iconAssetId").toString();artwork(id,QRectF(x+5,103,24,24));p->setPen(ink);p->drawText(rect.adjusted(id.isEmpty()?8:34,0,-8,0),Qt::AlignVCenter,QFontMetricsF(font).elidedText(group.value("label").toString(),Qt::ElideRight,width-(id.isEmpty()?16:42)));x+=width+8;}
  const auto activities=data.value("_boardActivities").toArray();const int columns=inner>=440?2:1;const qreal width=(inner-12*(columns-1))/columns;
  for(int i=0;i<activities.size()&&i<4;++i){const qreal top=146+(i/columns)*108,cardHeight=qMin(96.0,box.height()-top-36);if(cardHeight<56)break;const auto activity=activities[i].toObject();QRectF card(margin+(i%columns)*(width+12),top,width,cardHeight);p->setBrush(paper);p->setPen(edge);p->drawRoundedRect(card,6,6);artwork(data.value("entryFrameAssetId").toString(),card,true);QString icon;for(const auto& value:data.value("activityIcons").toArray())if(value.toObject()["activityId"]==activity["id"])icon=value.toObject()["imageAssetId"].toString();artwork(icon,QRectF(card.x()+10,card.y()+10,qMin(56.0,cardHeight-20),qMin(56.0,cardHeight-20)));const qreal start=icon.isEmpty()?12:78;p->setPen(ink);p->drawText(QRectF(card.x()+start,card.y()+10,width-start-10,30),Qt::AlignLeft|Qt::AlignVCenter,QFontMetricsF(font).elidedText(activity.value("label").toString(activity.value("id").toString()),Qt::ElideRight,width-start-10));if(cardHeight>=88&&data.value("showActivityDescription").toBool())p->drawText(card.adjusted(start,42,-10,-10),Qt::AlignLeft|Qt::AlignTop|Qt::TextWordWrap,activity.value("description").toString());}
  p->setPen(ink);p->drawText(QRectF(margin,qMax(0.0,box.height()-28),inner,22),Qt::AlignRight|Qt::AlignVCenter,QObject::tr("플레이에서 선택 · 적용 · 취소를 확인하세요."));p->restore();
}
