  std::string gallery_entry;
  size_t gallery_record=0,gallery_frame=0,gallery_page=0;
  std::string gallery_category;
  std::string gallery_overlay() {
    if(!game.content.at("project").contains("gallery")||!preview_screen.empty())return "";
    const auto config=display_content.at("project").at("gallery");
    if(!game.gallery.active)return "<div style=\"position:absolute;right:20px;top:90px;width:180px;height:42px;z-index:11000;\">"+button("gallery-open",config.at("title"),{{"type","galleryOpen"}})+"</div>";
    // Remove all underlying actions from keyboard/gamepad navigation as well as clicks.
    actions.clear();labels.clear();
    const int left=32,top=28,w=width-64,h=height-56;
    std::string out="<div style=\"position:absolute;left:0;top:0;width:100%;height:100%;background-color:#101820f5;z-index:20000;\"></div><div id=\"gallery\" style=\"position:absolute;left:"+number(left)+"px;top:"+number(top)+"px;width:"+number(w)+"px;height:"+number(h)+"px;z-index:20001;background-color:#19252e;color:#eee4c9;\">";
    auto control=[&](const std::string& id,const std::string& label,Json action,int x,int y,int bw,bool enabled=true){return "<div style=\"position:absolute;left:"+number(x)+"px;top:"+number(y)+"px;width:"+number(bw)+"px;height:42px;\">"+button(id,label,action,enabled)+"</div>";};
    out+=control("gallery-close","본편으로",{{"type","galleryClose"}},w-160,14,140);
    const auto entries=config.at("entries");int unlocked=0;for(const auto& e:entries)if(!game.gallery.records(e).empty())++unlocked;
    out+="<div style=\"position:absolute;left:22px;top:16px;font-size:24px;\">"+escape(config.at("title").get<std::string>())+" · "+std::to_string(unlocked)+" / "+std::to_string(entries.size())+"</div>";
    if(!game.gallery.error.empty())out+="<div style=\"position:absolute;left:22px;top:64px;color:#ffb1a0;\">"+escape(game.gallery.error)+"</div>";
    if(gallery_entry.empty()){
      std::vector<std::string> categories={""};for(const auto& e:entries){auto c=e.value("category","");if(std::find(categories.begin(),categories.end(),c)==categories.end())categories.push_back(c);}
      if(std::find(categories.begin(),categories.end(),gallery_category)==categories.end())gallery_category="";
      out+=control("gallery-filter",gallery_category.empty()?"분류: 전체":"분류: "+gallery_category,{{"type","galleryCategory"}},22,92,250);
      Json filtered=Json::array();for(const auto& e:entries)if(gallery_category.empty()||e.value("category","")==gallery_category)filtered.push_back(e);
      const size_t pageSize=6,pages=std::max<size_t>(1,(filtered.size()+pageSize-1)/pageSize);gallery_page=std::min(gallery_page,pages-1);
      const int cw=(w-66)/3,ch=(h-230)/2;
      for(size_t i=gallery_page*pageSize;i<std::min(filtered.size(),(gallery_page+1)*pageSize);i++){
        const auto& e=filtered[i];const auto records=game.gallery.records(e);bool seen=!records.empty();const int x=22+int(i%3)*(cw+11),y=148+int((i%6)/3)*(ch+10);
        out+="<div style=\"position:absolute;left:"+number(x)+"px;top:"+number(y)+"px;width:"+number(cw)+"px;height:"+number(ch)+"px;background-color:#293943;overflow:hidden;\">";
        if(seen&&e.contains("thumbnailAssetId"))out+="<div style=\"position:absolute;left:8px;top:8px;width:82px;height:82px;\">"+image({{"imageAssetId",e.at("thumbnailAssetId")},{"fit","contain"}})+"</div>";
        const auto label=seen?e.value("title",""):"???";
        out+="<div style=\"position:absolute;left:100px;right:8px;top:12px;\">"+escape(label)+"<br/>"+escape(e.value("category",""))+"<br/>"+escape(seen?"기록 "+std::to_string(records.size())+"개":e.value("hint","아직 보지 못한 이야기"))+"</div>";
        out+=control("gallery-entry-"+e.value("id",""),seen?"기록 보기":"미해금",{{"type","gallerySelect"},{"entryId",e.at("id")}},8,ch-50,cw-16,seen)+"</div>";
      }
      out+=control("gallery-prev-page","이전",{{"type","galleryPage"},{"delta",-1}},22,h-58,130,gallery_page>0);
      out+="<div style=\"position:absolute;left:172px;top:"+number(h-48)+"px;\">"+std::to_string(gallery_page+1)+" / "+std::to_string(pages)+"</div>";
      out+=control("gallery-next-page","다음",{{"type","galleryPage"},{"delta",1}},272,h-58,130,gallery_page+1<pages);
    }else{
      Json entry=nullptr;for(const auto& e:entries)if(e.at("id")==gallery_entry)entry=e;
      const auto records=entry.is_null()?Json::array():game.gallery.records(entry);
      if(!records.empty()){
        gallery_record=std::min(gallery_record,records.size()-1);const auto& r=records[gallery_record];const auto& frames=r.at("frames");gallery_frame=std::min(gallery_frame,frames.size());
        const int panelHeight=h-210;
        out+="<div id=\"gallery-record-title\" style=\"position:absolute;left:22px;top:84px;font-size:22px;\">"+escape(r.value("title",""))+" · "+escape(r.value("unlockedAt",""))+"</div>";
        std::string body;
        if(gallery_frame<frames.size()){
          const auto& f=frames[gallery_frame];body=f.value("speaker","")+"\n\n"+f.value("text","");if(f.contains("choice"))body+="\n\n선택: "+f.at("choice").get<std::string>();
          auto scene=f.value("scene",Json::object());if(scene.value("backgroundAssetId",Json()).is_null()&&entry.contains("thumbnailAssetId"))scene["backgroundAssetId"]=entry.at("thumbnailAssetId");if(scene.contains("backgroundAssetId")&&!scene.at("backgroundAssetId").is_null())out+="<div style=\"position:absolute;left:22px;top:130px;width:"+number(w/2-36)+"px;height:"+number(panelHeight)+"px;\">"+image({{"imageAssetId",scene.at("backgroundAssetId")},{"fit","contain"}})+"</div>";
        }else{body="이야기 완료 당시 능력치\n";for(const auto& [id,value]:r.at("attributes").items())body+="\n"+r.at("attributeLabels").value(id,id)+": "+value.dump();}
        out+="<div id=\"gallery-frame\" style=\"position:absolute;left:"+number(w/2)+"px;top:130px;width:"+number(w/2-24)+"px;height:"+number(panelHeight)+"px;overflow:auto;white-space:pre-wrap;font-size:22px;\">"+escape(body)+"</div>";
        out+=control("gallery-back","목록",{{"type","galleryBack"}},22,h-58,120);
        out+=control("gallery-variant","기록 "+std::to_string(gallery_record+1)+" / "+std::to_string(records.size()),{{"type","galleryVariant"}},154,h-58,180,records.size()>1);
        out+=control("gallery-prev","이전 대사",{{"type","galleryFrame"},{"delta",-1}},w-310,h-58,140,gallery_frame>0);
        out+=control("gallery-next",gallery_frame+1==frames.size()?"결과 보기":"다음 대사",{{"type","galleryFrame"},{"delta",1}},w-160,h-58,140,gallery_frame<frames.size());
      }
    }
    return out+"</div>";
  }
