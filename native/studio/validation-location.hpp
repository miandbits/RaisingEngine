#pragma once
#include <QJsonObject>
#include <QStringList>
#include <QDialog>
#include <functional>

namespace ValidationLocation {
bool tokens(const QString &pointer, QStringList &parts);
QJsonValue value(const QJsonValue &root, const QString &pointer);
bool replace(QJsonValue &root, const QString &pointer, const QJsonValue &value, bool remove = false);
using Apply = std::function<void(QJsonObject, std::function<void(QString)>)>;
QDialog *editor(QWidget *parent, QJsonObject document, QJsonArray documents,
                QJsonObject diagnostic, Apply apply);
}
