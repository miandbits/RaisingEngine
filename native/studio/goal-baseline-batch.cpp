#include "studio.hpp"
#include <QDateTime>
#include <QDialog>
#include <QHeaderView>
#include <QPointer>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <memory>

namespace {
bool baselineBusy(const QJsonObject &job){const auto s=job.value("status").toString();return s=="queued"||s=="running"||s=="cancelling";}
QString baselineStatus(QString s){if(s=="passed")return QObject::tr("등록 완료");if(s=="completed")return QObject::tr("완료");if(s=="running")return QObject::tr("기록 생성 중…");if(s=="queued")return QObject::tr("대기 중");if(s=="cancelling")return QObject::tr("취소 중…");if(s=="cancelled")return QObject::tr("취소됨");if(s=="timed-out")return QObject::tr("시간 초과");if(s=="failed")return QObject::tr("실패");return QObject::tr("미실행");}
struct BaselineRegistration {QString id;QJsonObject pending;bool starting=false,closed=false,cancel=false,reported=false;};
}

void StudioWindow::goalBaselineBatchDialog(QString routeId,QString name){
  if(projectData().isEmpty())return;
  if(auto old=findChild<QDialog*>("goalBaselineBatchDialog")){old->show();old->raise();return;}
  auto dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setObjectName("goalBaselineBatchDialog");dialog->setWindowTitle(tr("기준 기록 일괄 등록"));dialog->resize(1100,680);
  const auto root=view.value("root");QPointer<QDialog> guard(dialog);auto job=std::make_shared<BaselineRegistration>();
  auto box=new QVBoxLayout(dialog);auto help=new QLabel(tr("선택한 경로를 현재 초안의 고정 사본에서 실행해 이름별 기준을 저장합니다. 편집은 가능하지만 초안을 변경하면 미완료 기준은 등록되지 않습니다. 이 창을 닫으면 미완료 작업을 취소하며 이미 등록한 기준은 남습니다. 기존 기준을 덮어쓰거나 승인하지 않습니다."));help->setWordWrap(true);box->addWidget(help);
  auto naming=new QHBoxLayout;box->addLayout(naming);naming->addWidget(new QLabel(tr("공통 이름 접두어")));auto prefix=new QLineEdit(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm"));prefix->setObjectName("goalBaselinePrefix");naming->addWidget(prefix,1);auto applyNames=new QPushButton(tr("선택한 이름에 적용"));applyNames->setObjectName("goalBaselineApplyNames");naming->addWidget(applyNames);
  auto actions=new QHBoxLayout;box->addLayout(actions);auto selectAll=new QPushButton(tr("모두 선택"));selectAll->setObjectName("goalBaselineSelectAll");auto selectNone=new QPushButton(tr("선택 해제"));selectNone->setObjectName("goalBaselineSelectNone");auto refresh=new QPushButton(tr("목록 새로고침"));refresh->setObjectName("goalBaselineRefresh");for(auto button:{selectAll,selectNone,refresh})actions->addWidget(button);actions->addStretch();
  auto table=new QTableWidget(0,6);table->setObjectName("goalBaselineRoutes");table->setHorizontalHeaderLabels({tr("선택"),tr("등록 경로"),tr("저장할 기준 이름"),tr("진행"),tr("생성한 기준 ID"),tr("저장 위치 · 원인")});table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch);table->horizontalHeader()->setSectionResizeMode(5,QHeaderView::Stretch);table->setSelectionBehavior(QAbstractItemView::SelectRows);box->addWidget(table,1);
  auto limits=new QHBoxLayout;box->addLayout(limits);limits->addWidget(new QLabel(tr("경로당 제한 (초)")));auto routeSeconds=new QSpinBox;routeSeconds->setObjectName("goalBaselineRouteSeconds");routeSeconds->setRange(1,300);routeSeconds->setValue(300);limits->addWidget(routeSeconds);limits->addWidget(new QLabel(tr("전체 제한 (초)")));auto batchSeconds=new QSpinBox;batchSeconds->setObjectName("goalBaselineBatchSeconds");batchSeconds->setRange(1,3600);batchSeconds->setValue(3600);limits->addWidget(batchSeconds);limits->addStretch();
  auto summary=new QLabel(tr("경로와 기준 이름을 확인한 뒤 등록을 시작하세요. 이름은 1~120자입니다."));summary->setObjectName("goalBaselineSummary");summary->setWordWrap(true);summary->setTextInteractionFlags(Qt::TextSelectableByMouse);box->addWidget(summary);
  auto note=new QLabel(tr("등록 완료된 기준만 다음 저장의 비교에 사용합니다. 실패·취소·시간 초과·미실행은 등록 성공이 아닙니다. 다른 경로 검사가 진행 중이면 완료하거나 취소한 뒤 시작하세요."));note->setWordWrap(true);box->addWidget(note);
  auto footer=new QHBoxLayout;box->addLayout(footer);auto start=new QPushButton(tr("선택한 기준 등록 시작"));start->setObjectName("goalBaselineStart");footer->addWidget(start);auto cancel=new QPushButton(tr("등록 취소"));cancel->setObjectName("goalBaselineCancel");cancel->setEnabled(false);footer->addWidget(cancel);auto checks=new QPushButton(tr("저장 검사 진행 · 결과…"));checks->setObjectName("goalBaselineSavedChecks");footer->addWidget(checks);footer->addStretch();auto close=new QPushButton(tr("닫기 · 미완료 등록 취소"));footer->addWidget(close);
  auto polling=new QTimer(dialog);polling->setObjectName("goalBaselinePolling");polling->setInterval(500);auto loading=new QTimer(dialog);loading->setObjectName("goalBaselineLoading");loading->setInterval(200);
  auto setRunning=[=](bool running){for(auto button:{start,selectAll,selectNone,refresh,applyNames})button->setEnabled(!running);prefix->setEnabled(!running);for(int row=0;row<table->rowCount();++row){table->item(row,0)->setFlags(Qt::ItemIsSelectable|(running?Qt::NoItemFlags:Qt::ItemIsEnabled|Qt::ItemIsUserCheckable));table->item(row,2)->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|(running?Qt::NoItemFlags:Qt::ItemIsEditable));}routeSeconds->setEnabled(!running);batchSeconds->setEnabled(!running);cancel->setEnabled(running&&!job->cancel);};
  auto render=[=,this](QJsonObject result){
    if(!guard)return;lastGoalBaselineBatch=result;const bool running=baselineBusy(result);setRunning(running);const auto reports=result.value("reports").toArray();int saved=0;
    for(auto value:reports){auto report=value.toObject(),baseline=report.value("baseline").toObject(),failure=report.value("failure").toObject();if(report.value("ok").toBool()&&!baseline.isEmpty())++saved;
      for(int row=0;row<table->rowCount();++row){if(table->item(row,0)->data(Qt::UserRole).toString()!=report.value("id").toString())continue;QString text=baselineStatus(report.value("status").toString());if(report.value("status")=="running")text+=" · "+(report.value("captureStage")=="details"?tr("상세 기록 %1/%2"):tr("경로 재생 %1/%2")).arg(report.value("executed").toInt()).arg(report.value("commands").toInt());table->item(row,3)->setText(text);table->item(row,4)->setText(baseline.value("id").toString());const auto detail=baseline.value("file").toString(failure.value("message").toString());table->item(row,5)->setText(detail);table->item(row,5)->setToolTip(detail);}}
    QString text=tr("%1 · 처리 %2/%3 · 기준 등록 %4 · 초안 r%5").arg(job->cancel&&running?tr("취소 중…"):baselineStatus(result.value("status").toString())).arg(result.value("completed").toInt()).arg(reports.size()).arg(saved).arg(result.value("testedRevision").toInt());
    if(result.value("stale").toBool()||result.value("sourceVersion")!=view.value("draftVersion"))text+="\n"+tr("초안이 변경되어 미완료 기준을 등록할 수 없습니다. 완료된 기록은 남습니다. 취소한 뒤 최신 초안에서 다시 시작하세요.");
    if(!result.value("error").toString().isEmpty())text+="\n"+result.value("error").toString();
    if(!running&&saved<reports.size())text+="\n"+tr("일부 또는 모든 기준이 등록되지 않았습니다. 완료된 기준은 보존됩니다.");summary->setText(text);
    if(!running){polling->stop();job->cancel=false;if(!job->reported){job->reported=true;emit goalBaselinesReady(result);}}
  };
  // Closing the tool retires exactly its job, even if a status/start response is
  // in flight. A subsequent tool's job can never be cancelled by this callback.
  auto cancelOwned=[this,root](QString id){if(id.isEmpty())return;auto retry=new QTimer(this);retry->setInterval(100);connect(retry,&QTimer::timeout,this,[this,root,id,retry]{if(root!=view.value("root")){retry->deleteLater();return;}if(bridge.busy()||bridge.backgroundBusy())return;bridge.requestBackground("goalBaselineBatchStop",{{"root",root},{"jobId",id}},[](QJsonObject){});retry->deleteLater();});retry->start();};
  auto fail=[=](QString message){if(!guard)return;job->starting=false;job->cancel=false;polling->stop();setRunning(false);summary->setText(message);};
  connect(polling,&QTimer::timeout,dialog,[=,this]{
    if(!guard||job->closed||root!=view.value("root")||bridge.busy()||bridge.backgroundBusy())return;
    const bool starting=job->starting;const QString requestedId=job->id;QString method;QJsonObject params;
    if(starting){method="goalBaselineBatchStart";params=job->pending;job->starting=false;}
    else {if(requestedId.isEmpty())return;method=job->cancel?"goalBaselineBatchStop":"goalBaselineBatchStatus";params={{"root",root},{"jobId",requestedId}};}
    bridge.requestBackground(method,params,[=,this](QJsonObject reply){auto result=reply.value("goalBaselineBatch").toObject();if(!guard||job->closed){if(starting)cancelOwned(result.value("id").toString());return;}if(root!=view.value("root"))return;
      if(!starting&&job->id!=requestedId)return;if(!reply.value("error").toString().isEmpty()){fail(reply.value("error").toString());return;}if(result.isEmpty()){fail(tr("기준 등록 결과를 받지 못했습니다. 다시 확인하세요."));return;}
      if(starting)job->id=result.value("id").toString();if(job->cancel&&baselineBusy(result)){setRunning(true);summary->setText(tr("취소 중…"));}render(result);
    });
  });
  connect(loading,&QTimer::timeout,dialog,[=,this]{if(!guard||root!=view.value("root")||bridge.busy()||bridge.backgroundBusy())return;loading->stop();start->setEnabled(false);bridge.requestBackground("goalRoutesList",{{"revision",view.value("revision")}},[=,this](QJsonObject reply){if(!guard||root!=view.value("root"))return;start->setEnabled(true);if(!reply.value("error").toString().isEmpty()){summary->setText(reply.value("error").toString());return;}table->setRowCount(0);
    for(auto value:reply.value("goalRoutes").toArray()){auto entry=value.toObject(),route=entry.value("route").toObject();if(route.isEmpty())continue;const int row=table->rowCount();table->insertRow(row);auto selected=new QTableWidgetItem;selected->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|Qt::ItemIsUserCheckable);selected->setCheckState(routeId.isEmpty()||route.value("id")==routeId?Qt::Checked:Qt::Unchecked);selected->setData(Qt::UserRole,route.value("id").toString());selected->setData(Qt::UserRole+1,entry.value("revision").toVariant());table->setItem(row,0,selected);
      const auto label=route.value("name").toString(route.value("id").toString());QStringList cells{label,route.value("id")==routeId&&!name.trimmed().isEmpty()?name:prefix->text()+" · "+label,tr("미실행"),QString(),QString()};for(int c=0;c<cells.size();++c){auto item=new QTableWidgetItem(cells[c]);item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|(c==1?Qt::ItemIsEditable:Qt::NoItemFlags));table->setItem(row,c+1,item);}table->item(row,1)->setToolTip(route.value("id").toString());}
    summary->setText(tr("경로와 기준 이름을 확인한 뒤 등록을 시작하세요. 이름은 1~120자입니다."));start->setEnabled(table->rowCount()>0);
  });});
  connect(refresh,&QPushButton::clicked,dialog,[=]{loading->start();});connect(selectAll,&QPushButton::clicked,dialog,[=]{for(int i=0;i<table->rowCount();++i)table->item(i,0)->setCheckState(Qt::Checked);});connect(selectNone,&QPushButton::clicked,dialog,[=]{for(int i=0;i<table->rowCount();++i)table->item(i,0)->setCheckState(Qt::Unchecked);});
  connect(applyNames,&QPushButton::clicked,dialog,[=]{for(int i=0;i<table->rowCount();++i)if(table->item(i,0)->checkState()==Qt::Checked)table->item(i,2)->setText(prefix->text().trimmed().isEmpty()?table->item(i,1)->text():prefix->text().trimmed()+" · "+table->item(i,1)->text());});
  connect(start,&QPushButton::clicked,dialog,[=,this]{if(root!=view.value("root")||polling->isActive())return;QJsonArray ids;QJsonObject names,revisions;for(int i=0;i<table->rowCount();++i){if(table->item(i,0)->checkState()!=Qt::Checked)continue;const auto id=table->item(i,0)->data(Qt::UserRole).toString(),value=table->item(i,2)->text().trimmed();if(value.isEmpty()||value.size()>120){summary->setText(tr("각 기준 이름은 1~120자로 입력하세요."));return;}ids.append(id);names[id]=value;revisions[id]=QJsonValue::fromVariant(table->item(i,0)->data(Qt::UserRole+1));}if(ids.isEmpty()){summary->setText(tr("등록할 경로를 하나 이상 선택하세요."));return;}
    job->id.clear();job->reported=false;job->cancel=false;job->starting=true;job->pending={{"revision",view.value("revision")},{"ids",ids},{"names",names},{"routeRevisions",revisions},{"limits",QJsonObject{{"routeTimeoutMs",routeSeconds->value()*1000},{"batchTimeoutMs",batchSeconds->value()*1000}}}};loading->stop();setRunning(true);summary->setText(tr("현재 초안을 준비하는 중…"));polling->start();QMetaObject::invokeMethod(polling,"timeout",Qt::QueuedConnection);
  });
  connect(cancel,&QPushButton::clicked,dialog,[=]{job->cancel=true;cancel->setEnabled(false);summary->setText(tr("취소 중…"));if(job->starting){job->starting=false;polling->stop();setRunning(false);summary->setText(tr("취소됨"));}else QMetaObject::invokeMethod(polling,"timeout",Qt::QueuedConnection);});
  connect(checks,&QPushButton::clicked,this,&StudioWindow::goalSaveChecksDialog);connect(close,&QPushButton::clicked,dialog,&QDialog::close);
  connect(this,&StudioWindow::updated,dialog,[=,this]{if(root!=view.value("root"))dialog->close();else if(!job->id.isEmpty()&&lastGoalBaselineBatch.value("id")==job->id)render(lastGoalBaselineBatch);});
  connect(dialog,&QDialog::finished,this,[=](int){job->closed=true;loading->stop();polling->stop();if(!job->reported)cancelOwned(job->id);});
  dialog->show();loading->start();
}
