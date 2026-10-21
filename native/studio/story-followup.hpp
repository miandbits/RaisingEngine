#pragma once
#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>

struct StoryFollowupResult {
  QJsonArray documents;
  QString file, error;
};
// Detached plan: cancel/failure never changes the caller's draft.
StoryFollowupResult makeStoryFollowup(const QJsonArray &documents, const QString &source,
                                     const QJsonObject &settings);
QJsonArray storyFollowupPromises(const QJsonArray &documents, const QString &source);
class StoryFollowupDialog final : public QDialog {
public:
  StoryFollowupDialog(QJsonArray documents, QString source, QWidget *parent);
  StoryFollowupResult resultPlan;
};
