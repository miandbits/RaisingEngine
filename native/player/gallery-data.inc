// Gallery rows deliberately expose only public, unlocked metadata, never the live state.
  bool authored_gallery() const {
    return game.gallery.active && preview_screen.empty() &&
      (display_content.at("project").value("gallery",Json::object()).contains("screenId") ||
       (gallery_branch_view&&display_content.at("project").value("gallery",Json::object()).contains("branchScreenId")));
  }
  Json gallery_rows(bool selection) const {
    Json rows=Json::array();
    const auto config=display_content.at("project").value("gallery",Json::object());
    for(const auto& entry:config.value("entries",Json::array())) {
      const auto id=entry.value("id",""),kind=entry.value("kind",""),category=entry.value("category","");
      const auto records=game.gallery.records(entry);const bool unlocked=!records.empty();
      if(!unlocked&&entry.value("lockedDisplay","hint")=="hidden")continue;
      if(selection){if(id!=gallery_entry||!unlocked)continue;}
      else if((gallery_kind!="all"&&kind!=gallery_kind)||
        (gallery_status=="locked"&&unlocked)||(gallery_status=="unlocked"&&!unlocked)||
        (!gallery_category.empty()&&category!=gallery_category))continue;
      const size_t index=unlocked?std::min(gallery_record,records.size()-1):0;
      const bool replay=unlocked&&!records[index].value("replayFrames",Json::array()).empty();
      const auto image=unlocked?(kind=="cg"?entry.value("sourceId",""):entry.value("thumbnailAssetId","")):"";
      Json values={{"id",id},{"entryId",id},{"name",unlocked?entry.value("title",""):"???"},
        {"kind",kind},{"category",category},{"hint",unlocked?"":entry.value("hint","")},
        {"imageAssetId",image},{"unlocked",unlocked},{"locked",!unlocked},
        {"statusText",unlocked?"해금됨":"미해금"},{"recordCount",records.size()},
        {"recordLabel",unlocked?std::to_string(index+1)+" / "+std::to_string(records.size()):""},
        {"isCG",unlocked&&kind=="cg"},{"isMusic",unlocked&&kind=="music"},
        {"isNarrative",unlocked&&(kind=="event"||kind=="ending")},{"canReplay",replay},
        {"hasImage",!image.empty()},{"enabled",unlocked},{"action","galleryView"},
        {"musicLabel",gallery_music_playing?"일시정지":"음악 재생"},{"reason",unlocked?"":entry.value("hint","")}};
      rows.push_back({{"id",id},{"values",values},{"enabled",true},{"reason",""}});
    }
    return rows;
  }

#include "gallery-branches.inc"
