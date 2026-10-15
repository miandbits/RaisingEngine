#include "studio.hpp"
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

void StudioWindow::organizeContent() {
  if (bridge.busy() || projectData().isEmpty()) return;
  const auto revision=view.value("revision"), root=view.value("root");
  QDialog dialog(this); dialog.setObjectName("contentOrganizer");
  dialog.setWindowTitle(tr("콘텐츠 일괄 정리")); dialog.resize(1000,700);
  auto layout=new QVBoxLayout(&dialog);
  auto help=new QLabel(tr("사용하지 않는 문서를 선택해 초안에서 삭제합니다. 프로젝트 저장 전까지 디스크는 바뀌지 않으며 한 번에 되돌릴 수 있습니다.\n연결이 남은 문서는 적용 후 공통 검증기의 오류 위치를 확인하세요. 기본 엔딩과 주인공은 프로젝트 설정에서 먼저 변경해야 합니다."));
  help->setWordWrap(true);layout->addWidget(help);
  auto search=new QLineEdit;search->setObjectName("organizerSearch");search->setPlaceholderText(tr("종류 · 이름 · ID · 파일 검색"));layout->addWidget(search);
  auto table=new QTableWidget(0,5);table->setObjectName("organizerTable");
  table->setHorizontalHeaderLabels({tr("삭제"),tr("종류"),tr("이름 / ID"),tr("사건 기간"),tr("파일")});
  table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);table->horizontalHeader()->setSectionResizeMode(4,QHeaderView::Stretch);layout->addWidget(table);
  const auto project=projectData();
  for(auto value:view.value("documents").toArray()) {
    const auto doc=value.toObject(),data=doc.value("data").toObject();const auto kind=doc.value("kind").toString();
    if(!QStringList{"activity","event","ending","character","scenario"}.contains(kind))continue;
    const auto id=data.value("id").toString(),file=doc.value("file").toString();int row=table->rowCount();table->insertRow(row);
    const bool protectedRow=(kind=="ending"&&id==project.value("defaultEndingId"))||(kind=="character"&&id==project.value("protagonistId"));
    auto selected=new QTableWidgetItem;selected->setCheckState(Qt::Unchecked);selected->setData(Qt::UserRole,file);selected->setFlags(protectedRow?Qt::ItemIsSelectable:Qt::ItemIsUserCheckable|Qt::ItemIsEnabled|Qt::ItemIsSelectable);if(protectedRow)selected->setToolTip(tr("기본 엔딩 / 주인공 보호"));table->setItem(row,0,selected);
    auto window=data.value("window").toObject();QStringList labels{kind,data.value("label").toString(data.value("name").toString(id))+" · "+id,window.isEmpty()?QString():QString("%1–%2").arg(window.value("fromPeriod").toInt()).arg(window.value("throughPeriod").toInt()),file};
    for(int col=1;col<5;++col){auto item=new QTableWidgetItem(labels[col-1]);item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);table->setItem(row,col,item);}
  }
  connect(search,&QLineEdit::textChanged,&dialog,[=](QString query){for(int row=0;row<table->rowCount();++row){QString text;for(int c=1;c<5;++c)text+=table->item(row,c)->text()+" ";table->setRowHidden(row,!text.contains(query,Qt::CaseInsensitive));}});
  auto status=new QLabel(tr("선택한 문서 0개"));status->setObjectName("organizerStatus");layout->addWidget(status);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Save)->setText(tr("선택 문서를 초안에서 삭제"));layout->addWidget(buttons);
  connect(table,&QTableWidget::itemChanged,&dialog,[=]{int count=0;for(int r=0;r<table->rowCount();++r)if(table->item(r,0)->checkState()==Qt::Checked)++count;status->setText(tr("선택한 문서 %1개 · 숨겨진 선택도 포함").arg(count));});
  QJsonArray edits;
  connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
    if(bridge.busy()||view.value("revision")!=revision||view.value("root")!=root){status->setText(tr("프로젝트가 변경되었습니다. 선택 내용을 확인한 뒤 창을 다시 여세요."));return;}
    edits={};for(int r=0;r<table->rowCount();++r)if(table->item(r,0)->checkState()==Qt::Checked&&(table->item(r,0)->flags()&Qt::ItemIsEnabled))edits.append(QJsonObject{{"type","delete"},{"file",table->item(r,0)->data(Qt::UserRole).toString()}});
    if(edits.isEmpty()||edits.size()>100){status->setText(tr("한 번에 1~100개 문서를 선택하세요."));return;}dialog.accept();
  });
  connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  if(dialog.exec()==QDialog::Accepted)request("edit",{{"edit",QJsonObject{{"type","batch"},{"edits",edits}}}});
}
