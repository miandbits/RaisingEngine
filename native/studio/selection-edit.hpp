#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QPoint>
#include <QSet>
#include <QSize>
#include <QStringList>
#include <algorithm>
#include <climits>
#include <functional>

// Editor-only operations on existing fields. No project/save format change.
inline QStringList selectionRoots(const QJsonArray &elements,const QStringList &ids) {
  QMap<QString,QJsonObject> rows;for(auto v:elements)rows[v.toObject()["id"].toString()]=v.toObject();
  QStringList roots;
  for(const auto &id:ids){if(!rows.contains(id)||roots.contains(id))continue;auto p=rows[id]["parentId"].toString();QSet<QString> seen{id};bool nested=false;
    while(!p.isEmpty()&&!seen.contains(p)){seen.insert(p);if(ids.contains(p)){nested=true;break;}p=rows.value(p)["parentId"].toString();}
    if(!nested)roots.append(id);
  }return roots;
}

inline bool editSelection(const QJsonArray &source,const QStringList &ids,QSize canvas,
                          const QJsonObject &operation,QJsonArray &result,QString &error) {
  error.clear();result=source;QMap<QString,QJsonObject> rows;for(auto v:source)rows[v.toObject()["id"].toString()]=v.toObject();
  if(ids.isEmpty()){error=QObject::tr("요소를 먼저 선택하세요.");return false;}
  for(const auto &id:ids)if(!rows.contains(id)){error=QObject::tr("선택한 요소가 변경되었습니다. 다시 선택하세요.");return false;}
  const auto roots=selectionRoots(source,ids);const auto mode=operation.value("mode").toString();
  const auto patch=operation.value("patch").toObject();
  const bool geometry=mode!="properties"||patch.contains("width")||patch.contains("height");
  auto origin=[&](QString id){QPoint p;QSet<QString> seen;while(!id.isEmpty()&&!seen.contains(id)){seen.insert(id);const auto e=rows.value(id);p+=QPoint(e["x"].toInt(),e["y"].toInt());id=e["parentId"].toString();}return p;};
  for(const auto &id:ids){auto cursor=id;QSet<QString> seen;while(!cursor.isEmpty()&&!seen.contains(cursor)){seen.insert(cursor);auto e=rows.value(cursor);
      if(geometry&&e["locked"].toBool()){error=QObject::tr("잠긴 요소 또는 부모가 포함되어 있습니다: %1").arg(cursor);return false;}
      if(geometry&&cursor!=id&&!e["flowLayout"].toObject().isEmpty()){error=QObject::tr("자동 배치 그룹의 자식은 그룹 배치 설정에서 조정하세요: %1").arg(cursor);return false;}
      if(patch.contains("locked")&&!patch["locked"].toBool()&&cursor!=id&&e["locked"].toBool()&&!ids.contains(cursor)){error=QObject::tr("부모 잠금을 먼저 해제하세요: %1").arg(cursor);return false;}
      cursor=e["parentId"].toString();
  }}
  // Alignment and equal gaps use the dimensions that will actually be applied.
  for(const auto &id:ids)for(const auto *key:{"width","height"})if(patch.contains(key))rows[id][key]=patch[key];
  QMap<QString,QPoint> target;
  for(const auto &id:roots)target[id]=origin(id);
  if(mode=="move")for(const auto &id:roots)target[id]+=QPoint(operation["dx"].toInt(),operation["dy"].toInt());
  else if(mode=="align"||mode=="distribute"){
    if(roots.size()<(mode=="align"?2:3)){error=QObject::tr("정렬은 2개, 간격 맞춤은 3개 이상의 독립 요소가 필요합니다.");return false;}
    const auto axis=operation["axis"].toString();const bool horizontal=axis=="x";int low=INT_MAX,high=INT_MIN;
    auto pos=[&](QString id){return horizontal?target[id].x():target[id].y();};auto size=[&](QString id){return rows[id][horizontal?"width":"height"].toInt();};
    for(const auto &id:roots){low=qMin(low,pos(id));high=qMax(high,pos(id)+size(id));}
    if(mode=="align")for(const auto &id:roots){const auto edge=operation["edge"].toString();int v=edge=="start"?low:edge=="end"?high-size(id):qRound((low+high-size(id))/2.0);if(horizontal)target[id].setX(v);else target[id].setY(v);}
    else{auto ordered=roots;std::stable_sort(ordered.begin(),ordered.end(),[&](QString a,QString b){return pos(a)<pos(b);});int total=0;for(auto id:ordered)total+=size(id);const double gap=double(high-low-total)/(ordered.size()-1);double at=low;for(auto id:ordered){if(horizontal)target[id].setX(qRound(at));else target[id].setY(qRound(at));at+=size(id)+gap;}}
  }else if(mode!="properties"){error=QObject::tr("지원하지 않는 일괄 편집입니다.");return false;}
  QJsonArray next;
  for(auto v:source){auto e=v.toObject();const auto id=e["id"].toString();if(ids.contains(id))for(auto it=patch.begin();it!=patch.end();++it){
      if(!QStringList{"width","height","fontSize","opacity","hidden","locked","styleId","fontAssetId"}.contains(it.key())){error=QObject::tr("지원하지 않는 일괄 속성입니다.");return false;}
      if(it.value().isNull())e.remove(it.key());else e[it.key()]=it.value();
    }
    if(mode!="properties"&&target.contains(id)){auto p=target[id]-origin(e.value("parentId").toString());e["x"]=p.x();e["y"]=p.y();}
    if(geometry&&ids.contains(id)){
      const bool nested=!e.value("parentId").toString().isEmpty();int x=e["x"].toInt(),y=e["y"].toInt(),w=e["width"].toInt(),h=e["height"].toInt();
      if(w<16||h<16||w>3840||h>2160||x<(nested?-3840:0)||y<(nested?-2160:0)||x>3840||y>2160||(!nested&&(x+w>canvas.width()||y+h>canvas.height()))){error=QObject::tr("일괄 편집 결과가 배치 범위를 벗어납니다: %1").arg(id);return false;}
    }
    next.append(e);
  }result=next;return true;
}

class QWidget;
bool editSelectionDialog(QWidget *parent,const QJsonObject &visual,const QJsonArray &elements,
                         const QStringList &ids,QSize size,QJsonArray &output);
