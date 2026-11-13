#pragma once
#include <RmlUi/Core/FileInterface.h>
#include <SDL3/SDL_iostream.h>
#include <cstdio>

// RmlUi's default fopen interface uses the Windows active code page. SDL file
// I/O accepts UTF-8 paths on every platform and uses wide Windows file APIs.
class Utf8FileInterface : public Rml::FileInterface {
  static SDL_IOStream* stream(Rml::FileHandle file) {return reinterpret_cast<SDL_IOStream*>(file);}
public:
  Rml::FileHandle Open(const Rml::String& path) override {
    return reinterpret_cast<Rml::FileHandle>(SDL_IOFromFile(path.c_str(),"rb"));
  }
  void Close(Rml::FileHandle file) override {SDL_CloseIO(stream(file));}
  size_t Read(void* buffer,size_t size,Rml::FileHandle file) override {return SDL_ReadIO(stream(file),buffer,size);}
  bool Seek(Rml::FileHandle file,long offset,int origin) override {
    SDL_IOWhence whence;
    switch(origin){case SEEK_SET:whence=SDL_IO_SEEK_SET;break;case SEEK_CUR:whence=SDL_IO_SEEK_CUR;break;case SEEK_END:whence=SDL_IO_SEEK_END;break;default:return false;}
    return SDL_SeekIO(stream(file),offset,whence)>=0;
  }
  size_t Tell(Rml::FileHandle file) override {
    const auto position=SDL_TellIO(stream(file));return position<0?0:static_cast<size_t>(position);
  }
};
