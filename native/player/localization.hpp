#pragma once
#include "session.hpp"
#include <set>
#include <map>
inline std::string locale_segment(std::string text){std::string out;for(char c:text)out+=c=='~'?"~0":c=='/'?"~1":std::string(1,c);return out;}
inline Json translate_tree(const Json& value,const std::string& path,const std::map<std::string,Json>& entries,const std::string& locale,const std::string& fallback){
 if(value.is_array()){Json out=Json::array();size_t i=0;for(const auto& v:value){auto part=v.is_object()&&v.contains("id")&&v.at("id").is_string()?"@"+v.at("id").get<std::string>():std::to_string(i);out.push_back(translate_tree(v,path+"/"+locale_segment(part),entries,locale,fallback));i++;}return out;}
 if(!value.is_object())return value;
 static const std::set<std::string> fields={"title","description","label","name","text","prompt","valueFormat"};auto out=value;
 for(auto it=out.begin();it!=out.end();++it){if(it.key()=="localization"||it.key()=="assets"||it.key()=="initialState")continue;auto p=path+"/"+locale_segment(it.key());if(it.value().is_string()&&fields.count(it.key())){auto entry=entries.find(p);if(entry==entries.end()||entry->second.at("source")!=it.value())continue;const auto& t=entry->second.at("translations");for(const auto& id:{locale,fallback}){auto text=t.value(id,"");if(text.find_first_not_of(" \t\r\n")!=std::string::npos){it.value()=text;break;}}}else it.value()=translate_tree(it.value(),p,entries,locale,fallback);}return out;
}
inline Json localized_content(const Json& content,const std::string& locale){
 const auto config=content.at("project").value("localization",Json::object());if(config.empty())return content;
 std::map<std::string,Json> entries;for(const auto& e:config.at("entries"))entries[e.at("key")]=e;
 auto out=translate_tree(content,"",entries,locale,config.value("fallbackLocale",""));std::string font;for(const auto& l:config.at("locales"))if(l.at("id")==locale)font=l.value("fontAssetId","");
  if(!font.empty()&&out.at("project").contains("visual")){auto& visual=out["project"]["visual"];visual["defaultStyle"]["fontAssetId"]=font;for(auto& s:visual["screens"])for(auto& e:s["elements"])e["fontAssetId"]=font;if(visual.contains("components"))for(auto& c:visual["components"])for(auto& e:c["elements"])e["fontAssetId"]=font;}return out;
}
