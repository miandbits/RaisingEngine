#pragma once

#include <SDL3/SDL.h>
#include "json.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <iostream>
#include <memory>

// Opt-in diagnostic for the first physical contact. Recording performs no
// allocation or output; serialization happens once, outside input dispatch.
// In particular, this does not enable SDL's synchronous per-event logging.
class InputTrace {
  using Json=nlohmann::json;
  struct Record {
    std::atomic<bool> ready{false};
    const char* stage=nullptr;
    uint64_t at=0,event_at=0,generation=0,touch=0,finger=0,active=0;
    uint32_t type=0,window=0,flags=0,button=0;
    float x=0,y=0,dx=0,dy=0;
    char target[80]{},hover[80]{},focus[80]{};
  };
  static constexpr size_t capacity=512;
  std::unique_ptr<std::array<Record,capacity>> records;
  std::atomic<size_t> count{0};
  std::atomic<uint64_t> first_down{0};
  bool emitted=false;
  static void copy(char (&to)[80],const char* from) {
    if(from){std::strncpy(to,from,79);to[79]=0;}
  }
public:
  InputTrace() {
    if(SDL_GetHintBoolean("RAISING_INPUT_TRACE",false)) {
      records=std::make_unique<std::array<Record,capacity>>();
    }
  }
  bool enabled() const {return bool(records);}
  static bool relevant(uint32_t type) {
    return (type>=SDL_EVENT_FINGER_DOWN&&type<=SDL_EVENT_FINGER_CANCELED)||
      type==SDL_EVENT_MOUSE_BUTTON_DOWN||type==SDL_EVENT_MOUSE_BUTTON_UP||type==SDL_EVENT_MOUSE_MOTION||
      (type>=SDL_EVENT_WINDOW_FIRST&&type<=SDL_EVENT_WINDOW_LAST)||
      type==SDL_EVENT_WILL_ENTER_BACKGROUND||type==SDL_EVENT_DID_ENTER_BACKGROUND||
      type==SDL_EVENT_WILL_ENTER_FOREGROUND||type==SDL_EVENT_DID_ENTER_FOREGROUND;
  }
  void note(const char* stage,const SDL_Event* event=nullptr,uint64_t generation=0,
            uint64_t active=0,uint32_t flags=0,float x=0,float y=0,
            const char* target=nullptr,const char* hover=nullptr,const char* focus=nullptr) {
    if(!records)return;
    const auto at=SDL_GetTicksNS();
    if(event&&(event->type==SDL_EVENT_FINGER_DOWN||event->type==SDL_EVENT_MOUSE_BUTTON_DOWN)){uint64_t unset=0;first_down.compare_exchange_strong(unset,at);}
    const auto index=count.fetch_add(1,std::memory_order_relaxed);
    if(index>=capacity)return;
    auto& row=(*records)[index];
    row.stage=stage;row.at=at;row.generation=generation;row.active=active;row.flags=flags;row.x=x;row.y=y;
    if(event) {
      row.type=event->type;row.event_at=event->common.timestamp;
      if(event->type>=SDL_EVENT_FINGER_DOWN&&event->type<=SDL_EVENT_FINGER_CANCELED) {
        row.touch=event->tfinger.touchID;row.finger=event->tfinger.fingerID;row.window=event->tfinger.windowID;
        if(std::strcmp(stage,"mapped")!=0){row.x=event->tfinger.x;row.y=event->tfinger.y;}
        row.dx=event->tfinger.dx;row.dy=event->tfinger.dy;
      } else if(event->type==SDL_EVENT_MOUSE_BUTTON_DOWN||event->type==SDL_EVENT_MOUSE_BUTTON_UP) {
        row.window=event->button.windowID;row.finger=event->button.which;row.button=event->button.button;row.x=event->button.x;row.y=event->button.y;
      } else if(event->type==SDL_EVENT_MOUSE_MOTION) {
        row.window=event->motion.windowID;row.finger=event->motion.which;row.x=event->motion.x;row.y=event->motion.y;
        row.dx=event->motion.xrel;row.dy=event->motion.yrel;
      } else if(event->type>=SDL_EVENT_WINDOW_FIRST&&event->type<=SDL_EVENT_WINDOW_LAST)row.window=event->window.windowID;
    }
    copy(row.target,target);copy(row.hover,hover);copy(row.focus,focus);
    row.ready.store(true,std::memory_order_release);
  }
  void flush_if_due(uint64_t generation,uint64_t active,uint32_t flags) {
    if(!records||emitted)return;
    const auto now=SDL_GetTicksNS(),down=first_down.load();
    // Give an incomplete contact ten seconds to produce its terminal event.
    // This observes input; it neither delays dispatch nor synthesizes a release.
    if(!down||now-down<10000000000ULL)return;
    note("snapshot",nullptr,generation,active,flags);
    emitted=true;
    Json rows=Json::array();
    const auto total=count.load();
    size_t unpublished=0;
    for(size_t i=0;i<std::min(total,capacity);++i) {
      const auto& row=(*records)[i];
      if(!row.ready.load(std::memory_order_acquire)){++unpublished;continue;}
      rows.push_back({{"stage",row.stage},{"atNs",row.at},{"eventNs",row.event_at},
        {"eventType",row.type},{"window",row.window},{"button",row.button},{"generation",row.generation},
        {"touch",row.touch},{"finger",row.finger},{"active",row.active},{"flags",row.flags},
        {"x",row.x},{"y",row.y},{"dx",row.dx},{"dy",row.dy},
        {"target",row.target},{"hover",row.hover},{"focus",row.focus}});
    }
    std::cerr<<Json({{"event","input-trace"},{"version",1},{"overflow",total>capacity},
      {"recordCountAtFlush",total},{"unpublishedAtFlush",unpublished},{"flushAtNs",now},
      {"flags",{{"backgrounded",1},{"dragged",2},{"context",4},{"dirty",8},{"pending",16}}},
      {"records",std::move(rows)}}).dump()<<std::endl;
  }
};
