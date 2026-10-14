#include "timeline-stage.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <functional>

QString timelineLayerFrame(const QJsonObject &layer, qint64 elapsedMs) {
  const auto frames=layer.value("frames").toArray();
  qint64 total=0;for(auto f:frames)total+=qMax(1,f.toObject().value("durationMs").toInt(1));
  if(total==0)return {};
  auto time=qMax<qint64>(0,elapsedMs);
  if(layer.value("loop").toBool(false))time%=total;else time=qMin(time,total-1);
  for(auto f:frames){auto frame=f.toObject();const int duration=qMax(1,frame.value("durationMs").toInt(1));
    if(time<duration)return frame.value("imageAssetId").toString();time-=duration;}
  return {};
}
static QRectF stageRect(const QRectF &area,const QJsonObject &stage){
  QSizeF size(qMax(1e-12,stage.value("width").toDouble(320)),qMax(1e-12,stage.value("height").toDouble(180)));
  size.scale(area.size(),Qt::KeepAspectRatio);return QRectF(area.center()-QPointF(size.width()/2,size.height()/2),size);
}
void drawTimelineStage(QPainter &p,const QRectF &area,const QJsonObject &stage,
                       const QMap<QString,QPixmap> &images,qint64 elapsedMs){
  const auto target=stageRect(area,stage);p.save();p.setClipRect(target);
  p.translate(target.topLeft());p.scale(target.width()/qMax(1e-12,stage.value("width").toDouble()),target.height()/qMax(1e-12,stage.value("height").toDouble()));
  p.setRenderHint(QPainter::SmoothPixmapTransform,false);
  for(auto v:stage.value("layers").toArray()){auto l=v.toObject();auto img=images.value(timelineLayerFrame(l,elapsedMs));
    if(!img.isNull())p.drawPixmap(QRectF(l.value("x").toDouble(),l.value("y").toDouble(),l.value("width").toDouble(),l.value("height").toDouble()),img,QRectF(img.rect()));}
  p.restore();
}
namespace {
class StageCanvas final:public QWidget {
public:
  QJsonObject stage;QMap<QString,QPixmap> images;int elapsed=0,selected=-1;
  std::function<void(int)> selectLayer;
  std::function<void(QPointF)> moveLayer;
  explicit StageCanvas(QWidget *p=nullptr):QWidget(p){setMinimumSize(400,240);setObjectName("stageCanvas");}
protected:
  QPointF start,origin;bool dragging=false;
  QPointF logical(QPointF point)const{auto r=stageRect(rect(),stage);return QPointF((point.x()-r.x())*stage.value("width").toDouble()/r.width(),(point.y()-r.y())*stage.value("height").toDouble()/r.height());}
  void paintEvent(QPaintEvent*)override{QPainter p(this);p.fillRect(rect(),QColor("#15242c"));drawTimelineStage(p,rect(),stage,images,elapsed);
    auto layers=stage.value("layers").toArray();if(selected>=0&&selected<layers.size()){auto l=layers[selected].toObject();auto r=stageRect(rect(),stage);p.translate(r.topLeft());p.scale(r.width()/stage.value("width").toDouble(),r.height()/stage.value("height").toDouble());QPen pen(QColor("#ffe09b"));pen.setCosmetic(true);pen.setWidth(2);p.setPen(pen);p.drawRect(QRectF(l.value("x").toDouble(),l.value("y").toDouble(),l.value("width").toDouble(),l.value("height").toDouble()));}}
  void mousePressEvent(QMouseEvent *e)override{if(e->button()!=Qt::LeftButton)return;auto pt=logical(e->position());auto layers=stage.value("layers").toArray();for(int i=layers.size()-1;i>=0;--i){auto l=layers[i].toObject();if(QRectF(l.value("x").toDouble(),l.value("y").toDouble(),l.value("width").toDouble(),l.value("height").toDouble()).contains(pt)){selected=i;start=pt;origin={l.value("x").toDouble(),l.value("y").toDouble()};dragging=true;if(selectLayer)selectLayer(i);update();break;}}}
  void mouseMoveEvent(QMouseEvent *e)override{if(dragging&&moveLayer)moveLayer(origin+logical(e->position())-start);}
  void mouseReleaseEvent(QMouseEvent*)override{dragging=false;}
};
}

bool editTimelineStage(QWidget *parent,QJsonObject &stage,const QJsonArray &assets,int durationMs){
  QDialog dialog(parent);dialog.setObjectName("timelineStageDialog");dialog.setWindowTitle(QObject::tr("합성 장면 · 배경과 프레임 애니메이션"));dialog.resize(1140,800);
  auto outer=new QVBoxLayout(&dialog);auto help=new QLabel(QObject::tr("아래 레이어가 앞에 그려집니다. 배경은 프레임 한 장, 애니메이션은 여러 프레임으로 만드세요. 캔버스에서 드래그해 배치할 수 있습니다."));help->setWordWrap(true);outer->addWidget(help);
  auto sizes=new QHBoxLayout;outer->addLayout(sizes);auto width=new QDoubleSpinBox,height=new QDoubleSpinBox;
  for(auto p:QList<QPair<QDoubleSpinBox*,QString>>{{width,"stageWidth"},{height,"stageHeight"}}){p.first->setObjectName(p.second);p.first->setRange(1,8192);p.first->setDecimals(2);}
  sizes->addWidget(new QLabel(QObject::tr("장면 크기")));sizes->addWidget(width);sizes->addWidget(height);sizes->addStretch();
  width->setValue(stage.value("width").toDouble(320));height->setValue(stage.value("height").toDouble(180));
  auto body=new QHBoxLayout;outer->addLayout(body,1);auto left=new QVBoxLayout;body->addLayout(left);auto library=new QListWidget;library->setObjectName("stageLayers");library->setMaximumWidth(200);left->addWidget(library,1);
  auto button=[&](QBoxLayout *layout,QString id,QString text){auto b=new QPushButton(text);b->setObjectName(id);layout->addWidget(b);return b;};
  auto addLayer=button(left,"stageAddLayer",QObject::tr("+ 레이어")),deleteLayer=button(left,"stageRemoveLayer",QObject::tr("레이어 삭제")),up=button(left,"stageLayerUp",QObject::tr("뒤로")),down=button(left,"stageLayerDown",QObject::tr("앞으로"));
  auto center=new QVBoxLayout;body->addLayout(center,1);auto canvas=new StageCanvas;center->addWidget(canvas,1);for(auto a:assets){auto o=a.toObject();if(o.value("kind")=="image")canvas->images[o.value("id").toString()]=QPixmap(o.value("absolutePath").toString());}
  auto scrub=new QSlider(Qt::Horizontal);scrub->setObjectName("stageScrub");scrub->setRange(0,qMax(1,durationMs));center->addWidget(scrub);auto play=button(center,"stagePlay",QObject::tr("▶ 재생 / 일시정지"));
  auto form=new QFormLayout;center->addLayout(form);auto loop=new QCheckBox(QObject::tr("프레임 순서 반복"));loop->setObjectName("stageLoop");form->addRow(loop);
  QDoubleSpinBox *coords[4];QStringList keys{"x","y","width","height"},names{QObject::tr("X"),QObject::tr("Y"),QObject::tr("너비"),QObject::tr("높이")};auto row=new QHBoxLayout;form->addRow(row);
  for(int i=0;i<4;i++){coords[i]=new QDoubleSpinBox;coords[i]->setObjectName("stageLayer"+keys[i]);coords[i]->setDecimals(2);coords[i]->setRange(i<2?-1000000:0.01,1000000);row->addWidget(new QLabel(names[i]));row->addWidget(coords[i]);}
  auto frames=new QTableWidget(0,2);frames->setObjectName("stageFrames");frames->setHorizontalHeaderLabels({QObject::tr("이미지 프레임"),QObject::tr("시간 ms")});frames->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);frames->setMaximumHeight(230);center->addWidget(frames);
  auto frameButtons=new QHBoxLayout;center->addLayout(frameButtons);auto addFrames=button(frameButtons,"stageAddFrames",QObject::tr("+ 에셋에서 프레임")),removeFrame=button(frameButtons,"stageRemoveFrame",QObject::tr("프레임 삭제")),frameUp=button(frameButtons,"stageFrameUp",QObject::tr("↑")),frameDown=button(frameButtons,"stageFrameDown",QObject::tr("↓"));
  auto error=new QLabel;error->setObjectName("stageError");error->setStyleSheet("color:#e68f77");outer->addWidget(error);auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);outer->addWidget(buttons);bool cleared=false;auto clearStage=buttons->addButton(QObject::tr("합성 제거"),QDialogButtonBox::DestructiveRole);clearStage->setObjectName("stageClear");QObject::connect(clearStage,&QPushButton::clicked,&dialog,[&]{cleared=true;dialog.accept();});
  const double initialWidth=width->value(),initialHeight=height->value();double coordinateBaseline[4]{};
  QJsonArray layers=stage.value("layers").toArray();int selected=-1;bool filling=false;QTimer timer;timer.setInterval(16);QElapsedTimer elapsed;
  auto collectFrames=[&]{QJsonArray values;for(int r=0;r<frames->rowCount();r++)values.append(QJsonObject{{"imageAssetId",qobject_cast<QComboBox*>(frames->cellWidget(r,0))->currentData().toString()},{"durationMs",qobject_cast<QSpinBox*>(frames->cellWidget(r,1))->value()}});return values;};
  auto currentStage=[&]{return QJsonObject{{"width",width->value()==initialWidth?stage.value("width").toDouble(width->value()):width->value()},{"height",height->value()==initialHeight?stage.value("height").toDouble(height->value()):height->value()},{"layers",layers}};};
  auto refresh=[&]{canvas->stage=currentStage();canvas->selected=selected;canvas->elapsed=scrub->value();canvas->update();};
  auto commit=[&]{if(filling||selected<0||selected>=layers.size())return;auto l=layers[selected].toObject();for(int i=0;i<4;i++)if(!l.contains(keys[i])||coords[i]->value()!=coordinateBaseline[i]){l[keys[i]]=coords[i]->value();coordinateBaseline[i]=coords[i]->value();}if(l.contains("loop")||loop->isChecked())l["loop"]=loop->isChecked();l["frames"]=collectFrames();layers[selected]=l;refresh();};
  auto fillFrames=[&](QJsonArray values){filling=true;frames->setRowCount(0);for(auto v:values){auto f=v.toObject();int r=frames->rowCount();frames->insertRow(r);auto image=new QComboBox;for(auto a:assets){auto o=a.toObject();if(o.value("kind")=="image")image->addItem(o.value("id").toString(),o.value("id").toString());}auto id=f.value("imageAssetId").toString();if(image->findData(id)<0)image->addItem(QObject::tr("누락: ")+id,id);image->setCurrentIndex(image->findData(id));frames->setCellWidget(r,0,image);auto ms=new QSpinBox;ms->setRange(1,60000);ms->setValue(f.value("durationMs").toInt(120));frames->setCellWidget(r,1,ms);QObject::connect(image,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{commit();});QObject::connect(ms,qOverload<int>(&QSpinBox::valueChanged),&dialog,[&]{commit();});}filling=false;};
  auto load=[&](int index){selected=index;filling=true;auto l=index>=0?layers[index].toObject():QJsonObject{};for(int i=0;i<4;i++){coords[i]->setEnabled(index>=0);coords[i]->setValue(l.value(keys[i]).toDouble(i<2?0:32));coordinateBaseline[i]=coords[i]->value();}loop->setChecked(l.value("loop").toBool());loop->setEnabled(index>=0);filling=false;fillFrames(l.value("frames").toArray());addFrames->setEnabled(index>=0);refresh();};
  auto refill=[&](int index){QSignalBlocker block(library);library->clear();for(auto l:layers)library->addItem(l.toObject().value("id").toString());library->setCurrentRow(index);load(index);};
  QObject::connect(library,&QListWidget::currentRowChanged,&dialog,[&](int r){commit();load(r);});
  for(auto c:coords)QObject::connect(c,qOverload<double>(&QDoubleSpinBox::valueChanged),&dialog,[&]{commit();});QObject::connect(loop,&QCheckBox::toggled,&dialog,[&]{commit();});
  for(auto s:{width,height})QObject::connect(s,qOverload<double>(&QDoubleSpinBox::valueChanged),&dialog,[&]{refresh();});
  canvas->selectLayer=[&](int i){library->setCurrentRow(i);};canvas->moveLayer=[&](QPointF p){coords[0]->setValue(p.x());coords[1]->setValue(p.y());};
  QObject::connect(addLayer,&QPushButton::clicked,&dialog,[&]{commit();if(layers.size()>=32){error->setText(QObject::tr("레이어는 최대 32개입니다."));return;}QSet<QString> ids;for(auto l:layers)ids.insert(l.toObject().value("id").toString());int n=1;while(ids.contains("layer-"+QString::number(n)))n++;layers.append(QJsonObject{{"id","layer-"+QString::number(n)},{"x",0},{"y",0},{"width",width->value()},{"height",height->value()},{"frames",QJsonArray{}},{"loop",false}});refill(layers.size()-1);});
  QObject::connect(deleteLayer,&QPushButton::clicked,&dialog,[&]{if(selected<0)return;layers.removeAt(selected);refill(qMin(selected,int(layers.size())-1));});
  auto moveLayer=[&](int delta){int next=selected+delta;if(selected<0||next<0||next>=layers.size())return;commit();auto l=layers.takeAt(selected);layers.insert(next,l);refill(next);};QObject::connect(up,&QPushButton::clicked,&dialog,[&]{moveLayer(-1);});QObject::connect(down,&QPushButton::clicked,&dialog,[&]{moveLayer(1);});
  QObject::connect(addFrames,&QPushButton::clicked,&dialog,[&]{if(selected<0)return;QDialog pick(&dialog);pick.setObjectName("stageFramePicker");pick.setWindowTitle(QObject::tr("프레임 에셋 선택 · 표시 순서로 추가"));pick.resize(500,600);auto box=new QVBoxLayout(&pick);auto search=new QLineEdit;search->setPlaceholderText(QObject::tr("이름으로 찾기"));search->setObjectName("stageFrameSearch");box->addWidget(search);auto list=new QListWidget;list->setObjectName("stageFrameAssets");list->setSelectionMode(QAbstractItemView::ExtendedSelection);for(auto a:assets){auto o=a.toObject();if(o.value("kind")=="image")list->addItem(o.value("id").toString());}box->addWidget(list);auto ms=new QSpinBox;ms->setObjectName("stageFrameDuration");ms->setRange(1,60000);ms->setValue(120);ms->setSuffix(QObject::tr(" ms / 프레임"));box->addWidget(ms);auto ok=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);box->addWidget(ok);QObject::connect(search,&QLineEdit::textChanged,&pick,[&](const QString &s){for(int i=0;i<list->count();i++){auto item=list->item(i);const bool hidden=!item->text().contains(s,Qt::CaseInsensitive);item->setHidden(hidden);if(hidden)item->setSelected(false);}});QObject::connect(ok,&QDialogButtonBox::accepted,&pick,&QDialog::accept);QObject::connect(ok,&QDialogButtonBox::rejected,&pick,&QDialog::reject);if(pick.exec()!=QDialog::Accepted)return;auto values=collectFrames();for(int i=0;i<list->count();i++)if(list->item(i)->isSelected()&&!list->item(i)->isHidden())values.append(QJsonObject{{"imageAssetId",list->item(i)->text()},{"durationMs",ms->value()}});if(values.size()>500){error->setText(QObject::tr("레이어당 최대 500프레임입니다."));return;}fillFrames(values);commit();});
  QObject::connect(removeFrame,&QPushButton::clicked,&dialog,[&]{int r=frames->currentRow();if(r<0)return;auto values=collectFrames();values.removeAt(r);fillFrames(values);commit();});
  auto moveFrame=[&](int d){int r=frames->currentRow(),n=r+d;auto v=collectFrames();if(r<0||n<0||n>=v.size())return;auto f=v.takeAt(r);v.insert(n,f);fillFrames(v);frames->setCurrentCell(n,0);commit();};QObject::connect(frameUp,&QPushButton::clicked,&dialog,[&]{moveFrame(-1);});QObject::connect(frameDown,&QPushButton::clicked,&dialog,[&]{moveFrame(1);});
  QObject::connect(scrub,&QSlider::valueChanged,&dialog,[&]{refresh();});QObject::connect(play,&QPushButton::clicked,&dialog,[&]{if(timer.isActive())timer.stop();else{if(scrub->value()==scrub->maximum())scrub->setValue(0);elapsed.restart();timer.start();}});QObject::connect(&timer,&QTimer::timeout,&dialog,[&]{scrub->setValue(qMin(scrub->maximum(),scrub->value()+int(elapsed.restart())));if(scrub->value()==scrub->maximum())timer.stop();});
  QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{commit();if(layers.isEmpty()){error->setText(QObject::tr("최소 한 개의 레이어가 필요합니다."));return;}for(auto v:layers){auto l=v.toObject();if(l.value("frames").toArray().isEmpty()){error->setText(QObject::tr("각 레이어에 최소 한 프레임을 추가하세요."));return;}for(auto f:l.value("frames").toArray())if(!canvas->images.contains(f.toObject().value("imageAssetId").toString())){error->setText(QObject::tr("누락된 이미지 참조를 고치세요."));return;}}dialog.accept();});
  refill(layers.isEmpty()?-1:0);if(dialog.exec()!=QDialog::Accepted)return false;stage=cleared?QJsonObject{}:currentStage();return true;
}
