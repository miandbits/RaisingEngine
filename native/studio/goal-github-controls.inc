  auto github=std::make_shared<QJsonObject>(),bundle=std::make_shared<QJsonObject>();
  auto githubBox=new QVBoxLayout;box->insertLayout(1,githubBox);
  auto repoBar=new QHBoxLayout;githubBox->addLayout(repoBar);
  auto repo=new QLineEdit;repo->setObjectName("goalGitHubRepository");repo->setPlaceholderText("owner/repository");repoBar->addWidget(repo,2);
  auto branch=new QLineEdit;branch->setObjectName("goalGitHubBranch");branch->setPlaceholderText(tr("브랜치 (비우면 전체)"));repoBar->addWidget(branch,1);
  for(auto input:{repo,branch,file}){auto palette=input->palette();palette.setColor(QPalette::PlaceholderText,QColor("#aab4c2"));input->setPalette(palette);}
  auto refresh=new QPushButton(tr("GitHub 실행 조회"));refresh->setObjectName("goalGitHubRefresh");repoBar->addWidget(refresh);
  auto previous=new QPushButton(tr("이전"));previous->setObjectName("goalGitHubPrevious");previous->setEnabled(false);repoBar->addWidget(previous);
  auto next=new QPushButton(tr("다음"));next->setObjectName("goalGitHubNext");next->setEnabled(false);repoBar->addWidget(next);
  auto runBar=new QHBoxLayout;githubBox->addLayout(runBar);
  auto runs=new QComboBox;runs->setObjectName("goalGitHubRuns");runs->setMinimumContentsLength(30);runs->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);runBar->addWidget(runs,1);
  auto fetch=new QPushButton(tr("선택 실행의 보고서 가져오기"));fetch->setObjectName("goalGitHubFetch");fetch->setEnabled(false);runBar->addWidget(fetch);
  auto reportBar=new QHBoxLayout;githubBox->addLayout(reportBar);
  auto reports=new QComboBox;reports->setObjectName("goalGitHubReports");reports->setMinimumContentsLength(30);reports->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);reportBar->addWidget(reports,1);
  auto remoteOpen=new QPushButton(tr("보고서 열기 · 자동 재검증"));remoteOpen->setObjectName("goalGitHubOpen");remoteOpen->setEnabled(false);reportBar->addWidget(remoteOpen);
  auto onlyFailed=new QCheckBox(tr("실패·변화 경로만"));onlyFailed->setObjectName("goalCiOnlyFailed");bar->addWidget(onlyFailed);
  auto filterRows=[=]{for(int i=0;i<table->rowCount();i++){bool failed=table->item(i,0)->data(Qt::UserRole+1).toJsonObject().value("status")!="unchanged";int at=table->item(i,0)->data(Qt::UserRole).toInt();auto rows=local->value("rows").toArray();if(at>=0&&at<rows.size())failed=failed||rows[at].toObject().value("status")!="unchanged";table->setRowHidden(i,onlyFailed->isChecked()&&!failed);}if(table->currentRow()>=0&&table->isRowHidden(table->currentRow())){table->clearSelection();table->setCurrentCell(-1,-1);}if(table->currentRow()<0)for(int i=0;i<table->rowCount();i++)if(!table->isRowHidden(i)){table->selectRow(i);break;}};
  connect(onlyFailed,&QCheckBox::toggled,dialog,filterRows);
  connect(this,&StudioWindow::goalImpactReady,dialog,filterRows);
  auto listRuns=[=,this](int page){
    if(bridge.busy())return;
    const auto requestedRepo=repo->text().trimmed(),requestedBranch=branch->text().trimmed();
    *github={};*bundle={};runs->clear();reports->clear();fetch->setEnabled(false);remoteOpen->setEnabled(false);previous->setEnabled(false);next->setEnabled(false);
    notice->setText(tr("GitHub Actions 실행 목록 조회 중…"));
    request("goalGitHubRuns",{{"repository",requestedRepo},{"branch",requestedBranch},{"page",page}},[=](QJsonObject reply){
      if(!guard)return;
      if(repo->text().trimmed()!=requestedRepo||branch->text().trimmed()!=requestedBranch){notice->setText(tr("조회 조건이 변경되었습니다. 다시 조회하세요."));return;}
      if(reply.value("error").isString()){notice->setText(reply.value("error").toString());return;}
      *github=reply.value("goalGitHub").toObject();
      for(auto v:github->value("runs").toArray()){auto r=v.toObject();runs->addItem(tr("#%1 · %2 · %3 · %4 · %5 · %6").arg(r.value("number").toInt()).arg(r.value("conclusion").toString(r.value("status").toString()),r.value("branch").toString(),r.value("sha").toString().left(8),r.value("createdAt").toString(),r.value("title").toString()),r);}
      fetch->setEnabled(runs->count()>0);previous->setEnabled(github->value("page").toInt()>1);next->setEnabled(github->value("hasNext").toBool());
      notice->setText(runs->count()?tr("%1페이지 · 실행을 선택하고 보고서를 가져오세요. 현재 열린 보고서는 유지됩니다.").arg(github->value("page").toInt()):tr("이 브랜치에 경로 회귀 실행이 없습니다."));
    });
  };
  connect(refresh,&QPushButton::clicked,dialog,[=]{listRuns(1);});
  connect(previous,&QPushButton::clicked,dialog,[=]{listRuns(github->value("page").toInt()-1);});
  connect(next,&QPushButton::clicked,dialog,[=]{listRuns(github->value("page").toInt()+1);});
  auto invalidate=[=]{*github={};*bundle={};runs->clear();reports->clear();fetch->setEnabled(false);remoteOpen->setEnabled(false);previous->setEnabled(false);next->setEnabled(false);};
  connect(repo,&QLineEdit::textEdited,dialog,invalidate);connect(branch,&QLineEdit::textEdited,dialog,invalidate);
  connect(runs,&QComboBox::currentIndexChanged,dialog,[=]{*bundle={};reports->clear();remoteOpen->setEnabled(false);});
  connect(fetch,&QPushButton::clicked,dialog,[=,this]{
    if(bridge.busy()||runs->currentIndex()<0)return;
    const auto requestedRun=runs->currentData().toJsonObject().value("id"),requestedList=github->value("id");
    *bundle={};reports->clear();remoteOpen->setEnabled(false);notice->setText(tr("보고서를 가져오는 중… (최대 64 MiB, 90초 요청 제한)"));
    request("goalGitHubReports",{{"listingId",requestedList},{"runId",requestedRun}},[=](QJsonObject reply){
      if(!guard)return;
      if(github->value("id")!=requestedList||runs->currentData().toJsonObject().value("id")!=requestedRun){notice->setText(tr("선택 실행이 변경되었습니다. 다시 가져오세요."));return;}
      if(reply.value("error").isString()){notice->setText(reply.value("error").toString());return;}
      *bundle=reply.value("goalGitHub").toObject();
      for(auto v:bundle->value("reports").toArray()){auto r=v.toObject();reports->addItem(tr("%1 · %2 · 변화/실패 %3개 · %4").arg(r.value("primary").toBool()?tr("프로젝트"):tr("테스트/기타"),r.contains("error")?tr("손상"):r.value("ok").toBool()?"PASS":"FAIL").arg(r.value("failedRoutes").toInt()).arg(r.value("entry").toString()),r);}
      remoteOpen->setEnabled(reports->count()>0);notice->setText(tr("가져오기 완료. 보고서를 선택해 여세요. 테스트/기타 보고서는 의도한 실패 사례를 포함할 수 있습니다."));
    });
  });
  connect(remoteOpen,&QPushButton::clicked,dialog,[=,this]{
    if(bridge.busy()||reports->currentIndex()<0)return;
    *ci={};*local={};check->setEnabled(false);graph->setEnabled(false);table->setRowCount(0);details->clear();
    request("goalGitHubOpen",{{"bundleId",bundle->value("id")},{"reportId",reports->currentData().toJsonObject().value("id")}},[=](QJsonObject reply){
      if(!guard)return;
      if(reply.value("error").isString()){notice->setText(reply.value("error").toString());return;}
      *ci=reply.value("goalCi").toObject();check->setEnabled(true);render();filterRows();check->click();
    });
  });
  request("goalGitHubDefaults",{},[=](QJsonObject reply){if(!guard||!repo->text().isEmpty())return;repo->setText(reply.value("goalGitHub").toObject().value("repository").toString());});
