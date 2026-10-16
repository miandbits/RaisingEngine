#pragma once
// Runtime extension host (ABI raising-extension-1, packages/plugin-host/CONTRACT.md §5-§8).
// Plugins are presentation only: they observe committed transitions and return draw items.
// Nothing here can reach GameSession state, so animation can never change game results.
#include "session.hpp"
#include <SDL3/SDL_loadso.h>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <set>

/** Mesh topology (uvs 0..1 of the item's picture, triangle indices): shared by every frame of a mesh. */
struct ExtensionMeshTopology { std::vector<double> uvs; std::vector<int> indices; };
/**
 * One draw item, decoded once per frame reply (documented items or compact frames, CONTRACT.md §7)
 * so rendering never reads JSON. A mesh item with `mesh` set but no valid topology draws nothing
 * (never its undeformed fallback rectangle).
 */
struct ExtensionDrawItem {
  std::string image;
  bool valid=true;  // a malformed matrix draws nothing
  double m[6]={1,0,0,1,0,0},w=0,h=0,alpha=1;
  bool region=false; double r[4]={0,0,0,0};     // plugin-atlas-v1: the picture is this sub-image (page px)
  bool mesh=false; int mesh_id=-1;               // compact frames name the topology by id
  std::vector<double> positions;                  // element-local x, y per vertex
  std::shared_ptr<const ExtensionMeshTopology> topology;
  bool drawable_mesh() const {return mesh&&topology&&!topology->indices.empty()&&positions.size()==topology->uvs.size();}
};
struct ExtensionPlugin {
  std::string id,version,abi,entry,error;
  Json documents=Json::array(),diagnostics=Json::array();
  bool available=false;
  SDL_SharedObject* module=nullptr;
  const char* (*request)(const char*)=nullptr;
  // Compact frames (format 2, negotiated in init): mesh topology cached per id for the init lifetime.
  int frame_format=1;uint64_t mesh_epoch=0;
  std::map<int,std::shared_ptr<const ExtensionMeshTopology>> meshes;
  std::set<std::string> ops;  // optional requests the plugin listed in its init reply (`hitTest`, `points`)
};
/** Sound of an `event` notification that names an audio asset (CONTRACT.md §7 event payloads). */
struct ExtensionSound { std::string asset; double volume=1,balance=0; };
struct ExtensionMount {
  std::string instance,plugin,error;
  Json element=nullptr;
  std::vector<ExtensionDrawItem> items;
  uint64_t frame=0;  // compact frames: the frame number these items are (delta base)
  bool mounted=false,seen=false;
  bool interactive=false;  // mount reply: the plugin wants clicks on this element (bounding boxes)
  Json last_click=nullptr;  // the newest click's hitTest reply (reports)
};
/** Frame reply costs (automation reports, the large-rig benchmark): presentation only. */
struct ExtensionTiming { uint64_t frames=0,bytes=0,last_bytes=0;double ms=0,last_ms=0,request_ms=0; };
/**
 * SAX reader of a compact frame reply (CONTRACT.md §7 format 2): draw items straight from the text,
 * no JSON tree (a 20-character screen reads ~800 KB of replies per frame). Anything that is not a
 * compact reply (documented items, an error reply) sets `other`; the caller then parses it as JSON.
 */
struct CompactFrameReader {
  struct Entry { bool unchanged=false; ExtensionDrawItem item; };
  bool ok=false,other=false,has_items=false;int format=1;uint64_t epoch=0,frame=0;
  std::vector<Entry> entries;
  int depth=0,field=0,sub=-1,region_at=0;std::string root_key;
  ExtensionDrawItem& item(){return entries.back().item;}
  bool number(double v,bool integer) {
    if(depth==1){if(root_key=="format")format=int(v);else if(root_key=="meshEpoch")epoch=uint64_t(v);else if(root_key=="frame")frame=uint64_t(v);return true;}
    if(depth==2){entries.push_back({true,{}});return true;}  // 0 = unchanged since `since`
    if(depth==3) {
      auto& d=item();
      if(field>=1&&field<=6)d.m[field-1]=v;else if(field==7)d.w=v;else if(field==8)d.h=v;else if(field==9)d.alpha=v;
      else if(field==11){d.mesh=true;d.mesh_id=integer?int(v):-1;}else d.valid=false;
      field++;return true;
    }
    if(depth==4){auto& d=item();if(sub==10){if(region_at<4)d.r[region_at++]=v;}else if(sub==12)d.positions.push_back(v);return true;}
    return true;
  }
  bool null(){if(depth==3)field++;else if(depth>3)item().valid=false;return true;}
  bool boolean(bool b){if(depth==1&&root_key=="ok")ok=b;else if(depth>=3){item().valid=false;if(depth==3)field++;}return true;}
  bool number_integer(Json::number_integer_t v){return number(double(v),true);}
  bool number_unsigned(Json::number_unsigned_t v){return number(double(v),true);}
  bool number_float(Json::number_float_t v,const Json::string_t&){return number(v,false);}
  bool string(Json::string_t& s){if(depth==3){if(field==0)item().image=s;else item().valid=false;field++;}else if(depth>3)item().valid=false;return true;}
  bool binary(Json::binary_t&){return true;}
  bool start_object(std::size_t){if(depth==0){depth=1;return true;}other=true;return false;}
  bool key(Json::string_t& k){if(depth==1)root_key=k;return true;}
  bool end_object(){depth--;return true;}
  bool start_array(std::size_t n) {
    if(depth==1){if(root_key!="items"){other=true;return false;}depth=2;has_items=true;return true;}
    if(depth==2){depth=3;entries.emplace_back();field=0;return true;}
    if(depth==3){sub=field==10||field==12?field:-1;if(sub<0)item().valid=false;region_at=0;if(sub==12&&n!=std::size_t(-1))item().positions.reserve(n);depth=4;return true;}
    other=true;return false;
  }
  bool end_array() {
    if(depth==4){if(sub==10)item().region=region_at==4&&item().r[2]>0&&item().r[3]>0;depth=3;field++;sub=-1;return true;}
    if(depth==3){if(field<10)item().valid=false;depth=2;return true;}
    if(depth==2)depth=1;
    return true;
  }
  bool parse_error(std::size_t,const std::string&,const nlohmann::detail::exception&){other=true;return false;}
};
/**
 * A small JSON reader driving the same callbacks (compact replies only). nlohmann reads every
 * number through strtod, which on this toolchain costs ~0.2 us: 5000 numbers per character per
 * frame made decoding 3x the plugin's own work. Fixed-point decimals (what compact frames contain)
 * are exact here: mantissa / 10^digits with both exact doubles is correctly rounded. Returns false
 * on anything unusual (exponents, long digit runs, malformed text) so the caller uses nlohmann.
 */
inline bool read_compact_json(const char* s,CompactFrameReader& r) {
  static const double pow10[]={1,1e1,1e2,1e3,1e4,1e5,1e6,1e7,1e8,1e9,1e10,1e11,1e12,1e13,1e14,1e15,1e16,1e17,1e18};
  auto ws=[&]{while(*s==' '||*s=='\n'||*s=='\r'||*s=='\t')s++;};
  std::string text;
  auto str=[&]()->bool{
    if(*s!='"')return false;s++;text.clear();
    for(;;) {
      const char c=*s++;
      if(c=='"')return true;
      if(!c||(unsigned char)c<0x20)return false;
      if(c!='\\'){text+=c;continue;}
      const char e=*s++;
      switch(e){case '"':text+='"';break;case '\\':text+='\\';break;case '/':text+='/';break;case 'b':text+='\b';break;case 'f':text+='\f';break;case 'n':text+='\n';break;case 'r':text+='\r';break;case 't':text+='\t';break;
        default:return false;}  // \u escapes: leave them to nlohmann
    }
  };
  std::function<bool(int)> value=[&](int level)->bool{
    if(level>8)return false;
    ws();
    if(*s=='{') {
      s++;if(!r.start_object(std::size_t(-1)))return !r.other;ws();
      if(*s=='}'){s++;return r.end_object();}
      for(;;){ws();if(!str())return false;if(!r.key(text))return false;ws();if(*s++!=':')return false;if(!value(level+1))return false;ws();if(*s==','){s++;continue;}if(*s=='}'){s++;return r.end_object();}return false;}
    }
    if(*s=='[') {
      s++;if(!r.start_array(std::size_t(-1)))return false;ws();
      if(*s==']'){s++;return r.end_array();}
      for(;;){if(!value(level+1))return false;ws();if(*s==','){s++;continue;}if(*s==']'){s++;return r.end_array();}return false;}
    }
    if(*s=='"'){if(!str())return false;return r.string(text);}
    if(!std::strncmp(s,"true",4)){s+=4;return r.boolean(true);}
    if(!std::strncmp(s,"false",5)){s+=5;return r.boolean(false);}
    if(!std::strncmp(s,"null",4)){s+=4;return r.null();}
    const bool negative=*s=='-';if(negative)s++;
    if(*s<'0'||*s>'9')return false;
    uint64_t mantissa=0;int digits=0,fraction=0;
    while(*s>='0'&&*s<='9'){mantissa=mantissa*10+uint64_t(*s++-'0');if(++digits>18)return false;}
    bool integer=true;
    if(*s=='.'){integer=false;s++;if(*s<'0'||*s>'9')return false;while(*s>='0'&&*s<='9'){mantissa=mantissa*10+uint64_t(*s++-'0');fraction++;if(++digits>18)return false;}}
    if(*s=='e'||*s=='E')return false;
    if(mantissa>(uint64_t(1)<<53))return false;
    const double v=double(mantissa)/pow10[fraction];
    return r.number(negative?-v:v,integer);
  };
  if(!value(0))return false;
  ws();
  return *s==0;
}

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
  ExtensionTiming timing;
  uint64_t clicks=0;  // clicks handed to plugins (presentation only)
  static constexpr size_t MAX_MESH_VERTICES=2048,MAX_MESH_INDICES=3*4096;
  ExtensionHost()=default;
  // Some ABI v1 plugins keep global state. A private module copy gives recollection
  // its own plugin globals without resetting the live game's animation instances.
  bool isolated_modules=false;
  std::vector<fs::path> module_copies;
  ExtensionHost(const ExtensionHost&)=delete;
  ExtensionHost& operator=(const ExtensionHost&)=delete;
  ~ExtensionHost(){for(auto& p:plugins)if(p.module)SDL_UnloadObject(p.module);for(const auto& file:module_copies){std::error_code ec;fs::remove(file,ec);}}
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
    for(auto& [id,m]:mounts){m.mounted=false;m.items.clear();m.frame=0;if(!m.element.is_null())mount_request(m);}
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
  /** One frame request per mounted instance. Compact frames send only changed items ("0" keeps the
   *  previous one) and name mesh topology by id; both formats end up as ExtensionDrawItem lists. */
  void frame() {
    for(auto& [id,m]:mounts) {
      if(!m.mounted)continue;auto* p=plugin(m.plugin);if(!p||!p->available){m.items.clear();m.frame=0;continue;}
      const auto started=std::chrono::steady_clock::now();
      Json request={{"op","frame"},{"instance",id}};
      if(p->frame_format==2){request["format"]=2;if(m.frame)request["since"]=m.frame;}
      size_t bytes=0;
      if(p->frame_format==2) {
        // Compact reply: read with SAX; anything else (an error, documented items) as JSON below.
        const char* text=call_text(*p,request,&bytes);
        timing.request_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
        if(!text){m.items.clear();m.frame=0;continue;}
        CompactFrameReader compact;
        if(!read_compact_json(text,compact)){compact=CompactFrameReader();Json::sax_parse(text,&compact);}
        if(!compact.other&&compact.ok&&compact.has_items&&compact.format==2)apply_compact(*p,m,compact);
        else apply_items(m,parse_reply(*p,text));
      } else apply_items(m,call(*p,request,&bytes));
      const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
      timing.frames++;timing.ms+=ms;timing.last_ms=ms;timing.bytes+=bytes;timing.last_bytes=bytes;
    }
  }
  /**
   * A click at element-local px (x, y) of a mounted instance (CONTRACT.md §7 `hitTest`): the plugin
   * hit-tests its own shapes and, on a hit, routes its UI-only signal (cutout: `partClicked`) to that
   * instance. The click is only observed — it still reaches whatever the screen has under it — and
   * nothing here reaches the game session: it can start animation, never change game state or RNG.
   */
  Json click(const std::string& instance,double x,double y) {
    auto it=mounts.find(instance);
    if(it==mounts.end()||!it->second.mounted||!it->second.interactive||!std::isfinite(x)||!std::isfinite(y))return nullptr;
    auto* p=plugin(it->second.plugin);
    if(!p||!p->available||!p->ops.count("hitTest"))return nullptr;
    auto reply=call(*p,{{"op","hitTest"},{"instance",instance},{"x",x},{"y",y},{"dispatch",true}});
    ++clicks;it->second.last_click={{"x",x},{"y",y},{"hit",reply.value("hit",Json())},{"matched",reply.value("matched",Json::array())}};
    return reply;
  }
  /** Anchor points of a mounted instance in element px ([{slot, name, x, y, rotation}]), for UI that
   *  follows a part (a speech bubble...). Empty when the plugin has none or does not answer `points`. */
  Json points(const std::string& instance) {
    auto it=mounts.find(instance);
    if(it==mounts.end()||!it->second.mounted)return Json::array();
    auto* p=plugin(it->second.plugin);
    if(!p||!p->available||!p->ops.count("points"))return Json::array();
    const auto reply=call(*p,{{"op","points"},{"instance",instance}});
    return reply.value("ok",false)&&reply.contains("points")&&reply.at("points").is_array()?reply.at("points"):Json::array();
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
      // The documented item shape, whichever frame format the plugin answered with.
      Json items=Json::array();
      for(const auto& item:m.items) {
        items.push_back({{"imageAssetId",item.image},{"matrix",{item.m[0],item.m[1],item.m[2],item.m[3],item.m[4],item.m[5]}},{"width",item.w},{"height",item.h},{"alpha",item.alpha}});
        if(item.region)items.back()["region"]={item.r[0],item.r[1],item.r[2],item.r[3]};  // plugin-atlas-v1 sub-image
        if(item.mesh)items.back()["mesh"]={{"positions",item.positions},{"uvs",item.topology?item.topology->uvs:std::vector<double>()},{"indices",item.topology?item.topology->indices:std::vector<int>()}};  // plugin-mesh-v1 triangles
      }
      auto* p=plugin(m.plugin);
      const auto states=described.count(m.plugin)?described.at(m.plugin):Json::object();
      list.push_back({{"instance",id},{"plugin",m.plugin},{"available",p&&p->available&&m.mounted},{"error",m.error},{"element",m.element},{"items",items},{"state",states.value(id,Json())}});
      // Anchors and the newest click (plugin hit tests): only where there are some, older reports keep their shape.
      if(const auto anchors=points(id);!anchors.empty())list.back()["points"]=anchors;
      if(m.interactive)list.back()["interactive"]=true;
      if(!m.last_click.is_null())list.back()["lastClick"]=m.last_click;
    }
    Json events=Json::array();for(const auto& e:recent_events)events.push_back(e);
    std::string formats;for(const auto& p:plugins)formats+=(formats.empty()?"":",")+std::to_string(p.frame_format);
    return {{"active",active()},{"plugins",plugin_list},{"instances",list},{"observations",observations},{"clockMs",clock_ms},{"lastObserve",last_observe},
      {"events",events},{"eventCount",event_count},{"soundCount",sound_count},{"clicks",clicks},
      {"timing",{{"frames",timing.frames},{"frameMs",timing.ms},{"lastFrameMs",timing.last_ms},{"bytes",timing.bytes},{"lastBytes",timing.last_bytes},{"requestMs",timing.request_ms},{"frameFormats",formats}}}};
  }
  /** Documented item JSON (format 1) -> draw item; meshes and regions validated here, once. */
  static ExtensionDrawItem decode_item(const Json& item) {
    ExtensionDrawItem d;
    if(!item.is_object())return d;
    auto num=[](const Json& v,double fallback){return v.is_number()?v.get<double>():fallback;};
    d.image=item.contains("imageAssetId")&&item.at("imageAssetId").is_string()?item.at("imageAssetId").get<std::string>():"";
    const auto& matrix=item.value("matrix",Json::array());
    if(matrix.is_array()&&matrix.size()==6)for(int i=0;i<6;i++)d.m[i]=num(matrix[i],0);else d.valid=false;
    d.w=num(item.value("width",Json()),0);d.h=num(item.value("height",Json()),0);
    d.alpha=num(item.value("alpha",Json()),1);
    read_region(item.value("region",Json()),d);
    if(item.contains("mesh")) {
      d.mesh=true;
      const auto& mesh=item.at("mesh");
      if(mesh.is_object()) {
        auto topology=std::make_shared<ExtensionMeshTopology>();
        if(read_floats(mesh.value("positions",Json()),d.positions)&&read_floats(mesh.value("uvs",Json()),topology->uvs)&&read_indices(mesh.value("indices",Json()),topology->indices)&&mesh_ok(d.positions,*topology))d.topology=topology;
      }
    }
    return d;
  }
private:
  static bool read_floats(const Json& v,std::vector<double>& out) {
    if(!v.is_array())return false;out.reserve(v.size());
    for(const auto& x:v){if(!x.is_number())return false;out.push_back(x.get<double>());}
    return true;
  }
  static bool read_indices(const Json& v,std::vector<int>& out) {
    if(!v.is_array())return false;out.reserve(v.size());
    for(const auto& x:v){if(!x.is_number_integer())return false;out.push_back(x.get<int>());}
    return true;
  }
  /** Same limits as before: pairs of the same count, <= 2048 vertices / 4096 triangles, indices in range. */
  static bool mesh_ok(const std::vector<double>& positions,const ExtensionMeshTopology& t) {
    const size_t n=t.uvs.size()/2;
    if(t.uvs.size()%2||n>MAX_MESH_VERTICES||t.indices.empty()||t.indices.size()%3||t.indices.size()>MAX_MESH_INDICES)return false;
    for(int i:t.indices)if(i<0||size_t(i)>=n)return false;
    return positions.empty()||positions.size()==t.uvs.size();
  }
  static void read_region(const Json& v,ExtensionDrawItem& d) {
    if(!v.is_array()||v.size()!=4)return;
    for(int i=0;i<4;i++){if(!v[i].is_number())return;d.r[i]=v[i].get<double>();}
    d.region=d.r[2]>0&&d.r[3]>0;
  }
  /** Documented items (format 1) or a refused frame: decoded from the JSON reply. */
  void apply_items(ExtensionMount& m,const Json& reply) {
    m.items.clear();m.frame=0;
    if(!reply.value("ok",false)||!reply.contains("items")||!reply.at("items").is_array())return;
    for(const auto& item:reply.at("items"))m.items.push_back(decode_item(item));
  }
  /** Compact frame (format 2): [image, a..f, width, height, alpha(, region|null(, meshId, positions))], 0 = unchanged. */
  void apply_compact(ExtensionPlugin& p,ExtensionMount& m,CompactFrameReader& reply) {
    if(reply.epoch!=p.mesh_epoch){p.meshes.clear();p.mesh_epoch=reply.epoch;}
    std::vector<ExtensionDrawItem> next;next.reserve(reply.entries.size());
    bool complete=true;std::vector<int> unknown;
    for(size_t k=0;k<reply.entries.size();k++) {
      auto& e=reply.entries[k];
      if(e.unchanged){if(k<m.items.size())next.push_back(m.items[k]);else{complete=false;ExtensionDrawItem none;none.valid=false;next.push_back(none);}continue;}
      ExtensionDrawItem d=std::move(e.item);
      if(d.mesh) {
        auto found=p.meshes.find(d.mesh_id);
        if(found!=p.meshes.end())d.topology=found->second;else if(d.mesh_id>=0)unknown.push_back(d.mesh_id);
      }
      next.push_back(std::move(d));
    }
    if(!unknown.empty()) {
      // Topology the host has not cached (a rig loaded after init, a new epoch): fetched once.
      fetch_meshes(p,unknown);
      for(auto& d:next)if(d.mesh&&!d.topology){auto found=p.meshes.find(d.mesh_id);if(found!=p.meshes.end())d.topology=found->second;}
    }
    for(auto& d:next)if(d.mesh&&d.topology&&!mesh_ok(d.positions,*d.topology))d.topology=nullptr;
    m.items=std::move(next);
    // A "0" without a previous item means the plugin and this host disagree: ask for every item next time.
    m.frame=complete?reply.frame:0;
  }
  void fetch_meshes(ExtensionPlugin& p,const std::vector<int>& ids) {
    const auto reply=call(p,{{"op","meshes"},{"ids",ids}});
    if(reply.value("ok",false))store_meshes(p,reply.value("meshes",Json::array()));
  }
  static void store_meshes(ExtensionPlugin& p,const Json& list) {
    if(!list.is_array())return;
    for(const auto& entry:list) {
      if(!entry.is_object()||!entry.contains("id")||!entry.at("id").is_number_integer())continue;
      auto topology=std::make_shared<ExtensionMeshTopology>();
      if(read_floats(entry.value("uvs",Json()),topology->uvs)&&read_indices(entry.value("indices",Json()),topology->indices)&&mesh_ok({},*topology))p.meshes[entry.at("id").get<int>()]=topology;
    }
  }
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
    auto module_file=file;
    if(isolated_modules){
      module_file=fs::temp_directory_path()/("raising-recollection-"+std::to_string(SDL_GetTicksNS())+"-"+file.filename().u8string());
      fs::copy_file(file,module_file);module_copies.push_back(module_file);
    }
    p.module=SDL_LoadObject(module_file.u8string().c_str());
    if(!p.module)return fail(std::string("확장 모듈을 불러오지 못했습니다: ")+SDL_GetError());
    auto version=reinterpret_cast<int(*)(void)>(SDL_LoadFunction(p.module,"raising_extension_api_version"));
    p.request=reinterpret_cast<const char*(*)(const char*)>(SDL_LoadFunction(p.module,"raising_extension_request"));
    if(!version||!p.request)return fail("확장 모듈에 필요한 함수가 없습니다.");
    if(version()!=1)return fail("확장 API 버전이 맞지 않습니다: "+std::to_string(version()));
    p.available=true;
    if(!init(p))return fail("확장 초기화 실패: "+p.error);
  }
  bool init(ExtensionPlugin& p) {
    // frameFormats: compact frames when the plugin supports them (it then answers frameFormat 2 and
    // its mesh topology table); an older plugin ignores the field and keeps the documented items.
    // RAISING_EXTENSION_FRAME_FORMATS=1 (diagnostics, the large-rig benchmark) keeps the documented items.
    const char* only=std::getenv("RAISING_EXTENSION_FRAME_FORMATS");
    const auto reply=call(p,{{"op","init"},{"documents",p.documents},{"frameFormats",only&&std::string(only)=="1"?Json::array({1}):Json::array({2})}});
    if(!reply.value("ok",false)){p.error=reply.value("error","init");return false;}
    p.diagnostics=reply.value("diagnostics",Json::array());
    p.frame_format=reply.value("frameFormat",1)==2?2:1;p.meshes.clear();p.mesh_epoch=reply.value("meshEpoch",uint64_t(0));
    p.ops.clear();if(reply.contains("ops")&&reply.at("ops").is_array())for(const auto& op:reply.at("ops"))if(op.is_string())p.ops.insert(op.get<std::string>());
    if(p.frame_format==2)store_meshes(p,reply.value("meshes",Json::array()));
    return p.available;
  }
  bool mount_request(ExtensionMount& m) {
    auto* p=plugin(m.plugin);m.mounted=false;m.items.clear();m.frame=0;
    if(!p||!p->available){m.error=p?p->error:"플러그인이 없습니다.";return false;}
    const auto reply=call(*p,{{"op","mount"},{"instance",m.instance},{"element",m.element}});
    if(!reply.value("ok",false)){m.error=reply.value("error","mount");return false;}
    m.error.clear();m.mounted=true;m.interactive=reply.value("interactive",false);return true;
  }
  void unmount(ExtensionMount& m) {
    if(!m.mounted)return;m.mounted=false;m.items.clear();m.frame=0;m.interactive=false;
    if(auto* p=plugin(m.plugin);p&&p->available)call(*p,{{"op","unmount"},{"instance",m.instance}});
  }
  /** Never throws: a broken reply disables the plugin and leaves the game running. */
  Json call(ExtensionPlugin& p,const Json& request,size_t* bytes=nullptr) {
    const char* text=call_text(p,request,bytes);
    if(!text)return {{"ok",false},{"error",p.error}};
    return parse_reply(p,text);
  }
  /** The module's reply text (valid until its next call), or null when it is unavailable / silent. */
  const char* call_text(ExtensionPlugin& p,const Json& request,size_t* bytes=nullptr) {
    if(!p.available||!p.request)return nullptr;
    const char* text=p.request(request.dump().c_str());
    if(!text){p.available=false;p.error="확장 모듈이 응답하지 않았습니다.";return nullptr;}
    if(bytes)*bytes=std::strlen(text);
    return text;
  }
  Json parse_reply(ExtensionPlugin& p,const char* text) {
    auto reply=Json::parse(text,nullptr,false);
    if(reply.is_discarded()||!reply.is_object()){p.available=false;p.error="확장 모듈 응답이 올바른 JSON이 아닙니다.";return {{"ok",false},{"error",p.error}};}
    return reply;
  }
};
