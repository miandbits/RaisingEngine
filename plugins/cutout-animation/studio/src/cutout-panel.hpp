// "캐릭터 애니메이션" dock: character list, setup/animate modes, bone tree, canvas,
// properties, clips/dopesheet/mix and the game-connection (controller) editor.
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
#include "plugin-api.hpp"
#include "rig-canvas.hpp"
#include "rig-render.hpp"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
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
  bool addMixPair();
  bool removeMixPair();
  bool addLayer();
  bool removeLayer();
  bool addBinding();
  bool removeBinding();
  /** Fire a signal into the local preview instance (never the game). Returns matched binding ids. */
  QStringList previewSignal(const QString& name, const QMap<QString, QString>& params);

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
  Dopesheet* dopesheet() const { return dopesheet_; }
  GraphEditor* graph() const { return graph_; }
  /** Rig data currently shown (pending/drag preview included). */
  const cutout::Json& displayData() const;
  const cutout::Json& committedRig() const;
  const cutout::Json& committedController() const;

  // Injectable prompts: automation drives gestures without modal dialogs.
  std::function<QString(const QString& title, const QString& label, const QString& value)> askText;
  std::function<bool(const QString& title, const QString& text)> askConfirm;
  std::function<bool(PartChoice&)> askPart;

 private:
  struct Doc { QString file; QJsonObject envelope; cutout::Json data; };
  raising::StudioHost* host_;
  ImageCache images_;
  QMap<QString, Doc> rigs_, controllers_;
  QJsonArray characters_;  // project characters (kind "character") for characterId
  QString rigId_, controllerId_, selection_, clipId_, status_;
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
  struct SetupDelta { QString bone; cutout::Json delta; };
  std::optional<SetupDelta> lastSetupDelta_;

  // widgets
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
  QTabWidget* bottomTabs_ = nullptr;
  QListWidget* clipList_ = nullptr;
  QSpinBox* clipDuration_ = nullptr;
  QCheckBox* clipLoop_ = nullptr;
  QPushButton *playButton_ = nullptr;
  QCheckBox* loopPreview_ = nullptr;
  QComboBox* speedCombo_ = nullptr;
  QLabel* timeLabel_ = nullptr;
  Dopesheet* dopesheet_ = nullptr;
  GraphEditor* graph_ = nullptr;
  QCheckBox* graphNormalize_ = nullptr;
  QVector<QPushButton*> presetButtons_;
  QSpinBox* mixDefault_ = nullptr;
  QTableWidget* mixTable_ = nullptr;
  QComboBox *defaultClipCombo_ = nullptr, *characterCombo_ = nullptr, *controllerCombo_ = nullptr;
  QTableWidget *layerTable_ = nullptr, *bindingTable_ = nullptr;
  QComboBox* previewSignalCombo_ = nullptr;
  QLineEdit* previewParams_ = nullptr;
  QLabel* previewResult_ = nullptr;
  QTimer* timer_ = nullptr;
  qint64 lastTick_ = 0;
  bool playing_ = false;

  void buildUi();
  QWidget* buildHeader();
  QWidget* buildWorkspace();
  QWidget* buildAnimateTab();
  QWidget* buildControllerTab();

  // documents
  const Doc* rigDoc() const;
  const Doc* controllerDoc() const;
  cutout::Json controllersFor(const QString& rig) const;  // [{id,data}] for core ops
  QStringList clipIds() const;

  // edit pipeline
  bool submit(const QJsonArray& edits);
  QJsonObject replaceEdit(const Doc& doc, const QJsonObject& data) const;
  bool runOps(const std::vector<cutout::Json>& ops, bool keyGesture);
  bool commitRig(const cutout::EditResult& result);
  bool commitController(const cutout::Json& data);
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
  cutout::Pose currentPose() const;
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
