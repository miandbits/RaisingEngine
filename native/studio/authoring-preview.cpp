#include "renderer-preview.hpp"
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QScrollArea>
#include <QVBoxLayout>

void RendererPreviewDock::editScenario(){
  QDialog dialog(this);dialog.setObjectName("authoringPreviewDialog");dialog.setWindowTitle(tr("제작 상태 미리보기"));dialog.resize(620,680);
  auto layout=new QVBoxLayout(&dialog);auto notice=new QLabel(tr("선택한 부품의 캐릭터·활동만 임시 변경합니다. 다른 부품의 연결은 유지합니다.\n가상 검사이며 도달 가능성을 보장하지 않습니다. 프로젝트·본편·저장 파일에 반영되지 않습니다."));notice->setWordWrap(true);layout->addWidget(notice);
  auto active=new QCheckBox(tr("가상 검사 상태 사용"));active->setObjectName("previewScenarioEnabled");active->setChecked(scenario.value("enabled").toBool());layout->addWidget(active);
  auto scroll=new QScrollArea;scroll->setWidgetResizable(true);auto body=new QWidget;auto form=new QFormLayout(body);scroll->setWidget(body);layout->addWidget(scroll);
  auto actor=new QComboBox;actor->setObjectName("previewActor");actor->addItem(tr("기존 부품 연결 유지"),"");auto activity=new QComboBox;activity->setObjectName("previewActivity");activity->addItem(tr("기존 부품 연결 유지"),"");
  QStringList actors;for(auto member:project.value("roster").toObject().value("members").toArray())actors<<member.toObject().value("characterId").toString();
  QJsonArray attributes,flags;
  for(auto value:documents){auto doc=value.toObject(),data=doc.value("data").toObject();auto kind=doc.value("kind").toString();if(kind=="character"&&actors.contains(data.value("id").toString()))actor->addItem(data.value("name").toString(),data.value("id").toString());if(kind=="activity")activity->addItem(data.value("label").toString(),data.value("id").toString());if(kind=="attributes")attributes=data.value("attributes").toArray();if(kind=="flags")flags=data.value("flags").toArray();}
  actor->setCurrentIndex(qMax(0,actor->findData(scenario.value("actorId").toString())));activity->setCurrentIndex(qMax(0,activity->findData(scenario.value("activityId").toString())));form->addRow(tr("검사할 캐릭터"),actor);form->addRow(tr("검사할 활동"),activity);
  auto routing=new QCheckBox(tr("조건에 따라 연결된 화면 확인"));routing->setObjectName("previewFollowRouting");routing->setChecked(scenario.value("followRouting").toBool());form->addRow(tr("화면 선택"),routing);
  auto rows=new QWidget;auto values=new QFormLayout(rows);form->addRow(rows);
  QMap<QString,QSpinBox*> numbers;QMap<QString,QCheckBox*> useNumbers;QMap<QString,QComboBox*> states,booleans;
  for(auto value:attributes){auto a=value.toObject();auto id=a.value("id").toString();auto box=new QWidget;auto line=new QHBoxLayout(box);line->setContentsMargins(0,0,0,0);auto use=new QCheckBox(tr("변경"));use->setObjectName("previewUseAttribute-"+id);use->setChecked(scenario.value("attributes").toObject().contains(id));auto number=new QSpinBox;number->setObjectName("previewAttribute-"+id);number->setRange(a.value("min").toInt(),a.value("max").toInt());number->setValue(scenario.value("attributes").toObject().value(id).toInt(a.value("initial").toInt()));number->setEnabled(use->isChecked());connect(use,&QCheckBox::toggled,number,&QSpinBox::setEnabled);line->addWidget(use);line->addWidget(number);values->addRow(a.value("label").toString(),box);numbers[id]=number;useNumbers[id]=use;}
  for(auto value:project.value("stateGroups").toArray()){auto group=value.toObject();auto id=group.value("id").toString();auto combo=new QComboBox;combo->setObjectName("previewState-"+id);combo->addItem(tr("초기 상태 유지"),"");for(auto v:group.value("values").toArray())combo->addItem(v.toObject().value("name").toString(),v.toObject().value("id").toString());combo->setCurrentIndex(qMax(0,combo->findData(scenario.value("states").toObject().value(id).toString())));values->addRow(group.value("name").toString()+(group.value("scope")=="character"?tr(" · 선택 캐릭터"):tr(" · 공용")),combo);states[id]=combo;}
  for(auto value:flags){auto flag=value.toObject();auto id=flag.value("id").toString();auto combo=new QComboBox;combo->setObjectName("previewFlag-"+id);combo->addItem(tr("초기값 유지"),"");combo->addItem(tr("참"),"true");combo->addItem(tr("거짓"),"false");if(scenario.value("flags").toObject().contains(id))combo->setCurrentIndex(scenario.value("flags").toObject().value(id).toBool()?1:2);values->addRow(flag.value("label").toString(id),combo);booleans[id]=combo;}
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel|QDialogButtonBox::Reset);buttons->setObjectName("previewScenarioButtons");layout->addWidget(buttons);connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  connect(buttons->button(QDialogButtonBox::Reset),&QPushButton::clicked,&dialog,[&]{active->setChecked(false);actor->setCurrentIndex(0);activity->setCurrentIndex(0);routing->setChecked(false);for(auto use:useNumbers)use->setChecked(false);for(auto combo:states)combo->setCurrentIndex(0);for(auto combo:booleans)combo->setCurrentIndex(0);});
  if(dialog.exec()!=QDialog::Accepted)return;
  scenario={{"enabled",active->isChecked()},{"actorId",actor->currentData().toString()},{"activityId",activity->currentData().toString()},{"followRouting",routing->isChecked()}};
  QJsonObject attrs,custom,flagValues;for(auto it=numbers.begin();it!=numbers.end();++it)if(useNumbers[it.key()]->isChecked())attrs[it.key()]=it.value()->value();for(auto it=states.begin();it!=states.end();++it)if(!it.value()->currentData().toString().isEmpty())custom[it.key()]=it.value()->currentData().toString();for(auto it=booleans.begin();it!=booleans.end();++it)if(it.value()->currentIndex()>0)flagValues[it.key()]=it.value()->currentIndex()==1;
  scenario["attributes"]=attrs;scenario["states"]=custom;scenario["flags"]=flagValues;changed();
  status->setText(active->isChecked()?tr("가상 검사 상태 설정됨 · 현재 화면 렌더링을 눌러 확인하세요."):tr("초기 상태 검사로 복원했습니다."));
}

void RendererPreviewDock::showReasons(const QJsonObject& explanation){
  reasons->clear();if(explanation.isEmpty())return;
  auto top=new QTreeWidgetItem(reasons,{tr("화면 선택"),explanation.value("forced").toBool()?tr("편집 중 화면 고정 (플레이 화면 연결과 별개)"):tr("플레이와 같은 연결 우선순위")});
  for(auto value:explanation.value("routing").toObject().value("candidates").toArray()){auto c=value.toObject();new QTreeWidgetItem(top,{c.value("label").toString()+" → "+c.value("screenLabel").toString(c.value("screenId").toString()),(c.value("selected").toBool()?tr("[연결 선택] "):QString())+c.value("reason").toString()});}
  for(auto value:explanation.value("rules").toArray()){auto e=value.toObject();if(e.value("variants").toArray().isEmpty()&&e.value("visible").toBool()&&e.value("enabled").toBool())continue;auto item=new QTreeWidgetItem(reasons,{e.value("label").toString(),(e.value("visible").toBool()?tr("표시 조건 충족"):tr("숨김"))+(e.value("enabled").toBool()?QString():tr(" · 버튼 조건 불충족"))});item->setData(0,Qt::UserRole,e.value("elementId").toString());for(auto v:e.value("variants").toArray()){auto rule=v.toObject();auto row=new QTreeWidgetItem(item,{rule.value("label").toString(),(rule.value("selected").toBool()?tr("[적용] "):QString())+rule.value("reason").toString()});row->setData(0,Qt::UserRole,e.value("elementId").toString());}}
  reasons->expandAll();
}
