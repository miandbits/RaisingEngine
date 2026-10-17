#include "studio.hpp"
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QListWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QUuid>
#include <QVBoxLayout>
#include <memory>

namespace {
QString freshStateId(const QString &prefix) { return prefix+QUuid::createUuid().toString(QUuid::Id128); }
QString stateReferenceIssue(const QJsonValue &value,const QJsonArray &groups,const QString &path) {
  if(value.isArray()){int i=0;for(const auto &v:value.toArray()){const auto issue=stateReferenceIssue(v,groups,path+"/"+QString::number(i++));if(!issue.isEmpty())return issue;}}
  if(!value.isObject())return {};
  const auto o=value.toObject();const auto op=o.value("op").toString();
  if(op=="stateIs"||op=="setState"){
    QJsonObject group;for(const auto &g:groups)if(g.toObject().value("id")==o.value("stateGroupId"))group=g.toObject();
    bool found=false;for(const auto &v:group.value("values").toArray())if(v.toObject().value("id")==o.value("stateValueId"))found=true;
    if(!found)return path+QObject::tr(" · 사용 중인 상태/값은 삭제할 수 없습니다. 연결을 먼저 변경하세요.");
    if((group.value("scope")=="character")!=o.contains("owner"))return path+QObject::tr(" · 연결된 대상과 소유 범위가 달라집니다. 연결을 먼저 변경하세요.");
  }
  for(auto it=o.begin();it!=o.end();++it){const auto issue=stateReferenceIssue(it.value(),groups,path+"/"+it.key());if(!issue.isEmpty())return issue;}
  return {};
}
}

void StudioWindow::editCustomStates() {
  if(busy()||projectData().isEmpty())return;
  const auto original=projectData();const auto revision=view.value("revision"),root=view.value("root");
  auto groups=std::make_shared<QJsonArray>(original.value("stateGroups").toArray());
  auto dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setObjectName("customStatesDialog");dialog->setWindowTitle(tr("사용자 정의 상태"));dialog->resize(760,540);
  auto layout=new QVBoxLayout(dialog);auto help=new QLabel(tr("행동 모드(평상시·수련·잠입), 소속 등 게임만의 상태를 만드세요.\n조건의 ‘상태 확인’과 효과의 ‘상태 변경’에서 이름으로 선택합니다. 조건별 표현에도 연결할 수 있습니다.\n캐릭터별 상태는 캐릭터별 데이터 설정을 사용합니다. 엔진 진행 단계와는 별개입니다."));help->setWordWrap(true);layout->addWidget(help);
  auto list=new QListWidget;list->setObjectName("stateGroupList");layout->addWidget(list);
  auto refresh=[=](int row){list->clear();for(const auto &g:*groups){const auto o=g.toObject();QStringList names;for(const auto &v:o.value("values").toArray())names<<v.toObject().value("name").toString();list->addItem(o.value("name").toString()+" · "+(o.value("scope")=="shared"?tr("공용"):tr("캐릭터별"))+" · "+names.join(" / "));}if(!groups->isEmpty())list->setCurrentRow(qBound(0,row,int(groups->size())-1));};
  auto edit=[=,this](int row,bool adding){
    if(row<0||row>=groups->size())return;
    const auto originalGroup=groups->at(row).toObject();
    auto d=new QDialog(dialog);d->setAttribute(Qt::WA_DeleteOnClose);d->setObjectName("stateGroupDialog");d->setWindowTitle(tr("상태와 값 편집"));d->resize(570,460);
    auto outer=new QVBoxLayout(d);auto form=new QFormLayout;outer->addLayout(form);
    auto name=new QLineEdit(originalGroup.value("name").toString());name->setObjectName("stateGroupName");name->setMaxLength(200);form->addRow(tr("상태 이름"),name);
    auto scope=new QComboBox;scope->setObjectName("stateGroupScope");scope->addItem(tr("공용 · 게임 전체"),"shared");scope->addItem(tr("캐릭터별 · 각자 다른 값"),"character");scope->setCurrentIndex(scope->findData(originalGroup.value("scope").toString()));form->addRow(tr("소유 범위"),scope);
    auto table=new QTableWidget(0,1);table->setObjectName("stateValuesTable");table->setHorizontalHeaderLabels({tr("값 이름")});table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);outer->addWidget(table);
    auto initial=new QComboBox;initial->setObjectName("stateInitialValue");form->addRow(tr("새 게임 초기값"),initial);
    auto refill=[=]{const auto selected=initial->currentData().toString();initial->clear();for(int i=0;i<table->rowCount();++i)initial->addItem(table->item(i,0)->text(),table->item(i,0)->data(Qt::UserRole));const int index=initial->findData(selected);if(index>=0)initial->setCurrentIndex(index);};
    auto append=[=](const QJsonObject &v){int n=table->rowCount();table->insertRow(n);auto item=new QTableWidgetItem(v.value("name").toString());item->setData(Qt::UserRole,v.value("id").toString());table->setItem(n,0,item);refill();table->setCurrentCell(n,0);};
    for(const auto &v:originalGroup.value("values").toArray())append(v.toObject());initial->setCurrentIndex(initial->findData(originalGroup.value("initialValueId").toString()));
    connect(table,&QTableWidget::cellChanged,d,[=]{refill();});
    auto buttonsRow=new QHBoxLayout;auto add=new QPushButton(tr("＋ 값"));add->setObjectName("stateValueAdd");auto remove=new QPushButton(tr("선택 값 삭제"));remove->setObjectName("stateValueRemove");buttonsRow->addWidget(add);buttonsRow->addWidget(remove);outer->addLayout(buttonsRow);
    connect(add,&QPushButton::clicked,d,[=]{if(table->rowCount()<128)append({{"id",freshStateId("value-")},{"name",tr("새 값")}});});
    connect(remove,&QPushButton::clicked,d,[=]{if(table->currentRow()>=0){table->removeRow(table->currentRow());refill();}});
    auto error=new QLabel;error->setObjectName("stateGroupError");error->setWordWrap(true);outer->addWidget(error);auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);outer->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,d,&QDialog::reject);
    connect(d,&QDialog::rejected,dialog,[=]{if(adding){groups->removeAt(row);refresh(row);}});
    connect(buttons,&QDialogButtonBox::accepted,d,[=]{
      if(name->text().trimmed().isEmpty()||table->rowCount()==0||initial->currentIndex()<0){error->setText(tr("상태 이름, 값, 초기값을 지정하세요."));return;}
      if(scope->currentData()=="character"&&original.value("roster").toObject().value("stateModel")!="character-v1"){error->setText(tr("먼저 캐릭터별 데이터 설정에서 캐릭터 소유 상태를 활성화하세요."));return;}
      QSet<QString> names;QJsonArray values;for(int i=0;i<table->rowCount();++i){const auto text=table->item(i,0)->text().trimmed();if(text.isEmpty()||text.size()>200||names.contains(text)){error->setText(tr("값 이름은 비어 있지 않은 서로 다른 이름(200자 이하)이어야 합니다."));return;}names.insert(text);values.append(QJsonObject{{"id",table->item(i,0)->data(Qt::UserRole).toString()},{"name",text}});}
      auto g=originalGroup;g["name"]=name->text().trimmed();g["scope"]=scope->currentData().toString();g["initialValueId"]=initial->currentData().toString();g["values"]=values;(*groups)[row]=g;refresh(row);d->accept();
    });d->open();
  };
  auto row=new QHBoxLayout;auto add=new QPushButton(tr("＋ 상태"));add->setObjectName("stateGroupAdd");auto change=new QPushButton(tr("이름·값 편집"));change->setObjectName("stateGroupEdit");auto remove=new QPushButton(tr("상태 삭제"));remove->setObjectName("stateGroupRemove");for(auto b:{add,change,remove})row->addWidget(b);layout->addLayout(row);
  connect(add,&QPushButton::clicked,dialog,[=]{if(groups->size()>=128)return;const auto id=freshStateId("value-");groups->append(QJsonObject{{"id",freshStateId("state-")},{"name",tr("새 상태")},{"scope","shared"},{"initialValueId",id},{"values",QJsonArray{QJsonObject{{"id",id},{"name",tr("평상시")}}}}});refresh(groups->size()-1);edit(groups->size()-1,true);});
  connect(change,&QPushButton::clicked,dialog,[=]{edit(list->currentRow(),false);});
  connect(list,&QListWidget::itemDoubleClicked,dialog,[=]{edit(list->currentRow(),false);});
  connect(remove,&QPushButton::clicked,dialog,[=]{int r=list->currentRow();if(r>=0){groups->removeAt(r);refresh(r);}});
  auto error=new QLabel;error->setObjectName("customStatesError");error->setWordWrap(true);layout->addWidget(error);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
  connect(buttons,&QDialogButtonBox::accepted,dialog,[=,this]{
    if(busy()||view.value("revision")!=revision||view.value("root")!=root){error->setText(tr("프로젝트가 변경되었습니다. 다시 열어 주세요."));return;}
    QSet<QString> names;for(const auto &g:*groups){const auto name=g.toObject().value("name").toString();if(names.contains(name)){error->setText(tr("상태 이름은 서로 달라야 합니다."));return;}names.insert(name);}
    for(const auto &v:view.value("documents").toArray()){const auto doc=v.toObject();const auto issue=stateReferenceIssue(doc.value("data"),*groups,doc.value("file").toString());if(!issue.isEmpty()){error->setText(issue);return;}}
    auto project=original;if(!groups->isEmpty()||original.contains("stateGroups"))project["stateGroups"]=*groups;dialog->accept();if(project!=original)replaceProject(project);
  });refresh(0);dialog->open();
}
