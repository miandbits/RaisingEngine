#pragma once
#include "json.hpp"
#include <algorithm>
#include <cstdint>
#include <deque>
#include <string>
#include <utility>

namespace raising_player {
// Memory-only display receipts. Nothing in this type is a canonical save field.
struct ReadingHistoryPrefix { std::string text; std::size_t bytes=0; bool truncated=false; };
inline std::pair<std::uint32_t,std::size_t> history_scalar(const std::string& text,std::size_t at) {
  const auto first=static_cast<unsigned char>(text[at]);
  if(first<0x80)return {first,1};
  const std::size_t count=first>=0xc2&&first<=0xdf?2:first>=0xe0&&first<=0xef?3:first>=0xf0&&first<=0xf4?4:0;
  if(!count||at+count>text.size())return {0xfffd,1};
  std::uint32_t point=first&((1u<<(7-count))-1);
  for(std::size_t n=1;n<count;++n){const auto c=static_cast<unsigned char>(text[at+n]);if((c&0xc0)!=0x80)return {0xfffd,1};point=(point<<6)|(c&0x3f);}
  if((count==2&&point<0x80)||(count==3&&point<0x800)||(count==4&&point<0x10000)||point>0x10ffff||(point>=0xd800&&point<=0xdfff))return {0xfffd,1};
  return {point,count};
}
inline ReadingHistoryPrefix reading_history_prefix(const std::string& value,std::size_t budget) {
  ReadingHistoryPrefix result;std::size_t at=0;
  while(at<value.size()){
    const auto [point,used]=history_scalar(value,at);const bool replacement=point==0xfffd&&used==1;
    const std::size_t bytes=replacement?3:used;if(result.bytes+bytes>budget)break;
    if(replacement)result.text+="\xef\xbf\xbd";else result.text.append(value,at,used);
    result.bytes+=bytes;at+=used;
  }
  result.truncated=at<value.size();return result;
}
inline bool history_has_text(const std::string& text) {
  for(std::size_t at=0;at<text.size();){const auto [cp,n]=history_scalar(text,at);at+=n;
    if(!((cp>=9&&cp<=13)||cp==32||cp==0xa0||cp==0x1680||(cp>=0x2000&&cp<=0x200a)||cp==0x2028||cp==0x2029||cp==0x202f||cp==0x205f||cp==0x3000||cp==0xfeff))return true;
  }return false;
}
class ReadingHistory {
  struct Entry {
    std::uint64_t sequence=0;std::string key,kind,text,locale,speaker,date;bool truncated=false;std::size_t bytes=0;
  };
  std::deque<Entry> entries_;std::size_t bytes_=0;std::uint64_t omitted_=0,sequence_=0;
  std::string epoch_,reason_="new-session";
public:
  static constexpr std::size_t max_entries=200,total_bytes=256*1024,entry_bytes=16*1024,metadata_bytes=1024;
  explicit ReadingHistory(std::string epoch):epoch_(std::move(epoch)){}
  const std::string& epoch() const {return epoch_;}
  void reset(std::string epoch,std::string reason="new-run") {epoch_=std::move(epoch);reason_=std::move(reason);entries_.clear();bytes_=0;omitted_=sequence_=0;}
  nlohmann::json snapshot() const {
    auto entries=nlohmann::json::array();for(const auto& e:entries_)entries.push_back({{"sequence",e.sequence},{"kind",e.kind},{"text",e.text},{"locale",e.locale},{"speaker",e.speaker},{"date",e.date},{"truncated",e.truncated}});
    return {{"epoch",epoch_},{"reason",reason_},{"entries",entries},{"omittedEntries",omitted_},{"utf8Bytes",bytes_}};
  }
  bool record(const nlohmann::json& receipt) {
    if(!receipt.is_object()||!receipt.contains("epoch")||receipt.at("epoch")!=epoch_||!receipt.contains("key")||!receipt.at("key").is_string()||!receipt.contains("kind")||!receipt.at("kind").is_string()||!receipt.contains("text")||!receipt.at("text").is_string())return false;
    const std::string kind=receipt.at("kind");if(kind!="dialogue"&&kind!="prompt"&&kind!="chosen-response")return false;
    const auto key=reading_history_prefix(receipt.at("key").get_ref<const std::string&>(),metadata_bytes);if(key.text.empty()||key.truncated)return false;
    const auto field=[&](const char* name,std::size_t budget){const auto found=receipt.find(name);return found!=receipt.end()&&found->is_string()?reading_history_prefix(found->get_ref<const std::string&>(),budget):ReadingHistoryPrefix{};};
    const auto locale=field("locale",128),speaker=field("speaker",metadata_bytes),date=field("date",metadata_bytes);
    const std::size_t overhead=key.bytes+locale.bytes+speaker.bytes+date.bytes;const auto text=reading_history_prefix(receipt.at("text").get_ref<const std::string&>(),entry_bytes-overhead);
    if(!history_has_text(text.text))return false;
    Entry e{0,key.text,kind,text.text,locale.text,speaker.text,date.text,text.truncated||locale.truncated||speaker.truncated||date.truncated,overhead+text.bytes};
    const bool same=!entries_.empty()&&entries_.back().key==e.key&&entries_.back().kind==e.kind&&entries_.back().locale==e.locale&&entries_.back().speaker==e.speaker&&entries_.back().date==e.date;
    if(same&&entries_.back().text==e.text&&entries_.back().truncated==e.truncated)return false;
    if(same&&kind!="chosen-response"&&entries_.back().text.size()>e.text.size()&&entries_.back().text.compare(0,e.text.size(),e.text)==0)return false;
    if(same&&kind!="chosen-response"&&e.text.compare(0,entries_.back().text.size(),entries_.back().text)==0){e.sequence=entries_.back().sequence;bytes_-=entries_.back().bytes;entries_.back()=e;}
    else {e.sequence=++sequence_;entries_.push_back(e);}
    bytes_+=e.bytes;
    while(entries_.size()>max_entries||bytes_>total_bytes){bytes_-=entries_.front().bytes;entries_.pop_front();omitted_=std::min<std::uint64_t>(9007199254740991ULL,omitted_+1);}
    return true;
  }
};
}
