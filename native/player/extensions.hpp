#pragma once
#include <SDL3/SDL.h>
#include <functional>
#include <memory>
#include "session.hpp"

// Renderer-neutral raising-extension-1 ABI. No cutout/Spine types cross this boundary.
class RuntimeExtensions {
  struct Module {
    SDL_SharedObject* library=nullptr;
    using Request=const char* (*)(const char*);
    Request request=nullptr;
    Json documents;
    ~Module(){if(library)SDL_UnloadObject(library);}
    Json call(const Json& value) const {
      const char* reply=request(value.dump().c_str());
      if(!reply)throw std::runtime_error("Plugin returned an empty reply");
      auto result=Json::parse(reply);
      if(!result.value("ok",false))throw std::runtime_error(result.value("error","Plugin request failed"));
      return result;
    }
  };
  std::map<std::string,std::unique_ptr<Module>> modules;
public:
  std::vector<std::string> errors;
  Json observations=Json::array();
  void initialize(const Json& bundle,const fs::path& root) {
    if(!bundle.contains("extensions"))return;
    if(bundle.at("extensions").value("version",0)!=1)throw std::runtime_error("Unsupported runtime extensions version");
    for(const auto& spec:bundle.at("extensions").at("plugins")) {
      const auto id=spec.at("id").get<std::string>();
      if(spec.value("abi","")!="raising-extension-1"||modules.count(id))throw std::runtime_error("Invalid plugin ABI or duplicate plugin: "+id);
      const auto relative=fs::u8path(spec.at("entry").get<std::string>());
      if(relative.is_absolute())throw std::runtime_error("Absolute plugin path rejected");
      for(const auto& part:relative)if(part=="..")throw std::runtime_error("Plugin path escapes bundle");
      const auto file=fs::weakly_canonical(root/relative),base=fs::weakly_canonical(root);
      const auto checked=fs::relative(file,base);for(const auto& part:checked)if(part=="..")throw std::runtime_error("Plugin link escapes bundle");
      auto module=std::make_unique<Module>();module->documents=spec.at("documents");
      module->library=SDL_LoadObject(file.u8string().c_str());
      if(!module->library)throw std::runtime_error("Cannot load plugin "+id+": "+SDL_GetError());
      const auto version=reinterpret_cast<int(*)()>(SDL_LoadFunction(module->library,"raising_extension_api_version"));
      module->request=reinterpret_cast<Module::Request>(SDL_LoadFunction(module->library,"raising_extension_request"));
      if(!version||!module->request||version()!=1)throw std::runtime_error("Incompatible plugin ABI: "+id);
      const auto response=module->call({{"op","init"},{"documents",module->documents}});
      if(!response.value("diagnostics",Json::array()).empty())throw std::runtime_error("Plugin validation failed: "+response.at("diagnostics").dump());
      modules[id]=std::move(module);
    }
  }
  bool contains(const std::string& id) const{return modules.count(id)!=0;}
  void mount(const std::string& instance,const Json& e) {
    const auto extension=e.at("extension");const auto id=extension.at("plugin").get<std::string>();
    if(!contains(id))return;
    auto element=extension;element["width"]=e.at("width");element["height"]=e.at("height");
    modules.at(id)->call({{"op","mount"},{"instance",instance},{"element",element}});
  }
  Json frame(const std::string& id,const std::string& instance) const {
    return modules.at(id)->call({{"op","frame"},{"instance",instance}}).at("items");
  }
  void update(double dt){for(auto& [id,m]:modules)m->call({{"op","update"},{"dtMs",std::clamp(dt,0.0,250.0)}});}
  void observe(const Json& before,const Json& after,const Json& command,const std::string& error) {
    for(auto& [id,m]:modules) {
      const auto result=m->call({{"op","observe"},{"before",before},{"after",after},{"command",command},{"error",error}});
      observations.push_back({{"plugin",id},{"command",command},{"signals",result.value("signals",Json::array())},{"matched",result.value("matched",Json::array())}});
      if(observations.size()>64)observations.erase(observations.begin());
    }
  }
  Json save(Json preserved) const {
    if(!preserved.is_object())preserved=Json::object();
    for(auto& [id,m]:modules)preserved[id]=m->call({{"op","saveAll"}}).at("state");
    return preserved;
  }
  void restore(const Json& state) {
    for(auto& [id,m]:modules)if(state.contains(id))try{m->call({{"op","restoreAll"},{"state",state.at(id)}});}catch(const std::exception& e){errors.push_back(id+": "+e.what());}
  }
  void reset(){for(auto& [id,m]:modules)m->call({{"op","init"},{"documents",m->documents}});observations=Json::array();}
  Json describe() const {Json result=Json::object();for(auto& [id,m]:modules)result[id]=m->call({{"op","describeAll"}}).at("instances");return result;}
};
