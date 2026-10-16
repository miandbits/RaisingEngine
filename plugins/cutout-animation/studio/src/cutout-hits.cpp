// Bounding boxes and points authoring (plugin 1.17.0): the 판정 page of the look section, its
// canvas gestures (기준점 placement, box vertex editing with the mesh vertex tools, point drags) and
// the game-connection preview click. Every gesture is ONE core edit (addBoundingBox,
// setBoundingBox, addPoint, setAttachment, deleteSlot), so the MCP/LLM `edit` op builds the same
// documents and undo/redo has one step per gesture. Boxes and points draw nothing in any runtime:
// the canvas shows them as editor-only overlays (보기 menu toggles, QSettings).
#include "cutout-panel.hpp"
#include <QtWidgets>
#include <algorithm>
#include <cmath>
#include "cutout/editing.hpp"

namespace cutout_studio {
using cutout::Json;
namespace {
constexpr const char* VIEW_SETTINGS = "plugins/cutout-animation/view";
QString refKind(const QString& ref) { return ref.section(':', 0, 0); }
QString refId(const QString& ref) { return ref.section(':', 1); }
QWidget* hitRow(const char* name) {
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
}  // namespace

QWidget* CutoutPanel::buildHits() {
  QWidget* page = lookPage("cutoutHitPage");
  page->setProperty("cutoutGroup", "판정");
  auto* layout = static_cast<QVBoxLayout*>(page->layout());
  QWidget* tools = hitRow("cutoutHitTools");
  auto button = [&](QWidget* row, const QString& text, const char* name, const QString& tip, std::function<bool()> fn) {
    auto* b = new QPushButton(text);
    b->setObjectName(name);
    b->setToolTip(tip);
    connect(b, &QPushButton::clicked, this, [fn] { fn(); });
    addTo(row, b);
    return b;
  };
  hitBoxButton_ = button(tools, "판정 상자", "cutoutAddBoundingBox",
                         "조립 모드: 고른 조각 위에 클릭 판정 상자를 만듭니다(그림의 불투명한 윤곽을 따라감, 그림이 없으면 사각형). 본을 골랐으면 그 본에 사각형.\n"
                         "게임에서 이 상자를 클릭하면 「게임 연결」의 partClicked 신호가 납니다(연출 전용, 게임 규칙·난수와 무관).",
                         [this] { return addBoundingBox(); });
  hitPointButton_ = new QPushButton("기준점");
  hitPointButton_->setObjectName("cutoutAddPoint");
  hitPointButton_->setCheckable(true);
  hitPointButton_->setToolTip("조립 모드: 누른 뒤 캔버스를 클릭하면 고른 본(조각이면 그 본)에 기준점이 생깁니다. 끌어서 옮기고, 끝의 둥근 손잡이로 방향을 돌립니다.\n"
                              "게임 화면은 기준점의 위치로 말풍선 같은 UI를 붙일 수 있습니다. Esc = 취소");
  connect(hitPointButton_, &QPushButton::clicked, this, [this](bool on) { if (on) beginPointPlacement(); else { if (canvas_) canvas_->setPointPlacing(false); refreshHits(); } });
  addTo(tools, hitPointButton_);
  hitVerticesButton_ = new QPushButton("정점 편집");
  hitVerticesButton_->setObjectName("cutoutBoxVertices");
  hitVerticesButton_->setCheckable(true);
  hitVerticesButton_->setProperty("cutoutShortcut", "G");  // plugin 1.19.0
  hitVerticesButton_->setToolTip("조립 모드: 고른 판정 상자의 점을 캔버스에서 고치고(끌기·상자 선택), 「점 추가」로 변에 점을 넣고, Delete로 지웁니다.");
  connect(hitVerticesButton_, &QPushButton::toggled, this, [this](bool on) { if (!filling_) setBoxVertexEdit(on); });
  addTo(tools, hitVerticesButton_);
  hitDeleteButton_ = button(tools, "삭제", "cutoutHitDelete", "고른 판정 상자·기준점 조각을 지웁니다.", [this] { return deleteHitPart(); });
  // Shown only while 정점 편집 is on (conditional row: the layout checks skip it when hidden).
  hitVertexRow_ = hitRow("cutoutBoxVertexTools");
  hitVertexRow_->setProperty("cutoutConditionalRow", true);
  hitAddVertexButton_ = new QPushButton("점 추가");
  hitAddVertexButton_->setObjectName("cutoutBoxAddVertex");
  hitAddVertexButton_->setCheckable(true);
  hitAddVertexButton_->setToolTip("판정 상자 변 가까이를 클릭하면 그 변에 점이 생깁니다(다시 누르면 선택 도구).");
  connect(hitAddVertexButton_, &QPushButton::toggled, this, [this](bool on) { if (!filling_) { boxTool_ = on ? int(RigCanvas::MeshAddVertex) : int(RigCanvas::MeshSelect); refreshMesh(); } });
  addTo(hitVertexRow_, hitAddVertexButton_);
  button(hitVertexRow_, "점 삭제", "cutoutBoxDeleteVertices", "고른 점을 지웁니다(3개는 남아야 함).", [this] { return deleteVertices(); });
  hitList_ = new QListWidget;
  hitList_->setObjectName("cutoutHitList");
  hitList_->setMinimumHeight(LIST_MIN_HEIGHT);
  hitList_->setToolTip("이 캐릭터의 판정 상자(▢)와 기준점(✛): 조각 · 첨부 이름 · 본. 고르면 캔버스·트리에서 선택됩니다.");
  connect(hitList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) { if (!filling_ && item) selectItem("slot:" + item->data(Qt::UserRole).toString()); });
  auto* inner = new QWidget;
  auto* formLayout = new QVBoxLayout(inner);
  formLayout->setContentsMargins(0, 0, 0, 0);
  hitInfo_ = new QLabel;
  hitInfo_->setObjectName("cutoutHitInfo");
  hitInfo_->setWordWrap(true);
  hitInfo_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  hitInfo_->setStyleSheet("color:#a7b4c2;");
  formLayout->addWidget(hitInfo_);
  formLayout->addStretch();
  layout->addWidget(tools);
  layout->addWidget(hitVertexRow_);
  layout->addWidget(hitList_, 2);
  layout->addWidget(formScroll("cutoutHitFormScroll", inner), 1);
  hitVertexRow_->setVisible(false);
  return page;
}

QString CutoutPanel::boxSlot() const {
  int si;
  const cutout::Attachment* att;
  Json fields;
  return meshTarget(si, att, fields) && att->boundingBox ? refId(selection_) : QString();
}
QString CutoutPanel::pointSlot() const {
  int si;
  const cutout::Attachment* att;
  Json fields;
  return meshTarget(si, att, fields) && att->point ? refId(selection_) : QString();
}
bool CutoutPanel::boxEditing() const { return boxEdit_ && mode_ == Mode::Setup && !boxSlot().isEmpty(); }

void CutoutPanel::refreshHits() {
  if (!hitList_) return;
  const bool wasFilling = filling_;
  filling_ = true;
  const bool setup = mode_ == Mode::Setup;
  const QString box = boxSlot(), point = pointSlot();
  if (!setup || box.isEmpty()) { boxEdit_ = false; boxTool_ = int(RigCanvas::MeshSelect); }
  if (!setup && canvas_ && canvas_->pointPlacing()) canvas_->setPointPlacing(false);
  {
    QSignalBlocker block(hitList_);
    hitList_->clear();
    const cutout::Pose pose = rig_ ? currentPose(false) : cutout::Pose();
    for (size_t i = 0; rig_ && i < rig_->slots.size(); i++) {
      const auto& slot = rig_->slots[i];
      // Every box / point a slot can show (default list and skins), so hidden ones are listed too.
      QStringList names;
      bool isBox = false;
      auto scan = [&](const std::vector<cutout::Attachment>& list) {
        for (const auto& a : list) if (a.slot == slot.id && (a.boundingBox || a.point) && !names.contains(qs(a.name))) { names << qs(a.name); isBox = isBox || a.boundingBox; }
      };
      scan(rig_->attachments);
      for (const auto& skin : rig_->skins) scan(skin.attachments);
      if (names.isEmpty()) continue;
      auto* item = new QListWidgetItem(QString("%1 %2 · %3 · 본 %4").arg(isBox ? "▢" : "✛", qs(slot.id), names.join(", "), qs(slot.bone)), hitList_);
      item->setData(Qt::UserRole, qs(slot.id));
      if ("slot:" + qs(slot.id) == selection_) hitList_->setCurrentItem(item);
    }
  }
  int si = -1;
  const cutout::Attachment* att = nullptr;
  Json fields;
  const bool target = meshTarget(si, att, fields);
  hitBoxButton_->setEnabled(setup && rig_ && !rig_->bones.empty());
  hitPointButton_->setEnabled(setup && rig_ && !rig_->bones.empty());
  hitPointButton_->setChecked(canvas_ && canvas_->pointPlacing());
  hitVerticesButton_->setEnabled(setup && !box.isEmpty());
  hitVerticesButton_->setChecked(boxEditing());
  hitDeleteButton_->setEnabled(!box.isEmpty() || !point.isEmpty());
  hitVertexRow_->setVisible(boxEditing());
  hitAddVertexButton_->setChecked(boxTool_ == int(RigCanvas::MeshAddVertex));
  QString info = "조각(▣)을 고르고 「판정 상자」를 누르면 그 그림의 윤곽을 따라 클릭 판정 상자가 생깁니다. 「게임 연결」에서 partClicked(box=이름) 신호에 클립·표정을 연결하세요. "
                 "「기준점」은 말풍선 같은 UI를 붙일 자리(위치·방향)입니다. 둘 다 게임 화면에 그려지지 않습니다.";
  if (canvas_ && canvas_->pointPlacing()) info = "기준점 놓기: 캔버스를 클릭하면 그 자리에 기준점이 생깁니다(Esc = 취소).";
  else if (target && att->boundingBox)
    info = QString("판정 상자 「%1」(조각 %2) · 점 %3개%4. 그림·뼈를 따라 움직이고, 게임에서 클릭하면 partClicked {box: %1, slot: %2} 신호가 납니다(위에 그려지는 상자가 먼저).")
               .arg(qs(att->name), qs(rig_->slots[size_t(si)].id)).arg(att->vertexCount()).arg(att->weights.empty() ? QString() : QString(" · 웨이트"));
  else if (target && att->point) {
    QPointF at;
    double rotation = 0;
    pointOf(*rig_, currentPose(false), si, at, rotation);
    info = QString("기준점 「%1」(조각 %2) · 본 공간 (%3, %4) %5° · 화면 (%6, %7) %8°. 끌어서 옮기고 끝 손잡이로 방향을 돌립니다.")
               .arg(qs(att->name), qs(rig_->slots[size_t(si)].id)).arg(att->offset.x, 0, 'f', 1).arg(att->offset.y, 0, 'f', 1).arg(att->offset.rotation, 0, 'f', 1)
               .arg(at.x(), 0, 'f', 1).arg(at.y(), 0, 'f', 1).arg(rotation, 0, 'f', 1);
  }
  if (!setup) info += " (만들기·모양 고치기는 조립 모드에서 합니다.)";
  hitInfo_->setText(info);
  filling_ = wasFilling;
}

QString CutoutPanel::uniqueSlotId(const QString& base) const {
  QString id = base;
  for (int n = 2; rig_ && rig_->slotIndex(ss(id)) >= 0; n++) id = QString("%1-%2").arg(base).arg(n);
  return id;
}

bool CutoutPanel::addBoundingBox() {
  if (mode_ != Mode::Setup) { setStatus("판정 상자는 조립 모드에서 만듭니다.", true); return false; }
  if (!rig_ || rig_->bones.empty()) { setStatus("캐릭터(본)가 먼저 있어야 합니다.", true); return false; }
  int si;
  const cutout::Attachment* att;
  Json fields;
  Json op = {{"op", "addBoundingBox"}};
  QString base;
  if (meshTarget(si, att, fields) && !att->pictureless()) {
    // A part: outline its picture (the same alpha tracing as 자동 윤곽, the 메시 page's threshold /
    // detail), placed right above it on its bone. A part shown from a skin outlines its rectangle.
    const QString part = qs(rig_->slots[size_t(si)].id);
    base = part + "-box";
    op["from"] = ss(part);
    if (fields.contains("skin")) op["fromName"] = att->name;
    if (const AlphaMask* mask = fields.contains("skin") ? nullptr : alphaMaskOf(*att)) {
      const QByteArray bytes(reinterpret_cast<const char*>(mask->alpha.data()), qsizetype(mask->alpha.size()));
      op["mask"] = {{"width", mask->width}, {"height", mask->height}, {"alpha", bytes.toBase64().toStdString()}};
      op["alphaThreshold"] = outlineThreshold_->value();
      op["detail"] = outlineDetail_->value();
    }
  } else {
    QString bone = selectedBone();
    if (rig_->boneIndex(ss(bone)) < 0) bone = qs(rig_->bones[size_t(rig_->evalOrder.empty() ? 0 : rig_->evalOrder[0])].id);
    base = bone + "-box";
    op["bone"] = ss(bone);
  }
  const QString id = uniqueSlotId(base);
  op["slot"] = ss(id);
  const QString before = selection_;
  selection_ = "slot:" + id;
  int points = 0;
  if (!runOps({op}, false, [&](const cutout::EditResult& r) { points = r.extra.value("vertices", 0); })) { selection_ = before; return false; }
  setStatus(QString("판정 상자 「%1」을 만들었습니다(점 %2개). 「정점 편집」으로 모양을 고치고, 게임 연결에서 partClicked(box=box)에 연결하세요. 되돌리기: Ctrl+Z").arg(id).arg(points));
  return true;
}
bool CutoutPanel::beginPointPlacement() {
  if (mode_ != Mode::Setup) { setStatus("기준점은 조립 모드에서 놓습니다.", true); refreshHits(); return false; }
  if (!rig_ || rig_->bones.empty() || !canvas_) { setStatus("캐릭터(본)가 먼저 있어야 합니다.", true); refreshHits(); return false; }
  setBoxVertexEdit(false);
  canvas_->setPointPlacing(true);
  canvas_->setFocus(Qt::OtherFocusReason);
  refreshHits();
  setStatus("기준점: 캔버스를 클릭한 곳에 기준점이 생깁니다(고른 본에 붙음). Esc = 취소");
  return true;
}
bool CutoutPanel::addPointAt(const QPointF& rigPoint) {
  if (canvas_) canvas_->setPointPlacing(false);
  if (mode_ != Mode::Setup || !rig_ || rig_->bones.empty()) { refreshHits(); return false; }
  QString bone = selectedBone();
  if (rig_->boneIndex(ss(bone)) < 0) bone = qs(rig_->bones[size_t(rig_->evalOrder.empty() ? 0 : rig_->evalOrder[0])].id);
  const QString id = uniqueSlotId(bone + "-point");
  const QString before = selection_;
  selection_ = "slot:" + id;
  if (!runOps({{{"op", "addPoint"}, {"slot", ss(id)}, {"bone", ss(bone)}, {"worldX", rigPoint.x()}, {"worldY", rigPoint.y()}}}, false)) { selection_ = before; refreshHits(); return false; }
  setStatus(QString("기준점 「%1」을 본 「%2」에 놓았습니다. 끌어서 옮기고 끝 손잡이로 방향을 돌립니다. 되돌리기: Ctrl+Z").arg(id, bone));
  return true;
}
bool CutoutPanel::deleteHitPart() {
  const QString slot = !boxSlot().isEmpty() ? boxSlot() : pointSlot();
  if (slot.isEmpty()) { setStatus("지울 판정 상자·기준점 조각을 고르세요.", true); return false; }
  if (!askConfirm("판정 조각 삭제", "「" + slot + "」 조각을 지울까요? 이 상자·기준점을 쓰는 게임 연결은 더 이상 실행되지 않습니다.")) return false;
  boxEdit_ = false;
  return runOps({{{"op", "deleteSlot"}, {"slot", ss(slot)}}}, false);
}
void CutoutPanel::setBoxVertexEdit(bool on) {
  if (on && (mode_ != Mode::Setup || boxSlot().isEmpty())) { setStatus("판정 상자를 고르고 조립 모드에서 정점을 고칩니다.", true); on = false; }
  boxEdit_ = on;
  if (!on) boxTool_ = int(RigCanvas::MeshSelect);
  if (on && canvas_ && canvas_->pointPlacing()) canvas_->setPointPlacing(false);
  meshSelection_.clear();
  refreshMesh();
  refreshHits();
}
void CutoutPanel::setShowBoxes(bool on) {
  if (canvas_) canvas_->setShowBoxes(on);
  if (showBoxesAction_ && showBoxesAction_->isChecked() != on) { QSignalBlocker block(showBoxesAction_); showBoxesAction_->setChecked(on); }
  QSettings q;
  q.beginGroup(VIEW_SETTINGS);
  q.setValue("showBoxes", on);
}
void CutoutPanel::setShowPoints(bool on) {
  if (canvas_) canvas_->setShowPoints(on);
  if (showPointsAction_ && showPointsAction_->isChecked() != on) { QSignalBlocker block(showPointsAction_); showPointsAction_->setChecked(on); }
  QSettings q;
  q.beginGroup(VIEW_SETTINGS);
  q.setValue("showPoints", on);
}
void CutoutPanel::buildHitViewActions(QMenu* menu) {
  QSettings q;
  q.beginGroup(VIEW_SETTINGS);
  const bool boxes = q.value("showBoxes", true).toBool(), points = q.value("showPoints", true).toBool();
  q.endGroup();
  auto toggle = [&](const QString& text, const char* name, bool on, const QString& tip, std::function<void(bool)> fn) {
    QAction* a = menu->addAction(text);
    a->setObjectName(name);
    a->setProperty("cutoutGroup", "보기");
    a->setCheckable(true);
    a->setChecked(on);
    a->setToolTip(tip);
    connect(a, &QAction::toggled, this, std::move(fn));
    return a;
  };
  showBoxesAction_ = toggle("판정 상자 보이기", "cutoutShowBoxes", boxes, "캔버스에 판정 상자를 반투명 점선으로 보여 줍니다(편집기 전용, 게임에는 그려지지 않음).", [this](bool on) { setShowBoxes(on); });
  showPointsAction_ = toggle("기준점 보이기", "cutoutShowPoints", points, "캔버스에 기준점을 십자(+방향)로 보여 줍니다(편집기 전용).", [this](bool on) { setShowPoints(on); });
  if (canvas_) { canvas_->setShowBoxes(boxes); canvas_->setShowPoints(points); }
}

// ---------------------------------------------------------------- box vertex editing
/** setBoundingBox with the selected points moved by `rigDelta` (drag-start snapshot): each point's
 *  own affine map is inverted, so it follows the cursor whatever its bones and weights do. */
std::vector<Json> CutoutPanel::boxMoveOps(const QPointF& rigDelta) const {
  if (!dragRig_ || dragPose_.world.size() != dragRig_->bones.size() || meshSelection_.isEmpty() || mode_ != Mode::Setup) return {};
  int si;
  const cutout::Attachment* shown;
  Json op;
  if (!meshTarget(si, shown, op) || !shown->boundingBox) return {};
  const cutout::Attachment* att = dragRig_->attachment(ss(refId(selection_)), shown->name, dragPose_.skin);
  if (!att || !att->boundingBox) return {};
  std::vector<double> vertices = att->vertices;
  for (int v : selectedVertices()) {
    if (v < 0 || size_t(v) >= att->vertexCount()) continue;
    const cutout::Affine m = cutout::meshVertexMatrix(*dragRig_, dragPose_, si, *att, size_t(v));
    const double det = m.a * m.d - m.b * m.c;
    if (std::fabs(det) < 1e-12) continue;
    vertices[2 * size_t(v)] += (m.d * rigDelta.x() - m.c * rigDelta.y()) / det;
    vertices[2 * size_t(v) + 1] += (-m.b * rigDelta.x() + m.a * rigDelta.y()) / det;
  }
  op.update({{"op", "setBoundingBox"}, {"vertices", vertices}});
  return {op};
}
bool CutoutPanel::boxAddVertex(const QPointF& rigPoint) {
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op) || !att->boundingBox) return false;
  const QVector<QPointF> pts = meshPoints(*rig_, currentPose(false), si);
  const int n = pts.size();
  if (n != int(att->vertexCount()) || n < 3) return false;
  // Onto the nearest edge (widget px), at the clicked place along it.
  int edge = -1;
  double best = 12 * uiScale(), t = 0.5;
  const QPointF w = canvas_->rigToWidget(rigPoint);
  for (int k = 0; k < n; k++) {
    const QPointF wa = canvas_->rigToWidget(pts[k]), ab = canvas_->rigToWidget(pts[(k + 1) % n]) - wa;
    const double len2 = QPointF::dotProduct(ab, ab), s = len2 > 1e-12 ? std::clamp(QPointF::dotProduct(w - wa, ab) / len2, 0.0, 1.0) : 0.0;
    if (const double d = QLineF(w, wa + ab * s).length(); d < best) { best = d; edge = k; t = s; }
  }
  if (edge < 0) { setStatus("판정 상자의 변 가까이를 클릭하세요.", true); return false; }
  const int next = (edge + 1) % n;
  std::vector<double> vertices = att->vertices;
  // Interpolated in the box's own (bind) space: the new point lands on the drawn edge at the setup pose.
  const double ax = att->vertices[2 * size_t(edge)], ay = att->vertices[2 * size_t(edge) + 1];
  const double x = ax + (att->vertices[2 * size_t(next)] - ax) * t, y = ay + (att->vertices[2 * size_t(next) + 1] - ay) * t;
  vertices.insert(vertices.begin() + 2 * (edge + 1), {x, y});
  op.update({{"op", "setBoundingBox"}, {"vertices", vertices}});
  if (!runOps({op}, false)) return false;
  meshSelection_ = {edge + 1};
  refreshMesh();
  setStatus(QString("판정 상자에 점 %1을 넣었습니다(점 수가 바뀌면 웨이트는 지워짐). 되돌리기: Ctrl+Z").arg(edge + 1));
  return true;
}
bool CutoutPanel::boxDeleteVertices() {
  int si;
  const cutout::Attachment* att;
  Json op;
  if (!meshTarget(si, att, op) || !att->boundingBox) return false;
  if (meshSelection_.isEmpty()) { setStatus("지울 점을 먼저 고르세요.", true); return false; }
  const QVector<int> list = selectedVertices();
  if (int(att->vertexCount()) - list.size() < 3) { setStatus("판정 상자에는 점이 3개 이상 남아야 합니다.", true); return false; }
  std::vector<double> vertices;
  for (size_t v = 0; v < att->vertexCount(); v++)
    if (!list.contains(int(v))) { vertices.push_back(att->vertices[2 * v]); vertices.push_back(att->vertices[2 * v + 1]); }
  op.update({{"op", "setBoundingBox"}, {"vertices", vertices}});
  if (!runOps({op}, false)) return false;
  meshSelection_.clear();
  refreshMesh();
  setStatus(QString("판정 상자의 점 %1개를 지웠습니다. 되돌리기: Ctrl+Z").arg(list.size()));
  return true;
}

// ---------------------------------------------------------------- preview
bool CutoutPanel::previewClick(const QPointF& rigPoint, double tolerance) {
  if (bottomTabs_ && bottomTabs_->currentIndex() == 1) ensurePreview();
  if (!showingPreview() || !preview_) return false;
  const cutout::Pose pose = preview_->pose();
  const auto boxes = cutout::boxPolygons(preview_->rig(), pose);
  int hit = cutout::hitBox(boxes, rigPoint.x(), rigPoint.y());
  // On the drawn outline (plugin 1.21.0): the canvas names the box while hovering its line, so a
  // click there must not fall through to the select tool (it did: the click selected the box part).
  if (hit < 0 && tolerance > 0) {
    double nearest = tolerance * (1 + 1e-9);  // topmost first: a lower box wins only when nearer
    for (size_t k = boxes.size(); k-- > 0;) {
      const auto& p = boxes[k].polygon;
      const size_t n = p.size() / 2;
      for (size_t i = 0; n >= 3 && i < n; i++) {
        const double ax = p[2 * i], ay = p[2 * i + 1], bx = p[2 * ((i + 1) % n)], by = p[2 * ((i + 1) % n) + 1];
        const double dx = bx - ax, dy = by - ay, len = dx * dx + dy * dy;
        const double t = len > 0 ? std::clamp(((rigPoint.x() - ax) * dx + (rigPoint.y() - ay) * dy) / len, 0.0, 1.0) : 0;
        const double d = std::hypot(rigPoint.x() - (ax + t * dx), rigPoint.y() - (ay + t * dy));
        if (d < nearest) { nearest = d; hit = int(k); }
      }
    }
  }
  if (hit < 0) return false;
  // The same signal a player's click sends (hitTest dispatch): partClicked {box, slot}, topmost box.
  const auto& b = boxes[size_t(hit)];
  const QString box = qs(b.attachment->name), slot = qs(preview_->rig().slots[size_t(b.slot)].id);
  const QStringList matched = previewSignal("partClicked", {{"box", box}, {"slot", slot}});
  previewResult_->setText(matched.isEmpty() ? QString("판정 상자 「%1」(%2) 클릭 → 맞는 partClicked 연결이 없습니다.").arg(box, slot)
                                            : QString("판정 상자 「%1」(%2) 클릭 → 실행된 연결: %3").arg(box, slot, matched.join(", ")));
  lastPreviewClick_ = {box, slot, matched};
  return true;
}
cutout::Json CutoutPanel::previewState() const { return preview_ ? preview_->saveState() : cutout::Json(); }
QStringList CutoutPanel::rigBoxNames() const {
  QStringList out;
  auto scan = [&](const std::vector<cutout::Attachment>& list) { for (const auto& a : list) if (a.boundingBox && !out.contains(qs(a.name))) out << qs(a.name); };
  if (rig_) { scan(rig_->attachments); for (const auto& s : rig_->skins) scan(s.attachments); }
  return out;
}
}  // namespace cutout_studio
