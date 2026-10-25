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
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QDialog>
#include <QLabel>
#include <QRegularExpression>
#include "table-logic.hpp"
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
  // Every translated CJK glyph, not just the launcher's short labels.
  QSet<uint> checked;
  for(const auto &resource:{":/editor/translations.json",":/editor/diagnostics.json"}) {
    QFile catalog(resource); if(!catalog.open(QIODevice::ReadOnly))return 12;
    for(const auto &value:QJsonDocument::fromJson(catalog.readAll()).array()) {
      const auto text=value.toArray()[locale=="ja"?2:locale=="en"?1:0].toString();
      for(uint glyph:text.toUcs4())if((glyph>=0x3000&&glyph<=0x9fff)||(glyph>=0xac00&&glyph<=0xd7af)) {
        if(!checked.contains(glyph)&&!metrics.inFontUcs4(glyph)){qCritical("Missing glyph U+%04x",glyph);return 13;}checked.insert(glyph);
      }
    }
  }
  const QString message=QString::fromUtf8("편집 내용이 갱신되었습니다. 다시 시도해 주세요.");
  if(locale!="ko"&&editorDiagnostic(message)==message)return 14;
  QJsonObject diagnostic{{"message",message},{"code","same-code"},{"pointer","/이름/%1"}};
  QJsonObject testView{{"documents",QJsonArray{QJsonObject{{"message",message},{"name",message}}}},{"diagnostics",QJsonArray{diagnostic}}};
  QJsonObject original{{"error",message},{"view",testView}};
  const auto localized=localizeEditorReply(original);
  if(localized["view"].toObject()["documents"]!=original["view"].toObject()["documents"])return 15;
  if(localized["view"].toObject()["diagnostics"].toArray()[0].toObject()["pointer"]!="/이름/%1")return 16;
  if(editorDiagnostic("unknown third-party diagnostic %1")!="unknown third-party diagnostic %1")return 17;
  const auto opaque=QString::fromUtf8("C:/사용자/%1/[test].json");
  const auto environmentWarning=QString::fromUtf8("저장된 플러그인 저장소를 사용할 수 없습니다: ")+opaque;
  const auto localizedEnvironment=localizeEditorReply(QJsonObject{{"environmentWarning",environmentWarning}}).value("environmentWarning").toString();
  if(locale!="ko"&&(localizedEnvironment==environmentWarning||!localizedEnvironment.contains(opaque))){qCritical("Plugin environment warning translation lost its path");return 30;}
  const auto diagnosticSource=QString::fromUtf8("읽는 동안 변경되었습니다: ")+opaque;
  const auto expectedDiagnostic=(locale=="ja"?QString::fromUtf8("読み込み中に変更されました："):locale=="en"?QString("Changed while reading: "):QString::fromUtf8("읽는 동안 변경되었습니다: "))+opaque;
  if(editorDiagnostic(diagnosticSource)!=expectedDiagnostic)return 22;
  const QList<QStringList> readableDiagnostics{
    {"UI_READING_HISTORY_REQUIRED","이 행동을 연결하기 전에 프로젝트 설정에서 최근 읽은 내용을 켜세요.","Enable recent reading in project settings before connecting this action.","この動作を接続する前に、プロジェクト設定で最近読んだ内容を有効にしてください。"},
    {"READABLE_GUIDANCE_COMPACT","계획 안내에는 간결한 단일 인물 일정이 필요합니다.","Planning guidance requires a compact single-character schedule.","計画ガイドには単一人物のコンパクトな予定が必要です。"},
    {"READABLE_PLAN_REUSE_COMPACT","계획 재사용에는 간결한 단일 인물 일정이 필요합니다.","Plan reuse requires a compact single-character schedule.","計画の再利用には単一人物のコンパクトな予定が必要です。"},
    {"READABLE_SCHEDULE_KIND","간결한 일정 표시는 일정 요소에만 적용할 수 있습니다.","Compact planning applies only to schedule elements.","コンパクトな予定表示は予定要素にのみ適用できます。"},
    {"READABLE_PRIMARY_GROUP","주요 동작을 지정하려면 하단 버튼 묶기가 필요합니다.","A primary action requires grouped bottom actions.","主要操作を指定するには下部ボタンをまとめる設定が必要です。"},
    {"READABLE_PRIMARY_REFERENCE","주요 동작은 이 화면의 하단 버튼 목록에 있는 버튼이어야 합니다.","The primary action must be a button listed in this screen’s bottom actions.","主要操作はこの画面の下部ボタン一覧にあるボタンである必要があります。"},
    {"READABLE_COMPACT_ROSTER","간결한 일정 표시는 현재 단일 인물 일정만 지원합니다. 명단 일정은 기본 표시를 사용하세요.","Compact planning currently supports single-character schedules. Use the standard presentation for roster schedules.","コンパクトな予定表示は現在、単一人物の予定のみに対応しています。名簿の予定には標準表示を使ってください。"}
  };
  for(const auto &row:readableDiagnostics){const auto reply=localizeEditorReply(QJsonObject{{"view",QJsonObject{{"diagnostics",QJsonArray{QJsonObject{{"code",row[0]},{"message",row[2]},{"pointer","/visual/screens/0/readableLayout"}}}}}}});const auto diagnostic=reply.value("view").toObject().value("diagnostics").toArray().first().toObject();if(diagnostic.value("message")!=row[locale=="ko"?1:locale=="en"?2:3]||diagnostic.value("code")!=row[0]||diagnostic.value("pointer")!="/visual/screens/0/readableLayout")return 31;}
  if(TableLogic::keyword("Dialogue")!="대사"||TableLogic::keyword("活動前")!="활동 전")return 18;
  if(TableLogic::keyword("結末")!="결말")return 23;
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
  window.openProject(QDir::currentPath()+"/examples/academy-adventure");
  QElapsedTimer waiting;waiting.start();
  while((window.busy()||window.currentView().value("root").toString().isEmpty())&&waiting.elapsed()<20000)QTest::qWait(50);
  if(window.currentView().value("root").toString().isEmpty()){qCritical()<<window.lastError;return 19;}
  // Exercise the actual Node service -> bridge -> translated editor response boundary.
  StudioBridge diagnosticBridge(QDir::currentPath());
  QTest::qWait(200);bool replyReceived=false;QString responseError;
  diagnosticBridge.request("release-i18n-probe",{},[&](QJsonObject reply){replyReceived=true;responseError=reply.value("error").toString();});
  waiting.restart();while(!replyReceived&&waiting.elapsed()<15000)QTest::qWait(25);
  const auto expectedService=locale=="en"?QString("Unknown request: release-i18n-probe"):locale=="ja"?QString::fromUtf8("不明な要求：release-i18n-probe"):QString::fromUtf8("알 수 없는 요청: release-i18n-probe");
  if(!replyReceived||responseError!=expectedService){std::fprintf(stderr,"Diagnostic boundary: %s\n",qPrintable(responseError));return 24;}
  QJsonArray inspections;
  for(const auto &name:{"editComponents","editDataUi","editBattleHud","editExpeditions","editTimeSessions","editRules","editRuleExtensions","goalRoutes","editLocalization"}) {
    auto action=window.findChild<QAction*>(name);if(!action||!action->isEnabled())return 20;
    bool captured=false;
    QTimer closeDialog;
    QObject::connect(&closeDialog,&QTimer::timeout,[&]{
      auto dialog=qobject_cast<QDialog*>(app.activeModalWidget());
      if(!dialog)for(auto candidate:window.findChildren<QDialog*>())if(candidate->isVisible()){dialog=candidate;break;}
      if(!dialog)return;
      app.processEvents(); QJsonArray clipped;
      for(auto label:dialog->findChildren<QLabel*>())if(label->isVisible()&&!label->wordWrap()&&!label->text().contains('<')) {
        int width=0;for(const auto &line:label->text().split('\n'))width=qMax(width,label->fontMetrics().horizontalAdvance(line));
        if(width>label->contentsRect().width()+4)clipped.append(QJsonObject{{"name",label->objectName()},{"text",label->text()},{"width",label->width()}});
      }
      inspections.append(QJsonObject{{"tool",name},{"width",dialog->width()},{"height",dialog->height()},{"clippedLabels",clipped}});
      if(!output.isEmpty())dialog->grab().save(output+"/"+name+"-"+locale+".png");
      captured=true;closeDialog.stop();dialog->reject();
    });
    closeDialog.start(250);action->trigger();
    waiting.restart();while(!captured&&waiting.elapsed()<10000)QTest::qWait(50);
    closeDialog.stop();
    if(!captured){std::fprintf(stderr,"Advanced tool did not open: %s\n",name);return 21;}
  }
  if(!output.isEmpty()){QFile report(output+"/layout-"+locale+".json");report.open(QIODevice::WriteOnly);report.write(QJsonDocument(inspections).toJson());}
  qInfo("Editor language %s: persistence, restart boundary, widgets and fonts passed", qPrintable(locale));
  return 0;
}
