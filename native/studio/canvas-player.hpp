#pragma once
#include <QWidget>
#include <QProcess>
#include <QJsonObject>
#include <QTimer>
class QWindow;

// A foreign native player window inside the canvas; editing and simulation stay separate.
class CanvasPlayer : public QWidget {
  Q_OBJECT
public:
  explicit CanvasPlayer(QWidget* viewport);
  ~CanvasPlayer() override;
  void launch(QJsonObject descriptor);
  void stop();
  void reset();
  void pause(bool value);
  bool active() const {return process.state()!=QProcess::NotRunning;}
  bool paused() const {return pauseRequested;}
  QJsonObject lastReport() const {return report;}
  QString sessionDirectory() const {return directory;}
  qint64 command(QJsonObject value);
signals:
  void stateChanged();
  void failed(QString message);
protected:
  bool eventFilter(QObject*,QEvent*) override;
private:
  QProcess process;
  QTimer startup;
  QByteArray buffer;
  QJsonObject descriptor,report;
  QWidget* container=nullptr;
  QString directory;
  qint64 serial=0;
  bool stopping=false,pauseRequested=false;
  void receive();
};
