// Weight painting and linked meshes authoring (plugin 1.18.0) on the 메시 page: brush modes
// (더하기 · 빼기 · 바꾸기 · 부드럽게, Shift / Ctrl for one stroke), falloff, bone locks, the weight
// table of the selected vertices, 정리 (prune + max influences), 좌우 복사, 메시에서 복사, the
// editor-only 자세 미리 보기, and 링크드 메시 만들기 / 연결 끊기. Every finished gesture is ONE core
// edit (paintWeights, normalizeWeights, mirrorWeights, copyWeights, linkMesh, unlinkMesh), so the
// MCP/LLM `edit` op builds the same documents and undo has one step per gesture. Locks, the brush
// and the preview pose are editor state: they never reach documents.
#include "cutout-panel.hpp"
#include <QtWidgets>
#include <algorithm>
#include <cmath>
#include <set>
#include "cutout/editing.hpp"
#include "cutout/runtime.hpp"

namespace cutout_studio {
using cutout::Json;
namespace {
QWidget* weightRow(const char* name) {
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
const char* MODE_OPS[] = {"add", "subtract", "replace", "smooth"};
const char* MODE_NAMES[] = {"더하기", "빼기", "바꾸기", "부드럽게"};
/** "slot/name" or "slot/name@skin" (combo data, dialog choices) -> op fields. */
Json attachmentRef(const QString& ref) {
  const QString main = ref.section('@', 0, 0), skin = ref.section('@', 1);
  Json out = {{"slot", ss(main.section('/', 0, 0))}, {"name", ss(main.section('/', 1))}};
  if (!skin.isEmpty()) out["skin"] = ss(skin);
  return out;
}
}  // namespace

void CutoutPanel::buildWeightTools(QWidget* weightTools, QVBoxLayout* fixed, QVBoxLayout* form) {
  auto button = [this](QWidget* row, const QString& text, const char* name, const QString& tip, std::function<bool()> fn) {
    auto* b = new QPushButton(text);
    b->setObjectName(name);
    b->setToolTip(tip);
    connect(b, &QPushButton::clicked, this, [fn] { fn(); });
    addTo(row, b);
    return b;
  };
  normalizeButton_ = button(weightTools, "정리", "cutoutNormalizeWeights",
                            "고른 정점(없으면 모두)의 웨이트를 정리합니다: 「작은 웨이트 지우기」보다 작은 몫을 지우고, 정점당 「최대 본」 개수만 남기고, 합을 1로 맞춥니다. 잠긴 본의 몫은 그대로입니다.",
                            [this] { return normalizeWeights(); });
  // Brush modes: one exclusive row in setup weight mode. Shift+drag smooths and Ctrl+drag subtracts
  // for one stroke without changing the chosen mode.
  QWidget* modes = weightRow("cutoutWeightModes");
  modes->setProperty("cutoutModeRow", "setup-weights");
  const char* names[] = {"cutoutWeightModeAdd", "cutoutWeightModeSubtract", "cutoutWeightModeReplace", "cutoutWeightModeSmooth"};
  const QString tips[] = {"더하기: 붓이 지나간 정점에 고른 본의 웨이트를 「세기」만큼 더합니다(다른 본은 나머지를 나눠 가짐).",
                          "빼기: 고른 본의 웨이트를 「세기」만큼 뺍니다. 칠하는 중 Ctrl을 누르고 끌어도 빼기입니다.",
                          "바꾸기: 고른 본의 웨이트를 「웨이트」 값 쪽으로 「세기」만큼 바꿉니다(세기 1 = 그 값으로).",
                          "부드럽게: 정점의 웨이트를 이웃 정점들의 평균 쪽으로 「세기」만큼 고르게 합니다(모든 본). 칠하는 중 Shift를 누르고 끌어도 부드럽게입니다."};
  for (int i = 0; i < 4; i++) {
    auto* b = new QToolButton;
    b->setObjectName(names[i]);
    b->setText(MODE_NAMES[i]);
    b->setToolTip(tips[i]);
    b->setCheckable(true);
    connect(b, &QToolButton::clicked, this, [this, i] { setWeightMode(i); });
    addTo(modes, b);
    weightModeButtons_[i] = b;
  }
  // Mirror / copy weights and the linked-mesh gestures: one row in setup mode.
  QWidget* more = weightRow("cutoutMeshLinkTools");
  more->setProperty("cutoutModeRow", "setup");
  mirrorButton_ = button(more, "좌우 복사", "cutoutMirrorWeights",
                         "웨이트를 한쪽에서 반대쪽으로 거울처럼 복사합니다(방향은 아래 「좌우 복사 방향」). 본 이름의 짝을 찾아 바꿉니다: left↔right, -l↔-r, levyy↔pravyy 같은 음역 이름도. 짝이 없는 본은 그대로입니다.",
                         [this] { return mirrorWeights(); });
  copyButton_ = button(more, "메시에서 복사", "cutoutCopyWeights",
                       "아래 「가져올 메시」의 웨이트를 같은 위치(기본 자세)에서 가져옵니다. 고른 정점이 있으면 그 정점만.", [this] { return copyWeights(); });
  makeLinkedButton_ = button(more, "링크드 메시", "cutoutMakeLinkedMesh",
                             "링크드 메시 만들기: 그림(영역)이면 같은 비율의 메시를 골라 그 모양·웨이트·변형을 함께 쓰게 하고, 메시면 다른 스킨·그림용 링크드 복사본을 만듭니다(의상 바꾸기).\n"
                             "부모 메시의 정점·웨이트를 고치면 링크드 메시도 함께 바뀝니다.",
                             [this] { return makeLinkedMesh(); });
  unlinkButton_ = button(more, "연결 끊기", "cutoutUnlinkMesh", "링크드 메시를 지금 모양 그대로의 독립 메시로 바꿉니다(이후 따로 편집).", [this] { return unlinkMesh(); });
  fixed->insertWidget(fixed->indexOf(weightTools) + 1, modes);
  fixed->insertWidget(fixed->indexOf(meshBones_), more);
  meshModeRows_ << modes << more;
  // Settings, view toggles and the weight table scroll with the brush fields.
  weightSection_ = new QWidget;
  weightSection_->setObjectName("cutoutWeightSection");
  weightSection_->setProperty("cutoutConditionalRow", true);  // setup mode only
  auto* v = new QVBoxLayout(weightSection_);
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(2);
  auto* f = new QFormLayout;
  f->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  weightFalloffCombo_ = new QComboBox;
  weightFalloffCombo_->setObjectName("cutoutWeightFalloff");
  weightFalloffCombo_->addItems({"딱딱하게", "직선", "부드럽게"});
  weightFalloffCombo_->setCurrentIndex(weightFalloff_);
  weightFalloffCombo_->setToolTip("붓 감쇠: 딱딱하게 = 붓 안이 모두 같은 세기, 직선·부드럽게 = 가장자리로 갈수록 약하게(안쪽 점선 원부터)");
  connect(weightFalloffCombo_, &QComboBox::activated, this, [this](int i) { setWeightFalloff(i); });
  weightMaxInfluences_ = new QSpinBox;
  weightMaxInfluences_->setObjectName("cutoutWeightMaxInfluences");
  weightMaxInfluences_->setRange(1, 4);
  weightMaxInfluences_->setValue(4);
  weightMaxInfluences_->setToolTip("정점 하나를 움직이는 본의 최대 개수(칠하기·정리·복사 모두). 넘치면 가장 작은 몫부터 지웁니다.");
  weightPrune_ = new QDoubleSpinBox;
  weightPrune_->setObjectName("cutoutWeightPrune");
  weightPrune_->setRange(0, 0.5);
  weightPrune_->setSingleStep(0.01);
  weightPrune_->setDecimals(3);
  weightPrune_->setValue(0.01);
  weightPrune_->setToolTip("이보다 작은 웨이트 몫은 지우고 나머지를 1로 맞춥니다(칠하기·정리·복사 모두).");
  mirrorAxis_ = new QComboBox;
  mirrorAxis_->setObjectName("cutoutMirrorAxis");
  mirrorAxis_->addItem("왼쪽 → 오른쪽", "x:left");
  mirrorAxis_->addItem("오른쪽 → 왼쪽", "x:right");
  mirrorAxis_->addItem("위 → 아래", "y:top");
  mirrorAxis_->addItem("아래 → 위", "y:bottom");
  mirrorAxis_->setToolTip("좌우 복사: 웨이트를 가져올 쪽 → 복사할 쪽(기본 자세의 메시 가운데 선 기준).");
  copyWeightsFrom_ = new QComboBox;
  copyWeightsFrom_->setObjectName("cutoutCopyWeightsFrom");
  copyWeightsFrom_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  copyWeightsFrom_->setMinimumContentsLength(8);
  copyWeightsFrom_->setToolTip("메시에서 복사: 웨이트를 가져올 다른 메시");
  weightShowValue_ = new QCheckBox("커서 아래 값");
  weightShowValue_->setObjectName("cutoutWeightShowValue");
  weightShowValue_->setToolTip("붓 옆에 커서 아래의 웨이트 값(고른 본)을 숫자로 보여 줍니다.");
  connect(weightShowValue_, &QCheckBox::toggled, this, [this] { refreshMesh(); });
  weightPosePreview_ = new QCheckBox("자세 미리 보기");
  weightPosePreview_->setObjectName("cutoutWeightPosePreview");
  weightPosePreview_->setToolTip("편집 화면에서만: 칠할 본을 옆의 각도만큼 돌린 자세로 보여 줘서, 칠하면서 메시가 어떻게 휘는지 봅니다. 저장되지 않습니다.");
  connect(weightPosePreview_, &QCheckBox::toggled, this, [this] { updateCanvas(); });
  weightPreviewAngle_ = new QDoubleSpinBox;
  weightPreviewAngle_->setObjectName("cutoutWeightPreviewAngle");
  weightPreviewAngle_->setRange(-180, 180);
  weightPreviewAngle_->setDecimals(0);
  weightPreviewAngle_->setSingleStep(5);
  weightPreviewAngle_->setValue(30);
  weightPreviewAngle_->setSuffix("°");
  weightPreviewAngle_->setToolTip("자세 미리 보기에서 칠할 본을 돌리는 각도");
  connect(weightPreviewAngle_, &QDoubleSpinBox::valueChanged, this, [this] { updateCanvas(); });
  auto* preview = new QHBoxLayout;
  preview->addWidget(weightPosePreview_);
  preview->addWidget(weightPreviewAngle_, 1);
  f->addRow("감쇠", weightFalloffCombo_);
  f->addRow("최대 본", weightMaxInfluences_);
  f->addRow("작은 웨이트 지우기", weightPrune_);
  f->addRow("좌우 복사 방향", mirrorAxis_);
  f->addRow("가져올 메시", copyWeightsFrom_);
  f->addRow(weightShowValue_);
  f->addRow(preview);
  v->addLayout(f);
  auto* title = new QLabel("고른 정점의 웨이트 · 잠금 = 칠해도 그대로");
  title->setStyleSheet("color:#a7b4c2;");
  v->addWidget(title);
  weightTable_ = new QFrame;
  weightTable_->setObjectName("cutoutWeightTable");
  weightTable_->setToolTip("고른 정점들의 본별 웨이트(평균). 값을 바꾸면 고른 정점 모두 그 웨이트가 됩니다(편집 하나). 「잠금」한 본은 칠하기·정리·복사에서 몫이 그대로입니다.");
  auto* grid = new QGridLayout(weightTable_);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(6);
  grid->setVerticalSpacing(1);
  v->addWidget(weightTable_);
  form->insertWidget(form->count() - 1, weightSection_);  // before the trailing stretch
  meshTools_ << normalizeButton_ << weightModeButtons_[0] << weightModeButtons_[1] << weightModeButtons_[2] << weightModeButtons_[3] << mirrorButton_ << copyButton_
             << weightFalloffCombo_ << weightMaxInfluences_ << weightPrune_ << mirrorAxis_ << copyWeightsFrom_ << weightShowValue_ << weightPosePreview_ << weightPreviewAngle_;
  askLinkedMesh = [this](LinkedMeshChoice& c) { return runLinkedMeshDialog(c); };
}

QVector<CutoutPanel::WeightRow> CutoutPanel::weightRows() const {
  QVector<WeightRow> rows;
  int si;
  const cutout::Attachment* att;
  Json fields;
  if (!rig_ || !meshTarget(si, att, fields) || !att->mesh || att->linked) return rows;
  // The bones weighting the mesh (array order), the brush bone and the locked ones.
  std::set<int> bones;
  for (const auto& b : att->binds) bones.insert(b.first);
  if (att->binds.empty() && rig_->slots[size_t(si)].boneIndex >= 0) bones.insert(rig_->slots[size_t(si)].boneIndex);
  if (const int b = rig_->boneIndex(ss(brushBone_->currentData().toString())); b >= 0) bones.insert(b);
  for (const QString& b : weightLocks_) if (const int i = rig_->boneIndex(ss(b)); i >= 0) bones.insert(i);
  const QVector<int> vertices = selectedVertices();
  for (int b : bones) {
    WeightRow row;
    row.bone = qs(rig_->bones[size_t(b)].id);
    row.locked = weightLocks_.contains(row.bone);
    if (!vertices.isEmpty()) {
      double sum = 0;
      for (int v : vertices) sum += vertexWeight(*rig_, si, *att, size_t(v), b);
      row.weight = sum / vertices.size();
    }
    rows << row;
  }
  return rows;
}

void CutoutPanel::refreshWeights(int si, const cutout::Attachment* att, bool mesh, bool linked, bool animate) {
  if (!weightSection_) return;
  for (int i = 0; i < 4; i++) weightModeButtons_[i]->setChecked(weightMode_ == i);
  weightSection_->setVisible(!animate);
  const bool part = att && !att->pictureless();
  makeLinkedButton_->setEnabled(part && !animate && !att->sequence());
  unlinkButton_->setEnabled(mesh && linked && !animate);
  // 가져올 메시: every other mesh of the rig (default list, then each skin's).
  {
    QSignalBlocker block(copyWeightsFrom_);
    const QString keep = copyWeightsFrom_->currentData().toString();
    copyWeightsFrom_->clear();
    const QString self = att ? qs(att->slot + "/" + att->name) + (skinProviding(qs(att->slot), qs(att->name)).isEmpty() ? QString() : "@" + previewSkin_) : QString();
    auto add = [&](const cutout::Attachment& a, const QString& skin) {
      const QString ref = qs(a.slot + "/" + a.name) + (skin.isEmpty() ? QString() : "@" + skin);
      if (a.mesh && ref != self) copyWeightsFrom_->addItem(skin.isEmpty() ? qs(a.slot + "/" + a.name) : QString("%1 (스킨 %2)").arg(qs(a.slot + "/" + a.name), skin), ref);
    };
    for (const auto& a : rig_ ? rig_->attachments : std::vector<cutout::Attachment>()) add(a, QString());
    for (const auto& s : rig_ ? rig_->skins : std::vector<cutout::Skin>()) for (const auto& a : s.attachments) add(a, qs(s.id));
    copyWeightsFrom_->setCurrentIndex(std::max(0, copyWeightsFrom_->findData(keep)));
  }
  copyButton_->setEnabled(copyButton_->isEnabled() && copyWeightsFrom_->count() > 0);
  if (canvas_) {
    RigCanvas::WeightView view;
    view.mode = weightMode_;
    view.falloff = weightFalloff_;
    view.showValue = weightShowValue_->isChecked();
    for (const QString& b : weightLocks_) if (rig_ && rig_->boneIndex(ss(b)) >= 0) view.locked.insert(rig_->boneIndex(ss(b)));
    canvas_->setWeightView(view);
  }
  // The weight table: rebuilt when its bones change, values refreshed in place.
  const QVector<WeightRow> rows = weightRows();
  QStringList bones;
  for (const auto& r : rows) bones << r.bone;
  auto* grid = static_cast<QGridLayout*>(weightTable_->layout());
  const QString key = (mesh ? qs(att->slot + "/" + att->name) : QString()) + "|" + bones.join(",");
  if (key != weightTableKey_) {
    weightTableKey_ = key;
    // Later, not now: the rebuild can run inside a signal of one of these widgets (a table edit).
    while (QLayoutItem* item = grid->takeAt(0)) {
      if (QWidget* w = item->widget()) { w->hide(); w->setObjectName(QString()); w->setParent(nullptr); w->deleteLater(); }
      delete item;
    }
    if (rows.isEmpty()) {
      auto* hint = new QLabel(mesh && linked ? "링크드 메시: 웨이트는 부모 메시에서 칠합니다." : "메시를 고르면 본별 웨이트가 여기 나옵니다.");
      hint->setStyleSheet("color:#8b98a5;");
      grid->addWidget(hint, 0, 0, 1, 3);
    }
    for (int i = 0; i < rows.size(); i++) {
      const QString bone = rows[i].bone;
      auto* name = new QLabel(bone);
      name->setObjectName("cutoutWeightBone-" + bone);
      auto* value = new QDoubleSpinBox;
      value->setObjectName("cutoutWeightValue-" + bone);
      value->setRange(-1, 1);
      value->setSpecialValueText("—");
      value->setDecimals(3);
      value->setSingleStep(0.05);
      value->setToolTip(QString("고른 정점들의 「%1」 웨이트(평균). 바꾸면 고른 정점 모두 이 값이 되고, 나머지(잠기지 않은 본)가 남은 몫을 나눠 갖습니다.").arg(bone));
      connect(value, &QDoubleSpinBox::editingFinished, this, [this, value, bone] {
        if (filling_ || value->value() < 0 || std::fabs(value->value() - value->property("shown").toDouble()) < 5e-4) return;
        setVertexWeight(bone, value->value());
      });
      auto* lock = new QCheckBox("잠금");
      lock->setObjectName("cutoutWeightBoneLock-" + bone);
      lock->setToolTip(QString("「%1」의 몫을 잠급니다: 칠하기·정리·복사가 이 본의 웨이트를 바꾸지 않습니다(편집기 설정, 저장 안 됨).").arg(bone));
      connect(lock, &QCheckBox::toggled, this, [this, bone](bool on) { if (!filling_) setBoneLocked(bone, on); });
      grid->addWidget(name, i, 0);
      grid->addWidget(value, i, 1);
      grid->addWidget(lock, i, 2);
    }
    grid->setColumnStretch(1, 1);
  }
  for (const auto& r : rows) {
    auto* value = weightTable_->findChild<QDoubleSpinBox*>("cutoutWeightValue-" + r.bone);
    auto* lock = weightTable_->findChild<QCheckBox*>("cutoutWeightBoneLock-" + r.bone);
    if (!value || !lock) continue;
    QSignalBlocker a(value), b(lock);
    value->setValue(r.weight < 0 ? -1 : r.weight);
    value->setProperty("shown", r.weight);
    value->setEnabled(r.weight >= 0 && !animate && !r.locked);
    lock->setChecked(r.locked);
    lock->setEnabled(!animate);
  }
  Q_UNUSED(si);
}

void CutoutPanel::setWeightMode(int mode) {
  weightMode_ = std::clamp(mode, 0, 3);
  refreshMesh();
}
void CutoutPanel::setWeightFalloff(int falloff) {
  weightFalloff_ = std::clamp(falloff, 0, 2);
  if (weightFalloffCombo_) { QSignalBlocker block(weightFalloffCombo_); weightFalloffCombo_->setCurrentIndex(weightFalloff_); }
  refreshMesh();
}
void CutoutPanel::setBoneLocked(const QString& bone, bool locked) {
  if (locked) weightLocks_.insert(bone); else weightLocks_.remove(bone);
  setStatus(locked ? QString("본 「%1」의 웨이트를 잠갔습니다: 칠하기·정리·복사가 이 몫을 바꾸지 않습니다.").arg(bone) : QString("본 「%1」의 잠금을 풀었습니다.").arg(bone));
  refreshMesh();
}
QStringList CutoutPanel::lockedBones() const {
  QStringList out(weightLocks_.begin(), weightLocks_.end());
  out.sort();
  return out;
}
Json CutoutPanel::withWeightRules(Json op) const {
  Json locked = Json::array();
  for (const QString& b : lockedBones()) if (rig_ && rig_->boneIndex(ss(b)) >= 0) locked.push_back(ss(b));
  if (!locked.empty()) op["locked"] = locked;
  if (weightMaxInfluences_ && weightMaxInfluences_->value() != 4) op["maxInfluences"] = weightMaxInfluences_->value();
  if (weightPrune_) op["prune"] = weightPrune_->value();
  return op;
}

std::optional<Json> CutoutPanel::strokeOp(const RigCanvas::PaintStroke& stroke, QString* error) const {
  auto fail = [&](const QString& why) -> std::optional<Json> { if (error) *error = why; return std::nullopt; };
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op) || !att->mesh) return fail("메시인 조각을 먼저 고르세요.");
  if (att->linked) return fail("링크드 메시는 부모 메시에서 칠합니다(바꾸려면 「연결 끊기」).");
  int mode = weightMode_;
  if (stroke.modifiers & Qt::ShiftModifier) mode = RigCanvas::WeightSmooth;
  else if (stroke.modifiers & Qt::ControlModifier) mode = RigCanvas::WeightSubtract;
  const QString bone = brushBone_->currentData().toString();
  if (mode != RigCanvas::WeightSmooth && bone.isEmpty()) return fail("칠할 본을 고르세요.");
  if (mode != RigCanvas::WeightSmooth && weightLocks_.contains(bone)) return fail(QString("본 「%1」은 잠겨 있어 칠할 수 없습니다(웨이트 표에서 잠금을 푸세요).").arg(bone));
  std::vector<int> vertices;
  std::vector<double> amounts;
  for (int i = 0; i < stroke.vertices.size(); i++) {
    const double a = RigCanvas::falloffAt(weightFalloff_, i < stroke.reach.size() ? stroke.reach[i] : 0);
    if (a <= 1e-3) continue;  // the very edge of a soft brush paints nothing
    vertices.push_back(stroke.vertices[i]);
    amounts.push_back(std::round(a * 1e4) / 1e4);
  }
  if (vertices.empty()) return fail("붓이 닿은 정점이 없습니다.");
  op["op"] = "paintWeights";
  op["mode"] = MODE_OPS[mode];
  op["vertices"] = vertices;
  if (std::any_of(amounts.begin(), amounts.end(), [](double a) { return a < 1; })) op["amounts"] = amounts;
  op["strength"] = brushStrength_->value();
  if (mode != RigCanvas::WeightSmooth) op["bone"] = ss(bone);
  if (mode == RigCanvas::WeightReplace) op["weight"] = brushWeight_->value();
  return withWeightRules(op);
}
void CutoutPanel::previewStroke() {
  if (!pendingStroke_) return;
  const auto op = strokeOp(*pendingStroke_);
  if (!op) return;
  try { dragPreview_ = std::make_shared<const Json>(cutout::applyEdit(working_ ? *working_ : committedRig(), *op).data); } catch (const std::exception&) { return; }
  rebuildDisplay(false);
}
bool CutoutPanel::paintStroke(const RigCanvas::PaintStroke& stroke) {
  if (mode_ != Mode::Setup) { setStatus("웨이트는 조립 모드에서 칠합니다.", true); return false; }
  QString why;
  const auto op = strokeOp(stroke, &why);
  if (!op) { setStatus(why, true); return false; }
  int changed = 0;
  if (!runOps({*op}, false, [&](const cutout::EditResult& r) { changed = r.extra.value("vertices", 0); })) return false;
  meshBones_->setProperty("key", QString());
  const QString mode = qs((*op)["mode"].get<std::string>());
  const int index = int(std::find(std::begin(MODE_OPS), std::end(MODE_OPS), mode.toStdString()) - std::begin(MODE_OPS));
  setStatus(QString("%1: 정점 %2개%3. 되돌리기: Ctrl+Z").arg(MODE_NAMES[std::clamp(index, 0, 3)]).arg(changed)
                .arg(op->contains("bone") ? QString(" · 본 「%1」").arg(qs((*op)["bone"].get<std::string>())) : QString()));
  return true;
}
bool CutoutPanel::normalizeWeights() {
  if (mode_ != Mode::Setup) { setStatus("웨이트 정리는 조립 모드에서 합니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op) || !att->mesh || att->linked) { setStatus("웨이트를 정리할 메시(링크드 메시가 아닌)를 고르세요.", true); return false; }
  op["op"] = "normalizeWeights";
  if (const QVector<int> v = selectedVertices(); !v.isEmpty()) op["vertices"] = std::vector<int>(v.begin(), v.end());
  int pruned = 0;
  if (!runOps({withWeightRules(op)}, false, [&](const cutout::EditResult& r) { pruned = r.extra.value("pruned", 0); })) return false;
  setStatus(QString("웨이트를 정리했습니다: 작은 몫 %1개 지움 · 정점당 최대 본 %2개 · 합 1. 되돌리기: Ctrl+Z").arg(pruned).arg(weightMaxInfluences_->value()));
  return true;
}
bool CutoutPanel::mirrorWeights() {
  if (mode_ != Mode::Setup) { setStatus("웨이트 좌우 복사는 조립 모드에서 합니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op) || !att->mesh || att->linked) { setStatus("웨이트를 복사할 메시(링크드 메시가 아닌)를 고르세요.", true); return false; }
  const QString axis = mirrorAxis_->currentData().toString();
  op["op"] = "mirrorWeights";
  op["axis"] = ss(axis.section(':', 0, 0));
  op["from"] = ss(axis.section(':', 1));
  int changed = 0;
  QStringList pairs, alone;
  if (!runOps({withWeightRules(op)}, false, [&](const cutout::EditResult& r) {
        changed = r.extra.value("vertices", 0);
        for (auto it = r.extra["mapping"].begin(); it != r.extra["mapping"].end(); ++it)
          if (it.key() != it.value().get<std::string>()) pairs << qs(it.key() + "→" + it.value().get<std::string>());
        for (const auto& b : r.extra["unmapped"]) alone << qs(b.get<std::string>());
      }))
    return false;
  setStatus(QString("웨이트를 %1 복사했습니다: 정점 %2개 · 짝 %3%4. 되돌리기: Ctrl+Z").arg(mirrorAxis_->currentText()).arg(changed)
                .arg(pairs.isEmpty() ? QString("없음") : pairs.join(", ")).arg(alone.isEmpty() ? QString() : " · 짝 없음(그대로): " + alone.join(", ")));
  return true;
}
bool CutoutPanel::copyWeights(const QString& fromIn) {
  if (mode_ != Mode::Setup) { setStatus("웨이트 복사는 조립 모드에서 합니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op) || !att->mesh || att->linked) { setStatus("웨이트를 받을 메시(링크드 메시가 아닌)를 고르세요.", true); return false; }
  const QString from = fromIn.isEmpty() ? copyWeightsFrom_->currentData().toString() : fromIn;
  if (from.isEmpty()) { setStatus("웨이트를 가져올 메시가 없습니다.", true); return false; }
  op["op"] = "copyWeights";
  op["from"] = attachmentRef(from);
  if (const QVector<int> v = selectedVertices(); !v.isEmpty()) op["vertices"] = std::vector<int>(v.begin(), v.end());
  int changed = 0;
  if (!runOps({withWeightRules(op)}, false, [&](const cutout::EditResult& r) { changed = r.extra.value("vertices", 0); })) return false;
  meshBones_->setProperty("key", QString());
  setStatus(QString("「%1」의 웨이트를 정점 %2개에 가져왔습니다. 되돌리기: Ctrl+Z").arg(from.section('@', 0, 0)).arg(changed));
  return true;
}
bool CutoutPanel::setVertexWeight(const QString& bone, double weight) {
  if (mode_ != Mode::Setup) { setStatus("웨이트는 조립 모드에서 바꿉니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op) || !att->mesh || att->linked) { setStatus("메시(링크드 메시가 아닌)를 고르세요.", true); return false; }
  const QVector<int> vertices = selectedVertices();
  if (vertices.isEmpty()) { setStatus("웨이트를 바꿀 정점을 먼저 고르세요(정점 편집에서 클릭·상자 선택).", true); return false; }
  if (weightLocks_.contains(bone)) { setStatus(QString("본 「%1」은 잠겨 있습니다.").arg(bone), true); return false; }
  op.update({{"op", "paintWeights"}, {"mode", "replace"}, {"bone", ss(bone)}, {"weight", std::clamp(weight, 0.0, 1.0)}, {"vertices", std::vector<int>(vertices.begin(), vertices.end())}});
  if (!runOps({withWeightRules(op)}, false)) return false;
  meshBones_->setProperty("key", QString());
  setStatus(QString("고른 정점 %1개의 「%2」 웨이트를 %3으로 바꿨습니다. 되돌리기: Ctrl+Z").arg(vertices.size()).arg(bone).arg(weight, 0, 'f', 3));
  return true;
}
void CutoutPanel::stepBrushRadius(int direction) {
  if (meshEdit_ != MeshEdit::Weights || !brushRadius_) return;
  const double r = brushRadius_->value();
  const double next = direction > 0 ? std::max(r + 2, r * 1.25) : std::min(r - 2, r / 1.25);
  brushRadius_->setValue(std::clamp(std::round(next), brushRadius_->minimum(), brushRadius_->maximum()));
  setStatus(QString("붓 크기 %1 px ([ 작게 · ] 크게)").arg(brushRadius_->value()));
}
void CutoutPanel::setWeightPosePreview(bool on, double angle) {
  QSignalBlocker a(weightPosePreview_), b(weightPreviewAngle_);
  weightPosePreview_->setChecked(on);
  weightPreviewAngle_->setValue(angle);
  updateCanvas();
}
cutout::Pose CutoutPanel::weightPreviewPose(cutout::Pose pose) const {
  if (!rig_ || mode_ != Mode::Setup || meshEdit_ != MeshEdit::Weights || !weightPosePreview_ || !weightPosePreview_->isChecked() || showingPreview()) return pose;
  const int b = rig_->boneIndex(ss(brushBone_->currentData().toString()));
  if (b < 0 || pose.world.size() != rig_->bones.size() || pose.bones.size() != rig_->bones.size()) return pose;
  // The setup pose with the brush bone turned (editor-only): forward kinematics again, IK re-solved.
  pose.bones[size_t(b)].rotation += weightPreviewAngle_->value();
  pose.applied.clear();
  for (int i : rig_->evalOrder) pose.world[size_t(i)] = cutout::boneWorld(*rig_, pose.world, size_t(i), pose.bones[size_t(i)]);
  cutout::applyIk(*rig_, pose);
  return pose;
}

// ---------------------------------------------------------------- linked meshes (plugin 1.18.0)
QString CutoutPanel::linkedInfo(const cutout::Attachment& att) const {
  return QString("링크드 메시 「%1」 · 부모 「%2/%3」(%4) · 정점 %5 · 변형 키: %6 · 그림 %7×%8 · 정점·웨이트는 부모에서 편집(「연결 끊기」로 따로)")
      .arg(qs(att.name), qs(att.linkedSlot), qs(att.linkedName), att.linkedSkin.empty() ? QString("기본 첨부") : QString("스킨 「%1」").arg(qs(att.linkedSkin)))
      .arg(att.vertexCount()).arg(att.linkedDeform ? QString("부모와 공유(%1)").arg(qs(att.deformName)) : QString("따로"))
      .arg(att.linkedWidth).arg(att.linkedHeight);
}
bool CutoutPanel::makeLinkedMesh() {
  if (mode_ != Mode::Setup) { setStatus("링크드 메시는 조립 모드에서 만듭니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json fields;
  if (!meshTarget(si, att, fields) || att->pictureless()) { setStatus("그림이나 메시인 조각(▣)을 먼저 고르세요.", true); return false; }
  if (att->sequence()) { setStatus("이미지 시퀀스는 링크드 메시가 될 수 없습니다.", true); return false; }
  LinkedMeshChoice c;
  const double w = att->linked ? att->linkedWidth : att->width, h = att->linked ? att->linkedHeight : att->height;
  auto sameAspect = [](double w1, double h1, double w2, double h2) { return w1 > 0 && h1 > 0 && w2 > 0 && h2 > 0 && std::fabs((w1 / h1) / (w2 / h2) - 1) <= 0.02; };
  const QString ownSkin = fields.contains("skin") ? qs(fields["skin"].get<std::string>()) : QString();
  if (!att->mesh) {
    // A picture becomes a linked copy of a mesh drawn at the same aspect (an outfit picture over a meshed part).
    auto add = [&](const cutout::Attachment& m, const QString& skin) {
      if (m.mesh && !m.sequence() && sameAspect(w, h, m.width, m.height)) c.parents << qs(m.slot + "/" + m.name) + (skin.isEmpty() ? QString() : "@" + skin);
    };
    for (const auto& m : rig_->attachments) add(m, QString());
    for (const auto& s : rig_->skins) for (const auto& m : s.attachments) add(m, qs(s.id));
    if (c.parents.isEmpty()) { setStatus(QString("그림(%1×%2)과 같은 비율의 메시가 없습니다. 먼저 같은 비율의 그림을 메시로 만드세요.").arg(w).arg(h), true); return false; }
    const QString sameSlot = qs(att->slot + "/" + att->name);
    c.parent = c.parents.contains(sameSlot) ? sameSlot : c.parents.first();
  } else {
    // A mesh: a linked copy for an outfit (a skin's entry of the same name) or another picture.
    c.parent = qs(att->slot + "/" + att->name) + (ownSkin.isEmpty() ? QString() : "@" + ownSkin);
    for (const auto& s : rig_->skins) if (qs(s.id) != ownSkin) c.skins << qs(s.id);
    c.skins << QString();  // the default list (a new name)
    c.skin = c.skins.contains(previewSkin_) && !previewSkin_.isEmpty() ? previewSkin_ : c.skins.first();
    c.name = qs(att->name);
    for (const QString& id : images_.imageAssetIds()) {
      const QSize size = images_.pixelSize(id);
      if (id != qs(att->imageAssetId) && sameAspect(w, h, size.width(), size.height())) c.images << id;
    }
    if (c.images.isEmpty()) { setStatus("메시 그림과 같은 비율의 다른 그림 에셋이 없습니다. 의상 그림을 먼저 가져오세요.", true); return false; }
    c.assetId = c.images.first();
  }
  if (!askLinkedMesh || !askLinkedMesh(c)) return false;
  Json op;
  if (!att->mesh) {
    const Json parent = attachmentRef(c.parent);
    op = fields;
    op.update({{"op", "linkMesh"}, {"parent", parent["slot"].get<std::string>() + "/" + parent["name"].get<std::string>()}});
    if (parent.contains("skin")) op["parentSkin"] = parent["skin"];
  } else {
    const QSize size = images_.pixelSize(c.assetId);
    if (c.name.isEmpty() || !cutout::validName(ss(c.name))) { setStatus("링크드 메시 이름이 올바르지 않습니다(영문·숫자·-·_).", true); return false; }
    op = {{"op", "linkMesh"}, {"slot", att->slot}, {"name", ss(c.name)}, {"parent", att->slot + "/" + att->name}, {"imageAssetId", ss(c.assetId)}};
    if (size.isValid()) { op["width"] = size.width(); op["height"] = size.height(); }
    if (!c.skin.isEmpty()) op["skin"] = ss(c.skin);
    if (!ownSkin.isEmpty()) op["parentSkin"] = ss(ownSkin);
  }
  if (!c.deform) op["deform"] = false;
  QString made;
  if (!runOps({op}, false, [&](const cutout::EditResult& r) { made = qs(r.extra.value("attachment", std::string())); })) return false;
  setStatus(QString("링크드 메시 「%1」 → 부모 「%2」: 부모의 모양·웨이트%3를 함께 씁니다. 되돌리기: Ctrl+Z")
                .arg(made, qs(op["parent"].get<std::string>())).arg(c.deform ? "·변형 키" : ""));
  return true;
}
bool CutoutPanel::unlinkMesh() {
  if (mode_ != Mode::Setup) { setStatus("연결 끊기는 조립 모드에서 합니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op) || !att->linked) { setStatus("링크드 메시를 먼저 고르세요.", true); return false; }
  const QString name = qs(att->name);
  op["op"] = "unlinkMesh";
  if (!runOps({op}, false)) return false;
  setStatus(QString("「%1」을 독립 메시(지금 모양 그대로)로 바꿨습니다. 이제 정점·웨이트를 따로 편집합니다. 되돌리기: Ctrl+Z").arg(name));
  return true;
}
bool CutoutPanel::runLinkedMeshDialog(LinkedMeshChoice& c) {
  QDialog dialog(this);
  dialog.setObjectName("cutoutLinkedMeshDialog");
  dialog.setWindowTitle("링크드 메시 만들기");
  auto* layout = new QVBoxLayout(&dialog);
  auto* form = new QFormLayout;
  QComboBox *parent = nullptr, *skin = nullptr, *image = nullptr;
  QLineEdit* name = nullptr;
  if (!c.parents.isEmpty()) {
    layout->addWidget(new QLabel("이 그림이 모양·웨이트를 함께 쓸 부모 메시(같은 비율)를 고르세요."));
    parent = new QComboBox;
    parent->setObjectName("cutoutLinkedParent");
    for (const QString& p : c.parents) parent->addItem(p.contains('@') ? QString("%1 (스킨 %2)").arg(p.section('@', 0, 0), p.section('@', 1)) : p, p);
    parent->setCurrentIndex(std::max(0, parent->findData(c.parent)));
    form->addRow("부모 메시", parent);
  } else {
    layout->addWidget(new QLabel(QString("메시 「%1」의 모양·웨이트를 함께 쓰는 링크드 복사본을 만듭니다(다른 스킨·그림).").arg(c.parent.section('@', 0, 0))));
    skin = new QComboBox;
    skin->setObjectName("cutoutLinkedSkin");
    for (const QString& s : c.skins) skin->addItem(s.isEmpty() ? QString("기본 첨부(새 이름)") : QString("스킨 「%1」").arg(s), s);
    skin->setCurrentIndex(std::max(0, skin->findData(c.skin)));
    name = new QLineEdit(c.name);
    name->setObjectName("cutoutLinkedName");
    image = new QComboBox;
    image->setObjectName("cutoutLinkedImage");
    for (const QString& id : c.images) image->addItem(id, id);
    image->setCurrentIndex(std::max(0, image->findData(c.assetId)));
    form->addRow("넣을 곳", skin);
    form->addRow("첨부 이름", name);
    form->addRow("그림", image);
  }
  auto* deform = new QCheckBox("부모의 변형 키도 함께 쓰기");
  deform->setObjectName("cutoutLinkedDeform");
  deform->setChecked(c.deform);
  form->addRow(deform);
  layout->addLayout(form);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);
  if (dialog.exec() != QDialog::Accepted) return false;
  if (parent) c.parent = parent->currentData().toString();
  if (skin) c.skin = skin->currentData().toString();
  if (name) c.name = name->text().trimmed();
  if (image) c.assetId = image->currentData().toString();
  c.deform = deform->isChecked();
  return true;
}
}  // namespace cutout_studio
