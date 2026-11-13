  raising_player::ReadingHistory reading_history{"session-0"};
  uint64_t reading_history_epoch=0;
  std::string history_return_view,history_return_focus;
  bool history_enabled() const {return game.content.at("project").at("visual").contains("readingHistory");}
  void reset_reading_history(const std::string& reason) {
    reading_history.reset("session-"+std::to_string(++reading_history_epoch),reason);markup.reading_history_open=false;
  }
  bool history_foreground() const {
    return history_enabled()&&document&&!preview()&&!editor_live&&!backgrounded&&!recalling()&&!game.gallery.active&&!game.state.is_null()&&!markup.reading_overlay()&&markup.ui_modal.empty()&&markup.pending_purchase.is_null()&&!game.timeline_active()&&game.direction_cursor.is_null();
  }
  // Intersect actual layout boxes with clipping ancestors, then hit-test the
  // remaining area. A merely projected, routed-out or covered leaf is no receipt.
  bool history_visible_box(Rml::Element* element,const Rml::Vector2f& at,const Rml::Vector2f& size,Rml::Element* hit_owner) const {
    if(!element||!element->IsVisible(true))return false;
    float left=std::max(0.f,at.x),top=std::max(0.f,at.y),right=std::min(float(markup.width),at.x+size.x),bottom=std::min(float(markup.height),at.y+size.y);
    for(auto* e=element;e;e=e->GetParentNode()){
      if(e->GetProperty<float>("opacity")<=0)return false;
      if(e==element)continue;
      const auto p=e->GetAbsoluteOffset(Rml::BoxArea::Padding),s=e->GetBox().GetSize(Rml::BoxArea::Padding);
      if(e->GetProperty("overflow-x")->ToString()!="visible"){left=std::max(left,p.x);right=std::min(right,p.x+s.x);}
      if(e->GetProperty("overflow-y")->ToString()!="visible"){top=std::max(top,p.y);bottom=std::min(bottom,p.y+s.y);}
    }
    if(right-left<=1||bottom-top<=1)return false;
    for(const auto point:std::vector<Rml::Vector2f>{{(left+right)/2,(top+bottom)/2},{left+1,top+1},{right-1,top+1},{left+1,bottom-1},{right-1,bottom-1}})
      for(auto* e=context->GetElementAtPoint(point);e;e=e->GetParentNode())if(e==hit_owner)return true;
    return false;
  }
  bool history_visible_text(Rml::Element* element,Rml::Element* owner=nullptr) const {
    if(!element||!element->IsVisible(true))return false;if(!owner)owner=element;
    if(element->IsClassSet("reading-receipt-text")&&contains_rendered_text(element)){
      for(int i=0;i<element->GetNumBoxes();++i){Rml::Vector2f offset;const auto& box=element->GetBox(i,offset);
        if(history_visible_box(element,element->GetAbsoluteOffset(Rml::BoxArea::Border)+offset,box.GetSize(Rml::BoxArea::Content),owner))return true;
      }
    }
    for(int i=0;i<element->GetNumChildren();++i)if(history_visible_text(element->GetChild(i),owner))return true;
    return false;
  }
  std::string history_visible_speaker() const {
    for(const auto& e:markup.render_elements)if(e.value("kind","")=="text"&&e.value("binding","")=="speaker"&&markup.visible(e)&&e.value("opacity",1.0)>0&&history_visible_text(document->GetElementById("node-"+e.value("id",""))))return markup.bind("speaker");
    return "";
  }
  std::string history_key() const {
    const auto cursor=game.state.value("activeNarrative",Json());
    return Json::array({cursor.value("kind",""),cursor.value("id",""),cursor.value("nodeId",""),cursor.value("actorId",""),game.state.value("periodIndex",0),markup.locale}).dump();
  }
  Json history_receipt(const std::string& kind,const std::string& text) const {
    return {{"epoch",reading_history.epoch()},{"key",history_key()},{"kind",kind},{"text",text},{"locale",markup.locale},{"speaker",history_visible_speaker()},{"date",markup.bind("calendar")}};
  }
  void capture_reading_frame() {
    if(!history_foreground())return;
    const auto narrative=markup.narrative();if(narrative.is_null())return;
    const auto type=narrative.at("node").value("type","");if(type!="dialogue"&&type!="choice")return;
    for(const auto& e:markup.render_elements)if((e.value("kind","")=="dialogue"||(e.value("kind","")=="text"&&e.value("binding","")=="story"))&&markup.visible(e)&&e.value("opacity",1.0)>0&&history_visible_text(document->GetElementById("node-"+e.value("id","")))){
      auto text=markup.bind("story");if(dialogue_active())text.resize(std::min(text.size(),dialogue_prefix_bytes(dialogue_text,size_t(dialogue_characters))));
      reading_history.record(history_receipt(type=="choice"?"prompt":"dialogue",text));break;
    }
  }
  Json choice_reading_receipt(const Json& action) const {
    if(!history_foreground())return nullptr;
    for(const auto& [id,value]:markup.actions)if(value.value("type","")=="choose"&&value.value("choiceId",Json())==action.value("choiceId",Json())){
      auto* element=document->GetElementById(id);if(!element||element->HasAttribute("disabled")||!history_visible_text(element))continue;
      const auto label=markup.labels.find(id);if(label==markup.labels.end())continue;
      auto receipt=history_receipt("chosen-response",label->second);receipt["key"]=receipt.at("key").get<std::string>()+":"+action.value("choiceId","");return receipt;
    }
    return nullptr;
  }
