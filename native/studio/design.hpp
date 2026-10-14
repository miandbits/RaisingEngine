#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
#include <QFontDatabase>
#include <QMap>
inline QString projectFontFamily(const QString& path){static QMap<QString,QString> loaded;if(!loaded.contains(path)){const auto families=QFontDatabase::applicationFontFamilies(QFontDatabase::addApplicationFont(path));loaded[path]=families.value(0);}return loaded.value(path);}
inline QStringList styleFields(){return {"fontAssetId","fontSize","color","fill","radius","align","padding","borderWidth","borderColor","hoverFill","pressedFill","disabledColor"};}
inline QJsonObject resolvedStyle(const QJsonObject& visual,const QJsonObject& element){
  auto result=visual["defaultStyle"].toObject();
  for(auto s:visual["styles"].toArray()){auto style=s.toObject();if(style["id"]==element["styleId"])for(auto it=style.begin();it!=style.end();++it)if(it.key()!="id"&&it.key()!="label")result[it.key()]=it.value();}
  for(auto it=element.begin();it!=element.end();++it)result[it.key()]=it.value();return result;
}
