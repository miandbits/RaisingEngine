#pragma once
#include "session.hpp"
#include "presentation-update.hpp"
#include <set>

// Export-time JSON Schema remains the full content contract. These checks guard
// native rendering boundaries if an exported bundle is later damaged or edited.
inline void validate_bundle(const Json& bundle,const fs::path& root) {
  auto fail=[](const std::string& path){throw std::runtime_error("Invalid game bundle: "+path);};
  if(!bundle.is_object()||bundle.value("formatVersion",Json())!="1")fail("formatVersion");
  if(!bundle.contains("content")||!bundle.at("content").is_object())fail("content");
  if(!bundle.contains("assets")||!bundle.at("assets").is_object())fail("assets");
  const auto canonical_root=fs::weakly_canonical(root);
  for(const auto& [id,path]:bundle.at("assets").items()) {
    if(!path.is_string())fail("assets path");
    const auto relative=fs::u8path(path.get<std::string>());
    if(relative.is_absolute()||relative.has_root_name())fail("assets."+id+": absolute path");
    const auto resolved=fs::weakly_canonical(canonical_root/relative),within=resolved.lexically_relative(canonical_root);
    if(within.empty()||*within.begin()==".."||!fs::is_regular_file(resolved))fail("assets."+id+": missing or outside bundle");
  }
  const auto& content=bundle.at("content");
  if(!content.contains("project")||!content.at("project").is_object())fail("content.project");
  const auto& project=content.at("project");
  if(!project.contains("visual")||!project.at("visual").is_object())fail("content.project.visual");
  const auto& visual=project.at("visual");
  auto integer=[&](const Json& value,const char* key,int minimum,int maximum,const std::string& path){
    if(!value.contains(key)||!value.at(key).is_number_integer()||value.at(key).get<double>()<minimum||value.at(key).get<double>()>maximum)fail(path+"."+key);
  };
  integer(visual,"width",640,3840,"visual");integer(visual,"height",360,2160,"visual");
  const bool routed=visual.contains("routing");
  if(routed&&!visual.at("routing").is_object())fail("visual.routing");
  if(!visual.contains("screens")||!visual.at("screens").is_array()||visual.at("screens").size()<(routed?1:4)||visual.at("screens").size()>100)fail("visual.screens");
  std::set<std::string> screens;
  for(const auto& screen:visual.at("screens")) {
    if(!screen.is_object()||!screen.contains("id")||!screen.at("id").is_string()||!screens.insert(screen.at("id").get<std::string>()).second)fail("visual.screens[].id");
    if(!screen.contains("elements")||!screen.at("elements").is_array()||screen.at("elements").size()>100)fail("visual.screens[].elements");
    std::set<std::string> elements;
    for(const auto& element:screen.at("elements")) {
      if(!element.is_object()||!element.contains("id")||!element.at("id").is_string()||!elements.insert(element.at("id").get<std::string>()).second)fail("visual.elements[].id");
      if(element.contains("parentId")&&(!element.at("parentId").is_string()||element.at("parentId").get<std::string>().empty()))fail("visual.element.parentId");
      integer(element,"x",element.contains("parentId")?-3840:0,3840,"visual.element");integer(element,"y",element.contains("parentId")?-2160:0,2160,"visual.element");
      integer(element,"width",16,3840,"visual.element");integer(element,"height",16,2160,"visual.element");
    }
  }
  if(!routed)for(const auto* required:{"title","schedule","story","ending"})if(!screens.count(required))fail("visual.screens: missing "+std::string(required));
  if(routed){
    const auto& routing=visual.at("routing");
    if(!routing.contains("defaultScreenId")||!routing.at("defaultScreenId").is_string()||!screens.count(routing.at("defaultScreenId").get<std::string>()))fail("visual.routing.defaultScreenId");
    if(!routing.contains("phases")||!routing.at("phases").is_array())fail("visual.routing.phases");
    for(const auto* list:{"phases","conditions"})if(routing.contains(list)){
      if(!routing.at(list).is_array())fail(std::string("visual.routing.")+list);
      for(const auto& route:routing.at(list))if(!route.is_object()||!route.contains("screenId")||!route.at("screenId").is_string()||!screens.count(route.at("screenId").get<std::string>()))fail(std::string("visual.routing.")+list+".screenId");
    }
  }
  for(const auto* key:{"attributes","characters","activities","events","endings"})if(!content.contains(key)||!content.at(key).is_array())fail(std::string("content.")+key);
  if(content.at("activities").empty())fail("content.activities: empty");
  if(bundle.contains("presentationRevision")&&bundle.at("presentationRevision").contains("visual")){
    const auto& revision=bundle.at("presentationRevision");
    if(revision.value("formatVersion",Json())!="1"||revision.value("policy",Json())!="paint-layout-v1")fail("presentationRevision.policy");
    verify_presentation_visual(visual,revision.at("visual"));
    auto rendered=bundle;rendered.erase("presentationRevision");rendered["content"]["project"]["visual"]=revision.at("visual");
    validate_bundle(rendered,root);
  }
}
