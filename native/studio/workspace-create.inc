    if(kind=="stateGroup"){editCustomStates();return;}
    if(kind=="screen"){createScreen();return;}
    if(kind=="attribute"||kind=="flag"||kind=="activity"){
      const auto expectedRevision=view.value("revision"),expectedRoot=view.value("root");
      bool ok=false;const auto name=QInputDialog::getText(this,tr("새 ")+workspaceKind(kind),tr("이름"),QLineEdit::Normal,{},&ok).trimmed();
      if(!ok||name.isEmpty())return;
      if(expectedRevision!=view.value("revision")||expectedRoot!=view.value("root")){notice->setText(tr("프로젝트가 변경되었습니다. 다시 생성하세요."));return;}
      const auto id=kind+"-"+QUuid::createUuid().toString(QUuid::Id128).left(12);state->selected={{"key",kind+":"+id}};
      if(kind=="activity"){
        const auto file="activities/"+id+".json";
        const QJsonObject value{{"formatVersion","0.1.0"},{"id",id},{"label",name},{"requirements",QJsonObject{{"op","always"}}},{"costs",QJsonArray{}},{"baseEffects",QJsonArray{}}};
        const QJsonArray edits{QJsonObject{{"type","add"},{"kind","activity"},{"id",id}},QJsonObject{{"type","replace"},{"file",file},{"data",value}}};
        request("edit",{{"edit",QJsonObject{{"type","batch"},{"edits",edits}}}});return;
      }
      const auto plural=kind=="attribute"?QString("attributes"):QString("flags");
      for(const auto& v:view.value("documents").toArray()){const auto doc=v.toObject();if(doc.value("kind")!=plural)continue;auto data=doc.value("data").toObject();auto rows=data.value(plural).toArray();QJsonObject value{{"id",id},{"label",name},{"initial",kind=="attribute"?QJsonValue(0):QJsonValue(false)}};if(kind=="attribute"){value["min"]=0;value["max"]=100;}
        if(kind=="attribute")value["kind"]="stat";
        rows.append(value);data[plural]=rows;request("edit",{{"edit",QJsonObject{{"type","replace"},{"file",doc.value("file")},{"data",data}}}});return;
      }
      notice->setText(tr("필수 데이터 문서가 없습니다. 프로젝트 검증에서 복구하세요."));return;
    }
