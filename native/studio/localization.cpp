#include "studio.hpp"
#include "design.hpp"
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSet>

void StudioWindow::editLocalization(){
 if(bridge.busy()||projectData().isEmpty())return;
 auto project=projectData();auto config=project["localization"].toObject();
 if(config.isEmpty())config={{"defaultLocale","ko"},{"fallbackLocale","ko"},{"locales",QJsonArray{QJsonObject{{"id","ko"},{"label",tr("한국어")}}}},{"entries",QJsonArray{}}};
 auto locales=config["locales"].toArray(),entries=config["entries"].toArray();QMap<QString,QString> sources;
 for(auto v:view["localizationSources"].toArray()){auto m=v.toObject();sources[m["key"].toString()]=m["source"].toString();}
 QSet<QString> known;for(auto e:entries)known.insert(e.toObject()["key"].toString());
 for(auto it=sources.begin();it!=sources.end();++it)if(!known.contains(it.key()))entries.append(QJsonObject{{"key",it.key()},{"source",it.value()},{"translations",QJsonObject{}}});
 QDialog dialog(this);dialog.setObjectName("localizationDialog");dialog.setWindowTitle(tr("다국어 작업실"));dialog.resize(1160,780);
 auto layout=new QVBoxLayout(&dialog);auto top=new QHBoxLayout;layout->addLayout(top);
 auto language=new QComboBox;language->setObjectName("translationLocale");auto defaultLocale=new QComboBox;defaultLocale->setObjectName("defaultLocale");auto fallback=new QComboBox;fallback->setObjectName("fallbackLocale");
 top->addWidget(new QLabel(tr("편집 언어")));top->addWidget(language);auto add=new QPushButton(tr("＋ 언어"));add->setObjectName("addLocale");top->addWidget(add);top->addWidget(new QLabel(tr("시작 언어")));top->addWidget(defaultLocale);top->addWidget(new QLabel(tr("대체 언어")));top->addWidget(fallback);
 auto row=new QHBoxLayout;layout->addLayout(row);auto name=new QLineEdit;name->setObjectName("localeLabel");auto font=new QComboBox;font->setObjectName("localeFont");font->addItem(tr("요소의 폰트 유지"),QString());for(auto a:assets){auto o=a.toObject();if(o["kind"]=="font")font->addItem(projectFontFamily(o["absolutePath"].toString())+" · "+o["id"].toString(),o["id"].toString());}row->addWidget(new QLabel(tr("언어 이름")));row->addWidget(name);row->addWidget(new QLabel(tr("이 언어의 폰트")));row->addWidget(font,1);
 auto search=new QLineEdit;search->setObjectName("translationSearch");search->setPlaceholderText(tr("문구·키 검색"));auto missing=new QCheckBox(tr("미번역·재검토만"));missing->setObjectName("translationMissing");auto filters=new QHBoxLayout;filters->addWidget(search,1);filters->addWidget(missing);layout->addLayout(filters);
 auto table=new QTableWidget;table->setObjectName("translationTable");table->setColumnCount(4);table->setHorizontalHeaderLabels({tr("문구 키 / 위치"),tr("현재 원문"),tr("번역 · 비우면 대체 언어/원문"),tr("상태")});table->setRowCount(entries.size());table->setWordWrap(false);table->setSelectionBehavior(QAbstractItemView::SelectRows);table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Interactive);table->setColumnWidth(0,320);table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch);table->setColumnWidth(3,170);layout->addWidget(table,1);
 auto edit=new QPlainTextEdit;edit->setObjectName("translationText");edit->setMaximumHeight(110);edit->setPlaceholderText(tr("선택한 문구 번역 · 여러 줄 입력 가능"));layout->addWidget(edit);
 auto sample=new QLabel;sample->setObjectName("translationPreview");sample->setWordWrap(true);sample->setMinimumHeight(50);layout->addWidget(sample);
 auto acceptSource=new QPushButton(tr("선택 문구의 변경된 원문 확인 · 번역 유지"));acceptSource->setObjectName("acceptTranslationSource");layout->addWidget(acceptSource);
 auto hint=new QLabel(tr("언어별 폰트는 요소 폰트보다 우선합니다. 빈 번역은 대체 언어 → 현재 원문 순으로 표시합니다.\n원문 변경 시 확인 전에는 현재 원문을 표시합니다. ‘긴 문구’는 길이 비교 경고이며 실제 넘침은 캔버스와 플레이로 확인하세요."));hint->setWordWrap(true);layout->addWidget(hint);
 bool fillingForm=false;int activeLocale=-1,activeRow=-1;
 auto filter=[&]{for(int i=0;i<table->rowCount();i++){auto e=entries[i].toObject();auto key=e["key"].toString();auto t=e["translations"].toObject()[language->currentData().toString()].toString();auto source=sources.value(key);bool issue=t.trimmed().isEmpty()||source!=e["source"].toString()||!sources.contains(key);bool match=(key+source+t).contains(search->text(),Qt::CaseInsensitive);table->setRowHidden(i,!match||(missing->isChecked()&&!issue));}};
 auto storeLocale=[&]{if(activeLocale<0)return;auto l=locales[activeLocale].toObject();l["label"]=name->text();auto id=font->currentData().toString();if(id.isEmpty())l.remove("fontAssetId");else l["fontAssetId"]=id;locales[activeLocale]=l;};
 auto updatePreview=[&]{QFont f("Noto Sans KR");for(auto a:assets)if(a.toObject()["id"].toString()==font->currentData().toString())f.setFamily(projectFontFamily(a.toObject()["absolutePath"].toString()));f.setPixelSize(24);sample->setFont(f);sample->setText(edit->toPlainText());};
 auto updateStatus=[&](int i){auto e=entries[i].toObject();auto key=e["key"].toString(),source=sources.value(key),t=e["translations"].toObject()[language->currentData().toString()].toString();QString status=!sources.contains(key)?tr("미사용 · 원문 없음"):source!=e["source"].toString()?tr("원문 변경 · 재검토"):t.trimmed().isEmpty()?tr("미번역 · 대체 표시"):t.size()>qMax(20,source.size()*2)?tr("긴 문구 · 배치 확인"):tr("번역됨");table->item(i,3)->setText(status);};
 auto render=[&]{fillingForm=true;for(int i=0;i<entries.size();i++){auto e=entries[i].toObject();auto key=e["key"].toString();QStringList values={key,sources.value(key,e["source"].toString()),e["translations"].toObject()[language->currentData().toString()].toString(),QString()};for(int c=0;c<4;c++){auto item=new QTableWidgetItem(values[c]);item->setToolTip(values[c]);if(c!=2)item->setFlags(item->flags()&~Qt::ItemIsEditable);table->setItem(i,c,item);}updateStatus(i);}activeRow=-1;edit->clear();edit->setEnabled(false);fillingForm=false;filter();};
 connect(language,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&](int at){if(fillingForm||at<0)return;storeLocale();activeLocale=at;fillingForm=true;auto l=locales[at].toObject();name->setText(l["label"].toString());font->setCurrentIndex(qMax(0,font->findData(l["fontAssetId"].toString())));fillingForm=false;render();});
 auto fillLocales=[&]{fillingForm=true;auto d=defaultLocale->currentData().toString();if(d.isEmpty())d=config["defaultLocale"].toString();auto f=fallback->currentData().toString();if(f.isEmpty())f=config["fallbackLocale"].toString();language->clear();defaultLocale->clear();fallback->clear();for(auto l:locales){auto o=l.toObject();auto text=o["label"].toString()+" ("+o["id"].toString()+")";for(auto b:{language,defaultLocale,fallback})b->addItem(text,o["id"].toString());}defaultLocale->setCurrentIndex(qMax(0,defaultLocale->findData(d)));fallback->setCurrentIndex(qMax(0,fallback->findData(f)));fillingForm=false;language->setCurrentIndex(-1);language->setCurrentIndex(0);};
 connect(add,&QPushButton::clicked,&dialog,[&]{bool ok;auto id=QInputDialog::getText(&dialog,tr("언어 추가"),tr("언어 코드 · en, ja, zh-Hans 등"),QLineEdit::Normal,"en",&ok).trimmed();if(!ok||id.isEmpty())return;if(!QRegularExpression("^[a-z]{2,3}(-[A-Za-z0-9]{2,8})*$").match(id).hasMatch()){QMessageBox::information(&dialog,tr("언어 코드"),tr("올바른 언어 코드를 입력하세요."));return;}for(auto l:locales)if(l.toObject()["id"]==id)return;storeLocale();locales.append(QJsonObject{{"id",id},{"label",id}});activeLocale=-1;fillLocales();language->setCurrentIndex(locales.size()-1);});
 connect(table,&QTableWidget::currentCellChanged,&dialog,[&](int r,int,int,int){if(fillingForm||r<0)return;activeRow=r;fillingForm=true;edit->setEnabled(true);edit->setPlainText(table->item(r,2)->text());fillingForm=false;updatePreview();});
 connect(table,&QTableWidget::cellChanged,&dialog,[&](int r,int c){if(fillingForm||c!=2)return;auto e=entries[r].toObject();auto t=e["translations"].toObject();auto text=table->item(r,c)->text();if(text.trimmed().isEmpty())t.remove(language->currentData().toString());else t[language->currentData().toString()]=text;e["translations"]=t;entries[r]=e;fillingForm=true;updateStatus(r);if(activeRow==r)edit->setPlainText(text);fillingForm=false;updatePreview();filter();});
 connect(edit,&QPlainTextEdit::textChanged,&dialog,[&]{if(!fillingForm&&activeRow>=0)table->item(activeRow,2)->setText(edit->toPlainText());});
 connect(font,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{updatePreview();});
 connect(acceptSource,&QPushButton::clicked,&dialog,[&]{if(activeRow<0)return;auto e=entries[activeRow].toObject();auto key=e["key"].toString();if(!sources.contains(key))return;e["source"]=sources[key];entries[activeRow]=e;fillingForm=true;updateStatus(activeRow);fillingForm=false;filter();});
 connect(search,&QLineEdit::textChanged,&dialog,[&]{filter();});connect(missing,&QCheckBox::toggled,&dialog,[&]{filter();});
 auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);fillLocales();
 if(dialog.exec()!=QDialog::Accepted)return;storeLocale();config["defaultLocale"]=defaultLocale->currentData().toString();config["fallbackLocale"]=fallback->currentData().toString();config["locales"]=locales;config["entries"]=entries;project["localization"]=config;replaceProject(project);
}
