#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <QFontMetricsF>
#include <QObject>

// Editor-only initial assignments. Never executes a rule or writes preview data.
inline QJsonArray initialRosterPreview(const QJsonObject& project) {
  QJsonArray rows;
  const auto names=project.value("_previewContextNames").toObject().value("characterId").toObject();
  const auto activities=project.value("_previewActivities").toArray();
  const auto attributes=project.value("_previewAttributes").toObject();
  for(const auto& value:project.value("roster").toObject().value("members").toArray()) {
    const auto member=value.toObject();QJsonObject activity;
    for(const auto& entry:activities)if(entry.toObject().value("id")==member.value("activityId")){activity=entry.toObject();break;}
    QStringList costs;for(const auto& value:activity.value("costs").toArray()) {
      const auto cost=value.toObject();const auto id=cost.value("attributeId").toString();
      costs<<attributes.value(id).toObject().value("label").toString(id)+" "+QString::number(cost.value("amount").toDouble());
    }
    const auto id=member.value("characterId").toString();
    rows.append(QJsonObject{{"name",names.value(id).toString(id)},
      {"activity",activity.value("label").toString(QObject::tr("활동 미지정"))},
      {"costs",QObject::tr("비용 · %1").arg(costs.isEmpty()?QObject::tr("없음"):costs.join(" / "))},
      {"description",activity.value("description").toString()}});
  }
  return rows;
}

inline void paintRosterPreview(QPainter* p,const QRectF& box,const QJsonObject& data) {
  p->save();p->setClipRect(box,Qt::IntersectClip);
  const auto fillValue=data.value("fill").toString("#fff9e8");
  QColor fill(fillValue.left(7)),ink(data.value("color").toString("#304d46"));
  if(fillValue.size()==9)fill.setAlpha(fillValue.right(2).toInt(nullptr,16));
  p->setBrush(fill);p->setPen(Qt::NoPen);p->drawRoundedRect(box,6,6);
  QFont font(data.value("_fontFamily").toString("Noto Sans KR"));font.setPixelSize(17);p->setFont(font);p->setPen(ink);
  const auto rows=data.value("_rosterPreview").toArray();
  p->drawText(box.adjusted(20,12,-20,-12),Qt::AlignTop|Qt::AlignLeft,QObject::tr("초기 배정 미리보기 · 실행 결과 아님"));
  const qreal inner=box.width()-40;const int columns=inner>=560?2:1;
  const qreal width=(inner-16*(columns-1))/columns;
  const qreal footer=qMax(45.0,box.height()-54),bottom=qMax(45.0,footer-18);
  int shown=0;
  for(int i=0;i<rows.size();++i){
    const qreal y=46+(i/columns)*138;if(y+62>bottom)break;
    const qreal x=20+(i%columns)*(width+16);const auto row=rows[i].toObject();
    p->save();p->setClipRect(QRectF(x,y,width,bottom-y),Qt::IntersectClip);
    p->setPen(ink);p->drawText(QRectF(x,y,width,25),Qt::AlignLeft|Qt::AlignVCenter,QFontMetricsF(font).elidedText(row["name"].toString(),Qt::ElideRight,width));
    p->setBrush(QColor("#e1e9dd"));p->setPen(Qt::NoPen);p->drawRoundedRect(QRectF(x,y+27,width,34),3,3);
    p->setPen(ink);p->drawText(QRectF(x+10,y+27,width-34,34),Qt::AlignLeft|Qt::AlignVCenter,QFontMetricsF(font).elidedText(row["activity"].toString(),Qt::ElideRight,qMax(1.0,width-34)));
    p->drawText(QRectF(x+width-24,y+27,20,34),Qt::AlignCenter,QString::fromUtf8("▾"));
    QString detail;if(data.value("showActivityRules").toBool())detail=row["costs"].toString();
    if(data.value("showActivityDescription").toBool())detail+=(detail.isEmpty()?"":"\n")+row["description"].toString();
    p->drawText(QRectF(x,y+67,width,62),Qt::AlignLeft|Qt::AlignTop|Qt::TextWordWrap,detail);p->restore();++shown;
  }
  if(shown<rows.size()){p->fillRect(QRectF(20,bottom-22,inner,22),fill);p->setPen(ink);p->drawText(QRectF(20,bottom-22,inner,22),Qt::AlignRight|Qt::AlignVCenter,QObject::tr("플레이에서 스크롤 · %1명 더").arg(rows.size()-shown));}
  if(box.height()>=110){
    const QStringList buttons={QObject::tr("활동 배정 적용"),QObject::tr("배정한 활동 함께 실행")};
    for(int i=0;i<2;++i){QRectF r(20+i*(inner+12)/2,footer,(inner-12)/2,36);p->fillRect(r,QColor("#355c4c"));p->setPen(Qt::white);p->drawText(r.adjusted(8,0,-8,0),Qt::AlignCenter,QFontMetricsF(font).elidedText(buttons[i],Qt::ElideRight,r.width()-16));}
  }
  p->restore();
}
