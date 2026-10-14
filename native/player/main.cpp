#include <RmlUi/Core.h>
#include "RmlUi_Platform_SDL.h"
#include "RmlUi_Renderer_SDL.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include "raising/runtime.hpp"
#include "json.hpp"
#include "screen.hpp"
#include "audio.hpp"
#include "bundle.hpp"
#include "preview.hpp"
#include "scene-preview.hpp"
#include "extension-element.hpp"
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <iostream>
#include <deque>
#include <chrono>
#include <optional>

class PlayerApplication : public Rml::EventListener {
public:
  ExtensionHost extensions;
  GameSession game;
  ScreenMarkup markup;
  GameAudio audio;
  SDL_Window* window;
  SDL_Renderer* renderer;
  Rml::Context* context=nullptr;
  Rml::ElementDocument* document=nullptr;
  fs::path save_file;
  std::deque<Json> pending;
  std::map<std::string,float> collection_scroll;
  bool dirty=true, running=true, muted=false, backgrounded=false,timeline_manual_clock=false;
  bool editor_events=false;
  double extension_dt=0;
  uint64_t document_generation=0;
  bool syncing_user_inputs=false;
  std::string preview_element,preview_state="normal";
  bool preview() const {return !markup.preview_screen.empty();}
  int auto_revision=-1,auto_attempted=-1;
  double auto_elapsed=0;
  void advance_schedule_clock(double delta) {
    if(preview()||backgrounded||game.state.is_null())return;
    const int revision=game.state.value("stateRevision",-1);
    if(auto_revision!=revision){auto_revision=revision;auto_elapsed=0;}
    const auto command=game.schedule_step();
    if(command.empty()||auto_attempted==revision){auto_elapsed=0;return;}
    auto_elapsed+=std::clamp(delta,0.0,250.0);
    if(auto_elapsed<700)return;
    auto_attempted=revision;execute({{"type","scheduleStep"}});
  }
  void advance_timeline_clock(double delta) {
    if(preview()||!game.timeline_active())return;
    const auto before=game.timeline_cursor;game.tick_timeline(delta);
    if(before.at("clipIndex")!=game.timeline_cursor.at("clipIndex")||before.at("completed")!=game.timeline_cursor.at("completed"))dirty=true;
  }
  void update_timeline_visual() {
    if(!document||!game.timeline_active())return;
    const auto& cursor=game.timeline_cursor;const auto& clips=game.timeline_plan.at("clips");
    const auto& clip=clips.at(std::min(cursor.at("clipIndex").get<size_t>(),clips.size()-1));
    const double opacity=cursor.value("completed",false)||clip.value("transition","none")!="fade"?1:std::min(1.0,cursor.at("elapsedMs").get<double>()/std::min(300.0,clip.at("durationMs").get<double>()));
    if(auto* visual=document->GetElementById("timeline-visual"))visual->SetProperty("opacity",std::to_string(opacity));
    if(clip.contains("stage")) {
      const auto& layers=clip.at("stage").at("layers");const double elapsed=timeline_clip_elapsed(clip,cursor);
      for(size_t i=0;i<layers.size();i++)if(auto* element=document->GetElementById("timeline-stage-layer-"+std::to_string(i))) {
        const auto frame=timeline_layer_frame_index(layers.at(i),elapsed);
        if(element->GetAttribute<int>("data-frame-index",-1)==int(frame))continue;
        const auto& assetId=layers.at(i).at("frames").at(frame).at("imageAssetId");
        element->SetInnerRML(markup.image({{"imageAssetId",assetId},{"fit","fill"}}));
        element->SetAttribute("data-frame-index",int(frame));
        element->SetAttribute("data-image-asset-id",assetId.get<std::string>());
      }
    }
  }
  /** One presentation update per frame, then one frame request per mounted instance. */
  void update_extensions() {
    if(!extensions.active())return;
    const double dt=preview()?0:extension_dt;extension_dt=0;
    extensions.update(dt);extensions.frame();
  }
  void advance_extensions(double delta){if(!extensions.active())return;extensions.update(delta);extensions.frame();}
  void apply_preview() {
    if(!preview()||preview_element.empty())return;
    auto* element=document->GetElementById("node-"+preview_element);
    if(!element)element=document->GetElementById(preview_element);
    if(!element)throw std::runtime_error("Preview element is missing or hidden: "+preview_element);
    auto* target=element;
    if(element->GetTagName()!="button")for(const auto& entry:markup.render_elements)if(entry.value("id","")==preview_element&&entry.value("kind","")=="button"){if(auto* button=element->QuerySelector("button"))target=button;break;}
    if(preview_state!="normal"&&target->GetTagName()!="button")throw std::runtime_error("Preview state requires a button target: "+preview_element);
    target->SetPseudoClass("hover",preview_state=="hover"||preview_state=="pressed");
    target->SetPseudoClass("active",preview_state=="pressed");
    if(preview_state=="focus")target->Focus(true);
    if(preview_state=="disabled"){target->SetAttribute("disabled","disabled");target->SetPseudoClass("disabled",true);}
  }
  Json editor_command=Json::object();
  std::optional<SDL_FingerID> active_finger;
  int lifecycle_saves=0;
  std::string presentation_key;
  std::map<SDL_JoystickID,SDL_Gamepad*> gamepads;
  PlayerApplication(Json bundle, fs::path root, SDL_Window* window, SDL_Renderer* renderer, fs::path save_file)
    :markup(game,bundle.at("assets"),std::move(root)),window(window),renderer(renderer),save_file(std::move(save_file)) {
    extensions.load(bundle,markup.bundle);
    extensions.asset_path=[this](const std::string& id){return markup.asset(Json(id));};
    markup.extensions=&extensions;
    game.observer=[this](const Json& before,const Json& after,const Json& command,const std::string& error){extensions.observe(before,after,command,error);};
    game.extension_state=[this]{return extensions.save();};
    game.content=bundle.at("content"); game.refresh();
#ifdef SDL_PLATFORM_ANDROID
    SDL_AddEventWatch(watch_lifecycle,this);
#endif
  }
  ~PlayerApplication(){
#ifdef SDL_PLATFORM_ANDROID
    SDL_RemoveEventWatch(watch_lifecycle,this);
#endif
    for(auto& [_,pad]:gamepads)SDL_CloseGamepad(pad);
  }
  static bool SDLCALL watch_lifecycle(void* userdata,SDL_Event* event) {
    auto& app=*static_cast<PlayerApplication*>(userdata);
    if(event->type==SDL_EVENT_WILL_ENTER_BACKGROUND||event->type==SDL_EVENT_DID_ENTER_BACKGROUND||event->type==SDL_EVENT_TERMINATING)app.lifecycle(true);
    else if(event->type==SDL_EVENT_DID_ENTER_FOREGROUND)app.lifecycle(false);
    return true;
  }
  fs::path autosave_file() const {return save_file.parent_path()/"autosave.json";}
  void restore_autosave() {
    if(preview())return;
    auto backup=autosave_file();backup+=".bak";
    if(!fs::exists(autosave_file())&&!fs::exists(backup))return;
    bool restored=false;std::string error;
    for(const auto& candidate:{autosave_file(),backup})if(fs::exists(candidate)) {
      try {game.load(candidate);restored=true;break;}catch(const std::exception& failure){error=failure.what();}
    }
    if(restored) {
      extensions.reset();extensions.pending_restore=game.loaded_extensions;
      markup.user_values_draft=Json::object();
      if(!game.state.at("schedule").is_null())for(const auto& id:game.state.at("schedule"))markup.plan.push_back(id.get<std::string>());
      try{audio.sync_music(markup.asset(game.state.at("presentation").value("musicAssetId",Json())),muted);}catch(const std::exception& failure){game.message=failure.what();}
    } else game.message=std::string("자동 저장을 불러오지 못했습니다: ")+error;
    dirty=true;
  }
  void drain_actions(){while(!pending.empty()){const auto action=pending.front();pending.pop_front();execute(action);}}
  void lifecycle(bool paused) {
    if(backgrounded==paused)return;
    if(paused) {
      drain_actions();
      if(context&&active_finger){context->ProcessMouseMove(-1,-1,0);context->ProcessMouseButtonUp(0,0);}
      active_finger.reset();pending.clear();
      if(!preview()&&!game.state.is_null())try{game.save(autosave_file());lifecycle_saves++;}catch(const std::exception& error){game.message=error.what();}
    }
    backgrounded=paused;audio.suspend(paused);dirty=true;
  }
  void ProcessEvent(Rml::Event& event) override {
    if(preview())return;
    if(event.GetType()=="change"||event.GetType()=="input") {
      auto* element=event.GetTargetElement();const auto field=element->GetAttribute<Rml::String>("data-user-field","");
      if(!field.empty()) {
        if(syncing_user_inputs)return;
        if(game.state.is_null())if(auto* input=dynamic_cast<Rml::ElementFormControlInput*>(element)) {
          markup.user_values_draft[field]=input->GetValue();
          syncing_user_inputs=true;
          for(const auto& entry:markup.render_elements)if(entry.value("kind","")=="input"&&entry.value("userFieldId","")==field)if(auto* other=dynamic_cast<Rml::ElementFormControlInput*>(document->GetElementById("user-input-"+entry.value("id",""))))if(other!=input&&other->GetValue()!=input->GetValue())other->SetValue(input->GetValue());
          syncing_user_inputs=false;
          for(const auto& entry:markup.render_elements)if(entry.value("kind","")=="text"&&entry.value("binding","")=="user-field")if(auto* text=document->GetElementById("node-"+entry.value("id","")))text->SetInnerRML(escape(markup.bind("user-field",entry)));
        }
        return;
      }
    }
    if(event.GetType()=="change") {
      auto* element=event.GetTargetElement();std::string id=element->GetId();
      if(id.rfind("roster-",0)==0)if(auto* select=dynamic_cast<Rml::ElementFormControlSelect*>(element)){markup.roster_draft[id.substr(7)]=select->GetValue();dirty=true;}
      if(id.rfind("plan-",0)==0)if(auto* select=dynamic_cast<Rml::ElementFormControlSelect*>(element)) {
        const int index=std::stoi(id.substr(5));if(index>=0&&index<game.periods()&&markup.plan.size()==size_t(game.periods())){
          markup.plan[index]=select->GetValue();
          if(auto* summary=document->GetElementById("plan-summary-"+std::to_string(index))) {
            std::string value;for(const auto& activity:markup.display_content.at("activities"))if(activity.at("id")==markup.plan[index])value=markup.activity_preview(activity);
            summary->SetInnerRML(escape(value));
          }
        }
      }
      return;
    }
    for(auto* element=event.GetTargetElement();element;element=element->GetParentNode()) {
      auto found=markup.actions.find(element->GetId());
      if(found!=markup.actions.end()) {if(!element->HasAttribute("disabled"))pending.push_back(found->second);return;}
    }
  }
  void rebuild() {
    ++document_generation;
    if(document)for(const auto& entry:markup.render_elements)if(entry.value("kind","")=="collection")
      if(auto* element=document->GetElementById("node-"+entry.value("id","")))collection_scroll[markup.screen_id+"/"+entry.value("id","")]=element->GetScrollTop();
    SDL_GetWindowSizeInPixels(window,&markup.viewport_width,&markup.viewport_height);
    const auto source=markup.document();
    if(!context)context=Rml::CreateContext("game",{markup.width,markup.height});
    if(!context)throw std::runtime_error("렌더링 컨텍스트를 만들지 못했습니다.");
    context->SetDimensions({markup.width,markup.height});
    SDL_SetRenderLogicalPresentation(renderer,markup.width,markup.height,SDL_LOGICAL_PRESENTATION_LETTERBOX);
    if(document)document->Close();
    document=context->LoadDocumentFromMemory(source);
    if(!document)throw std::runtime_error("화면 문서를 만들지 못했습니다.");
    document->AddEventListener("click",this);document->AddEventListener("change",this);document->AddEventListener("input",this);document->Show();
    for(int i=0;i<game.periods();i++)if(auto* select=document->GetElementById("plan-"+std::to_string(i))) {
      if(auto* box=select->QuerySelector("selectbox"))box->SetProperty("width",number(std::max(60.f,select->GetProperty<float>("width")))+"px");
      if(auto* arrow=select->QuerySelector("selectarrow"))arrow->SetInnerRML("▾");
    }
    apply_preview();context->Update();
    for(const auto& entry:markup.render_elements)if(entry.value("kind","")=="collection") {
      const auto found=collection_scroll.find(markup.screen_id+"/"+entry.value("id",""));
      if(found!=collection_scroll.end())if(auto* element=document->GetElementById("node-"+entry.value("id","")))element->SetScrollTop(found->second);
    }
    const auto key=markup.screen_id+":"+(game.state.is_null()?"title":game.state.at("presentation").dump());
    if(!preview()&&key!=presentation_key)for(const auto& entry:markup.render_elements) {
      const auto motion=entry.value("motion","none");if(motion=="none")continue;
      auto* element=document->GetElementById("node-"+entry.value("id",""));if(!element)continue;
      const float duration=entry.value("durationMs",300)/1000.f;
      Rml::Property zero(0.f,Rml::Unit::NUMBER);
      element->Animate("opacity",Rml::Property(entry.value("opacity",1.f),Rml::Unit::NUMBER),duration,Rml::Tween{},1,false,0,&zero);
      if(motion=="slide-left"||motion=="slide-right") {
        const float target=entry.value("x",0.f);Rml::Property start(target+(motion=="slide-left"?40.f:-40.f),Rml::Unit::PX);
        element->Animate("left",Rml::Property(target,Rml::Unit::PX),duration,Rml::Tween{},1,false,0,&start);
      } else if(motion=="zoom") {
        for(const auto* property:{"width","height"}){const float target=entry.value(property,1.f);Rml::Property start(target*0.94f,Rml::Unit::PX);element->Animate(property,Rml::Property(target,Rml::Unit::PX),duration,Rml::Tween{},1,false,0,&start);}
      }
    }
    presentation_key=key;dirty=false;
    if(editor_events){std::cout<<Json({{"event","editor-view"},{"command",editor_command},{"view",ui_state()}}).dump()<<std::endl;editor_command=Json::object();}
  }
  void execute(const Json& action) {
    if(preview())return;
    editor_command=action;
    try {
      game.message.clear();const std::string type=action.value("type","");
      if(type=="timelineTick"){advance_timeline_clock(action.value("deltaMs",0.0));return;}
      if(type=="scheduleStep"){const auto command=game.schedule_step();if(!command.empty()){game.send({{"type",command}});game.settle();}dirty=true;return;}
      if(type=="timelinePause"||type=="timelineSpeed"||type=="timelineSkip") {
        if(game.timeline_active()&&!game.timeline_cursor.value("completed",false)) {
          if(type=="timelinePause")game.timeline_cursor["paused"]=!game.timeline_cursor.value("paused",false);
          if(type=="timelineSpeed")game.timeline_cursor["speed"]=game.timeline_cursor.value("speed",1)==1?4:1;
          if(type=="timelineSkip")game.timeline_cursor=skip_timeline(game.timeline_plan,game.timeline_cursor);
        }dirty=true;return;
      }
      if(game.timeline_active()&&type!="save"&&type!="load"&&type!="sound"&&type!="nextLocale"&&!(type=="advance"&&game.timeline_cursor.value("completed",false)))throw std::runtime_error("활동 연출을 마치거나 건너뛴 뒤 계속해 주세요.");
      if(type=="nextLocale"){markup.nextLocale();dirty=true;return;}
      if(!markup.pending_purchase.is_null()&&type!="confirmPurchase"&&type!="cancelPurchase"&&type!="uiBack")return;
      const auto transaction=markup.transaction_definition(action);
      if(transaction.value("confirmation",Json::object()).value("enabled",true)&&((type=="buy"&&(transaction.contains("confirmation")||game.content.at("project").at("visual").value("confirmPurchases",false)))||type=="sell"||type=="useService"||type=="setDiet"||type=="executeAction"||type=="selectPolicy")){
        std::string reason;if(!game.available(action,&reason))throw std::runtime_error(reason);
        markup.pending_purchase=action;markup.pending_confirmation=transaction.value("confirmation",Json::object());markup.pending_revision=game.state.value("stateRevision",0);dirty=true;return;
      }
      if(type=="start") {markup.roster_revision=-1;auto_attempted=-1;auto_revision=-1;game.start(markup.user_values_draft);extensions.reset();markup.quantities.clear();markup.plan.clear();markup.plan_slot=0;markup.plan_group.clear();markup.plan_month=-1;markup.plan_selected=0;}
      else if(type=="quantity"){auto& value=markup.quantities[action.value("key","")];value=std::clamp(value+action.value("delta",0),1,std::max(1,action.value("maximum",1)));}
      else if(type=="confirmPurchase"){auto purchase=markup.pending_purchase;markup.pending_purchase=nullptr;if(!purchase.is_null()){
        if(game.state.value("stateRevision",-1)!=markup.pending_revision)throw std::runtime_error("상태가 변경되었습니다. 내용을 다시 확인해 주세요.");
        game.send(purchase);game.settle();const auto operation=purchase.value("type","");game.message=operation=="buy"?"물건을 구입했습니다.":operation=="sell"?"물건을 판매했습니다.":operation=="setDiet"?"식단 정책을 변경했습니다.":operation=="selectPolicy"?"정책을 변경했습니다.":operation=="executeAction"?"행동을 실행했습니다.":"서비스를 이용했습니다.";
      }}
      else if(type=="cancelPurchase"){markup.pending_purchase=nullptr;}
      else if(type=="planSlot") {markup.plan_slot=std::clamp(action.value("slot",0),0,game.periods()-1);markup.plan_selected&=~(1<<markup.plan_slot);markup.plan_group.clear();}
      else if(type=="planGroup") {markup.plan_group=action.value("group","");}
      else if(type=="planActivity") {
        if(markup.plan.size()!=size_t(game.periods()))throw std::runtime_error("일정 화면을 먼저 열어 주세요.");
        markup.plan[markup.plan_slot]=action.at("activityId").get<std::string>();
        markup.plan_selected|=1<<markup.plan_slot;
        for(int i=1;i<=game.periods();i++){const int next=(markup.plan_slot+i)%game.periods();if(!(markup.plan_selected&(1<<next))){markup.plan_slot=next;break;}}markup.plan_group.clear();
      }
      else if(type=="planReset") {markup.plan_slot=game.state.value("periodIndex",0)%game.periods();markup.plan_selected=(1<<markup.plan_slot)-1;markup.plan_group.clear();}
      else if(type=="rosterCommit")game.send({{"type","assignActivities"},{"assignments",markup.roster_draft}});
      else if(type=="commitPlan") {if(markup.plan_selected!=((1<<game.periods())-1))throw std::runtime_error("이번 달 일정을 모두 선택하세요.");game.send({{"type","setSchedule"},{"activityIds",markup.plan}});game.send({{"type","executeActivity"}});game.settle();}
      else if(type=="uiBack") {
        if(!markup.pending_purchase.is_null())markup.pending_purchase=nullptr;
        else if(!markup.plan_group.empty())markup.plan_group.clear();
        else if(!game.state.is_null()&&(game.state.value("phase","")=="planning"||game.state.value("phase","")=="ready")) {
          const Json* target=nullptr;
          for(const auto& e:markup.render_elements){const auto id=e.value("id","");if((id=="back"||id=="home"||(id.size()>6&&id.substr(id.size()-6)=="__back")||(id.size()>6&&id.substr(id.size()-6)=="__home"))&&markup.visible(e))target=&e;}
          if(target){const auto found=markup.actions.find(target->value("id",""));if(found!=markup.actions.end())game.send(found->second);}
        }
      }
      else if(type=="save")game.save(save_file);
      else if(type=="load") {markup.roster_revision=-1;auto_attempted=-1;auto_revision=-1;game.load(save_file);extensions.reset();extensions.pending_restore=game.loaded_extensions;markup.user_values_draft=Json::object();markup.quantities.clear();markup.plan.clear();markup.plan_month=-1;markup.plan_selected=0;markup.plan_group.clear();markup.pending_purchase=nullptr;if(!game.state.at("schedule").is_null())for(const auto& id:game.state.at("schedule"))markup.plan.push_back(id.get<std::string>());}
      else if(type=="sound") {muted=!muted;game.message=muted?"소리를 껐습니다.":"소리를 켰습니다.";}
      else if(type=="selectBattleAction"||type=="selectBattleTarget"||type=="confirmBattleAction"||type=="cancelBattleAction") {
        const auto command=game.battle_ui(action);if(!command.is_null()){game.send(command);game.settle();}
      }
      else if(type=="advance") {
        if(game.state.is_null())throw std::runtime_error("새 게임을 시작해 주세요.");
        const auto phase=game.state.value("phase","");
        if(phase=="planning"||phase=="ready") {
          if(game.content.at("project").contains("roster")) {
            if(phase=="planning")game.send({{"type","assignActivities"},{"assignments",markup.roster_draft}});
            else game.send({{"type","executeActivity"}});
            game.settle();dirty=true;return;
          }
          if(markup.plan.size()!=size_t(game.periods()))throw std::runtime_error("이번 달 일정을 선택해 주세요.");
          if(phase=="planning"||game.state.at("schedule")!=Json(markup.plan))game.send({{"type","setSchedule"},{"activityIds",markup.plan}});
          else game.send({{"type","executeActivity"}});
        } else if(phase=="activity-result")game.send({{"type","acknowledgeResult"}});
        else if(phase=="period-end")game.send({{"type","resolvePeriodEnd"}});
        else game.send({{"type","advanceNarrative"}});
        game.settle();
      } else {
        const auto before=game.state;game.send(action);game.settle();
        if(type=="interact") {
          std::string feedback="";
          for(const auto& a:game.content.at("attributes")){const auto id=a.at("id").get<std::string>();const int delta=game.state.at("attributes").value(id,0)-before.at("attributes").value(id,0);if(delta)feedback+=a.value("label",id)+" "+(delta>0?"+":"")+std::to_string(delta)+"  ";}
          for(const auto& c:game.content.at("characters"))for(const auto& r:c.at("relationships")){auto cid=c.at("id").get<std::string>(),rid=r.at("id").get<std::string>();const int delta=game.state.at("relationships").at(cid).value(rid,0)-before.at("relationships").at(cid).value(rid,0);if(delta)feedback+=r.value("label",rid)+" "+(delta>0?"+":"")+std::to_string(delta)+"  ";}
          game.message=feedback.empty()?"이야기를 나누었습니다.":feedback;
        }
      }
      const auto presentation=game.state.is_null()?game.content.at("project").at("visual").value("defaultScene",Json::object()):game.state.at("presentation");
      audio.sync_music(markup.asset(presentation.value("musicAssetId",Json())),muted);
      for(const auto& cue:game.sound_cues)audio.effect(markup.asset(cue),muted);
      game.sound_cues.clear();
    } catch(const std::exception& error) {game.message=error.what();game.sound_cues.clear();}
    dirty=true;
  }
  void event(SDL_Event& event) {
    if(event.type==SDL_EVENT_QUIT)running=false;
    if(event.type==SDL_EVENT_WILL_ENTER_BACKGROUND||event.type==SDL_EVENT_DID_ENTER_BACKGROUND||event.type==SDL_EVENT_DID_ENTER_FOREGROUND||event.type==SDL_EVENT_TERMINATING) {
#ifndef SDL_PLATFORM_ANDROID
      watch_lifecycle(this,&event);
#endif
      return;
    }
    if(backgrounded)return;
    if(event.type==SDL_EVENT_FINGER_DOWN||event.type==SDL_EVENT_FINGER_MOTION||event.type==SDL_EVENT_FINGER_UP||event.type==SDL_EVENT_FINGER_CANCELED) {
      if(!context)return;
      const auto& finger=event.tfinger;
      int width=0,height=0;SDL_GetWindowSize(window,&width,&height);float x=0,y=0;
      if(!SDL_RenderCoordinatesFromWindow(renderer,finger.x*width,finger.y*height,&x,&y))return;
      if(event.type==SDL_EVENT_FINGER_DOWN) {
        if(active_finger||x<0||y<0||x>=markup.width||y>=markup.height)return;
        active_finger=finger.fingerID;context->ProcessMouseMove(static_cast<int>(x),static_cast<int>(y),0);context->ProcessMouseButtonDown(0,0);
      } else if(active_finger&&*active_finger==finger.fingerID) {
        if(event.type==SDL_EVENT_FINGER_CANCELED)context->ProcessMouseMove(-1,-1,0);
        else context->ProcessMouseMove(static_cast<int>(x),static_cast<int>(y),0);
        if(event.type!=SDL_EVENT_FINGER_MOTION){context->ProcessMouseButtonUp(0,0);active_finger.reset();}
      }
      return;
    }
    // Keep physical dimensions out of the simulation. Responsive layouts rebuild
    // the logical canvas; legacy fit layouts retain their authored dimensions.
    if(event.type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED||event.type==SDL_EVENT_WINDOW_RESIZED||event.type==SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
      if(game.content.at("project").at("visual").value("viewportMode","fit")=="responsive")dirty=true;
      if(context){context->SetDimensions({markup.width,markup.height});context->SetDensityIndependentPixelRatio(1.f);}return;
    }
    if(event.type==SDL_EVENT_GAMEPAD_ADDED){auto* pad=SDL_OpenGamepad(event.gdevice.which);if(pad)gamepads[event.gdevice.which]=pad;return;}
    if(event.type==SDL_EVENT_GAMEPAD_REMOVED){auto it=gamepads.find(event.gdevice.which);if(it!=gamepads.end()){SDL_CloseGamepad(it->second);gamepads.erase(it);}return;}
    if(event.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN&&context) {
      const auto button=event.gbutton.button;
      if(button==SDL_GAMEPAD_BUTTON_DPAD_DOWN||button==SDL_GAMEPAD_BUTTON_DPAD_RIGHT||button==SDL_GAMEPAD_BUTTON_DPAD_UP||button==SDL_GAMEPAD_BUTTON_DPAD_LEFT) {
        const int modifier=button==SDL_GAMEPAD_BUTTON_DPAD_UP||button==SDL_GAMEPAD_BUTTON_DPAD_LEFT?Rml::Input::KM_SHIFT:0;
        context->ProcessKeyDown(Rml::Input::KI_TAB,modifier);context->ProcessKeyUp(Rml::Input::KI_TAB,modifier);
      } else if(button==SDL_GAMEPAD_BUTTON_SOUTH){context->ProcessKeyDown(Rml::Input::KI_RETURN,0);context->ProcessKeyUp(Rml::Input::KI_RETURN,0);}
      return;
    }
    if(event.type==SDL_EVENT_KEY_DOWN) {
      if(event.key.key==SDLK_ESCAPE&&game.content.at("project").at("visual").value("widgetTheme","default")=="ornate"){pending.push_back({{"type","uiBack"}});return;}
      if(event.key.key==SDLK_AC_BACK&&context){context->ProcessKeyDown(Rml::Input::KI_ESCAPE,0);context->ProcessKeyUp(Rml::Input::KI_ESCAPE,0);return;}
      if(event.key.key==SDLK_F5){pending.push_back({{"type","save"}});return;}
      if(event.key.key==SDLK_F2){pending.push_back({{"type","nextLocale"}});return;}
      if(event.key.key==SDLK_F9){pending.push_back({{"type","load"}});return;}
    }
    if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN&&event.button.button==SDL_BUTTON_RIGHT&&game.content.at("project").at("visual").value("widgetTheme","default")=="ornate"){pending.push_back({{"type","uiBack"}});return;}
    SDL_ConvertEventToRenderCoordinates(renderer,&event);
    if(context)RmlSDL::InputEventHandler(context,window,event);
  }
  void screenshot(const fs::path& file) {
    fs::create_directories(file.parent_path());
    auto* surface=SDL_RenderReadPixels(renderer,nullptr);
    if(!surface)throw std::runtime_error(SDL_GetError());
    const bool ok=IMG_SavePNG(surface,file.u8string().c_str());SDL_DestroySurface(surface);
    if(!ok)throw std::runtime_error(SDL_GetError());
  }
  Json ui_state() {
    SDL_FRect presentation{};SDL_GetRenderLogicalPresentationRect(renderer,&presentation);int pixel_width=0,pixel_height=0;SDL_GetRenderOutputSize(renderer,&pixel_width,&pixel_height);
    Json elements=Json::array();
    for(const auto& [id,label]:markup.labels)if(auto* element=document->GetElementById(id)) {
      auto offset=element->GetAbsoluteOffset();auto size=element->GetBox().GetSize();
      elements.push_back({{"id",id},{"label",label},{"enabled",!element->HasAttribute("disabled")},
        {"x",offset.x},{"y",offset.y},{"width",size.x},{"height",size.y},
        {"fontFamily",element->GetProperty("font-family")->ToString()},
        {"fontSize",element->GetProperty("font-size")->ToString()},
        {"color",element->GetProperty("color")->ToString()}});
    }
    Json controls=Json::array(),options=Json::array();for(int i=0;i<game.periods();i++)if(auto* select=dynamic_cast<Rml::ElementFormControlSelect*>(document->GetElementById("plan-"+std::to_string(i)))){
      auto offset=select->GetAbsoluteOffset();auto size=select->GetBox().GetSize();controls.push_back({{"id",select->GetId()},{"value",select->GetValue()},{"x",offset.x},{"y",offset.y},{"width",size.x},{"height",size.y}});
      for(int j=0;j<select->GetNumOptions();j++)if(auto* option=select->GetOption(j)) {
        auto point=option->GetAbsoluteOffset();auto extent=option->GetBox().GetSize();auto* box=option->GetParentNode();auto clip=box->GetAbsoluteOffset();auto bounds=box->GetBox().GetSize();
        bool visible=option->IsVisible(true)&&point.x>=0&&point.y>=0&&point.x+extent.x<=markup.width&&point.y+extent.y<=markup.height&&point.y>=clip.y&&point.y+extent.y<=clip.y+bounds.y;
        options.push_back({{"id",option->GetId()},{"visible",visible},{"x",point.x},{"y",point.y},{"width",extent.x},{"height",extent.y},{"clipY",clip.y},{"clipHeight",bounds.y}});
      }
    }
    Json texts=Json::object();for(const auto& entry:markup.render_elements)if(entry.value("kind","")=="schedule"||entry.value("kind","")=="text"||entry.value("kind","")=="dialogue"||entry.value("kind","")=="equipment"||entry.value("kind","")=="quests"||entry.value("kind","")=="expedition"||entry.value("kind","")=="battle"||entry.value("kind","")=="relationship-stages"||entry.value("kind","")=="plugin") {
      const auto id=entry.value("id","");if(auto* element=document->GetElementById("node-"+id))texts[id]=element->GetInnerRML();
    }
    if(auto* fallback=document->GetElementById("node-system-widget"))texts["system-widget"]=fallback->GetInnerRML();
    Json result_layout=Json::array();
    for(const auto& entry:markup.render_elements)if(entry.value("kind","")=="result")if(auto* element=document->GetElementById("node-"+entry.value("id",""))){
      texts[entry.value("id","")]=element->GetInnerRML();
      for(int i=0;i<element->GetNumChildren();i++){auto* child=element->GetChild(i);auto point=child->GetAbsoluteOffset();auto size=child->GetBox().GetSize();result_layout.push_back({{"tag",child->GetTagName()},{"width",size.x},{"height",size.y},{"x",point.x},{"y",point.y}});}
    }
    Json nodes=Json::array();size_t runtime_button_count=0;bool runtime_buttons_truncated=false;
    for(const auto& entry:markup.render_elements){
      auto* element=document->GetElementById("node-"+entry.value("id",""));
      nodes.push_back(rendered_node_report(entry,element,markup.width,markup.height,context));
      if(element&&entry.value("kind","")=="collection") {
        nodes.back()["scrollTop"]=element->GetScrollTop();nodes.back()["rowCount"]=entry.value("_collectionCount",0);
      }
      if(entry.contains("_collectionParent")){nodes.back()["collectionId"]=entry.at("_collectionParent");nodes.back()["rowId"]=entry.at("_rowId");}
      if(element&&entry.value("kind","")!="button"){
        Rml::ElementList buttons;element->QuerySelectorAll(buttons,"button");
        for(auto* button:buttons){if(runtime_button_count++>=1024){runtime_buttons_truncated=true;break;}
          const auto id=button->GetId();const auto label=markup.labels.find(id);
          nodes.push_back(rendered_node_report({{"id",id},{"kind","runtime-button"},{"ownerId",entry.value("id","")},{"label",label==markup.labels.end()?id:label->second}},button,markup.width,markup.height,context));
        }
      }
    }
    Json timelineStage=nullptr;
    if(auto* stage=document->GetElementById("timeline-stage")) {
      const auto point=stage->GetAbsoluteOffset(),size=stage->GetBox().GetSize();Json layers=Json::array();
      for(int i=0;i<stage->GetNumChildren();i++) {
        auto* layer=stage->GetChild(i);const auto at=layer->GetAbsoluteOffset(),extent=layer->GetBox().GetSize();
        layers.push_back({{"id",layer->GetAttribute<Rml::String>("data-layer-id","")},{"frameIndex",layer->GetAttribute<int>("data-frame-index",-1)},
          {"imageAssetId",layer->GetAttribute<Rml::String>("data-image-asset-id","")},{"x",at.x},{"y",at.y},{"width",extent.x},{"height",extent.y},
          {"zIndex",layer->GetProperty("z-index")->ToString()},{"rml",layer->GetInnerRML()}});
      }
      timelineStage={{"x",point.x},{"y",point.y},{"width",size.x},{"height",size.y},{"clip",stage->GetProperty("clip")->ToString()},
        {"overflow",stage->GetProperty("overflow-x")->ToString()},{"layers",layers}};
    }
    Json input_values=Json::object();for(const auto& entry:markup.render_elements)if(entry.value("kind","")=="input")if(auto* input=dynamic_cast<Rml::ElementFormControlInput*>(document->GetElementById("user-input-"+entry.value("id",""))))input_values[entry.value("id","")]={{"value",input->GetValue()},{"disabled",input->HasAttribute("disabled")}};
    return {{"screenId",markup.screen_id},{"state",game.state},{"message",game.message},{"elements",elements},{"texts",texts},{"controls",controls},{"options",options},{"nodes",nodes},{"timelineStage",timelineStage},{"userInputs",input_values},{"userDrafts",markup.user_values_draft},{"documentGeneration",document_generation},{"viewport",{{"width",markup.width},{"height",markup.height}}},{"focusedId",context->GetFocusElement()?context->GetFocusElement()->GetId():""},
      {"preview",{{"active",preview()},{"isolated",preview()},{"screenId",markup.preview_screen},{"elementId",preview_element},{"state",preview_state},{"stateSource","initial"},{"sceneOverride",!markup.preview_scene.is_null()},{"displayScene",markup.display_scene()},{"measurement","rmlui-layout-boxes"}}},
      {"presentation",{{"x",presentation.x},{"y",presentation.y},{"width",presentation.w},{"height",presentation.h},{"pixelWidth",pixel_width},{"pixelHeight",pixel_height}}},
      {"runtimeButtonsTruncated",runtime_buttons_truncated},{"timeline",game.timeline_active()?Json{{"plan",game.timeline_plan},{"cursor",game.timeline_cursor}}:Json()},{"battleSelection",game.battle_selection},
      {"locale",markup.locale},{"resultLayout",result_layout},{"audio",{{"initialized",audio.ready()},{"musicPlaying",audio.playing()},{"loadedSounds",audio.loaded_count()},{"muted",muted}}},{"systems",game.projection.value("systems",Json())},{"backgrounded",backgrounded},{"lifecycleSaves",lifecycle_saves},{"extensions",extensions.report()},{"runtime","cpp-sdl3-rmlui"}};
  }
  Rml::Element* find_element(const std::string& id) {
    if(auto* element=document->GetElementById(id))return element;
    // RmlUi keeps dropdown options under non-DOM widget children.
    for(int i=0;i<game.periods();i++)if(auto* select=dynamic_cast<Rml::ElementFormControlSelect*>(document->GetElementById("plan-"+std::to_string(i))))for(int j=0;j<select->GetNumOptions();j++)if(auto* option=select->GetOption(j);option&&option->GetId()==id)return option;
    return nullptr;
  }
  void click(const std::string& id) {
    auto* element=find_element(id);if(!element)throw std::runtime_error("UI element missing: "+id);
    // Test-driver convenience, like browser locator.click(): reveal the target
    // first, then use the real pointer dispatch and hit testing below.
    element->ScrollIntoView(true);context->Update();
    auto position=element->GetAbsoluteOffset();auto size=element->GetBox().GetSize();
    const int x=int(position.x+size.x/2),y=int(position.y+size.y/2);
    // Identical RmlUi input path used by SDL pointer events, after logical-coordinate conversion.
    context->ProcessMouseMove(x,y,0);context->ProcessMouseButtonDown(0,0);context->ProcessMouseButtonUp(0,0);
  }
  void window_click(const std::string& id,bool touch=false,bool cancel=false) {
    auto* element=find_element(id);if(!element)throw std::runtime_error("UI element missing: "+id);
    auto position=element->GetAbsoluteOffset();auto size=element->GetBox().GetSize();float x,y;
    if(!element->IsVisible(true))throw std::runtime_error("UI element is hidden: "+id);
    if(element->GetTagName()=="option") {auto* box=element->GetParentNode();auto clip=box->GetAbsoluteOffset();auto bounds=box->GetBox().GetSize();if(position.y<clip.y||position.y+size.y>clip.y+bounds.y)throw std::runtime_error("UI option is clipped; scroll it into view first: "+id);}
    if(!SDL_RenderCoordinatesToWindow(renderer,position.x+size.x/2,position.y+size.y/2,&x,&y))throw std::runtime_error(SDL_GetError());
    int window_width,window_height;SDL_GetWindowSize(window,&window_width,&window_height);
    if(x<0||y<0||x>=window_width||y>=window_height)throw std::runtime_error("UI element is outside the window: "+id);
    const auto window_id=SDL_GetWindowID(window);
    if(touch) {
      SDL_Event input{};input.type=SDL_EVENT_FINGER_DOWN;input.tfinger.windowID=window_id;input.tfinger.touchID=1;input.tfinger.fingerID=1;input.tfinger.x=x/window_width;input.tfinger.y=y/window_height;event(input);
      input.type=cancel?SDL_EVENT_FINGER_CANCELED:SDL_EVENT_FINGER_UP;event(input);return;
    }
    SDL_Event input{};input.type=SDL_EVENT_MOUSE_MOTION;input.motion.windowID=window_id;input.motion.x=x;input.motion.y=y;event(input);
    input={};input.type=SDL_EVENT_MOUSE_BUTTON_DOWN;input.button.windowID=window_id;input.button.button=SDL_BUTTON_LEFT;input.button.x=x;input.button.y=y;event(input);
    input={};input.type=SDL_EVENT_MOUSE_BUTTON_UP;input.button.windowID=window_id;input.button.button=SDL_BUTTON_LEFT;input.button.x=x;input.button.y=y;event(input);
  }
};

int main(int argc,char** argv) {
#ifdef SDL_PLATFORM_ANDROID
  const auto* internal=SDL_GetAndroidInternalStoragePath();
  fs::path bundle_root=internal?fs::u8path(internal)/"game":fs::path("game"),save_file,automation_file,report_file;
#else
  fs::path bundle_root=fs::u8path(SDL_GetBasePath()),save_file,automation_file,report_file;
#endif
  int max_frames=0;bool hidden=false,editor_events=false;uint32_t requested_seed=42;
  std::string requested_locale,preview_screen,preview_element,preview_state="normal";
  fs::path preview_scene_file;Json preview_scene=nullptr;
  try {
  for(int i=1;i<argc;i++) {
    const std::string arg=argv[i];
    if(arg=="--bundle"&&i+1<argc)bundle_root=fs::u8path(argv[++i]);
    else if(arg=="--locale"&&i+1<argc)requested_locale=argv[++i];
    else if(arg=="--preview-screen"&&i+1<argc)preview_screen=argv[++i];
    else if(arg=="--preview-scene"&&i+1<argc)preview_scene_file=fs::u8path(argv[++i]);
    else if(arg=="--preview-element"&&i+1<argc)preview_element=argv[++i];
    else if(arg=="--preview-state"&&i+1<argc)preview_state=argv[++i];
    else if(arg=="--save"&&i+1<argc)save_file=fs::u8path(argv[++i]);
    else if(arg=="--automation"&&i+1<argc)automation_file=fs::u8path(argv[++i]);
    else if(arg=="--report"&&i+1<argc)report_file=fs::u8path(argv[++i]);
    else if(arg=="--seed"&&i+1<argc){const std::string value=argv[++i];size_t used=0;const auto parsed=std::stoull(value,&used);if(used!=value.size()||parsed>UINT32_MAX)throw std::runtime_error("Seed must be an unsigned 32-bit integer");requested_seed=static_cast<uint32_t>(parsed);}
    else if(arg=="--frames"&&i+1<argc)max_frames=std::stoi(argv[++i]);
    else if(arg=="--hidden")hidden=true;
    else if(arg=="--editor-events")editor_events=true;
    else if(arg=="--help"){std::cout<<"Raising Player --bundle FOLDER [--save FILE] [--seed UINT32] [--automation FILE --report FILE] [--frames N] [--hidden] [--preview-screen ID --preview-element ID --preview-state normal|hover|pressed|focus|disabled]\n";return 0;}
  }
    const auto bundle=read_json(bundle_root/"game.json");
    validate_bundle(bundle,bundle_root);
    if(!preview_scene_file.empty()) {
      if(preview_screen.empty())throw std::runtime_error("Preview scene requires --preview-screen");
      preview_scene=read_preview_scene(preview_scene_file,bundle.at("content"));
    }
    if(preview_state!="normal"&&preview_state!="hover"&&preview_state!="pressed"&&preview_state!="focus"&&preview_state!="disabled")throw std::runtime_error("Unknown preview state: "+preview_state);
    if(preview_screen.empty()&&(!preview_element.empty()||preview_state!="normal"))throw std::runtime_error("Preview styling requires --preview-screen");
    if(preview_state!="normal"&&preview_element.empty())throw std::runtime_error("Preview styling requires --preview-element");
    if(!preview_screen.empty()){bool found=false;for(const auto& screen:bundle.at("content").at("project").at("visual").at("screens"))if(screen.at("id")==preview_screen)found=true;if(!found)throw std::runtime_error("Unknown preview screen: "+preview_screen);}
    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS,"0");
#ifdef SDL_PLATFORM_ANDROID
    const auto& visual=bundle.at("content").at("project").at("visual");
    SDL_SetHint(SDL_HINT_ORIENTATIONS,visual.at("width").get<int>()>=visual.at("height").get<int>()?"LandscapeLeft LandscapeRight":"Portrait PortraitUpsideDown");
#endif
    if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD))throw std::runtime_error(SDL_GetError());
    if(save_file.empty()) {auto* pref=SDL_GetPrefPath("RaisingEngine",bundle.at("content").at("project").value("gameId","game").c_str());if(!pref)throw std::runtime_error(SDL_GetError());save_file=fs::u8path(pref)/"slot-1.json";SDL_free(pref);}
    auto flags=SDL_WINDOW_RESIZABLE|(hidden?SDL_WINDOW_HIDDEN:0);
    auto* window=SDL_CreateWindow(bundle.at("content").at("project").value("title","Raising Player").c_str(),1280,720,flags);
    if(!window)throw std::runtime_error(SDL_GetError());
    auto* renderer=SDL_CreateRenderer(window,nullptr);if(!renderer)throw std::runtime_error(SDL_GetError());
    SDL_SetRenderVSync(renderer,1);
    {
      SystemInterface_SDL system;system.SetWindow(window);RenderInterface_SDL render(renderer);
      Rml::SetSystemInterface(&system);Rml::SetRenderInterface(&render);if(!Rml::Initialise())throw std::runtime_error("RmlUi initialization failed");
      static Rml::ElementInstancerGeneric<ExtensionElement> extension_instancer;Rml::Factory::RegisterElementInstancer("raising-extension",&extension_instancer);
#ifdef SDL_PLATFORM_ANDROID
      const auto font=bundle_root/"fonts/NotoSansKR.ttf";
#else
      const auto font=fs::u8path(SDL_GetBasePath())/"fonts/NotoSansKR.ttf";
#endif
      if(!Rml::LoadFontFace(font.generic_u8string(),true))throw std::runtime_error("Cannot load bundled Korean font");
      std::map<std::string,std::vector<Rml::byte>> font_buffers;
      for(const auto& asset:bundle.at("content").at("assets"))if(asset.value("kind","")=="font") {
        const auto id=asset.at("id").get<std::string>();const auto file=bundle_root/fs::u8path(bundle.at("assets").at(id).get<std::string>());
        std::ifstream input(file,std::ios::binary);auto& bytes=font_buffers[id];bytes.assign(std::istreambuf_iterator<char>(input),{});
        if(bytes.empty()||!Rml::LoadFontFace(Rml::Span<const Rml::byte>(bytes.data(),bytes.size()),"engine-"+id,Rml::Style::FontStyle::Normal))throw std::runtime_error("Cannot load project font: "+id);
      }
      PlayerApplication app(bundle,bundle_root,window,renderer,save_file);
      ExtensionElement::host=&app.extensions;
      app.game.seed=requested_seed;
      if(!preview_screen.empty()){app.markup.preview_screen=preview_screen;app.markup.preview_scene=preview_scene;app.preview_element=preview_element;app.preview_state=preview_state;if(preview_screen!="title"||!preview_scene.is_null())app.game.start();}
      if(!requested_locale.empty()){app.markup.locale=requested_locale;app.dirty=true;}
      app.editor_events=editor_events;
#ifdef SDL_PLATFORM_ANDROID
      app.restore_autosave();
#endif
      auto automation=automation_file.empty()?Json::array():read_json(automation_file);size_t step=0;int frames=0,wait_frames=0;Json report={{"steps",Json::array()}};uint64_t timeline_clock=SDL_GetTicks();
      app.timeline_manual_clock=!automation.empty();
      while(app.running) {
        const auto timeline_now=SDL_GetTicks();const double timeline_delta=double(timeline_now-timeline_clock);timeline_clock=timeline_now;
        SDL_Event event;while(SDL_PollEvent(&event))app.event(event);
        if(app.backgrounded){SDL_Delay(50);continue;}
        app.drain_actions();
        if(!app.timeline_manual_clock){app.advance_timeline_clock(timeline_delta);app.advance_schedule_clock(timeline_delta);app.extension_dt+=timeline_delta;}
        if(app.dirty)app.rebuild();
        app.apply_preview();app.update_timeline_visual();app.update_extensions();app.context->Update();SDL_SetRenderDrawColor(renderer,19,35,28,255);SDL_RenderClear(renderer);render.BeginFrame();app.context->Render();render.EndFrame();
        if(wait_frames>0)--wait_frames;
        else if(!automation.empty()&&frames%3==2&&step<automation.size()) {
          const auto command=automation[step++];
          if(command.contains("click"))app.click(command.at("click"));
          if(command.contains("clickWindow"))app.window_click(command.at("clickWindow"));
          if(command.contains("textInput")){const auto text=command.at("textInput").get<std::string>();SDL_Event input{};input.type=SDL_EVENT_TEXT_INPUT;input.text.windowID=SDL_GetWindowID(window);input.text.text=text.c_str();app.event(input);}
          if(command.contains("wheelWindow")) {
            const auto wheel=command.at("wheelWindow");SDL_Event input{};input.type=SDL_EVENT_MOUSE_MOTION;input.motion.windowID=SDL_GetWindowID(window);input.motion.x=wheel.at("x");input.motion.y=wheel.at("y");app.event(input);
            input={};input.type=SDL_EVENT_MOUSE_WHEEL;input.wheel.windowID=SDL_GetWindowID(window);input.wheel.y=wheel.at("deltaY");input.wheel.mouse_x=wheel.at("x");input.wheel.mouse_y=wheel.at("y");input.wheel.direction=SDL_MOUSEWHEEL_NORMAL;app.event(input);
          }
          if(command.contains("touchWindow"))app.window_click(command.at("touchWindow"),true,command.value("cancelTouch",false));
          if(command.contains("lifecycle"))for(const auto& transition:command.at("lifecycle")) {
            SDL_Event lifecycle{};const auto phase=transition.get<std::string>();
            if(phase!="background"&&phase!="foreground")throw std::runtime_error("Unknown lifecycle transition");
            lifecycle.type=phase=="background"?SDL_EVENT_WILL_ENTER_BACKGROUND:SDL_EVENT_DID_ENTER_FOREGROUND;
            PlayerApplication::watch_lifecycle(&app,&lifecycle);
          }
          if(command.value("restoreAutosave",false))app.restore_autosave();
          if(command.contains("key")) {
            const auto key=command.at("key").get<std::string>();const auto code=key=="Tab"||key=="ShiftTab"?SDLK_TAB:key=="Enter"?SDLK_RETURN:key=="Escape"?SDLK_ESCAPE:key=="F2"?SDLK_F2:key=="F5"?SDLK_F5:key=="F9"?SDLK_F9:key=="ArrowDown"?SDLK_DOWN:key=="ArrowUp"?SDLK_UP:key=="Space"?SDLK_SPACE:key=="Backspace"?SDLK_BACKSPACE:key=="CtrlA"?SDLK_A:SDLK_UNKNOWN;
            if(code==SDLK_UNKNOWN)throw std::runtime_error("Unknown automation key: "+key);
            const auto previous_modifiers=SDL_GetModState();const auto modifiers=key=="ShiftTab"?SDL_KMOD_SHIFT:key=="CtrlA"?SDL_KMOD_CTRL:SDL_KMOD_NONE;SDL_SetModState(modifiers);
            SDL_Event input{};input.type=SDL_EVENT_KEY_DOWN;input.key.key=code;input.key.mod=modifiers;app.event(input);input.type=SDL_EVENT_KEY_UP;app.event(input);SDL_SetModState(previous_modifiers);
          }
          if(command.contains("waitFrames"))wait_frames=command.at("waitFrames").get<int>();
          if(command.contains("timelineRealtime")){app.timeline_manual_clock=!command.at("timelineRealtime").get<bool>();app.extension_dt=0;}
          if(command.contains("timelineAdvanceMs")){app.advance_timeline_clock(command.at("timelineAdvanceMs").get<double>());app.advance_extensions(command.at("timelineAdvanceMs").get<double>());}
          // Presentation-only clock step for extensions (deterministic under the manual automation clock).
          if(command.contains("extensionAdvanceMs"))for(int i=0,n=command.value("extensionSteps",1);i<std::max(1,n);i++)app.advance_extensions(command.at("extensionAdvanceMs").get<double>()/std::max(1,n));
          if(command.contains("resize")){const auto size=command.at("resize");SDL_SetWindowSize(window,size.at("width"),size.at("height"));}
          if(command.contains("select")) {
            auto* select=dynamic_cast<Rml::ElementFormControlSelect*>(app.document->GetElementById(command.at("select")));
            if(!select)throw std::runtime_error("Select missing");select->SetValue(command.at("value"));select->DispatchEvent("change",{{"value",Rml::Variant(command.at("value").get<std::string>())}});
          }
          if(command.contains("screenshot"))app.screenshot(fs::u8path(command.at("screenshot").get<std::string>()));
          if(command.contains("expectScreen")&&app.markup.screen_id!=command.at("expectScreen"))throw std::runtime_error("Unexpected screen: "+app.markup.screen_id);
          report["steps"].push_back({{"command",command},{"ui",app.ui_state()}});
        }
        SDL_RenderPresent(renderer);frames++;SDL_Delay(8);
        if(max_frames&&frames>=max_frames)break;
        if(!automation.empty()&&step==automation.size()&&frames%3==0&&app.pending.empty()&&wait_frames==0)break;
      }
      report["final"]=app.ui_state();report["ok"]=true;
      if(!report_file.empty()){fs::create_directories(report_file.parent_path());std::ofstream out(report_file);out<<report.dump(2);}
      if(app.document)app.document->Close();Rml::Shutdown();
    }
    SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
  } catch(const std::exception& error) {
    std::cerr<<error.what()<<std::endl;
    if(!report_file.empty()){fs::create_directories(report_file.parent_path());std::ofstream out(report_file);out<<Json({{"ok",false},{"error",error.what()}}).dump(2);}
    if(!hidden)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Raising Player",error.what(),nullptr);
    SDL_Quit();return 1;
  }
}

