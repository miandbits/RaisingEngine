#pragma once
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>

inline QString authoringKind(const QString& kind) {
  const QMap<QString,QString> labels{{"text",QObject::tr("글")},{"image",QObject::tr("이미지")},{"panel",QObject::tr("패널")},{"group",QObject::tr("그룹")},{"dialogue",QObject::tr("대사")},{"choices",QObject::tr("선택지")},{"button",QObject::tr("버튼")},{"schedule",QObject::tr("활동 계획")},{"result",QObject::tr("활동 결과")},{"stats",QObject::tr("능력치 목록")},{"meter",QObject::tr("수치 막대")},{"input",QObject::tr("입력")},{"saveSlots",QObject::tr("저장 목록")},{"loadSlots",QObject::tr("불러오기 목록")},{"playerSettings",QObject::tr("플레이어 설정")}};
  return labels.value(kind,kind);
}
// Author text wins. Known bindings/kinds describe unnamed controls without
// inventing a character name or a runtime value. IDs remain stable data/tooltips.
inline QString authoringLabel(const QJsonObject& item) {
  for(const auto field:{"label","name","text"}) {
    const auto value=item.value(field).toString().simplified();
    if(!value.isEmpty())return value.size()>80?value.left(77)+QString::fromUtf8("…"):value;
  }
  const QMap<QString,QString> bindings{{"calendar",QObject::tr("달력")},{"title",QObject::tr("게임 제목")},{"description",QObject::tr("게임 설명")},{"speaker",QObject::tr("말하는 인물")},{"relationship-value",QObject::tr("관계")},{"ending",QObject::tr("엔딩")}};
  const auto binding=item.value("binding").toString();if(bindings.contains(binding))return bindings.value(binding);
  const auto kind=item.value("kind").toString(),friendly=authoringKind(kind);
  return !kind.isEmpty()&&friendly!=kind?friendly:item.value("id").toString();
}
// In lists, duplicate names need a visible discriminator as well as a tooltip.
inline QString authoringDistinctLabel(const QJsonObject& item,const QMap<QString,QJsonObject>& siblings) {
  const auto name=authoringLabel(item),id=item.value("id").toString();
  for(auto it=siblings.begin();it!=siblings.end();++it){const auto other=it.value();if(other.value("id").toString()!=id&&authoringLabel(other)==name)return name+" · "+id;}
  return name;
}
