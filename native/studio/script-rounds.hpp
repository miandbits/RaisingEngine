#pragma once
#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>

// Compiles authoring data to existing narrative nodes; no new runtime contract.
QJsonObject appendScriptRounds(QJsonObject script, const QJsonObject &settings, const QString &afterNodeId = {});

class ScriptRoundsDialog final : public QDialog {
public:
  ScriptRoundsDialog(QJsonObject context, QJsonObject script, QString selectedNodeId, QWidget *parent = nullptr);
  QJsonObject resultScript() const { return result; }
private:
  QJsonObject result;
};
