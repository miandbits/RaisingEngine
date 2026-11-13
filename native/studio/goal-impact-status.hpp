#pragma once
#include <QJsonObject>
#include <QObject>
#include <QStringList>

inline QString goalImpactArchiveError(const QJsonObject &report){
  const auto detail=report.value("archiveError").toString();
  if(!detail.isEmpty())return detail;
  const auto error=report.value("error").toString();
  return error.startsWith("Could not save comparison report:")?error:QString();
}
inline bool goalImpactFatal(const QJsonObject &report){
  return !report.value("error").toString().isEmpty()||!goalImpactArchiveError(report).isEmpty()||(report.contains("archived")&&!report.value("archived").toBool());
}
inline QString goalImpactWarning(const QJsonObject &report){
  if(report.isEmpty())return {};
  QStringList lines;const auto archive=goalImpactArchiveError(report),error=report.value("error").toString();
  if(!archive.isEmpty()||(report.contains("archived")&&!report.value("archived").toBool())){
    lines<<QObject::tr("비교 보고서 저장 실패 · 승인과 호환성 확인에 사용할 수 없습니다.");
    if(!archive.isEmpty())lines<<archive;else if(!error.isEmpty())lines<<error;
  }else if(!error.isEmpty())lines<<QObject::tr("기준 비교 처리 오류 · 결과를 승인하거나 호환성 통과로 사용할 수 없습니다.")<<error;
  if(!report.value("complete").toBool())lines<<QObject::tr("기준 비교 미완료 · 호환성 통과가 아닙니다.");
  if(report.value("stale").toBool()&&!goalImpactFatal(report))lines<<QObject::tr("기준 비교가 현재 초안과 다릅니다. 이전 사본의 결과입니다.");
  return lines.join("\n");
}
