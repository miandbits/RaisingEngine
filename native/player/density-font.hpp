#pragma once

#include "vendor/RmlUi/Source/Core/FontEngineDefault/FontEngineInterfaceDefault.h"
#include <RmlUi/Core/Mesh.h>
#include <RmlUi/Core/TextShapingContext.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

// Keep authored font metrics (wrapping, hit boxes and saved UI positions) while
// rasterizing glyphs for the output display. SDL already draws geometry/images
// at output resolution; only the font atlas needs higher density.
class DensityFontEngine : public Rml::FontEngineInterfaceDefault {
  using Base = Rml::FontEngineInterfaceDefault;
  struct Face {
    Rml::String family;
    Rml::Style::FontStyle style;
    Rml::Style::FontWeight weight;
    int size = 0, density = 0, version = 0, low_version = -1, high_version = -1;
    Rml::FontFaceHandle high = 0;
  };
  std::map<Rml::FontFaceHandle, Face> faces;
  int density = 1;

  void prepare(Face& face) {
    if (face.density == density) return;
    face.high = density > 1 ? Base::GetFontFaceHandle(face.family, face.style, face.weight, face.size * density) : 0;
    face.density = density;
    ++face.version;
  }

public:
  void set_output_scale(float scale) {
    density = std::isfinite(scale) ? std::clamp(int(std::ceil(scale - 0.01f)), 1, 4) : 1;
  }
  int raster_density() const { return density; }

  Rml::FontFaceHandle GetFontFaceHandle(const Rml::String& family, Rml::Style::FontStyle style,
      Rml::Style::FontWeight weight, int size) override {
    const auto handle = Base::GetFontFaceHandle(family, style, weight, size);
    if (handle && !faces.count(handle)) faces.emplace(handle, Face{family, style, weight, size});
    return handle;
  }

  int GetVersion(Rml::FontFaceHandle handle) override {
    auto found = faces.find(handle);
    if (found == faces.end()) return Base::GetVersion(handle);
    auto& face = found->second;
    prepare(face);
    const int low = Base::GetVersion(handle), high = face.high ? Base::GetVersion(face.high) : -1;
    if (low != face.low_version || high != face.high_version) {
      face.low_version = low; face.high_version = high; ++face.version;
    }
    return face.version;
  }

  int GenerateString(Rml::RenderManager& render_manager, Rml::FontFaceHandle handle,
      Rml::FontEffectsHandle effects, Rml::StringView text, Rml::Vector2f position,
      Rml::ColourbPremultiplied colour, float opacity, const Rml::TextShapingContext& shaping,
      Rml::TexturedMeshList& meshes) override {
    auto found = faces.find(handle);
    // Effect handles belong to a particular face. Retain RmlUi's effect path.
    if (density == 1 || effects || found == faces.end())
      return Base::GenerateString(render_manager, handle, effects, text, position, colour, opacity, shaping, meshes);
    auto& face = found->second;
    prepare(face);
    if (!face.high)
      return Base::GenerateString(render_manager, handle, effects, text, position, colour, opacity, shaping, meshes);
    const int logical_width = Base::GetStringWidth(handle, text, shaping, Rml::Character::Null);
    auto high_shaping = shaping;
    high_shaping.letter_spacing *= density;
    // Populate the raster face before GenerateString rebuilds its atlas. A
    // previously unseen CJK glyph appended during generation otherwise has no
    // geometry until another string happens to dirty this face.
    Base::GetStringWidth(face.high, text, high_shaping, Rml::Character::Null);
    // Wrapped lines append to the same atlas meshes. Earlier glyphs are already
    // in logical coordinates and must not be translated/scaled a second time.
    std::vector<size_t> previous_vertices;
    previous_vertices.reserve(meshes.size());
    for (const auto& textured : meshes) previous_vertices.push_back(textured.mesh.vertices.size());
    const int raster_width = Base::GenerateString(render_manager, face.high, 0, text, {}, colour, opacity, high_shaping, meshes);
    // FreeType rounds advances to pixels at each size. Match the original line
    // advance so a DPI change cannot move text into adjacent controls.
    const float x_scale = raster_width > 0 ? float(logical_width) / raster_width : 1.f / density;
    for (size_t mesh_index = 0; mesh_index < meshes.size(); ++mesh_index) {
      auto& vertices = meshes[mesh_index].mesh.vertices;
      const size_t first = mesh_index < previous_vertices.size() ? previous_vertices[mesh_index] : 0;
      for (size_t vertex_index = first; vertex_index < vertices.size(); ++vertex_index) {
        auto& vertex = vertices[vertex_index];
        vertex.position.x = position.x + vertex.position.x * x_scale;
        vertex.position.y = position.y + vertex.position.y / density;
      }
    }
    return logical_width;
  }

  void ReleaseFontResources() override { faces.clear(); Base::ReleaseFontResources(); }
};
