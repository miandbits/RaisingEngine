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
  if(!child.contains("_groupIds"))child["_groupIds"]=Json::array();for(const auto& id:parent.value("_groupIds",Json::array()))child["_groupIds"].push_back(id);if(parent.value("kind","")=="group")child["_groupIds"].push_back(parent.at("id"));
  const auto when=parent.value("visibleWhen","always");
  child["hidden"]=child.value("hidden",false)||parent.value("hidden",false)||(when!="always"&&when!=phase);
  child["opacity"]=child.value("opacity",1.0)*parent.value("opacity",1.0);
  if(parent.contains("_boundEnabled"))child["_boundEnabled"]=child.value("_boundEnabled",true)&&parent.value("_boundEnabled",true);
  for(const auto& key:{"visibleCondition","enabledCondition"})if(parent.contains(key))
    child[key]=child.contains(key)?Json{{"op","all"},{"conditions",Json::array({parent.at(key),child.at(key)})}}:parent.at(key);
  auto clip=parent.value("_clipRect",Json());
  if((parent.value("clipChildren",false)||parent.value("overflow","")=="hidden")&&parent.value("overflow","")!="scroll")clip=intersect_ui_clip(clip,{{"x",parent.at("x")},{"y",parent.at("y")},{"width",parent.at("width")},{"height",parent.at("height")}});
  if(parent.value("overflow","")=="scroll")child["_scrollParent"]=parent.at("id");else if(parent.contains("_scrollParent")&&!child.contains("_scrollParent"))child["_scrollParent"]=parent.at("_scrollParent");
  clip=intersect_ui_clip(child.value("_clipRect",Json()),clip);if(!clip.is_null())child["_clipRect"]=clip;
}
inline Json hierarchy_elements(const Json& elements,double reference_width,double reference_height,double actual_width,double actual_height,const std::string& phase) {
  Json result=Json::array();std::set<std::string> visited;
  std::function<void(const std::string&,const Json&,double,double,double,double,int)> walk;
  walk=[&](const std::string& parent_id,const Json& parent,double rw,double rh,double aw,double ah,int depth){
    if(depth>32)return;
    double cursor=0,row_y=0,row_h=0;int index=0;const auto flow=parent.is_null()?Json::object():parent.value("flowLayout",Json::object());const double pad=flow.value("padding",0.0),gap=flow.value("gap",8.0);cursor=row_y=pad;
    for(const auto& raw:elements)if(raw.value("parentId","")==parent_id) {
      const auto id=raw.value("id","");if(!visited.insert(id).second)continue;
      auto e=anchored_element(raw,aw-rw,ah-rh);e.erase("parentId");
      if(!flow.empty()){const auto direction=flow.value("direction","column");if(direction=="row"){e["x"]=cursor;e["y"]=pad;cursor+=e.value("width",16.0)+gap;}else if(direction=="column"){e["x"]=pad;e["y"]=cursor;e["width"]=std::max(16.0,aw-2*pad);cursor+=e.value("height",16.0)+gap;}else{const int cols=flow.value("columns",2);if(index&&index%cols==0){row_y+=row_h+gap;row_h=0;}const double w=std::max(16.0,(aw-pad*2-gap*(cols-1))/cols);e["x"]=pad+(index%cols)*(w+gap);e["y"]=row_y;e["width"]=w;row_h=std::max(row_h,e.value("height",16.0));}++index;}
      if(!parent.is_null()){e["x"]=e.value("x",0.0)+parent.value("x",0.0);e["y"]=e.value("y",0.0)+parent.value("y",0.0);inherit_ui_element(e,parent,phase);}
      result.push_back(e);
      if(e.value("kind","")=="group")walk(id,e,raw.value("width",16.0),raw.value("height",16.0),e.value("width",16.0),e.value("height",16.0),depth+1);
    }
  };walk("",nullptr,reference_width,reference_height,actual_width,actual_height,0);return result;
}

inline Json expand_ui_components(const Json& visual,const Json& elements,const std::string& phase,std::set<std::string> stack={},int* remaining=nullptr) {
  int local_budget=10000;if(!remaining)remaining=&local_budget;Json result=Json::array();
  for(auto instance:elements){if(--*remaining<0)throw std::runtime_error("UI expansion exceeds 10000 elements");
    if(instance.value("kind","")!="component"){result.push_back(instance);continue;}
    const auto id=instance.value("componentId","");if(stack.count(id)||stack.size()>=8)throw std::runtime_error("UI component cycle/depth limit");
    Json definition;for(const auto& c:visual.value("components",Json::array()))if(c.value("id","")==id)definition=c;
    if(definition.is_null())throw std::runtime_error("Unknown UI component: "+id);
    auto children=definition.at("elements");for(auto& child:children)for(const auto& o:instance.value("overrides",Json::array()))if(o.at("elementId")==child.at("id"))for(const auto& [key,v]:o.at("values").items())child[key]=v;
    auto flat=hierarchy_elements(children,definition.value("width",16.0),definition.value("height",16.0),instance.value("width",16.0),instance.value("height",16.0),phase);
    const auto prefix=instance.value("id","")+"__";
    for(auto& child:flat){child["id"]=prefix+child.value("id","");if(child.contains("_groupIds"))for(auto& id:child["_groupIds"])id=prefix+id.get<std::string>();if(child.contains("_scrollParent"))child["_scrollParent"]=prefix+child.at("_scrollParent").get<std::string>();
      child["x"]=child.value("x",0.0)+instance.value("x",0.0);child["y"]=child.value("y",0.0)+instance.value("y",0.0);
      if(child.contains("_clipRect")){child["_clipRect"]["x"]=child["_clipRect"].value("x",0.0)+instance.value("x",0.0);child["_clipRect"]["y"]=child["_clipRect"].value("y",0.0)+instance.value("y",0.0);}inherit_ui_element(child,instance,phase);
    }
    auto next=stack;next.insert(id);for(auto child:expand_ui_components(visual,flat,phase,next,remaining))result.push_back(child);
  }return result;
}
inline Json expanded_elements(const Json& visual,const Json& screen,int width,int height,const std::string& phase) {
  return expand_ui_components(visual,hierarchy_elements(screen.at("elements"),visual.value("width",1280),visual.value("height",720),width,height,phase),phase);
}
