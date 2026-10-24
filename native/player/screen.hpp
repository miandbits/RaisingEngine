#pragma once
#include "session.hpp"
#include "styles.hpp"
#include "localization.hpp"
#include "layout.hpp"
#include "extension-host.hpp"
#include "presentation-variants.hpp"
#include "preferences.hpp"
#include <algorithm>
#include <map>
#include <iomanip>

inline std::string escape(const std::string& value) {
  std::string out; for (char c : value) { switch(c) { case '&': out += "&amp;"; break; case '<': out += "&lt;"; break; case '>': out += "&gt;"; break; case '\"': out += "&quot;"; break; case '\n': out += "<br/>"; break; default: out += c; } } return out;
}
inline std::string number(double n) { std::ostringstream out; out << n; return out.str(); }
inline std::string calendar_short_label(const std::string& value) {
  std::size_t offset=0;int count=0;
  while(offset<value.size()&&count<4){++offset;while(offset<value.size()&&(static_cast<unsigned char>(value[offset])&0xc0)==0x80)++offset;++count;}
  return value.substr(0,offset)+(offset<value.size()?"…":"");
}
inline std::string player_reason(const std::string& value) {
  // Keep diagnostic codes in the session/log; show the explanatory text in game UI.
  if (value.empty() || value.front() != '[') return value;
  const auto end = value.find("] ");
  if (end == std::string::npos || end < 2 || end + 2 >= value.size()) return value;
  bool separator = false;
  for (std::size_t i = 1; i < end; ++i) {
    const char c = value[i];
    if (c == '_') separator = true;
    else if ((c < 'A' || c > 'Z') && (c < '0' || c > '9')) return value;
  }
  return separator ? value.substr(end + 2) : value;
}
inline std::string css_color(std::string hex) {
  if (hex.size() != 7 && hex.size() != 9) return "transparent";
  try { int r=std::stoi(hex.substr(1,2),nullptr,16),g=std::stoi(hex.substr(3,2),nullptr,16),b=std::stoi(hex.substr(5,2),nullptr,16);
    int a=hex.size()==9 ? std::stoi(hex.substr(7,2),nullptr,16) : 255;
    return "rgba("+std::to_string(r)+","+std::to_string(g)+","+std::to_string(b)+","+number(a)+")";
  } catch (...) { return "transparent"; }
}

class ScreenMarkup {
public:
  unsigned authored_timeline_controls=0;
  GameSession& game;
  bool replay_controls=false,replay_autoplay=false;
  int replay_speed=1;
  size_t replay_cursor=0,replay_count=0;
  std::string replay_notice,replay_title;
  Json assets,display_content,display_projection,render_elements=Json::array();
  Json user_values_draft=Json::object();
  #include "save-slots-markup.inc"
  #include "settings-markup.inc"
  Json preview_scene=nullptr;
  Json display_scene() const {
    auto scene=game.state.is_null()?display_content.at("project").at("visual").value("defaultScene",Json::object()):game.state.at("presentation");
    if(!preview_screen.empty()&&preview_scene.is_object())for(const auto& [key,value]:preview_scene.items())scene[key]=value;
    return scene;
  }
  std::string activity_preview(const Json& activity) const {
    std::string details;
    for(const auto& entry:activity.value("previewAttributes",Json::array())) {
      const auto id=entry.at("attributeId").get<std::string>();
      for(const auto& attribute:display_content.at("attributes"))if(attribute.at("id")==id) {
        int value=attribute.value("initial",0);
        const auto initial=display_content.at("project").value("initialState",Json::object()).value("attributes",Json::object());
        if(initial.contains(id))value=initial.at(id).get<int>();
        if(!game.state.is_null()&&game.state.at("attributes").contains(id))value=game.state.at("attributes").at(id).get<int>();
        auto formatted=entry.value("valueFormat",std::string("{value}"));
        const auto number=std::to_string(value);std::size_t offset=0;
        while((offset=formatted.find("{value}",offset))!=std::string::npos){formatted.replace(offset,7,number);offset+=number.size();}
        if(!details.empty())details+=" · ";details+=entry.value("label",attribute.value("label",id))+" "+formatted;
        break;
      }
    }
    return details.empty()?details:"현재 "+details;
  }
  std::string user_field_value(const std::string& id) const {
    if(!game.state.is_null())return game.state.value("userValues",Json::object()).value(id,"");
    if(user_values_draft.contains(id))return user_values_draft.at(id).get<std::string>();
    for(const auto& field:game.content.at("project").value("userFields",Json::array()))if(field.at("id")==id)return field.value("initial","");
    return "";
  }
  std::string locale;
  void refreshDisplay(){
    const auto config=game.content.at("project").value("localization",Json::object());bool valid=false;for(const auto& l:config.value("locales",Json::array()))if(l.at("id")==locale)valid=true;
    if(!valid)locale=config.value("defaultLocale","");
    display_content=localized_content(game.content,locale);
    display_projection=Json::parse(raising::process(Json({{"operation","view"},{"content",display_content},{"state",game.state}}).dump()));
    if(!display_projection.value("ok",false))throw std::runtime_error("Localized view failed");
  }
  void nextLocale(){const auto config=game.content.at("project").value("localization",Json::object());const auto list=config.value("locales",Json::array());for(size_t i=0;i<list.size();i++)if(list[i].at("id")==locale){locale=list[(i+1)%list.size()].at("id");return;}}
  fs::path bundle;
  std::vector<std::string> plan;
  Json roster_draft=Json::object();
  int roster_revision=-1;
  int plan_slot=0;
  std::string plan_group;
  int plan_selected=0,plan_month=-1;
  mutable std::string forecast_key;
  mutable Json forecast_cache=nullptr;
  Json pending_purchase=nullptr;
  Json pending_confirmation=Json::object();
  Json ui_selections=Json::object();std::string ui_modal;
  // Supplied by the host from its configured save file. Never inferred from the
  // live state: a designer's save screen must distinguish current and saved play.
  Json saved_preview=Json::object();
  Json selected_values(const std::string& key) const {
    if(!ui_selections.contains(key))return Json();const auto selected=ui_selections.at(key);
    for(const auto& row:collection_rows(selected.at("source")))if(row.at("id")==selected.at("rowId")&&row.value("enabled",true))return row.at("values");return Json();
  }
  Json selection_action(Json action) const {
    if(!action.contains("_selectionKey"))return action;const auto values=selected_values(action.at("_selectionKey"));if(values.is_null())throw std::runtime_error("대상을 다시 선택하세요.");
    for(const auto& b:action.at("_selectionBindings")){const auto field=b.value("field","");if(!values.contains(field))throw std::runtime_error("선택 필드가 없습니다.");action[b.at("property").get<std::string>()]=values.at(field);}action.erase("_selectionKey");action.erase("_selectionBindings");return action;
  }
  int pending_revision=-1;
  std::map<std::string,int> quantities;
  std::map<std::string,Json> actions;
  std::map<std::string,std::string> labels;
  int width=1280, height=720;
  int viewport_width=0,viewport_height=0;
  std::string screen_id,preview_screen;
  ExtensionHost* extensions=nullptr;
  explicit ScreenMarkup(GameSession& game, Json assets, fs::path bundle):game(game),assets(std::move(assets)),bundle(std::move(bundle)){}
  std::string asset(const Json& id) const {
    if (!id.is_string() || !assets.contains(id.get<std::string>())) return "";
    const auto relative = fs::u8path(assets.at(id.get<std::string>()).get<std::string>());
    if (relative.is_absolute()) return "";
    const auto root = fs::weakly_canonical(bundle), candidate = fs::weakly_canonical(root / relative);
    auto rel = candidate.lexically_relative(root);
    if (rel.empty() || *rel.begin() == ".." || !fs::is_regular_file(candidate)) return "";
    return candidate.generic_u8string();
  }
  Json gameplay() const { return display_projection.value("gameplay", Json()); }
  Json transaction_definition(const Json& action) const {
    const auto config=display_content.at("project").value("gameplay",Json::object());
    const auto type=action.value("type","");
    const auto gp=gameplay();
    if(!gp.is_null()&&type=="executeAction")for(const auto& row:gp.value("actions",Json::array()))if(row.at("id")==action.value("actionId",Json()))return row;
    if(!gp.is_null()&&type=="selectPolicy")for(const auto& group:gp.value("policyGroups",Json::array()))if(group.at("id")==action.value("groupId",Json())) {
      for(auto row:group.at("options"))if(row.at("id")==action.value("optionId",Json())){row["trigger"]=group.at("trigger");return row;}
      return {{"name",group.at("name")}};
    }
    if(type=="setDiet"||type=="useService")for(const auto& row:config.value(type=="setDiet"?"diets":"services",Json::array()))
      if(row.at("id")==action.value(type=="setDiet"?"dietId":"serviceId",Json()))return row;
    if(type=="buy"||type=="sell")for(const auto& shop:config.value("shops",Json::array()))if(shop.at("id")==action.value("shopId",Json()))
      for(auto row:shop.value(type=="buy"?"offers":"sellOffers",Json::array()))if(row.at("itemId")==action.value("itemId",Json())) {
        row["currencyAttributeId"]=shop.at("currencyAttributeId");
        for(const auto& item:config.value("items",Json::array()))if(item.at("id")==row.at("itemId"))row["name"]=item.at("name");
        return row;
      }
    return Json::object();
  }
  std::string attribute_name(const std::string& id) const {
    for(const auto& a:display_content.at("attributes"))if(a.at("id")==id)return a.value("label",id);
    return id;
  }
  std::string target_name(const Json& target) const {
    if(target.value("kind","")=="attribute")return attribute_name(target.value("attributeId",""));
    if(target.value("kind","")=="inventory") {
      const auto id=target.value("itemId","");
      for(const auto& item:display_content.at("project").value("gameplay",Json::object()).value("items",Json::array()))
        if(item.value("id","")==id)return item.value("name",id);
      return id;
    }
    const auto relationship_id=target.value("relationshipId",""),character_id=target.value("characterId","");
    for(const auto& character:display_content.at("characters"))if(character.value("id","")==character_id) {
      std::string label=relationship_id;
      for(const auto& relationship:character.value("relationships",Json::array()))
        if(relationship.value("id","")==relationship_id){label=relationship.value("label",relationship_id);break;}
      return character.value("name",character_id)+" / "+label;
    }
    return relationship_id;
  }
#include "battle-data.inc"
#include "collection.hpp"
#include "timeline-screen.inc"
#include "direction-screen.inc"
  std::string cost_summary(const Json& row,int quantity=1) const {
    std::string out;
    if(row.contains("price"))return attribute_name(row.value("currencyAttributeId","money"))+" "+number(row.value("price",0.0)*quantity);
    for(const auto& cost:row.value("costs",Json::array())){if(!out.empty())out+=" · ";out+=attribute_name(cost.value("attributeId",""))+" "+number(cost.value("amount",0.0)*quantity);}
    return out.empty()?"무료":out;
  }
  std::string effect_summary(const Json& row,int quantity=1) const {
    std::string out;
    for(const auto& effect:row.value("effects",Json::array())) {
      const auto op=effect.value("op","");
      // Flags control content flow; their internal IDs are not player-facing effects.
      if(op=="setFlag")continue;
      if(op=="setState"){if(!out.empty())out+=" · ";std::string name=effect.value("stateGroupId",""),value=effect.value("stateValueId","");for(const auto &g:display_content.at("project").value("stateGroups",Json::array()))if(g.at("id")==effect.at("stateGroupId")){name=g.value("name",name);for(const auto &v:g.at("values"))if(v.at("id")==effect.at("stateValueId"))value=v.value("name",value);}out+=name+": "+value;continue;}
      if(!out.empty())out+=" · ";
      const auto target=effect.value("target",Json::object());out+=target_name(target);
      const auto value=effect.value("value",Json::object());
      if(value.value("op","")=="literal"){const auto amount=value.value("value",0.0)*(op=="add"?quantity:1);out+=(op=="set"?" = ":amount>=0?" +":" ")+(amount<0?"−"+number(-amount):number(amount));}else out+=" (현재 상태 기준 계산)";
    }
    return out;
  }
  bool progress_pending() const {return !game.state.is_null()&&game.state.contains("progression")&&!game.state.at("progression").at("pending").is_null();}
  std::string time_summary(const std::string &id,int quantity) const {
    if(!display_content.at("project").contains("progression"))return "";
    const auto &cfg=display_content.at("project").at("progression");for(const auto &rule:cfg.at("actions"))if(rule.at("actionId")==id)return " · "+std::to_string(rule.value("duration",0LL)*quantity)+" "+cfg.value("unitLabel","")+" / 행동력 "+std::to_string(rule.value("actionPointCost",0LL)*quantity);return "";
  }
  std::string transaction_summary(const Json& action) const {
    const auto row=transaction_definition(action);const auto type=action.value("type","");const int quantity=action.value("quantity",1);
    if(type=="setDiet"&&action.value("dietId",Json()).is_null())return "현재 식단을 해제합니다. 이후 기간에는 식단 비용과 효과가 적용되지 않습니다.";
    if(type=="selectPolicy"&&action.value("optionId",Json()).is_null())return row.value("name","")+" 선택을 해제합니다.";
    std::string out=row.value("name","");
    if(type=="setDiet")out+=" · "+std::to_string(row.value("intervalPeriods",1))+"기간마다 · "+cost_summary(row)+"\n선택 시에는 비용이 차감되지 않습니다. 활동 시작 시 적용됩니다.";
    else if(type=="selectPolicy") {const auto trigger=row.value("trigger",Json::object());out+=" · "+std::string(trigger.value("type","")=="afterActivity"?"활동 종료 후":"활동 시작 전")+" · "+std::to_string(trigger.value("intervalPeriods",1))+"기간마다 · "+cost_summary(row)+"\n선택 시에는 비용이 차감되지 않습니다.";}
    else out+=" × "+std::to_string(quantity)+" · "+(type=="sell"?"받을 금액 ":"비용 ")+cost_summary(row,quantity);
    const auto effects=effect_summary(row,quantity);if(!effects.empty())out+="\n효과: "+effects;
    // Show the core's actual, clamped changes, using a copy of state only.
    if(type!="setDiet"&&type!="selectPolicy"&&!game.state.is_null())try {
      const auto next=game.call({{"operation","command"},{"state",game.state},{"command",action},{"expectedRevision",game.state.at("stateRevision")}}).at("state");std::string changes;
      for(const auto& a:display_content.at("attributes")){const auto id=a.at("id").get<std::string>();const auto delta=next.at("attributes").value(id,0)-game.state.at("attributes").value(id,0);if(delta){if(!changes.empty())changes+=" · ";changes+=a.value("label",id)+" "+(delta>0?"+":"")+std::to_string(delta);}}
      if(!changes.empty())out+="\n실제 변화: "+changes;
    }catch(...){}
    return out;
  }
  std::string quantity_controls(const std::string& key,int maximum) {
    maximum=std::max(1,maximum);auto& quantity=quantities[key];quantity=std::clamp(quantity,1,maximum);
    if(maximum==1)return "";
    const auto action=[&](int delta){return Json{{"type","quantity"},{"key",key},{"delta",delta},{"maximum",maximum}};};
    return "<div class=\"quantity-controls\">"+button("less-"+key,"−",action(-1),quantity>1)+"<span>수량 "+std::to_string(quantity)+"</span>"+button("more-"+key,"+",action(1),quantity<maximum)+"</div>";
  }
  std::string facility_body(const Json& e) {
    const auto gp=gameplay();if(gp.is_null())return "";const auto kind=e.value("kind","");std::string out;
    const auto included=[&](const std::string& key,const Json& id){if(!e.contains(key)||e.at(key).empty())return true;for(const auto& value:e.at(key))if(value==id)return true;return false;};
    if(kind=="policies") {
      for(const auto& budget:gp.value("policyBudgets",Json::array()))out+="<p>"+escape(budget.value("name","")+": "+number(budget.value("used",0.0))+" / "+number(budget.value("capacity",0.0))+" "+budget.value("unit",""))+"</p>";
      for(const auto& group:gp.value("policyGroups",Json::array())) {
        if(!included("groupIds",group.at("id")))continue;const auto group_id=group.at("id").get<std::string>();const auto trigger=group.at("trigger");
        out+="<h3>"+escape(group.value("name",""))+"</h3><p>"+escape(std::string(trigger.value("type","")=="afterActivity"?"활동 종료 후":"활동 시작 전")+" · "+std::to_string(trigger.value("intervalPeriods",1))+"기간마다")+"</p>";
        for(const auto& option:group.at("options")) {
          const auto id=option.at("id").get<std::string>();const bool selected=option.value("selected",false);Json action={{"type","selectPolicy"},{"groupId",group_id},{"optionId",id}};std::string reason;const bool enabled=game.available(action,&reason)&&!selected;
          std::string allocations;for(const auto& use:option.value("budgetUses",Json::array())) {for(const auto& budget:gp.value("policyBudgets",Json::array()))if(budget.at("id")==use.at("budgetId"))allocations+=budget.value("name","")+" "+number(use.value("amount",0.0))+" "+budget.value("unit","")+" · ";}
          out+="<div class=\"facility-row\"><b>"+escape(option.value("name","")+(selected?" · 선택 중":""))+"</b><p>"+escape(cost_summary(option)+" · "+effect_summary(option)+"\n"+allocations)+"</p>"+button("policy__"+e.value("id","")+"__"+group_id+"__"+id,option.value("name","선택"),action,enabled,selected?"현재 선택":reason)+"</div>";
        }
        Json clear={{"type","selectPolicy"},{"groupId",group_id},{"optionId",nullptr}};std::string reason;const bool enabled=game.available(clear,&reason);out+=button("policy_clear__"+e.value("id","")+"__"+group_id,group.value("name","")+" 해제",clear,enabled,reason)+"<p>선택 시 비용 없이, 설정한 시점에 비용과 효과가 적용됩니다.</p>";
      }
    } else if(kind=="actions") {
      if(game.state.contains("progression")){
        const auto &p=game.state.at("progression");const auto &cfg=display_content.at("project").at("progression");const auto prefix="progress__"+e.value("id","")+"__";
        out+="<p>게임 시간 "+std::to_string(p.value("tick",0))+" "+escape(cfg.value("unitLabel",""))+" · 행동력 "+std::to_string(p.value("actionPoints",0))+" / "+std::to_string(cfg.at("actionPoints").value("max",0))+"</p>";
        if(p.contains("sessions")){
          out+="<p>현재 일정 칸 남은 시간: "+std::to_string((game.state.value("periodIndex",0)+1)*cfg.value("ticksPerPeriod",1)-p.value("tick",0))+" · 월 일정으로 이 칸을 마칩니다.</p>";
          for(const auto &session:p.at("sessions")){
            const auto id=session.value("id","");std::string name=id;if(session.value("kind","")=="calendar")name="월 일정";else if(session.value("kind","")=="wait")name="대기";else {for(const auto &c:display_content.at("characters"))if(c.at("id")==id)name=c.value("name",id);auto actionName=session.at("command").value("actionId","");for(const auto &a:display_content.at("project").at("gameplay").at("actions"))if(a.at("id")==actionName){actionName=a.value("name",actionName);break;}name+=" · "+actionName;if(p.at("actors").contains(id))name+=" · 행동력 "+std::to_string(p.at("actors").at(id).value("actionPoints",0));}
            out+="<p>"+escape(name)+" · 남은 시간 "+std::to_string(session.value("remaining",0))+"</p>";
            if(!session.at("command").is_null()&&session.at("command").contains("actorId")){Json cancel={{"type","cancelActorSession"},{"actorId",id}};out+=button(prefix+"cancel__"+id,"이 캐릭터 진행 취소",cancel,game.available(cancel));}
          }
          if(!p.at("sessions").empty()){Json advance={{"type","advanceSession"}},cancel={{"type","cancelSession"}};std::string reason;const bool available=game.available(advance,&reason);out+=button(prefix+"advance","다음 시점까지 진행",advance,available,reason)+button(prefix+"cancel","전체 진행 취소",cancel,game.available(cancel));}
          else {Json wait={{"type","waitTime"},{"ticks",1}};out+=button(prefix+"wait","1 "+cfg.value("unitLabel","")+" 대기",wait,game.available(wait));}
        }else if(!p.at("pending").is_null()){
          const auto &pending=p.at("pending");std::string current="대기";
          if(!pending.at("command").is_null()){const auto &cmd=pending.at("command");current=cmd.value("actionId","");for(const auto &a:display_content.at("project").at("gameplay").at("actions"))if(a.at("id")==cmd.at("actionId"))current=a.value("name",current);if(cmd.contains("actorId"))for(const auto &actor:display_content.at("characters"))if(actor.at("id")==cmd.at("actorId"))current=actor.value("name","")+" · "+current;}
          out+="<p>진행 중: "+escape(current)+"</p>";out+="<p>남은 시간: "+std::to_string(pending.value("remaining",0))+" "+escape(cfg.value("unitLabel",""))+"</p>";
          Json advance={{"type","advanceSession"}},cancel={{"type","cancelSession"}};std::string reason;const bool available=game.available(advance,&reason);
          out+=button(prefix+"advance","다음 시점까지 진행",advance,available,reason)+button(prefix+"cancel","진행 취소 (행동력 반환 없음)",cancel,game.available(cancel));return out;
        }else{Json wait={{"type","waitTime"},{"ticks",1}};out+=button(prefix+"wait","1 "+cfg.value("unitLabel","")+" 대기",wait,game.available(wait));}
      }
      if(display_content.at("project").value("roster",Json::object()).value("stateModel","")=="character-v1"){
        const auto definitions=display_content.at("project").at("gameplay").value("actions",Json::array());
        for(const auto& member:display_content.at("project").at("roster").at("members")){
          const auto actor=member.value("characterId","");std::string name=actor;for(const auto& c:display_content.at("characters"))if(c.at("id")==actor)name=c.value("name",actor);
          out+="<div style=\"display:inline-block;width:46%;margin:1%;vertical-align:top;\"><b>"+escape(name)+"</b>";
          if(game.state.contains("progression")&&game.state.at("progression").contains("actors")){const auto account=game.state.at("progression").at("actors").at(actor);out+="<p>개인 행동력 "+std::to_string(account.value("actionPoints",0))+" · 사용 시간 "+std::to_string(account.value("elapsed",0))+"</p>";}
          for(const auto& id:member.value("actionIds",Json::array()))for(const auto& definition:definitions)if(definition.at("id")==id&&included("actionIds",id)){
            const auto key="actor-action__"+e.value("id","")+"__"+actor+"__"+id.get<std::string>();const auto controls=quantity_controls(key,definition.value("maxQuantity",1));
            Json command={{"type","executeActorAction"},{"actorId",actor},{"actionId",id},{"quantity",quantities[key]}};std::string reason;const bool enabled=game.available(command,&reason);
            out+="<div>"+controls+button(key,definition.value("name","실행")+time_summary(id.get<std::string>(),quantities[key]),command,enabled,reason)+"</div>";
          }
          out+="</div>";
        }
        return out;
      }
      for(const auto& row:gp.value("actions",Json::array())) {
        if(!included("actionIds",row.at("id")))continue;const auto id=row.at("id").get<std::string>();const auto key="action__"+e.value("id","")+"__"+id;const auto controls=quantity_controls(key,row.value("maxQuantity",1));const int quantity=quantities[key];Json action={{"type","executeAction"},{"actionId",id},{"quantity",quantity}};std::string reason;const bool enabled=game.available(action,&reason);
        out+="<div class=\"facility-row\"><b>"+escape(row.value("name",""))+"</b><p>"+escape(cost_summary(row,quantity)+"\n"+effect_summary(row,quantity))+"</p>"+controls+button(key,row.value("name","실행")+time_summary(id,quantity),action,enabled,reason)+"</div>";
      }
    } else if(kind=="diets") {
      for(const auto& row:gp.value("diets",Json::array())) {
        const auto id=row.value("id","");const bool selected=row.value("selected",false);
        Json action={{"type","setDiet"},{"dietId",id}};std::string reason;const bool enabled=game.available(action,&reason)&&!selected;
        out+="<div class=\"facility-row\"><b>"+escape(row.value("name","")+(selected?" · 선택 중":""))+"</b><p>"+escape(cost_summary(row)+" · "+std::to_string(row.value("intervalPeriods",1))+"기간마다\n"+effect_summary(row))+"</p>"+button("diet-"+id,"식단 선택",action,enabled,selected?"현재 식단":reason)+"</div>";
      }
      Json stop={{"type","setDiet"},{"dietId",nullptr}};if(!game.state.is_null()&&!game.state.at("gameplay").value("dietId",Json()).is_null())out+=button("diet-clear","식단 해제",stop,game.available(stop));
    } else if(kind=="services") {
      for(const auto& row:gp.value("services",Json::array())) {
        const auto id=row.value("id","");const auto key="service-"+id;const auto controls=quantity_controls(key,row.value("maxQuantity",1));const int q=quantities[key];
        Json action={{"type","useService"},{"serviceId",id},{"quantity",q}};std::string reason;const bool enabled=game.available(action,&reason);
        out+="<div class=\"facility-row\"><b>"+escape(row.value("name",""))+"</b><p>"+escape(cost_summary(row,q)+"\n"+effect_summary(row,q))+"</p>"+controls+button(key,"이용",action,enabled,reason)+"</div>";
      }
    } else if(kind=="sell")for(const auto& shop:gp.at("shops")) {
      if(e.contains("shopId")&&e.at("shopId")!=shop.at("id"))continue;
      for(auto row:shop.value("sellOffers",Json::array())) {
        row["currencyAttributeId"]=shop.at("currencyAttributeId");const auto id=row.value("itemId","");const auto key="sell-"+shop.value("id","")+"-"+id;
        const auto controls=quantity_controls(key,row.value("maxQuantity",99));const int q=quantities[key];Json action={{"type","sell"},{"shopId",shop.at("id")},{"itemId",id},{"quantity",q}};
        std::string reason;const bool enabled=game.available(action,&reason);
        out+="<div class=\"facility-row\">"+item_icon(id)+"<b>"+escape(row.value("name",id))+"</b><p>"+escape("받을 금액 "+cost_summary(row,q)+" · 소지 "+std::to_string(row.value("owned",0)))+"</p>"+controls+button(key,"판매",action,enabled,reason)+"</div>";
      }
    }
    return out.empty()?"<p>이 장소에서 이용할 항목이 없습니다.</p>":out;
  }
  Json systems() const { return display_projection.value("systems",Json()); }
  Json narrative() const { return display_projection.value("narrative", Json()); }
  std::string phase() const {
    if(!preview_screen.empty()&&!preview_scene.is_null())return "dialogue";
    if (game.state.is_null()) return "title";
    const auto phase = game.state.value("phase", "planning");
    if (phase == "narrative") return narrative().at("node").value("type", "") == "choice" ? "choice" : "dialogue";
    if (phase == "activity-result") return "result";
    if (phase == "ready" && game.continuous() && game.content.at("project").at("visual").contains("resultPlayback")) {
      // A successful save/load notice does not turn an ongoing schedule into
      // planning. Keep the planning fallback when the next command is blocked.
      const auto next = game.schedule_step();
      if (game.message.empty() || (!next.empty() && game.available({{"type",next}}))) return "auto-ready";
    }
    if (phase == "finished") return "finished";
    if (phase == "battle" || phase == "expedition") return phase;
    return "planning";
  }
  bool visible(const Json& element) const {
    if (element.value("hidden",false)) return false;
    if(element.contains("visibleCondition")&&!visual_condition(element.at("visibleCondition")))return false;
    const auto when=element.value("visibleWhen","always"); return when=="always" || when==phase();
  }
  bool visual_condition(const Json& condition) const {
    try { return game.call({{"operation","evaluate"},{"state",game.state},{"condition",condition}}).value("value",false); }
    catch(...) { return false; }
  }
#include "data-binding.inc"
  std::string bind(const std::string& name,const Json& element=Json::object()) const {
    if(name=="player-notice")return game.message;
    if(name=="choice-result") {
      if(game.choice_feedback.is_null()||phase()!="dialogue")return "";
      std::string out;
      for(const auto& change:game.choice_feedback.value("changes",Json::array()))if(change.value("kind","")=="numeric") {
        if(!out.empty())out+=" · ";
        const auto delta=change.value("delta",0.0);
        out+=target_name(change.at("target"))+" "+number(change.at("before"))+" → "+number(change.at("after"))+" ("+(delta>=0?"+":"−")+number(std::abs(delta))+")";
      }
      if(out.empty())out=settings_words("능력치·관계 수치 변화 없음","No stat or relationship change","能力値・関係値の変化なし");
      const auto hint=game.choice_feedback.value("consequenceText","");
      return settings_words("선택 결과 · ","Choice result · ","選択結果 · ")+out+(hint.empty()?"":"\n"+hint);
    }
    if(name=="reading-status")return player_preferences.value("autoAdvance",false)?(phase()=="choice"?settings_words("자동 진행 대기 · 고르면 이어집니다","Auto paused · choose to continue","自動送り待機 · 選択後に再開"):settings_words("자동 진행 중 · 선택지에서 멈춤","Auto · stops at choices","自動送り · 選択肢で停止")):settings_words("직접 읽기","Manual reading","手動送り");
    if(name=="save-summary"){
      const auto status=saved_preview.value("status","empty");
      if(element.value("valueFormat","")=="write-warning")return saved_preview.value("justSaved",false)?"":"저장하면 기존 기록을 지금의 진행으로 바꿉니다.";
      if(status=="empty")return element.value("valueFormat","")=="status"?"아직 저장한 기록이 없습니다.":"—";
      if(status=="invalid")return element.value("valueFormat","")=="status"?"기록을 읽을 수 없습니다. 원본 파일은 보존됩니다.":"—";
      if(element.value("valueFormat","")=="status")return saved_preview.value("sameContent",false)?"이어 읽을 수 있는 기록":"다른 콘텐츠 버전의 기록 · 불러올 때 호환성을 확인합니다.";
      const auto& saved=saved_preview.at("state");const auto& project=display_content.at("project");
      if(element.value("valueFormat","")=="attributes"){
        Json attributes=display_content.at("attributes");
        if(element.contains("attributeIds")){attributes=Json::array();for(const auto& id:element.at("attributeIds"))for(const auto& a:display_content.at("attributes"))if(a.at("id")==id)attributes.push_back(a);}
        std::string out;for(const auto& a:attributes){
          const auto id=a.at("id").get<std::string>();
          if(element.contains("attributeIds")&&std::find(element.at("attributeIds").begin(),element.at("attributeIds").end(),a.at("id"))==element.at("attributeIds").end())continue;
          if(!out.empty())out+="\n";out+=a.value("label",id)+"   "+std::to_string(saved.at("attributes").value(id,int64_t(0)));
        }return out;
      }
      const int period=saved.value("periodIndex",0),months=period/game.periods(),month=project.value("startMonth",1)-1+months;
      std::string chapter=std::to_string(project.value("startYear",1)+month/12)+"년 "+std::to_string(month%12+1)+"월";
      if(saved.value("phase","")=="finished")return chapter+" · 마무리 기록";
      chapter+=" · "+std::to_string(period%game.periods()+1)+"번째 일정";
      const auto gp=saved.value("gameplay",Json::object());
      const auto destination=gp.value("screenId",std::string());
      std::string place;
      for(const auto& s:project.at("visual").at("screens"))if(s.at("id")==destination)place=s.value("label",destination);
      if(place.empty())for(const auto& l:project.value("gameplay",Json::object()).value("locations",Json::array()))if(l.at("id")==gp.value("locationId",""))place=l.value("name","");
      const auto narrative=saved.value("activeNarrative",Json());
      if(narrative.is_object())chapter+="\n이야기 중";
      else chapter+="\n"+(place.empty()?std::string(saved.value("phase","")=="planning"?"일정을 정하는 중":"하루를 보내는 중"):place);
      return chapter;
    }
    if(name=="schedule-summary"){
      std::string out;const bool current=!game.state.is_null()&&plan_month==game.state.value("periodIndex",0)/game.periods();
      for(size_t i=0;current&&i<plan.size();++i){
        if(i)out+="  /  ";out+=std::to_string(i+1)+"주 ";
        if(!(plan_selected&(1<<i))){out+="미정";continue;}
        std::string label=plan[i];for(const auto& a:display_content.at("activities"))if(a.at("id")==plan[i])label=a.value("label",label);
        out+=label;
      }
      if(element.value("valueFormat","")=="count"){
        int selected=0;for(size_t i=0;current&&i<plan.size();++i)if(plan_selected&(1<<i))++selected;
        return selected?std::to_string(selected)+" / "+std::to_string(game.periods())+"주 편성됨":std::to_string(game.periods())+"주의 일정을 정해 주세요";
      }
      return out.empty()?"아직 정하지 않은 일정":out;
    }
    if(name=="activity-progress"){
      if(game.state.is_null())return "";
      std::string label;const auto result=game.state.value("lastActivityResult",Json());
      if(result.is_object())for(const auto& a:display_content.at("activities"))if(a.at("id")==result.value("activityId",""))label=a.value("label","");
      const int current=game.state.value("periodIndex",0)%game.periods();
      if(element.contains("valueFormat")){
        auto text=element.at("valueFormat").get<std::string>();
        for(const auto& item:std::vector<std::pair<std::string,std::string>>{{"{activity}",label},{"{current}",std::to_string(current+1)},{"{total}",std::to_string(game.periods())}}){std::size_t pos=0;while((pos=text.find(item.first,pos))!=std::string::npos){text.replace(pos,item.first.size(),item.second);pos+=item.second.size();}}
        return text;
      }
      std::string track;for(int i=0;i<game.periods();++i)track+=(i<current?"■ ":i==current?"◆ ":"□ ")+std::to_string(i+1)+"주    ";
      return track+" ·   "+label;
    }
    if(name=="data-value")return data_binding(element);
    const auto& project=display_content.at("project");
    if(name=="context-name") {
      const auto field=element.value("contextField","actorId"),id=element.value("uiContext",Json::object()).value(field,"");
      Json rows=Json::array();
      if(field=="actorId"||field=="characterId")rows=display_content.value("characters",Json::array());
      else if(field=="activityId")rows=display_content.value("activities",Json::array());
      else if(field=="attributeId")rows=display_content.value("attributes",Json::array());
      else {const std::map<std::string,std::string> keys={{"itemId","items"},{"actionId","actions"},{"locationId","locations"},{"shopId","shops"}};if(keys.count(field))rows=project.value("gameplay",Json::object()).value(keys.at(field),Json::array());}
      for(const auto& row:rows)if(row.value("id","")==id)return row.value("name",row.value("label",id));
      return "—";
    }
    if(name=="replay-position")return replay_controls?std::to_string(replay_cursor+1)+" / "+std::to_string(replay_count):"";
    if(name=="replay-notice")return replay_controls?replay_notice:"";
    if(name=="replay-mode")return replay_controls?(replay_autoplay?"자동 재생 중":"자동 재생 멈춤"):"";
    if(name=="replay-speed")return replay_controls?std::to_string(replay_speed)+"×":"";
    if(name=="replay-title")return replay_controls?replay_title:"";
    if(name=="gallery-error")return game.gallery.error;
    if(name=="gallery-filter"){const std::map<std::string,std::string> kinds={{"all","전체 종류"},{"cg","CG"},{"music","음악"},{"event","사건"},{"ending","엔딩"}},statuses={{"all","모든 기록"},{"locked","미해금"},{"unlocked","해금됨"}};return kinds.at(gallery_kind)+" · "+statuses.at(gallery_status)+(gallery_category.empty()?"":" · "+gallery_category);}
    if(name=="gallery-summary"){size_t unlocked=0,total=0;for(const auto& e:project.value("gallery",Json::object()).value("entries",Json::array())){++total;if(!game.gallery.records(e).empty())++unlocked;}return std::to_string(unlocked)+" / "+std::to_string(total)+" 해금";}
    if(name=="selected-field"){const auto values=selected_values(element.value("selectionKey",""));if(values.is_null())return "선택 없음";const auto value=values.value(element.value("selectionField",""),Json());std::string text=value.is_string()?value.get<std::string>():value.is_null()?"":value.dump(),format=element.value("valueFormat","{value}");size_t pos=0;while((pos=format.find("{value}",pos))!=std::string::npos){format.replace(pos,7,text);pos+=text.size();}return format;}
    if(name=="schedule-prompt") {
      if(plan_selected!=((1<<game.periods())-1))return "스케줄 결정.\n이번 달 딸의 예정은? 제 "+std::to_string(plan_slot+1)+"회";
      const int next=game.state.value("periodIndex",0)%game.periods();
      if(game.continuous())return "이번 달 일정을 연속으로 진행합니다.";
      return next==0?"예, 스케줄에 편성하겠습니다.\n첫 일정을 시작하시겠습니까?":
        "이번 달 "+std::to_string(next)+"번째 일정을 마쳤습니다.\n다음 일정을 진행하시겠습니까?";
    }
    if(name.rfind("calendar-",0)==0) {
      const auto time=display_projection.value("time",Json());
      int year=time.is_null()?project.value("startYear",1):time.value("year",1),month=time.is_null()?project.value("startMonth",1):time.value("month",1);
      const int lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};const int days=lengths[std::clamp(month-1,0,11)]+(month==2&&year%4==0&&(year%100!=0||year%400==0)?1:0);
      const int day=time.is_null()?1:time.value("periodInMonth",0)*(game.periods()==3?10:days/game.periods())+1;
      if(name=="calendar-year")return std::to_string(year);
      if(name=="calendar-month")return std::to_string(month)+"월";
      if(name=="calendar-day")return std::to_string(day);
      if(name=="calendar-weekday"){static const char* days[]={"일","월","화","수","목","금","토"};int y=year,m=month;if(m<3){m+=12;--y;}int d=(day+13*(m+1)/5+y+y/4-y/100+y/400+6)%7;return days[d]+std::string("요일");}
    }
    if(name=="attribute-value"||name=="relationship-value"||name=="item-count") {
      int64_t value=0,delta=0;std::string label;
      const auto initial=project.value("initialState",Json::object());
      if(name=="attribute-value")for(const auto& attribute:display_content.at("attributes"))if(attribute.at("id")==element.value("attributeId",Json())) {
        const auto id=attribute.at("id").get<std::string>();label=attribute.value("label",id);
        value=game.state.is_null()?initial.value("attributes",Json::object()).value(id,attribute.value("initial",int64_t(0))):game.state.at("attributes").value(id,int64_t(0));
      }
      if(name=="relationship-value")for(const auto& character:display_content.at("characters"))if(character.at("id")==element.value("characterId",Json()))for(const auto& relationship:character.at("relationships"))if(relationship.at("id")==element.value("relationshipId",Json())) {
        const auto cid=character.at("id").get<std::string>(),rid=relationship.at("id").get<std::string>();label=character.value("name",cid)+" · "+relationship.value("label",rid);
        if(!game.state.is_null()&&!replay_controls)delta=game.relationship_changes.value(cid,Json::object()).value(rid,int64_t(0));
        value=game.state.is_null()?initial.value("relationships",Json::object()).value(cid,Json::object()).value(rid,relationship.value("initial",int64_t(0))):game.state.at("relationships").value(cid,Json::object()).value(rid,int64_t(0));
      }
      if(name=="item-count")for(const auto& item:project.value("gameplay",Json::object()).value("items",Json::array()))if(item.at("id")==element.value("itemId",Json())) {
        const auto id=item.at("id").get<std::string>();label=item.value("name",id);
        value=game.state.is_null()?item.value("initialQuantity",int64_t(0)):game.state.value("gameplay",Json::object()).value("inventory",Json::object()).value(id,int64_t(0));
      }
      auto format=element.value("valueFormat","{value}");
      for(const auto& [token,replacement]:std::vector<std::pair<std::string,std::string>>{{"{value}",std::to_string(value)},{"{label}",label},{"{delta}",(delta>0?"+":"")+std::to_string(delta)}}) {
        size_t at=0;while((at=format.find(token,at))!=std::string::npos){format.replace(at,token.size(),replacement);at+=replacement.size();}
      }
      return format;
    }
    if (name=="title") return project.value("title","");
    if (name=="user-field") {auto format=element.value("valueFormat","{value}");const auto value=user_field_value(element.value("userFieldId",""));for(size_t pos=0;(pos=format.find("{value}",pos))!=std::string::npos;pos+=value.size())format.replace(pos,7,value);return format;}
    if (name=="narrative-actor") {
      if(game.state.is_null()||game.state.value("activeNarrative",Json()).is_null())return "";
      const auto id=game.state.at("activeNarrative").value("actorId","");
      for(const auto& character:display_content.at("characters"))if(character.at("id")==id)return character.value("name","");
      return "";
    }
    if (name=="description") return project.value("description","");
    if (name=="location") return gameplay().is_null()?"":gameplay().at("location").value("name","");
    if (name=="calendar") { const auto time=display_projection.value("time",Json()); if(time.is_null())return ""; if(locale.rfind("en",0)==0)return "Year "+std::to_string(time.at("year").get<int>())+" · Month "+std::to_string(time.at("month").get<int>())+" · Period "+std::to_string(time.at("periodInMonth").get<int>()+1); return std::to_string(time.at("year").get<int>())+"년 "+std::to_string(time.at("month").get<int>())+"월 · "+std::to_string(time.at("periodInMonth").get<int>()+1)+"번째 일정"; }
    if (name=="ending" && !game.state.is_null() && !game.state.at("endingId").is_null()) for(const auto& e:display_content.at("endings"))if(e.at("id")==game.state.at("endingId"))return e.value("label","");
    if (name=="story") {const auto story=narrative();if(!story.is_null()){const auto& node=story.at("node");return node.value("type","")=="choice"?node.value("prompt",""):node.value("text","");}}
    if (name=="speaker") {const auto story=narrative();if(!story.is_null()&&story.at("node").contains("speakerId"))for(const auto& c:display_content.at("characters"))if(c.at("id")==story.at("node").at("speakerId"))return c.value("name","");}
    return "";
  }
  std::string button(std::string id, const std::string& label, Json action, bool enabled=true, std::string reason="") {
    // Prefixing makes authored IDs collision-free with the screen root and list container IDs.
    std::string base=id; for(int i=2;actions.count(id);i++)id=base+"-"+std::to_string(i);
    actions[id]=std::move(action); labels[id]=label;
    return "<button id=\""+escape(id)+"\""+(enabled?"":" disabled=\"disabled\"")+">"+escape(label)+"</button>"+(reason.empty()?"":"<div class=\"reason\">"+escape(player_reason(reason))+"</div>");
  }
  std::string image(const Json& element) const {
    std::string source=element.value("imageSource","asset"); Json id=nullptr;
    auto scene=display_scene();
    if(source=="asset")id=element.value("imageAssetId",Json());
    else if(source=="speaker-portrait") {
      id=element.value("imageAssetId",Json());const auto story=narrative();
      if(!story.is_null())for(const auto& character:display_content.at("characters"))if(character.at("id")==story.at("node").value("speakerId",Json()))id=character.at("portraits").value(element.value("portraitId","face"),id);
    }
    else if(source=="background")id=scene.value("backgroundAssetId",Json());
    else if(source.rfind("portrait-",0)==0) {
      for(const auto& portrait:scene.value("portraits",Json::array()))if(portrait.value("position","")==source.substr(9))
        for(const auto& character:display_content.at("characters"))if(character.at("id")==portrait.at("characterId"))id=character.at("portraits").value(portrait.at("portraitId").get<std::string>(),Json());
    }
    const auto path=asset(id);if(path.empty())return "";
    const std::string fit=element.value("fit",source=="background"?"cover":"contain");
    return "<div style=\"width:100%;height:100%;decorator:image(&quot;"+escape(path)+"&quot; "+fit+");\"></div>";
  }
  std::string item_icon(const std::string& id) const {
    const auto definition=display_content.at("project").value("gameplay",Json::object());
    for(const auto& item:definition.value("items",Json::array()))if(item.value("id","")==id) {
      const auto path=asset(item.value("imageAssetId",Json()));
      if(!path.empty())return "<div class=\"item-icon\" style=\"decorator:image(&quot;"+escape(path)+"&quot; contain);\"></div>";
    }
    return "";
  }
  std::string checked_button(const std::string& id,const std::string& label,const Json& command) {
    std::string reason;const bool enabled=game.available(command,&reason);
    return button(id,label,command,enabled,enabled?"":reason);
  }
  std::string battle_result(const Json& view) const {
    const auto result=view.value("lastBattle",Json());if(result.is_null())return "";
    const auto outcome=result.value("outcome","");
    std::string out="<div class=\"row battle-result\"><div class=\"heading\">"+std::string(outcome=="victory"?"전투 승리":outcome=="defeat"?"전투 패배":"전투에서 도주")+"</div>";
    out+="<p>준 피해 "+std::to_string(result.value("enemyDamage",0))+" · 받은 피해 "+std::to_string(result.value("heroDamage",0))+"</p>";
    for(const auto& lost:result.value("discardedItems",Json::array())) {
      const auto id=lost.value("itemId","");std::string name=id;
      for(const auto& item:display_content.at("project").value("gameplay",Json::object()).value("items",Json::array()))if(item.at("id")==id)name=item.value("name",id);
      out+="<p>가방 공간 부족으로 받지 못한 전리품: "+escape(name)+" × "+std::to_string(lost.value("quantity",0))+"</p>";
    }
    return out+"</div>";
  }
  #include "tactics-ui.inc"
  #include "requirement-hint.inc"
  std::string systems_body(const std::string& kind,const std::string& panel="all") {
    if(!game.state.is_null()&&display_content.at("project").contains("systems")&&display_content.at("project").at("systems").contains("tactics")&&kind=="battle")return tactics_body(panel);
    const auto view=systems();if(view.is_null())return "<p>표시할 정보가 없습니다.</p>";
    std::string out;
    if(kind=="equipment") {
      const auto stats=view.at("heroStats");out="<div class=\"heading\">장비</div><p>체력 "+std::to_string(stats.at("maxHp").get<int>())+" · 공격 "+std::to_string(stats.at("attack").get<int>())+" · 방어 "+std::to_string(stats.at("defense").get<int>())+"</p>";
      for(const auto& item:view.at("equipment")) {
        const auto id=item.value("id","");const bool equipped=item.value("equipped",false);
        out+="<div class=\"row\">"+item_icon(item.value("itemId",""))+escape(item.value("name",""))+" · 보유 "+std::to_string(item.value("owned",0))+(equipped?" · 장착 중":"");
        out+=equipped?checked_button("unequip-"+item.value("slot",""),"장착 해제",{{"type","unequip"},{"slot",item.at("slot")}}):button("equip-"+id,"장착",{{"type","equip"},{"equipmentId",id}},item.value("enabled",false),item.value("reason",Json()).is_string()?item.at("reason").get<std::string>():"");
        out+="</div>";
      }
    }
    if(kind=="quests") {
      out="<div class=\"heading\">의뢰</div>";
      for(const auto& quest:view.at("quests")) {
        const auto id=quest.value("id",""),status=quest.value("status","");
        const std::string label=status=="claimed"?"보상 수령 완료":status=="completed"?"달성":status=="accepted"?"진행 중":status=="locked"?"조건 미달":"수락 가능";
        out+="<div class=\"row\"><div>"+escape(quest.value("name",""))+" · "+label+"</div><p>"+std::to_string(quest.value("progress",0))+" / "+std::to_string(quest.value("targetCount",0))+"</p>";
        const auto reason=quest.value("reason",Json()).is_string()?quest.at("reason").get<std::string>():"";
        if(status=="available"||status=="locked")out+=button("accept-quest-"+id,"의뢰 수락",{{"type","acceptQuest"},{"questId",id}},quest.value("canAccept",false),reason);
        else if(status!="claimed")out+=button("claim-quest-"+id,"보상 받기",{{"type","claimQuest"},{"questId",id}},quest.value("canClaim",false),reason);
        out+="</div>";
      }
    }
    if(kind=="relationship-stages") {
      out="<div class=\"heading\">관계 단계</div>";
      for(const auto& stage:view.at("stages")) {
        const bool unlocked=stage.value("unlocked",false);const auto character=stage.value("characterId",""),relationship=stage.value("relationshipId","");
        const int current=game.state.at("relationships").value(character,Json::object()).value(relationship,0);
        out+="<div class=\"row\">"+escape(stage.value("name",""))+" · "+(unlocked?"도달":"진행 중")+"<p>"+std::to_string(current)+" / "+std::to_string(stage.value("threshold",0))+"</p></div>";
      }
    }
    if(kind=="expedition") {
      const auto trip=view.at("expedition");
      if(trip.is_null())return "<p>일정에서 탐험 활동을 선택해 출발하세요.</p>"+battle_result(view);
      out="<div class=\"heading\">"+escape(trip.value("name",""))+"</div><p>현재 위치: "+escape(trip.value("nodeName",""))+" · 체력 "+std::to_string(trip.value("heroHp",0))+" / "+std::to_string(trip.at("stats").value("maxHp",0))+"</p>";
      if(!trip.value("nodeDescription","").empty())out+="<p class=\"expedition-description\" style=\"display:block;width:100%;white-space:pre-wrap;word-break:break-all;\">"+escape(trip.value("nodeDescription",""))+"</p>";
      for(const auto& node:trip.at("connections")){
        const Json command={{"type","moveExpedition"},{"nodeId",node.at("id")}};
        std::string reason;const bool enabled=game.available(command,&reason);
        if(!enabled&&game.state.value("phase","")=="expedition")for(const auto& expedition:display_content.at("project").at("systems").at("expeditions"))if(expedition.at("id")==trip.at("expeditionId"))for(const auto& definition:expedition.at("nodes"))if(definition.at("id")==node.at("id")&&definition.contains("requirements"))reason="이동 조건: "+requirement_hint(definition.at("requirements"));
        out+=button("move-expedition-"+node.value("id",""),node.value("name",""),command,enabled,reason);
      }
      out+=checked_button("return-expedition","탐험에서 돌아가기",{{"type","returnFromExpedition"}});
      if(trip.contains("manualBattle")){const auto battle=trip.at("manualBattle");out+=button("start-node-battle",battle.value("name","")+"와 싸운다",{{"type","startNodeBattle"}},battle.value("enabled",false),battle.value("reason",Json()).is_string()?battle.at("reason").get<std::string>():"");}
      for(const auto& definition:display_content.at("project").at("systems").at("expeditions"))if(definition.at("id")==trip.at("expeditionId"))for(const auto& node:definition.at("nodes"))if(node.at("id")==trip.at("nodeId")&&!node.value("actionIds",Json::array()).empty())out+=facility_body({{"kind","actions"},{"id","expedition-node"},{"actionIds",node.at("actionIds")}});
      out+=battle_result(view);
    }
    if(kind=="battle") {
      const auto battle=view.at("battle");if(battle.is_null())return battle_result(view);
      out="<div class=\"heading\">"+escape(battle.value("enemyName",""))+" · "+std::to_string(battle.value("turn",0))+"턴</div><p>내 체력 "+std::to_string(battle.value("heroHp",0))+" / "+std::to_string(battle.value("heroMaxHp",0))+"</p><p>상대 체력 "+std::to_string(battle.value("enemyHp",0))+" / "+std::to_string(battle.value("enemyMaxHp",0))+"</p>";
      out+=checked_button("battle-attack","공격",{{"type","battleAction"},{"action","attack"}})+checked_button("battle-guard","방어",{{"type","battleAction"},{"action","guard"}});
      for(const auto& item:view.at("consumables"))out+=button("battle-item-"+item.value("itemId",""),item.value("name","")+" · 회복 "+std::to_string(item.value("heal",0))+" · 보유 "+std::to_string(item.value("quantity",0)),{{"type","battleAction"},{"action","useItem"},{"itemId",item.at("itemId")}},item.value("enabled",false));
      out+=checked_button("battle-flee","도주",{{"type","battleAction"},{"action","flee"}});
    }
    return out;
  }
  #include "schedule-board.inc"
  std::string calendar_schedule(const Json& e) {
    const bool editable=!progress_pending()&&!game.state.is_null()&&(game.state.value("phase","")=="planning"||game.state.value("phase","")=="ready");
    const int period=game.state.is_null()?0:game.state.value("periodIndex",0)%game.periods();
    const int monthIndex=game.state.is_null()?0:game.state.value("periodIndex",0)/game.periods();
    if(plan_month!=monthIndex){plan_month=monthIndex;plan_selected=game.state.is_null()||game.state.at("schedule").is_null()?0:((1<<game.periods())-1);plan_slot=period;plan_group.clear();plan.clear();if(!game.state.is_null()&&!game.state.at("schedule").is_null())for(const auto& id:game.state.at("schedule"))plan.push_back(id.get<std::string>());}
    if(plan.size()!=size_t(game.periods())){plan.assign(game.periods(),display_content.at("activities").at(0).value("id",""));for(const auto& a:display_content.at("activities"))if(a.value("id","")=="rest")plan.assign(game.periods(),"rest");}
    plan_slot=std::clamp(plan_slot,period,game.periods()-1);
    auto groups=e.value("scheduleGroups",Json::array());
    if(groups.empty())for(const auto& s:display_content.at("project").at("visual").at("screens"))for(const auto& el:s.at("elements"))if(el.value("scheduleLayout","")=="menu")groups=el.value("scheduleGroups",Json::array());
    auto label=[this](const std::string& id){for(const auto& a:display_content.at("activities"))if(a.at("id")==id)return a.value("label",id);return id;};
    std::string out;
    if(e.value("scheduleLayout","")=="calendar") {
      auto time=display_projection.value("time",Json::object());const auto& p=display_content.at("project");
      const int year=time.is_object()?time.value("year",p.value("startYear",1)):p.value("startYear",1),month=time.is_object()?time.value("month",p.value("startMonth",1)):p.value("startMonth",1);
      int y=year,m=month;if(m<3){m+=12;--y;}const int offset=(1+13*(m+1)/5+y+y/4-y/100+y/400+6)%7;
      const int lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};const int days=lengths[std::clamp(month-1,0,11)]+(month==2&&year%4==0&&(year%100!=0||year%400==0)?1:0);
      const double cw=e.value("width",480.0)/7,ch=(e.value("height",480.0)-80)/6;
      out+="<div style=\"height:36px;text-align:center;font-size:26px;color:#e9a09e;\">"+std::to_string(year)+"년  "+std::to_string(month)+"월</div>";
      const char* weeks_en[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
      const char* weeks_ko[]={"일","월","화","수","목","금","토"};
      const auto weeks=locale.rfind("ko",0)==0?weeks_ko:weeks_en;
      for(int i=0;i<7;i++)out+="<div style=\"position:absolute;left:"+number(i*cw)+"px;top:40px;width:"+number(cw)+"px;text-align:center;font-size:20px;\">"+weeks[i]+"</div>";
      for(int d=1;d<=days;d++){int cell=offset+d-1,slot=std::min(game.periods()-1,(d-1)/(game.periods()==3?10:days/game.periods()));std::string icon;if(plan_selected&(1<<slot))for(const auto& a:display_content.at("activities"))if(a.at("id")==plan[slot])icon=asset(a.value("iconAssetId",Json()));
        const auto id="calendar-day-"+std::to_string(d);actions[id]={{"type","planSlot"},{"slot",slot}};labels[id]=std::to_string(d)+"일 · "+label(plan[slot]);
        out+="<button class=\"calendar-cell\" id=\""+id+"\" style=\"position:absolute;left:"+number((cell%7)*cw)+"px;top:"+number(80+(cell/7)*ch)+"px;width:"+number(cw)+"px;height:"+number(ch)+"px;border-color:"+(slot==plan_slot?"#dfc579":"#967769")+";\""+(!editable||slot<period?" disabled=\"disabled\"":"")+"><div>"+std::to_string(d)+"</div>";
        if(!icon.empty())out+="<div style=\"width:28px;height:28px;margin-left:16px;decorator:image(&quot;"+escape(icon)+"&quot; contain);\"></div>";
        else if(plan_selected&(1<<slot))out+="<div class=\"calendar-activity-name\" style=\"font-size:12px;line-height:16px;height:32px;width:"+number(std::max(1.0,cw-4))+"px;text-align:center;white-space:normal;word-break:break-all;\">"+escape(calendar_short_label(label(plan[slot])))+"</div>";
        out+="</button>";
      }
      return out;
    }
    out="";
    const Json* group=nullptr;for(const auto& g:groups)if(g.at("id")==plan_group)group=&g;
    if(!group){out+="<div class=\"plan-categories\" style=\"top:auto;bottom:0;box-sizing:border-box;max-height:100%;max-width:100%;overflow-y:auto;\">";if(plan_selected==((1<<game.periods())-1)){out+=button("plan-start",game.continuous()?"월 일정 시작":period==0?"첫 일정 시작":"다음 일정 진행",{{"type","commitPlan"}},editable);out+=button("plan-revise",period==0?"다시 결정한다":"남은 일정 변경",{{"type","planReset"}},editable);}else for(const auto& g:groups)out+=button("plan-group-"+g.value("id",""),g.value("label",""),{{"type","planGroup"},{"group",g.at("id")}},editable);out+="</div>";}
    else {
      out+="<div class=\"plan-options\">";
      const auto visual=display_content.at("project").at("visual");
      const double padding=e.value("padding",visual.value("widgetTheme","default")=="ornate"?0.0:6.0);
      bool summaries=false;for(const auto& a:display_content.at("activities"))if(!a.value("previewAttributes",Json::array()).empty()){summaries=true;break;}
      const double available=std::max(1.0,e.value("width",528.0)-padding*2-e.value("borderWidth",0.0)*2-(summaries?16.0:0.0));
      const int columns=std::max(1,int((available+16)/236));
      const double card_width=std::max(1.0,(available-16*(columns-1))/columns-1);
      int card_index=0;
      for(const auto& id:group->at("activityIds"))for(const auto& a:display_content.at("activities"))if(a.at("id")==id){std::string caption=a.value("label","");auto at=caption.find(" · ");if(at!=std::string::npos)caption=caption.substr(at+4);int cost=0;for(const auto& c:a.at("costs"))cost+=c.value("amount",0);if(cost)caption+="  "+std::to_string(cost)+"G";
        auto html=button("plan-activity-"+id.get<std::string>(),caption,{{"type","planActivity"},{"activityId",id}},editable);const auto summary=activity_preview(a);
        html.insert(html.find('>')," style=\"width:"+number(card_width)+"px;margin-right:"+std::string(++card_index%columns?"16":"0")+"px;"+(summary.empty()?"":"height:auto;min-height:78px;")+"\"");
        if(!summary.empty())html.insert(html.rfind("</button>"),"<span class=\"activity-preview\" style=\"display:block;max-height:120px;overflow:auto;font-size:16px;line-height:20px;white-space:normal;word-break:break-all;\">"+escape(summary)+"</span>");auto icon=asset(a.value("iconAssetId",Json()));if(!icon.empty()){auto at=html.find('>')+1;html.insert(at,"<div class=\"plan-icon\" style=\"decorator:image(&quot;"+escape(icon)+"&quot; contain);\"></div>");}out+=html;
      }
      out+="</div>";out+=button("plan-back","뒤로",{{"type","planGroup"},{"group",""}},editable);
    }
    return out;
  }
  std::string render_node(const Json& raw) {
    if(!visible(raw))return "";
    if(!ui_modal.empty()&&!raw.contains("_authoredModal")&&raw.value("binding","")=="player-notice")return "";
    if(raw.contains("_clipRect")) {
      const auto clip=raw.at("_clipRect");if(clip.value("width",0.0)<=0||clip.value("height",0.0)<=0)return "";
      auto inner=raw;inner.erase("_clipRect");inner["x"]=raw.value("x",0.0)-clip.value("x",0.0);inner["y"]=raw.value("y",0.0)-clip.value("y",0.0);
      return "<div style=\"position:absolute;pointer-events:none;overflow:hidden;clip:always;left:"+number(clip.value("x",0.0))+"px;top:"+number(clip.value("y",0.0))+"px;width:"+number(clip.value("width",0.0))+"px;height:"+number(clip.value("height",0.0))+"px;\">"+render_node(inner)+"</div>";
    }
    const auto e=resolved_style(display_content.at("project").at("visual"),raw);const auto kind=e.value("kind","panel");
    std::string style="left:"+number(e.value("x",0.0))+"px;top:"+number(e.value("y",0.0))+"px;width:"+number(e.value("width",1.0))+"px;height:"+number(e.value("height",1.0))+"px;";
    const bool choice_cards=kind=="choices"&&e.value("choiceLayout","list")=="cards";
    if(e.contains("fill")&&!choice_cards)style+="background-color:"+css_color(e.at("fill"))+";";
    if(e.contains("color"))style+="color:"+css_color(e.at("color"))+";";
    if(e.contains("fontAssetId"))style+="font-family:engine-"+e.at("fontAssetId").get<std::string>()+";";
    if(e.contains("padding"))style+="padding:"+number(e.at("padding"))+"px;";
    if(e.contains("borderWidth")&&!choice_cards)style+="border-width:"+number(e.at("borderWidth"))+"px;border-color:"+css_color(e.value("borderColor","#ffffff"))+";";
    style+="font-size:"+number(e.value("fontSize",20.0))+"px;border-radius:"+number(e.value("radius",0.0))+"px;opacity:"+number(e.value("opacity",1.0))+";text-align:"+e.value("align","left")+";";
    if(kind=="group"&&e.value("overflow","")=="scroll")style+="padding:0;overflow-x:hidden;overflow-y:auto;";
    if(kind=="collection")style+="padding:0;overflow-x:hidden;overflow-y:auto;";
    if(kind=="plugin")style+="overflow:visible;";
    // Schedule popups need an elevated stacking layer, but their transparent
    // container must not swallow authored navigation buttons beneath it.
    // Plugin characters are decoration: they never swallow clicks meant for widgets beneath them.
    style+=((kind=="group"&&e.value("overflow","")!="scroll")||kind=="schedule"||kind=="plugin")?"pointer-events:none;":"pointer-events:auto;";
    if(kind=="schedule"&&display_content.at("project").contains("roster"))style+="overflow:hidden;";
    if(kind=="schedule"&&e.value("scheduleLayout","columns")=="menu")for(const auto& a:display_content.at("activities"))if(!a.value("previewAttributes",Json::array()).empty()){style+="overflow-x:hidden;overflow-y:auto;pointer-events:auto;";break;}
    auto body=element_body(e);
    if(!e.value("_boundEnabled",true)||(e.contains("enabledCondition")&&!visual_condition(e.at("enabledCondition")))) {
      for(const std::string tag:{"<button ","<select ","<input "})for(size_t at=0;(at=body.find(tag,at))!=std::string::npos;) {
        const auto end=body.find('>',at);if(body.substr(at,end-at).find(" disabled")==std::string::npos)body.insert(at+tag.size(),"disabled=\"disabled\" ");at=body.find('>',at)+1;
      }
    }
    if(kind=="meter"&&e.contains("meterThickness"))style+="background-color:transparent;";
    if(kind=="button"&&body.substr(0,body.find('>')).find("disabled=")!=std::string::npos)style+="opacity:"+number(e.value("opacity",1.0)*0.45)+";";
    return "<div id=\"node-"+escape(e.value("id",""))+"\" data-tooltip=\""+escape(e.value("tooltip",""))+"\" class=\"node "+kind+(kind=="image"||kind=="panel"||kind=="text"||kind=="button"||kind=="meter"||kind=="group"||kind=="input"||kind=="plugin"?"":" list")+"\" style=\""+style+"\">"+body+"</div>";
  }
  std::string roster_stats() {
    if(game.state.is_null())return "새 게임을 시작하면 대상별 능력을 표시합니다.";
    std::string out;
    const auto& r=display_content.at("project").at("roster");
    for(const auto& m:r.at("members")) {
      const auto id=m.value("characterId","");std::string name=id;
      for(const auto& c:display_content.at("characters"))if(c.at("id")==id)name=c.value("name",id);
      out+="<p><b>"+escape(name)+"</b></p>";
      for(const auto& key:r.at("localAttributeIds")) {
        std::string label=key.get<std::string>();for(const auto& a:display_content.at("attributes"))if(a.at("id")==key)label=a.value("label",label);
        out+="<div>"+escape(label)+": "+std::to_string(game.state.at("roster").at("attributes").at(id).at(key.get<std::string>()).get<int>())+"</div>";
      }
      if(game.state.at("roster").contains("inventory")){
        for(const auto& item:display_content.at("project").at("gameplay").at("items"))out+="<div>"+escape(item.value("name",""))+": "+std::to_string(game.state.at("roster").at("inventory").at(id).at(item.value("id","")).get<int>())+"</div>";
        for(const auto& other:display_content.at("characters"))for(const auto& relation:other.at("relationships"))out+="<div>"+escape(other.value("name","")+" · "+relation.value("label",relation.value("id","")))+": "+std::to_string(game.state.at("roster").at("relationships").at(id).at(other.value("id","")).at(relation.value("id","")).get<int>())+"</div>";
      }
    }
    out+="<p><b>공용 자원</b></p>";
    for(const auto& a:display_content.at("attributes")){bool local=false;for(const auto& key:r.at("localAttributeIds"))if(key==a.at("id"))local=true;if(!local)out+="<div>"+escape(a.value("label",""))+": "+std::to_string(game.state.at("attributes").at(a.value("id","")).get<int>())+"</div>";}
    return out;
  }
#include "activity-rules.inc"
  std::string roster_schedule(const Json& element) {
    if(game.state.is_null())return "새 게임을 시작하세요.";
    const auto phase=game.state.value("phase","");const bool editable=!progress_pending()&&(phase=="planning"||phase=="ready");
    if(roster_revision!=game.state.value("stateRevision",0)){roster_draft=game.state.at("roster").at("assignments");roster_revision=game.state.value("stateRevision",0);}
    const bool narrow=element.value("width",1100.0)<600;
    const std::string card_width=narrow?"96%":"46%",footer_height=narrow?"112":"60",list_bottom=narrow?"116":"64";
    std::string out="<div style=\"position:relative;width:100%;height:100%;\"><div style=\"margin:0;height:24px;\">이번 기간 · 캐릭터별 활동 (함께 정산)</div>";
    out+="<div id=\"roster-actors-"+escape(element.value("id",""))+"\" style=\"position:absolute;left:0;right:0;top:"+(narrow?std::string("54"):std::string("30"))+"px;bottom:"+list_bottom+"px;overflow-x:hidden;overflow-y:auto;pointer-events:auto;\">";
    for(const auto& m:display_content.at("project").at("roster").at("members")) {
      const auto id=m.value("characterId","");std::string name=id;for(const auto& c:display_content.at("characters"))if(c.at("id")==id)name=c.value("name",id);
      out+="<div style=\"display:inline-block;vertical-align:top;width:"+card_width+";margin:1%;\"><b>"+escape(name)+"</b><select id=\"roster-"+escape(id)+"\" style=\"width:95%;\""+(editable?std::string():" disabled=\"disabled\"")+">";
      for(const auto& a:display_content.at("activities"))if(!a.contains("expeditionId"))out+="<option value=\""+escape(a.value("id",""))+"\""+(roster_draft.value(id,"")==a.value("id","")?" selected=\"selected\"":"")+">"+escape(a.value("label",""))+"</option>";
      out+="</select>";
      if(element.value("showActivityRules",false))for(const auto& a:display_content.at("activities"))if(a.value("id","")==roster_draft.value(id,""))
        out+="<div id=\"roster-rules-"+escape(id)+"\" style=\"width:100%;box-sizing:border-box;font-size:17px;line-height:25px;white-space:normal;word-break:break-all;padding-top:10px;\">"+activity_rules(a,id)+"</div>";
      if(element.value("showActivityDescription",false))for(const auto& a:display_content.at("activities"))if(a.value("id","")==roster_draft.value(id,""))
        out+="<div id=\"roster-description-"+escape(id)+"\" style=\"width:100%;box-sizing:border-box;font-size:17px;line-height:25px;max-height:100px;overflow-x:hidden;overflow-y:auto;white-space:normal;word-break:break-all;padding:10px 4px 0 0;\">"+escape(a.value("description",""))+"</div>";
      out+="</div>";
    }
    out+="</div><div style=\"position:absolute;left:0;right:0;bottom:0;height:"+footer_height+"px;\">";
    out+="<div style=\"display:inline-block;width:"+card_width+";margin:1%;\">"+button("roster-assign","활동 배정 적용",{{"type","rosterCommit"}},editable)+"</div>";
    out+="<div style=\"display:inline-block;width:"+card_width+";margin:1%;\">"+button("roster-execute","배정한 활동 함께 실행",{{"type","executeActivity"}},editable&&phase=="ready"&&roster_draft==game.state.at("roster").at("assignments"))+"</div>";
    return out+"</div></div>";
  }
  std::string element_body(const Json& e) {
    const std::string kind=e.value("kind","panel");
    if(kind=="group"&&e.value("overflow","")=="scroll"){
      std::string out;double extent=0;const double ox=e.value("_worldX",e.value("x",0.0)),oy=e.value("_worldY",e.value("y",0.0));
      for(const auto& raw:render_elements)if(raw.value("_scrollParent","")==e.value("id","")){auto child=raw;child["_worldX"]=raw.value("x",0.0);child["_worldY"]=raw.value("y",0.0);child["x"]=raw.value("x",0.0)-ox;child["y"]=raw.value("y",0.0)-oy;extent=std::max(extent,child.value("y",0.0)+child.value("height",0.0));if(child.contains("_clipRect")){child["_clipRect"]["x"]=child["_clipRect"].value("x",0.0)-ox;child["_clipRect"]["y"]=child["_clipRect"].value("y",0.0)-oy;}out+=render_node(child);}
      return "<div style=\"position:relative;width:100%;height:"+number(extent)+"px;\">"+out+"</div>";
    }
    if(kind=="panel"||kind=="group")return "";
    if(kind=="input"){const auto id=e.value("userFieldId","");return "<input id=\"user-input-"+escape(e.value("id",""))+"\" type=\"text\" data-user-field=\""+escape(id)+"\" value=\""+escape(user_field_value(id))+"\" style=\"width:100%;height:100%;box-sizing:border-box;background-color:transparent;\""+(game.state.is_null()?"":" disabled=\"disabled\"")+"/>";}
    if(kind=="meter"){
      double value=e.value("value",0.0);
      if(e.contains("dataBinding")){auto numeric=e;numeric["valueFormat"]="{value}";try{value=std::stod(data_binding(numeric));}catch(...){value=0;}}
      const double maximum=e.value("maxValue",100.0),ratio=maximum>0?std::clamp(value/maximum,0.0,1.0):0.0;
      const double height=e.value("height",16.0),thickness=std::min(height,e.value("meterThickness",height));
      return "<div style=\"position:absolute;left:0;top:"+number((height-thickness)/2)+"px;width:100%;height:"+number(thickness)+"px;background-color:"+css_color(e.value("fill",Json("#00000000")))+";\"><div style=\"width:"+number(e.value("width",200.0)*ratio)+"px;height:100%;background-color:"+css_color(e.value("color",Json("#70b887")))+";\"></div></div>";
    }
    if(kind=="saveSlots"||kind=="loadSlots")return slots_body(e);
    if(kind=="playerSettings")return settings_body(e);
    if(kind=="image")return image(e);
    if(kind=="plugin")return plugin_body(e);
    if(kind=="text")return escape(e.value("binding","none")=="none"?e.value("text",""):bind(e.value("binding","none"),e));
    if(kind=="dialogue")return "<div class=\"dialogue\">"+escape(bind("story"))+"</div>";
    if(kind=="services"||kind=="diets"||kind=="sell"||kind=="policies"||kind=="actions")return facility_body(e);
    if(kind=="equipment"||kind=="quests"||kind=="expedition"||kind=="battle"||kind=="relationship-stages")return systems_body(kind,e.value("tacticsPanel","all"));
    if(kind=="button") {
      Json action={{"type",e.value("action","advance")}};
      // An instance may override the action while retaining the definition's old
      // target fields. Only forward the fields belonging to the chosen command.
      static const std::map<std::string,std::vector<std::string>> parameters={
        {"playSequence",{"sequenceId"}},{"openModal",{"screenId"}},{"selectRow",{"screenId","selectionKey","_rowId","_selectionSource"}},{"executeActorAction",{"actionId","actorId","quantity"}},{"navigate",{"screenId"}},{"visit",{"locationId"}},{"interact",{"interactionId"}},
        {"buy",{"shopId","itemId","quantity"}},{"sell",{"shopId","itemId","quantity"}},{"useService",{"serviceId","quantity"}},{"setDiet",{"dietId"}},{"equip",{"equipmentId"}},
        {"executeAction",{"actionId","quantity"}},{"selectPolicy",{"groupId","optionId"}},
        {"selectBattleAction",{"battleAction","itemId"}},{"selectBattleTarget",{"battleTarget"}},
        {"unequip",{"slot"}},{"acceptQuest",{"questId"}},{"claimQuest",{"questId"}},
        {"moveExpedition",{"nodeId"}},
        {"galleryBranches",{"entryId"}},{"galleryBranch",{"entryId","branchId"}},{"galleryView",{"entryId"}},{"galleryReplay",{"entryId"}},
        {"galleryMusicToggle",{"entryId"}},{"galleryMusicStop",{"entryId"}},
        {"galleryFilter",{"galleryFilter"}}
      };
      const auto selected=parameters.find(action.value("type",""));
      if(selected!=parameters.end())for(const auto& key:selected->second)if(e.contains(key))action[key]=e[key];
      if(action.at("type")=="assignActivity"){
        auto assignments=game.state.is_object()?game.state.value("roster",Json::object()).value("assignments",Json::object()):Json::object();
        assignments[e.value("actorId","")]=e.value("activityId","");action={{"type","assignActivities"},{"assignments",assignments}};
      }
      if(action.at("type")=="battleAction"){action["action"]=e.value("battleAction","attack");if(action["action"]=="useItem"&&e.contains("itemId"))action["itemId"]=e["itemId"];}
      if(action.at("type")=="selectBattleAction"&&action.value("itemId","").empty())action.erase("itemId");
      if((action.at("type")=="buy"||action.at("type")=="sell"||action.at("type")=="useService"||action.at("type")=="executeAction"||action.at("type")=="executeActorAction")&&!action.contains("quantity"))action["quantity"]=1;
      if(action.at("type")=="setDiet"&&!action.contains("dietId"))action["dietId"]=nullptr;
      if(action.at("type")=="selectPolicy"&&!action.contains("optionId"))action["optionId"]=nullptr;
      if(e.contains("selectionBindings")){action["_selectionKey"]=e.value("selectionKey","");action["_selectionBindings"]=e.at("selectionBindings");}
      auto boundAction=action;bool hasSelection=true;try{boundAction=selection_action(action);}catch(...){hasSelection=false;}
      const auto type=action.value("type","");std::string label=e.value("text","버튼");bool enabled=hasSelection;
      const auto raw_phase=game.state.is_null()?"title":game.state.value("phase","");
      if(type=="timelinePause"||type=="timelineSpeed"||type=="timelineSkip"){
        authored_timeline_controls|=type=="timelinePause"?1:type=="timelineSpeed"?2:4;
        const auto cursor=game.timeline_cursor.is_object()?game.timeline_cursor:Json::object();
        enabled=game.timeline_active()&&!cursor.value("completed",false)&&!replay_controls;
        if(e.value("autoLabel",true)){
          if(type=="timelinePause")label=cursor.value("paused",false)?settings_words("계속 재생","Resume","再開"):settings_words("일시정지","Pause","一時停止");
          if(type=="timelineSpeed")label=settings_words("속도 ×","Speed ×","速度 ×")+std::to_string(cursor.value("speed",1));
          if(type=="timelineSkip")label=settings_words("이번 활동 넘기기","Skip activity","この活動をスキップ");
        }
      }
      if(type=="dialogueAuto"){
        enabled=!replay_controls;
        if(e.value("autoLabel",true))label=player_preferences.value("autoAdvance",false)?settings_words("자동 진행: 켜짐","Auto: On","自動送り: オン"):settings_words("자동 진행: 꺼짐","Auto: Off","自動送り: オフ");
      }
      if(type=="galleryFilter"){
        const auto filter=e.value("galleryFilter","");
        if(filter=="kind:"+gallery_kind||filter=="status:"+gallery_status||(!gallery_category.empty()&&filter=="category:"+gallery_category))label="● "+label;
      }
      if(type=="advance") {
        label=raw_phase=="planning"?settings_words("일정 확정","Confirm schedule","予定を確定"):raw_phase=="ready"?settings_words("활동 시작","Start activity","活動を開始"):raw_phase=="activity-result"?settings_words("결과 확인 · 다음","Continue","次へ"):raw_phase=="narrative"?settings_words("이야기 계속","Continue story","物語を続ける"):settings_words("다음","Next","次へ");
        enabled=raw_phase!="title"&&raw_phase!="finished"&&raw_phase!="battle"&&raw_phase!="expedition"&&phase()!="choice";
        if(!e.value("autoLabel",true)){label=e.value("text","");if(raw_phase=="planning"&&plan_month>=0&&plan_selected!=((1<<game.periods())-1))enabled=false;}
      }
      if(type=="advance"&&progress_pending()&&(raw_phase=="planning"||raw_phase=="ready")){if(game.state.at("progression").contains("sessions")){enabled=game.available({{"type","advanceSession"}});label="다음 시점까지 진행";}else {enabled=false;label="행동 진행 중";}}
      if(type=="save")enabled=!game.state.is_null()&&(ui_modal.empty()||!saved_preview.value("justSaved",false));
      if(type=="load"&&!replay_controls)enabled=saved_preview.value("status","")=="ready";
      if(type=="commitPlan"){
        ensure_plan();
        enabled=!game.state.is_null()&&(raw_phase=="planning"||raw_phase=="ready")&&!progress_pending()&&plan_selected==((1<<game.periods())-1);
        if(enabled)try{game.call({{"operation","command"},{"state",game.state},{"command",{{"type","setSchedule"},{"activityIds",plan}}},{"expectedRevision",game.state.at("stateRevision")}});}catch(...){enabled=false;}
      }
      if(type=="assignActivities"||type=="executeActorAction"||type=="navigate"||type=="visit"||type=="buy"||type=="sell"||type=="useService"||type=="setDiet"||type=="executeAction"||type=="selectPolicy"||type=="interact") {
        enabled=raw_phase=="planning"||raw_phase=="ready"||(raw_phase=="expedition"&&type=="executeAction");
        // Preview against a serialized copy through the same C++ command contract.
        // This covers authored quantities and conditions without committing state.
        if(enabled)try {game.call({{"operation","command"},{"state",game.state},{"command",boundAction},{"expectedRevision",game.state.at("stateRevision")}});}catch(...){enabled=false;}
      }
      if(type=="navigate"&&display_content.at("project").at("visual").contains("routing")) {
        enabled=false;for(const auto& s:display_content.at("project").at("visual").at("screens"))if(s.at("id")==boundAction.value("screenId",Json())&&!s.value("modal",false))enabled=true;
        if(enabled&&!game.state.is_null())enabled=game.available(boundAction);
      }
      if(type=="equip"||type=="unequip"||type=="acceptQuest"||type=="claimQuest"||type=="moveExpedition"||type=="returnFromExpedition"||type=="battleAction")enabled=game.available(action);
      if(type=="selectBattleAction"||type=="selectBattleTarget"||type=="confirmBattleAction"||type=="cancelBattleAction")enabled=game.battle_ui_available(action);
      if(type.rfind("recollection",0)==0)enabled=replay_controls&&(type!="recollectionPrev"||replay_cursor>0);
      enabled=enabled&&hasSelection&&e.value("_boundEnabled",true)&&(!e.contains("enabledCondition")||visual_condition(e.at("enabledCondition")));
      auto result=button(e.value("id","button"),label,action,enabled);
      const std::string style="font-size:"+number(e.value("fontSize",20.0))+"px;";
      result.insert(7," style=\""+style+"\"");return result;
    }
    std::string out;
    if(kind=="collection") {
      out="<div style=\"position:relative;width:100%;height:"+number(e.value("_collectionExtent",0.0))+"px;\">";
      for(const auto& child:render_elements)if(child.value("_collectionParent","")==e.value("id","")&&!child.contains("_scrollParent"))out+=render_node(child);
      return out+"</div>";
    }
    const auto gp=gameplay(), story=narrative();
    if(kind=="stats"&&display_content.at("project").contains("roster"))return roster_stats();
    if(kind=="stats") for(const auto& a:display_content.at("attributes")) {
      if(e.contains("attributeIds")&&std::find(e.at("attributeIds").begin(),e.at("attributeIds").end(),a.at("id"))==e.at("attributeIds").end())continue;
      const int value=game.state.is_null()?a.value("initial",0):game.state.at("attributes").value(a.at("id").get<std::string>(),0);
      const auto max=a.value("max",100),min=a.value("min",0); const double percent=max==min?0:(value-min)*100.0/(max-min);
      if(e.value("statsLayout","")=="compact")out+="<div class=\"compact-stat\"><span class=\"stat-name\">"+escape(a.value("label",""))+"</span><span class=\"stat-number\">"+std::to_string(value)+"</span><div class=\"stat-meter\"><div style=\"width:"+number(std::min(100.0,value/4.0))+"%;\"/></div></div>";
      else out+="<div class=\"stat\"><span>"+escape(a.value("label",""))+"</span><span class=\"value\">"+std::to_string(value)+"</span><div class=\"bar\"><div style=\"width:"+number(percent)+"%;\"/></div></div>";
    }
    if(kind=="relationships")for(const auto& c:display_content.at("characters"))for(const auto& r:c.at("relationships")) {
      int value=game.state.is_null()?r.value("initial",0):game.state.at("relationships").at(c.at("id").get<std::string>()).value(r.at("id").get<std::string>(),0);
      out+="<div class=\"row\">"+escape(c.value("name","")+" · "+r.value("label",""))+" <b>"+std::to_string(value)+"</b></div>";
    }
    if(kind=="schedule"&&display_content.at("project").contains("roster"))return roster_schedule(e);
    if(kind=="schedule") {
      if(e.value("scheduleLayout","columns")=="board")return schedule_board(e);
      if(e.value("scheduleLayout","columns")!="columns")return calendar_schedule(e);
      ensure_plan();
      for(auto& id:plan)if(id.empty())id=display_content.at("activities").at(0).value("id","");
      plan_selected=(1<<game.periods())-1;
      const int period=game.state.is_null()?0:game.state.value("periodIndex",0)%game.periods();
      const bool editable=!progress_pending()&&!game.state.is_null()&&(game.state.value("phase","")=="planning"||game.state.value("phase","")=="ready");
      const auto column_width=std::max(70.0,(e.value("width",888.0)-12)/game.periods());
      for(int i=0;i<game.periods();i++) {out+="<div class=\"schedule-row\" style=\"width:"+number(column_width)+"px;\"><span>"+settings_words(std::to_string(i+1)+"번째 일정","Period "+std::to_string(i+1),std::to_string(i+1)+"番目の予定")+"</span><select style=\"width:"+number(column_width-16)+"px;\" id=\"plan-"+std::to_string(i)+"\""+(i<period||!editable?" disabled=\"disabled\"":"")+">";
        for(const auto& a:display_content.at("activities")){const auto summary=activity_preview(a);out+="<option style=\"width:"+number(column_width-32)+"px;"+(summary.empty()?std::string():"height:auto;min-height:40px;white-space:normal;")+"\" id=\"plan-"+std::to_string(i)+"-option-"+escape(a.value("id",""))+"\" value=\""+escape(a.value("id",""))+"\""+(plan[i]==a.at("id")?" selected=\"selected\"":"")+">"+escape(a.value("label","")+(summary.empty()?"":" · "+summary))+"</option>";}
        out+="</select><span class=\"dropdown-marker\">▼</span>";
        std::string selected_summary;for(const auto& a:display_content.at("activities"))if(a.at("id")==plan[i])selected_summary=activity_preview(a);
        out+="<div id=\"plan-summary-"+std::to_string(i)+"\" class=\"activity-preview\" style=\"font-size:16px;max-height:120px;overflow:auto;white-space:normal;word-break:break-all;\">"+escape(selected_summary)+"</div>";
        if(e.value("showActivityRules",false))for(const auto& a:display_content.at("activities"))if(a.at("id")==plan[i])out+="<div id=\"plan-rules-"+std::to_string(i)+"\" style=\"font-size:17px;line-height:25px;max-height:150px;overflow:auto;white-space:normal;padding-top:10px;\">"+activity_rules(a)+"</div>";
        if(e.value("showActivityDescription",false))for(const auto& a:display_content.at("activities"))if(a.at("id")==plan[i])out+="<div id=\"plan-description-"+std::to_string(i)+"\" style=\"font-size:17px;line-height:25px;max-height:125px;overflow:auto;white-space:normal;word-break:break-all;padding-top:10px;\">"+escape(a.value("description",""))+"</div>";
        out+="</div>";
      }
    }
    if(kind=="choices"&&!story.is_null()){
      int index=0;
      for(const auto& choice:story.value("choices",Json::array())){
        auto row=button("choose-"+choice.value("id",""),choice.value("text",""),{{"type","choose"},{"choiceId",choice.at("id")}},choice.value("enabled",false));
        if(e.value("choiceLayout","list")=="cards"){
          const auto ordinal=std::to_string(++index);
          row.insert(row.find('>')+1,std::string("<span style=\"display:inline-block;width:34px;opacity:0.6;\">")+(index<10?"0":"")+ordinal+"</span>");
        }
        if(e.value("showChoiceEffects",false))for(const auto& definition:story.at("node").value("choices",Json::array()))if(definition.at("id")==choice.at("id")) {
          auto preview=definition;preview["effects"]=raising::choice_immediate_effects(display_content,game.state.value("activeNarrative",Json()),definition);
          const auto effects=effect_summary(preview),hint=definition.value("consequenceText","");
          std::string summary=effects.empty()?settings_words("수치 변화 없음","No numeric change","数値の変化なし"):effects;
          if(!hint.empty())summary+="\n"+hint;
          row.insert(row.rfind("</button>"),"<span style=\"display:block;font-size:14px;line-height:18px;white-space:pre-wrap;opacity:0.85;\">"+escape(summary)+"</span>");
        }
        out+=row;
      }
    }
    if(kind=="locations"&&!gp.is_null())for(const auto& location:gp.at("locations")) {
      bool current=location.at("id")==gp.at("location").at("id");
      out+=button("visit-"+location.value("id",""),location.value("name","")+(current?" · 현재":""),{{"type","visit"},{"locationId",location.at("id")}},location.value("enabled",false)&&!current,location.value("reason",Json()).is_string()?location.at("reason").get<std::string>():"");
    }
    if(kind=="shop"&&e.value("shopLayout","")=="cards"&&!gp.is_null()) {
      for(const auto& shop:gp.at("shops"))if(!e.contains("shopId")||e.at("shopId")==shop.at("id"))for(const auto& offer:shop.at("offers")){
        const auto id=offer.value("itemId","");std::string description;for(const auto& item:display_content.at("project").at("gameplay").at("items"))if(item.at("id")==id)description=item.value("description","");
        const auto key="buy-"+shop.value("id","")+"-"+id;const auto controls=quantity_controls(key,offer.value("maxQuantity",99));const auto q=quantities[key];
        const auto html=item_icon(id)+"<span>"+escape(offer.value("name",""))+"<br/>"+std::to_string(offer.value("price",0)*q)+"G</span><div class=\"card-description\">"+escape(description)+"</div>";
        Json action={{"type","buy"},{"shopId",shop.at("id")},{"itemId",id},{"quantity",q}};std::string reason;const bool enabled=game.available(action,&reason);
        auto card=button(key,offer.value("name",""),action,enabled);auto at=card.find('>')+1;auto end=card.find("</button>",at);card.replace(at,end-at,html);out+="<div class=\"shop-card\">"+card+controls+(enabled?"":"<div class=\"muted\">"+escape(player_reason(reason))+"</div>")+"</div>";
      }return out;
    }
    if(kind=="shop"&&!gp.is_null())for(const auto& shop:gp.at("shops")) {
      if(e.contains("shopId")&&e.at("shopId")!=shop.at("id"))continue;
      out+="<div class=\"heading\">"+escape(shop.value("name",""))+"</div>";
      for(const auto& offer:shop.at("offers")) {out+="<div class=\"offer\">"+item_icon(offer.value("itemId",""))+"<div>"+escape(offer.value("name",""))+" · "+std::to_string(offer.value("price",0))+"<span class=\"muted\"> · 재고 "+(offer.at("stock").is_null()?"무제한":std::to_string(offer.at("stock").get<int>()))+"</span></div>";
        const auto key="buy-"+shop.value("id","")+"-"+offer.value("itemId","");out+=quantity_controls(key,offer.value("maxQuantity",99));Json action={{"type","buy"},{"shopId",shop.at("id")},{"itemId",offer.at("itemId")},{"quantity",quantities[key]}};std::string reason;const bool enabled=game.available(action,&reason);
        out+="<div>합계 "+std::to_string(offer.value("price",0)*quantities[key])+"</div>"+button(key,"구매",action,enabled,reason)+"</div>";
      }
    }
    if(kind=="inventory"&&!gp.is_null()) {if(gp.at("inventory").empty())out="<p class=\"muted\">가방이 비어 있습니다.</p>";for(const auto& item:gp.at("inventory"))out+="<div class=\"row\">"+item_icon(item.value("id",""))+escape(item.value("name",""))+" <b>× "+std::to_string(item.value("quantity",0))+"</b></div>";}
    if(kind=="interactions"&&!gp.is_null())for(const auto& interaction:gp.at("interactions"))out+=item_icon(interaction.value("itemId",""))+button("interact-"+interaction.value("id",""),interaction.value("name",""),{{"type","interact"},{"interactionId",interaction.at("id")}},interaction.value("enabled",false),interaction.value("reason",Json()).is_string()?interaction.at("reason").get<std::string>():"");
    if(kind=="result"&&game.timeline_active()&&game.timeline_plan.contains("resultPlayback"))return "<div data-result-playback=\"true\" data-result-fill=\""+escape(e.value("fill","#213c34e8"))+"\" data-result-color=\""+escape(e.value("color","#fff8e6"))+"\" data-result-changes=\""+std::string(e.value("resultLayout","summary")=="changes"?"true":"false")+"\" style=\"white-space:pre-wrap;line-height:38px;\">"+result_playback_markup(e.value("resultLayout","summary")=="changes",e)+"</div>";
    if(kind=="result"&&!game.state.is_null()&&!game.state.at("lastActivityResult").is_null()&&game.state.at("lastActivityResult").contains("actorResults")) {
      for(const auto& actor:game.state.at("lastActivityResult").at("actorResults")) {
        std::string name=actor.value("characterId","");for(const auto& c:display_content.at("characters"))if(c.at("id")==actor.at("characterId"))name=c.value("name",name);
        const auto& result=actor.at("result");std::string label=result.value("activityId","");for(const auto& a:display_content.at("activities"))if(a.at("id")==result.at("activityId"))label=a.value("label",label);
        out+="<div style=\"display:inline-block;width:46%;margin:1%;vertical-align:top;\"><div><b>"+escape(name+" · "+label)+"</b></div>";
        if(!result.at("success").is_null())out+="<div>"+std::string(result.at("success").value("passed",false)?"성공":"실패")+"</div>";
        for(const auto& change:result.at("changes"))if(change.value("kind","")=="numeric"&&change.value("delta",0)!=0)out+="<div>"+escape(target_name(change.at("target")))+" "+(change.value("delta",0)>0?"+":"")+std::to_string(change.value("delta",0))+"</div>";
        out+="</div>";
      }
      return out;
    }
    if(kind=="result"&&!game.state.is_null()&&!game.state.at("lastActivityResult").is_null()) {
      // Explicit content widths avoid RmlUi's intrinsic paragraph/scrollbar width cycle.
      const auto line_style=" style=\"display:block;width:"+number(std::max(1.0,e.value("width",448.0)-16))+"px;height:26px;line-height:26px;padding:0;margin:0;font-size:22px;\"";
      const auto result=game.state.at("lastActivityResult");for(const auto& a:display_content.at("activities"))if(a.at("id")==result.at("activityId"))out+="<div"+line_style+">"+escape(a.value("label",""))+" 완료</div>";
      const auto success=result.at("success");if(!success.is_null())out+="<div"+line_style+">"+std::string(success.value("passed",false)?"성공":"실패")+"</div>";
      std::map<std::string,std::pair<std::string,int>> totals;
      for(const auto& change:result.at("changes"))if(change.value("kind","")=="numeric"&&change.value("delta",0)!=0) {
        auto target=change.at("target");std::string label=target_name(target);
        auto& total=totals[target.dump()];total.first=label;total.second+=change.value("delta",0);
      }
      bool any_change=false;
      for(const auto& [key,total]:totals)if(total.second!=0){any_change=true;out+="<div"+line_style+">"+escape(total.first)+" "+(total.second>0?"+":"")+std::to_string(total.second)+"</div>";}
      if(!any_change)out+="<div"+line_style+">능력 변화 없음</div>";
      if(!systems().is_null())out+=battle_result(systems());
    }
    return out;
  }
  /** Extension elements mount per screen instance; unavailable plugins show a placeholder, never an error. */
  std::string plugin_body(const Json& e) {
    if(authored_gallery())return "<p>도감에서는 본편 애니메이션이 일시정지됩니다.</p>"; // Preserve paused live plugin instances.
    const auto ext=e.value("extension",Json::object());const auto plugin=ext.value("plugin","");
    const auto instance=screen_id+"/"+e.value("id","");
    if(extensions&&extensions->mount(instance,e))return "<raising-extension id=\"extension-"+escape(e.value("id",""))+"\" data-instance=\""+escape(instance)+"\" style=\"display:block;position:absolute;left:0;top:0;width:100%;height:100%;\"></raising-extension>";
    const auto* mount=extensions?extensions->find(instance):nullptr;const bool loaded=extensions&&extensions->plugin(plugin)&&extensions->plugin(plugin)->available;
    return "<div class=\"plugin-placeholder\" data-instance=\""+escape(instance)+"\" style=\"display:block;width:100%;height:100%;box-sizing:border-box;padding:8px;border-width:2px;border-color:#b98f5e;background-color:rgba(70,52,62,150);color:#fff0d0;font-size:18px;text-align:center;\">"+escape((loaded?"플러그인 오류: ":"플러그인 필요: ")+plugin)+(loaded&&mount&&!mount->error.empty()?"<br/>"+escape(mount->error):"")+"</div>";
  }
  Json fallback_screen(const std::string& phase) const {
    return {{"id",phase},{"elements",Json::array({
      {{"id","background"},{"kind","image"},{"imageSource","background"},{"fit","cover"},{"x",0},{"y",0},{"width",width},{"height",height}},
      {{"id","system-panel"},{"kind","panel"},{"x",width/25},{"y",height/25},{"width",width*23/25},{"height",height*23/25},{"fill","#f6f8ef"},{"radius",12}},
      {{"id","system-title"},{"kind","text"},{"text",phase=="battle"?"전투":"탐험"},{"x",width/16},{"y",height/16},{"width",width/2},{"height",height/12},{"fontSize",28}},
      {{"id","save"},{"kind","button"},{"text","저장"},{"action","save"},{"x",width*16/25},{"y",height/16},{"width",width*7/50},{"height",height/12},{"fill","#305b4c"}},
      {{"id","load"},{"kind","button"},{"text","불러오기"},{"action","load"},{"x",width*4/5},{"y",height/16},{"width",width*7/50},{"height",height/12},{"fill","#305b4c"}},
      {{"id","system-widget"},{"kind",phase},{"x",width/16},{"y",height/6},{"width",width*7/8},{"height",height*3/4}}
    })}};
  }
#include "gallery-screen.inc"
#include "gallery-data.inc"
  std::string title_screen;
  std::string document() {
    refreshDisplay();
    if(extensions&&!authored_gallery())extensions->begin_sync();
    actions.clear(); labels.clear(); authored_timeline_controls=0;
    const auto& visual=display_content.at("project").at("visual");
    const auto dimensions=logical_viewport(visual,viewport_width,viewport_height);width=dimensions.first;height=dimensions.second;
    const auto raw_phase=game.state.is_null()?"title":game.state.value("phase","");
    screen_id=raw_phase=="title"?"title":raw_phase=="finished"?"ending":raw_phase=="narrative"?"story":raw_phase=="battle"||raw_phase=="expedition"?raw_phase:
      (raw_phase=="planning"||raw_phase=="ready")&&game.state.contains("gameplay")?game.state.at("gameplay").value("screenId","schedule"):"schedule";
    if(visual.contains("routing")) {
      const auto& routing=visual.at("routing");screen_id=routing.at("defaultScreenId").get<std::string>();
      const auto key=raw_phase=="narrative"?(phase()=="choice"?"choice":"dialogue"):raw_phase;
      for(const auto& route:routing.at("phases"))if(route.at("phase")==key)screen_id=route.at("screenId").get<std::string>();
      if(routing.value("useLocationScreens",false)&&(raw_phase=="planning"||raw_phase=="ready")&&!game.state.is_null()&&game.state.contains("gameplay")) {
        const auto id=game.state.at("gameplay").value("screenId","");for(const auto& screen:visual.at("screens"))if(screen.at("id")==id)screen_id=id;
      }
      // First matching authored rule wins over location/phase/default, below explicit navigation.
      for(const auto& rule:routing.value("conditions",Json::array())) {
        const auto phases=rule.value("phases",Json::array());
        if(!phases.empty()&&std::find(phases.begin(),phases.end(),Json(key))==phases.end())continue;
        if(visual_condition(rule.at("when"))){screen_id=rule.at("screenId").get<std::string>();break;}
      }
      if(!game.state.is_null()&&game.state.contains("uiScreenId"))screen_id=game.state.at("uiScreenId").get<std::string>();
      else if(game.state.is_null()&&!title_screen.empty())screen_id=title_screen;
    }
    if(authored_gallery()){
      const auto config=display_content.at("project").at("gallery");
      screen_id=gallery_branch_view&&config.contains("branchScreenId")?config.at("branchScreenId").get<std::string>():!gallery_entry.empty()&&config.contains("detailScreenId")?config.at("detailScreenId").get<std::string>():config.at("screenId").get<std::string>();
    }
    if(!preview_screen.empty())screen_id=preview_screen;
    const Json* screen=nullptr;for(const auto& s:visual.at("screens"))if(s.at("id")==screen_id)screen=&s;
    render_elements=screen?expanded_collections(expanded_elements(visual,*screen,width,height,phase())):Json::array();
    Json generated_screen;
    if(!visual.contains("routing")&&!authored_gallery()&&preview_screen.empty()&&(raw_phase=="battle"||raw_phase=="expedition")) {
      bool has_widget=false;for(const auto& element:render_elements)if(visible(element)&&element.value("opacity",1.0)>0) {
        const auto kind=element.value("kind","");
        if(kind==raw_phase)has_widget=true;
        if(raw_phase=="battle"&&kind=="collection"&&element.value("source",Json::object()).value("type","")=="battleActions")has_widget=true;
        if(raw_phase=="battle"&&kind=="button"&&(element.value("action","")=="battleAction"||element.value("action","")=="selectBattleAction"))has_widget=true;
      }
      if(!has_widget){generated_screen=fallback_screen(raw_phase);screen=&generated_screen;render_elements=generated_screen.at("elements");}
    }
    if(!screen)throw std::runtime_error("표시할 화면이 없습니다: "+screen_id);
    const auto activity=game.state.is_object()&&game.state.value("phase","")=="activity-result"?game.state.value("lastActivityResult",Json::object()).value("activityId",""):"";
    auto apply_variant=[&](const Json& e){return raising_player::presentation_variant(e,context_activity(game.state,e),[&](const Json& c){return visual_condition(c);});};
    for(auto& element:render_elements)element=apply_variant(element);
    const auto portrait_scene=display_scene();
    for(auto& element:render_elements)if(element.value("kind","")=="image") {
      const auto source=element.value("imageSource","asset");
      if(source!="portrait-left"&&source!="portrait-center"&&source!="portrait-right")continue;
      for(const auto& portrait:portrait_scene.value("portraits",Json::array()))if(portrait.value("position","")==source.substr(9)) {
        const double scale=portrait.value("scale",1.0),dx=portrait.value("offsetX",0.0),dy=portrait.value("offsetY",0.0);
        const double w=element.value("width",0.0),h=element.value("height",0.0);
        element["x"]=element.value("x",0.0)+(w-w*scale)/2+w*dx;
        element["y"]=element.value("y",0.0)+h-h*scale+h*dy;
        element["width"]=w*scale;element["height"]=h*scale;break;
      }
    }
    std::string out="<rml><head><title>"+escape(display_content.at("project").value("title","Raising Player"))+"</title><style>";
    out+="div{display:block;} .equipment p,.quests p,.expedition p,.battle p,.relationship-stages p,.battle-result p{display:block;margin-top:6px;margin-bottom:6px;} span,b{display:inline;} body{font-family:Noto Sans KR;font-size:20px;color:#203a34;background-color:#dce9df;width:"+std::to_string(width)+"px;height:"+std::to_string(height)+"px;margin:0;} .node{position:absolute;box-sizing:border-box;overflow:hidden;} .list{overflow:auto;padding:6px;} button{font-family:Noto Sans KR;display:block;box-sizing:border-box;width:100%;min-height:40px;padding:9px 14px;margin-bottom:8px;color:#f8fff5;background-color:#305b4c;border-width:1px;border-color:#57806c;border-radius:7px;cursor:pointer;tab-index:auto;} button:hover{background-color:#417661;} button:focus{border-width:3px;border-color:#eabb68;} button:disabled{opacity:0.45;cursor:unavailable;} .button>button{height:100%;margin:0;background-color:transparent;border-width:0;} .button>button:hover{background-color:rgba(255,255,255,31);} .button>button:focus{border-width:3px;border-color:#eabb68;} .stat{height:54px;padding:3px 8px;} .value{float:right;font-weight:bold;} .bar{height:5px;margin-top:5px;background-color:#d8e3d8;} .bar>div{height:5px;background-color:#608c69;} .row{padding:10px 6px;border-bottom-width:1px;border-bottom-color:#d0ddcd;} .row>b{float:right;} .heading{font-size:24px;font-weight:bold;padding:4px 0 10px;} .reason,.muted{font-size:15px;color:#738175;} .item-icon{display:inline-block;width:56px;height:56px;vertical-align:middle;margin-right:10px;} .offer{padding:8px 3px;} .offer button{margin-top:8px;} .node.schedule{overflow:visible;z-index:20;} .dropdown-marker{position:absolute;right:15px;top:48px;font-size:14px;pointer-events:none;} .schedule-row{position:relative;display:inline-block;vertical-align:top;box-sizing:border-box;padding:8px;} select{display:block;height:44px;margin-top:8px;padding:0;background-color:#f8fff6;border-width:1px;border-color:#789c81;tab-index:auto;} selectvalue{display:block;width:auto;margin-right:26px;height:28px;padding:8px;color:#203a34;white-space:nowrap;} selectarrow{display:block;width:26px;height:44px;padding-top:8px;text-align:center;background-color:#d4e4d3;} selectbox{display:block;max-height:320px;padding:4px;background-color:#f5fbf1;border-width:1px;border-color:#54775d;} option{box-sizing:border-box;height:40px;white-space:nowrap;padding:8px;background-color:#f5fbf1;} option:hover{background-color:#c9e0cb;} .status{position:absolute;bottom:6px;left:12px;right:12px;padding:4px 10px;background-color:rgba(10,35,25,217);color:white;font-size:16px;}</style></head><body>";
    std::string extra_css="p{display:block;} .button>button{min-height:0;} .result .heading,.result .row,.result p{display:block;width:100%;box-sizing:border-box;} .result p{margin-top:4px;margin-bottom:4px;} .calendar-cell{padding:0;min-height:0;margin:0;border-radius:0;border-width:1px;background-color:transparent;font-size:24px;text-align:left;color:#fff0d0;} .calendar-cell:hover{background-color:#6d383a;} .plan-prompt{padding:8px 4px;}";
    extra_css+=".schedule button,.schedule select,.schedule selectbox,.schedule option{pointer-events:auto;} selectbox,option,selectarrow{color:#203a34;}";
    if(visual.value("widgetTheme","default")=="ornate")extra_css+="body{color:#fff0d0;} .list{padding:0;} .list button{background-color:transparent;color:#fff0d0;border-width:0;border-radius:0;padding:4px 8px;margin-bottom:4px;text-align:left;min-height:30px;} .list button:hover{color:#ffe673;background-color:#733e42;} .list button:focus{border-color:#ffd979;border-width:1px;} .list .muted,.list .reason{color:#c8a999;} .list .heading{font-size:22px;} .list .row{border-bottom-width:0;padding:3px;} .list .offer{padding:2px;} .list .offer button{margin-top:2px;} .node.schedule{z-index:20;} .button>button{padding:0;min-height:0;text-align:center;} .status{background-color:#46262d;color:#ffdf9b;}";
    extra_css+=".plan-categories{position:absolute;right:0;top:210px;width:310px;padding:10px 4px;background-color:#46343e;border-width:2px;border-color:#e5c880;}.plan-options>button{display:inline-block;vertical-align:top;width:244px;height:78px;margin:0 16px 16px 0;border-width:2px;border-color:#ddcbbb;background-color:#46343e;font-size:24px;white-space:normal;}.plan-icon{float:left;width:60px;height:60px;margin-right:6px;} .compact-stat{position:relative;height:32px;font-size:28px;line-height:32px;} .stat-name{display:inline-block;width:126px;background-color:#733c44;} .stat-number{display:inline-block;width:58px;text-align:right;} .stat-meter{position:absolute;left:184px;top:5px;height:22px;width:168px;border-width:1px;border-color:#ba8c84;background-color:#34151d;} .stat-meter>div{height:100%;background-color:#df5454;} .node.schedule .calendar-cell{border-width:1px;padding:0;margin:0;min-height:0;font-size:26px;background-color:transparent;}";
    extra_css+=".shop-card{display:inline-block;vertical-align:top;width:292px;height:112px;margin:0 12px 8px 0;}.shop-card>button{height:112px;border-width:2px;border-color:#ddcbbb;background-color:#46343e;font-size:24px;line-height:26px;}.shop-card .item-icon{float:left;width:56px;height:56px;margin-right:8px;}.card-description{clear:both;font-size:21px;padding-top:4px;}.purchase-dialog{position:absolute;z-index:100;right:8px;top:430px;width:304px;padding:12px;background-color:#46343e;color:#fff0d0;border-width:2px;border-color:#e5c880;}";
    Json modal_elements=Json::array();
    if(!authored_gallery()&&!ui_modal.empty())for(const auto& modal:visual.at("screens"))if(modal.at("id")==ui_modal&&modal.value("modal",false)){
      modal_elements=expanded_collections(expanded_elements(visual,modal,width,height,phase()));
      const auto prefix=ui_modal+"__";
      for(auto& e:modal_elements){e=apply_variant(e);e["id"]=prefix+e.at("id").get<std::string>();e["_authoredModal"]=true;for(const auto key:{"_collectionParent","_scrollParent","parentId"})if(e.contains(key))e[key]=prefix+e.at(key).get<std::string>();if(e.contains("_groupIds"))for(auto& id:e["_groupIds"])id=prefix+id.get<std::string>();render_elements.push_back(e);}break;
    }
    for(const auto& raw:render_elements) {
      const auto e=resolved_style(visual,raw);
      const auto selector="#node-"+e.value("id","");
      std::string typography;
      if(e.value("kind","")=="button")extra_css+=selector+">button{color:"+css_color(e.value("color","#ffffff"))+";}";
      if(e.value("kind","")=="saveSlots"||e.value("kind","")=="loadSlots"){
        extra_css+=selector+" button{border-width:2px;border-color:"+css_color(e.value("borderColor","#C7B99A"))+";}";
        extra_css+=selector+" button.slot-selected{border-width:3px;border-color:"+css_color(e.value("hoverFill","#345C50"))+";}";
        extra_css+=selector+" button:focus{border-width:3px;border-color:"+css_color("#B38B49")+";}";
      }
      if(e.value("kind","")=="playerSettings")extra_css+=selector+" button:focus,"+selector+" button:hover{border-width:2px;border-color:"+css_color("#B38B49")+";}";
      if(e.value("kind","")=="choices"&&e.value("choiceLayout","list")=="cards"){
        extra_css+=selector+">button{min-height:"+number(e.value("rowHeight",56.0))+"px;margin-bottom:"+number(e.value("gap",10.0))+"px;padding:12px 18px;text-align:left;white-space:normal;color:"+css_color(e.value("color","#263c36"))+";background-color:"+css_color(e.value("fill","#f5efdf"))+";border-width:"+number(e.value("borderWidth",1.0))+"px;border-color:"+css_color(e.value("borderColor","#c7b99a"))+";border-radius:"+number(e.value("radius",2.0))+"px;}";
        extra_css+=selector+">button:focus{border-width:3px;border-color:"+css_color(e.value("borderColor","#c7b99a"))+";}";
      }
      if(e.contains("fontAssetId"))typography+="font-family:engine-"+e.at("fontAssetId").get<std::string>()+";";
      if(raw.contains("styleId")||visual.contains("defaultStyle")){if(e.contains("fontSize"))typography+="font-size:"+number(e.at("fontSize"))+"px;";if(e.contains("color"))typography+="color:"+css_color(e.at("color"))+";";}
      if(!typography.empty())extra_css+=selector+","+selector+" button,"+selector+" select,"+selector+" .heading,"+selector+" .row{"+typography+"}";
      for(const auto& [field,pseudo]:std::vector<std::pair<std::string,std::string>>{{"hoverFill",":hover"},{"pressedFill",":active"}})if(e.contains(field))extra_css+=selector+" button"+pseudo+"{background-color:"+css_color(e.at(field))+";}";
      if(e.contains("disabledColor"))extra_css+=selector+" button:disabled{color:"+css_color(e.at("disabledColor"))+";}";
      const auto frame=asset(e.value("frameAssetId",Json())),entry=asset(e.value("entryFrameAssetId",Json()));
      if(!frame.empty())extra_css+=selector+" .plan-categories{font-size:28px;line-height:36px;border-width:0;padding:16px;decorator:image("+frame+");}";
      if(!entry.empty())extra_css+=selector+" .plan-options>button,"+selector+" .shop-card>button{border-width:0;padding:8px;decorator:image("+entry+");}";
    }
    extra_css+=".facility-row{padding:10px;margin-bottom:12px;border-width:1px;border-color:#987b66;} .facility-row p{margin:6px 0;} .quantity-controls{height:42px;margin:6px 0;} .quantity-controls button{display:inline-block;width:42px;height:36px;min-height:0;padding:2px;text-align:center;} .quantity-controls span{display:inline-block;width:128px;text-align:center;} .shop-card{height:190px;} .transaction-shade{position:absolute;left:0;top:0;width:100%;height:100%;z-index:90;background-color:rgba(0,0,0,160);} .purchase-dialog{max-height:90%;overflow-y:auto;}";
    extra_css+=".purchase-dialog button{min-height:42px;margin-top:8px;padding:8px 12px;border-width:1px;border-color:#e5c880;background-color:#68434d;color:#fff0d0;} .purchase-dialog button:hover,.purchase-dialog button:focus{background-color:#8a5965;border-color:#fff0a0;}";
    // RmlUi has no browser user-agent stylesheet. Unspecified scrollbar width
    // consumes the whole containing block when a long widget starts scrolling.
    extra_css+="scrollbarvertical{width:14px;} scrollbarhorizontal{height:14px;} scrollbarvertical slidertrack,scrollbarhorizontal slidertrack{background-color:#302c3a;} scrollbarvertical sliderbar,scrollbarhorizontal sliderbar{min-width:12px;min-height:24px;background-color:#8e809c;} scrollbarvertical sliderarrowdec,scrollbarvertical sliderarrowinc{height:12px;} scrollbarhorizontal sliderarrowdec,scrollbarhorizontal sliderarrowinc{width:12px;}";
    out.insert(out.find("</style>"),extra_css);
    out.insert(out.find("</style>"),".timeline-control button{width:100%;height:44px;min-height:0;padding:6px;border-width:1px;border-color:#acc6b3;background-color:#355c4c;color:#ffffff;font-size:18px;} .timeline-disabled button{background-color:#69796e;color:#bcc9c0;}");
    std::string base;for(const auto& raw:render_elements)if(!raw.contains("_authoredModal")&&!raw.contains("_collectionParent")&&!raw.contains("_scrollParent"))base+=render_node(raw);
    if(!authored_gallery()&&!ui_modal.empty()){
      for(const std::string tag:{"<button ","<input ","<select "})for(size_t at=0;(at=base.find(tag,at))!=std::string::npos;at+=tag.size()+21)base.insert(at+tag.size(),"disabled=\"disabled\" ");
    }out+=base;
    if(!authored_gallery()&&!ui_modal.empty())for(const auto& modal:visual.at("screens"))if(modal.at("id")==ui_modal&&modal.value("modal",false)){
      out+="<div id=\"ui-modal-shade\" style=\"z-index:9000;position:absolute;left:0;top:0;width:100%;height:100%;background-color:#000000aa;pointer-events:auto;\"></div><div id=\"ui-modal\" style=\"z-index:9001;position:absolute;left:0;top:0;width:100%;height:100%;\">";
      for(const auto& e:modal_elements)if(!e.contains("_collectionParent")&&!e.contains("_scrollParent"))out+=render_node(e);out+="</div>";break;
    }
    out+="<div id=\"ui-tooltip\" style=\"display:none;position:absolute;pointer-events:none;width:280px;padding:10px;background-color:#202838;color:#ffffff;border-width:1px;border-color:#b9cde8;z-index:10000;\"></div>";
    if(!authored_gallery()&&!pending_purchase.is_null())out+="<div class=\"transaction-shade\" style=\"z-index:10010;\"></div><div id=\"ui-confirmation\" class=\"purchase-dialog list\" style=\"z-index:10011;left:"+number(std::max(8,(width-540)/2))+"px;top:"+number(std::max(8,(height-440)/2))+"px;width:516px;\"><h3>"+escape(pending_confirmation.value("title","거래 확인"))+"</h3><p>"+escape(pending_confirmation.value("message","계속하시겠습니까?"))+"</p><p>"+escape(transaction_summary(pending_purchase))+"</p>"+button("confirm-purchase",pending_confirmation.value("confirmLabel","확인"),{{"type","confirmPurchase"}})+button("cancel-purchase",pending_confirmation.value("cancelLabel","취소"),{{"type","cancelPurchase"}})+"</div>";
    const auto localization=display_content.at("project").value("localization",Json::object());
    if(screen_id=="title"&&localization.value("locales",Json::array()).size()>1){std::string label=locale;for(const auto& l:localization.at("locales"))if(l.at("id")==locale)label=l.value("label",locale);out+="<div style=\"position:absolute;right:12px;bottom:12px;width:240px;z-index:200;\">"+button("engine-language","Language: "+label+" · F2",{{"type","nextLocale"}})+"</div>";}
    bool authored_notice=false;for(const auto& node:render_elements)if(node.value("binding","")=="player-notice"&&visible(node)&&(ui_modal.empty()?!node.contains("_authoredModal"):node.contains("_authoredModal")))authored_notice=true;
    if(!game.message.empty()&&!authored_notice)out+="<div class=\"status\">"+escape(game.message)+"</div>";
    if(!authored_gallery()){out+=timeline_overlay();out+=direction_overlay();}
    out+=gallery_overlay();
    if(extensions&&!authored_gallery())extensions->end_sync();
    return out+"</body></rml>";
  }
};






