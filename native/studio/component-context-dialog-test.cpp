#include <QtWidgets>
#include <iostream>
#include "component-context-editor.hpp"
#include "component-preview.hpp"
#include "components.hpp"
#include "canvas.hpp"

int main(int argc,char**argv){
  QApplication app(argc,argv);int checks=0;
  auto check=[&](bool ok,const char* label){if(!ok){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);}++checks;};
  const QJsonObject person{{"id","companion"},{"name","소운"},{"relationships",QJsonArray{QJsonObject{{"id","trust"},{"label","신뢰"}}}}};
  const QJsonObject view{{"documents",QJsonArray{QJsonObject{{"kind","project"},{"data",QJsonObject{}}},QJsonObject{{"kind","character"},{"data",person}}}}};
  const QJsonObject numeric{{"kind","relationship"},{"characterId","companion"},{"relationshipId","trust"},{"owner",QJsonObject{{"kind","actor"}}}};
  const QJsonObject base{{"id","name"},{"kind","text"},{"text","대체 문구"},{"uiContext",QJsonObject{{"characterId","companion"}}}};
  const QJsonObject preview{{"_previewContextNames",contextNamesForPreview(view)}};
  auto label=base;label["contextField"]="characterId";
  check(contextNameForPreview(preview,label)=="소운","canvas resolves named character");
  auto inherited=label;inherited.remove("uiContext");inheritCanvasElement(inherited,base);
  check(contextNameForPreview(preview,inherited)=="소운","canvas inherits component context");
  inheritCanvasElement(label,QJsonObject{{"uiContext",QJsonObject{{"characterId","someone-else"}}}});
  check(contextNameForPreview(preview,label)=="소운","child context overrides parent");
  auto documents=view.value("documents").toArray();documents.append(QJsonObject{{"kind","character"},{"data",QJsonObject{{"id","hero"},{"name","연서진"}}}});
  const QJsonObject names{{"_previewContextNames",contextNamesForPreview(QJsonObject{{"documents",documents}})}};
  QJsonObject reusable=label;reusable.remove("uiContext");reusable["width"]=180;reusable["height"]=40;
  const QJsonObject visual{{"width",640},{"height",480},{"components",QJsonArray{QJsonObject{{"id","card"},{"width",180},{"height",40},{"elements",QJsonArray{reusable}}}}}};
  QJsonArray instances;for(const auto id:{QString("companion"),QString("hero")})instances.append(QJsonObject{{"id",id},{"kind","component"},{"componentId","card"},{"width",180},{"height",40},{"uiContext",QJsonObject{{"characterId",id}}}});
  QJsonArray resolved;for(const auto& element:canvasElements(visual,instances,QSize(640,480)))if(element.toObject().value("kind")=="text")resolved.append(element);
  check(resolved.size()==2,"same component produces two canvas placements");
  check(contextNameForPreview(names,resolved[0].toObject())=="소운"&&contextNameForPreview(names,resolved[1].toObject())=="연서진","each placement keeps its own named target");
  for(const QString mode:{QString("characterId"),QString("data-value"),QString("none")}){
    auto original=base;
    original["binding"]=mode=="characterId"?"context-name":mode;
    if(mode=="characterId")original["contextField"]=mode;
    if(mode=="data-value"){original["dataBinding"]=numeric;original["valueFormat"]="{label}: {value}";}
    for(int pass=0;pass<2;++pass){
      QTimer::singleShot(0,[&]{
        auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());check(dialog!=nullptr,"context dialog visible");
        auto display=dialog->findChild<QComboBox*>("contextDisplay");
        check(display&&display->currentData().toString()==mode,"saved display mode restored");
        check(!display->currentText().trimmed().isEmpty(),"display mode has a visible label");
        if(mode=="data-value")check(dialog->findChild<QComboBox*>("contextNumeric")->currentData().toJsonObject()==numeric,"numeric binding restored");
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
      });
      QJsonObject output;check(editComponentContext(nullptr,original,view,output),"unchanged context saves");
      check(output==original,"reopen and save preserves original data");original=output;
    }
  }
  if(argc==3){
    check(QGuiApplication::platformName()=="offscreen","canvas capture must not use the user's desktop");
    const QDir root(QString::fromLocal8Bit(argv[1]));QJsonArray documents;QJsonObject project;
    QDirIterator files(root.absolutePath(),{"*.json"},QDir::Files,QDirIterator::Subdirectories);
    while(files.hasNext()){
      QFile file(files.next());check(file.open(QIODevice::ReadOnly),"read authored project");const auto data=QJsonDocument::fromJson(file.readAll()).object();
      const auto relative=root.relativeFilePath(file.fileName());QString kind;
      if(relative=="project.json"){kind="project";project=data;}
      else if(relative=="attributes.json")kind="attributes";
      else if(relative.startsWith("characters/"))kind="character";
      else if(relative.startsWith("activities/"))kind="activity";
      if(!kind.isEmpty())documents.append(QJsonObject{{"kind",kind},{"data",data}});
    }
    check(!project.isEmpty(),"authored project exists");project["_previewContextNames"]=contextNamesForPreview(QJsonObject{{"documents",documents}});
    const auto font=qEnvironmentVariable("RAISING_QT_TEST_FONT",QDir::current().absoluteFilePath("dist/native-player-authoring-preview/fonts/NotoSansKR.ttf"));
    check(QFileInfo::exists(font),"offscreen canvas has explicit Korean font");project["_defaultFontPath"]=font;
    const auto screen=project.value("visual").toObject().value("screens").toArray().first().toObject();
    ScreenCanvas canvas;canvas.resize(1024,640);canvas.setScreen(project,screen,{},QString(),true);canvas.show();app.processEvents();
    check(canvas.grab().save(QString::fromLocal8Bit(argv[2])),"offscreen authored canvas captured");
  }
  std::cout<<"PASS "<<checks<<" component context dialog checks\n";
}
