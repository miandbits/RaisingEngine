#pragma once
#include "../vendor/json.hpp"
#include <map>
#include <vector>

/** Same immutable, target-complete projection as packages/visual/growth-summary.ts. */
inline nlohmann::json activity_growth_rows(const nlohmann::json& result) {
  using J=nlohmann::json;J rows=J::array();std::map<std::string,size_t> indices;
  for(const auto& change:result.value("changes",J::array())) {
    if(change.value("kind","")!="numeric")continue;
    const auto& target=change.at("target");const auto key=target.dump();
    auto found=indices.find(key);
    if(found==indices.end()) {indices[key]=rows.size();rows.push_back({{"key",key},{"target",target},{"before",change.at("before")},{"after",change.at("after")}});}
    else rows.at(found->second)["after"]=change.at("after");
  }
  J net=J::array();for(auto row:rows){const int64_t delta=row.at("after").get<int64_t>()-row.at("before").get<int64_t>();if(delta){row["delta"]=delta;net.push_back(row);}}return net;
}
