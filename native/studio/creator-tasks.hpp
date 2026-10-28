#pragma once
#include "editor-input.hpp"
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <functional>

// A task launcher is a view of existing actions, never another command handler.
// Late action lookup lets installed extensions add release/analysis tools too.
inline void showCreatorTasks(QMainWindow* window,std::function<bool()> projectReady) {
  if(!commitEditorInput(window))return;
  QPointer<QWidget> previousFocus=QApplication::focusWidget();
  QDialog dialog(window);dialog.setObjectName("creatorTasksDialog");dialog.setWindowTitle(QObject::tr("무엇을 만들까요?"));dialog.resize(700,520);
  auto shell=new QVBoxLayout(&dialog);
  auto help=new QLabel(QObject::tr("목표를 고르고 기존 제작 도구를 여세요. 모든 도구는 메뉴와 명령 검색에서도 사용할 수 있습니다."));help->setWordWrap(true);shell->addWidget(help);
  auto tabs=new QTabWidget;tabs->setObjectName("creatorTaskTabs");shell->addWidget(tabs,1);
  struct Entry {QPointer<QAction> action;QPushButton* button;bool needsProject;};QList<Entry> entries;QPointer<QAction> selected;
  auto page=[&](QString title,QString name,QStringList ids){
    auto area=new QScrollArea;area->setWidgetResizable(true);area->setFrameShape(QFrame::NoFrame);area->setObjectName(name);auto body=new QWidget;auto layout=new QVBoxLayout(body);area->setWidget(body);tabs->addTab(area,title);
    for(const auto& id:ids){auto action=window->findChild<QAction*>(id);if(!action)continue;auto button=new QPushButton(action->text());button->setObjectName("creatorTask-"+id);button->setMinimumHeight(40);button->setToolTip(action->toolTip());layout->addWidget(button);const bool needsProject=!QStringList{"newProject","showProjectManager","firstGameChecklist"}.contains(id);entries.push_back({action,button,needsProject});QPointer<QAction> command=action;QObject::connect(button,&QPushButton::clicked,&dialog,[&,command]{if(!command||!command->isEnabled()||!command->isVisible())return;selected=command;dialog.accept();});}
    layout->addStretch();
  };
  page(QObject::tr("콘텐츠 만들기"),"creatorContentTasks",{"newProject","showProjectManager","showAuthoringWorkspace","storyFlowAction","editRoster"});
  page(QObject::tr("화면 디자인"),"creatorDesignTasks",{"raisingScreenFlow","shareScreenSection","editTimelines","screenSettings","editDesignSystem","readableLayout","editComponents"});
  page(QObject::tr("플레이 확인"),"creatorCheckTasks",{"inspectRenderer","validateProject","playProject","showAnalysis"});
  page(QObject::tr("출시 준비"),"creatorReleaseTasks",{"projectSettings","saveProject","exportGame"});
  auto notice=new QLabel;notice->setObjectName("creatorTaskNotice");notice->setWordWrap(true);shell->addWidget(notice);
  auto refresh=[&]{const bool ready=projectReady();notice->setText(ready?QString():QObject::tr("프로젝트를 열고 현재 작업이 끝나면 편집·검사·내보내기를 사용할 수 있습니다."));notice->setVisible(!ready);for(const auto& entry:entries){entry.button->setEnabled(entry.action&&entry.action->isEnabled()&&entry.action->isVisible()&&(!entry.needsProject||ready));if(entry.action){entry.button->setText(entry.action->text());entry.button->setToolTip(entry.action->toolTip());}}};
  for(const auto& entry:entries)QObject::connect(entry.action,&QAction::changed,&dialog,refresh);
  QTimer availability;availability.setInterval(100);QObject::connect(&availability,&QTimer::timeout,&dialog,refresh);availability.start();refresh();
  auto footer=new QDialogButtonBox(QDialogButtonBox::Close);auto search=footer->addButton(QObject::tr("모든 도구 검색…"),QDialogButtonBox::ActionRole);search->setObjectName("creatorFindTools");QObject::connect(search,&QPushButton::clicked,&dialog,[&]{selected=window->findChild<QAction*>("findAuthoringTool");dialog.accept();});QObject::connect(footer,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);shell->addWidget(footer);
  const bool accepted=dialog.exec()==QDialog::Accepted;
  if(previousFocus&&previousFocus->isVisible()&&previousFocus->isEnabled()){window->activateWindow();QApplication::setActiveWindow(window);previousFocus->setFocus(Qt::OtherFocusReason);}
  if(!accepted||!selected||!selected->isEnabled()||!selected->isVisible())return;
  for(const auto& entry:entries)if(entry.action==selected&&entry.needsProject&&!projectReady())return;
  selected->trigger();
}
