#pragma once
#include <RmlUi/Core/ElementText.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "json.hpp"

// Measurements of the current, laid-out document. This is deliberately separate
// from schema validation: hidden states and content behind a scroller are not bugs.
namespace LayoutDiagnostics {
inline bool scrolling(const std::string& value){return value=="auto"||value=="scroll";}
inline bool direct_text(Rml::Element* element){
  for(int i=0;i<std::min(element->GetNumChildren(),4096);++i)if(auto* text=dynamic_cast<Rml::ElementText*>(element->GetChild(i));text&&text->GetText().find_first_not_of(" \t\r\n")!=std::string::npos)return true;
  return false;
}
inline double luminance(Rml::Colourb color){auto linear=[](double value){value/=255.;return value<=.04045?value/12.92:std::pow((value+.055)/1.055,2.4);};return .2126*linear(color.red)+.7152*linear(color.green)+.0722*linear(color.blue);}
inline Json report(Rml::Element* document,const Json& nodes,float viewport_width,float viewport_height,float points_x,float points_y,Json configuration){
  Json issues=Json::array(),contrast=Json::array();size_t visible=0,controls=0,scroll_regions=0;bool truncated=false;
  std::unordered_map<std::string,std::string> owners;
  for(const auto& node:nodes){const auto id=node.value("id","");if(!id.empty())owners["node-"+id]=node.value("ownerId",id);if(!node.value("buttonId","").empty())owners[node.value("buttonId","")]=node.value("ownerId",id);}
  std::vector<Rml::Element*> elements{document};
  for(size_t i=0;i<elements.size();++i)for(int j=0;j<elements[i]->GetNumChildren();++j){if(elements.size()>=4096){truncated=true;break;}elements.push_back(elements[i]->GetChild(j));}
  const std::unordered_set<Rml::Element*> inspected(elements.begin(),elements.end());
  std::unordered_map<Rml::Element*,bool> complex_surface;
  for(auto it=elements.rbegin();it!=elements.rend();++it){auto* e=*it;auto* decoration=e->GetProperty("decorator");bool complex=e->GetTagName()=="img"||(decoration&&!decoration->ToString().empty()&&decoration->ToString()!="none");
    for(int i=0;i<e->GetNumChildren()&&!complex;++i){auto* child=e->GetChild(i);if(!inspected.count(child)){complex=true;break;}complex=complex_surface[child]||child->GetComputedValues().background_color().alpha!=0;}complex_surface[e]=complex;}
  std::unordered_set<std::string> seen;
  for(auto* element:elements){
    if(dynamic_cast<Rml::ElementText*>(element)||!element->IsVisible(true))continue;
    const auto tag=element->GetTagName();const bool control=tag=="button"||tag=="input"||tag=="select",text=direct_text(element),authored=owners.count(element->GetId())!=0;
    auto at=element->GetAbsoluteOffset(Rml::BoxArea::Border),size=element->GetBox().GetSize(Rml::BoxArea::Border);
    // Inline spans may have one box per wrapped line. The first box alone can
    // fit while later fragments are clipped by the authored parent.
    if(text&&element->GetNumBoxes()>1){const auto origin=at;float x1=at.x,y1=at.y,x2=at.x+size.x,y2=at.y+size.y;
      for(int i=1;i<std::min(element->GetNumBoxes(),4096);++i){Rml::Vector2f offset;const auto extent=element->GetBox(i,offset).GetSize(Rml::BoxArea::Border);x1=std::min(x1,origin.x+offset.x);y1=std::min(y1,origin.y+offset.y);x2=std::max(x2,origin.x+offset.x+extent.x);y2=std::max(y2,origin.y+offset.y+extent.y);}at={x1,y1};size={x2-x1,y2-y1};if(element->GetNumBoxes()>4096)truncated=true;
    }
    if(size.x<=0||size.y<=0||element->GetComputedValues().opacity()<=0)continue;
    const auto overflow_x=element->GetProperty("overflow-x")->ToString(),overflow_y=element->GetProperty("overflow-y")->ToString();
    const bool over_x=element->GetScrollWidth()>element->GetClientWidth()+.5f,over_y=element->GetScrollHeight()>element->GetClientHeight()+.5f;
    const bool scroll_region=(scrolling(overflow_x)&&over_x)||(scrolling(overflow_y)&&over_y);
    if(scroll_region)++scroll_regions;
    if(!control&&!text&&!authored&&!scroll_region)continue;
    ++visible;std::string owner;for(auto* e=element;e;e=e->GetParentNode())if(auto found=owners.find(e->GetId());found!=owners.end()){owner=found->second;break;}
    const std::string id=element->GetId().empty()?owner:element->GetId();
    const Json bounds={{"x",at.x},{"y",at.y},{"width",size.x},{"height",size.y}};
    auto add=[&](const std::string& code,Json evidence){const auto key=code+"|"+owner+"|"+(id=="node-"+owner?owner:id);if(!seen.insert(key).second)return;if(issues.size()>=512){truncated=true;return;}issues.push_back({{"code",code},{"severity","warning"},{"elementId",owner},{"nodeId",id},{"bounds",bounds},{"evidence",evidence}});};
    bool scroll_x=false,scroll_y=false,fixed_clip=false,transparent=false;float left=at.x,top=at.y,right=at.x+size.x,bottom=at.y+size.y;
    for(auto* ancestor=element;ancestor;ancestor=ancestor->GetParentNode()){
      transparent=transparent||ancestor->GetComputedValues().opacity()<.999f;
      if(ancestor==element)continue;
      const auto x=ancestor->GetProperty("overflow-x")->ToString(),y=ancestor->GetProperty("overflow-y")->ToString();
      // Scrolled-off content remains reachable. Do not mistake it for fixed clipping.
      scroll_x=scroll_x||scrolling(x);scroll_y=scroll_y||scrolling(y);
      const auto pos=ancestor->GetAbsoluteOffset(Rml::BoxArea::Padding),extent=ancestor->GetBox().GetSize(Rml::BoxArea::Padding);
      if(x=="hidden"&&!scroll_x){left=std::max(left,pos.x);right=std::min(right,pos.x+extent.x);}
      if(y=="hidden"&&!scroll_y){top=std::max(top,pos.y);bottom=std::min(bottom,pos.y+extent.y);}
    }
    fixed_clip=right-left<size.x-.5f||bottom-top<size.y-.5f;
    const bool outside_x=at.x<-.5f||at.x+size.x>viewport_width+.5f,outside_y=at.y<-.5f||at.y+size.y>viewport_height+.5f;
    if((authored||control)&&((outside_x&&!scroll_x)||(outside_y&&!scroll_y)))add("OUTSIDE_VIEWPORT",{{"viewportWidth",viewport_width},{"viewportHeight",viewport_height}});
    if(scroll_region&&fixed_clip)add("CLIPPED_SCROLL_REGION",{{"visibleWidth",std::max(0.f,right-left)},{"visibleHeight",std::max(0.f,bottom-top)}});
    if(control){++controls;
      if(fixed_clip)add("CLIPPED_CONTROL",{{"visibleWidth",std::max(0.f,right-left)},{"visibleHeight",std::max(0.f,bottom-top)}});
      // Full target size, not the transient portion revealed by a scroller.
      const float width=size.x*points_x,height=size.y*points_y;
      if(width<43.5f||height<43.5f)add("SMALL_TOUCH_TARGET",{{"widthPoints",width},{"heightPoints",height},{"minimumPoints",44}});
    }
    if(text&&(fixed_clip||(overflow_x=="hidden"&&over_x)||(overflow_y=="hidden"&&over_y)))add("CLIPPED_TEXT",{{"fixedAncestorClip",fixed_clip},{"clientWidth",element->GetClientWidth()},{"clientHeight",element->GetClientHeight()},{"scrollWidth",element->GetScrollWidth()},{"scrollHeight",element->GetScrollHeight()}});
    if(!text)continue;
    Json sample={{"elementId",owner},{"nodeId",id},{"measured",false}};
    const auto ink=element->GetProperty<Rml::Colourb>("color");Rml::Colourb background;bool solid=false;std::string reason;
    if(transparent||ink.alpha!=255)reason="transparency";
    else for(auto* ancestor=element;ancestor;ancestor=ancestor->GetParentNode()){
      // A solid-color calculation cannot prove contrast over artwork or effects.
      if(complex_surface[ancestor]){reason="image-decorator-or-layered-surface";break;}
      const auto color=ancestor->GetComputedValues().background_color();
      if(color.alpha==255){background=color;solid=true;break;}
      if(color.alpha!=0){reason="transparency";break;}
    }
    if(solid){const double a=luminance(ink),b=luminance(background),ratio=(std::max(a,b)+.05)/(std::min(a,b)+.05);
      const double font_points=element->GetComputedValues().font_size()*points_y;
      const bool large=font_points>=24||(font_points>=18.6667&&int(element->GetComputedValues().font_weight())>=700);const double minimum=large?3.:4.5;
      sample.update({{"measured",true},{"ratio",ratio},{"minimum",minimum},{"fontPoints",font_points},{"method","computed-opaque-colors"}});
      // Disabled UI is included as coverage, but is not an actionable contrast issue.
      bool disabled=false;for(auto* e=element;e;e=e->GetParentNode())disabled=disabled||e->HasAttribute("disabled");
      sample["disabled"]=disabled;if(!disabled&&ratio+.005<minimum)add("LOW_CONTRAST",sample);
    }else sample["reason"]=reason.empty()?"no-solid-background":reason;
    contrast.push_back(sample);
  }
  size_t measured=0;for(const auto& sample:contrast)if(sample.value("measured",false))++measured;
  return {{"version",1},{"measurement","rmlui-rendered-layout"},{"scope","current-screen-and-state"},{"configuration",configuration},{"issues",issues},{"contrastSamples",contrast},
    {"coverage",{{"visitedNodes",elements.size()},{"visibleCandidates",visible},{"controls",controls},{"scrollRegions",scroll_regions},{"contrastMeasured",measured},{"contrastUnmeasured",contrast.size()-measured},{"truncated",truncated}}},
    {"limits",Json::array({"Current rendered state only; other screens, phases and locales require separate checks.","Scrollable offscreen content is not treated as clipping. Text checks use shaped layout extents, not individual glyph pixels.","Contrast uses computed opaque solid colors; images, decorators, layered surfaces and transparency are unmeasured. Overlap, focus order, screen readers and human usability require acceptance testing.","Touch target guidance is 44 window points; desktop preview is not physical-device acceptance."})}};
}
}
