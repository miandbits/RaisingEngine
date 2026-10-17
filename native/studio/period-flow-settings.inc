// Optional calendar stages; all game-specific choices stay in project data.
  auto periodPage=new QWidget;auto periodForm=new QFormLayout(periodPage);tabs->addTab(periodPage,tr("기간 단계"));
  const auto periodConfig=project.value("periodFlow").toObject();
  auto allPeriodEvents=new QCheckBox(tr("기간 종료 사건을 우선순위대로 모두 진행"));allPeriodEvents->setObjectName("periodAllEvents");allPeriodEvents->setChecked(periodConfig.value("allPeriodEndEvents").toBool());periodForm->addRow(allPeriodEvents);
  auto preparePolicies=new QCheckBox(tr("생활 방침을 활동 전에 별도로 확정"));preparePolicies->setObjectName("periodPreparePolicies");preparePolicies->setChecked(periodConfig.value("preparePolicies").toBool());periodForm->addRow(preparePolicies);
  auto betweenEnabled=new QCheckBox(tr("활동 뒤 자유 행동 화면에서 대기"));betweenEnabled->setObjectName("periodIntermissionEnabled");betweenEnabled->setChecked(periodConfig.contains("intermission"));periodForm->addRow(betweenEnabled);
  auto betweenScreen=new QComboBox;betweenScreen->setObjectName("periodIntermissionScreen");
  for(auto value:project.value("visual").toObject().value("screens").toArray()){auto s=value.toObject();betweenScreen->addItem(s.value("name").toString(s.value("id").toString()),s.value("id").toString());}
  auto betweenFlag=new QComboBox;betweenFlag->setObjectName("periodIntermissionFlag");
  for(auto value:view.value("documents").toArray()){auto doc=value.toObject();if(doc.value("kind")=="flags")for(auto entry:doc.value("data").toObject().value("flags").toArray()){auto f=entry.toObject();betweenFlag->addItem(f.value("label").toString(f.value("id").toString()),f.value("id").toString());}}
  const auto betweenConfig=periodConfig.value("intermission").toObject();
  for(auto pair:QList<QPair<QComboBox*,QString>>{{betweenScreen,"screenId"},{betweenFlag,"completionFlagId"}}){auto id=betweenConfig.value(pair.second).toString();if(!id.isEmpty()){if(pair.first->findData(id)<0)pair.first->addItem(tr("없는 참조: ")+id,id);pair.first->setCurrentIndex(pair.first->findData(id));}pair.first->setEnabled(betweenEnabled->isChecked());connect(betweenEnabled,&QCheckBox::toggled,pair.first,&QWidget::setEnabled);}
  periodForm->addRow(tr("자유 행동 화면"),betweenScreen);periodForm->addRow(tr("자유 행동 완료 플래그"),betweenFlag);
  QJsonValue betweenEffects=betweenConfig.value("entryEffects").isArray()?betweenConfig.value("entryEffects"):QJsonValue(QJsonArray{});
  auto editBetweenEffects=new QPushButton(tr("자유 행동 진입 효과…"));editBetweenEffects->setObjectName("periodIntermissionEffects");editBetweenEffects->setEnabled(betweenEnabled->isChecked());connect(betweenEnabled,&QCheckBox::toggled,editBetweenEffects,&QWidget::setEnabled);periodForm->addRow(editBetweenEffects);
  connect(editBetweenEffects,&QPushButton::clicked,&dialog,[&]{LogicEditor editor(betweenEffects,"effects",view,&dialog);if(editor.exec()==QDialog::Accepted)betweenEffects=editor.value();});
  auto periodHelp=new QLabel(tr("준비 정산 후 활동 조건이나 비용이 부족하면 준비 비용을 다시 내지 않고 활동을 바꿀 수 있습니다.\n자유 행동 진입 시 완료 플래그를 끕니다. 행동이나 사건에서 이 플래그를 켜면 기간 종료 사건으로 이어집니다. 진입 효과는 한 번만 적용됩니다.\n현재 단일 대상 월 일정에서 지원합니다. 다인 배정·시간 세션과 함께 켜면 검증 오류로 안내합니다."));periodHelp->setWordWrap(true);periodForm->addRow(periodHelp);
  auto readPeriodFlow=[&]{QJsonObject result;if(allPeriodEvents->isChecked())result["allPeriodEndEvents"]=true;if(preparePolicies->isChecked())result["preparePolicies"]=true;if(betweenEnabled->isChecked())result["intermission"]=QJsonObject{{"screenId",betweenScreen->currentData().toString()},{"completionFlagId",betweenFlag->currentData().toString()},{"entryEffects",betweenEffects}};return result;};
