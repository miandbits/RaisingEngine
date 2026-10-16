#include "layer-import.hpp"
#include <QtWidgets>

namespace cutout_studio {
namespace {
constexpr int KEY_ROLE = Qt::UserRole, ROLE_ROLE = Qt::UserRole + 1, MERGE_TAG_ROLE = Qt::UserRole + 2;
constexpr int COLUMNS = 7, MERGE_COLUMN = 6;
const char* const ORIGINS[][2] = {{"bottom-center", "아래 가운데 (발 사이)"}, {"center", "가운데"}, {"top-center", "위 가운데"}, {"top-left", "왼쪽 위"}};
}  // namespace

QJsonObject LayerImportChoice::payload() const {
  QJsonObject p{{"file", file}, {"origin", origin}, {"trim", trim}, {"scale", autoScale ? QJsonValue("auto") : QJsonValue(scale)}, {"boneDirection", aimBones ? "auto" : "none"},
                {"exclude", QJsonArray::fromStringList(exclude)}, {"merge", QJsonArray::fromStringList(merge)}, {"effects", skipEffects ? "skip" : "keep"}};
  if (target.isEmpty()) p["id"] = id;
  else p["rig"] = target;
  return p;
}

QString layerRoleLabel(const QString& role) {
  static const QHash<QString, QString> labels{{"image", "그림"}, {"bone", "본"}, {"pivot", "회전 중심 표시"}, {"slot-group", "슬롯 묶음"}, {"group-in-slot", "슬롯 묶음 안"},
                                             {"ignored", "무시([ignore])"}, {"excluded", "빼기"}, {"empty", "빈 레이어"}, {"empty-group", "빈 그룹"},
                                             {"clipped", "클리핑 (아래 그림에 합침)"}, {"merged", "합친 그림"}, {"in-merge", "합친 그림 안"}, {"effect", "효과 (뺌)"},
                                             {"box", "판정 상자 ([box])"}, {"point", "기준점 ([point])"}};
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
  if (const int joints = s.value("joints").toInt()) text += QString(" · 관절 짐작 %1").arg(joints);  // plugin 1.20.0
  if (const int baked = s.value("baked").toInt()) text += QString(" · 클리핑 %1개 합침").arg(baked);
  if (const int merged = s.value("merged").toInt()) text += QString(" · 그룹 %1개 한 그림으로").arg(merged);
  // Over the rig limits (plugin 1.15.0): the preview still shows, the import is refused until it fits.
  const QJsonObject limits = report.value("limits").toObject();
  if (limits.value("exceeded").toBool()) {
    QStringList over;
    for (const auto& [key, label] : {std::pair{"bones", "본"}, {"slots", "슬롯"}, {"attachments", "첨부"}}) {
      const QJsonObject l = limits.value(key).toObject();
      if (l.value("count").toInt() > l.value("max").toInt()) over << QString("%1 %2/%3").arg(label).arg(l.value("count").toInt()).arg(l.value("max").toInt());
    }
    text += QString(" · 한도를 넘음(%1): 그룹을 합쳐야 가져올 수 있습니다").arg(over.join(", "));
  }
  return text;
}

LayerImportDialog::LayerImportDialog(QWidget* parent, LayerImportChoice& choice, const QStringList& rigs, const QJsonObject& report, Preview preview)
    : QDialog(parent), choice_(choice), preview_(std::move(preview)) {
  setObjectName("cutoutLayerImportDialog");
  setWindowTitle("레이어 그림 가져오기 (PSD)");
  // Plugin 1.20.0: a production file has hundreds of layers: a large, resizable window by default.
  setSizeGripEnabled(true);
  resize(defaultSize(parent));
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
                        "[pivot]이 없는 그룹은 매달린 가장자리(관절: 머리는 목, 다리·머리채·소매는 위끝)를 원점으로 짐작합니다(1.20.0).\n"
                        "그림은 반대로 돌려 붙여 PSD와 똑같이 보입니다. 끄면 1.11.0처럼 모든 본이 오른쪽(+x)을 향하고 길이가 없습니다.\n다시 가져올 때 이미 있는 본은 바꾸지 않습니다.");
  scale_ = new QDoubleSpinBox;
  scale_->setObjectName("cutoutLayerImportScale");
  scale_->setRange(0.05, 4);
  scale_->setDecimals(2);
  scale_->setSingleStep(0.25);
  scale_->setValue(choice.scale);
  scale_->setToolTip("1보다 작으면 그림을 읽으면서 그 배율로 줄입니다(가장자리 알파가 깨끗한 면적 평균). 1보다 크면 그림은 그대로 두고 배치에 배율을 곱합니다.\n"
                     "갱신할 때는 처음 가져올 때의 배율을 씁니다.");
  scale_->setEnabled(!choice.autoScale);
  autoScale_ = new QCheckBox("자동");
  autoScale_->setObjectName("cutoutLayerImportAutoScale");
  autoScale_->setChecked(choice.autoScale);
  autoScale_->setToolTip("캔버스 긴 변이 2048 px 이하, 그림이 64메가픽셀 이하가 되는 가장 큰 배율(1, 0.75, 0.5, 0.375, 0.25, 0.125)을 고릅니다. 작은 PSD는 1입니다.");
  connect(autoScale_, &QCheckBox::toggled, this, [this](bool on) { scale_->setEnabled(!on); });
  scaleInfo_ = new QLabel;
  scaleInfo_->setObjectName("cutoutLayerImportScaleInfo");
  auto* scaleRow = new QHBoxLayout;
  scaleRow->addWidget(scale_);
  scaleRow->addWidget(autoScale_);
  scaleRow->addWidget(scaleInfo_, 1);
  skipEffects_ = new QCheckBox("효과 레이어 빼기 (곱하기·선형 닷지·스크린 같은 덧칠 레이어)");
  skipEffects_->setObjectName("cutoutLayerImportSkipEffects");
  skipEffects_->setChecked(choice.skipEffects);
  skipEffects_->setToolTip("클리핑되지 않은, 표준이 아닌 혼합 모드 레이어(그림자·빛 덧칠)는 리그에서 표준으로만 그릴 수 있습니다.\n"
                           "켜면 가져오지 않습니다(effects \"skip\"). 살리려면 그 그룹을 한 그림으로 합치세요. 클리핑 레이어는 항상 아래 그림에 합칩니다.");
  form->addRow("가져올 곳", target_);
  form->addRow("캐릭터 ID", id_);
  form->addRow("원점", origin_);
  form->addRow("배율", scaleRow);
  form->addRow(QString(), trim_);
  form->addRow(QString(), aimBones_);
  form->addRow(QString(), skipEffects_);
  layout->addLayout(form);
  connect(target_, &QComboBox::currentIndexChanged, this, [this] { id_->setEnabled(target_->currentData().toString().isEmpty()); });
  id_->setEnabled(choice.target.isEmpty());

  tree_ = new QTreeWidget;
  tree_->setObjectName("cutoutLayerImportTree");
  tree_->setHeaderLabels({"레이어 (체크 = 가져오기)", "역할", "본", "슬롯", "첨부", "상태", "한 그림으로"});
  tree_->headerItem()->setToolTip(MERGE_COLUMN, "그룹을 체크하면 그 안의 보이는 레이어를 혼합 모드·클리핑까지 합성해 그림 하나(슬롯 하나)로 가져옵니다(merge). 이름에 [merge]를 붙여도 됩니다.");
  tree_->setRootIsDecorated(true);
  tree_->setUniformRowHeights(true);
  tree_->setMinimumSize(560, 260);
  // The same toggle as the column, from the row's context menu.
  mergeAction_ = new QAction("한 그림으로 합치기", tree_);
  mergeAction_->setObjectName("cutoutLayerImportMergeAction");
  mergeAction_->setCheckable(true);
  tree_->addAction(mergeAction_);
  tree_->setContextMenuPolicy(Qt::ActionsContextMenu);
  connect(tree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
    const bool group = item && item->data(MERGE_COLUMN, Qt::CheckStateRole).isValid();
    mergeAction_->setEnabled(group && !item->data(0, MERGE_TAG_ROLE).toBool());
    mergeAction_->setChecked(group && item->checkState(MERGE_COLUMN) == Qt::Checked);
  });
  connect(mergeAction_, &QAction::triggered, this, [this](bool on) {
    QTreeWidgetItem* item = tree_->currentItem();
    if (!item || !item->data(MERGE_COLUMN, Qt::CheckStateRole).isValid()) return;
    item->setCheckState(MERGE_COLUMN, on ? Qt::Checked : Qt::Unchecked);
    refresh();
  });
  connect(tree_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
    if (filling_) return;
    if (column == MERGE_COLUMN) {
      // A [merge] name always merges: its box stays checked.
      if (item->data(0, MERGE_TAG_ROLE).toBool() && item->checkState(MERGE_COLUMN) != Qt::Checked) { filling_ = true; item->setCheckState(MERGE_COLUMN, Qt::Checked); filling_ = false; }
      return;
    }
    if (column != 0) return;
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
  // Plugin 1.20.0: find layers by name, open or fold every group.
  auto* treeTools = new QHBoxLayout;
  filter_ = new QLineEdit;
  filter_->setObjectName("cutoutLayerImportFilter");
  filter_->setPlaceholderText("레이어 이름 찾기");
  filter_->setClearButtonEnabled(true);
  filter_->setToolTip("이름에 모든 낱말이 들어 있는 레이어와 그 위 그룹만 보입니다(체크·합치기는 숨은 줄도 그대로).");
  connect(filter_, &QLineEdit::textChanged, this, [this] { applyFilter(); });
  auto* expand = new QPushButton("모두 펼치기");
  expand->setObjectName("cutoutLayerImportExpandAll");
  expand->setToolTip("모든 그룹을 펼칩니다.");
  connect(expand, &QPushButton::clicked, tree_, &QTreeWidget::expandAll);
  auto* collapse = new QPushButton("모두 접기");
  collapse->setObjectName("cutoutLayerImportCollapseAll");
  collapse->setToolTip("모든 그룹을 접어 맨 위 단계만 보입니다.");
  connect(collapse, &QPushButton::clicked, tree_, &QTreeWidget::collapseAll);
  treeTools->addWidget(filter_, 1);
  treeTools->addWidget(expand);
  treeTools->addWidget(collapse);
  layout->addLayout(treeTools);
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
  auto* hint = new QLabel("그룹 = 본, 레이어 = 슬롯·그림(ID는 영문 이름에서, 러시아어·그리스어는 로마자로, 한글만 있으면 part-N). 「eyes/open」「eyes@closed」처럼 나누면 같은 슬롯의 바꿔 끼우는 그림이 되고, "
                          "숨긴 레이어는 숨긴 첨부(표정용)로 들어옵니다. 클리핑 레이어는 아래 그림에 합쳐집니다. [pivot] 레이어 = 그 그룹 본의 회전 중심, [slot] 그룹 = 한 슬롯, [merge] 그룹 = 한 그림, "
                          "[ignore] = 빼기. 가져오기는 편집 한 번이라 Ctrl+Z로 되돌립니다.");
  hint->setObjectName("cutoutLayerImportHint");
  hint->setWordWrap(true);
  layout->addWidget(hint);
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  ok_ = buttons->button(QDialogButtonBox::Ok);
  ok_->setText("가져오기");
  auto* again = buttons->addButton("미리 보기 갱신", QDialogButtonBox::ActionRole);
  again->setObjectName("cutoutLayerImportRefresh");
  again->setToolTip("빼기·합치기·원점·배율·잘라내기·본 방향·효과를 바꾼 뒤 본·슬롯 배치를 다시 계산해 봅니다.");
  connect(again, &QPushButton::clicked, this, [this] { refresh(); });
  autoMerge_ = buttons->addButton("자동 합치기", QDialogButtonBox::ActionRole);
  autoMerge_->setObjectName("cutoutLayerImportAutoMerge");
  autoMerge_->setToolTip("본·슬롯·첨부 한도(256·256·1024)를 넘을 때: 그림이 가장 많은 맨 아래 그룹부터 한 그림으로 합쳐 한도 안에 들게 합니다\n"
                         "(숨긴 표정 그림이 있는 그룹은 마지막에). 합친 그룹은 「한 그림으로」 칸에서 다시 풀 수 있습니다.");
  connect(autoMerge_, &QPushButton::clicked, this, [this] { autoMerge(); });
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
    // "한 그림으로" (plugin 1.15.0): any group outside a merged one; a [merge] name is always on.
    if (l.value("kind").toString() == "group" && role != "in-merge" && role != "clipped" && role != "ignored") {
      const bool tag = l.value("merge").toString() == "tag";
      item->setData(0, MERGE_TAG_ROLE, tag);
      item->setCheckState(MERGE_COLUMN, tag || choice_.merge.contains(l.value("key").toString()) ? Qt::Checked : Qt::Unchecked);
    }
    QStringList tip;
    const QJsonObject b = l.value("bounds").toObject();
    if (!b.isEmpty()) tip << QString("위치 %1, %2 · 크기 %3×%4").arg(b.value("x").toInt()).arg(b.value("y").toInt()).arg(b.value("width").toInt()).arg(b.value("height").toInt());
    if (l.value("opacity").toDouble(1) < 1) tip << QString("불투명도 %1%").arg(qRound(l.value("opacity").toDouble() * 100));
    if (l.value("blend").toString() != "norm" && l.value("blend").toString() != "pass" && !l.value("blend").toString().isEmpty()) tip << "혼합 모드 " + l.value("blend").toString().trimmed();
    for (const auto& w : l.value("warnings").toArray()) tip << w.toString();
    if (!tip.isEmpty()) for (int c = 0; c < COLUMNS; c++) item->setToolTip(c, tip.join("\n"));
    if (!l.value("warnings").toArray().isEmpty()) item->setForeground(1, QColor(200, 120, 0));
    addItems(item, l.value("children").toArray());
  }
}

void LayerImportDialog::showReport(const QJsonObject& report) {
  filling_ = true;
  report_ = report;
  const QJsonObject s = report.value("source").toObject();
  source_->setText(QString("%1 · %2 %3×%4 px").arg(s.value("file").toString(), s.value("format").toString().toUpper()).arg(s.value("width").toInt()).arg(s.value("height").toInt()));
  // The scale the service chose ("auto" on a big file = a downscale; the preview has no pixels yet).
  const QJsonObject o = report.value("options").toObject(), canvas = report.value("canvas").toObject();
  const QString mode = o.value("scaleMode").toString();
  const double scale = o.value("scale").toDouble(choice_.scale);
  if (mode == "auto") scale_->setValue(scale);
  QString info = QString("%1%2배 → 캔버스 %3×%4 px").arg(mode == "auto" ? "자동 " : mode == "kept" ? "처음 배율 " : "").arg(scale).arg(canvas.value("width").toDouble()).arg(canvas.value("height").toDouble());
  if (o.value("resolution").toDouble(1) < 1) info += QString(" · 그림을 %1배로 줄여 읽음").arg(o.value("resolution").toDouble());
  scaleInfo_->setText(o.isEmpty() ? QString() : info);
  tree_->clear();
  addItems(nullptr, report.value("layers").toArray());
  tree_->expandAll();
  for (int c = 0; c < COLUMNS; c++) tree_->resizeColumnToContents(c);
  applyFilter();
  warnings_->clear();
  for (const auto& w : report.value("warnings").toArray()) warnings_->addItem(w.toObject().value("message").toString());
  if (!warnings_->count()) warnings_->addItem("없음");
  const bool over = report.value("limits").toObject().value("exceeded").toBool();
  const QJsonArray proposal = report.value("autoMerge").toObject().value("keys").toArray();
  autoMerge_->setEnabled(over && !proposal.isEmpty());
  autoMerge_->setText(over && !proposal.isEmpty() ? QString("자동 합치기 (그룹 %1개)").arg(proposal.size()) : QString("자동 합치기"));
  ok_->setEnabled(!over);
  summary_->setText(layerImportSummary(report));
  summary_->setStyleSheet(over ? "color: rgb(200, 60, 40);" : QString());
  filling_ = false;
}

void LayerImportDialog::collect() {
  choice_.target = target_->currentData().toString();
  choice_.id = id_->text().trimmed();
  choice_.origin = origin_->currentData().toString();
  choice_.trim = trim_->isChecked();
  choice_.aimBones = aimBones_->isChecked();
  choice_.autoScale = autoScale_->isChecked();
  choice_.scale = scale_->value();
  choice_.skipEffects = skipEffects_->isChecked();
  choice_.exclude.clear();
  choice_.merge.clear();
  for (QTreeWidgetItemIterator m(tree_); *m; ++m)
    if ((*m)->data(MERGE_COLUMN, Qt::CheckStateRole).isValid() && (*m)->checkState(MERGE_COLUMN) == Qt::Checked && !(*m)->data(0, MERGE_TAG_ROLE).toBool())
      choice_.merge << (*m)->data(0, KEY_ROLE).toString();
  QTreeWidgetItemIterator it(tree_);
  for (; *it; ++it)
    if ((*it)->checkState(0) != Qt::Checked && (*it)->data(0, ROLE_ROLE).toString() != "ignored") {
      // Only the topmost left-out item: its subtree goes with it.
      bool covered = false;
      for (QTreeWidgetItem* p = (*it)->parent(); p && !covered; p = p->parent()) covered = p->checkState(0) != Qt::Checked;
      if (!covered) choice_.exclude << (*it)->data(0, KEY_ROLE).toString();
    }
}

void LayerImportDialog::refresh(bool fromWidgets) {
  if (fromWidgets) collect();
  if (!preview_) return;
  QPointer<LayerImportDialog> self(this);
  preview_(choice_, [self](const QJsonObject& report, const QString& error) {
    if (!self) return;
    if (!error.isEmpty()) { self->summary_->setText("미리 보기를 만들 수 없습니다: " + error); return; }
    self->showReport(report);
  });
}

void LayerImportDialog::autoMerge() {
  collect();
  for (const auto& key : report_.value("autoMerge").toObject().value("keys").toArray())
    if (!choice_.merge.contains(key.toString())) choice_.merge << key.toString();
  refresh(false);  // the tree does not show the proposal yet: preview the choice as it is
}

void LayerImportDialog::setFilter(const QString& text) { filter_->setText(text); }
void LayerImportDialog::applyFilter() {
  const QStringList words = filter_ ? filter_->text().split(' ', Qt::SkipEmptyParts) : QStringList();
  std::function<bool(QTreeWidgetItem*)> walk = [&](QTreeWidgetItem* it) {
    bool below = false;
    for (int i = 0; i < it->childCount(); i++) below = walk(it->child(i)) || below;
    bool match = !words.isEmpty();
    for (const QString& w : words) match = match && it->text(0).contains(w, Qt::CaseInsensitive);
    it->setHidden(!words.isEmpty() && !match && !below);
    if (below && !words.isEmpty()) it->setExpanded(true);  // a match inside a folded group shows
    return match || below;
  };
  for (int i = 0; i < tree_->topLevelItemCount(); i++) walk(tree_->topLevelItem(i));
}
QSize LayerImportDialog::defaultSize(const QWidget* parent) {
  const QScreen* screen = parent && parent->screen() ? parent->screen() : QGuiApplication::primaryScreen();
  const QSize avail = screen ? screen->availableGeometry().size() : QSize(1280, 720);
  const QSize least(std::min(900, avail.width()), std::min(600, avail.height()));
  QSettings q;
  q.beginGroup(SIZE_SETTINGS);
  const QSize saved = q.value("size").toSize();
  if (saved.isValid()) return saved.expandedTo(least).boundedTo(avail);
  return QSize(int(avail.width() * 0.7), int(avail.height() * 0.7)).expandedTo(least).boundedTo(avail);
}
void LayerImportDialog::done(int result) {
  QSettings q;
  q.beginGroup(SIZE_SETTINGS);
  q.setValue("size", size());
  QDialog::done(result);
}

void LayerImportDialog::accept() {
  collect();
  if (report_.value("limits").toObject().value("exceeded").toBool()) {
    summary_->setText(layerImportSummary(report_) + ". 「자동 합치기」나 「한 그림으로」 칸으로 그룹을 합친 뒤 미리 보기를 갱신하세요.");
    return;
  }
  if (choice_.target.isEmpty() && !QRegularExpression("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$").match(choice_.id).hasMatch()) {
    summary_->setText("캐릭터 ID는 영문 소문자로 시작하고 소문자·숫자·하이픈만 쓸 수 있습니다.");
    id_->setFocus();
    return;
  }
  QDialog::accept();
}
}  // namespace cutout_studio
