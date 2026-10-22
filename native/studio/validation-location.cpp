#include "validation-location.hpp"
#include <QJsonArray>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QPointer>
#include <QTimer>
#include <QMap>
#include <cmath>

namespace ValidationLocation {
static QString tr(const char *s) { return QObject::tr(s); }
static QString escaped(QString s) { return s.replace("~", "~0").replace("/", "~1"); }
static QString fieldName(const QString &key) {
  static const QMap<QString,QString> names{{"width",tr("너비")},{"height",tr("높이")},{"window",tr("사건 기간")},{"fromPeriod",tr("시작 일정")},{"throughPeriod",tr("마지막 일정")},{"nodes",tr("대본 노드")},{"next",tr("다음 대본")},{"entryNodeId",tr("시작 대본")},{"costs",tr("비용")},{"baseEffects",tr("기본 효과")},{"effects",tr("효과")},{"requirements",tr("조건")},{"attributeId",tr("능력치·자원")},{"amount",tr("수량")},{"characterId",tr("인물")},{"speakerId",tr("화자")},{"eventId",tr("사건")},{"activityId",tr("활동")},{"actionId",tr("행동")},{"screenId",tr("화면")},{"locationId",tr("장소")},{"text",tr("대사·표시문")},{"options",tr("선택지")},{"label",tr("이름")}};
  return names.contains(key)?names[key]+" · "+key:key;
}
bool tokens(const QString &pointer, QStringList &parts) {
  parts.clear(); if(pointer.isEmpty()) return true;
  if(!pointer.startsWith('/')) return false;
  for(auto s:pointer.mid(1).split('/')) {
    for(int i=0;i<s.size();++i) if(s[i]=='~' && (++i>=s.size() || (s[i]!='0' && s[i]!='1'))) return false;
    parts.append(s.replace("~1","/").replace("~0","~"));
  }
  return true;
}
static bool index(const QString &s, int size, int &i) {
  bool ok=false; i=s.toInt(&ok); return ok && s==QString::number(i) && i>=0 && i<size;
}
QJsonValue value(const QJsonValue &root, const QString &pointer) {
  QStringList path;if(!tokens(pointer,path))return QJsonValue::Undefined;
  auto v=root;
  for(const auto &p:path){if(v.isObject())v=v.toObject().value(p);else if(v.isArray()){int i;if(!index(p,v.toArray().size(),i))return QJsonValue::Undefined;v=v.toArray()[i];}else return QJsonValue::Undefined;}
  return v;
}
static bool changed(QJsonValue &v,const QStringList &path,int at,const QJsonValue &next,bool remove) {
  if(at==path.size()){if(remove)return false;v=next;return true;}
  const auto key=path[at];const bool leaf=at+1==path.size();
  if(v.isObject()) {auto o=v.toObject();if(leaf&&remove){if(!o.contains(key))return false;o.remove(key);}else {auto child=o.value(key);if(!leaf&&child.isUndefined())return false;if(!changed(child,path,at+1,next,remove))return false;o[key]=child;}v=o;return true;}
  if(v.isArray()) {auto a=v.toArray();int i;if(!index(key,a.size(),i))return false;if(leaf&&remove)a.removeAt(i);else{QJsonValue child=a.at(i);if(!changed(child,path,at+1,next,remove))return false;a[i]=child;}v=a;return true;}
  return false;
}
bool replace(QJsonValue &root,const QString &pointer,const QJsonValue &next,bool remove) {
  QStringList path;if(!tokens(pointer,path))return false;auto candidate=root;if(!changed(candidate,path,0,next,remove))return false;root=candidate;return true;
}
static QString caption(QJsonValue v) {
  if(v.isObject()){auto o=v.toObject();return o["name"].toString(o["label"].toString(o["id"].toString()));}
  if(v.isArray())return tr("목록 · %1개").arg(v.toArray().size());
  if(v.isUndefined())return tr("누락된 필드");
  if(v.isNull())return "null";
  if(v.isBool())return v.toBool()?"true":"false";
  if(v.isDouble())return QString::number(v.toDouble(),'g',17);
  return v.toString();
}

QDialog *editor(QWidget *parent,QJsonObject document,QJsonArray documents,QJsonObject diagnostic,Apply apply) {
  auto dialog=new QDialog(parent);dialog->setObjectName("validationLocationEditor");dialog->setWindowTitle(tr("오류 위치 편집"));dialog->resize(820,680);dialog->setAttribute(Qt::WA_DeleteOnClose);
  const auto data=document["data"].toObject();const auto target=diagnostic["pointer"].toString();
  auto layout=new QVBoxLayout(dialog);
  auto reason=new QLabel(diagnostic["code"].toString()+" · "+diagnostic["message"].toString());reason->setTextFormat(Qt::PlainText);reason->setWordWrap(true);reason->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(reason);
  auto address=new QLabel(document["file"].toString()+"  "+target);address->setTextFormat(Qt::PlainText);address->setWordWrap(true);address->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(address);
  auto tree=new QTreeWidget;tree->setObjectName("validationLocationTree");tree->setHeaderLabels({tr("필드 · 연결 대상"),tr("현재 값")});tree->setColumnWidth(0,320);layout->addWidget(tree,1);
  auto add=[tree](QTreeWidgetItem *parent,const QString &path,const QString &key,QJsonValue v){auto item=parent?new QTreeWidgetItem(parent):new QTreeWidgetItem(tree);item->setText(0,fieldName(key));item->setText(1,caption(v).left(180));item->setToolTip(1,caption(v));item->setData(0,Qt::UserRole,path);if(v.isObject()||v.isArray())item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);return item;};
  auto populate=[data,add](QTreeWidgetItem *item){if(item->data(0,Qt::UserRole+1).toBool())return;item->setData(0,Qt::UserRole+1,true);const auto path=item->data(0,Qt::UserRole).toString();auto v=value(data,path);if(v.isObject()){auto o=v.toObject();for(auto it=o.begin();it!=o.end();++it)add(item,path+"/"+escaped(it.key()),it.key(),it.value());}else if(v.isArray()){auto a=v.toArray();for(int i=0;i<a.size();++i)add(item,path+"/"+QString::number(i),QString::number(i)+" · "+caption(a[i]),a[i]);}};
  QObject::connect(tree,&QTreeWidget::itemExpanded,dialog,populate);
  auto root=add(nullptr,"",caption(data),data);auto selected=root;QStringList parts;tokens(target,parts);QString path;
  for(const auto &part:parts){populate(selected);selected->setExpanded(true);path+="/"+escaped(part);QTreeWidgetItem *child=nullptr;for(int i=0;i<selected->childCount();++i)if(selected->child(i)->data(0,Qt::UserRole)==path)child=selected->child(i);if(!child)child=add(selected,path,part,value(data,path));selected=child;}
  auto field=new QLabel;field->setObjectName("validationFieldPath");field->setTextFormat(Qt::PlainText);field->setWordWrap(true);layout->addWidget(field);
  auto form=new QFormLayout;layout->addLayout(form);
  auto type=new QComboBox;type->setObjectName("validationValueType");for(auto p:{qMakePair(tr("문자"),"string"),qMakePair(tr("숫자"),"number"),qMakePair(tr("참 / 거짓"),"boolean"),qMakePair(tr("빈 값"),"null")})type->addItem(p.first,p.second);form->addRow(tr("값 종류"),type);
  auto text=new QPlainTextEdit;text->setObjectName("validationTextValue");text->setMaximumHeight(110);form->addRow(tr("문자"),text);
  auto number=new QLineEdit;number->setObjectName("validationNumberValue");form->addRow(tr("숫자"),number);
  auto boolean=new QCheckBox(tr("참"));boolean->setObjectName("validationBooleanValue");form->addRow(boolean);
  auto references=new QComboBox;references->setObjectName("validationReference");form->addRow(tr("연결 대상"),references);
  auto status=new QLabel(tr("수정 적용은 실행 취소할 수 있습니다. 프로젝트 저장은 별도로 진행하세요."));status->setObjectName("validationRepairStatus");status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Close);buttons->button(QDialogButtonBox::Close)->setText(tr("닫기"));auto submit=buttons->addButton(tr("적용 후 재검증"),QDialogButtonBox::ActionRole);submit->setObjectName("validationApplyRecheck");auto remove=buttons->addButton(tr("필드 삭제 후 재검증"),QDialogButtonBox::ActionRole);remove->setObjectName("validationRemoveRecheck");layout->addWidget(buttons);QObject::connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
  auto update=[=]{const auto kind=type->isEnabled()?type->currentData().toString():QString();auto show=[=](QWidget *w,bool visible){w->setVisible(visible);if(auto label=form->labelForField(w))label->setVisible(visible);};show(text,kind=="string");show(number,kind=="number");show(boolean,kind=="boolean");show(references,kind=="string"&&references->count()>1);};
  QObject::connect(type,qOverload<int>(&QComboBox::currentIndexChanged),dialog,[=]{update();});
  QObject::connect(references,qOverload<int>(&QComboBox::activated),dialog,[=](int i){if(i>0)text->setPlainText(references->itemData(i).toString());});
  QObject::connect(tree,&QTreeWidget::currentItemChanged,dialog,[=](QTreeWidgetItem *item){if(!item)return;const auto path=item->data(0,Qt::UserRole).toString();field->setText(path);auto v=value(data,path);const bool container=v.isObject()||v.isArray();submit->setEnabled(!container&&!path.isEmpty());remove->setEnabled(!path.isEmpty()&&!v.isUndefined());type->setEnabled(!container);type->setCurrentIndex(v.isDouble()?1:v.isBool()?2:v.isNull()?3:0);text->setPlainText(v.toString());number->setText(v.isDouble()?QString::number(v.toDouble(),'g',17):"");boolean->setChecked(v.toBool());references->clear();references->addItem(tr("대상 선택"));
    const auto key=path.section('/',-1);QString collection,docKind;
    if(key=="activityId")docKind="activity";else if(key=="characterId"||key=="speakerId")docKind="character";else if(key=="eventId")docKind="event";else if(key=="endingId")docKind="ending";
    else if(key=="screenId")collection="visual/screens";else if(key=="componentId")collection="visual/components";else if(key=="actionId")collection="gameplay/actions";else if(key=="locationId")collection="gameplay/locations";else if(key=="attributeId")collection="attributes";else if(key=="resourceId")collection="resources";else if(key=="assetId"||key=="imageId")collection="assets";
    auto offer=[=](QJsonObject o){const auto id=o["id"].toString();if(!id.isEmpty())references->addItem(caption(o)+" · "+id,id);};
    if(key=="next"||key=="entryNodeId"||key=="nodeId")for(auto n:data["nodes"].toArray())offer(n.toObject());
    for(auto d:documents){auto doc=d.toObject();if(!docKind.isEmpty()&&doc["kind"]==docKind)offer(doc["data"].toObject());if(!collection.isEmpty())for(auto row:value(doc["data"],"/"+collection).toArray())offer(row.toObject());}
    update();if(container){populate(item);item->setExpanded(true);}else QTimer::singleShot(0,dialog,[=]{if(v.isDouble())number->setFocus();else if(v.isBool())boolean->setFocus();else text->setFocus();});
  });
  auto commit=[=](bool removing){auto item=tree->currentItem();if(!item)return;const auto path=item->data(0,Qt::UserRole).toString();QJsonValue next;const auto kind=type->currentData().toString();if(kind=="string")next=text->toPlainText();else if(kind=="number"){bool ok=false;double n=number->text().toDouble(&ok);if(!ok||!std::isfinite(n)){status->setText(tr("유효한 숫자를 입력하세요."));return;}next=n;}else if(kind=="boolean")next=boolean->isChecked();QJsonValue result=data;if(!replace(result,path,next,removing)||!result.isObject()){status->setText(tr("편집 위치를 찾을 수 없습니다. 다시 검증하세요."));return;}tree->setEnabled(false);submit->setEnabled(false);remove->setEnabled(false);QPointer<QDialog> guard(dialog);apply(result.toObject(),[guard,status](QString message){if(!guard)return;status->setText(message);});};
  // Each repair is one immutable transaction. Close and select the refreshed diagnostic
  // before another edit; array indices and duplicate IDs can change after repair.
  QObject::connect(submit,&QPushButton::clicked,dialog,[=]{commit(false);});
  QObject::connect(remove,&QPushButton::clicked,dialog,[=]{commit(true);});
  tree->setCurrentItem(selected);tree->scrollToItem(selected);return dialog;
}
}
