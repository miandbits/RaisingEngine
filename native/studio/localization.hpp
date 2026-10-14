#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QSet>
inline QString translationSegment(QString s){return s.replace("~","~0").replace("/","~1");}
inline QJsonValue localizeValue(const QJsonValue& value,const QString& path,const QMap<QString,QJsonObject>& entries,const QString& locale,const QString& fallback){
  if(value.isArray()){QJsonArray out;int i=0;for(auto v:value.toArray()){const auto id=v.toObject()["id"].toString();out.append(localizeValue(v,path+"/"+translationSegment(id.isEmpty()?QString::number(i):"@"+id),entries,locale,fallback));++i;}return out;}
  if(!value.isObject())return value;
  static const QSet<QString> fields={"title","description","label","name","text","prompt","valueFormat"};
  auto out=value.toObject();for(auto it=out.begin();it!=out.end();++it){if(it.key()=="localization"||it.key()=="assets"||it.key()=="initialState")continue;auto p=path+"/"+translationSegment(it.key());if(it.value().isString()&&fields.contains(it.key())){auto e=entries.value(p);if(e["source"]!=it.value())continue;auto t=e["translations"].toObject();auto text=t[locale].toString();if(text.trimmed().isEmpty())text=t[fallback].toString();if(!text.trimmed().isEmpty())it.value()=text;}else it.value()=localizeValue(it.value(),p,entries,locale,fallback);}return out;
}
inline QJsonObject localizedProject(QJsonObject project,QString locale){
 auto config=project["localization"].toObject();if(config.isEmpty())return project;
 if(locale.isEmpty())locale=config["defaultLocale"].toString();QMap<QString,QJsonObject> entries;for(auto e:config["entries"].toArray())entries[e.toObject()["key"].toString()]=e.toObject();
 auto out=localizeValue(project,"/project",entries,locale,config["fallbackLocale"].toString()).toObject();QString font;for(auto l:config["locales"].toArray())if(l.toObject()["id"]==locale)font=l.toObject()["fontAssetId"].toString();
  if(!font.isEmpty()){auto visual=out["visual"].toObject();auto defaults=visual["defaultStyle"].toObject();defaults["fontAssetId"]=font;visual["defaultStyle"]=defaults;for(auto key:{"screens","components"}){if(!visual.contains(key))continue;auto groups=visual[key].toArray();for(int i=0;i<groups.size();i++){auto s=groups[i].toObject();auto elements=s["elements"].toArray();for(int j=0;j<elements.size();j++){auto e=elements[j].toObject();e["fontAssetId"]=font;elements[j]=e;}s["elements"]=elements;groups[i]=s;}visual[key]=groups;}out["visual"]=visual;}return out;
}
