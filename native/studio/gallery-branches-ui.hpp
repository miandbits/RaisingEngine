#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

inline QJsonObject makeGalleryBranchesUi(QJsonObject project){
  auto visual=project.value("visual").toObject();auto screens=visual.value("screens").toArray(),components=visual.value("components").toArray();
  QSet<QString> used;for(auto v:screens)used.insert(v.toObject().value("id").toString());for(auto v:components)used.insert(v.toObject().value("id").toString());
  auto unique=[&](QString base){auto id=base;int i=2;while(used.contains(id))id=base+"-"+QString::number(i++);used.insert(id);return id;};
  const auto screenId=unique("gallery-branches"),cardId=unique("gallery-branch-card");
  const int w=visual.value("width").toInt(1280),h=visual.value("height").toInt(720);
  auto node=[](QString id,QString kind,int x,int y,int width,int height){return QJsonObject{{"id",id},{"kind",kind},{"x",x},{"y",y},{"width",width},{"height",height}};};
  auto text=[&](QString id,QString label,int x,int y,int width,int height,int size){auto e=node(id,"text",x,y,width,height);e["text"]=label;e["fontSize"]=size;e["color"]="#eee5d2";return e;};
  auto button=[&](QString id,QString label,QString action,int x,int y,int width){auto e=node(id,"button",x,y,width,44);e["text"]=label;e["action"]=action;e["fill"]="#304d51";e["color"]="#fff2da";e["hoverFill"]="#55736b";e["fontSize"]=18;e["radius"]=6;return e;};
  auto bg=node("background","panel",0,0,560,220);bg["fill"]="#243b43";bg["radius"]=8;bg["anchorX"]="stretch";bg["anchorY"]="stretch";
  auto play=button("play","이 분기 회상","galleryBranch",350,160,190);play["anchorX"]="right";play["anchorY"]="bottom";
  components.append(QJsonObject{{"id",cardId},{"label","도감 · 선택 분기 카드"},{"width",560},{"height",220},{"elements",QJsonArray{
    bg,text("name","선택 내용",20,14,520,54,22),text("prompt","선택 지점",20,70,520,70,16),
    text("status","해금 상태",20,160,320,44,16),play}}});
  auto bind=[](QString child,QString property,QString field){return QJsonObject{{"elementId",child},{"property",property},{"field",field}};};
  auto list=node("branches","collection",32,140,w-64,h-172);list["componentId"]=cardId;list["source"]=QJsonObject{{"type","galleryBranches"}};
  list["layout"]="grid";list["columns"]=2;list["gap"]=16;list["rowHeight"]=220;list["anchorX"]="stretch";list["anchorY"]="stretch";
  list["bindings"]=QJsonArray{bind("name","text","name"),bind("prompt","text","prompt"),bind("status","text","statusText"),bind("play","entryId","entryId"),bind("play","branchId","branchId"),bind("play","enabled","canReplay")};
  auto backdrop=node("background","panel",0,0,w,h);backdrop["fill"]="#15252d";backdrop["anchorX"]="stretch";backdrop["anchorY"]="stretch";
  auto back=button("back","도감 목록으로","galleryBack",w-220,24,188);back["anchorX"]="right";
  auto error=text("error","",32,104,w-64,30,16);error["binding"]="gallery-error";
  screens.append(QJsonObject{{"id",screenId},{"label","도감 · 선택 분기"},{"elements",QJsonArray{backdrop,
    text("heading","선택으로 갈라진 기억",32,24,w-280,44,30),back,
    text("help","경험한 선택의 기록을 회상합니다. 아직 보지 못한 선택은 잠금으로 표시됩니다.",32,78,w-64,28,18),error,list}}});
  auto gallery=project.value("gallery").toObject();
  for(int i=0;i<screens.size();++i){auto s=screens[i].toObject();if(s.value("id")!=gallery.value("detailScreenId"))continue;
    auto es=s.value("elements").toArray();auto open=button(unique("open-branches"),"선택 분기 보기","galleryBranches",w-420,24,188);open["anchorX"]="right";es.append(open);s["elements"]=es;screens[i]=s;
  }
  visual["screens"]=screens;visual["components"]=components;project["visual"]=visual;gallery["branchScreenId"]=screenId;project["gallery"]=gallery;return project;
}
