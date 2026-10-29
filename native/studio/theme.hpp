#pragma once
#include <QApplication>
#include <QFont>
inline void applyStudioTheme(QApplication &app) {
  app.setStyle("Fusion");
#ifdef Q_OS_WIN
  app.setFont(QFont("Malgun Gothic", 10));
#else
  auto font=app.font();font.setPointSize(11);app.setFont(font);
#endif
  QPalette p;
  p.setColor(QPalette::Window, QColor("#24292f"));
  p.setColor(QPalette::WindowText, QColor("#e3e9ef"));
  p.setColor(QPalette::Base, QColor("#1e2329"));
  p.setColor(QPalette::AlternateBase, QColor("#293039"));
  p.setColor(QPalette::Text, QColor("#e3e9ef"));
  p.setColor(QPalette::PlaceholderText, QColor("#9aa7b4"));
  p.setColor(QPalette::Button, QColor("#303740"));
  p.setColor(QPalette::ButtonText, QColor("#e3e9ef"));
  p.setColor(QPalette::Highlight, QColor("#356ca7"));
  p.setColor(QPalette::HighlightedText, Qt::white);
  p.setColor(QPalette::Disabled, QPalette::Text, QColor("#7f8b97"));
  p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#7f8b97"));
  app.setPalette(p);
  app.setStyleSheet(
      "QMainWindow::separator{background:#14181e;width:4px;height:4px} "
      "QDockWidget::title{padding:7px;background:#303741} "
      "QToolBar{padding:5px;spacing:7px;border:0;border-bottom:1px solid "
      "#15191f} QToolButton{padding:5px 9px} "
      "QToolButton:hover{background:#3c4755} "
      "QLineEdit,QSpinBox,QComboBox{padding:4px;min-height:20px;border:1px "
      "solid #46515f;border-radius:3px} "
      "QLineEdit:focus,QSpinBox:focus,QComboBox:focus{border:1px solid "
      "#75b3ff} QGroupBox{font-weight:600;border:1px solid "
      "#46515f;border-radius:3px;margin-top:8px} "
      "QGroupBox::title{subcontrol-origin:margin;left:9px} "
      "QTabBar::tab{padding:8px 14px} "
      "QTabBar::tab:selected{background:#345f89} QTreeView::item{height:27px} "
      "QListWidget::item{padding:5px} QPushButton{padding:6px 10px} "
      "QStatusBar{border-top:1px solid #46515f} ");
}
