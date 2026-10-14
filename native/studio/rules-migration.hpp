#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QSet>
#include <QString>
#include <functional>

// Pure draft migration: no filesystem writes, no run/save mutation.
// Definitions that cannot preserve their semantics return the original draft.
inline QJsonObject migrateFacilityRules(QJsonObject project, QString *error) {
  if (error) error->clear();
  const auto original = project;
  auto gameplay = project.value("gameplay").toObject();
  const auto diets = gameplay.value("diets").toArray();
  const auto services = gameplay.value("services").toArray();
  if (diets.isEmpty() && services.isEmpty()) return project;
  QString failure;
  QString dietLocation;
  int dietInterval = 1;
  for (int i = 0; i < diets.size(); ++i) {
    const auto diet = diets[i].toObject();
    const auto location = diet.value("locationId").toString();
    const int interval = diet.value("intervalPeriods").toInt(1);
    if (i == 0) { dietLocation = location; dietInterval = interval; }
    else if (location != dietLocation || interval != dietInterval) {
      if (error) *error = QStringLiteral("식단의 설정 장소와 적용 주기가 서로 다릅니다. 의미를 유지하려면 동일한 장소·주기로 정리한 뒤 변환하세요.");
      return original;
    }
  }
  auto actions = gameplay.value("actions").toArray();
  auto groups = gameplay.value("policyGroups").toArray();
  QSet<QString> actionIds, groupIds;
  for (const auto &value : actions) actionIds.insert(value.toObject().value("id").toString());
  for (const auto &value : groups) groupIds.insert(value.toObject().value("id").toString());
  auto uniqueId = [](QString base, QSet<QString> &ids) {
    base = base.left(64);
    QString candidate = base;
    int suffix = 2;
    while (ids.contains(candidate)) {
      const auto end = QStringLiteral("-%1").arg(suffix++);
      candidate = base.left(64 - end.size()) + end;
    }
    ids.insert(candidate);
    return candidate;
  };
  QMap<QString, QString> serviceActions;
  QSet<QString> dietOptions;
  QString dietGroup;
  QJsonArray options;
  for (const auto &value : services) {
    const auto old = value.toObject();
    auto action = old;
    const auto oldId = old.value("id").toString();
    const auto id = uniqueId(QStringLiteral("service-") + oldId, actionIds);
    action["id"] = id;
    action["locationIds"] = QJsonArray{old.value("locationId")};
    action.remove("locationId");
    action["rejectNoEffect"] = true;
    actions.append(action);
    serviceActions.insert(oldId, id);
  }
  if (!diets.isEmpty()) {
    dietGroup = uniqueId(QStringLiteral("daily-policy"), groupIds);
    for (const auto &value : diets) {
      const auto old = value.toObject();
      auto action = old;
      const auto oldId = old.value("id").toString();
      const auto id = uniqueId(QStringLiteral("policy-") + oldId, actionIds);
      action["id"] = id;
      action.remove("locationId");
      action.remove("intervalPeriods");
      actions.append(action);
      options.append(QJsonObject{{"id", oldId}, {"name", old.value("name")}, {"actionId", id}});
      dietOptions.insert(oldId);
    }
    groups.append(QJsonObject{{"id", dietGroup}, {"name", QStringLiteral("생활 정책")},
      {"locationIds", QJsonArray{dietLocation}}, {"trigger", QJsonObject{{"type", "beforeActivity"}, {"intervalPeriods", dietInterval}}}, {"options", options}});
  }
  std::function<QJsonValue(QJsonValue, bool)> rewrite = [&](QJsonValue value, bool overrideValues) -> QJsonValue {
    if (value.isArray()) {
      QJsonArray result;
      for (const auto &child : value.toArray()) result.append(rewrite(child, false));
      return result;
    }
    if (!value.isObject()) return value;
    auto object = value.toObject();
    const auto kind = object.value("kind").toString();
    const auto action = object.value("action").toString();
    if (kind == "services") {
      // An empty filter means all actions in the generic widget. Reject this
      // conversion instead of accidentally displaying unrelated action recipes.
      if (services.isEmpty()) failure = QStringLiteral("빈 서비스 표시 요소는 다른 행동을 표시할 수 있으므로 먼저 삭제하거나 서비스를 추가하세요.");
      object["kind"] = "actions";
      QJsonArray filter;
      for (const auto &entry : services) filter.append(serviceActions.value(entry.toObject().value("id").toString()));
      object["actionIds"] = filter;
    }
    if (kind == "diets") {
      if (dietGroup.isEmpty()) failure = QStringLiteral("식단 표시 요소에 연결할 기존 식단이 없습니다.");
      object["kind"] = "policies";
      object["groupIds"] = QJsonArray{dietGroup};
    }
    if (action == "useService") object["action"] = "executeAction";
    if (object.contains("serviceId")) {
      const auto oldId = object.value("serviceId").toString();
      if (!serviceActions.contains(oldId)) failure = QStringLiteral("연결된 기존 서비스 '%1'을 찾을 수 없습니다.").arg(oldId);
      object["actionId"] = serviceActions.value(oldId);
      object.remove("serviceId");
    }
    if (action == "setDiet" || object.contains("dietId")) {
      if (dietGroup.isEmpty()) failure = QStringLiteral("정책으로 변환할 기존 식단이 없습니다.");
      object["groupId"] = dietGroup;
      if (object.contains("dietId")) {
        const auto oldId = object.value("dietId");
        if (!oldId.isNull() && !dietOptions.contains(oldId.toString())) failure = QStringLiteral("연결된 기존 식단 '%1'을 찾을 수 없습니다.").arg(oldId.toString());
        object["optionId"] = oldId;
      } else if (!overrideValues) object["optionId"] = QJsonValue::Null;
      object.remove("dietId");
      if (action == "setDiet") object["action"] = "selectPolicy";
    }
    for (const auto &key : object.keys()) object[key] = rewrite(object.value(key), key == "values");
    return object;
  };
  if (project.contains("visual")) project["visual"] = rewrite(project.value("visual"), false);
  if (!failure.isEmpty()) { if (error) *error = failure; return original; }
  gameplay["actions"] = actions;
  if (!diets.isEmpty()) gameplay["policyGroups"] = groups;
  gameplay.remove("diets");
  gameplay.remove("services");
  project["gameplay"] = gameplay;
  return project;
}
