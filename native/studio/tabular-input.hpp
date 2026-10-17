#pragma once
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <functional>

// Designer-facing spreadsheet input. Does not interpret code or JSON.
inline bool readTabularInput(QWidget *parent, const QStringList &headers,
                            const std::function<QString(const QList<QStringList>&)> &apply) {
  QDialog dialog(parent); dialog.setWindowTitle(QObject::tr("표에서 여러 항목 입력")); dialog.setObjectName("tabularInputDialog");
  auto layout=new QVBoxLayout(&dialog);
  auto help=new QLabel(QObject::tr("스프레드시트에서 아래 열 순서대로 복사해 붙여넣으세요. 탭으로 열을, 줄바꿈으로 행을 구분합니다. 머리글 없이 최대 500행입니다.\n%1").arg(headers.join(" · "))); help->setWordWrap(true); layout->addWidget(help);
  auto text=new QPlainTextEdit; text->setObjectName("tabularInputText"); layout->addWidget(text);
  auto preview=new QTableWidget(0,headers.size());preview->setObjectName("tabularInputPreview");preview->setHorizontalHeaderLabels(headers);preview->setEditTriggers(QAbstractItemView::NoEditTriggers);preview->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);layout->addWidget(preview);
  auto error=new QLabel;error->setObjectName("tabularInputError");error->setWordWrap(true);layout->addWidget(error);
  QList<QStringList> rows;bool valid=false;
  auto update=[&]{rows.clear();preview->setRowCount(0);valid=false;error->clear();const auto lines=text->toPlainText().split('\n');for(int i=0;i<lines.size();++i){auto line=lines[i];if(line.endsWith('\r'))line.chop(1);if(line.trimmed().isEmpty())continue;auto fields=line.split('\t');if(fields.size()!=headers.size()){error->setText(QObject::tr("%1행: %2개 열이 필요합니다.").arg(i+1).arg(headers.size()));return;}if(rows.size()>=500){error->setText(QObject::tr("한 번에 최대 500행까지 입력하세요."));return;}for(auto &field:fields)field=field.trimmed();rows.append(fields);}for(const auto &fields:rows){int row=preview->rowCount();preview->insertRow(row);for(int col=0;col<fields.size();++col)preview->setItem(row,col,new QTableWidgetItem(fields[col]));}valid=!rows.isEmpty();};
  QObject::connect(text,&QPlainTextEdit::textChanged,&dialog,update);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Ok)->setText(QObject::tr("표에 반영"));layout->addWidget(buttons);
  QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{update();if(!valid){if(error->text().isEmpty())error->setText(QObject::tr("입력할 행을 붙여넣으세요."));return;}const auto problem=apply(rows);if(!problem.isEmpty()){error->setText(problem);return;}dialog.accept();});
  dialog.resize(900,620);return dialog.exec()==QDialog::Accepted;
}
