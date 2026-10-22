    if(kind=="event"||kind=="ending"){
      auto contents=new QTreeWidget;contents->setObjectName("workspaceStoryContents");contents->setHeaderLabel(tr("대사·선택 · 두 번 눌러 여기서 편집"));contents->setMaximumHeight(210);
      QMap<QString,QTreeWidgetItem*> byNode;
      for(auto v:state->nodes){auto row=v.toObject();if(row.value("file")!=n.value("file")||row.value("kind")!="scriptNode")continue;auto item=new QTreeWidgetItem(contents,{row.value("label").toString()});item->setData(0,Qt::UserRole,row.value("key").toString());byNode[row.value("id").toString()]=item;}
      for(auto v:state->nodes){auto row=v.toObject();if(row.value("file")!=n.value("file")||row.value("kind")!="scriptChoice")continue;auto key=row.value("id").toString();auto parent=byNode.value(key.left(key.lastIndexOf('/')));if(!parent)continue;auto item=new QTreeWidgetItem(parent,{row.value("value").toObject().value("text").toString()});item->setData(0,Qt::UserRole,row.value("key").toString());parent->setExpanded(true);}
      form->addRow(contents);connect(contents,&QTreeWidget::itemDoubleClicked,panel,[=](QTreeWidgetItem *item,int){if(state->dirty){notice->setText(tr("현재 입력을 먼저 적용하세요."));return;}locate(item->data(0,Qt::UserRole).toString());});
    }
    if(kind=="activity"){
      auto encounter=new QPushButton(tr("＋ 이 활동에서 만나는 사건 만들기"));encounter->setObjectName("workspaceEncounter");form->addRow(encounter);connect(encounter,&QPushButton::clicked,panel,[=]{addKind->setCurrentIndex(addKind->findData("event"));add->click();});
    }
    if(kind=="event"||kind=="ending"||kind=="scriptNode"||kind=="scriptChoice"){
      linkedEditor("workspaceScript",tr("이 사건의 대본·선택지·후속 연결 지도…"),[this,n]{
        const auto type=n.value("kind").toString();
        openScriptDocument(n.value("file").toString(),type=="scriptNode"||type=="scriptChoice"?n.value("id").toString().split('/').value(2):QString());
      });
    }
    if(kind=="scriptNode"||kind=="scriptChoice"){
      const auto type=state->draft.value("type").toString();
      auto prose=[&](QString key,QString caption){
        const auto original=state->draft.value(key);auto field=new QPlainTextEdit(original.toString());
        field->setObjectName("workspaceProse-"+key);field->setMaximumHeight(140);form->addRow(caption,field);
        if(!original.isUndefined()&&!original.isString()){field->setPlainText(tr("다국어 값 · 전용 편집기에서 편집"));field->setReadOnly(true);}
        state->fields[key]=[field,original]{return field->isReadOnly()?original:QJsonValue(field->toPlainText());};
        connect(field,&QPlainTextEdit::textChanged,panel,dirty);
      };
      if(type=="dialogue"||kind=="scriptChoice")prose("text",tr("대사·선택 문구"));
      if(type=="choice")prose("prompt",tr("선택 질문"));
      if(kind=="scriptChoice"){
        prose("consequenceText",tr("선택 결과 안내"));
        logic("requirements",tr("이 선택지를 사용할 조건…"),"condition");
        logic("effects",tr("선택 직후 적용할 효과…"),"effects");
      }
      if(type=="effect")logic("effects",tr("대본 효과 편집…"),"effects");
      if(type=="branch")logic("condition",tr("대본 분기 조건…"),"condition");
      for(const auto &key:QStringList{"next","then","else"})if(state->draft.contains(key)){
        auto target=new QComboBox;target->setObjectName("workspaceScriptLink-"+key);
        target->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);target->setMinimumContentsLength(18);
        for(auto v:state->nodes){const auto row=v.toObject();if(row.value("kind")=="scriptNode"&&row.value("file")==n.value("file"))
          target->addItem(row.value("label").toString().simplified().left(80),row.value("value").toObject().value("id").toString());}
        const auto current=state->draft.value(key).toString();if(target->findData(current)<0)target->addItem(tr("없는 대상: ")+current,current);
        target->setCurrentIndex(target->findData(current));form->addRow(key=="next"?tr("다음 대사·선택"):key=="then"?tr("조건이 맞으면"):tr("조건이 맞지 않으면"),target);
        state->fields[key]=[target]{return target->currentData().toString();};connect(target,qOverload<int>(&QComboBox::currentIndexChanged),panel,dirty);
      }
    }
