#include "script-authoring.hpp"
#include "script-continuation.hpp"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

void ScriptAuthoringDialog::insertDialogueAfter() {
  const auto selected = node();
  if (!QStringList{"scene", "dialogue", "effect"}.contains(selected.value("type").toString()) || selected.value("next").toString().isEmpty()) {
    error->setText(tr("대사·장면·효과를 선택하세요. 그 다음 연결을 유지하며 새 대사를 끼워 넣습니다."));
    return;
  }
  auto script = data(); auto nodes = script.value("nodes").toArray();
  QSet<QString> ids; for (auto value : nodes) ids.insert(value.toObject().value("id").toString());
  int number = 1; while (ids.contains("dialogue-" + QString::number(number))) ++number;
  const auto id = "dialogue-" + QString::number(number);
  auto previous = selected; previous["next"] = id; nodes[nodeIndex] = previous;
  nodes.append(QJsonObject{{"id", id}, {"type", "dialogue"}, {"speakerId", selected.value("speakerId").isUndefined() ? QJsonValue(QJsonValue::Null) : selected.value("speakerId")},
                          {"text", tr("새 대사")}, {"next", selected.value("next")}});
  script["nodes"] = nodes; setData(script); refreshNodes(id); error->clear();
}

void ScriptAuthoringDialog::connectPredecessor() {
  if (documentIndex < 0) return;
  auto dialog = new QDialog(this); dialog->setObjectName("storyPredecessorDialog");
  dialog->setWindowTitle(tr("앞선 사건과 연결")); dialog->resize(660, 460);
  auto layout = new QVBoxLayout(dialog);
  auto help = new QLabel(tr("선택한 사건을 끝낸 뒤에만 현재 사건이 열립니다. 기존 활동·날짜·선택 조건과 연출·효과는 그대로 유지합니다."));
  help->setWordWrap(true); layout->addWidget(help);
  auto search = new QLineEdit; search->setObjectName("predecessorSearch"); search->setPlaceholderText(tr("앞선 사건 이름 검색")); layout->addWidget(search);
  auto list = new QListWidget; list->setObjectName("predecessorEvents"); layout->addWidget(list, 1);
  const auto target = data().value("id").toString();
  const auto existing = requiredEvents(data().value("requirements").toObject());
  for (auto value : documents) {
    const auto doc = value.toObject(), event = doc.value("data").toObject();
    if (doc.value("kind") != "event" || event.value("id") == target) continue;
    const auto id = event.value("id").toString();
    auto row = new QListWidgetItem(event.value("label").toString(id) + (existing.contains(id) ? tr(" · 연결됨") : QString()), list);
    row->setData(Qt::UserRole, id); row->setToolTip(doc.value("file").toString());
  }
  connect(search, &QLineEdit::textChanged, dialog, [list](const QString &query) {
    for (int i=0; i<list->count(); ++i) list->item(i)->setHidden(!list->item(i)->text().contains(query, Qt::CaseInsensitive));
  });
  auto problem = new QLabel; problem->setObjectName("predecessorError"); problem->setWordWrap(true); layout->addWidget(problem);
  auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel); layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, dialog, [this, dialog, list, problem, target] {
    const auto selected = list->selectedItems();
    if (selected.size() != 1 || selected.first()->isHidden()) { problem->setText(tr("앞선 사건을 선택하세요.")); return; }
    const auto before = selected.first()->data(Qt::UserRole).toString();
    if (data().value("id") != target || continuationCycle(documents, target, before)) {
      problem->setText(tr("서로의 완료를 기다리는 순환 연결입니다. 다른 앞선 사건을 선택하세요.")); return;
    }
    auto script = data(); script["requirements"] = afterEvent(script.value("requirements").toObject(), before);
    setData(script); refreshMetadata(); error->clear(); dialog->accept();
  });
  dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->open();
}
