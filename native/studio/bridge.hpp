#pragma once
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <functional>

class StudioBridge : public QObject {
  Q_OBJECT
public:
  explicit StudioBridge(const QString &engineRoot, QObject *parent = nullptr);
  ~StudioBridge() override;
  bool busy() const { return bool(callback); }
  void request(const QString &method, QJsonObject params,
               std::function<void(QJsonObject)> done);
signals:
  void failed(QString message);

private:
  QProcess process;
  QByteArray buffer;
  int serial = 0;
  std::function<void(QJsonObject)> callback;
  void fail(const QString &message);
};
