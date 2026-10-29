#pragma once
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

inline std::string timeline_target_label(const Json& content,const Json& target) {
  const auto kind=target.value("kind","");
  if(kind=="attribute"){const auto id=target.value("attributeId","");for(const auto& a:content.at("attributes"))if(a.at("id")==id)return a.value("label",id);return id;}
  if(kind=="inventory"){const auto id=target.value("itemId","");for(const auto& a:content.at("project").value("gameplay",Json::object()).value("items",Json::array()))if(a.at("id")==id)return a.value("name",id);return id;}
  const auto cid=target.value("characterId",""),rid=target.value("relationshipId","");std::string character=cid,relationship=rid;
  for(const auto& c:content.at("characters"))if(c.at("id")==cid){character=c.value("name",cid);for(const auto& r:c.at("relationships"))if(r.at("id")==rid)relationship=r.value("label",rid);}
  return character+" / "+relationship;
}
inline std::string timeline_changes_text(const Json& content,const Json& changes) {
  std::vector<std::pair<std::string,Json>> values;
  for(const auto& change:changes) {
    std::string key,label;Json value;
    if(change.value("kind","")=="numeric") {key=change.at("target").dump();label=timeline_target_label(content,change.at("target"));value={{"label",label},{"delta",change.at("delta")}};}
    else if(change.value("kind","")=="state") {key="state:"+change.value("characterId","")+":"+change.value("stateGroupId","");label=change.value("stateGroupId","");std::string name=change.value("after","");for(const auto &g:content.at("project").value("stateGroups",Json::array()))if(g.at("id")==change.at("stateGroupId")){label=g.value("name",label);for(const auto &v:g.at("values"))if(v.at("id")==change.at("after"))name=v.value("name",name);}value={{"label",label},{"state",name}};}
    else {key="flag:"+change.value("flagId","");label=change.value("flagId","");for(const auto& flag:content.at("flags"))if(flag.at("id")==change.at("flagId"))label=flag.value("label",label);value={{"label",label},{"flag",change.at("after")}};}
    bool found=false;for(auto& pair:values)if(pair.first==key){if(value.contains("delta"))value["delta"]=pair.second.value("delta",0)+value.at("delta").get<int>();pair.second=value;found=true;break;}if(!found)values.emplace_back(key,value);
  }
  std::string result;for(const auto& pair:values){const auto& value=pair.second;std::string text;
    if(value.contains("delta")){const int delta=value.at("delta");if(!delta)continue;text=value.value("label","")+" "+(delta>=0?"+":"")+std::to_string(delta);}
    else text=value.value("label","")+": "+(value.contains("state")?value.value("state",""):(value.value("flag",false)?"true":"false"));
    if(!result.empty())result+=", ";result+=text;
  }return result;
}
inline std::string timeline_tokens(const std::string& source,const std::map<std::string,std::string>& tokens) {
  std::string result;size_t position=0;
  while(position<source.size()){const auto start=source.find('{',position);if(start==std::string::npos){result+=source.substr(position);break;}result+=source.substr(position,start-position);const auto end=source.find('}',start);if(end==std::string::npos){result+=source.substr(start);break;}const auto found=tokens.find(source.substr(start+1,end-start-1));result+=found==tokens.end()?source.substr(start,end-start+1):found->second;position=end+1;}return result;
}
inline std::string timeline_word(const std::string& locale,const std::string& ko,const std::string& en,const std::string& ja) {return locale.rfind("en",0)==0?en:locale.rfind("ja",0)==0?ja:ko;}
inline Json plan_result_playback(const Json& content,const Json& state,const Json& config) {
  const auto& result=state.at("lastActivityResult");Json rows=Json::array(),clips=Json::array();
  for(const auto& change:result.at("changes"))if(change.value("kind","")=="numeric"&&change.value("delta",0)!=0){
    const auto& target=change.at("target");std::string label=timeline_target_label(content,target);
    if(target.contains("actorId")){std::string actor=target.at("actorId");for(const auto& c:content.at("characters"))if(c.at("id")==target.at("actorId"))actor=c.value("name",actor);label=actor+" / "+label;}
    rows.push_back({{"label",label},{"before",change.at("before")},{"after",change.at("after")}});
  }
  int64_t total=0;for(size_t i=0;i<rows.size()+2;i++){const int duration=config.at(i==0?"leadInMs":i==rows.size()+1?"holdMs":"changeMs");total+=duration;const auto id="result-"+std::to_string(i);clips.push_back({{"id",id},{"clipId",id},{"durationMs",duration},{"text",""},{"transition","none"},{"day",nullptr}});}
  std::string title=result.at("activityId");for(const auto& a:content.at("activities"))if(a.at("id")==result.at("activityId"))title=a.value("label",title);

  return {{"resultRevision",state.at("stateRevision")},{"activityId",result.at("activityId")},{"timelineId","inline-result"},{"clips",clips},{"totalDurationMs",total},{"resultPlayback",{{"title",title},{"outcome",result.at("success").is_null()?"":result.at("success").value("passed",false)?"success":"failure"},{"rows",rows}}}};
}
inline std::string result_playback_text(const Json& plan,const Json& cursor,bool changes_only=false,const std::string& locale="ko") {
  if(!plan.contains("resultPlayback"))return "";
  const auto& playback=plan.at("resultPlayback");std::string title=playback.at("title");const auto outcome=playback.value("outcome","");if(!outcome.empty())title+=" · "+timeline_word(locale,outcome=="success"?"성공":"실패",outcome=="success"?"Success":"Failure",outcome=="success"?"成功":"失敗");std::string text=changes_only?"":title;const auto& rows=playback.at("rows");
  for(size_t i=0;i<rows.size();i++){
    const auto& row=rows.at(i);const int index=cursor.at("clipIndex");const double progress=cursor.value("completed",false)||index>int(i+1)?1:index==int(i+1)?std::min(1.0,cursor.at("elapsedMs").get<double>()/plan.at("clips").at(i+1).at("durationMs").get<double>()):0;
    const int64_t before=row.at("before"),after=row.at("after"),delta=static_cast<int64_t>((after-before)*progress);
    if(changes_only){
      if(delta==0)continue;
      if(!text.empty())text+="\n";
      text+=row.at("label").get<std::string>()+" "+(delta>0?"+":"−")+std::to_string(delta>0?delta:-delta)+timeline_word(locale,"   현재 ","   Now ","   現在 ")+std::to_string(before+delta);
    }else text+="\n"+row.at("label").get<std::string>()+"  "+std::to_string(before)+" → "+std::to_string(before+delta)+"  ("+(delta>=0?"+":"")+std::to_string(delta)+")";
  }
  if(rows.empty()&&(!changes_only||cursor.value("completed",false)))text+=timeline_word(locale,"능력 변화 없음","No stat changes","能力値の変化なし");return text;
}
inline Json plan_activity_timeline(const Json& content,const Json& state,const std::string& locale="ko") {
  if(state.is_null()||state.value("phase","")!="activity-result"||state.value("lastActivityResult",Json()).is_null())return nullptr;
  const auto result=state.at("lastActivityResult"),visual=content.at("project").value("visual",Json::object());Json binding=nullptr;
  for(const auto& candidate:visual.value("activityTimelines",Json::array()))if(candidate.at("activityId")==result.at("activityId")){binding=candidate;break;}if(binding.is_null())return visual.contains("resultPlayback")?plan_result_playback(content,state,visual.at("resultPlayback")):Json();
  auto timelineId=binding.at("timelineId");const auto success=result.value("success",Json());if(!success.is_null())timelineId=binding.value(success.value("passed",false)?"successTimelineId":"failureTimelineId",timelineId);
  Json timeline=nullptr;for(const auto& candidate:visual.value("timelines",Json::array()))if(candidate.at("id")==timelineId){timeline=candidate;break;}if(timeline.is_null()||timeline.at("clips").empty())return nullptr;
  std::vector<int> days={0};if(binding.value("repeat","once")=="logicalDays") {
    const int count=content.at("project").value("gameplay",Json::object()).value("ruleCalendar",Json::object()).value("daysPerMonth",0);if(count<3||count>366)return nullptr;
    days.clear();const int slot=result.at("time").value("periodIndex",0)%content.at("project").value("periodsPerMonth",3);for(int day=(content.at("project").value("periodsPerMonth",3)==3?slot*count/3:slot*(count/content.at("project").value("periodsPerMonth",3)));day<(content.at("project").value("periodsPerMonth",3)==3?(slot+1)*count/3:(slot==content.at("project").value("periodsPerMonth",3)-1?count:(slot+1)*(count/content.at("project").value("periodsPerMonth",3))));day++)days.push_back(day+1);
  }
  std::string activity=result.at("activityId");for(const auto& a:content.at("activities"))if(a.at("id")==result.at("activityId"))activity=a.value("label",activity);
  const auto summary=timeline_changes_text(content,result.at("changes"));Json clips=Json::array();int64_t duration=0;
  for(size_t index=0;index<days.size();index++)for(const auto& clip:timeline.at("clips")) {
    const int day=days[index];const std::map<std::string,std::string> tokens={{"activity",activity},{"outcome",success.is_null()?timeline_word(locale,"완료","complete","完了"):success.value("passed",false)?timeline_word(locale,"성공","Success","成功"):timeline_word(locale,"실패","Failure","失敗")},{"day",day?std::to_string(day):""},{"changes",index==0?summary:""}};
    Json item={{"id",(day?"day-"+std::to_string(day):"once")+"__"+clip.value("id","")},{"clipId",clip.at("id")},{"durationMs",clip.at("durationMs")},{"text",timeline_tokens(clip.value("text",""),tokens)},{"transition",clip.value("transition","none")},{"day",day?Json(day):Json()}};
    if(clip.contains("imageAssetId"))item["imageAssetId"]=clip.at("imageAssetId");
    if(clip.contains("stage"))item["stage"]=clip.at("stage");
    duration+=clip.at("durationMs").get<int>();clips.push_back(item);
  }
  return {{"resultRevision",state.at("stateRevision")},{"activityId",result.at("activityId")},{"timelineId",timelineId},{"clips",clips},{"totalDurationMs",duration}};
}
// Presentation time only: frame selection never dispatches a game command or
// consumes RNG. Durations are validated positive integers by the content schema.
inline size_t timeline_layer_frame_index(const Json& layer,double elapsed) {
  const auto& frames=layer.at("frames");
  int64_t total=0;for(const auto& frame:frames)total+=frame.at("durationMs").get<int>();
  if(frames.empty()||total<=0)throw std::runtime_error("Invalid timeline layer frames");
  double time=std::max(0.0,elapsed);
  if(layer.value("loop",false))time=std::fmod(time,double(total));
  else if(time>=double(total))return frames.size()-1;
  int64_t end=0;for(size_t i=0;i<frames.size();i++){end+=frames.at(i).at("durationMs").get<int>();if(time<double(end))return i;}
  return frames.size()-1;
}
inline double timeline_clip_elapsed(const Json& clip,const Json& cursor) {
  return cursor.value("completed",false)?clip.at("durationMs").get<double>():cursor.at("elapsedMs").get<double>();
}
inline Json create_timeline_cursor(const Json& plan) {return {{"resultRevision",plan.at("resultRevision")},{"activityId",plan.at("activityId")},{"timelineId",plan.at("timelineId")},{"clipIndex",0},{"elapsedMs",0},{"paused",false},{"speed",1},{"completed",false}};}
inline bool validate_timeline_cursor(const Json& plan,const Json& cursor) {
  if(plan.is_null()||!cursor.is_object()||cursor.size()!=8)return false;
  for(const auto* key:{"resultRevision","activityId","timelineId","clipIndex","elapsedMs","paused","speed","completed"})if(!cursor.contains(key))return false;
  if(cursor.at("resultRevision")!=plan.at("resultRevision")||cursor.at("activityId")!=plan.at("activityId")||cursor.at("timelineId")!=plan.at("timelineId")||!cursor.at("clipIndex").is_number_integer()||!cursor.at("elapsedMs").is_number()||!cursor.at("paused").is_boolean()||!cursor.at("completed").is_boolean()||!(cursor.at("speed")==1||cursor.at("speed")==4))return false;
  const double elapsed=cursor.at("elapsedMs");const int64_t index=cursor.at("clipIndex");if(!std::isfinite(elapsed)||elapsed<0)return false;
  return cursor.value("completed",false)?index==int(plan.at("clips").size())&&elapsed==0&&!cursor.value("paused",false):index>=0&&index<int(plan.at("clips").size())&&elapsed<plan.at("clips").at(index).value("durationMs",0);
}
inline Json skip_timeline(const Json& plan,Json cursor) {if(!validate_timeline_cursor(plan,cursor))throw std::runtime_error("Invalid timeline cursor");cursor["clipIndex"]=plan.at("clips").size();cursor["elapsedMs"]=0;cursor["paused"]=false;cursor["completed"]=true;return cursor;}
inline Json advance_timeline(const Json& plan,Json cursor,double delta) {
  if(!validate_timeline_cursor(plan,cursor))throw std::runtime_error("Invalid timeline cursor");if(!std::isfinite(delta)||delta<0)throw std::runtime_error("Timeline delta must be finite and nonnegative");
  if(cursor.value("paused",false)||cursor.value("completed",false)||delta==0)return cursor;
  double elapsed=cursor.at("elapsedMs").get<double>()+delta*cursor.at("speed").get<int>();int index=cursor.at("clipIndex");
  while(index<int(plan.at("clips").size())&&elapsed>=plan.at("clips").at(index).value("durationMs",0)){elapsed-=plan.at("clips").at(index).value("durationMs",0);++index;}
  if(index==int(plan.at("clips").size()))return skip_timeline(plan,cursor);cursor["clipIndex"]=index;cursor["elapsedMs"]=elapsed;return cursor;
}
