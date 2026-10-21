#include "script-identity.hpp"
#include "script-continuation.hpp"
#include <QObject>
#include <QMap>
#include <QSet>
#include <functional>

QString scriptIdentity(const QJsonObject &document, const QJsonArray &documents,
                       const QJsonArray &contextDocuments) {
  const auto script = document.value("data").toObject();
  QMap<QString, QString> people, events;
  for (auto value : contextDocuments) {
    const auto doc = value.toObject(), data = doc.value("data").toObject();
    if (doc.value("kind") == "character") people[data.value("id").toString()] = data.value("name").toString();
  }
  for (auto value : documents) {
    const auto data = value.toObject().value("data").toObject();
    events[data.value("id").toString()] = data.value("label").toString();
  }
  QSet<QString> ids;
  std::function<void(QJsonValue)> visit = [&](QJsonValue value) {
    if (value.isArray()) { for (auto child : value.toArray()) visit(child); return; }
    if (!value.isObject()) return;
    const auto object = value.toObject();
    for (auto it = object.begin(); it != object.end(); ++it) {
      if (QStringList{"characterId", "speakerId", "actorId"}.contains(it.key()) && !it.value().toString().isEmpty()) ids.insert(it.value().toString());
      visit(it.value());
    }
  };
  visit(script);
  QStringList names;
  for (const auto &id : ids) names << people.value(id, id);
  names.removeDuplicates(); names.sort();
  QStringList preceding;
  for (const auto &id : requiredEvents(script.value("requirements").toObject())) preceding << events.value(id, id);
  preceding.removeDuplicates(); preceding.sort();
  const auto window = script.value("window").toObject();
  const auto period = window.isEmpty() ? QObject::tr("발생 조건에 따름") : QObject::tr("%1–%2번째 일정").arg(window.value("fromPeriod").toInt() + 1).arg(window.value("throughPeriod").toInt() + 1);
  return QObject::tr("인물: %1 · 시기: %2\n필수 선행: %3")
      .arg(names.isEmpty() ? QObject::tr("미지정") : names.join(", "), period,
           preceding.isEmpty() ? QObject::tr("없음") : preceding.join(", "));
}
