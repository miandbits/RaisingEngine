void StudioWindow::goalCiDialog(){
  if(projectData().isEmpty())return;
  if(auto old=findChild<QDialog*>("goalCiDialog")){old->show();old->raise();return;}
  auto dialog=new QDialog(this);dialog->setObjectName("goalCiDialog");dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle(tr("CI 경로 보고서 · 원인 확인과 재검증"));dialog->resize(1220,780);
  QPointer<QDialog> guard(dialog);const auto root=view.value("root");
  auto ci=std::make_shared<QJsonObject>(),local=std::make_shared<QJsonObject>();
  auto box=new QVBoxLayout(dialog);auto help=new QLabel(tr("GitHub Actions의 goal-regression-reports를 풀고 report.json을 여세요. CI 결과는 원본으로 보존합니다. 현재 프로젝트를 재검증한 그래프에서 원인을 확인하고 부분 승인·기준 복원을 수행하세요."));help->setWordWrap(true);box->addWidget(help);
  auto filebar=new QHBoxLayout;box->addLayout(filebar);auto file=new QLineEdit;file->setObjectName("goalCiFile");file->setPlaceholderText(tr("다운로드한 report.json 경로"));filebar->addWidget(file,1);
  auto browse=new QPushButton(tr("파일 선택…"));filebar->addWidget(browse);auto open=new QPushButton(tr("보고서 열기"));open->setObjectName("goalCiLoad");filebar->addWidget(open);
  auto notice=new QLabel(tr("보고서를 선택하세요."));notice->setObjectName("goalCiNotice");notice->setTextFormat(Qt::PlainText);notice->setWordWrap(true);box->addWidget(notice);
  auto bar=new QHBoxLayout;box->addLayout(bar);auto check=new QPushButton(tr("현재 프로젝트 재검증 · 저장 복원 포함"));check->setObjectName("goalCiRecheck");check->setEnabled(false);bar->addWidget(check);
  auto graph=new QPushButton(tr("첫 차이 그래프 · 승인 / 복원"));graph->setObjectName("goalCiGraph");graph->setEnabled(false);bar->addWidget(graph);bar->addStretch();
  auto table=new QTableWidget(0,7);table->setObjectName("goalCiTable");table->setHorizontalHeaderLabels({tr("경로"),tr("CI 결과 (원본)"),tr("현재 결과"),tr("승인 / 남은 변화"),tr("CI 첫 차이"),tr("현재 첫 차이"),tr("저장 복원")});
  table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setSelectionMode(QAbstractItemView::SingleSelection);table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);box->addWidget(table,1);
  for(int col=1;col<7;col++){table->horizontalHeader()->setSectionResizeMode(col,QHeaderView::Interactive);table->setColumnWidth(col,col==3?130:105);}
  auto details=new QPlainTextEdit;details->setObjectName("goalCiDetails");details->setReadOnly(true);box->addWidget(details,1);
  auto selectedLocal=[=]{int at=table->currentRow();return at<0||!table->item(at,0)?-1:table->item(at,0)->data(Qt::UserRole).toInt();};
  auto selection=[=,this]{
    int row=selectedLocal();auto rows=local->value("rows").toArray();auto current=row>=0&&row<rows.size()?rows[row].toObject():QJsonObject();
    const bool fresh=local->value("viewRevision")==view.value("revision")&&!local->value("stale").toBool();
    graph->setEnabled(fresh&&current.contains("baselineId")&&QStringList{"changed","failed","unchanged"}.contains(current.value("status").toString()));
    QJsonObject source;if(table->currentRow()>=0)source=table->item(table->currentRow(),0)->data(Qt::UserRole+1).toJsonObject();
    QStringList lines{tr("CI 원본 · %1").arg(ci->value("sourceFile").toString()),tr("원본 보고서 SHA-256 · %1").arg(ci->value("checksum").toString())};
    for(const auto& pair:QList<QPair<QString,QJsonObject>>{{tr("CI 원본"),source},{tr("현재 로컬 재검증"),current}}){
      auto r=pair.second;lines<<""<<pair.first+" · "+impactStatus(r.value("status").toString());
      if(r.contains("error"))lines<<r.value("error").toString();
      if(r.contains("message"))lines<<r.value("message").toString();
      auto failure=r.value("failure").toObject();if(!failure.isEmpty())lines<<tr("실패 #%1 · %2").arg(failure.value("step").toInt()).arg(failure.value("message").toString());
      auto replay=r.value("replay").toObject();if(replay.contains("failure"))lines<<QString::fromUtf8(QJsonDocument(replay.value("failure").toObject()).toJson(QJsonDocument::Compact));
      int shown=0;for(auto v:r.value("changeItems").toArray()){auto item=v.toObject();if(item.value("approved").toBool())continue;auto obs=item.value("observations").toArray();auto first=obs.isEmpty()?QJsonObject():obs.first().toObject();lines<<tr("#%1 · %2 · %3 → %4").arg(item.value("firstStep").toInt()).arg(item.value("path").toString(),impactValue(first.value("before")),impactValue(first.value("after")));if(++shown>=40){lines<<tr("추가 항목은 그래프 또는 JSON 보고서에서 확인하세요.");break;}}
    }
    details->setPlainText(lines.join("\n"));
  };
  auto render=[=,this]{
    auto source=ci->value("source").toObject();auto a=source.value("rows").toArray(),b=local->value("rows").toArray();
    QString selected;if(table->currentRow()>=0)selected=table->item(table->currentRow(),0)->data(Qt::UserRole+2).toString();
    QSignalBlocker block(table);table->setRowCount(a.size());int focus=0;bool foundFailure=false;
    for(int i=0;i<a.size();i++){
      auto r=a[i].toObject();int at=-1;for(int j=0;j<b.size();j++)if(!r.value("routeId").toString().isEmpty()&&b[j].toObject().value("routeId")==r.value("routeId")){at=j;break;}
      auto now=at<0?QJsonObject():b[at].toObject();auto replay=now.value("replay").toObject();
      auto number=[](QJsonValue v){return v.isDouble()?QString::number(v.toInt()):QString("—");};
      QStringList cells{r.value("name").toString()+" · "+r.value("routeId").toString(),impactStatus(r.value("status").toString()),local->isEmpty()?tr("재검증 전"):at<0?tr("현재 경로 없음"):impactStatus(now.value("status").toString()),at<0?"—":tr("%1 / %2").arg(now.value("approvedChangeCount").toInt()).arg(now.value("pendingChangeCount").toInt()),number(r.value("firstDifference")),number(now.value("firstDifference")),replay.isEmpty()?tr("미실행"):tr("%1 · %2회").arg(replay.value("ok").toBool()?"PASS":"FAIL").arg(replay.value("saves").toInt())};
      for(int j=0;j<cells.size();j++){auto item=new QTableWidgetItem(cells[j]);item->setToolTip(cells[j]);table->setItem(i,j,item);}
      table->item(i,0)->setData(Qt::UserRole,at);table->item(i,0)->setData(Qt::UserRole+1,r);table->item(i,0)->setData(Qt::UserRole+2,r.value("routeId").toString());
      table->item(i,1)->setBackground(QColor(r.value("status")=="unchanged"?"#28664d":"#93464d"));if(at>=0)table->item(i,2)->setBackground(QColor(now.value("status")=="unchanged"?"#28664d":"#93464d"));
      if(!foundFailure&&r.value("status")!="unchanged"){focus=i;foundFailure=true;}if(!selected.isEmpty()&&selected==r.value("routeId").toString())focus=i;
    }
    if(table->rowCount())table->selectRow(focus);
    QStringList notes;for(const auto& key:QList<QPair<QString,QString>>{{"fingerprint",tr("콘텐츠 변경")},{"routesDigest",tr("등록 경로 변경")},{"selectionRevision",tr("승인 기준 변경")}})if(!local->isEmpty()&&source.value(key.first)!=local->value(key.first))notes<<key.second;
    for(auto v:ci->value("reasons").toArray())notes<<v.toString();
    notes.removeDuplicates();
    QString text=tr("CI 원본: %1 · 현재 재검증: %2").arg(source.value("ok").toBool()?"PASS":"FAIL",local->isEmpty()?tr("실행 전"):local->value("ok").toBool()?"PASS":"FAIL");
    if(!notes.isEmpty())text+=tr("\nCI 당시와 현재 입력이 다릅니다: ")+notes.join(" · ")+tr("\n그래프는 현재 로컬 실행입니다. CI 당시 화면을 재현하려면 해당 콘텐츠·경로·기준 파일을 복원하세요.");
    else if(!local->isEmpty())text+=tr("\nCI 입력과 일치합니다. 그래프는 현재 런타임에서 재실행해 확인한 결과입니다.");
    for(auto v:source.value("errors").toArray())text+="\nCI: "+v.toString();
    for(auto v:local->value("errors").toArray())text+="\n"+v.toString();
    notice->setText(text);selection();
  };
  connect(table,&QTableWidget::itemSelectionChanged,dialog,selection);
  connect(browse,&QPushButton::clicked,dialog,[=]{auto chosen=QFileDialog::getOpenFileName(dialog,tr("CI 경로 보고서"),file->text(),tr("JSON 보고서 (*.json)"));if(!chosen.isEmpty())file->setText(chosen);});
  connect(check,&QPushButton::clicked,dialog,[=,this]{if(bridge.busy()||ci->isEmpty())return;graph->setEnabled(false);notice->setText(tr("현재 콘텐츠·기준·승인값으로 재실행 및 저장 복원 검사 중…"));request("goalCiRecheck",{{"id",ci->value("id")}},[=,this](QJsonObject reply){if(!guard)return;if(reply.value("error").isString()){(*local)["stale"]=true;notice->setText(reply.value("error").toString());graph->setEnabled(false);return;}*ci=reply.value("goalCi").toObject();*local=reply.value("goalImpact").toObject();(*local)["viewRevision"]=view.value("revision");lastGoalImpact=*local;emit goalImpactReady(*local);render();});});
  connect(open,&QPushButton::clicked,dialog,[=,this]{if(bridge.busy())return;*ci={};*local={};check->setEnabled(false);graph->setEnabled(false);table->setRowCount(0);details->clear();request("goalCiOpen",{{"file",file->text()}},[=](QJsonObject reply){if(!guard)return;if(reply.value("error").isString()){notice->setText(reply.value("error").toString());return;}*ci=reply.value("goalCi").toObject();check->setEnabled(true);render();check->click();});});
  auto openGraph=[=,this]{if(bridge.busy()||!graph->isEnabled())return;goalCompareDialog({},{{"impactReportId",local->value("id")},{"impactRow",selectedLocal()}});};connect(graph,&QPushButton::clicked,dialog,openGraph);connect(table,&QTableWidget::cellDoubleClicked,dialog,[=](int,int){openGraph();});
  connect(this,&StudioWindow::goalImpactReady,dialog,[=](QJsonObject report){if(ci->isEmpty())return;*local=report;render();});
  connect(this,&StudioWindow::updated,dialog,[=,this]{if(root!=view.value("root")){dialog->close();return;}if(!local->isEmpty()&&local->value("viewRevision")!=view.value("revision")){(*local)["stale"]=true;graph->setEnabled(false);notice->setText(tr("초안이 변경되었습니다. 저장한 뒤 현재 프로젝트 재검증을 실행하세요. CI 원본은 유지됩니다."));}});
  dialog->show();
}
