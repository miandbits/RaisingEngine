#include "studio.hpp"
#include "surface-editor.hpp"
#include "surface-paint.hpp"
#include "surface-image.hpp"
#include "control-style-editor.hpp"
#include "validation-location.hpp"
#include <QPointer>
#include <QMessageBox>
#include "design.hpp"
#include "editor-language.hpp"
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QFile>
#include <QGraphicsItem>
#include <QJsonDocument>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QTreeWidgetItemIterator>

namespace {
QJsonObject tile(){return {{"mode","tile"},{"imageAssetId","surface-fixture"}};}
QJsonObject sliced(){return {{"mode","nine-slice"},{"imageAssetId","surface-fixture"},{"insets",QJsonObject{{"top",4},{"right",4},{"bottom",4},{"left",4}}}};}
QImage pattern(){QImage image(12,12,QImage::Format_ARGB32);QPainter p(&image);const QColor colors[]{Qt::red,Qt::yellow,Qt::green,Qt::cyan,Qt::white,Qt::magenta,Qt::blue,Qt::gray,Qt::black};for(int y=0;y<3;++y)for(int x=0;x<3;++x)p.fillRect(x*4,y*4,4,4,colors[y*3+x]);return image;}
bool writeJson(QString path,QJsonObject value){QFile f(path);return f.open(QIODevice::WriteOnly|QIODevice::Truncate)&&f.write(QJsonDocument(value).toJson())>0;}
QJsonObject readJson(QString path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return QJsonDocument::fromJson(f.readAll()).object();}
bool copyFixture(QString source,QString target){QDirIterator it(source,QDir::Files|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);while(it.hasNext()){auto file=it.next(),dest=target+"/"+QDir(source).relativeFilePath(file);if(!QDir().mkpath(QFileInfo(dest).absolutePath())||!QFile::copy(file,dest))return false;}return true;}
QJsonObject screenOf(QJsonObject project){for(auto value:project["visual"].toObject()["screens"].toArray())if(value.toObject()["id"]=="title")return value.toObject();return {};}
QJsonObject elementOf(QJsonObject project,QString id){for(auto value:screenOf(project)["elements"].toArray())if(value.toObject()["id"]==id)return value.toObject();return {};}
void chooseElement(StudioWindow& w,QString id){
  auto list=w.findChild<QListWidget*>("projectList");QVERIFY(list);QListWidgetItem* title=nullptr;for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole)=="title")title=list->item(i);QVERIFY(title);QMetaObject::invokeMethod(list,"itemActivated",Q_ARG(QListWidgetItem*,title));
  auto tree=w.findChild<QTreeWidget*>("hierarchy");QVERIFY(tree);QTreeWidgetItem* selected=nullptr;for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole)==id)selected=*it;QVERIFY(selected);tree->clearSelection();tree->setCurrentItem(selected);selected->setSelected(true);QCOMPARE(w.selectedId(),id);
}
void editSurface(SurfaceField* field,QString mode,bool accept=true){
  QVERIFY(field);bool ran=false;
  QTimer::singleShot(50,field,[&]{
    auto d=field->findChild<QDialog*>("surfaceDialog");QVERIFY(d);QTimer::singleShot(10000,d,&QDialog::reject);
    auto modes=d->findChild<QComboBox*>("surfaceMode");modes->setCurrentIndex(modes->findData(mode));
    if(mode=="tile"||mode=="nine-slice"){auto asset=d->findChild<QComboBox*>("surfaceAsset");QVERIFY(asset->findData("surface-fixture")>=0);asset->setCurrentIndex(asset->findData("surface-fixture"));}
    if(mode=="nine-slice")for(auto key:{"top","right","bottom","left"})d->findChild<QSpinBox*>("surface-"+QString(key))->setValue(4);
    ran=true;if(accept)d->findChild<QDialogButtonBox*>("surfaceButtons")->button(QDialogButtonBox::Ok)->click();else d->reject();
  });field->editValue();QVERIFY(ran);
}
}
class SurfaceAuthoringTests:public QObject {
  Q_OBJECT
private slots:
  void initTestCase(){QVERIFY(installEditorLanguage(*qApp,qEnvironmentVariable("RAISING_TEST_EDITOR_LOCALE","en")));QDir().mkpath(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath())+"/.cache/qt-surface-authoring");}
  void nineSliceAndTilePixels(){
    const auto image=pattern();QImage target(28,20,QImage::Format_ARGB32);target.fill(Qt::transparent);{QPainter p(&target);paintSurface(&p,target.rect(),image,sliced());}
    QCOMPARE(target.pixelColor(1,1),QColor(Qt::red));QCOMPARE(target.pixelColor(26,1),QColor(Qt::green));QCOMPARE(target.pixelColor(1,18),QColor(Qt::blue));QCOMPARE(target.pixelColor(26,18),QColor(Qt::black));QCOMPARE(target.pixelColor(14,10),QColor(Qt::white));QCOMPARE(target.pixelColor(14,1),QColor(Qt::yellow));QCOMPARE(target.pixelColor(1,10),QColor(Qt::cyan));
    QImage small(4,4,QImage::Format_ARGB32);small.fill(Qt::transparent);{QPainter p(&small);paintSurface(&p,small.rect(),image,sliced());}QCOMPARE(small.pixelColor(0,0),QColor(Qt::red));QCOMPARE(small.pixelColor(3,0),QColor(Qt::green));QCOMPARE(small.pixelColor(0,3),QColor(Qt::blue));QCOMPARE(small.pixelColor(3,3),QColor(Qt::black));
    {QPainter p(&target);paintSurface(&p,target.rect(),image,tile());}QCOMPARE(target.pixelColor(1,1),target.pixelColor(13,13));QCOMPARE(target.pixelColor(5,1),target.pixelColor(17,13));
    auto alpha=image;alpha.setPixelColor(0,0,Qt::transparent);target.fill(QColor("#123456"));{QPainter p(&target);paintSurface(&p,target.rect(),alpha,tile());}QCOMPARE(target.pixelColor(0,0),QColor("#123456"));
    const auto before=target;{QPainter p(&target);paintSurface(&p,target.rect(),image,{{"mode","none"}});}QCOMPARE(target,before);
    QVERIFY(target.save(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath())+"/.cache/qt-surface-authoring/paint.png"));
  }
  void boundedPngPicker(){
    QTemporaryDir temp;QVERIFY(temp.isValid());const auto image=pattern();QVERIFY(image.save(temp.path()+"/valid.png"));QVERIFY(image.save(temp.path()+"/disguised.jpg","PNG"));QVERIFY(readableSurfaceImage(temp.path()+"/valid.png"));QVERIFY(!readableSurfaceImage(temp.path()+"/disguised.jpg"));QVERIFY(loadSurfaceImage(temp.path()+"/disguised.jpg").isNull());
    QImage wide(8193,1,QImage::Format_ARGB32);wide.fill(Qt::red);QVERIFY(wide.save(temp.path()+"/wide.png"));QVERIFY(!readableSurfaceImage(temp.path()+"/wide.png"));QFile large(temp.path()+"/large.png");QVERIFY(large.open(QIODevice::WriteOnly));QVERIFY(large.resize(20LL*1024*1024+1));large.close();QVERIFY(!readableSurfaceImage(temp.path()+"/large.png"));
  }
  void editorValidationCancelAndInheritance(){
    QTemporaryDir temp;QVERIFY(temp.isValid());QVERIFY(pattern().save(temp.path()+"/surface.png"));SurfaceField field({QJsonObject{{"id","surface-fixture"},{"kind","image"},{"absolutePath",temp.path()+"/surface.png"}}});QSignalSpy changed(&field,&SurfaceField::valueChanged);
    editSurface(&field,"tile",false);QVERIFY(field.value().isUndefined());QCOMPARE(changed.count(),0);
    editSurface(&field,"tile");QCOMPARE(field.value().toObject(),tile());QCOMPARE(changed.count(),1);
    bool checked=false;QTimer::singleShot(50,&field,[&]{
      auto d=field.findChild<QDialog*>("surfaceDialog");QVERIFY(d);QTimer::singleShot(10000,d,&QDialog::reject);auto modes=d->findChild<QComboBox*>("surfaceMode");modes->setCurrentIndex(modes->findData("nine-slice"));auto left=d->findChild<QSpinBox*>("surface-left"),right=d->findChild<QSpinBox*>("surface-right");left->setValue(8);right->setValue(4);d->findChild<QDialogButtonBox*>("surfaceButtons")->button(QDialogButtonBox::Ok)->click();QVERIFY(d->isVisible());QVERIFY(!d->findChild<QLabel*>("surfaceError")->text().isEmpty());QCOMPARE(field.value().toObject(),tile());left->setValue(4);for(auto key:{"top","bottom"})d->findChild<QSpinBox*>("surface-"+QString(key))->setValue(4);QVERIFY(d->grab().save(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath())+"/.cache/qt-surface-authoring/editor-"+editorLanguage()+".png"));checked=true;d->findChild<QDialogButtonBox*>("surfaceButtons")->button(QDialogButtonBox::Ok)->click();
    });field.editValue();QVERIFY(checked);QCOMPARE(field.value().toObject(),sliced());editSurface(&field,"none");QCOMPARE(field.value().toObject(),QJsonObject({{"mode","none"}}));editSurface(&field,"");QVERIFY(field.value().isUndefined());
  }
  void canvasResolvesSurfaceWithoutChangingBounds(){
    QTemporaryDir temp;QVERIFY(pattern().save(temp.path()+"/surface.png"));const QJsonArray assets{QJsonObject{{"id","surface-fixture"},{"kind","image"},{"absolutePath",temp.path()+"/surface.png"}}};
    QJsonObject e{{"id","panel"},{"kind","panel"},{"x",4},{"y",4},{"width",48},{"height",32},{"borderWidth",2},{"borderColor","#000000"},{"fill","#123456"},{"radius",0}};
    QJsonObject visual{{"width",64},{"height",48},{"defaultStyle",QJsonObject{{"surface",sliced()}}}};ScreenCanvas canvas;const auto render=[&]{QImage image(64,48,QImage::Format_ARGB32);image.fill(Qt::transparent);QPainter p(&image);canvas.QGraphicsView::scene()->render(&p,QRectF(0,0,64,48),QRectF(0,0,64,48));return image;};
    canvas.setScreen({{"visual",visual}},{{"id","title"},{"elements",QJsonArray{e}}},assets,"",false);const auto textured=render();QCOMPARE(textured.pixelColor(7,7),QColor(Qt::red));QCOMPARE(textured.pixelColor(48,7),QColor(Qt::green));QGraphicsItem* item=nullptr;for(auto i:canvas.QGraphicsView::scene()->items())if(i->data(0)=="panel")item=i;QVERIFY(item);QCOMPARE(item->pos(),QPointF(4,4));QVERIFY(item->shape().contains(QPointF(30,20)));QVERIFY(!item->shape().contains(QPointF(50,20)));
    e["surface"]=QJsonObject{{"mode","none"}};canvas.setScreen({{"visual",visual}},{{"id","title"},{"elements",QJsonArray{e}}},assets,"",false);QCOMPARE(render().pixelColor(7,7),QColor("#123456"));QVERIFY(textured.save(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath())+"/.cache/qt-surface-authoring/canvas.png"));
  }
  void missingControlStyleRepairOffersNamedStyles(){
    QJsonObject visual{{"styles",QJsonArray{QJsonObject{{"id","palette"},{"label","Palette"}}}},{"readableLayout",QJsonObject{{"controlStyleId","missing"}}},{"screens",QJsonArray{QJsonObject{{"id","title"},{"elements",QJsonArray{QJsonObject{{"id","plan"},{"kind","schedule"},{"controlStyleId","missing"}}}}}}}};
    const QJsonObject project{{"visual",visual}};
    for(auto pointer:{"/visual/readableLayout/controlStyleId","/visual/screens/0/elements/0/controlStyleId"}){
      QJsonObject applied;int count=0;auto document=QJsonObject{{"file","project.json"},{"kind","project"},{"data",project}};
      QPointer<QDialog> d=ValidationLocation::editor(nullptr,document,{document},{{"pointer",pointer},{"code","UI_CONTROL_STYLE_REFERENCE"}},[&](QJsonObject next,std::function<void(QString)> done){applied=next;++count;done("verified");});d->open();
      QCOMPARE(d->findChild<QLabel*>("validationFieldPath")->text(),QString(pointer));auto choices=d->findChild<QComboBox*>("validationReference");const int at=choices->findData("palette");QVERIFY(at>0);choices->setCurrentIndex(at);QMetaObject::invokeMethod(choices,"activated",Q_ARG(int,at));d->findChild<QPushButton*>("validationApplyRecheck")->click();QCOMPARE(count,1);QCOMPARE(ValidationLocation::value(applied,pointer).toString(),QString("palette"));auto expected=QJsonValue(project);QVERIFY(ValidationLocation::replace(expected,pointer,"palette"));QCOMPARE(applied,expected.toObject());d->reject();QTRY_VERIFY(!d);
    }
  }
  void controlColorPickersPreserveScopeAndUndo(){
    const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());QTemporaryDir temp(engine+"/.cache/qt-control-style-XXXXXX");QVERIFY(temp.isValid());QVERIFY(copyFixture(engine+"/examples/academy-visual",temp.path()));
    auto project=readJson(temp.path()+"/project.json"),visual=project["visual"].toObject();
    auto styles=visual["styles"].toArray();styles.append(QJsonObject{{"id","control-palette"},{"label","Control palette"},{"fill","#123456"},{"color","#fefefe"},{"padding",32},{"fontSize",40}});visual["styles"]=styles;
    auto screens=visual["screens"].toArray();for(int i=0;i<screens.size();++i){auto s=screens[i].toObject();if(s["id"]!="title")continue;auto elements=s["elements"].toArray();for(auto kind:{"schedule","saveSlots","loadSlots"})elements.append(QJsonObject{{"id","test-"+QString(kind).toLower()},{"kind",kind},{"x",20},{"y",20},{"width",300},{"height",200}});s["elements"]=elements;screens[i]=s;}visual["screens"]=screens;project["visual"]=visual;QVERIFY(writeJson(temp.path()+"/project.json",project));
    QComboBox missing;fillControlStyles(&missing,visual,"removed-style");QCOMPARE(missing.currentData().toString(),QString("removed-style"));QVERIFY(missing.currentText().contains("removed-style"));
    QJsonObject opaque{{"screens",QJsonArray{QJsonObject{{"elements",QJsonArray{QJsonObject{{"kind","plugin"},{"extension",QJsonObject{{"props",QJsonObject{{"controlStyleId","control-palette"}}}}}}}}}}}};QVERIFY(!usesControlStyle(opaque,"control-palette"));
    StudioWindow w(engine);w.show();w.openProject(temp.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
    auto verifyUndoRedo=[&](QJsonObject before){const auto after=w.projectData();w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),before);w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),after);};
    for(auto kind:{"schedule","saveSlots","loadSlots"}){
      const auto id="test-"+QString(kind).toLower();chooseElement(w,id);auto field=w.findChild<QComboBox*>("elementControlStyle");QVERIFY(field);QVERIFY(!field->isHidden());const auto before=w.projectData(),original=elementOf(before,id);const int chosen=field->findData("control-palette");QVERIFY(chosen>0);field->setCurrentIndex(chosen);QMetaObject::invokeMethod(field,"activated",Q_ARG(int,chosen));QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);auto wanted=original;wanted["controlStyleId"]="control-palette";QCOMPARE(elementOf(w.projectData(),id),wanted);verifyUndoRedo(before);
      const auto selected=w.projectData();field->setCurrentIndex(0);QMetaObject::invokeMethod(field,"activated",Q_ARG(int,0));QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(elementOf(w.projectData(),id),original);w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),selected);
    }
    chooseElement(w,"start");QVERIFY(w.findChild<QComboBox*>("elementControlStyle")->isHidden());
    const auto beforeReading=w.projectData();QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("readableLayoutDialog");QVERIFY(d);QTimer::singleShot(10000,d,&QDialog::reject);d->findChild<QCheckBox*>("readableEnabled")->setChecked(true);auto style=d->findChild<QComboBox*>("readableControlStyle");QVERIFY(style);style->setCurrentIndex(style->findData("control-palette"));QVERIFY(d->grab().save(engine+"/.cache/qt-surface-authoring/control-reading-"+editorLanguage()+".png"));d->findChild<QDialogButtonBox*>("readableButtons")->button(QDialogButtonBox::Save)->click();});w.findChild<QAction*>("readableLayout")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData()["visual"].toObject()["readableLayout"].toObject()["controlStyleId"].toString(),QString("control-palette"));verifyUndoRedo(beforeReading);
    const auto beforeCancel=w.projectData();QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("readableLayoutDialog");QVERIFY(d);d->findChild<QComboBox*>("readableControlStyle")->setCurrentIndex(0);d->reject();});w.findChild<QAction*>("readableLayout")->trigger();QCOMPARE(w.projectData(),beforeCancel);
    bool protectedStyle=false;QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("designSystemDialog");QVERIFY(d);QTimer::singleShot(10000,d,&QDialog::reject);auto list=d->findChild<QListWidget*>("designStyles");QVERIFY(list);list->setCurrentRow(list->count()-1);const auto count=list->count();QTimer::singleShot(50,d,[&]{auto message=d->findChild<QMessageBox*>();QVERIFY(message);protectedStyle=true;message->accept();});d->findChild<QPushButton*>("removeSharedStyle")->click();QCOMPARE(list->count(),count);d->reject();});w.findChild<QAction*>("editDesignSystem")->trigger();QVERIFY(protectedStyle);QCOMPARE(w.projectData(),beforeCancel);
    const auto after=w.projectData();w.findChild<QAction*>("validateProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);for(auto diagnostic:w.currentView()["diagnostics"].toArray())QVERIFY2(diagnostic.toObject()["severity"]!="error",qPrintable(QString::fromUtf8(QJsonDocument(diagnostic.toObject()).toJson(QJsonDocument::Compact))));QVERIFY2(w.findChild<QAction*>("saveProject")->isEnabled(),qPrintable(w.lastError));w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));QTRY_VERIFY_WITH_TIMEOUT(!w.currentView()["dirty"].toBool(),20000);w.openProject(temp.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);QCOMPARE(w.projectData(),after);w.findChild<QAction*>("validateProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);for(auto diagnostic:w.currentView()["diagnostics"].toArray())QVERIFY2(diagnostic.toObject()["severity"]!="error",qPrintable(QString::fromUtf8(QJsonDocument(diagnostic.toObject()).toJson(QJsonDocument::Compact))));
    QVERIFY(writeJson(engine+"/.cache/qt-surface-authoring/control-styles-"+editorLanguage()+".json",{{"ok",true},{"root",after["visual"].toObject()["readableLayout"]},{"schedule",elementOf(after,"test-schedule")},{"save",elementOf(after,"test-saveslots")},{"load",elementOf(after,"test-loadslots")}}));
  }
  void projectAuthoringRoundtrip(){
    const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath());QTemporaryDir temp(engine+"/.cache/qt-surface-project-XXXXXX");QVERIFY(temp.isValid());QVERIFY(copyFixture(engine+"/examples/academy-visual",temp.path()));QVERIFY(pattern().save(temp.path()+"/assets/surface.png"));
    auto manifest=readJson(temp.path()+"/assets/manifest.json");auto rows=manifest["assets"].toArray();rows.append(QJsonObject{{"id","surface-fixture"},{"kind","image"},{"path","assets/surface.png"}});manifest["assets"]=rows;QVERIFY(writeJson(temp.path()+"/assets/manifest.json",manifest));
    auto p=readJson(temp.path()+"/project.json"),v=p["visual"].toObject();v["components"]=QJsonArray{QJsonObject{{"id","surface-card"},{"label","Surface card"},{"width",128},{"height",96},{"elements",QJsonArray{QJsonObject{{"id","back"},{"kind","panel"},{"x",0},{"y",0},{"width",128},{"height",96},{"surface",tile()}}}}}};auto screens=v["screens"].toArray();for(int i=0;i<screens.size();++i){auto s=screens[i].toObject();if(s["id"]!="title")continue;auto es=s["elements"].toArray();es.append(QJsonObject{{"id","surface-instance"},{"kind","component"},{"componentId","surface-card"},{"x",0},{"y",0},{"width",128},{"height",96}});s["elements"]=es;screens[i]=s;}v["screens"]=screens;p["visual"]=v;QVERIFY(writeJson(temp.path()+"/project.json",p));
    StudioWindow w(engine);w.show();w.openProject(temp.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);chooseElement(w,"start");const auto before=w.projectData();
    QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("surfaceDialog");QVERIFY(d);QTimer::singleShot(10000,d,&QDialog::reject);auto m=d->findChild<QComboBox*>("surfaceMode");m->setCurrentIndex(m->findData("tile"));auto a=d->findChild<QComboBox*>("surfaceAsset");a->setCurrentIndex(a->findData("surface-fixture"));d->findChild<QDialogButtonBox*>("surfaceButtons")->button(QDialogButtonBox::Ok)->click();});w.findChild<QPushButton*>("elementSurface")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(elementOf(w.projectData(),"start")["surface"].toObject(),tile());const auto elementEdit=w.projectData();
    w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),before);w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),elementEdit);
    auto verifyUndoRedo=[&](QJsonObject previous){const auto changed=w.projectData();w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),previous);w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData(),changed);};
    const auto beforeStyles=w.projectData();QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("designSystemDialog");QVERIFY(d);QTimer::singleShot(10000,d,&QDialog::reject);editSurface(d->findChild<SurfaceField*>("styleSurface"),"nine-slice");d->findChild<QPushButton*>("addSharedStyle")->click();d->findChild<QLineEdit*>("styleLabel")->setText("Surface style");editSurface(d->findChild<SurfaceField*>("styleSurface"),"tile");d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();});w.findChild<QAction*>("editDesignSystem")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData()["visual"].toObject()["defaultStyle"].toObject()["surface"].toObject(),sliced());QCOMPARE(w.projectData()["visual"].toObject()["styles"].toArray().last().toObject()["surface"].toObject(),tile());
    verifyUndoRedo(beforeStyles);const auto beforeOverride=w.projectData();chooseElement(w,"surface-instance");QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("componentOverridesDialog");QVERIFY(d);QTimer::singleShot(10000,d,&QDialog::reject);QVERIFY(d->findChild<SurfaceField*>("child-surface")->value().isUndefined());editSurface(d->findChild<SurfaceField*>("child-surface"),"none");d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});w.findChild<QPushButton*>("componentOverrides")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(elementOf(w.projectData(),"surface-instance")["overrides"].toArray().first().toObject()["values"].toObject()["surface"].toObject(),QJsonObject({{"mode","none"}}));
    verifyUndoRedo(beforeOverride);const auto beforeComponent=w.projectData();QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("componentsDialog");QVERIFY(d);QTimer::singleShot(10000,d,&QDialog::reject);d->findChild<QListWidget*>("componentChildren")->setCurrentRow(0);editSurface(d->findChild<SurfaceField*>("child-surface"),"nine-slice");d->findChild<QPushButton*>("applyComponentChild")->click();d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});w.findChild<QAction*>("editComponents")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData()["visual"].toObject()["components"].toArray().first().toObject()["elements"].toArray().first().toObject()["surface"].toObject(),sliced());
    verifyUndoRedo(beforeComponent);const auto beforeVariant=w.projectData();chooseElement(w,"start");w.findChild<QPushButton*>("presentationVariants")->click();auto variants=w.findChild<QDialog*>("presentationVariantsDialog");QVERIFY(variants);variants->findChild<QPushButton*>("variantAdd")->click();auto variant=variants->findChild<QDialog*>("presentationVariantEdit");QVERIFY(variant);auto property=variant->findChild<QComboBox*>("variantProperty");property->setCurrentIndex(property->findData("surface"));editSurface(variant->findChild<SurfaceField*>("variantSurface"),"none");variant->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();variants->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(elementOf(w.projectData(),"start")["variants"].toArray().first().toObject()["values"].toObject()["surface"].toObject(),QJsonObject({{"mode","none"}}));
    verifyUndoRedo(beforeVariant);const auto beforeReading=w.projectData();QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("readableLayoutDialog");QVERIFY(d);QTimer::singleShot(10000,d,&QDialog::reject);d->findChild<QCheckBox*>("readableEnabled")->setChecked(true);editSurface(d->findChild<SurfaceField*>("readableSurface"),"tile");d->findChild<QDialogButtonBox*>("readableButtons")->button(QDialogButtonBox::Save)->click();});w.findChild<QAction*>("readableLayout")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QCOMPARE(w.projectData()["visual"].toObject()["readableLayout"].toObject()["surface"].toObject(),tile());
    verifyUndoRedo(beforeReading);const auto after=w.projectData();QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.currentView()["dirty"].toBool(),20000);w.openProject(temp.path());QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);QCOMPARE(w.projectData(),after);w.findChild<QAction*>("validateProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);for(auto diagnostic:w.currentView()["diagnostics"].toArray())QVERIFY2(diagnostic.toObject()["severity"]!="error",qPrintable(QString::fromUtf8(QJsonDocument(diagnostic.toObject()).toJson(QJsonDocument::Compact))));
    QVERIFY(writeJson(engine+"/.cache/qt-surface-authoring/roundtrip-"+editorLanguage()+".json",{{"ok",true},{"locale",editorLanguage()},{"defaultSurface",after["visual"].toObject()["defaultStyle"].toObject()["surface"]},{"elementSurface",elementOf(after,"start")["surface"]},{"readingSurface",after["visual"].toObject()["readableLayout"].toObject()["surface"]}}));
  }
};
QTEST_MAIN(SurfaceAuthoringTests)
#include "surface-authoring-test.moc"
