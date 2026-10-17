  // Spreadsheet entry uses the same edit batch as individual content forms.
  for(const auto &kind:QStringList{"character","activity"}) {
    auto title=kind=="character"?tr("인물 · 관계 표 입력…"):tr("활동 · 비용 / 효과 표 입력…");
    contentMenu->addAction(title,this,[this,kind]{
      if(bridge.busy()||projectData().isEmpty())return;
      const auto root=view.value("root"),revision=view.value("revision");
      const QStringList headers=kind=="character"?QStringList{tr("ID"),tr("이름"),tr("설명"),tr("관계 ID (없으면 -)"),tr("관계 이름"),tr("초기 관계")}:QStringList{tr("ID"),tr("이름"),tr("설명"),tr("비용: ID=수량;… (없으면 -)"),tr("증감 효과: ID=정수;… (없으면 -)"),tr("조건 (; 모두, | 하나)")};
      QJsonArray edits;
      if(!readTabularInput(this,headers,[&](const QList<QStringList>& rows)->QString{
        edits={};QSet<QString> seen;if(rows.size()>50)return tr("한 번에 최대 50개 문서를 입력하세요.");TableLogic::Context logic(view.value("documents").toArray());
        QMap<QString,QString> attributes;QSet<QString> flags;
        for(auto value:view.value("documents").toArray()){const auto doc=value.toObject(),data=doc.value("data").toObject();if(doc.value("kind")=="attributes")for(auto a:data.value("attributes").toArray())attributes[a.toObject().value("id").toString()]=a.toObject().value("kind").toString();if(doc.value("kind")=="flags")for(auto f:data.value("flags").toArray())flags.insert(f.toObject().value("id").toString());}
        auto valid=[](const QString &id){return id.size()<=64&&QRegularExpression("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$").match(id).hasMatch();};
        for(const auto &row:rows){const auto id=row[0];if(!valid(id)||seen.contains(id)||row[1].isEmpty())return tr("ID 중복·형식 또는 이름을 확인하세요: ")+id;seen.insert(id);
          QString file=(kind=="character"?"characters/":"activities/")+id+".json";QJsonObject data;
          for(auto value:view.value("documents").toArray())if(value.toObject().value("file")==file)data=value.toObject().value("data").toObject();
          if(data.isEmpty()){edits.append(QJsonObject{{"type","add"},{"kind",kind},{"id",id}});data=QJsonObject{{"formatVersion","0.1.0"},{"id",id}};}
          data[kind=="character"?"name":"label"]=row[1];data["description"]=row[2];
          if(kind=="character"){
            if(!data.contains("portraits"))data["portraits"]=QJsonObject{};
            auto relations=data.value("relationships").toArray();
            if(row[3]!="-"&&!row[3].isEmpty()){bool ok=false;int initial=row[5].toInt(&ok);if(!valid(row[3])||row[4].isEmpty()||!ok||initial<0||initial>100)return tr("관계 ID·이름·초기값 0..100을 확인하세요: ")+id;QJsonObject relation{{"id",row[3]},{"label",row[4]},{"initial",initial},{"min",0},{"max",100}};int found=-1;for(int i=0;i<relations.size();++i)if(relations[i].toObject().value("id")==row[3]){found=i;relation=relations[i].toObject();relation["label"]=row[4];relation["initial"]=initial;break;}if(found<0)relations.append(relation);else relations[found]=relation;}
            data["relationships"]=relations;
          }else{
            QString problem;data["requirements"]=logic.condition(row[5],problem);if(!problem.isEmpty())return id+": "+problem;
            data["costs"]=logic.costs(row[3],problem);if(!problem.isEmpty())return id+": "+problem;
            data["baseEffects"]=logic.effects(row[4],problem);if(!problem.isEmpty())return id+": "+problem;
            edits.append(QJsonObject{{"type","replace"},{"file",file},{"data",data}});continue;
          }
          edits.append(QJsonObject{{"type","replace"},{"file",file},{"data",data}});
        }
        return {};
      }))return;
      if(root!=view.value("root")||revision!=view.value("revision")){errorLabel->setText(tr("프로젝트가 변경되었습니다. 최신 내용을 확인한 뒤 다시 입력하세요."));return;}
      request("edit",{{"edit",QJsonObject{{"type","batch"},{"edits",edits}}}});
    })->setObjectName("contentTable-"+kind);
  }
