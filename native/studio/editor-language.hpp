#pragma once
#include <QString>
class QApplication;
class QWidget;

// Editor preferences never change a project's content locale or saved game state.
QString editorLanguage();
QString savedEditorLanguage();
bool saveEditorLanguage(const QString &language);
bool installEditorLanguage(QApplication &app, const QString &requested = {});
void showEditorLanguageSettings(QWidget *parent);
QWidget *editorLanguagePicker(QWidget *parent);
