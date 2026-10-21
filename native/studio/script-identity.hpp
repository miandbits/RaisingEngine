#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

// Derived editor labels only; never persisted in content or used as identifiers.
QString scriptIdentity(const QJsonObject &document, const QJsonArray &documents,
                       const QJsonArray &contextDocuments);
