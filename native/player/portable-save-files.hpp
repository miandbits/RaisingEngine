#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
struct SDL_Window;
namespace raising_player {
enum class PortableSaveKind { import_file, export_file };
enum class PortableSaveStatus { chosen, cancelled, error };
// The last lease removes only this request's file and directory. It never owns
// an external provider URL, the caller's export snapshot, or a real save slot.
class PortableSaveStagedFile final {
  std::filesystem::path directory_;
  static constexpr const char* marker=".raising-portable-stage-v1";
  static constexpr const char* signature="RaisingEngine portable save staging v1\n";
  explicit PortableSaveStagedFile(std::filesystem::path directory):directory_(std::move(directory)),path(directory_/"raising-save.json"){}
 public:
  const std::filesystem::path path;
  PortableSaveStagedFile(const PortableSaveStagedFile&)=delete;
  PortableSaveStagedFile& operator=(const PortableSaveStagedFile&)=delete;
  ~PortableSaveStagedFile(){std::error_code ec;std::filesystem::remove(path,ec);std::filesystem::remove(directory_/"raising-save.json.tmp",ec);std::filesystem::remove(directory_/"raising-save.json.bak",ec);std::filesystem::remove(directory_/marker,ec);std::filesystem::remove(directory_,ec);}
  static std::shared_ptr<PortableSaveStagedFile> create(const std::filesystem::path& root,const std::string& uniqueName){
    if(uniqueName.empty()||uniqueName.size()>80||uniqueName.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-")!=std::string::npos)throw std::runtime_error("write-failed");
    std::error_code ec;std::filesystem::create_directories(root,ec);
    if(ec||std::filesystem::is_symlink(std::filesystem::symlink_status(root,ec))||ec)throw std::runtime_error("write-failed");
    auto directory=root/("request-"+uniqueName);
    if(!std::filesystem::create_directory(directory,ec)||ec)throw std::runtime_error("write-failed");
    auto file=std::shared_ptr<PortableSaveStagedFile>(new PortableSaveStagedFile(std::move(directory)));
    std::ofstream ownership(file->directory_/marker,std::ios::binary);ownership<<signature;ownership.close();
    if(!ownership)throw std::runtime_error("write-failed");
    return file;
  }
  // Call once per private root at process startup, before creating any leases.
  // A crash may leave requests behind. Never recurse or remove unknown files.
  static void cleanup_abandoned(const std::filesystem::path& root){
    std::error_code ec;if(!std::filesystem::is_directory(root,ec)||ec||std::filesystem::is_symlink(std::filesystem::symlink_status(root,ec))||ec)return;
    std::size_t checked=0;
    for(std::filesystem::directory_iterator it(root,ec),end;!ec&&it!=end&&checked++<512;it.increment(ec)){
      auto directory=it->path();auto name=directory.filename().u8string();
      if(name.rfind("request-",0)!=0||name.size()<=8||name.size()>88||name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-")!=std::string::npos)continue;
      auto status=std::filesystem::symlink_status(directory,ec);if(ec||!std::filesystem::is_directory(status)||std::filesystem::is_symlink(status))continue;
      auto markerPath=directory/marker;status=std::filesystem::symlink_status(markerPath,ec);if(ec){ec.clear();continue;}if(!std::filesystem::is_regular_file(status)||std::filesystem::is_symlink(status))continue;
      bool owned=false;
      { // Windows cannot remove the marker while its read handle is open.
        std::ifstream input(markerPath,std::ios::binary);char bytes[64]{};input.read(bytes,sizeof(bytes));
        owned=!input.bad()&&std::string(bytes,static_cast<std::size_t>(input.gcount()))==signature;
      }
      if(!owned)continue;
      std::filesystem::remove(directory/"raising-save.json",ec);ec.clear();std::filesystem::remove(directory/"raising-save.json.tmp",ec);ec.clear();std::filesystem::remove(directory/"raising-save.json.bak",ec);ec.clear();std::filesystem::remove(markerPath,ec);ec.clear();std::filesystem::remove(directory,ec);ec.clear();
    }
  }
};
struct PortableSaveResult {
  std::uint64_t requestId=0;
  PortableSaveKind kind=PortableSaveKind::import_file;
  PortableSaveStatus status=PortableSaveStatus::error;
  std::shared_ptr<PortableSaveStagedFile> file;
  // A fixed code only, never NSError.localizedDescription or a provider path.
  std::string error;
  // Sanitized basename only, UTF-8 <=256 bytes; never an external path.
  std::string displayName="Backup";
};
// Platform implementations and test doubles share only files/results, never game state.
class PortableSaveFilesBackend {
 public:
  virtual ~PortableSaveFilesBackend()=default;
  virtual bool available() const=0;
  virtual bool busy() const=0;
  virtual bool begin_import(std::uint64_t requestId)=0;
  virtual bool begin_export(std::uint64_t requestId,const std::filesystem::path& canonicalSnapshot)=0;
  virtual std::optional<PortableSaveResult> poll()=0;
  virtual void cancel(std::uint64_t requestId)=0;
  virtual void shutdown()=0;
};
class PortableSaveFiles final {
  std::shared_ptr<PortableSaveFilesBackend> backend_;
 public:
#if defined(RAISING_PORTABLE_SAVE_IOS)
  PortableSaveFiles(SDL_Window*,const std::filesystem::path& stagingRoot);
#else
  PortableSaveFiles(SDL_Window*,const std::filesystem::path&){}
#endif
#if defined(RAISING_PORTABLE_SAVE_TESTING)
  // Only test binaries contain this seam. A test must explicitly select it;
  // production desktop builds remain unavailable and claim no Files support.
  explicit PortableSaveFiles(std::shared_ptr<PortableSaveFilesBackend> backend):backend_(std::move(backend)){}
#endif
  ~PortableSaveFiles(){shutdown();}
  bool available() const{return backend_&&backend_->available();}
  bool busy() const{return backend_&&backend_->busy();}
  // False means unavailable/busy. Every accepted operation yields one terminal
  // result unless shutdown invalidates its owner. Host owns canonical preflight.
  bool begin_import(std::uint64_t requestId){return backend_&&backend_->begin_import(requestId);}
  // Copies this immutable app-owned snapshot before returning; never deletes it.
  bool begin_export(std::uint64_t requestId,const std::filesystem::path& canonicalSnapshot){return backend_&&backend_->begin_export(requestId,canonicalSnapshot);}
  std::optional<PortableSaveResult> poll(){return backend_?backend_->poll():std::nullopt;}
  void cancel(std::uint64_t requestId){if(backend_)backend_->cancel(requestId);}
  void shutdown(){if(backend_)backend_->shutdown();}
  PortableSaveFiles(const PortableSaveFiles&)=delete;
  PortableSaveFiles& operator=(const PortableSaveFiles&)=delete;
};
}
