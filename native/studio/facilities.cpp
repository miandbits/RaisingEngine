#include "studio.hpp"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
#include <functional>
#include <limits>

namespace {
QString uniqueId(const QJsonArray &rows, const QString &prefix) {
  QSet<QString> ids; for (auto v:rows) ids.insert(v.toObject()["id"].toString());
  for(int i=1;;i++){auto id=prefix+QString::number(i);if(!ids.contains(id))return id;}
}
void selectReference(QComboBox *combo, const QString &id) {
  int index=combo->findData(id);
  if(index<0&&!id.isEmpty()){combo->addItem(QObject::tr("없는 ID: %1").arg(id),id);index=combo->count()-1;}
  combo->setCurrentIndex(index<0&&id.isEmpty()&&combo->count()?0:index);
}
QStringList referencesTo(const QJsonValue &value,const QString &field,const QString &id,const QString &path="/visual") {
  QStringList found;
  if(value.isObject()){
    const auto object=value.toObject();
    if(object[field].isString()&&object[field].toString()==id)found.append(path+"/"+field);
    for(auto it=object.begin();it!=object.end();++it)found.append(referencesTo(it.value(),field,id,path+"/"+it.key()));
  }else if(value.isArray()){
    const auto array=value.toArray();for(int i=0;i<array.size();i++)found.append(referencesTo(array[i],field,id,path+"/"+QString::number(i)));
  }
  return found;
}
}

void StudioWindow::editFacilities() {
  if(bridge.busy())return;
  auto project=projectData();auto gameplay=project["gameplay"].toObject();
  if(gameplay.isEmpty()){QMessageBox::information(this,tr("게임플레이 필요"),tr("장소와 아이템이 있는 프로젝트에서 작업실을 열어 주세요."));return;}
  QJsonArray attributes;
  for(auto doc:view["documents"].toArray())if(doc.toObject()["kind"]=="attributes")attributes=doc.toObject()["data"].toObject()["attributes"].toArray();
  QDialog dialog(this);dialog.setObjectName("facilitiesDialog");dialog.setWindowTitle(tr("식단 · 병원 · 거래 작업실"));dialog.resize(1140,820);
  auto outer=new QVBoxLayout(&dialog);auto intro=new QLabel(tr("데이터를 설정한 뒤 ‘현재 화면에 배치’를 누르세요. 배치된 목록은 같은 데이터를 공유합니다. 식단 비용·효과는 일정 실행 시 반복 적용되며, 거래·치료는 확인 후 즉시 처리됩니다."));intro->setWordWrap(true);outer->addWidget(intro);
  auto split=new QSplitter;outer->addWidget(split,1);auto left=new QWidget;auto leftBox=new QVBoxLayout(left);split->addWidget(left);
  auto kind=new QComboBox;kind->setObjectName("facilityKind");kind->addItem(tr("구매 상품"),"buy");kind->addItem(tr("판매 매입표"),"sell");kind->addItem(tr("병원 · 즉시 서비스"),"service");kind->addItem(tr("지속 식단 정책"),"diet");leftBox->addWidget(kind);
  auto shop=new QComboBox;shop->setObjectName("facilityShop");leftBox->addWidget(shop);
  auto list=new QListWidget;list->setObjectName("facilityEntries");leftBox->addWidget(list,1);
  auto add=new QPushButton(tr("＋ 항목 추가"));add->setObjectName("addFacility");leftBox->addWidget(add);
  auto remove=new QPushButton(tr("선택한 항목 삭제"));remove->setObjectName("deleteFacility");leftBox->addWidget(remove);
  auto addShop=new QPushButton(tr("＋ 새 상점"));addShop->setObjectName("addFacilityShop");leftBox->addWidget(addShop);
  auto scroll=new QScrollArea;scroll->setWidgetResizable(true);split->addWidget(scroll);auto page=new QWidget;auto box=new QVBoxLayout(page);scroll->setWidget(page);split->setStretchFactor(1,3);
  auto form=new QFormLayout;box->addLayout(form);
  auto id=new QLineEdit;id->setReadOnly(true);id->setObjectName("facilityId");form->addRow(tr("ID · 연결 유지"),id);
  auto name=new QLineEdit;name->setObjectName("facilityName");name->setMaxLength(100);form->addRow(tr("이름"),name);
  auto shopName=new QLineEdit;shopName->setObjectName("facilityShopName");shopName->setMaxLength(100);form->addRow(tr("상점 이름 (공유)"),shopName);
  auto location=new QComboBox;location->setObjectName("facilityLocation");for(auto v:gameplay["locations"].toArray()){auto o=v.toObject();location->addItem(o["name"].toString(),o["id"].toString());}form->addRow(tr("이용 장소"),location);
  auto currency=new QComboBox;currency->setObjectName("facilityCurrency");for(auto v:attributes){auto a=v.toObject();currency->addItem(a["label"].toString(),a["id"].toString());}form->addRow(tr("상점 화폐 (공유)"),currency);
  auto item=new QComboBox;item->setObjectName("facilityItem");for(auto v:gameplay["items"].toArray()){auto o=v.toObject();item->addItem(o["name"].toString(),o["id"].toString());}form->addRow(tr("상품"),item);
  auto price=new QSpinBox;price->setObjectName("facilityPrice");price->setRange(0,std::numeric_limits<int>::max());form->addRow(tr("상품 1개 단가"),price);
  auto quantity=new QSpinBox;quantity->setObjectName("facilityMaxQuantity");quantity->setRange(1,999);form->addRow(tr("한 번에 선택할 최대 수량"),quantity);
  auto limitedStock=new QCheckBox(tr("구매 재고 제한"));limitedStock->setObjectName("facilityLimitedStock");form->addRow(limitedStock);
  auto stock=new QSpinBox;stock->setObjectName("facilityStock");stock->setRange(0,std::numeric_limits<int>::max());form->addRow(tr("초기 재고"),stock);
  auto interval=new QSpinBox;interval->setObjectName("facilityInterval");interval->setRange(1,3600);form->addRow(tr("식단 적용 간격 · 일정 칸 수"),interval);
  auto costs=new QTableWidget(0,2);costs->setObjectName("facilityCosts");costs->setHorizontalHeaderLabels({tr("소비할 능력 / 자원"),tr("1회 비용")});costs->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);costs->setMaximumHeight(155);box->addWidget(costs);
  auto costButtons=new QHBoxLayout;auto addCost=new QPushButton(tr("＋ 비용"));addCost->setObjectName("addFacilityCost");auto delCost=new QPushButton(tr("비용 삭제"));costButtons->addWidget(addCost);costButtons->addWidget(delCost);box->addLayout(costButtons);
  auto effects=new QTableWidget(0,3);effects->setObjectName("facilityEffects");effects->setHorizontalHeaderLabels({tr("능력 / 자원"),tr("처리"),tr("1회 효과")});effects->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);effects->setMaximumHeight(175);box->addWidget(effects);
  auto effectButtons=new QHBoxLayout;auto addEffect=new QPushButton(tr("＋ 효과"));addEffect->setObjectName("addFacilityEffect");auto delEffect=new QPushButton(tr("효과 삭제"));effectButtons->addWidget(addEffect);effectButtons->addWidget(delEffect);box->addLayout(effectButtons);
  auto confirmation=new QFormLayout;box->addLayout(confirmation);auto confirmEnabled=new QCheckBox(tr("실행 전에 확인창 표시"));confirmEnabled->setObjectName("facilityConfirm-enabled");confirmEnabled->setChecked(true);confirmation->addRow(confirmEnabled);QMap<QString,QLineEdit*> messages;
  const QStringList messageKeys{"title","message","confirmLabel","cancelLabel"},messageLabels{tr("확인창 제목"),tr("확인창 안내"),tr("실행 버튼"),tr("취소 버튼")};
  for(int i=0;i<messageKeys.size();i++){auto edit=new QLineEdit;edit->setObjectName("facilityConfirm-"+messageKeys[i]);edit->setMaxLength(i==1?2000:100);messages[messageKeys[i]]=edit;confirmation->addRow(messageLabels[i],edit);}
  auto preview=new QLabel;preview->setObjectName("facilitySummary");preview->setWordWrap(true);box->addWidget(preview);box->addStretch();
  auto error=new QLabel;error->setObjectName("facilityError");error->setStyleSheet("color:#ffb2ae");error->setWordWrap(true);outer->addWidget(error);
  auto bottom=new QHBoxLayout;auto place=new QPushButton(tr("현재 화면에 목록 배치"));place->setObjectName("placeFacility");bottom->addWidget(place);auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);bottom->addWidget(buttons);outer->addLayout(bottom);
  buttons->button(QDialogButtonBox::Save)->setText(tr("저장"));buttons->button(QDialogButtonBox::Cancel)->setText(tr("취소"));
  QString selectedKind="buy";int selectedShop=-1,selectedRow=-1;bool loading=false;
  auto rows=[&](){if(selectedKind=="diet"||selectedKind=="service")return gameplay[selectedKind=="diet"?"diets":"services"].toArray();const auto shops=gameplay["shops"].toArray();return selectedShop>=0&&selectedShop<shops.size()?shops[selectedShop].toObject()[selectedKind=="buy"?"offers":"sellOffers"].toArray():QJsonArray{};};
  auto putRows=[&](QJsonArray values){if(selectedKind=="diet"||selectedKind=="service")gameplay[selectedKind=="diet"?"diets":"services"]=values;else{auto shops=gameplay["shops"].toArray();if(selectedShop<0||selectedShop>=shops.size())return;auto s=shops[selectedShop].toObject();s[selectedKind=="buy"?"offers":"sellOffers"]=values;shops[selectedShop]=s;gameplay["shops"]=shops;}};
  auto appendRow=[&](QTableWidget *table,QJsonObject value,bool cost){
    int r=table->rowCount();table->insertRow(r);auto saved=new QTableWidgetItem;saved->setData(Qt::UserRole,value);table->setItem(r,0,saved);
    if(!cost&&!value.isEmpty()&&(value["target"].toObject()["kind"]!="attribute"||value["value"].toObject()["op"]!="literal")){saved->setText(tr("복합 효과 · 원본 보존"));saved->setFlags(saved->flags()&~Qt::ItemIsEditable);return;}
    auto attribute=new QComboBox;for(auto v:attributes){auto a=v.toObject();attribute->addItem(a["label"].toString(),a["id"].toString());}const auto key=cost?value["attributeId"].toString():value["target"].toObject()["attributeId"].toString();selectReference(attribute,key);table->setCellWidget(r,0,attribute);
    if(!cost){auto op=new QComboBox;op->addItem(tr("증감"),"add");op->addItem(tr("고정값"),"set");op->setCurrentIndex(qMax(0,op->findData(value["op"].toString())));table->setCellWidget(r,1,op);}
    auto amount=new QSpinBox;amount->setRange(cost?0:std::numeric_limits<int>::min(),std::numeric_limits<int>::max());amount->setValue(cost?value["amount"].toInt():value["value"].toObject()["value"].toInt());table->setCellWidget(r,cost?1:2,amount);
  };
  auto collect=[&](QTableWidget *table,bool cost){QJsonArray out;for(int r=0;r<table->rowCount();r++){auto attr=qobject_cast<QComboBox*>(table->cellWidget(r,0));if(!attr){out.append(table->item(r,0)->data(Qt::UserRole).toJsonObject());continue;}int n=qobject_cast<QSpinBox*>(table->cellWidget(r,cost?1:2))->value();if(cost)out.append(QJsonObject{{"attributeId",attr->currentData().toString()},{"amount",n}});else out.append(QJsonObject{{"target",QJsonObject{{"kind","attribute"},{"attributeId",attr->currentData().toString()}}},{"op",qobject_cast<QComboBox*>(table->cellWidget(r,1))->currentData().toString()},{"value",QJsonObject{{"op","literal"},{"value",n}}}});}return out;};
  auto store=[&](){if(loading||selectedRow<0)return;auto values=rows();if(selectedRow>=values.size())return;auto value=values[selectedRow].toObject();const bool trade=selectedKind=="buy"||selectedKind=="sell";
    if(trade){value["itemId"]=item->currentData().toString();value["price"]=price->value();if(value.contains("maxQuantity")||quantity->value()!=99)value["maxQuantity"]=quantity->value();if(selectedKind=="buy")value["stock"]=limitedStock->isChecked()?QJsonValue(stock->value()):QJsonValue(QJsonValue::Null);}
    else{value["name"]=name->text().trimmed();value["locationId"]=location->currentData().toString();value["costs"]=collect(costs,true);value["effects"]=collect(effects,false);if(selectedKind=="diet")value["intervalPeriods"]=interval->value();else value["maxQuantity"]=quantity->value();}
    QJsonObject c;if(!confirmEnabled->isChecked())c["enabled"]=false;for(auto it=messages.begin();it!=messages.end();++it)if(!it.value()->text().trimmed().isEmpty())c[it.key()]=it.value()->text();if(c.isEmpty())value.remove("confirmation");else value["confirmation"]=c;values[selectedRow]=value;putRows(values);
    if(trade){auto shops=gameplay["shops"].toArray();auto s=shops[selectedShop].toObject();s["name"]=shopName->text().trimmed();s["locationId"]=location->currentData().toString();s["currencyAttributeId"]=currency->currentData().toString();shops[selectedShop]=s;gameplay["shops"]=shops;}
    if(list->item(selectedRow))list->item(selectedRow)->setText(trade?item->currentText():name->text());
  };
  auto load=[&](int row){loading=true;selectedRow=row;const auto values=rows();auto value=row>=0&&row<values.size()?values[row].toObject():QJsonObject{};page->setEnabled(!value.isEmpty());const bool trade=selectedKind=="buy"||selectedKind=="sell";const auto shops=gameplay["shops"].toArray();auto s=trade&&selectedShop>=0?shops[selectedShop].toObject():QJsonObject{};
    form->setRowVisible(name,!trade);form->setRowVisible(shopName,trade);form->setRowVisible(currency,trade);form->setRowVisible(item,trade);form->setRowVisible(price,trade);form->setRowVisible(quantity,selectedKind!="diet");form->setRowVisible(limitedStock,selectedKind=="buy");form->setRowVisible(stock,selectedKind=="buy");form->setRowVisible(interval,selectedKind=="diet");shop->setVisible(trade);addShop->setVisible(trade);
    id->setText(value["id"].toString(value["itemId"].toString()));name->setText(value["name"].toString());name->setEnabled(!trade);shopName->setText(s["name"].toString());shopName->setEnabled(trade);selectReference(location,(trade?s:value)["locationId"].toString());selectReference(currency,s["currencyAttributeId"].toString());currency->setEnabled(trade);selectReference(item,value["itemId"].toString());item->setEnabled(trade);price->setValue(value["price"].toInt());price->setEnabled(trade);quantity->setValue(value["maxQuantity"].toInt(trade?99:1));quantity->setEnabled(selectedKind!="diet");limitedStock->setChecked(!value["stock"].isNull()&&!value["stock"].isUndefined());limitedStock->setEnabled(selectedKind=="buy");stock->setValue(value["stock"].toInt());stock->setEnabled(selectedKind=="buy"&&limitedStock->isChecked());interval->setValue(value["intervalPeriods"].toInt(1));interval->setEnabled(selectedKind=="diet");
    costs->setRowCount(0);effects->setRowCount(0);for(auto v:value["costs"].toArray())appendRow(costs,v.toObject(),true);for(auto v:value["effects"].toArray())appendRow(effects,v.toObject(),false);costs->setVisible(!trade);effects->setVisible(!trade);for(auto b:{addCost,delCost,addEffect,delEffect})b->setVisible(!trade);
    confirmEnabled->setChecked(value["confirmation"].toObject().value("enabled").toBool(true));for(auto it=messages.begin();it!=messages.end();++it)it.value()->setText(value["confirmation"].toObject()[it.key()].toString());
    preview->setText(trade?tr("단가 × 선택 수량으로 합계가 계산됩니다. 소지품·재고·자금은 실제 플레이에서 검사됩니다."):selectedKind=="diet"?tr("선택 시에는 비용이 들지 않습니다. 다음 일정 실행부터 지정 간격으로 비용과 효과를 적용합니다. 잔액이 부족하면 일정도 함께 실행되지 않습니다."):tr("수량만큼 비용과 증감 효과를 반복 적용합니다. 고정값 효과는 지정값으로 설정합니다. 취소는 게임 상태를 바꾸지 않습니다."));loading=false;};
  auto refill=[&](int row=0){QSignalBlocker block(list);list->clear();for(auto v:rows()){auto o=v.toObject();QString label=o["name"].toString();if(label.isEmpty()){int at=item->findData(o["itemId"].toString());label=at>=0?item->itemText(at):o["itemId"].toString();}list->addItem(label);}int target=list->count()?qBound(0,row,list->count()-1):-1;list->setCurrentRow(target);load(target);};
  auto fillShops=[&](){QSignalBlocker block(shop);shop->clear();for(auto v:gameplay["shops"].toArray()){auto s=v.toObject();shop->addItem(s["name"].toString(),s["id"].toString());}selectedShop=qBound(-1,selectedShop,shop->count()-1);if(selectedShop<0&&shop->count())selectedShop=0;shop->setCurrentIndex(selectedShop);};
  auto validate=[&](){
    QSet<QString> locations,items,attributeIds,services,diets;
    for(auto v:gameplay["locations"].toArray())locations.insert(v.toObject()["id"].toString());
    for(auto v:gameplay["items"].toArray())items.insert(v.toObject()["id"].toString());
    for(auto v:attributes)attributeIds.insert(v.toObject()["id"].toString());
    auto fail=[&](const QString &text){error->setText(text);return false;};
    for(auto v:gameplay["shops"].toArray()){
      const auto s=v.toObject();const auto title=s["name"].toString().trimmed();
      if(title.isEmpty())return fail(tr("상점 이름을 입력해 주세요: %1").arg(s["id"].toString()));
      if(!locations.contains(s["locationId"].toString()))return fail(tr("상점 ‘%1’의 이용 장소를 선택해 주세요.").arg(title));
      if(!attributeIds.contains(s["currencyAttributeId"].toString()))return fail(tr("상점 ‘%1’의 화폐 자원을 선택해 주세요.").arg(title));
      for(const auto &key:{"offers","sellOffers"}){
        QSet<QString> seen;
        for(auto offer:s[key].toArray()){
          const auto id=offer.toObject()["itemId"].toString();
          if(!items.contains(id))return fail(tr("상점 ‘%1’에 존재하지 않는 상품이 있습니다: %2").arg(title,id));
          if(seen.contains(id))return fail(tr("상점 ‘%1’의 %2에 상품 ‘%3’이 중복되었습니다. 다른 상품을 선택하거나 중복 항목을 삭제하세요.").arg(title,QString(key),id));
          seen.insert(id);
        }
      }
    }
    for(const auto &key:{"services","diets"}){
      auto &ids=QString(key)=="services"?services:diets;
      for(auto v:gameplay[key].toArray()){
        const auto value=v.toObject();const auto id=value["id"].toString(),title=value["name"].toString().trimmed();
        if(id.isEmpty()||ids.contains(id))return fail(tr("중복되거나 비어 있는 항목 ID입니다: %1").arg(id));ids.insert(id);
        if(title.isEmpty())return fail(tr("항목 이름을 입력해 주세요: %1").arg(id));
        if(!locations.contains(value["locationId"].toString()))return fail(tr("‘%1’의 이용 장소를 선택해 주세요.").arg(title));
        QSet<QString> costIds;for(auto cost:value["costs"].toArray()){
          const auto attribute=cost.toObject()["attributeId"].toString();
          if(!attributeIds.contains(attribute))return fail(tr("‘%1’의 비용 자원을 선택해 주세요: %2").arg(title,attribute));
          if(costIds.contains(attribute))return fail(tr("‘%1’의 비용 자원 ‘%2’이 중복되었습니다. 한 행으로 합쳐 주세요.").arg(title,attribute));costIds.insert(attribute);
        }
        std::function<bool(const QJsonValue&)> checkAttributes=[&](const QJsonValue &node){
          if(node.isObject()){const auto o=node.toObject();if(o.contains("attributeId")&&!attributeIds.contains(o["attributeId"].toString()))return false;for(auto it=o.begin();it!=o.end();++it)if(!checkAttributes(it.value()))return false;}
          else if(node.isArray())for(auto child:node.toArray())if(!checkAttributes(child))return false;
          return true;
        };
        if(!checkAttributes(value))return fail(tr("‘%1’의 효과나 조건에 없는 능력/자원 ID가 있습니다.").arg(title));
      }
    }
    std::function<bool(const QJsonValue&,const QString&)> checkReferences=[&](const QJsonValue &node,const QString &path){
      if(node.isObject()){
        const auto o=node.toObject();for(const auto &key:{"serviceId","dietId"})if(o.contains(key)&&!o[key].isNull()){
          const auto id=o[key].toString();const auto &ids=QString(key)=="serviceId"?services:diets;
          if(!ids.contains(id))return fail(tr("화면/구성 요소가 없는 항목을 참조합니다: %1 (%2/%3)").arg(id,path,QString(key)));
        }
        for(auto it=o.begin();it!=o.end();++it)if(!checkReferences(it.value(),path+"/"+it.key()))return false;
      }else if(node.isArray()){const auto a=node.toArray();for(int i=0;i<a.size();i++)if(!checkReferences(a[i],path+"/"+QString::number(i)))return false;}
      return true;
    };
    if(!checkReferences(project["visual"],"/visual"))return false;
    error->clear();return true;
  };
  connect(list,&QListWidget::currentRowChanged,&dialog,[&](int row){store();load(row);});
  connect(kind,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{store();selectedKind=kind->currentData().toString();shop->setEnabled(selectedKind=="buy"||selectedKind=="sell");refill();});
  connect(shop,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&](int index){store();selectedShop=index;refill();});
  connect(limitedStock,&QCheckBox::toggled,&dialog,[&](bool on){stock->setEnabled(on&&selectedKind=="buy");});
  connect(addCost,&QPushButton::clicked,&dialog,[&]{appendRow(costs,{},true);});connect(addEffect,&QPushButton::clicked,&dialog,[&]{appendRow(effects,{},false);});
  connect(delCost,&QPushButton::clicked,&dialog,[&]{if(costs->currentRow()>=0)costs->removeRow(costs->currentRow());});connect(delEffect,&QPushButton::clicked,&dialog,[&]{if(effects->currentRow()>=0)effects->removeRow(effects->currentRow());});
  connect(add,&QPushButton::clicked,&dialog,[&]{store();auto values=rows();QJsonObject v;const bool trade=selectedKind=="buy"||selectedKind=="sell";if(trade){if(selectedShop<0||item->count()==0){error->setText(tr("먼저 상점과 아이템이 필요합니다."));return;}QString available;for(int i=0;i<item->count();i++){bool used=false;for(auto offer:values)if(offer.toObject()["itemId"]==item->itemData(i).toString())used=true;if(!used){available=item->itemData(i).toString();break;}}if(available.isEmpty()){error->setText(tr("모든 아이템이 이미 등록되어 있습니다. 기존 항목을 선택해 수정하세요."));return;}v={{"itemId",available},{"price",10},{"maxQuantity",99}};if(selectedKind=="buy")v["stock"]=QJsonValue::Null;}else{v={{"id",uniqueId(values,selectedKind+"-")},{"name",selectedKind=="diet"?tr("새 식단"):tr("새 치료")},{"locationId",location->currentData().toString()},{"costs",QJsonArray{}},{"effects",QJsonArray{}}};if(selectedKind=="diet")v["intervalPeriods"]=1;else v["maxQuantity"]=1;}values.append(v);putRows(values);error->clear();refill(values.size()-1);});
  connect(remove,&QPushButton::clicked,&dialog,[&]{if(selectedRow<0)return;auto values=rows();if(selectedKind=="service"||selectedKind=="diet"){
    const auto id=values[selectedRow].toObject()["id"].toString();const auto field=selectedKind=="service"?"serviceId":"dietId";
    const auto references=referencesTo(project["visual"],field,id);if(!references.isEmpty()){error->setText(tr("‘%1’을 연결한 버튼이 있어 삭제할 수 없습니다. 화면·구성 요소·인스턴스의 연결을 먼저 변경하세요.\n%2").arg(id,references.mid(0,3).join("\n")));return;}
  }values.removeAt(selectedRow);putRows(values);selectedRow=-1;error->clear();refill();});
  connect(addShop,&QPushButton::clicked,&dialog,[&]{store();if(location->count()==0||currency->count()==0){error->setText(tr("상점에는 장소와 화폐 자원이 필요합니다."));return;}auto shops=gameplay["shops"].toArray();shops.append(QJsonObject{{"id",uniqueId(shops,"shop-")},{"name",tr("새 상점")},{"locationId",location->currentData().toString()},{"currencyAttributeId",currency->currentData().toString()},{"offers",QJsonArray{}}});gameplay["shops"]=shops;selectedShop=shops.size()-1;selectedRow=-1;fillShops();refill();});
  bool shouldPlace=false;connect(place,&QPushButton::clicked,&dialog,[&]{store();if(screenId.isEmpty()){error->setText(tr("목록을 배치할 화면을 먼저 열어 주세요."));return;}if((selectedKind=="buy"||selectedKind=="sell")&&selectedShop<0){error->setText(tr("상점을 먼저 선택하세요."));return;}if(!validate())return;shouldPlace=true;dialog.accept();});
  connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{store();if(validate())dialog.accept();});connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  const auto selectedElement=currentElement();const auto selectedWidget=selectedElement["kind"].toString();
  if(selectedWidget=="services"||selectedWidget=="diets"||selectedWidget=="sell"||selectedWidget=="shop"){
    selectedKind=selectedWidget=="services"?"service":selectedWidget=="diets"?"diet":selectedWidget=="shop"?"buy":"sell";
    QSignalBlocker blocker(kind);kind->setCurrentIndex(kind->findData(selectedKind));
    const auto shops=gameplay["shops"].toArray();for(int i=0;i<shops.size();i++)if(shops[i].toObject()["id"]==selectedElement["shopId"])selectedShop=i;
  }
  fillShops();refill();
  if(dialog.exec()!=QDialog::Accepted)return;
  project["gameplay"]=gameplay;
  if(shouldPlace){auto visual=project["visual"].toObject();auto screens=visual["screens"].toArray();for(int i=0;i<screens.size();i++){auto s=screens[i].toObject();if(s["id"]!=screenId)continue;auto elements=s["elements"].toArray();selection=uniqueId(elements,"facility-");QString widget=selectedKind=="buy"?"shop":selectedKind=="service"?"services":selectedKind=="diet"?"diets":"sell";int width=visual["width"].toInt(1280),height=visual["height"].toInt(720);QJsonObject e{{"id",selection},{"kind",widget},{"x",40},{"y",100},{"width",qMax(320,width-80)},{"height",qMax(200,height-180)},{"anchorX","stretch"},{"anchorY","stretch"}};if(widget=="shop"||widget=="sell")e["shopId"]=shop->currentData().toString();elements.append(e);s["elements"]=elements;screens[i]=s;}visual["screens"]=screens;project["visual"]=visual;}
  replaceProject(project);
}
