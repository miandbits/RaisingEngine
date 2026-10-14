#include "studio.hpp"
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
bool validUserText(const QString &text,int minimum,int maximum){
  int count=0;
  for(int i=0;i<text.size();++i){
    const auto ch=text[i];const auto value=ch.unicode();
    if(value<32||(value>=127&&value<=159)||value==0x2028||value==0x2029)return false;
    if(ch.isHighSurrogate()){if(i+1>=text.size()||!text[i+1].isLowSurrogate())return false;++i;}
    else if(ch.isLowSurrogate())return false;
    ++count;
  }
  return count>=minimum&&count<=maximum;
}
QString missingUserField(const QJsonValue &value,const QSet<QString> &ids){
  if(value.isObject()){
    const auto object=value.toObject();
    if(object.contains("userFieldId")&&!ids.contains(object.value("userFieldId").toString()))return object.value("userFieldId").toString();
    for(auto it=object.begin();it!=object.end();++it){const auto missing=missingUserField(it.value(),ids);if(!missing.isEmpty())return missing;}
  }else if(value.isArray())for(auto child:value.toArray()){const auto missing=missingUserField(child,ids);if(!missing.isEmpty())return missing;}
  return {};
}
}

void StudioWindow::editUserFields(){
  if(bridge.busy()||projectData().isEmpty())return;
  const auto original=projectData();auto project=original;
  const auto revision=view.value("revision"),root=view.value("root");
  QDialog dialog(this);dialog.setObjectName("userFieldsDialog");dialog.setWindowTitle(tr("새 게임 · 사용자 입력 필드"));dialog.resize(920,500);
  auto layout=new QVBoxLayout(&dialog);
  auto help=new QLabel(tr("이름 등 새 게임을 시작할 때 입력할 문자를 정의합니다. 입력 요소를 제목 화면에 배치하고, 텍스트의 ‘사용자 입력값’ 연결로 표시하세요. 길이는 한글·이모지를 각각 한 문자로 셉니다. 게임 시작 후에는 변경되지 않습니다."));help->setWordWrap(true);layout->addWidget(help);
  auto table=new QTableWidget(0,5);table->setObjectName("userFieldsTable");table->setHorizontalHeaderLabels({tr("ID"),tr("이름"),tr("초기값"),tr("최소 길이"),tr("최대 길이")});table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table->setSelectionBehavior(QAbstractItemView::SelectRows);layout->addWidget(table);
  const auto append=[&](const QJsonObject &field){
    const int row=table->rowCount();table->insertRow(row);
    auto id=new QTableWidgetItem(field.value("id").toString());id->setData(Qt::UserRole,field);table->setItem(row,0,id);
    table->setItem(row,1,new QTableWidgetItem(field.value("label").toString()));table->setItem(row,2,new QTableWidgetItem(field.value("initial").toString()));
    for(int col=3;col<=4;++col){auto spin=new QSpinBox;spin->setRange(col==3?0:1,256);spin->setValue(field.value(col==3?"minLength":"maxLength").toInt(col==3?0:32));table->setCellWidget(row,col,spin);}
  };
  for(auto value:original.value("userFields").toArray())append(value.toObject());
  auto row=new QHBoxLayout;auto add=new QPushButton(tr("＋ 필드"));add->setObjectName("userFieldAdd");auto remove=new QPushButton(tr("선택 필드 삭제"));remove->setObjectName("userFieldRemove");row->addWidget(add);row->addWidget(remove);row->addStretch();layout->addLayout(row);
  auto error=new QLabel;error->setObjectName("userFieldsError");error->setWordWrap(true);error->setStyleSheet("color:#f3a18a");layout->addWidget(error);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons->setObjectName("userFieldsButtons");layout->addWidget(buttons);
  connect(add,&QPushButton::clicked,&dialog,[&]{
    if(table->rowCount()>=32){error->setText(tr("최대 32개 필드를 지원합니다."));return;}
    QSet<QString> ids;for(int i=0;i<table->rowCount();++i)ids.insert(table->item(i,0)->text());int n=1;while(ids.contains("user-field-"+QString::number(n)))++n;
    append({{"id","user-field-"+QString::number(n)},{"type","text"},{"label",tr("이름")},{"initial",tr("주인공")},{"minLength",1},{"maxLength",32}});table->setCurrentCell(table->rowCount()-1,0);
  });
  connect(remove,&QPushButton::clicked,&dialog,[&]{if(table->currentRow()>=0)table->removeRow(table->currentRow());});
  connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
    if(view.value("revision")!=revision||view.value("root")!=root){error->setText(tr("프로젝트가 변경되었습니다. 창을 닫고 다시 여세요."));return;}
    QJsonArray fields;QSet<QString> ids;
    for(int i=0;i<table->rowCount();++i){
      auto field=table->item(i,0)->data(Qt::UserRole).toJsonObject();const auto id=table->item(i,0)->text(),label=table->item(i,1)->text(),initial=table->item(i,2)->text();
      const int minimum=qobject_cast<QSpinBox*>(table->cellWidget(i,3))->value(),maximum=qobject_cast<QSpinBox*>(table->cellWidget(i,4))->value();
      if(!QRegularExpression("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$").match(id).hasMatch()||id.size()>64||ids.contains(id)){error->setText(tr("%1행: ID는 영문 소문자로 시작하고 단어를 하이픈으로 구분한 고유 ID(최대 64자)여야 합니다.").arg(i+1));return;}
      if(label.trimmed().isEmpty()){error->setText(tr("%1행: 필드 이름을 입력하세요.").arg(i+1));return;}
      if(minimum>maximum||!validUserText(initial,minimum,maximum)){error->setText(tr("%1행: 초기값의 길이와 최소·최대 길이를 확인하세요. 줄바꿈·제어 문자는 사용할 수 없습니다.").arg(i+1));return;}
      field["id"]=id;field["type"]="text";field["label"]=label;field["initial"]=initial;field["minLength"]=minimum;field["maxLength"]=maximum;fields.append(field);ids.insert(id);
    }
    const auto missing=missingUserField(original.value("visual"),ids);
    if(!missing.isEmpty()){error->setText(tr("화면에서 사용 중인 필드 ‘%1’은 삭제하거나 ID를 바꿀 수 없습니다. 입력·텍스트 요소의 연결을 먼저 바꾸세요.").arg(missing));return;}
    if(!fields.isEmpty()||original.contains("userFields"))project["userFields"]=fields;
    dialog.accept();
  });
  if(dialog.exec()!=QDialog::Accepted||project==original)return;
  replaceProject(project);
}
