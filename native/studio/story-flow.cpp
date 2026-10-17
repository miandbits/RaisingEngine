#include "story-flow.hpp"
#include "studio.hpp"
#include <QtWidgets>
#include <memory>

namespace {
QString displayName(QJsonValue value) {
  if(value.isString())return value.toString();
  return QObject::tr("다국어 이름");
}
QString nameFor(const QJsonObject &view, QString kind, QString id) {
  for(auto v:view.value("documents").toArray()){auto d=v.toObject(),data=d.value("data").toObject();if(d.value("kind")==kind&&data.value("id")==id)return displayName(data.value(kind=="character"?"name":"label"));}
  return QObject::tr("연결 대상 없음");
}
}

StoryFlowDialog::StoryFlowDialog(QJsonObject context, QWidget *parent, std::function<QString()> guard,
    std::function<void(QJsonArray,std::function<void(QString)>)> apply):QDialog(parent) {
  setObjectName("storyFlowDialog");setWindowTitle(tr("이야기 제작 · 인물에서 후속 사건까지"));resize(1120,810);
  auto outer=new QVBoxLayout(this);
  auto intro=new QLabel(tr("이름을 고르고 대사를 쓰면 활동 → 만남 → 선택 → 관계 변화 → 후속 사건이 연결됩니다. 내부 연결 이름은 자동으로 관리합니다."));intro->setWordWrap(true);outer->addWidget(intro);
  auto selectorRow=new QHBoxLayout;auto selector=new QComboBox;selector->setObjectName("storyFlowSelector");selector->addItem(tr("＋ 새 이야기 만들기"),QJsonObject{});
  QJsonObject project;for(auto v:context.value("documents").toArray())if(v.toObject().value("kind")=="project")project=v.toObject().value("data").toObject();
  for(auto v:project.value("authoring").toObject().value("storyFlows").toArray())selector->addItem(v.toObject().value("name").toString(),v.toObject());
  selectorRow->addWidget(new QLabel(tr("편집할 이야기")));selectorRow->addWidget(selector,1);outer->addLayout(selectorRow);
  auto split=new QHBoxLayout;outer->addLayout(split,1);auto steps=new QListWidget;steps->setObjectName("storyFlowSteps");steps->setMaximumWidth(195);
  steps->addItems({tr("1  인물과 관계"),tr("2  활동 배정"),tr("3  만남과 대사"),tr("4  선택과 후속 사건"),tr("5  연결 확인")});split->addWidget(steps);
  auto pages=new QStackedWidget;pages->setObjectName("storyFlowPages");split->addWidget(pages,1);
  auto page=[&](QString title){auto w=new QWidget;auto l=new QVBoxLayout(w);auto h=new QLabel(title);h->setStyleSheet("font-size:18px;font-weight:600");l->addWidget(h);auto scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);auto body=new QWidget;auto form=new QFormLayout(body);form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);scroll->setWidget(body);l->addWidget(scroll,1);pages->addWidget(w);return form;};
  auto text=[&](QFormLayout*f,QString label,QString object){auto e=new QLineEdit;e->setObjectName(object);f->addRow(label,e);return e;};
  auto number=[&](QFormLayout*f,QString label,QString object,int min,int max){auto e=new QSpinBox;e->setObjectName(object);e->setRange(min,max);f->addRow(label,e);return e;};
  auto multiline=[&](QFormLayout*f,QString label,QString object){auto e=new QPlainTextEdit;e->setObjectName(object);e->setMaximumHeight(125);e->setMinimumHeight(75);f->addRow(label,e);return e;};
  auto namedCombo=[&](QString kind,QString first,QString object){auto c=new QComboBox;c->setObjectName(object);c->addItem(first,QString());QMap<QString,int> counts;
    for(auto v:context.value("documents").toArray()){auto d=v.toObject(),data=d.value("data").toObject();if(d.value("kind")!=kind)continue;QString n=displayName(data.value(kind=="character"?"name":"label"));int occurrence=++counts[n];if(occurrence>1)n+=tr(" (%1)").arg(occurrence);c->addItem(n,data.value("id").toString());}return c;};
  auto p1=page(tr("누구를 만나고, 어떤 관계를 쌓나요?"));
  auto title=text(p1,tr("이야기 이름"),"storyName");title->setPlaceholderText(tr("예: 약초밭에서 시작된 우정"));
  auto person=namedCombo("character",tr("＋ 새 인물 만들기"),"storyCharacter");p1->addRow(tr("만날 인물"),person);
  auto personName=text(p1,tr("새 인물 이름"),"storyCharacterName");
  auto bond=new QComboBox;bond->setObjectName("storyRelationship");p1->addRow(tr("변화시킬 관계"),bond);
  auto bondName=text(p1,tr("새 관계 이름"),"storyRelationshipName");
  auto initial=number(p1,tr("관계 시작값"),"storyRelationshipInitial",-100,100);
  auto relationHelp=new QLabel(tr("새 관계는 -100~100 범위입니다. 선택지마다 증감량을 정합니다. 기존 인물과 관계도 이름으로 고를 수 있습니다."));relationHelp->setWordWrap(true);p1->addRow(relationHelp);
  auto p2=page(tr("어떤 활동을 마치면 만남이 열리나요?"));
  auto actor=new QLabel(tr("일정을 실행하는 주인공: %1").arg(nameFor(context,"character",project.value("protagonistId").toString())));p2->addRow(actor);
  auto activity=namedCombo("activity",tr("＋ 새 활동 만들기"),"storyActivity");p2->addRow(tr("배정할 활동"),activity);
  auto activityName=text(p2,tr("새 활동 이름"),"storyActivityName");
  auto attr=[&](QString object){auto c=new QComboBox;c->setObjectName(object);c->addItem(tr("사용 안 함"),QString());for(auto v:context.value("documents").toArray())if(v.toObject().value("kind")=="attributes")for(auto a:v.toObject().value("data").toObject().value("attributes").toArray())c->addItem(displayName(a.toObject().value("label")),a.toObject().value("id").toString());return c;};
  auto costAttr=attr("storyCostAttribute");p2->addRow(tr("소비할 자원"),costAttr);auto cost=number(p2,tr("활동 1회 비용"),"storyCost",0,1000000);
  auto gainAttr=attr("storyGainAttribute");p2->addRow(tr("변화시킬 능력"),gainAttr);auto gain=number(p2,tr("활동 1회 변화"),"storyGain",-1000000,1000000);
  auto schedule=new QComboBox;schedule->setObjectName("storyScheduleGroup");schedule->addItem(tr("기본 일정 목록 · 분류 없는 화면"),QJsonObject{});
  for(auto sv:project.value("visual").toObject().value("screens").toArray()){auto sc=sv.toObject();for(auto ev:sc.value("elements").toArray()){auto e=ev.toObject();if(e.value("kind")!="schedule")continue;for(auto gv:e.value("scheduleGroups").toArray()){auto g=gv.toObject();schedule->addItem(displayName(sc.value("label"))+" / "+displayName(g.value("label")),QJsonObject{{"screenId",sc.value("id")},{"elementId",e.value("id")},{"groupId",g.value("id")}});}}}
  if(schedule->count()>1)schedule->setCurrentIndex(1);p2->addRow(tr("플레이에서 고를 일정 분류"),schedule);
  auto activityHelp=new QLabel(tr("새 활동은 비용과 효과를 한 번 적용합니다. 기존 활동의 규칙은 그대로 유지합니다. 플레이에서 이 활동을 일정에 넣고 마치면 만남이 열립니다. 관계 변화는 프로젝트의 공유 관계에 반영됩니다."));activityHelp->setWordWrap(true);p2->addRow(activityHelp);
  const int total=qMax(1,project.value("durationMonths").toInt()*project.value("periodsPerMonth").toInt());
  auto p3=page(tr("첫 만남을 씁니다"));auto encounter=text(p3,tr("만남 제목"),"storyEncounterTitle");auto dialogue=multiline(p3,tr("인물이 하는 말"),"storyEncounterText");
  auto prompt=text(p3,tr("플레이어에게 물을 질문"),"storyPrompt");
  auto from=number(p3,tr("몇 번째 일정부터"),"storyFrom",1,total);auto through=number(p3,tr("몇 번째 일정까지"),"storyThrough",1,total);
  auto triggerHelp=new QLabel(tr("이 기간 안에서 활동을 한 번 이상 마친 뒤, 일정 끝에 한 번 발생합니다. 숫자는 전체 게임의 일정 순서이며 첫 일정은 1입니다."));triggerHelp->setWordWrap(true);p3->addRow(triggerHelp);
  auto p4=page(tr("선택의 결과와 다음 이야기를 연결합니다"));auto choices=new QListWidget;choices->setObjectName("storyChoices");choices->setMaximumHeight(105);p4->addRow(tr("선택지"),choices);
  auto choiceButtons=new QWidget;auto cb=new QHBoxLayout(choiceButtons);cb->setContentsMargins(0,0,0,0);auto add=new QPushButton(tr("＋ 선택지 추가")),remove=new QPushButton(tr("선택지 삭제"));add->setObjectName("storyChoiceAdd");remove->setObjectName("storyChoiceRemove");cb->addWidget(add);cb->addWidget(remove);p4->addRow(choiceButtons);
  auto choiceLabel=text(p4,tr("버튼에 표시할 문구"),"storyChoiceText");auto reply=multiline(p4,tr("선택 후 인물의 응답"),"storyChoiceReply");auto delta=number(p4,tr("관계 변화 (+상승 / -감소)"),"storyChoiceDelta",-1000000,1000000);
  auto follow=new QCheckBox(tr("이 선택 뒤 후속 사건 연결"));follow->setObjectName("storyFollowupEnabled");p4->addRow(follow);
  auto followTitle=text(p4,tr("후속 사건 제목"),"storyFollowupTitle");auto followText=multiline(p4,tr("후속 사건 대사"),"storyFollowupText");
  auto minimum=number(p4,tr("관계가 이 값 이상이면"),"storyFollowupMinimum",-100,100);auto followFrom=number(p4,tr("몇 번째 일정부터 기다릴까"),"storyFollowupFrom",1,total);
  auto followHelp=new QLabel(tr("선택을 실제로 경험하고 만남을 끝낸 뒤, 관계와 일정 기준을 모두 만족하면 후속 사건이 한 번 열립니다. 선택하지 않은 쪽의 사건은 열리지 않습니다. 같은 일정의 사건 처리 개수는 기존 프로젝트 설정을 따릅니다."));followHelp->setWordWrap(true);p4->addRow(followHelp);
  auto p5=page(tr("이 연결대로 플레이됩니다"));auto graph=new QTreeWidget;graph->setObjectName("storyFlowPreview");graph->setHeaderLabels({tr("흐름"),tr("내용 / 조건")});graph->header()->setSectionResizeMode(0,QHeaderView::ResizeToContents);graph->header()->setSectionResizeMode(1,QHeaderView::Stretch);graph->setMinimumHeight(330);p5->addRow(graph);
  auto previewHelp=new QLabel(tr("항목을 두 번 누르면 해당 제작 단계로 돌아갑니다. 적용은 연결된 문서를 함께 바꾸며 실행 취소 한 번으로 되돌릴 수 있습니다. 적용 뒤 프로젝트 저장과 처음부터 플레이를 사용하세요."));previewHelp->setWordWrap(true);p5->addRow(previewHelp);
  auto error=new QLabel;error->setObjectName("storyFlowError");error->setWordWrap(true);error->setStyleSheet("color:#ffb2ae");outer->addWidget(error);
  auto controls=new QHBoxLayout;auto back=new QPushButton(tr("이전")),next=new QPushButton(tr("다음")),save=new QPushButton(tr("프로젝트에 적용")),cancel=new QPushButton(tr("닫기"));back->setObjectName("storyBack");next->setObjectName("storyNext");save->setObjectName("storyApply");controls->addWidget(back);controls->addWidget(next);controls->addStretch();controls->addWidget(save);controls->addWidget(cancel);outer->addLayout(controls);
  struct Draft { QJsonObject value; QJsonArray rows; int row=-1; bool filling=false; bool dirty=false; };
  auto draft=std::make_shared<Draft>();
  auto storeChoice=[=]{if(draft->filling||draft->row<0||draft->row>=draft->rows.size())return;auto r=draft->rows[draft->row].toObject();r["label"]=choiceLabel->text();r["reply"]=reply->toPlainText();r["delta"]=delta->value();r["followupEnabled"]=follow->isChecked();r["followupTitle"]=followTitle->text();r["followupText"]=followText->toPlainText();r["minimumRelationship"]=minimum->value();r["followupFrom"]=followFrom->value();draft->rows[draft->row]=r;if(auto item=choices->item(draft->row))item->setText(choiceLabel->text().isEmpty()?QObject::tr("새 선택지"):choiceLabel->text());draft->dirty=true;};
  auto fillChoice=[=](int row){storeChoice();draft->filling=true;draft->row=row;auto r=row>=0&&row<draft->rows.size()?draft->rows.at(row).toObject():QJsonObject{};choiceLabel->setText(r.value("label").toString());reply->setPlainText(r.value("reply").toString());delta->setValue(r.value("delta").toInt());follow->setChecked(r.value("followupEnabled").toBool());followTitle->setText(r.value("followupTitle").toString());followText->setPlainText(r.value("followupText").toString());minimum->setValue(r.value("minimumRelationship").toInt());followFrom->setValue(r.value("followupFrom").toInt(qMin(2,total)));for(QWidget*w:QList<QWidget*>{followTitle,followText,minimum,followFrom})w->setEnabled(follow->isChecked());draft->filling=false;};
  auto refreshBonds=[=]{const bool fresh=person->currentData().toString().isEmpty();personName->setEnabled(fresh);bond->clear();bond->addItem(QObject::tr("＋ 새 관계 만들기"),QString());if(!fresh)for(auto v:context.value("documents").toArray()){auto d=v.toObject().value("data").toObject();if(d.value("id")!=person->currentData().toString())continue;for(auto r:d.value("relationships").toArray())bond->addItem(displayName(r.toObject().value("label")),r.toObject().value("id").toString());}};
  connect(person,qOverload<int>(&QComboBox::currentIndexChanged),this,[=]{refreshBonds();});
  connect(bond,qOverload<int>(&QComboBox::currentIndexChanged),this,[=]{const bool fresh=bond->currentData().toString().isEmpty();bondName->setEnabled(fresh);initial->setEnabled(fresh);int lo=-100,hi=100;for(auto v:context.value("documents").toArray()){auto d=v.toObject().value("data").toObject();if(d.value("id")!=person->currentData().toString())continue;for(auto rv:d.value("relationships").toArray()){auto r=rv.toObject();if(r.value("id")==bond->currentData().toString()){lo=r.value("min").toInt();hi=r.value("max").toInt();}}}minimum->setRange(lo,hi);});
  connect(activity,qOverload<int>(&QComboBox::currentIndexChanged),this,[=]{bool fresh=activity->currentData().toString().isEmpty();for(QWidget*w:QList<QWidget*>{activityName,costAttr,cost,gainAttr,gain})w->setEnabled(fresh);});
  connect(follow,&QCheckBox::toggled,this,[=](bool enabled){for(QWidget*w:QList<QWidget*>{followTitle,followText,minimum,followFrom})w->setEnabled(enabled);});
  connect(choices,&QListWidget::currentRowChanged,this,fillChoice);
  // Accessibility selection can change selection without changing the current index.
  for(auto *list:{choices,steps})connect(list,&QListWidget::itemSelectionChanged,this,[list]{
    const auto selected=list->selectedItems();
    if(selected.size()==1&&list->currentItem()!=selected.first())list->setCurrentItem(selected.first());
  });
  connect(choiceLabel,&QLineEdit::textEdited,this,[=]{storeChoice();});connect(reply,&QPlainTextEdit::textChanged,this,storeChoice);connect(delta,qOverload<int>(&QSpinBox::valueChanged),this,[=]{storeChoice();});
  auto collect=[=]{storeChoice();auto s=draft->value;s["name"]=title->text().trimmed();s["newCharacter"]=person->currentData().toString().isEmpty();s["characterId"]=s.value("newCharacter").toBool()?s.value("id").toString()+"-person":person->currentData().toString();s["characterName"]=personName->text();s["newRelationship"]=bond->currentData().toString().isEmpty();s["relationshipId"]=s.value("newRelationship").toBool()?s.value("id").toString()+"-bond":bond->currentData().toString();s["relationshipLabel"]=bondName->text();s["relationshipInitial"]=initial->value();s["newActivity"]=activity->currentData().toString().isEmpty();s["activityId"]=s.value("newActivity").toBool()?s.value("id").toString()+"-activity":activity->currentData().toString();s["activityLabel"]=activityName->text();s["costAttributeId"]=costAttr->currentData().toString();s["cost"]=cost->value();s["gainAttributeId"]=gainAttr->currentData().toString();s["gain"]=gain->value();s["scheduleLink"]=schedule->currentData().toJsonObject();s["encounterTitle"]=encounter->text();s["encounterText"]=dialogue->toPlainText();s["prompt"]=prompt->text();s["fromPeriod"]=from->value();s["throughPeriod"]=through->value();s["choices"]=draft->rows;return s;};
  auto render=[=]{auto s=collect();graph->clear();auto addRow=[=](QTreeWidgetItem*parent,QString name,QString detail,int step){auto item=parent?new QTreeWidgetItem(parent,{name,detail}):new QTreeWidgetItem(graph,{name,detail});item->setData(0,Qt::UserRole,step);return item;};QString who=s.value("newCharacter").toBool()?personName->text():person->currentText();QString rel=s.value("newRelationship").toBool()?bondName->text():bond->currentText();
    auto p=addRow(nullptr,who,rel+QObject::tr(" 관계"),0);auto a=addRow(p,s.value("newActivity").toBool()?activityName->text():activity->currentText(),QObject::tr("이 활동을 일정에 배정하고 완료"),1);auto m=addRow(a,encounter->text(),QObject::tr("%1~%2번째 일정 끝 · 최초 1회").arg(from->value()).arg(through->value()),2);
    for(auto v:draft->rows){auto r=v.toObject();auto c=addRow(m,r.value("label").toString(),rel+" "+QString::number(r.value("delta").toInt())+" · "+r.value("reply").toString(),3);if(r.value("followupEnabled").toBool())addRow(c,r.value("followupTitle").toString(),QObject::tr("이 선택 + %1 ≥ %2 + %3번째 일정부터").arg(rel).arg(r.value("minimumRelationship").toInt()).arg(r.value("followupFrom").toInt()),3);}
    graph->expandAll();error->setText(makeStoryFlowPlan(context,s).error);};
  connect(graph,&QTreeWidget::itemDoubleClicked,this,[=](QTreeWidgetItem*item,int){steps->setCurrentRow(item->data(0,Qt::UserRole).toInt());});
  connect(steps,&QListWidget::currentRowChanged,this,[=](int row){pages->setCurrentIndex(row);back->setEnabled(row>0);next->setEnabled(row<4);if(row==4)render();});
  connect(back,&QPushButton::clicked,this,[=]{steps->setCurrentRow(qMax(0,steps->currentRow()-1));});connect(next,&QPushButton::clicked,this,[=]{steps->setCurrentRow(qMin(4,steps->currentRow()+1));});
  connect(add,&QPushButton::clicked,this,[=]{storeChoice();if(draft->rows.size()>=8)return;auto cid="c-"+QUuid::createUuid().toString(QUuid::Id128).left(8);draft->rows.append(QJsonObject{{"id",cid},{"label",""},{"reply",""},{"delta",0},{"followupEnabled",false},{"followupFrom",qMin(2,total)},{"minimumRelationship",0}});choices->addItem(QObject::tr("새 선택지"));choices->setCurrentRow(draft->rows.size()-1);});
  connect(remove,&QPushButton::clicked,this,[=]{if(draft->rows.size()<=2){error->setText(QObject::tr("선택지는 최소 두 개를 남겨 주세요."));return;}const int row=choices->currentRow();if(row<0)return;draft->filling=true;draft->rows.removeAt(row);delete choices->takeItem(row);draft->row=-1;draft->filling=false;choices->setCurrentRow(0);fillChoice(0);});
  auto load=[=](QJsonObject s){draft->filling=true;draft->value=s;draft->row=-1;draft->rows=s.value("choices").toArray();title->setText(s.value("name").toString());person->setCurrentIndex(s.value("newCharacter").toBool()?0:person->findData(s.value("characterId").toString()));refreshBonds();personName->setText(s.value("characterName").toString());bond->setCurrentIndex(s.value("newRelationship").toBool()?0:bond->findData(s.value("relationshipId").toString()));bondName->setText(s.value("relationshipLabel").toString());initial->setValue(s.value("relationshipInitial").toInt());activity->setCurrentIndex(s.value("newActivity").toBool()?0:activity->findData(s.value("activityId").toString()));activityName->setText(s.value("activityLabel").toString());costAttr->setCurrentIndex(qMax(0,costAttr->findData(s.value("costAttributeId").toString())));cost->setValue(s.value("cost").toInt());gainAttr->setCurrentIndex(qMax(0,gainAttr->findData(s.value("gainAttributeId").toString())));gain->setValue(s.value("gain").toInt());if(s.contains("scheduleLink"))schedule->setCurrentIndex(qMax(0,schedule->findData(s.value("scheduleLink").toObject())));encounter->setText(s.value("encounterTitle").toString());dialogue->setPlainText(s.value("encounterText").toString());prompt->setText(s.value("prompt").toString());from->setValue(s.value("fromPeriod").toInt(1));through->setValue(s.value("throughPeriod").toInt(total));choices->clear();for(auto v:draft->rows)choices->addItem(v.toObject().value("label").toString(QObject::tr("새 선택지")));draft->filling=false;const bool existing=s.contains("ownedDocuments");person->setEnabled(!existing);bond->setEnabled(!existing);activity->setEnabled(!existing);choices->setCurrentRow(0);fillChoice(0);steps->setCurrentRow(0);error->clear();draft->dirty=false;};
  auto blank=[=]{auto s=newStoryFlowSettings(context);s["gain"]=0;QJsonArray rows;for(int i=0;i<2;++i)rows.append(QJsonObject{{"id","choice-"+QString::number(i+1)},{"label",""},{"reply",""},{"delta",0},{"followupEnabled",false},{"followupFrom",qMin(2,total)},{"minimumRelationship",0}});s["choices"]=rows;return s;};
  // Switching stories does not silently discard the current draft.
  auto drafts=std::make_shared<QMap<int,QJsonObject>>();auto selection=std::make_shared<int>(0);
  connect(selector,qOverload<int>(&QComboBox::currentIndexChanged),this,[=](int index){(*drafts)[*selection]=collect();*selection=index;load(drafts->contains(index)?drafts->value(index):(index==0?blank():selector->itemData(index).toJsonObject()));});
  connect(cancel,&QPushButton::clicked,this,[=]{reject();});
  connect(save,&QPushButton::clicked,this,[=]{const auto reason=guard();if(!reason.isEmpty()){error->setText(reason);return;}auto plan=makeStoryFlowPlan(context,collect());if(!plan.error.isEmpty()){error->setText(plan.error);return;}save->setEnabled(false);QPointer<StoryFlowDialog> self(this);QPointer<QLabel> message(error);QPointer<QPushButton> button(save);apply(plan.edits,[self,message,button](QString e){if(!self)return;button->setEnabled(true);if(!e.isEmpty())message->setText(e);else self->accept();});});
  load(blank());
}

void StudioWindow::editStoryFlow() {
  if(bridge.busy()||projectData().empty())return;
  const auto revision=view.value("revision"),root=view.value("root");
  auto *dialog=new StoryFlowDialog(view,this,[this,revision,root]{if(bridge.busy())return tr("다른 작업을 처리 중입니다. 잠시 뒤 적용하세요.");if(view.value("revision")!=revision||view.value("root")!=root)return tr("프로젝트가 다른 곳에서 변경되었습니다. 입력을 덮어쓰지 않았습니다. 창을 다시 열어 현재 내용을 확인하세요.");return QString{};},
    [this](QJsonArray edits,std::function<void(QString)> done){request("edit",{{"edit",QJsonObject{{"type","batch"},{"edits",edits}}}},[done](QJsonObject reply){done(reply.value("error").toString());});});
  // Return from accessibility Invoke before entering the dialog's input loop.
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->open();
}
