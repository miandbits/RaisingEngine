#pragma once
#include "session.hpp"
#include <set>
#include <cmath>

// The CLI accepts a display-only scene patch, never script nodes or commands.
inline Json read_preview_scene(const fs::path& file,const Json& content) {
  const auto scene=read_json(file,64*1024);
  if(!scene.is_object()||scene.empty())throw std::runtime_error("Preview scene must be a nonempty scene object");
  const std::set<std::string> keys={"backgroundAssetId","musicAssetId","soundAssetId","portraits"};
  for(const auto& [key,value]:scene.items()) {
    if(!keys.count(key))throw std::runtime_error("Unknown preview scene field: "+key);
    if(key=="portraits")continue;
    if(value.is_null()&&key!="soundAssetId")continue;
    if(!value.is_string())throw std::runtime_error("Preview scene asset must be an ID");
    bool found=false;for(const auto& asset:content.at("assets"))if(asset.at("id")==value&&asset.value("kind","")==(key=="backgroundAssetId"?"image":"audio"))found=true;
    if(!found)throw std::runtime_error("Unknown or mismatched preview scene asset: "+value.get<std::string>());
  }
  if(scene.contains("portraits")) {
    const auto& portraits=scene.at("portraits");if(!portraits.is_array()||portraits.size()>3)throw std::runtime_error("Preview scene accepts at most three portraits");
    std::set<std::string> positions;
    for(const auto& portrait:portraits) {
      if(!portrait.is_object())throw std::runtime_error("Preview portrait must be an object");
      const std::set<std::string> fields={"characterId","portraitId","position","scale","offsetX","offsetY"};
      for(const auto& [key,value]:portrait.items())if(!fields.count(key))throw std::runtime_error("Unknown preview portrait field: "+key);
      for(const auto* key:{"characterId","portraitId","position"})if(!portrait.contains(key)||!portrait.at(key).is_string())throw std::runtime_error("Preview portrait requires string IDs and position");
      const auto position=portrait.at("position").get<std::string>();if((position!="left"&&position!="center"&&position!="right")||!positions.insert(position).second)throw std::runtime_error("Invalid or duplicate preview portrait position");
      for(const auto* key:{"scale","offsetX","offsetY"})if(portrait.contains(key)) {
        if(!portrait.at(key).is_number())throw std::runtime_error("Preview portrait transform must be numeric");
        const auto n=portrait.at(key).get<double>();const bool scale=std::string(key)=="scale";
        if(!std::isfinite(n)||n<(scale?.1:-2)||n>(scale?4:2))throw std::runtime_error("Preview portrait transform is out of range");
      }
      bool found=false;for(const auto& character:content.at("characters"))if(character.at("id")==portrait.at("characterId")&&character.at("portraits").contains(portrait.at("portraitId").get<std::string>()))found=true;
      if(!found)throw std::runtime_error("Unknown preview portrait reference");
    }
  }
  return scene;
}
