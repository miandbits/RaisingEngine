#pragma once
// Strict transport boundary for portable imports. Existing local-save readers are unchanged.
#include "json.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <istream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace raising_player::portable_input {
inline constexpr std::size_t max_bytes = 16 * 1024 * 1024;
inline constexpr std::size_t max_depth = 128;
enum class Code { unreadable, too_large, invalid_utf8, invalid_json, duplicate_key, too_deep, resource_limit };
inline const char* code_name(Code code) {
  switch (code) {
    case Code::unreadable:return "unreadable";
    case Code::too_large:return "too-large";
    case Code::invalid_utf8:return "invalid-utf8";
    case Code::invalid_json:return "invalid-json";
    case Code::duplicate_key:return "duplicate-key";
    case Code::too_deep:return "too-deep";
    case Code::resource_limit:return "resource-limit";
  }
  return "unreadable";
}
// Host UI maps codes to localized fixed messages. Never include a URL, key or token.
class Error final : public std::runtime_error {
 public:
  const Code code;
  explicit Error(Code value):std::runtime_error(code_name(value)),code(value){}
};
inline void require_depth(std::size_t depth) { if(depth > max_depth) throw Error(Code::too_deep); }
inline bool valid_utf8(std::string_view bytes) {
  for(std::size_t i=0;i<bytes.size();) {
    const auto a=static_cast<unsigned char>(bytes[i++]);
    if(a<0x80)continue;
    std::size_t tail=0;unsigned int point=0,minimum=0;
    if(a>=0xc2&&a<=0xdf){tail=1;point=a&0x1f;minimum=0x80;}
    else if(a>=0xe0&&a<=0xef){tail=2;point=a&0x0f;minimum=0x800;}
    else if(a>=0xf0&&a<=0xf4){tail=3;point=a&0x07;minimum=0x10000;}
    else return false;
    if(tail>bytes.size()-i)return false;
    while(tail--){const auto b=static_cast<unsigned char>(bytes[i++]);if((b&0xc0)!=0x80)return false;point=(point<<6)|(b&0x3f);}
    if(point<minimum||point>0x10ffff||(point>=0xd800&&point<=0xdfff))return false;
  }
  return true;
}
inline std::string bounded_read(std::istream& stream) {
  try {
    std::string result;result.reserve(64*1024);std::array<char,64*1024> chunk{};
    for(;;) {
      const auto wanted=std::min(chunk.size(),max_bytes-result.size()+1);
      stream.read(chunk.data(),static_cast<std::streamsize>(wanted));
      const auto got=static_cast<std::size_t>(stream.gcount());
      if(got>max_bytes-result.size())throw Error(Code::too_large);
      result.append(chunk.data(),got);
      if(stream.bad())throw Error(Code::unreadable);
      if(stream.eof())break;
      if(stream.fail()||got!=wanted)throw Error(Code::unreadable);
    }
    return result;
  } catch(const Error&) {throw;}
    catch(const std::bad_alloc&) {throw Error(Code::resource_limit);}
    catch(const std::exception&) {throw Error(Code::unreadable);}
}
inline nlohmann::json parse(std::string_view bytes) {
  using Json=nlohmann::json;
  try {
    if(bytes.size()>max_bytes)throw Error(Code::too_large);
    if(!valid_utf8(bytes))throw Error(Code::invalid_utf8);
    // Native's lexer can treat raw NUL as EOF. JSON never permits raw NUL,
    // including after a complete value; escaped \u0000 remains valid.
    if(bytes.find('\0')!=std::string_view::npos)throw Error(Code::invalid_json);
    // File.text() strips one UTF-8 BOM. The existing JSON lexer accepts
    // exactly one leading BOM; leave it here so a second BOM stays invalid.
    struct Frame {bool object;std::size_t depth;std::set<std::string> keys;};
    std::vector<Frame> frames;
    auto next_depth=[&](){return frames.empty()?0:frames.back().depth+(frames.back().object?2:1);};
    auto callback=[&](int,Json::parse_event_t event,Json& value){
      switch(event) {
        case Json::parse_event_t::object_start:
        case Json::parse_event_t::array_start: {
          const auto depth=next_depth();require_depth(depth);
          frames.push_back({event==Json::parse_event_t::object_start,depth,{}});break;
        }
        case Json::parse_event_t::key:
          require_depth(frames.back().depth+2);
          if(!frames.back().keys.insert(value.get<std::string>()).second)throw Error(Code::duplicate_key);
          break;
        case Json::parse_event_t::value:require_depth(next_depth());break;
        case Json::parse_event_t::object_end:
        case Json::parse_event_t::array_end:frames.pop_back();break;
      }
      return true;
    };
    return Json::parse(bytes.begin(),bytes.end(),callback,true,false);
  } catch(const Error&) {throw;}
    catch(const std::bad_alloc&) {throw Error(Code::resource_limit);}
    catch(const Json::exception&) {throw Error(Code::invalid_json);}
}
inline nlohmann::json read_staged_file(const std::filesystem::path& path) {
  // Only an app-owned copied/staged file, never a provider URL. The UIKit
  // adapter owns provider coordination and the immutable staging lifetime.
  try {
    std::error_code ec;
    if(!std::filesystem::is_regular_file(path,ec)||ec)throw Error(Code::unreadable);
    const auto advertised=std::filesystem::file_size(path,ec);
    if(ec)throw Error(Code::unreadable);
    if(advertised>max_bytes)throw Error(Code::too_large);
    std::ifstream input(path,std::ios::binary);
    if(!input)throw Error(Code::unreadable);
    return parse(bounded_read(input)); // Independently enforces the bound if the file grew.
  } catch(const Error&) {throw;}
    catch(const std::bad_alloc&) {throw Error(Code::resource_limit);}
    catch(const std::exception&) {throw Error(Code::unreadable);}
}
}
