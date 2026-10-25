#pragma once
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>
#include <functional>

class AnalysisBalance final:public QWidget {
  QLabel *summary_,*scope_;QTableWidget *resources_,*endings_;
  static QString value(QJsonValue v){return v.isDouble()?QString::number(v.toDouble(),'g',12):QString::fromUtf8("—");}
  static QString fraction(int count,int total){return QString::number(count)+" / "+QString::number(total);}
  static QTableWidgetItem* cell(const QString& text){auto item=new QTableWidgetItem(text);item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);item->setToolTip(text);return item;}
public:
  std::function<void(int,int)> replayWitness;std::function<void(QString)> endingRuns;
  explicit AnalysisBalance(QWidget* parent=nullptr):QWidget(parent){setObjectName("analysisBalancePage");auto outer=new QVBoxLayout(this);auto scroll=new QScrollArea;scroll->setWidgetResizable(true);outer->addWidget(scroll);auto body=new QWidget;scroll->setWidget(body);auto layout=new QVBoxLayout(body);
    summary_=new QLabel;summary_->setObjectName("analysisBalanceSummary");summary_->setWordWrap(true);layout->addWidget(summary_);scope_=new QLabel;scope_->setObjectName("analysisBalanceScope");scope_->setWordWrap(true);
    auto title=new QLabel(tr("자원·조건 관측"));layout->addWidget(title);resources_=new QTableWidget(0,5);resources_->setObjectName("analysisBalanceResources");resources_->setHorizontalHeaderLabels({tr("자원·조건"),tr("관측 최솟값 · 최댓값"),tr("범위 이탈"),tr("한도 도달"),tr("실제 회차 재생")});resources_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);resources_->horizontalHeaderItem(2)->setToolTip(tr("안심 범위 이탈 회차"));resources_->horizontalHeaderItem(3)->setToolTip(tr("선언 한도 도달 회차"));resources_->verticalHeader()->hide();resources_->setWordWrap(true);resources_->setMinimumHeight(190);layout->addWidget(resources_);layout->addWidget(scope_);
    auto endingsTitle=new QLabel(tr("엔딩 분포"));layout->addWidget(endingsTitle);endings_=new QTableWidget(0,3);endings_->setObjectName("analysisBalanceEndings");endings_->setHorizontalHeaderLabels({tr("엔딩 ID"),tr("관측 회차"),tr("기록 열기")});endings_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);endings_->verticalHeader()->hide();endings_->setMinimumHeight(150);layout->addWidget(endings_);layout->addStretch();update({});
  }
  void update(const QJsonObject& job){const auto result=job.value("result").toObject(),outcomes=result.value("outcomes").toObject();const int recorded=result.value("completed").toInt(),requested=result.value("requested").toInt();
    summary_->setText(tr("기록된 회차 %1 / 요청 %2\n완주 %3 · 막힘 %4 · 미완료·제한 %5 · 오류 %6 · 목표 도달 %7\n작업 종료 사유: %8").arg(recorded).arg(requested).arg(fraction(outcomes.value("finished").toInt(),recorded)).arg(fraction(outcomes.value("blocked").toInt(),recorded)).arg(fraction(outcomes.value("limit").toInt(),recorded)).arg(fraction(outcomes.value("error").toInt(),recorded)).arg(fraction(outcomes.value("target").toInt(),recorded)).arg(result.value("stopReason").toString(tr("기록 없음"))));
    auto options=job.value("options").toObject();if(options.value("priorityProfile").isObject()){const auto p=options.value("priorityProfile").toObject();summary_->setText(summary_->text()+tr("\n사용한 우선순위: %1 · %2").arg(p.value("name").toString(tr("직접 지정")),p.value("id").toString()));}
    resources_->setRowCount(0);endings_->setRowCount(0);
    const auto balance=result.value("balance").toObject();if(balance.isEmpty()){scope_->setText(tr("기록 없음 · 이 분석에는 명령별 자원 관측이 없습니다. 이전 결과를 0으로 해석하지 마세요."));}
    else {const int observed=balance.value("observedRuns").toInt();const auto terminations=balance.value("terminations").toObject();summary_->setText(summary_->text()+(!balance.contains("terminations")?tr("\n제한 사유별 회차: 기록 없음"):tr("\n관측 회차 중 명령 한도 %1 · 시간 제한 %2 · 취소 %3 · 탐색 상태 한도 %4").arg(fraction(terminations.value("command-limit").toInt(),observed)).arg(fraction(terminations.value("time-limit").toInt(),observed)).arg(fraction(terminations.value("cancelled").toInt(),observed)).arg(fraction(terminations.value("state-limit").toInt(),observed))));scope_->setText(tr("초기 상태와 선택해 수락된 모든 명령 직후를 표본으로 관측했습니다. 후보 명령은 제외합니다.\n관측 회차 %1 · 상태 표본 %2. 이탈·한도 도달은 한 번 이상 관측된 회차 수이며 시간 비율이 아닙니다. 시간·명령 한도로 멈춘 회차는 완주가 아닙니다.").arg(observed).arg(balance.value("observedStates").toInt()));
      const auto attributes=balance.value("attributes").toObject();for(auto it=attributes.begin();it!=attributes.end();++it){auto a=it.value().toObject();if(a.value("kind")!="resource"&&a.value("kind")!="condition")continue;int row=resources_->rowCount();resources_->insertRow(row);resources_->setItem(row,0,cell(a.value("label").toString(it.key())+"\n"+it.key()));resources_->setItem(row,1,cell(value(a.value("min"))+" · "+value(a.value("max"))));
        QStringList breach,bounds;for(auto side:{QString("Min"),QString("Max")}){const bool low=side=="Min";const auto label=low?tr("하한 미만"):tr("상한 초과");breach<<label+": "+(a.contains("band"+side)?fraction(a.value(low?"runsBelowBand":"runsAboveBand").toInt(),observed)+" ("+value(a.value("band"+side))+")":tr("설정 안 됨"));bounds<<(low?tr("최소"):tr("최대"))+": "+(a.contains("declared"+side)?fraction(a.value("runsAt"+side).toInt(),observed)+" ("+value(a.value("declared"+side))+")":tr("설정 안 됨"));}resources_->setItem(row,2,cell(breach.join("\n")));resources_->setItem(row,3,cell(bounds.join("\n")));
        auto witnessCell=new QWidget;auto buttons=new QVBoxLayout(witnessCell);buttons->setContentsMargins(3,3,3,3);for(auto key:{QString("firstBelow"),QString("firstAbove")})if(a.value(key).isObject()){const auto witness=a.value(key).toObject();auto button=new QPushButton(key=="firstBelow"?tr("아래 이탈 재생"):tr("위 이탈 재생"));button->setObjectName("analysisWitness-"+it.key()+"-"+key);button->setToolTip(tr("회차 %1 · 명령 %2 · 기간 %3").arg(witness.value("run").toInt()).arg(witness.value("commandIndex").toInt()).arg(witness.value("periodIndex").toInt()+1));connect(button,&QPushButton::clicked,this,[this,witness]{if(replayWitness)replayWitness(witness.value("run").toInt(),witness.value("commandIndex").toInt());});buttons->addWidget(button);}resources_->setCellWidget(row,4,witnessCell);resources_->setRowHeight(row,100);
      }
    }
    const auto endings=result.value("endings").toObject();for(auto it=endings.begin();it!=endings.end();++it){int row=endings_->rowCount();endings_->insertRow(row);endings_->setItem(row,0,cell(it.key()));endings_->setItem(row,1,cell(fraction(it.value().toInt(),recorded)));auto button=new QPushButton(tr("해당 회차 보기"));button->setObjectName("analysisEndingRuns-"+it.key());connect(button,&QPushButton::clicked,this,[this,id=it.key()]{if(endingRuns)endingRuns(id);});endings_->setCellWidget(row,2,button);endings_->setRowHeight(row,44);}
  }
};
