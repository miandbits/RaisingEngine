#pragma once
#include "../include/raising/calendar.hpp"
#include <QJsonObject>
#include <QJsonArray>
#include <QObject>
namespace StudioCalendar {
inline raising::dates::Spec spec(const QJsonObject& p){raising::dates::Spec s;auto c=p.value("calendar").toObject();if(c.isEmpty())return s;s.custom=true;s.months=c.value("monthsPerYear").toInt(12);s.days=c.value("daysPerMonth").toInt(30);s.startWeekday=c.value("startWeekday").toInt();s.year=c.value("yearLabel").toString("Year").toStdString();s.month=c.value("monthLabel").toString("Month").toStdString();s.period=c.value("periodLabel").toString("Period").toStdString();s.day=c.value("dayLabel").toString("Day").toStdString();auto w=c.value("weekdayLabels").toArray();if(w.size()==7)for(int i=0;i<7;i++)s.weekdays[i]=w[i].toString().toStdString();return s;}
inline raising::dates::Date date(const QJsonObject& p,int index){return raising::dates::date(p.value("startYear").toInt(1),p.value("startMonth").toInt(1),qMax(1,p.value("periodsPerMonth").toInt(3)),index,spec(p).months);}
inline QString label(const QJsonObject& p,int index,bool withPeriod=true){auto d=date(p,index);auto c=spec(p);if(c.custom)return QString::fromStdString(raising::dates::text(c,d,withPeriod));return withPeriod?QObject::tr("%1년 %2월 · %3번째 일정").arg(d.year).arg(d.month).arg(d.period+1):QObject::tr("%1년 %2월").arg(d.year).arg(d.month);}
}
