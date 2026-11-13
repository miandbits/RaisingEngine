#pragma once
#include <QString>
class QWidget;
void showSaveMigration(const QString& engineRoot, const QString& currentProject, QWidget* parent, const QString& previousProject = {}, const QString& sampleSave = {});
