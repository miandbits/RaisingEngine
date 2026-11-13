#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QSet>
#include <QStringList>

// Editor-only transformation of existing component/reading/localization fields.
namespace SharedSection {
struct Source {
  QString screenId, componentId;
  QJsonObject definition, instance;
  QSet<QString> selected;
  QStringList leaves;
  bool convert=false;
};
struct Target { QString screenId; bool appendReading=true; };
struct Review { QJsonObject project; QStringList lines; QString sourceInstanceId; int added=0, skipped=0; };
bool prepare(const QJsonObject& project,const QString& screenId,const QStringList& selected,Source& source,QString& error);
bool apply(const QJsonObject& project,const Source& source,const QString& label,const QList<Target>& targets,Review& review,QString& error);
}
