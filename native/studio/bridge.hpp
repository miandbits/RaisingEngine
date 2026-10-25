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
  bool backgroundBusy() const { return bool(backgroundCallback); }
  void request(const QString &method, QJsonObject params,
               std::function<void(QJsonObject)> done);
  // One passive status request may overlap one exclusive authoring request.
  void requestBackground(const QString &method, QJsonObject params,
                         std::function<void(QJsonObject)> done);
signals:
  void failed(QString message);

private:
  QProcess process;
  QByteArray buffer;
  int serial = 0, foregroundSerial = 0, backgroundSerial = 0;
  std::function<void(QJsonObject)> callback, backgroundCallback;
  void fail(const QString &message);
};
