#include "editor-language.hpp"
#include "project-manager.hpp"
#include "studio.hpp"
#include "theme.hpp"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFontMetrics>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <cstdio>

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  QTemporaryDir temp;
  if(!temp.isValid()){std::fprintf(stderr,"Cannot create isolated settings directory\n");return 10;}
  qputenv("RAISING_EDITOR_SETTINGS", (temp.path()+"/editor.ini").toUtf8());
  qputenv("RAISING_PROJECT_HISTORY", (temp.path()+"/history.ini").toUtf8());
  const QString locale = argc > 1 ? argv[1] : "en";
  if (!saveEditorLanguage(locale) || savedEditorLanguage()!=locale || saveEditorLanguage("invalid")) {std::fprintf(stderr,"Language preference check failed: requested=%s saved=%s path=%s\n",qPrintable(locale),qPrintable(savedEditorLanguage()),qPrintable(temp.path()));return 1;}
  applyStudioTheme(app);
  if (!installEditorLanguage(app) || editorLanguage()!=locale) return 2;
  // Qt standard buttons, original-source fallback, format placeholders and CJK glyphs.
  if (QObject::tr("untranslated sentinel")!="untranslated sentinel") return 3;
  QFontMetrics metrics(app.font());
  if (!metrics.inFont(locale=="ja" ? QChar(0x8a00) : QChar(0xd55c))) {std::fprintf(stderr,"CJK glyph missing: %s\n",qPrintable(app.font().families().join(",")));return 4;}
  if(locale=="ja")for(auto glyph:QString::fromUtf8("既存選択一覧保存環境"))if(!metrics.inFont(glyph))return 11;
  ProjectManager manager(QDir::currentPath()); manager.show(); app.processEvents();
  auto create=manager.findChild<QPushButton*>("launcherCreate");
  const QString expected=locale=="ja"?"プロジェクトを作成":locale=="en"?"Create project":"프로젝트 만들기";
  if (!create || create->text()!=expected) return 5;
  auto picker=manager.findChild<QComboBox*>("editorLanguage");
  if (!picker || picker->currentData()!=locale) return 6;
  const auto next=locale=="ja"?"en":"ja";
  picker->setCurrentIndex(picker->findData(next));
  QMetaObject::invokeMethod(picker,"activated",Qt::DirectConnection,Q_ARG(int,picker->currentIndex()));
  if(savedEditorLanguage()!=next || editorLanguage()!=locale || create->text()!=expected) return 7;
  StudioWindow window(QDir::currentPath()); window.show(); app.processEvents();
  const auto exportAction=window.findChild<QAction*>("exportGame");
  const QString exportLabel=locale=="ja"?"ゲームを書き出す…":locale=="en"?"Export game…":"게임 내보내기…";
  if(!exportAction || exportAction->text()!=exportLabel) return 8;
  auto output=qEnvironmentVariable("RAISING_LANGUAGE_EVIDENCE");
  if(!output.isEmpty()) { QDir().mkpath(output); manager.grab().save(output+"/launcher-"+locale+".png"); window.grab().save(output+"/editor-"+locale+".png"); }
  qInfo("Editor language %s: persistence, restart boundary, widgets and fonts passed", qPrintable(locale));
  return 0;
}
