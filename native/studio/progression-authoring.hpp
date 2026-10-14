#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

struct ProgressionAuthoringPlan {
  QJsonArray edits;
  QJsonObject preview;
  QString error;
};
// Generate ordinary editable project documents, not a new runtime feature.
ProgressionAuthoringPlan makeProgressionPlan(const QJsonObject &view, const QJsonObject &settings);
