#include "raising-screen-flow.hpp"
#include "studio.hpp"
#include "editor-language.hpp"
#include "combo-chooser.hpp"
#include <QtWidgets>
#include <memory>

namespace {
QString uniqueId(QString base,QSet<QString>& used){auto id=base;for(int n=2;used.contains(id);++n)id=base+"-"+QString::number(n);used.insert(id);return id;}
QJsonObject box(QString id,QString kind,int x,int y,int w,int h){return {{"id",id},{"kind",kind},{"x",x},{"y",y},{"width",w},{"height",h},{"color","#1f2937"},{"fill","#ffffff"},{"fontSize",22},{"surface",QJsonObject{{"mode","none"}}}};}
}
RaisingScreenFlowPlan planRaisingScreenFlow(const QJsonObject& project,const RaisingScreenFlowOptions& o){
  RaisingScreenFlowPlan p;const auto visual=project.value("visual").toObject(),routing=visual.value("routing").toObject();
  if(project.contains("roster")){p.error=QObject::tr("다인 육성 프로젝트는 이 화면 흐름을 지원하지 않습니다. 기존 화면 연결을 유지합니다.");return p;}
  if(project.contains("progression")||project.value("schedulePlayback").toString()=="continuous"||project.value("periodFlow").toObject().contains("intermission")){p.error=QObject::tr("이 도구는 수동 일정 진행을 지원합니다. 행동력·연속 진행·활동 사이 자유 행동이 있는 프로젝트는 기존 연결을 유지합니다.");return p;}
  if(!routing.value("conditions").toArray().isEmpty()||routing.value("useLocationScreens").toBool()){p.error=QObject::tr("조건별·장소별 화면 연결이 있습니다. 화면 전환 연결에서 검토한 뒤 다시 여세요. 연결을 자동 삭제하지 않습니다.");return p;}
  const int width=visual.value("width").toInt(),height=visual.value("height").toInt();
  if(width<960||height<640){p.error=QObject::tr("960 × 640 이상의 디자인 캔버스가 필요합니다. 작은 기기는 읽기 배치로 표시됩니다.");return p;}
  if(o.attributeIds.isEmpty()){p.error=QObject::tr("표시할 능력·자원을 하나 이상 선택하세요.");return p;}
  if(o.attributeIds.size()>100){p.error=QObject::tr("능력·자원은 최대 100개까지 선택할 수 있습니다.");return p;}
  auto text=[&](const char* ko,const char* en,const char* ja){return QString::fromUtf8(o.language=="ko"?ko:o.language=="ja"?ja:en);};
  QSet<QString> used,ordinary;auto oldScreens=visual.value("screens").toArray();
  for(auto value:oldScreens){auto s=value.toObject();used.insert(s["id"].toString());if(!s["modal"].toBool())ordinary.insert(s["id"].toString());}
  for(const auto* key:{"styles","components"})for(auto value:visual.value(key).toArray())used.insert(value.toObject()["id"].toString());
  QMap<QString,QString> routes;for(auto value:routing.value("phases").toArray()){auto row=value.toObject();routes[row["phase"].toString()]=row["screenId"].toString();}
  const QString oldDefault=routing.value("defaultScreenId").toString("schedule");
  for(auto pair:QList<QPair<QString,QString>>{{"title","title"},{"dialogue","story"},{"choice","story"},{"finished","ending"}}){
    const auto target=routes.value(pair.first,routing.isEmpty()?pair.second:oldDefault);
    if(!ordinary.contains(target)){p.error=QObject::tr("시작·이야기·엔딩에 연결된 일반 화면이 필요합니다. 기존 화면 전환 연결을 먼저 설정하세요.");return p;}routes[pair.first]=target;
  }
  if(oldScreens.size()>96){p.error=QObject::tr("화면 수가 많습니다. 기존 화면을 검토한 뒤 네 개의 새 화면을 추가하세요.");return p;}
  p.homeId=uniqueId("raising-home",used);p.statusId=uniqueId("raising-status",used);p.plannerId=uniqueId("raising-planner",used);p.growthId=uniqueId("raising-growth",used);
  const auto controlsId=uniqueId("raising-controls",used),saveId=uniqueId("raising-save-controls",used);
  auto v=visual;auto styles=v["styles"].toArray();auto components=v["components"].toArray();
  styles.append(QJsonObject{{"id",controlsId},{"label",text("화면 흐름 · 버튼","Screen flow · controls","画面フロー・ボタン")},{"fill","#ffffff"},{"color","#1f2937"},{"borderColor","#94a3b8"},{"hoverFill","#e2e8f0"},{"pressedFill","#cbd5e1"},{"disabledColor","#64748b"},{"borderWidth",1},{"radius",8},{"fontSize",22},{"surface",QJsonObject{{"mode","none"}}}});
  auto button=[&](QString id,QString label,QString action,int x,int y,int w=260){auto e=box(id,"button",x,y,w,56);e["text"]=label;e["action"]=action;e["styleId"]=controlsId;e.remove("fill");return e;};
  auto label=[&](QString id,QString value,int x,int y,int w,int h=60,int font=24){auto e=box(id,"text",x,y,w,h);e["text"]=value;e["fontSize"]=font;return e;};
  auto image=[&](QString id,QString asset,int x,int y,int w,int h){auto e=box(id,"image",x,y,w,h);e["imageAssetId"]=asset;e["fit"]="contain";e["fill"]="#f1f5f9";return e;};
  components.append(QJsonObject{{"id",saveId},{"label",text("화면 흐름 · 저장과 불러오기","Screen flow · save and load","画面フロー・セーブとロード")},{"width",288},{"height",56},{"elements",QJsonArray{button("save",text("저장","Save","セーブ"),"save",0,0,136),button("load",text("불러오기","Load","ロード"),"load",152,0,136)}}});
  auto common=[&](QString heading){QJsonArray e;auto bg=box("background","panel",0,0,width,height);bg["fill"]="#f8fafc";e.append(bg);e.append(label("heading",heading,32,24,width-384,58,32));auto save=box("records","component",width-320,26,288,56);save["componentId"]=saveId;e.append(save);return e;};
  auto screen=[&](QString id,QString name,QJsonArray elements,QJsonArray items,QJsonArray actions,QString primary){
    QJsonObject reading{{"items",items},{"actions",actions},{"actionLayout","grouped"}};if(!primary.isEmpty())reading["primaryAction"]=primary;
    QJsonObject s{{"id",id},{"label",name},{"elements",elements},{"readableLayout",reading}};p.screens.append(s);
  };
  auto item=[](QString id){return QJsonObject{{"elementId",id}};};
  auto home=common(text("거처","Home","ホーム"));const int sidebar=340,left=width-sidebar-64;
  if(!o.backgroundId.isEmpty())home.append(image("home-art",o.backgroundId,32,110,left,height-220));
  if(!o.portraitId.isEmpty()){auto portrait=image("portrait",o.portraitId,64,height-470,qMin(300,left-64),360);portrait["fill"]="#00000000";home.append(portrait);}
  auto calendar=label("calendar","",width-sidebar,116,sidebar-32,75);calendar["binding"]="calendar";home.append(calendar);
  auto next=button("continue",text("다음 활동 시작","Start next activity","次の活動を始める"),"advance",width-sidebar,210,sidebar-32);
  next["visibleCondition"]=QJsonObject{{"op","compare"},{"comparator","gt"},{"left",QJsonObject{{"op","time"},{"field","periodInMonth"}}},{"right",QJsonObject{{"op","literal"},{"value",0}}}};home.append(next);
  auto plan=button("plan",text("일정 계획","Plan activities","予定を立てる"),"openPlanner",width-sidebar,290,sidebar-32);plan["screenId"]=p.plannerId;plan["plannerElementId"]="board";home.append(plan);
  auto status=button("status",text("능력과 자원","Status","能力と資源"),"navigate",width-sidebar,370,sidebar-32);status["screenId"]=p.statusId;home.append(status);
  QJsonArray homeItems{item("heading"),item("continue")};if(!o.backgroundId.isEmpty())homeItems.append(QJsonObject{{"elementId","home-art"},{"imageHeight",220}});if(!o.portraitId.isEmpty())homeItems.append(QJsonObject{{"elementId","portrait"},{"imageHeight",180}});homeItems.append(item("calendar"));
  screen(p.homeId,text("거처","Home","ホーム"),home,homeItems,QJsonArray{"plan","status","records__save","records__load"},"plan");
  auto stats=common(text("능력과 자원","Status","能力と資源"));auto values=box("stats","stats",64,120,width-128,height-240);values["attributeIds"]=o.attributeIds;values["statsLayout"]="bars";stats.append(values);auto back=button("home",text("거처로","Home","ホームへ"),"navigate",width-292,height-84);back["screenId"]=p.homeId;stats.append(back);
  screen(p.statusId,text("능력과 자원","Status","能力と資源"),stats,QJsonArray{item("heading"),item("stats")},QJsonArray{"home","records__save","records__load"},"home");
  auto planner=common(text("일정 계획","Activity plan","活動の予定"));auto board=box("board","schedule",32,110,width-64,height-218);board["scheduleLayout"]="activity-board";board["showActivityDescription"]=true;board["showActivityRules"]=true;board["showPhaseForecast"]=true;board["controlStyleId"]=controlsId;board["activityBoard"]=QJsonObject{{"density","compact"},{"showSummary",true},{"advanceAfterApply",true},{"forecast","inline"},{"summaryAttributeIds",o.attributeIds}};board["visibleWhen"]="planning";planner.append(board);planner.append(back);auto begin=button("advance",text("일정 확정 · 활동 시작","Confirm plan · start activity","予定を確定・活動開始"),"commitPlan",32,height-84,400);planner.append(begin);
  screen(p.plannerId,text("일정 계획","Activity plan","活動の予定"),planner,QJsonArray{item("heading"),QJsonObject{{"elementId","board"},{"schedulePresentation","compact"},{"planningGuidance","first-use"},{"planReuse","last-committed"}}},QJsonArray{"advance","home","records__save","records__load"},"advance");
  auto growth=common(text("이번 활동의 성장","This activity's growth","今回の活動の成長"));auto result=box("growth","result",64,120,width-128,height-248);result["resultLayout"]="growth";result["visibleWhen"]="result";growth.append(result);growth.append(button("advance",text("결과 확인 · 계속","Review result · continue","結果を確認・続ける"),"advance",width-432,height-84,400));
  screen(p.growthId,text("활동 성장","Activity growth","活動の成長"),growth,QJsonArray{item("heading"),item("growth")},QJsonArray{"advance","records__save","records__load"},"advance");
  // Preserve all existing screen and content objects. Only reviewed phase routes
  // and the presentation-completion option change in this single project edit.
  for(auto s:p.screens)oldScreens.append(s);v["screens"]=oldScreens;v["styles"]=styles;v["components"]=components;
  if(!routes.contains("period-end")&&ordinary.contains(oldDefault))routes["period-end"]=oldDefault;
  routes["planning"]=p.homeId;routes["ready"]=p.homeId;routes["activity-result"]=p.growthId;
  QJsonArray phases;for(auto it=routes.begin();it!=routes.end();++it)phases.append(QJsonObject{{"phase",it.key()},{"screenId",it.value()}});
  auto nextRouting=routing;nextRouting["defaultScreenId"]=p.homeId;nextRouting["phases"]=phases;v["routing"]=nextRouting;v["activityTimelineCompletion"]="reveal-result";
  auto reading=v["readableLayout"].toObject();if(reading.isEmpty())reading=QJsonObject{{"background","#172033"},{"fontSize",18},{"minTargetSize",44},{"padding",16},{"gap",12},{"maxWidth",760}};if(o.neutralReadingFrame){reading["background"]="#172033";reading["controlStyleId"]=controlsId;}v["readableLayout"]=reading;
  p.project=project;p.project["visual"]=v;return p;
}

void StudioWindow::createRaisingScreenFlow(){
  if(busy()||projectData().isEmpty())return;
  if(auto old=findChild<QDialog*>("raisingScreenFlowDialog")){old->raise();old->activateWindow();return;}
  const auto original=projectData();const QJsonValue revision=view["revision"],root=view["root"];
  auto dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setObjectName("raisingScreenFlowDialog");dialog->setWindowTitle(tr("육성 게임 화면 흐름 만들기"));dialog->resize(900,640);
  auto shell=new QVBoxLayout(dialog);auto intro=new QLabel(tr("거처 → 능력·일정 → 활동 연출 → 성장 화면을 만듭니다. 기존 화면·규칙·이야기는 보존하며, 미리 본 화면 연결만 한 번에 적용합니다."));intro->setWordWrap(true);shell->addWidget(intro);
  auto tabs=new QTabWidget;tabs->setObjectName("raisingFlowTabs");shell->addWidget(tabs,1);auto area=new QScrollArea;area->setObjectName("raisingFlowScroll");area->setWidgetResizable(true);auto body=new QWidget;auto form=new QFormLayout(body);area->setWidget(body);tabs->addTab(area,tr("구성"));
  auto language=new QComboBox;language->setObjectName("raisingFlowLanguage");language->addItem("한국어","ko");language->addItem("English","en");language->addItem("日本語","ja");language->setCurrentIndex(qMax(0,language->findData(editorLanguage())));
  // Explicit shortcuts work even when macOS limits Tab navigation to text fields.
  // The label's native key hint remains visible; no system preference is changed.
  auto shortcutField=[&](QString title,QComboBox* field,int key,bool chooser=false){
    const QKeySequence sequence(Qt::ALT|key);auto label=new QLabel(title+" ("+sequence.toString(QKeySequence::NativeText)+")");label->setBuddy(field);form->addRow(label,chooser?raising::comboChooser(field,title):field);
    auto shortcut=new QShortcut(sequence,dialog);shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcut,&QShortcut::activated,dialog,[=]{tabs->setCurrentWidget(area);QTimer::singleShot(0,dialog,[=]{field->setFocus(Qt::ShortcutFocusReason);area->ensureWidgetVisible(field,0,8);});});
  };
  shortcutField(tr("새 화면의 문구 언어"),language,Qt::Key_L);
  auto images=[&](QString name){auto c=new QComboBox;c->setObjectName(name);c->addItem(tr("이미지 없이 만들기"),"");for(auto value:assets){auto a=value.toObject();if(a["kind"]=="image")c->addItem(a["label"].toString(a["id"].toString()),a["id"].toString());}return c;};
  auto background=images("raisingFlowBackground"),portrait=images("raisingFlowPortrait");shortcutField(tr("거처 배경 이미지"),background,Qt::Key_B,true);shortcutField(tr("인물 이미지"),portrait,Qt::Key_P,true);
  auto assetHelp=new QLabel(tr("이미지는 먼저 에셋에서 가져오세요. 배경과 인물은 일반 이미지 요소로 남아 나중에 바꿀 수 있습니다."));assetHelp->setWordWrap(true);form->addRow(assetHelp);
  auto attributes=new QListWidget;attributes->setObjectName("raisingFlowAttributes");attributes->setMinimumHeight(130);for(auto value:view["documents"].toArray()){auto d=value.toObject();if(d["kind"]!="attributes")continue;for(auto row:d["data"].toObject()["attributes"].toArray()){auto a=row.toObject();auto item=new QListWidgetItem(a["label"].toString(a["id"].toString()),attributes);item->setData(Qt::UserRole,a["id"].toString());item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(attributes->count()<=5?Qt::Checked:Qt::Unchecked);}}form->addRow(tr("능력·자원 표시와 예측 우선순위"),attributes);
  auto theme=new QCheckBox(tr("중립적인 읽기 모양 사용 (모든 읽기 화면의 배경과 공통 버튼)"));theme->setObjectName("raisingFlowNeutralFrame");form->addRow(theme);
  auto calendar=new QLabel(tr("현재 월별 일정 칸: %1. 달력·활동·사건은 바꾸지 않습니다. 일정 칸 수는 프로젝트 설정에서 변경하세요.").arg(original["periodsPerMonth"].toInt()));calendar->setWordWrap(true);form->addRow(calendar);
  auto previewPage=new QWidget;auto previewLayout=new QVBoxLayout(previewPage);auto screenChoice=new QComboBox;screenChoice->setObjectName("raisingFlowPreviewScreen");previewLayout->addWidget(screenChoice);auto preview=new ScreenCanvas;preview->setObjectName("raisingFlowPreviewCanvas");preview->setInteractive(false);preview->setFocusPolicy(Qt::NoFocus);preview->viewport()->setFocusPolicy(Qt::NoFocus);preview->setMinimumHeight(160);previewLayout->addWidget(preview,1);auto changes=new QLabel;changes->setObjectName("raisingFlowChanges");changes->setWordWrap(true);previewLayout->addWidget(changes);auto representative=new QLabel(tr("편집 캔버스의 구성 미리보기입니다. 실제 동작·글자 확대·휴대폰 배치는 적용 후 플레이와 렌더러 검사로 확인하세요."));representative->setWordWrap(true);previewLayout->addWidget(representative);tabs->addTab(previewPage,tr("추가 화면과 연결 검토"));
  auto error=new QLabel;error->setObjectName("raisingFlowError");error->setWordWrap(true);shell->addWidget(error);auto review=new QCheckBox(tr("추가 화면과 변경할 연결을 검토했습니다."));review->setObjectName("raisingFlowReviewed");review->setEnabled(false);shell->addWidget(review);
  auto footer=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel);footer->setObjectName("raisingFlowButtons");auto create=footer->button(QDialogButtonBox::Apply);create->setText(tr("화면 추가와 연결 적용"));create->setEnabled(false);auto show=footer->addButton(tr("변경 미리보기"),QDialogButtonBox::ActionRole);show->setObjectName("raisingFlowPreview");shell->addWidget(footer);
  // Native sheets do not reliably choose a first field. Focus an editable
  // control after showing each page; the read-only canvas is not a tab stop.
  auto focusPage=[=]{
    auto first=tabs->currentIndex()==0?language:screenChoice;
    first->setFocus(Qt::TabFocusReason);
  };
  connect(tabs,&QTabWidget::currentChanged,dialog,[=](int){QTimer::singleShot(0,dialog,focusPage);});
  QWidget::setTabOrder(language,background);QWidget::setTabOrder(background,portrait);
  QWidget::setTabOrder(portrait,attributes);QWidget::setTabOrder(attributes,theme);
  QWidget::setTabOrder(theme,show);QWidget::setTabOrder(screenChoice,review);
  auto plan=std::make_shared<RaisingScreenFlowPlan>();
  auto invalidate=[=]{review->setChecked(false);review->setEnabled(false);create->setEnabled(false);plan->project={};changes->setText(tr("설정이 바뀌었습니다. 변경 미리보기를 다시 확인하세요."));};
  for(auto combo:{language,background,portrait})connect(combo,qOverload<int>(&QComboBox::currentIndexChanged),dialog,invalidate);connect(theme,&QCheckBox::toggled,dialog,invalidate);connect(attributes,&QListWidget::itemChanged,dialog,invalidate);
  connect(footer,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
  connect(review,&QCheckBox::toggled,dialog,[=](bool checked){create->setEnabled(checked&&!plan->project.isEmpty());});
  connect(screenChoice,qOverload<int>(&QComboBox::currentIndexChanged),dialog,[=](int index){if(index>=0&&index<plan->screens.size())preview->setScreen(plan->project,plan->screens[index].toObject(),assets,{},true);});
  connect(show,&QPushButton::clicked,dialog,[=,this]{
    if(busy()||view["root"]!=root||view["revision"]!=revision){error->setText(tr("프로젝트가 변경되었습니다. 취소한 뒤 다시 열어 주세요."));return;}
    RaisingScreenFlowOptions options;options.backgroundId=background->currentData().toString();options.portraitId=portrait->currentData().toString();options.language=language->currentData().toString();options.neutralReadingFrame=theme->isChecked();for(int i=0;i<attributes->count();++i)if(attributes->item(i)->checkState()==Qt::Checked)options.attributeIds.append(attributes->item(i)->data(Qt::UserRole).toString());
    *plan=planRaisingScreenFlow(original,options);error->setText(plan->error);if(!plan->error.isEmpty()){review->setEnabled(false);create->setEnabled(false);return;}
    screenChoice->clear();for(auto s:plan->screens)screenChoice->addItem(s.toObject()["label"].toString()+" · "+s.toObject()["id"].toString());
    changes->setText(tr("새 화면 4개와 저장·불러오기 부품 1개를 추가합니다. 기존 화면을 덮어쓰지 않습니다.\n기본·일정 계획·활동 준비 → %1\n활동 결과 → %2\n시작·이야기·선택지·엔딩 연결은 유지합니다. 연출 뒤에는 실제 활동 결과를 확인합니다.%3").arg(plan->homeId,plan->growthId,theme->isChecked()?tr("\n공통 읽기 배경을 중립적인 진한 남색으로, 공통 버튼을 밝은 색으로 변경합니다."):QString()));
    review->setChecked(false);review->setEnabled(true);tabs->setCurrentWidget(previewPage);
  });
  connect(create,&QPushButton::clicked,dialog,[=,this]{
    if(!review->isChecked()||plan->project.isEmpty())return;if(busy()||view["revision"]!=revision||view["root"]!=root){error->setText(tr("프로젝트가 변경되었습니다. 취소한 뒤 다시 열어 주세요."));create->setEnabled(false);return;}
    create->setEnabled(false);QPointer<QDialog> guard(dialog);const auto homeId=plan->homeId;
    request("edit",{{"edit",QJsonObject{{"type","replace"},{"file",projectFile},{"data",plan->project}}}},[this,guard,error,create,homeId](QJsonObject reply){if(!guard)return;if(!reply["error"].toString().isEmpty()){error->setText(reply["error"].toString());create->setEnabled(true);return;}guard->accept();showWorkspace({});openScreen(homeId);});
  });dialog->open();QTimer::singleShot(0,dialog,[=]{dialog->activateWindow();focusPage();});
}
