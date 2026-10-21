#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

// Only positive, mandatory dependencies can prove that a completion cycle blocks progress.
inline QSet<QString> requiredEvents(QJsonObject condition) {
  QSet<QString> ids;
  if (condition.value("op") == "eventCompleted") ids.insert(condition.value("eventId").toString());
  if (condition.value("op") == "all")
    for (auto child : condition.value("conditions").toArray()) ids.unite(requiredEvents(child.toObject()));
  return ids;
}
inline bool continuationCycle(const QJsonArray &documents, const QString &event, const QString &before) {
  QSet<QString> visited;
  QStringList pending{before};
  while (!pending.isEmpty()) {
    const auto id = pending.takeLast();
    if (id == event) return true;
    if (visited.contains(id)) continue;
    visited.insert(id);
    for (auto value : documents) {
      const auto data = value.toObject().value("data").toObject();
      if (data.value("id") == id)
        for (const auto &next : requiredEvents(data.value("requirements").toObject())) pending.append(next);
    }
  }
  return false;
}
inline QJsonObject afterEvent(QJsonObject requirements, const QString &id) {
  if (requiredEvents(requirements).contains(id)) return requirements;
  const QJsonObject dependency{{"op", "eventCompleted"}, {"eventId", id}};
  if (requirements.isEmpty() || requirements.value("op") == "always") return dependency;
  if (requirements.value("op") == "all") {
    auto children = requirements.value("conditions").toArray(); children.append(dependency);
    requirements["conditions"] = children; return requirements;
  }
  return {{"op", "all"}, {"conditions", QJsonArray{requirements, dependency}}};
}
