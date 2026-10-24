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
#include <QKeySequence>
#include <QPlainTextEdit>
#include <QPointer>
#include <QSpinBox>
#include <QTabBar>
#include <QTabWidget>
#include <QTreeWidget>
class RendererPreviewDock;
class CanvasStateBar;
class QPushButton;
class QDockWidget;
class QMenu;
class QStackedWidget;
class QActionGroup;
class StudioPluginManager;
namespace raising { struct StudioWorkspace; }

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
  QStringList selectedIds() const { return selectedElements; }
  QString playAutomation, playReport;
  QString lastRecording, recordingRoot;
  QJsonObject lastGoalReport;
  QJsonObject lastGoalImpact;
  QString lastError;
  /** Automation: package file used by 패키지 설치… instead of the file dialog. */
  QString pluginPackagePath;
  StudioPluginManager *pluginManager() const { return plugins; }
  /** Mirrors a service `plugins` object; with load=true also (un)loads authoring DLLs. */
  void applyPluginState(const QJsonObject &state, bool load = true);
  QDockWidget *addStudioDock(const QString &title, const QString &name, QWidget *child, Qt::DockWidgetArea area);
  /**
   * Full-window work areas (화면 편집 = empty key, plugin ones = "<plugin>/<id>"). Switching
   * only changes presentation: docks + screen editor are hidden and their exact layout
   * (saveState) comes back on return; menus, undo/redo, save and play stay. Never an edit.
   */
  bool showWorkspace(const QString &key);
  QString currentWorkspace() const { return activeWorkspace; }
  QStringList workspaceKeys() const;
signals:
  void updated();
  void playerFinished(int code);
  void goalReportReady(QJsonObject report);
  void goalImpactReady(QJsonObject report);
  void followAuthoringItem(QString key);

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
  struct WorkspaceEntry { QString key, plugin, title; QPointer<QWidget> page; QKeySequence shortcut; };
  QList<WorkspaceEntry> workspaces;
  QStackedWidget *centralStack;
  QWidget *screenEditor;
  QTabBar *workspaceSwitcher;
  QMenu *workspaceMenu;
  QActionGroup *workspaceGroup;
  QList<QAction *> screenOnlyActions;  // toolbar entries that only act on the hidden screen editor
  QString activeWorkspace, wantedWorkspace, wantedWorkspaceRoot;
  QByteArray screenLayout;             // dock/toolbar state captured when the screen editor was left
  void setupWorkspaceUi();
  void rebuildWorkspaceUi();
  QWidget *addWorkspace(const QString &plugin, const raising::StudioWorkspace &workspace);
  void removeWorkspaces(const QString &plugin);
  QByteArray screenDockState() { return activeWorkspace.isEmpty() ? saveState(1) : screenLayout; }
  QString engineRoot, screenId = "home", selection, projectFile;
  QStringList selectedElements;
  QStringList openScreens;
  StudioBridge bridge;
  QJsonObject view;
  QJsonObject conditionPreview;
  void editConditionPreview();
  QJsonArray assets;
  QProcess player;
  QByteArray playerBuffer;
  QJsonObject previousState;
  bool filling = false;
  int playRevision = 0;
  ScreenCanvas *canvas;
  RendererPreviewDock *rendererPreview;
  CanvasStateBar *canvasState=nullptr;
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
  QComboBox *imageField, *bindingField, *attributeField, *relationshipField, *visibilityField, *userFieldField;
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
  void selectElements(const QStringList &ids);
  void editSelectedElements();
  void moveSelectedElements(QPoint delta);
  void replaceScreenElements(const QJsonArray &elements);
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
  void openComponentSource();
  QString componentToOpen;
  void editDataUi();
  void editTimelines();
  void editBattleHud();
  void editExpeditions();
  void createProgressionActivity();
  void editScripts();
  void runValidation();
  void validateForRepair(std::function<void(QString)> done);
  void openValidationLocation(QJsonObject diagnostic, int revision, QString root);
  void showAnalysis();
  void addGroup();
  void reparentElement(QString id,QString parentId);
  void rebuildHierarchy();
  QComboBox *parentField;
  QCheckBox *clipChildrenField;
  void editFacilities();
  void editRules();
  void editRuleExtensions();
  void editRoster();
  void editCharacterState();
  void editTimeSessions();
  void editLogicDocument();
  void editWorkflows();
  void showRuleDebugger();
  QWidget *makeRuleDebuggerPanel();
  void showAuthoringWorkspace();
  void followSelectedElement();
  void openScriptDocument(QString file, QString nodeId = {});
  void editUiComposition();
  void editTactics();
  void editDirection();
  void organizeContent();
  void createBenchmark();
  void runAssuranceDialog();
  void goalRoutesDialog();
  void goalSearchDialog(QJsonObject source = {});
  void goalCompareDialog(QString jobId = {}, QJsonObject impactLink = {});
  void goalImpactDialog(QJsonObject report = {});
  void goalCiDialog();
  void acceptGoalImpact(QJsonObject report);
  void acceptGoalReport(QJsonObject report);
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
  QDockWidget *firstGameDock = nullptr;
  QJsonObject firstGameState, releaseCompatibility, recoveryState;
  void showDraftRecovery();
  QSet<QString> firstGameObserved;
  void showFirstGameChecklist();
  void updateFirstGameChecklist();
  void observeFirstGamePlay(const QJsonObject &state);
  void showGettingStarted();
  void editStoryFlow();
  void initializeStarterScreens();
  void addRelationshipDisplay();
  void editScreenRouting();
  void editDataBinding();
  void editPresentationVariants();
  void editElementPresentationVariants(QJsonObject original,QWidget* parent,std::function<bool(QJsonArray,QString&)> apply);
  void editCustomStates();
  void editProjectSettings();
  void receivePlayer();
  bool mayDiscard();
  bool waitForPendingEdits();
  bool confirmingTransition = false;
  void setupBasicEditing(QMenu *edit);
  void copyElements();
  void pasteElements();
  void updateClipboardActions();
  void showHierarchy();
  void showVersionControl();
};
