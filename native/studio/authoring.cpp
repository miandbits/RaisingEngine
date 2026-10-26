#include "rule-extensions.hpp"
#include "control-style-editor.hpp"
#include "studio.hpp"
#include "hierarchy-lock.hpp"
#include "tabular-input.hpp"
#include "logic-editor.hpp"
#include "relationship-editor.hpp"
#include <QSignalBlocker>
#include "plugin-host.hpp"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QImageReader>
#include <QJsonDocument>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>
#include <limits>
#include <functional>

void StudioWindow::editWidgetSettings() {
  if (bridge.busy() || currentElement().isEmpty()) return;
  if (currentElement()["kind"] == "collection") { editDataUi(); return; }
  if (currentElement()["kind"] == "policies") { editRules(); return; }
  if (QStringList{"services", "diets", "sell"}.contains(currentElement()["kind"].toString())) { editFacilities(); return; }
  auto value = currentElement();
  const auto openedRevision=view.value("revision"),openedRoot=view.value("root");
  QDialog dialog(this); dialog.setObjectName("widgetSettingsDialog");
  dialog.setWindowTitle(tr("위젯 구성")); dialog.resize(720, 600);
  auto shell = new QVBoxLayout(&dialog);
  auto scroll=new QScrollArea;scroll->setObjectName("widgetSettingsScroll");scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);shell->addWidget(scroll,1);
  auto body=new QWidget;scroll->setWidget(body);auto layout=new QVBoxLayout(body);layout->setContentsMargins(0,0,0,0);layout->setSizeConstraint(QLayout::SetMinimumSize);
  auto form = new QFormLayout;form->setRowWrapPolicy(QFormLayout::WrapLongRows);layout->addLayout(form);
  auto imagePicker=[&](const QString& selected,const QString& objectName){
    auto field=new QComboBox;field->setObjectName(objectName);field->addItem(tr("아이콘 없음"),QString());field->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);field->setMinimumContentsLength(16);
    for(const auto& row:assets){const auto asset=row.toObject();if(asset["kind"]=="image")field->addItem(asset["id"].toString(),asset["id"].toString());}
    if(!selected.isEmpty()&&field->findData(selected)<0)field->addItem(tr("찾을 수 없음: %1").arg(selected),selected);
    field->setCurrentIndex(qMax(0,field->findData(selected)));return field;
  };
  auto appearance = new QComboBox;
  appearance->setObjectName("widgetTheme");
  appearance->addItem(tr("기본"), "default"); appearance->addItem(tr("고전 프레임"), "ornate");
  appearance->setCurrentIndex(appearance->findData(projectData()["visual"].toObject()["widgetTheme"].toString("default")));
  form->addRow(tr("프로젝트 위젯 테마"), appearance);
  QMap<QString,QComboBox*> frameFields;
  for(const auto &key: {QString("frameAssetId"),QString("entryFrameAssetId")}) {
    auto field=imagePicker(value[key].toString(),"widget-"+key);field->setItemText(0,tr("기본 프레임"));frameFields[key]=field;
    form->addRow(key=="frameAssetId"?tr("분류 메뉴 프레임"):tr("활동·상품 카드 프레임"),field);
  }
  auto confirm = new QCheckBox(tr("상품 구매 전 확인 창"));
  confirm->setChecked(projectData()["visual"].toObject()["confirmPurchases"].toBool()); form->addRow(confirm);
  auto mode = new QComboBox; mode->setObjectName("widgetLayout"); QString key;
  const auto kind = value["kind"].toString();
  if (kind == "schedule") {key="scheduleLayout"; mode->addItem(tr("일정 칸 목록"),"columns"); mode->addItem(tr("월간 달력"),"calendar"); mode->addItem(tr("분류 → 활동 메뉴"),"menu"); mode->addItem(tr("활동 카드 · 계획 · 예상 변화"),"board");mode->addItem(tr("활동 보드 · 카드와 일정 칸"),"activity-board");}
  if (kind == "stats") {key="statsLayout"; mode->addItem(tr("능력 막대"),"bars"); mode->addItem(tr("압축 표"),"compact");}
  if (kind == "shop") {key="shopLayout"; mode->addItem(tr("목록"),"list"); mode->addItem(tr("상품 카드"),"cards");}
  if (kind == "result") {key="resultLayout"; mode->addItem(tr("활동 결과 요약"),"summary"); mode->addItem(tr("연출에 맞춰 변화만 표시"),"changes");}
  if (kind == "choices") {key="choiceLayout"; mode->addItem(tr("기본 선택지 목록"),"list"); mode->addItem(tr("번호가 있는 선택 카드"),"cards");}
  if (!key.isEmpty()) {mode->setCurrentIndex(qMax(0,mode->findData(value[key].toString())));form->addRow(tr("표시 방식"),mode);}
  else mode->hide();
  auto boardHelp=new QLabel(tr("활동 보드는 카드를 살펴본 뒤 일정 칸에 적용합니다. 모든 활동 필터는 항상 제공되며, 분류에 없는 활동도 선택할 수 있습니다. 다인 육성은 지원하지 않습니다."));boardHelp->setObjectName("activityBoardHelp");boardHelp->setWordWrap(true);
  if(kind=="schedule"){form->addRow(boardHelp);boardHelp->setVisible(mode->currentData()=="activity-board");connect(mode,&QComboBox::currentIndexChanged,&dialog,[=]{boardHelp->setVisible(mode->currentData()=="activity-board");});}else boardHelp->hide();
  auto controlStyle=new QComboBox;controlStyle->setObjectName("widgetControlStyle");fillControlStyles(controlStyle,projectData()["visual"].toObject(),value.value("controlStyleId").toString());
  auto colorHelp=new QLabel(controlStyleHelp());colorHelp->setWordWrap(true);if(supportsControlStyle(kind)){form->addRow(tr("컨트롤 색 스타일"),controlStyle);form->addRow(colorHelp);}else{controlStyle->hide();colorHelp->hide();}
  auto autoLabel = new QCheckBox(tr("진행 버튼 이름을 현재 단계에 맞춰 자동 변경"));
  auto phaseForecast = new QCheckBox(tr("일정 전체 예상 결과 표시"));
  phaseForecast->setObjectName("showPhaseForecast");phaseForecast->setChecked(value.value("showPhaseForecast").toBool());
  auto forecastHelp=new QLabel(tr("비용, 회복, 정기 변화를 미리 확인합니다. 확률과 이야기 선택은 확정 결과로 표시하지 않습니다."));forecastHelp->setWordWrap(true);
  if(kind=="schedule"){form->addRow(phaseForecast);form->addRow(forecastHelp);}else{phaseForecast->hide();forecastHelp->hide();}
  auto activityRules = new QCheckBox(tr("일정 칸·다인 배정에 비용과 기본 효과 표시 (최종 결과 예측 아님)"));
  activityRules->setObjectName("showActivityRules");
  activityRules->setChecked(value.value("showActivityRules").toBool());
  if(kind=="schedule")form->addRow(activityRules);else activityRules->hide();
  auto activityDescription = new QCheckBox(tr("일정 칸 아래에 선택한 활동 설명 표시"));
  auto choiceEffects = new QCheckBox(tr("선택지 아래에 효과와 후속 안내 표시"));
  choiceEffects->setObjectName("showChoiceEffects");choiceEffects->setChecked(value.value("showChoiceEffects").toBool());
  if(kind=="choices")form->addRow(choiceEffects);else choiceEffects->hide();
  activityDescription->setObjectName("showActivityDescription");
  activityDescription->setChecked(value.value("showActivityDescription").toBool());
  if(kind=="schedule")form->addRow(activityDescription);else activityDescription->hide();
  bool boardDefaultsApplied=false;
  if(kind=="schedule")connect(mode,&QComboBox::currentIndexChanged,&dialog,[&]{
    if(mode->currentData()!="activity-board"||value.value("scheduleLayout")=="activity-board"||boardDefaultsApplied)return;
    if(!value.contains("showActivityRules"))activityRules->setChecked(true);
    if(!value.contains("showActivityDescription"))activityDescription->setChecked(true);
    boardDefaultsApplied=true;
  });
  autoLabel->setChecked(value.value("autoLabel").toBool(true)); if(kind=="button")form->addRow(autoLabel); else autoLabel->hide();
  auto activityIcons=value.value("activityIcons").toArray();
  if(kind=="schedule"){
    auto iconsButton=new QPushButton(tr("활동별 아이콘 선택…"));iconsButton->setObjectName("scheduleIconsButton");form->addRow(iconsButton);
    connect(iconsButton,&QPushButton::clicked,&dialog,[&]{
      QDialog iconsDialog(&dialog);iconsDialog.setObjectName("scheduleIconsDialog");iconsDialog.setWindowTitle(tr("활동별 아이콘"));iconsDialog.resize(540,400);
      auto iconsLayout=new QVBoxLayout(&iconsDialog);auto scroll=new QScrollArea;scroll->setWidgetResizable(true);auto content=new QWidget;auto fields=new QFormLayout(content);scroll->setWidget(content);iconsLayout->addWidget(scroll);QMap<QString,QComboBox*> iconFields;
      for(const auto& row:view["documents"].toArray())if(row.toObject()["kind"]=="activity"){
        const auto activity=row.toObject()["data"].toObject();const auto id=activity["id"].toString();QString selectedIcon;for(const auto& icon:activityIcons)if(icon.toObject()["activityId"]==id)selectedIcon=icon.toObject()["imageAssetId"].toString();auto field=imagePicker(selectedIcon,"scheduleIcon-"+id);
        fields->addRow(activity["label"].toString(),field);iconFields[id]=field;
      }
      auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);iconsLayout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&iconsDialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&iconsDialog,&QDialog::reject);
      if(iconsDialog.exec()==QDialog::Accepted){activityIcons=QJsonArray();for(auto it=iconFields.begin();it!=iconFields.end();++it)if(!it.value()->currentData().toString().isEmpty())activityIcons.append(QJsonObject{{"activityId",it.key()},{"imageAssetId",it.value()->currentData().toString()}});}
    });
  }
  auto groups = value.value("scheduleGroups").toArray();
  auto table = new QTableWidget(groups.size(),3);table->setHorizontalHeaderLabels({tr("분류 ID"),tr("표시 이름"),tr("분류 이미지")});table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setTabKeyNavigation(false);table->setObjectName("scheduleGroups");table->setMinimumHeight(130);table->setMaximumHeight(200);
  for(int i=0;i<groups.size();i++){auto g=groups[i].toObject();table->setItem(i,0,new QTableWidgetItem(g["id"].toString()));table->setItem(i,1,new QTableWidgetItem(g["label"].toString()));table->setCellWidget(i,2,imagePicker(g["iconAssetId"].toString(),"scheduleGroupIcon"));}
  auto list = new QListWidget;list->setObjectName("widgetEntries");list->setMinimumHeight(140);
  auto decreasing = new QListWidget;decreasing->setObjectName("decreasingAttributes");decreasing->setMaximumHeight(110);
  for(auto d:view["documents"].toArray())if(d.toObject()["kind"]=="attributes")for(auto v:d.toObject()["data"].toObject()["attributes"].toArray()){
    auto a=v.toObject();auto item=new QListWidgetItem(a["label"].toString(),decreasing);item->setData(Qt::UserRole,a["id"].toString());
    item->setCheckState(value.value("decreasingAttributeIds").toArray().contains(a["id"])?Qt::Checked:Qt::Unchecked);
  }
  if(kind=="schedule")form->addRow(tr("예상에서 낮아질수록 좋은 항목"),decreasing);else decreasing->hide();
  QJsonArray entries;
  for(auto d:view["documents"].toArray()){
    auto doc=d.toObject();auto data=doc["data"].toObject();
    if(kind=="schedule"&&doc["kind"]=="activity")entries.append(data);
    if(kind=="stats"&&doc["kind"]=="attributes")entries=data["attributes"].toArray();
  }
  const auto selectionField=kind=="actions"?QString("actionIds"):QString("attributeIds");
  if(kind=="actions")entries=ruleSelectableActions(projectData(),value.value("actionIds").toArray());
  for(auto v:entries){auto a=v.toObject();auto item=new QListWidgetItem(a.value("label").toString(a.value("name").toString())+" · "+a["id"].toString(),list);item->setData(Qt::UserRole,a["id"].toString());item->setCheckState(value.value(selectionField).toArray().contains(a["id"])?Qt::Checked:Qt::Unchecked);}
  int selected=-1;
  auto storeGroup=[&]{if(selected<0||selected>=groups.size())return;QJsonArray ids;for(int i=0;i<list->count();i++)if(list->item(i)->checkState()==Qt::Checked)ids.append(list->item(i)->data(Qt::UserRole).toString());auto g=groups[selected].toObject();g["activityIds"]=ids;groups[selected]=g;};
  connect(table,&QTableWidget::currentCellChanged,&dialog,[&](int row,int,int,int){storeGroup();selected=row;auto ids=row>=0&&row<groups.size()?groups[row].toObject()["activityIds"].toArray():QJsonArray();for(int i=0;i<list->count();i++)list->item(i)->setCheckState(ids.contains(list->item(i)->data(Qt::UserRole).toString())?Qt::Checked:Qt::Unchecked);});
  auto buttons = new QHBoxLayout;auto add=new QPushButton(tr("분류 추가"));auto remove=new QPushButton(tr("분류 삭제"));add->setObjectName("scheduleGroupAdd");remove->setObjectName("scheduleGroupRemove");buttons->addWidget(add);buttons->addWidget(remove);
  connect(add,&QPushButton::clicked,&dialog,[&]{if(table->rowCount()>=12)return;storeGroup();QSet<QString> ids;for(int i=0;i<table->rowCount();++i)ids.insert(table->item(i,0)->text());int suffix=1;while(ids.contains(QString("group-%1").arg(suffix)))++suffix;const auto id=QString("group-%1").arg(suffix);auto row=table->rowCount();groups.append(QJsonObject{{"id",id},{"label",tr("새 분류")},{"activityIds",QJsonArray()}});table->insertRow(row);table->setItem(row,0,new QTableWidgetItem(id));table->setItem(row,1,new QTableWidgetItem(tr("새 분류")));table->setCellWidget(row,2,imagePicker({},"scheduleGroupIcon"));table->setCurrentCell(row,1);add->setEnabled(table->rowCount()<12);});
  connect(remove,&QPushButton::clicked,&dialog,[&]{int row=table->currentRow();if(row<0)return;selected=-1;groups.removeAt(row);table->removeRow(row);if(table->rowCount())table->setCurrentCell(qMin(row,table->rowCount()-1),0);add->setEnabled(true);});
  add->setEnabled(table->rowCount()<12);
  if(kind=="schedule"){layout->addWidget(new QLabel(tr("분류를 선택한 뒤 포함할 활동을 체크하세요.")));layout->addWidget(table);layout->addLayout(buttons);layout->addWidget(list);if(!groups.empty())table->setCurrentCell(0,0);}
  else {table->hide();add->hide();remove->hide();if(kind=="stats"||kind=="actions"){layout->addWidget(new QLabel(tr("표시할 항목 (선택하지 않으면 전체)")));layout->addWidget(list);}else list->hide();}
  auto error=new QLabel;error->setObjectName("widgetSettingsError");error->setWordWrap(true);error->hide();shell->addWidget(error);
  auto ok = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);ok->setObjectName("widgetSettingsButtons");shell->addWidget(ok);
  connect(ok,&QDialogButtonBox::accepted,&dialog,[&]{
    auto fail=[&](const QString& message){error->setText(message);error->show();};
    if(view.value("revision")!=openedRevision||view.value("root")!=openedRoot){fail(tr("프로젝트가 변경되었습니다. 취소한 뒤 다시 열어 주세요."));return;}
    if(kind=="schedule"){
      if(mode->currentData()=="activity-board"&&projectData().contains("roster")){fail(tr("활동 보드는 한 명을 육성하는 프로젝트에서 사용할 수 있습니다. 다인 육성에서는 다른 표시 방식을 선택하세요."));return;}
      storeGroup();QSet<QString> ids;
      for(int i=0;i<groups.size();++i){const auto id=table->item(i,0)->text().trimmed();
        if(id.size()>64||!QRegularExpression("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$").match(id).hasMatch()||ids.contains(id)||table->item(i,1)->text().trimmed().isEmpty()||groups[i].toObject()["activityIds"].toArray().isEmpty()){fail(tr("각 분류에는 중복 없는 영문 소문자 ID, 표시 이름, 활동이 하나 이상 필요합니다."));table->setCurrentCell(i,0);scroll->ensureWidgetVisible(table);return;}ids.insert(id);
      }
    }
    dialog.accept();
  });connect(ok,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  if(dialog.exec()!=QDialog::Accepted)return;
  storeGroup();if(!key.isEmpty())value[key]=mode->currentData().toString();
  for(auto it=frameFields.begin();it!=frameFields.end();++it){auto id=it.value()->currentData().toString();if(id.isEmpty())value.remove(it.key());else value[it.key()]=id;}
  if(kind=="schedule") {for(int i=0;i<groups.size();i++){auto g=groups[i].toObject();g["id"]=table->item(i,0)->text().trimmed();g["label"]=table->item(i,1)->text();const auto icon=qobject_cast<QComboBox*>(table->cellWidget(i,2))->currentData().toString();if(icon.isEmpty())g.remove("iconAssetId");else g["iconAssetId"]=icon;groups[i]=g;}value["scheduleGroups"]=groups;}
  if(kind=="stats"||kind=="actions"){QJsonArray ids;for(int i=0;i<list->count();i++)if(list->item(i)->checkState()==Qt::Checked)ids.append(list->item(i)->data(Qt::UserRole).toString());if(ids.empty())value.remove(selectionField);else value[selectionField]=ids;}
  if(supportsControlStyle(kind)){if(controlStyle->currentData().toString().isEmpty())value.remove("controlStyleId");else value["controlStyleId"]=controlStyle->currentData().toString();}
  if(kind=="button")value["autoLabel"]=autoLabel->isChecked();
  if(kind=="choices"){if(choiceEffects->isChecked())value["showChoiceEffects"]=true;else value.remove("showChoiceEffects");}
  if(kind=="schedule"){QJsonArray ids;for(int i=0;i<decreasing->count();i++)if(decreasing->item(i)->checkState()==Qt::Checked)ids.append(decreasing->item(i)->data(Qt::UserRole).toString());if(ids.empty())value.remove("decreasingAttributeIds");else value["decreasingAttributeIds"]=ids;}
  if(kind=="schedule"){if(activityIcons.isEmpty())value.remove("activityIcons");else value["activityIcons"]=activityIcons;}
  if(kind=="schedule"){if(phaseForecast->isChecked())value["showPhaseForecast"]=true;else value.remove("showPhaseForecast");}
  if(kind=="schedule"){if(activityRules->isChecked())value["showActivityRules"]=true;else if(value["scheduleLayout"]=="activity-board"||value.contains("showActivityRules"))value["showActivityRules"]=false;else value.remove("showActivityRules");}
  if(kind=="schedule"){if(activityDescription->isChecked())value["showActivityDescription"]=true;else if(value["scheduleLayout"]=="activity-board"||value.contains("showActivityDescription"))value["showActivityDescription"]=false;else value.remove("showActivityDescription");}
  auto project=projectData();auto visual=project["visual"].toObject();auto screens=visual["screens"].toArray();
  for(int i=0;i<screens.size();i++){auto s=screens[i].toObject();if(s["id"]!=screenId)continue;auto elements=s["elements"].toArray();for(int j=0;j<elements.size();j++)if(elements[j].toObject()["id"]==selection)elements[j]=value;s["elements"]=elements;screens[i]=s;}
  visual["screens"]=screens;visual["widgetTheme"]=appearance->currentData().toString();visual["confirmPurchases"]=confirm->isChecked();project["visual"]=visual;replaceProject(project);
}

void StudioWindow::addElement(const QString &kind, const QString &assetId) {
  if (bridge.busy() || currentScreen().isEmpty())
    return;
  auto p = projectData();
  auto visual = p["visual"].toObject();
  auto screens = visual["screens"].toArray();
  auto elements = currentScreen()["elements"].toArray();
  if (elements.size() >= 100) {
    errorLabel->setText(tr("화면당 최대 100개 요소를 지원합니다."));
    return;
  }
  if(kind=="input"&&p.value("userFields").toArray().isEmpty()){errorLabel->setText(tr("디자인 > 새 게임 · 사용자 입력 필드에서 필드를 먼저 만드세요."));return;}
  QSet<QString> ids;
  for (auto e : elements)
    ids.insert(e.toObject()["id"].toString());
  int n = 1;
  while (ids.contains(kind + "-" + QString::number(n)))
    ++n;
  selection = kind + "-" + QString::number(n);
  QJsonObject element{{"id", selection},
                      {"kind", kind},
                      {"label", kind},
                      {"x", 80},
                      {"y", 100},
                      {"width", 400},
                      {"height", kind == "text" ? 60 : 280},
                      {"fontSize", 24},
                      {"color", "#203a34"}};
  if(kind=="saveSlots"||kind=="loadSlots"||kind=="playerSettings"){
    element["x"]=40;element["y"]=100;element["width"]=qMax(560,visual.value("width").toInt(1280)-80);element["height"]=qMax(300,visual.value("height").toInt(720)-180);
    element["fontSize"]=18;element["fill"]="#F5EFDF";element["color"]="#263C36";element["hoverFill"]="#345C50";element["borderColor"]="#C7B99A";
    if(kind!="playerSettings"&&!visual.contains("saveSystem"))visual["saveSystem"]=QJsonObject{{"manualSlots",6},{"autoSlots",3}};
  }
  if (kind == "text") {
    element["text"] = tr("새 텍스트");
    element["binding"] = "none";
  }
  if(kind=="input"){
    const auto field=p.value("userFields").toArray().first().toObject();element["userFieldId"]=field.value("id");element["label"]=field.value("label");element["height"]=56;element["fill"]="#ffffff";element["borderWidth"]=1;element["borderColor"]="#57806c";element["radius"]=6;
  }
  if (kind == "panel") {
    element["fill"] = "#fff6dfee";
    element["radius"] = 12;
  }
  if (kind == "image") {
    QString id = assetId;
    if (id.isEmpty() && assetList->currentItem())
      id = assetList->currentItem()->data(Qt::UserRole).toString();
    if (id.isEmpty()) {
      bottom->setCurrentIndex(1);
      errorLabel->setText(tr("이미지를 가져온 뒤 에셋을 더블클릭하세요."));
      return;
    }
    element["imageSource"] = "asset";
    element["imageAssetId"] = id;
    element["fit"] = "contain";
    for (auto a : assets)
      if (a.toObject()["id"] == id) {
        QImageReader reader(a.toObject()["absolutePath"].toString());
        auto size = reader.size();
        if (size.isValid()) {
          size.scale(600, 600, Qt::KeepAspectRatio);
          element["width"] = size.width();
          element["height"] = size.height();
        }
      }
  }
  elements.append(element);
  for (int i = 0; i < screens.size(); ++i) {
    auto s = screens[i].toObject();
    if (s["id"] == screenId) {
      s["elements"] = elements;
      screens[i] = s;
    }
  }
  visual["screens"] = screens;
  p["visual"] = visual;
  replaceProject(p);
}

void StudioWindow::importImage() {
  if (bridge.busy() || projectData().isEmpty())
    return;
  auto file = QFileDialog::getOpenFileName(this, tr("PNG 이미지 가져오기"), {},
                                           tr("PNG 이미지 (*.png)"), nullptr, qEnvironmentVariableIsSet("RAISING_QT_TEST") ? QFileDialog::DontUseNativeDialog : QFileDialog::Options{});
  if (file.isEmpty())
    return;
  QImageReader reader(file);
  auto size = reader.size();
  if (!size.isValid() || size.width() > 8192 || size.height() > 8192 ||
      qint64(size.width()) * size.height() > 32 * 1024 * 1024 ||
      reader.read().isNull()) {
    errorLabel->setText(tr("읽을 수 있는 PNG 이미지가 필요합니다. 최대 8192px "
                           "/ 32메가픽셀입니다."));
    return;
  }
  request("importImage", {{"file", file}}, [this](QJsonObject reply) {
    const auto id = reply["importedAssetId"].toString();
    if (id.isEmpty())
      return;
    assetSearch->clear();
    bottom->setCurrentIndex(1);
    for (int i = 0; i < assetList->count(); ++i)
      if (assetList->item(i)->data(Qt::UserRole) == id)
        assetList->setCurrentRow(i);
    status->setText(tr("이미지 등록됨 · 더블클릭하여 현재 화면에 배치하세요."));
  });
}

void StudioWindow::moveLayer(int delta) {
  if(selectedIds().size()>1){errorLabel->setText(tr("레이어 순서는 한 요소 또는 부모 그룹을 선택해 변경하세요."));return;}
  if (bridge.busy() || selection.isEmpty())
    return;
  if(!elementLockOwner(currentScreen().value("elements").toArray(),selection).isEmpty()){errorLabel->setText(tr("배치 잠금을 먼저 해제하세요."));return;}
  auto p = projectData();
  auto visual = p["visual"].toObject();
  auto screens = visual["screens"].toArray();
  for (int i = 0; i < screens.size(); ++i) {
    auto s = screens[i].toObject();
    if (s["id"] != screenId)
      continue;
    auto elements = s["elements"].toArray();
    const auto parent=currentElement().value("parentId").toString();QList<int> siblings;int at=-1;
    for(int j=0;j<elements.size();++j)if(elements[j].toObject().value("parentId").toString()==parent){if(elements[j].toObject()["id"]==selection)at=siblings.size();siblings.append(j);}
    if(at<0)return;const int next=qBound(0,at+delta,int(siblings.size())-1);if(next==at)return;
    const QJsonValue moving=elements[siblings[at]];const int direction=next>at?1:-1;
    for(int j=at;j!=next;j+=direction)elements[siblings[j]]=elements[siblings[j+direction]];
    elements[siblings[next]]=moving;
    s["elements"] = elements;
    screens[i] = s;
  }
  visual["screens"] = screens;
  p["visual"] = visual;
  replaceProject(p);
}

void StudioWindow::refreshContent() {
  const QString selected =
      contentList->currentItem()
          ? contentList->currentItem()->data(Qt::UserRole).toString()
          : QString();
  const QMap<QString, QString> names{
      {"activity", tr("활동")},        {"event", tr("사건")},
      {"ending", tr("엔딩")},          {"character", tr("인물")},
      {"attributes", tr("능력·자원")}, {"flags", tr("플래그")}};
  auto search = findChild<QLineEdit *>("contentSearch");
  contentList->clear();
  for (auto v : view["documents"].toArray()) {
    auto doc = v.toObject();
    const auto kind = doc["kind"].toString();
    if (!names.contains(kind))
      continue;
    auto data = doc["data"].toObject();
    auto item =
        new QListWidgetItem(names[kind] + " · " +
                                data["label"].toString(data["name"].toString(
                                    data["id"].toString(names[kind]))),
                            contentList);
    item->setData(Qt::UserRole, doc["file"].toString());
    item->setToolTip(doc["file"].toString());
    item->setHidden(
        search && !(item->text()+" "+doc["file"].toString()).contains(search->text(), Qt::CaseInsensitive));
    if (doc["file"] == selected)
      contentList->setCurrentItem(item);
  }
  // Plugin-owned documents are listed read-only here; their editor is the plugin's.
  for (auto v : view["documents"].toArray()) {
    const auto doc = v.toObject();
    const auto kind = doc["kind"].toString();
    if (kind != "extension" && kind != "pluginLock") continue;
    const auto data = doc["data"].toObject();
    const auto text = kind == "pluginLock" ? tr("플러그인 · 프로젝트 사용 목록")
                                           : tr("플러그인 문서 · %1/%2 · %3").arg(data["plugin"].toString(), data["type"].toString(), data["id"].toString());
    auto item = new QListWidgetItem(text, contentList);
    item->setData(Qt::UserRole, doc["file"].toString());
    item->setToolTip(doc["file"].toString());
    item->setHidden(search && !(item->text()+" "+doc["file"].toString()).contains(search->text(), Qt::CaseInsensitive));
    if (doc["file"] == selected) contentList->setCurrentItem(item);
  }
}

void StudioWindow::editContent() {
  if (bridge.busy() || !contentList->currentItem())
    return;
  const auto file = contentList->currentItem()->data(Qt::UserRole).toString();
  QJsonObject data;
  QString kind;
  for (auto v : view["documents"].toArray())
    if (v.toObject()["file"] == file) {
      data = v.toObject()["data"].toObject();
      kind = v.toObject()["kind"].toString();
    }
  if (data.isEmpty())
    return;
  if (kind == "event" || kind == "ending") { editScripts(); return; }
  if (kind == "pluginLock") { showPluginManager(); return; }
  if (kind == "extension") {
    const auto plugin = data["plugin"].toString();
    if (!plugins->plugin(plugin)) errorLabel->setText(plugins->placeholder(QJsonObject{{"extension", QJsonObject{{"plugin", plugin}}}}));
    else plugins->openEditor(plugin, data["type"].toString(), data["id"].toString());
    return;
  }
  const auto sourceRevision = view.value("revision"), sourceRoot = view.value("root");
  const bool isActivity = data.contains("requirements") && data.contains("costs") && data.contains("baseEffects");
  QDialog dialog(this);
  dialog.setObjectName("contentEditor");
  dialog.setWindowTitle(tr("콘텐츠 편집 · ") + file);
  dialog.resize(760, 600);
  auto layout = new QVBoxLayout(&dialog);
  auto tabs = new QTabWidget;
  layout->addWidget(tabs);
  auto formPage = new QWidget;
  auto form = new QFormLayout(formPage);
  tabs->addTab(formPage, tr("기본 속성"));
  const QString labelKey = data.contains("name") ? "name" : "label";
  auto label = new QLineEdit(data.value(labelKey).toString());
  label->setObjectName("contentLabel");
  label->setMaxLength(qMax(32767, int(data.value(labelKey).toString().size())));
  form->addRow(tr("ID (연결 보존)"), new QLabel(data.value("id").toString(file)));
  if (data.contains(labelKey))
    form->addRow(tr("이름"), label);
  else
    label->hide();
  auto description = new QPlainTextEdit(data.value("description").toString());
  description->setObjectName("contentDescription");
  description->setMaximumHeight(100);
  if (data.contains("description"))
    form->addRow(tr("설명"), description);
  else
    description->hide();
  QMap<QString, QSpinBox *> numbers;
  auto number = [&](const QString &key, const QString &caption, int value,
                    int min, int max) {
    auto spin = new QSpinBox;
    spin->setObjectName("content-" + key);
    spin->setRange(min, max);
    spin->setValue(value);
    numbers[key] = spin;
    form->addRow(caption, spin);
  };
  if (data.contains("priority"))
    number("priority", tr("우선순위"), data["priority"].toInt(), -100000,
           100000);
  if (data.contains("window")) {
    auto w = data["window"].toObject();
    number("fromPeriod", tr("발생 시작 일정 (0부터)"), w["fromPeriod"].toInt(),
           0, 3600);
    number("throughPeriod", tr("발생 마지막 일정"), w["throughPeriod"].toInt(),
           0, 3600);
  }
  QComboBox *eventScope=nullptr,*eventTrigger=nullptr;
  QJsonValue narrativeRequirements=data.value("requirements");
  if(data.contains("window")){
    eventScope=new QComboBox;eventScope->setObjectName("content-eventScope");eventScope->addItem(tr("공유 완료 기록"),"shared");eventScope->addItem(tr("실행자별 완료 기록"),"actor");eventScope->setCurrentIndex(eventScope->findData(data.value("scope").toString("shared")));form->addRow(tr("사건 소유 범위"),eventScope);
    eventTrigger=new QComboBox;eventTrigger->setObjectName("content-eventTrigger");eventTrigger->addItem(tr("기간 종료"),"period-end");eventTrigger->addItem(tr("행동에서 연결"),"interaction");eventTrigger->setCurrentIndex(eventTrigger->findData(data.value("trigger").toString("period-end")));form->addRow(tr("발생 방식"),eventTrigger);
    auto applyScope=[=]{const bool actor=eventScope->currentData()=="actor";if(actor)eventTrigger->setCurrentIndex(eventTrigger->findData("interaction"));eventTrigger->setEnabled(!actor);};connect(eventScope,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[applyScope]{applyScope();});applyScope();
  }
  if(!isActivity&&data.contains("requirements")){auto conditions=new QPushButton(tr("발생 조건 · 수식 편집…"));conditions->setObjectName("contentLogic-requirements");form->addRow(conditions);connect(conditions,&QPushButton::clicked,&dialog,[&]{LogicEditor editor(narrativeRequirements,"condition",view,&dialog);if(editor.exec()==QDialog::Accepted)narrativeRequirements=editor.value();});}
  QTableWidget *portraits = nullptr;
  QSet<QString> portraitImageIds;
  if (data.contains("portraits") && data.contains("relationships")) {
    auto page = new QWidget;
    auto box = new QVBoxLayout(page);
    auto help = new QLabel(tr("표정 ID를 이미지 에셋에 연결합니다. 대본에서 사용 중인 표정은 삭제하거나 ID를 변경할 수 없습니다. 같은 ID의 이미지는 교체할 수 있습니다."));
    help->setWordWrap(true); box->addWidget(help);
    portraits = new QTableWidget(0, 2);
    portraits->setObjectName("contentPortraits");
    portraits->setHorizontalHeaderLabels({tr("표정 ID"), tr("이미지 에셋")});
    portraits->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    box->addWidget(portraits);
    QJsonArray images;
    for (auto document : view.value("documents").toArray())
      if (document.toObject().value("kind") == "assets")
        for (auto asset : document.toObject().value("data").toObject().value("assets").toArray())
          if (asset.toObject().value("kind") == "image") {
            images.append(asset); portraitImageIds.insert(asset.toObject().value("id").toString());
          }
    auto appendPortrait = [portraits, images](const QString &id, const QString &assetId) {
      const int row = portraits->rowCount(); portraits->insertRow(row);
      portraits->setItem(row, 0, new QTableWidgetItem(id));
      auto asset = new QComboBox; asset->addItem(QObject::tr("이미지 선택…"), QString());
      for (auto value : images) { const auto image = value.toObject(); const auto imageId = image.value("id").toString(); asset->addItem(imageId, imageId); }
      if (!assetId.isEmpty() && asset->findData(assetId) < 0) asset->addItem(QObject::tr("없는 원본 이미지: ") + assetId, assetId);
      asset->setCurrentIndex(asset->findData(assetId)); portraits->setCellWidget(row, 1, asset); portraits->setCurrentCell(row, 0);
    };
    const auto originalPortraits = data.value("portraits").toObject();
    for (auto it = originalPortraits.begin(); it != originalPortraits.end(); ++it) appendPortrait(it.key(), it.value().toString());
    auto row = new QHBoxLayout;
    auto add = new QPushButton(tr("표정 추가")); add->setObjectName("contentPortraitAdd");
    auto remove = new QPushButton(tr("선택 표정 삭제")); remove->setObjectName("contentPortraitRemove");
    row->addWidget(add); row->addWidget(remove); row->addStretch(); box->addLayout(row);
    connect(add, &QPushButton::clicked, &dialog, [appendPortrait] { appendPortrait({}, {}); });
    connect(remove, &QPushButton::clicked, &dialog, [portraits] { if (portraits->currentRow() >= 0) portraits->removeRow(portraits->currentRow()); });
    tabs->addTab(page, tr("표정 · 초상화"));
  }
  QMap<QString, QTableWidget *> effectTables;
  RelationshipEditor *relationships = nullptr;
  if (kind == "character") {
    relationships = new RelationshipEditor(data, view);
    tabs->addTab(relationships, tr("관계 · 신뢰 / 호감"));
  }
  QMap<QString, std::function<void(QJsonArray)>> effectFillers;
  for (const auto &key : QStringList{"costs", "baseEffects"})
    if (data.contains(key)) {
      auto page = new QWidget;
      auto box = new QVBoxLayout(page);
      auto effects = new QTableWidget(0, 3);
      effects->setObjectName("content-" + key);
      effects->setHorizontalHeaderLabels(
          {tr("능력 / 자원"), tr("처리"), tr("값")});
      effects->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
      box->addWidget(effects);
      effectTables[key] = effects;
      auto append = [this, effects, key](QJsonObject value) {
        const bool cost = key == "costs",
                   simple =
                       value.isEmpty() || cost ||
                       (value.value("target").toObject().value("kind") == "attribute" && !value.value("target").toObject().contains("owner") &&
                        value.value("value").toObject().value("op") == "literal");
        int row = effects->rowCount();
        effects->insertRow(row);
        auto original = new QTableWidgetItem;
        original->setData(Qt::UserRole, QVariant::fromValue(value));
        effects->setItem(row, 0, original);
        if (!simple) {
          original->setText(tr("복합 효과 · 조건/효과 탭에서 편집"));
          original->setFlags(original->flags() & ~Qt::ItemIsEditable);
          return;
        }
        auto attribute = new QComboBox;
        for (auto d : view["documents"].toArray())
          if (d.toObject()["kind"] == "attributes")
            for (auto a :
                 d.toObject()["data"].toObject()["attributes"].toArray())
              attribute->addItem(a.toObject()["label"].toString(),
                                 a.toObject()["id"].toString());
        const auto id =
            cost ? value["attributeId"].toString()
                 : value["target"].toObject()["attributeId"].toString();
        if (attribute->findData(id) >= 0)
          attribute->setCurrentIndex(attribute->findData(id));
        effects->setCellWidget(row, 0, attribute);
        auto op = new QComboBox;
        op->addItem(cost ? tr("소비") : tr("증감"), "add");
        if (!cost)
          op->addItem(tr("고정값 설정"), "set");
        op->setCurrentIndex(qMax(0, op->findData(value["op"].toString())));
        op->setEnabled(!cost);
        effects->setCellWidget(row, 1, op);
        auto amount = new QSpinBox;
        amount->setRange(cost ? 0 : std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        amount->setValue(cost ? value["amount"].toInt()
                              : value["value"].toObject()["value"].toInt());
        effects->setCellWidget(row, 2, amount);
      };
      for (auto e : data[key].toArray())
        append(e.toObject());
      effectFillers[key] = [effects, append](QJsonArray values) {
        effects->setRowCount(0); for (auto value : values) append(value.toObject());
      };
      auto add = new QPushButton(tr("＋ 행 추가"));
      box->addWidget(add);
      connect(add, &QPushButton::clicked, &dialog, [append] { append({}); });
      auto remove = new QPushButton(tr("선택한 행 삭제"));
      box->addWidget(remove);
      connect(remove, &QPushButton::clicked, &dialog, [effects] {
        if (effects->currentRow() >= 0)
          effects->removeRow(effects->currentRow());
      });
      tabs->addTab(page, key == "costs" ? tr("비용") : tr("기본 효과"));
    }
  auto collectEffects = [&](const QString &key) {
    QJsonArray values;
    auto table = effectTables.value(key);
    if (!table) return values;
    for (int row = 0; row < table->rowCount(); ++row) {
      auto attribute = qobject_cast<QComboBox *>(table->cellWidget(row, 0));
      if (!attribute) { values.append(table->item(row, 0)->data(Qt::UserRole).toJsonObject()); continue; }
      const int amount = qobject_cast<QSpinBox *>(table->cellWidget(row, 2))->value();
      if (key == "costs") values.append(QJsonObject{{"attributeId", attribute->currentData().toString()}, {"amount", amount}});
      else values.append(QJsonObject{{"op", qobject_cast<QComboBox *>(table->cellWidget(row, 1))->currentData().toString()},
                                    {"target", QJsonObject{{"kind", "attribute"}, {"attributeId", attribute->currentData().toString()}}},
                                    {"value", QJsonObject{{"op", "literal"}, {"value", amount}}}});
    }
    return values;
  };
  QMap<QString, QJsonValue> activityLogic;
  QCheckBox *successEnabled = nullptr;
  QTableWidget *previewAttributes = nullptr;
  if (isActivity) {
    auto page = new QWidget;
    auto box = new QVBoxLayout(page);
    auto help = new QLabel(tr("조건과 성공률은 일정 시작 규칙·사전 정책 적용 후, 비용·활동 효과 적용 전의 값을 읽습니다. 비용 → 기본 효과 → 성공/실패 효과가 반영됩니다. 수식에서 숙련도·단계·수업료 같은 능력을 읽을 수 있으며 성공률은 0~10000 (10000=100%)입니다."));
    help->setWordWrap(true);
    box->addWidget(help);
    activityLogic["requirements"] = data.value("requirements");
    activityLogic["baseEffects"] = data.value("baseEffects");
    activityLogic["chance"] = data.value("successCheck").toObject().value("chance").isUndefined()
        ? QJsonValue(QJsonObject{{"op", "literal"}, {"value", 10000}})
        : data.value("successCheck").toObject().value("chance");
    activityLogic["successEffects"] = data.value("successEffects").toArray();
    activityLogic["failureEffects"] = data.value("failureEffects").toArray();
    auto addLogic = [&](const QString &key, const QString &caption, const QString &kind) {
      auto button = new QPushButton(caption);
      button->setObjectName("contentLogic-" + key);
      box->addWidget(button);
      connect(button, &QPushButton::clicked, &dialog, [&, key, kind, caption] {
        if (key == "baseEffects") activityLogic[key] = collectEffects(key);
        LogicEditor editor(activityLogic.value(key), kind, view, &dialog);
        editor.setWindowTitle(caption);
        if (editor.exec() == QDialog::Accepted) {
          activityLogic[key] = editor.value();
          if (key == "baseEffects") effectFillers.value(key)(editor.value().toArray());
        }
      });
      return button;
    };
    auto conditionAction = addLogic("requirements", tr("실행 조건 편집…"), "condition");
    auto effectsAction = addLogic("baseEffects", tr("능력·자원·관계 효과 편집…"), "effects");
    auto conditionShortcut = new QPushButton(tr("실행 조건 편집…")); conditionShortcut->setObjectName("contentRequirementsQuick"); form->addRow(conditionShortcut);
    connect(conditionShortcut, &QPushButton::clicked, conditionAction, &QPushButton::click);
    auto effectShortcut = new QPushButton(tr("능력·자원·관계 효과 편집…")); effectShortcut->setObjectName("contentEffectsQuick"); form->addRow(effectShortcut);
    connect(effectShortcut, &QPushButton::clicked, effectsAction, &QPushButton::click);
    successEnabled = new QCheckBox(tr("성공 / 실패 판정 사용"));
    successEnabled->setObjectName("contentSuccessEnabled");
    successEnabled->setChecked(data.contains("successCheck"));
    box->addWidget(successEnabled);
    for (const auto &spec : QList<QStringList>{{"chance", tr("성공률 수식 편집…"), "number"},
                                               {"successEffects", tr("성공 효과 편집…"), "effects"},
                                               {"failureEffects", tr("실패 효과 편집…"), "effects"}}) {
      auto button = addLogic(spec[0], spec[1], spec[2]);
      button->setEnabled(successEnabled->isChecked());
      connect(successEnabled, &QCheckBox::toggled, button, &QPushButton::setEnabled);
    }
    box->addStretch();
    tabs->addTab(page, tr("조건 · 효과 · 성공 판정"));
    auto detailsPage = new QWidget;
    auto detailsLayout = new QVBoxLayout(detailsPage);
    auto detailsHelp = new QLabel(tr("일정에서 활동을 고를 때 현재 단계·비용 같은 능력 값을 표시합니다. 현재 상태를 보여주며, 미래에 예약한 일정의 비용을 예측하지 않습니다. 이름을 비우면 능력 이름을 사용하고, 형식의 {value} 자리에 숫자가 들어갑니다."));
    detailsHelp->setWordWrap(true); detailsLayout->addWidget(detailsHelp);
    previewAttributes = new QTableWidget(0, 3); previewAttributes->setObjectName("contentPreviewAttributes");
    previewAttributes->setHorizontalHeaderLabels({tr("능력 / 자원"), tr("표시 이름 · 비우면 기본값"), tr("값 형식")});
    previewAttributes->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); detailsLayout->addWidget(previewAttributes);
    auto appendPreview = [this, previewAttributes](QJsonObject value) {
      const int row = previewAttributes->rowCount(); if (row >= 8) return;
      previewAttributes->insertRow(row);
      auto original = new QTableWidgetItem; original->setData(Qt::UserRole, value); previewAttributes->setItem(row, 0, original);
      auto ref = new QComboBox;
      for (auto doc : view.value("documents").toArray()) if (doc.toObject().value("kind") == "attributes")
        for (auto a : doc.toObject().value("data").toObject().value("attributes").toArray()) ref->addItem(a.toObject().value("label").toString(), a.toObject().value("id").toString());
      const auto id = value.value("attributeId").toString();
      if (!id.isEmpty() && ref->findData(id) < 0) ref->addItem(tr("없는 원본: ") + id, id);
      if (!id.isEmpty()) ref->setCurrentIndex(ref->findData(id));
      previewAttributes->setCellWidget(row, 0, ref);
      previewAttributes->setItem(row, 1, new QTableWidgetItem(value.value("label").toString()));
      previewAttributes->setItem(row, 2, new QTableWidgetItem(value.value("valueFormat").toString("{value}")));
    };
    for (auto v : data.value("previewAttributes").toArray()) appendPreview(v.toObject());
    auto detailButtons = new QHBoxLayout;
    auto addDetail = new QPushButton(tr("＋ 표시 항목 추가 · 최대 8개")); addDetail->setObjectName("contentPreviewAdd");
    auto removeDetail = new QPushButton(tr("선택 항목 삭제")); removeDetail->setObjectName("contentPreviewRemove");
    detailButtons->addWidget(addDetail); detailButtons->addWidget(removeDetail); detailsLayout->addLayout(detailButtons);
    connect(addDetail, &QPushButton::clicked, &dialog, [appendPreview] { appendPreview({}); });
    connect(removeDetail, &QPushButton::clicked, &dialog, [previewAttributes] { if (previewAttributes->currentRow() >= 0) previewAttributes->removeRow(previewAttributes->currentRow()); });
    tabs->addTab(detailsPage, tr("일정 선택 정보 · 현재 값"));
  }
  auto rows = data.value("attributes").toArray();
  auto table = new QTableWidget(0, 6);
  table->setObjectName("attributeTable");
  table->setHorizontalHeaderLabels(
      {tr("ID"), tr("이름"), tr("초기값"), tr("최소"), tr("최대"), tr("종류")});
  const QStringList keys{"id", "label", "initial", "min", "max"};
  auto appendAttribute = [table, keys](QJsonObject value, bool fresh) {
    const int r = table->rowCount();
    table->insertRow(r);
    for (int c = 0; c < 5; ++c) {
      auto v = value[keys[c]];
      auto item = new QTableWidgetItem(c < 2 ? v.toString()
                                             : QString::number(v.toInt()));
      if (c == 0) {
        item->setData(Qt::UserRole, value);
        if (!fresh) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
      }
      table->setItem(r, c, item);
    }
    auto kind = new QComboBox;
    kind->addItem(QObject::tr("능력"), "stat");
    kind->addItem(QObject::tr("자원"), "resource");
    kind->addItem(QObject::tr("상태"), "condition");
    kind->setCurrentIndex(qMax(0, kind->findData(value.value("kind").toString())));
    table->setCellWidget(r, 5, kind);
  };
  for (auto row : rows) appendAttribute(row.toObject(), false);
  table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  auto error = new QLabel;
  error->setObjectName("contentError");
  error->setWordWrap(true);
  error->setStyleSheet("color:#ffb2ae");
  if (data.contains("attributes")) {
    auto page = new QWidget;
    auto box = new QVBoxLayout(page);
    box->addWidget(table);
    auto buttons = new QHBoxLayout;
    auto add = new QPushButton(tr("＋ 능력 / 자원 추가"));
    add->setObjectName("contentAttributeAdd");
    auto remove = new QPushButton(tr("선택 항목 삭제"));
    remove->setObjectName("contentAttributeRemove");
    buttons->addWidget(add); buttons->addWidget(remove); box->addLayout(buttons);
    connect(add, &QPushButton::clicked, &dialog, [=] {
      QSet<QString> used; for (int r = 0; r < table->rowCount(); ++r) used.insert(table->item(r, 0)->text());
      int n = 1; while (used.contains(QString("attribute-%1").arg(n))) ++n;
      appendAttribute(QJsonObject{{"id", QString("attribute-%1").arg(n)}, {"label", tr("새 능력")},
                                  {"kind", "stat"}, {"initial", 0}, {"min", 0}, {"max", 1000}}, true);
      table->setCurrentCell(table->rowCount() - 1, 0);
      table->editItem(table->item(table->currentRow(), 0));
    });
    connect(remove, &QPushButton::clicked, &dialog, [&, table] {
      const int row = table->currentRow(); if (row < 0) return;
      const auto id = table->item(row, 0)->text();
      std::function<bool(QJsonValue)> referenced = [&](QJsonValue value) {
        if (value.isArray()) { for (auto v : value.toArray()) if (referenced(v)) return true; }
        if (value.isObject()) { const auto object = value.toObject();
          for (auto it = object.begin(); it != object.end(); ++it) {
            if (it.key().endsWith("attributeId", Qt::CaseInsensitive) && it.value() == id) return true;
            if (it.key() == "attributeIds" && it.value().toArray().contains(id)) return true;
            if (it.key() == "attributes" && it.value().isObject() && it.value().toObject().contains(id)) return true;
            if (referenced(it.value())) return true;
          }
        }
        return false;
      };
      for (auto doc : view.value("documents").toArray()) {
        auto document = doc.toObject();
        if (document.value("file") != file && referenced(document.value("data"))) {
          error->setText(tr("%1에서 사용하는 능력입니다. 연결을 먼저 해제하세요: %2").arg(document.value("file").toString(), id)); return;
        }
      }
      if (table->rowCount() <= 1) { error->setText(tr("능력 항목은 하나 이상 필요합니다.")); return; }
      table->removeRow(row); error->clear();
    });
    auto pasteRows=new QPushButton(tr("표에서 여러 능력 입력…"));pasteRows->setObjectName("contentAttributePaste");box->addWidget(pasteRows);
    connect(pasteRows,&QPushButton::clicked,&dialog,[&]{readTabularInput(&dialog,{tr("ID"),tr("이름"),tr("초기값"),tr("최소"),tr("최대"),tr("종류")},[&](const QList<QStringList>& values)->QString {
      QSet<QString> ids;QList<QJsonObject> additions;
      for(const auto &v:values){bool a=false,b=false,c=false;int initial=v[2].toInt(&a),minimum=v[3].toInt(&b),maximum=v[4].toInt(&c);QString kind=v[5];if(kind==tr("능력"))kind="stat";if(kind==tr("자원"))kind="resource";if(kind==tr("상태"))kind="condition";
        if(v[0].size()>64||!QRegularExpression("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$").match(v[0]).hasMatch()||ids.contains(v[0]))return tr("ID가 잘못되었거나 입력 안에서 중복됩니다: ")+v[0];
        if(v[1].isEmpty()||!a||!b||!c||minimum>initial||initial>maximum||!QStringList{"stat","resource","condition"}.contains(kind))return tr("이름·정수 범위·종류를 확인하세요: ")+v[0];
        ids.insert(v[0]);additions.append(QJsonObject{{"id",v[0]},{"label",v[1]},{"initial",initial},{"min",minimum},{"max",maximum},{"kind",kind}});
      }
      // Validate every row first: an invalid paste never partially edits the table.
      for(const auto &v:additions){int existing=-1;for(int r=0;r<table->rowCount();++r)if(table->item(r,0)->text()==v.value("id").toString()){existing=r;break;}if(existing<0)appendAttribute(v,true);else{for(int col=1;col<5;++col)table->item(existing,col)->setText(col==1?v.value(keys[col]).toString():QString::number(v.value(keys[col]).toInt()));auto combo=qobject_cast<QComboBox*>(table->cellWidget(existing,5));combo->setCurrentIndex(combo->findData(v.value("kind").toString()));}}
      return {};
    });});
    tabs->addTab(page, tr("능력 · 초기값 / 범위"));
  }
  else
    table->hide();
  QTableWidget *flagTable=nullptr;
  if(kind=="flags") {
    auto page=new QWidget;auto box=new QVBoxLayout(page);flagTable=new QTableWidget(0,3);flagTable->setObjectName("contentFlagTable");flagTable->setHorizontalHeaderLabels({tr("ID"),tr("이름"),tr("시작 값")});flagTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);box->addWidget(flagTable);
    auto appendFlag=[flagTable](QJsonObject flag,bool fresh){int row=flagTable->rowCount();flagTable->insertRow(row);auto id=new QTableWidgetItem(flag.value("id").toString());id->setData(Qt::UserRole,flag);if(!fresh)id->setFlags(id->flags()&~Qt::ItemIsEditable);flagTable->setItem(row,0,id);flagTable->setItem(row,1,new QTableWidgetItem(flag.value("label").toString()));auto initial=new QTableWidgetItem;initial->setFlags(Qt::ItemIsEnabled|Qt::ItemIsUserCheckable);initial->setCheckState(flag.value("initial").toBool()?Qt::Checked:Qt::Unchecked);flagTable->setItem(row,2,initial);};
    for(auto value:data.value("flags").toArray())appendFlag(value.toObject(),false);
    auto add=new QPushButton(tr("＋ 플래그 추가"));add->setObjectName("contentFlagAdd");box->addWidget(add);connect(add,&QPushButton::clicked,&dialog,[=]{int n=1;QSet<QString> used;for(int row=0;row<flagTable->rowCount();++row)used.insert(flagTable->item(row,0)->text());while(used.contains(QString("flag-%1").arg(n)))++n;appendFlag(QJsonObject{{"id",QString("flag-%1").arg(n)},{"label",tr("새 플래그")},{"initial",false}},true);flagTable->setCurrentCell(flagTable->rowCount()-1,0);flagTable->editItem(flagTable->item(flagTable->currentRow(),0));});
    auto paste=new QPushButton(tr("표에서 여러 플래그 입력…"));paste->setObjectName("contentFlagPaste");box->addWidget(paste);
    connect(paste,&QPushButton::clicked,&dialog,[&,appendFlag,flagTable]{readTabularInput(&dialog,{tr("ID"),tr("이름"),tr("시작 값: 켜짐 / 꺼짐")},[&,appendFlag,flagTable](const QList<QStringList>& rows)->QString{QSet<QString> ids;QJsonArray values;for(const auto &row:rows){if(row[0].size()>64||!QRegularExpression("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$").match(row[0]).hasMatch()||ids.contains(row[0])||row[1].isEmpty())return tr("ID 중복·형식 또는 이름을 확인하세요: ")+row[0];const auto initial=row[2].toLower();if(!QStringList{"true","false","0","1",tr("켜짐"),tr("꺼짐")}.contains(initial))return tr("시작 값은 켜짐 / 꺼짐입니다: ")+row[0];ids.insert(row[0]);values.append(QJsonObject{{"id",row[0]},{"label",row[1]},{"initial",initial=="true"||initial=="1"||initial==tr("켜짐")}});}for(auto value:values){auto flag=value.toObject();int found=-1;for(int row=0;row<flagTable->rowCount();++row)if(flagTable->item(row,0)->text()==flag.value("id").toString()){found=row;break;}if(found<0)appendFlag(flag,true);else{flagTable->item(found,1)->setText(flag.value("label").toString());flagTable->item(found,2)->setCheckState(flag.value("initial").toBool()?Qt::Checked:Qt::Unchecked);}}return {};});});
    tabs->addTab(page,tr("플래그 · 시작 값"));
  }
  auto nodes = data.value("nodes").toArray();
  auto dialogue = new QTableWidget(0, 2);
  dialogue->setObjectName("dialogueTable");
  dialogue->setHorizontalHeaderLabels({tr("노드"), tr("대사 / 질문")});
  for (int i = 0; i < nodes.size(); ++i) {
    auto node = nodes[i].toObject();
    QString key = node["type"] == "dialogue" ? "text"
                  : node["type"] == "choice" ? "prompt"
                                             : "";
    if (key.isEmpty())
      continue;
    int r = dialogue->rowCount();
    dialogue->insertRow(r);
    auto id = new QTableWidgetItem(node["id"].toString());
    id->setData(Qt::UserRole, i);
    id->setFlags(id->flags() & ~Qt::ItemIsEditable);
    dialogue->setItem(r, 0, id);
    dialogue->setItem(r, 1, new QTableWidgetItem(node[key].toString()));
  }
  dialogue->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  if (data.contains("nodes"))
    tabs->addTab(dialogue, tr("대사"));
  else
    dialogue->hide();
  auto json = new QPlainTextEdit;
  json->setObjectName("contentJson");
  json->setPlainText(QString::fromUtf8(QJsonDocument(data).toJson()));
  int advanced = tabs->addTab(json, tr("개발자용 JSON"));
  json->setToolTip(tr("보기만 하면 폼으로 돌아갈 수 있습니다. JSON을 직접 수정하면 폼과의 덮어쓰기를 막기 위해 저장 또는 취소 전까지 폼이 잠깁니다."));
  layout->addWidget(error);
  auto collect = [&]() -> QJsonObject {
    auto result = data;
    if(flagTable){QJsonArray flags;QSet<QString> ids;for(int row=0;row<flagTable->rowCount();++row){auto id=flagTable->item(row,0)->text().trimmed();auto label=flagTable->item(row,1)->text().trimmed();if(id.size()>64||!QRegularExpression("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$").match(id).hasMatch()||ids.contains(id)||label.isEmpty()){error->setText(tr("플래그 %1행: 고유 ID와 이름을 확인하세요.").arg(row+1));return {};}ids.insert(id);auto flag=flagTable->item(row,0)->data(Qt::UserRole).toJsonObject();flag["id"]=id;flag["label"]=label;flag["initial"]=flagTable->item(row,2)->checkState()==Qt::Checked;flags.append(flag);}result["flags"]=flags;}
    if (relationships) {
      QString problem;
      result["relationships"] = relationships->value(&problem);
      if (!problem.isEmpty()) { error->setText(problem); return {}; }
    }
    if(!isActivity&&data.contains("requirements"))result["requirements"]=narrativeRequirements;
    if(eventScope){if(eventScope->currentData()=="actor"){result["scope"]="actor";result["trigger"]="interaction";}else{if(data.contains("scope"))result["scope"]="shared";else result.remove("scope");result["trigger"]=eventTrigger->currentData().toString();}}
    if (portraits) {
      QJsonObject values;
      static const QRegularExpression idPattern("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$");
      for (int row = 0; row < portraits->rowCount(); ++row) {
        const auto id = portraits->item(row, 0)->text();
        const auto assetId = qobject_cast<QComboBox *>(portraits->cellWidget(row, 1))->currentData().toString();
        if (id.size() > 64 || !idPattern.match(id).hasMatch() || values.contains(id) || !portraitImageIds.contains(assetId)) {
          error->setText(tr("표정 %1행: 고유한 소문자·숫자·하이픈 ID(1~64자)와 존재하는 이미지 에셋을 선택하세요.").arg(row + 1)); return {};
        }
        values[id] = assetId;
      }
      result["portraits"] = values;
    }
    if (result.contains(labelKey))
      result[labelKey] = label->text();
    if (result.contains("description"))
      result["description"] = description->toPlainText();
    if (numbers.contains("priority"))
      result["priority"] = numbers["priority"]->value();
    if (numbers.contains("fromPeriod"))
      result["window"] =
          QJsonObject{{"fromPeriod", numbers["fromPeriod"]->value()},
                      {"throughPeriod", numbers["throughPeriod"]->value()}};
    for (auto it = effectTables.begin(); it != effectTables.end(); ++it) {
      QJsonArray values;
      auto t = it.value();
      for (int r = 0; r < t->rowCount(); ++r) {
        auto attribute = qobject_cast<QComboBox *>(t->cellWidget(r, 0));
        if (!attribute) {
          values.append(t->item(r, 0)->data(Qt::UserRole).toJsonObject());
          continue;
        }
        const int amount =
            qobject_cast<QSpinBox *>(t->cellWidget(r, 2))->value();
        if (it.key() == "costs")
          values.append(
              QJsonObject{{"attributeId", attribute->currentData().toString()},
                          {"amount", amount}});
        else
          values.append(QJsonObject{
              {"op", qobject_cast<QComboBox *>(t->cellWidget(r, 1))
                         ->currentData()
                         .toString()},
              {"target", QJsonObject{{"kind", "attribute"},
                                     {"attributeId",
                                      attribute->currentData().toString()}}},
              {"value", QJsonObject{{"op", "literal"}, {"value", amount}}}});
      }
      result[it.key()] = values;
    }
    if (isActivity) {
      result["requirements"] = activityLogic.value("requirements");
      result["baseEffects"] = collectEffects("baseEffects");
      if (successEnabled->isChecked()) {
        auto check = result.value("successCheck").toObject();
        check["chance"] = activityLogic.value("chance");
        result["successCheck"] = check;
        result["successEffects"] = activityLogic.value("successEffects");
        result["failureEffects"] = activityLogic.value("failureEffects");
      } else {
        result.remove("successCheck"); result.remove("successEffects"); result.remove("failureEffects");
      }
      QJsonArray details;
      for (int row = 0; row < previewAttributes->rowCount(); ++row) {
        auto value = previewAttributes->item(row, 0)->data(Qt::UserRole).toJsonObject();
        const auto id = qobject_cast<QComboBox *>(previewAttributes->cellWidget(row, 0))->currentData().toString();
        const auto label = previewAttributes->item(row, 1)->text(), format = previewAttributes->item(row, 2)->text();
        if (id.isEmpty() || label.toUcs4().size() > 80 || format.toUcs4().size() > 160) {
          error->setText(tr("일정 선택 정보 %1행: 능력을 선택하고, 이름 80자·형식 160자 이내로 입력하세요.").arg(row + 1)); return {};
        }
        value["attributeId"] = id;
        if (label.isEmpty()) value.remove("label"); else value["label"] = label;
        if (format != value.value("valueFormat").toString("{value}")) value["valueFormat"] = format;
        details.append(value);
      }
      if (details.empty()) result.remove("previewAttributes"); else result["previewAttributes"] = details;
    }
    if (result.contains("attributes")) {
      QJsonArray a;
      QSet<QString> ids;
      static const QRegularExpression idPattern("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$");
      for (int r = 0; r < table->rowCount(); ++r) {
        auto v = table->item(r, 0)->data(Qt::UserRole).toJsonObject();
        const auto id = table->item(r, 0)->text();
        if (id.size() > 64 || !idPattern.match(id).hasMatch() || ids.contains(id)) {
          error->setText(tr("%1행: 고유한 ID를 입력하세요. 영문 소문자로 시작하는 소문자·숫자·하이픈 1~64자입니다.").arg(r + 1)); return {};
        }
        ids.insert(id); v["id"] = id;
        v["label"] = table->item(r, 1)->text();
        if (v.value("label").toString().isEmpty()) {
          error->setText(tr("%1행: 이름을 입력하세요.").arg(r + 1)); return {};
        }
        v["kind"] = qobject_cast<QComboBox *>(table->cellWidget(r, 5))->currentData().toString();
        for (int c = 2; c < 5; ++c) {
          bool ok;
          qlonglong value = table->item(r, c)->text().toLongLong(&ok);
          if (!ok || value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
            error->setText(tr("%1행: 능력 값은 32비트 정수여야 합니다.").arg(r + 1));
            return {};
          }
          v[keys[c]] = int(value);
        }
        if (v.value("min").toInt() > v.value("initial").toInt() || v.value("initial").toInt() > v.value("max").toInt()) {
          error->setText(tr("%1행: 최소 ≤ 초기값 ≤ 최대여야 합니다.").arg(r + 1)); return {};
        }
        a.append(v);
      }
      result["attributes"] = a;
    }
    if (result.contains("nodes")) {
      auto a = nodes;
      for (int r = 0; r < dialogue->rowCount(); ++r) {
        int i = dialogue->item(r, 0)->data(Qt::UserRole).toInt();
        auto n = a[i].toObject();
        n[n["type"] == "choice" ? "prompt" : "text"] =
            dialogue->item(r, 1)->text();
        a[i] = n;
      }
      result["nodes"] = a;
    }
    return result;
  };
  // Merely viewing JSON must not lock the forms. Only actual raw edits do.
  connect(tabs, &QTabWidget::currentChanged, &dialog, [&](int index) {
    if (index == advanced) {
      auto value = collect();
      if (value.isEmpty()) {
        tabs->setCurrentIndex(0);
        return;
      }
      QSignalBlocker jsonSync(json);
      json->setPlainText(QString::fromUtf8(QJsonDocument(value).toJson()));

    }
  });
  connect(json, &QPlainTextEdit::textChanged, &dialog, [&] {
    if (tabs->currentIndex() != advanced) return;
    for (int i = 0; i < advanced; ++i) tabs->setTabEnabled(i, false);
    error->setText(tr("JSON을 직접 수정 중입니다. 폼 덮어쓰기를 막기 위해 저장 또는 취소 전까지 폼이 잠깁니다."));
  });
  auto buttons =
      new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
  layout->addWidget(buttons);
  QJsonObject result;
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
    if (bridge.busy()) { error->setText(tr("진행 중인 작업이 끝난 뒤 다시 저장하세요.")); return; }
    if (view.value("revision") != sourceRevision || view.value("root") != sourceRoot) {
      error->setText(tr("프로젝트가 변경되었습니다. 창을 닫고 다시 여세요.")); return;
    }
    if (tabs->currentIndex() == advanced) {
      QJsonParseError parse;
      auto document =
          QJsonDocument::fromJson(json->toPlainText().toUtf8(), &parse);
      if (parse.error != QJsonParseError::NoError || !document.isObject()) {
        error->setText(tr("JSON 오류: %1 · 위치 %2")
                           .arg(parse.errorString())
                           .arg(parse.offset));
        return;
      }
      result = document.object();
    } else
      result = collect();
    if (!result.isEmpty() && portraits) {
      const auto values = result.value("portraits").toObject();
      const auto characterId = data.value("id").toString();
      QString missing;
      bool hasSpeaker = false;
      QSet<QString> speakerPortraitIds;
      std::function<void(QJsonValue)> scan = [&](QJsonValue value) {
        if (!missing.isEmpty()) return;
        if (value.isArray()) { for (auto item : value.toArray()) scan(item); }
        else if (value.isObject()) {
          const auto object = value.toObject();
          if (object.value("speakerId").toString() == characterId) hasSpeaker = true;
          if (object.value("imageSource") == "speaker-portrait") speakerPortraitIds.insert(object.value("portraitId").toString("face"));
          if (object.value("characterId").toString() == characterId && object.contains("portraitId") && !values.contains(object.value("portraitId").toString())) { missing = object.value("portraitId").toString(); return; }
          for (auto it = object.begin(); it != object.end(); ++it) scan(it.value());
        }
      };
      for (auto document : view.value("documents").toArray()) scan(document.toObject().value("data"));
      if (missing.isEmpty() && hasSpeaker) {
        const auto original = data.value("portraits").toObject();
        for (const auto &id : speakerPortraitIds)
          if (original.contains(id) && !values.contains(id)) { missing = id; break; }
      }
      if (!missing.isEmpty()) { error->setText(tr("표정 '%1'은 대본 또는 화면에서 사용 중입니다. 연결을 먼저 바꾸거나 같은 표정 ID의 이미지를 교체하세요.").arg(missing)); return; }
    }
    if (!result.isEmpty())
      dialog.accept();
  });
  if (dialog.exec() == QDialog::Accepted)
    request("edit", {{"edit", QJsonObject{{"type", "replace"},
                                          {"file", file},
                                          {"data", result}}}});
  if (!table->parent())
    delete table;
  if (!dialogue->parent())
    delete dialogue;
  if (!label->parent())
    delete label;
  if (!description->parent())
    delete description;
}

