  contentMenu->addAction(tr("사건 · 결말 목록 표 입력…"),this,[this]{
    if(bridge.busy()||projectData().isEmpty())return;
    const auto root=view.value("root"),revision=view.value("revision");QJsonArray edits;
    TableLogic::Context context(view.value("documents").toArray());
    const QStringList headers{tr("ID"),tr("제목"),tr("실행: 직접 / 반복 방문 / 주말 / 결말"),tr("시작 주 (1부터)"),tr("마지막 주"),tr("우선순위"),tr("조건 (; 모두, | 하나)" )};
    if(!readTabularInput(this,headers,[&](const QList<QStringList>& rows)->QString{
      edits={};if(rows.size()>50)return tr("한 번에 최대 50개 문서를 만드세요.");QSet<QString> seen;
      for(const auto &r:rows){if(r[2]!="결말")context.events.insert(r[0]);}
      for(const auto &r:rows){const auto id=r[0];if(!TableLogic::id(id)||seen.contains(id)||r[1].isEmpty())return tr("ID 중복·형식 또는 제목을 확인하세요: ")+id;seen.insert(id);
        if(!QStringList{"직접","반복 방문","주말","결말"}.contains(r[2]))return tr("실행은 직접 / 반복 방문 / 주말 / 결말입니다: ")+id;
        const bool ending=r[2]=="결말";bool ok1=false,ok2=false,ok3=false;int from=r[3].toInt(&ok1),through=r[4].toInt(&ok2),priority=r[5].toInt(&ok3);
        if(!ok3||priority<0||priority>100||(!ending&&(!ok1||!ok2||from<1||through<from||through>projectData().value("durationMonths").toInt()*projectData().value("periodsPerMonth").toInt())))return tr("주차 범위와 우선순위 0..100을 확인하세요: ")+id;
        QString error;auto condition=context.condition(r[6],error);if(!error.isEmpty())return id+": "+error;
        const auto file=(ending?"endings/":"events/")+id+".json";QJsonObject data;
        for(const auto &v:view.value("documents").toArray())if(v.toObject().value("file")==file)data=v.toObject().value("data").toObject();
        if(data.isEmpty()){edits.append(QJsonObject{{"type","add"},{"kind",ending?"ending":"event"},{"id",id}});data={{"formatVersion","0.1.0"},{"id",id},{"entryNodeId","end"},{"nodes",QJsonArray{QJsonObject{{"id","end"},{"type","end"}}}}};}
        data["label"]=r[1];data["priority"]=priority;data["requirements"]=condition;
        if(!ending){data["trigger"]=r[2]=="주말"?"period-end":"interaction";if(r[2]=="반복 방문")data["repeatable"]=true;else data.remove("repeatable");data["window"]=QJsonObject{{"fromPeriod",from-1},{"throughPeriod",through-1}};}
        edits.append(QJsonObject{{"type","replace"},{"file",file},{"data",data}});
      }return {};
    }))return;
    if(root!=view.value("root")||revision!=view.value("revision")){errorLabel->setText(tr("프로젝트가 변경되었습니다. 다시 입력하세요."));return;}
    request("edit",{{"edit",QJsonObject{{"type","batch"},{"edits",edits}}}});
  })->setObjectName("contentTable-narratives");
  contentMenu->addAction(tr("대사 · 선택 분기 표 입력…"),this,[this]{
    if(bridge.busy()||projectData().isEmpty())return;
    const auto root=view.value("root"),revision=view.value("revision");QJsonArray edits;
    TableLogic::Context context(view.value("documents").toArray());
    const QStringList headers{tr("사건/결말 ID"),tr("노드/선택 ID"),tr("종류: 대사/질문/선택/분기/효과/종료"),tr("문구"),tr("화자 ID / 선택의 질문 ID"),tr("조건"),tr("효과"),tr("다음 노드"),tr("거짓 노드")};
    if(!readTabularInput(this,headers,[&](const QList<QStringList>& rows)->QString{
      edits={};QMap<QString,QJsonObject> documents;QMap<QString,QJsonArray> nodes;QMap<QString,QSet<QString>> ids;QMap<QString,QString> entries,files;QMap<QString,QList<QPair<QString,QJsonObject>>> choices;
      for(const auto &r:rows){const auto docId=r[0],nodeId=r[1],type=r[2];
        if(!documents.contains(docId)){for(const auto &v:view.value("documents").toArray()){const auto d=v.toObject();if((d.value("kind")=="event"||d.value("kind")=="ending")&&d.value("data").toObject().value("id")==docId){documents[docId]=d.value("data").toObject();files[docId]=d.value("file").toString();}}if(!documents.contains(docId))return tr("먼저 사건·결말 목록을 만드세요: ")+docId;}
        if(!TableLogic::id(nodeId)||ids[docId].contains(nodeId))return tr("노드·선택 ID 중복 또는 형식 오류: ")+docId+"/"+nodeId;ids[docId].insert(nodeId);
        QString error;auto condition=context.condition(r[5],error);if(!error.isEmpty())return docId+"/"+nodeId+": "+error;auto effects=context.effects(r[6],error);if(!error.isEmpty())return docId+"/"+nodeId+": "+error;
        if(type!="선택"&&type!="분기"&&!TableLogic::blank(r[5]))return tr("조건은 선택 또는 분기 노드에서만 사용합니다: ")+nodeId;
        if(type!="선택"&&type!="효과"&&!TableLogic::blank(r[6]))return tr("효과는 선택 또는 효과 노드에서만 사용합니다: ")+nodeId;
        QJsonObject n{{"id",nodeId}};
        if(type=="대사"){if(r[3].isEmpty()||(!TableLogic::blank(r[4])&&!context.characters.contains(r[4])))return tr("문구 또는 화자 ID를 확인하세요: ")+nodeId;n["type"]="dialogue";n["speakerId"]=TableLogic::blank(r[4])?QJsonValue(QJsonValue::Null):QJsonValue(r[4]);n["text"]=r[3];n["next"]=r[7];}
        else if(type=="질문"){if(r[3].isEmpty())return tr("질문 문구가 필요합니다: ")+nodeId;n["type"]="choice";n["prompt"]=r[3];n["choices"]=QJsonArray{};}
        else if(type=="선택"){if(r[3].isEmpty()||TableLogic::blank(r[4]))return tr("선택 문구와 질문 ID가 필요합니다: ")+nodeId;n["text"]=r[3];n["requirements"]=condition;n["effects"]=effects;n["next"]=r[7];choices[docId].append({r[4],n});continue;}
        else if(type=="분기"){n["type"]="branch";n["condition"]=condition;n["then"]=r[7];n["else"]=r[8];}
        else if(type=="효과"){n["type"]="effect";n["effects"]=effects;n["next"]=r[7];}
        else if(type=="종료")n["type"]="end";
        else return tr("알 수 없는 노드 종류: ")+type;
        if(!entries.contains(docId))entries[docId]=nodeId;nodes[docId].append(n);
      }
      if(documents.size()>50)return tr("한 번에 최대 50개 문서까지 입력하세요.");
      for(auto it=documents.begin();it!=documents.end();++it){const auto doc=it.key();auto array=nodes[doc];
        for(const auto &choice:choices[doc]){bool found=false;for(int i=0;i<array.size();++i){auto n=array[i].toObject();if(n.value("id")==choice.first&&n.value("type")=="choice"){auto c=n.value("choices").toArray();c.append(choice.second);n["choices"]=c;array[i]=n;found=true;break;}}if(!found)return tr("선택의 질문 노드가 없습니다: ")+doc+"/"+choice.first;}
        QMap<QString,QStringList> edges;for(const auto &v:array){const auto n=v.toObject();QStringList next;const auto type=n.value("type").toString();if(type=="choice"){const auto options=n.value("choices").toArray();if(options.isEmpty())return tr("선택 없는 질문: ")+doc+"/"+n.value("id").toString();for(const auto &c:options)next.append(c.toObject().value("next").toString());}else if(type=="branch")next={n.value("then").toString(),n.value("else").toString()};else if(type!="end")next={n.value("next").toString()};edges[n.value("id").toString()]=next;}
        for(auto e=edges.cbegin();e!=edges.cend();++e)for(const auto &target:e.value())if(!edges.contains(target))return tr("연결 대상 노드가 없습니다: ")+doc+"/"+target;
        QSet<QString> visiting,visited;std::function<bool(QString)> visit=[&](QString node){if(visiting.contains(node))return false;if(visited.contains(node))return true;visiting.insert(node);for(const auto &next:edges[node])if(!visit(next))return false;visiting.remove(node);visited.insert(node);return true;};
        for(const auto &node:edges.keys())if(!visit(node))return tr("대본 순환을 제거하세요: ")+doc+"/"+node;
        if(entries[doc].isEmpty())return tr("대본에 노드가 필요합니다: ")+doc;
        it.value()["entryNodeId"]=entries[doc];it.value()["nodes"]=array;edits.append(QJsonObject{{"type","replace"},{"file",files[doc]},{"data",it.value()}});
      }return {};
    }))return;
    if(root!=view.value("root")||revision!=view.value("revision")){errorLabel->setText(tr("프로젝트가 변경되었습니다. 다시 입력하세요."));return;}
    if(QMessageBox::question(this,tr("대본 교체 확인"),tr("표에 포함된 사건·결말의 기존 대본 전체를 교체합니다. 첫 노드부터 시작하며 표에 없는 기존 노드는 제거됩니다. 계속할까요?"))!=QMessageBox::Yes)return;
    request("edit",{{"edit",QJsonObject{{"type","batch"},{"edits",edits}}}});
  })->setObjectName("contentTable-scripts");
