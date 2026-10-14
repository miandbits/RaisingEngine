#include "studio.hpp"
#include <QCheckBox>
#include <QCloseEvent>
#include <QPointer>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QImageReader>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>
#include <functional>

namespace {
class SpriteImportDialog final:public QDialog {
public:
  using QDialog::QDialog;bool pending=false;
  void reject() override {if(!pending)QDialog::reject();}
protected:
  void closeEvent(QCloseEvent *event) override {if(pending)event->ignore();else QDialog::closeEvent(event);}
};
class SpriteSheetCanvas final:public QWidget {
public:
  QImage source,preview;QVector<QRect> regions;std::function<void(QRect)> addRegion;
  explicit SpriteSheetCanvas(QWidget *parent=nullptr):QWidget(parent){setMinimumSize(500,260);setObjectName("spriteSheetCanvas");}
  QRectF imageRect()const{QSizeF size=source.size();size.scale(QSizeF(width()-16,height()-16),Qt::KeepAspectRatio);return {QPointF(width()/2.0-size.width()/2,height()/2.0-size.height()/2),size};}
protected:
  QPoint start,last;bool dragging=false;
  QPoint logical(QPointF pos)const{auto r=imageRect();return {qBound(0,int((pos.x()-r.x())*source.width()/r.width()),source.width()),qBound(0,int((pos.y()-r.y())*source.height()/r.height()),source.height())};}
  QRect selection()const{return QRect(QPoint(qMin(start.x(),last.x()),qMin(start.y(),last.y())),QSize(qAbs(last.x()-start.x()),qAbs(last.y()-start.y())));}
  void paintEvent(QPaintEvent*)override{QPainter p(this);p.fillRect(rect(),QColor("#202b34"));auto r=imageRect();p.setClipRect(r);for(int y=int(r.y());y<r.bottom();y+=16)for(int x=int(r.x());x<r.right();x+=16)p.fillRect(QRect(x,y,16,16),QColor(((x-int(r.x()))/16+(y-int(r.y()))/16)%2?"#65727a":"#48545c"));p.setRenderHint(QPainter::SmoothPixmapTransform,false);p.drawImage(r,preview.isNull()?source:preview);p.translate(r.topLeft());p.scale(r.width()/source.width(),r.height()/source.height());QPen pen(QColor("#ffe088"));pen.setCosmetic(true);pen.setWidth(2);p.setPen(pen);for(int i=0;i<regions.size();i++){p.drawRect(regions[i]);p.drawText(regions[i].topLeft()+QPoint(2,12),QString::number(i+1));}if(dragging){pen.setColor(Qt::white);p.setPen(pen);p.drawRect(selection());}}
  void mousePressEvent(QMouseEvent *e)override{if(e->button()!=Qt::LeftButton||!imageRect().contains(e->position()))return;start=last=logical(e->position());dragging=true;update();}
  void mouseMoveEvent(QMouseEvent *e)override{if(dragging){last=logical(e->position());update();}}
  void mouseReleaseEvent(QMouseEvent *e)override{if(!dragging)return;last=logical(e->position());dragging=false;auto region=selection();if(region.width()>0&&region.height()>0&&addRegion)addRegion(region);update();}
};
}

void StudioWindow::importSprites(){
  if(bridge.busy()||projectData().isEmpty())return;
  auto file=QFileDialog::getOpenFileName(this,tr("스프라이트 시트 PNG 가져오기"),{},tr("PNG 이미지 (*.png)"));if(file.isEmpty())return;
  QImageReader reader(file);auto size=reader.size();if(!size.isValid()||size.width()>8192||size.height()>8192||qint64(size.width())*size.height()>32*1024*1024||QFileInfo(file).size()>20*1024*1024){errorLabel->setText(tr("PNG는 20MiB, 8192px, 32메가픽셀 이내여야 합니다."));return;}
  auto source=reader.read();if(source.isNull()){errorLabel->setText(tr("PNG를 읽을 수 없습니다."));return;}
  SpriteImportDialog dialog(this);dialog.setObjectName("spriteImportDialog");dialog.setWindowTitle(tr("스프라이트 시트 · 영역과 투명색"));dialog.resize(1120,820);auto outer=new QVBoxLayout(&dialog);
  auto help=new QLabel(tr("원본을 보존하고 선택 영역을 별도 PNG 에셋으로 등록합니다. 드래그로 영역을 추가하거나 격자로 나누세요. 번호가 프레임 순서입니다."));help->setWordWrap(true);outer->addWidget(help);
  auto top=new QHBoxLayout;outer->addLayout(top);top->addWidget(new QLabel(tr("에셋 ID 접두어")));QSet<QString> usedIds;for(auto a:assets)usedIds.insert(a.toObject().value("id").toString());QString proposed="sprite";int serial=2;while(usedIds.contains(proposed+"-1"))proposed="sprite-"+QString::number(serial++);auto prefix=new QLineEdit(proposed);prefix->setObjectName("spritePrefix");prefix->setMaxLength(56);top->addWidget(prefix);auto key=new QCheckBox(tr("색상키 투명화"));key->setObjectName("spriteColorKeyEnabled");top->addWidget(key);auto color=new QLineEdit(source.pixelColor(0,0).name());color->setObjectName("spriteColorKey");color->setMaximumWidth(100);top->addWidget(color);top->addWidget(new QLabel(tr("허용 오차")));auto tolerance=new QSpinBox;tolerance->setObjectName("spriteTolerance");tolerance->setRange(0,255);top->addWidget(tolerance);
  auto canvas=new SpriteSheetCanvas;canvas->source=source;outer->addWidget(canvas,1);
  auto grid=new QHBoxLayout;outer->addLayout(grid);QStringList names{tr("시작 X"),tr("시작 Y"),tr("폭"),tr("높이"),tr("열"),tr("행"),tr("간격 X"),tr("간격 Y")};QSpinBox *fields[8];QStringList ids{"X","Y","Width","Height","Columns","Rows","GapX","GapY"};for(int i=0;i<8;i++){grid->addWidget(new QLabel(names[i]));fields[i]=new QSpinBox;fields[i]->setObjectName("spriteGrid"+ids[i]);fields[i]->setRange(i>=2&&i<=5?1:0,i==4||i==5?256:8192);fields[i]->setValue(i==2?32:i==3?40:i==4||i==5?1:0);grid->addWidget(fields[i]);}auto addGrid=new QPushButton(tr("격자 추가"));addGrid->setObjectName("spriteAddGrid");grid->addWidget(addGrid);
  auto table=new QTableWidget(0,4);table->setObjectName("spriteRegions");table->setHorizontalHeaderLabels({"X","Y",tr("폭"),tr("높이")});table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table->setMaximumHeight(210);outer->addWidget(table);
  auto controls=new QHBoxLayout;outer->addLayout(controls);auto remove=new QPushButton(tr("선택 영역 삭제")),up=new QPushButton(tr("↑")),down=new QPushButton(tr("↓"));remove->setObjectName("spriteRemoveRegion");controls->addWidget(remove);controls->addWidget(up);controls->addWidget(down);auto summary=new QLabel;controls->addWidget(summary,1);auto error=new QLabel;error->setObjectName("spriteImportError");error->setStyleSheet("color:#e68f77");outer->addWidget(error);auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);outer->addWidget(buttons);
  bool filling=false;QJsonArray regions;
  auto refresh=[&]{if(filling)return;regions={};canvas->regions.clear();qint64 pixels=0;for(int r=0;r<table->rowCount();r++){int v[4];for(int c=0;c<4;c++)v[c]=qobject_cast<QSpinBox*>(table->cellWidget(r,c))->value();regions.append(QJsonObject{{"x",v[0]},{"y",v[1]},{"width",v[2]},{"height",v[3]}});canvas->regions.append(QRect(v[0],v[1],v[2],v[3]));pixels+=qint64(v[2])*v[3];}summary->setText(tr("%1개 프레임 · %2 픽셀 · 원본 %3×%4").arg(regions.size()).arg(pixels).arg(source.width()).arg(source.height()));canvas->update();};
  auto append=[&](QRect region){filling=true;int row=table->rowCount();table->insertRow(row);int v[]{region.x(),region.y(),region.width(),region.height()};for(int c=0;c<4;c++){auto value=new QSpinBox;value->setRange(c<2?0:1,8192);value->setValue(v[c]);table->setCellWidget(row,c,value);connect(value,qOverload<int>(&QSpinBox::valueChanged),&dialog,[&]{refresh();});}filling=false;refresh();};
  canvas->addRegion=[&](QRect region){if(table->rowCount()>=256){error->setText(tr("최대 256개 프레임입니다."));return;}append(region);};
  connect(addGrid,&QPushButton::clicked,&dialog,[&]{int columns=fields[4]->value(),rows=fields[5]->value();if(table->rowCount()+columns*rows>256){error->setText(tr("최대 256개 프레임입니다."));return;}QVector<QRect> values;for(int y=0;y<rows;y++)for(int x=0;x<columns;x++){QRect r(fields[0]->value()+x*(fields[2]->value()+fields[6]->value()),fields[1]->value()+y*(fields[3]->value()+fields[7]->value()),fields[2]->value(),fields[3]->value());if(!source.rect().contains(r)){error->setText(tr("격자가 원본 이미지 경계를 벗어납니다."));return;}values.append(r);}for(auto r:values)append(r);error->clear();});
  auto updateKey=[&]{if(!key->isChecked()){canvas->preview={};canvas->update();return;}QColor selected(color->text());if(!QRegularExpression("^#[0-9a-fA-F]{6}$").match(color->text()).hasMatch()||!selected.isValid()){error->setText(tr("투명색은 #RRGGBB로 입력하세요."));return;}auto img=source.convertToFormat(QImage::Format_ARGB32);int tol=tolerance->value();for(int y=0;y<img.height();y++){auto row=reinterpret_cast<QRgb*>(img.scanLine(y));for(int x=0;x<img.width();x++){auto rgb=row[x];if(qAbs(qRed(rgb)-selected.red())<=tol&&qAbs(qGreen(rgb)-selected.green())<=tol&&qAbs(qBlue(rgb)-selected.blue())<=tol)row[x]=qRgba(qRed(rgb),qGreen(rgb),qBlue(rgb),0);}}canvas->preview=img;canvas->update();};
  connect(key,&QCheckBox::toggled,&dialog,[&]{updateKey();});connect(color,&QLineEdit::editingFinished,&dialog,[&]{updateKey();});connect(tolerance,&QSpinBox::editingFinished,&dialog,[&]{updateKey();});
  connect(remove,&QPushButton::clicked,&dialog,[&]{if(table->currentRow()>=0){table->removeRow(table->currentRow());refresh();}});
  auto move=[&](int d){int r=table->currentRow(),n=r+d;if(r<0||n<0||n>=table->rowCount())return;for(int c=0;c<4;c++){auto a=qobject_cast<QSpinBox*>(table->cellWidget(r,c)),b=qobject_cast<QSpinBox*>(table->cellWidget(n,c));int v=a->value();a->setValue(b->value());b->setValue(v);}table->setCurrentCell(n,0);refresh();};connect(up,&QPushButton::clicked,&dialog,[&]{move(-1);});connect(down,&QPushButton::clicked,&dialog,[&]{move(1);});
  connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{refresh();if(!QRegularExpression("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$").match(prefix->text()).hasMatch()){error->setText(tr("ID는 영문 소문자로 시작하고 소문자·숫자·하이픈을 사용하세요."));return;}if(regions.isEmpty()){error->setText(tr("최소 한 영역을 선택하세요."));return;}qint64 pixels=0;for(auto r:canvas->regions){if(!source.rect().contains(r)){error->setText(tr("선택 영역이 원본을 벗어납니다."));return;}pixels+=qint64(r.width())*r.height();}if(pixels>32*1024*1024){error->setText(tr("프레임 합계는 32메가픽셀 이내여야 합니다."));return;}if(key->isChecked()&&!QRegularExpression("^#[0-9a-fA-F]{6}$").match(color->text()).hasMatch()){error->setText(tr("투명색은 #RRGGBB로 입력하세요."));return;}QJsonObject params{{"filePath",file},{"prefix",prefix->text()},{"regions",regions}};if(key->isChecked())params["colorKey"]=QJsonObject{{"color",color->text()},{"tolerance",tolerance->value()}};
    dialog.pending=true;dialog.setEnabled(false);error->setText(tr("프레임을 등록하는 중…"));QPointer<SpriteImportDialog> guarded(&dialog);
    request("importSprites",params,[this,guarded,error](QJsonObject reply){if(!guarded)return;guarded->pending=false;guarded->setEnabled(true);auto result=reply.value("spriteImport").toObject();if(result.isEmpty()){error->setText(reply.value("error").toString(tr("등록하지 못했습니다. 설정을 확인하고 다시 시도하세요.")));return;}assetSearch->clear();bottom->setCurrentIndex(1);auto ids=result.value("assetIds").toArray();if(!ids.isEmpty())for(int i=0;i<assetList->count();i++)if(assetList->item(i)->data(Qt::UserRole).toString()==ids[0].toString())assetList->setCurrentRow(i);status->setText(tr("%1개 프레임 등록됨 · 연출 레이어의 프레임으로 선택하세요.").arg(ids.size()));guarded->accept();});
  });
  refresh();dialog.exec();
}
