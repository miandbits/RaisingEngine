#include "studio.hpp"
#include "logic-editor.hpp"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLineEdit>
#include <QInputDialog>
#include <QDoubleSpinBox>
#include <QJsonDocument>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>
#include <limits>
#include <functional>

void StudioWindow::editWidgetSettings() {
  if (bridge.busy() || currentElement().isEmpty()) return;
  if(currentElement()["kind"]=="plugin") {
    const auto extension=currentElement()["extension"].toObject(),props=extension["props"].toObject();
    QDialog dialog(this);dialog.setWindowTitle(tr("애니메이션 플러그인 연결"));dialog.setObjectName("pluginElementSettings");
    auto form=new QFormLayout(&dialog);
    auto plugin=new QLineEdit(extension["plugin"].toString()),type=new QLineEdit(extension["type"].toString()),ref=new QLineEdit(extension["ref"].toString());
    plugin->setObjectName("pluginId");type->setObjectName("pluginType");ref->setObjectName("pluginReference");
    form->addRow(tr("플러그인 ID"),plugin);form->addRow(tr("요소 유형"),type);form->addRow(tr("컨트롤러 ID"),ref);
    auto poster=new QComboBox;poster->addItem(tr("미리보기 이미지 없음"),QString());for(auto a:assets)if(a.toObject()["kind"]=="image")poster->addItem(a.toObject()["id"].toString(),a.toObject()["id"].toString());
    poster->setCurrentIndex(qMax(0,poster->findData(props["posterAssetId"].toString())));form->addRow(tr("캔버스 정지 미리보기"),poster);
    auto scale=new QDoubleSpinBox;scale->setRange(.05,10);scale->setSingleStep(.05);scale->setValue(props["scale"].toDouble(1));form->addRow(tr("배율"),scale);
    auto flip=new QCheckBox;flip->setChecked(props["flipX"].toBool());form->addRow(tr("좌우 반전"),flip);
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    const QRegularExpression id("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$");for(const auto* field:{plugin,type,ref})if(!id.match(field->text()).hasMatch()){errorLabel->setText(tr("플러그인·유형·컨트롤러 ID를 확인하세요."));return;}
    updateElement([&](QJsonObject& e){auto next=extension;auto values=props;next["plugin"]=plugin->text();next["type"]=type->text();next["ref"]=ref->text();values["scale"]=scale->value();values["flipX"]=flip->isChecked();if(poster->currentData().toString().isEmpty())values.remove("posterAssetId");else values["posterAssetId"]=poster->currentData().toString();next["props"]=values;e["extension"]=next;});return;
  }
  if (currentElement()["kind"] == "collection") { editDataUi(); return; }
  if (QStringList{"actions", "policies"}.contains(currentElement()["kind"].toString())) { editRules(); return; }
  if (QStringList{"services", "diets", "sell"}.contains(currentElement()["kind"].toString())) { editFacilities(); return; }
  auto value = currentElement();
  QDialog dialog(this); dialog.setObjectName("widgetSettingsDialog");
  dialog.setWindowTitle(tr("위젯 구성")); dialog.resize(720, 600);
  auto layout = new QVBoxLayout(&dialog);
  auto form = new QFormLayout; layout->addLayout(form);
  auto appearance = new QComboBox;
  appearance->setObjectName("widgetTheme");
  appearance->addItem(tr("기본"), "default"); appearance->addItem(tr("고전 프레임"), "ornate");
  appearance->setCurrentIndex(appearance->findData(projectData()["visual"].toObject()["widgetTheme"].toString("default")));
  form->addRow(tr("프로젝트 위젯 테마"), appearance);
  QMap<QString,QComboBox*> frameFields;
  for(const auto &key: {QString("frameAssetId"),QString("entryFrameAssetId")}) {
    auto field=new QComboBox;field->addItem(tr("기본 프레임"),QString());
    for(auto a:assets){auto asset=a.toObject();if(asset["kind"]=="image")field->addItem(asset["id"].toString(),asset["id"].toString());}
    field->setCurrentIndex(qMax(0,field->findData(value[key].toString())));frameFields[key]=field;
    form->addRow(key=="frameAssetId"?tr("분류 메뉴 프레임"):tr("활동·상품 카드 프레임"),field);
  }
  auto confirm = new QCheckBox(tr("상품 구매 전 확인 창"));
  confirm->setChecked(projectData()["visual"].toObject()["confirmPurchases"].toBool()); form->addRow(confirm);
  auto mode = new QComboBox; mode->setObjectName("widgetLayout"); QString key;
  const auto kind = value["kind"].toString();
  if (kind == "schedule") {key="scheduleLayout"; mode->addItem(tr("일정 칸 목록"),"columns"); mode->addItem(tr("월간 달력"),"calendar"); mode->addItem(tr("분류 → 활동 메뉴"),"menu");}
  if (kind == "stats") {key="statsLayout"; mode->addItem(tr("능력 막대"),"bars"); mode->addItem(tr("압축 표"),"compact");}
  if (kind == "shop") {key="shopLayout"; mode->addItem(tr("목록"),"list"); mode->addItem(tr("상품 카드"),"cards");}
  if (!key.isEmpty()) {mode->setCurrentIndex(qMax(0,mode->findData(value[key].toString())));form->addRow(tr("표시 방식"),mode);}
  else mode->hide();
  auto autoLabel = new QCheckBox(tr("진행 버튼 이름을 현재 단계에 맞춰 자동 변경"));
  autoLabel->setChecked(value["autoLabel"].toBool(true)); if(kind=="button")form->addRow(autoLabel); else autoLabel->hide();
  auto groups = value["scheduleGroups"].toArray();
  auto table = new QTableWidget(groups.size(),2);table->setHorizontalHeaderLabels({tr("분류 ID"),tr("표시 이름")});table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setObjectName("scheduleGroups");
  for(int i=0;i<groups.size();i++){auto g=groups[i].toObject();table->setItem(i,0,new QTableWidgetItem(g["id"].toString()));table->setItem(i,1,new QTableWidgetItem(g["label"].toString()));}
  auto list = new QListWidget;list->setObjectName("widgetEntries");
  QJsonArray entries;
  for(auto d:view["documents"].toArray()){
    auto doc=d.toObject();auto data=doc["data"].toObject();
    if(kind=="schedule"&&doc["kind"]=="activity")entries.append(data);
    if(kind=="stats"&&doc["kind"]=="attributes")entries=data["attributes"].toArray();
  }
  for(auto v:entries){auto a=v.toObject();auto item=new QListWidgetItem(a["label"].toString()+" · "+a["id"].toString(),list);item->setData(Qt::UserRole,a["id"].toString());item->setCheckState(value["attributeIds"].toArray().contains(a["id"])?Qt::Checked:Qt::Unchecked);}
  int selected=-1;
  auto storeGroup=[&]{if(selected<0||selected>=groups.size())return;QJsonArray ids;for(int i=0;i<list->count();i++)if(list->item(i)->checkState()==Qt::Checked)ids.append(list->item(i)->data(Qt::UserRole).toString());auto g=groups[selected].toObject();g["activityIds"]=ids;groups[selected]=g;};
  connect(table,&QTableWidget::currentCellChanged,&dialog,[&](int row,int,int,int){storeGroup();selected=row;auto ids=row>=0&&row<groups.size()?groups[row].toObject()["activityIds"].toArray():QJsonArray();for(int i=0;i<list->count();i++)list->item(i)->setCheckState(ids.contains(list->item(i)->data(Qt::UserRole).toString())?Qt::Checked:Qt::Unchecked);});
  auto buttons = new QHBoxLayout;auto add=new QPushButton(tr("분류 추가"));auto remove=new QPushButton(tr("분류 삭제"));buttons->addWidget(add);buttons->addWidget(remove);
  connect(add,&QPushButton::clicked,&dialog,[&]{storeGroup();auto row=table->rowCount();groups.append(QJsonObject{{"id",QString("group-%1").arg(row+1)},{"label",tr("새 분류")},{"activityIds",QJsonArray()}});table->insertRow(row);table->setItem(row,0,new QTableWidgetItem(QString("group-%1").arg(row+1)));table->setItem(row,1,new QTableWidgetItem(tr("새 분류")));table->setCurrentCell(row,1);});
  connect(remove,&QPushButton::clicked,&dialog,[&]{int row=table->currentRow();if(row<0)return;selected=-1;groups.removeAt(row);table->removeRow(row);if(table->rowCount())table->setCurrentCell(qMin(row,table->rowCount()-1),0);});
  if(kind=="schedule"){layout->addWidget(new QLabel(tr("분류를 선택한 뒤 포함할 활동을 체크하세요.")));layout->addWidget(table);layout->addLayout(buttons);layout->addWidget(list);if(!groups.empty())table->setCurrentCell(0,0);}
  else {table->hide();add->hide();remove->hide();if(kind=="stats"){layout->addWidget(new QLabel(tr("표시할 능력·자원 (선택하지 않으면 전체)")));layout->addWidget(list);}else list->hide();}
  auto ok = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(ok);connect(ok,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(ok,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  if(dialog.exec()!=QDialog::Accepted)return;
  storeGroup();if(!key.isEmpty())value[key]=mode->currentData().toString();
  for(auto it=frameFields.begin();it!=frameFields.end();++it){auto id=it.value()->currentData().toString();if(id.isEmpty())value.remove(it.key());else value[it.key()]=id;}
  if(kind=="schedule") {for(int i=0;i<groups.size();i++){auto g=groups[i].toObject();g["id"]=table->item(i,0)->text().trimmed();g["label"]=table->item(i,1)->text();groups[i]=g;}value["scheduleGroups"]=groups;}
  if(kind=="stats"){QJsonArray ids;for(int i=0;i<list->count();i++)if(list->item(i)->checkState()==Qt::Checked)ids.append(list->item(i)->data(Qt::UserRole).toString());if(ids.empty())value.remove("attributeIds");else value["attributeIds"]=ids;}
  if(kind=="button")value["autoLabel"]=autoLabel->isChecked();
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
  if (kind == "text") {
    element["text"] = tr("새 텍스트");
    element["binding"] = "none";
  }
  if(kind=="plugin") {
    bool ok=false;const auto identity=QInputDialog::getText(this,tr("애니메이션 연결"),tr("플러그인 ID / 유형 / 컨트롤러 ID"),QLineEdit::Normal,"cutout-animation/character/",&ok);
    if(!ok)return;const auto parts=identity.split('/');const QRegularExpression id("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$");
    if(parts.size()!=3||!id.match(parts[0]).hasMatch()||!id.match(parts[1]).hasMatch()||!id.match(parts[2]).hasMatch()){errorLabel->setText(tr("예: cutout-animation/character/alice-home"));return;}
    element["extension"]=QJsonObject{{"plugin",parts[0]},{"type",parts[1]},{"ref",parts[2]},{"props",QJsonObject{{"scale",1},{"flipX",false}}}};element["label"]=parts[2];element["height"]=500;
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
                                           tr("PNG 이미지 (*.png)"));
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
  if (bridge.busy() || selection.isEmpty())
    return;
  auto p = projectData();
  auto visual = p["visual"].toObject();
  auto screens = visual["screens"].toArray();
  for (int i = 0; i < screens.size(); ++i) {
    auto s = screens[i].toObject();
    if (s["id"] != screenId)
      continue;
    auto elements = s["elements"].toArray();
    for (int j = 0; j < elements.size(); ++j)
      if (elements[j].toObject()["id"] == selection) {
        const int next = j + delta;
        if (next < 0 || next >= elements.size())
          return;
        auto e = elements[j];
        elements[j] = elements[next];
        elements[next] = e;
        break;
      }
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
        search && !item->text().contains(search->text(), Qt::CaseInsensitive));
    if (doc["file"] == selected)
      contentList->setCurrentItem(item);
  }
}

void StudioWindow::editContent() {
  if (bridge.busy() || !contentList->currentItem())
    return;
  const auto file = contentList->currentItem()->data(Qt::UserRole).toString();
  QJsonObject data;
  for (auto v : view["documents"].toArray())
    if (v.toObject()["file"] == file)
      data = v.toObject()["data"].toObject();
  if (data.isEmpty())
    return;
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
                       (value["target"].toObject()["kind"] == "attribute" &&
                        value["value"].toObject()["op"] == "literal");
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
    addLogic("requirements", tr("실행 조건 편집…"), "condition");
    addLogic("baseEffects", tr("기본 효과 · 수식 편집…"), "effects");
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
    tabs->addTab(page, tr("능력 · 초기값 / 범위"));
  }
  else
    table->hide();
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
  int advanced = tabs->addTab(json, tr("고급 JSON · 조건 / 효과 / 분기"));
  layout->addWidget(error);
  auto collect = [&]() -> QJsonObject {
    auto result = data;
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
  // Advanced mode is one-way for this dialog: avoids silently overwriting JSON
  // edits with stale form values.
  connect(tabs, &QTabWidget::currentChanged, &dialog, [&](int index) {
    if (index == advanced) {
      auto value = collect();
      if (value.isEmpty()) {
        tabs->setCurrentIndex(0);
        return;
      }
      json->setPlainText(QString::fromUtf8(QJsonDocument(value).toJson()));
      for (int i = 0; i < advanced; ++i)
        tabs->setTabEnabled(i, false);
    }
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

