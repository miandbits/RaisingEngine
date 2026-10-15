#include "layer-import.hpp"
#include <QtWidgets>

namespace cutout_studio {
namespace {
constexpr int KEY_ROLE = Qt::UserRole, ROLE_ROLE = Qt::UserRole + 1;
const char* const ORIGINS[][2] = {{"bottom-center", "아래 가운데 (발 사이)"}, {"center", "가운데"}, {"top-center", "위 가운데"}, {"top-left", "왼쪽 위"}};
}  // namespace

QJsonObject LayerImportChoice::payload() const {
  QJsonObject p{{"file", file}, {"origin", origin}, {"trim", trim}, {"scale", scale}, {"boneDirection", aimBones ? "auto" : "none"}, {"exclude", QJsonArray::fromStringList(exclude)}};
  if (target.isEmpty()) p["id"] = id;
  else p["rig"] = target;
  return p;
}

QString layerRoleLabel(const QString& role) {
  static const QHash<QString, QString> labels{{"image", "그림"}, {"bone", "본"}, {"pivot", "회전 중심 표시"}, {"slot-group", "슬롯 묶음"}, {"group-in-slot", "슬롯 묶음 안"},
                                             {"ignored", "무시([ignore])"}, {"excluded", "빼기"}, {"empty", "빈 레이어"}, {"empty-group", "빈 그룹"}};
  return labels.value(role, role);
}
QString layerStatusLabel(const QString& status) {
  static const QHash<QString, QString> labels{{"add", "추가"}, {"update", "그림 교체"}, {"same", "같음"}, {"skip", "건너뜀"}, {"kept", "유지"}};
  return labels.value(status, status);
}
QString layerImportSummary(const QJsonObject& report) {
  const QJsonObject s = report.value("summary").toObject();
  QString text = QString("본 %1개 추가 · 슬롯 %2 · 첨부 %3(숨김 %4)").arg(s.value("bones").toInt()).arg(s.value("slots").toInt()).arg(s.value("attachments").toInt()).arg(s.value("hidden").toInt());
  if (report.value("mode").toString() == "update")
    text += QString(" · 새 그림 %1 · 교체 %2 · 같음 %3").arg(s.value("added").toInt()).arg(s.value("updated").toInt()).arg(s.value("unchanged").toInt());
  const int missing = report.value("changes").toObject().value("attachments").toObject().value("missing").toArray().size();
  if (missing) text += QString(" · PSD에 없는 첨부 %1(그대로 둠)").arg(missing);
  return text;
}

LayerImportDialog::LayerImportDialog(QWidget* parent, LayerImportChoice& choice, const QStringList& rigs, const QJsonObject& report, Preview preview)
    : QDialog(parent), choice_(choice), preview_(std::move(preview)) {
  setObjectName("cutoutLayerImportDialog");
  setWindowTitle("레이어 그림 가져오기 (PSD)");
  auto* layout = new QVBoxLayout(this);
  source_ = new QLabel;
  source_->setObjectName("cutoutLayerImportSource");
  source_->setWordWrap(true);
  layout->addWidget(source_);

  auto* form = new QFormLayout;
  target_ = new QComboBox;
  target_->setObjectName("cutoutLayerImportTarget");
  target_->addItem("새 캐릭터", QString());
  for (const QString& rig : rigs) target_->addItem(QString("「%1」 갱신 (본·키 유지)").arg(rig), rig);
  target_->setCurrentIndex(std::max(0, target_->findData(choice.target)));
  id_ = new QLineEdit(choice.id);
  id_->setObjectName("cutoutLayerImportId");
  id_->setPlaceholderText("영문 소문자·숫자·하이픈");
  origin_ = new QComboBox;
  origin_->setObjectName("cutoutLayerImportOrigin");
  for (const auto& o : ORIGINS) origin_->addItem(o[1], o[0]);
  origin_->setCurrentIndex(std::max(0, origin_->findData(choice.origin)));
  origin_->setToolTip("캐릭터 원점(루트 본). 맨 위 단계의 [pivot] 레이어가 있으면 그 자리가 원점입니다.");
  trim_ = new QCheckBox("투명 여백 잘라내기");
  trim_->setObjectName("cutoutLayerImportTrim");
  trim_->setChecked(choice.trim);
  aimBones_ = new QCheckBox("본 방향·길이 자동 (흔들림 물리·IK 늘이기용)");
  aimBones_->setObjectName("cutoutLayerImportAimBones");
  aimBones_->setChecked(choice.aimBones);
  aimBones_->setToolTip("새 본을 그 그룹의 자식 본(하나일 때) 또는 그림 쪽으로 향하게 하고, 그림 끝까지의 길이를 줍니다(없으면 위쪽).\n"
                        "그림은 반대로 돌려 붙여 PSD와 똑같이 보입니다. 끄면 1.11.0처럼 모든 본이 오른쪽(+x)을 향하고 길이가 없습니다.\n다시 가져올 때 이미 있는 본은 바꾸지 않습니다.");
  scale_ = new QDoubleSpinBox;
  scale_->setObjectName("cutoutLayerImportScale");
  scale_->setRange(0.05, 4);
  scale_->setDecimals(2);
  scale_->setSingleStep(0.25);
  scale_->setValue(choice.scale);
  scale_->setToolTip("그림은 원본 해상도 그대로 두고 배치에 배율을 곱합니다(갱신할 때는 처음 배율을 씁니다).");
  form->addRow("가져올 곳", target_);
  form->addRow("캐릭터 ID", id_);
  form->addRow("원점", origin_);
  form->addRow("배율", scale_);
  form->addRow(QString(), trim_);
  form->addRow(QString(), aimBones_);
  layout->addLayout(form);
  connect(target_, &QComboBox::currentIndexChanged, this, [this] { id_->setEnabled(target_->currentData().toString().isEmpty()); });
  id_->setEnabled(choice.target.isEmpty());

  tree_ = new QTreeWidget;
  tree_->setObjectName("cutoutLayerImportTree");
  tree_->setHeaderLabels({"레이어 (체크 = 가져오기)", "역할", "본", "슬롯", "첨부", "상태"});
  tree_->setRootIsDecorated(true);
  tree_->setUniformRowHeights(true);
  tree_->setMinimumSize(560, 260);
  connect(tree_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
    if (filling_ || column != 0) return;
    // A left-out group leaves out everything under it: the children follow its box.
    filling_ = true;
    std::function<void(QTreeWidgetItem*, bool)> follow = [&](QTreeWidgetItem* at, bool on) {
      for (int i = 0; i < at->childCount(); i++) {
        QTreeWidgetItem* c = at->child(i);
        if (c->data(0, ROLE_ROLE).toString() != "ignored") c->setDisabled(!on);
        follow(c, on && c->checkState(0) == Qt::Checked);
      }
    };
    follow(item, item->checkState(0) == Qt::Checked);
    filling_ = false;
  });
  layout->addWidget(tree_, 1);
  warnings_ = new QListWidget;
  warnings_->setObjectName("cutoutLayerImportWarnings");
  warnings_->setMaximumHeight(110);
  layout->addWidget(new QLabel("주의"));
  layout->addWidget(warnings_);
  summary_ = new QLabel;
  summary_->setObjectName("cutoutLayerImportSummary");
  summary_->setWordWrap(true);
  layout->addWidget(summary_);
  auto* hint = new QLabel("그룹 = 본, 레이어 = 슬롯·그림(ID는 영문 이름에서, 한글만 있으면 part-N). 「eyes/open」「eyes@closed」처럼 나누면 같은 슬롯의 바꿔 끼우는 그림이 되고, "
                          "숨긴 레이어는 숨긴 첨부(표정용)로 들어옵니다. [pivot] 레이어 = 그 그룹 본의 회전 중심, [slot] 그룹 = 한 슬롯, [ignore] = 빼기. 가져오기는 편집 한 번이라 Ctrl+Z로 되돌립니다.");
  hint->setObjectName("cutoutLayerImportHint");
  hint->setWordWrap(true);
  layout->addWidget(hint);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttons->button(QDialogButtonBox::Ok)->setText("가져오기");
  auto* again = buttons->addButton("미리 보기 갱신", QDialogButtonBox::ActionRole);
  again->setObjectName("cutoutLayerImportRefresh");
  again->setToolTip("빼기·원점·배율·잘라내기·본 방향을 바꾼 뒤 본·슬롯 배치를 다시 계산해 봅니다.");
  connect(again, &QPushButton::clicked, this, [this] { refresh(); });
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);
  showReport(report);
}

void LayerImportDialog::addItems(QTreeWidgetItem* parent, const QJsonArray& layers) {
  // Report lists are back → front; the tree shows Photoshop's order (front on top).
  for (int i = layers.size() - 1; i >= 0; i--) {
    const QJsonObject l = layers[i].toObject();
    const QString role = l.value("role").toString(), name = l.value("name").toString();
    auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree_);
    item->setText(0, name.isEmpty() ? "(이름 없음)" : name);
    if (!l.value("visible").toBool(true)) item->setText(0, item->text(0) + "  (숨김)");
    item->setText(1, layerRoleLabel(role));
    item->setText(2, l.value("bone").toString());
    item->setText(3, l.value("slot").toString());
    item->setText(4, l.value("attachment").toString() + (l.value("setup").toBool() ? "  ●" : QString()));
    item->setText(5, layerStatusLabel(l.value("status").toString()));
    item->setData(0, KEY_ROLE, l.value("key").toString());
    item->setData(0, ROLE_ROLE, role);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    // Unchecked = left out by its own key; under a left-out group an item stays checked but greyed,
    // so checking the group again brings the whole group back.
    const bool own = choice_.exclude.contains(l.value("key").toString());
    item->setCheckState(0, own || role == "ignored" ? Qt::Unchecked : Qt::Checked);
    if (role == "ignored" || (parent && (parent->checkState(0) != Qt::Checked || parent->isDisabled()))) item->setDisabled(true);
    QStringList tip;
    const QJsonObject b = l.value("bounds").toObject();
    if (!b.isEmpty()) tip << QString("위치 %1, %2 · 크기 %3×%4").arg(b.value("x").toInt()).arg(b.value("y").toInt()).arg(b.value("width").toInt()).arg(b.value("height").toInt());
    if (l.value("opacity").toDouble(1) < 1) tip << QString("불투명도 %1%").arg(qRound(l.value("opacity").toDouble() * 100));
    for (const auto& w : l.value("warnings").toArray()) tip << w.toString();
    if (!tip.isEmpty()) for (int c = 0; c < 6; c++) item->setToolTip(c, tip.join("\n"));
    if (!l.value("warnings").toArray().isEmpty()) item->setForeground(1, QColor(200, 120, 0));
    addItems(item, l.value("children").toArray());
  }
}

void LayerImportDialog::showReport(const QJsonObject& report) {
  filling_ = true;
  const QJsonObject s = report.value("source").toObject();
  source_->setText(QString("%1 · %2 %3×%4 px").arg(s.value("file").toString(), s.value("format").toString().toUpper()).arg(s.value("width").toInt()).arg(s.value("height").toInt()));
  tree_->clear();
  addItems(nullptr, report.value("layers").toArray());
  tree_->expandAll();
  for (int c = 0; c < 6; c++) tree_->resizeColumnToContents(c);
  warnings_->clear();
  for (const auto& w : report.value("warnings").toArray()) warnings_->addItem(w.toObject().value("message").toString());
  if (!warnings_->count()) warnings_->addItem("없음");
  summary_->setText(layerImportSummary(report));
  filling_ = false;
}

void LayerImportDialog::collect() {
  choice_.target = target_->currentData().toString();
  choice_.id = id_->text().trimmed();
  choice_.origin = origin_->currentData().toString();
  choice_.trim = trim_->isChecked();
  choice_.aimBones = aimBones_->isChecked();
  choice_.scale = scale_->value();
  choice_.exclude.clear();
  QTreeWidgetItemIterator it(tree_);
  for (; *it; ++it)
    if ((*it)->checkState(0) != Qt::Checked && (*it)->data(0, ROLE_ROLE).toString() != "ignored") {
      // Only the topmost left-out item: its subtree goes with it.
      bool covered = false;
      for (QTreeWidgetItem* p = (*it)->parent(); p && !covered; p = p->parent()) covered = p->checkState(0) != Qt::Checked;
      if (!covered) choice_.exclude << (*it)->data(0, KEY_ROLE).toString();
    }
}

void LayerImportDialog::refresh() {
  collect();
  if (!preview_) return;
  QPointer<LayerImportDialog> self(this);
  preview_(choice_, [self](const QJsonObject& report, const QString& error) {
    if (!self) return;
    if (!error.isEmpty()) { self->summary_->setText("미리 보기를 만들 수 없습니다: " + error); return; }
    self->showReport(report);
  });
}

void LayerImportDialog::accept() {
  collect();
  if (choice_.target.isEmpty() && !QRegularExpression("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$").match(choice_.id).hasMatch()) {
    summary_->setText("캐릭터 ID는 영문 소문자로 시작하고 소문자·숫자·하이픈만 쓸 수 있습니다.");
    id_->setFocus();
    return;
  }
  QDialog::accept();
}
}  // namespace cutout_studio
