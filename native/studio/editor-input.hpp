#pragma once
#include <QApplication>
#include <QAbstractNativeEventFilter>
#include <QDebug>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScopedValueRollback>
#include <QTextEdit>
#include <functional>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <imm.h>
#endif

inline bool ownsEditorInput(QWidget *root, QObject *object) {
  for (auto current = object; current; current = current->parent())
    if (current == root) return true;
  return false;
}

// Preedit belongs to the input method, never to project undo history.
class EditorInputGuard final : public QObject, public QAbstractNativeEventFilter {
public:
  explicit EditorInputGuard(QWidget *root, std::function<void()> resetInputMethod = [] {
    QGuiApplication::inputMethod()->reset();
  }) : QObject(root), root_(root), resetInputMethod_(std::move(resetInputMethod)) {
    setObjectName("editorInputGuard"); qApp->installEventFilter(this);
    qApp->installNativeEventFilter(this);
  }
  ~EditorInputGuard() override { qApp->removeNativeEventFilter(this); qApp->removeEventFilter(this); }
  bool nativeEventFilter(const QByteArray &, void *message, qintptr *result) override {
#ifdef Q_OS_WIN
    auto msg = static_cast<MSG *>(message);
    auto widget = QApplication::focusWidget();
    if (!widget || !ownsEditorInput(root_, widget)) return false;
    if (msg->message != WM_KEYDOWN && msg->message != WM_SYSKEYDOWN && msg->message != WM_IME_KEYDOWN)
      return false;
    auto key = msg->wParam == VK_PROCESSKEY ? ImmGetVirtualKey(msg->hwnd) : UINT(msg->wParam);
    const bool undo = key == 'Z' && (GetKeyState(VK_CONTROL) & 0x8000) &&
                      !(GetKeyState(VK_SHIFT) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000);
    if (qEnvironmentVariableIsSet("RAISING_IME_TRACE"))
      qInfo() << "IME native" << msg->message << key << widget->objectName()
              << widget->property("raisingInputComposing").toBool();
    if ((key == VK_ESCAPE || undo) && widget->property("raisingInputComposing").toBool()) {
      cancelPreedit(widget);
      *result = 0;
      return true;
    }
#else
    Q_UNUSED(message); Q_UNUSED(result);
#endif
    return false;
  }
protected:
  bool eventFilter(QObject *object, QEvent *event) override {
    auto widget = qobject_cast<QWidget *>(object);
    if (!widget || !ownsEditorInput(root_, widget)) return false;
    if (!qobject_cast<QLineEdit *>(widget) && !qobject_cast<QTextEdit *>(widget) &&
        !qobject_cast<QPlainTextEdit *>(widget)) return false;
    if (event->type() == QEvent::InputMethod) {
      // Qt's Windows reset() can emit a commit before cancelling the native IME.
      // That commit must not enter the text widget's contents or undo history.
      if (cancelTarget_ == widget) return true;
      if (qEnvironmentVariableIsSet("RAISING_IME_TRACE"))
        qInfo() << "IME event" << widget->objectName()
                << static_cast<QInputMethodEvent *>(event)->preeditString().size()
                << static_cast<QInputMethodEvent *>(event)->commitString().size();
      widget->setProperty("raisingInputComposing", !static_cast<QInputMethodEvent *>(event)->preeditString().isEmpty());
    } else if (event->type() == QEvent::FocusOut) {
      widget->setProperty("raisingInputComposing", false);
    } else if (widget->property("raisingInputComposing").toBool() &&
               (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
      auto key = static_cast<QKeyEvent *>(event);
      if (key->key() != Qt::Key_Escape && !key->matches(QKeySequence::Undo)) return false;
      if (event->type() == QEvent::KeyPress) {
        cancelPreedit(widget);
      }
      event->accept(); return true;
    }
    return false;
  }
private:
  void cancelPreedit(QPointer<QWidget> widget) {
    {
      QScopedValueRollback<QPointer<QWidget>> suppress(cancelTarget_, widget);
      resetInputMethod_();
    }
    if (widget) { QInputMethodEvent cancel; QApplication::sendEvent(widget, &cancel); }
  }
  QWidget *root_;
  std::function<void()> resetInputMethod_;
  QPointer<QWidget> cancelTarget_;
};

inline bool commitEditorInput(QWidget *root) {
  QPointer<QWidget> focus = QApplication::focusWidget();
  if (!focus || !ownsEditorInput(root, focus)) return true;
  QGuiApplication::inputMethod()->commit();
  // A platform may refuse to commit a candidate. Do not save or switch away
  // while it still reports preedit, and never manufacture committed text.
  return !focus || !focus->property("raisingInputComposing").toBool();
}
