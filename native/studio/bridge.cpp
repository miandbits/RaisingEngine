#include "bridge.hpp"
#include "editor-language.hpp"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTimer>

StudioBridge::StudioBridge(const QString &engineRoot, QObject *parent)
    : QObject(parent) {
  connect(&process, &QProcess::readyReadStandardOutput, this, [this] {
    buffer += process.readAllStandardOutput();
    int at;
    while ((at = buffer.indexOf('\n')) >= 0) {
      auto line = buffer.left(at);
      buffer.remove(0, at + 1);
      auto doc = QJsonDocument::fromJson(line);
      if (!doc.isObject()) {
        fail(tr("프로젝트 서비스 응답을 읽을 수 없습니다."));
        continue;
      }
      auto reply = localizeEditorReply(doc.object());
      const auto id=reply.value("id").toInt();
      if(backgroundCallback&&id==backgroundSerial){auto done=std::move(backgroundCallback);backgroundCallback=nullptr;done(reply);}
      else if(callback&&id==foregroundSerial){auto done=std::move(callback);callback=nullptr;done(reply);}
    }
  });
  connect(&process, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError) { fail(process.errorString()); });
  connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
          this, [this](int, QProcess::ExitStatus) {
            fail(tr("프로젝트 서비스가 종료되었습니다. 편집기를 다시 열어 "
                    "주세요.\n") +
                 QString::fromUtf8(process.readAllStandardError()));
          });
  process.setWorkingDirectory(engineRoot);
  const auto bundledNode=QDir(engineRoot).filePath(
#ifdef Q_OS_WIN
      "node.exe"
#else
      "node"
#endif
  );
  process.start(qEnvironmentVariable("RAISING_NODE", QFileInfo::exists(bundledNode)?bundledNode:"node"),
                {QDir(engineRoot).filePath("apps/qt-studio/service.mjs")});
}
StudioBridge::~StudioBridge() { shutdown(); }
void StudioBridge::shutdown() {
  if(shuttingDown)return;
  shuttingDown=true;callback=nullptr;backgroundCallback=nullptr;
  process.disconnect(this);
  process.closeWriteChannel();
  // Allow owned workers to observe EOF, cancel, and retire their process tree.
  // The service has a bounded 2 s cancellation grace plus Windows taskkill (5 s).
  if (!process.waitForFinished(10000)) {
    process.kill();
    process.waitForFinished(1500);
  }
}
void StudioBridge::fail(const QString &message) {
  auto done = std::move(callback), backgroundDone=std::move(backgroundCallback);
  callback = nullptr;backgroundCallback=nullptr;
  if (done)done({{"error", message}});
  if (backgroundDone)backgroundDone({{"error", message}});
  emit failed(message);
}
void StudioBridge::request(const QString &method, QJsonObject params,
                           std::function<void(QJsonObject)> done) {
  if (busy()) {
    done({{"error", tr("이전 작업이 끝난 뒤 다시 시도해 주세요.")}});
    return;
  }
  if (process.state() == QProcess::NotRunning) {
    done({{"error", tr("프로젝트 서비스가 실행되지 않았습니다.")}});
    return;
  }
  callback = std::move(done);
  foregroundSerial=++serial;
  process.write(
      QJsonDocument(
          QJsonObject{{"id", foregroundSerial}, {"method", method}, {"params", params}})
          .toJson(QJsonDocument::Compact) +
      '\n');
}

void StudioBridge::requestBackground(const QString &method,QJsonObject params,std::function<void(QJsonObject)> done){
  if(backgroundBusy()){done({{"error",tr("이전 작업이 끝난 뒤 다시 시도해 주세요.")}});return;}
  if(process.state()==QProcess::NotRunning){done({{"error",tr("프로젝트 서비스가 실행되지 않았습니다.")}});return;}
  backgroundCallback=std::move(done);backgroundSerial=++serial;
  process.write(QJsonDocument(QJsonObject{{"id",backgroundSerial},{"method",method},{"params",params}}).toJson(QJsonDocument::Compact)+'\n');
}
