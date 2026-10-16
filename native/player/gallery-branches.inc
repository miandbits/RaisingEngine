  Json gallery_branch_rows() const {
    Json rows=Json::array(),entry=nullptr,narrative=nullptr;
    for(const auto& e:display_content.at("project").value("gallery",Json::object()).value("entries",Json::array()))if(e.value("id","")==gallery_entry)entry=e;
    if(entry.is_null()||(entry.value("kind","")!="event"&&entry.value("kind","")!="ending"))return rows;
    const auto records=game.gallery.records(entry);if(records.empty())return rows;
    for(const auto& n:display_content.at(entry.at("kind")=="event"?"events":"endings"))if(n.at("id")==entry.at("sourceId"))narrative=n;
    if(narrative.is_null())return rows;
    std::map<std::string,Json> nodes;for(const auto& n:narrative.at("nodes"))nodes[n.at("id")]=n;
    std::set<std::string> reachable;std::vector<std::string> queue={narrative.at("entryNodeId")};
    for(size_t i=0;i<queue.size();++i){auto id=queue[i];if(!reachable.insert(id).second||!nodes.count(id))continue;const auto& n=nodes.at(id);
      if(n.at("type")=="choice")for(const auto& c:n.at("choices"))queue.push_back(c.at("next"));
      else if(n.at("type")=="branch"){queue.push_back(n.at("then"));queue.push_back(n.at("else"));}
      else if(n.contains("next"))queue.push_back(n.at("next"));
    }
    for(const auto& n:narrative.at("nodes"))if(n.at("type")=="choice"&&reachable.count(n.at("id"))){
      std::string prompt="아직 보지 못한 선택 지점";bool visited=false;
      for(const auto& r:records)for(const auto& f:r.at("frames"))if(!visited&&f.value("nodeId",Json())==n.at("id")){prompt=f.value("text","");visited=true;}
      for(const auto& c:n.at("choices")){
        int count=0,index=-1;bool selected=false;
        for(size_t i=0;i<records.size();++i)for(const auto& f:records[i].at("frames"))if(f.value("nodeId",Json())==n.at("id")&&f.value("choiceId",Json())==c.at("id")){
          ++count;selected=selected||i==gallery_record;if(index<0&&!records[i].value("replayFrames",Json::array()).empty())index=int(i);break;
        }
        const auto node=n.at("id").get<std::string>(),id="n"+std::to_string(node.size())+"-"+node+"-"+c.at("id").get<std::string>();
        Json values={{"id",id},{"entryId",gallery_entry},{"branchId",id},{"name",count?c.value("text",""):"미경험 선택지"},{"prompt",prompt},
          {"statusText",count?"경험한 분기 · 기록 "+std::to_string(count)+"개":"잠금 · 본편에서 이 선택을 경험하세요"},
          {"unlocked",count>0},{"locked",count==0},{"canReplay",index>=0},{"recordCount",count},{"recordIndex",index},{"selected",selected},
          {"action","galleryBranch"},{"enabled",index>=0},{"reason",count?"원본 연출 기록이 필요합니다":"아직 경험하지 않은 선택 분기입니다"}};
        rows.push_back({{"id",id},{"values",values},{"enabled",true},{"reason",""}});
      }
    }
    return rows;
  }
