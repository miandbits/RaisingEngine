#include "surface-editor.hpp"
#include "surface-paint.hpp"
#include "surface-image.hpp"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QImageReader>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include <cmath>

namespace {
class SurfacePreview:public QWidget {
public:
  QImage image;
  QString imagePath;
  QJsonObject surface;
  explicit SurfacePreview(QWidget* parent=nullptr):QWidget(parent){setMinimumHeight(152);setObjectName("surfacePreview");}
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);p.fillRect(rect(),QColor("#252c32"));
    const QRectF sourceBox(8,8,qMax(1,width()/2-16),height()-16),resultBox(width()/2+8,8,qMax(1,width()/2-16),height()-16);
    for(const auto& box:{sourceBox,resultBox}) {
      p.save();p.setClipRect(box);
      for(int y=int(box.top());y<box.bottom();y+=12)for(int x=int(box.left());x<box.right();x+=12)
        p.fillRect(QRectF(x,y,12,12),((x-int(box.left()))/12+(y-int(box.top()))/12)%2?QColor("#c8c8c8"):QColor("#ededed"));
      p.restore();
    }
    if(image.isNull())return;
    auto size=image.size();size.scale(sourceBox.size().toSize(),Qt::KeepAspectRatio);
    const QRectF target(sourceBox.center()-QPointF(size.width()/2.,size.height()/2.),size);
    p.drawImage(target,image);
    if(surface.value("mode")=="nine-slice"){
      const auto inset=surface.value("insets").toObject();
      p.save();p.setClipRect(target);p.setPen(QPen(QColor("#e03991"),1,Qt::DashLine));
      const auto x=[&](double v){return target.left()+v*target.width()/image.width();};
      const auto y=[&](double v){return target.top()+v*target.height()/image.height();};
      for(auto position:{x(inset.value("left").toInt()),x(image.width()-inset.value("right").toInt())})p.drawLine(QPointF(position,target.top()),QPointF(position,target.bottom()));
      for(auto position:{y(inset.value("top").toInt()),y(image.height()-inset.value("bottom").toInt())})p.drawLine(QPointF(target.left(),position),QPointF(target.right(),position));
      p.restore();
    }
    paintSurface(&p,resultBox,image,surface);
  }
};
}

SurfaceField::SurfaceField(QJsonArray values,QWidget* parent):QWidget(parent),assets(std::move(values)) {
  auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
  button=new QPushButton;button->setObjectName("surfaceEdit");button->setMinimumWidth(0);layout->addWidget(button);
  connect(button,&QPushButton::clicked,this,&SurfaceField::edit);setValue(QJsonValue::Undefined);
}
void SurfaceField::setValue(QJsonValue value) {
  current=value;
  const auto surface=value.toObject();const auto mode=surface.value("mode").toString();
  const auto label=value.isUndefined()?tr("상속"):mode=="none"?tr("없음"):mode=="tile"?tr("타일"):mode=="nine-slice"?tr("9분할"):tr("알 수 없는 설정");
  button->setText(tr("표면 편집…")+" · "+label);
  button->setToolTip(surface.value("imageAssetId").toString());
}
void SurfaceField::edit() {
  QDialog dialog(this);dialog.setObjectName("surfaceDialog");dialog.setWindowTitle(tr("배경 표면"));dialog.resize(640,570);
  auto outer=new QVBoxLayout(&dialog);auto scroll=new QScrollArea;scroll->setWidgetResizable(true);outer->addWidget(scroll,1);
  auto content=new QWidget;scroll->setWidget(content);auto layout=new QVBoxLayout(content);auto form=new QFormLayout;form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);form->setRowWrapPolicy(QFormLayout::WrapLongRows);layout->addLayout(form);
  auto mode=new QComboBox;mode->setObjectName("surfaceMode");
  for(const auto& item:QList<QPair<QString,QString>>{{tr("상속"),""},{tr("없음"),"none"},{tr("타일"),"tile"},{tr("9분할"),"nine-slice"}})mode->addItem(item.first,item.second);
  const auto initial=current.toObject();auto initialMode=initial.value("mode").toString();int modeIndex=mode->findData(initialMode);
  if(!current.isUndefined()&&modeIndex<0){mode->addItem(tr("알 수 없는 설정")+" · "+initialMode,initialMode);modeIndex=mode->count()-1;}
  mode->setCurrentIndex(qMax(0,modeIndex));form->addRow(tr("방식"),mode);
  auto asset=new QComboBox;asset->setObjectName("surfaceAsset");asset->setMinimumContentsLength(16);asset->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);asset->addItem(tr("PNG 이미지 선택"),"");
  QMap<QString,QString> paths;
  for(auto item:assets){const auto a=item.toObject();if(a.value("kind")!="image")continue;if(!readableSurfaceImage(a.value("absolutePath").toString()))continue;const auto id=a.value("id").toString();paths[id]=a.value("absolutePath").toString();asset->addItem(a.value("label").toString(id),id);}
  const auto initialAsset=initial.value("imageAssetId").toString();if(!initialAsset.isEmpty()&&asset->findData(initialAsset)<0)asset->addItem(tr("누락되었거나 PNG가 아님")+" · "+initialAsset,initialAsset);
  asset->setCurrentIndex(qMax(0,asset->findData(initialAsset)));form->addRow(tr("PNG 이미지"),asset);
  auto limits=new QLabel(tr(".png 파일만 사용합니다. 최대 20 MiB, 한 변 8192픽셀, 전체 32메가픽셀입니다."));limits->setWordWrap(true);layout->addWidget(limits);
  auto insetBox=new QWidget;auto grid=new QGridLayout(insetBox);grid->setContentsMargins(0,0,0,0);QMap<QString,QSpinBox*> fields;int index=0;
  for(const auto& item:QList<QPair<QString,QString>>{{"top",tr("위")},{"right",tr("오른쪽")},{"bottom",tr("아래")},{"left",tr("왼쪽")}}){auto spin=new QSpinBox;spin->setObjectName("surface-"+item.first);spin->setRange(0,1000000);spin->setValue(initial.value("insets").toObject().value(item.first).toInt());spin->setSuffix(tr(" px"));fields[item.first]=spin;grid->addWidget(new QLabel(item.second),index/2,index%2*2);grid->addWidget(spin,index/2,index%2*2+1);++index;}
  form->addRow(tr("가장자리 두께"),insetBox);
  auto dimensions=new QLabel;dimensions->setObjectName("surfaceDimensions");layout->addWidget(dimensions);
  auto titles=new QLabel(tr("원본과 분할선                         크기가 바뀐 표면 미리보기"));titles->setWordWrap(true);layout->addWidget(titles);
  auto preview=new SurfacePreview;layout->addWidget(preview);
  auto help=new QLabel(tr("PNG 전체를 사용합니다. 원본 1픽셀은 배치 1단위입니다. 타일은 반복하고 9분할은 모서리를 보존하며 가장자리와 가운데를 늘립니다. 작은 대상에서는 양쪽 모서리가 비례해 줄어듭니다. 투명 영역은 기존 바탕색을 보여 줍니다. 표면은 여백이나 클릭 영역을 바꾸지 않습니다."));help->setWordWrap(true);layout->addWidget(help);
  auto error=new QLabel;error->setObjectName("surfaceError");error->setWordWrap(true);layout->addWidget(error);layout->addStretch();
  const auto draft=[&]()->QJsonValue {const auto key=mode->currentData().toString();if(key.isEmpty())return QJsonValue::Undefined;QJsonObject value{{"mode",key}};if(key=="tile"||key=="nine-slice")value["imageAssetId"]=asset->currentData().toString();if(key=="nine-slice"){QJsonObject insets;for(auto it=fields.begin();it!=fields.end();++it)insets[it.key()]=it.value()->value();value["insets"]=insets;}return value;};
  auto refresh=[&]{
    const auto key=mode->currentData().toString();const bool textured=key=="tile"||key=="nine-slice";asset->setEnabled(textured);insetBox->setEnabled(key=="nine-slice");const auto imagePath=paths.value(asset->currentData().toString());if(preview->imagePath!=imagePath){preview->imagePath=imagePath;preview->image=loadSurfaceImage(imagePath);}preview->surface=draft().toObject();preview->update();dimensions->setText(preview->image.isNull()?QString():tr("원본 크기: %1 × %2 px").arg(preview->image.width()).arg(preview->image.height()));error->clear();
  };
  connect(mode,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{refresh();});connect(asset,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{refresh();});for(auto spin:fields)connect(spin,qOverload<int>(&QSpinBox::valueChanged),&dialog,[&]{refresh();});
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->setObjectName("surfaceButtons");outer->addWidget(buttons);
  connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
    const auto key=mode->currentData().toString();
    if(key!=""&&key!="none"&&key!="tile"&&key!="nine-slice"){error->setText(tr("표면 방식을 선택하세요."));return;}
    if((key=="tile"||key=="nine-slice")&&preview->image.isNull()){error->setText(tr("읽을 수 있는 PNG 이미지를 선택하세요."));return;}
    if(key=="nine-slice"&&(fields["left"]->value()+fields["right"]->value()>=preview->image.width()||fields["top"]->value()+fields["bottom"]->value()>=preview->image.height())){error->setText(tr("양쪽 가장자리의 합은 원본 크기보다 작아야 합니다. 가운데에 최소 1픽셀이 필요합니다."));return;}
    dialog.accept();
  });refresh();
  if(dialog.exec()==QDialog::Accepted){const auto next=draft();if(next!=current){setValue(next);emit valueChanged();}}
}
