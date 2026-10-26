#pragma once
#include <QCoreApplication>
#include <QDir>
#include <memory>
#include <QJsonObject>
#include <QJsonArray>
#include <QScrollArea>
#include <QLineEdit>
#include <QListWidget>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPointer>
#include <QMessageBox>
#include <functional>
namespace StudioRelease {
inline QString tr(const char* text){return QCoreApplication::translate("StudioWindow",text);}
class Settings : public QScrollArea {
public:
 QLineEdit *name;QLabel *iconLabel;QListWidget *notices;QPushButton *backup;QJsonObject original;QString iconFile;bool iconRemoved=false;
 Settings(const QJsonObject& release,const QString& root,QWidget* parent=nullptr):QScrollArea(parent),original(release){
  setObjectName("projectReleaseSettings");setWidgetResizable(true);auto body=new QWidget;auto form=new QFormLayout(body);setWidget(body);
  name=new QLineEdit(release.value("displayName").toString());name->setObjectName("projectReleaseName");name->setMaxLength(80);name->setPlaceholderText(tr("플레이어의 기본 앱 이름 유지"));form->addRow(tr("앱 이름"),name);
  auto help=new QLabel(tr("출시 설정은 패키징에만 적용되며 게임 규칙이나 저장 지문을 바꾸지 않습니다. 실행 파일명은 RaisingPlayer를 유지하고 운영체제에 표시되는 이름과 아이콘을 변경합니다. Windows 출력은 서명되지 않으므로 배포 전에 완성된 게임에 서명하세요."));help->setWordWrap(true);form->addRow(help);
  const auto savedIcon=release.value("icon").toObject().value("path").toString();iconLabel=new QLabel(savedIcon.isEmpty()?tr("사용자 지정 앱 아이콘 없음"):QFileInfo(savedIcon).fileName());iconLabel->setToolTip(savedIcon);iconLabel->setObjectName("projectReleaseIcon");iconLabel->setWordWrap(true);form->addRow(tr("앱 아이콘"),iconLabel);
  auto icons=new QWidget;auto il=new QHBoxLayout(icons);il->setContentsMargins(0,0,0,0);auto chooseIcon=new QPushButton(tr("아이콘 PNG 선택…")),clearIcon=new QPushButton(tr("기본 아이콘 사용"));chooseIcon->setObjectName("projectReleaseChooseIcon");clearIcon->setObjectName("projectReleaseClearIcon");il->addWidget(chooseIcon);il->addWidget(clearIcon);form->addRow(icons);
  auto iconHelp=new QLabel(tr("불투명한 1024 × 1024 PNG를 사용하세요(최대 20 MiB). 내보내기에서 Windows·macOS·Web·iOS용 실제 아이콘 리소스를 생성합니다. 설정 적용 시 파일을 가져옵니다."));iconHelp->setWordWrap(true);form->addRow(iconHelp);
  connect(chooseIcon,&QPushButton::clicked,this,[this]{const auto file=QFileDialog::getOpenFileName(this,tr("앱 아이콘 선택"),{},"PNG (*.png)");if(!file.isEmpty())selectIcon(file);});
  connect(clearIcon,&QPushButton::clicked,this,[this]{iconFile.clear();iconRemoved=true;iconLabel->setText(tr("사용자 지정 앱 아이콘 없음"));});
  notices=new QListWidget;notices->setObjectName("projectReleaseNotices");notices->setMinimumHeight(100);notices->setSelectionMode(QAbstractItemView::ExtendedSelection);form->addRow(tr("선언한 라이선스·고지 파일"),notices);
  for(auto value:release.value("notices").toArray()){const auto notice=value.toObject();auto item=new QListWidgetItem(notice.value("label").toString(notice.value("id").toString()),notices);item->setToolTip(notice.value("path").toString()+"\n"+notice.value("sha256").toString());item->setData(Qt::UserRole,notice);}
  auto row=new QWidget;auto controls=new QHBoxLayout(row);controls->setContentsMargins(0,0,0,0);auto add=new QPushButton(tr("고지 추가…")),remove=new QPushButton(tr("선택 항목 제거")),view=new QPushButton(tr("고지 보기"));add->setObjectName("projectReleaseAddNotice");remove->setObjectName("projectReleaseRemoveNotice");view->setObjectName("projectReleaseViewNotice");for(auto b:{add,remove,view})controls->addWidget(b);form->addRow(row);
  auto noticeHelp=new QLabel(tr("필요한 고지 파일을 직접 모두 선택하세요. 라이선스를 추정하거나 다운로드하지 않습니다. 파일당 최대 1 MiB의 UTF-8 텍스트를 줄바꿈까지 원본 그대로 보존해 licenses/project에 포함합니다. 선언한 파일이 변경되거나 없으면 내보내기가 중단됩니다. 이용 조건은 직접 확인하세요."));noticeHelp->setWordWrap(true);form->addRow(noticeHelp);
  connect(add,&QPushButton::clicked,this,[this]{for(const auto& file:QFileDialog::getOpenFileNames(this,tr("고지 파일 선택"),{},tr("텍스트 파일 (*.txt *.md);;모든 파일 (*)"))){auto item=new QListWidgetItem(QFileInfo(file).fileName(),notices);item->setToolTip(file);item->setData(Qt::UserRole,QJsonObject{{"source",file}});}});
  connect(remove,&QPushButton::clicked,this,[this]{qDeleteAll(notices->selectedItems());});
  connect(view,&QPushButton::clicked,this,[this,root]{auto item=notices->currentItem();if(!item)return;const auto value=item->data(Qt::UserRole).toJsonObject();QFile file(value.contains("source")?value.value("source").toString():QDir(root).filePath(value.value("path").toString()));if(!file.open(QIODevice::ReadOnly)||file.size()>1024*1024){QMessageBox::warning(this,tr("고지를 열 수 없음"),tr("선택한 고지 파일이 없거나 읽을 수 없거나 1 MiB보다 큽니다."));return;}QDialog dialog(this);dialog.setWindowTitle(tr("고지 미리보기"));dialog.resize(620,480);auto layout=new QVBoxLayout(&dialog);auto text=new QPlainTextEdit(QString::fromUtf8(file.readAll()));text->setReadOnly(true);layout->addWidget(text);auto buttons=new QDialogButtonBox(QDialogButtonBox::Close);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);dialog.exec();});
  backup=new QPushButton(tr("프로젝트 백업 검증·복원…"));backup->setObjectName("projectReleaseRestoreBackup");form->addRow(backup);
  auto backupHelp=new QLabel(tr("버전을 지정한 내보내기는 Project-backup.zip을 새 임시 폴더에서 다시 열어 검증합니다. 복원 시 모든 선언된 원본 해시를 다시 검사하고 새 프로젝트 폴더를 만듭니다. 현재 프로젝트를 덮어쓰지 않으며 검증 중 신뢰하지 않는 플러그인 코드를 실행하지 않습니다."));backupHelp->setWordWrap(true);form->addRow(backupHelp);
 }
 void selectIcon(const QString& file){iconFile=file;iconRemoved=false;iconLabel->setText(QFileInfo(file).fileName());iconLabel->setToolTip(file);}
 QJsonObject values()const{QJsonObject result=original;const auto title=name->text().trimmed();if(title.isEmpty())result.remove("displayName");else result["displayName"]=title;if(iconRemoved)result.remove("icon");QJsonArray entries;for(int i=0;i<notices->count();++i){const auto value=notices->item(i)->data(Qt::UserRole).toJsonObject();if(!value.contains("source"))entries.append(value);}if(entries.isEmpty())result.remove("notices");else result["notices"]=entries;return result;}
 QJsonArray imports()const{QJsonArray files;for(int i=0;i<notices->count();++i){const auto value=notices->item(i)->data(Qt::UserRole).toJsonObject();if(value.contains("source"))files.append(value.value("source"));}return files;}
};
using Request=std::function<void(QString,QJsonObject,std::function<void(QJsonObject)>)>;
inline void restoreDialog(QWidget* parent,Request request){
 QDialog dialog(parent);dialog.setObjectName("releaseBackupDialog");dialog.setWindowTitle(tr("프로젝트 백업 검증·복원"));dialog.resize(620,380);auto layout=new QVBoxLayout(&dialog);auto file=new QLineEdit;file->setObjectName("releaseBackupFile");auto choose=new QPushButton(tr("Project-backup.zip 선택…"));layout->addWidget(file);layout->addWidget(choose);auto verify=new QPushButton(tr("백업 검증"));verify->setObjectName("releaseBackupVerify");layout->addWidget(verify);auto status=new QLabel(tr("프로젝트 백업을 선택하세요. 원본 바이트를 검사하고 문서를 다시 열며 플러그인이 없으면 게임 지문도 확인합니다. 압축 파일 작성자의 신원을 보증하지는 않습니다."));status->setObjectName("releaseBackupStatus");status->setWordWrap(true);layout->addWidget(status);auto restore=new QPushButton(tr("새 폴더에 복원…"));restore->setObjectName("releaseBackupRestore");restore->setEnabled(false);layout->addWidget(restore);auto close=new QDialogButtonBox(QDialogButtonBox::Close);layout->addWidget(close);QObject::connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
 auto state=std::make_shared<QJsonObject>();QPointer<QDialog> guard=&dialog;QPointer<QLabel> feedback=status;QPointer<QPushButton> restoreGuard=restore,verifyGuard=verify;QPointer<QLineEdit> fileGuard=file;
 QObject::connect(choose,&QPushButton::clicked,&dialog,[file,&dialog]{auto selected=QFileDialog::getOpenFileName(&dialog,tr("프로젝트 백업 선택"),{},"ZIP (*.zip)");if(!selected.isEmpty())file->setText(selected);});
 QObject::connect(file,&QLineEdit::textChanged,&dialog,[state,restore]{state->remove("archiveSha256");restore->setEnabled(false);});
 QObject::connect(verify,&QPushButton::clicked,&dialog,[=]{verify->setEnabled(false);restore->setEnabled(false);status->setText(tr("백업 검증 중…"));const auto selected=file->text();request("releaseBackupVerify",{{"file",selected}},[=](QJsonObject reply){if(!guard)return;verifyGuard->setEnabled(true);if(!reply.value("error").toString().isEmpty()){feedback->setText(reply.value("error").toString());return;}if(fileGuard->text()!=selected){feedback->setText(tr("선택한 백업이 변경되었습니다. 다시 검증하세요."));return;}*state=reply.value("backupVerification").toObject();feedback->setText(tr("원본 파일 %1개를 검증하고 문서 %2개를 다시 열었습니다. %3").arg(state->value("sourceFilesVerified").toInt()).arg(state->value("documentsReopened").toInt()).arg(state->value("pluginValidation").toString()));restoreGuard->setEnabled(state->value("ok").toBool());});});
 QObject::connect(restore,&QPushButton::clicked,&dialog,[=]{const auto destination=QFileDialog::getSaveFileName(guard,tr("새 프로젝트 폴더"),QDir::homePath()+"/Restored game",{},nullptr,QFileDialog::DontConfirmOverwrite);if(destination.isEmpty())return;restoreGuard->setEnabled(false);verifyGuard->setEnabled(false);request("releaseBackupRestore",{{"file",fileGuard->text()},{"destination",destination},{"sha256",state->value("archiveSha256")}},[=](QJsonObject reply){if(!guard)return;verifyGuard->setEnabled(true);if(!reply.value("error").toString().isEmpty()){feedback->setText(reply.value("error").toString());restoreGuard->setEnabled(true);return;}feedback->setText(tr("복원 및 검증 완료: %1\n준비되면 이 새 폴더를 여세요. 현재 프로젝트는 변경되지 않았습니다.").arg(reply.value("backupRestored").toObject().value("directory").toString()));});});
 dialog.exec();
}
}
