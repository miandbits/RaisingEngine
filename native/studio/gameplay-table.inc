  auto gameplayTables=contentMenu->addMenu(tr("장소 · 물품 · 행동 표 입력"));
  for(const auto &section:QStringList{"locations","items","actions","policyGroups","rules"}) {
    const QMap<QString,QString> names{{"locations",tr("장소")},{"items",tr("물품")},{"actions",tr("행동")},{"policyGroups",tr("생활 방침")},{"rules",tr("실행 시점 규칙")}};
    gameplayTables->addAction(names[section],this,[this,section]{
      if(bridge.busy()||projectData().isEmpty())return;
      const auto root=view.value("root"),revision=view.value("revision");auto project=projectData();auto gameplay=project.value("gameplay").toObject();
      for(const auto &key:QStringList{"items","shops","interactions"})if(!gameplay.contains(key))gameplay[key]=QJsonArray{};
      TableLogic::Context context(view.value("documents").toArray());QStringList headers;
      if(section=="locations")headers={tr("ID"),tr("이름"),tr("화면 ID (없으면 -)"),tr("연결 장소 ID (; 구분)"),tr("조건")};
      if(section=="items")headers={tr("ID"),tr("이름"),tr("설명"),tr("최대 수량"),tr("초기 수량")};
      if(section=="actions")headers={tr("ID"),tr("이름"),tr("비용"),tr("조건"),tr("효과"),tr("사건 ID"),tr("탐험 ID"),tr("확인창: 켜짐 / 꺼짐")};
      if(section=="policyGroups")headers={tr("방침 ID"),tr("방침 이름"),tr("선택 ID"),tr("선택 이름"),tr("행동 ID"),tr("시점: 활동 전 / 활동 후")};
      if(section=="rules")headers={tr("ID"),tr("이름"),tr("시점: 주 시작 / 주 종료 / 월 시작 / 월 종료"),tr("행동 ID"),tr("조건")};
      if(!readTabularInput(this,headers,[&](const QList<QStringList>& rows)->QString{
        auto entries=gameplay.value(section).toArray();QSet<QString> seen,knownActions,knownLocations,knownScreens,knownExpeditions;
        for(const auto &a:gameplay.value("actions").toArray())knownActions.insert(a.toObject().value("id").toString());
        for(const auto &a:gameplay.value("locations").toArray())knownLocations.insert(a.toObject().value("id").toString());
        for(const auto &a:project.value("visual").toObject().value("screens").toArray())knownScreens.insert(a.toObject().value("id").toString());
        for(const auto &a:project.value("systems").toObject().value("expeditions").toArray())knownExpeditions.insert(a.toObject().value("id").toString());
        if(section=="locations")for(const auto &r:rows)knownLocations.insert(r[0]);
        if(section!="locations"&&knownLocations.isEmpty())return tr("먼저 장소 표에서 첫 장소를 만드세요.");
        QMap<QString,QJsonObject> changed;
        for(auto r:rows){if(section=="actions")r[7]=TableLogic::keyword(r[7]);if(section=="policyGroups")r[5]=TableLogic::keyword(r[5]);if(section=="rules"){r[2]=TableLogic::keyword(r[2]);if(r[2]=="주말")r[2]="주 종료";}const auto id=r[0];const auto unique=section=="policyGroups"?id+"/"+r[2]:id;
          if(!TableLogic::id(id)||r[1].isEmpty()||seen.contains(unique))return tr("ID 중복·형식 또는 이름을 확인하세요: ")+id;seen.insert(unique);QJsonObject data;
          for(const auto &v:entries)if(v.toObject().value("id")==id)data=v.toObject();if(changed.contains(id))data=changed[id];data["id"]=id;data["name"]=r[1];QString error;
          if(section=="locations") {
            QJsonArray connections;if(!TableLogic::blank(r[3]))for(const auto &key:r[3].split(';')){if(!knownLocations.contains(key.trimmed()))return tr("연결 장소가 없습니다: ")+key;if(!connections.contains(key.trimmed()))connections.append(key.trimmed());}
            data["connections"]=connections;data["requirements"]=context.condition(r[4],error);
            if(TableLogic::blank(r[2]))data.remove("screenId");else if(!knownScreens.contains(r[2]))return tr("화면을 먼저 만드세요: ")+r[2];else data["screenId"]=r[2];
          } else if(section=="items") {
            bool a=false,b=false;const int maximum=r[3].toInt(&a),initial=r[4].toInt(&b);if(!a||!b||maximum<1||initial<0||initial>maximum)return tr("물품의 최대·초기 수량을 확인하세요: ")+id;data["description"]=r[2];data["maxStack"]=maximum;data["initialQuantity"]=initial;
          } else if(section=="actions") {
            data["costs"]=context.costs(r[2],error);if(!error.isEmpty())return id+": "+error;data["requirements"]=context.condition(r[3],error);if(!error.isEmpty())return id+": "+error;data["effects"]=context.effects(r[4],error);
            if(TableLogic::blank(r[5]))data.remove("eventId");else if(!context.events.contains(r[5]))return tr("사건 ID가 없습니다: ")+r[5];else data["eventId"]=r[5];
            if(TableLogic::blank(r[6]))data.remove("expeditionId");else if(!knownExpeditions.contains(r[6]))return tr("탐험을 먼저 만드세요: ")+r[6];else data["expeditionId"]=r[6];
            if(r[7]!="켜짐"&&r[7]!="꺼짐")return tr("확인창은 켜짐 또는 꺼짐입니다: ")+id;
            auto confirmation=data.value("confirmation").toObject();confirmation["enabled"]=r[7]=="켜짐";data["confirmation"]=confirmation;
          } else if(section=="policyGroups") {
            if(!TableLogic::id(r[2])||r[3].isEmpty()||!knownActions.contains(r[4]))return tr("방침 선택 ID·이름·행동을 확인하세요: ")+id;
            if(r[5]!="활동 전"&&r[5]!="활동 후")return tr("방침 시점은 활동 전 또는 활동 후입니다: ")+id;
            auto options=data.value("options").toArray();QJsonObject option{{"id",r[2]},{"name",r[3]},{"actionId",r[4]}};int found=-1;for(int i=0;i<options.size();++i)if(options[i].toObject().value("id")==r[2]){option=options[i].toObject();option["name"]=r[3];option["actionId"]=r[4];found=i;break;}if(found<0)options.append(option);else options[found]=option;data["options"]=options;data["trigger"]=QJsonObject{{"type",r[5]=="활동 전"?"beforeActivity":"afterActivity"}};
          } else {
            const QMap<QString,QString> triggers{{"주 시작","periodStart"},{"주 종료","periodEnd"},{"월 시작","monthStart"},{"월 종료","monthEnd"}};
            if(!triggers.contains(r[2])||!knownActions.contains(r[3]))return tr("실행 시점 또는 행동 ID를 확인하세요: ")+id;
            data["trigger"]=QJsonObject{{"type",triggers[r[2]]}};data["actionId"]=r[3];data["condition"]=context.condition(r[4],error);
          }
          if(!error.isEmpty())return id+": "+error;changed[id]=data;
        }
        for(auto it=changed.cbegin();it!=changed.cend();++it){int found=-1;for(int i=0;i<entries.size();++i)if(entries[i].toObject().value("id")==it.key()){found=i;break;}if(found<0)entries.append(it.value());else entries[found]=it.value();}
        gameplay[section]=entries;if(section=="locations"&&!gameplay.contains("startLocationId"))gameplay["startLocationId"]=rows.front()[0];return {};
      }))return;
      if(root!=view.value("root")||revision!=view.value("revision")){errorLabel->setText(tr("프로젝트가 변경되었습니다. 다시 입력하세요."));return;}
      project["gameplay"]=gameplay;request("edit",{{"edit",QJsonObject{{"type","replace"},{"file","project.json"},{"data",project}}}});
    })->setObjectName("contentTable-"+section);
  }
