#pragma once
#include <QComboBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalBlocker>
#include <QStringList>

inline bool supportsControlStyle(const QString& kind) {
  return QStringList{"schedule","saveSlots","loadSlots"}.contains(kind);
}
inline QString controlStyleHelp() {
  return QObject::tr("공통 스타일의 배경·글자·테두리·마우스 올림·누름·비활성 글자 색만 적용합니다. 크기, 여백, 글꼴, 배경 표면은 바꾸지 않습니다.");
}
inline void fillControlStyles(QComboBox* field,const QJsonObject& visual,const QString& selected) {
  const QSignalBlocker block(field);
  field->clear();field->addItem(QObject::tr("기본 컨트롤 색"),QString());
  for(auto value:visual.value("styles").toArray()) {
    const auto style=value.toObject();const auto id=style.value("id").toString();
    field->addItem(style.value("label").toString(id),id);
  }
  if(!selected.isEmpty()&&field->findData(selected)<0)field->addItem(QObject::tr("찾을 수 없음: %1").arg(selected),selected);
  field->setCurrentIndex(qMax(0,field->findData(selected)));
}
// Only declared authoring scopes are references; plugin data is opaque.
inline bool usesControlStyle(const QJsonObject& visual,const QString& id) {
  if(id.isEmpty())return false;
  if(visual.value("readableLayout").toObject().value("controlStyleId").toString()==id)return true;
  for(auto scope:{"screens","components"})for(auto entry:visual.value(scope).toArray())
    for(auto child:entry.toObject().value("elements").toArray()) {
      const auto element=child.toObject();
      if(element.value("controlStyleId").toString()==id)return true;
      for(auto field:{"variants","overrides"})for(auto value:element.value(field).toArray())
        if(value.toObject().value("values").toObject().value("controlStyleId").toString()==id)return true;
    }
  return false;
}
