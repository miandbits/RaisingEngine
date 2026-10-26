  // Reused by authored canvas, readable leaves and the readable viewport.
  mutable std::map<std::string,SurfacePaint::Size> surface_dimensions;
  SurfacePaint::Size surface_size(const std::string& id,const std::string& path) const {
    if(path.empty())return {};
    try {
      const auto hash=game.content.value("assetHashes",Json::object()).value(id,"");
      const auto key=path+"|"+hash+"|"+std::to_string(fs::file_size(fs::u8path(path)))+"|"+std::to_string(static_cast<long long>(fs::last_write_time(fs::u8path(path)).time_since_epoch().count()));
      if(const auto found=surface_dimensions.find(key);found!=surface_dimensions.end())return found->second;
      if(surface_dimensions.size()>=256)surface_dimensions.clear();
      return surface_dimensions.emplace(key,SurfacePaint::png_size(fs::u8path(path))).first->second;
    }catch(...){return {};}
  }
  std::string surface_styles(const Json& visual,const Json& elements,const Json& root_surface=Json()) const {
    std::string definitions,rules;std::set<std::string> declared;
    const auto add=[&](const Json& surface,const std::string& selector){
      if(!SurfacePaint::active(surface))return;
      const auto id=surface.at("imageAssetId").get<std::string>(),path=asset(id);const auto size=surface_size(id,path);if(!size)return;
      const auto mode=surface.value("mode","");std::string decorator;
      if(mode=="tile")decorator="image("+Json(path).dump()+" repeat left top)";
      else {
        const auto insets=surface.value("insets",Json::object());std::array<int,4> edge{};int n=0;
        for(const auto* key:{"top","right","bottom","left"}){if(!insets.contains(key)||!insets.at(key).is_number_integer())return;edge[n++]=insets.at(key).get<int>();if(edge[n-1]<0||edge[n-1]>8192)return;}
        if(unsigned(edge[1]+edge[3])>=size.width||unsigned(edge[0]+edge[2])>=size.height)return;
        // Stable internal names; the source file is always a whole authored PNG.
        uint64_t hash=1469598103934665603ull;for(unsigned char c:path+surface.dump()){hash^=c;hash*=1099511628211ull;}
        const auto name="engine-surface-"+std::to_string(hash);
        if(declared.insert(name).second)definitions+="@spritesheet "+name+"{src:"+Json(path).dump()+";resolution:1x;"+name+"-outer:0px 0px "+std::to_string(size.width)+"px "+std::to_string(size.height)+"px;"+name+"-inner:"+std::to_string(edge[3])+"px "+std::to_string(edge[0])+"px "+std::to_string(size.width-edge[1]-edge[3])+"px "+std::to_string(size.height-edge[0]-edge[2])+"px;}";
        decorator="ninepatch("+name+"-outer, "+name+"-inner)";
      }
      // Padding-box paint preserves authored borders and never participates in layout.
      rules+=selector+"{decorator:"+decorator+" padding-box;}";
    };
    add(root_surface,"body");
    for(const auto& raw:elements){const auto e=resolved_style(visual,raw),surface=e.value("surface",Json());const auto selector="#node-"+e.value("id","")+(e.value("kind","")=="button"?">button":"");add(surface,selector);
      if(SurfacePaint::active(surface)&&e.value("kind","")=="button"){
        for(const auto& pair:std::vector<std::pair<std::string,std::string>>{{"hoverFill",":hover"},{"pressedFill",":active"}})if(e.contains(pair.first))rules+=selector+pair.second+"{decorator:none;background-color:"+css_color(e.at(pair.first))+";}";
      }
    }
    return definitions+rules;
  }
