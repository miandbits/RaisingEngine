#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "simulator-input-proof.hpp"
#include <cmath>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <unistd.h>

// Diagnostic sample, not a replacement for the engine's Rml button semantics.
int main(int argc,char** argv) {
  using Json=nlohmann::json;
  SimulatorInputProof proof;std::string nonce;
  SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
  std::map<SDL_JoystickID,SDL_Gamepad*> gamepads;
  try {
    for(int i=1;i<argc;++i){const std::string arg=argv[i];
      if(arg=="--input-proof-nonce"&&i+1<argc)nonce=argv[++i];
      else throw std::runtime_error("Unknown reference argument");
    }
    if(nonce.size()!=64||nonce.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::runtime_error("Missing input nonce");
    proof.enabled=true;
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS,"0");
    SDL_SetHint(SDL_HINT_ORIENTATIONS,"Portrait LandscapeLeft LandscapeRight");
    if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD))throw std::runtime_error(SDL_GetError());
    window=SDL_CreateWindow("SDL Contact Reference",1280,720,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if(!window)throw std::runtime_error(SDL_GetError());
    renderer=SDL_CreateRenderer(window,nullptr);if(!renderer)throw std::runtime_error(SDL_GetError());
    SDL_SetRenderVSync(renderer,1);
    bool running=true,dirty=true,ready=false,dragged=false;
    int width=0,height=0,activations=0;float startX=0,startY=0;
    std::optional<SDL_FingerID> active;
    const SDL_FRect button{91.f,546.5f,200.f,52.f};
    const auto inside=[&](float x,float y){return x>=button.x&&x<button.x+button.w&&y>=button.y&&y<button.y+button.h;};
    while(running) {
      SDL_Event event;
      while(SDL_PollEvent(&event)) {
        proof.observe(event);
        if(event.type==SDL_EVENT_QUIT)running=false;
        if(event.type==SDL_EVENT_WINDOW_RESIZED||event.type==SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED||
           event.type==SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED||event.type==SDL_EVENT_WINDOW_SAFE_AREA_CHANGED)dirty=true;
        if(event.type==SDL_EVENT_GAMEPAD_ADDED){if(auto* pad=SDL_OpenGamepad(event.gdevice.which))gamepads[event.gdevice.which]=pad;}
        if(event.type==SDL_EVENT_GAMEPAD_REMOVED){auto item=gamepads.find(event.gdevice.which);if(item!=gamepads.end()){SDL_CloseGamepad(item->second);gamepads.erase(item);}}
        if(SimulatorInputProof::touch(event.type)) {
          int currentWidth=0,currentHeight=0;SDL_GetWindowSize(window,&currentWidth,&currentHeight);
          float x=0,y=0;
          if(!SDL_RenderCoordinatesFromWindow(renderer,event.tfinger.x*currentWidth,event.tfinger.y*currentHeight,&x,&y))continue;
          proof.converted(event,currentWidth,currentHeight,x,y);
          const bool direct=SDL_GetTouchDeviceType(event.tfinger.touchID)==SDL_TOUCH_DEVICE_DIRECT&&event.tfinger.touchID!=SDL_MOUSE_TOUCHID;
          if(event.type==SDL_EVENT_FINGER_DOWN) {
            if(!active&&direct&&inside(x,y)){active=event.tfinger.fingerID;startX=x;startY=y;dragged=false;}
          } else if(active&&*active==event.tfinger.fingerID) {
            if(std::hypot(x-startX,y-startY)>8)dragged=true;
            if(event.type==SDL_EVENT_FINGER_UP){if(direct&&!dragged&&inside(x,y))++activations;active.reset();}
            else if(event.type==SDL_EVENT_FINGER_CANCELED)active.reset();
          }
        }
      }
      if(proof.due()){
        auto value=proof.report(window,renderer,nonce,getpid());
        value["referenceSupplement"]={{"activations",activations},{"activeFinger",active?std::to_string(*active):""},{"dragged",dragged}};
        std::cout<<value.dump()<<std::endl;
      }
      if(dirty){SDL_GetWindowSize(window,&width,&height);SDL_SetRenderLogicalPresentation(renderer,width,height,SDL_LOGICAL_PRESENTATION_LETTERBOX);dirty=false;}
      proof.ready(window,renderer);
      SDL_SetRenderDrawColor(renderer,19,35,28,255);SDL_RenderClear(renderer);
      SDL_SetRenderDrawColor(renderer,activations?40:220,activations?180:180,70,255);SDL_RenderFillRect(renderer,&button);
      SDL_RenderPresent(renderer);
      if(!ready&&width==390&&height==844){ready=true;
        std::cout<<Json({{"event","vendor-ready"},{"version",1},{"arm","S"},{"nonce",nonce},{"pid",getpid()},
          {"windowWidth",width},{"windowHeight",height},{"renderer",SDL_GetRendererName(renderer)},
          {"control",{{"x",91},{"y",546.5},{"width",200},{"height",52}}},{"activations",activations}}).dump()<<std::endl;
      }
    }
  } catch(const std::exception& error){std::cerr<<error.what()<<std::endl;return 1;}
  for(auto& item:gamepads)SDL_CloseGamepad(item.second);
  SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
