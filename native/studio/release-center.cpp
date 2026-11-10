#include "release-center.hpp"
#include "studio.hpp"
#include "save-migration.hpp"
#include <QCheckBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace StudioReleaseCenter {
namespace {
QString tr(const char* text){return QObject::tr(text);}
QString known(const QJsonValue& value){return value.isString()&&!value.toString().isEmpty()?value.toString():tr("확인되지 않음");}
QLabel* label(const QString& text,const QString& name={}){auto l=new QLabel(text);l->setObjectName(name);l->setWordWrap(true);l->setTextFormat(Qt::PlainText);l->setTextInteractionFlags(Qt::TextSelectableByMouse);return l;}
QString stateLabel(QString state){
  if(state=="passed")return tr("검증 통과");if(state=="failed")return tr("실패");if(state=="running")return tr("검사 중");
  if(state=="stale")return tr("기준 변경 · 재검사 필요");if(state=="cancelled")return tr("취소됨");if(state=="migration-required")return tr("저장 이전 검토 필요");
  if(state=="completed")return tr("검사 완료 · 항목별 결과 확인");if(state=="not-applicable")return tr("해당 없음 · 제작자 기록");return tr("미확인");
}
QString distributionLabel(const QJsonObject& row,const QString& target){
  const auto id=row.value("id").toString();
  if(id=="playthrough")return tr("실제 불러오기→진행→저장→재실행");
  if(id=="device")return target=="ios"?tr("실제 iPhone·iPad 검사"):tr("지원 대상 기기 검사");
  if(id=="accessibility")return tr("접근성·보조 기술 확인");
  if(id=="rights")return tr("라이선스·고지·에셋 권리 확인");
  if(id=="branding")return tr("앱 이름·아이콘·브랜딩 확인");
  if(id=="distribution")return target=="web"?tr("호스팅·전달 설정 확인"):target=="ios"?tr("서명·프로비저닝·스토어 배포 확인"):tr("서명·설치 프로그램·전달 확인");
  return row.value("label").toString(id);
}
QString engineText(const QJsonObject& identity){
  const auto source=identity.value("source").toObject();
  return tr("엔진 %1 · 서비스 소스 %2").arg(known(identity.value("engineVersion")),known(source.value("commit")))+
    (source.value("dirty").isBool()&&source.value("dirty").toBool()?tr(" · 로컬 변경 있음"):QString());
}
struct PathRow{QWidget* box;QLineEdit* edit;};
PathRow pathRow(QWidget* owner,const QString& id,const QString& title,bool directory){
  auto box=new QWidget;auto row=new QHBoxLayout(box);row->setContentsMargins(0,0,0,0);auto edit=new QLineEdit;edit->setObjectName(id);row->addWidget(edit,1);auto choose=new QPushButton(tr("찾기…"));choose->setObjectName(id+"Browse");row->addWidget(choose);
  QObject::connect(choose,&QPushButton::clicked,owner,[=]{auto value=directory?QFileDialog::getExistingDirectory(owner,title,edit->text()):QFileDialog::getOpenFileName(owner,title,edit->text(),tr("저장 파일 (*.json);;모든 파일 (*)"));if(!value.isEmpty())edit->setText(value);});return{box,edit};
}
class Center final:public QDialog {
  Request rpc;QString engineRoot,projectRoot;std::function<void()> exporter;
  QJsonArray entries,jobs;QJsonObject current,job;bool waiting=false,polling=false;
  QComboBox *releases,*caseKind,*distributionStatus;QLineEdit *directory,*selectedDirectory,*releaseName,*caseName,*caseFile,*caseSource,*restoreDestination;
  QTreeWidget *cases,*distribution;QLabel *identity,*scope,*status,*recoveryStatus,*currentEngine,*migrationHelp;QPlainTextEdit *details,*distributionNote;
  QPushButton *refreshButton,*registerButton,*exportButton,*openRelease,*addCase,*checkSelected,*checkAll,*migrate,*checkRelease,*verifyRecovery,*restore,*openRestored,*applyDistribution,*cancel;
  QProgressBar* progress;QTimer poll;
  QString selectedCase()const{auto item=cases->currentItem();return item?item->data(0,Qt::UserRole).toString():QString();}
  QJsonObject caseValue()const{auto id=selectedCase();for(auto value:current.value("cases").toArray())if(value.toObject().value("id").toString()==id)return value.toObject();return{};}
  QString releaseId()const{return current.value("id").toString();}
  QJsonObject params()const{return{{"root",projectRoot},{"releaseId",releaseId()},{"entryRevision",current.value("entryRevision")}};}
  bool running()const{return job.value("status").toString()=="running";}
  bool migrationSourceMatches()const{const auto id=current.value("identity").toObject();return id.value("sourceContentFingerprint").toString().isEmpty()||id.value("sourceContentFingerprint")==id.value("contentFingerprint");}
  void enable(){
    const bool ready=!waiting&&!running(),selected=!releaseId().isEmpty();
    for(auto b:{registerButton,exportButton,refreshButton})b->setEnabled(ready);releases->setEnabled(!waiting);
    openRelease->setEnabled(!current.value("directory").toString().isEmpty());
    for(auto b:{addCase,checkAll,checkRelease,verifyRecovery,restore,applyDistribution})b->setEnabled(ready&&selected);
    checkSelected->setEnabled(ready&&selected&&!selectedCase().isEmpty());
    const auto c=caseValue();const auto checks=current.value("checks").toObject();const auto recovery=checks.value("recovery").toObject();const bool integrity=checks.value("release").toObject().value("status")=="passed";
    migrate->setEnabled(ready&&integrity&&migrationSourceMatches()&&!c.value("sourceProject").toString().isEmpty()&&!recovery.value("directory").toString().isEmpty()&&recovery.value("status")=="passed"&&recovery.value("restored").toBool());
    openRestored->setEnabled(integrity&&!recovery.value("directory").toString().isEmpty()&&recovery.value("restored").toBool()&&recovery.value("status")=="passed");
    migrationHelp->setText(migrationSourceMatches()?tr("저장 이전 검토는 이전 프로젝트와 선택한 릴리스의 복원된 백업을 사용합니다. 먼저 백업·복원 탭에서 새 폴더로 복원하세요. 변환 결과는 새 파일로 저장한 뒤 별도 예시로 등록해 검사하세요.") : tr("이 릴리스는 화면 변경 뒤에도 이전 저장 지문을 유지합니다. 복원 원본의 지문이 달라 저장 이전 검토를 열 수 없습니다. 등록한 저장의 직접 호환성 검사는 가능합니다."));
    cancel->setEnabled(!waiting&&running());progress->setRange(0,running()?0:1);progress->setValue(running()?0:1);
  }
  void send(QString method,QJsonObject values,std::function<void(QJsonObject)> success){
    if(waiting)return;waiting=true;values["root"]=projectRoot;enable();auto guard=QPointer<Center>(this);
    rpc(method,values,[guard,success](QJsonObject reply){if(!guard)return;guard->waiting=false;guard->enable();const auto error=reply.value("error").toString();if(!error.isEmpty()){guard->status->setText(error);return;}success(reply);});
  }
  void selectJob(){
    QJsonObject active;for(auto value:jobs){auto candidate=value.toObject();if(candidate.value("releaseId").toString()==releaseId()&&candidate.value("status")=="running")active=candidate;}
    if(!active.isEmpty())job=active;else if(job.value("releaseId").toString()!=releaseId()||running())job={};
    if(running()){showJob();poll.start();}else {poll.stop();if(job.isEmpty()){status->clear();status->setProperty("jobStatus",QString());}}enable();
  }
  void showEntry(){
    const auto selected=releases->currentData().toString();current={};for(auto value:entries)if(value.toObject().value("id").toString()==selected)current=value.toObject();
    const auto id=current.value("identity").toObject(),checks=current.value("checks").toObject();
    auto releaseEngine=id.value("engineIdentity").toObject();if(!releaseEngine.contains("engineVersion"))releaseEngine["engineVersion"]=id.value("engineVersion");
    identity->setText(current.isEmpty()?tr("먼저 버전을 지정하여 내보낸 릴리스 폴더를 등록하세요."):tr("선택한 릴리스: %1 · %2 · %3\n콘텐츠 지문: %4\n이 릴리스의 엔진: %5").arg(current.value("name").toString(),known(id.value("version")),known(id.value("target")),known(id.value("contentFingerprint")),engineText(releaseEngine)));
    selectedDirectory->setText(current.value("directory").toString());selectedDirectory->setToolTip(selectedDirectory->text());
    scope->setText(tr("이 검사는 선택한 내보내기와 등록한 저장 예시에만 적용됩니다. 수동 배포 확인은 자동 검사와 별도로 기록합니다."));
    const auto chosen=selectedCase();cases->clear();for(auto value:current.value("cases").toArray()){const auto c=value.toObject(),check=c.value("check").toObject();auto row=new QTreeWidgetItem(cases,{c.value("name").toString(),c.value("kind")=="migrated"?tr("이전한 저장"):tr("원본 저장"),stateLabel(check.value("status").toString()),check.value("message").toString()});row->setData(0,Qt::UserRole,c.value("id").toString());row->setToolTip(0,c.value("file").toString());if(c.value("id").toString()==chosen)cases->setCurrentItem(row);}
    distribution->clear();for(auto value:current.value("distribution").toArray()){const auto c=value.toObject();auto row=new QTreeWidgetItem(distribution,{distributionLabel(c,id.value("target").toString()),c.value("status")=="passed"?tr("통과 · 제작자 확인"):stateLabel(c.value("status").toString()),c.value("note").toString()});row->setData(0,Qt::UserRole,c);}
    const auto recovery=checks.value("recovery").toObject();recoveryStatus->setText(tr("릴리스 파일: %1\n백업·복원: %2%3").arg(stateLabel(checks.value("release").toObject().value("status").toString()),recovery.value("status")=="passed"?(recovery.value("restored").toBool()?tr("새 폴더 복원·검증 완료"):tr("백업 검증 완료 · 복원은 미실행")):stateLabel(recovery.value("status").toString()),recovery.value("directory").toString().isEmpty()?QString():"\n"+recovery.value("directory").toString()));
    details->setPlainText(QString::fromUtf8(QJsonDocument(current).toJson(QJsonDocument::Indented)));selectJob();
  }
  void updateEntry(QJsonObject entry){auto id=entry.value("id").toString();bool replaced=false;for(int i=0;i<entries.size();++i)if(entries[i].toObject().value("id").toString()==id){entries[i]=entry;replaced=true;break;}if(!replaced)entries.append(entry);showList(id);}
  void showList(QString selected={}){if(selected.isEmpty())selected=releases->currentData().toString();QSignalBlocker block(releases);releases->clear();for(auto value:entries){auto e=value.toObject(),id=e.value("identity").toObject();releases->addItem(e.value("name").toString()+" · "+known(id.value("version"))+" · "+known(id.value("target")),e.value("id").toString());}int index=releases->findData(selected);if(index>=0)releases->setCurrentIndex(index);showEntry();}
  void reload(){send("releaseWorkflowList",{},[this](QJsonObject reply){auto data=reply.value("releaseWorkflow").toObject();entries=data.value("releases").toArray();jobs=data.value("jobs").toArray();showList();enable();});}
  void showJob(){const auto id=job.value("id").toString();if(!id.isEmpty()){bool found=false;for(int i=0;i<jobs.size();++i)if(jobs[i].toObject().value("id").toString()==id){jobs[i]=job;found=true;break;}if(!found)jobs.append(job);}const auto message=job.value("message").toString();status->setText(stateLabel(job.value("status").toString())+(message.isEmpty()?QString():"\n"+message));status->setProperty("jobStatus",job.value("status").toString());enable();}
  void start(QString kind,QJsonObject extra={}){auto p=params();p["kind"]=kind;for(auto i=extra.begin();i!=extra.end();++i)p[i.key()]=i.value();send("releaseWorkflowStart",p,[this](QJsonObject reply){job=reply.value("releaseJob").toObject();showJob();if(running())poll.start();else reload();});}
public:
  Center(QWidget* parent,QString engine,QString root,Request request,std::function<void()> exportGame):QDialog(parent),rpc(std::move(request)),engineRoot(std::move(engine)),projectRoot(std::move(root)),exporter(std::move(exportGame)){
    setObjectName("releaseCenter");setWindowTitle(tr("릴리스 센터"));setAttribute(Qt::WA_DeleteOnClose);resize(1020,790);setMinimumSize(760,620);
    auto layout=new QVBoxLayout(this);currentEngine=label(tr("현재 에디터 정보를 확인하는 중…"),"releaseCurrentEditorIdentity");layout->addWidget(currentEngine);
    auto top=new QHBoxLayout;releases=new QComboBox;releases->setObjectName("releaseSelection");releases->setMinimumContentsLength(25);releases->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);top->addWidget(releases,1);refreshButton=new QPushButton(tr("새로 고침"));refreshButton->setObjectName("releaseRefresh");top->addWidget(refreshButton);exportButton=new QPushButton(tr("새 릴리스 내보내기…"));exportButton->setObjectName("releaseExport");top->addWidget(exportButton);layout->addLayout(top);
    auto registration=new QGroupBox(tr("기존 내보내기를 릴리스 기준으로 등록"));auto form=new QFormLayout(registration);auto path=pathRow(this,"releaseDirectory",tr("RELEASE.json이 있는 릴리스 폴더"),true);directory=path.edit;form->addRow(tr("릴리스 폴더"),path.box);auto nameRow=new QHBoxLayout;releaseName=new QLineEdit;releaseName->setObjectName("releaseName");releaseName->setMaxLength(120);nameRow->addWidget(releaseName,1);registerButton=new QPushButton(tr("릴리스 등록"));registerButton->setObjectName("releaseRegister");nameRow->addWidget(registerButton);form->addRow(tr("구분할 이름"),nameRow);layout->addWidget(registration);
    identity=label({},"releaseSelectedIdentity");layout->addWidget(identity);auto selectedPath=new QHBoxLayout;selectedDirectory=new QLineEdit;selectedDirectory->setObjectName("releaseSelectedDirectory");selectedDirectory->setReadOnly(true);selectedDirectory->setAccessibleName(tr("선택한 릴리스 폴더"));selectedPath->addWidget(selectedDirectory,1);openRelease=new QPushButton(tr("릴리스 폴더 열기"));openRelease->setObjectName("releaseOpenFolder");selectedPath->addWidget(openRelease);layout->addLayout(selectedPath);connect(openRelease,&QPushButton::clicked,this,[this]{const auto path=current.value("directory").toString();if(!path.isEmpty())QDesktopServices::openUrl(QUrl::fromLocalFile(path));});scope=label({},"releaseCheckScope");layout->addWidget(scope);
    auto tabs=new QTabWidget;tabs->setObjectName("releaseTabs");layout->addWidget(tabs,1);
    auto saves=new QWidget;auto savesLayout=new QVBoxLayout(saves);savesLayout->addWidget(label(tr("이름으로 저장 예시를 관리합니다. 원본 파일을 유지하고, 이전한 저장은 별도 예시로 등록하세요. 검사는 저장 당시 진행을 재현하며 모든 플레이 경로의 호환성을 보장하지 않습니다.")));
    cases=new QTreeWidget;cases->setObjectName("releaseSaveCases");cases->setHeaderLabels({tr("이름"),tr("종류"),tr("검사 결과"),tr("검사 내용")});cases->setRootIsDecorated(false);cases->setWordWrap(true);cases->setColumnWidth(0,170);cases->setColumnWidth(1,95);cases->setColumnWidth(2,170);savesLayout->addWidget(cases,1);
    auto addForm=new QFormLayout;caseName=new QLineEdit;caseName->setObjectName("releaseCaseName");caseName->setMaxLength(120);caseKind=new QComboBox;caseKind->setObjectName("releaseCaseKind");caseKind->addItem(tr("원본 저장"),"original");caseKind->addItem(tr("이전한 저장 · 제작자 지정"),"migrated");auto caseNameRow=new QHBoxLayout;caseNameRow->addWidget(caseName,1);caseNameRow->addWidget(caseKind);addForm->addRow(tr("예시 이름"),caseNameRow);auto file=pathRow(this,"releaseCaseFile",tr("등록할 저장 파일"),false);caseFile=file.edit;addForm->addRow(tr("저장 파일"),file.box);auto source=pathRow(this,"releaseCaseSource",tr("저장 당시의 이전 프로젝트 (선택)"),true);caseSource=source.edit;addForm->addRow(tr("이전 프로젝트 (선택)"),source.box);savesLayout->addLayout(addForm);
    auto saveActions=new QHBoxLayout;addCase=new QPushButton(tr("저장 예시 등록"));addCase->setObjectName("releaseAddCase");checkSelected=new QPushButton(tr("선택한 저장 검사"));checkSelected->setObjectName("releaseCheckCase");checkAll=new QPushButton(tr("전체 저장 검사"));checkAll->setObjectName("releaseCheckAllCases");migrate=new QPushButton(tr("저장 이전 검토…"));migrate->setObjectName("releaseMigrateCase");for(auto b:{addCase,checkSelected,checkAll,migrate})saveActions->addWidget(b);savesLayout->addLayout(saveActions);migrationHelp=label(tr("저장 이전 검토는 이전 프로젝트와 선택한 릴리스의 복원된 백업을 사용합니다. 먼저 백업·복원 탭에서 새 폴더로 복원하세요. 변환 결과는 새 파일로 저장한 뒤 별도 예시로 등록해 검사하세요."),"releaseMigrationHelp");savesLayout->addWidget(migrationHelp);tabs->addTab(saves,tr("저장 호환성"));
    auto recovery=new QWidget;auto recoveryLayout=new QVBoxLayout(recovery);recoveryLayout->addWidget(label(tr("등록한 릴리스의 파일과 프로젝트 백업을 검사합니다. 복원은 새 폴더에만 만들며, 기존 프로젝트와 배포 파일을 덮어쓰지 않습니다.")));recoveryStatus=label({},"releaseRecoveryStatus");recoveryLayout->addWidget(recoveryStatus);auto recoveryActions=new QHBoxLayout;checkRelease=new QPushButton(tr("릴리스 파일 검사"));checkRelease->setObjectName("releaseCheckIntegrity");verifyRecovery=new QPushButton(tr("백업 검증"));verifyRecovery->setObjectName("releaseVerifyBackup");recoveryActions->addWidget(checkRelease);recoveryActions->addWidget(verifyRecovery);recoveryLayout->addLayout(recoveryActions);auto restoreForm=new QFormLayout;auto destinationBox=new QWidget;auto destinationRow=new QHBoxLayout(destinationBox);destinationRow->setContentsMargins(0,0,0,0);restoreDestination=new QLineEdit;restoreDestination->setObjectName("releaseRestoreDestination");destinationRow->addWidget(restoreDestination,1);auto chooseDestination=new QPushButton(tr("새 폴더 지정…"));destinationRow->addWidget(chooseDestination);restoreForm->addRow(tr("새 복원 폴더"),destinationBox);recoveryLayout->addLayout(restoreForm);restore=new QPushButton(tr("새 폴더에 복원·검증"));restore->setObjectName("releaseRestoreBackup");recoveryLayout->addWidget(restore);openRestored=new QPushButton(tr("복원 폴더 열기"));openRestored->setObjectName("releaseOpenRestored");recoveryLayout->addWidget(openRestored);connect(openRestored,&QPushButton::clicked,this,[this]{const auto recovery=current.value("checks").toObject().value("recovery").toObject();if(recovery.value("restored").toBool())QDesktopServices::openUrl(QUrl::fromLocalFile(recovery.value("directory").toString()));});recoveryLayout->addStretch();tabs->addTab(recovery,tr("백업·복원"));
    auto delivery=new QWidget;auto deliveryLayout=new QVBoxLayout(delivery);deliveryLayout->addWidget(label(tr("아래 항목은 제작자가 확인 근거를 남기는 수동 기록입니다. 통과를 선택해도 엔진이 기기 검사·권리 확인·스토어 승인을 수행한 것은 아닙니다.")));distribution=new QTreeWidget;distribution->setObjectName("releaseDistributionChecks");distribution->setHeaderLabels({tr("확인 항목"),tr("제작자 기록"),tr("근거·남은 작업")});distribution->setRootIsDecorated(false);distribution->setWordWrap(true);distribution->setColumnWidth(0,220);distribution->setColumnWidth(1,180);deliveryLayout->addWidget(distribution,1);distributionStatus=new QComboBox;distributionStatus->setObjectName("releaseDistributionStatus");distributionStatus->addItem(tr("미확인"),"pending");distributionStatus->addItem(tr("통과 · 제작자 확인"),"passed");distributionStatus->addItem(tr("실패 · 수정 필요"),"failed");distributionStatus->addItem(tr("해당 없음 · 제작자 기록"),"not-applicable");deliveryLayout->addWidget(distributionStatus);distributionNote=new QPlainTextEdit;distributionNote->setObjectName("releaseDistributionNote");distributionNote->setPlaceholderText(tr("확인한 환경·증거 위치·담당자 또는 남은 작업을 적으세요."));distributionNote->setMaximumHeight(95);deliveryLayout->addWidget(distributionNote);applyDistribution=new QPushButton(tr("선택한 항목에 기록 저장"));applyDistribution->setObjectName("releaseRecordDistribution");deliveryLayout->addWidget(applyDistribution);tabs->addTab(delivery,tr("배포 확인"));
    auto advanced=new QWidget;auto advancedLayout=new QVBoxLayout(advanced);advancedLayout->addWidget(label(tr("현재 에디터 정보와 릴리스에 기록된 엔진 정보는 다를 수 있습니다. 아래는 선택한 릴리스의 읽기 전용 검사 기록입니다.")));details=new QPlainTextEdit;details->setObjectName("releaseDetails");details->setReadOnly(true);advancedLayout->addWidget(details);tabs->addTab(advanced,tr("상세 기록"));
    status=label(tr("등록된 릴리스를 불러오는 중…"),"releaseWorkflowStatus");layout->addWidget(status);progress=new QProgressBar;progress->setObjectName("releaseJobProgress");progress->setTextVisible(false);progress->setMaximumHeight(7);layout->addWidget(progress);auto bottom=new QHBoxLayout;bottom->addWidget(label(tr("검사 중에도 편집할 수 있습니다. 이 창을 닫아도 시작한 검사는 계속됩니다.")),1);cancel=new QPushButton(tr("진행 중 검사 취소"));cancel->setObjectName("releaseCancelJob");bottom->addWidget(cancel);auto close=new QPushButton(tr("닫기"));close->setObjectName("releaseClose");bottom->addWidget(close);layout->addLayout(bottom);
    connect(close,&QPushButton::clicked,this,&QDialog::close);connect(refreshButton,&QPushButton::clicked,this,[this]{reload();});connect(exportButton,&QPushButton::clicked,this,[this]{exporter();});connect(releases,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{showEntry();});connect(cases,&QTreeWidget::currentItemChanged,this,[this]{enable();});
    connect(registerButton,&QPushButton::clicked,this,[this]{send("releaseWorkflowRegister",{{"directory",directory->text()},{"name",releaseName->text()}},[this](QJsonObject reply){updateEntry(reply.value("releaseEntry").toObject());status->setText(tr("릴리스 기준을 등록했습니다. 아직 실행하지 않은 검사는 미확인으로 남습니다."));});});
    connect(addCase,&QPushButton::clicked,this,[this]{auto p=params();p["name"]=caseName->text();p["file"]=caseFile->text();p["kind"]=caseKind->currentData().toString();if(!caseSource->text().isEmpty())p["sourceProject"]=caseSource->text();send("releaseWorkflowAddCase",p,[this](QJsonObject reply){updateEntry(reply.value("releaseEntry").toObject());status->setText(tr("저장 예시를 등록했습니다. 호환성 검사를 실행하세요."));});});
    connect(checkSelected,&QPushButton::clicked,this,[this]{start("save",{{"caseId",selectedCase()}});});connect(checkAll,&QPushButton::clicked,this,[this]{start("save");});connect(checkRelease,&QPushButton::clicked,this,[this]{start("release");});connect(verifyRecovery,&QPushButton::clicked,this,[this]{start("recovery");});
    connect(chooseDestination,&QPushButton::clicked,this,[this]{auto value=QFileDialog::getSaveFileName(this,tr("새 복원 폴더"),restoreDestination->text(),{},nullptr,QFileDialog::DontConfirmOverwrite);if(!value.isEmpty())restoreDestination->setText(value);});connect(restore,&QPushButton::clicked,this,[this]{if(restoreDestination->text().trimmed().isEmpty()){status->setText(tr("새 복원 폴더를 지정하세요."));return;}start("recovery",{{"destination",restoreDestination->text()}});});
    connect(migrate,&QPushButton::clicked,this,[this]{auto c=caseValue(),recovery=current.value("checks").toObject().value("recovery").toObject();showSaveMigration(engineRoot,recovery.value("directory").toString(),this,c.value("sourceProject").toString(),c.value("file").toString());});
    connect(distribution,&QTreeWidget::currentItemChanged,this,[this]{auto item=distribution->currentItem();if(!item)return;auto c=item->data(0,Qt::UserRole).toJsonObject();distributionStatus->setCurrentIndex(qMax(0,distributionStatus->findData(c.value("status").toString())));distributionNote->setPlainText(c.value("note").toString());});connect(applyDistribution,&QPushButton::clicked,this,[this]{auto item=distribution->currentItem();if(!item){status->setText(tr("기록할 배포 확인 항목을 선택하세요."));return;}auto p=params();p["checkId"]=item->data(0,Qt::UserRole).toJsonObject().value("id");p["status"]=distributionStatus->currentData().toString();p["note"]=distributionNote->toPlainText();send("releaseWorkflowDistribution",p,[this](QJsonObject reply){updateEntry(reply.value("releaseEntry").toObject());status->setText(tr("제작자의 배포 확인 기록을 저장했습니다."));});});
    poll.setInterval(500);connect(&poll,&QTimer::timeout,this,[this]{if(polling||waiting||!running())return;polling=true;auto guard=QPointer<Center>(this);const auto polledRelease=releaseId(),polledJob=job.value("id").toString();rpc("releaseWorkflowStatus",{{"root",projectRoot},{"jobId",job.value("id")}},[guard,polledRelease,polledJob](QJsonObject reply){if(!guard)return;guard->polling=false;if(guard->releaseId()!=polledRelease||guard->job.value("id").toString()!=polledJob)return;if(!reply.value("error").toString().isEmpty()){guard->poll.stop();guard->status->setText(reply.value("error").toString());return;}guard->job=reply.value("releaseJob").toObject();guard->showJob();if(!guard->running()){guard->poll.stop();guard->reload();}});});
    connect(cancel,&QPushButton::clicked,this,[this]{send("releaseWorkflowCancel",{{"jobId",job.value("id")}},[this](QJsonObject reply){job=reply.value("releaseJob").toObject();showJob();if(!running()){poll.stop();reload();}});});
    enable();auto guard=QPointer<Center>(this);QTimer::singleShot(0,this,[this,guard]{rpc("editorIdentity",{},[guard](QJsonObject reply){if(!guard)return;guard->currentEngine->setText(tr("현재 에디터: %1").arg(engineText(reply.value("editorIdentity").toObject())));});reload();});
  }
  void offer(const QJsonObject& output){directory->setText(output.value("directory").toString());status->setText(tr("새 내보내기가 준비되었습니다. 버전·백업을 포함한 릴리스라면 이름을 붙여 등록하세요."));}
};
}
QDialog* create(QWidget* parent,const QString& engineRoot,const QString& projectRoot,Request request,std::function<void()> exportGame){return new Center(parent,engineRoot,projectRoot,std::move(request),std::move(exportGame));}
void offerExport(QDialog* dialog,const QJsonObject& output){if(auto center=dynamic_cast<Center*>(dialog))center->offer(output);}
QDialog* showIdentity(QWidget* parent,Request request){
  auto d=new QDialog(parent);d->setAttribute(Qt::WA_DeleteOnClose);d->setObjectName("editorIdentityDialog");d->setWindowTitle(tr("Raising Studio 정보"));d->resize(700,480);auto layout=new QVBoxLayout(d);layout->addWidget(label(tr("현재 실행 중인 에디터")));auto body=label(tr("현재 에디터 정보를 확인하는 중…"),"editorIdentitySummary");layout->addWidget(body);auto scope=label({},"editorIdentityScope");layout->addWidget(scope);auto details=new QPlainTextEdit;details->setObjectName("editorIdentityDetails");details->setReadOnly(true);layout->addWidget(details,1);auto close=new QDialogButtonBox(QDialogButtonBox::Close);layout->addWidget(close);QObject::connect(close,&QDialogButtonBox::rejected,d,&QDialog::close);QPointer<QDialog> guard=d;
  request("editorIdentity",{},[guard,body,scope,details](QJsonObject reply){if(!guard)return;auto i=reply.value("editorIdentity").toObject();if(!reply.value("error").toString().isEmpty()){body->setText(reply.value("error").toString());return;}auto distribution=i.value("distribution").toObject(),source=i.value("source").toObject(),launch=i.value("launch").toObject();body->setText(engineText(i)+"\n"+tr("배포 버전: %1 · 채널: %2\n실행 진입점: %3\n실행 명령: %4").arg(known(distribution.value("releaseVersion")),known(distribution.value("channel")),known(launch.value("entryPoint")),known(launch.value("command"))));scope->setText(tr("서비스 소스 정보는 실행 파일 자체의 빌드 출처 검증을 뜻하지 않습니다.")+"\n"+i.value("support").toObject().value("summary").toString());details->setPlainText(QString::fromUtf8(QJsonDocument(i).toJson(QJsonDocument::Indented)));body->setProperty("sourceCommit",source.value("commit").toString());});d->show();return d;
}
}

void StudioWindow::showReleaseCenter(){
  if(auto existing=findChild<QDialog*>("releaseCenter")){existing->show();existing->raise();existing->activateWindow();return;}
  const auto root=view.value("root").toString();if(root.isEmpty())return;
  auto rpc=[this,root](QString method,QJsonObject params,StudioReleaseCenter::Done done){auto timer=new QTimer(this);timer->setInterval(50);connect(timer,&QTimer::timeout,this,[this,root,method,params,done,timer]{if(method!="editorIdentity"&&view.value("root").toString()!=root){timer->deleteLater();done({{"error","RELEASE_PROJECT_CHANGED"}});return;}if(bridge.backgroundBusy()||(method!="releaseWorkflowStatus"&&bridge.busy()))return;timer->stop();timer->deleteLater();bridge.requestBackground(method,params,done);});timer->start();};
  auto dialog=StudioReleaseCenter::create(this,engineRoot,root,rpc,[this]{exportGameDialog();});connect(this,&StudioWindow::updated,dialog,[this,root,dialog]{if(view.value("root").toString()!=root)dialog->close();});connect(this,&StudioWindow::releaseExported,dialog,[dialog](QJsonObject output){StudioReleaseCenter::offerExport(dialog,output);});dialog->show();
}
void StudioWindow::showEditorIdentity(){
  if(auto existing=findChild<QDialog*>("editorIdentityDialog")){existing->show();existing->raise();return;}
  StudioReleaseCenter::showIdentity(this,[this](QString method,QJsonObject params,StudioReleaseCenter::Done done){auto timer=new QTimer(this);timer->setInterval(50);connect(timer,&QTimer::timeout,this,[this,timer,method,params,done]{if(bridge.backgroundBusy())return;timer->stop();timer->deleteLater();bridge.requestBackground(method,params,done);});timer->start();});
}
