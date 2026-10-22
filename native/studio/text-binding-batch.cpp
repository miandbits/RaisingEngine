#include <QObject>
#include "studio.hpp"
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
const QStringList connectionKeys{"binding","dataBinding","attributeId","characterId","relationshipId","itemId","userFieldId","valueFormat"};
const QMap<QString,QString> bindingLabels{
  {"data-value",QObject::tr("공용·개인 데이터")},
  {"narrative-actor",QObject::tr("현재 사건의 대상 인물")},
  {"none",QObject::tr("직접 입력")},{"title",QObject::tr("게임 제목")},{"description",QObject::tr("게임 설명")},
  {"calendar",QObject::tr("날짜")},{"calendar-year",QObject::tr("연도")},{"calendar-month",QObject::tr("월")},{"calendar-day",QObject::tr("일")},{"calendar-weekday",QObject::tr("요일")},
  {"schedule-prompt",QObject::tr("일정 안내")},{"ending",QObject::tr("엔딩")},{"speaker",QObject::tr("대화 인물")},{"story",QObject::tr("대사")},{"location",QObject::tr("현재 장소")},
  {"attribute-value",QObject::tr("능력·자원")},{"relationship-value",QObject::tr("인물 관계")},{"item-count",QObject::tr("물품 수량")},{"user-field",QObject::tr("사용자 입력값")}
};
QStringList requiredBindingKeys(const QString &binding){
  if(binding=="data-value")return {"dataBinding"};
  if(binding=="attribute-value")return {"attributeId"};
  if(binding=="relationship-value")return {"characterId","relationshipId"};
  if(binding=="item-count")return {"itemId"};
  if(binding=="user-field")return {"userFieldId"};
  return {};
}
QJsonObject bindingPayload(const QJsonObject &source){
  QJsonObject result;const auto binding=source.value("binding").toString("none");
  if(source.contains("binding"))result["binding"]=source.value("binding");
  const auto keys=requiredBindingKeys(binding);
  for(const auto &key:keys)result[key]=source.value(key);
  if(!keys.isEmpty()&&source.contains("valueFormat"))result["valueFormat"]=source.value("valueFormat");
  return result;
}
QJsonObject applyBinding(QJsonObject target,const QJsonObject &payload){
  for(const auto &key:connectionKeys)target.remove(key);
  for(auto it=payload.begin();it!=payload.end();++it)target[it.key()]=it.value();
  return target;
}
QString bindingSummary(const QJsonObject &element){
  const auto binding=element.value("binding").toString("none");QStringList parts{bindingLabels.value(binding,binding)};
  for(const auto &key:requiredBindingKeys(binding))parts<<(key=="dataBinding"?element.value(key).toObject().value("kind").toString():element.value(key).toString());
  if(!requiredBindingKeys(binding).isEmpty())parts<<QObject::tr("형식: ")+element.value("valueFormat").toString("{value}");
  return parts.join(" · ");
}
}

void StudioWindow::applyTextBindingBatch(){
  if(bridge.busy())return;
  const auto source=currentElement();
  if(source.value("kind")!="text"){errorLabel->setText(tr("연결을 복사할 텍스트 요소를 먼저 선택하세요."));return;}
  const auto binding=source.value("binding").toString("none");
  if(!bindingLabels.contains(binding)){errorLabel->setText(tr("지원하지 않는 텍스트 연결입니다: ")+binding);return;}
  for(const auto &key:requiredBindingKeys(binding))if(key=="dataBinding"?source.value(key).toObject().isEmpty():source.value(key).toString().isEmpty()){errorLabel->setText(tr("선택한 텍스트의 연결 대상을 먼저 지정하세요."));return;}
  const auto original=projectData();const auto revision=view.value("revision"),root=view.value("root");
  const auto visual=original.value("visual").toObject();const auto screens=visual.value("screens").toArray();
  const auto sourceScreen=screenId,sourceId=source.value("id").toString();const auto payload=bindingPayload(source);
  struct Target{int screen;int element;QJsonObject before;QJsonObject after;};QList<Target> targets;
  for(int s=0;s<screens.size();++s){const auto screen=screens[s].toObject();if(screen.value("id")==sourceScreen)continue;
    const auto elements=screen.value("elements").toArray();for(int e=0;e<elements.size();++e){const auto element=elements[e].toObject();if(element.value("kind")=="text"&&element.value("id")==sourceId)targets.append({s,e,element,applyBinding(element,payload)});}
  }
  QDialog dialog(this);dialog.setObjectName("textBindingBatchDialog");dialog.setWindowTitle(tr("텍스트 연결 일괄 적용"));dialog.resize(1040,620);
  auto layout=new QVBoxLayout(&dialog);auto help=new QLabel(tr("기준: %1 / %2\n적용할 연결: %3\n다른 화면의 같은 ID 텍스트를 선택하세요. 문구·위치·스타일은 유지하고 연결 속성만 바꿉니다.").arg(currentScreen().value("label").toString(sourceScreen),sourceId,bindingSummary(source)));help->setWordWrap(true);layout->addWidget(help);
  auto table=new QTableWidget(0,5);table->setObjectName("textBindingTargets");table->setHorizontalHeaderLabels({tr("선택"),tr("화면"),tr("텍스트 요소"),tr("현재 연결"),tr("적용 후 연결")});table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);table->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch);table->horizontalHeader()->setSectionResizeMode(4,QHeaderView::Stretch);layout->addWidget(table,1);
  for(const auto &target:targets){const auto screen=screens[target.screen].toObject();const int row=table->rowCount();table->insertRow(row);auto check=new QTableWidgetItem;check->setFlags(Qt::ItemIsEnabled|Qt::ItemIsUserCheckable);check->setCheckState(Qt::Unchecked);table->setItem(row,0,check);
    table->setItem(row,1,new QTableWidgetItem(screen.value("label").toString()+" ("+screen.value("id").toString()+")"));table->setItem(row,2,new QTableWidgetItem(target.before.value("label").toString(sourceId)+" ("+sourceId+")"));table->setItem(row,3,new QTableWidgetItem(bindingSummary(target.before)));table->setItem(row,4,new QTableWidgetItem(bindingSummary(target.after)));table->item(row,1)->setData(Qt::UserRole,screen.value("id").toString());
  }
  auto selectionButtons=new QHBoxLayout;auto all=new QPushButton(tr("모두 선택"));all->setObjectName("textBindingSelectAll");auto clear=new QPushButton(tr("선택 해제"));clear->setObjectName("textBindingSelectNone");selectionButtons->addWidget(all);selectionButtons->addWidget(clear);selectionButtons->addStretch();layout->addLayout(selectionButtons);
  auto preview=new QPlainTextEdit;preview->setObjectName("textBindingPreview");preview->setReadOnly(true);preview->setMaximumHeight(170);layout->addWidget(preview);
  auto error=new QLabel;error->setObjectName("textBindingBatchError");error->setWordWrap(true);error->setStyleSheet("color:#f3a18a");layout->addWidget(error);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel);buttons->setObjectName("textBindingBatchButtons");layout->addWidget(buttons);
  const auto refreshPreview=[&]{QStringList lines;int changes=0;for(int i=0;i<targets.size();++i)if(table->item(i,0)->checkState()==Qt::Checked){const auto &target=targets[i];if(target.before!=target.after)++changes;lines<<table->item(i,1)->text()+" / "+sourceId+"\n"+bindingSummary(target.before)+" → "+bindingSummary(target.after)+"\n"+tr("기존 문구 유지: ")+target.before.value("text").toString();}preview->setPlainText(targets.isEmpty()?tr("다른 화면에 같은 ID의 텍스트가 없습니다."):tr("변경 예정: %1개").arg(changes)+"\n\n"+lines.join("\n\n"));buttons->button(QDialogButtonBox::Apply)->setEnabled(changes>0);};
  connect(table,&QTableWidget::itemChanged,&dialog,[&]{refreshPreview();});
  connect(all,&QPushButton::clicked,&dialog,[&]{for(int i=0;i<table->rowCount();++i)table->item(i,0)->setCheckState(Qt::Checked);});
  connect(clear,&QPushButton::clicked,&dialog,[&]{for(int i=0;i<table->rowCount();++i)table->item(i,0)->setCheckState(Qt::Unchecked);});
  connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,[&]{
    if(bridge.busy()){error->setText(tr("현재 작업이 끝난 뒤 다시 적용하세요."));return;}
    if(view.value("revision")!=revision||view.value("root")!=root){error->setText(tr("프로젝트가 변경되었습니다. 창을 닫고 대상을 다시 확인하세요."));return;}
    dialog.accept();
  });refreshPreview();
  if(dialog.exec()!=QDialog::Accepted)return;
  if(bridge.busy()||view.value("revision")!=revision||view.value("root")!=root){errorLabel->setText(tr("프로젝트가 변경되어 연결을 적용하지 않았습니다."));return;}
  auto updatedScreens=screens;for(int i=0;i<targets.size();++i)if(table->item(i,0)->checkState()==Qt::Checked){const auto &target=targets[i];auto screen=updatedScreens[target.screen].toObject();auto elements=screen.value("elements").toArray();elements[target.element]=target.after;screen["elements"]=elements;updatedScreens[target.screen]=screen;}
  auto project=original,updatedVisual=visual;updatedVisual["screens"]=updatedScreens;project["visual"]=updatedVisual;
  if(project!=original)replaceProject(project);
}
