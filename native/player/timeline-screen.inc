  std::string result_playback_markup(bool changes_only,const Json& element) const {
    const auto text=result_playback_text(plan_activity_timeline(display_content,game.state,locale),game.timeline_cursor,changes_only,locale);
    if(!changes_only)return escape(text);
    std::istringstream lines(text);std::string line,out;
    while(std::getline(lines,line))if(!line.empty())
      out+="<div style=\"background-color:"+css_color(element.value("fill","#213c34e8"))+";color:"+css_color(element.value("color","#fff8e6"))+";padding:4px 12px;margin-bottom:7px;border-left-width:"+std::string(element.value("fill","")=="#00000000"?"0":"3")+"px;border-left-color:#c9a96b;\">"+escape(line)+"</div>";
    return out;
  }
  std::string timeline_stage(const Json& stage,double elapsed,double availableWidth,double availableHeight) const {
    const double stageWidth=stage.at("width"),stageHeight=stage.at("height");
    const double scale=std::min(availableWidth/stageWidth,availableHeight/stageHeight);
    const double fittedWidth=stageWidth*scale,fittedHeight=stageHeight*scale;
    std::string out="<div id=\"timeline-stage\" style=\"position:absolute;pointer-events:none;overflow:hidden;clip:always;left:"+number((availableWidth-fittedWidth)/2)+"px;top:"+number((availableHeight-fittedHeight)/2)+"px;width:"+number(fittedWidth)+"px;height:"+number(fittedHeight)+"px;\">";
    const auto& layers=stage.at("layers");
    for(size_t i=0;i<layers.size();i++) {
      const auto& layer=layers.at(i);const auto frame=timeline_layer_frame_index(layer,elapsed);
      const auto& assetId=layer.at("frames").at(frame).at("imageAssetId");
      out+="<div id=\"timeline-stage-layer-"+std::to_string(i)+"\" data-layer-id=\""+escape(layer.value("id",""))+"\" data-frame-index=\""+std::to_string(frame)+"\" data-image-asset-id=\""+escape(assetId.get<std::string>())+"\" style=\"position:absolute;pointer-events:none;left:"+number(layer.at("x").get<double>()*scale)+"px;top:"+number(layer.at("y").get<double>()*scale)+"px;width:"+number(layer.at("width").get<double>()*scale)+"px;height:"+number(layer.at("height").get<double>()*scale)+"px;z-index:"+std::to_string(i)+";\">";
      out+=image({{"imageAssetId",assetId},{"fit","fill"}})+"</div>";
    }
    return out+"</div>";
  }
  bool readable_timeline_visible() const {
    return ui_modal.empty()&&game.timeline_active()&&!(game.timeline_cursor.value("completed",false)&&display_content.at("project").value("visual",Json::object()).value("activityTimelineCompletion","")=="reveal-result");
  }
  std::string readable_timeline_body(double availableWidth,double availableHeight) {
    const auto plan=plan_activity_timeline(display_content,game.state,locale);const auto& cursor=game.timeline_cursor;
    const bool completed=cursor.value("completed",false),paused=cursor.value("paused",false);
    std::string out="<div id=\"timeline-overlay\" class=\"reading-item\"><p id=\"timeline-status\"><b>"+(completed?settings_words("재생 완료","Presentation complete","演出完了"):paused?settings_words("일시 정지","Paused","一時停止中"):settings_words("활동 연출","Activity presentation","活動の演出"))+"</b></p>";
    if(plan.contains("resultPlayback")){
      out+="<div id=\"timeline-text\" data-result-playback=\"true\" data-result-changes=\"false\" style=\"white-space:pre-wrap;line-height:1.45;\">"+result_playback_markup(false,Json::object())+"</div>";
    }else{
      const auto& clip=plan.at("clips").at(std::min(cursor.at("clipIndex").get<size_t>(),plan.at("clips").size()-1));
      if(clip.contains("stage")||clip.contains("imageAssetId")){
        const double mediaWidth=std::max(44.0,availableWidth),mediaHeight=std::min(mediaWidth*.5625,std::max(72.0,availableHeight*.25));
        out+="<div id=\"timeline-visual\" style=\"position:relative;pointer-events:none;width:"+number(mediaWidth)+"px;height:"+number(mediaHeight)+"px;overflow:hidden;clip:always;margin-bottom:12px;\">";
        if(clip.contains("stage"))out+=timeline_stage(clip.at("stage"),timeline_clip_elapsed(clip,cursor),mediaWidth,mediaHeight);
        else out+=image({{"imageAssetId",clip.at("imageAssetId")},{"width",mediaWidth},{"height",mediaHeight},{"fit","contain"}});
        out+="</div>";
      }
      // The narrative stays in normal flow, independent of the media fade and source canvas geometry.
      out+="<p id=\"timeline-text\" style=\"white-space:pre-wrap;line-height:1.45;\">"+escape(clip.value("text",""))+"</p>";
    }
    out+="</div>";
    const auto control=[&](const std::string& id,const std::string& label,const std::string& type,bool enabled=true){return "<div class=\"reading-item\">"+button(id,label,{{"type",type}},enabled)+"</div>";};
    out+=control("timeline-speed",settings_words("재생 속도 ×","Speed ×","速度 ×")+std::to_string(cursor.value("speed",1)),"timelineSpeed",!completed);
    out+=control("timeline-save",settings_words("저장","Save","保存"),"save");
    out+=control("timeline-load",settings_words("불러오기","Load","読み込む"),"load");
    out+="<div class=\"reading-item\">"+button("engine-reading-options",settings_words("읽기 설정","Reading options","読み方の設定"),{{"type","readingOptions"},{"open",true}})+"</div>";
    return out;
  }
  std::string timeline_overlay() {
    if(!game.timeline_active())return "";
    const auto plan=plan_activity_timeline(display_content,game.state,locale);const auto& cursor=game.timeline_cursor;
    if(cursor.value("completed",false)&&display_content.at("project").value("visual",Json::object()).value("activityTimelineCompletion","")=="reveal-result")return "";
    const bool completed=cursor.value("completed",false),paused=cursor.value("paused",false);
    if(plan.contains("resultPlayback")){
      if(authored_timeline_controls==7)return "";
      const int barWidth=std::min(720,width-32),buttonWidth=(barWidth-24)/3;
      const std::vector<Json> controls={
        {{"id","timeline-pause"},{"label",paused?settings_words("계속 재생","Resume","再開"):settings_words("일시정지","Pause","一時停止")},{"type","timelinePause"}},
        {{"id","timeline-speed"},{"label",settings_words("재생 속도 ×","Speed ×","速度 ×")+std::to_string(cursor.value("speed",1))},{"type","timelineSpeed"}},
        {{"id","timeline-skip"},{"label",settings_words("이번 활동 연출 건너뛰기","Skip activity presentation","この活動の演出をスキップ")},{"type","timelineSkip"}}
      };
      std::string out="<div id=\"result-playback-controls\" style=\"position:absolute;left:"+number((width-barWidth)/2)+"px;top:"+number(height-60)+"px;width:"+number(barWidth)+"px;height:48px;z-index:121;\">";
      for(size_t i=0;i<controls.size();i++){const auto& c=controls[i];out+="<div style=\"position:absolute;left:"+number(i*(buttonWidth+12))+"px;top:0;width:"+number(buttonWidth)+"px;height:48px;\">"+button(c.at("id"),c.at("label"),{{"type",c.at("type")}},!completed)+"</div>";}
      return out+"</div>";
    }
    const auto& clip=plan.at("clips").at(std::min(cursor.at("clipIndex").get<size_t>(),plan.at("clips").size()-1));
    const int panelWidth=std::max(240,std::min(960,width-24)),panelHeight=std::max(240,std::min(620,height-24));
    const int innerWidth=panelWidth-32,mediaHeight=std::max(60,panelHeight-194),buttonWidth=std::max(60,(innerWidth-24)/3);
    std::string result="<div style=\"position:absolute;left:0;top:0;width:100%;height:100%;z-index:120;background-color:rgba(0,0,0,190);\"></div>";
    result+="<div id=\"timeline-overlay\" style=\"position:absolute;left:"+number((width-panelWidth)/2)+"px;top:"+number((height-panelHeight)/2)+"px;width:"+number(panelWidth)+"px;height:"+number(panelHeight)+"px;background-color:#eff4e8;color:#20372d;z-index:121;overflow:auto;\">";
    result+="<div id=\"timeline-status\" style=\"position:absolute;left:16px;top:10px;width:"+number(innerWidth)+"px;height:30px;font-size:20px;\">"+(completed?settings_words("재생 완료","Presentation complete","演出完了"):paused?settings_words("일시 정지","Paused","一時停止中"):settings_words("활동 연출","Activity presentation","活動の演出"))+" · "+std::to_string(std::min(cursor.at("clipIndex").get<size_t>()+1,plan.at("clips").size()))+" / "+std::to_string(plan.at("clips").size())+"</div>";
    result+="<div id=\"timeline-visual\" style=\"position:absolute;left:16px;top:48px;width:"+number(innerWidth)+"px;height:"+number(mediaHeight)+"px;overflow:hidden;background-color:#20372d;\">";
    if(clip.contains("stage"))result+=timeline_stage(clip.at("stage"),timeline_clip_elapsed(clip,cursor),innerWidth,mediaHeight);
    else if(clip.contains("imageAssetId"))result+=image({{"imageAssetId",clip.at("imageAssetId")},{"width",innerWidth},{"height",mediaHeight},{"fit","contain"}});
    result+="<div id=\"timeline-text\" style=\"position:absolute;z-index:33;left:0;bottom:0;width:"+number(innerWidth-24)+"px;max-height:"+number(std::max(40,mediaHeight/2))+"px;padding:12px;overflow:auto;background-color:rgba(0,0,0,190);color:#fff9e8;font-size:22px;white-space:pre-wrap;\">"+escape(clip.value("text",""))+"</div></div>";
    const std::vector<Json> controls={
      {{"id","timeline-pause"},{"label",paused?settings_words("재생","Resume","再開"):settings_words("일시 정지","Pause","一時停止")},{"type","timelinePause"},{"enabled",!completed}},
      {{"id","timeline-speed"},{"label",settings_words("속도 ×","Speed ×","速度 ×")+std::to_string(cursor.value("speed",1))},{"type","timelineSpeed"},{"enabled",!completed}},
      {{"id","timeline-skip"},{"label",settings_words("건너뛰기","Skip","スキップ")},{"type","timelineSkip"},{"enabled",!completed}},
      {{"id","timeline-save"},{"label",settings_words("저장","Save","保存")},{"type","save"},{"enabled",true}},
      {{"id","timeline-load"},{"label",settings_words("불러오기","Load","読み込む")},{"type","load"},{"enabled",true}},
      {{"id","timeline-continue"},{"label",settings_words("결과 확인 · 계속","Review result · continue","結果を確認 · 続ける")},{"type","advance"},{"enabled",completed}}
    };
    for(size_t i=0;i<controls.size();i++){const auto& control=controls[i];result+="<div class=\"timeline-control"+std::string(control.value("enabled",true)?"":" timeline-disabled")+"\" style=\"position:absolute;left:"+number(16+(i%3)*(buttonWidth+12))+"px;top:"+number(panelHeight-118+(i/3)*52)+"px;width:"+number(buttonWidth)+"px;height:44px;\">"+button(control.at("id"),control.at("label"),{{"type",control.at("type")}},control.value("enabled",true))+"</div>";}
    return result+"</div>";
  }
