#pragma once
#include <memory>
#include <SDL3/SDL.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include "json.hpp"

// UIKit owns the editing session. RmlUi remains the authored value and layout
// source, including validation and the ordinary change event.
class IOSTextInput {
public:
  IOSTextInput(SDL_Window* window, SDL_Renderer* renderer);
  ~IOSTextInput();
  bool owns_keyboard() const;
  void register_font(const std::string& family,const std::string& path);
  void begin_rebuild();
  void end_rebuild();
  void update();
  nlohmann::json report() const;
private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};
