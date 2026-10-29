#pragma once
#include "editor-input.hpp"
#include <QAction>
#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <functional>
#include <memory>

// Persistent navigation over existing actions, never a second command handler.
// A stage means where the author is working, not a passed acceptance checklist.
inline void showCreatorTasks(QMainWindow* window,std::function<bool()> projectReady,std::function<QString()> context={}) {
  if(!commitEditorInput(window))return;
  if(auto existing=window->findChild<QDockWidget*>("creatorTasksDock")){existing->show();existing->raise();return;}
  auto dock=new QDockWidget(QObject::tr("무엇을 만들까요?"),window);dock->setObjectName("creatorTasksDock");
  auto body=new QWidget;auto shell=new QVBoxLayout(body);dock->setWidget(body);
  auto help=new QLabel(QObject::tr("작업 단계를 선택하세요. 도구를 열어도 이 안내와 선택한 단계는 유지됩니다. 단계 선택은 검증 완료를 뜻하지 않습니다."));help->setWordWrap(true);shell->addWidget(help);
  auto current=new QLabel;current->setObjectName("creatorTaskContext");current->setWordWrap(true);current->setTextFormat(Qt::PlainText);shell->addWidget(current);
  auto tabs=new QTabWidget;tabs->setObjectName("creatorTaskTabs");shell->addWidget(tabs,1);
  struct Entry {QPointer<QAction> action;QPushButton* button;bool needsProject;};auto entries=std::make_shared<QList<Entry>>();
  auto invoke=[window,projectReady](QPointer<QAction> command,bool needsProject){
    if(!command||!command->isEnabled()||!command->isVisible()||!commitEditorInput(window))return;
    if(needsProject&&!projectReady())return;
    command->trigger();
  };
  auto page=[&](QString title,QString name,QStringList ids){
    auto area=new QScrollArea;area->setWidgetResizable(true);area->setFrameShape(QFrame::NoFrame);area->setObjectName(name);auto content=new QWidget;auto layout=new QVBoxLayout(content);area->setWidget(content);tabs->addTab(area,title);
    for(const auto& id:ids){auto action=window->findChild<QAction*>(id);if(!action)continue;auto button=new QPushButton(action->text());button->setObjectName("creatorTask-"+id);button->setMinimumHeight(40);layout->addWidget(button);const bool needsProject=!QStringList{"newProject","showProjectManager","firstGameChecklist"}.contains(id);entries->push_back({action,button,needsProject});QPointer<QAction> command=action;QObject::connect(button,&QPushButton::clicked,dock,[invoke,command,needsProject]{invoke(command,needsProject);});}
    layout->addStretch();
  };
  page(QObject::tr("콘텐츠 만들기"),"creatorContentTasks",{"newProject","showProjectManager","showAuthoringWorkspace","storyFlowAction","editRoster"});
  page(QObject::tr("화면 디자인"),"creatorDesignTasks",{"raisingScreenFlow","shareScreenSection","editTimelines","screenSettings","editDesignSystem","readableLayout","editComponents"});
  page(QObject::tr("플레이 확인"),"creatorCheckTasks",{"inspectRenderer","validateProject","playProject","showAnalysis"});
  page(QObject::tr("출시 준비"),"creatorReleaseTasks",{"projectSettings","saveProject","exportGame"});
  auto notice=new QLabel;notice->setObjectName("creatorTaskNotice");notice->setWordWrap(true);shell->addWidget(notice);
  auto refresh=[entries,projectReady,context,current,notice]{const bool ready=projectReady();notice->setText(ready?QString():QObject::tr("프로젝트를 열고 현재 작업이 끝나면 편집·검사·내보내기를 사용할 수 있습니다."));notice->setVisible(!ready);current->setText(context?context():QString());current->setVisible(!current->text().isEmpty());for(const auto& entry:*entries){entry.button->setEnabled(entry.action&&entry.action->isEnabled()&&entry.action->isVisible()&&(!entry.needsProject||ready));if(entry.action){entry.button->setText(entry.action->text());entry.button->setToolTip(entry.action->toolTip());}}};
  for(const auto& entry:*entries)QObject::connect(entry.action,&QAction::changed,dock,refresh);
  auto availability=new QTimer(dock);availability->setInterval(150);QObject::connect(availability,&QTimer::timeout,dock,[dock,refresh]{if(dock->isVisible())refresh();});availability->start();refresh();
  auto search=new QPushButton(QObject::tr("모든 도구 검색…"));search->setObjectName("creatorFindTools");shell->addWidget(search);QObject::connect(search,&QPushButton::clicked,dock,[window,invoke]{invoke(window->findChild<QAction*>("findAuthoringTool"),false);});
  window->addDockWidget(Qt::BottomDockWidgetArea,dock);
  if(auto project=window->findChild<QDockWidget*>("projectDock"))window->tabifyDockWidget(project,dock);
  dock->show();dock->raise();window->resizeDocks({dock},{280},Qt::Vertical);
}
