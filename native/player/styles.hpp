#pragma once
// Same precedence and field contract as packages/visual/styles.ts.
inline Json resolved_style(const Json& visual,const Json& element) {
  Json result=visual.value("defaultStyle",Json::object());
  for(const auto& style:visual.value("styles",Json::array()))if(style.at("id")==element.value("styleId",Json()))for(auto it=style.begin();it!=style.end();++it)if(it.key()!="id"&&it.key()!="label")result[it.key()]=it.value();
  for(auto it=element.begin();it!=element.end();++it)result[it.key()]=it.value();
  return result;
}
