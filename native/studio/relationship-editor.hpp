#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QWidget>
class QTableWidget;
class RelationshipEditor final : public QWidget {
public:
  RelationshipEditor(QJsonObject character, QJsonObject context, QWidget *parent = nullptr);
  QJsonArray value(QString *error) const;
private:
  QJsonObject character, context;
  QTableWidget *table;
  void append(QJsonObject relationship, bool fresh);
};
