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
#include <memory>
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
  }, std::function<bool()> deferPlatformCommit = [] {
#ifdef Q_OS_WIN
    return (GetKeyState(VK_ESCAPE) & 0x8000) || (GetKeyState(VK_CONTROL) & 0x8000);
#else
    return false;
#endif
  }) : QObject(root), root_(root), resetInputMethod_(std::move(resetInputMethod)),
       deferPlatformCommit_(std::move(deferPlatformCommit)) {
    setObjectName("editorInputGuard"); qApp->installEventFilter(this);
    qApp->installNativeEventFilter(this);
  }
  ~EditorInputGuard() override { qApp->removeNativeEventFilter(this); qApp->removeEventFilter(this); }
  void commitInput() {
    flushPendingCommit();
    QScopedValueRollback<bool> delivering(deliveringPending_, true);
    QGuiApplication::inputMethod()->commit();
  }
  bool nativeEventFilter(const QByteArray &, void *message, qintptr *result) override {
#ifdef Q_OS_WIN
    auto msg = static_cast<MSG *>(message);
    auto widget = QApplication::focusWidget();
    if (!widget || !ownsEditorInput(root_, widget)) return false;
    if (msg->message == WM_KEYUP && msg->wParam == VK_CONTROL) flushPendingCommit();
    if (msg->message != WM_KEYDOWN && msg->message != WM_SYSKEYDOWN && msg->message != WM_IME_KEYDOWN)
      return false;
    auto key = msg->wParam == VK_PROCESSKEY ? ImmGetVirtualKey(msg->hwnd) : UINT(msg->wParam);
    const bool undo = key == 'Z' && (GetKeyState(VK_CONTROL) & 0x8000) &&
                      !(GetKeyState(VK_SHIFT) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000);
    if (qEnvironmentVariableIsSet("RAISING_IME_TRACE"))
      qInfo() << "IME native" << msg->message << key << widget->objectName()
              << widget->property("raisingInputComposing").toBool() << "hasResult" << bool(result);
    if ((key == VK_ESCAPE || undo) && widget->property("raisingInputComposing").toBool()) {
      cancelPreedit(widget);
      // Qt's dispatcher can filter a queued message without a result slot.
      if (result) *result = 0;
      return true;
    }
    if (key != VK_CONTROL && key != VK_SHIFT && key != VK_MENU) flushPendingCommit();
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
      auto input = static_cast<QInputMethodEvent *>(event);
      if (qEnvironmentVariableIsSet("RAISING_IME_TRACE"))
        qInfo() << "IME event" << widget->objectName()
                << input->preeditString().size() << input->commitString().size()
                << "defer" << deferPlatformCommit_();
      // Microsoft IME can finish composition BEFORE Escape/Z reaches Qt.
      // Hold that final commit until the key is known, so it never enters the
      // widget undo stack when the user's intent is to cancel composition.
      if (!deliveringPending_ && widget->property("raisingInputComposing").toBool() &&
          input->preeditString().isEmpty() && !input->commitString().isEmpty() && deferPlatformCommit_()) {
        pendingTarget_ = widget;
        pendingCommit_ = std::make_unique<QInputMethodEvent>(input->preeditString(), input->attributes());
        pendingCommit_->setCommitString(input->commitString(), input->replacementStart(), input->replacementLength());
        return true;
      }
      if (!deliveringPending_ && pendingCommit_ && input->preeditString().isEmpty() && input->commitString().isEmpty())
        return true;
      if (!deliveringPending_ && pendingCommit_) flushPendingCommit();
      widget->setProperty("raisingInputComposing", !input->preeditString().isEmpty());
    } else if (event->type() == QEvent::FocusOut) {
      flushPendingCommit();
      widget->setProperty("raisingInputComposing", false);
    } else if (widget->property("raisingInputComposing").toBool() &&
               (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
      auto key = static_cast<QKeyEvent *>(event);
      if (key->key() != Qt::Key_Escape && !key->matches(QKeySequence::Undo)) {
        if (key->key() != Qt::Key_Control && key->key() != Qt::Key_Shift && key->key() != Qt::Key_Alt)
          flushPendingCommit();
        return false;
      }
      if (event->type() == QEvent::KeyPress) {
        cancelPreedit(widget);
      }
      event->accept(); return true;
    }
    if (pendingCommit_ && event->type() == QEvent::KeyRelease &&
        static_cast<QKeyEvent *>(event)->key() == Qt::Key_Control) flushPendingCommit();
    return false;
  }
private:
  void cancelPreedit(QPointer<QWidget> widget) {
    pendingCommit_.reset(); pendingTarget_.clear();
    {
      QScopedValueRollback<QPointer<QWidget>> suppress(cancelTarget_, widget);
      resetInputMethod_();
    }
    if (widget) { QInputMethodEvent cancel; QApplication::sendEvent(widget, &cancel); }
  }
  void flushPendingCommit() {
    if (!pendingCommit_) return;
    auto target = pendingTarget_;
    auto commit = std::move(pendingCommit_);
    pendingTarget_.clear();
    QScopedValueRollback<bool> delivering(deliveringPending_, true);
    if (target) QApplication::sendEvent(target, commit.get());
  }
  QWidget *root_;
  std::function<void()> resetInputMethod_;
  std::function<bool()> deferPlatformCommit_;
  QPointer<QWidget> cancelTarget_;
  QPointer<QWidget> pendingTarget_;
  std::unique_ptr<QInputMethodEvent> pendingCommit_;
  bool deliveringPending_ = false;
};

inline bool commitEditorInput(QWidget *root) {
  QPointer<QWidget> focus = QApplication::focusWidget();
  if (!focus || !ownsEditorInput(root, focus)) return true;
  auto guard = dynamic_cast<EditorInputGuard *>(root->findChild<QObject *>("editorInputGuard", Qt::FindDirectChildrenOnly));
  if (guard) guard->commitInput();
  else QGuiApplication::inputMethod()->commit();
  // A platform may refuse to commit a candidate. Do not save or switch away
  // while it still reports preedit, and never manufacture committed text.
  return !focus || !focus->property("raisingInputComposing").toBool();
}
