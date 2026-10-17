#include "studio.hpp"
#include "logic-editor.hpp"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <memory>

void StudioWindow::editPresentationVariants() {
  if(busy()||currentElement().isEmpty())return;
  const auto original=currentElement();const auto revision=view.value("revision"),root=view.value("root");
  const auto selected=selection,screen=screenId;
  editElementPresentationVariants(original,this,[=,this](QJsonArray rules,QString& error){
    if(busy()||view.value("revision")!=revision||view.value("root")!=root||selection!=selected||screenId!=screen){error=tr("편집 대상이 변경되었습니다. 다시 열어 주세요.");return false;}
    updateElement([=](QJsonObject& element){if(rules.isEmpty())element.remove("variants");else element["variants"]=rules;});return true;
  });
}

void StudioWindow::editElementPresentationVariants(QJsonObject original,QWidget* parent,std::function<bool(QJsonArray,QString&)> apply) {
  auto rules=std::make_shared<QJsonArray>(original.value("variants").toArray());
  auto dialog=new QDialog(parent);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setObjectName("presentationVariantsDialog");
  dialog->setWindowTitle(tr("조건별 표현"));dialog->resize(680,520);
  auto outer=new QVBoxLayout(dialog);auto help=new QLabel(tr("위에서부터 처음 조건이 맞는 표현을 사용합니다. 맞는 조건이 없으면 원래 모습으로 돌아갑니다.\n화면·위치·게임 데이터는 유지됩니다. 활동 필터는 기본적으로 결과 표시 중에 적용합니다. 부품에 활동을 연결하면 그 활동을 기준으로 합니다.\n애니메이션은 같은 플러그인의 컨트롤러를 선택합니다. 세부 동작 규칙은 애니메이션 편집기에서 만드세요."));help->setWordWrap(true);outer->addWidget(help);
  auto list=new QListWidget;list->setObjectName("presentationVariantList");outer->addWidget(list);
  auto refresh=[=](int row){list->clear();for(const auto& v:*rules){const auto r=v.toObject();QStringList properties;const auto values=r.value("values").toObject();for(auto it=values.begin();it!=values.end();++it)properties.append(it.key());list->addItem(r.value("label").toString()+" · "+properties.join(", "));}if(!rules->isEmpty())list->setCurrentRow(qBound(0,row,int(rules->size())-1));};
  auto edit=[=,this](int row,bool added){
    if(row<0||row>=rules->size())return;
    const auto rule=rules->at(row).toObject();auto d=new QDialog(dialog);d->setAttribute(Qt::WA_DeleteOnClose);d->setObjectName("presentationVariantEdit");d->setWindowTitle(tr("표현 연결"));d->resize(590,420);
    auto layout=new QVBoxLayout(d);auto form=new QFormLayout;layout->addLayout(form);
    auto name=new QLineEdit(rule.value("label").toString());name->setObjectName("variantName");name->setMaxLength(100);form->addRow(tr("표현 이름"),name);
    auto activity=new QComboBox;activity->setObjectName("variantActivity");activity->addItem(tr("활동에 관계없이"),"");
    for(const auto& doc:view.value("documents").toArray()){const auto obj=doc.toObject();if(obj.value("kind")=="activity"){const auto data=obj.value("data").toObject();activity->addItem(data.value("label").toString(data.value("name").toString(data.value("id").toString())),data.value("id").toString());}}
    const auto ids=rule.value("activityIds").toArray();if(ids.size()==1){const auto id=ids.first().toString();if(activity->findData(id)<0)activity->addItem(tr("누락된 활동 · ")+id,id);activity->setCurrentIndex(activity->findData(id));}
    // Multi-activity filters authored elsewhere are preserved until explicitly changed.
    if(ids.size()>1){activity->addItem(tr("기존 여러 활동 유지"),"__preserve");activity->setCurrentIndex(activity->count()-1);}form->addRow(tr("활동 결과"),activity);
    auto condition=std::make_shared<QJsonValue>(rule.value("when"));auto conditionButton=new QPushButton(tr("조건 편집…"));conditionButton->setObjectName("variantCondition");form->addRow(tr("추가 조건"),conditionButton);
    connect(conditionButton,&QPushButton::clicked,d,[=,this]{auto editor=new LogicEditor(*condition,"condition",view,d);editor->setAttribute(Qt::WA_DeleteOnClose);connect(editor,&QDialog::accepted,d,[=]{*condition=editor->value();});editor->open();});
    auto property=new QComboBox;property->setObjectName("variantProperty");const auto kind=original.value("kind").toString();
    if(kind=="text"||kind=="button")property->addItem(tr("문구"),"text");
    if(kind=="image")property->addItem(tr("이미지"),"imageAssetId");
    if(kind=="plugin")property->addItem(tr("애니메이션 컨트롤러"),"extensionRef");
    property->addItem(tr("문자 색"),"color");property->addItem(tr("배경 색"),"fill");property->addItem(tr("불투명도 (0~1)"),"opacity");
    const auto values=rule.value("values").toObject();for(int i=0;i<property->count();++i)if(values.contains(property->itemData(i).toString())){property->setCurrentIndex(i);break;}form->addRow(tr("바꿀 속성"),property);
    auto value=new QLineEdit;value->setObjectName("variantValue");auto resource=new QComboBox;resource->setObjectName("variantResource");form->addRow(tr("값"),value);form->addRow(tr("연결 대상"),resource);
    auto fill=[=,this]{const auto key=property->currentData().toString();const bool isResource=key=="imageAssetId"||key=="extensionRef";value->setVisible(!isResource);resource->setVisible(isResource);resource->clear();
      if(key=="imageAssetId")for(const auto& doc:view.value("documents").toArray())if(doc.toObject().value("kind")=="assets")for(const auto& asset:doc.toObject().value("data").toObject().value("assets").toArray()){const auto a=asset.toObject();if(a.value("kind")=="image")resource->addItem(a.value("label").toString(a.value("id").toString()),a.value("id").toString());}
      if(key=="extensionRef")for(int i=0;i<pluginRefField->count();++i)if(!pluginRefField->itemData(i).toString().isEmpty())resource->addItem(pluginRefField->itemText(i),pluginRefField->itemData(i));
      if(isResource&&values.contains(key)){int index=resource->findData(values.value(key).toString());if(index<0){resource->addItem(tr("누락된 참조 · ")+values.value(key).toString(),values.value(key).toString());index=resource->count()-1;}resource->setCurrentIndex(index);}
      value->setText(values.value(key).isDouble()?QString::number(values.value(key).toDouble()):values.value(key).toString());};
    connect(property,qOverload<int>(&QComboBox::currentIndexChanged),d,[=](int){fill();});fill();
    auto message=new QLabel;message->setWordWrap(true);layout->addWidget(message);auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,d,&QDialog::reject);
    connect(d,&QDialog::rejected,dialog,[=]{if(added){rules->removeAt(row);refresh(row);}});
    connect(buttons,&QDialogButtonBox::accepted,d,[=]{
      if(name->text().trimmed().isEmpty()){message->setText(tr("표현 이름을 입력하세요."));return;}
      const auto key=property->currentData().toString();QJsonValue next=value->text();if(key=="imageAssetId"||key=="extensionRef"){if(resource->currentIndex()<0){message->setText(tr("연결할 리소스를 먼저 만드세요."));return;}next=resource->currentData().toString();}
      if(key=="opacity"){bool ok=false;const auto number=value->text().toDouble(&ok);if(!ok||number<0||number>1){message->setText(tr("불투명도는 0~1입니다."));return;}next=number;}
      if((key=="color"||key=="fill")&&!QColor(value->text()).isValid()){message->setText(tr("유효한 색상을 입력하세요. 예: #80c0a0"));return;}
      auto updated=rule,newValues=values;newValues[key]=next;updated["label"]=name->text().trimmed();updated["values"]=newValues;updated["when"]=*condition;
      const auto id=activity->currentData().toString();if(id!="__preserve"){updated.remove("activityIds");if(!id.isEmpty())updated["activityIds"]=QJsonArray{id};}
      (*rules)[row]=updated;refresh(row);d->accept();
    });d->open();
  };
  auto row=new QHBoxLayout;outer->addLayout(row);
  for(const auto& pair:QList<QPair<QString,QString>>{{"Add",tr("추가")},{"Edit",tr("편집")},{"Remove",tr("삭제")},{"Up",tr("위로")},{"Down",tr("아래로")}}){auto button=new QPushButton(pair.second);button->setObjectName("variant"+pair.first);row->addWidget(button);connect(button,&QPushButton::clicked,dialog,[=]{const auto index=list->currentRow();if(pair.first=="Add"){if(rules->size()>=64)return;rules->append(QJsonObject{{"label",tr("새 표현")},{"when",QJsonObject{{"op","always"}}},{"values",QJsonObject{{"opacity",1}}}});refresh(rules->size()-1);edit(rules->size()-1,true);}else if(index>=0){if(pair.first=="Edit")edit(index,false);else if(pair.first=="Remove"){rules->removeAt(index);refresh(index);}else{const int target=index+(pair.first=="Up"?-1:1);if(target>=0&&target<rules->size()){const auto item=rules->at(index);rules->removeAt(index);rules->insert(target,item);refresh(target);}}}});}
  auto error=new QLabel;error->setWordWrap(true);outer->addWidget(error);auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Save)->setText(tr("표현 연결 적용"));outer->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
  connect(buttons,&QDialogButtonBox::accepted,dialog,[=]{QString problem;if(!apply(*rules,problem)){error->setText(problem);return;}dialog->accept();});
  refresh(0);dialog->open();
}
