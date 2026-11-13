#include "canvas-player.hpp"
#include <QtWidgets>
#include <QWindow>

CanvasPlayer::CanvasPlayer(QWidget* viewport):QWidget(viewport){
  setObjectName("canvasLivePlayer");setGeometry(viewport->rect());viewport->installEventFilter(this);
  auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);hide();
  startup.setSingleShot(true);startup.setInterval(20000);
  connect(&startup,&QTimer::timeout,this,[this]{stop();emit failed(tr("캔버스 플레이어 연결 시간이 초과됐습니다."));});
  connect(&process,&QProcess::readyReadStandardOutput,this,&CanvasPlayer::receive);
  connect(&process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart){const auto message=process.errorString();stop();emit failed(message);}});
  connect(&process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus){
    if(stopping)return;const auto error=QString::fromUtf8(process.readAllStandardError()).right(1500);stop();if(code)emit failed(tr("캔버스 재생 실패: %1").arg(error));
  });
}
CanvasPlayer::~CanvasPlayer(){disconnect(this,&CanvasPlayer::stateChanged,nullptr,nullptr);disconnect(this,&CanvasPlayer::failed,nullptr,nullptr);stop();}
bool CanvasPlayer::eventFilter(QObject* target,QEvent* event){if(target==parentWidget()&&event->type()==QEvent::Resize)setGeometry(parentWidget()->rect());return QWidget::eventFilter(target,event);}
void CanvasPlayer::launch(QJsonObject value){
  stop();descriptor=value;buffer.clear();report={};serial=0;pauseRequested=false;
  directory=QFileInfo(value.value("report").toString()).absolutePath()+"/live-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
  if(!QDir().mkpath(directory)){emit failed(tr("임시 재생 폴더를 만들 수 없습니다."));return;}
  QStringList args{"--bundle",value.value("directory").toString(),"--preview-screen",value.value("screenId").toString(),"--preview-inspection",value.value("inspection").toString(),"--editor-live-preview",directory};
#ifdef Q_OS_WIN
  args<<"--hidden";
#endif
  if(!value.value("locale").toString().isEmpty())args<<"--locale"<<value.value("locale").toString();
  process.setWorkingDirectory(value.value("directory").toString());process.start(value.value("executable").toString(),args);startup.start();emit stateChanged();
}
qint64 CanvasPlayer::command(QJsonObject value){
  if(!active())return 0;value["serial"]=++serial;
  QSaveFile file(directory+"/control.json");if(!file.open(QIODevice::WriteOnly)){emit failed(file.errorString());return 0;}file.write(QJsonDocument(value).toJson(QJsonDocument::Compact));if(!file.commit()){emit failed(file.errorString());return 0;}return serial;
}
void CanvasPlayer::pause(bool value){pauseRequested=value;command({{"type","pause"},{"paused",value}});emit stateChanged();}
void CanvasPlayer::reset(){if(!descriptor.isEmpty())launch(descriptor);}
void CanvasPlayer::stop(){
  if(stopping)return;stopping=true;startup.stop();hide();
  if(active()){command({{"type","stop"}});if(!process.waitForFinished(1000)){process.kill();process.waitForFinished(1000);}}
  delete container;container=nullptr;stopping=false;emit stateChanged();
}
void CanvasPlayer::receive(){
  buffer+=process.readAllStandardOutput();
  while(buffer.contains('\n')){
    const auto pos=buffer.indexOf('\n');const auto line=buffer.left(pos);buffer.remove(0,pos+1);const auto value=QJsonDocument::fromJson(line).object().value("editorPreview").toObject();if(value.isEmpty())continue;
    if(value.value("ready").toBool()){
      if(value.contains("embedded") && !value.value("embedded").toBool()){
        container=new QLabel(tr("Live preview is running in a separate player window. Pause, reset and stop remain available here."),this);
        static_cast<QLabel*>(container)->setWordWrap(true);layout()->addWidget(container);show();raise();startup.stop();pause(false);continue;
      }
      auto window=QWindow::fromWinId(WId(value.value("windowHandle").toString().toULongLong()));
      if(!window){stop();emit failed(tr("이 환경에서는 플레이어 창을 캔버스에 연결할 수 없습니다."));return;}
      container=QWidget::createWindowContainer(window,this);container->setFocusPolicy(Qt::StrongFocus);layout()->addWidget(container);setGeometry(parentWidget()->rect());show();raise();container->show();startup.stop();pause(false);container->setFocus();
    }else{report=value;emit stateChanged();}
  }
}
