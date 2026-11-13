#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <functional>
class QWidget;

// The same widget authoring dialog serves screen elements and shared children.
// Results are returned only on acceptance; callers own the project edit/undo.
bool editWidgetConfiguration(QWidget* parent,const QJsonObject& project,const QJsonArray& documents,
                             const QJsonArray& assets,QJsonObject& element,QJsonObject& visualOptions,
                             const std::function<bool()>& current);
