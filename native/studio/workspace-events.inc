    if(kind=="event"){
      auto trigger=new QComboBox;trigger->setObjectName("workspaceEventTrigger");
      for(const auto &p:QList<QPair<QString,QString>>{{"period-end",tr("일정 종료")},{"period-start",tr("일정 시작")},{"after-activity",tr("지정 활동 완료")},{"location-enter",tr("장소 입장")},{"after-event",tr("사건 완료 후")},{"interaction",tr("행동에서 실행")}})trigger->addItem(p.second,p.first);
      const auto previous=state->draft.value("trigger").toString("period-end");if(trigger->findData(previous)<0)trigger->addItem(previous,previous);trigger->setCurrentIndex(trigger->findData(previous));
      form->addRow(tr("발생 시점"),trigger);state->fields["trigger"]=[trigger]{return trigger->currentData().toString();};connect(trigger,qOverload<int>(&QComboBox::currentIndexChanged),panel,dirty);
      choice("activityId",tr("연결 활동"),"activity",true);choice("locationId",tr("연결 장소"),"location",true);
      auto activity=properties->findChild<QComboBox*>("workspaceField-activityId"),location=properties->findChild<QComboBox*>("workspaceField-locationId");
      auto timing=[=]{activity->setEnabled(trigger->currentData()=="after-activity");location->setEnabled(trigger->currentData()=="location-enter");};connect(trigger,qOverload<int>(&QComboBox::currentIndexChanged),panel,timing);timing();
      auto scope=new QComboBox;scope->setObjectName("workspaceEventScope");scope->addItem(tr("공용 · 모두가 같은 사건 기록"),"shared");scope->addItem(tr("캐릭터별 · 실행한 인물의 선택·약속 기록"),"actor");scope->setCurrentIndex(state->draft.value("scope")=="actor"?1:0);form->addRow(tr("기록 범위"),scope);
      state->fields["scope"]=[scope]{return scope->currentData().toString();};connect(scope,qOverload<int>(&QComboBox::currentIndexChanged),panel,dirty);
      auto help=new QLabel(tr("공용 → 캐릭터별 전환은 조건·효과·선택·후속 사건의 변경 내용을 먼저 보여줍니다. 공동 자원과 특정 인물 대상은 구분하여 유지합니다."));help->setWordWrap(true);form->addRow(help);
      if(n.value("value").toObject().value("scope")!="actor"){
        auto preview=new QPushButton(tr("캐릭터별 전환 미리보기…"));preview->setObjectName("workspaceScopePreview");form->addRow(preview);
        connect(preview,&QPushButton::clicked,panel,[=]{
          if(bridge.busy()||state->revision!=view.value("revision").toInt()||state->root!=view.value("root").toString()){notice->setText(tr("프로젝트 버전이 바뀌었습니다. 새로 고침 후 다시 편집하세요."));return;}
          collect();auto draft=state->draft;const auto trigger=draft.value("trigger").toString("period-end");if(trigger!="after-activity")draft.remove("activityId");if(trigger!="location-enter")draft.remove("locationId");
          const auto docs=view.value("documents").toArray();const auto revision=view.value("revision"),root=view.value("root");
          auto editor=new StoryScopeDialog(docs,n.value("file").toString(),draft);
          embed(editor,n.value("label").toString()+tr(" · 캐릭터별 전환"),[=]{
            if(bridge.busy()||revision!=view.value("revision")||root!=view.value("root")||docs!=view.value("documents").toArray()){editor->findChild<QLabel*>("scopeProblems")->setText(tr("프로젝트가 변경되었습니다. 취소하고 다시 연결하세요."));return false;}
            if(!editor->resultPlan.errors.isEmpty())return false;QJsonArray edits;
            for(auto v:editor->resultPlan.documents){auto d=v.toObject();for(auto old:docs)if(old.toObject().value("file")==d.value("file")&&old.toObject().value("data")!=d.value("data"))edits.append(QJsonObject{{"type","replace"},{"file",d.value("file")},{"data",d.value("data")}});}
            if(edits.isEmpty())return true;
            editor->setEnabled(false);request("edit",{{"edit",QJsonObject{{"type","batch"},{"edits",edits}}}},[=](QJsonObject r){editor->setEnabled(true);if(!r.value("error").toString().isEmpty()){editor->findChild<QLabel*>("scopeProblems")->setText(r.value("error").toString());return;}
              state->dirty=false;state->pending=n.value("key").toString();inlineEditor->complete();emit updated();
            });return false;
          });
        });
      }
      const auto p=projectData();const int per=qMax(1,p.value("periodsPerMonth").toInt()),total=qMax(1,p.value("durationMonths").toInt()*per);
      for(const auto &field:QStringList{"fromPeriod","throughPeriod"}){
        auto date=new QComboBox;date->setObjectName("workspaceDate-"+field);
        for(int i=0;i<total;++i)date->addItem(StudioCalendar::label(p,i),i);
        const int previous=state->draft.value("window").toObject().value(field).toInt();if(date->findData(previous)<0)date->addItem(tr("범위 밖 일정: %1").arg(previous+1),previous);date->setCurrentIndex(date->findData(previous));
        form->addRow(field=="fromPeriod"?tr("발생 시작 날짜"):tr("발생 마지막 날짜"),date);state->fields["window/"+field]=[date]{return date->currentData().toInt();};connect(date,qOverload<int>(&QComboBox::currentIndexChanged),panel,dirty);
      }
    }
    if(kind=="scriptNode"&&state->draft.value("type")=="dialogue"){
      auto speaker=new QComboBox;speaker->setObjectName("workspaceSpeaker");speaker->addItem(tr("해설"),QString());
      for(auto v:state->nodes){auto c=v.toObject();if(c.value("kind")=="character")speaker->addItem(c.value("label").toString(),c.value("id").toString());}
      const auto previous=state->draft.value("speakerId").toString();if(speaker->findData(previous)<0)speaker->addItem(tr("없는 인물: ")+previous,previous);speaker->setCurrentIndex(speaker->findData(previous));
      form->addRow(tr("말하는 인물"),speaker);state->fields["speakerId"]=[speaker]{return speaker->currentData().toString().isEmpty()?QJsonValue(QJsonValue::Null):QJsonValue(speaker->currentData().toString());};connect(speaker,qOverload<int>(&QComboBox::currentIndexChanged),panel,dirty);
    }
    if(kind=="event"||kind=="scriptChoice"){
      auto b=new QPushButton(tr("＋ 이 사건·선택에서 후속 사건 연결"));b->setObjectName("workspaceFollowup");form->addRow(b);
      connect(b,&QPushButton::clicked,panel,[=]{
        if(state->dirty){notice->setText(tr("선행 내용의 변경을 먼저 적용한 뒤 후속 사건을 연결하세요."));return;}
        const auto docs=view.value("documents").toArray();const auto model=buildStoryConnections(docs);QString source;
        for(auto it=model.nodes.begin();it!=model.nodes.end();++it){const auto &candidate=it.value();if(candidate.file!=n.value("file").toString())continue;
          if(kind=="event"&&candidate.kind=="event")source=it.key();
          if(kind=="scriptChoice"&&candidate.kind=="choice"){const auto ids=n.value("id").toString().split('/');if(candidate.scriptNode==ids.value(2)&&candidate.choice==ids.value(3))source=it.key();}
        }
        if(source.isEmpty()){notice->setText(tr("연결할 선행 사건·선택을 찾지 못했습니다."));return;}
        auto editor=new StoryFollowupDialog(docs,source,nullptr);const auto revision=view.value("revision"),root=view.value("root");
        embed(editor,n.value("label").toString()+tr(" → 후속 사건"),[=]{
          if(bridge.busy()||revision!=view.value("revision")||root!=view.value("root")){notice->setText(tr("프로젝트가 변경되었습니다. 취소하고 다시 연결하세요."));return false;}
          QJsonArray edits;QString id;
          for(auto v:editor->resultPlan.documents){auto d=v.toObject();QJsonObject old;for(auto before:docs)if(before.toObject().value("file")==d.value("file"))old=before.toObject();if(d.value("file")==editor->resultPlan.file)id=d.value("data").toObject().value("id").toString();if(old.value("data")==d.value("data"))continue;
            if(old.empty())edits.append(QJsonObject{{"type","add"},{"kind",d.value("kind")},{"id",d.value("data").toObject().value("id")}});
            edits.append(QJsonObject{{"type","replace"},{"file",d.value("file")},{"data",d.value("data")}});
          }
          editor->setEnabled(false);request("edit",{{"edit",QJsonObject{{"type","batch"},{"edits",edits}}}},[=](QJsonObject r){editor->setEnabled(true);if(!r.value("error").toString().isEmpty()){editor->findChild<QLabel*>("followupError")->setText(r.value("error").toString());return;}state->pending="event:"+id;inlineEditor->complete();emit updated();});return false;
        });
      });
    }
