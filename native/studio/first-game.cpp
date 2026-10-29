#include "studio.hpp"
#include <QtWidgets>

void StudioWindow::showFirstGameChecklist() {
  if (!firstGameDock) {
    auto body = new QWidget;
    auto layout = new QVBoxLayout(body);
    auto intro = new QLabel(tr("Build a complete game with the existing editing tools. Your starter already plays from beginning to end. Customize any step, then test and export."));
    intro->setWordWrap(true); layout->addWidget(intro);
    auto progress = new QLabel; progress->setWordWrap(true); progress->setObjectName("firstGameSummary"); layout->addWidget(progress);
    auto target=new QComboBox;target->setObjectName("firstGameTarget");
    for(const auto &item:QList<QPair<QString,QString>>{{"macos","macOS"},{"windows","Windows"},{"web","Web"},{"ios","iOS"}})target->addItem(item.second,item.first);
    layout->addWidget(new QLabel(tr("Where will people play?")));layout->addWidget(target);
    auto compatibility=new QLabel;compatibility->setWordWrap(true);compatibility->setObjectName("firstGameCompatibility");layout->addWidget(compatibility);
    connect(target,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,target]{if(!busy())request("releaseTarget",{{"target",target->currentData().toString()}});});
    auto recovery=new QLabel;recovery->setWordWrap(true);recovery->setObjectName("firstGameRecovery");layout->addWidget(recovery);
    for (int i = 0; i < 8; ++i) {
      auto button = new QPushButton; button->setObjectName("firstGameStep" + QString::number(i));
      button->setMinimumHeight(34); layout->addWidget(button);
      auto help = new QLabel; help->setObjectName("firstGameHint" + QString::number(i)); help->setWordWrap(true); layout->addWidget(help);
      connect(button, &QPushButton::clicked, this, [this, i] {
        if (busy()) return;
        const auto rows = firstGameState.value("rows").toArray();
        if (i >= rows.size()) return;
        const auto command = rows[i].toObject().value("action").toString();
        if (command == "settings") { editProjectSettings(); return; }
        if (command == "look") { showWorkspace({}); openScreen("title"); return; }
        if (command == "activity" || command == "ending") {
          bottom->setCurrentWidget(contentList->parentWidget());
          if (auto search = findChild<QLineEdit*>("contentSearch")) search->clear();
          const auto docs = view.value("documents").toArray();
          for (int n = 0; n < contentList->count(); ++n) {
            auto item = contentList->item(n);
            // Content rows carry their project-relative document path.
            const auto file = item->data(Qt::UserRole).toString();
            for (const auto &value : docs) {
              const auto doc = value.toObject();
              if (doc.value("file").toString() == file && doc.value("kind").toString() == command) {
                contentList->setCurrentItem(item); editContent(); return;
              }
            }
          }
          showAuthoringWorkspace(); return;
        }
        if (auto action = findChild<QAction*>(command)) action->trigger();
      });
    }
    auto why=new QPushButton(tr("Why didn't an event or ending happen?"));why->setObjectName("firstGameWhy");layout->addWidget(why);
    connect(why,&QPushButton::clicked,this,[this]{showRuleDebugger();if(auto type=ruleDebugDock->findChild<QComboBox*>("debugSubjectKind"))type->setCurrentIndex(type->findData("event"));if(!busy())ruleDebugDock->findChild<QPushButton*>("debugStart")->click();});
    auto routes=new QPushButton(tr("Save and rerun named playtests"));routes->setObjectName("firstGameRoutes");layout->addWidget(routes);
    connect(routes,&QPushButton::clicked,this,&StudioWindow::goalRoutesDialog);
    auto recover=new QPushButton(tr("Recover unsaved work..."));layout->addWidget(recover);connect(recover,&QPushButton::clicked,this,&StudioWindow::showDraftRecovery);
    layout->addStretch();
    auto scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setWidget(body); scroll->setMinimumWidth(350);
    firstGameDock = addStudioDock(tr("My first raising game"), "firstGameChecklist", scroll, Qt::RightDockWidgetArea);
    if (auto inspector = findChild<QDockWidget*>("inspectorDock")) tabifyDockWidget(inspector, firstGameDock);
    connect(this, &StudioWindow::updated, this, &StudioWindow::updateFirstGameChecklist);
  }
  updateFirstGameChecklist(); firstGameDock->show(); firstGameDock->raise();
}
void StudioWindow::updateFirstGameChecklist() {
  if (!firstGameDock) return;
  auto target=firstGameDock->findChild<QComboBox*>("firstGameTarget");
  {QSignalBlocker block(target);target->setCurrentIndex(qMax(0,target->findData(releaseCompatibility["target"].toString())));}
  target->setEnabled(!busy());
  firstGameDock->findChild<QLabel*>("firstGameCompatibility")->setText(releaseCompatibility["summary"].toString());
  firstGameDock->findChild<QLabel*>("firstGameRecovery")->setText(recoveryState.contains("warning")?recoveryState["warning"].toString():recoveryState.contains("savedAt")?tr("Unsaved edits have a recovery copy. Save project to apply them to your project files."):tr("Project saves and player saves are separate. Recovery copies protect edits applied in the editor."));
  const auto rows = firstGameState.value("rows").toArray(); int complete = 0;
  QSet<QString> completedChecks;
  for (int i = 0; i < 8; ++i) {
    auto button = firstGameDock->findChild<QPushButton*>("firstGameStep" + QString::number(i));
    auto help = firstGameDock->findChild<QLabel*>("firstGameHint" + QString::number(i));
    const auto row = i < rows.size() ? rows[i].toObject() : QJsonObject();
    button->setVisible(!row.isEmpty()); help->setVisible(!row.isEmpty());
    if (row.isEmpty()) continue;
    const bool done = row.value("complete").toBool();
    if (done) { ++complete; completedChecks.insert(row.value("id").toString()); }
    button->setText((done ? QString::fromUtf8("✓ ") : QString::number(i + 1) + ". ") + row.value("title").toString());
    button->setProperty("complete", done); button->setEnabled(!busy());
    help->setText(row.value("description").toString());
  }
  firstGameDock->findChild<QLabel*>("firstGameSummary")->setText(rows.isEmpty()
    ? firstGameState.value("warning").toString(tr("Create a project with the recommended My first raising game starter to use this checklist."))
    : completedChecks.contains("play") && completedChecks.contains("check") && completedChecks.contains("export")
      ? tr("현재 버전을 엔딩까지 플레이하고 내보냈습니다. 추가 수정은 선택 사항입니다.")
      : tr("%1 of %2 steps completed · customization is optional").arg(complete).arg(rows.size()));
}
void StudioWindow::observeFirstGamePlay(const QJsonObject &state) {
  const auto phase = state.value("phase").toString();
  if (firstGameState.isEmpty() || (phase != "activity-result" && phase != "finished")) return;
  const auto token = state.value("contentFingerprint").toString() + phase;
  if (firstGameObserved.contains(token)) return;
  if (busy()) { QTimer::singleShot(100, this, [this, state]{ observeFirstGamePlay(state); }); return; }
  firstGameObserved.insert(token);
  request("firstGameObserve", {{"fingerprint", state.value("contentFingerprint")}, {"phase", phase}}, [this, token](QJsonObject reply) {
    if (!reply.value("error").toString().isEmpty()) firstGameObserved.remove(token);
  });
}

void StudioWindow::showDraftRecovery() {
  if(busy()||view["root"].toString().isEmpty())return;
  if(auto existing=findChild<QDialog*>("draftRecoveryDialog")){existing->show();existing->raise();return;}
  request("recoveryList",{},[this](QJsonObject reply){
    if(!reply["error"].toString().isEmpty())return;
    auto dialog=new QDialog(this);dialog->setObjectName("draftRecoveryDialog");dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle(tr("Recover unsaved work"));dialog->resize(680,340);
    auto box=new QVBoxLayout(dialog);auto explanation=new QLabel(tr("Recovery copies contain edits applied in the editor and their imported assets. Restore into a new folder to review them; your original project stays intact. Text still inside an unapplied dialog is not captured."));explanation->setWordWrap(true);box->addWidget(explanation);
    auto choices=new QComboBox;choices->setObjectName("recoveryChoice");box->addWidget(choices);
    for(const auto item:reply["recoveries"].toArray()){auto entry=item.toObject();choices->addItem(entry.contains("error")?entry["error"].toString():entry["title"].toString()+" · "+entry["updatedAt"].toString(),entry);}
    auto destination=new QLineEdit(QFileInfo(view["root"].toString()).absoluteFilePath()+"-recovered-"+QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"));destination->setObjectName("recoveryDestination");box->addWidget(new QLabel(tr("New project folder")));box->addWidget(destination);
    auto status=new QLabel;status->setObjectName("recoveryResult");status->setWordWrap(true);box->addWidget(status);
    auto buttons=new QHBoxLayout;box->addLayout(buttons);auto restore=new QPushButton(tr("Restore as a new project"));restore->setObjectName("recoveryRestore");auto open=new QPushButton(tr("Open recovered project"));open->setObjectName("recoveryOpen");open->setEnabled(false);auto close=new QPushButton(tr("Close"));buttons->addWidget(restore);buttons->addWidget(open);buttons->addWidget(close);
    restore->setEnabled(choices->count()>0);if(!choices->count())status->setText(tr("No unsaved recovery copies are available for this project."));
    connect(close,&QPushButton::clicked,dialog,&QDialog::reject);
    connect(restore,&QPushButton::clicked,dialog,[this,dialog,choices,destination,status,restore,open]{
      if(busy())return;auto entry=choices->currentData().toJsonObject();if(!entry.contains("revision")){status->setText(tr("This recovery copy needs repair. Its files have been preserved."));return;}
      entry["destination"]=destination->text();restore->setEnabled(false);QPointer<QDialog> guard(dialog);
      entry["recoveryRevision"]=entry.take("revision");request("recoveryRestore",entry,[guard,status,restore,open](QJsonObject reply){if(!guard)return;restore->setEnabled(true);if(!reply["error"].toString().isEmpty()){status->setText(reply["error"].toString());return;}const auto root=reply["restoredProject"].toString();open->setProperty("root",root);open->setEnabled(true);status->setText(QObject::tr("Recovered project: %1\nReview it before continuing. The original project was preserved.").arg(root));});
    });
    connect(open,&QPushButton::clicked,dialog,[this,dialog,open]{if(!mayDiscard())return;const auto root=open->property("root").toString();dialog->accept();openProject(root);});
    dialog->show();
  });
}
