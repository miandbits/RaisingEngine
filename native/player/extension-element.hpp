#pragma once
// <raising-extension data-instance="screen/element"> draws a mounted extension's items.
// The SDL renderer backend has no SetTransform, so each item becomes a CPU-transformed
// textured quad. Drawing inside the RML tree keeps the authored z-order of the screen.
#include <RmlUi/Core.h>
#include "extension-host.hpp"

class ExtensionElement : public Rml::Element {
public:
  static inline ExtensionHost* host=nullptr;
  explicit ExtensionElement(const Rml::String& tag):Rml::Element(tag){}
protected:
  void OnRender() override {
    if(!host)return;
    const auto* mount=host->find(GetAttribute<Rml::String>("data-instance",""));
    auto* render=GetRenderManager();
    if(!mount||!mount->mounted||!render||!host->asset_path)return;
    const auto origin=GetAbsoluteOffset(Rml::BoxArea::Content);
    const float opacity=GetComputedValues().opacity();
    for(const auto& item:mount->items) {
      const auto id=item.value("imageAssetId","");auto found=textures.find(id);
      if(found==textures.end()){const auto path=host->asset_path(id);found=textures.emplace(id,path.empty()?Rml::Texture():render->LoadTexture(path)).first;}
      const auto& matrix=item.value("matrix",Json::array());
      if(!found->second||!matrix.is_array()||matrix.size()!=6)continue;
      double m[6];for(int i=0;i<6;i++)m[i]=matrix[i].is_number()?matrix[i].get<double>():0;
      const double w=item.value("width",0.0),h=item.value("height",0.0);
      const float alpha=std::clamp(float(item.value("alpha",1.0))*opacity,0.f,1.f);
      if(w<=0||h<=0||alpha<=0)continue;
      const auto a=Rml::byte(std::lround(alpha*255));
      Rml::Mesh mesh;
      const double corners[4][2]={{0,0},{w,0},{w,h},{0,h}};const float uv[4][2]={{0,0},{1,0},{1,1},{0,1}};
      for(int i=0;i<4;i++) {
        const double x=corners[i][0],y=corners[i][1];
        mesh.vertices.push_back({{float(m[0]*x+m[2]*y+m[4]),float(m[1]*x+m[3]*y+m[5])},Rml::ColourbPremultiplied(a,a,a,a),{uv[i][0],uv[i][1]}});
      }
      mesh.indices={0,1,2,0,2,3};
      render->MakeGeometry(std::move(mesh)).Render(origin,found->second);
    }
  }
private:
  std::map<std::string,Rml::Texture> textures;
};
