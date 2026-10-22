#include "studio.hpp"
#include "components.hpp"
#include <QApplication>
#include <QClipboard>
#include <QDockWidget>
#include <QEventLoop>
#include <QJsonDocument>
#include <QMenu>
#include <QMimeData>
#include <QScopedValueRollback>
#include <QTimer>

namespace {
const char *mime = "application/x-raising-studio-elements";
QJsonValue remapElements(QJsonValue value, const QMap<QString, QString> &ids) {
  if (value.isArray()) { auto a=value.toArray(); for(int i=0;i<a.size();++i)a[i]=remapElements(a[i],ids); return a; }
  if (!value.isObject()) return value;
  auto o=value.toObject();
  for(auto it=o.begin();it!=o.end();++it) {
    if((it.key()=="parentId"||it.key()=="elementId")&&ids.contains(it.value().toString())) it.value()=ids[it.value().toString()];
    else it.value()=remapElements(it.value(),ids);
  }
  return o;
}
}

bool StudioWindow::waitForPendingEdits() {
  if(!bridge.busy())return true;
  QEventLoop loop; QTimer timeout; timeout.setSingleShot(true);
  connect(&timeout,&QTimer::timeout,&loop,&QEventLoop::quit);
  connect(this,&StudioWindow::updated,&loop,&QEventLoop::quit);
  timeout.start(15000); loop.exec();
  if(bridge.busy()){errorLabel->setText(tr("편집 처리가 끝나지 않았습니다. 창을 유지합니다."));return false;}
  return true;
}

void StudioWindow::showHierarchy() {
  showWorkspace({});
  if(auto panel=findChild<QDockWidget*>("hierarchyDock")){panel->show();panel->raise();hierarchySearch->clear();hierarchy->setFocus();}
}

void StudioWindow::setupBasicEditing(QMenu *edit) {
  auto copy=edit->addAction(tr("요소 복사"),QKeySequence::Copy,this,&StudioWindow::copyElements);copy->setObjectName("copyElements");
  auto paste=edit->addAction(tr("요소 붙여넣기"),QKeySequence::Paste,this,&StudioWindow::pasteElements);paste->setObjectName("pasteElements");
  copy->setToolTip(tr("선택한 요소와 자식 전체를 복사합니다. 같은 프로젝트의 다른 화면에도 붙여넣을 수 있습니다."));
  paste->setToolTip(copy->toolTip());
  redoAction->setShortcuts({QKeySequence::Redo,QKeySequence("Ctrl+Shift+Z")});
  edit->addSeparator();
  edit->addAction(tr("맨 앞으로"),QKeySequence("Ctrl+Shift+]"),this,[this]{moveLayer(1000);})->setObjectName("layerFront");
  edit->addAction(tr("맨 뒤로"),QKeySequence("Ctrl+Shift+["),this,[this]{moveLayer(-1000);})->setObjectName("layerBack");
  findChild<QAction*>("layerForward")->setShortcut(QKeySequence("Ctrl+]"));
  findChild<QAction*>("layerBackward")->setShortcut(QKeySequence("Ctrl+["));
  viewMenu->addAction(tr("계층 · 레이어 표시"),this,&StudioWindow::showHierarchy)->setObjectName("showHierarchy");
  viewMenu->addAction(tr("프로젝트 버전 관리…"),this,&StudioWindow::showVersionControl)->setObjectName("versionControl");
  hierarchy->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(hierarchy,&QWidget::customContextMenuRequested,this,[this,copy,paste](QPoint point){
    if(auto item=hierarchy->itemAt(point))hierarchy->setCurrentItem(item);
    QMenu menu(this);menu.addAction(copy);menu.addAction(paste);menu.addAction(findChild<QAction*>("duplicateElementAction"));menu.addSeparator();
    for(auto name:{"layerForward","layerBackward","layerFront","layerBack"})menu.addAction(findChild<QAction*>(name));
    menu.exec(hierarchy->viewport()->mapToGlobal(point));
  });
  connect(QApplication::clipboard(),&QClipboard::dataChanged,this,&StudioWindow::updateClipboardActions);
  connect(this,&StudioWindow::updated,this,&StudioWindow::updateClipboardActions);
  updateClipboardActions();
}

void StudioWindow::updateClipboardActions() {
  auto copy=findChild<QAction*>("copyElements"),paste=findChild<QAction*>("pasteElements");if(!copy||!paste)return;
  copy->setEnabled(!bridge.busy()&&activeWorkspace.isEmpty()&&!currentElement().isEmpty());
  const auto data=QApplication::clipboard()->mimeData();
  paste->setEnabled(!bridge.busy()&&activeWorkspace.isEmpty()&&!currentScreen().isEmpty()&&data&&data->hasFormat(mime));
}

void StudioWindow::copyElements() {
  if(bridge.busy()||!activeWorkspace.isEmpty()||currentElement().isEmpty())return;
  const auto elements=currentScreen().value("elements").toArray();QSet<QString> subtree{selection};
  bool changed=true;while(changed){changed=false;for(auto v:elements){auto e=v.toObject();auto id=e.value("id").toString();if(subtree.contains(e.value("parentId").toString())&&!subtree.contains(id)){subtree.insert(id);changed=true;}}}
  QJsonArray copied;for(auto v:elements){auto e=v.toObject();if(!subtree.contains(e.value("id").toString()))continue;
    if(e.value("id")==selection&&e.contains("parentId")){
      const auto visual=projectData().value("visual").toObject();const QSize size(visual.value("width").toInt(),visual.value("height").toInt());
      for(auto item:hierarchyCanvasElements(elements,size,size))if(item.toObject().value("id")==selection)for(auto key:{"x","y","width","height"})e[key]=item.toObject().value(key);
      e.remove("parentId");e.remove("anchorX");e.remove("anchorY");
    }
    copied.append(e);
  }
  auto data=new QMimeData;data->setData(mime,QJsonDocument(QJsonObject{{"format",1},{"project",view.value("root")},{"rootId",selection},{"elements",copied}}).toJson(QJsonDocument::Compact));QApplication::clipboard()->setMimeData(data);
  status->setText(tr("요소 %1개 복사됨 · 다른 화면에서도 붙여넣을 수 있습니다.").arg(copied.size()));
}

void StudioWindow::pasteElements() {
  if(bridge.busy()||!activeWorkspace.isEmpty()||currentScreen().isEmpty())return;
  const auto clipboard=QApplication::clipboard()->mimeData();if(!clipboard)return;
  const auto bytes=clipboard->data(mime);
  if(bytes.size()>2*1024*1024)return;
  const auto data=QJsonDocument::fromJson(bytes).object();
  if(data.value("format")!=1||data.value("project")!=view.value("root")){errorLabel->setText(tr("같은 프로젝트에서 복사한 UI 요소만 붙여넣을 수 있습니다."));return;}
  auto source=data.value("elements").toArray(),elements=currentScreen().value("elements").toArray();
  if(source.isEmpty()||source.size()+elements.size()>100){errorLabel->setText(tr("붙여넣기 후 화면 요소 수가 100개를 초과합니다."));return;}
  QSet<QString> used;for(auto e:elements)used.insert(e.toObject().value("id").toString());QMap<QString,QString> ids;
  for(auto v:source){const auto old=v.toObject().value("id").toString();if(old.isEmpty()||ids.contains(old))return;const auto base=old.left(45)+"-copy";int n=1;while(used.contains(base+QString::number(n)))++n;ids[old]=base+QString::number(n);used.insert(ids[old]);}
  const auto root=data.value("rootId").toString();if(!ids.contains(root))return;
  auto p=projectData(),visual=p.value("visual").toObject();
  for(auto v:source){auto e=remapElements(v,ids).toObject();const auto old=e.value("id").toString();e["id"]=ids[old];
    if(old==root){e.remove("parentId");e["label"]=e.value("label").toString(old)+tr(" 복사");e["x"]=qMax(0,qMin(visual.value("width").toInt()-e.value("width").toInt(),e.value("x").toInt()+16));e["y"]=qMax(0,qMin(visual.value("height").toInt()-e.value("height").toInt(),e.value("y").toInt()+16));}
    elements.append(e);
  }
  auto screens=visual.value("screens").toArray();for(int i=0;i<screens.size();++i){auto s=screens[i].toObject();if(s.value("id")==screenId){s["elements"]=elements;screens[i]=s;}}
  visual["screens"]=screens;p["visual"]=visual;const auto pasted=ids[root];
  // The shared revision-checked transaction owns validation, undo and redo.
  request("edit",{{"edit",QJsonObject{{"type","replace"},{"file",projectFile},{"data",p}}}},[this,pasted](QJsonObject reply){if(reply.value("error").toString().isEmpty())selectElement(pasted);});
}
