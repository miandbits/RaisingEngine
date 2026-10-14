#include "studio.hpp"
#include "hierarchy-lock.hpp"
#include <QMessageBox>
#include <QSet>
#include <QSignalBlocker>
#include <QTreeWidgetItemIterator>

namespace {
QPoint elementOrigin(const QJsonArray &elements,QString id){QPoint p;QSet<QString> seen;while(!id.isEmpty()&&!seen.contains(id)){seen.insert(id);QJsonObject e;for(auto value:elements)if(value.toObject().value("id")==id)e=value.toObject();if(e.empty())break;p+=QPoint(e.value("x").toInt(),e.value("y").toInt());id=e.value("parentId").toString();}return p;}
}
void StudioWindow::rebuildHierarchy(){
  const QSignalBlocker blocker(hierarchy);
  const auto query=hierarchySearch->text().trimmed();
  const bool searching=!query.isEmpty();
  hierarchy->clear();
  hierarchy->setDragEnabled(!searching);hierarchy->setAcceptDrops(!searching);
  const auto screens=searching?projectData().value("visual").toObject().value("screens").toArray():QJsonArray{currentScreen()};
  int visibleScreens=0;
  for(const auto& screenValue:screens){
    const auto screen=screenValue.toObject();if(screen.isEmpty())continue;
    const auto owner=screen.value("id").toString();
    auto root=new QTreeWidgetItem(hierarchy,{screen.value("label").toString(owner)+(searching?" · "+owner:QString())});
    root->setData(0,Qt::UserRole+1,owner);
    root->setFlags(Qt::ItemIsEnabled|(searching?Qt::ItemIsSelectable:Qt::ItemIsDropEnabled));
    root->setData(0,Qt::UserRole+2,screen.value("label").toString()+" "+owner);
    QMap<QString,QTreeWidgetItem*> items;const auto elements=screen.value("elements").toArray();
    for(auto value:elements){
      const auto e=value.toObject();const auto id=e.value("id").toString();
      auto displayName=e.value("label").toString().trimmed();
      if(displayName.isEmpty())displayName=e.value("text").toString().trimmed();
      if(displayName.isEmpty())displayName=id;
      auto item=new QTreeWidgetItem({displayName+(e.value("locked").toBool()?tr(" [잠금]"):QString())+(e.value("hidden").toBool()?tr(" [숨김]"):QString())});
      item->setData(0,Qt::UserRole,id);item->setData(0,Qt::UserRole+1,owner);
      item->setData(0,Qt::UserRole+2,QStringList{id,e.value("label").toString(),e.value("text").toString(),e.value("kind").toString()}.join(" "));
      item->setToolTip(0,e.value("kind").toString()+" · "+id);
      const auto lockOwner=elementLockOwner(elements,id);
      if(!lockOwner.isEmpty()&&lockOwner!=id)item->setText(0,item->text(0)+tr(" [부모 잠금]"));
      item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|(!searching&&lockOwner.isEmpty()?Qt::ItemIsDragEnabled:Qt::NoItemFlags)|(!searching&&e.value("kind")=="group"&&lockOwner.isEmpty()?Qt::ItemIsDropEnabled:Qt::NoItemFlags));
      items[id]=item;
    }
    for(auto value:elements){
      const auto e=value.toObject();const auto id=e.value("id").toString(),parent=e.value("parentId").toString();
      QSet<QString> seen{id};bool valid=true;auto cursor=parent;
      while(!cursor.isEmpty()){
        if(seen.contains(cursor)){valid=false;break;}seen.insert(cursor);QString next;bool found=false;
        for(auto v:elements)if(v.toObject().value("id")==cursor){found=true;valid=valid&&v.toObject().value("kind")=="group";next=v.toObject().value("parentId").toString();}
        if(!found){valid=false;break;}cursor=next;
      }
      (valid&&items.contains(parent)?items[parent]:root)->addChild(items[id]);
      if(!valid)items[id]->setText(0,items[id]->text(0)+tr(" [부모 오류]"));
    }
    std::function<bool(QTreeWidgetItem*,bool)> filter=[&](QTreeWidgetItem* item,bool inherited){
      const bool matched=!searching||inherited||item->data(0,Qt::UserRole+2).toString().contains(query,Qt::CaseInsensitive);
      bool visible=matched;for(int j=0;j<item->childCount();j++)visible=filter(item->child(j),matched)||visible;
      item->setHidden(!visible);return visible;
    };
    if(filter(root,false))visibleScreens++;
  }
  if(!visibleScreens){auto empty=new QTreeWidgetItem(hierarchy,{searching?tr("검색 결과 없음"):tr("프로젝트에서 화면을 열어 주세요")});empty->setFlags(Qt::ItemIsEnabled);}
  hierarchy->expandAll();
  QTreeWidgetItemIterator it(hierarchy);while(*it){(*it)->setSelected((*it)->data(0,Qt::UserRole).toString()==selection&&(*it)->data(0,Qt::UserRole+1).toString()==screenId&&!selection.isEmpty());++it;}
}
void StudioWindow::addGroup(){
  if(bridge.busy()||currentScreen().empty())return;auto p=projectData(),v=p.value("visual").toObject();auto screens=v.value("screens").toArray();auto elements=currentScreen().value("elements").toArray();if(elements.size()>=100){errorLabel->setText(tr("화면당 최대 100개 요소입니다."));return;}QSet<QString> ids;for(auto e:elements)ids.insert(e.toObject().value("id").toString());int n=1;while(ids.contains("group-"+QString::number(n)))++n;selection="group-"+QString::number(n);elements.append(QJsonObject{{"id",selection},{"kind","group"},{"label",tr("새 UI 그룹")},{"x",0},{"y",0},{"width",v.value("width").toInt(1280)},{"height",v.value("height").toInt(720)},{"clipChildren",false}});for(int i=0;i<screens.size();i++){auto s=screens[i].toObject();if(s.value("id")==screenId){s["elements"]=elements;screens[i]=s;}}v["screens"]=screens;p["visual"]=v;replaceProject(p);
}
void StudioWindow::reparentElement(QString id,QString parentId){
  if(bridge.busy()||id.isEmpty())return;auto elements=currentScreen().value("elements").toArray();QJsonObject child,parent;int at=-1;for(int i=0;i<elements.size();i++){auto e=elements[i].toObject();if(e.value("id")==id){child=e;at=i;}if(e.value("id")==parentId)parent=e;}if(at<0)return;
  if(!parentId.isEmpty()&&(parent.empty()||parent.value("kind")!="group")){errorLabel->setText(tr("UI 그룹 안으로만 옮길 수 있습니다."));return;}
  QSet<QString> visited;auto cursor=parentId;while(!cursor.isEmpty()){if(cursor==id||visited.contains(cursor)){errorLabel->setText(tr("자신이나 자손을 부모로 지정할 수 없습니다."));return;}visited.insert(cursor);QString next;for(auto e:elements)if(e.toObject().value("id")==cursor)next=e.toObject().value("parentId").toString();cursor=next;}
  if(!elementLockOwner(elements,id).isEmpty()||(!parentId.isEmpty()&&!elementLockOwner(elements,parentId).isEmpty())){errorLabel->setText(tr("요소와 부모 그룹의 배치 잠금을 먼저 해제하세요."));updateInspector();rebuildHierarchy();return;}if(child.value("parentId").toString()==parentId)return;
  const auto origin=elementOrigin(elements,id)-elementOrigin(elements,parentId);if(parentId.isEmpty()&&(origin.x()<0||origin.y()<0)){errorLabel->setText(tr("화면 밖 요소입니다. 화면 안으로 이동한 뒤 그룹에서 꺼내세요."));return;}
  child["x"]=origin.x();child["y"]=origin.y();if(parentId.isEmpty())child.remove("parentId");else child["parentId"]=parentId;elements[at]=child;
  auto p=projectData(),v=p.value("visual").toObject();auto screens=v.value("screens").toArray();for(int i=0;i<screens.size();i++){auto s=screens[i].toObject();if(s.value("id")==screenId){s["elements"]=elements;screens[i]=s;}}v["screens"]=screens;p["visual"]=v;selection=id;replaceProject(p);
}
