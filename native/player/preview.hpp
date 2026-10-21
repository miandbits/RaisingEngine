#pragma once
#include <RmlUi/Core/ElementText.h>
#include "json.hpp"

// RmlUi text fragments do not expose their line boxes publicly. Its scroll extent
// includes the real shaped/wrapped text layout, including overflow:hidden content.
inline bool contains_rendered_text(Rml::Element* element) {
  if(auto* text=dynamic_cast<Rml::ElementText*>(element);text&&!text->GetText().empty())return true;
  for(int i=0;i<element->GetNumChildren();++i)if(contains_rendered_text(element->GetChild(i)))return true;
  return false;
}

inline Json rendered_node_report(const Json& entry,Rml::Element* element,int viewport_width,int viewport_height,Rml::Context* context) {
  const auto id=entry.value("id","");
  Json result={{"id",id},{"kind",entry.value("kind","")},{"rendered",element!=nullptr}};
  if(entry.contains("ownerId"))result["ownerId"]=entry.at("ownerId");
  if(entry.contains("label"))result["label"]=entry.at("label");
  if(!element){result["visible"]=false;return result;}
  const auto point=element->GetAbsoluteOffset(Rml::BoxArea::Border),size=element->GetBox().GetSize(Rml::BoxArea::Border);
  result.update({{"x",point.x},{"y",point.y},{"width",size.x},{"height",size.y},{"visible",element->IsVisible(true)},
    {"outsideViewport",point.x<-.5f||point.y<-.5f||point.x+size.x>viewport_width+.5f||point.y+size.y>viewport_height+.5f}});
  result["fontFamily"]=element->GetProperty("font-family")->ToString();
  result["fontSize"]=element->GetProperty("font-size")->ToString();
  result["color"]=element->GetProperty("color")->ToString();
  result["opacity"]=element->GetProperty<float>("opacity");
  const bool has_text=contains_rendered_text(element);
  const bool overflow_x=element->GetScrollWidth()>element->GetClientWidth()+.5f,overflow_y=element->GetScrollHeight()>element->GetClientHeight()+.5f;
  result["overflowX"]=overflow_x;result["overflowY"]=overflow_y;result["textMeasured"]=has_text;
  result["measurement"]="rmlui-scroll-layout";
  result["clientWidth"]=element->GetClientWidth();result["clientHeight"]=element->GetClientHeight();
  const auto overflow=element->GetProperty("overflow-x")->ToString(),overflow_y_mode=element->GetProperty("overflow-y")->ToString();
  result["clipped"]=(overflow=="hidden"&&overflow_x)||(overflow_y_mode=="hidden"&&overflow_y);
  result["scrollWidth"]=element->GetScrollWidth();result["scrollHeight"]=element->GetScrollHeight();
  auto* button=element->GetTagName()=="button"?element:entry.value("kind","")=="button"?element->QuerySelector("button"):nullptr;
  if(button) {
    const auto position=button->GetAbsoluteOffset(Rml::BoxArea::Border),extent=button->GetBox().GetSize(Rml::BoxArea::Border);
    result["buttonId"]=button->GetId();result["enabled"]=!button->HasAttribute("disabled");
    result["hitbox"]={{"x",position.x},{"y",position.y},{"width",extent.x},{"height",extent.y}};
    float hit_left=std::max(0.f,position.x),hit_top=std::max(0.f,position.y),
      hit_right=std::min(float(viewport_width),position.x+extent.x),hit_bottom=std::min(float(viewport_height),position.y+extent.y);
    for(auto* ancestor=button->GetParentNode();ancestor;ancestor=ancestor->GetParentNode()) {
      const auto clip=ancestor->GetAbsoluteOffset(Rml::BoxArea::Padding),bounds=ancestor->GetBox().GetSize(Rml::BoxArea::Padding);
      if(ancestor->GetProperty("overflow-x")->ToString()!="visible"){hit_left=std::max(hit_left,clip.x);hit_right=std::min(hit_right,clip.x+bounds.x);}
      if(ancestor->GetProperty("overflow-y")->ToString()!="visible"){hit_top=std::max(hit_top,clip.y);hit_bottom=std::min(hit_bottom,clip.y+bounds.y);}
    }
    result["visibleHitbox"]={{"x",hit_left},{"y",hit_top},{"width",std::max(0.f,hit_right-hit_left)},{"height",std::max(0.f,hit_bottom-hit_top)}};
    auto* hit=context->GetElementAtPoint({position.x+extent.x/2,position.y+extent.y/2});
    bool target_hit=false;for(auto* ancestor=hit;ancestor;ancestor=ancestor->GetParentNode())if(ancestor==button){target_hit=true;break;}
    result["hitTestedId"]=hit?hit->GetId():"";result["occludedAtCenter"]=!target_hit;
    result["clickableAtCenter"]=target_hit&&!button->HasAttribute("disabled")&&button->IsVisible(true);
    result["backgroundColor"]=button->GetProperty("background-color")->ToString();
    result["color"]=button->GetProperty("color")->ToString();
    result["fontFamily"]=button->GetProperty("font-family")->ToString();
    result["fontSize"]=button->GetProperty("font-size")->ToString();
  }
  return result;
}
