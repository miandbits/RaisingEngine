#pragma once
// Presentation only: never sends a simulation command.
inline Json direction_config(const Json& content){return content.at("project").value("visual",Json::object()).value("direction",Json::object());}
inline Json direction_sequence(const Json& d,const std::string& id){for(const auto& s:d.value("sequences",Json::array()))if(s.at("id")==id)return s;throw std::runtime_error("Unknown presentation sequence: "+id);}
inline int direction_duration(const Json& s){int n=0;for(const auto& stage:s.at("stages"))n+=stage.at("durationMs").get<int>();return n;}
inline Json direction_start(const Json& d,const std::string& id){direction_sequence(d,id);return {{"sequenceId",id},{"elapsedMs",0},{"speed",1},{"paused",false},{"status","playing"}};}
inline bool direction_valid(const Json& d,const Json& c){try{
 if(!c.is_object()||c.size()!=5||!c.at("sequenceId").is_string()||!c.at("elapsedMs").is_number_integer()||!c.at("speed").is_number_integer()||!c.at("paused").is_boolean()||!c.at("status").is_string())return false;
 const int duration=direction_duration(direction_sequence(d,c.at("sequenceId"))),ms=c.at("elapsedMs"),speed=c.at("speed");const std::string status=c.at("status");const bool paused=c.at("paused");
 return (speed==1||speed==4)&&ms>=0&&ms<=duration&&(status=="playing"||status=="stopped"||status=="completed")&&(status=="completed"?ms==duration&&!paused:ms<duration)&&(status=="playing"||!paused);
 }catch(...){return false;}}
inline Json direction_advance(const Json& d,Json c,int delta){if(delta<0||delta>60000)throw std::runtime_error("Invalid presentation delta");if(c.at("paused")==true||c.at("status")!="playing")return c;const int duration=direction_duration(direction_sequence(d,c.at("sequenceId"))),ms=std::min(duration,c.at("elapsedMs").get<int>()+delta*c.at("speed").get<int>());c["elapsedMs"]=ms;if(ms==duration){c["status"]="completed";c["paused"]=false;}return c;}
inline Json direction_frame(const Json& d,const Json& c){
 const auto sequence=direction_sequence(d,c.at("sequenceId"));int start=0,index=0;Json actors=Json::array();
 for(const auto& stage:sequence.at("stages")){const int duration=stage.at("durationMs"),local=std::clamp(c.at("elapsedMs").get<int>()-start,0,duration);const double t=double(local)/duration;
  for(const auto& cue:stage.at("cues")){Json actor;for(const auto& a:d.at("actors"))if(a.at("id")==cue.at("actorId")){actor=a;break;}Json assets=Json::array({actor.at("assetId")});
   for(const auto& m:actor.value("motions",Json::array()))if(m.at("id")==cue.value("motionId","")){assets[0]=m.at("frames").at((local/m.at("frameMs").get<int>())%m.at("frames").size());break;}
   for(const auto& group:std::vector<std::pair<std::string,std::string>>{{"outfits","outfitId"},{"expressions","expressionId"}})for(const auto& v:actor.value(group.first,Json::array()))if(v.at("id")==cue.value(group.second,""))assets.push_back(v.at("assetId"));
   const double x=cue.at("x"),y=cue.at("y");Json frame={{"actorId",cue.at("actorId")},{"x",x+(cue.value("toX",x)-x)*t},{"y",y+(cue.value("toY",y)-y)*t},{"width",cue.at("width")},{"height",cue.at("height")},{"opacity",cue.value("opacity",1.0)},{"assets",assets}};
   bool found=false;for(auto& a:actors)if(a.at("actorId")==cue.at("actorId")){a=frame;found=true;break;}if(!found)actors.push_back(frame);
  }
  if(c.at("elapsedMs").get<int>()<start+duration||index==int(sequence.at("stages").size())-1)return {{"name",sequence.at("name")},{"stage",stage.at("name")},{"index",index},{"actors",actors}};start+=duration;++index;
 }throw std::runtime_error("Empty presentation sequence");
}
