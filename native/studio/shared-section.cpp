#include "shared-section.hpp"
#include "studio.hpp"
#include <QtWidgets>
#include <algorithm>
#include <functional>

namespace SharedSection {
namespace {
QJsonObject find(const QJsonArray& rows,const QString& id){for(auto v:rows)if(v.toObject().value("id")==id)return v.toObject();return {};}
QString fresh(const QJsonArray& rows,const QString& base){QSet<QString> ids;for(auto v:rows)ids.insert(v.toObject().value("id").toString());for(int n=1;;++n){auto id=base.left(48)+"-"+QString::number(n);if(!ids.contains(id))return id;}}
bool selectionDependent(const QJsonObject& element){
  if(element.contains("selectionKey")||element.contains("selectionBindings")||element.value("binding")=="selected-field")return true;
  // Instance overrides can introduce a dependency absent from the shared definition.
  for(auto entry:element.value("overrides").toArray()){
    const auto values=entry.toObject().value("values").toObject();
    if(values.contains("selectionKey")||values.contains("selectionBindings"))return true;
  }
  return false;
}
bool collectLeaves(const QJsonObject& visual,const QJsonArray& children,const QString& prefix,QStringList& leaves,QSet<QString> stack,int& budget,QString& error){
  if(stack.size()>8){error=QObject::tr("중첩 부품의 깊이를 확인하세요.");return false;}
  for(auto value:children){if(--budget<0){error=QObject::tr("부품 수 또는 요소 수 한도를 초과합니다.");return false;}const auto e=value.toObject();const auto kind=e.value("kind").toString(),id=e.value("id").toString();
    if(!QStringList{"panel","text","image","button","meter","group","input","component","schedule","stats","result"}.contains(kind)){error=QObject::tr("공유할 수 없는 위젯입니다: %1 (%2). 원래 화면에 유지하세요.").arg(id,kind);return false;}
    if(kind=="component"){
      const auto cid=e.value("componentId").toString();const auto c=find(visual.value("components").toArray(),cid);
      if(c.isEmpty()||stack.contains(cid)){error=QObject::tr("부품 참조 또는 순환을 먼저 수정하세요: %1").arg(cid);return false;}
      auto next=stack;next.insert(cid);if(!collectLeaves(visual,c.value("elements").toArray(),prefix+id+"__",leaves,next,budget,error))return false;
    }else if(kind!="panel"&&kind!="group")leaves.append(prefix+id);
    // A row-selected value cannot acquire its original collection context on another screen.
    if(selectionDependent(e)){
      error=QObject::tr("목록 선택에 의존하는 요소는 함께 공유할 수 없습니다: %1").arg(id);return false;
    }
  }return true;
}
QString remapped(QString id,const QMap<QString,QString>& map){if(map.contains(id))return map.value(id);for(auto it=map.begin();it!=map.end();++it)if(id.startsWith(it.key()+"__"))return it.value()+id.mid(it.key().size());return id;}
void remapReading(QJsonObject& screen,const QMap<QString,QString>& map){
  if(!screen.contains("readableLayout"))return;auto layout=screen.value("readableLayout").toObject();auto items=layout.value("items").toArray();
  for(int i=0;i<items.size();++i){auto item=items[i].toObject();item["elementId"]=remapped(item.value("elementId").toString(),map);items[i]=item;}layout["items"]=items;
  if(layout.contains("actions")){auto actions=layout.value("actions").toArray();for(int i=0;i<actions.size();++i)actions[i]=remapped(actions[i].toString(),map);layout["actions"]=actions;}
  if(layout.contains("primaryAction"))layout["primaryAction"]=remapped(layout.value("primaryAction").toString(),map);screen["readableLayout"]=layout;
}
void remapPlannerTargets(QJsonObject& visual,const QString& screenId,const QMap<QString,QString>& map){
  const auto definitions=visual.value("components").toArray();
  auto remapElement=[&](QJsonObject element){
    if(element.value("action")=="openPlanner"&&element.value("screenId")==screenId)
      element["plannerElementId"]=remapped(element.value("plannerElementId").toString(),map);
    auto overrides=element.value("overrides").toArray();
    const auto children=find(definitions,element.value("componentId").toString()).value("elements").toArray();
    for(int i=0;i<overrides.size();++i){auto override=overrides[i].toObject(),values=override.value("values").toObject();const auto base=find(children,override.value("elementId").toString());auto effective=base;
      for(auto it=values.begin();it!=values.end();++it)effective[it.key()]=it.value();
      if(effective.value("action")=="openPlanner"){
        const auto old=effective.value("plannerElementId").toString();const auto next=effective.value("screenId")==screenId?remapped(old,map):old;
        const auto inherited=base.value("action")=="openPlanner"&&base.value("screenId")==screenId?remapped(base.value("plannerElementId").toString(),map):base.value("plannerElementId").toString();
        // Keep inherited links inherited. Add an override only when this instance
        // targets another screen whose local path must not follow the source edit.
        if(values.contains("plannerElementId")||next!=inherited)values["plannerElementId"]=next;
        override["values"]=values;overrides[i]=override;
      }
    }
    if(element.contains("overrides"))element["overrides"]=overrides;return element;
  };
  for(const auto key:{"screens","components"}){auto rows=visual.value(key).toArray();for(int i=0;i<rows.size();++i){auto owner=rows[i].toObject();auto elements=owner.value("elements").toArray();for(int j=0;j<elements.size();++j)elements[j]=remapElement(elements[j].toObject());owner["elements"]=elements;rows[i]=owner;}visual[key]=rows;}
}
void localizePaths(QJsonObject& project,const QString& oldPrefix,const QString& newPrefix,bool move){
  auto localization=project.value("localization").toObject();if(localization.isEmpty())return;
  auto entries=localization.value("entries").toArray();QSet<QString> known;for(auto v:entries)known.insert(v.toObject().value("key").toString());QJsonArray extra;
  for(int i=0;i<entries.size();++i){auto e=entries[i].toObject();auto key=e.value("key").toString();if(!key.startsWith(oldPrefix+"/"))continue;const auto next=newPrefix+key.mid(oldPrefix.size());e["key"]=next;if(move)entries[i]=e;else if(!known.contains(next)){extra.append(e);known.insert(next);}}
  for(auto e:extra)entries.append(e);localization["entries"]=entries;project["localization"]=localization;
}
}

bool prepare(const QJsonObject& project,const QString& screenId,const QStringList& selection,Source& source,QString& error){
  source={};error.clear();const auto visual=project.value("visual").toObject();const auto screen=find(visual.value("screens").toArray(),screenId);const auto elements=screen.value("elements").toArray();
  if(screen.isEmpty()||selection.isEmpty()){error=QObject::tr("레이어 목록에서 공유할 영역이나 부품 배치본을 선택하세요.");return false;}
  source.screenId=screenId;for(auto id:selection){if(find(elements,id).isEmpty()){error=QObject::tr("선택한 요소가 변경되었습니다. 다시 선택하세요.");return false;}source.selected.insert(id);}
  bool added=true;while(added){added=false;for(auto v:elements){auto e=v.toObject();if(source.selected.contains(e.value("parentId").toString())&&!source.selected.contains(e.value("id").toString())){source.selected.insert(e.value("id").toString());added=true;}}}
  QJsonArray selected;for(auto v:elements){auto e=v.toObject();if(!source.selected.contains(e.value("id").toString()))continue;
    if(e.contains("parentId")&&!source.selected.contains(e.value("parentId").toString())){error=QObject::tr("부모의 조건과 배치를 유지하려면 부모 그룹 전체를 선택하세요: %1").arg(e.value("parentId").toString());return false;}
    selected.append(e);
  }
  if(selected.size()==1&&selected.first().toObject().value("kind")=="component"){
    const auto selectedInstance=selected.first().toObject();if(selectionDependent(selectedInstance)){error=QObject::tr("목록 선택에 의존하는 요소는 함께 공유할 수 없습니다: %1").arg(selectedInstance.value("id").toString());return false;}
    source.instance=selected.first().toObject();source.componentId=source.instance.value("componentId").toString();source.definition=find(visual.value("components").toArray(),source.componentId);
    if(source.definition.isEmpty()){error=QObject::tr("부품 참조 또는 순환을 먼저 수정하세요: %1").arg(source.componentId);return false;}
  }else{
    source.convert=true;
    if(selected.size()>100||visual.value("components").toArray().size()>=256){error=QObject::tr("부품 수 또는 요소 수 한도를 초과합니다.");return false;}
    // Keep painter order: roots not in the section must not be interleaved with selected roots.
    bool seen=false,ended=false;for(auto v:elements){auto e=v.toObject();if(e.contains("parentId"))continue;const bool chosen=source.selected.contains(e.value("id").toString());if(chosen&&ended){error=QObject::tr("겹침 순서를 유지하려면 레이어 목록에서 연속된 최상위 영역을 선택하세요.");return false;}if(chosen)seen=true;else if(seen)ended=true;}
    for(auto v:selected){const auto e=v.toObject();
      for(const auto key:{"layout","gap","rowFilter","showChoiceEffects","rowHeight","bindings","extension","columns","source"})if(e.contains(key)){error=QObject::tr("부품에서 지원하지 않는 설정이 있습니다: %1 / %2").arg(e.value("id").toString(),key);return false;}
    }
    source.componentId=fresh(visual.value("components").toArray(),"shared-section");
    // Full canvas reference preserves every child's existing responsive anchor and group geometry.
    source.definition={{"id",source.componentId},{"label",QObject::tr("공유 화면 영역")},{"width",visual.value("width")},{"height",visual.value("height")},{"elements",selected}};
    source.instance={{"kind","component"},{"componentId",source.componentId},{"x",0},{"y",0},{"width",visual.value("width")},{"height",visual.value("height")},{"anchorX","stretch"},{"anchorY","stretch"}};
  }
  int budget=10000;if(!collectLeaves(visual,source.definition.value("elements").toArray(),{},source.leaves,{source.componentId},budget,error))return false;
  return true;
}

bool apply(const QJsonObject& project,const Source& source,const QString& label,const QList<Target>& targets,Review& review,QString& error){
  review={};error.clear();if(source.convert&&label.trimmed().isEmpty()){error=QObject::tr("공유 영역의 이름을 입력하세요.");return false;}
  auto next=project,visual=next.value("visual").toObject();auto screens=visual.value("screens").toArray();auto components=visual.value("components").toArray();
  if(source.convert){auto definition=source.definition;definition["label"]=label.trimmed();components.append(definition);visual["components"]=components;}
  QMap<QString,QString> convertedReferences;
  QSet<QString> requested;for(auto target:targets)requested.insert(target.screenId);
  if(source.convert&&!requested.contains(source.screenId)){error=QObject::tr("원본 화면을 공유 영역으로 전환해야 합니다.");return false;}
  for(auto target:targets){
    int at=-1;for(int i=0;i<screens.size();++i)if(screens[i].toObject().value("id")==target.screenId)at=i;
    if(at<0){error=QObject::tr("대상 화면이 없습니다: %1").arg(target.screenId);return false;}
    auto screen=screens[at].toObject();auto elements=screen.value("elements").toArray();const auto display=screen.value("label").toString(target.screenId)+" ("+target.screenId+")";
    bool exists=false;for(auto v:elements)if(v.toObject().value("componentId")==source.componentId)exists=true;
    if(exists){++review.skipped;review.lines.append(QObject::tr("%1 · 건너뜀: 이미 같은 부품이 있습니다.").arg(display));continue;}
    const bool convert=source.convert&&target.screenId==source.screenId;
    if(!convert&&elements.size()>=1000){error=QObject::tr("화면 요소 수 한도를 초과합니다: %1").arg(display);return false;}
    auto instance=source.instance;const auto instanceId=fresh(elements,source.componentId);instance["id"]=instanceId;if(source.convert)instance["label"]=label.trimmed();
    QJsonArray kept;QMap<QString,QString> remap;bool inserted=false;
    for(auto v:elements){auto e=v.toObject();const auto id=e.value("id").toString();if(convert&&source.selected.contains(id)){remap[id]=instanceId+"__"+id;if(!inserted){kept.append(instance);inserted=true;}continue;}kept.append(e);}
    if(!inserted)kept.append(instance);screen["elements"]=kept;
    if(convert){
      convertedReferences=remap;
      remapReading(screen,remap);review.sourceInstanceId=instanceId;
      auto resume=visual.value("resumeContext").toObject();if(resume.value("screenId")==source.screenId){auto ids=resume.value("elementIds").toArray();for(int i=0;i<ids.size();++i)ids[i]=remapped(ids[i].toString(),remap);resume["elementIds"]=ids;visual["resumeContext"]=resume;}
      for(auto id:source.selected)localizePaths(next,"/project/visual/screens/@"+source.screenId+"/elements/@"+id,"/project/visual/components/@"+source.componentId+"/elements/@"+id,true);
    }else if(!source.convert){localizePaths(next,"/project/visual/screens/@"+source.screenId+"/elements/@"+source.instance.value("id").toString(),"/project/visual/screens/@"+target.screenId+"/elements/@"+instanceId,false);}
    QString reading;
    if(screen.contains("readableLayout")){
      auto layout=screen.value("readableLayout").toObject();auto items=layout.value("items").toArray();QSet<QString> mapped;
      for(auto v:items)mapped.insert(v.toObject().value("elementId").toString());for(auto v:layout.value("actions").toArray())mapped.insert(v.toString());
      int missing=0,appended=0;for(auto leaf:source.leaves){const auto id=instanceId+"__"+leaf;if(mapped.contains(id))continue;++missing;if(target.appendReading&&!convert){items.append(QJsonObject{{"elementId",id}});++appended;}}
      if(items.size()>1000){error=QObject::tr("읽기 항목 수 한도를 초과합니다: %1").arg(display);return false;}
      layout["items"]=items;screen["readableLayout"]=layout;
      reading=convert?QObject::tr("원본 읽기 순서·포함 항목 유지 · 기존 제외 항목 %1개").arg(missing):target.appendReading?QObject::tr("읽기 본문에 %1개 추가 · 기존 하단 행동 유지").arg(appended):QObject::tr("주의: 읽기 배치에서 %1개 항목이 표시되지 않습니다.").arg(missing);
    }else reading=QObject::tr("읽기 배치 없음 · 휴대폰에서도 기존 캔버스 사용");
    review.lines.append(QObject::tr("%1 · %2 · 위치 (%3, %4), 크기 %5 × %6\n%7").arg(display,instanceId).arg(instance.value("x").toInt()).arg(instance.value("y").toInt()).arg(instance.value("width").toInt()).arg(instance.value("height").toInt()).arg(reading));
    screens[at]=screen;++review.added;
  }
  if(!review.added){error=QObject::tr("새로 배치할 대상 화면을 선택하세요. 같은 부품이 있는 화면은 건너뜁니다.");return false;}
  visual["screens"]=screens;if(!convertedReferences.isEmpty())remapPlannerTargets(visual,source.screenId,convertedReferences);next["visual"]=visual;review.project=next;return true;
}
}

void StudioWindow::shareScreenSection(){
  if(bridge.busy()||projectData().isEmpty())return;if(auto focus=QApplication::focusWidget())focus->clearFocus();if(!waitForPendingEdits())return;
  const auto original=projectData();const auto openedRevision=view.value("revision"),openedRoot=view.value("root");const auto openedScreen=screenId;const auto openedSelection=selectedElements;
  SharedSection::Source source;QString failure;
  if(!SharedSection::prepare(original,screenId,selectedElements,source,failure)){errorLabel->setText(failure);return;}
  QDialog dialog(this);dialog.setObjectName("sharedSectionDialog");dialog.setWindowTitle(tr("화면 영역을 여러 화면에 공유…"));dialog.resize(880,640);
  auto outer=new QVBoxLayout(&dialog);auto body=new QWidget;auto layout=new QVBoxLayout(body);auto scroll=new QScrollArea;scroll->setObjectName("sharedSectionScroll");scroll->setWidgetResizable(true);scroll->setWidget(body);outer->addWidget(scroll,1);
  auto help=new QLabel(source.convert?tr("선택 영역을 하나의 부품 원본으로 전환합니다. 좌표·앵커·조건·데이터 연결은 유지됩니다. 원본 편집은 모든 배치본에 반영됩니다."):tr("선택한 부품 원본을 다른 화면에도 배치합니다. 배치본의 덮어쓰기와 조건도 함께 유지됩니다."));help->setWordWrap(true);layout->addWidget(help);
  auto form=new QFormLayout;auto name=new QLineEdit(source.definition.value("label").toString());name->setObjectName("sharedSectionName");name->setMaxLength(100);name->setReadOnly(!source.convert);form->addRow(tr("공유 영역 이름"),name);layout->addLayout(form);
  auto targets=new QTableWidget;targets->setObjectName("sharedSectionTargets");targets->setColumnCount(2);targets->setHorizontalHeaderLabels({tr("대상 화면"),tr("휴대폰 읽기 배치")});targets->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);targets->verticalHeader()->hide();targets->setMinimumHeight(180);targets->setEditTriggers(QAbstractItemView::NoEditTriggers);layout->addWidget(targets);
  const auto screens=original.value("visual").toObject().value("screens").toArray();
  for(auto v:screens){auto s=v.toObject();const auto id=s.value("id").toString();bool exists=false;for(auto e:s.value("elements").toArray())if(e.toObject().value("componentId")==source.componentId)exists=true;
    int row=targets->rowCount();targets->insertRow(row);auto item=new QTableWidgetItem(s.value("label").toString(id)+" ("+id+")"+(exists?tr(" · 이미 배치됨"):QString()));item->setData(Qt::UserRole,id);item->setCheckState(source.convert&&id==openedScreen?Qt::Checked:Qt::Unchecked);if(exists||(source.convert&&id==openedScreen))item->setFlags(item->flags()&~Qt::ItemIsEnabled);targets->setItem(row,0,item);
    auto choice=new QComboBox;choice->setObjectName("sharedReading-"+id);choice->addItem(tr("지원 항목을 읽기 본문 끝에 추가"),true);choice->addItem(tr("캔버스만 · 읽기 목록에 추가 안 함"),false);if(!s.contains("readableLayout")){choice->clear();choice->addItem(tr("읽기 배치 없음 · 캔버스 유지"),false);choice->setEnabled(false);}if(source.convert&&id==openedScreen){choice->clear();choice->addItem(tr("원본 읽기 목록 유지 · 참조만 갱신"),false);choice->setEnabled(false);}if(exists)choice->setEnabled(false);targets->setCellWidget(row,1,choice);
  }
  auto readingHelp=new QLabel(tr("읽기 본문에는 문구·이미지·버튼·게이지·입력·일정·능력치·성장 결과를 추가합니다. 버튼도 본문에 배치하며, 원본의 읽기 설정과 기존 하단 행동·대표 행동은 유지합니다. 장식 패널·그룹은 읽기 배치에 추가하지 않습니다. 겹침과 휴대폰 배치는 적용 후 미리보기에서 확인하세요."));readingHelp->setWordWrap(true);layout->addWidget(readingHelp);
  auto reviewText=new QPlainTextEdit;reviewText->setObjectName("sharedSectionReview");reviewText->setReadOnly(true);reviewText->setMinimumHeight(120);layout->addWidget(reviewText);
  auto error=new QLabel;error->setObjectName("sharedSectionError");error->setWordWrap(true);outer->addWidget(error);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel);buttons->setObjectName("sharedSectionButtons");auto reviewButton=buttons->addButton(tr("배치 검토"),QDialogButtonBox::ActionRole);reviewButton->setObjectName("reviewSharedSection");auto applyButton=buttons->button(QDialogButtonBox::Apply);applyButton->setEnabled(false);outer->addWidget(buttons);
  SharedSection::Review reviewed;bool ready=false;
  auto invalidate=[&]{ready=false;applyButton->setEnabled(false);reviewText->clear();error->clear();};
  connect(name,&QLineEdit::textChanged,&dialog,invalidate);connect(targets,&QTableWidget::itemChanged,&dialog,invalidate);for(auto combo:targets->findChildren<QComboBox*>())connect(combo,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,invalidate);
  connect(reviewButton,&QPushButton::clicked,&dialog,[&]{QList<SharedSection::Target> chosen;for(int row=0;row<targets->rowCount();++row)if(targets->item(row,0)->checkState()==Qt::Checked)chosen.append({targets->item(row,0)->data(Qt::UserRole).toString(),qobject_cast<QComboBox*>(targets->cellWidget(row,1))->currentData().toBool()});QString message;ready=SharedSection::apply(original,source,name->text(),chosen,reviewed,message);error->setText(message);reviewText->setPlainText(reviewed.lines.join("\n\n"));applyButton->setEnabled(ready);});
  connect(applyButton,&QPushButton::clicked,&dialog,[&]{if(!ready)return;if(bridge.busy()||view.value("revision")!=openedRevision||view.value("root")!=openedRoot||screenId!=openedScreen||selectedElements!=openedSelection){error->setText(tr("편집 대상이 변경되었습니다. 다시 열어 주세요."));applyButton->setEnabled(false);return;}dialog.accept();});connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  QTimer::singleShot(0,&dialog,[&]{if(auto screen=dialog.screen()){const auto area=screen->availableGeometry();dialog.resize(qMin(dialog.width(),qMax(320,area.width()-32)),qMin(dialog.height(),qMax(300,area.height()-64)));}});
  if(dialog.exec()!=QDialog::Accepted)return;if(!reviewed.sourceInstanceId.isEmpty()){selection=reviewed.sourceInstanceId;selectedElements={selection};}replaceProject(reviewed.project);
}
