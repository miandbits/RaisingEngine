#include "portable-save-files.hpp"
#include "portable-save-input.hpp"
#include <SDL3/SDL.h>
#import <UIKit/UIKit.h>
#import <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <atomic>
#include <fstream>
#include <mutex>
#include <set>

namespace raising_player {
struct IOSPortableRequest;
struct IOSPortableState;
}
@interface RaisingPortableDocumentDelegate : NSObject <UIDocumentPickerDelegate,UIAdaptivePresentationControllerDelegate> {
 @public
  std::weak_ptr<raising_player::IOSPortableState> owner;
  std::weak_ptr<raising_player::IOSPortableRequest> operation;
}
@end
namespace raising_player {
struct IOSPortableRequest {
  std::uint64_t id=0;
  PortableSaveKind kind=PortableSaveKind::import_file;
  std::atomic<bool> cancelled{false};
  bool selected=false,finishing=false;
  std::string displayName="Backup";
  std::optional<PortableSaveResult> result;
  std::shared_ptr<PortableSaveStagedFile> file;
  __strong UIDocumentPickerViewController* picker=nil;
  __strong RaisingPortableDocumentDelegate* delegate=nil;
  __strong NSFileCoordinator* coordinator=nil;
};
struct IOSPortableState {
  mutable std::mutex mutex;
  bool alive=true,copying=false;
  __weak UIWindow* window=nil;
  std::filesystem::path root;
  std::shared_ptr<IOSPortableRequest> active;
};
static void main_thread(dispatch_block_t work){if([NSThread isMainThread])work();else dispatch_async(dispatch_get_main_queue(),work);}
static bool current(const std::shared_ptr<IOSPortableState>& state,const std::shared_ptr<IOSPortableRequest>& request){
  std::lock_guard<std::mutex> lock(state->mutex);return state->alive&&state->active==request&&!request->result;
}
static PortableSaveResult terminal(const std::shared_ptr<IOSPortableRequest>& request,PortableSaveStatus status,const std::string& error={}){
  return {request->id,request->kind,status,status==PortableSaveStatus::chosen&&request->kind==PortableSaveKind::import_file?request->file:nullptr,error,request->displayName};
}
// Escaping Objective-C blocks capture C++ reference parameters as references.
// These callbacks must own their state/request copies until presentation/copy ends.
static void finish(std::shared_ptr<IOSPortableState> state,std::shared_ptr<IOSPortableRequest> request,PortableSaveStatus status,std::string error={}){
  main_thread(^{
    {std::lock_guard<std::mutex> lock(state->mutex);if(!state->alive||state->active!=request||request->finishing||request->result)return;request->finishing=true;}
    auto publish=^{
      request->picker.delegate=nil;request->picker.presentationController.delegate=nil;
      request->picker=nil;request->delegate=nil;request->coordinator=nil;
      std::lock_guard<std::mutex> lock(state->mutex);
      if(state->alive&&state->active==request&&!request->result)request->result=terminal(request,request->cancelled?PortableSaveStatus::cancelled:status,error);
    };
    if(request->picker.presentingViewController)[request->picker dismissViewControllerAnimated:YES completion:publish];else publish();
  });
}
static std::shared_ptr<PortableSaveStagedFile> make_stage(const std::shared_ptr<IOSPortableState>& state){
  return PortableSaveStagedFile::create(state->root,[[NSUUID UUID].UUIDString UTF8String]);
}
static void write_snapshot(const std::shared_ptr<PortableSaveStagedFile>& file,const std::string& bytes){
  std::ofstream output(file->path,std::ios::binary|std::ios::trunc);
  if(!output)throw std::runtime_error("write-failed");
  output.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));output.close();
  if(!output)throw std::runtime_error("write-failed");
}
static std::string display_name(NSURL* url){
  NSMutableCharacterSet* removed=[[NSCharacterSet controlCharacterSet] mutableCopy];
  [removed formUnionWithCharacterSet:[NSCharacterSet newlineCharacterSet]];
  NSString* label=[[url.lastPathComponent componentsSeparatedByCharactersInSet:removed] componentsJoinedByString:@" "];
  label=[label stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
  NSData* utf8=[label dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO];
  if(!utf8||!utf8.length)return "Backup";
  std::string value(static_cast<const char*>(utf8.bytes),std::min<std::size_t>(utf8.length,256));
  while(!value.empty()&&!portable_input::valid_utf8(value))value.pop_back();
  return value.empty()?"Backup":value;
}
static void selected(std::shared_ptr<IOSPortableState> state,std::shared_ptr<IOSPortableRequest> request,NSArray<NSURL*>* urls){
  if(!current(state,request)||request->cancelled||request->selected)return;
  request->selected=true;
  if(urls.count!=1){finish(state,request,PortableSaveStatus::error,"unreadable");return;}
  if(request->kind==PortableSaveKind::export_file){finish(state,request,PortableSaveStatus::chosen);return;}
  NSURL* selectedURL=urls.firstObject;request->displayName=display_name(selectedURL);
  if(!selectedURL.isFileURL){finish(state,request,PortableSaveStatus::error,"unreadable");return;}
  NSFileCoordinator* coordinator=[[NSFileCoordinator alloc] initWithFilePresenter:nil];request->coordinator=coordinator;
  {std::lock_guard<std::mutex> lock(state->mutex);state->copying=true;}
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{@autoreleasepool {
    // asCopy may already return an accessible sandbox URL. A false scope return
    // is not itself failure; always balance a successful acquisition.
    const BOOL scoped=[selectedURL startAccessingSecurityScopedResource];
    __block std::string error;__block bool copied=false;
    @try {
      NSError* coordinationError=nil;
      [coordinator coordinateReadingItemAtURL:selectedURL options:0 error:&coordinationError byAccessor:^(NSURL* accessorURL){
        if(request->cancelled)return;
        try {
          if(!accessorURL.isFileURL||!accessorURL.fileSystemRepresentation)throw portable_input::Error(portable_input::Code::unreadable);
          auto inputPath=std::filesystem::path(accessorURL.fileSystemRepresentation);
          std::error_code ec;
          if(!std::filesystem::is_regular_file(inputPath,ec)||ec)throw portable_input::Error(portable_input::Code::unreadable);
          auto size=std::filesystem::file_size(inputPath,ec);if(ec)throw portable_input::Error(portable_input::Code::unreadable);
          if(size>portable_input::max_bytes)throw portable_input::Error(portable_input::Code::too_large);
          std::ifstream input(inputPath,std::ios::binary);if(!input)throw portable_input::Error(portable_input::Code::unreadable);
          auto bytes=portable_input::bounded_read(input);
          if(request->cancelled)return;
          request->file=make_stage(state);write_snapshot(request->file,bytes);copied=true;
        }catch(const portable_input::Error& e){error=portable_input::code_name(e.code);}
         catch(const std::bad_alloc&){error="resource-limit";}
         catch(const std::exception&){error="write-failed";}
      }];
      if(coordinationError&&[coordinationError.domain isEqualToString:NSCocoaErrorDomain]&&coordinationError.code==NSUserCancelledError)request->cancelled=true;
      if(coordinationError&&!request->cancelled&&error.empty())error="unreadable";
    } @catch(NSException*) {error="unreadable";}
      @finally {if(scoped)[selectedURL stopAccessingSecurityScopedResource];}
    {std::lock_guard<std::mutex> lock(state->mutex);state->copying=false;}
    if(request->cancelled)finish(state,request,PortableSaveStatus::cancelled);
    else if(copied&&error.empty())finish(state,request,PortableSaveStatus::chosen);
    else finish(state,request,PortableSaveStatus::error,error.empty()?"unreadable":error);
  }});
}
static void present(std::shared_ptr<IOSPortableState> state,std::shared_ptr<IOSPortableRequest> request){
  main_thread(^{
    if(!current(state,request)||request->cancelled)return;
    UIWindow* window=state->window;UIViewController* presenter=window.rootViewController;
    // Do not cover somebody else's UIKit modal or present through a detached view.
    if(!window||window.hidden||!presenter||!presenter.view.window||presenter.presentedViewController||presenter.isBeingPresented||presenter.isBeingDismissed||(window.windowScene&&window.windowScene.activationState!=UISceneActivationStateForegroundActive)){finish(state,request,PortableSaveStatus::error,"presentation-failed");return;}
    @try {
      UIDocumentPickerViewController* picker;
      if(request->kind==PortableSaveKind::import_file)picker=[[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeJSON] asCopy:YES];
      else {
        auto source=request->file->path.u8string();NSString* path=[[NSString alloc] initWithBytes:source.data() length:source.size() encoding:NSUTF8StringEncoding];
        if(!path){finish(state,request,PortableSaveStatus::error,"unreadable");return;}
        picker=[[UIDocumentPickerViewController alloc] initForExportingURLs:@[[NSURL fileURLWithPath:path]] asCopy:YES];
      }
      if(!picker){finish(state,request,PortableSaveStatus::error,"presentation-failed");return;}
      auto delegate=[[RaisingPortableDocumentDelegate alloc] init];delegate->owner=state;delegate->operation=request;
      request->picker=picker;request->delegate=delegate;picker.delegate=delegate;
      picker.allowsMultipleSelection=NO;picker.shouldShowFileExtensions=YES;
      picker.modalPresentationStyle=UIModalPresentationFormSheet;
      // A form sheet adapts to rotation/iPad without popover source rectangles.
      picker.presentationController.delegate=delegate;
      [presenter presentViewController:picker animated:YES completion:^{
        picker.presentationController.delegate=delegate;
        if(!current(state,request)||request->cancelled)[picker dismissViewControllerAnimated:NO completion:nil];
      }];
    } @catch(NSException*) {finish(state,request,PortableSaveStatus::error,"presentation-failed");}
  });
}
class IOSPortableBackend final:public PortableSaveFilesBackend {
  std::shared_ptr<IOSPortableState> state_;
  std::shared_ptr<IOSPortableRequest> begin(std::uint64_t id,PortableSaveKind kind){
    std::lock_guard<std::mutex> lock(state_->mutex);
    // At most one active/provider copy, even if a cancelled provider is slow to
    // release coordination. New requests fail busy until that worker retires.
    if(!state_->alive||!state_->window||state_->active||state_->copying)return {};
    auto request=std::make_shared<IOSPortableRequest>();request->id=id;request->kind=kind;state_->active=request;return request;
  }
 public:
  IOSPortableBackend(SDL_Window* window,const std::filesystem::path& root):state_(std::make_shared<IOSPortableState>()){
    static std::mutex cleanupMutex;static std::set<std::filesystem::path> cleaned;
    {std::lock_guard<std::mutex> lock(cleanupMutex);if(cleaned.insert(root.lexically_normal()).second)PortableSaveStagedFile::cleanup_abandoned(root);}
    state_->root=root;state_->window=(__bridge UIWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER,nullptr);
  }
  ~IOSPortableBackend()override{shutdown();}
  bool available()const override{std::lock_guard<std::mutex> lock(state_->mutex);return state_->alive&&state_->window;}
  bool busy()const override{std::lock_guard<std::mutex> lock(state_->mutex);return state_->active||state_->copying;}
  bool begin_import(std::uint64_t id)override{auto request=begin(id,PortableSaveKind::import_file);if(!request)return false;present(state_,request);return true;}
  bool begin_export(std::uint64_t id,const std::filesystem::path& snapshot)override{
    auto request=begin(id,PortableSaveKind::export_file);if(!request)return false;
    try {
      std::error_code ec;if(!std::filesystem::is_regular_file(snapshot,ec)||ec)throw portable_input::Error(portable_input::Code::unreadable);
      auto size=std::filesystem::file_size(snapshot,ec);if(ec)throw portable_input::Error(portable_input::Code::unreadable);
      if(size>portable_input::max_bytes)throw portable_input::Error(portable_input::Code::too_large);
      std::ifstream input(snapshot,std::ios::binary);if(!input)throw portable_input::Error(portable_input::Code::unreadable);
      auto bytes=portable_input::bounded_read(input);request->file=make_stage(state_);write_snapshot(request->file,bytes);present(state_,request);
    }catch(const portable_input::Error& e){finish(state_,request,PortableSaveStatus::error,portable_input::code_name(e.code));}
     catch(const std::bad_alloc&){finish(state_,request,PortableSaveStatus::error,"resource-limit");}
     catch(const std::exception&){finish(state_,request,PortableSaveStatus::error,"write-failed");}
    return true;
  }
  std::optional<PortableSaveResult> poll()override{
    std::lock_guard<std::mutex> lock(state_->mutex);
    if(!state_->alive||!state_->active||!state_->active->result)return std::nullopt;
    auto result=std::move(state_->active->result);state_->active.reset();return result;
  }
  void cancel(std::uint64_t id)override{
    std::shared_ptr<IOSPortableRequest> request;
    {std::lock_guard<std::mutex> lock(state_->mutex);request=state_->active;if(!request||request->id!=id||request->result)return;request->cancelled=true;}
    auto state=state_;main_thread(^{[request->coordinator cancel];finish(state,request,PortableSaveStatus::cancelled);});
  }
  void shutdown()override{
    std::shared_ptr<IOSPortableRequest> request;
    {std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->alive)return;state_->alive=false;request=std::move(state_->active);if(request)request->cancelled=true;}
    if(request)main_thread(^{
      [request->coordinator cancel];request->picker.delegate=nil;request->picker.presentationController.delegate=nil;
      [request->picker dismissViewControllerAnimated:NO completion:nil];request->picker=nil;request->delegate=nil;request->coordinator=nil;
    });
  }
};
PortableSaveFiles::PortableSaveFiles(SDL_Window* window,const std::filesystem::path& root):backend_(std::make_shared<IOSPortableBackend>(window,root)){}
}
@implementation RaisingPortableDocumentDelegate
- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
  (void)controller;auto state=owner.lock();auto request=operation.lock();if(state&&request)raising_player::selected(state,request,urls);
}
- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
  (void)controller;auto state=owner.lock();auto request=operation.lock();if(state&&request){request->cancelled=true;raising_player::finish(state,request,raising_player::PortableSaveStatus::cancelled);}
}
- (void)presentationControllerDidDismiss:(UIPresentationController*)controller {
  (void)controller;auto state=owner.lock();auto request=operation.lock();if(state&&request&&!request->selected){request->cancelled=true;raising_player::finish(state,request,raising_player::PortableSaveStatus::cancelled);}
}
@end
