#include "studio.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

void StudioWindow::editCharacterState(){
  if(bridge.busy()||projectData().isEmpty())return;
  auto project=projectData(),roster=project.value("roster").toObject();
  if(roster.isEmpty()){errorLabel->setText(tr("먼저 디자인 → 다인 육성에서 육성 대상을 만드세요."));return;}
  const auto revision=view.value("revision"),root=view.value("root");auto members=roster.value("members").toArray();
  auto gameplay=project.value("gameplay").toObject();
  if(gameplay.empty()){
    QJsonObject home{{"id","home"},{"name",tr("집")},{"connections",QJsonArray{}}};
    const auto screens=project.value("visual").toObject().value("screens").toArray();
    for(const auto &v:screens){const auto id=v.toObject().value("id").toString();if(!home.contains("screenId")||id=="schedule")home["screenId"]=id;}
    gameplay={{"startLocationId","home"},{"locations",QJsonArray{home}},{"items",QJsonArray{}},{"shops",QJsonArray{}},{"interactions",QJsonArray{}}};
  }
  QJsonArray characters;for(const auto &v:view.value("documents").toArray())if(v.toObject().value("kind")=="character")characters.append(v.toObject().value("data"));
  auto characterName=[&](QString id){for(const auto &v:characters)if(v.toObject().value("id")==id)return v.toObject().value("name").toString(id);return id;};
  QDialog dialog(this);dialog.setObjectName("characterStateDialog");dialog.setWindowTitle(tr("캐릭터 소유 상태 · 관계 · 행동 연결"));dialog.resize(1120,760);auto box=new QVBoxLayout(&dialog);
  auto enabled=new QCheckBox(tr("캐릭터별 소유 상태 사용"));enabled->setObjectName("characterStateEnabled");enabled->setChecked(true);box->addWidget(enabled);
  auto help=new QLabel(tr("소지품·관계는 캐릭터별로 독립 관리합니다. 공용 상태는 유지됩니다. 공통 행동을 만든 뒤 마지막 탭에서 연결하세요. 조건·효과의 소유자는 '행동 실행자' 또는 '지정 캐릭터'로 설정합니다. 해제하면 이 창의 개별 초기값과 행동 연결 설정을 제거합니다."));help->setWordWrap(true);box->addWidget(help);
  auto tabs=new QTabWidget;tabs->setObjectName("characterStateTabs");box->addWidget(tabs,1);
  auto itemPage=new QWidget;auto ib=new QVBoxLayout(itemPage);auto items=new QTableWidget(0,4+members.size());items->setObjectName("characterItems");QStringList headers{tr("물품 ID"),tr("이름"),tr("최대 보유"),tr("공용 초기값")};for(const auto &v:members)headers<<characterName(v.toObject().value("characterId").toString());items->setHorizontalHeaderLabels(headers);items->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);ib->addWidget(items);
  auto number=[](int value,int min,int max){auto n=new QSpinBox;n->setRange(min,max);n->setValue(value);return n;};
  auto addItem=[&](QJsonObject item){int row=items->rowCount();items->insertRow(row);auto id=item.value("id").toString();auto cell=new QTableWidgetItem(id);cell->setData(Qt::UserRole,item);items->setItem(row,0,cell);items->setItem(row,1,new QTableWidgetItem(item.value("name").toString()));items->setCellWidget(row,2,number(item.value("maxStack").toInt(99),1,2147483647));items->setCellWidget(row,3,number(item.value("initialQuantity").toInt(),0,2147483647));
    for(int i=0;i<members.size();i++)items->setCellWidget(row,4+i,number(members[i].toObject().value("initialInventory").toObject().value(id).toInt(item.value("initialQuantity").toInt()),0,2147483647));};
  for(const auto &v:gameplay.value("items").toArray())addItem(v.toObject());
  auto add=new QPushButton(tr("＋ 물품 정의"));add->setObjectName("characterAddItem");ib->addWidget(add);connect(add,&QPushButton::clicked,&dialog,[&]{int n=items->rowCount()+1;QString id;bool used;do{id="item-"+QString::number(n++);used=false;for(int row=0;row<items->rowCount();row++)if(items->item(row,0)->text()==id)used=true;}while(used);addItem({{"id",id},{"name",tr("새 물품")},{"maxStack",99}});});tabs->addTab(itemPage,tr("물품 정의 · 개별 소지품"));
  auto relationships=new QTableWidget(0,4);relationships->setObjectName("characterRelationships");relationships->setHorizontalHeaderLabels({tr("소유 캐릭터"),tr("관계 상대"),tr("관계 종류"),tr("초기값")});relationships->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  for(int i=0;i<members.size();i++)for(const auto &v:characters){auto c=v.toObject();for(const auto &r:c.value("relationships").toArray()){auto rel=r.toObject();int row=relationships->rowCount();relationships->insertRow(row);auto owner=new QTableWidgetItem(characterName(members[i].toObject().value("characterId").toString()));owner->setData(Qt::UserRole,i);owner->setFlags(owner->flags()&~Qt::ItemIsEditable);relationships->setItem(row,0,owner);auto other=new QTableWidgetItem(c.value("name").toString());other->setData(Qt::UserRole,c.value("id").toString());other->setFlags(other->flags()&~Qt::ItemIsEditable);relationships->setItem(row,1,other);auto kind=new QTableWidgetItem(rel.value("label").toString(rel.value("id").toString()));kind->setData(Qt::UserRole,rel.value("id").toString());kind->setFlags(kind->flags()&~Qt::ItemIsEditable);relationships->setItem(row,2,kind);auto initial=members[i].toObject().value("initialRelationships").toObject().value(c.value("id").toString()).toObject();relationships->setCellWidget(row,3,number(initial.value(rel.value("id").toString()).toInt(project.value("initialState").toObject().value("relationships").toObject().value(c.value("id").toString()).toObject().value(rel.value("id").toString()).toInt(rel.value("initial").toInt())),rel.value("min").toInt(),rel.value("max").toInt()));}}
  auto relationPage=new QWidget;auto relationBox=new QVBoxLayout(relationPage);relationBox->addWidget(relationships);auto addRelation=new QPushButton(tr("＋ 관계 종류 정의"));addRelation->setObjectName("characterAddRelationship");relationBox->addWidget(addRelation);tabs->addTab(relationPage,tr("캐릭터별 관계"));QSet<QString> changedCharacters;
  connect(addRelation,&QPushButton::clicked,&dialog,[&]{
    QDialog definition(&dialog);definition.setObjectName("characterRelationshipDefinition");definition.setWindowTitle(tr("관계 종류 정의"));auto form=new QFormLayout(&definition);
    auto other=new QComboBox;other->setObjectName("relationshipCharacter");for(const auto &v:characters)other->addItem(v.toObject().value("name").toString(),v.toObject().value("id").toString());form->addRow(tr("관계 상대"),other);
    auto id=new QLineEdit;id->setObjectName("relationshipId");form->addRow(tr("관계 ID"),id);auto name=new QLineEdit;name->setObjectName("relationshipName");form->addRow(tr("이름"),name);
    auto min=number(0,-2147483647-1,2147483647),max=number(100,-2147483647-1,2147483647),initial=number(0,-2147483647-1,2147483647);min->setObjectName("relationshipMin");max->setObjectName("relationshipMax");initial->setObjectName("relationshipInitial");form->addRow(tr("최솟값"),min);form->addRow(tr("최댓값"),max);form->addRow(tr("기본 초기값"),initial);
    auto message=new QLabel;form->addRow(message);auto controls=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);form->addRow(controls);connect(controls,&QDialogButtonBox::rejected,&definition,&QDialog::reject);
    connect(controls,&QDialogButtonBox::accepted,&definition,[&]{const auto key=id->text().trimmed();if(key.size()>64||!QRegularExpression("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$").match(key).hasMatch()||name->text().trimmed().isEmpty()||min->value()>max->value()||initial->value()<min->value()||initial->value()>max->value()){message->setText(tr("고유한 소문자 ID·이름과 올바른 범위·초기값을 입력하세요."));return;}
      for(int index=0;index<characters.size();index++){auto c=characters[index].toObject();if(c.value("id")!=other->currentData().toString())continue;auto definitions=c.value("relationships").toArray();for(const auto &v:definitions)if(v.toObject().value("id")==key){message->setText(tr("이미 존재하는 관계 ID입니다."));return;}
        definitions.append(QJsonObject{{"id",key},{"label",name->text().trimmed()},{"min",min->value()},{"max",max->value()},{"initial",initial->value()}});c["relationships"]=definitions;characters[index]=c;changedCharacters.insert(c.value("id").toString());
        for(int i=0;i<members.size();i++){const int row=relationships->rowCount();relationships->insertRow(row);auto owner=new QTableWidgetItem(characterName(members[i].toObject().value("characterId").toString()));owner->setData(Qt::UserRole,i);owner->setFlags(owner->flags()&~Qt::ItemIsEditable);relationships->setItem(row,0,owner);auto target=new QTableWidgetItem(c.value("name").toString());target->setData(Qt::UserRole,c.value("id").toString());target->setFlags(target->flags()&~Qt::ItemIsEditable);relationships->setItem(row,1,target);auto label=new QTableWidgetItem(name->text().trimmed());label->setData(Qt::UserRole,key);label->setFlags(label->flags()&~Qt::ItemIsEditable);relationships->setItem(row,2,label);relationships->setCellWidget(row,3,number(initial->value(),min->value(),max->value()));}
        definition.accept();return;
      }
    });definition.exec();
  });
  auto actions=new QTableWidget(members.size(),2);actions->setObjectName("characterActions");actions->setHorizontalHeaderLabels({tr("실행자"),tr("연결할 공통 행동")});actions->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  for(int i=0;i<members.size();i++){const auto member=members[i].toObject();auto name=new QTableWidgetItem(characterName(member.value("characterId").toString()));name->setFlags(name->flags()&~Qt::ItemIsEditable);actions->setItem(i,0,name);auto list=new QListWidget;list->setObjectName("characterActions-"+member.value("characterId").toString());for(const auto &v:gameplay.value("actions").toArray()){auto action=v.toObject();if(action.contains("expeditionId"))continue;auto item=new QListWidgetItem(action.value("name").toString()+" · "+action.value("id").toString(),list);item->setData(Qt::UserRole,action.value("id").toString());item->setCheckState(member.value("actionIds").toArray().contains(action.value("id"))?Qt::Checked:Qt::Unchecked);}actions->setCellWidget(i,1,list);actions->setRowHeight(i,170);}
  tabs->addTab(actions,tr("대상별 행동 연결"));
  auto error=new QLabel;error->setObjectName("characterStateError");error->setWordWrap(true);box->addWidget(error);auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons->setObjectName("characterStateButtons");box->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
    if(view.value("revision")!=revision||view.value("root")!=root){error->setText(tr("프로젝트 버전이 바뀌었습니다. 창을 다시 열어 주세요."));return;}
    QJsonArray catalog;QSet<QString> ids;QVector<QJsonObject> inventories(members.size()),relations(members.size());
    for(int row=0;row<items->rowCount();row++){
      auto item=items->item(row,0)->data(Qt::UserRole).toJsonObject();const auto id=items->item(row,0)->text().trimmed(),name=items->item(row,1)->text().trimmed();
      if(id.size()>64||!QRegularExpression("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$").match(id).hasMatch()||ids.contains(id)||name.isEmpty()){error->setText(tr("물품에는 고유한 소문자 ID와 이름이 필요합니다."));return;}ids.insert(id);
      const int max=qobject_cast<QSpinBox*>(items->cellWidget(row,2))->value();item["id"]=id;item["name"]=name;item["maxStack"]=max;item["initialQuantity"]=qobject_cast<QSpinBox*>(items->cellWidget(row,3))->value();
      for(int col=3;col<items->columnCount();col++)if(qobject_cast<QSpinBox*>(items->cellWidget(row,col))->value()>max){error->setText(tr("%1: 초기 수량은 최대 보유 수량을 넘을 수 없습니다.").arg(name));return;}
      catalog.append(item);for(int i=0;i<members.size();i++)inventories[i][id]=qobject_cast<QSpinBox*>(items->cellWidget(row,4+i))->value();
    }
    for(int row=0;row<relationships->rowCount();row++){const int i=relationships->item(row,0)->data(Qt::UserRole).toInt();const auto other=relationships->item(row,1)->data(Qt::UserRole).toString(),key=relationships->item(row,2)->data(Qt::UserRole).toString();auto values=relations[i].value(other).toObject();values[key]=qobject_cast<QSpinBox*>(relationships->cellWidget(row,3))->value();relations[i][other]=values;}
    for(int i=0;i<members.size();i++){auto member=members[i].toObject();if(enabled->isChecked()){member["initialInventory"]=inventories[i];member["initialRelationships"]=relations[i];QJsonArray bound;auto list=qobject_cast<QListWidget*>(actions->cellWidget(i,1));for(int j=0;j<list->count();j++)if(list->item(j)->checkState()==Qt::Checked)bound.append(list->item(j)->data(Qt::UserRole).toString());member["actionIds"]=bound;}else{member.remove("initialInventory");member.remove("initialRelationships");member.remove("actionIds");}members[i]=member;}
    gameplay["items"]=catalog;project["gameplay"]=gameplay;roster["members"]=members;if(enabled->isChecked())roster["stateModel"]="character-v1";else roster.remove("stateModel");project["roster"]=roster;
    QJsonArray edits;for(const auto &v:view.value("documents").toArray()){auto doc=v.toObject();if(doc.value("kind")!="character"||!changedCharacters.contains(doc.value("data").toObject().value("id").toString()))continue;for(const auto &c:characters)if(c.toObject().value("id")==doc.value("data").toObject().value("id"))edits.append(QJsonObject{{"type","replace"},{"file",doc.value("file")},{"data",c}});}
    edits.append(QJsonObject{{"type","replace"},{"file",projectFile},{"data",project}});request("edit",{{"edit",QJsonObject{{"type","batch"},{"edits",edits}}}});dialog.accept();
  });dialog.exec();
}
