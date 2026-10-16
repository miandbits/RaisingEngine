#include "studio.hpp"
#include "gallery-ui.hpp"
#include "logic-editor.hpp"
#include <QStandardPaths>
#include <QTableWidget>
#include <QHeaderView>
#include <QDialog>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QVBoxLayout>

void StudioWindow::newProjectWizard() {
  QDialog dialog(this);dialog.setObjectName("newProjectDialog");dialog.setWindowTitle(tr("새 프로젝트"));dialog.resize(680,340);
  auto layout=new QVBoxLayout(&dialog);auto form=new QFormLayout;layout->addLayout(form);
  auto base=new QComboBox;base->setObjectName("newProjectTemplate");
  base->addItem(tr("최소 일정 육성 · 2개월"),"academy-demo");base->addItem(tr("일정 육성 · 12개월"),"academy-year");
  base->addItem(tr("비주얼 UI 샘플"),"academy-visual");base->addItem(tr("도시·상점·관계 샘플"),"academy-journey");
  base->addItem(tr("탐험·전투 샘플"),"academy-adventure");base->addItem(tr("기존 프로젝트의 콘텐츠 재사용"),"existing");base->setCurrentIndex(4);form->addRow(tr("시작 구성"),base);
  auto source=new QLineEdit(view.value("root").toString());source->setObjectName("newProjectSource");
  auto sourceRow=new QWidget;auto sourceLayout=new QHBoxLayout(sourceRow);sourceLayout->setContentsMargins(0,0,0,0);sourceLayout->addWidget(source);auto sourceBrowse=new QPushButton(tr("원본 선택…"));sourceLayout->addWidget(sourceBrowse);form->addRow(tr("원본 폴더"),sourceRow);
  const auto userProjects=QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath("Raising Studio Projects");
  auto folder=new QLineEdit(userProjects+"/new-game");folder->setObjectName("newProjectDestination");
  auto folderRow=new QWidget;auto folderLayout=new QHBoxLayout(folderRow);folderLayout->setContentsMargins(0,0,0,0);folderLayout->addWidget(folder);auto browse=new QPushButton(tr("찾아보기…"));folderLayout->addWidget(browse);form->addRow(tr("새 프로젝트 폴더"),folderRow);
  auto help=new QLabel(tr("새 폴더 경로를 입력하거나 빈 폴더를 선택하세요. 기존 파일은 덮어쓰지 않습니다.\n기존 프로젝트 재사용은 저장된 콘텐츠와 에셋만 복제합니다. 저장 게임·분석·편집 기록은 복제하지 않습니다.\n제목·시작 날짜·육성 기간은 생성 후 ‘파일 → 프로젝트 설정’에서 변경할 수 있습니다."));help->setWordWrap(true);layout->addWidget(help);
  auto error=new QLabel;error->setObjectName("newProjectError");error->setWordWrap(true);layout->addWidget(error);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->setObjectName("newProjectButtons");buttons->button(QDialogButtonBox::Ok)->setText(tr("프로젝트 만들기"));layout->addWidget(buttons);
  auto update=[=]{sourceRow->setEnabled(base->currentData()=="existing");};connect(base,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,update);update();
  connect(sourceBrowse,&QPushButton::clicked,&dialog,[&]{auto selected=QFileDialog::getExistingDirectory(&dialog,tr("재사용할 프로젝트 폴더"),engineRoot+"/projects");if(!selected.isEmpty())source->setText(selected);});
  connect(browse,&QPushButton::clicked,&dialog,[&]{auto selected=QFileDialog::getExistingDirectory(&dialog,tr("새 프로젝트를 만들 빈 폴더"),userProjects);if(!selected.isEmpty())folder->setText(selected);});
  connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{if(folder->text().trimmed().isEmpty()){error->setText(tr("새 프로젝트 폴더를 입력하세요."));return;}if(base->currentData()=="existing"&&source->text().trimmed().isEmpty()){error->setText(tr("재사용할 원본 프로젝트 폴더를 선택하세요."));return;}dialog.accept();});
  if(dialog.exec()!=QDialog::Accepted)return;
  request("create",{{"root",QDir::cleanPath(folder->text().trimmed())},{"template",base->currentData().toString()},{"source",QDir::cleanPath(source->text().trimmed())}});
}

void StudioWindow::editProjectSettings() {
  if(bridge.busy()||projectData().empty())return;
  auto project=projectData();QDialog dialog(this);dialog.setObjectName("projectSettingsDialog");dialog.setWindowTitle(tr("프로젝트 설정"));dialog.resize(640,650);
  auto layout=new QVBoxLayout(&dialog);auto tabs=new QTabWidget;layout->addWidget(tabs);auto basic=new QWidget;auto form=new QFormLayout(basic);tabs->addTab(basic,tr("기본 · 달력"));
  auto title=new QLineEdit(project.value("title").toString());title->setObjectName("projectTitle");form->addRow(tr("게임 제목"),title);
  auto id=new QLineEdit(project.value("gameId").toString());id->setObjectName("projectGameId");id->setMaxLength(64);form->addRow(tr("게임 ID"),id);
  auto description=new QPlainTextEdit(project.value("description").toString());description->setObjectName("projectDescription");description->setMaximumHeight(90);form->addRow(tr("설명"),description);
  auto year=new QSpinBox;year->setObjectName("projectStartYear");year->setRange(1,9999);year->setValue(project.value("startYear").toInt(1));form->addRow(tr("시작 연도"),year);
  auto month=new QSpinBox;month->setObjectName("projectStartMonth");month->setRange(1,12);month->setValue(project.value("startMonth").toInt(1));form->addRow(tr("시작 월"),month);
  auto duration=new QSpinBox;duration->setObjectName("projectDurationMonths");duration->setRange(1,1200);duration->setSuffix(tr("개월"));duration->setValue(project.value("durationMonths").toInt(12));form->addRow(tr("전체 육성 기간"),duration);
  auto periods=new QSpinBox;periods->setObjectName("projectPeriodsPerMonth");periods->setRange(1,12);periods->setValue(project.value("periodsPerMonth").toInt(3));form->addRow(tr("월별 일정 칸 수"),periods);
  auto playback=new QComboBox;playback->setObjectName("projectSchedulePlayback");playback->addItem(tr("일정마다 확인"),"manual");playback->addItem(tr("월 일정 연속 진행"),"continuous");playback->setCurrentIndex(playback->findData(project.value("schedulePlayback").toString("manual")));form->addRow(tr("일정 진행 방식"),playback);
  auto resultPage=new QWidget;auto resultForm=new QFormLayout(resultPage);tabs->addTab(resultPage,tr("활동 결과 연출"));
  const auto resultConfig=project.value("visual").toObject().value("resultPlayback").toObject();
  auto resultEnabled=new QCheckBox(tr("SD 화면에서 능력치 변화를 차례로 표시"));resultEnabled->setObjectName("projectResultPlayback");resultEnabled->setChecked(!resultConfig.isEmpty());resultForm->addRow(resultEnabled);
  auto timing=[&](const char* key,const QString& label,int initial,int minimum){auto field=new QSpinBox;field->setObjectName(QString("projectResult-")+key);field->setRange(minimum,10000);field->setSuffix(" ms");field->setValue(resultConfig.value(key).toInt(initial));field->setEnabled(resultEnabled->isChecked());connect(resultEnabled,&QCheckBox::toggled,field,&QWidget::setEnabled);resultForm->addRow(label,field);return field;};
  auto lead=timing("leadInMs",tr("동작 먼저 보여주기"),800,1),change=timing("changeMs",tr("능력치 한 항목 변화 시간"),800,100),hold=timing("holdMs",tr("마지막 수치 유지"),1000,1);
  auto resultHelp=new QLabel(tr("화면의 결과 위젯에 실제 확정된 수치를 순서대로 표시합니다. 기본 탭에서 월 일정 연속 진행을 선택하면 연출 후 다음 활동으로 이동하며 사건·선택지에서는 멈춥니다. 별도로 연결한 활동 타임라인이 있으면 그 연출이 우선합니다."));resultHelp->setWordWrap(true);resultForm->addRow(resultHelp);
#include "gallery-settings.inc"
  auto calendar=new QLabel;calendar->setObjectName("projectCalendarSummary");calendar->setWordWrap(true);form->addRow(calendar);
  auto updateCalendar=[=]{const int last=month->value()-1+duration->value()-1;calendar->setText(tr("마지막 일정: %1년 %2월 · 총 %3기간\n연속 진행은 결과 연출 후 다음 일정으로 이어집니다. 대화·선택·전투는 입력을 기다립니다.\n일정 칸 수 변경은 사건의 기간 번호를 자동 수정하지 않습니다. 새 게임으로 확인하세요.").arg(year->value()+last/12).arg(last%12+1).arg(duration->value()*periods->value()));};
  for(auto spin:{year,month,duration,periods})connect(spin,qOverload<int>(&QSpinBox::valueChanged),&dialog,updateCalendar);updateCalendar();
  auto protagonist=new QComboBox;protagonist->setObjectName("projectProtagonist");auto ending=new QComboBox;ending->setObjectName("projectDefaultEnding");
  QJsonArray attributeDefinitions;
  for(auto value:view.value("documents").toArray()){auto doc=value.toObject(),data=doc.value("data").toObject();auto kind=doc.value("kind").toString();if(kind=="character")protagonist->addItem(data.value("name").toString(data.value("id").toString()),data.value("id").toString());if(kind=="ending")ending->addItem(data.value("label").toString(data.value("id").toString()),data.value("id").toString());if(kind=="attributes")attributeDefinitions=data.value("attributes").toArray();}
  auto select=[&](QComboBox *combo,const QJsonValue &value){int index=combo->findData(value.toString());if(index<0){combo->addItem(value.toString(),value.toString());index=combo->count()-1;}combo->setCurrentIndex(index);};
  select(protagonist,project.value("protagonistId"));select(ending,project.value("defaultEndingId"));form->addRow(tr("주인공"),protagonist);form->addRow(tr("기본 엔딩"),ending);
  auto statePage=new QScrollArea;statePage->setWidgetResizable(true);auto stateBody=new QWidget;auto stateForm=new QFormLayout(stateBody);statePage->setWidget(stateBody);tabs->addTab(statePage,tr("시작 능력·자원"));
  QMap<QString,QSpinBox*> initialFields;const auto initial=project.value("initialState").toObject(),initialAttributes=initial.value("attributes").toObject();
  for(auto value:attributeDefinitions){auto attribute=value.toObject();auto key=attribute.value("id").toString();auto field=new QSpinBox;field->setObjectName("projectInitial-"+key);field->setRange(-2147483647,2147483647);field->setValue(initialAttributes.value(key).toInt(attribute.value("initial").toInt()));field->setProperty("originalValue",field->value());field->setToolTip(tr("설정 범위 %1–%2 · 범위 오류는 공통 검증기에 표시됩니다.").arg(attribute.value("min").toInt()).arg(attribute.value("max").toInt()));initialFields[key]=field;stateForm->addRow(attribute.value("label").toString(key),field);}
  auto help=new QLabel(tr("적용은 하나의 되돌리기 작업입니다. 저장 전 공통 검증 결과를 확인하세요.\n달력·초기값 변경은 새 게임으로 확인하며 기존 저장을 자동 변환하지 않습니다."));help->setWordWrap(true);layout->addWidget(help);
  auto error=new QLabel;error->setObjectName("projectSettingsError");error->setWordWrap(true);layout->addWidget(error);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->setObjectName("projectSettingsButtons");buttons->button(QDialogButtonBox::Ok)->setText(tr("설정 적용"));layout->addWidget(buttons);
  connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{if(galleryEnabled->isChecked()){const auto config=readGallery();if(config.value("title").toString().isEmpty()){error->setText(tr("도감 제목을 입력하세요."));return;}QSet<QString> ids;for(auto value:config.value("entries").toArray()){const auto e=value.toObject();const auto id=e.value("id").toString();if(!QRegularExpression("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$").match(id).hasMatch()||ids.contains(id)||e.value("sourceId").toString().isEmpty()||e.value("title").toString().isEmpty()){error->setText(tr("도감 항목의 고유 ID·대본·제목을 확인하세요."));return;}ids.insert(id);}}if(title->text().trimmed().isEmpty()){error->setText(tr("게임 제목을 입력하세요."));return;}if(!QRegularExpression("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$").match(id->text()).hasMatch()){error->setText(tr("게임 ID는 소문자로 시작하고 영문 소문자·숫자·하이픈을 사용합니다."));return;}dialog.accept();});
  if(dialog.exec()!=QDialog::Accepted)return;
  if(galleryEnabled->isChecked())project["gallery"]=readGallery();else project.remove("gallery");
  project["title"]=title->text().trimmed();project["gameId"]=id->text();project["description"]=description->toPlainText();project["startYear"]=year->value();project["startMonth"]=month->value();project["durationMonths"]=duration->value();project["protagonistId"]=protagonist->currentData().toString();project["defaultEndingId"]=ending->currentData().toString();
  project["periodsPerMonth"]=periods->value();if(playback->currentData()=="manual"&&!project.contains("schedulePlayback"))project.remove("schedulePlayback");else project["schedulePlayback"]=playback->currentData().toString();
  if(resultEnabled->isChecked()||!resultConfig.isEmpty()){auto visual=project.value("visual").toObject();if(resultEnabled->isChecked())visual["resultPlayback"]=QJsonObject{{"leadInMs",lead->value()},{"changeMs",change->value()},{"holdMs",hold->value()}};else visual.remove("resultPlayback");project["visual"]=visual;}
  auto nextInitial=initial,nextAttributes=initialAttributes;for(auto it=initialFields.begin();it!=initialFields.end();++it)if(it.value()->value()!=it.value()->property("originalValue").toInt())nextAttributes[it.key()]=it.value()->value();nextInitial["attributes"]=nextAttributes;project["initialState"]=nextInitial;
  replaceProject(project);
}
