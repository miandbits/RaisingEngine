#pragma once
#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <functional>

struct StoryFlowPlan {
  QJsonArray edits;
  QJsonObject settings;
  QString error;
};

// Produces ordinary character/activity/event documents; runtime has no flow-specific rules.
StoryFlowPlan makeStoryFlowPlan(const QJsonObject &view, QJsonObject settings);
QJsonObject newStoryFlowSettings(const QJsonObject &view);

class StoryFlowDialog : public QDialog {
public:
  StoryFlowDialog(QJsonObject view, QWidget *parent,
      std::function<QString()> guard,
      std::function<void(QJsonArray, std::function<void(QString)>)> apply);
};
