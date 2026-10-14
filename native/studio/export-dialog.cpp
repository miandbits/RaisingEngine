#include "studio.hpp"
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QUrl>
#include <QVBoxLayout>

void StudioWindow::exportGameDialog() {
  if (bridge.busy() || view.value("root").toString().isEmpty()) return;
  const auto sourceRoot = view.value("root"), sourceRevision = view.value("revision");
  QDialog dialog(this); dialog.setObjectName("exportGameDialog"); dialog.setWindowTitle(tr("게임 내보내기")); dialog.resize(700, 330);
  auto layout = new QVBoxLayout(&dialog);
  auto help = new QLabel(tr("현재 편집 내용을 검증하여 새 폴더에 배포용 게임을 만듭니다. 저장하지 않은 초안도 포함하며, 프로젝트 파일을 자동 저장하거나 기존 출력 폴더를 덮어쓰지 않습니다.")); help->setWordWrap(true); layout->addWidget(help);
  auto form = new QFormLayout; layout->addLayout(form);
  auto target = new QComboBox; target->setObjectName("exportTarget"); target->addItem(tr("Windows · 실행 파일"), "windows"); target->addItem(tr("Web · 웹 서버에 게시할 파일"), "web"); form->addRow(tr("대상"), target);
  auto folderRow = new QHBoxLayout; auto parent = new QLineEdit(QFileInfo(sourceRoot.toString()).dir().absolutePath()); parent->setObjectName("exportParent"); folderRow->addWidget(parent);
  auto browse = new QPushButton(tr("찾기…")); folderRow->addWidget(browse); form->addRow(tr("상위 폴더"), folderRow);
  auto name = new QLineEdit; name->setObjectName("exportFolderName"); form->addRow(tr("새 폴더 이름"), name);
  QString automaticName;
  auto suggest = [&] { const auto suggested = QFileInfo(sourceRoot.toString()).fileName() + "-" + target->currentData().toString() + "-" + QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"); if (name->text().isEmpty() || name->text() == automaticName) name->setText(suggested); automaticName = suggested; };
  suggest(); connect(target, qOverload<int>(&QComboBox::currentIndexChanged), &dialog, [&](int) { suggest(); });
  connect(browse, &QPushButton::clicked, &dialog, [&] { const auto path = QFileDialog::getExistingDirectory(&dialog, tr("출력 폴더를 만들 상위 폴더"), parent->text()); if (!path.isEmpty()) parent->setText(path); });
  auto status = new QLabel; status->setObjectName("exportResult"); status->setWordWrap(true); status->setTextInteractionFlags(Qt::TextSelectableByMouse); layout->addWidget(status);
  auto row = new QHBoxLayout; layout->addLayout(row); auto open = new QPushButton(tr("출력 폴더 열기")); open->setObjectName("exportOpenFolder"); open->setEnabled(false); row->addWidget(open); row->addStretch();
  auto start = new QPushButton(tr("내보내기")); start->setObjectName("exportStart"); row->addWidget(start); auto close = new QPushButton(tr("닫기")); row->addWidget(close); connect(close, &QPushButton::clicked, &dialog, &QDialog::reject);
  connect(open, &QPushButton::clicked, &dialog, [open] { QDesktopServices::openUrl(QUrl::fromLocalFile(open->property("directory").toString())); });
  connect(start, &QPushButton::clicked, &dialog, [&] {
    if (bridge.busy()) { status->setText(tr("진행 중인 작업이 끝난 뒤 다시 내보내세요.")); return; }
    if (view.value("root") != sourceRoot || view.value("revision") != sourceRevision) { status->setText(tr("프로젝트가 변경되었습니다. 창을 다시 열어 최신 내용을 내보내세요.")); return; }
    const auto leaf = name->text();
    if (!QDir::isAbsolutePath(parent->text()) || !QDir(parent->text()).exists() || leaf.isEmpty() || leaf == "." || leaf == ".." || leaf.endsWith(' ') || leaf.endsWith('.') || leaf.contains(QRegularExpression(R"([<>:"/\\|?*])"))) { status->setText(tr("존재하는 상위 폴더와 올바른 새 폴더 이름을 입력하세요.")); return; }
    const auto destination = QDir(parent->text()).filePath(leaf);
    if (QFileInfo::exists(destination)) { status->setText(tr("출력 폴더가 이미 있습니다. 새 이름을 선택하세요.")); return; }
    start->setEnabled(false); close->setEnabled(false); open->setEnabled(false); status->setText(tr("검증 및 내보내기 중…"));
    QPointer<QDialog> guard = &dialog; QPointer<QPushButton> startGuard = start, closeGuard = close, openGuard = open; QPointer<QLabel> statusGuard = status;
    request("export", {{"target", target->currentData().toString()}, {"destination", destination}}, [guard, startGuard, closeGuard, openGuard, statusGuard](QJsonObject reply) {
      if (!guard || !startGuard || !closeGuard || !openGuard || !statusGuard) return;
      startGuard->setEnabled(true); closeGuard->setEnabled(true);
      if (!reply.value("error").toString().isEmpty() || !reply.contains("exported")) {
        statusGuard->setText(reply.value("error").toString(QObject::tr("내보내기 결과를 받지 못했습니다.")) + "\n" + QObject::tr("문제를 해결한 뒤 다시 내보내세요. 실패한 작업의 임시 파일은 정리되며, 기존 출력 폴더는 보존합니다. 프로젝트를 변경했다면 이 창을 다시 열어 주세요."));
        return;
      }
      const auto exported = reply.value("exported").toObject(); const auto directory = exported.value("directory").toString(); openGuard->setProperty("directory", directory); openGuard->setEnabled(true);
      statusGuard->setText(QObject::tr("내보내기 완료 · %1\n%2\n콘텐츠 지문: %3").arg(exported.value("target").toString(), directory, exported.value("contentFingerprint").toString()));
    });
  });
  dialog.exec();
}
