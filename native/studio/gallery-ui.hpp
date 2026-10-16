#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

// A scaffold made exclusively of ordinary screens, components and data bindings.
// Once created, every element is editable through the normal canvas and inspector.
inline QJsonObject makeGalleryUi(QJsonObject project) {
  auto visual=project.value("visual").toObject();auto screens=visual.value("screens").toArray(),components=visual.value("components").toArray();
  QSet<QString> ids;for(auto v:screens)ids.insert(v.toObject().value("id").toString());for(auto v:components)ids.insert(v.toObject().value("id").toString());
  auto unique=[&](QString base){QString id=base;int n=2;while(ids.contains(id))id=base+"-"+QString::number(n++);ids.insert(id);return id;};
  const auto listId=unique("gallery"),detailId=unique("gallery-detail"),cardId=unique("gallery-card"),detailCardId=unique("gallery-detail-card");
  const int w=visual.value("width").toInt(1280),h=visual.value("height").toInt(720);
  auto node=[](QString id,QString kind,int x,int y,int width,int height){return QJsonObject{{"id",id},{"kind",kind},{"x",x},{"y",y},{"width",width},{"height",height}};};
  auto text=[&](QString id,QString value,int x,int y,int width,int size=22){auto e=node(id,"text",x,y,width,40);e["text"]=value;e["fontSize"]=size;e["color"]="#eee5d2";return e;};
  auto button=[&](QString id,QString label,QString action,int x,int y,int width=150){auto e=node(id,"button",x,y,width,44);e["text"]=label;e["action"]=action;e["fill"]="#304d51";e["hoverFill"]="#55736b";e["color"]="#fff2da";e["fontSize"]=18;e["radius"]=6;return e;};
  auto bind=[](QString child,QString property,QString field){return QJsonObject{{"elementId",child},{"property",property},{"field",field}};};
  auto bg=node("background","panel",0,0,w,h);bg["fill"]="#15252d";bg["anchorX"]="stretch";bg["anchorY"]="stretch";
  auto image=node("image","image",16,12,328,100);image["imageSource"]="asset";image["fit"]="contain";image["anchorX"]="stretch";
  auto cardBg=node("background","panel",0,0,360,256);cardBg["fill"]="#243b43";cardBg["anchorX"]="stretch";cardBg["anchorY"]="stretch";cardBg["radius"]=8;
  auto title=text("name","도감 제목",16,118,328);title["anchorX"]="stretch";
  auto status=text("status","해금 상태",16,154,328,16);status["anchorX"]="stretch";
  auto open=button("open","자세히 보기","galleryView",16,200,328);open["anchorX"]="stretch";open["anchorY"]="bottom";
  components.append(QJsonObject{{"id",cardId},{"label","도감 · 항목 카드"},{"width",360},{"height",256},{"elements",QJsonArray{cardBg,image,title,status,open}}});
  QJsonArray bindings{bind("name","text","name"),bind("status","text","statusText"),bind("image","imageAssetId","imageAssetId"),bind("image","visible","hasImage"),bind("open","entryId","entryId"),bind("open","enabled","unlocked")};
  auto heading=text("heading",project.value("gallery").toObject().value("title").toString("추억 도감"),32,24,w-260,30);
  auto summary=text("summary","",32,68,350,18);summary["binding"]="gallery-summary";
  auto error=text("error","",400,68,w-432,16);error["binding"]="gallery-error";error["color"]="#ffb4a3";
  auto close=button("close","본편으로","galleryClose",w-190,26);close["anchorX"]="right";
  QJsonArray elements{bg,heading,summary,error,close};
  int x=32,n=0;for(const auto& p:QList<QPair<QString,QString>>{{"all","전체"},{"cg","CG"},{"music","음악"},{"event","사건"},{"ending","엔딩"}}){auto e=button("tab-"+p.first,p.second,"galleryFilter",x,120,110);e["galleryFilter"]="kind:"+p.first;elements.append(e);x+=120;}
  x=32;for(const auto& p:QList<QPair<QString,QString>>{{"all","모든 기록"},{"unlocked","해금됨"},{"locked","미해금"}}){auto e=button("status-"+p.first,p.second,"galleryFilter",x,174,140);e["galleryFilter"]="status:"+p.first;elements.append(e);x+=150;}
  auto reset=button("reset","필터 초기화","galleryFilter",x,174,150);reset["galleryFilter"]="all";elements.append(reset);
  auto filters=text("filters","",650,174,w-682,16);filters["binding"]="gallery-filter";elements.append(filters);
  auto list=node("entries","collection",32,232,w-64,h-264);list["anchorX"]="stretch";list["anchorY"]="stretch";list["componentId"]=cardId;list["source"]=QJsonObject{{"type","galleryEntries"}};list["layout"]="grid";list["columns"]=3;list["gap"]=18;list["rowHeight"]=256;list["bindings"]=bindings;elements.append(list);
  screens.append(QJsonObject{{"id",listId},{"label","도감 · 목록"},{"elements",elements}});
  auto detailImage=node("image","image",24,55,w-112,h-330);detailImage["imageSource"]="asset";detailImage["fit"]="contain";detailImage["anchorX"]="stretch";detailImage["anchorY"]="stretch";
  auto detailName=text("name","기록 제목",24,0,w-112,26);
  auto record=text("record","기록",24,h-265,140,18);record["anchorY"]="bottom";
  auto replay=button("replay","원본 연출 회상","galleryReplay",24,h-210,190);replay["anchorY"]="bottom";
  auto music=button("music","음악 재생","galleryMusicToggle",230,h-210,190);music["anchorY"]="bottom";
  auto stop=button("stop","음악 정지","galleryMusicStop",436,h-210,150);stop["anchorY"]="bottom";
  auto variant=button("variant","다른 기록","galleryVariant",174,h-275,150);variant["anchorY"]="bottom";
  components.append(QJsonObject{{"id",detailCardId},{"label","도감 · 상세 카드"},{"width",w-64},{"height",h-144},{"elements",QJsonArray{detailName,detailImage,record,replay,music,stop,variant}}});
  auto detail=node("selection","collection",32,106,w-64,h-144);detail["componentId"]=detailCardId;detail["source"]=QJsonObject{{"type","gallerySelection"}};detail["layout"]="list";detail["rowHeight"]=h-144;detail["anchorX"]="stretch";detail["anchorY"]="stretch";
  detail["bindings"]=QJsonArray{bind("name","text","name"),bind("image","imageAssetId","imageAssetId"),bind("image","visible","hasImage"),bind("record","text","recordLabel"),bind("replay","visible","canReplay"),bind("replay","entryId","entryId"),bind("music","visible","isMusic"),bind("music","text","musicLabel"),bind("music","entryId","entryId"),bind("stop","visible","isMusic"),bind("stop","entryId","entryId"),bind("variant","visible","isNarrative")};
  screens.append(QJsonObject{{"id",detailId},{"label","도감 · 상세"},{"elements",QJsonArray{bg,button("back","목록으로","galleryBack",32,26),close,error,detail}}});
  // Entry controls are ordinary authored buttons, including on the title screen.
  for(int i=0;i<screens.size()-2;++i){auto screen=screens[i].toObject();if(screen.value("modal").toBool())continue;auto es=screen.value("elements").toArray();QSet<QString> used;for(auto e:es)used.insert(e.toObject().value("id").toString());QString id="open-gallery";while(used.contains(id))id+="-new";auto entry=button(id,"추억 도감","galleryOpen",w-190,88);entry["anchorX"]="right";es.append(entry);screen["elements"]=es;screens[i]=screen;}
  visual["screens"]=screens;visual["components"]=components;project["visual"]=visual;auto gallery=project.value("gallery").toObject();gallery["screenId"]=listId;gallery["detailScreenId"]=detailId;project["gallery"]=gallery;return project;
}
