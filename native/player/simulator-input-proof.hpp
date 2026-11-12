#pragma once
#include <array>
#include <SDL3/SDL.h>
#include "json.hpp"

// Diagnostic-only, consumed SDL events. No event synthesis, filtering, allocation,
// output, hit testing, or geometry query in observe()/converted(). observe()
// performs the same read-only SDL_GetTouchDeviceType lookup in S and E. They share
// this exact layer. It cannot observe unconsumed events or all UIKit callbacks.
struct SimulatorInputProof {
  struct Sample {
    Uint32 type=0; Uint64 timestamp=0,observedAt=0;
    SDL_TouchID touch=0; SDL_FingerID finger=0; SDL_WindowID window=0;
    SDL_TouchDeviceType device=SDL_TOUCH_DEVICE_INVALID;
    float x=0,y=0,mappedX=0,mappedY=0; int width=0,height=0; Uint8 button=0;
    bool converted=false;
  };
  struct Event {
    Uint32 type=0; Uint64 timestamp=0,observedAt=0;
    SDL_WindowID window=0; Sint32 data1=0,data2=0;
  };
  struct Geometry {
    Uint64 at=0,flags=0; SDL_WindowID window=0;
    int width=0,height=0,pixelWidth=0,pixelHeight=0; SDL_Rect safe{}; SDL_FRect presentation{};
    bool valid=false;
  };
  std::array<Sample,64> samples{};
  std::array<Event,512> events{};
  size_t count=0,eventCount=0; Uint64 firstDown=0;
  bool enabled=false,terminal=false,overflow=false,eventOverflow=false,emitted=false;
  Geometry initial{};
  static bool touch(Uint32 type) {
    return type==SDL_EVENT_FINGER_DOWN||type==SDL_EVENT_FINGER_MOTION||type==SDL_EVENT_FINGER_UP||type==SDL_EVENT_FINGER_CANCELED;
  }
  static bool relevant(Uint32 type) {
    return touch(type)||type==SDL_EVENT_MOUSE_BUTTON_DOWN||type==SDL_EVENT_MOUSE_BUTTON_UP||
      (type>=SDL_EVENT_WINDOW_FIRST&&type<=SDL_EVENT_WINDOW_LAST)||
      type==SDL_EVENT_WILL_ENTER_BACKGROUND||type==SDL_EVENT_DID_ENTER_BACKGROUND||
      type==SDL_EVENT_WILL_ENTER_FOREGROUND||type==SDL_EVENT_DID_ENTER_FOREGROUND||type==SDL_EVENT_TERMINATING;
  }
  void observe(const SDL_Event& event) {
    if(!enabled||emitted||!relevant(event.type))return;
    const auto now=SDL_GetTicksNS();
    if(eventCount==events.size())eventOverflow=true;
    else {
      auto& row=events[eventCount++];row.type=event.type;row.timestamp=event.common.timestamp;row.observedAt=now;
      if(event.type>=SDL_EVENT_WINDOW_FIRST&&event.type<=SDL_EVENT_WINDOW_LAST){row.window=event.window.windowID;row.data1=event.window.data1;row.data2=event.window.data2;}
      else if(touch(event.type))row.window=event.tfinger.windowID;
      else if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN||event.type==SDL_EVENT_MOUSE_BUTTON_UP)row.window=event.button.windowID;
    }
    const bool mouse=event.type==SDL_EVENT_MOUSE_BUTTON_DOWN||event.type==SDL_EVENT_MOUSE_BUTTON_UP;
    if(!touch(event.type)&&!mouse)return;
    if(!firstDown&&(event.type==SDL_EVENT_FINGER_DOWN||event.type==SDL_EVENT_MOUSE_BUTTON_DOWN))firstDown=now;
    if(event.type==SDL_EVENT_FINGER_UP||event.type==SDL_EVENT_FINGER_CANCELED||event.type==SDL_EVENT_MOUSE_BUTTON_UP)terminal=true;
    if(count==samples.size()){overflow=true;return;}
    auto& item=samples[count++];item.type=event.type;item.timestamp=event.common.timestamp;item.observedAt=now;
    if(touch(event.type)){
      item.touch=event.tfinger.touchID;item.finger=event.tfinger.fingerID;item.window=event.tfinger.windowID;
      item.device=SDL_GetTouchDeviceType(item.touch);item.x=event.tfinger.x;item.y=event.tfinger.y;
    } else {item.window=event.button.windowID;item.button=event.button.button;}
  }
  // Copy the conversion the app already performs; do not perform a second query.
  void converted(const SDL_Event& event,int width,int height,float x,float y) {
    if(!enabled||emitted||!count||overflow)return;
    auto& sample=samples[count-1];
    if(sample.type!=event.type||sample.timestamp!=event.common.timestamp||!touch(event.type))return;
    sample.width=width;sample.height=height;sample.mappedX=x;sample.mappedY=y;sample.converted=true;
  }
  static Geometry geometry(SDL_Window* window,SDL_Renderer* renderer) {
    Geometry value;value.at=SDL_GetTicksNS();value.window=SDL_GetWindowID(window);value.flags=SDL_GetWindowFlags(window);
    value.valid=SDL_GetWindowSize(window,&value.width,&value.height)&&SDL_GetWindowSizeInPixels(window,&value.pixelWidth,&value.pixelHeight)&&
      SDL_GetWindowSafeArea(window,&value.safe)&&SDL_GetRenderLogicalPresentationRect(renderer,&value.presentation);
    return value;
  }
  void ready(SDL_Window* window,SDL_Renderer* renderer) {if(enabled&&!initial.at)initial=geometry(window,renderer);}
  bool due() const {return enabled&&!emitted&&(terminal||(firstDown&&SDL_GetTicksNS()-firstDown>=10000000000ULL));}
  static const char* phase(Uint32 type) {
    return type==SDL_EVENT_FINGER_DOWN?"down":type==SDL_EVENT_FINGER_UP?"up":type==SDL_EVENT_FINGER_MOTION?"motion":
      type==SDL_EVENT_FINGER_CANCELED?"cancel":type==SDL_EVENT_MOUSE_BUTTON_DOWN?"mouse-down":"mouse-up";
  }
  static nlohmann::json geometryJSON(const Geometry& v) {
    return {{"atNs",std::to_string(v.at)},{"windowID",v.window},{"flags",std::to_string(v.flags)},{"valid",v.valid},
      {"width",v.width},{"height",v.height},{"pixelWidth",v.pixelWidth},{"pixelHeight",v.pixelHeight},
      {"safeArea",{{"x",v.safe.x},{"y",v.safe.y},{"width",v.safe.w},{"height",v.safe.h}}},
      {"presentation",{{"x",v.presentation.x},{"y",v.presentation.y},{"width",v.presentation.w},{"height",v.presentation.h}}}};
  }
  // Called once after the complete terminal SDL batch, or an explicitly
  // incomplete existing-loop deadline. Allocation/serialization occurs here only.
  nlohmann::json report(SDL_Window* window,SDL_Renderer* renderer,const std::string& nonce,int pid) {
    emitted=true;auto rows=nlohmann::json::array(),trace=nlohmann::json::array();
    for(size_t i=0;i<count;++i){const auto& s=samples[i];rows.push_back({{"phase",phase(s.type)},{"timestampNS",std::to_string(s.timestamp)},
      {"observedAtNS",std::to_string(s.observedAt)},{"touchID",std::to_string(s.touch)},{"fingerID",std::to_string(s.finger)},
      {"windowID",s.window},{"direct",s.device==SDL_TOUCH_DEVICE_DIRECT},{"mouseTouch",s.touch==SDL_MOUSE_TOUCHID},
      {"x",s.x},{"y",s.y},{"windowWidth",s.width},{"windowHeight",s.height},{"button",s.button},
      {"converted",s.converted},{"mappedX",s.mappedX},{"mappedY",s.mappedY}});}
    for(size_t i=0;i<eventCount;++i){const auto& e=events[i];trace.push_back({{"type",e.type},{"timestampNS",std::to_string(e.timestamp)},
      {"observedAtNS",std::to_string(e.observedAt)},{"windowID",e.window},{"data1",e.data1},{"data2",e.data2}});}
    return {{"event","input-proof"},{"version",1},{"nonce",nonce},{"pid",pid},{"source","SDL_PollEvent"},{"captureClock","SDL_GetTicksNS"},{"eventClock","SDL event timestamp"},
      {"windowID",SDL_GetWindowID(window)},{"overflow",overflow},{"terminal",terminal},{"samples",rows},
      {"geometryObservation",{{"scope","consumed-only; not complete UIKit callback history"},{"overflow",eventOverflow},
        {"eventCount",eventCount},{"initialPhase","first frame after logical dimensions; before engine context Update"},{"initial",geometryJSON(initial)},
        {"finalPhase",terminal?"after first observed terminal SDL batch":"incomplete original-loop deadline"},
        {"final",geometryJSON(geometry(window,renderer))},{"events",trace}}}};
  }
};
