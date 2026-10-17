#pragma once
#include "logic-editor.hpp"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

inline bool editComponentContext(QWidget* parent,QJsonObject original,const QJsonObject& view,QJsonObject& result) {
  QDialog dialog(parent);dialog.setObjectName("componentContextDialog");dialog.setWindowTitle(QObject::tr("부품 대상 · 표시와 조건 연결"));dialog.resize(640,760);
  auto layout=new QVBoxLayout(&dialog);auto help=new QLabel(QObject::tr("대상은 부모 부품에서 이어받습니다. 여기서 지정하면 이 요소와 자식에 적용됩니다.\n‘대상 사용’을 켠 연결만 바뀝니다. 특정 인물로 지정한 데이터 소유자는 유지됩니다."));help->setWordWrap(true);layout->addWidget(help);
  QJsonObject project;for(auto v:view.value("documents").toArray())if(v.toObject().value("kind")=="project")project=v.toObject().value("data").toObject();
  const QList<QPair<QString,QString>> fields={{"actorId","실행 인물"},{"activityId","활동"},{"characterId","관계 대상 인물"},{"itemId","물품"},{"actionId","공통 행동"},{"locationId","장소"},{"shopId","상점"},{"attributeId","능력·자원"}};
  QMap<QString,QComboBox*> choices;QMap<QString,QCheckBox*> uses;auto form=new QFormLayout;layout->addLayout(form);
  for(const auto& pair:fields){auto line=new QWidget;auto row=new QHBoxLayout(line);row->setContentsMargins(0,0,0,0);auto c=new QComboBox;c->setObjectName("context-"+pair.first);c->addItem(QObject::tr("부모 대상 사용 / 지정 없음"),"");choices[pair.first]=c;row->addWidget(c,1);
    auto add=[&](QJsonObject value){c->addItem(value.value("name").toString(value.value("label").toString(value.value("id").toString())),value.value("id").toString());};
    for(auto value:view.value("documents").toArray()){const auto d=value.toObject(),data=d.value("data").toObject();const auto kind=d.value("kind").toString();
      if(kind=="character"&&(pair.first=="characterId"||pair.first=="actorId")){bool playable=pair.first=="characterId";for(auto m:project.value("roster").toObject().value("members").toArray())if(m.toObject().value("characterId")==data.value("id"))playable=true;if(playable)add(data);}
      if(kind=="activity"&&pair.first=="activityId")add(data);
      if(kind=="attributes"&&pair.first=="attributeId")for(auto a:data.value("attributes").toArray())add(a.toObject());
    }
    const QMap<QString,QString> keys={{"itemId","items"},{"actionId","actions"},{"locationId","locations"},{"shopId","shops"}};
    if(keys.contains(pair.first))for(auto v:project.value("gameplay").toObject().value(keys[pair.first]).toArray())add(v.toObject());
    const auto id=original.value("uiContext").toObject().value(pair.first).toString();if(c->findData(id)<0)c->addItem(QObject::tr("누락된 대상 · ")+id,id);c->setCurrentIndex(c->findData(id));
    if(original.value("kind")!="component"){auto use=new QCheckBox(QObject::tr("대상 사용"));use->setObjectName("contextUse-"+pair.first);use->setChecked(original.value("contextBindings").toArray().contains(pair.first));uses[pair.first]=use;row->addWidget(use);}form->addRow(pair.second,line);
  }
  auto display=new QComboBox;display->setObjectName("contextDisplay");display->addItem(QObject::tr("기존 표시 유지"),"");display->addItem(QObject::tr("직접 입력한 문구 사용"),"none");for(auto pair:fields)display->addItem(pair.second+QObject::tr(" 이름"),pair.first);display->addItem(QObject::tr("실행 인물의 수치 데이터"),"data-value");
  if(original.value("binding")=="context-name")display->setCurrentIndex(display->findData(original.value("contextField")));
  form->addRow(QObject::tr("표시"),display);
  auto numeric=new QComboBox;numeric->setObjectName("contextNumeric");numeric->addItem(QObject::tr("기존 데이터 유지"),QJsonObject());
  for(auto d:view.value("documents").toArray()){auto doc=d.toObject(),data=doc.value("data").toObject();if(doc.value("kind")=="attributes")for(auto a:data.value("attributes").toArray()){auto value=a.toObject();if(project.value("roster").toObject().value("localAttributeIds").toArray().contains(value.value("id")))numeric->addItem(value.value("label").toString(),QJsonObject{{"kind","attribute"},{"attributeId",value.value("id")},{"owner",QJsonObject{{"kind","actor"}}}});}
    if(doc.value("kind")=="character")for(auto r:data.value("relationships").toArray()){auto rel=r.toObject();numeric->addItem(data.value("name").toString()+" · "+rel.value("label").toString(),QJsonObject{{"kind","relationship"},{"characterId",data.value("id")},{"relationshipId",rel.value("id")},{"owner",QJsonObject{{"kind","actor"}}}});}}
  for(auto i:project.value("gameplay").toObject().value("items").toArray()){auto item=i.toObject();numeric->addItem(item.value("name").toString()+QObject::tr(" 수량"),QJsonObject{{"kind","inventory"},{"itemId",item.value("id")},{"owner",QJsonObject{{"kind","actor"}}}});}
  form->addRow(QObject::tr("수치 연결"),numeric);
  auto format=new QLineEdit(original.value("valueFormat").toString("{label}: {value}"));format->setObjectName("contextFormat");form->addRow(QObject::tr("표시 형식"),format);
  QMap<QString,QJsonValue> conditions;for(auto key:{"visibleCondition","enabledCondition"}){conditions[key]=original.value(key);auto button=new QPushButton(QString(key)=="visibleCondition"?QObject::tr("표시 조건…"):QObject::tr("버튼 사용 조건…"));button->setObjectName("context-"+QString(key));layout->addWidget(button);QObject::connect(button,&QPushButton::clicked,&dialog,[&,key]{LogicEditor edit(conditions[key].isUndefined()?QJsonValue(QJsonObject{{"op","always"}}):conditions[key],"condition",view,&dialog);if(edit.exec()==QDialog::Accepted)conditions[key]=edit.value();});}
  auto error=new QLabel;layout->addWidget(error);auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);
  QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
    auto out=original;QJsonObject context;for(auto it=choices.begin();it!=choices.end();++it)if(!it.value()->currentData().toString().isEmpty())context[it.key()]=it.value()->currentData().toString();if(context.empty())out.remove("uiContext");else out["uiContext"]=context;
    if(!uses.empty()){QJsonArray bindings;for(auto pair:fields)if(uses[pair.first]->isChecked())bindings.append(pair.first);if(bindings.empty())out.remove("contextBindings");else out["contextBindings"]=bindings;}
    const auto mode=display->currentData().toString();if(!mode.isEmpty()){if(out.value("kind")!="text"){error->setText(QObject::tr("이름·수치 표시는 문구 요소에서 설정하세요."));return;}out["binding"]=mode=="data-value"||mode=="none"?mode:"context-name";if(mode=="data-value"){auto data=numeric->currentData().toJsonObject();if(data.empty())data=original.value("dataBinding").toObject();if(data.empty()){error->setText(QObject::tr("표시할 수치를 선택하세요."));return;}out["dataBinding"]=data;out["valueFormat"]=format->text();out.remove("contextField");}else{if(mode=="none")out.remove("contextField");else out["contextField"]=mode;out.remove("dataBinding");out.remove("valueFormat");}}
    for(auto it=conditions.begin();it!=conditions.end();++it)if(!it.value().isUndefined())out[it.key()]=it.value();result=out;dialog.accept();
  });return dialog.exec()==QDialog::Accepted;
}
