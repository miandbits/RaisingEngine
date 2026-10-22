    auto linkedEditor=[&,this](QString name,QString caption,std::function<void()> open){auto button=new QPushButton(caption);button->setObjectName(name);form->addRow(button);connect(button,&QPushButton::clicked,panel,[state,notice,open]{if(state->dirty){notice->setText(QObject::tr("현재 입력을 먼저 적용하거나 취소하세요."));return;}open();});};
    if(kind=="attribute"){
      auto type=new QComboBox;type->setObjectName("workspaceAttributeKind");type->addItem(tr("능력"),"stat");type->addItem(tr("자원"),"resource");type->addItem(tr("상태 수치"),"condition");type->setCurrentIndex(type->findData(state->draft.value("kind").toString()));form->addRow(tr("데이터 종류"),type);state->fields["kind"]=[type]{return type->currentData().toString();};connect(type,qOverload<int>(&QComboBox::currentIndexChanged),panel,dirty);
    }
    if(kind=="stateGroup"){
      auto initial=new QComboBox;initial->setObjectName("workspaceStateInitial");for(const auto& v:state->draft.value("values").toArray()){const auto value=v.toObject();initial->addItem(value.value("name").toString(),value.value("id").toString());}initial->setCurrentIndex(initial->findData(state->draft.value("initialValueId").toString()));form->addRow(tr("초기 상태"),initial);state->fields["initialValueId"]=[initial]{return initial->currentData().toString();};connect(initial,qOverload<int>(&QComboBox::currentIndexChanged),panel,dirty);
      form->addRow(tr("적용 범위"),new QLabel(state->draft.value("scope")=="character"?tr("캐릭터별"):tr("공용")));
      linkedEditor("workspaceStateValues",tr("상태 값 추가·삭제·범위 편집…"),[this]{editCustomStates();});
    }
    if(kind=="screen"||kind=="screenRouting"||kind=="screenRoute"){
      if(kind=="screen"){
        linkedEditor("workspaceCanvas",tr("이 화면의 배치 편집…"),[this,n]{openScreen(n.value("id").toString());if(auto dock=findChild<QDockWidget*>("authoringWorkspaceDock"))dock->hide();});
        linkedEditor("workspaceAddButton",tr("이 화면에 행동 버튼 추가"),[this,n]{openScreen(n.value("id").toString());addButton();});
        linkedEditor("workspaceAddText",tr("이 화면에 데이터 문구 추가"),[this,n]{openScreen(n.value("id").toString());addElement("text");});
      }
      linkedEditor("workspaceRouting",tr("기본·조건별 화면 연결과 우선순위…"),[this]{editScreenRouting();});
      if(kind=="screenRoute"){choice("screenId",tr("표시할 화면"),"screen");logic("when",tr("화면을 표시할 조건…"),"condition");}
    }
    if(kind=="screenElement"||kind=="presentation"){
      const auto ids=n.value("id").toString().split('/');const auto screen=ids.value(0),element=ids.value(1);
      auto select=[this,screen,element]{openScreen(screen);selectElement(element);};
      linkedEditor("workspaceCanvas",tr("화면에서 위치·크기 편집…"),[this,select]{select();if(auto dock=findChild<QDockWidget*>("authoringWorkspaceDock"))dock->hide();});
      linkedEditor("workspacePresentation",tr("조건별 문구·이미지·애니메이션…"),[this,select]{select();editPresentationVariants();});
      if(kind=="presentation"){logic("when",tr("이 표현을 사용할 조건…"),"condition");}
      else {
        const auto elementKind=state->draft.value("kind").toString();
        if(elementKind=="text"||elementKind=="button")text("text",tr("표시 문구"));
        logic("visibleCondition",tr("표시 조건…"),"condition");
        if(elementKind=="text"||elementKind=="meter")linkedEditor("workspaceDataBinding",tr("표시할 능력·관계·데이터 선택…"),[this,select]{select();editDataBinding();});
        if(elementKind=="button"){
          logic("enabledCondition",tr("버튼 사용 조건…"),"condition");
          // Existing actions are preserved. Opt-in binding is explicit and name-based.
          auto mode=new QComboBox;mode->setObjectName("workspaceButtonMode");mode->addItem(tr("기존 버튼 동작 유지"),"keep");mode->addItem(tr("공통 행동 실행"),"executeAction");mode->addItem(tr("화면 이동"),"navigate");form->addRow(tr("연결 종류"),mode);
          auto target=new QComboBox;target->setObjectName("workspaceButtonTarget");form->addRow(tr("연결 대상"),target);
          auto fillTarget=[=]{target->clear();const auto type=mode->currentData().toString();if(type=="keep"){target->setEnabled(false);return;}target->setEnabled(true);const auto wanted=type=="executeAction"?"action":"screen";for(const auto& v:state->nodes){const auto row=v.toObject();if(row.value("kind")==wanted&&(wanted==QString("action")||!row.value("value").toObject().value("modal").toBool()))target->addItem(row.value("label").toString(),row.value("id").toString());}const auto id=state->draft.value(type=="executeAction"?"actionId":"screenId").toString();const auto at=target->findData(id);if(at>=0)target->setCurrentIndex(at);};
          connect(mode,qOverload<int>(&QComboBox::currentIndexChanged),panel,[=]{fillTarget();dirty();});connect(target,qOverload<int>(&QComboBox::currentIndexChanged),panel,dirty);fillTarget();
          state->buttonMode=mode;state->buttonTarget=target;
        }
      }
    }
