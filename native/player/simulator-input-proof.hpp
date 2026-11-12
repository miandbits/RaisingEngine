#pragma once
#include <array>
#include <SDL3/SDL.h>

// Simulator test observation only. Called exclusively for real SDL_PollEvent
// results. No allocation, output, event synthesis, filtering, or Rml access.
struct SimulatorInputProof {
  struct Sample {
    Uint32 type=0; Uint64 timestamp=0;
    SDL_TouchID touch=0; SDL_FingerID finger=0; SDL_WindowID window=0;
    SDL_TouchDeviceType device=SDL_TOUCH_DEVICE_INVALID;
    float x=0,y=0; int width=0,height=0; Uint8 button=0;
  };
  std::array<Sample,64> samples{};
  size_t count=0; bool enabled=false,terminal=false,overflow=false,emitted=false;
  void observe(const SDL_Event& event) {
    if(!enabled || emitted)return;
    const bool touch=event.type==SDL_EVENT_FINGER_DOWN || event.type==SDL_EVENT_FINGER_MOTION || event.type==SDL_EVENT_FINGER_UP || event.type==SDL_EVENT_FINGER_CANCELED;
    const bool mouse=event.type==SDL_EVENT_MOUSE_BUTTON_DOWN || event.type==SDL_EVENT_MOUSE_BUTTON_UP;
    if(!touch && !mouse)return;
    if(event.type==SDL_EVENT_FINGER_UP || event.type==SDL_EVENT_FINGER_CANCELED)terminal=true;
    if(count==samples.size()){overflow=true;return;}
    auto& item=samples[count++];item.type=event.type;item.timestamp=event.common.timestamp;
    if(touch){
      item.touch=event.tfinger.touchID;item.finger=event.tfinger.fingerID;item.window=event.tfinger.windowID;
      item.device=SDL_GetTouchDeviceType(item.touch);item.x=event.tfinger.x;item.y=event.tfinger.y;
      if(auto* window=SDL_GetWindowFromID(item.window))SDL_GetWindowSize(window,&item.width,&item.height);
    } else {item.window=event.button.windowID;item.button=event.button.button;}
  }
};
