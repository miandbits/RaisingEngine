// Property-only environment doubles: the complete production transform is inserted
// unchanged. These tests prove its bounds contract, not UIKit runtime timing.
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#include <iostream>
#include <string>
typedef NS_ENUM(NSInteger,UIInterfaceOrientation){UIInterfaceOrientationUnknown=0,UIInterfaceOrientationPortrait=1,UIInterfaceOrientationPortraitUpsideDown=2,UIInterfaceOrientationLandscapeLeft=3,UIInterfaceOrientationLandscapeRight=4};
typedef NSUInteger UIInterfaceOrientationMask;
static constexpr UIInterfaceOrientationMask UIInterfaceOrientationMaskPortrait=1<<1,UIInterfaceOrientationMaskPortraitUpsideDown=1<<2,UIInterfaceOrientationMaskLandscapeLeft=1<<3,UIInterfaceOrientationMaskLandscapeRight=1<<4;
static BOOL UIInterfaceOrientationIsLandscape(UIInterfaceOrientation o){return o==UIInterfaceOrientationLandscapeLeft||o==UIInterfaceOrientationLandscapeRight;}
@interface UIWindowScene:NSObject
@property UIInterfaceOrientation interfaceOrientation;
@end
@implementation UIWindowScene
@end
@interface UIScreen:NSObject
@property CGRect bounds;
@end
@implementation UIScreen
@end
@interface UIWindow:NSObject
@property CGRect bounds;
@property(strong) UIWindowScene* windowScene;
@end
@implementation UIWindow
@end
@interface SDL_UIKitWindowData:NSObject
@property(strong) UIWindow* uiwindow;
@end
@implementation SDL_UIKitWindowData
@end
@interface UIApplication:NSObject
@property UIInterfaceOrientation statusBarOrientation;
+(instancetype)sharedApplication;
@end
@implementation UIApplication
+(instancetype)sharedApplication{static UIApplication* value=[UIApplication new];return value;}
@end
struct SDL_Window{void* internal;UIInterfaceOrientationMask mask;};
static UIInterfaceOrientationMask UIKit_GetSupportedOrientations(SDL_Window* window){return window->mask;}
static double systemVersion=26.5;
static bool UIKit_IsSystemVersionAtLeast(double version){return systemVersion>=version;}
// The test driver inserts the complete hash-verified production function here.
// PRODUCTION_FUNCTION
int main(){@autoreleasepool{
 struct Case{const char* name;double version;UIInterfaceOrientation orientation;CGRect screen,window,expected;bool scene;};
 const CGRect portrait=CGRectMake(0,0,390,844),landscape=CGRectMake(0,0,844,390),split=CGRectMake(0,0,640,390),offset=CGRectMake(4,8,844,390);
 const Case cases[]={
  {"modern unknown full landscape",26.5,UIInterfaceOrientationUnknown,landscape,landscape,landscape,true},
  {"modern iOS13 boundary",13.0,UIInterfaceOrientationUnknown,landscape,landscape,landscape,true},
  {"legacy below iOS13",12.9,UIInterfaceOrientationUnknown,landscape,landscape,portrait,true},
  {"modern known portrait",26.5,UIInterfaceOrientationPortrait,portrait,portrait,portrait,true},
  {"modern known landscape",26.5,UIInterfaceOrientationLandscapeLeft,landscape,landscape,landscape,true},
  {"modern unknown portrait",26.5,UIInterfaceOrientationUnknown,portrait,portrait,portrait,true},
  {"modern split bounds",26.5,UIInterfaceOrientationUnknown,landscape,split,split,true},
  {"modern bounds origin",26.5,UIInterfaceOrientationUnknown,offset,offset,offset,true},
  {"modern absent scene",26.5,UIInterfaceOrientationUnknown,landscape,landscape,landscape,false},
  {"legacy portrait workaround",10.0,UIInterfaceOrientationPortrait,landscape,landscape,portrait,true},
  {"legacy landscape workaround",10.0,UIInterfaceOrientationLandscapeLeft,portrait,portrait,landscape,true},
  {"legacy split unchanged",10.0,UIInterfaceOrientationPortrait,landscape,split,split,true}
 };
 int failures=0;
 for(const auto& test:cases){systemVersion=test.version;
  UIScreen* screen=[UIScreen new];screen.bounds=test.screen;UIWindow* window=[UIWindow new];window.bounds=test.window;
  if(test.scene){window.windowScene=[UIWindowScene new];window.windowScene.interfaceOrientation=test.orientation;}
  [UIApplication sharedApplication].statusBarOrientation=test.orientation;
  SDL_UIKitWindowData* data=[SDL_UIKitWindowData new];data.uiwindow=window;
  SDL_Window sdl{(__bridge void*)data,UIInterfaceOrientationMaskPortrait|UIInterfaceOrientationMaskLandscapeLeft|UIInterfaceOrientationMaskLandscapeRight};
  const CGRect actual=UIKit_ComputeViewFrame(&sdl,screen);
  const bool pass=CGRectEqualToRect(actual,test.expected);if(!pass)++failures;
  std::cout<<(pass?"PASS ":"FAIL ")<<test.name<<" actual="<<actual.origin.x<<","<<actual.origin.y<<","<<actual.size.width<<","<<actual.size.height<<" expected="<<test.expected.origin.x<<","<<test.expected.origin.y<<","<<test.expected.size.width<<","<<test.expected.size.height<<"\n";
 }
 return failures?1:0;
}}
