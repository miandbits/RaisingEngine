#pragma once
// Runtime extension host (ABI raising-extension-1, packages/plugin-host/CONTRACT.md §5-§8).
// Plugins are presentation only: they observe committed transitions and return draw items.
// Nothing here can reach GameSession state, so animation can never change game results.
#include "session.hpp"
#include <SDL3/SDL_loadso.h>
#include <deque>
#include <functional>
#include <map>
#include <set>

struct ExtensionPlugin {
  std::string id,version,abi,entry,error;
  Json documents=Json::array(),diagnostics=Json::array();
  bool available=false;
  SDL_SharedObject* module=nullptr;
  const char* (*request)(const char*)=nullptr;
};
/** Sound of an `event` notification that names an audio asset (CONTRACT.md §7 event payloads). */
struct ExtensionSound { std::string asset; double volume=1,balance=0; };
struct ExtensionMount {
  std::string instance,plugin,error;
  Json element=nullptr,items=Json::array();
  bool mounted=false,seen=false;
};

class ExtensionHost {
public:
  std::vector<ExtensionPlugin> plugins;
  std::map<std::string,ExtensionMount> mounts;
  // Saved states of plugins that are missing in this build survive load→save round trips.
  Json preserved=Json::object(),pending_restore=nullptr,last_observe=nullptr;
  std::function<std::string(const std::string&)> asset_path;
  uint64_t observations=0;double clock_ms=0;
  // `event` notifications of update replies: the newest ones for automation reports and the sounds
  // they name, for the player's audio. Presentation only: none of it reaches the game session.
  static constexpr size_t MAX_RECENT_EVENTS=32,MAX_PENDING_SOUNDS=64;
  std::deque<Json> recent_events;uint64_t event_count=0,sound_count=0;
  std::vector<ExtensionSound> sounds;
  ExtensionHost()=default;
  ExtensionHost(const ExtensionHost&)=delete;
  ExtensionHost& operator=(const ExtensionHost&)=delete;
  ~ExtensionHost(){for(auto& p:plugins)if(p.module)SDL_UnloadObject(p.module);}
  bool active() const {return !plugins.empty();}
  ExtensionPlugin* plugin(const std::string& id){for(auto& p:plugins)if(p.id==id)return &p;return nullptr;}
  const ExtensionMount* find(const std::string& instance) const {auto it=mounts.find(instance);return it==mounts.end()?nullptr:&it->second;}

  /** Reads game.json `extensions`. Every failure degrades to placeholder mode; the game still runs. */
  void load(const Json& bundle,const fs::path& root) {
    if(!bundle.contains("extensions")||!bundle.at("extensions").is_object())return;
    const auto& config=bundle.at("extensions");
    if(config.value("version",0)!=1||!config.contains("plugins")||!config.at("plugins").is_array())return;
    for(const auto& entry:config.at("plugins")) {
      if(!entry.is_object()||!entry.contains("id")||!entry.at("id").is_string()||plugin(entry.at("id").get<std::string>()))continue;
      ExtensionPlugin p;p.id=entry.at("id").get<std::string>();p.version=entry.value("version","");p.abi=entry.value("abi","");p.entry=entry.value("entry","");
      p.documents=entry.contains("documents")&&entry.at("documents").is_array()?entry.at("documents"):Json::array();
      plugins.push_back(std::move(p));open(plugins.back(),root);
    }
  }
  /** Re-initializes every plugin (new game / load) and re-mounts the instances currently on screen. */
  void reset() {
    preserved=Json::object();pending_restore=nullptr;
    for(auto& p:plugins)if(p.available)init(p);
    for(auto& [id,m]:mounts){m.mounted=false;m.items=Json::array();if(!m.element.is_null())mount_request(m);}
  }
  void begin_sync(){for(auto& [id,m]:mounts)m.seen=false;}
  /** Mounts (or keeps) one rendered plugin element. Returns false when it must show a placeholder. */
  bool mount(const std::string& instance,const Json& element) {
    const auto ext=element.value("extension",Json::object());
    auto& m=mounts[instance];m.instance=instance;m.seen=true;
    const Json spec={{"type",ext.value("type","")},{"ref",ext.value("ref","")},{"width",element.value("width",0.0)},{"height",element.value("height",0.0)},{"props",ext.value("props",Json::object())}};
    const auto plugin_id=ext.value("plugin","");
    if(m.plugin!=plugin_id&&m.mounted)unmount(m);
    m.plugin=plugin_id;
    if(m.mounted&&m.element==spec)return true;
    m.element=spec;return mount_request(m);
  }
  void end_sync() {
    for(auto it=mounts.begin();it!=mounts.end();) {
      if(it->second.seen){++it;continue;}
      unmount(it->second);it=mounts.erase(it);
    }
    if(!pending_restore.is_null()){const auto saved=pending_restore;pending_restore=nullptr;restore(saved);}
  }
  /** Called exactly once per submitted command, after its reply, never from frames. */
  void observe(const Json& before,const Json& after,const Json& command,const std::string& error) {
    if(!active())return;
    ++observations;Json replies=Json::object();
    for(auto& p:plugins)if(p.available) {
      Json request={{"op","observe"},{"before",before},{"after",after},{"command",command},{"error",error.empty()?Json():Json(error)}};
      const auto reply=call(p,request);replies[p.id]={{"signals",reply.value("signals",Json::array())},{"matched",reply.value("matched",Json::array())}};
    }
    last_observe={{"command",command},{"error",error.empty()?Json():Json(error)},{"plugins",replies}};
  }
  /** Presentation clock only; notifications are deliberately ignored for gameplay. */
  void update(double dt_ms) {
    if(!active())return;
    dt_ms=std::clamp(dt_ms,0.0,10000.0);clock_ms+=dt_ms;
    for(auto& p:plugins)if(p.available)collect_events(p.id,call(p,{{"op","update"},{"dtMs",dt_ms}}));
  }
  /** Sounds of the events fired since the last call (the player plays them unless muted). */
  std::vector<ExtensionSound> take_sounds(){std::vector<ExtensionSound> out;out.swap(sounds);return out;}
  void frame() {
    for(auto& [id,m]:mounts) {
      if(!m.mounted)continue;auto* p=plugin(m.plugin);if(!p||!p->available){m.items=Json::array();continue;}
      const auto reply=call(*p,{{"op","frame"},{"instance",id}});
      m.items=reply.value("ok",false)&&reply.contains("items")&&reply.at("items").is_array()?reply.at("items"):Json::array();
    }
  }
  /** playerPresentation.extensions value, or null when no extension is in use. */
  Json save() {
    if(!active())return nullptr;
    Json out=preserved;
    for(auto& p:plugins)if(p.available) {const auto reply=call(p,{{"op","saveAll"}});if(reply.value("ok",false)&&reply.contains("state"))out[p.id]=reply.at("state");}
    return out.empty()?Json():out;
  }
  void restore(const Json& saved) {
    if(!saved.is_object())return;
    for(const auto& [id,state]:saved.items()) {
      auto* p=plugin(id);
      if(!p||!p->available){preserved[id]=state;continue;}
      call(*p,{{"op","restoreAll"},{"state",state}});
    }
  }
  Json report() {
    Json list=Json::array(),plugin_list=Json::array();std::map<std::string,Json> described;
    for(auto& p:plugins) {
      plugin_list.push_back({{"id",p.id},{"version",p.version},{"abi",p.abi},{"available",p.available},{"error",p.error},{"diagnostics",p.diagnostics}});
      if(p.available){const auto reply=call(p,{{"op","describeAll"}});described[p.id]=reply.value("instances",Json::object());}
    }
    for(const auto& [id,m]:mounts) {
      Json items=Json::array();
      for(const auto& item:m.items) {
        items.push_back({{"imageAssetId",item.value("imageAssetId","")},{"matrix",item.value("matrix",Json::array())},{"width",item.value("width",0.0)},{"height",item.value("height",0.0)},{"alpha",item.value("alpha",1.0)}});
        if(item.contains("mesh")&&item.at("mesh").is_object())items.back()["mesh"]=item.at("mesh");  // plugin-mesh-v1 triangles
      }
      auto* p=plugin(m.plugin);
      const auto states=described.count(m.plugin)?described.at(m.plugin):Json::object();
      list.push_back({{"instance",id},{"plugin",m.plugin},{"available",p&&p->available&&m.mounted},{"error",m.error},{"element",m.element},{"items",items},{"state",states.value(id,Json())}});
    }
    Json events=Json::array();for(const auto& e:recent_events)events.push_back(e);
    return {{"active",active()},{"plugins",plugin_list},{"instances",list},{"observations",observations},{"clockMs",clock_ms},{"lastObserve",last_observe},
      {"events",events},{"eventCount",event_count},{"soundCount",sound_count}};
  }
private:
  /** update reply {notifications: {instance: [..]}}: keeps `event` entries (with their payload). */
  void collect_events(const std::string& plugin_id,const Json& reply) {
    if(!reply.contains("notifications")||!reply.at("notifications").is_object())return;
    for(const auto& [instance,list]:reply.at("notifications").items()) {
      if(!list.is_array())continue;
      for(const auto& n:list) {
        if(!n.is_object()||n.value("kind","")!="event")continue;
        Json entry=n;entry["instance"]=instance;entry["plugin"]=plugin_id;entry["clockMs"]=clock_ms;
        recent_events.push_back(std::move(entry));if(recent_events.size()>MAX_RECENT_EVENTS)recent_events.pop_front();
        ++event_count;
        const auto audio=n.contains("audio")&&n.at("audio").is_string()?n.at("audio").get<std::string>():"";
        if(audio.empty()||sounds.size()>=MAX_PENDING_SOUNDS)continue;
        auto number=[&](const char* k,double fallback){return n.contains(k)&&n.at(k).is_number()?n.at(k).get<double>():fallback;};
        sounds.push_back({audio,std::clamp(number("volume",1),0.0,1.0),std::clamp(number("balance",0),-1.0,1.0)});++sound_count;
      }
    }
  }
  void open(ExtensionPlugin& p,const fs::path& root) {
    auto fail=[&](const std::string& message){p.available=false;p.error=message;if(p.module){SDL_UnloadObject(p.module);p.module=nullptr;}p.request=nullptr;};
    if(p.abi!="raising-extension-1")return fail("지원하지 않는 확장 ABI입니다: "+p.abi);
    const auto relative=fs::u8path(p.entry);
    if(p.entry.empty()||relative.is_absolute()||relative.has_root_name())return fail("확장 모듈 경로가 올바르지 않습니다.");
    std::error_code ec;const auto base=fs::weakly_canonical(root,ec),file=fs::weakly_canonical(base/relative,ec);
    const auto within=file.lexically_relative(base);
    if(ec||within.empty()||*within.begin()==".."||!fs::is_regular_file(file,ec))return fail("확장 모듈 파일이 없습니다: "+p.entry);
    if(file.extension()!=".dll"&&file.extension()!=".so")return fail("이 플레이어용 확장 모듈이 아닙니다: "+p.entry);
    p.module=SDL_LoadObject(file.u8string().c_str());
    if(!p.module)return fail(std::string("확장 모듈을 불러오지 못했습니다: ")+SDL_GetError());
    auto version=reinterpret_cast<int(*)(void)>(SDL_LoadFunction(p.module,"raising_extension_api_version"));
    p.request=reinterpret_cast<const char*(*)(const char*)>(SDL_LoadFunction(p.module,"raising_extension_request"));
    if(!version||!p.request)return fail("확장 모듈에 필요한 함수가 없습니다.");
    if(version()!=1)return fail("확장 API 버전이 맞지 않습니다: "+std::to_string(version()));
    p.available=true;
    if(!init(p))return fail("확장 초기화 실패: "+p.error);
  }
  bool init(ExtensionPlugin& p) {
    const auto reply=call(p,{{"op","init"},{"documents",p.documents}});
    if(!reply.value("ok",false)){p.error=reply.value("error","init");return false;}
    p.diagnostics=reply.value("diagnostics",Json::array());return p.available;
  }
  bool mount_request(ExtensionMount& m) {
    auto* p=plugin(m.plugin);m.mounted=false;m.items=Json::array();
    if(!p||!p->available){m.error=p?p->error:"플러그인이 없습니다.";return false;}
    const auto reply=call(*p,{{"op","mount"},{"instance",m.instance},{"element",m.element}});
    if(!reply.value("ok",false)){m.error=reply.value("error","mount");return false;}
    m.error.clear();m.mounted=true;return true;
  }
  void unmount(ExtensionMount& m) {
    if(!m.mounted)return;m.mounted=false;m.items=Json::array();
    if(auto* p=plugin(m.plugin);p&&p->available)call(*p,{{"op","unmount"},{"instance",m.instance}});
  }
  /** Never throws: a broken reply disables the plugin and leaves the game running. */
  Json call(ExtensionPlugin& p,const Json& request) {
    if(!p.available||!p.request)return {{"ok",false},{"error",p.error}};
    const char* text=p.request(request.dump().c_str());
    if(!text){p.available=false;p.error="확장 모듈이 응답하지 않았습니다.";return {{"ok",false},{"error",p.error}};}
    auto reply=Json::parse(text,nullptr,false);
    if(reply.is_discarded()||!reply.is_object()){p.available=false;p.error="확장 모듈 응답이 올바른 JSON이 아닙니다.";return {{"ok",false},{"error",p.error}};}
    return reply;
  }
};
