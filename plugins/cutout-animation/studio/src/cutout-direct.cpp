// Direct manipulation on the canvas (plugin 1.19.0): arrow-key nudges, 복제 (Ctrl+D), double-click
// part <-> bone, Esc stepping back, the right-click menu and the shortcut overlay. Keys and gestures
// only choose which existing gesture runs: every edit is still ONE core op batch through runOps
// (transformBones / setAttachment for a nudge, duplicateBones, reparentBone...), so the MCP/LLM
// `edit` op builds the same documents and undo has one step per gesture. Everything else here (the
// overlay, the menu, the nudge preview) is editor-only presentation.
#include "cutout-panel.hpp"
#include <QtWidgets>
#include <algorithm>
#include <memory>
#include "cutout/editing.hpp"

namespace cutout_studio {
using cutout::Json;
namespace {
QString refKind(const QString& ref) { return ref.section(':', 0, 0); }
QString refId(const QString& ref) { return ref.section(':', 1); }
Json batchOf(const std::vector<Json>& ops) { return ops.size() == 1 ? ops[0] : Json{{"op", "batch"}, {"operations", ops}}; }
const QStringList& groupOrder() {
  static const QStringList order = {"도구", "선택", "변형", "보기", "편집", "애니메이션", "메시", "도움말", "작업 공간"};
  return order;
}
}  // namespace

// ---- nudge: arrow presses within NUDGE_IDLE_MS are one gesture (auto-repeat included)
std::vector<Json> CutoutPanel::nudgeOps() const {
  if (!nudge_ || nudge_->total.isNull()) return {};
  RigCanvas::Drag d;
  d.ref = nudge_->ref;
  d.tool = RigCanvas::Move;
  d.current = nudge_->total;
  d.moved = true;
  // 숫자 축: world = rig px; local / parent = the parent's axes (a part: its bone's axes).
  d.parentSpace = axes_ != cutout::Axes::World;
  return dragOps(d);
}
bool CutoutPanel::nudgeSelection(const QPoint& step) {
  const QString kind = refKind(selection_);
  if (!rigDoc() || !rig_ || (kind != "bone" && kind != "slot")) { setStatus("화살표로 옮길 본이나 조각을 먼저 고르세요.", true); return false; }
  if (mode_ == Mode::Animate && clipId_.isEmpty()) { setStatus("키를 넣을 클립을 먼저 만드세요.", true); return false; }
  if (nudge_ && nudge_->ref != selection_) flushNudge();
  if (!nudge_) {
    if (dragActive_) return false;  // a mouse drag owns the preview right now
    nudge_ = Nudge{selection_, QPointF()};
    dragBase_ = editBase();
    dragRig_ = rig_;
    dragPose_ = currentPose(false);  // keyable values, as a drag uses
  }
  nudge_->total += QPointF(step);
  const std::vector<Json> ops = nudgeOps();
  dragPreview_.reset();
  if (!ops.empty()) {
    try { dragPreview_ = std::make_shared<const Json>(cutout::applyEdit(dragBase_, batchOf(ops)).data); }
    catch (const std::exception& e) { setStatus(qs(e.what()), true); }
  }
  rebuildDisplay(false);
  if (!nudgeTimer_) {
    nudgeTimer_ = new QTimer(this);
    nudgeTimer_->setSingleShot(true);
    connect(nudgeTimer_, &QTimer::timeout, this, [this] { flushNudge(); });
  }
  nudgeTimer_->start(NUDGE_IDLE_MS);
  const QString space = axes_ == cutout::Axes::World ? "월드" : axes_ == cutout::Axes::Parent ? "부모" : "로컬";
  setStatus(QString("화살표 이동 %1, %2 px (%3 축) · 키를 멈추면 한 번의 편집이 됩니다").arg(nudge_->total.x()).arg(nudge_->total.y()).arg(space));
  return true;
}
bool CutoutPanel::flushNudge() {
  if (!nudge_) return false;
  if (nudgeTimer_) nudgeTimer_->stop();
  const std::vector<Json> ops = nudgeOps();
  const QPointF total = nudge_->total;
  flushingNudge_ = true;
  nudge_.reset();
  dragPreview_.reset();
  bool ok = false;
  if (ops.empty()) rebuildDisplay(false);
  else ok = runOps(ops, mode_ == Mode::Animate);
  flushingNudge_ = false;
  if (ok && !working_) setStatus(QString("화살표로 %1, %2 px 옮겼습니다. 되돌리기: Ctrl+Z").arg(total.x()).arg(total.y()));
  return ok;
}

// ---- 복제
bool CutoutPanel::duplicateSelected() {
  if (!rigDoc()) { setStatus("먼저 캐릭터를 만드세요.", true); return false; }
  if (mode_ == Mode::Animate) { setStatus("복제는 조립 모드에서 합니다(기본 자세의 데이터).", true); return false; }
  const QStringList bones = selectedBones();
  QStringList slots;
  for (const QString& ref : selectedItems()) if (refKind(ref) == "slot") slots << refId(ref);
  if (bones.isEmpty() && slots.isEmpty()) { setStatus("복제할 본이나 조각을 고르세요.", true); return false; }
  Json op = {{"op", "duplicateBones"}};
  if (!bones.isEmpty()) { op["bones"] = Json::array(); for (const QString& b : bones) op["bones"].push_back(ss(b)); }
  if (!slots.isEmpty()) { op["slots"] = Json::array(); for (const QString& s : slots) op["slots"].push_back(ss(s)); }
  int boneCount = 0, slotCount = 0;
  const QString primary = selection_;
  const bool ok = runOps({op}, false, [&](const cutout::EditResult& r) {
    // The copies become the selection (primary = the primary item's copy) before the commit shows them.
    const Json bm = r.extra.value("bones", Json::object()), sm = r.extra.value("slots", Json::object());
    boneCount = int(bm.size());
    slotCount = int(sm.size());
    QStringList refs;
    QString first;
    for (const QString& ref : selectedItems()) {
      const Json& map = refKind(ref) == "bone" ? bm : sm;
      if (map.contains(ss(refId(ref)))) refs << refKind(ref) + ":" + qs(map.at(ss(refId(ref))).get<std::string>());
      if (ref == primary && !refs.isEmpty()) first = refs.back();
    }
    selected_ = refs;
    selection_ = first.isEmpty() ? refs.value(0) : first;
  });
  if (ok) setStatus(QString("본 %1개 · 조각 %2개를 복제했습니다(키·표정은 복제하지 않음). 되돌리기: Ctrl+Z").arg(boneCount).arg(slotCount));
  return ok;
}

// ---- double-click, Esc
void CutoutPanel::canvasDoubleClick(const QString& ref) {
  if (!rig_) return;
  if (refKind(ref) == "slot") {
    const int si = rig_->slotIndex(ss(refId(ref)));
    if (si < 0) return;
    const QString bone = qs(rig_->slots[size_t(si)].bone);
    selectItem("bone:" + bone);
    setStatus(QString("조각 「%1」의 본 「%2」를 골랐습니다 (더블클릭: 조각 ↔ 본).").arg(refId(ref), bone));
    return;
  }
  const int bi = rig_->boneIndex(ss(refId(ref)));
  if (bi < 0) return;
  for (const auto& s : rig_->slots)
    if (s.boneIndex == bi) {
      selectItem("slot:" + qs(s.id));
      setStatus(QString("본 「%1」의 첫 조각 「%2」를 골랐습니다 (더블클릭: 본 ↔ 조각).").arg(refId(ref), qs(s.id)));
      return;
    }
  selectItem(ref);
  setStatus(QString("본 「%1」에 붙은 조각이 없습니다.").arg(refId(ref)));
}
void CutoutPanel::canvasEscape() {
  if (meshEdit_ != MeshEdit::Off) { setMeshEdit(MeshEdit::Off); setStatus("메시 편집을 껐습니다(Esc). 다시 Esc = 선택 도구 · 선택 해제"); return; }
  if (boxEdit_) { setBoxVertexEdit(false); setStatus("판정 상자 정점 편집을 껐습니다(Esc)."); return; }
  if (tool() != RigCanvas::Move) { setTool(RigCanvas::Move); setStatus("선택·이동 도구 (Esc). 다시 Esc = 선택 해제"); return; }
  if (!selection_.isEmpty()) { selectItem(QString()); setStatus("선택을 해제했습니다(Esc)."); }
}
void CutoutPanel::toggleMeshHotkey(MeshEdit mode) {
  if (meshSlot().isEmpty()) {
    setStatus(QString("%1: 메시인 조각을 먼저 고르세요(「메시로 변환」).").arg(mode == MeshEdit::Vertices ? "M 메시 정점" : "W 웨이트 칠하기"), true);
    return;
  }
  setMeshEdit(meshEdit_ == mode ? MeshEdit::Off : mode);
}

// ---- right-click menu: the existing commands for the item under the cursor
QMenu* CutoutPanel::canvasMenu(const QString& ref) {
  auto* menu = new QMenu(this);
  menu->setObjectName("cutoutCanvasMenu");
  const QString kind = refKind(ref), id = refId(ref);
  const bool setup = mode_ == Mode::Setup, item = (kind == "bone" || kind == "slot") && rig_;
  // An action on an item outside the selection selects it first; inside, it acts on the selection.
  auto pick = [this, ref] { if (!ref.isEmpty() && !selectedItems().contains(ref)) selectItem(ref); };
  auto add = [&](QMenu* in, const char* name, const QString& text, std::function<void()> fn) {
    QAction* a = in->addAction(text);
    a->setObjectName(name);
    connect(a, &QAction::triggered, this, [pick, fn] { pick(); fn(); });
    return a;
  };
  if (item) {
    add(menu, "cutoutCanvasMenuSelect", QString("「%1」 선택").arg(id), [] {});
    if (kind == "slot") add(menu, "cutoutCanvasMenuSelectBone", "그 본 선택 (더블클릭)", [this, ref] { canvasDoubleClick(ref); });
    else {
      bool parts = false;
      const int bi = rig_->boneIndex(ss(id));
      for (const auto& s : rig_->slots) parts = parts || s.boneIndex == bi;
      if (parts) add(menu, "cutoutCanvasMenuSelectPart", "첫 조각 선택 (더블클릭)", [this, ref] { canvasDoubleClick(ref); });
    }
    if (setup) {
      // 부모 바꾸기 (bone, keeps the world pose) / 붙을 본 바꾸기 (part): every other bone, never one
      // below the bone itself (that would be a cycle).
      QMenu* parents = menu->addMenu(kind == "bone" ? "부모 본 바꾸기" : "붙을 본 바꾸기");
      parents->setObjectName("cutoutCanvasMenuParent");
      QSet<int> below;
      if (kind == "bone") {
        below.insert(rig_->boneIndex(ss(id)));
        for (bool grew = true; grew;) {
          grew = false;
          for (size_t i = 0; i < rig_->bones.size(); i++)
            if (!below.contains(int(i)) && below.contains(rig_->bones[i].parentIndex)) { below.insert(int(i)); grew = true; }
        }
        add(parents, "cutoutCanvasMenuParentNone", "(없음: 맨 위 본)", [this, id] { requestReparent(id, QString()); });
      }
      for (size_t i = 0; i < rig_->bones.size(); i++) {
        if (below.contains(int(i))) continue;
        const QString bone = qs(rig_->bones[i].id);
        QAction* a = add(parents, "cutoutCanvasMenuParentTo", bone, [this, kind, id, bone] { if (kind == "bone") requestReparent(id, bone); else requestSlotBone(id, bone); });
        a->setData(bone);
      }
    }
    menu->addSeparator();
    if (!setup) add(menu, "cutoutCanvasMenuKey", "키 넣기 (K)", [this] { keySelected(); });
    if (setup && kind == "bone") add(menu, "cutoutCanvasMenuOrient", "본 방향 맞추기", [this] { orientBones(); });
    if (setup) add(menu, "cutoutCanvasMenuBox", "판정 상자 만들기", [this] { addBoundingBox(); });
    if (setup && kind == "slot") {
      // Only a picture shown as a plain region converts (the 메시 page's 메시로 변환, its grid fields).
      const cutout::Pose pose = currentPose(false);
      const int si = rig_->slotIndex(ss(id));
      const cutout::Attachment* att = si >= 0 && size_t(si) < pose.attachments.size() && pose.attachments[size_t(si)] ? rig_->attachment(ss(id), *pose.attachments[size_t(si)], pose.skin) : nullptr;
      if (att && !att->mesh && !att->clipping && !att->boundingBox && !att->point)
        add(menu, "cutoutCanvasMenuMesh", "메시로 바꾸기", [this] { runCommand("cutoutConvertMesh"); });
    }
    if (kind == "slot")
      add(menu, "cutoutCanvasMenuHide", "편집기에서 숨기기", [this, id] { hidden_.insert(id); updateCanvas(); refreshTree(); setStatus(QString("조각 「%1」을 편집기에서 숨겼습니다(저장되지 않음, 계층의 보기 체크로 되돌림).").arg(id)); });
    // Plugin 1.21.0: the item stays drawn but stops catching clicks (a paper sheet, a big body part).
    add(menu, "cutoutCanvasMenuLock", "캔버스에서 잠그기 (안 골라짐)", [this, ref] { setLocked(ref, true); });
    menu->addSeparator();
    if (setup) add(menu, "cutoutCanvasMenuDuplicate", "복제 (Ctrl+D)", [this] { duplicateSelected(); });
    add(menu, "cutoutCanvasMenuDelete", "삭제… (Delete)", [this] { deleteSelected(); });
  } else {
    add(menu, "cutoutCanvasMenuFrame", "선택 맞춤 (F)", [this] { canvas_->frameSelection(); });
    add(menu, "cutoutCanvasMenuFitAll", "전체 맞춤 (Home)", [this] { canvas_->fitToCanvas(); });
    if (!selection_.isEmpty()) add(menu, "cutoutCanvasMenuSelectNone", "선택 해제 (Esc)", [this] { selectItem(QString()); });
  }
  if (!hidden_.isEmpty()) add(menu, "cutoutCanvasMenuShowAll", "숨긴 조각 모두 보이기", [this] { hidden_.clear(); updateCanvas(); refreshTree(); });
  if (!locked_.isEmpty()) add(menu, "cutoutCanvasMenuUnlockAll", "잠금 모두 풀기", [this] { unlockAll(); });
  menu->addSeparator();
  add(menu, "cutoutCanvasMenuShortcuts", "단축키 모음 (?)", [this] { showShortcutOverlay(true); });
  return menu;
}

// ---- shortcut overlay: built from the panel's QShortcuts and the canvas's binding table
QVector<CutoutPanel::ShortcutInfo> CutoutPanel::shortcutList() const {
  QVector<ShortcutInfo> out;
  for (const QShortcut* s : findChildren<QShortcut*>())
    if (s->objectName().startsWith("cutoutShortcut"))
      out.push_back({s->key().toString(QKeySequence::NativeText), s->property("cutoutShortcutGroup").toString(), s->property("cutoutShortcutText").toString(), "panel"});
  for (const RigCanvas::KeyBinding& b : RigCanvas::keyBindings()) out.push_back({QString::fromUtf8(b.keys), QString::fromUtf8(b.group), QString::fromUtf8(b.text), "canvas"});
  return out;
}
void CutoutPanel::buildShortcutOverlay() {
  shortcutOverlay_ = new QFrame(this);
  shortcutOverlay_->setObjectName("cutoutShortcutOverlay");
  shortcutOverlay_->setFrameShape(QFrame::StyledPanel);
  shortcutOverlay_->setAutoFillBackground(true);
  shortcutOverlay_->hide();
  auto* layout = new QVBoxLayout(shortcutOverlay_);
  layout->setContentsMargins(8, 6, 8, 8);
  layout->setSpacing(4);
  auto* head = new QHBoxLayout;
  auto* title = new QLabel("캔버스 단축키 · 직접 조작 (?로 열고 닫기 · Esc 닫기 · 글자 칸에 입력 중에는 글자 키가 단축키가 아닙니다)");
  title->setObjectName("cutoutShortcutTitle");
  title->setWordWrap(true);
  title->setStyleSheet("color:#a7b4c2;");
  auto* close = new QToolButton;
  close->setObjectName("cutoutShortcutOverlayClose");
  close->setText("닫기 ✕");
  close->setToolTip("단축키 모음을 닫습니다 (Esc).");
  connect(close, &QToolButton::clicked, this, [this] { showShortcutOverlay(false); });
  head->addWidget(title, 1);
  head->addWidget(close);
  shortcutTree_ = new QTreeWidget;
  shortcutTree_->setObjectName("cutoutShortcutList");
  shortcutTree_->setColumnCount(2);
  shortcutTree_->setHeaderLabels({"키", "하는 일"});
  shortcutTree_->setRootIsDecorated(false);
  shortcutTree_->setUniformRowHeights(true);
  shortcutTree_->setWordWrap(false);
  shortcutTree_->installEventFilter(this);
  layout->addLayout(head);
  layout->addWidget(shortcutTree_, 1);
}
void CutoutPanel::showShortcutOverlay(bool on) {
  if (!shortcutOverlay_) return;
  if (!on) {
    shortcutOverlay_->hide();
    if (canvas_) canvas_->setFocus(Qt::OtherFocusReason);
    return;
  }
  shortcutTree_->clear();
  // Groups in a fixed reading order (unknown groups after them, in first-seen order).
  QStringList groups = groupOrder();
  const QVector<ShortcutInfo> list = shortcutList();
  for (const ShortcutInfo& s : list) if (!groups.contains(s.group)) groups << s.group;
  for (const QString& g : groups) {
    QTreeWidgetItem* head = nullptr;
    for (const ShortcutInfo& s : list) {
      if (s.group != g) continue;
      if (!head) {
        head = new QTreeWidgetItem(shortcutTree_, {g});
        head->setFirstColumnSpanned(true);
        QFont f = head->font(0);
        f.setBold(true);
        head->setFont(0, f);
        head->setFlags(Qt::ItemIsEnabled);
      }
      auto* row = new QTreeWidgetItem(head, {s.keys, s.text});
      row->setData(0, Qt::UserRole, s.source);
      row->setToolTip(1, s.text);
    }
  }
  shortcutTree_->expandAll();
  shortcutTree_->resizeColumnToContents(0);
  const int top = modeBar_ ? modeBar_->geometry().bottom() + 4 : 8;
  const int w = std::max(240, std::min(width() - 24, int(720 * ui_)));
  shortcutOverlay_->setGeometry((width() - w) / 2, top, w, std::max(160, std::min(height() - top - 12, int(560 * ui_))));
  shortcutOverlay_->show();
  shortcutOverlay_->raise();
  shortcutTree_->setFocus(Qt::OtherFocusReason);
}
bool CutoutPanel::shortcutOverlayVisible() const { return shortcutOverlay_ && shortcutOverlay_->isVisible(); }
}  // namespace cutout_studio
