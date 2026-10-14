#pragma once
#include <QTreeWidget>
#include <QDropEvent>
#include <functional>

class HierarchyTree final:public QTreeWidget {
public:
  using QTreeWidget::QTreeWidget;
  std::function<void(QString,QString)> reparent;
protected:
  void dropEvent(QDropEvent *event) override {
    auto target=itemAt(event->position().toPoint());
    if(selectedItems().isEmpty()||!reparent){event->ignore();return;}
    auto id=selectedItems().first()->data(0,Qt::UserRole).toString();
    auto parent=target?target->data(0,Qt::UserRole).toString():QString();
    event->ignore();if(!id.isEmpty())reparent(id,parent);
  }
};
