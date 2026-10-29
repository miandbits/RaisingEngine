#include "studio.hpp"
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QCheckBox>
#include <QPointer>
#include <QUuid>

void StudioWindow::showGettingStarted() {
  if (!firstGameState.isEmpty()) { showFirstGameChecklist(); return; }
  if (busy()) return;
  QDialog dialog(this); dialog.setObjectName("gettingStartedDialog");
  dialog.setWindowTitle(tr("처음 만드는 게임 · 제작 순서")); dialog.resize(760,560);
  auto layout=new QVBoxLayout(&dialog);
  auto intro=new QLabel(tr("작은 일정 육성게임은 아래 순서로 만들 수 있습니다. 각 버튼은 실제 편집 도구로 이동합니다.\n프로젝트 저장은 제작 내용을 저장하며, 플레이 중 저장은 진행 상황을 별도로 저장합니다."));
  intro->setWordWrap(true); layout->addWidget(intro);
  auto grid=new QGridLayout;layout->addLayout(grid);
  QString command;
  const QList<QStringList> steps{
    {"storyFlowAction",tr("인물에서 후속 사건까지 한 번에"),tr("이름·대사·숫자로 활동과 만남, 선택의 관계 변화와 후속 사건을 연결합니다.")},
    {"settings",tr("1. 제목·기간·초깃값"),tr("이야기 소개, 달력, 주인공과 시작 자원을 설정합니다.")},
    {"initializeStarterScreens",tr("기본 플레이 화면 생성"),tr("화면 없는 프로젝트에 타이틀·일정·대화·엔딩과 공용 저장 부품을 추가합니다. 기존 화면은 덮어쓰지 않습니다.")},
    {"addRelationshipDisplay",tr("관계 수치·변화 표시"),tr("인물과 관계를 골라 화면에 배치합니다. 위치·글꼴·표시 형식은 속성 창에서 바꿀 수 있습니다.")},
    {"attributes",tr("2. 능력·자원"),tr("내공·수명·학력처럼 게임에 필요한 수치를 이름과 범위로 정의합니다.")},
    {"character",tr("3. 인물"),tr("인물 이름과 표정, 관계를 설정합니다. 내부 ID는 연결을 위해 유지됩니다.")},
    {"activity",tr("4. 활동·비용·효과"),tr("활동의 비용과 증감을 편집합니다. 복잡한 조건은 수치 규칙 도구를 사용하세요.")},
    {"event",tr("5. 사건"),tr("발생 시점과 조건을 설정합니다. 대사·선택지는 대본 작업실에서 연결합니다.")},
    {"ending",tr("6. 엔딩"),tr("엔딩 조건과 우선순위를 정합니다. 기본 엔딩은 조건 불충족 시의 결과입니다.")},
    {"scriptAuthoringAction",tr("대본·선택지 작업실"),tr("선택 문구, 다음 대사, 조건과 효과를 그래프로 연결합니다.")},
    {"showAuthoringWorkspace",tr("참조·영향 보기"),tr("제작 작업실에서 데이터가 쓰이는 곳과 변경 영향을 확인합니다.")},
    {"editComponents",tr("화면·재사용 부품"),tr("부품 원본 수정은 연결된 인스턴스에 반영됩니다. 개별 덮어쓰기는 별도입니다.")},
    {"validateProject",tr("7. 검증·오류 수정"),tr("검증 결과의 오류 위치를 더블클릭해 수정합니다. 오류가 없어도 도달성 시험은 필요합니다.")},
    {"playProject",tr("8. 실제 플레이"),tr("저장한 제작 내용으로 플레이합니다. 중간 저장과 불러오기도 확인하세요.")}
  };
  for(int i=0;i<steps.size();++i){const auto step=steps[i];auto button=new QPushButton(step[1]);button->setObjectName("gettingStarted-"+step[0]);button->setEnabled(!projectData().empty());auto hint=new QLabel(step[2]);hint->setWordWrap(true);grid->addWidget(button,i,0);grid->addWidget(hint,i,1);connect(button,&QPushButton::clicked,&dialog,[&dialog,&command,step]{command=step[0];dialog.accept();});}
  auto close=new QDialogButtonBox(QDialogButtonBox::Close);layout->addWidget(close);connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  if(dialog.exec()!=QDialog::Accepted)return;
  if(command=="settings"){editProjectSettings();return;}
  const QMap<QString,QString> names{{"attributes",tr("능력·자원")},{"character",tr("인물")},{"activity",tr("활동")},{"event",tr("사건")},{"ending",tr("엔딩")}};
  if(names.contains(command)){
    bottom->setCurrentIndex(4);findChild<QLineEdit*>("contentSearch")->setText(names.value(command));
    for(int i=0;i<contentList->count();++i)if(!contentList->item(i)->isHidden()){contentList->setCurrentRow(i);if(command=="attributes")editContent();break;}
    contentList->setFocus();return;
  }
  if(auto action=findChild<QAction*>(command))action->trigger();
}

void StudioWindow::addRelationshipDisplay() {
  if(busy()||projectData().empty())return;
  const auto project=projectData();const auto revision=view.value("revision"),root=view.value("root");
  const auto visual=project.value("visual").toObject();
  if(visual.value("screens").toArray().empty()){errorLabel->setText(tr("먼저 기본 플레이 화면이나 새 화면을 만드세요."));return;}
  auto dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setObjectName("relationshipDisplayDialog");dialog->setWindowTitle(tr("관계 표시 배치"));dialog->resize(580,300);
  auto layout=new QVBoxLayout(dialog);auto form=new QFormLayout;layout->addLayout(form);
  auto targets=new QComboBox;targets->setObjectName("relationshipDisplayTarget");
  for(const auto &document:view.value("documents").toArray())if(document.toObject().value("kind")=="character"){
    const auto character=document.toObject().value("data").toObject();
    for(const auto &value:character.value("relationships").toArray()){
      const auto relation=value.toObject();targets->addItem(character.value("name").toString()+" · "+relation.value("label").toString(),QVariantMap{{"characterId",character.value("id").toString()},{"relationshipId",relation.value("id").toString()}});
    }
  }
  form->addRow(tr("표시할 관계"),targets);
  auto screens=new QComboBox;screens->setObjectName("relationshipDisplayScreen");
  screens->addItem(tr("기본 플레이 화면 모두 (일정·대화·엔딩)"),"@play");
  for(const auto &value:visual.value("screens").toArray()){const auto s=value.toObject();screens->addItem(s.value("label").toString(),s.value("id").toString());}
  form->addRow(tr("배치할 화면"),screens);
  auto format=new QLineEdit("{label}: {value} (최근 {delta})");format->setObjectName("relationshipDisplayFormat");form->addRow(tr("표시 형식"),format);
  auto help=new QLabel(tr("{label}은 인물·관계 이름, {value}는 현재 값, {delta}는 마지막으로 실제 변한 양입니다.\n일반 텍스트 요소로 추가합니다. 배치 후 캔버스와 속성 창에서 자유롭게 바꾸세요."));help->setWordWrap(true);layout->addWidget(help);
  auto error=new QLabel;error->setWordWrap(true);layout->addWidget(error);
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Ok)->setText(tr("배치"));buttons->button(QDialogButtonBox::Ok)->setEnabled(targets->count()>0);layout->addWidget(buttons);
  if(!targets->count())error->setText(tr("표시할 관계가 없습니다. 인물 편집에서 관계를 먼저 추가하세요."));
  connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
  connect(buttons,&QDialogButtonBox::accepted,dialog,[this,dialog,targets,screens,format,error,project,visual,revision,root]{
    if(busy()||view.value("revision")!=revision||view.value("root")!=root){error->setText(tr("프로젝트가 변경되었습니다. 창을 다시 열어 주세요."));return;}
    auto updated=project,v=visual;auto list=v.value("screens").toArray();const auto pair=targets->currentData().toMap();const auto destination=screens->currentData().toString();
    const auto id="relationship-"+QUuid::createUuid().toString(QUuid::WithoutBraces).left(12);QString first;int added=0;
    for(int i=0;i<list.size();++i){auto s=list[i].toObject();const auto sid=s.value("id").toString();if(destination=="@play"?!QStringList{"schedule","story","ending"}.contains(sid):sid!=destination)continue;
      auto elements=s.value("elements").toArray();int count=0;for(const auto &e:elements)if(e.toObject().value("binding")=="relationship-value")++count;
      elements.append(QJsonObject{{"id",id},{"kind","text"},{"label",targets->currentText()},{"binding","relationship-value"},{"characterId",pair.value("characterId").toString()},{"relationshipId",pair.value("relationshipId").toString()},{"valueFormat",format->text()},{"x",24},{"y",qMax(0,v.value("height").toInt(720)-90-count*48)},{"width",qMax(120,qMin(820,v.value("width").toInt(1280)-48))},{"height",44},{"anchorY","bottom"},{"fontSize",20},{"color","#e9e3d5"},{"fill","#263d43"},{"radius",8}});
      s["elements"]=elements;list[i]=s;if(first.isEmpty())first=sid;++added;
    }
    if(!added){error->setText(tr("기본 플레이 화면이 없습니다. 배치할 화면을 직접 선택하세요."));return;}
    v["screens"]=list;updated["visual"]=v;openScreen(first);selection=id;replaceProject(updated);dialog->accept();
  });
  dialog->open();
}

void StudioWindow::initializeStarterScreens() {
  if (busy() || projectData().empty()) return;
  auto project=projectData();
  if(project.contains("visual")){errorLabel->setText(tr("이미 화면 구성이 있습니다. 기존 화면을 열거나 ＋ 새 화면을 사용하세요."));return;}
  auto element=[](QString id,QString kind,int x,int y,int width,int height){return QJsonObject{{"id",id},{"kind",kind},{"x",x},{"y",y},{"width",width},{"height",height},{"fontSize",22},{"color","#e9e3d5"}};};
  auto text=[&](QString id,QString binding,int x,int y,int width,int height,int font=24){auto value=element(id,"text",x,y,width,height);value["binding"]=binding;value["fontSize"]=font;return value;};
  auto button=[&](QString id,QString caption,QString action,int x,int y,int width){auto value=element(id,"button",x,y,width,48);value["text"]=caption;value["action"]=action;value["fill"]="#375a63";value["radius"]=8;return value;};
  auto background=[&]{auto value=element("background","panel",0,0,1280,720);value["fill"]="#18252b";return value;};
  auto toolbar=[&]{auto value=element("toolbar","component",960,24,288,48);value["componentId"]="save-controls";return value;};
  auto makeScreen=[&](QString id,QString label,QJsonArray entries){return QJsonObject{{"id",id},{"label",label},{"elements",entries}};};
  auto schedule=element("schedule","schedule",330,120,910,240);schedule["visibleWhen"]="planning";
  auto result=element("result","result",330,380,910,220);result["visibleWhen"]="result";
  auto choices=element("choices","choices",64,450,1152,140);choices["visibleWhen"]="choice";
  const QJsonArray screens{
    makeScreen("title",tr("타이틀"),{background(),text("title","title",80,150,1120,100,44),text("description","description",80,270,1120,210),button("start",tr("이야기 시작"),"start",80,570,300),button("load",tr("불러오기"),"load",420,570,220)}),
    makeScreen("schedule",tr("일정·활동 결과"),{background(),text("title","title",24,24,780,64,30),text("calendar","calendar",24,104,280,64),element("stats","stats",24,190,280,450),toolbar(),schedule,result,button("advance",tr("다음 진행"),"advance",890,640,350)}),
    makeScreen("story",tr("대화·선택지"),{background(),toolbar(),text("speaker","speaker",64,130,900,56),element("dialogue","dialogue",64,210,1152,220),choices,button("advance",tr("계속"),"advance",1010,640,220)}),
    makeScreen("ending",tr("엔딩"),{background(),toolbar(),text("title","title",80,150,1120,80,32),text("ending","ending",80,290,1120,160,40),button("start",tr("다시 시작"),"start",450,560,380)})
  };
  const QJsonArray components{QJsonObject{{"id","save-controls"},{"label",tr("공용 저장·불러오기")},{"width",288},{"height",48},{"elements",QJsonArray{button("save",tr("저장"),"save",0,0,128),button("load",tr("불러오기"),"load",144,0,144)}}}};
  project["visual"]=QJsonObject{{"width",1280},{"height",720},{"screens",screens},{"components",components}};
  request("edit",{{"edit",QJsonObject{{"type","replace"},{"file",projectFile},{"data",project}}}},[this](QJsonObject reply){if(reply.value("error").toString().isEmpty())openScreen("title");});
}
