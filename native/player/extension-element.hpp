#pragma once
// <raising-extension data-instance="screen/element"> draws a mounted extension's items.
// The SDL renderer backend has no SetTransform, so each item becomes a CPU-transformed
// textured quad. Drawing inside the RML tree keeps the authored z-order of the screen.
// Mesh items (capability plugin-mesh-v1, CONTRACT.md §7) arrive as ready-made element-local
// triangles: RmlUi geometry is triangle based, so they are emitted as they are.
// Atlas items (plugin-atlas-v1) name a page and the sub-image (region) that is their picture:
// their texture coordinates are mapped into that rectangle of the page texture.
// Items are decoded once per frame reply by the host (ExtensionDrawItem): no JSON is read here.
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
      if(!item.valid)continue;
      auto found=textures.find(item.image);
      if(found==textures.end()){const auto path=host->asset_path(item.image);found=textures.emplace(item.image,path.empty()?Rml::Texture():render->LoadTexture(path)).first;}
      const float alpha=std::clamp(float(item.alpha)*opacity,0.f,1.f);
      if(!found->second||alpha<=0)continue;
      const auto a=Rml::byte(std::lround(alpha*255));
      // The picture's 0..1 texture space: the whole image, or its atlas sub-image of the page.
      float u0=0,v0=0,su=1,sv=1;
      if(item.region) {
        const auto size=found->second.GetDimensions();
        if(size.x<=0||size.y<=0)continue;
        u0=float(item.r[0]/size.x);v0=float(item.r[1]/size.y);su=float(item.r[2]/size.x);sv=float(item.r[3]/size.y);
      }
      if(item.mesh) {
        // A malformed mesh is skipped, never replaced by its fallback rectangle: that would show the undeformed image.
        if(!item.drawable_mesh())continue;
        const auto& t=*item.topology;const size_t n=t.uvs.size()/2;
        Rml::Mesh mesh;mesh.vertices.reserve(n);
        for(size_t i=0;i<n;i++)mesh.vertices.push_back({{float(item.positions[2*i]),float(item.positions[2*i+1])},Rml::ColourbPremultiplied(a,a,a,a),{u0+su*float(t.uvs[2*i]),v0+sv*float(t.uvs[2*i+1])}});
        mesh.indices.assign(t.indices.begin(),t.indices.end());
        render->MakeGeometry(std::move(mesh)).Render(origin,found->second);
        continue;
      }
      const double w=item.w,h=item.h;const double* m=item.m;
      if(w<=0||h<=0)continue;
      Rml::Mesh mesh;
      const double corners[4][2]={{0,0},{w,0},{w,h},{0,h}};const float uv[4][2]={{u0,v0},{u0+su,v0},{u0+su,v0+sv},{u0,v0+sv}};
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
