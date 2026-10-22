#include "studio.hpp"
#include "selection-edit.hpp"
#include "renderer-preview.hpp"
#include <QApplication>
#include <QTreeWidgetItemIterator>

void StudioWindow::selectElements(const QStringList &ids){
  selectedElements.clear();for(auto v:currentScreen()["elements"].toArray()){auto id=v.toObject()["id"].toString();if(ids.contains(id))selectedElements.append(id);}
  selection=selectedElements.value(0);filling=true;canvas->selectMany(selectedElements);
  QTreeWidgetItemIterator it(hierarchy);while(*it){(*it)->setSelected((*it)->data(0,Qt::UserRole+1).toString()==screenId&&selectedElements.contains((*it)->data(0,Qt::UserRole).toString()));++it;}
  filling=false;updateInspector();updateClipboardActions();
  rendererPreview->setDocuments(view["documents"].toArray());rendererPreview->setContext(projectData(),screenId,selection,view["revision"].toInt(),view["root"].toString(),previewLocaleField->currentData().toString());
}
void StudioWindow::replaceScreenElements(const QJsonArray &elements){
  auto p=projectData(),v=p["visual"].toObject();auto screens=v["screens"].toArray();for(int i=0;i<screens.size();++i){auto s=screens[i].toObject();if(s["id"]==screenId){if(s["elements"].toArray()==elements)return;s["elements"]=elements;screens[i]=s;}}v["screens"]=screens;p["visual"]=v;replaceProject(p);
}
void StudioWindow::editSelectedElements(){
  if(bridge.busy()||selectedElements.isEmpty())return;if(auto focus=QApplication::focusWidget())focus->clearFocus();if(!waitForPendingEdits())return;
  const auto revision=view["revision"],root=view["root"];const auto screen=screenId;const auto ids=selectedElements;const auto visual=projectData()["visual"].toObject();QJsonArray edited;
  if(!editSelectionDialog(this,visual,currentScreen()["elements"].toArray(),ids,{visual["width"].toInt(),visual["height"].toInt()},edited))return;
  if(bridge.busy()||view["revision"]!=revision||view["root"]!=root||screenId!=screen||selectedElements!=ids){errorLabel->setText(tr("편집 대상이 변경되었습니다. 다시 열어 주세요."));return;}
  replaceScreenElements(edited);
}
void StudioWindow::moveSelectedElements(QPoint delta){
  if(bridge.busy()||selectedElements.isEmpty())return;const auto v=projectData()["visual"].toObject();QJsonArray edited;QString error;
  if(!editSelection(currentScreen()["elements"].toArray(),selectedElements,{v["width"].toInt(),v["height"].toInt()},{{"mode","move"},{"dx",delta.x()},{"dy",delta.y()}},edited,error)){errorLabel->setText(error);return;}
  replaceScreenElements(edited);
}
void StudioWindow::openComponentSource(){
  if(bridge.busy()||selectedElements.isEmpty())return;QString component;
  for(auto v:currentScreen()["elements"].toArray()){auto e=v.toObject();if(!selectedElements.contains(e["id"].toString()))continue;const auto id=e["componentId"].toString();if(id.isEmpty()||(!component.isEmpty()&&id!=component)){errorLabel->setText(tr("같은 원본을 사용하는 부품 배치본을 선택하세요."));return;}component=id;}
  componentToOpen=component;editComponents();componentToOpen.clear();
}
