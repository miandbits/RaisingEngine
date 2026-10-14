#include "cutout-panel.hpp"
#include <QtWidgets>
#include <climits>
#include <cmath>
#include <set>
#include <tuple>
#include "cutout/host.hpp"
#include "cutout/templates.hpp"
#include "dopesheet.hpp"
#include "graph-editor.hpp"

namespace cutout_studio {
using cutout::Json;
namespace {
const char* PROPS[] = {"x", "y", "rotation", "scaleX", "scaleY", "pivotX", "pivotY", "alpha"};
QString refKind(const QString& ref) { return ref.section(':', 0, 0); }
QString refId(const QString& ref) { return ref.section(':', 1); }
const Json* findIn(const Json& data, const char* list, const std::string& id) {
  if (!data.is_object() || !data.contains(list) || !data.at(list).is_array()) return nullptr;
  for (const auto& x : data.at(list)) if (x.is_object() && x.value("id", std::string()) == id) return &x;
  return nullptr;
}
const Json& listOf(const Json& data, const char* key) {
  static const Json empty = Json::array();
  return data.is_object() && data.contains(key) && data.at(key).is_array() ? data.at(key) : empty;
}
Json batch(const std::vector<Json>& ops) { return ops.size() == 1 ? ops[0] : Json{{"op", "batch"}, {"operations", ops}}; }
QString capital(const QString& s) { return s.left(1).toUpper() + s.mid(1); }
const char* NUMERIC_PROPS[] = {"x", "y", "rotation", "scaleX", "scaleY", "alpha"};
QString propertyLabel(const QString& property) {
  static const QMap<QString, QString> names = {{"x", "X"}, {"y", "Y"}, {"rotation", "회전"}, {"scaleX", "크기 X"}, {"scaleY", "크기 Y"}, {"alpha", "투명도"}, {"attachment", "그림"}};
  return names.value(property, property);
}
std::string interpOf(const Json& key) { return key.contains("interp") && key.at("interp").is_string() ? key.at("interp").get<std::string>() : "linear"; }
cutout::Curve curveOf(const Json& key) {
  cutout::Curve c = cutout::LINEAR_CURVE;
  if (key.contains("curve") && key.at("curve").is_array() && key.at("curve").size() == 4)
    for (size_t i = 0; i < 4; i++) if (key.at("curve")[i].is_number()) c[i] = key.at("curve")[i].get<double>();
  return c;
}
QMap<QString, QString> parsePairs(const QString& text) {
  QMap<QString, QString> out;
  for (const QString& part : text.split(',', Qt::SkipEmptyParts)) {
    const int eq = part.indexOf('=');
    if (eq > 0) out.insert(part.left(eq).trimmed(), part.mid(eq + 1).trimmed());
  }
  return out;
}
QString joinPairs(const QJsonObject& o) {
  QStringList parts;
  for (auto it = o.begin(); it != o.end(); ++it) parts << it.key() + "=" + it.value().toString();
  return parts.join(", ");
}
QStringList idsOf(const Json& data, const char* list) {
  QStringList out;
  for (const auto& x : listOf(data, list)) out << qs(x.value("id", std::string()));
  return out;
}
QString uniqueId(const Json& data, const char* list, const QString& base) {
  const QStringList ids = idsOf(data, list);
  QString id = base;
  for (int n = 2; ids.contains(id); n++) id = QString("%1-%2").arg(base).arg(n);
  return id;
}
/** Names a slot can show: default attachments first, then names only skins provide. */
QStringList slotNames(const Json& data, const std::string& slot) {
  QStringList out;
  for (const auto& a : listOf(data, "attachments")) if (a.value("slot", std::string()) == slot) out << qs(a.value("name", std::string()));
  for (const auto& s : listOf(data, "skins"))
    for (const auto& a : listOf(s, "attachments"))
      if (a.value("slot", std::string()) == slot && !out.contains(qs(a.value("name", std::string())))) out << qs(a.value("name", std::string()));
  return out;
}
}  // namespace

const QVector<SignalInfo>& signalVocabulary() {
  static const QVector<SignalInfo> v = {
    {"actionStarted", "행동 시작", {"actionId"}},
    {"actionResult", "행동 결과", {"actionId", "outcome"}},
    {"activityStarted", "활동 시작", {"activityId"}},
    {"activityResult", "활동 결과", {"activityId", "outcome"}},
    {"dialogueLine", "대사", {"speakerId", "nodeId", "self"}},
    {"dialogueEnded", "대화 끝", {"speakerId", "self"}},
    {"portraitChanged", "표정·초상 변경", {"characterId", "portraitId", "position", "self"}},
    {"phaseChanged", "진행 단계 변경", {"phase"}},
  };
  return v;
}

// Tree whose drops are turned into core reparent ops instead of moving items locally.
class BoneTree : public QTreeWidget {
 public:
  using QTreeWidget::QTreeWidget;
  std::function<void(const QString& dragged, const QString& targetBone)> onDrop;

 protected:
  void dropEvent(QDropEvent* e) override {
    QTreeWidgetItem* source = currentItem();
    QTreeWidgetItem* target = itemAt(e->position().toPoint());
    const auto where = dropIndicatorPosition();
    e->setDropAction(Qt::IgnoreAction);
    e->accept();
    if (!source || !onDrop) return;
    // Above/below an item means "sibling of it", i.e. child of its parent bone.
    if (target && where != QAbstractItemView::OnItem) target = target->parent();
    QString bone;
    for (auto* t = target; t; t = t->parent())
      if (t->data(0, Qt::UserRole).toString().startsWith("bone:")) { bone = refId(t->data(0, Qt::UserRole).toString()); break; }
    onDrop(source->data(0, Qt::UserRole).toString(), bone);
  }
};

// Image part picker: project images (or import a PNG), slot id, attachment name, bone.
class PartDialog : public QDialog {
 public:
  PartDialog(QWidget* parent, ImageCache& images, const QStringList& bones, const QStringList& slots, CutoutPanel::PartChoice& choice)
      : QDialog(parent), choice_(choice) {
    setObjectName("cutoutPartDialog");
    setWindowTitle("이미지 조각 추가");
    auto* layout = new QVBoxLayout(this);
    assets_ = new QListWidget;
    assets_->setObjectName("cutoutPartAssetList");
    assets_->setIconSize(QSize(40, 40));
    for (const QString& id : images.imageAssetIds()) {
      auto* item = new QListWidgetItem(QIcon(images.path(id)), id, assets_);
      item->setData(Qt::UserRole, id);
      const QSize size = images.pixelSize(id);
      item->setToolTip(images.path(id) + (size.isValid() ? QString("\n%1×%2").arg(size.width()).arg(size.height()) : QString()));
    }
    auto* import = new QPushButton("PNG 가져오기…");
    import->setObjectName("cutoutPartImport");
    auto* form = new QFormLayout;
    slot_ = new QComboBox;
    slot_->setObjectName("cutoutPartSlot");
    slot_->setEditable(true);
    slot_->addItems(slots);
    slot_->setCurrentText(choice.slot);
    name_ = new QLineEdit(choice.name);
    name_->setObjectName("cutoutPartName");
    name_->setPlaceholderText("비우면 자동 (표정 교체용이면 open, closed 처럼)");
    bone_ = new QComboBox;
    bone_->setObjectName("cutoutPartBone");
    bone_->addItems(bones);
    bone_->setCurrentText(choice.bone);
    form->addRow("조각 ID", slot_);
    form->addRow("첨부 이름", name_);
    form->addRow("붙일 본", bone_);
    auto* hint = new QLabel("이미 있는 조각 ID를 고르면 같은 자리에 바꿔 끼울 그림(눈 감기·입 모양)으로 추가됩니다.");
    hint->setWordWrap(true);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(new QLabel("프로젝트 이미지"));
    layout->addWidget(assets_, 1);
    layout->addWidget(import);
    layout->addLayout(form);
    layout->addWidget(hint);
    layout->addWidget(buttons);
    connect(assets_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
      if (item && (slot_->currentText().isEmpty() || autoSlot_)) {
        slot_->setCurrentText(qs(cutout::slugId(ss(item->text()), "part")));
        autoSlot_ = true;
      }
    });
    connect(slot_, &QComboBox::editTextChanged, this, [this] { if (!assets_->signalsBlocked()) autoSlot_ = false; });
    connect(import, &QPushButton::clicked, this, [this] {
      const QString file = QFileDialog::getOpenFileName(this, "PNG 가져오기", QString(), "PNG 이미지 (*.png)");
      if (file.isEmpty()) return;
      choice_.importFile = file;
      if (slot_->currentText().isEmpty()) slot_->setCurrentText(qs(cutout::slugId(ss(QFileInfo(file).completeBaseName()), "part")));
      finish();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
      if (!assets_->currentItem()) { QMessageBox::information(this, windowTitle(), "이미지를 고르거나 PNG를 가져오세요."); return; }
      choice_.assetId = assets_->currentItem()->data(Qt::UserRole).toString();
      finish();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    resize(420, 520);
  }

 private:
  void finish() {
    choice_.slot = slot_->currentText().trimmed();
    choice_.name = name_->text().trimmed();
    choice_.bone = bone_->currentText();
    accept();
  }
  CutoutPanel::PartChoice& choice_;
  QListWidget* assets_;
  QComboBox *slot_, *bone_;
  QLineEdit* name_;
  bool autoSlot_ = true;
};

// ============================================================================
CutoutPanel::CutoutPanel(raising::StudioHost* host, QWidget* parent) : QWidget(parent), host_(host) {
  setObjectName("cutoutPanel");
  onion_ = OnionSettings::load();
  askText = [this](const QString& title, const QString& label, const QString& value) {
    bool ok = false;
    const QString text = QInputDialog::getText(this, title, label, QLineEdit::Normal, value, &ok);
    return ok ? text.trimmed() : QString();
  };
  askConfirm = [this](const QString& title, const QString& text) {
    return QMessageBox::question(this, title, text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
  };
  askPart = [this](PartChoice& choice) {
    QStringList bones, slots;
    const Json data = displayData();
    for (const auto& b : listOf(data, "bones")) bones << qs(b.value("id", std::string()));
    for (const auto& s : listOf(data, "slots")) slots << qs(s.value("id", std::string()));
    PartDialog dialog(this, images_, bones, slots, choice);
    return dialog.exec() == QDialog::Accepted;
  };
  timer_ = new QTimer(this);
  timer_->setInterval(16);
  connect(timer_, &QTimer::timeout, this, [this] { tick(); });
  buildUi();
}
CutoutPanel::~CutoutPanel() = default;
void CutoutPanel::detachHost() {
  timer_->stop();
  playing_ = false;
  host_ = nullptr;
}

// ---------------------------------------------------------------- UI build
namespace {
constexpr const char* LAYOUT_GROUP = "plugins/cutout-animation/layout";
/** Tool row that never forces the panel wider: what does not fit goes behind its » button. */
QToolBar* toolRow(const char* name) {
  auto* bar = new QToolBar;
  bar->setObjectName(name);
  bar->setMovable(false);
  bar->setFloatable(false);
  bar->setIconSize(QSize(16, 16));
  bar->setToolButtonStyle(Qt::ToolButtonTextOnly);
  bar->layout()->setContentsMargins(0, 0, 0, 0);
  return bar;
}
QWidget* spacer() {
  auto* w = new QWidget;
  w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  return w;
}
}  // namespace

// Spine-like single layout, the same in the host workspace and in an old host's dock:
//   mode bar (always visible)
//   [ left: 계층 / 그리기 순서 / 캐릭터 | center: tools + canvas | right: 속성 · 키 / 스킨 · 표정 / 진단 ]
//   [ bottom tabs: 애니메이션 (clips | transport + dopesheet/graph | mix) · 게임 연결 ]
// Every pane is a splitter child with a small minimum: lists scroll and tool rows overflow
// instead of imposing a large minimum height/width on the window.
void CutoutPanel::buildUi() {
  // Compact rows (cascades over the host theme): a 1280x720 window must hold canvas, side
  // panels and the timeline at their minimum sizes.
  setStyleSheet("QToolBar{padding:0;spacing:3px;border:0}"
                "QToolBar QPushButton,QToolBar QToolButton,#cutoutClipsPane QPushButton,#cutoutMixPane QPushButton{padding:3px 8px}"
                "QToolBar QSpinBox,QToolBar QComboBox{padding:2px;min-height:18px}"
                "QToolButton[cutoutSection=\"true\"]{padding:2px 4px;font-weight:bold}"
                "QTabBar::tab{padding:4px 12px}");
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(4, 4, 4, 4);
  root->setSpacing(4);
  // Mode bar with a strong color banner so the user always knows whether edits make keys.
  auto* modeBar = new QWidget;
  modeBar->setObjectName("cutoutModeBar");
  auto* modeLayout = new QHBoxLayout(modeBar);
  modeLayout->setContentsMargins(0, 0, 0, 0);
  setupButton_ = new QPushButton("조립 (Setup)");
  setupButton_->setObjectName("cutoutModeSetup");
  animateButton_ = new QPushButton("애니메이션 (Animate)");
  animateButton_->setObjectName("cutoutModeAnimate");
  for (auto* b : {setupButton_, animateButton_}) { b->setCheckable(true); b->setMinimumHeight(28); }
  auto* group = new QButtonGroup(this);
  group->setExclusive(true);
  group->addButton(setupButton_);
  group->addButton(animateButton_);
  connect(setupButton_, &QPushButton::clicked, this, [this] { setMode(Mode::Setup); });
  connect(animateButton_, &QPushButton::clicked, this, [this] { setMode(Mode::Animate); });
  banner_ = new QLabel;
  banner_->setObjectName("cutoutModeBanner");
  banner_->setMinimumHeight(28);
  banner_->setWordWrap(true);
  // Ignored width: a long banner wraps instead of widening the whole workspace.
  banner_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  fullViewButton_ = new QToolButton;
  fullViewButton_->setObjectName("cutoutFullView");
  fullViewButton_->setText("캔버스 크게 (Tab)");
  fullViewButton_->setCheckable(true);
  fullViewButton_->setToolTip("왼쪽·오른쪽 패널을 숨겨 캔버스를 넓힙니다. 캔버스·도프시트·그래프에서 Tab 키로도 전환합니다.");
  connect(fullViewButton_, &QToolButton::toggled, this, [this](bool on) { setFullView(on); });
  modeLayout->addWidget(setupButton_);
  modeLayout->addWidget(animateButton_);
  modeLayout->addWidget(banner_, 1);
  modeLayout->addWidget(fullViewButton_);
  root->addWidget(modeBar);

  mainSplit_ = new QSplitter(Qt::Vertical);
  mainSplit_->setObjectName("cutoutMainSplitter");
  mainSplit_->setChildrenCollapsible(false);
  mainSplit_->addWidget(buildWorkspace());
  bottomTabs_ = new QTabWidget;
  bottomTabs_->setObjectName("cutoutBottomTabs");
  bottomTabs_->addTab(buildAnimateTab(), "애니메이션");
  bottomTabs_->addTab(buildControllerTab(), "게임 연결");
  connect(bottomTabs_, &QTabWidget::currentChanged, this, [this](int index) {
    if (index == 1) ensurePreview();
    if (index == 1 || playing_) { lastTick_ = QDateTime::currentMSecsSinceEpoch(); timer_->start(); }
    else timer_->stop();
    updateCanvas();
  });
  mainSplit_->addWidget(bottomTabs_);
  // Both stretch: extra height is shared in the current ratio (timeline rows + dopesheet +
  // graph need about as much as the canvas), so tall screens grow both.
  mainSplit_->setStretchFactor(0, 3);
  mainSplit_->setStretchFactor(1, 2);
  mainSplit_->setSizes({600, 400});
  root->addWidget(mainSplit_, 1);
  statusLabel_ = new QLabel;
  statusLabel_->setObjectName("cutoutStatus");
  statusLabel_->setWordWrap(true);
  statusLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  root->addWidget(statusLabel_);
  auto shortcut = [this](const QKeySequence& key, const char* name, std::function<void()> fn) {
    auto* s = new QShortcut(key, this);
    s->setObjectName(name);
    s->setContext(Qt::WidgetWithChildrenShortcut);
    connect(s, &QShortcut::activated, this, fn);
  };
  shortcut(Qt::Key_V, "cutoutShortcutMove", [this] { setTool(RigCanvas::Move); });
  shortcut(Qt::Key_R, "cutoutShortcutRotate", [this] { setTool(RigCanvas::Rotate); });
  shortcut(Qt::Key_S, "cutoutShortcutScale", [this] { setTool(RigCanvas::Scale); });
  shortcut(Qt::Key_P, "cutoutShortcutPivot", [this] { setTool(RigCanvas::Pivot); });
  shortcut(Qt::Key_K, "cutoutShortcutKey", [this] { keySelected(); });
  shortcut(Qt::Key_O, "cutoutShortcutOnion", [this] { setOnionSkin(!onion_.enabled); });
  shortcut(Qt::Key_Space, "cutoutShortcutPlay", [this] { if (mode_ == Mode::Animate) playButton_->click(); });
  shortcut(Qt::Key_Comma, "cutoutShortcutStepBack", [this] { if (mode_ == Mode::Animate) stepPlayhead(-1); });
  shortcut(Qt::Key_Period, "cutoutShortcutStepForward", [this] { if (mode_ == Mode::Animate) stepPlayhead(1); });
  // Tab = full view, but only where Tab has no text-entry meaning (form fields keep focus
  // navigation): an event filter sees the key before QWidget's focus-chain handling.
  for (QWidget* w : {static_cast<QWidget*>(canvas_), static_cast<QWidget*>(dopesheet_), static_cast<QWidget*>(graph_)}) w->installEventFilter(this);
  restoreLayout();
  for (QSplitter* s : splitters()) connect(s, &QSplitter::splitterMoved, this, [this] { saveLayout(); });
  refreshModeUi();
  syncOnionUi();
}

QList<QSplitter*> CutoutPanel::splitters() const {
  return findChildren<QSplitter*>(QRegularExpression("^cutout.*Splitter$"));
}
QWidget* CutoutPanel::section(const QString& title, const char* name, QWidget* body) {
  auto* box = new QWidget;
  box->setObjectName(name);
  auto* layout = new QVBoxLayout(box);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(2);
  auto* head = new QToolButton;
  head->setObjectName(QString(name) + "Toggle");
  head->setText(title);
  head->setCheckable(true);
  head->setChecked(true);
  head->setAutoRaise(true);
  head->setArrowType(Qt::DownArrow);
  head->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  head->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  head->setToolTip("눌러서 접거나 펼칩니다. 접힌 구역의 공간은 다른 구역이 씁니다.");
  head->setProperty("cutoutSection", true);
  layout->addWidget(head);
  layout->addWidget(body, 1);
  connect(head, &QToolButton::toggled, this, [this, box, head, body](bool open) {
    body->setVisible(open);
    head->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    // A splitter honours maximum sizes: a collapsed section shrinks to its header.
    box->setMaximumHeight(open ? QWIDGETSIZE_MAX : head->sizeHint().height());
    if (!restoringLayout_) saveLayout();
  });
  sections_ << head;
  return box;
}
void CutoutPanel::saveLayout() const {
  QSettings q;
  q.beginGroup(LAYOUT_GROUP);
  for (QSplitter* s : splitters()) q.setValue(s->objectName(), s->saveState());
  QStringList collapsed;
  for (QToolButton* head : sections_) if (!head->isChecked()) collapsed << head->objectName();
  q.setValue("collapsed", collapsed);
}
void CutoutPanel::restoreLayout() {
  restoringLayout_ = true;
  QSettings q;
  q.beginGroup(LAYOUT_GROUP);
  for (QSplitter* s : splitters())
    if (q.contains(s->objectName())) s->restoreState(q.value(s->objectName()).toByteArray());
  const QStringList collapsed = q.value("collapsed").toStringList();
  for (QToolButton* head : sections_) head->setChecked(!collapsed.contains(head->objectName()));
  restoringLayout_ = false;
}
void CutoutPanel::setFullView(bool on) {
  // Editor-only presentation (never saved): the canvas takes the side panels' width.
  leftPane_->setVisible(!on);
  rightPane_->setVisible(!on);
  if (fullViewButton_->isChecked() != on) { QSignalBlocker block(fullViewButton_); fullViewButton_->setChecked(on); }
}
bool CutoutPanel::fullView() const { return leftPane_ && leftPane_->isHidden(); }
bool CutoutPanel::eventFilter(QObject* watched, QEvent* event) {
  // PNG/asset drops on the skin's attachment list go to the selected part of the previewed skin.
  if (skinAttachments_ && watched == skinAttachments_->viewport()) {
    if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) { static_cast<QDragMoveEvent*>(event)->acceptProposedAction(); return true; }
    if (event->type() == QEvent::Drop) {
      auto* drop = static_cast<QDropEvent*>(event);
      drop->acceptProposedAction();
      dropImages(drop->mimeData(), QString());
      return true;
    }
  }
  if (event->type() == QEvent::KeyPress) {
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_Tab && !(key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier))) {
      setFullView(!fullView());
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}
QImage CutoutPanel::thumbnail(const QSize& size) {
  QImage img(size, QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  if (!rig_ || size.isEmpty()) return img;
  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  p.setTransform(elementPlacement(*rig_, QRectF(QPointF(0, 0), QSizeF(size)), 1, false));
  drawRig(p, *rig_, currentPose(), images_, hidden_);
  return img;
}

QWidget* CutoutPanel::buildCharacters() {
  auto* body = new QWidget;
  auto* layout = new QVBoxLayout(body);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(2);
  characterList_ = new QTreeWidget;
  characterList_->setObjectName("cutoutCharacterList");
  characterList_->setHeaderLabels({"캐릭터 · 게임 연결"});
  characterList_->setHeaderHidden(true);  // the section title says it
  characterList_->setMinimumHeight(24);
  connect(characterList_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
    if (filling_ || !item) return;
    const QString ref = item->data(0, Qt::UserRole).toString();
    if (refKind(ref) == "rig") selectRig(refId(ref));
    else { selectController(refId(ref)); showControllerTab(); }
  });
  auto* buttons = toolRow("cutoutCharacterTools");
  auto button = [&](const QString& text, const char* name, std::function<bool()> fn) {
    auto* b = new QPushButton(text);
    b->setObjectName(name);
    connect(b, &QPushButton::clicked, this, [fn] { fn(); });
    buttons->addWidget(b);
    return b;
  };
  button("새 캐릭터", "cutoutNewCharacter", [this] { return newCharacter(); });
  button("이름 변경", "cutoutRenameCharacter", [this] { return renameCharacter(); });
  button("삭제", "cutoutDeleteCharacter", [this] { return deleteCharacter(); });
  layout->addWidget(characterList_, 1);
  layout->addWidget(buttons);
  return section("캐릭터 · 게임 연결", "cutoutSectionCharacters", body);
}

// Skins (outfits) and expressions: one tabbed section. Pages scroll so the section never forces
// the left pane taller than a 1280x720 window allows.
QWidget* CutoutPanel::buildLooks() {
  lookTabs_ = new QTabWidget;
  lookTabs_->setObjectName("cutoutLookTabs");
  lookTabs_->setDocumentMode(true);
  auto page = [](const char* name) {
    auto* inner = new QWidget;
    auto* layout = new QVBoxLayout(inner);
    layout->setContentsMargins(0, 2, 0, 0);
    layout->setSpacing(2);
    auto* scroll = new QScrollArea;
    scroll->setObjectName(name);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(inner);
    scroll->setMinimumHeight(30);
    return std::pair<QScrollArea*, QVBoxLayout*>{scroll, layout};
  };
  auto button = [this](QToolBar* row, const QString& text, const char* name, const QString& tip, std::function<bool()> fn) {
    auto* b = new QPushButton(text);
    b->setObjectName(name);
    b->setToolTip(tip);
    connect(b, &QPushButton::clicked, this, [fn] { fn(); });
    row->addWidget(b);
  };
  auto list = [](const char* name, const QString& tip) {
    auto* l = new QListWidget;
    l->setObjectName(name);
    l->setToolTip(tip);
    l->setMinimumHeight(24);
    l->setIconSize(QSize(20, 20));
    return l;
  };
  auto [skinPage, skins] = page("cutoutSkinPage");
  skinList_ = list("cutoutSkinList", "고른 스킨(의상)이 캔버스 미리 보기에 입혀집니다(편집기 전용, 저장되지 않음).\n클립은 그림 이름만 키로 쓰므로 스킨을 바꿔도 같은 애니메이션이 그대로 재생됩니다.");
  connect(skinList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
    if (!filling_ && item) setPreviewSkin(item->data(Qt::UserRole).toString());
  });
  auto* skinTools = toolRow("cutoutSkinTools");
  button(skinTools, "새 스킨", "cutoutNewSkin", "빈 스킨을 만듭니다. 캔버스의 조각에 PNG를 끌어 놓으면 이 스킨의 그림이 됩니다.", [this] { return newSkin(); });
  button(skinTools, "복제", "cutoutDuplicateSkin", "고른 스킨을 복사합니다(색만 다른 의상 등).", [this] { return duplicateSkin(); });
  button(skinTools, "이름 변경", "cutoutRenameSkin", "스킨 ID를 바꿉니다. 게임 연결의 기본 스킨·연결 규칙도 함께 바뀝니다.", [this] { return renameSkin(); });
  button(skinTools, "삭제", "cutoutDeleteSkin", "고른 스킨을 삭제합니다(게임 연결에서 쓰는 곳도 함께 지움).", [this] { return deleteSkin(); });
  skinHint_ = new QLabel;
  skinHint_->setObjectName("cutoutSkinHint");
  skinHint_->setWordWrap(true);
  skinHint_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  skinHint_->setStyleSheet("color:#a7b4c2;");
  skinAttachments_ = list("cutoutSkinAttachments", "이 스킨이 바꾸는 그림 · PNG나 이미지를 여기에 끌어 놓으면 고른 조각에 들어갑니다.");
  skinAttachments_->setAcceptDrops(true);
  skinAttachments_->viewport()->setAcceptDrops(true);
  skinAttachments_->viewport()->installEventFilter(this);
  connect(skinAttachments_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
    if (!filling_ && item) selectItem("slot:" + item->data(Qt::UserRole).toString());
  });
  auto* attachmentTools = toolRow("cutoutSkinAttachmentTools");
  button(attachmentTools, "그림 지정…", "cutoutSkinAssign", "고른 조각에 이 스킨의 그림(프로젝트 이미지 또는 PNG)을 지정합니다.", [this] { return assignSkinImageInteractive(); });
  button(attachmentTools, "빼기", "cutoutSkinRemoveAttachment", "이 스킨에서 그림을 빼면 그 조각은 기본 첨부를 보여 줍니다.", [this] { return removeSkinAttachment(); });
  skins->addWidget(skinList_, 1);
  skins->addWidget(skinTools);
  skins->addWidget(skinHint_);
  skins->addWidget(skinAttachments_, 1);
  skins->addWidget(attachmentTools);

  auto [expressionPage, expressions] = page("cutoutExpressionPage");
  expressionList_ = list("cutoutExpressionList", "고른 표정을 캔버스에 미리 봅니다(편집기 전용). 표정은 기본 동작(트랙 0) 위, 레이어(눈깜빡임 등) 아래에 겹칩니다.");
  connect(expressionList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
    if (!filling_ && item) setPreviewExpression(item->data(Qt::UserRole).toString());
  });
  auto* expressionTools = toolRow("cutoutExpressionTools");
  button(expressionTools, "현재 상태로 표정 만들기", "cutoutCaptureExpression",
         "애니메이션 모드에서 눈·입 그림이나 본을 바꾼 뒤(K로 키를 넣기 전에) 누르면, 바뀐 부분만 새 표정이 됩니다.\n바꾼 것이 없으면 이 시간 자세가 기본 자세와 다른 부분을 담습니다.", [this] { return captureExpression(); });
  button(expressionTools, "다시 담기", "cutoutRecaptureExpression", "고른 표정을 지금 상태로 덮어씁니다(이름 유지).", [this] { return recaptureExpression(); });
  button(expressionTools, "이름 변경", "cutoutRenameExpression", "표정 ID를 바꿉니다. 표정 키와 게임 연결도 함께 바뀝니다.", [this] { return renameExpression(); });
  button(expressionTools, "삭제", "cutoutDeleteExpression", "고른 표정과 그 표정 키를 삭제합니다(게임 연결에서 쓰는 곳도 함께 지움).", [this] { return deleteExpression(); });
  expressionTable_ = new QTableWidget(0, 2);
  expressionTable_->setObjectName("cutoutExpressionTable");
  expressionTable_->setHorizontalHeaderLabels({"대상", "표정 값"});
  expressionTable_->horizontalHeader()->setStretchLastSection(true);
  expressionTable_->verticalHeader()->hide();
  expressionTable_->setMinimumHeight(40);
  expressionTable_->setToolTip("조각: 표정이 바꿀 그림(— = 표정에 넣지 않음, 숨김 = 감춤) · 본: rotation=10, y=-3 처럼 덮을 값(비우면 뺌)");
  expressions->addWidget(expressionList_, 1);
  expressions->addWidget(expressionTools);
  expressions->addWidget(expressionTable_, 2);
  lookTabs_->addTab(skinPage, "스킨");
  lookTabs_->addTab(expressionPage, "표정");
  return section("스킨 · 표정", "cutoutSectionLooks", lookTabs_);
}

QWidget* CutoutPanel::buildWorkspace() {
  workspaceSplit_ = new QSplitter(Qt::Horizontal);
  workspaceSplit_->setObjectName("cutoutWorkspaceSplitter");
  workspaceSplit_->setChildrenCollapsible(false);
  // Left: hierarchy, draw order and characters stacked in collapsible sections.
  auto* treeBody = new QWidget;
  auto* treeLayout = new QVBoxLayout(treeBody);
  treeLayout->setContentsMargins(0, 0, 0, 0);
  treeLayout->setSpacing(2);
  tree_ = new BoneTree;
  tree_->setObjectName("cutoutBoneTree");
  tree_->setHeaderLabels({"본 · 조각", "보기"});
  tree_->setColumnWidth(0, 170);
  tree_->setMinimumHeight(36);
  tree_->setDragDropMode(QAbstractItemView::InternalMove);
  tree_->setToolTip("끌어서 놓으면 부모 본이 바뀝니다(화면 위치 유지). 보기 체크는 편집기에서만 숨깁니다.");
  tree_->onDrop = [this](const QString& dragged, const QString& bone) {
    if (refKind(dragged) == "bone") requestReparent(refId(dragged), bone);
    else if (refKind(dragged) == "slot" && !bone.isEmpty()) requestSlotBone(refId(dragged), bone);
  };
  connect(tree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
    if (!filling_ && item) selectItem(item->data(0, Qt::UserRole).toString());
  });
  connect(tree_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
    if (filling_ || column != 1) return;
    // Eye toggle: editor-only visibility, never written to the document.
    const bool visible = item->checkState(1) == Qt::Checked;
    std::function<void(QTreeWidgetItem*)> apply = [&](QTreeWidgetItem* it) {
      const QString ref = it->data(0, Qt::UserRole).toString();
      if (refKind(ref) == "slot") { if (visible) hidden_.remove(refId(ref)); else hidden_.insert(refId(ref)); }
      for (int i = 0; i < it->childCount(); i++) apply(it->child(i));
    };
    apply(item);
    updateCanvas();
    // itemChanged is emitted from inside item->setData(); rebuilding the tree right here
    // deletes that item while Qt is still using it (crash on an eye click). Rebuild next turn.
    QTimer::singleShot(0, this, [this] { refreshTree(); });
  });
  auto* treeButtons = toolRow("cutoutTreeTools");
  auto* addBoneButton = new QPushButton("본 추가");
  addBoneButton->setObjectName("cutoutAddBone");
  auto* addPartButton = new QPushButton("이미지 조각 추가…");
  addPartButton->setObjectName("cutoutAddPart");
  auto* deleteButton = new QPushButton("삭제");
  deleteButton->setObjectName("cutoutDeleteItem");
  connect(addBoneButton, &QPushButton::clicked, this, [this] { addBone(); });
  connect(addPartButton, &QPushButton::clicked, this, [this] { addPartInteractive(); });
  connect(deleteButton, &QPushButton::clicked, this, [this] { deleteSelected(); });
  treeButtons->addWidget(addBoneButton);
  treeButtons->addWidget(addPartButton);
  treeButtons->addWidget(deleteButton);
  treeLayout->addWidget(tree_, 1);
  treeLayout->addWidget(treeButtons);
  auto* orderBody = new QWidget;
  auto* orderLayout = new QVBoxLayout(orderBody);
  orderLayout->setContentsMargins(0, 0, 0, 0);
  orderLayout->setSpacing(2);
  drawOrder_ = new QListWidget;
  drawOrder_->setObjectName("cutoutDrawOrder");
  drawOrder_->setToolTip("그리는 순서: 위가 뒤, 아래가 앞입니다.");
  drawOrder_->setMinimumHeight(24);
  connect(drawOrder_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
    if (!filling_ && item) selectItem("slot:" + item->data(Qt::UserRole).toString());
  });
  auto* orderButtons = toolRow("cutoutDrawOrderTools");
  auto* up = new QPushButton("뒤로 ↑");
  up->setObjectName("cutoutDrawUp");
  auto* down = new QPushButton("앞으로 ↓");
  down->setObjectName("cutoutDrawDown");
  connect(up, &QPushButton::clicked, this, [this] { moveSelectedSlot(-1); });
  connect(down, &QPushButton::clicked, this, [this] { moveSelectedSlot(1); });
  orderButtons->addWidget(up);
  orderButtons->addWidget(down);
  orderLayout->addWidget(drawOrder_, 1);
  orderLayout->addWidget(orderButtons);
  leftSplit_ = new QSplitter(Qt::Vertical);
  leftSplit_->setObjectName("cutoutLeftSplitter");
  leftSplit_->setChildrenCollapsible(false);
  leftSplit_->addWidget(section("계층 (본 · 조각)", "cutoutSectionHierarchy", treeBody));
  leftSplit_->addWidget(section("그리는 순서 (뒤 → 앞)", "cutoutSectionDrawOrder", orderBody));
  leftSplit_->addWidget(buildCharacters());
  leftSplit_->setSizes({500, 300, 200});
  leftPane_ = leftSplit_;
  leftPane_->setMinimumWidth(170);

  // Center: tools + canvas.
  auto* middle = new QWidget;
  middle->setObjectName("cutoutCanvasPane");
  auto* middleLayout = new QVBoxLayout(middle);
  middleLayout->setContentsMargins(0, 0, 0, 0);
  middleLayout->setSpacing(2);
  auto* tools = toolRow("cutoutCanvasTools");
  const char* toolNames[] = {"cutoutToolSelect", "cutoutToolRotate", "cutoutToolScale", "cutoutToolPivot"};
  const char* toolLabels[] = {"선택/이동 (V)", "회전 (R)", "크기 (S)", "회전 중심 (P)"};
  auto* toolGroup = new QButtonGroup(this);
  for (int i = 0; i < 4; i++) {
    auto* b = new QToolButton;
    b->setObjectName(toolNames[i]);
    b->setText(toolLabels[i]);
    b->setCheckable(true);
    toolGroup->addButton(b, i);
    connect(b, &QToolButton::clicked, this, [this, i] { setTool(i); });
    tools->addWidget(b);
    toolButtons_[i] = b;
  }
  toolButtons_[0]->setChecked(true);
  auto* fit = new QToolButton;
  fit->setObjectName("cutoutFitView");
  fit->setText("화면 맞춤 (F)");
  tools->addSeparator();
  onionButton_ = new QToolButton;
  onionButton_->setObjectName("cutoutOnionSkin");
  onionButton_->setText("어니언 스킨 (O)");
  onionButton_->setCheckable(true);
  onionButton_->setToolTip("앞뒤 시간의 자세를 반투명하게 겹쳐 보여 줍니다(편집기 전용, 저장되지 않음).");
  connect(onionButton_, &QToolButton::toggled, this, [this](bool on) { if (!onionFilling_) setOnionSkin(on); });
  onionSettingsButton_ = new QToolButton;
  onionSettingsButton_->setObjectName("cutoutOnionSettingsButton");
  onionSettingsButton_->setText("▾");
  onionSettingsButton_->setToolTip("어니언 스킨 설정");
  trailButton_ = new QToolButton;
  trailButton_->setObjectName("cutoutMotionTrail");
  trailButton_->setText("궤적");
  trailButton_->setCheckable(true);
  trailButton_->setToolTip("선택한 본의 원점이 클립 동안 지나는 길(점 = 키 위치)");
  connect(trailButton_, &QToolButton::toggled, this, [this](bool on) {
    if (onionFilling_) return;
    OnionSettings s = onion_;
    s.trail = on;
    setOnionSettings(s);
  });
  onionPopup_ = buildOnionSettings();
  connect(onionSettingsButton_, &QToolButton::clicked, this, [this] {
    onionPopup_->adjustSize();
    onionPopup_->move(onionSettingsButton_->mapToGlobal(QPoint(0, onionSettingsButton_->height())));
    onionPopup_->show();
  });
  tools->addWidget(onionButton_);
  tools->addWidget(onionSettingsButton_);
  tools->addWidget(trailButton_);
  tools->addWidget(spacer());
  tools->addWidget(fit);
  canvas_ = new RigCanvas;
  canvas_->setImages(&images_);
  canvas_->onSelect = [this](const QString& ref) { selectItem(ref); };
  canvas_->originOf = [this](const QString& ref) { return manipulationOrigin(ref); };
  canvas_->onDrag = [this](const RigCanvas::Drag& d, bool finished) { onCanvasDrag(d, finished); };
  canvas_->onTool = [this](int t) { setTool(t); };
  canvas_->onKey = [this] { keySelected(); };
  canvas_->onOnion = [this] { setOnionSkin(!onion_.enabled); };
  canvas_->onDropImage = [this](const QMimeData* mime, const QString& slot) { dropImages(mime, slot); };
  connect(fit, &QToolButton::clicked, canvas_, &RigCanvas::fitToCanvas);
  middleLayout->addWidget(tools);
  middleLayout->addWidget(canvas_, 1);
  middle->setMinimumWidth(240);

  // Right: numeric properties + keying (scrolls on short screens), diagnostics below.
  auto* right = new QWidget;
  right->setObjectName("cutoutProperties");
  auto* rightLayout = new QVBoxLayout(right);
  rightLayout->setContentsMargins(0, 0, 4, 0);
  selectionLabel_ = new QLabel("선택 없음");
  selectionLabel_->setObjectName("cutoutSelectionLabel");
  selectionLabel_->setWordWrap(true);
  auto* form = new QFormLayout;
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  const char* labels[] = {"X", "Y", "회전°", "크기 X", "크기 Y", "이미지 기준점 X", "이미지 기준점 Y", "투명도"};
  for (int i = 0; i < 8; i++) {
    const QString key = PROPS[i];
    auto* s = new QDoubleSpinBox;
    s->setObjectName("cutoutProp" + capital(key));
    s->setKeyboardTracking(false);
    if (key == "x" || key == "y") { s->setRange(-100000, 100000); s->setDecimals(2); }
    else if (key == "rotation") { s->setRange(-36000, 36000); s->setDecimals(2); }
    else if (key.startsWith("scale")) { s->setRange(-100, 100); s->setDecimals(3); s->setSingleStep(0.01); }
    else if (key.startsWith("pivot")) { s->setRange(-4, 4); s->setDecimals(3); s->setSingleStep(0.05); }
    else { s->setRange(0, 1); s->setDecimals(3); s->setSingleStep(0.05); }
    connect(s, &QDoubleSpinBox::editingFinished, this, [this, key, s] { commitProperty(key, s->value()); });
    props_[key] = s;
    form->addRow(labels[i], s);
  }
  attachmentCombo_ = new QComboBox;
  attachmentCombo_->setObjectName("cutoutPropAttachment");
  attachmentCombo_->setToolTip("애니메이션 모드에서 바꾸면 그림 교체 키(눈 깜빡임·입 모양)가 됩니다.");
  attachmentCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  attachmentCombo_->setMinimumContentsLength(8);
  connect(attachmentCombo_, &QComboBox::activated, this, [this] { commitAttachmentChoice(); });
  form->addRow("첨부 그림", attachmentCombo_);
  keyButton_ = new QPushButton("◇ 키 추가 (K)");
  keyButton_->setObjectName("cutoutKeyButton");
  keyButton_->setMinimumHeight(30);
  connect(keyButton_, &QPushButton::clicked, this, [this] { keySelected(); });
  discardButton_ = new QPushButton("변경 취소");
  discardButton_->setObjectName("cutoutDiscardPending");
  connect(discardButton_, &QPushButton::clicked, this, [this] { discardPending(); });
  autoKeyCheck_ = new QCheckBox("자동 키");
  autoKeyCheck_->setObjectName("cutoutAutoKey");
  autoKeyCheck_->setToolTip("켜면 애니메이션 모드의 모든 변경이 곧바로 키가 됩니다.");
  connect(autoKeyCheck_, &QCheckBox::toggled, this, [this](bool on) { autoKey_ = on; refreshModeUi(); });
  deltaButton_ = new QPushButton("기존 키에도 같은 변화 적용");
  deltaButton_->setObjectName("cutoutApplySetupDelta");
  deltaButton_->setToolTip("조립 모드에서 바꾼 기본 자세 변화량을 이 본의 기존 키에 더합니다.");
  deltaButton_->hide();
  connect(deltaButton_, &QPushButton::clicked, this, [this] { applySetupDeltaToKeys(); });
  rightLayout->addWidget(selectionLabel_);
  rightLayout->addLayout(form);
  rightLayout->addWidget(keyButton_);
  auto* keyRow = new QHBoxLayout;
  keyRow->addWidget(autoKeyCheck_);
  keyRow->addWidget(discardButton_);
  rightLayout->addLayout(keyRow);
  rightLayout->addWidget(deltaButton_);
  rightLayout->addStretch();
  auto* propertiesScroll = new QScrollArea;
  propertiesScroll->setObjectName("cutoutPropertiesScroll");
  propertiesScroll->setWidgetResizable(true);
  propertiesScroll->setFrameShape(QFrame::NoFrame);
  propertiesScroll->setWidget(right);
  propertiesScroll->setMinimumHeight(60);
  diagnostics_ = new QListWidget;
  diagnostics_->setObjectName("cutoutDiagnostics");
  diagnostics_->setToolTip("검증 결과 · 클릭하면 해당 항목을 선택합니다.");
  diagnostics_->setMinimumHeight(40);
  diagnostics_->setTextElideMode(Qt::ElideMiddle);
  connect(diagnostics_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
    onDiagnosticClicked(item->data(Qt::UserRole).toString(), item->data(Qt::UserRole + 1).toString());
  });
  rightSplit_ = new QSplitter(Qt::Vertical);
  rightSplit_->setObjectName("cutoutRightSplitter");
  rightSplit_->setChildrenCollapsible(false);
  rightSplit_->addWidget(section("속성 · 키", "cutoutSectionProperties", propertiesScroll));
  // Skins/expressions sit by the properties (the look of the selected part); the left column
  // already holds three sections at the 1280x720 minimum.
  rightSplit_->addWidget(buildLooks());
  rightSplit_->addWidget(section("진단", "cutoutSectionDiagnostics", diagnostics_));
  rightSplit_->setSizes({520, 320, 160});
  rightPane_ = rightSplit_;
  rightPane_->setMinimumWidth(210);

  workspaceSplit_->addWidget(leftPane_);
  workspaceSplit_->addWidget(middle);
  workspaceSplit_->addWidget(rightPane_);
  // Only the canvas stretches: on a very wide screen the side panels keep their width.
  workspaceSplit_->setStretchFactor(0, 0);
  workspaceSplit_->setStretchFactor(1, 1);
  workspaceSplit_->setStretchFactor(2, 0);
  workspaceSplit_->setSizes({260, 1000, 290});
  return workspaceSplit_;
}

QWidget* CutoutPanel::buildAnimateTab() {
  auto* tab = new QWidget;
  tab->setObjectName("cutoutAnimateTab");
  auto* layout = new QHBoxLayout(tab);
  layout->setContentsMargins(2, 2, 2, 2);
  // Clips.
  auto* clipsPane = new QWidget;
  clipsPane->setObjectName("cutoutClipsPane");
  clipsPane->setMinimumWidth(140);
  auto* clips = new QVBoxLayout(clipsPane);
  clips->setContentsMargins(0, 0, 0, 0);
  clipList_ = new QListWidget;
  clipList_->setObjectName("cutoutClipList");
  clipList_->setMinimumHeight(32);
  connect(clipList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
    if (!filling_ && item) selectClip(item->data(Qt::UserRole).toString());
  });
  auto* clipButtons = new QGridLayout;
  auto add = [&](const QString& text, const char* name, int r, int c, std::function<bool()> fn) {
    auto* b = new QPushButton(text);
    b->setObjectName(name);
    connect(b, &QPushButton::clicked, this, [fn] { fn(); });
    clipButtons->addWidget(b, r, c);
    return b;
  };
  add("새 클립", "cutoutNewClip", 0, 0, [this] { return newClip(); });
  add("복제", "cutoutDuplicateClip", 0, 1, [this] { return duplicateClip(); });
  add("이름 변경", "cutoutRenameClip", 1, 0, [this] { return renameClip(); });
  add("삭제", "cutoutDeleteClip", 1, 1, [this] { return deleteClip(); });
  add("숨쉬기", "cutoutTemplateBreathing", 2, 0, [this] { return applyBreathing(); })->setToolTip("선택한 본(없으면 루트)에 숨쉬기 반복 클립을 만듭니다.");
  add("눈깜빡임", "cutoutTemplateBlink", 2, 1, [this] { return applyBlink(); })->setToolTip("선택한 눈 조각의 첨부(뜬 눈/감은 눈)를 바꾸는 반복 클립을 만들고 게임 연결 레이어에 등록합니다.");
  auto* clipForm = new QFormLayout;
  clipDuration_ = new QSpinBox;
  clipDuration_->setObjectName("cutoutClipDuration");
  clipDuration_->setRange(1, 600000);
  clipDuration_->setSuffix(" ms");
  clipDuration_->setKeyboardTracking(false);
  connect(clipDuration_, &QSpinBox::editingFinished, this, [this] { commitClipSettings(); });
  clipLoop_ = new QCheckBox("반복 재생");
  clipLoop_->setObjectName("cutoutClipLoop");
  connect(clipLoop_, &QCheckBox::clicked, this, [this] { commitClipSettings(); });
  clipForm->addRow("길이", clipDuration_);
  clipForm->addRow("", clipLoop_);
  clips->addWidget(new QLabel("클립"));
  clips->addWidget(clipList_, 1);
  clips->addLayout(clipButtons);
  clips->addLayout(clipForm);
  // Transport + dopesheet.
  auto* timelinePane = new QWidget;
  timelinePane->setObjectName("cutoutTimelinePane");
  timelinePane->setMinimumWidth(320);
  auto* timeline = new QVBoxLayout(timelinePane);
  timeline->setContentsMargins(0, 0, 0, 0);
  timeline->setSpacing(2);
  auto* transport = toolRow("cutoutTransportTools");
  auto* rewind = new QPushButton("⏮ 처음으로");
  rewind->setObjectName("cutoutRewind");
  playButton_ = new QPushButton("▶ 재생");
  playButton_->setObjectName("cutoutPlay");
  playButton_->setCheckable(true);
  loopPreview_ = new QCheckBox("반복");
  loopPreview_->setObjectName("cutoutLoopPreview");
  loopPreview_->setChecked(true);
  speedCombo_ = new QComboBox;
  speedCombo_->setObjectName("cutoutSpeed");
  for (double s : {0.25, 0.5, 1.0, 1.5, 2.0}) speedCombo_->addItem(QString("%1×").arg(s), s);
  speedCombo_->setCurrentIndex(2);
  timeLabel_ = new QLabel("0 ms");
  timeLabel_->setObjectName("cutoutTimeLabel");
  connect(rewind, &QPushButton::clicked, this, [this] { setPlayhead(0); });
  connect(playButton_, &QPushButton::toggled, this, [this](bool on) { togglePlay(on); });
  transport->addWidget(rewind);
  transport->addWidget(playButton_);
  transport->addWidget(loopPreview_);
  transport->addWidget(new QLabel("속도"));
  transport->addWidget(speedCombo_);
  QAction* transportStretch = transport->addWidget(spacer());  // the view buttons go before it
  transport->addWidget(timeLabel_);
  // One time view for both views: zoom, scroll, playhead, loop range and key selection.
  timeView_ = new TimeView(this);
  auto tool = [this](QWidget* w, const char* name, const QString& tip) {
    w->setObjectName(name);
    w->setToolTip(tip);
    timelineTools_ << w;
    return w;
  };
  auto toolButton = [&](const QString& text, const char* name, const QString& tip, std::function<void()> fn) {
    auto* b = new QPushButton(text);
    tool(b, name, tip);
    connect(b, &QPushButton::clicked, this, [fn] { fn(); });
    return b;
  };
  auto msField = [&](const char* name, int lo, int hi, int value, const QString& tip) {
    auto* s = new QSpinBox;
    s->setRange(lo, hi);
    s->setValue(value);
    s->setSuffix(" ms");
    s->setKeyboardTracking(false);
    tool(s, name, tip);
    return s;
  };
  // Precise time: numeric playhead, frame steps (, . or arrow keys), zoom, loop range.
  auto* timeRow = toolRow("cutoutTimeTools");
  timeField_ = msField("cutoutTimeField", 0, 600000, 0, "재생 헤드 시간(ms)을 정확히 입력합니다.");
  connect(timeField_, &QSpinBox::editingFinished, this, [this] { setPlayhead(timeField_->value()); });
  stepField_ = msField("cutoutStepSize", 1, 10000, 10, "◀ ▶ 버튼·쉼표/마침표·화살표 키가 옮기는 간격입니다.");
  timeRow->addWidget(new QLabel("시간"));
  timeRow->addWidget(timeField_);
  timeRow->addWidget(toolButton("◀", "cutoutStepBack", "이전 간격으로 (쉼표 , 또는 ←)", [this] { stepPlayhead(-1); }));
  timeRow->addWidget(toolButton("▶", "cutoutStepForward", "다음 간격으로 (마침표 . 또는 →)", [this] { stepPlayhead(1); }));
  timeRow->addWidget(new QLabel("간격"));
  timeRow->addWidget(stepField_);
  timeRow->addSeparator();
  timeRow->addWidget(toolButton("[ 구간 시작", "cutoutLoopIn", "재생 헤드를 미리 보기 반복 구간의 시작으로 (편집기 전용, 저장 안 됨)",
                                [this] { setLoopRange(playheadMs_, playheadMs_ < timeView_->loopOut() ? timeView_->loopOut() : -1); }));
  timeRow->addWidget(toolButton("구간 끝 ]", "cutoutLoopOut", "재생 헤드를 미리 보기 반복 구간의 끝으로 (눈금자의 초록 표시를 끌어도 됩니다)",
                                [this] { setLoopRange(playheadMs_ > timeView_->loopIn() ? timeView_->loopIn() : 0, playheadMs_); }));
  timeRow->addWidget(toolButton("구간 해제", "cutoutLoopClear", "미리 보기 반복 구간을 클립 전체로 되돌립니다.", [this] { clearLoopRange(); }));
  timeRow->addWidget(spacer());
  timeRow->addWidget(toolButton("−", "cutoutZoomOut", "시간 축소 (Ctrl+휠)", [this] { timeView_->zoomAround(timeView_->scrollMs() + timeView_->visibleMs() / 2, 0.8); }));
  timeRow->addWidget(toolButton("+", "cutoutZoomIn", "시간 확대 (Ctrl+휠)", [this] { timeView_->zoomAround(timeView_->scrollMs() + timeView_->visibleMs() / 2, 1.25); }));
  timeRow->addWidget(toolButton("맞춤", "cutoutTimeFit", "클립 전체가 보이도록 시간 축을 맞춥니다.", [this] { timeView_->fit(); }));
  // Key tools: clipboard, offset, timing scale, pose copy.
  auto* keyRow = toolRow("cutoutKeyTools");
  keyRow->addWidget(toolButton("복사", "cutoutCopyKeys", "고른 키를 복사합니다 (Ctrl+C).", [this] { copyKeys(); }));
  keyRow->addWidget(toolButton("붙여넣기", "cutoutPasteKeys", "복사한 키를 재생 헤드 시간에 붙여넣습니다 (Ctrl+V). 다른 클립에도 됩니다.\n한 대상만 복사했다면 지금 고른 본·조각에 붙습니다.", [this] { pasteKeys(); }));
  keyRow->addWidget(toolButton("복제", "cutoutDuplicateKeys", "고른 키를 재생 헤드 시간에 복제합니다 (Ctrl+D).", [this] { duplicateKeys(); }));
  keyRow->addSeparator();
  offsetField_ = msField("cutoutKeyOffset", -600000, 600000, 100, "고른 키 전부를 이만큼 옮깁니다(음수 = 앞으로).");
  keyRow->addWidget(offsetField_);
  keyRow->addWidget(toolButton("만큼 이동", "cutoutKeyOffsetApply", "고른 키를 간격을 유지한 채 옮깁니다. 다른 키와 겹치거나 클립 밖이면 거부됩니다.", [this] { offsetSelectedKeys(offsetField_->value()); }));
  scaleField_ = new QSpinBox;
  scaleField_->setRange(1, 100000);
  scaleField_->setValue(100);
  scaleField_->setSuffix(" %");
  scaleField_->setKeyboardTracking(false);
  tool(scaleField_, "cutoutKeyScale", "고른 키들의 시간 간격 배율(첫 키 기준). 선택 띠 양 끝 손잡이를 끌어도 됩니다(Alt: 재생 헤드 기준).");
  keyRow->addWidget(scaleField_);
  keyRow->addWidget(toolButton("간격 배율", "cutoutKeyScaleApply", "고른 키들의 시간 간격을 배율만큼 늘이거나 줄입니다.", [this] { scaleSelectedKeys(scaleField_->value() / 100.0); }));
  keyRow->addSeparator();
  auto* copyPoseButton = new QPushButton("포즈 복사");
  copyPoseButton->setObjectName("cutoutCopyPose");
  copyPoseButton->setToolTip("고른 본과 그 자식 본(없으면 모든 본)의 현재 자세를 복사합니다.");
  connect(copyPoseButton, &QPushButton::clicked, this, [this] { copyPose(); });
  keyRow->addWidget(copyPoseButton);
  keyRow->addWidget(toolButton("포즈 붙여넣기", "cutoutPastePose", "복사한 자세를 재생 헤드 시간에 키로 넣습니다(다른 클립에도 됩니다).", [this] { pastePose(); }));
  keyRow->addSeparator();
  keyRow->addWidget(toolButton("표정 키", "cutoutKeyExpression", "「스킨 · 표정」에서 고른 표정(없으면 표정 없음)을 재생 헤드 시간에 키로 넣습니다(계단 전환).\n도프시트의 「표정」 행에 보입니다.", [this] { keyExpression(); }));

  dopesheet_ = new Dopesheet;
  dopesheet_->setTimeView(timeView_);
  dopesheet_->onScrub = [this](int ms) { setPlayhead(ms); };
  dopesheet_->onMoveKeys = [this](const QVector<KeyRef>& keys, int delta) { return moveKeys(keys, delta); };
  dopesheet_->onScaleKeys = [this](const QVector<KeyRef>& keys, int pivot, double factor) { return scaleKeys(keys, pivot, factor); };
  dopesheet_->onDeleteKeys = [this](const QVector<KeyRef>& keys) { return deleteKeys(keys); };
  dopesheet_->onPreset = [this](const QString& preset) { applyPreset(preset); };
  dopesheet_->onCommand = [this](const QString& command) { keyCommand(command); };
  dopesheet_->onStep = [this](int steps) { stepPlayhead(steps); };
  dopesheet_->onRowSelected = [this](int row) {
    if (row < 0 || row >= dopesheet_->rows().size()) return;
    // The rig expression row has no canvas item: show the expression list instead.
    if (dopesheet_->rows()[row].target == "rig") { if (lookTabs_) lookTabs_->setCurrentIndex(1); return; }
    selectItem(dopesheet_->rows()[row].target);
  };
  auto* scroll = new QScrollArea;
  scroll->setObjectName("cutoutDopesheetScroll");
  scroll->setWidgetResizable(true);
  scroll->setWidget(dopesheet_);
  scroll->setMinimumHeight(32);
  // Graph editor below the dopesheet: same time view, so key columns line up at any zoom.
  auto* graphPanel = new QWidget;
  graphPanel->setObjectName("cutoutGraphPanel");
  auto* graphLayout = new QVBoxLayout(graphPanel);
  graphLayout->setContentsMargins(0, 0, 0, 0);
  graphLayout->setSpacing(2);
  auto* presetRow = toolRow("cutoutCurveTools");
  presetRow->addWidget(new QLabel("곡선"));
  for (const auto& preset : interpPresets()) {
    auto* b = new QPushButton(preset.label);
    b->setObjectName(QString("cutoutPreset") + preset.suffix);
    b->setToolTip(QString(preset.tip) + "\n고른 키(도프시트·그래프 공통 선택)부터 다음 키까지의 구간에 적용합니다.");
    const QString id = preset.id;
    connect(b, &QPushButton::clicked, this, [this, id] { applyPreset(id); });
    presetRow->addWidget(b);
    presetButtons_ << b;
  }
  presetRow->addWidget(spacer());
  graphNormalize_ = new QCheckBox("채널별 맞춤");
  graphNormalize_->setObjectName("cutoutGraphNormalize");
  graphNormalize_->setToolTip("값 범위가 다른 채널(회전 각도와 크기 등)을 각자 높이에 맞춰 모양을 비교합니다.");
  presetRow->addWidget(graphNormalize_);
  graph_ = new GraphEditor;
  graph_->setTimeView(timeView_);
  graph_->setToolTip("키 드래그: 위아래 = 값, 좌우 = 시간(Alt: 둘 다, Shift: 1ms) · 고른 키의 손잡이를 끌어 곡선 조절\n"
                     "Ctrl+클릭: 토글 · Shift+클릭: 추가 · 빈 곳 드래그: 상자 선택 · Ctrl+A: 모두 · 빈 곳 클릭: 시간 이동\n"
                     "Ctrl+휠: 시간 확대 · 휠/가운데 버튼 드래그: 시간 이동 · 오른쪽 클릭: 프리셋 · 범례 클릭: 채널 숨김");
  graph_->onScrub = [this](int ms) { setPlayhead(ms); };
  graph_->onMoveKey = [this](const GraphEditor::KeyRef& k, int to, double value) { return moveGraphKey(k.target, k.property, k.timeMs, to, value); };
  graph_->onMoveKeys = [this](const QVector<KeyRef>& keys, int delta, const QHash<KeyRef, double>& values) { return moveKeys(keys, delta, values); };
  graph_->onCurve = [this](const GraphEditor::KeyRef& k, const cutout::Curve& curve) { return setKeyCurve(k.target, k.property, k.timeMs, curve); };
  graph_->onPreset = [this](const QString& preset) { applyPreset(preset); };
  graph_->onDeleteKeys = [this](const QVector<KeyRef>& keys) { return deleteKeys(keys); };
  graph_->onCommand = [this](const QString& command) { keyCommand(command); };
  graph_->onStep = [this](int steps) { stepPlayhead(steps); };
  connect(graphNormalize_, &QCheckBox::toggled, graph_, &GraphEditor::setNormalized);
  graphLayout->addWidget(presetRow);
  graphLayout->addWidget(graph_, 1);
  // Short timeline areas: show the dopesheet, the graph, or both stacked (same time axis).
  auto* viewGroup = new QButtonGroup(this);
  viewGroup->setExclusive(true);
  for (const auto& [text, name, sheet, curves] : {std::tuple{"도프시트", "cutoutViewDopesheet", true, false},
                                                  std::tuple{"그래프", "cutoutViewGraph", false, true}, std::tuple{"둘 다", "cutoutViewBoth", true, true}}) {
    auto* b = new QPushButton(text);
    b->setObjectName(name);
    b->setCheckable(true);
    b->setChecked(sheet && curves);
    b->setToolTip("아래 영역에 도프시트, 값 곡선 그래프 또는 둘 다를 보여 줍니다.");
    viewGroup->addButton(b);
    transport->insertWidget(transportStretch, b);
    connect(b, &QPushButton::clicked, this, [scroll, graphPanel, sheet = sheet, curves = curves] {
      // Hiding the focused dopesheet makes QScrollArea "ensure visible" it (rows jump): keep the row scroll.
      const int rowsAt = scroll->verticalScrollBar()->value();
      scroll->setVisible(sheet);
      graphPanel->setVisible(curves);
      scroll->verticalScrollBar()->setValue(rowsAt);
    });
  }
  auto* timelineSplit = new QSplitter(Qt::Vertical);
  timelineSplit->setObjectName("cutoutTimelineSplitter");
  timelineSplit->addWidget(scroll);
  timelineSplit->addWidget(graphPanel);
  timelineSplit->setChildrenCollapsible(false);
  timelineSplit->setStretchFactor(0, 2);
  timelineSplit->setStretchFactor(1, 3);
  timelineSplit->setSizes({200, 300});  // both stretch: extra height keeps this ratio
  // Horizontal time scrollbar (units of 0.1 ms so deep zoom still scrolls smoothly).
  timeScroll_ = new QScrollBar(Qt::Horizontal);
  timeScroll_->setObjectName("cutoutTimeScroll");
  connect(timeScroll_, &QScrollBar::valueChanged, this, [this](int v) { timeView_->setScroll(v / 10.0); });
  connect(timeView_, &TimeView::changed, this, [this] { syncTimeScroll(); });
  timeline->addWidget(transport);
  timeline->addWidget(timeRow);
  timeline->addWidget(keyRow);
  timeline->addWidget(timelineSplit, 1);
  timeline->addWidget(timeScroll_);
  // The gesture list is the dopesheet tooltip: rows of help text would cost key rows.
  dopesheet_->setToolTip("클릭: 키 선택 · 겹친 키 묶음(숫자 표시)은 누를 때마다 다음 키, 더블클릭: 확대\n"
                         "Ctrl+클릭: 토글 · Shift+클릭: 추가 · 빈 곳 드래그: 상자 선택 · 행 이름 클릭: 행 전체 · Alt+클릭: 같은 시간 전체 · Ctrl+A: 모두\n"
                         "드래그: 고른 키 함께 이동(10ms, Shift 1ms) · 선택 띠 양 끝: 시간 간격 배율(Alt: 재생 헤드 기준)\n"
                         "Delete: 삭제 · Ctrl+C/V/D: 복사/붙여넣기/복제(재생 헤드) · ←/→ 또는 , .: 한 간격 이동\n"
                         "Ctrl+휠: 시간 확대 · Shift+휠/가운데 버튼 드래그: 시간 이동 · 눈금자: 재생 헤드(10ms, Shift 1ms), 초록 표시: 반복 구간");
  // Mix table.
  auto* mixPane = new QWidget;
  mixPane->setObjectName("cutoutMixPane");
  mixPane->setMinimumWidth(170);
  auto* mix = new QVBoxLayout(mixPane);
  mix->setContentsMargins(0, 0, 0, 0);
  mixDefault_ = new QSpinBox;
  mixDefault_->setObjectName("cutoutMixDefault");
  mixDefault_->setRange(0, 10000);
  mixDefault_->setSuffix(" ms");
  mixDefault_->setKeyboardTracking(false);
  connect(mixDefault_, &QSpinBox::editingFinished, this, [this] { commitMixTable(); });
  mixTable_ = new QTableWidget(0, 3);
  mixTable_->setObjectName("cutoutMixTable");
  mixTable_->setHorizontalHeaderLabels({"이전 클립", "다음 클립", "전환"});
  mixTable_->horizontalHeader()->setStretchLastSection(true);
  mixTable_->verticalHeader()->hide();
  mixTable_->setMinimumHeight(40);
  auto* mixButtons = new QHBoxLayout;
  auto* mixAdd = new QPushButton("전환 추가");
  mixAdd->setObjectName("cutoutMixAdd");
  auto* mixRemove = new QPushButton("전환 삭제");
  mixRemove->setObjectName("cutoutMixRemove");
  connect(mixAdd, &QPushButton::clicked, this, [this] { addMixPair(); });
  connect(mixRemove, &QPushButton::clicked, this, [this] { removeMixPair(); });
  mixButtons->addWidget(mixAdd);
  mixButtons->addWidget(mixRemove);
  auto* mixForm = new QFormLayout;
  mixForm->addRow("기본 전환", mixDefault_);
  mix->addWidget(new QLabel("클립 전환(크로스페이드)"));
  mix->addLayout(mixForm);
  mix->addWidget(mixTable_, 1);
  mix->addLayout(mixButtons);
  // clip list | dopesheet + graph | mix: only the time area stretches on wide screens.
  timelineAreaSplit_ = new QSplitter(Qt::Horizontal);
  timelineAreaSplit_->setObjectName("cutoutTimelineAreaSplitter");
  timelineAreaSplit_->setChildrenCollapsible(false);
  timelineAreaSplit_->addWidget(clipsPane);
  timelineAreaSplit_->addWidget(timelinePane);
  timelineAreaSplit_->addWidget(mixPane);
  timelineAreaSplit_->setStretchFactor(0, 0);
  timelineAreaSplit_->setStretchFactor(1, 1);
  timelineAreaSplit_->setStretchFactor(2, 0);
  timelineAreaSplit_->setSizes({190, 1000, 260});
  layout->addWidget(timelineAreaSplit_);
  return tab;
}

QWidget* CutoutPanel::buildControllerTab() {
  auto* tab = new QWidget;
  tab->setObjectName("cutoutControllerTab");
  auto* layout = new QVBoxLayout(tab);
  layout->setContentsMargins(2, 2, 2, 2);
  auto* top = toolRow("cutoutControllerTools");
  controllerCombo_ = new QComboBox;
  controllerCombo_->setObjectName("cutoutControllerSelect");
  connect(controllerCombo_, &QComboBox::activated, this, [this] { selectController(controllerCombo_->currentData().toString()); });
  defaultClipCombo_ = new QComboBox;
  defaultClipCombo_->setObjectName("cutoutControllerDefaultClip");
  connect(defaultClipCombo_, &QComboBox::activated, this, [this] { commitControllerFromUi(); });
  characterCombo_ = new QComboBox;
  characterCombo_->setObjectName("cutoutControllerCharacter");
  characterCombo_->setToolTip("이 캐릭터가 말하거나 표정이 바뀌는 신호에 self=true가 붙습니다.");
  connect(characterCombo_, &QComboBox::activated, this, [this] { commitControllerFromUi(); });
  top->addWidget(new QLabel("게임 연결"));
  top->addWidget(controllerCombo_);
  top->addWidget(new QLabel("기본(대기) 클립"));
  top->addWidget(defaultClipCombo_);
  top->addWidget(new QLabel("게임 인물"));
  top->addWidget(characterCombo_);
  defaultSkinCombo_ = new QComboBox;
  defaultSkinCombo_->setObjectName("cutoutControllerDefaultSkin");
  defaultSkinCombo_->setToolTip("새로 배치된 캐릭터의 스킨(의상). 화면 요소 속성 props.skin이 있으면 그것이 우선합니다.");
  connect(defaultSkinCombo_, &QComboBox::activated, this, [this] { commitControllerFromUi(); });
  defaultExpressionCombo_ = new QComboBox;
  defaultExpressionCombo_->setObjectName("cutoutControllerDefaultExpression");
  defaultExpressionCombo_->setToolTip("처음 표정. 연결 규칙(예: portraitChanged → 표정)이나 클립의 표정 키가 바꿉니다.");
  connect(defaultExpressionCombo_, &QComboBox::activated, this, [this] { commitControllerFromUi(); });
  top->addWidget(new QLabel("기본 스킨"));
  top->addWidget(defaultSkinCombo_);
  top->addWidget(new QLabel("기본 표정"));
  top->addWidget(defaultExpressionCombo_);
  auto* tables = new QHBoxLayout;
  auto* layers = new QVBoxLayout;
  layerTable_ = new QTableWidget(0, 2);
  layerTable_->setObjectName("cutoutLayerTable");
  layerTable_->setHorizontalHeaderLabels({"트랙", "항상 재생할 클립"});
  layerTable_->horizontalHeader()->setStretchLastSection(true);
  layerTable_->verticalHeader()->hide();
  auto* layerButtons = new QHBoxLayout;
  auto* layerAdd = new QPushButton("레이어 추가");
  layerAdd->setObjectName("cutoutLayerAdd");
  auto* layerRemove = new QPushButton("레이어 삭제");
  layerRemove->setObjectName("cutoutLayerRemove");
  connect(layerAdd, &QPushButton::clicked, this, [this] { addLayer(); });
  connect(layerRemove, &QPushButton::clicked, this, [this] { removeLayer(); });
  layerButtons->addWidget(layerAdd);
  layerButtons->addWidget(layerRemove);
  layers->addWidget(new QLabel("레이어 (트랙 1..15, 예: 눈깜빡임)"));
  layers->addWidget(layerTable_, 1);
  layers->addLayout(layerButtons);
  auto* bindings = new QVBoxLayout;
  bindingTable_ = new QTableWidget(0, 10);
  bindingTable_->setObjectName("cutoutBindingTable");
  bindingTable_->setHorizontalHeaderLabels({"ID", "게임 신호", "조건 (키=값, …)", "클립", "트랙", "반복", "끝나면", "중지 신호", "스킨", "표정"});
  bindingTable_->setToolTip("클립 없이 스킨·표정만 바꾸는 연결도 됩니다(트랙을 차지하지 않음).\n예: portraitChanged (portraitId=smile, self=true) → 표정 smile");
  bindingTable_->horizontalHeader()->setStretchLastSection(true);
  bindingTable_->verticalHeader()->hide();
  auto* bindingButtons = new QHBoxLayout;
  auto* bindingAdd = new QPushButton("연결 추가");
  bindingAdd->setObjectName("cutoutBindingAdd");
  auto* bindingRemove = new QPushButton("연결 삭제");
  bindingRemove->setObjectName("cutoutBindingRemove");
  connect(bindingAdd, &QPushButton::clicked, this, [this] { addBinding(); });
  connect(bindingRemove, &QPushButton::clicked, this, [this] { removeBinding(); });
  bindingButtons->addWidget(bindingAdd);
  bindingButtons->addWidget(bindingRemove);
  bindingButtons->addStretch();
  bindings->addWidget(new QLabel("게임 신호 → 클립 연결 (트랙마다 위에서 처음 맞는 연결이 실행됩니다)"));
  bindings->addWidget(bindingTable_, 1);
  bindings->addLayout(bindingButtons);
  tables->addLayout(layers, 1);
  tables->addLayout(bindings, 4);
  auto* preview = toolRow("cutoutPreviewTools");
  previewSignalCombo_ = new QComboBox;
  previewSignalCombo_->setObjectName("cutoutPreviewSignalName");
  for (const auto& s : signalVocabulary()) previewSignalCombo_->addItem(QString("%1 · %2").arg(s.name, s.label), s.name);
  previewParams_ = new QLineEdit;
  previewParams_->setObjectName("cutoutPreviewSignalParams");
  previewParams_->setPlaceholderText("신호 값 예: outcome=success, actionId=work");
  previewParams_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  previewParams_->setMinimumWidth(120);
  auto* fire = new QPushButton("미리 보기 신호");
  fire->setObjectName("cutoutPreviewSignal");
  auto* reset = new QPushButton("미리 보기 초기화");
  reset->setObjectName("cutoutPreviewReset");
  previewResult_ = new QLabel("이 탭에서는 캔버스가 게임 연결 미리 보기를 보여 줍니다(게임에는 영향 없음).");
  previewResult_->setObjectName("cutoutPreviewResult");
  previewResult_->setWordWrap(true);
  previewResult_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  connect(fire, &QPushButton::clicked, this, [this] {
    previewSignal(previewSignalCombo_->currentData().toString(), parsePairs(previewParams_->text()));
  });
  connect(reset, &QPushButton::clicked, this, [this] { previewKey_.clear(); ensurePreview(); updateCanvas(); });
  preview->addWidget(new QLabel("신호"));
  preview->addWidget(previewSignalCombo_);
  preview->addWidget(previewParams_);
  preview->addWidget(fire);
  preview->addWidget(reset);
  layout->addWidget(top);
  layout->addLayout(tables, 1);
  layout->addWidget(preview);
  layout->addWidget(previewResult_);
  return tab;
}

// ---------------------------------------------------------------- documents
const CutoutPanel::Doc* CutoutPanel::rigDoc() const { auto it = rigs_.find(rigId_); return it == rigs_.end() ? nullptr : &*it; }
const CutoutPanel::Doc* CutoutPanel::controllerDoc() const { auto it = controllers_.find(controllerId_); return it == controllers_.end() ? nullptr : &*it; }
// References (never temporaries): callers iterate listOf(displayData(), ...) directly, and an
// empty object (never null) when absent because nlohmann value() throws on null.
static const Json& emptyObject() { static const Json e = Json::object(); return e; }
const Json& CutoutPanel::committedRig() const { const Doc* d = rigDoc(); return d ? d->data : emptyObject(); }
const Json& CutoutPanel::committedController() const { const Doc* d = controllerDoc(); return d ? d->data : emptyObject(); }
const Json& CutoutPanel::displayData() const {
  if (dragPreview_) return *dragPreview_;
  if (inflight_) return *inflight_;
  if (working_) return *working_;
  return committedRig();
}
Json CutoutPanel::controllersFor(const QString& rig) const {
  Json out = Json::array();
  for (auto it = controllers_.begin(); it != controllers_.end(); ++it)
    if (qs(it->data.value("rig", std::string())) == rig) out.push_back({{"id", ss(it.key())}, {"data", it->data}});
  return out;
}
QStringList CutoutPanel::clipIds() const {
  QStringList out;
  for (const auto& c : listOf(displayData(), "clips")) out << qs(c.value("id", std::string()));
  return out;
}
QString CutoutPanel::uniqueBoneId(const QString& base) const {
  const Json data = displayData();
  QString id = base;
  for (int n = 2; findIn(data, "bones", ss(id)); n++) id = QString("%1-%2").arg(base).arg(n);
  return id;
}
QString CutoutPanel::uniqueClipId(const QString& base) const {
  const QStringList ids = clipIds();
  QString id = base;
  for (int n = 2; ids.contains(id); n++) id = QString("%1-%2").arg(base).arg(n);
  return id;
}

// ---------------------------------------------------------------- edit pipeline
bool CutoutPanel::busy() {
  if (host_ && !host_->busy()) return false;
  setStatus("편집기가 다른 작업을 처리 중입니다. 잠시 후 다시 시도하세요.", true);
  return true;
}
void CutoutPanel::setStatus(const QString& text, bool error) {
  status_ = text;
  if (!statusLabel_) return;
  statusLabel_->setText(text);
  statusLabel_->setStyleSheet(error ? "color:#f87171;" : "color:#a7b4c2;");
}
QString CutoutPanel::status() const { return status_; }
QJsonObject CutoutPanel::replaceEdit(const Doc& doc, const QJsonObject& data) const {
  QJsonObject envelope = doc.envelope;
  envelope["data"] = data;
  return {{"type", "replace"}, {"file", doc.file}, {"data", envelope}};
}
bool CutoutPanel::submit(const QJsonArray& edits) {
  if (edits.isEmpty() || busy()) return false;
  const QJsonObject edit = edits.size() == 1 ? edits[0].toObject() : QJsonObject{{"type", "batch"}, {"edits", edits}};
  QPointer<CutoutPanel> self(this);
  // A synchronous host may call viewChanged() from inside request(); inSubmit_ defers that
  // rebuild so widgets emitting the gesture are not destroyed under their own signal.
  inSubmit_ = true;
  host_->request("edit", {{"edit", edit}}, [self](QJsonObject reply) {
    if (!self) return;
    const QString error = reply.value("error").toString();
    if (!error.isEmpty()) self->setStatus("편집이 거부되었습니다: " + error, true);
  });
  inSubmit_ = false;
  return true;
}
bool CutoutPanel::commitRig(const cutout::EditResult& result) {
  const Doc* doc = rigDoc();
  if (!doc) { setStatus("먼저 캐릭터를 선택하거나 만드세요.", true); return false; }
  if (result.data == doc->data && !result.extra.contains("controllers")) { working_.reset(); setStatus("바뀐 것이 없습니다."); rebuildDisplay(); return false; }
  QJsonArray edits{replaceEdit(*doc, toQtObject(result.data))};
  if (result.extra.contains("controllers"))
    for (const auto& c : result.extra.at("controllers")) {
      auto it = controllers_.find(qs(c.value("id", std::string())));
      if (it != controllers_.end()) edits.append(replaceEdit(*it, toQtObject(c.at("data"))));
    }
  QStringList warnings;
  for (const auto& d : result.diagnostics) if (d.severity == "warning" && d.code.rfind("ANIM_REPARENT", 0) == 0) warnings << qs(d.message);
  if (!submit(edits)) { rebuildDisplay(false); return false; }
  working_.reset();
  inflight_ = result.data;
  setStatus(warnings.isEmpty() ? QString("변경을 적용했습니다. 되돌리기: Ctrl+Z") : warnings.join(" "), !warnings.isEmpty());
  rebuildDisplay();
  return true;
}
bool CutoutPanel::runOps(const std::vector<Json>& ops, bool keyGesture, const std::function<void(const cutout::EditResult&)>& accepted) {
  if (!rigDoc()) { setStatus("먼저 캐릭터를 선택하거나 만드세요.", true); return false; }
  if (ops.empty()) return false;
  cutout::EditResult result;
  try {
    result = cutout::applyEdit(working_ ? *working_ : committedRig(), batch(ops), controllersFor(rigId_));
  } catch (const std::exception& e) {
    setStatus(qs(e.what()), true);
    rebuildDisplay(false);
    return false;
  }
  if (keyGesture && mode_ == Mode::Animate && !autoKey_) {
    // Auto-key off: show the change but keep it local until K commits it as one edit.
    working_ = result.data;
    setStatus("변경됨 · K(키 추가)를 누르면 이 시간에 키로 저장됩니다. 시간을 옮기면 버려집니다.");
    rebuildDisplay();
    return true;
  }
  if (accepted) accepted(result);
  return commitRig(result);
}
bool CutoutPanel::commitController(const Json& data) {
  const Doc* doc = controllerDoc();
  if (!doc) { setStatus("이 캐릭터에 게임 연결 문서가 없습니다.", true); return false; }
  if (data == doc->data) return false;
  auto hasError = [&](const Json& d) {
    for (const auto& x : cutout::validateController(d, rig_.get())) if (x.severity == "error") return qs(x.message + " (" + x.pointer + ")");
    return QString();
  };
  const QString error = hasError(data);
  if (!error.isEmpty() && hasError(doc->data).isEmpty()) {
    setStatus("게임 연결을 적용하지 않았습니다: " + error, true);
    refreshController();
    return false;
  }
  if (!submit({replaceEdit(*doc, toQtObject(data))})) { refreshController(); return false; }
  setStatus("게임 연결을 바꿨습니다. 되돌리기: Ctrl+Z");
  return true;
}

// ---------------------------------------------------------------- view cache
void CutoutPanel::refreshFromView() {
  if (inSubmit_) {
    if (!refreshQueued_) {
      refreshQueued_ = true;
      QTimer::singleShot(0, this, [this] { refreshQueued_ = false; refreshFromView(); });
    }
    return;
  }
  if (!host_) return;
  const QJsonObject view = host_->view();
  const int revision = view.value("revision").toInt();
  images_.setAssets(host_->assets());
  rigs_.clear();
  controllers_.clear();
  characters_ = {};
  for (const auto& v : view.value("documents").toArray()) {
    const QJsonObject doc = v.toObject();
    const QJsonObject data = doc.value("data").toObject();
    if (doc.value("kind") == "character") { characters_.append(data); continue; }
    if (doc.value("kind") != "extension" || data.value("plugin") != PLUGIN_ID) continue;
    const QString type = data.value("type").toString(), id = data.value("id").toString();
    Doc d{doc.value("file").toString(), data, toJson(data.value("data").toObject())};
    if (type == "rig") rigs_.insert(id, d);
    else if (type == "controller") controllers_.insert(id, d);
  }
  if (inflight_) inflight_.reset();
  else if (revision != lastRevision_ && working_) { working_.reset(); setStatus("문서가 바뀌어 키로 저장하지 않은 변경을 버렸습니다."); }
  lastRevision_ = revision;
  dragPreview_.reset();
  if (!rigs_.contains(rigId_)) rigId_ = rigs_.isEmpty() ? QString() : rigs_.firstKey();
  auto controllerFits = [&](const QString& id) { return controllers_.contains(id) && qs(controllers_[id].data.value("rig", std::string())) == rigId_; };
  if (!controllerFits(controllerId_)) {
    controllerId_.clear();
    for (auto it = controllers_.begin(); it != controllers_.end(); ++it) if (controllerFits(it.key())) { controllerId_ = it.key(); break; }
  }
  rebuildDisplay();
}

void CutoutPanel::rebuildDisplay(bool full) {
  const Json data = displayData();
  rig_.reset();
  rigError_.clear();
  if (rigDoc() && data.is_object()) {
    try {
      cutout::Rig r = cutout::parseRig(ss(rigId_), data);
      // The editor samples clip time itself (scrubbing to the very end must show the end pose).
      for (auto& c : r.clips) c.loop = false;
      rig_ = std::make_shared<const cutout::Rig>(std::move(r));
    } catch (const std::exception& e) { rigError_ = qs(e.what()); }
  }
  displayGeneration_++;  // every rebuild may change the data (or the preview look): ghost poses are resampled
  // The preview look follows the data: a deleted (or undone) skin/expression stops being previewed.
  if (!previewSkin_.isEmpty() && !findIn(data, "skins", ss(previewSkin_))) previewSkin_.clear();
  if (!previewExpression_.isEmpty() && !findIn(data, "expressions", ss(previewExpression_))) previewExpression_.clear();
  if (full) {
    const QStringList clips = clipIds();
    if (!clips.contains(clipId_)) {
      const QString def = qs(committedController().value("defaultClip", std::string()));
      clipId_ = clips.contains(def) ? def : clips.value(0);
    }
    const QString kind = refKind(selection_), id = refId(selection_);
    if (!selection_.isEmpty() && !findIn(data, kind == "bone" ? "bones" : "slots", ss(id))) selection_.clear();
    refreshCharacters();
    refreshDiagnostics();
    refreshTree();
    refreshDrawOrder();
    refreshClips();
    refreshDopesheet();
    refreshGraph();
    refreshMix();
    refreshController();
    refreshLooks();
    refreshModeUi();
  }
  refreshProperties();
  updateCanvas();
}

void CutoutPanel::refreshCharacters() {
  QSignalBlocker block(characterList_);
  filling_ = true;
  characterList_->clear();
  for (auto it = rigs_.begin(); it != rigs_.end(); ++it) {
    const QString name = qs(it->data.value("name", std::string()));
    auto* item = new QTreeWidgetItem(characterList_, {name.isEmpty() || name == it.key() ? it.key() : QString("%1 (%2)").arg(name, it.key())});
    item->setData(0, Qt::UserRole, "rig:" + it.key());
    if (it.key() == rigId_) characterList_->setCurrentItem(item);
    for (auto c = controllers_.begin(); c != controllers_.end(); ++c) {
      if (qs(c->data.value("rig", std::string())) != it.key()) continue;
      auto* child = new QTreeWidgetItem(item, {"게임 연결: " + c.key()});
      child->setData(0, Qt::UserRole, "controller:" + c.key());
    }
    item->setExpanded(true);
  }
  filling_ = false;
}

void CutoutPanel::refreshDiagnostics() {
  diagnostics_->clear();
  if (!host_) return;
  for (const auto& v : host_->view().value("diagnostics").toArray()) {
    const QJsonObject d = v.toObject();
    const QString file = d.value("file").toString();
    if (!file.startsWith("plugins/cutout-animation/")) continue;
    const QString severity = d.value("severity").toString();
    auto* item = new QListWidgetItem(QString("%1 · %2\n%3 %4").arg(severity == "error" ? "오류" : severity == "warning" ? "경고" : "정보",
                                                                  d.value("message").toString(), file, d.value("pointer").toString()), diagnostics_);
    item->setData(Qt::UserRole, file);
    item->setData(Qt::UserRole + 1, d.value("pointer").toString());
    item->setForeground(severity == "error" ? QColor("#f87171") : QColor("#fbbf24"));
    item->setToolTip(item->text());  // the narrow side column elides long paths
  }
  if (diagnostics_->count() == 0) new QListWidgetItem("검증 문제 없음", diagnostics_);
}

void CutoutPanel::refreshTree() {
  QSignalBlocker block(tree_);
  filling_ = true;
  tree_->clear();
  const Json data = displayData();
  std::map<std::string, QTreeWidgetItem*> items;
  std::map<std::string, std::vector<std::string>> children;
  std::vector<std::string> roots;
  for (const auto& b : listOf(data, "bones")) {
    const std::string id = b.value("id", std::string()), parent = b.contains("parent") && b.at("parent").is_string() ? b.at("parent").get<std::string>() : "";
    if (parent.empty() || !findIn(data, "bones", parent)) roots.push_back(id); else children[parent].push_back(id);
  }
  std::function<void(const std::string&, QTreeWidgetItem*, int)> add = [&](const std::string& id, QTreeWidgetItem* parent, int depth) {
    if (depth > 256 || items.count(id)) return;
    auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree_);
    item->setText(0, "◉ " + qs(id));
    item->setData(0, Qt::UserRole, "bone:" + qs(id));
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    items[id] = item;
    for (const auto& s : listOf(data, "slots")) {
      if (s.value("bone", std::string()) != id) continue;
      const QString sid = qs(s.value("id", std::string()));
      const QString att = s.contains("attachment") && s.at("attachment").is_string() ? qs(s.at("attachment").get<std::string>()) : "없음";
      auto* slot = new QTreeWidgetItem(item, {QString("▣ %1 · %2").arg(sid, att)});
      slot->setData(0, Qt::UserRole, "slot:" + sid);
      slot->setFlags((slot->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled) & ~Qt::ItemIsDropEnabled);
      slot->setCheckState(1, hidden_.contains(sid) ? Qt::Unchecked : Qt::Checked);
      slot->setForeground(0, QColor("#c9b6ff"));
    }
    for (const auto& c : children[id]) add(c, item, depth + 1);
  };
  for (const auto& r : roots) add(r, nullptr, 0);
  // Bone eye = all slots of its subtree visible.
  std::function<bool(QTreeWidgetItem*)> visible = [&](QTreeWidgetItem* it) {
    bool all = refKind(it->data(0, Qt::UserRole).toString()) != "slot" || it->checkState(1) == Qt::Checked;
    for (int i = 0; i < it->childCount(); i++) all = visible(it->child(i)) && all;
    if (refKind(it->data(0, Qt::UserRole).toString()) == "bone") it->setCheckState(1, all ? Qt::Checked : Qt::Unchecked);
    return all;
  };
  for (int i = 0; i < tree_->topLevelItemCount(); i++) visible(tree_->topLevelItem(i));
  tree_->expandAll();
  QTreeWidgetItemIterator it(tree_);
  for (; *it; ++it) if ((*it)->data(0, Qt::UserRole).toString() == selection_) { tree_->setCurrentItem(*it); break; }
  filling_ = false;
}

void CutoutPanel::refreshDrawOrder() {
  QSignalBlocker block(drawOrder_);
  filling_ = true;
  drawOrder_->clear();
  for (const auto& s : listOf(displayData(), "slots")) {
    const QString id = qs(s.value("id", std::string()));
    auto* item = new QListWidgetItem(QString("%1 · %2").arg(id, qs(s.value("bone", std::string()))), drawOrder_);
    item->setData(Qt::UserRole, id);
    if ("slot:" + id == selection_) drawOrder_->setCurrentItem(item);
  }
  filling_ = false;
}

int CutoutPanel::boneIndex(const QString& id) const { return rig_ ? rig_->boneIndex(ss(id)) : -1; }
QString CutoutPanel::selectedBone() const {
  if (refKind(selection_) == "bone") return refId(selection_);
  if (refKind(selection_) == "slot") if (const Json* s = findIn(displayData(), "slots", ss(refId(selection_)))) return qs(s->value("bone", std::string()));
  return {};
}

bool CutoutPanel::showingPreview() const {
  return rig_ && bottomTabs_ && bottomTabs_->currentIndex() == 1 && preview_ && !dragPreview_ && preview_->rig().bones.size() == rig_->bones.size() &&
         preview_->rig().slots.size() == rig_->slots.size();
}
cutout::Look CutoutPanel::lookFor(const cutout::Rig& rig) const {
  cutout::Look look;
  look.skin = previewSkin_.isEmpty() ? -1 : rig.skinIndex(ss(previewSkin_));
  look.expression = previewExpression_.isEmpty() ? -1 : rig.expressionIndex(ss(previewExpression_));
  return look;
}
QString CutoutPanel::skinProviding(const QString& slot, const QString& name) const {
  if (previewSkin_.isEmpty()) return {};
  if (const Json* skin = findIn(displayData(), "skins", ss(previewSkin_)))
    for (const auto& a : listOf(*skin, "attachments"))
      if (qs(a.value("slot", std::string())) == slot && qs(a.value("name", std::string())) == name) return previewSkin_;
  return {};
}
cutout::Pose CutoutPanel::currentPose(bool expressions) const {
  if (!rig_) return {};
  if (showingPreview()) return preview_->pose();
  cutout::Look look = lookFor(*rig_);
  look.expressions = expressions;
  if (mode_ == Mode::Animate) return cutout::samplePose(*rig_, rig_->clipIndex(ss(clipId_)), playheadExact_, look);
  return cutout::samplePose(*rig_, -1, 0, look);
}

void CutoutPanel::refreshProperties() {
  filling_ = true;
  const QString kind = refKind(selection_), id = refId(selection_);
  const Json data = displayData();
  const cutout::Pose pose = currentPose(false);  // keyable values (a previewed expression only overlays the canvas)
  const bool animate = mode_ == Mode::Animate;
  QMap<QString, double> values;
  QMap<QString, bool> enabled;
  for (const char* p : PROPS) enabled[p] = false;
  QSignalBlocker blockCombo(attachmentCombo_);
  attachmentCombo_->clear();
  attachmentCombo_->setEnabled(false);
  QString label = "선택 없음 · 캔버스나 트리에서 본(◉) 또는 조각(▣)을 고르세요.";
  if (kind == "bone" && rig_) {
    const int i = rig_->boneIndex(ss(id));
    if (i >= 0) {
      const cutout::Transform t = animate && size_t(i) < pose.bones.size() ? pose.bones[size_t(i)] : rig_->bones[size_t(i)].setup;
      values = {{"x", t.x}, {"y", t.y}, {"rotation", t.rotation}, {"scaleX", t.scaleX}, {"scaleY", t.scaleY}};
      for (const char* p : {"x", "y", "rotation", "scaleX", "scaleY"}) enabled[p] = true;
      label = QString("본 %1%2").arg(id, animate ? QString(" · %1ms 자세").arg(playheadMs_) : QString(" · 기본 자세"));
    }
  } else if (kind == "slot" && rig_) {
    const int i = rig_->slotIndex(ss(id));
    if (i >= 0) {
      const auto& slot = rig_->slots[size_t(i)];
      const std::optional<std::string> current = size_t(i) < pose.attachments.size() ? pose.attachments[size_t(i)] : slot.attachment;
      attachmentCombo_->addItem("(없음 · 숨김)", QString());
      for (const auto& a : rig_->attachments) if (a.slot == slot.id) attachmentCombo_->addItem(qs(a.name), qs(a.name));
      // Names only a skin provides (an outfit-only part) are keyable too.
      for (const QString& n : slotNames(data, slot.id)) if (attachmentCombo_->findData(n) < 0) attachmentCombo_->addItem(n + " (스킨)", n);
      attachmentCombo_->setCurrentIndex(current ? std::max(0, attachmentCombo_->findData(qs(*current))) : 0);
      attachmentCombo_->setEnabled(true);
      values["alpha"] = size_t(i) < pose.alpha.size() ? pose.alpha[size_t(i)] : slot.alpha;
      enabled["alpha"] = true;
      if (current) if (const cutout::Attachment* att = rig_->attachment(slot.id, *current, pose.skin)) {
        values["x"] = att->offset.x; values["y"] = att->offset.y; values["rotation"] = att->offset.rotation;
        values["scaleX"] = att->offset.scaleX; values["scaleY"] = att->offset.scaleY; values["pivotX"] = att->pivotX; values["pivotY"] = att->pivotY;
        // Attachment placement is setup data; in Animate mode only alpha/attachment are keyable.
        if (!animate) for (const char* p : {"x", "y", "rotation", "scaleX", "scaleY", "pivotX", "pivotY"}) enabled[p] = true;
      }
      label = QString("조각 %1 · 본 %2%3").arg(id, qs(slot.bone), animate ? " · 키: 첨부 그림·투명도" : " · 첨부 배치");
      if (current && !skinProviding(id, qs(*current)).isEmpty()) label += QString(" · 스킨 「%1」의 그림").arg(previewSkin_);
    }
  }
  for (auto it = props_.begin(); it != props_.end(); ++it) {
    QSignalBlocker block(it.value());
    it.value()->setValue(values.value(it.key(), 0));
    it.value()->setProperty("shown", it.value()->value());
    it.value()->setEnabled(enabled.value(it.key()));
  }
  selectionLabel_->setText(label);
  (void)data;
  // Key indicator: none / keyed / modified.
  const QString state = keyState();
  keyButton_->setProperty("keyState", state);
  keyButton_->setEnabled(animate && (!selection_.isEmpty() || working_));
  keyButton_->setText(state == "modified" ? "◆ 변경됨 · 키 추가 (K)" : state == "keyed" ? "◆ 키 있음 · 다시 키 (K)" : "◇ 키 추가 (K)");
  keyButton_->setStyleSheet(state == "modified" ? "background:#f59e0b;color:#111;font-weight:bold;" : state == "keyed" ? "background:#16a34a;color:white;" : "");
  discardButton_->setEnabled(working_.has_value());
  filling_ = false;
}

QString CutoutPanel::keyState() const {
  if (mode_ != Mode::Animate) return "setup";
  if (working_) return "modified";
  const QString kind = refKind(selection_);
  if (kind.isEmpty()) return "none";
  const std::string target = ss(selection_);
  if (const Json* clip = findIn(committedRig(), "clips", ss(clipId_)))
    for (const auto& t : listOf(*clip, "tracks"))
      if (t.value("target", std::string()) == target)
        for (const auto& k : listOf(t, "keys")) if (k.value("timeMs", int64_t(-1)) == playheadMs_) return "keyed";
  return "none";
}

void CutoutPanel::refreshClips() {
  QSignalBlocker block(clipList_);
  filling_ = true;
  clipList_->clear();
  const Json data = displayData();
  const Json* current = nullptr;
  for (const auto& c : listOf(data, "clips")) {
    const QString id = qs(c.value("id", std::string())), name = qs(c.value("name", std::string()));
    auto* item = new QListWidgetItem(QString("%1%2 · %3ms%4").arg(id, name.isEmpty() || name == id ? QString() : " (" + name + ")")
                                         .arg(c.value("durationMs", int64_t(0))).arg(c.value("loop", true) ? " · 반복" : ""), clipList_);
    item->setData(Qt::UserRole, id);
    if (id == clipId_) { clipList_->setCurrentItem(item); current = &c; }
  }
  {
    QSignalBlocker b1(clipDuration_), b2(clipLoop_);
    clipDuration_->setValue(current ? int(current->value("durationMs", int64_t(1000))) : 1000);
    clipLoop_->setChecked(current ? current->value("loop", true) : true);
    clipDuration_->setEnabled(current);
    clipLoop_->setEnabled(current);
  }
  filling_ = false;
}

void CutoutPanel::refreshDopesheet() {
  QVector<Dopesheet::Row> rows;
  int duration = 1000;
  if (const Json* clip = findIn(displayData(), "clips", ss(clipId_))) {
    duration = int(clip->value("durationMs", int64_t(1000)));
    for (const auto& t : listOf(*clip, "tracks")) {
      Dopesheet::Row row;
      row.target = qs(t.value("target", std::string()));
      row.property = qs(t.value("property", std::string()));
      row.attachment = row.property == "attachment" || row.property == "expression";  // both switch (step)
      row.label = row.target == "rig" ? QString("표정 (리그)") : QString("%1 %2").arg(refId(row.target), propertyLabel(row.property));
      for (const auto& k : listOf(t, "keys")) {
        row.times << int(k.value("timeMs", int64_t(0)));
        row.step << (interpOf(k) == "step");
        row.interps << qs(interpOf(k));
        row.curves << curveOf(k);
      }
      rows << row;
    }
  }
  // Another clip (or character): its own fitted zoom and remembered preview loop range.
  const QString viewClip = rigId_ + "/" + clipId_;
  if (viewClip != viewClip_) {
    if (!viewClip_.isEmpty()) {
      if (timeView_->loopActive()) loops_[viewClip_] = {timeView_->loopIn(), timeView_->loopOut()};
      else loops_.remove(viewClip_);
    }
    viewClip_ = viewClip;
    timeView_->clearSelection();
    timeView_->setDuration(duration);
    timeView_->fit();
    const auto loop = loops_.value(viewClip, {0, -1});
    timeView_->setLoop(loop.first, loop.second);
  }
  dopesheet_->setRows(rows, duration);
  playheadMs_ = std::clamp(playheadMs_, 0, duration);
  playheadExact_ = std::clamp(playheadExact_, 0.0, double(duration));
  timeView_->setPlayhead(playheadMs_);
  updateTimeUi(duration);
}
void CutoutPanel::updateTimeUi(int duration) {
  timeLabel_->setText(QString("%1 / %2 ms").arg(playheadMs_).arg(duration));
  QSignalBlocker block(timeField_);
  timeField_->setRange(0, duration);
  timeField_->setValue(playheadMs_);
}
void CutoutPanel::syncTimeScroll() {
  if (!timeScroll_) return;
  QSignalBlocker block(timeScroll_);
  const double visible = timeView_->visibleMs();
  timeScroll_->setRange(0, int(std::lround(std::max(0.0, timeView_->duration() - visible) * 10)));
  timeScroll_->setPageStep(std::max(1, int(visible * 10)));
  timeScroll_->setSingleStep(std::max(1, int(visible)));
  timeScroll_->setValue(int(std::lround(timeView_->scrollMs() * 10)));
}

QString CutoutPanel::graphTarget() const {
  if (!selection_.isEmpty()) return selection_;
  const int row = dopesheet_->selectedRow();
  return row >= 0 && row < dopesheet_->rows().size() ? dopesheet_->rows()[row].target : QString();
}
void CutoutPanel::refreshGraph() {
  // Numeric tracks of the selected bone/slot in the current clip, one colored channel each.
  static const QMap<QString, QColor> colors = {{"x", QColor("#f87171")}, {"y", QColor("#4ade80")}, {"rotation", QColor("#60a5fa")},
                                               {"scaleX", QColor("#fb923c")}, {"scaleY", QColor("#c084fc")}, {"alpha", QColor("#e5e7eb")}};
  QVector<GraphEditor::Channel> channels;
  int duration = 1000;
  const QString target = graphTarget();
  if (const Json* clip = findIn(displayData(), "clips", ss(clipId_))) {
    duration = int(clip->value("durationMs", int64_t(1000)));
    for (const char* property : NUMERIC_PROPS)
      for (const auto& t : listOf(*clip, "tracks")) {
        if (qs(t.value("target", std::string())) != target || t.value("property", std::string()) != property) continue;
        GraphEditor::Channel channel{target, property, propertyLabel(property), colors.value(property), {}};
        for (const auto& k : listOf(t, "keys")) {
          if (!k.contains("value") || !k.at("value").is_number()) continue;
          channel.keys.push_back({int(k.value("timeMs", int64_t(0))), k.at("value").get<double>(), qs(interpOf(k)), curveOf(k)});
        }
        channels << channel;
      }
  }
  graph_->setMessage(target.isEmpty() ? "본·조각을 고르거나 도프시트 행을 누르면 그 트랙의 값 곡선이 여기에 보입니다."
                                      : QString("「%1」에는 이 클립의 숫자 키가 없습니다. 애니메이션 모드에서 값을 바꾸고 K를 누르세요.").arg(refId(target)));
  graph_->setChannels(channels, duration);
  graph_->setPlayhead(playheadMs_);
}

void CutoutPanel::refreshMix() {
  filling_ = true;
  const Json data = displayData();
  const Json mix = data.is_object() && data.contains("mix") && data.at("mix").is_object() ? data.at("mix") : Json::object();
  {
    QSignalBlocker block(mixDefault_);
    mixDefault_->setValue(int(mix.value("defaultMs", int64_t(0))));
  }
  const QStringList clips = clipIds();
  mixTable_->setRowCount(0);
  int row = 0;
  for (const auto& p : listOf(mix, "pairs")) {
    mixTable_->insertRow(row);
    auto combo = [&](const char* key, const QString& name) {
      auto* c = new QComboBox;
      c->setObjectName(name + QString::number(row));
      c->addItems(clips);
      c->setCurrentText(qs(p.value(key, std::string())));
      connect(c, &QComboBox::activated, this, [this] { commitMixTable(); });
      return c;
    };
    mixTable_->setCellWidget(row, 0, combo("from", "cutoutMixFrom"));
    mixTable_->setCellWidget(row, 1, combo("to", "cutoutMixTo"));
    auto* ms = new QSpinBox;
    ms->setObjectName("cutoutMixMs" + QString::number(row));
    ms->setRange(0, 10000);
    ms->setSuffix(" ms");
    ms->setKeyboardTracking(false);
    ms->setValue(int(p.value("durationMs", int64_t(0))));
    connect(ms, &QSpinBox::editingFinished, this, [this] { commitMixTable(); });
    mixTable_->setCellWidget(row, 2, ms);
    row++;
  }
  filling_ = false;
}

void CutoutPanel::refreshController() {
  filling_ = true;
  const Json c = committedController();
  const QStringList clips = clipIds(), skins = idsOf(displayData(), "skins"), expressions = idsOf(displayData(), "expressions");
  for (auto [combo, key, none, ids] : {std::tuple{defaultSkinCombo_, "defaultSkin", "(기본 첨부)", &skins}, std::tuple{defaultExpressionCombo_, "defaultExpression", "(없음)", &expressions}}) {
    QSignalBlocker b(combo);
    combo->clear();
    combo->addItem(none, QString());
    for (const QString& id : *ids) combo->addItem(id, id);
    const QString current = qs(c.contains(key) && c.at(key).is_string() ? c.at(key).get<std::string>() : std::string());
    if (!current.isEmpty() && combo->findData(current) < 0) combo->addItem(current + " (리그에 없음)", current);
    combo->setCurrentIndex(std::max(0, combo->findData(current)));
  }
  {
    QSignalBlocker b(controllerCombo_);
    controllerCombo_->clear();
    for (auto it = controllers_.begin(); it != controllers_.end(); ++it)
      if (qs(it->data.value("rig", std::string())) == rigId_) controllerCombo_->addItem(it.key(), it.key());
    controllerCombo_->setCurrentIndex(controllerCombo_->findData(controllerId_));
  }
  {
    QSignalBlocker b(defaultClipCombo_);
    defaultClipCombo_->clear();
    defaultClipCombo_->addItems(clips);
    defaultClipCombo_->setCurrentText(qs(c.value("defaultClip", std::string())));
  }
  {
    QSignalBlocker b(characterCombo_);
    characterCombo_->clear();
    characterCombo_->addItem("(없음)", QString());
    for (const auto& v : characters_) {
      const QJsonObject ch = v.toObject();
      characterCombo_->addItem(QString("%1 (%2)").arg(ch.value("name").toString(ch.value("id").toString()), ch.value("id").toString()), ch.value("id").toString());
    }
    const QString current = qs(c.value("characterId", std::string()));
    if (!current.isEmpty() && characterCombo_->findData(current) < 0) characterCombo_->addItem(current + " (프로젝트에 없음)", current);
    characterCombo_->setCurrentIndex(std::max(0, characterCombo_->findData(current)));
  }
  layerTable_->setRowCount(0);
  int row = 0;
  for (const auto& l : listOf(c, "layers")) {
    layerTable_->insertRow(row);
    auto* track = new QSpinBox;
    track->setObjectName("cutoutLayerTrack" + QString::number(row));
    track->setRange(1, 15);
    track->setKeyboardTracking(false);
    track->setValue(l.value("track", 1));
    connect(track, &QSpinBox::editingFinished, this, [this] { commitControllerFromUi(); });
    auto* clip = new QComboBox;
    clip->setObjectName("cutoutLayerClip" + QString::number(row));
    clip->addItems(clips);
    clip->setCurrentText(qs(l.value("clip", std::string())));
    connect(clip, &QComboBox::activated, this, [this] { commitControllerFromUi(); });
    layerTable_->setCellWidget(row, 0, track);
    layerTable_->setCellWidget(row, 1, clip);
    row++;
  }
  bindingTable_->setRowCount(0);
  row = 0;
  for (const auto& b : listOf(c, "bindings")) {
    bindingTable_->insertRow(row);
    const QString n = QString::number(row);
    auto line = [&](const char* name, const QString& text, const QString& placeholder) {
      auto* e = new QLineEdit(text);
      e->setObjectName(name + n);
      e->setPlaceholderText(placeholder);
      connect(e, &QLineEdit::editingFinished, this, [this] { commitControllerFromUi(); });
      return e;
    };
    auto combo = [&](const char* name) {
      auto* box = new QComboBox;
      box->setObjectName(name + n);
      connect(box, &QComboBox::activated, this, [this] { commitControllerFromUi(); });
      return box;
    };
    bindingTable_->setCellWidget(row, 0, line("cutoutBindingId", qs(b.value("id", std::string())), "id"));
    auto* signal = combo("cutoutBindingSignal");
    QString params;
    for (const auto& s : signalVocabulary()) {
      signal->addItem(QString("%1 · %2").arg(s.name, s.label), s.name);
      if (s.name == b.value("signal", std::string())) params = s.params.join(", ");
    }
    const QString sig = qs(b.value("signal", std::string()));
    if (signal->findData(sig) < 0) signal->addItem(sig, sig);
    signal->setCurrentIndex(signal->findData(sig));
    bindingTable_->setCellWidget(row, 1, signal);
    QJsonObject match;
    if (b.contains("match") && b.at("match").is_object()) match = toQtObject(b.at("match"));
    auto* matchEdit = line("cutoutBindingMatch", joinPairs(match), "예: outcome=success");
    matchEdit->setToolTip("이 신호의 값: " + params);
    bindingTable_->setCellWidget(row, 2, matchEdit);
    auto* clip = combo("cutoutBindingClip");
    clip->addItem("— (클립 없음)", QString());
    for (const QString& id : clips) clip->addItem(id, id);
    const QString clipValue = qs(b.contains("clip") && b.at("clip").is_string() ? b.at("clip").get<std::string>() : std::string());
    if (!clipValue.isEmpty() && clip->findData(clipValue) < 0) clip->addItem(clipValue, clipValue);
    clip->setCurrentIndex(std::max(0, clip->findData(clipValue)));
    bindingTable_->setCellWidget(row, 3, clip);
    auto* track = new QSpinBox;
    track->setObjectName("cutoutBindingTrack" + n);
    track->setRange(0, 15);
    track->setKeyboardTracking(false);
    track->setValue(b.value("track", 0));
    connect(track, &QSpinBox::editingFinished, this, [this] { commitControllerFromUi(); });
    bindingTable_->setCellWidget(row, 4, track);
    auto* loop = combo("cutoutBindingLoop");
    loop->addItem("클립 설정", "");
    loop->addItem("반복", "true");
    loop->addItem("한 번", "false");
    loop->setCurrentIndex(b.contains("loop") && b.at("loop").is_boolean() ? (b.at("loop").get<bool>() ? 1 : 2) : 0);
    bindingTable_->setCellWidget(row, 5, loop);
    auto* then = combo("cutoutBindingThen");
    then->addItem("대기 클립으로", "default");
    then->addItem("마지막 자세 유지", "hold");
    then->addItem("멈추고 아래 트랙", "stop");
    then->setCurrentIndex(std::max(0, then->findData(qs(b.value("then", std::string("default"))))));
    bindingTable_->setCellWidget(row, 6, then);
    QStringList stops;
    for (const auto& s : listOf(b, "stopOn")) if (s.is_string()) stops << qs(s.get<std::string>());
    bindingTable_->setCellWidget(row, 7, line("cutoutBindingStopOn", stops.join(", "), "예: dialogueEnded"));
    auto* skin = combo("cutoutBindingSkin");
    skin->addItem("—", QString());
    for (const QString& id : skins) skin->addItem(id, id);
    const QString skinValue = qs(b.contains("skin") && b.at("skin").is_string() ? b.at("skin").get<std::string>() : std::string());
    if (!skinValue.isEmpty() && skin->findData(skinValue) < 0) skin->addItem(skinValue, skinValue);
    skin->setCurrentIndex(std::max(0, skin->findData(skinValue)));
    bindingTable_->setCellWidget(row, 8, skin);
    auto* expression = combo("cutoutBindingExpression");
    expression->addItem("— (바꾸지 않음)", "-");
    expression->addItem("(표정 없음으로)", QString());
    for (const QString& id : expressions) expression->addItem(id, id);
    QString expressionValue = "-";
    if (b.contains("expression")) expressionValue = b.at("expression").is_string() ? qs(b.at("expression").get<std::string>()) : QString();
    if (expression->findData(expressionValue) < 0) expression->addItem(expressionValue, expressionValue);
    expression->setCurrentIndex(std::max(0, expression->findData(expressionValue)));
    bindingTable_->setCellWidget(row, 9, expression);
    row++;
  }
  bindingTable_->resizeColumnsToContents();
  filling_ = false;
}

void CutoutPanel::refreshModeUi() {
  const bool animate = mode_ == Mode::Animate;
  setupButton_->setChecked(!animate);
  animateButton_->setChecked(animate);
  if (animate) {
    banner_->setText(QString("● 애니메이션 모드 · 클립 「%1」 %2ms · 변경은 이 시간의 키가 됩니다 (%3)")
                         .arg(clipId_.isEmpty() ? "없음" : clipId_).arg(playheadMs_).arg(autoKey_ ? "자동 키 켜짐" : "자동 키 꺼짐 · K로 키 추가"));
    banner_->setStyleSheet("background:#b91c1c;color:white;font-weight:bold;padding:4px 8px;border-radius:3px;");
  } else {
    banner_->setText("■ 조립 모드 · 본과 이미지 조각의 기본 자세를 편집합니다. 키는 만들어지지 않습니다.");
    banner_->setStyleSheet("background:#1d4ed8;color:white;font-weight:bold;padding:4px 8px;border-radius:3px;");
  }
  banner_->setProperty("mode", animate ? "animate" : "setup");
  toolButtons_[RigCanvas::Pivot]->setEnabled(!animate);
  for (QWidget* w : {static_cast<QWidget*>(dopesheet_), static_cast<QWidget*>(playButton_), static_cast<QWidget*>(graph_)}) w->setEnabled(animate);
  for (QPushButton* b : presetButtons_) b->setEnabled(animate);
  for (QWidget* w : timelineTools_) w->setEnabled(animate);
  autoKeyCheck_->setEnabled(animate);
  if (canvas_) canvas_->setAnimateMode(animate);
  bool keyed = false;
  if (lastSetupDelta_)
    for (const auto& c : listOf(committedRig(), "clips"))
      for (const auto& t : listOf(c, "tracks")) keyed |= t.value("target", std::string()) == "bone:" + ss(lastSetupDelta_->bone);
  deltaButton_->setVisible(!animate && keyed);
}

void CutoutPanel::updateCanvas() {
  if (!canvas_) return;
  canvas_->setHiddenSlots(hidden_);
  canvas_->setSelection(selection_);
  canvas_->setMessage(rigId_.isEmpty() ? "캐릭터가 없습니다. 「새 캐릭터」를 눌러 시작하세요."
                                       : !rigError_.isEmpty() ? "리그 데이터 오류: " + rigError_ : QString());
  canvas_->setScene(rig_, currentPose());
  updateOnion();
}

// ---------------------------------------------------------------- onion skin / motion trail
QFrame* CutoutPanel::buildOnionSettings() {
  // A popup frame (not a QWidgetAction menu) so the controls stay children of the panel:
  // automation finds them by objectName even while the popup is closed.
  auto* frame = new QFrame(this, Qt::Popup);
  frame->setObjectName("cutoutOnionSettings");
  frame->setFrameShape(QFrame::StyledPanel);
  auto* form = new QFormLayout(frame);
  auto spin = [&](const char* name, int lo, int hi, const QString& suffix) {
    auto* s = new QSpinBox;
    s->setObjectName(name);
    s->setRange(lo, hi);
    s->setSuffix(suffix);
    connect(s, &QSpinBox::valueChanged, this, [this] { onionFromUi(); });
    return s;
  };
  auto colors = [&](const char* name) {
    auto* c = new QComboBox;
    c->setObjectName(name);
    const QPair<QString, QString> presets[] = {{"빨강", "#ef4444"}, {"주황", "#f97316"}, {"초록", "#22c55e"}, {"파랑", "#3b82f6"}, {"보라", "#a855f7"}, {"원래 색", ""}};
    for (const auto& [label, color] : presets) {
      QPixmap swatch(12, 12);
      swatch.fill(color.isEmpty() ? QColor(0, 0, 0, 0) : QColor(color));
      c->addItem(QIcon(swatch), label, color);
    }
    connect(c, &QComboBox::currentIndexChanged, this, [this] { onionFromUi(); });
    return c;
  };
  onionBefore_ = spin("cutoutOnionBefore", 0, OnionSettings::MAX_GHOSTS, "");
  onionAfter_ = spin("cutoutOnionAfter", 0, OnionSettings::MAX_GHOSTS, "");
  onionMode_ = new QComboBox;
  onionMode_->setObjectName("cutoutOnionMode");
  onionMode_->addItem("일정 간격", false);
  onionMode_->addItem("키 위치만", true);
  onionMode_->setToolTip("키 위치만: 선택한 대상(없으면 보이는 모든 트랙)의 앞뒤 키 시간에 유령을 그립니다.");
  connect(onionMode_, &QComboBox::currentIndexChanged, this, [this] { onionFromUi(); });
  onionStep_ = spin("cutoutOnionStep", 1, 5000, " ms");
  onionOpacity_ = spin("cutoutOnionOpacity", 5, 100, " %");
  onionFalloff_ = spin("cutoutOnionFalloff", 0, 100, " %");
  onionFalloff_->setToolTip("멀리 있는 유령일수록 흐려지는 정도(0 = 모두 같은 투명도)");
  onionBeforeColor_ = colors("cutoutOnionBeforeColor");
  onionAfterColor_ = colors("cutoutOnionAfterColor");
  onionCurrent_ = new QCheckBox("현재 시간도 표시 (저장된 자세, 회색)");
  onionCurrent_->setObjectName("cutoutOnionIncludeCurrent");
  onionCurrent_->setToolTip("키로 저장하지 않은 변경이나 끄는 중일 때 원래 자세를 함께 보여 줍니다.");
  onionSubtree_ = new QCheckBox("선택한 본과 자식만");
  onionSubtree_->setObjectName("cutoutOnionSubtree");
  for (auto* c : {onionCurrent_, onionSubtree_}) connect(c, &QCheckBox::toggled, this, [this] { onionFromUi(); });
  form->addRow("앞 (이전)", onionBefore_);
  form->addRow("뒤 (이후)", onionAfter_);
  form->addRow("방식", onionMode_);
  form->addRow("간격", onionStep_);
  form->addRow("가장 가까운 유령 불투명도", onionOpacity_);
  form->addRow("멀수록 흐리게", onionFalloff_);
  form->addRow("이전 색", onionBeforeColor_);
  form->addRow("이후 색", onionAfterColor_);
  form->addRow(onionCurrent_);
  form->addRow(onionSubtree_);
  return frame;
}
void CutoutPanel::syncOnionUi() {
  if (!onionPopup_) return;
  onionFilling_ = true;
  auto pick = [](QComboBox* c, const QColor& color) {
    const QString name = color.isValid() ? color.name() : QString();
    int i = c->findData(name);
    if (i < 0) {  // a custom color from QSettings stays selectable
      QPixmap swatch(12, 12);
      swatch.fill(color);
      c->insertItem(c->count() - 1, QIcon(swatch), name, name);
      i = c->findData(name);
    }
    c->setCurrentIndex(i);
  };
  onionButton_->setChecked(onion_.enabled);
  trailButton_->setChecked(onion_.trail);
  onionBefore_->setValue(onion_.before);
  onionAfter_->setValue(onion_.after);
  onionMode_->setCurrentIndex(onion_.keysOnly ? 1 : 0);
  onionStep_->setValue(onion_.stepMs);
  onionStep_->setEnabled(!onion_.keysOnly);
  onionOpacity_->setValue(int(std::lround(onion_.opacity * 100)));
  onionFalloff_->setValue(int(std::lround(onion_.falloff * 100)));
  pick(onionBeforeColor_, onion_.beforeColor);
  pick(onionAfterColor_, onion_.afterColor);
  onionCurrent_->setChecked(onion_.includeCurrent);
  onionSubtree_->setChecked(onion_.subtreeOnly);
  onionFilling_ = false;
}
void CutoutPanel::onionFromUi() {
  if (onionFilling_) return;
  OnionSettings s = onion_;
  s.before = onionBefore_->value();
  s.after = onionAfter_->value();
  s.keysOnly = onionMode_->currentData().toBool();
  s.stepMs = onionStep_->value();
  s.opacity = onionOpacity_->value() / 100.0;
  s.falloff = onionFalloff_->value() / 100.0;
  s.beforeColor = QColor(onionBeforeColor_->currentData().toString());
  s.afterColor = QColor(onionAfterColor_->currentData().toString());
  s.includeCurrent = onionCurrent_->isChecked();
  s.subtreeOnly = onionSubtree_->isChecked();
  setOnionSettings(s);
}
void CutoutPanel::setOnionSettings(const OnionSettings& settings) {
  OnionSettings s = settings;
  s.before = std::clamp(s.before, 0, OnionSettings::MAX_GHOSTS);
  s.after = std::clamp(s.after, 0, OnionSettings::MAX_GHOSTS);
  s.stepMs = std::clamp(s.stepMs, 1, 10000);
  s.opacity = std::clamp(s.opacity, 0.05, 1.0);
  s.falloff = std::clamp(s.falloff, 0.0, 1.0);
  const bool changed = s != onion_;
  onion_ = s;
  syncOnionUi();
  if (!changed) return;
  onion_.save();  // per-user editor preference, never part of the rig/controller documents
  updateOnion();
}
void CutoutPanel::setOnionSkin(bool on) {
  OnionSettings s = onion_;
  s.enabled = on;
  setOnionSettings(s);
}

QSet<int> CutoutPanel::boneSubtree(int bone) const {
  QSet<int> out;
  if (!rig_ || bone < 0) return out;
  for (int i = 0; i < int(rig_->bones.size()); i++)
    for (int b = i, guard = 0; b >= 0 && guard <= int(rig_->bones.size()); b = rig_->bones[size_t(b)].parentIndex, guard++)
      if (b == bone) { out.insert(i); break; }
  return out;
}
QSet<QString> CutoutPanel::ghostHiddenSlots() const {
  QSet<QString> out = hidden_;
  const int bone = boneIndex(selectedBone());
  if (!onion_.subtreeOnly || !rig_ || bone < 0) return out;
  const QSet<int> keep = boneSubtree(bone);
  for (const auto& slot : rig_->slots) if (!keep.contains(slot.boneIndex)) out.insert(qs(slot.id));
  return out;
}
QVector<int> CutoutPanel::onionKeyTimes(int clip) const {
  // Keys of the selected target (with "subtree only": its bones and their parts); none selected
  // or nothing keyed there -> every visible track, so keys-only mode never goes silently empty.
  QSet<QString> targets;
  if (!selection_.isEmpty()) targets.insert(selection_);
  if (onion_.subtreeOnly && rig_) {
    const QSet<int> tree = boneSubtree(boneIndex(selectedBone()));
    for (int b : tree) targets.insert("bone:" + qs(rig_->bones[size_t(b)].id));
    for (const auto& slot : rig_->slots) if (tree.contains(slot.boneIndex)) targets.insert("slot:" + qs(slot.id));
  }
  QVector<int> selected, visible;
  for (const auto& t : rig_->clips[size_t(clip)].tracks) {
    if (!t.bone && hidden_.contains(qs(t.target))) continue;
    const bool mine = targets.contains((t.bone ? "bone:" : "slot:") + qs(t.target));
    for (const auto& k : t.keys) {
      visible << int(k.timeMs);
      if (mine) selected << int(k.timeMs);
    }
  }
  return selected.isEmpty() ? visible : selected;
}
std::shared_ptr<const cutout::Rig> CutoutPanel::committedParsed() {
  if (&displayData() == &committedRig()) return rig_;
  const QString key = QString("%1/%2").arg(lastRevision_).arg(rigId_);
  if (key != committedParsedKey_) {
    committedParsedKey_ = key;
    committedParsed_.reset();
    try {
      cutout::Rig r = cutout::parseRig(ss(rigId_), committedRig());
      for (auto& c : r.clips) c.loop = false;  // same scrubbing rule as rig_
      committedParsed_ = std::make_shared<const cutout::Rig>(std::move(r));
    } catch (const std::exception&) {}
  }
  return committedParsed_;
}

void CutoutPanel::updateOnion() {
  ghostInfo_.clear();
  const int clip = rig_ && mode_ == Mode::Animate && !showingPreview() ? rig_->clipIndex(ss(clipId_)) : -1;
  updateTrail(clip);
  canvas_->setTrail(trailPoints_, trailKeys_);
  if (!onion_.enabled || clip < 0) { canvas_->setGhosts({}, {}); return; }
  // Ghosts are pure clip samples (cutout::samplePose): the controller preview's mixes and layers
  // are runtime state with no fixed time, so ghosting is only offered on the clip being edited.
  const Json* data = findIn(displayData(), "clips", ss(clipId_));
  const bool loop = data && data->value("loop", true);
  const double duration = double(rig_->clips[size_t(clip)].durationMs);
  const QVector<GhostTime> times = onionTimes(onion_, playheadExact_, duration, loop, onion_.keysOnly ? onionKeyTimes(clip) : QVector<int>());
  int counts[2] = {0, 0};
  for (const auto& t : times) counts[t.offset > 0]++;
  QVector<RigCanvas::Ghost> ghosts;
  for (const auto& t : times) {
    const QColor tint = t.offset < 0 ? onion_.beforeColor : onion_.afterColor;
    const double alpha = ghostAlpha(onion_, t.offset, counts[t.offset > 0]);
    ghosts.push_back({rig_, ghostCache_.pose(*rig_, displayGeneration_, clip, t.sampleMs, lookFor(*rig_)), tint, alpha, t.offset,
                      quint64(qHashMulti(0, displayGeneration_, clip, qint64(std::llround(t.sampleMs * 1000))))});
    ghostInfo_.push_back({t.sampleMs, t.offset, alpha, tint, false});
  }
  if (onion_.includeCurrent) {
    // The saved pose at the playhead: shows where a pending change or an ongoing drag started.
    const auto saved = committedParsed();
    const int savedClip = saved ? saved->clipIndex(ss(clipId_)) : -1;
    if (saved && savedClip >= 0 && saved->bones.size() == rig_->bones.size()) {
      const double t = std::clamp(playheadExact_, 0.0, double(saved->clips[size_t(savedClip)].durationMs));
      const quint64 generation = quint64(qHashMulti(0, committedParsedKey_, saved.get() == rig_.get() ? displayGeneration_ : 0, previewSkin_, previewExpression_));
      const QColor gray("#d1d5db");
      ghosts.push_back({saved, committedCache_.pose(*saved, generation, savedClip, t, lookFor(*saved)), gray, ghostAlpha(onion_, 0, 1), 0,
                        quint64(qHashMulti(1, generation, savedClip, qint64(std::llround(t * 1000))))});
      ghostInfo_.push_back({t, 0, ghostAlpha(onion_, 0, 1), gray, true});
    }
  }
  canvas_->setGhosts(ghosts, ghostHiddenSlots());
}
void CutoutPanel::updateTrail(int clip) {
  const int bone = clip >= 0 && onion_.trail ? boneIndex(selectedBone()) : -1;
  if (bone < 0) {
    trailKey_.clear();
    trailTimes_.clear();
    trailPoints_.clear();
    trailKeys_.clear();
    trailKeyTimes_.clear();
    return;
  }
  const QString key = QString("%1/%2/%3").arg(displayGeneration_).arg(clip).arg(bone);
  if (key == trailKey_) return;
  trailKey_ = key;
  const auto& c = rig_->clips[size_t(clip)];
  trailTimes_ = cutout_studio::trailTimes(double(c.durationMs));
  trailPoints_.clear();
  auto origin = [&](double t) {
    const cutout::Pose pose = cutout::samplePose(*rig_, clip, t, lookFor(*rig_));
    return QPointF(pose.world[size_t(bone)].e, pose.world[size_t(bone)].f);
  };
  for (double t : trailTimes_) trailPoints_ << origin(t);
  // Dots at keys of the bone and of its ancestors: those are the keys that move its origin.
  QSet<QString> chain;
  for (int b = bone, guard = 0; b >= 0 && guard <= int(rig_->bones.size()); b = rig_->bones[size_t(b)].parentIndex, guard++) chain.insert(qs(rig_->bones[size_t(b)].id));
  std::set<int> times;
  for (const auto& t : c.tracks)
    if (t.bone && chain.contains(qs(t.target))) for (const auto& k : t.keys) times.insert(int(k.timeMs));
  trailKeyTimes_ = QVector<int>(times.begin(), times.end());
  trailKeys_.clear();
  for (int t : trailKeyTimes_) trailKeys_ << origin(t);
}

// ---------------------------------------------------------------- navigation
void CutoutPanel::selectRig(const QString& id) {
  if (id == rigId_ || !rigs_.contains(id)) return;
  if (working_) { working_.reset(); setStatus("다른 캐릭터로 바꿔 키로 저장하지 않은 변경을 버렸습니다."); }
  rigId_ = id;
  selection_.clear();
  clipId_.clear();
  controllerId_.clear();
  previewSkin_.clear();
  previewExpression_.clear();
  lastSetupDelta_.reset();
  for (auto it = controllers_.begin(); it != controllers_.end(); ++it)
    if (qs(it->data.value("rig", std::string())) == id) { controllerId_ = it.key(); break; }
  previewKey_.clear();
  rebuildDisplay();
  canvas_->fitToCanvas();
}
void CutoutPanel::selectController(const QString& id) {
  if (!controllers_.contains(id)) return;
  const QString rig = qs(controllers_[id].data.value("rig", std::string()));
  if (rig != rigId_ && rigs_.contains(rig)) selectRig(rig);
  controllerId_ = id;
  previewKey_.clear();
  rebuildDisplay();
}
void CutoutPanel::selectItem(const QString& ref) {
  selection_ = ref;
  {
    QSignalBlocker b1(tree_), b2(drawOrder_);
    filling_ = true;
    QTreeWidgetItemIterator it(tree_);
    tree_->setCurrentItem(nullptr);
    for (; *it; ++it) if ((*it)->data(0, Qt::UserRole).toString() == ref) { tree_->setCurrentItem(*it); break; }
    drawOrder_->setCurrentItem(nullptr);
    for (int i = 0; i < drawOrder_->count(); i++) if ("slot:" + drawOrder_->item(i)->data(Qt::UserRole).toString() == ref) drawOrder_->setCurrentRow(i);
    filling_ = false;
  }
  refreshProperties();
  refreshGraph();
  updateCanvas();
}
void CutoutPanel::selectClip(const QString& id) {
  if (!clipIds().contains(id)) return;
  if (id != clipId_ && working_) { working_.reset(); setStatus("클립을 바꿔 키로 저장하지 않은 변경을 버렸습니다."); }
  clipId_ = id;
  playheadMs_ = 0;
  playheadExact_ = 0;
  rebuildDisplay();
}
void CutoutPanel::setMode(Mode mode) {
  if (mode == mode_) { refreshModeUi(); return; }
  if (working_) { working_.reset(); setStatus("모드를 바꿔 키로 저장하지 않은 변경을 버렸습니다."); }
  if (playing_) playButton_->setChecked(false);
  mode_ = mode;
  if (mode == Mode::Animate) {
    bottomTabs_->setCurrentIndex(0);
    if (tool() == RigCanvas::Pivot) setTool(RigCanvas::Move);
  }
  rebuildDisplay();
}
int CutoutPanel::tool() const { return canvas_->tool(); }
void CutoutPanel::setTool(int t) {
  if (t == RigCanvas::Pivot && mode_ == Mode::Animate) { setStatus("회전 중심은 조립 모드에서만 바꿀 수 있습니다.", true); return; }
  canvas_->setTool(t);
  for (int i = 0; i < 4; i++) toolButtons_[i]->setChecked(i == t);
}
void CutoutPanel::showControllerTab() { bottomTabs_->setCurrentIndex(1); }
void CutoutPanel::setAutoKey(bool on) { autoKeyCheck_->setChecked(on); }
void CutoutPanel::setPlayhead(int ms) {
  int duration = 1000;
  if (const Json* clip = findIn(displayData(), "clips", ss(clipId_))) duration = int(clip->value("durationMs", int64_t(1000)));
  ms = std::clamp(ms, 0, duration);
  if (ms != playheadMs_ && working_) { working_.reset(); setStatus("시간을 옮겨 키로 저장하지 않은 변경을 버렸습니다."); rebuildDisplay(); }
  playheadMs_ = ms;
  playheadExact_ = ms;
  timeView_->setPlayhead(ms);
  timeView_->reveal(ms);
  updateTimeUi(duration);
  refreshModeUi();
  refreshProperties();
  updateCanvas();
}

void CutoutPanel::onDiagnosticClicked(const QString& file, const QString& pointer) {
  const QString name = QFileInfo(file).completeBaseName();
  if (file.startsWith(RIG_DIR)) selectRig(name);
  else if (file.startsWith(CONTROLLER_DIR)) { selectController(name); showControllerTab(); return; }
  static const QRegularExpression re("/(bones|slots|clips|attachments|skins|expressions)/(\\d+)");
  const auto m = re.match(pointer);
  if (!m.hasMatch()) return;
  const Json data = committedRig();
  const size_t index = m.captured(2).toUInt();
  const Json& list = listOf(data, ss(m.captured(1)).c_str());
  if (index >= list.size()) return;
  const QString kind = m.captured(1);
  if (kind == "skins" || kind == "expressions") {
    lookTabs_->setCurrentIndex(kind == "skins" ? 0 : 1);
    if (kind == "skins") setPreviewSkin(qs(list[index].value("id", std::string())));
    else setPreviewExpression(qs(list[index].value("id", std::string())));
    return;
  }
  if (kind == "bones") selectItem("bone:" + qs(list[index].value("id", std::string())));
  else if (kind == "slots") selectItem("slot:" + qs(list[index].value("id", std::string())));
  else if (kind == "attachments") selectItem("slot:" + qs(list[index].value("slot", std::string())));
  else { setMode(Mode::Animate); selectClip(qs(list[index].value("id", std::string()))); }
}

// ---------------------------------------------------------------- characters
bool CutoutPanel::newCharacter() {
  if (busy()) return false;
  QString base = "character";
  for (int n = 2; rigs_.contains(base) || controllers_.contains(base); n++) base = QString("character-%1").arg(n);
  const QString id = askText("새 캐릭터", "캐릭터 ID (영문 소문자·숫자·하이픈)", base);
  if (id.isEmpty()) return false;
  if (!cutout::validId(ss(id))) { setStatus("ID는 영문 소문자로 시작하고 소문자·숫자·하이픈만 쓸 수 있습니다: " + id, true); return false; }
  if (rigs_.contains(id) || controllers_.contains(id)) { setStatus("이미 있는 캐릭터 ID입니다: " + id, true); return false; }
  // Canvas 512x768 with the anchor between the feet; root bone sits on the anchor.
  const Json rig = {{"schemaVersion", 1}, {"name", ss(id)}, {"canvas", {{"width", 512}, {"height", 768}, {"originX", 256}, {"originY", 720}}},
                    {"bones", Json::array({{{"id", "root"}, {"parent", nullptr}, {"x", 256}, {"y", 720}}})},
                    {"slots", Json::array()}, {"attachments", Json::array()},
                    {"clips", Json::array({{{"id", "idle"}, {"name", "대기"}, {"durationMs", 1000}, {"loop", true}, {"tracks", Json::array()}}})},
                    {"mix", {{"defaultMs", 150}, {"pairs", Json::array()}}}};
  const Json controller = {{"schemaVersion", 1}, {"rig", ss(id)}, {"defaultClip", "idle"}, {"layers", Json::array()}, {"bindings", Json::array()}};
  auto envelope = [&](const char* type, const Json& data) {
    return QJsonObject{{"formatVersion", "0.1.0"}, {"plugin", PLUGIN_ID}, {"type", type}, {"id", id}, {"data", toQtObject(data)}};
  };
  const QJsonArray edits{QJsonObject{{"type", "create"}, {"file", QString(RIG_DIR) + id + ".json"}, {"data", envelope("rig", rig)}},
                         QJsonObject{{"type", "create"}, {"file", QString(CONTROLLER_DIR) + id + ".json"}, {"data", envelope("controller", controller)}}};
  if (!submit(edits)) return false;
  rigId_ = id;
  controllerId_ = id;
  selection_ = "bone:root";
  clipId_ = "idle";
  setMode(Mode::Setup);
  setStatus("캐릭터 「" + id + "」를 만들었습니다. 「이미지 조각 추가…」로 몸 조각을 붙이세요.");
  return true;
}
bool CutoutPanel::renameCharacter() {
  if (!rigDoc()) { setStatus("이름을 바꿀 캐릭터를 선택하세요.", true); return false; }
  const QString current = qs(committedRig().value("name", ss(rigId_)));
  const QString name = askText("이름 변경", "표시 이름 (파일 ID는 바뀌지 않아 게임 연결이 유지됩니다)", current);
  if (name.isEmpty() || name == current) return false;
  return runOps({{{"op", "setName"}, {"name", ss(name)}}}, false);
}
bool CutoutPanel::deleteCharacter() {
  const Doc* doc = rigDoc();
  if (!doc) { setStatus("삭제할 캐릭터를 선택하세요.", true); return false; }
  QJsonArray edits{QJsonObject{{"type", "delete"}, {"file", doc->file}}};
  QStringList names;
  for (auto it = controllers_.begin(); it != controllers_.end(); ++it)
    if (qs(it->data.value("rig", std::string())) == rigId_) { edits.append(QJsonObject{{"type", "delete"}, {"file", it->file}}); names << it.key(); }
  if (!askConfirm("캐릭터 삭제", QString("「%1」 리그%2를 삭제할까요? 되돌리기(Ctrl+Z)로 복구할 수 있습니다.")
                                    .arg(rigId_, names.isEmpty() ? QString() : " 와 게임 연결 " + names.join(", "))))
    return false;
  if (!submit(edits)) return false;
  setStatus("캐릭터를 삭제했습니다. 되돌리기: Ctrl+Z");
  return true;
}

// ---------------------------------------------------------------- structure gestures
bool CutoutPanel::addBone() {
  if (!rigDoc()) { setStatus("먼저 캐릭터를 만드세요.", true); return false; }
  const QString parent = selectedBone();
  const QString id = askText("본 추가", parent.isEmpty() ? "새 루트 본 ID" : "「" + parent + "」 아래 새 본 ID", uniqueBoneId("bone"));
  if (id.isEmpty()) return false;
  Json op = {{"op", "addBone"}, {"id", ss(id)}, {"parent", parent.isEmpty() ? Json(nullptr) : Json(ss(parent))}};
  // Place it a little above the parent origin so it is immediately visible and grabbable.
  const auto world = cutout::setupWorld(displayData());
  if (!parent.isEmpty() && world.count(ss(parent))) { op["worldX"] = world.at(ss(parent)).e; op["worldY"] = world.at(ss(parent)).f - 60; }
  else if (rig_) { op["worldX"] = rig_->width / 2; op["worldY"] = rig_->height / 2; }
  const QString before = selection_;
  selection_ = "bone:" + id;
  if (runOps({op}, false)) return true;
  selection_ = before;
  return false;
}
bool CutoutPanel::addPartInteractive() {
  if (!rigDoc()) { setStatus("먼저 캐릭터를 만드세요.", true); return false; }
  PartChoice choice;
  choice.bone = selectedBone().isEmpty() && rig_ && !rig_->bones.empty() ? qs(rig_->bones[0].id) : selectedBone();
  if (refKind(selection_) == "slot") choice.slot = refId(selection_);
  if (!askPart || !askPart(choice)) return false;
  if (!choice.importFile.isEmpty()) { importAndAddPart(choice.importFile, choice.slot, choice.name, choice.bone); return true; }
  return addPart(choice.assetId, choice.slot, choice.name, choice.bone);
}
bool CutoutPanel::addPart(const QString& assetId, const QString& slotIn, const QString& name, const QString& bone) {
  const QSize size = images_.pixelSize(assetId);
  if (!size.isValid() || size.isEmpty()) { setStatus("이미지 크기를 읽을 수 없습니다: " + assetId, true); return false; }
  const QString slot = slotIn.isEmpty() ? qs(cutout::slugId(ss(assetId), "part")) : slotIn;
  Json op = {{"op", "addPart"}, {"slot", ss(slot)}, {"imageAssetId", ss(assetId)}, {"width", size.width()}, {"height", size.height()}};
  if (!bone.isEmpty()) op["bone"] = ss(bone);
  if (!name.isEmpty()) op["name"] = ss(name);
  const QString before = selection_;
  selection_ = "slot:" + slot;
  if (runOps({op}, false)) return true;
  selection_ = before;
  return false;
}
void CutoutPanel::importAndAddPart(const QString& file, const QString& slot, const QString& name, const QString& bone) {
  if (busy()) return;
  QPointer<CutoutPanel> self(this);
  const QString fallbackSlot = slot.isEmpty() ? qs(cutout::slugId(ss(QFileInfo(file).completeBaseName()), "part")) : slot;
  inSubmit_ = true;
  host_->request("importImage", {{"file", file}}, [self, fallbackSlot, name, bone](QJsonObject reply) {
    if (!self) return;
    const QString id = reply.value("importedAssetId").toString();
    if (!reply.value("error").toString().isEmpty() || id.isEmpty()) {
      self->setStatus("PNG를 가져오지 못했습니다: " + reply.value("error").toString(), true);
      return;
    }
    // Next turn of the event loop: the host is no longer busy and assets() includes the image.
    QTimer::singleShot(0, self, [self, id, fallbackSlot, name, bone] {
      if (!self || !self->host_) return;
      self->images_.setAssets(self->host_->assets());
      self->addPart(id, fallbackSlot, name, bone);
    });
  });
  inSubmit_ = false;
}
bool CutoutPanel::deleteSelected() {
  const QString kind = refKind(selection_), id = refId(selection_);
  if (kind == "bone") {
    int parts = 0;
    for (const auto& s : listOf(displayData(), "slots")) parts += qs(s.value("bone", std::string())) == id;
    if (!askConfirm("본 삭제", QString("본 「%1」를 삭제할까요?%2 자식 본은 위 본으로 옮겨집니다.").arg(id, parts ? QString(" 붙어 있는 조각 %1개도 삭제됩니다.").arg(parts) : QString())))
      return false;
    return runOps({{{"op", "deleteBone"}, {"bone", ss(id)}, {"cascade", true}}}, false);
  }
  if (kind == "slot") {
    if (!askConfirm("조각 삭제", "조각 「" + id + "」와 그 그림·키를 삭제할까요?")) return false;
    return runOps({{{"op", "deleteSlot"}, {"slot", ss(id)}}}, false);
  }
  setStatus("삭제할 본이나 조각을 선택하세요.", true);
  return false;
}
bool CutoutPanel::requestReparent(const QString& bone, const QString& parent) {
  return runOps({{{"op", "reparentBone"}, {"bone", ss(bone)}, {"parent", parent.isEmpty() ? Json(nullptr) : Json(ss(parent))}}}, false);
}
bool CutoutPanel::requestSlotBone(const QString& slot, const QString& bone) {
  return runOps({{{"op", "setSlot"}, {"slot", ss(slot)}, {"bone", ss(bone)}}}, false);
}
bool CutoutPanel::moveSelectedSlot(int delta) {
  if (refKind(selection_) != "slot") { setStatus("순서를 바꿀 조각을 선택하세요.", true); return false; }
  return runOps({{{"op", "moveSlot"}, {"slot", ss(refId(selection_))}, {"delta", delta}}}, false);
}

// ---------------------------------------------------------------- canvas / properties
QPointF CutoutPanel::manipulationOrigin(const QString& ref) const {
  if (!rig_) return {};
  const cutout::Pose pose = currentPose();
  const QString kind = refKind(ref), id = refId(ref);
  QString bone = kind == "bone" ? id : QString();
  if (kind == "slot") {
    const int si = rig_->slotIndex(ss(id));
    if (si < 0) return {};
    bone = qs(rig_->slots[size_t(si)].bone);
    if (mode_ == Mode::Setup && size_t(si) < pose.attachments.size() && pose.attachments[size_t(si)])
      if (const auto* att = rig_->attachment(ss(id), *pose.attachments[size_t(si)], pose.skin)) {
        double x, y;
        pose.world[size_t(rig_->slots[size_t(si)].boneIndex)].apply(att->offset.x, att->offset.y, x, y);
        return {x, y};
      }
  }
  const int bi = rig_->boneIndex(ss(bone));
  if (bi < 0 || size_t(bi) >= pose.world.size()) return {};
  return {pose.world[size_t(bi)].e, pose.world[size_t(bi)].f};
}

std::vector<Json> CutoutPanel::dragOps(const RigCanvas::Drag& d) const {
  if (!dragRig_ || dragPose_.world.size() != dragRig_->bones.size()) return {};
  const cutout::Rig& rig = *dragRig_;
  const cutout::Pose& pose = dragPose_;
  const bool animate = mode_ == Mode::Animate;
  const double k = d.shift ? 0.25 : 1.0;  // Shift = fine adjustment
  const QPointF delta = (d.current - d.start) * k;
  auto snap = [&](double r) { return d.shift ? std::round(r / 15) * 15 : r; };
  const double ratio = d.shift ? 1 + (d.ratio - 1) * 0.25 : d.ratio;
  QString kind = refKind(d.ref), id = refId(d.ref);
  if (d.tool == RigCanvas::Pivot) {
    if (animate) return {};
    QString bone = kind == "bone" ? id : QString();
    if (kind == "slot") { const int si = rig.slotIndex(ss(id)); if (si >= 0) bone = qs(rig.slots[size_t(si)].bone); }
    if (bone.isEmpty()) return {};
    return {{{"op", "setPivot"}, {"bone", ss(bone)}, {"worldX", d.current.x()}, {"worldY", d.current.y()}}};
  }
  if (kind == "slot" && !animate) {
    const int si = rig.slotIndex(ss(id));
    if (si < 0 || size_t(si) >= pose.attachments.size() || !pose.attachments[size_t(si)]) return {};
    const auto* att = rig.attachment(ss(id), *pose.attachments[size_t(si)], pose.skin);
    if (!att) return {};
    const cutout::Affine& boneWorld = pose.world[size_t(rig.slots[size_t(si)].boneIndex)];
    // The previewed skin's own image is placed in that skin; otherwise the default attachment.
    const QString skin = skinProviding(id, qs(att->name));
    Json op = skin.isEmpty() ? Json{{"op", "setAttachment"}, {"slot", ss(id)}, {"name", att->name}}
                             : Json{{"op", "setSkinAttachment"}, {"skin", ss(skin)}, {"slot", ss(id)}, {"name", att->name}};
    if (d.tool == RigCanvas::Move) {
      double ax, ay;
      boneWorld.apply(att->offset.x, att->offset.y, ax, ay);
      const auto inv = boneWorld.inverse();
      if (!inv) return {};
      double lx, ly;
      inv->apply(ax + delta.x(), ay + delta.y(), lx, ly);
      op["x"] = lx; op["y"] = ly;
    } else if (d.tool == RigCanvas::Rotate) {
      const double sign = boneWorld.a * boneWorld.d - boneWorld.b * boneWorld.c < 0 ? -1 : 1;
      op["rotation"] = snap(att->offset.rotation + sign * d.angle);
    } else {
      op["scaleX"] = att->offset.scaleX * ratio; op["scaleY"] = att->offset.scaleY * ratio;
    }
    return {op};
  }
  if (kind == "slot") {  // Animate: parts are moved through their bone (only bones are keyable)
    const int si = rig.slotIndex(ss(id));
    if (si < 0) return {};
    id = qs(rig.slots[size_t(si)].bone);
  }
  const int bi = rig.boneIndex(ss(id));
  if (bi < 0) return {};
  const cutout::Transform cur = pose.bones[size_t(bi)];
  const int parent = rig.bones[size_t(bi)].parentIndex;
  const cutout::Affine pw = parent >= 0 ? pose.world[size_t(parent)] : cutout::Affine{};
  QMap<QString, double> values;
  if (d.tool == RigCanvas::Move) {
    const auto inv = pw.inverse();
    if (!inv) return {};
    double lx, ly;
    inv->apply(pose.world[size_t(bi)].e + delta.x(), pose.world[size_t(bi)].f + delta.y(), lx, ly);
    values = {{"x", lx}, {"y", ly}};
  } else if (d.tool == RigCanvas::Rotate) {
    const double sign = pw.a * pw.d - pw.b * pw.c < 0 ? -1 : 1;
    values = {{"rotation", snap(cur.rotation + sign * d.angle)}};
  } else {
    values = {{"scaleX", cur.scaleX * ratio}, {"scaleY", cur.scaleY * ratio}};
  }
  std::vector<Json> ops;
  if (!animate) {
    Json op = {{"op", "setBone"}, {"bone", ss(id)}};
    for (auto it = values.begin(); it != values.end(); ++it) op[ss(it.key())] = it.value();
    ops.push_back(op);
  } else {
    for (auto it = values.begin(); it != values.end(); ++it)
      ops.push_back({{"op", "setKey"}, {"clip", ss(clipId_)}, {"target", "bone:" + ss(id)}, {"property", ss(it.key())}, {"timeMs", playheadMs_}, {"value", it.value()}});
  }
  return ops;
}

void CutoutPanel::onCanvasDrag(const RigCanvas::Drag& d, bool finished) {
  if (!dragActive_) {
    dragActive_ = true;
    dragBase_ = working_ ? *working_ : committedRig();
    dragRig_ = rig_;
    dragPose_ = currentPose(false);  // deltas apply to the keyable values, never to a previewed expression
  }
  if (mode_ == Mode::Animate && clipId_.isEmpty()) { setStatus("키를 넣을 클립을 먼저 만드세요.", true); if (finished) dragActive_ = false; return; }
  if (d.tool == RigCanvas::Pivot && mode_ == Mode::Animate) { setStatus("회전 중심은 조립 모드에서만 바꿀 수 있습니다.", true); if (finished) dragActive_ = false; return; }
  const std::vector<Json> ops = dragOps(d);
  if (!finished) {
    if (ops.empty()) return;
    try { dragPreview_ = cutout::applyEdit(dragBase_, batch(ops)).data; } catch (const std::exception&) { return; }
    rebuildDisplay(false);
    return;
  }
  dragActive_ = false;
  dragPreview_.reset();
  if (ops.empty()) { rebuildDisplay(false); return; }
  // Remember setup deltas of keyed bones so the user can rebase existing keys explicitly.
  if (mode_ == Mode::Setup && ops[0].value("op", std::string()) == "setBone") {
    const std::string bone = ops[0].value("bone", std::string());
    const Json* before = findIn(dragBase_, "bones", bone);
    Json delta = Json::object();
    for (const char* p : {"x", "y", "rotation", "scaleX", "scaleY"})
      if (ops[0].contains(p) && before) delta[p] = ops[0].at(p).get<double>() - before->value(p, std::string(p).rfind("scale", 0) == 0 ? 1.0 : 0.0);
    if (runOps(ops, false)) { lastSetupDelta_ = SetupDelta{qs(bone), delta}; refreshModeUi(); }
    return;
  }
  runOps(ops, mode_ == Mode::Animate);
}

void CutoutPanel::commitProperty(const QString& property, double value) {
  if (filling_) return;
  QDoubleSpinBox* spin = props_.value(property);
  const double shown = spin ? spin->property("shown").toDouble() : 0;
  if (spin && std::fabs(value - shown) < 0.5 * std::pow(10.0, -spin->decimals())) return;  // focus-out without change
  const QString kind = refKind(selection_), id = refId(selection_);
  const bool animate = mode_ == Mode::Animate;
  if (kind == "bone") {
    if (animate) {
      if (clipId_.isEmpty()) { setStatus("키를 넣을 클립을 먼저 만드세요.", true); return; }
      runOps({{{"op", "setKey"}, {"clip", ss(clipId_)}, {"target", ss(selection_)}, {"property", ss(property)}, {"timeMs", playheadMs_}, {"value", value}}}, true);
    } else if (runOps({{{"op", "setBone"}, {"bone", ss(id)}, {ss(property), value}}}, false)) {
      lastSetupDelta_ = SetupDelta{id, Json{{ss(property), value - shown}}};
      refreshModeUi();
    }
  } else if (kind == "slot") {
    if (property == "alpha") {
      if (animate) runOps({{{"op", "setKey"}, {"clip", ss(clipId_)}, {"target", ss(selection_)}, {"property", "alpha"}, {"timeMs", playheadMs_}, {"value", value}}}, true);
      else runOps({{{"op", "setSlot"}, {"slot", ss(id)}, {"alpha", value}}}, false);
      return;
    }
    const QString name = attachmentCombo_->currentData().toString();
    if (animate || name.isEmpty()) { setStatus("조각 배치는 조립 모드에서 표시 중인 첨부에 대해 바꿉니다.", true); refreshProperties(); return; }
    if (const QString skin = skinProviding(id, name); !skin.isEmpty())
      runOps({{{"op", "setSkinAttachment"}, {"skin", ss(skin)}, {"slot", ss(id)}, {"name", ss(name)}, {ss(property), value}}}, false);
    else runOps({{{"op", "setAttachment"}, {"slot", ss(id)}, {"name", ss(name)}, {ss(property), value}}}, false);
  }
}
void CutoutPanel::commitAttachmentChoice() {
  if (filling_ || refKind(selection_) != "slot") return;
  const QString name = attachmentCombo_->currentData().toString();
  const Json value = name.isEmpty() ? Json(nullptr) : Json(ss(name));
  if (mode_ == Mode::Animate) {
    if (clipId_.isEmpty()) { setStatus("키를 넣을 클립을 먼저 만드세요.", true); return; }
    runOps({{{"op", "setKey"}, {"clip", ss(clipId_)}, {"target", ss(selection_)}, {"property", "attachment"}, {"timeMs", playheadMs_}, {"value", value}}}, true);
  } else {
    runOps({{{"op", "setSlot"}, {"slot", ss(refId(selection_))}, {"attachment", value}}}, false);
  }
}

bool CutoutPanel::keySelected() {
  if (mode_ != Mode::Animate) { setStatus("키는 애니메이션 모드에서 추가합니다.", true); return false; }
  if (clipId_.isEmpty()) { setStatus("키를 넣을 클립을 먼저 만드세요.", true); return false; }
  if (working_) {
    cutout::EditResult r;
    r.data = *working_;
    return commitRig(r);
  }
  if (!rig_ || selection_.isEmpty() || refKind(selection_) == "rig") { setStatus("키를 넣을 본이나 조각을 선택하세요(표정은 「표정 키」).", true); return false; }
  const cutout::Pose pose = currentPose(false);
  const QString kind = refKind(selection_), id = refId(selection_);
  std::vector<Json> ops;
  auto key = [&](const char* prop, const Json& v) {
    ops.push_back({{"op", "setKey"}, {"clip", ss(clipId_)}, {"target", ss(selection_)}, {"property", prop}, {"timeMs", playheadMs_}, {"value", v}});
  };
  if (kind == "bone") {
    const int i = rig_->boneIndex(ss(id));
    if (i < 0) return false;
    const auto& t = pose.bones[size_t(i)];
    key("x", t.x); key("y", t.y); key("rotation", t.rotation); key("scaleX", t.scaleX); key("scaleY", t.scaleY);
  } else {
    const int i = rig_->slotIndex(ss(id));
    if (i < 0) return false;
    key("attachment", pose.attachments[size_t(i)] ? Json(*pose.attachments[size_t(i)]) : Json(nullptr));
  }
  return runOps(ops, false);
}
bool CutoutPanel::discardPending() {
  if (!working_) return false;
  working_.reset();
  setStatus("키로 저장하지 않은 변경을 취소했습니다.");
  rebuildDisplay();
  return true;
}
bool CutoutPanel::applySetupDeltaToKeys() {
  if (!lastSetupDelta_) return false;
  const SetupDelta d = *lastSetupDelta_;
  lastSetupDelta_.reset();
  refreshModeUi();
  return runOps({{{"op", "applySetupDeltaToKeys"}, {"bone", ss(d.bone)}, {"delta", d.delta}}}, false);
}

// ---------------------------------------------------------------- clips
bool CutoutPanel::newClip() {
  if (!rigDoc()) { setStatus("먼저 캐릭터를 만드세요.", true); return false; }
  const QString id = askText("새 클립", "클립 ID (예: joy, talk, walk)", uniqueClipId("clip"));
  if (id.isEmpty()) return false;
  const QString before = clipId_;
  clipId_ = id;
  if (runOps({{{"op", "addClip"}, {"id", ss(id)}, {"name", ss(id)}, {"durationMs", 1000}, {"loop", true}}}, false)) { playheadMs_ = 0; playheadExact_ = 0; return true; }
  clipId_ = before;
  return false;
}
bool CutoutPanel::duplicateClip() {
  if (clipId_.isEmpty()) { setStatus("복제할 클립을 선택하세요.", true); return false; }
  const QString id = askText("클립 복제", "새 클립 ID", uniqueClipId(clipId_ + "-copy"));
  if (id.isEmpty()) return false;
  const QString before = clipId_;
  clipId_ = id;
  if (runOps({{{"op", "duplicateClip"}, {"clip", ss(before)}, {"id", ss(id)}}}, false)) return true;
  clipId_ = before;
  return false;
}
bool CutoutPanel::renameClip() {
  if (clipId_.isEmpty()) { setStatus("이름을 바꿀 클립을 선택하세요.", true); return false; }
  const QString id = askText("클립 이름 변경", "새 클립 ID (게임 연결·전환 표도 함께 바뀝니다)", clipId_);
  if (id.isEmpty() || id == clipId_) return false;
  const QString before = clipId_;
  clipId_ = id;
  if (runOps({{{"op", "renameClip"}, {"clip", ss(before)}, {"id", ss(id)}, {"name", ss(id)}}}, false)) return true;
  clipId_ = before;
  return false;
}
bool CutoutPanel::deleteClip() {
  if (clipId_.isEmpty()) { setStatus("삭제할 클립을 선택하세요.", true); return false; }
  if (!askConfirm("클립 삭제", "클립 「" + clipId_ + "」와 그 키를 삭제할까요?")) return false;
  return runOps({{{"op", "deleteClip"}, {"clip", ss(clipId_)}}}, false);
}
void CutoutPanel::commitClipSettings() {
  if (filling_ || clipId_.isEmpty()) return;
  const Json* clip = findIn(displayData(), "clips", ss(clipId_));
  if (!clip) return;
  Json op = {{"op", "setClip"}, {"clip", ss(clipId_)}};
  if (clipDuration_->value() != clip->value("durationMs", int64_t(0))) op["durationMs"] = clipDuration_->value();
  if (clipLoop_->isChecked() != clip->value("loop", true)) op["loop"] = clipLoop_->isChecked();
  if (op.size() == 2) return;
  if (op.contains("durationMs")) {
    try { cutout::applyEdit(displayData(), op); } catch (const cutout::EditError&) {
      if (!askConfirm("클립 길이", "새 길이 밖에 있는 키와 이벤트를 잘라낼까요?")) { refreshClips(); return; }
      op["trim"] = true;
    }
  }
  if (!runOps({op}, false)) refreshClips();
}
bool CutoutPanel::applyBreathing() {
  if (!rigDoc() || !rig_) { setStatus("먼저 캐릭터를 만드세요.", true); return false; }
  QString bone = selectedBone();
  if (bone.isEmpty()) bone = qs(rig_->bones.front().id);
  const QString id = uniqueClipId("breathing");
  cutout::Host core;
  const Json reply = core.handle({{"op", "template"}, {"kind", "breathing"}, {"rig", displayData()}, {"clipId", ss(id)},
                                  {"params", {{"bone", ss(bone)}, {"property", "scaleY"}, {"amplitude", 0.03}, {"periodMs", 3200}}}});
  const QString before = clipId_;
  clipId_ = id;
  if (runOps({{{"op", "addClip"}, {"clip", reply.at("clip")}}}, false)) { setStatus("숨쉬기 클립 「" + id + "」를 만들었습니다(본 " + bone + ", 3.2초 주기)."); return true; }
  clipId_ = before;
  return false;
}
bool CutoutPanel::applyBlink() {
  if (!rigDoc() || !rig_) { setStatus("먼저 캐릭터를 만드세요.", true); return false; }
  // Pick the eye slot: the selected part, otherwise the first part with 2+ attachments.
  QString slot = refKind(selection_) == "slot" ? refId(selection_) : QString();
  auto names = [&](const QString& s) { QStringList out; for (const auto& a : rig_->attachments) if (qs(a.slot) == s) out << qs(a.name); return out; };
  if (names(slot).size() < 2) {
    slot.clear();
    for (const auto& s : rig_->slots) if (names(qs(s.id)).size() >= 2) { slot = qs(s.id); break; }
  }
  const QStringList atts = names(slot);
  if (atts.size() < 2) { setStatus("눈 조각에 뜬 눈·감은 눈 그림(첨부) 2개 이상이 필요합니다. 같은 조각 ID로 그림을 하나 더 추가하세요.", true); return false; }
  auto find = [&](std::initializer_list<const char*> words, const QString& fallback) {
    for (const QString& n : atts) for (const char* w : words) if (n.contains(w, Qt::CaseInsensitive)) return n;
    return fallback;
  };
  const QString open = find({"open", "default"}, atts.front());
  const QString closed = find({"close", "shut"}, atts.back());
  QString half = find({"half"}, atts.size() >= 3 ? atts[1] : QString());
  if (half == open || half == closed) half.clear();
  const QString id = uniqueClipId("blink");
  cutout::Host core;
  Json params = {{"slot", ss(slot)}, {"open", ss(open)}, {"closed", ss(closed)}, {"intervalMs", 4000}};
  if (!half.isEmpty()) params["half"] = ss(half);
  const Json reply = core.handle({{"op", "template"}, {"kind", "blink"}, {"rig", displayData()}, {"clipId", ss(id)}, {"params", params}});
  if (!reply.value("ok", false)) { setStatus(qs(reply.value("error", std::string())), true); return false; }
  cutout::EditResult result;
  try { result = cutout::applyEdit(working_ ? *working_ : committedRig(), {{"op", "addClip"}, {"clip", reply.at("clip")}}); }
  catch (const std::exception& e) { setStatus(qs(e.what()), true); return false; }
  // Register as an always-on layer in the same gesture (one batch edit).
  Json controller = committedController();
  if (controllerDoc()) {
    std::set<int> used;
    for (const auto& l : listOf(controller, "layers")) used.insert(l.value("track", 0));
    int track = 1;
    while (used.count(track) && track < 15) track++;
    if (!used.count(track)) {
      controller["layers"].push_back({{"track", track}, {"clip", ss(id)}});
      result.extra["controllers"] = Json::array({{{"id", ss(controllerId_)}, {"data", controller}}});
    }
  }
  const QString before = clipId_;
  clipId_ = id;
  if (commitRig(result)) { setStatus("눈깜빡임 클립 「" + id + "」를 만들고 게임 연결 레이어에 등록했습니다."); return true; }
  clipId_ = before;
  return false;
}

// ---------------------------------------------------------------- dopesheet gestures
bool CutoutPanel::moveKey(int row, int from, int to) {
  if (working_) { setStatus("먼저 K로 변경을 키로 저장하거나 변경을 취소하세요.", true); return false; }
  if (row < 0 || row >= dopesheet_->rows().size()) return false;
  const auto& r = dopesheet_->rows()[row];
  const bool ok = runOps({{{"op", "moveKey"}, {"clip", ss(clipId_)}, {"target", ss(r.target)}, {"property", ss(r.property)}, {"fromMs", from}, {"toMs", to}}}, false);
  if (!ok) refreshDopesheet();
  return ok;
}
bool CutoutPanel::deleteKey(int row, int ms) {
  if (working_) { setStatus("먼저 K로 변경을 키로 저장하거나 변경을 취소하세요.", true); return false; }
  if (row < 0 || row >= dopesheet_->rows().size()) return false;
  const auto& r = dopesheet_->rows()[row];
  return runOps({{{"op", "deleteKey"}, {"clip", ss(clipId_)}, {"target", ss(r.target)}, {"property", ss(r.property)}, {"timeMs", ms}}}, false);
}
bool CutoutPanel::setKeyInterp(int row, int ms, const QString& interp) {
  if (working_) { setStatus("먼저 K로 변경을 키로 저장하거나 변경을 취소하세요.", true); return false; }
  if (row < 0 || row >= dopesheet_->rows().size()) return false;
  const auto& r = dopesheet_->rows()[row];
  return runOps({{{"op", "setInterp"}, {"clip", ss(clipId_)}, {"target", ss(r.target)}, {"property", ss(r.property)}, {"timeMs", ms}, {"interp", ss(interp)}}}, false);
}


// ---------------------------------------------------------------- graph editor gestures
bool CutoutPanel::moveGraphKey(const QString& target, const QString& property, int from, int to, double value) {
  if (working_) { setStatus("먼저 K로 변경을 키로 저장하거나 변경을 취소하세요.", true); refreshGraph(); return false; }
  const Json base = {{"clip", ss(clipId_)}, {"target", ss(target)}, {"property", ss(property)}};
  std::optional<double> current;
  if (const Json* clip = findIn(committedRig(), "clips", ss(clipId_)))
    for (const auto& t : listOf(*clip, "tracks"))
      if (qs(t.value("target", std::string())) == target && qs(t.value("property", std::string())) == property)
        for (const auto& k : listOf(t, "keys")) if (k.value("timeMs", int64_t(-1)) == from && k.contains("value") && k.at("value").is_number()) current = k.at("value").get<double>();
  if (!current) { setStatus("그 시간에 키가 없습니다.", true); refreshGraph(); return false; }
  // Time first (refused on collision), then the value at the new time: one batch = one edit.
  std::vector<Json> ops;
  if (to != from) { Json op = base; op["op"] = "moveKey"; op["fromMs"] = from; op["toMs"] = to; ops.push_back(op); }
  if (value != *current) { Json op = base; op["op"] = "setKey"; op["timeMs"] = to; op["value"] = value; ops.push_back(op); }
  if (ops.empty()) return false;
  const bool ok = runOps(ops, false);
  if (!ok) refreshGraph();
  return ok;
}
bool CutoutPanel::setKeyCurve(const QString& target, const QString& property, int ms, const cutout::Curve& curve) {
  if (working_) { setStatus("먼저 K로 변경을 키로 저장하거나 변경을 취소하세요.", true); refreshGraph(); return false; }
  const bool ok = runOps({{{"op", "setCurve"}, {"clip", ss(clipId_)}, {"target", ss(target)}, {"property", ss(property)}, {"timeMs", ms},
                           {"curve", Json::array({curve[0], curve[1], curve[2], curve[3]})}}}, false);
  if (!ok) refreshGraph();
  return ok;
}
bool CutoutPanel::applyPreset(const QString& preset) {
  if (working_) { setStatus("먼저 K로 변경을 키로 저장하거나 변경을 취소하세요.", true); return false; }
  const QVector<KeyRef> keys = timeView_->selection();  // shared by dopesheet and graph
  std::vector<Json> ops;
  for (const auto& k : keys)
    if (k.property != "attachment")
      ops.push_back({{"op", "setInterp"}, {"clip", ss(clipId_)}, {"target", ss(k.target)}, {"property", ss(k.property)}, {"timeMs", k.timeMs}, {"interp", ss(preset)}});
  if (ops.empty()) { setStatus("곡선을 바꿀 키를 먼저 고르세요(그래프·도프시트에서 키 클릭, Ctrl+클릭으로 여러 개). 그림 키는 항상 계단입니다.", true); return false; }
  QString label = preset;
  for (const auto& p : interpPresets()) if (preset == p.id) label = p.label;
  if (!runOps(ops, false)) return false;
  setStatus(QString("키 %1개에 「%2」 보간을 적용했습니다. 되돌리기: Ctrl+Z").arg(ops.size()).arg(label));
  return true;
}

// ---------------------------------------------------------------- key selection gestures
// Each gesture is one core op (moveKeys, scaleKeys, deleteKeys, pasteKeys, pastePose) in one
// host edit; the selection follows the keys to their new times before the commit rebuild.
bool CutoutPanel::refusePending() {
  if (!working_) return false;
  setStatus("먼저 K로 변경을 키로 저장하거나 변경을 취소하세요.", true);
  return true;
}
Json CutoutPanel::refsJson(const QVector<KeyRef>& keys) const {
  Json out = Json::array();
  for (const auto& k : keys) out.push_back({{"target", ss(k.target)}, {"property", ss(k.property)}, {"timeMs", k.timeMs}});
  return out;
}
void CutoutPanel::selectKeysFrom(const Json& refs) {
  QVector<KeyRef> keys;
  if (refs.is_array())
    for (const auto& r : refs) keys.push_back({qs(r.value("target", std::string())), qs(r.value("property", std::string())), int(r.value("timeMs", int64_t(0)))});
  timeView_->setSelection(keys);
}
void CutoutPanel::keyCommand(const QString& command) {
  if (command == "copy") copyKeys();
  else if (command == "paste") pasteKeys();
  else if (command == "duplicate") duplicateKeys();
}
bool CutoutPanel::moveKeys(const QVector<KeyRef>& keysIn, int deltaMs, const QHash<KeyRef, double>& values) {
  const QVector<KeyRef> keys = keysIn;  // callers may pass the live selection
  if (refusePending()) { refreshGraph(); return false; }
  if (keys.isEmpty() || clipId_.isEmpty()) { setStatus("옮길 키를 먼저 고르세요.", true); return false; }
  std::vector<Json> ops;
  if (deltaMs != 0) ops.push_back({{"op", "moveKeys"}, {"clip", ss(clipId_)}, {"keys", refsJson(keys)}, {"deltaMs", deltaMs}});
  // Values are set at the moved times, after the move (graph drags on the value axis).
  for (auto it = values.begin(); it != values.end(); ++it)
    ops.push_back({{"op", "setKey"}, {"clip", ss(clipId_)}, {"target", ss(it.key().target)}, {"property", ss(it.key().property)}, {"timeMs", it.key().timeMs + deltaMs}, {"value", it.value()}});
  if (ops.empty()) return false;
  const bool ok = runOps(ops, false, [&](const cutout::EditResult&) {
    QVector<KeyRef> moved;
    for (const auto& k : keys) moved.push_back({k.target, k.property, k.timeMs + deltaMs});
    timeView_->setSelection(moved);
  });
  if (!ok) { timeView_->setSelection(keys); refreshDopesheet(); refreshGraph(); return false; }
  setStatus(deltaMs ? QString("키 %1개를 %2ms 옮겼습니다(간격 유지). 되돌리기: Ctrl+Z").arg(keys.size()).arg(deltaMs)
                    : QString("키 %1개의 값을 바꿨습니다. 되돌리기: Ctrl+Z").arg(values.size()));
  return true;
}
bool CutoutPanel::scaleKeys(const QVector<KeyRef>& keysIn, int pivotMs, double factor) {
  const QVector<KeyRef> keys = keysIn;
  if (refusePending()) return false;
  if (keys.size() < 2 || clipId_.isEmpty()) { setStatus("시간 간격을 바꿀 키를 2개 이상 고르세요.", true); return false; }
  const bool ok = runOps({{{"op", "scaleKeys"}, {"clip", ss(clipId_)}, {"keys", refsJson(keys)}, {"pivotMs", pivotMs}, {"factor", factor}}}, false,
                         [this](const cutout::EditResult& r) { selectKeysFrom(r.extra.value("keys", Json::array())); });
  if (!ok) { refreshDopesheet(); return false; }
  setStatus(QString("키 %1개의 시간 간격을 %2%로 바꿨습니다(기준 %3ms, 곡선 모양 유지). 되돌리기: Ctrl+Z").arg(keys.size()).arg(factor * 100, 0, 'f', 1).arg(pivotMs));
  return true;
}
bool CutoutPanel::deleteKeys(const QVector<KeyRef>& keysIn) {
  const QVector<KeyRef> keys = keysIn;
  if (refusePending()) return false;
  if (keys.isEmpty() || clipId_.isEmpty()) { setStatus("삭제할 키를 먼저 고르세요.", true); return false; }
  if (!runOps({{{"op", "deleteKeys"}, {"clip", ss(clipId_)}, {"keys", refsJson(keys)}}}, false)) return false;
  setStatus(QString("키 %1개를 삭제했습니다. 되돌리기: Ctrl+Z").arg(keys.size()));
  return true;
}
bool CutoutPanel::offsetSelectedKeys(int deltaMs) {
  if (deltaMs == 0) return false;
  return moveKeys(timeView_->selection(), deltaMs);
}
bool CutoutPanel::scaleSelectedKeys(double factor) {
  const QVector<KeyRef> keys = timeView_->selection();
  int pivot = INT_MAX;
  for (const auto& k : keys) pivot = std::min(pivot, k.timeMs);
  if (factor == 1) return false;
  return scaleKeys(keys, keys.isEmpty() ? 0 : pivot, factor);
}
int CutoutPanel::clipboardKeys() const { return clipboard_.is_object() && clipboard_.contains("keys") ? int(clipboard_.at("keys").size()) : 0; }
bool CutoutPanel::copyKeys() {
  const QVector<KeyRef> keys = timeView_->selection();
  if (keys.isEmpty() || clipId_.isEmpty()) { setStatus("복사할 키를 먼저 고르세요(클릭, Ctrl/Shift+클릭, 빈 곳 드래그).", true); return false; }
  try {
    clipboard_ = cutout::applyEdit(displayData(), {{"op", "copyKeys"}, {"clip", ss(clipId_)}, {"keys", refsJson(keys)}}).extra.at("clipboard");
  } catch (const std::exception& e) { setStatus(qs(e.what()), true); return false; }
  setStatus(QString("키 %1개를 복사했습니다(%2ms부터). 재생 헤드를 옮기고(다른 클립도 됩니다) Ctrl+V로 붙여넣으세요.")
                .arg(clipboardKeys()).arg(clipboard_.value("fromMs", int64_t(0))));
  return true;
}
bool CutoutPanel::pasteKeys() {
  if (refusePending()) return false;
  if (!clipboardKeys()) { setStatus("먼저 키를 복사하세요(Ctrl+C).", true); return false; }
  if (mode_ != Mode::Animate || clipId_.isEmpty()) { setStatus("키는 애니메이션 모드에서 클립에 붙여넣습니다.", true); return false; }
  Json op = {{"op", "pasteKeys"}, {"clip", ss(clipId_)}, {"timeMs", playheadMs_}, {"keys", clipboard_.at("keys")}};
  std::set<std::string> targets;
  for (const auto& k : clipboard_.at("keys")) targets.insert(k.value("target", std::string()));
  // A copied row (one target) lands on the selected bone/part when that is another target of the same kind.
  QString onto;
  if (targets.size() == 1 && !selection_.isEmpty() && ss(selection_) != *targets.begin() && refKind(selection_) == refKind(qs(*targets.begin()))) {
    op["target"] = ss(selection_);
    onto = selection_;
  }
  if (!runOps({op}, false, [this](const cutout::EditResult& r) { selectKeysFrom(r.extra.value("keys", Json::array())); })) return false;
  setStatus(QString("키 %1개를 「%2」 %3ms에 붙여넣었습니다%4. 되돌리기: Ctrl+Z").arg(clipboardKeys()).arg(clipId_).arg(playheadMs_)
                .arg(onto.isEmpty() ? QString() : " (대상: " + refId(onto) + ")"));
  return true;
}
bool CutoutPanel::duplicateKeys() {
  if (refusePending()) return false;
  const QVector<KeyRef> keys = timeView_->selection();
  if (keys.isEmpty() || clipId_.isEmpty()) { setStatus("복제할 키를 먼저 고르세요.", true); return false; }
  Json board;
  try {
    board = cutout::applyEdit(displayData(), {{"op", "copyKeys"}, {"clip", ss(clipId_)}, {"keys", refsJson(keys)}}).extra.at("clipboard");
  } catch (const std::exception& e) { setStatus(qs(e.what()), true); return false; }
  if (board.value("fromMs", int64_t(-1)) == playheadMs_) { setStatus("재생 헤드를 복제할 시간으로 옮기세요(지금은 고른 키의 시작 시간입니다).", true); return false; }
  if (!runOps({{{"op", "pasteKeys"}, {"clip", ss(clipId_)}, {"timeMs", playheadMs_}, {"keys", board.at("keys")}}}, false,
              [this](const cutout::EditResult& r) { selectKeysFrom(r.extra.value("keys", Json::array())); }))
    return false;
  setStatus(QString("키 %1개를 %2ms에 복제했습니다. 되돌리기: Ctrl+Z").arg(keys.size()).arg(playheadMs_));
  return true;
}
bool CutoutPanel::copyPose() {
  if (!rigDoc()) { setStatus("먼저 캐릭터를 선택하거나 만드세요.", true); return false; }
  // The selected bone with all of its children (a whole limb), or the whole skeleton.
  std::vector<std::string> bones;
  if (const QString root = selectedBone(); !root.isEmpty()) {
    bones.push_back(ss(root));
    for (size_t i = 0; i < bones.size(); i++)
      for (const auto& b : listOf(displayData(), "bones")) {
        const std::string id = b.value("id", std::string());
        if (b.contains("parent") && b.at("parent").is_string() && b.at("parent").get<std::string>() == bones[i] && std::find(bones.begin(), bones.end(), id) == bones.end())
          bones.push_back(id);
      }
  }
  Json op = {{"op", "copyPose"}};
  if (!bones.empty()) op["bones"] = bones;
  const bool animated = mode_ == Mode::Animate && !clipId_.isEmpty();
  if (animated) { op["clip"] = ss(clipId_); op["timeMs"] = playheadMs_; }
  try { poseClipboard_ = cutout::applyEdit(displayData(), op).extra.at("pose"); } catch (const std::exception& e) { setStatus(qs(e.what()), true); return false; }
  setStatus(QString("포즈를 복사했습니다: 본 %1개 (%2). 다른 시간·클립에서 「포즈 붙여넣기」를 누르세요.")
                .arg(poseClipboard_.size()).arg(animated ? QString("%1 %2ms").arg(clipId_).arg(playheadMs_) : QString("기본 자세")));
  return true;
}
bool CutoutPanel::pastePose() {
  if (refusePending()) return false;
  if (!poseClipboard_.is_array() || poseClipboard_.empty()) { setStatus("먼저 「포즈 복사」를 누르세요.", true); return false; }
  if (mode_ != Mode::Animate || clipId_.isEmpty()) { setStatus("포즈는 애니메이션 모드에서 클립에 키로 넣습니다.", true); return false; }
  if (!runOps({{{"op", "pastePose"}, {"clip", ss(clipId_)}, {"timeMs", playheadMs_}, {"pose", poseClipboard_}}}, false,
              [this](const cutout::EditResult& r) { selectKeysFrom(r.extra.value("keys", Json::array())); }))
    return false;
  setStatus(QString("포즈(본 %1개)를 「%2」 %3ms에 키로 넣었습니다. 되돌리기: Ctrl+Z").arg(poseClipboard_.size()).arg(clipId_).arg(playheadMs_));
  return true;
}
void CutoutPanel::stepPlayhead(int steps) { setPlayhead(playheadMs_ + steps * stepField_->value()); }
void CutoutPanel::setLoopRange(int inMs, int outMs) {
  timeView_->setLoop(inMs, outMs);
  setStatus(timeView_->loopActive() ? QString("미리 보기 반복 구간 %1–%2ms (편집기 전용, 저장되지 않음)").arg(timeView_->loopIn()).arg(timeView_->loopOut())
                                    : QString("미리 보기 반복 구간 = 클립 전체"));
}
void CutoutPanel::clearLoopRange() { setLoopRange(0, -1); }

// ---------------------------------------------------------------- mix
void CutoutPanel::commitMixTable() {
  if (filling_ || !rigDoc()) return;
  Json pairs = Json::array();
  for (int r = 0; r < mixTable_->rowCount(); r++) {
    auto* from = qobject_cast<QComboBox*>(mixTable_->cellWidget(r, 0));
    auto* to = qobject_cast<QComboBox*>(mixTable_->cellWidget(r, 1));
    auto* ms = qobject_cast<QSpinBox*>(mixTable_->cellWidget(r, 2));
    if (from && to && ms) pairs.push_back({{"from", ss(from->currentText())}, {"to", ss(to->currentText())}, {"durationMs", ms->value()}});
  }
  const Json data = displayData();
  const Json mix = data.contains("mix") && data.at("mix").is_object() ? data.at("mix") : Json::object();
  if (mix.value("defaultMs", int64_t(0)) == mixDefault_->value() && mix.value("pairs", Json::array()) == pairs) return;
  if (!runOps({{{"op", "setMix"}, {"defaultMs", mixDefault_->value()}, {"pairs", pairs}}}, false)) refreshMix();
}
bool CutoutPanel::addMixPair() {
  const QStringList clips = clipIds();
  const Json data = displayData();
  const Json mix = data.contains("mix") && data.at("mix").is_object() ? data.at("mix") : Json::object();
  Json pairs = mix.value("pairs", Json::array());
  for (const QString& a : clips) for (const QString& b : clips) {
    if (a == b) continue;
    bool exists = false;
    for (const auto& p : pairs) exists |= p.value("from", std::string()) == ss(a) && p.value("to", std::string()) == ss(b);
    if (exists) continue;
    pairs.push_back({{"from", ss(a)}, {"to", ss(b)}, {"durationMs", 150}});
    return runOps({{{"op", "setMix"}, {"pairs", pairs}}}, false);
  }
  setStatus("전환을 추가하려면 서로 다른 클립이 2개 이상 필요합니다.", true);
  return false;
}
bool CutoutPanel::removeMixPair() {
  const int row = mixTable_->currentRow() >= 0 ? mixTable_->currentRow() : mixTable_->rowCount() - 1;
  if (row < 0) return false;
  const Json data = displayData();
  Json pairs = data.contains("mix") && data.at("mix").is_object() ? data.at("mix").value("pairs", Json::array()) : Json::array();
  if (size_t(row) >= pairs.size()) return false;
  pairs.erase(pairs.begin() + row);
  return runOps({{{"op", "setMix"}, {"pairs", pairs}}}, false);
}

// ---------------------------------------------------------------- controller
void CutoutPanel::commitControllerFromUi() {
  if (filling_) return;
  if (!controllerDoc()) return;
  Json c = committedController();
  c["defaultClip"] = ss(defaultClipCombo_->currentText());
  const QString character = characterCombo_->currentData().toString();
  if (character.isEmpty()) c.erase("characterId"); else c["characterId"] = ss(character);
  for (auto [combo, key] : {std::pair{defaultSkinCombo_, "defaultSkin"}, std::pair{defaultExpressionCombo_, "defaultExpression"}}) {
    const QString value = combo->currentData().toString();
    if (value.isEmpty()) c.erase(key); else c[key] = ss(value);
  }
  Json layers = Json::array();
  for (int r = 0; r < layerTable_->rowCount(); r++) {
    auto* track = qobject_cast<QSpinBox*>(layerTable_->cellWidget(r, 0));
    auto* clip = qobject_cast<QComboBox*>(layerTable_->cellWidget(r, 1));
    if (track && clip) layers.push_back({{"track", track->value()}, {"clip", ss(clip->currentText())}});
  }
  c["layers"] = layers;
  Json bindings = Json::array();
  for (int r = 0; r < bindingTable_->rowCount(); r++) {
    auto cell = [&](int col) { return bindingTable_->cellWidget(r, col); };
    auto* id = qobject_cast<QLineEdit*>(cell(0));
    auto* signal = qobject_cast<QComboBox*>(cell(1));
    auto* match = qobject_cast<QLineEdit*>(cell(2));
    auto* clip = qobject_cast<QComboBox*>(cell(3));
    auto* track = qobject_cast<QSpinBox*>(cell(4));
    auto* loop = qobject_cast<QComboBox*>(cell(5));
    auto* then = qobject_cast<QComboBox*>(cell(6));
    auto* stop = qobject_cast<QLineEdit*>(cell(7));
    if (!id || !signal || !match || !clip || !track || !loop || !then || !stop) continue;
    Json b = {{"id", ss(id->text().trimmed())}, {"signal", ss(signal->currentData().toString())}};
    if (!clip->currentData().toString().isEmpty()) b["clip"] = ss(clip->currentData().toString());
    // Skin/expression columns: "" = no skin change; expression "-" = no change, "" = clear (null).
    if (auto* skin = qobject_cast<QComboBox*>(cell(8)); skin && !skin->currentData().toString().isEmpty()) b["skin"] = ss(skin->currentData().toString());
    if (auto* expression = qobject_cast<QComboBox*>(cell(9)); expression && expression->currentData().toString() != "-")
      b["expression"] = expression->currentData().toString().isEmpty() ? Json(nullptr) : Json(ss(expression->currentData().toString()));
    const auto pairs = parsePairs(match->text());
    if (!pairs.isEmpty()) { Json m = Json::object(); for (auto it = pairs.begin(); it != pairs.end(); ++it) m[ss(it.key())] = ss(it.value()); b["match"] = m; }
    if (track->value() != 0) b["track"] = track->value();
    if (!loop->currentData().toString().isEmpty()) b["loop"] = loop->currentData().toString() == "true";
    if (then->currentData().toString() != "default") b["then"] = ss(then->currentData().toString());
    Json stops = Json::array();
    for (const QString& s : stop->text().split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts)) stops.push_back(ss(s));
    if (!stops.empty()) b["stopOn"] = stops;
    // Keep fields this table does not edit (speed).
    const Json& old = listOf(committedController(), "bindings");
    if (size_t(r) < old.size() && old[size_t(r)].contains("speed")) b["speed"] = old[size_t(r)].at("speed");
    bindings.push_back(b);
  }
  c["bindings"] = bindings;
  commitController(c);
}
bool CutoutPanel::addLayer() {
  if (!controllerDoc()) { setStatus("이 캐릭터에 게임 연결 문서가 없습니다.", true); return false; }
  Json c = committedController();
  std::set<int> used;
  for (const auto& l : listOf(c, "layers")) used.insert(l.value("track", 0));
  int track = 1;
  while (used.count(track) && track < 15) track++;
  if (used.count(track)) { setStatus("빈 트랙이 없습니다.", true); return false; }
  const QStringList clips = clipIds();
  c["layers"].push_back({{"track", track}, {"clip", ss(clips.contains(clipId_) ? clipId_ : clips.value(0))}});
  return commitController(c);
}
bool CutoutPanel::removeLayer() {
  Json c = committedController();
  const int row = layerTable_->currentRow() >= 0 ? layerTable_->currentRow() : layerTable_->rowCount() - 1;
  if (!controllerDoc() || row < 0 || size_t(row) >= listOf(c, "layers").size()) return false;
  c["layers"].erase(c["layers"].begin() + row);
  return commitController(c);
}
bool CutoutPanel::addBinding() {
  if (!controllerDoc()) { setStatus("이 캐릭터에 게임 연결 문서가 없습니다.", true); return false; }
  Json c = committedController();
  if (!c.contains("bindings") || !c["bindings"].is_array()) c["bindings"] = Json::array();
  std::set<std::string> ids;
  for (const auto& b : c["bindings"]) ids.insert(b.value("id", std::string()));
  std::string id = "binding";
  for (int n = 2; ids.count(id); n++) id = "binding-" + std::to_string(n);
  // Sensible default: a successful action plays the selected (non-idle) clip once, then idle.
  const QStringList clips = clipIds();
  QString clip = clipId_;
  if (clip.isEmpty() || qs(c.value("defaultClip", std::string())) == clip) for (const QString& x : clips) if (qs(c.value("defaultClip", std::string())) != x) { clip = x; break; }
  if (clip.isEmpty()) clip = clips.value(0);
  c["bindings"].push_back({{"id", id}, {"signal", "actionResult"}, {"match", {{"outcome", "success"}}}, {"clip", ss(clip)}, {"then", "default"}});
  return commitController(c);
}
bool CutoutPanel::removeBinding() {
  Json c = committedController();
  const int row = bindingTable_->currentRow() >= 0 ? bindingTable_->currentRow() : bindingTable_->rowCount() - 1;
  if (!controllerDoc() || row < 0 || size_t(row) >= listOf(c, "bindings").size()) return false;
  c["bindings"].erase(c["bindings"].begin() + row);
  return commitController(c);
}
void CutoutPanel::ensurePreview() {
  const Doc* rig = rigDoc();
  const Doc* controller = controllerDoc();
  const QString key = QString("%1/%2/%3").arg(lastRevision_).arg(rigId_, controllerId_);
  if (key == previewKey_ && preview_) return;
  previewKey_ = key;
  preview_.reset();
  if (!rig || !controller) return;
  try {
    // Real loop flags here: the preview behaves exactly like the game runtime.
    auto parsed = std::make_shared<const cutout::Rig>(cutout::parseRig(ss(rigId_), rig->data));
    preview_ = std::make_unique<cutout::Instance>(parsed, cutout::parseController(ss(controllerId_), controller->data));
  } catch (const std::exception& e) {
    previewResult_->setText("미리 보기를 만들 수 없습니다: " + qs(e.what()));
  }
}
QStringList CutoutPanel::previewSignal(const QString& name, const QMap<QString, QString>& params) {
  showControllerTab();
  ensurePreview();
  if (!preview_) { setStatus("게임 연결 미리 보기를 만들 수 없습니다(리그·연결 검증 오류 확인).", true); return {}; }
  std::map<std::string, std::string> p;
  for (auto it = params.begin(); it != params.end(); ++it) p[ss(it.key())] = ss(it.value());
  QStringList matched;
  for (const auto& id : preview_->signal(ss(name), p)) matched << qs(id);
  preview_->takeNotifications();
  previewResult_->setText(matched.isEmpty() ? QString("「%1」 신호에 맞는 연결이 없습니다.").arg(name) : QString("「%1」 → 실행된 연결: %2").arg(name, matched.join(", ")));
  lastTick_ = QDateTime::currentMSecsSinceEpoch();
  timer_->start();
  updateCanvas();
  return matched;
}

// ---------------------------------------------------------------- skins / expressions
void CutoutPanel::refreshLooks() {
  if (!skinList_) return;
  filling_ = true;
  const Json data = displayData();
  {
    QSignalBlocker block(skinList_);
    skinList_->clear();
    (new QListWidgetItem("(기본 첨부 · 스킨 없음)", skinList_))->setData(Qt::UserRole, QString());
    for (const auto& s : listOf(data, "skins")) {
      const QString id = qs(s.value("id", std::string())), name = qs(s.value("name", std::string()));
      auto* item = new QListWidgetItem(QString("%1%2 · 그림 %3").arg(id, name.isEmpty() || name == id ? QString() : " (" + name + ")").arg(listOf(s, "attachments").size()), skinList_);
      item->setData(Qt::UserRole, id);
    }
    for (int i = 0; i < skinList_->count(); i++) if (skinList_->item(i)->data(Qt::UserRole).toString() == previewSkin_) skinList_->setCurrentRow(i);
  }
  {
    QSignalBlocker block(skinAttachments_);
    skinAttachments_->clear();
    if (const Json* skin = findIn(data, "skins", ss(previewSkin_)))
      for (const auto& a : listOf(*skin, "attachments")) {
        const QString slot = qs(a.value("slot", std::string())), name = qs(a.value("name", std::string())), asset = qs(a.value("imageAssetId", std::string()));
        auto* item = new QListWidgetItem(QIcon(images_.path(asset)), QString("%1 · %2 ← %3").arg(slot, name, asset), skinAttachments_);
        item->setData(Qt::UserRole, slot);
        item->setData(Qt::UserRole + 1, name);
        if ("slot:" + slot == selection_) skinAttachments_->setCurrentItem(item);
      }
    skinHint_->setText(previewSkin_.isEmpty() ? "스킨을 고르면 캔버스의 조각(▣)에 PNG를 끌어 놓아 그 스킨의 그림을 넣습니다. 클립은 그림 이름만 키로 쓰므로 의상을 바꿔도 새 애니메이션이 필요 없습니다."
                                              : QString("「%1」 스킨의 그림 · 없는 조각은 기본 첨부가 보입니다. 캔버스 조각에 PNG를 끌어 놓거나 「그림 지정…」").arg(previewSkin_));
  }
  {
    QSignalBlocker block(expressionList_);
    expressionList_->clear();
    (new QListWidgetItem("(표정 없음)", expressionList_))->setData(Qt::UserRole, QString());
    for (const auto& e : listOf(data, "expressions")) {
      const QString id = qs(e.value("id", std::string())), name = qs(e.value("name", std::string()));
      const size_t slots = e.contains("slots") && e.at("slots").is_object() ? e.at("slots").size() : 0, bones = e.contains("bones") && e.at("bones").is_object() ? e.at("bones").size() : 0;
      auto* item = new QListWidgetItem(QString("%1%2 · 그림 %3 · 본 %4").arg(id, name.isEmpty() || name == id ? QString() : " (" + name + ")").arg(slots).arg(bones), expressionList_);
      item->setData(Qt::UserRole, id);
    }
    for (int i = 0; i < expressionList_->count(); i++) if (expressionList_->item(i)->data(Qt::UserRole).toString() == previewExpression_) expressionList_->setCurrentRow(i);
  }
  expressionTable_->setRowCount(0);
  if (const Json* e = findIn(data, "expressions", ss(previewExpression_))) {
    const Json slots = e->contains("slots") && e->at("slots").is_object() ? e->at("slots") : Json::object();
    int row = 0;
    auto label = [&](const QString& text) {
      auto* item = new QTableWidgetItem(text);
      item->setFlags(Qt::ItemIsEnabled);
      expressionTable_->setItem(row, 0, item);
    };
    for (const auto& s : listOf(data, "slots")) {
      const std::string sid = s.value("id", std::string());
      expressionTable_->insertRow(row);
      label("▣ " + qs(sid));
      auto* combo = new QComboBox;
      combo->setObjectName("cutoutExpressionSlot" + qs(sid));
      combo->addItem("— (표정에 넣지 않음)", "-");
      combo->addItem("(숨김)", QString());
      for (const QString& n : slotNames(data, sid)) combo->addItem(n, n);
      combo->setCurrentIndex(!slots.contains(sid) ? 0 : slots.at(sid).is_string() ? std::max(0, combo->findData(qs(slots.at(sid).get<std::string>()))) : 1);
      connect(combo, &QComboBox::activated, this, [this, sid, combo] { setExpressionSlot(qs(sid), combo->currentData().toString()); });
      expressionTable_->setCellWidget(row++, 1, combo);
    }
    if (e->contains("bones") && e->at("bones").is_object())
      for (auto it = e->at("bones").begin(); it != e->at("bones").end(); ++it) {
        const QString bone = qs(it.key());
        QStringList parts;
        for (const char* k : {"x", "y", "rotation", "scaleX", "scaleY"})
          if (it.value().contains(k) && it.value().at(k).is_number()) parts << QString("%1=%2").arg(k).arg(it.value().at(k).get<double>());
        expressionTable_->insertRow(row);
        label("◉ " + bone);
        auto* edit = new QLineEdit(parts.join(", "));
        edit->setObjectName("cutoutExpressionBone" + bone);
        edit->setPlaceholderText("rotation=10, y=-3 (비우면 뺌)");
        connect(edit, &QLineEdit::editingFinished, this, [this, bone, edit] {
          QMap<QString, double> values;
          const auto pairs = parsePairs(edit->text());
          for (auto p = pairs.begin(); p != pairs.end(); ++p) { bool ok = false; const double v = p.value().toDouble(&ok); if (ok) values.insert(p.key(), v); }
          setExpressionBone(bone, values);
        });
        expressionTable_->setCellWidget(row++, 1, edit);
      }
    expressionTable_->resizeColumnToContents(0);
  }
  filling_ = false;
}

void CutoutPanel::setPreviewSkin(const QString& id) {
  if (!id.isEmpty() && !findIn(displayData(), "skins", ss(id))) { setStatus("없는 스킨입니다: " + id, true); return; }
  if (id == previewSkin_) return;
  previewSkin_ = id;
  rebuildDisplay();  // new display generation: canvas, ghosts and properties follow the look
  setStatus(id.isEmpty() ? QString("미리 보기: 기본 첨부(스킨 없음)") : QString("미리 보기 스킨 「%1」 · 편집기 전용이라 문서·게임에는 저장되지 않습니다.").arg(id));
}
void CutoutPanel::setPreviewExpression(const QString& id) {
  if (!id.isEmpty() && !findIn(displayData(), "expressions", ss(id))) { setStatus("없는 표정입니다: " + id, true); return; }
  if (id == previewExpression_) return;
  previewExpression_ = id;
  rebuildDisplay();
  setStatus(id.isEmpty() ? QString("미리 보기: 표정 없음")
                         : QString("미리 보기 표정 「%1」 · 표정이 덮는 그림·본 채널은 키를 바꿔도 캔버스에서 그대로 보입니다(편집기 전용).").arg(id));
}
bool CutoutPanel::newSkin() {
  if (!rigDoc()) { setStatus("먼저 캐릭터를 만드세요.", true); return false; }
  const QString id = askText("새 스킨", "스킨 ID (영문 소문자, 예: winter, uniform)", uniqueId(displayData(), "skins", "skin"));
  if (id.isEmpty()) return false;
  const QString before = previewSkin_;
  previewSkin_ = id;  // the commit rebuild previews the new skin
  if (runOps({{{"op", "addSkin"}, {"id", ss(id)}}}, false)) {
    setStatus(QString("스킨 「%1」를 만들었습니다. 캔버스의 조각에 PNG를 끌어 놓거나 「그림 지정…」으로 그림을 넣으세요.").arg(id));
    return true;
  }
  previewSkin_ = before;
  return false;
}
bool CutoutPanel::duplicateSkin() {
  if (previewSkin_.isEmpty()) { setStatus("복제할 스킨을 고르세요.", true); return false; }
  const QString id = askText("스킨 복제", "새 스킨 ID", uniqueId(displayData(), "skins", previewSkin_ + "-copy"));
  if (id.isEmpty()) return false;
  const QString from = previewSkin_;
  previewSkin_ = id;
  if (runOps({{{"op", "duplicateSkin"}, {"skin", ss(from)}, {"id", ss(id)}}}, false)) return true;
  previewSkin_ = from;
  return false;
}
bool CutoutPanel::renameSkin() {
  if (previewSkin_.isEmpty()) { setStatus("이름을 바꿀 스킨을 고르세요.", true); return false; }
  const QString id = askText("스킨 이름 변경", "새 스킨 ID (게임 연결의 기본 스킨·연결 규칙도 함께 바뀝니다)", previewSkin_);
  if (id.isEmpty() || id == previewSkin_) return false;
  const QString from = previewSkin_;
  Json op = {{"op", "renameSkin"}, {"skin", ss(from)}, {"id", ss(id)}};
  if (const Json* skin = findIn(displayData(), "skins", ss(from)); skin && qs(skin->value("name", std::string())) == from) op["name"] = ss(id);
  previewSkin_ = id;
  if (runOps({op}, false)) return true;
  previewSkin_ = from;
  return false;
}
bool CutoutPanel::deleteSkin() {
  if (previewSkin_.isEmpty()) { setStatus("삭제할 스킨을 고르세요.", true); return false; }
  if (!askConfirm("스킨 삭제", QString("스킨 「%1」와 그 그림 지정을 삭제할까요? 게임 연결에서 이 스킨을 쓰는 곳도 함께 지웁니다. 되돌리기(Ctrl+Z)로 복구할 수 있습니다.").arg(previewSkin_)))
    return false;
  return runOps({{{"op", "deleteSkin"}, {"skin", ss(previewSkin_)}, {"force", true}}}, false);
}
bool CutoutPanel::assignSkinImage(const QString& slot, const QString& assetId, const QString& name) {
  if (previewSkin_.isEmpty()) { setStatus("먼저 스킨을 고르거나 만드세요(「새 스킨」). 기본 그림은 「이미지 조각 추가…」로 바꿉니다.", true); return false; }
  if (slot.isEmpty() || !rig_ || rig_->slotIndex(ss(slot)) < 0) { setStatus("그림을 넣을 조각(▣)을 고르세요.", true); return false; }
  const QSize size = images_.pixelSize(assetId);
  if (!size.isValid() || size.isEmpty()) { setStatus("이미지 크기를 읽을 수 없습니다: " + assetId, true); return false; }
  Json op = {{"op", "setSkinAttachment"}, {"skin", ss(previewSkin_)}, {"slot", ss(slot)}, {"imageAssetId", ss(assetId)}, {"width", size.width()}, {"height", size.height()}};
  // The name shown right now (setup, or the keyed one at the playhead, e.g. closed eyes) gets the outfit image.
  const cutout::Pose pose = currentPose(false);
  const int si = rig_->slotIndex(ss(slot));
  if (!name.isEmpty()) op["name"] = ss(name);
  else if (size_t(si) < pose.attachments.size() && pose.attachments[size_t(si)]) op["name"] = *pose.attachments[size_t(si)];
  const QString before = selection_;
  selection_ = "slot:" + slot;
  if (runOps({op}, false)) {
    setStatus(QString("스킨 「%1」의 조각 「%2」에 그림 %3을 넣었습니다. 되돌리기: Ctrl+Z").arg(previewSkin_, slot, assetId));
    return true;
  }
  selection_ = before;
  return false;
}
void CutoutPanel::importAndAssignSkinImage(const QString& file, const QString& slot) {
  if (busy()) return;
  QPointer<CutoutPanel> self(this);
  inSubmit_ = true;
  host_->request("importImage", {{"file", file}}, [self, slot](QJsonObject reply) {
    if (!self) return;
    const QString id = reply.value("importedAssetId").toString();
    if (!reply.value("error").toString().isEmpty() || id.isEmpty()) {
      self->setStatus("PNG를 가져오지 못했습니다: " + reply.value("error").toString(), true);
      return;
    }
    // Next turn: the host is idle again and assets() includes the image (same as importAndAddPart).
    QTimer::singleShot(0, self, [self, id, slot] {
      if (!self || !self->host_) return;
      self->images_.setAssets(self->host_->assets());
      self->assignSkinImage(slot, id);
    });
  });
  inSubmit_ = false;
}
bool CutoutPanel::assignSkinImageInteractive() {
  if (previewSkin_.isEmpty()) { setStatus("먼저 스킨을 고르거나 만드세요(「새 스킨」).", true); return false; }
  PartChoice choice;
  choice.slot = refKind(selection_) == "slot" ? refId(selection_) : QString();
  choice.bone = selectedBone();
  if (!askPart || !askPart(choice)) return false;
  if (choice.slot.isEmpty()) { setStatus("그림을 넣을 조각 ID를 고르세요.", true); return false; }
  if (!choice.importFile.isEmpty()) { importAndAssignSkinImage(choice.importFile, choice.slot); return true; }
  return assignSkinImage(choice.slot, choice.assetId, choice.name);
}
bool CutoutPanel::removeSkinAttachment() {
  QListWidgetItem* item = skinAttachments_ ? skinAttachments_->currentItem() : nullptr;
  if (previewSkin_.isEmpty() || !item) { setStatus("뺄 스킨 그림을 목록에서 고르세요.", true); return false; }
  return runOps({{{"op", "removeSkinAttachment"}, {"skin", ss(previewSkin_)}, {"slot", ss(item->data(Qt::UserRole).toString())}, {"name", ss(item->data(Qt::UserRole + 1).toString())}}}, false);
}
bool CutoutPanel::dropImages(const QMimeData* mime, const QString& slotRef) {
  if (!mime) return false;
  const QString slot = refKind(slotRef) == "slot" ? refId(slotRef) : refKind(selection_) == "slot" ? refId(selection_) : QString();
  if (previewSkin_.isEmpty()) { setStatus("스킨을 먼저 고르세요(「스킨 · 표정」). 기본 그림은 「이미지 조각 추가…」로 넣습니다.", true); return false; }
  if (slot.isEmpty()) { setStatus("그림을 놓을 조각 위에 놓거나 조각을 먼저 고르세요.", true); return false; }
  for (const QUrl& url : mime->urls())
    if (url.isLocalFile() && url.toLocalFile().endsWith(".png", Qt::CaseInsensitive)) { importAndAssignSkinImage(url.toLocalFile(), slot); return true; }
  const QString text = mime->text().trimmed();
  const QString asset = text.startsWith("asset:") ? text.mid(6) : text;
  if (!asset.isEmpty() && images_.imageAssetIds().contains(asset)) return assignSkinImage(slot, asset);
  setStatus("PNG 파일이나 프로젝트 이미지만 놓을 수 있습니다.", true);
  return false;
}
bool CutoutPanel::captureExpressionAs(const QString& idIn, bool replace) {
  if (!rigDoc() || !rig_) { setStatus("먼저 캐릭터를 선택하거나 만드세요.", true); return false; }
  QString id = idIn;
  if (id.isEmpty()) id = askText("현재 상태로 표정 만들기", "표정 ID (영문 소문자, 예: smile, angry, sad)", uniqueId(committedRig(), "expressions", "expression"));
  if (id.isEmpty()) return false;
  Json op = {{"op", "captureExpression"}, {"id", ss(id)}};
  if (replace) op["replace"] = true;
  if (mode_ == Mode::Animate && !clipId_.isEmpty()) {
    op["clip"] = ss(clipId_);
    op["timeMs"] = playheadMs_;
    if (working_) op["from"] = *working_;  // only the unsaved changes (not the clip's own motion) become the expression
  }
  cutout::EditResult result;
  try {
    result = cutout::applyEdit(committedRig(), op, controllersFor(rigId_));
  } catch (const std::exception& e) { setStatus(qs(e.what()), true); return false; }
  const QString before = previewExpression_;
  previewExpression_ = id;
  // One edit; the pending change was turned into the expression, so it is not kept as keys.
  if (!commitRig(result)) { previewExpression_ = before; return false; }
  const Json& e = result.extra.at("expression");
  setStatus(QString("표정 「%1」을 %2했습니다(그림 %3 · 본 %4). 게임 연결에서 신호(예: portraitChanged)에 연결하거나 「표정 키」로 클립에 넣으세요.")
                .arg(id, replace ? "다시 담기" : "만들기").arg(e.contains("slots") ? e.at("slots").size() : 0).arg(e.contains("bones") ? e.at("bones").size() : 0));
  return true;
}
bool CutoutPanel::captureExpression() { return captureExpressionAs(QString(), false); }
bool CutoutPanel::recaptureExpression() {
  if (previewExpression_.isEmpty()) { setStatus("다시 담을 표정을 고르세요.", true); return false; }
  return captureExpressionAs(previewExpression_, true);
}
bool CutoutPanel::renameExpression() {
  if (previewExpression_.isEmpty()) { setStatus("이름을 바꿀 표정을 고르세요.", true); return false; }
  const QString id = askText("표정 이름 변경", "새 표정 ID (표정 키와 게임 연결도 함께 바뀝니다)", previewExpression_);
  if (id.isEmpty() || id == previewExpression_) return false;
  const QString from = previewExpression_;
  Json op = {{"op", "renameExpression"}, {"expression", ss(from)}, {"id", ss(id)}};
  if (const Json* e = findIn(displayData(), "expressions", ss(from)); e && qs(e->value("name", std::string())) == from) op["name"] = ss(id);
  previewExpression_ = id;
  if (runOps({op}, false)) return true;
  previewExpression_ = from;
  return false;
}
bool CutoutPanel::deleteExpression() {
  if (previewExpression_.isEmpty()) { setStatus("삭제할 표정을 고르세요.", true); return false; }
  if (!askConfirm("표정 삭제", QString("표정 「%1」과 그 표정 키를 삭제할까요? 게임 연결에서 쓰는 곳도 함께 지웁니다. 되돌리기(Ctrl+Z)로 복구할 수 있습니다.").arg(previewExpression_)))
    return false;
  return runOps({{{"op", "deleteExpression"}, {"expression", ss(previewExpression_)}, {"force", true}}}, false);
}
bool CutoutPanel::setExpressionSlot(const QString& slot, const QString& value) {
  if (filling_ || previewExpression_.isEmpty()) return false;
  Json op = {{"op", "setExpressionSlot"}, {"expression", ss(previewExpression_)}, {"slot", ss(slot)}};
  if (value == "-") op["remove"] = true;
  else op["attachment"] = value.isEmpty() ? Json(nullptr) : Json(ss(value));
  if (!runOps({op}, false)) { refreshLooks(); return false; }
  return true;
}
bool CutoutPanel::setExpressionBone(const QString& bone, const QMap<QString, double>& values) {
  if (filling_ || previewExpression_.isEmpty()) return false;
  Json op = {{"op", "setExpressionBone"}, {"expression", ss(previewExpression_)}, {"bone", ss(bone)}};
  if (values.isEmpty()) op["remove"] = true;
  else for (const char* k : {"x", "y", "rotation", "scaleX", "scaleY"}) op[k] = values.contains(k) ? Json(values.value(k)) : Json(nullptr);
  // Unchanged text (focus-out) must not issue an edit: the core reports "no change" and refreshes.
  if (!runOps({op}, false)) { refreshLooks(); return false; }
  return true;
}
bool CutoutPanel::keyExpression() {
  if (mode_ != Mode::Animate) { setStatus("표정 키는 애니메이션 모드에서 넣습니다.", true); return false; }
  if (clipId_.isEmpty()) { setStatus("키를 넣을 클립을 먼저 만드세요.", true); return false; }
  const Json value = previewExpression_.isEmpty() ? Json(nullptr) : Json(ss(previewExpression_));
  if (!runOps({{{"op", "setKey"}, {"clip", ss(clipId_)}, {"target", "rig"}, {"property", "expression"}, {"timeMs", playheadMs_}, {"value", value}}}, false)) return false;
  setStatus(QString("「%1」 %2ms에 표정 키(%3)를 넣었습니다. 다음 키까지 이 표정이 유지됩니다(계단). 되돌리기: Ctrl+Z")
                .arg(clipId_).arg(playheadMs_).arg(previewExpression_.isEmpty() ? QString("표정 없음") : previewExpression_));
  return true;
}

// ---------------------------------------------------------------- playback
void CutoutPanel::togglePlay(bool on) {
  playing_ = on && mode_ == Mode::Animate;
  playButton_->setText(playing_ ? "⏸ 일시정지" : "▶ 재생");
  if (playing_ && working_) { working_.reset(); setStatus("재생하려고 키로 저장하지 않은 변경을 버렸습니다."); rebuildDisplay(); }
  // Starting outside the preview loop range jumps to its start.
  if (playing_ && timeView_->loopActive() && (playheadExact_ < timeView_->loopIn() || playheadExact_ >= timeView_->loopOut())) {
    playheadMs_ = timeView_->loopIn();
    playheadExact_ = playheadMs_;
    timeView_->setPlayhead(playheadMs_);
  }
  lastTick_ = QDateTime::currentMSecsSinceEpoch();
  if (playing_ || bottomTabs_->currentIndex() == 1) timer_->start(); else timer_->stop();
  if (!playing_) { refreshProperties(); refreshModeUi(); }
}
void CutoutPanel::tick() {
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  const double dt = std::clamp(double(now - lastTick_), 0.0, 250.0);
  lastTick_ = now;
  if (playing_ && rig_) {
    const int ci = rig_->clipIndex(ss(clipId_));
    if (ci >= 0) {
      const double d = double(rig_->clips[size_t(ci)].durationMs);
      // The editor-only loop range narrows playback to [in, out] (whole clip by default).
      const double lo = timeView_->loopActive() ? timeView_->loopIn() : 0, hi = timeView_->loopActive() ? std::min<double>(timeView_->loopOut(), d) : d;
      playheadExact_ += dt * speedCombo_->currentData().toDouble();
      if (playheadExact_ > hi) {
        if (loopPreview_->isChecked() && hi > lo) playheadExact_ = lo + std::fmod(playheadExact_ - lo, hi - lo);
        else { playheadExact_ = hi; playButton_->setChecked(false); }
      }
      playheadMs_ = int(playheadExact_);
      timeView_->setPlayhead(playheadMs_);
      timeView_->reveal(playheadMs_);
      updateTimeUi(int(d));
    }
  }
  if (bottomTabs_->currentIndex() == 1 && preview_) {
    preview_->update(dt);
    preview_->takeNotifications();  // presentation only; nothing reaches the game
  }
  updateCanvas();
}
}  // namespace cutout_studio
