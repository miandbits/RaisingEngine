    if(kind=="character"||kind=="activity"||kind=="event"){
      const auto revision=view.value("revision"),root=view.value("root");
      auto dialog=new QDialog;auto box=new QVBoxLayout(dialog);auto form=new QFormLayout;box->addLayout(form);
      auto name=new QLineEdit;name->setObjectName("workspaceNewName");form->addRow(tr("이름"),name);
      auto member=new QCheckBox(tr("이 인물을 육성 대상으로 함께 등록"));member->setObjectName("workspaceNewMember");member->setEnabled(!project.value("roster").toObject().isEmpty());if(kind=="character")form->addRow(member);else member->setParent(dialog),member->hide();
      auto activity=new QComboBox;activity->setObjectName("workspaceNewActivity");for(auto v:state->nodes){auto n=v.toObject();if(n.value("kind")=="activity")activity->addItem(n.value("label").toString(),n.value("id").toString());}
      const auto selected=state->selected;if(selected.value("kind")=="activity")activity->setCurrentIndex(activity->findData(selected.value("id").toString()));
      if(kind=="event"||kind=="character")form->addRow(kind=="event"?tr("이 활동을 마친 뒤 만남"):tr("육성 대상의 기본 활동"),activity);else activity->setParent(dialog),activity->hide();
      auto personal=new QCheckBox(tr("캐릭터별 사건 · 활동한 인물의 기록 사용"));personal->setObjectName("workspaceNewPersonal");personal->setEnabled(project.value("roster").toObject().value("stateModel")=="character-v1");
      auto speaker=new QComboBox;speaker->setObjectName("workspaceNewSpeaker");speaker->addItem(tr("해설"),QString());for(auto v:state->nodes){auto n=v.toObject();if(n.value("kind")=="character")speaker->addItem(n.value("label").toString(),n.value("id").toString());}
      auto dialogue=new QPlainTextEdit;dialogue->setObjectName("workspaceNewDialogue");dialogue->setMaximumHeight(100);
      auto prompt=new QLineEdit;prompt->setObjectName("workspaceNewPrompt");
      auto yes=new QLineEdit, no=new QLineEdit;yes->setObjectName("workspaceNewChoice1");no->setObjectName("workspaceNewChoice2");
      if(kind=="event"){form->addRow(personal);form->addRow(tr("말하는 인물"),speaker);form->addRow(tr("첫 대사"),dialogue);form->addRow(tr("선택 질문 · 비우면 대사만"),prompt);form->addRow(tr("첫 번째 선택"),yes);form->addRow(tr("두 번째 선택"),no);auto help=new QLabel(tr("생성 후 대사·선택을 눌러 효과와 후속 사건을 연결합니다. 날짜·발생 조건도 같은 작업실에서 조정할 수 있습니다."));help->setWordWrap(true);box->addWidget(help);}
      else for(QWidget *w:QList<QWidget*>{personal,speaker,dialogue,prompt,yes,no}){w->setParent(dialog);w->hide();}
      auto error=new QLabel;error->setWordWrap(true);error->setObjectName("workspaceCreateError");box->addWidget(error);box->addStretch();
      auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Save)->setText(tr("만들기"));box->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
      embed(dialog,tr("새 %1 만들기").arg(workspaceKind(kind)),[=]{
        auto fail=[=](QString message){error->setText(message);return false;};
        if(bridge.busy()||revision!=view.value("revision")||root!=view.value("root"))return fail(tr("프로젝트가 변경되었습니다. 취소하고 다시 만드세요."));
        if(name->text().trimmed().isEmpty())return fail(tr("이름을 입력하세요."));
        if((kind=="event"||member->isChecked())&&activity->currentIndex()<0)return fail(tr("일정 활동을 먼저 만드세요."));
        if(kind=="event"&&(dialogue->toPlainText().trimmed().isEmpty()||(!prompt->text().trimmed().isEmpty()&&(yes->text().trimmed().isEmpty()||no->text().trimmed().isEmpty()))))return fail(tr("첫 대사와 선택 질문에 필요한 두 선택을 입력하세요."));
        const auto id=kind+"-"+QUuid::createUuid().toString(QUuid::Id128).left(12),file=(kind=="activity"?"activities":kind=="character"?"characters":"events")+QString("/")+id+".json";
        QJsonObject data{{"formatVersion","0.1.0"},{"id",id},{kind=="character"?"name":"label",name->text().trimmed()}};
        if(kind=="character"){data["portraits"]=QJsonObject{};data["relationships"]=QJsonArray{QJsonObject{{"id","trust"},{"label",tr("신뢰")},{"min",0},{"max",100},{"initial",0}}};}
        if(kind=="activity"){data["requirements"]=QJsonObject{{"op","always"}};data["costs"]=QJsonArray{};data["baseEffects"]=QJsonArray{};}
        if(kind=="event"){
          const bool choices=!prompt->text().trimmed().isEmpty();QJsonArray nodes{QJsonObject{{"id","greeting"},{"type","dialogue"},{"speakerId",speaker->currentData().toString().isEmpty()?QJsonValue(QJsonValue::Null):QJsonValue(speaker->currentData().toString())},{"text",dialogue->toPlainText().trimmed()},{"next",choices?"decision":"end"}}};
          if(choices)nodes.append(QJsonObject{{"id","decision"},{"type","choice"},{"prompt",prompt->text().trimmed()},{"choices",QJsonArray{QJsonObject{{"id","first"},{"text",yes->text().trimmed()},{"requirements",QJsonObject{{"op","always"}}},{"effects",QJsonArray{}},{"next","end"}},QJsonObject{{"id","second"},{"text",no->text().trimmed()},{"requirements",QJsonObject{{"op","always"}}},{"effects",QJsonArray{}},{"next","end"}}}}});
          nodes.append(QJsonObject{{"id","end"},{"type","end"}});data["nodes"]=nodes;data["entryNodeId"]="greeting";data["requirements"]=QJsonObject{{"op","always"}};data["priority"]=50;data["trigger"]="after-activity";data["activityId"]=activity->currentData().toString();data["scope"]=personal->isChecked()?"actor":"shared";data["window"]=QJsonObject{{"fromPeriod",0},{"throughPeriod",qMax(0,project.value("durationMonths").toInt()*project.value("periodsPerMonth").toInt()-1)}};
        }
        QJsonArray edits{QJsonObject{{"type","add"},{"kind",kind},{"id",id}},QJsonObject{{"type","replace"},{"file",file},{"data",data}}};
        if(kind=="character"&&member->isChecked()){
          auto updated=project,roster=project.value("roster").toObject();auto members=roster.value("members").toArray();QJsonObject attrs;
          for(auto v:roster.value("localAttributeIds").toArray()){auto initial=project.value("initialState").toObject().value("attributes").toObject().value(v.toString());if(initial.isUndefined())for(auto n:state->nodes){auto row=n.toObject();if(row.value("kind")=="attribute"&&row.value("id")==v)initial=row.value("value").toObject().value("initial");}attrs[v.toString()]=initial.isUndefined()?QJsonValue(0):initial;}
          members.append(QJsonObject{{"characterId",id},{"activityId",activity->currentData().toString()},{"initialAttributes",attrs}});roster["members"]=members;updated["roster"]=roster;edits.append(QJsonObject{{"type","replace"},{"file","project.json"},{"data",updated}});
        }
        dialog->setEnabled(false);request("edit",{{"edit",QJsonObject{{"type","batch"},{"edits",edits}}}},[=](QJsonObject reply){dialog->setEnabled(true);if(!reply.value("error").toString().isEmpty()){error->setText(reply.value("error").toString());return;}state->pending=kind+":"+id;inlineEditor->complete();(*refresh)(true);});return false;
      });return;
    }
