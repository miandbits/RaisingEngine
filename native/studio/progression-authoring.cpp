#include "studio.hpp"
#include "progression-authoring.hpp"
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>
#include <functional>
#include <limits>

namespace {
QJsonObject literal(int n) { return {{"op", "literal"}, {"value", n}}; }
QJsonObject attributeTarget(QString id) { return {{"kind", "attribute"}, {"attributeId", id}}; }
QJsonObject readAttribute(QString id) { return {{"op", "read"}, {"target", attributeTarget(id)}}; }
QJsonObject setAttribute(QString id, int n) { return {{"op", "set"}, {"target", attributeTarget(id)}, {"value", literal(n)}}; }
QJsonObject addAttribute(QString id, QJsonObject expression) { return {{"op", "add"}, {"target", attributeTarget(id)}, {"value", expression}}; }
QJsonObject action(QString id, QString name, QJsonArray effects, QJsonArray flow = {}) {
  QJsonObject result{{"id", id}, {"name", name}, {"costs", QJsonArray{}}, {"effects", effects}};
  if (!flow.empty()) result["flow"] = flow;
  return result;
}
bool integerIn(QJsonValue v, int low, int high) {
  const double n = v.toDouble(); return v.isDouble() && std::isfinite(n) && std::floor(n) == n && n >= low && n <= high;
}
}

ProgressionAuthoringPlan makeProgressionPlan(const QJsonObject &view, const QJsonObject &settings) {
  ProgressionAuthoringPlan plan;
  const auto fail = [&](QString message) { plan.error = message; return plan; };
  const auto id = settings.value("id").toString(), name = settings.value("name").toString();
  static const QRegularExpression idPattern("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$");
  if (!idPattern.match(id).hasMatch() || id.size() > 48) return fail(QObject::tr("활동 ID는 소문자·숫자·하이픈 1~48자여야 합니다."));
  if (name.trimmed().isEmpty()) return fail(QObject::tr("활동 이름을 입력하세요."));
  const int maxInt = std::numeric_limits<int>::max();
  if (!integerIn(settings.value("experienceGain"), 1, 1000000)) return fail(QObject::tr("숙련 증가는 1~1000000 정수입니다."));
  const int xpGain = settings.value("experienceGain").toInt();
  const auto tiers = settings.value("tiers").toArray();
  if (tiers.empty() || tiers.size() > 12) return fail(QObject::tr("단계는 1~12개입니다."));
  QJsonObject project, attributeDocument;
  QString projectFile, attributeFile;
  QSet<QString> used;
  std::function<void(QJsonValue)> scanIds = [&](QJsonValue value) {
    if (value.isArray()) for (auto child : value.toArray()) scanIds(child);
    if (value.isObject()) { const auto object = value.toObject();
      if (object.value("id").isString()) used.insert(object.value("id").toString());
      for (auto it = object.begin(); it != object.end(); ++it) scanIds(it.value());
    }
  };
  for (auto item : view.value("documents").toArray()) {
    const auto doc = item.toObject(); scanIds(doc.value("data"));
    if (doc.value("kind") == "project") { project = doc.value("data").toObject(); projectFile = doc.value("file").toString(); }
    if (doc.value("kind") == "attributes") { attributeDocument = doc.value("data").toObject(); attributeFile = doc.value("file").toString(); }
    if (doc.value("file") == "activities/" + id + ".json") return fail(QObject::tr("활동 파일이 이미 존재합니다: ") + id);
  }
  if (project.isEmpty() || attributeDocument.isEmpty()) return fail(QObject::tr("프로젝트와 능력 정의 문서가 필요합니다."));
  if (!project.value("gameplay").isObject()) return fail(QObject::tr("장소·행동 시스템이 설정된 프로젝트에서 사용하세요."));
  auto attributes = attributeDocument.value("attributes").toArray();
  const auto target = settings.value("targetAttributeId").toString(), currency = settings.value("currencyAttributeId").toString();
  QJsonObject targetDefinition, currencyDefinition;
  for (auto value : attributes) {
    const auto a = value.toObject(); if (a.value("id") == target) targetDefinition = a;
    if (a.value("id") == currency) currencyDefinition = a;
  }
  if (targetDefinition.empty() || currencyDefinition.empty()) return fail(QObject::tr("대상 능력과 비용 자원을 선택하세요."));
  const int currencyMin = currencyDefinition.value("min").toInt(), currencyMax = currencyDefinition.value("max").toInt();
  int previous = -1;
  for (int i = 0; i < tiers.size(); ++i) {
    const auto tier = tiers[i].toObject();
    if (!integerIn(tier.value("minimumExperience"), 0, maxInt - xpGain) ||
        !integerIn(tier.value("cost"), 0, maxInt) || !integerIn(tier.value("gain"), 0, maxInt) ||
        !integerIn(tier.value("chance"), 0, 10000)) return fail(QObject::tr("%1단계: 숙련·비용·상승량·확률의 정수 범위를 확인하세요.").arg(i + 1));
    const int xp = tier.value("minimumExperience").toInt();
    if ((i == 0 && xp != 0) || xp <= previous) return fail(QObject::tr("첫 숙련 기준은 0이고, 다음 단계 기준은 엄격히 증가해야 합니다."));
    if (qint64(currencyMin) + tier.value("cost").toInt() > currencyMax) return fail(QObject::tr("%1단계 비용은 자원의 사용 가능한 범위를 넘습니다.").arg(i + 1));
    const int gain = tier.value("gain").toInt();
    if (qint64(targetDefinition.value("max").toInt()) + gain > maxInt) return fail(QObject::tr("%1단계 상승량은 대상 능력의 최댓값과 합할 때 32비트 범위를 넘습니다.").arg(i + 1));
    previous = xp;
  }
  const QString xpId = id + "-xp", tierId = id + "-tier", feeId = id + "-fee", gainId = id + "-gain", chanceId = id + "-chance";
  QStringList generatedIds{id, xpId, tierId, feeId, gainId, chanceId, id + "-profile", id + "-profile-start", id + "-profile-end"};
  for (int i = 0; i < tiers.size(); ++i) { generatedIds.append(id + "-stage-" + QString::number(i + 1)); if (i > 0) generatedIds.append(id + "-select-" + QString::number(i)); }
  for (const auto &generated : generatedIds) if (used.contains(generated) || generated.size() > 64) return fail(QObject::tr("생성할 ID가 이미 사용 중입니다: ") + generated);
  QJsonArray generatedAttributes, generatedActions, generatedRules;
  auto addDefinition = [&](QString aid, QString label, int initial, int maximum) {
    generatedAttributes.append(QJsonObject{{"id", aid}, {"label", name + " · " + label}, {"kind", "stat"}, {"initial", initial}, {"min", 0}, {"max", maximum}});
  };
  const auto first = tiers.first().toObject();
  addDefinition(xpId, QObject::tr("숙련"), 0, maxInt - xpGain);
  addDefinition(tierId, QObject::tr("단계"), 1, tiers.size());
  addDefinition(feeId, QObject::tr("비용"), first.value("cost").toInt(), maxInt);
  addDefinition(gainId, QObject::tr("상승량"), first.value("gain").toInt(), maxInt);
  addDefinition(chanceId, QObject::tr("성공률"), first.value("chance").toInt(), 10000);
  for (int i = 0; i < tiers.size(); ++i) {
    const auto tier = tiers[i].toObject();
    generatedActions.append(action(id + "-stage-" + QString::number(i + 1), name + QObject::tr(" · %1단계 값").arg(i + 1),
      {setAttribute(tierId, i + 1), setAttribute(feeId, tier.value("cost").toInt()), setAttribute(gainId, tier.value("gain").toInt()), setAttribute(chanceId, tier.value("chance").toInt())}));
  }
  QString entry = id + "-stage-1";
  for (int i = 1; i < tiers.size(); ++i) {
    const auto branch = id + "-select-" + QString::number(i);
    generatedActions.append(action(branch, name + QObject::tr(" · 단계 분기 %1").arg(i), {},
      {QJsonObject{{"id", "select"}, {"condition", QJsonObject{{"op", "compare"}, {"comparator", "gte"}, {"left", readAttribute(xpId)}, {"right", literal(tiers[i].toObject().value("minimumExperience").toInt())}}},
                    {"thenActionId", id + "-stage-" + QString::number(i + 1)}, {"elseActionId", entry}}}));
    entry = branch;
  }
  generatedActions.append(action(id + "-profile", name + QObject::tr(" · 단계 갱신"), {},
    {QJsonObject{{"id", "refresh"}, {"condition", QJsonObject{{"op", "always"}}}, {"thenActionId", entry}}}));
  for (auto spec : QList<QPair<QString, QString>>{{"start", "periodStart"}, {"end", "periodEnd"}})
    generatedRules.append(QJsonObject{{"id", id + "-profile-" + spec.first}, {"name", name + QObject::tr(" · 단계 갱신 ") + spec.first}, {"trigger", QJsonObject{{"type", spec.second}}}, {"actionId", id + "-profile"}});
  QJsonObject requiredMoney = readAttribute(feeId);
  if (currencyMin != 0) requiredMoney = {{"op", "add"}, {"left", literal(currencyMin)}, {"right", readAttribute(feeId)}};
  QJsonObject activity{{"formatVersion", "0.1.0"}, {"id", id}, {"label", name},
    {"previewAttributes", QJsonArray{QJsonObject{{"attributeId", tierId}, {"label", QObject::tr("단계")}}, QJsonObject{{"attributeId", feeId}, {"label", QObject::tr("비용")}}}},
    {"description", QObject::tr("일정 칸당 1회 판정. 단계별 비용은 기본 효과로 지불하며 실패해도 지불합니다. 성공 시 숙련과 대상 능력이 증가합니다.")},
    {"requirements", QJsonObject{{"op", "compare"}, {"comparator", "gte"}, {"left", readAttribute(currency)}, {"right", requiredMoney}}},
    {"costs", QJsonArray{}}, {"baseEffects", QJsonArray{addAttribute(currency, {{"op", "subtract"}, {"left", literal(0)}, {"right", readAttribute(feeId)}})}},
    {"successCheck", QJsonObject{{"chance", readAttribute(chanceId)}}},
    {"successEffects", QJsonArray{addAttribute(target, readAttribute(gainId)), addAttribute(xpId, literal(xpGain))}}, {"failureEffects", QJsonArray{}}};
  for (auto value : generatedAttributes) attributes.append(value);
  attributeDocument["attributes"] = attributes;
  auto gameplay = project.value("gameplay").toObject();
  auto actions = gameplay.value("actions").toArray(), rules = gameplay.value("rules").toArray();
  for (auto value : generatedActions) actions.append(value); for (auto value : generatedRules) rules.append(value);
  gameplay["actions"] = actions; gameplay["rules"] = rules; project["gameplay"] = gameplay;
  const auto scheduleLink = settings.value("scheduleLink").toObject();
  if (!scheduleLink.isEmpty()) {
    auto visual = project.value("visual").toObject(); auto screens = visual.value("screens").toArray(); bool linked = false;
    for (int s = 0; s < screens.size(); ++s) { auto screen = screens[s].toObject(); if (screen.value("id") != scheduleLink.value("screenId")) continue;
      auto elements = screen.value("elements").toArray();
      for (int e = 0; e < elements.size(); ++e) { auto element = elements[e].toObject(); if (element.value("id") != scheduleLink.value("elementId") || element.value("kind") != "schedule") continue;
        auto groups = element.value("scheduleGroups").toArray();
        for (int g = 0; g < groups.size(); ++g) { auto group = groups[g].toObject(); if (group.value("id") != scheduleLink.value("groupId")) continue;
          auto activities = group.value("activityIds").toArray(); activities.append(id); group["activityIds"] = activities; groups[g] = group; linked = true;
        }
        element["scheduleGroups"] = groups; elements[e] = element;
      }
      screen["elements"] = elements; screens[s] = screen;
    }
    if (!linked) return fail(QObject::tr("연결할 일정 분류를 다시 선택하세요."));
    visual["screens"] = screens; project["visual"] = visual;
  }
  plan.edits = {QJsonObject{{"type", "replace"}, {"file", attributeFile}, {"data", attributeDocument}},
                QJsonObject{{"type", "replace"}, {"file", projectFile}, {"data", project}},
                QJsonObject{{"type", "add"}, {"kind", "activity"}, {"id", id}},
                QJsonObject{{"type", "replace"}, {"file", "activities/" + id + ".json"}, {"data", activity}}};
  plan.preview = {{"activity", activity}, {"attributes", generatedAttributes}, {"actions", generatedActions}, {"rules", generatedRules}};
  if (!scheduleLink.isEmpty()) plan.preview["scheduleLink"] = scheduleLink;
  return plan;
}

void StudioWindow::createProgressionActivity() {
  if (bridge.busy() || projectData().isEmpty()) return;
  const auto baseline = view;
  const auto revision = view.value("revision"), root = view.value("root");
  QDialog dialog(this); dialog.setObjectName("progressionAuthoring"); dialog.setWindowTitle(tr("숙련 단계 활동 만들기")); dialog.resize(1050, 850);
  auto layout = new QVBoxLayout(&dialog); auto form = new QFormLayout; layout->addLayout(form);
  auto id = new QLineEdit; id->setObjectName("progressionId"); id->setMaxLength(48); form->addRow(tr("새 활동 ID"), id);
  auto name = new QLineEdit; name->setObjectName("progressionName"); form->addRow(tr("이름"), name);
  auto target = new QComboBox; target->setObjectName("progressionTarget"); auto currency = new QComboBox; currency->setObjectName("progressionCurrency");
  for (auto doc : baseline.value("documents").toArray()) if (doc.toObject().value("kind") == "attributes") for (auto value : doc.toObject().value("data").toObject().value("attributes").toArray()) {
    const auto a = value.toObject(); target->addItem(a.value("label").toString() + " · " + a.value("id").toString(), a.value("id").toString()); currency->addItem(a.value("label").toString() + " · " + a.value("id").toString(), a.value("id").toString());
    if (a.value("kind") == "resource") currency->setCurrentIndex(currency->count() - 1);
  }
  form->addRow(tr("상승할 능력"), target); form->addRow(tr("비용 자원"), currency);
  auto schedule = new QComboBox; schedule->setObjectName("progressionScheduleLink"); schedule->addItem(tr("분류 연결 안 함 · 이후 위젯 구성에서 연결"), QVariant::fromValue(QJsonObject{}));
  for (auto screenValue : projectData().value("visual").toObject().value("screens").toArray()) {
    const auto screen = screenValue.toObject();
    for (auto elementValue : screen.value("elements").toArray()) { const auto element = elementValue.toObject(); if (element.value("kind") != "schedule") continue;
      for (auto groupValue : element.value("scheduleGroups").toArray()) { const auto group = groupValue.toObject();
        schedule->addItem(screen.value("label").toString() + " / " + element.value("label").toString(element.value("id").toString()) + " / " + group.value("label").toString(),
          QVariant::fromValue(QJsonObject{{"screenId", screen.value("id")}, {"elementId", element.value("id")}, {"groupId", group.value("id")}}));
      }
    }
  }
  form->addRow(tr("일정 선택 메뉴 연결"), schedule);
  auto xp = new QSpinBox; xp->setObjectName("progressionExperienceGain"); xp->setRange(1, 1000000); xp->setValue(1); form->addRow(tr("성공 시 숙련 증가"), xp);
  auto help = new QLabel(tr("기존 능력·행동·규칙·활동 문서를 생성합니다. 일정 칸당 1회 판정이며 일별 독립 판정이 아닙니다. 실패해도 비용을 지불하고, 성공할 때만 숙련과 능력이 오릅니다. 첫 단계 숙련 기준은 0입니다. 확률 10000 = 100%. 생성한 값은 이후 일반 편집기로 수정할 수 있습니다.")); help->setWordWrap(true); layout->addWidget(help);
  auto tiers = new QTableWidget(1, 4); tiers->setObjectName("progressionTiers"); tiers->setHorizontalHeaderLabels({tr("최소 숙련"), tr("비용"), tr("능력 상승량"), tr("성공 확률 / 10000")}); tiers->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  for (int c = 0; c < 4; ++c) tiers->setItem(0, c, new QTableWidgetItem(QString::number(c == 3 ? 10000 : c == 2 ? 1 : 0))); layout->addWidget(tiers);
  auto row = new QHBoxLayout; auto add = new QPushButton(tr("＋ 단계 추가")); add->setObjectName("progressionAddTier"); auto remove = new QPushButton(tr("선택 단계 삭제")); remove->setObjectName("progressionRemoveTier"); row->addWidget(add); row->addWidget(remove); layout->addLayout(row);
  connect(add, &QPushButton::clicked, &dialog, [=] { if (tiers->rowCount() >= 12) return; const int r = tiers->rowCount(); tiers->insertRow(r); for (int c = 0; c < 4; ++c) tiers->setItem(r, c, new QTableWidgetItem(c == 0 ? QString::number(r * 10) : tiers->item(r - 1, c)->text())); });
  connect(remove, &QPushButton::clicked, &dialog, [=] { if (tiers->rowCount() > 1 && tiers->currentRow() >= 0) tiers->removeRow(tiers->currentRow()); });
  auto previewButton = new QPushButton(tr("생성 문서 미리보기")); previewButton->setObjectName("progressionPreview"); layout->addWidget(previewButton);
  auto preview = new QPlainTextEdit; preview->setObjectName("progressionPreviewText"); preview->setReadOnly(true); layout->addWidget(preview, 1);
  auto error = new QLabel; error->setObjectName("progressionError"); error->setWordWrap(true); error->setStyleSheet("color:#ffb2ae"); layout->addWidget(error);
  auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel); layout->addWidget(buttons);
  auto collect = [&] {
    QJsonArray rows; const QStringList keys{"minimumExperience", "cost", "gain", "chance"};
    for (int r = 0; r < tiers->rowCount(); ++r) { QJsonObject v; for (int c = 0; c < 4; ++c) { bool ok; const double n = tiers->item(r, c)->text().toDouble(&ok); v[keys[c]] = ok ? QJsonValue(n) : QJsonValue(); } rows.append(v); }
    return makeProgressionPlan(baseline, {{"id", id->text()}, {"name", name->text()}, {"targetAttributeId", target->currentData().toString()}, {"currencyAttributeId", currency->currentData().toString()}, {"experienceGain", xp->value()}, {"tiers", rows}, {"scheduleLink", schedule->currentData().toJsonObject()}});
  };
  connect(previewButton, &QPushButton::clicked, &dialog, [&] { auto plan = collect(); error->setText(plan.error); preview->setPlainText(plan.error.isEmpty() ? QString::fromUtf8(QJsonDocument(plan.preview).toJson()) : QString()); });
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
    if (bridge.busy()) { error->setText(tr("진행 중인 요청이 끝난 뒤 다시 저장하세요.")); return; }
    if (view.value("revision") != revision || view.value("root") != root) { error->setText(tr("프로젝트가 변경되었습니다. 초안을 확인하고 창을 다시 여세요.")); return; }
    auto plan = collect(); error->setText(plan.error); if (!plan.error.isEmpty()) return;
    preview->setPlainText(QString::fromUtf8(QJsonDocument(plan.preview).toJson())); buttons->setEnabled(false);
    QPointer<QDialog> dialogGuard(&dialog); QPointer<QLabel> errorGuard(error); QPointer<QDialogButtonBox> buttonsGuard(buttons);
    request("edit", {{"edit", QJsonObject{{"type", "batch"}, {"edits", plan.edits}}}}, [dialogGuard, errorGuard, buttonsGuard](QJsonObject reply) {
      if (!dialogGuard) return;
      buttonsGuard->setEnabled(true); if (!reply.value("error").toString().isEmpty()) { errorGuard->setText(reply.value("error").toString()); return; } dialogGuard->accept();
    });
  });
  dialog.exec();
}
