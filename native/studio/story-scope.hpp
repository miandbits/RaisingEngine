#pragma once
#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QStringList>

struct StoryScopePlan {
  QJsonArray documents, changes, options;
  QStringList errors, warnings;
  QStringList eventIds;
};
// Pure preview: no filesystem writes and no mutation of the caller's documents.
StoryScopePlan planPersonalStory(const QJsonArray &documents, const QString &file,
                                const QJsonObject &draft={}, const QMap<QString,bool> &owners={});

class StoryScopeDialog final : public QDialog {
public:
  StoryScopeDialog(const QJsonArray &documents,const QString &file,const QJsonObject &draft,QWidget *parent=nullptr);
  StoryScopePlan resultPlan;
};
