#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

// Editor-only labels. Never written back into the project or runtime state.
inline QJsonObject contextNamesForPreview(const QJsonObject& view){
  QJsonObject names;
  auto add=[&](QString field,const QJsonObject& row){
    auto values=names.value(field).toObject();auto label=row.contains("name")?row.value("name"):row.value("label");
    values[row.value("id").toString()]=label.isString()?label.toString():QObject::tr("다국어 이름");names[field]=values;
  };
  for(const auto& value:view.value("documents").toArray()){
    const auto doc=value.toObject(),data=doc.value("data").toObject();const auto kind=doc.value("kind").toString();
    if(kind=="character"){add("actorId",data);add("characterId",data);}
    if(kind=="activity")add("activityId",data);
    if(kind=="attributes")for(const auto& row:data.value("attributes").toArray())add("attributeId",row.toObject());
    if(kind=="project")for(const auto pair:{qMakePair("items","itemId"),qMakePair("actions","actionId"),qMakePair("locations","locationId"),qMakePair("shops","shopId")})
      for(const auto& row:data.value("gameplay").toObject().value(pair.first).toArray())add(pair.second,row.toObject());
  }
  return names;
}

inline QString contextNameForPreview(const QJsonObject& project,const QJsonObject& element){
  const auto field=element.value("contextField").toString();
  auto id=element.value("uiContext").toObject().value(field).toString();
  if(id.isEmpty()&&field=="actorId")id=project.value("protagonistId").toString();
  const auto name=project.value("_previewContextNames").toObject().value(field).toObject().value(id).toString();
  return name.isEmpty()?(id.isEmpty()?QObject::tr("이름을 표시할 대상 연결"):QObject::tr("연결 대상 없음")):name;
}
