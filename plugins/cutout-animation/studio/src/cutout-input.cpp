// Workspace input (plugin 1.21.0), all editor-only: picking locks (the hierarchy's 잠금 column,
// never stored, like the 보기 eye), value fields that select their text when focused (typing
// "600" into a 1000 field used to give 61000), and K (키 추가) from anywhere in the workspace, not
// only from the canvas, whose click changes the selection.
#include "cutout-panel.hpp"
#include <QtWidgets>

namespace cutout_studio {
namespace {
QString refKind(const QString& ref) { return ref.section(':', 0, 0); }
}  // namespace

/**
 * App-wide filter (it acts on this panel's widgets only, and goes away with the panel):
 *   * FocusIn of a spin box (or its line edit) by a mouse click or Tab: the whole text is selected
 *     once the press has placed the cursor (deferred), so typing replaces the value. Later clicks
 *     in a focused field place the cursor as usual (no FocusIn).
 *   * K / Shift+K pressed in a widget of the panel that kept the key from the workspace shortcut:
 *     a spin box commits what was typed (editingFinished) and then keys; a free-text field (ids,
 *     names, filters) types it; anything else (tree, lists, buttons, canvas) keys.
 */
class InputGuard : public QObject {
 public:
  explicit InputGuard(CutoutPanel* panel) : QObject(panel), panel_(panel) {}
  bool eventFilter(QObject* watched, QEvent* event) override {
    const QEvent::Type type = event->type();
    if ((type != QEvent::FocusIn && type != QEvent::KeyPress) || !watched->isWidgetType()) return false;
    auto* w = static_cast<QWidget*>(watched);
    if (!panel_->isAncestorOf(w)) return false;  // other windows (dialogs) and the host keep their keys
    QAbstractSpinBox* spin = spinOf(w);
    if (type == QEvent::FocusIn) {
      const Qt::FocusReason reason = static_cast<QFocusEvent*>(event)->reason();
      if (spin && (reason == Qt::MouseFocusReason || reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason))
        QTimer::singleShot(0, spin, [s = QPointer<QAbstractSpinBox>(spin)] { if (s && s->hasFocus()) s->selectAll(); });
      return false;
    }
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() != Qt::Key_K || (key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) || key->isAutoRepeat()) return false;
    // Once per press: the focus widget sees it first, a parent only if it was passed on.
    if (w != QApplication::focusWidget()) return false;
    if (spin) {
      spin->interpretText();
      Q_EMIT spin->editingFinished();  // the field's own commit (property, duration, ...) first
    } else if (qobject_cast<QLineEdit*>(w) || qobject_cast<QTextEdit*>(w) || qobject_cast<QPlainTextEdit*>(w)) {
      return false;  // free text: K is a letter
    }
    panel_->keySelected();
    return true;
  }

 private:
  /** The spin box of `w` (itself or its line edit) inside the panel. */
  QAbstractSpinBox* spinOf(QWidget* w) const {
    for (int depth = 0; w && w != panel_ && depth < 2; w = w->parentWidget(), depth++)
      if (auto* s = qobject_cast<QAbstractSpinBox*>(w)) return s;
    return nullptr;
  }
  CutoutPanel* panel_;
};

void CutoutPanel::installInputGuard() { qApp->installEventFilter(new InputGuard(this)); }

// ---------------------------------------------------------------- picking locks
void CutoutPanel::setLocked(const QString& ref, bool on) {
  const QString kind = refKind(ref);
  if (kind != "bone" && kind != "slot") return;
  if (on == locked_.contains(ref)) return;
  if (on) locked_.insert(ref); else locked_.remove(ref);
  // The tree's check box follows (a call from the menu / palette / automation, not a click on it).
  if (treeBase_) {
    const bool keep = filling_;
    filling_ = true;
    for (QTreeWidgetItemIterator it(treeBase_); *it; ++it)
      if ((*it)->data(0, Qt::UserRole).toString() == ref && ((*it)->checkState(2) == Qt::Checked) != on) (*it)->setCheckState(2, on ? Qt::Checked : Qt::Unchecked);
    filling_ = keep;
  }
  updateCanvas();
  setStatus(on ? QString("「%1」을 잠갔습니다: 캔버스에서 골라지지 않습니다(계층에서는 고름, 저장되지 않음).").arg(ref.section(':', 1))
               : QString("「%1」의 잠금을 풀었습니다.").arg(ref.section(':', 1)));
}
QStringList CutoutPanel::lockedItems() const {
  QStringList out(locked_.begin(), locked_.end());
  out.sort();
  return out;
}
bool CutoutPanel::lockSelected() {
  const QStringList refs = selectedItems();
  if (refs.isEmpty()) { setStatus("잠글 본이나 조각을 계층에서 먼저 고르세요.", true); return false; }
  bool all = true;
  for (const QString& r : refs) all = all && locked_.contains(r);
  for (const QString& r : refs) setLocked(r, !all);
  setStatus(all ? QString("고른 %1개의 잠금을 풀었습니다.").arg(refs.size())
                : QString("고른 %1개를 잠갔습니다: 캔버스에서 골라지지 않습니다(계층의 잠금 칸·「잠금 모두 풀기」로 풂).").arg(refs.size()));
  return true;
}
void CutoutPanel::unlockAll() {
  const QStringList refs = lockedItems();
  for (const QString& r : refs) setLocked(r, false);
  setStatus(refs.isEmpty() ? QString("잠긴 항목이 없습니다.") : QString("잠금 %1개를 모두 풀었습니다.").arg(refs.size()));
}
}  // namespace cutout_studio
