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
  double scale = 1;
  QStringList exclude;  // report layer keys left out
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

 private:
  void collect();  // widgets -> choice_
  void refresh();
  void addItems(QTreeWidgetItem* parent, const QJsonArray& layers);
  void accept() override;
  LayerImportChoice& choice_;
  Preview preview_;
  QLabel *source_ = nullptr, *summary_ = nullptr;
  QComboBox *target_ = nullptr, *origin_ = nullptr;
  QLineEdit* id_ = nullptr;
  QCheckBox* trim_ = nullptr;
  QCheckBox* aimBones_ = nullptr;
  QDoubleSpinBox* scale_ = nullptr;
  QTreeWidget* tree_ = nullptr;
  QListWidget* warnings_ = nullptr;
  bool filling_ = false;
};
}  // namespace cutout_studio
