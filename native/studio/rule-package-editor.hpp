#pragma once
#include <QDialog>
#include <QJsonObject>
#include <QTreeWidget>
#include <QFormLayout>
#include <QLabel>

// Edits a detached package. Installation and project undo remain the caller's job.
class RulePackageEditor : public QDialog {
public:
  RulePackageEditor(QJsonObject package, QJsonObject context, QWidget *parent=nullptr);
  QJsonObject value() const { return package; }
  static QString validate(const QJsonObject &package);
private:
  QJsonObject package, context;
  QTreeWidget *tree;
  QFormLayout *form;
  QLabel *status;
  QStringList selected;
  void refresh(QStringList path={});
  void details();
  void put(QStringList path,QJsonValue value);
  void add(QString kind);
  void removeSelected();
  void editBody(QStringList path,QString kind,QStringList definitionPath);
  void editParameters(QStringList definitionPath);
  QJsonObject editorContext() const;
};
