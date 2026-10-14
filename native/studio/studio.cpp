#include "studio.hpp"
#include "hierarchy-tree.hpp"
#include "hierarchy-lock.hpp"
#include "inspector-wheel.hpp"
#include "logic-editor.hpp"
#include <QTreeWidgetItemIterator>
#include "renderer-preview.hpp"
#include "design.hpp"
#include "localization.hpp"
#include <QSignalBlocker>
#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QPointer>
#include <QRegularExpression>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <memory>

namespace {
// A request may start inside focusOutEvent (Tab or a mouse click). Qt's current
// focus widget is already the destination, so bookmark it rather than sender().
class InspectorFocusBookmark final : public QObject {
public:
  explicit InspectorFocusBookmark(QWidget *inspector) {
    auto focus = QApplication::focusWidget();
    if (!focus || !inspector->isAncestorOf(focus)) return;
    target = focus;
    activeWindow = QApplication::activeWindow();
    line = qobject_cast<QLineEdit *>(focus);
    if (!line && qobject_cast<QAbstractSpinBox *>(focus))
      line = focus->findChild<QLineEdit *>();
    if (line) {
      cursor = line->cursorPosition();
      const int start = line->selectionStart();
      anchor = start < 0 ? cursor : cursor == start ? start + line->selectedText().size() : start;
    }
  }
  ~InspectorFocusBookmark() override { stopWatching(); }
  void watch() {
    if (!target) return;
    parkedFocus = QApplication::focusWidget();
    qApp->installEventFilter(this);
    focusConnection = connect(qApp, &QApplication::focusChanged, this,
      [this](QWidget *, QWidget *now) { if (now != parkedFocus) canceled = true; });
    stateConnection = connect(qApp, &QGuiApplication::applicationStateChanged, this,
      [this](Qt::ApplicationState state) { if (state != Qt::ApplicationActive) canceled = true; });
  }
  void stopWatching() {
    qApp->removeEventFilter(this);
    disconnect(focusConnection);
    disconnect(stateConnection);
  }
  void restore() {
    if (canceled || !target || !activeWindow || QApplication::activeWindow() != activeWindow ||
        QApplication::activeModalWidget() || !target->isEnabled() || !target->isVisible()) return;
    target->setFocus(Qt::OtherFocusReason);
    if (line && line->hasFocus()) {
      const int length = line->text().size();
      const int end = qBound(0, cursor, length), begin = qBound(0, anchor, length);
      if (begin == end) line->setCursorPosition(end);
      else line->setSelection(begin, end - begin);
    }
  }
protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    // A mouse press that caused focusOut is still delivered to its destination
    // and then its ancestors. Those are not a second choice of focus. A click
    // elsewhere, including an already focused fallback widget, cancels restore.
    const auto receiver = qobject_cast<QWidget *>(watched);
    const bool otherMouseTarget = event->type() == QEvent::MouseButtonPress &&
      (!receiver || (receiver != target && !receiver->isAncestorOf(target)));
    // Return can continue bubbling through parents after editingFinished starts
    // this request. Only input directed at the current focus (or Escape) is new
    // keyboard intent, not those remaining deliveries of the committing key.
    const bool directedKey = event->type() == QEvent::KeyPress &&
      (watched == QApplication::focusWidget() || static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape);
    if (otherMouseTarget || directedKey ||
        (watched == activeWindow && event->type() == QEvent::WindowDeactivate)) canceled = true;
    return false;
  }
private:
  QPointer<QWidget> target, activeWindow, parkedFocus;
  QPointer<QLineEdit> line;
  QMetaObject::Connection focusConnection, stateConnection;
  int cursor = 0, anchor = 0;
  bool canceled = false;
};
}

static QString label(const QJsonObject &e) {
  return e["label"].toString(e["text"].toString(e["id"].toString()));
}
StudioWindow::StudioWindow(QString root, QWidget *parent)
    : QMainWindow(parent), engineRoot(root), bridge(root, this) {
  setObjectName("raisingStudio");
  setWindowTitle("Raising Studio · Qt");
  resize(1440, 920);
  setMinimumSize(1000, 680);
  setDockOptions(AllowNestedDocks | AllowTabbedDocks | AnimatedDocks);
  auto file = menuBar()->addMenu(tr("파일"));
  file->addAction(
      tr("프로젝트 열기…"), this, [this] { chooseProject(false); },
      QKeySequence::Open);
  file->addAction(tr("새 프로젝트…"), this, [this] { chooseProject(true); }, QKeySequence::New)->setObjectName("newProject");
  file->addAction(tr("프로젝트 설정…"), this, &StudioWindow::editProjectSettings)->setObjectName("projectSettings");
  saveAction = file->addAction(tr("프로젝트 저장"), this, &StudioWindow::save,
                               QKeySequence::Save);
  saveAction->setObjectName("saveProject");
  file->addAction(tr("게임 내보내기…"), this, &StudioWindow::exportGameDialog)->setObjectName("exportGame");
  auto edit = menuBar()->addMenu(tr("편집"));
  undoAction = edit->addAction(
      tr("되돌리기"), this,
      [this] { request("edit", {{"edit", QJsonObject{{"type", "undo"}}}}); },
      QKeySequence::Undo);
  undoAction->setObjectName("undoEdit");
  redoAction = edit->addAction(
      tr("다시 실행"), this,
      [this] { request("edit", {{"edit", QJsonObject{{"type", "redo"}}}}); },
      QKeySequence::Redo);
  redoAction->setObjectName("redoEdit");
  auto duplicateSelection=edit->addAction(tr("선택 요소 복제"),QKeySequence(Qt::CTRL|Qt::Key_D),this,&StudioWindow::duplicateElement);
  duplicateSelection->setObjectName("duplicateElementAction");duplicateSelection->setToolTip(tr("선택 요소와 그룹의 자식을 복제합니다. Ctrl+D"));duplicateSelection->setEnabled(false);
  auto screensMenu = menuBar()->addMenu(tr("화면"));
  screensMenu->addAction(tr("화면 설정…"),this,&StudioWindow::editScreenSettings)->setObjectName("screenSettings");
  newScreenAction = screensMenu->addAction(tr("＋ 새 화면…"), this,
                                           [this] { createScreen(); });
  newScreenAction->setObjectName("newScreen");
  duplicateScreenAction = screensMenu->addAction(
      tr("화면 복제…"), this, [this] { createScreen(true); });
  duplicateScreenAction->setObjectName("duplicateScreen");
  renameScreenAction = screensMenu->addAction(tr("이름 변경…"), this,
                                              &StudioWindow::renameScreen);
  renameScreenAction->setObjectName("renameScreen");
  deleteScreenAction = screensMenu->addAction(tr("화면 삭제…"), this,
                                              &StudioWindow::deleteScreen);
  deleteScreenAction->setObjectName("deleteScreen");
  screensMenu->addSeparator();
  screensMenu->addAction(
      tr("현재 탭 닫기"), this,
      [this] { closeScreen(documents->currentIndex()); }, QKeySequence::Close);
  auto viewMenu = menuBar()->addMenu(tr("보기"));
  auto designMenu = menuBar()->addMenu(tr("디자인"));
  designMenu->addAction(tr("공통 스타일 · 프로젝트 폰트…"),this,&StudioWindow::editDesignSystem)->setObjectName("editDesignSystem");
  designMenu->addAction(tr("폰트 파일 가져오기…"),this,&StudioWindow::importFont)->setObjectName("importFont");
  designMenu->addAction(tr("다국어 작업실…"),this,&StudioWindow::editLocalization)->setObjectName("editLocalization");
  designMenu->addAction(tr("새 게임 · 사용자 입력 필드…"),this,&StudioWindow::editUserFields)->setObjectName("editUserFields");
  designMenu->addAction(tr("텍스트 연결 일괄 적용…"),this,&StudioWindow::applyTextBindingBatch)->setObjectName("applyTextBindingBatch");
  designMenu->addAction(tr("재사용 UI 부품 작업실…"),this,&StudioWindow::editComponents)->setObjectName("editComponents");
  designMenu->addAction(tr("데이터 · 반복 UI 연결…"),this,&StudioWindow::editDataUi)->setObjectName("editDataUi");
  designMenu->addAction(tr("스프라이트 시트 가져오기…"),this,&StudioWindow::importSprites)->setObjectName("importSprites");
  designMenu->addAction(tr("일정 · 일별 연출 타임라인…"),this,&StudioWindow::editTimelines)->setObjectName("editTimelines");
  designMenu->addAction(tr("전투 HUD 제작…"),this,&StudioWindow::editBattleHud)->setObjectName("editBattleHud");
  designMenu->addAction(tr("탐험 · 조우 작성실…"),this,&StudioWindow::editExpeditions)->setObjectName("editExpeditions");
  designMenu->addAction(tr("대본 · 선택지 작업실…"),this,&StudioWindow::editScripts)->setObjectName("scriptAuthoringAction");
  designMenu->addAction(tr("정책 · 행동 · 공유 한도 작업실…"),this,&StudioWindow::editRules)->setObjectName("editRules");
  designMenu->addAction(tr("콘텐츠 조건 · 효과 · 수식…"),this,&StudioWindow::editLogicDocument)->setObjectName("editLogicDocument");
  designMenu->addAction(tr("실행 시점 · 행동 흐름…"),this,&StudioWindow::editWorkflows)->setObjectName("editWorkflows");
  designMenu->addAction(tr("규칙 디버거 · 영향 분석"),this,&StudioWindow::showRuleDebugger)->setObjectName("showRuleDebugger");
  designMenu->addAction(tr("거래 · 기존 시설 데이터…"),this,&StudioWindow::editFacilities)->setObjectName("editFacilities");
  auto toolbar = addToolBar(tr("제작 도구"));
  toolbar->setObjectName("mainToolbar");
  toolbar->setMovable(false);
  toolbar->setIconSize({18, 18});
  previewLocaleField=new QComboBox;previewLocaleField->setObjectName("previewLocale");previewLocaleField->setToolTip(tr("캔버스와 다음 실행의 언어"));toolbar->addWidget(previewLocaleField);
  connect(previewLocaleField,qOverload<int>(&QComboBox::activated),this,[this]{refresh();});
  toolbar->addAction(newScreenAction);
  addAction = toolbar->addAction(tr("＋ 버튼"), this, &StudioWindow::addButton);
  addAction->setObjectName("addButton");
  auto palette = new QToolButton;
  palette->setText(tr("＋ 요소"));
  palette->setObjectName("elementPalette");
  palette->setPopupMode(QToolButton::InstantPopup);
  auto paletteMenu = new QMenu(palette);
  for (const auto &pair : QList<QPair<QString, QString>>{
           {"text", tr("텍스트")},
           {"input", tr("사용자 입력란 · 새 게임")},
           {"panel", tr("패널")},
           {"image", tr("이미지 · 에셋 선택 후 추가")},
           {"plugin", tr("애니메이션 · 플러그인 요소")},
           {"stats", tr("능력·자원 목록")},
           {"schedule", tr("일정 선택")},
           {"dialogue", tr("대사")},
           {"choices", tr("선택지")},
           {"result", tr("활동 결과")},
           {"shop", tr("상점 목록")},
           {"actions", tr("공통 행동 목록")},
           {"policies", tr("독립 정책 그룹")},
           {"sell", tr("판매 목록")},
           {"services", tr("병원 · 서비스 목록")},
           {"diets", tr("지속 식단 목록")},
           {"inventory", tr("소지품")},
           {"interactions", tr("대화·상호작용")},
           {"locations", tr("이동할 장소")},
           {"equipment", tr("장비")},
           {"expedition", tr("탐험")},
           {"battle", tr("전투")}})
    paletteMenu->addAction(pair.second, this,
                           [this, pair] { addElement(pair.first); });
  palette->setMenu(paletteMenu);
  toolbar->addWidget(palette);
  toolbar->addAction(undoAction);
  toolbar->addAction(redoAction);
  toolbar->addSeparator();
  toolbar->addAction(saveAction);
  toolbar->addSeparator();
  playAction =
      toolbar->addAction(tr("▶ 처음부터 플레이"), this, &StudioWindow::play);
  playAction->setObjectName("playProject");
  playAction->setShortcut(Qt::Key_F5);
  stopAction =
      toolbar->addAction(tr("■ 정지"), this, [this] { player.terminate(); });
  stopAction->setEnabled(false);
  toolbar->addSeparator();
  auto validateAction =
      toolbar->addAction(tr("검증"), this, &StudioWindow::runValidation);
  validateAction->setObjectName("validateProject");
  toolbar->addAction(tr("자동 플레이 · 경로 지도"),this,&StudioWindow::showAnalysis)->setObjectName("showAnalysis");
  auto center = new QWidget;
  auto centerLayout = new QVBoxLayout(center);
  centerLayout->setContentsMargins(0, 0, 0, 0);
  centerLayout->setSpacing(0);
  documents = new QTabBar;
  documents->setObjectName("documentTabs");
  documents->setExpanding(false);
  documents->setUsesScrollButtons(true);
  documents->setTabsClosable(true);
  centerLayout->addWidget(documents);
  auto canvasTools = new QToolBar;
  canvasTools->setObjectName("canvasToolbar");
  viewportModeField=new QComboBox;viewportModeField->setObjectName("viewportMode");
  viewportModeField->addItem(tr("화면: 비율 유지"),"fit");viewportModeField->addItem(tr("화면: 반응형 앵커"),"responsive");canvasTools->addWidget(viewportModeField);
  connect(viewportModeField,qOverload<int>(&QComboBox::activated),this,[this]{if(filling||bridge.busy())return;auto p=projectData();auto v=p["visual"].toObject();v["viewportMode"]=viewportModeField->currentData().toString();p["visual"]=v;replaceProject(p);});
  previewSizeField=new QComboBox;previewSizeField->setObjectName("previewAspect");
  previewSizeField->addItem(tr("설계 크기 · 배치 가능"),0.0);previewSizeField->addItem(tr("16:9 미리보기"),16.0/9);previewSizeField->addItem(tr("4:3 미리보기"),4.0/3);previewSizeField->addItem(tr("9:16 미리보기"),9.0/16);canvasTools->addWidget(previewSizeField);
  previewSizeField->setToolTip(tr("미리보기에서는 배치를 잠급니다. 위치·크기는 기준 설계 좌표로 편집하세요."));
  connect(previewSizeField,qOverload<int>(&QComboBox::activated),this,[this]{refresh(true);});
  auto conditions=canvasTools->addAction(tr("조건 보기…"),this,[this]{editConditionPreview();});
  conditions->setObjectName("conditionPreviewAction");
  conditions->setToolTip(tr("게임 데이터를 바꾸지 않고 임시 값으로 표시 조건을 검사합니다."));
  auto allElements=canvasTools->addAction(tr("전체 요소"),this,[this]{conditionPreview={};refresh();});
  allElements->setObjectName("conditionPreviewClear");
  canvasTools->addAction(tr("화면 맞춤"), this,
                         [this] { canvas->fitCanvas(); });
  auto snap = canvasTools->addAction(tr("8px 스냅"));
  snap->setCheckable(true);
  connect(snap, &QAction::toggled, this, [this](bool b) { canvas->snap = b; });
  canvasTools->addAction(tr("오른쪽 아래 정렬"), this, [this] {
    auto visual = projectData()["visual"].toObject();
    updateElement([visual](QJsonObject &e) {
      e["x"] = qMax(0, visual["width"].toInt() - e["width"].toInt() - 32);
      e["y"] = qMax(0, visual["height"].toInt() - e["height"].toInt() - 32);
    });
  });
  canvasTools->addSeparator();
  auto forward =
      canvasTools->addAction(tr("앞으로"), this, [this] { moveLayer(1); });
  forward->setObjectName("layerForward");
  auto backward =
      canvasTools->addAction(tr("뒤로"), this, [this] { moveLayer(-1); });
  backward->setObjectName("layerBackward");
  auto hint = new QLabel(tr("  Ctrl+휠 확대 · Space 이동 · 모서리 크기 조절"));
  canvasTools->addWidget(hint);
  centerLayout->addWidget(canvasTools);
  canvas = new ScreenCanvas;
  centerLayout->addWidget(canvas, 1);
  emptyDocument = new QLabel(tr("열린 화면이 없습니다\n\n프로젝트에서 화면을 "
                                "더블클릭하거나 ‘＋ 새 화면’을 선택하세요."));
  emptyDocument->setAlignment(Qt::AlignCenter);
  emptyDocument->setStyleSheet("color:#a9bbc9;font-size:16px;");
  centerLayout->addWidget(emptyDocument, 1);
  setCentralWidget(center);
  auto dock = [this, viewMenu](QString title, QString name, QWidget *child,
                               Qt::DockWidgetArea area) {
    auto d = new QDockWidget(title, this);
    d->setObjectName(name);
    d->setWidget(child);
    addDockWidget(area, d);
    viewMenu->addAction(d->toggleViewAction());
    return d;
  };
  auto treePanel = new QWidget;
  auto treeLayout = new QVBoxLayout(treePanel);
  treeLayout->setContentsMargins(4, 4, 4, 4);
  hierarchySearch = new QLineEdit;
  hierarchySearch->setObjectName("hierarchySearch");
  hierarchySearch->setPlaceholderText(tr("전체 화면에서 찾기 · 이름 / ID"));
  hierarchySearch->setToolTip(tr("화면과 요소의 이름·ID·종류·텍스트를 검색합니다. 결과를 선택하면 해당 화면을 엽니다. 검색을 지우면 현재 화면 계층으로 돌아갑니다."));
  hierarchySearch->setClearButtonEnabled(true);
  treeLayout->addWidget(hierarchySearch);
  auto hierarchyTree=new HierarchyTree;hierarchy=hierarchyTree;hierarchyTree->reparent=[this](QString id,QString parent){reparentElement(id,parent);};
  hierarchy->setDragEnabled(true);hierarchy->setAcceptDrops(true);hierarchy->setDropIndicatorShown(true);hierarchy->setDragDropMode(QAbstractItemView::InternalMove);
  hierarchy->setObjectName("hierarchy");
  hierarchy->setHeaderHidden(true);
  hierarchy->setColumnCount(1);
  treeLayout->addWidget(hierarchy);
  auto groupButton=new QPushButton(tr("＋ UI 그룹"));groupButton->setObjectName("addGroup");treeLayout->addWidget(groupButton);connect(groupButton,&QPushButton::clicked,this,&StudioWindow::addGroup);
  auto treeDock =
      dock(tr("계층"), "hierarchyDock", treePanel, Qt::LeftDockWidgetArea);
  treeDock->setMinimumWidth(190);
  auto scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setMinimumWidth(310);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  inspector = new QWidget;
  inspector->setObjectName("inspector");
  auto formLayout = new QVBoxLayout(inspector);
  formLayout->setContentsMargins(12, 10, 12, 10);
  formLayout->setSpacing(12);
  selectedTitle = new QLabel(tr("요소를 선택하세요"));
  selectedTitle->setStyleSheet("font-size:16px;font-weight:600");
  formLayout->addWidget(selectedTitle);
  auto group = [formLayout](QString name) {
    auto container = new QWidget;
    auto layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    auto header = new QToolButton;
    header->setText(name);
    header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    header->setCheckable(true);
    header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    header->setStyleSheet(
        "text-align:left;padding:7px;background:#303741;font-weight:600");
    auto body = new QWidget;
    auto f = new QFormLayout(body);
    f->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    f->setContentsMargins(4, 3, 4, 6);
    f->setSpacing(7);
    bool expanded = name != QString::fromUtf8("외형");
    header->setChecked(expanded);
    header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    body->setVisible(expanded);
    QObject::connect(
        header, &QToolButton::toggled, body, [header, body](bool open) {
          body->setVisible(open);
          header->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        });
    layout->addWidget(header);
    layout->addWidget(body);
    formLayout->addWidget(container);
    return f;
  };
  auto basic = group(tr("기본"));
  nameField = new QLineEdit;
  nameField->setObjectName("elementName");
  textField = new QLineEdit;
  textField->setObjectName("buttonText");
  basic->addRow(tr("이름"), nameField);
  basic->addRow(tr("문구"), textField);
  parentField=new QComboBox;parentField->setObjectName("elementParent");basic->addRow(tr("부모 그룹"),parentField);
  connect(parentField,qOverload<int>(&QComboBox::activated),this,[this]{if(!filling)reparentElement(selection,parentField->currentData().toString());});
  clipChildrenField=new QCheckBox(tr("그룹 영역 밖 자식 자르기"));clipChildrenField->setObjectName("clipChildren");basic->addRow(clipChildrenField);connect(clipChildrenField,&QCheckBox::toggled,this,[this](bool enabled){if(!filling&&currentElement().value("kind")=="group")updateElement([enabled](QJsonObject&e){e["clipChildren"]=enabled;});});
  auto layout = group(tr("배치 · 논리 좌표"));
  auto spin = [](QString caption, QString name, int min, int max) {
    auto s = new QSpinBox;
    s->setObjectName(name);
    s->setAccessibleName(caption);
    s->setRange(min, max);
    s->setKeyboardTracking(false);
    return s;
  };
  xField = spin("X", "elementX", 0, 3840);
  yField = spin("Y", "elementY", 0, 2160);
  widthField = spin(tr("너비"), "elementWidth", 16, 3840);
  heightField = spin(tr("높이"), "elementHeight", 16, 2160);
  auto pairRow = [layout](QString caption, QWidget *a, QWidget *b) {
    auto row = new QWidget;
    auto horizontal = new QHBoxLayout(row);
    horizontal->setContentsMargins(0, 0, 0, 0);
    horizontal->addWidget(a);
    horizontal->addWidget(b);
    layout->addRow(caption, row);
  };
  pairRow(tr("위치 X / Y"), xField, yField);
  pairRow(tr("크기 W / H"), widthField, heightField);
  anchorXField=new QComboBox;anchorXField->setObjectName("anchorX");
  anchorYField=new QComboBox;anchorYField->setObjectName("anchorY");
  for(auto p:QList<QPair<QString,QString>>{{"왼쪽","left"},{"가운데","center"},{"오른쪽","right"},{"좌우 늘이기","stretch"}})anchorXField->addItem(p.first,p.second);
  for(auto p:QList<QPair<QString,QString>>{{"위","top"},{"가운데","center"},{"아래","bottom"},{"상하 늘이기","stretch"}})anchorYField->addItem(p.first,p.second);
  pairRow(tr("앵커 가로 / 세로"),anchorXField,anchorYField);
  for(auto p:{qMakePair(anchorXField,QString("anchorX")),qMakePair(anchorYField,QString("anchorY"))})connect(p.first,qOverload<int>(&QComboBox::activated),this,[this,p]{if(!filling)updateElement([p](QJsonObject&e){e[p.second]=p.first->currentData().toString();});});
  auto instanceSettings=new QPushButton(tr("부품 문구·행동 덮어쓰기…"));instanceSettings->setObjectName("componentOverrides");
  basic->addRow(instanceSettings);connect(instanceSettings,&QPushButton::clicked,this,&StudioWindow::editComponentInstance);
  lockedField = new QCheckBox(tr("배치 잠금"));
  lockedField->setObjectName("elementLocked");
  hiddenField = new QCheckBox(tr("게임에서 숨김"));
  hiddenField->setObjectName("elementHidden");
  layout->addRow(lockedField);
  layout->addRow(hiddenField);
  visibleConditionField = new QPushButton(tr("표시 조건 편집…"));
  visibleConditionField->setObjectName("elementVisibleCondition");
  visibleConditionField->setToolTip(tr("능력·자원·플래그 조건을 만족할 때 표시합니다. 부모 조건·게임에서 숨김·실행 단계 설정도 함께 적용됩니다."));
  clearVisibleConditionField = new QPushButton(tr("표시 조건 해제"));
  clearVisibleConditionField->setObjectName("elementVisibleConditionClear");
  clearVisibleConditionField->setToolTip(tr("이 요소의 수치·플래그 조건만 제거합니다. 부모 조건·숨김·실행 단계 설정은 유지합니다."));
  layout->addRow(visibleConditionField);
  layout->addRow(clearVisibleConditionField);
  connect(visibleConditionField, &QPushButton::clicked, this, &StudioWindow::editVisibilityCondition);
  connect(clearVisibleConditionField, &QPushButton::clicked, this, [this] {
    updateElement([](QJsonObject &element) { element.remove("visibleCondition"); });
  });
  auto appearance = group(tr("외형"));
  styleField = new QComboBox;
  styleField->setObjectName("elementStyle");
  appearance->addRow(tr("공통 스타일"), styleField);
  auto inherit = new QPushButton(tr("개별 모양 지우고 스타일 상속"));
  inherit->setObjectName("inheritStyle");
  appearance->addRow(inherit);
  connect(styleField, qOverload<int>(&QComboBox::activated), this, [this] {
    if (!filling) updateElement([this](QJsonObject &e) {
      const auto id=styleField->currentData().toString();
      if(id.isEmpty())e.remove("styleId");else e["styleId"]=id;
    });
  });
  connect(inherit, &QPushButton::clicked, this, [this] {
    updateElement([](QJsonObject &e) { for(auto key:styleFields())e.remove(key); });
  });
  fillField = new QLineEdit;
  fillField->setObjectName("elementFill");
  colorField = new QLineEdit;
  colorField->setObjectName("elementColor");
  fontField = new QSpinBox;
  fontField->setRange(9, 120);
  fontField->setSpecialValueText(tr("상속"));
  fontField->setKeyboardTracking(false);
  fontField->setObjectName("elementFont");
  appearance->addRow(tr("배경 색"), fillField);
  appearance->addRow(tr("문자 색"), colorField);
  appearance->addRow(tr("글자 크기"), fontField);
  auto dataForm = group(tr("이미지 · 데이터 연결"));
  imageField = new QComboBox;
  imageField->setObjectName("imageAsset");
  bindingField = new QComboBox;
  bindingField->setObjectName("textBinding");
  for (const auto &p :
       QList<QPair<QString, QString>>{{"none", tr("직접 입력")},
                                      {"attribute-value", tr("능력·자원 수치")},
                                      {"user-field", tr("사용자 입력값")},
                                      {"calendar", tr("날짜")},
                                      {"calendar-year", tr("달력 · 연도")},
                                      {"calendar-month", tr("달력 · 월")},
                                      {"calendar-day", tr("달력 · 일")},
                                      {"calendar-weekday", tr("달력 · 요일")},
                                      {"schedule-prompt", tr("일정 선택 안내")},
                                      {"title", tr("게임 제목")},
                                      {"location", tr("현재 장소")},
                                      {"ending", tr("엔딩 제목")},
                                      {"speaker", tr("대화 인물")},
                                      {"story", tr("대사")}})
    bindingField->addItem(p.second, p.first);
  builtinBindingCount=bindingField->count();
  userFieldField=new QComboBox;userFieldField->setObjectName("bindingUserField");
  attributeField = new QComboBox;
  attributeField->setObjectName("bindingAttribute");
  valueFormatField = new QLineEdit;
  valueFormatField->setObjectName("valueFormat");
  visibilityField = new QComboBox;
  visibilityField->setObjectName("visibilityPhase");
  for (const auto &p :
       QList<QPair<QString, QString>>{{"always", tr("항상")},
                                      {"planning", tr("일정 계획")},
                                      {"result", tr("활동 결과")},
                                      {"dialogue", tr("대화")},
                                      {"choice", tr("선택지")},
                                      {"finished", tr("엔딩 완료")}})
    visibilityField->addItem(p.second, p.first);
  dataForm->addRow(tr("이미지"), imageField);
  dataForm->addRow(tr("문자 내용"), bindingField);
  dataForm->addRow(tr("사용자 입력 필드"), userFieldField);
  dataForm->addRow(tr("능력·자원"), attributeField);
  dataForm->addRow(tr("표시 형식"), valueFormatField);
  dataForm->addRow(tr("표시 시점"), visibilityField);
  auto bindingHint = new QLabel(
      tr("표시 형식: {label} · {value}\n에셋 더블클릭으로 이미지 배치"));
  bindingHint->setWordWrap(true);
  dataForm->addRow(bindingHint);
  auto batchBinding=new QPushButton(tr("다른 화면에 이 텍스트 연결 적용…"));batchBinding->setObjectName("textBindingBatch");dataForm->addRow(batchBinding);connect(batchBinding,&QPushButton::clicked,this,&StudioWindow::applyTextBindingBatch);
  auto widgetSettings = new QPushButton(tr("위젯 구성 · 분류 / 표시 항목"));
  widgetSettings->setObjectName("widgetSettings");
  dataForm->addRow(widgetSettings);
  connect(widgetSettings, &QPushButton::clicked, this, &StudioWindow::editWidgetSettings);
  connect(imageField, qOverload<int>(&QComboBox::activated), this, [this] {
    if (filling)
      return;
    const auto id = imageField->currentData().toString();
    updateElement([id](QJsonObject &e) {
      if (id.startsWith("@")) {
        e["imageSource"] = id.mid(1);
        e.remove("imageAssetId");
      } else {
        e["imageSource"] = "asset";
        e["imageAssetId"] = id;
      }
    });
  });
  connect(bindingField, qOverload<int>(&QComboBox::activated), this, [this] {
    if (filling)
      return;
    const auto binding = bindingField->currentData().toString();
    const auto attribute = attributeField->currentData().toString();
    const auto userField = userFieldField->currentData().toString();
    if(binding=="user-field"&&userField.isEmpty()){errorLabel->setText(tr("디자인 > 새 게임 · 사용자 입력 필드에서 필드를 먼저 만드세요."));updateInspector();return;}
    updateElement([binding, attribute, userField](QJsonObject &e) {
      e["binding"] = binding;
      if(binding=="user-field"){e["userFieldId"]=userField;e["valueFormat"]="{value}";}else e.remove("userFieldId");
      if (binding == "attribute-value") {
        e["attributeId"] = attribute;
        if (!e.contains("valueFormat"))
          e["valueFormat"] = "{label}: {value}";
      }
    });
  });
  connect(userFieldField,qOverload<int>(&QComboBox::activated),this,[this]{if(!filling&&!userFieldField->currentData().toString().isEmpty())updateElement([this](QJsonObject &e){e["userFieldId"]=userFieldField->currentData().toString();});});
  connect(attributeField, qOverload<int>(&QComboBox::activated), this, [this] {
    if (!filling)
      updateElement([this](QJsonObject &e) {
        e["attributeId"] = attributeField->currentData().toString();
      });
  });
  connect(valueFormatField, &QLineEdit::editingFinished, this, [this] {
    if (!filling)
      updateElement([this](QJsonObject &e) {
        e["valueFormat"] = valueFormatField->text();
      });
  });
  connect(visibilityField, qOverload<int>(&QComboBox::activated), this, [this] {
    if (!filling)
      updateElement([this](QJsonObject &e) {
        e["visibleWhen"] = visibilityField->currentData().toString();
      });
  });
  auto action = group(tr("행동 · 클릭했을 때"));
  actionField = new QComboBox;
  actionField->setObjectName("buttonAction");
  actionField->addItem(tr("화면 이동"), "navigate");
  actionField->addItem(tr("상점 열기 · 장소 이동"), "shop");
  actionField->addItem(tr("장소 이동"), "visit");
  actionField->addItem(tr("게임 저장"), "save");
  actionField->addItem(tr("게임 불러오기"), "load");
  actionField->addItem(tr("새 게임 시작"), "start");
  actionField->addItem(tr("진행 · 대화 / 결과 확인"), "advance");
  actionField->addItem(tr("공통 행동 실행"), "executeAction");
  actionField->addItem(tr("전투 행동 선택"), "selectBattleAction");
  actionField->addItem(tr("전투 대상 선택"), "selectBattleTarget");
  actionField->addItem(tr("선택한 전투 행동 실행"), "confirmBattleAction");
  actionField->addItem(tr("전투 선택 취소"), "cancelBattleAction");
  actionField->setProperty("builtinActionCount", actionField->count());
  targetField = new QComboBox;
  targetField->setObjectName("actionTarget");
  action->addRow(tr("행동"), actionField);
  action->addRow(tr("대상"), targetField);
  actionHelp = new QLabel;
  actionHelp->setWordWrap(true);
  actionHelp->setStyleSheet("color:#a9bbc9");
  action->addRow(actionHelp);
  auto openTarget = new QPushButton(tr("대상 화면 열기"));
  openTarget->setObjectName("openActionTarget");
  action->addRow(openTarget);
  connect(openTarget, &QPushButton::clicked, this, [this] {
    auto e = currentElement();
    QString id = e["screenId"].toString();
    if (e["action"] == "visit")
      for (auto loc :
           projectData()["gameplay"].toObject()["locations"].toArray())
        if (loc.toObject()["id"] == e["locationId"])
          id = loc.toObject()["screenId"].toString();
    openScreen(id);
  });
  formLayout->addStretch();
  scroll->setWidget(inspector);
  new InspectorWheelGuard(inspector, scroll);
  auto propertiesDock =
      dock(tr("속성"), "inspectorDock", scroll, Qt::RightDockWidgetArea);
  bottom = new QTabWidget;
  bottom->setObjectName("bottomTabs");
  projectList = new QListWidget;
  projectList->setObjectName("projectList");
  assetList = new QListWidget;
  assetList->setViewMode(QListView::IconMode);
  assetList->setIconSize({88, 58});
  assetList->setGridSize({136, 100});
  assetList->setResizeMode(QListView::Adjust);
  assetList->setWordWrap(true);
  assetList->setObjectName("assetList");
  connect(assetList, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem *item) {
            const auto id=item->data(Qt::UserRole).toString();
            for(auto a:assets)if(a.toObject()["id"]==id){
              if(a.toObject()["kind"]=="image")addElement("image",id);
              else if(a.toObject()["kind"]=="font")editDesignSystem();
              break;
            }
          });
  problems = new QListWidget;
  problems->setObjectName("problems");
  validationPanel=new QWidget;
  auto validationLayout=new QVBoxLayout(validationPanel);
  validationSummary=new QLabel(tr("검증을 누르면 결과와 오류 위치를 표시합니다."));
  validationSummary->setObjectName("validationSummary");
  validationSummary->setWordWrap(true);
  validationLayout->addWidget(validationSummary);
  validationLayout->addWidget(problems,1);
  log = new QPlainTextEdit;
  log->setReadOnly(true);
  log->setObjectName("executionLog");
  log->setMaximumBlockCount(1000);
  auto projectPanel = new QWidget;
  auto projectLayout = new QVBoxLayout(projectPanel);
  projectLayout->setContentsMargins(4, 4, 4, 4);
  auto screenTools = new QToolBar;
  screenTools->setObjectName("screenToolbar");
  screenTools->addActions({newScreenAction, duplicateScreenAction,
                           renameScreenAction, deleteScreenAction});
  projectLayout->addWidget(screenTools);
  projectLayout->addWidget(new QLabel(
      tr("화면 · 더블클릭하여 열기 · 탭 닫기는 화면을 삭제하지 않습니다")));
  projectLayout->addWidget(projectList);
  bottom->addTab(projectPanel, tr("프로젝트"));
  projectList->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(projectList, &QListWidget::customContextMenuRequested, this,
          [this](QPoint point) {
            if (auto item = projectList->itemAt(point))
              projectList->setCurrentItem(item);
            QMenu menu(this);
            menu.addActions({newScreenAction, duplicateScreenAction,
                             renameScreenAction, deleteScreenAction});
            menu.exec(projectList->viewport()->mapToGlobal(point));
          });
  connect(projectList, &QListWidget::currentItemChanged, this, [this] {
    if (!filling)
      updateScreenActions();
  });
  auto assetsPanel = new QWidget;
  auto assetsLayout = new QVBoxLayout(assetsPanel);
  assetsLayout->setContentsMargins(4, 4, 4, 4);
  auto assetsTools = new QHBoxLayout;
  assetSearch = new QLineEdit;
  assetSearch->setPlaceholderText(tr("에셋 이름·경로 검색"));
  auto importButton = new QPushButton(tr("PNG 가져오기…"));
  importButton->setObjectName("importImage");
  assetsTools->addWidget(assetSearch, 1);
  assetsTools->addWidget(importButton);
  assetsLayout->addLayout(assetsTools);
  assetsLayout->addWidget(assetList);
  connect(importButton, &QPushButton::clicked, this,
          &StudioWindow::importImage);
  connect(assetSearch, &QLineEdit::textChanged, this, [this](const QString &q) {
    for (int i = 0; i < assetList->count(); ++i) {
      auto item = assetList->item(i);
      item->setHidden(
          !(item->text() + item->toolTip()).contains(q, Qt::CaseInsensitive));
    }
  });
  bottom->addTab(assetsPanel, tr("에셋"));
  bottom->addTab(validationPanel, tr("검증 결과"));
  bottom->addTab(log, tr("실행 로그"));
  auto contentPanel = new QWidget;
  auto contentLayout = new QVBoxLayout(contentPanel);
  contentLayout->setContentsMargins(4, 4, 4, 4);
  auto contentSearch = new QLineEdit;
  contentSearch->setObjectName("contentSearch");
  contentSearch->setPlaceholderText(
      tr("콘텐츠 검색 · 활동, 사건, 엔딩, 인물, 능력"));
  contentLayout->addWidget(contentSearch);
  contentList = new QListWidget;
  contentList->setObjectName("contentList");
  contentLayout->addWidget(contentList);
  bottom->addTab(contentPanel, tr("콘텐츠"));
  connect(contentList, &QListWidget::itemDoubleClicked, this,
          [this] { editContent(); });
  connect(contentSearch, &QLineEdit::textChanged, this,
          [this](const QString &q) {
            for (int i = 0; i < contentList->count(); ++i) {
              auto item = contentList->item(i);
              item->setHidden(!item->text().contains(q, Qt::CaseInsensitive));
            }
          });
  auto contentMenu = menuBar()->addMenu(tr("콘텐츠"));
  contentMenu->addAction(tr("숙련 단계 활동 만들기…"), this, &StudioWindow::createProgressionActivity)->setObjectName("progressionAuthoringAction");
  for (const auto &p :
       QList<QPair<QString, QString>>{{"activity", tr("새 활동")},
                                      {"event", tr("새 사건")},
                                      {"ending", tr("새 엔딩")},
                                      {"character", tr("새 인물")}})
    contentMenu->addAction(p.second, this, [this, p] {
      if (bridge.busy() || projectData().isEmpty())
        return;
      bool ok = false;
      const auto id = QInputDialog::getText(
          this, p.second, tr("ID · 영문 소문자, 숫자, 하이픈"),
          QLineEdit::Normal, {}, &ok);
      if (ok && !id.isEmpty())
        request("edit",
                {{"edit",
                  QJsonObject{{"type", "add"}, {"kind", p.first}, {"id", id}}}},
                [this, id](QJsonObject reply) {
                  if (!reply["error"].toString().isEmpty())
                    return;
                  bottom->setCurrentIndex(4);
                  for (int i = 0; i < contentList->count(); ++i)
                    if (contentList->item(i)
                            ->data(Qt::UserRole)
                            .toString()
                            .endsWith("/" + id + ".json")) {
                      contentList->setCurrentRow(i);
                      editContent();
                      break;
                    }
                });
    });
  auto bottomDock = dock(tr("프로젝트 / 진단"), "projectDock", bottom,
                         Qt::BottomDockWidgetArea);
  bottomDock->setMinimumHeight(130);
  runtimeState = new QLabel(tr("플레이하면 실제 실행 상태가 표시됩니다."));
  runtimeState->setObjectName("runtimeState");
  runtimeState->setWordWrap(true);
  runtimeState->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  runtimeState->setMargin(12);
  auto runtimeDock = dock(tr("실행 상태"), "runtimeDock", runtimeState,
                          Qt::RightDockWidgetArea);
  tabifyDockWidget(propertiesDock, runtimeDock);
  propertiesDock->raise();
  errorLabel = new QLabel;
  errorLabel->setWordWrap(true);
  errorLabel->setStyleSheet("color:#ffb2ae");
  statusBar()->addWidget(errorLabel, 1);
  status = new QLabel;
  statusBar()->addPermanentWidget(status);
  resizeDocks({treeDock, propertiesDock}, {210, 330}, Qt::Horizontal);
  resizeDocks({bottomDock}, {180}, Qt::Vertical);
  rendererPreview=new RendererPreviewDock(this);
  addDockWidget(Qt::BottomDockWidgetArea,rendererPreview);
  rendererPreview->hide();
  viewMenu->addAction(rendererPreview->toggleViewAction());
  auto inspectRender=toolbar->addAction(tr("실제 렌더러 검사"),this,[this]{rendererPreview->show();rendererPreview->raise();rendererPreview->begin();});
  inspectRender->setObjectName("inspectRenderer");
  connect(rendererPreview,&RendererPreviewDock::snapshotRequested,this,[this]{
    if(bridge.busy()){rendererPreview->fail(tr("현재 편집 처리가 끝난 뒤 다시 렌더링하세요."));return;}
    request("preview",{},[this](QJsonObject reply){if(!reply["error"].toString().isEmpty()||!reply.contains("play")){rendererPreview->fail(reply["error"].toString(tr("미리보기를 준비할 수 없습니다.")));return;}rendererPreview->launch(reply["play"].toObject());});
  });
  connect(rendererPreview,&RendererPreviewDock::elementPicked,this,[this](QString id){
    for(auto e:currentScreen()["elements"].toArray()){auto element=e.toObject();const auto candidate=element["id"].toString();if(candidate==id||(element["kind"]=="component"&&id.startsWith(candidate+"__"))){selectElement(candidate);break;}}
  });
  defaultLayout = saveState(1);
  QSettings settings;
  if (!qEnvironmentVariableIsSet("RAISING_QT_TEST")) {
    restoreGeometry(settings.value("windowGeometry").toByteArray());
    restoreState(settings.value("dockLayout").toByteArray(), 1);
  }
  viewMenu->addSeparator();
  viewMenu->addAction(tr("작업 공간 초기화"), this,
                      [this] { restoreState(defaultLayout, 1); });
  connect(documents, &QTabBar::currentChanged, this, [this](int i) {
    if (filling || i < 0)
      return;
    screenId = documents->tabData(i).toString();
    selection.clear();
    refresh(true);
  });
  connect(documents, &QTabBar::tabCloseRequested, this,
          &StudioWindow::closeScreen);
  connect(hierarchy, &QTreeWidget::itemSelectionChanged, this, [this] {
    if (!filling && !bridge.busy() && !hierarchy->selectedItems().isEmpty()) {
      const auto item=hierarchy->selectedItems().first();
      const auto id=item->data(0,Qt::UserRole).toString();
      const auto targetScreen=item->data(0,Qt::UserRole+1).toString();
      if(!targetScreen.isEmpty()&&targetScreen!=screenId)openScreen(targetScreen);
      selectElement(id);
    }
  });
  connect(hierarchySearch, &QLineEdit::textChanged, this, [this] { rebuildHierarchy(); });
  connect(canvas, &ScreenCanvas::selectionChanged, this,
          &StudioWindow::selectElement);
  connect(canvas, &ScreenCanvas::geometryEdited, this,
          [this](QString id, QRect rect) {
            selection = id;
            updateElement([rect](QJsonObject &e) {
              e["x"] = rect.x();
              e["y"] = rect.y();
              e["width"] = rect.width();
              e["height"] = rect.height();
            });
          });
  connect(canvas, &ScreenCanvas::deleteSelected, this,
          &StudioWindow::deleteElement);
  connect(canvas, &ScreenCanvas::duplicateSelected, this,
          &StudioWindow::duplicateElement);
  connect(projectList, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem *item) {
            openScreen(item->data(Qt::UserRole).toString());
          });
  connect(
      problems, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        auto d = item->data(Qt::UserRole).toJsonObject();
        auto pointer = d["pointer"].toString();
        auto m =
            QRegularExpression("/visual/screens/(\\d+)(?:/elements/(\\d+))?")
                .match(pointer);
        if (m.hasMatch()) {
          int s = m.captured(1).toInt();
          auto screens =
              projectData()["visual"].toObject()["screens"].toArray();
          if (s < screens.size()) {
            screenId = screens[s].toObject()["id"].toString();
            if (!openScreens.contains(screenId))
              openScreens.append(screenId);
            auto elems = screens[s].toObject()["elements"].toArray();
            selection = m.captured(2).isEmpty()
                            ? QString()
                            : m.captured(2).toInt()>=elems.size()?QString():elems.at(m.captured(2).toInt())
                                  .toObject()["id"]
                                  .toString();
            refresh(true);
          }
        }
      });
  connect(nameField, &QLineEdit::editingFinished, this, [this] {
    if (!filling)
      updateElement([this](QJsonObject &e) { e["label"] = nameField->text(); });
  });
  connect(textField, &QLineEdit::editingFinished, this, [this] {
    if (!filling)
      updateElement([this](QJsonObject &e) { e["text"] = textField->text(); });
  });
  for (auto pair :
       {qMakePair(xField, QString("x")), qMakePair(yField, QString("y")),
        qMakePair(widthField, QString("width")),
        qMakePair(heightField, QString("height")),
        qMakePair(fontField, QString("fontSize"))})
    connect(pair.first, &QSpinBox::editingFinished, this, [this, pair] {
      if (!filling)
        updateElement(
            [pair](QJsonObject &e) { if(pair.second=="fontSize"&&pair.first->value()==9)e.remove(pair.second);else e[pair.second] = pair.first->value(); });
    });
  for (auto pair : {qMakePair(fillField, QString("fill")),
                    qMakePair(colorField, QString("color"))})
    connect(pair.first, &QLineEdit::editingFinished, this, [this, pair] {
      if (filling)
        return;
      auto value = pair.first->text();
      if(value.trimmed().isEmpty()){updateElement([pair](QJsonObject &e){e.remove(pair.second);});return;}
      auto re = pair.second == "fill"
                    ? QRegularExpression("^#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$")
                    : QRegularExpression("^#[0-9a-fA-F]{6}$");
      if (!re.match(value).hasMatch()) {
        errorLabel->setText(tr("색상은 #RRGGBB 형식으로 입력하세요."));
        return;
      }
      updateElement([pair, value](QJsonObject &e) { e[pair.second] = value; });
    });
  connect(lockedField, &QCheckBox::toggled, this, [this](bool b) {
    if (!filling)
      updateElement([b](QJsonObject &e) { e["locked"] = b; });
  });
  connect(hiddenField, &QCheckBox::toggled, this, [this](bool b) {
    if (!filling)
      updateElement([b](QJsonObject &e) { e["hidden"] = b; });
  });
  connect(actionField, qOverload<int>(&QComboBox::activated), this, [this] {
    if (filling)
      return;
    QString action = actionField->currentData().toString();
    updateTargets(action, {});
    QString target = targetField->currentData().toString();
    updateElement([action, target](QJsonObject &e) {
      for (auto key :
           {"screenId", "locationId", "shopId", "itemId", "interactionId", "battleAction", "battleTarget", "actionId"})
        e.remove(key);
      e["action"] = action == "shop" ? "visit" : action;
      if (action == "executeAction") {e["actionId"]=target;e["quantity"]=1;}
      if (action == "selectBattleAction") {e["battleAction"]=target.section(':',0,0);if(target.startsWith("useItem:"))e["itemId"]=target.section(':',1);}
      if (action == "selectBattleTarget")e["battleTarget"]=target;
      if (action == "navigate")
        e["screenId"] = target;
      if (action == "shop" || action == "visit")
        e["locationId"] = target;
    });
  });
  connect(targetField, qOverload<int>(&QComboBox::activated), this, [this] {
    if (filling)
      return;
    QString type = actionField->currentData().toString(),
            target = targetField->currentData().toString();
    updateElement([type, target](QJsonObject &e) {
      if(type=="selectBattleAction"){e["battleAction"]=target.section(':',0,0);e.remove("itemId");if(target.startsWith("useItem:"))e["itemId"]=target.section(':',1);}
      else e[type == "navigate" ? "screenId" : type=="selectBattleTarget"?"battleTarget":type=="executeAction"?"actionId":"locationId"] = target;
    });
  });
  connect(&bridge, &StudioBridge::failed, this,
          [this](QString message) { errorLabel->setText(message); });
  connect(&player, &QProcess::readyReadStandardOutput, this,
          &StudioWindow::receivePlayer);
  connect(&player, &QProcess::errorOccurred, this, [this] {
    lastError = player.errorString();
    errorLabel->setText(lastError);
    playAction->setEnabled(true);
    stopAction->setEnabled(false);
  });
  connect(&player, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
          this, [this](int code, QProcess::ExitStatus) {
            receivePlayer();
            stopAction->setEnabled(false);
            playAction->setEnabled(!view.isEmpty());
            log->appendPlainText(tr("플레이 종료 · 코드 %1").arg(code));
            if (code)
              errorLabel->setText(
                  QString::fromUtf8(player.readAllStandardError()));
            emit playerFinished(code);
          });
  refresh();
}

QJsonObject StudioWindow::projectData() const {
  for (auto d : view["documents"].toArray())
    if (d.toObject()["kind"] == "project")
      return d.toObject()["data"].toObject();
  return {};
}
QJsonObject StudioWindow::currentScreen() const {
  for (auto s : projectData()["visual"].toObject()["screens"].toArray())
    if (s.toObject()["id"].toString() == screenId)
      return s.toObject();
  return {};
}
QJsonObject StudioWindow::currentElement() const {
  for (auto e : currentScreen()["elements"].toArray())
    if (e.toObject()["id"].toString() == selection)
      return e.toObject();
  return {};
}
void StudioWindow::request(QString method, QJsonObject params,
                           std::function<void(QJsonObject)> done) {
  if (bridge.busy())
    return;
  params["revision"] = view["revision"].toInt();
  lastError.clear();
  errorLabel->clear();
  const auto focus = method == "edit" ? std::make_shared<InspectorFocusBookmark>(inspector) : nullptr;
  const auto focusRoot = view.value("root").toString(), focusScreen = screenId, focusSelection = selection;
  {
  // Disabling a focused editor can emit editingFinished before bridge.busy()
  // becomes true. Do not let that start a second replacement transaction.
  QScopedValueRollback<bool> blockInspectorSignals(filling, true);
  inspector->setEnabled(false);
  canvas->setEnabled(false);
  hierarchy->setEnabled(false);
  documents->setEnabled(false);
  addAction->setEnabled(false);
  saveAction->setEnabled(false);
  playAction->setEnabled(false);
  undoAction->setEnabled(false);
  redoAction->setEnabled(false);
  findChild<QAction*>("duplicateElementAction")->setEnabled(false);
  for (auto action : {newScreenAction, duplicateScreenAction,
                      renameScreenAction, deleteScreenAction})
    action->setEnabled(false);
  }
  if (focus) focus->watch();
  status->setText(tr("처리 중…"));
  bridge.request(method, params, [this, done, method, focus, focusRoot, focusScreen, focusSelection](QJsonObject reply) {
    if (focus) focus->stopWatching();
    if (reply.contains("view"))
      view = reply["view"].toObject();
    if(reply.contains("localizationSources"))view["localizationSources"]=reply["localizationSources"];
    if (reply.contains("assets"))
      assets = reply["assets"].toArray();
    lastError = reply["error"].toString();
    if ((method == "open" || method == "create") && lastError.isEmpty()) {
      conditionPreview={};
      validatedRevision=-1;
      validationSummary->setText(tr("검증을 누르면 결과와 오류 위치를 표시합니다."));
      openScreens.clear();
      const auto screens =
          projectData()["visual"].toObject()["screens"].toArray();
      screenId.clear();
      for (auto s : screens)
        if (s.toObject()["id"] == "home")
          screenId = "home";
      if (screenId.isEmpty() && !screens.isEmpty())
        screenId = screens[0].toObject()["id"].toString();
      if (!screenId.isEmpty())
        openScreens.append(screenId);
      selection.clear();
    }
    refresh(method == "open" || method == "create");
    if (focus && reply.contains("view") && lastError.isEmpty() &&
        view.value("root").toString() == focusRoot && screenId == focusScreen && selection == focusSelection)
      focus->restore();
    if (!lastError.isEmpty()) {
      errorLabel->setText(lastError);
      bottom->setCurrentWidget(validationPanel);
      log->appendPlainText(lastError);
    }
    if (done)
      done(reply);
    emit updated();
  });
}
void StudioWindow::openProject(const QString &root) {
  request("open", {{"root", root}});
}
void StudioWindow::refresh(bool fit) {
  filling = true;
  auto project = projectData();
  bool loaded = !project.isEmpty();
  viewportModeField->setCurrentIndex(project["visual"].toObject()["viewportMode"]=="responsive"?1:0);
  for (auto d : view["documents"].toArray())
    if (d.toObject()["kind"] == "project")
      projectFile = d.toObject()["file"].toString();
  auto screens = project["visual"].toObject()["screens"].toArray();
  QSet<QString> existing;
  for (auto s : screens)
    existing.insert(s.toObject()["id"].toString());
  for (int i = openScreens.size() - 1; i >= 0; --i)
    if (!existing.contains(openScreens[i]))
      openScreens.removeAt(i);
  if (!openScreens.contains(screenId)) {
    screenId = openScreens.isEmpty() ? QString() : openScreens.last();
    selection.clear();
  }
  const auto managedId = managedScreenId();
  while (documents->count())
    documents->removeTab(0);
  projectList->clear();
  int tabIndex = -1;
  for (const auto &id : openScreens)
    for (auto s : screens) {
      auto screen = s.toObject();
      if (screen["id"].toString() != id)
        continue;
      int i = documents->addTab(screen["label"].toString(id));
      documents->setTabData(i, id);
      documents->setTabToolTip(i, id + tr(" · 닫아도 프로젝트에 유지됩니다"));
      if (id == screenId)
        tabIndex = i;
    }
  for (auto s : screens) {
    auto screen = s.toObject();
    auto item = new QListWidgetItem(screen["label"].toString() + "  ·  " +
                                        screen["id"].toString(),
                                    projectList);
    item->setData(Qt::UserRole, screen["id"].toString());
    if (screen["id"].toString() == (screenId.isEmpty() ? managedId : screenId))
      projectList->setCurrentItem(item);
  }
  documents->setCurrentIndex(tabIndex);
  rebuildHierarchy();
  auto preview = project;
  QJsonObject previewPortraits, previewAttributes;
  for (auto d : view["documents"].toArray()) {
    auto doc = d.toObject();
    auto data = doc["data"].toObject();
    if (doc["kind"] == "attributes")
      for (auto a : data["attributes"].toArray())
        previewAttributes[a.toObject()["id"].toString()] = a;
    if (doc["kind"] == "character")
      for (auto p : project["visual"]
                        .toObject()["defaultScene"]
                        .toObject()["portraits"]
                        .toArray()) {
        auto portrait = p.toObject();
        if (portrait["characterId"] == data["id"])
          previewPortraits[portrait["position"].toString()] =
              data["portraits"].toObject()[portrait["portraitId"].toString()];
      }
  }
  preview["_previewPortraits"] = previewPortraits;
  if(!conditionPreview.isEmpty()&&conditionPreview["revision"]==view["revision"]){
    preview["_previewHiddenIds"]=conditionPreview["hidden"].toObject()[screenId];
    const auto values=conditionPreview["attributes"].toObject();
    for(auto it=values.begin();it!=values.end();++it){auto a=previewAttributes[it.key()].toObject();a["initial"]=it.value();previewAttributes[it.key()]=a;}
  } else if(!conditionPreview.isEmpty())conditionPreview={};
  preview["_previewAttributes"] = previewAttributes;
  preview["_defaultFontPath"] = engineRoot+"/dist/native-player-app/fonts/NotoSansKR.ttf";
  const auto aspect=previewSizeField->currentData().toDouble();const auto visual=project["visual"].toObject();
  if(aspect>0&&visual["viewportMode"]=="responsive") {
    const int w=visual["width"].toInt(),h=visual["height"].toInt();
    preview["_previewWidth"]=qRound(qMax(double(w),h*aspect));preview["_previewHeight"]=qRound(qMax(double(h),w/aspect));preview["_responsivePreview"]=true;
  }
  {
    const QSignalBlocker block(previewLocaleField);auto selectedLocale=previewLocaleField->currentData().toString();previewLocaleField->clear();
    auto loc=project["localization"].toObject();for(auto l:loc["locales"].toArray()){auto o=l.toObject();previewLocaleField->addItem(o["label"].toString(),o["id"].toString());}
    if(previewLocaleField->count()==0)previewLocaleField->addItem(tr("원문"),QString());
    auto index=previewLocaleField->findData(selectedLocale);if(index<0)index=previewLocaleField->findData(loc["defaultLocale"].toString());previewLocaleField->setCurrentIndex(qMax(0,index));
  }
  auto translatedPreview=localizedProject(preview,previewLocaleField->currentData().toString());auto translatedScreen=currentScreen();for(auto s:translatedPreview["visual"].toObject()["screens"].toArray())if(s.toObject()["id"]==screenId)translatedScreen=s.toObject();
  canvas->setScreen(translatedPreview, translatedScreen, assets, selection, fit);
  rendererPreview->setContext(project,screenId,selection,view["revision"].toInt(),view["root"].toString(),previewLocaleField->currentData().toString());
  canvas->setVisible(!screenId.isEmpty());
  emptyDocument->setVisible(screenId.isEmpty());
  assetList->clear();
  for (auto a : assets) {
    auto asset = a.toObject();
    auto item = new QListWidgetItem(QIcon(asset["absolutePath"].toString()),
                                    asset["id"].toString(), assetList);
    item->setToolTip(asset["path"].toString());
    item->setData(Qt::UserRole, asset["id"].toString());
    item->setHidden(!(item->text() + item->toolTip())
                         .contains(assetSearch->text(), Qt::CaseInsensitive));
  }
  problems->clear();
  for (auto d : view["diagnostics"].toArray()) {
    auto v = d.toObject();
    auto item = new QListWidgetItem(
        v["severity"].toString() + " · " + v["message"].toString() + "\n" +
            v["file"].toString() + v["pointer"].toString(),
        problems);
    item->setData(Qt::UserRole, v);
  }
  bottom->setTabText(2, tr("문제 %1").arg(problems->count()));
  if(validatedRevision>=0&&validatedRevision!=view.value("revision").toInt())validationSummary->setText(tr("프로젝트가 수정되었습니다. 다시 검증하세요. 마지막 검증 r%1 · 현재 r%2").arg(validatedRevision).arg(view.value("revision").toInt()));
  addAction->setEnabled(loaded && !screenId.isEmpty());
  saveAction->setEnabled(loaded);
  undoAction->setEnabled(view["undo"].toBool());
  redoAction->setEnabled(view["redo"].toBool());
  playAction->setEnabled(loaded && player.state() == QProcess::NotRunning);
  canvas->setEnabled(loaded && !screenId.isEmpty());
  hierarchy->setEnabled(loaded);
  documents->setEnabled(loaded);
  setWindowTitle((view["dirty"].toBool() ? "● " : "") +
                 project["title"].toString(tr("프로젝트를 열어 주세요")) +
                 " — Raising Studio Qt");
  status->setText(
      view["dirty"].toBool()
          ? tr("저장되지 않은 변경 · r%1").arg(view["revision"].toInt())
          : tr("저장됨 · r%1").arg(view["revision"].toInt()));
  filling = false;
  updateScreenActions();
  refreshContent();
  updateInspector();
}
void StudioWindow::selectElement(const QString &id) {
  selection = id;
  filling = true;
  canvas->select(id);
  QTreeWidgetItemIterator it(hierarchy);while(*it){(*it)->setSelected((*it)->data(0,Qt::UserRole).toString()==id&&(*it)->data(0,Qt::UserRole+1).toString()==screenId);++it;}
  filling = false;
  updateInspector();
  rendererPreview->setContext(projectData(),screenId,selection,view["revision"].toInt(),view["root"].toString(),previewLocaleField->currentData().toString());
}
void StudioWindow::updateTargets(const QString &action, const QJsonObject &e) {
  targetField->clear();
  auto project = projectData();
  QString selected;
  if(action=="executeAction") {
    for(auto a:project.value("gameplay").toObject().value("actions").toArray()) {
      const auto value=a.toObject();targetField->addItem(value.value("name").toString(value.value("id").toString()),value.value("id").toString());
    }
    selected=e.value("actionId").toString();
  }
  if (action == "navigate") {
    for (auto s : project["visual"].toObject()["screens"].toArray()) {
      auto v = s.toObject();
      targetField->addItem(v["label"].toString(v["id"].toString()),
                           v["id"].toString());
    }
    selected = e["screenId"].toString();
  }
  if (action == "visit")
    for (auto l : project["gameplay"].toObject()["locations"].toArray()) {
      auto v = l.toObject();
      targetField->addItem(v["name"].toString(), v["id"].toString());
    }
  if (action == "shop")
    for (auto s : project["gameplay"].toObject()["shops"].toArray()) {
      auto v = s.toObject();
      targetField->addItem(v["name"].toString(), v["locationId"].toString());
    }
  if (action == "visit" || action == "shop")
    selected = e["locationId"].toString();
  if(action=="selectBattleAction"){for(auto a:{"attack","guard","flee"})targetField->addItem(a,a);for(auto v:project.value("gameplay").toObject().value("items").toArray()){auto item=v.toObject();targetField->addItem(tr("물품: ")+item.value("name").toString(),"useItem:"+item.value("id").toString());}selected=e.value("battleAction").toString();if(selected=="useItem")selected+=":"+e.value("itemId").toString();}
  if(action=="selectBattleTarget"){targetField->addItem(tr("자신"),"self");targetField->addItem(tr("적"),"enemy");selected=e.value("battleTarget").toString();}
  if (!selected.isEmpty()) {
    int at = targetField->findData(selected);
    if (at < 0) {
      targetField->addItem(tr("없는 대상: ") + selected, selected);
      at = targetField->count() - 1;
    }
    targetField->setCurrentIndex(at);
  }
  targetField->setEnabled(targetField->count() > 0);
  actionHelp->setText(action == "shop"
                          ? tr("상점이 있는 장소로 이동합니다. 이동 조건과 "
                               "구매 규칙은 실제 플레이어가 검사합니다.")
                      : action == "navigate"
                          ? tr("화면만 바꿉니다. 현재 장소는 바뀌지 않습니다.")
                      : action == "advance"
                          ? tr("현재 단계에 따라 대화·활동 결과를 넘기거나 일정을 진행합니다. 선택지가 있으면 먼저 선택해야 합니다. 실행 중 문구와 활성 상태는 현재 단계에 맞춰 바뀝니다.")
                          : QString());
}
void StudioWindow::editVisibilityCondition() {
  if (filling || bridge.busy() || currentElement().isEmpty()) return;
  const auto original = currentElement();
  const auto revision = view.value("revision"), root = view.value("root");
  const auto selected = selection, screen = screenId;
  class GuardedConditionEditor : public LogicEditor {
  public:
    using LogicEditor::LogicEditor;
    std::function<QString()> guard;
  protected:
    void done(int result) override {
      if (result == QDialog::Accepted && guard) {
        const auto message = guard();
        if (!message.isEmpty()) { findChild<QLabel *>("logicError")->setText(message); return; }
      }
      QDialog::done(result);
    }
  };
  GuardedConditionEditor editor(original.contains("visibleCondition")
      ? original.value("visibleCondition") : QJsonValue(QJsonObject{{"op", "always"}}), "condition", view, this);
  editor.setObjectName("elementVisibilityEditor");
  editor.setWindowTitle(tr("표시 조건 · ") + label(original));
  editor.guard = [&] {
    if (bridge.busy()) return tr("진행 중인 작업이 끝난 뒤 다시 저장하세요.");
    if (view.value("revision") != revision || view.value("root") != root || selection != selected || screenId != screen)
      return tr("프로젝트 또는 선택 요소가 변경되었습니다. 작성 내용을 확인한 뒤 취소하고 다시 여세요.");
    return QString();
  };
  if (editor.exec() != QDialog::Accepted) return;
  const auto condition = editor.value();
  // Opening and accepting an unset condition must not create an unnecessary override.
  if (!original.contains("visibleCondition") && condition == QJsonValue(QJsonObject{{"op", "always"}})) return;
  updateElement([condition](QJsonObject &element) { element["visibleCondition"] = condition; });
}

void StudioWindow::updateInspector() {
  filling = true;
  auto setText = [](QLineEdit *field, const QString &value) {
    if (field->text() != value) field->setText(value);
  };
  auto e = currentElement();
  findChild<QAction*>("duplicateElementAction")->setEnabled(!e.isEmpty()&&!bridge.busy()&&elementLockOwner(currentScreen().value("elements").toArray(),selection).isEmpty());
  inspector->setEnabled(!e.isEmpty() && !bridge.busy());
  selectedTitle->setText(e.isEmpty() ? (currentScreen().isEmpty()?tr("요소를 선택하세요"):tr("화면: %1\n화면 메뉴 > 화면 설정…").arg(currentScreen().value("label").toString())) : label(e));
  setText(nameField, label(e));
  setText(textField, e["text"].toString());
  parentField->clear();parentField->addItem(tr(" 화면 최상위"),QString());for(auto v:currentScreen().value("elements").toArray()){auto g=v.toObject();if(g.value("kind")=="group"&&g.value("id")!=e.value("id"))parentField->addItem(g.value("label").toString(g.value("id").toString()),g.value("id").toString());}int parentIndex=parentField->findData(e.value("parentId").toString());if(parentIndex<0){parentField->addItem(tr("없는 부모: ")+e.value("parentId").toString(),e.value("parentId").toString());parentIndex=parentField->count()-1;}parentField->setCurrentIndex(parentIndex);
  clipChildrenField->setVisible(e.value("kind")=="group");clipChildrenField->setChecked(e.value("clipChildren").toBool());xField->setMinimum(e.contains("parentId")?-3840:0);yField->setMinimum(e.contains("parentId")?-2160:0);
  xField->setValue(e["x"].toInt());
  yField->setValue(e["y"].toInt());
  widthField->setValue(e["width"].toInt(16));
  heightField->setValue(e["height"].toInt(16));
  anchorXField->setCurrentIndex(qMax(0,anchorXField->findData(e["anchorX"].toString("left"))));
  anchorYField->setCurrentIndex(qMax(0,anchorYField->findData(e["anchorY"].toString("top"))));
  findChild<QPushButton*>("componentOverrides")->setEnabled(e["kind"]=="component");
  styleField->clear();styleField->addItem(tr("프로젝트 기본"),QString());
  for(auto s:projectData()["visual"].toObject()["styles"].toArray())styleField->addItem(s.toObject()["label"].toString(),s.toObject()["id"].toString());
  styleField->setCurrentIndex(qMax(0,styleField->findData(e["styleId"].toString())));
  const auto effective=resolvedStyle(projectData()["visual"].toObject(),e);
  fontField->setValue(e["fontSize"].toInt(9));
  fontField->setToolTip(tr("적용 값: %1 px").arg(effective["fontSize"].toInt(20)));
  const auto scope=currentScreen().value("elements").toArray();
  const auto inheritedLock=elementLockOwner(scope,selection,false);
  const bool locked=!elementLockOwner(scope,selection).isEmpty();
  lockedField->setChecked(e.value("locked").toBool());
  lockedField->setEnabled(inheritedLock.isEmpty());
  lockedField->setToolTip(inheritedLock.isEmpty()?QString():tr("부모 그룹 %1의 배치 잠금을 먼저 해제하세요.").arg(inheritedLock));
  hiddenField->setChecked(e["hidden"].toBool());
  visibleConditionField->setText(e.contains("visibleCondition") ? tr("표시 조건 편집… · 설정됨") : tr("표시 조건 편집… · 조건 없음"));
  clearVisibleConditionField->setEnabled(e.contains("visibleCondition"));
  for (auto field : {xField, yField, widthField, heightField})
    field->setEnabled(!locked);
  anchorXField->setEnabled(!locked);anchorYField->setEnabled(!locked);
  parentField->setEnabled(!locked);clipChildrenField->setEnabled(!locked&&e.value("kind")=="group");
  setText(fillField, e["fill"].toString());
  setText(colorField, e["color"].toString());
  fillField->setPlaceholderText(tr("상속: ")+effective["fill"].toString("#355c4c"));
  colorField->setPlaceholderText(tr("상속: ")+effective["color"].toString("#ffffff"));
  QString action = e["action"].toString();
  if (action == "visit")
    for (auto shop : projectData()["gameplay"].toObject()["shops"].toArray())
      if (shop.toObject()["locationId"] == e["locationId"])
        action = "shop";
  while (actionField->count() > actionField->property("builtinActionCount").toInt())
    actionField->removeItem(actionField->count() - 1);
  int at = actionField->findData(action);
  if (at < 0 && !action.isEmpty()) {
    actionField->addItem(tr("기존 행동: ") + action, action);
    at = actionField->count() - 1;
  }
  actionField->setCurrentIndex(at);
  actionField->setEnabled(e["kind"] == "button");
  updateTargets(action, e);
  imageField->clear();
  for (const auto &p : QList<QPair<QString, QString>>{
           {"@background", tr("장면 배경")},
           {"@speaker-portrait", tr("대화 인물 · 얼굴(face)")},
           {"@portrait-center", tr("장면 인물 · 중앙")},
           {"@portrait-left", tr("장면 인물 · 왼쪽")},
           {"@portrait-right", tr("장면 인물 · 오른쪽")}})
    imageField->addItem(p.second, p.first);
  for (auto a : assets)
    if(a.toObject()["kind"]=="image")imageField->addItem(a.toObject()["id"].toString(),
                        a.toObject()["id"].toString());
  const auto source = e["imageSource"].toString("asset");
  imageField->setCurrentIndex(imageField->findData(
      source == "asset" ? e["imageAssetId"].toString() : "@" + source));
  imageField->setEnabled(e["kind"] == "image");
  while (bindingField->count() > builtinBindingCount)
    bindingField->removeItem(bindingField->count() - 1);
  const auto binding = e["binding"].toString("none");
  if (bindingField->findData(binding) < 0)
    bindingField->addItem(tr("기존 연결: ") + binding, binding);
  bindingField->setCurrentIndex(bindingField->findData(binding));
  bindingField->setEnabled(e["kind"] == "text");
  findChild<QPushButton*>("textBindingBatch")->setEnabled(e["kind"]=="text");
  userFieldField->clear();
  for(auto value:projectData().value("userFields").toArray()){const auto field=value.toObject();userFieldField->addItem(field.value("label").toString()+" · "+field.value("id").toString(),field.value("id").toString());}
  const auto fieldId=e.value("userFieldId").toString();
  if(!fieldId.isEmpty()&&userFieldField->findData(fieldId)<0)userFieldField->addItem(tr("없는 필드: ")+fieldId,fieldId);
  userFieldField->setCurrentIndex(qMax(0,userFieldField->findData(fieldId)));
  userFieldField->setEnabled(e["kind"]=="input"||(e["kind"]=="text"&&binding=="user-field"));
  attributeField->clear();
  for (auto d : view["documents"].toArray())
    if (d.toObject()["kind"] == "attributes")
      for (auto v : d.toObject()["data"].toObject()["attributes"].toArray()) {
        auto a = v.toObject();
        attributeField->addItem(a["label"].toString(), a["id"].toString());
      }
  int attrIndex = attributeField->findData(e["attributeId"].toString());
  attributeField->setCurrentIndex(attrIndex < 0 ? 0 : attrIndex);
  attributeField->setEnabled(e["kind"] == "text" &&
                             binding == "attribute-value");
  setText(valueFormatField, e["valueFormat"].toString("{label}: {value}"));
  valueFormatField->setEnabled(e["kind"] == "text" &&
                               (binding.endsWith("value")||binding=="user-field"));
  visibilityField->setCurrentIndex(
      visibilityField->findData(e["visibleWhen"].toString("always")));
  filling = false;
}
void StudioWindow::replaceProject(QJsonObject data) {
  request("edit", {{"edit", QJsonObject{{"type", "replace"},
                                        {"file", projectFile},
                                        {"data", data}}}});
}
void StudioWindow::updateElement(std::function<void(QJsonObject &)> mutate) {
  if (filling || bridge.busy() || selection.isEmpty())
    return;
  auto project = projectData();
  auto visual = project["visual"].toObject();
  auto screens = visual["screens"].toArray();
  for (int i = 0; i < screens.size(); i++) {
    auto s = screens[i].toObject();
    if (s["id"].toString() != screenId)
      continue;
    auto elements = s["elements"].toArray();
    for (int j = 0; j < elements.size(); j++) {
      auto e = elements[j].toObject();
      if (e["id"].toString() != selection)
        continue;
      auto old = e;
      mutate(e);
      if (e == old)
        return;
      const auto inheritedLock=elementLockOwner(elements,selection,false);
      if(!inheritedLock.isEmpty()&&e.value("locked")!=old.value("locked")) {
        errorLabel->setText(tr("부모 그룹 %1의 배치 잠금을 먼저 해제하세요.").arg(inheritedLock));updateInspector();return;
      }
      const auto lockOwner=elementLockOwner(elements,selection);
      if (!lockOwner.isEmpty()) {
        for (auto key : {"x", "y", "width", "height", "anchorX", "anchorY", "parentId", "clipChildren"})
          if (e.value(key) != old.value(key)) {
            errorLabel->setText(tr("배치 잠금을 먼저 해제하세요."));
            updateInspector();
            return;
          }
      }
      elements[j] = e;
    }
    s["elements"] = elements;
    screens[i] = s;
  }
  visual["screens"] = screens;
  project["visual"] = visual;
  replaceProject(project);
}
void StudioWindow::addButton() {
  if (bridge.busy() || currentScreen().isEmpty())
    return;
  auto project = projectData();
  auto visual = project["visual"].toObject();
  auto screens = visual["screens"].toArray();
  int suffix = 1;
  auto elements = currentScreen()["elements"].toArray();
  auto contains = [elements](QString id) {
    for (auto e : elements)
      if (e.toObject()["id"].toString() == id)
        return true;
    return false;
  };
  while (contains("button-" + QString::number(suffix)))
    suffix++;
  selection = "button-" + QString::number(suffix);
  elements.append(QJsonObject{{"id", selection},
                              {"kind", "button"},
                              {"label", tr("새 버튼")},
                              {"text", tr("버튼")},
                              {"x", qMax(0, visual["width"].toInt() - 252)},
                              {"y", qMax(0, visual["height"].toInt() - 88)},
                              {"width", 220},
                              {"height", 56},
                              {"fontSize", 22},
                              {"radius", 10},
                              {"fill", "#355c4c"},
                              {"color", "#ffffff"},
                              {"action", "navigate"},
                              {"screenId", screenId}});
  for (int i = 0; i < screens.size(); i++) {
    auto s = screens[i].toObject();
    if (s["id"].toString() == screenId) {
      s["elements"] = elements;
      screens[i] = s;
    }
  }
  visual["screens"] = screens;
  project["visual"] = visual;
  replaceProject(project);
}
void StudioWindow::deleteElement() {
  if (currentElement().isEmpty() || bridge.busy())
    return;
  if(!elementLockOwner(currentScreen().value("elements").toArray(),selection).isEmpty()){errorLabel->setText(tr("배치 잠금을 먼저 해제하세요."));return;}
  QSet<QString> removed{selection};const auto source=currentScreen().value("elements").toArray();bool added=true;while(added){added=false;for(auto value:source){auto e=value.toObject();auto id=e.value("id").toString();if(removed.contains(e.value("parentId").toString())&&!removed.contains(id)){removed.insert(id);added=true;}}}
  if(removed.size()>1&&QMessageBox::question(this,tr("그룹 삭제"),tr("그룹과 자식 %1개를 삭제할까요? 실행 취소로 복원할 수 있습니다.").arg(removed.size()-1))!=QMessageBox::Yes)return;
  auto p = projectData();
  auto v = p["visual"].toObject();
  auto screens = v["screens"].toArray();
  for (int i = 0; i < screens.size(); i++) {
    auto s = screens[i].toObject();
    if (s["id"].toString() == screenId) {
      QJsonArray kept;
      for (auto e : s["elements"].toArray())
        if (!removed.contains(e.toObject()["id"].toString()))
          kept.append(e);
      s["elements"] = kept;
      screens[i] = s;
    }
  }
  selection.clear();
  v["screens"] = screens;
  p["visual"] = v;
  replaceProject(p);
}
void StudioWindow::duplicateElement() {
  if(!elementLockOwner(currentScreen().value("elements").toArray(),selection).isEmpty()){errorLabel->setText(tr("배치 잠금을 먼저 해제하세요."));return;}
  auto original=currentElement();if(original.empty()||bridge.busy())return;auto p=projectData(),v=p.value("visual").toObject();auto screens=v.value("screens").toArray();auto elements=currentScreen().value("elements").toArray();QSet<QString> subtree{selection},ids;for(auto value:elements)ids.insert(value.toObject().value("id").toString());bool changed=true;while(changed){changed=false;for(auto value:elements){auto e=value.toObject();auto id=e.value("id").toString();if(subtree.contains(e.value("parentId").toString())&&!subtree.contains(id)){subtree.insert(id);changed=true;}}}if(elements.size()+subtree.size()>100){errorLabel->setText(tr("복제 후 화면 요소 수가 100개를 초과합니다."));return;}
  QMap<QString,QString> copies;for(auto value:elements){auto e=value.toObject();auto id=e.value("id").toString();if(!subtree.contains(id))continue;auto base=id.left(45)+"-copy";int n=1;while(ids.contains(base+QString::number(n)))++n;copies[id]=base+QString::number(n);ids.insert(copies[id]);}
  QJsonArray added;for(auto value:elements){auto e=value.toObject();auto id=e.value("id").toString();if(!copies.contains(id))continue;e["id"]=copies[id];if(copies.contains(e.value("parentId").toString()))e["parentId"]=copies[e.value("parentId").toString()];if(id==selection){e["label"]=label(e)+tr(" 복사");if(!e.contains("parentId")){e["x"]=qMin(v.value("width").toInt()-e.value("width").toInt(),e.value("x").toInt()+16);e["y"]=qMin(v.value("height").toInt()-e.value("height").toInt(),e.value("y").toInt()+16);}else{e["x"]=e.value("x").toInt()+16;e["y"]=e.value("y").toInt()+16;}}added.append(e);}for(auto e:added)elements.append(e);selection=copies[selection];for(int i=0;i<screens.size();i++){auto s=screens[i].toObject();if(s.value("id")==screenId){s["elements"]=elements;screens[i]=s;}}v["screens"]=screens;p["visual"]=v;replaceProject(p);
}
void StudioWindow::save() {
  if (bridge.busy()) {
    QTimer::singleShot(50, this, &StudioWindow::save);
    return;
  }
  request("save");
}
void StudioWindow::play() {
  if (bridge.busy()) {
    QTimer::singleShot(50, this, &StudioWindow::play);
    return;
  }
  if (player.state() != QProcess::NotRunning)
    return;
  request("play", {}, [this](QJsonObject reply) {
    if (!reply["error"].toString().isEmpty() || !reply.contains("play"))
      return;
    auto p = reply["play"].toObject();
    playRevision = p["revision"].toInt();
    previousState = {};
    playerBuffer.clear();
    log->clear();
    log->appendPlainText(tr("저장된 프로젝트 r%1 실행 · %2")
                             .arg(playRevision)
                             .arg(p["fingerprint"].toString()));
    QStringList args = {"--bundle",
                        p["directory"].toString(),
                        "--save",
                        p["save"].toString(),
                        "--editor-events",
                        "--report",
                        playReport.isEmpty() ? p["report"].toString()
                                             : playReport};
    if (!playAutomation.isEmpty())
      args << "--automation" << playAutomation;
    if(!previewLocaleField->currentData().toString().isEmpty())args << "--locale" << previewLocaleField->currentData().toString();
    player.setWorkingDirectory(p["directory"].toString());
    player.start(p["executable"].toString(), args);
    playAction->setEnabled(false);
    stopAction->setEnabled(true);
    bottom->setCurrentWidget(log);
    runtimeState->setText(tr("실행 버전 r%1 · 새 게임을 시작하세요.\n편집 "
                             "변경은 다음 실행에 반영됩니다.")
                              .arg(playRevision));
  });
}
void StudioWindow::receivePlayer() {
  playerBuffer += player.readAllStandardOutput();
  int at;
  while ((at = playerBuffer.indexOf('\n')) >= 0) {
    auto line = playerBuffer.left(at);
    playerBuffer.remove(0, at + 1);
    auto event = QJsonDocument::fromJson(line).object();
    if (event["event"] != "editor-view")
      continue;
    auto ui = event["view"].toObject();
    auto state = ui["state"].toObject();
    auto attrs = state["attributes"].toObject();
    auto inventory = state["gameplay"].toObject()["inventory"].toObject();
    QMap<QString, QString> names;
    for (auto document : view["documents"].toArray())
      for (auto attribute :
           document.toObject()["data"].toObject()["attributes"].toArray()) {
        auto a = attribute.toObject();
        names[a["id"].toString()] = a["label"].toString(a["id"].toString());
      }
    for (auto item : projectData()["gameplay"].toObject()["items"].toArray()) {
      auto value = item.toObject();
      names[value["id"].toString()] = value["name"].toString();
    }
    QStringList rows;
    rows << tr("실행 버전 r%1").arg(playRevision)
         << tr("화면: %1").arg(ui["screenId"].toString());
    for (auto i = attrs.begin(); i != attrs.end(); ++i)
      rows << names.value(i.key(), i.key()) + ": " +
                  QString::number(i.value().toDouble());
    for (auto i = inventory.begin(); i != inventory.end(); ++i)
      rows << tr("소지품 %1: %2")
                  .arg(names.value(i.key(), i.key()))
                  .arg(i.value().toInt());
    rows << tr("편집 변경은 다음 실행에 반영됩니다.");
    runtimeState->setText(rows.join('\n'));
    auto command = event["command"].toObject();
    const auto type = command["type"].toString();
    if (!type.isEmpty()) {
      static const QMap<QString, QString> names = {
          {"start", "새 게임 시작"}, {"visit", "장소 이동"},
          {"navigate", "화면 이동"}, {"buy", "상품 구매"},
          {"save", "게임 저장"},     {"load", "게임 불러오기"}};
      QString target = command["locationId"].toString(
          command["screenId"].toString(command["itemId"].toString()));
      if (type == "navigate")
        target = ui["screenId"].toString();
      log->appendPlainText(names.value(type, type) +
                           (target.isEmpty() ? QString() : " · " + target));
    }
    if (!previousState.isEmpty()) {
      auto before = previousState["attributes"].toObject();
      for (auto i = attrs.begin(); i != attrs.end(); ++i)
        if (before[i.key()].toDouble() != i.value().toDouble())
          log->appendPlainText(tr("%1: %2 → %3")
                                   .arg(i.key())
                                   .arg(before[i.key()].toDouble())
                                   .arg(i.value().toDouble()));
      auto old = previousState["gameplay"].toObject()["inventory"].toObject();
      for (auto i = inventory.begin(); i != inventory.end(); ++i)
        if (old[i.key()].toInt() != i.value().toInt())
          log->appendPlainText(tr("소지품 %1: %2 → %3")
                                   .arg(i.key())
                                   .arg(old[i.key()].toInt())
                                   .arg(i.value().toInt()));
    }
    if (!ui["message"].toString().isEmpty())
      log->appendPlainText(ui["message"].toString());
    previousState = state;
  }
}
bool StudioWindow::mayDiscard() {
  if (!view["dirty"].toBool())
    return true;
  return QMessageBox::question(
             this, tr("저장되지 않은 변경"),
             tr("현재 변경을 버리고 계속하시겠습니까?\n유지하려면 취소 후 "
                "프로젝트를 저장하세요."),
             QMessageBox::Discard | QMessageBox::Cancel,
             QMessageBox::Cancel) == QMessageBox::Discard;
}
void StudioWindow::chooseProject(bool create) {
  if (bridge.busy() || !mayDiscard())
    return;
  if (create) { newProjectWizard(); return; }
  auto path = QFileDialog::getExistingDirectory(
      this, tr("project.json이 있는 폴더"), engineRoot + "/projects");
  if (path.isEmpty())
    return;
  openProject(path);
}
void StudioWindow::closeEvent(QCloseEvent *e) {
  if (bridge.busy() || !mayDiscard()) {
    e->ignore();
    return;
  }
  if (player.state() != QProcess::NotRunning) {
    player.terminate();
    if (!player.waitForFinished(1200)) {
      player.kill();
      player.waitForFinished(1200);
    }
  }
  if (!qEnvironmentVariableIsSet("RAISING_QT_TEST")) {
    QSettings settings;
    settings.setValue("windowGeometry", saveGeometry());
    settings.setValue("dockLayout", saveState(1));
  }
  e->accept();
}
