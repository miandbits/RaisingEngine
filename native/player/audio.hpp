#pragma once
#include "vendor/miniaudio/miniaudio.h"
#include <map>
#include <memory>
#include <string>
#include <stdexcept>

// Device initialization is lazy: a silent project needs no audio device.
class GameAudio {
  ma_engine engine{};
  bool initialized=false;
  std::map<std::string,std::unique_ptr<ma_sound>> sounds;
  ma_sound* music=nullptr;
  std::string current_music;
  void initialize() {
    if(initialized)return;
    auto config=ma_engine_config_init();
    if(ma_engine_init(&config,&engine)!=MA_SUCCESS)throw std::runtime_error("오디오 장치를 열 수 없습니다.");
    initialized=true;
  }
  ma_sound* sound(const std::string& file,bool background=false) {
    initialize();
    const auto key=std::string(background?"music:":"effect:")+file;
    auto it=sounds.find(key);if(it!=sounds.end())return it->second.get();
    auto value=std::make_unique<ma_sound>();
    if(ma_sound_init_from_file(&engine,file.c_str(),MA_SOUND_FLAG_DECODE,nullptr,nullptr,value.get())!=MA_SUCCESS)
      throw std::runtime_error("오디오 파일을 재생할 수 없습니다: "+file);
    auto* result=value.get();sounds[key]=std::move(value);return result;
  }
public:
  ~GameAudio(){for(auto& [_,s]:sounds)ma_sound_uninit(s.get());if(initialized)ma_engine_uninit(&engine);}
  uint64_t cursor() const {ma_uint64 value=0;if(music)ma_sound_get_cursor_in_pcm_frames(music,&value);return value;}
  void seek(uint64_t value){if(music&&ma_sound_seek_to_pcm_frame(music,value)!=MA_SUCCESS)throw std::runtime_error("음악 재생 위치를 복원할 수 없습니다.");}
  bool ready() const{return initialized;}
  bool playing() const{return music&&ma_sound_is_playing(music);}
  size_t loaded_count() const{return sounds.size();}
  void mute(bool muted){if(initialized)ma_engine_set_volume(&engine,muted?0.f:1.f);}
  void suspend(bool paused){if(initialized){if(paused)ma_engine_stop(&engine);else ma_engine_start(&engine);}}
  void sync_music(const std::string& file,bool muted){
    if(file==current_music){mute(muted);return;}
    if(music){ma_sound_stop(music);music=nullptr;}
    current_music.clear();
    if(!file.empty()){music=sound(file,true);ma_sound_set_looping(music,MA_TRUE);ma_sound_seek_to_pcm_frame(music,0);ma_sound_start(music);current_music=file;}
    mute(muted);
  }
  void effect(const std::string& file,bool muted){effect(file,muted,1.f,0.f);}
  /** volume 0..1, pan -1 (left)..1 (right): plugin event sounds (extension notifications). */
  void effect(const std::string& file,bool muted,float volume,float pan){if(file.empty()||muted)return;auto* value=sound(file);ma_sound_set_looping(value,MA_FALSE);ma_sound_set_volume(value,volume);ma_sound_set_pan(value,pan);ma_sound_seek_to_pcm_frame(value,0);ma_sound_start(value);}
};
