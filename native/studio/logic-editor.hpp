#pragma once
#include <QDialog>
#include <QJsonObject>
#include <QJsonArray>
#include <QTreeWidget>
#include <QFormLayout>
#include <QLabel>
QJsonValue logicValueAt(QJsonValue value,const QStringList &path);
QJsonValue logicReplaceAt(QJsonValue value,const QStringList &path,QJsonValue next);

// A visual editor for the same typed AST consumed by validation and both runtimes.
class LogicEditor : public QDialog {
public:
  LogicEditor(QJsonValue value, QString kind, QJsonObject context, QWidget *parent=nullptr);
  QJsonValue value() const { return root; }
private:
  QJsonValue root;
  QString rootKind;
  QJsonObject context;
  QTreeWidget *tree;
  QWidget *properties;
  QFormLayout *form;
  QLabel *error;
  QStringList selectedPath;
  QString selectedKind;
  QJsonValue node(const QStringList &path) const;
  void update(const QStringList &path,QJsonValue value);
  void refresh(const QStringList &select={});
  void addTree(QTreeWidgetItem *parent,QString label,QJsonValue value,QString kind,QStringList path);
  void editSelected();
  QJsonObject defaults(QString kind,QString op) const;
  QJsonArray references(QString kind,QString character={}) const;
  void field(QString key,QString label,QStringList values,QStringList labels={});
  void reference(QString key,QString label,QString kind,QString character={});
  void integer(QString key,QString label);
  void boolean(QString key,QString label);
  void ownerField();
  void property(QString key,QJsonValue value);
  void propertyAt(const QStringList &path,QString key,QJsonValue value);
  void changeType(QString op);
  QString validationError(QJsonValue value,QString kind,QString path, int depth=0) const;
  void addChild();
  void removeChild();
  void moveChild(int offset);
};
