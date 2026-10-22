#include "studio.hpp"
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

void StudioWindow::showVersionControl() {
  const auto root=view.value("root").toString();if(root.isEmpty())return;
  QDialog dialog(this);dialog.setObjectName("versionControlDialog");dialog.setWindowTitle(tr("프로젝트 버전 관리"));dialog.resize(940,640);
  auto layout=new QVBoxLayout(&dialog);
  auto hint=new QLabel(tr("Git은 디스크에 저장된 파일을 비교합니다. 먼저 프로젝트를 저장하세요. 이 창에서는 커밋·되돌리기·푸시를 자동 실행하지 않습니다."));hint->setWordWrap(true);layout->addWidget(hint);
  auto path=new QLabel(root);path->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(path);
  auto bar=new QHBoxLayout;layout->addLayout(bar);
  auto saveButton=new QPushButton(tr("프로젝트 저장"));saveButton->setObjectName("versionSave");bar->addWidget(saveButton);connect(saveButton,&QPushButton::clicked,this,&StudioWindow::save);
  auto output=new QPlainTextEdit;output->setObjectName("versionOutput");output->setReadOnly(true);output->setLineWrapMode(QPlainTextEdit::NoWrap);layout->addWidget(output);
  auto process=new QProcess(&dialog);process->setWorkingDirectory(root);process->setProcessChannelMode(QProcess::MergedChannels);
  auto timeout=new QTimer(&dialog);timeout->setSingleShot(true);
  connect(timeout,&QTimer::timeout,&dialog,[process,output]{process->kill();output->appendPlainText(QObject::tr("Git 조회 시간이 초과되어 중단했습니다."));});
  connect(process,&QProcess::readyReadStandardOutput,&dialog,[process,output]{
    const auto text=QString::fromUtf8(process->readAllStandardOutput());
    if(output->document()->characterCount()+text.size()>200000){process->kill();output->appendPlainText(QObject::tr("표시 한도를 넘었습니다. 외부 버전 관리 도구에서 전체 내용을 확인하세요."));return;}
    auto cursor=output->textCursor();cursor.movePosition(QTextCursor::End);cursor.insertText(text);output->setTextCursor(cursor);
  });
  connect(process,&QProcess::errorOccurred,&dialog,[output](QProcess::ProcessError error){if(error==QProcess::FailedToStart)output->setPlainText(QObject::tr("Git을 실행할 수 없습니다. Git 설치와 PATH를 확인하세요."));});
  connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),&dialog,[timeout,output](int code,QProcess::ExitStatus){timeout->stop();if(code)output->appendPlainText(QObject::tr("Git 조회가 실패했습니다. 저장소 여부와 위 메시지를 확인하세요."));else if(output->toPlainText().isEmpty())output->setPlainText(QObject::tr("표시할 변경이나 이력이 없습니다."));});
  auto run=[process,timeout,output](QStringList args){if(process->state()!=QProcess::NotRunning)return;output->clear();args.prepend("core.quotepath=false");args.prepend("-c");process->start("git",args);timeout->start(15000);};
  for(const auto &item:QList<QPair<QString,QStringList>>{
    {tr("변경 상태"),{"status","--short","--branch","--","."}},
    {tr("작업 변경"),{"diff","--no-ext-diff","--no-textconv","--","."}},
    {tr("스테이징 변경"),{"diff","--cached","--no-ext-diff","--no-textconv","--","."}},
    {tr("최근 이력"),{"log","-30","--date=short","--format=%h %ad %s","--","."}}}){
    auto button=new QPushButton(item.first);bar->addWidget(button);button->setObjectName("version-"+item.second[0]+(item.second.contains("--cached")?"-cached":""));connect(button,&QPushButton::clicked,&dialog,[run,item]{run(item.second);});
  }
  auto tools=new QHBoxLayout;layout->addLayout(tools);
  auto external=new QPushButton(tr("외부 버전 관리 도구 열기"));external->setObjectName("versionExternal");tools->addWidget(external);
  auto configure=new QPushButton(tr("외부 도구 지정…"));tools->addWidget(configure);
  auto program=new QLabel(QSettings().value("versionControlProgram").toString());program->setWordWrap(true);layout->addWidget(program);
  connect(configure,&QPushButton::clicked,&dialog,[&dialog,program]{
    const auto selected=QFileDialog::getOpenFileName(&dialog,QObject::tr("프로젝트 폴더를 인자로 받는 버전 관리 프로그램"),{},QObject::tr("실행 파일 (*.exe)"));
    if(selected.isEmpty())return;program->setText(selected);QSettings().setValue("versionControlProgram",selected);
  });
  connect(external,&QPushButton::clicked,&dialog,[program,root,output]{
    const QFileInfo file(program->text());
    if(!file.isFile()||file.suffix().compare("exe",Qt::CaseInsensitive)!=0){output->setPlainText(QObject::tr("외부 도구 실행 파일을 먼저 지정하세요. 프로젝트 폴더 하나를 인자로 전달합니다."));return;}
    if(!QProcess::startDetached(file.absoluteFilePath(),{root},root))output->setPlainText(QObject::tr("외부 도구를 실행하지 못했습니다."));
  });
  auto folder=new QPushButton(tr("프로젝트 폴더 열기"));tools->addWidget(folder);connect(folder,&QPushButton::clicked,&dialog,[root]{QDesktopServices::openUrl(QUrl::fromLocalFile(root));});
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Close);buttons->button(QDialogButtonBox::Close)->setText(tr("닫기"));layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  QTimer::singleShot(0,&dialog,[run]{run({"status","--short","--branch","--","."});});
  dialog.exec();if(process->state()!=QProcess::NotRunning){process->kill();process->waitForFinished(1000);}
}
