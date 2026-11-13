#pragma once
#include "components.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSignalBlocker>

namespace StudioResumeContext {
inline QString issue(const QJsonObject& element) {
  if(element.isEmpty())return QObject::tr("찾을 수 없는 텍스트");
  if(element.value("kind")!="text")return QObject::tr("텍스트 요소만 선택할 수 있습니다.");
  if(!element.value("selectionBindings").toArray().isEmpty()||element.value("binding")=="selected-field"||element.contains("dataBinding"))return QObject::tr("선택한 행 또는 데이터 연결은 사용할 수 없습니다.");
  if(element.value("binding")=="relationship-value"&&element.value("valueFormat").toString().contains("{delta}"))return QObject::tr("현재 상태를 나타내는 연결 또는 고정 텍스트를 선택하세요.");
  const QSet<QString> allowed{"none","calendar","calendar-year","calendar-month","calendar-day","calendar-weekday","location","user-field","attribute-value","relationship-value","item-count"};
  if(!allowed.contains(element.value("binding").toString("none")))return QObject::tr("현재 상태를 나타내는 연결 또는 고정 텍스트를 선택하세요.");
  return {};
}
inline QMap<QString,QJsonObject> leaves(const QJsonObject& visual,QString screenId) {
  QMap<QString,QJsonObject> result;int budget=10000;
  // This chooser needs stable resolved IDs, including nested instances. The
  // canvas preview expands one component level and also creates collection rows.
  // Reuse hierarchy/override semantics here without preview rows or clipping.
  std::function<void(QJsonArray,QSize,QSize,QString,QJsonObject,QSet<QString>)> expand;
  expand=[&](QJsonArray elements,QSize reference,QSize actual,QString prefix,QJsonObject inherited,QSet<QString> stack){
    for(auto value:hierarchyCanvasElements(elements,reference,actual)){
      if(--budget<0)return;auto leaf=value.toObject();leaf["id"]=prefix+leaf.value("id").toString();
      if(!inherited.isEmpty())inheritCanvasElement(leaf,inherited);
      if(leaf.value("kind")=="collection")continue;
      if(leaf.value("kind")!="component"){result[leaf.value("id").toString()]=leaf;continue;}
      const auto componentId=leaf.value("componentId").toString();if(stack.contains(componentId)||stack.size()>=8)continue;
      for(auto cv:visual.value("components").toArray()){
        const auto component=cv.toObject();if(component.value("id")!=componentId)continue;
        QJsonArray children;for(auto childValue:component.value("elements").toArray()){
          auto child=childValue.toObject();for(auto ov:leaf.value("overrides").toArray()){const auto override=ov.toObject();if(override.value("elementId")!=child.value("id"))continue;const auto values=override.value("values").toObject();for(auto it=values.begin();it!=values.end();++it)child[it.key()]=it.value();break;}children.append(child);
        }
        auto next=stack;next.insert(componentId);expand(children,QSize(component.value("width").toInt(),component.value("height").toInt()),QSize(leaf.value("width").toInt(),leaf.value("height").toInt()),leaf.value("id").toString()+"__",leaf,next);break;
      }
    }
  };
  const QSize size(visual.value("width").toInt(1280),visual.value("height").toInt(720));
  for(auto value:visual.value("screens").toArray())if(value.toObject().value("id")==screenId){expand(value.toObject().value("elements").toArray(),size,size,{}, {},{});break;}
  return result;
}
inline bool hasScreen(const QJsonObject& visual,QString id){for(auto value:visual.value("screens").toArray())if(value.toObject().value("id")==id)return true;return false;}
inline QString validate(const QJsonObject& visual,const QJsonObject& config) {
  if(!hasScreen(visual,config.value("screenId").toString()))return QObject::tr("현재 알림의 원본 화면을 선택하세요.");
  const auto ids=config.value("elementIds").toArray();if(ids.isEmpty()||ids.size()>64)return QObject::tr("텍스트를 1~64개 선택하세요.");
  const auto available=leaves(visual,config.value("screenId").toString());QSet<QString> seen;
  for(auto value:ids){const auto id=value.toString();if(seen.contains(id))return QObject::tr("같은 텍스트를 두 번 선택할 수 없습니다: %1").arg(id);seen.insert(id);const auto error=issue(available.value(id));if(!error.isEmpty())return id+": "+error;}
  return {};
}
inline bool hasAction(const QJsonValue& value){if(value.isObject()){auto object=value.toObject();if((object.value("kind")=="button"&&object.value("action")=="resumeContext")||(object.contains("elementId")&&object.value("values").toObject().value("action")=="resumeContext"))return true;for(auto it=object.begin();it!=object.end();++it)if(hasAction(it.value()))return true;}else if(value.isArray())for(auto item:value.toArray())if(hasAction(item))return true;return false;}

class Dialog final:public QDialog {
  QJsonObject visual,original;
  QComboBox* screen;
  QListWidget *available,*selected;
  QCheckBox* afterLoad;
  QLabel* error;
  QMap<QString,QJsonObject> resolved;
  QString id(QListWidgetItem* row)const{return row?row->data(Qt::UserRole).toString():QString();}
  QString label(QString key)const {
    const auto e=resolved.value(key);const auto problem=issue(e);
    if(!problem.isEmpty())return key+" · "+problem;
    auto text=e.value("text").toString().simplified();if(text.size()>80)text=text.left(77)+"…";
    return key+(text.isEmpty()?QString():" · "+text);
  }
  void append(QString key){auto row=new QListWidgetItem(label(key),selected);row->setData(Qt::UserRole,key);row->setToolTip(key+"\n"+issue(resolved.value(key)));}
  void refresh(){
    resolved=leaves(visual,screen->currentData().toString());
    QSet<QString> used;for(int i=0;i<selected->count();++i){auto row=selected->item(i);used.insert(id(row));row->setText(label(id(row)));row->setToolTip(id(row)+"\n"+issue(resolved.value(id(row))));}
    available->clear();for(auto it=resolved.begin();it!=resolved.end();++it)if(!used.contains(it.key())&&issue(it.value()).isEmpty()){auto row=new QListWidgetItem(label(it.key()),available);row->setData(Qt::UserRole,it.key());row->setToolTip(it.key());}
    error->setText(validate(visual,value()));
  }
  QJsonObject value()const{auto config=original;config["screenId"]=screen->currentData().toString();QJsonArray ids;for(int i=0;i<selected->count();++i)ids.append(id(selected->item(i)));config["elementIds"]=ids;if(afterLoad->isChecked())config["showAfterLoad"]=true;else config.remove("showAfterLoad");return config;}
public:
  QJsonObject config;
  Dialog(QJsonObject design,QJsonObject current,QWidget* parent=nullptr):QDialog(parent),visual(design),original(current){
    setObjectName("resumeContextDialog");setWindowTitle(tr("현재 알림 구성"));resize(720,600);
    auto layout=new QVBoxLayout(this);auto help=new QLabel(tr("기존 화면의 텍스트를 골라 현재 알림으로 표시합니다. 선택한 순서, 조건과 번역을 유지하며 원본 버튼은 가져오지 않습니다. 대사·활동 결과·선택한 행의 정보는 사용할 수 없습니다."));help->setWordWrap(true);layout->addWidget(help);
    auto form=new QFormLayout;layout->addLayout(form);screen=new QComboBox;screen->setObjectName("resumeContextScreen");screen->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);screen->setMinimumContentsLength(14);screen->addItem(tr("원본 화면 선택…"),QString());for(auto value:visual.value("screens").toArray()){auto s=value.toObject();screen->addItem(s.value("label").toString(s.value("id").toString())+" · "+s.value("id").toString(),s.value("id").toString());}
    const auto currentId=current.value("screenId").toString();if(!currentId.isEmpty()&&screen->findData(currentId)<0)screen->addItem(tr("찾을 수 없음: %1").arg(currentId),currentId);screen->setCurrentIndex(qMax(0,screen->findData(currentId)));form->addRow(tr("원본 화면"),screen);
    auto lists=new QHBoxLayout;layout->addLayout(lists,1);auto makeList=[&](QString title,QString name){auto group=new QGroupBox(title);auto column=new QVBoxLayout(group);auto list=new QListWidget;list->setObjectName(name);list->setWordWrap(true);list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);list->setMinimumSize(140,100);column->addWidget(list);lists->addWidget(group,1);return list;};available=makeList(tr("사용 가능한 텍스트"),"resumeContextAvailable");selected=makeList(tr("알림 순서"),"resumeContextSelected");
    auto commands=new QHBoxLayout;layout->addLayout(commands);auto button=[&](QString title,QString name){auto b=new QPushButton(title);b->setObjectName(name);commands->addWidget(b);return b;};auto add=button(tr("추가"),"resumeContextAdd"),remove=button(tr("제거"),"resumeContextRemove"),up=button(tr("위로 이동"),"resumeContextUp"),down=button(tr("아래로 이동"),"resumeContextDown");
    afterLoad=new QCheckBox(tr("저장을 불러온 뒤 자동으로 표시"));afterLoad->setObjectName("resumeContextAfterLoad");afterLoad->setChecked(current.value("showAfterLoad").toBool());layout->addWidget(afterLoad);
    auto boundary=new QLabel(tr("현재 상태를 보여주는 디자이너의 알림입니다. 읽기 기록이나 자동 줄거리 요약이 아니며 저장 파일과 게임 진행을 바꾸지 않습니다."));boundary->setWordWrap(true);layout->addWidget(boundary);
    error=new QLabel;error->setObjectName("resumeContextError");error->setWordWrap(true);layout->addWidget(error);
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons->setObjectName("resumeContextButtons");buttons->button(QDialogButtonBox::Save)->setText(tr("선택 적용"));layout->addWidget(buttons);
    for(auto value:current.value("elementIds").toArray())append(value.toString());refresh();
    connect(screen,qOverload<int>(&QComboBox::currentIndexChanged),this,[&]{refresh();});
    connect(add,&QPushButton::clicked,this,[&]{const auto key=id(available->currentItem());if(key.isEmpty())return;if(selected->count()>=64){error->setText(tr("텍스트를 1~64개 선택하세요."));return;}append(key);selected->setCurrentRow(selected->count()-1);refresh();});
    connect(remove,&QPushButton::clicked,this,[&]{delete selected->takeItem(selected->currentRow());refresh();});
    auto move=[&](int offset){const int at=selected->currentRow();if(at<0||at+offset<0||at+offset>=selected->count())return;auto row=selected->takeItem(at);selected->insertItem(at+offset,row);selected->setCurrentRow(at+offset);};
    connect(up,&QPushButton::clicked,this,[=]{move(-1);});connect(down,&QPushButton::clicked,this,[=]{move(1);});
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);connect(buttons,&QDialogButtonBox::accepted,this,[&]{const auto next=value();const auto problem=validate(visual,next);if(!problem.isEmpty()){error->setText(problem);return;}config=next;accept();});
  }
};
}
