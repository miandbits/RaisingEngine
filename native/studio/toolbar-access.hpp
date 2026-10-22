#pragma once

#include <QComboBox>
#include <QMenuBar>
#include <QToolBar>
#include <QToolButton>
#include <QWidgetAction>

// Keep a discoverable entry point even when Qt's small overflow chevron is
// clipped or hard to find. Reuse actions so enabled/check states stay in sync.
inline void installToolbarAccess(QToolBar* toolbar, QMenuBar* menus, QAction* search) {
  auto menu = menus->addMenu(QObject::tr("제작 도구"));
  menu->setObjectName("toolbarAccessMenu");
  auto button = new QToolButton(toolbar);
  button->setObjectName("toolbarAccessButton");
  button->setText(QObject::tr("제작 도구"));
  button->setToolTip(QObject::tr("창이 좁아 숨겨진 도구도 여기에서 모두 열 수 있습니다."));
  button->setPopupMode(QToolButton::InstantPopup);
  button->setMenu(menu);
  auto entry = toolbar->addWidget(button);
  entry->setObjectName("toolbarAccessEntry");
  QObject::connect(menu, &QMenu::aboutToShow, menu, [toolbar, menu, entry, search] {
    // clear() removes submenu actions but leaves their parent-owned menus
    // alive. Retire those generated menus before rebuilding current controls.
    qDeleteAll(menu->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly));
    menu->clear();
    if (search) { menu->addAction(search); menu->addSeparator(); }
    for (auto action : toolbar->actions()) {
      if (action == entry || !action->isVisible()) continue;
      auto widgetAction = qobject_cast<QWidgetAction*>(action);
      auto widget = widgetAction ? widgetAction->defaultWidget() : nullptr;
      if (auto combo = qobject_cast<QComboBox*>(widget)) {
        auto choices = menu->addMenu(combo->accessibleName().isEmpty() ? combo->toolTip() : combo->accessibleName());
        choices->setObjectName(combo->objectName() + "AccessMenu");
        choices->setEnabled(action->isEnabled() && combo->isEnabled());
        for (int i = 0; i < combo->count(); ++i) {
          auto choice = choices->addAction(combo->itemText(i));
          choice->setCheckable(true);
          choice->setChecked(i == combo->currentIndex());
          choice->setEnabled(combo->model()->flags(combo->model()->index(i, combo->modelColumn(), combo->rootModelIndex())).testFlag(Qt::ItemIsEnabled));
          QObject::connect(choice, &QAction::triggered, combo, [combo, i] {
            combo->setCurrentIndex(i);
            emit combo->activated(i);
          });
        }
      } else if (auto tool = qobject_cast<QToolButton*>(widget); tool && tool->menu()) {
        auto submenu = menu->addMenu(tool->text());
        submenu->setObjectName(tool->objectName() + "AccessMenu");
        submenu->setEnabled(action->isEnabled() && tool->isEnabled());
        submenu->addActions(tool->menu()->actions());
      } else if (!widgetAction) {
        menu->addAction(action);
      }
    }
  });
}
