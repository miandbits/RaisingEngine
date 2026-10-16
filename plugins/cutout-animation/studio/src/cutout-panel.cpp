#include "cutout-panel.hpp"
#include <QtWidgets>
#include <climits>
#include <cmath>
#include <set>
#include <tuple>
#include "cutout/host.hpp"
#include "cutout/mesh.hpp"
#include "cutout/templates.hpp"
#include "dopesheet.hpp"
#include "graph-editor.hpp"

namespace cutout_studio {
using cutout::Json;
namespace {
const char* PROPS[] = {"x", "y", "rotation", "scaleX", "scaleY", "pivotX", "pivotY", "alpha", "length", "shearX", "shearY"};
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
const char* NUMERIC_PROPS[] = {"x", "y", "rotation", "scaleX", "scaleY", "shearX", "shearY", "alpha", "mix",
                               "mixRotate", "mixX", "mixY", "mixScaleX", "mixScaleY", "mixShearY"};  // shear / transform mixes: plugin 1.16.0
QString propertyLabel(const QString& property) {
  static const QMap<QString, QString> names = {{"x", "X"}, {"y", "Y"}, {"rotation", "회전"}, {"scaleX", "크기 X"}, {"scaleY", "크기 Y"}, {"alpha", "투명도"}, {"attachment", "그림"},
                                               {"mix", "혼합"}, {"bendPositive", "굽힘"}, {"sequence", "시퀀스"}, {"shearX", "기울이기 X"}, {"shearY", "기울이기 Y"},
                                               {"mixRotate", "회전 혼합"}, {"mixX", "X 혼합"}, {"mixY", "Y 혼합"}, {"mixScaleX", "크기 X 혼합"}, {"mixScaleY", "크기 Y 혼합"},
                                               {"mixShearY", "기울이기 혼합"}};
  return names.value(property, property);
}
std::string interpOf(const Json& key) { return key.contains("interp") && key.at("interp").is_string() ? key.at("interp").get<std::string>() : "linear"; }
/** Dopesheet rows and key refs name a deform track "deform:<attachment>" (one row per mesh); core
 *  ops take property "deform" plus `attachment`. These two convert between the forms. */
void putProperty(Json& op, const QString& property) {
  if (property.startsWith("deform:")) { op["property"] = "deform"; op["attachment"] = ss(property.mid(7)); }
  else op["property"] = ss(property);
}
QString uiProperty(const Json& ref) {
  const std::string p = ref.value("property", std::string());
  return p == "deform" ? "deform:" + qs(ref.value("attachment", std::string())) : qs(p);
}
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
// Picking an image fills 조각 ID only while it is empty or still the previous automatic value:
// a prefilled (selected part) or typed id is never overwritten; 첨부 이름 is never touched.
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
    // The list takes the focus first and makes its first row current by itself: that must not
    // count as a choice of the slot either.
    autoSlot_ = choice.slot.trimmed().isEmpty();
    connect(assets_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
      if (!item || !(autoSlot_ || slot_->currentText().trimmed().isEmpty())) return;
      filling_ = true;
      slot_->setCurrentText(qs(cutout::slugId(ss(item->data(Qt::UserRole).toString()), "part")));
      filling_ = false;
      autoSlot_ = true;
    });
    connect(slot_, &QComboBox::editTextChanged, this, [this](const QString& text) { if (!filling_) autoSlot_ = text.trimmed().isEmpty(); });
    connect(import, &QPushButton::clicked, this, [this] {
      const QString file = QFileDialog::getOpenFileName(this, "PNG 가져오기", QString(), "PNG 이미지 (*.png)");
      if (file.isEmpty()) return;
      choice_.importFile = file;
      if (autoSlot_ || slot_->currentText().trimmed().isEmpty()) slot_->setCurrentText(qs(cutout::slugId(ss(QFileInfo(file).completeBaseName()), "part")));
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
  bool autoSlot_ = true, filling_ = false;
};

// 그림 지정… of a skin: which existing part (slot, never free text) and which of its attachment
// names get which image. The image list only chooses the image: slot and name stay as set.
class SkinImageDialog : public QDialog {
 public:
  using Preview = std::function<QImage(const QString& slot, const QString& name, const QString& assetId, const QSize& size)>;
  SkinImageDialog(QWidget* parent, ImageCache& images, const QVector<CutoutPanel::SkinSlotInfo>& slots, CutoutPanel::SkinImageChoice& choice, Preview preview)
      : QDialog(parent), choice_(choice), slots_(slots), preview_(std::move(preview)) {
    setObjectName("cutoutSkinImageDialog");
    setWindowTitle("스킨 그림 지정 — " + choice.skin);
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    slot_ = new QComboBox;
    slot_->setObjectName("cutoutSkinImageSlot");
    slot_->setToolTip("그림을 바꿀 조각 · 트리나 캔버스에서 고른 조각(본을 골랐으면 그 본의 첫 조각)으로 시작합니다.");
    for (const auto& s : slots_) slot_->addItem(QString("▣ %1 · %2").arg(s.slot, s.bone), s.slot);
    name_ = new QComboBox;
    name_->setObjectName("cutoutSkinImageName");
    name_->setEditable(true);
    name_->setInsertPolicy(QComboBox::NoInsert);
    name_->setToolTip("이 조각의 첨부 이름 · 지금 보이는 이름으로 시작합니다.\n새 이름을 입력하면 이 스킨에만 있는 새 그림(예: 겨울 옷의 다른 입 모양)이 됩니다.");
    name_->lineEdit()->setPlaceholderText("새 이름 입력 가능");
    form->addRow("조각", slot_);
    form->addRow("첨부 이름", name_);
    assets_ = new QListWidget;
    assets_->setObjectName("cutoutSkinImageAssets");
    assets_->setIconSize(QSize(40, 40));
    for (const QString& id : images.imageAssetIds()) {
      auto* item = new QListWidgetItem(QIcon(images.path(id)), id, assets_);
      item->setData(Qt::UserRole, id);
      const QSize size = images.pixelSize(id);
      item->setToolTip(images.path(id) + (size.isValid() ? QString("\n%1×%2").arg(size.width()).arg(size.height()) : QString()));
    }
    auto* import = new QPushButton("PNG 가져오기…");
    import->setObjectName("cutoutSkinImageImport");
    import->setToolTip("PNG를 프로젝트 이미지로 가져와 고른 조각·이름에 바로 지정합니다.");
    previewLabel_ = new QLabel;
    previewLabel_->setObjectName("cutoutSkinImagePreview");
    previewLabel_->setFixedSize(160, 200);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setWordWrap(true);
    previewLabel_->setStyleSheet("background:#1e2329;border:1px solid #46515f;color:#a7b4c2;");
    previewLabel_->setToolTip("결과 미리 보기: 이 스킨을 입고 고른 조각이 이 이름을 보여 줄 때");
    auto* body = new QHBoxLayout;
    auto* left = new QVBoxLayout;
    left->addWidget(new QLabel("프로젝트 이미지"));
    left->addWidget(assets_, 1);
    left->addWidget(import);
    body->addLayout(left, 1);
    body->addWidget(previewLabel_, 0, Qt::AlignTop);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setObjectName("cutoutSkinImageOk");
    layout->addLayout(form);
    layout->addLayout(body, 1);
    layout->addWidget(buttons);
    slot_->setCurrentIndex(std::max(0, slot_->findData(choice.slot)));
    fillNames(choice.name);
    connect(slot_, &QComboBox::currentIndexChanged, this, [this] { fillNames(QString()); });
    connect(name_, &QComboBox::currentTextChanged, this, [this] { updatePreview(); });
    connect(assets_, &QListWidget::currentItemChanged, this, [this] { updatePreview(); });
    connect(import, &QPushButton::clicked, this, [this] {
      const QString file = QFileDialog::getOpenFileName(this, "PNG 가져오기", QString(), "PNG 이미지 (*.png)");
      if (file.isEmpty()) return;
      choice_.importFile = file;
      finish();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
      if (!assets_->currentItem()) { QMessageBox::information(this, windowTitle(), "이미지를 고르거나 PNG를 가져오세요."); return; }
      if (name_->currentText().trimmed().isEmpty()) { QMessageBox::information(this, windowTitle(), "첨부 이름을 고르거나 입력하세요."); return; }
      choice_.assetId = assets_->currentItem()->data(Qt::UserRole).toString();
      finish();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    resize(520, 460);
  }

 private:
  /** Names of the chosen slot; `prefer` (else the name the slot shows now) is selected. */
  void fillNames(const QString& prefer) {
    const CutoutPanel::SkinSlotInfo* info = currentSlot();
    {
      QSignalBlocker block(name_);
      name_->clear();
      if (info) name_->addItems(info->names);
      const QString pick = !prefer.isEmpty() ? prefer : info ? info->shown : QString();
      const int i = name_->findText(pick);
      if (i >= 0) name_->setCurrentIndex(i);
      else name_->setEditText(pick);
    }
    // The skin's current image of that (slot, name), when it has one, is where the list starts.
    if (info && info->images.contains(name_->currentText())) {
      QSignalBlocker quiet(assets_);
      for (int r = 0; r < assets_->count(); r++)
        if (assets_->item(r)->data(Qt::UserRole).toString() == info->images.value(name_->currentText())) assets_->setCurrentRow(r);
    }
    updatePreview();
  }
  const CutoutPanel::SkinSlotInfo* currentSlot() const {
    const QString id = slot_->currentData().toString();
    for (const auto& s : slots_) if (s.slot == id) return &s;
    return nullptr;
  }
  void updatePreview() {
    QListWidgetItem* item = assets_->currentItem();
    if (!preview_ || !item || slot_->currentData().toString().isEmpty()) {
      previewLabel_->setPixmap(QPixmap());
      previewLabel_->setText("이미지를 고르면 결과가 보입니다");
      return;
    }
    const QImage img = preview_(slot_->currentData().toString(), name_->currentText().trimmed(), item->data(Qt::UserRole).toString(), previewLabel_->size() - QSize(4, 4));
    previewLabel_->setText(QString());
    previewLabel_->setPixmap(QPixmap::fromImage(img));
  }
  void finish() {
    choice_.slot = slot_->currentData().toString();
    choice_.name = name_->currentText().trimmed();
    accept();
  }
  CutoutPanel::SkinImageChoice& choice_;
  QVector<CutoutPanel::SkinSlotInfo> slots_;
  Preview preview_;
  QComboBox *slot_, *name_;
  QListWidget* assets_;
  QLabel* previewLabel_;
};

// ============================================================================
namespace {
/**
 * Wheel events over an unfocused spin box or combo box of `panel` scroll the nearest scroll area
 * instead; once the field has keyboard focus (clicked, tabbed into) the wheel steps its value as
 * usual. Qt gives a Qt::WheelFocus widget (spin boxes, their line edits, combo boxes) the focus
 * before any event filter sees a real wheel event, so the fields and their inner line edits are
 * switched to Qt::StrongFocus as soon as they are shown or hovered, before the first wheel.
 */
class WheelGuard : public QObject {
 public:
  explicit WheelGuard(QWidget* panel) : QObject(panel), panel_(panel) {}
  bool eventFilter(QObject* watched, QEvent* event) override {
    const QEvent::Type type = event->type();
    if ((type != QEvent::Wheel && type != QEvent::Show && type != QEvent::Enter) || !watched->isWidgetType()) return false;
    QWidget* field = fieldOf(static_cast<QWidget*>(watched));
    if (!field) return false;
    calm(field);
    if (type != QEvent::Wheel) return false;
    if (field->hasFocus() || (qobject_cast<QComboBox*>(field) && static_cast<QComboBox*>(field)->view()->isVisible())) return false;
    // A forwarded (non-spontaneous) wheel is not propagated by Qt: hand it to the scroll area.
    for (QWidget* p = field->parentWidget(); p && p != panel_; p = p->parentWidget())
      if (auto* area = qobject_cast<QAbstractScrollArea*>(p)) { QCoreApplication::sendEvent(area->viewport(), event); break; }
    return true;
  }

 private:
  /** The spin box / combo box of `w` (itself, or its line edit or popup view) inside the panel. */
  QWidget* fieldOf(QWidget* w) const {
    for (; w && w != panel_; w = w->parentWidget())
      if (qobject_cast<QAbstractSpinBox*>(w) || qobject_cast<QComboBox*>(w)) return panel_->isAncestorOf(w) ? w : nullptr;
    return nullptr;
  }
  static void calm(QWidget* field) {
    if (field->focusPolicy() == Qt::WheelFocus) field->setFocusPolicy(Qt::StrongFocus);
    for (QWidget* c : field->findChildren<QWidget*>(Qt::FindDirectChildrenOnly))
      if (c->focusPolicy() == Qt::WheelFocus) c->setFocusPolicy(Qt::StrongFocus);
  }
  QWidget* panel_;
};
}  // namespace

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
  askPart = [this](PartChoice& choice) { return runPartDialog(choice); };
  askSkinImage = [this](SkinImageChoice& choice) { return runSkinImageDialog(choice); };
  askEventKey = [this](EventKeyChoice& choice) { return runEventKeyDialog(choice); };
  timer_ = new QTimer(this);
  timer_->setInterval(16);
  connect(timer_, &QTimer::timeout, this, [this] { tick(); });
  buildUi();
  // A wheel over a value field or list box that was not clicked scrolls the page instead of
  // changing the value (rolling over 복원력 used to turn 60 into 10 unnoticed). Fields created
  // later (property pages are rebuilt) are covered too: the guard is app-wide but only acts on
  // this panel's widgets, and it goes away with the panel.
  qApp->installEventFilter(new WheelGuard(this));
}
CutoutPanel::~CutoutPanel() = default;
bool CutoutPanel::runPartDialog(PartChoice& choice) {
  QStringList bones, slots;
  const JsonPtr heldData = displayShared();  // shared, not copied
  const Json& data = *heldData;
  for (const auto& b : listOf(data, "bones")) bones << qs(b.value("id", std::string()));
  for (const auto& s : listOf(data, "slots")) slots << qs(s.value("id", std::string()));
  PartDialog dialog(this, images_, bones, slots, choice);
  return dialog.exec() == QDialog::Accepted;
}
bool CutoutPanel::runSkinImageDialog(SkinImageChoice& choice) {
  SkinImageDialog dialog(this, images_, skinSlots(), choice,
                         [this](const QString& slot, const QString& name, const QString& asset, const QSize& size) { return skinImagePreview(slot, name, asset, size); });
  return dialog.exec() == QDialog::Accepted;
}
bool CutoutPanel::runLayerImportDialog(LayerImportChoice& choice, const QJsonObject& report) {
  QPointer<CutoutPanel> self(this);
  LayerImportDialog dialog(this, choice, rigs_.keys(), report, [self](const LayerImportChoice& c, std::function<void(const QJsonObject&, const QString&)> done) {
    if (!self || !self->host_) return;
    self->requestLayerImport(c, true, [done](const QJsonObject& reply) {
      done(reply.value("pluginImport").toObject().value("report").toObject(), reply.value("error").toString());
    });
  });
  return dialog.exec() == QDialog::Accepted;
}
void CutoutPanel::detachHost() {
  timer_->stop();
  playing_ = false;
  host_ = nullptr;
}

// ---------------------------------------------------------------- UI build
namespace {
constexpr const char* LAYOUT_GROUP = "plugins/cutout-animation/layout";
/** Bumped when saved splitter states can no longer be trusted: v1 (plugin <= 1.4.0) put the right
 *  column under the canvas only and could give the look pages so little height that their
 *  buttons were scrolled away; v2 (plugin <= 1.9.0) states were fixed pixel sizes from before the
 *  rows were regrouped and the UI scale existed (a narrow left pane hid 자동 길이 behind »). v3
 *  states are only saved once the user dragged a splitter (`custom`) and are sanitized on restore. */
constexpr int LAYOUT_VERSION = 3;
constexpr const char* VIEW_GROUP = "plugins/cutout-animation/view";
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
/** Small caption that names a group of tools in a row (선택 · 변형 · 본 · 키 ...). The tools after
 *  it carry the group as property cutoutGroup: the command palette lists them under it. */
QLabel* groupLabel(const QString& text) {
  auto* label = new QLabel(text);
  label->setObjectName("cutoutGroupLabel");
  label->setProperty("cutoutGroupLabel", true);
  return label;
}
void setGroup(const QString& group, std::initializer_list<QObject*> objects) {
  for (QObject* o : objects) if (o) o->setProperty("cutoutGroup", group);
}
template <class T> T* childNamed(QObject* in, const char* name) { return in->findChild<T*>(name); }
/** Action-button row of the look pages. Unlike toolRow it never hides a button behind », so
 *  the widest row (in the host's font) is the right pane's minimum width. */
QWidget* buttonRow(const char* name) {
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
void addToRow(QWidget* row, QWidget* w) {
  auto* layout = static_cast<QHBoxLayout*>(row->layout());
  layout->insertWidget(layout->count() - 1, w);  // before the trailing stretch
}
/** Wrapped hint text that may shrink to nothing: when the page is short, lists and buttons win. */
class HintLabel : public QLabel {
 public:
  QSize minimumSizeHint() const override { return {0, 0}; }
};
}  // namespace

// Spine-like single layout, the same in the host workspace and in an old host's dock:
//   mode bar (always visible)
//   [ [ left: 계층 / 그리기 순서 / 캐릭터 | center: tools + canvas ]            | right (full height):
//     [ bottom tabs: 애니메이션 (clips | transport + dopesheet/graph | mix) · 게임 연결 ] |   속성 · 키 / 스킨 · 표정 · IK · 메시 / 진단 ]
// Every pane is a splitter child with a small minimum: lists scroll and tool rows overflow
// instead of imposing a large minimum height/width on the window (the look pages' button rows
// are the exception: they set the right column's minimum so no button is ever hidden).
void CutoutPanel::buildUi() {
  // UI scale (plugin 1.10.0): the user's choice, else the screen's default. The style sheet with
  // the compact (scaled) paddings is set by applyUiScale at the end.
  {
    QSettings q;
    q.beginGroup(VIEW_GROUP);
    uiScaleChosen_ = q.contains("uiScale");
    const QScreen* screen = QGuiApplication::primaryScreen();
    ui_ = uiScaleChosen_ ? std::clamp(q.value("uiScale").toDouble(), 0.75, 2.0)
                         : screen ? defaultUiScale(screen->size(), screen->devicePixelRatio(), screen->logicalDotsPerInch()) : 1.0;
  }
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(4, 4, 4, 4);
  root->setSpacing(4);
  // Mode bar with a strong color banner so the user always knows whether edits make keys.
  auto* modeBar = modeBar_ = new QWidget;
  modeBar->setObjectName("cutoutModeBar");
  modeBar->setProperty("cutoutGroup", "모드");
  auto* modeLayout = new QHBoxLayout(modeBar);
  modeLayout->setContentsMargins(0, 0, 0, 0);
  setupButton_ = new QPushButton("조립 (Setup)");
  setupButton_->setObjectName("cutoutModeSetup");
  setupButton_->setToolTip("조립 모드: 본과 이미지 조각의 기본 자세를 편집합니다(키가 생기지 않음).");
  animateButton_ = new QPushButton("애니메이션 (Animate)");
  animateButton_->setObjectName("cutoutModeAnimate");
  animateButton_->setToolTip("애니메이션 모드: 재생 헤드 시간에 자세를 바꾸고 K로 키를 넣습니다.");
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
  buildViewMenu();
  // 보기 (plugin 1.10.0): canvas overlays, fit, full view and the 보기 ▾ menu share the mode bar's
  // free width, so the canvas rows only hold the tools (no » on 1280 px windows).
  viewTools_ = toolRow("cutoutViewTools");
  viewTools_->setProperty("cutoutGroup", "보기");
  modeLayout->addWidget(viewTools_);
  root->addWidget(modeBar);

  mainSplit_ = new QSplitter(Qt::Vertical);
  mainSplit_->setObjectName("cutoutMainSplitter");
  mainSplit_->setChildrenCollapsible(false);
  mainSplit_->addWidget(buildWorkspace());
  viewTools_->addSeparator();
  viewTools_->addWidget(fullViewButton_);
  viewTools_->addWidget(viewMenuButton_);
  buildHintStrip();
  bottomTabs_ = new QTabWidget;
  bottomTabs_->setObjectName("cutoutBottomTabs");
  bottomTabs_->addTab(buildAnimateTab(), "애니메이션");
  bottomTabs_->addTab(buildControllerTab(), "게임 연결");
  bottomTabs_->addTab(buildExportTab(), "내보내기");
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
  // The right column (속성 · 키 / 스킨 · 표정 · IK · 메시 / 진단) spans the full height beside
  // canvas and timeline: under the canvas only it got a third of a 1280x720 window, too little
  // for the look pages' buttons. It keeps its width on wide screens.
  outerSplit_ = new QSplitter(Qt::Horizontal);
  outerSplit_->setObjectName("cutoutOuterSplitter");
  outerSplit_->setChildrenCollapsible(false);
  outerSplit_->addWidget(mainSplit_);
  outerSplit_->addWidget(rightPane_);
  outerSplit_->setStretchFactor(0, 1);
  outerSplit_->setStretchFactor(1, 0);
  outerSplit_->setSizes({1300, 330});
  root->addWidget(outerSplit_, 1);
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
  shortcut(Qt::Key_C, "cutoutShortcutCreate", [this] { setTool(RigCanvas::Create); });
  shortcut(Qt::Key_K, "cutoutShortcutKey", [this] { keySelected(); });
  shortcut(Qt::Key_O, "cutoutShortcutOnion", [this] { setOnionSkin(!onion_.enabled); });
  shortcut(Qt::Key_Space, "cutoutShortcutPlay", [this] { if (mode_ == Mode::Animate) playButton_->click(); });
  shortcut(Qt::Key_Comma, "cutoutShortcutStepBack", [this] { if (mode_ == Mode::Animate) stepPlayhead(-1); });
  shortcut(Qt::Key_Period, "cutoutShortcutStepForward", [this] { if (mode_ == Mode::Animate) stepPlayhead(1); });
  // Plugin 1.10.0: the command palette and the workspace text size.
  shortcut(QKeySequence(Qt::CTRL | Qt::Key_K), "cutoutShortcutPalette", [this] { showCommandPalette(); });
  shortcut(Qt::Key_F1, "cutoutShortcutPaletteF1", [this] { showCommandPalette(); });
  shortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal), "cutoutShortcutUiLarger", [this] { stepUiScale(1); });
  shortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus), "cutoutShortcutUiLargerPlus", [this] { stepUiScale(1); });
  shortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus), "cutoutShortcutUiSmaller", [this] { stepUiScale(-1); });
  shortcut(QKeySequence(Qt::CTRL | Qt::Key_0), "cutoutShortcutUiNormal", [this] { setUiScale(1.0); });
  // Tab = full view, but only where Tab has no text-entry meaning (form fields keep focus
  // navigation): an event filter sees the key before QWidget's focus-chain handling.
  for (QWidget* w : {static_cast<QWidget*>(canvas_), static_cast<QWidget*>(dopesheet_), static_cast<QWidget*>(graph_)}) w->installEventFilter(this);
  buildCommandPalette();
  finishTooltips();
  restoreLayout();
  // A drag of any splitter makes the layout the user's (kept, saved) from then on.
  for (QSplitter* s : splitters()) connect(s, &QSplitter::splitterMoved, this, [this] {
    if (applyingLayout_) return;
    autoLayout_ = false;
    userSized_ = true;
    saveLayout();
  });
  refreshModeUi();
  syncOnionUi();
  applyUiScale();
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
  q.setValue("version", LAYOUT_VERSION);
  // Splitter states only once the user sized the panes: an automatic layout is recomputed from
  // the window (and UI scale) every time instead of freezing one window's pixel sizes.
  q.setValue("custom", !autoLayout_);
  for (QSplitter* s : splitters()) {
    if (autoLayout_) q.remove(s->objectName());
    else q.setValue(s->objectName(), s->saveState());
  }
  QStringList collapsed;
  for (QToolButton* head : sections_) if (!head->isChecked()) collapsed << head->objectName();
  q.setValue("collapsed", collapsed);
}
void CutoutPanel::restoreLayout() {
  restoringLayout_ = true;
  QSettings q;
  q.beginGroup(LAYOUT_GROUP);
  // Splitter states of an older layout come back as the defaults (collapsed sections are kept):
  // v1 states hold three panes in the workspace splitter and right-column sizes that could
  // starve the look pages; v2 states predate the regrouped rows. The next save writes v3.
  bool restored = false;
  if (q.value("version", 1).toInt() >= LAYOUT_VERSION && q.value("custom", false).toBool())
    for (QSplitter* s : splitters())
      if (q.contains(s->objectName())) restored = s->restoreState(q.value(s->objectName()).toByteArray()) || restored;
  autoLayout_ = !restored;
  userSized_ = false;  // a restored layout is still sanitized (adjustLayout) until the user drags
  const QStringList collapsed = q.value("collapsed").toStringList();
  for (QToolButton* head : sections_) head->setChecked(!collapsed.contains(head->objectName()));
  restoringLayout_ = false;
  adjustLayout();
}
void CutoutPanel::setFullView(bool on) {
  // Editor-only presentation (never saved): the canvas takes the side panels' width.
  leftPane_->setVisible(!on);
  rightPane_->setVisible(!on);
  if (fullViewButton_->isChecked() != on) { QSignalBlocker block(fullViewButton_); fullViewButton_->setChecked(on); }
}
bool CutoutPanel::fullView() const { return leftPane_ && leftPane_->isHidden(); }
bool CutoutPanel::eventFilter(QObject* watched, QEvent* event) {
  if (watched == watchedParent_ && event->type() == QEvent::FontChange) {
    QTimer::singleShot(0, this, [this] { applyUiScale(); });
    return false;
  }
  if (watched == canvas_ && (event->type() == QEvent::Resize || event->type() == QEvent::FontChange)) layoutHintStrip();
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
  if (watched == tree_ && (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
    // The focused hierarchy owns Ctrl+A / Ctrl+Shift+A (bones, not its own select-all of parts;
    // there the selection command wins over the host's workspace toggle, which the canvas keeps)
    // and not the host's workspace toggle).
    auto* key = static_cast<QKeyEvent*>(event);
    if (key->key() == Qt::Key_A && (key->modifiers() & Qt::ControlModifier) && !(key->modifiers() & Qt::AltModifier)) {
      event->accept();
      if (event->type() == QEvent::KeyPress) { if (key->modifiers() & Qt::ShiftModifier) selectChildren(); else selectAllBones(); }
      return true;
    }
  }
  if (palette_ && (watched == paletteSearch_ || watched == paletteList_)) {
    if (event->type() == QEvent::KeyPress) {
      auto* key = static_cast<QKeyEvent*>(event);
      if (key->key() == Qt::Key_Escape) { palette_->hide(); canvas_->setFocus(Qt::OtherFocusReason); return true; }
      // The search field keeps the focus: arrows and page keys move the list's current row.
      if (watched == paletteSearch_ && (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down || key->key() == Qt::Key_PageUp || key->key() == Qt::Key_PageDown)) {
        QCoreApplication::sendEvent(paletteList_, event);
        return true;
      }
    }
    if (event->type() == QEvent::FocusOut) {
      // Closing on focus loss (a click elsewhere), unless the focus moved inside the palette.
      QTimer::singleShot(0, this, [this] {
        QWidget* f = QApplication::focusWidget();
        if (palette_->isVisible() && !(f && palette_->isAncestorOf(f))) palette_->hide();
      });
    }
    return QWidget::eventFilter(watched, event);
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

// ---------------------------------------------------------------- workspace view (plugin 1.10.0)
// Editor-only presentation: the UI scale, the hint strip and the automatic layout live in QSettings
// (plugins/cutout-animation/view, .../layout); none of it is ever an edit.
const QVector<CutoutPanel::UiScaleChoice>& CutoutPanel::uiScales() {
  static const QVector<UiScaleChoice> v = {{0.9, "작게", "cutoutUiScaleSmall"}, {1.0, "보통", "cutoutUiScaleNormal"},
                                           {1.25, "크게", "cutoutUiScaleLarge"}, {1.5, "아주 크게", "cutoutUiScaleHuge"}};
  return v;
}
double CutoutPanel::defaultUiScale(const QSize& screen, double devicePixelRatio, double logicalDpi) {
  // Windows display scaling (dpr > 1) or a larger system text size already sizes the text.
  if (screen.isEmpty() || devicePixelRatio > 1.2 || logicalDpi > 110) return 1.0;
  // At 100 %, the short side tells the pixel density of the common monitors (1440 = 27" QHD and
  // ultrawides, 2160 = 4K) whose 9 pt text is what users found too small.
  const int shortSide = std::min(screen.width(), screen.height());
  double scale = shortSide >= 2000 ? 1.5 : shortSide >= 1400 ? 1.25 : 1.0;
  // Each step keeps 1280 logical px of width per 1.0 (a portrait QHD screen stays 보통).
  while (scale > 1.0 && screen.width() < 1280 * scale) scale = scale > 1.25 ? 1.25 : 1.0;
  return scale;
}
void CutoutPanel::setUiScale(double scale) {
  scale = std::clamp(scale, 0.75, 2.0);
  uiScaleChosen_ = true;
  QSettings q;
  q.beginGroup(VIEW_GROUP);
  q.setValue("uiScale", scale);
  ui_ = scale;
  applyUiScale();  // also when unchanged: the host font may have changed since
  QString label = QString::number(int(std::lround(scale * 100))) + "%";
  for (const auto& c : uiScales()) if (std::fabs(c.scale - scale) < 1e-6) label = QString("%1 (%2)").arg(c.label, label);
  setStatus(QString("작업 공간 글자 크기: %1 · Ctrl+= 크게 · Ctrl+- 작게 · Ctrl+0 보통 · 보기 ▾에서 「자동」 (이 컴퓨터에만 저장)").arg(label));
}
void CutoutPanel::setUiScaleAuto() {
  // Plugin 1.14.0: back to the screen's default (no saved choice), as on first start.
  uiScaleChosen_ = false;
  QSettings q;
  q.beginGroup(VIEW_GROUP);
  q.remove("uiScale");
  const QScreen* s = screen() ? screen() : QGuiApplication::primaryScreen();
  ui_ = s ? defaultUiScale(s->size(), s->devicePixelRatio(), s->logicalDotsPerInch()) : 1.0;
  applyUiScale();
  QString label = QString::number(int(std::lround(ui_ * 100))) + "%";
  for (const auto& c : uiScales()) if (std::fabs(c.scale - ui_) < 1e-6) label = QString("%1 (%2)").arg(c.label, label);
  setStatus(QString("작업 공간 글자 크기: 자동 — 이 화면에서는 %1. 화면이 바뀌면 다시 맞춥니다 (보기 ▾ › 작업 공간 글자 크기).").arg(label));
}
void CutoutPanel::stepUiScale(int delta) {
  const auto& v = uiScales();
  int at = 0;
  for (int i = 0; i < v.size(); i++) if (std::fabs(v[i].scale - ui_) < std::fabs(v[at].scale - ui_)) at = i;
  setUiScale(v[std::clamp(at + delta, 0, int(v.size()) - 1)].scale);
}
void CutoutPanel::applyUiScale() {
  const double u = ui_;
  auto px = [u](double v) { return std::max(1, int(std::lround(v * u))); };
  // Text: the host's font size times the scale (only the size is set, family and style stay the
  // host's). Top-level popups do not inherit fonts, so they get it explicitly.
  const QFont base = parentWidget() ? parentWidget()->font() : QApplication::font();
  QFont f;
  if (base.pointSizeF() > 0) f.setPointSizeF(base.pointSizeF() * u);
  else f.setPixelSize(px(base.pixelSize() > 0 ? base.pixelSize() : 12));
  setFont(f);
  if (onionPopup_) onionPopup_->setFont(font());
  // The size also as a style-sheet rule: under a style sheet, child widgets do not inherit a font
  // set on the panel, so the rule is what reaches every child (and the panel's dialogs).
  const QString fontSize = base.pointSizeF() > 0 ? QString::number(base.pointSizeF() * u, 'f', 2) + "pt" : QString::number(f.pixelSize()) + "px";
  // Compact rows (cascades over the host theme), paddings scaled with the text.
  setStyleSheet(QString("#cutoutPanel,#cutoutPanel *{font-size:%1}").arg(fontSize) +
                QString("QToolBar{padding:0;spacing:%1px;border:0}"
                        "QToolBar QPushButton,QToolBar QToolButton,#cutoutClipsPane QPushButton,#cutoutMixPane QPushButton,"
                        "QWidget[cutoutRow=\"true\"] QPushButton,QWidget[cutoutRow=\"true\"] QToolButton{padding:%1px %2px}"
                        "QToolBar QSpinBox,QToolBar QComboBox{padding:%3px;min-height:%4px}"
                        "QToolButton[cutoutSection=\"true\"]{padding:%3px %5px;font-weight:bold}"
                        "QLabel[cutoutGroupLabel=\"true\"]{color:#8b98a5;font-weight:bold;padding:0 %3px 0 %5px}"
                        "#cutoutHintStrip{background:#233044;border:1px solid #3b4d66;border-radius:%5px}"
                        "#cutoutCommandPalette{background:#1c232c;border:1px solid #60738a;border-radius:%5px}"
                        "QTabBar::tab{padding:%5px %6px}")
                    .arg(px(3)).arg(px(6)).arg(px(2)).arg(px(18)).arg(px(4)).arg(px(12)));
  for (QToolBar* bar : findChildren<QToolBar*>()) bar->setIconSize(QSize(px(16), px(16)));
  for (QPushButton* b : {setupButton_, animateButton_}) b->setMinimumHeight(px(28));
  banner_->setMinimumHeight(px(28));
  keyButton_->setMinimumHeight(px(30));
  tree_->setColumnWidth(0, px(170));
  for (QListWidget* l : {skinList_, skinAttachments_, expressionList_}) l->setIconSize(QSize(px(20), px(20)));
  canvas_->setUiScale(u);
  timeView_->setUiScale(u);
  dopesheet_->setRows(dopesheet_->rows(), timeView_->duration());  // row height -> its minimum height
  graph_->update();
  refreshModeUi();  // the banner's own style sheet
  for (QAction* a : uiScaleActions_) a->setChecked(a->data().toDouble() < 0 ? !uiScaleChosen_ : uiScaleChosen_ && std::fabs(a->data().toDouble() - u) < 1e-6);
  // Rows got wider: an automatic layout is recomputed, a user layout widened where needed.
  if (autoLayout_) applyDefaultLayout();
  else sanitizeLayout();
}

void CutoutPanel::buildViewMenu() {
  viewMenuButton_ = new QToolButton;
  viewMenuButton_->setObjectName("cutoutViewMenu");
  viewMenuButton_->setText("보기 ▾");
  viewMenuButton_->setPopupMode(QToolButton::InstantPopup);
  viewMenuButton_->setToolTip("작업 공간 글자 크기(Ctrl+= / Ctrl+-), 명령 찾기(Ctrl+K), 도움말 줄, 레이아웃 초기화");
  auto* menu = new QMenu(viewMenuButton_);
  menu->setObjectName("cutoutViewMenuPopup");
  auto action = [this](QMenu* in, const QString& text, const char* name, const QString& key, std::function<void()> fn) {
    QAction* a = in->addAction(key.isEmpty() ? text : text + "\t" + key);
    a->setObjectName(name);
    a->setProperty("cutoutGroup", "보기");
    if (!key.isEmpty()) a->setProperty("cutoutShortcut", key);
    connect(a, &QAction::triggered, this, std::move(fn));
    return a;
  };
  // The palette opens after the menu closed (its own focus would go back to the menu otherwise).
  action(menu, "명령 찾기…", "cutoutCommandPaletteAction", "Ctrl+K", [this] { QTimer::singleShot(0, this, [this] { showCommandPalette(); }); });
  QMenu* sizes = menu->addMenu("작업 공간 글자 크기");
  sizes->setObjectName("cutoutUiScaleMenu");
  auto* group = new QActionGroup(sizes);
  group->setExclusive(true);
  QAction* automatic = action(sizes, "자동 (화면에 맞춤)", "cutoutUiScaleAuto", QString(), [this] { setUiScaleAuto(); });
  automatic->setProperty("cutoutGroup", "작업 공간 글자 크기");
  automatic->setToolTip("저장한 크기를 지우고 처음처럼 화면 크기·배율에 맞춥니다.");
  automatic->setCheckable(true);
  automatic->setData(-1.0);
  group->addAction(automatic);
  uiScaleActions_ << automatic;
  sizes->addSeparator();
  for (const auto& c : uiScales()) {
    const double scale = c.scale;
    QAction* a = action(sizes, QString("%1 (%2%)").arg(c.label).arg(int(std::lround(scale * 100))), c.objectName, QString(), [this, scale] { setUiScale(scale); });
    a->setProperty("cutoutGroup", "작업 공간 글자 크기");
    a->setCheckable(true);
    a->setData(scale);
    group->addAction(a);
    uiScaleActions_ << a;
  }
  sizes->addSeparator();
  action(sizes, "한 단계 크게", "cutoutUiScaleUp", "Ctrl+=", [this] { stepUiScale(1); })->setProperty("cutoutGroup", "작업 공간 글자 크기");
  action(sizes, "한 단계 작게", "cutoutUiScaleDown", "Ctrl+-", [this] { stepUiScale(-1); })->setProperty("cutoutGroup", "작업 공간 글자 크기");
  menu->addSeparator();
  hintAction_ = action(menu, "도움말 줄 보이기", "cutoutShowHintAction", QString(), [this] { setHintVisible(hintAction_->isChecked()); });
  hintAction_->setCheckable(true);
  action(menu, "레이아웃 초기화", "cutoutResetLayoutAction", QString(), [this] { resetLayout(); });
  action(menu, "캔버스 크게", "cutoutFullViewAction", "Tab", [this] { setFullView(!fullView()); });
  viewMenuButton_->setMenu(menu);
}

void CutoutPanel::buildHintStrip() {
  // A strip over the top of the canvas (its child) rather than a layout row: at 1280x720 every
  // row of height is taken, and the canvas has room until the user dismisses it.
  hintStrip_ = new QFrame(canvas_);
  hintStrip_->setObjectName("cutoutHintStrip");
  hintStrip_->setProperty("cutoutGroup", "도움말");
  auto* row = new QHBoxLayout(hintStrip_);
  row->setContentsMargins(6, 2, 2, 2);
  auto* text = new HintLabel;
  text->setObjectName("cutoutHintText");
  text->setText("처음이라면: Ctrl+K(또는 F1)로 모든 명령을 이름·단축키로 찾아 실행합니다 · 조립 모드: 「새 캐릭터」 → 「이미지 조각 추가…」 → 본 만들기(C) · "
                "애니메이션 모드: 자세를 바꾸고 K · 글자 크기: 「보기 ▾」 또는 Ctrl+= / Ctrl+-");
  text->setWordWrap(true);
  text->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  auto* open = new QToolButton;
  open->setObjectName("cutoutHintPalette");
  open->setText("명령 찾기 (Ctrl+K)");
  open->setToolTip("모든 명령을 이름이나 단축키로 찾아 실행합니다 (Ctrl+K 또는 F1).");
  open->setProperty("cutoutShortcut", "Ctrl+K");
  connect(open, &QToolButton::clicked, this, [this] { showCommandPalette(); });
  auto* close = new QToolButton;
  close->setObjectName("cutoutHintDismiss");
  close->setText("닫기 ✕");
  close->setToolTip("이 안내 줄을 닫습니다. 「보기 ▾ › 도움말 줄 보이기」로 다시 켭니다(이 컴퓨터에만 저장).");
  connect(close, &QToolButton::clicked, this, [this] { setHintVisible(false); });
  row->addWidget(text, 1);
  row->addWidget(open);
  row->addWidget(close);
  QSettings q;
  q.beginGroup(VIEW_GROUP);
  const bool shown = !q.value("hintDismissed", false).toBool();
  hintStrip_->setVisible(shown);
  if (hintAction_) hintAction_->setChecked(shown);
  layoutHintStrip();
}
void CutoutPanel::layoutHintStrip() {
  if (!hintStrip_ || !canvas_) return;
  const int margin = int(std::lround(8 * ui_)), w = std::max(80, canvas_->width() - 2 * margin);
  QLayout* layout = hintStrip_->layout();
  const int h = std::max(layout->minimumSize().height(), layout->hasHeightForWidth() ? layout->heightForWidth(w) : layout->sizeHint().height());
  hintStrip_->setGeometry(margin, margin, w, std::min(h, std::max(24, canvas_->height() / 2)));
  hintStrip_->raise();
}
void CutoutPanel::setHintVisible(bool on) {
  hintStrip_->setVisible(on);
  if (on) layoutHintStrip();
  if (hintAction_ && hintAction_->isChecked() != on) hintAction_->setChecked(on);
  QSettings q;
  q.beginGroup(VIEW_GROUP);
  q.setValue("hintDismissed", !on);
}
bool CutoutPanel::hintVisible() const { return hintStrip_ && !hintStrip_->isHidden(); }

// ---- command palette
namespace {
/** Group of a command: the nearest cutoutGroup property up the parent chain. */
QString groupOf(const QObject* o) {
  for (; o; o = o->parent()) if (o->property("cutoutGroup").isValid()) return o->property("cutoutGroup").toString();
  return {};
}
/** Buttons that are commands: named, not a section header, not opening a menu (its actions are
 *  listed instead), not part of the palette or the hint strip's own close button. */
bool isCommandButton(const QAbstractButton* b) {
  const QString n = b->objectName();
  if (!n.startsWith("cutout") || n == "cutoutHintDismiss" || b->property("cutoutSection").toBool()) return false;
  if (auto* t = qobject_cast<const QToolButton*>(b); t && t->menu()) return false;
  return true;
}
/** A button's name: its text, or for a symbol-only button (◀ ▶ − + ▾) its tooltip's first phrase. */
QString commandName(const QString& rawText, const QString& tip) {
  QString text = rawText;
  text.remove('&');
  text = text.section('\t', 0, 0).simplified();
  bool word = false;
  for (QChar c : text) word = word || c.isLetterOrNumber();
  if (word || tip.isEmpty()) return text;
  QString first = tip.section('\n', 0, 0);
  const int paren = first.indexOf(" (");
  if (paren > 0) first = first.left(paren);
  if (first.endsWith('.')) first.chop(1);
  return first.simplified();
}
QString shortcutOf(const QObject* o, const QString& text) {
  if (o->property("cutoutShortcut").isValid()) return o->property("cutoutShortcut").toString();
  return text.contains('\t') ? text.section('\t', 1) : QString();
}
}  // namespace

QVector<CutoutPanel::Command> CutoutPanel::commands() const {
  QVector<Command> out;
  QSet<QString> seen;
  auto skip = [this](const QObject* o) {
    for (const QObject* p = o; p; p = p->parent()) if (p == palette_) return true;
    return false;
  };
  for (QAbstractButton* b : findChildren<QAbstractButton*>()) {
    if (!isCommandButton(b) || skip(b) || seen.contains(b->objectName())) continue;
    seen.insert(b->objectName());
    out.push_back({b->objectName(), commandName(b->text(), b->toolTip()), groupOf(b), shortcutOf(b, b->text()), b->toolTip(), b->isEnabled()});
  }
  for (QAction* a : findChildren<QAction*>()) {
    if (!a->objectName().startsWith("cutout") || a->objectName() == "cutoutCommandPaletteAction" || seen.contains(a->objectName())) continue;
    seen.insert(a->objectName());
    out.push_back({a->objectName(), commandName(a->text(), a->toolTip()), groupOf(a), shortcutOf(a, a->text()), a->toolTip(), a->isEnabled()});
  }
  // Pages (bottom tabs, look tabs): opening one is a command too.
  for (QTabWidget* tabs : findChildren<QTabWidget*>()) {
    if (!tabs->objectName().startsWith("cutout")) continue;
    for (int i = 0; i < tabs->count(); i++)
      out.push_back({QString("tab:%1:%2").arg(tabs->objectName()).arg(i), tabs->tabText(i) + " 탭 열기", "보기", QString(),
                     QString("「%1」 탭을 엽니다.").arg(tabs->tabText(i)), true});
  }
  return out;
}
QVector<CutoutPanel::Command> CutoutPanel::findCommands(const QString& queryIn) const {
  const QVector<Command> all = commands();
  const QString query = queryIn.simplified();
  if (query.isEmpty()) return all;
  const QStringList tokens = query.split(' ', Qt::SkipEmptyParts);
  QVector<QPair<int, int>> scored;  // (score, index)
  for (int i = 0; i < all.size(); i++) {
    const Command& c = all[i];
    int score = 0;
    if (c.name.compare(query, Qt::CaseInsensitive) == 0) score = 100;
    else if (!c.shortcut.isEmpty() && c.shortcut.compare(query, Qt::CaseInsensitive) == 0) score = 95;
    else if (c.name.startsWith(query, Qt::CaseInsensitive)) score = 80;
    else if (c.name.contains(query, Qt::CaseInsensitive)) score = 60;
    else {
      // Every word somewhere in group / name / shortcut ("스킨 삭제", "메시 정점").
      const QString hay = c.group + " " + c.name + " " + c.shortcut;
      bool all = true;
      for (const QString& t : tokens) all = all && hay.contains(t, Qt::CaseInsensitive);
      if (all) score = 50;
      else if (c.tip.contains(query, Qt::CaseInsensitive)) score = 20;
    }
    if (score > 0) scored.push_back({score + (c.enabled ? 1 : 0), i});
  }
  std::stable_sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
  QVector<Command> out;
  for (const auto& s : scored) out << all[s.second];
  return out;
}
QWidget* CutoutPanel::commandWidget(const QString& id) const {
  for (QAbstractButton* b : findChildren<QAbstractButton*>(id)) if (isCommandButton(b)) return b;
  return nullptr;
}
void CutoutPanel::revealWidget(QWidget* w) {
  if (!w) return;
  if (fullView() && (leftPane_->isAncestorOf(w) || rightPane_->isAncestorOf(w))) setFullView(false);
  for (QToolButton* head : sections_)
    if (!head->isChecked() && head->parentWidget() && head->parentWidget()->isAncestorOf(w)) head->setChecked(true);
  // A tab page's parent is the tab widget's stack: select every page on the way up.
  for (QWidget* p = w; p && p != this; p = p->parentWidget()) {
    QWidget* stack = p->parentWidget();
    if (auto* tabs = qobject_cast<QTabWidget*>(stack ? stack->parentWidget() : nullptr)) {
      const int i = tabs->indexOf(p);
      if (i >= 0 && tabs->currentIndex() != i) tabs->setCurrentIndex(i);
    }
  }
}
bool CutoutPanel::runCommand(const QString& id) {
  if (id.startsWith("tab:")) {
    auto* tabs = findChild<QTabWidget*>(id.section(':', 1, 1));
    const int i = id.section(':', 2).toInt();
    if (!tabs || i < 0 || i >= tabs->count()) return false;
    revealWidget(tabs);
    tabs->setCurrentIndex(i);
    return true;
  }
  if (auto* b = qobject_cast<QAbstractButton*>(commandWidget(id))) {
    if (!b->isEnabled()) {
      setStatus(QString("「%1」은(는) 지금 쓸 수 없습니다(모드나 선택을 확인하세요).").arg(commandName(b->text(), b->toolTip())), true);
      return false;
    }
    revealWidget(b);
    b->click();
    return true;
  }
  for (QAction* a : findChildren<QAction*>(id)) {
    if (!a->isEnabled()) return false;
    a->trigger();
    return true;
  }
  return false;
}
void CutoutPanel::buildCommandPalette() {
  // An overlay child (not a window): it shares the workspace font and focus chain, and closes
  // when it loses the focus or with Esc.
  palette_ = new QFrame(this);
  palette_->setObjectName("cutoutCommandPalette");
  palette_->hide();
  auto* layout = new QVBoxLayout(palette_);
  layout->setContentsMargins(8, 6, 8, 8);
  layout->setSpacing(4);
  auto* title = new QLabel("명령 찾기 · 이름·묶음·단축키로 찾고 Enter로 실행 (↑↓ 고르기, Esc 닫기)");
  title->setObjectName("cutoutCommandTitle");
  title->setStyleSheet("color:#a7b4c2;");
  paletteSearch_ = new QLineEdit;
  paletteSearch_->setObjectName("cutoutCommandSearch");
  paletteSearch_->setPlaceholderText("예: 본 추가 · 메시 · 스킨 삭제 · K · Ctrl+C");
  paletteSearch_->setClearButtonEnabled(true);
  paletteList_ = new QListWidget;
  paletteList_->setObjectName("cutoutCommandList");
  paletteList_->setUniformItemSizes(true);
  layout->addWidget(title);
  layout->addWidget(paletteSearch_);
  layout->addWidget(paletteList_, 1);
  auto run = [this] {
    QListWidgetItem* item = paletteList_->currentItem();
    if (!item) return;
    const QString id = item->data(Qt::UserRole).toString();
    palette_->hide();
    canvas_->setFocus(Qt::OtherFocusReason);
    runCommand(id);
  };
  connect(paletteSearch_, &QLineEdit::textChanged, this, [this](const QString& text) { fillPalette(text); });
  connect(paletteSearch_, &QLineEdit::returnPressed, this, run);
  connect(paletteList_, &QListWidget::itemActivated, this, run);
  paletteSearch_->installEventFilter(this);
  paletteList_->installEventFilter(this);
}
void CutoutPanel::fillPalette(const QString& query) {
  paletteList_->clear();
  for (const Command& c : findCommands(query)) {
    QString text = c.group.isEmpty() ? c.name : c.group + " › " + c.name;
    if (!c.shortcut.isEmpty()) text += "   [" + c.shortcut + "]";
    if (!c.enabled) text += "   (지금은 쓸 수 없음)";
    auto* item = new QListWidgetItem(text, paletteList_);
    item->setData(Qt::UserRole, c.id);
    item->setToolTip(c.tip);
    if (!c.enabled) item->setForeground(QColor("#6b7580"));
  }
  if (paletteList_->count()) paletteList_->setCurrentRow(0);
}
void CutoutPanel::showCommandPalette() {
  const int top = modeBar_->geometry().bottom() + 4;
  const int w = std::max(200, std::min(width() - 24, int(640 * ui_)));
  palette_->setGeometry((width() - w) / 2, top, w, std::max(120, std::min(height() - top - 12, int(460 * ui_))));
  {
    QSignalBlocker quiet(paletteSearch_);
    paletteSearch_->clear();
  }
  fillPalette(QString());
  palette_->show();
  palette_->raise();
  paletteSearch_->setFocus(Qt::OtherFocusReason);
}
QWidget* CutoutPanel::commandPalette() const { return palette_; }

void CutoutPanel::finishTooltips() {
  // Every tool explains itself on hover, and names its shortcut when it has one.
  auto finish = [](QObject* o, const QString& text, const QString& tip, const std::function<void(const QString&)>& set) {
    QString out = tip.isEmpty() ? commandName(text, QString()) : tip;
    const QString key = shortcutOf(o, text);
    if (!key.isEmpty() && !out.contains(key)) out += QString("\n단축키: %1").arg(key);
    if (out != tip) set(out);
  };
  for (QAbstractButton* b : findChildren<QAbstractButton*>())
    if (b->objectName().startsWith("cutout")) finish(b, b->text(), b->toolTip(), [b](const QString& t) { b->setToolTip(t); });
  for (QAction* a : findChildren<QAction*>())
    if (a->objectName().startsWith("cutout")) finish(a, a->text(), a->property("cutoutTip").toString(), [a](const QString& t) { a->setToolTip(t); a->setProperty("cutoutTip", t); });
  for (QMenu* m : findChildren<QMenu*>()) m->setToolTipsVisible(true);
}

// ---- layout: proportional defaults from the window size, never below the rows' needs
namespace {
/** Widest tool row of a pane: QToolBar's size hint is the width that shows all of its items. */
int rowsNeed(const QWidget* pane) {
  int need = 0;
  if (pane) for (const QToolBar* bar : pane->findChildren<QToolBar*>()) need = std::max(need, bar->sizeHint().width());
  return need;
}
}  // namespace
int CutoutPanel::paneNeed(const QString& pane) const {
  if (pane == "left") return std::max({rowsNeed(leftPane_) + 4, leftPane_->minimumSizeHint().width(), leftPane_->minimumWidth()});
  if (pane == "canvas") return std::max(rowsNeed(canvasPane_) + 4, canvasPane_->minimumWidth());
  if (pane == "right") {
    // The look pages' button rows (the column's minimum) and the property form without scrolling sideways.
    const int form = propertiesScroll_->widget()->sizeHint().width() + propertiesScroll_->verticalScrollBar()->sizeHint().width() + 8;
    return std::max(rightPane_->minimumSizeHint().width(), form);
  }
  if (pane == "clips") return std::max(clipsPane_->minimumSizeHint().width(), clipsPane_->minimumWidth());
  if (pane == "mix") return std::max(mixPane_->minimumSizeHint().width(), mixPane_->minimumWidth());
  if (pane == "timeline") return std::max(rowsNeed(timelinePane_) + 4, timelinePane_->minimumWidth());
  if (pane == "bottom") {
    // Both pages that share the bottom: clips | time rows | mix, and the game-connection rows.
    const int animate = paneNeed("clips") + paneNeed("timeline") + paneNeed("mix") + 2 * timelineAreaSplit_->handleWidth();
    const int controller = rowsNeed(bottomTabs_->findChild<QWidget*>("cutoutControllerTab"));
    return std::max(animate, controller) + 12;  // tab frame and page margins
  }
  return 0;
}
QStringList CutoutPanel::overflowingRows() const {
  QStringList out;
  for (QToolBar* bar : findChildren<QToolBar*>()) {
    if (!bar->isVisible()) continue;
    auto* more = bar->findChild<QToolButton*>("qt_toolbar_ext_button");
    if (more && more->isVisible()) out << bar->objectName();
  }
  return out;
}
void CutoutPanel::applyDefaultLayout() {
  const int W = outerSplit_->width(), H = outerSplit_->height();
  if (W < 320 || H < 240 || fullView()) return;
  applyingLayout_ = true;
  const int needL = paneNeed("left"), needC = paneNeed("canvas"), needR = paneNeed("right"), needB = paneNeed("bottom");
  const int minL = std::max(leftPane_->minimumWidth(), leftPane_->minimumSizeHint().width()), minR = rightPane_->minimumSizeHint().width();
  const int hO = outerSplit_->handleWidth(), hW = workspaceSplit_->handleWidth();
  // Proportional share of the window, at least what the rows need, at most a bounded multiple
  // of it (an ultrawide gives its extra width to the canvas and the time area).
  auto grow = [](int need, double share, int avail, double cap) { return std::max(need, std::min(int(avail * share), int(need * cap))); };
  int right = grow(needR, 0.2, W, 1.6);
  int left = grow(needL, 0.14, W - right - hO, 1.5);
  // Too narrow for every need: the side panes give back their extra, then go down to their minimum.
  auto deficit = [&] { return std::max(left + hW + needC, needB) - (W - right - hO); };
  for (auto [size, floor] : {std::pair<int*, int>{&right, needR}, {&left, needL}, {&right, minR}, {&left, minL}}) {
    const int d = deficit();
    if (d <= 0) break;
    *size -= std::clamp(d, 0, std::max(0, *size - floor));
  }
  const int mainW = W - right - hO;
  outerSplit_->setSizes({mainW, right});
  workspaceSplit_->setSizes({left, std::max(1, mainW - left - hW)});
  // Bottom: clip list | time rows | mix.
  const int hT = timelineAreaSplit_->handleWidth(), bw = mainW - 12;
  const int needCl = paneNeed("clips"), needT = paneNeed("timeline"), needM = paneNeed("mix");
  int clips = grow(needCl, 0.12, bw, 1.4), mix = grow(needM, 0.15, bw, 1.4);
  for (auto [size, floor] : {std::pair<int*, int>{&mix, needM}, {&clips, needCl}}) {
    const int over = clips + mix + 2 * hT + needT - bw;
    if (over <= 0) break;
    *size -= std::clamp(over, 0, std::max(0, *size - floor));
  }
  timelineAreaSplit_->setSizes({clips, std::max(1, bw - clips - mix - 2 * hT), mix});
  // Heights: the timeline takes ~42 % (at least its rows and a few dopesheet/graph rows), the
  // canvas the rest; the right column gives the look pages about half.
  const int u = int(std::lround(ui_ * 100));
  const int hM = mainSplit_->handleWidth();
  const int bottomMin = bottomTabs_->minimumSizeHint().height();
  const int topPref = canvasPane_->minimumSizeHint().height() + 150 * u / 100;
  int bottom = std::max(int(H * 0.42), bottomMin + 140 * u / 100);
  bottom = std::max(bottomMin, std::min(bottom, H - hM - topPref));
  mainSplit_->setSizes({std::max(1, H - hM - bottom), bottom});
  const int hR = rightSplit_->handleWidth();
  QWidget *props = rightSplit_->widget(0), *looks = rightSplit_->widget(1), *diag = rightSplit_->widget(2);
  const int diagH = std::max(diag->minimumSizeHint().height(), std::min(int(H * 0.08), 160 * u / 100));
  int looksH = std::max(looks->minimumSizeHint().height(), int(H * 0.5));
  int propsH = H - 2 * hR - diagH - looksH;
  if (propsH < props->minimumSizeHint().height()) { looksH = std::max(looks->minimumSizeHint().height(), looksH - (props->minimumSizeHint().height() - propsH)); propsH = H - 2 * hR - diagH - looksH; }
  rightSplit_->setSizes({std::max(1, propsH), looksH, diagH});
  const int leftH = H - mainSplit_->sizes().value(1) - hM;  // the left column sits above the timeline
  leftSplit_->setSizes({leftH * 50 / 100, leftH * 28 / 100, leftH * 22 / 100});
  applyingLayout_ = false;
}
void CutoutPanel::sanitizeLayout() {
  // A restored (or rescaled) user layout keeps its sizes where they show every tool; a pane that
  // would hide some behind » is widened from the canvas / time area while those keep their needs.
  if (fullView() || outerSplit_->width() < 320) return;
  applyingLayout_ = true;
  const int needL = paneNeed("left"), needC = paneNeed("canvas"), needR = paneNeed("right"), needB = paneNeed("bottom");
  const int hW = workspaceSplit_->handleWidth();
  QList<int> outer = outerSplit_->sizes(), ws = workspaceSplit_->sizes();
  if (outer.size() == 2 && ws.size() == 2) {
    const int mainMin = std::max(std::max(ws[0], needL) + hW + needC, needB);
    const int give = std::clamp(needR - outer[1], 0, std::max(0, outer[0] - mainMin));
    if (give > 0) { outer[0] -= give; outer[1] += give; outerSplit_->setSizes(outer); }
    const int widen = std::clamp(needL - ws[0], 0, std::max(0, ws[1] - needC));
    if (widen > 0) { ws[0] += widen; ws[1] -= widen; workspaceSplit_->setSizes(ws); }
  }
  QList<int> area = timelineAreaSplit_->sizes();
  if (area.size() == 3) {
    const int needT = paneNeed("timeline");
    for (int i : {0, 2}) {
      const int need = paneNeed(i == 0 ? "clips" : "mix");
      const int widen = std::clamp(need - area[i], 0, std::max(0, area[1] - needT));
      area[i] += widen;
      area[1] -= widen;
    }
    timelineAreaSplit_->setSizes(area);
  }
  applyingLayout_ = false;
}
void CutoutPanel::adjustLayout() {
  if (restoringLayout_ || applyingLayout_ || userSized_ || !outerSplit_) return;
  if (autoLayout_) applyDefaultLayout();
  else sanitizeLayout();
}
void CutoutPanel::resetLayout() {
  autoLayout_ = true;
  userSized_ = false;
  saveLayout();
  applyDefaultLayout();
  setStatus("레이아웃을 창 크기에 맞춘 기본값으로 되돌렸습니다(경계를 끌면 다시 그 크기가 기억됩니다).");
}
void CutoutPanel::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  adjustLayout();
  if (palette_ && palette_->isVisible()) showCommandPalette();
}
void CutoutPanel::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  // Widgets under a style sheet do not get the application's font changes: follow the host font
  // through the parent (dock body or workspace page), whose font change we watch.
  if (parentWidget() != watchedParent_) {
    if (watchedParent_) watchedParent_->removeEventFilter(this);
    watchedParent_ = parentWidget();
    if (watchedParent_) watchedParent_->installEventFilter(this);
  }
  // The real screen is known once shown (the constructor only saw the primary screen), and a new
  // parent (dock -> workspace page) may have another font: rescale from the current host font.
  if (!uiScaleChosen_) if (const QScreen* s = screen()) ui_ = defaultUiScale(s->size(), s->devicePixelRatio(), s->logicalDotsPerInch());
  applyUiScale();
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
  buttons->setProperty("cutoutGroup", "캐릭터");
  button("새 캐릭터", "cutoutNewCharacter", [this] { return newCharacter(); })->setToolTip("리그와 게임 연결 문서를 함께 만듭니다(편집 한 번).");
  button("PSD 가져오기…", "cutoutImportLayers", [this] { return importLayersInteractive(); })
      ->setToolTip("Photoshop PSD·PSB(또는 PNG 폴더)의 레이어로 캐릭터를 만들거나 고른 캐릭터의 그림을 갱신합니다. 미리 보기 후 편집 한 번.");
  button("이름 변경", "cutoutRenameCharacter", [this] { return renameCharacter(); })->setToolTip("고른 캐릭터의 이름(표시 이름)을 바꿉니다.");
  button("삭제", "cutoutDeleteCharacter", [this] { return deleteCharacter(); })->setToolTip("고른 캐릭터의 리그와 연결 문서를 삭제합니다(먼저 묻습니다).");
  layout->addWidget(characterList_, 1);
  layout->addWidget(buttons);
  return section("캐릭터 · 게임 연결", "cutoutSectionCharacters", body);
}

// Skins (outfits), expressions, IK and meshes: one tabbed section. Every page keeps its action
// buttons in fixed rows above/below its list (never inside a scrolling area, never behind a »
// overflow), and the page's minimum height (rows + small lists) is what the section asks of the
// right splitter, so the buttons stay reachable on every window from 1280x720 up. Only secondary
// values (IK settings, mesh grid and brush) scroll.
QWidget* CutoutPanel::buildLooks() {
  lookTabs_ = new QTabWidget;
  lookTabs_->setObjectName("cutoutLookTabs");
  lookTabs_->setDocumentMode(true);
  auto button = [this](QWidget* row, const QString& text, const char* name, const QString& tip, std::function<bool()> fn) {
    auto* b = new QPushButton(text);
    b->setObjectName(name);
    b->setToolTip(tip);
    connect(b, &QPushButton::clicked, this, [fn] { fn(); });
    addToRow(row, b);
  };
  auto list = [](const char* name, const QString& tip) {
    auto* l = new QListWidget;
    l->setObjectName(name);
    l->setToolTip(tip);
    l->setMinimumHeight(LIST_MIN_HEIGHT);
    l->setIconSize(QSize(20, 20));
    return l;
  };
  QWidget* skinPage = lookPage("cutoutSkinPage");
  skinPage->setProperty("cutoutGroup", "스킨");
  auto* skins = static_cast<QVBoxLayout*>(skinPage->layout());
  skinList_ = list("cutoutSkinList", "고른 스킨(의상)이 캔버스 미리 보기에 입혀집니다(편집기 전용, 저장되지 않음).\n클립은 그림 이름만 키로 쓰므로 스킨을 바꿔도 같은 애니메이션이 그대로 재생됩니다.");
  connect(skinList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
    if (!filling_ && item) setPreviewSkin(item->data(Qt::UserRole).toString());
  });
  QWidget* skinTools = buttonRow("cutoutSkinTools");
  button(skinTools, "새 스킨", "cutoutNewSkin", "빈 스킨을 만듭니다. 캔버스의 조각에 PNG를 끌어 놓으면 이 스킨의 그림이 됩니다.", [this] { return newSkin(); });
  button(skinTools, "복제", "cutoutDuplicateSkin", "고른 스킨을 복사합니다(색만 다른 의상 등).", [this] { return duplicateSkin(); });
  button(skinTools, "이름 변경", "cutoutRenameSkin", "스킨 ID를 바꿉니다. 게임 연결의 기본 스킨·연결 규칙도 함께 바뀝니다.", [this] { return renameSkin(); });
  button(skinTools, "삭제", "cutoutDeleteSkin", "고른 스킨을 삭제합니다(게임 연결에서 쓰는 곳도 함께 지움).", [this] { return deleteSkin(); });
  skinHint_ = new HintLabel;
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
  QWidget* attachmentTools = buttonRow("cutoutSkinAttachmentTools");
  button(attachmentTools, "그림 지정…", "cutoutSkinAssign", "고른 조각(트리·캔버스·이 목록)에 이 스킨의 그림(프로젝트 이미지 또는 PNG)을 지정합니다.", [this] { return assignSkinImageInteractive(); });
  button(attachmentTools, "빼기", "cutoutSkinRemoveAttachment", "이 스킨에서 그림을 빼면 그 조각은 기본 첨부를 보여 줍니다.", [this] { return removeSkinAttachment(); });
  skins->addWidget(skinTools);
  skins->addWidget(skinList_, 1);
  skins->addWidget(attachmentTools);
  skins->addWidget(skinAttachments_, 1);
  skins->addWidget(skinHint_);

  QWidget* expressionPage = lookPage("cutoutExpressionPage");
  expressionPage->setProperty("cutoutGroup", "표정");
  auto* expressions = static_cast<QVBoxLayout*>(expressionPage->layout());
  expressionList_ = list("cutoutExpressionList", "고른 표정을 캔버스에 미리 봅니다(편집기 전용). 표정은 기본 동작(트랙 0) 위, 레이어(눈깜빡임 등) 아래에 겹칩니다.");
  connect(expressionList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
    if (!filling_ && item) setPreviewExpression(item->data(Qt::UserRole).toString());
  });
  QWidget* expressionTools = buttonRow("cutoutExpressionTools");
  button(expressionTools, "현재 상태로 표정 만들기", "cutoutCaptureExpression",
         "애니메이션 모드에서 눈·입 그림이나 본을 바꾼 뒤(K로 키를 넣기 전에) 누르면, 바뀐 부분만 새 표정이 됩니다.\n바꾼 것이 없으면 이 시간 자세가 기본 자세와 다른 부분을 담습니다.", [this] { return captureExpression(); });
  button(expressionTools, "다시 담기", "cutoutRecaptureExpression", "고른 표정을 지금 상태로 덮어씁니다(이름 유지).", [this] { return recaptureExpression(); });
  QWidget* expressionEdit = buttonRow("cutoutExpressionEditTools");
  button(expressionEdit, "이름 변경", "cutoutRenameExpression", "표정 ID를 바꿉니다. 표정 키와 게임 연결도 함께 바뀝니다.", [this] { return renameExpression(); });
  button(expressionEdit, "삭제", "cutoutDeleteExpression", "고른 표정과 그 표정 키를 삭제합니다(게임 연결에서 쓰는 곳도 함께 지움).", [this] { return deleteExpression(); });
  expressionTable_ = new QTableWidget(0, 2);
  expressionTable_->setObjectName("cutoutExpressionTable");
  expressionTable_->setHorizontalHeaderLabels({"대상", "표정 값"});
  expressionTable_->horizontalHeader()->setStretchLastSection(true);
  expressionTable_->verticalHeader()->hide();
  expressionTable_->setMinimumHeight(LIST_MIN_HEIGHT);
  expressionTable_->setToolTip("조각: 표정이 바꿀 그림(— = 표정에 넣지 않음, 숨김 = 감춤) · 본: rotation=10, y=-3 처럼 덮을 값(비우면 뺌)");
  expressions->addWidget(expressionTools);
  expressions->addWidget(expressionEdit);
  expressions->addWidget(expressionList_, 1);
  expressions->addWidget(expressionTable_, 2);
  lookTabs_->addTab(skinPage, "스킨");
  lookTabs_->addTab(expressionPage, "표정");
  // IK and meshes share the section as more pages: one more section would not fit the 1280x720 minimum.
  lookTabs_->addTab(buildIk(), "IK");
  lookTabs_->addTab(buildMesh(), "메시");
  lookTabs_->addTab(buildEvents(), "이벤트");
  // Clipping and sequences (plugin 1.12.0): pages of the same section, like IK and meshes.
  lookTabs_->addTab(buildClipping(), "클리핑");
  lookTabs_->addTab(buildSequence(), "시퀀스");
  lookTabs_->addTab(buildPhysics(), "물리");  // plugin 1.13.0
  lookTabs_->addTab(buildTransform(), "변형 제약");  // plugin 1.16.0
  return section("스킨 · 표정 · IK · 메시 · 이벤트 · 클리핑 · 시퀀스 · 물리 · 변형", "cutoutSectionLooks", lookTabs_);
}
QWidget* CutoutPanel::lookPage(const char* name) {
  auto* page = new QWidget;
  page->setObjectName(name);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 2, 0, 0);
  layout->setSpacing(2);
  return page;
}
QScrollArea* CutoutPanel::formScroll(const char* name, QWidget* inner) {
  // Secondary values only: the page's buttons and list never live in here. No minimum: at the
  // smallest window the buttons and the list win, the values scroll (or wait for more height).
  auto* scroll = new QScrollArea;
  scroll->setObjectName(name);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->setWidget(inner);
  scroll->setMinimumHeight(1);
  return scroll;
}
// Meshes (plugin 1.4.0): convert the selected part, edit its vertices, weights and deform keys.
// The page acts on the attachment the part shows now (the preview skin's own when it has one).
// Rows that only one mode can use are hidden in the other mode (refreshMesh), which keeps the
// page's minimum at three button rows.
QWidget* CutoutPanel::buildMesh() {
  QWidget* page = lookPage("cutoutMeshPage");
  page->setProperty("cutoutGroup", "메시");
  auto* layout = static_cast<QVBoxLayout*>(page->layout());
  auto* inner = new QWidget;
  auto* formLayout = new QVBoxLayout(inner);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setSpacing(2);
  auto button = [this](QWidget* row, const QString& text, const char* name, const QString& tip, std::function<bool()> fn) {
    auto* b = new QPushButton(text);
    b->setObjectName(name);
    b->setToolTip(tip);
    connect(b, &QPushButton::clicked, this, [fn] { fn(); });
    addToRow(row, b);
    return b;
  };
  meshInfo_ = new QLabel;
  meshInfo_->setObjectName("cutoutMeshInfo");
  meshInfo_->setWordWrap(true);
  meshInfo_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  meshInfo_->setStyleSheet("color:#a7b4c2;");
  QWidget* convertRow = buttonRow("cutoutMeshConvertTools");
  meshRows_ = new QSpinBox;
  meshRows_->setObjectName("cutoutMeshRows");
  meshRows_->setRange(1, 40);
  meshRows_->setValue(4);
  meshRows_->setPrefix("세로 ");
  meshRows_->setToolTip("격자 세로 칸 수");
  meshCols_ = new QSpinBox;
  meshCols_->setObjectName("cutoutMeshCols");
  meshCols_->setRange(1, 40);
  meshCols_->setValue(4);
  meshCols_->setPrefix("가로 ");
  meshCols_->setToolTip("격자 가로 칸 수");
  meshTrim_ = new QCheckBox("투명 칸 빼기");
  meshTrim_->setObjectName("cutoutMeshTrim");
  meshTrim_->setChecked(true);
  meshTrim_->setToolTip("그림이 없는(완전히 투명한) 칸은 메시에서 뺍니다.");
  addToRow(convertRow, meshRows_);
  addToRow(convertRow, meshCols_);
  addToRow(convertRow, meshTrim_);
  QWidget* convertTools = buttonRow("cutoutMeshConvertButtons");
  convertTools->setProperty("cutoutModeRow", "setup");
  QPushButton* convert = button(convertTools, "메시로 변환", "cutoutConvertMesh", "조립 모드: 고른 조각의 그림을 격자 메시로 바꿉니다(격자 크기는 아래). 모양은 그대로이고, 정점을 옮기거나 본 웨이트를 주면 휘어집니다.",
                                [this] { return convertToMesh(meshRows_->value(), meshCols_->value(), meshTrim_->isChecked()); });
  QPushButton* drawOutline = button(convertTools, "윤곽 그리기", "cutoutMeshDrawOutline", "조립 모드: 캔버스에서 그림 둘레를 차례로 클릭해 윤곽을 그립니다. 첫 점 클릭·더블클릭·Enter로 닫으면 그 안을 삼각형으로 나눈 메시가 됩니다(오목해도 됨). Backspace = 마지막 점 지우기, Esc = 취소.",
                                    [this] { return beginOutline(); });
  QPushButton* autoOutlineButton = button(convertTools, "자동 윤곽", "cutoutMeshAutoOutline", "조립 모드: 그림의 불투명한 부분(알파 기준 이상)을 따라 윤곽을 자동으로 찾아 메시로 만듭니다. 기준·디테일·안쪽 간격은 아래 칸, 「미리보기」를 켜면 캔버스에 점선으로 보입니다.",
                                          [this] { return autoOutline(); });
  QPushButton* revert = button(convertTools, "이미지로 되돌리기", "cutoutConvertRegion", "메시를 지우고 보통 그림(사각형)으로 되돌립니다. 이 메시의 변형 키도 지워집니다.", [this] { return convertToRegion(); });
  QWidget* modeRow = buttonRow("cutoutMeshModes");
  auto mode = [&](const QString& text, const char* name, const QString& tip, MeshEdit m) {
    auto* b = new QToolButton;
    b->setObjectName(name);
    b->setText(text);
    b->setToolTip(tip);
    b->setCheckable(true);
    connect(b, &QToolButton::clicked, this, [this, b, m] { setMeshEdit(b->isChecked() ? m : MeshEdit::Off); });
    addToRow(modeRow, b);
    return b;
  };
  meshVerticesButton_ = mode("정점 편집", "cutoutMeshVertices", "캔버스에서 정점을 클릭·상자로 골라 끕니다.\n조립 모드: 메시 모양(그림은 제자리) · 애니메이션 모드: 이 시간의 변형(K로 키).", MeshEdit::Vertices);
  meshWeightsButton_ = mode("웨이트 칠하기", "cutoutMeshWeights", "조립 모드: 캔버스를 끌어 고른 본의 웨이트를 칠합니다(색: 파랑 0 → 빨강 1).", MeshEdit::Weights);
  meshBones_ = new QListWidget;
  meshBones_->setObjectName("cutoutMeshBones");
  meshBones_->setMinimumHeight(LIST_MIN_HEIGHT);
  meshBones_->setToolTip("자동 웨이트에 쓸 본 · 체크한 본들 가운데 가까운 1~4개가 정점을 움직입니다.");
  // Shape tools (plugin 1.6.0): shown in setup mode while 정점 편집 is on, in place of the weight row.
  QWidget* shapeTools = buttonRow("cutoutMeshShapeTools");
  shapeTools->setProperty("cutoutModeRow", "setup-vertices");
  auto shapeTool = [&](const QString& text, const char* name, const QString& tip, int t) {
    auto* b = new QToolButton;
    b->setObjectName(name);
    b->setText(text);
    b->setToolTip(tip);
    b->setCheckable(true);
    connect(b, &QToolButton::clicked, this, [this, t] { setMeshTool(t); });
    addToRow(shapeTools, b);
    return b;
  };
  meshSelectTool_ = shapeTool("선택·이동", "cutoutMeshToolSelect", "정점을 클릭·상자로 골라 끌어 옮깁니다.", RigCanvas::MeshSelect);
  meshAddVertexTool_ = shapeTool("정점 추가", "cutoutMeshToolAddVertex", "메시 안을 클릭하면 안쪽 정점, 윤곽 가까이를 클릭하면 윤곽 정점이 생깁니다. 웨이트·변형은 주변 정점 값을 섞어 받고, 메시는 다시 삼각분할됩니다.", RigCanvas::MeshAddVertex);
  meshAddEdgeTool_ = shapeTool("가장자리 추가", "cutoutMeshToolAddEdge", "정점에서 다른 정점으로 끌면 그 사이 가장자리를 고정합니다(하늘색). 다시 삼각분할해도 남아, 메시가 그 선을 따라 접힙니다.", RigCanvas::MeshAddEdge);
  QWidget* shapeActions = buttonRow("cutoutMeshShapeActions");
  shapeActions->setProperty("cutoutModeRow", "setup-vertices");
  QPushButton* deleteVertex = button(shapeActions, "정점 삭제", "cutoutMeshDeleteVertices", "고른 정점을 지웁니다(Delete). 윤곽 정점은 윤곽이 3개 이상 남고 안쪽 정점이 모두 그 안에 있을 때만 지울 수 있습니다.", [this] { return deleteVertices(); });
  QPushButton* removeEdge = button(shapeActions, "가장자리 삭제", "cutoutMeshRemoveEdge", "고른 두 정점 사이의 고정 가장자리(하늘색)를 풉니다.", [this] { return removeMeshEdge(); });
  QPushButton* retriangulate = button(shapeActions, "다시 삼각분할", "cutoutMeshRetriangulate", "지금 정점으로 삼각형을 다시 나눕니다(들로네, 윤곽·고정 가장자리 유지). 정점을 많이 옮겨 삼각형이 뒤집혔을 때 씁니다.", [this] { return retriangulateMesh(); });
  QWidget* weightTools = buttonRow("cutoutMeshWeightTools");
  weightTools->setProperty("cutoutModeRow", "setup-weights");
  QPushButton* autoButton = button(weightTools, "자동 웨이트", "cutoutMeshAutoWeights", "아래 목록에서 체크한 본(없으면 조각의 본과 그 부모·자식)까지의 거리로 웨이트를 자동으로 줍니다. 부드럽게 이어지고 합은 1입니다.", [this] { return autoWeights(); });
  QPushButton* unbind = button(weightTools, "웨이트 해제", "cutoutMeshUnbind", "웨이트를 지웁니다. 메시는 다시 조각의 본만 따라갑니다(모양은 그대로).", [this] { return unbindMesh(); });
  auto* form = new QFormLayout;
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  brushBone_ = new QComboBox;
  brushBone_->setObjectName("cutoutMeshBrushBone");
  brushBone_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  brushBone_->setMinimumContentsLength(6);
  connect(brushBone_, &QComboBox::activated, this, [this] { refreshMesh(); });
  auto spin = [&](const char* name, double lo, double hi, double step, double value, const QString& tip) {
    auto* s = new QDoubleSpinBox;
    s->setObjectName(name);
    s->setRange(lo, hi);
    s->setSingleStep(step);
    s->setDecimals(step < 1 ? 2 : 0);
    s->setValue(value);
    s->setToolTip(tip);
    connect(s, &QDoubleSpinBox::valueChanged, this, [this] { refreshMesh(); });
    return s;
  };
  brushWeight_ = spin("cutoutMeshBrushWeight", 0, 1, 0.1, 1, "붓이 칠하는 웨이트(1 = 이 본이 전부 움직임, 0 = 이 본의 영향 지우기)");
  brushStrength_ = spin("cutoutMeshBrushStrength", 0.05, 1, 0.05, 0.5, "한 번 칠할 때 목표 웨이트로 가까워지는 정도");
  brushRadius_ = spin("cutoutMeshBrushRadius", 4, 200, 2, 24, "붓 반지름(화면 픽셀)");
  form->addRow("칠할 본", brushBone_);
  form->addRow("웨이트", brushWeight_);
  form->addRow("세기", brushStrength_);
  form->addRow("붓 크기", brushRadius_);
  QWidget* deformTools = buttonRow("cutoutMeshDeformTools");
  deformTools->setProperty("cutoutModeRow", "animate");
  QPushButton* deformKey = button(deformTools, "변형 키", "cutoutMeshDeformKey", "애니메이션 모드: 지금 보이는 메시 변형을 이 시간의 키로 넣습니다(K와 같음).", [this] { return keyDeform(); });
  QPushButton* deformDelete = button(deformTools, "변형 키 삭제", "cutoutMeshDeleteDeform", "애니메이션 모드: 이 시간의 변형 키를 지웁니다.", [this] { return deleteDeformKey(); });
  // 자동 윤곽 settings (plugin 1.6.0): editor state, the op carries them.
  auto* outlineForm = new QFormLayout;
  outlineForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  auto outlineSpin = [&](const char* name, double lo, double hi, double step, double value, int decimals, const QString& tip) {
    auto* sp = new QDoubleSpinBox;
    sp->setObjectName(name);
    sp->setRange(lo, hi);
    sp->setSingleStep(step);
    sp->setDecimals(decimals);
    sp->setValue(value);
    sp->setToolTip(tip);
    connect(sp, &QDoubleSpinBox::valueChanged, this, [this] { refreshOutlinePreview(); });
    return sp;
  };
  outlineThreshold_ = outlineSpin("cutoutMeshOutlineThreshold", 0, 0.95, 0.05, 0.05, 2, "자동 윤곽: 이 값보다 불투명한 픽셀을 그림으로 봅니다(0 = 조금이라도 보이면).");
  outlineDetail_ = outlineSpin("cutoutMeshOutlineDetail", 0, 1, 0.1, 0.5, 2, "자동 윤곽: 1에 가까울수록 윤곽 점이 많아 그림을 꼭 맞게 따라갑니다.");
  outlineSpacing_ = outlineSpin("cutoutMeshOutlineSpacing", 0, 500, 5, 0, 0, "윤곽 그리기·자동 윤곽: 안쪽에 이 간격(그림 px)으로 정점을 채웁니다. 0 = 윤곽 정점만.");
  outlineSpacing_->setSpecialValueText("없음");
  outlinePreview_ = new QCheckBox("자동 윤곽 미리보기");
  outlinePreview_->setObjectName("cutoutMeshOutlinePreview");
  outlinePreview_->setToolTip("자동 윤곽이 만들 윤곽을 캔버스에 점선(분홍)으로 보여 줍니다.");
  connect(outlinePreview_, &QCheckBox::toggled, this, [this] { refreshOutlinePreview(); });
  outlineForm->addRow("알파 기준", outlineThreshold_);
  outlineForm->addRow("윤곽 디테일", outlineDetail_);
  outlineForm->addRow("안쪽 간격", outlineSpacing_);
  outlineForm->addRow(outlinePreview_);
  // Fixed: the mode's action rows and the bone list. Scrolling: state text, grid size, brush.
  layout->addWidget(convertTools);
  layout->addWidget(modeRow);
  layout->addWidget(shapeTools);
  layout->addWidget(shapeActions);
  layout->addWidget(weightTools);
  layout->addWidget(deformTools);
  layout->addWidget(meshBones_, 2);
  formLayout->addWidget(meshInfo_);
  formLayout->addWidget(convertRow);
  formLayout->addLayout(outlineForm);
  formLayout->addLayout(form);
  formLayout->addStretch();
  layout->addWidget(formScroll("cutoutMeshFormScroll", inner), 3);
  meshModeRows_ = {convertTools, shapeTools, shapeActions, weightTools, deformTools};
  meshSetupTools_ = {meshRows_, meshCols_, meshTrim_, convert, drawOutline, autoOutlineButton, outlineThreshold_, outlineDetail_, outlineSpacing_, outlinePreview_};
  meshTools_ = {revert, meshVerticesButton_, meshWeightsButton_, meshBones_, autoButton, unbind, brushBone_, brushWeight_, brushStrength_, brushRadius_,
                meshSelectTool_, meshAddVertexTool_, meshAddEdgeTool_, deleteVertex, removeEdge, retriangulate};
  meshAnimateTools_ = {deformKey, deformDelete};
  return page;
}

// Clip events (plugin 1.8.0): the rig's event definitions (default payload, optional sound), the
// key tools of the dopesheet's 이벤트 row and the preview log, as the "이벤트" page of the look section.
// Events are presentation only: controller bindings react to game signals, never to clip events.
QWidget* CutoutPanel::buildEvents() {
  QWidget* page = lookPage("cutoutEventPage");
  page->setProperty("cutoutGroup", "이벤트");
  auto* layout = static_cast<QVBoxLayout*>(page->layout());
  auto* inner = new QWidget;
  auto* formLayout = new QVBoxLayout(inner);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setSpacing(2);
  auto button = [this](QWidget* row, const QString& text, const char* name, const QString& tip, std::function<bool()> fn) {
    auto* b = new QPushButton(text);
    b->setObjectName(name);
    b->setToolTip(tip);
    connect(b, &QPushButton::clicked, this, [fn] { fn(); });
    addToRow(row, b);
    return b;
  };
  eventList_ = new QListWidget;
  eventList_->setObjectName("cutoutEventList");
  eventList_->setMinimumHeight(LIST_MIN_HEIGHT);
  eventList_->setToolTip("이벤트 정의 · 키의 기본 값(int·float·string)과 소리를 정합니다. 정의가 하나라도 있으면 키는 정의된 이름만 쓸 수 있습니다.\n"
                         "이벤트는 표시 전용입니다: 게임 연결 규칙은 게임 신호에만 반응하고 이벤트를 받지 않습니다.");
  connect(eventList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
    if (!filling_ && item) selectEvent(item->data(Qt::UserRole).toString());
  });
  QWidget* tools = buttonRow("cutoutEventTools");
  button(tools, "새 이벤트", "cutoutNewEvent", "이벤트 정의를 만듭니다(발소리, 효과, 타격 판정 표시 등). 첫 정의는 키가 이미 쓰는 이름도 함께 정의합니다.", [this] { return newEvent(); });
  button(tools, "이름 변경", "cutoutRenameEvent", "이벤트 이름을 바꿉니다. 모든 클립의 그 이벤트 키도 함께 바뀝니다.", [this] { return renameEvent(); });
  button(tools, "삭제", "cutoutDeleteEvent", "이벤트 정의를 삭제합니다(그 이벤트 키도 함께, 먼저 묻습니다).", [this] { return deleteEvent(); });
  eventKeyTools_ = buttonRow("cutoutEventKeyTools");
  eventKeyTools_->setProperty("cutoutModeRow", "animate");
  button(eventKeyTools_, "이벤트 키", "cutoutEventKeyAdd", "고른 이벤트를 재생 헤드 시간에 키로 넣습니다(도프시트 「이벤트」 행: 오른쪽 클릭 · 빈 곳 더블클릭도 됩니다).", [this] { return addEventKey(); });
  button(eventKeyTools_, "키 편집…", "cutoutEventKeyEdit", "도프시트에서 고른 이벤트 키의 이름과 값(int·float·string 덮어쓰기)을 바꿉니다(이벤트 키 더블클릭).", [this] { return editEventKeyInteractive(); });
  auto* form = new QFormLayout;
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  eventInt_ = new QSpinBox;
  eventInt_->setObjectName("cutoutEventInt");
  eventInt_->setRange(-2147483647, 2147483647);
  eventInt_->setKeyboardTracking(false);
  connect(eventInt_, &QSpinBox::editingFinished, this, [this] {
    if (!filling_ && eventInt_->value() != eventInt_->property("shown").toInt()) setEventField("int", eventInt_->value());
  });
  eventFloat_ = new QDoubleSpinBox;
  eventFloat_->setObjectName("cutoutEventFloat");
  eventFloat_->setRange(-1e6, 1e6);
  eventFloat_->setDecimals(4);
  eventFloat_->setKeyboardTracking(false);
  connect(eventFloat_, &QDoubleSpinBox::editingFinished, this, [this] {
    if (!filling_ && std::fabs(eventFloat_->value() - eventFloat_->property("shown").toDouble()) > 5e-5) setEventField("float", eventFloat_->value());
  });
  eventString_ = new QLineEdit;
  eventString_->setObjectName("cutoutEventString");
  eventString_->setMaxLength(1000);
  connect(eventString_, &QLineEdit::editingFinished, this, [this] {
    if (!filling_ && eventString_->text() != eventString_->property("shown").toString()) setEventField("string", ss(eventString_->text()));
  });
  eventAudio_ = new QComboBox;
  eventAudio_->setObjectName("cutoutEventAudio");
  eventAudio_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  eventAudio_->setMinimumContentsLength(6);
  eventAudio_->setToolTip("이벤트가 일어날 때 플레이어가 재생할 수 있는 소리(프로젝트 오디오 에셋). 재생은 플레이어(호스트)의 몫입니다.");
  connect(eventAudio_, &QComboBox::activated, this, [this] { if (!filling_) setEventField("audio", ss(eventAudio_->currentData().toString())); });
  eventVolume_ = new QDoubleSpinBox;
  eventVolume_->setObjectName("cutoutEventVolume");
  eventVolume_->setRange(0, 1);
  eventVolume_->setDecimals(2);
  eventVolume_->setSingleStep(0.05);
  eventVolume_->setKeyboardTracking(false);
  connect(eventVolume_, &QDoubleSpinBox::editingFinished, this, [this] {
    if (!filling_ && std::fabs(eventVolume_->value() - eventVolume_->property("shown").toDouble()) > 5e-3) setEventField("volume", eventVolume_->value());
  });
  eventBalance_ = new QDoubleSpinBox;
  eventBalance_->setObjectName("cutoutEventBalance");
  eventBalance_->setRange(-1, 1);
  eventBalance_->setDecimals(2);
  eventBalance_->setSingleStep(0.1);
  eventBalance_->setKeyboardTracking(false);
  eventBalance_->setToolTip("-1 = 왼쪽, 0 = 가운데, 1 = 오른쪽");
  connect(eventBalance_, &QDoubleSpinBox::editingFinished, this, [this] {
    if (!filling_ && std::fabs(eventBalance_->value() - eventBalance_->property("shown").toDouble()) > 5e-3) setEventField("balance", eventBalance_->value());
  });
  form->addRow("int", eventInt_);
  form->addRow("float", eventFloat_);
  form->addRow("string", eventString_);
  form->addRow("소리", eventAudio_);
  form->addRow("음량", eventVolume_);
  form->addRow("좌우", eventBalance_);
  eventInfo_ = new QLabel;
  eventInfo_->setObjectName("cutoutEventInfo");
  eventInfo_->setWordWrap(true);
  eventInfo_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  eventInfo_->setStyleSheet("color:#a7b4c2;");
  QWidget* logTools = buttonRow("cutoutEventLogTools");
  auto* logLabel = new QLabel("미리 보기 로그");
  logLabel->setToolTip("재생(▶)과 게임 연결 미리 보기에서 일어난 이벤트 · 시간 이동(스크럽)은 이벤트를 일으키지 않습니다.");
  addToRow(logTools, logLabel);
  button(logTools, "지우기", "cutoutEventLogClear", "미리 보기 로그를 비웁니다.", [this] { clearEventLog(); return true; });
  eventLogList_ = new QListWidget;
  eventLogList_->setObjectName("cutoutEventLog");
  eventLogList_->setMinimumHeight(LIST_MIN_HEIGHT);
  eventLogList_->setToolTip(logLabel->toolTip());
  // Fixed: definition tools above the list, key tools and the log below; the values scroll.
  layout->addWidget(tools);
  layout->addWidget(eventList_, 2);
  layout->addWidget(eventKeyTools_);
  formLayout->addLayout(form);
  formLayout->addWidget(eventInfo_);
  formLayout->addStretch();
  layout->addWidget(formScroll("cutoutEventFormScroll", inner), 2);
  layout->addWidget(logTools);
  layout->addWidget(eventLogList_, 1);
  eventEditors_ = {eventInt_, eventFloat_, eventString_, eventAudio_, eventVolume_, eventBalance_};
  return page;
}

// IK constraints (plugin 1.3.0): the list, chain tools and the selected constraint's values, as the
// "IK" page of the 스킨 · 표정 · IK · 메시 section.
QWidget* CutoutPanel::buildIk() {
  QWidget* page = lookPage("cutoutIkPage");
  page->setProperty("cutoutGroup", "IK");
  auto* layout = static_cast<QVBoxLayout*>(page->layout());
  auto* inner = new QWidget;
  auto* formLayout = new QVBoxLayout(inner);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setSpacing(2);
  auto button = [this](QWidget* row, const QString& text, const char* name, const QString& tip, std::function<bool()> fn) {
    auto* b = new QPushButton(text);
    b->setObjectName(name);
    b->setToolTip(tip);
    connect(b, &QPushButton::clicked, this, [fn] { fn(); });
    addToRow(row, b);
    return b;
  };
  ikList_ = new QListWidget;
  ikList_->setObjectName("cutoutIkList");
  ikList_->setMinimumHeight(LIST_MIN_HEIGHT);
  ikList_->setToolTip("IK 제약 · 캔버스의 대상 본(✛)을 끌면 팔·다리가 따라옵니다. 고른 제약의 사슬은 캔버스에 주황색으로 강조됩니다.");
  connect(ikList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
    if (!filling_ && item) selectIk(item->data(Qt::UserRole).toString());
  });
  QWidget* tools = buttonRow("cutoutIkTools");
  button(tools, "팔·다리 IK", "cutoutNewIk", "선택한 본(아래팔·정강이처럼 사슬의 끝)과 그 부모로 두 본 IK를 만들고, 손·발 자리에 대상 본(✛)을 만듭니다.\n끝점은 선택한 본의 첫 자식 본(손·발)입니다.", [this] { return newIk(false); });
  button(tools, "조준 IK", "cutoutNewAimIk", "선택한 본이 대상 본을 향하게 하는 한 본 IK를 만듭니다(눈·머리·무기·팔 조준).\n조준 축은 본이 뻗은 방향(자식 본, 없으면 붙은 그림)에서 시작하므로 만들 때 본이 돌지 않습니다.", [this] { return newIk(true); });
  button(tools, "사슬 IK", "cutoutNewChainIk", "고른 본들(Ctrl/Shift+클릭 또는 「자식 본까지 선택」, 부모 → 자식으로 한 줄)로 사슬 IK를 만듭니다.\n본 2개 = 팔·다리, 3개 이상 = 꼬리·촉수·머리채처럼 FABRIK으로 풉니다. 끝점에 대상 본(✛)이 생깁니다.", [this] { return newChainIk(); });
  button(tools, "이름 변경", "cutoutRenameIk", "IK ID를 바꿉니다. 이 IK의 키(혼합·굽힘)도 함께 바뀝니다.", [this] { return renameIk(); });
  button(tools, "삭제", "cutoutDeleteIk", "고른 IK와 그 키를 삭제합니다(다른 곳에서 쓰지 않는 대상 본도 함께).", [this] { return deleteIk(); });
  auto* form = new QFormLayout;
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  ikTarget_ = new QComboBox;
  ikTarget_->setObjectName("cutoutIkTarget");
  ikTarget_->setToolTip("사슬 끝이 닿으려는 본 · 사슬의 본이나 그 자손은 고를 수 없습니다.");
  ikTarget_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  ikTarget_->setMinimumContentsLength(6);
  connect(ikTarget_, &QComboBox::activated, this, [this] { if (!filling_) setIkTarget(ikTarget_->currentData().toString()); });
  ikMix_ = new QDoubleSpinBox;
  ikMix_->setObjectName("cutoutIkMix");
  ikMix_->setRange(0, 1);
  ikMix_->setDecimals(3);
  ikMix_->setSingleStep(0.05);
  ikMix_->setKeyboardTracking(false);
  ikMix_->setToolTip("1 = IK가 자세를 완전히 정함 · 0 = 키로 만든 회전(FK) 그대로. 애니메이션 모드에서는 이 시간의 키가 됩니다.");
  connect(ikMix_, &QDoubleSpinBox::editingFinished, this, [this] {
    if (filling_ || std::fabs(ikMix_->value() - ikMix_->property("shown").toDouble()) < 5e-4) return;  // focus-out without change
    setIkMix(ikMix_->value());
  });
  ikBend_ = new QCheckBox("양(+) 방향 · 시계 방향으로 굽힘");
  ikBend_->setObjectName("cutoutIkBend");
  ikBend_->setToolTip("팔꿈치·무릎이 굽는 쪽을 뒤집습니다(두 본 IK). 애니메이션 모드에서는 이 시간의 계단 키가 됩니다.");
  connect(ikBend_, &QCheckBox::clicked, this, [this](bool on) { if (!filling_) setIkBend(on); });
  ikOrder_ = new QSpinBox;
  ikOrder_->setObjectName("cutoutIkOrder");
  ikOrder_->setRange(-1000, 1000);
  ikOrder_->setKeyboardTracking(false);
  ikOrder_->setToolTip("작은 순서부터 풉니다. 다른 IK가 움직이는 본을 대상·사슬로 쓰는 IK는 더 큰 순서로 두세요.");
  connect(ikOrder_, &QSpinBox::editingFinished, this, [this] {
    if (!filling_ && ikOrder_->value() != ikOrder_->property("shown").toInt()) setIkOrder(ikOrder_->value());
  });
  form->addRow("대상 본 ✛", ikTarget_);
  form->addRow("혼합 (mix)", ikMix_);
  form->addRow("굽힘", ikBend_);
  form->addRow("순서", ikOrder_);
  // Stretch (plugin 1.13.0): setup flags, never keys.
  auto flag = [this](const QString& text, const char* name, const QString& key, const QString& tip) {
    auto* c = new QCheckBox(text);
    c->setObjectName(name);
    c->setToolTip(tip);
    connect(c, &QCheckBox::clicked, this, [this, key](bool on) { if (!filling_) setIkStretch(key, on); });
    return c;
  };
  ikStretch_ = flag("늘이기", "cutoutIkStretch", "stretch", "대상이 닿지 않는 곳에 있으면 사슬을 본 길이 방향으로 늘여 닿게 합니다(사슬의 모든 본에 길이가 필요).");
  ikCompress_ = flag("줄이기", "cutoutIkCompress", "compress", "조준 IK: 대상이 본 길이보다 가까우면 본을 줄여 대상에 맞춥니다.");
  ikUniform_ = flag("균일", "cutoutIkUniform", "uniform", "늘이기·줄이기에서 두께(y)도 같은 비율로 바꿉니다.");
  auto* stretchRow = new QHBoxLayout;
  stretchRow->setSpacing(6);
  stretchRow->addWidget(ikStretch_);
  stretchRow->addWidget(ikCompress_);
  stretchRow->addWidget(ikUniform_);
  stretchRow->addStretch();
  form->addRow("늘이기", stretchRow);
  // Aim axis (plugin 1.4.1): a fixed row, the fit button stays reachable with the list.
  QWidget* aimTools = buttonRow("cutoutIkAimTools");
  auto* aimLabel = new QLabel("조준 축(°)");
  aimLabel->setToolTip("조준 IK: 대상을 향하는 본의 방향 · 0 = 오른쪽(→), 90 = 아래(↓), -90 = 위(↑). 캔버스에 주황 화살표로 보입니다.");
  ikAimAxis_ = new QDoubleSpinBox;
  ikAimAxis_->setObjectName("cutoutIkAimAxis");
  ikAimAxis_->setRange(-360, 360);
  ikAimAxis_->setDecimals(1);
  ikAimAxis_->setSingleStep(5);
  ikAimAxis_->setKeyboardTracking(false);
  ikAimAxis_->setToolTip(aimLabel->toolTip() + "\n애니메이션 모드에서도 키가 아닌 IK 설정이 바뀝니다.");
  connect(ikAimAxis_, &QDoubleSpinBox::editingFinished, this, [this] {
    if (filling_ || std::fabs(ikAimAxis_->value() - ikAimAxis_->property("shown").toDouble()) < 0.05) return;
    setIkAimAxis(ikAimAxis_->value());
  });
  addToRow(aimTools, aimLabel);
  addToRow(aimTools, ikAimAxis_);
  QPushButton* fit = button(aimTools, "현재 방향으로 맞추기", "cutoutIkFitAimAxis",
                            "IK 없이 본이 지금 놓인 방향에서 대상 본(✛)이 보이는 각도를 조준 축으로 삼습니다.\nIK를 켜도 본이 돌지 않게 됩니다(애니메이션 모드: 재생 헤드 시간의 자세 기준).", [this] { return fitIkAimAxis(); });
  QWidget* keyTools = buttonRow("cutoutIkKeyTools");
  QPushButton* key = button(keyTools, "IK 키", "cutoutIkKey", "애니메이션 모드: 이 시간에 고른 IK의 혼합·굽힘 키를 넣습니다.", [this] { return keyIk(); });
  button(keyTools, "대상 선택", "cutoutIkSelectTarget", "고른 IK의 대상 본을 선택합니다(캔버스에서 끌어 옮기기).", [this] { return selectIkTarget(); });
  QPushButton* bake = button(keyTools, "IK 결과를 키로 굽기", "cutoutBakeIk", "애니메이션 모드: IK가 만든 회전을 이 시간의 회전 키로 넣고 혼합을 0으로 키합니다.\n그 뒤로는 키(FK)로 다듬습니다.", [this] { return bakeIk(); });
  ikInfo_ = new QLabel;
  ikInfo_->setObjectName("cutoutIkInfo");
  ikInfo_->setWordWrap(true);
  ikInfo_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  ikInfo_->setStyleSheet("color:#a7b4c2;");
  // Fixed: chain tools above the list, key tools below it; the constraint's values scroll.
  layout->addWidget(tools);
  layout->addWidget(ikList_, 2);
  layout->addWidget(aimTools);
  layout->addWidget(keyTools);
  formLayout->addLayout(form);
  formLayout->addWidget(ikInfo_);
  formLayout->addStretch();
  layout->addWidget(formScroll("cutoutIkFormScroll", inner), 3);
  ikEditors_ = {ikTarget_, ikMix_, ikBend_, ikOrder_, ikStretch_, ikUniform_};
  ikAnimateTools_ = {key, bake};
  ikAimTools_ = {aimLabel, ikAimAxis_, fit};
  return page;
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
  // Ctrl/Shift+click selects several bones/parts (plugin 1.7.0): the tree's selection is the
  // panel's multi-selection, its current item the primary one.
  tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  tree_->installEventFilter(this);  // Ctrl+A = all bones, Ctrl+Shift+A = children (not the workspace toggle)
  connect(tree_, &QTreeWidget::itemSelectionChanged, this, [this] {
    if (filling_) return;
    QStringList refs;
    for (auto* item : tree_->selectedItems()) refs << item->data(0, Qt::UserRole).toString();
    QTreeWidgetItem* current = tree_->currentItem();
    setSelection(refs, current && current->isSelected() ? current->data(0, Qt::UserRole).toString() : QString());
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
  // Two labelled rows (plugin 1.10.0): 본 (add / length helpers) and 조각 (parts, delete). One row
  // of five buttons was wider than the default left pane and hid 자동 길이 / 본 방향 맞추기 behind ».
  auto* treeButtons = toolRow("cutoutTreeTools");
  treeButtons->setProperty("cutoutGroup", "본");
  auto* addBoneButton = new QPushButton("본 추가");
  addBoneButton->setObjectName("cutoutAddBone");
  addBoneButton->setToolTip("고른 본(없으면 루트)의 자식 본을 하나 추가합니다(조립 모드). 캔버스에서 끌어 그리려면 본 만들기(C) 도구를 씁니다.");
  auto* addPartButton = new QPushButton("이미지 조각 추가…");
  addPartButton->setObjectName("cutoutAddPart");
  addPartButton->setToolTip("프로젝트 이미지나 PNG를 고른 본에 붙는 조각(슬롯)으로 추가합니다.");
  auto* deleteButton = new QPushButton("삭제");
  deleteButton->setObjectName("cutoutDeleteItem");
  deleteButton->setToolTip("고른 본(자식·조각 포함) 또는 조각을 삭제합니다. 쓰는 키·IK도 함께 지웁니다.");
  connect(addBoneButton, &QPushButton::clicked, this, [this] { addBone(); });
  connect(addPartButton, &QPushButton::clicked, this, [this] { addPartInteractive(); });
  connect(deleteButton, &QPushButton::clicked, this, [this] { deleteSelected(); });
  treeButtons->addWidget(groupLabel("본"));
  treeButtons->addWidget(addBoneButton);
  // Bone length helpers (plugin 1.5.0): the selected bone, or every bone when none is selected.
  auto* fitLengths = new QPushButton("자동 길이");
  fitLengths->setObjectName("cutoutFitBoneLengths");
  fitLengths->setToolTip("고른 본(없으면 모든 본)의 길이를 첫 자식 본까지의 거리로 맞춥니다. 화면의 그림은 움직이지 않습니다.");
  auto* orient = new QPushButton("본 방향 맞추기");
  orient->setObjectName("cutoutOrientBones");
  orient->setToolTip("점 본(길이 0)의 방향을 자식 본이나 그림 쪽으로 돌리고 길이를 넣습니다(Spine 식 본).\n"
                     "자식·그림·키·조준 축을 함께 보정하므로 애니메이션은 그대로입니다. 고른 본(없으면 모든 본)에 적용합니다.");
  connect(fitLengths, &QPushButton::clicked, this, [this] { fitBoneLengths(); });
  connect(orient, &QPushButton::clicked, this, [this] { orientBones(); });
  treeButtons->addWidget(fitLengths);
  treeButtons->addWidget(orient);
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
  // 조각 (parts): adding one, and its place in the draw order.
  auto* orderButtons = toolRow("cutoutDrawOrderTools");
  orderButtons->setProperty("cutoutGroup", "조각");
  orderButtons->addWidget(groupLabel("조각"));
  orderButtons->addWidget(addPartButton);
  auto* up = new QPushButton("뒤로 ↑");
  up->setObjectName("cutoutDrawUp");
  up->setToolTip("고른 조각을 한 칸 뒤로(먼저 그림) 보냅니다.");
  auto* down = new QPushButton("앞으로 ↓");
  down->setObjectName("cutoutDrawDown");
  down->setToolTip("고른 조각을 한 칸 앞으로(나중에 그림) 가져옵니다.");
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
  auto* middle = canvasPane_ = new QWidget;
  middle->setObjectName("cutoutCanvasPane");
  auto* middleLayout = new QVBoxLayout(middle);
  middleLayout->setContentsMargins(0, 0, 0, 0);
  middleLayout->setSpacing(2);
  // Row 1 (plugin 1.10.0, labelled groups): 선택 · 변형 · 본 tools, then 화면 맞춤. Row 2: the
  // transform options and the 보기 overlays (어니언 스킨, 궤적). Each row fits the default canvas
  // width from 1280 px windows up, so no tool hides behind ».
  auto* tools = toolRow("cutoutCanvasTools");
  const char* toolNames[] = {"cutoutToolSelect", "cutoutToolRotate", "cutoutToolScale", "cutoutToolPivot", "cutoutToolCreate"};
  const char* toolLabels[] = {"선택/이동 (V)", "회전 (R)", "크기 (S)", "회전 중심 (P)", "본 만들기 (C)"};
  const char* toolKeys[] = {"V", "R", "S", "P", "C"};
  const char* toolGroups[] = {"선택", "변형", "변형", "변형", "본"};
  const char* toolTips[] = {"고르고 끌어 옮깁니다. 본 몸통을 끌면 원점을 중심으로 돌고, 빈 곳을 끌면 상자 선택(Ctrl/Shift = 더하기).",
                            "끌어서 돌립니다(Shift: 15° 단위). 여러 본은 「중심」에 따라 각자 원점 / 선택 중심으로 돕니다.",
                            "끌어서 크기를 바꿉니다. 애니메이션 모드에서는 크기 키가 됩니다.",
                            "관절 위치(본 원점 또는 그림 기준점)를 찍습니다. 자식 본과 조각은 화면 자리를 지킵니다(조립 모드).",
                            "끌어서 본을 그립니다: 고른 본(없으면 루트)의 자식, 누른 곳이 원점, 끈 방향·거리가 회전·길이 · 클릭 = 길이 0 · Shift 15° · 본 원점 클릭 = 부모 고르기 (조립 모드)"};
  auto* toolGroup = new QButtonGroup(this);
  QString lastGroup;
  for (int i = 0; i < RigCanvas::TOOL_COUNT; i++) {
    if (toolGroups[i] != lastGroup) {
      if (!lastGroup.isEmpty()) tools->addSeparator();
      tools->addWidget(groupLabel(toolGroups[i]));
      lastGroup = toolGroups[i];
    }
    auto* b = new QToolButton;
    b->setObjectName(toolNames[i]);
    b->setText(toolLabels[i]);
    b->setCheckable(true);
    b->setToolTip(toolTips[i]);
    b->setProperty("cutoutShortcut", toolKeys[i]);
    b->setProperty("cutoutGroup", toolGroups[i]);
    toolGroup->addButton(b, i);
    connect(b, &QToolButton::clicked, this, [this, i] { setTool(i); });
    tools->addWidget(b);
    toolButtons_[i] = b;
  }
  selectMenuButton_ = new QToolButton;  // 선택 ▾ (selection commands): first on the second row
  toolButtons_[0]->setChecked(true);
  auto* fit = new QToolButton;
  fit->setObjectName("cutoutFitView");
  fit->setText("화면 맞춤 (F)");
  fit->setToolTip("캐릭터 전체가 캔버스에 들어오게 확대·위치를 맞춥니다.");
  fit->setProperty("cutoutShortcut", "F");
  fit->setProperty("cutoutGroup", "보기");
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
  viewTools_->addWidget(groupLabel("보기"));
  viewTools_->addWidget(onionButton_);
  viewTools_->addWidget(onionSettingsButton_);
  viewTools_->addWidget(trailButton_);
  viewTools_->addWidget(fit);
  onionButton_->setProperty("cutoutShortcut", "O");
  setGroup("보기", {onionButton_, onionSettingsButton_, trailButton_});
  canvas_ = new RigCanvas;
  canvas_->setImages(&images_);
  canvas_->onSelect = [this](const QString& ref) { selectItem(ref); };
  canvas_->originOf = [this](const QString& ref) { return manipulationOrigin(ref); };
  canvas_->onDrag = [this](const RigCanvas::Drag& d, bool finished) { onCanvasDrag(d, finished); };
  canvas_->onTool = [this](int t) { setTool(t); };
  canvas_->onKey = [this] { keySelected(); };
  canvas_->onOnion = [this] { setOnionSkin(!onion_.enabled); };
  canvas_->onDropImage = [this](const QMimeData* mime, const QString& slot) { dropImages(mime, slot); };
  canvas_->onMeshSelect = [this](const QVector<int>& vertices, bool add) { selectVertices(vertices, add); };
  canvas_->onMeshDrag = [this](const QPointF& delta, bool finished) { onMeshDrag(delta, finished); };
  // One outline tool, two uses: a mesh outline, or a clip polygon (plugin 1.12.0, outlineForClip_).
  canvas_->onMeshOutline = [this](const QPolygonF& points) { if (outlineForClip_) clippingFromCanvas(points); else outlineFromCanvas(points); };
  canvas_->onOutlineCancel = [this] { outlineSlot_.clear(); outlineForClip_ = false; setStatus("윤곽 그리기를 취소했습니다."); refreshMesh(); refreshClipping(); };
  canvas_->onMeshAddVertex = [this](const QPointF& at) { addVertexAt(at); };
  canvas_->onMeshAddEdge = [this](int a, int b) { addMeshEdge(a, b); };
  canvas_->onMeshDelete = [this] { deleteVertices(); };
  canvas_->onMeshPaint = [this](const QVector<int>& vertices, bool finished) {
    if (finished) { dragPreview_.reset(); paintWeights(vertices); return; }
    // Live heat map while the stroke goes on; the stroke is ONE edit on release.
    int si = -1;
    const cutout::Attachment* att = nullptr;
    Json op;
    if (!meshTarget(si, att, op) || brushBone_->currentData().toString().isEmpty()) return;
    op.update({{"op", "setWeights"}, {"vertices", std::vector<int>(vertices.begin(), vertices.end())}, {"bone", ss(brushBone_->currentData().toString())},
               {"weight", brushWeight_->value()}, {"strength", brushStrength_->value()}});
    try { dragPreview_ = std::make_shared<const Json>(cutout::applyEdit(working_ ? *working_ : committedRig(), op).data); } catch (const std::exception&) { return; }
    rebuildDisplay(false);
  };
  canvas_->onToggleSelect = [this](const QString& ref) { toggleSelected(ref); };
  canvas_->onBoxSelect = [this](const QStringList& refs, bool add) {
    if (add) addToSelection(refs);
    else setSelection(refs);
    setStatus(refs.isEmpty() ? QString("상자 안에 본이 없습니다.") : QString("상자 선택: 본 %1개 (선택 %2개)").arg(refs.size()).arg(selectedItems().size()));
  };
  canvas_->onSelectAll = [this] { selectAllBones(); };
  connect(fit, &QToolButton::clicked, canvas_, &RigCanvas::fitToCanvas);
  // Second row (plugin 1.7.0, editor-only options): selection commands, the pivot of a
  // multi-bone rotate/scale, setup-pose compensation and the axis space of the numeric fields.
  auto* transformTools = toolRow("cutoutTransformTools");
  transformTools->setProperty("cutoutGroup", "변형");
  selectMenuButton_->setProperty("cutoutGroup", "선택");
  selectMenuButton_->setObjectName("cutoutSelectMenu");
  selectMenuButton_->setText("선택 ▾");
  selectMenuButton_->setPopupMode(QToolButton::InstantPopup);
  selectMenuButton_->setToolTip("Ctrl/Shift+클릭 = 여러 개 고르기 · 이동 도구로 빈 곳을 끌면 상자 선택(Ctrl/Shift = 더하기)\n"
                                "캔버스·계층에서 Ctrl+A = 모든 본 · 계층에서 Ctrl+Shift+A = 고른 본의 자식 본까지\n"
                                "(캔버스의 Ctrl+Shift+A는 작업 공간 전환 그대로)");
  auto* selectMenu = new QMenu(selectMenuButton_);
  auto menuAction = [&](const char* name, const QString& text, std::function<void()> fn) {
    QAction* a = selectMenu->addAction(text);
    a->setObjectName(name);
    connect(a, &QAction::triggered, this, std::move(fn));
  };
  menuAction("cutoutSelectChildren", "자식 본까지 선택 (계층: Ctrl+Shift+A)", [this] { selectChildren(); });
  menuAction("cutoutSelectAllBones", "모든 본 선택\tCtrl+A", [this] { selectAllBones(); });
  menuAction("cutoutSelectNone", "선택 해제", [this] { selectItem(QString()); });
  selectMenu->findChild<QAction*>("cutoutSelectChildren")->setProperty("cutoutShortcut", "Ctrl+Shift+A");
  selectMenu->findChild<QAction*>("cutoutSelectAllBones")->setProperty("cutoutShortcut", "Ctrl+A");
  selectMenuButton_->setMenu(selectMenu);
  transformTools->addWidget(selectMenuButton_);
  transformTools->addSeparator();
  transformTools->addWidget(groupLabel("변형 옵션"));
  transformTools->addWidget(new QLabel("중심 "));
  pivotCombo_ = new QComboBox;
  pivotCombo_->setObjectName("cutoutPivotMode");
  pivotCombo_->addItem("각자 원점", int(Pivot::Origin));
  pivotCombo_->addItem("선택 중심", int(Pivot::Centroid));
  pivotCombo_->setToolTip("여러 본을 돌리거나 크기를 바꿀 때의 중심: 각 본이 자기 원점을 중심으로(연결된 본은 함께 휘어짐) /\n"
                          "고른 본 원점들의 가운데를 중심으로 한 덩어리처럼. 이동은 언제나 맨 위의 고른 본만 옮깁니다.");
  connect(pivotCombo_, &QComboBox::activated, this, [this] { setPivotMode(Pivot(pivotCombo_->currentData().toInt())); });
  transformTools->addWidget(pivotCombo_);
  auto toggle = [&](const char* name, const QString& text, const QString& tip, bool images) {
    auto* b = new QToolButton;
    b->setObjectName(name);
    b->setText(text);
    b->setCheckable(true);
    b->setToolTip(tip);
    connect(b, &QToolButton::toggled, this, [this, images](bool on) {
      setCompensation(images ? compensateBones_ : on, images ? on : compensateImages_);
    });
    transformTools->addWidget(b);
    return b;
  };
  compensateBonesButton_ = toggle("cutoutCompensateBones", "본 보정", "켜면 조립 모드에서 본을 옮기거나 돌려도 고르지 않은 자식 본은 화면의 자리에 그대로 있습니다(Spine 본 보정).\n"
                                  "기본 자세만 바꿉니다: 애니메이션 모드의 키에는 적용되지 않습니다.", false);
  compensateImagesButton_ = toggle("cutoutCompensateImages", "이미지 보정", "켜면 조립 모드에서 본을 옮기거나 돌려도 그 본에 붙은 그림(모든 스킨)은 화면의 자리에 그대로 있습니다(Spine 이미지 보정).\n"
                                   "기본 자세만 바꿉니다.", true);
  transformTools->addSeparator();
  transformTools->addWidget(new QLabel("숫자 축 "));
  axesCombo_ = new QComboBox;
  axesCombo_->setObjectName("cutoutAxesMode");
  axesCombo_->addItem("로컬", int(cutout::Axes::Local));
  axesCombo_->addItem("부모", int(cutout::Axes::Parent));
  axesCombo_->addItem("월드", int(cutout::Axes::World));
  axesCombo_->setToolTip("속성 칸의 X·Y·회전을 보여 주고 입력받는 공간 (저장은 언제나 로컬):\n"
                         "로컬 = 저장된 값(부모 기준) · 부모 = 부모 원점에서 부모 축 방향 px와 부모 축에서 잰 각도 · 월드 = 캐릭터 화면 좌표와 화면 각도.\n"
                         "크기는 언제나 로컬입니다.");
  connect(axesCombo_, &QComboBox::activated, this, [this] { setAxes(cutout::Axes(axesCombo_->currentData().toInt())); });
  transformTools->addWidget(axesCombo_);
  middleLayout->addWidget(tools);
  middleLayout->addWidget(transformTools);
  middleLayout->addWidget(canvas_, 1);
  middle->setMinimumWidth(240);

  // Right: numeric properties + keying (scrolls on short screens), diagnostics below.
  auto* right = new QWidget;
  right->setObjectName("cutoutProperties");
  right->setProperty("cutoutGroup", "속성 · 키");
  auto* rightLayout = new QVBoxLayout(right);
  rightLayout->setContentsMargins(0, 0, 4, 0);
  selectionLabel_ = new QLabel("선택 없음");
  selectionLabel_->setObjectName("cutoutSelectionLabel");
  selectionLabel_->setWordWrap(true);
  auto* form = new QFormLayout;
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  const char* labels[] = {"X", "Y", "회전°", "크기 X", "크기 Y", "이미지 기준점 X", "이미지 기준점 Y", "투명도", "본 길이", "기울이기 X°", "기울이기 Y°"};
  for (int i = 0; i < 11; i++) {
    const QString key = PROPS[i];
    auto* s = new QDoubleSpinBox;
    s->setObjectName("cutoutProp" + capital(key));
    s->setKeyboardTracking(false);
    if (key == "x" || key == "y") { s->setRange(-100000, 100000); s->setDecimals(2); }
    else if (key == "rotation") { s->setRange(-36000, 36000); s->setDecimals(2); }
    else if (key.startsWith("shear")) {
      s->setRange(-36000, 36000); s->setDecimals(2);
      s->setToolTip(key == "shearX" ? "기울이기 X(도, plugin 1.16.0): 본의 x축을 회전보다 이만큼 더 돌립니다(그림이 비스듬히 밀림). 애니메이션 모드에서는 키가 됩니다."
                                    : "기울이기 Y(도): 본의 y축을 회전 + 90°보다 이만큼 더 돌립니다. 애니메이션 모드에서는 키가 됩니다.");
    }
    else if (key.startsWith("scale")) { s->setRange(-100, 100); s->setDecimals(3); s->setSingleStep(0.01); }
    else if (key.startsWith("pivot")) { s->setRange(-4, 4); s->setDecimals(3); s->setSingleStep(0.05); }
    else if (key == "length") {
      s->setRange(0, 100000); s->setDecimals(2);
      s->setToolTip("본이 원점에서 +x 방향으로 뻗는 길이(px, 0 = 점 본) · 캔버스에 본 모양으로 그려지고, 두 본 IK의 끝점이 됩니다. 조립 모드 전용.");
    }
    else { s->setRange(0, 1); s->setDecimals(3); s->setSingleStep(0.05); }
    connect(s, &QDoubleSpinBox::editingFinished, this, [this, key, s] { commitProperty(key, s->value()); });
    props_[key] = s;
    form->addRow(labels[i], s);
  }
  // Bone inheritance (plugin 1.16.0): setup data, not keyable.
  inheritCombo_ = new QComboBox;
  inheritCombo_->setObjectName("cutoutPropInherit");
  for (auto [id, text] : {std::pair{"normal", "일반 (모두 상속)"}, {"onlyTranslation", "위치만"}, {"noRotationOrReflection", "회전·뒤집기 제외"},
                          {"noScale", "크기 제외"}, {"noScaleOrReflection", "크기·뒤집기 제외"}})
    inheritCombo_->addItem(text, QString(id));
  inheritCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  inheritCombo_->setMinimumContentsLength(8);
  inheritCombo_->setToolTip("상속: 부모 본에게서 무엇을 물려받는지(원점 위치는 언제나 부모를 따름).\n"
                            "일반 = 회전·크기·뒤집기 모두 · 위치만 = 화면 축 그대로 · 회전·뒤집기 제외 = 부모 크기만 · 크기 제외 = 부모 회전(뒤집기 포함)만 · 크기·뒤집기 제외 = 부모 회전만.\n"
                            "조립 모드 전용(키가 아닙니다).");
  connect(inheritCombo_, &QComboBox::activated, this, [this] { if (!filling_) setBoneInherit(inheritCombo_->currentData().toString()); });
  form->addRow("상속", inheritCombo_);
  attachmentCombo_ = new QComboBox;
  attachmentCombo_->setObjectName("cutoutPropAttachment");
  attachmentCombo_->setToolTip("애니메이션 모드에서 바꾸면 그림 교체 키(눈 깜빡임·입 모양)가 됩니다.");
  attachmentCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  attachmentCombo_->setMinimumContentsLength(8);
  connect(attachmentCombo_, &QComboBox::activated, this, [this] { commitAttachmentChoice(); });
  form->addRow("첨부 그림", attachmentCombo_);
  keyButton_ = new QPushButton("◇ 키 추가 (K)");
  keyButton_->setObjectName("cutoutKeyButton");
  keyButton_->setToolTip("애니메이션 모드: 고른 본·조각의 지금 값을 재생 헤드 시간의 키로 넣습니다.");
  keyButton_->setProperty("cutoutShortcut", "K");
  keyButton_->setMinimumHeight(30);
  connect(keyButton_, &QPushButton::clicked, this, [this] { keySelected(); });
  discardButton_ = new QPushButton("변경 취소");
  discardButton_->setObjectName("cutoutDiscardPending");
  discardButton_->setToolTip("애니메이션 모드에서 키로 넣지 않은 변경을 버리고 키 값으로 돌아갑니다.");
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
  relativeCheck_ = new QCheckBox("상대값 입력");
  relativeCheck_->setObjectName("cutoutPropRelative");
  relativeCheck_->setToolTip("켜면 본 칸에 입력한 값을 고른 본마다 지금 값에 더합니다(여러 본을 같은 만큼 옮기기·돌리기).\n"
                             "끄면 입력한 값이 고른 모든 본의 값이 됩니다. 값이 서로 다르면 칸에 —가 보입니다.");
  connect(relativeCheck_, &QCheckBox::toggled, this, [this](bool on) { setRelativeEdit(on); });
  rightLayout->addWidget(selectionLabel_);
  rightLayout->addWidget(relativeCheck_);
  rightLayout->addLayout(form);
  rightLayout->addWidget(keyButton_);
  auto* keyRow = new QHBoxLayout;
  keyRow->addWidget(autoKeyCheck_);
  keyRow->addWidget(discardButton_);
  rightLayout->addLayout(keyRow);
  rightLayout->addWidget(deltaButton_);
  rightLayout->addStretch();
  auto* propertiesScroll = propertiesScroll_ = new QScrollArea;
  propertiesScroll->setObjectName("cutoutPropertiesScroll");
  propertiesScroll->setWidgetResizable(true);
  propertiesScroll->setFrameShape(QFrame::NoFrame);
  propertiesScroll->setWidget(right);
  propertiesScroll->setMinimumHeight(60);
  diagnostics_ = new QListWidget;
  diagnostics_->setObjectName("cutoutDiagnostics");
  diagnostics_->setToolTip("검증 결과 · 클릭하면 해당 항목을 선택합니다.");
  diagnostics_->setMinimumHeight(40);  // small by default (the list scrolls), see rightSplit_
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
  // Extra height goes to properties and the look pages; diagnostics stay a small strip (its
  // list scrolls) unless the user drags it taller.
  rightSplit_->setStretchFactor(0, 2);
  rightSplit_->setStretchFactor(1, 3);
  rightSplit_->setStretchFactor(2, 0);
  rightSplit_->setSizes({400, 480, 80});
  rightPane_ = rightSplit_;
  // No explicit minimum width: the widest look-page button row (in the host's font) is the
  // minimum, so no button is ever squeezed or hidden.

  workspaceSplit_->addWidget(leftPane_);
  workspaceSplit_->addWidget(middle);
  // Only the canvas stretches: on a very wide screen the left panel keeps its width. The right
  // column is not in here: buildUi puts it beside canvas + timeline, at full height.
  workspaceSplit_->setStretchFactor(0, 0);
  workspaceSplit_->setStretchFactor(1, 1);
  workspaceSplit_->setSizes({260, 1000});
  return workspaceSplit_;
}

QWidget* CutoutPanel::buildAnimateTab() {
  auto* tab = new QWidget;
  tab->setObjectName("cutoutAnimateTab");
  auto* layout = new QHBoxLayout(tab);
  layout->setContentsMargins(2, 2, 2, 2);
  // Clips.
  auto* clipsPane = clipsPane_ = new QWidget;
  clipsPane->setObjectName("cutoutClipsPane");
  clipsPane->setProperty("cutoutGroup", "클립");
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
  add("새 클립", "cutoutNewClip", 0, 0, [this] { return newClip(); })->setToolTip("빈 클립을 만듭니다(길이 1000 ms).");
  add("복제", "cutoutDuplicateClip", 0, 1, [this] { return duplicateClip(); })->setToolTip("고른 클립을 키와 함께 복사합니다.");
  add("이름 변경", "cutoutRenameClip", 1, 0, [this] { return renameClip(); })->setToolTip("클립 ID를 바꿉니다. 게임 연결의 기본 클립·연결 규칙도 함께 바뀝니다.");
  add("삭제", "cutoutDeleteClip", 1, 1, [this] { return deleteClip(); })->setToolTip("고른 클립을 삭제합니다(게임 연결에서 쓰는 곳도 함께 지움).");
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
  auto* timelinePane = timelinePane_ = new QWidget;
  timelinePane->setObjectName("cutoutTimelinePane");
  timelinePane->setMinimumWidth(320);
  auto* timeline = new QVBoxLayout(timelinePane);
  timeline->setContentsMargins(0, 0, 0, 0);
  timeline->setSpacing(2);
  auto* transport = toolRow("cutoutTransportTools");
  transport->setProperty("cutoutGroup", "재생");
  auto* rewind = new QPushButton("⏮ 처음으로");
  rewind->setObjectName("cutoutRewind");
  rewind->setToolTip("재생 헤드를 클립 처음(0 ms)으로 옮깁니다.");
  playButton_ = new QPushButton("▶ 재생");
  playButton_->setObjectName("cutoutPlay");
  playButton_->setCheckable(true);
  playButton_->setToolTip("미리 보기 재생·정지(애니메이션 모드). 「반복」이 켜져 있으면 클립(또는 반복 구간)을 되풀이합니다.");
  playButton_->setProperty("cutoutShortcut", "Space");
  loopPreview_ = new QCheckBox("반복");
  loopPreview_->setObjectName("cutoutLoopPreview");
  loopPreview_->setChecked(true);
  loopPreview_->setToolTip("미리 보기 재생을 되풀이합니다(편집기 전용, 클립의 「반복 재생」과 별개).");
  speedCombo_ = new QComboBox;
  speedCombo_->setObjectName("cutoutSpeed");
  for (double s : {0.25, 0.5, 1.0, 1.5, 2.0}) speedCombo_->addItem(QString("%1×").arg(s), s);
  speedCombo_->setCurrentIndex(2);
  timeLabel_ = new QLabel("0 ms");
  timeLabel_->setObjectName("cutoutTimeLabel");
  connect(rewind, &QPushButton::clicked, this, [this] { setPlayhead(0); });
  connect(playButton_, &QPushButton::toggled, this, [this](bool on) { togglePlay(on); });
  transport->addWidget(groupLabel("재생"));
  transport->addWidget(rewind);
  transport->addWidget(playButton_);
  transport->addWidget(loopPreview_);
  transport->addWidget(new QLabel("속도"));
  transport->addWidget(speedCombo_);
  QAction* transportStretch = transport->addWidget(spacer());  // the view buttons go before it
  eventFlash_ = new QLabel;
  eventFlash_->setObjectName("cutoutEventFlash");
  eventFlash_->setStyleSheet("color:#f472b6;font-weight:bold;");
  eventFlash_->setToolTip("재생 중 마지막으로 일어난 이벤트 (전체 로그: 「이벤트」 페이지)");
  transport->addWidget(eventFlash_);
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
  timeRow->setProperty("cutoutGroup", "시간");
  timeField_ = msField("cutoutTimeField", 0, 600000, 0, "재생 헤드 시간(ms)을 정확히 입력합니다.");
  connect(timeField_, &QSpinBox::editingFinished, this, [this] { setPlayhead(timeField_->value()); });
  stepField_ = msField("cutoutStepSize", 1, 10000, 10, "◀ ▶ 버튼·쉼표/마침표·화살표 키가 옮기는 간격입니다.");
  timeRow->addWidget(new QLabel("시간"));
  timeRow->addWidget(timeField_);
  timeRow->addWidget(toolButton("◀", "cutoutStepBack", "이전 간격으로 (쉼표 , 또는 ←)", [this] { stepPlayhead(-1); }));
  timeRow->addWidget(toolButton("▶", "cutoutStepForward", "다음 간격으로 (마침표 . 또는 →)", [this] { stepPlayhead(1); }));
  childNamed<QWidget>(timeRow, "cutoutStepBack")->setProperty("cutoutShortcut", ",");
  childNamed<QWidget>(timeRow, "cutoutStepForward")->setProperty("cutoutShortcut", ".");
  timeRow->addWidget(new QLabel("간격"));
  timeRow->addWidget(stepField_);
  timeRow->addSeparator();
  timeRow->addWidget(toolButton("[ 구간 시작", "cutoutLoopIn", "재생 헤드를 미리 보기 반복 구간의 시작으로 (편집기 전용, 저장 안 됨)",
                                [this] { setLoopRange(playheadMs_, playheadMs_ < timeView_->loopOut() ? timeView_->loopOut() : -1); }));
  timeRow->addWidget(toolButton("구간 끝 ]", "cutoutLoopOut", "재생 헤드를 미리 보기 반복 구간의 끝으로 (눈금자의 초록 표시를 끌어도 됩니다)",
                                [this] { setLoopRange(playheadMs_ > timeView_->loopIn() ? timeView_->loopIn() : 0, playheadMs_); }));
  timeRow->addWidget(toolButton("구간 해제", "cutoutLoopClear", "미리 보기 반복 구간을 클립 전체로 되돌립니다.", [this] { clearLoopRange(); }));

  // Key tools: clipboard, pose copy, event / expression keys ("키"); offset and timing scale of
  // the selection get their own "타이밍" row (plugin 1.10.0): one row was wider than the time area
  // of a 1280 px window and hid the expression/event keys behind ».
  auto* keyRow = toolRow("cutoutKeyTools");
  keyRow->setProperty("cutoutGroup", "키");
  auto* timingRow = toolRow("cutoutTimingTools");
  timingRow->setProperty("cutoutGroup", "타이밍");
  keyRow->addWidget(groupLabel("키"));
  keyRow->addWidget(toolButton("복사", "cutoutCopyKeys", "고른 키를 복사합니다 (Ctrl+C).", [this] { copyKeys(); }));
  keyRow->addWidget(toolButton("붙여넣기", "cutoutPasteKeys", "복사한 키를 재생 헤드 시간에 붙여넣습니다 (Ctrl+V). 다른 클립에도 됩니다.\n한 대상만 복사했다면 지금 고른 본·조각에 붙습니다.", [this] { pasteKeys(); }));
  keyRow->addWidget(toolButton("복제", "cutoutDuplicateKeys", "고른 키를 재생 헤드 시간에 복제합니다 (Ctrl+D).", [this] { duplicateKeys(); }));
  childNamed<QWidget>(keyRow, "cutoutCopyKeys")->setProperty("cutoutShortcut", "Ctrl+C");
  childNamed<QWidget>(keyRow, "cutoutPasteKeys")->setProperty("cutoutShortcut", "Ctrl+V");
  childNamed<QWidget>(keyRow, "cutoutDuplicateKeys")->setProperty("cutoutShortcut", "Ctrl+D");
  keyRow->addSeparator();
  timingRow->addWidget(groupLabel("타이밍"));
  offsetField_ = msField("cutoutKeyOffset", -600000, 600000, 100, "고른 키 전부를 이만큼 옮깁니다(음수 = 앞으로).");
  timingRow->addWidget(offsetField_);
  timingRow->addWidget(toolButton("만큼 이동", "cutoutKeyOffsetApply", "고른 키를 간격을 유지한 채 옮깁니다. 다른 키와 겹치거나 클립 밖이면 거부됩니다.", [this] { offsetSelectedKeys(offsetField_->value()); }));
  scaleField_ = new QSpinBox;
  scaleField_->setRange(1, 100000);
  scaleField_->setValue(100);
  scaleField_->setSuffix(" %");
  scaleField_->setKeyboardTracking(false);
  tool(scaleField_, "cutoutKeyScale", "고른 키들의 시간 간격 배율(첫 키 기준). 선택 띠 양 끝 손잡이를 끌어도 됩니다(Alt: 재생 헤드 기준).");
  timingRow->addSeparator();
  timingRow->addWidget(scaleField_);
  timingRow->addWidget(toolButton("간격 배율", "cutoutKeyScaleApply", "고른 키들의 시간 간격을 배율만큼 늘이거나 줄입니다.", [this] { scaleSelectedKeys(scaleField_->value() / 100.0); }));
  // Time-axis zoom (dopesheet + graph) at the end of this row: the 시간 row stays short.
  timingRow->addWidget(spacer());
  timingRow->addWidget(groupLabel("시간 축"));
  timingRow->addWidget(toolButton("−", "cutoutZoomOut", "시간 축소 (Ctrl+휠)", [this] { timeView_->zoomAround(timeView_->scrollMs() + timeView_->visibleMs() / 2, 0.8); }));
  timingRow->addWidget(toolButton("+", "cutoutZoomIn", "시간 확대 (Ctrl+휠)", [this] { timeView_->zoomAround(timeView_->scrollMs() + timeView_->visibleMs() / 2, 1.25); }));
  timingRow->addWidget(toolButton("맞춤", "cutoutTimeFit", "클립 전체가 보이도록 시간 축을 맞춥니다.", [this] { timeView_->fit(); }));
  auto* copyPoseButton = new QPushButton("포즈 복사");
  copyPoseButton->setObjectName("cutoutCopyPose");
  copyPoseButton->setToolTip("고른 본들(조각은 그 본, 없으면 모든 본)의 현재 자세를 복사합니다. 팔·다리 전체는 「선택 ▾ › 자식 본까지 선택」 뒤에 복사하세요.");
  connect(copyPoseButton, &QPushButton::clicked, this, [this] { copyPose(); });
  keyRow->addWidget(copyPoseButton);
  keyRow->addWidget(toolButton("포즈 붙여넣기", "cutoutPastePose", "복사한 자세를 재생 헤드 시간에 키로 넣습니다(다른 클립에도 됩니다).", [this] { pastePose(); }));
  keyRow->addSeparator();
  keyRow->addWidget(toolButton("이벤트 키", "cutoutAddEventKey", "「이벤트」 페이지에서 고른 이벤트를 재생 헤드 시간에 키로 넣습니다(정의가 없으면 이름을 묻습니다).\n도프시트 「이벤트」 행: 오른쪽 클릭 · 빈 곳 더블클릭으로도 넣고, 끌어서 옮기고, Delete로 지웁니다.", [this] { addEventKey(); }));
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
  dopesheet_->onAddEvent = [this](int ms) { addEventKey(ms); };
  dopesheet_->onEditEvent = [this](const KeyRef& key) { editEventKeyInteractive(key); };
  dopesheet_->onRowSelected = [this](int row, bool add) {
    if (row < 0 || row >= dopesheet_->rows().size()) return;
    // The event row has no canvas item either: its definitions are the 이벤트 page.
    if (dopesheet_->rows()[row].events) { if (lookTabs_) lookTabs_->setCurrentWidget(lookTabs_->findChild<QWidget*>("cutoutEventPage")); return; }
    // The rig expression row has no canvas item: show the expression list instead.
    if (dopesheet_->rows()[row].target == "rig") { if (lookTabs_) lookTabs_->setCurrentIndex(1); return; }
    // A sequence row (plugin 1.12.0): its part, and the 시퀀스 page with the key fields.
    if (dopesheet_->rows()[row].property == "sequence") {
      selectItem(dopesheet_->rows()[row].target);
      if (lookTabs_) lookTabs_->setCurrentWidget(lookTabs_->findChild<QWidget*>("cutoutSequencePage"));
      return;
    }
    // A transform constraint row (plugin 1.16.0): its constraint in the 변형 제약 page.
    if (refKind(dopesheet_->rows()[row].target) == "transform") {
      if (lookTabs_) lookTabs_->setCurrentWidget(lookTabs_->findChild<QWidget*>("cutoutTransformPage"));
      selectTransform(refId(dopesheet_->rows()[row].target));
      selectItem(QString());
      return;
    }
    // A physics row (plugin 1.13.0): its constraint in the 물리 page, like an IK row.
    if (refKind(dopesheet_->rows()[row].target) == "physics") {
      if (lookTabs_) lookTabs_->setCurrentWidget(lookTabs_->findChild<QWidget*>("cutoutPhysicsPage"));
      selectPhysics(refId(dopesheet_->rows()[row].target));
      selectItem(QString());
      return;
    }
    // An IK row: its constraint in the IK page; no canvas selection, so the graph shows its mix curve.
    if (refKind(dopesheet_->rows()[row].target) == "ik") {
      if (lookTabs_) lookTabs_->setCurrentIndex(2);
      selectIk(refId(dopesheet_->rows()[row].target));
      selectItem(QString());
      return;
    }
    // Ctrl/Shift on a row adds its bone/part to the canvas selection (shared editor selection).
    const QString target = dopesheet_->rows()[row].target;
    if (add && !selection_.isEmpty()) { if (!selectedItems().contains(target)) addToSelection({target}); }
    else selectItem(target);
  };
  auto* scroll = new QScrollArea;
  scroll->setObjectName("cutoutDopesheetScroll");
  scroll->setWidgetResizable(true);
  scroll->setWidget(dopesheet_);
  scroll->setMinimumHeight(32);
  // Graph editor below the dopesheet: same time view, so key columns line up at any zoom.
  auto* graphPanel = new QWidget;
  graphPanel->setObjectName("cutoutGraphPanel");
  graphPanel->setProperty("cutoutGroup", "그래프");
  auto* graphLayout = new QVBoxLayout(graphPanel);
  graphLayout->setContentsMargins(0, 0, 0, 0);
  graphLayout->setSpacing(2);
  auto* presetRow = toolRow("cutoutCurveTools");
  presetRow->setProperty("cutoutGroup", "곡선");
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
  graphFitValues_ = new QPushButton("값 맞춤");
  graphFitValues_->setObjectName("cutoutGraphFitValues");
  graphFitValues_->setToolTip("보이는 채널의 값이 모두 들어오도록 세로(값) 축을 맞춥니다(값 확대·이동을 되돌림). 시간 축은 그대로입니다.");
  presetRow->addWidget(graphFitValues_);
  graph_ = new GraphEditor;
  graph_->setTimeView(timeView_);
  graph_->setToolTip("키 드래그: 위아래 = 값, 좌우 = 시간(Alt: 둘 다, Shift: 1ms) · 고른 키의 손잡이를 끌어 곡선 조절\n"
                     "Ctrl+클릭: 토글 · Shift+클릭: 추가 · 빈 곳 드래그: 상자 선택 · Ctrl+A: 모두 · 빈 곳 클릭: 시간 이동\n"
                     "Ctrl+휠: 시간 확대 · Ctrl+Shift+휠 또는 Alt+휠: 값 확대(커서 기준) · 휠: 시간 이동 · 가운데 버튼 드래그: 시간·값 이동\n"
                     "「값 맞춤」: 값 축 되돌리기 · 오른쪽 클릭: 프리셋 · 범례 클릭: 채널 숨김");
  graph_->onScrub = [this](int ms) { setPlayhead(ms); };
  graph_->onMoveKey = [this](const GraphEditor::KeyRef& k, int to, double value) { return moveGraphKey(k.target, k.property, k.timeMs, to, value); };
  graph_->onMoveKeys = [this](const QVector<KeyRef>& keys, int delta, const QHash<KeyRef, double>& values) { return moveKeys(keys, delta, values); };
  graph_->onCurve = [this](const GraphEditor::KeyRef& k, const cutout::Curve& curve) { return setKeyCurve(k.target, k.property, k.timeMs, curve); };
  graph_->onPreset = [this](const QString& preset) { applyPreset(preset); };
  graph_->onDeleteKeys = [this](const QVector<KeyRef>& keys) { return deleteKeys(keys); };
  graph_->onCommand = [this](const QString& command) { keyCommand(command); };
  graph_->onStep = [this](int steps) { stepPlayhead(steps); };
  connect(graphNormalize_, &QCheckBox::toggled, graph_, &GraphEditor::setNormalized);
  connect(graphFitValues_, &QPushButton::clicked, graph_, &GraphEditor::fitValues);
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
  timeline->addWidget(timingRow);
  timeline->addWidget(timelineSplit, 1);
  timeline->addWidget(timeScroll_);
  // The gesture list is the dopesheet tooltip: rows of help text would cost key rows.
  dopesheet_->setToolTip("클릭: 키 선택 · 겹친 키 묶음(숫자 표시)은 누를 때마다 다음 키, 더블클릭: 확대\n"
                         "Ctrl+클릭: 토글 · Shift+클릭: 추가 · 빈 곳 드래그: 상자 선택 · 행 이름 클릭: 행 전체 · Alt+클릭: 같은 시간 전체 · Ctrl+A: 모두\n"
                         "드래그: 고른 키 함께 이동(10ms, Shift 1ms) · 선택 띠 양 끝: 시간 간격 배율(Alt: 재생 헤드 기준)\n"
                         "Delete: 삭제 · Ctrl+C/V/D: 복사/붙여넣기/복제(재생 헤드) · ←/→ 또는 , .: 한 간격 이동\n"
                         "Ctrl+휠: 시간 확대 · Shift+휠/가운데 버튼 드래그: 시간 이동 · 눈금자: 재생 헤드(10ms, Shift 1ms), 초록 표시: 반복 구간");
  // Mix table.
  auto* mixPane = mixPane_ = new QWidget;
  mixPane->setObjectName("cutoutMixPane");
  mixPane->setProperty("cutoutGroup", "전환");
  mixPane->setMinimumWidth(120);  // its buttons and form set the real minimum
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
  mixAdd->setToolTip("두 클립 사이의 크로스페이드 시간을 표에 추가합니다.");
  auto* mixRemove = new QPushButton("전환 삭제");
  mixRemove->setObjectName("cutoutMixRemove");
  mixRemove->setToolTip("표에서 고른 전환을 지웁니다(기본 전환 시간을 씁니다).");
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

// ---------------------------------------------------------------- export settings (plugin 1.9.0)
static const char* SETTINGS_FILE = "plugins/cutout-animation/settings/project.json";
QWidget* CutoutPanel::buildExportTab() {
  auto* tab = new QWidget;
  tab->setObjectName("cutoutExportTab");
  tab->setProperty("cutoutGroup", "내보내기");
  auto* form = new QFormLayout(tab);
  atlasEnabled_ = new QCheckBox("내보낼 때 캐릭터 그림을 아틀라스로 묶기");
  atlasEnabled_->setObjectName("cutoutAtlasEnabled");
  atlasEnabled_->setToolTip("게임으로 내보낼 때 리그가 그리는 PNG를 큰 페이지 이미지 몇 장으로 합칩니다. 파일 수와 텍스처 전환이 줄고, 그려지는 픽셀은 같습니다.");
  atlasPageSize_ = new QComboBox;
  atlasPageSize_->setObjectName("cutoutAtlasPageSize");
  for (int size : {512, 1024, 2048, 4096}) atlasPageSize_->addItem(QString("%1 × %1 px").arg(size), size);
  atlasPageSize_->setToolTip("페이지 한 변의 최대 크기. 이보다 큰 그림은 따로 내보냅니다.");
  atlasPadding_ = new QSpinBox;
  atlasPadding_->setObjectName("cutoutAtlasPadding");
  atlasPadding_->setRange(0, 8);
  atlasPadding_->setSuffix(" px");
  atlasPadding_->setToolTip("그림마다 둘레에 가장자리 픽셀을 반복해 넣는 폭. 축소·필터링 때 이웃 그림이 비치지 않게 합니다.");
  atlasTrim_ = new QCheckBox("투명한 여백 잘라내기 (원래 크기로 그리는 조각)");
  atlasTrim_->setObjectName("cutoutAtlasTrim");
  atlasTrim_->setToolTip("완전히 투명한 테두리는 페이지에 넣지 않습니다. 위치는 그대로 유지되고 1 px 투명 테두리는 남깁니다. 메시와 크기를 바꿔 그리는 조각은 자르지 않습니다.");
  atlasSummary_ = new QLabel;
  atlasSummary_->setObjectName("cutoutAtlasSummary");
  atlasSummary_->setWordWrap(true);
  form->addRow(atlasEnabled_);
  form->addRow("페이지 크기", atlasPageSize_);
  form->addRow("여백", atlasPadding_);
  form->addRow(atlasTrim_);
  form->addRow(atlasSummary_);
  for (auto* c : {atlasEnabled_, atlasTrim_}) connect(c, &QCheckBox::toggled, this, [this] { if (!filling_) commitExportSettings(); });
  connect(atlasPageSize_, &QComboBox::activated, this, [this] { if (!filling_) commitExportSettings(); });
  connect(atlasPadding_, &QSpinBox::editingFinished, this, [this] { if (!filling_) commitExportSettings(); });
  return tab;
}
Json CutoutPanel::atlasSettings() const {
  Json out = {{"enabled", true}, {"maxPageSize", 2048}, {"padding", 2}, {"trim", true}};
  const Json stored = settings_ && settings_->data ? settings_->data->value("export", Json::object()).value("atlas", Json::object()) : Json::object();
  if (stored.is_object()) for (auto it = stored.begin(); it != stored.end(); ++it) out[it.key()] = it.value();
  return out;
}
void CutoutPanel::refreshExportSettings() {
  if (!atlasEnabled_) return;
  const Json s = atlasSettings();
  const bool keep = filling_;
  filling_ = true;
  {
    QSignalBlocker a(atlasEnabled_), b(atlasTrim_), c(atlasPageSize_), d(atlasPadding_);
    atlasEnabled_->setChecked(s.value("enabled", true));
    atlasTrim_->setChecked(s.value("trim", true));
    atlasPageSize_->setCurrentIndex(std::max(0, atlasPageSize_->findData(s.value("maxPageSize", 2048))));
    atlasPadding_->setValue(s.value("padding", 2));
    for (QWidget* w : std::initializer_list<QWidget*>{atlasTrim_, atlasPageSize_, atlasPadding_}) w->setEnabled(s.value("enabled", true));
  }
  QSet<QString> seen;  // images the rigs draw (default attachments and skins): what packing would read
  auto add = [&](const Json& list) { for (const auto& a : list) if (a.is_object() && !a.value("imageAssetId", std::string()).empty()) seen.insert(qs(a.value("imageAssetId", std::string()))); };
  for (const auto& rig : rigs_) {
    add(rig.data->value("attachments", Json::array()));
    for (const auto& skin : rig.data->value("skins", Json::array())) if (skin.is_object()) add(skin.value("attachments", Json::array()));
  }
  const int images = int(seen.size());
  atlasSummary_->setText(QString("리그가 그리는 그림 %1장이 대상입니다 (쓰이지 않는 그림은 묶지 않습니다). 설정은 %2에 저장되며, 문서가 없으면 기본값(켜짐, 2048 px, 여백 2 px, 잘라내기)을 씁니다.%3")
                             .arg(images).arg(SETTINGS_FILE).arg(settings_ ? "" : " · 아직 설정 문서가 없습니다."));
  filling_ = keep;
}
void CutoutPanel::commitExportSettings() {
  if (!host_) return;
  setAtlasSettings(atlasEnabled_->isChecked(), atlasPageSize_->currentData().toInt(), atlasPadding_->value(), atlasTrim_->isChecked());
}
bool CutoutPanel::setAtlasSettings(bool enabled, int maxPageSize, int padding, bool trim) {
  if (!host_) return false;
  if (!(maxPageSize == 512 || maxPageSize == 1024 || maxPageSize == 2048 || maxPageSize == 4096) || padding < 0 || padding > 8) {
    setStatus("아틀라스 페이지 크기는 512·1024·2048·4096, 여백은 0..8 px입니다.", true);
    refreshExportSettings();
    return false;
  }
  Json data = settings_ && settings_->data ? *settings_->data : Json{{"schemaVersion", 1}};
  data["export"]["atlas"] = {{"enabled", enabled}, {"maxPageSize", maxPageSize}, {"padding", padding}, {"trim", trim}};
  if (settings_ && settings_->data && data == *settings_->data) return false;
  QJsonObject edit;
  if (settings_) edit = replaceEdit(*settings_, toQtObject(data));
  else edit = {{"type", "create"}, {"file", SETTINGS_FILE},
               {"data", QJsonObject{{"formatVersion", "0.1.0"}, {"plugin", PLUGIN_ID}, {"type", "settings"}, {"id", "project"}, {"data", toQtObject(data)}}}};
  if (!submit({edit})) { refreshExportSettings(); return false; }
  setStatus(enabled ? QString("내보낼 때 그림을 %1 px 아틀라스 페이지로 묶습니다. 되돌리기: Ctrl+Z").arg(maxPageSize) : QString("내보낼 때 그림을 묶지 않습니다(개별 파일). 되돌리기: Ctrl+Z"));
  return true;
}

QWidget* CutoutPanel::buildControllerTab() {
  auto* tab = new QWidget;
  tab->setObjectName("cutoutControllerTab");
  tab->setProperty("cutoutGroup", "게임 연결");
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
  layerAdd->setToolTip("항상 재생할 클립(예: 눈깜빡임)을 트랙 1..15에 추가합니다.");
  auto* layerRemove = new QPushButton("레이어 삭제");
  layerRemove->setObjectName("cutoutLayerRemove");
  layerRemove->setToolTip("표에서 고른 레이어를 지웁니다.");
  connect(layerAdd, &QPushButton::clicked, this, [this] { addLayer(); });
  connect(layerRemove, &QPushButton::clicked, this, [this] { removeLayer(); });
  layerButtons->addWidget(layerAdd);
  layerButtons->addWidget(layerRemove);
  // Captions wrap (ignored width): the right column now sits beside this tab, so its minimum
  // width must stay small enough for a 1080 px wide portrait window.
  auto caption = [](const QString& text) {
    auto* label = new QLabel(text);
    label->setWordWrap(true);
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    return label;
  };
  layers->addWidget(caption("레이어 (트랙 1..15, 예: 눈깜빡임)"));
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
  bindingAdd->setToolTip("게임 신호 → 클립(·스킨·표정) 연결 규칙을 추가합니다.");
  auto* bindingRemove = new QPushButton("연결 삭제");
  bindingRemove->setObjectName("cutoutBindingRemove");
  bindingRemove->setToolTip("표에서 고른 연결 규칙을 지웁니다.");
  connect(bindingAdd, &QPushButton::clicked, this, [this] { addBinding(); });
  connect(bindingRemove, &QPushButton::clicked, this, [this] { removeBinding(); });
  bindingButtons->addWidget(bindingAdd);
  bindingButtons->addWidget(bindingRemove);
  bindingButtons->addStretch();
  bindings->addWidget(caption("게임 신호 → 클립 연결 (트랙마다 위에서 처음 맞는 연결이 실행됩니다)"));
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
  fire->setToolTip("고른 신호(와 값)를 편집기의 미리 보기 캐릭터에만 보냅니다(게임에는 영향 없음).");
  auto* reset = new QPushButton("미리 보기 초기화");
  reset->setObjectName("cutoutPreviewReset");
  reset->setToolTip("미리 보기 캐릭터를 처음 상태(기본 클립·스킨·표정)로 되돌립니다.");
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
const Json& CutoutPanel::committedRig() const { const Doc* d = rigDoc(); return d && d->data ? *d->data : emptyObject(); }
const Json& CutoutPanel::committedController() const { const Doc* d = controllerDoc(); return d && d->data ? *d->data : emptyObject(); }
CutoutPanel::JsonPtr CutoutPanel::displayShared() const {
  static const JsonPtr empty = std::make_shared<const Json>(Json::object());
  if (dragPreview_) return dragPreview_;
  if (inflight_) return inflight_;
  if (working_) return working_;
  const Doc* d = rigDoc();
  return d && d->data ? d->data : empty;
}
const Json& CutoutPanel::displayData() const { return *displayShared(); }
CutoutPanel::JsonPtr CutoutPanel::committedShared() const {
  const Doc* d = rigDoc();
  return d && d->data ? d->data : std::make_shared<const Json>(Json::object());
}
CutoutPanel::JsonPtr CutoutPanel::controllerShared() const {
  const Doc* d = controllerDoc();
  return d && d->data ? d->data : std::make_shared<const Json>(Json::object());
}
Json CutoutPanel::controllersFor(const QString& rig) const {
  Json out = Json::array();
  for (auto it = controllers_.begin(); it != controllers_.end(); ++it)
    if (qs(it->data->value("rig", std::string())) == rig) out.push_back({{"id", ss(it.key())}, {"data", *it->data}});
  return out;
}
QStringList CutoutPanel::clipIds() const {
  QStringList out;
  for (const auto& c : listOf(displayData(), "clips")) out << qs(c.value("id", std::string()));
  return out;
}
QString CutoutPanel::uniqueBoneId(const QString& base) const {
  const JsonPtr heldData = displayShared();  // shared, not copied
  const Json& data = *heldData;
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
bool CutoutPanel::commitRig(cutout::EditResult result) {
  const Doc* doc = rigDoc();
  if (!doc) { setStatus("먼저 캐릭터를 선택하거나 만드세요.", true); return false; }
  if (result.data == *doc->data && !result.extra.contains("controllers")) { working_.reset(); setStatus("바뀐 것이 없습니다."); rebuildDisplay(); return false; }
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
  // The sent data itself (moved, not copied): shown now, and adopted as the document when the
  // host's view reports the accepted edit (no reconversion of the large Qt JSON reply).
  bool valid = true;
  for (const auto& d : result.diagnostics) valid = valid && d.severity != "error";
  inflight_ = std::make_shared<const Json>(std::move(result.data));
  inflightStamp_ = ++stampCounter_;
  inflightRig_ = rigId_;
  inflightValid_ = valid;  // applyEdit just validated it: the display parse can skip that pass
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
    working_ = std::make_shared<const Json>(std::move(result.data));
    setStatus("변경됨 · K(키 추가)를 누르면 이 시간에 키로 저장됩니다. 시간을 옮기면 버려집니다.");
    rebuildDisplay();
    return true;
  }
  if (accepted) accepted(result);
  return commitRig(std::move(result));
}
bool CutoutPanel::commitController(const Json& data) {
  const Doc* doc = controllerDoc();
  if (!doc) { setStatus("이 캐릭터에 게임 연결 문서가 없습니다.", true); return false; }
  if (data == *doc->data) return false;
  auto hasError = [&](const Json& d) {
    for (const auto& x : cutout::validateController(d, rig_.get())) if (x.severity == "error") return qs(x.message + " (" + x.pointer + ")");
    return QString();
  };
  const QString error = hasError(data);
  if (!error.isEmpty() && hasError(*doc->data).isEmpty()) {
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
  const QJsonArray assets = host_->assets();
  images_.setAssets(assets);
  // Documents whose envelope is unchanged keep their parsed data and stamp (converting a large rig
  // from Qt JSON costs tens of ms); the accepted reply to our own edit is the data we sent.
  const QMap<QString, Doc> oldRigs = rigs_, oldControllers = controllers_;
  const QJsonArray oldCharacters = characters_;
  settings_.reset();
  const bool ownEdit = inflight_ && revision == lastRevision_ + 1;
  rigs_.clear();
  controllers_.clear();
  characters_ = {};
  for (const auto& v : view.value("documents").toArray()) {
    const QJsonObject doc = v.toObject();
    const QJsonObject data = doc.value("data").toObject();
    if (doc.value("kind") == "character") { characters_.append(data); continue; }
    if (doc.value("kind") != "extension" || data.value("plugin") != PLUGIN_ID) continue;
    const QString type = data.value("type").toString(), id = data.value("id").toString();
    if (type == "settings") {
      // The one settings document (plugin 1.9.0); other ids are reported by the service and ignored.
      if (id == "project") settings_ = Doc{doc.value("file").toString(), data, std::make_shared<const Json>(toJson(data.value("data").toObject())), 0};
      continue;
    }
    if (type != "rig" && type != "controller") continue;
    Doc d{doc.value("file").toString(), data, nullptr, 0};
    const QMap<QString, Doc>& old = type == "rig" ? oldRigs : oldControllers;
    const auto previous = old.find(id);
    if (previous != old.end() && previous->file == d.file && previous->envelope == data) { d.data = previous->data; d.stamp = previous->stamp; }
    else if (type == "rig" && ownEdit && id == inflightRig_) { d.data = inflight_; d.stamp = inflightStamp_; }
    else { d.data = std::make_shared<const Json>(toJson(data.value("data").toObject())); d.stamp = ++stampCounter_; }
    (type == "rig" ? rigs_ : controllers_).insert(id, d);
  }
  if (inflight_) inflight_.reset();
  else if (revision != lastRevision_ && working_) { working_.reset(); setStatus("문서가 바뀌어 키로 저장하지 않은 변경을 버렸습니다."); }
  lastRevision_ = revision;
  const bool dragged = dragPreview_ != nullptr;
  dragPreview_.reset();
  if (!rigs_.contains(rigId_)) rigId_ = rigs_.isEmpty() ? QString() : rigs_.firstKey();
  auto controllerFits = [&](const QString& id) { return controllers_.contains(id) && qs(controllers_[id].data->value("rig", std::string())) == rigId_; };
  if (!controllerFits(controllerId_)) {
    controllerId_.clear();
    for (auto it = controllers_.begin(); it != controllers_.end(); ++it) if (controllerFits(it.key())) { controllerId_ = it.key(); break; }
  }
  // Incremental refresh: the shown documents (and the lists built from them) are unchanged, as for
  // the reply to our own edit, which was already displayed from inflight_: only the host-side state
  // (diagnostics) changed. Anything else rebuilds every widget as before.
  refreshExportSettings();
  if (!dragged && !working_ && characters_ == oldCharacters && assets == shownAssets_ && displaySignature() == shownSignature_) {
    lightRefreshes_++;
    refreshDiagnostics();
    return;
  }
  rebuildDisplay();
}

QString CutoutPanel::displaySignature() const {
  // Stamp of the data on screen (0 = pending/drag data) + every document's identity and stamp.
  const quint64 shown = dragPreview_ || working_ ? 0 : inflight_ ? inflightStamp_ : rigDoc() ? rigDoc()->stamp : 0;
  QString s = QString("%1|%2|%3|%4").arg(rigId_, controllerId_).arg(shown).arg(controllerDoc() ? controllerDoc()->stamp : 0);
  for (auto it = rigs_.begin(); it != rigs_.end(); ++it) s += QString("|r:%1:%2").arg(it.key()).arg(it->stamp);
  for (auto it = controllers_.begin(); it != controllers_.end(); ++it) s += QString("|c:%1:%2").arg(it.key()).arg(it->stamp);
  return s;
}

void CutoutPanel::rebuildDisplay(bool full) {
  const JsonPtr held = displayShared();  // kept alive for this call (no 1 MB copy)
  const Json& data = *held;
  fullRefreshes_ += full;
  // The parse is reused while the same data is shown (selection, clip or mode changes rebuild the
  // widgets but not the rig); pending and drag data (stamp 0) are always parsed again.
  const quint64 stamp = dragPreview_ || working_ ? 0 : inflight_ ? inflightStamp_ : rigDoc() ? rigDoc()->stamp : 0;
  if (!stamp || stamp != parsedStamp_ || parsedRig_ != rigId_) {
    rig_.reset();
    physicsRunning_ = false;  // its bodies index the old parse; playback restarts it settled
    rigError_.clear();
    parsedStamp_ = stamp;
    parsedRig_ = rigId_;
    if (rigDoc() && data.is_object()) {
      try {
        cutout::Rig r = inflight_ && inflightValid_ && stamp == inflightStamp_ ? cutout::parseValidatedRig(ss(rigId_), data) : cutout::parseRig(ss(rigId_), data);
        // The editor samples clip time itself (scrubbing to the very end must show the end pose).
        for (auto& c : r.clips) c.loop = false;
        rig_ = std::make_shared<const cutout::Rig>(std::move(r));
      } catch (const std::exception& e) { rigError_ = qs(e.what()); }
    }
  }
  if (!rigDoc()) { rig_.reset(); rigError_.clear(); parsedStamp_ = 0; }
  const QString signature = displaySignature();
  shownSignature_ = full || signature == shownSignature_ ? signature : QString();
  if (full && host_) shownAssets_ = host_->assets();
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
    // Deleted (or undone) items leave the multi-selection; the primary falls back to the last one left.
    selected_.erase(std::remove_if(selected_.begin(), selected_.end(), [&](const QString& r) {
      return !findIn(data, refKind(r) == "bone" ? "bones" : "slots", ss(refId(r)));
    }), selected_.end());
    if (selection_.isEmpty() && !selected_.isEmpty()) selection_ = selected_.last();
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
    refreshEvents();
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
    const QString name = qs(it->data->value("name", std::string()));
    auto* item = new QTreeWidgetItem(characterList_, {name.isEmpty() || name == it.key() ? it.key() : QString("%1 (%2)").arg(name, it.key())});
    item->setData(0, Qt::UserRole, "rig:" + it.key());
    if (it.key() == rigId_) characterList_->setCurrentItem(item);
    for (auto c = controllers_.begin(); c != controllers_.end(); ++c) {
      if (qs(c->data->value("rig", std::string())) != it.key()) continue;
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
  const JsonPtr heldData = displayShared();  // shared, not copied
  const Json& data = *heldData;
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
  filling_ = false;
  syncSelectionWidgets();
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
  if (mode_ == Mode::Animate) {
    // Physics (plugin 1.13.0) only while preview playback simulates: scrubbing shows the clip pose.
    if (physicsRunning_ && playing_ && physicsPreview_) {
      cutout::Pose pose = cutout::samplePose(*rig_, rig_->clipIndex(ss(clipId_)), cutout::snapMs(playheadExact_), look);
      editorPhysics_.apply(*rig_, pose);
      return pose;
    }
    return cutout::samplePose(*rig_, rig_->clipIndex(ss(clipId_)), playheadExact_, look);
  }
  return cutout::samplePose(*rig_, -1, 0, look);
}

void CutoutPanel::refreshProperties() {
  filling_ = true;
  const QString kind = refKind(selection_), id = refId(selection_);
  const JsonPtr heldData = displayShared();  // shared, not copied
  const Json& data = *heldData;
  const cutout::Pose pose = currentPose(false);  // keyable values (a previewed expression only overlays the canvas)
  const bool animate = mode_ == Mode::Animate;
  QMap<QString, double> values;
  QMap<QString, bool> enabled;
  for (const char* p : PROPS) enabled[p] = false;
  QSignalBlocker blockCombo(attachmentCombo_);
  attachmentCombo_->clear();
  attachmentCombo_->setEnabled(false);
  QString label = "선택 없음 · 캔버스나 트리에서 본(◉) 또는 조각(▣)을 고르세요.";
  // Bone fields edit every selected bone (plugin 1.7.0): shared values in the chosen axes, "—"
  // where they differ (absolute input), 0 in relative input mode.
  const QStringList bones = selectedBones();
  const bool boneFields = rig_ && !bones.isEmpty() && (kind == "bone" || bones.size() > 1);
  QSet<QString> mixed;
  QSet<int> inherits;  // plugin 1.16.0: the selected bones' modes (several = shown blank)
  if (boneFields) {
    const auto setupWorld = animate ? std::map<std::string, cutout::Affine>() : cutout::setupWorld(data);
    QMap<QString, QVector<double>> all;
    for (const QString& b : bones) {
      const int i = rig_->boneIndex(ss(b));
      if (i < 0) continue;
      const cutout::Transform t = shownBone(i, pose, setupWorld);
      for (auto [p, v] : {std::pair{"x", t.x}, {"y", t.y}, {"rotation", t.rotation}, {"scaleX", t.scaleX}, {"scaleY", t.scaleY}, {"length", rig_->bones[size_t(i)].length},
                          {"shearX", t.shearX}, {"shearY", t.shearY}}) all[p] << v;
      inherits.insert(int(rig_->bones[size_t(i)].inherit));
    }
    for (auto it = all.begin(); it != all.end(); ++it) {
      values[it.key()] = it.value().value(0);
      const double tolerance = 0.5 * std::pow(10.0, -props_.value(it.key())->decimals());
      for (double v : it.value()) if (std::fabs(v - it.value().value(0)) >= tolerance) mixed.insert(it.key());
    }
    for (const char* p : {"x", "y", "rotation", "scaleX", "scaleY", "shearX", "shearY"}) enabled[p] = true;
    enabled["length"] = !animate;  // setup data, never keyed
    const QString when = animate ? QString(" · %1ms 자세").arg(playheadMs_) : QString(" · 기본 자세");
    label = bones.size() == 1 ? QString("본 %1%2").arg(bones[0], when) : QString("본 %1개 · %2%3").arg(bones.size()).arg(bones.join(", "), when);
    if (axes_ != cutout::Axes::Local) label += axes_ == cutout::Axes::World ? " · 월드 축" : " · 부모 축";
    if (relative_) label += " · 상대값 입력";
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
    QDoubleSpinBox* spin = it.value();
    const bool relative = relative_ && boneFields, isMixed = boneFields && !relative && mixed.contains(it.key());
    // Relative input shows the delta (0); a negative length delta must be typeable.
    if (it.key() == "length") spin->setMinimum(relative ? -100000 : 0);
    // Mixed values: the spin box shows its special text "—" at its minimum value.
    spin->setSpecialValueText(isMixed ? QString("—") : QString());
    spin->setValue(relative ? 0 : isMixed ? spin->minimum() : values.value(it.key(), 0));
    spin->setProperty("shown", spin->value());
    spin->setProperty("mixed", isMixed);
    spin->setEnabled(enabled.value(it.key()));
  }
  {
    QSignalBlocker block(inheritCombo_);
    inheritCombo_->setCurrentIndex(inherits.size() == 1 ? *inherits.begin() : -1);
    inheritCombo_->setEnabled(boneFields && !animate);
  }
  selectionLabel_->setText(label);
  (void)data;
  // Key indicator: none / keyed / modified.
  const QString state = keyState();
  keyButton_->setProperty("keyState", state);
  keyButton_->setEnabled(animate && (!selection_.isEmpty() || working_));
  keyButton_->setToolTip((selectedItems().size() > 1 ? QString("고른 %1개(본: 위치·회전·크기, 조각: 그림)를 이 시간에 모두 키로 넣습니다.").arg(selectedItems().size())
                                                   : QString("애니메이션 모드: 고른 본·조각의 지금 값을 재생 헤드 시간의 키로 넣습니다.")) + "\n단축키: K");
  keyButton_->setText(state == "modified" ? "◆ 변경됨 · 키 추가 (K)" : state == "keyed" ? "◆ 키 있음 · 다시 키 (K)" : "◇ 키 추가 (K)");
  keyButton_->setStyleSheet(state == "modified" ? "background:#f59e0b;color:#111;font-weight:bold;" : state == "keyed" ? "background:#16a34a;color:white;" : "");
  discardButton_->setEnabled(working_ != nullptr);
  filling_ = false;
  refreshIk();  // mix/bend shown at the playhead follow every pose change
  refreshMesh();
  refreshClipping();
  refreshSequence();
  refreshPhysics();
  refreshTransform();
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
  const JsonPtr heldData = displayShared();  // shared, not copied
  const Json& data = *heldData;
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
      row.property = uiProperty(t);
      row.attachment = row.property == "attachment" || row.property == "expression" || row.property == "bendPositive" || row.property == "sequence";  // all switch (step)
      row.label = row.target == "rig" ? QString("표정 (리그)")
                  : refKind(row.target) == "ik" ? QString("IK %1 %2").arg(refId(row.target), propertyLabel(row.property))
                  : refKind(row.target) == "physics" ? QString("물리 %1 %2").arg(refId(row.target), propertyLabel(row.property))
                  : refKind(row.target) == "transform" ? QString("변형 %1 %2").arg(refId(row.target), propertyLabel(row.property))
                  : row.property.startsWith("deform:") ? QString("%1 변형 (%2)").arg(refId(row.target), row.property.mid(7))
                                                        : QString("%1 %2").arg(refId(row.target), propertyLabel(row.property));
      for (const auto& k : listOf(t, "keys")) {
        row.times << int(k.value("timeMs", int64_t(0)));
        row.step << (interpOf(k) == "step");
        row.interps << qs(interpOf(k));
        row.curves << curveOf(k);
      }
      rows << row;
    }
    // Clip events (plugin 1.8.0): one row, always there so keys can be added on it. Sorted by
    // time (older documents may not be); the event name is each key's ref property.
    Dopesheet::Row events;
    events.target = "event";
    events.label = "이벤트";
    events.events = true;
    QVector<std::pair<int, QString>> keys;
    for (const auto& e : listOf(*clip, "events")) keys.push_back({int(e.value("timeMs", int64_t(0))), qs(e.value("name", std::string()))});
    std::stable_sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& [t, name] : keys) { events.times << t; events.names << name; events.step << true; events.interps << "step"; events.curves << cutout::LINEAR_CURVE; }
    rows << events;
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
  const QStringList selectedRefs = selectedItems();
  dopesheet_->setHighlightTargets(QSet<QString>(selectedRefs.begin(), selectedRefs.end()));
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
  return row >= 0 && row < dopesheet_->rows().size() && !dopesheet_->rows()[row].events ? dopesheet_->rows()[row].target : QString();
}
void CutoutPanel::refreshGraph() {
  // Numeric tracks of the selected bone/slot in the current clip, one colored channel each.
  static const QMap<QString, QColor> colors = {{"x", QColor("#f87171")}, {"y", QColor("#4ade80")}, {"rotation", QColor("#60a5fa")},
                                               {"scaleX", QColor("#fb923c")}, {"scaleY", QColor("#c084fc")}, {"alpha", QColor("#e5e7eb")}, {"mix", RigCanvas::ikColor()},
                                               {"shearX", QColor("#facc15")}, {"shearY", QColor("#2dd4bf")}, {"mixRotate", QColor("#60a5fa")}, {"mixX", QColor("#f87171")},
                                               {"mixY", QColor("#4ade80")}, {"mixScaleX", QColor("#fb923c")}, {"mixScaleY", QColor("#c084fc")}, {"mixShearY", RigCanvas::transformColor()}};
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
  const JsonPtr heldData = displayShared();  // shared, not copied
  const Json& data = *heldData;
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
  const JsonPtr heldController = controllerShared();
  const Json& c = *heldController;
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
      if (qs(it->data->value("rig", std::string())) == rigId_) controllerCombo_->addItem(it.key(), it.key());
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
  } else {
    banner_->setText("■ 조립 모드 · 본과 이미지 조각의 기본 자세를 편집합니다. 키는 만들어지지 않습니다.");
  }
  // Paddings follow the workspace UI scale (plugin 1.10.0).
  banner_->setStyleSheet(QString("background:%1;color:white;font-weight:bold;padding:%2px %3px;border-radius:3px;")
                             .arg(animate ? "#b91c1c" : "#1d4ed8").arg(int(std::lround(4 * ui_))).arg(int(std::lround(8 * ui_))));
  banner_->setProperty("mode", animate ? "animate" : "setup");
  toolButtons_[RigCanvas::Pivot]->setEnabled(!animate);
  toolButtons_[RigCanvas::Create]->setEnabled(!animate);
  for (QWidget* w : {static_cast<QWidget*>(dopesheet_), static_cast<QWidget*>(playButton_), static_cast<QWidget*>(graph_)}) w->setEnabled(animate);
  for (QPushButton* b : presetButtons_) b->setEnabled(animate);
  for (QWidget* w : timelineTools_) w->setEnabled(animate);
  if (eventKeyTools_) eventKeyTools_->setVisible(animate);  // event keys live in clips (Animate)
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
  const QStringList refs = selectedItems();
  canvas_->setMultiSelection(QSet<QString>(refs.begin(), refs.end()));
  canvas_->setMessage(rigId_.isEmpty() ? "캐릭터가 없습니다. 「새 캐릭터」를 눌러 시작하세요."
                                       : !rigError_.isEmpty() ? "리그 데이터 오류: " + rigError_ : QString());
  canvas_->setScene(rig_, currentPose());
  // Physics marks + the tether to the unsimulated tips while preview playback simulates.
  QHash<int, QPointF> rest;
  if (rig_ && physicsRunning_ && playing_ && mode_ == Mode::Animate && !showingPreview()) {
    const cutout::Pose plain = cutout::samplePose(*rig_, rig_->clipIndex(ss(clipId_)), cutout::snapMs(playheadExact_), lookFor(*rig_));
    for (const auto& k : rig_->physics) {
      double x = plain.world[size_t(k.bone)].e, y = plain.world[size_t(k.bone)].f;
      if (rig_->bones[size_t(k.bone)].length > 0) plain.world[size_t(k.bone)].apply(rig_->bones[size_t(k.bone)].length, 0, x, y);
      rest.insert(k.bone, QPointF(x, y));
    }
  }
  canvas_->setPhysicsView(physicsId_, rest);
  canvas_->setSelectedTransform(transformId_);
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
    if (qs(it->data->value("rig", std::string())) == id) { controllerId_ = it.key(); break; }
  previewKey_.clear();
  rebuildDisplay();
  canvas_->fitToCanvas();
}
void CutoutPanel::selectController(const QString& id) {
  if (!controllers_.contains(id)) return;
  const QString rig = qs(controllers_[id].data->value("rig", std::string()));
  if (rig != rigId_ && rigs_.contains(rig)) selectRig(rig);
  controllerId_ = id;
  previewKey_.clear();
  rebuildDisplay();
}
void CutoutPanel::selectItem(const QString& ref) {
  selected_.clear();
  setSelection(ref.isEmpty() ? QStringList() : QStringList{ref}, ref);
}
QStringList CutoutPanel::selectedItems() const {
  // selected_ only counts while it holds the primary item: code that assigns selection_ directly
  // (a new bone, a refused gesture restoring the old one) gets a plain single selection.
  if (selection_.isEmpty()) return {};
  return selected_.contains(selection_) ? selected_ : QStringList{selection_};
}
QStringList CutoutPanel::selectedBones(bool partsAsBones) const {
  const Json& data = displayData();
  QSet<QString> ids;
  for (const QString& ref : selectedItems()) {
    if (refKind(ref) == "bone") ids.insert(refId(ref));
    else if (partsAsBones && refKind(ref) == "slot")
      if (const Json* s = findIn(data, "slots", ss(refId(ref)))) ids.insert(qs(s->value("bone", std::string())));
  }
  QStringList out;  // rig (array) order
  for (const auto& b : listOf(data, "bones")) if (ids.contains(qs(b.value("id", std::string())))) out << qs(b.value("id", std::string()));
  return out;
}
void CutoutPanel::toggleSelected(const QString& ref) {
  QStringList refs = selectedItems();
  if (refs.contains(ref)) {
    refs.removeAll(ref);
    setSelection(refs, selection_ == ref ? refs.value(refs.size() - 1) : selection_);
  } else {
    refs << ref;
    setSelection(refs, ref);
  }
}
void CutoutPanel::addToSelection(const QStringList& add) {
  setSelection(selectedItems() + add, selection_.isEmpty() ? add.value(0) : selection_);
}
bool CutoutPanel::selectChildren() {
  const QStringList roots = selectedBones();
  if (roots.isEmpty()) { setStatus("자식 본을 고를 본을 먼저 선택하세요.", true); return false; }
  QStringList bones = roots;
  for (int i = 0; i < bones.size(); i++)
    for (const auto& b : listOf(displayData(), "bones"))
      if (b.contains("parent") && b.at("parent").is_string() && qs(b.at("parent").get<std::string>()) == bones[i] && !bones.contains(qs(b.value("id", std::string()))))
        bones << qs(b.value("id", std::string()));
  QStringList refs = selectedItems();
  int added = 0;
  for (const QString& b : bones) if (!refs.contains("bone:" + b)) { refs << "bone:" + b; added++; }
  setSelection(refs, selection_);
  setStatus(added ? QString("자식 본 %1개를 선택에 더했습니다 (본 %2개).").arg(added).arg(selectedBones().size()) : QString("고른 본에 더할 자식 본이 없습니다."));
  return added > 0;
}
bool CutoutPanel::selectAllBones() {
  QStringList refs;
  for (const auto& b : listOf(displayData(), "bones")) refs << "bone:" + qs(b.value("id", std::string()));
  if (refs.isEmpty()) return false;
  selected_.clear();
  setSelection(refs, refs.contains(selection_) ? selection_ : refs.first());
  setStatus(QString("모든 본 %1개를 선택했습니다.").arg(refs.size()));
  return true;
}
void CutoutPanel::setPivotMode(Pivot pivot) {
  pivot_ = pivot;
  if (pivotCombo_) { QSignalBlocker block(pivotCombo_); pivotCombo_->setCurrentIndex(pivotCombo_->findData(int(pivot))); }
}
void CutoutPanel::setAxes(cutout::Axes axes) {
  axes_ = axes;
  if (axesCombo_) { QSignalBlocker block(axesCombo_); axesCombo_->setCurrentIndex(axesCombo_->findData(int(axes))); }
  refreshProperties();
}
void CutoutPanel::setRelativeEdit(bool on) {
  relative_ = on;
  if (relativeCheck_) { QSignalBlocker block(relativeCheck_); relativeCheck_->setChecked(on); }
  refreshProperties();
}
void CutoutPanel::setCompensation(bool bones, bool images) {
  compensateBones_ = bones;
  compensateImages_ = images;
  for (auto [button, on] : {std::pair{compensateBonesButton_, bones}, std::pair{compensateImagesButton_, images}})
    if (button) { QSignalBlocker block(button); button->setChecked(on); }
}
void CutoutPanel::syncSelectionWidgets() {
  const QStringList refs = selectedItems();
  const QSet<QString> set(refs.begin(), refs.end());
  const bool wasFilling = filling_;
  filling_ = true;
  {
    QSignalBlocker b1(tree_), b2(drawOrder_);
    tree_->clearSelection();
    QTreeWidgetItem* current = nullptr;
    for (QTreeWidgetItemIterator it(tree_); *it; ++it) {
      const QString ref = (*it)->data(0, Qt::UserRole).toString();
      if (ref == selection_) current = *it;
      if (set.contains(ref)) (*it)->setSelected(true);
    }
    if (current) tree_->setCurrentItem(current, 0, QItemSelectionModel::NoUpdate);
    else tree_->selectionModel()->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
    tree_->viewport()->update();
    drawOrder_->setCurrentItem(nullptr);
    for (int i = 0; i < drawOrder_->count(); i++) if ("slot:" + drawOrder_->item(i)->data(Qt::UserRole).toString() == selection_) drawOrder_->setCurrentRow(i);
  }
  filling_ = wasFilling;
  if (canvas_) canvas_->setMultiSelection(set);
  if (dopesheet_) dopesheet_->setHighlightTargets(set);
}
void CutoutPanel::setSelection(const QStringList& refsIn, const QString& primaryIn) {
  QStringList refs;
  for (const QString& r : selected_) if (refsIn.contains(r) && !refs.contains(r)) refs << r;  // keep the pick order
  for (const QString& r : refsIn) if (!r.isEmpty() && !refs.contains(r)) refs << r;
  const QString ref = refs.contains(primaryIn) ? primaryIn : refs.value(refs.size() - 1);
  selected_ = refs;
  selection_ = ref;
  // Picking a chain or target bone shows its IK (the current one when it already uses the bone).
  if (refKind(ref) == "bone") {
    auto uses = [&](const Json& k) {
      bool in = k.value("target", std::string()) == ss(refId(ref));
      for (const auto& b : listOf(k, "bones")) in = in || (b.is_string() && b.get<std::string>() == ss(refId(ref)));
      return in;
    };
    const Json* current = findIn(displayData(), "ik", ss(ikId_));
    if (!current || !uses(*current))
      for (const auto& k : listOf(displayData(), "ik")) if (uses(k)) { ikId_ = qs(k.value("id", std::string())); break; }
  }
  syncSelectionWidgets();
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
    if (tool() == RigCanvas::Pivot || tool() == RigCanvas::Create) setTool(RigCanvas::Move);
  }
  rebuildDisplay();
}
int CutoutPanel::tool() const { return canvas_->tool(); }
void CutoutPanel::setTool(int t) {
  if (t == RigCanvas::Pivot && mode_ == Mode::Animate) { setStatus("회전 중심은 조립 모드에서만 바꿀 수 있습니다.", true); return; }
  if (t == RigCanvas::Create && mode_ == Mode::Animate) { setStatus("본은 조립 모드에서만 만들 수 있습니다.", true); return; }
  canvas_->setTool(t);
  for (int i = 0; i < RigCanvas::TOOL_COUNT; i++) toolButtons_[i]->setChecked(i == t);
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
  if (physicsRunning_) startPhysicsPreview();  // a jump is not a motion: settle at the new time
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
  static const QRegularExpression re("/(bones|slots|clips|attachments|skins|expressions|ik|physics)/(\\d+)");
  const auto m = re.match(pointer);
  if (!m.hasMatch()) return;
  const JsonPtr heldRig = committedShared();
  const Json& data = *heldRig;
  const size_t index = m.captured(2).toUInt();
  const Json& list = listOf(data, ss(m.captured(1)).c_str());
  if (index >= list.size()) return;
  const QString kind = m.captured(1);
  if (kind == "ik") {
    lookTabs_->setCurrentIndex(2);
    selectIk(qs(list[index].value("id", std::string())));
    return;
  }
  if (kind == "physics") {
    lookTabs_->setCurrentWidget(lookTabs_->findChild<QWidget*>("cutoutPhysicsPage"));
    selectPhysics(qs(list[index].value("id", std::string())));
    return;
  }
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
// ---------------------------------------------------------------- layered-art import (plugin 1.11.0)
LayerImportChoice CutoutPanel::layerImportDefaults(const QString& file) const {
  LayerImportChoice choice;
  choice.file = file;
  const QFileInfo info(file);
  // A PNG of a folder stands for the folder (the service reads every PNG / its layout.json).
  const bool png = info.suffix().compare("png", Qt::CaseInsensitive) == 0;
  const QString source = png ? info.dir().dirName() : info.fileName();
  const QString base = info.isDir() || png ? source : info.completeBaseName();
  if (auto it = rigs_.find(rigId_); it != rigs_.end() && it->data) {
    const Json& data = *it->data;
    const Json* record = data.contains("editor") && data["editor"].is_object() && data["editor"].contains("layerImport") ? &data["editor"]["layerImport"] : nullptr;
    // Re-import: the selected character came from that file (its import record names it).
    if (record && record->is_object() && qs(record->value("source", std::string())) == source) choice.target = rigId_;
  }
  const QString stem = qs(cutout::slugId(ss(base), "character"));
  QString id = stem;
  for (int n = 2; rigs_.contains(id) || controllers_.contains(id); n++) id = QString("%1-%2").arg(stem).arg(n);
  choice.id = id;
  return choice;
}
void CutoutPanel::requestLayerImport(const LayerImportChoice& choice, bool dryRun, std::function<void(const QJsonObject&)> done) {
  inSubmit_ = true;  // a synchronous host answers (and calls viewChanged) inside request()
  host_->request("pluginImport", {{"plugin", PLUGIN_ID}, {"op", "importLayers"}, {"payload", choice.payload()}, {"dryRun", dryRun}},
                 [done](QJsonObject reply) { if (done) done(reply); });
  inSubmit_ = false;
}
bool CutoutPanel::importLayersInteractive() {
  if (!host_ || busy()) return false;
  const QString file = askLayerFile ? askLayerFile()
                                    : QFileDialog::getOpenFileName(this, "레이어 그림 가져오기", host_->projectRoot(),
                                                                   "레이어 그림 (*.psd *.psb *.png *.json);;Photoshop (*.psd *.psb);;PNG 폴더의 그림 또는 layout.json (*.png *.json)");
  if (file.isEmpty()) return false;
  const LayerImportChoice choice = layerImportDefaults(file);
  QPointer<CutoutPanel> self(this);
  setStatus("레이어를 읽는 중…");
  requestLayerImport(choice, true, [self, choice](const QJsonObject& reply) {
    if (!self) return;
    const QString error = reply.value("error").toString();
    if (!error.isEmpty()) { self->setStatus("레이어 그림을 가져올 수 없습니다: " + error, true); return; }
    const QJsonObject report = reply.value("pluginImport").toObject().value("report").toObject();
    // Next turn of the event loop: the dialog's own requests must not nest inside this reply.
    QTimer::singleShot(0, self, [self, choice, report] {
      if (!self) return;
      LayerImportChoice chosen = choice;
      if (!self->runLayerImportDialog(chosen, report)) { if (self) self->setStatus("레이어 가져오기를 취소했습니다."); return; }
      if (self) self->importLayers(chosen);
    });
  });
  return true;
}
bool CutoutPanel::importLayers(const LayerImportChoice& choice) {
  if (!host_ || busy()) return false;
  QPointer<CutoutPanel> self(this);
  const QString target = choice.target.isEmpty() ? choice.id : choice.target;
  requestLayerImport(choice, false, [self, target](const QJsonObject& reply) {
    if (!self) return;
    const QString error = reply.value("error").toString();
    if (!error.isEmpty()) { self->setStatus("레이어 그림을 가져오지 못했습니다: " + error, true); return; }
    self->lastLayerImport_ = reply.value("pluginImport").toObject();
    QTimer::singleShot(0, self, [self, target] {
      if (!self || !self->host_) return;
      const QJsonObject report = self->lastLayerImport_.value("report").toObject();
      self->images_.setAssets(self->host_->assets());  // the new pictures
      self->selectRig(target);
      self->setMode(Mode::Setup);
      const bool update = report.value("mode").toString() == "update";
      const int warnings = report.value("warnings").toArray().size();
      self->setStatus(QString("「%1」 %2: %3%4. 되돌리기: Ctrl+Z").arg(target, update ? "갱신" : "가져옴", layerImportSummary(report),
                                                              warnings ? QString(" · 주의 %1개").arg(warnings) : QString()));
    });
  });
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
    if (qs(it->data->value("rig", std::string())) == rigId_) { edits.append(QJsonObject{{"type", "delete"}, {"file", it->file}}); names << it.key(); }
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
bool CutoutPanel::fitBoneLengths() {
  if (!rigDoc()) { setStatus("먼저 캐릭터를 만드세요.", true); return false; }
  if (mode_ == Mode::Animate) { setStatus("본 길이는 조립 모드에서 바꿉니다.", true); return false; }
  Json op = {{"op", "fitBoneLengths"}};
  if (refKind(selection_) == "bone") op["bones"] = Json::array({ss(refId(selection_))});
  size_t count = 0;
  if (!runOps({op}, false, [&](const cutout::EditResult& r) { count = r.extra.value("lengths", Json::object()).size(); })) return false;
  setStatus(QString("본 %1개의 길이를 첫 자식 본까지 맞췄습니다. 되돌리기: Ctrl+Z").arg(count));
  return true;
}
bool CutoutPanel::orientBones() {
  if (!rigDoc()) { setStatus("먼저 캐릭터를 만드세요.", true); return false; }
  if (mode_ == Mode::Animate) { setStatus("본 방향은 조립 모드에서 맞춥니다.", true); return false; }
  Json op = {{"op", "orientBones"}};
  if (refKind(selection_) == "bone") op["bones"] = Json::array({ss(refId(selection_))});
  size_t done = 0, skipped = 0;
  if (!runOps({op}, false, [&](const cutout::EditResult& r) {
        done = r.extra.value("oriented", Json::object()).size();
        skipped = r.extra.value("skipped", Json::object()).size();
      }))
    return false;
  setStatus(QString("본 %1개의 방향과 길이를 맞췄습니다%2. 애니메이션은 그대로입니다. 되돌리기: Ctrl+Z")
                .arg(done).arg(skipped ? QString(" (%1개는 그대로: 크기가 비균일하거나 가리킬 것이 없음)").arg(skipped) : QString()));
  return true;
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
    int parts = 0, iks = 0;
    for (const auto& s : listOf(displayData(), "slots")) parts += qs(s.value("bone", std::string())) == id;
    for (const auto& k : listOf(displayData(), "ik")) {
      bool uses = qs(k.value("target", std::string())) == id;
      for (const auto& b : listOf(k, "bones")) uses = uses || (b.is_string() && qs(b.get<std::string>()) == id);
      iks += uses;
    }
    if (!askConfirm("본 삭제", QString("본 「%1」를 삭제할까요?%2%3 자식 본은 위 본으로 옮겨집니다.")
                                   .arg(id, parts ? QString(" 붙어 있는 조각 %1개도 삭제됩니다.").arg(parts) : QString(),
                                        iks ? QString(" 이 본을 쓰는 IK %1개와 그 키도 삭제됩니다.").arg(iks) : QString())))
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
  // A group turn (선택 중심) measures its angle around the selected bones' centroid.
  if (const QStringList bones = dragBones("bone:" + bone); pivot_ == Pivot::Centroid && bones.size() > 1 && (kind == "bone" || mode_ == Mode::Animate)) {
    QPointF c;
    for (const QString& b : bones) if (const int i = rig_->boneIndex(ss(b)); i >= 0) c += QPointF(pose.world[size_t(i)].e, pose.world[size_t(i)].f) / double(bones.size());
    return c;
  }
  return {pose.world[size_t(bi)].e, pose.world[size_t(bi)].f};
}
QStringList CutoutPanel::dragBones(const QString& ref) const {
  // Dragging a selected bone transforms the whole (bone) selection; anything else drags alone.
  const QString bone = refId(ref);
  const QStringList bones = selectedBones(mode_ == Mode::Animate);
  return bones.contains(bone) ? bones : QStringList{bone};
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
  if (d.tool == RigCanvas::Create) {
    // A child of the selected bone (a part: its bone), else of the root (first top-level bone):
    // origin at the press, rotation/length from the drag vector in the parent's space, so the
    // tip lands exactly under the cursor whatever the parent's transform.
    if (animate || createId_.isEmpty()) return {};
    QString parent = kind == "bone" ? id : QString();
    if (kind == "slot") { const int si = rig.slotIndex(ss(id)); if (si >= 0) parent = qs(rig.slots[size_t(si)].bone); }
    if (rig.boneIndex(ss(parent)) < 0) {
      parent.clear();
      for (const auto& b : rig.bones) if (b.parentIndex < 0) { parent = qs(b.id); break; }
    }
    const int pi = rig.boneIndex(ss(parent));
    const auto inv = (pi >= 0 ? pose.world[size_t(pi)] : cutout::Affine{}).inverse();
    if (!inv) return {};
    const QPointF end = d.moved ? RigCanvas::createEnd(d) : d.start;
    double sx, sy, ex, ey;
    inv->apply(d.start.x(), d.start.y(), sx, sy);
    inv->apply(end.x(), end.y(), ex, ey);
    Json op = {{"op", "addBone"}, {"id", ss(createId_)}, {"parent", parent.isEmpty() ? Json(nullptr) : Json(ss(parent))}, {"x", sx}, {"y", sy}};
    if (const double len = std::hypot(ex - sx, ey - sy); len >= 0.01) {
      op["rotation"] = std::atan2(ey - sy, ex - sx) * 180 / 3.14159265358979323846;
      op["length"] = len;
    }
    return {op};
  }
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
  // One core transformBones per gesture (plugin 1.7.0), for one bone or the whole selection, so
  // the LLM `edit` op does exactly what a canvas drag does.
  const QStringList bones = dragBones("bone:" + id);
  const bool group = pivot_ == Pivot::Centroid && bones.size() > 1;
  const cutout::Transform cur = pose.bones[size_t(bi)];
  const int parent = rig.bones[size_t(bi)].parentIndex;
  const cutout::Affine pw = parent >= 0 ? pose.world[size_t(parent)] : cutout::Affine{};
  // The local turn of an on-screen turn: reversed under a mirrored parent only for bones that inherit
  // its mirror (plugin 1.16.0: transformBones applies the same rule).
  const cutout::Inherit inherit = parent >= 0 ? rig.bones[size_t(bi)].inherit : cutout::Inherit::Normal;
  const bool mirrors = inherit == cutout::Inherit::Normal || inherit == cutout::Inherit::NoScale;
  Json change = Json::object();
  if (d.tool == RigCanvas::Move) {
    if (delta.isNull()) return {};
    change = {{"x", delta.x()}, {"y", delta.y()}};
  } else if (d.tool == RigCanvas::Rotate) {
    // Shift snaps the grabbed bone's own rotation to 15° (a group: the turn itself).
    const double sign = mirrors && pw.a * pw.d - pw.b * pw.c < 0 ? -1 : 1, turn = group ? snap(d.angle) : sign * (snap(cur.rotation + sign * d.angle) - cur.rotation);
    if (std::fabs(turn) < 1e-9) return {};
    change = {{"rotation", turn}};
  } else {
    if (std::fabs(ratio - 1) < 1e-12 || std::fabs(ratio) < 1e-9) return {};
    change = {{"scaleX", ratio}, {"scaleY", ratio}};
  }
  std::vector<std::string> ids;
  for (const QString& b : bones) ids.push_back(ss(b));
  Json op = {{"op", "transformBones"}, {"bones", ids}, {"delta", change}};
  if (group) op["pivot"] = "centroid";
  if (animate) {
    op["clip"] = ss(clipId_);
    op["timeMs"] = playheadMs_;
  } else {
    // Compensation is a setup-pose operation (Spine): the Animate drag never sends it.
    if (compensateBones_) op["compensateBones"] = true;
    if (compensateImages_) op["compensateImages"] = true;
  }
  return {op};
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
  if (d.tool == RigCanvas::Create && mode_ == Mode::Animate) { setStatus("본은 조립 모드에서만 만들 수 있습니다.", true); if (finished) dragActive_ = false; return; }
  if (d.tool == RigCanvas::Create && createId_.isEmpty()) {
    // One id for the whole gesture (its previews and the commit), unique in the data at the press.
    QString id = "bone";
    for (int n = 2; findIn(dragBase_, "bones", ss(id)); n++) id = QString("bone-%1").arg(n);
    createId_ = id;
  }
  const std::vector<Json> ops = dragOps(d);
  if (!finished) {
    if (ops.empty()) return;
    try { dragPreview_ = std::make_shared<const Json>(cutout::applyEdit(dragBase_, batch(ops)).data); } catch (const std::exception&) { return; }
    rebuildDisplay(false);
    return;
  }
  dragActive_ = false;
  dragPreview_.reset();
  const QString created = d.tool == RigCanvas::Create ? createId_ : QString();
  createId_.clear();
  if (ops.empty()) { rebuildDisplay(false); return; }
  if (!created.isEmpty()) {
    // The new bone becomes the selection: the next drag (from its tip) draws its child.
    const QString before = selection_;
    selection_ = "bone:" + created;
    if (runOps(ops, false)) setStatus(QString("본 「%1」을 만들었습니다. 이어서 끌면 그 자식 본이 됩니다. 되돌리기: Ctrl+Z").arg(created));
    else selection_ = before;
    return;
  }
  // Remember setup deltas of a dragged (single) keyed bone so the user can rebase its keys explicitly.
  if (mode_ == Mode::Setup && ops[0].value("op", std::string()) == "transformBones" && ops[0].at("bones").size() == 1) {
    const std::string bone = ops[0].at("bones")[0].get<std::string>();
    const Json* before = findIn(dragBase_, "bones", bone);
    Json delta = Json::object();
    const bool ok = runOps(ops, false, [&](const cutout::EditResult& r) {
      const Json* after = findIn(r.data, "bones", bone);
      for (const char* p : {"x", "y", "rotation", "scaleX", "scaleY"}) {
        const double def = std::string(p).rfind("scale", 0) == 0 ? 1.0 : 0.0;
        if (after && before && std::fabs(after->value(p, def) - before->value(p, def)) > 1e-9) delta[p] = after->value(p, def) - before->value(p, def);
      }
    });
    if (ok && !delta.empty()) { lastSetupDelta_ = SetupDelta{qs(bone), delta}; refreshModeUi(); }
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
  if (property != "alpha" && property != "pivotX" && property != "pivotY" && !selectedBones().isEmpty() && (kind == "bone" || selectedBones().size() > 1)) {
    commitBoneProperty(property, value);
    return;
  }
  if (kind == "slot") {
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
cutout::Transform CutoutPanel::shownBone(int i, const cutout::Pose& pose, const std::map<std::string, cutout::Affine>& setupWorld) const {
  const auto& b = rig_->bones[size_t(i)];
  const bool animate = mode_ == Mode::Animate;
  const cutout::Transform local = animate && size_t(i) < pose.bones.size() ? pose.bones[size_t(i)] : b.setup;
  cutout::Affine parent;
  if (b.parentIndex >= 0) {
    // Setup: the stored FK setup world (what setBone `axes` converts with); Animate: the pose shown.
    if (animate) { if (size_t(b.parentIndex) < pose.world.size()) parent = pose.world[size_t(b.parentIndex)]; }
    else if (auto it = setupWorld.find(rig_->bones[size_t(b.parentIndex)].id); it != setupWorld.end()) parent = it->second;
  }
  return cutout::toAxes(local, parent, axes_, b.parentIndex >= 0 ? b.inherit : cutout::Inherit::Normal);
}
/**
 * A bone field typed for the selected bones (one edit). Setup: setBone per bone, parents first, in
 * axes_ (core converts with the current setup world, so a moved parent is taken into account) and
 * with the compensation flags. Animate: setKey per bone and channel; world/parent values convert
 * through the parent's world as it will be after the parents' own new values.
 */
void CutoutPanel::commitBoneProperty(const QString& property, double value) {
  const bool animate = mode_ == Mode::Animate;
  QStringList bones = selectedBones();
  if (!rig_ || bones.isEmpty()) return;
  // Parents first (evaluation order).
  QHash<QString, int> rank;
  for (size_t k = 0; k < rig_->evalOrder.size(); k++) rank[qs(rig_->bones[size_t(rig_->evalOrder[k])].id)] = int(k);
  std::sort(bones.begin(), bones.end(), [&](const QString& a, const QString& b) { return rank.value(a) < rank.value(b); });
  const QString single = bones.size() == 1 ? bones[0] : QString();
  std::vector<Json> ops;
  if (property == "length") {
    if (animate) { setStatus("본 길이는 조립 모드에서 바꿉니다(키가 아닙니다).", true); refreshProperties(); return; }
    for (const QString& b : bones)
      ops.push_back({{"op", "setBone"}, {"bone", ss(b)}, {"length", std::max(0.0, relative_ ? rig_->bones[size_t(rig_->boneIndex(ss(b)))].length + value : value)}});
    runOps(ops, false);
    return;
  }
  if (animate && clipId_.isEmpty()) { setStatus("키를 넣을 클립을 먼저 만드세요.", true); return; }
  auto field = [&](cutout::Transform& t) -> double& {
    return property == "x" ? t.x : property == "y" ? t.y : property == "rotation" ? t.rotation : property == "scaleX" ? t.scaleX
         : property == "shearX" ? t.shearX : property == "shearY" ? t.shearY : t.scaleY;
  };
  const bool spatial = axes_ != cutout::Axes::Local && (property == "x" || property == "y" || property == "rotation");
  const cutout::Pose pose = currentPose(false);
  const std::string axesName = axes_ == cutout::Axes::World ? "world" : axes_ == cutout::Axes::Parent ? "parent" : "local";
  if (!animate) {
    const auto setupWorld = cutout::setupWorld(displayData());
    for (const QString& b : bones) {
      cutout::Transform shown = shownBone(rig_->boneIndex(ss(b)), pose, setupWorld);
      Json op = {{"op", "setBone"}, {"bone", ss(b)}, {ss(property), relative_ ? field(shown) + value : value}};
      if (spatial) op["axes"] = axesName;
      if (compensateBones_) op["compensateBones"] = true;
      if (compensateImages_) op["compensateImages"] = true;
      ops.push_back(op);
    }
    const Json before = single.isEmpty() ? Json() : *findIn(displayData(), "bones", ss(single));
    std::optional<Json> after;
    if (runOps(ops, false, [&](const cutout::EditResult& r) { if (const Json* b = findIn(r.data, "bones", ss(single))) after = *b; }) && after) {
      // Remember the setup change of one bone so its existing keys can be rebased explicitly.
      Json delta = Json::object();
      for (const char* p : {"x", "y", "rotation", "scaleX", "scaleY"}) {
        const double def = std::string(p).rfind("scale", 0) == 0 ? 1.0 : 0.0, d = after->value(p, def) - before.value(p, def);
        if (std::fabs(d) > 1e-9) delta[p] = d;
      }
      if (!delta.empty()) { lastSetupDelta_ = SetupDelta{single, delta}; refreshModeUi(); }
    }
    return;
  }
  // Animate: walk parents first; a changed bone's subtree gets its new world matrices so the
  // conversion of a selected descendant sees where its parent will be.
  std::vector<cutout::Affine> world = pose.world;
  std::vector<char> dirty(rig_->bones.size(), 0);
  QSet<int> chosen;
  for (const QString& b : bones) chosen.insert(rig_->boneIndex(ss(b)));
  for (int i : rig_->evalOrder) {
    const int p = rig_->bones[size_t(i)].parentIndex;
    const cutout::Affine pw = p >= 0 ? world[size_t(p)] : cutout::Affine{};
    const cutout::Inherit inherit = p >= 0 ? rig_->bones[size_t(i)].inherit : cutout::Inherit::Normal;
    if (chosen.contains(i)) {
      const cutout::Transform local = pose.bones[size_t(i)];
      cutout::Transform before = cutout::toAxes(local, p >= 0 ? pose.world[size_t(p)] : cutout::Affine{}, axes_, inherit), shown = cutout::toAxes(local, pw, axes_, inherit);
      field(shown) = relative_ ? field(before) + value : value;
      const auto back = cutout::fromAxes(shown, pw, axes_, local, inherit);
      if (!back) { setStatus("부모 본의 크기가 0이라 이 축으로 바꿀 수 없습니다.", true); refreshProperties(); return; }
      const std::string target = "bone:" + rig_->bones[size_t(i)].id;
      auto key = [&](const char* prop, double v) {
        ops.push_back({{"op", "setKey"}, {"clip", ss(clipId_)}, {"target", target}, {"property", prop}, {"timeMs", playheadMs_}, {"value", v}});
      };
      if (spatial && property != "rotation") { key("x", back->x); key("y", back->y); }
      else if (spatial) key("rotation", back->rotation);
      else { cutout::Transform next = *back; key(property.toUtf8().constData(), field(next)); }
      world[size_t(i)] = cutout::boneWorld(*rig_, world, size_t(i), *back);  // shear / inherit (plugin 1.16.0)
      dirty[size_t(i)] = 1;
    } else if (p >= 0 && dirty[size_t(p)]) {
      world[size_t(i)] = cutout::boneWorld(*rig_, world, size_t(i), pose.local(size_t(i)));
      dirty[size_t(i)] = 1;
    }
  }
  runOps(ops, true);
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
  if (meshEdit_ != MeshEdit::Off && !meshSlot().isEmpty()) return keyDeform();  // editing a mesh: K keys its shape
  const cutout::Pose pose = currentPose(false);
  std::vector<Json> ops;
  // Every selected item (plugin 1.7.0): bones key their 5 channels, parts their shown image. One edit.
  for (const QString& ref : selectedItems()) {
    const QString kind = refKind(ref), id = refId(ref);
    auto key = [&](const char* prop, const Json& v) {
      ops.push_back({{"op", "setKey"}, {"clip", ss(clipId_)}, {"target", ss(ref)}, {"property", prop}, {"timeMs", playheadMs_}, {"value", v}});
    };
    if (kind == "bone") {
      const int i = rig_->boneIndex(ss(id));
      if (i < 0) continue;
      const auto& t = pose.bones[size_t(i)];
      key("x", t.x); key("y", t.y); key("rotation", t.rotation); key("scaleX", t.scaleX); key("scaleY", t.scaleY);
      // Shear (plugin 1.16.0) only where the bone uses it: unsheared rigs keep five channels.
      const Json* clip = findIn(displayData(), "clips", ss(clipId_));
      auto tracked = [&](const char* prop) {
        if (clip) for (const auto& tr : listOf(*clip, "tracks")) if (tr.value("target", std::string()) == ss(ref) && tr.value("property", std::string()) == prop) return true;
        return false;
      };
      if (t.shearX != 0 || tracked("shearX")) key("shearX", t.shearX);
      if (t.shearY != 0 || tracked("shearY")) key("shearY", t.shearY);
    } else if (kind == "slot") {
      const int i = rig_->slotIndex(ss(id));
      if (i < 0) continue;
      key("attachment", pose.attachments[size_t(i)] ? Json(*pose.attachments[size_t(i)]) : Json(nullptr));
    }
  }
  if (ops.empty()) return false;
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
  Json op = {{"op", "moveKey"}, {"clip", ss(clipId_)}, {"target", ss(r.target)}, {"fromMs", from}, {"toMs", to}};
  putProperty(op, r.property);
  const bool ok = runOps({op}, false);
  if (!ok) refreshDopesheet();
  return ok;
}
bool CutoutPanel::deleteKey(int row, int ms) {
  if (working_) { setStatus("먼저 K로 변경을 키로 저장하거나 변경을 취소하세요.", true); return false; }
  if (row < 0 || row >= dopesheet_->rows().size()) return false;
  const auto& r = dopesheet_->rows()[row];
  Json op = {{"op", "deleteKey"}, {"clip", ss(clipId_)}, {"target", ss(r.target)}, {"timeMs", ms}};
  putProperty(op, r.property);
  return runOps({op}, false);
}
bool CutoutPanel::setKeyInterp(int row, int ms, const QString& interp) {
  if (working_) { setStatus("먼저 K로 변경을 키로 저장하거나 변경을 취소하세요.", true); return false; }
  if (row < 0 || row >= dopesheet_->rows().size()) return false;
  const auto& r = dopesheet_->rows()[row];
  Json op = {{"op", "setInterp"}, {"clip", ss(clipId_)}, {"target", ss(r.target)}, {"timeMs", ms}, {"interp", ss(interp)}};
  putProperty(op, r.property);
  return runOps({op}, false);
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
    if (k.target != "event" && k.property != "attachment" && k.property != "expression" && k.property != "bendPositive") {
      Json op = {{"op", "setInterp"}, {"clip", ss(clipId_)}, {"target", ss(k.target)}, {"timeMs", k.timeMs}, {"interp", ss(preset)}};
      putProperty(op, k.property);
      ops.push_back(op);
    }
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
  for (const auto& k : keys) {
    Json ref = {{"target", ss(k.target)}, {"timeMs", k.timeMs}};
    putProperty(ref, k.property);
    out.push_back(ref);
  }
  return out;
}
void CutoutPanel::selectKeysFrom(const Json& refs) {
  QVector<KeyRef> keys;
  if (refs.is_array())
    for (const auto& r : refs) keys.push_back({qs(r.value("target", std::string())), uiProperty(r), int(r.value("timeMs", int64_t(0)))});
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
  // Exactly the selected bones (a part counts as its bone; 자식 본까지 선택 adds a whole limb), or
  // the whole skeleton when nothing is selected.
  std::vector<std::string> bones;
  for (const QString& b : selectedBones(true)) bones.push_back(ss(b));
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
  const JsonPtr heldData = displayShared();  // shared, not copied
  const Json& data = *heldData;
  const Json mix = data.contains("mix") && data.at("mix").is_object() ? data.at("mix") : Json::object();
  if (mix.value("defaultMs", int64_t(0)) == mixDefault_->value() && mix.value("pairs", Json::array()) == pairs) return;
  if (!runOps({{{"op", "setMix"}, {"defaultMs", mixDefault_->value()}, {"pairs", pairs}}}, false)) refreshMix();
}
bool CutoutPanel::addMixPair() {
  const QStringList clips = clipIds();
  const JsonPtr heldData = displayShared();  // shared, not copied
  const Json& data = *heldData;
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
  const JsonPtr heldData = displayShared();  // shared, not copied
  const Json& data = *heldData;
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
    auto parsed = std::make_shared<const cutout::Rig>(cutout::parseRig(ss(rigId_), *rig->data));
    preview_ = std::make_unique<cutout::Instance>(parsed, cutout::parseController(ss(controllerId_), *controller->data));
    preview_->setPhysicsEnvironment(physicsWind_, physicsGravity_);
    preview_->resetPhysics();
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
  const JsonPtr heldData = displayShared();  // shared, not copied
  const Json& data = *heldData;
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
void CutoutPanel::importAndAssignSkinImage(const QString& file, const QString& slot, const QString& name) {
  if (busy()) return;
  QPointer<CutoutPanel> self(this);
  inSubmit_ = true;
  host_->request("importImage", {{"file", file}}, [self, slot, name](QJsonObject reply) {
    if (!self) return;
    const QString id = reply.value("importedAssetId").toString();
    if (!reply.value("error").toString().isEmpty() || id.isEmpty()) {
      self->setStatus("PNG를 가져오지 못했습니다: " + reply.value("error").toString(), true);
      return;
    }
    // Next turn: the host is idle again and assets() includes the image (same as importAndAddPart).
    QTimer::singleShot(0, self, [self, id, slot, name] {
      if (!self || !self->host_) return;
      self->images_.setAssets(self->host_->assets());
      self->assignSkinImage(slot, id, name);
    });
  });
  inSubmit_ = false;
}
bool CutoutPanel::assignSkinImageInteractive() {
  if (previewSkin_.isEmpty()) { setStatus("먼저 스킨을 고르거나 만드세요(「새 스킨」).", true); return false; }
  const QVector<SkinSlotInfo> slots = skinSlots();
  if (slots.isEmpty()) { setStatus("그림을 바꿀 조각이 없습니다. 먼저 「이미지 조각 추가…」로 조각을 만드세요.", true); return false; }
  SkinImageChoice choice;
  choice.skin = previewSkin_;
  // The selected part, else the first part (draw order) on the selected bone, else the first part.
  if (refKind(selection_) == "slot") choice.slot = refId(selection_);
  else if (const QString bone = selectedBone(); !bone.isEmpty())
    for (const auto& s : slots) if (s.bone == bone) { choice.slot = s.slot; break; }
  if (choice.slot.isEmpty() || std::none_of(slots.begin(), slots.end(), [&](const SkinSlotInfo& s) { return s.slot == choice.slot; })) choice.slot = slots[0].slot;
  for (const auto& s : slots) if (s.slot == choice.slot) choice.name = s.shown;
  if (!askSkinImage || !askSkinImage(choice)) return false;
  if (choice.slot.isEmpty() || !rig_ || rig_->slotIndex(ss(choice.slot)) < 0) { setStatus("그림을 넣을 조각을 고르세요.", true); return false; }
  if (!choice.importFile.isEmpty()) { importAndAssignSkinImage(choice.importFile, choice.slot, choice.name); return true; }
  return assignSkinImage(choice.slot, choice.assetId, choice.name);
}
QVector<CutoutPanel::SkinSlotInfo> CutoutPanel::skinSlots() const {
  QVector<SkinSlotInfo> out;
  const JsonPtr heldData = displayShared();  // shared, not copied
  const Json& data = *heldData;
  const cutout::Pose pose = currentPose(false);
  const Json* skin = findIn(data, "skins", ss(previewSkin_));
  for (const auto& s : listOf(data, "slots")) {
    SkinSlotInfo info;
    info.slot = qs(s.value("id", std::string()));
    info.bone = qs(s.value("bone", std::string()));
    info.names = slotNames(data, ss(info.slot));
    // The name shown right now (setup, or the keyed one at the playhead): what 그림 지정 replaces.
    const int si = rig_ ? rig_->slotIndex(ss(info.slot)) : -1;
    if (si >= 0 && size_t(si) < pose.attachments.size() && pose.attachments[size_t(si)]) info.shown = qs(*pose.attachments[size_t(si)]);
    else if (s.contains("attachment") && s.at("attachment").is_string()) info.shown = qs(s.at("attachment").get<std::string>());
    if (skin)
      for (const auto& a : listOf(*skin, "attachments"))
        if (qs(a.value("slot", std::string())) == info.slot) info.images[qs(a.value("name", std::string()))] = qs(a.value("imageAssetId", std::string()));
    out << info;
  }
  return out;
}
QImage CutoutPanel::skinImagePreview(const QString& slot, const QString& name, const QString& assetId, const QSize& size) {
  QImage img(size, QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  const QSize pixels = images_.pixelSize(assetId);
  if (size.isEmpty() || previewSkin_.isEmpty() || name.isEmpty() || !pixels.isValid()) return img;
  // What confirming would commit (the same core op), drawn with that name shown: never sent.
  std::shared_ptr<const cutout::Rig> rig;
  try {
    const Json op = {{"op", "setSkinAttachment"}, {"skin", ss(previewSkin_)}, {"slot", ss(slot)}, {"name", ss(name)}, {"imageAssetId", ss(assetId)}, {"width", pixels.width()}, {"height", pixels.height()}};
    rig = std::make_shared<const cutout::Rig>(cutout::parseRig(ss(rigId_), cutout::applyEdit(displayData(), op).data));
  } catch (const std::exception&) { return img; }
  cutout::Look look = lookFor(*rig);
  look.expressions = false;
  cutout::Pose pose = mode_ == Mode::Animate ? cutout::samplePose(*rig, rig->clipIndex(ss(clipId_)), playheadExact_, look) : cutout::samplePose(*rig, -1, 0, look);
  const int si = rig->slotIndex(ss(slot));
  if (si >= 0 && size_t(si) < pose.attachments.size()) pose.attachments[size_t(si)] = ss(name);
  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);
  p.setTransform(elementPlacement(*rig, QRectF(QPointF(0, 0), QSizeF(size)), 1, false));
  drawRig(p, *rig, pose, images_, hidden_);
  return img;
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

// ---------------------------------------------------------------- IK constraints
void CutoutPanel::refreshIk() {
  if (!ikList_) return;
  const bool wasFilling = filling_;
  filling_ = true;
  const JsonPtr heldData = displayShared();  // shared, not copied
  const Json& data = *heldData;
  const Json& list = listOf(data, "ik");
  if (!ikId_.isEmpty() && !findIn(data, "ik", ss(ikId_))) ikId_.clear();
  if (ikId_.isEmpty() && !list.empty()) ikId_ = qs(list[0].value("id", std::string()));
  {
    QSignalBlocker block(ikList_);
    ikList_->clear();
    for (const auto& k : list) {
      const QString id = qs(k.value("id", std::string())), name = qs(k.value("name", std::string()));
      QStringList chain;
      for (const auto& b : listOf(k, "bones")) chain << qs(b.is_string() ? b.get<std::string>() : std::string());
      auto* item = new QListWidgetItem(QString("%1%2 · %3 %4 ✛ %5").arg(id, name.isEmpty() || name == id ? QString() : " (" + name + ")",
                                                                       chain.size() == 1 ? QString("조준") : chain.size() == 2 ? QString("팔·다리") : QString("사슬 %1").arg(chain.size()), chain.join(" → "),
                                                                       qs(k.value("target", std::string()))), ikList_);
      item->setData(Qt::UserRole, id);
      if (id == ikId_) ikList_->setCurrentItem(item);
    }
  }
  const Json* k = findIn(data, "ik", ss(ikId_));
  const bool animate = mode_ == Mode::Animate;
  const int index = rig_ ? rig_->ikIndex(ss(ikId_)) : -1;
  double mix = k ? k->value("mix", 1.0) : 1;
  bool bend = k ? k->value("bendPositive", true) : true;
  if (animate && index >= 0) {
    // Animate mode shows the sampled (keyed) values at the playhead: editing them makes keys.
    const cutout::Pose pose = currentPose(false);
    if (size_t(index) < pose.ikMix.size()) { mix = pose.ikMix[size_t(index)]; bend = pose.ikBend[size_t(index)] != 0; }
  }
  {
    QSignalBlocker b1(ikTarget_), b2(ikMix_), b3(ikBend_), b4(ikOrder_);
    ikTarget_->clear();
    std::set<std::string> chain;
    if (k) for (const auto& b : listOf(*k, "bones")) if (b.is_string()) chain.insert(b.get<std::string>());
    for (const auto& b : listOf(data, "bones")) {
      const std::string id = b.value("id", std::string());
      if (!chain.count(id)) ikTarget_->addItem(qs(id), qs(id));
    }
    ikTarget_->setCurrentIndex(k ? ikTarget_->findData(qs(k->value("target", std::string()))) : -1);
    ikMix_->setValue(mix);
    ikMix_->setProperty("shown", ikMix_->value());
    ikBend_->setChecked(bend);
    ikOrder_->setValue(k ? int(k->value("order", int64_t(0))) : 0);
    ikOrder_->setProperty("shown", ikOrder_->value());
    QSignalBlocker b5(ikAimAxis_);
    ikAimAxis_->setValue(k ? k->value("aimAxis", 0.0) : 0);
    ikAimAxis_->setProperty("shown", ikAimAxis_->value());
    QSignalBlocker b6(ikStretch_), b7(ikCompress_), b8(ikUniform_);
    ikStretch_->setChecked(k && k->value("stretch", false));
    ikCompress_->setChecked(k && k->value("compress", false));
    ikUniform_->setChecked(k && k->value("uniform", false));
  }
  for (QWidget* w : ikEditors_) w->setEnabled(k);
  const bool aim = k && listOf(*k, "bones").size() == 1;
  for (QWidget* w : ikAimTools_) w->setEnabled(aim);
  ikCompress_->setEnabled(aim);
  ikBend_->setEnabled(k && listOf(*k, "bones").size() == 2);
  for (QWidget* w : ikAnimateTools_) w->setEnabled(k && animate);
  QString info = "본을 고른 뒤 「팔·다리 IK」(아래팔·정강이) 또는 「조준 IK」(머리·눈)를 누르세요. 캔버스의 ✛ 대상 본을 끌면 사슬이 따라옵니다.";
  if (k && index >= 0) {
    const auto& c = rig_->ik[size_t(index)];
    const QString tip = c.bones.size() == 1 ? QString("조준 축 %1°가 대상을 향함").arg(c.aimAxis, 0, 'g', 6) : c.tip >= 0 ? QString("끝점: %1의 원점").arg(qs(rig_->bones[size_t(c.tip)].id))
                      : c.tipLength > 0 ? QString("끝점: %1의 끝(길이 %2)").arg(qs(rig_->bones[size_t(c.bones.back())].id)).arg(c.tipLength, 0, 'g', 6)
                                                                                      : QString("끝점 본 없음 · %1의 원점이 대상을 향함").arg(qs(rig_->bones[size_t(c.bones.back())].id));
    // Chains (plugin 1.13.0): what the solve did at the shown pose (FABRIK passes, gap, stretch).
    const cutout::Pose shown = currentPose(false);
    const cutout::IkSolve solve = size_t(index) < shown.ikSolve.size() ? shown.ikSolve[size_t(index)] : cutout::IkSolve{};
    QString solved;
    if (c.bones.size() > 2)
      solved = QString(" · FABRIK %1회 반복(최대 %2회, 허용 오차 %3px) · 끝점 오차 %4px").arg(solve.iterations).arg(cutout::IK_CHAIN_ITERATIONS).arg(cutout::IK_CHAIN_TOLERANCE).arg(solve.error, 0, 'f', 3);
    if (c.stretch || c.compress) {
      solved += !c.stretchable ? QString(" · 늘이기: 길이 없는 본이 있어 쓰이지 않음") : solve.stretch != 1 ? QString(" · 늘이기 ×%1").arg(solve.stretch, 0, 'f', 3) : QString(" · 늘이기 대기(닿는 거리)");
    }
    info = tip + solved + (animate ? " · 애니메이션 모드: 혼합·굽힘을 바꾸면 이 시간의 키가 되고, ✛를 끌면 대상 본 위치가 바뀝니다(K로 키)."
                                   : " · 조립 모드: 값은 IK의 기본값입니다. ✛를 끌면 대상 본의 기본 자세가 바뀝니다.");
  }
  ikInfo_->setText(info);
  if (canvas_) canvas_->setSelectedIk(ikId_);
  filling_ = wasFilling;
}
const Json* CutoutPanel::currentIk() const {
  return ikId_.isEmpty() ? nullptr : findIn(displayData(), "ik", ss(ikId_));
}
bool CutoutPanel::newIk(bool aim) {
  if (!rigDoc()) { setStatus("먼저 캐릭터를 만드세요.", true); return false; }
  const QString bone = selectedBone();
  if (bone.isEmpty()) { setStatus(aim ? "조준할 본(머리·눈 등)을 먼저 선택하세요." : "IK 사슬의 끝이 될 본(아래팔·정강이 등)을 먼저 선택하세요.", true); return false; }
  Json bones = Json::array();
  if (!aim) {
    const Json* b = findIn(displayData(), "bones", ss(bone));
    const std::string parent = b && b->contains("parent") && b->at("parent").is_string() ? b->at("parent").get<std::string>() : std::string();
    if (parent.empty()) { setStatus("두 본 IK에는 부모 본이 필요합니다. 위팔 아래의 아래팔처럼 부모가 있는 본을 고르거나 「조준 IK」를 쓰세요.", true); return false; }
    bones.push_back(parent);
  }
  bones.push_back(ss(bone));
  QString target;
  const bool ok = runOps({{{"op", "addIk"}, {"bones", bones}, {"createTarget", true}}}, false, [&](const cutout::EditResult& r) {
    // Before the commit rebuilds the view: the new constraint and its target are shown selected.
    ikId_ = qs(r.extra.value("ik", std::string()));
    target = qs(r.extra.value("target", std::string()));
    selection_ = "bone:" + target;
  });
  if (!ok) return false;
  setStatus(QString("IK 「%1」를 만들었습니다. 캔버스의 ✛ 「%2」를 끌면 %3. 되돌리기: Ctrl+Z")
                .arg(ikId_, target, aim ? QString("「%1」가 그쪽을 바라봅니다").arg(bone) : QString("팔·다리가 따라옵니다")));
  return true;
}
bool CutoutPanel::setIkAimAxis(double degrees) {
  const Json* k = currentIk();
  if (!k) { setStatus("먼저 IK를 고르세요.", true); return false; }
  if (listOf(*k, "bones").size() != 1) { setStatus("조준 축은 조준 IK(본 1개)에만 씁니다. 팔·다리 IK는 본 위치로 풉니다.", true); refreshIk(); return false; }
  // Not keyable: in both modes this is the constraint's setup value (one edit).
  const double clean = std::round(degrees * 10) / 10;
  return runOps({{{"op", "setIk"}, {"ik", ss(ikId_)}, {"aimAxis", clean == 0 ? Json(nullptr) : Json(clean)}}}, false);
}
bool CutoutPanel::fitIkAimAxis() {
  const Json* k = currentIk();
  if (!k) { setStatus("먼저 IK를 고르세요.", true); return false; }
  if (listOf(*k, "bones").size() != 1) { setStatus("현재 방향으로 맞추기는 조준 IK(본 1개)에만 씁니다.", true); return false; }
  if (refusePending()) return false;
  Json op = {{"op", "setIk"}, {"ik", ss(ikId_)}, {"fitAimAxis", true}};
  if (mode_ == Mode::Animate && !clipId_.isEmpty()) { op["clip"] = ss(clipId_); op["timeMs"] = playheadExact_; }
  if (!runOps({op}, false)) return false;
  const Json* fitted = findIn(committedRig(), "ik", ss(ikId_));
  setStatus(QString("조준 축을 %1°로 맞췄습니다(지금 본이 대상을 보는 방향). 되돌리기: Ctrl+Z").arg(fitted ? fitted->value("aimAxis", 0.0) : 0.0, 0, 'g', 6));
  return true;
}
bool CutoutPanel::selectIk(const QString& id) {
  if (!findIn(displayData(), "ik", ss(id))) { setStatus("없는 IK입니다: " + id, true); return false; }
  ikId_ = id;
  refreshIk();
  return true;
}
bool CutoutPanel::selectIkTarget() {
  const Json* k = currentIk();
  if (!k) { setStatus("먼저 IK를 고르세요.", true); return false; }
  selectItem("bone:" + qs(k->value("target", std::string())));
  return true;
}
bool CutoutPanel::setIkMix(double mix) {
  const Json* k = currentIk();
  if (!k) { setStatus("먼저 IK를 만들거나 고르세요.", true); refreshIk(); return false; }
  if (mode_ == Mode::Animate) {
    if (clipId_.isEmpty()) { setStatus("키를 넣을 클립을 먼저 만드세요.", true); refreshIk(); return false; }
    return runOps({{{"op", "setKey"}, {"clip", ss(clipId_)}, {"target", "ik:" + ss(ikId_)}, {"property", "mix"}, {"timeMs", playheadMs_}, {"value", mix}}}, true);
  }
  return runOps({{{"op", "setIk"}, {"ik", ss(ikId_)}, {"mix", mix}}}, false);
}
bool CutoutPanel::setIkBend(bool positive) {
  const Json* k = currentIk();
  if (!k) { setStatus("먼저 IK를 만들거나 고르세요.", true); refreshIk(); return false; }
  if (mode_ == Mode::Animate) {
    if (clipId_.isEmpty()) { setStatus("키를 넣을 클립을 먼저 만드세요.", true); refreshIk(); return false; }
    return runOps({{{"op", "setKey"}, {"clip", ss(clipId_)}, {"target", "ik:" + ss(ikId_)}, {"property", "bendPositive"}, {"timeMs", playheadMs_}, {"value", positive}}}, true);
  }
  return runOps({{{"op", "setIk"}, {"ik", ss(ikId_)}, {"bendPositive", positive}}}, false);
}
bool CutoutPanel::setIkOrder(int order) {
  if (!currentIk()) { setStatus("먼저 IK를 고르세요.", true); return false; }
  return runOps({{{"op", "setIk"}, {"ik", ss(ikId_)}, {"order", order == 0 ? Json(nullptr) : Json(order)}}}, false);
}
bool CutoutPanel::setIkTarget(const QString& bone) {
  if (!currentIk()) { setStatus("먼저 IK를 고르세요.", true); return false; }
  if (bone.isEmpty()) return false;
  return runOps({{{"op", "setIk"}, {"ik", ss(ikId_)}, {"target", ss(bone)}}}, false);
}
bool CutoutPanel::renameIk() {
  const Json* k = currentIk();
  if (!k) { setStatus("이름을 바꿀 IK를 고르세요.", true); return false; }
  const QString id = askText("IK 이름 변경", "새 IK ID (이 IK의 키도 함께 바뀝니다)", ikId_);
  if (id.isEmpty() || id == ikId_) return false;
  return runOps({{{"op", "renameIk"}, {"ik", ss(ikId_)}, {"id", ss(id)}}}, false, [&](const cutout::EditResult&) { ikId_ = id; });
}
bool CutoutPanel::deleteIk() {
  const Json* k = currentIk();
  if (!k) { setStatus("삭제할 IK를 고르세요.", true); return false; }
  const QString target = qs(k->value("target", std::string()));
  if (!askConfirm("IK 삭제", QString("IK 「%1」와 그 키를 삭제할까요? 다른 곳에서 쓰지 않는 대상 본 「%2」도 함께 삭제됩니다. 사슬의 본은 그대로 남습니다.").arg(ikId_, target)))
    return false;
  const QString id = ikId_;
  return runOps({{{"op", "deleteIk"}, {"ik", ss(id)}, {"deleteTarget", true}}}, false, [&](const cutout::EditResult&) {
    ikId_.clear();
    if (selection_ == "bone:" + target) selection_.clear();
  });
}
bool CutoutPanel::keyIk() {
  if (mode_ != Mode::Animate) { setStatus("IK 키는 애니메이션 모드에서 넣습니다.", true); return false; }
  if (clipId_.isEmpty()) { setStatus("키를 넣을 클립을 먼저 만드세요.", true); return false; }
  if (!currentIk() || !rig_) { setStatus("키를 넣을 IK를 고르세요.", true); return false; }
  const int index = rig_->ikIndex(ss(ikId_));
  const cutout::Pose pose = currentPose(false);
  if (index < 0 || size_t(index) >= pose.ikMix.size()) return false;
  const std::string target = "ik:" + ss(ikId_);
  return runOps({{{"op", "setKey"}, {"clip", ss(clipId_)}, {"target", target}, {"property", "mix"}, {"timeMs", playheadMs_}, {"value", pose.ikMix[size_t(index)]}},
                 {{"op", "setKey"}, {"clip", ss(clipId_)}, {"target", target}, {"property", "bendPositive"}, {"timeMs", playheadMs_}, {"value", pose.ikBend[size_t(index)] != 0}}}, false);
}
bool CutoutPanel::bakeIk() {
  if (mode_ != Mode::Animate) { setStatus("IK 굽기는 애니메이션 모드에서 합니다(클립의 이 시간에 키를 넣음).", true); return false; }
  if (clipId_.isEmpty()) { setStatus("키를 넣을 클립을 먼저 만드세요.", true); return false; }
  if (!currentIk()) { setStatus("구울 IK를 고르세요.", true); return false; }
  if (!runOps({{{"op", "bakeIk"}, {"clip", ss(clipId_)}, {"timeMs", playheadMs_}, {"ik", ss(ikId_)}, {"disable", true}}}, false)) return false;
  setStatus(QString("「%1」 %2ms: IK 회전을 키로 굽고 혼합을 0으로 키했습니다. 이 시간부터는 회전 키로 다듬으세요. 되돌리기: Ctrl+Z").arg(clipId_).arg(playheadMs_));
  return true;
}

// ---------------------------------------------------------------- meshes (plugin 1.4.0)
bool CutoutPanel::meshTarget(int& si, const cutout::Attachment*& att, Json& fields) const {
  si = -1;
  att = nullptr;
  if (!rig_ || refKind(selection_) != "slot") return false;
  si = rig_->slotIndex(ss(refId(selection_)));
  const cutout::Pose pose = currentPose(false);
  if (si < 0 || size_t(si) >= pose.attachments.size() || !pose.attachments[size_t(si)]) return false;
  att = rig_->attachment(rig_->slots[size_t(si)].id, *pose.attachments[size_t(si)], pose.skin);
  if (!att) return false;
  fields = {{"slot", rig_->slots[size_t(si)].id}, {"name", att->name}};
  // The preview skin's own entry is edited when it provides the shown name (like placement drags).
  if (const QString skin = skinProviding(refId(selection_), qs(att->name)); !skin.isEmpty()) fields["skin"] = ss(skin);
  return true;
}
QString CutoutPanel::meshSlot() const {
  int si;
  const cutout::Attachment* att;
  Json fields;
  return meshTarget(si, att, fields) && att->mesh ? refId(selection_) : QString();
}
void CutoutPanel::refreshMesh() {
  if (!meshInfo_) return;
  const bool wasFilling = filling_;
  filling_ = true;
  int si = -1;
  const cutout::Attachment* att = nullptr;
  Json fields;
  // A clip slot (plugin 1.12.0) has no picture: the mesh tools leave it alone (its outline is still drawn).
  const bool target = meshTarget(si, att, fields), part = target && !att->clipping, mesh = part && att->mesh, animate = mode_ == Mode::Animate;
  const QString key = part ? qs(fields.dump()) : QString();
  if (key != meshSelectionKey_) { meshSelection_.clear(); meshSelectionKey_ = key; }
  if (mesh) for (auto it = meshSelection_.begin(); it != meshSelection_.end();) it = *it >= int(att->vertexCount()) ? meshSelection_.erase(it) : std::next(it);
  if (!mesh || (animate && meshEdit_ == MeshEdit::Weights)) meshEdit_ = MeshEdit::Off;
  // Outline drawing belongs to one part in setup mode: another selection or Animate ends it.
  if (!outlineSlot_.isEmpty() && (animate || !target || refId(selection_) != outlineSlot_)) { outlineSlot_.clear(); outlineForClip_ = false; }
  // Bone lists: rebuilt for another part (checks are editor state of this part's next auto weights).
  if (meshBones_->property("key").toString() != key || meshBones_->count() != int(rig_ ? rig_->bones.size() : 0)) {
    QSignalBlocker block(meshBones_);
    meshBones_->clear();
    std::set<int> weighted;
    if (mesh) for (const auto& b : att->binds) weighted.insert(b.first);
    for (size_t b = 0; rig_ && b < rig_->bones.size(); b++) {
      auto* item = new QListWidgetItem(qs(rig_->bones[b].id), meshBones_);
      item->setData(Qt::UserRole, qs(rig_->bones[b].id));
      item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
      item->setCheckState(weighted.count(int(b)) ? Qt::Checked : Qt::Unchecked);
    }
    meshBones_->setProperty("key", key);
  }
  {
    QSignalBlocker block(brushBone_);
    const QString current = brushBone_->currentData().toString();
    brushBone_->clear();
    for (size_t b = 0; rig_ && b < rig_->bones.size(); b++) brushBone_->addItem(qs(rig_->bones[b].id), qs(rig_->bones[b].id));
    int at = brushBone_->findData(current);
    if (at < 0 && part) at = brushBone_->findData(qs(rig_->slots[size_t(si)].bone));
    brushBone_->setCurrentIndex(std::max(0, at));
  }
  for (QWidget* w : meshSetupTools_) w->setEnabled(part && !animate);
  // Setup rows: the shape tools replace the weight row while 정점 편집 is on (plugin 1.6.0).
  const bool shaping = !animate && mesh && meshEdit_ == MeshEdit::Vertices;
  for (QWidget* row : meshModeRows_) {
    const QString kind = row->property("cutoutModeRow").toString();
    row->setVisible(animate ? kind == "animate" : kind == "setup" || (kind == "setup-vertices" ? shaping : kind == "setup-weights" && !shaping));
  }
  meshSelectTool_->setChecked(meshTool_ == RigCanvas::MeshSelect);
  meshAddVertexTool_->setChecked(meshTool_ == RigCanvas::MeshAddVertex);
  meshAddEdgeTool_->setChecked(meshTool_ == RigCanvas::MeshAddEdge);
  for (QWidget* w : meshTools_) w->setEnabled(mesh && (!animate || w == meshVerticesButton_));
  for (QWidget* w : meshAnimateTools_) w->setEnabled(mesh && animate);
  meshVerticesButton_->setChecked(meshEdit_ == MeshEdit::Vertices);
  meshWeightsButton_->setChecked(meshEdit_ == MeshEdit::Weights);
  QString info = "조각(▣)을 고른 뒤 「메시로 변환」을 누르면 그림을 휘게 할 수 있습니다.";
  if (part && !mesh) info = QString("「%1」은 보통 그림입니다. 조립 모드에서 「메시로 변환」(가로·세로 칸)을 누르세요.").arg(qs(att->name));
  if (mesh) {
    QStringList bones;
    for (const auto& b : att->binds) bones << qs(rig_->bones[size_t(b.first)].id);
    info = QString("메시 「%1」 · 정점 %2 · 삼각형 %3 · %4 · 고른 정점 %5개")
               .arg(qs(att->name)).arg(att->vertexCount()).arg(att->triangles.size() / 3)
               .arg(bones.isEmpty() ? QString("웨이트 없음(본 「%1」을 따라감)").arg(qs(rig_->slots[size_t(si)].bone)) : "웨이트: " + bones.join(", "))
               .arg(meshSelection_.size());
    int pinned = 0;
    std::set<std::pair<int, int>> outline;
    for (const auto& e : meshOutline(att->triangles)) outline.insert({e.first, e.second});
    for (size_t k = 0; k + 1 < att->edges.size(); k += 2)
      pinned += att->edges[k] != att->edges[k + 1] && !outline.count({std::min(att->edges[k], att->edges[k + 1]), std::max(att->edges[k], att->edges[k + 1])});
    info += att->hull >= 3 ? QString(" · 윤곽 정점 %1").arg(att->hull) : QString(" · 윤곽 없음(격자)");
    if (pinned) info += QString(" · 고정 가장자리 %1").arg(pinned);
    if (animate) info += " · 애니메이션 모드: 정점을 끌면 이 시간의 변형이 됩니다(K로 키).";
  }
  if (!outlineSlot_.isEmpty() && !outlineForClip_) info = QString("윤곽 그리기 중: 「%1」 둘레를 클릭하세요. 첫 점 클릭·더블클릭·Enter = 메시 만들기, Esc = 취소.").arg(outlineSlot_);
  meshInfo_->setText(info);
  if (canvas_) {
    canvas_->setMeshEdit(int(meshEdit_), mesh ? refId(selection_) : QString(), meshSelection_,
                         rig_ ? rig_->boneIndex(ss(brushBone_->currentData().toString())) : -1, brushRadius_->value());
    // Only on change: both reset the canvas gesture (a refresh can run in the middle of a drag).
    const int tool = shaping ? meshTool_ : int(RigCanvas::MeshSelect);
    if (canvas_->meshTool() != tool) canvas_->setMeshTool(tool);
    if (canvas_->outlineDrawing() != !outlineSlot_.isEmpty()) canvas_->setOutlineDrawing(outlineSlot_);
  }
  refreshOutlinePreview();
  filling_ = wasFilling;
}
bool CutoutPanel::convertToMesh(int rows, int cols, bool trim) {
  if (mode_ != Mode::Setup) { setStatus("메시 변환은 조립 모드에서 합니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op)) { setStatus("메시로 바꿀 조각(▣)을 먼저 고르세요.", true); return false; }
  op["op"] = "convertToMesh";
  op["rows"] = rows;
  op["cols"] = cols;
  const QImage img = images_.image(qs(att->imageAssetId));
  if (trim && !img.isNull()) {
    // Alpha-trimmed grid: a cell stays when any of its pixels is visibly opaque.
    std::string cells;
    for (int r = 0; r < rows; r++)
      for (int c = 0; c < cols; c++) {
        const int x0 = c * img.width() / cols, x1 = std::max(x0 + 1, (c + 1) * img.width() / cols);
        const int y0 = r * img.height() / rows, y1 = std::max(y0 + 1, (r + 1) * img.height() / rows);
        bool opaque = false;
        for (int y = y0; y < y1 && !opaque; y++)
          for (int x = x0; x < x1 && !opaque; x++) opaque = qAlpha(img.pixel(std::min(x, img.width() - 1), std::min(y, img.height() - 1))) >= 16;
        cells += opaque ? '1' : '0';
      }
    if (cells.find('0') != std::string::npos) op["cells"] = cells;
  }
  int vertices = 0;
  const QString name = qs(att->name);  // the commit re-parses the rig: `att` dangles after runOps
  if (!runOps({op}, false, [&](const cutout::EditResult& r) { vertices = r.extra.value("vertices", 0); })) return false;
  setStatus(QString("「%1」을 메시(정점 %2개)로 바꿨습니다. 「정점 편집」으로 모양을, 「자동 웨이트」로 본을 따라 휘게 하세요. 되돌리기: Ctrl+Z").arg(name).arg(vertices));
  return true;
}
bool CutoutPanel::convertToRegion() {
  if (mode_ != Mode::Setup) { setStatus("메시 되돌리기는 조립 모드에서 합니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op) || !att->mesh) { setStatus("메시인 조각을 먼저 고르세요.", true); return false; }
  op["op"] = "convertToRegion";
  return runOps({op}, false);
}
void CutoutPanel::setMeshEdit(MeshEdit mode) {
  if (mode != MeshEdit::Off && meshSlot().isEmpty()) { setStatus("메시인 조각을 먼저 고르세요(「메시로 변환」).", true); mode = MeshEdit::Off; }
  if (mode == MeshEdit::Weights && mode_ == Mode::Animate) { setStatus("웨이트는 조립 모드에서 칠합니다(기본 자세의 데이터).", true); mode = MeshEdit::Off; }
  meshEdit_ = mode;
  refreshMesh();
}
void CutoutPanel::selectVertices(const QVector<int>& vertices, bool add) {
  if (!add) meshSelection_.clear();
  for (int v : vertices) meshSelection_.insert(v);
  refreshMesh();
}
QVector<int> CutoutPanel::selectedVertices() const {
  QVector<int> out(meshSelection_.begin(), meshSelection_.end());
  std::sort(out.begin(), out.end());
  return out;
}
void CutoutPanel::setBrush(const QString& bone, double weight, double strength, double radius) {
  brushBone_->setCurrentIndex(std::max(0, brushBone_->findData(bone)));
  brushWeight_->setValue(weight);
  brushStrength_->setValue(strength);
  brushRadius_->setValue(radius);
  refreshMesh();
}
/**
 * Ops for moving the selected vertices by a rig-space delta, from the drag-start snapshot
 * (dragRig_/dragPose_): each vertex's own affine map (weighted blend of its bones) is inverted,
 * so the vertex follows the cursor exactly whatever its bones do. Setup: new image positions;
 * Animate: the shown deform offsets plus the image-space deltas, keyed at the playhead.
 */
std::vector<Json> CutoutPanel::meshMoveOps(const QPointF& rigDelta) const {
  if (!dragRig_ || dragPose_.world.size() != dragRig_->bones.size() || meshSelection_.isEmpty()) return {};
  int si;
  const cutout::Attachment* shown;
  Json fields;
  if (!meshTarget(si, shown, fields) || !shown->mesh) return {};
  const cutout::Attachment* att = dragRig_->attachment(ss(refId(selection_)), shown->name, dragPose_.skin);
  if (!att || !att->mesh) return {};
  const std::vector<double>* deform = cutout::deformOf(dragPose_, si, *att);
  std::vector<double> offsets = deform ? *deform : std::vector<double>(att->vertices.size(), 0.0);
  std::vector<int> list;
  std::vector<double> positions;
  for (int v : selectedVertices()) {
    if (v < 0 || size_t(v) >= att->vertexCount()) continue;
    const cutout::Affine m = cutout::meshVertexMatrix(*dragRig_, dragPose_, si, *att, size_t(v));
    const double det = m.a * m.d - m.b * m.c;
    if (std::fabs(det) < 1e-12) continue;
    const double dx = (m.d * rigDelta.x() - m.c * rigDelta.y()) / det, dy = (-m.b * rigDelta.x() + m.a * rigDelta.y()) / det;
    list.push_back(v);
    positions.push_back(att->vertices[2 * size_t(v)] + dx);
    positions.push_back(att->vertices[2 * size_t(v) + 1] + dy);
    offsets[2 * size_t(v)] += dx;
    offsets[2 * size_t(v) + 1] += dy;
  }
  if (list.empty()) return {};
  if (mode_ == Mode::Setup) {
    Json op = fields;
    op.update({{"op", "setMeshVertices"}, {"vertices", list}, {"positions", positions}});
    return {op};
  }
  return {{{"op", "setKey"}, {"clip", ss(clipId_)}, {"target", ss(selection_)}, {"property", "deform"}, {"attachment", att->name}, {"timeMs", playheadMs_}, {"offsets", offsets}}};
}
void CutoutPanel::onMeshDrag(const QPointF& delta, bool finished) {
  if (!dragActive_) {
    dragActive_ = true;
    dragBase_ = working_ ? *working_ : committedRig();
    dragRig_ = rig_;
    dragPose_ = currentPose(false);
  }
  if (mode_ == Mode::Animate && clipId_.isEmpty()) { setStatus("키를 넣을 클립을 먼저 만드세요.", true); if (finished) dragActive_ = false; return; }
  const std::vector<Json> ops = meshMoveOps(delta);
  if (!finished) {
    if (ops.empty()) return;
    try { dragPreview_ = std::make_shared<const Json>(cutout::applyEdit(dragBase_, batch(ops)).data); } catch (const std::exception&) { return; }
    rebuildDisplay(false);
    return;
  }
  dragActive_ = false;
  dragPreview_.reset();
  if (ops.empty()) { rebuildDisplay(false); return; }
  runOps(ops, mode_ == Mode::Animate);
}
bool CutoutPanel::moveVertices(const QPointF& rigDelta) {
  if (meshSelection_.isEmpty()) { setStatus("옮길 정점을 먼저 고르세요.", true); return false; }
  if (mode_ == Mode::Animate && clipId_.isEmpty()) { setStatus("키를 넣을 클립을 먼저 만드세요.", true); return false; }
  dragRig_ = rig_;
  dragPose_ = currentPose(false);
  const std::vector<Json> ops = meshMoveOps(rigDelta);
  if (ops.empty()) { setStatus("메시인 조각과 정점을 먼저 고르세요.", true); return false; }
  return runOps(ops, mode_ == Mode::Animate);
}
bool CutoutPanel::autoWeights(const QStringList& bonesIn) {
  if (mode_ != Mode::Setup) { setStatus("웨이트는 조립 모드에서 줍니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op) || !att->mesh) { setStatus("메시인 조각을 먼저 고르세요.", true); return false; }
  QStringList bones = bonesIn;
  if (bones.isEmpty())
    for (int i = 0; i < meshBones_->count(); i++) if (meshBones_->item(i)->checkState() == Qt::Checked) bones << meshBones_->item(i)->data(Qt::UserRole).toString();
  op["op"] = "autoWeights";
  if (!bones.isEmpty()) { std::vector<std::string> list; for (const auto& b : bones) list.push_back(ss(b)); op["bones"] = list; }
  Json used = Json::array();
  if (!runOps({op}, false, [&](const cutout::EditResult& r) { used = r.extra.value("bones", Json::array()); })) return false;
  QStringList names;
  for (const auto& b : used) names << qs(b.get<std::string>());
  meshBones_->setProperty("key", QString());  // re-check the bones that now weight the mesh
  setStatus(QString("자동 웨이트를 줬습니다(본: %1). 애니메이션 모드에서 본을 움직이면 메시가 휘어집니다. 되돌리기: Ctrl+Z").arg(names.join(", ")));
  return true;
}
bool CutoutPanel::paintWeights(const QVector<int>& vertices) {
  if (mode_ != Mode::Setup) { setStatus("웨이트는 조립 모드에서 칠합니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op) || !att->mesh) { setStatus("메시인 조각을 먼저 고르세요.", true); return false; }
  if (vertices.isEmpty() || brushBone_->currentData().toString().isEmpty()) { setStatus("칠할 정점과 본을 고르세요.", true); return false; }
  op.update({{"op", "setWeights"}, {"vertices", std::vector<int>(vertices.begin(), vertices.end())}, {"bone", ss(brushBone_->currentData().toString())},
             {"weight", brushWeight_->value()}, {"strength", brushStrength_->value()}});
  if (!runOps({op}, false)) return false;
  meshBones_->setProperty("key", QString());
  setStatus(QString("정점 %1개에 본 「%2」 웨이트를 칠했습니다. 되돌리기: Ctrl+Z").arg(vertices.size()).arg(brushBone_->currentData().toString()));
  return true;
}
bool CutoutPanel::unbindMesh() {
  if (mode_ != Mode::Setup) { setStatus("웨이트는 조립 모드에서 지웁니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op) || !att->mesh) { setStatus("메시인 조각을 먼저 고르세요.", true); return false; }
  op["op"] = "unbindMesh";
  meshBones_->setProperty("key", QString());
  return runOps({op}, false);
}
bool CutoutPanel::keyDeform() {
  if (mode_ != Mode::Animate || clipId_.isEmpty()) { setStatus("변형 키는 애니메이션 모드에서 클립에 넣습니다.", true); return false; }
  if (working_) { cutout::EditResult r; r.data = *working_; return commitRig(std::move(r)); }
  int si;
  const cutout::Attachment* att;
  Json fields;
  if (!meshTarget(si, att, fields) || !att->mesh) { setStatus("메시인 조각을 먼저 고르세요.", true); return false; }
  const cutout::Pose pose = currentPose(false);
  const std::vector<double>* d = cutout::deformOf(pose, si, *att);
  return runOps({{{"op", "setKey"}, {"clip", ss(clipId_)}, {"target", ss(selection_)}, {"property", "deform"}, {"attachment", att->name}, {"timeMs", playheadMs_},
                  {"offsets", d ? *d : std::vector<double>(att->vertices.size(), 0.0)}}}, false);
}
bool CutoutPanel::deleteDeformKey() {
  if (refusePending()) return false;
  if (mode_ != Mode::Animate || clipId_.isEmpty()) { setStatus("변형 키는 애니메이션 모드에서 지웁니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json fields;
  if (!meshTarget(si, att, fields) || !att->mesh) { setStatus("메시인 조각을 먼저 고르세요.", true); return false; }
  return runOps({{{"op", "deleteKey"}, {"clip", ss(clipId_)}, {"target", ss(selection_)}, {"property", "deform"}, {"attachment", att->name}, {"timeMs", playheadMs_}}}, false);
}

// ---------------------------------------------------------------- freeform meshes (plugin 1.6.0)
// Gestures turn canvas points into image pixels of the shown attachment and run ONE core op; the
// core triangulates (constrained Delaunay) and keeps weights / deform keys consistent.
const CutoutPanel::AlphaMask* CutoutPanel::alphaMaskOf(const cutout::Attachment& att) {
  const QImage img = images_.image(qs(att.imageAssetId));
  if (img.isNull()) return nullptr;
  const QString key = qs(att.imageAssetId) + "#" + QString::number(img.cacheKey());
  if (alphaMask_.key != key) {
    alphaMask_ = {key, img.width(), img.height(), std::vector<uint8_t>(size_t(img.width()) * size_t(img.height()))};
    const QImage argb = img.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < argb.height(); y++) {
      const QRgb* line = reinterpret_cast<const QRgb*>(argb.constScanLine(y));
      for (int x = 0; x < argb.width(); x++) alphaMask_.alpha[size_t(y) * size_t(argb.width()) + size_t(x)] = uint8_t(qAlpha(line[x]));
    }
  }
  return &alphaMask_;
}
QPolygonF CutoutPanel::autoOutlinePreview() {
  int si;
  const cutout::Attachment* att;
  Json fields;
  if (!outlinePreview_ || !outlinePreview_->isChecked() || mode_ != Mode::Setup || !meshTarget(si, att, fields)) return {};
  const AlphaMask* mask = alphaMaskOf(*att);
  if (!mask) return {};
  // The trace (the slow part) is cached per image and settings; the placement is cheap.
  const QString key = mask->key + QString("|%1|%2").arg(outlineThreshold_->value()).arg(outlineDetail_->value());
  if (key != outlinePreviewKey_) {
    outlinePreviewKey_ = key;
    try { previewOutline_ = cutout::traceAlphaOutline(mask->alpha, mask->width, mask->height, outlineThreshold_->value(), outlineDetail_->value()); }
    catch (const std::exception&) { previewOutline_.clear(); }
  }
  const cutout::Attachment* shown = nullptr;
  const cutout::Affine m = attachmentMatrix(*rig_, currentPose(false), si, &shown);
  QPolygonF out;
  const double sx = att->width / mask->width, sy = att->height / mask->height;
  for (size_t i = 0; i + 1 < previewOutline_.size(); i += 2) {
    double x, y;
    m.apply(previewOutline_[i] * sx, previewOutline_[i + 1] * sy, x, y);
    out << QPointF(x, y);
  }
  return out;
}
void CutoutPanel::refreshOutlinePreview() { if (canvas_) canvas_->setOutlinePreview(autoOutlinePreview()); }
void CutoutPanel::setAutoOutlineParams(double threshold, double detail, double spacing, bool preview) {
  outlineThreshold_->setValue(threshold);
  outlineDetail_->setValue(detail);
  outlineSpacing_->setValue(spacing);
  outlinePreview_->setChecked(preview);
  refreshOutlinePreview();
}
bool CutoutPanel::beginOutline() {
  if (mode_ != Mode::Setup) { setStatus("윤곽 그리기는 조립 모드에서 합니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json fields;
  if (!meshTarget(si, att, fields)) { setStatus("윤곽을 그릴 조각(▣)을 먼저 고르세요.", true); return false; }
  outlineSlot_ = refId(selection_);
  outlineForClip_ = false;
  meshEdit_ = MeshEdit::Off;  // the new outline replaces the shape: vertex handles would only get in the way
  refreshMesh();
  if (canvas_) canvas_->setFocus(Qt::OtherFocusReason);
  setStatus(QString("윤곽 그리기: 「%1」 그림 둘레를 차례로 클릭하세요. 첫 점 클릭·더블클릭·Enter로 닫으면 메시가 됩니다. Esc = 취소").arg(outlineSlot_));
  return true;
}
bool CutoutPanel::outlineFromCanvas(const QPolygonF& rigPoints) {
  int si;
  const cutout::Attachment* att;
  Json op;
  if (mode_ != Mode::Setup || !meshTarget(si, att, op)) { outlineSlot_.clear(); refreshMesh(); return false; }
  const cutout::Attachment* shown = nullptr;
  const auto inverse = attachmentMatrix(*rig_, currentPose(false), si, &shown).inverse();
  if (!inverse) { setStatus("크기가 0인 조각에는 윤곽을 그릴 수 없습니다.", true); return false; }
  std::vector<double> hull;
  for (const QPointF& p : rigPoints) {
    double x, y;
    inverse->apply(p.x(), p.y(), x, y);
    hull.push_back(x);
    hull.push_back(y);
  }
  op["op"] = "createMeshFromOutline";
  op["hull"] = hull;
  if (outlineSpacing_->value() > 0) op["spacing"] = outlineSpacing_->value();
  int vertices = 0, triangles = 0;
  // A refused outline (crossing itself...) keeps the tool on so the author can draw again.
  if (!runOps({op}, false, [&](const cutout::EditResult& r) { vertices = r.extra.value("vertices", 0); triangles = r.extra.value("triangles", 0); })) return false;
  outlineSlot_.clear();
  meshEdit_ = MeshEdit::Vertices;
  meshTool_ = RigCanvas::MeshSelect;
  refreshMesh();
  setStatus(QString("윤곽으로 메시를 만들었습니다(정점 %1 · 삼각형 %2). 「정점 추가」로 안쪽 정점을 더하고 「자동 웨이트」로 본에 묶으세요. 되돌리기: Ctrl+Z").arg(vertices).arg(triangles));
  return true;
}
bool CutoutPanel::autoOutline() {
  if (mode_ != Mode::Setup) { setStatus("자동 윤곽은 조립 모드에서 합니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op)) { setStatus("윤곽을 찾을 조각(▣)을 먼저 고르세요.", true); return false; }
  const AlphaMask* mask = alphaMaskOf(*att);
  if (!mask) { setStatus("이 조각의 그림을 읽을 수 없습니다(이미지 에셋을 확인하세요).", true); return false; }
  // The decoded alpha travels with the op: the core traces it (it has no PNG decoder), the same
  // op the service builds from imagePath for MCP.
  const QByteArray bytes(reinterpret_cast<const char*>(mask->alpha.data()), qsizetype(mask->alpha.size()));
  op.update({{"op", "autoOutline"}, {"mask", {{"width", mask->width}, {"height", mask->height}, {"alpha", bytes.toBase64().toStdString()}}},
             {"alphaThreshold", outlineThreshold_->value()}, {"detail", outlineDetail_->value()}});
  if (outlineSpacing_->value() > 0) op["spacing"] = outlineSpacing_->value();
  int hull = 0, vertices = 0, triangles = 0;
  if (!runOps({op}, false, [&](const cutout::EditResult& r) { hull = r.extra.value("hull", 0); vertices = r.extra.value("vertices", 0); triangles = r.extra.value("triangles", 0); })) return false;
  outlineSlot_.clear();
  meshEdit_ = MeshEdit::Vertices;
  refreshMesh();
  setStatus(QString("자동 윤곽으로 메시를 만들었습니다(윤곽 점 %1 · 정점 %2 · 삼각형 %3). 되돌리기: Ctrl+Z").arg(hull).arg(vertices).arg(triangles));
  return true;
}
void CutoutPanel::setMeshTool(int tool) {
  meshTool_ = std::clamp(tool, int(RigCanvas::MeshSelect), int(RigCanvas::MeshAddEdge));
  if (meshEdit_ != MeshEdit::Vertices && !meshSlot().isEmpty() && mode_ == Mode::Setup) meshEdit_ = MeshEdit::Vertices;
  refreshMesh();
}
bool CutoutPanel::addVertexAt(const QPointF& rigPoint) {
  if (mode_ != Mode::Setup) { setStatus("정점 추가는 조립 모드에서 합니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op) || !att->mesh) { setStatus("메시인 조각을 먼저 고르세요.", true); return false; }
  const QVector<QPointF> pts = meshPoints(*rig_, currentPose(false), si);
  if (pts.size() != int(att->vertexCount())) return false;
  auto image = [&](int v) { return QPointF(att->vertices[2 * size_t(v)], att->vertices[2 * size_t(v) + 1]); };
  std::optional<QPointF> at;
  // Near the outline (6 widget px): onto that edge, so the core adds an outline vertex there.
  double best = 6;
  const QPointF w = canvas_->rigToWidget(rigPoint);
  for (const auto& [a, b] : meshOutline(att->triangles)) {
    const QPointF wa = canvas_->rigToWidget(pts[a]), wb = canvas_->rigToWidget(pts[b]), ab = wb - wa;
    const double len2 = QPointF::dotProduct(ab, ab);
    const double t = len2 > 1e-12 ? std::clamp(QPointF::dotProduct(w - wa, ab) / len2, 0.0, 1.0) : 0.0;
    const double d = QLineF(w, wa + ab * t).length();
    if (d < best && t > 0.02 && t < 0.98) { best = d; at = image(a) + (image(b) - image(a)) * t; }
  }
  // Inside: the shown triangle under the click, mapped barycentrically onto the setup shape (exact
  // whatever bones or deform do to the drawn mesh).
  for (size_t t = 0; !at && t + 2 < att->triangles.size(); t += 3) {
    const int i = att->triangles[t], j = att->triangles[t + 1], k = att->triangles[t + 2];
    const QPointF a = pts[i], b = pts[j], c = pts[k];
    const double det = (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
    if (std::fabs(det) < 1e-12) continue;
    const double l1 = ((rigPoint.x() - a.x()) * (c.y() - a.y()) - (rigPoint.y() - a.y()) * (c.x() - a.x())) / det;
    const double l2 = ((b.x() - a.x()) * (rigPoint.y() - a.y()) - (b.y() - a.y()) * (rigPoint.x() - a.x())) / det;
    if (l1 < 0 || l2 < 0 || l1 + l2 > 1) continue;
    at = image(i) + (image(j) - image(i)) * l1 + (image(k) - image(i)) * l2;
  }
  if (!at) { setStatus("메시 밖입니다. 메시 안이나 윤곽 가까이를 클릭하세요.", true); return false; }
  op.update({{"op", "addMeshVertex"}, {"position", {at->x(), at->y()}}});
  int vertex = -1;
  if (!runOps({op}, false, [&](const cutout::EditResult& r) { vertex = r.extra.value("vertex", -1); })) return false;
  meshSelection_ = {vertex};
  refreshMesh();
  setStatus(QString("정점 %1을 추가했습니다(웨이트·변형은 주변에서 섞음). 되돌리기: Ctrl+Z").arg(vertex));
  return true;
}
bool CutoutPanel::deleteVertices() {
  if (mode_ != Mode::Setup) { setStatus("정점 삭제는 조립 모드에서 합니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op) || !att->mesh) { setStatus("메시인 조각을 먼저 고르세요.", true); return false; }
  if (meshSelection_.isEmpty()) { setStatus("지울 정점을 먼저 고르세요.", true); return false; }
  const QVector<int> list = selectedVertices();
  op.update({{"op", "removeMeshVertex"}, {"vertices", std::vector<int>(list.begin(), list.end())}});
  if (!runOps({op}, false)) return false;
  meshSelection_.clear();
  refreshMesh();
  setStatus(QString("정점 %1개를 지웠습니다. 되돌리기: Ctrl+Z").arg(list.size()));
  return true;
}
bool CutoutPanel::addMeshEdge(int a, int b) {
  if (mode_ != Mode::Setup) { setStatus("가장자리는 조립 모드에서 고정합니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op) || !att->mesh) { setStatus("메시인 조각을 먼저 고르세요.", true); return false; }
  op.update({{"op", "addMeshEdge"}, {"edge", {a, b}}});
  if (!runOps({op}, false)) return false;
  setStatus(QString("가장자리 %1-%2를 고정했습니다(하늘색). 되돌리기: Ctrl+Z").arg(a).arg(b));
  return true;
}
bool CutoutPanel::removeMeshEdge() {
  if (mode_ != Mode::Setup) { setStatus("가장자리는 조립 모드에서 풉니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op) || !att->mesh) { setStatus("메시인 조각을 먼저 고르세요.", true); return false; }
  const QVector<int> list = selectedVertices();
  if (list.size() != 2) { setStatus("풀 가장자리의 두 정점을 고르세요(Shift+클릭).", true); return false; }
  op.update({{"op", "removeMeshEdge"}, {"edge", {list[0], list[1]}}});
  if (!runOps({op}, false)) return false;
  setStatus(QString("가장자리 %1-%2 고정을 풀었습니다. 되돌리기: Ctrl+Z").arg(list[0]).arg(list[1]));
  return true;
}
bool CutoutPanel::retriangulateMesh() {
  if (mode_ != Mode::Setup) { setStatus("삼각분할은 조립 모드에서 합니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op) || !att->mesh) { setStatus("메시인 조각을 먼저 고르세요.", true); return false; }
  op["op"] = "retriangulate";
  int triangles = 0;
  if (!runOps({op}, false, [&](const cutout::EditResult& r) { triangles = r.extra.value("triangles", 0); })) return false;
  setStatus(QString("삼각형 %1개로 다시 나눴습니다. 되돌리기: Ctrl+Z").arg(triangles));
  return true;
}

// ---------------------------------------------------------------- clip events (plugin 1.8.0)
namespace {
/** "int 3 · float 2.5 · "left" · 🔊 sfx-step" of a definition, key or notification payload. */
QString payloadText(const Json& p) {
  QStringList parts;
  if (p.contains("int") && p.at("int").is_number()) parts << QString("int %1").arg(p.at("int").get<qint64>());
  if (p.contains("float") && p.at("float").is_number()) parts << QString("float %1").arg(p.at("float").get<double>());
  if (p.contains("string") && p.at("string").is_string()) parts << QString("\"%1\"").arg(qs(p.at("string").get<std::string>()));
  if (p.contains("audio") && p.at("audio").is_string()) parts << "🔊 " + qs(p.at("audio").get<std::string>());
  return parts.join(" · ");
}
}  // namespace

const Json* CutoutPanel::currentEvent() const {
  for (const auto& e : listOf(displayData(), "events")) if (qs(e.value("name", std::string())) == eventId_) return &e;
  return nullptr;
}
void CutoutPanel::refreshEvents() {
  if (!eventList_) return;
  filling_ = true;
  const JsonPtr heldData = displayShared();  // shared, not copied
  const Json& data = *heldData;
  const Json& defs = listOf(data, "events");
  if (!currentEvent()) eventId_ = defs.empty() ? QString() : qs(defs.front().value("name", std::string()));
  {
    QSignalBlocker block(eventList_);
    eventList_->clear();
    for (const auto& e : defs) {
      const QString name = qs(e.value("name", std::string())), payload = payloadText(e);
      auto* item = new QListWidgetItem(payload.isEmpty() ? name : name + " · " + payload, eventList_);
      item->setData(Qt::UserRole, name);
      if (name == eventId_) eventList_->setCurrentItem(item);
    }
  }
  const Json* def = currentEvent();
  auto number = [&](const char* k, double fallback) { return def && def->contains(k) && def->at(k).is_number() ? def->at(k).get<double>() : fallback; };
  eventInt_->setValue(int(number("int", 0)));
  eventInt_->setProperty("shown", eventInt_->value());
  eventFloat_->setValue(number("float", 0));
  eventFloat_->setProperty("shown", eventFloat_->value());
  eventString_->setText(def ? qs(def->value("string", std::string())) : QString());
  eventString_->setProperty("shown", eventString_->text());
  eventVolume_->setValue(number("volume", 1));
  eventVolume_->setProperty("shown", eventVolume_->value());
  eventBalance_->setValue(number("balance", 0));
  eventBalance_->setProperty("shown", eventBalance_->value());
  {
    // Project audio assets; a sound id the project no longer has stays visible (and diagnosed).
    QSignalBlocker block(eventAudio_);
    eventAudio_->clear();
    eventAudio_->addItem("(소리 없음)", QString());
    const QString audio = def ? qs(def->value("audio", std::string())) : QString();
    if (host_)
      for (const auto& v : host_->assets()) {
        const QJsonObject a = v.toObject();
        if (a.value("kind").toString() == "audio") eventAudio_->addItem(a.value("id").toString(), a.value("id").toString());
      }
    if (!audio.isEmpty() && eventAudio_->findData(audio) < 0) eventAudio_->addItem(audio + " (없는 에셋)", audio);
    eventAudio_->setCurrentIndex(std::max(0, eventAudio_->findData(audio)));
  }
  const bool sound = def && def->contains("audio");
  for (QWidget* w : eventEditors_) w->setEnabled(def && ((w != eventVolume_ && w != eventBalance_) || sound));
  int keys = 0, inClip = 0;
  for (const auto& c : listOf(data, "clips"))
    for (const auto& k : listOf(c, "events")) {
      const bool hit = def ? qs(k.value("name", std::string())) == eventId_ : true;
      keys += hit;
      inClip += hit && qs(c.value("id", std::string())) == clipId_;
    }
  eventInfo_->setText(defs.empty()
      ? QString("정의 없음: 키는 아무 이름이나 쓸 수 있습니다(지금 키 %1개). 「새 이벤트」로 정의를 만들면 값과 소리를 정할 수 있습니다.").arg(keys)
      : QString("「%1」 키 %2개 (이 클립 %3개) · 0 / 빈 값은 저장하지 않습니다(키가 덮어쓰지 않으면 0, \"\"로 전달).").arg(eventId_).arg(keys).arg(inClip));
  eventKeyTools_->setVisible(mode_ == Mode::Animate);
  {
    QSignalBlocker block(eventLogList_);
    if (eventLogList_->count() != eventLog_.size()) {
      eventLogList_->clear();
      eventLogList_->addItems(eventLog_);
      eventLogList_->scrollToBottom();
    }
  }
  filling_ = false;
}
bool CutoutPanel::selectEvent(const QString& name) {
  bool found = false;
  for (const auto& e : listOf(displayData(), "events")) found |= qs(e.value("name", std::string())) == name;
  if (!found) { setStatus("없는 이벤트입니다: " + name, true); return false; }
  eventId_ = name;
  refreshEvents();
  return true;
}
bool CutoutPanel::newEvent() {
  if (!rigDoc()) { setStatus("먼저 캐릭터를 만드세요.", true); return false; }
  QStringList names;
  for (const auto& e : listOf(displayData(), "events")) names << qs(e.value("name", std::string()));
  QString suggestion = "event";
  for (int n = 2; names.contains(suggestion); n++) suggestion = QString("event%1").arg(n);
  const QString name = askText("새 이벤트", "이벤트 이름 (영문·숫자·_·-, 예: footstep, hit)", suggestion);
  if (name.isEmpty()) return false;
  QString defined;
  if (!runOps({{{"op", "addEventDef"}, {"name", ss(name)}}}, false, [&](const cutout::EditResult& r) {
        eventId_ = name;
        QStringList also;
        for (const auto& n : r.extra.value("defined", Json::array())) also << qs(n.get<std::string>());
        defined = also.join(", ");
      }))
    return false;
  setStatus(QString("이벤트 「%1」를 만들었습니다%2. 애니메이션 모드에서 「이벤트 키」로 클립에 넣으세요.").arg(name, defined.isEmpty() ? QString() : " (키가 쓰던 " + defined + "도 정의)"));
  return true;
}
bool CutoutPanel::renameEvent() {
  if (!currentEvent()) { setStatus("이름을 바꿀 이벤트를 고르세요.", true); return false; }
  const QString name = askText("이벤트 이름 변경", "새 이름 (모든 클립의 이 이벤트 키도 함께 바뀝니다)", eventId_);
  if (name.isEmpty() || name == eventId_) return false;
  qint64 renamed = 0;
  if (!runOps({{{"op", "renameEventDef"}, {"event", ss(eventId_)}, {"name", ss(name)}}}, false, [&](const cutout::EditResult& r) {
        eventId_ = name;
        renamed = r.extra.value("renamedKeys", 0);
      }))
    return false;
  setStatus(QString("이벤트 이름을 「%1」로 바꿨습니다(키 %2개 함께). 되돌리기: Ctrl+Z").arg(name).arg(renamed));
  return true;
}
bool CutoutPanel::deleteEvent() {
  if (!currentEvent()) { setStatus("삭제할 이벤트를 고르세요.", true); return false; }
  int used = 0;
  for (const auto& c : listOf(displayData(), "clips")) for (const auto& k : listOf(c, "events")) used += qs(k.value("name", std::string())) == eventId_;
  if (used && !askConfirm("이벤트 삭제", QString("이벤트 「%1」를 쓰는 키가 %2개 있습니다. 키까지 함께 삭제할까요?").arg(eventId_).arg(used))) return false;
  const QString name = eventId_;
  if (!runOps({{{"op", "deleteEventDef"}, {"event", ss(name)}, {"force", used > 0}}}, false, [&](const cutout::EditResult&) { eventId_.clear(); })) return false;
  setStatus(QString("이벤트 「%1」%2를 삭제했습니다. 되돌리기: Ctrl+Z").arg(name, used ? QString("와 키 %1개").arg(used) : QString()));
  return true;
}
bool CutoutPanel::setEventField(const QString& field, const Json& value) {
  if (!currentEvent()) { setStatus("값을 정할 이벤트를 고르세요.", true); refreshEvents(); return false; }
  // Spine-like defaults are not stored: 0, "" and "no sound" remove the field.
  const bool empty = value.is_null() || (value.is_number() && (field == "int" || field == "float") && value.get<double>() == 0) ||
                     (value.is_string() && value.get<std::string>().empty());
  const bool ok = runOps({{{"op", "setEventDef"}, {"event", ss(eventId_)}, {ss(field), empty ? Json(nullptr) : value}}}, false);
  if (!ok) refreshEvents();
  return ok;
}
bool CutoutPanel::addEventKey(int ms, const QString& nameIn) {
  if (refusePending()) return false;
  if (mode_ != Mode::Animate || clipId_.isEmpty()) { setStatus("이벤트 키는 애니메이션 모드에서 클립에 넣습니다.", true); return false; }
  const int at = ms < 0 ? playheadMs_ : ms;
  QString name = nameIn.isEmpty() ? eventId_ : nameIn;
  if (name.isEmpty()) {
    // No definitions: any name (the first 「새 이벤트」 later defines it).
    name = askText("이벤트 키", "이벤트 이름 (영문·숫자·_·-)", "event");
    if (name.isEmpty()) return false;
  }
  if (const Json* clip = findIn(displayData(), "clips", ss(clipId_)))
    for (const auto& k : listOf(*clip, "events"))
      if (k.value("timeMs", int64_t(-1)) == at && qs(k.value("name", std::string())) == name) {
        setStatus(QString("%1ms에 이미 「%2」 이벤트 키가 있습니다.").arg(at).arg(name), true);
        return false;
      }
  if (!runOps({{{"op", "setEventKey"}, {"clip", ss(clipId_)}, {"timeMs", at}, {"name", ss(name)}}}, false,
              [this](const cutout::EditResult& r) { selectKeysFrom(r.extra.value("keys", Json::array())); }))
    return false;
  setStatus(QString("이벤트 「%1」 키를 %2ms에 넣었습니다(끌어서 이동, 더블클릭으로 값 편집). 되돌리기: Ctrl+Z").arg(name).arg(at));
  return true;
}
bool CutoutPanel::editEventKey(const KeyRef& key, const EventKeyChoice& choice) {
  if (refusePending()) return false;
  if (key.target != "event" || clipId_.isEmpty()) { setStatus("편집할 이벤트 키를 고르세요.", true); return false; }
  const QString name = choice.name.trimmed().isEmpty() ? key.property : choice.name.trimmed();
  Json op = {{"op", "setEventKey"}, {"clip", ss(clipId_)}, {"timeMs", key.timeMs}, {"name", ss(key.property)}, {"newName", ss(name)},
             {"int", choice.intValue ? Json(*choice.intValue) : Json(nullptr)}, {"float", choice.floatValue ? Json(*choice.floatValue) : Json(nullptr)},
             {"string", choice.stringValue ? Json(ss(*choice.stringValue)) : Json(nullptr)}};
  if (!runOps({op}, false, [this](const cutout::EditResult& r) { selectKeysFrom(r.extra.value("keys", Json::array())); })) return false;
  setStatus(QString("이벤트 키 %1ms를 「%2」(%3)로 바꿨습니다. 되돌리기: Ctrl+Z").arg(key.timeMs).arg(name, choice.intValue || choice.floatValue || choice.stringValue ? "값 덮어쓰기" : "정의의 값"));
  return true;
}
bool CutoutPanel::editEventKeyInteractive(const KeyRef& keyIn) {
  KeyRef key = keyIn;
  if (key.target.isEmpty())
    for (const auto& k : timeView_->selection()) if (k.target == "event") { key = k; break; }
  if (key.target != "event") { setStatus("도프시트 「이벤트」 행에서 편집할 이벤트 키를 고르세요.", true); return false; }
  const Json* clip = findIn(displayData(), "clips", ss(clipId_));
  const Json* found = nullptr;
  if (clip) for (const auto& k : listOf(*clip, "events")) if (!found && k.value("timeMs", int64_t(-1)) == key.timeMs && qs(k.value("name", std::string())) == key.property) found = &k;
  if (!found) { setStatus("그 시간에 이벤트 키가 없습니다.", true); return false; }
  EventKeyChoice choice;
  choice.name = key.property;
  if (found->contains("int") && found->at("int").is_number()) choice.intValue = found->at("int").get<qint64>();
  if (found->contains("float") && found->at("float").is_number()) choice.floatValue = found->at("float").get<double>();
  if (found->contains("string") && found->at("string").is_string()) choice.stringValue = qs(found->at("string").get<std::string>());
  timeView_->setSelection({key});
  if (!askEventKey || !askEventKey(choice)) return false;
  return editEventKey(key, choice);
}
bool CutoutPanel::runEventKeyDialog(EventKeyChoice& choice) {
  const JsonPtr heldData = displayShared();  // shared, not copied
  const Json& data = *heldData;
  const Json& defs = listOf(data, "events");
  QDialog dialog(this);
  dialog.setObjectName("cutoutEventKeyDialog");
  dialog.setWindowTitle("이벤트 키");
  auto* form = new QFormLayout(&dialog);
  auto* name = new QComboBox;
  name->setObjectName("cutoutEventKeyName");
  for (const auto& e : defs) name->addItem(qs(e.value("name", std::string())));
  name->setEditable(defs.empty());  // without definitions any name works
  if (name->findText(choice.name) < 0) name->addItem(choice.name);
  name->setCurrentText(choice.name);
  form->addRow("이벤트", name);
  auto* intOn = new QCheckBox("int");
  intOn->setObjectName("cutoutEventKeyIntOn");
  auto* intValue = new QSpinBox;
  intValue->setObjectName("cutoutEventKeyInt");
  intValue->setRange(-2147483647, 2147483647);
  auto* floatOn = new QCheckBox("float");
  floatOn->setObjectName("cutoutEventKeyFloatOn");
  auto* floatValue = new QDoubleSpinBox;
  floatValue->setObjectName("cutoutEventKeyFloat");
  floatValue->setRange(-1e6, 1e6);
  floatValue->setDecimals(4);
  auto* stringOn = new QCheckBox("string");
  stringOn->setObjectName("cutoutEventKeyStringOn");
  auto* stringValue = new QLineEdit;
  stringValue->setObjectName("cutoutEventKeyString");
  stringValue->setMaxLength(1000);
  auto* defaults = new QLabel;
  defaults->setObjectName("cutoutEventKeyDefaults");
  defaults->setStyleSheet("color:#a7b4c2;");
  // Unchecked fields show (and send) the definition's value: only checked ones are the key's own.
  auto showDefaults = [&defs, defaults, name] {
    Json def = Json::object();
    for (const auto& e : defs) if (qs(e.value("name", std::string())) == name->currentText()) def = e;
    const QString text = payloadText(def);
    defaults->setText("정의의 값: " + (text.isEmpty() ? QString("없음 (0, 0, \"\")") : text));
  };
  showDefaults();
  connect(name, &QComboBox::currentTextChanged, &dialog, [showDefaults] { showDefaults(); });
  intOn->setChecked(choice.intValue.has_value());
  intValue->setValue(int(choice.intValue.value_or(0)));
  floatOn->setChecked(choice.floatValue.has_value());
  floatValue->setValue(choice.floatValue.value_or(0));
  stringOn->setChecked(choice.stringValue.has_value());
  stringValue->setText(choice.stringValue.value_or(QString()));
  for (auto [on, field] : {std::pair<QCheckBox*, QWidget*>{intOn, intValue}, {floatOn, floatValue}, {stringOn, stringValue}}) {
    field->setEnabled(on->isChecked());
    connect(on, &QCheckBox::toggled, field, &QWidget::setEnabled);
    form->addRow(on, field);
  }
  form->addRow(defaults);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  form->addRow(buttons);
  if (dialog.exec() != QDialog::Accepted) return false;
  choice.name = name->currentText().trimmed();
  choice.intValue = intOn->isChecked() ? std::optional<qint64>(intValue->value()) : std::nullopt;
  choice.floatValue = floatOn->isChecked() ? std::optional<double>(floatValue->value()) : std::nullopt;
  choice.stringValue = stringOn->isChecked() ? std::optional<QString>(stringValue->text()) : std::nullopt;
  return true;
}
void CutoutPanel::logEvent(const QString& source, const QString& clip, double timeMs, const QString& name, const Json& payload) {
  const QString payloadPart = payloadText(payload);
  const QString at = timeMs >= 0 ? QString(" %1ms").arg(int(std::floor(timeMs))) : QString();
  const QString line = QString("%1 · %2%3 · %4%5").arg(source, clip, at, name, payloadPart.isEmpty() ? QString() : " (" + payloadPart + ")");
  eventLog_ << line;
  while (eventLog_.size() > 200) eventLog_.removeFirst();
  if (eventFlash_) eventFlash_->setText("◆ " + name);
  if (eventLogList_) {
    QSignalBlocker block(eventLogList_);
    eventLogList_->addItem(line);
    while (eventLogList_->count() > eventLog_.size()) delete eventLogList_->takeItem(0);
    eventLogList_->scrollToBottom();
  }
}
void CutoutPanel::clearEventLog() {
  eventLog_.clear();
  if (eventLogList_) eventLogList_->clear();
  if (eventFlash_) eventFlash_->clear();
}
void CutoutPanel::firePlaybackEvents(double from, double to, bool toInclusive) {
  if (!rig_) return;
  const int ci = rig_->clipIndex(ss(clipId_));
  if (ci < 0) return;
  // Same half-open rule as the runtime: an event at t fires once when playback crosses [t, t+dt).
  for (const auto& e : rig_->clips[size_t(ci)].events) {
    const double t = double(e.timeMs);
    if (t >= from && (t < to || (toInclusive && t == to))) logEvent("재생", clipId_, t, qs(e.name), e.payload);
  }
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
  // Physics preview (plugin 1.13.0): starts settled with playback, stops with it (scrub = no simulation).
  if (playing_) startPhysicsPreview(); else physicsRunning_ = false;
  if (playing_ || bottomTabs_->currentIndex() == 1) timer_->start(); else timer_->stop();
  if (!playing_) { refreshProperties(); refreshModeUi(); updateCanvas(); }
}
void CutoutPanel::tick() {
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  const double dt = std::clamp(double(now - lastTick_), 0.0, 250.0);
  lastTick_ = now;
  advancePlayback(manualClock_ ? 0 : dt);
}
void CutoutPanel::advancePlayback(double dt) {
  dt = std::clamp(dt, 0.0, 250.0);
  if (playing_ && rig_) {
    const int ci = rig_->clipIndex(ss(clipId_));
    if (ci >= 0) {
      const double d = double(rig_->clips[size_t(ci)].durationMs);
      // The editor-only loop range narrows playback to [in, out] (whole clip by default).
      const double lo = timeView_->loopActive() ? timeView_->loopIn() : 0, hi = timeView_->loopActive() ? std::min<double>(timeView_->loopOut(), d) : d;
      bool ended = false;
      // One piece of clip time (the whole frame, or the pieces between physics steps).
      auto advanceClip = [&](double ms) {
        if (ended || !(ms > 0)) return;
        const double from = playheadExact_;
        playheadExact_ += ms;
        // Events fire once per pass over their time (the preview log); scrubbing never gets here.
        if (playheadExact_ > hi) {
          if (loopPreview_->isChecked() && hi > lo) {
            playheadExact_ = lo + std::fmod(playheadExact_ - lo, hi - lo);
            firePlaybackEvents(from, hi, false);
            firePlaybackEvents(lo, playheadExact_, false);
          } else { firePlaybackEvents(from, hi, true); playheadExact_ = hi; ended = true; }
        } else firePlaybackEvents(from, playheadExact_, false);
      };
      const double clipDt = dt * speedCombo_->currentData().toDouble();
      // Physics preview (plugin 1.13.0): fixed steps in clip time, each at its own playhead time.
      if (physicsPreview_ && !rig_->physics.empty()) {
        if (!physicsRunning_) startPhysicsPreview();
        const cutout::Look look = lookFor(*rig_);
        cutout::runPhysics(editorPhysics_, *rig_, clipDt, advanceClip, [&] { return cutout::samplePose(*rig_, ci, cutout::snapMs(playheadExact_), look); });
      } else advanceClip(clipDt);
      if (physicsRunning_) refreshPhysicsInfo();
      if (ended) playButton_->setChecked(false);
      playheadMs_ = int(playheadExact_);
      timeView_->setPlayhead(playheadMs_);
      timeView_->reveal(playheadMs_);
      updateTimeUi(int(d));
    }
  }
  if (bottomTabs_->currentIndex() == 1 && preview_) {
    preview_->update(dt);
    // Presentation only; nothing reaches the game. Clip events go to the preview log.
    for (const auto& n : preview_->takeNotifications())
      if (n.kind == "event") logEvent("게임 연결", qs(n.clip), -1, qs(n.name), n.payload);
  }
  updateCanvas();
}
}  // namespace cutout_studio
