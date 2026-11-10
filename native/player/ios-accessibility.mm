#include "ios-accessibility.hpp"
#include <RmlUi/Core.h>
#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/ElementText.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include <SDL3/SDL.h>
#import <UIKit/UIKit.h>
#include <algorithm>
#include <cctype>
#include <cmath>

@interface RaisingAXElement : UIAccessibilityElement {
@public
  Rml::ObserverPtr<Rml::Element> target;
}
@property(nonatomic,copy) NSString* key;
@property(nonatomic,copy) CGRect (^boundsForElement)(RaisingAXElement*);
@property(nonatomic,copy) void (^didFocus)(RaisingAXElement*);
@property(nonatomic,copy) BOOL (^activate)(RaisingAXElement*);
@property(nonatomic,copy) void (^adjust)(RaisingAXElement*, int);
@property(nonatomic,copy) BOOL (^scroll)(RaisingAXElement*, UIAccessibilityScrollDirection);
@end

@implementation RaisingAXElement
- (CGRect)accessibilityFrameInContainerSpace {
  return self.boundsForElement ? self.boundsForElement(self) : CGRectNull;
}
- (CGRect)accessibilityFrame {
  UIView* container = (UIView*)self.accessibilityContainer;
  const CGRect frame = self.accessibilityFrameInContainerSpace;
  return container.window && !CGRectIsNull(frame)
    ? UIAccessibilityConvertFrameToScreenCoordinates(frame, container) : CGRectZero;
}
- (void)accessibilityElementDidBecomeFocused { if(self.didFocus)self.didFocus(self); }
- (BOOL)accessibilityActivate { return self.activate && self.activate(self); }
- (void)accessibilityIncrement { if(self.adjust)self.adjust(self, 1); }
- (void)accessibilityDecrement { if(self.adjust)self.adjust(self, -1); }
- (BOOL)accessibilityScroll:(UIAccessibilityScrollDirection)direction {
  return self.scroll && self.scroll(self, direction);
}
@end

@interface RaisingAXView : UIView
@property(nonatomic,copy) NSArray* (^readElements)(void);
@property(nonatomic,copy) BOOL (^scroll)(UIAccessibilityScrollDirection);
@end

@implementation RaisingAXView
- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event { return nil; }
- (NSArray*)accessibilityElements { return self.readElements ? self.readElements() : @[]; }
- (BOOL)accessibilityScroll:(UIAccessibilityScrollDirection)direction {
  return self.scroll && self.scroll(direction);
}
@end

namespace {
NSString* string(const std::string& value) {
  return [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding] ?: @"";
}
std::string collapse(const std::string& source) {
  std::string result;
  bool space = false;
  for(unsigned char c:source) {
    if(std::isspace(c))space = !result.empty();
    else {
      if(space)result += ' ';
      result += char(c);
      space = false;
    }
  }
  return result;
}
bool visible(Rml::Element* element) {
  return element->IsVisible() && element->GetComputedValues().opacity() > 0 &&
    element->GetAttribute<Rml::String>("aria-hidden", "") != "true";
}
bool is_heading(Rml::Element* e) {
  const auto& tag = e->GetTagName();
  return (tag.size() == 2 && tag[0] == 'h' && tag[1] >= '1' && tag[1] <= '6') ||
    e->IsClassSet("heading") || e->GetAttribute<Rml::String>("role", "") == "heading";
}
bool is_control(const Rml::String& tag) {
  return tag=="button" || tag=="input" || tag=="select";
}
bool unavailable(Rml::Element* e) {
  for(auto* p=e; p; p=p->GetParentNode())
    if(p->HasAttribute("disabled") || p->GetAttribute<Rml::String>("aria-disabled", "") == "true")return true;
  return false;
}
// A visual block starts a new phrase, while inline formatting keeps words and
// punctuation together. RmlUi has already decoded entities in text nodes.
std::string text(Rml::Element* e, bool only_inline=false, bool check_visibility=true) {
  if(check_visibility && !visible(e))return {};
  if(auto* node = dynamic_cast<Rml::ElementText*>(e))return node->GetText();
  if(e->GetTagName() == "br")return " ";
  std::string result;
  for(int i=0; i<e->GetNumChildren(); ++i) {
    auto* child=e->GetChild(i);
    const bool block=child->GetDisplay() != Rml::Style::Display::Inline;
    if(only_inline && (is_control(child->GetTagName()) || child->GetTagName()=="img" || is_heading(child) ||
                       child->HasAttribute("aria-label")))continue;
    if(only_inline && block && !dynamic_cast<Rml::ElementText*>(child))continue;
    auto phrase=text(child, only_inline, check_visibility);
    if(phrase.empty())continue;
    if(block && !result.empty())result += ' ';
    result += phrase;
    if(block)result += ' ';
  }
  return result;
}
bool can_scroll(Rml::Element* e, bool horizontal) {
  const auto& style=e->GetComputedValues();
  const auto overflow=horizontal ? style.overflow_x() : style.overflow_y();
  if(overflow != Rml::Style::Overflow::Auto && overflow != Rml::Style::Overflow::Scroll)return false;
  return horizontal ? e->GetScrollWidth() > e->GetClientWidth()+1
                    : e->GetScrollHeight() > e->GetClientHeight()+1;
}
}

struct IOSAccessibility::Impl {
  SDL_Window* window;
  SDL_Renderer* renderer;
  RaisingAXView* view;
  Rml::ObserverPtr<Rml::Element> root;
  std::map<std::string, std::string> labels;
  NSString* locale;
  std::string surface;
  bool editing=false;
  bool exposed=false;
  uint64_t last_query=0;
  NSMutableDictionary<NSString*, RaisingAXElement*>* elements=[NSMutableDictionary new];
  NSArray<RaisingAXElement*>* order=@[];
  NSMutableDictionary<NSString*, NSString*>* previous_focus=[NSMutableDictionary new];
  NSString* focused;

  Impl(SDL_Window* w, SDL_Renderer* r):window(w),renderer(r) {
    view=[RaisingAXView new];
    view.isAccessibilityElement=NO;
    view.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
    view.readElements=^NSArray* { refresh(); return order; };
    view.scroll=^BOOL(UIAccessibilityScrollDirection direction) { return scroll(elements[focused ?: @""], direction); };
  }
  ~Impl() {
    clear();
    view.readElements=nil;
    view.scroll=nil;
    [view removeFromSuperview];
  }
  void retire(RaisingAXElement* element) {
    element->target.reset();
    element.boundsForElement=nil;
    element.didFocus=nil;
    element.activate=nil;
    element.adjust=nil;
    element.scroll=nil;
  }
  void clear() {
    for(RaisingAXElement* element in order)retire(element);
    [elements removeAllObjects];
    order=@[];
    root.reset();
    labels.clear();
    focused=nil;
  }
  Rml::Element* live(RaisingAXElement* element) {
    if(!root || !element || elements[element.key] != element)return nullptr;
    auto* e=element->target.get();
    if(!e)return nullptr;
    for(auto* p=e; p; p=p->GetParentNode()) {
      if(!visible(p))return nullptr;
      if(p==root.get())return e;
    }
    return nullptr;
  }
  CGRect frame(Rml::Element* e) const {
    if(!e)return CGRectNull;
    const auto offset=e->GetAbsoluteOffset(Rml::BoxArea::Border);
    const auto size=e->GetBox().GetSize(Rml::BoxArea::Border);
    float x=0,y=0,right=0,bottom=0;
    if(size.x<=0 || size.y<=0 ||
       !SDL_RenderCoordinatesToWindow(renderer,offset.x,offset.y,&x,&y) ||
       !SDL_RenderCoordinatesToWindow(renderer,offset.x+size.x,offset.y+size.y,&right,&bottom))return CGRectNull;
    return CGRectMake(x,y,right-x,bottom-y);
  }
  void reveal(Rml::Element* e) {
    e->ScrollIntoView({Rml::ScrollAlignment::Nearest,Rml::ScrollAlignment::Nearest});
  }
  void focus(RaisingAXElement* element) {
    if(auto* e=live(element)) {
      focused=element.key;
      reveal(e);
    }
  }
  BOOL activate(RaisingAXElement* element) {
    auto* e=live(element);
    if(!e || unavailable(e))return NO;
    reveal(e);
    if(e->GetTagName()=="input")return e->Focus(true);
    e->Click();
    return YES;
  }
  void adjust(RaisingAXElement* element, int step) {
    auto* e=live(element);
    auto* select=dynamic_cast<Rml::ElementFormControlSelect*>(e);
    if(!select || unavailable(select))return;
    for(int next=select->GetSelection()+step; next>=0 && next<select->GetNumOptions(); next+=step) {
      if(unavailable(select->GetOption(next)))continue;
      select->SetSelection(next); // RmlUi dispatches the normal change event.
      break;
    }
    refresh(true);
  }
  BOOL scroll(RaisingAXElement* element, UIAccessibilityScrollDirection direction) {
    const bool horizontal=direction==UIAccessibilityScrollDirectionLeft || direction==UIAccessibilityScrollDirectionRight;
    const bool forward=direction==UIAccessibilityScrollDirectionRight || direction==UIAccessibilityScrollDirectionDown ||
      direction==UIAccessibilityScrollDirectionNext;
    auto* e=live(element);
    if(!e && root)e=root->GetElementById("reading-main");
    for(auto* p=e; p; p=p->GetParentNode()) {
      if(!can_scroll(p,horizontal))continue;
      const float from=horizontal ? p->GetScrollLeft() : p->GetScrollTop();
      const float size=horizontal ? p->GetClientWidth() : p->GetClientHeight();
      const float length=horizontal ? p->GetScrollWidth() : p->GetScrollHeight();
      const float to=std::clamp(from+(forward ? .85f : -.85f)*size,0.f,length-size);
      if(std::abs(to-from)<1)continue;
      if(horizontal)p->SetScrollLeft(to);else p->SetScrollTop(to);
      UIAccessibilityPostNotification(UIAccessibilityPageScrolledNotification,nil);
      return YES;
    }
    return NO;
  }
  void append(Rml::Element* e, const std::string& path, const std::string& name,
              UIAccessibilityTraits traits, NSMutableArray<RaisingAXElement*>* next,
              NSMutableDictionary<NSString*, RaisingAXElement*>* lookup) {
    if(name.empty() || CGRectIsNull(frame(e)))return;
    const auto& id=e->GetId();
    NSString* key=string(id.empty() ? "path:"+path : "id:"+id);
    if(lookup[key])key=string("path:"+path);
    RaisingAXElement* element=elements[key];
    if(!element) {
      element=[[RaisingAXElement alloc] initWithAccessibilityContainer:view];
      element.key=key;
      element.isAccessibilityElement=YES;
      element.boundsForElement=^CGRect(RaisingAXElement* item) { return frame(live(item)); };
      element.didFocus=^(RaisingAXElement* item) { focus(item); };
      element.scroll=^BOOL(RaisingAXElement* item, UIAccessibilityScrollDirection direction) { return scroll(item,direction); };
    }
    element->target=e->GetObserverPtr();
    const bool button=(traits & UIAccessibilityTraitButton)!=0;
    const bool adjustable=(traits & UIAccessibilityTraitAdjustable)!=0;
    if(unavailable(e))traits|=UIAccessibilityTraitNotEnabled;
    if(e->GetAttribute<Rml::String>("aria-pressed","")=="true" ||
       e->GetAttribute<Rml::String>("aria-selected","")=="true" ||
       e->IsClassSet("board-selected") || e->IsClassSet("slot-selected"))traits|=UIAccessibilityTraitSelected;
    element.accessibilityIdentifier=id.empty() ? nil : string(id);
    element.accessibilityLabel=string(name);
    element.accessibilityLanguage=locale;
    element.accessibilityTraits=traits;
    NSString* value=string(e->GetAttribute<Rml::String>("aria-valuetext",""));
    element.accessibilityValue=value.length ? value : nil;
    element.activate=nil;
    element.adjust=nil;
    if(button)element.activate=^BOOL(RaisingAXElement* item) { return activate(item); };
    if(auto* input=dynamic_cast<Rml::ElementFormControlInput*>(e)) {
      if(input->GetAttribute<Rml::String>("type","text")!="password")element.accessibilityValue=string(input->GetValue());
    }
    if(adjustable) {
      auto* select=static_cast<Rml::ElementFormControlSelect*>(e);
      if(auto* option=select->GetOption(select->GetSelection()))element.accessibilityValue=string(collapse(text(option,false,false)));
      element.adjust=^(RaisingAXElement* item, int step) { adjust(item,step); };
    }
    lookup[key]=element;
    [next addObject:element];
  }
  void collect(Rml::Element* e, const std::string& path,
               NSMutableArray<RaisingAXElement*>* next,
               NSMutableDictionary<NSString*, RaisingAXElement*>* lookup,
               bool text_already_included=false) {
    if(!visible(e) || e->GetId()=="ui-tooltip" || dynamic_cast<Rml::ElementText*>(e))return;
    const auto& tag=e->GetTagName();
    auto name=e->GetAttribute<Rml::String>("aria-label","");
    if(is_control(tag)) {
      if(tag=="input" && (e->GetAttribute<Rml::String>("type","")=="hidden" ||
                          (editing && e->GetContext()->GetFocusElement()==e)))return;
      if(name.empty() && tag=="button")name=text(e);
      const auto label=labels.find(e->GetId());
      if(name.empty() && label!=labels.end())name=label->second;
      if(name.empty() && tag=="select") {
        const auto language=locale.UTF8String ?: "";
        name=std::string(language).rfind("ko",0)==0 ? "옵션" :
             std::string(language).rfind("ja",0)==0 ? "オプション" : "Options";
      }
      append(e,path,collapse(name),tag=="select" ? UIAccessibilityTraitAdjustable : UIAccessibilityTraitButton,next,lookup);
      return;
    }
    const bool heading=is_heading(e);
    if(name.empty() && !text_already_included) {
      if(tag=="img")name=e->GetAttribute<Rml::String>("alt","");
      else name=text(e,true);
    }
    const bool leaf=!collapse(name).empty();
    if(leaf)append(e,path,collapse(name),heading ? UIAccessibilityTraitHeader :
                   tag=="img" ? UIAccessibilityTraitImage : UIAccessibilityTraitStaticText,next,lookup);
    for(int i=0; i<e->GetNumChildren(); ++i) {
      auto* child=e->GetChild(i);
      const auto& child_tag=child->GetTagName();
      // Inline text was included in its nearest block. Keep a separate stop
      // for controls, images and headings even if authored inline.
      const bool included=(leaf || text_already_included) && child->GetDisplay()==Rml::Style::Display::Inline &&
        !is_control(child_tag) && child_tag!="img" && !is_heading(child) && !child->HasAttribute("aria-label");
      collect(child,path+"/"+std::to_string(i),next,lookup,included);
    }
  }
  void refresh(bool force=false) {
    const auto now=SDL_GetTicks();
    if(!root || (!force && last_query && now-last_query<80))return;
    last_query=now;
    auto* next=[NSMutableArray<RaisingAXElement*> new];
    auto* lookup=[NSMutableDictionary<NSString*, RaisingAXElement*> new];
    collect(root.get(),"0",next,lookup);
    for(RaisingAXElement* element in order)if(!lookup[element.key])retire(element);
    elements=lookup;
    order=next;
  }
  void update(Rml::Element* next, const std::map<std::string, std::string>& next_labels,
              const std::string& next_surface, const std::string& language, bool next_editing) {
    auto* host=(__bridge UIWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER,nullptr);
    UIViewController* controller=host.rootViewController;
    UIView* parent=controller.view;
    if(parent && view.superview!=parent) {
      view.frame=parent.bounds;
      [parent addSubview:view];
    }
    const bool native_modal=controller.presentedViewController!=nil;
    view.accessibilityElementsHidden=native_modal;
    if(native_modal)next=nullptr;
    const bool changed=surface!=next_surface || exposed!=(next!=nullptr);
    const bool rebuilt=next!=root.get() || editing!=next_editing;
    if(changed) {
      if(focused && !surface.empty()) {
        if(previous_focus.count>=32)[previous_focus removeAllObjects];
        previous_focus[string(surface)]=focused;
      }
      clear();
    }
    if(next!=root.get()) {
      root=next ? next->GetObserverPtr() : nullptr;
      labels=next_labels;
    }
    surface=next_surface;
    editing=next_editing;
    exposed=next!=nullptr;
    if(language!=(locale.UTF8String ?: ""))locale=language.empty() ? nil : string(language);
    if(!next) { if(order.count)clear(); return; }
    if(rebuilt)last_query=0;
    if(changed && UIAccessibilityIsVoiceOverRunning()) {
      refresh(true);
      auto* first=elements[previous_focus[string(surface)] ?: @""] ?: order.firstObject;
      UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,first);
    }
  }
};

IOSAccessibility::IOSAccessibility(SDL_Window* window, SDL_Renderer* renderer)
  :impl(std::make_unique<Impl>(window,renderer)) {}
IOSAccessibility::~IOSAccessibility()=default;
void IOSAccessibility::update(Rml::Element* root, const std::map<std::string,std::string>& labels,
                             const std::string& surface, const std::string& locale, bool editing) {
  @autoreleasepool { impl->update(root,labels,surface,locale,editing); }
}
void IOSAccessibility::clear() { impl->clear(); }
