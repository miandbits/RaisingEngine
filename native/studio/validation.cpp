#include "studio.hpp"
#include <QDateTime>
#include <QDockWidget>
#include "validation-location.hpp"
#include <QApplication>
#include <QPointer>

void StudioWindow::runValidation(){
  if(auto focus=QApplication::focusWidget())focus->clearFocus();
  if(!waitForPendingEdits())return;
  validateForRepair({});
}
void StudioWindow::validateForRepair(std::function<void(QString)> done){
  auto dock=findChild<QDockWidget*>("projectDock");dock->show();dock->raise();bottom->setCurrentWidget(validationPanel);
  if(bridge.busy()){validationSummary->setText(tr("이전 작업 처리 중입니다. 완료 후 검증을 다시 눌러 주세요."));return;}
  if(projectData().empty()){validationSummary->setText(tr("검증할 프로젝트를 먼저 열어 주세요."));return;}
  validationSummary->setText(tr("검증 중… 문서·참조·수치·화면 연결을 검사합니다."));
  const auto began=QDateTime::currentMSecsSinceEpoch();
  request("validate",{},[this,began,done](QJsonObject reply){
    if(!reply.value("error").toString().isEmpty()){validationSummary->setText(tr("검증 실행 실패: ")+reply.value("error").toString());if(done)done(validationSummary->text());return;}
    int errors=0,warnings=0;for(auto value:view.value("diagnostics").toArray()){const auto severity=value.toObject().value("severity").toString();if(severity=="error")++errors;else if(severity=="warning")++warnings;}
    validatedRevision=view.value("revision").toInt();
    validationSummary->setText(tr("검증 완료 · 오류 %1건 · 경고 %2건 · r%3 · %4 ms\n%5").arg(errors).arg(warnings).arg(validatedRevision).arg(QDateTime::currentMSecsSinceEpoch()-began).arg(errors?tr("항목을 클릭하면 해당 화면과 요소로 이동합니다."):tr("검사한 규칙에서 오류가 발견되지 않았습니다. 실제 도달 경로는 자동 플레이에서 확인하세요.")));
    log->appendPlainText(validationSummary->text());bottom->setCurrentWidget(validationPanel);
    if(done)done(validationSummary->text());
  });
}

void StudioWindow::openValidationLocation(QJsonObject diagnostic,int revision,QString root){
  if(findChild<QDialog*>("validationLocationEditor"))return;
  if(auto focus=QApplication::focusWidget())focus->clearFocus();
  if(!waitForPendingEdits())return;
  if(root!=view["root"].toString()||revision!=view["revision"].toInt()){
    validateForRepair([this](QString){validationSummary->setText(validationSummary->text()+"\n"+tr("편집 내용이 바뀌어 오류 목록을 갱신했습니다. 새 목록에서 다시 선택하세요."));});return;
  }
  QJsonObject document;for(auto d:view["documents"].toArray())if(d.toObject()["file"]==diagnostic["file"])document=d.toObject();
  if(document.isEmpty()||document["kind"]=="extension"||document["kind"]=="pluginLock"){
    validationSummary->setText(diagnostic["file"].toString()+diagnostic["pointer"].toString()+"\n"+diagnostic["message"].toString()+"\n"+tr("이 위치는 일반 필드 편집 대상이 아닙니다. 파일 또는 플러그인 진단에서 수정한 뒤 다시 검증하세요."));return;
  }
  if(document["kind"]=="project"&&diagnostic.value("code")=="UI_READING_HISTORY_REQUIRED"){editProjectSettings(true);return;}
  QStringList parts;if(!ValidationLocation::tokens(diagnostic["pointer"].toString(),parts)){validationSummary->setText(tr("편집 위치를 찾을 수 없습니다. 다시 검증하세요."));return;}
  // Also reveal the screen/element behind the precise field inspector.
  if(document["kind"]=="project"&&parts.size()>=3&&parts[0]=="visual"&&parts[1]=="screens"){
    const auto s=ValidationLocation::value(document["data"],"/visual/screens/"+parts[2]).toObject();
    if(!s["id"].toString().isEmpty()){openScreen(s["id"].toString());if(parts.size()>=5&&parts[3]=="elements")selectElement(ValidationLocation::value(document["data"],"/visual/screens/"+parts[2]+"/elements/"+parts[4]).toObject()["id"].toString());}
  }
  const auto file=document["file"].toString();
  auto dialog=ValidationLocation::editor(this,document,view["documents"].toArray(),diagnostic,[this,revision,root,file](QJsonObject data,std::function<void(QString)> done){
    if(bridge.busy()||view["revision"].toInt()!=revision||view["root"].toString()!=root){done(tr("편집 대상이 변경되었습니다. 다시 열어 주세요."));return;}
    request("edit",{{"edit",QJsonObject{{"type","replace"},{"file",file},{"data",data}}}},[this,done](QJsonObject reply){
      if(!reply["error"].toString().isEmpty()){done(reply["error"].toString());return;}
      validateForRepair([done](QString summary){done(summary+"\n"+tr("수정이 적용되었습니다. 닫은 뒤 갱신된 목록에서 다음 오류를 선택하세요."));});
    });
  });
  dialog->open();
}
