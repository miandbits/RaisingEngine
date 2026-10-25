  std::string resume_context_return_view,resume_context_return_focus;
  void reset_resume_context(const std::string& reason) {
    markup.resume_context_open=false;resume_context_return_view.clear();resume_context_return_focus.clear();
    if(reason!="restored"||preview()||editor_live||recalling()||game.gallery.active)return;
    const auto config=game.content.at("project").at("visual").value("resumeContext",Json::object());
    if(!config.value("showAfterLoad",false))return;
    markup.reading_options=markup.reading_menu=markup.compact_picker=markup.planning_guide=false;
    markup.resume_context_open=true;reading_focus="engine-resume-context-current";reading_jump="engine-resume-context-panel";
    pending.clear();
  }
  bool resume_context_action(const Json& action) {
    const auto type=action.value("type","");
    if(markup.resume_context_open){
      if(type=="resumeContextClose"||type=="uiBack"){
        markup.resume_context_open=false;markup.reading_options=resume_context_return_view=="options";markup.reading_menu=resume_context_return_view=="menu";
        reading_focus=resume_context_return_focus;dirty=true;
      }
      return true;
    }
    if(type=="resumeContextClose")return true;
    if(type!="resumeContext")return false;
    if(!markup.resume_context_enabled()||preview()||editor_live||recalling()||game.gallery.active||backgrounded||markup.compact_picker||markup.planning_guide||markup.plan_reuse_open||markup.reading_history_open||!markup.pending_purchase.is_null())return true;
    resume_context_return_view=markup.reading_options?"options":markup.reading_menu?"menu":"game";
    resume_context_return_focus=context&&context->GetFocusElement()?context->GetFocusElement()->GetId():"";
    markup.reading_options=markup.reading_menu=false;markup.resume_context_open=true;
    reading_focus="engine-resume-context-current";reading_jump="engine-resume-context-panel";dirty=true;return true;
  }
