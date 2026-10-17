#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QSize>
#include <cmath>
#include <QSet>
#include <QRectF>
#include <functional>

// Reference rectangles retain their margins when their parent changes size.
inline QJsonObject anchoredElement(QJsonObject e, QSize reference, QSize actual) {
  for (const auto &axis : {QString("X"), QString("Y")}) {
    const bool horizontal = axis == "X";
    const auto position = horizontal ? "x" : "y", extent = horizontal ? "width" : "height";
    const double delta = horizontal ? actual.width()-reference.width() : actual.height()-reference.height();
    const auto anchor=e["anchor"+axis].toString();
    if(anchor=="center") e[position]=e[position].toDouble()+delta/2;
    if(anchor=="right"||anchor=="bottom") e[position]=qRound(e[position].toDouble()+delta);
    if(anchor=="stretch") e[extent]=qMax(16,qRound(e[extent].toDouble()+delta));
  }
  return e;
}
inline QRectF canvasClipRect(const QJsonObject& r){return {r.value("x").toDouble(),r.value("y").toDouble(),r.value("width").toDouble(),r.value("height").toDouble()};}
inline QJsonObject canvasRectJson(const QRectF& r){return {{"x",r.x()},{"y",r.y()},{"width",qMax(0.,r.width())},{"height",qMax(0.,r.height())}};}
inline void inheritCanvasElement(QJsonObject& child,const QJsonObject& parent) {
  auto context=parent.value("uiContext").toObject();const auto own=child.value("uiContext").toObject();
  for(auto it=own.begin();it!=own.end();++it)context[it.key()]=it.value();
  if(!context.isEmpty())child["uiContext"]=context;
  child["hidden"]=child.value("hidden").toBool()||parent.value("hidden").toBool();
  child["locked"]=child.value("locked").toBool()||parent.value("locked").toBool();
  child["opacity"]=child.value("opacity").toDouble(1)*parent.value("opacity").toDouble(1);
  for(const auto key:{"visibleCondition","enabledCondition"})if(parent.contains(key))child[key]=child.contains(key)?QJsonValue(QJsonObject{{"op","all"},{"conditions",QJsonArray{parent.value(key),child.value(key)}}}):parent.value(key);
  bool has=parent.contains("_clipRect");QRectF clip=canvasClipRect(parent.value("_clipRect").toObject());
  if(parent.value("clipChildren").toBool()){auto box=QRectF(parent.value("x").toDouble(),parent.value("y").toDouble(),parent.value("width").toDouble(),parent.value("height").toDouble());clip=has?clip.intersected(box):box;has=true;}
  if(child.contains("_clipRect")){auto r=canvasClipRect(child.value("_clipRect").toObject());clip=has?clip.intersected(r):r;has=true;}
  if(has)child["_clipRect"]=canvasRectJson(clip);
}
inline QJsonArray hierarchyCanvasElements(const QJsonArray& elements,QSize reference,QSize actual) {
  QJsonArray out;QSet<QString> visited;
  std::function<void(QString,QJsonObject,QSize,QSize,int)> walk;
  walk=[&](QString id,QJsonObject parent,QSize ref,QSize size,int depth){if(depth>32)return;
    for(const auto v:elements){auto raw=v.toObject();if(raw.value("parentId").toString()!=id)continue;const auto key=raw.value("id").toString();if(visited.contains(key))continue;visited.insert(key);
      auto e=anchoredElement(raw,ref,size);e.remove("parentId");e["_hasParent"]=!id.isEmpty();
      e["_parentX"]=parent.value("x").toDouble();e["_parentY"]=parent.value("y").toDouble();e["_parentWidth"]=size.width();e["_parentHeight"]=size.height();
      if(!id.isEmpty()){e["x"]=e.value("x").toDouble()+parent.value("x").toDouble();e["y"]=e.value("y").toDouble()+parent.value("y").toDouble();inheritCanvasElement(e,parent);}
      out.append(e);if(e.value("kind")=="group")walk(key,e,QSize(raw.value("width").toInt(),raw.value("height").toInt()),QSize(e.value("width").toInt(),e.value("height").toInt()),depth+1);
    }
  };walk({}, {},reference,actual,0);return out;
}
inline void translateCanvasElement(QJsonObject& e,double x,double y){e["x"]=e.value("x").toDouble()+x;e["y"]=e.value("y").toDouble()+y;if(e.contains("_clipRect")){auto r=canvasClipRect(e.value("_clipRect").toObject());r.translate(x,y);e["_clipRect"]=canvasRectJson(r);}}
inline QJsonArray canvasElements(const QJsonObject& visual,const QJsonArray& elements,QSize actual) {
  const QSize reference(visual["width"].toInt(1280),visual["height"].toInt(720));
  QJsonArray out;
  for(auto value:hierarchyCanvasElements(elements,reference,actual)){
    auto instance=value.toObject();
    if(instance["kind"]=="collection"){
      QJsonArray rows;const auto source=instance["source"].toObject();
      if(source["type"]=="table")for(auto t:visual["dataTables"].toArray())if(t.toObject()["id"]==source["tableId"])rows=t.toObject()["rows"].toArray();
      const bool placeholders=source["type"]!="table";
      if(placeholders)for(int n=0;n<3;n++)rows.append(QJsonObject{{"id",QString("preview-%1").arg(n+1)},{"values",QJsonObject{}}});
      int columns=instance["layout"]=="grid"?qMax(1,instance["columns"].toInt(1)):1;int gap=instance["gap"].toInt(8),rh=instance["rowHeight"].toInt(180);int cw=qMax(16,(instance["width"].toInt()-(columns-1)*gap)/columns);
      for(auto cv:visual["components"].toArray()){auto c=cv.toObject();if(c["id"]!=instance["componentId"])continue;
        rh=instance["rowHeight"].toInt(c["height"].toInt(180));int displayRow=0;
        for(int r=0;r<rows.size();r++){auto row=rows[r].toObject(),values=row["values"].toObject();values["id"]=row["id"];auto f=instance["rowFilter"].toObject();if(!placeholders&&!f.empty()){auto a=values[f["field"].toString()],b=f["value"];auto op=f["operator"].toString();bool pass=op=="eq"?a==b:op=="ne"?a!=b:op=="gt"?a.toDouble()>b.toDouble():op=="gte"?a.toDouble()>=b.toDouble():op=="lt"?a.toDouble()<b.toDouble():a.toDouble()<=b.toDouble();if(!pass)continue;}int y=(displayRow/columns)*(rh+gap);int x=(displayRow%columns)*(cw+gap);++displayRow;if(y+rh>instance["height"].toInt())break;
          QJsonArray boundChildren;for(auto ch:c["elements"].toArray()){auto child=ch.toObject();for(auto bv:instance["bindings"].toArray()){auto b=bv.toObject();if(b["elementId"]!=child["id"])continue;auto p=b["property"].toString(),field=b["field"].toString();if(p=="text"||p=="label"){auto v=values[field];QString s=placeholders?"{"+field+"}":v.isString()?v.toString():v.isBool()?(v.toBool()?"true":"false"):QString::number(v.toDouble());if(b.contains("format"))s=b["format"].toString().replace("{value}",s);child[p]=s;if(p=="label")child["text"]=s;}else if(p=="imageAssetId"&&!placeholders)child[p]=values[field];else if(p=="visible"&&!placeholders&&!values[field].toBool())child["hidden"]=true;}
            boundChildren.append(child);
          }
          for(auto ch:hierarchyCanvasElements(boundChildren,QSize(c["width"].toInt(),c["height"].toInt()),QSize(cw,rh))){auto child=ch.toObject();
            child["id"]=instance["id"].toString()+"__"+row["id"].toString()+"__"+child["id"].toString();child["_componentChild"]=true;
            translateCanvasElement(child,instance.value("x").toDouble()+x,instance.value("y").toDouble()+y);inheritCanvasElement(child,instance);out.append(child);
          }
        }
      }
      instance["kind"]="panel";instance["fill"]="#00000000";instance["borderWidth"]=0;out.append(instance);continue;
    }
    if(instance["kind"]!="component"){out.append(instance);continue;}
    for(auto v:visual["components"].toArray()) {
      auto component=v.toObject();if(component["id"]!=instance["componentId"])continue;
      QJsonArray children;
      for(auto childValue:component["elements"].toArray()) {
        auto child=childValue.toObject();const auto childId=child["id"].toString();
        for(auto o:instance["overrides"].toArray())if(o.toObject()["elementId"]==childId){auto values=o.toObject()["values"].toObject();for(auto it=values.begin();it!=values.end();++it)child[it.key()]=it.value();}
        children.append(child);
      }
      for(auto childValue:hierarchyCanvasElements(children,QSize(component["width"].toInt(),component["height"].toInt()),QSize(instance["width"].toInt(),instance["height"].toInt()))) {
        auto child=childValue.toObject();const auto childId=child.value("id").toString();
        child["id"]=instance["id"].toString()+"__"+childId;child["_componentChild"]=true;
        translateCanvasElement(child,instance.value("x").toDouble(),instance.value("y").toDouble());inheritCanvasElement(child,instance);out.append(child);
      }
    }
    // A transparent selection rectangle moves the complete instance as one item.
    instance["kind"]="panel";instance["fill"]="#00000000";instance["borderWidth"]=0;out.append(instance);
  }
  return out;
}
