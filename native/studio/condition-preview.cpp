#include "studio.hpp"
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

void StudioWindow::editConditionPreview() {
  if(bridge.busy()||view["root"].toString().isEmpty())return;
  QDialog dialog(this);dialog.setWindowTitle(tr("표시 조건 미리보기 · 저장 데이터 변경 없음"));dialog.resize(690,650);
  auto layout=new QVBoxLayout(&dialog);
  auto info=new QLabel(tr("임시 값을 적용해 캔버스의 표시 조건을 확인합니다.\n실행 중 게임과 프로젝트 파일은 변경하지 않습니다. 편집 후에는 다시 적용하세요."));info->setWordWrap(true);layout->addWidget(info);
  auto phase=new QComboBox;phase->setObjectName("conditionPreviewPhase");
  for(const auto& entry:QList<QPair<QString,QString>>{{tr("편성 / 자유 행동"),"planning"},{tr("활동 결과"),"result"},{tr("대사"),"dialogue"},{tr("선택지"),"choice"},{tr("종료"),"finished"}})phase->addItem(entry.first,entry.second);
  phase->setCurrentIndex(qMax(0,phase->findData(conditionPreview["phase"].toString("planning"))));layout->addWidget(phase);
  auto search=new QLineEdit;search->setObjectName("conditionPreviewSearch");search->setPlaceholderText(tr("이름 / ID 검색"));layout->addWidget(search);
  auto table=new QTableWidget(0,3);table->setObjectName("conditionPreviewValues");table->setHorizontalHeaderLabels({tr("ID"),tr("이름"),tr("임시 값")});table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);layout->addWidget(table);
  QMap<QString,QSpinBox*> fields;
  const auto overrides=projectData()["initialState"].toObject()["attributes"].toObject();
  for(const auto& document:view["documents"].toArray())if(document.toObject()["kind"]=="attributes")for(const auto& value:document.toObject()["data"].toObject()["attributes"].toArray()){
    const auto a=value.toObject();const auto id=a["id"].toString();const int row=table->rowCount();table->insertRow(row);
    for(int col=0;col<2;col++){auto item=new QTableWidgetItem(col==0?id:a["label"].toString());item->setFlags(item->flags()&~Qt::ItemIsEditable);table->setItem(row,col,item);}
    auto field=new QSpinBox;field->setObjectName("preview-"+id);field->setRange(a["min"].toInt(),a["max"].toInt());
    const auto previous=conditionPreview["attributes"].toObject();
    field->setValue((previous.contains(id)?previous[id]:(overrides.contains(id)?overrides[id]:a["initial"])).toInt());fields[id]=field;table->setCellWidget(row,2,field);
  }
  connect(search,&QLineEdit::textChanged,&dialog,[table](const QString& q){for(int r=0;r<table->rowCount();r++)table->setRowHidden(r,!table->item(r,0)->text().contains(q,Qt::CaseInsensitive)&&!table->item(r,1)->text().contains(q,Qt::CaseInsensitive));});
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Ok)->setText(tr("미리보기 적용"));layout->addWidget(buttons);
  connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  if(dialog.exec()!=QDialog::Accepted)return;
  QJsonObject values;for(auto it=fields.begin();it!=fields.end();++it)values[it.key()]=it.value()->value();
  request("conditionPreview",{{"revision",view["revision"]},{"attributes",values},{"phase",phase->currentData().toString()}},[this](const QJsonObject& reply){
    if(!reply["error"].toString().isEmpty())return;
    conditionPreview=reply["conditionPreview"].toObject();refresh();
    status->setText(tr("조건 미리보기 적용 · 전체 요소로 해제"));
  });
}
