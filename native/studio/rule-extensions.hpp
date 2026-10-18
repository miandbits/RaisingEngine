#pragma once
#include <QDialog>
#include <QJsonObject>
#include <QJsonArray>
#include <QFormLayout>
#include <functional>

QJsonObject ruleProject(const QJsonObject &context);
QJsonArray ruleDefinitions(const QJsonObject &project, const QString &kind);
QJsonArray ruleSelectableActions(const QJsonObject &project, const QJsonArray &referenced = {});
QJsonArray ruleProgressionGroups(const QJsonObject &project);
QJsonObject defaultRuleCall(const QJsonObject &definition, const QString &op);
QWidget *ruleCallForm(QJsonObject call, QString kind, QJsonObject context, std::function<void(QJsonObject)> changed, QWidget *parent);
void ruleArgumentFields(QFormLayout *form, const QJsonObject &definition, const QJsonObject &args, const QJsonObject &context, std::function<void(QString,QJsonValue)> changed);
QString ruleReferenceName(const QJsonObject &context, const QString &type, const QString &id);

class RuleExtensionDialog : public QDialog {
public:
  RuleExtensionDialog(QJsonObject project, QJsonObject context, QString screenId, QWidget *parent=nullptr);
  QJsonObject value() const { return project; }
private:
  QJsonObject project;
};
