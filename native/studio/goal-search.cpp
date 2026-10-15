#include "studio.hpp"
#include <QDialog>
#include <QFormLayout>
#include <QPushButton>
#include <QPointer>
#include <QTimer>
#include <QDateTime>
#include <QVBoxLayout>
#include <memory>

void StudioWindow::goalSearchDialog(QJsonObject source){
  if(bridge.busy()||projectData().isEmpty())return;
  if(auto old=findChild<QDialog*>("goalSearchDialog")){old->show();old->raise();return;}
  auto dialog=new QDialog(this);dialog->setObjectName("goalSearchDialog");dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle(tr("목표 도달 경로 자동 탐색"));dialog->resize(860,720);
  QPointer<QDialog> guard(dialog);const auto root=view.value("root");auto valid=[=,this]{return guard&&root==view.value("root")&&!bridge.busy();};
  auto box=new QVBoxLayout(dialog);auto help=new QLabel(tr("목표를 고르면 실행 가능한 명령으로 도달 경로를 탐색합니다. 발견 경로는 현재 콘텐츠로 재생·중간 저장/복원을 확인한 뒤 등록합니다.\n사건 목표는 해당 사건의 시작에 도달하는 검사입니다. 탐색 실패는 도달 불가능의 증명이 아닙니다. 원본 경로는 덮어쓰지 않습니다."));help->setWordWrap(true);box->addWidget(help);
  auto form=new QFormLayout;box->addLayout(form);auto target=new QComboBox;target->setObjectName("goalSearchTarget");
  for(auto v:view.value("documents").toArray()){auto d=v.toObject();auto kind=d.value("kind").toString();if(kind!="ending"&&kind!="event")continue;auto data=d.value("data").toObject();target->addItem((kind=="event"?tr("사건 · "):tr("엔딩 · "))+data.value("label").toString(data.value("id").toString())+" ("+data.value("id").toString()+")",QJsonObject{{"kind",kind},{"id",data.value("id")}});}
  auto original=source.value("route").toObject();if(!original.isEmpty()){auto goal=original.value("target").toObject();if(goal.isEmpty())goal={{"kind","ending"},{"id",original.value("endingId")}};target->setCurrentIndex(target->findData(goal));target->setEnabled(false);}
  form->addRow(tr("목표 엔딩 / 사건"),target);
  auto seed=new QSpinBox;seed->setObjectName("goalSearchSeed");seed->setRange(0,2147483647);seed->setValue(42);form->addRow(tr("시작 seed (새 경로는 연속 4개 검사)"),seed);
  auto states=new QSpinBox;states->setObjectName("goalSearchStates");states->setRange(1,100000);states->setValue(4000);form->addRow(tr("최대 탐색 상태 수"),states);
  auto seconds=new QSpinBox;seconds->setObjectName("goalSearchSeconds");seconds->setRange(1,120);seconds->setValue(30);form->addRow(tr("시간 제한 (초)"),seconds);
  auto commands=new QSpinBox;commands->setObjectName("goalSearchCommands");commands->setRange(1,10000);commands->setValue(1000);form->addRow(tr("경로당 최대 명령"),commands);
  auto width=new QSpinBox;width->setObjectName("goalSearchWidth");width->setRange(1,64);width->setValue(24);form->addRow(tr("탐색 폭"),width);
  auto id=new QLineEdit((original.isEmpty()?QString("found"):original.value("id").toString().left(30)+"-alt")+"-"+QString::number(QDateTime::currentMSecsSinceEpoch()));id->setObjectName("goalSearchId");auto name=new QLineEdit(original.isEmpty()?tr("자동 탐색 경로"):original.value("name").toString()+tr(" · 대체 경로"));name->setObjectName("goalSearchName");form->addRow(tr("새 경로 ID"),id);form->addRow(tr("이름"),name);
  auto bar=new QHBoxLayout;box->addLayout(bar);auto start=new QPushButton(original.isEmpty()?tr("경로 찾기"):tr("대체 경로 찾기"));start->setObjectName("goalSearchStart");auto stop=new QPushButton(tr("취소"));stop->setObjectName("goalSearchStop");stop->setEnabled(false);auto save=new QPushButton(tr("발견 경로를 회귀 테스트로 등록"));save->setObjectName("goalSearchRegister");save->setEnabled(false);bar->addWidget(start);bar->addWidget(stop);bar->addWidget(save);
  auto compare=new QPushButton(tr("발견 경로 그래프 · 재생 / 비교"));compare->setObjectName("goalSearchCompare");compare->setEnabled(false);box->addWidget(compare);
  auto summary=new QLabel(original.isEmpty()?tr("목표를 선택하고 경로 찾기를 누르세요."):tr("원본 %1 보존 · 원본 활동/행동 변경을 먼저 시도하고 새 경로도 탐색합니다.").arg(original.value("id").toString()));summary->setObjectName("goalSearchSummary");summary->setWordWrap(true);box->addWidget(summary);auto details=new QPlainTextEdit;details->setObjectName("goalSearchDetails");details->setReadOnly(true);box->addWidget(details);
  auto jobId=std::make_shared<QString>();auto searchRevision=std::make_shared<int>(-1);auto timer=new QTimer(dialog);timer->setInterval(500);
  auto show=[=,this](QJsonObject job){if(!guard)return;auto status=job.value("status").toString();auto progress=job.value("progress").toObject();bool running=status=="running",stale=job.value("revision")!=view.value("revision");start->setEnabled(!running);stop->setEnabled(running);save->setEnabled(status=="found"&&!stale);compare->setEnabled(status=="found"&&!stale);target->setEnabled(!running&&original.isEmpty());for(auto field:{seed,states,seconds,commands,width})field->setEnabled(!running);if(!running)timer->stop();
    if(running)summary->setText(tr("탐색 중 · 기존 경로 변형 %1개 · 탐색 상태 %2개").arg(progress.value("attemptedRoutes").toInt()).arg(progress.value("exploredStates").toInt()));
    else if(status=="found")summary->setText(stale?tr("탐색 후 초안이 변경되었습니다. 현재 초안에서 다시 탐색하세요."):tr("목표 %1 도달 · %2명령 · 저장/복원 %3회 통과 · seed %4.\n새 경로 ID로 등록할 수 있습니다. 참고한 기록: %5").arg(job.value("target").toObject().value("id").toString()).arg(job.value("commands").toInt()).arg(job.value("result").toObject().value("report").toObject().value("saves").toInt()).arg(job.value("seed").toDouble()).arg(job.value("sourceRouteId").toString(tr("없음 — 새 경로"))));
    else {summary->setText(status=="cancelled"?tr("탐색 취소 · 기존 경로는 보존됩니다."):tr("검증된 경로를 찾지 못했습니다. 도달 불가능 판정이 아닙니다.\n")+job.value("error").toString()+job.value("result").toObject().value("message").toString());}
    details->setPlainText(QString::fromUtf8(QJsonDocument(job).toJson()));};
  connect(start,&QPushButton::clicked,dialog,[=,this]{if(!valid())return;QJsonObject options{{"seed",seed->value()},{"maxStates",states->value()},{"timeoutMs",seconds->value()*1000},{"maxCommands",commands->value()},{"beamWidth",width->value()}};QJsonObject params{{"target",target->currentData().toJsonObject()},{"options",options}};if(!original.isEmpty()){params["sourceId"]=original.value("id");params["sourceRevision"]=source.value("revision");}save->setEnabled(false);compare->setEnabled(false);start->setEnabled(false);summary->setText(tr("현재 초안을 준비하는 중…"));request("goalSearchStart",params,[=](QJsonObject reply){if(!guard)return;auto job=reply.value("goalSearch").toObject();if(job.isEmpty()){start->setEnabled(true);summary->setText(reply.value("error").toString());return;}*jobId=job.value("id").toString();*searchRevision=job.value("revision").toInt();show(job);timer->start();});});
  connect(timer,&QTimer::timeout,dialog,[=,this]{if(!valid()||jobId->isEmpty())return;request("goalSearchStatus",{{"root",root},{"jobId",*jobId}},[=](QJsonObject reply){if(!guard)return;auto job=reply.value("goalSearch").toObject();if(job.isEmpty()){timer->stop();summary->setText(reply.value("error").toString());start->setEnabled(true);stop->setEnabled(false);return;}show(job);});});
  connect(stop,&QPushButton::clicked,dialog,[=,this]{if(valid()&&!jobId->isEmpty())request("goalSearchStop",{{"root",root},{"jobId",*jobId}},[=](QJsonObject reply){show(reply.value("goalSearch").toObject());});});
  connect(compare,&QPushButton::clicked,dialog,[=,this]{if(valid()&&!jobId->isEmpty())goalCompareDialog(*jobId);});
  connect(save,&QPushButton::clicked,dialog,[=,this]{if(!valid())return;if(*searchRevision!=view.value("revision").toInt()){save->setEnabled(false);summary->setText(tr("초안이 변경되었습니다. 다시 탐색하세요."));return;}request("goalSearchRegister",{{"jobId",*jobId},{"id",id->text().trimmed()},{"name",name->text().trimmed()}},[=,this](QJsonObject reply){if(!guard)return;if(!reply.contains("goalSearchRegistered")){summary->setText(reply.value("error").toString());return;}save->setEnabled(false);summary->setText(tr("회귀 테스트 등록 완료 · 프로젝트 저장 시 자동 검사됩니다. 원본 경로는 보존되었습니다."));details->setPlainText(QString::fromUtf8(QJsonDocument(reply.value("goalSearchRegistered").toObject()).toJson()));if(auto refresh=findChild<QPushButton*>("goalRefresh"))refresh->click();});});
  connect(this,&StudioWindow::updated,dialog,[=,this]{if(root!=view.value("root")){timer->stop();dialog->close();}else if(*searchRevision>=0&&*searchRevision!=view.value("revision").toInt()){save->setEnabled(false);compare->setEnabled(false);}});
  auto changed=[=]{if(save->isEnabled()||compare->isEnabled()){save->setEnabled(false);compare->setEnabled(false);summary->setText(tr("목표 또는 탐색 설정이 변경되었습니다. 다시 탐색하세요."));}};
  connect(target,&QComboBox::currentIndexChanged,dialog,[=](int){changed();});for(auto field:{seed,states,seconds,commands,width})connect(field,&QSpinBox::valueChanged,dialog,[=](int){changed();});
  connect(dialog,&QDialog::finished,this,[=,this](int){if(!bridge.busy()&&!jobId->isEmpty())request("goalSearchStop",{{"root",root},{"jobId",*jobId}});});
  dialog->show();
}
