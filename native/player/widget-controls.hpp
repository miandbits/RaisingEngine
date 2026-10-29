#pragma once
#include <string>

// Legacy screen widgets keep their public control IDs. Reusable widget controls
// belong to the fully expanded instance path, including nested instances.
inline std::string widget_control_id(const std::string& owner,const std::string& id) {
  const auto suffix="-"+owner;
  const bool owned=id==owner||id.rfind(owner+"__",0)==0||(id.size()>=suffix.size()&&id.compare(id.size()-suffix.size(),suffix.size(),suffix)==0);
  if(owner.find("__")==std::string::npos||owned)return id;
  return owner+"__"+id;
}
inline std::string scope_widget_controls(std::string body,const std::string& owner) {
  if(owner.find("__")==std::string::npos)return body;
  const std::string marker=" id=\"";
  for(size_t offset=0;(offset=body.find(marker,offset))!=std::string::npos;) {
    const auto begin=offset+marker.size(),end=body.find('"',begin);
    if(end==std::string::npos)break;
    const auto id=widget_control_id(owner,body.substr(begin,end-begin));
    body.replace(begin,end-begin,id);offset=begin+id.size()+1;
  }
  return body;
}
