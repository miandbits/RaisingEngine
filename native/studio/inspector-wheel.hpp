#pragma once

#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QPointer>
#include <QScrollArea>
#include <QWheelEvent>

// Scrolling a property form must not edit whichever value is under the pointer.
// Forward to the viewport explicitly: swallowing an event alone loses scrolling.
class InspectorWheelGuard final : public QObject {
public:
  InspectorWheelGuard(QWidget *form, QScrollArea *area)
      : QObject(form), scroll(area) {
    for (auto widget : form->findChildren<QWidget *>()) {
      if (!qobject_cast<QAbstractSpinBox *>(widget) &&
          !qobject_cast<QComboBox *>(widget))
        continue;
      widget->installEventFilter(this);
      // Spin boxes and editable combos may receive wheel events at their editor.
      for (auto editor : widget->findChildren<QLineEdit *>())
        editor->installEventFilter(this);
    }
  }

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    if (event->type() != QEvent::Wheel || !scroll)
      return QObject::eventFilter(watched, event);
    auto wheel = static_cast<QWheelEvent *>(event);
    auto viewport = scroll->viewport();
    QWheelEvent forwarded(viewport->mapFromGlobal(wheel->globalPosition()),
                          wheel->globalPosition(), wheel->pixelDelta(),
                          wheel->angleDelta(), wheel->buttons(),
                          wheel->modifiers(), wheel->phase(), wheel->inverted(),
                          wheel->source(), wheel->pointingDevice());
    QApplication::sendEvent(viewport, &forwarded);
    wheel->setAccepted(forwarded.isAccepted());
    return true;
  }

private:
  QPointer<QScrollArea> scroll;
};
