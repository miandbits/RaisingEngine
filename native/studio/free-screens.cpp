#include "studio.hpp"
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

void StudioWindow::editScreenRouting() {
  if(busy()||projectData().isEmpty())return;
  const auto project=projectData(),visual=project.value("visual").toObject();
  const auto revision=view.value("revision"),root=view.value("root");
  if(visual.value("screens").toArray().isEmpty())return;
  auto dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setObjectName("screenRoutingDialog");dialog->setWindowTitle(tr("화면 전환 연결"));dialog->resize(560,530);
  auto layout=new QVBoxLayout(dialog);auto help=new QLabel(tr("화면은 자유로운 캔버스입니다. 게임 상태별로 표시할 화면을 선택하세요.\n‘기본 화면’을 선택하면 일정·대화·관계를 같은 화면에 함께 배치할 수 있습니다.\n버튼의 ‘화면 이동’은 현재 상태에서 화면만 바꿉니다. 게임 상태가 바뀌면 이 규칙을 다시 적용합니다."));help->setWordWrap(true);layout->addWidget(help);
  auto form=new QFormLayout;layout->addLayout(form);const auto routing=visual.value("routing").toObject();
  auto defaultScreen=new QComboBox;defaultScreen->setObjectName("routingDefault");
  for(const auto& value:visual.value("screens").toArray()){const auto s=value.toObject();if(!s.value("modal").toBool())defaultScreen->addItem(s.value("label").toString(),s.value("id").toString());}
  auto current=routing.value("defaultScreenId").toString(screenId);defaultScreen->setCurrentIndex(qMax(0,defaultScreen->findData(current)));form->addRow(tr("기본 화면"),defaultScreen);
  QList<QPair<QString,QString>> phases={{"title",tr("시작 전")},{"planning",tr("일정 계획")},{"ready",tr("활동 준비")},{"activity-result",tr("활동 결과")},{"period-end",tr("기간 종료")},{"dialogue",tr("대사")},{"choice",tr("선택지")},{"finished",tr("엔딩 완료")},{"expedition",tr("탐험")},{"battle",tr("전투")}};
  QList<QComboBox*> fields;
  for(const auto& phase:phases){auto field=new QComboBox;field->setObjectName("routing-"+phase.first);field->addItem(tr("기본 화면"),"");for(int i=0;i<defaultScreen->count();++i)field->addItem(defaultScreen->itemText(i),defaultScreen->itemData(i));
    for(const auto& r:routing.value("phases").toArray())if(r.toObject().value("phase")==phase.first)field->setCurrentIndex(qMax(0,field->findData(r.toObject().value("screenId").toString())));
    form->addRow(phase.second,field);fields.append(field);
  }
  auto location=new QCheckBox(tr("일정 계획·준비 중에는 장소의 연결 화면을 우선 사용"));location->setObjectName("routingLocation");location->setChecked(routing.value("useLocationScreens").toBool());layout->addWidget(location);
  auto error=new QLabel;error->setWordWrap(true);layout->addWidget(error);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Ok)->setText(tr("전환 연결 적용"));layout->addWidget(buttons);
  connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
  connect(buttons,&QDialogButtonBox::accepted,dialog,[=,this]{
    if(busy()||view.value("revision")!=revision||view.value("root")!=root){error->setText(tr("프로젝트가 변경되었습니다. 다시 열어 주세요."));return;}
    if(defaultScreen->currentIndex()<0){error->setText(tr("일반 화면을 먼저 만드세요."));return;}
    QJsonArray routes;for(int i=0;i<phases.size();++i)if(!fields[i]->currentData().toString().isEmpty())routes.append(QJsonObject{{"phase",phases[i].first},{"screenId",fields[i]->currentData().toString()}});
    auto updated=project,v=visual;v["routing"]=QJsonObject{{"defaultScreenId",defaultScreen->currentData().toString()},{"phases",routes},{"useLocationScreens",location->isChecked()}};updated["visual"]=v;replaceProject(updated);dialog->accept();
  });dialog->open();
}

void StudioWindow::editDataBinding() {
  if(busy()||currentElement().value("kind")!="text"){errorLabel->setText(tr("텍스트 요소를 선택한 뒤 데이터를 연결하세요."));return;}
  const auto original=currentElement(),project=projectData();const auto revision=view.value("revision"),root=view.value("root");const auto selected=selection,screen=screenId;
  auto dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setObjectName("dataBindingDialog");dialog->setWindowTitle(tr("표시할 데이터 연결"));dialog->resize(600,560);
  auto layout=new QVBoxLayout(dialog);auto search=new QLineEdit;search->setObjectName("dataBindingSearch");search->setPlaceholderText(tr("인물·능력·관계·물품 이름으로 검색"));layout->addWidget(search);
  auto list=new QListWidget;list->setObjectName("dataBindingSources");layout->addWidget(list);
  auto add=[&](QString label,QJsonObject payload){auto item=new QListWidgetItem(label,list);item->setData(Qt::UserRole,payload);};
  for(const auto& pair:QList<QPair<QString,QString>>{{"title",tr("게임 제목")},{"description",tr("게임 소개")},{"calendar",tr("현재 날짜")},{"location",tr("현재 장소")},{"speaker",tr("대화 인물")},{"story",tr("현재 대사·선택 질문")},{"ending",tr("엔딩 제목")}})add(pair.second,{{"binding",pair.first}});
  const auto roster=project.value("roster").toObject();
  QMap<QString,QString> names;for(const auto& d:view.value("documents").toArray())if(d.toObject().value("kind")=="character"){const auto c=d.toObject().value("data").toObject();names[c.value("id").toString()]=c.value("name").toString();}
  auto numeric=[&](QString label,QJsonObject target){add(tr("공용 / ")+label,{{"binding","data-value"},{"dataBinding",target}});
    if(target.value("kind")=="attribute"?!roster.value("localAttributeIds").toArray().contains(target.value("attributeId")):roster.value("stateModel")!="character-v1")return;
    for(const auto& m:roster.value("members").toArray()){const auto id=m.toObject().value("characterId").toString();auto owned=target;owned["owner"]=QJsonObject{{"kind","character"},{"characterId",id}};add(names.value(id,id)+" / "+label,{{"binding","data-value"},{"dataBinding",owned}});}
  };
  for(const auto& d:view.value("documents").toArray()){
    const auto doc=d.toObject(),data=doc.value("data").toObject();
    if(doc.value("kind")=="attributes")for(const auto& row:data.value("attributes").toArray()){const auto a=row.toObject();numeric(a.value("label").toString(),{{"kind","attribute"},{"attributeId",a.value("id")}});}
    if(doc.value("kind")=="character")for(const auto& row:data.value("relationships").toArray()){const auto r=row.toObject();numeric(data.value("name").toString()+" · "+r.value("label").toString(),{{"kind","relationship"},{"characterId",data.value("id")},{"relationshipId",r.value("id")}});}
  }
  for(const auto& row:project.value("gameplay").toObject().value("items").toArray()){const auto item=row.toObject();numeric(item.value("name").toString()+tr(" 수량"),{{"kind","inventory"},{"itemId",item.value("id")}});}
  for(const auto& row:project.value("userFields").toArray()){const auto f=row.toObject();add(tr("입력값 / ")+f.value("label").toString(),{{"binding","user-field"},{"userFieldId",f.value("id")}});}
  auto format=new QLineEdit(original.value("valueFormat").toString("{label}: {value}"));format->setObjectName("dataBindingFormat");layout->addWidget(new QLabel(tr("수치·입력값 표시 형식 ({label}, {value})")));layout->addWidget(format);
  auto error=new QLabel;layout->addWidget(error);auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Ok)->setText(tr("연결"));layout->addWidget(buttons);
  connect(search,&QLineEdit::textChanged,dialog,[=](QString text){for(int i=0;i<list->count();++i)list->item(i)->setHidden(!list->item(i)->text().contains(text,Qt::CaseInsensitive));});
  connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
  connect(buttons,&QDialogButtonBox::accepted,dialog,[=,this]{
    if(busy()||view.value("revision")!=revision||view.value("root")!=root||screenId!=screen||selection!=selected){error->setText(tr("프로젝트 또는 선택 요소가 바뀌었습니다. 다시 열어 주세요."));return;}
    if(!list->currentItem()||list->currentItem()->isHidden()){error->setText(tr("표시할 데이터를 선택하세요."));return;}
    const auto payload=list->currentItem()->data(Qt::UserRole).toJsonObject();const auto text=format->text();
    updateElement([=](QJsonObject& e){for(const auto key:{"dataBinding","attributeId","characterId","relationshipId","itemId","userFieldId","valueFormat"})e.remove(key);for(auto it=payload.begin();it!=payload.end();++it)e[it.key()]=it.value();if(payload.value("binding")=="data-value"||payload.value("binding")=="user-field")e["valueFormat"]=text;});dialog->accept();
  });dialog->open();
}
