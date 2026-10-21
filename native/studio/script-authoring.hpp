#pragma once
#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <functional>
class QComboBox;
class QListWidget;
class QFormLayout;
class QLabel;
class QLineEdit;
class QGraphicsView;
class QGraphicsScene;
QStringList scriptGraphIssues(const QJsonObject &script);
class ScriptAuthoringDialog final : public QDialog {
public:
  ScriptAuthoringDialog(QJsonObject context,QJsonArray assets,QWidget *parent=nullptr,std::function<QString()> saveGuard={},std::function<void(QJsonObject,std::function<void(QJsonObject)>)> scenePreview={});
  QJsonArray edits() const;
  void selectDocument(const QString &file);
  void setApplyHandler(std::function<void(QJsonArray,std::function<void(QString)>)> handler) { applyEdits = std::move(handler); }
private:
  QJsonObject context;
  QJsonArray original,documents,assets;
  QJsonArray externalOriginal, externalDocuments;
  QWidget *metadata = nullptr;
  QLabel *referenceSummary = nullptr;
  QWidget *makeMetadata();
  void refreshMetadata();
  QStringList externalReferences(QString nodeId, QString choiceId = {}) const;
  void renameExternalReferences(QString from, QString to, QString choiceFrom = {}, QString choiceTo = {});
  int documentIndex=-1,nodeIndex=-1;
  bool filling=false;
  std::function<QString()> saveGuard;
  std::function<void(QJsonArray,std::function<void(QString)>)> applyEdits;
  std::function<void(QJsonObject,std::function<void(QJsonObject)>)> scenePreview;
  QComboBox *documentField,*entryField,*kindField;
  QListWidget *nodeList;
  QWidget *properties;
  QFormLayout *form;
  QLineEdit *idField=nullptr;
  QLabel *error;
  QGraphicsView *graph;
  QGraphicsScene *graphScene;
  QJsonObject data() const;
  QJsonObject node() const;
  void setData(QJsonObject data);
  void setNode(QJsonObject node);
  void loadDocument(int index);
  void refreshNodes(QString selected={});
  void fillProperties();
  void refreshGraph();
  void addNode();
  void insertDialogueAfter();
  void connectPredecessor();
  void addRounds();
  void duplicateNode();
  void removeNode();
  bool commitId();
  void editPortraits();
  void previewScene();
};
