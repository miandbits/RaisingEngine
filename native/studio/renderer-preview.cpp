#include "renderer-preview.hpp"
#include <QCheckBox>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QPainter>
#include <QSplitter>
#include <QVBoxLayout>
#include <functional>

class RenderImage : public QWidget {
public:
  QImage pixels;
  QJsonObject ui;
  QString selected;
  bool boxes=true;
  std::function<void(QString)> picked;
  RenderImage(QWidget* parent=nullptr):QWidget(parent){setMinimumSize(160,180);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);}
  QRectF imageRect() const {QSizeF size=pixels.size();size.scale(this->size(),Qt::KeepAspectRatio);return QRectF(QPointF((width()-size.width())/2,(height()-size.height())/2),size);}
  QRectF nodeRect(QJsonObject node) const {
    const auto viewport=ui["viewport"].toObject();const double w=viewport["width"].toDouble(pixels.width()),h=viewport["height"].toDouble(pixels.height());
    if(w<=0||h<=0||pixels.isNull())return {};
    const auto presentation=ui["presentation"].toObject();
    const double fit=presentation.isEmpty()?qMin(pixels.width()/w,pixels.height()/h):presentation["width"].toDouble()/w,display=imageRect().width()/pixels.width();
    const QPointF offset=presentation.isEmpty()?QPointF((pixels.width()-w*fit)/2,(pixels.height()-h*fit)/2):QPointF(presentation["x"].toDouble(),presentation["y"].toDouble());
    return QRectF(imageRect().topLeft()+(offset+QPointF(node["x"].toDouble(),node["y"].toDouble())*fit)*display,QSizeF(node["width"].toDouble(),node["height"].toDouble())*fit*display);
  }
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);p.fillRect(rect(),QColor("#171d24"));if(pixels.isNull()){p.setPen(QColor("#a9b8c8"));p.drawText(rect(),Qt::AlignCenter,QString::fromUtf8("실제 렌더링 결과가 여기에 표시됩니다"));return;}
    p.drawImage(imageRect(),pixels);
    for(auto n:ui["nodes"].toArray()){auto node=n.toObject();const auto id=node["id"].toString();const bool chosen=id==selected;if(!chosen&&(!boxes||!node.contains("buttonId")))continue;
      if(!node["rendered"].toBool(true)||!node["visible"].toBool(true))continue;
      p.setBrush(Qt::NoBrush);p.setPen(QPen(chosen?QColor("#ffcd67"):QColor("#67c7ff"),chosen?2:1));p.drawRect(nodeRect(chosen?node:node[node.contains("visibleHitbox")?"visibleHitbox":"hitbox"].toObject()));}
  }
  void mousePressEvent(QMouseEvent* e) override {
    auto array=ui["nodes"].toArray();for(int i=array.size()-1;i>=0;--i){auto node=array[i].toObject();if(!node["rendered"].toBool(true)||!node["visible"].toBool(true))continue;const auto hit=node.contains("visibleHitbox")?node["visibleHitbox"].toObject():node;if(nodeRect(hit).contains(e->position())){selected=node["id"].toString();update();if(picked)picked(selected);return;}}
  }
};

RendererPreviewDock::RendererPreviewDock(QWidget* parent):QDockWidget(tr("실제 렌더러 검사"),parent){
  setObjectName("rendererPreviewDock");auto body=new QWidget;setWidget(body);auto layout=new QVBoxLayout(body);
  auto controls=new QHBoxLayout;resolution=new QComboBox;resolution->setObjectName("rendererResolution");
  resolution->addItem(tr("설계 크기"),QSize());resolution->addItem("1280 × 720",QSize(1280,720));resolution->addItem("1600 × 900",QSize(1600,900));resolution->addItem("720 × 1280",QSize(720,1280));controls->addWidget(resolution);
  width=new QSpinBox;height=new QSpinBox;width->setObjectName("rendererWidth");height->setObjectName("rendererHeight");width->setRange(320,3840);height->setRange(240,2160);width->setValue(1280);height->setValue(720);controls->addWidget(width);controls->addWidget(new QLabel("×"));controls->addWidget(height);
  locale=new QComboBox;locale->setObjectName("rendererLocale");controls->addWidget(locale);
  interaction=new QComboBox;interaction->setObjectName("rendererInteraction");for(auto p:QList<QPair<QString,QString>>{{"기본 상태","normal"},{"마우스 올림","hover"},{"누르는 중","pressed"},{"키보드 초점","focus"},{"비활성 모양","disabled"}})interaction->addItem(p.first,p.second);controls->addWidget(interaction);
  render=new QPushButton(tr("현재 화면 렌더링"));render->setObjectName("renderPreview");controls->addWidget(render);cancel=new QPushButton(tr("취소"));cancel->setEnabled(false);controls->addWidget(cancel);layout->addLayout(controls);
  auto detail=new QHBoxLayout;element=new QComboBox;element->setObjectName("rendererElement");detail->addWidget(new QLabel(tr("검사 대상")));detail->addWidget(element,1);auto boxes=new QCheckBox(tr("버튼 영역 표시"));boxes->setChecked(true);detail->addWidget(boxes);pin=new QPushButton(tr("비교 기준으로 보관"));pin->setObjectName("pinRendererPreview");pin->setEnabled(false);detail->addWidget(pin);layout->addLayout(detail);
  auto configure=new QPushButton(tr("제작 상태 설정"));configure->setObjectName("configureAuthoringPreview");detail->addWidget(configure);connect(configure,&QPushButton::clicked,this,&RendererPreviewDock::editScenario);
  reasons=new QTreeWidget;reasons->setObjectName("previewReasons");reasons->setHeaderLabels({tr("화면·표현 규칙"),tr("선택 이유")});reasons->setColumnWidth(0,240);reasons->setMinimumHeight(120);reasons->setMaximumHeight(180);layout->addWidget(reasons);
  status=new QLabel(tr("선택한 화면을 배포용 렌더러로 확인합니다. 초기 게임 상태를 사용하며, 버튼 클릭은 요소 선택만 합니다."));status->setWordWrap(true);status->setObjectName("rendererStatus");layout->addWidget(status);
  auto split=new QSplitter;auto column=[&](QLabel*& title,RenderImage*& surface){auto w=new QWidget;auto v=new QVBoxLayout(w);v->setContentsMargins(0,0,0,0);title=new QLabel;title->setWordWrap(true);v->addWidget(title);surface=new RenderImage;v->addWidget(surface,1);split->addWidget(w);};column(referenceCaption,reference);column(caption,image);reference->parentWidget()->hide();layout->addWidget(split,1);
  nodes=new QTreeWidget;nodes->setObjectName("rendererDiagnostics");nodes->setHeaderLabels({tr("실행 요소"),tr("영역 x / y / w / h"),tr("검사 결과")});nodes->setRootIsDecorated(false);nodes->setMaximumHeight(180);nodes->header()->setSectionResizeMode(QHeaderView::ResizeToContents);layout->addWidget(nodes);
  connect(render,&QPushButton::clicked,this,&RendererPreviewDock::begin);
  connect(cancel,&QPushButton::clicked,this,[this]{if(process.state()!=QProcess::NotRunning)process.kill();fail(tr("미리보기를 취소했습니다."));});
  connect(pin,&QPushButton::clicked,this,[this]{reference->pixels=image->pixels;reference->ui=image->ui;reference->selected=image->selected;referenceCaption->setText(caption->text());reference->parentWidget()->show();reference->update();});
  connect(boxes,&QCheckBox::toggled,this,[this](bool checked){image->boxes=reference->boxes=checked;image->update();reference->update();});
  connect(resolution,qOverload<int>(&QComboBox::activated),this,[this]{auto size=resolution->currentData().toSize();if(size.isEmpty()){auto v=project["visual"].toObject();size={v["width"].toInt(1280),v["height"].toInt(720)};}width->setValue(size.width());height->setValue(size.height());});
  for(auto field:{width,height})connect(field,qOverload<int>(&QSpinBox::valueChanged),this,[this]{changed();});
  for(auto field:{locale,interaction,element})connect(field,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{changed();});
  auto pick=[this](QString id){image->selected=id;image->update();if(root!=renderedRoot||screen!=renderedScreen||pending)return;const int index=element->findData(id);if(index>=0)element->setCurrentIndex(index);QString owner=id;for(auto n:renderedUi["nodes"].toArray())if(n.toObject()["id"]==id)owner=n.toObject()["ownerId"].toString(id);emit elementPicked(owner);};image->picked=pick;
  connect(reasons,&QTreeWidget::itemClicked,this,[pick](QTreeWidgetItem* row,int){const auto id=row->data(0,Qt::UserRole).toString();if(!id.isEmpty())pick(id);});
  connect(nodes,&QTreeWidget::itemClicked,this,[pick](QTreeWidgetItem* row,int){pick(row->data(0,Qt::UserRole).toString());});
  connect(&process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus){if(pending)complete(code);});
  connect(&process,&QProcess::stateChanged,this,[this](QProcess::ProcessState state){if(state==QProcess::NotRunning&&!pending)render->setEnabled(!screen.isEmpty());});
  connect(&process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){if(e==QProcess::FailedToStart)fail(process.errorString());});
  timeout.setSingleShot(true);connect(&timeout,&QTimer::timeout,this,[this]{process.kill();fail(tr("렌더러 응답 시간이 초과되었습니다."));});
}
RendererPreviewDock::~RendererPreviewDock(){process.disconnect(this);if(process.state()!=QProcess::NotRunning){process.kill();process.waitForFinished(1000);}}
QString RendererPreviewDock::contextKey() const {return root+"|"+QString::number(revision)+"|"+screen+"|"+QString::number(width->value())+"x"+QString::number(height->value())+"|"+locale->currentData().toString()+"|"+element->currentData().toString()+"|"+interaction->currentData().toString()+QString::fromUtf8(QJsonDocument(scenario).toJson(QJsonDocument::Compact));}
void RendererPreviewDock::changed(){if(filling)return;if(!pending&&!renderedContext.isEmpty()&&contextKey()!=renderedContext)status->setText(tr("설정 또는 프로젝트가 변경되었습니다. 표시된 결과는 이전 스냅샷입니다. 다시 렌더링하세요."));image->selected=element->currentData().toString();image->update();}
void RendererPreviewDock::setContext(const QJsonObject& p,QString sid,QString selected,int rev,QString projectRoot,QString preferredLocale){
  filling=true;const bool other=root!=projectRoot;if(other){scenario={};showReasons({});}const bool selectionChanged=other||screen!=sid||contextSelection!=selected;contextSelection=selected;project=p;screen=sid;revision=rev;root=projectRoot;
  auto chosenLocale=other?preferredLocale:locale->currentData().toString();locale->clear();auto config=p["localization"].toObject();for(auto l:config["locales"].toArray())locale->addItem(l.toObject()["label"].toString(),l.toObject()["id"].toString());if(locale->count()==0)locale->addItem(tr("원문"),QString());int index=locale->findData(chosenLocale);if(index<0)index=locale->findData(config["defaultLocale"].toString());locale->setCurrentIndex(qMax(0,index));
  const auto old=element->currentData().toString();element->clear();element->addItem(tr("화면 전체"),QString());auto visual=p["visual"].toObject();
  for(auto s:visual["screens"].toArray())if(s.toObject()["id"]==sid)for(auto e:s.toObject()["elements"].toArray()){auto o=e.toObject();const auto id=o["id"].toString();if(o["kind"]=="component"){element->addItem(o["label"].toString(id)+tr(" · 부품 전체"),id);for(auto c:visual["components"].toArray())if(c.toObject()["id"]==o["componentId"])for(auto child:c.toObject()["elements"].toArray()){auto childId=id+"__"+child.toObject()["id"].toString();element->addItem(childId,childId);}}else element->addItem(o["label"].toString(id),id);}
  if(renderedRoot==root&&renderedScreen==screen)for(auto n:renderedUi["nodes"].toArray()){auto node=n.toObject();const auto id=node["id"].toString();if(node.contains("ownerId")&&element->findData(id)<0){element->addItem(node["label"].toString(id)+" · "+id,id);element->setItemData(element->count()-1,node["ownerId"].toString(),Qt::UserRole+1);}}
  const int previousTarget=element->findData(old);const bool keepChild=previousTarget>=0&&element->itemData(previousTarget,Qt::UserRole+1).toString()==selected;
  int target=element->findData(selectionChanged&&!keepChild?selected:old);if(target<0)target=element->findData(old);element->setCurrentIndex(qMax(0,target));
  if(other&&resolution->currentIndex()==0){width->setValue(visual["width"].toInt(1280));height->setValue(visual["height"].toInt(720));}
  render->setEnabled(!pending&&process.state()==QProcess::NotRunning&&!p.isEmpty()&&!sid.isEmpty());filling=false;changed();
}
void RendererPreviewDock::begin(){
  if(pending||process.state()!=QProcess::NotRunning||screen.isEmpty()||project.isEmpty())return;
  if(interaction->currentData()!="normal"&&element->currentData().toString().isEmpty()){fail(tr("버튼 상태를 검사하려면 검사 대상을 선택하세요."));return;}
  requestedScreen=screen;requestedLocale=locale->currentData().toString();requestedElement=element->currentData().toString();requestedState=interaction->currentData().toString();requestedRevision=revision;requestedWidth=width->value();requestedHeight=height->value();requestContext=contextKey();
  requestedRoot=root;
  requestedScenario={};if(scenario.value("enabled").toBool()){auto value=scenario;value["screenId"]=requestedScreen;value["elementId"]=requestedElement;requestedScenario["authoringContext"]=value;}
  pending=true;render->setEnabled(false);cancel->setEnabled(false);status->setText(tr("저장 전 편집 내용을 임시 스냅샷으로 준비하는 중…"));emit snapshotRequested();
}
void RendererPreviewDock::fail(QString message){timeout.stop();pending=false;render->setEnabled(process.state()==QProcess::NotRunning&&!screen.isEmpty());cancel->setEnabled(false);status->setText(message);emit snapshotReady(false);}
void RendererPreviewDock::launch(QJsonObject descriptor){
  if(!pending)return;
  if(descriptor["revision"].toInt()!=requestedRevision){fail(tr("준비 중 프로젝트 버전이 변경되었습니다. 다시 렌더링하세요."));return;}
  if(descriptor.contains("inspection")){const auto routed=descriptor.value("screenId").toString(requestedScreen);if(routed!=requestedScreen){requestedElement.clear();requestedState="normal";}requestedScreen=routed;}showReasons(descriptor.value("explanation").toObject());
  const auto run=QFileInfo(descriptor["report"].toString()).absolutePath();imageFile=run+"/preview.png";reportFile=run+"/preview-report.json";const auto stepsFile=run+"/preview-steps.json";
  QJsonArray steps{QJsonObject{{"resize",QJsonObject{{"width",requestedWidth},{"height",requestedHeight}}}},QJsonObject{{"waitFrames",8}},QJsonObject{{"checkpoint","preview"}},QJsonObject{{"screenshot",imageFile}}};
  QFile file(stepsFile);if(!file.open(QIODevice::WriteOnly)){fail(file.errorString());return;}file.write(QJsonDocument(steps).toJson());file.close();
  QStringList args{"--bundle",descriptor["directory"].toString(),"--save",descriptor["save"].toString(),"--hidden","--preview-screen",requestedScreen,"--preview-state",requestedState,"--automation",stepsFile,"--report",reportFile};
  if(descriptor.contains("inspection"))args<<"--preview-inspection"<<descriptor.value("inspection").toString();
  if(!descriptor.value("scene").toString().isEmpty())args<<"--preview-scene"<<descriptor.value("scene").toString();
  if(!requestedElement.isEmpty())args<<"--preview-element"<<requestedElement;if(!requestedLocale.isEmpty())args<<"--locale"<<requestedLocale;
  process.setWorkingDirectory(descriptor["directory"].toString());status->setText(tr("배포용 렌더러로 그리는 중…"));cancel->setEnabled(true);timeout.start(60000);process.start(descriptor["executable"].toString(),args);
}
void RendererPreviewDock::complete(int code){
  timeout.stop();QFile file(reportFile);QJsonObject report;if(file.open(QIODevice::ReadOnly))report=QJsonDocument::fromJson(file.readAll()).object();
  if(code!=0||!report["ok"].toBool()){fail(tr("렌더링 실패: ")+report["error"].toString(QString::fromUtf8(process.readAllStandardError())));return;}
  renderedUi={};for(auto step:report["steps"].toArray())if(step.toObject()["command"].toObject()["checkpoint"]=="preview")renderedUi=step.toObject()["ui"].toObject();
  QImage pixels(imageFile);if(pixels.isNull()||renderedUi.isEmpty()){fail(tr("렌더링 결과 이미지 또는 검사 정보가 없습니다."));return;}
  image->pixels=pixels;image->ui=renderedUi;image->selected=requestedElement;image->update();renderedContext=requestContext;
  renderedRoot=requestedRoot;renderedScreen=requestedScreen;
  setContext(project,screen,contextSelection,revision,root,locale->currentData().toString());
  caption->setText(tr("%1 · r%2 · %3×%4 · %5 · %6").arg(requestedScreen).arg(requestedRevision).arg(pixels.width()).arg(pixels.height()).arg(requestedLocale.isEmpty()?tr("원문"):requestedLocale,requestedState));
  nodes->clear();int warnings=0;for(auto value:renderedUi["nodes"].toArray()){
    auto node=value.toObject();QStringList issues;
    if(node["overflowX"].toBool())issues<<tr("가로 넘침");
    if(node["overflowY"].toBool())issues<<tr("세로 넘침");
    if(node["outsideViewport"].toBool())issues<<tr("화면 밖");
    if(node["clipped"].toBool())issues<<tr("잘림");
    if(node["occludedAtCenter"].toBool())issues<<tr("버튼 중앙 가림");
    if(node.contains("buttonId")&&!node["enabled"].toBool(true))issues<<tr("사용 불가");
    if(!node["rendered"].toBool(true)||!node["visible"].toBool(true))issues<<tr("현재 상태에서 미표시");
    if(!issues.isEmpty())++warnings;
    const auto bounds=node["rendered"].toBool(true)?QString("%1 / %2 / %3 / %4").arg(node["x"].toDouble()).arg(node["y"].toDouble()).arg(node["width"].toDouble()).arg(node["height"].toDouble()):QString("—");
    const auto label=node["label"].toString();auto row=new QTreeWidgetItem(nodes,{label.isEmpty()?node["id"].toString():label+" · "+node["id"].toString(),bounds,issues.isEmpty()?tr("영역 내"):issues.join(" · ")});row->setData(0,Qt::UserRole,node["id"].toString());
    row->setToolTip(2,tr("실제 폰트: %1 · %2\n글자색: %3 · 배경색: %4\n텍스트 배치 측정: %5").arg(node["fontFamily"].toString(),node["fontSize"].toString(),node["color"].toString(),node["backgroundColor"].toString(),node["textMeasured"].toBool()?tr("있음"):tr("없음")));
  }
  pending=false;render->setEnabled(!screen.isEmpty());cancel->setEnabled(false);pin->setEnabled(true);status->setText(tr("검사 상태의 실제 렌더링 완료 · 확인 항목 %1개. 게임 행동과 사용자 저장 파일은 변경하지 않습니다.").arg(warnings));if(renderedUi["runtimeButtonsTruncated"].toBool())status->setText(status->text()+tr(" 내부 버튼이 많아 일부 검사 결과를 생략했습니다."));changed();emit snapshotReady(true);
}
