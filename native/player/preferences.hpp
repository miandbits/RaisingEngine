#pragma once
#include "json.hpp"
#include <fstream>
#include <filesystem>
#include <cmath>

inline Json default_player_preferences() {
  return {{"version",1},{"musicVolume",80},{"effectsVolume",80},{"fullscreen",false},{"textSpeed",40},{"autoAdvance",false},{"autoDelay",3},{"readingLayout","auto"},{"textScale",1.0},{"guidance",Json::object()}};
}
inline bool has_player_settings(const Json& visual,bool include_readable=true) {
  if(include_readable&&visual.contains("readableLayout"))return true;
  for(const auto* group:{"screens","components"})for(const auto& owner:visual.value(group,Json::array()))
    for(const auto& e:owner.value("elements",Json::array()))if((e.value("kind","")=="playerSettings"||e.value("action","")=="dialogueAuto"))return true;
  return false;
}
inline Json checked_player_preferences(const Json& raw) {
  if(!raw.is_object()||raw.value("version",Json())!=1)throw std::runtime_error("Unsupported player settings version");
  auto result=default_player_preferences();
  for(const auto* key:{"musicVolume","effectsVolume","textSpeed","autoDelay"}){
    const auto value=raw.value(key,Json());if(!value.is_number_integer())throw std::runtime_error("Invalid player setting: "+std::string(key));
    const double numeric=value.get<double>();if(numeric<0||numeric>100)throw std::runtime_error("Player setting out of range");const int n=value.get<int>();
    if((std::string(key)=="textSpeed"&&n!=0&&n!=20&&n!=40&&n!=70)||(std::string(key)=="autoDelay"&&(n<1||n>6))||((std::string(key)=="musicVolume"||std::string(key)=="effectsVolume")&&(n<0||n>100)))throw std::runtime_error("Player setting out of range: "+std::string(key));
    result[key]=n;
  }
  for(const auto* key:{"fullscreen","autoAdvance"}){if(!raw.value(key,Json()).is_boolean())throw std::runtime_error("Invalid player toggle");result[key]=raw.at(key);}
  if(raw.value("readingLayout",Json()).is_string()&&raw.at("readingLayout")=="on")result["readingLayout"]="on";
  if(raw.value("textScale",Json()).is_number()){const double scale=raw.at("textScale");if(scale==1||scale==1.25||scale==1.5||scale==2)result["textScale"]=scale;}
  // One versioned, bounded topic; unknown future topics never accumulate here.
  if(raw.contains("guidance")) {
    const auto& topics=raw.at("guidance");
    if(!topics.is_object()||(topics.contains("compact-planning-v1")&&!topics.at("compact-planning-v1").is_boolean()))throw std::runtime_error("Invalid planning guidance preference");
    if(topics.value("compact-planning-v1",false))result["guidance"]["compact-planning-v1"]=true;
  }
  return result;
}
inline void write_player_preferences(const std::filesystem::path& file,const Json& value) {
  auto temp=file;temp+=".tmp";auto backup=file;backup+=".bak";
  std::filesystem::create_directories(file.parent_path());
  {std::ofstream out(temp,std::ios::binary|std::ios::trunc);out<<checked_player_preferences(value).dump(2);out.flush();if(!out)throw std::runtime_error("Cannot write player settings");}
  if(std::filesystem::exists(file)){std::error_code ec;std::filesystem::remove(backup,ec);std::filesystem::rename(file,backup);}
  try{std::filesystem::rename(temp,file);}catch(...){if(std::filesystem::exists(backup)&&!std::filesystem::exists(file))std::filesystem::rename(backup,file);throw;}
}
// Never split a UTF-8 code point while revealing Korean/Japanese dialogue.
inline size_t dialogue_prefix_bytes(const std::string& text,size_t count) {
  size_t offset=0;while(offset<text.size()&&count--){++offset;while(offset<text.size()&&(static_cast<unsigned char>(text[offset])&0xc0)==0x80)++offset;}return offset;
}
