    // The character definition and playable actor are distinct documents, but
    // designers should be able to follow either direction without knowing IDs.
    auto jump=[&](QString caption,QString key,QString objectName){
      auto b=new QPushButton(caption);b->setObjectName(objectName);form->addRow(b);
      connect(b,&QPushButton::clicked,panel,[=]{if(state->dirty){notice->setText(tr("현재 입력을 먼저 적용하거나 취소하세요."));return;}locate(key);});
    };
    if(kind=="character"){
      const auto id=n.value("id").toString();
      for(auto v:state->nodes){auto a=v.toObject();if(a.value("kind")=="actor"&&a.value("id")==id)
        jump(tr("이 인물의 활동 배정·개별 성장 편집"),a.value("key").toString(),"workspaceActor");}
    }
    if(kind=="actor")jump(tr("인물 이름·관계 정의 편집"),"character:"+n.value("id").toString(),"workspaceCharacter");
    if(kind=="actor"&&!state->draft.value("activityId").toString().isEmpty())
      jump(tr("배정 활동의 조건·비용·효과 편집"),"activity:"+state->draft.value("activityId").toString(),"workspaceAssignedActivity");
