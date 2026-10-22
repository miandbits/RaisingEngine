#pragma once
#include <QApplication>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QTextEdit>

inline bool ownsEditorInput(QWidget *root, QObject *object) {
  for (auto current = object; current; current = current->parent())
    if (current == root) return true;
  return false;
}

// Preedit belongs to the input method, never to project undo history.
class EditorInputGuard final : public QObject {
public:
  explicit EditorInputGuard(QWidget *root) : QObject(root), root_(root) {
    setObjectName("editorInputGuard"); qApp->installEventFilter(this);
  }
  ~EditorInputGuard() override { qApp->removeEventFilter(this); }
protected:
  bool eventFilter(QObject *object, QEvent *event) override {
    auto widget = qobject_cast<QWidget *>(object);
    if (!widget || !ownsEditorInput(root_, widget)) return false;
    if (!qobject_cast<QLineEdit *>(widget) && !qobject_cast<QTextEdit *>(widget) &&
        !qobject_cast<QPlainTextEdit *>(widget)) return false;
    if (event->type() == QEvent::InputMethod) {
      widget->setProperty("raisingInputComposing", !static_cast<QInputMethodEvent *>(event)->preeditString().isEmpty());
    } else if (event->type() == QEvent::FocusOut) {
      widget->setProperty("raisingInputComposing", false);
    } else if (widget->property("raisingInputComposing").toBool() &&
               (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
      auto key = static_cast<QKeyEvent *>(event);
      if (key->key() != Qt::Key_Escape && !key->matches(QKeySequence::Undo)) return false;
      if (event->type() == QEvent::KeyPress) {
        QGuiApplication::inputMethod()->reset();
        QInputMethodEvent cancel; QApplication::sendEvent(widget, &cancel);
      }
      event->accept(); return true;
    }
    return false;
  }
private:
  QWidget *root_;
};

inline bool commitEditorInput(QWidget *root) {
  QPointer<QWidget> focus = QApplication::focusWidget();
  if (!focus || !ownsEditorInput(root, focus)) return true;
  QGuiApplication::inputMethod()->commit();
  // A platform may refuse to commit a candidate. Do not save or switch away
  // while it still reports preedit, and never manufacture committed text.
  return !focus || !focus->property("raisingInputComposing").toBool();
}
