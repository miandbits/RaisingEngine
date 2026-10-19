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

QString editorDiagnostic(const QString &message) {
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
  auto view=reply.value("view").toObject();
  auto diagnostics=view.value("diagnostics").toArray();
  for(qsizetype i=0;i<diagnostics.size();++i) {
    auto diagnostic=diagnostics[i].toObject(); translateDiagnosticField(diagnostic,"message");
    diagnostics[i]=diagnostic;
  }
  if(view.contains("diagnostics")) {view["diagnostics"]=diagnostics;reply["view"]=view;}
  return reply;
}
