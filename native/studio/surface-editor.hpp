#pragma once
#include <QJsonArray>
#include <QJsonValue>
#include <QWidget>
class QPushButton;

// A compact field with a validated, cancellable editor shared by every authoring surface.
class SurfaceField : public QWidget {
  Q_OBJECT
public:
  explicit SurfaceField(QJsonArray assets,QWidget* parent=nullptr);
  QJsonValue value() const { return current; }
  void setValue(QJsonValue value);
  void editValue() { edit(); }
signals:
  void valueChanged();
private:
  QJsonArray assets;
  QJsonValue current=QJsonValue::Undefined;
  QPushButton* button;
  void edit();
};
