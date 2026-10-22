#pragma once
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>
#include <functional>

inline void showAuthoringToolSearch(QWidget* parent,QMenu* menu) {
  struct Tool { QPointer<QAction> action; QString path; };
  QList<Tool> tools;
  std::function<void(QMenu*,QString)> collect=[&](QMenu* branch,QString path){
    for(auto* action:branch->actions()){
      if(action->isSeparator()||!action->isVisible()||action->objectName()=="findAuthoringTool")continue;
      if(action->menu())collect(action->menu(),path.isEmpty()?action->text():path+" › "+action->text());
      else tools.push_back({action,path});
    }
  };
  collect(menu,{});
  QDialog dialog(parent);dialog.setObjectName("authoringToolSearch");dialog.setWindowTitle(QObject::tr("제작 도구 찾기"));dialog.resize(700,520);
  // Inspector-only tools must remain discoverable when their section is collapsed
  // or scrolled out of view. Dispatch the existing button so its guards still apply.
  for(const auto& id:{"dataBindingPicker","presentationVariants","elementVisibleCondition","selectionBatch","componentSource"}) {
    auto button=parent->findChild<QPushButton*>(id);if(!button)continue;
    auto action=new QAction(button->text(),&dialog);action->setEnabled(button->isEnabled());action->setToolTip(button->toolTip());
    QPointer<QPushButton> target=button;
    QObject::connect(action,&QAction::triggered,&dialog,[target]{if(target&&target->isEnabled())target->click();});
    tools.push_back({action,QObject::tr("선택 요소")});
  }
  auto layout=new QVBoxLayout(&dialog);
  auto search=new QLineEdit;search->setObjectName("authoringToolQuery");search->setPlaceholderText(QObject::tr("인물, 활동, 조건, 폰트처럼 작업 이름으로 찾으세요"));layout->addWidget(search);
  auto list=new QListWidget;list->setObjectName("authoringToolResults");layout->addWidget(list);
  auto status=new QLabel;status->setObjectName("authoringToolCount");layout->addWidget(status);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Open|QDialogButtonBox::Cancel);layout->addWidget(buttons);
  auto open=buttons->button(QDialogButtonBox::Open);open->setDefault(true);
  auto canOpen=[&]{auto* item=list->currentItem();return item&&(item->flags()&Qt::ItemIsEnabled);};
  auto filter=[&]{
    list->clear();const auto words=search->text().simplified().split(' ',Qt::SkipEmptyParts);
    for(int i=0;i<tools.size();++i){const auto& tool=tools[i];if(!tool.action)continue;
      const auto haystack=tool.action->text()+" "+tool.path+" "+tool.action->toolTip();
      bool matches=true;for(const auto& word:words)if(!haystack.contains(word,Qt::CaseInsensitive)){matches=false;break;}if(!matches)continue;
      auto item=new QListWidgetItem(tool.action->text()+(tool.path.isEmpty()?QString():"\n"+tool.path),list);item->setData(Qt::UserRole,i);item->setSizeHint({0,tool.path.isEmpty()?36:56});
      item->setToolTip(tool.action->isEnabled()?tool.action->toolTip():QObject::tr("현재 상태에서는 사용할 수 없는 도구입니다."));
      if(!tool.action->isEnabled())item->setFlags(item->flags()&~Qt::ItemIsEnabled&~Qt::ItemIsSelectable);
    }
    for(int i=0;i<list->count();++i)if(list->item(i)->flags()&Qt::ItemIsEnabled){list->setCurrentRow(i);break;}
    status->setText(list->count()?QObject::tr("%1개 도구 · Enter로 열기 · Esc로 닫기").arg(list->count()):QObject::tr("일치하는 도구가 없습니다. 다른 작업 이름을 입력하세요."));open->setEnabled(canOpen());
  };
  QObject::connect(search,&QLineEdit::textChanged,&dialog,filter);
  QObject::connect(list,&QListWidget::currentRowChanged,&dialog,[&]{open->setEnabled(canOpen());});
  QObject::connect(list,&QListWidget::itemActivated,&dialog,[&]{if(canOpen())dialog.accept();});
  QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{if(canOpen())dialog.accept();});
  QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  filter();search->setFocus();
  if(dialog.exec()==QDialog::Accepted&&canOpen()){
    const auto action=tools[list->currentItem()->data(Qt::UserRole).toInt()].action;
    if(action&&action->isEnabled())action->trigger();
  }
}

inline void organizeAuthoringTools(QMenu* menu,QWidget* owner) {
  const auto original=menu->actions();
  auto group=[&](const QString& title,const QString& name,const QStringList& ids){
    auto sub=menu->addMenu(title);sub->setObjectName(name);
    for(const auto& id:ids)for(auto* action:original)if(action->objectName()==id){menu->removeAction(action);sub->addAction(action);break;}
  };
  group(QObject::tr("인물 · 성장"),"authoringCharacters",{"editRoster","editCharacterState","editTimeSessions","editCustomStates","editUserFields"});
  group(QObject::tr("이야기 · 규칙"),"authoringStory",{"scriptAuthoringAction","editRules","editLogicDocument","editWorkflows","editRuleExtensions"});
  group(QObject::tr("화면 · 연출"),"authoringPresentation",{"editUiComposition","editComponents","editDataUi","applyTextBindingBatch","editDirection","editTimelines","importSprites","editDesignSystem","importFont","editLocalization"});
  group(QObject::tr("탐험 · 전투 · 거래"),"authoringAdventure",{"editTactics","editBattleHud","editExpeditions","editFacilities"});
  group(QObject::tr("검증 · 회귀"),"authoringVerification",{"runAssurance","goalRoutes","goalImpact","goalCiOpen","showRuleDebugger"});
  group(QObject::tr("시작 구성 · 샘플"),"authoringStarters",{"createBenchmark"});
  auto find=new QAction(QObject::tr("제작 도구 찾기…"),owner);find->setObjectName("findAuthoringTool");find->setShortcut(QKeySequence("Ctrl+K"));
  QObject::connect(find,&QAction::triggered,owner,[owner,menu]{showAuthoringToolSearch(owner,menu);});
  menu->insertAction(menu->actions().value(0),find);menu->insertSeparator(menu->actions().value(1));
}
