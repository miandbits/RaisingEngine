// Included by editor-language.cpp; service messages remain language-neutral on disk.
#include <QRegularExpression>
#include <algorithm>

namespace {
struct DiagnosticTranslation {
  QString source, translated;
  QRegularExpression pattern;
  int specificity = 0;
};
QList<DiagnosticTranslation> diagnosticCatalog(const QString &locale) {
  QFile file(":/editor/diagnostics.json");
  QList<DiagnosticTranslation> result;
  if (!file.open(QIODevice::ReadOnly)) return result;
  const QRegularExpression slot("%([1-9][0-9]*)");
  for (const auto &value : QJsonDocument::fromJson(file.readAll()).array()) {
    auto row=value.toArray();
    if(row.size()!=3) continue;
    DiagnosticTranslation item; item.source=row[0].toString();
    item.translated=row[locale=="ja"?2:1].toString();
    QString pattern="\\A"; qsizetype end=0;
    auto matches=slot.globalMatch(item.source);
    while(matches.hasNext()) {
      auto match=matches.next(); auto literal=item.source.mid(end,match.capturedStart()-end);
      item.specificity+=literal.size(); pattern+=QRegularExpression::escape(literal)+"(.*?)";
      end=match.capturedEnd();
    }
    const auto suffix=item.source.mid(end); item.specificity+=suffix.size();
    pattern+=QRegularExpression::escape(suffix)+"\\z";
    item.pattern=QRegularExpression(pattern,QRegularExpression::DotMatchesEverythingOption);
    result.append(item);
  }
  std::stable_sort(result.begin(),result.end(),[](const auto &a,const auto &b){return a.specificity>b.specificity;});
  return result;
}
void translateDiagnosticField(QJsonObject &object,const QString &key) {
  if(!object.value(key).isString()) return;
  const auto original=object.value(key).toString(), translated=editorDiagnostic(original);
  if(original!=translated) { object[key+"Original"]=original; object[key]=translated; }
}
}

QString editorDiagnostic(const QString &sourceMessage) {
  // These new shared validation messages originate in English. Normalize only exact
  // engine-owned messages; third-party diagnostics and opaque parameters stay intact.
  static const QHash<QString,QString> readableMessages{
    {"Configure current reminders in project settings before connecting this action.","이 행동을 연결하기 전에 프로젝트 설정에서 현재 알림을 구성하세요."},
    {"Choose an existing screen for current reminders.","현재 알림에 사용할 기존 화면을 선택하세요."},
    {"Select each reminder text only once.","각 알림 텍스트는 한 번만 선택하세요."},
    {"This reminder text no longer exists on the selected screen.","선택한 화면에서 이 알림 텍스트를 찾을 수 없습니다."},
    {"Current reminders can include text elements only.","현재 알림에는 텍스트 요소만 넣을 수 있습니다."},
    {"Choose static text or a supported current-state binding for this reminder.","이 알림에는 고정 텍스트나 지원되는 현재 상태 연결을 선택하세요."},
    {"Current reminders cannot use selected-row or data bindings.","현재 알림에는 선택한 행이나 데이터 연결을 사용할 수 없습니다."},

    {"Enable recent reading in project settings before connecting this action.","이 행동을 연결하기 전에 프로젝트 설정에서 최근 읽은 내용을 켜세요."},
    {"Planning guidance requires a compact single-character schedule.","계획 안내에는 간결한 단일 인물 일정이 필요합니다."},
    {"Plan reuse requires a compact single-character schedule.","계획 재사용에는 간결한 단일 인물 일정이 필요합니다."},
    {"Compact planning applies only to schedule elements.","간결한 일정 표시는 일정 요소에만 적용할 수 있습니다."},
    {"A primary action requires grouped bottom actions.","주요 동작을 지정하려면 하단 버튼 묶기가 필요합니다."},
    {"The primary action must be a button listed in this screen’s bottom actions.","주요 동작은 이 화면의 하단 버튼 목록에 있는 버튼이어야 합니다."},
    {"Compact planning currently supports single-character schedules. Use the standard presentation for roster schedules.","간결한 일정 표시는 현재 단일 인물 일정만 지원합니다. 명단 일정은 기본 표시를 사용하세요."}
  };
  const auto message=readableMessages.value(sourceMessage,sourceMessage);
  if(editorLanguage()=="ko" || message.size()>16384) return message;
  static QString loaded;
  static QList<DiagnosticTranslation> catalog;
  if(loaded!=editorLanguage()) {loaded=editorLanguage();catalog=diagnosticCatalog(loaded);}
  // Prefer exact messages before templates; parameters are opaque, including Korean IDs.
  for(const auto &entry:catalog) if(entry.source==message) return entry.translated;
  const QRegularExpression parameters("%([1-9][0-9]*)");
  for(const auto &entry:catalog) {
    if(!entry.source.contains(parameters) || entry.specificity<8) continue;
    auto match=entry.pattern.match(message); if(!match.hasMatch()) continue;
    QString output; qsizetype end=0; auto replacements=parameters.globalMatch(entry.translated);
    while(replacements.hasNext()) {
      auto replacement=replacements.next();
      output+=entry.translated.mid(end,replacement.capturedStart()-end);
      output+=match.captured(replacement.captured(1).toInt()); end=replacement.capturedEnd();
    }
    return output+entry.translated.mid(end);
  }
  return message; // Unknown third-party diagnostics stay intact for investigation.
}

QJsonObject localizeEditorReply(QJsonObject reply) {
  translateDiagnosticField(reply,"error"); translateDiagnosticField(reply,"notice");
  translateDiagnosticField(reply,"environmentWarning");
  auto view=reply.value("view").toObject();
  auto diagnostics=view.value("diagnostics").toArray();
  for(qsizetype i=0;i<diagnostics.size();++i) {
    auto diagnostic=diagnostics[i].toObject(); translateDiagnosticField(diagnostic,"message");
    diagnostics[i]=diagnostic;
  }
  if(view.contains("diagnostics")) {view["diagnostics"]=diagnostics;reply["view"]=view;}
  return reply;
}
