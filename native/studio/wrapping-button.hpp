#pragma once
#include <QPainter>
#include <QPushButton>
#include <QStyleOptionButton>

// Text-only commands retain QPushButton keyboard/accessibility behavior while
// allowing translated labels to reflow inside narrow docks and scroll areas.
class WrappingButton final : public QPushButton {
public:
  explicit WrappingButton(const QString& text,QWidget* parent=nullptr):QPushButton(text,parent){
    auto policy=QSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);policy.setHeightForWidth(true);setSizePolicy(policy);
  }
  QSize sizeHint()const override{const int width=qMin(280,QPushButton::sizeHint().width());return {width,heightForWidth(width)};}
  QSize minimumSizeHint()const override{return {80,QPushButton::sizeHint().height()};}
  int heightForWidth(int width)const override{
    QStyleOptionButton option;initStyleOption(&option);option.rect=QRect(0,0,width,1000);
    const auto contents=style()->subElementRect(QStyle::SE_PushButtonContents,&option,this);
    const auto bounds=fontMetrics().boundingRect(QRect(0,0,qMax(1,contents.width()),10000),textFlags(),text());
    return qMax(QPushButton::sizeHint().height(),bounds.height()+option.rect.height()-contents.height());
  }
protected:
  void paintEvent(QPaintEvent*)override{
    QStyleOptionButton option;initStyleOption(&option);const auto caption=option.text;option.text.clear();QPainter painter(this);style()->drawControl(QStyle::CE_PushButton,&option,&painter,this);
    auto contents=style()->subElementRect(QStyle::SE_PushButtonContents,&option,this);
    if(option.state&(QStyle::State_Sunken|QStyle::State_On))contents.translate(style()->pixelMetric(QStyle::PM_ButtonShiftHorizontal,&option,this),style()->pixelMetric(QStyle::PM_ButtonShiftVertical,&option,this));
    style()->drawItemText(&painter,contents,textFlags(),option.palette,isEnabled(),caption,QPalette::ButtonText);
  }
private:
  int textFlags()const{return Qt::AlignCenter|Qt::TextWordWrap|Qt::TextShowMnemonic|(style()->styleHint(QStyle::SH_UnderlineShortcut,nullptr,this)?0:Qt::TextHideMnemonic);}
};
