// Workspace view helpers (plugin 1.20.0), all editor-only presentation (QSettings
// plugins/cutout-animation/view, never an edit): the look pages' two-row page selector (every
// page reachable on a 1280 px window, no scrolling tab bar), 보기 ▾ 본 표시 / 본 흐리게 / 캔버스 안내
// 줄 with H cycling the bone display, and the 계층 / 그리는 순서 filters that find bones and parts by
// id or by the original PSD layer / group name the importer kept in rig.editor.layerImport.
#include "cutout-panel.hpp"
#include <QtWidgets>
#include <algorithm>

namespace cutout_studio {
using cutout::Json;
namespace {
constexpr const char* VIEW_SETTINGS = "plugins/cutout-animation/view";
QString refKind(const QString& ref) { return ref.section(':', 0, 0); }
QString refId(const QString& ref) { return ref.section(':', 1); }
/** Every whitespace-separated word of `query` somewhere in `hay` (case-insensitive). */
bool matchesAll(const QString& hay, const QString& query) {
  const QStringList words = query.split(' ', Qt::SkipEmptyParts);
  for (const QString& w : words) if (!hay.contains(w, Qt::CaseInsensitive)) return false;
  return !words.isEmpty();
}
}  // namespace

// ---------------------------------------------------------------- look page selector
QWidget* CutoutPanel::buildLookPageBar() {
  // Two rows of page buttons instead of the tab widget's single scrolling tab bar: at 1456x819 the
  // ten pages needed scroll arrows and 판정 was out of sight. The QTabWidget stays (its pages,
  // currentIndex / setCurrentWidget and the palette's "탭 열기" commands); only its bar is hidden.
  auto* bar = new QWidget;
  bar->setObjectName("cutoutLookPageBar");
  bar->setProperty("cutoutGroup", "보기");
  bar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  auto* grid = new QGridLayout(bar);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(2);
  grid->setVerticalSpacing(2);
  lookPageButtons_ = new QButtonGroup(bar);
  lookPageButtons_->setExclusive(true);
  const int pages = lookTabs_->count(), columns = (pages + 1) / 2;
  for (int i = 0; i < pages; i++) {
    auto* b = new QToolButton;
    b->setObjectName(lookTabs_->widget(i)->objectName() + "Tab");
    b->setText(lookTabs_->tabText(i));
    b->setCheckable(true);
    b->setToolButtonStyle(Qt::ToolButtonTextOnly);
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    b->setToolTip(QString("「%1」 페이지를 엽니다.").arg(lookTabs_->tabText(i)));
    b->setProperty("cutoutPageButton", true);  // the palette lists the page ("… 탭 열기"), not this button
    lookPageButtons_->addButton(b, i);
    grid->addWidget(b, i / columns, i % columns);
  }
  connect(lookPageButtons_, &QButtonGroup::idClicked, this, [this](int i) { lookTabs_->setCurrentIndex(i); });
  auto sync = [this](int i) { if (QAbstractButton* b = lookPageButtons_->button(i)) b->setChecked(true); };
  connect(lookTabs_, &QTabWidget::currentChanged, this, sync);
  sync(lookTabs_->currentIndex());
  lookTabs_->tabBar()->hide();
  return bar;
}

// ---------------------------------------------------------------- 보기 ▾ bone display, canvas hint
void CutoutPanel::buildBoneViewActions(QMenu* menu) {
  QSettings q;
  q.beginGroup(VIEW_SETTINGS);
  const int display = std::clamp(q.value("boneDisplay", int(RigCanvas::BonesAll)).toInt(), 0, 2);
  const bool dim = q.value("bonesDim", false).toBool(), hint = q.value("canvasHint", true).toBool();
  q.endGroup();
  QMenu* bones = menu->addMenu("본 표시");
  bones->setObjectName("cutoutBoneDisplayMenu");
  boneDisplayGroup_ = new QActionGroup(bones);
  boneDisplayGroup_->setExclusive(true);
  const struct { const char* text; const char* name; const char* tip; } modes[] = {
      {"모두", "cutoutBonesAll", "모든 본을 캔버스에 그립니다."},
      {"선택·관련만", "cutoutBonesSelected", "고른 본(조각이면 그 본)과 그 부모·자식, 같은 IK·변형 제약의 본만 그립니다. 그리지 않는 본은 캔버스에서 잡히지 않습니다(조각은 잡힘)."},
      {"숨김", "cutoutBonesHidden", "본을 그리지 않고 그림만 봅니다(계층에서 고른 본은 그려져 끌 수 있음). IK·변형 제약·물리 표시도 숨깁니다."},
  };
  for (int i = 0; i < 3; i++) {
    QAction* a = bones->addAction(i == 0 ? QString(modes[i].text) + "\tH" : QString(modes[i].text));
    a->setObjectName(modes[i].name);
    a->setProperty("cutoutGroup", "본 표시");
    a->setCheckable(true);
    a->setChecked(i == display);
    a->setToolTip(QString(modes[i].tip) + "\nH 키로 모두 → 선택·관련만 → 숨김을 차례로 바꿉니다(편집기 전용, 이 컴퓨터에 저장).");
    a->setData(i);
    boneDisplayGroup_->addAction(a);
    connect(a, &QAction::triggered, this, [this, i] { setBoneDisplay(i); });
  }
  bonesDimAction_ = menu->addAction("본 흐리게");
  bonesDimAction_->setObjectName("cutoutBonesDim");
  bonesDimAction_->setProperty("cutoutGroup", "보기");
  bonesDimAction_->setCheckable(true);
  bonesDimAction_->setChecked(dim);
  bonesDimAction_->setToolTip("고르지 않은 본을 반투명(40 %)하게 그려 그림이 잘 보이게 합니다(편집기 전용).");
  connect(bonesDimAction_, &QAction::toggled, this, [this](bool on) { setBonesDim(on); });
  canvasHintAction_ = menu->addAction("캔버스 안내 줄");
  canvasHintAction_->setObjectName("cutoutShowCanvasHint");
  canvasHintAction_->setProperty("cutoutGroup", "보기");
  canvasHintAction_->setCheckable(true);
  canvasHintAction_->setChecked(hint);
  canvasHintAction_->setToolTip("캔버스 아래의 도구 안내 줄(「도구: 이동(V) · …」)을 보이거나 숨깁니다(편집기 전용).");
  connect(canvasHintAction_, &QAction::toggled, this, [this](bool on) { setCanvasHintVisible(on); });
  if (canvas_) { canvas_->setBoneDisplay(display); canvas_->setBonesDim(dim); canvas_->setShowHint(hint); }
}
void CutoutPanel::setBoneDisplay(int mode) {
  mode = std::clamp(mode, 0, 2);
  if (canvas_) canvas_->setBoneDisplay(mode);
  if (boneDisplayGroup_)
    for (QAction* a : boneDisplayGroup_->actions()) if (a->data().toInt() == mode && !a->isChecked()) a->setChecked(true);
  QSettings q;
  q.beginGroup(VIEW_SETTINGS);
  q.setValue("boneDisplay", mode);
}
int CutoutPanel::boneDisplay() const { return canvas_ ? canvas_->boneDisplay() : 0; }
void CutoutPanel::cycleBoneDisplay() {
  static const char* names[] = {"모두", "선택·관련만", "숨김"};
  const int next = (boneDisplay() + 1) % 3;
  setBoneDisplay(next);
  setStatus(QString("본 표시: %1 (H로 바꿈 · 보기 ▾ › 본 표시)").arg(names[next]));
}
void CutoutPanel::setBonesDim(bool on) {
  if (canvas_) canvas_->setBonesDim(on);
  if (bonesDimAction_ && bonesDimAction_->isChecked() != on) { QSignalBlocker block(bonesDimAction_); bonesDimAction_->setChecked(on); }
  QSettings q;
  q.beginGroup(VIEW_SETTINGS);
  q.setValue("bonesDim", on);
}
bool CutoutPanel::bonesDim() const { return canvas_ && canvas_->bonesDim(); }
void CutoutPanel::setCanvasHintVisible(bool on) {
  if (canvas_) canvas_->setShowHint(on);
  if (canvasHintAction_ && canvasHintAction_->isChecked() != on) { QSignalBlocker block(canvasHintAction_); canvasHintAction_->setChecked(on); }
  QSettings q;
  q.beginGroup(VIEW_SETTINGS);
  q.setValue("canvasHint", on);
}
bool CutoutPanel::canvasHintVisible() const { return canvas_ && canvas_->showHint(); }

// ---------------------------------------------------------------- 계층 / 그리는 순서 filters
QLineEdit* CutoutPanel::buildFilterField(const char* name, const QString& tip, QWidget* list) {
  auto* field = new QLineEdit;
  field->setObjectName(name);
  field->setPlaceholderText("본·조각 찾기");
  field->setClearButtonEnabled(true);
  field->setToolTip(tip + "\nEnter = 첫 결과 고르기 · Esc = 지우기 · Ctrl+F = 이 칸으로(목록이나 캔버스에 초점이 있을 때)");
  field->installEventFilter(this);  // Esc clears (filterKey)
  connect(field, &QLineEdit::textChanged, this, [this, field] { if (field == treeFilter_) applyTreeFilter(); else applyOrderFilter(); });
  connect(field, &QLineEdit::returnPressed, this, [this, field] { pickFirstMatch(field); });
  focusFilterFrom(list, field);
  return field;
}
void CutoutPanel::focusFilterFrom(QWidget* from, QLineEdit* field) {
  // Ctrl+F while `from` has the focus (the host binds no Ctrl+F). Not a cutoutShortcut*: the
  // shortcut overlay lists it once, from the canvas binding table.
  if (!from || !field) return;
  auto* s = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), from);
  s->setObjectName(QString("cutoutFind") + field->objectName().mid(6) + (from == canvas_ ? "Canvas" : "List"));
  s->setContext(Qt::WidgetShortcut);
  connect(s, &QShortcut::activated, this, [field] { field->setFocus(Qt::ShortcutFocusReason); field->selectAll(); });
}
bool CutoutPanel::filterKey(QObject* watched, QEvent* e) {
  if ((watched != treeFilter_ && watched != orderFilter_) || e->type() != QEvent::KeyPress) return false;
  auto* field = static_cast<QLineEdit*>(watched);
  if (static_cast<QKeyEvent*>(e)->key() != Qt::Key_Escape) return false;
  if (!field->text().isEmpty()) field->clear();
  else if (QWidget* list = field == treeFilter_ ? static_cast<QWidget*>(treeBase_) : static_cast<QWidget*>(drawOrder_)) list->setFocus(Qt::OtherFocusReason);
  return true;
}
QStringList CutoutPanel::importedNames(const QString& ref) const {
  // rig.editor.layerImport (plugin 1.20.0 records the original names; older rigs have none).
  const Json& data = displayData();
  if (!data.contains("editor") || !data["editor"].is_object() || !data["editor"].contains("layerImport") || !data["editor"]["layerImport"].is_object()) return {};
  const Json& record = data["editor"]["layerImport"];
  QStringList out;
  const std::string id = ss(refId(ref));
  if (refKind(ref) == "bone") {
    if (record.contains("bones") && record["bones"].is_object() && record["bones"].contains(id) && record["bones"][id].is_string()) out << qs(record["bones"][id].get<std::string>());
  } else if (refKind(ref) == "slot" && record.contains("layers") && record["layers"].is_object()) {
    const std::string prefix = id + "/";
    for (const auto& [key, layer] : record["layers"].items())
      if (key.compare(0, prefix.size(), prefix) == 0 && layer.is_object() && layer.contains("name") && layer["name"].is_string()) out << qs(layer["name"].get<std::string>());
  }
  return out;
}
bool CutoutPanel::filterMatches(const QString& ref, const QString& query) const {
  QString hay = refId(ref);
  if (refKind(ref) == "slot") {
    const Json& data = displayData();
    if (data.contains("attachments") && data["attachments"].is_array())
      for (const auto& a : data["attachments"]) if (a.is_object() && qs(a.value("slot", std::string())) == refId(ref)) hay += " " + qs(a.value("name", std::string()));
  }
  for (const QString& n : importedNames(ref)) hay += " " + n;
  return matchesAll(hay, query);
}
void CutoutPanel::setTreeFilter(const QString& text) { if (treeFilter_) treeFilter_->setText(text); }
void CutoutPanel::setDrawOrderFilter(const QString& text) { if (orderFilter_) orderFilter_->setText(text); }
void CutoutPanel::applyTreeFilter() {
  if (!treeBase_ || !treeFilter_) return;
  const QString query = treeFilter_->text().simplified();
  QSignalBlocker block(treeBase_);  // font changes would come back as itemChanged (the eye column)
  const bool keep = filling_;
  filling_ = true;
  // Matches and their ancestors stay; matches are bold.
  std::function<bool(QTreeWidgetItem*)> walk = [&](QTreeWidgetItem* it) {
    bool below = false;
    for (int i = 0; i < it->childCount(); i++) below = walk(it->child(i)) || below;
    const bool match = !query.isEmpty() && filterMatches(it->data(0, Qt::UserRole).toString(), query);
    QFont f = it->font(0);
    if (f.bold() != match) { f.setBold(match); it->setFont(0, f); }
    it->setHidden(!query.isEmpty() && !match && !below);
    return match || below;
  };
  for (int i = 0; i < treeBase_->topLevelItemCount(); i++) walk(treeBase_->topLevelItem(i));
  filling_ = keep;
}
void CutoutPanel::applyOrderFilter() {
  if (!drawOrder_ || !orderFilter_) return;
  const QString query = orderFilter_->text().simplified();
  for (int i = 0; i < drawOrder_->count(); i++) {
    QListWidgetItem* item = drawOrder_->item(i);
    item->setHidden(!query.isEmpty() && !filterMatches("slot:" + item->data(Qt::UserRole).toString(), query));
  }
}
QStringList CutoutPanel::filterMatchRefs(bool tree) const {
  QStringList out;
  const QLineEdit* field = tree ? treeFilter_ : orderFilter_;
  const QString query = field ? field->text().simplified() : QString();
  if (query.isEmpty()) return out;
  if (tree) {
    for (QTreeWidgetItemIterator it(treeBase_); *it; ++it)
      if (!(*it)->isHidden() && filterMatches((*it)->data(0, Qt::UserRole).toString(), query)) out << (*it)->data(0, Qt::UserRole).toString();
  } else {
    for (int i = 0; i < drawOrder_->count(); i++)
      if (!drawOrder_->item(i)->isHidden()) out << "slot:" + drawOrder_->item(i)->data(Qt::UserRole).toString();
  }
  return out;
}
void CutoutPanel::pickFirstMatch(QLineEdit* field) {
  const QStringList matches = filterMatchRefs(field == treeFilter_);
  if (matches.isEmpty()) { setStatus(QString("「%1」에 맞는 본·조각이 없습니다.").arg(field->text().simplified()), true); return; }
  selectItem(matches.first());
}
}  // namespace cutout_studio
