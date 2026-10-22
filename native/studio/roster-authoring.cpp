#include "studio.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
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
  auto scopeHelp=new QLabel(tr("기존 인물은 이름으로 찾아 추가하세요. 이름 변경은 해당 인물 콘텐츠에도 반영됩니다. 대상에서 제외해도 인물 콘텐츠는 보존됩니다."));scopeHelp->setWordWrap(true);layout->addWidget(scopeHelp);
  auto locals=new QListWidget;locals->setObjectName("rosterLocalAttributes");locals->setMaximumHeight(125);
  for(const auto &v:attrs){const auto a=v.toObject();auto item=new QListWidgetItem(a["label"].toString(),locals);item->setData(Qt::UserRole,a["id"].toString());item->setToolTip(a["id"].toString());item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState((previous.isEmpty()?a["kind"]!="resource":previous["localAttributeIds"].toArray().contains(a["id"]))?Qt::Checked:Qt::Unchecked);}layout->addWidget(locals);
  auto sharedSummary=new QLabel;sharedSummary->setObjectName("rosterSharedSummary");sharedSummary->setWordWrap(true);layout->addWidget(sharedSummary);
  auto table=new QTableWidget(0,3+attrs.size());table->setObjectName("rosterMembers");QStringList headers{tr("인물 ID"),tr("이름"),tr("기본 활동")};for(const auto &v:attrs)headers<<v.toObject()["label"].toString();table->setHorizontalHeaderLabels(headers);table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);layout->addWidget(table);
  auto addMember=[&](QJsonObject member){const int row=table->rowCount();table->insertRow(row);auto id=member["characterId"].toString();QString name=id;for(const auto &v:characters)if(v.toObject()["id"]==id)name=v.toObject()["name"].toString();
    table->setItem(row,0,new QTableWidgetItem(id));table->item(row,0)->setFlags(table->item(row,0)->flags()&~Qt::ItemIsEditable);table->item(row,0)->setToolTip(tr("참조를 보존하기 위해 여기서는 ID를 바꾸지 않습니다. 콘텐츠 편집기의 ID 변경을 사용하세요."));table->setItem(row,1,new QTableWidgetItem(member.value("name").toString(name)));
    auto activity=new QComboBox;activity->setObjectName("rosterActivity"+QString::number(row));for(const auto &v:activities){auto a=v.toObject();activity->addItem(a["label"].toString(),a["id"].toString());}auto ai=activity->findData(member["activityId"].toString());if(ai>=0)activity->setCurrentIndex(ai);table->setCellWidget(row,2,activity);
    for(int col=0;col<attrs.size();col++){auto a=attrs[col].toObject();auto spin=new QSpinBox;spin->setRange(a["min"].toInt(),a["max"].toInt());spin->setValue(member["initialAttributes"].toObject().value(a["id"].toString()).toInt(project["initialState"].toObject()["attributes"].toObject().value(a["id"].toString()).toInt(a["initial"].toInt())));spin->setEnabled(locals->item(col)->checkState()==Qt::Checked);spin->setToolTip(tr("개별 능력으로 체크한 값만 캐릭터별 초기값에 적용됩니다. 공용 값은 프로젝트 초기 상태에서 편집하세요."));table->setCellWidget(row,col+3,spin);}
  };
  auto members=previous["members"].toArray();if(members.isEmpty())members.append(QJsonObject{{"characterId",project["protagonistId"]}});for(const auto &m:members)addMember(m.toObject());
  table->horizontalHeader()->setMinimumSectionSize(80);
  for(int col=0;col<3;col++)table->horizontalHeader()->setSectionResizeMode(col,QHeaderView::Interactive);
  table->setColumnWidth(0,140);table->setColumnWidth(1,110);table->setColumnWidth(2,170);
  auto refreshScope=[&]{QStringList shared;for(int i=0;i<locals->count();++i){const bool local=locals->item(i)->checkState()==Qt::Checked;table->setColumnHidden(i+3,!local);for(int row=0;row<table->rowCount();++row)table->cellWidget(row,i+3)->setEnabled(local);if(!local)shared<<attrs[i].toObject().value("label").toString();}sharedSummary->setText(tr("공유 값: %1 · 프로젝트 설정에서 초기값을 편집합니다.").arg(shared.isEmpty()?tr("없음"):shared.join(" · ")));};
  connect(locals,&QListWidget::itemChanged,&dialog,[&](QListWidgetItem*){refreshScope();});refreshScope();
  auto existingRow=new QHBoxLayout;auto existing=new QComboBox;existing->setObjectName("rosterExistingCharacter");existing->setEditable(true);existing->setInsertPolicy(QComboBox::NoInsert);existing->lineEdit()->setPlaceholderText(tr("기존 인물 이름 검색"));existing->completer()->setCaseSensitivity(Qt::CaseInsensitive);existing->completer()->setFilterMode(Qt::MatchContains);existingRow->addWidget(existing,1);auto addExisting=new QPushButton(tr("선택한 인물 추가"));addExisting->setObjectName("rosterAddExisting");existingRow->addWidget(addExisting);layout->addLayout(existingRow);
  auto rowButtons=new QHBoxLayout;auto add=new QPushButton(tr("＋ 새 인물 만들기"));add->setObjectName("rosterAddMember");auto remove=new QPushButton(tr("육성 대상에서 제외"));remove->setObjectName("rosterRemoveMember");rowButtons->addWidget(add);rowButtons->addWidget(remove);rowButtons->addStretch();auto showIds=new QCheckBox(tr("참조 ID 표시"));showIds->setObjectName("rosterShowIds");rowButtons->addWidget(showIds);layout->addLayout(rowButtons);table->setColumnHidden(0,true);
  connect(showIds,&QCheckBox::toggled,&dialog,[=](bool checked){table->setColumnHidden(0,!checked);});
  auto usedIds=[&]{QSet<QString> ids;for(int row=0;row<table->rowCount();++row)ids.insert(table->item(row,0)->text());return ids;};
  auto refreshExisting=[&]{const auto selected=existing->currentData().toString();const auto used=usedIds();existing->clear();QMap<QString,int> counts;for(const auto &v:characters)++counts[v.toObject().value("name").toString()];for(const auto &v:characters){const auto c=v.toObject();const auto id=c.value("id").toString(),name=c.value("name").toString();if(!used.contains(id))existing->addItem(counts[name]>1?name+" · "+id:name,id);}const int restored=existing->findData(selected);if(restored>=0)existing->setCurrentIndex(restored);existing->setEnabled(existing->count()>0&&table->rowCount()<16);addExisting->setEnabled(existing->isEnabled());add->setEnabled(table->rowCount()<16);remove->setEnabled(table->currentRow()>0);};
  connect(table,&QTableWidget::currentCellChanged,&dialog,[&](int row,int,int,int){remove->setEnabled(row>0);});
  connect(addExisting,&QPushButton::clicked,&dialog,[&]{const int selected=existing->findText(existing->currentText());if(selected<0||table->rowCount()>=16)return;const auto id=existing->itemData(selected).toString();if(id.isEmpty()||usedIds().contains(id))return;QJsonObject member;for(const auto &v:previous.value("members").toArray())if(v.toObject().value("characterId")==id)member=v.toObject();member["characterId"]=id;addMember(member);table->setCurrentCell(table->rowCount()-1,1);refreshScope();refreshExisting();});
  connect(add,&QPushButton::clicked,&dialog,[&]{if(table->rowCount()>=16)return;auto used=usedIds();for(const auto &v:characters)used.insert(v.toObject().value("id").toString());int number=table->rowCount()+1;while(used.contains("trainee-"+QString::number(number)))++number;addMember(QJsonObject{{"characterId","trainee-"+QString::number(number)},{"name",tr("새 육성 대상 %1").arg(number)}});table->setCurrentCell(table->rowCount()-1,1);table->editItem(table->item(table->rowCount()-1,1));refreshScope();refreshExisting();});
  connect(remove,&QPushButton::clicked,&dialog,[&]{if(table->currentRow()>0)table->removeRow(table->currentRow());refreshExisting();});refreshExisting();
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
      QJsonObject member;for(const auto &value:previous.value("members").toArray())if(value.toObject().value("characterId")==id)member=value.toObject();
      member["characterId"]=id;member["initialAttributes"]=initial;member["activityId"]=qobject_cast<QComboBox*>(table->cellWidget(row,2))->currentData().toString();values.append(member);
      QJsonObject character;for(const auto &v:characters)if(v.toObject()["id"]==id)character=v.toObject();
      if(character.isEmpty())character=QJsonObject{{"formatVersion",project["formatVersion"]},{"id",id},{"name",name},{"portraits",QJsonObject{}},{"relationships",QJsonArray{}}};else character["name"]=name;
      const auto file=files.value(id,"characters/"+id+".json");if(!files.contains(id))edits.append(QJsonObject{{"type","add"},{"kind","character"},{"id",id}});edits.append(QJsonObject{{"type","replace"},{"file",file},{"data",character}});
    }
    if(enabled->isChecked()){auto roster=previous;roster["localAttributeIds"]=ids;roster["members"]=values;project["roster"]=roster;}else project.remove("roster");
    edits.append(QJsonObject{{"type","replace"},{"file",projectFile},{"data",project}});
    request("edit",{{"edit",QJsonObject{{"type","batch"},{"edits",edits}}}});dialog.accept();
  });dialog.exec();
}
