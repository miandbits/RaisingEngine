#include "canvas-state.hpp"
#include "canvas.hpp"
#include "renderer-preview.hpp"
#include "canvas-player.hpp"
#include <QtWidgets>

static std::unique_ptr<QSettings> canvasPreferences(){
  const auto file=qEnvironmentVariable("RAISING_EDITOR_SETTINGS");
  if(!file.isEmpty())return std::make_unique<QSettings>(file,QSettings::IniFormat);
  return std::make_unique<QSettings>("RaisingEngine","Raising Studio Qt");
}

CanvasStateBar::CanvasStateBar(ScreenCanvas* view,QMainWindow* window):QWidget(window),canvas(view){
  setObjectName("canvasStateBar");auto rows=new QVBoxLayout(this);rows->setContentsMargins(6,3,6,3);auto layout=new QHBoxLayout;rows->addLayout(layout);
  enabled=new QCheckBox(tr("플레이 상태"));enabled->setObjectName("canvasStateEnabled");layout->addWidget(enabled);
  auto configure=new QPushButton(tr("상태 선택…"));configure->setObjectName("canvasStateConfigure");layout->addWidget(configure);
  actor=new QComboBox;actor->setObjectName("canvasStateActor");actor->setMinimumContentsLength(8);actor->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);layout->addWidget(actor);
  auto retry=new QPushButton(tr("갱신"));retry->setObjectName("canvasStateRefresh");layout->addWidget(retry);
  auto reasons=new QPushButton(tr("값·조건"));reasons->setObjectName("canvasStateReasons");layout->addWidget(reasons);
  status=new QLabel(tr("설계 화면"));status->setObjectName("canvasStateStatus");status->setMinimumWidth(0);status->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);layout->addWidget(status,1);
  preview=new RendererPreviewDock(window);preview->setObjectName("canvasStateRenderer");preview->setWindowTitle(tr("캔버스 · 값과 조건"));preview->setCanvasMode(true);window->addDockWidget(Qt::BottomDockWidgetArea,preview);preview->hide();
  player=new CanvasPlayer(canvas->viewport());auto playback=new QHBoxLayout;rows->addLayout(playback);
  playButton=new QPushButton(tr("재생"));playButton->setObjectName("canvasPlay");playback->addWidget(playButton);
  pauseButton=new QPushButton(tr("일시정지"));pauseButton->setObjectName("canvasPause");playback->addWidget(pauseButton);
  resetButton=new QPushButton(tr("초기화"));resetButton->setObjectName("canvasReset");playback->addWidget(resetButton);
  editButton=new QPushButton(tr("편집으로"));editButton->setObjectName("canvasStop");playback->addWidget(editButton);
  auto note=new QLabel(tr("격리된 재생 · 본편 저장 유지"));note->setMinimumWidth(0);note->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);note->setToolTip(note->text());playback->addWidget(note,1);
  connect(playButton,&QPushButton::clicked,this,[this]{
    if(preparingPlay||preview->running()||(busy&&busy()))return;
    preparingPlay=true;playKey=preview->currentKey();debounce.stop();playButton->setEnabled(false);editButton->setEnabled(true);status->setText(tr("캔버스 재생 준비 중…"));emit playRequested(preview->interactiveRequest());
  });
  connect(pauseButton,&QPushButton::clicked,this,[this]{player->pause(!player->paused());});
  connect(resetButton,&QPushButton::clicked,player,&CanvasPlayer::reset);
  connect(editButton,&QPushButton::clicked,this,[this]{preparingPlay=false;player->stop();changed();});
  connect(player,&CanvasPlayer::failed,this,&CanvasStateBar::failPlay);
  connect(player,&CanvasPlayer::stateChanged,this,[this]{
    const bool active=player->active();playButton->setEnabled(!active&&!preparingPlay);pauseButton->setEnabled(active);resetButton->setEnabled(active);editButton->setEnabled(active||preparingPlay);pauseButton->setText(player->paused()?tr("계속 재생"):tr("일시정지"));
    if(active)status->setText(player->paused()?tr("일시정지 · 입력과 소리 멈춤"):tr("캔버스 재생 중 · 임시 상태"));else if(!preparingPlay)changed();
  });
  pauseButton->setEnabled(false);resetButton->setEnabled(false);editButton->setEnabled(false);
  debounce.setSingleShot(true);debounce.setInterval(350);
  connect(&debounce,&QTimer::timeout,this,&CanvasStateBar::refresh);
  connect(preview,&RendererPreviewDock::snapshotRequested,this,&CanvasStateBar::snapshotRequested);
  connect(preview,&RendererPreviewDock::contextChanged,this,&CanvasStateBar::changed);
  connect(enabled,&QCheckBox::toggled,this,[this]{if(!filling&&!enabled->isChecked()){preparingPlay=false;player->stop();}remember();changed();});
  connect(configure,&QPushButton::clicked,this,[this]{preview->configureScenario();const auto settings=preview->scenarioSettings();if(settings.value("enabled").toBool())enabled->setChecked(true);const QSignalBlocker block(actor);actor->setCurrentIndex(qMax(0,actor->findData(settings.value("actorId").toString())));remember();changed();});
  connect(actor,qOverload<int>(&QComboBox::activated),this,[this]{auto settings=preview->scenarioSettings();settings["enabled"]=true;settings["actorId"]=actor->currentData().toString();preview->setScenarioSettings(settings);enabled->setChecked(true);remember();});
  connect(retry,&QPushButton::clicked,this,[this]{enabled->setChecked(true);canvas->clearRuntimePreview();status->setText(tr("플레이 상태 갱신 중…"));debounce.start();});
  connect(reasons,&QPushButton::clicked,this,[this]{preview->show();preview->raise();});
  connect(preview,&RendererPreviewDock::snapshotReady,this,[this](bool ok){
    if(!enabled->isChecked())return;
    if(key!=preview->currentKey()||(!preview->snapshotCurrent()&&ok)){changed();return;}
    if(ok)applySnapshot();
    else {canvas->clearRuntimePreview();status->setText(tr("상태 적용 실패 · 설계 화면"));}
    if(!ok)status->setToolTip(preview->message());
  });
}
void CanvasStateBar::remember(){
  if(filling||root.isEmpty())return;
  auto settings=canvasPreferences();const auto prefix="canvasPreview/"+QString(QCryptographicHash::hash(root.toUtf8(),QCryptographicHash::Sha256).toHex());
  settings->setValue(prefix+"/enabled",enabled->isChecked());settings->setValue(prefix+"/scenario",QJsonDocument(preview->scenarioSettings()).toJson(QJsonDocument::Compact));
}
void CanvasStateBar::applySnapshot(){
  canvas->setRuntimePreview(preview->result(),preview->snapshotImage());
  status->setText(preview->explanation().value("modified").toBool()?tr("가상 검사 상태 · 배치 편집 가능"):tr("플레이 상태 적용 · 배치 편집 가능"));
  status->setToolTip(preview->explanation().value("notice").toString());
}
void CanvasStateBar::updateContext(QJsonObject view,QString screen,QString locale,QSize viewport){
  filling=true;QJsonObject project;for(auto value:view.value("documents").toArray())if(value.toObject().value("kind")=="project")project=value.toObject().value("data").toObject();
  const bool other=root!=view.value("root").toString();root=view.value("root").toString();
  preview->setDocuments(view.value("documents").toArray());preview->setContext(project,screen,{},view.value("revision").toInt(),root,locale);
  preview->setCanvasViewport(viewport);
  if(other){auto settings=canvasPreferences();const auto prefix="canvasPreview/"+QString(QCryptographicHash::hash(root.toUtf8(),QCryptographicHash::Sha256).toHex());preview->setScenarioSettings(QJsonDocument::fromJson(settings->value(prefix+"/scenario").toByteArray()).object());enabled->setChecked(settings->value(prefix+"/enabled",false).toBool());attempted.clear();}
  const auto selected=preview->scenarioSettings().value("actorId").toString();actor->clear();actor->addItem(tr("기존 대상 유지"),"");
  QSet<QString> members;for(auto member:project.value("roster").toObject().value("members").toArray())members.insert(member.toObject().value("characterId").toString());
  for(auto value:view.value("documents").toArray()){const auto doc=value.toObject(),data=doc.value("data").toObject();if(doc.value("kind")=="character"&&members.contains(data.value("id").toString()))actor->addItem(data.value("name").toString(),data.value("id").toString());}
  actor->setCurrentIndex(qMax(0,actor->findData(selected)));setEnabled(!screen.isEmpty());filling=false;changed();
}
void CanvasStateBar::changed(){
  if(filling)return;
  const auto next=preview->currentKey();
  if(player->active()||preparingPlay){if(playKey==next)return;preparingPlay=false;player->stop();}
  if(!enabled->isChecked()){debounce.stop();canvas->clearRuntimePreview();status->setText(tr("설계 화면"));return;}
  if(next!=key){canvas->clearRuntimePreview();key=next;attempted.clear();}
  if(preview->snapshotCurrent()){applySnapshot();return;}
  if(attempted!=key){status->setText(tr("플레이 상태 갱신 중…"));debounce.start();}
}
void CanvasStateBar::refresh(){
  if(!enabled->isChecked()||!isEnabled()||player->active()||preparingPlay)return;
  if(preview->running()||(busy&&busy())){debounce.start();return;}
  attempted=key;preview->begin();
}
void CanvasStateBar::launchPlay(QJsonObject descriptor){
  if(!preparingPlay)return;preparingPlay=false;
  if(playKey!=preview->currentKey()){failPlay(tr("재생 준비 중 편집 상태가 변경되었습니다. 다시 재생하세요."));return;}
  player->launch(descriptor);
}
void CanvasStateBar::failPlay(QString message){preparingPlay=false;player->stop();playButton->setEnabled(true);status->setText(tr("캔버스 재생 실패"));status->setToolTip(message);}
