#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
struct RaisingScreenFlowOptions {
  QString backgroundId, portraitId, language="en";
  QJsonArray attributeIds;
  bool neutralReadingFrame=false;
};
struct RaisingScreenFlowPlan {
  QJsonObject project;
  QJsonArray screens;
  QString error, homeId, plannerId, growthId, statusId;
};
RaisingScreenFlowPlan planRaisingScreenFlow(const QJsonObject& project,const RaisingScreenFlowOptions& options);
