#pragma once
#include <QApplication>
#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>
#include <QPointer>
#include <algorithm>

// Keep panel navigation available even when a dock's tab or toolbar is clipped.
inline void installPanelNavigation(QMainWindow *window, QMenu *viewMenu) {
  auto panels = [window] {
    QList<QWidget *> result;
    if (window->centralWidget() && window->centralWidget()->isVisible())
      result.append(window->centralWidget());
    auto docks = window->findChildren<QDockWidget *>(QString(), Qt::FindDirectChildrenOnly);
    // raise() changes QObject child order. Navigation order must not change with it.
    std::sort(docks.begin(), docks.end(), [](QDockWidget *a, QDockWidget *b) {
      return a->objectName() < b->objectName();
    });
    for (auto dock : docks)
      if (!dock->isHidden()) result.append(dock);
    return result;
  };
  auto focusPanel = [](QWidget *panel) {
    if (!panel) return;
    panel->show();
    if (auto dock = qobject_cast<QDockWidget *>(panel)) dock->raise();
    if (auto previous = panel->focusWidget(); previous && previous->isVisible() && previous->isEnabled()) {
      previous->setFocus(Qt::ShortcutFocusReason);
      return;
    }
    for (auto child : panel->findChildren<QWidget *>()) {
      if (child->isVisible() && child->isEnabled() && (child->focusPolicy() & Qt::TabFocus)) {
        child->setFocus(Qt::ShortcutFocusReason);
        return;
      }
    }
    // A read-only status panel still needs a focus stop, otherwise F6 gets stuck.
    panel->setFocusPolicy(Qt::StrongFocus);
    panel->setFocus(Qt::ShortcutFocusReason);
  };
  auto cycle = [panels, focusPanel](int direction) {
    const auto list = panels();
    if (list.isEmpty()) return;
    auto current = QApplication::focusWidget();
    int index = -1;
    for (int i = 0; i < list.size(); ++i)
      if (list[i] == current || list[i]->isAncestorOf(current)) { index = i; break; }
    if (index < 0) focusPanel(direction > 0 ? list.first() : list.last());
    else focusPanel(list[(index + direction + list.size()) % list.size()]);
  };
  viewMenu->addAction(QMainWindow::tr("다음 패널"), QKeySequence(Qt::Key_F6), window,
    [cycle] { cycle(1); })->setObjectName("focusNextPanel");
  viewMenu->addAction(QMainWindow::tr("이전 패널"), QKeySequence(Qt::SHIFT | Qt::Key_F6), window,
    [cycle] { cycle(-1); })->setObjectName("focusPreviousPanel");
  auto menu = viewMenu->addMenu(QMainWindow::tr("패널로 이동"));
  menu->setObjectName("panelNavigationMenu");
  QObject::connect(menu, &QMenu::aboutToShow, window, [window, menu, focusPanel] {
    menu->clear();
    for (auto dock : window->findChildren<QDockWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
      QPointer<QDockWidget> target(dock);
      menu->addAction(dock->windowTitle(), window, [target, focusPanel] {
        if (target) focusPanel(target);
      });
    }
  });
}
