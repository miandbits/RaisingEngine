#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <set>

// Texture metadata only. The validated exporter checks the complete PNG; RmlUi's
// image backend decodes its pixels. Keep this independent of SDL for markup tests.
namespace SurfacePaint {
struct Size { unsigned width=0,height=0; explicit operator bool() const {return width&&height;} };
inline uint32_t png_u32(const unsigned char* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
inline uint32_t crc32(const unsigned char* p,size_t n){uint32_t value=0xffffffffu;for(size_t i=0;i<n;++i){value^=p[i];for(int bit=0;bit<8;++bit)value=value&1?(value>>1)^0xedb88320u:value>>1;}return value^0xffffffffu;}
inline Size png_size(const fs::path& path){
  try {
    if(!fs::is_regular_file(path)||fs::file_size(path)<45||fs::file_size(path)>20*1024*1024)return {};
    std::array<unsigned char,33> b{};std::ifstream input(path,std::ios::binary);if(!input.read(reinterpret_cast<char*>(b.data()),b.size()))return {};
    const std::array<unsigned char,8> signature{137,80,78,71,13,10,26,10};
    if(!std::equal(signature.begin(),signature.end(),b.begin())||png_u32(b.data()+8)!=13||std::string(reinterpret_cast<char*>(b.data()+12),4)!="IHDR"||crc32(b.data()+12,17)!=png_u32(b.data()+29))return {};
    Size size{png_u32(b.data()+16),png_u32(b.data()+20)};
    if(!size||size.width>8192||size.height>8192||uint64_t(size.width)*size.height>32*1024*1024)return {};
    return size;
  }catch(...){return {};}
}
inline bool active(const Json& surface){return surface.is_object()&&(surface.value("mode","")=="tile"||surface.value("mode","")=="nine-slice")&&surface.contains("imageAssetId")&&surface.at("imageAssetId").is_string();}
inline std::string marker(const Json& surface){return active(surface)?" data-engine-surface=\"image\"":"";}
}
