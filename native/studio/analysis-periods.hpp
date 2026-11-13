#pragma once
#include <QComboBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <functional>

class BalanceCurve final:public QWidget {
  QJsonArray points_;QString axis_;bool comparison_=false;
public:
  explicit BalanceCurve(QWidget* parent=nullptr):QWidget(parent){setObjectName("analysisPeriodCurve");setMinimumHeight(200);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);setAccessibleName(tr("기간별 밸런스 관측 그래프"));}
  void setPoints(QJsonArray points,QString axis,bool comparison){points_=points;axis_=axis;comparison_=comparison;setProperty("sampleCount",points.size());update();}
protected:
  void paintEvent(QPaintEvent*)override{
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),palette().base());
    QRectF box(66,24,std::max(20,width()-90),height()-70);double min=0,max=0;int first=0,last=1;bool any=false;
    for(auto v:points_){auto point=v.toObject();for(const auto& key:{QString("before"),QString("after")}){auto row=point.value(key).toObject();if(row.isEmpty())continue;double low=row.value("min").toDouble(),high=row.value("max").toDouble();if(!any){min=low;max=high;first=point.value("periodIndex").toInt();last=first;any=true;}min=std::min(min,low);max=std::max(max,high);last=std::max(last,point.value("periodIndex").toInt());}}
    p.setPen(palette().text().color());if(!any){p.drawText(rect(),Qt::AlignCenter,tr("기록 없음"));return;}
    if(min==max){min-=1;max+=1;}const int finalObserved=last;if(first==last)last=first+1;
    const auto position=[&](int period,double value){return QPointF(box.left()+(period-first)*box.width()/(last-first),box.bottom()-(value-min)*box.height()/(max-min));};
    for(int i=0;i<=4;i++){double value=min+(max-min)*i/4;p.setPen(QPen(palette().mid().color(),1,Qt::DotLine));p.drawLine(position(first,value),position(last,value));p.setPen(palette().text().color());p.drawText(QRectF(0,position(first,value).y()-10,60,20),Qt::AlignRight|Qt::AlignVCenter,QString::number(value,'g',5));}
    p.drawText(QRectF(box.left(),box.bottom()+7,box.width(),22),Qt::AlignLeft,QString::number(first+1));p.drawText(QRectF(box.left(),box.bottom()+7,box.width(),22),Qt::AlignRight,QString::number(finalObserved+1));p.drawText(QRectF(box.left(),box.bottom()+27,box.width(),20),Qt::AlignCenter,axis_);
    for(const auto& key:{QString("before"),QString("after")}){if(key=="before"&&!comparison_)continue;const QColor color=key=="before"?QColor("#d18f36"):QColor("#49a7cf");QPainterPath line;bool joined=false;
      for(auto v:points_){auto point=v.toObject(),row=point.value(key).toObject();if(row.isEmpty()){joined=false;continue;}const int period=point.value("periodIndex").toInt();const auto center=position(period,row.value("mean").toDouble());p.setPen(QPen(color,1));p.drawLine(position(period,row.value("min").toDouble()),position(period,row.value("max").toDouble()));if(joined)line.lineTo(center);else line.moveTo(center);joined=true;p.setBrush(color);p.drawEllipse(center,3,3);}p.setPen(QPen(color,2,key=="before"?Qt::DashLine:Qt::SolidLine));p.setBrush(Qt::NoBrush);p.drawPath(line);
    }
  }
};

class AnalysisPeriods final:public QWidget {
  QJsonObject job_,comparison_;QJsonArray jobs_;QComboBox *baseline_,*attribute_;QLabel *scope_,*identity_,*details_,*status_;BalanceCurve* curve_;QTableWidget* rows_;
  static QString number(QJsonValue value){return value.isDouble()?QString::number(value.toDouble(),'g',8):QObject::tr("기록 없음");}
  static QTableWidgetItem* cell(QString text){auto item=new QTableWidgetItem(text);item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);item->setToolTip(text);return item;}
  static QString metric(const QJsonObject& row){return row.isEmpty()?QObject::tr("기록 없음"):QObject::tr("%1 [%2–%3]").arg(number(row.value("mean")),number(row.value("min")),number(row.value("max")));}
  static QString terminations(const QJsonObject& row){QStringList values;for(auto it=row.begin();it!=row.end();++it)values<<reason(it.key())+": "+QString::number(it.value().toInt());return values.isEmpty()?QObject::tr("없음"):values.join(", ");}
  static QString sample(const QJsonObject& row){return row.isEmpty()?QObject::tr("기록 없음"):QObject::tr("관측 %1 · 기간 완료 %2\n여기서 종료: %3").arg(row.value("count").toInt()).arg(row.value("completeRuns").toInt()).arg(terminations(row.value("terminations").toObject()));}
  static QString reason(QString key){
    if(key=="matched-setup")return QObject::tr("동일한 분석 설정");if(key=="changed-setup")return QObject::tr("분석 설정 변경됨");if(key=="incomparable")return QObject::tr("비교 불가");if(key=="not-recorded")return QObject::tr("기록 없음");
    if(key=="PERIOD_SERIES_NOT_RECORDED"||key=="ATTRIBUTE_NOT_RECORDED")return QObject::tr("기간별 값이 기록되지 않았습니다");
    if(key=="RUNNING_REPORT")return QObject::tr("아직 실행 중인 보고서가 있습니다");if(key=="MEASUREMENT_VERSION_CHANGED")return QObject::tr("측정 방식 또는 엔진 버전이 변경되었습니다");if(key=="PERIOD_AXIS_CHANGED")return QObject::tr("달력 또는 표본 간격이 변경되었습니다");
    if(key=="TARGET_SEARCH_NOT_A_RUN_COHORT")return QObject::tr("목표 탐색은 일반 회차 집단을 나타내지 않습니다");if(key=="OBSERVED_RUN_COUNTS_DIFFER")return QObject::tr("관측한 회차 수가 다릅니다");if(key=="INCOMPLETE_RUN_COHORT")return QObject::tr("요청한 회차 중 일부가 완료되지 않았습니다");if(key=="ATTRIBUTE_CAPTURE_LIMIT")return QObject::tr("일부 능력치가 기록 한도를 초과했습니다");if(key=="ATTRIBUTE_DEFINITION_CHANGED")return QObject::tr("능력치 유형 또는 선언 범위가 변경되었습니다");if(key=="NO_COMPARABLE_ATTRIBUTES")return QObject::tr("서로 일치하는 기록된 능력치가 없습니다");
    if(key=="seed")return QObject::tr("Seed");if(key=="strategy")return QObject::tr("분석 전략");if(key=="priorityProfile")return QObject::tr("플레이어 우선순위");if(key=="runs")return QObject::tr("요청 회차");if(key=="workers")return QObject::tr("작업 스레드");if(key=="maxCommands")return QObject::tr("명령 한도");if(key=="timeoutMs")return QObject::tr("시간 제한");if(key=="attributeId")return QObject::tr("집중 능력치");
    if(key=="finished")return QObject::tr("완주");if(key=="blocked")return QObject::tr("막힘");if(key=="command-limit")return QObject::tr("명령 한도");if(key=="time-limit")return QObject::tr("시간 제한");if(key=="cancelled")return QObject::tr("취소됨");if(key=="error")return QObject::tr("오류");if(key=="state-limit")return QObject::tr("탐색 한도");if(key=="target")return QObject::tr("목표 도달");if(key=="incomplete")return QObject::tr("미완료");return key;
  }
  static QString identity(const QJsonObject& report,QString label){auto options=report.value("options").toObject(),result=report.value("result").toObject();const int observed=report.contains("completed")?report.value("completed").toInt():result.value("completed").toInt(),requested=options.value("runs").toInt();const auto profile=options.value("priorityProfile").toObject();
    return QObject::tr("%1: %2 · %3 · 콘텐츠 %4\nSeed %5 · 관측 %6 / 요청 %7 · 명령 %8 · 시간 %9 ms · 스레드 %10\n전략: %11 · 플레이어 우선순위: %12").arg(label,report.value("id").toString().left(8),report.value("sourceVersion").toString(),report.value("fingerprint").toString().right(12)).arg(options.value("seed").toDouble(),0,'f',0).arg(observed).arg(requested).arg(options.value("maxCommands").toInt()).arg(options.value("timeoutMs").toInt()).arg(options.value("workers").toInt(1)).arg(options.value("strategy").toString(),profile.isEmpty()?QObject::tr("없음"):profile.value("name").toString(QObject::tr("직접 지정")));
  }
  void attributes(){const QSignalBlocker block(attribute_);auto selected=attribute_->currentData();attribute_->clear();auto balance=job_.value("result").toObject().value("balance").toObject(),rows=balance.value("attributes").toObject();for(auto id:balance.value("periodSeries").toObject().value("attributeIds").toArray()){attribute_->addItem(rows.value(id.toString()).toObject().value("label").toString(id.toString()),id.toString());attribute_->setItemData(attribute_->count()-1,id.toString(),Qt::ToolTipRole);}for(auto value:comparison_.value("attributes").toArray()){auto a=value.toObject();if(attribute_->findData(a.value("id").toString())<0)attribute_->addItem(a.value("label").toString(),a.value("id").toString());}attribute_->setCurrentIndex(std::max(0,attribute_->findData(selected)));}

public:
  std::function<void(QString)> compareRequested;
  explicit AnalysisPeriods(QWidget* parent=nullptr):QWidget(parent){setObjectName("analysisPeriodsPage");auto layout=new QVBoxLayout(this);auto controls=new QHBoxLayout;layout->addLayout(controls);controls->addWidget(new QLabel(tr("능력치")));attribute_=new QComboBox;attribute_->setObjectName("analysisPeriodAttribute");attribute_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);controls->addWidget(attribute_,1);controls->addWidget(new QLabel(tr("이전 보고서")));baseline_=new QComboBox;baseline_->setObjectName("analysisBalanceBaseline");baseline_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);baseline_->addItem(tr("비교 안 함"),QString());controls->addWidget(baseline_,2);
    identity_=new QLabel;identity_->setObjectName("analysisComparisonIdentity");identity_->setWordWrap(true);identity_->setTextInteractionFlags(Qt::TextSelectableByMouse);auto identityRow=new QHBoxLayout;identityRow->addWidget(identity_,1);auto detailsButton=new QToolButton;detailsButton->setObjectName("analysisComparisonDetails");detailsButton->setText(tr("보고서 상세"));detailsButton->setCheckable(true);identityRow->addWidget(detailsButton);layout->addLayout(identityRow);details_=new QLabel;details_->setObjectName("analysisComparisonDetailsText");details_->setWordWrap(true);details_->setTextInteractionFlags(Qt::TextSelectableByMouse);details_->hide();layout->addWidget(details_);connect(detailsButton,&QToolButton::toggled,details_,&QWidget::setVisible);status_=new QLabel;status_->setObjectName("analysisComparisonStatus");status_->setWordWrap(true);layout->addWidget(status_);
    curve_=new BalanceCurve;layout->addWidget(curve_);auto legend=new QLabel(tr("파랑: 현재 보고서 · 황색 점선: 이전 보고서. 세로선: 관측 최솟값–최댓값. 연결선은 표본 평균을 이으며 미관측 기간을 예측하지 않습니다."));legend->setWordWrap(true);layout->addWidget(legend);
    rows_=new QTableWidget(0,6);rows_->setObjectName("analysisPeriodRows");rows_->setHorizontalHeaderLabels({tr("기간"),tr("이전 평균\n[범위]"),tr("현재 평균\n[범위]"),tr("평균\n차이"),tr("이전\n표본"),tr("현재\n표본")});rows_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);rows_->verticalHeader()->hide();rows_->setMinimumHeight(180);layout->addWidget(rows_,1);
    scope_=new QLabel;scope_->setObjectName("analysisPeriodScope");scope_->setWordWrap(true);layout->addWidget(scope_);
    connect(attribute_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{render();});connect(baseline_,qOverload<int>(&QComboBox::activated),this,[this]{comparison_={};render();if(!baselineId().isEmpty()&&compareRequested)compareRequested(baselineId());});setJob({});
  }
  QString baselineId()const{return baseline_->currentData().toString();}
  void setJobs(const QJsonArray& jobs){jobs_=jobs;const QSignalBlocker block(baseline_);const auto selected=baselineId();baseline_->clear();baseline_->addItem(tr("비교 안 함"),QString());for(auto v:jobs){auto j=v.toObject();if(j.value("status")=="running"||j.value("id")==job_.value("id"))continue;baseline_->addItem(j.value("createdAt").toString()+" · "+j.value("id").toString().left(8)+" · "+j.value("fingerprint").toString().right(10),j.value("id").toString());}baseline_->setCurrentIndex(std::max(0,baseline_->findData(selected)));if(baselineId()!=selected){comparison_={};render();}}
  void setJob(const QJsonObject& job){const bool changed=job_.value("id")!=job.value("id"),finished=job_.value("status")=="running"&&job.value("status")!="running";job_=job;if(changed||finished)comparison_={};if(job.isEmpty())setJobs({});else{bool found=false;for(int i=0;i<jobs_.size();i++)if(jobs_[i].toObject().value("id")==job.value("id")){jobs_[i]=job;found=true;break;}if(!found)jobs_.prepend(job);setJobs(jobs_);}attributes();baseline_->setEnabled(job.value("status")!="running");render();if((changed||finished)&&job.value("status")!="running"&&!baselineId().isEmpty()&&compareRequested)compareRequested(baselineId());}
  void setComparison(const QJsonObject& comparison){comparison_=comparison;attributes();render();}
  void comparisonFailed(QString message){comparison_={};render();status_->setText(tr("비교를 불러올 수 없습니다: %1").arg(message));}
private:
  void render(){
    const auto result=job_.value("result").toObject(),series=result.value("balance").toObject().value("periodSeries").toObject();const auto id=attribute_->currentData().toString();
    rows_->setRowCount(0);QJsonArray points;bool comparing=!baselineId().isEmpty();identity_->setText(tr("현재: %1 · 콘텐츠 %2").arg(job_.value("id").toString().left(8),job_.value("fingerprint").toString().right(12)));details_->setText(identity(job_,tr("현재")));identity_->setToolTip(tr("작업: %1\n콘텐츠: %2\n스냅샷: %3\n설정: %4").arg(job_.value("id").toString(),job_.value("fingerprint").toString(),job_.value("snapshotHash").toString(),QString::fromUtf8(QJsonDocument(job_.value("options").toObject()).toJson(QJsonDocument::Compact))));

    if(series.isEmpty()){status_->setText(tr("기록 없음 · 이 보고서에는 기간별 관측이 없습니다. 누락된 값은 0이 아닙니다."));scope_->clear();curve_->setPoints({},tr("기간"),false);return;}
    scope_->setText(tr("표본 기간에서 마지막으로 수락된 상태를 회차당 한 번 기록합니다. 다음 기간으로 이동하거나 게임이 끝나면 기간 완료로 표시하며, 미완료 표본도 남깁니다. 회차가 멈추면 표본 수가 줄 수 있으며 이후 평균은 계속 관측된 회차만 나타냅니다.\n%1기간 간격, 최대 %2지점·%3능력치. 관측한 회차-기간 %5개 중 %4개 기록, 능력치 %6개 제외. 최적성이나 재미를 보장하지 않습니다.").arg(series.value("stride").toInt()).arg(series.value("maxPoints").toInt()).arg(series.value("maxAttributes").toInt()).arg(series.value("recordedPeriods").toInt()).arg(series.value("observedPeriods").toInt()).arg(series.value("omittedAttributeIds").toArray().size()));
    status_->setText(tr("현재 보고서만 표시합니다. 평균에는 아래 표본 수를 사용합니다.\n회차 종료: %1").arg(terminations(result.value("balance").toObject().value("terminations").toObject())));
    if(comparing){
      if(comparison_.isEmpty()){status_->setText(tr("저장된 이전 보고서를 불러오는 중…"));curve_->setPoints({},tr("기간"),true);return;}
      const auto before=comparison_.value("before").toObject();identity_->setText(identity_->text()+"\n"+tr("이전: %1 · 콘텐츠 %2").arg(before.value("id").toString().left(8),before.value("fingerprint").toString().right(12)));details_->setText(details_->text()+"\n"+identity(before,tr("이전")));identity_->setToolTip(identity_->toolTip()+tr("\n이전 작업: %1\n이전 콘텐츠: %2\n이전 스냅샷: %3\n이전 설정: %4").arg(before.value("id").toString(),before.value("fingerprint").toString(),before.value("snapshotHash").toString(),QString::fromUtf8(QJsonDocument(before.value("options").toObject()).toJson(QJsonDocument::Compact))));
      QStringList changes,reasons;for(auto c:comparison_.value("setupChanges").toArray())changes<<reason(c.toString());for(auto c:comparison_.value("reasons").toArray())reasons<<reason(c.toString());const auto state=comparison_.value("status").toString();status_->setText(tr("비교: %1 · 콘텐츠 변경: %2").arg(reason(state),comparison_.value("contentChanged").toBool()?tr("예"):tr("아니요")));if(!changes.isEmpty())status_->setText(status_->text()+"\n"+tr("변경된 설정: %1").arg(changes.join(", ")));if(!reasons.isEmpty())status_->setText(status_->text()+"\n"+tr("한계: %1").arg(reasons.join(", ")));status_->setText(status_->text()+"\n"+tr("이전 종료: %1 · 현재 종료: %2").arg(terminations(before.value("terminations").toObject()),terminations(comparison_.value("after").toObject().value("terminations").toObject())));scope_->setText(scope_->text()+"\n"+tr("관측값의 설명이며 대응 표본 효과나 인과 효과가 아닙니다. 동일 ID는 같은 양을 뜻한다고 가정합니다."));

      if(state=="not-recorded"||state=="incomparable"){curve_->setPoints({},tr("기간"),true);return;}
      for(auto value:comparison_.value("attributes").toArray()){auto attribute=value.toObject();if(attribute.value("id")!=id)continue;if(!attribute.value("comparable").toBool()){status_->setText(status_->text()+"\n"+tr("이 능력치는 비교할 수 없습니다: %1").arg(reason(attribute.value("reason").toString())));curve_->setPoints({},tr("기간"),true);return;}points=attribute.value("points").toArray();break;}
    }else for(auto v:series.value("points").toArray()){auto row=v.toObject(),metric=row.value("attributes").toObject().value(id).toObject();if(metric.isEmpty()||metric.value("count").toInt()==0)continue;metric["mean"]=metric.value("sum").toDouble()/metric.value("count").toInt();metric["completeRuns"]=row.value("completeRuns");metric["terminations"]=row.value("terminations");points.append(QJsonObject{{"periodIndex",row.value("periodIndex")},{"after",metric}});}
    details_->setToolTip(identity_->toolTip());const auto axis=series.value("axis").toObject();QString label=axis.value("periodLabel").toString();if(label.isEmpty())label=tr("기간");curve_->setPoints(points,label,comparing);
    for(auto value:points){auto point=value.toObject(),before=point.value("before").toObject(),after=point.value("after").toObject();int row=rows_->rowCount();rows_->insertRow(row);QStringList cells{label+" "+QString::number(point.value("periodIndex").toInt()+1),metric(before),metric(after),comparing?number(point.value("meanDifference")):tr("비교 안 함"),sample(before),sample(after)};for(int col=0;col<cells.size();col++)rows_->setItem(row,col,cell(cells[col]));}rows_->resizeRowsToContents();
  }
};
