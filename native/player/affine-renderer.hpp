#pragma once
#include "RmlUi_Renderer_SDL.h"
#include <RmlUi/Core/Matrix4.h>
#include <RmlUi/Core/Vertex.h>
#include <vector>

// Upstream SDL backend ignores SetTransform. Apply RmlUi's transform to geometry
// so plugin quads and authored CSS transforms use the same coordinate system.
class AffineSDLRenderer : public RenderInterface_SDL {
  struct Geometry {std::vector<Rml::Vertex> vertices;std::vector<int> indices;};
  SDL_Renderer* renderer_;
  Rml::Matrix4f transform_=Rml::Matrix4f::Identity();
public:
  explicit AffineSDLRenderer(SDL_Renderer* renderer):RenderInterface_SDL(renderer),renderer_(renderer){}
  void SetTransform(const Rml::Matrix4f* transform) override {transform_=transform?*transform:Rml::Matrix4f::Identity();}
  Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,Rml::Span<const int> indices) override {
    return reinterpret_cast<Rml::CompiledGeometryHandle>(new Geometry{{vertices.begin(),vertices.end()},{indices.begin(),indices.end()}});
  }
  void ReleaseGeometry(Rml::CompiledGeometryHandle handle) override {delete reinterpret_cast<Geometry*>(handle);}
  void RenderGeometry(Rml::CompiledGeometryHandle handle,Rml::Vector2f translation,Rml::TextureHandle texture) override {
    const auto& geometry=*reinterpret_cast<Geometry*>(handle);std::vector<SDL_Vertex> vertices;vertices.reserve(geometry.vertices.size());
    for(const auto& v:geometry.vertices) {
      const auto point=transform_*Rml::Vector4f(v.position.x+translation.x,v.position.y+translation.y,0,1);
      const float w=point.w==0?1:point.w;
      vertices.push_back({{point.x/w,point.y/w},{v.colour.red/255.f,v.colour.green/255.f,v.colour.blue/255.f,v.colour.alpha/255.f},{v.tex_coord.x,v.tex_coord.y}});
    }
    SDL_RenderGeometry(renderer_,reinterpret_cast<SDL_Texture*>(texture),vertices.data(),int(vertices.size()),geometry.indices.data(),int(geometry.indices.size()));
  }
};
