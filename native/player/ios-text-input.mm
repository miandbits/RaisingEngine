#include "ios-text-input.hpp"
#include <RmlUi/Core.h>
#import <UIKit/UIKit.h>
#import <CoreText/CoreText.h>
#include <algorithm>
#include <cmath>
#include <map>

@interface RaisingTextField : UITextField
@property(nonatomic) UIEdgeInsets textInsets;
@property(nonatomic,copy) void (^navigate)(BOOL reverse,BOOL dismiss);
@end
@implementation RaisingTextField
- (CGRect)textRectForBounds:(CGRect)bounds { return UIEdgeInsetsInsetRect(bounds,self.textInsets); }
- (CGRect)editingRectForBounds:(CGRect)bounds { return [self textRectForBounds:bounds]; }
- (NSArray<UIKeyCommand*>*)keyCommands {
  NSMutableArray* commands=[NSMutableArray arrayWithArray:[super keyCommands]?:@[]];
  for(UIKeyCommand* command in @[
    [UIKeyCommand keyCommandWithInput:@"\t" modifierFlags:0 action:@selector(moveFocus:)],
    [UIKeyCommand keyCommandWithInput:@"\t" modifierFlags:UIKeyModifierShift action:@selector(moveFocus:)],
    [UIKeyCommand keyCommandWithInput:UIKeyInputEscape modifierFlags:0 action:@selector(moveFocus:)]]) {
    command.wantsPriorityOverSystemBehavior=YES;[commands addObject:command];
  }
  return commands;
}
- (void)moveFocus:(UIKeyCommand*)command {if(self.navigate)self.navigate((command.modifierFlags&UIKeyModifierShift)!=0,[command.input isEqual:UIKeyInputEscape]);}
@end

@interface RaisingTextDelegate : NSObject <UITextFieldDelegate>
@property(nonatomic,copy) void (^changed)(void);
@property(nonatomic,copy) void (^selectionChanged)(void);
@property(nonatomic,copy) void (^finished)(void);
- (void)valueChanged:(UITextField*)field;
@end
@implementation RaisingTextDelegate
- (void)valueChanged:(UITextField*)field { if(self.changed)self.changed(); }
- (void)textFieldDidChangeSelection:(UITextField*)field { if(self.selectionChanged)self.selectionChanged(); }
- (BOOL)textFieldShouldReturn:(UITextField*)field { if(self.finished)self.finished(); return NO; }
- (BOOL)textField:(UITextField*)field shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString*)text {
  // The iPad software Tab key arrives as replacement text, while physical Tab
  // uses UIKeyCommand. Both must move authored focus, never insert a control.
  if([text isEqualToString:@"\t"]){RaisingTextField* input=(RaisingTextField*)field;if(input.navigate)input.navigate(NO,NO);return NO;}
  return YES;
}
@end

namespace {
NSString* string(const std::string& value) { return [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding] ?: @""; }
int characters(NSString* value, NSInteger end) {
  int count=0;const auto length=std::min<NSInteger>(value.length,std::max<NSInteger>(0,end));
  for(NSInteger i=0;i<length;i++,count++)if(CFStringIsSurrogateHighCharacter([value characterAtIndex:i])&&i+1<length&&CFStringIsSurrogateLowCharacter([value characterAtIndex:i+1]))i++;
  return count;
}
NSInteger utf16(NSString* value,int count) {
  NSInteger i=0;for(int n=0;n<count&&i<value.length;n++,i++)if(CFStringIsSurrogateHighCharacter([value characterAtIndex:i])&&i+1<value.length&&CFStringIsSurrogateLowCharacter([value characterAtIndex:i+1]))i++;
  return i;
}
UIColor* color(Rml::Colourb value) { return [UIColor colorWithRed:value.red/255.0 green:value.green/255.0 blue:value.blue/255.0 alpha:value.alpha/255.0]; }
UIFont* font(CGFloat size) {
  static NSString* name=nil;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    NSString* file=[[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"game/fonts/NotoSansKR.ttf"];
    CGDataProviderRef data=CGDataProviderCreateWithURL((__bridge CFURLRef)[NSURL fileURLWithPath:file]);
    if(data){CGFontRef face=CGFontCreateWithDataProvider(data);if(face){CTFontManagerRegisterGraphicsFont(face,nullptr);name=CFBridgingRelease(CGFontCopyPostScriptName(face));CGFontRelease(face);}CGDataProviderRelease(data);}
  });
  return (name?[UIFont fontWithName:name size:size]:nil)?:[UIFont systemFontOfSize:size];
}
}

struct IOSTextInput::Impl {
  SDL_Window* window;
  SDL_Renderer* renderer;
  Rml::ElementFormControlInput* input=nullptr;
  RaisingTextField* field;
  RaisingTextDelegate* delegate;
  std::string identity,binding;
  bool rebuilding=false,updating=false,stopping=false;
  int sessions=0,font_updates=0,text_writes=0;
  float font_size=0;int font_weight=-1;std::string font_family;
  CGRect keyboard_screen=CGRectNull;
  id keyboard_observer,hide_observer;
  bool lifted=false;
  std::map<std::string,std::string> font_names;

  Impl(SDL_Window* window,SDL_Renderer* renderer):window(window),renderer(renderer) {
    field=[[RaisingTextField alloc] initWithFrame:CGRectZero];
    field.borderStyle=UITextBorderStyleNone;
    field.autocorrectionType=UITextAutocorrectionTypeNo;
    field.spellCheckingType=UITextSpellCheckingTypeNo;
    field.autocapitalizationType=UITextAutocapitalizationTypeSentences;
    field.returnKeyType=UIReturnKeyDone;
    field.accessibilityIdentifier=@"raising-native-text-input";

    delegate=[RaisingTextDelegate new];
    auto* self=this;
    auto* notifications=[NSNotificationCenter defaultCenter];
    keyboard_observer=[notifications addObserverForName:UIKeyboardWillChangeFrameNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification* note){
      if(self->field.isFirstResponder)self->keyboard_screen=[note.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
    }];
    hide_observer=[notifications addObserverForName:UIKeyboardWillHideNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification*){
      if(self->field.isFirstResponder)self->keyboard_screen=CGRectNull;
    }];
    field.navigate=^(BOOL reverse,BOOL dismiss){
      [self->field unmarkText];self->changed();
      if(dismiss){if(self->input)self->input->Blur();self->stop();}
      else if(auto* context=Rml::GetContext("game")){const int modifiers=reverse?Rml::Input::KM_SHIFT:0;context->ProcessKeyDown(Rml::Input::KI_TAB,modifiers);context->ProcessKeyUp(Rml::Input::KI_TAB,modifiers);}
    };
    delegate.changed=^{self->changed();};
    delegate.selectionChanged=^{self->selection();};
    delegate.finished=^{if(self->input)self->input->Blur();self->stop();};
    field.delegate=delegate;
    [field addTarget:delegate action:@selector(valueChanged:) forControlEvents:UIControlEventEditingChanged];
  }
  ~Impl() {
    [[NSNotificationCenter defaultCenter] removeObserver:keyboard_observer];
    [[NSNotificationCenter defaultCenter] removeObserver:hide_observer];
    input=nullptr;field.navigate=nil;delegate.changed=nil;delegate.selectionChanged=nil;delegate.finished=nil;
    field.delegate=nil;[field.undoManager removeAllActions];[field resignFirstResponder];[field removeFromSuperview];
  }
  UIWindow* uiwindow() const {return (__bridge UIWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER,nullptr);}
  void selection() {
    if(updating||!input||!field.selectedTextRange)return;
    const NSInteger start=[field offsetFromPosition:field.beginningOfDocument toPosition:field.selectedTextRange.start],end=[field offsetFromPosition:field.beginningOfDocument toPosition:field.selectedTextRange.end];
    updating=true;input->SetSelectionRange(characters(field.text,start),characters(field.text,end));updating=false;
  }
  void changed() {
    if(updating||!input||input->IsDisabled())return;
    updating=true;
    const std::string value=field.text.UTF8String?:"";
    const bool composing=field.markedTextRange!=nil;
    // RmlUi remains the authored value source. Never rewrite UIKit's marked
    // range while an IME is composing; reconcile after it commits.
    input->SetValue(value);
    input->DispatchEvent("change",{{"value",Rml::Variant(input->GetValue())}});
    if(!composing&&input->GetValue()!=value){field.text=string(input->GetValue());++text_writes;}
    updating=false;selection();
  }
  void stop() {
    if(rebuilding||stopping)return;
    stopping=true;
    [field resignFirstResponder];
    changed();
    if(input)input->RemoveProperty("opacity");
    input=nullptr;identity.clear();binding.clear();keyboard_screen=CGRectNull;[field removeFromSuperview];stopping=false;
  }
  void activate(Rml::ElementFormControlInput* next) {
    if(stopping||!next||next->IsDisabled())return;
    if(input==next){update();return;}
    if(!identity.empty())stop();
    input=next;identity=next->GetId();binding=next->GetAttribute<Rml::String>("data-user-field","");input->SetProperty("opacity","0");
    updating=true;field.text=string(next->GetValue());++text_writes;
    int start=0,end=0;next->GetSelection(&start,&end,nullptr);
    UITextPosition* a=[field positionFromPosition:field.beginningOfDocument offset:utf16(field.text,start)];
    UITextPosition* b=[field positionFromPosition:field.beginningOfDocument offset:utf16(field.text,end)];
    if(a&&b)field.selectedTextRange=[field textRangeFromPosition:a toPosition:b];
    // Use UITextField's own editing manager; a separate override does not
    // receive UIKit's text operations. Clear history when binding a new field.
    [field.undoManager removeAllActions];
    updating=false;++sessions;
    update();
    [field becomeFirstResponder];
  }
  void update() {
    if(!input||updating||!renderer)return;
    auto* context=Rml::GetContext("game");
    if(!rebuilding&&(!context||context->GetFocusElement()!=input)){stop();return;}
    const auto& box=input->GetBox();const auto& style=input->GetComputedValues();
    const auto offset=input->GetAbsoluteOffset(Rml::BoxArea::Border),size=box.GetSize(Rml::BoxArea::Border);
    float x=0,y=0,right=0,bottom=0;
    if(!SDL_RenderCoordinatesToWindow(renderer,offset.x,offset.y,&x,&y)||!SDL_RenderCoordinatesToWindow(renderer,offset.x+size.x,offset.y+size.y,&right,&bottom))return;
    UIWindow* host=uiwindow();if(!host)return;
    if(field.superview!=host)[host addSubview:field];
    CGRect frame=CGRectMake(x,y,std::max(1.f,right-x),std::max(1.f,bottom-y));
    // Keep a focused field usable even when its authored position is covered by
    // the software keyboard. Only this native editor moves; game layout/state
    // and foreign controllers such as Files keep their own geometry.
    const CGRect natural=frame;
    if(!CGRectIsNull(keyboard_screen)){
      const CGRect keyboard=[host convertRect:keyboard_screen fromCoordinateSpace:host.screen.coordinateSpace];
      if(CGRectIntersectsRect(frame,keyboard))frame.origin.y=std::max(CGFloat(host.safeAreaInsets.top),CGRectGetMinY(keyboard)-frame.size.height-8);
    }
    lifted=!CGRectEqualToRect(natural,frame);
    if(!CGRectEqualToRect(field.frame,frame))field.frame=frame;
    const auto scale=(right-x)/std::max(1.f,size.x);
    const auto px=std::max(1.f,style.font_size()*scale);
    const auto family=style.font_family();const int weight=static_cast<int>(style.font_weight());
    // Subpixel coordinate conversion during rotation must not reassign UIFont
    // and interrupt a composing keyboard for an effectively unchanged style.
    if(font_family!=family||font_weight!=weight||std::abs(font_size-px)>0.01f){
      const auto found=font_names.find(family);
      UIFont* desiredFont=found==font_names.end()?font(px):[UIFont fontWithName:string(found->second) size:px];
      if(!desiredFont)desiredFont=font(px);
      // The bundled variable font defaults to Thin; use the authored weight.
      UIFontDescriptor* descriptor=[desiredFont.fontDescriptor fontDescriptorByAddingAttributes:@{(__bridge NSString*)kCTFontVariationAttribute:@{@(0x77676874):@(weight)}}];
      field.font=[UIFont fontWithDescriptor:descriptor size:px];
      font_family=family;font_weight=weight;font_size=px;++font_updates;
    }
    const auto padding=box.GetPosition(Rml::BoxArea::Content),content=box.GetSize();
    field.textInsets=UIEdgeInsetsMake(padding.y*scale,padding.x*scale,(size.y-padding.y-content.y)*scale,(size.x-padding.x-content.x)*scale);
    field.layer.borderWidth=box.GetEdge(Rml::BoxArea::Border,Rml::BoxEdge::Top)*scale;
    field.layer.borderColor=color(style.border_top_color()).CGColor;
    field.layer.cornerRadius=style.border_top_left_radius()*scale;
    const auto alignment=style.text_align()==Rml::Style::TextAlign::Right?NSTextAlignmentRight:style.text_align()==Rml::Style::TextAlign::Center?NSTextAlignmentCenter:NSTextAlignmentLeft;
    if(field.textAlignment!=alignment)field.textAlignment=alignment;
    UIColor* ink=color(input->GetProperty<Rml::Colourb>("color"));if(![field.textColor isEqual:ink])field.textColor=ink;
    auto background_color=style.background_color();
    if(lifted&&background_color.alpha==0)for(auto* parent=input->GetParentNode();parent;parent=parent->GetParentNode())if(parent->GetComputedValues().background_color().alpha==255){background_color=parent->GetComputedValues().background_color();break;}
    UIColor* background=color(background_color);if(![field.backgroundColor isEqual:background])field.backgroundColor=background;
    // Native text, selection and marked text paint once, above the authored box.
    field.accessibilityLabel=string(input->GetAttribute<Rml::String>("aria-label",identity));
    if(!field.markedTextRange&&input->GetValue()!=std::string(field.text.UTF8String?:"")){
      updating=true;field.text=string(input->GetValue());++text_writes;updating=false;
    }
  }
  nlohmann::json report() const {
    nlohmann::json result={{"active",input!=nullptr},{"native",true},{"sessions",sessions},{"firstResponder",bool(field.isFirstResponder)},{"marked",field.markedTextRange!=nil}};
    if(field.selectedTextRange){result["selectionStart"]=[field offsetFromPosition:field.beginningOfDocument toPosition:field.selectedTextRange.start];result["selectionEnd"]=[field offsetFromPosition:field.beginningOfDocument toPosition:field.selectedTextRange.end];}
    if(field.markedTextRange)result["markedText"]=std::string([field textInRange:field.markedTextRange].UTF8String?:"");
    result["canUndo"]=bool(field.undoManager.canUndo);result["canRedo"]=bool(field.undoManager.canRedo);
    result["fontUpdates"]=font_updates;result["textWrites"]=text_writes;result["fontSize"]=font_size;
    result["lifted"]=lifted;result["frame"]={{"x",field.frame.origin.x},{"y",field.frame.origin.y},{"width",field.frame.size.width},{"height",field.frame.size.height}};
    result["language"]=std::string(field.textInputMode.primaryLanguage.UTF8String?:"");
    if(input){result["value"]=std::string(field.text.UTF8String?:"");result["fieldId"]=identity;}
    return result;
  }
};

IOSTextInput::IOSTextInput(SDL_Window* window,SDL_Renderer* renderer):impl(std::make_unique<Impl>(window,renderer)){}
IOSTextInput::~IOSTextInput()=default;
void IOSTextInput::begin_rebuild(){if(impl->input){impl->rebuilding=true;impl->input=nullptr;}}
void IOSTextInput::end_rebuild(){
  if(!impl->rebuilding)return;
  auto* context=Rml::GetContext("game");
  auto* next=context?dynamic_cast<Rml::ElementFormControlInput*>(context->GetFocusElement()):nullptr;
  if(next&&!next->IsDisabled()&&next->GetId()==impl->identity&&next->GetAttribute<Rml::String>("data-user-field","")==impl->binding){impl->input=next;next->SetProperty("opacity","0");}
  impl->rebuilding=false;
  if(!impl->input)impl->stop();else {impl->update();impl->selection();}
}
void IOSTextInput::update(){
  // These UIKit calls run from the C++ frame loop, outside UIKit's event
  // callbacks. Drain their autoreleased temporaries on every frame.
  @autoreleasepool {
    if(impl->rebuilding)return;
    auto* context=Rml::GetContext("game");
    auto* focused=context?dynamic_cast<Rml::ElementFormControlInput*>(context->GetFocusElement()):nullptr;
    if(focused&&focused->IsDisabled())focused=nullptr;
    if(focused!=impl->input){if(focused)impl->activate(focused);else impl->stop();}
    impl->update();
  }
}
nlohmann::json IOSTextInput::report() const{
  @autoreleasepool {return impl->report();}
}

bool IOSTextInput::owns_keyboard() const{return impl->input&&impl->field.isFirstResponder;}
void IOSTextInput::register_font(const std::string& family,const std::string& path){
  CGDataProviderRef data=CGDataProviderCreateWithURL((__bridge CFURLRef)[NSURL fileURLWithPath:string(path)]);
  if(!data)return;
  CGFontRef face=CGFontCreateWithDataProvider(data);
  if(face){CTFontManagerRegisterGraphicsFont(face,nullptr);NSString* name=CFBridgingRelease(CGFontCopyPostScriptName(face));if(name)impl->font_names[family]=name.UTF8String;CGFontRelease(face);}
  CGDataProviderRelease(data);
}
