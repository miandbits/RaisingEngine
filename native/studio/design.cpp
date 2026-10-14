#include "studio.hpp"
#include "design.hpp"
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QRawFont>
#include <QPushButton>
#include <QVBoxLayout>

void StudioWindow::importFont(){
  if(bridge.busy()||projectData().isEmpty())return;
  auto file=QFileDialog::getOpenFileName(this,tr("프로젝트 폰트 가져오기"),{},tr("TrueType / OpenType (*.ttf *.otf)"));if(file.isEmpty())return;
  QRawFont face(file,24);if(!face.isValid()){errorLabel->setText(tr("읽을 수 없는 폰트 파일입니다."));return;}
  request("importFont",{{"file",file}},[this](QJsonObject){status->setText(tr("폰트 등록됨 · 디자인 > 공통 스타일에서 선택하세요."));});
}

void StudioWindow::editDesignSystem(){
  if(bridge.busy()||projectData().isEmpty())return;
  auto project=projectData();auto visual=project["visual"].toObject();auto styles=visual["styles"].toArray();auto defaults=visual["defaultStyle"].toObject();
  QDialog dialog(this);dialog.setObjectName("designSystemDialog");dialog.setWindowTitle(tr("공통 스타일 · 프로젝트 폰트"));dialog.resize(820,720);
  auto outer=new QVBoxLayout(&dialog);auto layout=new QHBoxLayout;outer->addLayout(layout);
  auto left=new QVBoxLayout;layout->addLayout(left,1);auto list=new QListWidget;list->setObjectName("designStyles");left->addWidget(list);
  auto add=new QPushButton(tr("＋ 공통 스타일"));add->setObjectName("addSharedStyle");auto remove=new QPushButton(tr("스타일 삭제"));left->addWidget(add);left->addWidget(remove);
  auto form=new QFormLayout;layout->addLayout(form,2);
  auto idLabel=new QLabel;auto label=new QLineEdit;label->setObjectName("styleLabel");form->addRow(tr("ID"),idLabel);form->addRow(tr("이름"),label);
  auto font=new QComboBox;font->setObjectName("styleFont");font->addItem(tr("상속 / 기본 폰트"),QString());
  for(auto v:assets){auto a=v.toObject();if(a["kind"]!="font")continue;const auto family=projectFontFamily(a["absolutePath"].toString());font->addItem(family+" · "+a["id"].toString(),a["id"].toString());}
  form->addRow(tr("프로젝트 폰트"),font);
  QMap<QString,QSpinBox*> numbers;
  for(const auto& pair:QList<QPair<QString,QString>>{{"fontSize",tr("글자 크기")},{"padding",tr("내부 여백")},{"radius",tr("모서리")},{"borderWidth",tr("테두리 두께")}}){auto spin=new QSpinBox;spin->setObjectName("style-"+pair.first);spin->setRange(-1,pair.first=="fontSize"?120:pair.first=="borderWidth"?32:pair.first=="radius"?100:128);spin->setSpecialValueText(tr("상속"));numbers[pair.first]=spin;form->addRow(pair.second,spin);}
  QMap<QString,QLineEdit*> colors;
  for(const auto& pair:QList<QPair<QString,QString>>{{"color",tr("글자 색")},{"fill",tr("바탕색")},{"borderColor",tr("테두리 색")},{"hoverFill",tr("마우스 올림")},{"pressedFill",tr("누른 상태")},{"disabledColor",tr("비활성 글자")}}){auto field=new QLineEdit;field->setObjectName("style-"+pair.first);field->setPlaceholderText(pair.first=="fill"?tr("상속 · #RRGGBB 또는 #RRGGBBAA"):tr("상속 · #RRGGBB"));colors[pair.first]=field;form->addRow(pair.second,field);}
  auto align=new QComboBox;align->addItem(tr("상속"),QString());align->addItem(tr("왼쪽"),"left");align->addItem(tr("가운데"),"center");align->addItem(tr("오른쪽"),"right");form->addRow(tr("문자 정렬"),align);
  auto sample=new QLabel(tr("가나다 ABC 123 · 육성 시뮬레이션"));sample->setMinimumHeight(80);sample->setWordWrap(true);form->addRow(tr("폰트 미리보기"),sample);
  auto preview=[&]{QFont f("Noto Sans KR");for(auto v:assets){auto a=v.toObject();if(a["id"]==font->currentData().toString()){QRawFont raw(a["absolutePath"].toString(),24);if(raw.isValid())f.setFamily(raw.familyName());}}f.setPixelSize(numbers["fontSize"]->value()>0?numbers["fontSize"]->value():24);sample->setFont(f);};
  connect(font,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{preview();});connect(numbers["fontSize"],qOverload<int>(&QSpinBox::valueChanged),&dialog,[&]{preview();});
  auto apply=new QComboBox;apply->setObjectName("styleApplyScope");apply->addItem(tr("기존 연결 유지"),"keep");if(!selection.isEmpty()){apply->addItem(tr("선택 요소에 연결 · 개별 모양 설정 제거"),"selected");apply->addItem(tr("프로젝트 전체의 같은 종류에 연결 · 개별 모양 설정 제거"),"kind");}form->addRow(tr("적용 범위"),apply);
  auto hint=new QLabel(tr("적용 순서: 프로젝트 기본 → 공통 스타일 → 요소별 설정.\n빈 값은 상속합니다. 폰트 파일은 디자인 메뉴에서 가져올 수 있습니다.\n공통 스타일을 고치면 연결된 모든 화면에 반영됩니다."));hint->setWordWrap(true);outer->addWidget(hint);
  int selected=-1;bool fillingForm=false;
  auto store=[&]{if(selected<0||fillingForm)return;auto s=selected==0?defaults:styles[selected-1].toObject();if(selected)s["label"]=label->text();for(auto it=numbers.begin();it!=numbers.end();++it){if(it.value()->value()<0)s.remove(it.key());else s[it.key()]=it.key()=="fontSize"?qMax(10,it.value()->value()):it.value()->value();}for(auto it=colors.begin();it!=colors.end();++it){auto text=it.value()->text().trimmed();if(text.isEmpty())s.remove(it.key());else s[it.key()]=text;}for(auto pair:QList<QPair<QString,QComboBox*>>{{"fontAssetId",font},{"align",align}}){auto value=pair.second->currentData().toString();if(value.isEmpty())s.remove(pair.first);else s[pair.first]=value;}if(selected==0)defaults=s;else styles[selected-1]=s;};
  auto reload=[&]{list->clear();list->addItem(tr("프로젝트 기본 스타일"));for(auto v:styles)list->addItem(v.toObject()["label"].toString());};
  connect(list,&QListWidget::currentRowChanged,&dialog,[&](int row){store();selected=row;if(row<0)return;fillingForm=true;auto s=row==0?defaults:styles[row-1].toObject();idLabel->setText(s["id"].toString("default"));label->setText(s["label"].toString());label->setEnabled(row>0);for(auto it=numbers.begin();it!=numbers.end();++it)it.value()->setValue(s.contains(it.key())?s[it.key()].toInt():-1);for(auto it=colors.begin();it!=colors.end();++it)it.value()->setText(s[it.key()].toString());font->setCurrentIndex(qMax(0,font->findData(s["fontAssetId"].toString())));align->setCurrentIndex(qMax(0,align->findData(s["align"].toString())));fillingForm=false;preview();});
  connect(add,&QPushButton::clicked,&dialog,[&]{store();int suffix=1;QString id;while(true){id=QString("style-%1").arg(suffix++);bool found=false;for(auto s:styles)if(s.toObject()["id"]==id)found=true;if(!found)break;}styles.append(QJsonObject{{"id",id},{"label",tr("새 스타일")}});selected=-1;reload();list->setCurrentRow(styles.size());});
  connect(remove,&QPushButton::clicked,&dialog,[&]{if(selected<=0)return;auto id=styles[selected-1].toObject()["id"];for(auto s:visual["screens"].toArray())for(auto e:s.toObject()["elements"].toArray())if(e.toObject()["styleId"]==id){QMessageBox::information(&dialog,tr("연결된 스타일"),tr("사용 중인 요소에 다른 스타일을 연결한 뒤 삭제하세요."));return;}styles.removeAt(selected-1);selected=-1;reload();list->setCurrentRow(0);});
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);outer->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);reload();list->setCurrentRow(0);
  if(dialog.exec()!=QDialog::Accepted)return;store();visual["defaultStyle"]=defaults;visual["styles"]=styles;
  if(selected>0&&apply->currentData()!="keep") {const auto kind=currentElement()["kind"];auto screens=visual["screens"].toArray();for(int i=0;i<screens.size();i++){auto s=screens[i].toObject();auto elements=s["elements"].toArray();for(int j=0;j<elements.size();j++){auto e=elements[j].toObject();if((apply->currentData()=="selected"&&s["id"]==screenId&&e["id"]==selection)||(apply->currentData()=="kind"&&e["kind"]==kind)){for(auto key:styleFields())e.remove(key);e["styleId"]=styles[selected-1].toObject()["id"];elements[j]=e;}}s["elements"]=elements;screens[i]=s;}visual["screens"]=screens;}
  project["visual"]=visual;replaceProject(project);
}
