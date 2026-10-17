#include "script-authoring.hpp"
#include "logic-editor.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <limits>

QWidget *ScriptAuthoringDialog::makeMetadata() {
  auto page = new QWidget; page->setObjectName("scriptMetadata");
  auto grid = new QGridLayout(page); grid->setContentsMargins(0, 0, 0, 0);
  auto name = new QLineEdit; name->setObjectName("scriptLabel");
  grid->addWidget(new QLabel(tr("표시 이름")), 0, 0); grid->addWidget(name, 0, 1);
  auto priority = new QSpinBox; priority->setObjectName("scriptPriority"); priority->setRange(0, 100);
  priority->setToolTip(tr("여러 사건·결말의 조건이 맞으면 높은 우선순위를 먼저 선택합니다. 기본 결말은 낮은 우선순위로 두세요."));
  grid->addWidget(new QLabel(tr("우선순위")), 0, 2); grid->addWidget(priority, 0, 3);
  auto conditions = new QPushButton(tr("발생 / 결말 조건 편집…")); conditions->setObjectName("scriptRequirements"); grid->addWidget(conditions, 0, 4);
  auto window = new QWidget; window->setObjectName("scriptWindow"); auto range = new QHBoxLayout(window); range->setContentsMargins(0, 0, 0, 0);
  auto from = new QSpinBox; from->setObjectName("scriptFromPeriod"); from->setRange(1, std::numeric_limits<int>::max());
  auto through = new QSpinBox; through->setObjectName("scriptThroughPeriod"); through->setRange(1, std::numeric_limits<int>::max());
  range->addWidget(new QLabel(tr("전체 일정 중"))); range->addWidget(from); range->addWidget(new QLabel(tr("번째부터"))); range->addWidget(through); range->addWidget(new QLabel(tr("번째까지 (1부터)")));
  grid->addWidget(window, 1, 0, 1, 2);
  auto trigger = new QComboBox; trigger->setObjectName("scriptTrigger"); trigger->addItem(tr("기간 종료 때"), "period-end"); trigger->addItem(tr("행동에서 연결"), "interaction"); grid->addWidget(trigger, 1, 2);
  auto scope = new QComboBox; scope->setObjectName("scriptScope"); scope->addItem(tr("공유 완료 기록"), "shared"); scope->addItem(tr("실행자별 완료 기록"), "actor"); grid->addWidget(scope, 1, 3);
  auto references = new QPushButton(tr("선택 노드의 참조 위치…")); references->setObjectName("scriptReferences"); grid->addWidget(references, 1, 4);
  auto repeatable = new QCheckBox(tr("조건을 만족하면 방문할 때 다시 시작")); repeatable->setObjectName("scriptRepeatable");
  repeatable->setToolTip(tr("보류한 상담·처방을 다음 방문에 다시 열 수 있습니다. 완료 기록은 하나로 유지되며, 효과는 대본 조건으로 제한하세요."));
  grid->addWidget(repeatable, 3, 0, 1, 5);
  connect(repeatable, &QCheckBox::toggled, this, [this](bool checked) { if (!filling && data().contains("window")) { auto d=data(); if (checked) d["repeatable"]=true; else d.remove("repeatable"); setData(d); } });
  referenceSummary = new QLabel(tr("대사·표시 이름을 바꿀 때 ID를 바꿀 필요는 없습니다. ID 변경은 연결과 시나리오 참조를 함께 갱신합니다."));
  referenceSummary->setObjectName("scriptReferenceSummary"); referenceSummary->setWordWrap(true); grid->addWidget(referenceSummary, 2, 0, 1, 5);
  connect(name, &QLineEdit::textChanged, this, [this, name] {
    if (filling || documentIndex < 0) return;
    auto d = data(); d["label"] = name->text(); setData(d);
    documentField->setItemText(documentIndex, name->text() + " · " + documents[documentIndex].toObject().value("file").toString());
  });
  connect(priority, qOverload<int>(&QSpinBox::valueChanged), this, [this](int n) { if (!filling && documentIndex >= 0) { auto d = data(); d["priority"] = n; setData(d); } });
  const auto updateWindow = [this, from, through] {
    if (filling || documentIndex < 0 || !data().contains("window")) return;
    auto d = data(); auto w = d.value("window").toObject(); w["fromPeriod"] = from->value() - 1; w["throughPeriod"] = through->value() - 1; d["window"] = w; setData(d);
  };
  connect(from, qOverload<int>(&QSpinBox::valueChanged), this, updateWindow);
  connect(through, qOverload<int>(&QSpinBox::valueChanged), this, updateWindow);
  connect(trigger, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, trigger, repeatable] { if (!filling && data().contains("window")) { auto d = data(); d["trigger"] = trigger->currentData().toString(); const bool direct=trigger->currentData()=="interaction"; if (!direct) d.remove("repeatable"); setData(d); QSignalBlocker blocker(repeatable); repeatable->setEnabled(direct); repeatable->setChecked(d.value("repeatable").toBool()); } });
  connect(scope, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, scope, trigger] {
    if (filling || !data().contains("window")) return;
    auto d = data(); d["scope"] = scope->currentData().toString();
    if (scope->currentData() == "actor") d["trigger"] = "interaction";
    setData(d); QSignalBlocker blocker(trigger); trigger->setCurrentIndex(trigger->findData(d.value("trigger").toString("period-end"))); trigger->setEnabled(scope->currentData() != "actor");
  });
  connect(conditions, &QPushButton::clicked, this, [this] {
    if (documentIndex < 0) return;
    LogicEditor editor(data().value("requirements"), "condition", context, this);
    if (editor.exec() == QDialog::Accepted) { auto d = data(); d["requirements"] = editor.value(); setData(d); }
  });
  connect(references, &QPushButton::clicked, this, [this] {
    const auto id = node().value("id").toString(); if (id.isEmpty()) return;
    QStringList lines;
    if (data().value("entryNodeId") == id) lines << tr("이 대본의 시작 노드");
    for (auto v : data().value("nodes").toArray()) {
      const auto n = v.toObject();
      for (const auto &key : {QString("next"), QString("then"), QString("else")}) if (n.value(key) == id) lines << n.value("id").toString() + " / " + key;
      for (auto choice : n.value("choices").toArray()) if (choice.toObject().value("next") == id) lines << n.value("id").toString() + " / " + choice.toObject().value("id").toString();
    }
    lines += externalReferences(id);
    QMessageBox::information(this, tr("노드 %1의 참조 위치").arg(id), lines.isEmpty() ? tr("이 노드를 참조하는 연결이 없습니다.") : lines.join('\n'));
  });
  return page;
}
void ScriptAuthoringDialog::refreshMetadata() {
  if (!metadata) return;
  const bool previous = filling; filling = true; const auto d = data(); metadata->setEnabled(!d.isEmpty());
  metadata->findChild<QLineEdit *>("scriptLabel")->setText(d.value("label").toString());
  metadata->findChild<QSpinBox *>("scriptPriority")->setValue(d.value("priority").toInt());
  const auto w = d.value("window").toObject(); const bool event = d.contains("window");
  metadata->findChild<QWidget *>("scriptWindow")->setVisible(event);
  metadata->findChild<QSpinBox *>("scriptFromPeriod")->setValue(w.value("fromPeriod").toInt() + 1);
  metadata->findChild<QSpinBox *>("scriptThroughPeriod")->setValue(w.value("throughPeriod").toInt() + 1);
  auto scope = metadata->findChild<QComboBox *>("scriptScope"); scope->setVisible(event); scope->setCurrentIndex(scope->findData(d.value("scope").toString("shared")));
  auto trigger = metadata->findChild<QComboBox *>("scriptTrigger"); trigger->setVisible(event); trigger->setCurrentIndex(trigger->findData(d.value("trigger").toString("period-end"))); trigger->setEnabled(scope->currentData() != "actor");
  auto repeatable = metadata->findChild<QCheckBox *>("scriptRepeatable"); repeatable->setVisible(event); repeatable->setEnabled(trigger->currentData()=="interaction"); repeatable->setChecked(d.value("repeatable").toBool());
  filling = previous;
}
void ScriptAuthoringDialog::selectDocument(const QString &file) {
  for (int i = 0; i < documents.size(); ++i) if (documents[i].toObject().value("file") == file) { documentField->setCurrentIndex(i); return; }
}

// Only scenario choice references are rewritten; arbitrary strings, historical
// recordings and saved games are deliberately not treated as mutable links.
static QJsonValue scenarioReferences(QJsonValue value, QString path, QString event, QString from,
                                    QString to, QString choiceFrom, QString choiceTo, QStringList *hits) {
  if (value.isArray()) { auto a = value.toArray(); for (int i = 0; i < a.size(); ++i) a[i] = scenarioReferences(a[i], path + "/" + QString::number(i), event, from, to, choiceFrom, choiceTo, hits); return a; }
  if (!value.isObject()) return value;
  auto obj = value.toObject();
  if (obj.value("eventId") == event && obj.value("nodeId") == from && (choiceFrom.isEmpty() || obj.value("choiceId") == choiceFrom)) {
    hits->append(path + (choiceFrom.isEmpty() ? "/nodeId" : "/choiceId"));
    if (choiceFrom.isEmpty()) obj["nodeId"] = to; else obj["choiceId"] = choiceTo;
  }
  for (auto it = obj.begin(); it != obj.end(); ++it) it.value() = scenarioReferences(it.value(), path + "/" + it.key(), event, from, to, choiceFrom, choiceTo, hits);
  return obj;
}
QStringList ScriptAuthoringDialog::externalReferences(QString id, QString choice) const {
  QStringList result;
  if (documentIndex < 0 || documents[documentIndex].toObject().value("kind") != "event") return result;
  for (auto doc : externalDocuments) {
    QStringList hits; scenarioReferences(doc.toObject().value("data"), {}, data().value("id").toString(), id, id, choice, choice, &hits);
    for (const auto &hit : hits) result << doc.toObject().value("file").toString() + " · " + hit;
  }
  return result;
}
void ScriptAuthoringDialog::renameExternalReferences(QString from, QString to, QString choiceFrom, QString choiceTo) {
  if (documentIndex < 0 || documents[documentIndex].toObject().value("kind") != "event") return;
  int count = 0;
  for (int i = 0; i < externalDocuments.size(); ++i) {
    auto doc = externalDocuments[i].toObject(); QStringList hits;
    doc["data"] = scenarioReferences(doc.value("data"), {}, data().value("id").toString(), from, to, choiceFrom, choiceTo, &hits);
    count += hits.size(); externalDocuments[i] = doc;
  }
  referenceSummary->setText(tr("ID 변경: %1 → %2 · 대본 연결과 시나리오 참조 %3곳을 갱신했습니다. 저장하면 함께 적용되고, 취소하면 모두 취소됩니다.").arg(choiceFrom.isEmpty() ? from : choiceFrom, choiceFrom.isEmpty() ? to : choiceTo).arg(count));
}
