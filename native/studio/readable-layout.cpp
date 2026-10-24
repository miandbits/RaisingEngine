#include "studio.hpp"
#include "components.hpp"
#include "renderer-preview.hpp"
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QColorDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <algorithm>

namespace {
const QSet<QString> readingKinds{"text","image","dialogue","choices","button","stats","meter","schedule","result","input","saveSlots","loadSlots","playerSettings"};
QString readingId(const QListWidgetItem* item) { return item ? item->data(Qt::UserRole).toJsonObject().value("elementId").toString() : QString(); }
class OptionalReadingSize:public QSpinBox {
  int first;
public:
  explicit OptionalReadingSize(int minimum):first(minimum){setKeyboardTracking(false);}
protected:
  void stepBy(int steps) override {if(value()==0&&steps>0)setValue(first+steps-1);else if(value()>=first&&value()+steps<first)setValue(0);else QSpinBox::stepBy(steps);}
};
}

void StudioWindow::editReadableLayout() {
  if(bridge.busy()||projectData().isEmpty())return;
  auto project=projectData(),visual=project.value("visual").toObject();auto screens=visual.value("screens").toArray();if(screens.empty())return;
  const auto original=project;const auto revision=view.value("revision"),root=view.value("root");
  QDialog dialog(this);dialog.setObjectName("readableLayoutDialog");dialog.setWindowTitle(tr("읽기 편한 배치 · 휴대전화와 큰 글자"));dialog.resize(940,730);
  auto outer=new QVBoxLayout(&dialog);
  auto enabled=new QCheckBox(tr("이 프로젝트에서 읽기 편한 배치 사용"));enabled->setObjectName("readableEnabled");enabled->setChecked(visual.contains("readableLayout"));outer->addWidget(enabled);
  auto disableHelp=new QLabel(tr("이 기능을 끄고 저장하면 모든 화면의 읽기 순서가 제거됩니다. 실행 취소로 복원할 수 있습니다."));disableHelp->setObjectName("readableDisableHelp");disableHelp->setWordWrap(true);disableHelp->setVisible(!enabled->isChecked());outer->addWidget(disableHelp);
  auto global=new QHBoxLayout;outer->addLayout(global);const auto config=visual.value("readableLayout").toObject();QMap<QString,QSpinBox*> fields;
  const auto number=[&](QString key,QString label,int low,int high,int value){auto column=new QVBoxLayout;column->addWidget(new QLabel(label));auto field=new QSpinBox;field->setObjectName("readable-"+key);field->setRange(low,high);field->setValue(config.value(key).toInt(value));field->setSuffix(tr(" pt"));column->addWidget(field);global->addLayout(column);fields[key]=field;};
  number("maxShortEdge",tr("자동 전환 기준: 짧은 변"),320,1200,600);number("fontSize",tr("기본 글자 크기"),16,32,18);number("minTargetSize",tr("최소 터치 영역"),44,96,44);number("padding",tr("페이지 안쪽 여백"),0,64,16);number("gap",tr("항목 간격"),0,48,12);number("maxWidth",tr("본문 최대 너비"),320,1600,760);
  auto backgroundRow=new QHBoxLayout;outer->addLayout(backgroundRow);backgroundRow->addWidget(new QLabel(tr("읽기 화면 배경색")));auto background=new QLineEdit(config.value("background").toString("#172c29"));background->setObjectName("readableBackground");background->setMaxLength(7);backgroundRow->addWidget(background);auto pickBackground=new QPushButton(tr("배경색 선택…"));backgroundRow->addWidget(pickBackground);connect(pickBackground,&QPushButton::clicked,&dialog,[&]{const auto color=QColorDialog::getColor(QColor(background->text()),&dialog,tr("읽기 화면 배경색"));if(color.isValid())background->setText(color.name());});
  auto screenRow=new QHBoxLayout;outer->addLayout(screenRow);screenRow->addWidget(new QLabel(tr("화면")));auto screenChoice=new QComboBox;screenChoice->setObjectName("readableScreen");for(auto value:screens){auto s=value.toObject();screenChoice->addItem(s.value("label").toString(s.value("id").toString()),s.value("id").toString());}screenRow->addWidget(screenChoice,1);
  auto screenEnabled=new QCheckBox(tr("이 화면에 읽기 순서 적용"));screenEnabled->setObjectName("readableScreenEnabled");screenRow->addWidget(screenEnabled);
  auto help=new QLabel(tr("기존 요소를 읽기 순서대로 선택하세요. 목록에 없는 장식은 생략됩니다. 조건, 데이터 연결, 버튼 동작은 유지됩니다. 하단 버튼은 스크롤 영역 밖에서 줄바꿈됩니다. 본문 목록을 비워 둘 수도 있습니다."));help->setWordWrap(true);outer->addWidget(help);
  auto columns=new QHBoxLayout;outer->addLayout(columns,1);
  auto makeList=[&](QString title,QString name){auto group=new QGroupBox(title);auto layout=new QVBoxLayout(group);auto list=new QListWidget;list->setObjectName(name);list->setMinimumWidth(200);layout->addWidget(list);columns->addWidget(group,1);return list;};
  auto available=makeList(tr("추가할 수 있는 요소"),"readableAvailable"),items=makeList(tr("읽기 순서"),"readableItems"),actions=makeList(tr("하단 버튼"),"readableActions");
  auto commands=new QHBoxLayout;outer->addLayout(commands);
  auto command=[&](QString label,QString name){auto button=new QPushButton(label);button->setObjectName(name);commands->addWidget(button);return button;};
  auto add=command(tr("본문에 추가"),"readableAdd"),addAction=command(tr("하단에 추가"),"readableAddAction"),remove=command(tr("제거"),"readableRemove"),up=command(tr("위로 이동"),"readableUp"),down=command(tr("아래로 이동"),"readableDown"),seed=command(tr("남은 요소 모두 추가"),"readableSeed");
  auto overrides=new QHBoxLayout;outer->addLayout(overrides);auto font=new OptionalReadingSize(16),imageHeight=new OptionalReadingSize(48);font->setObjectName("readableItemFont");font->setRange(0,64);font->setSpecialValueText(tr("기본 글자 크기"));imageHeight->setObjectName("readableImageHeight");imageHeight->setRange(0,320);imageHeight->setSpecialValueText(tr("기본값 (160 pt)"));overrides->addWidget(new QLabel(tr("선택한 항목의 글자 크기")));overrides->addWidget(font);overrides->addWidget(new QLabel(tr("이미지 높이")));overrides->addWidget(imageHeight);overrides->addStretch();
  auto previewRow=new QHBoxLayout;outer->addLayout(previewRow);auto preview=new QComboBox;preview->setObjectName("readablePreviewSize");preview->addItem("390 × 844",QSize(390,844));preview->addItem("320 × 568",QSize(320,568));preview->addItem("844 × 390",QSize(844,390));preview->addItem("1280 × 720",QSize(1280,720));previewRow->addWidget(new QLabel(tr("미리보기 화면 크기")));previewRow->addWidget(preview);auto previewState=new QLabel;previewState->setObjectName("readablePreviewStatus");previewState->setWordWrap(true);previewRow->addWidget(previewState,1);
  auto previewHelp=new QLabel(tr("저장 후 실제 렌더러 검사에서 이 화면 크기와 100~200% 글자 크기를 확인하세요. 미리보기는 게임 진행을 바꾸지 않습니다. 포인트는 웹의 CSS 픽셀에 해당합니다."));previewHelp->setWordWrap(true);outer->addWidget(previewHelp);
  auto error=new QLabel;error->setObjectName("readableError");error->setWordWrap(true);outer->addWidget(error);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons->setObjectName("readableButtons");auto openPreview=buttons->addButton(tr("저장하고 미리보기 열기"),QDialogButtonBox::ActionRole);openPreview->setObjectName("readableOpenPreview");outer->addWidget(buttons);
  int current=-1;bool filling=false,wantsPreview=false;QMap<QString,QJsonObject> leaves;QListWidget* selected=items;
  auto append=[&](QListWidget* list,QJsonObject value){const auto id=value.value("elementId").toString();const auto e=leaves.value(id);auto row=new QListWidgetItem(e.isEmpty()?tr("찾을 수 없음: %1").arg(id):e.value("label").toString(id)+" · "+e.value("kind").toString(),list);row->setData(Qt::UserRole,QVariant::fromValue(value));row->setToolTip(id);};
  auto serialize=[](QListWidget* list,bool ids){QJsonArray result;for(int i=0;i<list->count();++i)result.append(ids?QJsonValue(readingId(list->item(i))):QJsonValue(list->item(i)->data(Qt::UserRole).toJsonObject()));return result;};
  auto storeScreen=[&]{if(current<0)return;auto s=screens[current].toObject();if(screenEnabled->isChecked()){auto layout=s.value("readableLayout").toObject();layout["items"]=serialize(items,false);if(actions->count()>0||layout.contains("actions"))layout["actions"]=serialize(actions,true);s["readableLayout"]=layout;}else s.remove("readableLayout");screens[current]=s;};
  auto refreshAvailable=[&]{const auto selection=readingId(available->currentItem());QSet<QString> used;for(auto list:{items,actions})for(int i=0;i<list->count();++i)used.insert(readingId(list->item(i)));available->clear();for(auto it=leaves.begin();it!=leaves.end();++it)if(!used.contains(it.key())&&readingKinds.contains(it.value().value("kind").toString())){append(available,{{"elementId",it.key()}});if(it.key()==selection)available->setCurrentRow(available->count()-1);}if(available->currentRow()<0)available->setCurrentRow(0);};
  auto refreshState=[&]{const auto size=preview->currentData().toSize();const bool active=enabled->isChecked()&&screenEnabled->isChecked()&&qMin(size.width(),size.height())<=fields["maxShortEdge"]->value();previewState->setProperty("readableActive",active);previewState->setText(active?tr("읽기 배치 적용 · 세로 스크롤, 캔버스 축소 없음"):tr("기존 캔버스 적용 · 더 큰 화면은 렌더러에서 읽기 배치를 항상 켜서 확인하세요"));const bool editable=screenEnabled->isChecked();for(auto w:{add,addAction,remove,up,down,seed})w->setEnabled(editable);addAction->setEnabled(editable&&leaves.value(readingId(available->currentItem())).value("kind")=="button"&&actions->count()<12);};
  auto loadScreen=[&]{filling=true;current=screenChoice->currentIndex();const auto s=screens[current].toObject();screenEnabled->setChecked(s.contains("readableLayout"));leaves.clear();for(auto value:canvasElements(visual,s.value("elements").toArray(),{visual.value("width").toInt(1280),visual.value("height").toInt(720)})){auto e=value.toObject();leaves[e.value("id").toString()]=e;}items->clear();actions->clear();const auto layout=s.value("readableLayout").toObject();for(auto v:layout.value("items").toArray())append(items,v.toObject());for(auto v:layout.value("actions").toArray())append(actions,{{"elementId",v}});refreshAvailable();font->setValue(0);imageHeight->setValue(0);font->setEnabled(false);imageHeight->setEnabled(false);filling=false;refreshState();};
  auto selectionChanged=[&](QListWidget* list){if(filling)return;selected=list;for(auto other:{available,items,actions})if(other!=list)other->clearSelection();const QSignalBlocker b(font),c(imageHeight);const auto row=list->currentItem();const auto v=row?row->data(Qt::UserRole).toJsonObject():QJsonObject{};font->setValue(v.value("fontSize").toInt());imageHeight->setValue(v.value("imageHeight").toInt());font->setEnabled(list==items&&row&&screenEnabled->isChecked());imageHeight->setEnabled(list==items&&row&&leaves.value(readingId(row)).value("kind")=="image"&&screenEnabled->isChecked());refreshState();};
  for(auto list:{available,items,actions}){connect(list,&QListWidget::currentRowChanged,&dialog,[&,list]{selectionChanged(list);});connect(list,&QListWidget::itemClicked,&dialog,[&,list]{selectionChanged(list);});}
  connect(qApp,&QApplication::focusChanged,&dialog,[&](QWidget*,QWidget* focused){if(focused)for(auto list:{available,items,actions})if(list==focused||list->isAncestorOf(focused))selectionChanged(list);});
  connect(screenChoice,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{storeScreen();loadScreen();});
  connect(screenEnabled,&QCheckBox::toggled,&dialog,[&]{if(!filling){refreshState();selectionChanged(selected);}});
  connect(enabled,&QCheckBox::toggled,&dialog,[&]{disableHelp->setVisible(!enabled->isChecked());refreshState();});connect(preview,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{refreshState();});connect(fields["maxShortEdge"],qOverload<int>(&QSpinBox::valueChanged),&dialog,[&]{refreshState();});
  auto addSelected=[&](QListWidget* list){const auto id=readingId(available->currentItem());if(id.isEmpty()||!screenEnabled->isChecked())return;if(list==actions&&(leaves.value(id).value("kind")!="button"||actions->count()>=12))return;append(list,{{"elementId",id}});refreshAvailable();list->setCurrentRow(list->count()-1);selectionChanged(list);};
  connect(add,&QPushButton::clicked,&dialog,[&]{addSelected(items);});connect(addAction,&QPushButton::clicked,&dialog,[&]{addSelected(actions);});
  connect(remove,&QPushButton::clicked,&dialog,[&]{if(selected==available)return;auto list=selected;delete list->takeItem(list->currentRow());refreshAvailable();selectionChanged(list);});
  auto move=[&](int offset){if(selected==available)return;int row=selected->currentRow();if(row<0||row+offset<0||row+offset>=selected->count())return;auto item=selected->takeItem(row);selected->insertItem(row+offset,item);selected->setCurrentRow(row+offset);};
  connect(up,&QPushButton::clicked,&dialog,[&]{move(-1);});connect(down,&QPushButton::clicked,&dialog,[&]{move(1);});
  connect(seed,&QPushButton::clicked,&dialog,[&]{QList<QJsonObject> remaining;for(int i=0;i<available->count();++i)remaining.append(leaves.value(readingId(available->item(i))));std::stable_sort(remaining.begin(),remaining.end(),[](const QJsonObject& a,const QJsonObject& b){return a.value("y").toDouble()==b.value("y").toDouble()?a.value("x").toDouble()<b.value("x").toDouble():a.value("y").toDouble()<b.value("y").toDouble();});for(const auto& e:remaining)append(items,{{"elementId",e.value("id")}});refreshAvailable();refreshState();});
  auto updateItem=[&](QString key,int value){if(filling||selected!=items||!items->currentItem())return;auto v=items->currentItem()->data(Qt::UserRole).toJsonObject();if(value==0)v.remove(key);else v[key]=qMax(key=="fontSize"?16:48,value);items->currentItem()->setData(Qt::UserRole,QVariant::fromValue(v));};
  connect(font,qOverload<int>(&QSpinBox::valueChanged),&dialog,[&](int v){if(v>0&&v<16){const QSignalBlocker b(font);font->setValue(v=16);}updateItem("fontSize",v);});connect(imageHeight,qOverload<int>(&QSpinBox::valueChanged),&dialog,[&](int v){if(v>0&&v<48){const QSignalBlocker b(imageHeight);imageHeight->setValue(v=48);}updateItem("imageHeight",v);});
  auto save=[&]{if(view.value("revision")!=revision||view.value("root")!=root){error->setText(tr("프로젝트가 변경되었습니다. 이 창을 다시 열고 저장하세요."));return;}storeScreen();if(!enabled->isChecked()){visual.remove("readableLayout");for(int i=0;i<screens.size();++i){auto screen=screens[i].toObject();screen.remove("readableLayout");screens[i]=screen;}visual["screens"]=screens;project["visual"]=visual;dialog.accept();return;}for(auto value:screens){const auto s=value.toObject();if(!s.contains("readableLayout"))continue;QMap<QString,QString> kinds;for(auto v:canvasElements(visual,s.value("elements").toArray(),{visual.value("width").toInt(1280),visual.value("height").toInt(720)})){auto e=v.toObject();kinds[e.value("id").toString()]=e.value("kind").toString();}QSet<QString> used;auto layout=s.value("readableLayout").toObject();for(auto v:layout.value("items").toArray()){auto id=v.toObject().value("elementId").toString();if(used.contains(id)||!readingKinds.contains(kinds.value(id))){error->setText(tr("화면 %1의 누락, 중복 또는 미지원 요소를 수정하세요: %2").arg(s.value("id").toString(),id));return;}used.insert(id);}for(auto v:layout.value("actions").toArray()){auto id=v.toString();if(used.contains(id)||kinds.value(id)!="button"){error->setText(tr("화면 %1의 하단 항목은 중복 없는 기존 버튼이어야 합니다: %2").arg(s.value("id").toString(),id));return;}used.insert(id);}}
    if(enabled->isChecked()){if(!QRegularExpression("^#[0-9a-fA-F]{6}$").match(background->text()).hasMatch()){error->setText(tr("배경색을 #RRGGBB 형식으로 지정하세요."));return;}auto next=config;for(auto it=fields.begin();it!=fields.end();++it)if(config.contains(it.key())||it.value()->value()!=QMap<QString,int>{{"maxShortEdge",600},{"fontSize",18},{"minTargetSize",44},{"padding",16},{"gap",12},{"maxWidth",760}}.value(it.key()))next[it.key()]=it.value()->value();if(config.contains("background")||background->text()!="#172c29")next["background"]=background->text();visual["readableLayout"]=next;}else visual.remove("readableLayout");visual["screens"]=screens;project["visual"]=visual;dialog.accept();};
  connect(buttons,&QDialogButtonBox::accepted,&dialog,save);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);connect(openPreview,&QPushButton::clicked,&dialog,[&]{wantsPreview=true;save();});
  screenChoice->setCurrentIndex(qMax(0,screenChoice->findData(managedScreenId())));if(current<0)loadScreen();
  if(dialog.exec()!=QDialog::Accepted)return;
  const auto size=preview->currentData().toSize();const auto target=screenChoice->currentData().toString();
  auto launchPreview=[this,size,target,root]{if(view.value("root")!=root||!lastError.isEmpty())return;openScreen(target);rendererPreview->setCanvasViewport(size);rendererPreview->show();rendererPreview->raise();};
  if(project!=original){if(wantsPreview)connect(this,&StudioWindow::updated,this,launchPreview,Qt::SingleShotConnection);replaceProject(project);}
  else if(wantsPreview)launchPreview();
}
