// Included inside exportGameDialog after the generic compatibility controls.
// The accepted selection outlives the nested review; unaccepted edits never do.
  auto presentationSelection=std::make_shared<QJsonObject>();
  auto presentationRow=new QHBoxLayout;layout->insertLayout(layout->count()-1,presentationRow);
  auto presentationReview=new QPushButton(tr("외형 업데이트에서 저장 유지…"));presentationReview->setObjectName("exportPresentationReview");presentationRow->addWidget(presentationReview);
  auto presentationSummary=new QLabel(tr("외형 업데이트가 선택되지 않았습니다."));presentationSummary->setObjectName("exportPresentationSummary");presentationSummary->setWordWrap(true);presentationRow->addWidget(presentationSummary,1);
  auto presentationClear=new QPushButton(tr("선택 해제"));presentationClear->setObjectName("exportPresentationClear");presentationClear->setEnabled(false);presentationRow->addWidget(presentationClear);
  auto clearPresentation=[presentationSelection,summary=QPointer<QLabel>(presentationSummary),clear=QPointer<QPushButton>(presentationClear)]{
    *presentationSelection=QJsonObject{};if(summary)summary->setText(QObject::tr("외형 업데이트가 선택되지 않았습니다."));if(clear)clear->setEnabled(false);
  };
  connect(presentationClear,&QPushButton::clicked,&dialog,clearPresentation);
  connect(target,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[clearPresentation]{clearPresentation();});
  connect(this,&StudioWindow::updated,&dialog,[this,sourceRoot,sourceRevision,clearPresentation]{if(view.value("root")!=sourceRoot||view.value("revision")!=sourceRevision||view.value("dirty").toBool())clearPresentation();});
  if(auto genericCheck=dialog.findChild<QPushButton*>("exportCheckSaveCompatibility"))connect(genericCheck,&QPushButton::clicked,&dialog,clearPresentation);
  connect(presentationReview,&QPushButton::clicked,&dialog,[&,this]{
    QDialog review(&dialog);review.setObjectName("presentationUpdateDialog");review.setWindowTitle(tr("외형 업데이트 · 기존 저장 유지"));review.resize(700,560);
    auto shell=new QVBoxLayout(&review);auto area=new QScrollArea;area->setWidgetResizable(true);area->setFrameShape(QFrame::NoFrame);shell->addWidget(area,1);auto body=new QWidget;area->setWidget(body);auto content=new QVBoxLayout(body);
    auto scope=new QLabel(tr("배치·색·연결되지 않은 화면 글과 이미지 픽셀만 검사합니다. 규칙·이야기·동작·데이터 연결 변경은 허용하지 않습니다. 성공한 검사를 명시적으로 선택하면 이전 게임의 저장 정체성을 유지합니다. 원본 게임과 저장 파일은 바꾸지 않습니다."));scope->setWordWrap(true);content->addWidget(scope);
    auto form=new QFormLayout;content->addLayout(form);
    auto fileRow=[&](QString title,QString id,QString initial){auto box=new QWidget;auto line=new QHBoxLayout(box);line->setContentsMargins(0,0,0,0);auto field=new QLineEdit(initial);field->setObjectName(id);auto choose=new QPushButton(tr("파일 선택…"));choose->setObjectName(id+"Browse");line->addWidget(field,1);line->addWidget(choose);form->addRow(title,box);connect(choose,&QPushButton::clicked,&review,[&,field]{const auto path=QFileDialog::getOpenFileName(&review,tr("검사할 JSON 파일 선택"),field->text(),tr("JSON 파일 (*.json)"));if(!path.isEmpty())field->setText(path);});return field;};
    auto previous=fileRow(tr("이전 내보내기의 game.json"),"presentationPreviousBundle",presentationSelection->value("previousBundle").toString());
    auto sample=fileRow(tr("예시 플레이 저장 (선택)"),"presentationSampleSave",presentationSelection->value("sampleSave").toString());
    auto platform=new QLabel(target->currentText());platform->setObjectName("presentationTarget");form->addRow(tr("대상"),platform);
    auto check=new QPushButton(tr("외형 변경과 기존 저장 확인"));check->setObjectName("presentationCheck");content->addWidget(check);
    auto message=new QLabel(tr("현재 프로젝트를 저장한 다음 이전 내보내기를 선택하고 검사하세요."));message->setObjectName("presentationStatus");message->setWordWrap(true);message->setTextInteractionFlags(Qt::TextSelectableByMouse);content->addWidget(message);
    auto detailsToggle=new QToolButton;detailsToggle->setObjectName("presentationDetailsToggle");detailsToggle->setText(tr("검사 세부 정보"));detailsToggle->setCheckable(true);detailsToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);detailsToggle->setArrowType(Qt::RightArrow);content->addWidget(detailsToggle);
    auto details=new QPlainTextEdit;details->setObjectName("presentationDetails");details->setReadOnly(true);details->setMaximumHeight(180);details->hide();content->addWidget(details);connect(detailsToggle,&QToolButton::toggled,&review,[=](bool visible){details->setVisible(visible);detailsToggle->setArrowType(visible?Qt::DownArrow:Qt::RightArrow);});content->addStretch();
    auto optIn=new QCheckBox(tr("이 검사를 사용하여 외형 업데이트에서 기존 저장 유지"));optIn->setObjectName("presentationOptIn");optIn->setEnabled(false);shell->addWidget(optIn);
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel);auto apply=buttons->button(QDialogButtonBox::Apply);apply->setObjectName("presentationApply");apply->setEnabled(false);shell->addWidget(buttons);
    auto result=std::make_shared<QJsonObject>();auto generation=std::make_shared<int>(0);
    auto invalidate=[result,generation,optIn,apply,details,message]{++*generation;*result=QJsonObject{};optIn->setChecked(false);optIn->setEnabled(false);apply->setEnabled(false);details->clear();message->setText(QObject::tr("선택이나 프로젝트가 바뀌었습니다. 다시 검사하세요."));};
    connect(previous,&QLineEdit::textChanged,&review,[invalidate]{invalidate();});connect(sample,&QLineEdit::textChanged,&review,[invalidate]{invalidate();});
    connect(target,qOverload<int>(&QComboBox::currentIndexChanged),&review,[&,invalidate]{platform->setText(target->currentText());invalidate();});
    connect(this,&StudioWindow::updated,&review,[this,sourceRoot,sourceRevision,invalidate]{if(view.value("root")!=sourceRoot||view.value("revision")!=sourceRevision||view.value("dirty").toBool())invalidate();});
    connect(optIn,&QCheckBox::toggled,&review,[apply,result](bool checked){apply->setEnabled(checked&&!result->value("checkId").toString().isEmpty());});
    connect(buttons,&QDialogButtonBox::rejected,&review,&QDialog::reject);
    connect(apply,&QPushButton::clicked,&review,[&,result]{if(optIn->isChecked()&&optIn->isEnabled()&&!result->value("checkId").toString().isEmpty())review.accept();});
    connect(check,&QPushButton::clicked,&review,[&,this,result,generation,invalidate]{
      invalidate();
      if(bridge.busy()){message->setText(tr("현재 편집 작업이 끝난 뒤 다시 검사하세요."));return;}
      if(view.value("root")!=sourceRoot||view.value("revision")!=sourceRevision){message->setText(tr("프로젝트가 변경되었습니다. 내보내기 창을 다시 여세요."));return;}
      if(view.value("dirty").toBool()){message->setText(tr("현재 프로젝트를 먼저 저장한 뒤 다시 확인하세요. 초안을 자동 저장하지 않습니다."));return;}
      const auto baseline=QFileInfo(previous->text()).canonicalFilePath(),save=sample->text().trimmed().isEmpty()?QString():QFileInfo(sample->text()).canonicalFilePath();
      if(!QDir::isAbsolutePath(previous->text())||baseline.isEmpty()||!QFileInfo(baseline).isFile()||(!sample->text().trimmed().isEmpty()&&(!QDir::isAbsolutePath(sample->text())||save.isEmpty()||!QFileInfo(save).isFile()))){message->setText(tr("존재하는 이전 game.json과 선택한 예시 저장 파일의 절대 경로를 지정하세요."));return;}
      const auto targetId=target->currentData().toString();const int attempt=*generation;
      QJsonObject params{{"revision",sourceRevision},{"previousBundle",baseline},{"target",targetId}};if(!save.isEmpty())params["sampleSave"]=save;
      check->setEnabled(false);previous->parentWidget()->setEnabled(false);sample->parentWidget()->setEnabled(false);message->setText(tr("외형 변경을 검사하는 중… 원본 파일은 변경하지 않습니다."));
      request("presentationUpdateCheck",params,[this,guard=QPointer<QDialog>(&review),check=QPointer<QPushButton>(check),previous=QPointer<QLineEdit>(previous),sample=QPointer<QLineEdit>(sample),message=QPointer<QLabel>(message),details=QPointer<QPlainTextEdit>(details),optIn=QPointer<QCheckBox>(optIn),targetGuard=QPointer<QComboBox>(target),sourceRoot,sourceRevision,targetId,baseline,save,attempt,result,generation](QJsonObject reply){
        if(!guard||!check||!previous||!sample||!message||!details||!optIn||!targetGuard)return;
        check->setEnabled(true);previous->parentWidget()->setEnabled(true);sample->parentWidget()->setEnabled(true);
        if(attempt!=*generation||view.value("root")!=sourceRoot||view.value("revision")!=sourceRevision||view.value("dirty").toBool()||targetGuard->currentData()!=targetId)return;
        const auto report=reply.value("presentationUpdate").toObject();
        const auto diagnostic=reply.value("error").toString();
        if(!diagnostic.isEmpty()){
          const auto code=diagnostic.section(':',0,0);QString summary;
          if(code=="PRESENTATION_CONTENT_CHANGED"||code=="PRESENTATION_PLUGINS_CHANGED"||code=="PRESENTATION_ASSET_CHANGED")summary=QObject::tr("규칙·이야기 또는 보호된 내용이 바뀌었습니다. 기존 저장 호환성 검사와 이전 도구를 사용하세요.");
          else if(code=="PRESENTATION_SAVE_FIRST")summary=QObject::tr("현재 프로젝트를 먼저 저장한 뒤 다시 확인하세요. 초안을 자동 저장하지 않습니다.");
          else if(code=="PRESENTATION_SAVE_INVALID")summary=QObject::tr("예시 저장을 이전 내보내기에서 검증할 수 없습니다. 해당 버전의 올바른 원본 저장을 선택하세요.");
          else if(code=="PRESENTATION_CHECK_STALE")summary=QObject::tr("프로젝트나 선택한 파일이 바뀌었습니다. 다시 검사하세요.");
          details->setPlainText(diagnostic);message->setText(summary.isEmpty()?diagnostic:summary+"\n"+diagnostic);return;
        }
        const auto classification=report.value("classification").toString();
        if(report.value("checkId").toString().isEmpty()||(classification!="presentation-only"&&classification!="unchanged")||report.value("target")!=targetId||report.value("checkedEditorRevision")!=sourceRevision||report.value("previousBundle")!=baseline||report.value("currentProject")!=sourceRoot||(!save.isEmpty()&&(!report.value("sample").toObject().value("valid").toBool()||report.value("sample").toObject().value("file")!=save))){message->setText(QObject::tr("검사 결과가 현재 선택과 일치하지 않습니다. 다시 검사하세요."));return;}
        *result=report;(*result)["sampleSave"]=save;
        message->setText((classification=="unchanged"?QObject::tr("내용 변경이 없습니다."):QObject::tr("외형만 변경되었습니다. 변경 위치 %1개 · 이미지 %2개.").arg(report.value("changedPaths").toArray().size()).arg(report.value("images").toArray().size()))+"\n"+(save.isEmpty()?QObject::tr("예시 저장은 선택하지 않았습니다."):QObject::tr("선택한 예시 저장을 검증했습니다."))+"\n"+QObject::tr("아래 선택을 켜고 적용해야 이 검사로 내보냅니다."));
        details->setPlainText(QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented)));optIn->setEnabled(true);
      });
    });
    if(review.exec()!=QDialog::Accepted)return;
    if(view.value("root")!=sourceRoot||view.value("revision")!=sourceRevision||view.value("dirty").toBool()||result->value("target").toString()!=target->currentData().toString()||!optIn->isChecked()||!optIn->isEnabled()||result->value("checkId").toString().isEmpty())return;
    *presentationSelection=*result;presentationSummary->setText(tr("외형 업데이트에서 기존 저장 유지가 선택되었습니다."));presentationClear->setEnabled(true);
  });
