#include "selection-edit.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

bool editSelectionDialog(QWidget *parent,const QJsonObject &visual,const QJsonArray &elements,const QStringList &ids,QSize size,QJsonArray &output){
  QDialog dialog(parent);dialog.setObjectName("selectionEditDialog");dialog.setWindowTitle(QObject::tr("선택 요소 일괄 편집 · 정렬"));dialog.resize(520,580);
  auto layout=new QVBoxLayout(&dialog);auto note=new QLabel(QObject::tr("%1개 선택 · 체크한 속성만 변경합니다. 서로 다른 값과 연결은 그대로 유지합니다. 적용 전체를 한 번에 되돌릴 수 있습니다.").arg(ids.size()));note->setWordWrap(true);layout->addWidget(note);
  auto form=new QFormLayout;layout->addLayout(form);auto operation=new QComboBox;operation->setObjectName("batchOperation");
  for(auto p:QList<QPair<QString,QString>>{{QObject::tr("속성만 변경"),"properties"},{QObject::tr("함께 이동"),"move"},{QObject::tr("왼쪽 정렬"),"x:start"},{QObject::tr("가로 중앙 정렬"),"x:center"},{QObject::tr("오른쪽 정렬"),"x:end"},{QObject::tr("위쪽 정렬"),"y:start"},{QObject::tr("세로 중앙 정렬"),"y:center"},{QObject::tr("아래쪽 정렬"),"y:end"},{QObject::tr("가로 간격 맞춤"),"x:distribute"},{QObject::tr("세로 간격 맞춤"),"y:distribute"}})operation->addItem(p.first,p.second);
  form->addRow(QObject::tr("배치 작업"),operation);auto dx=new QSpinBox,dy=new QSpinBox;dx->setObjectName("batchDx");dy->setObjectName("batchDy");dx->setRange(-3840,3840);dy->setRange(-2160,2160);form->addRow(QObject::tr("가로 이동량"),dx);form->addRow(QObject::tr("세로 이동량"),dy);
  auto moving=[=]{dx->setEnabled(operation->currentData()=="move");dy->setEnabled(operation->currentData()=="move");};QObject::connect(operation,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,moving);moving();
  QMap<QString,QCheckBox*> checks;QMap<QString,std::function<QJsonValue()>> values;
  auto common=[&](QString key,QJsonValue fallback){QJsonValue value;bool first=true;for(auto v:elements){auto e=v.toObject();if(!ids.contains(e["id"].toString()))continue;auto x=e.contains(key)?e[key]:fallback;if(!first&&x!=value)return QJsonValue();value=x;first=false;}return value;};
  auto row=[&](QString key,QString label,QWidget *field,std::function<QJsonValue()> read,QJsonValue fallback){auto use=new QCheckBox(label);use->setObjectName("batchUse-"+key);field->setObjectName("batchValue-"+key);field->setEnabled(false);QObject::connect(use,&QCheckBox::toggled,field,&QWidget::setEnabled);form->addRow(use,field);checks[key]=use;values[key]=read;if(common(key,fallback).isNull())field->setToolTip(QObject::tr("여러 값 · 체크하지 않으면 각 값을 유지합니다."));};
  for(auto key:QStringList{"width","height","fontSize"}){auto f=new QSpinBox;f->setRange(key=="fontSize"?10:16,key=="width"?3840:key=="height"?2160:120);auto v=common(key,key=="fontSize"?20:100);f->setValue(v.isDouble()?v.toInt():f->minimum());row(key,key=="width"?QObject::tr("너비"):key=="height"?QObject::tr("높이"):QObject::tr("글자 크기"),f,[f]{return f->value();},key=="fontSize"?20:100);}
  auto opacity=new QDoubleSpinBox;opacity->setRange(0,1);opacity->setSingleStep(.05);opacity->setValue(common("opacity",1).toDouble(1));row("opacity",QObject::tr("불투명도"),opacity,[opacity]{return opacity->value();},1);
  for(auto key:QStringList{"hidden","locked"}){auto f=new QCheckBox(QObject::tr("사용"));f->setChecked(common(key,false).toBool());row(key,key=="hidden"?QObject::tr("숨김"):QObject::tr("배치 잠금"),f,[f]{return f->isChecked();},false);}
  auto style=new QComboBox;style->addItem(QObject::tr("프로젝트 기본"),QString());for(auto v:visual["styles"].toArray()){auto s=v.toObject();style->addItem(s["label"].toString(s["id"].toString()),s["id"].toString());}style->setCurrentIndex(qMax(0,style->findData(common("styleId",QString()).toString())));row("styleId",QObject::tr("공통 스타일"),style,[style]{return style->currentData().toString().isEmpty()?QJsonValue(QJsonValue::Null):QJsonValue(style->currentData().toString());},QString());
  auto error=new QLabel;error->setObjectName("batchError");error->setWordWrap(true);layout->addWidget(error);layout->addStretch();auto buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel);layout->addWidget(buttons);
  QObject::connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,[&]{QJsonObject patch;for(auto it=checks.begin();it!=checks.end();++it)if(it.value()->isChecked())patch[it.key()]=values[it.key()]();auto mode=operation->currentData().toString();QJsonObject op{{"mode",mode},{"patch",patch},{"dx",dx->value()},{"dy",dy->value()}};if(mode.contains(':')){auto p=mode.split(':');op["mode"]=p[1]=="distribute"?"distribute":"align";op["axis"]=p[0];op["edge"]=p[1];}QString problem;if(!editSelection(elements,ids,size,op,output,problem)){error->setText(problem);return;}dialog.accept();});
  QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);return dialog.exec()==QDialog::Accepted;
}
