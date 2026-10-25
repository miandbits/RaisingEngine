#include "save-migration.hpp"
#include "bridge.hpp"
#include <QCheckBox>
#include <QDialog>
#include <QFileDialog>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <memory>

void showSaveMigration(const QString& engineRoot,const QString& currentProject,QWidget* parent,const QString& previousProject,const QString& sampleSave){
  auto d=new QDialog(parent);d->setAttribute(Qt::WA_DeleteOnClose);d->setObjectName("saveMigrationDialog");d->setWindowTitle(QObject::tr("콘텐츠 업데이트 · 저장 이전 / 복구"));d->resize(1000,700);
  auto layout=new QVBoxLayout(d);auto intro=new QLabel(QObject::tr("디스크에 저장된 이전·새 프로젝트로 검사합니다. 편집 중인 초안은 사용하지 않습니다.\n원본 저장과 기존 출력 파일은 덮어쓰지 않습니다. 복구하려면 보관한 이전 프로젝트에서 원본 저장을 여세요."),d);intro->setWordWrap(true);layout->addWidget(intro);
  auto form=new QFormLayout;layout->addLayout(form);
  auto row=[&](const QString& name,const QString& title,bool directory,bool output){auto box=new QWidget(d);auto line=new QHBoxLayout(box);line->setContentsMargins(0,0,0,0);auto edit=new QLineEdit(box);edit->setObjectName(name);auto browse=new QPushButton(QObject::tr("찾기…"),box);line->addWidget(edit);line->addWidget(browse);form->addRow(title,box);QObject::connect(browse,&QPushButton::clicked,d,[=]{QString value;if(directory)value=QFileDialog::getExistingDirectory(d,title,edit->text());else if(output)value=QFileDialog::getSaveFileName(d,title,edit->text(),"JSON (*.json)");else value=QFileDialog::getOpenFileName(d,title,edit->text(),"JSON (*.json)");if(!value.isEmpty())edit->setText(value);});return edit;};
  auto from=row("migrationFrom",QObject::tr("이전 프로젝트"),true,false),to=row("migrationTo",QObject::tr("새 프로젝트"),true,false),save=row("migrationSave",QObject::tr("원본 저장"),false,false),output=row("migrationOutput",QObject::tr("새 저장 경로"),false,true);to->setText(currentProject);from->setText(previousProject);save->setText(sampleSave);
  auto reset=new QCheckBox(QObject::tr("연출·플러그인 재생 위치 초기화 허용 (게임 진행은 명령 기록으로 재현)"),d);reset->setObjectName("migrationReset");layout->addWidget(reset);
  auto resetHelp=new QLabel(QObject::tr("연출 초기화는 미확정 계획, 기록 이름·썸네일, 재생 위치와 플러그인 연출 상태를 제거합니다. 게임 진행은 검증한 명령으로 다시 만듭니다."),d);resetHelp->setObjectName("migrationResetHelp");resetHelp->setWordWrap(true);layout->addWidget(resetHelp);
  auto summary=new QLabel(QObject::tr("이전 원본과 새 콘텐츠를 선택하고 검사하세요."),d);summary->setWordWrap(true);summary->setObjectName("migrationSummary");layout->addWidget(summary);
  auto tree=new QTreeWidget(d);tree->setObjectName("migrationChanges");tree->setHeaderLabels({QObject::tr("변경 위치"),QObject::tr("이전 값"),QObject::tr("새 값")});layout->addWidget(tree,1);
  auto buttons=new QHBoxLayout;layout->addLayout(buttons);auto preview=new QPushButton(QObject::tr("검사·비교"),d),publish=new QPushButton(QObject::tr("검토한 결과를 새 파일로 저장"),d),close=new QPushButton(QObject::tr("닫기"),d);preview->setObjectName("migrationPreview");publish->setObjectName("migrationPublish");publish->setEnabled(false);buttons->addWidget(preview);buttons->addWidget(publish);buttons->addStretch();buttons->addWidget(close);
  auto bridge=new StudioBridge(engineRoot,d);auto approval=std::make_shared<QString>();QPointer<QDialog> guard(d);
  auto changed=[=]{approval->clear();publish->setEnabled(false);};for(auto field:{from,to,save,output})QObject::connect(field,&QLineEdit::textChanged,d,changed);QObject::connect(reset,&QCheckBox::toggled,d,changed);
  auto run=[=](bool writing){if(bridge->busy())return;preview->setEnabled(false);publish->setEnabled(false);for(auto field:{from,to,save,output})field->setEnabled(false);reset->setEnabled(false);summary->setText(QObject::tr("원본 검증 및 새 콘텐츠 재현 중…"));QJsonObject params{{"from",from->text()},{"to",to->text()},{"save",save->text()},{"output",output->text()},{"approval",*approval},{"resetPresentation",reset->isChecked()}};
    bridge->request(writing?"saveMigrationPublish":"saveMigrationPreview",params,[=](QJsonObject reply){if(!guard)return;preview->setEnabled(true);for(auto field:{from,to,save,output})field->setEnabled(true);reset->setEnabled(true);if(!reply.value("error").isNull()){summary->setText(reply.value("error").toString());approval->clear();return;}auto result=reply.value("migration").toObject(),report=result.value("report").toObject();tree->clear();auto display=[](QJsonValue v){auto bytes=QJsonDocument(QJsonArray{v}).toJson(QJsonDocument::Compact);return QString::fromUtf8(bytes.mid(1,bytes.size()-2));};for(auto value:report.value("changes").toArray()){auto c=value.toObject();new QTreeWidgetItem(tree,{c.value("path").toString(),display(c.value("before")),display(c.value("after"))});}tree->resizeColumnToContents(0);*approval=report.value("approval").toString();publish->setEnabled(!writing&&!report.value("truncated").toBool());summary->setText(writing?QObject::tr("새 저장 완료: %1\n원본은 보존되었습니다.").arg(result.value("file").toString()):QObject::tr("%1개 명령 재현 완료 · 변경 %2개 · 연출 초기화 %3%4\n변경 값을 검토한 후 새 파일로 저장하세요.").arg(report.value("commands").toInt()).arg(tree->topLevelItemCount()).arg(report.value("presentationReset").toBool()?QObject::tr("허용됨"):QObject::tr("없음")).arg(report.value("truncated").toBool()?QObject::tr(" · 표시 한도 초과, GUI 승인 불가"):QString()));});};
  QObject::connect(preview,&QPushButton::clicked,d,[=]{run(false);});QObject::connect(publish,&QPushButton::clicked,d,[=]{run(true);});QObject::connect(close,&QPushButton::clicked,d,&QDialog::close);
  QObject::connect(bridge,&StudioBridge::failed,d,[=](QString error){summary->setText(error);preview->setEnabled(true);publish->setEnabled(false);for(auto field:{from,to,save,output})field->setEnabled(true);reset->setEnabled(true);});d->show();
}
