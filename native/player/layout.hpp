#pragma once
#include "session.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <set>

// Presentation layout never changes the authored document or simulation state.
inline Json anchored_element(Json element,double delta_width,double delta_height) {
  const auto horizontal=element.value("anchorX","left"),vertical=element.value("anchorY","top");
  if(horizontal=="center")element["x"]=element.value("x",0.0)+delta_width/2;
  else if(horizontal=="right")element["x"]=element.value("x",0.0)+delta_width;
  else if(horizontal=="stretch")element["width"]=std::max(16.0,element.value("width",16.0)+delta_width);
  if(vertical=="center")element["y"]=element.value("y",0.0)+delta_height/2;
  else if(vertical=="bottom")element["y"]=element.value("y",0.0)+delta_height;
  else if(vertical=="stretch")element["height"]=std::max(16.0,element.value("height",16.0)+delta_height);
  return element;
}

inline std::pair<int,int> logical_viewport(const Json& visual,int viewport_width,int viewport_height) {
  const int reference_width=visual.value("width",1280),reference_height=visual.value("height",720);
  if(visual.value("viewportMode","fit")!="responsive"||viewport_width<=0||viewport_height<=0)return {reference_width,reference_height};
  const double aspect=static_cast<double>(viewport_width)/viewport_height;
  if(aspect>static_cast<double>(reference_width)/reference_height)return {static_cast<int>(std::lround(reference_height*aspect)),reference_height};
  return {reference_width,static_cast<int>(std::lround(reference_width/aspect))};
}

inline Json intersect_ui_clip(const Json& a,const Json& b) {
  if(a.is_null())return b;if(b.is_null())return a;
  const double x=std::max(a.value("x",0.0),b.value("x",0.0)),y=std::max(a.value("y",0.0),b.value("y",0.0));
  return {{"x",x},{"y",y},{"width",std::max(0.0,std::min(a.value("x",0.0)+a.value("width",0.0),b.value("x",0.0)+b.value("width",0.0))-x)},
    {"height",std::max(0.0,std::min(a.value("y",0.0)+a.value("height",0.0),b.value("y",0.0)+b.value("height",0.0))-y)}};
}
inline void inherit_ui_element(Json& child,const Json& parent,const std::string& phase) {
  const auto when=parent.value("visibleWhen","always");
  child["hidden"]=child.value("hidden",false)||parent.value("hidden",false)||(when!="always"&&when!=phase);
  child["opacity"]=child.value("opacity",1.0)*parent.value("opacity",1.0);
  if(parent.contains("_boundEnabled"))child["_boundEnabled"]=child.value("_boundEnabled",true)&&parent.value("_boundEnabled",true);
  for(const auto& key:{"visibleCondition","enabledCondition"})if(parent.contains(key))
    child[key]=child.contains(key)?Json{{"op","all"},{"conditions",Json::array({parent.at(key),child.at(key)})}}:parent.at(key);
  auto clip=parent.value("_clipRect",Json());
  if(parent.value("clipChildren",false))clip=intersect_ui_clip(clip,{{"x",parent.at("x")},{"y",parent.at("y")},{"width",parent.at("width")},{"height",parent.at("height")}});
  clip=intersect_ui_clip(child.value("_clipRect",Json()),clip);if(!clip.is_null())child["_clipRect"]=clip;
}
inline Json hierarchy_elements(const Json& elements,double reference_width,double reference_height,double actual_width,double actual_height,const std::string& phase) {
  Json result=Json::array();std::set<std::string> visited;
  std::function<void(const std::string&,const Json&,double,double,double,double,int)> walk;
  walk=[&](const std::string& parent_id,const Json& parent,double rw,double rh,double aw,double ah,int depth){
    if(depth>32)return;
    for(const auto& raw:elements)if(raw.value("parentId","")==parent_id) {
      const auto id=raw.value("id","");if(!visited.insert(id).second)continue;
      auto e=anchored_element(raw,aw-rw,ah-rh);e.erase("parentId");
      if(!parent.is_null()){e["x"]=e.value("x",0.0)+parent.value("x",0.0);e["y"]=e.value("y",0.0)+parent.value("y",0.0);inherit_ui_element(e,parent,phase);}
      result.push_back(e);
      if(e.value("kind","")=="group")walk(id,e,raw.value("width",16.0),raw.value("height",16.0),e.value("width",16.0),e.value("height",16.0),depth+1);
    }
  };walk("",nullptr,reference_width,reference_height,actual_width,actual_height,0);return result;
}

inline Json expanded_elements(const Json& visual,const Json& screen,int width,int height,const std::string& phase) {
  Json result=Json::array();
  for(auto instance:hierarchy_elements(screen.at("elements"),visual.value("width",1280),visual.value("height",720),width,height,phase)) {
    if(instance.value("kind","")!="component"){result.push_back(std::move(instance));continue;}
    const auto components=visual.value("components",Json::array());
    const Json* definition=nullptr;
    for(const auto& candidate:components)if(candidate.value("id","")==instance.value("componentId","")){definition=&candidate;break;}
    if(!definition)throw std::runtime_error("Unknown UI component: "+instance.value("componentId",""));
    auto children=definition->at("elements");for(auto& child:children) {
      if(child.value("kind","")=="component")throw std::runtime_error("Nested UI components are not supported");
      const auto child_id=child.value("id","");
      for(const auto& override_entry:instance.value("overrides",Json::array()))if(override_entry.value("elementId","")==child_id)
        for(auto it=override_entry.at("values").begin();it!=override_entry.at("values").end();++it)child[it.key()]=it.value();
    }
    for(auto child:hierarchy_elements(children,definition->value("width",16.0),definition->value("height",16.0),instance.value("width",16.0),instance.value("height",16.0),phase)) {
      const auto child_id=child.value("id","");
      child["id"]=instance.value("id","")+"__"+child_id;
      child["x"]=child.value("x",0.0)+instance.value("x",0.0);
      child["y"]=child.value("y",0.0)+instance.value("y",0.0);
      if(child.contains("_clipRect")){child["_clipRect"]["x"]=child["_clipRect"].value("x",0.0)+instance.value("x",0.0);child["_clipRect"]["y"]=child["_clipRect"].value("y",0.0)+instance.value("y",0.0);}
      inherit_ui_element(child,instance,phase);
      result.push_back(std::move(child));
    }
  }
  return result;
}
