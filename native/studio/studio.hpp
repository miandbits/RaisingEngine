#pragma once
#include "bridge.hpp"
#include "canvas.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTabBar>
#include <QTabWidget>
#include <QTreeWidget>
class RendererPreviewDock;
class QPushButton;
class QDockWidget;
class QMenu;
class StudioPluginManager;

class StudioWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit StudioWindow(QString engineRoot, QWidget *parent = nullptr);
  ~StudioWindow() override;
  void openProject(const QString &root);
  bool busy() const { return bridge.busy(); }
  QJsonObject currentView() const { return view; }
  QJsonObject projectData() const;
  QString selectedId() const { return selection; }
  QString playAutomation, playReport;
  QString lastError;
  /** Automation: package file used by 패키지 설치… instead of the file dialog. */
  QString pluginPackagePath;
  StudioPluginManager *pluginManager() const { return plugins; }
  /** Mirrors a service `plugins` object; with load=true also (un)loads authoring DLLs. */
  void applyPluginState(const QJsonObject &state, bool load = true);
  QDockWidget *addStudioDock(const QString &title, const QString &name, QWidget *child, Qt::DockWidgetArea area);
signals:
  void updated();
  void playerFinished(int code);

protected:
  void closeEvent(QCloseEvent *event) override;

private:
  // The plugin host forwards StudioHost calls to the window's own state and request().
  friend class StudioPluginManager;
  StudioPluginManager *plugins;
  QMenu *viewMenu, *pluginsMenu, *paletteMenu;
  QWidget *pluginGroup;
  QLabel *pluginStatusLabel;
  QComboBox *pluginRefField;
  QLineEdit *pluginPropsField;
  QPushButton *pluginEditButton;
  void showPluginManager();
  void rebuildPluginUi();
  void addPluginElement(const QString &plugin, const QString &type);
  void updatePluginInspector(const QJsonObject &element);
  void editPluginElement();
  void editPluginProps();
  QString engineRoot, screenId = "home", selection, projectFile;
  QStringList openScreens;
  StudioBridge bridge;
  QJsonObject view;
  QJsonArray assets;
  QProcess player;
  QByteArray playerBuffer;
  QJsonObject previousState;
  bool filling = false;
  int playRevision = 0;
  ScreenCanvas *canvas;
  RendererPreviewDock *rendererPreview;
  QTreeWidget *hierarchy;
  QLineEdit *hierarchySearch;
  QTabBar *documents;
  QTabWidget *bottom;
  QListWidget *projectList, *assetList, *problems;
  QPlainTextEdit *log;
  QWidget *validationPanel;
  QLabel *validationSummary;
  int validatedRevision=-1;
  QWidget *inspector;
  QLabel *selectedTitle, *actionHelp, *status, *runtimeState, *errorLabel;
  QLabel *emptyDocument;
  QLineEdit *nameField, *textField, *fillField, *colorField;
  QLineEdit *assetSearch;
  QSpinBox *xField, *yField, *widthField, *heightField, *fontField;
  QComboBox *actionField, *targetField, *styleField;
  QComboBox *previewLocaleField;
  QComboBox *anchorXField, *anchorYField, *viewportModeField, *previewSizeField;
  QComboBox *imageField, *bindingField, *attributeField, *visibilityField, *userFieldField;
  int builtinBindingCount=0;
  QLineEdit *valueFormatField;
  QListWidget *contentList;
  QCheckBox *lockedField, *hiddenField;
  QPushButton *visibleConditionField, *clearVisibleConditionField;
  QAction *addAction, *undoAction, *redoAction, *saveAction, *playAction,
      *stopAction;
  QAction *newScreenAction, *duplicateScreenAction, *renameScreenAction,
      *deleteScreenAction;
  QByteArray defaultLayout;
  void request(QString method, QJsonObject params = {},
               std::function<void(QJsonObject)> done = {});
  void refresh(bool fit = false);
  void updateInspector();
  void editVisibilityCondition();
  void updateTargets(const QString &action, const QJsonObject &element);
  void selectElement(const QString &id);
  QJsonObject currentScreen() const;
  QJsonObject currentElement() const;
  void replaceProject(QJsonObject data);
  void updateElement(std::function<void(QJsonObject &)> mutate);
  void addButton();
  void deleteElement();
  void duplicateElement();
  void addElement(const QString &kind, const QString &assetId = {});
  void importImage();
  void importSprites();
  void importFont();
  void editDesignSystem();
  void editScreenSettings();
  void editUserFields();
  void applyTextBindingBatch();
  void editLocalization();
  void editComponents();
  void editDataUi();
  void editTimelines();
  void editBattleHud();
  void editExpeditions();
  void createProgressionActivity();
  void editScripts();
  void runValidation();
  void showAnalysis();
  void addGroup();
  void reparentElement(QString id,QString parentId);
  void rebuildHierarchy();
  QComboBox *parentField;
  QCheckBox *clipChildrenField;
  void editFacilities();
  void editRules();
  void editLogicDocument();
  void editWorkflows();
  void showRuleDebugger();
  QDockWidget *ruleDebugDock=nullptr;
  QDockWidget *analysisDock=nullptr;
  void editComponentInstance();
  void moveLayer(int delta);
  void editContent();
  void editWidgetSettings();
  void refreshContent();
  void openScreen(const QString &id);
  void closeScreen(int index);
  void createScreen(bool duplicate = false);
  void renameScreen();
  void deleteScreen();
  QString managedScreenId() const;
  void updateScreenActions();
  void save();
  void exportGameDialog();
  void play();
  void chooseProject(bool create);
  void newProjectWizard();
  void editProjectSettings();
  void receivePlayer();
  bool mayDiscard();
};
