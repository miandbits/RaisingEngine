// Clipping and image sequence authoring (plugin 1.12.0): the 클리핑 and 시퀀스 pages of the look
// section, their canvas gestures and their edits. Like every panel gesture, each one is ONE core
// edit (addClipping, setClipping, setSequence, setKey ...), so the MCP/LLM `edit` op builds the
// same documents; the canvas preview draws through drawRig, which clips and picks frames exactly
// like the runtimes.
#include "cutout-panel.hpp"
#include <QtWidgets>
#include <algorithm>
#include "cutout/editing.hpp"

namespace cutout_studio {
using cutout::Json;
namespace {
QString refKind(const QString& ref) { return ref.section(':', 0, 0); }
QString refId(const QString& ref) { return ref.section(':', 1); }
/** The look pages' action-button row (same contract as cutout-panel.cpp's buttonRow: never hides
 *  a button, the widest row is the right pane's minimum width). */
QWidget* pageRow(const char* name) {
  auto* row = new QWidget;
  row->setObjectName(name);
  row->setProperty("cutoutRow", true);
  row->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  auto* layout = new QHBoxLayout(row);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(3);
  layout->addStretch();
  return row;
}
void addTo(QWidget* row, QWidget* w) {
  auto* layout = static_cast<QHBoxLayout*>(row->layout());
  layout->insertWidget(layout->count() - 1, w);
}
/** Spine sequence modes as the page lists them (data = the document value). */
const QVector<QPair<const char*, const char*>>& sequenceModes() {
  static const QVector<QPair<const char*, const char*>> modes = {
    {"hold", "멈춤 (그 프레임)"}, {"once", "한 번"}, {"loop", "반복"}, {"pingpong", "왕복"},
    {"onceReverse", "거꾸로 한 번"}, {"loopReverse", "거꾸로 반복"}, {"pingpongReverse", "거꾸로 왕복"}};
  return modes;
}
}  // namespace

// ---------------------------------------------------------------- pages
QWidget* CutoutPanel::buildClipping() {
  QWidget* page = lookPage("cutoutClippingPage");
  page->setProperty("cutoutGroup", "클리핑");
  auto* layout = static_cast<QVBoxLayout*>(page->layout());
  auto button = [this](QWidget* row, const QString& text, const char* name, const QString& tip, std::function<bool()> fn) {
    auto* b = new QPushButton(text);
    b->setObjectName(name);
    b->setToolTip(tip);
    connect(b, &QPushButton::clicked, this, [fn] { fn(); });
    addTo(row, b);
    return b;
  };
  QWidget* tools = pageRow("cutoutClippingTools");
  clippingCreate_ = button(tools, "클리핑 만들기", "cutoutClippingCreate",
                       "조립 모드: 고른 조각 위에 클리핑 영역을 그립니다. 캔버스를 차례로 클릭해 다각형을 그리고(오목해도 됨) 첫 점·더블클릭·Enter로 닫으면, "
                       "그 조각 바로 아래에 클리핑 조각이 생기고 그 조각은 다각형 안에서만 보입니다. Esc = 취소.",
                       [this] { return beginClipping(); });
  clippingRedraw_ = button(tools, "모양 다시 그리기", "cutoutClippingRedraw", "조립 모드: 고른 클리핑 조각의 다각형을 캔버스에 새로 그립니다(점 수가 바뀌면 웨이트·변형 키는 지워짐).",
                       [this] { return redrawClipping(); });
  clippingDelete_ = button(tools, "클리핑 삭제", "cutoutClippingDelete", "고른 클리핑 조각을 지웁니다. 잘려 보이던 조각은 다시 전부 보입니다.", [this] { return deleteClipping(); });
  QWidget* endRow = pageRow("cutoutClippingEndRow");
  auto* endLabel = new QLabel("끝 조각");
  endLabel->setToolTip("이 조각까지(그리는 순서) 클리핑됩니다.");
  clippingEnd_ = new QComboBox;
  clippingEnd_->setObjectName("cutoutClippingEnd");
  clippingEnd_->setToolTip("클리핑이 적용되는 마지막 조각(포함). 클리핑 조각 바로 위부터 이 조각까지가 다각형 안에서만 보입니다.");
  clippingEnd_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  clippingEnd_->setMinimumContentsLength(8);
  connect(clippingEnd_, &QComboBox::activated, this, [this] { if (!filling_) setClipEnd(clippingEnd_->currentData().toString()); });
  clippingOutlines_ = new QToolButton;
  clippingOutlines_->setObjectName("cutoutClippingOutlines");
  clippingOutlines_->setText("윤곽 보기");
  clippingOutlines_->setCheckable(true);
  clippingOutlines_->setChecked(true);
  clippingOutlines_->setToolTip("캔버스에 클리핑 다각형을 점선(분홍)으로 보여 줍니다(편집기 전용). 눈 끄기(숨김)한 클리핑 조각은 자르지 않습니다.");
  connect(clippingOutlines_, &QToolButton::toggled, this, [this](bool on) { setClipOutlines(on); });
  addTo(endRow, endLabel);
  addTo(endRow, clippingEnd_);
  addTo(endRow, clippingOutlines_);
  clippingList_ = new QListWidget;
  clippingList_->setObjectName("cutoutClippingList");
  clippingList_->setMinimumHeight(LIST_MIN_HEIGHT);
  clippingList_->setToolTip("이 캐릭터의 클리핑 조각과 그 범위(시작 조각 → 끝 조각). 고르면 캔버스·트리에서 선택됩니다.");
  connect(clippingList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
    if (!filling_ && item) selectItem("slot:" + item->data(Qt::UserRole).toString());
  });
  auto* inner = new QWidget;
  auto* formLayout = new QVBoxLayout(inner);
  formLayout->setContentsMargins(0, 0, 0, 0);
  clippingInfo_ = new QLabel;
  clippingInfo_->setObjectName("cutoutClippingInfo");
  clippingInfo_->setWordWrap(true);
  clippingInfo_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  clippingInfo_->setStyleSheet("color:#a7b4c2;");
  formLayout->addWidget(clippingInfo_);
  formLayout->addStretch();
  layout->addWidget(tools);
  layout->addWidget(endRow);
  layout->addWidget(clippingList_, 2);
  layout->addWidget(formScroll("cutoutClippingFormScroll", inner), 1);
  return page;
}

QWidget* CutoutPanel::buildSequence() {
  QWidget* page = lookPage("cutoutSequencePage");
  page->setProperty("cutoutGroup", "시퀀스");
  auto* layout = static_cast<QVBoxLayout*>(page->layout());
  auto button = [this](QWidget* row, const QString& text, const char* name, const QString& tip, std::function<bool()> fn) {
    auto* b = new QPushButton(text);
    b->setObjectName(name);
    b->setToolTip(tip);
    connect(b, &QPushButton::clicked, this, [fn] { fn(); });
    addTo(row, b);
    return b;
  };
  QWidget* tools = pageRow("cutoutSequenceTools");
  QPushButton* import = button(tools, "PNG로 시퀀스…", "cutoutSequenceImport",
                               "조립 모드: 같은 크기의 PNG 여러 장(프레임 순서대로 이름 정렬)을 골라 고른 조각의 그림을 프레임 시퀀스로 바꿉니다. "
                               "조각을 고르지 않았으면 고른 본에 새 조각을 만듭니다.",
                               [this] { return importSequenceInteractive(); });
  QPushButton* add = button(tools, "프레임 추가…", "cutoutSequenceAddFrames", "조립 모드: PNG를 골라 시퀀스 끝에 프레임으로 붙입니다(같은 크기여야 함).", [this] { return addSequenceFramesInteractive(); });
  QPushButton* clear = button(tools, "시퀀스 해제", "cutoutSequenceClear", "시퀀스를 없애고 첫 프레임만 보통 그림으로 남깁니다. 이 조각의 시퀀스 키도 지워집니다.", [this] { return clearSequence(); });
  QWidget* frameTools = pageRow("cutoutSequenceFrameTools");
  QPushButton* up = button(frameTools, "▲", "cutoutSequenceUp", "고른 프레임을 앞으로(번호 -1) 옮깁니다.", [this] {
    const int row = sequenceFrames_->currentRow();
    return row > 0 && moveSequenceFrame(row, row - 1);
  });
  QPushButton* down = button(frameTools, "▼", "cutoutSequenceDown", "고른 프레임을 뒤로(번호 +1) 옮깁니다.", [this] {
    const int row = sequenceFrames_->currentRow();
    return row >= 0 && row + 1 < sequenceFrames_->count() && moveSequenceFrame(row, row + 1);
  });
  QPushButton* remove = button(frameTools, "빼기", "cutoutSequenceRemoveFrame", "고른 프레임을 시퀀스에서 뺍니다(그 뒤 번호는 하나씩 당겨짐).", [this] { return removeSequenceFrame(sequenceFrames_->currentRow()); });
  sequencePlay_ = new QToolButton;
  sequencePlay_->setObjectName("cutoutSequencePlay");
  sequencePlay_->setText("▶ 미리보기");
  sequencePlay_->setCheckable(true);
  sequencePlay_->setToolTip("아래 재생 방식·시작 프레임·간격으로 프레임을 이 페이지에서 재생해 봅니다(편집기 전용, 문서는 바뀌지 않음).");
  connect(sequencePlay_, &QToolButton::toggled, this, [this](bool on) { if (!filling_) setSequencePreview(on); });
  addTo(frameTools, sequencePlay_);
  sequenceFrames_ = new QListWidget;
  sequenceFrames_->setObjectName("cutoutSequenceFrames");
  sequenceFrames_->setMinimumHeight(LIST_MIN_HEIGHT);
  sequenceFrames_->setIconSize(QSize(28, 28));
  sequenceFrames_->setToolTip("프레임 목록(0부터). ▲▼로 순서를 바꾸고, 시퀀스 키의 시작 프레임 번호가 이 번호입니다.");
  connect(sequenceFrames_, &QListWidget::currentRowChanged, this, [this](int row) {
    if (filling_ || row < 0 || sequencePreviewPlaying()) return;
    sequencePreviewFrame_ = row;
    showSequenceFrame();
  });
  QWidget* keyTools = pageRow("cutoutSequenceKeyTools");
  keyTools->setProperty("cutoutModeRow", "animate");
  QPushButton* key = button(keyTools, "시퀀스 키", "cutoutSequenceKey", "애니메이션 모드: 재생 방식·시작 프레임·간격을 재생 헤드 시간의 시퀀스 키로 넣습니다(그 시간부터 프레임이 바뀜).", [this] { return keySequence(); });
  QPushButton* keyDelete = button(keyTools, "키 삭제", "cutoutSequenceDeleteKey", "애니메이션 모드: 재생 헤드 시간의 시퀀스 키를 지웁니다.", [this] { return deleteSequenceKey(); });
  auto* inner = new QWidget;
  auto* formLayout = new QVBoxLayout(inner);
  formLayout->setContentsMargins(0, 0, 0, 0);
  sequenceInfo_ = new QLabel;
  sequenceInfo_->setObjectName("cutoutSequenceInfo");
  sequenceInfo_->setWordWrap(true);
  sequenceInfo_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  sequenceInfo_->setStyleSheet("color:#a7b4c2;");
  sequencePreview_ = new QLabel;
  sequencePreview_->setObjectName("cutoutSequencePreview");
  sequencePreview_->setFixedSize(72, 72);
  sequencePreview_->setAlignment(Qt::AlignCenter);
  sequencePreview_->setStyleSheet("background:#2b3139;border:1px solid #56606c;");
  sequencePreview_->setToolTip("미리보기 중인 프레임");
  auto* form = new QFormLayout;
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  sequenceSetup_ = new QSpinBox;
  sequenceSetup_->setObjectName("cutoutSequenceSetup");
  sequenceSetup_->setToolTip("조립 모드: 키가 없을 때(기본 자세, 첫 키 전) 보이는 프레임 번호");
  connect(sequenceSetup_, &QSpinBox::editingFinished, this, [this] { if (!filling_) setSequenceSetupFrame(sequenceSetup_->value()); });
  sequenceMode_ = new QComboBox;
  sequenceMode_->setObjectName("cutoutSequenceMode");
  for (const auto& [mode, label] : sequenceModes()) sequenceMode_->addItem(label, QString(mode));
  sequenceMode_->setCurrentIndex(2);  // loop: what a new key usually wants
  sequenceMode_->setToolTip("시퀀스 키의 재생 방식: 멈춤 = 시작 프레임 고정, 한 번 = 끝 프레임에서 멈춤, 반복, 왕복, 거꾸로(마지막 프레임부터 셈)");
  sequenceIndex_ = new QSpinBox;
  sequenceIndex_->setObjectName("cutoutSequenceIndex");
  sequenceIndex_->setToolTip("시퀀스 키의 시작 프레임 번호(거꾸로 방식은 마지막 프레임부터 셈)");
  sequenceDelay_ = new QDoubleSpinBox;
  sequenceDelay_->setObjectName("cutoutSequenceDelay");
  sequenceDelay_->setRange(1, 60000);
  sequenceDelay_->setDecimals(1);
  sequenceDelay_->setValue(100);
  sequenceDelay_->setSuffix(" ms");
  sequenceDelay_->setToolTip("프레임 하나를 보여 주는 시간(ms). 100 ms = 초당 10프레임");
  form->addRow("기본 프레임", sequenceSetup_);
  form->addRow("재생 방식", sequenceMode_);
  form->addRow("시작 프레임", sequenceIndex_);
  form->addRow("프레임 간격", sequenceDelay_);
  auto* previewRow = new QHBoxLayout;
  previewRow->addWidget(sequencePreview_);
  previewRow->addWidget(sequenceInfo_, 1);
  formLayout->addLayout(previewRow);
  formLayout->addLayout(form);
  formLayout->addStretch();
  sequenceTimer_ = new QTimer(this);
  sequenceTimer_->setInterval(16);
  connect(sequenceTimer_, &QTimer::timeout, this, [this] { advanceSequencePreview(16); });
  layout->addWidget(tools);
  layout->addWidget(frameTools);
  layout->addWidget(keyTools);
  layout->addWidget(sequenceFrames_, 2);
  layout->addWidget(formScroll("cutoutSequenceFormScroll", inner), 2);
  sequencePartTools_ = {import};
  sequenceFrameTools_ = {add, clear, up, down, remove, sequenceSetup_};
  sequenceKeyTools_ = {key, keyDelete};
  meshModeRows_.push_back(keyTools);  // shown in Animate mode only (refreshMesh toggles cutoutModeRow rows)
  return page;
}

// ---------------------------------------------------------------- clipping
QString CutoutPanel::clipSlot() const {
  int si;
  const cutout::Attachment* att;
  Json fields;
  return meshTarget(si, att, fields) && att->clipping ? refId(selection_) : QString();
}
void CutoutPanel::setClipOutlines(bool on) {
  if (clippingOutlines_ && clippingOutlines_->isChecked() != on) { QSignalBlocker block(clippingOutlines_); clippingOutlines_->setChecked(on); }
  if (canvas_) canvas_->setShowClipOutlines(on);
}
void CutoutPanel::refreshClipping() {
  if (!clippingList_) return;
  const bool wasFilling = filling_;
  filling_ = true;
  const bool setup = mode_ == Mode::Setup;
  const QString clip = clipSlot();
  int si = -1;
  const cutout::Attachment* att = nullptr;
  Json fields;
  const bool part = meshTarget(si, att, fields) && !att->pictureless();
  {
    QSignalBlocker block(clippingList_);
    clippingList_->clear();
    for (size_t i = 0; rig_ && i < rig_->slots.size(); i++) {
      const auto& slot = rig_->slots[i];
      const cutout::Attachment* a = slot.attachment ? rig_->attachment(slot.id, *slot.attachment) : nullptr;
      if (!a || !a->clipping) continue;
      const QString first = i + 1 < rig_->slots.size() ? qs(rig_->slots[i + 1].id) : QString("-");
      auto* item = new QListWidgetItem(QString("✂ %1 · %2 → %3").arg(qs(slot.id), first, qs(a->clipEnd)), clippingList_);
      item->setData(Qt::UserRole, qs(slot.id));
      if (qs(slot.id) == clip) clippingList_->setCurrentItem(item);
    }
  }
  {
    QSignalBlocker block(clippingEnd_);
    clippingEnd_->clear();
    const int at = clip.isEmpty() ? -1 : rig_->slotIndex(ss(clip));
    if (at >= 0) {
      const cutout::Attachment* a = nullptr;
      attachmentMatrix(*rig_, currentPose(false), at, &a);
      for (size_t i = size_t(at) + 1; i < rig_->slots.size(); i++) clippingEnd_->addItem(qs(rig_->slots[i].id), qs(rig_->slots[i].id));
      if (a) clippingEnd_->setCurrentIndex(std::max(0, clippingEnd_->findData(qs(a->clipEnd))));
    }
  }
  clippingCreate_->setEnabled(setup && part && !outlineForClip_);
  clippingRedraw_->setEnabled(setup && !clip.isEmpty() && !outlineForClip_);
  clippingDelete_->setEnabled(!clip.isEmpty());
  clippingEnd_->setEnabled(setup && !clip.isEmpty());
  QString info = "조각(▣)을 고르고 「클리핑 만들기」로 다각형을 그리면, 그 조각이 다각형 안에서만 보입니다(가면·창문·주머니 속 등). 끝 조각을 바꾸면 위의 조각들까지 함께 잘립니다.";
  if (outlineForClip_ && !outlineSlot_.isEmpty())
    info = QString("클리핑 다각형 그리는 중: 캔버스를 차례로 클릭하세요. 첫 점 클릭·더블클릭·Enter = 완성, Backspace = 마지막 점 지우기, Esc = 취소. (%1)").arg(outlineSlot_);
  else if (!clip.isEmpty()) {
    const int at = rig_->slotIndex(ss(clip));
    const cutout::Attachment* a = nullptr;
    attachmentMatrix(*rig_, currentPose(false), at, &a);
    QStringList range;
    for (int i = at + 1; a && i < int(rig_->slots.size()) && i <= a->clipEndIndex; i++) range << qs(rig_->slots[size_t(i)].id);
    info = QString("클리핑 「%1」 · 점 %2개%3 · 잘리는 조각: %4. 다각형 밖은 그려지지 않습니다(게임에서도 같음).")
               .arg(clip).arg(a ? a->vertexCount() : 0).arg(a && !a->weights.empty() ? " · 웨이트" : "").arg(range.isEmpty() ? QString("없음") : range.join(", "));
    if (!setup) info += " 모양과 끝 조각은 조립 모드에서 바꿉니다.";
  } else if (part && !setup) info = "클리핑은 조립 모드에서 만듭니다.";
  clippingInfo_->setText(info);
  if (canvas_) canvas_->setShowClipOutlines(clippingOutlines_->isChecked());
  filling_ = wasFilling;
}
bool CutoutPanel::beginClipping() {
  if (mode_ != Mode::Setup) { setStatus("클리핑은 조립 모드에서 만듭니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json fields;
  if (!meshTarget(si, att, fields) || att->pictureless()) { setStatus("클리핑할 조각(▣)을 먼저 고르세요.", true); return false; }
  outlineSlot_ = refId(selection_);
  outlineForClip_ = true;
  meshEdit_ = MeshEdit::Off;
  refreshMesh();
  refreshClipping();
  if (canvas_) canvas_->setFocus(Qt::OtherFocusReason);
  setStatus(QString("클리핑: 「%1」이 보일 영역을 캔버스에 차례로 클릭해 그리세요. 첫 점 클릭·더블클릭·Enter로 닫습니다. Esc = 취소").arg(outlineSlot_));
  return true;
}
bool CutoutPanel::redrawClipping() {
  if (mode_ != Mode::Setup) { setStatus("클리핑 모양은 조립 모드에서 바꿉니다.", true); return false; }
  const QString clip = clipSlot();
  if (clip.isEmpty()) { setStatus("다시 그릴 클리핑 조각을 고르세요(클리핑 목록·트리).", true); return false; }
  outlineSlot_ = clip;
  outlineForClip_ = true;
  meshEdit_ = MeshEdit::Off;
  refreshMesh();
  refreshClipping();
  if (canvas_) canvas_->setFocus(Qt::OtherFocusReason);
  setStatus(QString("클리핑 「%1」의 새 다각형을 캔버스에 그리세요. 첫 점 클릭·더블클릭·Enter로 닫습니다. Esc = 취소").arg(clip));
  return true;
}
bool CutoutPanel::clippingFromCanvas(const QPolygonF& rigPoints) {
  const QString target = outlineSlot_;
  if (mode_ != Mode::Setup || target.isEmpty() || !rig_) { outlineSlot_.clear(); outlineForClip_ = false; refreshMesh(); refreshClipping(); return false; }
  bool ok = false;
  if (target == clipSlot()) {
    int si;
    const cutout::Attachment* att;
    Json op;
    meshTarget(si, att, op);
    op["op"] = "setClipping";
    Json points = Json::array();
    for (const QPointF& p : rigPoints) { points.push_back(p.x()); points.push_back(p.y()); }
    op["worldVertices"] = points;
    ok = runOps({op}, false);
    if (ok) setStatus(QString("클리핑 「%1」의 모양을 바꿨습니다(점 %2개). 되돌리기: Ctrl+Z").arg(target).arg(rigPoints.size()));
  } else ok = addClipping(target, rigPoints);
  // A refused polygon (crossing itself...) keeps the tool on so the author can draw again.
  if (!ok) return false;
  outlineSlot_.clear();
  outlineForClip_ = false;
  refreshMesh();
  refreshClipping();
  return true;
}
bool CutoutPanel::addClipping(const QString& part, const QPolygonF& rigPoints) {
  if (!rig_ || rig_->slotIndex(ss(part)) < 0) { setStatus("클리핑할 조각이 없습니다: " + part, true); return false; }
  if (rigPoints.size() < 3) { setStatus("클리핑 다각형에는 점이 3개 이상 필요합니다.", true); return false; }
  QString id = part + "-clip";
  for (int n = 2; rig_->slotIndex(ss(id)) >= 0; n++) id = QString("%1-clip-%2").arg(part).arg(n);
  Json points = Json::array();
  for (const QPointF& p : rigPoints) { points.push_back(p.x()); points.push_back(p.y()); }
  const QString before = selection_;
  selection_ = "slot:" + id;
  if (!runOps({{{"op", "addClipping"}, {"slot", ss(id)}, {"before", ss(part)}, {"worldVertices", points}}}, false)) { selection_ = before; return false; }
  setStatus(QString("클리핑 「%1」을 만들었습니다: 「%2」은 이제 다각형 안에서만 보입니다. 위 조각까지 자르려면 「끝 조각」을 바꾸세요. 되돌리기: Ctrl+Z").arg(id, part));
  return true;
}
bool CutoutPanel::setClipEnd(const QString& end) {
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op) || !att->clipping) { setStatus("끝 조각을 바꿀 클리핑 조각을 고르세요.", true); return false; }
  if (mode_ != Mode::Setup) { setStatus("끝 조각은 조립 모드에서 바꿉니다.", true); refreshClipping(); return false; }
  op["op"] = "setClipping";
  op["end"] = ss(end);
  return runOps({op}, false);
}
bool CutoutPanel::deleteClipping() {
  const QString clip = clipSlot();
  if (clip.isEmpty()) { setStatus("지울 클리핑 조각을 고르세요.", true); return false; }
  if (!askConfirm("클리핑 삭제", "클리핑 조각 「" + clip + "」를 지울까요? 잘려 보이던 조각은 다시 전부 보입니다.")) return false;
  return runOps({{{"op", "deleteSlot"}, {"slot", ss(clip)}}}, false);
}

// ---------------------------------------------------------------- sequences
QString CutoutPanel::sequenceSlot() const {
  int si;
  const cutout::Attachment* att;
  Json fields;
  return meshTarget(si, att, fields) && att->sequence() ? refId(selection_) : QString();
}
void CutoutPanel::refreshSequence() {
  if (!sequenceFrames_) return;
  const bool wasFilling = filling_;
  filling_ = true;
  const bool setup = mode_ == Mode::Setup, animate = !setup;
  int si = -1;
  const cutout::Attachment* att = nullptr;
  Json fields;
  const bool part = meshTarget(si, att, fields) && !att->pictureless();
  const bool sequence = part && att->sequence();
  const int count = sequence ? int(att->frames.size()) : 0;
  // The list is rebuilt only for other frames (scrubbing refreshes this page on every move).
  QStringList frames;
  if (sequence) for (const auto& f : att->frames) frames << qs(f);
  const QString key = sequence ? qs(fields.dump()) + "#" + frames.join("|") : QString();
  if (sequenceFrames_->property("key").toString() != key) {
    QSignalBlocker block(sequenceFrames_);
    const int keep = sequenceFrames_->currentRow();
    sequenceFrames_->clear();
    for (int f = 0; f < count; f++) {
      const QImage img = images_.image(frames[f]);
      auto* item = new QListWidgetItem(QString("%1 · %2").arg(f).arg(frames[f]), sequenceFrames_);
      if (!img.isNull()) item->setIcon(QIcon(QPixmap::fromImage(img.scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation))));
      item->setData(Qt::UserRole, frames[f]);
    }
    sequenceFrames_->setCurrentRow(std::clamp(keep, count ? 0 : -1, count - 1));
    sequenceFrames_->setProperty("key", key);
    sequencePreviewFrame_ = sequence ? std::clamp(att->setupIndex, 0, count - 1) : 0;
  }
  {
    QSignalBlocker block(sequenceSetup_);
    sequenceSetup_->setRange(0, std::max(0, count - 1));
    sequenceSetup_->setValue(sequence ? att->setupIndex : 0);
  }
  sequenceIndex_->setRange(0, std::max(0, count - 1));
  // Animate mode: the key at the playhead (if any) fills the key fields, so 시퀀스 키 edits it.
  if (sequence && animate && !clipId_.isEmpty()) {
    const JsonPtr held = displayShared();  // kept alive while its keys are read
    const Json& data = *held;
    const Json none = Json::array();
    for (const auto& c : data.contains("clips") && data.at("clips").is_array() ? data.at("clips") : none) {
      if (c.value("id", std::string()) != ss(clipId_) || !c.contains("tracks") || !c.at("tracks").is_array()) continue;
      for (const auto& t : c.at("tracks")) {
        if (t.value("target", std::string()) != ss(selection_) || t.value("property", std::string()) != "sequence" || !t.contains("keys")) continue;
        for (const auto& k : t.at("keys"))
          if (k.value("timeMs", int64_t(-1)) == playheadMs_) {
            QSignalBlocker b1(sequenceMode_), b2(sequenceIndex_), b3(sequenceDelay_);
            sequenceMode_->setCurrentIndex(std::max(0, sequenceMode_->findData(qs(k.value("mode", std::string("hold"))))));
            sequenceIndex_->setValue(int(k.value("index", int64_t(0))));
            sequenceDelay_->setValue(k.value("delay", 100.0));
          }
      }
    }
  }
  for (QWidget* w : sequencePartTools_) w->setEnabled(setup && rig_ && (part || !selectedBone().isEmpty() || !rig_->bones.empty()));
  for (QWidget* w : sequenceFrameTools_) w->setEnabled(setup && sequence);
  for (QWidget* w : sequenceKeyTools_) w->setEnabled(animate && sequence && !clipId_.isEmpty());
  sequencePlay_->setEnabled(sequence);
  if (!sequence && sequencePreviewPlaying()) setSequencePreview(false);
  QString info = "조각(▣)을 고르고 「PNG로 시퀀스…」로 같은 크기 PNG 여러 장을 넣으면 프레임 애니메이션(불꽃·눈 깜빡임·입 모양 등)이 됩니다. 조각을 고르지 않으면 고른 본에 새 조각이 생깁니다.";
  if (sequence) {
    info = QString("「%1」 · 프레임 %2개 · 기본 %3번 · 미리보기 %4번. ").arg(qs(att->name)).arg(count).arg(att->setupIndex).arg(sequencePreviewFrame_);
    info += animate ? "재생 방식·시작 프레임·간격을 정하고 「시퀀스 키」를 누르면 그 시간부터 프레임이 바뀝니다(도프시트의 시퀀스 행)."
                    : "프레임 순서·기본 프레임을 정하세요. 키는 애니메이션 모드에서 넣습니다.";
  } else if (part) info = QString("「%1」은 한 장짜리 그림입니다. 「PNG로 시퀀스…」로 프레임을 넣으세요(조립 모드).").arg(qs(att->name));
  sequenceInfo_->setText(info);
  showSequenceFrame();
  filling_ = wasFilling;
}
void CutoutPanel::showSequenceFrame() {
  if (!sequencePreview_) return;
  const QString id = sequenceSlot().isEmpty() || sequencePreviewFrame_ >= sequenceFrames_->count() ? QString()
                                                                                                   : sequenceFrames_->item(sequencePreviewFrame_)->data(Qt::UserRole).toString();
  const QImage img = id.isEmpty() ? QImage() : images_.image(id);
  sequencePreview_->setPixmap(img.isNull() ? QPixmap() : QPixmap::fromImage(img.scaled(sequencePreview_->size() - QSize(4, 4), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
  sequencePreview_->setText(img.isNull() ? (id.isEmpty() ? QString("—") : QString::number(sequencePreviewFrame_)) : QString());
}
bool CutoutPanel::sequencePreviewPlaying() const { return sequencePlay_ && sequencePlay_->isChecked(); }
void CutoutPanel::setSequencePreview(bool playing) {
  if (playing && sequenceSlot().isEmpty()) playing = false;
  sequencePreviewMs_ = 0;
  // A manual clock (automation) steps the preview through advanceSequencePreview instead.
  if (playing && !manualClock_) sequenceTimer_->start();
  else sequenceTimer_->stop();
  if (sequencePlay_->isChecked() != playing) { QSignalBlocker block(sequencePlay_); sequencePlay_->setChecked(playing); }
  if (playing) advanceSequencePreview(0);
}
void CutoutPanel::advanceSequencePreview(double dtMs) {
  int si;
  const cutout::Attachment* att;
  Json fields;
  if (!meshTarget(si, att, fields) || !att->sequence()) return;
  sequencePreviewMs_ += std::max(0.0, dtMs);
  // The same frame rule as the runtimes (cutout::sequenceFrame), from the key fields.
  cutout::SequenceState s;
  s.keyed = true;
  s.mode = cutout::parseSequenceMode(ss(sequenceMode_->currentData().toString())).value_or(cutout::SequenceMode::Loop);
  s.index = sequenceIndex_->value();
  s.delay = sequenceDelay_->value();
  s.elapsedMs = sequencePreviewMs_;
  sequencePreviewFrame_ = cutout::sequenceFrame(s, att->frames.size(), att->setupIndex);
  showSequenceFrame();
}
bool CutoutPanel::importSequenceInteractive() {
  if (mode_ != Mode::Setup) { setStatus("시퀀스는 조립 모드에서 만듭니다.", true); return false; }
  const QStringList files = askSequenceFiles ? askSequenceFiles()
                                             : QFileDialog::getOpenFileNames(this, "시퀀스 프레임 PNG 고르기", host_ ? host_->projectRoot() : QString(), "PNG 이미지 (*.png)");
  return !files.isEmpty() && importSequence(files, false);
}
bool CutoutPanel::addSequenceFramesInteractive() {
  if (sequenceSlot().isEmpty()) { setStatus("프레임을 붙일 시퀀스 조각을 고르세요.", true); return false; }
  const QStringList files = askSequenceFiles ? askSequenceFiles()
                                             : QFileDialog::getOpenFileNames(this, "붙일 프레임 PNG 고르기", host_ ? host_->projectRoot() : QString(), "PNG 이미지 (*.png)");
  return !files.isEmpty() && importSequence(files, true);
}
void CutoutPanel::importImages(QStringList files, QStringList ids, std::function<void(const QStringList&)> done) {
  if (files.isEmpty()) { done(ids); return; }
  QPointer<CutoutPanel> self(this);
  const QString file = files.takeFirst();
  inSubmit_ = true;
  host_->request("importImage", {{"file", file}}, [self, files, ids, done, file](QJsonObject reply) mutable {
    if (!self) return;
    const QString id = reply.value("importedAssetId").toString();
    if (!reply.value("error").toString().isEmpty() || id.isEmpty()) {
      self->setStatus(QString("PNG를 가져오지 못했습니다(%1): %2").arg(QFileInfo(file).fileName(), reply.value("error").toString()), true);
      done({});
      return;
    }
    ids << id;
    // Next turn of the event loop: the host is idle again before the next file.
    QTimer::singleShot(0, self, [self, files, ids, done] { if (self && self->host_) self->importImages(files, ids, done); });
  });
  inSubmit_ = false;
}
bool CutoutPanel::importSequence(const QStringList& filesIn, bool append) {
  if (!host_ || busy()) return false;
  if (mode_ != Mode::Setup) { setStatus("시퀀스는 조립 모드에서 만듭니다.", true); return false; }
  if (!rigDoc() || !rig_) { setStatus("먼저 캐릭터를 만드세요.", true); return false; }
  QStringList files = filesIn;
  // Frame order = file name order with numbers compared as numbers (walk2 before walk10).
  QCollator order;
  order.setNumericMode(true);
  std::stable_sort(files.begin(), files.end(), [&](const QString& a, const QString& b) { return order.compare(QFileInfo(a).fileName(), QFileInfo(b).fileName()) < 0; });
  QStringList keep;
  if (append) {
    int si;
    const cutout::Attachment* att;
    Json fields;
    if (!meshTarget(si, att, fields) || !att->sequence()) { setStatus("프레임을 붙일 시퀀스 조각을 고르세요.", true); return false; }
    for (const auto& f : att->frames) keep << qs(f);
  }
  QPointer<CutoutPanel> self(this);
  setStatus(QString("프레임 PNG %1장을 가져오는 중…").arg(files.size()));
  importImages(files, {}, [self, keep](const QStringList& ids) {
    if (!self || ids.isEmpty()) return;
    QTimer::singleShot(0, self, [self, keep, ids] {
      if (!self || !self->host_) return;
      self->images_.setAssets(self->host_->assets());
      self->setSequenceFrames(keep + ids);
    });
  });
  return true;
}
bool CutoutPanel::setSequenceFrames(const QStringList& ids, int setupIndex) {
  if (mode_ != Mode::Setup) { setStatus("시퀀스 프레임은 조립 모드에서 바꿉니다.", true); return false; }
  if (ids.isEmpty()) return clearSequence();
  if (ids.size() > int(cutout::MAX_SEQUENCE_FRAMES)) { setStatus(QString("프레임은 %1장까지입니다.").arg(cutout::MAX_SEQUENCE_FRAMES), true); return false; }
  // One size for every frame: they share the attachment's placement (and a mesh's uvs).
  const QSize size = images_.pixelSize(ids[0]);
  if (!size.isValid() || size.isEmpty()) { setStatus("이미지 크기를 읽을 수 없습니다: " + ids[0], true); return false; }
  for (const QString& id : ids) {
    const QSize s = images_.pixelSize(id);
    if (s != size) { setStatus(QString("프레임 「%1」(%2×%3)의 크기가 첫 프레임(%4×%5)과 다릅니다. 모든 프레임을 같은 크기로 저장하세요.").arg(id).arg(s.width()).arg(s.height()).arg(size.width()).arg(size.height()), true); return false; }
  }
  Json frames = Json::array();
  for (const QString& id : ids) frames.push_back(ss(id));
  int si;
  const cutout::Attachment* att;
  Json op;
  if (meshTarget(si, att, op) && !att->pictureless()) {
    op["op"] = "setSequence";
    op["frames"] = frames;
    op["width"] = size.width();
    op["height"] = size.height();
    if (setupIndex >= 0) op["setupIndex"] = setupIndex;
    if (!runOps({op}, false)) return false;
    setStatus(QString("「%1」: 프레임 %2장. 애니메이션 모드에서 「시퀀스 키」로 재생 방식·간격을 정하세요. 되돌리기: Ctrl+Z").arg(refId(selection_)).arg(ids.size()));
    return true;
  }
  // No part selected: a new part on the selected bone (or the first bone) showing the frames, one edit.
  const QString bone = !selectedBone().isEmpty() ? selectedBone() : rig_ && !rig_->bones.empty() ? qs(rig_->bones[0].id) : QString();
  if (bone.isEmpty()) { setStatus("시퀀스 조각을 붙일 본이 없습니다.", true); return false; }
  const QString base = qs(cutout::slugId(ss(ids[0]), "sequence"));
  QString slot = base;
  for (int n = 2; rig_->slotIndex(ss(slot)) >= 0; n++) slot = QString("%1-%2").arg(base).arg(n);
  const QString before = selection_;
  selection_ = "slot:" + slot;
  const bool ok = runOps({{{"op", "addPart"}, {"slot", ss(slot)}, {"bone", ss(bone)}, {"imageAssetId", ss(ids[0])}, {"width", size.width()}, {"height", size.height()}},
                          {{"op", "setSequence"}, {"slot", ss(slot)}, {"frames", frames}}}, false);
  if (!ok) { selection_ = before; return false; }
  setStatus(QString("본 「%1」에 시퀀스 조각 「%2」(프레임 %3장)를 만들었습니다. 되돌리기: Ctrl+Z").arg(bone, slot).arg(ids.size()));
  return true;
}
bool CutoutPanel::moveSequenceFrame(int from, int to) {
  int si;
  const cutout::Attachment* att;
  Json fields;
  if (!meshTarget(si, att, fields) || !att->sequence()) { setStatus("시퀀스 조각을 고르세요.", true); return false; }
  const int n = int(att->frames.size());
  if (from < 0 || from >= n || to < 0 || to >= n || from == to) return false;
  QStringList frames;
  for (const auto& f : att->frames) frames << qs(f);
  // The setup frame stays the same picture wherever it moves.
  const QString setupFrame = frames[att->setupIndex];
  frames.move(from, to);
  if (!setSequenceFrames(frames, int(frames.indexOf(setupFrame)))) return false;
  sequenceFrames_->setCurrentRow(to);
  return true;
}
bool CutoutPanel::removeSequenceFrame(int index) {
  int si;
  const cutout::Attachment* att;
  Json fields;
  if (!meshTarget(si, att, fields) || !att->sequence()) { setStatus("시퀀스 조각을 고르세요.", true); return false; }
  if (index < 0 || index >= int(att->frames.size())) { setStatus("뺄 프레임을 목록에서 고르세요.", true); return false; }
  if (att->frames.size() == 1) { setStatus("마지막 프레임은 뺄 수 없습니다. 「시퀀스 해제」를 쓰세요.", true); return false; }
  QStringList frames;
  for (const auto& f : att->frames) frames << qs(f);
  frames.removeAt(index);
  const int setup = att->setupIndex > index ? att->setupIndex - 1 : std::min(att->setupIndex, int(frames.size()) - 1);
  return setSequenceFrames(frames, setup);
}
bool CutoutPanel::clearSequence() {
  if (mode_ != Mode::Setup) { setStatus("시퀀스 해제는 조립 모드에서 합니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op) || !att->sequence()) { setStatus("시퀀스 조각을 고르세요.", true); return false; }
  op["op"] = "setSequence";
  op["frames"] = nullptr;
  return runOps({op}, false);
}
bool CutoutPanel::setSequenceSetupFrame(int index) {
  int si;
  const cutout::Attachment* att;
  Json fields;
  if (!meshTarget(si, att, fields) || !att->sequence()) { setStatus("시퀀스 조각을 고르세요.", true); return false; }
  if (index == att->setupIndex) return false;
  QStringList frames;
  for (const auto& f : att->frames) frames << qs(f);
  return setSequenceFrames(frames, index);
}
bool CutoutPanel::keySequence() {
  return keySequence(sequenceMode_->currentData().toString(), sequenceIndex_->value(), sequenceDelay_->value());
}
bool CutoutPanel::keySequence(const QString& mode, int index, double delayMs) {
  if (mode_ != Mode::Animate || clipId_.isEmpty()) { setStatus("시퀀스 키는 애니메이션 모드에서 클립에 넣습니다.", true); return false; }
  if (refusePending()) return false;
  const QString slot = sequenceSlot();
  if (slot.isEmpty()) { setStatus("시퀀스 조각을 먼저 고르세요.", true); return false; }
  if (!runOps({{{"op", "setKey"}, {"clip", ss(clipId_)}, {"target", ss("slot:" + slot)}, {"property", "sequence"}, {"timeMs", playheadMs_},
                {"mode", ss(mode)}, {"index", index}, {"delay", delayMs}}}, false))
    return false;
  setStatus(QString("「%1」 %2ms: 시퀀스 키(%3, %4번부터, %5 ms). 되돌리기: Ctrl+Z").arg(slot).arg(playheadMs_).arg(mode).arg(index).arg(delayMs));
  return true;
}
bool CutoutPanel::deleteSequenceKey() {
  if (mode_ != Mode::Animate || clipId_.isEmpty()) { setStatus("시퀀스 키는 애니메이션 모드에서 지웁니다.", true); return false; }
  if (refusePending()) return false;
  const QString slot = sequenceSlot();
  if (slot.isEmpty()) { setStatus("시퀀스 조각을 먼저 고르세요.", true); return false; }
  return runOps({{{"op", "deleteKey"}, {"clip", ss(clipId_)}, {"target", ss("slot:" + slot)}, {"property", "sequence"}, {"timeMs", playheadMs_}}}, false);
}
}  // namespace cutout_studio
