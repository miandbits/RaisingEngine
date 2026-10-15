#pragma once
// Recording contains the current branch, including automatic narrative commands.
// Load/restart never retain checkpoint indices from an abandoned branch.
class RouteRecording {
public:
  fs::path file;
  Json saves=Json::array(), last_origin=nullptr;
  std::string previous;
  void reset(){saves=Json::array();previous.clear();last_origin=nullptr;}
  void checkpoint(const GameSession& game){if(!file.empty()&&!game.state.is_null())saves.push_back(game.commands);}
  void publish(const GameSession& game){
    if(file.empty()||game.state.is_null())return;
    if(!last_origin.is_null()&&last_origin!=game.origin)saves=Json::array();last_origin=game.origin;
    Json points=Json::array(),kept=Json::array();
    for(const auto& prefix:saves){bool same=prefix.size()<=game.commands.size();for(size_t i=0;same&&i<prefix.size();++i)same=prefix[i]==game.commands[i];if(same){kept.push_back(prefix);if(std::find(points.begin(),points.end(),Json(prefix.size()))==points.end())points.push_back(prefix.size());}}
    saves=std::move(kept);
    const Json value={{"recordingVersion",1},{"gameId",game.content.at("project").at("gameId")},{"contentFingerprint",game.content.at("contentFingerprint")},{"origin",game.origin},{"commands",game.commands},{"state",{{"phase",game.state.at("phase")},{"endingId",game.state.at("endingId")}}},{"checkpoints",points}};
    const auto text=value.dump(2);if(text==previous)return;
    fs::create_directories(file.parent_path());auto temp=file;temp+=".tmp";auto backup=file;backup+=".bak";
    {std::ofstream out(temp,std::ios::binary|std::ios::trunc);out<<text;out.flush();if(!out)throw std::runtime_error("플레이 경로 기록 파일을 쓰지 못했습니다.");}
    if(fs::exists(backup))fs::remove(backup);if(fs::exists(file))fs::rename(file,backup);
    try{fs::rename(temp,file);}catch(...){if(fs::exists(backup)&&!fs::exists(file))fs::rename(backup,file);throw;}
    previous=text;
  }
};
