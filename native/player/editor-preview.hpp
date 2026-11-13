#pragma once
#ifdef SDL_PLATFORM_IOS
#include "ios-text-input.hpp"
#endif

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
  #ifdef SDL_PLATFORM_IOS
  std::unique_ptr<IOSTextInput> keyboard;
  SDL_Window* keyboard_window=nullptr;
  #endif
  bool frozen=false;
  double stopped=0,offset=0;
public:
  #ifdef SDL_PLATFORM_IOS
  void SetWindow(SDL_Window* window){SystemInterface_SDL::SetWindow(window);keyboard_window=window;}
  void SetRenderer(SDL_Renderer* renderer){keyboard=std::make_unique<IOSTextInput>(keyboard_window,renderer);}
  // RmlUi sends these callbacks before Context finishes changing focus.
  // Resolve the final focus once per frame, after the document/layout settles.
  void ActivateKeyboard(Rml::Vector2f,float) override {}
  void DeactivateKeyboard() override {}
  void BeginKeyboardRebuild(){if(keyboard)keyboard->begin_rebuild();}
  void EndKeyboardRebuild(){if(keyboard)keyboard->end_rebuild();}
  void UpdateKeyboard(){if(keyboard)keyboard->update();}
  bool OwnsKeyboard() const {return keyboard&&keyboard->owns_keyboard();}
  void RegisterKeyboardFont(const std::string& family,const std::string& path){if(keyboard)keyboard->register_font(family,path);}
  void CloseKeyboard(){keyboard.reset();}
  Json KeyboardReport() const {return keyboard?keyboard->report():Json();}
  #else
  void SetRenderer(SDL_Renderer*){}
  void BeginKeyboardRebuild(){}
  void EndKeyboardRebuild(){}
  void UpdateKeyboard(){}
  bool OwnsKeyboard() const {return false;}
  void RegisterKeyboardFont(const std::string&,const std::string&){}
  void CloseKeyboard(){}
  Json KeyboardReport() const {return Json();}
  #endif
  void pause(bool value) {
    if(value==frozen)return;
    const double now=SystemInterface_SDL::GetElapsedTime();
    if(value)stopped=now;else offset+=now-stopped;
    frozen=value;
  }
  double GetElapsedTime() override {return (frozen?stopped:SystemInterface_SDL::GetElapsedTime())-offset;}
  void JoinPath(Rml::String& translated, const Rml::String& document, const Rml::String& path) override {
#ifndef _WIN32
    // Exported assets use absolute filesystem paths. RmlUi's default treats a
    // leading slash as a virtual resource root and removes it on POSIX systems.
    if(fs::u8path(path).is_absolute()){translated=path;return;}
#endif
    SystemInterface_SDL::JoinPath(translated,document,path);
  }
};

struct EditorKeyboardRebuildGuard {
  EditorClockSystem* system=static_cast<EditorClockSystem*>(Rml::GetSystemInterface());
  EditorKeyboardRebuildGuard(){system->BeginKeyboardRebuild();}
  void finish(){if(system){system->EndKeyboardRebuild();system=nullptr;}}
  ~EditorKeyboardRebuildGuard(){finish();}
};
