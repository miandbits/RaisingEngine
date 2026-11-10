#pragma once
#include <map>
#include <memory>
#include <string>

struct SDL_Window;
struct SDL_Renderer;
namespace Rml { class Element; }

// A UIKit view exposes the rendered RmlUi surface without handling touch input.
class IOSAccessibility {
public:
  IOSAccessibility(SDL_Window* window, SDL_Renderer* renderer);
  ~IOSAccessibility();
  void update(Rml::Element* root, const std::map<std::string, std::string>& labels,
              const std::string& surface, const std::string& locale,
              bool editing);
  void clear();
private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};
