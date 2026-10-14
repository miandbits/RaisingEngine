#include "studio.hpp"
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

void StudioWindow::editScreenSettings() {
  if(bridge.busy()||projectData().isEmpty())return;
  const auto targetId=managedScreenId();
  auto project=projectData(),visual=project.value("visual").toObject();
  auto screens=visual.value("screens").toArray();int screenIndex=-1;
  for(int i=0;i<screens.size();++i)if(screens[i].toObject().value("id")==targetId)screenIndex=i;
  if(screenIndex<0)return;
  auto screen=screens[screenIndex].toObject();const auto elements=screen.value("elements").toArray();
  const auto originalProject=project;
  const auto originalRevision=view.value("revision"),originalRoot=view.value("root");
  const int originalWidth=visual.value("width").toInt(1280),originalHeight=visual.value("height").toInt(720);
  int backgroundIndex=-1;
  // A background is an existing ordinary root element, so every renderer and
  // the element inspector retain the same contract and undo representation.
  for(int i=0;i<elements.size();++i){
    const auto e=elements[i].toObject();const auto kind=e.value("kind").toString();
    if(!e.contains("parentId")&&(kind=="panel"||kind=="image")&&
       !e.value("hidden").toBool()&&!e.contains("visibleCondition")&&e.value("visibleWhen").toString("always")=="always"&&
       (kind!="image"||e.value("imageSource").toString("asset")=="asset"||e.value("imageSource").toString()=="background")&&
       e.value("x").toDouble()==0&&e.value("y").toDouble()==0&&
       e.value("width").toDouble()>=originalWidth&&e.value("height").toDouble()>=originalHeight){backgroundIndex=i;break;}
  }
  const auto existing=backgroundIndex<0?QJsonObject{}:elements[backgroundIndex].toObject();
  QDialog dialog(this);dialog.setObjectName("screenSettingsDialog");dialog.setWindowTitle(tr("화면 설정 · ")+screen.value("label").toString());dialog.resize(620,460);
  auto outer=new QVBoxLayout(&dialog);auto form=new QFormLayout;outer->addLayout(form);
  auto name=new QLineEdit(screen.value("label").toString());name->setObjectName("screenSettingsName");name->setMaxLength(100);form->addRow(tr("화면 이름"),name);
  auto mode=new QComboBox;mode->setObjectName("screenBackgroundMode");
  mode->addItem(tr("현재 배경 유지"),"keep");mode->addItem(tr("단색"),"color");mode->addItem(tr("이미지 에셋"),"asset");mode->addItem(tr("실행 중 장면 배경"),"scene");form->addRow(tr("배경"),mode);
  auto color=new QLineEdit(existing.value("fill").toString("#dce9df"));color->setObjectName("screenBackgroundColor");
  auto colorRow=new QHBoxLayout;colorRow->addWidget(color);auto choose=new QPushButton(tr("색상 선택…"));choose->setObjectName("screenBackgroundPickColor");colorRow->addWidget(choose);form->addRow(tr("배경색"),colorRow);
  auto image=new QComboBox;image->setObjectName("screenBackgroundAsset");image->addItem(tr("이미지 선택"),QString());
  for(auto a:assets){auto asset=a.toObject();if(asset.value("kind")=="image")image->addItem(asset.value("id").toString(),asset.value("id").toString());}
  image->setCurrentIndex(qMax(0,image->findData(existing.value("imageAssetId").toString())));form->addRow(tr("배경 이미지"),image);
  auto fit=new QComboBox;fit->setObjectName("screenBackgroundFit");fit->addItem(tr("화면 채우기 (잘림 허용)"),"cover");fit->addItem(tr("전체 이미지 맞추기"),"contain");fit->setCurrentIndex(qMax(0,fit->findData(existing.value("fit").toString("cover"))));form->addRow(tr("이미지 맞춤"),fit);
  auto width=new QSpinBox;
  auto height=new QSpinBox;width->setObjectName("screenReferenceWidth");height->setObjectName("screenReferenceHeight");width->setRange(640,3840);height->setRange(360,2160);width->setValue(originalWidth);height->setValue(originalHeight);
  auto sizeRow=new QHBoxLayout;sizeRow->addWidget(width);sizeRow->addWidget(new QLabel("×"));sizeRow->addWidget(height);form->addRow(tr("프로젝트 기준 크기"),sizeRow);
  auto help=new QLabel(tr("기준 크기는 모든 화면에 적용됩니다. 기존 요소의 좌표와 크기는 유지됩니다. 배경을 바꾸면 이 화면의 전체 영역을 채우며, 요소 목록에서 다시 편집할 수 있습니다."));help->setWordWrap(true);outer->addWidget(help);
  auto error=new QLabel;error->setObjectName("screenSettingsError");error->setWordWrap(true);error->setStyleSheet("color:#f3a18a");outer->addWidget(error);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons->setObjectName("screenSettingsButtons");outer->addWidget(buttons);
  const auto update=[&]{const auto value=mode->currentData().toString();color->setEnabled(value=="color");choose->setEnabled(value=="color");image->setEnabled(value=="asset");fit->setEnabled(value=="asset"||value=="scene");};
  connect(mode,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{update();});update();
  connect(choose,&QPushButton::clicked,&dialog,[&]{const auto picked=QColorDialog::getColor(QColor(color->text()),&dialog,tr("화면 배경색"));if(picked.isValid())color->setText(picked.name());});
  connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
    if(view.value("revision")!=originalRevision||view.value("root")!=originalRoot){error->setText(tr("프로젝트가 변경되었습니다. 창을 닫고 화면 설정을 다시 여세요."));return;}
    if(name->text().trimmed().isEmpty()){error->setText(tr("화면 이름을 입력하세요."));return;}
    const auto value=mode->currentData().toString();
    if(value=="color"&&!QRegularExpression("^#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$").match(color->text()).hasMatch()){error->setText(tr("배경색은 #RRGGBB 또는 #RRGGBBAA로 입력하세요."));return;}
    if(value=="asset"&&image->currentData().toString().isEmpty()){error->setText(tr("배경 이미지를 선택하세요."));return;}
    auto nextElements=elements;
    if(value!="keep"){
      auto background=existing;
      if(backgroundIndex<0){
        if(elements.size()>=100){error->setText(tr("배경을 추가하려면 요소 수를 100개 미만으로 줄이세요."));return;}
        QSet<QString> ids;for(auto e:elements)ids.insert(e.toObject().value("id").toString());QString id="screen-background";int n=1;while(ids.contains(id))id="screen-background-"+QString::number(n++);
        background={{"id",id},{"label",tr("화면 배경")},{"locked",true}};
      }
      background["kind"]=value=="color"?"panel":"image";background["x"]=0;background["y"]=0;background["width"]=width->value();background["height"]=height->value();
      if(value=="color")background["fill"]=color->text();
      else{background["imageSource"]=value=="asset"?"asset":"background";if(value=="asset")background["imageAssetId"]=image->currentData().toString();background["fit"]=fit->currentData().toString();}
      if(backgroundIndex<0)nextElements.prepend(background);else nextElements[backgroundIndex]=background;
    }
    screen["label"]=name->text().trimmed();screen["elements"]=nextElements;screens[screenIndex]=screen;
    visual["screens"]=screens;visual["width"]=width->value();visual["height"]=height->value();project["visual"]=visual;dialog.accept();
  });
  if(dialog.exec()!=QDialog::Accepted||project==originalProject)return;
  replaceProject(project);
}
