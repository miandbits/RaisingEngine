#include "project-manager.hpp"
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <memory>

namespace {
std::unique_ptr<QSettings> history() {
  const auto overridePath = qEnvironmentVariable("RAISING_PROJECT_HISTORY");
  if (!overridePath.isEmpty()) return std::make_unique<QSettings>(overridePath, QSettings::IniFormat);
  return std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope,
                                     "RaisingEngine", "ProjectManager");
}
QString projectRoot(QString path) {
  QFileInfo info(path.trimmed());
  if (info.isFile() && info.fileName() == "project.json") info = QFileInfo(info.absolutePath());
  return info.exists() ? info.canonicalFilePath() : QDir::cleanPath(info.absoluteFilePath());
}
QString projectTitle(const QString &root) {
  QFile file(root + "/project.json");
  if (!file.open(QIODevice::ReadOnly)) return QFileInfo(root).fileName();
  return QJsonDocument::fromJson(file.readAll()).object().value("title").toString(QFileInfo(root).fileName());
}
}

QStringList ProjectManager::recentProjects() { return history()->value("recentProjects").toStringList(); }
void ProjectManager::remember(const QString &path) {
  if (qEnvironmentVariableIsSet("RAISING_QT_TEST") && !qEnvironmentVariableIsSet("RAISING_PROJECT_HISTORY")) return;
  const auto root = projectRoot(path);
  if (!QFileInfo::exists(root + "/project.json")) return;
  auto entries = recentProjects();
  entries.removeAll(root);
  entries.prepend(root);
  while (entries.size() > 40) entries.removeLast();
  history()->setValue("recentProjects", entries);
}

#include "editor-language.hpp"
ProjectManager::ProjectManager(const QString &engineRoot, QWidget *parent)
    : QDialog(parent), bridge(engineRoot, this) {
  setObjectName("projectManager");
  setWindowTitle(tr("Raising Studio — 프로젝트"));
  resize(1100, 760);
  setMinimumSize(900, 700);
  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(28, 24, 28, 24);
  auto title = new QLabel(tr("Raising Studio"));
  title->setStyleSheet("font-size:28px;font-weight:700;");
  layout->addWidget(title);
  layout->addWidget(editorLanguagePicker(this));
  layout->addWidget(new QLabel(tr("육성 시뮬레이션 제작 도구  ·  프로젝트를 선택해 작업을 시작하세요.")));
  controls = new QWidget;
  auto columns = new QHBoxLayout(controls);
  columns->setContentsMargins(0, 16, 0, 0);
  auto left = new QVBoxLayout;
  left->addWidget(new QLabel(tr("최근 프로젝트")));
  search = new QLineEdit;
  search->setObjectName("projectSearch");
  search->setPlaceholderText(tr("제목 또는 경로 검색"));
  left->addWidget(search);
  recent = new QListWidget;
  recent->setObjectName("recentProjects");
  recent->setSpacing(5);
  left->addWidget(recent, 1);
  auto actions = new QHBoxLayout;
  openButton = new QPushButton(tr("편집기 열기")); openButton->setObjectName("openRecentProject");
  removeButton = new QPushButton(tr("목록에서 제거")); removeButton->setObjectName("removeRecentProject");
  removeButton->setToolTip(tr("최근 목록에서만 제거합니다. 프로젝트 파일은 보존합니다."));
  actions->addWidget(openButton); actions->addWidget(removeButton); left->addLayout(actions);
  columns->addLayout(left, 2);
  auto right = new QVBoxLayout;
  auto existing = new QGroupBox(tr("기존 프로젝트 열기"));
  auto importLayout = new QVBoxLayout(existing);
  location = new QLineEdit; location->setObjectName("projectLocation");
  location->setPlaceholderText(tr("프로젝트 폴더 또는 project.json 경로"));
  importLayout->addWidget(location);
  auto importActions = new QHBoxLayout;
  auto browse = new QPushButton(tr("찾아보기…"));
  auto import = new QPushButton(tr("열기")); import->setObjectName("importProject");
  importActions->addWidget(browse); importActions->addWidget(import); importLayout->addLayout(importActions);
  right->addWidget(existing);
  auto createGroup = new QGroupBox(tr("새 프로젝트"));
  auto createLayout = new QVBoxLayout(createGroup);
  base = new QComboBox; base->setObjectName("launcherTemplate");
  base->addItem(tr("최소 일정 육성 · 2개월"), "academy-demo");
  base->addItem(tr("일정 육성 · 12개월"), "academy-year");
  base->addItem(tr("화면·UI 제작"), "academy-visual");
  base->addItem(tr("도시·상점·관계"), "academy-journey");
  base->addItem(tr("탐험·전투"), "academy-adventure");
  createLayout->addWidget(base);
  destination = new QLineEdit(QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath("Raising Studio Projects/new-game"));
  destination->setObjectName("launcherDestination"); createLayout->addWidget(destination);
  for (auto field : {search, location, destination}) {
    auto palette = field->palette();
    palette.setColor(QPalette::PlaceholderText, QColor("#aab5c3"));
    field->setPalette(palette);
  }
  auto choose = new QPushButton(tr("저장할 빈 폴더 선택…")); createLayout->addWidget(choose);
  auto hint = new QLabel(tr("샘플을 새 폴더로 복사해 시작합니다.\n기존 파일을 덮어쓰지 않습니다. 제목과 기간은 프로젝트 설정에서 변경할 수 있습니다."));
  hint->setWordWrap(true); createLayout->addWidget(hint);
  auto create = new QPushButton(tr("프로젝트 만들기")); create->setObjectName("launcherCreate"); createLayout->addWidget(create);
  right->addWidget(createGroup); right->addStretch();
  auto boundary = new QLabel(tr("프로젝트와 저장 게임은 엔진과 별도 폴더에 보관됩니다.\n최근 목록은 이 컴퓨터에만 저장됩니다."));
  boundary->setWordWrap(true); right->addWidget(boundary);
  columns->addLayout(right, 3); layout->addWidget(controls, 1);
  message = new QLabel; message->setObjectName("launcherMessage"); message->setWordWrap(true); layout->addWidget(message);
  auto close = new QPushButton(tr("닫기")); close->setAutoDefault(false);
  layout->addWidget(close, 0, Qt::AlignRight);
  connect(close, &QPushButton::clicked, this, &ProjectManager::reject);
  connect(search, &QLineEdit::textChanged, this, [this] { refresh(); });
  connect(recent, &QListWidget::currentRowChanged, this, [this] {
    openButton->setEnabled(recent->currentItem()); removeButton->setEnabled(recent->currentItem());
  });
  connect(openButton, &QPushButton::clicked, this, [this] { if (auto item = recent->currentItem()) open(item->data(Qt::UserRole).toString()); });
  connect(recent, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) { open(item->data(Qt::UserRole).toString()); });
  connect(removeButton, &QPushButton::clicked, this, [this] {
    if (auto item = recent->currentItem()) {
      auto entries = recentProjects(); entries.removeAll(item->data(Qt::UserRole).toString());
      history()->setValue("recentProjects", entries); refresh();
    }
  });
  connect(import, &QPushButton::clicked, this, [this] { open(location->text()); });
  connect(location, &QLineEdit::returnPressed, import, &QPushButton::click);
  connect(browse, &QPushButton::clicked, this, [this] {
    const auto file = QFileDialog::getOpenFileName(this, tr("프로젝트 선택"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), "Raising project (project.json)");
    if (!file.isEmpty()) { location->setText(file); open(file); }
  });
  connect(choose, &QPushButton::clicked, this, [this] {
    const auto folder = QFileDialog::getExistingDirectory(this, tr("새 프로젝트의 빈 폴더"), QFileInfo(destination->text()).absolutePath());
    if (!folder.isEmpty()) destination->setText(folder);
  });
  connect(create, &QPushButton::clicked, this, [this] {
    if (destination->text().trimmed().isEmpty()) { message->setText(tr("새 프로젝트 폴더를 입력하세요.")); return; }
    submit("create", {{"root", projectRoot(destination->text())}, {"template", base->currentData().toString()}});
  });
  connect(&bridge, &StudioBridge::failed, this, [this](const QString &error) { setBusy(false); message->setText(error); });
  refresh();
}
void ProjectManager::refresh() {
  recent->clear();
  for (const auto &root : recentProjects()) {
    const auto title = projectTitle(root);
    if (!(title + root).contains(search->text(), Qt::CaseInsensitive)) continue;
    const auto missing = !QFileInfo::exists(root + "/project.json");
    auto item = new QListWidgetItem(title + (missing ? tr("  ·  위치를 찾을 수 없음") : "") + "\n" + QDir::toNativeSeparators(root), recent);
    item->setData(Qt::UserRole, root); item->setToolTip(root); item->setSizeHint(QSize(300, 66));
  }
  if (recent->count()) recent->setCurrentRow(0);
  openButton->setEnabled(recent->currentItem()); removeButton->setEnabled(recent->currentItem());
  if (recentProjects().isEmpty()) message->setText(tr("아직 연 프로젝트가 없습니다. 새 프로젝트를 만들거나 기존 프로젝트를 선택하세요."));
}
void ProjectManager::open(const QString &path) {
  if (path.trimmed().isEmpty()) { message->setText(tr("프로젝트 경로를 입력하세요.")); return; }
  const auto root = projectRoot(path);
  if (!QFileInfo::exists(root + "/project.json")) { message->setText(tr("project.json을 찾을 수 없습니다. 이동한 프로젝트는 새 위치에서 다시 열어 주세요.\n") + root); return; }
  submit("open", {{"root", root}});
}
void ProjectManager::setBusy(bool busy) { controls->setEnabled(!busy); }
void ProjectManager::submit(const QString &method, QJsonObject params) {
  if (bridge.busy()) return;
  setBusy(true); message->setText(tr("프로젝트를 준비하고 있습니다…"));
  bridge.request(method, params, [this](QJsonObject reply) {
    setBusy(false);
    if (!reply.value("error").toString().isEmpty()) { message->setText(reply.value("error").toString()); return; }
    selected = reply.value("view").toObject().value("root").toString();
    if (selected.isEmpty()) { message->setText(tr("프로젝트를 열지 못했습니다. 서비스 응답에 프로젝트 경로가 없습니다.")); return; }
    selected = projectRoot(selected);
    remember(selected); accept();
  });
}
void ProjectManager::reject() {
  if (bridge.busy()) { message->setText(tr("진행 중인 프로젝트 작업이 끝나면 닫을 수 있습니다.")); return; }
  QDialog::reject();
}
