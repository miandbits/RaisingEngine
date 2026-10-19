#pragma once
#include <QString>
#include <QJsonObject>
class QApplication;
class QWidget;

// Editor preferences never change a project's content locale or saved game state.
QString editorLanguage();
QString savedEditorLanguage();
bool saveEditorLanguage(const QString &language);
bool installEditorLanguage(QApplication &app, const QString &requested = {});
void showEditorLanguageSettings(QWidget *parent);
QWidget *editorLanguagePicker(QWidget *parent);
// Display boundary only: never translate project documents or game state.
QString editorDiagnostic(const QString &message);
QJsonObject localizeEditorReply(QJsonObject reply);
