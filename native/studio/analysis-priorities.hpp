#pragma once
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMap>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <functional>
#include <limits>

// Authoring-only preferences. This dialog never changes the game's attributes.
class AnalysisPriorityDialog final:public QDialog {
  QLineEdit* name_;QLabel* error_;QTableWidget *priorities_,*bands_;
  QJsonArray attributes_;
  QJsonObject attribute(const QString& id)const{for(const auto& value:attributes_)if(value.toObject().value("id").toString()==id)return value.toObject();return {};}
  static QSpinBox* number(QWidget* parent,const QString& id,int value){auto s=new QSpinBox(parent);s->setObjectName(id);s->setRange(std::numeric_limits<int>::min(),std::numeric_limits<int>::max());s->setValue(value);return s;}
  static QWidget* boundCell(const QString& id,const QJsonObject& row,const QString& key,int fallback){auto cell=new QWidget;auto layout=new QHBoxLayout(cell);layout->setContentsMargins(4,2,4,2);auto enabled=new QCheckBox(QObject::tr("사용"));enabled->setObjectName(id+"-enabled");enabled->setAccessibleName(key=="min"?QObject::tr("최솟값 사용"):QObject::tr("최댓값 사용"));enabled->setChecked(row.contains(key));auto value=number(cell,id,row.value(key).toInt(fallback));value->setEnabled(enabled->isChecked());layout->addWidget(enabled);layout->addWidget(value);QObject::connect(enabled,&QCheckBox::toggled,value,&QSpinBox::setEnabled);return cell;}
public:
  QJsonObject profile;QString profileName;
  AnalysisPriorityDialog(QJsonArray attributes,const QJsonObject& stored,const QString& name,QWidget* parent):QDialog(parent),attributes_(attributes){
    setObjectName("analysisPriorityDialog");setWindowTitle(tr("플레이어 우선순위 편집"));resize(760,620);
    auto layout=new QVBoxLayout(this);auto intro=new QLabel(tr("다음에 선택할 명령의 즉시 결과를 선호합니다. 미래를 예측하거나 최적 경로를 보장하지 않습니다. 게임 규칙이나 자원은 바뀌지 않습니다."));intro->setWordWrap(true);layout->addWidget(intro);
    auto form=new QFormLayout;name_=new QLineEdit(name);name_->setObjectName("priorityName");name_->setMaxLength(120);form->addRow(tr("이름"),name_);layout->addLayout(form);
    auto tabs=new QTabWidget;tabs->setObjectName("priorityEditorTabs");layout->addWidget(tabs,1);
    priorities_=new QTableWidget(0,3);priorities_->setObjectName("priorityAttributeTable");priorities_->setHorizontalHeaderLabels({tr("능력치"),tr("선호 방향"),tr("가중치")});priorities_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);priorities_->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);priorities_->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);priorities_->verticalHeader()->hide();tabs->addTab(priorities_,tr("즉시 결과 선호"));
    bands_=new QTableWidget(0,3);bands_->setObjectName("priorityBandTable");bands_->setHorizontalHeaderLabels({tr("자원·조건"),tr("편안한 최솟값"),tr("편안한 최댓값")});bands_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);bands_->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);bands_->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);bands_->verticalHeader()->hide();tabs->addTab(bands_,tr("자원·조건 선호 한도"));
    QMap<QString,QJsonObject> known,preferences,bands;QStringList ids;
    for(auto a:attributes_){auto o=a.toObject();auto id=o.value("id").toString();known[id]=o;ids<<id;}
    for(auto a:stored.value("priorities").toArray()){auto o=a.toObject();auto id=o.value("attributeId").toString();preferences[id]=o;if(!ids.contains(id))ids<<id;}
    for(auto a:stored.value("bands").toArray()){auto o=a.toObject();auto id=o.value("attributeId").toString();bands[id]=o;if(!ids.contains(id))ids<<id;}
    for(const auto& id:ids){const auto a=known.value(id);const auto label=a.isEmpty()?tr("사용 불가: %1").arg(id):a.value("label").toString(id);const auto p=preferences.value(id);
      int row=priorities_->rowCount();priorities_->insertRow(row);auto item=new QTableWidgetItem(label);item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);item->setData(Qt::UserRole,id);item->setToolTip(id);priorities_->setItem(row,0,item);
      auto direction=new QComboBox;direction->setObjectName("priorityDirection-"+id);direction->addItem(tr("선호 없음"),QString());direction->addItem(tr("증가"),"increase");direction->addItem(tr("감소"),"decrease");direction->setCurrentIndex(qMax(0,direction->findData(p.value("direction").toString())));priorities_->setCellWidget(row,1,direction);auto weight=new QSpinBox;weight->setObjectName("priorityWeight-"+id);weight->setRange(1,10);weight->setValue(p.value("weight").toInt(1));weight->setEnabled(direction->currentIndex()>0);priorities_->setCellWidget(row,2,weight);connect(direction,qOverload<int>(&QComboBox::currentIndexChanged),weight,[weight](int n){weight->setEnabled(n>0);});priorities_->setRowHeight(row,44);
      if((a.value("kind")=="resource"||a.value("kind")=="condition")||bands.contains(id)){row=bands_->rowCount();bands_->insertRow(row);auto b=new QTableWidgetItem((a.value("kind")=="resource"||a.value("kind")=="condition")?label:tr("사용 불가: %1").arg(id));b->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);b->setData(Qt::UserRole,id);b->setToolTip(id);bands_->setItem(row,0,b);bands_->setCellWidget(row,1,boundCell("priorityMin-"+id,bands.value(id),"min",a.value("min").toInt()));bands_->setCellWidget(row,2,boundCell("priorityMax-"+id,bands.value(id),"max",a.value("max").toInt()));bands_->setRowHeight(row,44);}
    }
    auto note=new QLabel(tr("각 목록은 최대 32개입니다. 범위는 자원·조건에만 설정할 수 있으며 선언된 최솟값·최댓값 안에 있어야 합니다. 사용 불가 항목은 선호 없음 또는 범위 해제로 직접 제거하세요."));note->setWordWrap(true);layout->addWidget(note);error_=new QLabel;error_->setObjectName("priorityEditorError");error_->setWordWrap(true);layout->addWidget(error_);
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons->setObjectName("priorityEditorButtons");layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);connect(buttons,&QDialogButtonBox::accepted,this,[this]{
      profileName=name_->text().trimmed();if(profileName.isEmpty()){error_->setText(tr("이름을 입력하세요."));return;}
      QJsonArray preferences,bands;for(int row=0;row<priorities_->rowCount();++row){const auto direction=qobject_cast<QComboBox*>(priorities_->cellWidget(row,1))->currentData().toString();if(!direction.isEmpty())preferences.append(QJsonObject{{"attributeId",priorities_->item(row,0)->data(Qt::UserRole).toString()},{"direction",direction},{"weight",qobject_cast<QSpinBox*>(priorities_->cellWidget(row,2))->value()}});}
      for(int row=0;row<bands_->rowCount();++row){QJsonObject band{{"attributeId",bands_->item(row,0)->data(Qt::UserRole).toString()}};for(int col=1;col<=2;++col){auto cell=bands_->cellWidget(row,col);if(cell->findChild<QCheckBox*>()->isChecked())band[col==1?"min":"max"]=cell->findChild<QSpinBox*>()->value();}if(band.size()>1){if(band.contains("min")&&band.contains("max")&&band.value("min").toInt()>band.value("max").toInt()){error_->setText(tr("안심 범위의 최솟값이 최댓값보다 큽니다."));return;}bands.append(band);}}
      for(const auto& value:preferences){const auto id=value.toObject().value("attributeId").toString();if(attribute(id).isEmpty()){error_->setText(tr("사용 불가 항목을 해제하세요: %1").arg(id));return;}}
      for(const auto& value:bands){const auto b=value.toObject();const auto id=b.value("attributeId").toString();const auto a=attribute(id);if(a.isEmpty()||(a.value("kind")!="resource"&&a.value("kind")!="condition")){error_->setText(tr("사용 불가 항목을 해제하세요: %1").arg(id));return;}for(const auto key:{QString("min"),QString("max")})if(b.contains(key)&&(b.value(key).toDouble()<a.value("min").toDouble()||b.value(key).toDouble()>a.value("max").toDouble())){error_->setText(tr("%1의 범위는 선언된 %2–%3 안에 있어야 합니다.").arg(id).arg(a.value("min").toDouble()).arg(a.value("max").toDouble()));return;}}
      if(preferences.size()>32||bands.size()>32){error_->setText(tr("각 목록은 최대 32개까지 선택할 수 있습니다."));return;}
      if(preferences.empty()&&bands.empty()){error_->setText(tr("우선순위 또는 안심 범위를 하나 이상 선택하세요."));return;}
      profile={{"formatVersion",1},{"priorities",preferences},{"bands",bands}};accept();
    });
  }
};
