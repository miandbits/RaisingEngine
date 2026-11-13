#pragma once
#include "session.hpp"
/** Application branding is host metadata; authored game title bindings remain unchanged. */
inline std::string application_title(const Json& bundle){
 const auto fallback=bundle.at("content").at("project").value("title","Raising Player");
 if(!bundle.contains("application"))return fallback;
 const auto& metadata=bundle.at("application");
 if(!metadata.is_object()||metadata.size()!=1||!metadata.contains("displayName")||!metadata.at("displayName").is_string())throw std::runtime_error("Invalid application display name");
 const auto name=metadata.at("displayName").get<std::string>();size_t characters=0;
 for(const unsigned char c:name){if(c<32||c==127)throw std::runtime_error("Invalid application display name");if((c&0xc0)!=0x80)characters+=c>=0xf0?2:1;}
 if(name.empty()||characters>80||name.find_first_not_of(' ')==std::string::npos)throw std::runtime_error("Invalid application display name");return name;
}
