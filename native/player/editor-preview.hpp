#pragma once

// The editor owns this process and its private directory. There is no network listener.
struct EditorPreviewControl {
  fs::path directory;
  int64_t serial=0;
  bool paused=true;
  double elapsed=0;
  Json poll() {
    if(directory.empty()||!fs::exists(directory/"control.json"))return Json();
    try {
      auto value=read_json(directory/"control.json",65536);
      const auto next=value.at("serial").get<int64_t>();
      if(next<=serial)return Json();
      serial=next;return value;
    }catch(...){return Json();} // Atomic editor writes; a stale/incomplete command never advances play.
  }
};

class EditorClockSystem : public SystemInterface_SDL {
  bool frozen=false;
  double stopped=0,offset=0;
public:
  void pause(bool value) {
    if(value==frozen)return;
    const double now=SystemInterface_SDL::GetElapsedTime();
    if(value)stopped=now;else offset+=now-stopped;
    frozen=value;
  }
  double GetElapsedTime() override {return (frozen?stopped:SystemInterface_SDL::GetElapsedTime())-offset;}
};
