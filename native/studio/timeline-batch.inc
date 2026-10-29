#include "timeline-batch.hpp"
#include "combo-chooser.hpp"
#include <QApplication>
#include <QScrollArea>
#include <QScreen>
#include <QShortcut>

namespace {
bool editTimelineBatch(QWidget *parent, const QJsonArray &activities, const QJsonArray &assets,
                       const QJsonArray &timelines, const QJsonArray &bindings, TimelineBatchPlan &accepted) {
  auto tr=[](const char *text){return QObject::tr(text);};
  QDialog dialog(parent); dialog.setObjectName("timelineBatchDialog"); dialog.setWindowTitle(tr("활동 연출 일괄 만들기 · 연결"));
  auto outer=new QVBoxLayout(&dialog); auto scroll=new QScrollArea; scroll->setObjectName("timelineBatchScroll"); scroll->setWidgetResizable(true);
  auto body=new QWidget; auto layout=new QVBoxLayout(body); scroll->setWidget(body); outer->addWidget(scroll,1);
  auto intro=new QLabel(tr("활동을 선택하고 변경 내용을 검토하세요. 적용은 이 편집 창의 초안만 바꿉니다. 타임라인 창에서 저장해야 프로젝트에 반영됩니다. 활동 비용·효과·달력 규칙은 바꾸지 않습니다.")); intro->setWordWrap(true); intro->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Minimum); layout->addWidget(intro);
  auto form=new QFormLayout; form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow); layout->addLayout(form);
  auto mode=new QComboBox; mode->setObjectName("timelineBatchMode"); mode->addItem(tr("활동마다 새 연출 만들기"),"create"); mode->addItem(tr("기존 연출에 연결"),"link"); form->addRow(tr("작업"),mode);
  auto existing=new QComboBox; existing->setObjectName("timelineBatchExisting"); existing->addItem(tr("연결할 연출 선택"),QString()); for(auto t:timelines){auto o=t.toObject(); existing->addItem(o["name"].toString(),o["id"].toString()); existing->setItemData(existing->count()-1,o["id"].toString(),Qt::ToolTipRole);} form->addRow(tr("기존 연출"),existing);
  auto replace=new QCheckBox(tr("이미 연결된 활동의 기본 연출도 교체 (성공·실패·반복 유지)")); replace->setObjectName("timelineBatchReplace"); layout->addWidget(replace);
  auto selectRow=new QHBoxLayout; layout->addLayout(selectRow);
  auto unmapped=new QPushButton(tr("연결 없는 활동 선택")); unmapped->setObjectName("timelineBatchSelectUnmapped"); selectRow->addWidget(unmapped);
  auto all=new QPushButton(tr("모두 선택")); all->setObjectName("timelineBatchSelectAll"); selectRow->addWidget(all);
  auto none=new QPushButton(tr("선택 해제")); none->setObjectName("timelineBatchSelectNone"); selectRow->addWidget(none); selectRow->addStretch();
  auto list=new QListWidget; list->setObjectName("timelineBatchActivities"); list->setMinimumHeight(130); list->setMaximumHeight(190); layout->addWidget(list);
  QSet<QString> mapped,images; QMap<QString,QString> imageLabels;
  for(auto b:bindings) mapped.insert(b.toObject()["activityId"].toString());
  for(auto a:assets){auto asset=a.toObject(); if(asset["kind"]=="image"){auto id=asset["id"].toString(); images.insert(id); imageLabels[id]=asset["label"].toString(id);}}
  for(auto a:activities){auto activity=a.toObject(); auto id=activity["id"].toString(); auto item=new QListWidgetItem(activity["label"].toString(id)+(mapped.contains(id)?tr(" · 연결 있음"):QString()),list); item->setData(Qt::UserRole,id); item->setToolTip(id); item->setFlags(item->flags()|Qt::ItemIsUserCheckable); item->setCheckState(mapped.contains(id)?Qt::Unchecked:Qt::Checked);}
  auto templateBox=new QWidget; templateBox->setObjectName("timelineBatchTemplate"); auto templateLayout=new QVBoxLayout(templateBox); templateLayout->setContentsMargins(0,0,0,0); layout->addWidget(templateBox);
  auto templateForm=new QFormLayout; templateLayout->addLayout(templateForm);
  auto suffix=new QLineEdit(tr(" · 활동 연출")); suffix->setObjectName("timelineBatchSuffix"); suffix->setMaxLength(100); templateForm->addRow(tr("새 연출 이름의 접미사"),suffix);
  auto fallback=new QComboBox; fallback->setObjectName("timelineBatchFallback"); fallback->addItem(tr("배경이 없을 때 사용할 방식 선택"),"unset"); fallback->addItem(tr("문구만 표시 (이미지 없음)"),"none"); for(const auto &id:images){fallback->addItem(imageLabels[id],"asset:"+id); fallback->setItemData(fallback->count()-1,id,Qt::ToolTipRole);} auto fallbackLabel=new QLabel(tr("활동 배경의 대체 이미지")+" ("+QKeySequence(Qt::ALT|Qt::Key_B).toString(QKeySequence::NativeText)+")"); fallbackLabel->setObjectName("timelineBatchFallbackLabel"); fallbackLabel->setBuddy(fallback); templateForm->addRow(fallbackLabel,raising::comboChooser(fallback,tr("활동 배경의 대체 이미지")));
  // Explicit shortcuts remain available when macOS disables ordinary Tab access to combo boxes.
  auto fallbackShortcut=new QShortcut(QKeySequence(Qt::ALT|Qt::Key_B),&dialog); fallbackShortcut->setContext(Qt::WidgetWithChildrenShortcut); fallbackShortcut->setAutoRepeat(false); QObject::connect(fallbackShortcut,&QShortcut::activated,&dialog,[&]{if(templateBox->isVisible()&&fallback->isEnabled()){scroll->ensureWidgetVisible(fallback);fallback->setFocus(Qt::ShortcutFocusReason);}});
  auto tokenHelp=new QLabel(tr("공통 클립 문구: {activity} 활동 이름 · {outcome} 결과 · {day} 날짜 · {changes} 확정된 변화. 이미지는 각 활동의 배경을 사용하거나 클립별로 직접 선택할 수 있습니다.")); tokenHelp->setWordWrap(true); tokenHelp->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Minimum); templateLayout->addWidget(tokenHelp);
  auto clips=new QTableWidget(0,4); clips->setObjectName("timelineBatchClips"); clips->setHorizontalHeaderLabels({tr("시간 (ms)"),tr("문구"),tr("이미지"),tr("등장")}); clips->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents); clips->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch); clips->setMinimumHeight(120); templateLayout->addWidget(clips);
  auto clipTools=new QHBoxLayout; templateLayout->addLayout(clipTools); auto add=new QPushButton(tr("클립 추가")); add->setObjectName("timelineBatchClipAdd"); clipTools->addWidget(add); auto remove=new QPushButton(tr("삭제")); remove->setObjectName("timelineBatchClipRemove"); clipTools->addWidget(remove); clipTools->addStretch();
  auto review=new QPushButton(tr("변경 내용 검토")); review->setObjectName("timelineBatchReview"); layout->addWidget(review);
  auto summary=new QLabel; summary->setObjectName("timelineBatchSummary"); summary->setWordWrap(true); summary->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Minimum); layout->addWidget(summary);
  auto impact=new QTableWidget(0,4); impact->setObjectName("timelineBatchImpact"); impact->setEditTriggers(QAbstractItemView::NoEditTriggers); impact->setHorizontalHeaderLabels({tr("활동"),tr("변경"),tr("연출"),tr("이미지")}); impact->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); impact->setMinimumHeight(150); layout->addWidget(impact);
  auto error=new QLabel; error->setObjectName("timelineBatchError"); error->setWordWrap(true); error->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Minimum); error->setStyleSheet("color:#b84036"); layout->addWidget(error); layout->addStretch();
  auto buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel); buttons->setObjectName("timelineBatchButtons"); auto apply=buttons->button(QDialogButtonBox::Apply); apply->setText(tr("검토한 변경 적용")); apply->setEnabled(false); outer->addWidget(buttons);
  TimelineBatchPlan reviewed; bool reviewedCurrent=false;
  auto invalidate=[&]{reviewedCurrent=false; apply->setEnabled(false); summary->clear(); impact->setRowCount(0); error->clear();};
  auto addClip=[&](const QString &text){int row=clips->rowCount(); if(row>=100)return; clips->insertRow(row); auto duration=new QSpinBox; duration->setRange(1,60000); duration->setValue(1200); clips->setCellWidget(row,0,duration); clips->setItem(row,1,new QTableWidgetItem(text)); auto image=new QComboBox; image->addItem(tr("활동 배경 또는 대체 이미지"),"activity"); image->addItem(tr("이미지 없음"),"none"); for(const auto &id:images){image->addItem(imageLabels[id],"asset:"+id); image->setItemData(image->count()-1,id,Qt::ToolTipRole);} clips->setCellWidget(row,2,image); auto transition=new QComboBox; transition->addItem(tr("즉시"),"none"); transition->addItem(tr("서서히"),"fade"); clips->setCellWidget(row,3,transition); QObject::connect(duration,qOverload<int>(&QSpinBox::valueChanged),&dialog,invalidate); QObject::connect(image,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,invalidate); QObject::connect(transition,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,invalidate); invalidate();};
  addClip("{activity} · {day}"); addClip("{outcome}\n{changes}");
  QObject::connect(add,&QPushButton::clicked,&dialog,[&]{addClip("{activity}");}); QObject::connect(remove,&QPushButton::clicked,&dialog,[&]{if(clips->currentRow()>=0){clips->removeRow(clips->currentRow());invalidate();}});
  QObject::connect(clips,&QTableWidget::itemChanged,&dialog,invalidate); QObject::connect(list,&QListWidget::itemChanged,&dialog,invalidate); QObject::connect(replace,&QCheckBox::toggled,&dialog,invalidate); QObject::connect(suffix,&QLineEdit::textChanged,&dialog,invalidate); QObject::connect(fallback,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,invalidate); QObject::connect(existing,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,invalidate);
  auto select=[&](int mode){for(int i=0;i<list->count();++i){auto item=list->item(i); item->setCheckState((mode==1||(mode==2&&!mapped.contains(item->data(Qt::UserRole).toString())))?Qt::Checked:Qt::Unchecked);} invalidate();};
  QObject::connect(all,&QPushButton::clicked,&dialog,[&]{select(1);}); QObject::connect(none,&QPushButton::clicked,&dialog,[&]{select(0);}); QObject::connect(unmapped,&QPushButton::clicked,&dialog,[&]{select(2);});
  auto modeChanged=[&]{bool create=mode->currentData()=="create"; templateBox->setVisible(create); existing->setEnabled(!create); invalidate();}; QObject::connect(mode,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,modeChanged); modeChanged();
  QObject::connect(review,&QPushButton::clicked,&dialog,[&]{
    invalidate(); TimelineBatchOptions options; for(int i=0;i<list->count();++i)if(list->item(i)->checkState()==Qt::Checked)options.selected.insert(list->item(i)->data(Qt::UserRole).toString()); options.replaceMapped=replace->isChecked(); options.nameSuffix=suffix->text();
    if(mode->currentData()=="link"){options.existingTimelineId=existing->currentData().toString(); if(options.existingTimelineId.isEmpty()){error->setText(tr("연결할 연출을 선택하세요."));scroll->ensureWidgetVisible(error);return;}}
    options.fallbackChosen=fallback->currentData()!="unset"; auto fallbackValue=fallback->currentData().toString(); if(fallbackValue.startsWith("asset:"))options.fallbackImage=fallbackValue.mid(6);
    for(int r=0;r<clips->rowCount();++r)options.clips.append(QJsonObject{{"durationMs",qobject_cast<QSpinBox*>(clips->cellWidget(r,0))->value()},{"text",clips->item(r,1)->text()},{"imageSource",qobject_cast<QComboBox*>(clips->cellWidget(r,2))->currentData().toString()},{"transition",qobject_cast<QComboBox*>(clips->cellWidget(r,3))->currentData().toString()}});
    reviewed=planTimelineBatch(activities,timelines,bindings,images,options);
    if(!reviewed.valid()){error->setText(reviewed.error);scroll->ensureWidgetVisible(error);return;}
    for(auto value:reviewed.impact){auto row=value.toObject(); int r=impact->rowCount(); impact->insertRow(r); auto action=row["action"].toString(); QString label=action=="skip"?tr("기존 연결 유지"):action=="create"?tr("새로 만들고 연결"):tr("기존 연출 연결"); if(row["replacesDefault"].toBool())label+=tr(" · 기본만 교체"); QStringList used; for(auto id:row["images"].toArray())used<<imageLabels.value(id.toString(),id.toString()); auto image=action=="create"?(used.isEmpty()?tr("문구만 표시"):used.join(", ")):tr("기존 연출 유지"); if(row["fallbackUsed"].toBool())image+=tr(" · 대체 사용"); QString timelineName=row["timelineId"].toString(); for(auto t:reviewed.timelines)if(t.toObject()["id"]==row["timelineId"])timelineName=t.toObject()["name"].toString(); const QStringList cells{row["label"].toString(),label,timelineName,image}; for(int c=0;c<cells.size();++c){auto item=new QTableWidgetItem(cells[c]); item->setToolTip(c==0?row["activityId"].toString():c==2?row["timelineId"].toString():cells[c]); impact->setItem(r,c,item);}}
    impact->resizeRowsToContents(); summary->setText(tr("새 연출 %1개 · 연결 %2개 · 기존 연결 유지 %3개\n성공·실패 연출과 반복 방식은 유지합니다. 새 연결은 일정 칸당 한 번 재생합니다.").arg(reviewed.created).arg(reviewed.linked).arg(reviewed.skipped)); reviewedCurrent=true; apply->setEnabled(reviewed.linked>0); scroll->ensureWidgetVisible(summary);
  });
  QObject::connect(apply,&QPushButton::clicked,&dialog,[&]{if(reviewedCurrent&&reviewed.valid()&&reviewed.linked>0){accepted=reviewed;dialog.accept();}}); QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  QObject::connect(qApp,&QApplication::focusChanged,&dialog,[&](QWidget*,QWidget *next){if(next&&body->isAncestorOf(next))scroll->ensureWidgetVisible(next);});
  const auto available=(parent->screen()?parent->screen():QApplication::primaryScreen())->availableGeometry(); dialog.resize(qMin(960,available.width()-32),qMin(800,available.height()-48));
  return dialog.exec()==QDialog::Accepted;
}
}
