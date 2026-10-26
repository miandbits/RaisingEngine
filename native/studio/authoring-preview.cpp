#include "renderer-preview.hpp"
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QLineEdit>

void RendererPreviewDock::editScenario(){
  QDialog dialog(this);dialog.setObjectName("authoringPreviewDialog");dialog.setWindowTitle(tr("제작 상태 미리보기"));dialog.resize(620,680);
  auto layout=new QVBoxLayout(&dialog);auto notice=new QLabel(tr("초기 상태 또는 재현 검증한 플레이 저장을 기준으로 검사합니다. 사건 선택은 조건 검사이며 사건을 시작하지 않습니다.\n선택한 부품만 대상 연결을 바꿉니다. 가상 값 변경은 프로젝트·본편·저장 파일에 반영되지 않습니다."));notice->setWordWrap(true);layout->addWidget(notice);
  auto active=new QCheckBox(tr("가상 검사 상태 사용"));active->setObjectName("previewScenarioEnabled");active->setChecked(scenario.value("enabled").toBool());layout->addWidget(active);
  if(canvasMode){active->setChecked(true);active->hide();notice->setText(tr("편집 중인 화면에 실제 플레이어의 표시 값과 조건별 표현을 그립니다. 캐릭터·활동은 대상이 고정되지 않은 요소에만 적용합니다. 가상 변경은 본편과 저장 파일에 반영되지 않습니다."));}
  auto scroll=new QScrollArea;scroll->setWidgetResizable(true);auto body=new QWidget;auto form=new QFormLayout(body);scroll->setWidget(body);layout->addWidget(scroll);
  auto source=new QComboBox;source->setObjectName("previewStateSource");source->addItem(tr("새 게임 초기 상태"));source->addItem(tr("플레이 저장 · 재현 검사"));source->setCurrentIndex(scenario.value("savePath").toString().isEmpty()?0:1);form->addRow(tr("기준 상태"),source);
  auto saveRow=new QWidget;auto saveLayout=new QHBoxLayout(saveRow);saveLayout->setContentsMargins(0,0,0,0);auto savePath=new QLineEdit(scenario.value("savePath").toString());savePath->setObjectName("previewSavePath");savePath->setPlaceholderText(tr("같은 콘텐츠의 플레이 저장 파일"));auto browse=new QPushButton(tr("파일 선택…"));browse->setObjectName("previewBrowseSave");saveLayout->addWidget(savePath,1);saveLayout->addWidget(browse);form->addRow(tr("저장 파일"),saveRow);saveRow->setEnabled(source->currentIndex()==1);
  connect(source,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[=](int index){saveRow->setEnabled(index==1);});connect(browse,&QPushButton::clicked,&dialog,[&]{auto file=QFileDialog::getOpenFileName(&dialog,tr("미리보기용 저장 선택"),root,tr("저장 파일 (*.json);;모든 파일 (*)"));if(!file.isEmpty())savePath->setText(file);});
  auto actor=new QComboBox;actor->setObjectName("previewActor");actor->addItem(tr("기존 부품 연결 유지"),"");auto activity=new QComboBox;activity->setObjectName("previewActivity");activity->addItem(tr("기존 부품 연결 유지"),"");
  QStringList actors;for(auto member:project.value("roster").toObject().value("members").toArray())actors<<member.toObject().value("characterId").toString();
  QJsonArray attributes,flags;auto event=new QComboBox;event->setObjectName("previewEvent");event->addItem(tr("사건 조건 검사 안 함"),"");
  for(auto value:documents){auto doc=value.toObject(),data=doc.value("data").toObject();auto kind=doc.value("kind").toString();if(kind=="character"&&actors.contains(data.value("id").toString()))actor->addItem(data.value("name").toString(),data.value("id").toString());if(kind=="activity")activity->addItem(data.value("label").toString(),data.value("id").toString());if(kind=="attributes")attributes=data.value("attributes").toArray();if(kind=="flags")flags=data.value("flags").toArray();}
  actor->setCurrentIndex(qMax(0,actor->findData(scenario.value("actorId").toString())));activity->setCurrentIndex(qMax(0,activity->findData(scenario.value("activityId").toString())));form->addRow(tr("검사할 캐릭터"),actor);form->addRow(tr("검사할 활동"),activity);
  for(auto value:documents){auto doc=value.toObject(),data=doc.value("data").toObject();if(doc.value("kind")=="event")event->addItem(data.value("label").toString(data.value("name").toString())+" · "+data.value("id").toString(),data.value("id").toString());}event->setCurrentIndex(qMax(0,event->findData(scenario.value("eventId").toString())));form->addRow(tr("검사할 사건"),event);
  auto routing=new QCheckBox(tr("조건에 따라 연결된 화면 확인"));routing->setObjectName("previewFollowRouting");routing->setChecked(scenario.value("followRouting").toBool());form->addRow(tr("화면 선택"),routing);
  if(canvasMode){routing->setChecked(false);routing->setEnabled(false);routing->setToolTip(tr("편집 캔버스에서는 현재 화면을 유지합니다. 화면 연결은 실제 렌더러 검사에서 확인하세요."));}
  auto rows=new QWidget;auto values=new QFormLayout(rows);form->addRow(rows);
  auto flagScope=new QComboBox;flagScope->setObjectName("previewFlagScope");flagScope->addItem(tr("공용 기록"),"shared");flagScope->addItem(tr("선택 캐릭터의 기록"),"character");flagScope->setCurrentIndex(scenario.value("flagScope")=="character"?1:0);form->insertRow(6,tr("플래그 검사 범위"),flagScope);
  QMap<QString,QSpinBox*> numbers;QMap<QString,QCheckBox*> useNumbers;QMap<QString,QComboBox*> states,booleans;
  for(auto value:attributes){auto a=value.toObject();auto id=a.value("id").toString();auto box=new QWidget;auto line=new QHBoxLayout(box);line->setContentsMargins(0,0,0,0);auto use=new QCheckBox(tr("변경"));use->setObjectName("previewUseAttribute-"+id);use->setChecked(scenario.value("attributes").toObject().contains(id));auto number=new QSpinBox;number->setObjectName("previewAttribute-"+id);number->setRange(a.value("min").toInt(),a.value("max").toInt());number->setValue(scenario.value("attributes").toObject().value(id).toInt(a.value("initial").toInt()));number->setEnabled(use->isChecked());number->setVisible(use->isChecked());auto keep=new QLabel(tr("기준 상태 유지"));keep->setVisible(!use->isChecked());connect(use,&QCheckBox::toggled,number,&QSpinBox::setEnabled);connect(use,&QCheckBox::toggled,number,&QSpinBox::setVisible);connect(use,&QCheckBox::toggled,keep,[keep](bool checked){keep->setVisible(!checked);});line->addWidget(use);line->addWidget(number);line->addWidget(keep);values->addRow(a.value("label").toString(),box);numbers[id]=number;useNumbers[id]=use;}
  for(auto value:project.value("stateGroups").toArray()){auto group=value.toObject();auto id=group.value("id").toString();auto combo=new QComboBox;combo->setObjectName("previewState-"+id);combo->addItem(tr("기준 상태 유지"),"");for(auto v:group.value("values").toArray())combo->addItem(v.toObject().value("name").toString(),v.toObject().value("id").toString());combo->setCurrentIndex(qMax(0,combo->findData(scenario.value("states").toObject().value(id).toString())));values->addRow(group.value("name").toString()+(group.value("scope")=="character"?tr(" · 선택 캐릭터"):tr(" · 공용")),combo);states[id]=combo;}
  for(auto value:flags){auto flag=value.toObject();auto id=flag.value("id").toString();auto combo=new QComboBox;combo->setObjectName("previewFlag-"+id);combo->addItem(tr("기준 상태 유지"),"");combo->addItem(tr("참"),"true");combo->addItem(tr("거짓"),"false");if(scenario.value("flags").toObject().contains(id))combo->setCurrentIndex(scenario.value("flags").toObject().value(id).toBool()?1:2);values->addRow(flag.value("label").toString(id),combo);booleans[id]=combo;}
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel|QDialogButtonBox::Reset);buttons->setObjectName("previewScenarioButtons");layout->addWidget(buttons);connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,[&]{if(active->isChecked()&&source->currentIndex()==1&&savePath->text().trimmed().isEmpty()){notice->setText(tr("검사할 플레이 저장 파일을 선택하세요."));savePath->setFocus();return;}dialog.accept();});connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  connect(buttons->button(QDialogButtonBox::Reset),&QPushButton::clicked,&dialog,[&]{active->setChecked(false);actor->setCurrentIndex(0);activity->setCurrentIndex(0);event->setCurrentIndex(0);source->setCurrentIndex(0);savePath->clear();flagScope->setCurrentIndex(0);routing->setChecked(false);for(auto use:useNumbers)use->setChecked(false);for(auto combo:states)combo->setCurrentIndex(0);for(auto combo:booleans)combo->setCurrentIndex(0);});
  if(dialog.exec()!=QDialog::Accepted)return;
  scenario={{"enabled",active->isChecked()},{"actorId",actor->currentData().toString()},{"activityId",activity->currentData().toString()},{"followRouting",routing->isChecked()}};
  scenario["eventId"]=event->currentData().toString();scenario["flagScope"]=flagScope->currentData().toString();if(source->currentIndex()==1)scenario["savePath"]=savePath->text().trimmed();
  QJsonObject attrs,custom,flagValues;for(auto it=numbers.begin();it!=numbers.end();++it)if(useNumbers[it.key()]->isChecked())attrs[it.key()]=it.value()->value();for(auto it=states.begin();it!=states.end();++it)if(!it.value()->currentData().toString().isEmpty())custom[it.key()]=it.value()->currentData().toString();for(auto it=booleans.begin();it!=booleans.end();++it)if(it.value()->currentIndex()>0)flagValues[it.key()]=it.value()->currentIndex()==1;
  scenario["attributes"]=attrs;scenario["states"]=custom;scenario["flags"]=flagValues;changed();
  status->setText(active->isChecked()?tr("가상 검사 상태 설정됨 · 현재 화면 렌더링을 눌러 확인하세요."):tr("초기 상태 검사로 복원했습니다."));
}

void RendererPreviewDock::showReasons(const QJsonObject& explanation){
  reasons->clear();reasons->setVisible(!explanation.isEmpty());if(explanation.isEmpty())return;
  auto stateRow=new QTreeWidgetItem(reasons,{tr("검사 상태"),(explanation.value("source")=="validated-save"?tr("플레이 저장 · C++ 재현 일치"):tr("새 게임 초기 상태"))+(explanation.value("modified").toBool()?tr(" · 가상 값 변경됨"):QString())});stateRow->setToolTip(0,explanation.value("notice").toString());stateRow->setToolTip(1,explanation.value("notice").toString());
  auto values=new QTreeWidgetItem(reasons,{tr("능력치·자원 · 현재 / 기준"),explanation.value("actorName").toString()});
  for(auto value:explanation.value("values").toArray()){auto row=value.toObject();new QTreeWidgetItem(values,{row.value("label").toString()+" · "+row.value("scopeName").toString(row.value("scope").toString()),QString::number(row.value("value").toInt())+" / "+QString::number(row.value("baseline").toInt())});}
  auto facts=new QTreeWidgetItem(reasons,{tr("상태·기록 · 현재 / 기준"),explanation.value("phase").toString()});for(auto value:explanation.value("facts").toArray()){auto row=value.toObject();new QTreeWidgetItem(facts,{row.value("label").toString()+" · "+row.value("scopeName").toString(),row.value("value").toString()+" / "+row.value("baseline").toString()});}
  const auto event=explanation.value("event").toObject();if(!event.isEmpty()){
    const QMap<QString,QString> reasonNames{{"wrong-phase",tr("발생 가능한 진행 단계가 아님")},{"interaction-only",tr("활동·장소·행동에서 시작하는 사건")},{"requirements",tr("필요 조건 미충족")},{"completed",tr("이미 완료한 사건")},{"selected",tr("발생 대상으로 선택됨")},{"eligible",tr("발생 가능")},{"outside-window",tr("발생 기간 밖")},{"priority",tr("다른 사건의 우선순위가 높음")}};
    auto reason=[&](QString code){return reasonNames.value(code,code);};
    auto eventRow=new QTreeWidgetItem(reasons,{tr("사건 · ")+event.value("label").toString(),(event.value("eligible").toBool()?tr("조건 충족 · "):tr("현재 발생 불가 · "))+reason(event.value("reason").toString())});
    const QMap<QString,QString> operators{{"all",tr("모든 조건")},{"any",tr("하나 이상")},{"not",tr("반대 조건")},{"flag",tr("선택·약속 기록")},{"eventCompleted",tr("선행 사건 완료")},{"compare",tr("수치 비교")},{"always",tr("항상")},{"state",tr("사용자 정의 상태")}};
    const QMap<QString,QString> comparisons{{"gte","≥"},{"gt",">"},{"lte","≤"},{"lt","<"},{"eq","="},{"neq","≠"}};
    auto display=[&](QJsonValue v){return v.isBool()?(v.toBool()?tr("참"):tr("거짓")):v.isDouble()?QString::number(v.toDouble()):v.isString()?v.toString():tr("평가 오류");};
    std::function<void(QTreeWidgetItem*,QJsonObject)> addCondition=[&](QTreeWidgetItem* parent,QJsonObject c){
      const auto op=c.value("op").toString(),label=c.value("subjectLabel").toString();
      const auto text=operators.value(op,op)+(label.isEmpty()?QString():" · "+label);
      auto result=c.value("value").isNull()?tr("평가 오류"):c.value("value").toBool()?tr("충족"):tr("미충족");
      if(c.contains("leftValue"))result+=" · "+display(c.value("leftValue"))+" "+comparisons.value(c.value("comparator").toString(),"=")+" "+display(c.value("rightValue"));
      auto row=new QTreeWidgetItem(parent,{text,result});row->setToolTip(0,text+"\n"+c.value("path").toString());row->setToolTip(1,result);
      for(auto child:c.value("children").toArray())if(child.toObject().value("kind")!="expression")addCondition(row,child.toObject());
    };addCondition(eventRow,event.value("condition").toObject());
    for(auto value:event.value("checks").toArray()){auto c=value.toObject();auto row=new QTreeWidgetItem(eventRow,{tr("발생 시점·경로"),reason(c.value("reason").toString())});row->setToolTip(1,c.value("path").toString());}
  }
  auto top=new QTreeWidgetItem(reasons,{tr("화면 선택"),explanation.value("forced").toBool()?tr("편집 중 화면 고정 (플레이 화면 연결과 별개)"):tr("플레이와 같은 연결 우선순위")});
  for(auto value:explanation.value("routing").toObject().value("candidates").toArray()){auto c=value.toObject();new QTreeWidgetItem(top,{c.value("label").toString()+" → "+c.value("screenLabel").toString(c.value("screenId").toString()),(c.value("selected").toBool()?tr("[연결 선택] "):QString())+c.value("reason").toString()});}
  for(auto value:explanation.value("rules").toArray()){auto e=value.toObject();if(e.value("variants").toArray().isEmpty()&&e.value("visible").toBool()&&e.value("enabled").toBool())continue;auto item=new QTreeWidgetItem(reasons,{e.value("label").toString(),(e.value("visible").toBool()?tr("표시 조건 충족"):tr("숨김"))+(e.value("enabled").toBool()?QString():tr(" · 버튼 조건 불충족"))});item->setData(0,Qt::UserRole,e.value("elementId").toString());for(auto v:e.value("variants").toArray()){auto rule=v.toObject();auto row=new QTreeWidgetItem(item,{rule.value("label").toString(),(rule.value("selected").toBool()?tr("[적용] "):QString())+rule.value("reason").toString()});row->setData(0,Qt::UserRole,e.value("elementId").toString());}}
  reasons->expandAll();facts->setExpanded(false);
}
