#include "script-authoring.hpp"
#include "script-continuation.hpp"
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
  auto trigger = new QComboBox; trigger->setObjectName("scriptTrigger"); trigger->addItem(tr("기간 종료 때"), "period-end"); trigger->addItem(tr("행동에서 연결"), "interaction"); trigger->addItem(tr("사건 완료 후"), "after-event"); grid->addWidget(trigger, 1, 2);
  auto scope = new QComboBox; scope->setObjectName("scriptScope"); scope->addItem(tr("공유 완료 기록"), "shared"); scope->addItem(tr("실행자별 완료 기록"), "actor"); grid->addWidget(scope, 1, 3);
  trigger->addItem(tr("약속 날짜 · 기간 시작"), "period-start"); trigger->addItem(tr("지정 활동을 마친 뒤"), "after-activity"); trigger->addItem(tr("지정 장소에 들어갈 때"), "location-enter");
  auto appointmentActivity=new QComboBox;appointmentActivity->setObjectName("scriptAppointmentActivity");appointmentActivity->setToolTip(tr("대상 활동"));
  auto appointmentLocation=new QComboBox;appointmentLocation->setObjectName("scriptAppointmentLocation");appointmentLocation->setToolTip(tr("대상 장소"));
  for(auto v:context.value("documents").toArray()){const auto doc=v.toObject(),d=doc.value("data").toObject();if(doc.value("kind")=="activity")appointmentActivity->addItem(d.value("label").toString(d.value("id").toString()),d.value("id").toString());if(doc.value("kind")=="project")for(auto l:d.value("gameplay").toObject().value("locations").toArray()){auto location=l.toObject();appointmentLocation->addItem(location.value("name").toString(location.value("id").toString()),location.value("id").toString());}}
  grid->addWidget(appointmentActivity,5,0,1,2);grid->addWidget(appointmentLocation,5,2,1,2);
  for(auto pair:{qMakePair(appointmentActivity,QString("activityId")),qMakePair(appointmentLocation,QString("locationId"))})connect(pair.first,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,pair]{if(filling)return;auto d=data();const auto expected=pair.second=="activityId"?"after-activity":"location-enter";if(d.value("trigger")!=expected)return;d[pair.second]=pair.first->currentData().toString();setData(d);});
  auto references = new QPushButton(tr("선택 노드의 참조 위치…")); references->setObjectName("scriptReferences"); grid->addWidget(references, 1, 4);
  auto repeatable = new QCheckBox(tr("조건을 만족하면 방문할 때 다시 시작")); repeatable->setObjectName("scriptRepeatable");
  repeatable->setToolTip(tr("보류한 상담·처방을 다음 방문에 다시 열 수 있습니다. 완료 기록은 하나로 유지되며, 효과는 대본 조건으로 제한하세요."));
  grid->addWidget(repeatable, 3, 0, 1, 5);
  connect(repeatable, &QCheckBox::toggled, this, [this](bool checked) { if (!filling && data().contains("window")) { auto d=data(); if (checked) d["repeatable"]=true; else d.remove("repeatable"); setData(d); } });
  referenceSummary = new QLabel(tr("대사·표시 이름을 바꿀 때 ID를 바꿀 필요는 없습니다. ID 변경은 연결과 시나리오 참조를 함께 갱신합니다."));
  referenceSummary->setObjectName("scriptReferenceSummary"); referenceSummary->setWordWrap(true); grid->addWidget(referenceSummary, 2, 0, 1, 5);
  auto predecessors = new QLabel; predecessors->setObjectName("scriptPredecessorSummary"); predecessors->setWordWrap(true);
  grid->addWidget(predecessors, 4, 0, 1, 5);
  connect(name, &QLineEdit::textChanged, this, [this, name] {
    if (filling || documentIndex < 0) return;
    auto d = data(); d["label"] = name->text(); setData(d);
  });
  connect(priority, qOverload<int>(&QSpinBox::valueChanged), this, [this](int n) { if (!filling && documentIndex >= 0) { auto d = data(); d["priority"] = n; setData(d); } });
  const auto updateWindow = [this, from, through] {
    if (filling || documentIndex < 0 || !data().contains("window")) return;
    auto d = data(); auto w = d.value("window").toObject(); w["fromPeriod"] = from->value() - 1; w["throughPeriod"] = through->value() - 1; d["window"] = w; setData(d);
  };
  connect(from, qOverload<int>(&QSpinBox::valueChanged), this, updateWindow);
  connect(through, qOverload<int>(&QSpinBox::valueChanged), this, updateWindow);
  connect(trigger, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, trigger, repeatable,appointmentActivity,appointmentLocation] { if (!filling && data().contains("window")) { auto d = data(); d["trigger"] = trigger->currentData().toString(); const bool direct=trigger->currentData()=="interaction"; if (!direct) d.remove("repeatable"); d.remove("activityId");d.remove("locationId");const bool activity=trigger->currentData()=="after-activity",location=trigger->currentData()=="location-enter";if(activity)d["activityId"]=appointmentActivity->currentData().toString();if(location)d["locationId"]=appointmentLocation->currentData().toString();appointmentActivity->setVisible(activity);appointmentLocation->setVisible(location);setData(d); QSignalBlocker blocker(repeatable); repeatable->setEnabled(direct); repeatable->setChecked(d.value("repeatable").toBool()); } });
  connect(scope, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, scope, trigger] {
    if (filling || !data().contains("window")) return;
    auto d = data(); d["scope"] = scope->currentData().toString();
    if (scope->currentData() == "actor" && d.value("trigger").toString("period-end")=="period-end") d["trigger"] = "interaction";
    setData(d); QSignalBlocker blocker(trigger); trigger->setCurrentIndex(trigger->findData(d.value("trigger").toString("period-end"))); trigger->setEnabled(true);
  });
  connect(conditions, &QPushButton::clicked, this, [this] {
    if (documentIndex < 0) return;
    LogicEditor editor(data().value("requirements"), "condition", context, this);
    if (editor.exec() == QDialog::Accepted) { auto d = data(); d["requirements"] = editor.value(); setData(d); refreshMetadata(); }
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
  auto trigger = metadata->findChild<QComboBox *>("scriptTrigger"); trigger->setVisible(event); trigger->setCurrentIndex(trigger->findData(d.value("trigger").toString("period-end"))); trigger->setEnabled(true);
  for(auto pair:{qMakePair(QString("scriptAppointmentActivity"),QString("activityId")),qMakePair(QString("scriptAppointmentLocation"),QString("locationId"))}){auto combo=metadata->findChild<QComboBox*>(pair.first);combo->setVisible(event&&d.value("trigger")== (pair.second=="activityId"?"after-activity":"location-enter"));combo->setCurrentIndex(d.contains(pair.second)?combo->findData(d.value(pair.second).toString()):0);}
  auto repeatable = metadata->findChild<QCheckBox *>("scriptRepeatable"); repeatable->setVisible(event); repeatable->setEnabled(trigger->currentData()=="interaction"); repeatable->setChecked(d.value("repeatable").toBool());
  refreshPredecessorSummary();
  filling = previous;
}
void ScriptAuthoringDialog::refreshPredecessorSummary() {
  if (!metadata) return;
  const auto d = data();
  QStringList preceding;
  for (const auto &id : requiredEvents(d.value("requirements").toObject())) {
    QString name = id;
    for (auto v : documents) if (v.toObject().value("data").toObject().value("id") == id) name = v.toObject().value("data").toObject().value("label").toString(id);
    preceding << name;
  }
  preceding.sort();
  metadata->findChild<QLabel *>("scriptPredecessorSummary")->setText(preceding.isEmpty() ? tr("앞선 사건 연결 없음 · 발생 조건에서 활동과 날짜를 확인하세요.") : tr("먼저 완료할 사건: %1 → %2").arg(preceding.join(tr(" 및 ")), d.value("label").toString()));
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
    if (doc.toObject().value("kind") != "scenario") continue;
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
    if (doc.value("kind") != "scenario") continue;
    doc["data"] = scenarioReferences(doc.value("data"), {}, data().value("id").toString(), from, to, choiceFrom, choiceTo, &hits);
    count += hits.size(); externalDocuments[i] = doc;
  }
  referenceSummary->setText(tr("ID 변경: %1 → %2 · 대본 연결과 시나리오 참조 %3곳을 갱신했습니다. 저장하면 함께 적용되고, 취소하면 모두 취소됩니다.").arg(choiceFrom.isEmpty() ? from : choiceFrom, choiceFrom.isEmpty() ? to : choiceTo).arg(count));
}
