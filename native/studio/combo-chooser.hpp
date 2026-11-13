#pragma once
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QPushButton>

namespace raising {
// Keep the ordinary combo model and selection semantics while exposing a standard
// button action to native accessibility clients that cannot open the combo itself.
class ComboChooserRow final : public QWidget {
public:
  ComboChooserRow(QComboBox *field,const QString &fieldLabel,QWidget *parent=nullptr):QWidget(parent),combo(field){
    auto layout=new QHBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(6);layout->addWidget(combo,1);
    if(combo->accessibleName().isEmpty())combo->setAccessibleName(fieldLabel);
    choose=new QPushButton(QObject::tr("선택…"),this);choose->setObjectName(combo->objectName()+"Choose");choose->setAccessibleName(fieldLabel+" · "+choose->text());choose->setAutoDefault(false);choose->setEnabled(combo->isEnabled());layout->addWidget(choose);
    combo->installEventFilter(this);
    QObject::connect(choose,&QPushButton::clicked,combo,[field]{if(!field->isEnabled())return;field->setFocus(Qt::OtherFocusReason);field->showPopup();});
  }
protected:
  bool eventFilter(QObject *watched,QEvent *event) override {
    if(watched==combo&&event->type()==QEvent::EnabledChange)choose->setEnabled(combo->isEnabled());
    return QWidget::eventFilter(watched,event);
  }
private:
  QComboBox *combo;
  QPushButton *choose;
};
inline QWidget *comboChooser(QComboBox *combo,const QString &fieldLabel,QWidget *parent=nullptr){return new ComboChooserRow(combo,fieldLabel,parent);}
}
