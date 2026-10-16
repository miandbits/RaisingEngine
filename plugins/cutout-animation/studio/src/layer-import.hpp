// "PSD 가져오기…" (plugin 1.11.0): preview dialog of the layered-art import. The mapping itself is
// the plugin service op `importLayers` (service/layer-import.mjs), reached through the host's
// `pluginImport` request (dryRun = preview; otherwise images + documents as ONE undoable edit),
// so the dialog, MCP plugin_import and plugin_call show and do exactly the same thing.
#pragma once
#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <functional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QAction;
class QTreeWidget;
class QTreeWidgetItem;

namespace cutout_studio {
/** What the import sends: the `importLayers` payload (docs/contract.md "Layer import"). */
struct LayerImportChoice {
  QString file;
  QString target;  // "" = new character, else the rig id to update (re-import)
  QString id;      // id of a new character (rig + controller)
  QString origin = "bottom-center";
  bool trim = true;
  bool aimBones = true;  // boneDirection "auto" (plugin 1.14.0): new bones point along their part
  // scale "auto" (plugin 1.15.0): canvas long edge <= 2048 px, pictures downscaled while reading;
  // a small document gets 1, so it is the default for every file.
  bool autoScale = true;
  double scale = 1;
  bool skipEffects = false;  // effects "skip": leave out layers whose blend mode cannot be drawn
  // background "keep" (plugin 1.21.0): import the bottom layer the service takes for a paper sheet
  // (default "skip": its row is unchecked, 「배경으로 보여 뺐음」; checking it sets this).
  bool keepBackground = false;
  QStringList exclude;  // report layer keys left out
  QStringList merge;    // report keys of groups flattened into one picture (plugin 1.15.0)
  QJsonObject payload() const;
};

/** Korean labels of report roles / statuses (dialog and status line). */
QString layerRoleLabel(const QString& role);
QString layerStatusLabel(const QString& status);
/** One-line summary of an import report ("본 3 · 슬롯 8 · 첨부 10(숨김 2) …"). */
QString layerImportSummary(const QJsonObject& report);

class LayerImportDialog : public QDialog {
 public:
  /** Re-runs the preview for the current choice; `done(report, error)`. */
  using Preview = std::function<void(const LayerImportChoice&, std::function<void(const QJsonObject&, const QString&)>)>;
  LayerImportDialog(QWidget* parent, LayerImportChoice& choice, const QStringList& rigs, const QJsonObject& report, Preview preview);
  void showReport(const QJsonObject& report);
  QTreeWidget* tree() const { return tree_; }
  /** Plugin 1.20.0: layer rows whose name contains every word of `text` stay, with their parent
   *  groups (cutoutLayerImportFilter); "" shows all. Kept across 미리 보기 갱신. */
  void setFilter(const QString& text);
  /** Size the dialog opens at: the remembered one (QSettings, editor-only), else ~70 % of the
   *  available screen, at least 900x600 (or the whole screen when it is smaller). */
  static QSize defaultSize(const QWidget* parent);
  static constexpr const char* SIZE_SETTINGS = "plugins/cutout-animation/layerImport";

 private:
  void collect();  // widgets -> choice_
  void refresh(bool fromWidgets = true);
  void autoMerge();  // the report's proposal (autoMerge.keys) into the merge list, then preview again
  void addItems(QTreeWidgetItem* parent, const QJsonArray& layers);
  void accept() override;
  void done(int result) override;  // remembers the size
  void applyFilter();
  LayerImportChoice& choice_;
  Preview preview_;
  QJsonObject report_;
  QLabel *source_ = nullptr, *summary_ = nullptr, *scaleInfo_ = nullptr;
  QComboBox *target_ = nullptr, *origin_ = nullptr;
  QLineEdit* id_ = nullptr;
  QLineEdit* filter_ = nullptr;
  QCheckBox* trim_ = nullptr;
  QCheckBox* aimBones_ = nullptr;
  QCheckBox* autoScale_ = nullptr;
  QCheckBox* skipEffects_ = nullptr;
  QDoubleSpinBox* scale_ = nullptr;
  QTreeWidget* tree_ = nullptr;
  QListWidget* warnings_ = nullptr;
  QPushButton *ok_ = nullptr, *autoMerge_ = nullptr;
  QAction* mergeAction_ = nullptr;
  bool filling_ = false;
};
}  // namespace cutout_studio
