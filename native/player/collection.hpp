// Included in ScreenMarkup: data-bound rows use the authored component primitives.
  Json collection_rows(const Json& collection) const {
    Json rows=Json::array();const auto source=collection.at("source");const auto type=source.value("type","");
    const auto gp=gameplay(),config=display_content.at("project").value("gameplay",Json::object());
    auto append=[&](std::string id,Json values,bool enabled=true,std::string reason="") {
      values["id"]=id;rows.push_back({{"id",id},{"values",values},{"enabled",enabled},{"reason",reason}});
    };
    if(type=="galleryBranches")rows=gallery_branch_rows();
    else if(type=="galleryEntries"||type=="gallerySelection")rows=gallery_rows(type=="gallerySelection");
    if(type.rfind("battle",0)==0)rows=battle_collection_rows(type);
    if(type=="shopOffers"&&!gp.is_null())for(const auto& shop:gp.value("shops",Json::array())) {
      const auto shopId=shop.value("id","");if(source.contains("shopId")&&source.at("shopId")!=shopId)continue;
      for(const auto& offer:shop.at("offers")) {
        const auto itemId=offer.value("itemId","");Json definition=Json::object();for(const auto& item:config.value("items",Json::array()))if(item.value("id","")==itemId)definition=item;
        auto reason=offer.value("reason",Json()).is_string()?offer.at("reason").get<std::string>():"";
        append(shopId+"--"+itemId,{{"name",offer.at("name")},{"shopId",shopId},{"itemId",itemId},{"price",offer.at("price")},
          {"stock",offer.at("stock").is_null()?Json(-1):offer.at("stock")},{"owned",offer.at("owned")},{"maxQuantity",offer.at("maxQuantity")},
          {"imageAssetId",definition.value("imageAssetId","")},{"description",definition.value("description","")},
          {"enabled",offer.at("enabled")},{"reason",reason},{"action","buy"},{"quantity",1}},offer.value("enabled",false),reason);
      }
    }
    if(type=="policyOptions"&&!gp.is_null())for(const auto& group:gp.value("policyGroups",Json::array())) {
      const auto groupId=group.value("id","");if(source.contains("groupId")&&source.at("groupId")!=groupId)continue;
      for(const auto& option:group.at("options")) {
        std::string costs;for(const auto& cost:option.at("costs")){if(!costs.empty())costs+=", ";costs+=attribute_name(cost.value("attributeId",""))+": "+number(cost.value("amount",0.0));}
        auto reason=option.value("reason",Json()).is_string()?option.at("reason").get<std::string>():"";
        append(groupId+"--"+option.value("id",""),{{"name",option.at("name")},{"groupName",group.at("name")},{"groupId",groupId},
          {"optionId",option.at("id")},{"actionId",option.at("actionId")},{"selected",option.at("selected")},{"enabled",option.at("enabled")},
          {"reason",reason},{"action","selectPolicy"},{"costsText",costs}},option.value("enabled",false),reason);
      }
    }
    if(type=="relationships"&&!game.state.is_null())for(const auto& character:display_content.at("characters")) {
      const auto characterId=character.value("id","");if(source.contains("characterId")&&source.at("characterId")!=characterId)continue;
      for(const auto& relation:character.at("relationships")) {
        const auto relationId=relation.value("id","");
        append(characterId+"--"+relationId,{{"name",character.at("name")},{"characterId",characterId},{"relationshipId",relationId},
          {"relationshipName",relation.value("label",relationId)},{"value",game.state.at("relationships").at(characterId).at(relationId)},
          {"min",relation.at("min")},{"max",relation.at("max")},{"enabled",true},{"reason",""}});
      }
    }
    if(type=="table")for(const auto& table:display_content.at("project").at("visual").value("dataTables",Json::array()))
      if(table.at("id")==source.at("tableId"))for(const auto& row:table.at("rows"))append(row.at("id"),row.at("values"));
    Json filtered=Json::array();
    for(const auto& row:rows) {
      if(collection.contains("rowFilter")) {
        const auto filter=collection.at("rowFilter");const auto actual=row.at("values").value(filter.value("field",""),Json()),expected=filter.at("value");
        const auto op=filter.value("operator","eq");bool matches=op=="eq"?actual==expected:op=="ne"?actual!=expected:false;
        if(actual.is_number()&&expected.is_number()){const double a=actual,b=expected;if(op=="gt")matches=a>b;if(op=="gte")matches=a>=b;if(op=="lt")matches=a<b;if(op=="lte")matches=a<=b;}
        if(!matches)continue;
      }
      filtered.push_back(row);if(filtered.size()==(type=="galleryEntries"?2000:1000))break;
    }
    if(collection.contains("sort")){const auto sort=collection.at("sort");const auto field=sort.value("field","");const bool desc=sort.value("direction","asc")=="desc";std::stable_sort(filtered.begin(),filtered.end(),[&](const Json& a,const Json& b){const auto x=a.at("values").value(field,Json()),y=b.at("values").value(field,Json());if(x==y)return false;if(x.is_null())return false;if(y.is_null())return true;return desc?y<x:x<y;});}
    return filtered;
  }
  Json expanded_collections(Json elements) const {
    Json result=Json::array();const auto visual=display_content.at("project").at("visual");
    for(auto element:elements) {
      if(element.value("kind","")!="collection"){result.push_back(element);continue;}
      Json definition=Json::object();for(const auto& component:visual.value("components",Json::array()))if(component.at("id")==element.at("componentId"))definition=component;
      if(definition.empty())throw std::runtime_error("Unknown collection component");
      const auto rows=collection_rows(element);const int columns=element.value("layout","list")=="grid"?std::max(1,element.value("columns",2)):1;
      const double gap=element.value("gap",8.0),rowHeight=element.value("rowHeight",definition.value("height",100.0));
      const double cardWidth=std::max(16.0,(element.value("width",300.0)-16-gap*(columns-1))/columns);
      element["_collectionExtent"]=rows.empty()?0:std::ceil(double(rows.size())/columns)*(rowHeight+gap)-gap;
      element["_collectionCount"]=rows.size();result.push_back(element);
      size_t index=0;for(const auto& row:rows) {
        auto children=expand_ui_components(visual,hierarchy_elements(definition.at("elements"),definition.value("width",cardWidth),definition.value("height",rowHeight),cardWidth,rowHeight,phase(),element.value("uiContext",Json::object())),phase());for(auto& child:children) {
          const auto childId=child.value("id","");
          for(const auto& binding:element.value("bindings",Json::array()))if(binding.at("elementId")==childId) {
            const auto property=binding.value("property","");if(!row.at("values").contains(binding.value("field","")))continue;auto value=row.at("values").at(binding.value("field",""));
            if((property=="text"||property=="label")&&binding.contains("format")) {
              std::string text=value.is_string()?value.get<std::string>():value.is_null()?"":value.dump(),format=binding.at("format");
              size_t at=0;while((at=format.find("{value}",at))!=std::string::npos){format.replace(at,7,text);at+=text.size();}value=format;
            }
            if(property=="visible")child["hidden"]=child.value("hidden",false)||(value.is_boolean()&&!value.get<bool>());
            else if(property=="enabled")child["_boundEnabled"]=child.value("_boundEnabled",true)&&value.is_boolean()&&value.get<bool>();
            else {if((property=="text"||property=="label")&&!value.is_string())value=value.dump();child[property]=value;if(property=="label")child["text"]=value;}
          }
        }
        for(auto& e:children)for(const auto& id:e.value("_groupIds",Json::array()))for(const auto& parent:children)if(parent.at("id")==id){if(parent.value("hidden",false))e["hidden"]=true;if(!parent.value("_boundEnabled",true))e["_boundEnabled"]=false;}
        for(auto child:children) {
          const auto childId=child.value("id","");
          child["id"]=element.value("id","")+"__"+row.value("id","")+"__"+childId;
          child["_collectionParent"]=element.at("id");child["_rowId"]=row.at("id");
          if(child.contains("_scrollParent"))child["_scrollParent"]=element.value("id","")+"__"+row.value("id","")+"__"+child.at("_scrollParent").get<std::string>();
          if(element.contains("selectionKey")){child["_selectionSource"]=element;child["selectionKey"]=element.at("selectionKey");}
          child["x"]=child.value("x",0.0)+(index%columns)*(cardWidth+gap);child["y"]=child.value("y",0.0)+(index/columns)*(rowHeight+gap);
          if(child.contains("_clipRect")){child["_clipRect"]["x"]=child["_clipRect"].value("x",0.0)+(index%columns)*(cardWidth+gap);child["_clipRect"]["y"]=child["_clipRect"].value("y",0.0)+(index/columns)*(rowHeight+gap);}
          child["hidden"]=child.value("hidden",false)||!visible(element);
          child["_boundEnabled"]=child.value("_boundEnabled",true)&&row.value("enabled",true);
          child["_rowReason"]=row.value("reason","");
          for(const auto& key:{"visibleCondition","enabledCondition"})if(element.contains(key))child[key]=child.contains(key)?Json{{"op","all"},{"conditions",Json::array({element.at(key),child.at(key)})}}:element.at(key);
          result.push_back(child);
        }
        ++index;
      }
    }
    return result;
  }
