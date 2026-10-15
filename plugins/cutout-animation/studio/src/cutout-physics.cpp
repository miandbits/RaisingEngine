// IK chains + physics authoring (plugin 1.13.0): 사슬 IK from a selected bone run, the IK stretch
// toggles, and the 물리 page (physics constraints with presets, parameters, mix keys and the preview
// simulation). Every authoring gesture is ONE core edit (addIk, setIk, addPhysics, setPhysics,
// setKey ...), so the MCP/LLM `edit` op builds the same documents. The preview simulation is
// editor-only state: it runs while preview playback plays and never while scrubbing.
#include "cutout-panel.hpp"
#include <QtWidgets>
#include <algorithm>
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
QWidget* physicsRow(const char* name) {
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
/** The page's parameter fields: key, label, range, decimals, step, default (the core's), tooltip. */
struct FieldSpec { const char* key; const char* objectName; const char* label; double lo, hi; int decimals; double step, def; const char* tip; };
const QVector<FieldSpec>& fieldSpecs() {
  static const QVector<FieldSpec> specs = {
    {"rotate", "cutoutPhysicsRotate", "회전 흔들림", 0, 1, 2, 0.1, 0, "0..1: 본이 원점을 축으로 흔들리는 정도(머리카락·치마). 지렛대는 본 길이(없으면 50px)."},
    {"x", "cutoutPhysicsX", "가로 흔들림 (x)", 0, 1, 2, 0.1, 0, "0..1: 원점이 가로로 밀리는 정도(장식의 출렁임)."},
    {"y", "cutoutPhysicsY", "세로 흔들림 (y)", 0, 1, 2, 0.1, 0, "0..1: 원점이 세로로 밀리는 정도(통통 튀는 장식)."},
    {"scaleX", "cutoutPhysicsScaleX", "늘어남 (scaleX)", 0, 1, 2, 0.1, 0, "0..1: 본 길이 방향으로 늘었다 줄었다 하는 정도."},
    {"inertia", "cutoutPhysicsInertia", "관성", 0, 1, 2, 0.05, 1, "0..1: 부모가 움직일 때 제자리에 남으려는 정도. 1 = 크게 뒤처짐."},
    {"strength", "cutoutPhysicsStrength", "복원력", 0, 10000, 1, 10, 100, "애니메이션 자세로 되돌아가는 힘(1/s²). 클수록 빠르고 단단하게 돌아옵니다."},
    {"damping", "cutoutPhysicsDamping", "감쇠 (유지율)", 0, 1, 3, 0.01, 0.85, "1/60초마다 남는 속도(0..1). 작을수록 빨리 멈추고, 1이면 멈추지 않습니다."},
    {"mass", "cutoutPhysicsMass", "질량", 0.01, 1000, 2, 0.1, 1, "모든 힘을 나눕니다. 클수록 묵직하게 천천히 움직입니다."},
    {"wind", "cutoutPhysicsWindField", "바람 (px/s²)", -100000, 100000, 0, 50, 0, "+x(오른쪽)로 미는 힘. 미리 보기 바람은 여기에 더해집니다."},
    {"gravity", "cutoutPhysicsGravityField", "중력 (px/s²)", -100000, 100000, 0, 50, 0, "+y(아래)로 당기는 힘. 처음부터 그만큼 처진 채로 시작합니다(정착)."},
    {"mix", "cutoutPhysicsMix", "혼합 (mix)", 0, 1, 3, 0.05, 1, "0 = 물리 없음(애니메이션 그대로) · 1 = 전부. 애니메이션 모드에서는 이 시간의 키가 됩니다."},
    {"limit", "cutoutPhysicsLimit", "전달 한계 (px/s)", 1, 1000000, 0, 500, 5000, "이보다 빠른 움직임도 이 속도만큼만 흔들림을 만듭니다(급한 동작에서 튀는 것 방지)."},
    {"teleport", "cutoutPhysicsTeleport", "순간 이동 (px)", 1, 1000000, 0, 50, 300, "한 단계에 원점이 이보다 멀리 뛰면 날아가지 않고 그 자리에서 다시 정착합니다."},
    {"step", "cutoutPhysicsStepHz", "단계 (Hz)", 10, 240, 0, 10, 60, "고정 시뮬레이션 단계(초당 횟수, 10..240). 화면 프레임과 상관없이 같은 결과가 나옵니다."}};
  return specs;
}
}  // namespace

// ---------------------------------------------------------------- 사슬 IK + stretch
bool CutoutPanel::newChainIk() {
  if (!rigDoc()) { setStatus("먼저 캐릭터를 만드세요.", true); return false; }
  const QStringList picked = selectedBones();
  if (picked.size() < 2) { setStatus("사슬 IK: 이어진 본을 2개 이상 고르세요(Ctrl/Shift+클릭, 「자식 본까지 선택」). 본 하나는 「조준 IK」를 쓰세요.", true); return false; }
  // The run, parent first: the one bone whose parent is not picked, then the picked child each time.
  const Json& data = displayData();
  auto parentOf = [&](const QString& id) {
    const Json* b = findById(data, "bones", ss(id));
    return b && b->contains("parent") && b->at("parent").is_string() ? qs(b->at("parent").get<std::string>()) : QString();
  };
  QStringList tops;
  for (const QString& b : picked) if (!picked.contains(parentOf(b))) tops << b;
  if (tops.size() != 1) { setStatus("사슬 IK: 고른 본이 부모 → 자식으로 한 줄로 이어져야 합니다(맨 위 본이 " + QString::number(tops.size()) + "개).", true); return false; }
  Json bones = Json::array({ss(tops[0])});
  for (QString at = tops[0]; bones.size() < size_t(picked.size());) {
    QStringList next;
    for (const QString& b : picked) if (parentOf(b) == at) next << b;
    if (next.size() != 1) { setStatus(QString("사슬 IK: 「%1」 아래에서 사슬이 %2 갈래로 나뉩니다. 한 줄로 이어진 본만 고르세요.").arg(at).arg(next.size()), true); return false; }
    at = next[0];
    bones.push_back(ss(at));
  }
  QString target;
  const bool ok = runOps({{{"op", "addIk"}, {"bones", bones}, {"createTarget", true}}}, false, [&](const cutout::EditResult& r) {
    ikId_ = qs(r.extra.value("ik", std::string()));
    target = qs(r.extra.value("target", std::string()));
    selection_ = "bone:" + target;
    selected_.clear();
  });
  if (!ok) return false;
  lookTabs_->setCurrentWidget(findChild<QWidget*>("cutoutIkPage"));
  setStatus(QString("사슬 IK 「%1」(본 %2개%3)를 만들었습니다. 캔버스의 ✛ 「%4」를 끌면 사슬 전체가 따라옵니다. 되돌리기: Ctrl+Z")
                .arg(ikId_).arg(bones.size()).arg(bones.size() > 2 ? " · FABRIK" : " · 팔·다리").arg(target));
  return true;
}
bool CutoutPanel::setIkStretch(const QString& flag, bool on) {
  if (!currentIk()) { setStatus("먼저 IK를 고르세요.", true); return false; }
  if (flag != "stretch" && flag != "compress" && flag != "uniform") return false;
  // Not keyable: the constraint's setup value in both modes (one edit).
  if (!runOps({{{"op", "setIk"}, {"ik", ss(ikId_)}, {ss(flag), on}}}, false)) { refreshIk(); return false; }
  const QString what = flag == "stretch" ? "늘이기" : flag == "compress" ? "줄이기" : "균일 크기";
  setStatus(QString("IK 「%1」 %2를 %3. 되돌리기: Ctrl+Z").arg(ikId_, what, on ? "켰습니다" : "껐습니다"));
  return true;
}

// ---------------------------------------------------------------- 물리 page
QWidget* CutoutPanel::buildPhysics() {
  QWidget* page = lookPage("cutoutPhysicsPage");
  page->setProperty("cutoutGroup", "물리");
  auto* layout = static_cast<QVBoxLayout*>(page->layout());
  auto button = [this](QWidget* row, const QString& text, const char* name, const QString& tip, std::function<bool()> fn) {
    auto* b = new QPushButton(text);
    b->setObjectName(name);
    b->setToolTip(tip);
    connect(b, &QPushButton::clicked, this, [fn] { fn(); });
    addTo(row, b);
    return b;
  };
  QWidget* tools = physicsRow("cutoutPhysicsTools");
  physicsPreset_ = new QComboBox;
  physicsPreset_->setObjectName("cutoutPhysicsPreset");
  physicsPreset_->setToolTip("새 물리와 「프리셋 적용」에 쓰는 흔들림 설정: 머리카락(늘어져 따라옴) · 치마(단단히 따라와 빨리 멈춤) · 장식 흔들림(오래 출렁임).");
  for (const auto& p : cutout::physicsPresets()) physicsPreset_->addItem(qs(p.at("label").get<std::string>()), qs(p.at("id").get<std::string>()));
  addTo(tools, physicsPreset_);
  button(tools, "물리 추가", "cutoutAddPhysics", "고른 본마다(여러 개 가능: 머리카락 끝, 치마 자락) 고른 프리셋으로 물리 제약을 만듭니다.\n재생하면 부모의 움직임을 따라 흔들립니다.",
                            [this] { return addPhysics(physicsPreset_->currentData().toString()); });
  QPushButton* preset = button(tools, "프리셋 적용", "cutoutApplyPhysicsPreset", "고른 물리의 흔들림 값을 고른 프리셋으로 바꿉니다(혼합·단계는 그대로).",
                               [this] { return applyPhysicsPreset(physicsPreset_->currentData().toString()); });
  QPushButton* remove = button(tools, "삭제", "cutoutDeletePhysics", "고른 물리 제약과 그 혼합 키를 지웁니다(본은 그대로).", [this] { return deletePhysics(); });
  physicsList_ = new QListWidget;
  physicsList_->setObjectName("cutoutPhysicsList");
  physicsList_->setMinimumHeight(LIST_MIN_HEIGHT);
  physicsList_->setToolTip("물리 제약 · 캔버스에서 청록색 용수철 표시(~)가 붙은 본입니다. 재생하면 점선이 애니메이션 자리에서 흔들린 자리까지 이어집니다.");
  connect(physicsList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
    if (!filling_ && item) selectPhysics(item->data(Qt::UserRole).toString());
  });
  // Preview environment (editor-only): never written into documents.
  QWidget* preview = physicsRow("cutoutPhysicsPreviewTools");
  physicsPreviewButton_ = new QToolButton;
  physicsPreviewButton_->setObjectName("cutoutPhysicsPreview");
  physicsPreviewButton_->setText("재생 중 물리");
  physicsPreviewButton_->setCheckable(true);
  physicsPreviewButton_->setChecked(physicsPreview_);
  physicsPreviewButton_->setToolTip("애니메이션 미리 보기 재생(▶) 중에 물리를 계산해 보여 줍니다. 재생 헤드를 끌어 움직일 때(스크럽)는 계산하지 않습니다(편집기 전용).");
  connect(physicsPreviewButton_, &QToolButton::toggled, this, [this](bool on) { if (!filling_) setPhysicsPreview(on); });
  auto envSpin = [&](const char* name, const QString& tip) {
    auto* s = new QDoubleSpinBox;
    s->setObjectName(name);
    s->setRange(-5000, 5000);
    s->setDecimals(0);
    s->setSingleStep(50);
    s->setKeyboardTracking(false);
    s->setToolTip(tip);
    connect(s, &QDoubleSpinBox::editingFinished, this, [this] { if (!filling_) setPhysicsEnvironment(physicsWindSpin_->value(), physicsGravitySpin_->value()); });
    return s;
  };
  auto* windLabel = new QLabel("바람");
  physicsWindSpin_ = envSpin("cutoutPhysicsWind", "미리 보기 바람(px/s², +x = 오른쪽): 모든 물리 제약에 더해집니다. 편집기에서만 쓰이고 저장되지 않습니다.");
  auto* gravityLabel = new QLabel("중력");
  physicsGravitySpin_ = envSpin("cutoutPhysicsGravity", "미리 보기 중력(px/s², +y = 아래): 모든 물리 제약에 더해집니다. 편집기에서만 쓰이고 저장되지 않습니다.");
  addTo(preview, physicsPreviewButton_);
  addTo(preview, windLabel);
  addTo(preview, physicsWindSpin_);
  addTo(preview, gravityLabel);
  addTo(preview, physicsGravitySpin_);
  QWidget* keyTools = physicsRow("cutoutPhysicsKeyTools");
  QPushButton* key = button(keyTools, "혼합 키", "cutoutPhysicsKey", "애니메이션 모드: 고른 물리의 지금 혼합 값을 이 시간의 키로 넣습니다(0이면 그 구간은 흔들리지 않음).", [this] { return keyPhysicsMix(); });
  button(keyTools, "다시 놓기", "cutoutPhysicsReset", "미리 보기 흔들림을 멈추고 지금 자세에서 다시 정착시킵니다(편집기 전용).", [this] { resetPhysicsPreview(); return true; });
  button(keyTools, "본 선택", "cutoutPhysicsSelectBone", "고른 물리의 본을 선택합니다.", [this] { return selectPhysicsBone(); });
  QPushButton* rename = button(keyTools, "이름 변경", "cutoutRenamePhysics", "물리 ID를 바꿉니다. 이 물리의 혼합 키도 함께 바뀝니다.", [this] { return renamePhysics(); });
  // Parameters (scroll): one spin box per field, editingFinished = one edit.
  auto* inner = new QWidget;
  auto* formLayout = new QVBoxLayout(inner);
  formLayout->setContentsMargins(0, 0, 0, 0);
  formLayout->setSpacing(2);
  auto* form = new QFormLayout;
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
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
      setPhysicsField(field, field == "step" ? 1.0 / spin->value() : spin->value());
    });
    form->addRow(f.label, spin);
    physicsFields_[field] = spin;
    physicsEditors_ << spin;
  }
  physicsResetOnStart_ = new QCheckBox("기본 트랙 클립이 시작될 때 다시 정착");
  physicsResetOnStart_->setObjectName("cutoutPhysicsResetOnStart");
  physicsResetOnStart_->setToolTip("resetOnStart: 0번 트랙에서 새 클립이 시작되면 흔들림을 멈추고 그 자세에서 다시 정착합니다(레이어 트랙은 해당 없음).");
  connect(physicsResetOnStart_, &QCheckBox::clicked, this, [this](bool on) { if (!filling_) setPhysicsResetOnStart(on); });
  form->addRow("다시 정착", physicsResetOnStart_);
  physicsEditors_ << physicsResetOnStart_ << preset << remove << rename;
  physicsInfo_ = new QLabel;
  physicsInfo_->setObjectName("cutoutPhysicsInfo");
  physicsInfo_->setWordWrap(true);
  physicsInfo_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  physicsInfo_->setStyleSheet("color:#a7b4c2;");
  formLayout->addLayout(form);
  formLayout->addWidget(physicsInfo_);
  formLayout->addStretch();
  layout->addWidget(tools);
  layout->addWidget(physicsList_, 2);
  layout->addWidget(preview);
  layout->addWidget(keyTools);
  layout->addWidget(formScroll("cutoutPhysicsFormScroll", inner), 3);
  physicsAnimateTools_ = {key};
  return page;
}

const Json* CutoutPanel::currentPhysics() const { return physicsId_.isEmpty() ? nullptr : findById(displayData(), "physics", ss(physicsId_)); }

void CutoutPanel::refreshPhysics() {
  if (!physicsList_) return;
  const bool wasFilling = filling_;
  filling_ = true;
  const JsonPtr held = displayShared();
  const Json& data = *held;
  const Json& list = listIn(data, "physics");
  if (!physicsId_.isEmpty() && !findById(data, "physics", ss(physicsId_))) physicsId_.clear();
  if (physicsId_.isEmpty() && !list.empty()) physicsId_ = qs(list[0].value("id", std::string()));
  {
    QSignalBlocker block(physicsList_);
    physicsList_->clear();
    for (const auto& k : list) {
      const QString id = qs(k.value("id", std::string())), name = qs(k.value("name", std::string()));
      QStringList channels;
      for (const char* c : {"rotate", "x", "y", "scaleX"}) if (k.value(c, 0.0) > 0) channels << c;
      auto* item = new QListWidgetItem(QString("~ %1%2 · 본 %3 · %4").arg(id, name.isEmpty() || name == id ? QString() : " (" + name + ")",
                                                                       qs(k.value("bone", std::string())), channels.isEmpty() ? QString("채널 없음") : channels.join("·")), physicsList_);
      item->setData(Qt::UserRole, id);
      if (id == physicsId_) physicsList_->setCurrentItem(item);
    }
  }
  const Json* k = currentPhysics();
  const bool animate = mode_ == Mode::Animate;
  const int index = rig_ ? rig_->physicsIndex(ss(physicsId_)) : -1;
  double mix = k ? k->value("mix", 1.0) : 1;
  if (animate && index >= 0) {
    // Animate mode shows the sampled (keyed) mix at the playhead: editing it makes a key.
    const cutout::Pose pose = currentPose(false);
    if (size_t(index) < pose.physicsMix.size()) mix = pose.physicsMix[size_t(index)];
  }
  for (const auto& f : fieldSpecs()) {
    QDoubleSpinBox* spin = physicsFields_.value(f.key);
    QSignalBlocker block(spin);
    double v = k ? k->value(f.key, f.def) : f.def;
    if (QString(f.key) == "mix") v = mix;
    if (QString(f.key) == "step") v = k && k->contains("step") && k->at("step").is_number() && k->at("step").get<double>() > 0 ? 1.0 / k->at("step").get<double>() : 60;
    spin->setValue(v);
    spin->setProperty("shown", spin->value());
  }
  {
    QSignalBlocker block(physicsResetOnStart_);
    physicsResetOnStart_->setChecked(k && k->value("resetOnStart", false));
  }
  for (QWidget* w : physicsEditors_) w->setEnabled(k);
  for (QWidget* w : physicsAnimateTools_) w->setEnabled(k && animate);
  {
    QSignalBlocker b1(physicsPreviewButton_), b2(physicsWindSpin_), b3(physicsGravitySpin_);
    physicsPreviewButton_->setChecked(physicsPreview_);
    physicsWindSpin_->setValue(physicsWind_);
    physicsGravitySpin_->setValue(physicsGravity_);
  }
  refreshPhysicsInfo();
  filling_ = wasFilling;
}

/** The info line: the constraint's bone, step and lever, and while preview playback simulates its
 *  state (refreshed every tick without rebuilding the page). */
void CutoutPanel::refreshPhysicsInfo() {
  if (!physicsInfo_) return;
  const bool animate = mode_ == Mode::Animate;
  const int index = rig_ ? rig_->physicsIndex(ss(physicsId_)) : -1;
  const Json* k = currentPhysics();
  QString info = "흔들릴 본(머리카락 끝·치마 자락·장식)을 고르고 프리셋을 정한 뒤 「물리 추가」를 누르세요. 여러 본을 고르면 본마다 하나씩 만듭니다. "
                 "▶ 재생하면 부모의 움직임을 따라 흔들립니다(스크럽 중에는 계산하지 않음).";
  if (k && index >= 0) {
    const auto& c = rig_->physics[size_t(index)];
    info = QString("본 %1 · %2Hz 고정 단계 · 지렛대 %3px%4 · %5")
               .arg(qs(rig_->bones[size_t(c.bone)].id)).arg(std::round(1000 / c.stepMs)).arg(c.lever, 0, 'g', 4)
               .arg(rig_->bones[size_t(c.bone)].length > 0 ? QString() : QString(" (길이 없는 본: 기본값)"))
               .arg(animate ? "애니메이션 모드: 혼합을 바꾸면 이 시간의 키가 되고, 다른 값은 물리 설정입니다." : "조립 모드: 값은 물리의 기본 설정입니다.");
    if (physicsRunning_ && size_t(index) < editorPhysics_.bodies.size()) {
      const auto& b = editorPhysics_.bodies[size_t(index)];
      info += QString(" · 미리 보기: %1단계, 회전 %2°, 위치 (%3, %4)px").arg(b.steps).arg(b.r * 180 / 3.14159265358979323846, 0, 'f', 1).arg(b.ox, 0, 'f', 1).arg(b.oy, 0, 'f', 1);
    }
  }
  physicsInfo_->setText(info);
}
bool CutoutPanel::addPhysics(const QString& preset) {
  if (!rigDoc()) { setStatus("먼저 캐릭터를 만드세요.", true); return false; }
  const QStringList bones = selectedBones(true);
  if (bones.isEmpty()) { setStatus("흔들릴 본(머리카락 끝, 치마 자락, 장식)을 먼저 고르세요. 여러 개를 고르면 본마다 만듭니다.", true); return false; }
  Json list = Json::array();
  for (const QString& b : bones) list.push_back(ss(b));
  Json op = {{"op", "addPhysics"}, {"bones", list}};
  if (!preset.isEmpty()) op["preset"] = ss(preset);
  QStringList made;
  const bool ok = runOps({op}, false, [&](const cutout::EditResult& r) {
    for (const auto& id : r.extra.value("physics", Json::array())) made << qs(id.get<std::string>());
    if (!made.isEmpty()) physicsId_ = made.back();
  });
  if (!ok) return false;
  lookTabs_->setCurrentWidget(findChild<QWidget*>("cutoutPhysicsPage"));
  setStatus(QString("물리 %1개(%2)를 만들었습니다: %3. ▶ 재생하면 흔들립니다. 되돌리기: Ctrl+Z").arg(made.size()).arg(physicsPreset_ ? physicsPreset_->currentText() : preset, made.join(", ")));
  return true;
}
bool CutoutPanel::selectPhysics(const QString& id) {
  if (!findById(displayData(), "physics", ss(id))) { setStatus("없는 물리입니다: " + id, true); return false; }
  physicsId_ = id;
  refreshPhysics();
  updateCanvas();
  return true;
}
bool CutoutPanel::applyPhysicsPreset(const QString& preset) {
  if (!currentPhysics()) { setStatus("먼저 물리를 고르세요.", true); return false; }
  if (!runOps({{{"op", "setPhysics"}, {"physics", ss(physicsId_)}, {"preset", ss(preset)}}}, false)) return false;
  setStatus(QString("물리 「%1」에 프리셋 「%2」를 적용했습니다. 되돌리기: Ctrl+Z").arg(physicsId_, physicsPreset_ ? physicsPreset_->itemText(physicsPreset_->findData(preset)) : preset));
  return true;
}
bool CutoutPanel::setPhysicsField(const QString& field, double value) {
  if (!currentPhysics()) { setStatus("먼저 물리를 만들거나 고르세요.", true); refreshPhysics(); return false; }
  if (field == "mix" && mode_ == Mode::Animate) {
    if (clipId_.isEmpty()) { setStatus("키를 넣을 클립을 먼저 만드세요.", true); refreshPhysics(); return false; }
    return runOps({{{"op", "setKey"}, {"clip", ss(clipId_)}, {"target", "physics:" + ss(physicsId_)}, {"property", "mix"}, {"timeMs", playheadMs_}, {"value", value}}}, true);
  }
  // A default value drops the key (documents keep only what differs, like the core's writeT).
  Json v = value;
  for (const auto& f : fieldSpecs())
    if (field == f.key && field != "step" && std::fabs(value - f.def) < 1e-12) v = nullptr;
  if (field == "step" && std::fabs(value - 1.0 / 60) < 1e-12) v = nullptr;
  if (!runOps({{{"op", "setPhysics"}, {"physics", ss(physicsId_)}, {ss(field), v}}}, false)) { refreshPhysics(); return false; }
  return true;
}
bool CutoutPanel::setPhysicsResetOnStart(bool on) {
  if (!currentPhysics()) { setStatus("먼저 물리를 고르세요.", true); return false; }
  return runOps({{{"op", "setPhysics"}, {"physics", ss(physicsId_)}, {"resetOnStart", on}}}, false);
}
bool CutoutPanel::renamePhysics() {
  if (!currentPhysics()) { setStatus("이름을 바꿀 물리를 고르세요.", true); return false; }
  const QString id = askText("물리 이름 변경", "새 물리 ID (이 물리의 혼합 키도 함께 바뀝니다)", physicsId_);
  if (id.isEmpty() || id == physicsId_) return false;
  return runOps({{{"op", "renamePhysics"}, {"physics", ss(physicsId_)}, {"id", ss(id)}}}, false, [&](const cutout::EditResult&) { physicsId_ = id; });
}
bool CutoutPanel::deletePhysics() {
  if (!currentPhysics()) { setStatus("삭제할 물리를 고르세요.", true); return false; }
  if (!askConfirm("물리 삭제", QString("물리 「%1」와 그 혼합 키를 삭제할까요? 본은 그대로 남습니다.").arg(physicsId_))) return false;
  const QString id = physicsId_;
  return runOps({{{"op", "deletePhysics"}, {"physics", ss(id)}}}, false, [&](const cutout::EditResult&) { physicsId_.clear(); });
}
bool CutoutPanel::keyPhysicsMix() {
  if (mode_ != Mode::Animate) { setStatus("물리 혼합 키는 애니메이션 모드에서 넣습니다.", true); return false; }
  if (clipId_.isEmpty()) { setStatus("키를 넣을 클립을 먼저 만드세요.", true); return false; }
  if (!currentPhysics() || !rig_) { setStatus("키를 넣을 물리를 고르세요.", true); return false; }
  const int index = rig_->physicsIndex(ss(physicsId_));
  const cutout::Pose pose = currentPose(false);
  if (index < 0 || size_t(index) >= pose.physicsMix.size()) return false;
  return runOps({{{"op", "setKey"}, {"clip", ss(clipId_)}, {"target", "physics:" + ss(physicsId_)}, {"property", "mix"}, {"timeMs", playheadMs_}, {"value", pose.physicsMix[size_t(index)]}}}, false);
}
bool CutoutPanel::selectPhysicsBone() {
  const Json* k = currentPhysics();
  if (!k) { setStatus("먼저 물리를 고르세요.", true); return false; }
  selectItem("bone:" + qs(k->value("bone", std::string())));
  return true;
}

// ---------------------------------------------------------------- preview simulation (editor-only)
void CutoutPanel::startPhysicsPreview() {
  physicsRunning_ = false;
  if (!rig_ || rig_->physics.empty() || !physicsPreview_ || mode_ != Mode::Animate || !playing_) return;
  editorPhysics_.init(*rig_);
  editorPhysics_.wind = physicsWind_;
  editorPhysics_.gravity = physicsGravity_;
  cutout::Look look = lookFor(*rig_);
  editorPhysics_.settle(*rig_, cutout::samplePose(*rig_, rig_->clipIndex(ss(clipId_)), cutout::snapMs(playheadExact_), look));
  physicsRunning_ = true;
}
void CutoutPanel::setPhysicsPreview(bool on) {
  physicsPreview_ = on;
  if (on) startPhysicsPreview();
  else physicsRunning_ = false;
  refreshPhysics();
  updateCanvas();
}
void CutoutPanel::setPhysicsEnvironment(double wind, double gravity) {
  physicsWind_ = wind;
  physicsGravity_ = gravity;
  editorPhysics_.wind = wind;
  editorPhysics_.gravity = gravity;
  if (preview_) preview_->setPhysicsEnvironment(wind, gravity);  // the game-connection preview too
  setStatus(QString("미리 보기 바람 %1, 중력 %2 px/s² (편집기 전용, 저장되지 않음).").arg(wind).arg(gravity));
  refreshPhysics();
}
void CutoutPanel::resetPhysicsPreview() {
  if (physicsRunning_) startPhysicsPreview();
  if (preview_) preview_->resetPhysics();
  refreshPhysics();
  updateCanvas();
}
}  // namespace cutout_studio
