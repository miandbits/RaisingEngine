#include "script-rounds.hpp"
#include "script-authoring.hpp"
#include "logic-editor.hpp"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
QJsonObject always() { return {{"op","always"}}; }
QJsonObject roundChoice(QString id, QString text, QString next, QJsonObject condition = always(), QJsonArray effects = {}) {
  return {{"id",id},{"text",text},{"next",next},{"requirements",condition},{"effects",effects}};
}
QString summary(QJsonValue value) {
  if(value.isArray()) return QObject::tr("효과 %1개").arg(value.toArray().size());
  return value.toObject().value("op")=="always" ? QObject::tr("항상 참") : QObject::tr("설정됨: %1").arg(value.toObject().value("op").toString());
}
}

QJsonObject appendScriptRounds(QJsonObject script, const QJsonObject &s, const QString &after) {
  auto nodes=script.value("nodes").toArray(); const auto rounds=s.value("rounds").toArray();
  if(rounds.isEmpty()||rounds.size()>32||s.value("title").toString().trimmed().isEmpty()) return {};
  QSet<QString> ids; for(auto n:nodes) ids.insert(n.toObject().value("id").toString());
  QString continuation=script.value("entryNodeId").toString(); int source=-1;
  if(!after.isEmpty()) {
    for(int i=0;i<nodes.size();++i) if(nodes[i].toObject().value("id")==after) source=i;
    if(source<0) return {};
    const auto n=nodes[source].toObject();
    if(!QStringList{"scene","dialogue","effect"}.contains(n.value("type").toString())) return {};
    continuation=n.value("next").toString();
  }
  if(!ids.contains(continuation)) return {};
  QString prefix;
  for(int n=1;;++n) { prefix=QString("rounds-%1-").arg(n); bool used=false; for(const auto &id:ids) if(id.startsWith(prefix)) used=true; if(!used) break; }
  const auto id=[&](QString suffix){return prefix+suffix;};
  const auto dialogue=[&](QString suffix,QString text,QString next) { nodes.append(QJsonObject{{"id",id(suffix)},{"type","dialogue"},{"speakerId",QJsonValue::Null},{"text",text},{"next",next}}); };
  const auto effect=[&](QString suffix,QJsonArray effects,QString next) { nodes.append(QJsonObject{{"id",id(suffix)},{"type","effect"},{"effects",effects},{"next",next}}); };
  nodes.append(QJsonObject{{"id",id("entry")},{"type","choice"},{"prompt",s.value("title")},{"choices",QJsonArray{
    roundChoice("enter",s.value("enterText").toString(),id("r1-choice"),s.value("requirements").toObject(),s.value("entryEffects").toArray()),
    roundChoice("leave",s.value("leaveText").toString(),id("withdraw"))}}});
  for(int i=0;i<rounds.size();++i) {
    const auto r=rounds[i].toObject(); const auto suffix=QString("r%1-").arg(i+1);
    const auto next=i+1<rounds.size()?id(QString("r%1-choice").arg(i+2)):id("win-reward");
    nodes.append(QJsonObject{{"id",id(suffix+"choice")},{"type","choice"},{"prompt",r.value("label")},{"choices",QJsonArray{
      roundChoice("attempt",r.value("choiceText").toString(),id(suffix+"check"),always(),r.value("choiceEffects").toArray()),
      roundChoice("leave",s.value("leaveText").toString(),id("withdraw"))}}});
    nodes.append(QJsonObject{{"id",id(suffix+"check")},{"type","branch"},{"condition",r.value("condition")},{"then",id(suffix+"reward")},{"else",id("loss-reward")}});
    effect(suffix+"reward",r.value("effects").toArray(),id(suffix+"result"));
    dialogue(suffix+"result",r.value("resultText").toString(),next);
  }
  effect("win-reward",s.value("winEffects").toArray(),id("win"));
  dialogue("win",s.value("winText").toString(),continuation);
  effect("loss-reward",s.value("lossEffects").toArray(),id("loss"));
  dialogue("loss",s.value("lossText").toString(),continuation);
  dialogue("withdraw",s.value("withdrawText").toString(),continuation);
  if(source<0) script["entryNodeId"]=id("entry");
  else {auto n=nodes[source].toObject(); n["next"]=id("entry"); nodes[source]=n;}
  script["nodes"]=nodes;
  return scriptGraphIssues(script).isEmpty()?script:QJsonObject{};
}

ScriptRoundsDialog::ScriptRoundsDialog(QJsonObject context,QJsonObject script,QString selected,QWidget *parent):QDialog(parent) {
  setObjectName("scriptRoundsDialog"); setWindowTitle(tr("라운드 흐름 추가")); resize(1060,780);
  auto box=new QVBoxLayout(this);auto help=new QLabel(tr("참가 선택 → 라운드별 도전·중단 → 조건 판정 → 결과·보상을 기존 대본 노드로 만듭니다. 조건과 효과는 직접 설정하세요. 기본 조건은 항상 참, 기본 효과는 없음입니다. 전투 토너먼트 기능은 아닙니다. 생성 후 대본 작업실에서 선택지와 연결을 더 편집할 수 있습니다."));help->setWordWrap(true);box->addWidget(help);
  auto form=new QFormLayout;box->addLayout(form);
  auto attach=new QComboBox;attach->setObjectName("roundsAttachment");attach->addItem(tr("대본 시작 앞"),QString());
  for(auto v:script.value("nodes").toArray()){const auto n=v.toObject();if(QStringList{"scene","dialogue","effect"}.contains(n.value("type").toString()))attach->addItem(tr("%1 다음").arg(n.value("id").toString()),n.value("id"));}
  if(attach->findData(selected)>=0) attach->setCurrentIndex(attach->findData(selected));
  form->addRow(tr("삽입 위치"),attach);
  auto text=[&](QString key,QString label,QString initial){auto e=new QLineEdit(initial);e->setObjectName("rounds-"+key);form->addRow(label,e);return e;};
  auto title=text("title",tr("참가 질문"),tr("참가할까요?"));auto enter=text("enterText",tr("참가 선택"),tr("참가한다"));auto leave=text("leaveText",tr("중단 선택"),tr("참가하지 않거나 중단한다"));
  QJsonObject settings{{"requirements",always()},{"entryEffects",QJsonArray{}},{"winEffects",QJsonArray{}},{"lossEffects",QJsonArray{}}};
  // Captured state must outlive nested exec() callbacks; owned by the dialog.
  setProperty("roundsSettings",settings);
  auto controls=new QHBoxLayout;box->addLayout(controls);
  auto logicButton=[&](QString key,QString label,QString kind){auto b=new QPushButton(label+" · "+summary(settings.value(key)));b->setObjectName("rounds-"+key);controls->addWidget(b);connect(b,&QPushButton::clicked,this,[this,context,key,label,kind,b]{auto s=property("roundsSettings").toJsonObject();LogicEditor editor(s.value(key),kind,context,this);if(editor.exec()==QDialog::Accepted){s[key]=editor.value();setProperty("roundsSettings",s);b->setText(label+" · "+summary(editor.value()));}});};
  logicButton("requirements",tr("참가 조건"),"condition");logicButton("entryEffects",tr("참가 시 효과"),"effects");logicButton("winEffects",tr("최종 통과 보상"),"effects");logicButton("lossEffects",tr("실패 시 효과"),"effects");
  auto table=new QTableWidget(0,7);table->setObjectName("roundsTable");table->setHorizontalHeaderLabels({tr("라운드 질문"),tr("도전 선택 문구"),tr("도전 시 효과"),tr("통과 조건"),tr("통과 보상"),tr("통과 결과 문구"),tr("순서")});table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table->setSelectionBehavior(QAbstractItemView::SelectRows);box->addWidget(table,1);
  auto append=[this,table,context]{if(table->rowCount()>=32)return;const int row=table->rowCount();table->insertRow(row);table->setItem(row,0,new QTableWidgetItem(tr("라운드 %1").arg(row+1)));table->setItem(row,1,new QTableWidgetItem(tr("도전한다")));table->setItem(row,5,new QTableWidgetItem(tr("라운드를 통과했습니다.")));
    for(int column=2;column<=4;++column){const QString kind=column==3?"condition":"effects";QJsonValue initial=column==3?QJsonValue(always()):QJsonValue(QJsonArray{});auto b=new QPushButton(summary(initial));b->setProperty("logicValue",initial);b->setObjectName(QString("roundsLogic-%1-%2").arg(row).arg(column));table->setCellWidget(row,column,b);connect(b,&QPushButton::clicked,this,[this,b,context,kind]{LogicEditor editor(b->property("logicValue").toJsonValue(),kind,context,this);if(editor.exec()==QDialog::Accepted){b->setProperty("logicValue",editor.value());b->setText(summary(editor.value()));}});}
    auto order=new QTableWidgetItem(QString::number(row+1));order->setFlags(order->flags()&~Qt::ItemIsEditable);table->setItem(row,6,order);table->setCurrentCell(row,0);
  };
  auto rowButtons=new QHBoxLayout;box->addLayout(rowButtons);auto add=new QPushButton(tr("라운드 추가 (최대 32)"));add->setObjectName("roundsAdd");rowButtons->addWidget(add);connect(add,&QPushButton::clicked,this,append);auto remove=new QPushButton(tr("선택 라운드 삭제"));remove->setObjectName("roundsRemove");rowButtons->addWidget(remove);connect(remove,&QPushButton::clicked,this,[table]{if(table->rowCount()>1&&table->currentRow()>=0){table->removeRow(table->currentRow());for(int i=0;i<table->rowCount();++i)table->item(i,6)->setText(QString::number(i+1));}});rowButtons->addStretch();append();
  auto win=text("winText",tr("최종 통과 문구"),tr("모든 라운드를 통과했습니다."));auto loss=text("lossText",tr("실패 문구"),tr("이번 도전은 여기서 마칩니다."));auto withdraw=text("withdrawText",tr("중단 문구"),tr("참가를 중단했습니다."));
  auto notice=new QLabel(tr("통과 보상은 각 라운드 결과 대사 전에, 최종 보상은 최종 결과 전에 한 번 적용됩니다. 실패·중단·완료 후 모두 기존 대본으로 돌아갑니다. 이 창의 적용은 초안만 바꾸며, 작업실의 저장에서 한 번에 반영됩니다."));notice->setWordWrap(true);box->addWidget(notice);auto error=new QLabel;error->setObjectName("roundsError");error->setWordWrap(true);box->addWidget(error);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel);buttons->setObjectName("roundsButtons");box->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
  connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,this,[=,this]{auto s=property("roundsSettings").toJsonObject();for(auto e:{title,enter,leave,win,loss,withdraw}){if(e->text().trimmed().isEmpty()){error->setText(tr("질문·선택·결과 문구를 모두 입력하세요."));return;}s[e->objectName().mid(7)]=e->text();}QJsonArray rounds;for(int row=0;row<table->rowCount();++row){QJsonObject r;for(auto p:QList<QPair<int,QString>>{{0,"label"},{1,"choiceText"},{5,"resultText"}}){auto value=table->item(row,p.first)->text();if(value.trimmed().isEmpty()){error->setText(tr("라운드 문구를 모두 입력하세요."));return;}r[p.second]=value;}for(auto p:QList<QPair<int,QString>>{{2,"choiceEffects"},{3,"condition"},{4,"effects"}})r[p.second]=table->cellWidget(row,p.first)->property("logicValue").toJsonValue();rounds.append(r);}s["rounds"]=rounds;auto built=appendScriptRounds(script,s,attach->currentData().toString());if(built.isEmpty()){error->setText(tr("삽입 위치나 대본 연결을 확인하세요. 기존 대본의 잘못된 연결을 먼저 수정해야 합니다."));return;}result=built;accept();});
}
