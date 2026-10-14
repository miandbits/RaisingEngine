#include "studio.hpp"
#include <QDateTime>
#include <QDockWidget>

void StudioWindow::runValidation(){
  auto dock=findChild<QDockWidget*>("projectDock");dock->show();dock->raise();bottom->setCurrentWidget(validationPanel);
  if(bridge.busy()){validationSummary->setText(tr("이전 작업 처리 중입니다. 완료 후 검증을 다시 눌러 주세요."));return;}
  if(projectData().empty()){validationSummary->setText(tr("검증할 프로젝트를 먼저 열어 주세요."));return;}
  validationSummary->setText(tr("검증 중… 문서·참조·수치·화면 연결을 검사합니다."));
  const auto began=QDateTime::currentMSecsSinceEpoch();
  request("validate",{},[this,began](QJsonObject reply){
    if(!reply.value("error").toString().isEmpty()){validationSummary->setText(tr("검증 실행 실패: ")+reply.value("error").toString());return;}
    int errors=0,warnings=0;for(auto value:view.value("diagnostics").toArray()){const auto severity=value.toObject().value("severity").toString();if(severity=="error")++errors;else if(severity=="warning")++warnings;}
    validatedRevision=view.value("revision").toInt();
    validationSummary->setText(tr("검증 완료 · 오류 %1건 · 경고 %2건 · r%3 · %4 ms\n%5").arg(errors).arg(warnings).arg(validatedRevision).arg(QDateTime::currentMSecsSinceEpoch()-began).arg(errors?tr("항목을 클릭하면 해당 화면과 요소로 이동합니다."):tr("검사한 규칙에서 오류가 발견되지 않았습니다. 실제 도달 경로는 자동 플레이에서 확인하세요.")));
    log->appendPlainText(validationSummary->text());bottom->setCurrentWidget(validationPanel);
  });
}
