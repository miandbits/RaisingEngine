#pragma once
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

class GenreWidgetSettings final:public QWidget {
  QCheckBox *compact=nullptr,*summary=nullptr,*advance=nullptr,*forecast=nullptr,*open=nullptr;
  QListWidget* priorities=nullptr;QComboBox *board=nullptr,*group=nullptr,*activity=nullptr;
  QJsonArray boards,activities;QJsonObject original;
  QJsonObject selectedBoard() const {const int i=board?board->currentIndex():-1;return i>=0&&i<boards.size()?boards.at(i).toObject():QJsonObject{};}
  void fillActivities(const QString& selected={}) {
    const auto old=selected.isNull()?activity->currentData().toString():selected;activity->clear();activity->addItem(tr("카드 미리 선택 안 함"),QString());
    const auto target=selectedBoard().value("board").toObject();QJsonArray ids;
    for(const auto& row:target.value("scheduleGroups").toArray())if(row.toObject().value("id").toString()==group->currentData().toString())ids=row.toObject().value("activityIds").toArray();
    for(const auto& row:activities){auto a=row.toObject();if(!group->currentData().toString().isEmpty()&&!ids.contains(a.value("id")))continue;activity->addItem(a.value("label").toString(a.value("id").toString()),a.value("id").toString());}
    if(!old.isEmpty()&&activity->findData(old)<0)activity->addItem(tr("없는 대상: ")+old,old);activity->setCurrentIndex(qMax(0,activity->findData(old)));
  }
  void fillGroups(const QString& selected={},const QString& selectedActivity={}) {
    group->clear();group->addItem(tr("모든 활동"),QString());const auto target=selectedBoard().value("board").toObject();
    for(const auto& row:target.value("scheduleGroups").toArray()){auto g=row.toObject();group->addItem(g.value("label").toString(g.value("id").toString()),g.value("id").toString());}
    if(!selected.isEmpty()&&group->findData(selected)<0)group->addItem(tr("없는 대상: ")+selected,selected);group->setCurrentIndex(qMax(0,group->findData(selected)));fillActivities(selectedActivity);
  }
public:
  GenreWidgetSettings(const QJsonObject& value,const QJsonObject& project,const QJsonArray& documents,QWidget* parent=nullptr):QWidget(parent),original(value) {
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
    for(const auto& row:documents){auto doc=row.toObject();if(doc.value("kind")=="activity")activities.append(doc.value("data"));}
    if(value.value("kind")=="schedule"){
      auto box=new QGroupBox(tr("활동 보드 표시와 선택 흐름"));box->setObjectName("activityBoardOptions");auto form=new QFormLayout(box);layout->addWidget(box);const auto options=value.value("activityBoard").toObject();
      auto check=[&](const QString& text,const char* name,bool checked){auto c=new QCheckBox(text);c->setObjectName(name);c->setChecked(checked);form->addRow(c);return c;};
      compact=check(tr("압축 카드 배치"),"boardCompact",options.value("density")=="compact");summary=check(tr("카드에 활동 요약 표시"),"boardSummary",options.value("showSummary").toBool());advance=check(tr("적용 후 다음 빈 일정 칸 선택"),"boardAdvance",options.value("advanceAfterApply").toBool());forecast=check(tr("보드 안에 예상 결과 표시"),"boardInlineForecast",options.value("forecast")=="inline");
      auto help=new QLabel(tr("활동 보드에만 적용됩니다. 요약에는 활동 규칙 표시를, 보드 안 예상 결과에는 일정 전체 예상 결과 표시를 켜세요. 선택 순서는 게임 규칙을 바꾸지 않습니다."));help->setWordWrap(true);form->addRow(help);
      priorities=new QListWidget;priorities->setObjectName("boardSummaryAttributes");priorities->setDragDropMode(QAbstractItemView::InternalMove);priorities->setMinimumHeight(130);
      QJsonArray attrs;for(const auto& row:documents){auto doc=row.toObject();if(doc.value("kind")=="attributes")attrs=doc.value("data").toObject().value("attributes").toArray();}
      auto add=[&](const QJsonObject& a,bool checked){auto item=new QListWidgetItem(a.value("label").toString(a.value("id").toString()),priorities);item->setData(Qt::UserRole,a.value("id").toString());item->setCheckState(checked?Qt::Checked:Qt::Unchecked);};
      const auto selected=options.value("summaryAttributeIds").toArray();for(auto id:selected){bool found=false;for(auto a:attrs)if(a.toObject().value("id")==id){add(a.toObject(),true);found=true;break;}if(!found)add(QJsonObject{{"id",id},{"label",tr("없는 대상: ")+id.toString()}},true);}for(auto a:attrs)if(!selected.contains(a.toObject().value("id")))add(a.toObject(),false);
      form->addRow(tr("요약 우선 항목 · 끌어서 순서 변경"),priorities);
    }
    if(value.value("kind")=="button"){
      auto box=new QGroupBox(tr("활동 계획 열기"));auto form=new QFormLayout(box);layout->addWidget(box);open=new QCheckBox(tr("이 버튼으로 활동 계획 열기"));open->setObjectName("openPlannerEnabled");open->setChecked(value.value("action")=="openPlanner");form->addRow(open);
      board=new QComboBox;board->setObjectName("openPlannerBoard");group=new QComboBox;group->setObjectName("openPlannerGroup");activity=new QComboBox;activity->setObjectName("openPlannerActivity");
      for(auto row:project.value("visual").toObject().value("screens").toArray()){auto s=row.toObject();if(s.value("modal").toBool())continue;for(auto item:s.value("elements").toArray()){auto e=item.toObject();if(e.value("kind")!="schedule"||e.value("scheduleLayout")!="activity-board")continue;boards.append(QJsonObject{{"screenId",s.value("id")},{"board",e}});board->addItem(s.value("label").toString(s.value("id").toString())+" / "+e.value("label").toString(e.value("id").toString()));if(s.value("id")==value.value("screenId")&&e.value("id")==value.value("plannerElementId"))board->setCurrentIndex(board->count()-1);}}
      if(open->isChecked()){bool found=false;for(auto b:boards)if(b.toObject().value("screenId")==value.value("screenId")&&b.toObject().value("board").toObject().value("id")==value.value("plannerElementId"))found=true;if(!found){boards.append(QJsonObject{});board->addItem(tr("없는 대상: ")+value.value("plannerElementId").toString());board->setCurrentIndex(board->count()-1);}}
      fillGroups(value.value("plannerGroupId").toString(),value.value("plannerActivityId").toString());
      connect(board,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{fillGroups();});connect(group,qOverload<int>(&QComboBox::activated),this,[this]{fillActivities(QString(""));});
      for(auto field:{board,group,activity}){field->setEnabled(open->isChecked());connect(open,&QCheckBox::toggled,field,&QWidget::setEnabled);}
      form->addRow(tr("대상 화면 / 활동 보드"),board);form->addRow(tr("처음 보여줄 분류"),group);form->addRow(tr("처음 살펴볼 활동"),activity);
      auto help=new QLabel(tr("화면을 열고 카드를 살펴볼 뿐, 일정을 적용하거나 시간을 진행하지 않습니다. 적용·확정은 플레이어가 선택합니다."));help->setWordWrap(true);form->addRow(help);
    }
  }
  QString validate(const QString& layout,bool rules,bool forecasts) const {
    if(compact){bool selectedPriority=false;for(int i=0;i<priorities->count();++i)selectedPriority|=priorities->item(i)->checkState()==Qt::Checked;const bool used=compact->isChecked()||summary->isChecked()||advance->isChecked()||forecast->isChecked()||selectedPriority;if(used&&layout!="activity-board")return tr("활동 보드 표시 방식을 먼저 선택하세요.");if(summary->isChecked()&&!rules)return tr("활동 요약을 위해 활동 규칙 표시를 켜세요.");if(forecast->isChecked()&&!forecasts)return tr("보드 안 예상 결과를 위해 일정 전체 예상 결과 표시를 켜세요.");}
    if(open&&open->isChecked()){const auto target=selectedBoard().value("board").toObject();if(target.isEmpty())return tr("유효한 활동 보드를 선택하세요.");const auto g=group->currentData().toString(),a=activity->currentData().toString();QJsonArray ids;bool found=g.isEmpty();for(auto row:target.value("scheduleGroups").toArray())if(row.toObject().value("id").toString()==g){found=true;ids=row.toObject().value("activityIds").toArray();}if(!found)return tr("유효한 활동 분류를 선택하세요.");if(!a.isEmpty()){bool exists=false;for(auto row:activities)if(row.toObject().value("id")==a)exists=true;if(!exists||(!g.isEmpty()&&!ids.contains(a)))return tr("선택한 분류의 활동을 선택하세요.");}}
    return {};
  }
  void apply(QJsonObject& value) const {
    if(compact){QJsonObject options;if(compact->isChecked())options["density"]="compact";if(summary->isChecked())options["showSummary"]=true;if(advance->isChecked())options["advanceAfterApply"]=true;if(forecast->isChecked())options["forecast"]="inline";QJsonArray ids;for(int i=0;i<priorities->count();++i)if(priorities->item(i)->checkState()==Qt::Checked)ids.append(priorities->item(i)->data(Qt::UserRole).toString());if(!ids.empty())options["summaryAttributeIds"]=ids;if(options.empty())value.remove("activityBoard");else value["activityBoard"]=options;}
    if(open){if(open->isChecked()){const auto b=selectedBoard();value["action"]="openPlanner";value["screenId"]=b.value("screenId");value["plannerElementId"]=b.value("board").toObject().value("id");for(const auto& pair:QList<QPair<QString,QComboBox*>>{{"plannerGroupId",group},{"plannerActivityId",activity}}){const auto id=pair.second->currentData().toString();if(id.isEmpty())value.remove(pair.first);else value[pair.first]=id;}}
      else if(original.value("action")=="openPlanner"){value["action"]="navigate";value.remove("plannerElementId");value.remove("plannerGroupId");value.remove("plannerActivityId");}}
  }
};
