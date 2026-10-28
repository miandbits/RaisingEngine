#include "studio.hpp"
#include "goal-impact-status.hpp"
#include <QDialog>
#include <QHeaderView>
#include <QPushButton>
#include <QStatusBar>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {
bool activeCheck(const QJsonObject &check) {
  const auto status=check.value("status").toString();
  return status=="queued"||status=="running"||status=="cancelling";
}
QString checkStatus(QString value) {
  if(value=="queued")return QObject::tr("대기 중");
  if(value=="running")return QObject::tr("검사 중…");
  if(value=="cancelling")return QObject::tr("취소 중…");
  if(value=="completed")return QObject::tr("완료");
  if(value=="passed")return QObject::tr("통과");
  if(value=="failed")return QObject::tr("실패");
  if(value=="timed-out")return QObject::tr("시간 초과");
  if(value=="cancelled")return QObject::tr("취소됨");
  return QObject::tr("미실행");
}
}

void StudioWindow::setupGoalSaveChecks() {
  goalSaveCheckButton=new QPushButton(this);goalSaveCheckButton->setObjectName("goalSaveCheckStatus");goalSaveCheckButton->hide();
  goalSaveCheckButton->setAutoDefault(false);goalSaveCheckButton->setToolTip(tr("저장된 사본의 검사 진행과 결과를 확인합니다. 편집을 계속할 수 있습니다."));
  statusBar()->addPermanentWidget(goalSaveCheckButton);
  connect(goalSaveCheckButton,&QPushButton::clicked,this,&StudioWindow::goalSaveChecksDialog);
  goalSaveCheckPolling=new QTimer(this);goalSaveCheckPolling->setObjectName("goalSaveCheckPolling");goalSaveCheckPolling->setInterval(750);
  connect(goalSaveCheckPolling,&QTimer::timeout,this,&StudioWindow::pollGoalSaveCheck);
  connect(this,&StudioWindow::updated,this,[this]{
    if(goalSaveCheckRoot!=view.value("root").toString()){
      goalSaveCheckRoot=view.value("root").toString();lastGoalSaveCheck={};goalSaveCheckReportedId.clear();goalSaveCheckCancelId.clear();goalSaveCheckPolling->stop();
      if(auto dialog=findChild<QDialog*>("goalSaveChecksDialog"))dialog->close();
    }
    refreshGoalSaveCheck();
  });
}

void StudioWindow::acceptGoalSaveCheck(QJsonObject check) {
  if(check.isEmpty()) {lastGoalSaveCheck={};goalSaveCheckPolling->stop();refreshGoalSaveCheck();return;}
  if(check.value("root")!=view.value("root"))return;
  // A passive status reply may arrive after a newer save reply. Never replace
  // that newer saved snapshot with the result of an earlier request.
  if(lastGoalSaveCheck.value("root")==check.value("root")&&lastGoalSaveCheck.value("testedRevision").toInt(-1)>check.value("testedRevision").toInt(-1))return;
  if(lastGoalSaveCheck.value("id")==check.value("id")&&!activeCheck(lastGoalSaveCheck)&&activeCheck(check))return;
  check["stale"]=check.value("stale").toBool()||check.value("sourceVersion")!=view.value("draftVersion");
  if(!goalSaveCheckCancelId.isEmpty()&&goalSaveCheckCancelId!=check.value("id").toString())goalSaveCheckCancelId.clear();
  goalSaveCheckRoot=check.value("root").toString();lastGoalSaveCheck=check;
  if(activeCheck(check))goalSaveCheckPolling->start();else{
    goalSaveCheckPolling->stop();goalSaveCheckCancelId.clear();
    const auto id=check.value("id").toString();
    const bool firstResult=!id.isEmpty()&&goalSaveCheckReportedId!=id;
    if(firstResult){goalSaveCheckReportedId=id;if(!check.value("reports").toArray().isEmpty())acceptGoalReport(check);}
    // Cache baseline results without opening a dialog or stealing typing focus.
    // A repeated save may retain this check while advancing the view revision.
    if(check.value("goalImpact").isObject()){
      auto previous=lastGoalImpact;previous.remove("viewRevision");
      lastGoalImpact=check.value("goalImpact").toObject();lastGoalImpact["sourceVersion"]=check.value("sourceVersion");
      lastGoalImpact["viewRevision"]=check.value("sourceVersion")==view.value("draftVersion")?view.value("revision"):check.value("testedRevision");
      if(check.value("stale").toBool())lastGoalImpact["stale"]=true;
      auto comparable=lastGoalImpact;comparable.remove("viewRevision");
      if(firstResult||previous!=comparable)emit goalImpactReady(lastGoalImpact);
    }
  }
  refreshGoalSaveCheck();
}

void StudioWindow::refreshGoalSaveCheck() {
  if(!goalSaveCheckButton)return;
  goalSaveCheckButton->setVisible(!lastGoalSaveCheck.isEmpty());
  auto check=lastGoalSaveCheck;
  if(!check.isEmpty()){
    const bool stale=check.value("stale").toBool()||check.value("sourceVersion")!=view.value("draftVersion");check["stale"]=stale;
    auto text=tr("저장 검사 · %1 · %2/%3").arg(checkStatus(check.value("status").toString())).arg(check.value("completed").toInt()).arg(check.value("reports").toArray().size());
    const bool comparisonFailed=goalImpactFatal(check.value("goalImpact").toObject());
    if(comparisonFailed)text+=tr(" · 비교 오류");
    if(stale&&(!comparisonFailed||check.value("sourceVersion")!=view.value("draftVersion")||check.value("routesChanged").toBool()||check.value("baselinesChanged").toBool()))text+=tr(" · 이전 사본");
    goalSaveCheckButton->setText(text);goalSaveCheckButton->setAccessibleName(text);
  }
  emit goalSaveCheckReady(check);
}

void StudioWindow::pollGoalSaveCheck() {
  if(goalSaveCheckRoot.isEmpty()||goalSaveCheckRoot!=view.value("root").toString()){goalSaveCheckPolling->stop();return;}
  if(bridge.busy()||bridge.backgroundBusy())return;
  const auto root=goalSaveCheckRoot,cancelId=goalSaveCheckCancelId,requestedId=lastGoalSaveCheck.value("id").toString();const bool cancelling=!cancelId.isEmpty();
  QJsonObject params{{"root",root}};if(cancelling)params["checkId"]=cancelId;
  bridge.requestBackground(cancelling?"goalSaveCheckStop":"goalSaveCheckStatus",params,[this,root,cancelId,requestedId](QJsonObject reply){
    if(root!=view.value("root").toString())return;
    // A save/reopen can replace the job while this passive request is in flight.
    // Its error, null result or old result must not overwrite the replacement.
    const auto currentId=lastGoalSaveCheck.value("id").toString();
    if(requestedId!=currentId&&(!reply.value("error").toString().isEmpty()||reply.value("goalSaveCheck").toObject().value("id").toString()!=currentId))return;
    if(!reply.value("error").toString().isEmpty()){
      goalSaveCheckPolling->stop();goalSaveCheckCancelId.clear();
      lastGoalSaveCheck["status"]="failed";lastGoalSaveCheck["ok"]=false;lastGoalSaveCheck["error"]=reply.value("error");
      refreshGoalSaveCheck();return;
    }
    if(goalSaveCheckCancelId==cancelId)goalSaveCheckCancelId.clear();
    acceptGoalSaveCheck(reply.value("goalSaveCheck").toObject());
  });
}

void StudioWindow::stopGoalSaveCheck() {
  if(lastGoalSaveCheck.isEmpty()||!activeCheck(lastGoalSaveCheck))return;
  goalSaveCheckCancelId=lastGoalSaveCheck.value("id").toString();goalSaveCheckPolling->start();refreshGoalSaveCheck();pollGoalSaveCheck();
}

void StudioWindow::goalSaveChecksDialog() {
  if(auto previous=findChild<QDialog*>("goalSaveChecksDialog")){previous->show();previous->raise();return;}
  auto dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setObjectName("goalSaveChecksDialog");dialog->setWindowTitle(tr("저장 검사 진행 · 결과"));dialog->resize(1000,620);
  auto layout=new QVBoxLayout(dialog);auto help=new QLabel(tr("저장은 이미 완료되었습니다. 검사는 저장 당시의 고정 사본을 사용하며 이 창을 닫아도 계속됩니다. 다시 저장하면 최신 사본으로 교체합니다. 프로젝트를 바꾸거나 에디터를 종료하면 진행 중인 검사를 취소합니다."));help->setWordWrap(true);layout->addWidget(help);
  auto summary=new QLabel;summary->setObjectName("goalSaveCheckSummary");summary->setWordWrap(true);layout->addWidget(summary);
  auto table=new QTableWidget(0,4);table->setObjectName("goalSaveCheckRoutes");table->setHorizontalHeaderLabels({tr("등록 경로"),tr("검사 결과"),tr("명령 수"),tr("실패 위치 · 원인")});table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->setSelectionBehavior(QAbstractItemView::SelectRows);table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);table->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch);layout->addWidget(table,1);
  auto comparison=new QLabel;comparison->setObjectName("goalSaveCheckComparison");comparison->setWordWrap(true);layout->addWidget(comparison);
  auto bar=new QHBoxLayout;layout->addLayout(bar);auto cancel=new QPushButton(tr("검사 취소"));cancel->setObjectName("goalSaveCheckCancel");bar->addWidget(cancel);auto routes=new QPushButton(tr("목표 경로 기록 · 회귀 검사…"));routes->setObjectName("goalSaveCheckManual");bar->addWidget(routes);auto impact=new QPushButton(tr("저장 기준 비교 결과…"));impact->setObjectName("goalSaveCheckImpact");bar->addWidget(impact);bar->addStretch();auto close=new QPushButton(tr("닫기"));bar->addWidget(close);
  connect(cancel,&QPushButton::clicked,this,&StudioWindow::stopGoalSaveCheck);connect(routes,&QPushButton::clicked,this,&StudioWindow::goalRoutesDialog);connect(impact,&QPushButton::clicked,this,[this]{if(lastGoalSaveCheck.value("goalImpact").isObject())goalImpactDialog(lastGoalImpact);});connect(close,&QPushButton::clicked,dialog,&QDialog::close);
  auto render=[=,this](QJsonObject check){
    cancel->setEnabled(activeCheck(check)&&check.value("status")!="cancelling"&&goalSaveCheckCancelId.isEmpty());impact->setEnabled(check.value("goalImpact").isObject()&&!activeCheck(check));
    if(check.isEmpty()){summary->setText(tr("아직 저장 검사가 없습니다. 경로가 등록된 프로젝트를 저장하면 검사가 시작됩니다."));table->setRowCount(0);comparison->clear();return;}
    const auto reports=check.value("reports").toArray();int passed=0;
    for(auto value:reports)if(value.toObject().value("ok").toBool())++passed;
    auto text=tr("%1 · 처리 %2/%3 · 통과 %4 · 저장한 사본 r%5").arg(!goalSaveCheckCancelId.isEmpty()?tr("취소 중…"):checkStatus(check.value("status").toString())).arg(check.value("completed").toInt()).arg(reports.size()).arg(passed).arg(check.value("testedRevision").toInt());
    if(activeCheck(check)&&check.value("stage")=="comparison")text+="\n"+tr("경로 검사가 끝나 기준 기록과 비교하는 중입니다.");
    const bool inputsChanged=check.value("sourceVersion")!=view.value("draftVersion")||check.value("routesChanged").toBool()||check.value("baselinesChanged").toBool();
    if(inputsChanged||(check.value("stale").toBool()&&!goalImpactFatal(check.value("goalImpact").toObject())))text+="\n"+tr("검사 후 초안 또는 경로가 변경되었습니다. 이 결과는 이전 사본의 결과입니다.");
    if(!check.value("error").toString().isEmpty())text+="\n"+check.value("error").toString();
    summary->setText(text);summary->setToolTip(check.value("contentFingerprint").toString()+"\n"+check.value("file").toString());
    table->setRowCount(reports.size());
    for(int i=0;i<reports.size();++i){auto report=reports[i].toObject(),failure=report.value("failure").toObject();QStringList cells{report.value("name").toString(report.value("id").toString()),checkStatus(report.value("status").toString()),tr("%1 / %2").arg(report.value("executed").toInt()).arg(report.value("commands").toInt()),failure.value("message").toString()};for(int c=0;c<cells.size();++c){if(!table->item(i,c))table->setItem(i,c,new QTableWidgetItem);table->item(i,c)->setText(cells[c]);}table->item(i,0)->setToolTip(report.value("id").toString());table->item(i,3)->setToolTip(failure.value("code").toString());}
    const auto result=check.value("goalImpact").toObject(),counts=result.value("counts").toObject();
    if(result.isEmpty())comparison->setText(tr("기준 비교는 경로 검사와 별도입니다. 기준이 없거나 검사가 끝나지 않으면 호환성 통과로 볼 수 없습니다."));
    else {
      QStringList lines;
      const auto problem=goalImpactWarning(result);if(!problem.isEmpty())lines<<problem;
      if(!counts.isEmpty())lines<<tr("기준 비교 · 기준 없음 %1 · 변화 %2 · 실패 %3 · 미완료 %4 · 오류 %5").arg(counts.value("missing-baseline").toInt()).arg(counts.value("changed").toInt()).arg(counts.value("failed").toInt()).arg(counts.value("incomplete").toInt()).arg(counts.value("error").toInt()+result.value("baselineErrors").toArray().size());
      if(lines.isEmpty())lines<<tr("기준 비교 결과를 열어 확인하세요.");
      comparison->setText(lines.join("\n"));
    }
  };
  connect(this,&StudioWindow::goalSaveCheckReady,dialog,render);refreshGoalSaveCheck();dialog->show();
}
