#pragma once
#include "editor-input.hpp"
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QKeySequence>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMainWindow>
#include <QMenuBar>
#include <QComboBox>
#include <QToolBar>
#include <QToolButton>
#include <QWidgetAction>
#include <QSet>
#include <QFontMetrics>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>
#include <functional>

class ToolSearchNavigation final : public QObject {
public:
  ToolSearchNavigation(QLineEdit *search, QListWidget *list, QObject *parent)
      : QObject(parent), search_(search), list_(list) { search->installEventFilter(this); }
protected:
  bool eventFilter(QObject *, QEvent *event) override {
    if (event->type() != QEvent::KeyPress || search_->property("raisingInputComposing").toBool()) return false;
    const auto key = static_cast<QKeyEvent *>(event);
    if (key->key() != Qt::Key_Down && key->key() != Qt::Key_Up) return false;
    const int direction = key->key() == Qt::Key_Down ? 1 : -1;
    for (int row = list_->currentRow() + direction; row >= 0 && row < list_->count(); row += direction)
      if (list_->item(row)->flags() & Qt::ItemIsEnabled) {
        list_->setCurrentRow(row); list_->scrollToItem(list_->item(row)); break;
      }
    return true;
  }
private:
  QLineEdit *search_; QListWidget *list_;
};

// Search the same live actions used by menus and toolbars. Never duplicate the
// command handler: its save, validation, and unsaved-draft guards still apply.
inline void showAuthoringToolSearch(QWidget* parent,QMenu* menu) {
  if (!commitEditorInput(parent)) return;
  QPointer<QWidget> previousFocus = QApplication::focusWidget();
  QDialog dialog(parent);dialog.setObjectName("authoringToolSearch");dialog.setWindowTitle(QObject::tr("명령 · 도구 찾기"));dialog.resize(700,520);
  struct Tool { QPointer<QAction> action; QString path; std::function<bool()> available; };
  QList<Tool> tools;
  QSet<QAction*> seen;
  QSet<QAction*> observed;
  using Guards = QList<QPointer<QAction>>;
  auto label=[](QString value) { value.replace("&&", "\x01");value.remove('&');return value.replace("\x01", "&"); };
  auto available=[](const Guards& guards) {
    for (const auto& guard:guards) if (!guard || !guard->isEnabled() || !guard->isVisible()) return false;
    return true;
  };
  auto append=[&](QAction* action,const QString& path,Guards guards) {
    if(!action||seen.contains(action)||action->isSeparator()||!action->isVisible()||action->objectName()=="findAuthoringTool")return;
    seen.insert(action);guards.push_back(action);for(const auto& guard:guards)if(guard)observed.insert(guard);
    tools.push_back({action,path,[guards,available]{return available(guards);}});
  };
  std::function<void(QMenu*,QString,Guards)> collect=[&](QMenu* branch,QString path,Guards guards){
    // This generated mirror is already represented by the original toolbars.
    if(!branch||branch->objectName()=="toolbarAccessMenu")return;
    guards.push_back(branch->menuAction());
    for(auto* action:branch->actions()){
      if(action->isSeparator()||!action->isVisible())continue;
      if(action->menu())collect(action->menu(),path.isEmpty()?label(action->text()):path+" › "+label(action->text()),guards);
      else append(action,path,guards);
    }
  };
  // Keep authoring tasks first, then add File, View, screen and workspace actions.
  collect(menu, qobject_cast<QMainWindow*>(parent)?label(menu->title()):QString(), {});
  if(auto window=qobject_cast<QMainWindow*>(parent)) {
    for(auto action:window->menuBar()->actions())if(action->menu()&&action->isVisible())collect(action->menu(),label(action->text()),{});
    for(auto toolbar:window->findChildren<QToolBar*>()) {
      // Hidden workspace toolbars must not expose commands for another context.
      if(!toolbar->isVisibleTo(parent))continue;
      auto path=label(toolbar->windowTitle());if(path.isEmpty())path=QObject::tr("화면 도구");
      for(auto action:toolbar->actions()) {
        if(!action->isVisible()||action->isSeparator())continue;
        auto widgetAction=qobject_cast<QWidgetAction*>(action);
        auto widget=widgetAction?widgetAction->defaultWidget():nullptr;
        if(auto combo=qobject_cast<QComboBox*>(widget)) {
          auto name=combo->accessibleName();if(name.isEmpty())name=combo->toolTip();if(name.isEmpty())name=path;
          const auto choicesPath=path+" › "+name;
          for(int index=0;index<combo->count();++index) {
            auto choice=new QAction(combo->itemText(index),&dialog);
            choice->setObjectName(combo->objectName()+":"+QString::number(index));
            choice->setCheckable(true);choice->setChecked(index==combo->currentIndex());
            QPointer<QComboBox> target=combo;QPointer<QAction> source=action;
            const auto text=combo->itemText(index);const auto data=combo->itemData(index);
            auto ready=[target,source,index,text,data]{return target&&source&&source->isEnabled()&&source->isVisible()&&target->isEnabled()&&index<target->count()&&target->itemText(index)==text&&target->itemData(index)==data&&target->model()->flags(target->model()->index(index,target->modelColumn(),target->rootModelIndex())).testFlag(Qt::ItemIsEnabled);};
            QObject::connect(choice,&QAction::triggered,&dialog,[target,index,ready]{if(ready()){target->setCurrentIndex(index);emit target->activated(index);}});
            tools.push_back({choice,choicesPath,ready});observed.insert(action);
          }
        } else if(auto button=qobject_cast<QToolButton*>(widget);button&&button->menu()) {
          const auto start=tools.size();collect(button->menu(),path+" › "+label(button->text()),{action});
          QPointer<QToolButton> target=button;
          for(int i=start;i<tools.size();++i){const auto ready=tools[i].available;tools[i].available=[ready,target]{return ready()&&target&&target->isEnabled();};}
        } else if(!widgetAction) {
          const auto start=tools.size();append(action,path,{});QPointer<QToolBar> target=toolbar;
          for(int i=start;i<tools.size();++i){const auto ready=tools[i].available;tools[i].available=[ready,target]{return ready()&&target&&target->isEnabled();};}
        }
      }
    }
  }
  // Inspector tools remain discoverable when collapsed or scrolled out of view.
  for(const auto& id:{"dataBindingPicker","presentationVariants","elementVisibleCondition","selectionBatch","componentSource"}) {
    auto button=parent->findChild<QPushButton*>(id);if(!button)continue;
    auto action=new QAction(button->text(),&dialog);action->setObjectName(id);action->setToolTip(button->toolTip());
    QPointer<QPushButton> target=button;
    QObject::connect(action,&QAction::triggered,&dialog,[target]{if(target&&target->isEnabled())target->click();});
    tools.push_back({action,QObject::tr("선택 요소"),[target]{return target&&target->isEnabled();}});
  }
  auto layout=new QVBoxLayout(&dialog);
  auto search=new QLineEdit;search->setObjectName("authoringToolQuery");search->setPlaceholderText(QObject::tr("저장, 내보내기, 플레이, 인물처럼 작업 이름으로 찾으세요"));search->setAccessibleName(QObject::tr("명령 · 도구 찾기"));layout->addWidget(search);
  auto list=new QListWidget;list->setObjectName("authoringToolResults");list->setAccessibleName(QObject::tr("검색 결과"));layout->addWidget(list);
  new ToolSearchNavigation(search, list, &dialog);
  auto status=new QLabel;status->setObjectName("authoringToolCount");status->setWordWrap(true);layout->addWidget(status);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Open|QDialogButtonBox::Cancel);layout->addWidget(buttons);
  auto open=buttons->button(QDialogButtonBox::Open);open->setDefault(true);
  auto canOpen=[&]{auto item=list->currentItem();return item&&tools[item->data(Qt::UserRole).toInt()].available();};
  auto filter=[&]{
    QPointer<QAction> selected; if(auto item=list->currentItem())selected=tools[item->data(Qt::UserRole).toInt()].action;
    list->clear();const auto words=search->text().simplified().split(' ',Qt::SkipEmptyParts);
    for(int i=0;i<tools.size();++i){const auto& tool=tools[i];if(!tool.action)continue;
      const auto shortcut=tool.action->shortcut().toString(QKeySequence::NativeText);
      const auto title=label(tool.action->text());const auto haystack=title+" "+tool.path+" "+tool.action->toolTip()+" "+shortcut;
      bool matches=true;for(const auto& word:words)if(!haystack.contains(word,Qt::CaseInsensitive)){matches=false;break;}if(!matches)continue;
      const auto caption=title+(shortcut.isEmpty()?QString():"   "+shortcut);
      const auto details=tool.path+(tool.action->isCheckable()&&tool.action->isChecked()?" · "+QObject::tr("현재 선택"):QString());
      auto item=new QListWidgetItem(caption+(details.isEmpty()?QString():"\n"+details),list);item->setData(Qt::UserRole,i);item->setData(Qt::UserRole+1,tool.action->objectName());
      const QFontMetrics metrics(list->font());item->setSizeHint({0,metrics.height()*(details.isEmpty()?1:2)+16});
      const bool enabled=tool.available();item->setToolTip(enabled?tool.action->toolTip():QObject::tr("현재 상태에서는 사용할 수 없는 도구입니다."));
      if(!enabled)item->setFlags(item->flags()&~Qt::ItemIsEnabled&~Qt::ItemIsSelectable);
      else if(selected==tool.action)list->setCurrentItem(item);
    }
    if(!list->currentItem())for(int i=0;i<list->count();++i)if(list->item(i)->flags()&Qt::ItemIsEnabled){list->setCurrentRow(i);break;}
    status->setText(list->count()?QObject::tr("%1개 명령 · ↑↓로 선택 · Enter로 열기 · Esc로 닫기").arg(list->count()):QObject::tr("일치하는 도구가 없습니다. 다른 작업 이름을 입력하세요."));open->setEnabled(canOpen());
  };
  QObject::connect(search,&QLineEdit::textChanged,&dialog,filter);
  for(auto action:observed)QObject::connect(action,&QAction::changed,&dialog,filter);
  QObject::connect(list,&QListWidget::currentRowChanged,&dialog,[&]{open->setEnabled(canOpen());});
  QObject::connect(list,&QListWidget::itemActivated,&dialog,[&]{if(canOpen())dialog.accept();});
  QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{if(canOpen())dialog.accept();});
  QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  filter();search->setFocus();
  const bool accepted=dialog.exec()==QDialog::Accepted&&canOpen();
  const int selected=accepted?list->currentItem()->data(Qt::UserRole).toInt():-1;
  // Commands such as panel navigation use the invoking focus as their context.
  // Restore it before dispatch; a command that opens a dialog then owns focus.
  if(previousFocus&&previousFocus->isVisible()&&previousFocus->isEnabled()) {
    auto window=previousFocus->window();window->activateWindow();
    // Native activation can arrive after exec() returns. Set Qt's focus context
    // now so synchronous actions (F6/panel navigation) see the invoking panel.
    QApplication::setActiveWindow(window);previousFocus->setFocus(Qt::OtherFocusReason);
  }
  if(selected>=0&&tools[selected].action&&tools[selected].available())tools[selected].action->trigger();
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
  auto find=new QAction(QObject::tr("명령 · 도구 찾기…"),owner);find->setObjectName("findAuthoringTool");find->setShortcut(QKeySequence("Ctrl+K"));
  QObject::connect(find,&QAction::triggered,owner,[owner,menu]{showAuthoringToolSearch(owner,menu);});
  menu->insertAction(menu->actions().value(0),find);menu->insertSeparator(menu->actions().value(1));
}
