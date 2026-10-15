#include "studio.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

void StudioWindow::editTimeSessions(){
  if(bridge.busy()||projectData().isEmpty())return;
  auto project=projectData();const auto revision=view.value("revision"),root=view.value("root");const auto timing=project.value("progression").toObject();
  QDialog dialog(this);dialog.setObjectName("timeSessionsDialog");dialog.setWindowTitle(tr("시간 · 행동력 · 예약 사건"));dialog.resize(1060,740);auto box=new QVBoxLayout(&dialog);
  auto enabled=new QCheckBox(tr("시간·진행 세션 사용"));enabled->setObjectName("timeEnabled");enabled->setChecked(true);box->addWidget(enabled);
  auto help=new QLabel(tr("월 일정과 독립된 게임 시간입니다. 직접 실행한 공통 행동에 적용됩니다. 시작 시 공용 행동력 차감, 완료 시 비용·효과 적용. 예약 사건 후 남은 시간을 이어갑니다. 취소 시 이미 사용한 행동력·시간은 반환하지 않습니다."));help->setWordWrap(true);box->addWidget(help);
  auto form=new QFormLayout;box->addLayout(form);auto unit=new QLineEdit(timing.value("unitLabel").toString(tr("시간")));unit->setMaxLength(20);unit->setObjectName("timeUnit");form->addRow(tr("시간 단위 이름"),unit);
  auto spin=[](int value,int min,int max,QString name){auto w=new QSpinBox;w->setRange(min,max);w->setValue(value);w->setObjectName(name);return w;};const auto ap=timing.value("actionPoints").toObject();
  auto max=spin(ap.value("max").toInt(10),1,1000000,"timeApMax"),initial=spin(ap.value("initial").toInt(10),0,1000000,"timeApInitial"),recovery=spin(ap.value("recoveryPerTick").toInt(1),0,1000000,"timeApRecovery");form->addRow(tr("최대 행동력"),max);form->addRow(tr("초기 행동력"),initial);form->addRow(tr("시간 단위당 행동력 회복"),recovery);
  auto actions=new QTableWidget(0,4);actions->setObjectName("timedActions");actions->setHorizontalHeaderLabels({tr("시간 사용"),tr("공통 행동"),tr("소요 시간 / 수량 1"),tr("행동력 / 수량 1")});actions->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);box->addWidget(actions);
  for(const auto &v:project.value("gameplay").toObject().value("actions").toArray()){
    const auto a=v.toObject();const auto row=actions->rowCount();actions->insertRow(row);QJsonObject rule;for(const auto &t:timing.value("actions").toArray())if(t.toObject().value("actionId")==a.value("id"))rule=t.toObject();
    auto check=new QCheckBox;check->setChecked(!rule.empty());actions->setCellWidget(row,0,check);auto label=new QTableWidgetItem(a.value("name").toString());label->setData(Qt::UserRole,a.value("id").toString());label->setFlags(label->flags()&~Qt::ItemIsEditable);actions->setItem(row,1,label);actions->setCellWidget(row,2,spin(rule.value("duration").toInt(1),1,1000000,"duration"));actions->setCellWidget(row,3,spin(rule.value("actionPointCost").toInt(1),0,1000000,"actionPointCost"));
  }
  auto appointments=new QTableWidget(0,4);appointments->setObjectName("timeAppointments");appointments->setHorizontalHeaderLabels({tr("예약 ID"),tr("사건"),tr("절대 게임 시간"),tr("개인 사건 실행자")});appointments->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);box->addWidget(appointments);
  auto addRow=[&](const QJsonObject &a){const int row=appointments->rowCount();appointments->insertRow(row);appointments->setItem(row,0,new QTableWidgetItem(a.value("id").toString("appointment-"+QString::number(row+1))));auto event=new QComboBox;for(const auto &v:view.value("documents").toArray()){const auto d=v.toObject(),data=d.value("data").toObject();if(d.value("kind")=="event"&&data.value("trigger")=="interaction")event->addItem(data.value("label").toString(),data.value("id").toString());}if(a.contains("eventId"))event->setCurrentIndex(event->findData(a.value("eventId").toString()));appointments->setCellWidget(row,1,event);appointments->setCellWidget(row,2,spin(a.value("atTick").toInt(),0,2147483647,"atTick"));auto actor=new QComboBox;actor->addItem(tr("공용 사건"),QString());for(const auto &m:project.value("roster").toObject().value("members").toArray()){const auto id=m.toObject().value("characterId").toString();actor->addItem(id,id);}actor->setCurrentIndex(qMax(0,actor->findData(a.value("actorId").toString())));appointments->setCellWidget(row,3,actor);};
  for(const auto &a:timing.value("appointments").toArray())addRow(a.toObject());
  auto rowButtons=new QHBoxLayout;box->addLayout(rowButtons);auto add=new QPushButton(tr("＋ 예약 사건"));add->setObjectName("addTimeAppointment");rowButtons->addWidget(add);connect(add,&QPushButton::clicked,&dialog,[&]{addRow({});});auto remove=new QPushButton(tr("선택 예약 삭제"));remove->setObjectName("removeTimeAppointment");rowButtons->addWidget(remove);connect(remove,&QPushButton::clicked,&dialog,[&]{if(appointments->currentRow()>=0)appointments->removeRow(appointments->currentRow());});
  auto error=new QLabel;error->setWordWrap(true);box->addWidget(error);auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);box->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
    if(view.value("revision")!=revision||view.value("root")!=root){error->setText(tr("프로젝트 버전이 바뀌었습니다. 창을 다시 여세요."));return;}
    if(enabled->isChecked()){
      if(unit->text().trimmed().isEmpty()||initial->value()>max->value()){error->setText(tr("시간 단위와 행동력 범위를 확인하세요."));return;}
      QJsonArray rules,reservations;for(int i=0;i<actions->rowCount();i++)if(qobject_cast<QCheckBox*>(actions->cellWidget(i,0))->isChecked())rules.append(QJsonObject{{"actionId",actions->item(i,1)->data(Qt::UserRole).toString()},{"duration",qobject_cast<QSpinBox*>(actions->cellWidget(i,2))->value()},{"actionPointCost",qobject_cast<QSpinBox*>(actions->cellWidget(i,3))->value()}});
      for(int i=0;i<appointments->rowCount();i++){if(appointments->item(i,0)->text().trimmed().isEmpty()||qobject_cast<QComboBox*>(appointments->cellWidget(i,1))->currentData().toString().isEmpty()){error->setText(tr("예약 %1행: 예약 ID와 사건을 선택하세요.").arg(i+1));return;}QJsonObject a{{"id",appointments->item(i,0)->text().trimmed()},{"eventId",qobject_cast<QComboBox*>(appointments->cellWidget(i,1))->currentData().toString()},{"atTick",qobject_cast<QSpinBox*>(appointments->cellWidget(i,2))->value()}};const auto actor=qobject_cast<QComboBox*>(appointments->cellWidget(i,3))->currentData().toString();if(!actor.isEmpty())a["actorId"]=actor;reservations.append(a);}
      project["progression"]=QJsonObject{{"unitLabel",unit->text().trimmed()},{"actionPoints",QJsonObject{{"max",max->value()},{"initial",initial->value()},{"recoveryPerTick",recovery->value()}}},{"actions",rules},{"appointments",reservations}};
    }else project.remove("progression");
    replaceProject(project);dialog.accept();
  });dialog.exec();
}
