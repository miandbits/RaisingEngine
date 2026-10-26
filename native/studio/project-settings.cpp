#include "release-settings.hpp"
#include "calendar.hpp"
#include "resume-context-settings.hpp"
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
  base->addItem(tr("My first raising game · recommended"),"my-first-raising-game");
  base->addItem(tr("최소 일정 육성 · 2개월"),"academy-demo");base->addItem(tr("일정 육성 · 12개월"),"academy-year");
  base->addItem(tr("비주얼 UI 샘플"),"academy-visual");base->addItem(tr("도시·상점·관계 샘플"),"academy-journey");
  base->addItem(tr("탐험·전투 샘플"),"academy-adventure");base->addItem(tr("여러 인물 함께 육성 · 개인 능력과 공용 자금"),"genre-suite/roster");base->addItem(tr("기존 프로젝트의 콘텐츠 재사용"),"existing");base->setCurrentIndex(0);form->addRow(tr("시작 구성"),base);
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

void StudioWindow::editProjectSettings(bool focusReadingHistory) {
  if(bridge.busy()||projectData().empty())return;
  auto project=projectData();QDialog dialog(this);dialog.setObjectName("projectSettingsDialog");dialog.setWindowTitle(tr("프로젝트 설정"));dialog.resize(640,650);
  auto layout=new QVBoxLayout(&dialog);auto tabs=new QTabWidget;tabs->setElideMode(Qt::ElideNone);tabs->setUsesScrollButtons(true);layout->addWidget(tabs);auto basic=new QWidget;auto form=new QFormLayout(basic);tabs->addTab(basic,tr("기본 · 달력"));
  QJsonObject releaseMetadata;for(auto document:view.value("documents").toArray())if(document.toObject().value("kind")=="assets")releaseMetadata=document.toObject().value("data").toObject().value("release").toObject();
  auto releaseSettings=new StudioRelease::Settings(releaseMetadata,view.value("root").toString());tabs->addTab(releaseSettings,tr("출시"));
  connect(releaseSettings->backup,&QPushButton::clicked,&dialog,[&]{StudioRelease::restoreDialog(&dialog,[this](QString method,QJsonObject params,std::function<void(QJsonObject)> done){request(method,params,done);});});
  auto title=new QLineEdit(project.value("title").toString());title->setObjectName("projectTitle");form->addRow(tr("게임 제목"),title);
  auto id=new QLineEdit(project.value("gameId").toString());id->setObjectName("projectGameId");id->setMaxLength(64);form->addRow(tr("게임 ID"),id);
  auto description=new QPlainTextEdit(project.value("description").toString());description->setObjectName("projectDescription");description->setMaximumHeight(90);form->addRow(tr("설명"),description);
  auto year=new QSpinBox;year->setObjectName("projectStartYear");year->setRange(1,9999);year->setValue(project.value("startYear").toInt(1));form->addRow(tr("시작 연도"),year);
  auto month=new QSpinBox;month->setObjectName("projectStartMonth");month->setRange(1,qMax(12,StudioCalendar::spec(project).months));month->setValue(project.value("startMonth").toInt(1));form->addRow(tr("시작 월"),month);
  auto duration=new QSpinBox;duration->setObjectName("projectDurationMonths");duration->setRange(1,1200);duration->setSuffix(tr("개월"));duration->setValue(project.value("durationMonths").toInt(12));form->addRow(tr("전체 육성 기간"),duration);
  auto periods=new QSpinBox;periods->setObjectName("projectPeriodsPerMonth");periods->setRange(1,12);periods->setValue(project.value("periodsPerMonth").toInt(3));form->addRow(tr("월별 일정 칸 수"),periods);
  auto playback=new QComboBox;playback->setObjectName("projectSchedulePlayback");playback->addItem(tr("일정마다 확인"),"manual");playback->addItem(tr("월 일정 연속 진행"),"continuous");playback->setCurrentIndex(playback->findData(project.value("schedulePlayback").toString("manual")));form->addRow(tr("일정 진행 방식"),playback);
  auto calendarScroll=new QScrollArea;calendarScroll->setWidgetResizable(true);auto calendarPage=new QWidget;auto calendarForm=new QFormLayout(calendarPage);calendarScroll->setWidget(calendarPage);tabs->addTab(calendarScroll,tr("Custom calendar"));
  const auto originalCalendar=project.value("calendar").toObject();
  auto customCalendar=new QCheckBox(tr("Use a fixed-length calendar"));customCalendar->setObjectName("projectCustomCalendar");customCalendar->setChecked(project.contains("calendar"));calendarForm->addRow(customCalendar);
  auto units=new QSpinBox;units->setObjectName("projectCalendarUnits");units->setRange(1,60);units->setValue(originalCalendar.value("monthsPerYear").toInt(12));calendarForm->addRow(tr("Units per year"),units);
  auto days=new QSpinBox;days->setObjectName("projectCalendarDays");days->setRange(1,366);days->setValue(originalCalendar.value("daysPerMonth").toInt(30));calendarForm->addRow(tr("Days per unit"),days);
  QMap<QString,QLineEdit*> calendarLabels;for(const auto &pair:QList<QPair<QString,QString>>{{"yearLabel","Year"},{"monthLabel","Month"},{"periodLabel","Period"},{"dayLabel","Day"}}){auto field=new QLineEdit(originalCalendar.value(pair.first).toString(pair.second));field->setObjectName("projectCalendar-"+pair.first);field->setMaxLength(80);calendarLabels[pair.first]=field;calendarForm->addRow(pair.second,field);}
  QList<QLineEdit*> weekdays;const auto originalWeekdays=originalCalendar.value("weekdayLabels").toArray();const QStringList weekdayDefaults{"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  for(int i=0;i<7;++i){auto field=new QLineEdit(originalWeekdays.size()==7?originalWeekdays[i].toString():weekdayDefaults[i]);field->setObjectName("projectCalendarWeekday"+QString::number(i));field->setMaxLength(80);weekdays.append(field);calendarForm->addRow(tr("Weekday %1").arg(i+1),field);}
  auto startWeekday=new QComboBox;startWeekday->setObjectName("projectCalendarStartWeekday");startWeekday->addItems(weekdayDefaults);startWeekday->setCurrentIndex(originalCalendar.value("startWeekday").toInt());calendarForm->addRow(tr("Weekday on the first day"),startWeekday);
  auto calendarHelp=new QLabel(tr("A unit can be a month, phase or season. Activity slots divide each unit; the final slot includes any remaining days. Rule-calendar days, when enabled, must match. Existing event indices and saves are not rewritten."));calendarHelp->setWordWrap(true);calendarForm->addRow(calendarHelp);
  auto readCalendar=[=]{QJsonObject c{{"monthsPerYear",units->value()},{"daysPerMonth",days->value()},{"startWeekday",startWeekday->currentIndex()}};for(auto it=calendarLabels.begin();it!=calendarLabels.end();++it)c[it.key()]=it.value()->text().trimmed();QJsonArray w;for(auto field:weekdays)w.append(field->text().trimmed());c["weekdayLabels"]=w;return c;};
  auto readingPage=new QWidget;readingPage->setObjectName("projectReadingPage");auto readingLayout=new QVBoxLayout(readingPage);auto readingScroll=new QScrollArea;readingScroll->setWidgetResizable(true);auto readingBody=new QWidget;auto readingForm=new QFormLayout(readingBody);readingScroll->setWidget(readingBody);readingLayout->addWidget(readingScroll);tabs->addTab(readingPage,tr("읽기"));
  const auto originalVisual=project.value("visual").toObject();
  auto readingHistory=new QCheckBox(tr("최근 읽은 내용 사용"));readingHistory->setObjectName("projectReadingHistory");readingHistory->setChecked(originalVisual.contains("readingHistory"));readingHistory->setEnabled(project.value("visual").isObject());readingForm->addRow(readingHistory);
  auto readingHelp=new QLabel(tr("플레이어가 이번 실행에서 화면에 표시한 대사와 실제로 고른 응답을 다시 읽을 수 있습니다. 저장 파일에 포함되지 않으며 게임을 새로 시작하거나 저장을 불러오면 지워집니다. 미리보기는 기록하지 않습니다.\n플레이어의 읽기 메뉴에서 열거나 버튼의 행동을 ‘최근 읽은 내용 열기’로 설정하세요."));readingHelp->setObjectName("projectReadingHistoryHelp");readingHelp->setWordWrap(true);readingForm->addRow(readingHelp);
  if(!project.value("visual").isObject()){auto unavailable=new QLabel(tr("최근 읽은 내용을 사용하려면 비주얼 화면이 필요합니다."));unavailable->setWordWrap(true);readingForm->addRow(unavailable);}
  auto resumeConfig=originalVisual.value("resumeContext").toObject();
  auto reminders=new QGroupBox(tr("현재 알림"));reminders->setObjectName("projectResumeContextGroup");auto reminderLayout=new QVBoxLayout(reminders);readingForm->addRow(reminders);
  auto resumeEnabled=new QCheckBox(tr("현재 알림 사용"));resumeEnabled->setObjectName("projectResumeContext");resumeEnabled->setChecked(originalVisual.contains("resumeContext"));resumeEnabled->setEnabled(project.value("visual").isObject());reminderLayout->addWidget(resumeEnabled);
  auto resumeHelp=new QLabel(tr("기존 화면에서 현재 상태 텍스트를 선택합니다. 플레이어는 읽기 메뉴나 설정에서 열 수 있습니다. 읽기 기록이나 자동 줄거리 요약이 아닙니다."));resumeHelp->setWordWrap(true);reminderLayout->addWidget(resumeHelp);
  auto resumeSummary=new QLabel;resumeSummary->setObjectName("projectResumeContextSummary");resumeSummary->setWordWrap(true);reminderLayout->addWidget(resumeSummary);
  auto configureResume=new QPushButton(tr("알림 텍스트 선택…"));configureResume->setObjectName("projectConfigureResumeContext");configureResume->setEnabled(project.value("visual").isObject());reminderLayout->addWidget(configureResume);
  auto refreshResume=[&]{const auto problem=StudioResumeContext::validate(originalVisual,resumeConfig);resumeSummary->setText(problem.isEmpty()?tr("%1 · 텍스트 %2개%3").arg(resumeConfig.value("screenId").toString()).arg(resumeConfig.value("elementIds").toArray().size()).arg(resumeConfig.value("showAfterLoad").toBool()?tr(" · 불러온 뒤 표시"):QString()):problem);};refreshResume();
  connect(configureResume,&QPushButton::clicked,&dialog,[&]{StudioResumeContext::Dialog chooser(originalVisual,resumeConfig,&dialog);if(chooser.exec()==QDialog::Accepted){resumeConfig=chooser.config;resumeEnabled->setChecked(true);refreshResume();}});
  if(focusReadingHistory){tabs->setCurrentWidget(readingPage);readingHistory->setFocus();}
  const auto hasReadingAction=[](const QJsonValue& value,auto&& self)->bool{if(value.isObject()){const auto object=value.toObject();if((object.value("kind")=="button"&&object.value("action")=="readingHistory")||(object.contains("elementId")&&object.value("values").toObject().value("action")=="readingHistory"))return true;for(auto it=object.begin();it!=object.end();++it)if(self(it.value(),self))return true;}else if(value.isArray())for(auto entry:value.toArray())if(self(entry,self))return true;return false;};
  auto resultPage=new QWidget;auto resultForm=new QFormLayout(resultPage);tabs->addTab(resultPage,tr("활동 결과 연출"));
  const auto resultConfig=project.value("visual").toObject().value("resultPlayback").toObject();
  auto resultEnabled=new QCheckBox(tr("SD 화면에서 능력치 변화를 차례로 표시"));resultEnabled->setObjectName("projectResultPlayback");resultEnabled->setChecked(!resultConfig.isEmpty());resultForm->addRow(resultEnabled);
  auto timing=[&](const char* key,const QString& label,int initial,int minimum){auto field=new QSpinBox;field->setObjectName(QString("projectResult-")+key);field->setRange(minimum,10000);field->setSuffix(" ms");field->setValue(resultConfig.value(key).toInt(initial));field->setEnabled(resultEnabled->isChecked());connect(resultEnabled,&QCheckBox::toggled,field,&QWidget::setEnabled);resultForm->addRow(label,field);return field;};
  auto lead=timing("leadInMs",tr("동작 먼저 보여주기"),800,1),change=timing("changeMs",tr("능력치 한 항목 변화 시간"),800,100),hold=timing("holdMs",tr("마지막 수치 유지"),1000,1);
  auto resultHelp=new QLabel(tr("화면의 결과 위젯에 실제 확정된 수치를 순서대로 표시합니다. 기본 탭에서 월 일정 연속 진행을 선택하면 연출 후 다음 활동으로 이동하며 사건·선택지에서는 멈춥니다. 별도로 연결한 활동 타임라인이 있으면 그 연출이 우선합니다."));resultHelp->setWordWrap(true);resultForm->addRow(resultHelp);
#include "gallery-settings.inc"
#include "period-flow-settings.inc"
  auto calendar=new QLabel;calendar->setObjectName("projectCalendarSummary");calendar->setWordWrap(true);form->addRow(calendar);
  auto updateCalendar=[=]{const bool custom=customCalendar->isChecked();const int limit=custom?units->value():12;month->setMaximum(qMax(limit,month->value()));for(auto field:{units,days})field->setEnabled(custom);startWeekday->setEnabled(custom);for(auto field:calendarLabels)field->setEnabled(custom);for(int i=0;i<weekdays.size();i++){weekdays[i]->setEnabled(custom);startWeekday->setItemText(i,weekdays[i]->text());}duration->setSuffix(custom?" "+calendarLabels.value("monthLabel")->text():tr("개월"));
    if(auto label=qobject_cast<QLabel*>(form->labelForField(month)))label->setText(custom?tr("Starting %1").arg(calendarLabels.value("monthLabel")->text()):tr("시작 월"));
    if(auto label=qobject_cast<QLabel*>(form->labelForField(periods)))label->setText(custom?tr("%1 slots per %2").arg(calendarLabels.value("periodLabel")->text(),calendarLabels.value("monthLabel")->text()):tr("월별 일정 칸 수"));
    auto p=project;p["startYear"]=year->value();p["startMonth"]=month->value();p["periodsPerMonth"]=periods->value();if(custom)p["calendar"]=readCalendar();else p.remove("calendar");
    const auto date=StudioCalendar::label(p,duration->value()*periods->value()-1);
    if(!custom&&month->value()<=12){const int last=month->value()-1+duration->value()-1;calendar->setText(tr("마지막 일정: %1년 %2월 · 총 %3기간\n연속 진행은 결과 연출 후 다음 일정으로 이어집니다. 대화·선택·전투는 입력을 기다립니다.\n일정 칸 수 변경은 사건의 기간 번호를 자동 수정하지 않습니다. 새 게임으로 확인하세요.").arg(year->value()+last/12).arg(last%12+1).arg(duration->value()*periods->value()));return;}
    calendar->setText((month->value()>limit?tr("Starting unit exceeds the selected year. Choose a valid starting unit before applying.\n"):QString())+tr("Last activity: %1 · %2 activity slots\nChanging the calendar does not rewrite event indices. Test with a new game.").arg(date).arg(duration->value()*periods->value()));};
  for(auto spin:{year,month,duration,periods,units,days})connect(spin,qOverload<int>(&QSpinBox::valueChanged),&dialog,updateCalendar);connect(customCalendar,&QCheckBox::toggled,&dialog,updateCalendar);for(auto field:calendarLabels)connect(field,&QLineEdit::textChanged,&dialog,updateCalendar);for(auto field:weekdays)connect(field,&QLineEdit::textChanged,&dialog,updateCalendar);updateCalendar();
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
  connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{if(!resumeEnabled->isChecked()&&StudioResumeContext::hasAction(originalVisual)){error->setText(tr("현재 알림을 끄기 전에 연결된 버튼의 행동을 변경하세요. 화면과 재사용 요소의 버튼을 확인하세요."));tabs->setCurrentWidget(readingPage);return;}if(resumeEnabled->isChecked()){const auto problem=StudioResumeContext::validate(originalVisual,resumeConfig);if(!problem.isEmpty()){error->setText(problem);tabs->setCurrentWidget(readingPage);configureResume->setFocus();return;}}if(!readingHistory->isChecked()&&(hasReadingAction(originalVisual.value("screens"),hasReadingAction)||hasReadingAction(originalVisual.value("components"),hasReadingAction))){error->setText(tr("최근 읽은 내용을 끄기 전에 연결된 버튼의 행동을 변경하세요. 화면과 재사용 요소의 버튼을 확인하세요."));tabs->setCurrentWidget(readingPage);return;}if(month->value()>(customCalendar->isChecked()?units->value():12)){error->setText(tr("Starting unit exceeds the selected year."));return;}if(customCalendar->isChecked()){if(days->value()<periods->value()){error->setText(tr("Days per unit must be at least the number of activity slots."));return;}for(auto field:calendarLabels)if(field->text().trimmed().isEmpty()){error->setText(tr("Enter every calendar label."));return;}for(auto field:weekdays)if(field->text().trimmed().isEmpty()){error->setText(tr("Enter seven weekday names."));return;}const auto rule=project.value("gameplay").toObject().value("ruleCalendar").toObject();if(!rule.isEmpty()&&rule.value("daysPerMonth").toInt()!=days->value()){error->setText(tr("Logical rule days must match the custom calendar days per unit."));return;}}if(galleryEnabled->isChecked()){const auto config=readGallery();if(config.value("title").toString().isEmpty()){error->setText(tr("도감 제목을 입력하세요."));return;}QSet<QString> ids;for(auto value:config.value("entries").toArray()){const auto e=value.toObject();const auto id=e.value("id").toString();if(!QRegularExpression("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$").match(id).hasMatch()||ids.contains(id)||e.value("sourceId").toString().isEmpty()||e.value("title").toString().isEmpty()){error->setText(tr("도감 항목의 고유 ID·대본·제목을 확인하세요."));return;}ids.insert(id);}}if(title->text().trimmed().isEmpty()){error->setText(tr("게임 제목을 입력하세요."));return;}if(!QRegularExpression("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$").match(id->text()).hasMatch()){error->setText(tr("게임 ID는 소문자로 시작하고 영문 소문자·숫자·하이픈을 사용합니다."));return;}dialog.accept();});
  if(dialog.exec()!=QDialog::Accepted)return;
  if(customCalendar->isChecked())project["calendar"]=readCalendar();else project.remove("calendar");
  if(galleryEnabled->isChecked())project["gallery"]=readGallery();else project.remove("gallery");
  const auto nextPeriodFlow=readPeriodFlow();if(nextPeriodFlow.isEmpty())project.remove("periodFlow");else project["periodFlow"]=nextPeriodFlow;
  project["title"]=title->text().trimmed();project["gameId"]=id->text();project["description"]=description->toPlainText();project["startYear"]=year->value();project["startMonth"]=month->value();project["durationMonths"]=duration->value();project["protagonistId"]=protagonist->currentData().toString();project["defaultEndingId"]=ending->currentData().toString();
  project["periodsPerMonth"]=periods->value();if(playback->currentData()=="manual"&&!project.contains("schedulePlayback"))project.remove("schedulePlayback");else project["schedulePlayback"]=playback->currentData().toString();
  if(resultEnabled->isChecked()||!resultConfig.isEmpty()){auto visual=project.value("visual").toObject();if(resultEnabled->isChecked())visual["resultPlayback"]=QJsonObject{{"leadInMs",lead->value()},{"changeMs",change->value()},{"holdMs",hold->value()}};else visual.remove("resultPlayback");project["visual"]=visual;}
  if(readingHistory->isChecked()!=originalVisual.contains("readingHistory")){auto visual=project.value("visual").toObject();if(readingHistory->isChecked())visual["readingHistory"]=QJsonObject{};else visual.remove("readingHistory");project["visual"]=visual;}
  if(resumeEnabled->isChecked()||originalVisual.contains("resumeContext")){auto visual=project.value("visual").toObject();if(resumeEnabled->isChecked())visual["resumeContext"]=resumeConfig;else visual.remove("resumeContext");project["visual"]=visual;}
  auto nextInitial=initial,nextAttributes=initialAttributes;for(auto it=initialFields.begin();it!=initialFields.end();++it)if(it.value()->value()!=it.value()->property("originalValue").toInt())nextAttributes[it.key()]=it.value()->value();nextInitial["attributes"]=nextAttributes;project["initialState"]=nextInitial;
  request("projectReleaseSettings",{{"project",project},{"release",releaseSettings->values()},{"iconFile",releaseSettings->iconFile},{"noticeFiles",releaseSettings->imports()}});
}
