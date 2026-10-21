#pragma once

// Slot paths are derived exclusively from the configured save path and bounded
// integer indices. Content and metadata never supply an arbitrary file path.
inline bool has_save_slots(const Json& visual) {
  for(const auto& screen:visual.value("screens",Json::array()))
    for(const auto& e:screen.value("elements",Json::array()))
      if(e.value("kind","")=="saveSlots"||e.value("kind","")=="loadSlots")return true;
  for(const auto& component:visual.value("components",Json::array()))
    for(const auto& e:component.value("elements",Json::array()))
      if(e.value("kind","")=="saveSlots"||e.value("kind","")=="loadSlots")return true;
  return visual.contains("saveSystem");
}
inline int slot_count(const Json& visual,bool automatic) {
  return std::clamp(visual.value("saveSystem",Json::object()).value(automatic?"autoSlots":"manualSlots",automatic?3:6),automatic?0:1,automatic?9:30);
}
inline fs::path slot_directory(const fs::path& legacy) {return legacy.parent_path()/(legacy.stem().u8string()+".slots");}
inline fs::path slot_path(const fs::path& legacy,bool automatic,int index) {
  if(!automatic&&index==0)return legacy;
  return slot_directory(legacy)/((automatic?"auto-":"manual-")+std::to_string(index+1)+".json");
}
inline Json slot_preview(const fs::path& file,const Json& content) {
  Json out={{"status","empty"}};
  try {
    if(!fs::exists(file))return out;
    const auto value=read_json(file),state=value.value("state",Json());
    if(!state.is_object()||!state.value("attributes",Json()).is_object()||!state.value("periodIndex",Json()).is_number_integer()||!state.value("phase",Json()).is_string()||!value.value("commands",Json()).is_array())throw std::runtime_error("Invalid save metadata");
    if(state.at("periodIndex").get<int64_t>()<0||state.at("periodIndex").get<int64_t>()>1000000)throw std::runtime_error("Invalid save period");
    for(const auto& a:state.at("attributes"))if(!a.is_number_integer())throw std::runtime_error("Invalid save attribute");
    const bool same=value.value("contentFingerprint",Json())==content.at("contentFingerprint");
    out={{"status",same?"ready":"incompatible"},{"state",state},{"sameContent",same}};
    const auto record=value.value("playerPresentation",Json::object()).value("saveRecord",Json::object());
    if(record.is_object()){
      Json safe=Json::object();
      for(const auto key:{"date","place","screenId","phase","thumbnail","savedAt"})if(record.contains(key)&&record.at(key).is_string()&&record.at(key).get<std::string>().size()<512)safe[key]=record.at(key);
      if(record.contains("sequence")&&record.at("sequence").is_number_integer()&&record.at("sequence").get<int64_t>()>=0&&record.at("sequence").get<int64_t>()<INT64_MAX-1)safe["sequence"]=record.at("sequence");
      out["record"]=safe;
    }
  } catch(...) {out={{"status","invalid"}};}
  return out;
}
