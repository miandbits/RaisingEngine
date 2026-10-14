// "캐릭터 애니메이션" authoring panel: character list, setup/animate modes, bone tree, canvas,
// properties, clips/dopesheet/mix and the game-connection (controller) editor. One Spine-like
// splitter layout; it is the host's full-window workspace (StudioWorkspacePlugin) or, on a
// host without workspaces, the content of the dock.
//
// Single source of truth = StudioHost::view() documents. Everything here is a cache that
// refreshFromView() rebuilds. Every user gesture builds new envelope JSON (through the core
// editing ops) and sends exactly ONE host->request("edit", ...) so host undo/redo works.
#pragma once
#include <QJsonObject>
#include <QMap>
#include <QPointer>
#include <QSet>
#include <QWidget>
#include <functional>
#include <memory>
#include <optional>
#include "cutout/editing.hpp"
#include "onion-skin.hpp"
#include "plugin-api.hpp"
#include "rig-canvas.hpp"
#include "rig-render.hpp"
#include "time-view.hpp"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFrame;
class QLabel;
class QLineEdit;
class QListWidget;
class QMimeData;
class QPushButton;
class QScrollBar;
class QSpinBox;
class QSplitter;
class QTabWidget;
class QTableWidget;
class QTimer;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace cutout_studio {
class Dopesheet;
class GraphEditor;
class BoneTree;

constexpr const char* PLUGIN_ID = "cutout-animation";
constexpr const char* RIG_DIR = "plugins/cutout-animation/rig/";
constexpr const char* CONTROLLER_DIR = "plugins/cutout-animation/controller/";

/** v1 signal vocabulary (docs/contract.md) with the params bindings may match. */
struct SignalInfo { const char* name; const char* label; QStringList params; };
const QVector<SignalInfo>& signalVocabulary();

class CutoutPanel : public QWidget {
  Q_OBJECT
 public:
  enum class Mode { Setup, Animate };
  struct PartChoice { QString assetId, importFile, slot, name, bone; };
  explicit CutoutPanel(raising::StudioHost* host, QWidget* parent = nullptr);
  ~CutoutPanel() override;

  void refreshFromView();
  /** Plugin deactivate: stop timers and never touch the host again. */
  void detachHost();
  ImageCache& images() { return images_; }

  // ---- Navigation (openEditor, diagnostics click, automation) ----
  void selectRig(const QString& id);
  void selectController(const QString& id);
  void selectItem(const QString& ref);  // "bone:<id>" | "slot:<id>" | ""
  void selectClip(const QString& id);
  void setMode(Mode mode);
  void setPlayhead(int ms);
  void setTool(int tool);
  int tool() const;
  void showControllerTab();
  void setAutoKey(bool on);

  // ---- Gestures (each sends at most one edit request; false = refused, see status()) ----
  bool newCharacter();
  bool renameCharacter();
  bool deleteCharacter();
  bool addBone();
  bool addPartInteractive();
  bool addPart(const QString& assetId, const QString& slot, const QString& name, const QString& bone);
  /** importImage (asset gesture) then addPart (edit gesture) once the asset exists. */
  void importAndAddPart(const QString& file, const QString& slot, const QString& name, const QString& bone);
  bool deleteSelected();
  bool requestReparent(const QString& bone, const QString& parent);  // tree drop, keeps world pose
  bool requestSlotBone(const QString& slot, const QString& bone);    // tree drop of a part
  bool moveSelectedSlot(int delta);
  bool keySelected();        // K
  bool discardPending();
  bool applySetupDeltaToKeys();
  bool newClip();
  bool duplicateClip();
  bool renameClip();
  bool deleteClip();
  bool applyBreathing();
  bool applyBlink();
  bool moveKey(int row, int fromMs, int toMs);
  bool deleteKey(int row, int ms);
  bool setKeyInterp(int row, int ms, const QString& interp);
  /** Interp/preset ("linear", "step", "easeIn", "auto", "back"...) on the graph-selected keys
   *  (or the dopesheet key): one batch edit for all of them. */
  bool applyPreset(const QString& preset);
  /** Graph key drag: moveKey and/or setKey value in one edit. Collisions are refused. */
  bool moveGraphKey(const QString& target, const QString& property, int fromMs, int toMs, double value);
  /** Graph handle drag: setCurve of the segment starting at `ms`. */
  bool setKeyCurve(const QString& target, const QString& property, int ms, const cutout::Curve& curve);
  // Key selection gestures (selection shared by dopesheet and graph = timeView()->selection()).
  /** Move keys by deltaMs (core moveKeys: spacing kept, collisions/range refused) and set
   *  `values` (original ref -> new value) at the moved times: one edit. */
  bool moveKeys(const QVector<KeyRef>& keys, int deltaMs, const QHash<KeyRef, double>& values = {});
  bool scaleKeys(const QVector<KeyRef>& keys, int pivotMs, double factor);
  bool deleteKeys(const QVector<KeyRef>& keys);
  bool offsetSelectedKeys(int deltaMs);   // offset field
  bool scaleSelectedKeys(double factor);  // timing scale field, around the first selected key
  bool copyKeys();                        // selection -> clipboard (no edit)
  /** Clipboard at the playhead of the current clip (same-time keys overwritten). A clipboard of
   *  one target goes onto the selected bone/part when that is another target of the same kind. */
  bool pasteKeys();
  bool duplicateKeys();  // selection copied to the playhead (clipboard untouched)
  /** Pose of the selected bone and its children (all bones when none is selected) at the playhead. */
  bool copyPose();
  bool pastePose();      // copied pose as keys at the playhead of the current clip
  void stepPlayhead(int steps);  // by the step field (10 ms default)
  /** Editor-only preview loop range of the current clip (never stored in documents). */
  void setLoopRange(int inMs, int outMs);
  void clearLoopRange();
  int clipboardKeys() const;
  bool addMixPair();
  bool removeMixPair();
  bool addLayer();
  bool removeLayer();
  bool addBinding();
  bool removeBinding();
  // ---- Skins (outfits) and expressions. The preview look of the canvas is editor-only state;
  // every change of skins/expressions themselves is one core edit (addSkin, captureExpression...).
  void setPreviewSkin(const QString& id);        // "" = default attachments only
  void setPreviewExpression(const QString& id);  // "" = no expression
  QString previewSkin() const { return previewSkin_; }
  QString previewExpression() const { return previewExpression_; }
  bool newSkin();
  bool duplicateSkin();
  bool renameSkin();
  bool deleteSkin();
  /** Image `assetId` for `slot` in the preview skin, as attachment `name` (default: the name shown now). */
  bool assignSkinImage(const QString& slot, const QString& assetId, const QString& name = QString());
  /** importImage (asset gesture) then assignSkinImage (edit gesture). */
  void importAndAssignSkinImage(const QString& file, const QString& slot);
  bool assignSkinImageInteractive();  // 그림 지정…: image picker for the selected part
  bool removeSkinAttachment();        // selected entry of the skin's attachment list
  /** Drop of PNG files / an asset id onto part `slot` ("" = selected part): goes into the preview skin. */
  bool dropImages(const QMimeData* mime, const QString& slot);
  /** 현재 상태로 표정 만들기: the shown changes (Animate: unsaved changes at the playhead) -> new expression. */
  bool captureExpression();
  bool recaptureExpression();  // same, overwriting the previewed expression
  bool renameExpression();
  bool deleteExpression();
  /** Expression table: value "-" = not part of the expression, "" = hidden (null), else an attachment name. */
  bool setExpressionSlot(const QString& slot, const QString& value);
  /** Bone override channels ("x", "rotation"...); channels missing from `values` are dropped, empty = remove. */
  bool setExpressionBone(const QString& bone, const QMap<QString, double>& values);
  /** 표정 키: rig expression key (the previewed expression, or none) at the playhead. */
  bool keyExpression();
  // ---- IK constraints (plugin 1.3.0). Setup mode edits the constraint (setIk); Animate mode keys
  // mix/bend at the playhead (ik:<id> tracks). Dragging the target bone is an ordinary bone drag.
  /** 팔·다리 IK (aim=false): [parent, selected bone]; 조준 IK (aim=true): [selected]. Creates the target bone. */
  bool newIk(bool aim = false);
  bool selectIk(const QString& id);
  QString ikId() const { return ikId_; }
  bool setIkMix(double mix);
  bool setIkBend(bool positive);
  bool setIkOrder(int order);
  bool setIkTarget(const QString& bone);
  bool selectIkTarget();  // canvas/tree selection = the constraint's target bone
  bool renameIk();
  bool deleteIk();        // also its target bone when nothing else uses it
  bool keyIk();           // IK 키: mix + bend of the selected constraint at the playhead
  bool bakeIk();          // IK 결과를 키로 굽기: solved rotations as keys, mix 0 there
  // ---- Meshes (plugin 1.4.0) on the selected part's shown attachment (the preview skin's own when
  // it provides it). Setup mode edits the mesh (vertices, weights); Animate mode makes deform keys.
  enum class MeshEdit { Off = 0, Vertices = 1, Weights = 2 };
  /** 메시로 변환: rows × cols grid; trim drops cells without opaque pixels (alpha-trimmed grid). */
  bool convertToMesh(int rows, int cols, bool trim);
  bool convertToRegion();
  void setMeshEdit(MeshEdit mode);  // canvas gestures edit vertices / paint weights while on
  MeshEdit meshEdit() const { return meshEdit_; }
  QString meshSlot() const;         // selected part when it shows a mesh ("" otherwise)
  void selectVertices(const QVector<int>& vertices, bool add = false);
  QVector<int> selectedVertices() const;
  /** Selected vertices moved by `rigDelta`: Setup = setMeshVertices, Animate = deform key (pending unless auto-key). */
  bool moveVertices(const QPointF& rigDelta);
  bool autoWeights(const QStringList& bones = {});  // 자동 웨이트 (bones: checked in the list when empty)
  bool paintWeights(const QVector<int>& vertices);  // brush: bone/weight/strength of the weight tools
  bool unbindMesh();
  void setBrush(const QString& bone, double weight, double strength, double radius);
  bool keyDeform();          // deform key of the shown offsets at the playhead
  bool deleteDeformKey();    // the deform key at the playhead
  // ---- Onion skin + motion trail (editor-only: QSettings, never an edit or a clip event)
  struct GhostInfo { double timeMs = 0; int offset = 0; double alpha = 0; QColor tint; bool committed = false; };
  const OnionSettings& onionSettings() const { return onion_; }
  void setOnionSettings(const OnionSettings& settings);  // syncs the UI and saves to QSettings
  void setOnionSkin(bool on);                            // toolbar toggle / O
  /** Ghosts currently drawn (empty when off, outside Animate mode or in the controller preview). */
  const QVector<GhostInfo>& ghosts() const { return ghostInfo_; }
  int ghostSamples() const { return ghostCache_.samples() + committedCache_.samples(); }
  QSet<QString> ghostHiddenSlots() const;
  const QVector<double>& trailTimes() const { return trailTimes_; }
  const QPolygonF& trailPoints() const { return trailPoints_; }
  const QPolygonF& trailKeyPoints() const { return trailKeys_; }
  const QVector<int>& trailKeyTimes() const { return trailKeyTimes_; }
  /** Fire a signal into the local preview instance (never the game). Returns matched binding ids. */
  QStringList previewSignal(const QString& name, const QMap<QString, QString>& params);
  // ---- Layout (editor-only presentation: QSettings plugins/cutout-animation/layout, never an edit)
  /** Tab / 캔버스 크게: hide the left and right panels so the canvas takes the width. */
  void setFullView(bool on);
  bool fullView() const;
  /** Splitter states (objectName -> saveState) + collapsed sections. Saved on every splitter move. */
  void saveLayout() const;
  void restoreLayout();
  QList<QSplitter*> splitters() const;
  /** Current pose drawn contain-fit into `size` (dock launcher mini preview). */
  QImage thumbnail(const QSize& size);

  // ---- State (automation / tests) ----
  QString rigId() const { return rigId_; }
  QString controllerId() const { return controllerId_; }
  QString selection() const { return selection_; }
  QString clipId() const { return clipId_; }
  Mode mode() const { return mode_; }
  int playhead() const { return playheadMs_; }
  bool hasPending() const { return working_.has_value(); }
  QString keyState() const;
  QString status() const;
  RigCanvas* canvas() const { return canvas_; }
  QWidget* leftPane() const { return leftPane_; }
  QWidget* rightPane() const { return rightPane_; }
  Dopesheet* dopesheet() const { return dopesheet_; }
  GraphEditor* graph() const { return graph_; }
  TimeView* timeView() const { return timeView_; }
  /** Rig data currently shown (pending/drag preview included). */
  const cutout::Json& displayData() const;
  const cutout::Json& committedRig() const;
  const cutout::Json& committedController() const;

  // Injectable prompts: automation drives gestures without modal dialogs.
  std::function<QString(const QString& title, const QString& label, const QString& value)> askText;
  std::function<bool(const QString& title, const QString& text)> askConfirm;
  std::function<bool(PartChoice&)> askPart;

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  struct Doc { QString file; QJsonObject envelope; cutout::Json data; };
  raising::StudioHost* host_;
  ImageCache images_;
  QMap<QString, Doc> rigs_, controllers_;
  QJsonArray characters_;  // project characters (kind "character") for characterId
  QString rigId_, controllerId_, selection_, clipId_, status_;
  QString previewSkin_, previewExpression_;  // editor-only canvas look (never stored)
  QString ikId_;                              // IK constraint shown in the IK section (editor state)
  Mode mode_ = Mode::Setup;
  int playheadMs_ = 0, lastRevision_ = -1;
  double playheadExact_ = 0;
  bool filling_ = false, inSubmit_ = false, refreshQueued_ = false, autoKey_ = false, dragActive_ = false;
  std::optional<cutout::Json> working_;      // uncommitted animate-mode changes (auto-key off): shown, not saved
  std::optional<cutout::Json> inflight_;     // sent, waiting for the host reply (avoids flicker)
  std::optional<cutout::Json> dragPreview_;  // live drag preview, never sent until release
  cutout::Json dragBase_;                    // data/pose at drag start: deltas are relative to it
  std::shared_ptr<const cutout::Rig> dragRig_;
  cutout::Pose dragPose_;
  std::shared_ptr<const cutout::Rig> rig_;   // parse of displayData(); clips forced non-looping for scrubbing
  QString rigError_;
  QSet<QString> hidden_;                     // editor-only eye toggles: never stored
  std::unique_ptr<cutout::Instance> preview_;
  QString previewKey_;
  TimeView* timeView_ = nullptr;             // zoom/scroll/playhead/loop/selection of dopesheet + graph
  cutout::Json clipboard_, poseClipboard_;   // editor-only (core copyKeys/copyPose results)
  QMap<QString, QPair<int, int>> loops_;     // "<rig>/<clip>" -> preview loop range, editor-only
  QString viewClip_;                         // clip the time view currently shows
  // onion skin / motion trail (editor-only)
  OnionSettings onion_;
  GhostPoseCache ghostCache_, committedCache_;
  quint64 displayGeneration_ = 0;            // ++ whenever rig_ is rebuilt: ghost caches key on it
  std::shared_ptr<const cutout::Rig> committedParsed_;  // includeCurrent: the saved rig
  QString committedParsedKey_;
  QVector<GhostInfo> ghostInfo_;
  QString trailKey_;
  QVector<double> trailTimes_;
  QPolygonF trailPoints_, trailKeys_;
  QVector<int> trailKeyTimes_;
  bool onionFilling_ = false;
  struct SetupDelta { QString bone; cutout::Json delta; };
  std::optional<SetupDelta> lastSetupDelta_;

  // widgets
  QSplitter *mainSplit_ = nullptr, *workspaceSplit_ = nullptr, *leftSplit_ = nullptr, *rightSplit_ = nullptr, *timelineAreaSplit_ = nullptr;
  QWidget *leftPane_ = nullptr, *rightPane_ = nullptr;
  QToolButton* fullViewButton_ = nullptr;
  QList<QToolButton*> sections_;  // collapsible section headers (objectName "<section>Toggle")
  bool restoringLayout_ = false;
  QTreeWidget* characterList_ = nullptr;
  QListWidget* diagnostics_ = nullptr;
  QPushButton *setupButton_ = nullptr, *animateButton_ = nullptr;
  QLabel *banner_ = nullptr, *statusLabel_ = nullptr;
  BoneTree* tree_ = nullptr;
  QListWidget* drawOrder_ = nullptr;
  RigCanvas* canvas_ = nullptr;
  QToolButton* toolButtons_[4] = {};
  QMap<QString, QDoubleSpinBox*> props_;
  QComboBox* attachmentCombo_ = nullptr;
  QPushButton *keyButton_ = nullptr, *discardButton_ = nullptr, *deltaButton_ = nullptr;
  QCheckBox* autoKeyCheck_ = nullptr;
  QLabel* selectionLabel_ = nullptr;
  QToolButton *onionButton_ = nullptr, *onionSettingsButton_ = nullptr, *trailButton_ = nullptr;
  QFrame* onionPopup_ = nullptr;
  QSpinBox *onionBefore_ = nullptr, *onionAfter_ = nullptr, *onionStep_ = nullptr, *onionOpacity_ = nullptr, *onionFalloff_ = nullptr;
  QComboBox *onionMode_ = nullptr, *onionBeforeColor_ = nullptr, *onionAfterColor_ = nullptr;
  QCheckBox *onionCurrent_ = nullptr, *onionSubtree_ = nullptr;
  QTabWidget* bottomTabs_ = nullptr;
  QListWidget* clipList_ = nullptr;
  QSpinBox* clipDuration_ = nullptr;
  QCheckBox* clipLoop_ = nullptr;
  QPushButton *playButton_ = nullptr;
  QCheckBox* loopPreview_ = nullptr;
  QComboBox* speedCombo_ = nullptr;
  QLabel* timeLabel_ = nullptr;
  QSpinBox *timeField_ = nullptr, *stepField_ = nullptr, *offsetField_ = nullptr, *scaleField_ = nullptr;
  QScrollBar* timeScroll_ = nullptr;
  QVector<QWidget*> timelineTools_;  // enabled in Animate mode only
  Dopesheet* dopesheet_ = nullptr;
  GraphEditor* graph_ = nullptr;
  QCheckBox* graphNormalize_ = nullptr;
  QVector<QPushButton*> presetButtons_;
  QSpinBox* mixDefault_ = nullptr;
  QTableWidget* mixTable_ = nullptr;
  QComboBox *defaultClipCombo_ = nullptr, *characterCombo_ = nullptr, *controllerCombo_ = nullptr;
  QComboBox *defaultSkinCombo_ = nullptr, *defaultExpressionCombo_ = nullptr;
  QTabWidget* lookTabs_ = nullptr;
  QListWidget *skinList_ = nullptr, *skinAttachments_ = nullptr, *expressionList_ = nullptr;
  QLabel* skinHint_ = nullptr;
  QTableWidget* expressionTable_ = nullptr;
  QListWidget* ikList_ = nullptr;
  QComboBox* ikTarget_ = nullptr;
  QDoubleSpinBox* ikMix_ = nullptr;
  QCheckBox* ikBend_ = nullptr;
  QSpinBox* ikOrder_ = nullptr;
  QLabel* ikInfo_ = nullptr;
  QVector<QWidget*> ikEditors_, ikAnimateTools_;
  // meshes (editor state: mode, vertex selection and brush never reach documents)
  MeshEdit meshEdit_ = MeshEdit::Off;
  QSet<int> meshSelection_;
  QString meshSelectionKey_;  // slot/attachment the selection belongs to
  QSpinBox *meshRows_ = nullptr, *meshCols_ = nullptr;
  QCheckBox* meshTrim_ = nullptr;
  QToolButton *meshVerticesButton_ = nullptr, *meshWeightsButton_ = nullptr;
  QListWidget* meshBones_ = nullptr;
  QComboBox* brushBone_ = nullptr;
  QDoubleSpinBox *brushWeight_ = nullptr, *brushStrength_ = nullptr, *brushRadius_ = nullptr;
  QLabel* meshInfo_ = nullptr;
  QVector<QWidget*> meshTools_, meshSetupTools_, meshAnimateTools_;
  QTableWidget *layerTable_ = nullptr, *bindingTable_ = nullptr;
  QComboBox* previewSignalCombo_ = nullptr;
  QLineEdit* previewParams_ = nullptr;
  QLabel* previewResult_ = nullptr;
  QTimer* timer_ = nullptr;
  qint64 lastTick_ = 0;
  bool playing_ = false;

  void buildUi();
  QWidget* buildCharacters();
  QWidget* buildLooks();
  void refreshLooks();
  QWidget* buildIk();
  void refreshIk();
  QWidget* buildMesh();
  void refreshMesh();
  /** The edited mesh: slot index, attachment, and op fields {slot, name, skin?} (false = none). */
  bool meshTarget(int& slot, const cutout::Attachment*& attachment, cutout::Json& fields) const;
  void onMeshDrag(const QPointF& rigDelta, bool finished);
  std::vector<cutout::Json> meshMoveOps(const QPointF& rigDelta) const;
  const cutout::Json* currentIk() const;  // ikId_ in displayData(), with a status when none
  cutout::Look lookFor(const cutout::Rig& rig) const;  // preview skin/expression ids -> indices of `rig`
  /** Preview skin when it provides (slot, name): setup edits of that attachment go to the skin. */
  QString skinProviding(const QString& slot, const QString& name) const;
  QWidget* buildWorkspace();
  QWidget* section(const QString& title, const char* name, QWidget* body);
  QWidget* buildAnimateTab();
  QWidget* buildControllerTab();
  QFrame* buildOnionSettings();
  void syncOnionUi();
  void onionFromUi();
  void updateOnion();     // ghosts + trail for the current canvas pose
  void updateTrail(int clip);
  bool showingPreview() const;  // canvas shows the controller preview instance, not the clip
  QSet<int> boneSubtree(int bone) const;
  QVector<int> onionKeyTimes(int clip) const;
  std::shared_ptr<const cutout::Rig> committedParsed();

  // documents
  const Doc* rigDoc() const;
  const Doc* controllerDoc() const;
  cutout::Json controllersFor(const QString& rig) const;  // [{id,data}] for core ops
  QStringList clipIds() const;

  // edit pipeline
  bool submit(const QJsonArray& edits);
  QJsonObject replaceEdit(const Doc& doc, const QJsonObject& data) const;
  /** `accepted` runs after the core accepted the ops and before the commit (selection follow-up). */
  bool runOps(const std::vector<cutout::Json>& ops, bool keyGesture, const std::function<void(const cutout::EditResult&)>& accepted = {});
  bool refusePending();  // true (and a status) while uncommitted pose changes exist
  cutout::Json refsJson(const QVector<KeyRef>& keys) const;
  void selectKeysFrom(const cutout::Json& refs);
  void keyCommand(const QString& command);
  void updateTimeUi(int duration);
  void syncTimeScroll();
  bool commitRig(const cutout::EditResult& result);
  bool commitController(const cutout::Json& data);
  bool captureExpressionAs(const QString& id, bool replace);
  bool busy();
  void setStatus(const QString& text, bool error = false);

  // view cache
  void rebuildDisplay(bool full = true);
  void refreshCharacters();
  void refreshDiagnostics();
  void refreshTree();
  void refreshDrawOrder();
  void refreshProperties();
  void refreshClips();
  void refreshDopesheet();
  void refreshGraph();
  QString graphTarget() const;  // selection, else the dopesheet's selected row
  void refreshMix();
  void refreshController();
  void refreshModeUi();
  void updateCanvas();
  /** Canvas pose; expressions=false gives the raw (keyable) values: properties, K and drags use it. */
  cutout::Pose currentPose(bool expressions = true) const;
  int boneIndex(const QString& id) const;
  QString selectedBone() const;  // bone of the selection (slot -> its bone)
  QPointF manipulationOrigin(const QString& ref) const;
  std::vector<cutout::Json> dragOps(const RigCanvas::Drag& drag) const;
  void onCanvasDrag(const RigCanvas::Drag& drag, bool finished);
  void commitProperty(const QString& property, double value);
  void commitAttachmentChoice();
  void commitClipSettings();
  void commitMixTable();
  void commitControllerFromUi();
  void tick();
  void togglePlay(bool on);
  void ensurePreview();
  void onDiagnosticClicked(const QString& file, const QString& pointer);
  QString uniqueBoneId(const QString& base) const;
  QString uniqueClipId(const QString& base) const;
};
}  // namespace cutout_studio
