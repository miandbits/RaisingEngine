#pragma once
#include <QDialog>
#include <QLabel>
#include <QPointer>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <functional>

// Reuse the validated editors in-place. Their Save/Cancel only completes this
// editing step; the workspace still owns revision checks and project edits.
class WorkspaceInlineEditor final : public QWidget {
public:
  explicit WorkspaceInlineEditor(QWidget *parent=nullptr):QWidget(parent) {
    auto layout=new QVBoxLayout(this);
    heading=new QLabel;heading->setWordWrap(true);heading->setObjectName("workspaceInlineTitle");layout->addWidget(heading);
    area=new QScrollArea;area->setWidgetResizable(true);layout->addWidget(area,1);
    setObjectName("workspaceInlineEditor");
  }
  void open(QDialog *dialog,const QString &title,std::function<bool()> accept,
            std::function<void()> finished) {
    heading->setText(title);active=dialog;onFinished=std::move(finished);
    dialog->setParent(this);dialog->setWindowFlags(Qt::Widget);
    dialog->setMinimumSize(0,0);area->setWidget(dialog);
    connect(dialog,&QDialog::finished,this,[this,dialog,accept](int result){
      if(result==QDialog::Accepted&&!accept()){dialog->show();return;}
      complete();
    });
    dialog->show();
  }
  void cancel(){if(active)active->reject();}
  void complete(){if(!active)return;auto dialog=active;area->takeWidget();active=nullptr;dialog->deleteLater();auto finished=std::move(onFinished);if(finished)finished();}
private:
  QLabel *heading;
  QScrollArea *area;
  QPointer<QDialog> active;
  std::function<void()> onFinished;
};
