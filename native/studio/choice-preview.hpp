#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <QFontMetricsF>

// Design-time sample only. Never evaluates conditions, writes game state, or
// claims these choices are available in the current play session.
inline QJsonObject initialChoicePreview(const QJsonArray& documents) {
  for (const auto& entry : documents) {
    const auto doc=entry.toObject();
    if(doc.value("kind")!="event"&&doc.value("kind")!="ending")continue;
    const auto data=doc.value("data").toObject();
    for(const auto& value:data.value("nodes").toArray()) {
      const auto node=value.toObject();
      if(node.value("type")!="choice"||node.value("choices").toArray().isEmpty())continue;
      QJsonArray rows;
      for(const auto& choice:node.value("choices").toArray()) {
        const auto row=choice.toObject();rows.append(QJsonObject{{"text",row.value("text")},{"hint",row.value("consequenceText")}});
      }
      return {{"source",data.value("label").toString(data.value("title").toString(data.value("id").toString()))+" / "+node.value("id").toString()},{"rows",rows}};
    }
  }
  return {};
}

inline void paintChoicePreview(QPainter* p,const QRectF& box,const QJsonObject& data) {
  p->save();p->setClipRect(box,Qt::IntersectClip);
  const auto sample=data.value("_choicePreview").toObject();const auto rows=sample.value("rows").toArray();
  QFont font(data.value("_fontFamily").toString("Noto Sans KR"));font.setPixelSize(qMax(12,data.value("fontSize").toInt(20)));p->setFont(font);
  QColor ink(data.value("color").toString("#304d46"));p->setPen(ink);
  const auto title=rows.isEmpty()?QObject::tr("선택지 영역 · 대본의 선택 노드를 만든 뒤 확인"):
      QObject::tr("배치 예시 · %1 · 조건과 효과는 플레이에서 확인").arg(sample.value("source").toString());
  QFont small=font;small.setPixelSize(14);p->setFont(small);
  p->drawText(box.adjusted(8,4,-8,-4),Qt::AlignTop|Qt::AlignLeft,QFontMetricsF(small).elidedText(title,Qt::ElideRight,qMax(1.0,box.width()-16)));
  qreal y=28;int shown=0;
  for(int i=0;i<rows.size();++i){
    const auto row=rows[i].toObject();const qreal width=qMax(1.0,box.width()-32);
    const QString label=(data.value("choiceLayout")=="cards"?QString("%1  ").arg(i+1,2,10,QChar('0')):QString())+row.value("text").toString();
    const auto titleHeight=qMax(qreal(font.pixelSize()+8),QFontMetricsF(font).boundingRect(QRectF(0,0,width,10000),Qt::TextWordWrap,label).height());
    const auto hint=data.value("showChoiceEffects").toBool()?row.value("hint").toString():QString();
    const qreal hintHeight=hint.isEmpty()?0:QFontMetricsF(small).boundingRect(QRectF(0,0,width,10000),Qt::TextWordWrap,hint).height()+6;
    const qreal height=titleHeight+hintHeight+20;
    if(y+height>box.height()-8)break;
    const QRectF card(8,y,box.width()-16,height);p->setBrush(QColor("#e1e9dd"));p->setPen(QPen(ink,1));p->drawRoundedRect(card,4,4);
    p->setFont(font);p->drawText(QRectF(16,y+8,width,titleHeight),Qt::AlignLeft|Qt::AlignTop|Qt::TextWordWrap,label);
    if(!hint.isEmpty()){p->setFont(small);p->drawText(QRectF(16,y+12+titleHeight,width,hintHeight),Qt::AlignLeft|Qt::AlignTop|Qt::TextWordWrap,hint);}
    y+=height+8;++shown;
  }
  if(shown<rows.size()){
    p->setFont(small);p->setPen(ink);
    p->drawText(QRectF(8,qMax(28.0,box.height()-22),box.width()-16,20),Qt::AlignRight|Qt::AlignVCenter,QObject::tr("영역 밖 선택지 %1개 · 높이 또는 글자 크기 확인").arg(rows.size()-shown));
  }
  p->restore();
}
