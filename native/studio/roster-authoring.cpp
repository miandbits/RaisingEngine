#include "studio.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

void StudioWindow::editRoster() {
  if(bridge.busy())return;
  const auto revision=view.value("revision"),root=view.value("root");
  auto project=projectData();const auto previous=project["roster"].toObject();
  QJsonArray attrs,characters,activities;QMap<QString,QString> files;
  for(const auto &v:view["documents"].toArray()) {
    const auto d=v.toObject(),data=d["data"].toObject();
    if(d["kind"]=="attributes")attrs=data["attributes"].toArray();
    if(d["kind"]=="character"){characters.append(data);files[data["id"].toString()]=d["file"].toString();}
    if(d["kind"]=="activity"&&!data.contains("expeditionId"))activities.append(data);
  }
  QDialog dialog(this);dialog.setObjectName("rosterAuthoringDialog");dialog.setWindowTitle(tr("다인 육성 · 대상과 활동 배정"));dialog.resize(1080,670);
  auto layout=new QVBoxLayout(&dialog);
  auto enabled=new QCheckBox(tr("다인 육성 사용 (기존 프로젝트는 기본 꺼짐)"));enabled->setObjectName("rosterEnabled");enabled->setChecked(true);layout->addWidget(enabled);
  auto help=new QLabel(tr("체크한 능력은 캐릭터별로 관리하고, 나머지는 공유합니다. 첫 행은 주인공입니다. 활동은 같은 기간에 순서대로 정산되며 실패하면 모두 취소됩니다. 현재 배정은 한 기간 단위입니다."));help->setWordWrap(true);layout->addWidget(help);
  auto locals=new QListWidget;locals->setObjectName("rosterLocalAttributes");locals->setMaximumHeight(125);
  for(const auto &v:attrs){const auto a=v.toObject();auto item=new QListWidgetItem(a["label"].toString()+" · "+a["id"].toString(),locals);item->setData(Qt::UserRole,a["id"].toString());item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState((previous.isEmpty()?a["kind"]!="resource":previous["localAttributeIds"].toArray().contains(a["id"]))?Qt::Checked:Qt::Unchecked);}layout->addWidget(locals);
  auto table=new QTableWidget(0,3+attrs.size());table->setObjectName("rosterMembers");QStringList headers{tr("인물 ID"),tr("이름"),tr("기본 활동")};for(const auto &v:attrs)headers<<v.toObject()["label"].toString();table->setHorizontalHeaderLabels(headers);table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);layout->addWidget(table);
  auto addMember=[&](QJsonObject member){const int row=table->rowCount();table->insertRow(row);auto id=member["characterId"].toString();QString name=id;for(const auto &v:characters)if(v.toObject()["id"]==id)name=v.toObject()["name"].toString();
    table->setItem(row,0,new QTableWidgetItem(id));if(row==0)table->item(row,0)->setFlags(table->item(row,0)->flags()&~Qt::ItemIsEditable);table->setItem(row,1,new QTableWidgetItem(name));
    auto activity=new QComboBox;activity->setObjectName("rosterActivity"+QString::number(row));for(const auto &v:activities){auto a=v.toObject();activity->addItem(a["label"].toString(),a["id"].toString());}auto ai=activity->findData(member["activityId"].toString());if(ai>=0)activity->setCurrentIndex(ai);table->setCellWidget(row,2,activity);
    for(int col=0;col<attrs.size();col++){auto a=attrs[col].toObject();auto spin=new QSpinBox;spin->setRange(a["min"].toInt(),a["max"].toInt());spin->setValue(member["initialAttributes"].toObject().value(a["id"].toString()).toInt(project["initialState"].toObject()["attributes"].toObject().value(a["id"].toString()).toInt(a["initial"].toInt())));spin->setEnabled(locals->item(col)->checkState()==Qt::Checked);spin->setToolTip(tr("개별 능력으로 체크한 값만 캐릭터별 초기값에 적용됩니다. 공용 값은 프로젝트 초기 상태에서 편집하세요."));table->setCellWidget(row,col+3,spin);}
  };
  auto members=previous["members"].toArray();if(members.isEmpty())members.append(QJsonObject{{"characterId",project["protagonistId"]}});for(const auto &m:members)addMember(m.toObject());
  table->horizontalHeader()->setMinimumSectionSize(80);
  for(int col=0;col<3;col++)table->horizontalHeader()->setSectionResizeMode(col,QHeaderView::Interactive);
  table->setColumnWidth(0,140);table->setColumnWidth(1,110);table->setColumnWidth(2,170);
  connect(locals,&QListWidget::itemChanged,&dialog,[&](QListWidgetItem *item){const int col=locals->row(item)+3;for(int row=0;row<table->rowCount();row++)table->cellWidget(row,col)->setEnabled(item->checkState()==Qt::Checked);});
  auto rowButtons=new QHBoxLayout;auto add=new QPushButton(tr("＋ 육성 대상"));add->setObjectName("rosterAddMember");auto remove=new QPushButton(tr("선택 대상 삭제"));rowButtons->addWidget(add);rowButtons->addWidget(remove);rowButtons->addStretch();layout->addLayout(rowButtons);
  connect(add,&QPushButton::clicked,&dialog,[&]{if(table->rowCount()>=16)return;addMember(QJsonObject{{"characterId","trainee-"+QString::number(table->rowCount()+1)}});});
  connect(remove,&QPushButton::clicked,&dialog,[&]{if(table->currentRow()>0)table->removeRow(table->currentRow());});
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);
  connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
    if(view["revision"]!=revision||view["root"]!=root){QMessageBox::warning(&dialog,tr("버전 충돌"),tr("프로젝트가 바뀌었습니다. 창을 다시 열어 주세요."));return;}
    QJsonArray edits,ids,values;for(int i=0;i<locals->count();i++)if(locals->item(i)->checkState()==Qt::Checked)ids.append(locals->item(i)->data(Qt::UserRole).toString());
    if(enabled->isChecked()&&ids.isEmpty()){QMessageBox::warning(&dialog,tr("개별 능력"),tr("개별 능력을 하나 이상 선택하세요."));return;}
    QSet<QString> seen;
    if(enabled->isChecked())for(int row=0;row<table->rowCount();row++){
      const auto id=table->item(row,0)->text().trimmed(),name=table->item(row,1)->text().trimmed();
      if(id.size()>64||!QRegularExpression("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$").match(id).hasMatch()||name.isEmpty()||seen.contains(id)){QMessageBox::warning(&dialog,tr("대상 오류"),tr("ID는 영문 소문자로 시작하는 영문·숫자·하이픈 64자 이내여야 하며 중복될 수 없습니다. 이름도 입력하세요."));return;}seen.insert(id);
      if(qobject_cast<QComboBox*>(table->cellWidget(row,2))->currentIndex()<0){QMessageBox::warning(&dialog,tr("활동 없음"),tr("먼저 활동 콘텐츠를 하나 이상 만드세요."));return;}
      QJsonObject initial;for(int col=0;col<attrs.size();col++){const auto key=attrs[col].toObject()["id"].toString();if(ids.contains(key))initial[key]=qobject_cast<QSpinBox*>(table->cellWidget(row,col+3))->value();}
      values.append(QJsonObject{{"characterId",id},{"initialAttributes",initial},{"activityId",qobject_cast<QComboBox*>(table->cellWidget(row,2))->currentData().toString()}});
      QJsonObject character;for(const auto &v:characters)if(v.toObject()["id"]==id)character=v.toObject();
      if(character.isEmpty())character=QJsonObject{{"formatVersion",project["formatVersion"]},{"id",id},{"name",name},{"portraits",QJsonObject{}},{"relationships",QJsonArray{}}};else character["name"]=name;
      const auto file=files.value(id,"characters/"+id+".json");if(!files.contains(id))edits.append(QJsonObject{{"type","add"},{"kind","character"},{"id",id}});edits.append(QJsonObject{{"type","replace"},{"file",file},{"data",character}});
    }
    if(enabled->isChecked())project["roster"]=QJsonObject{{"localAttributeIds",ids},{"members",values}};else project.remove("roster");
    edits.append(QJsonObject{{"type","replace"},{"file",projectFile},{"data",project}});
    request("edit",{{"edit",QJsonObject{{"type","batch"},{"edits",edits}}}});dialog.accept();
  });dialog.exec();
}
