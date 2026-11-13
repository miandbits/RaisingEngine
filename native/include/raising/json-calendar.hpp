#pragma once
#include "calendar.hpp"
#include "json.hpp"
namespace raising::dates {
inline Spec spec(const nlohmann::json& p,const std::string& locale="en"){
 Spec s;if(!p.contains("calendar"))return s;s.custom=true;const auto& c=p.at("calendar");s.months=c.at("monthsPerYear");s.days=c.at("daysPerMonth");s.startWeekday=c.value("startWeekday",0);
 const bool ko=locale.rfind("ko",0)==0,ja=locale.rfind("ja",0)==0;
 s.year=c.value("yearLabel",ko?"연도":ja?"年":"Year");s.month=c.value("monthLabel",ko?"월":ja?"月":"Month");s.period=c.value("periodLabel",ko?"일정":ja?"期間":"Period");s.day=c.value("dayLabel",ko?"일":ja?"日":"Day");
 if(ko)s.weekdays={{"일","월","화","수","목","금","토"}};if(ja)s.weekdays={{"日","月","火","水","木","金","土"}};
 if(c.contains("weekdayLabels"))for(int i=0;i<7;++i)s.weekdays[i]=c.at("weekdayLabels").at(i).get<std::string>();return s;
}
inline Date date(const nlohmann::json& p,int64_t index){return date(p.value("startYear",1),p.value("startMonth",1),p.value("periodsPerMonth",3),index,spec(p).months);}
inline Grid grid(const nlohmann::json& p,const Date& d){return grid(spec(p),d,p.value("startYear",1),p.value("startMonth",1),p.value("periodsPerMonth",3));}
inline std::string text(const nlohmann::json& p,int64_t index,const std::string& locale="en",bool period=true){return text(spec(p,locale),date(p,index),period);}
inline std::string problem(const nlohmann::json& p){
 auto integer=[](const nlohmann::json& o,const char* k,int lo,int hi){return o.contains(k)&&o.at(k).is_number_integer()&&o.at(k)>=lo&&o.at(k)<=hi;};
 int months=12;if(p.contains("calendar")){const auto& c=p.at("calendar");if(!c.is_object())return "/calendar";
 if(!integer(c,"monthsPerYear",1,60))return "/calendar/monthsPerYear";months=c.at("monthsPerYear");
 if(!integer(c,"daysPerMonth",p.value("periodsPerMonth",3),366))return "/calendar/daysPerMonth";
 if(c.contains("startWeekday")&&!integer(c,"startWeekday",0,6))return "/calendar/startWeekday";
 auto label=[](const nlohmann::json& v){if(!v.is_string())return false;const auto t=v.get<std::string>();size_t n=0;for(unsigned char ch:t)if((ch&0xc0)!=0x80)++n;return n<=80&&t.find_first_not_of(" \t\r\n")!=std::string::npos;};
 for(const char* k:{"yearLabel","monthLabel","periodLabel","dayLabel"})if(c.contains(k)&&!label(c.at(k)))return std::string("/calendar/")+k;
 if(c.contains("weekdayLabels")){const auto& w=c.at("weekdayLabels");if(!w.is_array()||w.size()!=7)return "/calendar/weekdayLabels";for(const auto& v:w)if(!label(v))return "/calendar/weekdayLabels";}
 if(p.contains("gameplay")&&p.at("gameplay").contains("ruleCalendar")&&p.at("gameplay").at("ruleCalendar").at("daysPerMonth")!=c.at("daysPerMonth"))return "/gameplay/ruleCalendar/daysPerMonth";
 }if(!integer(p,"startMonth",1,months))return "/startMonth";return {};
}
} // namespace raising::dates
