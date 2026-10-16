// Transform constraints and bone inheritance authoring (plugin 1.16.0): the 변형 제약 page (list,
// 제약 추가 from the bone selection, channel mixes / offsets / local / relative / order, mix keys)
// and the 상속 combo of the bone properties. Every gesture is ONE core edit (addTransform,
// setTransform, renameTransform, deleteTransform, setKey on transform:<id>, setBone inherit), so
// the MCP/LLM `edit` op builds the same documents and undo/redo has one step per gesture.
#include "cutout-panel.hpp"
#include <QtWidgets>
#include <algorithm>
#include <array>
#include <cmath>
#include "cutout/editing.hpp"

namespace cutout_studio {
using cutout::Json;
namespace {
const Json* findById(const Json& data, const char* list, const std::string& id) {
  if (!data.is_object() || !data.contains(list) || !data.at(list).is_array()) return nullptr;
  for (const auto& x : data.at(list)) if (x.is_object() && x.value("id", std::string()) == id) return &x;
  return nullptr;
}
const Json& listIn(const Json& data, const char* key) {
  static const Json empty = Json::array();
  return data.is_object() && data.contains(key) && data.at(key).is_array() ? data.at(key) : empty;
}
QWidget* toolRow(const char* name) {
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
/** The page's fields: key, objectName, label, range, decimals, step, default (the core's), tooltip. */
struct FieldSpec { const char* key; const char* objectName; const char* label; double lo, hi; int decimals; double step, def; const char* tip; };
const QVector<FieldSpec>& fieldSpecs() {
  static const QVector<FieldSpec> specs = {
    {"mixRotate", "cutoutTransformMixRotate", "회전 혼합", 0, 1, 3, 0.05, 1, "0..1: 대상의 회전을 따르는 정도. 애니메이션 모드에서는 이 시간의 키가 됩니다."},
    {"mixX", "cutoutTransformMixX", "위치 X 혼합", 0, 1, 3, 0.05, 1, "0..1: 대상의 위치(x)를 따르는 정도."},
    {"mixY", "cutoutTransformMixY", "위치 Y 혼합", 0, 1, 3, 0.05, 1, "0..1: 대상의 위치(y)를 따르는 정도."},
    {"mixScaleX", "cutoutTransformMixScaleX", "크기 X 혼합", 0, 1, 3, 0.05, 0, "0..1: 대상의 가로 크기를 따르는 정도(기본 0 = 제 크기 유지)."},
    {"mixScaleY", "cutoutTransformMixScaleY", "크기 Y 혼합", 0, 1, 3, 0.05, 0, "0..1: 대상의 세로 크기를 따르는 정도(기본 0)."},
    {"mixShearY", "cutoutTransformMixShearY", "기울이기 혼합", 0, 1, 3, 0.05, 0, "0..1: 대상의 y축 기울기를 따르는 정도(기본 0)."},
    {"offsetRotation", "cutoutTransformOffsetRotation", "회전 오프셋°", -36000, 36000, 2, 5, 0, "대상의 회전에 더하는 각도(도)."},
    {"offsetX", "cutoutTransformOffsetX", "위치 오프셋 X", -100000, 100000, 2, 1, 0, "월드: 대상 공간의 px(대상과 함께 돕니다) · 로컬: 대상 x에 더하는 값."},
    {"offsetY", "cutoutTransformOffsetY", "위치 오프셋 Y", -100000, 100000, 2, 1, 0, "월드: 대상 공간의 px · 로컬: 대상 y에 더하는 값."},
    {"offsetScaleX", "cutoutTransformOffsetScaleX", "크기 오프셋 X", -1000, 1000, 3, 0.05, 0, "대상의 가로 크기에 더하는 값."},
    {"offsetScaleY", "cutoutTransformOffsetScaleY", "크기 오프셋 Y", -1000, 1000, 3, 0.05, 0, "대상의 세로 크기에 더하는 값."},
    {"offsetShearY", "cutoutTransformOffsetShearY", "기울이기 오프셋°", -36000, 36000, 2, 5, 0, "대상의 y축 기울기에 더하는 각도(도)."},
    {"order", "cutoutTransformOrder", "순서", -1000, 1000, 0, 1, 0, "IK와 함께 작은 순서부터 풉니다(같으면 IK 먼저). 다른 제약이 움직이는 본을 대상으로 쓰면 더 크게 두세요."}};
  return specs;
}
bool isMix(const QString& field) { return field.startsWith("mix"); }
QString refKind(const QString& ref) { return ref.section(':', 0, 0); }
QString refId(const QString& ref) { return ref.section(':', 1); }
}  // namespace

// ---------------------------------------------------------------- 변형 제약 page
QWidget* CutoutPanel::buildTransform() {
  QWidget* page = lookPage("cutoutTransformPage");
  page->setProperty("cutoutGroup", "변형 제약");
  auto* layout = static_cast<QVBoxLayout*>(page->layout());
  auto button = [this](QWidget* row, const QString& text, const char* name, const QString& tip, std::function<bool()> fn) {
    auto* b = new QPushButton(text);
    b->setObjectName(name);
    b->setToolTip(tip);
    connect(b, &QPushButton::clicked, this, [fn] { fn(); });
    addTo(row, b);
    return b;
  };
  QWidget* tools = toolRow("cutoutTransformTools");
  button(tools, "제약 추가", "cutoutAddTransform",
         "고른 본들이 대상 본을 따라 움직이게 합니다: 본을 2개 이상 고르면 마지막에 고른 본이 대상, 1개면 대상 본 ID를 묻습니다.\n"
         "지금 자세는 그대로 두고(오프셋 자동), 대상이 움직이는 만큼 따라옵니다.", [this] { return addTransform(); });
  QPushButton* remove = button(tools, "삭제", "cutoutDeleteTransform", "고른 변형 제약과 그 혼합 키를 지웁니다(본은 그대로).", [this] { return deleteTransform(); });
  QPushButton* rename = button(tools, "이름 변경", "cutoutRenameTransform", "변형 제약 ID를 바꿉니다. 이 제약의 혼합 키도 함께 바뀝니다.", [this] { return renameTransform(); });
  transformList_ = new QListWidget;
  transformList_->setObjectName("cutoutTransformList");
  transformList_->setMinimumHeight(LIST_MIN_HEIGHT);
  transformList_->setToolTip("변형 제약 · 캔버스에서 제약된 본에서 대상 본까지 분홍 점선으로 이어집니다(고른 제약은 굵게).");
  connect(transformList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
    if (!filling_ && item) selectTransform(item->data(Qt::UserRole).toString());
  });
  QWidget* keyTools = toolRow("cutoutTransformKeyTools");
  QPushButton* key = button(keyTools, "혼합 키", "cutoutTransformKey", "애니메이션 모드: 고른 제약의 혼합(0이 아니거나 이미 키가 있는 채널)을 이 시간의 키로 넣습니다.", [this] { return keyTransformMixes(); });
  QPushButton* target = button(keyTools, "대상 선택", "cutoutTransformSelectTarget", "고른 제약의 대상 본을 선택합니다.", [this] { return selectTransformTarget(); });
  QPushButton* bones = button(keyTools, "본 선택", "cutoutTransformSelectBones", "고른 제약이 움직이는 본들을 선택합니다.", [this] { return selectTransformBones(); });
  auto* inner = new QWidget;
  auto* formLayout = new QVBoxLayout(inner);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setSpacing(2);
  auto* form = new QFormLayout;
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  transformLocal_ = new QCheckBox("로컬 값으로");
  transformLocal_->setObjectName("cutoutTransformLocal");
  transformLocal_->setToolTip("켜면 월드 행렬 대신 로컬 값(부모 기준 x·y·회전·크기·기울이기)을 맞춥니다.");
  connect(transformLocal_, &QCheckBox::clicked, this, [this](bool on) { if (!filling_) setTransformFlag("local", on); });
  transformRelative_ = new QCheckBox("상대 (대상 값을 더함)");
  transformRelative_->setObjectName("cutoutTransformRelative");
  transformRelative_->setToolTip("켜면 대상 쪽으로 옮기는 대신 대상의 값(회전·위치·크기·기울이기)을 본의 값에 더합니다.");
  connect(transformRelative_, &QCheckBox::clicked, this, [this](bool on) { if (!filling_) setTransformFlag("relative", on); });
  form->addRow("방식", transformLocal_);
  form->addRow("", transformRelative_);
  for (const auto& f : fieldSpecs()) {
    auto* spin = new QDoubleSpinBox;
    spin->setObjectName(f.objectName);
    spin->setRange(f.lo, f.hi);
    spin->setDecimals(f.decimals);
    spin->setSingleStep(f.step);
    spin->setKeyboardTracking(false);
    spin->setToolTip(f.tip);
    const QString field = f.key;
    connect(spin, &QDoubleSpinBox::editingFinished, this, [this, spin, field] {
      if (filling_ || std::fabs(spin->value() - spin->property("shown").toDouble()) < 0.5 * std::pow(10.0, -spin->decimals())) return;
      setTransformField(field, spin->value());
    });
    form->addRow(f.label, spin);
    transformFields_[field] = spin;
    transformEditors_ << spin;
  }
  transformEditors_ << transformLocal_ << transformRelative_ << remove << rename << target << bones;
  transformInfo_ = new QLabel;
  transformInfo_->setObjectName("cutoutTransformInfo");
  transformInfo_->setWordWrap(true);
  transformInfo_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  transformInfo_->setStyleSheet("color:#a7b4c2;");
  formLayout->addLayout(form);
  formLayout->addWidget(transformInfo_);
  formLayout->addStretch();
  layout->addWidget(tools);
  layout->addWidget(transformList_, 2);
  layout->addWidget(keyTools);
  layout->addWidget(formScroll("cutoutTransformFormScroll", inner), 3);
  transformAnimateTools_ = {key};
  return page;
}

const Json* CutoutPanel::currentTransform() const { return transformId_.isEmpty() ? nullptr : findById(displayData(), "transform", ss(transformId_)); }

void CutoutPanel::refreshTransform() {
  if (!transformList_) return;
  const bool wasFilling = filling_;
  filling_ = true;
  const JsonPtr held = displayShared();
  const Json& data = *held;
  const Json& list = listIn(data, "transform");
  if (!transformId_.isEmpty() && !findById(data, "transform", ss(transformId_))) transformId_.clear();
  if (transformId_.isEmpty() && !list.empty()) transformId_ = qs(list[0].value("id", std::string()));
  {
    QSignalBlocker block(transformList_);
    transformList_->clear();
    for (const auto& k : list) {
      const QString id = qs(k.value("id", std::string())), name = qs(k.value("name", std::string()));
      QStringList bones;
      for (const auto& b : listIn(k, "bones")) if (b.is_string()) bones << qs(b.get<std::string>());
      const QString mode = QString(k.value("local", false) ? "로컬" : "월드") + (k.value("relative", false) ? "·상대" : "");
      auto* item = new QListWidgetItem(QString("⇢ %1%2 · %3 → %4 · %5").arg(id, name.isEmpty() || name == id ? QString() : " (" + name + ")",
                                                                         bones.join(", "), qs(k.value("target", std::string())), mode), transformList_);
      item->setData(Qt::UserRole, id);
      if (id == transformId_) transformList_->setCurrentItem(item);
    }
  }
  const Json* k = currentTransform();
  const bool animate = mode_ == Mode::Animate;
  const int index = rig_ ? rig_->transformIndex(ss(transformId_)) : -1;
  // Animate mode shows the sampled (keyed) mixes at the playhead: editing one makes a key.
  std::array<double, cutout::TRANSFORM_MIXES> sampled{};
  bool haveSampled = false;
  if (animate && index >= 0) {
    const cutout::Pose pose = currentPose(false);
    if (size_t(index) < pose.transformMix.size()) { sampled = pose.transformMix[size_t(index)]; haveSampled = true; }
  }
  int m = 0;
  for (const auto& f : fieldSpecs()) {
    QDoubleSpinBox* spin = transformFields_.value(f.key);
    QSignalBlocker block(spin);
    double v = k ? k->value(f.key, f.def) : f.def;
    if (isMix(f.key) && haveSampled) v = sampled[size_t(m)];
    if (isMix(f.key)) m++;
    spin->setValue(v);
    spin->setProperty("shown", spin->value());
  }
  {
    QSignalBlocker b1(transformLocal_), b2(transformRelative_);
    transformLocal_->setChecked(k && k->value("local", false));
    transformRelative_->setChecked(k && k->value("relative", false));
  }
  for (QWidget* w : transformEditors_) w->setEnabled(k);
  for (QWidget* w : transformAnimateTools_) w->setEnabled(k && animate);
  QString info = "따라갈 본들을 고르고 마지막에 대상 본을 Ctrl+클릭한 뒤 「제약 추가」를 누르세요(본 하나만 고르면 대상 ID를 묻습니다). "
                 "기본은 대상의 위치·회전을 따르고 크기·기울이기는 그대로입니다.";
  if (k && index >= 0) {
    const auto& c = rig_->transforms[size_t(index)];
    int place = 0;
    for (size_t i = 0; i < rig_->constraintOrder.size(); i++) if (rig_->constraintOrder[i].transform && rig_->constraintOrder[i].index == index) place = int(i) + 1;
    info = QString("본 %1개가 「%2」를 따릅니다 · %3 · 제약 풀이 순서 %4/%5 (IK 포함, 물리는 그 뒤)%6")
               .arg(c.bones.size()).arg(qs(rig_->bones[size_t(c.target)].id))
               .arg(c.local ? (c.relative ? "로컬 값에 대상 값을 더함" : "로컬 값을 대상 쪽으로") : (c.relative ? "월드에 대상 변형을 더함" : "월드 자세를 대상 쪽으로"))
               .arg(place).arg(rig_->constraintOrder.size())
               .arg(animate ? " · 애니메이션 모드: 혼합을 바꾸면 이 시간의 키가 되고, 다른 값은 제약 설정입니다." : QString());
  }
  transformInfo_->setText(info);
  filling_ = wasFilling;
}

bool CutoutPanel::addTransform() {
  if (!rigDoc()) { setStatus("먼저 캐릭터를 만드세요.", true); return false; }
  QStringList bones = selectedBones();
  if (bones.isEmpty()) { setStatus("따라 움직일 본을 먼저 고르세요(대상 본은 마지막에 Ctrl+클릭).", true); return false; }
  QString target;
  if (bones.size() >= 2 && refKind(selection_) == "bone" && bones.contains(refId(selection_))) target = refId(selection_);
  else if (bones.size() == 1) {
    target = askText("변형 제약 추가", QString("본 「%1」이 따라갈 대상 본 ID").arg(bones[0]), QString());
    if (target.isEmpty()) return false;
    if (!findById(displayData(), "bones", ss(target))) { setStatus("없는 본입니다: " + target, true); return false; }
  } else { setStatus("대상 본을 마지막에 고르세요(Ctrl+클릭).", true); return false; }
  bones.removeAll(target);
  if (bones.isEmpty()) { setStatus("대상과 다른 본을 하나 이상 고르세요.", true); return false; }
  Json list = Json::array();
  for (const QString& b : bones) list.push_back(ss(b));
  QString made;
  const bool ok = runOps({{{"op", "addTransform"}, {"bones", list}, {"target", ss(target)}, {"keepPose", true}}}, false, [&](const cutout::EditResult& r) {
    made = qs(r.extra.value("transform", std::string()));
    transformId_ = made;
  });
  if (!ok) return false;
  lookTabs_->setCurrentWidget(findChild<QWidget*>("cutoutTransformPage"));
  setStatus(QString("변형 제약 「%1」: %2이(가) 「%3」을 따릅니다(지금 자세 유지 오프셋). 대상을 움직이면 따라옵니다. 되돌리기: Ctrl+Z").arg(made, bones.join(", "), target));
  return true;
}
bool CutoutPanel::selectTransform(const QString& id) {
  if (!findById(displayData(), "transform", ss(id))) { setStatus("없는 변형 제약입니다: " + id, true); return false; }
  transformId_ = id;
  refreshTransform();
  updateCanvas();
  return true;
}
bool CutoutPanel::setTransformField(const QString& field, double value) {
  if (!currentTransform()) { setStatus("먼저 변형 제약을 만들거나 고르세요.", true); refreshTransform(); return false; }
  if (isMix(field) && mode_ == Mode::Animate) {
    if (clipId_.isEmpty()) { setStatus("키를 넣을 클립을 먼저 만드세요.", true); refreshTransform(); return false; }
    return runOps({{{"op", "setKey"}, {"clip", ss(clipId_)}, {"target", "transform:" + ss(transformId_)}, {"property", ss(field)}, {"timeMs", playheadMs_}, {"value", value}}}, true);
  }
  if (!runOps({{{"op", "setTransform"}, {"transform", ss(transformId_)}, {ss(field), field == "order" ? Json(int64_t(std::llround(value))) : Json(value)}}}, false)) { refreshTransform(); return false; }
  return true;
}
bool CutoutPanel::setTransformFlag(const QString& flag, bool on) {
  if (!currentTransform()) { setStatus("먼저 변형 제약을 고르세요.", true); refreshTransform(); return false; }
  if (flag != "local" && flag != "relative") return false;
  // The offsets keep their meaning per mode: switching keeps the shown pose (keepPose) in one edit.
  if (!runOps({{{"op", "setTransform"}, {"transform", ss(transformId_)}, {ss(flag), on}, {"keepPose", true}}}, false)) { refreshTransform(); return false; }
  setStatus(QString("변형 제약 「%1」: %2 %3(지금 자세 유지 오프셋). 되돌리기: Ctrl+Z").arg(transformId_, flag == "local" ? "로컬 값" : "상대", on ? "켬" : "끔"));
  return true;
}
bool CutoutPanel::renameTransform() {
  if (!currentTransform()) { setStatus("이름을 바꿀 변형 제약을 고르세요.", true); return false; }
  const QString id = askText("변형 제약 이름 변경", "새 변형 제약 ID (이 제약의 혼합 키도 함께 바뀝니다)", transformId_);
  if (id.isEmpty() || id == transformId_) return false;
  return runOps({{{"op", "renameTransform"}, {"transform", ss(transformId_)}, {"id", ss(id)}}}, false, [&](const cutout::EditResult&) { transformId_ = id; });
}
bool CutoutPanel::deleteTransform() {
  if (!currentTransform()) { setStatus("삭제할 변형 제약을 고르세요.", true); return false; }
  if (!askConfirm("변형 제약 삭제", QString("변형 제약 「%1」와 그 혼합 키를 삭제할까요? 본은 그대로 남습니다.").arg(transformId_))) return false;
  const QString id = transformId_;
  return runOps({{{"op", "deleteTransform"}, {"transform", ss(id)}}}, false, [&](const cutout::EditResult&) { transformId_.clear(); });
}
bool CutoutPanel::keyTransformMixes() {
  if (mode_ != Mode::Animate) { setStatus("변형 제약 혼합 키는 애니메이션 모드에서 넣습니다.", true); return false; }
  if (clipId_.isEmpty()) { setStatus("키를 넣을 클립을 먼저 만드세요.", true); return false; }
  if (!currentTransform() || !rig_) { setStatus("키를 넣을 변형 제약을 고르세요.", true); return false; }
  const int index = rig_->transformIndex(ss(transformId_));
  const cutout::Pose pose = currentPose(false);
  if (index < 0 || size_t(index) >= pose.transformMix.size()) return false;
  const std::string target = "transform:" + ss(transformId_);
  const Json* clip = findById(displayData(), "clips", ss(clipId_));
  std::vector<Json> ops;
  int m = 0;
  for (const auto& f : fieldSpecs()) {
    if (!isMix(f.key)) continue;
    const double v = pose.transformMix[size_t(index)][size_t(m++)];
    bool keyed = false;
    if (clip) for (const auto& t : listIn(*clip, "tracks")) keyed = keyed || (t.value("target", std::string()) == target && t.value("property", std::string()) == f.key);
    if (v > 0 || keyed) ops.push_back({{"op", "setKey"}, {"clip", ss(clipId_)}, {"target", target}, {"property", f.key}, {"timeMs", playheadMs_}, {"value", v}});
  }
  if (ops.empty()) { setStatus("혼합이 모두 0이라 넣을 키가 없습니다.", true); return false; }
  if (!runOps(ops, false)) return false;
  setStatus(QString("변형 제약 「%1」의 혼합 %2개를 %3ms에 키로 넣었습니다. 되돌리기: Ctrl+Z").arg(transformId_).arg(ops.size()).arg(playheadMs_));
  return true;
}
bool CutoutPanel::selectTransformTarget() {
  const Json* k = currentTransform();
  if (!k) { setStatus("먼저 변형 제약을 고르세요.", true); return false; }
  selectItem("bone:" + qs(k->value("target", std::string())));
  return true;
}
bool CutoutPanel::selectTransformBones() {
  const Json* k = currentTransform();
  if (!k) { setStatus("먼저 변형 제약을 고르세요.", true); return false; }
  QStringList refs;
  for (const auto& b : listIn(*k, "bones")) if (b.is_string()) refs << "bone:" + qs(b.get<std::string>());
  if (refs.isEmpty()) return false;
  setSelection(refs, refs.back());
  return true;
}

// ---------------------------------------------------------------- 상속 (bone properties)
bool CutoutPanel::setBoneInherit(const QString& inherit) {
  if (mode_ == Mode::Animate) { setStatus("상속은 조립 모드에서 바꿉니다(키가 아닙니다).", true); refreshProperties(); return false; }
  const QStringList bones = selectedBones();
  if (bones.isEmpty()) { setStatus("상속을 바꿀 본을 고르세요.", true); return false; }
  if (!cutout::parseInherit(ss(inherit))) return false;
  std::vector<Json> ops;
  for (const QString& b : bones) ops.push_back({{"op", "setBone"}, {"bone", ss(b)}, {"inherit", inherit == "normal" ? Json(nullptr) : Json(ss(inherit))}});
  if (!runOps(ops, false)) { refreshProperties(); return false; }
  setStatus(QString("본 %1개의 상속을 「%2」(으)로 바꿨습니다. 되돌리기: Ctrl+Z").arg(bones.size()).arg(inheritCombo_ ? inheritCombo_->currentText() : inherit));
  return true;
}
}  // namespace cutout_studio
