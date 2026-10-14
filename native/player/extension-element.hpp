#pragma once
// <raising-extension data-instance="screen/element"> draws a mounted extension's items.
// The SDL renderer backend has no SetTransform, so each item becomes a CPU-transformed
// textured quad. Drawing inside the RML tree keeps the authored z-order of the screen.
// Mesh items (capability plugin-mesh-v1, CONTRACT.md §7) arrive as ready-made element-local
// triangles: RmlUi geometry is triangle based, so they are emitted as they are.
#include <RmlUi/Core.h>
#include "extension-host.hpp"

class ExtensionElement : public Rml::Element {
public:
  static inline ExtensionHost* host=nullptr;
  static constexpr size_t MAX_MESH_VERTICES=2048,MAX_MESH_INDICES=3*4096;
  explicit ExtensionElement(const Rml::String& tag):Rml::Element(tag){}
  /** Validated mesh of an item (positions/uvs pairs of the same count, indices in range); false = draw nothing. */
  static bool readMesh(const Json& mesh,Rml::byte alpha,Rml::Mesh& out) {
    if(!mesh.is_object())return false;
    const auto& p=mesh.value("positions",Json::array());const auto& uv=mesh.value("uvs",Json::array());const auto& ix=mesh.value("indices",Json::array());
    if(!p.is_array()||!uv.is_array()||!ix.is_array()||p.size()%2||p.size()!=uv.size()||p.size()/2>MAX_MESH_VERTICES||ix.empty()||ix.size()%3||ix.size()>MAX_MESH_INDICES)return false;
    const size_t n=p.size()/2;
    for(size_t i=0;i<n;i++) {
      if(!p[2*i].is_number()||!p[2*i+1].is_number()||!uv[2*i].is_number()||!uv[2*i+1].is_number())return false;
      out.vertices.push_back({{p[2*i].get<float>(),p[2*i+1].get<float>()},Rml::ColourbPremultiplied(alpha,alpha,alpha,alpha),{uv[2*i].get<float>(),uv[2*i+1].get<float>()}});
    }
    for(const auto& v:ix){if(!v.is_number_integer()||v.get<int64_t>()<0||v.get<int64_t>()>=int64_t(n))return false;out.indices.push_back(v.get<int>());}
    return true;
  }
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
      const float alpha=std::clamp(float(item.value("alpha",1.0))*opacity,0.f,1.f);
      if(!found->second||alpha<=0)continue;
      const auto a=Rml::byte(std::lround(alpha*255));
      if(item.contains("mesh")) {
        // A malformed mesh is skipped, never replaced by its fallback rectangle: that would show the undeformed image.
        Rml::Mesh mesh;
        if(readMesh(item.at("mesh"),a,mesh))render->MakeGeometry(std::move(mesh)).Render(origin,found->second);
        continue;
      }
      const auto& matrix=item.value("matrix",Json::array());
      if(!matrix.is_array()||matrix.size()!=6)continue;
      double m[6];for(int i=0;i<6;i++)m[i]=matrix[i].is_number()?matrix[i].get<double>():0;
      const double w=item.value("width",0.0),h=item.value("height",0.0);
      if(w<=0||h<=0)continue;
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
